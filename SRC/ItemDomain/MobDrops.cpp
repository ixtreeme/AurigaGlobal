#include <item/MobDrops.hpp>
#include <item/ItemCatalog.hpp>
#include "ContentValidation.hpp"
#include <algorithm>
#include <limits>

namespace auriga::item
{
namespace
{
using E = MobDropGroupError;
bool Chanced(MobDropRoll roll) noexcept { return roll != MobDropRoll::OneInKills; }
}

std::string_view MobDropGroupErrorName(MobDropGroupError e) noexcept
{
    switch (e)
    {
    case E::InvalidIdentity: return "InvalidIdentity";
    case E::InvalidTarget: return "InvalidTarget";
    case E::InvalidRoll: return "InvalidRoll";
    case E::EmptyGroup: return "EmptyGroup";
    case E::TooManyEntries: return "TooManyEntries";
    case E::InvalidEntryOrder: return "InvalidEntryOrder";
    case E::InvalidItem: return "InvalidItem";
    case E::InvalidLevel: return "InvalidLevel";
    case E::InvalidAmount: return "InvalidAmount";
    case E::InvalidChance: return "InvalidChance";
    case E::InvalidWeight: return "InvalidWeight";
    case E::WeightOverflow: return "WeightOverflow";
    case E::InvalidRareChance: return "InvalidRareChance";
    case E::InvalidKillerLevels: return "InvalidKillerLevels";
    }
    return "Unknown";
}

std::expected<void, MobDropGroupError> ValidateMobDropGroup(MobDropGroup& group)
{
    if (!group.id || !detail::ContentCode(group.code)) return std::unexpected(E::InvalidIdentity);
    switch (group.target)
    {
    case MobDropTarget::Mob: if (!group.targetValue) return std::unexpected(E::InvalidTarget); break;
    // The common drop of a rank: every entry rolls (common_drop_item.json).
    case MobDropTarget::Rank: if (group.targetValue > MaxMobRank || group.roll != MobDropRoll::EachEntry) return std::unexpected(E::InvalidTarget); break;
    default: return std::unexpected(E::InvalidTarget);
    }
    if (group.roll < MobDropRoll::EachEntry || group.roll > MobDropRoll::DoubleDrop ||
        (group.roll == MobDropRoll::OneInKills) != (group.killsPerDrop != 0) ||
        (group.roll == MobDropRoll::LevelGated) != (group.minimumKillerLevel != 0))
        return std::unexpected(E::InvalidRoll);
    if (group.entries.empty()) return std::unexpected(E::EmptyGroup);
    if (group.entries.size() > MaxMobDropEntries) return std::unexpected(E::TooManyEntries);
    std::ranges::sort(group.entries, {}, &MobDropEntry::entry);
    std::uint64_t total = 0;
    for (std::size_t i = 0; i < group.entries.size(); ++i)
    {
        const auto& entry = group.entries[i];
        if (entry.entry != i + 1) return std::unexpected(E::InvalidEntryOrder);
        if (!entry.amount) return std::unexpected(E::InvalidAmount);
        if (Chanced(group.roll) ? (!entry.chancePpb || entry.chancePpb > MobDropChanceScale || entry.weight)
                                : (entry.chancePpb || !entry.weight))
            return std::unexpected(Chanced(group.roll) ? E::InvalidChance : E::InvalidWeight);
        total += entry.weight;
        if (total > std::numeric_limits<std::uint32_t>::max()) return std::unexpected(E::WeightOverflow);
        if (entry.rareAttributeChanceBp > 10'000) return std::unexpected(E::InvalidRareChance);
        if (!entry.killerLevelMin || entry.killerLevelMin > entry.killerLevelMax) return std::unexpected(E::InvalidKillerLevels);
    }
    return {};
}

std::expected<void, MobDropGroupError> ValidateMobDropItems(const MobDropGroup& group, const ItemCatalog& catalog)
{
    for (const auto& entry : group.entries)
    {
        const auto* item = catalog.Find(entry.item);
        if (!item || item->base.retired) return std::unexpected(E::InvalidItem);
        if (entry.amount > item->base.maxStack) return std::unexpected(E::InvalidAmount);
        const auto* profile = catalog.FindRefinement(item->base.refinementProfile);
        if (!profile || !profile->FindLevel(entry.refinementLevel)) return std::unexpected(E::InvalidLevel);
    }
    return {};
}

std::expected<std::vector<MobDrop>, MobDropError> PlanMobDrops(const ItemCatalog& catalog, const MobDropContext& kill, const MobDropDraw& draw)
{
    if (!kill.randomRange || !kill.killerLevel) return std::unexpected(MobDropError::InvalidContext);
    std::vector<MobDrop> drops;
    const auto roll = [&](std::uint32_t bound) -> std::expected<std::uint32_t, MobDropError> {
        const auto value = draw(bound);
        if (value < 1 || value > bound) return std::unexpected(MobDropError::DrawOutOfRange);
        return value;
    };
    const auto give = [&](const MobDropGroup& group, const MobDropEntry& entry) {
        drops.push_back({group.id, entry.entry, entry.item, entry.refinementLevel, entry.amount, entry.rareAttributeChanceBp});
    };
    // Legacy: (dwPct * delta / 100) >= number(1, range), with dwPct = ppb / 250.
    const auto scaled = [&](std::uint32_t ppb) { return static_cast<std::int64_t>(ppb) * kill.deltaPercent / 25'000; };
    const auto inWindow = [&](const MobDropEntry& entry) {
        return kill.killerLevel >= entry.killerLevelMin && kill.killerLevel <= entry.killerLevelMax;
    };
    for (const auto* group : catalog.MobDropGroupsFor(kill.mobVnum, kill.mobRank))
    {
        if (group->target == MobDropTarget::Rank && kill.stone) continue;
        switch (group->roll)
        {
        case MobDropRoll::DoubleDrop:
            if (kill.stone || !kill.doubleDrop) break;
            [[fallthrough]];
        case MobDropRoll::EachEntry:
            for (const auto& entry : group->entries)
            {
                if (!inWindow(entry)) continue;
                const auto value = roll(kill.randomRange); if (!value) return std::unexpected(value.error());
                if (scaled(entry.chancePpb) >= static_cast<std::int64_t>(*value)) give(*group, entry);
            }
            break;
        case MobDropRoll::OneInKills:
        {
            // Legacy: (40000 * delta / killDrop) >= number(1, range), then one
            // weighted entry: number(1, total) against the cumulative weights.
            const auto value = roll(kill.randomRange); if (!value) return std::unexpected(value.error());
            if (40'000 * kill.deltaPercent / group->killsPerDrop < static_cast<std::int64_t>(*value)) break;
            std::uint32_t total = 0;
            for (const auto& entry : group->entries) total += entry.weight;
            const auto pick = roll(total); if (!pick) return std::unexpected(pick.error());
            std::uint32_t cumulative = 0;
            for (const auto& entry : group->entries)
                if ((cumulative += entry.weight) >= *pick) { if (inWindow(entry)) give(*group, entry); break; }
            break;
        }
        case MobDropRoll::LevelGated:
            // Legacy: dwPct >= number(1, 1000000), unscaled; here ppb >= [1, 1e9].
            if (kill.stone || kill.killerLevel < group->minimumKillerLevel) break;
            for (const auto& entry : group->entries)
            {
                if (!inWindow(entry)) continue;
                const auto value = roll(MobDropChanceScale); if (!value) return std::unexpected(value.error());
                if (entry.chancePpb >= *value) give(*group, entry);
            }
            break;
        }
    }
    return drops;
}
} // namespace auriga::item
