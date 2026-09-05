#include "../../SRC/Server/GameServer/stdafx.h"
#include "../../SRC/Server/GameServer/ecs/systems/ItemSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/NetworkSyncSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PointSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SocialSystem.hpp"
#include "../../SRC/Server/GameServer/new_switchbot.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/components/item_proto_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/inventory_components.hpp"
#include "../../SRC/Server/GameServer/ecs/detail/ItemAttributeRules.hpp"
#include "../../SRC/Server/GameServer/constants.h"
#include "../../SRC/Server/GameServer/log.h"
#include "../../SRC/Server/GameServer/char.h"
#include "../../SRC/Server/GameServer/char_manager.h"
#include "../../SRC/Server/GameServer/desc.h"
#include "../../SRC/Server/GameServer/buffer_manager.h"
#include "../../SRC/Server/GameServer/p2p.h"
#include "../../SRC/Server/GameServer/battle_pass.h"
#include <Core/Logging.hpp>
#include <stdexcept>
#include <iostream>
#include <cstdlib>

entt::registry g_registry;
TItemAttrMap g_map_itemAttr;
TItemAttrMap g_map_itemRare;
int g_iDbLogLevel = 0;
const int aiItemMagicAttributePercentHigh[ITEM_ATTRIBUTE_MAX_LEVEL] = {0, 0, 0, 0, 100};
const int aiItemMagicAttributePercentLow[ITEM_ATTRIBUTE_MAX_LEVEL] = {100, 0, 0, 0, 0};

namespace {
int saves = 0;
int updates = 0;
int randomCalls = 0;
int checks = 0;
int payments = 0;
bool rejectPayment = false;
bool rejectGoldPayment = false;
entt::entity watchedItem = entt::null;
std::array<TPlayerItemAttribute, ITEM_ATTRIBUTE_MAX_NUM> beforePayment{};
std::map<std::tuple<entt::entity, uint8_t, uint16_t>, entt::entity> inventory;
struct TestPlayer { int64_t gold = 100; };
void Check(bool condition, const char* message)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}

bool EqualAttributes(const auto& left, const auto& right)
{
    for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
        if (left[i].bType != right[i].bType || left[i].sValue != right[i].sValue)
            return false;
    return true;
}

void CheckPaymentOrder()
{
    Check(watchedItem != entt::null && g_registry.valid(watchedItem), "payment has no live target");
    Check(EqualAttributes(g_registry.get<ecs::ItemAttributes>(watchedItem).attrs, beforePayment),
        "attributes changed before payment succeeded");
    Check(saves == 0 && updates == 0, "partial attributes published before payment");
}

[[noreturn]] void UnexpectedSwitchbotService()
{
    std::cerr << "Unexpected timer/UI/manager service call in a transaction test\n";
    std::abort();
}
}

// The entire production new_switchbot.cpp is compiled, not a copied test-only
// slice. These dependencies belong to the timer/UI/manager paths outside the
// transaction tests; fail immediately if the tests accidentally enter them.
int passes_per_sec = 25;
std::shared_ptr<spdlog::logger> logging::GetErrorLogger() { UnexpectedSwitchbotService(); }
void intrusive_ptr_add_ref(EVENT*) { UnexpectedSwitchbotService(); }
void intrusive_ptr_release(EVENT*) { UnexpectedSwitchbotService(); }
LPEVENT event_create_ex(TEVENTFUNC, event_info_data*, int32_t) { UnexpectedSwitchbotService(); }
void event_cancel(LPEVENT*) { UnexpectedSwitchbotService(); }
#ifdef TEXTS_IMPROVEMENT
void ecs::ChatSystem::SendNew(entt::entity, uint8_t, uint32_t, const char*, ...) { UnexpectedSwitchbotService(); }
#endif
uint32_t ecs::PlayerRuntime::GetPlayerID(entt::entity) { UnexpectedSwitchbotService(); }
std::string_view ecs::PlayerRuntime::GetName(entt::entity) { UnexpectedSwitchbotService(); }
#ifdef ENABLE_BATTLE_PASS
uint8_t ecs::PlayerRuntime::GetBattlePassId(entt::entity) { UnexpectedSwitchbotService(); }
uint32_t ecs::PlayerRuntime::GetMissionProgress(entt::entity, uint32_t, uint32_t) { UnexpectedSwitchbotService(); }
bool ecs::PlayerRuntime::UpdateMissionProgress(entt::entity, uint32_t, uint32_t, uint32_t, uint32_t, bool) { UnexpectedSwitchbotService(); }
bool CBattlePass::BattlePassMissionGetInfo(uint8_t, uint8_t, uint32_t*, uint32_t*) { UnexpectedSwitchbotService(); }
#endif
#ifdef ENABLE_RANKING
int64_t ecs::PlayerRuntime::GetRankPoints(entt::entity, int) { UnexpectedSwitchbotService(); }
bool ecs::PlayerRuntime::SetRankPoints(entt::entity, int, int64_t) { UnexpectedSwitchbotService(); }
#endif
void DESC::BufferedPacket(const void*, int) { UnexpectedSwitchbotService(); }
void DESC::Packet(const void*, int) { UnexpectedSwitchbotService(); }
void BroadcastNotice(const char*, bool) { UnexpectedSwitchbotService(); }
TEMP_BUFFER::TEMP_BUFFER(int, bool) { UnexpectedSwitchbotService(); }
TEMP_BUFFER::~TEMP_BUFFER() = default; // The fail-fast constructor never creates a buffer.
const void* TEMP_BUFFER::read_peek() { UnexpectedSwitchbotService(); }
void TEMP_BUFFER::write(const void*, int) { UnexpectedSwitchbotService(); }
int TEMP_BUFFER::size() { UnexpectedSwitchbotService(); }
entt::entity CHARACTER_MANAGER::FindEntityByPID(uint32_t) { UnexpectedSwitchbotService(); }
void P2P_MANAGER::Send(const void*, int, LPDESC) { UnexpectedSwitchbotService(); }
entt::entity ItemSystem::FindItemByID(uint32_t) { UnexpectedSwitchbotService(); }
const char* ItemSystem::GetItemName(entt::entity) { UnexpectedSwitchbotService(); }
entt::entity ItemSystem::GetItemOwnerEntity(entt::entity) { UnexpectedSwitchbotService(); }
int ItemSystem::GetItemAttributeType(entt::entity, int) { UnexpectedSwitchbotService(); }
int ItemSystem::GetItemAttributeValue(entt::entity, int) { UnexpectedSwitchbotService(); }

