#include "../../SRC/Server/GameServer/stdafx.h"
#include "../../SRC/Server/GameServer/shop.h"
#include "../../SRC/Server/GameServer/shop_manager.h"
#include "../../SRC/Server/GameServer/item_manager.h"
#include "../../SRC/Server/GameServer/log.h"
#include "../../SRC/Server/GameServer/db.h"
#include "../../SRC/Server/GameServer/desc.h"
#include "../../SRC/Server/GameServer/desc_client.h"
#include "../../SRC/Server/GameServer/battle_pass.h"
#include "../../SRC/Server/GameServer/config.h"
#include "../../SRC/Server/GameServer/questmanager.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/components/identity_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/social_components.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PointSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/InventorySystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ItemSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SessionSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ChatSystem.hpp"
#include <Core/Logging.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// Real shop.cpp + shop_manager.cpp with entity-only fixtures. Descriptors,
// items, points, sessions and the DB are doubles.
entt::registry g_registry;
int passes_per_sec = 25;
int test_server = 0;
int g_bItemCountLimit = 200;
bool g_bEmpireShopPriceTripleDisable = false;
uint32_t g_BuySellTimeLimitValue = 0;
namespace { int dbMarker = 0; }
LPCLIENT_DESC db_clientdesc = reinterpret_cast<LPCLIENT_DESC>(&dbMarker);

CShopManager shop_manager;
ITEM_MANAGER item_manager;
LogManager log_manager;

namespace {
int checks = 0;
int packetsSent = 0;
int64_t playerGold = 0;
int64_t goldChanges = 0;
bool inventoryFull = false;
std::vector<entt::entity> destroyedItems;
TItemTable testItemProto {};

struct Actor {
    uint32_t pid = 0;
    uint32_t vid = 0;
    int32_t x = 0;
    int32_t y = 0;
    uint8_t empire = 0;
    bool pc = true;
    std::string name;
};

struct TestItem {
    uint32_t vnum = 0;
    uint32_t count = 1;
    uint32_t id = 0;
};

void Check(bool value, const char* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}

void Reset() {
    g_registry.clear();
    shop_manager.Destroy();
    packetsSent = 0;
    goldChanges = 0;
    playerGold = 0;
    inventoryFull = false;
    destroyedItems.clear();
}

entt::entity ActorEntity(uint32_t pid, uint32_t vid, bool pc) {
    const auto entity = g_registry.create();
    auto& actor = g_registry.emplace<Actor>(entity);
    actor.pid = pid;
    actor.vid = vid;
    actor.pc = pc;
    actor.name = pc ? "player" : "npc";
    g_registry.emplace<ecs::TagCharacter>(entity);
    return entity;
}

entt::entity ItemEntity(uint32_t vnum, uint32_t count, uint32_t id) {
    const auto entity = g_registry.create();
    auto& item = g_registry.emplace<TestItem>(entity);
    item.vnum = vnum;
    item.count = count;
    item.id = id;
    return entity;
}

ecs::ShopData& ShopOf(entt::entity shop) { return g_registry.get<ecs::ShopData>(shop); }

// One NPC shop table with two items and a terminating zero row.
std::vector<TShopTable> MakeNpcTable(uint32_t vnum, uint32_t npcVnum) {
    std::vector<TShopTable> table(1);
    table[0] = {};
    table[0].dwVnum = vnum;
    table[0].dwNPCVnum = npcVnum;
    table[0].items[0].vnum = 13001;
    table[0].items[0].count = 5;
    table[0].items[0].price = 1000;
    table[0].items[0].pos = TItemPos(INVENTORY, 0);
    table[0].items[1].vnum = 13002;
    table[0].items[1].count = 2;
    table[0].items[1].price = 250;
    table[0].items[1].pos = TItemPos(INVENTORY, 1);
    return table;
}
}

void DESC::Packet(const void*, int) { ++packetsSent; }
void DESC::BufferedPacket(const void*, int) { ++packetsSent; }
void CLIENT_DESC::DBPacket(uint8_t, uint32_t, const void*, uint32_t) {}
void CLIENT_DESC::DBPacketHeader(uint8_t, uint32_t, uint32_t) {}

