#include "stdafx.h"
#include "../dungeon/DungeonUiEvents.hpp"
#include "TypedItemOps.hpp"
#include "TypedQuestItems.hpp"
#include "TypedMoney.hpp"
#include "TypedPrivateShops.hpp"
#include "TypedWorldUtilities.hpp"
#include "TypedDungeonItems.hpp"
#include "TypedRecovery.hpp"
#include "TypedDrops.hpp"
#include "ItemCatalogRuntime.hpp"
#include "ItemStore.hpp"
#include "TypedInventorySync.hpp"
#include "TypedEquipment.hpp"
#include "TypedCostumes.hpp"
#include "TypedTools.hpp"
#include "refine.h"
#include "../command/cmd.h"
#include "../core/packet.h"
#include "../core/utils.h"
#include "../ecs/Registry.hpp"
#include "../ecs/AIHelpers.hpp"
#include "../ecs/ResourceAdmission.hpp"
#include "../ecs/services/VisibilityService.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../ecs/systems/InventorySystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/NetworkSyncSystem.hpp"
#include "../ecs/systems/SessionSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../ecs/systems/MovementSystem.hpp"
#include "../ecs/components/status_components.hpp"
#include "../ecs/components/social_components.hpp"
#include "../ecs/components/quest_components.hpp"
#include "../quest/questpc.h"
#include "../network/desc.h"
#include "../entity/char_manager.h"
#include "../combat/pvp.h"
#include "../quest/questmanager.h"
#include "../world/arena.h"
#include "../world/war_map.h"
#include "../social/marriage.h"

#include <Core/Logging.hpp>
#include <item/Consumables.hpp>
#include <item/ItemCatalog.hpp>
#include <item/BuiltinUtilityRuntime.hpp>
#include "../ecs/systems/AffectSystem.hpp"
#include "../combat/affect.h"
#include "../core/config.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <random>
#include <string>
#include <type_traits>
#include <variant>

bool IS_SUMMONABLE_ZONE(int map_index);

namespace
{
namespace items = auriga::item;

const char* ErrorText(items::ItemPersistenceError e)
{
	static const char* texts[] = { "érvénytelen tulajdonos", "érvénytelen tároló", "érvénytelen tárgyállapot", "nincs ilyen tárgy",
		"ez a kategória még nem támogatott", "érvénytelen szint", "érvénytelen bónusz", "érvénytelen foglalat",
		"nincs hely, vagy ütközik egy másik tárggyal", "érvénytelen művelet", "elavult munkamenet (lépj be újra)",
		"érvénytelen változás", "érvénytelen pénztárca", "túl sok adat", "katalóguseltérés", "hash-hiba", "adatbázishiba",
		"ütközés egy párhuzamos mentéssel", "a mentés eredménye ismeretlen", "nyugtaeltérés", "sérült tárolt adat", "nincs kiépítve", "a karakter nem felel meg a feltételeknek (szint, kaszt vagy nem)", "ez a tárgy nem dobható el vagy adható át" };
	const auto code = static_cast<std::size_t>(e);
	return code < std::size(texts) ? texts[code] : "ismeretlen hiba";
}

std::mt19937_64& Engine()
{
	static std::mt19937_64 engine { std::random_device {}() ^ (static_cast<std::uint64_t>(std::random_device {}()) << 32) };
	return engine;
}

std::array<std::uint8_t, 16> NewOperationId()
{
	auto& engine = Engine();
	std::array<std::uint8_t, 16> id {};
	do
		for (auto& byte : id) byte = static_cast<std::uint8_t>(engine());
	while (std::ranges::all_of(id, [](auto b) { return b == 0; }));
	return id;
}

void Tell(entt::entity character, const char* text)
{
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "%s", text);
}
} // namespace

static std::expected<void, items::ItemMissionError> RequestTypedItemExchange(entt::entity character,
	std::vector<items::MissionItemAmount> costs, std::vector<items::MissionItemAmount> rewards,
	std::uint64_t goldCost, std::function<void(bool)> done,
	quest::TypedItemUseSource source, std::uint64_t goldReward, bool inQuest)
{
	using Error = items::ItemMissionError;
	if (!g_registry.valid(character) || g_registry.all_of<ecs::TypedItemBusy>(character))
		return std::unexpected(Error::Unavailable);

	const auto catalog = items::runtime::CurrentCatalog();
	const auto* typed = g_registry.try_get<ecs::TypedItems>(character);
	const auto* login = g_registry.try_get<ecs::TypedItemLogin>(character);
	const auto* quest = g_registry.try_get<ecs::QuestPCState>(character);
	auto* desc = ecs::PlayerRuntime::GetDesc(character);
	if (!catalog || !typed || !login || !quest || !quest->pc || inQuest!=quest->pc->IsRunning() ||
		!desc || !desc->IsPhase(PHASE_GAME) || !InventorySystem::CanHandleItems(character))
		return std::unexpected(Error::Unavailable);

	const auto fence = typed->state.fence;
	const auto pid = ecs::PlayerRuntime::GetPlayerID(character);
	const auto token = login->token;
	const auto current = [pid, token] {
		const auto entity = CHARACTER_MANAGER::instance().FindEntityByPID(pid);
		const auto* active = g_registry.valid(entity) ? g_registry.try_get<ecs::TypedItemLogin>(entity) : nullptr;
		return active && active->token == token ? entity : entt::null;
	};
	auto permit = ecs::resource_admission::Enter({character}, auriga::resource_admission::Operation::ItemUse);
	if (!permit || !permit->Current() || current() != character)
		return std::unexpected(Error::Unavailable);
	typed = g_registry.try_get<ecs::TypedItems>(character);
	if (!typed || typed->state.fence != fence || g_registry.all_of<ecs::TypedItemBusy>(character))
		return std::unexpected(Error::Unavailable);

	const auto& state = typed->state;
	std::optional<items::ItemInstanceId> sourceId;
	if (source.id)
	{
		const auto id = items::ItemInstanceId::Create(source.id);
		if (!id)
			return std::unexpected(Error::Requirements);
		sourceId = *id;
		const auto selected = std::ranges::find(state.items, *sourceId, &items::PersistentItem::id);
		if (selected == state.items.end() || selected->revision != source.revision ||
			!items::MissionItemUsable(*selected, fence.owner))
			return std::unexpected(Error::Requirements);
		costs = {{selected->key, selected->refinementLevel, 1}};
	}

	items::ItemMissionProof proof{fence.owner};
	const auto nonce = NewOperationId();
	std::ranges::copy(nonce, proof.action.begin());
	items::ItemMission exchange;
	exchange.id = exchange.version = 1;
	exchange.code = "quest.item_exchange";
	exchange.name = "Quest item exchange";
	exchange.costs = std::move(costs);
	exchange.rewards = std::move(rewards);
	exchange.goldCost = goldCost;
	exchange.goldReward = goldReward;
	const auto ids = exchange.rewards.empty() ? std::vector<items::ItemInstanceId>{}
		: items::runtime::PeekInstanceIds(items::MaxItemCommitItems);
	auto commit = inQuest
		? items::MakeItemMissionCommit(*catalog, exchange, proof, 0, fence, state.storages, state.items, state.wallets, ids, TypedItemEffectNow(), sourceId)
		: items::MakeGameplayItemExchangeCommit(*catalog, exchange, proof, fence, state.storages, state.items, state.wallets, ids, TypedItemEffectNow());
	if (!commit)
		return std::unexpected(commit.error());
	if (goldReward && std::ranges::any_of(commit->wallets, [](const auto& change) { return change.after.balance >= GOLD_MAX; }))
		return std::unexpected(Error::Limit);

	items::runtime::SpendInstanceIds(static_cast<std::size_t>(std::ranges::count_if(
		commit->items, [](const items::ItemChange& change) { return !change.before; })));
	auto held = std::make_shared<auriga::resource_admission::Permit>(std::move(*permit));
	g_registry.emplace<ecs::TypedItemBusy>(character);
	if (!items::runtime::RequestCommit(fence.owner, fence.sessionGeneration, std::move(*commit),
		[current, done = std::move(done), held = std::move(held)](items::runtime::CommitOutcome result) mutable {
			const auto entity = current();
			if (entity == entt::null)
				return;
			if (result.state)
				g_registry.emplace_or_replace<ecs::TypedItems>(entity, std::move(*result.state));
			g_registry.remove<ecs::TypedItemBusy>(entity);
			held.reset();
			SendTypedInventory(entity);
			if (current() != entity)
				return;
			if (done)
				done(result.write.has_value());
			if (current() == entity)
				PumpTypedItemGrants(entity);
		}))
	{
		g_registry.remove<ecs::TypedItemBusy>(character);
		return std::unexpected(Error::Unavailable);
	}
	return {};
}

