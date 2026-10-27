#pragma once
#include "MissionRewardBinding.hpp"

namespace auriga::mission
{
enum class ResourceWindow : uint8_t
{
    Inventory = 1,
    ExtraInventory,
    Equipment,
    Reserved4, // Retired storage code; never reuse.
    Belt,
    Safebox,
    Mall,
    Ground
};
enum class ResourceItemFlag : uint8_t
{
    Locked = 1,
    Exchanging = 2,
    Sealed = 4,
    PendingDelete = 8
};
struct ResourceItem
{
    uint32_t id = 0, owner = 0, vnum = 0, count = 0;
    ResourceWindow window = ResourceWindow::Inventory;
    uint16_t category = 0, cell = 0;
    uint8_t flags = 0;
    bool operator==(const ResourceItem &) const = default;
};
struct ResourceProgress
{
    uint16_t level = 1;
    uint8_t step = 0;
    uint32_t experience = 0;
    uint16_t stat = 0, skill = 0, subSkill = 0, randomHP = 0, randomSP = 0;
    uint8_t job = 0, skillGroup = 0;
    bool experienceBlocked = false;
    bool operator==(const ResourceProgress &) const = default;
};
struct ResourceSnapshot
{
    CharacterId character = 0;
    uint64_t revision = 0; // Durable resource revision, NOT Mission storage revision.
    ResourceProgress progress;
    int64_t gold = 0;
    std::vector<ResourceItem> items;
    bool operator==(const ResourceSnapshot &) const = default;
};
struct ResourceItemRule
{
    uint32_t maximumGrant = 0, maximumConsume = 0;
    bool operator==(const ResourceItemRule &) const = default;
};
struct ResourceRules
{
    uint32_t version = 1;
    uint16_t maximumLevel = 250, lastStatLevel = 115, statCap = 344;
    uint8_t lastStatStep = 2, job = 0;
    uint16_t hpMinimum = 0, hpMaximum = 0, spMinimum = 0, spMaximum = 0;
    int64_t goldExclusiveLimit = 999000000000LL;
    bool forceExperience = false;
    // Index = level. Entry zero is zero; final entry is unused. Actual live
    // exp_table/config/prototype data must be copied by the trusted adapter.
    std::vector<uint32_t> nextExperience;
    std::map<uint32_t, ResourceItemRule> items;
    bool operator==(const ResourceRules &) const = default;
};
struct ResourceLevelRoll
{
    uint16_t fromLevel = 0, hp = 0, sp = 0;
    bool operator==(const ResourceLevelRoll &) const = default;
};
struct ResourceConsumption
{
    uint32_t id = 0, vnum = 0, before = 0, consumed = 0, after = 0;
    bool operator==(const ResourceConsumption &) const = default;
};
struct ResourcePreparation;
class PreparedResources final
{
  public:
    static constexpr size_t MaximumItems = 1024, MaximumBytes = 2 * 1024 * 1024;
    PreparedResources(const PreparedResources &) = default;
    PreparedResources(PreparedResources &&) = default;
    PreparedResources &operator=(const PreparedResources &) = default;
    PreparedResources &operator=(PreparedResources &&) = default;
    const ActionKey &Action() const
    {
        return action_;
    }
    const ContentDigest &DefinitionDigest() const
    {
        return definition_;
    }
    const ContentDigest &PayloadDigest() const
    {
        return payload_;
    }
    const ContentDigest &Digest() const
    {
        return digest_;
    }
    const ResourceSnapshot &Before() const
    {
        return before_;
    }
    const ResourceSnapshot &After() const
    {
        return after_;
    }
    const ResourceRules &Rules() const
    {
        return rules_;
    }
    const std::vector<ResourceLevelRoll> &LevelRolls() const
    {
        return rolls_;
    }
    const std::vector<ResourceConsumption> &Consumption() const
    {
        return consumption_;
    }
    const std::vector<uint8_t> &Bytes() const
    {
        return bytes_;
    }
    bool RefillVitals() const
    {
        return refillVitals_;
    }

  private:
    PreparedResources() = default;
    friend ResourcePreparation PrepareResources(const PinnedReward &, ResourceSnapshot, ResourceRules,
                                                std::vector<ResourceLevelRoll>);
    ActionKey action_;
    ContentDigest definition_{}, payload_{}, digest_{};
    ResourceSnapshot before_, after_;
    ResourceRules rules_;
    std::vector<ResourceLevelRoll> rolls_;
    std::vector<ResourceConsumption> consumption_;
    bool refillVitals_ = false;
    std::vector<uint8_t> bytes_;
};
struct ResourcePreparation
{
    std::optional<PreparedResources> plan;
    std::vector<Diagnostic> errors;
    explicit operator bool() const
    {
        return plan.has_value() && errors.empty();
    }
};
// Pure bounded decision. No ECS, RNG calls, SQL writes or inventory publication.
// Costs use pre-existing eligible stacks, never grants in this bundle. Every
// level-up roll is an explicit, archived input. Any failure rejects everything.
ResourcePreparation PrepareResources(const PinnedReward &reward, ResourceSnapshot before, ResourceRules rules,
                                     std::vector<ResourceLevelRoll> rolls = {});
// Canonical, hashed, bounded envelope. Recomputes the complete decision from
// archived inputs and the exact reward pin; a digest alone is not authorization.
ResourcePreparation DecodeResources(std::span<const uint8_t> bytes, const PinnedReward &reward);

struct ResourceEffectReceipt
{
    ContentDigest action{}, decision{};
    uint64_t appliedRevision = 0;
};
enum class ResourceApplication
{
    Ready,
    AlreadyApplied,
    Reconcile,
    Stale,
    Invalid
};
// Receipt must come from the authoritative durable resource writer. An equal
// after-image WITHOUT its receipt is ambiguous, never proof of application.
ResourceApplication InspectResourceApplication(const PreparedResources &plan, ResourceSnapshot current,
                                               std::optional<ResourceEffectReceipt> receipt = {});
} // namespace auriga::mission