ITEM_MANAGER::ITEM_MANAGER() {}
ITEM_MANAGER::~ITEM_MANAGER() = default;
LogManager::LogManager() : m_bIsConnect(false) {}
LogManager::~LogManager() = default;




quest::CQuestManager::CQuestManager() = default;
quest::CQuestManager::~CQuestManager() = default;
quest::PC::~PC() = default;
quest::NPC::~NPC() = default;

entt::entity ITEM_MANAGER::CreateItem(uint32_t vnum, uint32_t count, uint32_t id, bool, int, bool) {
    return ItemEntity(vnum, count, id);
}
TItemTable* ITEM_MANAGER::GetTable(uint32_t vnum) {
    testItemProto.bSize = 1;
    testItemProto.dwVnum = vnum;
    std::snprintf(testItemProto.szName, sizeof(testItemProto.szName), "item-%u", vnum);
    return &testItemProto;
}

void LogManager::ItemLog(entt::entity, int, int, const char*, const char*) {}
void LogManager::ItemLogEntity(entt::entity, entt::entity, const char*, const char*) {}
void LogManager::GoldBarLog(uint32_t, uint32_t, GOLDBAR_HOW, const char*) {}
void LogManager::HackLog(const char*, entt::entity) {}
void DBManager::SendMoneyLog(uint8_t, uint32_t, int64_t) {}
CAsyncSQL::CAsyncSQL() = default;
CAsyncSQL::~CAsyncSQL() = default;
CSemaphore::CSemaphore() = default;
CSemaphore::~CSemaphore() = default;
bool CBattlePass::BattlePassMissionGetInfo(uint8_t, uint8_t, uint32_t*, uint32_t*) { return false; }
int quest::CQuestManager::GetEventFlag(const std::string&) { return 0; }
const std::string& LocaleService_GetBasePath() { static const std::string path = "/never/read"; return path; }
void ContinueOnFatalError() {}

namespace ecs::PlayerRuntime {
bool IsPC(entt::entity e) { const auto* a = g_registry.valid(e) ? g_registry.try_get<Actor>(e) : nullptr; return a && a->pc; }
bool IsValid(entt::entity e) { return e != entt::null && g_registry.valid(e); }
std::string_view GetName(entt::entity e) { const auto* a = g_registry.try_get<Actor>(e); return a ? a->name : "unknown"; }
uint32_t GetPlayerID(entt::entity e) { const auto* a = g_registry.try_get<Actor>(e); return a ? a->pid : 0; }
uint32_t GetPacketVID(entt::entity e) { const auto* a = g_registry.try_get<Actor>(e); return a ? a->vid : 0; }
uint32_t GetRaceNum(entt::entity e) { const auto* a = g_registry.try_get<Actor>(e); return a ? a->vid : 0; }
int32_t GetX(entt::entity e) { const auto* a = g_registry.try_get<Actor>(e); return a ? a->x : 0; }
int32_t GetY(entt::entity e) { const auto* a = g_registry.try_get<Actor>(e); return a ? a->y : 0; }
uint8_t GetEmpire(entt::entity e) { const auto* a = g_registry.try_get<Actor>(e); return a ? a->empire : 0; }
uint8_t GetGMLevel(entt::entity) { return GM_PLAYER; }
LPDESC GetDesc(entt::entity e) { return IsPC(e) ? reinterpret_cast<LPDESC>(static_cast<uintptr_t>(0x3000 + entt::to_entity(e))) : nullptr; }
uint8_t GetBattlePassId(entt::entity) { return 0; }
uint32_t GetMissionProgress(entt::entity, uint32_t, uint32_t) { return 0; }
bool UpdateMissionProgress(entt::entity, uint32_t, uint32_t, uint32_t, uint32_t, bool) { return false; }
}

