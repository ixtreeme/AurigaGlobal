#include "../../SRC/Server/GameServer/stdafx.h"
#include "../../SRC/Server/GameServer/item.h"
#include "../../SRC/Server/GameServer/item_manager.h"
#include "../../SRC/Server/GameServer/affect.h"
#include "../../SRC/Server/GameServer/char.h"
#include "../../SRC/Server/GameServer/desc.h"
#include "../../SRC/Server/GameServer/log.h"
#include "../../SRC/Server/GameServer/party.h"
#include "../../SRC/Server/GameServer/constants.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/EntityFactory.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SessionSystem.hpp"
#include "../../SRC/Server/GameServer/battle_pass.h"
#include "../../SRC/Server/GameServer/sectree.h"
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
#include "../../SRC/Server/GameServer/ecs/ItemRegistry.hpp"
#include <Core/Logging.hpp>
#include "../../SRC/Server/GameServer/ecs/components/inventory_components.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/CombatSystem.hpp"
#include "../../SRC/Server/GameServer/fishing.h"
#include "../../SRC/Server/GameServer/refine.h"
#include "../../SRC/Server/GameServer/unique_item.h"
#include "../../SRC/Server/GameServer/questmanager.h"
#include "../../SRC/Server/GameServer/RuneDungeon.h"
#include "../../SRC/Server/GameServer/Halloween2022Dungeon.h"
#include "../../SRC/Server/GameServer/VikingDungeon.h"
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
struct Actor { bool pc = false; uint32_t race = 0; int32_t x = 0, y = 0, map = 1; bool dead = false; bool observer = false, ground = false; };
bool pickupTest = false, pickupAllowed = true, rejectPickupPlace = false, rejectPickupDestroy = false;
int pickupCell = 0, pickupDetaches = 0, pickupPlacements = 0, pickupRestores = 0, pickupSaves = 0;
int lastPickupWindow = 0;
int64_t pickupGold = 0;
std::function<void(entt::entity)> onPickupDetach, onPickupPlace, onPickupDestroy;
bool npcTest = false;
int revivals = 0, feeds = 0;
std::string receivedItemName;
std::function<bool(entt::entity)> onUse;
bool failAllocation = false;
bool wearStopTest = false;
int flushed = 0;
std::function<void(entt::entity)> onSave, onUpdate;
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
    onCreate = onPublish = onComponent = {}; onCancel = {};
    onUse = {}; npcTest = false; revivals = feeds = 0; receivedItemName.clear();
    onSave = onUpdate = {}; wearStopTest = false; flushed = 0;
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
void ecs::ChatSystem::SendNew(entt::entity, uint8_t, uint32_t message, const char* format, ...) {
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
bool SetQuestNPCID(entt::entity, uint32_t) { Unexpected(); }
LPSECTREE GetSectree(entt::entity e) {
    if (!pickupTest) Unexpected();
    static SECTREE tree;
    const auto* actor = g_registry.try_get<Actor>(e);
    return actor && actor->ground ? &tree : nullptr;
}
bool IsValid(entt::entity e) { return e != entt::null && g_registry.valid(e) && g_registry.all_of<Actor>(e); }
}
CParty* ecs::SocialSystem::GetParty(entt::entity) { if (pickupTest) return nullptr; Unexpected(); }
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
bool UseItemEx(entt::entity, entt::entity item, TItemPos) { if (onUse) return onUse(item); Unexpected(); }
bool CanConsumeOwnedItem(entt::entity owner, entt::entity item, uint32_t amount, ItemCostStorage) {
    // Full storage/payment policy is covered by ItemAttributeTests.
    return ecs::PlayerRuntime::IsPC(owner) && IsValidItem(item) && GetItemOwner(item) == owner &&
        GetItemCount(item) >= amount && !IsItemEquipped(item) && !IsItemLocked(item) && !IsItemExchanging(item);
}
bool RefineInformation(entt::entity, uint8_t, uint8_t, int) { Unexpected(); }
}
bool CombatSystem::IsDead(entt::entity e) { return g_registry.get<Actor>(e).dead; }
bool MountSystem::ReviveHorse(entt::entity) { ++revivals; return true; }
void MountSystem::FeedHorse(entt::entity) { ++feeds; }
void NetworkSyncSystem::BroadcastEffect(entt::registry&, entt::entity, uint8_t) {}
void InventorySystem::SetRefineNPC(entt::entity, entt::entity) { Unexpected(); }
bool fishing::GrillFishEcs(entt::entity, entt::entity) { Unexpected(); }
bool quest::CQuestManager::TakeItem(unsigned int, unsigned int, entt::entity) { Unexpected(); }
CRuneDungeon& CRuneDungeon::instance() { static CRuneDungeon dungeon; return dungeon; }
CHalloween2022Dungeon& CHalloween2022Dungeon::instance() { static CHalloween2022Dungeon dungeon; return dungeon; }
CVikingDungeon& CVikingDungeon::instance() { static CVikingDungeon dungeon; return dungeon; }
bool CRuneDungeon::OnNpcTakeItem(entt::entity, entt::entity, entt::entity) { return false; }
bool CHalloween2022Dungeon::OnNpcTakeItem(entt::entity, entt::entity, entt::entity) { return false; }
bool CVikingDungeon::OnNpcTakeItem(entt::entity, entt::entity, entt::entity) { return false; }
bool CHARACTER::GiveItemFromSpecialItemGroup(uint32_t, std::vector<uint32_t>&,
    std::vector<uint32_t>&, std::vector<entt::entity>&, int&) { Unexpected(); }
