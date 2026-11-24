#pragma once

#include "EquipmentStats.hpp"
#include <algorithm>
#include <string_view>

namespace auriga::item
{
// Values used by the element byte in GC_TARGET.
enum class TargetElement : std::uint8_t { None, Lightning, Fire, Ice, Wind, Earth, Dark };

inline TargetElement EquippedPendantElement(const ItemCatalog& catalog, const EquipmentStats* stats)
{
    if (!stats) return TargetElement::None;
    const auto pendant = std::ranges::find(stats->equipped, static_cast<std::uint8_t>(EquipmentSlot::Pendant), &EquippedStatItem::slot);
    if (pendant == stats->equipped.end()) return TargetElement::None;
    const auto* definition = catalog.Find(pendant->key);
    if (!definition) return TargetElement::None;

    // Each imported family has multiple catalog definitions and refinement curves.
    // The stable content-code prefix also identifies its unrefined, zero-bonus form.
    const std::string_view code = definition->base.contentCode;
    constexpr std::pair<std::string_view, TargetElement> families[] = {
        {"catalog.2.lightning_talisman_", TargetElement::Lightning},
        {"catalog.2.fire_talisman_", TargetElement::Fire},
        {"catalog.2.ice_talisman_", TargetElement::Ice},
        {"catalog.2.wind_talisman_", TargetElement::Wind},
        {"catalog.2.earth_talisman_", TargetElement::Earth},
        {"catalog.2.dark_talisman_", TargetElement::Dark},
    };
    for (const auto& [prefix, element] : families)
        if (code.starts_with(prefix)) return element;
    return TargetElement::None;
}
}
