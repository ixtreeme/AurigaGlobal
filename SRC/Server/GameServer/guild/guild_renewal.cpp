#include "stdafx.h"
#include <Core/Logging.hpp>
#include "JsonFile.h"
#include <nlohmann/json.hpp>
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../ecs/AIHelpers.hpp"
#include "../ecs/systems/PointSystem.hpp"

#ifdef ENABLE_GUILD_RENEWAL_BY_RAZOR93

#include "guild_renewal.h"

#include "char_interface.hpp"
#include "char_manager.h"
#include "../ecs/CharacterAccessors.hpp"
#include "desc.h"
#include "desc_manager.h"
#include "guild.h"
#include "guild_manager.h"
#include "p2p.h"
#include "packet.h"
#include "db.h"
#include "utils.h"
#include "../item/ItemStore.hpp"
#include "../item/ItemCatalogRuntime.hpp"
#include "../item/TypedQuestItems.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/EntityFactory.hpp"

#include <memory>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unordered_map>
#include "locale_service.h"

namespace
{
	static const int kGuildStorageSize = 15; // 5x3 (client UI)

	namespace items = auriga::item;
	using TKisAdoItemArr = std::array<uint32_t, 5>;
	static std::unordered_map<uint32_t, std::unordered_map<uint32_t, TKisAdoItemArr>> s_kisAdoItemContrib;

	uint64_t PackedMaterial(uint32_t historicalId)
	{
		const auto key = items::GuildProgressMaterial(historicalId);
		return key ? items::PackItemKey(*key) : 0;
	}

	static TKisAdoItemArr KisAdo_GetItemArrCopy(uint32_t guildId, uint32_t pid)
	{
		TKisAdoItemArr z{ {0,0,0,0,0} };
		auto itG = s_kisAdoItemContrib.find(guildId);
		if (itG == s_kisAdoItemContrib.end())
			return z;
		auto itP = itG->second.find(pid);
		if (itP == itG->second.end())
			return z;
		return itP->second;
	}

	static void KisAdo_ClearGuild(uint32_t guildId)
	{
		s_kisAdoItemContrib.erase(guildId);
	}


	static void KisAdo_DBLoadGuild(uint32_t guildId)
	{
		// friss DB load: eloszor toroljuk a memoriat, majd ujratoltjuk
		KisAdo_ClearGuild(guildId);

		std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery(
			"SELECT pid,item0,item1,item2,item3,item4 FROM guild_renewal_kisado_item WHERE guild_id=%u",
			guildId));

		if (!msg || !msg->Get())
			return;

		MYSQL_RES* res = msg->Get()->pSQLResult;
		if (!res)
			return;

		const int rows = (int)mysql_num_rows(res);
		for (int i = 0; i < rows; ++i)
		{
			MYSQL_ROW row = mysql_fetch_row(res);
			if (!row)
				continue;

			uint32_t pid = (uint32_t)atoi(row[0]);
			TKisAdoItemArr arr{ {0,0,0,0,0} };
			arr[0] = (uint32_t)strtoul(row[1] ? row[1] : "0", nullptr, 10);
			arr[1] = (uint32_t)strtoul(row[2] ? row[2] : "0", nullptr, 10);
			arr[2] = (uint32_t)strtoul(row[3] ? row[3] : "0", nullptr, 10);
			arr[3] = (uint32_t)strtoul(row[4] ? row[4] : "0", nullptr, 10);
			arr[4] = (uint32_t)strtoul(row[5] ? row[5] : "0", nullptr, 10);

			s_kisAdoItemContrib[guildId][pid] = arr;
		}
	}


}

CGuildRenewal& CGuildRenewal::instance()
{
	static CGuildRenewal s;
	return s;
}

CGuildRenewal::CGuildRenewal()
{
}

CGuildRenewal::GuildCache& CGuildRenewal::GetCache(uint32_t guildId)
{
	auto& c = m_cache[guildId];
	if (c.storage.empty())
		c.storage.resize(kGuildStorageSize);
	return c;
}

