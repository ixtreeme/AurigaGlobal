#include <item/ItemCatalog.hpp>
#include "CatalogHash.hpp"
#include "ContentValidation.hpp"
#include <algorithm>
#include <limits>
#include <set>

namespace auriga::item
{
namespace
{
ItemCatalogError Error(ItemCatalogErrorCode code, std::string field, std::optional<ItemKey> key = {}, std::uint32_t id = 0)
{
    return {code, std::move(field), key, id, {}, {}, {}, {}, {}};
}
template <typename T> const T* FindProfile(const std::vector<T>& rows, std::uint32_t id) noexcept
{
    const auto found = std::ranges::lower_bound(rows, id, {}, &T::id);
    return found != rows.end() && found->id == id ? &*found : nullptr;
}
template <typename T, typename Validator>
std::expected<void, ItemCatalogError> Profiles(std::vector<T>& rows, std::string_view field, Validator validate)
{
    std::ranges::sort(rows, {}, &T::id);
    std::set<std::string> codes;
    std::uint32_t previous = 0;
    for (const auto& row : rows)
    {
        if (!row.id || !detail::ContentCode(row.code))
            return std::unexpected(Error(ItemCatalogErrorCode::InvalidIdentity, std::string(field), {}, row.id));
        if (previous == row.id || !codes.insert(row.code).second)
            return std::unexpected(Error(ItemCatalogErrorCode::DuplicateProfile, std::string(field), {}, row.id));
        if (!validate(row)) return std::unexpected(Error(ItemCatalogErrorCode::InvalidProfile, std::string(field), {}, row.id));
        previous = row.id;
    }
    return {};
}
std::expected<void, ItemCatalogError> Budget(const ItemCatalogSource& source)
{
    if (source.items.size() > ItemCatalogLimits::Items || source.weapons.size() > ItemCatalogLimits::Items ||
        source.armors.size() > ItemCatalogLimits::Items || source.accessories.size() > ItemCatalogLimits::Items ||
        source.materials.size() > ItemCatalogLimits::Items || source.consumables.size() > ItemCatalogLimits::Items ||
        source.containers.size() > ItemCatalogLimits::Items || source.containerKeys.size() > ItemCatalogLimits::Items)
        return std::unexpected(Error(ItemCatalogErrorCode::LimitExceeded, "items"));
    std::size_t textBytes = 0;
    auto add = [&](std::size_t length) {
        if (length > ItemCatalogLimits::TextBytes - textBytes) return false;
        textBytes += length;
        return true;
    };
    auto profiles = [&](const auto& rows) {
        if (rows.size() > ItemCatalogLimits::ProfilesPerKind) return false;
        for (const auto& row : rows) if (!add(row.code.size())) return false;
        return true;
    };
    if (!profiles(source.usePolicies) || !profiles(source.tradePolicies) || !profiles(source.appearances) ||
        !profiles(source.attacks) || !profiles(source.ammoPolicies) || !profiles(source.resistances) || !profiles(source.refinements) ||
        !profiles(source.bonuses) || !profiles(source.accessorySockets) || !profiles(source.recoveries) || !profiles(source.consumableCooldowns) ||
        !profiles(source.lootProfiles))
        return std::unexpected(Error(ItemCatalogErrorCode::LimitExceeded, "profiles"));
    for (const auto& profile : source.lootProfiles)
        if (profile.entries.size() > MaxLootEntries)
            return std::unexpected(Error(ItemCatalogErrorCode::LimitExceeded, "loot.entries", {}, profile.id));
    for (const auto& profile : source.bonuses)
        if (profile.entries.size() > MaxBonusEntriesPerProfile)
            return std::unexpected(Error(ItemCatalogErrorCode::LimitExceeded, "bonus.entries", {}, profile.id));
    for (const auto& row : source.appearances)
        if (!add(row.modelAsset.size())) return std::unexpected(Error(ItemCatalogErrorCode::LimitExceeded, "appearance"));
    for (const auto& row : source.items)
        if (!add(row.contentCode.size()) || !add(row.nameKey.size()) || !add(row.descriptionKey.size()) || !add(row.iconAsset.size()))
            return std::unexpected(Error(ItemCatalogErrorCode::LimitExceeded, "text"));
    return {};
}
ItemBaseStats BaseStats(const ItemWeaponRow& row) { return {row.physicalAttackBaseRaw, row.magicalAttackBaseRaw, 0, 0}; }
ItemBaseStats BaseStats(const ItemArmorRow& row) { return {0, 0, row.physicalDefenseBaseRaw, row.magicalDefenseBaseRaw}; }
ItemBaseStats BaseStats(const ItemAccessoryRow&) { return {}; }
ItemBaseStats BaseStats(const ItemMaterialRow&) { return {}; }
ItemBaseStats BaseStats(const ItemConsumableRow&) { return {}; }
ItemBaseStats BaseStats(const ItemContainerRow&) { return {}; }
ItemBaseStats BaseStats(const ItemContainerKeyRow&) { return {}; }
}

std::expected<std::shared_ptr<const ItemCatalog>, ItemCatalogError> BuildItemCatalog(ItemCatalogSource source)
{
    if (source.schemaVersion != 6 || !source.revision) return std::unexpected(Error(ItemCatalogErrorCode::InvalidVersion, "metadata"));
    if (auto result = Budget(source); !result) return std::unexpected(result.error());
    if (auto r = Profiles(source.usePolicies, "use_policy", [](const auto& row) {
        return row.allowedClasses > 0 && row.allowedClasses <= 15 && row.allowedSexes > 0 && row.allowedSexes <= 3;
    }); !r) return std::unexpected(r.error());
    if (auto r = Profiles(source.tradePolicies, "trade_policy", [](const auto&) { return true; }); !r) return std::unexpected(r.error());
    if (auto r = Profiles(source.appearances, "appearance", [](const auto& row) { return detail::AssetReference(row.modelAsset); }); !r) return std::unexpected(r.error());
    if (auto r = Profiles(source.attacks, "attack", [](const auto& row) {
        return row.intervalTicks && row.windupTicks <= row.intervalTicks && row.reachMillimeters && row.maxTargets && row.maxTargets <= 1024;
    }); !r) return std::unexpected(r.error());
    if (auto r = Profiles(source.ammoPolicies, "ammo", [](const auto& row) {
        return (row.kind == AmmoKind::None && row.unitsPerHit == 0) || (row.kind == AmmoKind::ConsumeStack && row.unitsPerHit > 0);
    }); !r) return std::unexpected(r.error());
    if (auto r = Profiles(source.resistances, "resistance", [](const auto& row) {
        return std::ranges::all_of(row.Values(), [](auto value) { return value >= -10'000 && value <= 10'000; });
    }); !r) return std::unexpected(r.error());
    if (auto r = Profiles(source.refinements, "refinement", [](const auto&) { return true; }); !r) return std::unexpected(r.error());
    if (auto r = Profiles(source.bonuses, "bonus", [](const auto&) { return true; }); !r) return std::unexpected(r.error());
    if (auto r = Profiles(source.accessorySockets, "accessory_socket", [](const auto&) { return true; }); !r) return std::unexpected(r.error());
    if (auto r = Profiles(source.recoveries, "recovery", [](const auto&) { return true; }); !r) return std::unexpected(r.error());
    if (auto r = Profiles(source.consumableCooldowns, "consumable_cooldown", ValidConsumableCooldown); !r) return std::unexpected(r.error());
    if (auto r = Profiles(source.lootProfiles, "loot", [](const auto&) { return true; }); !r) return std::unexpected(r.error());

    auto catalog = std::unique_ptr<ItemCatalog>(new ItemCatalog());
    for (auto& row : source.recoveries)
    {
        const auto id = row.id;
        auto validated = ValidateRecoveryProfile(std::move(row));
        if (!validated)
        {
            auto error = Error(ItemCatalogErrorCode::InvalidRecoveryProfile, "recovery", {}, id);
            error.recoveryError = validated.error();
            return std::unexpected(std::move(error));
        }
        row = validated->Data();
        if (row.cooldownProfile && !FindProfile(source.consumableCooldowns, *row.cooldownProfile))
            return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile, "recovery.cooldown", {}, *row.cooldownProfile));
        catalog->recoveries_.emplace(id, std::move(*validated));
    }
    for (auto& row : source.lootProfiles)
    {
        const auto id = row.id;
        auto validated = ValidateLootProfile(std::move(row));
        if (!validated)
        {
            auto error = Error(ItemCatalogErrorCode::InvalidLootProfile, "loot", {}, id);
            error.lootError = validated.error();
            return std::unexpected(std::move(error));
        }
        row = validated->Data(); // normalized entry order also feeds canonical hashing
        catalog->loots_.emplace(id, std::move(*validated));
    }
    for (auto& row : source.accessorySockets)
    {
        const auto id = row.id;
        auto validated = ValidateAccessorySocketProfile(std::move(row));
        if (!validated)
        {
            auto error = Error(ItemCatalogErrorCode::InvalidSocketProfile, "accessory_socket", {}, id);
            error.socketError = validated.error();
            return std::unexpected(std::move(error));
        }
        row = validated->Data();
        catalog->accessorySockets_.emplace(row.id, std::move(*validated));
    }
    catalog->bonuses_.reserve(source.bonuses.size());
    for (auto& row : source.bonuses)
    {
        const auto id = row.id;
        auto validated = ValidateBonusProfile(std::move(row));
        if (!validated)
        {
            auto error = Error(ItemCatalogErrorCode::InvalidBonusProfile, "bonus", {}, id);
            error.bonusError = validated.error();
            return std::unexpected(std::move(error));
        }
        row = validated->Data();
        catalog->bonuses_.push_back(std::move(*validated));
    }
    for (auto& row : source.refinements)
    {
        const auto id = row.id;
        auto validated = ValidateRefinementProfile(std::move(row), catalog->bonuses_);
        if (!validated)
        {
            auto error = Error(ItemCatalogErrorCode::InvalidRefinement, "refinement", {}, id);
            error.refinementError = validated.error();
            return std::unexpected(std::move(error));
        }
        row = validated->Data(); // normalized levels also feed canonical hashing
        catalog->refinements_.emplace(row.id, std::move(*validated));
    }
    std::ranges::sort(source.items, {}, &ItemBaseRow::key);
    std::ranges::sort(source.weapons, {}, &ItemWeaponRow::key);
    std::ranges::sort(source.armors, {}, &ItemArmorRow::key);
    std::ranges::sort(source.accessories, {}, &ItemAccessoryRow::key);
    std::ranges::sort(source.materials, {}, &ItemMaterialRow::key);
    std::ranges::sort(source.consumables, {}, &ItemConsumableRow::key);
    std::ranges::sort(source.containers, {}, &ItemContainerRow::key);
    std::ranges::sort(source.containerKeys, {}, &ItemContainerKeyRow::key);
    std::set<std::string> codes;
    std::map<ItemKey, const ItemWeaponRow*> weapons;
    std::map<ItemKey, const ItemArmorRow*> armors;
    std::map<ItemKey, const ItemAccessoryRow*> accessories;
    std::map<ItemKey, const ItemMaterialRow*> materials;
    std::map<ItemKey, const ItemConsumableRow*> consumables;
    std::map<ItemKey, const ItemContainerRow*> containers;
    std::map<ItemKey, const ItemContainerKeyRow*> containerKeys;
    for (const auto& row : source.weapons)
    {
        if (row.key.Category() != ItemCategory::Weapon) return std::unexpected(Error(ItemCatalogErrorCode::WrongCategory, "weapon", row.key));
        if (!weapons.emplace(row.key, &row).second) return std::unexpected(Error(ItemCatalogErrorCode::DuplicateCategoryRow, "weapon", row.key));
        if (row.slot != EquipmentSlot::Weapon || row.subtype < WeaponSubtype::Sword || row.subtype > WeaponSubtype::MountSpear)
            return std::unexpected(Error(ItemCatalogErrorCode::InvalidProfile, "weapon.type", row.key));
        const auto* ammo = FindProfile(source.ammoPolicies, row.ammoPolicy);
        if (!ammo || !FindProfile(source.attacks, row.attackProfile)) return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile, "weapon", row.key));
        if ((row.subtype == WeaponSubtype::Bow) != (ammo->kind == AmmoKind::ConsumeStack))
            return std::unexpected(Error(ItemCatalogErrorCode::InvalidProfile, "weapon.ammo", row.key));
    }
    for (const auto& row : source.armors)
    {
        if (row.key.Category() != ItemCategory::Armor) return std::unexpected(Error(ItemCatalogErrorCode::WrongCategory, "armor", row.key));
        if (!armors.emplace(row.key, &row).second) return std::unexpected(Error(ItemCatalogErrorCode::DuplicateCategoryRow, "armor", row.key));
        if (row.slot < EquipmentSlot::Body || row.slot > EquipmentSlot::Feet)
            return std::unexpected(Error(ItemCatalogErrorCode::InvalidProfile, "armor.slot", row.key));
        if (!FindProfile(source.resistances, row.resistanceProfile)) return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile, "armor.resistance", row.key));
    }
    for (const auto& row : source.accessories)
    {
        if (row.key.Category() != ItemCategory::Accessory) return std::unexpected(Error(ItemCatalogErrorCode::WrongCategory, "accessory", row.key));
        if (!accessories.emplace(row.key, &row).second) return std::unexpected(Error(ItemCatalogErrorCode::DuplicateCategoryRow, "accessory", row.key));
        if (row.slot < EquipmentSlot::Wrist || row.slot > EquipmentSlot::Ear)
            return std::unexpected(Error(ItemCatalogErrorCode::InvalidProfile, "accessory.slot", row.key));
        if (!FindProfile(source.resistances, row.resistanceProfile))
            return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile, "accessory.resistance", row.key));
        if (row.socketProfile && !catalog->FindAccessorySocket(*row.socketProfile))
            return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile, "accessory.socket", row.key, *row.socketProfile));
    }
    for (const auto& row : source.materials)
    {
        if (row.key.Category() != ItemCategory::Material) return std::unexpected(Error(ItemCatalogErrorCode::WrongCategory, "material", row.key));
        if (!materials.emplace(row.key, &row).second) return std::unexpected(Error(ItemCatalogErrorCode::DuplicateCategoryRow, "material", row.key));
        if (row.kind < MaterialKind::Leather || row.kind > MaterialKind::Jewel)
            return std::unexpected(Error(ItemCatalogErrorCode::InvalidProfile, "material.kind", row.key));
    }
    for (const auto& row : source.consumables)
    {
        if (row.key.Category() != ItemCategory::Consumable) return std::unexpected(Error(ItemCatalogErrorCode::WrongCategory, "consumable", row.key));
        if (!consumables.emplace(row.key, &row).second) return std::unexpected(Error(ItemCatalogErrorCode::DuplicateCategoryRow, "consumable", row.key));
        if (!catalog->FindRecovery(row.recoveryProfile))
            return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile, "consumable.recovery", row.key, row.recoveryProfile));
    }
    for (const auto& row : source.containers)
    {
        if (row.key.Category() != ItemCategory::Container) return std::unexpected(Error(ItemCatalogErrorCode::WrongCategory, "container", row.key));
        if (!containers.emplace(row.key, &row).second) return std::unexpected(Error(ItemCatalogErrorCode::DuplicateCategoryRow, "container", row.key));
        const bool scripted = row.kind == ContainerKind::Scripted;
        if (row.kind < ContainerKind::Use || row.kind > ContainerKind::Scripted || scripted == row.lootProfile.has_value() ||
            (row.kind == ContainerKind::Locked) != (row.lockId > 0))
            return std::unexpected(Error(ItemCatalogErrorCode::InvalidContainer, "container.kind", row.key));
        if (row.lootProfile && !catalog->FindLoot(*row.lootProfile))
            return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile, "container.loot", row.key, *row.lootProfile));
    }
    for (const auto& row : source.containerKeys)
    {
        if (row.key.Category() != ItemCategory::ContainerKey) return std::unexpected(Error(ItemCatalogErrorCode::WrongCategory, "container_key", row.key));
        if (!containerKeys.emplace(row.key, &row).second) return std::unexpected(Error(ItemCatalogErrorCode::DuplicateCategoryRow, "container_key", row.key));
        if (!row.lockId) return std::unexpected(Error(ItemCatalogErrorCode::InvalidContainer, "container_key.lock", row.key));
    }
    std::optional<ItemKey> previous;
    for (const auto& row : source.items)
    {
        if (previous == row.key) return std::unexpected(Error(ItemCatalogErrorCode::DuplicateItem, "item_base", row.key));
        previous = row.key;
        const auto category = row.key.Category();
        const bool material = category == ItemCategory::Material;
        const bool consumable = category == ItemCategory::Consumable;
        const bool container = category == ItemCategory::Container;
        const bool containerKey = category == ItemCategory::ContainerKey;
        const bool stackable = material || consumable || container || containerKey;
        if (category != ItemCategory::Weapon && category != ItemCategory::Armor && category != ItemCategory::Accessory && !stackable)
            return std::unexpected(Error(ItemCatalogErrorCode::UnsupportedCategory, "item_base", row.key));
        if (!detail::ContentCode(row.contentCode) || !detail::ContentCode(row.nameKey, 128) ||
            !detail::ContentCode(row.descriptionKey, 128) || !detail::AssetReference(row.iconAsset) ||
            !row.width || !row.height || !row.maxStack || (!stackable && row.maxStack != 1) ||
            row.binding < BindingPolicy::None || row.binding > BindingPolicy::OnEquip ||
            row.currency != ItemCurrency::Gold || row.buyAmount > INT64_MAX || row.sellAmount > INT64_MAX)
            return std::unexpected(Error(ItemCatalogErrorCode::InvalidBase, "item_base", row.key));
        if (!codes.insert(row.contentCode).second) return std::unexpected(Error(ItemCatalogErrorCode::DuplicateContentCode, "content_code", row.key));
        if (stackable && (row.binding == BindingPolicy::OnEquip || row.appearanceProfile || row.baseBonusProfile))
            return std::unexpected(Error(ItemCatalogErrorCode::InvalidBase, "non_equipment.equipment_state", row.key));
        if (row.baseBonusProfile)
        {
            const auto* bonus = catalog->FindBonus(*row.baseBonusProfile);
            if (!bonus) return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile, "item_base.bonus", row.key, *row.baseBonusProfile));
            if (!AcceptsBonusSource(*bonus, BonusSource::ItemBase))
                return std::unexpected(Error(ItemCatalogErrorCode::WrongBonusSource, "item_base.bonus", row.key, *row.baseBonusProfile));
        }
        if (!FindProfile(source.usePolicies, row.usePolicy) || !FindProfile(source.tradePolicies, row.tradePolicy) ||
            (row.appearanceProfile && !FindProfile(source.appearances, *row.appearanceProfile)))
            return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile, "item_base", row.key));
        const auto* profile = catalog->FindRefinement(row.refinementProfile);
        if (!profile) return std::unexpected(Error(ItemCatalogErrorCode::MissingProfile, "refinement", row.key, row.refinementProfile));
        const auto expectedKind = category == ItemCategory::Weapon ? RefinementKind::Weapon :
            category == ItemCategory::Armor ? RefinementKind::Armor :
            category == ItemCategory::Accessory ? RefinementKind::Accessory : RefinementKind::None;
        if (profile->Data().kind != RefinementKind::None && profile->Data().kind != expectedKind)
            return std::unexpected(Error(ItemCatalogErrorCode::WrongRefinementKind, "refinement", row.key, row.refinementProfile));
        std::optional<ItemCategoryRow> child;
        if (category == ItemCategory::Weapon && weapons.contains(row.key)) child = *weapons.at(row.key);
        if (category == ItemCategory::Armor && armors.contains(row.key)) child = *armors.at(row.key);
        if (category == ItemCategory::Accessory && accessories.contains(row.key)) child = *accessories.at(row.key);
        if (material && materials.contains(row.key)) child = *materials.at(row.key);
        if (consumable && consumables.contains(row.key)) child = *consumables.at(row.key);
        if (container && containers.contains(row.key)) child = *containers.at(row.key);
        if (containerKey && containerKeys.contains(row.key)) child = *containerKeys.at(row.key);
        if (!child) return std::unexpected(Error(ItemCatalogErrorCode::MissingCategoryRow, "category", row.key));
        if (consumable && catalog->FindRecovery(consumables.at(row.key)->recoveryProfile)->Data().unitsPerUse > row.maxStack)
            return std::unexpected(Error(ItemCatalogErrorCode::InvalidBase, "consumable.stack_below_use_units", row.key));
        const auto base = std::visit([](const auto& value) { return BaseStats(value); }, *child);
        // Gains are monotone; the maximum level proves the sum bound at all levels.
        if (!ResolveRefinedBaseStats(base, *profile, profile->Data().maxLevel))
            return std::unexpected(Error(ItemCatalogErrorCode::StatOutOfRange, "base_plus_gain", row.key));
        catalog->items_.emplace(row.key, ItemCatalogEntry{row, std::move(*child)});
    }
    for (const auto& row : source.weapons)
        if (!catalog->Find(row.key)) return std::unexpected(Error(ItemCatalogErrorCode::OrphanCategoryRow, "weapon", row.key));
    for (const auto& row : source.armors)
        if (!catalog->Find(row.key)) return std::unexpected(Error(ItemCatalogErrorCode::OrphanCategoryRow, "armor", row.key));
    for (const auto& row : source.accessories)
        if (!catalog->Find(row.key)) return std::unexpected(Error(ItemCatalogErrorCode::OrphanCategoryRow, "accessory", row.key));
    for (const auto& row : source.materials)
        if (!catalog->Find(row.key)) return std::unexpected(Error(ItemCatalogErrorCode::OrphanCategoryRow, "material", row.key));
    for (const auto& row : source.consumables)
        if (!catalog->Find(row.key)) return std::unexpected(Error(ItemCatalogErrorCode::OrphanCategoryRow, "consumable", row.key));
    for (const auto& row : source.containers)
        if (!catalog->Find(row.key)) return std::unexpected(Error(ItemCatalogErrorCode::OrphanCategoryRow, "container", row.key));
    for (const auto& row : source.containerKeys)
        if (!catalog->Find(row.key)) return std::unexpected(Error(ItemCatalogErrorCode::OrphanCategoryRow, "container_key", row.key));
    // Item rewards are ordinary, live, stack-sized grants of a published item.
    for (const auto& profile : source.lootProfiles)
        for (const auto& reward : profile.entries)
        {
            if (reward.kind != LootRewardKind::Item) continue;
            const auto* target = catalog->Find(*reward.item);
            if (!target || target->base.retired || reward.amount > target->base.maxStack)
            {
                auto error = Error(ItemCatalogErrorCode::InvalidLootReward, "loot.item", *reward.item, profile.id);
                return std::unexpected(std::move(error));
            }
        }
    for (const auto& profile : source.accessorySockets)
    {
        for (const auto& key : profile.unlockMaterials)
            if (!materials.contains(key)) return std::unexpected(Error(ItemCatalogErrorCode::InvalidSocketMaterial, "socket.unlock_material", key, profile.id));
        for (const auto& key : profile.chargeMaterials)
            if (!materials.contains(key) || materials.at(key)->kind != MaterialKind::Jewel)
                return std::unexpected(Error(ItemCatalogErrorCode::InvalidSocketMaterial, "socket.charge_material", key, profile.id));
        for (const auto& key : profile.permanentChargeMaterials)
            if (!materials.contains(key) || materials.at(key)->kind != MaterialKind::Jewel)
                return std::unexpected(Error(ItemCatalogErrorCode::InvalidSocketMaterial, "socket.permanent_charge_material", key, profile.id));
    }
    auto digest = detail::CanonicalCatalogHash(source);
    if (!digest) return std::unexpected(Error(digest.error(), "content_hash"));
    catalog->digest_ = *digest;
    catalog->source_ = std::move(source);
    return std::shared_ptr<const ItemCatalog>(std::move(catalog));
}

