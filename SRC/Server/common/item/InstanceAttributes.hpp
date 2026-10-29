#pragma once
#include "BonusProfiles.hpp"
#include "SocketServices.hpp"
#include <span>

namespace auriga::item
{
enum class AttributePool : std::uint8_t { Normal=1, Rare=2, Extra=3 };
struct AttributeGroup { AttributePool pool{};std::uint8_t capacity=0;bool excludeStatic=false; };
struct AttributeValue { std::uint8_t tier=0;std::int64_t value=0; };
struct AttributeCandidate
{
    AttributePool pool{};BonusId bonus{};std::uint32_t weight=0;
    std::vector<AttributeValue> values;
};
struct AttributeProfile
{
    std::uint32_t id=0;std::string code;bool allowLock=false;
    std::vector<AttributeGroup> groups;
    std::vector<AttributeCandidate> candidates;
};
struct ItemAttributeBinding {ItemKey key;std::uint32_t profile=0;};
enum class AttributeOperation : std::uint8_t { Add=1,Reroll=2,Lock=3,Relock=4,Unlock=5 };
struct AttributeTierChance {std::uint8_t tier=0;std::uint16_t chanceBp=0;};
struct AttributeServiceProfile
{
    std::uint32_t id=0;std::string code;AttributeOperation operation{};AttributePool pool{};
    std::uint16_t units=0,successBp=0;
    std::uint8_t targetMask=0,minimumCount=0,maximumCount=0,addCount=0;
    std::uint16_t maximumTargetLevel=0; // 0 = no minimum-level ceiling on the target prototype.
    bool consumeOnFailure=false;
    std::vector<AttributeTierChance> tiers;
};
inline constexpr std::size_t AttributeSlots=8,MaxAttributeCandidates=128,MaxAttributeTiers=16;
struct InstanceAttributes
{
    std::array<std::optional<BonusEntry>,AttributeSlots> slots{};
    std::optional<std::uint8_t> lockedSlot;
    bool operator==(const InstanceAttributes&) const = default;
};
bool ValidAttributeProfile(const AttributeProfile&);
bool ValidAttributeService(const AttributeServiceProfile&) noexcept;
struct AttributeDraw {std::uint16_t tierRoll=0;std::uint32_t bonusRoll=0;};
struct AttributeRequest
{
    SocketItemPair items;
    InstanceAttributes state;
    std::uint16_t successRoll=0;
    std::uint8_t lockRoll=0; // Inclusive 1..eligible occupied slots; 0 for non-lock operations.
    std::vector<AttributeDraw> draws;
};
struct AttributePlan
{
    AttributeRequest expected;
    std::uint64_t catalogRevision=0;
    std::array<std::uint8_t,32> catalogHash{};
    InstanceAttributes after;
    std::uint16_t consumeUnits=0;
    bool succeeded=false;
};
enum class AttributeError
{
    AdmissionDenied,WrongSource,IncompatibleTarget,InvalidState,InvalidDecision,
    NoEffect,NotEnoughAttributes,AlreadyLocked,NotLocked,NoAlternative,
    NoCandidate,UnassignedTier,InsufficientUnits,TargetDenied,InvalidPayload,
};
class ItemCatalog;
std::expected<void,AttributeError> ValidateInstanceAttributes(const ItemCatalog&,ItemKey,const InstanceAttributes&);
std::expected<AttributePlan,AttributeError> PlanAttributeService(const ItemCatalog&,const AttributeRequest&);
// A complete source replacement, not summed/cached character stats. Retain the
// exact duplicate IDs from different pools as separate additive contributions.
std::expected<std::vector<BonusEntry>,AttributeError> InstanceAttributeContributions(const ItemCatalog&,ItemKey,const InstanceAttributes&);
std::expected<std::vector<std::uint8_t>,AttributeError> EncodeInstanceAttributes(const ItemCatalog&,ItemKey,const InstanceAttributes&);
std::expected<InstanceAttributes,AttributeError> DecodeInstanceAttributes(const ItemCatalog&,ItemKey,std::span<const std::uint8_t>);
} // namespace auriga::item
