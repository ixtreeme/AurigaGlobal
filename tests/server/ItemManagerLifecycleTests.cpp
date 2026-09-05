#include "../../SRC/Server/GameServer/stdafx.h"
#include "../../SRC/Server/GameServer/utils.h"
#include "../../SRC/Server/GameServer/item_manager.h"
#include "../../SRC/Server/GameServer/item.h"
#include "../../SRC/Server/GameServer/char.h"
#include "../../SRC/Server/GameServer/safebox.h"
#include "../../SRC/Server/GameServer/log.h"
#include "../../SRC/Server/GameServer/desc_client.h"
#include "../../SRC/Server/GameServer/config.h"
#include "../../SRC/Server/GameServer/constants.h"
#include "../../SRC/Server/GameServer/char_manager.h"
#include "../../SRC/Server/GameServer/db.h"
#include "../../SRC/Server/GameServer/skill.h"
#include "../../SRC/Server/GameServer/priv_manager.h"
#include "../../SRC/Server/GameServer/questmanager.h"
#include "../../SRC/Server/GameServer/blend_item.h"
#include "../../SRC/Server/GameServer/DragonSoul.h"
#include "../../SRC/Server/GameServer/refine.h"
#include "../../SRC/Server/GameServer/item_manager_private_types.h"
#include "../../SRC/Server/GameServer/ecs/EntityFactory.hpp"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/InventorySystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ItemSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/NetworkSyncSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/AffectSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PointSystem.hpp"
#include <Core/Logging.hpp>
#include <functional>
#include <iostream>
#include <stdexcept>

entt::registry g_registry;
LPCLIENT_DESC db_clientdesc = nullptr;

