#include "stdafx.h"
#include "TypedItemOps.hpp"
#include "ItemCatalogRuntime.hpp"
#include "ItemStore.hpp"
#include "TypedInventorySync.hpp"
#include "../command/cmd.h"
#include "../core/packet.h"
#include "../ecs/Registry.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/NetworkSyncSystem.hpp"
#include "../entity/char_manager.h"
#include "../combat/pvp.h"
#include "../quest/questmanager.h"
#include "../world/arena.h"
#include "../world/war_map.h"

#include <Core/Logging.hpp>
#include <item/Consumables.hpp>
#include <item/ItemCatalog.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <random>

namespace
{
namespace items = auriga::item;

const char* ErrorText(items::ItemPersistenceError e)
{
	static const char* texts[] = { "érvénytelen tulajdonos", "érvénytelen tároló", "érvénytelen tárgyállapot", "nincs ilyen tárgy",
		"ez a kategória még nem támogatott", "érvénytelen szint", "érvénytelen bónusz", "érvénytelen foglalat",
		"nincs hely, vagy ütközik egy másik tárggyal", "érvénytelen művelet", "elavult munkamenet (lépj be újra)",
		"érvénytelen változás", "érvénytelen pénztárca", "túl sok adat", "katalóguseltérés", "hash-hiba", "adatbázishiba",
		"ütközés egy párhuzamos mentéssel", "a mentés eredménye ismeretlen", "nyugtaeltérés", "sérült tárolt adat", "nincs kiépítve", "a karakter nem felel meg a feltételeknek (szint, kaszt vagy nem)" };
	const auto code = static_cast<std::size_t>(e);
	return code < std::size(texts) ? texts[code] : "ismeretlen hiba";
}

std::array<std::uint8_t, 16> NewOperationId()
{
	static std::mt19937_64 engine { std::random_device {}() ^ (static_cast<std::uint64_t>(std::random_device {}()) << 32) };
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

void RequestTypedItemMove(entt::entity character, std::uint64_t itemId, std::uint8_t storageKind, std::uint64_t storageId, std::uint32_t cell)
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
	const items::ItemPlacement target { static_cast<items::ItemStorageKind>(storageKind), storageId, cell };
	const auto& state = typed->state;
	const auto current = id ? std::ranges::find_if(state.items, [&](const items::PersistentItem& i) { return i.id == *id && !i.destroyed; }) : state.items.end();
	std::expected<items::ItemCommit, items::ItemPersistenceError> commit = std::unexpected(items::ItemPersistenceError::MissingItem);
	if (current != state.items.end() && current->placement)
	{
		// One request, three operations: equip into the item's own slot, take
		// off into the inventory, or move inside the inventory (C3/C4).
		if (target.kind == items::ItemStorageKind::Equipment)
		{
			const items::EquipContext who { static_cast<std::uint16_t>(std::max(0, ecs::PointSystem::GetLevel(character))),
				ecs::PlayerRuntime::GetJob(character), ecs::PlayerRuntime::GetSex(character) };
			commit = items::MakeEquipCommit(*catalog, NewOperationId(), state.fence, state.storages, state.items, *id, who);
		}
		else if (current->placement->kind == items::ItemStorageKind::Equipment)
			commit = items::MakeUnequipCommit(*catalog, NewOperationId(), state.fence, state.storages, state.items, *id, target);
		else
			commit = items::MakeMoveCommit(*catalog, NewOperationId(), state.fence, state.storages, state.items, *id, target);
	}
	if (!commit)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "A tárgyművelet nem lehetséges: %s.", ErrorText(commit.error()));
		SendTypedInventory(character); // the client reverts to the authoritative layout
		return;
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
	// Anything but a recovery item is refused by the planner (wrong category or delivery).
	const auto* entry = catalog->Find(current->key);
	const auto* consumable = entry ? std::get_if<items::ItemConsumableRow>(&entry->category) : nullptr;
	const auto* recovery = consumable && consumable->recoveryProfile ? catalog->FindRecovery(consumable->recoveryProfile) : nullptr;
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
	const uint32_t pid = ecs::PlayerRuntime::GetPlayerID(character);
	const uint64_t token = login->token;
	g_registry.emplace_or_replace<ecs::TypedItemBusy>(character);
	const bool queued = items::runtime::RequestCommit(typed->state.fence.owner, typed->state.fence.sessionGeneration, std::move(*commit),
		[pid, token, effect = *plan](items::runtime::CommitOutcome outcome) {
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
