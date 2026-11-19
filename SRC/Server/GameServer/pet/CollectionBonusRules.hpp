#pragma once
#include <cmath>
#include <cstdint>
#include <map>
#include <set>

namespace CollectionBonuses {
struct Bonus {
    double items = 0;
    double perTier = 0;
    double Total(unsigned tiers) const { return items + perTier * tiers; }
};
struct Totals {
    explicit Totals(uint8_t storageWindow) : window(storageWindow), threshold(storageWindow == 5 ? 10 : 5) {
        if (window == 5) {
            bonuses[1].perTier = 500;
            for (uint8_t type : {116,117,119,63}) bonuses[type].perTier = 2;
        } else if (window == 14) {
            for (uint8_t type : {103,74,73}) bonuses[type].perTier = .2;
        }
    }
    uint8_t window;
    uint8_t threshold;
    std::set<uint32_t> items;
    std::map<uint8_t,Bonus> bonuses;
    unsigned Count() const { return static_cast<unsigned>(items.size()); }
    unsigned Tiers() const { return Count() / threshold; }
    bool AddItem(uint32_t vnum) { return vnum != 0 && items.insert(vnum).second; }
    void AddApply(uint8_t type, double value) {
        if (type && std::isfinite(value) && value != 0) bonuses[type].items += value;
    }
};
}
