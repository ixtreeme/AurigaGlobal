#pragma once
#include "ItemKey.hpp"
#include "RefinementProfiles.hpp"
#include "AccessorySockets.hpp"
#include "Consumables.hpp"
#include <array>
#include <cstdint>
#include <expected>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace auriga::item
{
enum class BindingPolicy : std::uint8_t { None = 1, OnAcquire = 2, OnEquip = 3 };
enum class ItemCurrency : std::uint8_t { Gold = 1 };
enum class EquipmentSlot : std::uint8_t
{
    Weapon = 1, Body = 2, Head = 3, Shield = 4, Feet = 5, Wrist = 6, Neck = 7, Ear = 8,
};
enum class WeaponSubtype : std::uint8_t { Sword = 1, Dagger = 2, Bow = 3, TwoHanded = 4, Bell = 5, Fan = 6, MountSpear = 7 };
enum class AmmoKind : std::uint8_t { None = 1, ConsumeStack = 2 };
// Plain crafting inputs. Soul-refinement catalysts/resources need their own
// typed behavior and are deliberately not accepted as these ordinary materials.
enum class MaterialKind : std::uint8_t { Leather = 1, Blood = 2, Root = 3, Needle = 4, Jewel = 5 };

struct ItemUsePolicy
{
    std::uint32_t id = 0;
    std::string code;
    std::uint8_t allowedClasses = 0; // warrior/assassin/sura/shaman bits 0..3
    std::uint8_t allowedSexes = 0;   // male/female bits 0..1
    bool useDuringCombat = false;
};
struct ItemTradePolicy
{
    std::uint32_t id = 0;
    std::string code;
    bool allowTrade = false, allowDrop = false, allowSell = false;
};
struct ItemAppearanceProfile
{
    std::uint32_t id = 0;
    std::string code, modelAsset;
};
struct ItemAttackProfile
{
    std::uint32_t id = 0;
    std::string code;
    std::uint32_t intervalTicks = 0, windupTicks = 0, reachMillimeters = 0;
    std::uint16_t maxTargets = 0;
};
struct ItemAmmoPolicy
{
    std::uint32_t id = 0;
    std::string code;
    AmmoKind kind = AmmoKind::None;
    std::uint16_t unitsPerHit = 0;
};
struct ItemResistanceProfile
{
    std::uint32_t id = 0;
    std::string code;
    // Structural range [-10000,10000] bp. Ruleset gameplay caps are separate.
    std::int32_t physicalBp = 0, magicalBp = 0;
    std::int32_t swordBp = 0, daggerBp = 0, bowBp = 0, twoHandedBp = 0, bellBp = 0, fanBp = 0, mountSpearBp = 0;
    std::int32_t fireBp = 0, electricBp = 0, iceBp = 0, windBp = 0, earthBp = 0, darkBp = 0;
    auto Values() const noexcept
    {
        return std::array{physicalBp, magicalBp, swordBp, daggerBp, bowBp, twoHandedBp, bellBp, fanBp,
                          mountSpearBp, fireBp, electricBp, iceBp, windBp, earthBp, darkBp};
    }
};
struct ItemBaseRow
{
    ItemKey key;
    std::string contentCode, nameKey, descriptionKey, iconAsset;
    std::uint8_t width = 1, height = 1;
    std::uint32_t maxStack = 1, weightGrams = 0;
    std::uint16_t minimumLevel = 0;
    BindingPolicy binding = BindingPolicy::None;
    std::uint32_t usePolicy = 0, tradePolicy = 0, refinementProfile = 0;
    std::optional<std::uint32_t> appearanceProfile, baseBonusProfile;
    ItemCurrency currency = ItemCurrency::Gold;
    std::uint64_t buyAmount = 0, sellAmount = 0;
    bool retired = false;
};
struct ItemWeaponRow
{
    ItemKey key;
    WeaponSubtype subtype = WeaponSubtype::Sword;
    EquipmentSlot slot = EquipmentSlot::Weapon;
    std::int64_t physicalAttackBaseRaw = 0, magicalAttackBaseRaw = 0;
    std::uint32_t attackProfile = 0, ammoPolicy = 0;
};
struct ItemArmorRow
{
    ItemKey key;
    EquipmentSlot slot = EquipmentSlot::Body;
    std::int64_t physicalDefenseBaseRaw = 0, magicalDefenseBaseRaw = 0;
    std::uint32_t resistanceProfile = 0;
};
struct ItemAccessoryRow
{
    ItemKey key;
    EquipmentSlot slot{}; // Required; omission must not silently mean wrist.
    std::uint32_t resistanceProfile = 0;
    // Flat stats and refinement gains are explicit bonus profiles. Accessory
    // socket grade is separate from refinement; ring/belt/unique are unported.
    std::optional<std::uint32_t> socketProfile;
};
struct ItemMaterialRow
{
    ItemKey key;
    MaterialKind kind{}; // Required; omission must not silently mean leather.
};
struct ItemConsumableRow
{
    ItemKey key;
    // Currently only typed recovery effects are publishable in this category.
    std::uint32_t recoveryProfile = 0;
};
using ItemCategoryRow = std::variant<ItemWeaponRow, ItemArmorRow, ItemAccessoryRow, ItemMaterialRow, ItemConsumableRow>;
struct ItemCatalogSource
{
    std::uint32_t schemaVersion = 5;
    std::uint64_t revision = 0;
    std::vector<ItemBaseRow> items;
    std::vector<ItemWeaponRow> weapons;
    std::vector<ItemArmorRow> armors;
    std::vector<ItemAccessoryRow> accessories;
    std::vector<ItemMaterialRow> materials;
    std::vector<ItemUsePolicy> usePolicies;
    std::vector<ItemTradePolicy> tradePolicies;
    std::vector<ItemAppearanceProfile> appearances;
    std::vector<ItemAttackProfile> attacks;
    std::vector<ItemAmmoPolicy> ammoPolicies;
    std::vector<ItemResistanceProfile> resistances;
    std::vector<RefinementProfile> refinements;
    std::vector<BonusProfile> bonuses;
    std::vector<AccessorySocketProfile> accessorySockets;
    std::vector<ItemConsumableRow> consumables;
    std::vector<RecoveryProfile> recoveries;
    std::vector<ConsumableCooldownProfile> consumableCooldowns;
};
struct ItemCatalogLimits
{
    static constexpr std::size_t Items = 100'000, ProfilesPerKind = 4096;
    static constexpr std::size_t TextBytes = 64 * 1024 * 1024;
};
enum class ItemCatalogErrorCode
{
    InvalidVersion, LimitExceeded, InvalidIdentity, DuplicateProfile, InvalidProfile, MissingProfile,
    InvalidRefinement, DuplicateItem, DuplicateContentCode, InvalidBase, UnsupportedCategory,
    InvalidBonusProfile, WrongBonusSource, WrongCategory, DuplicateCategoryRow, MissingCategoryRow, OrphanCategoryRow,
    WrongRefinementKind, StatOutOfRange, HashFailure, InvalidSocketProfile, InvalidSocketMaterial, InvalidRecoveryProfile,
};
struct ItemCatalogError
{
    ItemCatalogErrorCode code;
    std::string field;
    std::optional<ItemKey> key;
    std::uint32_t profileId = 0;
    std::optional<RefinementError> refinementError;
    std::optional<BonusProfileError> bonusError;
    std::optional<AccessorySocketError> socketError;
    std::optional<RecoveryProfileError> recoveryError;
};
struct ItemCatalogEntry
{
    ItemBaseRow base;
    ItemCategoryRow category;
};
enum class ItemCatalogLookupError { MissingItem, InvalidRefinementLevel };
using ItemCatalogDigest = std::array<std::uint8_t, 32>;
struct ItemBonusProfiles
{
    // Borrowed views: retain the catalog snapshot while using these pointers.
    const ValidatedBonusProfile* base = nullptr;
    const ValidatedBonusProfile* refinement = nullptr;
};

class ItemCatalog;
std::expected<std::shared_ptr<const ItemCatalog>, ItemCatalogError> BuildItemCatalog(ItemCatalogSource source);

// Immutable, owning snapshot. No borrowed proto rows, lazy cache or live registry.
// Supports weapon/armor, wrist/neck/ear accessories with optional timed/permanent
// socket profiles, ordinary materials and recovery consumables. Unsupported categories and
// behaviors fail closed during publication; this is not a runtime stat builder.
class ItemCatalog final
{
public:
    std::uint64_t Revision() const noexcept { return source_.revision; }
    std::size_t Size() const noexcept { return items_.size(); }
    const ItemCatalogDigest& ContentHash() const noexcept { return digest_; }
    const ItemCatalogEntry* Find(ItemKey key) const noexcept;
    const ValidatedRefinementProfile* FindRefinement(std::uint32_t id) const noexcept;
    const ItemUsePolicy* FindUsePolicy(std::uint32_t id) const noexcept;
    const ItemTradePolicy* FindTradePolicy(std::uint32_t id) const noexcept;
    const ItemAppearanceProfile* FindAppearance(std::uint32_t id) const noexcept;
    const ItemAttackProfile* FindAttack(std::uint32_t id) const noexcept;
    const ItemAmmoPolicy* FindAmmoPolicy(std::uint32_t id) const noexcept;
    const ItemResistanceProfile* FindResistance(std::uint32_t id) const noexcept;
    const ValidatedBonusProfile* FindBonus(std::uint32_t id) const noexcept;
    const ValidatedAccessorySocketProfile* FindAccessorySocket(std::uint32_t id) const noexcept;
    const ValidatedRecoveryProfile* FindRecovery(std::uint32_t id) const noexcept;
    const ConsumableCooldownProfile* FindConsumableCooldown(std::uint32_t id) const noexcept;
    std::expected<ItemBaseStats, ItemCatalogLookupError> RefinedBaseStats(ItemKey key, std::uint16_t level) const noexcept;
    // Exactly the current level's cumulative bonus profile, never a sum of
    // profiles from all preceding refinement steps. This does not apply stats.
    std::expected<ItemBonusProfiles, ItemCatalogLookupError> BonusProfilesAt(ItemKey key, std::uint16_t level) const noexcept;
    const std::map<ItemKey, ItemCatalogEntry>& Entries() const noexcept { return items_; }
    ItemCatalog(const ItemCatalog&) = delete;
    ItemCatalog& operator=(const ItemCatalog&) = delete;
private:
    ItemCatalog() = default;
    friend std::expected<std::shared_ptr<const ItemCatalog>, ItemCatalogError> BuildItemCatalog(ItemCatalogSource);
    ItemCatalogSource source_;
    std::map<ItemKey, ItemCatalogEntry> items_;
    std::map<std::uint32_t, ValidatedRefinementProfile> refinements_;
    std::vector<ValidatedBonusProfile> bonuses_;
    std::map<std::uint32_t, ValidatedAccessorySocketProfile> accessorySockets_;
    std::map<std::uint32_t, ValidatedRecoveryProfile> recoveries_;
    ItemCatalogDigest digest_ {};
};
} // namespace auriga::item