std::expected<void, items::ItemMissionError> RequestTypedQuestItemExchange(entt::entity character,
	std::vector<items::MissionItemAmount> costs, std::vector<items::MissionItemAmount> rewards,
	std::uint64_t goldCost, std::function<void(bool)> done, quest::TypedItemUseSource source, std::uint64_t goldReward)
{
	return RequestTypedItemExchange(character,std::move(costs),std::move(rewards),goldCost,std::move(done),source,goldReward,true);
}

std::expected<void, items::ItemMissionError> RequestTypedGameplayItemExchange(entt::entity character,
	std::vector<items::MissionItemAmount> costs, std::vector<items::MissionItemAmount> rewards,
	std::uint64_t goldCost, std::function<void(bool)> done)
{
	return RequestTypedItemExchange(character,std::move(costs),std::move(rewards),goldCost,std::move(done),{},0,false);
}

void RequestTypedItemMove(entt::entity character, std::uint64_t itemId, std::uint8_t storageKind, std::uint64_t storageId, std::uint32_t cell)
{
    if(TypedGatheringActive(character)){Tell(character,"Gyűjtés közben a tárgyak nem helyezhetők át.");return;}
	if (!g_registry.valid(character))
		return;
	auto* typed = g_registry.try_get<ecs::TypedItems>(character);
	const auto* login = g_registry.try_get<ecs::TypedItemLogin>(character);
	const auto catalog = items::runtime::CurrentCatalog();
	if (!typed || !login || !catalog)
	{
		Tell(character, "Az új tárgyrendszer ennél a karakternél nincs bekapcsolva.");
		return;
	}
	if (g_registry.all_of<ecs::TypedItemBusy>(character))
	{
		Tell(character, "Az előző tárgyművelet még folyamatban van.");
		return;
	}
	const auto id = items::ItemInstanceId::Create(itemId);
	const items::ItemPlacement target { static_cast<items::ItemStorageKind>(storageKind), storageId, cell };
	const auto& state = typed->state;
	const auto current = id ? std::ranges::find_if(state.items, [&](const items::PersistentItem& i) { return i.id == *id && !i.destroyed; }) : state.items.end();
	std::expected<items::ItemCommit, items::ItemPersistenceError> commit = std::unexpected(items::ItemPersistenceError::MissingItem);
	if (current != state.items.end() && current->placement)
	{
        const auto elapsed=TypedCostumeElapsed(character);
        items::EquipContext who{static_cast<std::uint16_t>(std::max(0,ecs::PointSystem::GetLevel(character))),ecs::PlayerRuntime::GetJob(character),ecs::PlayerRuntime::GetSex(character)};
        who.nowUtcMs=TypedItemEffectNow();who.costumeElapsed=elapsed;who.inCombat=TypedItemEffectFacts(character).actor.inCombat;
		// One request, three operations: equip into the item's own slot, take
		// off into the inventory, or move inside the inventory (C3/C4).
		if (target.kind == items::ItemStorageKind::Equipment)
		{

			commit = items::MakeEquipCommit(*catalog, NewOperationId(), state.fence, state.storages, state.items, *id, who);
		}
		else if (current->placement->kind == items::ItemStorageKind::Equipment)
			commit = items::MakeUnequipCommit(*catalog, NewOperationId(), state.fence, state.storages, state.items, *id, target,who);
		else
			commit = items::MakeMoveCommit(*catalog, NewOperationId(), state.fence, state.storages, state.items, *id, target);
	}
	if (!commit)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "A tárgyművelet nem lehetséges: %s.", ErrorText(commit.error()));
		SendTypedInventory(character); // the client reverts to the authoritative layout
		return;
	}
	if (target.kind == items::ItemStorageKind::Equipment)
	{
		auto equipped = state.items;
		for (const auto& change : commit->items)
			if (auto item = std::ranges::find(equipped, change.after.id, &items::PersistentItem::id); item != equipped.end()) *item = change.after;
		if (!items::BuildEquipmentStats(*catalog, state.fence.owner, equipped))
		{ Tell(character, "Ez a felszerelés még nem támogatott vagy túl nagy statértéket tartalmaz."); SendTypedInventory(character); return; }
	}
	const uint32_t pid = ecs::PlayerRuntime::GetPlayerID(character);
	const uint64_t token = login->token;
	g_registry.emplace_or_replace<ecs::TypedItemBusy>(character);
	const bool queued = items::runtime::RequestCommit(typed->state.fence.owner, typed->state.fence.sessionGeneration, std::move(*commit),
		[pid, token](items::runtime::CommitOutcome outcome) {
			const entt::entity entity = CHARACTER_MANAGER::instance().FindEntityByPID(pid);
			const auto* current = entity != entt::null && g_registry.valid(entity) ? g_registry.try_get<ecs::TypedItemLogin>(entity) : nullptr;
			if (!current || current->token != token)
				return; // that login is gone; the next login reloads anyway
			g_registry.remove<ecs::TypedItemBusy>(entity);
			if (outcome.state)
				g_registry.emplace_or_replace<ecs::TypedItems>(entity, std::move(*outcome.state));
			else
				LOG_ERROR("TypedItems: pid {} reload after a move failed (error {})", pid, static_cast<int>(outcome.state.error()));
			if (!outcome.write)
			{
				LOG_INFO("TypedItems: pid {} move refused by the store (error {})", pid, static_cast<int>(outcome.write.error()));
				ecs::ChatSystem::Send(entity, CHAT_TYPE_INFO, "A tárgyművelet nem sikerült: %s.", ErrorText(outcome.write.error()));
			}
			SendTypedInventory(entity);
			PumpTypedItemGrants(entity); // drops that waited for this answer
		});
	if (!queued)
	{
		g_registry.remove<ecs::TypedItemBusy>(character);
		Tell(character, "A tárgytároló nem fut; az áthelyezés nem lehetséges.");
	}
}

static_assert(sizeof(TPacketCGTypedItemMove) == 22, "CG_TYPED_ITEM_MOVE wire size is pinned by the Unity writer");

bool HandleTypedItemMovePacket(entt::entity character, const char* data)
{
	const auto* packet = reinterpret_cast<const TPacketCGTypedItemMove*>(data);
	RequestTypedItemMove(character, packet->qwItemId, packet->bStorageKind, packet->qwStorageId, packet->dwCell);
	return true;
}

// Test command until the client inventory UI exists (F5):
// /typed_move <from cell> <to cell>   (main inventory, storage 1)
ACMD(do_typed_move)
{
	char from[32], to[32];
	two_arguments(argument, from, sizeof(from), to, sizeof(to));
	const auto* typed = g_registry.valid(character) ? g_registry.try_get<ecs::TypedItems>(character) : nullptr;
	if (!*from || !*to || !typed)
	{
		Tell(character, "Használat: /typed_move <honnan cella> <hová cella>  (új tárgyrendszer, fő inventory)");
		return;
	}
	const auto source = static_cast<std::uint32_t>(std::strtoul(from, nullptr, 10));
	const auto target = static_cast<std::uint32_t>(std::strtoul(to, nullptr, 10));
	for (const auto& item : typed->state.items)
		if (!item.destroyed && item.placement && item.placement->kind == items::ItemStorageKind::Inventory &&
			item.placement->storageId == 1 && item.placement->anchor == source)
		{
			RequestTypedItemMove(character, item.id.Value(), static_cast<std::uint8_t>(items::ItemStorageKind::Inventory), 1, target);
			return;
		}
	Tell(character, "Ezen a cellán nincs új rendszerű tárgy.");
}

