#pragma once

#include "ItemMissions.hpp"
#include <algorithm>

namespace auriga::item
{
inline constexpr std::uint16_t GuildProgressOutcome = 0x4212;
inline constexpr std::size_t GuildProgressSlots = 15, GuildProgressRequirements = 5;

struct GuildProgressSlot
{
    std::uint64_t material = 0; // packed ItemKey, zero for an empty slot
    std::uint32_t count = 0;
    bool operator==(const GuildProgressSlot&) const = default;
};

struct GuildProgress
{
    std::uint32_t guild = 0, currentLevel = 0;
    bool levelUp = false;
    std::uint64_t moneyBefore = 0, moneyPaid = 0;
    std::array<GuildProgressSlot, GuildProgressSlots> before{}, after{};
    std::array<std::uint32_t, GuildProgressRequirements> contributed{};
};

inline std::vector<std::uint8_t> EncodeGuildProgress(const GuildProgress& progress)
{
    std::vector<std::uint8_t> bytes;
    const auto write = [&](std::uint64_t field, unsigned size) {
        for (unsigned byte = 0; byte < size; ++byte)
            bytes.push_back(static_cast<std::uint8_t>(field >> (8 * byte)));
    };
    write(progress.guild, 4);
    write(progress.currentLevel, 1);
    write(progress.levelUp, 1);
    write(progress.moneyBefore, 8);
    write(progress.moneyPaid, 8);
    for (const auto& slots : {&progress.before, &progress.after})
        for (const auto& slot : *slots)
        {
            write(slot.material, 8);
            write(slot.count, 4);
        }
    for (auto count : progress.contributed)
        write(count, 4);
    return bytes;
}

inline std::optional<GuildProgress> DecodeGuildProgress(const ItemCommit& commit)
{
    if (commit.outcomeCode != GuildProgressOutcome || commit.kind != ItemOperationKind::Service ||
        commit.owners.size() != 1 || commit.owners.front().owner.kind != ItemOwnerKind::Character ||
        commit.result.size() != 22 + GuildProgressSlots * 24 + GuildProgressRequirements * 4)
        return {};
    std::size_t offset = 0;
    const auto read = [&](unsigned size) {
        std::uint64_t value = 0;
        for (unsigned byte = 0; byte < size; ++byte)
            value |= std::uint64_t{commit.result[offset++]} << (8 * byte);
        return value;
    };
    GuildProgress progress;
    progress.guild = static_cast<std::uint32_t>(read(4));
    progress.currentLevel = static_cast<std::uint32_t>(read(1));
    const auto flag = read(1);
    progress.levelUp = flag != 0;
    progress.moneyBefore = read(8);
    progress.moneyPaid = read(8);
    if (!progress.guild || progress.currentLevel < 20 || progress.currentLevel >= 60 || flag > 1 ||
        progress.moneyBefore > INT64_MAX || progress.moneyPaid > INT64_MAX - progress.moneyBefore)
        return {};
    for (auto* slots : {&progress.before, &progress.after})
        for (auto& slot : *slots)
        {
            slot.material = read(8);
            slot.count = static_cast<std::uint32_t>(read(4));
            if ((slot.material == 0) != (slot.count == 0) || (slot.material && !UnpackItemKey(slot.material)))
                return {};
        }
    std::uint64_t contributed = 0;
    for (auto& count : progress.contributed)
    {
        count = static_cast<std::uint32_t>(read(4));
        contributed += count;
    }
    if (progress.levelUp)
        return !progress.moneyPaid && !contributed && commit.items.empty() && commit.wallets.empty() &&
            progress.after == decltype(progress.after){} ? std::optional{progress} : std::nullopt;
    if (commit.wallets.size() != (progress.moneyPaid ? 1u : 0u) || (!progress.moneyPaid && !contributed))
        return {};
    if (progress.moneyPaid)
    {
        const auto& wallet = commit.wallets.front();
        if (wallet.before.currency != WalletCurrency::Gold || wallet.before.balance < progress.moneyPaid ||
            wallet.after.balance != wallet.before.balance - progress.moneyPaid)
            return {};
    }
    std::map<ItemKey, std::uint64_t> credited, debited;
    std::uint64_t total = 0;
    for (std::size_t index = 0; index < progress.before.size(); ++index)
    {
        const auto& before = progress.before[index];
        const auto& after = progress.after[index];
        if (before == after)
            continue;
        const auto key = UnpackItemKey(after.material);
        if (!key || (before.material && before.material != after.material) || before.count >= after.count)
            return {};
        credited[*key] += after.count - before.count;
        total += after.count - before.count;
    }
    const auto owner = commit.owners.front().owner;
    for (const auto& change : commit.items)
    {
        if (!change.before || change.before->refinementLevel != 0 || change.before->count <= change.after.count ||
            !(PlainInventoryItem(*change.before, owner) || PlainInventoryItem(*change.before, owner, ItemStorageKind::SpecialInventory)))
            return {};
        debited[change.before->key] += change.before->count - change.after.count;
    }
    return contributed == total && credited == debited ? std::optional{progress} : std::nullopt;
}
}
