#pragma once
#include "ItemKey.hpp"
#include <cstdint>
#include <vector>
namespace auriga::item {
enum class ForgeAidKind : std::uint8_t { Scroll=1, Booster=2 };
enum class ForgeChance : std::uint8_t { Recipe=1, Replace=2, Add=3, Levels=4 };
enum class ForgeFailure : std::uint8_t { Recipe=0, Keep=1, Destroy=2, Downgrade=3 };
struct ForgeChanceLevel {std::uint16_t level=0,chanceBp=0;bool operator==(const ForgeChanceLevel&) const=default;};
struct ItemForgeAidRow {
    ItemKey key;
    ForgeAidKind kind=ForgeAidKind::Scroll;
    std::uint16_t units=1;
    std::uint8_t targetMask=7; // weapon, armor, accessory, insertable stone: bits 0..3
    std::uint16_t minimumLevel=0,maximumLevel=255;
    ForgeChance chanceMode=ForgeChance::Recipe;
    std::uint16_t chanceBp=0;
    ForgeFailure failure=ForgeFailure::Downgrade;
    std::uint8_t boosterGroup=0; // scroll 0; boosters 1..3, one of each per attempt
    std::vector<ForgeChanceLevel> levels;
};
bool ValidForgeAid(const ItemForgeAidRow&);
}