void CGuildRenewal::EnsureLoaded(uint32_t guildId)
{
	auto& c = GetCache(guildId);
	if (c.loaded)
		return;

	// Money
	{
		auto msg = std::unique_ptr<SQLMsg>(DBManager::instance().DirectQuery(
			"SELECT money FROM guild_renewal_money WHERE guild_id=%u", guildId));
		if (msg && msg->Get() && msg->Get()->uiNumRows > 0)
		{
			MYSQL_ROW row = msg->Get()->FetchRow();
			if (row && row[0])
				c.money = atoll(row[0]);
		}
		else
		{
			// ensure row exists
			DBManager::instance().Query("INSERT IGNORE INTO guild_renewal_money(guild_id,money) VALUES(%u,0)", guildId);
			c.money = 0;
		}
	}

	// Tax
	{
		auto msg = std::unique_ptr<SQLMsg>(DBManager::instance().DirectQuery(
			"SELECT active,deadline,per_member_money,"
			"item1_vnum,item1_count,item2_vnum,item2_count,item3_vnum,item3_count,item4_vnum,item4_count,item5_vnum,item5_count "
			"FROM guild_renewal_tax WHERE guild_id=%u", guildId));
		if (msg && msg->Get() && msg->Get()->uiNumRows > 0)
		{
			MYSQL_ROW row = msg->Get()->FetchRow();
			if (row)
			{
				c.tax.active = (atoi(row[0]) != 0);
				str_to_number(c.tax.deadline, row[1]);
				c.tax.perMemberMoney = atoll(row[2]);
				for (int i = 0; i < 5; i++)
				{
					c.tax.vnum[i] = (uint32_t)atoi(row[3 + i * 2]);
					c.tax.count[i] = (uint32_t)atoi(row[4 + i * 2]);
				}
			}
		}
		else
		{
			DBManager::instance().Query(
				"INSERT IGNORE INTO guild_renewal_tax(guild_id,active,deadline,per_member_money) VALUES(%u,0,0,0)", guildId);
			c.tax = Tax{};
		}
	}

	// Storage slots
	{
		auto msg = std::unique_ptr<SQLMsg>(DBManager::instance().DirectQuery(
			"SELECT slot,vnum,count FROM guild_renewal_storage WHERE guild_id=%u", guildId));
		if (msg && msg->Get() && msg->Get()->uiNumRows > 0)
		{
			for (uint64_t i = 0; i < msg->Get()->uiNumRows; i++)
			{
				MYSQL_ROW row = msg->Get()->FetchRow();
				if (!row)
					continue;
				int slot = atoi(row[0]);
				if (slot < 0 || slot >= kGuildStorageSize)
					continue;
				c.storage[slot].vnum = (uint32_t)atoi(row[1]);
				c.storage[slot].count = (uint32_t)atoi(row[2]);
			}
		}
	}

	// Contrib
	{
		auto msg = std::unique_ptr<SQLMsg>(DBManager::instance().DirectQuery(
			"SELECT pid,paid_money,paid_item_total,paid_flag,last_pay_time FROM guild_renewal_contrib WHERE guild_id=%u", guildId));
		if (msg && msg->Get() && msg->Get()->uiNumRows > 0)
		{
			for (uint64_t i = 0; i < msg->Get()->uiNumRows; i++)
			{
				MYSQL_ROW row = msg->Get()->FetchRow();
				if (!row)
					continue;
				uint32_t pid = (uint32_t)atoi(row[0]);
				Contrib cc;
				cc.paidMoney = atoll(row[1]);
				cc.paidItemTotal = atoll(row[2]);
				cc.paidFlag = (uint8_t)atoi(row[3]);
				str_to_number(cc.lastPayTime, row[4]);
				c.contrib[pid] = cc;
			}
		}
	}


	// Kis ado item bontas (DB)
	KisAdo_DBLoadGuild(guildId);

	c.loaded = true;
}

void CGuildRenewal::InvalidateCache(uint32_t guildId)
{
	auto it = m_cache.find(guildId);
	if (it == m_cache.end())
		return;

	GuildCache& c = it->second;
	c.loaded = false;
	c.money = 0;
	c.tax = Tax{};
	c.contrib.clear();
	c.storage.clear();
	KisAdo_ClearGuild(guildId);
}

void CGuildRenewal::InvalidateGuild(uint32_t guildId)
{
	m_cache.erase(guildId);
	KisAdo_ClearGuild(guildId);
}

void CGuildRenewal::P2P_BroadcastRefresh(uint32_t guildId)
{
	TPacketGGGuild p;
	p.bHeader = HEADER_GG_GUILD;
	p.bSubHeader = GUILD_SUBHEADER_GG_RENEWAL_REFRESH;
	p.dwGuild = guildId;

	P2P_MANAGER::instance().Send(&p, sizeof(p));
}