namespace
{
const items::PersistentItem* ItemAt(const ecs::TypedItems& typed, items::ItemStorageKind kind, std::uint32_t anchor)
{
	for (const auto& item : typed.state.items)
		if (!item.destroyed && item.placement && item.placement->kind == kind && item.placement->storageId == 1 && item.placement->anchor == anchor)
			return &item;
	return nullptr;
}
}

// /typed_equip <inventory cell>: equip into the item's own slot (C4 test command).
ACMD(do_typed_equip)
{
	char from[32];
	one_argument(argument, from, sizeof(from));
	const auto* typed = g_registry.valid(character) ? g_registry.try_get<ecs::TypedItems>(character) : nullptr;
	if (!*from || !typed)
	{
		Tell(character, "Használat: /typed_equip <inventory cella>  (új tárgyrendszer)");
		return;
	}
	if (const auto* item = ItemAt(*typed, items::ItemStorageKind::Inventory, static_cast<std::uint32_t>(std::strtoul(from, nullptr, 10))))
		RequestTypedItemMove(character, item->id.Value(), static_cast<std::uint8_t>(items::ItemStorageKind::Equipment), 1, 0);
	else
		Tell(character, "Ezen a cellán nincs új rendszerű tárgy.");
}

// /typed_unequip <slot 1-8> <inventory cell>: take an item off (C4 test command).
ACMD(do_typed_unequip)
{
	char slot[32], to[32];
	two_arguments(argument, slot, sizeof(slot), to, sizeof(to));
	const auto* typed = g_registry.valid(character) ? g_registry.try_get<ecs::TypedItems>(character) : nullptr;
	const auto number = static_cast<std::uint32_t>(std::strtoul(slot, nullptr, 10));
	if (!*slot || !*to || !typed || number < 1 || number > 8)
	{
		Tell(character, "Használat: /typed_unequip <hely 1-8> <inventory cella>  (1 fegyver, 2 test, 3 fej, 4 pajzs, 5 láb, 6 csukló, 7 nyak, 8 fül)");
		return;
	}
	if (const auto* item = ItemAt(*typed, items::ItemStorageKind::Equipment, number - 1))
		RequestTypedItemMove(character, item->id.Value(), static_cast<std::uint8_t>(items::ItemStorageKind::Inventory), 1,
			static_cast<std::uint32_t>(std::strtoul(to, nullptr, 10)));
	else
		Tell(character, "Ezen a helyen nincs felszerelt új rendszerű tárgy.");
}

// ---------------------------------------------------------------------------
// Use (F4 C5): instant recovery consumables. PlanInstantRecovery decides from
// the running character; the store spends the units; the healing, cooldown and
// arena quota follow the store's success, on the character as it is by then.

namespace
{
const char* UseErrorText(items::RecoveryUseError e)
{
	using E = items::RecoveryUseError;
	switch (e)
	{
	case E::MissingItem: return "nincs ilyen tárgy";
	case E::WrongCategory:
	case E::WrongDelivery: return "ez a tárgy így nem használható";
	case E::RetiredItem: return "ez a tárgy már nincs forgalomban";
	case E::InvalidState: return "érvénytelen állapot";
	case E::DeadCharacter: return "halott karakter nem használhatja";
	case E::ContextDenied: return "itt nem használható";
	case E::PvpDenied: return "PvP közben nem használható";
	case E::CombatDenied: return "harc közben nem használható";
	case E::LevelDenied: return "túl alacsony a szinted";
	case E::ClassDenied: return "a kasztod nem használhatja";
	case E::SexDenied: return "a karaktered neme nem használhatja";
	case E::OnCooldown: return "még nem használható újra";
	case E::InsufficientUnits: return "nincs elég belőle";
	case E::NoEffect: return "most nem lenne hatása";
	case E::ArenaDenied: return "az arénában most nem használható";
	case E::QueueFull: return "megtelt a visszatöltési tartalék";
	case E::ArenaQuotaExceeded: return "elfogyott az aréna-kereted";
	}
	return "ismeretlen hiba";
}

std::int64_t NowMs()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// The facts PlanInstantRecovery reads, from the running character. Pools are
// raw (1000 raw = 1 point, IC-003). Context: an arena map is Arena, a guild
// war map GuildWar, a PvP duel Duel, anything else World.
items::InstantRecoveryAdmission RecoveryAdmission(entt::entity character, std::optional<std::uint32_t> cooldownProfile, std::uint32_t units)
{
	items::InstantRecoveryAdmission state;
	const auto pool = [](std::uint64_t current, std::uint64_t max) {
		if(max>static_cast<std::uint64_t>(items::MaxItemStatRaw/1000))return std::pair<std::int64_t,std::int64_t>{-1,-1};
		return std::pair { static_cast<std::int64_t>(std::min(current, max)) * 1000, static_cast<std::int64_t>(max) * 1000 };
	};
	const auto [hp, maxHp] = pool(ecs::PlayerRuntime::GetHP(character), ecs::PointSystem::GetMaxHP(character));
	const auto [sp, maxSp] = pool(ecs::PlayerRuntime::GetSP(character), ecs::PointSystem::GetMaxSP(character));
	state.pools = { hp, maxHp, sp, maxSp };
	state.potionBonusBp = static_cast<std::int32_t>(std::clamp<std::int64_t>(ecs::PointSystem::Get(character, POINT_POTION_BONUS) * 100, -10000, 100000));
	if (cooldownProfile)
		if (const auto* cooldowns = g_registry.try_get<ecs::TypedItemCooldowns>(character))
			if (const auto it = cooldowns->readyAtMs.find(*cooldownProfile); it != cooldowns->readyAtMs.end())
				state.cooldownRemainingMs = static_cast<std::uint64_t>(std::max<std::int64_t>(0, it->second - NowMs()));
	if(cooldownProfile)state.cooldownRemainingMs=std::max(state.cooldownRemainingMs,TypedRecoveryCooldown(character,*cooldownProfile));
	const auto map = ecs::PlayerRuntime::GetMapIndex(character);
	state.inPvp = CPVPManager::instance().IsFighting(ecs::PlayerRuntime::GetPlayerID(character));
	state.context = CArenaManager::instance().IsArenaMap(map) ? items::RecoveryContext::Arena
		: CWarMapManager::instance().IsWarMap(map) ? items::RecoveryContext::GuildWar
		: state.inPvp ? items::RecoveryContext::Duel : items::RecoveryContext::World;
	state.inCombat = ecs::PlayerRuntime::GetPosition(character) == POS_FIGHTING;
	state.level = static_cast<std::uint16_t>(std::clamp(ecs::PointSystem::GetLevel(character), 0, 65535));
	state.classIndex = ecs::PlayerRuntime::GetJob(character);
	state.sexIndex = ecs::PlayerRuntime::GetSex(character);
	state.availableUnits = units;
	if (state.context == items::RecoveryContext::Arena)
	{
		auto& quest = quest::CQuestManager::instance();
		items::RecoveryArenaAdmission arena { quest.GetEventFlag("arena_potion_limit") <= 0, std::nullopt };
		if (quest.GetEventFlag("arena_potion_limit_count") < 10000)
			arena.remainingUses = static_cast<std::uint32_t>(std::max(0, ecs::PlayerRuntime::GetPotionLimit(character)));
		state.arena = arena;
	}
	return state;
}

// The units are spent. A character who died meanwhile gets no healing; the
// cooldown and the arena quota count the use either way.
void ApplyInstantRecovery(entt::entity character, const items::InstantRecoveryPlan& plan)
{
	if (plan.cooldown)
		g_registry.get_or_emplace<ecs::TypedItemCooldowns>(character).readyAtMs[plan.cooldown->profileId] =
			NowMs() + static_cast<std::int64_t>(std::min<std::uint64_t>(plan.cooldown->durationMs, INT64_MAX / 4));
	if (plan.arenaQuotaToConsume)
		ecs::PlayerRuntime::SetPotionLimit(character, ecs::PlayerRuntime::GetPotionLimit(character) - plan.arenaQuotaToConsume);
	if (CombatSystem::IsDead(character))
		return;
	const auto restore = [character](std::int64_t raw, std::uint64_t current, std::uint64_t max, std::uint8_t point, std::uint8_t effect) {
		const auto missing = static_cast<std::int64_t>(max - std::min(current, max));
		const auto points = std::min(raw / 1000, missing);
		if (points <= 0)
			return;
		ecs::PointSystem::Change(character, point, points);
		NetworkSyncSystem::BroadcastEffect(g_registry, character, effect);
	};
	restore(plan.hpRestoredRaw, ecs::PlayerRuntime::GetHP(character), ecs::PointSystem::GetMaxHP(character), POINT_HP, SE_HPUP_RED);
	restore(plan.spRestoredRaw, ecs::PlayerRuntime::GetSP(character), ecs::PointSystem::GetMaxSP(character), POINT_SP, SE_SPUP_BLUE);
}
} // namespace