// Deterministic I/O doubles. The system under test never constructs CItem or
// CHARACTER, and fixtures deliberately contain no LegacyItemPtr component.
int number_ex(int low, int high, const char*, int)
{
    Check(low <= high, "invalid random interval");
    ++randomCalls;
    return low;
}
float gauss_random(float, float) { return 4.0f; }
void LogManager::ItemLog(uint32_t, uint32_t, uint32_t, uint32_t, const char*, const char*, const char*, uint32_t) {}
void LogManager::ItemLogEntity(entt::entity, entt::entity, const char*, const char*) {}
LPDESC ecs::PlayerRuntime::GetDesc(entt::entity) { return nullptr; }
bool ecs::PlayerRuntime::IsValid(entt::entity e) { return e != entt::null && g_registry.valid(e); }
bool ecs::PlayerRuntime::IsPC(entt::entity e) { return IsValid(e) && g_registry.all_of<TestPlayer>(e); }
CShop* ecs::SocialSystem::GetMyShop(entt::entity) { return nullptr; }
int64_t ecs::PointSystem::GetGold(entt::entity e) { return g_registry.get<TestPlayer>(e).gold; }
void ecs::PointSystem::Change(entt::entity e, uint8_t type, int64_t amount, bool, bool
#ifdef __ENABLE_BLOCK_EXP__
    , bool
#endif
)
{
    CheckPaymentOrder();
    Check(type == POINT_GOLD && amount < 0, "unexpected currency operation");
    ++payments;
    if (!rejectGoldPayment)
        g_registry.get<TestPlayer>(e).gold += amount;
}
void ecs::ItemNetworkSystem::SendItemUpdate(entt::registry&, entt::entity) { ++updates; }

namespace ItemSystem {
bool IsValidItem(entt::entity item)
{
    return item != entt::null && g_registry.valid(item) && g_registry.all_of<ecs::ItemIdentity>(item);
}
const TItemTable* GetItemProto(entt::entity item)
{
    const auto* ref = g_registry.try_get<ecs::ItemProtoRef>(item);
    return ref ? ref->proto : nullptr;
}
uint8_t GetItemType(entt::entity item) { const auto* p = GetItemProto(item); return p ? p->bType : 0; }
uint8_t GetItemSubType(entt::entity item) { const auto* p = GetItemProto(item); return p ? p->bSubType : 0; }
uint32_t GetItemWearFlags(entt::entity item) { const auto* p = GetItemProto(item); return p ? p->dwWearFlags : 0; }
uint32_t GetItemVnum(entt::entity item) { return g_registry.get<ecs::ItemIdentity>(item).vnum; }
uint32_t GetItemOriginalVnum(entt::entity item) { return GetItemVnum(item); }
uint32_t GetItemID(entt::entity item) { return g_registry.get<ecs::ItemIdentity>(item).id; }
entt::entity GetItemOwner(entt::entity item)
{
    const auto* owner = g_registry.try_get<ecs::ItemOwner>(item);
    return owner ? owner->owner : entt::entity{entt::null};
}
entt::entity GetItem(entt::entity owner, TItemPos pos)
{
    const auto found = inventory.find({owner, pos.window_type, pos.cell});
    return found != inventory.end() ? found->second : entt::entity{entt::null};
}
uint8_t GetItemWindow(entt::entity item) { return g_registry.get<ecs::ItemLocation>(item).window; }
uint16_t GetItemCell(entt::entity item) { return g_registry.get<ecs::ItemLocation>(item).cell; }
uint32_t GetItemCount(entt::entity item) { return g_registry.get<ecs::ItemCount>(item).count; }
int GetItemAttributeCount(entt::entity item)
{
    return ecs::item_attributes::Count(g_registry.get<ecs::ItemAttributes>(item).attrs, 0, ITEM_ATTRIBUTE_NORM_NUM);
}
bool IsItemEquipped(entt::entity item)
{
    const auto* equipped = g_registry.try_get<ecs::ItemEquipped>(item);
    return equipped && equipped->equipped;
}
bool IsItemExchanging(entt::entity item)
{
    const auto* flags = g_registry.try_get<ecs::ItemFlags>(item);
    return flags && flags->exchanging;
}
bool IsItemLocked(entt::entity item)
{
    const auto* flags = g_registry.try_get<ecs::ItemFlags>(item);
    return flags && flags->isLocked;
}
bool ConsumeItemEcs(entt::entity item, uint32_t amount)
{
    CheckPaymentOrder();
    ++payments;
    if (rejectPayment)
        return false;
    Check(IsValidItem(item) && amount > 0 && GetItemCount(item) >= amount, "invalid material debit");
    if (GetItemCount(item) == amount) {
        inventory.erase({GetItemOwner(item), GetItemWindow(item), GetItemCell(item)});
        g_registry.destroy(item);
    } else {
        g_registry.get<ecs::ItemCount>(item).count -= static_cast<int>(amount);
    }
    return true;
}
uint32_t GetItemSocket(entt::entity, int) { return 0; }
TItemExtraProto* GetItemExtraProto(entt::entity item)
{
    const auto* ref = g_registry.try_get<ecs::ItemExtraProtoRef>(item);
    return ref ? ref->proto : nullptr;
}
short GetItemLockedAttr(entt::entity item)
{
    const auto* locked = g_registry.try_get<ecs::ItemLockedAttribute>(item);
    return locked ? locked->index : -1;
}
TPlayerItemAttribute GetItemAttribute(entt::entity item, int index)
{
    return g_registry.get<ecs::ItemAttributes>(item).attrs[index];
}
void SetItemAttributes(entt::entity item, const TPlayerItemAttribute* attrs)
{
    auto& component = g_registry.get<ecs::ItemAttributes>(item);
    std::copy_n(attrs, ITEM_ATTRIBUTE_MAX_NUM, component.attrs.begin());
    ++saves;
}
}

