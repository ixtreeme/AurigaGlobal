#include "../../SRC/Server/GameServer/core/stdafx.h"
#include "../../SRC/Server/GameServer/social/shop.h"
#include "../../SRC/Server/GameServer/social/shop_manager.h"
#include "../../SRC/Server/GameServer/item/item_manager.h"
#include "../../SRC/Server/GameServer/core/log.h"
#include "../../SRC/Server/GameServer/network/db.h"
#include "../../SRC/Server/GameServer/network/desc.h"
#include "../../SRC/Server/GameServer/network/desc_client.h"
#include "../../SRC/Server/GameServer/core/config.h"
#include "../../SRC/Server/GameServer/quest/questmanager.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/components/identity_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/social_components.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PointSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/InventorySystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ItemSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SessionSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SocialSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ChatSystem.hpp"
#include <Core/Logging.hpp>
#include <Core/shop_limits.h>
#include "../../SRC/Server/GameServer/ecs/ResourceAdmission.hpp"
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// Real shop.cpp + shop_manager.cpp with entity-only fixtures. Descriptors,
// items, points, sessions and the DB are doubles. NPC inventory commits are
// a controlled boundary here; QuickslotTests runs the actual transaction.
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
bool factoryFails = false, consumptionFails = false;
int createdItems = 0, moneyLogs = 0;
std::function<void()> onFactory, onSpace, onPoint, onSave, onRetire;
std::unordered_map<uint32_t, int> currencyItems;
std::vector<entt::entity> destroyedItems;
TItemTable testItemProto {};

struct Actor {
    uint32_t pid = 0;
    uint32_t vid = 0;
    int32_t x = 0;
    int32_t y = 0;
    int32_t map = 1;
    uint8_t empire = 0;
    bool pc = true;
    std::string name;
};

struct TestItem {
    uint32_t vnum = 0;
    uint32_t count = 1;
    uint32_t id = 0;
    entt::entity owner = entt::null;
    uint8_t window = RESERVED_WINDOW;
    uint16_t cell = 0;
    int64_t price = 0;
    int flags = 0;
    bool locked = false, pending = false, exchanging = false;
};

void Check(bool value, const char* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}

void Reset() {
    onFactory = onSpace = onPoint = onSave = onRetire = {};
    g_registry.clear();
    shop_manager.Destroy();
    packetsSent = 0;
    goldChanges = 0;
    playerGold = 0;
    inventoryFull = false;
    destroyedItems.clear();
    factoryFails = consumptionFails = false;
    createdItems = moneyLogs = 0;
    g_BuySellTimeLimitValue = 0;
    currencyItems.clear();
}