void CGuildRenewal::OnP2PRefresh(uint32_t guildId)
{
	// Mark local cache dirty so next EnsureLoaded pulls the newest state from DB.
	InvalidateCache(guildId);

	// Refresh only if this guild has online members on this core.
	std::vector<entt::entity> members;
	members.reserve(16);

	const DESC_MANAGER::DESC_SET& set = DESC_MANAGER::instance().GetClientSet();
	for (DESC_MANAGER::DESC_SET::const_iterator it = set.begin(); it != set.end(); ++it)
	{
		LPDESC d = *it;
		if (!d)
			continue;

		const entt::entity member = d->GetEntity();
		if (!ecs::IsCharacter(member))
			continue;

		CGuild* g = ecs::SocialSystem::GetGuild(member);
		if (!g)
			continue;

		if (g->GetID() != guildId)
			continue;

		members.push_back(member);
	}

	if (members.empty())
		return;

	// Force load now, then broadcast fresh state to members on this core.
	EnsureLoaded(guildId);
	EnsureLevelReqLoaded();

	for (const entt::entity m : members)
		SendFullStateTo(m);
}

void CGuildRenewal::EnsureLevelReqLoaded()
{
	if (m_levelReqLoaded)
		return;

	// ugyanonnan mint az exppettable.txt
	std::string base = LocaleService_GetBasePath();
	if (!base.empty() && base.back() != '/' && base.back() != '\\')
		base += '/';

	std::string p0 = base + "guild_renewal_levelup.json";
	if (LoadLevelReqFromJson(p0.c_str()))
	{
		LOG_INFO("GUILD_RENEWAL: loaded levelup requirements from {}", p0.c_str());
		m_levelReqLoaded = true;
		return;
	}

	// fallback pathok (ha valamirt nem a locale basepath-bl fut)
	const char* paths[] =
	{
		"../conf/guild_renewal_levelup.json",
		"share/conf/guild_renewal_levelup.json",
		"./guild_renewal_levelup.json",
	};

	for (const char* p : paths)
	{
		if (LoadLevelReqFromJson(p))
		{
			LOG_INFO("GUILD_RENEWAL: loaded levelup requirements from {}", p);
			m_levelReqLoaded = true;
			return;
		}
	}

	LOG_ERROR("GUILD_RENEWAL: FAILED to load guild_renewal_levelup.json. Tried: {}, ../conf/, share/conf/, ./", p0.c_str());

	m_levelReqLoaded = true; // ne prblja minden open-nl jra
}


bool CGuildRenewal::LoadLevelReqFromJson(const char* filename)
{
	nlohmann::json doc;

	if (!JsonFile::Load(filename, doc))
		return false;

	if (!doc.contains("levels") || !doc.at("levels").is_array())
	{
		LOG_ERROR("GUILD_RENEWAL: {} needs a 'levels' array", filename);
		return false;
	}

	// ha tbb helyrl prbljuk, ne keveredjen ssze
	m_levelReqByTargetLevel.clear();

	for (const auto& entry : doc.at("levels"))
	{
		if (!entry.contains("level") || !entry.contains("yang"))
			continue;

		int lvl = entry.value("level", 0);            // cl ch szint (pl 22, ha most 21 a ch)

		if (lvl < 21 || lvl > NSBUZERANT)
			continue;

		LevelReq r;
		r.yang = entry.value("yang", (long long)0);

		if (entry.contains("items") && entry.at("items").is_array())
		{
			const auto& items = entry.at("items");

			for (int i = 0; i < 5 && i < (int)items.size(); ++i)
			{
				r.vnum[i] = items.at(i).value("vnum", 0u);
				r.count[i] = items.at(i).value("count", 0u);
			}
		}

		m_levelReqByTargetLevel[(uint8_t)lvl] = r;
	}

	if (m_levelReqByTargetLevel.empty())
	{
		LOG_ERROR("GUILD_RENEWAL: file loaded but NO valid rows parsed: {}", filename);
		return false;
	}

	return true;
}


uint64_t CGuildRenewal::Storage_Count(uint32_t guildId, uint32_t vnum) const
{
	auto it = m_cache.find(guildId);
	if (it == m_cache.end() || it->second.storage.empty())
		return 0;

	uint64_t sum = 0;
	for (const auto& s : it->second.storage)
		if (s.vnum == vnum)
			sum += s.count;
	return sum;
}

