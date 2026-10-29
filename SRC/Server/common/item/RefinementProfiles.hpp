#pragma once
#include "BonusProfiles.hpp"
#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace auriga::item
{
inline constexpr std::int64_t ItemStatRawPerPoint = 1'000;
inline constexpr std::int64_t MaxItemStatRaw = 1'000'000'000'000'000LL;

struct ItemBaseStats
{
    std::int64_t physicalAttackRaw = 0;
    std::int64_t magicalAttackRaw = 0;
    std::int64_t physicalDefenseRaw = 0;
    std::int64_t magicalDefenseRaw = 0;
    constexpr auto Values() const noexcept
    {
        return std::array{physicalAttackRaw, magicalAttackRaw, physicalDefenseRaw, magicalDefenseRaw};
    }
    bool operator==(const ItemBaseStats&) const = default;
};

enum class RefinementKind : std::uint8_t { None = 1, Weapon = 2, Armor = 3, Accessory = 4, SocketStone = 5, Costume = 6, Tool = 7, Soul = 8 };
struct RefinementLevel
{
    std::uint16_t level = 0;
    // Cumulative gains relative to level zero, not a delta from the prior step.
    ItemBaseStats gain;
    std::optional<std::uint32_t> extraBonusProfile;
};
struct RefinementProfile
{
    std::uint32_t id = 0;
    std::string code;
    RefinementKind kind = RefinementKind::None;
    std::uint16_t maxLevel = 0;
    std::vector<RefinementLevel> levels;
};
enum class RefinementError
{
    InvalidIdentity, InvalidKind, InvalidLevelRange, IncompleteLevels,
    InvalidGain, NonzeroLevelZero, NonMonotonicGain, WrongStatKind,
    MissingBonusProfile, WrongBonusSource, InvalidBonusContext, StatOutOfRange, AtMaximumLevel,
};

class ValidatedRefinementProfile;
std::expected<ValidatedRefinementProfile, RefinementError> ValidateRefinementProfile(
    RefinementProfile source, std::span<const ValidatedBonusProfile> bonuses = {});

class ValidatedRefinementProfile
{
public:
    const RefinementProfile& Data() const noexcept { return data_; }
    const RefinementLevel* FindLevel(std::uint16_t level) const noexcept
    {
        return level <= data_.maxLevel && level < data_.levels.size() ? &data_.levels[level] : nullptr;
    }
private:
    explicit ValidatedRefinementProfile(RefinementProfile data) : data_(std::move(data)) {}
    friend std::expected<ValidatedRefinementProfile, RefinementError> ValidateRefinementProfile(
        RefinementProfile, std::span<const ValidatedBonusProfile>);
    RefinementProfile data_;
};

// Adds only the prototype base and refinement gain. Equipment bonus/buff
// contributions belong to the future StatBuilder, not a second application here.
std::expected<ItemBaseStats, RefinementError> ResolveRefinedBaseStats(
    const ItemBaseStats& base, const ValidatedRefinementProfile& profile, std::uint16_t level) noexcept;

// The configured profile owns the gameplay ceiling. Crossing +9 requires no
// prototype change. This resolves a level only; resource/instance commit is a
// separate authoritative domain operation.
std::expected<std::uint16_t, RefinementError> NextRefinementLevel(
    const ValidatedRefinementProfile& profile, std::uint16_t currentLevel) noexcept;
} // namespace auriga::item
