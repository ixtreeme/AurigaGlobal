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
#include "../entity/char_manager.h"

#include <Core/Logging.hpp>
#include <item/ItemCatalog.hpp>

#include <array>
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
