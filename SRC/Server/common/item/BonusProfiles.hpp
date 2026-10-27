#pragma once
#include "BonusRegistry.hpp"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace auriga::item
{
struct BonusEntry
{
    BonusId bonus{};
    std::int64_t valueScaled = 0; // Unit comes exclusively from BonusDefinition.
    bool operator==(const BonusEntry&) const = default;
};
struct BonusProfile
{
    std::uint32_t id = 0;
    std::string code;
    BonusSource source = BonusSource::ItemBase;
    std::vector<BonusEntry> entries;
};
inline constexpr std::size_t MaxBonusEntriesPerProfile = 128;
inline constexpr std::size_t MaxBonusProfiles = 4096;
enum class BonusProfileError
{
    InvalidIdentity, InvalidSource, EmptyProfile, TooManyEntries, UnknownBonus,
    DuplicateBonus, ValueOutOfRange, SourceNotAllowed,
};
class ValidatedBonusProfile;
std::expected<ValidatedBonusProfile, BonusProfileError> ValidateBonusProfile(BonusProfile source);
class ValidatedBonusProfile final
{
public:
    const BonusProfile& Data() const noexcept { return data_; }
private:
    explicit ValidatedBonusProfile(BonusProfile source) : data_(std::move(source)) {}
    friend std::expected<ValidatedBonusProfile, BonusProfileError> ValidateBonusProfile(BonusProfile);
    BonusProfile data_;
};
bool AcceptsBonusSource(const ValidatedBonusProfile& profile, BonusSource source) noexcept;
// Context lookup over validated, owning profiles. An absent context never means
// an unknown profile is valid. The catalog supplies the complete validated set.
const ValidatedBonusProfile* FindBonusProfile(std::span<const ValidatedBonusProfile> profiles, std::uint32_t id) noexcept;
} // namespace auriga::item