void CGuildRenewal::SendFullStateTo(entt::entity chEntity)
{
	if (!ecs::IsCharacter(chEntity) || !ecs::PlayerRuntime::GetDesc(chEntity))
		return;

	CGuild* g = ecs::SocialSystem::GetGuild(chEntity);
	if (!g)
		return;

	const uint32_t guildId = g->GetID();

	EnsureLoaded(guildId);
	EnsureLevelReqLoaded();

	auto& c = GetCache(guildId);

	// UI reset + guild storage money
	ecs::ChatSystem::Send(chEntity, CHAT_TYPE_COMMAND, "gr_clear");
	ecs::ChatSystem::Send(chEntity, CHAT_TYPE_COMMAND, "gr_money %lld", (long long)c.money);
	const auto catalog = items::runtime::CurrentCatalog();
	std::set<items::ItemKey> sentMaterials;
	const auto sendMaterial = [&](uint32_t material) {
		const auto key = items::GuildProgressMaterial(material);
		if (!catalog || !key || !sentMaterials.insert(*key).second)
			return;
		const auto* entry = catalog->Find(*key);
		if (!entry)
			return;
		const auto& base = entry->base;
		ecs::ChatSystem::Send(chEntity, CHAT_TYPE_COMMAND, "gr_material %llu %s %s %s %s %u %u",
			static_cast<unsigned long long>(items::PackItemKey(*key)), base.contentCode.c_str(),
			base.nameKey.c_str(), base.descriptionKey.empty() ? "-" : base.descriptionKey.c_str(),
			base.iconAsset.empty() ? "-" : base.iconAsset.c_str(), unsigned{base.width}, unsigned{base.height});
	};

	// per-player paid flag (Ad: O/X a kliensben)
	{
		uint8_t paidFlag = 0;
		auto itPaid = c.contrib.find(ecs::PlayerRuntime::GetPlayerID(chEntity));
		if (itPaid != c.contrib.end())
			paidFlag = itPaid->second.paidFlag;

		ecs::ChatSystem::Send(chEntity, CHAT_TYPE_COMMAND, "gr_paid %u", (unsigned)paidFlag);
	}

	// next guild level requirements (yang + 5 item)
	{
		const uint8_t target = (uint8_t)(g->GetLevel() + 1);

		long long reqYang = 0;
		uint64_t rv[5] = { 0,0,0,0,0 };
		uint32_t rc[5] = { 0,0,0,0,0 };

		auto itReq = m_levelReqByTargetLevel.find(target);
		if (itReq != m_levelReqByTargetLevel.end())
		{
			reqYang = (long long)itReq->second.yang;
			for (int i = 0; i < 5; ++i)
			{
				sendMaterial(itReq->second.vnum[i]);
				rv[i] = PackedMaterial(itReq->second.vnum[i]);
				rc[i] = itReq->second.count[i];
			}
		}
		else
		{
			LOG_INFO("GUILD_RENEWAL: no req row for target level={} (guild level={})", (unsigned)target, g->GetLevel());
		}

		ecs::ChatSystem::Send(chEntity, CHAT_TYPE_COMMAND, "gr_req2 %u %lld %llu %u %llu %u %llu %u %llu %u %llu %u",
			static_cast<unsigned>(target), reqYang,
			static_cast<unsigned long long>(rv[0]), rc[0],
			static_cast<unsigned long long>(rv[1]), rc[1],
			static_cast<unsigned long long>(rv[2]), rc[2],
			static_cast<unsigned long long>(rv[3]), rc[3],
			static_cast<unsigned long long>(rv[4]), rc[4]);
	}

	// storage slots
	for (uint16_t i = 0; i < c.storage.size(); ++i)
	{
		auto& s = c.storage[i];
		if (s.vnum && s.count)
		{
			sendMaterial(s.vnum);
			ecs::ChatSystem::Send(chEntity, CHAT_TYPE_COMMAND, "gr_item2 %u %llu %u", (unsigned)i,
				static_cast<unsigned long long>(PackedMaterial(s.vnum)), s.count);
		}
	}

	// tax request state (kis ad rendszerben nincs kivetett ad)
	ecs::ChatSystem::Send(chEntity, CHAT_TYPE_COMMAND, "gr_tax 0 0 0 0 0 0 0 0 0 0 0 0 0");


	// contrib list
	for (auto& kv : c.contrib)
	{
		const TKisAdoItemArr itemArr = KisAdo_GetItemArrCopy(guildId, kv.first);
		ecs::ChatSystem::Send(chEntity, CHAT_TYPE_COMMAND, "gr_contrib2 %u %u %lld %lld %u %u %u %u %u",
			(unsigned)kv.first,
			(unsigned)kv.second.paidFlag,
			(long long)kv.second.paidMoney,
			(long long)kv.second.paidItemTotal,
			(unsigned)itemArr[0],
			(unsigned)itemArr[1],
			(unsigned)itemArr[2],
			(unsigned)itemArr[3],
			(unsigned)itemArr[4]);

		ecs::ChatSystem::Send(chEntity, CHAT_TYPE_COMMAND, "gr_contrib %u %u %lld %lld",
			(unsigned)kv.first,
			(unsigned)kv.second.paidFlag,
			(long long)kv.second.paidMoney,
			(long long)kv.second.paidItemTotal);
	}

	ecs::ChatSystem::Send(chEntity, CHAT_TYPE_COMMAND, "gr_done");
}


