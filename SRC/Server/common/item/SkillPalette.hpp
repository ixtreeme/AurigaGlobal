#pragma once

#include "PersistentItems.hpp"
#include <algorithm>

namespace auriga::item
{
inline constexpr auto RainbowElixir = *ItemKey::Create(10, 1485);
inline constexpr std::uint16_t SkillPaletteOutcome = 0x4311;
using SkillLayers = std::array<std::uint32_t, 5>;
using PersonalSkillPalette = std::array<std::optional<SkillLayers>, 6>;

struct SkillPaletteEdit
{
    std::uint8_t slot;
    SkillLayers layers;
    bool CostsElixir() const { return layers != SkillLayers{}; }
};

inline std::vector<std::uint8_t> EncodeSkillPalette(const SkillPaletteEdit& edit)
{
    std::vector<std::uint8_t> bytes{edit.slot};
    for (auto layer : edit.layers)
        for (unsigned bit = 0; bit < 32; bit += 8)
            bytes.push_back(static_cast<std::uint8_t>(layer >> bit));
    return bytes;
}

inline std::optional<SkillPaletteEdit> DecodeSkillPalette(const ItemCommit& commit)
{
    if (commit.outcomeCode != SkillPaletteOutcome || commit.kind != ItemOperationKind::Service ||
        commit.owners.size() != 1 || commit.owners.front().owner.kind != ItemOwnerKind::Character ||
        !commit.wallets.empty() || commit.result.size() != 21 || commit.result.front() >= PersonalSkillPalette{}.size())
        return {};
    SkillPaletteEdit edit{commit.result.front(), {}};
    std::size_t offset = 1;
    for (auto& layer : edit.layers)
        for (unsigned bit = 0; bit < 32; bit += 8)
            layer |= static_cast<std::uint32_t>(commit.result[offset++]) << bit;
    if (edit.CostsElixir())
    {
        if (commit.items.size() != 1 || !commit.items.front().before) return {};
        const auto& before = *commit.items.front().before;
        if (before.key != RainbowElixir || before.refinementLevel != 0 || before.count == 0 ||
            (!PlainInventoryItem(before, commit.owners.front().owner) &&
             !PlainInventoryItem(before, commit.owners.front().owner, ItemStorageKind::SpecialInventory))) return {};
        auto expected = before;
        ++expected.revision;
        if (--expected.count == 0) { expected.destroyed = true; expected.placement.reset(); }
        if (expected != commit.items.front().after) return {};
    }
    else if (!commit.items.empty()) return {};
    return edit;
}
}
