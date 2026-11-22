#include "stdafx.h"
#include "TypedInventorySync.hpp"
#include "TypedStoneExchange.hpp"
#include "ItemCatalogRuntime.hpp"
#include "ItemStore.hpp"
#include "TypedDrops.hpp"
#include "TypedEquipment.hpp"
#include "TypedCostumes.hpp"
#include "TypedCompanions.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../network/desc.h"
#include "../network/p2p.h"
#include "../entity/char_manager.h"
#include "../core/packet.h"
#include "../quest/questmanager.h"

#include <item/TypedInventoryWire.hpp>
#include <item/TypedItemDetails.hpp>
#include <algorithm>
#include <Core/Logging.hpp>

static_assert(HEADER_GC_TYPED_INVENTORY == auriga::item::TypedInventoryHeader, "GC_TYPED_INVENTORY header differs from the wire codec");
static_assert(sizeof(TPacketGGTypedItemRefresh) == 21, "GG typed owner invalidation wire size");

void NotifyRemoteTypedItemOwner(const auriga::item::persistence::LoadedItemOwner& state)
{
	const TPacketGGTypedItemRefresh packet { HEADER_GG_TYPED_ITEM_REFRESH,
		static_cast<std::uint32_t>(state.fence.owner.id), state.fence.sessionGeneration, state.fence.revision };
	P2P_MANAGER::instance().Send(&packet, sizeof(packet));
}

namespace
{
namespace items = auriga::item;
entt::entity CurrentLogin(std::uint32_t pid, std::uint64_t token)
{
	const auto entity = CHARACTER_MANAGER::instance().FindEntityByPID(pid);
	const auto* login = entity != entt::null && g_registry.valid(entity) ? g_registry.try_get<ecs::TypedItemLogin>(entity) : nullptr;
	return login && login->token == token ? entity : entt::null;
}
bool QueueOwnerRefresh(std::uint32_t pid, std::uint64_t token, std::chrono::milliseconds delay = {})
{
	return items::runtime::ItemStore::Instance().Submit([pid, token](CAsyncSQL& sql, const items::ItemCatalog& catalog) -> items::runtime::ItemStore::Completion {
		auto state = items::persistence::LoadCurrentItemOwner(sql, catalog, {items::ItemOwnerKind::Character, pid});
		return [pid, token, state = std::move(state)]() mutable {
			const auto entity = CurrentLogin(pid, token);
			if (entity == entt::null) return;
			if (!state && state.error() == items::ItemPersistenceError::SqlFailure)
			{
				if (!QueueOwnerRefresh(pid, token, std::chrono::seconds(1))) g_registry.remove<ecs::TypedItemBusy>(entity);
				return;
			}
			auto* current = g_registry.try_get<ecs::TypedItems>(entity);
			if (state && current && current->state.fence.sessionGeneration == state->fence.sessionGeneration && current->state.fence.revision <= state->fence.revision)
				g_registry.emplace_or_replace<ecs::TypedItems>(entity, std::move(*state));
			g_registry.remove<ecs::TypedItemBusy>(entity);
			if (!state) { LOG_ERROR("TypedItems: remote refresh of pid {} failed (error {})", pid, static_cast<int>(state.error())); return; }
			SendTypedInventory(entity);
			PumpTypedItemGrants(entity); // pumps a newer invalidation before any queued grant
		};
	}, delay);
}
}

bool PumpRemoteTypedItemRefresh(entt::entity character)
{
	if (!g_registry.valid(character)) return false;
	const auto* wanted = g_registry.try_get<ecs::TypedItemRefresh>(character);
	const auto* state = g_registry.try_get<ecs::TypedItems>(character);
	const auto* login = g_registry.try_get<ecs::TypedItemLogin>(character);
	if (!wanted || !state || !login) return false;
	if (wanted->session != state->state.fence.sessionGeneration || wanted->revision <= state->state.fence.revision)
	{
		g_registry.remove<ecs::TypedItemRefresh>(character); return false;
	}
	if (g_registry.all_of<ecs::TypedItemBusy>(character)) return true;
	const auto pid = ecs::PlayerRuntime::GetPlayerID(character);
	const auto token = login->token;
	g_registry.emplace_or_replace<ecs::TypedItemBusy>(character);
	if (!QueueOwnerRefresh(pid, token)) g_registry.remove<ecs::TypedItemBusy>(character);
	return true;
}

