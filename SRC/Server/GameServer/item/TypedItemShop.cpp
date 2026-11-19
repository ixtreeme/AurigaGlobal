#include "stdafx.h"
#include "TypedItemShop.hpp"
#include "TypedInventorySync.hpp"
#include "TypedDrops.hpp"
#include "TypedRecovery.hpp"
#include "ItemStore.hpp"
#include "ItemCatalogRuntime.hpp"
#include <StoragePersistenceSql.hpp>
#include <ItemPersistenceSql.hpp>
#include <common/DragonCoinWallet.h>
#include "../ecs/Registry.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../entity/char_manager.h"
#include "../network/desc.h"
#include "../network/desc_client.h"
#include "../network/db.h"
#include "../core/packet.h"
#include "../core/buffer_manager.h"
#include <Core/Logging.hpp>
#include <Core/Time.h>
#include <openssl/rand.h>
#include <algorithm>
#include <ctime>

#ifdef ENABLE_ITEMSHOP
namespace
{
namespace items = auriga::item;
namespace store = items::runtime;
namespace sql = items::persistence;
using E = items::ItemPersistenceError;

// One shop row as stored now, with its one-time native conversion (if any).
struct ShopRow
{
	std::uint32_t vnum = 0, price = 0;
	std::uint8_t discount = 0;
	int weekLimit = 0, monthLimit = 0;
	std::optional<items::ItemKey> key;
	std::uint16_t level = 0;
};
struct Loaded
{
	std::optional<ShopRow> row;
	std::uint64_t boughtWeek = 0, boughtMonth = 0;
	std::expected<sql::LoadedItemOwner, E> account = std::unexpected(E::SqlFailure);
};

entt::entity Current(std::uint32_t pid, std::uint64_t token)
{
	const auto e = CHARACTER_MANAGER::instance().FindEntityByPID(pid);
	const auto* login = e != entt::null && g_registry.valid(e) ? g_registry.try_get<ecs::TypedItemLogin>(e) : nullptr;
	return login && login->token == token ? e : entt::null;
}

void Tell(entt::entity e, const char* text)
{
	if (g_registry.valid(e))
		ecs::ChatSystem::Send(e, CHAT_TYPE_INFO, "%s", text);
}

std::uint64_t Number(const char* text)
{
	return text && *text && *text != '-' ? std::strtoull(text, nullptr, 10) : 0;
}

std::optional<ShopRow> ReadRow(CAsyncSQL& db, std::uint32_t shopId)
{
	std::unique_ptr<SQLMsg> m(db.DirectQueryPrepared(
		"SELECT d.itemVnum,d.itemPrice,d.discount,d.week_limit,d.month_limit,n.category_id,n.local_id,n.refinement_level "
		"FROM player.ishop_data d LEFT JOIN player.ishop_native_item n ON n.shop_id=d.id AND n.legacy_vnum=d.itemVnum WHERE d.id=?",
		{SqlParam::UInt(shopId)}));
	if (!m || m->uiSQLErrno || !m->Get() || m->Get()->uiNumRows != 1)
		return std::nullopt;
	MYSQL_ROW r = m->Get()->FetchRow();
	if (!r)
		return std::nullopt;
	ShopRow row;
	row.vnum = static_cast<std::uint32_t>(Number(r[0]));
	row.price = static_cast<std::uint32_t>(Number(r[1]));
	row.discount = static_cast<std::uint8_t>(std::min<std::uint64_t>(Number(r[2]), 100));
	row.weekLimit = static_cast<int>(std::min<std::uint64_t>(Number(r[3]), INT32_MAX));
	row.monthLimit = static_cast<int>(std::min<std::uint64_t>(Number(r[4]), INT32_MAX));
	if (r[5] && r[6])
		if (const auto key = items::ItemKey::Create(static_cast<std::uint16_t>(Number(r[5])), static_cast<std::uint32_t>(Number(r[6]))))
			row.key = *key;
	row.level = static_cast<std::uint16_t>(Number(r[7]));
	return row;
}

std::uint64_t Bought(CAsyncSQL& db, std::uint32_t account, std::uint32_t vnum, const char* interval)
{
	const std::string query = std::string("SELECT COALESCE(SUM(itemCount),0) FROM player.ishop_log WHERE buyDate > DATE_SUB(NOW(), INTERVAL 1 ")
		+ interval + ") AND accountID=? AND itemVnum=?";
	std::unique_ptr<SQLMsg> m(db.DirectQueryPrepared(query.c_str(), {SqlParam::UInt(account), SqlParam::UInt(vnum)}));
	if (!m || m->uiSQLErrno || !m->Get() || !m->Get()->uiNumRows)
		return UINT64_MAX; // unknown: treated as over the limit
	MYSQL_ROW r = m->Get()->FetchRow();
	return r ? Number(r[0]) : UINT64_MAX;
}

std::array<std::uint8_t, 16> OperationId()
{
	std::array<std::uint8_t, 16> id {};
	while (std::ranges::all_of(id, [](auto b) { return b == 0; }))
		if (RAND_bytes(id.data(), static_cast<int>(id.size())) != 1)
			return {};
	return id;
}

TIShopLogData LogEntry(std::uint32_t account, const std::string& name, const std::string& ip, std::uint32_t vnum, int count, std::uint64_t price)
{
	TIShopLogData log {};
	log.accountID = account;
	strlcpy(log.playerName, name.c_str(), sizeof(log.playerName));
	const time_t now = time(nullptr);
	const struct tm local = auriga::time::LocalTime(now);
	strftime(log.buyDate, sizeof(log.buyDate), "%Y-%m-%d %X", &local);
	log.buyTime = static_cast<int>(now);
	strlcpy(log.ipAdress, ip.c_str(), sizeof(log.ipAdress));
	log.itemVnum = vnum;
	log.itemCount = count;
	log.itemPrice = static_cast<std::uint32_t>(std::min<std::uint64_t>(price, UINT32_MAX));
	return log;
}

// The Database process owns the purchase log and the sell counter.
void RecordPurchase(std::uint32_t shopId, const TIShopLogData& log)
{
	if (!db_clientdesc)
		return;
	const std::uint8_t subIndex = ITEMSHOP_RECORD;
	db_clientdesc->DBPacketHeader(HEADER_GD_ITEMSHOP, 0, sizeof(subIndex) + sizeof(TIShopLogData) + sizeof(shopId));
	db_clientdesc->Packet(&subIndex, sizeof(subIndex));
	db_clientdesc->Packet(&log, sizeof(TIShopLogData));
	db_clientdesc->Packet(&shopId, sizeof(shopId));
}

// Same answer as a legacy purchase: new balance, plus the log line if the log is open.
void SendBalance(entt::entity e, std::uint64_t balance, bool logOpen, const TIShopLogData* log)
{
	auto* desc = ecs::PlayerRuntime::GetDesc(e);
	if (!desc)
		return;
	TPacketGCItemShop p {};
	p.header = HEADER_GC_ITEMSHOP;
	const bool withLog = logOpen && log;
	p.size = sizeof(TPacketGCItemShop) + sizeof(std::uint8_t) + sizeof(std::uint32_t) + sizeof(bool) + (withLog ? sizeof(TIShopLogData) : 0);
	const std::uint8_t subIndex = ITEMSHOP_DRAGONCOIN;
	const std::uint32_t wire = static_cast<std::uint32_t>(std::min<std::uint64_t>(balance, UINT32_MAX));
	TEMP_BUFFER buf;
	buf.write(&p, sizeof(p));
	buf.write(&subIndex, sizeof(subIndex));
	buf.write(&wire, sizeof(wire));
	buf.write(&withLog, sizeof(withLog));
	if (withLog)
		buf.write(log, sizeof(TIShopLogData));
	desc->Packet(buf.read_peek(), buf.size());
}
} // namespace