entt::entity ActorEntity(uint32_t pid, uint32_t vid, bool pc) {
    const auto entity = g_registry.create();
    auto& actor = g_registry.emplace<Actor>(entity);
    actor.pid = pid;
    actor.vid = vid;
    actor.pc = pc;
    actor.name = pc ? "player" : "npc";
    g_registry.emplace<ecs::TagCharacter>(entity);
    if (pc) g_registry.emplace<ecs::PlayerID>(entity, ecs::PlayerID{pid});
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
DBManager::DBManager() = default;
DBManager::~DBManager() = default;




quest::CQuestManager::CQuestManager() = default;
quest::CQuestManager::~CQuestManager() = default;
quest::PC::~PC() = default;
quest::NPC::~NPC() = default;

entt::entity ITEM_MANAGER::CreateItem(uint32_t vnum, uint32_t count, uint32_t id, bool, int, bool) {
    ++createdItems;
    if (onFactory) onFactory();
    if (factoryFails) return entt::null;
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
void DBManager::SendMoneyLog(uint8_t, uint32_t, int64_t) { ++moneyLogs; }
CAsyncSQL::CAsyncSQL() : m_queueLane(0) {}
CAsyncSQL::~CAsyncSQL() = default;
CSemaphore::CSemaphore() = default;
CSemaphore::~CSemaphore() = default;
int64_t quest::CQuestManager::GetEventFlag(const std::string&) { return 0; }
const std::string& LocaleService_GetBasePath() { static const std::string path = "/never/read"; return path; }
void ContinueOnFatalError() {}

namespace ecs::PlayerRuntime {
bool IsPC(entt::entity e) { const auto* a = g_registry.valid(e) ? g_registry.try_get<Actor>(e) : nullptr; return a && a->pc; }
bool IsValid(entt::entity e) { return e != entt::null && g_registry.valid(e); }
std::string_view GetName(entt::entity e) { const auto* a = g_registry.try_get<Actor>(e); return a ? std::string_view(a->name) : std::string_view("unknown"); }
uint32_t GetPlayerID(entt::entity e) { const auto* a = g_registry.try_get<Actor>(e); return a ? a->pid : 0; }
uint32_t GetPacketVID(entt::entity e) { const auto* a = g_registry.try_get<Actor>(e); return a ? a->vid : 0; }
uint32_t GetRaceNum(entt::entity e) { const auto* a = g_registry.try_get<Actor>(e); return a ? a->vid : 0; }
int32_t GetX(entt::entity e) { const auto* a = g_registry.try_get<Actor>(e); return a ? a->x : 0; }
int32_t GetY(entt::entity e) { const auto* a = g_registry.try_get<Actor>(e); return a ? a->y : 0; }
int32_t GetMapIndex(entt::entity e) { const auto* a = g_registry.try_get<Actor>(e); return a ? a->map : 0; }
uint8_t GetEmpire(entt::entity e) { const auto* a = g_registry.try_get<Actor>(e); return a ? a->empire : 0; }
uint8_t GetGMLevel(entt::entity) { return GM_PLAYER; }
LPDESC GetDesc(entt::entity e) { return IsPC(e) ? reinterpret_cast<LPDESC>(static_cast<uintptr_t>(0x3000 + entt::to_entity(e))) : nullptr; }
}

namespace ItemSystem {
bool IsValidItem(entt::entity e) { return e != entt::null && g_registry.valid(e) && g_registry.all_of<TestItem>(e); }
uint32_t GetItemVnum(entt::entity e) { return g_registry.get<TestItem>(e).vnum; }
uint32_t GetItemCount(entt::entity e) { return g_registry.get<TestItem>(e).count; }
uint32_t GetItemID(entt::entity e) { return g_registry.get<TestItem>(e).id; }
uint8_t GetItemSize(entt::entity) { return 1; }
entt::entity GetItemOwner(entt::entity e) { return g_registry.get<TestItem>(e).owner; }
uint8_t GetItemWindow(entt::entity e) { return g_registry.get<TestItem>(e).window; }
entt::entity GetItem(entt::entity owner, TItemPos position) {
    for (auto item : g_registry.view<TestItem>()) {
        const auto& state = g_registry.get<TestItem>(item);
        if (state.owner == owner && state.window == position.window_type && state.cell == position.cell) return item;
    }
    return entt::null;
}
entt::entity GetInventoryItem(entt::entity, uint16_t) { return entt::null; }
entt::entity GetExtraInventoryItem(entt::entity, uint16_t) { return entt::null; }
const TItemTable* GetItemProto(entt::entity) { return &testItemProto; }
uint32_t GetItemSocket(entt::entity, int) { return 0; }
TPlayerItemAttribute GetItemAttribute(entt::entity, int) { return {}; }
int16_t GetItemLockedAttributeIndex(entt::entity) { return -1; }
uint16_t GetItemCell(entt::entity e) { return g_registry.get<TestItem>(e).cell; }
bool IsExtraItem(entt::entity) { return false; }
bool IsItemStackable(entt::entity) { return false; }
bool IsItemEquipped(entt::entity) { return false; }
bool IsItemLocked(entt::entity e) { return g_registry.get<TestItem>(e).locked; }
bool IsItemConsumptionPending(entt::entity e) { return g_registry.get<TestItem>(e).pending; }
bool IsItemExchanging(entt::entity e) { return g_registry.get<TestItem>(e).exchanging; }
uint32_t GetItemAntiFlag(entt::entity) { return 0; }
int32_t GetItemFlags(entt::entity e) { return g_registry.get<TestItem>(e).flags; }
int64_t GetItemShopBuyPrice(entt::entity e) { return g_registry.get<TestItem>(e).price; }
int GetEmptyExtraInventory(entt::entity, entt::entity) { return inventoryFull ? -1 : 0; }
int CountItem(entt::entity, uint32_t vnum) { return currencyItems[vnum]; }
int CountTypeItem(entt::entity, uint8_t) { return 0; }
bool RemoveSpecifyItemEcs(entt::entity, uint32_t, uint32_t, bool) { return false; }
void RemoveSpecifyTypeItem(entt::entity, uint8_t, int) {}
bool AddItemCountEcs(entt::entity item, int amount) { g_registry.get<TestItem>(item).count += amount; return true; }
bool SetItemCountEcs(entt::entity item, uint32_t amount) { g_registry.get<TestItem>(item).count = amount; return true; }
bool DestroyItemEntityEcs(entt::entity item, const char*) {
    if (onRetire) onRetire();
    if (consumptionFails) return false;
    destroyedItems.push_back(item);
    if (g_registry.valid(item)) g_registry.destroy(item);
    return true;
}
bool FlushDelayedSaveEcs(entt::entity) { return true; }
const char* GetItemName(entt::entity item) { return g_registry.valid(item) ? "item" : "unknown"; }
bool ConsumeItemEcs(entt::entity item, uint32_t amount) {
    if (onRetire) onRetire();
    if (consumptionFails) return false;
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
int GetEmptyInventory(entt::entity, uint8_t) { if (onSpace) onSpace(); return inventoryFull ? -1 : 0; }
bool AddToCharacter(entt::entity item, entt::entity owner, TItemPos position, bool) {
    if (!ItemSystem::IsValidItem(item) || !g_registry.valid(owner)) return false;
    auto& state = g_registry.get<TestItem>(item);
    state.owner = owner; state.window = position.window_type; state.cell = position.cell;
    return true;
}
bool AddToCharacter(entt::entity item, entt::entity owner, TItemPos pos) { return AddToCharacter(item, owner, pos, false); }
entt::entity RemoveFromCharacter(entt::entity) { return entt::null; }
void SyncQuickslot(entt::entity, uint16_t, uint16_t, uint16_t) {}
NpcTradeResult BuyFromNpc(entt::entity buyer, const NpcPurchase& request, const std::function<bool()>& guard) {
    if (!guard()) return NpcTradeResult::Rejected;
    const auto item = item_manager.CreateItem(request.vnum, request.count);
    if (!ItemSystem::IsValidItem(item)) return NpcTradeResult::FactoryFailed;
    const auto abort = [&](NpcTradeResult result) {
        if (ItemSystem::IsValidItem(item) && ItemSystem::GetItemOwner(item) == entt::null)
            ItemSystem::DestroyItemEntityEcs(item, "SHOP_ABORT");
        return result;
    };
    if (!guard()) return abort(NpcTradeResult::Rejected);
    const int cell = GetEmptyInventory(buyer, 1);
    if (cell < 0) return abort(NpcTradeResult::InventoryFull);
    if (!guard()) return abort(NpcTradeResult::Rejected);
    if (playerGold < request.gold) return abort(NpcTradeResult::NotEnoughGold);
    ecs::PointSystem::Change(buyer, POINT_GOLD, -request.gold, false);
    AddToCharacter(item, buyer, TItemPos(INVENTORY, uint16_t(cell)), true);
    ecs::SocialSystem::SetLastBuySellTime(buyer, get_dword_time());
    ecs::SessionSystem::Save(buyer);
    return NpcTradeResult::Committed;
}
bool SellToNpc(entt::entity owner, TItemPos source, uint32_t amount, int64_t credit, const std::function<bool()>& guard) {
    if (!guard()) return false;
    const auto item = ItemSystem::GetItem(owner, source);
    const bool consumed = amount == ItemSystem::GetItemCount(item) ?
        ItemSystem::DestroyItemEntityEcs(item, "SELL") : ItemSystem::ConsumeItemEcs(item, amount);
    if (!consumed) return false;
    ecs::SocialSystem::SetLastBuySellTime(owner, get_dword_time());
    ecs::PointSystem::Change(owner, POINT_GOLD, credit, false);
    ++moneyLogs;
    return true;
}
}

namespace ecs::PointSystem {
int64_t GetGold(entt::entity) { return playerGold; }
void Change(entt::entity, uint8_t point, int64_t amount, bool, bool, bool) {
    if (point == POINT_GOLD) { playerGold += amount; goldChanges += amount; }
    if (onPoint) onPoint();
}
}

namespace ecs::SessionSystem {
void Save(entt::entity) { if (onSave) onSave(); }
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

auriga::player_session::Key NativeKey(entt::entity actor) {
    auriga::player_session::Token token{}; token.fill(7);
    return {token, token, token, 1, 500, ecs::PlayerRuntime::GetPlayerID(actor)};
}
auto Pause(entt::entity actor) {
    const auto key = NativeKey(actor);
    return ecs::resource_admission::Prepare(actor, key, key.session);
}
struct PurchaseFixture {
    entt::entity shop, buyer, keeper;
    explicit PurchaseFixture(bool extended = false) {
        Reset();
        buyer = ActorEntity(101, 101, true);
        keeper = ActorEntity(501, 501, false);
        if (extended) {
            shop = g_registry.create(); g_registry.emplace<ecs::ShopData>(shop);
            ShopOf(shop).extended = true;
            TShopTableEx tab{}; tab.coinType = SHOP_COIN_TYPE_GOLD;
            tab.items[0].vnum = 13001; tab.items[0].count = 5; tab.items[0].price = 1000;
            Check(ShopSystem::AddShopTable(shop, tab), "fixture tab refused");
        } else {
            auto table = MakeNpcTable(101, 501);
            Check(shop_manager.Initialize(table.data(), 1), "fixture table refused");
            shop = shop_manager.Get(101);
        }
        Check(ShopSystem::AddGuest(shop, buyer, 501, false), "fixture guest refused");
        ecs::SocialSystem::SetShopOwner(buyer, keeper);
        playerGold = 100000;
    }
    entt::entity SellItem(uint32_t count = 5) {
        const auto item = ItemEntity(13001, count, 42);
        auto& data = g_registry.get<TestItem>(item);
        data.owner = buyer; data.window = INVENTORY; data.price = 100;
        return item;
    }
};

void PriceBoundsAndMalformedOffers() {
    using auriga::shop_limits::Multiply;
    Check(!Multiply(-1, 0) && !Multiply(-1, 1), "negative price accepted");
    Check(Multiply(INT64_MAX, 1) == INT64_MAX && !Multiply(INT64_MAX, 2) &&
        Multiply(0, UINT64_MAX) == 0 && Multiply(1, uint64_t(INT64_MAX)) == INT64_MAX &&
        !Multiply(1, UINT64_MAX), "multiplication boundary failed");
    for (int64_t price = 0; price < 250; ++price)
        for (uint64_t count = 0; count < 200; ++count)
            Check(Multiply(price, count) == price * int64_t(count), "valid small quote changed");
    for (int defect = 0; defect < 5; ++defect) {
        PurchaseFixture f;
        auto& row = ShopOf(f.shop).items[0];
        if (defect == 0) row.price = -100;
        if (defect == 1) row.price = INT64_MAX;
        if (defect == 2) row.count = 0;
        if (defect == 3) row.count = -1;
        if (defect == 4) row.vnum = 0;
        Check(ShopSystem::Buy(f.shop, f.buyer, 0) != SHOP_SUBHEADER_GC_OK &&
            goldChanges == 0 && createdItems == 0, "malformed normal offer reached the factory or debit");
        Check(ShopSystem::MultipleBuy(f.shop, f.buyer, 0, 3) != SHOP_SUBHEADER_GC_OK &&
            goldChanges == 0 && createdItems == 0, "malformed batch quote accepted");
    }
    for (int defect = 0; defect < 4; ++defect) {
        PurchaseFixture f(true);
        auto& tab = ShopOf(f.shop).tabs[0];
        if (defect == 0) tab.items[0].price = INT64_MAX;
        if (defect == 1) tab.coinType = static_cast<EShopCoinType>(255);
        if (defect == 2) tab.items[0].count = 0;
        if (defect == 3) tab.items[0].vnum = 0;
        ShopOf(f.shop).guests[f.buyer] = true;
        Check(ShopSystem::Buy(f.shop, f.buyer, 0) != SHOP_SUBHEADER_GC_OK &&
            goldChanges == 0 && createdItems == 0, "malformed extended quote accepted");
    }
    for (bool batch : {false, true}) {
        PurchaseFixture f;
        auto& row = ShopOf(f.shop).items[0];
        row.itemprice[0] = {500, 3}; row.itemprice[1] = {500, 3};
        currencyItems[500] = batch ? 11 : 5;
        const auto result = batch ? ShopSystem::MultipleBuy(f.shop, f.buyer, 0, 2) : ShopSystem::Buy(f.shop, f.buyer, 0);
        Check(result == SHOP_SUBHEADER_GC_NOT_ENOUGH_ITEM && createdItems == 0 && goldChanges == 0,
            "duplicate currency rows checked independent balances");
        row.itemprice[0].count = UINT32_MAX;
        Check(ShopSystem::MultipleBuy(f.shop, f.buyer, 0, 100) == SHOP_SUBHEADER_GC_NOT_ENOUGH_ITEM,
            "batch item price overflow accepted");
    }
    PurchaseFixture f;
    Check(ShopSystem::MultipleBuy(f.shop, f.buyer, 0, 0) == SHOP_SUBHEADER_GC_INVALID_POS,
        "empty batch accepted");
    factoryFails = true;
    Check(ShopSystem::MultipleBuy(f.shop, f.buyer, 0, 4) == SHOP_SUBHEADER_GC_SOLD_OUT && createdItems == 1,
        "sold out batch counted failures as successful purchases");
}

void PurchaseAdmissionAndCallbacks() {
    for (bool extended : {false, true}) {
        PurchaseFixture f(extended);
        auto paused = Pause(f.buyer);
        Check(paused.has_value(), "buyer native pause failed");
        Check(ShopSystem::Buy(f.shop, f.buyer, 0) == SHOP_SUBHEADER_GC_END && createdItems == 0,
            "paused buyer entered shop purchase");
        Check(ShopSystem::MultipleBuy(f.shop, f.buyer, 0, 2) == SHOP_SUBHEADER_GC_END,
            "paused buyer entered batch");
        shop_manager.Buy(f.buyer, 0);
        Check(goldChanges == 0 && ecs::SocialSystem::GetLastBuySellTime(f.buyer) == 0,
            "paused manager mutated wallet or cooldown");
        paused.reset();
        bool factoryHeld = false, pointHeld = false, saveHeld = false;
        onFactory = [&] { factoryHeld = !Pause(f.buyer); };
        onPoint = [&] { pointHeld = !Pause(f.buyer); };
        onSave = [&] { saveHeld = !Pause(f.buyer); };
        Check(ShopSystem::Buy(f.shop, f.buyer, 0) == SHOP_SUBHEADER_GC_OK && playerGold == 99000 &&
            factoryHeld && pointHeld && saveHeld, "purchase activity did not surround preparation/debit/publication");
        Check(Pause(f.buyer).has_value(), "purchase leaked admission");
    }
    for (bool extended : {false, true})
        for (int defect = 0; defect < 6; ++defect) {
            PurchaseFixture f(extended);
            const auto before = playerGold;
            const auto mutate = [&] {
                if (defect == 0) ecs::resource_admission::Invalidate(f.buyer);
                if (defect == 1) g_registry.destroy(f.shop);
                if (defect == 2) {
                    if (extended) ShopOf(f.shop).tabs[0].items[0].price = 2000;
                    else ShopOf(f.shop).items[0].price = 2000;
                }
                if (defect == 3) ShopOf(f.shop).guests.erase(f.buyer);
                if (defect == 4) playerGold = 0;
                if (defect == 5) {
                    if (extended) ShopOf(f.shop).tabs.clear();
                    else ShopOf(f.shop).items.clear();
                }
            };
            if (defect % 2) onSpace = mutate; else onFactory = mutate;
            Check(ShopSystem::Buy(f.shop, f.buyer, 0) != SHOP_SUBHEADER_GC_OK && goldChanges == 0 &&
                playerGold == (defect == 4 ? 0 : before) && destroyedItems.size() == 1,
                "stale preparation debited resources or leaked its clone");
            onFactory = onSpace = {};
        }
    // Seller's independent pause must block the buyer without leaving the
    // buyer active. The actual personal-shop transfer remains a later phase.
    PurchaseFixture f;
    const auto seller = ActorEntity(202, 202, true);
    ShopOf(f.shop).owner = seller;
    auto sellerPause = Pause(seller);
    Check(sellerPause && ShopSystem::Buy(f.shop, f.buyer, 0) == SHOP_SUBHEADER_GC_END &&
        goldChanges == 0 && createdItems == 0 && Pause(f.buyer).has_value(),
        "seller pause allowed a partial participant admission");
    shop_manager.Buy(f.buyer, 0);
    Check(ecs::SocialSystem::GetLastBuySellTime(f.buyer) == 0 && goldChanges == 0,
        "seller pause spent buyer cooldown through manager");
}

void SellValidationAndAdmission() {
    for (int defect = 0; defect < 10; ++defect) {
        PurchaseFixture f;
        const auto item = f.SellItem();
        auto& data = g_registry.get<TestItem>(item);
        if (defect == 0) data.price = -1;
        if (defect == 1) data.price = INT64_MAX;
        if (defect == 2) data.pending = true;
        if (defect == 3) data.exchanging = true;
        if (defect == 4) data.locked = true;
        if (defect == 5) consumptionFails = true;
        if (defect == 6) g_registry.get<Actor>(f.keeper).x = 10000;
        if (defect == 7) ShopOf(f.shop).guests.erase(f.buyer);
        if (defect == 8) playerGold = -1;
        if (defect == 9) g_registry.get<Actor>(f.keeper).map = 2;
        shop_manager.Sell(f.buyer, TItemPos(INVENTORY, 0), 0);
        Check(goldChanges == 0 && ItemSystem::IsValidItem(item) && ItemSystem::GetItemCount(item) == 5 &&
            moneyLogs == 0 && ecs::SocialSystem::GetLastBuySellTime(f.buyer) == 0,
            "refused sale changed resources/log/cooldown");
    }
    for (uint32_t count : {5u, 70000u}) {
        PurchaseFixture f;
        const auto item = f.SellItem(count);
        auto pause = Pause(f.buyer);
        shop_manager.Sell(f.buyer, TItemPos(INVENTORY, 0), 0);
        Check(pause && goldChanges == 0 && ItemSystem::GetItemCount(item) == count, "paused seller mutated resources");
        pause.reset();
        bool retirementHeld = false, pointHeld = false;
        onRetire = [&] { retirementHeld = !Pause(f.buyer); };
        onPoint = [&] { pointHeld = !Pause(f.buyer); };
        shop_manager.Sell(f.buyer, TItemPos(INVENTORY, 0), 0);
        Check(!ItemSystem::IsValidItem(item) && goldChanges == int64_t(count) * 100 &&
            moneyLogs == 1 && retirementHeld && pointHeld &&
            uint32_t(get_dword_time() - ecs::SocialSystem::GetLastBuySellTime(f.buyer)) < 5000 && Pause(f.buyer).has_value(),
            "full sale truncated quantity or escaped admission");
    }
    PurchaseFixture f;
    const auto item = f.SellItem();
    Check(!auriga::shop_limits::Ready(25, UINT32_MAX - 49, 100) &&
        auriga::shop_limits::Ready(51, UINT32_MAX - 49, 100), "wrapped cooldown arithmetic failed");
    g_BuySellTimeLimitValue = 100000;
    ecs::SocialSystem::SetLastBuySellTime(f.buyer, get_dword_time());
    shop_manager.Sell(f.buyer, TItemPos(INVENTORY, 0), 2);
    Check(goldChanges == 0 && ItemSystem::GetItemCount(item) == 5, "wrapped cooldown expired too early");
    ecs::SocialSystem::SetLastBuySellTime(f.buyer, get_dword_time() - 100001);
    shop_manager.Sell(f.buyer, TItemPos(INVENTORY, 0), 2);
    Check(goldChanges == 200 && ItemSystem::GetItemCount(item) == 3 &&
        uint32_t(get_dword_time() - ecs::SocialSystem::GetLastBuySellTime(f.buyer)) < 5000, "expired cooldown did not recover");
}
}

int main() {
    try {
        quest::CQuestManager questManager;
        DBManager dbManager;
        NpcShopLifecycle();
        PersonalShopLifecycle();
        BuyGuards();
        ExtendedShop();
        DestroyReleasesEveryGuest();
        PriceBoundsAndMalformedOffers();
        PurchaseAdmissionAndCallbacks();
        SellValidationAndAdmission();
        Reset();
        std::cout << "Shop lifecycle checks passed: " << checks << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
