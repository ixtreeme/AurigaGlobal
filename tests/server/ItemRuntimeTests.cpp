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
#include <functional>
#include <iostream>
#include <stdexcept>

entt::registry g_registry;
entt::dispatcher g_dispatcher;
int passes_per_sec = 25;
int test_server = 0;
int g_iDbLogLevel = 0;
const int aiAccessorySocketDegradeTime[ITEM_ACCESSORY_SOCKET_MAX_NUM + 1] = {};

namespace {
int checks = 0;
bool failAllocation = false;
std::vector<LPEVENT> queued;
std::function<void(entt::entity)> onCreate, onPublish, onComponent;
void Check(bool value, const char* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
[[noreturn]] void Unexpected() { throw std::runtime_error("unexpected legacy/live service"); }
void Published(const ecs::EvItemExpired& event) { if (onPublish) onPublish(event.itemEntity); }
void Constructed(entt::registry&, entt::entity item) { if (onComponent) onComponent(item); }
void Reset() {
    onCreate = onPublish = onComponent = {};
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
    Check(itemInfo != nullptr && delay > 0, "timer lost its entity payload or positive delay");
    if (failAllocation) return nullptr;
    queued.push_back(result);
    if (onCreate) onCreate(itemInfo->item);
    return result;
}
void event_cancel(LPEVENT* event) {
    if (!event || !*event) return;
    (*event)->is_force_to_end = 1;
    event->reset();
}
EVENTFUNC(real_time_expire_event) { throw std::runtime_error("unexpected timer execution"); }
EVENTFUNC(soul_item_event) { throw std::runtime_error("unexpected timer execution"); }
void ITEM_MANAGER::RemoveItem(entt::entity, const char*) { throw std::runtime_error("unexpected item removal"); }
namespace AffectSystem {
CAffect* FindAffect(entt::entity, uint32_t, uint8_t) { return nullptr; }
bool RemoveAffect(entt::entity, uint32_t) { return false; }
}
void ecs::ChatSystem::SendNew(entt::entity, uint8_t, uint32_t, const char*, ...) {}
// Link seams for other ItemSystem operations. Tests must never accidentally
// pass by falling back to CHARACTER/CItem or starting live server services.
int MIN(int a, int b) { return std::min(a, b); }
int MINMAX(int low, int value, int high) { return std::clamp(value, low, high); }
int number_ex(int, int, const char*, int) { Unexpected(); }
int32_t event_processing_time(LPEVENT) { Unexpected(); }
int32_t event_time(LPEVENT) { Unexpected(); }
EVENTFUNC(unique_expire_event) { Unexpected(); }
EVENTFUNC(timer_based_on_wear_expire_event) { Unexpected(); }
EVENTFUNC(accessory_socket_expire_event) { Unexpected(); }
namespace ecs::PlayerRuntime {
entt::entity FindByPlayerID(uint32_t) { Unexpected(); }
uint32_t GetPlayerID(entt::entity) { Unexpected(); }
uint8_t GetJob(entt::entity) { Unexpected(); }
std::string_view GetName(entt::entity) { Unexpected(); }
int32_t GetX(entt::entity) { Unexpected(); }
int32_t GetY(entt::entity) { Unexpected(); }
LPSECTREE GetSectree(entt::entity) { Unexpected(); }
bool IsValid(entt::entity) { Unexpected(); }
}
CParty* ecs::SocialSystem::GetParty(entt::entity) { Unexpected(); }
entt::entity InventorySystem::RemoveFromCharacter(entt::entity) { Unexpected(); }
namespace ItemSystem {
entt::entity GetWearItem(entt::entity, uint8_t) { Unexpected(); }
bool UnequipItemEcs(entt::entity, entt::entity) { Unexpected(); }
bool EquipItemEcs(entt::entity, entt::entity, int) { Unexpected(); }
uint32_t GetItemCount(entt::entity) { Unexpected(); }
bool SetItemCountEcs(entt::entity, uint32_t) { Unexpected(); }
void ModifyPoints(entt::entity, bool) { Unexpected(); }
bool IsItemConsumptionPending(entt::entity) { Unexpected(); }
bool RefreshItemOwnerPID(entt::entity) { Unexpected(); }
bool SetGroundOwnership(entt::entity, entt::entity, int) { Unexpected(); }
bool UseItemEx(entt::entity, entt::entity, TItemPos) { Unexpected(); }
}
bool CHARACTER::CanReceiveItem(entt::entity, LPITEM) const { Unexpected(); }
void CHARACTER::ReceiveItem(entt::entity, LPITEM) { Unexpected(); }
bool CHARACTER::GiveItemFromSpecialItemGroup(uint32_t, std::vector<uint32_t>&,
    std::vector<uint32_t>&, std::vector<entt::entity>&, int&) { Unexpected(); }
void CParty::ChatPacketToAllMemberNew(uint8_t, uint32_t, const char*, ...) { Unexpected(); }
void MountSystem::ForceClearRidingState(entt::entity) { Unexpected(); }
void ecs::ItemNetworkSystem::SendItemUpdate(entt::registry&, entt::entity) { Unexpected(); }
void ecs::PointSystem::Change(entt::entity, uint8_t, int64_t, bool, bool, bool) { Unexpected(); }
int CItem::GetSpecialGroup() const { Unexpected(); }
uint32_t CItem::GetSIGVnum() const { Unexpected(); }
void CItem::ChangeRuneAttr(int32_t) { Unexpected(); }
void CItem::ActivateRuneBonus() { Unexpected(); }
void CItem::DeactivateRuneBonus() { Unexpected(); }
void CItem::ActivateRune() { Unexpected(); }
void CItem::DeactivateRune() { Unexpected(); }
void ITEM_MANAGER::DelayedSave(entt::entity) { Unexpected(); }
void ITEM_MANAGER::FlushDelayedSave(entt::entity) { Unexpected(); }
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

namespace {
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
    Check(!g_registry.any_of<ecs::LegacyItemPtr>(f.item), "load required a CItem");
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
}
int main() {
    g_dispatcher.sink<ecs::EvItemExpired>().connect<&Published>();
    g_registry.on_construct<ecs::ItemEvents>().connect<&Constructed>();
    try { NativeIdentityRegistry(); LevelChecks(); LoadedTimers(); TimerFailuresAndCallbacks(); Reset(); }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    std::cout << "Item runtime: " << checks << " checks passed\n";
}
