#pragma once
#include <item/ItemCatalog.hpp>
#include <tuple>

namespace item_catalog_fixture
{
using namespace auriga::item;
inline ItemKey Sword() { return ItemKey::Create(ItemCategory::Weapon, 1).value(); }
inline ItemKey Armor() { return ItemKey::Create(ItemCategory::Armor, 1).value(); }
inline ItemKey Bracelet() { return ItemKey::Create(ItemCategory::Accessory, 1).value(); }
inline ItemKey Leather() { return ItemKey::Create(ItemCategory::Material, 1).value(); }
inline ItemKey SocketJewel() { return ItemKey::Create(ItemCategory::Material, 2).value(); }
inline ItemKey SocketDrill() { return ItemKey::Create(ItemCategory::Material, 3).value(); }
inline ItemKey InstantPotion() { return ItemKey::Create(ItemCategory::Consumable, 1).value(); }
inline ItemKey ReservePotion() { return ItemKey::Create(ItemCategory::Consumable, 2).value(); }
inline ItemKey PeriodicPotion() { return ItemKey::Create(ItemCategory::Consumable, 3).value(); }
inline ItemKey GiftBox() { return ItemKey::Create(ItemCategory::Container, 1).value(); }
inline ItemKey IronChest() { return ItemKey::Create(ItemCategory::Container, 2).value(); }
inline ItemKey QuestBox() { return ItemKey::Create(ItemCategory::Container, 3).value(); }
inline ItemKey IronKey() { return ItemKey::Create(ItemCategory::ContainerKey, 1).value(); }
inline ItemKey GoldBar() { return ItemKey::Create(ItemCategory::CurrencyToken, 1).value(); }
inline ItemKey CoinVoucher() { return ItemKey::Create(ItemCategory::CurrencyToken, 2).value(); }
inline constexpr std::size_t FixtureItems = 15;
inline AccessorySocketProfile SocketProfile()
{
    return {1, "socket.copper", 3, 1, 1,
        {{0, 0, 0, 0, 0, 0, 5000, 10000},
         {1, 86400000, 1000, 1000, 100, 1, 5000, 10000},
         {2, 43200000, 2000, 2000, 200, 2, 5000, 10000},
         {3, 21600000, 4000, 3000, 300, 3, 0, 0}}, {SocketDrill()}, {SocketJewel()}};
}
inline RefinementProfile WeaponRefinement()
{
    RefinementProfile profile {2, "refine.weapon_linear", RefinementKind::Weapon, 9, {}};
    for (std::uint16_t i = 0; i <= 9; ++i)
        profile.levels.push_back({i, {10'000LL * i, 4'000LL * i, 0, 0}, {}});
    return profile;
}
inline ItemCatalogSource Catalog()
{
    ItemCatalogSource source;
    source.revision = 1;
    source.usePolicies = {{1, "use.equipment", 15, 3, true}};
    source.tradePolicies = {{1, "trade.unbound", true, true, true}};
    source.appearances = {{1, "appearance.sword", "models/iron_sword.model"}, {2, "appearance.armor", "models/iron_armor.model"}};
    source.attacks = {{1, "attack.melee", 25, 10, 3000, 1}};
    source.ammoPolicies = {{1, "ammo.none", AmmoKind::None, 0}, {2, "ammo.arrow", AmmoKind::ConsumeStack, 1}};
    source.resistances = {{1, "resistance.none"}};
    source.bonuses = {
        {1, "bonus.sword_base", BonusSource::ItemBase, {{BonusId::NormalOutgoing, 500}, {BonusId::PhysicalAttackFlat, 2500}}},
        {2, "bonus.refinement_three", BonusSource::Refinement, {{BonusId::CritChance, 100}, {BonusId::PhysicalAttackFlat, 5000}}},
        {3, "bonus.refinement_nine", BonusSource::Refinement, {{BonusId::CritChance, 200}}},
        {4, "bonus.accessory_low", BonusSource::Refinement, {{BonusId::PhysicalDefenseFlat, 2000}}},
        {5, "bonus.accessory_high", BonusSource::Refinement, {{BonusId::PhysicalDefenseFlat, 6000}}},
        {6, "bonus.accessory_base", BonusSource::ItemBase, {{BonusId::PhysicalDefenseFlat, 3000}}},
    };
    source.refinements = {{1, "refine.none", RefinementKind::None, 0, {{0, {}, {}}}}, WeaponRefinement(),
                          {3, "refine.armor", RefinementKind::Armor, 2, {{0, {}, {}}, {1, {0, 0, 5000, 2000}, {}}, {2, {0, 0, 10000, 4000}, {}}}}};
    source.refinements[1].levels[3].extraBonusProfile = 2;
    source.refinements[1].levels[9].extraBonusProfile = 3;
    RefinementProfile accessory {4, "refine.accessory", RefinementKind::Accessory, 15, {{0, {}, {}}}};
    for (std::uint16_t level = 1; level <= 15; ++level)
        accessory.levels.push_back({level, {}, level == 15 ? 5u : 4u});
    source.refinements.push_back(std::move(accessory));
    ItemBaseRow sword {Sword()};
    sword.contentCode = "weapon.iron_sword"; sword.nameKey = "item.iron_sword.name"; sword.descriptionKey = "item.iron_sword.description";
    sword.iconAsset = "icons/iron_sword.png"; sword.height = 3; sword.weightGrams = 1200;
    sword.minimumLevel = 1; sword.usePolicy = 1; sword.tradePolicy = 1; sword.refinementProfile = 2; sword.appearanceProfile = 1;
    sword.buyAmount = 100; sword.sellAmount = 50;
    sword.baseBonusProfile = 1;
    ItemBaseRow armor {Armor()};
    armor.contentCode = "armor.iron_armor"; armor.nameKey = "item.iron_armor.name"; armor.descriptionKey = "item.iron_armor.description";
    armor.iconAsset = "icons/iron_armor.png"; armor.height = 2; armor.weightGrams = 5000;
    armor.minimumLevel = 5; armor.usePolicy = 1; armor.tradePolicy = 1; armor.refinementProfile = 3; armor.appearanceProfile = 2;
    armor.buyAmount = 200; armor.sellAmount = 100;
    ItemBaseRow bracelet {Bracelet()};
    bracelet.contentCode = "accessory.copper_bracelet"; bracelet.nameKey = "item.copper_bracelet.name";
    bracelet.descriptionKey = "item.copper_bracelet.description"; bracelet.iconAsset = "icons/copper_bracelet.png";
    bracelet.weightGrams = 80; bracelet.usePolicy = 1; bracelet.tradePolicy = 1; bracelet.refinementProfile = 4; bracelet.baseBonusProfile = 6;
    ItemBaseRow leather {Leather()};
    leather.contentCode = "material.leather"; leather.nameKey = "item.leather.name";
    leather.descriptionKey = "item.leather.description"; leather.iconAsset = "icons/leather.png";
    leather.maxStack = 200; leather.weightGrams = 10; leather.usePolicy = 2; leather.tradePolicy = 1; leather.refinementProfile = 1;
    source.usePolicies.push_back({2, "use.material", 15, 3, false});
    ItemBaseRow jewel {SocketJewel()};
    jewel.contentCode = "material.amber"; jewel.nameKey = "item.amber.name";
    jewel.descriptionKey = "item.amber.description"; jewel.iconAsset = "icons/amber.png";
    jewel.maxStack = 200; jewel.weightGrams = 5; jewel.usePolicy = 2; jewel.tradePolicy = 1; jewel.refinementProfile = 1;
    ItemBaseRow drill {SocketDrill()};
    drill.contentCode = "material.socket_drill"; drill.nameKey = "item.socket_drill.name";
    drill.descriptionKey = "item.socket_drill.description"; drill.iconAsset = "icons/socket_drill.png";
    drill.maxStack = 20; drill.weightGrams = 30; drill.usePolicy = 2; drill.tradePolicy = 1; drill.refinementProfile = 1;
    source.items = {sword, armor, bracelet, leather, jewel, drill};
    source.weapons = {{Sword(), WeaponSubtype::Sword, EquipmentSlot::Weapon, 120000, 30000, 1, 1}};
    source.armors = {{Armor(), EquipmentSlot::Body, 40000, 20000, 1}};
    source.accessories = {{Bracelet(), EquipmentSlot::Wrist, 1, 1}};
    source.materials = {{Leather(), MaterialKind::Leather}, {SocketJewel(), MaterialKind::Jewel}, {SocketDrill(), MaterialKind::Needle}};
    source.accessorySockets = {SocketProfile()};
    source.usePolicies.push_back({3, "use.recovery", 15, 3, true});
    source.consumableCooldowns = {{1, "cooldown.recovery", 1000}};
    source.recoveries = {
        {1, "recovery.instant", 1, 15, true, 1, InstantRecovery{300000,100000,500,1000,10000}},
        {2, "recovery.reserve", 1, 9, false, 1, ReserveRecovery{500000,200000,10000,1000,700}},
        {3, "recovery.periodic", 1, 15, false, {}, PeriodicRecovery{25000,0,1000,60000}},
    };
    for (auto [key, code, profile] : {std::tuple{InstantPotion(), "instant_flask", 1u},
            {ReservePotion(), "reserve_flask", 2u}, {PeriodicPotion(), "regeneration_flask", 3u}})
    {
        ItemBaseRow potion {key};
        potion.contentCode = std::string("consumable.") + code; potion.nameKey = std::string("item.") + code + ".name";
        potion.descriptionKey = std::string("item.") + code + ".description"; potion.iconAsset = std::string("icons/") + code + ".png";
        potion.maxStack = 200; potion.weightGrams = 50; potion.minimumLevel = 10;
        potion.usePolicy = 3; potion.tradePolicy = 1; potion.refinementProfile = 1;
        source.items.push_back(std::move(potion)); source.consumables.push_back({key, profile});
    }
    source.lootProfiles = {
        {1, "loot.gift_box", LootPickMode::WeightedOne, {
            {1, LootRewardKind::Item, Leather(), 5, 70, 0},
            {2, LootRewardKind::Gold, {}, 1000, 20, 0},
            {3, LootRewardKind::Item, Sword(), 1, 10, 500}}},
        {2, "loot.iron_chest", LootPickMode::IndependentChance, {
            {1, LootRewardKind::Item, ReservePotion(), 3, 10000, 0},
            {2, LootRewardKind::Experience, {}, 5000, 2500, 0},
            {3, LootRewardKind::SpawnMonsterGroup, {}, 101, 500, 0},
            {4, LootRewardKind::Poison, {}, 0, 100, 0}}},
    };
    for (auto [key, code, stack] : {std::tuple{GiftBox(), "container.gift_box", 50u}, {IronChest(), "container.iron_chest", 10u},
            {QuestBox(), "container.quest_box", 1u}, {IronKey(), "container_key.iron_key", 20u},
            {GoldBar(), "currency_token.gold_bar", 200u}, {CoinVoucher(), "currency_token.coin_voucher", 200u}})
    {
        ItemBaseRow row {key};
        const std::string name = std::string(code).substr(std::string(code).find('.') + 1);
        row.contentCode = code; row.nameKey = "item." + name + ".name"; row.descriptionKey = "item." + name + ".description";
        row.iconAsset = "icons/" + name + ".png"; row.maxStack = stack; row.weightGrams = 100;
        row.usePolicy = 2; row.tradePolicy = 1; row.refinementProfile = 1;
        source.items.push_back(std::move(row));
    }
    source.containers = {{GiftBox(), ContainerKind::Use, 1, 0}, {IronChest(), ContainerKind::Locked, 2, 7}, {QuestBox(), ContainerKind::Scripted, {}, 0}};
    source.containerKeys = {{IronKey(), 7}};
    source.currencyTokens = {{GoldBar(), WalletCurrency::Gold, 5'000'000}, {CoinVoucher(), WalletCurrency::DragonCoin, 100}};
    return source;
}
}
