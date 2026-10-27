#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <common/length.h>
#include <common/retired_content.h>

namespace retired = auriga::retired_content;
namespace {
size_t checks = 0;
void Check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
void PersistedCodes() {
    // These are persisted/wire codes, not a compact enumeration to renumber.
    static_assert(ITEM_RETIRED_29 == 29 && ITEM_RETIRED_30 == 30 && ITEM_RETIRED_31 == 31);
    static_assert(RETIRED_WINDOW_6 == 6 && BELT_INVENTORY == 7);
    static_assert(USE_RETIRED_42 == 42);
    for (uint16_t subtype = 0; subtype <= UINT8_MAX; ++subtype) {
        for (uint8_t type : {29, 30, 31})
            Check(retired::Item(type, uint8_t(subtype)), "retired type admitted a subtype");
        Check(!retired::Item(ITEM_WEAPON, uint8_t(subtype)) &&
              !retired::Item(ITEM_ARMOR, uint8_t(subtype)), "ordinary gear was retired");
#ifdef ENABLE_SOUL_SYSTEM
        Check(!retired::Item(ITEM_SOUL, uint8_t(subtype)), "independent Soul feature was retired");
#endif
    }
    for (const auto [type, subtype] : std::array<std::pair<uint8_t, uint8_t>, 6>{{
        {ITEM_MATERIAL, 5}, {ITEM_MATERIAL, 6}, {ITEM_MATERIAL, 7},
        {ITEM_USE, 27}, {ITEM_USE, 28}, {ITEM_USE, 42}}})
        Check(retired::Item(type, subtype), "retired catalyst/charge admitted");
    Check(!retired::Item(ITEM_MATERIAL, 4) && !retired::Item(ITEM_MATERIAL, 8) &&
          !retired::Item(ITEM_USE, USE_POTION), "unrelated item behavior retired");
#ifdef ENABLE_STOLE_COSTUME
    Check(!retired::Item(ITEM_USE, USE_ENCHANT_STOLE), "stole enchantment was retired");
#endif
    for (uint32_t type : {540u, 541u, 542u, 563u, 564u, 565u, 566u})
        Check(retired::Affect(type), "retired persisted affect admitted");
    for (uint32_t type : {0u, 539u, 543u, 562u, 567u, UINT32_MAX})
        Check(!retired::Affect(type), "neighboring affect code retired");
}
void Positions() {
    for (unsigned cell = 0; cell <= UINT16_MAX; ++cell) {
        const auto at = uint16_t(cell);
        Check(!TItemPos(RETIRED_WINDOW_6, at).IsValidItemPosition(), "retired window admitted a position");
        const auto inMain = TItemPos(INVENTORY, at);
        const bool wear = cell >= INVENTORY_MAX_NUM && cell < INVENTORY_MAX_NUM + WEAR_MAX_NUM;
        const bool belt = cell >= BELT_INVENTORY_SLOT_START && cell < BELT_INVENTORY_SLOT_END;
        Check(inMain.IsValidItemPosition() == (cell < INVENTORY_MAX_NUM || wear || belt),
              "inventory/equipment/belt position contract changed");
        Check(inMain.IsEquipPosition() == wear, "retired wear slot accepted as equipment");
        Check(TItemPos(EQUIPMENT, at).IsValidItemPosition() == wear,
              "equipment window accepted a reserved slot");
        Check(TItemPos(BELT_INVENTORY, at).IsValidItemPosition() == belt,
              "belt range changed");
    }
}
}
int main() {
    try {
        PersistedCodes(); Positions();
        std::cout << "Retired content: " << checks << " checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
