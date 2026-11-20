#include "stdafx.h"
#include "TypedWikiSources.hpp"
#include "ItemCatalogRuntime.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../entity/mob_manager.h"
#include "../network/desc.h"

#include <Headers/Header_GC.h>
#include <item/ItemCatalog.hpp>

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <string_view>
#include <vector>

namespace
{
namespace items = auriga::item;
enum class SourceKind : std::uint8_t { Mob = 1, Rank = 2, Container = 3 };
enum class Status : std::uint8_t { Ready, UnknownItem, Unavailable, Truncated };

struct Source
{
    SourceKind kind;
    std::uint16_t category;
    std::uint32_t id;

    auto operator<=>(const Source&) const = default;
};

using SourceIndex = std::map<items::ItemKey, std::vector<Source>>;

std::shared_ptr<const SourceIndex> GetIndex(const std::shared_ptr<const items::ItemCatalog>& catalog)
{
    static std::mutex mutex;
    static std::weak_ptr<const items::ItemCatalog> indexedCatalog;
    static std::shared_ptr<const SourceIndex> cachedIndex;
    std::lock_guard lock(mutex);
    if (cachedIndex && indexedCatalog.lock() == catalog)
        return cachedIndex;

    auto index = std::make_shared<SourceIndex>();
    for (const auto& group : catalog->MobDropGroups())
    {
        const bool byRank = group.target == items::MobDropTarget::Rank;
        if (!byRank)
        {
            const auto* mob = CMobManager::instance().Get(group.targetValue);
            if (!mob || (mob->m_table.bType != CHAR_TYPE_MONSTER && mob->m_table.bType != CHAR_TYPE_STONE))
                continue;
            if (mob->m_table.bType == CHAR_TYPE_STONE &&
                (group.roll == items::MobDropRoll::DoubleDrop || group.roll == items::MobDropRoll::LevelGated))
                continue;
        }
        const Source source {byRank ? SourceKind::Rank : SourceKind::Mob, 0, group.targetValue};
        for (const auto& entry : group.entries)
            if (entry.killerLevelMax >= group.minimumKillerLevel)
                (*index)[entry.item].push_back(source);
    }

    for (const auto& [key, entry] : catalog->Entries())
    {
        const auto* box = std::get_if<items::ItemContainerRow>(&entry.category);
        if (entry.base.retired || !box || !box->lootProfile)
            continue;
        const auto* loot = catalog->FindLoot(*box->lootProfile);
        if (!loot) continue;

        const Source source {SourceKind::Container, static_cast<std::uint16_t>(key.Category()), key.LocalId()};
        for (const auto& reward : loot->Data().entries)
            if (reward.kind == items::LootRewardKind::Item && reward.item)
                (*index)[*reward.item].push_back(source);
    }

    for (auto& [key, sources] : *index)
    {
        std::ranges::sort(sources);
        sources.erase(std::unique(sources.begin(), sources.end()), sources.end());
    }
    indexedCatalog = catalog;
    cachedIndex = index;
    return index;
}
}

void HandleTypedWikiSources(entt::entity player, const char* data)
{
    if (!ecs::PlayerRuntime::IsPC(player)) return;
    auto* desc = ecs::PlayerRuntime::GetDesc(player);
    if (!desc) return;

    const auto* request = reinterpret_cast<const std::uint8_t*>(data);
    const auto read = [&](std::size_t at, unsigned bytes) {
        std::uint32_t value = 0;
        for (unsigned i = 0; i < bytes; ++i)
            value |= std::uint32_t{request[at + i]} << (i * 8);
        return value;
    };
    const auto id = read(2, 4);
    const auto category = static_cast<std::uint16_t>(read(6, 2));
    const auto local = read(8, 4);
    if (request[1] != 1 || !id || !category || !local) return;

    const auto key = items::ItemKey::Create(category, local);
    const auto catalog = items::runtime::CurrentDropCatalog();
    const auto* item = catalog && key ? catalog->Find(*key) : nullptr;
    Status status = !catalog ? Status::Unavailable : !item || item->base.retired ? Status::UnknownItem : Status::Ready;

    std::vector<std::uint8_t> packet;
    const auto put = [&](std::uint32_t value, unsigned bytes) {
        for (unsigned i = 0; i < bytes; ++i)
            packet.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
    };
    const auto putText = [&](std::string_view value) {
        put(static_cast<std::uint32_t>(value.size()), 2);
        packet.insert(packet.end(), value.begin(), value.end());
    };
    put(HEADER_GC_TYPED_WIKI_SOURCES, 1);
    put(0, 4);
    put(1, 1);
    put(id, 4);
    put(0, 1);
    put(category, 2);
    put(local, 4);
    put(0, 2);

    std::uint16_t count = 0;
    if (status == Status::Ready)
    {
        const auto index = GetIndex(catalog);
        if (const auto it = index->find(*key); it != index->end())
        {
            constexpr std::size_t maxFrame = 1 << 20;
            constexpr auto maxShort = std::numeric_limits<std::uint16_t>::max();
            for (const auto& source : it->second)
            {
                std::string_view nameKey, code, icon;
                if (source.kind == SourceKind::Container)
                {
                    const auto* container = catalog->Find(*items::ItemKey::Create(source.category, source.id));
                    nameKey = container->base.nameKey;
                    code = container->base.contentCode;
                    icon = container->base.iconAsset;
                }
                if (nameKey.size() > maxShort || code.size() > maxShort || icon.size() > maxShort)
                {
                    status = Status::Truncated;
                    continue;
                }
                if (count == maxShort || packet.size() + 13 + nameKey.size() + code.size() + icon.size() > maxFrame)
                {
                    status = Status::Truncated;
                    break;
                }
                put(static_cast<std::uint8_t>(source.kind), 1);
                put(source.category, 2);
                put(source.id, 4);
                putText(nameKey);
                putText(code);
                putText(icon);
                ++count;
            }
        }
    }

    for (unsigned i = 0; i < 4; ++i)
        packet[1 + i] = static_cast<std::uint8_t>(packet.size() >> (i * 8));
    packet[10] = static_cast<std::uint8_t>(status);
    packet[17] = static_cast<std::uint8_t>(count);
    packet[18] = static_cast<std::uint8_t>(count >> 8);
    if (packet.size() > 65535)
        desc->LargePacket(packet.data(), static_cast<int>(packet.size()));
    else
        desc->Packet(packet.data(), static_cast<int>(packet.size()));
}
