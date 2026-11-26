#include "../../SRC/Server/GameServer/core/stdafx.h"
#include "../../SRC/Server/GameServer/item/item.h"
#include "../../SRC/Server/GameServer/item/item_manager.h"
#include "../../SRC/Server/GameServer/combat/affect.h"
#include "../../SRC/Server/GameServer/entity/char.h"
#include "../../SRC/Server/GameServer/network/desc.h"
#include "../../SRC/Server/GameServer/core/log.h"
#include "../../SRC/Server/GameServer/social/party.h"
#include "../../SRC/Server/GameServer/core/constants.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/ResourceAdmission.hpp"
#include "../../SRC/Server/GameServer/ecs/EntityFactory.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SessionSystem.hpp"
#include "../../SRC/Server/GameServer/world/sectree.h"
#include "../../SRC/Server/GameServer/ecs/ItemInvariants.hpp"
#include "../../SRC/Server/GameServer/ecs/EventDispatcher.hpp"
#include "../../SRC/Server/GameServer/ecs/events.hpp"
#include "../../SRC/Server/GameServer/ecs/components/item_proto_components.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ItemSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/AffectSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ChatSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SocialSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/InventorySystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/MountSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/NetworkSyncSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PointSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/WoodcuttingSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/HerbalismSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/ItemRegistry.hpp"
#include <Core/Logging.hpp>
#include "../../SRC/Server/GameServer/ecs/components/inventory_components.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/CombatSystem.hpp"
#include "../../SRC/Server/GameServer/world/fishing.h"
#include "../../SRC/Server/GameServer/item/refine.h"
#include "../../SRC/Server/GameServer/item/unique_item.h"
#include "../../SRC/Server/GameServer/quest/questmanager.h"
#include "../../SRC/Server/GameServer/dungeon/RuneDungeon.h"
#include "../../SRC/Server/GameServer/dungeon/Halloween2022Dungeon.h"
#include "../../SRC/Server/GameServer/dungeon/VikingDungeon.h"
#include <cstdarg>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <source_location>

entt::registry g_registry;
entt::dispatcher g_dispatcher;
int passes_per_sec = 25;
int test_server = 0;
int g_iDbLogLevel = 0;
const int aiAccessorySocketDegradeTime[ITEM_ACCESSORY_SOCKET_MAX_NUM + 1] = {};