const ItemCatalogEntry* ItemCatalog::Find(ItemKey key) const noexcept
{
    const auto found = items_.find(key);
    return found == items_.end() ? nullptr : &found->second;
}
const ValidatedRefinementProfile* ItemCatalog::FindRefinement(std::uint32_t id) const noexcept
{
    const auto found = refinements_.find(id);
    return found == refinements_.end() ? nullptr : &found->second;
}
const ItemUsePolicy* ItemCatalog::FindUsePolicy(std::uint32_t id) const noexcept { return FindProfile(source_.usePolicies, id); }
const ItemTradePolicy* ItemCatalog::FindTradePolicy(std::uint32_t id) const noexcept { return FindProfile(source_.tradePolicies, id); }
const ItemAppearanceProfile* ItemCatalog::FindAppearance(std::uint32_t id) const noexcept { return FindProfile(source_.appearances, id); }
const ItemAttackProfile* ItemCatalog::FindAttack(std::uint32_t id) const noexcept { return FindProfile(source_.attacks, id); }
const ItemAmmoPolicy* ItemCatalog::FindAmmoPolicy(std::uint32_t id) const noexcept { return FindProfile(source_.ammoPolicies, id); }
const ItemResistanceProfile* ItemCatalog::FindResistance(std::uint32_t id) const noexcept { return FindProfile(source_.resistances, id); }
const ValidatedBonusProfile* ItemCatalog::FindBonus(std::uint32_t id) const noexcept
{
    // The builder has already established a sorted, unique, nonempty profile set.
    const auto found = std::ranges::lower_bound(bonuses_, id, {}, [](const auto& row) { return row.Data().id; });
    return found != bonuses_.end() && found->Data().id == id ? &*found : nullptr;
}
const ValidatedAccessorySocketProfile* ItemCatalog::FindAccessorySocket(std::uint32_t id) const noexcept
{
    const auto found = accessorySockets_.find(id);
    return found == accessorySockets_.end() ? nullptr : &found->second;
}
const ValidatedRecoveryProfile* ItemCatalog::FindRecovery(std::uint32_t id) const noexcept
{
    const auto found = recoveries_.find(id);
    return found == recoveries_.end() ? nullptr : &found->second;
}
const ConsumableCooldownProfile* ItemCatalog::FindConsumableCooldown(std::uint32_t id) const noexcept
{
    return FindProfile(source_.consumableCooldowns, id);
}
const ValidatedLootProfile* ItemCatalog::FindLoot(std::uint32_t id) const noexcept
{
    const auto found = loots_.find(id);
    return found == loots_.end() ? nullptr : &found->second;
}
std::expected<ItemBonusProfiles, ItemCatalogLookupError> ItemCatalog::BonusProfilesAt(ItemKey key, std::uint16_t level) const noexcept
{
    const auto* item = Find(key);
    if (!item) return std::unexpected(ItemCatalogLookupError::MissingItem);
    const auto* step = FindRefinement(item->base.refinementProfile)->FindLevel(level);
    if (!step) return std::unexpected(ItemCatalogLookupError::InvalidRefinementLevel);
    return ItemBonusProfiles{
        item->base.baseBonusProfile ? FindBonus(*item->base.baseBonusProfile) : nullptr,
        step->extraBonusProfile ? FindBonus(*step->extraBonusProfile) : nullptr};
}
std::expected<ItemBaseStats, ItemCatalogLookupError> ItemCatalog::RefinedBaseStats(ItemKey key, std::uint16_t level) const noexcept
{
    const auto* item = Find(key);
    if (!item) return std::unexpected(ItemCatalogLookupError::MissingItem);
    const auto* profile = FindRefinement(item->base.refinementProfile);
    const auto base = std::visit([](const auto& row) { return BaseStats(row); }, item->category);
    const auto result = ResolveRefinedBaseStats(base, *profile, level);
    if (!result) return std::unexpected(ItemCatalogLookupError::InvalidRefinementLevel);
    return *result;
}
} // namespace auriga::item
