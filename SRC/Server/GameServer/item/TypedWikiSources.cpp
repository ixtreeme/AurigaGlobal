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
enum class Status : std::uint8_t { Ready, UnknownTarget, Unavailable, Truncated, NoLootTable };
enum class LootTarget : std::uint8_t { Mob = 1, Container = 2 };

bool IsLootMob(const CMob* mob)
{
    return mob && (mob->m_table.bType == CHAR_TYPE_MONSTER || mob->m_table.bType == CHAR_TYPE_STONE);
}

bool GroupAppliesToMob(const items::MobDropGroup& group, const CMob& mob)
{
    return mob.m_table.bType != CHAR_TYPE_STONE ||
        (group.target != items::MobDropTarget::Rank && group.roll != items::MobDropRoll::DoubleDrop &&
         group.roll != items::MobDropRoll::LevelGated);
}

std::uint16_t EffectiveMinimumLevel(const items::MobDropGroup& group, const items::MobDropEntry& entry)
{
    return std::max(group.minimumKillerLevel, entry.killerLevelMin);
}

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
            if (!IsLootMob(mob) || !GroupAppliesToMob(group, *mob))
                continue;
        }
        const Source source {byRank ? SourceKind::Rank : SourceKind::Mob, 0, group.targetValue};
        for (const auto& entry : group.entries)
            if (entry.killerLevelMax >= EffectiveMinimumLevel(group, entry))
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
    Status status = !catalog ? Status::Unavailable : !item || item->base.retired ? Status::UnknownTarget : Status::Ready;

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

void HandleTypedWikiLoot(entt::entity player, const char* data)
{
    if (!ecs::PlayerRuntime::IsPC(player)) return;
    auto* desc = ecs::PlayerRuntime::GetDesc(player);
    if (!desc) return;

    const auto* request = reinterpret_cast<const std::uint8_t*>(data);
    const auto read = [&](std::size_t offset, unsigned bytes) {
        std::uint32_t value = 0;
        for (unsigned i = 0; i < bytes; ++i)
            value |= std::uint32_t{request[offset + i]} << (8 * i);
        return value;
    };
    const auto id = read(2, 4);
    const auto targetKind = static_cast<LootTarget>(request[6]);
    const auto category = static_cast<std::uint16_t>(read(7, 2));
    const auto target = read(9, 4);
    if (request[1] != 1 || !id || !target ||
        (targetKind != LootTarget::Mob && targetKind != LootTarget::Container) ||
        (targetKind == LootTarget::Mob ? category != 0 : category == 0))
        return;

    std::vector<std::uint8_t> packet;
    const auto put = [&](std::uint64_t value, unsigned bytes) {
        for (unsigned i = 0; i < bytes; ++i)
            packet.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
    };
    const auto putText = [&](std::string_view value) {
        put(value.size(), 2);
        packet.insert(packet.end(), value.begin(), value.end());
    };

    put(HEADER_GC_TYPED_WIKI_LOOT, 1);
    put(0, 4);
    put(1, 1);
    put(id, 4);
    put(request[6], 1);
    put(0, 1);
    put(category, 2);
    put(target, 4);
    put(0, 2);

    const auto catalog = items::runtime::CurrentDropCatalog();
    Status status = catalog ? Status::Ready : Status::Unavailable;
    std::uint16_t count = 0;
    const auto append = [&](items::LootRewardKind kind, std::optional<items::ItemKey> key, std::uint16_t level,
                            std::uint64_t amount, bool doubleDrop, std::uint16_t minimum, std::uint16_t maximum) {
        const auto* item = key ? catalog->Find(*key) : nullptr;
        if (key && (!item || item->base.retired))
            return true;
        const std::string_view name = item ? std::string_view(item->base.nameKey) : std::string_view{};
        const std::string_view code = item ? std::string_view(item->base.contentCode) : std::string_view{};
        const std::string_view icon = item ? std::string_view(item->base.iconAsset) : std::string_view{};
        const std::string_view description = item ? std::string_view(item->base.descriptionKey) : std::string_view{};
        constexpr auto maxShort = std::numeric_limits<std::uint16_t>::max();
        if (name.size() > maxShort || code.size() > maxShort || icon.size() > maxShort || description.size() > maxShort)
        {
            status = Status::Truncated;
            return true;
        }
        if (count == maxShort || packet.size() + 31 + name.size() + code.size() + icon.size() + description.size() > (1 << 20))
        {
            status = Status::Truncated;
            return false;
        }

        put(static_cast<std::uint8_t>(kind), 1);
        put(key ? static_cast<std::uint16_t>(key->Category()) : 0, 2);
        put(key ? key->LocalId() : 0, 4);
        put(level, 2);
        put(amount, 8);
        put(doubleDrop ? 1 : 0, 1);
        put(minimum, 2);
        put(maximum, 2);
        put(item ? item->base.height : 1, 1);
        putText(name);
        putText(code);
        putText(icon);
        putText(description);
        ++count;
        return true;
    };

    if (catalog && targetKind == LootTarget::Mob)
    {
        const auto* mob = CMobManager::instance().Get(target);
        if (!IsLootMob(mob))
            status = Status::UnknownTarget;
        else
        {
            bool hasRoom = true;
            for (const auto* group : catalog->MobDropGroupsFor(target, mob->m_table.bRank))
            {
                if (!hasRoom) break;
                if (!GroupAppliesToMob(*group, *mob)) continue;
                for (const auto& entry : group->entries)
                {
                    const auto minimum = EffectiveMinimumLevel(*group, entry);
                    if (minimum > entry.killerLevelMax) continue;
                    hasRoom = append(items::LootRewardKind::Item, entry.item, entry.refinementLevel, entry.amount,
                        group->roll == items::MobDropRoll::DoubleDrop, minimum, entry.killerLevelMax);
                    if (!hasRoom) break;
                }
            }
        }
    }
    else if (catalog)
    {
        const auto key = items::ItemKey::Create(category, target);
        const auto* item = key ? catalog->Find(*key) : nullptr;
        const auto* container = item && !item->base.retired ? std::get_if<items::ItemContainerRow>(&item->category) : nullptr;
        const auto* loot = container && container->lootProfile ? catalog->FindLoot(*container->lootProfile) : nullptr;
        if (!container)
            status = Status::UnknownTarget;
        else if (!loot)
            status = Status::NoLootTable;
        else
            for (const auto& entry : loot->Data().entries)
                if (!append(entry.kind, entry.item, entry.refinementLevel, entry.amount, false, 0, 0))
                    break;
    }

    for (unsigned i = 0; i < 4; ++i)
        packet[1 + i] = static_cast<std::uint8_t>(packet.size() >> (8 * i));
    packet[11] = static_cast<std::uint8_t>(status);
    packet[18] = static_cast<std::uint8_t>(count);
    packet[19] = static_cast<std::uint8_t>(count >> 8);
    if (packet.size() > 65535)
        desc->LargePacket(packet.data(), static_cast<int>(packet.size()));
    else
        desc->Packet(packet.data(), static_cast<int>(packet.size()));
}