namespace {
int checks = 0, groundCalls = 0, detachCalls = 0, factoryCalls = 0, frees = 0;
bool rejectFactory = false;
std::function<void(entt::entity)> onGround, onDetach, onFactory;
std::map<std::pair<entt::entity, uint16_t>, entt::entity> inventory;
void Check(bool value, const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
[[noreturn]] void Unexpected() { throw std::runtime_error("unexpected legacy/live service"); }
struct Player {};
class Manager : public ITEM_MANAGER {
public:
    void Register(entt::entity item) {
        const auto& identity = g_registry.get<ecs::ItemIdentity>(item);
        m_VIDMap[identity.vid] = item;
        if (identity.id) m_map_pkItemByID[identity.id] = item;
        m_set_pkItemForDelayedSave.insert(item);
    }
    bool Indexed(entt::entity item) const {
        return std::any_of(m_VIDMap.begin(), m_VIDMap.end(), [=](auto& row) { return row.second == item; }) ||
            std::any_of(m_map_pkItemByID.begin(), m_map_pkItemByID.end(), [=](auto& row) { return row.second == item; }) ||
            m_set_pkItemForDelayedSave.contains(item);
    }
    bool Busy(entt::entity item) const { return m_itemsBeingDestroyed.contains(item); }
    ~Manager() { m_VIDMap.clear(); m_map_pkItemByID.clear(); m_set_pkItemForDelayedSave.clear(); }
};
void Reset() {
    g_registry.clear(); inventory.clear();
    groundCalls = detachCalls = factoryCalls = frees = 0;
    rejectFactory = false;
    onGround = onDetach = onFactory = {};
}
entt::entity Item(Manager& manager, uint32_t id = 10, uint32_t vid = 20) {
    const auto item = g_registry.create();
    auto& identity = g_registry.emplace<ecs::ItemIdentity>(item);
    identity.id = id; identity.vid = vid; identity.vnum = 100;
    g_registry.emplace<ecs::ItemOwner>(item);
    g_registry.emplace<ecs::ItemLocation>(item, ecs::ItemLocation {RESERVED_WINDOW, 0});
    g_registry.emplace<ecs::ItemFlags>(item).skipSave = true;
    manager.Register(item);
    return item;
}
entt::entity Owner(entt::entity item, uint8_t window = INVENTORY, uint16_t cell = 5) {
    const auto owner = g_registry.create(); g_registry.emplace<Player>(owner);
    g_registry.get<ecs::ItemOwner>(item).owner = owner;
    g_registry.get<ecs::ItemLocation>(item) = {window, cell};
    inventory[{owner, cell}] = item;
    return owner;
}
}

std::shared_ptr<spdlog::logger> logging::GetErrorLogger() {
    static auto logger = std::make_shared<spdlog::logger>("item-manager-error-test"); return logger;
}
std::shared_ptr<spdlog::logger> logging::GetLogger() {
    static auto logger = std::make_shared<spdlog::logger>("item-manager-test"); return logger;
}

// Only the allocation boundary is exercised for legacy-backed test items.
// The engine CItem/CEntity constructors, destructors and factory are doubles.
CEntity::CEntity() = default;
CEntity::~CEntity() = default;
CItem::CItem(uint32_t vnum) : m_pProto(nullptr), m_dwVnum(vnum), m_dwID(0), m_dwVID(0),
    m_dwCount(0), m_lFlag(0), m_dwMaskVnum(0), m_dwSIGVnum(0) {}
CItem::~CItem() { Check(GetEntityHandle() == entt::null, "allocation freed before entity unbinding"); ++frees; }
void CLIENT_DESC::DBPacket(uint8_t, uint32_t, const void*, uint32_t) { Unexpected(); }

namespace ItemSystem {
bool IsValidItem(entt::entity item) { return g_registry.valid(item) && g_registry.all_of<ecs::ItemIdentity>(item); }
uint32_t GetItemID(entt::entity item) { return g_registry.get<ecs::ItemIdentity>(item).id; }
uint32_t GetItemVID(entt::entity item) { return g_registry.get<ecs::ItemIdentity>(item).vid; }
const char* GetItemName(entt::entity item) { Check(IsValidItem(item), "name read after destruction"); return "item"; }
bool GetItemSkipSave(entt::entity item) { return g_registry.get<ecs::ItemFlags>(item).skipSave; }
entt::entity GetItemOwner(entt::entity item) {
    const auto owner = g_registry.get<ecs::ItemOwner>(item).owner;
    return g_registry.valid(owner) ? owner : entt::null;
}
void SetItemOwnerEntity(entt::entity item, entt::entity owner) { g_registry.get<ecs::ItemOwner>(item).owner = owner; }
uint32_t GetItemLastOwnerPID(entt::entity item) { return g_registry.get<ecs::ItemOwner>(item).lastOwnerPID; }
uint8_t GetItemWindow(entt::entity item) { return g_registry.get<ecs::ItemLocation>(item).window; }
uint16_t GetItemCell(entt::entity item) { return g_registry.get<ecs::ItemLocation>(item).cell; }
entt::entity GetItem(entt::entity owner, TItemPos position) {
    Check(g_registry.valid(owner), "inventory lookup with stale owner");
    const auto cell = static_cast<uint16_t>(position.window_type == EQUIPMENT ? INVENTORY_MAX_NUM + position.cell : position.cell);
    const auto it = inventory.find({owner, cell});
    return it == inventory.end() ? entt::null : it->second;
}
}
namespace InventorySystem {
entt::entity RemoveFromGround(entt::entity item) {
    Check(ItemSystem::IsValidItem(item), "ground removal received stale item");
    ++groundCalls; if (onGround) onGround(item); return item;
}
entt::entity RemoveFromCharacter(entt::entity item) {
    Check(ItemSystem::IsValidItem(item), "detachment received stale item");
    ++detachCalls;
    const auto owner = ItemSystem::GetItemOwner(item);
    const auto cell = ItemSystem::GetItemCell(item);
    Check(g_registry.valid(owner), "detachment used stale owner");
    if (auto it = inventory.find({owner, cell}); it != inventory.end() && it->second == item) inventory.erase(it);
    ItemSystem::SetItemOwnerEntity(item, entt::null);
    if (onDetach) onDetach(item);
    return item;
}
}
void EntityFactory::DestroyItemEntity(entt::registry& registry, entt::entity item) {
    Check(ItemSystem::IsValidItem(item), "duplicate factory destruction");
    ++factoryCalls;
    if (onFactory) onFactory(item);
    if (rejectFactory || !registry.valid(item)) return;
    if (const auto* legacy = registry.try_get<ecs::LegacyItemPtr>(item); legacy && legacy->ptr)
        legacy->ptr->SetEntityHandle(entt::null);
    registry.destroy(item);
}
namespace ecs::PlayerRuntime {
std::string_view GetName(entt::entity owner) { Check(g_registry.valid(owner), "stale owner name lookup"); return "owner"; }
}

// The entire original manager translation unit is linked. Dependencies of
// unrelated creation, drop and legacy removal paths must fail if exercised.
const int* aiPercentByDeltaLev = nullptr;
const int* aiPercentByDeltaLevForBoss = nullptr;
int test_server = 0;
int g_bItemCountLimit = 200;
std::vector<CItemDropInfo> g_vec_pkCommonDropItem[MOB_RANK_MAX_NUM];
int MAX(int, int) { Unexpected(); }
int MINMAX(int, int, int) { Unexpected(); }
int number_ex(int, int, const char*, int) { Unexpected(); }
time_t get_global_time() { Unexpected(); }
const uint32_t GetRandomSkillVnum(uint8_t) { Unexpected(); }
const char* get_table_postfix() { Unexpected(); }
bool AffectSystem::IsPolymorphed(entt::entity) { Unexpected(); }
uint32_t CHARACTER::GetMobDropItemVnum() const { Unexpected(); }
uint32_t CHARACTER::GetPolymorphItemVnum() const { Unexpected(); }
void CHARACTER::ComputePoints() { Unexpected(); }
void CHARACTER::SyncQuickslot(uint8_t, uint8_t, uint8_t) { Unexpected(); }
bool CHARACTER::IsEquipUniqueGroup(uint32_t) const { Unexpected(); }
void CHARACTER::UpdateMountCountOverheadToViewers() { Unexpected(); }
CSafebox* CHARACTER::GetSafebox() const { Unexpected(); }
void CHARACTER::SendMountInventory() { Unexpected(); }
CSafebox* CHARACTER::GetMall() const { Unexpected(); }
int CHARACTER::GetPremiumRemainSeconds(uint8_t) const { Unexpected(); }
entt::entity CSafebox::Remove(uint32_t) { Unexpected(); }
int64_t ecs::PointSystem::Get(entt::entity, uint8_t) { Unexpected(); }
int ecs::PointSystem::GetLevel(entt::entity) { Unexpected(); }
namespace ecs::PlayerRuntime {
LPDESC GetDesc(entt::entity) { Unexpected(); }
uint32_t GetPlayerID(entt::entity) { Unexpected(); }
uint8_t GetEmpire(entt::entity) { Unexpected(); }
uint32_t GetRaceNum(entt::entity) { Unexpected(); }
bool IsValid(entt::entity) { Unexpected(); }
bool IsPC(entt::entity) { Unexpected(); }
bool IsStone(entt::entity) { Unexpected(); }
uint8_t GetMobRank(entt::entity) { Unexpected(); }
int GetPremiumRemainSeconds(entt::entity, uint8_t) { Unexpected(); }
bool IsPCBang(entt::entity) { Unexpected(); }
}
void NetworkSyncSystem::PointsPacket(entt::entity) { Unexpected(); }
namespace ItemSystem {
bool IsEquipUniqueItem(entt::entity, uint32_t) { Unexpected(); }
bool IsEquipUniqueGroup(entt::entity, uint32_t) { Unexpected(); }
bool IsDragonSoulItem(entt::entity) { Unexpected(); }
uint32_t GetItemVnum(entt::entity) { Unexpected(); }
uint32_t GetItemOriginalVnum(entt::entity) { Unexpected(); }
void SetItemExtraProto(entt::entity, TItemExtraProto*) { Unexpected(); }
uint8_t GetItemType(entt::entity) { Unexpected(); }
uint32_t GetItemCount(entt::entity) { Unexpected(); }
uint8_t GetItemLimitType(entt::entity, uint32_t) { Unexpected(); }
int32_t GetItemLimitValue(entt::entity, uint32_t) { Unexpected(); }
int32_t GetItemFlags(entt::entity) { Unexpected(); }
bool DestroyItemEntityEcs(entt::entity, const char*) { Unexpected(); }
int16_t GetItemLockedAttr(entt::entity) { Unexpected(); }
void StartUniqueExpireEvent(entt::entity) { Unexpected(); }
void StartTimerBasedOnWearExpireEvent(entt::entity) { Unexpected(); }
uint32_t GetItemSocket(entt::entity, int) { Unexpected(); }
TPlayerItemAttribute GetItemAttribute(entt::entity, int) { Unexpected(); }
bool SetItemSocket(entt::entity, int, uint32_t, bool) { Unexpected(); }
bool SetItemForceAttributeEcs(entt::entity, int, uint8_t, int16_t) { Unexpected(); }
bool ApplyItemAddon(entt::entity, int) { Unexpected(); }
bool IsItemConsumptionPending(entt::entity) { Unexpected(); }
void ProcessPendingItemConsumptions() { Unexpected(); }
bool SetItemSkipSave(entt::entity, bool) { Unexpected(); }
bool AlterItemToMagicItem(entt::entity) { Unexpected(); }
bool IsItemEquipped(entt::entity) { Unexpected(); }
bool StartRealTimeExpireEventEcs(entt::entity) { Unexpected(); }
bool StartSoulItemEventEcs(entt::entity) { Unexpected(); }
bool SyncItemStateFromLegacy(entt::entity) { Unexpected(); }
}
int CHARACTER_MANAGER::GetMobItemRate(entt::entity) { Unexpected(); }
const event_struct_* CHARACTER_MANAGER::CheckEventIsActive(uint8_t, uint8_t) { Unexpected(); }
void CHARACTER_MANAGER::CheckEventForDrop(entt::entity, entt::entity, std::vector<entt::entity>&) { Unexpected(); }
void CLIENT_DESC::DBPacketHeader(uint8_t, uint32_t, uint32_t) { Unexpected(); }
void CLIENT_DESC::Packet(const void*, int) { Unexpected(); }
void DBManager::ReturnQuery(int, uint32_t, void*, const char*, ...) { Unexpected(); }
void DBManager::SendMoneyLog(uint8_t, uint32_t, int64_t) { Unexpected(); }
void LogManager::ItemLogEntity(entt::entity, entt::entity, const char*, const char*) { Unexpected(); }
CSkillProto* CSkillManager::Get(uint32_t) { Unexpected(); }
int CPrivManager::GetPriv(entt::entity, uint8_t) { Unexpected(); }
int quest::CQuestManager::GetEventFlag(const std::string&) { Unexpected(); }
void quest::CQuestManager::RegisterNPCVnum(uint32_t) { Unexpected(); }
bool Blend_Item_set_value(LPITEM) { Unexpected(); }
bool Blend_Item_find(uint32_t) { Unexpected(); }
void CItem::Initialize() { Unexpected(); }
uint8_t CItem::GetWindow() const { Unexpected(); }
void CItem::SetProto(const TItemTable*) { Unexpected(); }
const char* CItem::GetName(uint8_t) { Unexpected(); }
bool CItem::SetCount(int) { Unexpected(); }
int CItem::GetCount() { Unexpected(); }
int32_t CItem::GetValue(uint32_t) { Unexpected(); }
int32_t CItem::GetSocket(int) const { Unexpected(); }
void CItem::SetSocket(int, int32_t, bool) { Unexpected(); }
void CItem::AlterToSocketItem(int) { Unexpected(); }
int CItem::GetAttributeCount() { Unexpected(); }
bool CItem::IsExtraItem() { Unexpected(); }
void CItem::InitializeRune() { Unexpected(); }
uint32_t ITEM_MANAGER::GetNewID() { Unexpected(); }
entt::entity EntityFactory::CreateItemEntity(entt::registry&, LPITEM) { Unexpected(); }
bool DSManager::DragonSoulItemInitialize(entt::entity) { Unexpected(); }
const TRefineTable* CRefineManager::GetRefineRecipe(uint32_t) { Unexpected(); }

namespace {
void NativeAndLegacy() {
    for (const bool legacy : {false, true}) {
        Reset(); Manager manager;
        const auto item = Item(manager);
        const auto owner = Owner(item); // No CHARACTER; no player ID either.
        if (legacy) {
            auto* allocation = new CItem(100);
            allocation->SetEntityHandle(item);
            // Deliberately different legacy IDs: ECS identity must be used.
            allocation->SetID(999); allocation->SetVID(998);
            g_registry.emplace<ecs::LegacyItemPtr>(item).ptr = allocation;
        }
        onFactory = [&](entt::entity current) {
            Check(manager.Busy(current) && !manager.Indexed(current), "factory saw published manager references");
            manager.DestroyItem(current);
        };
        manager.DestroyItem(item);
        Check(!g_registry.valid(item) && !manager.Indexed(item) && !manager.Busy(item), "item not completely retired");
        Check(groundCalls == 1 && detachCalls == 1 && factoryCalls == 1 && frees == (legacy ? 1 : 0), "cleanup repeated or skipped");
        Check(!inventory.contains({owner, 5}), "zero-PID owner slot not cleared");
        manager.DestroyItem(item);
        Check(factoryCalls == 1, "repeated destruction entered factory");
    }
}
void ReentryAndExceptions() {
    for (int stage = 0; stage < 3; ++stage) {
        Reset(); Manager manager; const auto item = Item(manager); Owner(item);
        const auto recurse = [&](entt::entity current) {
            Check(manager.Busy(current), "destruction guard not set before callback");
            manager.DestroyItem(current);
            manager.RemoveItem(current);
            Check(g_registry.valid(current), "recursive removal bypassed guard");
        };
        if (stage == 0) onGround = recurse;
        if (stage == 1) onDetach = recurse;
        if (stage == 2) onFactory = recurse;
        manager.DestroyItem(item);
        Check(factoryCalls == 1 && !g_registry.valid(item) && !manager.Busy(item), "reentrant cleanup failed");
    }
    Reset(); Manager manager; const auto item = Item(manager);
    onGround = [](entt::entity) { throw std::runtime_error("injected callback failure"); };
    bool threw = false;
    try { manager.DestroyItem(item); } catch (const std::runtime_error&) { threw = true; }
    Check(threw, "exception not injected");
    Check(!manager.Busy(item) && g_registry.valid(item) && manager.Indexed(item), "guard or identity lost on exception");
    onGround = {}; manager.DestroyItem(item);
    Check(!g_registry.valid(item), "retry after exception failed");
}
void FactoryFailures() {
    for (const bool throws : {false, true}) {
        Reset(); Manager manager; const auto item = Item(manager);
        auto* allocation = new CItem(100); allocation->SetEntityHandle(item);
        g_registry.emplace<ecs::LegacyItemPtr>(item).ptr = allocation;
        rejectFactory = true;
        onFactory = [&](entt::entity current) {
            Check(!manager.Indexed(current) && manager.Busy(current), "failed factory saw published item");
            if (throws) throw std::runtime_error("factory failure");
        };
        bool caught = false;
        try { manager.DestroyItem(item); } catch (const std::runtime_error&) { caught = true; }
        Check(caught == throws && g_registry.valid(item) && manager.Indexed(item) && !manager.Busy(item) && frees == 0,
            "failed factory lost manager identity or freed allocation");
        rejectFactory = false; onFactory = {}; manager.DestroyItem(item);
        Check(!g_registry.valid(item) && !manager.Indexed(item) && frees == 1, "factory retry failed");
    }
    Reset(); Manager manager; const auto item = Item(manager);
    auto* allocation = new CItem(100);
    allocation->SetEntityHandle(entt::null);
    g_registry.emplace<ecs::LegacyItemPtr>(item).ptr = allocation;
    manager.DestroyItem(item);
    Check(g_registry.valid(item) && manager.Indexed(item) && frees == 0 && factoryCalls == 0,
        "foreign legacy allocation was destroyed");
    allocation->SetEntityHandle(item); manager.DestroyItem(item);
    Check(frees == 1, "repaired allocation could not be retired");
}
void RecycledAndTransferred() {
    for (int stage = 0; stage < 2; ++stage) {
        Reset(); Manager manager; const auto item = Item(manager); Owner(item);
        entt::entity replacement {entt::null};
        const auto recycle = [&](entt::entity current) {
            g_registry.destroy(current); replacement = Item(manager);
            Check(replacement != current && entt::to_entity(replacement) == entt::to_entity(current), "generation was not recycled");
        };
        if (stage == 0) onGround = recycle; else onDetach = recycle;
        manager.DestroyItem(item);
        Check(g_registry.valid(replacement) && manager.Indexed(replacement) && !manager.Indexed(item) && factoryCalls == 0,
            "old cleanup removed replacement entity or manager indices");
        onGround = onDetach = {}; manager.DestroyItem(replacement);
    }
    for (const bool fromGround : {false, true}) {
      for (const bool staleNewOwner : {false, true}) {
        Reset(); Manager manager; const auto item = Item(manager); Owner(item);
        const auto transfer = [&](entt::entity current) {
            const auto owner = Owner(current, INVENTORY, 7);
            if (staleNewOwner) g_registry.destroy(owner);
        };
        if (fromGround) onGround = transfer; else onDetach = transfer;
        manager.DestroyItem(item);
        Check(g_registry.valid(item) && manager.Indexed(item) && !manager.Busy(item) && factoryCalls == 0,
            "callback-transferred ownership was deleted");
        onGround = onDetach = {}; ItemSystem::SetItemOwnerEntity(item, entt::null); manager.DestroyItem(item);
      }
    }
}
void SlotAndPersistenceGuards() {
    for (int scenario = 0; scenario < 5; ++scenario) {
        Reset(); Manager manager; const auto item = Item(manager);
        const auto owner = Owner(item, scenario == 1 ? EQUIPMENT : INVENTORY,
            scenario == 1 ? INVENTORY_MAX_NUM + WEAR_BODY : 5);
        if (scenario == 0) inventory[{owner, 5}] = Item(manager, 11, 21);
        if (scenario == 2) g_registry.destroy(owner);
        if (scenario == 3) inventory.clear();
        if (scenario == 4) g_registry.get<ecs::ItemOwner>(item).lastOwnerPID = 999;
        manager.DestroyItem(item);
        Check(!g_registry.valid(item) && factoryCalls == 1 && detachCalls == (scenario == 1 || scenario == 4 ? 1 : 0),
            "slot identity/equipment/stale-owner validation failed");
        if (scenario == 0) {
            const auto replacement = inventory.at({owner, 5});
            Check(g_registry.valid(replacement) && manager.Indexed(replacement), "foreign inventory occupant was removed");
            manager.DestroyItem(replacement);
        }
    }
    Reset(); Manager manager;
    const auto persisted = Item(manager);
    g_registry.get<ecs::ItemFlags>(persisted).skipSave = false;
    manager.DestroyItem(persisted);
    Check(g_registry.valid(persisted) && manager.Indexed(persisted) && groundCalls == 0 && !manager.Busy(persisted),
        "missing DB descriptor lost persistent item");
    g_registry.get<ecs::ItemFlags>(persisted).skipSave = true;
    manager.DestroyItem(persisted);
    const auto transient = Item(manager, 0, 99);
    g_registry.get<ecs::ItemFlags>(transient).skipSave = false;
    manager.DestroyItem(transient);
    Check(!g_registry.valid(transient), "ID-less item incorrectly required DB connection");
    const auto nonItem = g_registry.create();
    manager.DestroyItem(nonItem); manager.DestroyItem(entt::null);
    Check(g_registry.valid(nonItem), "non-item entity destroyed");
}
}

int main() {
    try {
        NativeAndLegacy(); ReentryAndExceptions(); FactoryFailures(); RecycledAndTransferred(); SlotAndPersistenceGuards();
        std::cout << "Item-manager lifecycle checks passed: " << checks << '\n'; return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
