#pragma once
#include <cstdint>
#include <string_view>

namespace auriga::item
{
// New domain codes. These are not legacy APPLY/POINT numeric values.
enum class BonusUnit : std::uint8_t { Raw = 1, BasisPoints = 2, Integer = 3 };
enum class BonusStage : std::uint8_t
{
    StatFlat = 1, StatPercent = 2, OutgoingAction = 3, OutgoingMode = 4,
    Penetration = 5, Resistance = 6, Critical = 7, IncomingAction = 8,
    IncomingMode = 9, Block = 10, Accuracy = 11, OutgoingGlobal = 12, Reward = 13,
};
enum class BonusTarget : std::uint8_t
{
    PhysicalAttack = 1, MagicalAttack = 2, PhysicalDefense = 3, MagicalDefense = 4,
    OutgoingDamage = 5, IncomingDamage = 6, FlatPenetration = 7, PercentPenetration = 8,
    Resistance = 9, CritChance = 10, CritResistance = 11, CritMultiplierExtra = 12,
    BlockReduction = 13, Evasion = 14, MovementSpeed = 15, AttackSpeed = 16, CastSpeed = 17,
    Strength = 18, Dexterity = 19, Constitution = 20, Intelligence = 21, Experience = 22,
};
enum class BonusChannel : std::uint8_t { Any = 1, Physical = 2, Magical = 3 };
enum class BonusAction : std::uint8_t { Any = 1, Normal = 2, Skill = 3 };
enum class BonusMode : std::uint8_t { Any = 1, Pve = 2, Pvp = 3 };
// Family/rank/element-specific bonuses require their own audited content registry.
// Zero means unrestricted, not an unknown family or an implicit lookup fallback.
enum class BonusFamily : std::uint16_t { Any = 0 };
enum class BonusRank : std::uint16_t { Any = 0 };
enum class BonusElement : std::uint8_t { Any = 0 };
enum class BonusStack : std::uint8_t { Add = 1, Max = 2, ReplaceByPriority = 3, UniqueSource = 4 };
// Individual records are rejected outside their definition's structural range.
// Aggregate caps are ruleset-owned and must be supplied by the future StatBuilder.
enum class BonusCap : std::uint8_t
{
    RawStat = 1, StatPercent = 2, Outgoing = 3, Incoming = 4, FlatPenetration = 5,
    PercentPenetration = 6, Resistance = 7, CritChance = 8, CritResistance = 9,
    CritMultiplierExtra = 10, Block = 11, Accuracy = 12, Speed = 13, Attribute = 14, Experience = 15,
};
enum class BonusSource : std::uint8_t { ItemBase = 1, Refinement = 2, TimedEffect = 3 };

struct BonusDefinition
{
    std::uint32_t id;
    std::string_view code, displayKey;
    BonusUnit unit;
    BonusTarget target;
    BonusStage stage;
    BonusChannel channel;
    BonusAction action;
    BonusMode mode;
    BonusFamily family;
    BonusRank rank;
    BonusElement element;
    BonusStack stacking;
    BonusCap aggregateCap;
    std::uint8_t allowedSources; // bits 0 ItemBase, 1 Refinement, 2 TimedEffect
    std::int64_t minimum, maximum;
};
constexpr bool AllowsBonusSource(const BonusDefinition& definition, BonusSource source) noexcept
{
    if (source != BonusSource::ItemBase && source != BonusSource::Refinement && source != BonusSource::TimedEffect) return false;
    return (definition.allowedSources & (1u << (static_cast<unsigned>(source) - 1))) != 0;
}
} // namespace auriga::item