items::GuildProgress CGuildRenewal::ProgressSnapshot(uint32_t guildId, uint8_t currentLevel)
{
	EnsureLoaded(guildId);
	const auto& cached = GetCache(guildId);
	items::GuildProgress progress;
	progress.guild = guildId;
	progress.currentLevel = currentLevel;
	progress.moneyBefore = static_cast<uint64_t>(std::max<int64_t>(0, cached.money));
	for (std::size_t i = 0; i < cached.storage.size() && i < progress.before.size(); ++i)
	{
		const auto& slot = cached.storage[i];
		if (slot.vnum && slot.count)
			progress.before[i] = {slot.vnum, slot.count};
	}
	progress.after = progress.before;
	return progress;
}

bool CGuildRenewal::RequestProgress(entt::entity character, items::GuildProgress progress,
	std::vector<items::MissionItemAmount> costs, std::optional<items::ItemInstanceId> source)
{
	const auto pid = ecs::PlayerRuntime::GetPlayerID(character);
	const auto submitted = RequestTypedGuildExchange(character, std::move(costs), progress.moneyPaid,
		items::GuildProgressOutcome, items::EncodeGuildProgress(progress),
		[this, pid, guildId = progress.guild, level = progress.currentLevel, upgrade = progress.levelUp](bool committed) {
			if (committed)
			{
				if (upgrade)
					if (auto* guild = CGuildManager::instance().FindGuild(guildId))
						guild->RenewalSetLevel(static_cast<uint8_t>(level + 1), true);
				OnP2PRefresh(guildId);
				P2P_BroadcastRefresh(guildId);
				return;
			}
			OnP2PRefresh(guildId);
			const auto member = CHARACTER_MANAGER::instance().FindEntityByPID(pid);
			if (ecs::IsCharacter(member))
				ecs::ChatSystem::Send(member, CHAT_TYPE_INFO, "A céh befizetései megváltoztak, vagy a művelet nem sikerült. Próbáld újra.");
		}, source);
	if (!submitted)
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "A céhfejlesztés jelenleg nem elérhető.");
	return submitted.has_value();
}

bool CGuildRenewal::DepositItem(entt::entity character, uint32_t target, uint64_t itemId, uint32_t count)
{
	const auto id = items::ItemInstanceId::Create(itemId);
	if (!id)
		return false;
	std::array<uint32_t, 5> counts{};
	counts.front() = count ? count : UINT32_MAX;
	return PayCustom(character, target, 0, counts, *id);
}

