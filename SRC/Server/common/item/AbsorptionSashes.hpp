#pragma once
#include "BonusProfiles.hpp"
#include "ItemKey.hpp"
#include <array>
#include <optional>

namespace auriga::item {
// Absorption sashes are independent of the six-bonus stole and rune clocks.
struct AbsorptionSashProfile {
    ItemKey key,next;
    std::uint8_t grade=0,initialMinimum=0,initialMaximum=0;
    // For grade four these are increments; otherwise the next grade's range.
    std::uint8_t combineMinimum=0,combineMaximum=0,maximumPercent=25;
    std::uint16_t successBp=0;
    std::uint64_t goldCost=0;
};
struct SashDonorProfile {
    ItemKey key;
    std::uint16_t refinement=0;
    std::int64_t physicalMaximumRaw=0,magicalMaximumRaw=0;
    bool physicalBasePositive=false,magicalBasePositive=false;
    // Preserve each original APPLY component before rounding. Combining equal
    // bonus IDs first can change the original integer absorption result.
    std::vector<BonusEntry> components;
};
struct AbsorbedSashDonor {
    ItemKey key;
    std::uint16_t refinement=0;
    std::array<std::optional<BonusEntry>,8> attributes{};
    bool operator==(const AbsorbedSashDonor&) const=default;
};
struct AbsorptionSashState {
    std::uint8_t percent=0;
    std::optional<AbsorbedSashDonor> donor;
    bool operator==(const AbsorptionSashState&) const=default;
};
bool ValidAbsorptionSashProfile(const AbsorptionSashProfile&);
bool ValidSashDonorProfile(const SashDonorProfile&);
bool ValidAbsorptionSashState(const AbsorptionSashProfile&,const AbsorptionSashState&);
AbsorptionSashState InitialAbsorptionSash(const AbsorptionSashProfile&,std::span<const std::uint8_t,16>,std::uint64_t instance);
std::int64_t AbsorbSashComponent(BonusEntry,std::uint8_t percent);
std::vector<BonusEntry> AbsorptionSashContributions(const SashDonorProfile&,const AbsorptionSashState&);
std::optional<std::vector<std::uint8_t>> EncodeAbsorptionSashState(const AbsorptionSashState&);
std::optional<AbsorptionSashState> DecodeAbsorptionSashState(std::span<const std::uint8_t>);
}