items::InstantRecoveryAdmission TypedRecoveryAdmission(entt::entity e,std::optional<std::uint32_t> cd,std::uint32_t units){return RecoveryAdmission(e,cd,units);}
void ApplyTypedRecovery(entt::entity e,const items::InstantRecoveryPlan& p){ApplyInstantRecovery(e,p);}

namespace {
struct TypedTeleportChoice
{
    std::uint64_t item=0,token=0,revision=0,session=0;
    unsigned stage=0;
    std::chrono::steady_clock::time_point deadline;
};
struct TeleportDestination { const char* label; std::int32_t x,y; unsigned level; };
// Reviewed destinations/gates from the authored ring quest, not item numbers.
constexpr TeleportDestination Cities[]={{"Főváros",984217,265525,0},{"Dungeon",535279,1428276,0},{"Játszótér",384465,249238,0}};
constexpr TeleportDestination Hunting[]={{"Orkok",478084,722518,0},{"Jégföld",172510,338610,1},{"Nefrit",1087234,1654784,60},{"Sivatag",286791,542794,72},{"Rémálom",37078,173319,82},{"Sötét erdő",855317,367769,92},{"Tűzföld",743387,236596,112},{"SD5",362166,36355,122},{"Vérföld",2148100,2668000,140}};
void TypedQuestScript(entt::entity e,const std::string& text)
{
    auto* d=ecs::PlayerRuntime::GetDesc(e);if(!d)return;
    packet_script p{};p.header=HEADER_GC_SCRIPT;p.skin=quest::CQuestManager::QUEST_SKIN_NORMAL;p.src_size=static_cast<decltype(p.src_size)>(text.size());p.size=sizeof(p)+p.src_size;
    d->BufferedPacket(&p,sizeof(p));d->Packet(text.data(),static_cast<int>(text.size()));
}
bool UtilityAdmitted(entt::entity e)
{
    if(CombatSystem::IsDead(e) || !InventorySystem::CanHandleItems(e) || g_registry.all_of<ecs::TypedItemBusy>(e) || ecs::SocialSystem::HasExchange(e) || ecs::SessionSystem::IsSafeboxOpen(e) || ecs::SessionSystem::IsCubeOpen(e) || ecs::SocialSystem::GetMyShop(e)!=entt::null || ecs::SocialSystem::GetShopOwner(e)!=entt::null)return false;
    const auto* q=g_registry.try_get<ecs::QuestPCState>(e);const auto* f=g_registry.try_get<ecs::StatusFlags>(e);
    return q && q->pc && !q->pc->IsRunning() && (!f || (!f->isObserverMode && !f->isStunned));
}
bool TryUseTypedQuestScript(entt::entity character, const items::PersistentItem& source, const items::ItemCatalog& catalog)
{
	const auto* entry = catalog.Find(source.key);
	const auto* script = entry ? std::get_if<items::ItemQuestRow>(&entry->category) : nullptr;
	auto& quests = quest::CQuestManager::instance();
	if (!script || script->dispatch != items::QuestItemDispatch::FirstHandler || !script->groupCode.empty() ||
		!quests.HasTypedItemUse(entry->base.contentCode))
		return false;

	const auto* policy = catalog.FindUsePolicy(entry->base.usePolicy);
	const auto facts = RecoveryAdmission(character, {}, source.count);
	const bool admitted = UtilityAdmitted(character) && items::MissionItemUsable(source, source.owner) &&
		policy && (policy->allowedClasses & (1u << facts.classIndex)) && (policy->allowedSexes & (1u << facts.sexIndex));
	const auto plan = items::PlanQuestItemUse(catalog, {source.key, source.count, facts.level,
		static_cast<items::QuestItemContext>(facts.context), false, facts.inCombat, admitted, true});
	if (!plan || !quests.UseTypedQuestItem(character, entry->base.contentCode, {source.id.Value(), source.revision}))
		Tell(character, "Ez a küldetéstárgy most nem használható.");
	return true;
}

bool UseTypedUtility(entt::entity e,const items::PersistentItem& source,const items::ItemCatalog& catalog)
{
    const auto* entry=catalog.Find(source.key);const auto* quest=entry?std::get_if<items::ItemQuestRow>(&entry->category):nullptr;
    if(!quest)return false;
    const auto handler=items::BuiltinQuestHandler(quest->groupCode);
    if(handler==items::BuiltinQuestItem::PrivateShopBundle || handler==items::BuiltinQuestItem::PrivateShopSilk){if(!OpenTypedPrivateShopComposer(e,source.id))Tell(e,"A bolt most nem nyitható meg.");return true;}
    const auto facts=TypedRecoveryAdmission(e,{},source.count);const auto* policy=catalog.FindUsePolicy(entry->base.usePolicy);
    const bool admitted=UtilityAdmitted(e) && source.placement && source.placement->kind==items::ItemStorageKind::Inventory &&
        policy && (policy->allowedClasses&(1u<<ecs::PlayerRuntime::GetJob(e))) && (policy->allowedSexes&(1u<<ecs::PlayerRuntime::GetSex(e)));
    items::BuiltinUtilityFacts utility{{source.key,source.count,static_cast<std::uint16_t>(ecs::PointSystem::GetLevel(e)),static_cast<items::QuestItemContext>(facts.context),false,facts.inCombat,admitted,true},ecs::PlayerRuntime::GetJob(e),ecs::PlayerRuntime::GetSex(e),ecs::PlayerRuntime::GetMapIndex(e),!CombatSystem::IsDead(e)};
    if(handler==items::BuiltinQuestItem::SpouseWarp){
        utility.summonZoneAllowed=IS_SUMMONABLE_ZONE(utility.mapIndex);utility.warpAllowed=ecs::PlayerRuntime::CanWarp(e);
        utility.currentSp=ecs::PlayerRuntime::GetSP(e);utility.maximumSp=ecs::PointSystem::GetMaxSP(e);
        const auto couple=marriage::CManager::instance().Get(ecs::PlayerRuntime::GetPlayerID(e));
        if(const auto* m=marriage::MarriageSystem::State(couple)){
            utility.marriageAvailable=true;
            for(const auto partner:{m->character1,m->character2})if(ecs::IsCharacter(partner) && CArenaManager::instance().IsArenaMap(ecs::PlayerRuntime::GetMapIndex(partner)))utility.marriageArenaBlocked=true;
        }
    }
#ifdef ENABLE_NEWSTUFF
    utility.pvpPotionsBlocked=g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(ecs::PlayerRuntime::GetPlayerID(e));
#endif
    const auto& state=g_registry.get<ecs::TypedItems>(e).state;
    const auto plan=items::PrepareBuiltinUtility(catalog,source,state.fence.owner,utility);
    if(!plan){Tell(e,"Ez a tárgy itt nem használható.");return true;}
    const auto token=g_registry.get<ecs::TypedItemLogin>(e).token;
    if(handler==items::BuiltinQuestItem::PolymorphCancel){
        const auto selected=CombatSystem::GetSelectedTarget(e);const auto target=selected==entt::null ? e : selected;
        if(!g_registry.valid(target))return true;
        if(const auto* native=g_registry.try_get<ecs::TypedItems>(target);native && native->state.recovery.polymorph){
            if(!RequestTypedPolymorphClear(target))Tell(e,"Az átváltozás megszüntetése most nem indítható.");
        }else if(AffectSystem::IsPolymorphed(target)){
            AffectSystem::SetPolymorph(target,0,false);AffectSystem::RemoveAffect(target,AFFECT_POLYMORPH);
        }
        SendTypedInventory(e);return true;
    }
    if(handler==items::BuiltinQuestItem::SpouseWarp){
        const auto couple=marriage::CManager::instance().Get(ecs::PlayerRuntime::GetPlayerID(e));
        const auto* coupleState=marriage::MarriageSystem::State(couple);if(!coupleState)return true;
        ecs::PointSystem::Change(e,POINT_SP,-static_cast<std::int64_t>(plan->spCost),false);
        ecs::SessionSystem::WarpToPID(e,marriage::MarriageSystem::GetOther(couple,ecs::PlayerRuntime::GetPlayerID(e)));return true;
    }
    if(handler==items::BuiltinQuestItem::TeleportRing){
        g_registry.emplace_or_replace<TypedTeleportChoice>(e,source.id.Value(),token,state.fence.revision,state.fence.sessionGeneration,0u,std::chrono::steady_clock::now()+std::chrono::seconds(120));
        TypedQuestScript(e,"[TITLE]Teleport gyűrű[/TITLE][TEXT]Hová szeretnél utazni?[/TEXT][QUESTION 1;Városok|2;Vadászterületek|3;Mégse]");return true;
    }
    auto commit=items::MakeBuiltinUtilityCommit(catalog,NewOperationId(),state.fence,state.storages,state.items,source.id,utility);
    if(!commit){Tell(e,"A tárgy nem használható.");return true;}
    const auto pid=ecs::PlayerRuntime::GetPlayerID(e);g_registry.emplace_or_replace<ecs::TypedItemBusy>(e);
    if(!items::runtime::RequestCommit(state.fence.owner,state.fence.sessionGeneration,std::move(*commit),[pid,token,action=*plan,map=utility.mapIndex](items::runtime::CommitOutcome out){
        const auto ch=CHARACTER_MANAGER::instance().FindEntityByPID(pid);const auto* login=g_registry.valid(ch)?g_registry.try_get<ecs::TypedItemLogin>(ch):nullptr;
        if(!login || login->token!=token)return;
        g_registry.remove<ecs::TypedItemBusy>(ch);if(out.state)g_registry.emplace_or_replace<ecs::TypedItems>(ch,std::move(*out.state));
        if(out.write && !out.write->replay && !CombatSystem::IsDead(ch) && ecs::PlayerRuntime::GetMapIndex(ch)==map){
            using Handler=items::BuiltinQuestItem;
            switch(action.handler){
            case Handler::Courage:case Handler::CourageCombined:
                NetworkSyncSystem::BroadcastEffect(g_registry,ch,SE_MANTELLO);CombatSystem::AggregateMonster(ch);
                if(action.handler==Handler::CourageCombined)CombatSystem::AggregateMonsterPlus(ch);break;
            case Handler::Cleanse:AffectSystem::RemoveBadAffects(ch);break;
            case Handler::Invisibility:AffectSystem::AddAffect(ch,AFFECT_INVISIBILITY,POINT_NONE,0,AFF_INVISIBILITY,action.durationSeconds,0,true);break;
            case Handler::WhiteFlag:CombatSystem::ForgetMyAttacker(ch);break;
            case Handler::SelfPoison:AffectSystem::ApplyPoison(ch,entt::null);break;
            case Handler::RestoreHpFull:ecs::PointSystem::Change(ch,POINT_HP,int64_t(ecs::PointSystem::GetMaxHP(ch))-ecs::PlayerRuntime::GetHP(ch));break;
            case Handler::RestoreSpFull:ecs::PointSystem::Change(ch,POINT_SP,int64_t(ecs::PointSystem::GetMaxSP(ch))-ecs::PlayerRuntime::GetSP(ch));break;
            case Handler::RestoreStaminaFull:ecs::PointSystem::Change(ch,POINT_STAMINA,int64_t(ecs::PlayerRuntime::GetMaxStamina(ch))-ecs::PlayerRuntime::GetStamina(ch));break;
            case Handler::ChinaFirework:case Handler::SpinningTop:
                NetworkSyncSystem::BroadcastEffect(g_registry,ch,action.handler==Handler::ChinaFirework ? SE_CHINA_FIREWORK : SE_SPIN_TOP);
#ifdef ENABLE_FIREWORK_STUN
                AffectSystem::AddAffect(ch,AFFECT_CHINA_FIREWORK,POINT_STUN_PCT,30,AFF_CHINA_FIREWORK,300,0,true);
#endif
                break;
            default:if(action.firstFirework)CombatSystem::CreateFly(ch,FLY_FIREWORK1-1+number(action.firstFirework,action.lastFirework),ch);break;
            }
        }
        SendTypedInventory(ch);PumpTypedItemGrants(ch);
    })){g_registry.remove<ecs::TypedItemBusy>(e);Tell(e,"A tárgytároló nem érhető el.");}
    return true;
}
}
bool HandleTypedQuestAnswer(entt::entity e,std::uint8_t answer)
{
    if(!g_registry.valid(e))return false;auto* choice=g_registry.try_get<TypedTeleportChoice>(e);if(!choice)return false;
    // The ring menu can be replaced without an answer (a server-opened quest dialog closes it
    // client-side). Such an answer belongs to the running quest, and an expired menu owns
    // nothing: forget the menu and let the quest system handle the answer.
    const auto* questState=g_registry.try_get<ecs::QuestPCState>(e);
    if((questState && questState->pc && questState->pc->IsRunning()) || std::chrono::steady_clock::now()>choice->deadline){g_registry.remove<TypedTeleportChoice>(e);return false;}
    const auto* typed=g_registry.try_get<ecs::TypedItems>(e);const auto* login=g_registry.try_get<ecs::TypedItemLogin>(e);
    const auto catalog=items::runtime::CurrentCatalog();
    auto close=[&]{g_registry.remove<TypedTeleportChoice>(e);TypedQuestScript(e,"[DONE]");};
    // Bound to the login/session and the ring instance (checked below), not to the inventory
    // revision: unrelated item changes, e.g. an automatic elixir tick, must not close the menu.
    if(!typed || !login || !catalog || login->token!=choice->token || typed->state.fence.sessionGeneration!=choice->session || !UtilityAdmitted(e)){close();return true;}
    const auto source=std::ranges::find_if(typed->state.items,[&](const auto& i){return i.id.Value()==choice->item && !i.destroyed && i.placement && i.placement->kind==items::ItemStorageKind::Inventory;});
    const auto* entry=source!=typed->state.items.end()?catalog->Find(source->key):nullptr;const auto* quest=entry?std::get_if<items::ItemQuestRow>(&entry->category):nullptr;
    if(!quest || items::BuiltinQuestHandler(quest->groupCode)!=items::BuiltinQuestItem::TeleportRing){close();return true;}
    if(choice->stage==0){
        if(answer>1){close();return true;}choice->stage=answer+1;
        std::span<const TeleportDestination> choices=choice->stage==1?std::span<const TeleportDestination>(Cities):std::span<const TeleportDestination>(Hunting);
        std::string text="[TITLE]Teleport gyűrű[/TITLE][QUESTION ";unsigned n=0;for(const auto& d:choices){if(n)text+='|';text+=std::to_string(++n)+";"+d.label;}
        text+="|"+std::to_string(n+1)+";Mégse]";TypedQuestScript(e,text);return true;
    }
    std::span<const TeleportDestination> choices=choice->stage==1?std::span<const TeleportDestination>(Cities):std::span<const TeleportDestination>(Hunting);
    if(answer>=choices.size()){close();return true;}const auto destination=choices[answer];close();
    if(ecs::PointSystem::GetLevel(e)<static_cast<int>(destination.level)){Tell(e,"Az utazáshoz magasabb szint szükséges.");return true;}
    if(!ecs::MovementSystem::WarpSet(e,destination.x,destination.y))Tell(e,"A célterület most nem érhető el.");
    return true;
}

