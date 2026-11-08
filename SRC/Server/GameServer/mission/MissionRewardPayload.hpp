#pragma once
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <compare>

namespace auriga::mission
{
struct RewardLimits
{
    static constexpr size_t Bundles = 128, ItemEntries = 32, DefinitionItems = 512, PackageItems = 4096;
    static constexpr uint32_t ItemCount = 50'000;
    static constexpr uint64_t Amount = INT64_MAX;
};
enum class RewardPayloadKind : uint8_t
{
    Item = 1,
    Experience = 2,
    Gold = 3,
    ConsumeItem = 4,
    TypedItem = 5, TypedConsumeItem = 6, TypedRequirement = 7, GoldCost = 8, Levels = 9, AccountCoins = 10
};
// Unordered, all-or-nothing bundle. Costs require pre-existing resources; grants
// cannot finance costs in this same bundle. No random rolls, scripts or silent
// clamping. Decoding a payload alone never authorizes a gameplay effect.
struct RewardBundle
{
    uint16_t schema = 1;
    std::map<uint32_t, uint32_t> items;
    uint64_t experience = 0, gold = 0;
    std::map<uint32_t, uint32_t> consumeItems;
    struct ItemKey
    {
        uint16_t category = 0;
        uint32_t local = 0;
        uint16_t level = 0;
        auto operator<=>(const ItemKey &) const = default;
    };
    // Schema 2 is exclusively category/local/level based. Legacy prototype
    // numbers are never packed into, or inferred from, these keys.
    std::map<ItemKey, uint32_t> typedItems, typedConsumeItems, typedRequirements;
    uint64_t goldCost = 0, accountCoins = 0;
    uint16_t levels = 0;
    bool operator==(const RewardBundle &) const = default;
};
inline bool ValidRewardBundle(const RewardBundle &bundle)
{
    if (bundle.schema == 2 || bundle.schema == 3)
    {
        if (!bundle.items.empty() || !bundle.consumeItems.empty() || (bundle.schema == 2 && (bundle.experience || bundle.levels || bundle.accountCoins)) ||
            bundle.experience > RewardLimits::Amount || bundle.accountCoins > RewardLimits::Amount || bundle.levels > 254 ||
            bundle.gold > RewardLimits::Amount || bundle.goldCost > RewardLimits::Amount ||
            (bundle.typedItems.empty() && bundle.typedConsumeItems.empty() && bundle.gold == bundle.goldCost && !bundle.experience && !bundle.levels && !bundle.accountCoins))
            return false;
        size_t total = 0;
        for (const auto *table : {&bundle.typedItems, &bundle.typedConsumeItems, &bundle.typedRequirements})
        {
            if (table->size() > 16 || table->size() > RewardLimits::ItemEntries - total)
                return false;
            total += table->size();
            for (const auto &[key, count] : *table)
                if (!key.category || !key.local || key.level > 255 || !count || count > RewardLimits::ItemCount)
                    return false;
        }
        return true;
    }
    if (bundle.schema != 1 || !bundle.typedItems.empty() || !bundle.typedConsumeItems.empty() ||
        !bundle.typedRequirements.empty() || bundle.goldCost || bundle.levels || bundle.accountCoins || bundle.items.size() > RewardLimits::ItemEntries ||
        bundle.consumeItems.size() > RewardLimits::ItemEntries - bundle.items.size() ||
        bundle.experience > RewardLimits::Amount || bundle.gold > RewardLimits::Amount ||
        (bundle.items.empty() && bundle.consumeItems.empty() && !bundle.experience && !bundle.gold))
        return false;
    for (const auto *table : {&bundle.items, &bundle.consumeItems})
        for (const auto &[vnum, count] : *table)
            if (!vnum || !count || count > RewardLimits::ItemCount)
                return false;
    return true;
}
} // namespace auriga::mission
