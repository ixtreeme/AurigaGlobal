#pragma once
#include "ItemKey.hpp"
#include <cstdint>
#include <expected>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace auriga::item
{
// Monster drop tables (F4 C7b, IC-035). They replace mob_drop_item.json,
// drop_item_group.json and common_drop_item.json; only the offline converter
// knows those. Mobs are not catalog content yet, so a group names a legacy mob
// vnum, or a mob rank for the common drop.
enum class MobDropTarget : std::uint8_t
{
    Mob = 1,  // one monster race (legacy mob vnum)
    Rank = 2, // every monster of a rank, never a metin stone (the common drop)
};
enum class MobDropRoll : std::uint8_t
{
    EachEntry = 1,  // every entry rolls its own chance (legacy "drop")
    OneInKills = 2, // one weighted entry once in N kills (legacy "kill")
    LevelGated = 3, // unscaled chances above a killer level (legacy "limit")
    DoubleDrop = 4, // like EachEntry, only with a double drop (legacy "thiefgloves")
};
inline constexpr std::size_t MaxMobDropEntries = 256;
inline constexpr std::size_t MaxMobDropGroups = 100'000;
inline constexpr std::uint8_t MaxMobRank = 5;
// Chances are parts per billion per kill at neutral modifiers.
inline constexpr std::uint32_t MobDropChanceScale = 1'000'000'000;

struct MobDropEntry
{
    std::uint16_t entry = 0;           // 1..N, contiguous: the stable roll order
    ItemKey item;
    std::uint16_t refinementLevel = 0;
    std::uint32_t amount = 1;
    std::uint32_t chancePpb = 0;       // EachEntry, LevelGated, DoubleDrop
    std::uint32_t weight = 0;          // OneInKills
    std::uint16_t rareAttributeChanceBp = 0;
    std::uint16_t killerLevelMin = 1, killerLevelMax = 65535; // inclusive
};
struct MobDropGroup
{
    std::uint32_t id = 0;
    std::string code;
    MobDropTarget target = MobDropTarget::Mob;
    std::uint32_t targetValue = 0;     // Mob: legacy vnum (> 0); Rank: 0..5
    MobDropRoll roll = MobDropRoll::EachEntry;
    std::uint32_t killsPerDrop = 0;    // OneInKills only
    std::uint16_t minimumKillerLevel = 0; // LevelGated only
    std::vector<MobDropEntry> entries;
};
enum class MobDropGroupError
{
    InvalidIdentity, InvalidTarget, InvalidRoll, EmptyGroup, TooManyEntries, InvalidEntryOrder,
    InvalidItem, InvalidLevel, InvalidAmount, InvalidChance, InvalidWeight, WeightOverflow,
    InvalidRareChance, InvalidKillerLevels,
};
std::string_view MobDropGroupErrorName(MobDropGroupError) noexcept;
class ItemCatalog;
// Sorts the entries by number and checks the group on its own, then against
// the catalog (live items, stack size, refinement level). Never repairs.
std::expected<void, MobDropGroupError> ValidateMobDropGroup(MobDropGroup&);
std::expected<void, MobDropGroupError> ValidateMobDropItems(const MobDropGroup&, const ItemCatalog&);

// The facts of one kill, from the server's legacy drop rules (GetDropPct).
struct MobDropContext
{
    std::uint32_t mobVnum = 0;
    std::uint8_t mobRank = 0;
    bool stone = false;
    std::uint16_t killerLevel = 0;
    std::int64_t deltaPercent = 100;     // level difference and drop bonuses, percent
    std::uint32_t randomRange = 4'000'000; // after privileges and events
    bool doubleDrop = false;             // premium, gloves, APPLY_DOUBLE_DROP_ITEM
};
// Caller-owned uniform source: a value in [1, bound], like the legacy
// number(1, bound). A recorded sequence replays exactly.
using MobDropDraw = std::function<std::uint32_t(std::uint32_t bound)>;
struct MobDrop
{
    std::uint32_t group = 0;
    std::uint16_t entry = 0;
    ItemKey item;
    std::uint16_t refinementLevel = 0;
    std::uint32_t amount = 0;
    std::uint16_t rareAttributeChanceBp = 0;
};
enum class MobDropError { InvalidContext, DrawOutOfRange };
// Every group of the mob's rank (not for stones) and of the mob itself, in
// group order; see IC-035 for each roll's legacy rule. Nothing is granted here.
std::expected<std::vector<MobDrop>, MobDropError> PlanMobDrops(const ItemCatalog&, const MobDropContext&, const MobDropDraw&);
} // namespace auriga::item
