#pragma once

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <limits>

namespace ecs::item_attributes {

// Work on an attribute component (or a snapshot of it). Locked slots stay at
// their original index, including while the preceding slots are empty.
template <class Attributes>
int FindEmpty(const Attributes& attrs, int begin, int end, int locked = -1)
{
    for (int i = begin; i < end; ++i)
        if (i != locked && attrs[i].bType == 0)
            return i;
    return -1;
}

template <class Attributes>
int Count(const Attributes& attrs, int begin, int end)
{
    int count = 0;
    for (int i = begin; i < end; ++i)
        count += attrs[i].bType != 0;
    return count;
}

template <class Attributes>
bool Has(const Attributes& attrs, int begin, int end, uint32_t type)
{
    for (int i = begin; i < end; ++i)
        if (attrs[i].bType == type)
            return true;
    return false;
}

template <class Attributes>
void Clear(Attributes& attrs, int begin, int end, int locked = -1)
{
    for (int i = begin; i < end; ++i)
        if (i != locked)
            attrs[i] = {};
}

template <class Attributes>
bool RemoveType(Attributes& attrs, int begin, int end, uint32_t type, int locked = -1)
{
    bool removed = false;
    for (int i = begin; i < end; ++i) {
        if (i != locked && attrs[i].bType == type) {
            attrs[i] = {};
            removed = true;
        }
    }
    // Compact only unlocked slots; never shift a locked bonus or rare slots.
    if (removed) {
        int dest = begin;
        for (int src = begin; src < end; ++src) {
            if (src == locked || attrs[src].bType == 0)
                continue;
            while (dest == locked)
                ++dest;
            if (dest != src) {
                attrs[dest] = attrs[src];
                attrs[src] = {};
            }
            ++dest;
        }
    }
    return removed;
}

// Random uses the server's inclusive [low, high] convention. A malformed or
// empty table must never reach number(1, 0) or index bMaxLevelBySet[-1].
template <class TableMap, class Eligible, class Random>
auto Select(const TableMap& table, int set, bool weighted, Eligible eligible, Random random)
    -> typename TableMap::const_iterator
{
    uint64_t total = 0;
    auto weight = [&](const auto& entry) -> uint64_t {
        const auto& row = entry.second;
        if (set < 0 || static_cast<size_t>(set) >= std::size(row.bMaxLevelBySet) ||
            row.bMaxLevelBySet[set] == 0 || !eligible(entry.first, row))
            return 0;
        return weighted ? row.dwProb : 1;
    };
    for (const auto& entry : table)
        total += weight(entry);
    if (total == 0 || total > static_cast<uint64_t>(std::numeric_limits<int>::max()))
        return table.end();

    uint64_t roll = random(1, static_cast<int>(total));
    if (roll == 0 || roll > total)
        return table.end();
    for (auto it = table.begin(); it != table.end(); ++it) {
        const uint64_t value = weight(*it);
        if (value != 0 && roll <= value)
            return it;
        roll -= value;
    }
    return table.end();
}

template <class Probabilities, class Random>
int RollLevel(const Probabilities& probabilities, int levels, Random random)
{
    int total = 0;
    for (int i = 0; i < levels; ++i) {
        if (probabilities[i] < 0 || probabilities[i] > 100 - total)
            return 0;
        total += probabilities[i];
    }
    // Some changers intentionally leave part of the 1..100 range unassigned.
    // Preserve those odds; an uncovered roll fails without modifying the item.
    if (total == 0)
        return 0;
    int roll = random(1, 100);
    for (int i = 0; i < levels; ++i) {
        if (roll <= probabilities[i])
            return i + 1;
        roll -= probabilities[i];
    }
    return 0;
}

} // namespace ecs::item_attributes
