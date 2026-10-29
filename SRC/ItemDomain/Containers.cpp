#include <item/Containers.hpp>
#include <item/ItemCatalog.hpp>
#include "ContentValidation.hpp"
#include <algorithm>
#include <limits>

namespace auriga::item
{
namespace
{
bool ValidAmount(const LootEntry& entry) noexcept
{
    switch (entry.kind)
    {
    case LootRewardKind::Item: return entry.amount >= 1 && entry.amount <= std::numeric_limits<std::uint32_t>::max();
    case LootRewardKind::Gold:
    case LootRewardKind::Experience: return entry.amount >= 1 && entry.amount <= MaxLootCurrency;
    case LootRewardKind::SpawnMonster:
    case LootRewardKind::SpawnMonsterGroup: return entry.amount >= 1 && entry.amount <= std::numeric_limits<std::uint32_t>::max();
    case LootRewardKind::Slow:
    case LootRewardKind::DrainHp: return entry.amount >= 1 && entry.amount <= LootChanceScaleBp;
    case LootRewardKind::Poison: return entry.amount == 0;
    }
    return false;
}
}

std::expected<ValidatedLootProfile, LootProfileError> ValidateLootProfile(LootProfile source)
{
    if (!source.id || !detail::ContentCode(source.code)) return std::unexpected(LootProfileError::InvalidIdentity);
    if (source.mode != LootPickMode::WeightedOne && source.mode != LootPickMode::IndependentChance)
        return std::unexpected(LootProfileError::InvalidMode);
    if (source.entries.empty()) return std::unexpected(LootProfileError::EmptyProfile);
    if (source.entries.size() > MaxLootEntries) return std::unexpected(LootProfileError::TooManyEntries);
    std::ranges::sort(source.entries, {}, &LootEntry::entry);
    std::uint64_t total = 0;
    for (std::size_t i = 0; i < source.entries.size(); ++i)
    {
        const auto& entry = source.entries[i];
        if (entry.entry != i + 1) return std::unexpected(LootProfileError::InvalidEntryOrder);
        if (entry.kind < LootRewardKind::Item || entry.kind > LootRewardKind::Poison ||
            (entry.kind == LootRewardKind::Item) != entry.item.has_value())
            return std::unexpected(LootProfileError::InvalidReward);
        if (!ValidAmount(entry)) return std::unexpected(LootProfileError::InvalidAmount);
        if (!entry.weight || (source.mode == LootPickMode::IndependentChance && entry.weight > LootChanceScaleBp))
            return std::unexpected(LootProfileError::InvalidWeight);
        if (entry.rareAttributeChanceBp > LootChanceScaleBp || (entry.kind != LootRewardKind::Item && entry.rareAttributeChanceBp))
            return std::unexpected(LootProfileError::InvalidRareChance);
        total += entry.weight;
        if (total > std::numeric_limits<std::uint32_t>::max()) return std::unexpected(LootProfileError::WeightOverflow);
    }
    const auto weight = source.mode == LootPickMode::WeightedOne ? static_cast<std::uint32_t>(total)
                                                                 : static_cast<std::uint32_t>(source.entries.size());
    return ValidatedLootProfile(std::move(source), weight);
}

std::expected<std::vector<std::size_t>, LootPickError> PickLoot(const ValidatedLootProfile& profile, const LootDraw& draw)
{
    const auto& entries = profile.Data().entries;
    std::vector<std::size_t> picked;
    if (profile.Data().mode == LootPickMode::WeightedOne)
    {
        const auto value = draw(profile.TotalWeight());
        if (value >= profile.TotalWeight()) return std::unexpected(LootPickError::DrawOutOfRange);
        std::uint64_t cumulative = 0;
        for (std::size_t i = 0; i < entries.size(); ++i)
        {
            cumulative += entries[i].weight;
            if (value < cumulative) { picked.push_back(i); break; }
        }
        return picked;
    }
    for (std::size_t i = 0; i < entries.size(); ++i)
    {
        const auto value = draw(LootChanceScaleBp);
        if (value >= LootChanceScaleBp) return std::unexpected(LootPickError::DrawOutOfRange);
        if (value < entries[i].weight) picked.push_back(i);
    }
    return picked;
}

std::expected<ContainerOpenPlan, ContainerOpenError> PlanContainerOpen(
    const ItemCatalog& catalog, const ContainerOpenRequest& request, const LootDraw& draw)
{
    const auto* entry = catalog.Find(request.container);
    if (!entry) return std::unexpected(ContainerOpenError::MissingItem);
    const auto* container = std::get_if<ItemContainerRow>(&entry->category);
    if (!container) return std::unexpected(ContainerOpenError::NotContainer);
    if (entry->base.retired) return std::unexpected(ContainerOpenError::RetiredItem);
    if (request.containerUnits > entry->base.maxStack) return std::unexpected(ContainerOpenError::InvalidStack);
    if (container->kind == ContainerKind::PetEgg) return std::unexpected(ContainerOpenError::PetEggRequiresHatching);
    if (container->kind == ContainerKind::Scripted) return std::unexpected(ContainerOpenError::ScriptedContainer);
    if (container->kind == ContainerKind::Use && request.key) return std::unexpected(ContainerOpenError::KeyNotAccepted);
    if (container->kind == ContainerKind::Locked)
    {
        if (!request.key) return std::unexpected(ContainerOpenError::KeyRequired);
        const auto* keyEntry = catalog.Find(*request.key);
        if (!keyEntry) return std::unexpected(ContainerOpenError::MissingItem);
        const auto* key = std::get_if<ItemContainerKeyRow>(&keyEntry->category);
        if (!key) return std::unexpected(ContainerOpenError::NotKey);
        if (keyEntry->base.retired) return std::unexpected(ContainerOpenError::RetiredItem);
        if (request.keyUnits > keyEntry->base.maxStack) return std::unexpected(ContainerOpenError::InvalidStack);
        if (key->lockId != container->lockId) return std::unexpected(ContainerOpenError::WrongKey);
        if (request.keyUnits < 1) return std::unexpected(ContainerOpenError::InsufficientUnits);
    }
    if (request.containerUnits < 1) return std::unexpected(ContainerOpenError::InsufficientUnits);
    const auto* profile = catalog.FindLoot(*container->lootProfile);
    const auto picked = PickLoot(*profile, draw);
    if (!picked) return std::unexpected(ContainerOpenError::DrawOutOfRange);
    ContainerOpenPlan plan{request.container, container->kind == ContainerKind::Locked ? request.key : std::nullopt,
                           1, static_cast<std::uint16_t>(container->kind == ContainerKind::Locked ? 1 : 0), {}};
    for (auto index : *picked) plan.rewards.push_back(profile->Data().entries[index]);
    return plan;
}
} // namespace auriga::item
