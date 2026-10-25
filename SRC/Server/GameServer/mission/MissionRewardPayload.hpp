#pragma once
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>

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
    ConsumeItem = 4
};
// Unordered, all-or-nothing bundle. Costs require pre-existing resources; grants
// cannot finance costs in this same bundle. No random rolls, scripts or silent
// clamping. A typed payload is NOT an implemented gameplay effect.
struct RewardBundle
{
    uint16_t schema = 1;
    std::map<uint32_t, uint32_t> items;
    uint64_t experience = 0, gold = 0;
    std::map<uint32_t, uint32_t> consumeItems;
    bool operator==(const RewardBundle &) const = default;
};
inline bool ValidRewardBundle(const RewardBundle &bundle)
{
    if (bundle.schema != 1 || bundle.items.size() > RewardLimits::ItemEntries ||
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
