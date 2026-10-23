#pragma once
#include <array>
#include <cstdint>

namespace character_creation {
inline constexpr uint8_t ChoicesMarker = 0xA5;
inline constexpr uint8_t StartLevel = 5;
inline constexpr std::array<uint32_t, 8> HairItemBases{73001,73251,73501,73751,75001,75201,75401,75601};
constexpr bool ValidHair(uint16_t appearance) {
    return appearance == 0 || (appearance / 10 >= 2 && appearance / 10 <= 4 && appearance % 10 >= 1 && appearance % 10 <= 4);
}
constexpr uint32_t HairItem(uint8_t race, uint16_t appearance) {
    if (race >= HairItemBases.size() || !ValidHair(appearance) || appearance == 0) return 0;
    return HairItemBases[race] + (appearance / 10 - 2) * 4 + appearance % 10 - 1;
}
constexpr uint32_t Weapon(uint8_t race, uint8_t group) {
    switch (race % 4) {case 0:return group == 2 ? 3009 : 19;case 1:return group == 2 ? 2009 : 1009;case 2:return 19;default:return group == 2 ? 5009 : 7009;}
}
constexpr uint32_t Armor(uint8_t race) {return 11209 + (race % 4) * 200;}
constexpr bool Male(uint8_t race) {return race == 0 || race == 2 || race == 5 || race == 7;}
constexpr uint32_t WeaponCostume(uint32_t weapon) {
    switch (weapon) {case 19:return 40129;case 1009:return 40130;case 2009:return 40131;case 3009:return 40132;case 5009:return 40133;case 7009:return 40134;default:return 0;}
}
}
