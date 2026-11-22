#include "stdafx.h"
#include "TypedChatLinks.hpp"
#include "config.h"
#include "desc.h"
#include "../ecs/Registry.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../item/ItemCatalogRuntime.hpp"
#include "../item/ItemStore.hpp"
#include "../item/TypedQuestItems.hpp"

#include <Headers/Header_GC.h>
#include <charconv>
#include <item/TypedItemDetails.hpp>

namespace
{
namespace items = auriga::item;
constexpr std::string_view PrismCode = "catalog.10.noname_3ecfd1022b96";
constexpr std::string_view ReferencePrefix = "tr:";

const items::PersistentItem* FindShareable(const items::persistence::LoadedItemOwner& state, std::uint64_t id)
{
    const auto instance = items::ItemInstanceId::Create(id);
    if (!instance)
        return nullptr;
    const auto found = std::ranges::find(state.items, *instance, &items::PersistentItem::id);
    if (found == state.items.end() || found->destroyed || found->owner != state.fence.owner || !found->placement)
        return nullptr;
    using Kind = items::ItemStorageKind;
    switch (found->placement->kind)
    {
        case Kind::Inventory:
        case Kind::Equipment:
        case Kind::SpecialInventory:
        case Kind::MountInventory:
            return &*found;
        default:
            return nullptr;
    }
}

bool AppendLink(std::string& out, const items::ItemCatalog& catalog, const items::PersistentItem& item)
{
    const auto* entry = catalog.Find(item.key);
    if (!entry || entry->base.retired || !catalog.RefinedBaseStats(item.key, item.refinementLevel))
        return false;
    const auto& name = entry->base.nameKey;
    if (name.empty() || name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.-") != std::string::npos)
        return false;
    out += fmt::format("|Hti:{}:{}:{}:{}:{}:", static_cast<unsigned>(item.key.Category()), item.key.LocalId(),
        item.refinementLevel, item.count, item.boundTo.has_value() ? 1 : 0);
    bool separator = false;
    for (std::size_t slot = 0; slot < item.attributes.slots.size(); ++slot)
    {
        if (const auto& bonus = item.attributes.slots[slot])
        {
            if (separator) out += ';';
            out += fmt::format("{},{},{}", slot, static_cast<unsigned>(bonus->bonus), bonus->valueScaled);
            separator = true;
        }
    }
    out += ':';
    separator = false;
    for (std::size_t slot = 0; slot < item.stones.size(); ++slot)
    {
        const auto& socket = item.stones[slot];
        if (socket.state == items::StoneSocketState::Closed)
            continue;
        if (separator) out += ';';
        out += fmt::format("{},{},{},{},{},{}", slot, static_cast<unsigned>(socket.state), socket.grade,
            socket.stone ? static_cast<unsigned>(socket.stone->Category()) : 0,
            socket.stone ? socket.stone->LocalId() : 0, socket.refinementLevel);
        separator = true;
    }
    out += "|h[" + name + "]|h";
    return true;
}

std::uint64_t Read(const char* data, unsigned width)
{
    std::uint64_t value = 0;
    for (unsigned i = 0; i < width; ++i)
        value |= static_cast<std::uint64_t>(static_cast<unsigned char>(data[i])) << (i * 8);
    return value;
}

void Tell(entt::entity player, const char* message)
{
    ecs::ChatSystem::Send(player, CHAT_TYPE_INFO, "%s", message);
}
}

bool PrepareTypedChatLinks(entt::entity sender, std::string_view message, std::size_t maxSize,
    std::function<void(std::string)> deliver)
{
    if (message.find('|') == std::string_view::npos)
        return false;
    std::string result;
    result.reserve(message.size());
    std::uint32_t links = 0;
    bool color = false;
    const auto catalog = items::runtime::CurrentCatalog();
    const auto* owned = g_registry.try_get<ecs::TypedItems>(sender);
    const auto invalid = [&] {
        Tell(sender, "A tárgylink nem érvényes vagy a tárgy már nincs a készletedben.");
        return true;
    };
    for (std::size_t i = 0; i < message.size();)
    {
        const auto rest = message.substr(i);
        if (rest.starts_with("||"))
        {
            result += "||";
            i += 2;
        }
        else if (rest.starts_with("|H"))
        {
            const auto payloadEnd = message.find("|h", i + 2);
            if (payloadEnd == std::string_view::npos)
                return invalid();
            const auto labelEnd = message.find("|h", payloadEnd + 2);
            auto payload = message.substr(i + 2, payloadEnd - i - 2);
            if (labelEnd == std::string_view::npos || !payload.starts_with(ReferencePrefix) ||
                !catalog || !owned)
                return invalid();
            payload.remove_prefix(ReferencePrefix.size());
            std::uint64_t id = 0;
            const auto [end, error] = std::from_chars(payload.data(), payload.data() + payload.size(), id);
            if (error != std::errc{} || end != payload.data() + payload.size())
                return invalid();
            const auto* item = FindShareable(owned->state, id);
            if (!item || !AppendLink(result, *catalog, *item))
                return invalid();
            ++links;
            i = labelEnd + 2;
        }
        else
        {
            color = color || rest.starts_with("|c");
            result += message[i++];
        }
        if (result.size() > maxSize)
        {
            Tell(sender, "A tárgylinkekkel együtt túl hosszú az üzenet.");
            return true;
        }
    }
    if (color && !links)
        return invalid();
    if (!links)
        return false;
#ifdef ENABLE_NEWSTUFF
    if (g_bDisablePrismNeed)
    {
        deliver(std::move(result));
        return true;
    }
#endif
    const auto& definitions = catalog->Entries();
    const auto prism = std::ranges::find_if(definitions, [](const auto& row) {
        return row.second.base.contentCode == PrismCode && !row.second.base.retired;
    });
    if (prism == definitions.end())
    {
        Tell(sender, "A tárgylink küldéséhez szükséges Betekintés üvege nem érhető el.");
        return true;
    }
    const auto write = RequestTypedGameplayItemExchange(sender, {{prism->first, 0, links}}, {}, 0,
        [sender, text = std::move(result), deliver = std::move(deliver)](bool committed) mutable {
            if (committed)
                deliver(std::move(text));
            else
                Tell(sender, "A tárgylink küldése most nem sikerült.");
        });
    if (!write)
        Tell(sender, write.error() == items::ItemMissionError::Requirements
            ? "A tárgylink küldéséhez Betekintés üvege szükséges." : "A tárgylink küldése most nem sikerült.");
    return true;
}

void HandleTypedChatItem(entt::entity player, const char* data)
{
    auto* desc = ecs::PlayerRuntime::IsPC(player) ? ecs::PlayerRuntime::GetDesc(player) : nullptr;
    const auto catalog = items::runtime::CurrentCatalog();
    const auto request = Read(data + 2, 4);
    if (!desc || !catalog || data[1] != 1 || request == 0)
        return;

    std::vector<items::ItemGrant> preview;
    for (unsigned index = 0; index < 4; ++index)
    {
        const auto* row = data + 6 + index * 8;
        const auto key = items::ItemKey::Create(Read(row, 2), Read(row + 2, 4));
        const auto level = static_cast<std::uint16_t>(Read(row + 6, 2));
        const auto* entry = key ? catalog->Find(*key) : nullptr;
        if (entry && !entry->base.retired && catalog->RefinedBaseStats(*key, level))
            preview.push_back({*key, level, 1});
        else if (index == 0)
            break;
    }
    const auto metadata = items::EncodeTypedItemDetails(*catalog, {}, false, preview);
    if (!metadata || metadata->size() + 10 > (1u << 20))
        return;
    std::vector<std::byte> packet;
    const auto put = [&](std::uint64_t value, unsigned width) {
        for (unsigned i = 0; i < width; ++i)
            packet.push_back(static_cast<std::byte>((value >> (i * 8)) & 0xff));
    };
    put(HEADER_GC_TYPED_CHAT_ITEM, 1);
    put(metadata->size() + 10, 4);
    put(1, 1);
    put(request, 4);
    packet.insert(packet.end(), metadata->begin(), metadata->end());
    desc->LargePacket(packet.data(), static_cast<int>(packet.size()));
}
