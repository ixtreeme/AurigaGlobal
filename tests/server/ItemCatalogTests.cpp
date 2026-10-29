#include "ItemCatalogFixture.hpp"
#include <item/ItemInstanceId.hpp>
#include <algorithm>
#include <atomic>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <source_location>
#include <stdexcept>
#include <thread>
#include <type_traits>

using namespace auriga::item;
namespace
{
std::size_t checks = 0;
void Check(bool condition, const char* message, const std::source_location location = std::source_location::current())
{
    ++checks;
    if (!condition) throw std::runtime_error(std::string(message) + " at line " + std::to_string(location.line()));
}
template <typename Mutate> void Reject(Mutate mutate, ItemCatalogErrorCode expected,const std::source_location location=std::source_location::current())
{
    auto source = item_catalog_fixture::Catalog();
    mutate(source);
    const auto result = BuildItemCatalog(std::move(source));
    Check(!result && result.error().code == expected, "invalid catalog must not publish a snapshot",location);
}
std::string Hex(const ItemCatalogDigest& digest)
{
    std::ostringstream result;
    for (auto value : digest) result << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(value);
    return result.str();
}
void ValidSnapshot()
{
    auto source = item_catalog_fixture::Catalog();
    const auto snapshot = BuildItemCatalog(source).value();
    const auto sword = item_catalog_fixture::Sword(), armor = item_catalog_fixture::Armor();
    Check(snapshot->Size() == item_catalog_fixture::FixtureItems && snapshot->Revision() == 1, "eight categories and fifteen typed composite prototypes published");
    Check(snapshot->RefinedBaseStats(sword, 3).value() == ItemBaseStats{150000, 42000, 0, 0}, "weapon golden");
    Check(snapshot->RefinedBaseStats(armor, 2).value() == ItemBaseStats{0, 0, 50000, 24000}, "armor golden");
    Check(snapshot->RefinedBaseStats(sword, 10).error() == ItemCatalogLookupError::InvalidRefinementLevel, "no fallback refinement");
    Check(snapshot->RefinedBaseStats(ItemKey::Create(ItemCategory::Weapon, 999).value(), 0).error() == ItemCatalogLookupError::MissingItem, "no fallback prototype");
    Check(snapshot->FindUsePolicy(1)->allowedClasses == 15 && !snapshot->FindUsePolicy(999), "use profile resolved");
    Check(snapshot->FindTradePolicy(1)->allowTrade && !snapshot->FindTradePolicy(999), "trade profile resolved");
    Check(snapshot->FindAppearance(1)->modelAsset == "models/iron_sword.model", "appearance resolved");
    Check(snapshot->FindAttack(1)->intervalTicks == 25 && snapshot->FindAmmoPolicy(1)->unitsPerHit == 0, "attack and ammo policies resolved");
    Check(snapshot->FindResistance(1)->physicalBp == 0, "resistance profile resolved");
    Check(snapshot->FindBonus(1)->Data().entries.front() == BonusEntry{BonusId::PhysicalAttackFlat, 2500}, "bonus entries canonically sorted");
    Check(!snapshot->FindBonus(0) && !snapshot->FindBonus(999), "unknown bonus profile has no fallback");
    Check(snapshot->BonusProfilesAt(sword, 0)->base->Data().id == 1 && !snapshot->BonusProfilesAt(sword, 0)->refinement, "base bonus only at level zero");
    Check(snapshot->BonusProfilesAt(sword, 3)->refinement->Data().id == 2, "current refinement bonus resolved");
    Check(!snapshot->BonusProfilesAt(sword, 4)->refinement, "earlier level bonus is not inherited implicitly");
    Check(snapshot->BonusProfilesAt(sword, 9)->refinement->Data().id == 3, "level nine uses its own complete profile");
    Check(!snapshot->BonusProfilesAt(armor, 0)->base && !snapshot->BonusProfilesAt(armor, 0)->refinement, "absent bonus is explicit");
    Check(snapshot->BonusProfilesAt(sword, 10).error() == ItemCatalogLookupError::InvalidRefinementLevel, "no bonus level fallback");
    Check(snapshot->BonusProfilesAt(ItemKey::Create(ItemCategory::Weapon, 999).value(), 0).error() == ItemCatalogLookupError::MissingItem, "no item bonus fallback");
    static_assert(std::is_const_v<std::remove_reference_t<decltype(*snapshot->Find(sword))>>);
    source.items[0].contentCode = "caller.mutated";
    source.weapons[0].physicalAttackBaseRaw = 1;
    source.refinements[1].levels.clear();
    source.bonuses[0].entries.clear();
    Check(snapshot->Find(sword)->base.contentCode == "weapon.iron_sword", "snapshot owns input strings");
    Check(snapshot->RefinedBaseStats(sword, 3)->physicalAttackRaw == 150000, "snapshot owns rows and refinement levels");
    Check(snapshot->FindBonus(1)->Data().entries.size() == 2, "snapshot owns bonus data");
    auto replacement = item_catalog_fixture::Catalog();
    replacement.revision = 2; replacement.weapons[0].physicalAttackBaseRaw += 1000;
    const auto newer = BuildItemCatalog(std::move(replacement)).value();
    Check(newer->RefinedBaseStats(sword, 3)->physicalAttackRaw == 151000 && snapshot->RefinedBaseStats(sword, 3)->physicalAttackRaw == 150000,
          "pinned snapshots retain their own content");
    std::atomic<bool> correct = true;
    std::vector<std::thread> readers;
    for (unsigned worker = 0; worker < 8; ++worker)
        readers.emplace_back([snapshot, sword, &correct] {
            for (std::uint16_t i = 0; i < 1000; ++i)
            {
                const auto stats = snapshot->RefinedBaseStats(sword, i % 10);
                if (!stats || stats->physicalAttackRaw != 120000 + 10000LL * (i % 10)) correct = false;
            }
        });
    for (auto& reader : readers) reader.join();
    Check(correct.load(), "eight concurrent immutable readers");
}
void InvalidCatalogs()
{
    Reject([](auto& s) { s.revision = 0; }, ItemCatalogErrorCode::InvalidVersion);
    Reject([](auto& s) { s.schemaVersion = 1; }, ItemCatalogErrorCode::InvalidVersion);
    Reject([](auto& s) { s.schemaVersion = 2; }, ItemCatalogErrorCode::InvalidVersion);
    Reject([](auto& s) { s.schemaVersion = 3; }, ItemCatalogErrorCode::InvalidVersion);
    Reject([](auto& s) { s.schemaVersion = 4; }, ItemCatalogErrorCode::InvalidVersion);
    Reject([](auto& s) { s.schemaVersion = 5; }, ItemCatalogErrorCode::InvalidVersion);
    Reject([](auto& s) { s.schemaVersion = 6; }, ItemCatalogErrorCode::InvalidVersion);
    Reject([](auto& s) { s.schemaVersion = 7; }, ItemCatalogErrorCode::InvalidVersion);
    Reject([](auto& s) { s.schemaVersion = 8; }, ItemCatalogErrorCode::InvalidVersion);
    Reject([](auto& s) { s.schemaVersion = 9; }, ItemCatalogErrorCode::InvalidVersion);
    Reject([](auto& s) { s.schemaVersion = 10; }, ItemCatalogErrorCode::InvalidVersion);
    Reject([](auto& s) { s.schemaVersion = 11; }, ItemCatalogErrorCode::InvalidVersion);
    Reject([](auto& s) { s.schemaVersion = 12; }, ItemCatalogErrorCode::InvalidVersion);
    Reject([](auto& s) { s.schemaVersion = 13; }, ItemCatalogErrorCode::InvalidVersion);
    Reject([](auto& s) { s.schemaVersion = 14; }, ItemCatalogErrorCode::InvalidVersion);
    Reject([](auto& s) { s.schemaVersion = 15; }, ItemCatalogErrorCode::InvalidVersion);
    Reject([](auto& s) { s.schemaVersion = 16; }, ItemCatalogErrorCode::InvalidVersion);
    Reject([](auto& s) { s.schemaVersion = 17; }, ItemCatalogErrorCode::InvalidVersion);
    Reject([](auto& s) { s.schemaVersion = 18; }, ItemCatalogErrorCode::InvalidVersion);
    Reject([](auto& s) { s.schemaVersion = 19; }, ItemCatalogErrorCode::InvalidVersion);
    Reject([](auto& s) { s.schemaVersion = 22; }, ItemCatalogErrorCode::InvalidVersion);
    Reject([](auto& s) { s.schemaVersion = 21; }, ItemCatalogErrorCode::InvalidVersion);
    Reject([](auto& s) { s.schemaVersion = 20; }, ItemCatalogErrorCode::InvalidVersion);
    Reject([](auto& s) { s.usePolicies.resize(ItemCatalogLimits::ProfilesPerKind + 1); }, ItemCatalogErrorCode::LimitExceeded);
    Reject([](auto& s) { s.usePolicies.push_back(s.usePolicies[0]); }, ItemCatalogErrorCode::DuplicateProfile);
    Reject([](auto& s) { s.appearances[1].code = s.appearances[0].code; }, ItemCatalogErrorCode::DuplicateProfile);
    Reject([](auto& s) { s.attacks[0].id = 0; }, ItemCatalogErrorCode::InvalidIdentity);
    Reject([](auto& s) { s.attacks[0].intervalTicks = 0; }, ItemCatalogErrorCode::InvalidProfile);
    Reject([](auto& s) { s.attacks[0].windupTicks = 26; }, ItemCatalogErrorCode::InvalidProfile);
    Reject([](auto& s) { s.appearances[0].modelAsset = "../escape.model"; }, ItemCatalogErrorCode::InvalidProfile);
    Reject([](auto& s) { s.usePolicies[0].allowedClasses = 0; }, ItemCatalogErrorCode::InvalidProfile);
    Reject([](auto& s) { s.usePolicies[0].allowedClasses = 16; }, ItemCatalogErrorCode::InvalidProfile);
    Reject([](auto& s) { s.usePolicies[0].allowedSexes = 4; }, ItemCatalogErrorCode::InvalidProfile);
    Reject([](auto& s) { s.resistances[0].fireBp = 10001; }, ItemCatalogErrorCode::InvalidProfile);
    Reject([](auto& s) { s.ammoPolicies[0].unitsPerHit = 1; }, ItemCatalogErrorCode::InvalidProfile);
    Reject([](auto& s) { s.items.push_back(s.items[0]); }, ItemCatalogErrorCode::DuplicateItem);
    Reject([](auto& s) { s.items[1].contentCode = s.items[0].contentCode; }, ItemCatalogErrorCode::DuplicateContentCode);
    Reject([](auto& s) { s.items[0].maxStack = 2; }, ItemCatalogErrorCode::InvalidBase);
    Reject([](auto& s) { s.items[0].width = 0; }, ItemCatalogErrorCode::InvalidBase);
    Reject([](auto& s) { s.items[0].buyAmount = std::numeric_limits<std::uint64_t>::max(); }, ItemCatalogErrorCode::InvalidBase);
    Reject([](auto& s) { s.items[0].contentCode = "name+9"; }, ItemCatalogErrorCode::InvalidBase);
    Reject([](auto& s) { s.items[0].nameKey.clear(); }, ItemCatalogErrorCode::InvalidBase);
    Reject([](auto& s) { s.items[0].iconAsset = "/absolute/icon"; }, ItemCatalogErrorCode::InvalidBase);
    Reject([](auto& s) { s.items[0].baseBonusProfile = 999; }, ItemCatalogErrorCode::MissingProfile);
    Reject([](auto& s) { s.items[0].baseBonusProfile = 2; }, ItemCatalogErrorCode::WrongBonusSource);
    Reject([](auto& s) { s.refinements[1].levels[3].extraBonusProfile = 1; }, ItemCatalogErrorCode::InvalidRefinement);
    Reject([](auto& s) { s.refinements[1].levels[3].extraBonusProfile = 999; }, ItemCatalogErrorCode::InvalidRefinement);
    Reject([](auto& s) { s.bonuses[0].entries.clear(); }, ItemCatalogErrorCode::InvalidBonusProfile);
    Reject([](auto& s) { s.bonuses[0].entries[0].bonus = static_cast<BonusId>(999); }, ItemCatalogErrorCode::InvalidBonusProfile);
    Reject([](auto& s) { s.bonuses.push_back(s.bonuses[0]); }, ItemCatalogErrorCode::DuplicateProfile);
    Reject([](auto& s) { s.bonuses[0].entries.resize(MaxBonusEntriesPerProfile + 1); }, ItemCatalogErrorCode::LimitExceeded);
    Reject([](auto& s) { s.items[0].appearanceProfile = 999; }, ItemCatalogErrorCode::MissingProfile);
    Reject([](auto& s) { s.items[0].usePolicy = 999; }, ItemCatalogErrorCode::MissingProfile);
    Reject([](auto& s) { s.weapons[0].attackProfile = 999; }, ItemCatalogErrorCode::MissingProfile);
    Reject([](auto& s) { s.armors[0].resistanceProfile = 999; }, ItemCatalogErrorCode::MissingProfile);
    Reject([](auto& s) { s.items[0].refinementProfile = 3; }, ItemCatalogErrorCode::WrongRefinementKind);
    Reject([](auto& s) { s.refinements[1].levels.pop_back(); }, ItemCatalogErrorCode::InvalidRefinement);
    Reject([](auto& s) { s.weapons.clear(); }, ItemCatalogErrorCode::MissingCategoryRow);
    Reject([](auto& s) { s.weapons.push_back(s.weapons[0]); }, ItemCatalogErrorCode::DuplicateCategoryRow);
    Reject([](auto& s) { s.weapons[0].key = item_catalog_fixture::Armor(); }, ItemCatalogErrorCode::WrongCategory);
    Reject([](auto& s) { s.weapons[0].slot = EquipmentSlot::Body; }, ItemCatalogErrorCode::InvalidProfile);
    Reject([](auto& s) { s.weapons[0].subtype = WeaponSubtype::Bow; }, ItemCatalogErrorCode::InvalidProfile);
    Reject([](auto& s) { s.weapons[0].physicalAttackBaseRaw = -1; }, ItemCatalogErrorCode::StatOutOfRange);
    Reject([](auto& s) { s.weapons[0].physicalAttackBaseRaw = MaxItemStatRaw; }, ItemCatalogErrorCode::StatOutOfRange);
    Reject([](auto& s) { s.items.erase(s.items.begin()); }, ItemCatalogErrorCode::OrphanCategoryRow);
    Reject([](auto& s) { s.items[0].key = ItemKey::Create(ItemCategory::Special, 1).value(); }, ItemCatalogErrorCode::InvalidBase);
    Reject([](auto& s) { s.accessories.clear(); }, ItemCatalogErrorCode::MissingCategoryRow);
    Reject([](auto& s) { s.materials.clear(); }, ItemCatalogErrorCode::MissingCategoryRow);
    Reject([](auto& s) { s.accessories.push_back(s.accessories[0]); }, ItemCatalogErrorCode::DuplicateCategoryRow);
    Reject([](auto& s) { s.materials.push_back(s.materials[0]); }, ItemCatalogErrorCode::DuplicateCategoryRow);
    Reject([](auto& s) { s.accessories[0].key = item_catalog_fixture::Armor(); }, ItemCatalogErrorCode::WrongCategory);
    Reject([](auto& s) { s.materials[0].key = item_catalog_fixture::Bracelet(); }, ItemCatalogErrorCode::WrongCategory);
    Reject([](auto& s) { s.accessories[0].slot = EquipmentSlot::Body; }, ItemCatalogErrorCode::InvalidProfile);
    Reject([](auto& s) { s.accessories[0].slot = static_cast<EquipmentSlot>(9); }, ItemCatalogErrorCode::InvalidProfile);
    Reject([](auto& s) { s.accessories[0].resistanceProfile = 999; }, ItemCatalogErrorCode::MissingProfile);
    Reject([](auto& s) { s.materials[0].kind = static_cast<MaterialKind>(0); }, ItemCatalogErrorCode::InvalidProfile);
    Reject([](auto& s) { s.materials[0].kind = static_cast<MaterialKind>(6); }, ItemCatalogErrorCode::InvalidProfile);
    Reject([](auto& s) { s.accessories[0] = ItemAccessoryRow{item_catalog_fixture::Bracelet()}; }, ItemCatalogErrorCode::InvalidProfile);
    Reject([](auto& s) { s.materials[0] = ItemMaterialRow{item_catalog_fixture::Leather()}; }, ItemCatalogErrorCode::InvalidProfile);
    Reject([](auto& s) { s.items[2].maxStack = 2; }, ItemCatalogErrorCode::InvalidBase);
    Reject([](auto& s) { s.items[3].maxStack = 0; }, ItemCatalogErrorCode::InvalidBase);
    Reject([](auto& s) { s.items[3].binding = BindingPolicy::OnEquip; }, ItemCatalogErrorCode::InvalidBase);
    Reject([](auto& s) { s.items[3].baseBonusProfile = 1; }, ItemCatalogErrorCode::InvalidBase);
    Reject([](auto& s) { s.items[3].appearanceProfile = 1; }, ItemCatalogErrorCode::InvalidBase);
    Reject([](auto& s) { s.items[2].refinementProfile = 2; }, ItemCatalogErrorCode::WrongRefinementKind);
    Reject([](auto& s) { s.items[3].refinementProfile = 4; }, ItemCatalogErrorCode::WrongRefinementKind);
    Reject([](auto& s) { s.items[0].refinementProfile = 4; }, ItemCatalogErrorCode::WrongRefinementKind);
    Reject([](auto& s) { s.items.erase(s.items.begin() + 2); }, ItemCatalogErrorCode::OrphanCategoryRow);
    Reject([](auto& s) { s.items.pop_back(); }, ItemCatalogErrorCode::OrphanCategoryRow);
    Reject([](auto& s) { s.accessories[0].socketProfile = 999; }, ItemCatalogErrorCode::MissingProfile);
    Reject([](auto& s) { s.accessories[0].socketProfile = 0; }, ItemCatalogErrorCode::MissingProfile);
    Reject([](auto& s) { s.accessorySockets[0].grades.pop_back(); }, ItemCatalogErrorCode::InvalidSocketProfile);
    Reject([](auto& s) { s.accessorySockets[0].chargeMaterials = {ItemKey::Create(ItemCategory::Material, 999).value()}; }, ItemCatalogErrorCode::InvalidSocketMaterial);
    Reject([](auto& s) { s.accessorySockets[0].chargeMaterials = {item_catalog_fixture::Leather()}; }, ItemCatalogErrorCode::InvalidSocketMaterial);
    Reject([](auto& s) { s.accessorySockets[0].unlockMaterials = {ItemKey::Create(ItemCategory::Material, 999).value()}; }, ItemCatalogErrorCode::InvalidSocketMaterial);
    Reject([](auto& s) { s.accessorySockets[0].permanentChargeMaterials = {ItemKey::Create(ItemCategory::Material, 999).value()}; }, ItemCatalogErrorCode::InvalidSocketMaterial);
    Reject([](auto& s) { s.accessorySockets[0].permanentChargeMaterials = {item_catalog_fixture::Leather()}; }, ItemCatalogErrorCode::InvalidSocketMaterial);
    Reject([](auto& s) { s.accessorySockets[0].permanentChargeMaterials = s.accessorySockets[0].chargeMaterials; }, ItemCatalogErrorCode::InvalidSocketProfile);
    Reject([](auto& s) { s.accessorySockets.push_back(s.accessorySockets[0]); }, ItemCatalogErrorCode::DuplicateProfile);
}
void DeterministicHash()
{
    const auto fixture = item_catalog_fixture::Catalog();
    const auto original = BuildItemCatalog(fixture).value()->ContentHash();
    auto reordered = fixture;
    std::ranges::reverse(reordered.items); std::ranges::reverse(reordered.refinements);
    std::ranges::reverse(reordered.appearances); std::ranges::reverse(reordered.ammoPolicies);
    std::ranges::reverse(reordered.bonuses);
    for (auto& profile : reordered.bonuses) std::ranges::reverse(profile.entries);
    std::ranges::reverse(reordered.accessories); std::ranges::reverse(reordered.materials);
    for (auto& profile : reordered.refinements) std::ranges::reverse(profile.levels);
    for (auto& profile : reordered.accessorySockets) std::ranges::reverse(profile.grades);
    Check(BuildItemCatalog(reordered).value()->ContentHash() == original, "source row order does not change canonical hash");
    reordered.revision = 99;
    Check(BuildItemCatalog(reordered).value()->ContentHash() == original, "revision is separate release identity");
    const std::vector<std::function<void(ItemCatalogSource&)>> changes {
        [](auto& s) { ++s.weapons[0].physicalAttackBaseRaw; }, [](auto& s) { ++s.armors[0].magicalDefenseBaseRaw; },
        [](auto& s) { ++s.items[0].weightGrams; }, [](auto& s) { ++s.items[0].buyAmount; },
        [](auto& s) { s.items[1].retired = true; }, [](auto& s) { s.items[0].iconAsset = "icons/changed.png"; },
        [](auto& s) { s.tradePolicies[0].allowTrade = false; }, [](auto& s) { ++s.attacks[0].reachMillimeters; },
        [](auto& s) { ++s.ammoPolicies[1].unitsPerHit; }, [](auto& s) { ++s.resistances[0].iceBp; },
        [](auto& s) { s.appearances[0].modelAsset = "models/changed.model"; },
        [](auto& s) { ++s.refinements[1].levels.back().gain.physicalAttackRaw; },
        [](auto& s) { ++s.bonuses[0].entries[0].valueScaled; },
        [](auto& s) { s.bonuses[2].code = "bonus.changed"; },
        [](auto& s) { s.refinements[1].levels[3].extraBonusProfile = 3; },
        [](auto& s) { s.items[0].baseBonusProfile.reset(); },
        [](auto& s) { s.accessories[0].slot = EquipmentSlot::Neck; },
        [](auto& s) { s.materials[0].kind = MaterialKind::Root; },
        [](auto& s) { ++s.items[3].maxStack; },
        [](auto& s) { s.accessories[0].socketProfile.reset(); },
        [](auto& s) { ++s.accessorySockets[0].grades[3].durationMs; },
        [](auto& s) { ++s.accessorySockets[0].grades[3].bonusIncreaseBp; },
        [](auto& s) { ++s.accessorySockets[0].grades[3].minimumRaw; },
        [](auto& s) { ++s.accessorySockets[0].grades[3].minimumBp; },
        [](auto& s) { ++s.accessorySockets[0].grades[3].minimumInteger; },
        [](auto& s) { ++s.accessorySockets[0].grades[0].unlockChanceBp; },
        [](auto& s) { --s.accessorySockets[0].grades[0].chargeChanceBp; },
        [](auto& s) { ++s.accessorySockets[0].unlockUnitsPerAttempt; },
        [](auto& s) { ++s.accessorySockets[0].chargeUnitsPerAttempt; },
        [](auto& s) { s.accessorySockets[0].unlockMaterials = {item_catalog_fixture::Leather()}; },
    };
    for (const auto& change : changes)
    {
        auto modified = fixture; change(modified);
        const auto rebuilt = BuildItemCatalog(std::move(modified));
        Check(rebuilt && rebuilt.value()->ContentHash() != original, "content change must change digest");
    }
}
void ExtendedRefinementKeepsOnePrototype()
{
    auto source = item_catalog_fixture::Catalog();
    auto& weapon = source.refinements[1];
    weapon.maxLevel = 30;
    for (std::uint16_t level = 10; level <= 30; ++level)
        weapon.levels.push_back({level, {10000LL * level, 4000LL * level, 0, 0}, {}});
    // Armor uses a different ceiling without changing the weapon's rules.
    auto& armor = source.refinements[2];
    armor.maxLevel = 15;
    for (std::uint16_t level = 3; level <= 15; ++level)
        armor.levels.push_back({level, {0, 0, 5000LL * level, 2000LL * level}, {}});
    const auto catalog = BuildItemCatalog(source).value();
    const auto sword = item_catalog_fixture::Sword(), suit = item_catalog_fixture::Armor();
    Check(catalog->Size() == item_catalog_fixture::FixtureItems, "higher refinement levels do not create additional prototypes");
    Check(catalog->RefinedBaseStats(sword, 10).value() == ItemBaseStats{220000, 70000, 0, 0}, "+10 stats resolved on the original ItemKey");
    Check(catalog->RefinedBaseStats(sword, 15).value() == ItemBaseStats{270000, 90000, 0, 0}, "+15 stats resolved on the original ItemKey");
    Check(catalog->RefinedBaseStats(sword, 30).value() == ItemBaseStats{420000, 150000, 0, 0}, "+30 stats resolved on the original ItemKey");
    Check(catalog->RefinedBaseStats(sword, 31).error() == ItemCatalogLookupError::InvalidRefinementLevel, "weapon profile controls its own maximum");
    Check(catalog->RefinedBaseStats(suit, 15).value() == ItemBaseStats{0, 0, 115000, 50000}, "armor independently supports fifteen");
    Check(catalog->RefinedBaseStats(suit, 16).error() == ItemCatalogLookupError::InvalidRefinementLevel, "armor does not borrow the weapon ceiling");
    Check(catalog->ContentHash() != BuildItemCatalog(item_catalog_fixture::Catalog()).value()->ContentHash(), "changed refinement policy changes catalog content identity");

    // Test-local holders model independent concrete copies. These are not a
    // replacement for the forthcoming ECS ownership/persistence components.
    struct HeldItem { ItemInstanceId id; ItemKey prototype; std::uint16_t level = 0; };
    HeldItem playerASword {ItemInstanceId::Create(1001).value(), sword};
    HeldItem playerBSword {ItemInstanceId::Create(1002).value(), sword};
    const auto initialHash = catalog->ContentHash();
    const auto* sharedPrototype = catalog->Find(sword);
    const auto* sharedProfile = catalog->FindRefinement(sharedPrototype->base.refinementProfile);
    Check(playerASword.id != playerBSword.id, "two concrete items have distinct persistent identities");
    Check(playerASword.prototype == playerBSword.prototype && playerASword.level == 0 && playerBSword.level == 0,
          "two newly acquired copies reference one prototype at level zero");
    for (unsigned step = 0; step < 30; ++step)
        playerASword.level = NextRefinementLevel(*sharedProfile, playerASword.level).value();
    Check(playerASword.level == 30 && playerBSword.level == 0, "refining one copy leaves the other copy at zero");
    Check(catalog->RefinedBaseStats(playerASword.prototype, playerASword.level).value() == ItemBaseStats{420000, 150000, 0, 0} &&
          catalog->RefinedBaseStats(playerBSword.prototype, playerBSword.level).value() == ItemBaseStats{120000, 30000, 0, 0},
          "one immutable prototype resolves different stats for independent instance levels");
    for (unsigned step = 0; step < 5; ++step)
        playerBSword.level = NextRefinementLevel(*sharedProfile, playerBSword.level).value();
    Check(playerBSword.level == 5 && playerASword.level == 30, "the second copy advances independently too");
    Check(catalog->RefinedBaseStats(playerBSword.prototype, playerBSword.level).value() == ItemBaseStats{170000, 50000, 0, 0},
          "the second copy uses its own level's gain");
    Check(playerASword.id.Value() == 1001 && playerBSword.id.Value() == 1002 && playerASword.prototype == sword && playerBSword.prototype == sword,
          "neither prototype key nor instance identity changes while refining");
    Check(catalog->ContentHash() == initialHash && catalog->Find(sword) == sharedPrototype && catalog->Size() == item_catalog_fixture::FixtureItems,
          "concrete copies do not mutate or duplicate catalog prototypes");

    source.refinements[1].levels.erase(source.refinements[1].levels.begin() + 10);
    Check(BuildItemCatalog(std::move(source)).error().code == ItemCatalogErrorCode::InvalidRefinement, "missing step ten cannot be skipped between old and extended levels");
}
void AccessoryAndMaterialProfiles()
{
    const auto catalog = BuildItemCatalog(item_catalog_fixture::Catalog()).value();
    const auto bracelet = item_catalog_fixture::Bracelet(), leather = item_catalog_fixture::Leather();
    Check(std::get<ItemAccessoryRow>(catalog->Find(bracelet)->category).slot == EquipmentSlot::Wrist, "accessory has its own typed category row");
    Check(std::get<ItemMaterialRow>(catalog->Find(leather)->category).kind == MaterialKind::Leather, "material has a typed kind, not a generic value array");
    Check(catalog->Find(leather)->base.maxStack == 200 && catalog->Find(bracelet)->base.maxStack == 1, "stack rules differ by actual category");
    const auto* profile = catalog->FindRefinement(catalog->Find(bracelet)->base.refinementProfile);
    Check(NextRefinementLevel(*profile, 9).value() == 10 && NextRefinementLevel(*profile, 15).error() == RefinementError::AtMaximumLevel,
          "accessory maximum is profile-defined beyond nine");
    const auto bonuses = catalog->BonusProfilesAt(bracelet, 15).value();
    Check(bonuses.base->Data().id == 6 && bonuses.refinement->Data().id == 5, "accessory base and current level bonuses are separate sources");
    Check(bonuses.base->Data().entries.front() == BonusEntry{BonusId::PhysicalDefenseFlat, 3000} &&
          bonuses.refinement->Data().entries.front() == BonusEntry{BonusId::PhysicalDefenseFlat, 6000}, "accessory stats are typed raw bonuses");
    Check(catalog->RefinedBaseStats(bracelet, 15).value() == ItemBaseStats{}, "bonus stats are not applied twice through equipment base stats");
    const auto* noRefine = catalog->FindRefinement(catalog->Find(leather)->base.refinementProfile);
    Check(NextRefinementLevel(*noRefine, 0).error() == RefinementError::AtMaximumLevel, "ordinary material is not refinable");
    Check(catalog->RefinedBaseStats(leather, 1).error() == ItemCatalogLookupError::InvalidRefinementLevel, "material cannot borrow equipment levels");
    Check(!catalog->BonusProfilesAt(leather, 0)->base && !catalog->BonusProfilesAt(leather, 0)->refinement, "material cannot add passive combat bonuses");
    for (auto slot : {EquipmentSlot::Wrist, EquipmentSlot::Neck, EquipmentSlot::Ear})
    {
        auto source = item_catalog_fixture::Catalog(); source.accessories[0].slot = slot;
        Check(BuildItemCatalog(std::move(source)).has_value(), "each supported jewelry slot publishes");
    }
    for (auto kind : {MaterialKind::Leather, MaterialKind::Blood, MaterialKind::Root, MaterialKind::Needle, MaterialKind::Jewel})
    {
        auto source = item_catalog_fixture::Catalog(); source.materials[0].kind = kind;
        Check(BuildItemCatalog(std::move(source)).has_value(), "each ordinary material kind publishes");
    }
    auto multiple = item_catalog_fixture::Catalog();
    auto otherAccessory = multiple.items[2];
    otherAccessory.key = ItemKey::Create(ItemCategory::Accessory, 2).value();
    otherAccessory.contentCode = "accessory.second";
    multiple.items.push_back(otherAccessory);
    multiple.accessories.push_back({otherAccessory.key, EquipmentSlot::Neck, 1});
    auto otherMaterial = multiple.items[3];
    otherMaterial.key = ItemKey::Create(ItemCategory::Material, 20).value();
    otherMaterial.contentCode = "material.second";
    multiple.items.push_back(otherMaterial);
    multiple.materials.push_back({otherMaterial.key, MaterialKind::Root});
    const auto hash = BuildItemCatalog(multiple).value()->ContentHash();
    std::ranges::reverse(multiple.items);
    std::ranges::reverse(multiple.accessories);
    std::ranges::reverse(multiple.materials);
    Check(BuildItemCatalog(std::move(multiple)).value()->ContentHash() == hash, "multiple accessory/material rows normalize independently of input order");
    auto source = item_catalog_fixture::Catalog(); source.items[3].maxStack = UINT32_MAX;
    Check(BuildItemCatalog(std::move(source)).has_value(), "stack limit retains its full explicit uint32 range");
    Check(catalog->FindAccessorySocket(1)->FindGrade(3)->durationMs == 21600000, "socket profile lifetime is catalog content");
    Check(!catalog->FindAccessorySocket(0) && !catalog->FindAccessorySocket(999), "unknown socket profile has no fallback");
    auto compatibility = item_catalog_fixture::Catalog();
    compatibility.materials[0].kind = MaterialKind::Jewel;
    compatibility.accessorySockets[0].unlockMaterials = {item_catalog_fixture::SocketDrill(), item_catalog_fixture::Leather()};
    compatibility.accessorySockets[0].chargeMaterials = {item_catalog_fixture::SocketJewel(), item_catalog_fixture::Leather()};
    const auto orderedHash = BuildItemCatalog(compatibility).value()->ContentHash();
    std::ranges::reverse(compatibility.accessorySockets[0].unlockMaterials);
    std::ranges::reverse(compatibility.accessorySockets[0].chargeMaterials);
    std::ranges::reverse(compatibility.accessorySockets[0].grades);
    Check(BuildItemCatalog(std::move(compatibility)).value()->ContentHash() == orderedHash, "socket compatibility and grade order is canonical");
    auto permanent = item_catalog_fixture::Catalog();
    permanent.materials[0].kind = MaterialKind::Jewel;
    const auto timedOnlyHash = BuildItemCatalog(permanent).value()->ContentHash();
    permanent.accessorySockets[0].permanentChargeMaterials = {item_catalog_fixture::Leather()};
    const auto permanentCatalog = BuildItemCatalog(permanent).value();
    Check(permanentCatalog->ContentHash() != timedOnlyHash, "permanent material compatibility participates in catalog identity");
    Check(permanentCatalog->FindAccessorySocket(1)->Data().permanentChargeMaterials == std::vector<ItemKey>{item_catalog_fixture::Leather()},
          "permanent material resolves to an actual typed Jewel row");
}
}
int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string_view(argv[1]) == "--hash-fixture")
        {
            std::cout << Hex(BuildItemCatalog(item_catalog_fixture::Catalog()).value()->ContentHash()) << '\n'; return 0;
        }
        ValidSnapshot(); InvalidCatalogs(); DeterministicHash(); ExtendedRefinementKeepsOnePrototype(); AccessoryAndMaterialProfiles();
        std::cout << "Catalog checks passed: " << checks << '\n'; return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