void CParty::ChatPacketToAllMemberNew(uint8_t, uint32_t, const char*, ...) { Unexpected(); }
void MountSystem::ForceClearRidingState(entt::entity) { Unexpected(); }
void ecs::ItemNetworkSystem::SendItemUpdate(entt::registry&, entt::entity item) {
    Check(wearStopTest && ItemSystem::IsValidItem(item), "wear stop published a stale item");
    if (onUpdate) onUpdate(item);
}
void ecs::PointSystem::Change(entt::entity, uint8_t, int64_t, bool, bool, bool) { Unexpected(); }
void ITEM_MANAGER::DelayedSave(entt::entity item) {
    Check(wearStopTest && ItemSystem::IsValidItem(item), "wear stop saved a stale item");
    if (onSave) onSave(item);
}
void ITEM_MANAGER::FlushDelayedSave(entt::entity item) {
    Check(wearStopTest && ItemSystem::IsValidItem(item), "wear stop flushed a stale item");
    ++flushed;
}
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
uint8_t ecs::PlayerRuntime::GetBattlePassId(entt::entity) { return 0; }
uint32_t ecs::PlayerRuntime::GetMissionProgress(entt::entity, uint32_t, uint32_t) { Unexpected(); }
bool ecs::PlayerRuntime::UpdateMissionProgress(entt::entity, uint32_t, uint32_t, uint32_t, uint32_t, bool) { Unexpected(); }
int64_t ecs::PlayerRuntime::GetRankPoints(entt::entity, int) { return 0; }
bool ecs::PlayerRuntime::SetRankPoints(entt::entity, int, int64_t) { return true; }
bool CBattlePass::BattlePassMissionGetInfo(uint8_t, uint8_t, uint32_t*, uint32_t*) { Unexpected(); }
quest::PC* quest::CQuestManager::GetPCForce(unsigned int) { return nullptr; }
void ecs::SessionSystem::Save(entt::entity owner) {
    Check(pickupTest && ecs::PlayerRuntime::IsPC(owner), "pickup saved a stale character"); ++pickupSaves;
}
void ItemSystem::GiveGold(entt::entity owner, int64_t amount) {
    Check(pickupTest && ecs::PlayerRuntime::IsPC(owner), "pickup credited stale character"); pickupGold += amount;
}
bool ItemSystem::IsOwnership(entt::entity item, entt::entity owner) {
    return pickupTest && IsValidItem(item) && ecs::PlayerRuntime::IsPC(owner) && pickupAllowed;
}
ItemSystem::StackMergeResult ItemSystem::MergeItemStacksEcs(entt::entity, entt::entity, entt::entity,
    uint32_t, StackSource) { Unexpected(); } // Full stack policy tested with real ItemAttributeSystem.