namespace ItemSystem {
bool IsValidItem(entt::entity e) { return e != entt::null && g_registry.valid(e) && g_registry.all_of<TestItem>(e); }
uint32_t GetItemVnum(entt::entity e) { return g_registry.get<TestItem>(e).vnum; }
uint32_t GetItemCount(entt::entity e) { return g_registry.get<TestItem>(e).count; }
uint32_t GetItemID(entt::entity e) { return g_registry.get<TestItem>(e).id; }
uint8_t GetItemSize(entt::entity) { return 1; }
entt::entity GetItemOwner(entt::entity) { return entt::null; }
entt::entity GetItem(entt::entity, TItemPos) { return entt::null; }
entt::entity GetInventoryItem(entt::entity, uint16_t) { return entt::null; }
entt::entity GetExtraInventoryItem(entt::entity, uint16_t) { return entt::null; }
const TItemTable* GetItemProto(entt::entity) { return &testItemProto; }
uint32_t GetItemSocket(entt::entity, int) { return 0; }
TPlayerItemAttribute GetItemAttribute(entt::entity, int) { return {}; }
int16_t GetItemLockedAttributeIndex(entt::entity) { return -1; }
uint16_t GetItemCell(entt::entity) { return 0; }
bool IsDragonSoulItem(entt::entity) { return false; }
bool IsExtraItem(entt::entity) { return false; }
bool IsItemStackable(entt::entity) { return false; }
bool IsItemEquipped(entt::entity) { return false; }
bool IsItemLocked(entt::entity) { return false; }
uint32_t GetItemAntiFlag(entt::entity) { return 0; }
int32_t GetItemFlags(entt::entity) { return 0; }
int64_t GetItemShopBuyPrice(entt::entity) { return 0; }
int GetEmptyDragonSoulInventory(entt::entity, entt::entity) { return inventoryFull ? -1 : 0; }
int GetEmptyExtraInventory(entt::entity, entt::entity) { return inventoryFull ? -1 : 0; }
int CountItem(entt::entity, uint32_t) { return 0; }
int CountTypeItem(entt::entity, uint8_t) { return 0; }
bool RemoveSpecifyItemEcs(entt::entity, uint32_t, uint32_t, bool) { return false; }
void RemoveSpecifyTypeItem(entt::entity, uint8_t, int) {}
bool AddItemCountEcs(entt::entity item, int amount) { g_registry.get<TestItem>(item).count += amount; return true; }
bool SetItemCountEcs(entt::entity item, uint32_t amount) { g_registry.get<TestItem>(item).count = amount; return true; }
bool DestroyItemEntityEcs(entt::entity item, const char*) {
    destroyedItems.push_back(item);
    if (g_registry.valid(item)) g_registry.destroy(item);
    return true;
}
bool FlushDelayedSaveEcs(entt::entity) { return true; }
const char* GetItemName(entt::entity item) { return g_registry.valid(item) ? "item" : "unknown"; }
bool ConsumeItemEcs(entt::entity item, uint32_t amount) {
    if (!IsValidItem(item)) return false;
    auto& count = g_registry.get<TestItem>(item).count;
    if (amount > count) return false;
    count -= amount;
    if (count == 0) g_registry.destroy(item);
    return true;
}
}

namespace InventorySystem {
bool CanHandleItems(entt::entity e, bool, bool) { return ecs::PlayerRuntime::IsPC(e); }
int GetEmptyInventory(entt::entity, uint8_t) { return inventoryFull ? -1 : 0; }
bool AddToCharacter(entt::entity, entt::entity, TItemPos, bool) { return true; }
bool AddToCharacter(entt::entity, entt::entity, TItemPos) { return true; }
entt::entity RemoveFromCharacter(entt::entity) { return entt::null; }
void SyncQuickslot(entt::entity, uint16_t, uint16_t, uint16_t) {}
}

namespace ecs::PointSystem {
int64_t GetGold(entt::entity) { return playerGold; }
void Change(entt::entity, uint8_t point, int64_t amount, bool, bool, bool) {
    if (point == POINT_GOLD) { playerGold += amount; goldChanges += amount; }
}
}