void RequestTypedItemUse(entt::entity character, std::uint64_t itemId)
{
	if (!g_registry.valid(character))
		return;
	auto* typed = g_registry.try_get<ecs::TypedItems>(character);
	const auto* login = g_registry.try_get<ecs::TypedItemLogin>(character);
	const auto catalog = items::runtime::CurrentCatalog();
	if (!typed || !login || !catalog)
	{
		Tell(character, "Az új tárgyrendszer ennél a karakternél nincs bekapcsolva.");
		return;
	}
	if (g_registry.all_of<ecs::TypedItemBusy>(character))
	{
		Tell(character, "Az előző tárgyművelet még folyamatban van.");
		return;
	}
	const auto id = items::ItemInstanceId::Create(itemId);
	const auto& state = typed->state;
	const auto current = id ? std::ranges::find_if(state.items, [&](const items::PersistentItem& i) { return i.id == *id && !i.destroyed; }) : state.items.end();
	if (current == state.items.end() || !current->placement)
	{
		Tell(character, "Ez a tárgy nincs nálad.");
		SendTypedInventory(character);
		return;
	}
	if(TryUseTypedDungeonKey(character,*current,*catalog))return;
	if(TryUseTypedWorldUtility(character,*current,*catalog))return;
	if(TryUseTypedQuestScript(character,*current,*catalog))return;
	if(UseTypedUtility(character,*current,*catalog))return;
	// Anything but a recovery item is refused by the planner (wrong category or delivery).
	const auto* entry = catalog->Find(current->key);
	const auto* consumable = entry ? std::get_if<items::ItemConsumableRow>(&entry->category) : nullptr;
	const auto* recovery = consumable && consumable->recoveryProfile ? catalog->FindRecovery(consumable->recoveryProfile) : nullptr;
	if(recovery && (recovery->Data().Delivery()==items::RecoveryDelivery::Reserve || recovery->Data().Delivery()==items::RecoveryDelivery::Periodic)){PreviewTypedRecovery(character,itemId);return;}
	const auto plan = items::PlanInstantRecovery(*catalog, current->key,
		RecoveryAdmission(character, recovery ? recovery->Data().cooldownProfile : std::nullopt, current->count));
	if (!plan)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "A tárgy nem használható: %s.", UseErrorText(plan.error()));
		return;
	}
	auto commit = items::MakeConsumeCommit(*catalog, NewOperationId(), state.fence, state.storages, state.items, *id, plan->unitsToConsume);
	if (!commit)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "A tárgyművelet nem lehetséges: %s.", ErrorText(commit.error()));
		SendTypedInventory(character);
		return;
	}
	if(plan->cooldown)
    {
        const auto elapsed=TypedRecoveryElapsed(character);auto advanced=items::AdvanceRecoverySchedule(*catalog,state.recovery,elapsed);
        if(!advanced || state.recovery.revision==UINT64_MAX || !items::SetRecoveryCooldown(*catalog,*advanced,plan->cooldown->profileId,plan->cooldown->durationMs))return;
        advanced->revision=state.recovery.revision+1;commit->recovery=items::ItemRecoveryChange{state.fence.owner,state.recovery,*advanced,elapsed};
    }
	const uint32_t pid = ecs::PlayerRuntime::GetPlayerID(character);
	const uint64_t token = login->token;
	g_registry.emplace_or_replace<ecs::TypedItemBusy>(character);
	const bool queued = items::runtime::RequestCommit(typed->state.fence.owner, typed->state.fence.sessionGeneration, std::move(*commit),
		[pid, token, effect = *plan](items::runtime::CommitOutcome outcome) mutable {
            effect.cooldown.reset();
			const entt::entity entity = CHARACTER_MANAGER::instance().FindEntityByPID(pid);
			const auto* current = entity != entt::null && g_registry.valid(entity) ? g_registry.try_get<ecs::TypedItemLogin>(entity) : nullptr;
			if (!current || current->token != token)
				return; // that login is gone; the next login reloads anyway
			g_registry.remove<ecs::TypedItemBusy>(entity);
			if (outcome.state)
				g_registry.emplace_or_replace<ecs::TypedItems>(entity, std::move(*outcome.state));
			else
				LOG_ERROR("TypedItems: pid {} reload after a use failed (error {})", pid, static_cast<int>(outcome.state.error()));
			if (outcome.write)
				ApplyInstantRecovery(entity, effect);
			else
			{
				LOG_INFO("TypedItems: pid {} use refused by the store (error {})", pid, static_cast<int>(outcome.write.error()));
				ecs::ChatSystem::Send(entity, CHAT_TYPE_INFO, "A tárgyhasználat nem sikerült: %s.", ErrorText(outcome.write.error()));
			}
			SendTypedInventory(entity);
			PumpTypedItemGrants(entity); // drops that waited for this answer
		});
	if (!queued)
	{
		g_registry.remove<ecs::TypedItemBusy>(character);
		Tell(character, "A tárgytároló nem fut; a használat nem lehetséges.");
	}
}

