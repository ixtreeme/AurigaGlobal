#pragma once
#include "ItemKey.hpp"
#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace auriga::item
{
// Independent red/blue Soul system. No DragonSoul grade/stage/slot namespace.
enum class SoulKind : std::uint8_t { Core=1, RefinementScroll=2 };
enum class SoulChannel : std::uint8_t { NormalAttack=1, Skill=2 };
struct SoulLevel
{
    std::uint16_t level=0;
    std::uint32_t chargeCapacity=0, cycleChargeCost=0, strikesPerCycle=0;
    std::uint32_t initialCharge=0, initialStrikes=0;
    std::uint64_t rechargeIntervalMs=0; // One charge unit per elapsed owner-clock interval.
    std::uint32_t firstMultiplierBp=0, secondMultiplierBp=0, thirdMultiplierBp=0;
};
struct SoulProfile { std::uint32_t id=0;std::string code;std::vector<SoulLevel> levels; };
bool ValidSoulProfile(const SoulProfile&) noexcept;
struct ItemSoulRow
{
    ItemKey key;
    SoulKind kind{};
    std::optional<SoulChannel> channel;
    std::optional<std::uint32_t> profile;
    std::uint16_t refineChanceBp=0; // Scroll only; consumes one on either outcome.
};
struct SoulState
{
    std::uint32_t charge=0,strikes=0;
    std::uint64_t rechargeRemainderMs=0;
    bool active=false;
    bool operator==(const SoulState&) const = default;
};
struct SoulScrollSnapshot
{
    ItemKey key;
    std::uint64_t instanceId=0,revision=0,ownerId=0;
    std::uint32_t count=0;
    bool locked=false;
};
enum class SoulAction : std::uint8_t { Activate=1, Deactivate=2, Recharge=3, Strike=4, Refine=5 };
struct SoulRequest
{
    ItemKey key;
    SoulState state;
    std::uint64_t instanceId=0,revision=0,ownerId=0,actorId=0,rulesetRevision=0;
    std::array<std::uint8_t,16> operationId{};
    std::uint32_t count=0;
    std::uint16_t refinementLevel=0,characterLevel=0,recordedRoll=0;
    std::uint8_t characterClass=0,sex=0;
    SoulAction action{};
    SoulChannel damageChannel{};
    std::uint64_t damageEventId=0,targetId=0,targetRevision=0;
    std::uint64_t elapsedOwnerMs=0;
    std::int64_t inputDamageRaw=0;
    std::optional<SoulScrollSnapshot> scroll;
    bool admittedByServer=false,lockedByOther=false,exchanging=false,equipped=false,inCombat=false;
    bool actorIsPlayer=false,targetIsNpc=false,polymorphed=false,channelOccupiedByOther=false;
};
struct SoulPlan
{
    SoulRequest expected;
    SoulState after;
    std::uint16_t afterLevel=0;
    std::uint32_t consumeScrollUnits=0,multiplierBp=0;
    std::int64_t outputDamageRaw=0,additionalDamageRaw=0;
    bool success=false;
    std::uint64_t catalogRevision=0;
    std::array<std::uint8_t,32> catalogHash{};
};
enum class SoulError
{
    Forbidden,InvalidOperation,InvalidSnapshot,MissingItem,NotCore,RetiredItem,InvalidLevel,
    InvalidState,InvalidAction,InvalidDecision,NotReady,AlreadyActive,NotActive,ChannelOccupied,
    InvalidDamage,DamageOverflow,IneligibleTarget,MissingScroll,InvalidScroll,AtMaximum,
};
class ItemCatalog;
std::expected<SoulState,SoulError> MakeSoulState(const ItemCatalog&,ItemKey,std::uint16_t level);
// Pure owner-copied plan. Owner serializes recharge/toggle/strike/refine with
// combat commit, rechecks actor/target/catalog pins and exact instance revisions,
// then receipts the operation atomically with item/scroll/damage changes.
// No offline recharge is inferred, no RNG/SQL/ECS access or callbacks here.
std::expected<SoulPlan,SoulError> PlanSoul(const ItemCatalog&,const SoulRequest&);
}