int InventorySystem::GetEmptyInventory(entt::entity, uint8_t) { return pickupCell; }
int ItemSystem::GetEmptyDragonSoulInventory(entt::entity, entt::entity) { return pickupCell; }
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
void NativeInventoryAndUse() {
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
    g_registry.emplace<ecs::DragonSoulInventoryComponent>(owner).items[0] = f.item;
#ifdef ENABLE_EXTRA_INVENTORY
    g_registry.emplace<ecs::ExtraInventoryRuntimeComponent>(owner).items[0] = f.item;
    Check(ItemSystem::GetExtraInventoryItem(owner, 0) == f.item, "native extra item lookup failed");
#endif
#ifdef ENABLE_SWITCHBOT
    g_registry.emplace<ecs::SwitchbotRuntimeComponent>(owner).items[0] = f.item;
#endif
    Check(ItemSystem::GetItem(owner, TItemPos(DRAGON_SOUL_INVENTORY, 0)) == f.item,
        "native dragon soul item lookup failed");
    onUse = [](entt::entity item) { g_registry.get<ecs::ItemIdentity>(item).vnum = 4321; return true; };
    Check(ItemSystem::UseItemEcs(owner, f.item, TItemPos(INVENTORY, 1)) && ItemSystem::GetItemVnum(f.item) == 4321,
        "native use result was overwritten by legacy synchronization");
    onUse = [](entt::entity item) { g_registry.destroy(item); return true; };
    Check(ItemSystem::UseItemEcs(owner, f.item, TItemPos(INVENTORY, 1)), "consumed use lost its handled result");
    for (uint8_t window : {uint8_t(INVENTORY), uint8_t(EQUIPMENT), uint8_t(DRAGON_SOUL_INVENTORY)
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

void NativeNpcItems() {
    Reset(); Fixture f; npcTest = true;
    const auto owner = g_registry.create(), npc = g_registry.create();
    g_registry.emplace<Actor>(owner).pc = true;
    g_registry.emplace<Actor>(npc).race = BLACKSMITH_WEAPON_MOB;
    g_registry.emplace<ecs::ItemOwner>(f.item).owner = owner;
    g_registry.emplace<ecs::ItemCount>(f.item).count = 1;
    g_registry.get<ecs::ItemPrototypeMeta>(f.item).type = ITEM_WEAPON;
    f.proto.dwRefinedVnum = 1001;
    g_registry.get<ecs::ItemProtoRef>(f.item).refined_vnum = 1001;
    Check(ItemSystem::CanReceiveItemEcs(npc, owner, f.item), "entity-only blacksmith rejected a weapon");
    g_registry.get<Actor>(npc).map = 2;
    Check(!ItemSystem::CanReceiveItemEcs(npc, owner, f.item), "cross-map NPC accepted an item");
    g_registry.get<Actor>(npc).map = 1;
    g_registry.get<Actor>(npc).x = INT32_MAX; g_registry.get<Actor>(owner).x = INT32_MIN;
    Check(!ItemSystem::CanReceiveItemEcs(npc, owner, f.item), "overflowing NPC distance accepted");
    g_registry.get<Actor>(npc).x = g_registry.get<Actor>(owner).x = 0;
    g_registry.get<Actor>(npc).pc = true;
    Check(!ItemSystem::CanReceiveItemEcs(npc, owner, f.item), "PC used NPC receive path");
    g_registry.get<Actor>(npc).pc = false;
    g_registry.get<ecs::ItemOwner>(f.item).owner = npc;
    Check(!ItemSystem::CanReceiveItemEcs(npc, owner, f.item), "foreign item accepted");
    g_registry.get<ecs::ItemOwner>(f.item).owner = owner;
    g_registry.get<Actor>(npc).race = DEVILTOWER_BLACKSMITH_WEAPON_MOB;
    f.proto.aLimits[0] = {LIMIT_LEVEL, 90};
    Check(!ItemSystem::CanReceiveItemEcs(npc, owner, f.item), "tower level restriction lost");
    f.proto.aLimits[0].lValue = 89;
    Check(ItemSystem::CanReceiveItemEcs(npc, owner, f.item), "eligible tower item rejected");
    auto& horse = g_registry.get<Actor>(npc); horse.race = 20101;
    g_registry.get<ecs::ItemIdentity>(f.item).vnum = ITEM_REVIVE_HORSE_1;
    Check(!ItemSystem::CanReceiveItemEcs(npc, owner, f.item), "live horse accepted revival");
    horse.dead = true;
    std::strcpy(f.proto.szName, "Revival herb");
    std::strcpy(g_registry.get<ecs::ItemProtoRef>(f.item).name, "Revival herb");
    Check(ItemSystem::ReceiveItemEcs(npc, owner, f.item) && revivals == 1 && !g_registry.valid(f.item),
        "entity-only horse revival failed");
#ifdef TEXTS_IMPROVEMENT
    Check(receivedItemName == "Revival herb", "revival read item name after destroying it");
#endif
    Check(!ItemSystem::ReceiveItemEcs(npc, owner, f.item), "stale item was received twice");
    g_registry.destroy(npc);
    Check(!ItemSystem::ReceiveItemEcs(npc, owner, f.item), "stale receiver accepted an item");
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
    Check(!ItemSystem::CheckItemUseLevel(f.item, 999) && !ItemSystem::OnAfterCreatedItem(f.item),
        "missing prototype accepted");
    g_registry.destroy(f.item); const auto replacement = g_registry.create();
    Check(!ItemSystem::CheckItemUseLevel(f.item, 999) && !ItemSystem::CheckItemUseLevel(entt::null, 999) &&
        !ItemSystem::OnAfterCreatedItem(f.item) && g_registry.valid(replacement), "stale handle accepted");
}
void LoadedTimers() {
    Reset(); Fixture f;
    Check(ItemSystem::OnAfterCreatedItem(f.item) && queued.empty(), "ordinary item started a timer");
    f.FirstUse(false);
    Check(ItemSystem::OnAfterCreatedItem(f.item) && queued.empty(), "unused first-use item started expiring");
    f.FirstUse(true);
    Check(ItemSystem::OnAfterCreatedItem(f.item) && queued.size() == 1, "used item did not resume its timer");
    Check(ItemSystem::OnAfterCreatedItem(f.item) && queued.size() == 1, "loading twice duplicated the timer");
#ifdef ENABLE_SOUL_SYSTEM
    Reset(); Fixture soul;
    g_registry.get<ecs::ItemPrototypeMeta>(soul.item).type = ITEM_SOUL;
    soul.proto.aLimits[1].lValue = 10;
    g_registry.get<ecs::ItemSockets>(soul.item).sockets[2] = 100000;
    Check(ItemSystem::OnAfterCreatedItem(soul.item) && queued.empty(), "fully charged soul treated as failure");
    g_registry.get<ecs::ItemSockets>(soul.item).sockets[2] = 90000;
    Check(ItemSystem::OnAfterCreatedItem(soul.item) && queued.size() == 1, "soul charging did not resume");
    Check(ItemSystem::OnAfterCreatedItem(soul.item) && queued.size() == 1, "soul charging duplicated");
#endif
}
void WearTimers() {
    Reset(); Fixture f;
    Check(ItemSystem::StartTimerBasedOnWearExpireEventEcs(f.item) && queued.empty(), "unlimited item started wear timer");
    f.proto.cLimitTimerBasedOnWearIndex = 0;
    g_registry.get<ecs::ItemSockets>(f.item).sockets[0] = 100;
    Check(ItemSystem::StartTimerBasedOnWearExpireEventEcs(f.item) && queued.size() == 1 &&
        dynamic_cast<item_event_info*>(queued.front()->info), "wear timer lost its correct native event payload");
    Check(ItemSystem::StartTimerBasedOnWearExpireEventEcs(f.item) && queued.size() == 1, "wear timer start duplicated");
    event_cancel(&g_registry.get<ecs::ItemEvents>(f.item).timerBasedOnWearExpire);
    failAllocation = true;
    Check(!ItemSystem::StartTimerBasedOnWearExpireEventEcs(f.item), "failed wear timer allocation reported success");
    failAllocation = false;
    Check(ItemSystem::StartTimerBasedOnWearExpireEventEcs(f.item) && queued.size() == 2, "stopped wear timer did not restart");

    Reset(); Fixture retired; retired.proto.cLimitTimerBasedOnWearIndex = 0;
    onCreate = [](entt::entity item) { g_registry.destroy(item); };
    Check(!ItemSystem::StartTimerBasedOnWearExpireEventEcs(retired.item) && queued.front()->is_force_to_end,
        "wear allocation callback left an orphan event");

    Reset(); Fixture removed; removed.proto.cLimitTimerBasedOnWearIndex = 0;
    onComponent = [](entt::entity item) { g_registry.remove<ecs::ItemEvents>(item); };
    Check(!ItemSystem::StartTimerBasedOnWearExpireEventEcs(removed.item) && queued.empty(), "wear timer recreated retired components");

    Reset(); Fixture unpublished; unpublished.proto.cLimitTimerBasedOnWearIndex = 0;
    onPublish = [](entt::entity item) { g_registry.remove<ecs::ItemEvents>(item); };
    Check(!ItemSystem::StartTimerBasedOnWearExpireEventEcs(unpublished.item) && queued.front()->is_force_to_end,
        "publication-time component removal left an orphan wear timer");
}
void WearTimerStops() {
    for (bool active : {false, true}) {
        Reset(); Fixture f; wearStopTest = true;
        f.proto.cLimitTimerBasedOnWearIndex = 0;
        auto& meta = g_registry.get<ecs::ItemPrototypeMeta>(f.item);
        meta.type = ITEM_COSTUME; meta.subType = RUNE_SLOT1;
        g_registry.get<ecs::ItemSockets>(f.item).sockets[0] = 5;
        g_registry.get<ecs::ItemSockets>(f.item).sockets[1] = active;
        Check(ItemSystem::StartTimerBasedOnWearExpireEventEcs(f.item), "wear stop fixture timer failed");
        ItemSystem::StopTimerBasedOnWearExpireEvent(f.item);
        Check(queued.front()->is_force_to_end && !g_registry.get<ecs::ItemEvents>(f.item).timerBasedOnWearExpire &&
            g_registry.get<ecs::ItemSockets>(f.item).sockets[0] == (active ? 0 : 5) && flushed == 1,
            "wear stop lost paused-rune semantics, nonnegative time, or cancellation");
    }
    for (int boundary = 0; boundary < 2; ++boundary) {
        Reset(); Fixture f; wearStopTest = true; f.proto.cLimitTimerBasedOnWearIndex = 0;
        ItemSystem::StartTimerBasedOnWearExpireEventEcs(f.item);
        if (boundary == 0) onSave = [](entt::entity item) { g_registry.destroy(item); };
        else onUpdate = [](entt::entity item) { g_registry.destroy(item); };
        ItemSystem::StopTimerBasedOnWearExpireEvent(f.item);
        Check(queued.front()->is_force_to_end && flushed == 0, "wear stop reused a retired entity after publication");
    }
    Reset(); Fixture f; wearStopTest = true; f.proto.cLimitTimerBasedOnWearIndex = 0;
    ItemSystem::StartTimerBasedOnWearExpireEventEcs(f.item);
    onSave = [](entt::entity item) { Check(ItemSystem::StartTimerBasedOnWearExpireEventEcs(item), "nested wear restart failed"); };
    ItemSystem::StopTimerBasedOnWearExpireEvent(f.item);
    Check(queued.size() == 2 && queued[0]->is_force_to_end && !queued[1]->is_force_to_end &&
        g_registry.get<ecs::ItemEvents>(f.item).timerBasedOnWearExpire == queued[1], "wear stop canceled a replacement timer");
}
void TimerFailuresAndCallbacks() {
    Reset(); Fixture f; f.FirstUse(true); failAllocation = true;
    Check(!ItemSystem::OnAfterCreatedItem(f.item), "failed timer allocation reported success");
    failAllocation = false;
    Check(ItemSystem::OnAfterCreatedItem(f.item), "failed allocation prevented a retry");

    Reset(); Fixture destroyed; destroyed.FirstUse(true);
    entt::entity replacement = entt::null;
    onCreate = [&](entt::entity item) { g_registry.destroy(item); replacement = g_registry.create(); };
    Check(!ItemSystem::OnAfterCreatedItem(destroyed.item) && queued.front()->is_force_to_end,
        "creation-time retirement left an orphan timer");
    Check(g_registry.valid(replacement) && !g_registry.any_of<ecs::ItemEvents>(replacement),
        "a recycled generation inherited the timer");

    Reset(); Fixture removed; removed.FirstUse(true);
    onComponent = [](entt::entity item) { g_registry.remove<ecs::ItemEvents>(item); };
    Check(!ItemSystem::OnAfterCreatedItem(removed.item) && queued.empty(), "removed event component was reused");

    Reset(); Fixture nested; nested.FirstUse(true);
    bool reentered = false;
    onCreate = [&](entt::entity item) {
        if (!reentered) { reentered = true; Check(ItemSystem::OnAfterCreatedItem(item), "nested start failed"); }
    };
    Check(ItemSystem::OnAfterCreatedItem(nested.item) && queued.size() == 2,
        "nested start was not exercised");
    Check(queued[0]->is_force_to_end && !queued[1]->is_force_to_end &&
        g_registry.get<ecs::ItemEvents>(nested.item).realTimeExpire == queued[1],
        "outer timer overwrote the nested timer");

    Reset(); Fixture published; published.FirstUse(true);
    onPublish = [](entt::entity item) {
        event_cancel(&g_registry.get<ecs::ItemEvents>(item).realTimeExpire);
        g_registry.destroy(item);
    };
    Check(!ItemSystem::OnAfterCreatedItem(published.item), "publication-time retirement reported a live item");
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
    std::array<LPEVENT, 8> timers;
    for (auto& timer : timers) timer = LPEVENT(new EVENT);
    events.destroy = timers[0]; events.expire = timers[1]; events.ownership = timers[2];
    events.uniqueExpire = timers[3]; events.soulItem = timers[4]; events.timerBasedOnWearExpire = timers[5];
    events.realTimeExpire = timers[6]; events.accessorySocketExpire = timers[7];
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


void NativePickups() {
    struct PickupFixture {
        TItemTable proto {};
        entt::entity owner, item;
        PickupFixture(uint8_t type = ITEM_WEAPON) {
            Reset(); pickupTest = true;
            owner = g_registry.create();
            g_registry.emplace<Actor>(owner).pc = true;
            g_registry.emplace<ecs::MainInventoryRuntimeComponent>(owner);
            g_registry.emplace<ecs::ExtraInventoryRuntimeComponent>(owner);
            proto.dwVnum = 500; proto.bSize = 1; proto.bType = type;
            item = EntityFactory::CreateItemEntity(g_registry, &proto, 500, type == ITEM_ELK ? 0 : 1501, 1502);
            g_registry.emplace<Actor>(item).ground = true;
            g_registry.get<ecs::ItemLocation>(item).window = GROUND;
            g_registry.get<ecs::ItemCount>(item).count = 7;
        }
        bool Pickup() { return ItemSystem::PickupItem(owner, 1502); }
    };
    for (uint8_t type : {ITEM_WEAPON, ITEM_MATERIAL, ITEM_DS}) {
        PickupFixture f(type);
        Check(f.Pickup() && ItemSystem::GetItemOwner(f.item) == f.owner && pickupDetaches == 1 &&
            pickupPlacements == 1 && lastPickupWindow == (type == ITEM_DS ? DRAGON_SOUL_INVENTORY :
                type == ITEM_MATERIAL ? EXTRA_INVENTORY : INVENTORY),
            "native pickup lost its normal/extra/dragon-soul window");
        Check(!f.Pickup() && pickupPlacements == 1, "stored item picked twice");
    }
    for (int invalid = 0; invalid < 8; ++invalid) {
        PickupFixture f;
        if (invalid == 0) g_registry.get<Actor>(f.owner).dead = true;
        if (invalid == 1) g_registry.get<Actor>(f.owner).observer = true;
        if (invalid == 2) g_registry.get<Actor>(f.item).map = 2;
        if (invalid == 3) g_registry.get<Actor>(f.item).x = 2401;
        if (invalid == 4) { g_registry.get<Actor>(f.item).x = INT_MAX; g_registry.get<Actor>(f.owner).x = INT_MIN; }
        if (invalid == 5) g_registry.get<ecs::ItemFlags>(f.item).isLocked = true;
        if (invalid == 6) pickupAllowed = false;
        if (invalid == 7) pickupCell = -1;
        Check(!f.Pickup() && pickupDetaches == 0 && pickupPlacements == 0 && ItemSystem::IsValidItem(f.item),
            "invalid/cross-map/full-inventory pickup modified the item");
    }
    {
        PickupFixture f; rejectPickupPlace = true;
        Check(!f.Pickup() && pickupRestores == 1 && ItemSystem::GetItemWindow(f.item) == GROUND &&
            ItemSystem::GetItemOwner(f.item) == entt::null, "failed pickup placement stranded the item");
    }
    {
        PickupFixture f;
        onPickupDetach = [&](entt::entity item) { Check(!f.Pickup(), "ground callback reentered pickup"); g_registry.destroy(item); };
        Check(!f.Pickup() && pickupPlacements == 0, "pickup reused a destroyed entity after detachment");
    }
    {
        PickupFixture f;
        const auto recipient = g_registry.create(); g_registry.emplace<Actor>(recipient).pc = true;
        onPickupDetach = [&](entt::entity item) { g_registry.get<ecs::ItemOwner>(item).owner = recipient; };
        Check(!f.Pickup() && pickupRestores == 0 && ItemSystem::GetItemOwner(f.item) == recipient,
            "pickup rollback stole a callback-transferred item");
    }
    {
        PickupFixture f;
        onPickupPlace = [&](entt::entity item) { g_registry.destroy(item); };
        Check(f.Pickup() && pickupPlacements == 1, "committed pickup read a retired entity");
    }
    for (bool reject : {false, true}) {
        PickupFixture f(ITEM_ELK); rejectPickupDestroy = reject;
        onPickupDestroy = [&](entt::entity) { Check(!f.Pickup(), "gold destruction callback reentered pickup"); };
        Check(f.Pickup() == !reject && pickupGold == (reject ? 0 : 7) && pickupSaves == (reject ? 0 : 1),
            "gold was duplicated or credited before successful retirement");
        Check(!reject || ItemSystem::IsValidItem(f.item), "rejected gold retirement lost the item");
    }
}

}
int main() {
    ITEM_MANAGER itemManager;
    g_dispatcher.sink<ecs::EvItemExpired>().connect<&Published>();
    g_registry.on_construct<ecs::ItemEvents>().connect<&Constructed>();
    g_registry.on_construct<ecs::ItemIdentity>().connect<&FactoryIdentity>();
    g_dispatcher.sink<ecs::EvItemDestroyed>().connect<&Retired>();
    try { NativeFactory(); NativePrototypeRefresh(); FactoryCallbacks(); NativePickups(); NativeInventoryAndUse(); NativeNpcItems(); NativeIdentityRegistry(); LocalizedNames(); LevelChecks(); LoadedTimers(); WearTimers(); WearTimerStops(); TimerFailuresAndCallbacks(); Reset(); }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    std::cout << "Item runtime: " << checks << " checks passed\n";
}
