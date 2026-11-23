#pragma once

#include "ItemMissions.hpp"
#include <algorithm>
#include <bit>
#include <cmath>

namespace auriga::item
{
inline constexpr std::uint16_t GuildConstructionOutcome = 0x4211;

// Existing world data stores these three material references as numbers.
// Convert at that boundary; the economy only sees catalog keys.
inline constexpr auto GuildConstructionMaterials = std::to_array<std::pair<std::uint32_t, ItemKey>>({
    {90010, *ItemKey::Create(6, 755)}, {90011, *ItemKey::Create(6, 756)},
    {90012, *ItemKey::Create(6, 757)},
});

struct GuildConstruction
{
    std::uint32_t land, guild, prototype;
    std::int32_t map, x, y;
    float xRotation, yRotation, zRotation;
    std::uint32_t objectCount, lastObject, gold;
    bool requireMaster;
};

inline std::vector<std::uint8_t> EncodeGuildConstruction(const GuildConstruction& b)
{
    const std::uint32_t fields[]{b.land, b.guild, b.prototype,
        std::bit_cast<std::uint32_t>(b.map), std::bit_cast<std::uint32_t>(b.x), std::bit_cast<std::uint32_t>(b.y),
        std::bit_cast<std::uint32_t>(b.xRotation), std::bit_cast<std::uint32_t>(b.yRotation),
        std::bit_cast<std::uint32_t>(b.zRotation), b.objectCount, b.lastObject, b.gold, b.requireMaster};
    std::vector<std::uint8_t> bytes;
    for (auto field : fields)
        for (unsigned shift = 0; shift < 32; shift += 8)
            bytes.push_back(static_cast<std::uint8_t>(field >> shift));
    return bytes;
}

inline std::optional<GuildConstruction> DecodeGuildConstruction(const ItemCommit& commit)
{
    if (commit.outcomeCode != GuildConstructionOutcome || commit.kind != ItemOperationKind::Service ||
        commit.result.size() != 52 || commit.owners.size() != 1 ||
        commit.owners.front().owner.kind != ItemOwnerKind::Character)
        return {};
    const auto field = [&](std::size_t index) {
        std::uint32_t value = 0;
        for (unsigned byte = 0; byte < 4; ++byte)
            value |= std::uint32_t{commit.result[4 * index + byte]} << (8 * byte);
        return value;
    };
    const GuildConstruction b{field(0), field(1), field(2),
        std::bit_cast<std::int32_t>(field(3)), std::bit_cast<std::int32_t>(field(4)),
        std::bit_cast<std::int32_t>(field(5)), std::bit_cast<float>(field(6)),
        std::bit_cast<float>(field(7)), std::bit_cast<float>(field(8)), field(9), field(10), field(11), field(12) != 0};
    if (!b.land || !b.prototype || b.map <= 0 || !std::isfinite(b.xRotation) ||
        !std::isfinite(b.yRotation) || !std::isfinite(b.zRotation) || field(12) > 1 ||
        (b.requireMaster && !b.guild) || (b.objectCount == 0) != (b.lastObject == 0) ||
        b.gold > 100'000'000 || commit.wallets.size() != (b.gold ? 1u : 0u))
        return {};
    if (b.gold)
    {
        const auto& wallet = commit.wallets.front();
        if (wallet.before.currency != WalletCurrency::Gold || wallet.before.balance < b.gold ||
            wallet.after.balance != wallet.before.balance - b.gold)
            return {};
    }
    for (const auto& change : commit.items)
    {
        if (!change.before || change.before->count <= change.after.count || change.before->refinementLevel != 0 ||
            std::ranges::none_of(GuildConstructionMaterials, [&](const auto& material) {
                return material.second == change.before->key;
            }))
            return {};
    }
    return b;
}
}
