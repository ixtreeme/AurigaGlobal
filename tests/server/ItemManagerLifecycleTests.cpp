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
#include "../../SRC/Server/GameServer/dragon_soul_table.h"
#include "../../SRC/Server/GameServer/unique_item.h"
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
#include "../../SRC/Server/GameServer/ecs/systems/MountSystem.hpp"
#include <Core/Logging.hpp>
#include <functional>
#include <iostream>
#include <stdexcept>

entt::registry g_registry;
LPCLIENT_DESC db_clientdesc = nullptr;

namespace {
int checks = 0, groundCalls = 0, detachCalls = 0, factoryCalls = 0, frees = 0;
int logs = 0, quickslots = 0, mountPackets = 0, computes = 0, points = 0, overheads = 0;
uint16_t quickslotType = 0, quickslotCell = 0;
bool rejectFactory = false;
bool rejectDetach = false;
bool creationTest = false, entityOnlyFactory = false, rejectAllocation = false, rejectCount = false;
bool rejectRune = false, rejectDS = false, rejectTimer = false, blendExists = false;
bool highDraw = false;
uint32_t nextItemID = 1000;
int creationSaves = 0, allocations = 0;
std::function<void(const char*, entt::entity)> onCreation;
std::vector<std::string> creationStages;
std::set<uint32_t> availableSkills;
struct CreationProto { TItemTable value; };
std::function<void(entt::entity)> onGround, onDetach, onFactory;
std::function<void(entt::entity)> onLog, onQuickslot, onCompute;
std::map<std::pair<entt::entity, uint16_t>, entt::entity> inventory;
void Check(bool value, const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
[[noreturn]] void Unexpected() { throw std::runtime_error("unexpected legacy/live service"); }
void CreationStage(const char* stage, entt::entity item) {
    Check(creationTest && ItemSystem::IsValidItem(item), "creation service received stale item");
    creationStages.emplace_back(stage);
    if (onCreation) { auto callback = onCreation; callback(stage, item); }
}
const TItemTable& Proto(entt::entity item) {
    Check(g_registry.valid(item) && g_registry.all_of<CreationProto>(item), "stale creation prototype read");
    return g_registry.get<CreationProto>(item).value;
}
struct Player {};
class Manager : public ITEM_MANAGER {
public:
    void Prototype(TItemTable proto) { m_vec_prototype = {proto}; }
    TItemTable& Prototype() { return m_vec_prototype.front(); }
    void VID(uint32_t value) { m_dwVIDCount = value; }
    void Group(CSpecialItemGroup& group, bool quest) {
        (quest ? m_map_pkQuestItemGroup : m_map_pkSpecialItemGroup)[group.m_dwVnum] = &group;
    }
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
    onCreation = {}; creationTest = entityOnlyFactory = rejectAllocation = rejectCount = false;
    rejectRune = rejectDS = rejectTimer = blendExists = highDraw = false;
    nextItemID = 1000; creationSaves = allocations = 0; creationStages.clear(); availableSkills.clear();
    g_bItemCountLimit = 200;
    g_registry.clear(); inventory.clear();
    groundCalls = detachCalls = factoryCalls = frees = 0;
    logs = quickslots = mountPackets = computes = points = overheads = 0;
    rejectFactory = rejectDetach = false;
    onGround = onDetach = onFactory = {};
    onLog = onQuickslot = onCompute = {};
}
entt::entity Item(Manager& manager, uint32_t id = 10, uint32_t vid = 20) {
    const auto item = g_registry.create();
    auto& identity = g_registry.emplace<ecs::ItemIdentity>(item);
    identity.id = id; identity.vid = vid; identity.vnum = 100;
    g_registry.emplace<ecs::ItemOwner>(item);
    g_registry.emplace<ecs::ItemLocation>(item, ecs::ItemLocation {RESERVED_WINDOW, 0});
    g_registry.emplace<ecs::ItemFlags>(item).skipSave = true;
    g_registry.emplace<ecs::ItemCount>(item).count = 1;
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
    m_lFlag(0), m_dwMaskVnum(0) {}
CItem::~CItem() { Check(GetEntityHandle() == entt::null, "allocation freed before entity unbinding"); ++frees; }
void CLIENT_DESC::DBPacket(uint8_t, uint32_t, const void*, uint32_t) { Unexpected(); }
void DESC::Packet(const void*, int) { Unexpected(); }
CSemaphore::CSemaphore() = default;
CSemaphore::~CSemaphore() = default;
CAsyncSQL::CAsyncSQL() = default;
CAsyncSQL::~CAsyncSQL() = default;
CPoly::CPoly() = default;
CPoly::~CPoly() = default;
CSkillManager::CSkillManager() = default;
CSkillManager::~CSkillManager() = default;
DSManager::DSManager() = default;
DSManager::~DSManager() = default;
DragonSoulTable::~DragonSoulTable() = default;
LogManager::LogManager() : m_bIsConnect(false) {}
LogManager::~LogManager() = default;

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
    if (position.window_type == SAFEBOX || position.window_type == MALL) {
        const auto storage = SafeboxSystem::Get(owner, position.window_type);
        return storage ? storage->Get(position.cell) : entt::null;
    }
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
    if (rejectDetach) return entt::null;
    ++detachCalls;
    const auto owner = ItemSystem::GetItemOwner(item);
    const auto cell = ItemSystem::GetItemCell(item);
    Check(g_registry.valid(owner), "detachment used stale owner");
    if (auto it = inventory.find({owner, cell}); it != inventory.end() && it->second == item) inventory.erase(it);
    ItemSystem::SetItemOwnerEntity(item, entt::null);
    g_registry.get<ecs::ItemLocation>(item) = {RESERVED_WINDOW, 0};
    if (onDetach) onDetach(item);
    return item;
}
void SyncQuickslot(entt::entity owner, uint16_t type, uint16_t oldPos, uint16_t newPos) {
    Check(g_registry.valid(owner) && newPos == 255, "invalid native quickslot cleanup");
    ++quickslots; quickslotType = type; quickslotCell = oldPos;
    if (onQuickslot) onQuickslot(owner);
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
// unrelated drop and legacy removal paths must fail if exercised. Creation
// services below are controlled entity-only doubles, not live engine services.
const int* aiPercentByDeltaLev = nullptr;
const int* aiPercentByDeltaLevForBoss = nullptr;
int test_server = 0;
int g_bItemCountLimit = 200;
std::vector<CItemDropInfo> g_vec_pkCommonDropItem[MOB_RANK_MAX_NUM];
int MAX(int, int) { Unexpected(); }
int MINMAX(int, int, int) { Unexpected(); }
int number_ex(int low, int high, const char*, int) { Check(creationTest && low <= high, "invalid creation RNG"); return highDraw ? high : low; }
time_t get_global_time() { Check(creationTest, "unexpected clock"); return 10000; }
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
int64_t ecs::PointSystem::Get(entt::entity, uint8_t) { Unexpected(); }
int ecs::PointSystem::GetLevel(entt::entity) { Unexpected(); }
namespace ecs::PlayerRuntime {
LPDESC GetDesc(entt::entity owner) { Check(g_registry.valid(owner), "stale descriptor lookup"); return nullptr; }
uint32_t GetAccountID(entt::entity owner) { Check(g_registry.valid(owner), "stale account lookup"); return 0; }
uint32_t GetPlayerID(entt::entity) { Unexpected(); }
uint8_t GetEmpire(entt::entity) { Unexpected(); }
uint32_t GetRaceNum(entt::entity) { Unexpected(); }
bool IsValid(entt::entity) { Unexpected(); }
bool IsPC(entt::entity owner) { return g_registry.valid(owner) && g_registry.all_of<Player>(owner); }
bool IsStone(entt::entity) { Unexpected(); }
uint8_t GetMobRank(entt::entity) { Unexpected(); }
int GetPremiumRemainSeconds(entt::entity, uint8_t) { Unexpected(); }
bool IsPCBang(entt::entity) { Unexpected(); }
}
void NetworkSyncSystem::PointsPacket(entt::entity owner) { Check(g_registry.valid(owner), "stale points packet"); ++points; }
void ecs::PointSystem::Compute(entt::entity owner) {
    Check(g_registry.valid(owner), "stale point computation"); ++computes; if (onCompute) onCompute(owner);
}
void MountSystem::SendMountInventory(entt::entity owner) { Check(g_registry.valid(owner), "stale mount packet"); ++mountPackets; }
entt::entity MountSystem::GetMountInventoryItem(entt::entity owner, uint32_t cell) {
    return ItemSystem::GetItem(owner, TItemPos(MOUNT_INVENTORY, cell));
}
void MountSystem::UpdateMountCountOverheadToViewers(entt::entity owner) { Check(g_registry.valid(owner), "stale overhead"); ++overheads; }
namespace ItemSystem {
bool IsEquipUniqueItem(entt::entity, uint32_t) { Unexpected(); }
bool IsEquipUniqueGroup(entt::entity, uint32_t) { Unexpected(); }
bool IsDragonSoulItem(entt::entity item) { return GetItemType(item) == ITEM_DS; }
uint32_t GetItemVnum(entt::entity item) { return g_registry.get<ecs::ItemIdentity>(item).vnum; }
uint32_t GetItemOriginalVnum(entt::entity item) { return g_registry.get<ecs::ItemIdentity>(item).originalVnum; }
void SetItemExtraProto(entt::entity item, TItemExtraProto*) { CreationStage("extra", item); }
uint8_t GetItemType(entt::entity item) { return Proto(item).bType; }
int GetItemAttributeCount(entt::entity item) {
    const auto& attrs = g_registry.get<ecs::ItemAttributes>(item).attrs;
    return std::count_if(attrs.begin(), attrs.end(), [](auto a) { return a.bType != 0; });
}
uint32_t GetItemCount(entt::entity item) { return g_registry.get<ecs::ItemCount>(item).count; }
uint8_t GetItemLimitType(entt::entity, uint32_t) { Unexpected(); }
int32_t GetItemLimitValue(entt::entity, uint32_t) { Unexpected(); }
int32_t GetItemFlags(entt::entity item) { return g_registry.get<ecs::ItemFlags>(item).flags; }
bool DestroyItemEntityEcs(entt::entity item, const char* reason) {
    ITEM_MANAGER::instance().RemoveItem(item, reason); return !g_registry.valid(item);
}
int16_t GetItemLockedAttr(entt::entity) { Unexpected(); }
void StartUniqueExpireEvent(entt::entity item) { CreationStage("unique", item); }
void StartTimerBasedOnWearExpireEvent(entt::entity) { Unexpected(); }
uint32_t GetItemSocket(entt::entity, int) { return 0; }
TPlayerItemAttribute GetItemAttribute(entt::entity, int) { return {}; }
bool SetItemSocket(entt::entity, int, uint32_t, bool) { Unexpected(); }
bool SetItemForceAttributeEcs(entt::entity item, int index, uint8_t type, int16_t value) {
    g_registry.get<ecs::ItemAttributes>(item).attrs[index] = {type, value}; CreationStage("attribute", item); return true;
}
bool ApplyItemAddon(entt::entity item, int) { CreationStage("addon", item); return true; }
bool IsItemConsumptionPending(entt::entity item) { return g_registry.valid(item) && g_registry.get<ecs::ItemCount>(item).count < 0; }
void ProcessPendingItemConsumptions() { Unexpected(); }
bool SetItemSkipSave(entt::entity item, bool flag) { g_registry.get<ecs::ItemFlags>(item).skipSave = flag; return true; }
uint8_t GetItemSize(entt::entity) { return 1; }
uint32_t GetItemAntiFlags(entt::entity) { return 0; }
int16_t GetItemLockedAttributeIndex(entt::entity) { return -1; }
bool SetItemWindow(entt::entity item, uint8_t window) { g_registry.get<ecs::ItemLocation>(item).window = window; return true; }
bool SetItemCell(entt::entity item, entt::entity owner, uint16_t cell) {
    g_registry.get<ecs::ItemLocation>(item).cell = cell; SetItemOwnerEntity(item, owner); return true;
}
bool SaveItemEcs(entt::entity, bool) { return true; }
bool FlushDelayedSaveEcs(entt::entity item) { Check(GetItemSkipSave(item), "storage flush can delete persistence"); return true; }
bool RemoveItemEcs(entt::entity item) { return InventorySystem::RemoveFromCharacter(item) == item; }
bool IsItemExchanging(entt::entity) { return false; }
bool IsItemLocked(entt::entity) { return false; }
bool ConsumeItemEcs(entt::entity, uint32_t) { Unexpected(); }
bool AddItemCountEcs(entt::entity, int) { Unexpected(); }
bool AlterItemToMagicItem(entt::entity item) { CreationStage("magic", item); return true; }
bool IsItemEquipped(entt::entity) { Unexpected(); }
bool StartRealTimeExpireEventEcs(entt::entity item) { CreationStage("real-time", item); return !rejectTimer; }
bool StartSoulItemEventEcs(entt::entity item) { CreationStage("soul", item); return !rejectTimer; }
bool InitializeRuneItem(entt::entity item) { CreationStage("rune", item); return !rejectRune; }
void SaveItem(entt::entity item) { ++creationSaves; CreationStage("save", item); }
bool SyncItemStateFromLegacy(entt::entity) { Unexpected(); }
}
int CHARACTER_MANAGER::GetMobItemRate(entt::entity) { Unexpected(); }
const event_struct_* CHARACTER_MANAGER::CheckEventIsActive(uint8_t, uint8_t) { Unexpected(); }
void CHARACTER_MANAGER::CheckEventForDrop(entt::entity, entt::entity, std::vector<entt::entity>&) { Unexpected(); }
void CLIENT_DESC::DBPacketHeader(uint8_t, uint32_t, uint32_t) { Unexpected(); }
void CLIENT_DESC::Packet(const void*, int) { Unexpected(); }
void DBManager::ReturnQuery(int, uint32_t, void*, const char*, ...) { Unexpected(); }
void DBManager::SendMoneyLog(uint8_t, uint32_t, int64_t) { Unexpected(); }
void LogManager::ItemLogEntity(entt::entity owner, entt::entity item, const char* reason, const char* hint) {
    Check(g_registry.valid(owner) && ItemSystem::IsValidItem(item) && reason && std::string(hint) == "item 1 ", "invalid item log");
    ++logs; if (onLog) onLog(item);
}
CSkillProto* CSkillManager::Get(uint32_t id) {
    Check(creationTest, "unexpected skill query"); static CSkillProto skill;
    return availableSkills.contains(id) ? &skill : nullptr;
}
int CPrivManager::GetPriv(entt::entity, uint8_t) { Unexpected(); }
int quest::CQuestManager::GetEventFlag(const std::string&) { Unexpected(); }
void quest::CQuestManager::RegisterNPCVnum(uint32_t) { Unexpected(); }
bool Blend_Item_set_value(entt::entity item) {
    g_registry.get<ecs::ItemSockets>(item).sockets = {3, 7, 900}; CreationStage("blend", item); return true;
}
bool Blend_Item_find(uint32_t) { Check(creationTest, "unexpected blend query"); return blendExists; }
void CItem::Initialize() { Check(creationTest, "unexpected legacy initialization"); SetEntityHandle(entt::null); }
uint8_t CItem::GetWindow() const { Unexpected(); }
void CItem::SetProto(const TItemTable* proto) { Check(creationTest, "unexpected legacy proto"); m_pProto = proto; m_lFlag = proto->dwFlags; }
const char* CItem::GetName(uint8_t) { Unexpected(); }
bool CItem::SetCount(int) { Unexpected(); }
bool ItemSystem::SetItemCountEcs(entt::entity item, uint32_t count) {
    if (rejectCount) return false;
    g_registry.get<ecs::ItemCount>(item).count = static_cast<int>(count); CreationStage("count", item); return true;
}
int CItem::GetCount() { Unexpected(); }
int32_t CItem::GetValue(uint32_t) { Unexpected(); }
int32_t CItem::GetSocket(int) const { Unexpected(); }
void CItem::SetSocket(int, int32_t, bool) { Unexpected(); }
int CItem::GetAttributeCount() { Unexpected(); }
bool CItem::IsExtraItem() { Unexpected(); }
uint32_t ITEM_MANAGER::GetNewID() { Check(creationTest, "unexpected ID allocation"); return nextItemID++; }
entt::entity EntityFactory::CreateItemEntity(entt::registry& registry, LPITEM allocation) {
    Check(creationTest, "unexpected entity factory"); ++allocations;
    if (rejectAllocation) return entt::null;
    const auto item = registry.create();
    registry.emplace<CreationProto>(item, *allocation->GetProto());
    auto& identity = registry.emplace<ecs::ItemIdentity>(item);
    identity.id = allocation->GetID(); identity.vid = allocation->GetVID();
    identity.vnum = allocation->GetVnum(); identity.originalVnum = allocation->GetOriginalVnum();
    registry.emplace<ecs::ItemCount>(item);
    registry.emplace<ecs::ItemOwner>(item);
    registry.emplace<ecs::ItemLocation>(item, ecs::ItemLocation {RESERVED_WINDOW, 0});
    registry.emplace<ecs::ItemEquipped>(item);
    registry.emplace<ecs::ItemFlags>(item).flags = allocation->GetFlag();
    registry.emplace<ecs::ItemSockets>(item); registry.emplace<ecs::ItemAttributes>(item);
    if (entityOnlyFactory) delete allocation;
    else { allocation->SetEntityHandle(item); registry.emplace<ecs::LegacyItemPtr>(item, allocation); }
    return item;
}
bool DSManager::DragonSoulItemInitialize(entt::entity item) { CreationStage("dragon-soul", item); return !rejectDS; }
const TRefineTable* CRefineManager::GetRefineRecipe(uint32_t) { Unexpected(); }

namespace {
struct CreationFixture {
    Manager manager;
    explicit CreationFixture(bool native = true) {
        Reset(); creationTest = true; entityOnlyFactory = native;
        TItemTable proto {}; proto.dwVnum = 100; proto.bType = ITEM_WEAPON; proto.bSize = 1;
        manager.Prototype(proto);
    }
    entt::entity Create(uint32_t count = 1, uint32_t id = 0, bool magic = false, bool skip = false) {
        return manager.CreateItem(manager.Prototype().dwVnum, count, id, magic, -1, skip);
    }
    void Retire(entt::entity item) {
        if (!ItemSystem::IsValidItem(item)) return;
        g_registry.get<ecs::ItemFlags>(item).skipSave = true;
        manager.DestroyItem(item);
        Check(!g_registry.valid(item), "creation fixture could not retire item");
    }
    bool Stage(const char* stage) const {
        return std::find(creationStages.begin(), creationStages.end(), stage) != creationStages.end();
    }
};
void CreationQuantitiesAndIdentity() {
    for (bool native : {false, true}) {
        for (bool loaded : {false, true}) {
            for (bool skip : {false, true}) {
                CreationFixture f(native);
                f.manager.Prototype().dwFlags = ITEM_FLAG_STACKABLE;
                const auto item = f.Create(UINT32_MAX, loaded ? 400 : 0, false, skip);
                Check(ItemSystem::IsValidItem(item) && ItemSystem::GetItemCount(item) == 200, "creation count clamp failed");
                Check(ItemSystem::GetItemID(item) == (loaded ? 400 : 1000) && ItemSystem::GetItemVID(item) == 1,
                    "creation lost native ID/VID");
                Check(f.manager.Indexed(item) == !skip && !ItemSystem::GetItemSkipSave(item), "creation index/save policy changed");
                Check(g_registry.any_of<ecs::LegacyItemPtr>(item) == !native, "incorrect allocation fixture");
                Check(creationSaves == 1 && creationStages.back() == "save", "creation did not save exactly once at completion");
                Check(f.Stage("rune") == !loaded, "load repeated new-item initialization");
                f.Retire(item); Check(frees == allocations, "creation leaked/double-freed allocation");
            }
        }
    }
    for (uint32_t requested : {0u, 1u, 17u, UINT32_MAX}) {
        for (int kind = 0; kind < 3; ++kind) {
            CreationFixture f;
            auto& proto = f.manager.Prototype();
            if (kind == 1) proto.dwFlags = ITEM_FLAG_STACKABLE | ITEM_FLAG_MAKECOUNT;
            if (kind == 2) proto.bType = ITEM_ELK;
            proto.alValues[1] = 15;
            const auto item = f.Create(requested, 0, true);
            if (kind == 2 && requested == 0) { Check(item == entt::null && allocations == 0, "zero gold allocated"); continue; }
            const uint32_t expected = kind == 0 ? 1 : kind == 2 ? std::min(requested, uint32_t(INT_MAX)) :
                requested <= 1 ? 15 : std::min(requested, 200u);
            Check(ItemSystem::GetItemCount(item) == expected, "MAKECOUNT/non-stack/gold normalization changed");
            Check(kind != 2 || ItemSystem::GetItemID(item) == 0, "gold acquired persistent ID");
            f.Retire(item);
        }
    }
    for (int scenario = 0; scenario < 6; ++scenario) {
        CreationFixture f;
        if (scenario == 0) f.manager.Prototype().bSize = 0;
        if (scenario == 1) g_bItemCountLimit = 0;
        if (scenario == 2) nextItemID = 0;
        if (scenario == 3) f.manager.VID(UINT32_MAX);
        if (scenario == 4) f.manager.Prototype().dwFlags = ITEM_FLAG_STACKABLE | ITEM_FLAG_MAKECOUNT;
        if (scenario == 5) f.manager.Prototype().dwVnum = 0;
        Check(f.Create(1, 0, true) == entt::null && allocations == 0 && creationSaves == 0, "invalid creation had allocation effects");
    }
    for (bool stale : {false, true}) {
        CreationFixture f;
        const auto previous = Item(f.manager, 1000, 99);
        if (stale) g_registry.destroy(previous);
        const auto item = f.Create();
        if (stale) { Check(ItemSystem::IsValidItem(item) && f.manager.Indexed(item), "stale ID blocked creation"); f.Retire(item); }
        else { Check(item == entt::null && allocations == 0 && f.manager.Indexed(previous), "native duplicate ID overwritten"); f.Retire(previous); }
    }
}
void CreationPayloads() {
    for (int scenario = 0; scenario < 11; ++scenario) {
        CreationFixture f;
        auto& proto = f.manager.Prototype();
        proto.alValues[0] = 1234;
        if (scenario == 0) { proto.bType = ITEM_UNIQUE; proto.alValues[2] = 1; }
        if (scenario == 1) proto.bType = ITEM_UNIQUE;
        if (scenario == 2) proto.dwVnum = ITEM_AUTO_HP_RECOVERY_S;
        if (scenario == 3) proto.aLimits[0] = {LIMIT_TIMER_BASED_ON_WEAR, 876};
        if (scenario == 4) proto.aLimits[0] = {LIMIT_TIMER_BASED_ON_WEAR, 0};
        if (scenario == 5) proto.bGainSocketPct = 255;
        if (scenario == 6) { proto.bType = ITEM_USE; proto.bSubType = USE_TIME_CHARGE_PER; }
        if (scenario == 7) proto.dwVnum = 100000;
        if (scenario == 8) { proto.bType = ITEM_SOUL; proto.alValues[2] = 777; proto.aLimits[1].lValue = 10; }
        if (scenario == 9) { proto.bType = ITEM_USE; proto.bSubType = USE_NEW_POTIION; proto.aLimits[0].lValue = 555; }
        if (scenario == 10) { proto.bType = ITEM_SOUL; proto.alValues[2] = 100000; proto.aLimits[1].lValue = 10; }
        onCreation = [&](const char* stage, entt::entity item) {
            if (std::string_view(stage) == "unique")
                Check(g_registry.get<ecs::ItemSockets>(item).sockets[ITEM_SOCKET_UNIQUE_REMAIN_TIME] == 11234,
                    "unique timer started before its payload");
            if (std::string_view(stage) == "soul")
                Check(g_registry.get<ecs::ItemSockets>(item).sockets[2] == 777, "soul timer saw partial payload");
        };
        const auto item = f.Create();
        Check(ItemSystem::IsValidItem(item), "special item creation rejected");
        const auto sockets = g_registry.get<ecs::ItemSockets>(item).sockets;
        if (scenario < 2) Check(sockets[ITEM_SOCKET_UNIQUE_REMAIN_TIME] == (scenario == 0 ? 11234 : 1234), "unique duration changed");
        if (scenario == 2) Check(sockets[2] == 1234, "auto-potion capacity missing");
        if (scenario == 3 || scenario == 4) Check(sockets[0] == (scenario == 3 ? 876 : 36000), "wear duration/default missing");
        if (scenario == 5) Check(std::all_of(sockets.begin(), sockets.end(), [](auto v) { return v == 1; }), "socket holes overflow/missing");
        if (scenario == 6) Check(sockets[0] == 1234, "DS percent potion payload missing");
        if (scenario == 7) Check(sockets[ITEM_SOCKET_CHARGING_AMOUNT_IDX] == 1234, "DS charge missing");
        if (scenario == 8) Check(f.Stage("soul") && sockets[2] == 777, "soul initialization missing");
        if (scenario == 9) Check(sockets[0] == 555 && sockets[1] == 0, "new potion initialization missing");
        if (scenario == 10) Check(!f.Stage("soul") && sockets[2] == 100000, "fully charged soul was rejected or restarted");
        f.Retire(item);
    }
    for (bool loaded : {false, true}) {
        CreationFixture f;
        auto& proto = f.manager.Prototype(); proto.bAlterToMagicItemPct = 100; proto.sAddonType = 1;
        const auto item = f.Create(1, loaded ? 20 : 0, true);
        Check(f.Stage("magic") && f.Stage("addon") == !loaded, "new/load magic/addon dispatch changed"); f.Retire(item);
    }
    for (bool quest : {false, true}) {
        CreationFixture f; f.manager.Prototype().bType = quest ? ITEM_QUEST : ITEM_UNIQUE;
        CSpecialItemGroup first(50, quest ? CSpecialItemGroup::QUEST : CSpecialItemGroup::SPECIAL);
        CSpecialItemGroup last(60, first.m_bType); first.AddItem(100, 1, 1, 0); last.AddItem(100, 1, 1, 0);
        f.manager.Group(first, quest); f.manager.Group(last, quest);
        const auto item = f.Create(); Check(g_registry.get<ecs::ItemIdentity>(item).sigVnum == 60, "native SIG/order lost"); f.Retire(item);
    }
    {
        CreationFixture f; auto& proto = f.manager.Prototype(); proto.bType = ITEM_BLEND;
        proto.bAlterToMagicItemPct = 100; proto.sAddonType = 1; proto.bGainSocketPct = 3; blendExists = true;
        const auto item = f.Create(1, 0, true);
        const auto sockets = g_registry.get<ecs::ItemSockets>(item).sockets;
        Check(sockets[0] == 3 && sockets[1] == 7 && sockets[2] == 900, "blend payload overwritten");
        Check(f.Stage("blend") && !f.Stage("magic") && !f.Stage("addon") && !f.Stage("rune"), "blend early path changed"); f.Retire(item);
    }
    for (uint32_t vnum : {50300u, uint32_t(ITEM_SKILLFORGET_VNUM), uint32_t(ITEM_SKILLFORGET2_VNUM)}) {
        for (bool hasSkills : {false, true}) {
            CreationFixture f; f.manager.Prototype().dwVnum = vnum;
            if (hasSkills) availableSkills = {17, 114, 119};
            const auto item = f.Create();
            if (!hasSkills) {
                Check(item == entt::null && creationSaves == 0 && factoryCalls == 1, "missing skills did not reject and retire book");
            } else {
                Check(g_registry.get<ecs::ItemSockets>(item).sockets[0] == (vnum == ITEM_SKILLFORGET2_VNUM ? 114 : 17), "skill selection changed");
                f.Retire(item);
            }
        }
    }
}
void SkillBookSelection() {
    CreationFixture f;
    constexpr uint32_t skills[][12] = {
        {1, 2, 3, 4, 5, 6, 16, 17, 18, 19, 20, 21},
        {31, 32, 33, 34, 35, 36, 46, 47, 48, 49, 50, 51},
        {61, 62, 63, 64, 65, 66, 76, 77, 78, 79, 80, 81},
        {91, 92, 93, 94, 95, 96, 106, 107, 108, 109, 110, 111},
    };
    for (int job = 0; job < JOB_MAX_NUM; ++job) {
        for (uint32_t skill : skills[job]) {
            availableSkills = {skill, 999};
            Check(GetRandomSkillVnum(job) == skill && GetRandomSkillVnum(JOB_MAX_NUM) == skill,
                "bounded skill selection lost a supported skill/job");
            Check(GetRandomSkillVnum((job + 1) % JOB_MAX_NUM) == 0, "skill leaked between jobs");
        }
    }
    availableSkills.clear();
    for (uint8_t job : {uint8_t(0), uint8_t(JOB_MAX_NUM), uint8_t(255)})
        Check(GetRandomSkillVnum(job) == 0, "empty skill data did not terminate");
    for (const auto& group : skills) for (uint32_t skill : group) availableSkills.insert(skill);
    Check(GetRandomSkillVnum(JOB_MAX_NUM) == 1 && GetRandomSkillVnum(255) == 91, "skill low boundary/job clamp changed");
    highDraw = true;
    Check(GetRandomSkillVnum(JOB_MAX_NUM) == 111 && GetRandomSkillVnum(0) == 21, "skill high boundary changed");
}
void CreationFailuresAndCallbacks() {
    for (bool native : {false, true}) {
        for (int stage = 0; stage < 6; ++stage) {
            CreationFixture f(native);
            if (stage == 0) rejectAllocation = true;
            if (stage == 1) rejectCount = true;
            if (stage == 2) rejectRune = true;
            if (stage == 3) { rejectDS = true; f.manager.Prototype().bType = ITEM_DS; }
            if (stage == 4) { rejectTimer = true; f.manager.Prototype().aLimits[0] = {LIMIT_REAL_TIME, 100}; }
            if (stage == 5) { rejectTimer = true; f.manager.Prototype().bType = ITEM_SOUL; f.manager.Prototype().aLimits[1].lValue = 10; }
            Check(f.Create() == entt::null && creationSaves == 0 && frees == allocations, "failed creation leaked allocation or saved partial item");
            Check(g_registry.view<ecs::ItemIdentity>().size() == 0, "failed creation left live item");
        }
    }
    for (const char* stage : {"extra", "count", "addon", "magic", "rune", "unique", "real-time", "soul", "save"}) {
        for (int action = 0; action < 5; ++action) {
            CreationFixture f;
            auto& proto = f.manager.Prototype(); proto.sAddonType = 1; proto.bAlterToMagicItemPct = 100;
            proto.bType = std::string_view(stage) == "soul" ? ITEM_SOUL : ITEM_UNIQUE;
            proto.alValues[2] = 1; proto.alValues[0] = 100;
            proto.aLimits[0] = {LIMIT_REAL_TIME, 100};
            proto.aLimits[1].lValue = 10;
            entt::entity watched = entt::null, replacement = entt::null;
            onCreation = [&](const char* current, entt::entity item) {
                if (std::string_view(current) != stage) return;
                watched = item;
                if (action == 0) { g_registry.get<ecs::ItemFlags>(item).skipSave = true; f.manager.DestroyItem(item); }
                if (action == 1) {
                    const auto identity = g_registry.get<ecs::ItemIdentity>(item);
                    g_registry.destroy(item); replacement = Item(f.manager, identity.id, identity.vid);
                    Check(entt::to_entity(replacement) == entt::to_entity(item) && replacement != item, "factory callback did not recycle generation");
                }
                if (action == 2) Owner(item, INVENTORY, 17);
                if (action == 3) Check(f.manager.CreateItem(100, 1, ItemSystem::GetItemID(item)) == entt::null, "recursive duplicate ID accepted");
                if (action == 4) throw std::runtime_error("initializer failure");
            };
            bool threw = false; entt::entity result = entt::null;
            try { result = f.Create(1, 0, true); } catch (const std::runtime_error&) { threw = true; }
            Check(watched != entt::null && threw == (action == 4), "creation callback stage/exception missing");
            Check((result != entt::null) == (action == 3), "creation returned invalid/moved item");
            if (action < 2) Check(!f.manager.Indexed(watched), "retired generation still indexed");
            if (action == 1) Check(f.manager.Indexed(replacement) && g_registry.valid(replacement), "rollback destroyed replacement");
            if (action == 2) Check(g_registry.valid(watched) && f.manager.Indexed(watched) && !ItemSystem::GetItemSkipSave(watched),
                "rollback destroyed/unindexed/transiently stranded transferred item");
            if (action == 4 && std::string_view(stage) != "save") Check(!g_registry.valid(watched), "exception left detached partial item");
            onCreation = {}; f.Retire(watched); f.Retire(replacement);
        }
    }
    for (bool throws : {false, true}) {
        CreationFixture f(false); entt::entity item = entt::null;
        onCreation = [&](const char* stage, entt::entity current) {
            if (std::string_view(stage) != "rune") return;
            item = current; rejectRune = rejectFactory = true;
            if (throws) onFactory = [](entt::entity) { throw std::runtime_error("retirement failure"); };
        };
        Check(f.Create() == entt::null && g_registry.valid(item) && f.manager.Indexed(item) && !f.manager.Busy(item) && frees == 0,
            "failed rollback lost recoverable item/indices");
        onFactory = {}; rejectFactory = false; f.Retire(item);
    }
}
void CreationRollbackTransfers() {
    for (bool throws : {false, true}) {
        CreationFixture f(false); rejectRune = true;
        entt::entity transferred = entt::null;
        onGround = [&](entt::entity item) {
            transferred = item; Owner(item, INVENTORY, 17);
            if (throws) throw std::runtime_error("transfer during failed creation cleanup");
        };
        Check(f.Create() == entt::null && ItemSystem::IsValidItem(transferred) && f.manager.Indexed(transferred) &&
            !ItemSystem::GetItemSkipSave(transferred) && !f.manager.Busy(transferred) && factoryCalls == 0,
            "rollback callback stranded/transiently marked its new owner's item");
        onGround = {}; f.Retire(transferred);
    }
}
void NativeRemoval() {
    for (const bool legacy : {false, true}) {
        for (const uint8_t window : {INVENTORY, EQUIPMENT, EXTRA_INVENTORY, DRAGON_SOUL_INVENTORY, SWITCHBOT, MOUNT_INVENTORY}) {
            Reset(); Manager manager; const auto item = Item(manager);
            const uint16_t cell = window == EQUIPMENT ? INVENTORY_MAX_NUM + WEAR_BODY : 300;
            const auto owner = Owner(item, window, cell);
            if (legacy) {
                auto* allocation = new CItem(100); allocation->SetEntityHandle(item);
                g_registry.emplace<ecs::LegacyItemPtr>(item).ptr = allocation;
            }
            manager.RemoveItem(item, "TEST_REMOVE");
            Check(!g_registry.valid(item) && !manager.Indexed(item) && !manager.Busy(item), "high-level removal left item published");
            Check(logs == 1 && detachCalls == 1 && factoryCalls == 1 && frees == (legacy ? 1 : 0), "high-level cleanup repeated/skipped");
            Check(!inventory.contains({owner, cell}), "high-level cleanup left owner slot");
            const bool hasQuickslot = window == INVENTORY || window == EXTRA_INVENTORY;
            Check(quickslots == (hasQuickslot ? 1 : 0), "foreign window quickslot was cleared");
            if (hasQuickslot) Check(quickslotCell == cell && quickslotType == (window == EXTRA_INVENTORY ? QUICKSLOT_TYPE_ITEM_EXTRA : QUICKSLOT_TYPE_ITEM),
                "quickslot cell narrowed or wrong quickslot type");
            const int expectedMount = window == MOUNT_INVENTORY ? 1 : 0;
            Check(mountPackets == expectedMount && computes == expectedMount && points == expectedMount && overheads == expectedMount,
                "native mount refresh skipped/repeated");
        }
    }
}
void RemovalCallbacks() {
    for (int stage = 0; stage < 5; ++stage) {
        Reset(); Manager manager; const auto item = Item(manager); Owner(item, stage == 4 ? MOUNT_INVENTORY : INVENTORY);
        const auto recurse = [&](entt::entity) {
            Check(manager.Busy(item), "high-level callback ran outside guard");
            manager.RemoveItem(item); manager.DestroyItem(item);
            Check(g_registry.valid(item), "recursive removal destroyed guarded item");
        };
        if (stage == 0) onLog = recurse;
        if (stage == 1) onQuickslot = recurse;
        if (stage == 2) onDetach = recurse;
        if (stage == 3) onFactory = recurse;
        if (stage == 4) onCompute = recurse;
        manager.RemoveItem(item);
        Check(logs == 1 && factoryCalls == 1 && !g_registry.valid(item) && !manager.Busy(item), "high-level recursion failed");
    }
    for (int stage = 0; stage < 4; ++stage) {
        for (const bool recycle : {false, true}) {
            Reset(); Manager manager; const auto item = Item(manager); Owner(item, stage == 3 ? MOUNT_INVENTORY : INVENTORY);
            entt::entity replacement = entt::null;
            const auto mutate = [&](entt::entity) {
                if (recycle) { g_registry.destroy(item); replacement = Item(manager); }
                else Owner(item, INVENTORY, 17);
            };
            if (stage == 0) onLog = mutate;
            if (stage == 1) onQuickslot = mutate;
            if (stage == 2) onDetach = mutate;
            if (stage == 3) onCompute = mutate;
            manager.RemoveItem(item);
            Check(factoryCalls == 0 && !manager.Busy(item), "callback-transferred/recycled item was destroyed");
            if (recycle) Check(g_registry.valid(replacement) && manager.Indexed(replacement) && !manager.Indexed(item), "replacement indices lost");
            else Check(g_registry.valid(item) && manager.Indexed(item) && ItemSystem::GetItemCell(item) == 17, "transferred item lost");
            onLog = onQuickslot = onDetach = onCompute = {};
            manager.RemoveItem(recycle ? replacement : item);
        }
    }
    Reset(); Manager manager; const auto item = Item(manager); const auto owner = Owner(item, MOUNT_INVENTORY);
    onCompute = [&](entt::entity current) { Check(current == owner, "wrong mount owner"); g_registry.destroy(current); };
    manager.RemoveItem(item);
    Check(!g_registry.valid(item) && points == 0 && overheads == 0, "stale owner used after mount computation");
}
void RemovalFailures() {
    for (int scenario = 0; scenario < 4; ++scenario) {
        Reset(); Manager manager; const auto item = Item(manager); const auto owner = Owner(item);
        if (scenario == 0) g_registry.get<ecs::ItemFlags>(item).skipSave = false;
        if (scenario == 1) inventory[{owner, 5}] = Item(manager, 11, 21);
        if (scenario == 2) rejectDetach = true;
        if (scenario == 3) onLog = [](entt::entity) { throw std::runtime_error("log failure"); };
        bool caught = false;
        try { manager.RemoveItem(item); } catch (const std::runtime_error&) { caught = true; }
        Check(caught == (scenario == 3) && g_registry.valid(item) && manager.Indexed(item) && !manager.Busy(item) && factoryCalls == 0,
            "rejected high-level removal lost item/guard");
        if (scenario < 2) Check(logs == 0 && quickslots == 0 && detachCalls == 0, "preflight rejection had side effects");
        onLog = {}; rejectDetach = false; g_registry.get<ecs::ItemFlags>(item).skipSave = true;
        if (scenario == 1) {
            const auto replacement = inventory.at({owner, 5});
            Check(g_registry.valid(replacement), "foreign occupant destroyed");
            manager.RemoveItem(replacement); inventory[{owner, 5}] = item;
        }
        manager.RemoveItem(item);
        Check(!g_registry.valid(item), "high-level retry failed");
    }
    Reset(); Manager manager; const auto item = Item(manager); const auto owner = Owner(item);
    const auto replacement = Item(manager, 11, 21);
    onLog = [&](entt::entity) { inventory[{owner, 5}] = replacement; };
    manager.RemoveItem(item);
    Check(g_registry.valid(item) && g_registry.valid(replacement) && inventory.at({owner, 5}) == replacement &&
        quickslots == 0 && detachCalls == 0 && factoryCalls == 0, "log callback replacement slot/quickslot was cleared");
    onLog = {}; inventory[{owner, 5}] = item;
    manager.RemoveItem(item); manager.RemoveItem(replacement);
}
void StorageRemovalIntegration() {
    for (const uint8_t window : {SAFEBOX, MALL}) {
        for (int scenario = 0; scenario < 5; ++scenario) {
            Reset(); Manager manager; const auto owner = g_registry.create(); g_registry.emplace<Player>(owner);
            auto storage = SafeboxSystem::Open(owner, window, 3);
            std::weak_ptr<CSafebox> retired = storage;
            const auto item = Item(manager);
            Check(storage && storage->Add(0, item), "managed bank add failed");
            storage.reset(); // Manager must acquire its own callback-safe lease.
            if (scenario == 1) onDetach = [&](entt::entity) { SafeboxSystem::Close(owner, window, false); Check(!retired.expired(), "storage freed inside its Remove"); };
            if (scenario == 2) onLog = [&](entt::entity) { SafeboxSystem::Close(owner, window, false); };
            if (scenario == 3) rejectDetach = true;
            if (scenario == 4) onDetach = [&](entt::entity current) { Owner(current, INVENTORY, 17); };
            manager.RemoveItem(item);
            Check(quickslots == 0 && mountPackets == 0 && !manager.Busy(item), "bank removal ran unrelated effects/kept guard");
            if (scenario < 3) Check(!g_registry.valid(item) && !manager.Indexed(item) && factoryCalls == 1, "bank/close callback left orphan item");
            else Check(g_registry.valid(item) && manager.Indexed(item) && factoryCalls == 0, "failed/transferred bank item destroyed");
            if (scenario == 1 || scenario == 2) Check(retired.expired() && !SafeboxSystem::Get(owner, window), "closed storage retained");
            onLog = onDetach = {}; rejectDetach = false;
            if (g_registry.valid(item)) manager.RemoveItem(item);
            SafeboxSystem::Close(owner, window, false);
            Check(!g_registry.valid(item), "bank retry/close left live item");
        }
    }
}
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
        LogManager log;
        CSkillManager skills;
        DSManager dragonSouls;
        CreationQuantitiesAndIdentity(); CreationPayloads(); SkillBookSelection(); CreationFailuresAndCallbacks();
        CreationRollbackTransfers();
        NativeAndLegacy(); ReentryAndExceptions(); FactoryFailures(); RecycledAndTransferred(); SlotAndPersistenceGuards();
        NativeRemoval(); RemovalCallbacks(); RemovalFailures(); StorageRemovalIntegration();
        std::cout << "Item-manager lifecycle checks passed: " << checks << '\n'; return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