static_assert(sizeof(TPacketCGTypedItemUse) == 9, "CG_TYPED_ITEM_USE wire size");

bool HandleTypedItemUsePacket(entt::entity character, const char* data)
{
	RequestTypedItemUse(character, reinterpret_cast<const TPacketCGTypedItemUse*>(data)->qwItemId);
	return true;
}

// /typed_use <inventory cell>: use the item there (C5 test command).
ACMD(do_typed_use)
{
	char cell[32];
	one_argument(argument, cell, sizeof(cell));
	const auto* typed = g_registry.valid(character) ? g_registry.try_get<ecs::TypedItems>(character) : nullptr;
	if (!*cell || !typed)
	{
		Tell(character, "Használat: /typed_use <inventory cella>  (új tárgyrendszer)");
		return;
	}
	if (const auto* item = ItemAt(*typed, items::ItemStorageKind::Inventory, static_cast<std::uint32_t>(std::strtoul(cell, nullptr, 10))))
		RequestTypedItemUse(character, item->id.Value());
	else
		Tell(character, "Ezen a cellán nincs új rendszerű tárgy.");
}

// ---------------------------------------------------------------------------
// Refine (F4 C6): one inventory item, one catalog recipe, at a normal smith.
// The game thread admits (the legacy refine window's rules and the smith's
// range), rolls, and builds the complete write set with MakeOwnerRefinement;
// the store commits the item, the materials and the Gold wallet together.
// Guild and Devil Tower smiths and the scrolls have their own fee and chance
// rules, which no native recipe carries yet: later slices.

