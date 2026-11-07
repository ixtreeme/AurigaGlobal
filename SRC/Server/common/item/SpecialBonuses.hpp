#pragma once
#include "BonusProfiles.hpp"
#include "ItemKey.hpp"
#include <array>
#include <optional>

namespace auriga::item {
// Stoles are the six-bonus costume, not the independent absorption sash.
enum class RegaliaKind : std::uint8_t { Stole=1,Rune=2 };
struct RegaliaRule {
    std::uint8_t index=0,variant=0;
    std::uint16_t minimumChargeBp=0;
    BonusEntry bonus;
};
struct ItemRegaliaRow {
    ItemKey key;
    RegaliaKind kind{};
    std::uint8_t slot=0,grade=0;
    std::uint64_t capacityMs=0,initialMs=0;
    std::vector<RegaliaRule> rules;
};
enum class SpecialBonusOperation : std::uint8_t {
    StoleReroll=1,RuneCharge=2,CostumeReset=3,CostumeGrant=4,CostumeRemove=5,PairedAddon=6,
};
struct SpecialBonusTier {std::uint8_t draw=0,tier=0;std::uint16_t chanceBp=0;};
struct ItemBonusToolRow {
    ItemKey key;
    SpecialBonusOperation operation{};
    std::uint16_t units=0,parameter=0;
    std::optional<BonusEntry> payload;
    std::vector<SpecialBonusTier> tiers;
};
struct RegaliaState {
    std::array<std::optional<BonusEntry>,6> bonuses{};
    std::uint64_t capacityMs=0,remainingMs=0;
    bool active=false;
    bool operator==(const RegaliaState&) const=default;
};
struct BonusToolState {
    std::optional<BonusEntry> payload;
    std::uint16_t remainingBp=0;
    bool operator==(const BonusToolState&) const=default;
};
bool ValidRegalia(const ItemRegaliaRow&);
bool ValidBonusTool(const ItemBonusToolRow&);
std::uint32_t RegaliaSlot(const ItemRegaliaRow&);
bool ValidRegaliaState(const ItemRegaliaRow&,const RegaliaState&);
bool ValidBonusToolState(const ItemBonusToolRow&,const BonusToolState&);
RegaliaState InitialRegalia(const ItemRegaliaRow&,std::span<const std::uint8_t,16> entropy,std::uint64_t instance);
BonusToolState InitialBonusTool(const ItemBonusToolRow&);
RegaliaState AdvanceRegalia(const ItemRegaliaRow&,RegaliaState,std::uint64_t elapsed);
std::vector<BonusEntry> RegaliaContributions(const ItemRegaliaRow&,const RegaliaState&);
std::vector<std::uint8_t> EncodeRegaliaState(const RegaliaState&);
std::optional<RegaliaState> DecodeRegaliaState(std::span<const std::uint8_t>);
std::vector<std::uint8_t> EncodeBonusToolState(const BonusToolState&);
std::optional<BonusToolState> DecodeBonusToolState(std::span<const std::uint8_t>);
}