namespace {
struct Fixture {
    TItemTable proto{};
    entt::entity item;
    Fixture()
    {
        g_registry.clear();
        g_map_itemAttr.clear();
        g_map_itemRare.clear();
        saves = updates = randomCalls = 0;
        payments = 0;
        rejectPayment = rejectGoldPayment = false;
        watchedItem = entt::null;
        inventory.clear();
        proto.bType = ITEM_WEAPON;
        proto.bSubType = WEAPON_SWORD;
        item = g_registry.create();
        g_registry.emplace<ecs::ItemIdentity>(item, ecs::ItemIdentity{1, 1000, 1000});
        g_registry.emplace<ecs::ItemAttributes>(item);
        auto& ref = g_registry.emplace<ecs::ItemProtoRef>(item);
        ref.proto = &proto;
        Check(!g_registry.any_of<ecs::LegacyItemPtr>(item), "fixture must be entity-only");
    }
    auto& Attrs() { return g_registry.get<ecs::ItemAttributes>(item).attrs; }
    void Rows(int count, bool rare = false)
    {
        for (int i = 1; i <= count; ++i) {
            TItemAttrTable row;
            row.dwApplyIndex = i;
            row.dwProb = 1;
            std::fill(std::begin(row.bMaxLevelBySet), std::end(row.bMaxLevelBySet), 5);
            for (size_t level = 0; level < std::size(row.lValues); ++level)
                row.lValues[level] = static_cast<int32_t>(10 + level);
            (rare ? g_map_itemRare : g_map_itemAttr).emplace(i, row);
        }
    }
};

struct PaidFixture : Fixture {
    entt::entity owner;
    entt::entity material;
    PaidFixture()
    {
        owner = g_registry.create();
        g_registry.emplace<TestPlayer>(owner);
        Place(item, SWITCHBOT, 0);
        material = Material(c_arSwitchingItems[0], 2, 0);
        Rows(5);
        Attrs()[0] = {20, 200};
        Attrs()[1] = {21, 300};
        Watch();
    }
    void Place(entt::entity e, uint8_t window, uint16_t cell)
    {
        std::erase_if(inventory, [e](const auto& entry) { return entry.second == e; });
        g_registry.emplace_or_replace<ecs::ItemOwner>(e, ecs::ItemOwner{owner});
        g_registry.emplace_or_replace<ecs::ItemLocation>(e, ecs::ItemLocation{window, cell});
        inventory[{owner, window, cell}] = e;
    }
    entt::entity Material(uint32_t vnum, int count, uint16_t cell)
    {
        const auto e = g_registry.create();
        g_registry.emplace<ecs::ItemIdentity>(e, ecs::ItemIdentity{uint32_t(100 + cell), vnum, vnum});
        g_registry.emplace<ecs::ItemCount>(e, ecs::ItemCount{count});
        Place(e, INVENTORY, cell);
        return e;
    }
    void Watch()
    {
        watchedItem = item;
        beforePayment = Attrs();
        saves = updates = payments = 0;
    }
};

void EntityAndTableValidation()
{
    Fixture f;
    Check(!ItemSystem::AddItemAttributeEcs(entt::null), "null entity accepted");
    const auto foreign = g_registry.create();
    Check(!ItemSystem::AddItemRareAttributeEcs(foreign), "non-item entity accepted");
    f.proto.bSubType = WEAPON_ARROW;
    f.Rows(2, true);
    Check(!ItemSystem::AddItemRareAttributeEcs(f.item), "arrow must not index attribute set -1");
    Check(randomCalls == 0 && saves == 0 && updates == 0, "invalid item caused side effects");
    f.proto.bSubType = WEAPON_SWORD;
    Check(!ItemSystem::AddItemAttributeEcs(f.item), "empty table accepted");
    Check(g_map_itemAttr.empty(), "read inserted rows into the attribute table");
    f.Rows(1);
    g_map_itemAttr.begin()->second.dwProb = 0;
    Check(!ItemSystem::AddItemAttributeEcs(f.item), "zero-weight table accepted");
    Check(saves == 0 && updates == 0, "failed roll saved item");
    g_registry.destroy(f.item);
    Check(!ItemSystem::ChangeItemAttributeEcs(f.item), "stale entity accepted");
}

void LockedSlotAndRarePreservation()
{
    Fixture f;
    f.Rows(10);
    for (int i = 0; i < 5; ++i)
        f.Attrs()[i] = {static_cast<uint8_t>(i + 1), static_cast<int16_t>(100 + i)};
    f.Attrs()[5] = {40, 70};
    f.Attrs()[6] = {41, 80};
    g_registry.emplace<ecs::ItemLockedAttribute>(f.item, ecs::ItemLockedAttribute{2});
    Check(ItemSystem::ChangeItemAttributeEcs(f.item), "locked normal reroll failed");
    Check(f.Attrs()[2].bType == 3 && f.Attrs()[2].sValue == 102, "locked slot changed");
    Check(ecs::item_attributes::Count(f.Attrs(), 0, 5) == 5, "reroll lost a normal bonus");
    Check(f.Attrs()[5].bType == 40 && f.Attrs()[6].bType == 41, "normal reroll touched rare slots");
    Check(saves == 1 && updates == 1, "reroll must publish once after success");
    Check(!ItemSystem::AddItemAttributeEcs(f.item), "full attributes accepted another bonus");
    Check(ItemSystem::ClearNormalItemAttributes(f.item), "clear normal failed");
    Check(f.Attrs()[2].sValue == 102 && f.Attrs()[5].sValue == 70, "clear destroyed protected bonus");
}

void FailureIsAtomic()
{
    Fixture f;
    f.Rows(1);
    f.Attrs()[0] = {20, 100};
    f.Attrs()[1] = {21, 200};
    const auto before = f.Attrs();
    Check(!ItemSystem::ChangeItemAttributeEcs(f.item), "insufficient distinct bonuses accepted");
    Check(f.Attrs()[0].bType == before[0].bType && f.Attrs()[1].sValue == before[1].sValue,
        "failed reroll changed original attributes");
    Check(saves == 0 && updates == 0, "failed reroll published partial state");
    const int badProbabilities[5] = {0, 0, 0, 0, 0};
    Check(!ItemSystem::ChangeItemAttributeEcs(f.item, badProbabilities), "invalid level probabilities accepted");
}

void RareHolesAndFailure()
{
    Fixture f;
    f.Rows(3, true);
    f.Attrs()[6] = {3, 333};
    Check(ItemSystem::AddItemRareAttributeEcs(f.item), "rare gap could not be filled");
    Check(f.Attrs()[5].bType == 1 && f.Attrs()[6].sValue == 333, "rare add overwrote occupied slot");
    Check(!ItemSystem::AddItemRareAttributeEcs(f.item), "rare slots overflowed");
    const auto before = f.Attrs();
    g_map_itemRare.erase(2);
    g_map_itemRare.erase(3);
    saves = updates = 0;
    Check(!ItemSystem::ChangeItemRareAttributeEcs(f.item), "rare reroll accepted too few types");
    Check(f.Attrs()[5].bType == before[5].bType && f.Attrs()[6].sValue == before[6].sValue,
        "failed rare reroll lost bonuses");
    Check(saves == 0 && updates == 0, "failed rare reroll published state");
}

void AddonRemovalAndDuplicates()
{
    Fixture f;
    f.Attrs()[0] = {APPLY_NORMAL_HIT_DAMAGE_BONUS, 20};
    f.Attrs()[1] = {APPLY_SKILL_DAMAGE_BONUS, -5};
    f.Attrs()[2] = {APPLY_MAX_HP, 500};
    f.Attrs()[5] = {40, 444};
    Check(ItemSystem::RemoveItemAttributeType(f.item, APPLY_SKILL_DAMAGE_BONUS), "addon removal failed");
    Check(f.Attrs()[1].bType == APPLY_MAX_HP && f.Attrs()[5].sValue == 444, "addon removal moved wrong slots");
    Check(ItemSystem::ApplyItemAddon(f.item, -1), "entity-only addon failed");
    Check(ecs::item_attributes::Count(f.Attrs(), 0, 5) == 3, "addon did not replace bonus pair");
    Check(!ItemSystem::AddItemAttribute(f.item, APPLY_MAX_HP, 1000), "duplicate normal bonus accepted");
    Check(f.Attrs()[5].sValue == 444, "addon touched rare attribute");
}

void WeightedBoundaries()
{
    Fixture f;
    f.Rows(2);
    g_map_itemAttr.at(1).dwProb = 2;
    g_map_itemAttr.at(2).dwProb = 3;
    auto eligible = [](auto, const auto&) { return true; };
    for (int roll = 1; roll <= 5; ++roll) {
        const auto found = ecs::item_attributes::Select(g_map_itemAttr, ATTRIBUTE_SET_WEAPON, true,
            eligible, [=](int, int) { return roll; });
        Check(found != g_map_itemAttr.end() && found->first == (roll <= 2 ? 1 : 2), "weighted boundary changed");
    }
    auto noRandom = [](int, int) { throw std::runtime_error("invalid table reached RNG"); return 0; };
    const int partialProbabilities[5] = {0, 0, 30, 40, 3};
    Check(ecs::item_attributes::RollLevel(partialProbabilities, 5, [](int, int) { return 30; }) == 3,
        "partial probability table rejected a covered roll");
    Check(ecs::item_attributes::RollLevel(partialProbabilities, 5, [](int, int) { return 73; }) == 5,
        "partial probability table last boundary changed");
    Check(ecs::item_attributes::RollLevel(partialProbabilities, 5, [](int, int) { return 74; }) == 0,
        "unassigned roll was silently renormalized");
    Check(ecs::item_attributes::Select(g_map_itemAttr, -1, true, eligible, noRandom) == g_map_itemAttr.end(),
        "negative attribute set accepted");
    Check(ecs::item_attributes::Select(g_map_itemAttr, ATTRIBUTE_SET_MAX_NUM, true, eligible, noRandom) == g_map_itemAttr.end(),
        "attribute set overflow accepted");
    g_map_itemAttr.at(1).dwProb = UINT32_MAX;
    Check(ecs::item_attributes::Select(g_map_itemAttr, ATTRIBUTE_SET_WEAPON, true, eligible, noRandom) == g_map_itemAttr.end(),
        "probability sum overflow accepted");
}

void ProtoAndCostumeRules()
{
    Fixture f;
    f.Rows(3);
    f.proto.aApplies[0].bType = 1;
    Check(ItemSystem::AddItemAttributeEcs(f.item), "base apply exclusion blocked all bonuses");
    Check(f.Attrs()[0].bType == 2, "normal roll duplicated a base apply");
    f.Attrs().fill({});
    g_registry.get<ecs::ItemIdentity>(f.item).vnum = 300;
    Check(ItemSystem::HasItemAttribute(f.item, 1), "base-apply query lost the Zodiac base bonus");
    Check(ItemSystem::AddItemAttributeEcs(f.item), "zodiac roll failed");
#ifndef DISABLE_ZODIAC_ATT
    Check(f.Attrs()[0].bType == 1, "zodiac base-apply exception lost");
#endif
#ifdef ENABLE_ATTR_COSTUMES
    f.Attrs().fill({});
    f.proto.aApplies[0] = {};
    f.proto.bType = ITEM_COSTUME;
    f.proto.bSubType = COSTUME_BODY;
    Check(ItemSystem::AddItemAttributeEcs(f.item), "costume roll failed");
    Check(f.Attrs()[0].sValue == 15, "costume used non-costume level values");
    f.Attrs().fill({});
    Check(ItemSystem::AlterItemToMagicItem(f.item), "entity-only magic generation failed");
    Check(ecs::item_attributes::Count(f.Attrs(), 0, 5) == 3 && f.Attrs()[0].sValue == 19,
        "costume magic count or high-level value changed");
#endif
    saves = updates = 0;
    Check(!ItemSystem::SetItemForceAttributeEcs(f.item, -1, 1, 10), "negative force-attribute index accepted");
    Check(!ItemSystem::SetItemForceAttributeEcs(f.item, ITEM_ATTRIBUTE_MAX_NUM, 1, 10), "force-attribute index overflow");
    Check(saves == 0 && updates == 0, "invalid force write caused side effects");
    Check(ItemSystem::SetItemForceAttributeEcs(f.item, 6, 2, -30), "signed force attribute failed");
    Check(f.Attrs()[6].sValue == -30 && saves == 1 && updates == 1, "force write did not update client and save");
}

void PaidRerollTransactions()
{
    PaidFixture f;
    g_map_itemAttr.erase(2);
    g_map_itemAttr.erase(3);
    g_map_itemAttr.erase(4);
    g_map_itemAttr.erase(5);
    Check(!ItemSystem::ChangeItemAttributeWithItemCost(f.item, f.material), "incomplete paid roll accepted");
    Check(payments == 0 && ItemSystem::GetItemCount(f.material) == 2 && EqualAttributes(f.Attrs(), beforePayment),
        "failed preparation consumed material or changed bonuses");
    f.Rows(5);
    rejectPayment = true;
    Check(!ItemSystem::ChangeItemAttributeWithItemCost(f.item, f.material), "failed payment accepted");
    Check(payments == 1 && saves == 0 && updates == 0 && EqualAttributes(f.Attrs(), beforePayment),
        "failed payment published a reroll");
    rejectPayment = false;
    f.Watch();
    Check(ItemSystem::ChangeItemAttributeWithItemCost(f.item, f.material), "paid reroll failed");
    Check(payments == 1 && ItemSystem::GetItemCount(f.material) == 1 && saves == 1 && updates == 1,
        "paid reroll did not charge and publish exactly once");
    f.Watch();
    Check(ItemSystem::ChangeItemAttributeWithItemCost(f.item, f.material), "last material failed");
    Check(!g_registry.valid(f.material) && g_registry.valid(f.item) && saves == 1 && updates == 1,
        "last-unit consumption invalidated target or missed commit");
    f.Watch();
    Check(!ItemSystem::ChangeItemAttributeWithItemCost(f.item, f.material) && payments == 0,
        "stale material was reused");
}

void PaymentValidation()
{
    PaidFixture f;
    Check(!ItemSystem::CanPayItemAttributeCost(f.item, f.item), "target accepted as material");
    Check(!ItemSystem::CanPayItemAttributeCost(f.item, f.material, 0), "zero cost accepted");
    Check(!ItemSystem::CanPayItemAttributeCost(f.item, f.material, 3), "material stack underflow accepted");
    g_registry.get<ecs::ItemCount>(f.material).count = -1;
    Check(!ItemSystem::CanPayItemAttributeCost(f.item, f.material), "negative stack wrapped into an affordable count");
    g_registry.get<ecs::ItemCount>(f.material).count = 2;
    auto& flags = g_registry.emplace<ecs::ItemFlags>(f.material);
    flags.exchanging = true;
    Check(!ItemSystem::ChangeItemAttributeWithItemCost(f.item, f.material), "exchanging material consumed");
    flags.exchanging = false;
    flags.isLocked = true;
    Check(!ItemSystem::CanPayItemAttributeCost(f.item, f.material), "locked material accepted");
    flags.isLocked = false;
    auto& equipped = g_registry.emplace<ecs::ItemEquipped>(f.item);
    equipped.equipped = true;
    Check(!ItemSystem::ChangeItemAttributeWithItemCost(f.item, f.material), "equipped target changed");
    equipped.equipped = false;
    const auto stranger = g_registry.create();
    g_registry.get<ecs::ItemOwner>(f.material).owner = stranger;
    Check(!ItemSystem::CanPayItemAttributeCost(f.item, f.material), "foreign material accepted");
    g_registry.get<ecs::ItemOwner>(f.material).owner = f.owner;
    inventory.erase({f.owner, INVENTORY, 0});
    Check(!ItemSystem::CanPayItemAttributeCost(f.item, f.material), "detached material accepted");
    Check(payments == 0 && saves == 0 && updates == 0, "invalid payment caused side effects");
    f.Place(f.material, INVENTORY, 0);
    g_registry.destroy(f.owner);
    Check(!ItemSystem::ChangeItemAttributeWithItemCost(f.item, f.material), "stale owner accepted");
}

void GoldTransactions()
{
    PaidFixture f;
    Check(!ItemSystem::ChangeItemAttributeWithGoldCost(f.item, 101), "insufficient yang accepted");
    Check(!ItemSystem::ChangeItemAttributeWithGoldCost(f.item, 0), "free yang reroll accepted");
    Check(!ItemSystem::ChangeItemAttributeWithGoldCost(f.item, -1), "negative price accepted");
    g_map_itemAttr.clear();
    Check(!ItemSystem::ChangeItemAttributeWithGoldCost(f.item, 10), "invalid roll charged yang");
    Check(payments == 0 && ecs::PointSystem::GetGold(f.owner) == 100, "failed preparation changed wallet");
    f.Rows(5);
    rejectGoldPayment = true;
    Check(!ItemSystem::ChangeItemAttributeWithGoldCost(f.item, 10), "rejected wallet debit accepted");
    Check(saves == 0 && updates == 0 && EqualAttributes(f.Attrs(), beforePayment), "rejected debit changed attributes");
    rejectGoldPayment = false;
    f.Watch();
    Check(ItemSystem::ChangeItemAttributeWithGoldCost(f.item, 100), "exact yang payment failed");
    Check(ecs::PointSystem::GetGold(f.owner) == 0 && payments == 1 && saves == 1 && updates == 1,
        "yang debit and commit were not applied exactly once");
}

void CostumeResetTransactions()
{
#ifdef ENABLE_ATTR_COSTUMES
    PaidFixture f;
    f.proto.bType = ITEM_COSTUME;
    f.proto.bSubType = COSTUME_BODY;
    f.Attrs()[5] = {40, 400};
    f.Attrs()[6] = {41, 500};
    f.Watch();
    g_map_itemAttr.clear();
    f.Rows(2);
    Check(!ItemSystem::ResetCostumeAttributesWithItemCost(f.item, f.material), "partial costume reset accepted");
    Check(EqualAttributes(f.Attrs(), beforePayment) && saves == 0 && updates == 0 && payments == 0,
        "failed reset cleared original bonuses or consumed reset item");
    f.Rows(5);
    rejectPayment = true;
    Check(!ItemSystem::ResetCostumeAttributesWithItemCost(f.item, f.material), "unpaid costume reset accepted");
    Check(EqualAttributes(f.Attrs(), beforePayment) && saves == 0 && updates == 0,
        "rejected reset payment changed attributes");
    rejectPayment = false;
    f.Watch();
    g_registry.get<ecs::ItemCount>(f.material).count = 1;
    Check(ItemSystem::ResetCostumeAttributesWithItemCost(f.item, f.material), "costume reset failed");
    Check(ItemSystem::GetItemAttributeCount(f.item) == 3 && f.Attrs()[0].sValue == 19 && f.Attrs()[1].sValue == 15,
        "costume reset changed three-bonus level rules");
    Check(f.Attrs()[5].sValue == 400 && f.Attrs()[6].sValue == 500 && !g_registry.valid(f.material),
        "costume reset damaged rare bonuses or failed to consume last material");
    Check(saves == 1 && updates == 1 && payments == 1, "costume reset published intermediate empty attributes");
    f.material = f.Material(39028, 2, 0);
    f.Attrs()[2] = {30, 777};
    g_registry.emplace<ecs::ItemLockedAttribute>(f.item, ecs::ItemLockedAttribute{2});
    f.Watch();
    Check(ItemSystem::ResetCostumeAttributesWithItemCost(f.item, f.material), "locked costume reset failed");
    Check(f.Attrs()[2].bType == 30 && f.Attrs()[2].sValue == 777, "reset lost locked slot");
    f.proto.bType = ITEM_WEAPON;
    f.Watch();
    Check(!ItemSystem::ResetCostumeAttributesWithItemCost(f.item, f.material) && payments == 0,
        "costume reset accepted weapon");
#endif
}

#ifdef ENABLE_ATTR_COSTUMES
struct CostumeFixture : PaidFixture {
    TItemTable materialProto{};
    explicit CostumeFixture(uint8_t operation)
    {
        proto.bType = ITEM_COSTUME;
        proto.bSubType = COSTUME_BODY;
        Place(item, INVENTORY, 5);
        materialProto.bType = ITEM_USE;
        materialProto.bSubType = operation;
        g_registry.emplace<ecs::ItemProtoRef>(material).proto = &materialProto;
        g_registry.emplace<ecs::ItemSockets>(material).sockets = {40, 100};
        Check(!g_registry.any_of<ecs::LegacyCharPtr>(owner), "costume owner must be entity-only");
        Watch();
    }
    auto Use() { return ItemSystem::UseCostumeAttributeItem(owner, item, material); }
    void Unchanged()
    {
        Check(EqualAttributes(Attrs(), beforePayment) && saves == 0 && updates == 0,
            "rejected costume operation published attributes");
        Check(ItemSystem::GetItemCount(material) == 2, "rejected costume operation consumed material");
    }
};
#endif

void CostumeSelectionValidation()
{
#ifdef ENABLE_ATTR_COSTUMES
    using Result = ItemSystem::CostumeAttributeResult;
    CostumeFixture f(USE_REMOVE_ATTR_COSTUME);
    f.Attrs()[5] = {40, 400};
    f.Attrs()[6] = {41, 500};
    f.Watch();
    Check(!ItemSystem::SelectCostumeAttributeToRemove(entt::null, "0"), "null dialog owner accepted");
    Check(!ItemSystem::SelectCostumeAttributeToRemove(f.item, "0"), "non-player dialog owner accepted");
    for (const auto input : {"", "-1", "-5", "-6", "2", "garbage", "1suffix", "01", "+1", " 1", "999999999999999999999"}) {
        Check(ItemSystem::SelectCostumeAttributeToRemove(f.owner, "1"), "valid dialog selection failed");
        Check(!ItemSystem::SelectCostumeAttributeToRemove(f.owner, input), "malformed selection accepted");
        Check(f.Use() == Result::InvalidSelection, "invalid command reused previous selection");
        f.Unchanged();
    }
    // Defend the operation as well as the command boundary against bad state.
    for (const int index : {INT_MIN, -6, -5, -1, 2, INT_MAX}) {
        g_registry.get<ecs::CostumeAttributeSelection>(f.owner).rareSlot = index;
        Check(f.Use() == Result::InvalidSelection, "out-of-range rare index accepted");
        f.Unchanged();
    }
    Check(payments == 0, "invalid selection attempted payment");
    g_registry.destroy(f.owner);
    Check(!ItemSystem::SelectCostumeAttributeToRemove(f.owner, "0"), "stale dialog owner accepted");
    Check(f.Use() == Result::InvalidTarget, "stale owner used costume item");
#endif
}

void CostumeAdditionTransactions()
{
#ifdef ENABLE_ATTR_COSTUMES
    using Result = ItemSystem::CostumeAttributeResult;
    for (const uint8_t subtype : {COSTUME_BODY, COSTUME_HAIR, COSTUME_WEAPON})
    for (const uint8_t operation : {USE_ADD_ATTR_COSTUME1, USE_ADD_ATTR_COSTUME2}) {
        CostumeFixture f(operation);
        f.proto.bSubType = subtype;
        g_registry.get<ecs::ItemSockets>(f.material).sockets[1] = -123;
        g_registry.get<ecs::ItemCount>(f.material).count = 1;
        Check(f.Use() == Result::Success, "costume addition failed");
        Check(f.Attrs()[5].bType == 40 && f.Attrs()[5].sValue == -123 && f.Attrs()[6].bType == 0,
            "costume addition changed signed payload or rare slot order");
        Check(f.Attrs()[0].sValue == 200 && f.Attrs()[1].sValue == 300 && !g_registry.valid(f.material),
            "costume addition damaged normal bonuses or missed last-unit debit");
        Check(saves == 1 && updates == 1 && payments == 1 && randomCalls == 0,
            "explicit costume addition was not a single deterministic transaction");
        f.Watch();
        Check(f.Use() == Result::InvalidMaterial && payments == 0, "stale costume material reused");
    }
    CostumeFixture f(USE_ADD_ATTR_COSTUME1);
    f.Attrs()[6] = {40, 500};
    f.Watch();
    Check(f.Use() == Result::DuplicateAttribute, "duplicate rare bonus accepted across a gap");
    f.Unchanged();
    auto& sockets = g_registry.get<ecs::ItemSockets>(f.material).sockets;
    for (const int32_t type : {0, -1, 256, static_cast<int>(MAX_APPLY_NUM), INT32_MAX}) {
        sockets[0] = type;
        Check(f.Use() == Result::InvalidMaterial, "invalid costume apply narrowed to a byte");
        f.Unchanged();
    }
    sockets[0] = 20; // A normal-slot duplicate is allowed by the existing costume rules.
    for (const int32_t value : {0, INT16_MIN - 1, INT16_MAX + 1, INT32_MIN, INT32_MAX}) {
        sockets[1] = value;
        Check(f.Use() == Result::InvalidMaterial, "invalid costume value narrowed to a short");
        f.Unchanged();
    }
    sockets[1] = INT16_MIN;
    rejectPayment = true;
    Check(f.Use() == Result::Failed, "unpaid costume addition accepted");
    f.Unchanged();
    rejectPayment = false;
    f.Watch();
    Check(f.Use() == Result::Success, "costume rare-gap addition failed");
    Check(f.Attrs()[5].bType == 20 && f.Attrs()[5].sValue == INT16_MIN && f.Attrs()[6].sValue == 500,
        "rare-gap addition lost other bonuses");
    f.Watch();
    Check(f.Use() == Result::SlotsFull && payments == 0 && saves == 0, "full rare slots accepted addition");
    f.Attrs()[5] = {};
    f.Watch();
    g_registry.remove<ecs::ItemSockets>(f.material);
    Check(f.Use() == Result::InvalidMaterial && payments == 0, "missing material sockets accepted");
#endif
}

void CostumeRemovalTransactions()
{
#ifdef ENABLE_ATTR_COSTUMES
    using Result = ItemSystem::CostumeAttributeResult;
    for (const int selected : {0, 1}) {
        CostumeFixture f(USE_REMOVE_ATTR_COSTUME);
        f.Attrs()[5] = {40, 400};
        f.Attrs()[6] = {41, 500};
        f.Watch();
        // No component retains the legacy default of slot zero.
        if (selected == 1)
            Check(ItemSystem::SelectCostumeAttributeToRemove(f.owner, "1"), "select second rare slot failed");
        rejectPayment = true;
        Check(f.Use() == Result::Failed, "unpaid costume removal accepted");
        f.Unchanged();
        rejectPayment = false;
        f.Watch();
        g_registry.get<ecs::ItemCount>(f.material).count = 1;
        Check(f.Use() == Result::Success, "costume removal failed");
        Check(f.Attrs()[5].bType == (selected == 0 ? 41 : 40) &&
            f.Attrs()[5].sValue == (selected == 0 ? 500 : 400) && f.Attrs()[6].bType == 0,
            "costume removal compacted wrong slots");
        Check(f.Attrs()[0].sValue == 200 && f.Attrs()[1].sValue == 300,
            "costume removal changed normal bonuses");
        Check(!g_registry.valid(f.material) && saves == 1 && updates == 1 && payments == 1,
            "costume removal published intermediate state or missed last-unit debit");
    }
    CostumeFixture f(USE_REMOVE_ATTR_COSTUME);
    Check(f.Use() == Result::NoRareAttributes, "empty costume rare slots consumed remover");
    f.Attrs()[6] = {41, 500};
    f.Watch();
    Check(f.Use() == Result::NoRareAttributes, "empty selected slot consumed remover");
    f.Unchanged();
    Check(ItemSystem::SelectCostumeAttributeToRemove(f.owner, "1"), "select rare slot after gap failed");
    Check(f.Use() == Result::Success && f.Attrs()[6].bType == 0, "rare-slot gap blocked valid removal");
#endif
}

void CostumeOperationValidation()
{
#ifdef ENABLE_ATTR_COSTUMES
    using Result = ItemSystem::CostumeAttributeResult;
    CostumeFixture f(USE_CHANGE_ATTR_COSTUME);
    const auto stranger = g_registry.create();
    g_registry.emplace<TestPlayer>(stranger);
    Check(ItemSystem::UseCostumeAttributeItem(stranger, f.item, f.material) == Result::InvalidTarget,
        "costume operation accepted foreign owner");
    Check(ItemSystem::UseCostumeAttributeItem(f.owner, entt::null, f.material) == Result::InvalidTarget,
        "costume operation accepted null target");
    inventory.erase({f.owner, INVENTORY, 5});
    Check(f.Use() == Result::InvalidTarget, "costume operation accepted detached target");
    f.Place(f.item, INVENTORY, 5);
    auto& flags = g_registry.emplace<ecs::ItemFlags>(f.item);
    flags.isLocked = true;
    Check(f.Use() == Result::InvalidTarget, "costume operation accepted locked target");
    flags.isLocked = false;
    flags.exchanging = true;
    Check(f.Use() == Result::InvalidTarget, "costume operation accepted exchanging target");
    flags.exchanging = false;
    g_registry.emplace<ecs::ItemEquipped>(f.item).equipped = true;
    Check(f.Use() == Result::InvalidTarget, "costume operation accepted equipped target");
    g_registry.get<ecs::ItemEquipped>(f.item).equipped = false;
    f.proto.bType = ITEM_WEAPON;
    Check(f.Use() == Result::InvalidTarget, "costume operation accepted weapon");
    f.proto.bType = ITEM_COSTUME;
    f.proto.bSubType = COSTUME_ACCE;
    Check(f.Use() == Result::InvalidTarget, "costume operation accepted sash");
    f.proto.bSubType = COSTUME_BODY;
    f.materialProto.bType = ITEM_WEAPON;
    Check(f.Use() == Result::InvalidMaterial, "non-consumable costume material accepted");
    f.materialProto.bType = ITEM_USE;
    f.materialProto.bSubType = USE_POTION;
    Check(f.Use() == Result::InvalidMaterial, "unrelated consumable accepted");
    f.materialProto.bSubType = USE_CHANGE_ATTR_COSTUME;
    g_registry.get<ecs::ItemOwner>(f.material).owner = stranger;
    Check(f.Use() == Result::InvalidMaterial, "foreign costume material accepted");
    g_registry.get<ecs::ItemOwner>(f.material).owner = f.owner;
    inventory.erase({f.owner, INVENTORY, 0});
    Check(f.Use() == Result::InvalidMaterial, "detached costume material accepted");
    f.Place(f.material, INVENTORY, 0);
    g_registry.emplace<ecs::ItemFlags>(f.material).isLocked = true;
    Check(f.Use() == Result::InvalidMaterial, "locked costume material accepted");
    g_registry.get<ecs::ItemFlags>(f.material).isLocked = false;
    Check(payments == 0, "invalid costume request attempted payment");
    f.Unchanged();
    f.Attrs()[0] = f.Attrs()[1] = {};
    f.Watch();
    Check(f.Use() == Result::NoAttributes && payments == 0, "empty costume rerolled");
    f.Attrs()[0] = {20, 200};
    f.Attrs()[1] = {21, 300};
    f.Attrs()[5] = {40, 400};
    f.Attrs()[6] = {41, 500};
    f.Watch();
    g_map_itemAttr.clear();
    Check(f.Use() == Result::Failed && payments == 0, "empty table charged costume changer");
    f.Unchanged();
    f.Rows(5);
    rejectPayment = true;
    Check(f.Use() == Result::Failed, "unpaid costume reroll accepted");
    f.Unchanged();
    rejectPayment = false;
    f.Watch();
    Check(f.Use() == Result::Success && saves == 1 && updates == 1 && payments == 1, "costume reroll transaction failed");
    Check(f.Attrs()[5].sValue == 400 && f.Attrs()[6].sValue == 500 && ItemSystem::GetItemAttributeCount(f.item) == 2,
        "costume changer altered rare bonuses or normal count");
    f.Watch();
    Check(f.Use() == Result::Success && !g_registry.valid(f.material) && saves == 1 && updates == 1,
        "costume changer failed last-unit debit");
    f.Watch();
    g_registry.destroy(f.item);
    Check(f.Use() == Result::InvalidTarget && payments == 0, "stale costume target accepted");
#endif
}

void SwitchbotTransactions()
{
#if defined(ENABLE_SWITCHBOT)
    using Result = SwitchbotHelper::Result;
    PaidFixture f;
    const auto stranger = g_registry.create();
    g_registry.emplace<TestPlayer>(stranger);
    Check(SwitchbotHelper::TrySwitch(stranger, f.item, 0).result == Result::InvalidTarget, "switchbot accepted foreign target");
    Check(SwitchbotHelper::TrySwitch(f.owner, f.item, 1).result == Result::InvalidTarget, "switchbot accepted wrong slot");
    Check(SwitchbotHelper::TrySwitch(f.owner, f.item, SWITCHBOT_SLOT_COUNT).result == Result::InvalidTarget, "switchbot slot overflow");
    inventory.erase({f.owner, SWITCHBOT, 0});
    Check(SwitchbotHelper::TrySwitch(f.owner, f.item, 0).result == Result::InvalidTarget, "switchbot accepted detached target");
    f.Place(f.item, SWITCHBOT, 0);
    g_map_itemAttr.clear();
    Check(SwitchbotHelper::TrySwitch(f.owner, f.item, 0).result == Result::RollFailed, "switchbot ignored failed reroll");
    Check(payments == 0 && ItemSystem::GetItemCount(f.material) == 2, "switchbot consumed item for failed roll");
    f.Rows(5);
    const auto outcome = SwitchbotHelper::TrySwitch(f.owner, f.item, 0);
    Check(outcome.result == Result::Success && outcome.materialVnum == c_arSwitchingItems[0], "switchbot transaction failed");
    Check(payments == 1 && saves == 1 && updates == 1, "switchbot charged or committed more than once");
    f.Watch();
    g_registry.get<ecs::ItemCount>(f.material).count = 0;
    Check(SwitchbotHelper::TrySwitch(f.owner, f.item, 0).result == Result::NoPayment && payments == 0, "empty material stack accepted");

    const auto limited = f.Material(71151, 2, 1);
    f.proto.aLimits[0] = {LIMIT_LEVEL, 31};
    Check(SwitchbotHelper::TrySwitch(f.owner, f.item, 0).result == Result::NoPayment, "level-limited changer used above level 30");
    f.proto.aLimits[0].lValue = 30;
    Check(SwitchbotHelper::TrySwitch(f.owner, f.item, 0).result == Result::Success && ItemSystem::GetItemCount(limited) == 1,
        "level-30 limited changer rejected");
    f.Watch();
#ifdef DISABLE_ZODIAC_ATT
    g_registry.get<ecs::ItemIdentity>(f.item).vnum = 12314141;
#else
    g_registry.get<ecs::ItemIdentity>(f.item).vnum = 300;
#endif
    Check(SwitchbotHelper::TrySwitch(f.owner, f.item, 0).result == Result::NoPayment, "zodiac accepted normal changer");
    const auto zodiac = f.Material(86060, 1, 2);
    const auto zodiacOutcome = SwitchbotHelper::TrySwitch(f.owner, f.item, 0);
    Check(zodiacOutcome.result == Result::Success && zodiacOutcome.materialVnum == 86060 && !g_registry.valid(zodiac),
        "zodiac payment or destroyed-material ID lost");
    f.Watch();
    g_registry.destroy(f.item);
    Check(SwitchbotHelper::TrySwitch(f.owner, f.item, 0).result == Result::InvalidTarget && payments == 0, "stale switchbot entity accepted");
#endif
}

void SwitchbotMaterialSelection()
{
#ifdef ENABLE_SWITCHBOT
    using Result = SwitchbotHelper::Result;
    PaidFixture f;
    g_registry.emplace<ecs::ItemFlags>(f.material).isLocked = true;
    const auto fallback = f.Material(c_arSwitchingItems[0], 1, 1);
    Check(SwitchbotHelper::TrySwitch(f.owner, f.item, 0).result == Result::Success, "locked first stack blocked valid fallback");
    Check(ItemSystem::GetItemCount(f.material) == 2 && !g_registry.valid(fallback), "wrong material stack consumed");
#ifdef ENABLE_EXTRA_INVENTORY
    g_registry.get<ecs::ItemFlags>(f.material).isLocked = false;
    f.Place(f.material, EXTRA_INVENTORY, 5);
    f.Watch();
    Check(SwitchbotHelper::TrySwitch(f.owner, f.item, 0).result == Result::Success && ItemSystem::GetItemCount(f.material) == 1,
        "extra-inventory material was not consumed");
#endif
#endif
}
}

int main()
{
    try {
        EntityAndTableValidation();
        LockedSlotAndRarePreservation();
        FailureIsAtomic();
        RareHolesAndFailure();
        AddonRemovalAndDuplicates();
        WeightedBoundaries();
        ProtoAndCostumeRules();
        PaidRerollTransactions();
        PaymentValidation();
        GoldTransactions();
        CostumeResetTransactions();
        CostumeSelectionValidation();
        CostumeAdditionTransactions();
        CostumeRemovalTransactions();
        CostumeOperationValidation();
        SwitchbotTransactions();
        SwitchbotMaterialSelection();
        std::cout << "Item attribute regression checks passed: " << checks << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
