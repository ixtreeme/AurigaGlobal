#pragma once
#include "ItemKey.hpp"
#include "Containers.hpp"
#include <array>
#include <optional>
#include <string>
#include <vector>

namespace auriga::item
{
enum class ToolKind : std::uint8_t { Pick = 1, Rod = 2, Axe = 3, Shears = 4 };
enum class FishingPool : std::uint8_t { None = 1, First = 2, Second = 3 };
enum class ToolFailure : std::uint8_t { LosePoints = 1, ResetAtLevel = 2 };
struct ToolLevel
{
    std::uint16_t level = 0;
    std::uint32_t power = 0;
    std::uint16_t gatherBonusBp = 0;
    std::uint32_t requiredPoints = 0, practiceNumerator = 0, practiceDenominator = 1;
    std::uint16_t refineChanceBp = 0;
    ToolFailure failure = ToolFailure::LosePoints;
    std::optional<std::uint16_t> failureLevel;
    std::uint16_t failureLossBp = 0;
};
struct ToolProfile { std::uint32_t id = 0; std::string code; std::vector<ToolLevel> levels; };
struct ItemToolRow
{
    ItemKey key;
    ToolKind kind{};
    std::uint32_t profile = 0;
    FishingPool fishingPool = FishingPool::None;
};
bool ValidToolProfile(const ToolProfile&) noexcept;
enum class FishingKind : std::uint8_t { LiveFish = 1, DeadFish = 2, Bait = 3 };
struct ItemFishingRow
{
    ItemKey key;
    FishingKind kind{};
    std::optional<std::uint32_t> useLootProfile;
    std::optional<ItemKey> grillResult;
    std::uint32_t baitPower = 0;
};
bool ValidFishingItem(const ItemFishingRow&) noexcept;

// Owner-thread copies only. No socket sentinels or borrowed ECS handles.
struct GatheringItemSnapshot
{
    ItemKey item;
    std::uint64_t instanceId = 0, revision = 0, ownerId = 0;
    std::uint16_t refinementLevel = 0;
    std::uint32_t count = 0;
    bool equipped = false, locked = false, exchanging = false;
};
struct GatheringContext
{
    std::uint64_t actorId = 0, rulesetRevision = 0;
    std::array<std::uint8_t,16> operationId{};
    std::uint16_t characterLevel = 0;
    bool admittedByServer = false, inCombat = false;
};
struct GatheringPin { std::uint64_t revision = 0; std::array<std::uint8_t,32> hash{}; };
enum class GatheringError
{
    Forbidden, InvalidOperation, InvalidSnapshot, MissingItem, RetiredItem,
    InvalidStack, InvalidLevel, InvalidState, NotTool, NotFishingItem,
    WrongAction, AtMaximum, NotReady, InvalidDecision, CampfireRequired, FishingActive,
};
enum class ToolAction : std::uint8_t { Practice = 1, Refine = 2 };
struct ToolActionRequest
{
    GatheringContext context;
    GatheringItemSnapshot tool;
    ToolAction action{};
    std::uint32_t trainingPoints = 0, recordedRoll = 0;
};
struct ToolActionPlan
{
    ToolActionRequest expected;
    GatheringPin catalog;
    std::uint16_t afterLevel = 0;
    std::uint32_t afterPoints = 0;
    bool success = false;
};
class ItemCatalog;
std::expected<ToolActionPlan,GatheringError> PlanToolAction(const ItemCatalog&,const ToolActionRequest&);
enum class FishAction : std::uint8_t { Use = 1, Grill = 2 };
struct FishProcessRequest
{
    GatheringContext context;
    GatheringItemSnapshot fish;
    FishAction action{};
    bool campfireAvailable = false;
};
struct FishingYield { ItemKey item; std::uint32_t count = 0; };
struct FishProcessPlan
{
    FishProcessRequest expected;
    GatheringPin catalog;
    std::uint32_t consumeUnits = 0;
    std::vector<FishingYield> yields;
};
std::expected<FishProcessPlan,GatheringError> PlanFishProcess(const ItemCatalog&,const FishProcessRequest&,const LootDraw&);
struct BaitRequest
{
    GatheringContext context;
    GatheringItemSnapshot bait, rod;
    std::uint32_t previousBaitPower = 0;
    bool fishingActive = false;
};
struct BaitPlan { BaitRequest expected; GatheringPin catalog; std::uint32_t afterBaitPower = 0; std::uint16_t consumeUnits = 1; };
std::expected<BaitPlan,GatheringError> PlanApplyBait(const ItemCatalog&,const BaitRequest&);
// Pure owning plans. The owner admits resource/NPC/campfire/session and class/
// sex/location checks, rechecks exact revisions, reserves all output slots and
// atomically commits operation receipt + consumption + instance changes/grants.
// Grill yields preserve the complete count; split grants by target maxStack.
// CommitUnknown requires durable operation-ID reconciliation, never blind retry.
}