namespace
{
// How far an item can be handed to an NPC (CanReceiveItemEcs).
constexpr std::int64_t SmithRange = 2000;

const char* RefineErrorText(items::RefinementUseError e)
{
	using E = items::RefinementUseError;
	switch (e)
	{
	case E::Forbidden: return "itt nem fejleszthetsz";
	case E::InvalidContext: return "érvénytelen kérés";
	case E::MissingRecipe: return "ehhez a tárgyhoz nincs ilyen fejlesztés";
	case E::InvalidSource: return "ez a tárgy így nem fejleszthető";
	case E::InvalidInventory: return "az inventory állapota érvénytelen";
	case E::InsufficientGold: return "nincs elég aranyad";
	case E::InsufficientMaterials: return "nincs elég alapanyagod";
	case E::InvalidDecision: return "érvénytelen dobás";
	}
	return "ismeretlen hiba";
}

bool NormalSmithInRange(entt::entity character, entt::entity npc)
{
	if (npc == entt::null || !g_registry.valid(npc) || !ecs::PlayerRuntime::IsValid(npc) || ecs::PlayerRuntime::IsPC(npc))
		return false;
	const auto race = ecs::PlayerRuntime::GetRaceNum(npc);
	if ((race != BLACKSMITH_MOB && race != BLACKSMITH2_MOB) || ecs::PlayerRuntime::GetMapIndex(npc) != ecs::PlayerRuntime::GetMapIndex(character))
		return false;
	const std::int64_t dx = std::abs(std::int64_t(ecs::PlayerRuntime::GetX(npc)) - ecs::PlayerRuntime::GetX(character));
	const std::int64_t dy = std::abs(std::int64_t(ecs::PlayerRuntime::GetY(npc)) - ecs::PlayerRuntime::GetY(character));
	return dx <= SmithRange && dy <= SmithRange && DISTANCE_APPROX(static_cast<int>(dx), static_cast<int>(dy)) <= SmithRange;
}

// The legacy refine window's rules (CG_REFINE and DoRefine); nullptr when allowed.
const char* RefineRefusal(entt::entity character)
{
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
	if (ecs::PlayerRuntime::GetGMLevel(character) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(character) < GM_IMPLEMENTOR)
		return "játékmesterként nem fejleszthetsz";
#endif
	if (ecs::SocialSystem::HasExchange(character) || ecs::SessionSystem::IsSafeboxOpen(character) ||
		ecs::SocialSystem::GetShopOwner(character) != entt::null || ecs::SocialSystem::GetMyShop(character) != entt::null ||
		ecs::SessionSystem::IsCubeOpen(character) || !InventorySystem::CanHandleItems(character))
		return "előbb zárd be a nyitott ablakokat (csere, raktár, bolt, kocka, fejlesztés)";
	const auto updated = quest::CQuestManager::instance().GetEventFlag("update_refine_time");
	if (updated != 0 && get_global_time() < updated + 60 * 5)
		return "a fejlesztés most átmenetileg szünetel";
	return nullptr;
}

void TellRefineOutcome(entt::entity character, const items::RefinementPlan& plan)
{
	ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, plan.success ? "RefineSuceeded" : "RefineFailed");
	if (plan.success)
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Sikeres fejlesztés: +%u.", static_cast<unsigned>(plan.resultLevel.value_or(0)));
	else if (!plan.result)
		Tell(character, "A fejlesztés nem sikerült, a tárgy megsemmisült.");
	else if (plan.resultLevel == plan.sourceLevel)
		Tell(character, "A fejlesztés nem sikerült, a tárgy megmaradt.");
	else
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "A fejlesztés nem sikerült, a tárgy +%u lett.", static_cast<unsigned>(plan.resultLevel.value_or(0)));
}

void RefineAt(entt::entity character, std::uint64_t itemId, std::uint32_t recipeId, entt::entity smith)
{
	if (!g_registry.valid(character))
		return;
	auto* typed = g_registry.try_get<ecs::TypedItems>(character);
	const auto* login = g_registry.try_get<ecs::TypedItemLogin>(character);
	const auto catalog = items::runtime::CurrentCatalog();
	if (!typed || !login || !catalog)
	{
		Tell(character, "Az új tárgyrendszer ennél a karakternél nincs bekapcsolva.");
		return;
	}
	if (g_registry.all_of<ecs::TypedItemBusy>(character))
	{
		Tell(character, "Az előző tárgyművelet még folyamatban van.");
		return;
	}
	if (const char* why = RefineRefusal(character))
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "A fejlesztés nem lehetséges: %s.", why);
		return;
	}
	if (!NormalSmithInRange(character, smith))
	{
		Tell(character, "A fejlesztéshez egy kovács közelében kell lenned.");
		return;
	}
	const auto id = items::ItemInstanceId::Create(itemId);
	const auto& state = typed->state;
	const auto current = id ? std::ranges::find_if(state.items, [&](const items::PersistentItem& i) { return i.id == *id && !i.destroyed; }) : state.items.end();
	if (current == state.items.end() || !current->placement)
	{
		Tell(character, "Ez a tárgy nincs nálad.");
		SendTypedInventory(character);
		return;
	}
	if (recipeId == 0)
	{
		const auto offers = catalog->RefinementRecipesFrom(current->key, current->refinementLevel);
		if (offers.empty())
		{
			Tell(character, "Ez a tárgy nem fejleszthető tovább.");
			return;
		}
		if (offers.size() > 1)
		{
			std::string ids;
			for (const auto* offer : offers)
				ids += (ids.empty() ? "" : ", ") + std::to_string(offer->id);
			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Több fejlesztés is van ehhez a tárgyhoz; add meg a receptet: %s.", ids.c_str());
			return;
		}
		recipeId = offers.front()->id;
	}
	// 0% and 100% are decided without a roll (PrepareRefinement).
	const auto* recipe = catalog->FindRefinementRecipe(recipeId);
	const std::uint16_t roll = recipe && recipe->successChanceBp != 0 && recipe->successChanceBp != 10000
		? static_cast<std::uint16_t>(std::uniform_int_distribution<int>(1, 10000)(Engine())) : 0;
	auto made = items::MakeOwnerRefinement(*catalog, state.fence, state.storages, state.items, state.wallets,
		{ NewOperationId(), *id, recipeId, roll, true });
	if (!made)
	{
		const char* why = std::visit([](auto e) {
			if constexpr (std::is_same_v<decltype(e), items::RefinementUseError>) return RefineErrorText(e);
			else return ErrorText(e);
		}, made.error());
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "A fejlesztés nem lehetséges: %s.", why);
		SendTypedInventory(character);
		return;
	}
	const uint32_t pid = ecs::PlayerRuntime::GetPlayerID(character);
	const uint64_t token = login->token;
	g_registry.emplace_or_replace<ecs::TypedItemBusy>(character);
	const bool queued = items::runtime::RequestCommit(typed->state.fence.owner, typed->state.fence.sessionGeneration, std::move(made->commit),
		[pid, token, plan = std::move(made->plan)](items::runtime::CommitOutcome outcome) {
			const entt::entity entity = CHARACTER_MANAGER::instance().FindEntityByPID(pid);
			const auto* current = entity != entt::null && g_registry.valid(entity) ? g_registry.try_get<ecs::TypedItemLogin>(entity) : nullptr;
			if (!current || current->token != token)
				return; // that login is gone; the next login reloads anyway
			g_registry.remove<ecs::TypedItemBusy>(entity);
			if (outcome.state)
				g_registry.emplace_or_replace<ecs::TypedItems>(entity, std::move(*outcome.state));
			else
				LOG_ERROR("TypedItems: pid {} reload after a refine failed (error {})", pid, static_cast<int>(outcome.state.error()));
			if (outcome.write)
			{
				LOG_INFO("TypedItems: pid {} refine recipe {} roll {} {} (cost {})", pid, plan.recipeId, plan.recordedRoll,
					plan.success ? "succeeded" : "failed", plan.goldCost);
				TellRefineOutcome(entity, plan);
			}
			else
			{
				LOG_INFO("TypedItems: pid {} refine refused by the store (error {})", pid, static_cast<int>(outcome.write.error()));
				ecs::ChatSystem::Send(entity, CHAT_TYPE_INFO, "A fejlesztés nem sikerült: %s.", ErrorText(outcome.write.error()));
			}
			SendTypedInventory(entity);
			PumpTypedItemGrants(entity); // drops that waited for this answer
		});
	if (!queued)
	{
		g_registry.remove<ecs::TypedItemBusy>(character);
		Tell(character, "A tárgytároló nem fut; a fejlesztés nem lehetséges.");
	}
}
} // namespace

void RequestTypedItemRefine(entt::entity character, std::uint64_t itemId, std::uint32_t recipeId, std::uint32_t npcVid)
{
	RefineAt(character, itemId, recipeId, CHARACTER_MANAGER::instance().FindEntity(npcVid));
}

static_assert(sizeof(TPacketCGTypedItemRefine) == 17, "CG_TYPED_ITEM_REFINE wire size");

bool HandleTypedItemRefinePacket(entt::entity character, const char* data)
{
	const auto* packet = reinterpret_cast<const TPacketCGTypedItemRefine*>(data);
	RequestTypedItemRefine(character, packet->qwItemId, packet->dwRecipeId, packet->dwNpcVid);
	return true;
}

// ---------------------------------------------------------------------------
// Grant (F4 C7a): new units of one item state into the character's inventory.
// MakeGrantCommit tops up matching stacks first, then takes free cells; the
// new items' IDs come from the store's pool and are spent before the commit is
// queued, so no other grant can take them meanwhile.