bool BeginTypedItemShopBuy(entt::entity character, std::uint32_t shopId, int count, bool logOpen)
{
	if (!g_registry.valid(character))
		return false;
	const auto* login = g_registry.try_get<ecs::TypedItemLogin>(character);
	if (!login || login->mode != ecs::TypedItemLogin::Mode::Native || !g_registry.all_of<ecs::TypedItems>(character))
		return false;
	auto* desc = ecs::PlayerRuntime::GetDesc(character);
	if (!desc || !desc->IsPhase(PHASE_GAME) || !desc->GetAccountTable().id || count < 1 || count > 20)
		return true;
	if (g_registry.all_of<ecs::TypedItemBusy>(character))
	{
		Tell(character, "Az előző tárgyművelet még folyamatban van; próbáld újra egy pillanat múlva.");
		return true;
	}
	const auto pid = ecs::PlayerRuntime::GetPlayerID(character);
	const auto token = login->token;
	const std::uint32_t account = desc->GetAccountTable().id;
	const std::string name(ecs::PlayerRuntime::GetName(character));
	const std::string ip = desc->GetHostName();
	g_registry.emplace_or_replace<ecs::TypedItemBusy>(character);

	const bool submitted = store::ItemStore::Instance().Submit([=](CAsyncSQL& db, const items::ItemCatalog& c) -> store::ItemStore::Completion {
		auto loaded = std::make_shared<Loaded>();
		bool swept = false;
		auriga::dragon_coin::SweepAndRead(db, account, swept); // legacy top-ups first
		loaded->row = ReadRow(db, shopId);
		if (loaded->row && loaded->row->key)
		{
			if (loaded->row->weekLimit > 0) loaded->boughtWeek = Bought(db, account, loaded->row->vnum, "WEEK");
			if (loaded->row->monthLimit > 0) loaded->boughtMonth = Bought(db, account, loaded->row->vnum, "MONTH");
			const items::ItemOwner owner { items::ItemOwnerKind::Account, account };
			if (const auto ensured = sql::EnsureRuntimeStorageOwner(db, c, owner); ensured)
				loaded->account = sql::LoadCurrentItemOwner(db, c, owner);
			else
				loaded->account = std::unexpected(ensured.error());
		}
		return [=]() {
			const auto e = Current(pid, token);
			if (e == entt::null)
				return;
			const auto finish = [&](const char* text) {
				g_registry.remove<ecs::TypedItemBusy>(e);
				if (text) Tell(e, text);
				PumpTypedItemGrants(e);
			};
			const auto catalog = store::CurrentCatalog();
			auto* typed = g_registry.try_get<ecs::TypedItems>(e);
			if (!catalog || !typed)
				return finish("Az új tárgyrendszer most nem érhető el; Sárkányérme nem fogyott.");
			if (!loaded->row)
				return finish("Ez a tárgybolti tétel már nem létezik.");
			const auto& row = *loaded->row;
			if (!row.key)
				return finish("Ez a tétel még nincs átállítva az új tárgyrendszerre, ezért most nem vásárolható meg. Sárkányérme nem fogyott.");
			if (row.weekLimit > 0 && loaded->boughtWeek + count > static_cast<std::uint64_t>(row.weekLimit))
				return finish("Ebből a tételből elérted a heti vásárlási korlátot.");
			if (row.monthLimit > 0 && loaded->boughtMonth + count > static_cast<std::uint64_t>(row.monthLimit))
				return finish("Ebből a tételből elérted a havi vásárlási korlátot.");
			if (!loaded->account)
				return finish("A fiók pénztárcája most nem olvasható; Sárkányérme nem fogyott.");
			const auto& wallets = loaded->account->wallets;
			const auto wallet = std::ranges::find(wallets, items::WalletCurrency::DragonCoin, &items::ItemWallet::currency);
			if (wallet == wallets.end())
				return finish("A fiók pénztárcája most nem olvasható; Sárkányérme nem fogyott.");
			std::uint64_t price = static_cast<std::uint64_t>(row.price) * static_cast<std::uint64_t>(count);
			if (row.discount > 0)
				price = price * (100 - row.discount) / 100;
			if (price > wallet->balance)
				return finish("Nincs elég Sárkányérméd ehhez a vásárláshoz.");

			const auto* entry = catalog->Find(*row.key);
			if (!entry)
				return finish("A tétel natív tárgya hiányzik a katalógusból; Sárkányérme nem fogyott.");
			const std::uint32_t stack = entry->base.maxStack ? entry->base.maxStack : 1;
			const std::size_t most = (static_cast<std::size_t>(count) + stack - 1) / stack;
			const auto ids = store::PeekInstanceIds(most);
			const auto& state = typed->state;
			items::ItemGrant grant { *row.key, row.level, static_cast<std::uint32_t>(count) };
			grant.createdAtUtcMs = TypedItemEffectNow();
			const auto operation = OperationId();
			if (std::ranges::all_of(operation, [](auto b) { return b == 0; }))
				return finish("A vásárlás most nem indítható; Sárkányérme nem fogyott.");
			auto commit = items::MakeGrantCommit(*catalog, operation, state.fence, state.storages, state.items, grant, ids);
			if (!commit)
				return finish(commit.error() == E::InvalidPlacement ? "Nincs elég hely a leltárban; Sárkányérme nem fogyott."
					: "Ez a tárgy most nem adható ki; Sárkányérme nem fogyott.");
			if (price)
			{
				auto after = *wallet;
				after.balance -= price;
				++after.revision;
				commit->owners.push_back(loaded->account->fence);
				commit->wallets.push_back({*wallet, after});
			}
			if (const auto valid = items::ValidateItemCommit(*catalog, *commit); !valid)
			{
				LOG_ERROR("ItemShop: pid {} shop item {} native commit invalid (error {})", pid, shopId, static_cast<int>(valid.error()));
				return finish("A vásárlás nem állítható össze; Sárkányérme nem fogyott.");
			}
			store::SpendInstanceIds(static_cast<std::size_t>(std::ranges::count_if(commit->items, [](const items::ItemChange& ch) { return !ch.before; })));
			std::vector<store::OwnerRead> reads { { state.fence.owner, state.fence.sessionGeneration } };
			if (price)
				reads.push_back({ loaded->account->fence.owner, 0 });
			const auto balanceAfter = wallet->balance - price;
			const auto log = LogEntry(account, name, ip, row.vnum, count, price);
			const auto code = entry->base.contentCode;
			const bool queued = store::RequestCommitMany(std::move(reads), std::move(*commit),
				[pid, token, shopId, log, logOpen, balanceAfter, price, code, count](store::MultiCommitOutcome result) {
					if (result.write)
						RecordPurchase(shopId, log); // durable even if the character left meanwhile
					const auto e = Current(pid, token);
					if (e == entt::null)
						return;
					g_registry.remove<ecs::TypedItemBusy>(e);
					if (!result.states.empty() && result.states.front())
						g_registry.emplace_or_replace<ecs::TypedItems>(e, std::move(*result.states.front()));
					if (result.write)
					{
						LOG_INFO("ItemShop: pid {} bought shop item {} ({} x{}) for {} Dragon Coin", pid, shopId, code, count, price);
						ecs::ChatSystem::Send(e, CHAT_TYPE_INFO, "Megvásároltad: %s ×%d (%llu Sárkányérme).", code.c_str(), count,
							static_cast<unsigned long long>(price));
						SendBalance(e, balanceAfter, logOpen, &log);
					}
					else
					{
						LOG_INFO("ItemShop: pid {} purchase of shop item {} refused by the store (error {})", pid, shopId, static_cast<int>(result.write.error()));
						Tell(e, "A vásárlás nem sikerült; Sárkányérme nem fogyott. Próbáld újra.");
						bool ok = false;
						if (auto* d = ecs::PlayerRuntime::GetDesc(e))
							SendBalance(e, auriga::dragon_coin::SweepAndRead(DBManager::instance(), d->GetAccountTable().id, ok), false, nullptr);
					}
					SendTypedInventory(e);
					PumpTypedItemGrants(e);
				});
			if (!queued)
				finish("A tárgytároló most nem fut; Sárkányérme nem fogyott.");
		};
	});
	if (!submitted)
	{
		g_registry.remove<ecs::TypedItemBusy>(character);
		Tell(character, "A tárgytároló most nem fut; Sárkányérme nem fogyott.");
	}
	return true;
}
#else
bool BeginTypedItemShopBuy(entt::entity, std::uint32_t, int, bool) { return false; }
#endif
