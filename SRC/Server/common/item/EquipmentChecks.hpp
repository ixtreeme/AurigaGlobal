#pragma once
#include "EquipmentStats.hpp"

namespace auriga::item
{
struct EquippedWeaponFacts
{
    std::uint64_t instance = 0;
    WeaponSubtype subtype;
};
inline std::optional<EquippedWeaponFacts> EquippedWeapon(const EquipmentStats* stats)
{
    if (!stats || !stats->weapon) return std::nullopt;
    for (const auto& item : stats->equipped)
        if (item.slot == static_cast<std::uint8_t>(EquipmentSlot::Weapon) &&
            item.key.Category() == ItemCategory::Weapon && item.id.Value() != 0)
            return EquippedWeaponFacts{item.id.Value(), *stats->weapon};
    return std::nullopt;
}
inline bool WeaponAllowedForJob(const EquippedWeaponFacts& weapon, std::uint8_t job)
{
    using W = WeaponSubtype;
    switch (job) {
        case 0: return weapon.subtype == W::Sword || weapon.subtype == W::TwoHanded;
        case 1: return weapon.subtype == W::Sword || weapon.subtype == W::Dagger || weapon.subtype == W::Bow;
        case 2: return weapon.subtype == W::Sword;
        case 3: return weapon.subtype == W::Bell || weapon.subtype == W::Fan;
        default: return false;
    }
}
// Preserve the normal horse's ceremonial body-armor restriction. Costume
// appearances and other armor do not inherit it through a model or name match.
inline bool BlocksHorseRiding(const ItemCatalog& catalog, const EquipmentStats& stats)
{
    for (const auto& item : stats.equipped) {
        if (item.slot != static_cast<std::uint8_t>(EquipmentSlot::Body)) continue;
        const auto* entry = catalog.Find(item.key);
        const auto* armor = entry ? std::get_if<ItemArmorRow>(&entry->category) : nullptr;
        if (!armor || armor->slot != EquipmentSlot::Body) continue;
        const auto& code = entry->base.contentCode;
        return code == "armor.szmoking" || code == "armor.szmoking_40abc1b8530c" ||
            code == "armor.menyasszonyi_ruha" || code == "armor.menyasszonyi_ruha_a5fdd09ff2b2";
    }
    return false;
}
} // namespace auriga::item
