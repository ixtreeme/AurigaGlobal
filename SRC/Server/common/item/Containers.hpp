#pragma once
#include "ItemKey.hpp"
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace auriga::item
{
// Typed boxes, locked chests and their keys. Replaces special_item_group
// lookups by box vnum and the GOLD/EXP/MOB/... sentinel "vnums".
enum class ContainerKind : std::uint8_t
{
    Use = 1,      // opened by using the item itself
    Locked = 2,   // opened by applying a key with the same lock id
    Scripted = 3, // no built-in contents; a quest/script decides
};
enum class LootPickMode : std::uint8_t
{
    WeightedOne = 1,       // exactly one entry, by relative weight
    IndependentChance = 2, // every entry rolls its own chance (bp)
};
enum class LootRewardKind : std::uint8_t
{
    Item = 1, Gold = 2, Experience = 3, SpawnMonster = 4, SpawnMonsterGroup = 5, Slow = 6, DrainHp = 7, Poison = 8,
};
inline constexpr std::size_t MaxLootEntries = 256;
inline constexpr std::uint32_t LootChanceScaleBp = 10'000;
inline constexpr std::uint64_t MaxLootCurrency = 9'223'372'036'854'775'807ULL;

struct LootEntry
{
    std::uint16_t entry = 0;       // 1..N, contiguous: the stable draw order
    LootRewardKind kind = LootRewardKind::Item;
    std::optional<ItemKey> item;   // present exactly for Item
    // Item: units; Gold/Experience: amount; SpawnMonster/Group: legacy mob/group
    // id (mobs are not catalog content yet); Slow: movement points for 300 s;
    // DrainHp: bp of max HP, never below 1 HP; Poison: 0.
    std::uint64_t amount = 0;
    std::uint32_t weight = 0;      // WeightedOne: relative weight; IndependentChance: bp chance
    std::uint16_t rareAttributeChanceBp = 0; // Item only
};
struct LootProfile
{
    std::uint32_t id = 0;
    std::string code;
    LootPickMode mode = LootPickMode::WeightedOne;
    std::vector<LootEntry> entries;
};
enum class LootProfileError
{
    InvalidIdentity, InvalidMode, EmptyProfile, TooManyEntries, InvalidEntryOrder,
    InvalidReward, InvalidAmount, InvalidWeight, InvalidRareChance, WeightOverflow,
};
class ValidatedLootProfile;
std::expected<ValidatedLootProfile, LootProfileError> ValidateLootProfile(LootProfile source);
class ValidatedLootProfile final
{
public:
    const LootProfile& Data() const noexcept { return data_; }
    // Sum of weights for WeightedOne (fits in uint32); entry count otherwise.
    std::uint32_t TotalWeight() const noexcept { return total_; }
private:
    ValidatedLootProfile(LootProfile source, std::uint32_t total) : data_(std::move(source)), total_(total) {}
    friend std::expected<ValidatedLootProfile, LootProfileError> ValidateLootProfile(LootProfile);
    LootProfile data_;
    std::uint32_t total_ = 0;
};

// Caller-owned uniform source: returns a value in [0, bound). The server RNG
// stays outside the domain, so a recorded draw sequence replays exactly.
using LootDraw = std::function<std::uint32_t(std::uint32_t bound)>;
enum class LootPickError { DrawOutOfRange };
// Returned indexes refer to Data().entries in draw order. IndependentChance may
// legitimately return nothing; WeightedOne always returns exactly one entry.
std::expected<std::vector<std::size_t>, LootPickError> PickLoot(const ValidatedLootProfile& profile, const LootDraw& draw);

class ItemCatalog;
struct ContainerOpenRequest
{
    ItemKey container;
    std::optional<ItemKey> key;       // the applied key item, if any
    std::uint32_t containerUnits = 0; // units in the selected stack
    std::uint32_t keyUnits = 0;
};
struct ContainerOpenPlan
{
    ItemKey container;
    std::optional<ItemKey> key;
    std::uint16_t containerUnitsToConsume = 0, keyUnitsToConsume = 0;
    std::vector<LootEntry> rewards; // owned copies, in draw order
};
enum class ContainerOpenError
{
    MissingItem, NotContainer, RetiredItem, ScriptedContainer, KeyRequired, KeyNotAccepted,
    NotKey, WrongKey, InsufficientUnits, DrawOutOfRange,
};
// Pure proposal. Nothing is consumed or granted here: the future writer must
// check capacity for every item reward and then commit consumption and all
// rewards together (no partial grant that leaves the box reusable).
std::expected<ContainerOpenPlan, ContainerOpenError> PlanContainerOpen(
    const ItemCatalog& catalog, const ContainerOpenRequest& request, const LootDraw& draw);
} // namespace auriga::item
