#include "ItemCatalogFixture.hpp"
#include <deque>
#include <iostream>
#include <source_location>
#include <stdexcept>

using namespace auriga::item;
namespace f = item_catalog_fixture;
namespace
{
std::size_t checks = 0;
void Check(bool condition, const char* message, const std::source_location where = std::source_location::current())
{
    ++checks;
    if (!condition) throw std::runtime_error(std::string(message) + " at line " + std::to_string(where.line()));
}
// Scripted uniform source: replays exactly the given draws and records bounds.
struct Script
{
    std::deque<std::uint32_t> values;
    std::vector<std::uint32_t> bounds;
    LootDraw Draw()
    {
        return [this](std::uint32_t bound) {
            bounds.push_back(bound);
            const auto value = values.front(); values.pop_front(); return value;
        };
    }
};
template <typename Mutate> void RejectProfile(std::size_t index, Mutate mutate, LootProfileError expected)
{
    auto source = f::Catalog().lootProfiles[index]; mutate(source);
    const auto result = ValidateLootProfile(std::move(source));
    Check(!result && result.error() == expected, "invalid loot profile rejected with its reason");
}
template <typename Mutate> void RejectCatalog(Mutate mutate, ItemCatalogErrorCode expected)
{
    auto source = f::Catalog(); mutate(source);
    const auto result = BuildItemCatalog(std::move(source));
    Check(!result && result.error().code == expected, "invalid container catalogue rejected with its reason");
}
void Profiles()
{
    const auto fixture = f::Catalog();
    const auto weighted = ValidateLootProfile(fixture.lootProfiles[0]).value();
    Check(weighted.TotalWeight() == 100, "weighted total is the sum of relative weights");
    auto shuffled = fixture.lootProfiles[0];
    std::swap(shuffled.entries[0], shuffled.entries[2]);
    Check(ValidateLootProfile(shuffled).value().Data().entries[0].entry == 1, "entries are normalized to their draw order");
    RejectProfile(0, [](auto& p) { p.id = 0; }, LootProfileError::InvalidIdentity);
    RejectProfile(0, [](auto& p) { p.code = "Bad Code"; }, LootProfileError::InvalidIdentity);
    RejectProfile(0, [](auto& p) { p.mode = static_cast<LootPickMode>(3); }, LootProfileError::InvalidMode);
    RejectProfile(0, [](auto& p) { p.entries.clear(); }, LootProfileError::EmptyProfile);
    RejectProfile(0, [](auto& p) { p.entries.resize(MaxLootEntries + 1, p.entries[1]); }, LootProfileError::TooManyEntries);
    RejectProfile(0, [](auto& p) { p.entries[2].entry = 5; }, LootProfileError::InvalidEntryOrder);
    RejectProfile(0, [](auto& p) { p.entries[1].entry = 1; }, LootProfileError::InvalidEntryOrder);
    RejectProfile(0, [](auto& p) { p.entries[0].item.reset(); }, LootProfileError::InvalidReward);
    RejectProfile(0, [](auto& p) { p.entries[1].item = f::Leather(); }, LootProfileError::InvalidReward);
    RejectProfile(0, [](auto& p) { p.entries[1].kind = static_cast<LootRewardKind>(9); }, LootProfileError::InvalidReward);
    RejectProfile(0, [](auto& p) { p.entries[0].amount = 0; }, LootProfileError::InvalidAmount);
    RejectProfile(0, [](auto& p) { p.entries[1].amount = MaxLootCurrency + 1; }, LootProfileError::InvalidAmount);
    RejectProfile(1, [](auto& p) { p.entries[3].amount = 1; }, LootProfileError::InvalidAmount);
    RejectProfile(1, [](auto& p) { p.entries[1] = {2, LootRewardKind::DrainHp, {}, 10001, 100, 0}; }, LootProfileError::InvalidAmount);
    RejectProfile(0, [](auto& p) { p.entries[0].weight = 0; }, LootProfileError::InvalidWeight);
    RejectProfile(1, [](auto& p) { p.entries[0].weight = LootChanceScaleBp + 1; }, LootProfileError::InvalidWeight);
    RejectProfile(0, [](auto& p) { p.entries[2].rareAttributeChanceBp = 10001; }, LootProfileError::InvalidRareChance);
    RejectProfile(0, [](auto& p) { p.entries[1].rareAttributeChanceBp = 1; }, LootProfileError::InvalidRareChance);
    RejectProfile(0, [](auto& p) { p.entries[0].weight = UINT32_MAX; }, LootProfileError::WeightOverflow);
}
void Picking()
{
    const auto fixture = f::Catalog();
    const auto weighted = ValidateLootProfile(fixture.lootProfiles[0]).value();
    for (auto [draw, index] : {std::pair{0u, 0u}, {69u, 0u}, {70u, 1u}, {89u, 1u}, {90u, 2u}, {99u, 2u}})
    {
        Script script{{draw}};
        const auto picked = PickLoot(weighted, script.Draw()).value();
        Check(picked.size() == 1 && picked[0] == index && script.bounds == std::vector<std::uint32_t>{100}, "weighted draw boundaries");
    }
    Script outside{{100}};
    Check(PickLoot(weighted, outside.Draw()).error() == LootPickError::DrawOutOfRange, "out-of-range draw is not wrapped");
    const auto chances = ValidateLootProfile(fixture.lootProfiles[1]).value();
    Script all{{0, 2499, 499, 99}};
    Check(PickLoot(chances, all.Draw()).value() == std::vector<std::size_t>{0, 1, 2, 3} && all.bounds.size() == 4,
          "independent chances roll once per entry");
    Script none{{9999, 2500, 500, 100}};
    // The guaranteed 100% entry still wins; the others miss at their exact chance.
    Check(PickLoot(chances, none.Draw()).value() == std::vector<std::size_t>{0}, "chance boundaries are exclusive");
    auto rare = fixture.lootProfiles[1];
    rare.entries[0].weight = 1;
    Script empty{{1, 9999, 9999, 9999}};
    Check(PickLoot(ValidateLootProfile(rare).value(), empty.Draw()).value().empty(), "independent profile may yield nothing");
}
void Opening()
{
    const auto catalog = BuildItemCatalog(f::Catalog()).value();
    Check(catalog->Size() == f::FixtureItems, "containers and keys are published prototypes");
    Script gift{{95}};
    const auto plan = PlanContainerOpen(*catalog, {f::GiftBox(), {}, 3, 0}, gift.Draw()).value();
    Check(plan.containerUnitsToConsume == 1 && !plan.key && plan.keyUnitsToConsume == 0, "a use box consumes one unit and no key");
    Check(plan.rewards.size() == 1 && plan.rewards[0].item == f::Sword() && plan.rewards[0].rareAttributeChanceBp == 500,
          "the plan owns the drawn reward and its rare chance");
    Script any{{0, 0, 0, 0}};
    Check(PlanContainerOpen(*catalog, {f::GiftBox(), f::IronKey(), 1, 1}, any.Draw()).error() == ContainerOpenError::KeyNotAccepted,
          "a use box is not opened with a key");
    Check(PlanContainerOpen(*catalog, {f::GiftBox(), {}, 0, 0}, any.Draw()).error() == ContainerOpenError::InsufficientUnits, "empty stack");
    Check(PlanContainerOpen(*catalog, {f::IronChest(), {}, 1, 0}, any.Draw()).error() == ContainerOpenError::KeyRequired,
          "a locked chest needs a key");
    Check(PlanContainerOpen(*catalog, {f::IronChest(), f::Leather(), 1, 1}, any.Draw()).error() == ContainerOpenError::NotKey,
          "only a typed key opens a chest");
    Check(PlanContainerOpen(*catalog, {f::IronChest(), f::IronKey(), 1, 0}, any.Draw()).error() == ContainerOpenError::InsufficientUnits,
          "the key stack must hold a unit");
    Script chest{{0, 2500, 0, 9999}};
    const auto opened = PlanContainerOpen(*catalog, {f::IronChest(), f::IronKey(), 1, 1}, chest.Draw()).value();
    Check(opened.key == f::IronKey() && opened.keyUnitsToConsume == 1 && opened.rewards.size() == 2 &&
          opened.rewards[0].item == f::ReservePotion() && opened.rewards[1].kind == LootRewardKind::SpawnMonsterGroup,
          "matching key opens the chest with every rolled reward");
    Check(PlanContainerOpen(*catalog, {f::QuestBox(), {}, 1, 0}, any.Draw()).error() == ContainerOpenError::ScriptedContainer,
          "a scripted box is opened by its quest, not by use");
    Check(PlanContainerOpen(*catalog, {f::Sword(), {}, 1, 0}, any.Draw()).error() == ContainerOpenError::NotContainer, "not a box");
    Check(PlanContainerOpen(*catalog, {ItemKey::Create(ItemCategory::Container, 99).value(), {}, 1, 0}, any.Draw()).error() ==
          ContainerOpenError::MissingItem, "unknown box");
    auto wrong = f::Catalog();
    wrong.containerKeys[0].lockId = 8;
    Check(PlanContainerOpen(*BuildItemCatalog(std::move(wrong)).value(), {f::IronChest(), f::IronKey(), 1, 1}, any.Draw()).error() ==
          ContainerOpenError::WrongKey, "a different lock id is refused");
    auto retired = f::Catalog();
    for (auto& row : retired.items) if (row.key == f::GiftBox()) row.retired = true;
    Check(PlanContainerOpen(*BuildItemCatalog(std::move(retired)).value(), {f::GiftBox(), {}, 1, 0}, any.Draw()).error() ==
          ContainerOpenError::RetiredItem, "retired boxes are not opened");
}
void CatalogRules()
{
    RejectCatalog([](auto& s) { s.containers[0].lootProfile.reset(); }, ItemCatalogErrorCode::InvalidContainer);
    RejectCatalog([](auto& s) { s.containers[2].lootProfile = 1; }, ItemCatalogErrorCode::InvalidContainer);
    RejectCatalog([](auto& s) { s.containers[1].lockId = 0; }, ItemCatalogErrorCode::InvalidContainer);
    RejectCatalog([](auto& s) { s.containers[0].lockId = 3; }, ItemCatalogErrorCode::InvalidContainer);
    RejectCatalog([](auto& s) { s.containers[0].kind = static_cast<ContainerKind>(4); }, ItemCatalogErrorCode::InvalidContainer);
    RejectCatalog([](auto& s) { s.containers[0].lootProfile = 9; }, ItemCatalogErrorCode::MissingProfile);
    RejectCatalog([](auto& s) { s.containerKeys[0].lockId = 0; }, ItemCatalogErrorCode::InvalidContainer);
    RejectCatalog([](auto& s) { s.containers.pop_back(); }, ItemCatalogErrorCode::MissingCategoryRow);
    RejectCatalog([](auto& s) { s.containerKeys.clear(); }, ItemCatalogErrorCode::MissingCategoryRow);
    RejectCatalog([](auto& s) { s.containers.push_back(s.containers[0]); }, ItemCatalogErrorCode::DuplicateCategoryRow);
    RejectCatalog([](auto& s) { s.containers[0].key = f::IronKey(); }, ItemCatalogErrorCode::WrongCategory);
    RejectCatalog([](auto& s) { s.containerKeys.push_back({ItemKey::Create(ItemCategory::ContainerKey, 9).value(), 1}); },
                  ItemCatalogErrorCode::OrphanCategoryRow);
    RejectCatalog([](auto& s) { s.lootProfiles[0].entries[0].amount = 201; }, ItemCatalogErrorCode::InvalidLootReward);
    RejectCatalog([](auto& s) { s.lootProfiles[0].entries[2].amount = 2; }, ItemCatalogErrorCode::InvalidLootReward);
    RejectCatalog([](auto& s) { s.lootProfiles[0].entries[0].item = ItemKey::Create(ItemCategory::Material, 99).value(); },
                  ItemCatalogErrorCode::InvalidLootReward);
    RejectCatalog([](auto& s) { for (auto& row : s.items) if (row.key == f::Leather()) row.retired = true; },
                  ItemCatalogErrorCode::InvalidLootReward);
    RejectCatalog([](auto& s) { s.lootProfiles[1].entries[0].weight = 0; }, ItemCatalogErrorCode::InvalidLootProfile);
    RejectCatalog([](auto& s) { s.lootProfiles[1].code = s.lootProfiles[0].code; }, ItemCatalogErrorCode::DuplicateProfile);
    RejectCatalog([](auto& s) { for (auto& row : s.items) if (row.key == f::GiftBox()) row.binding = BindingPolicy::OnEquip; },
                  ItemCatalogErrorCode::InvalidBase);
    RejectCatalog([](auto& s) { for (auto& row : s.items) if (row.key == f::IronKey()) row.baseBonusProfile = 1; },
                  ItemCatalogErrorCode::InvalidBase);
    RejectCatalog([](auto& s) { for (auto& row : s.items) if (row.key == f::GiftBox()) row.refinementProfile = 2; },
                  ItemCatalogErrorCode::WrongRefinementKind);
    // A loot profile shared by two boxes, and a box rewarding another box, are valid content.
    auto shared = f::Catalog();
    shared.containers[1].lootProfile = 1;
    shared.lootProfiles[0].entries.push_back({4, LootRewardKind::Item, f::GiftBox(), 2, 5, 0});
    const auto catalog = BuildItemCatalog(std::move(shared));
    Check(catalog.has_value() && (*catalog)->FindLoot(1)->TotalWeight() == 105, "shared and nested container content is valid");
    const auto original = BuildItemCatalog(f::Catalog()).value()->ContentHash();
    auto changed = f::Catalog(); changed.lootProfiles[0].entries[1].weight = 21;
    Check(BuildItemCatalog(std::move(changed)).value()->ContentHash() != original, "loot weights participate in catalogue identity");
    auto relock = f::Catalog(); relock.containers[1].lockId = 8; relock.containerKeys[0].lockId = 8;
    Check(BuildItemCatalog(std::move(relock)).value()->ContentHash() != original, "lock ids participate in catalogue identity");
}
}
int main()
{
    try
    {
        Profiles(); Picking(); Opening(); CatalogRules();
        std::cout << "Container checks passed: " << checks << '\n'; return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