std::expected<void, TypedGrantRefusal> RequestTypedItemGrant(entt::entity character, const items::ItemGrant& grant,
	std::function<void(bool written)> done)
{
	if (!g_registry.valid(character))
		return std::unexpected(TypedGrantRefusal::NotEnabled);
	auto* typed = g_registry.try_get<ecs::TypedItems>(character);
	const auto* login = g_registry.try_get<ecs::TypedItemLogin>(character);
	const auto catalog = items::runtime::CurrentCatalog();
	if (!typed || !login || !catalog)
		return std::unexpected(TypedGrantRefusal::NotEnabled);
	if (g_registry.all_of<ecs::TypedItemBusy>(character))
		return std::unexpected(TypedGrantRefusal::Busy);
	const auto* entry = catalog->Find(grant.key);
	if (!entry || !grant.count)
		return std::unexpected(TypedGrantRefusal::Invalid);
	if(catalog->CurrencyMode(grant.key)==items::CurrencyTokenMode::GroundGold){if(grant.refinementLevel || grant.count!=1 || !grant.currencyAmount)return std::unexpected(TypedGrantRefusal::Invalid);return RequestTypedGoldCredit(character,*grant.currencyAmount,{},std::move(done));}
	const std::uint32_t stack = entry->base.maxStack ? entry->base.maxStack : 1;
	const std::size_t most = (std::size_t { grant.count } + stack - 1) / stack;
	if (most > items::MaxItemCommitItems)
		return std::unexpected(TypedGrantRefusal::Invalid);
	const auto ids = items::runtime::PeekInstanceIds(most);
	const auto& state = typed->state;
	auto timedGrant=grant;if(!timedGrant.createdAtUtcMs)timedGrant.createdAtUtcMs=TypedItemEffectNow();
	auto commit = items::MakeGrantCommit(*catalog, NewOperationId(), state.fence, state.storages, state.items, timedGrant, ids);
	if (!commit)
	{
		switch (commit.error())
		{
		case items::ItemPersistenceError::InvalidPlacement: return std::unexpected(TypedGrantRefusal::NoRoom);
		case items::ItemPersistenceError::LimitExceeded:
			if (ids.size() < most) return std::unexpected(TypedGrantRefusal::NoIds);
			return std::unexpected(TypedGrantRefusal::Invalid);
		default: return std::unexpected(TypedGrantRefusal::Invalid);
		}
	}
	items::runtime::SpendInstanceIds(static_cast<std::size_t>(std::ranges::count_if(commit->items, [](const items::ItemChange& c) { return !c.before; })));
	const uint32_t pid = ecs::PlayerRuntime::GetPlayerID(character);
	const uint64_t token = login->token;
	const auto dungeonRun=DungeonUi::Events::Context(character);
	g_registry.emplace_or_replace<ecs::TypedItemBusy>(character);
	const bool queued = items::runtime::RequestCommit(typed->state.fence.owner, typed->state.fence.sessionGeneration, std::move(*commit),
		[pid, token, grant, dungeonRun, code = entry->base.contentCode, done = std::move(done)](items::runtime::CommitOutcome outcome) {
			if(outcome.write)DungeonUi::Events::Loot(dungeonRun,pid,items::PackItemKey(grant.key),grant.refinementLevel,grant.count);
			const entt::entity entity = CHARACTER_MANAGER::instance().FindEntityByPID(pid);
			const auto* current = entity != entt::null && g_registry.valid(entity) ? g_registry.try_get<ecs::TypedItemLogin>(entity) : nullptr;
			if (!current || current->token != token)
			{
				if (done) done(outcome.write.has_value()); // persistence outlives the login
				return;
			}
			g_registry.remove<ecs::TypedItemBusy>(entity);
			if (outcome.state)
				g_registry.emplace_or_replace<ecs::TypedItems>(entity, std::move(*outcome.state));
			else
				LOG_ERROR("TypedItems: pid {} reload after a grant failed (error {})", pid, static_cast<int>(outcome.state.error()));
			if (outcome.write)
			{
				LOG_INFO("TypedItems: pid {} received {} x{} (+{})", pid, code, grant.count, grant.refinementLevel);
				ecs::ChatSystem::Send(entity, CHAT_TYPE_INFO, "Új tárgy: %s ×%u.", code.c_str(), static_cast<unsigned>(grant.count));
			}
			else
			{
				LOG_INFO("TypedItems: pid {} grant refused by the store (error {})", pid, static_cast<int>(outcome.write.error()));
				ecs::ChatSystem::Send(entity, CHAT_TYPE_INFO, "A tárgy átadása nem sikerült: %s.", ErrorText(outcome.write.error()));
			}
			SendTypedInventory(entity);
			if (done) done(outcome.write.has_value());
			PumpTypedItemGrants(entity); // drops that waited for this answer
		});
	if (!queued)
	{
		g_registry.remove<ecs::TypedItemBusy>(character);
		return std::unexpected(TypedGrantRefusal::NotEnabled);
	}
	return {};
}

// /typed_give <category> <local id> [count] [level]: GM test grant (C7a); drops queue through TypedDrops (C7b-3).
ACMD(do_typed_give)
{
	char category[32], local[32], count[32], level[32];
	const char* rest = two_arguments(argument, category, sizeof(category), local, sizeof(local));
	two_arguments(rest, count, sizeof(count), level, sizeof(level));
	std::optional<items::ItemKey> key;
	if (*category && *local)
		if (const auto made = items::ItemKey::Create(static_cast<std::uint16_t>(std::strtoul(category, nullptr, 10)),
				static_cast<std::uint32_t>(std::strtoul(local, nullptr, 10))))
			key = *made;
	if (!key)
	{
		Tell(character, "Használat: /typed_give <kategória> <helyi azonosító> [darab] [szint]  (új tárgyrendszer)");
		return;
	}
	const items::ItemGrant grant { *key, static_cast<std::uint16_t>(*level ? std::strtoul(level, nullptr, 10) : 0),
		static_cast<std::uint32_t>(*count ? std::strtoul(count, nullptr, 10) : 1) };
	const auto queued = RequestTypedItemGrant(character, grant);
	if (queued)
		return;
	switch (queued.error())
	{
	case TypedGrantRefusal::NotEnabled: Tell(character, "Az új tárgyrendszer ennél a karakternél nincs bekapcsolva."); break;
	case TypedGrantRefusal::Busy: Tell(character, "Az előző tárgyművelet még folyamatban van."); break;
	case TypedGrantRefusal::Invalid: Tell(character, "Nincs ilyen tárgy, vagy érvénytelen a darabszám vagy a szint."); break;
	case TypedGrantRefusal::NoRoom: Tell(character, "Nincs elég hely az inventoryban."); break;
	case TypedGrantRefusal::NoIds: Tell(character, "Nincs lefoglalt tárgyazonosító; nézd meg a szervernaplót."); break;
	}
}

// /typed_refine <inventory cell> [recipe]: refine at the nearest normal smith (C6 test command).
ACMD(do_typed_refine)
{
	char cell[32], recipe[32];
	two_arguments(argument, cell, sizeof(cell), recipe, sizeof(recipe));
	const auto* typed = g_registry.valid(character) ? g_registry.try_get<ecs::TypedItems>(character) : nullptr;
	if (!*cell || !typed)
	{
		Tell(character, "Használat: /typed_refine <inventory cella> [recept]  (új tárgyrendszer, egy kovács mellett)");
		return;
	}
	const auto* item = ItemAt(*typed, items::ItemStorageKind::Inventory, static_cast<std::uint32_t>(std::strtoul(cell, nullptr, 10)));
	if (!item)
	{
		Tell(character, "Ezen a cellán nincs új rendszerű tárgy.");
		return;
	}
	entt::entity smith = entt::null;
	for (const entt::entity nearby : ecs::VisibilityService::GetEntitiesInRange(g_registry, character, static_cast<std::int32_t>(SmithRange)))
		if (NormalSmithInRange(character, nearby))
		{
			smith = nearby;
			break;
		}
	RefineAt(character, item->id.Value(), static_cast<std::uint32_t>(std::strtoul(recipe, nullptr, 10)), smith);
}