namespace ecs::SessionSystem {
void Save(entt::entity) {}
void SaveReal(entt::entity) {}
bool IsSafeboxOpen(entt::entity) { return false; }
bool IsCubeOpen(entt::entity) { return false; }
}

namespace ecs {
void ChatSystem::Send(entt::entity, uint8_t, const char*, ...) {}
void ChatSystem::SendNew(entt::entity, uint8_t, uint32_t, const char*, ...) {}
}

namespace ecs::SocialSystem {
entt::entity GetShop(entt::entity e) {
    const auto* state = g_registry.try_get<ecs::ShopState>(e);
    return state ? state->currentShop : entt::null;
}
entt::entity GetMyShop(entt::entity e) {
    const auto* state = g_registry.try_get<ecs::ShopState>(e);
    return state ? state->myShop : entt::null;
}
void SetShop(entt::entity e, entt::entity shop) {
    auto& state = g_registry.get_or_emplace<ecs::ShopState>(e);
    state.currentShop = shop;
    if (shop == entt::null)
        state.shopOwner = entt::null;
}
entt::entity GetShopOwner(entt::entity e) {
    const auto* state = g_registry.try_get<ecs::ShopState>(e);
    return state ? state->shopOwner : entt::null;
}
void SetShopOwner(entt::entity e, entt::entity owner) {
    g_registry.get_or_emplace<ecs::ShopState>(e).shopOwner = owner;
}
bool HasExchange(entt::entity) { return false; }
void SetMyShopTime(entt::entity) {}
uint32_t GetLastBuySellTime(entt::entity e) {
    const auto* timers = g_registry.try_get<ecs::ShopTimers>(e);
    return timers ? timers->lastBuySellTime : 0;
}
void SetLastBuySellTime(entt::entity e, uint32_t when) {
    g_registry.get_or_emplace<ecs::ShopTimers>(e).lastBuySellTime = when;
}
}

