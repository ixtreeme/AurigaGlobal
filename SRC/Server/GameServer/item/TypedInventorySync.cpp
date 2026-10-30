#include "stdafx.h"
#include "TypedInventorySync.hpp"
#include "ItemCatalogRuntime.hpp"
#include "ItemStore.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../network/desc.h"
#include "../core/packet.h"

#include <item/TypedInventoryWire.hpp>
#include <Core/Logging.hpp>

static_assert(HEADER_GC_TYPED_INVENTORY == auriga::item::TypedInventoryHeader, "GC_TYPED_INVENTORY header differs from the wire codec");

void SendTypedInventory(entt::entity character)
{
	if (!g_registry.valid(character))
		return;
	const auto* typed = g_registry.try_get<ecs::TypedItems>(character);
	auto* desc = ecs::PlayerRuntime::GetDesc(character);
	if (!typed || !desc || !desc->IsPhase(PHASE_GAME))
		return;
	const auto catalog = auriga::item::runtime::CurrentCatalog();
	if (!catalog)
		return;
	const auto& state = typed->state;
	const auto bytes = auriga::item::EncodeTypedInventory(*catalog, state.storages, state.wallets, state.items);
	if (!bytes)
	{
		LOG_ERROR("TypedItems: pid {} inventory not encodable (error {})", ecs::PlayerRuntime::GetPlayerID(character), static_cast<int>(bytes.error()));
		return;
	}
	const int size = static_cast<int>(bytes->size());
	if (size > 16 * 1024)
		desc->LargePacket(bytes->data(), size);
	else
		desc->Packet(bytes->data(), size);
}