void RefreshRemoteTypedItemOwner(std::uint32_t pid, std::uint64_t session, std::uint64_t revision)
{
	const auto entity = CHARACTER_MANAGER::instance().FindEntityByPID(pid);
	const auto* state = entity != entt::null && g_registry.valid(entity) ? g_registry.try_get<ecs::TypedItems>(entity) : nullptr;
	if (!state || state->state.fence.sessionGeneration != session || state->state.fence.revision >= revision) return;
	auto* wanted = g_registry.try_get<ecs::TypedItemRefresh>(entity);
	if (!wanted || wanted->session != session || wanted->revision < revision)
		g_registry.emplace_or_replace<ecs::TypedItemRefresh>(entity, session, revision);
	PumpRemoteTypedItemRefresh(entity);
}

void TickTypedItemRefreshes()
{
	std::vector<entt::entity> pending;
	for (auto entity : g_registry.view<ecs::TypedItemRefresh>()) pending.push_back(entity);
	for (auto entity : pending) PumpRemoteTypedItemRefresh(entity);
}

void SendTypedInventory(entt::entity character)
{
	UpdateTypedCostumes(character);
	const auto companion = RefreshTypedCompanion(character);
	RefreshTypedEquipment(character);
	if (!g_registry.valid(character))
		return;
	const auto* typed = g_registry.try_get<ecs::TypedItems>(character);
	auto* desc = ecs::PlayerRuntime::GetDesc(character);
	if (!desc || !desc->IsPhase(PHASE_GAME))
		return;
	if (!typed)
	{
		const auto* login = g_registry.try_get<ecs::TypedItemLogin>(character);
		if (login && login->mode == ecs::TypedItemLogin::Mode::Unavailable)
			ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "typed_item_backend unavailable");
		return;
	}
	const auto catalog = auriga::item::runtime::CurrentCatalog();
	if (!catalog)
		return;
	const auto& state = typed->state;
	// Escrow is server-owned ground state, never a client inventory page. The special
	// inventory (C47) is the character's own: it travels in the same snapshot (wire v18).
	using auriga::item::ItemStorageKind;
	const auto clientVisible = [](ItemStorageKind kind) {
		return kind == ItemStorageKind::Inventory || kind == ItemStorageKind::Equipment || kind == ItemStorageKind::PetCarrier || kind == ItemStorageKind::SpecialInventory;
	};
	std::vector<auriga::item::ItemStorageLayout> storages;
	std::vector<auriga::item::PersistentItem> visible;
	for (const auto& layout : state.storages)
		if (clientVisible(layout.storage.kind))
			storages.push_back(layout);
	for (const auto& item : state.items)
		if (!item.destroyed && item.placement && clientVisible(item.placement->kind))
			visible.push_back(item);
	std::vector<auriga::item::ItemKey> scriptedUse;
	const auto& quests = quest::CQuestManager::instance();
	for (const auto& item : visible)
		if (const auto* definition = catalog->Find(item.key); definition && quests.HasTypedItemUse(definition->base.contentCode))
			scriptedUse.push_back(item.key);
	const bool ownedDropEnabled = auriga::item::runtime::ItemStore::Instance().GroundRun().has_value() &&
		std::ranges::any_of(state.storages, [](const auto& storage) {
			return storage.storage.kind == ItemStorageKind::TradeEscrow && storage.storage.id == auriga::item::OwnedGroundStorageId;
		});
	const auto details = auriga::item::EncodeTypedItemDetails(*catalog, visible, ownedDropEnabled, {}, scriptedUse);
	const auto bytes = auriga::item::EncodeTypedInventory(*catalog, storages, state.wallets, visible, companion);
	if (!details)
	{
		LOG_ERROR("TypedItems: pid {} details not encodable (error {})", ecs::PlayerRuntime::GetPlayerID(character), static_cast<int>(details.error()));
		return;
	}
	if (!bytes)
	{
		LOG_ERROR("TypedItems: pid {} inventory not encodable (error {})", ecs::PlayerRuntime::GetPlayerID(character), static_cast<int>(bytes.error()));
		return;
	}
	desc->LargePacket(details->data(), static_cast<int>(details->size()));
	const int size = static_cast<int>(bytes->size());
	if (size > 16 * 1024)
		desc->LargePacket(bytes->data(), size);
	else
		desc->Packet(bytes->data(), size);
	SendTypedEquipment(character);
    SendTypedCostumeAppearance(character,character);
    RefreshTypedStoneExchange(character);
}