bool CGuildRenewal::PayCustom(entt::entity character, uint32_t target, int64_t yang,
	const std::array<uint32_t, 5>& counts, std::optional<items::ItemInstanceId> source)
{
	auto* guild = ecs::SocialSystem::GetGuild(character);
	const auto* typed = g_registry.try_get<ecs::TypedItems>(character);
	if (!guild || !typed || guild->GetLevel() < 20 || target != uint32_t{guild->GetLevel()} + 1)
		return false;
	EnsureLevelReqLoaded();
	const auto requirement = m_levelReqByTargetLevel.find(static_cast<uint8_t>(target));
	if (requirement == m_levelReqByTargetLevel.end())
		return false;
	const auto& req = requirement->second;
	auto progress = ProgressSnapshot(guild->GetID(), guild->GetLevel());
	const auto& state = typed->state;
	const auto owner = state.fence.owner;
	const items::PersistentItem* selected = nullptr;
	if (source)
	{
		const auto found = std::ranges::find(state.items, *source, &items::PersistentItem::id);
		if (found == state.items.end() || found->refinementLevel != 0 ||
			!(items::PlainInventoryItem(*found, owner) || items::PlainInventoryItem(*found, owner, items::ItemStorageKind::SpecialInventory)))
			return false;
		selected = &*found;
	}

	const auto wallet = std::ranges::find(state.wallets, items::WalletCurrency::Gold, &items::ItemWallet::currency);
	if (yang > 0 && req.yang > 0 && static_cast<uint64_t>(req.yang) > progress.moneyBefore && wallet != state.wallets.end())
		progress.moneyPaid = std::min({static_cast<uint64_t>(yang), static_cast<uint64_t>(req.yang) - progress.moneyBefore, wallet->balance});

	std::vector<items::MissionItemAmount> costs;
	for (std::size_t index = 0; index < req.vnum.size(); ++index)
	{
		const auto material = req.vnum[index];
		const auto key = items::GuildProgressMaterial(material);
		if (!key || !req.count[index] || (selected && selected->key != *key))
			continue;
		const auto existing = std::ranges::find(costs, *key, &items::MissionItemAmount::key);
		std::uint64_t alreadyPaid = existing != costs.end() ? existing->count : 0;
		std::uint64_t wanted = selected ? counts.front() : counts[index];
		const auto available = selected ? selected->count : items::CountGameplayExchangeItems(state.items, owner, *key, 0);
		const auto have = Storage_Count(progress.guild, material) + alreadyPaid;
		if (have >= req.count[index] || available <= alreadyPaid)
			continue;
		const auto paid = static_cast<std::uint32_t>(std::min({wanted, req.count[index] - have,
			available - alreadyPaid, uint64_t{50'000} - alreadyPaid}));
		if (!paid)
			continue;

		auto slot = std::ranges::find(progress.after, material, &items::GuildProgressSlot::material);
		if (slot == progress.after.end())
			slot = std::ranges::find(progress.after, 0u, &items::GuildProgressSlot::count);
		if (slot == progress.after.end() || UINT32_MAX - slot->count < paid)
			continue;
		*slot = {material, slot->count + paid};
		progress.contributed[index] = paid;
		if (existing == costs.end())
			costs.push_back({*key, 0, paid});
		else
			existing->count += paid;
		if (selected)
			break;
	}

	if (!progress.moneyPaid && costs.empty())
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Nincs befizethető tárgy vagy Yang az aktuális fejlesztéshez.");
		return false;
	}
	return RequestProgress(character, progress, std::move(costs), source);
}

bool CGuildRenewal::TryLevelUp(entt::entity character)
{
	auto* guild = ecs::SocialSystem::GetGuild(character);
	if (!guild || guild->GetMasterPID() != ecs::PlayerRuntime::GetPlayerID(character) ||
		guild->GetLevel() < 20 || guild->GetLevel() >= NSBUZERANT)
		return false;
	EnsureLevelReqLoaded();
	const auto requirement = m_levelReqByTargetLevel.find(static_cast<uint8_t>(guild->GetLevel() + 1));
	if (requirement == m_levelReqByTargetLevel.end())
		return false;
	const auto& req = requirement->second;
	auto progress = ProgressSnapshot(guild->GetID(), guild->GetLevel());
	if (req.yang < 0 || progress.moneyBefore < static_cast<uint64_t>(req.yang))
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Nincs elegendő Yang a céh leltárában.");
		return false;
	}
	std::unordered_map<uint32_t, uint64_t> required;
	for (std::size_t index = 0; index < req.vnum.size(); ++index)
		if (req.count[index])
			required[req.vnum[index]] += req.count[index];
	for (const auto& [material, count] : required)
		if (!items::GuildProgressMaterial(material) || Storage_Count(progress.guild, material) < count)
		{
			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Hiányzik fejlesztési tárgy a céh leltárából.");
			return false;
		}
	progress.levelUp = true;
	progress.after = {};
	return RequestProgress(character, progress, {});
}

#endif // ENABLE_GUILD_RENEWAL_BY_RAZOR93