namespace {
// ---------------------------------------------------------------------------
// Table shops: initialize, lookup and teardown
// ---------------------------------------------------------------------------
void NpcShopLifecycle() {
    Reset();
    auto table = MakeNpcTable(101, 501);

    Check(shop_manager.Initialize(table.data(), 1), "NPC table did not load");
    const entt::entity shop = shop_manager.Get(101);
    Check(shop != entt::null && ShopSystem::IsValid(shop), "shop vnum lookup failed");
    Check(shop_manager.GetByNPCVnum(501) == shop, "shopkeeper lookup failed");
    Check(shop_manager.GetByNPCVnum(9999) == entt::null, "unknown shopkeeper resolved");
    Check(ShopSystem::GetNPCVnum(shop) == 501, "shopkeeper vnum wrong");
    Check(!ShopSystem::IsPCShop(shop), "NPC shop counted as personal shop");

    Check(ShopSystem::GetNumberByVnum(shop, 13001) == 5, "item count wrong");
    Check(ShopSystem::GetNumberByVnum(shop, 13002) == 2, "second item count wrong");
    Check(ShopSystem::GetNumberByVnum(shop, 13003) == 0, "unknown item counted");
    Check(!ShopSystem::IsSellingItem(shop, 13001), "prototype shop reports item identity");

    // Second initialize on a populated manager is refused.
    Check(!shop_manager.Initialize(table.data(), 1), "second initialize accepted");

    shop_manager.Destroy();
    Check(!ShopSystem::IsValid(shop), "destroy left the shop entity");
    Check(shop_manager.Get(101) == entt::null, "destroy left the index");

    // A stale handle is a no-op through the whole surface.
    Check(!ShopSystem::IsPCShop(shop) && ShopSystem::GetVnum(shop) == 0, "stale handle answered state");
    Check(ShopSystem::Buy(shop, entt::null, 0) == SHOP_SUBHEADER_GC_END, "stale handle bought");
    ShopSystem::Destroy(shop);
}

// ---------------------------------------------------------------------------
// Personal shop: create, index, guest window, teardown
// ---------------------------------------------------------------------------
void PersonalShopLifecycle() {
    Reset();
    const entt::entity owner = ActorEntity(11, 11, true);
    const entt::entity guest = ActorEntity(12, 12, true);

    std::vector<TShopItemTable> items(1);
    items[0].vnum = 13001;
    items[0].count = 1;
    items[0].price = 500;
    items[0].pos = TItemPos(INVENTORY, 0);
    items[0].display_pos = 3;

    const entt::entity myShop = shop_manager.CreatePCShop(owner, items.data(), 1);
    Check(myShop != entt::null && ShopSystem::IsValid(myShop), "personal shop was not created");
    Check(ShopSystem::IsPCShop(myShop), "personal shop is not marked as such");
    Check(ShopSystem::GetVnum(myShop) == 0 && ShopSystem::GetNPCVnum(myShop) == 0, "personal shop kept a table identity");
    Check(ecs::SocialSystem::GetMyShop(owner) == entt::null, "manager wrote the character relation");
    Check(shop_manager.FindPCShop(ecs::PlayerRuntime::GetPacketVID(owner)) == myShop, "VID lookup failed");
    Check(shop_manager.FindPCShop(4242) == entt::null, "unknown VID resolved");
    Check(shop_manager.CreatePCShop(owner, items.data(), 1) == entt::null, "duplicate personal shop");

    // Browsing: the guest relation and the window packet.
    packetsSent = 0;
    Check(ShopSystem::AddGuest(myShop, guest, ecs::PlayerRuntime::GetPacketVID(owner), false), "guest was refused");
    Check(ecs::SocialSystem::GetShop(guest) == myShop, "guest relation missing");
    Check(ShopOf(myShop).guests.count(guest) == 1, "guest set missing the guest");
    Check(packetsSent > 0, "guest window was not sent");
    Check(!ShopSystem::AddGuest(myShop, guest, 1, false), "second window accepted");

    packetsSent = 0;
    ShopSystem::RemoveGuest(myShop, guest);
    Check(ecs::SocialSystem::GetShop(guest) == entt::null, "guest relation survived the close");
    Check(ShopOf(myShop).guests.empty(), "guest set survived the close");
    Check(packetsSent > 0, "close packet missing");

    // Destroy clears the relation and retires the entity.
    Check(ShopSystem::AddGuest(myShop, guest, 1, false), "rejoin refused");
    shop_manager.DestroyPCShop(owner);
    Check(!g_registry.valid(myShop) && shop_manager.FindPCShop(ecs::PlayerRuntime::GetPacketVID(owner)) == entt::null,
        "personal shop survived destroy");
    Check(ecs::SocialSystem::GetShop(guest) == entt::null, "destroy left the guest relation");
}

// ---------------------------------------------------------------------------
// Buy guards
// ---------------------------------------------------------------------------
void BuyGuards() {
    Reset();
    auto table = MakeNpcTable(101, 501);
    Check(shop_manager.Initialize(table.data(), 1), "NPC table did not load");
    const entt::entity shop = shop_manager.Get(101);
    const entt::entity guest = ActorEntity(21, 21, true);

    Check(ShopSystem::Buy(shop, guest, 0) == SHOP_SUBHEADER_GC_END, "buy without a window succeeded");

    Check(ShopSystem::AddGuest(shop, guest, 0, false), "guest was refused");
    Check(ShopSystem::Buy(shop, guest, 200) == SHOP_SUBHEADER_GC_INVALID_POS, "invalid position accepted");

    playerGold = 100;
    Check(ShopSystem::Buy(shop, guest, 0) == SHOP_SUBHEADER_GC_NOT_ENOUGH_MONEY, "underpaid buy succeeded");

    playerGold = 100000;
    inventoryFull = true;
    Check(ShopSystem::Buy(shop, guest, 0) == SHOP_SUBHEADER_GC_INVENTORY_FULL, "full inventory accepted");
    Check(goldChanges == 0, "gold moved on a refused buy");
    Check(!destroyedItems.empty(), "refused buy leaked the created item");

    // A personal shop whose grid entry lost its item is sold out, not sold.
    const entt::entity owner = ActorEntity(22, 22, true);
    std::vector<TShopItemTable> pcItems(1);
    pcItems[0].vnum = 13001;
    pcItems[0].count = 1;
    pcItems[0].price = 100;
    pcItems[0].pos = TItemPos(INVENTORY, 0);
    pcItems[0].display_pos = 0;
    const entt::entity pcShop = shop_manager.CreatePCShop(owner, pcItems.data(), 1);
    Check(pcShop != entt::null, "personal shop for the guard was not created");
    const entt::entity pcGuest = ActorEntity(23, 23, true);
    Check(ShopSystem::AddGuest(pcShop, pcGuest, 0, false), "guest was refused");
    // The prototype item of the fixture never exists as an entity.
    Check(ShopSystem::Buy(pcShop, pcGuest, 0) == SHOP_SUBHEADER_GC_SOLD_OUT, "stale personal shop item sold");

    shop_manager.Destroy();
}

// ---------------------------------------------------------------------------
// Extended (tabbed) shops
// ---------------------------------------------------------------------------
void ExtendedShop() {
    Reset();
    const entt::entity shop = g_registry.create();
    g_registry.emplace<ecs::ShopData>(shop);
    ShopSystem::Initialize(shop);
    {
        auto& state = g_registry.get<ecs::ShopData>(shop);
        state.vnum = 0;
        state.npcVnum = 601;
        state.extended = true;
    }

    TShopTableEx tab {};
    tab.dwVnum = 202;
    tab.name = "tabs";
    tab.coinType = SHOP_COIN_TYPE_GOLD;
    tab.items[0].vnum = 13001;
    tab.items[0].price = 100;
    tab.items[0].count = 1;

    Check(ShopSystem::AddShopTable(shop, tab), "first tab refused");
    Check(ShopSystem::AddShopTable(shop, tab) == false, "duplicate tab accepted");
    Check(ShopSystem::GetTabCount(shop) == 1, "tab count wrong");
    Check(!ShopSystem::IsSellingItem(shop, 13001), "extended shop reports item identity");

    const entt::entity guest = ActorEntity(31, 31, true);
    packetsSent = 0;
    Check(ShopSystem::AddGuest(shop, guest, 0, false), "extended guest was refused");
    Check(packetsSent > 0, "extended window was not sent");

    Check(ShopSystem::Buy(shop, guest, 250) == SHOP_SUBHEADER_GC_INVALID_POS, "unknown tab accepted");
    Check(ShopSystem::MultipleBuy(shop, guest, 0, 2) == SHOP_SUBHEADER_GC_INVALID_POS, "extended multiple buy accepted");

    ShopSystem::Destroy(shop);
    Check(ecs::SocialSystem::GetShop(guest) == entt::null, "destroy left the extended guest");
}

// ---------------------------------------------------------------------------
// Destroy with several guests and stale handles
// ---------------------------------------------------------------------------
void DestroyReleasesEveryGuest() {
    Reset();
    auto table = MakeNpcTable(101, 501);
    Check(shop_manager.Initialize(table.data(), 1), "NPC table did not load");
    const entt::entity shop = shop_manager.Get(101);

    std::vector<entt::entity> guests;
    for (int i = 0; i < 3; ++i)
        guests.push_back(ActorEntity(40 + i, 40 + i, true));

    for (const entt::entity guest : guests)
        Check(ShopSystem::AddGuest(shop, guest, 0, false), "guest was refused");

    shop_manager.Destroy();
    for (const entt::entity guest : guests)
        Check(ecs::SocialSystem::GetShop(guest) == entt::null, "destroy left a guest relation");
    Check(!ShopSystem::IsValid(shop), "destroy left the shop");
    ShopSystem::Destroy(shop);
    ShopSystem::RemoveGuest(shop, guests.front());
    Check(ShopSystem::GetNumberByVnum(shop, 13001) == 0, "stale handle answered items");
}
}

int main() {
    try {
        NpcShopLifecycle();
        PersonalShopLifecycle();
        BuyGuards();
        ExtendedShop();
        DestroyReleasesEveryGuest();
        Reset();
        std::cout << "Shop lifecycle checks passed: " << checks << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