namespace {
int checks = 0;
struct Actor { bool pc = false; uint32_t race = 0; int32_t x = 0, y = 0, map = 1; bool dead = false; bool observer = false, ground = false; entt::entity party = entt::null; };
bool pickupTest = false, pickupAllowed = true, rejectPickupPlace = false, rejectPickupDestroy = false;
int pickupCell = 0, pickupDetaches = 0, pickupPlacements = 0, pickupRestores = 0, pickupSaves = 0;
int lastPickupWindow = 0;
int64_t pickupGold = 0;
std::function<void(entt::entity)> onPickupDetach, onPickupPlace, onPickupDestroy;
std::function<void(entt::entity)> onPickupCredit, onPickupSave, onPickupNotify, onPickupFindCell;
std::function<ItemSystem::StackMergeResult(entt::entity, entt::entity, entt::entity)> onPickupMerge;
entt::entity pickupClaim = entt::null, creditedOwner = entt::null;
bool npcTest = false;
int revivals = 0, feeds = 0;
std::string receivedItemName;
bool failAllocation = false;
bool wearStopTest = false;
int wearUpdates = 0;
std::function<void(entt::entity)> onUpdate;
std::vector<LPEVENT> queued;
std::function<void(entt::entity)> onCreate, onPublish, onComponent;
std::function<void()> onCancel;
void Check(bool value, const char* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
[[noreturn]] void Unexpected(const std::source_location where = std::source_location::current()) {
    throw std::runtime_error(std::string("unexpected legacy/live service: ") + where.function_name());
}
void Published(const ecs::EvItemExpired& event) { if (onPublish) onPublish(event.itemEntity); }
void Constructed(entt::registry&, entt::entity item) { if (onComponent) onComponent(item); }
void Reset() {
    pickupTest = rejectPickupPlace = rejectPickupDestroy = false; pickupAllowed = true;
    pickupCell = pickupDetaches = pickupPlacements = pickupRestores = pickupSaves = 0; pickupGold = 0;
    onPickupDetach = onPickupPlace = onPickupDestroy = {};
    onPickupCredit = onPickupSave = onPickupNotify = onPickupFindCell = {};
    onPickupMerge = {}; pickupClaim = creditedOwner = entt::null;
    onCreate = onPublish = onComponent = {}; onCancel = {};
    npcTest = false; revivals = feeds = 0; receivedItemName.clear();
    onUpdate = {}; wearStopTest = false; wearUpdates = 0;
    g_registry.clear(); queued.clear(); failAllocation = false; test_server = 0;
}
struct Fixture {
    TItemTable proto {};
    entt::entity item;
    Fixture() : item(g_registry.create()) {
        proto.cLimitRealTimeFirstUseIndex = -1;
        proto.cLimitTimerBasedOnWearIndex = -1;
        g_registry.emplace<ecs::ItemIdentity>(item).id = 123;
        g_registry.emplace<ecs::ItemSockets>(item);
        g_registry.emplace<ecs::ItemPrototypeMeta>(item);
        g_registry.emplace<ecs::ItemProtoRef>(item).proto = &proto;
    }
    void FirstUse(bool used) {
        proto.cLimitRealTimeFirstUseIndex = 0;
        proto.aLimits[0].bType = LIMIT_REAL_TIME_START_FIRST_USE;
        g_registry.get<ecs::ItemSockets>(item).sockets[1] = used;
    }
};
}

void intrusive_ptr_add_ref(EVENT* event) { ++event->ref_count; }
void intrusive_ptr_release(EVENT* event) { if (!--event->ref_count) delete event; }
LPEVENT event_create_ex(TEVENTFUNC callback, event_info_data* info, int32_t delay) {
    LPEVENT result(new EVENT);
    result->info = info; result->func = callback;
    const auto* itemInfo = dynamic_cast<item_vid_event_info*>(info);
    const auto* wearInfo = dynamic_cast<item_event_info*>(info);
    Check((itemInfo || wearInfo) && delay > 0, "timer lost its entity payload or positive delay");
    const auto item = itemInfo ? itemInfo->item : wearInfo->item;
    if (failAllocation) return nullptr;
    queued.push_back(result);
    if (onCreate) onCreate(item);
    return result;
}
void event_cancel(LPEVENT* event) {
    if (!event || !*event) return;
    (*event)->is_force_to_end = 1;
    event->reset();
    if (onCancel) { auto callback = onCancel; callback(); }
}
EVENTFUNC(real_time_expire_event) { throw std::runtime_error("unexpected timer execution"); }
EVENTFUNC(soul_item_event) { throw std::runtime_error("unexpected timer execution"); }
void ITEM_MANAGER::RemoveItem(entt::entity item, const char*) {
    Check((npcTest || pickupTest) && ItemSystem::IsValidItem(item), "unexpected item removal");
    if (pickupTest) {
        if (onPickupDestroy) onPickupDestroy(item);
        if (rejectPickupDestroy) return;
        CItemRegistry::Instance().Unregister(item);
    }
    if (g_registry.valid(item)) g_registry.destroy(item);
}
ITEM_MANAGER::ITEM_MANAGER() {}
ITEM_MANAGER::~ITEM_MANAGER() {}
namespace AffectSystem {
CAffect* FindAffect(entt::entity, uint32_t, uint8_t) { return nullptr; }
bool RemoveAffect(entt::entity, uint32_t) { return false; }
}
void ecs::ChatSystem::SendNew(entt::entity owner, uint8_t, uint32_t message, const char* format, ...) {
    if (pickupTest && onPickupNotify) onPickupNotify(owner);
    if (!npcTest || (message != 329 && message != 112)) return;
    va_list args; va_start(args, format);
    receivedItemName = va_arg(args, const char*);
    va_end(args);
}
// Link seams for other ItemSystem operations. Tests must never accidentally
// pass by falling back to CHARACTER/CItem or starting live server services.
int MIN(int a, int b) { return std::min(a, b); }
int MINMAX(int low, int value, int high) { return std::clamp(value, low, high); }
int number_ex(int, int, const char*, int) { Unexpected(); }
int32_t event_processing_time(LPEVENT) { if (!wearStopTest) Unexpected(); return passes_per_sec * 10; }
int32_t event_time(LPEVENT) { Unexpected(); }
EVENTFUNC(unique_expire_event) { Unexpected(); }
EVENTFUNC(timer_based_on_wear_expire_event) { Unexpected(); }
EVENTFUNC(accessory_socket_expire_event) { Unexpected(); }
namespace ecs::PlayerRuntime {
LPDESC GetDesc(entt::entity owner) {
    Check(g_registry.valid(owner), "name lookup used a stale owner");
    return nullptr;
}
entt::entity FindByPlayerID(uint32_t) { Unexpected(); }
uint32_t GetPlayerID(entt::entity e) { if (pickupTest) return entt::to_integral(e) + 1; Unexpected(); }
uint8_t GetJob(entt::entity) { Unexpected(); }
std::string_view GetName(entt::entity) { if (pickupTest) return "owner"; Unexpected(); }
int32_t GetX(entt::entity e) { return g_registry.get<Actor>(e).x; }
int32_t GetY(entt::entity e) { return g_registry.get<Actor>(e).y; }
int32_t GetMapIndex(entt::entity e) { return g_registry.get<Actor>(e).map; }
uint32_t GetRaceNum(entt::entity e) { return g_registry.get<Actor>(e).race; }
uint32_t GetPacketVID(entt::entity) { return 99; }
bool IsPC(entt::entity e) { return IsValid(e) && g_registry.get<Actor>(e).pc; }
bool SetQuestNPC(entt::entity, entt::entity) { Unexpected(); }
LPSECTREE GetSectree(entt::entity e) {
    if (!pickupTest) Unexpected();
    static SECTREE tree;
    const auto* actor = g_registry.try_get<Actor>(e);
    return actor && actor->ground ? &tree : nullptr;
}
bool IsValid(entt::entity e) { return e != entt::null && g_registry.valid(e) && g_registry.all_of<Actor>(e); }
}
entt::entity ecs::SocialSystem::GetParty(entt::entity owner) { if (pickupTest) return g_registry.get<Actor>(owner).party; Unexpected(); }
entt::entity InventorySystem::RemoveFromCharacter(entt::entity) { Unexpected(); }
namespace ItemSystem {
entt::entity GetWearItem(entt::entity, uint8_t) { Unexpected(); }
bool UnequipItemEcs(entt::entity, entt::entity) { Unexpected(); }
bool EquipItemEcs(entt::entity, entt::entity, int) { Unexpected(); }
uint32_t GetItemCount(entt::entity item) {
    const auto* count = g_registry.try_get<ecs::ItemCount>(item);
    return count && count->count > 0 ? count->count : 0;
}
bool SetItemCountEcs(entt::entity item, uint32_t count) {
    Check(npcTest && IsValidItem(item), "NPC consumed a stale item");
    if (count)
        g_registry.get<ecs::ItemCount>(item).count = count;
    else
        g_registry.destroy(item);
    return true;
}
void ModifyPoints(entt::entity, bool) { Unexpected(); }
// These fixtures never queue consumption; real queue policy is tested in ItemAttributeTests.
bool IsItemConsumptionPending(entt::entity) { return false; }
bool RefreshItemOwnerPID(entt::entity) { Unexpected(); }
bool SetGroundOwnership(entt::entity, entt::entity, int) { if (pickupTest) return true; Unexpected(); }
bool CanConsumeOwnedItem(entt::entity owner, entt::entity item, uint32_t amount, ItemCostStorage) {
    // Full storage/payment policy is covered by ItemAttributeTests.
    return ecs::PlayerRuntime::IsPC(owner) && IsValidItem(item) && GetItemOwner(item) == owner &&
        GetItemCount(item) >= amount && !IsItemEquipped(item) && !IsItemLocked(item) && !IsItemExchanging(item);
}
}
bool CombatSystem::IsDead(entt::entity e) { return g_registry.get<Actor>(e).dead; }
bool MountSystem::ReviveHorse(entt::entity) { ++revivals; return true; }
void MountSystem::FeedHorse(entt::entity) { ++feeds; }
void NetworkSyncSystem::BroadcastEffect(entt::registry&, entt::entity, uint8_t) {}
void InventorySystem::SetRefineNPC(entt::entity, entt::entity) { Unexpected(); }
quest::CQuestManager::CQuestManager() = default;
quest::CQuestManager::~CQuestManager() = default;
LogManager::LogManager() : m_bIsConnect(false) {}
LogManager::~LogManager() = default;
CSemaphore::CSemaphore() = default;
CSemaphore::~CSemaphore() = default;
CAsyncSQL::CAsyncSQL() : m_queueLane(0) {}
CAsyncSQL::~CAsyncSQL() = default;
quest::NPC::~NPC() = default;
CRuneDungeon& CRuneDungeon::instance() { static CRuneDungeon dungeon; return dungeon; }
CHalloween2022Dungeon& CHalloween2022Dungeon::instance() { static CHalloween2022Dungeon dungeon; return dungeon; }
CVikingDungeon& CVikingDungeon::instance() { static CVikingDungeon dungeon; return dungeon; }
void PartySystem::ChatPacketToAllMemberNew(entt::entity, uint8_t, uint32_t, const char*, ...) { Unexpected(); }
namespace ShopSystem {
bool IsSellingItem(entt::entity, uint32_t) { Unexpected(); }
}
uint32_t PartySystem::GetNearMemberCount(entt::entity) { Unexpected(); }
void MountSystem::ForceClearRidingState(entt::entity) { Unexpected(); }
void ecs::ItemNetworkSystem::SendItemUpdate(entt::registry&, entt::entity item) {
    Check(wearStopTest && ItemSystem::IsValidItem(item), "wear stop published a stale item");
    ++wearUpdates;
    if (onUpdate) onUpdate(item);
}
void ecs::PointSystem::Change(entt::entity, uint8_t, int64_t, bool, bool, bool) { Unexpected(); }
void ITEM_MANAGER::DestroyItem(entt::entity) { Unexpected(); }
TItemTable* ITEM_MANAGER::GetTable(uint32_t) { Unexpected(); }
bool ITEM_MANAGER::IsExtraItem(uint32_t) { Unexpected(); }
void DESC::Packet(const void*, int) { Unexpected(); }
void LogManager::ItemLog(uint32_t, uint32_t, uint32_t, uint32_t, const char*, const char*, const char*, uint32_t) { Unexpected(); }
void LogManager::GoldBarLog(uint32_t, uint32_t, GOLDBAR_HOW, const char*) { Unexpected(); }
namespace logging {
std::shared_ptr<spdlog::logger> GetLogger() {
    static auto logger = std::make_shared<spdlog::logger>("item-runtime"); return logger;
}
std::shared_ptr<spdlog::logger> GetErrorLogger() { return GetLogger(); }
}


SECTREE::SECTREE() = default;
SECTREE::~SECTREE() = default;
int32_t ecs::PlayerRuntime::GetZ(entt::entity) { return 0; }
bool ecs::PlayerRuntime::IsObserverMode(entt::entity e) { return g_registry.get<Actor>(e).observer; }
int64_t ecs::PlayerRuntime::GetRankPoints(entt::entity, int) { return 0; }
bool ecs::PlayerRuntime::SetRankPoints(entt::entity, int, int64_t) { return true; }
quest::PC* quest::CQuestManager::GetPCForce(unsigned int) { return nullptr; }
void ecs::SessionSystem::Save(entt::entity owner) {
    Check(pickupTest && ecs::PlayerRuntime::IsPC(owner), "pickup saved a stale character"); ++pickupSaves;
    if (onPickupSave) onPickupSave(owner);
}
void ItemSystem::GiveGold(entt::entity owner, int64_t amount) {
    Check(pickupTest && ecs::PlayerRuntime::IsPC(owner), "pickup credited stale character"); pickupGold += amount;
    creditedOwner = owner;
    if (onPickupCredit) onPickupCredit(owner);
}
bool ItemSystem::IsOwnership(entt::entity item, entt::entity owner) {
    return pickupTest && IsValidItem(item) && ecs::PlayerRuntime::IsPC(owner) && pickupAllowed &&
        (pickupClaim == entt::null || pickupClaim == owner);
}
ItemSystem::StackMergeResult ItemSystem::MergeItemStacksEcs(entt::entity owner, entt::entity source, entt::entity target,
    uint32_t, StackSource kind) {
    if (pickupTest && onPickupMerge && kind == StackSource::GroundPickup) return onPickupMerge(owner, source, target);
    Unexpected(); // Full stack policy is tested with real ItemAttributeSystem.
}
int InventorySystem::GetEmptyInventory(entt::entity owner, uint8_t) {
    if (onPickupFindCell) onPickupFindCell(owner);
    return pickupCell;
}
int ItemSystem::GetEmptyExtraInventory(entt::entity, entt::entity) { return pickupCell; }
entt::entity InventorySystem::RemoveFromGround(entt::entity item) {
    Check(pickupTest && ItemSystem::IsValidItem(item), "pickup detached stale entity");
    ++pickupDetaches;
    g_registry.get<Actor>(item).ground = false;
    g_registry.get<ecs::ItemLocation>(item).window = RESERVED_WINDOW;
    if (onPickupDetach) onPickupDetach(item);
    return item;
}
bool InventorySystem::AddToCharacter(entt::entity item, entt::entity owner, TItemPos pos, bool) {
    Check(pickupTest && ItemSystem::IsValidItem(item), "pickup placed stale entity");
    if (rejectPickupPlace || ItemSystem::GetItemOwner(item) != entt::null ||
        !ecs::PlayerRuntime::IsPC(owner)) return false;
    ++pickupPlacements; lastPickupWindow = pos.window_type;
    g_registry.get<ecs::ItemOwner>(item).owner = owner;
    g_registry.get<ecs::ItemLocation>(item) = {pos.window_type, pos.cell};
    if (onPickupPlace) onPickupPlace(item);
    return true;
}
bool ItemSystem::PlaceItemOnGround(entt::entity item, int32_t map, const PIXEL_POSITION& pos, int) {
    if (!IsValidItem(item) || GetItemOwner(item) != entt::null || GetItemWindow(item) != RESERVED_WINDOW) return false;
    ++pickupRestores;
    auto& actor = g_registry.get<Actor>(item); actor.ground = true; actor.map = map; actor.x = pos.x; actor.y = pos.y;
    g_registry.get<ecs::ItemLocation>(item).window = GROUND; return true;
}
void LogManager::ItemLogEntity(entt::entity owner, entt::entity item, const char*, const char*) {
    Check(pickupTest && ecs::PlayerRuntime::IsPC(owner) && ItemSystem::IsValidItem(item), "pickup logged stale entity");
}

namespace {
void NativeInventory() {
    Reset(); Fixture f;
    const auto owner = g_registry.create();
    auto& main = g_registry.emplace<ecs::MainInventoryRuntimeComponent>(owner);
    main.items[0] = f.item;
    main.items[INVENTORY_MAX_NUM] = f.item;
    Check(ItemSystem::GetInventoryItem(owner, 0) == f.item &&
        ItemSystem::GetItem(owner, TItemPos(EQUIPMENT, 0)) == f.item, "native inventory required CItem");
    Check(ItemSystem::GetItem(owner, TItemPos(EQUIPMENT, static_cast<uint16_t>(65536 - INVENTORY_MAX_NUM))) == entt::null,
        "equipment offset wrapped into inventory");
    Check(ItemSystem::GetInventoryItem(owner, INVENTORY_AND_EQUIP_SLOT_MAX) == entt::null,
        "out of bounds inventory access accepted");
    const auto retiredCell = uint16_t(INVENTORY_MAX_NUM + WEAR_MAX_NUM);
    main.items[retiredCell] = f.item;
    Check(ItemSystem::GetInventoryItem(owner, retiredCell) == entt::null &&
        ItemSystem::GetItem(owner, TItemPos(INVENTORY, retiredCell)) == entt::null &&
        ItemSystem::GetItem(owner, TItemPos(EQUIPMENT, WEAR_MAX_NUM)) == entt::null &&
        ItemSystem::GetItem(owner, TItemPos(RETIRED_WINDOW_6, 0)) == entt::null,
        "retired storage remained readable through an alias");
    main.items[retiredCell] = entt::null;
#ifdef ENABLE_EXTRA_INVENTORY
    g_registry.emplace<ecs::ExtraInventoryRuntimeComponent>(owner).items[0] = f.item;
    Check(ItemSystem::GetExtraInventoryItem(owner, 0) == f.item, "native extra item lookup failed");
#endif
#ifdef ENABLE_SWITCHBOT
    g_registry.emplace<ecs::SwitchbotRuntimeComponent>(owner).items[0] = f.item;
#endif
    g_registry.destroy(f.item);
    for (uint8_t window : {uint8_t(INVENTORY), uint8_t(EQUIPMENT)
#ifdef ENABLE_EXTRA_INVENTORY
        , uint8_t(EXTRA_INVENTORY)
#endif
#ifdef ENABLE_SWITCHBOT
        , uint8_t(SWITCHBOT)
#endif
    })
        Check(ItemSystem::GetItem(owner, TItemPos(window, 0)) == entt::null, "lookup leaked a stale item generation");
    const auto replacement = g_registry.create();
    g_registry.emplace<ecs::ItemIdentity>(replacement);
    Check(ItemSystem::GetInventoryItem(owner, 0) == entt::null, "recycled entity replaced a stale inventory entry");
}

void NativeIdentityRegistry() {
    Reset();
    CItemRegistry registry;
    const auto make = [](uint32_t id, uint32_t vid) {
        const auto item = g_registry.create();
        auto& identity = g_registry.emplace<ecs::ItemIdentity>(item);
        identity.id = id; identity.vid = vid;
        return item;
    };
    const auto a = make(10, 20), b = make(30, 40);
    Check(registry.Register(10, 20, a) && registry.Register(30, b), "native identities not registered");
    Check(registry.Find(10) == a && registry.FindByVID(20) == a && registry.FindByVID(40) == b,
        "ID and VID do not resolve the same entity");
    Check(registry.Register(10, 20, a), "repeated registration is not idempotent");
    Check(!registry.Register(99, 20, a) && !registry.Register(10, 99, a), "mismatched identity accepted");

    auto& bIdentity = g_registry.get<ecs::ItemIdentity>(b);
    bIdentity.id = 10;
    Check(!registry.Register(10, 40, b), "a duplicate ID stole a live item");
    bIdentity.id = 30; bIdentity.vid = 20;
    Check(!registry.Register(30, 20, b), "a duplicate VID stole a live item");
    bIdentity.vid = 40;
    Check(registry.Find(10) == a && registry.FindByVID(20) == a &&
        registry.Find(30) == b && registry.FindByVID(40) == b, "failed registration changed existing bindings");

    bIdentity.id = 50; bIdentity.vid = 60;
    Check(registry.Register(50, 60, b) && registry.Find(30) == entt::null &&
        registry.FindByVID(40) == entt::null && registry.Find(50) == b && registry.FindByVID(60) == b,
        "reindexing left stale aliases");
    registry.Unregister(50, a);
    Check(registry.Find(50) == b, "foreign generation removed a binding");
    registry.Unregister(50, b);
    Check(registry.Find(50) == entt::null && registry.FindByVID(60) == entt::null,
        "conditional unregister did not remove both indexes");

    g_registry.destroy(a);
    Check(registry.Find(10) == entt::null && registry.FindByVID(20) == entt::null, "stale entity escaped lookup");
    const auto reused = make(10, 20);
    Check(entt::to_entity(a) == entt::to_entity(reused) && a != reused, "entity slot was not recycled");
    Check(registry.Register(10, 20, reused), "stale binding prevented generation reuse");
    registry.Unregister(10, a); registry.Unregister(a);
    Check(registry.Find(10) == reused && registry.FindByVID(20) == reused,
        "old generation cleanup removed its successor");
    registry.Unregister(10);
    Check(registry.Find(10) == entt::null && registry.FindByVID(20) == entt::null, "ID removal left a VID alias");

    const auto gold = make(0, 70), idOnly = make(80, 0);
    Check(registry.Register(0, 70, gold) && registry.Register(80, idOnly), "zero-ID gold or ID-only item rejected");
    Check(registry.Find(0) == entt::null && registry.FindByVID(0) == entt::null &&
        registry.FindByVID(70) == gold && registry.Find(80) == idOnly, "zero sentinel became an identity");
    const auto noKeys = make(0, 0), nonItem = g_registry.create();
    Check(!registry.Register(0, 0, noKeys) && !registry.Register(90, 91, nonItem) &&
        !registry.Register(90, 91, entt::null) && !registry.Register(10, 20, a), "invalid registration accepted");

    g_registry.remove<ecs::ItemIdentity>(gold);
    Check(registry.FindByVID(70) == entt::null, "an entity without item identity remained visible");
    const auto replacement = make(0, 70);
    Check(registry.Register(0, 70, replacement), "removed identity blocked a valid item");
    registry.Unregister(gold);
    Check(registry.FindByVID(70) == replacement, "removed item's cleanup deleted a new binding");
    registry.Unregister(replacement); registry.Unregister(idOnly);
    Check(registry.FindByVID(70) == entt::null && registry.Find(80) == entt::null, "entity-only cleanup failed");

    // ItemSystem's public lookups execute the same production registry.
    auto& shared = CItemRegistry::Instance();
    Check(shared.Register(10, 20, reused), "shared registry rejected a live entity");
    Check(ItemSystem::FindItemByID(10) == reused && ItemSystem::FindItemByVID(20) == reused,
        "public item lookup did not use native identity");
    shared.Unregister(reused);
}

void LocalizedNames() {
#ifdef ENABLE_MULTI_NAMES
    Reset(); Fixture f;
    strcpy_s(f.proto.szName, "internal-name");
    strcpy_s(f.proto.szLocaleName[1], "fallback-name");
    strcpy_s(f.proto.szLocaleName[2], "localized-name");
    Check(std::string_view(ItemSystem::GetItemName(f.item, 2)) == "localized-name", "explicit item locale was lost");
    Check(std::string_view(ItemSystem::GetItemName(f.item, 0)) == "fallback-name" &&
        std::string_view(ItemSystem::GetItemName(f.item, UINT8_MAX)) == "fallback-name", "default/invalid item locale changed");
    const auto owner = g_registry.create();
    g_registry.emplace<ecs::ItemOwner>(f.item).owner = owner;
    Check(std::string_view(ItemSystem::GetItemName(f.item, 0)) == "fallback-name", "owner without descriptor lost fallback");
    f.proto.szLocaleName[1][0] = 0;
    Check(std::string_view(ItemSystem::GetItemName(f.item, 0)) == "internal-name", "empty locale did not fall back to proto name");
    g_registry.destroy(owner);
    Check(std::string_view(ItemSystem::GetItemName(f.item, 0)) == "internal-name", "retired owner broke native name lookup");
    g_registry.destroy(f.item);
    Check(std::string_view(ItemSystem::GetItemName(f.item, 0)).empty() &&
        std::string_view(ItemSystem::GetItemName(entt::null, 0)).empty(), "stale localized item name escaped");
#endif
}
void LevelChecks() {
    Reset(); Fixture f;
    Check(ItemSystem::CheckItemUseLevel(f.item, 0), "unrestricted item rejected");
    f.proto.aLimits[0] = {LIMIT_LEVEL, 300};
    Check(!ItemSystem::CheckItemUseLevel(f.item, 299) && ItemSystem::CheckItemUseLevel(f.item, 300),
        "level limit was narrowed to the cached byte");
    f.proto.aLimits[1] = {LIMIT_LEVEL, 500};
    Check(ItemSystem::CheckItemUseLevel(f.item, 300), "first matching level-limit semantics changed");
    const auto ordinary = g_registry.create();
    g_registry.emplace<ecs::ItemProtoRef>(ordinary).proto = &f.proto;
    Check(!ItemSystem::CheckItemUseLevel(ordinary, 999), "non-item prototype accepted");
    g_registry.remove<ecs::ItemProtoRef>(f.item);
    Check(!ItemSystem::CheckItemUseLevel(f.item, 999),
        "missing prototype accepted");
    g_registry.destroy(f.item); const auto replacement = g_registry.create();
    Check(!ItemSystem::CheckItemUseLevel(f.item, 999) && !ItemSystem::CheckItemUseLevel(entt::null, 999) &&
        g_registry.valid(replacement), "stale handle accepted");
}
std::function<void(entt::registry&, entt::entity)> onFactoryIdentity;
std::function<void(const ecs::EvItemDestroyed&)> onRetire;
void FactoryIdentity(entt::registry& registry, entt::entity item) {
    if (onFactoryIdentity) { auto callback = onFactoryIdentity; callback(registry, item); }
}
void Retired(const ecs::EvItemDestroyed& event) { if (onRetire) { auto callback = onRetire; callback(event); } }

void NativeFactory() {
    Reset();
    TItemTable proto {};
    proto.dwVnum = 100; proto.bType = ITEM_WEAPON; proto.bSize = 2;
    proto.dwFlags = ITEM_FLAG_STACKABLE; proto.dwWearFlags = WEARABLE_WEAPON;
    proto.dwAntiFlags = ITEM_ANTIFLAG_GIVE; proto.dwImmuneFlag = 7;
    proto.aLimits[0] = {LIMIT_LEVEL, 123}; proto.cLimitTimerBasedOnWearIndex = -1;
    proto.dwRefinedVnum = 101; proto.alValues[3] = -1; proto.alValues[4] = 20;
    proto.alValues[1] = 8; proto.alValues[2] = 13; proto.alValues[5] = 99;
    std::strcpy(proto.szName, "test+12");
    const auto item = EntityFactory::CreateItemEntity(g_registry, &proto, 102, 901, 902, 777);
    Check(ItemSystem::IsValidItem(item) && ecs::ItemInvariants::HasMinimumItemComponents(g_registry, item),
        "native factory returned an incomplete item");
    Check(ItemSystem::GetItemID(item) == 901 && ItemSystem::GetItemVID(item) == 902 &&
        ItemSystem::GetItemOriginalVnum(item) == 102 && ItemSystem::GetItemVnum(item) == 777,
        "native factory lost ranged/masked identity");
    const auto snapshot = g_registry.get<ecs::ItemProtoRef>(item);
    Check(snapshot.proto == &proto && snapshot.base_vnum == 100 && snapshot.size == 2 &&
        snapshot.level_limit == 123 && snapshot.refine_level == 12 && snapshot.refined_vnum == 101 &&
        snapshot.weapon_min == 0 && snapshot.weapon_max == 20 && snapshot.wear_flags == WEARABLE_WEAPON &&
        snapshot.magic_min == 8 && snapshot.magic_max == 13 && snapshot.defense == 8 &&
        snapshot.anti_flags == ITEM_ANTIFLAG_GIVE && snapshot.immune_flags == 7,
        "native prototype snapshot differs from item data");
    Check(ItemSystem::GetItemCount(item) == 0 && ItemSystem::GetItemOwner(item) == entt::null &&
        ItemSystem::GetItemWindow(item) == RESERVED_WINDOW && g_registry.get<ecs::ItemLockedAttribute>(item).index == -1 &&
        !ItemSystem::IsItemEquipped(item) && !ItemSystem::IsItemLocked(item) && !ItemSystem::GetItemSkipSave(item),
        "native creation did not initialize item state");
    Check(CItemRegistry::Instance().Find(901) == item && CItemRegistry::Instance().FindByVID(902) == item,
        "factory failed to publish both indexes");
    Check(EntityFactory::CreateItemEntity(g_registry, &proto, 100, 901, 903) == entt::null &&
        EntityFactory::CreateItemEntity(g_registry, &proto, 100, 904, 902) == entt::null &&
        CItemRegistry::Instance().Find(901) == item, "duplicate creation overwrote a live item");
    EntityFactory::DestroyItemEntity(g_registry, item);
    Check(!g_registry.valid(item) && CItemRegistry::Instance().Find(901) == entt::null &&
        CItemRegistry::Instance().FindByVID(902) == entt::null, "factory retirement left identity indexed");

    Check(EntityFactory::CreateItemEntity(g_registry, nullptr, 100, 1, 2) == entt::null &&
        EntityFactory::CreateItemEntity(g_registry, &proto, 0, 1, 2) == entt::null &&
        EntityFactory::CreateItemEntity(g_registry, &proto, 100, 0, 2) == entt::null &&
        EntityFactory::CreateItemEntity(g_registry, &proto, 100, 1, 0) == entt::null,
        "invalid native factory input was accepted");
    for (const auto [type, subtype] : std::array<std::pair<uint8_t, uint8_t>, 9>{{
            {29, 0}, {30, 0}, {31, 0}, {ITEM_MATERIAL, 5}, {ITEM_MATERIAL, 6}, {ITEM_MATERIAL, 7},
            {ITEM_USE, 27}, {ITEM_USE, 28}, {ITEM_USE, 42}}}) {
        proto.bType = type; proto.bSubType = subtype;
        Check(EntityFactory::CreateItemEntity(g_registry, &proto, 100, 903, 904) == entt::null &&
            CItemRegistry::Instance().Find(903) == entt::null && CItemRegistry::Instance().FindByVID(904) == entt::null,
            "retired prototype created or registered a native item");
    }
    proto.bSubType = 0;
    proto.bType = ITEM_ELK;
    const auto gold = EntityFactory::CreateItemEntity(g_registry, &proto, 1, 0, 905);
    Check(ItemSystem::IsValidItem(gold) && CItemRegistry::Instance().FindByVID(905) == gold &&
        CItemRegistry::Instance().Find(0) == entt::null, "ID-less gold lost its native VID");
    EntityFactory::DestroyItemEntity(g_registry, gold);
}

void NativePrototypeRefresh() {
    Reset();
    TItemTable proto {}; proto.dwVnum = 100; proto.bSize = 1; proto.bType = ITEM_MATERIAL;
    auto item = EntityFactory::CreateItemEntity(g_registry, &proto, 100, 501, 502);
    Check(ItemSystem::GetItemExtraCategory(item) == 1 && ItemSystem::IsExtraItem(item), "material category initialization changed");
    g_registry.get<ecs::ItemIdentity>(item).vnum = 777;
    Check(ItemSystem::IsExtraItem(item), "masked material classification required another prototype");
    g_registry.get<ecs::ItemIdentity>(item).vnum = 30002;
    Check(!ItemSystem::IsExtraItem(item), "extra-inventory exclusion was lost");
    g_registry.get<ecs::ItemIdentity>(item).vnum = 100;
    auto& flags = g_registry.get<ecs::ItemFlags>(item);
    flags.skipSave = flags.isLocked = flags.exchanging = true;
    g_registry.get<ecs::ItemCount>(item).count = 73;
    g_registry.get<ecs::ItemSockets>(item).sockets[0] = 81;
    g_registry.get<ecs::ItemAttributes>(item).attrs[0] = {APPLY_MAX_HP, 900};
    TItemTable updated = proto;
    updated.bType = ITEM_USE; updated.bSubType = USE_POTION; updated.bSize = 2;
    updated.dwFlags = ITEM_FLAG_STACKABLE; updated.aLimits[0] = {LIMIT_LEVEL, 300};
    std::strcpy(updated.szName, "potion+300");
    Check(ItemSystem::RefreshItemPrototype(item, &updated) && ItemSystem::GetItemProto(item) == &updated &&
        ItemSystem::GetItemType(item) == ITEM_USE && ItemSystem::GetItemSubType(item) == USE_POTION &&
        ItemSystem::GetItemExtraCategory(item) == 5 && ItemSystem::GetItemSize(item) == 2 &&
        ItemSystem::GetItemRefineLevel(item) == 255 && ItemSystem::GetItemLevelLimit(item) == 255 &&
        ItemSystem::GetItemFlags(item) == ITEM_FLAG_STACKABLE,
        "reload left stale native proto fields");
    Check(ItemSystem::GetItemCount(item) == 73 && ItemSystem::GetItemSocket(item, 0) == 81 &&
        g_registry.get<ecs::ItemLockedAttribute>(item).index == -1 && ItemSystem::IsItemLocked(item) &&
        ItemSystem::IsItemExchanging(item) && ItemSystem::GetItemSkipSave(item) &&
        g_registry.get<ecs::ItemAttributes>(item).attrs[0].sValue == 900,
        "reload overwrote runtime item state");
    auto retired = updated; retired.bType = ITEM_RETIRED_29;
    Check(ItemSystem::RefreshItemPrototype(item, &retired) && !ItemSystem::GetItemProto(item) &&
        ItemSystem::GetItemType(item) == 0 && ItemSystem::GetItemCount(item) == 73,
        "retired prototype reload reactivated an item");
    Check(ItemSystem::RefreshItemPrototype(item, nullptr) && !ItemSystem::GetItemProto(item) &&
        ItemSystem::GetItemSize(item) == 0 && ItemSystem::GetItemType(item) == 0 &&
        ItemSystem::GetItemFlags(item) == 0 && ItemSystem::GetItemCount(item) == 73,
        "removed prototype left a dangling reference or lost the stack");
    EntityFactory::DestroyItemEntity(g_registry, item);
    Check(!ItemSystem::RefreshItemPrototype(item, &updated), "stale prototype reload succeeded");
}

void FactoryCallbacks() {
    for (int stage = 0; stage < 2; ++stage) for (int action = 0; action < 4; ++action) {
        Reset();
        TItemTable proto {}; proto.dwVnum = 100; proto.bType = ITEM_WEAPON; proto.bSize = 1;
        entt::entity watched = entt::null, replacement = entt::null;
        const auto mutate = [&](entt::entity item) {
            watched = item;
            if (action == 0) {
                g_registry.destroy(item); replacement = g_registry.create();
                Check(replacement != item && entt::to_entity(replacement) == entt::to_entity(item),
                    "factory test did not recycle the generation");
            } else if (action == 1) throw std::runtime_error("component construction failure");
            else if (action == 2) g_registry.remove<ecs::ItemCount>(item);
            else if (stage == 0) g_registry.remove<ecs::ItemEvents>(item);
            else g_registry.remove<ecs::ItemIdentity>(item);
        };
        if (stage == 0) onComponent = mutate;
        else onFactoryIdentity = [&](entt::registry&, entt::entity item) { mutate(item); };
        entt::entity result = entt::null; bool threw = false;
        try { result = EntityFactory::CreateItemEntity(g_registry, &proto, 100, 601, 602); }
        catch (const std::runtime_error&) { threw = true; }
        onComponent = {}; onFactoryIdentity = {};
        Check(watched != entt::null && result == entt::null && threw == (action == 1) &&
            !g_registry.valid(watched) && CItemRegistry::Instance().Find(601) == entt::null,
            "failed construction left a partially indexed item");
        if (action == 0) Check(g_registry.valid(replacement) && !g_registry.any_of<ecs::ItemIdentity>(replacement),
            "factory wrote/destroyed a recycled generation");
    }
    Reset();
    TItemTable proto {}; proto.dwVnum = 100; proto.bType = ITEM_WEAPON; proto.bSize = 1;
    entt::entity collision = entt::null;
    onFactoryIdentity = [&](entt::registry& registry, entt::entity item) {
        // A final construction callback claims the same persistent ID first.
        auto callback = std::move(onFactoryIdentity); onFactoryIdentity = {};
        collision = registry.create();
        auto identity = registry.get<ecs::ItemIdentity>(item); identity.vid = 704;
        registry.emplace<ecs::ItemIdentity>(collision, identity);
        Check(CItemRegistry::Instance().Register(identity.id, identity.vid, collision), "collision fixture registration failed");
    };
    Check(EntityFactory::CreateItemEntity(g_registry, &proto, 100, 701, 702) == entt::null &&
        CItemRegistry::Instance().Find(701) == collision && g_registry.valid(collision),
        "construction rollback removed another entity's index");
    EntityFactory::DestroyItemEntity(g_registry, collision);

    const auto item = EntityFactory::CreateItemEntity(g_registry, &proto, 100, 801, 802);
    int calls = 0;
    onRetire = [&](const ecs::EvItemDestroyed& event) {
        ++calls; EntityFactory::DestroyItemEntity(g_registry, event.itemEntity);
        Check(g_registry.valid(item), "recursive factory call bypassed retirement guard");
    };
    EntityFactory::DestroyItemEntity(g_registry, item); onRetire = {};
    Check(calls == 1 && !g_registry.valid(item), "native factory destruction reentered");

    const auto moved = EntityFactory::CreateItemEntity(g_registry, &proto, 100, 803, 804);
    const auto owner = g_registry.create();
    onRetire = [&](const ecs::EvItemDestroyed&) { g_registry.get<ecs::ItemOwner>(moved).owner = owner; };
    EntityFactory::DestroyItemEntity(g_registry, moved); onRetire = {};
    Check(g_registry.valid(moved) && CItemRegistry::Instance().Find(803) == moved,
        "event callback's transferred item was deleted");
    g_registry.get<ecs::ItemOwner>(moved).owner = entt::null;
    EntityFactory::DestroyItemEntity(g_registry, moved);

    const auto timed = EntityFactory::CreateItemEntity(g_registry, &proto, 100, 805, 806);
    auto& events = g_registry.get<ecs::ItemEvents>(timed);
    std::array<LPEVENT, 3> timers;
    for (auto& timer : timers) timer = LPEVENT(new EVENT);
    events.destroy = timers[0]; events.expire = timers[1]; events.ownership = timers[2];
    entt::entity replacement = entt::null;
    onCancel = [&] {
        onCancel = {};
        g_registry.destroy(timed); replacement = g_registry.create();
        Check(replacement != timed && entt::to_entity(replacement) == entt::to_entity(timed),
            "timer cancellation did not recycle the generation");
    };
    EntityFactory::DestroyItemEntity(g_registry, timed);
    Check(g_registry.valid(replacement) && !g_registry.any_of<ecs::ItemEvents>(replacement),
        "timer cleanup touched a replacement entity");
    Check(std::all_of(timers.begin(), timers.end(), [](const auto& timer) { return timer->is_force_to_end; }),
        "native retirement left an active timer");

    const auto retry = EntityFactory::CreateItemEntity(g_registry, &proto, 100, 807, 808);
    onRetire = [](const ecs::EvItemDestroyed&) { throw std::runtime_error("retirement callback"); };
    bool threw = false;
    try { EntityFactory::DestroyItemEntity(g_registry, retry); } catch (const std::runtime_error&) { threw = true; }
    onRetire = {};
    Check(threw && g_registry.valid(retry) && CItemRegistry::Instance().Find(807) == retry,
        "throwing retirement lost a live item's identity");
    EntityFactory::DestroyItemEntity(g_registry, retry);
    Check(!g_registry.valid(retry), "retirement guard prevented retry after exception");
}


entt::entity PickupActor() {
    const auto owner = g_registry.create();
    g_registry.emplace<Actor>(owner).pc = true;
    g_registry.emplace<ecs::PlayerID>(owner).pid = entt::to_integral(owner) + 1;
    g_registry.emplace<ecs::MainInventoryRuntimeComponent>(owner);
    g_registry.emplace<ecs::ExtraInventoryRuntimeComponent>(owner);
    return owner;
}
struct PickupFixture {
        TItemTable proto {};
        entt::entity owner, item;
        PickupFixture(uint8_t type = ITEM_WEAPON) {
            Reset(); pickupTest = true;
            owner = PickupActor();
            proto.dwVnum = 500; proto.bSize = 1; proto.bType = type;
            item = EntityFactory::CreateItemEntity(g_registry, &proto, 500, type == ITEM_ELK ? 0 : 1501, 1502);
            g_registry.emplace<Actor>(item).ground = true;
            g_registry.get<ecs::ItemLocation>(item).window = GROUND;
            g_registry.get<ecs::ItemCount>(item).count = 7;
        }
        bool Pickup() { return ItemSystem::PickupItem(owner, 1502); }
};
void NativePickups() {
    // Concrete ECS items are legacy fixtures. No player may pick one up after
    // native retirement, even if a stale spatial entity remains in memory.
    for (uint8_t type : {ITEM_WEAPON, ITEM_MATERIAL, ITEM_ELK}) {
        PickupFixture f(type);
        Check(!f.Pickup() && pickupDetaches==0 && pickupPlacements==0 &&
            pickupGold==0 && pickupSaves==0 && ItemSystem::IsValidItem(f.item) &&
            ItemSystem::GetItemCount(f.item)==7 && ItemSystem::GetItemWindow(f.item)==GROUND,
            "retired pickup reached a legacy mutation service");
    }
}

auriga::player_session::Key AdmissionKey(entt::entity owner) {
    auriga::player_session::Token token{}; token.fill(11);
    return {token, token, token, 1, 900, g_registry.get<ecs::PlayerID>(owner).pid};
}
entt::entity PartyRecipient(PickupFixture& fixture) {
    const auto recipient = PickupActor(), party = g_registry.create();
    auto& state = g_registry.emplace<ecs::PartyState>(party);
    for (auto member : {fixture.owner, recipient}) {
        state.members[g_registry.get<ecs::PlayerID>(member).pid].member = member;
        g_registry.get<Actor>(member).party = party;
    }
    pickupClaim = recipient;
    return recipient;
}
void PickupAdmission() {
    namespace admission = ecs::resource_admission;
    for (uint8_t type : {ITEM_WEAPON, ITEM_ELK}) {
        for (bool party : {false,true}) {
            for (bool paused : {false,true}) {
                PickupFixture fixture(type);
                const auto recipient=party?PartyRecipient(fixture):fixture.owner;
                const auto check=[&] {
                    Check(!fixture.Pickup() && pickupDetaches==0 && pickupPlacements==0 &&
                        pickupGold==0 && pickupSaves==0 && ItemSystem::IsValidItem(fixture.item),
                        "party/admission state reenabled retired pickup");
                };
                if (paused) { const auto key=AdmissionKey(recipient);auto pause=admission::Prepare(recipient,key,key.request);Check(pause.has_value(),"pickup pause fixture");check(); }
                else check();
            }
        }
    }
}


}
int main() {
    ITEM_MANAGER itemManager;
    quest::CQuestManager quests;
    LogManager logs;
    g_dispatcher.sink<ecs::EvItemExpired>().connect<&Published>();
    g_registry.on_construct<ecs::ItemEvents>().connect<&Constructed>();
    g_registry.on_construct<ecs::ItemIdentity>().connect<&FactoryIdentity>();
    g_dispatcher.sink<ecs::EvItemDestroyed>().connect<&Retired>();
    try { NativeFactory(); NativePrototypeRefresh(); FactoryCallbacks(); NativePickups(); PickupAdmission(); NativeInventory(); NativeIdentityRegistry(); LocalizedNames(); LevelChecks();     Reset(); }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    std::cout << "Item runtime: " << checks << " checks passed\n";
}
