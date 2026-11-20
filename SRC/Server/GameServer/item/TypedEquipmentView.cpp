#include "stdafx.h"
#include "TypedEquipmentView.hpp"

#include "ItemCatalogRuntime.hpp"
#include "ItemStore.hpp"
#include "TypedCostumes.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../network/desc.h"

#include <Headers/Header_GC.h>
#include <item/ItemWireCodec.hpp>
#include <item/TypedItemDetails.hpp>

#include <algorithm>
#include <vector>

void SendTypedEquipmentView(entt::entity wearer, entt::entity viewer, EquipmentViewContext context)
{
    namespace item = auriga::item;
    if (!g_registry.valid(wearer) || !g_registry.valid(viewer) ||
        !ecs::PlayerRuntime::IsPC(wearer) || !ecs::PlayerRuntime::IsPC(viewer))
        return;

    auto* desc = ecs::PlayerRuntime::GetDesc(viewer);
    const auto vid = ecs::PlayerRuntime::GetPacketVID(wearer);
    const auto* typed = g_registry.try_get<ecs::TypedItems>(wearer);
    const auto catalog = item::runtime::CurrentCatalog();
    if (!desc || !vid || !typed || !catalog) return;

    std::vector<item::PersistentItem> worn;
    for (const auto& instance : typed->state.items)
    {
        const auto& slot = instance.placement;
        if (!instance.destroyed && instance.owner == typed->state.fence.owner && slot &&
            slot->kind == item::ItemStorageKind::Equipment && slot->storageId == 1 &&
            slot->anchor < item::EquipmentSlotCount)
            worn.push_back(instance);
    }
    ProjectTypedCostumes(wearer, worn);
    std::erase_if(worn, [](const auto& instance) { return instance.destroyed || !instance.placement; });
    std::ranges::sort(worn, {}, [](const auto& instance) { return instance.placement->anchor; });

    const auto details = item::EncodeTypedItemDetails(*catalog, worn);
    if (!details) return;

    // v1: context, wearer VID, sparse equipment slots, then length-prefixed item metadata.
    // A slot carries the type key, refinement level, quantity, attributes and sockets.
    // Only the wearer VID is public. Item instance IDs and owner state stay server-side.
    std::vector<std::byte> packet;
    const auto put = [&](std::uint64_t value, unsigned width) {
        for (unsigned i = 0; i < width; ++i)
            packet.push_back(static_cast<std::byte>((value >> (8 * i)) & 0xff));
    };
    const auto putKey = [&](item::ItemKey key) {
        const auto bytes = item::EncodeItemKey(key);
        packet.insert(packet.end(), bytes.begin(), bytes.end());
    };

    put(HEADER_GC_TYPED_EQUIPMENT_VIEW, 1);
    put(0, 4);
    put(1, 1);
    put(static_cast<std::uint8_t>(context), 1);
    put(vid, 4);
    put(worn.size(), 1);
    for (const auto& instance : worn)
    {
        put(instance.placement->anchor, 1);
        putKey(instance.key);
        put(instance.refinementLevel, 2);
        put(instance.count, 4);
        const auto& attributes = instance.attributes.slots;
        put(std::ranges::count_if(attributes, [](const auto& slot) { return slot.has_value(); }), 1);
        for (std::size_t i = 0; i < attributes.size(); ++i)
        {
            if (!attributes[i]) continue;
            put(i, 1);
            put(static_cast<std::uint32_t>(attributes[i]->bonus), 4);
            put(static_cast<std::uint64_t>(attributes[i]->valueScaled), 8);
        }
        for (const auto& socket : instance.stones)
        {
            put(static_cast<std::uint8_t>(socket.state), 1);
            put(socket.grade, 1);
            put(socket.refinementLevel, 2);
            if (socket.stone) putKey(*socket.stone);
            else put(0, item::ItemKeyWireSize);
        }
    }

    put(details->size(), 4);
    if (packet.size() + details->size() > (1u << 20)) return;
    packet.insert(packet.end(), details->begin(), details->end());
    for (unsigned i = 0; i < 4; ++i)
        packet[1 + i] = static_cast<std::byte>((packet.size() >> (8 * i)) & 0xff);
    if (packet.size() > 65535)
        desc->LargePacket(packet.data(), static_cast<int>(packet.size()));
    else
        desc->Packet(packet.data(), static_cast<int>(packet.size()));
}
