#include "../../SRC/Server/GameServer/core/stdafx.h"
#include "../../SRC/Server/GameServer/core/utils.h"
#include "../../SRC/Server/GameServer/item/item_manager.h"
#include "../../SRC/Server/GameServer/item/item.h"
#include "../../SRC/Server/GameServer/entity/char.h"
#include "../../SRC/Server/GameServer/item/safebox.h"
#include "../../SRC/Server/GameServer/core/log.h"
#include "../../SRC/Server/GameServer/network/desc_client.h"
#include "../../SRC/Server/GameServer/core/config.h"
#include "../../SRC/Server/GameServer/core/constants.h"
#include "../../SRC/Server/GameServer/entity/char_manager.h"
#include "../../SRC/Server/GameServer/network/db.h"
#include "../../SRC/Server/GameServer/combat/skill.h"
#include "../../SRC/Server/GameServer/social/priv_manager.h"
#include "../../SRC/Server/GameServer/quest/questmanager.h"
#include "../../SRC/Server/GameServer/item/unique_item.h"
#include "../../SRC/Server/GameServer/item/refine.h"
#include "../../SRC/Server/GameServer/ecs/EntityFactory.hpp"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/InventorySystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ItemSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/NetworkSyncSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PointSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/MountSystem.hpp"
#include <Core/Logging.hpp>
#include "../../SRC/Server/GameServer/ecs/components/inventory_components.hpp"
#include <functional>
#include <iostream>
#include <stdexcept>

entt::registry g_registry;
LPCLIENT_DESC db_clientdesc = nullptr;

namespace {
int checks = 0, groundCalls = 0, detachCalls = 0, factoryCalls = 0, retirements = 0;
int logs = 0, quickslots = 0, computes = 0, points = 0, overheads = 0;
uint16_t quickslotType = 0, quickslotCell = 0;
bool rejectFactory = false;
bool rejectDetach = false;
bool creationTest = false, rejectAllocation = false, rejectCount = false;
bool rejectRune = false, rejectTimer = false;
bool highDraw = false;
uint32_t nextItemID = 1000;
int allocations = 0;
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
    void Register(entt::entity item) {
        const auto& identity = g_registry.get<ecs::ItemIdentity>(item);
        m_VIDMap[identity.vid] = item;
        if (identity.id) m_map_pkItemByID[identity.id] = item;
    }
    bool Indexed(entt::entity item) const {
        return std::any_of(m_VIDMap.begin(), m_VIDMap.end(), [=](auto& row) { return row.second == item; }) ||
            std::any_of(m_map_pkItemByID.begin(), m_map_pkItemByID.end(), [=](auto& row) { return row.second == item; });
    }
    bool Busy(entt::entity item) const { return m_itemsBeingDestroyed.contains(item); }
    ~Manager() { m_VIDMap.clear(); m_map_pkItemByID.clear(); }
};
void Reset() {
    db_clientdesc = nullptr;
    onCreation = {}; creationTest = rejectAllocation = rejectCount = false;
    rejectRune = rejectTimer = highDraw = false;
    nextItemID = 1000; allocations = 0; creationStages.clear(); availableSkills.clear();
    g_bItemCountLimit = 200;
    g_registry.clear(); inventory.clear();
    groundCalls = detachCalls = factoryCalls = retirements = 0;
    logs = quickslots = computes = points = overheads = 0;
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

// Descriptor/character seams for unrelated engine services.
// Real descriptor objects with controlled packet sinks; no fabricated pointers
// or live networking/crypto/parser services in persistence regression tests.
DESC::DESC() { m_socketHandle = 1; m_entity = entt::null; m_accountTable = {}; }
DESC::~DESC() {}
void DESC::Destroy() { Unexpected(); }
void DESC::SetPhase(int) { Unexpected(); }
void DESC::OnInput() { Unexpected(); }
void DESC::OnPeerGone() { Unexpected(); }
void DESC::OnBytesWritten(std::size_t) { Unexpected(); }
CLIENT_DESC::CLIENT_DESC() {}
CLIENT_DESC::~CLIENT_DESC() {}
void CLIENT_DESC::Destroy() { Unexpected(); }
void CLIENT_DESC::SetPhase(int) { Unexpected(); }
void CLIENT_DESC::OnInput() { Unexpected(); }
void CLIENT_DESC::OnHeartbeatSend() { Unexpected(); }
CInputProcessor::CInputProcessor() {}
bool CInputProcessor::Process(DESC*, const void*, int, int&) { Unexpected(); }
void CInputProcessor::Handshake(DESC*, const char*) { Unexpected(); }
CInputHandshake::CInputHandshake() {}
CInputHandshake::~CInputHandshake() {}
int CInputHandshake::Analyze(DESC*, uint8_t, const char*) { Unexpected(); }
int CInputLogin::Analyze(DESC*, uint8_t, const char*) { Unexpected(); }
int CInputMain::Analyze(DESC*, uint8_t, const char*) { Unexpected(); }
int CInputDead::Analyze(DESC*, uint8_t, const char*) { Unexpected(); }
int CInputDB::Analyze(DESC*, uint8_t, const char*) { Unexpected(); }
bool CInputDB::Process(DESC*, const void*, int, int&) { Unexpected(); }
CInputP2P::CInputP2P() {}
int CInputP2P::Analyze(DESC*, uint8_t, const char*) { Unexpected(); }
CPacketInfo::CPacketInfo() : m_pCurrentPacket(nullptr), m_dwStartTime(0) {}
CPacketInfo::~CPacketInfo() {}
CPacketInfoCG::CPacketInfoCG() {}
CPacketInfoGG::CPacketInfoGG() {}
CPacketInfoCG::~CPacketInfoCG() {}
CPacketInfoGG::~CPacketInfoGG() {}
void intrusive_ptr_add_ref(EVENT* event) { ++event->ref_count; }
void intrusive_ptr_release(EVENT* event) { if (!--event->ref_count) delete event; }
void CLIENT_DESC::DBPacket(uint8_t, uint32_t, const void*, uint32_t) { Unexpected(); }
void DESC::Packet(const void*, int) { Unexpected(); }
CSemaphore::CSemaphore() = default;
CSemaphore::~CSemaphore() = default;
CAsyncSQL::CAsyncSQL() : m_queueLane(0) {}
CAsyncSQL::~CAsyncSQL() = default;
CPoly::CPoly() = default;
CPoly::~CPoly() = default;
CSkillManager::CSkillManager() = default;
CSkillManager::~CSkillManager() = default;
LogManager::LogManager() : m_bIsConnect(false) {}
LogManager::~LogManager() = default;
// Supply the key virtual functions of service doubles on the Itanium ABI too.
// singleton's release guard references their RTTI even when no live service
// is constructed. Linking the real managers here would start unrelated systems.
CHARACTER_MANAGER::~CHARACTER_MANAGER() = default;
DBManager::~DBManager() = default;
CRefineManager::~CRefineManager() = default;
quest::CQuestManager::~CQuestManager() = default;
quest::NPC::~NPC() = default;

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
        return storage != entt::null ? SafeboxSystem::GetItem(storage, position.cell) : entt::null;
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
    registry.destroy(item); ++retirements;
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
int MAX(int, int) { Unexpected(); }
int MINMAX(int, int, int) { Unexpected(); }
int number_ex(int low, int high, const char*, int) { Check(creationTest && low <= high, "invalid creation RNG"); return highDraw ? high : low; }
time_t get_global_time() { Check(creationTest, "unexpected clock"); return 10000; }
const char* get_table_postfix() { Unexpected(); }
bool MountSystem::IsHorseRiding(entt::entity) { return false; }
int ecs::PointSystem::GetLevel(entt::entity) { Unexpected(); }
double ecs::PointSystem::GetPrecise(entt::entity, uint8_t) { Unexpected(); }
namespace ecs::PlayerRuntime {
LPDESC GetDesc(entt::entity owner) { Check(g_registry.valid(owner), "stale descriptor lookup"); return nullptr; }
uint32_t GetAccountID(entt::entity owner) { Check(g_registry.valid(owner), "stale account lookup"); return 0; }
uint8_t GetEmpire(entt::entity) { Unexpected(); }
bool IsPC(entt::entity owner) { return g_registry.valid(owner) && g_registry.all_of<Player>(owner); }
bool IsStone(entt::entity) { Unexpected(); }
uint8_t GetCharType(entt::entity) { Unexpected(); }
uint8_t GetMobRank(entt::entity) { Unexpected(); }
int64_t GetPremiumRemainSeconds(entt::entity, uint8_t) { Unexpected(); }
}
void NetworkSyncSystem::PointsPacket(entt::entity owner) { Check(g_registry.valid(owner), "stale points packet"); ++points; }
void ecs::PointSystem::Compute(entt::entity owner) {
    Check(g_registry.valid(owner), "stale point computation"); ++computes; if (onCompute) onCompute(owner);
}
entt::entity MountSystem::GetMountInventoryItem(entt::entity owner, uint32_t cell) {
    return ItemSystem::GetItem(owner, TItemPos(MOUNT_INVENTORY, cell));
}
void MountSystem::UpdateMountCountOverheadToViewers(entt::entity owner) { Check(g_registry.valid(owner), "stale overhead"); ++overheads; }
namespace ItemSystem {
bool IsEquipUniqueItem(entt::entity, uint32_t) { Unexpected(); }
bool IsEquipUniqueGroup(entt::entity, uint32_t) { Unexpected(); }
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


uint32_t GetItemSocket(entt::entity item, int index) {
    const auto* sockets = g_registry.try_get<ecs::ItemSockets>(item);
    return sockets ? sockets->sockets[index] : 0;
}
TPlayerItemAttribute GetItemAttribute(entt::entity item, int index) {
    const auto* attrs = g_registry.try_get<ecs::ItemAttributes>(item);
    return attrs ? attrs->attrs[index] : TPlayerItemAttribute{};
}
bool SetItemSocket(entt::entity, int, uint32_t, bool) { Unexpected(); }
bool SetItemForceAttributeEcs(entt::entity item, int index, uint8_t type, double value) {
    g_registry.get<ecs::ItemAttributes>(item).attrs[index] = {type, value}; CreationStage("attribute", item); return true;
}
bool ApplyItemAddon(entt::entity item, int) { CreationStage("addon", item); return true; }
bool IsItemConsumptionPending(entt::entity item) { return g_registry.valid(item) && g_registry.get<ecs::ItemCount>(item).count < 0; }
bool SetItemSkipSave(entt::entity item, bool flag) { g_registry.get<ecs::ItemFlags>(item).skipSave = flag; return true; }
uint8_t GetItemSize(entt::entity) { return 1; }
uint32_t GetItemAntiFlags(entt::entity) { return 0; }
int16_t GetItemLockedAttributeIndex(entt::entity) { return -1; }
bool SetItemWindow(entt::entity item, uint8_t window) { g_registry.get<ecs::ItemLocation>(item).window = window; return true; }
bool SetItemCell(entt::entity item, entt::entity owner, uint16_t cell) {
    g_registry.get<ecs::ItemLocation>(item).cell = cell; SetItemOwnerEntity(item, owner); return true;
}
bool RemoveItemEcs(entt::entity item) { return InventorySystem::RemoveFromCharacter(item) == item; }
bool IsItemExchanging(entt::entity) { return false; }
bool IsItemLocked(entt::entity) { return false; }
bool ConsumeItemEcs(entt::entity, uint32_t) { Unexpected(); }
bool AddItemCountEcs(entt::entity, int) { Unexpected(); }
bool AlterItemToMagicItem(entt::entity item) { CreationStage("magic", item); return true; }
bool IsItemEquipped(entt::entity) { Unexpected(); }


bool InitializeRuneItem(entt::entity item) { CreationStage("rune", item); return !rejectRune; }
}
int CHARACTER_MANAGER::GetMobItemRate(entt::entity) { Unexpected(); }
const event_struct_* CHARACTER_MANAGER::CheckEventIsActive(uint8_t, uint8_t) { Unexpected(); }
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
int64_t quest::CQuestManager::GetEventFlag(const std::string&) { Unexpected(); }
void quest::CQuestManager::RegisterNPCVnum(uint32_t) { Unexpected(); }
bool ItemSystem::SetItemCountEcs(entt::entity item, uint32_t count) {
    if (rejectCount) return false;
    g_registry.get<ecs::ItemCount>(item).count = static_cast<int>(count); CreationStage("count", item); return true;
}
uint32_t ITEM_MANAGER::GetNewID() { Check(creationTest, "unexpected ID allocation"); return nextItemID++; }
entt::entity EntityFactory::CreateItemEntity(entt::registry& registry, const TItemTable* proto,
    uint32_t vnum, uint32_t id, uint32_t vid, uint32_t mask) {
    Check(creationTest, "unexpected entity factory");
    if (rejectAllocation) return entt::null;
    ++allocations;
    const auto item = registry.create();
    registry.emplace<CreationProto>(item, *proto);
    auto& identity = registry.emplace<ecs::ItemIdentity>(item);
    identity.id = id; identity.vid = vid;
    identity.vnum = mask ? mask : vnum; identity.originalVnum = vnum; identity.maskVnum = mask;
    registry.emplace<ecs::ItemCount>(item);
    registry.emplace<ecs::ItemOwner>(item);
    registry.emplace<ecs::ItemLocation>(item, ecs::ItemLocation {RESERVED_WINDOW, 0});
    registry.emplace<ecs::ItemEquipped>(item);
    registry.emplace<ecs::ItemFlags>(item).flags = proto->dwFlags;
    registry.emplace<ecs::ItemSockets>(item); registry.emplace<ecs::ItemAttributes>(item);
    return item;
}
bool ItemSystem::RefreshItemPrototype(entt::entity item, const TItemTable* proto) {
    Check(g_registry.valid(item), "prototype reload used stale entity");
    g_registry.emplace_or_replace<CreationProto>(item, proto ? *proto : TItemTable{});
    return true;
}
const TRefineTable* CRefineManager::GetRefineRecipe(uint32_t) { Unexpected(); }

namespace {
void SkillBookSelection() {
    Reset(); creationTest = true;
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
void NativeRemoval() {

    for (const uint8_t window : {INVENTORY, EQUIPMENT, EXTRA_INVENTORY, SWITCHBOT, MOUNT_INVENTORY}) {
        Reset(); Manager manager; const auto item = Item(manager);
        const uint16_t cell = window == EQUIPMENT ? INVENTORY_MAX_NUM + WEAR_BODY : 300;
        const auto owner = Owner(item, window, cell);
        manager.RemoveItem(item, "TEST_REMOVE");
        Check(!g_registry.valid(item) && !manager.Indexed(item) && !manager.Busy(item), "high-level removal left item published");
        Check(logs == 1 && detachCalls == 1 && factoryCalls == 1, "high-level cleanup repeated/skipped");
        Check(!inventory.contains({owner, cell}), "high-level cleanup left owner slot");
        const bool hasQuickslot = window == INVENTORY || window == EXTRA_INVENTORY;
        Check(quickslots == (hasQuickslot ? 1 : 0), "foreign window quickslot was cleared");
        if (hasQuickslot) Check(quickslotCell == cell && quickslotType == (window == EXTRA_INVENTORY ? QUICKSLOT_TYPE_ITEM_EXTRA : QUICKSLOT_TYPE_ITEM),
            "quickslot cell narrowed or wrong quickslot type");
        const int expectedMount = window == MOUNT_INVENTORY ? 1 : 0;
        Check(computes == expectedMount && points == expectedMount && overheads == expectedMount,
            "native mount refresh skipped/repeated");
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
    for (int scenario = 1; scenario < 4; ++scenario) {
        Reset(); Manager manager; const auto item = Item(manager); const auto owner = Owner(item);
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
            const auto storage = SafeboxSystem::Open(owner, window, 3);
            const auto item = Item(manager);
            Check(storage != entt::null && SafeboxSystem::Add(storage, 0, item), "managed bank add failed");
            // The storage entry points revalidate the entity after callbacks,
            // so the manager does not need a separate container lease.
            if (scenario == 1) onDetach = [&](entt::entity) { SafeboxSystem::Close(owner, window); };
            if (scenario == 2) onLog = [&](entt::entity) { SafeboxSystem::Close(owner, window); };
            if (scenario == 3) rejectDetach = true;
            if (scenario == 4) onDetach = [&](entt::entity current) { Owner(current, INVENTORY, 17); };
            manager.RemoveItem(item);
            Check(quickslots == 0 && !manager.Busy(item), "bank removal ran unrelated effects/kept guard");
            if (scenario < 3) Check(!g_registry.valid(item) && !manager.Indexed(item) && factoryCalls == 1, "bank/close callback left orphan item");
            else Check(g_registry.valid(item) && manager.Indexed(item) && factoryCalls == 0, "failed/transferred bank item destroyed");
            if (scenario == 1 || scenario == 2) Check(!g_registry.valid(storage) && SafeboxSystem::Get(owner, window) == entt::null, "closed storage retained");
            onLog = onDetach = {}; rejectDetach = false;
            if (g_registry.valid(item)) manager.RemoveItem(item);
            SafeboxSystem::Close(owner, window);
            Check(!g_registry.valid(item), "bank retry/close left live item");
        }
    }
}
void NativeLifecycle() {

    Reset(); Manager manager;
    const auto item = Item(manager);
    const auto owner = Owner(item); // No CHARACTER; no player ID either.
    onFactory = [&](entt::entity current) {
        Check(manager.Busy(current) && !manager.Indexed(current), "factory saw published manager references");
        manager.DestroyItem(current);
    };
    manager.DestroyItem(item);
    Check(!g_registry.valid(item) && !manager.Indexed(item) && !manager.Busy(item), "item not completely retired");
    Check(groundCalls == 1 && detachCalls == 1 && factoryCalls == 1, "cleanup repeated or skipped");
    Check(!inventory.contains({owner, 5}), "zero-PID owner slot not cleared");
    manager.DestroyItem(item);
    Check(factoryCalls == 1, "repeated destruction entered factory");

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
        rejectFactory = true;
        onFactory = [&](entt::entity current) {
            Check(!manager.Indexed(current) && manager.Busy(current), "failed factory saw published item");
            if (throws) throw std::runtime_error("factory failure");
        };
        bool caught = false;
        try { manager.DestroyItem(item); } catch (const std::runtime_error&) { caught = true; }
        Check(caught == throws && g_registry.valid(item) && manager.Indexed(item) && !manager.Busy(item) && retirements == 0,
            "failed factory lost manager identity or freed allocation");
        rejectFactory = false; onFactory = {}; manager.DestroyItem(item);
        Check(!g_registry.valid(item) && !manager.Indexed(item) && retirements == 1, "factory retry failed");
    }
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
    Check(!g_registry.valid(persisted) && !manager.Indexed(persisted) && groundCalls == 1 && !manager.Busy(persisted),
        "retiring an identified item required a database connection");
    const auto owned = Item(manager, 11, 21);
    const auto owner = Owner(owned);
    g_registry.get<ecs::ItemFlags>(owned).skipSave = false;
    manager.RemoveItem(owned);
    Check(!g_registry.valid(owned) && !manager.Indexed(owned) && !inventory.contains({owner, 5}),
        "removing an identified item required a database connection");
    const auto transient = Item(manager, 0, 99);
    g_registry.get<ecs::ItemFlags>(transient).skipSave = false;
    manager.DestroyItem(transient);
    Check(!g_registry.valid(transient), "ID-less item incorrectly required DB connection");
    const auto nonItem = g_registry.create();
    manager.DestroyItem(nonItem); manager.DestroyItem(entt::null);
    Check(g_registry.valid(nonItem), "non-item entity destroyed");
}

void DuplicateLoadRetirement() {
    {
        Reset(); Manager manager; const auto item = Item(manager);
        const auto owner = Owner(item, EQUIPMENT, INVENTORY_MAX_NUM + WEAR_BODY);
        g_registry.get<ecs::ItemFlags>(item).skipSave = false;
        onDetach = [&](entt::entity current) {
            Check(manager.Busy(current) && ItemSystem::GetItemSkipSave(current),
                "duplicate detached outside manager guard or could delete DB row");
        };
        Check(db_clientdesc == nullptr && ItemSystem::DestroyLoadedDuplicateItem(item),
            "duplicate retirement required legacy object or DB connection");
        Check(!g_registry.valid(item) && !manager.Indexed(item) && !manager.Busy(item) &&
            !inventory.contains({owner, INVENTORY_MAX_NUM + WEAR_BODY}) &&
            groundCalls == 1 && detachCalls == 1 && factoryCalls == 1,
            "duplicate retirement skipped/repeated entity, slot, index or allocation cleanup");
        Check(!ItemSystem::DestroyLoadedDuplicateItem(item) && factoryCalls == 1,
            "already retired duplicate was destroyed again");
    }

    for (int invalid = 0; invalid < 4; ++invalid) {
        Reset(); Manager manager; const auto item = Item(manager);
        if (invalid == 0) g_registry.remove<ecs::ItemIdentity>(item);
        if (invalid == 1) g_registry.remove<ecs::ItemFlags>(item);
        if (invalid == 2) g_registry.remove<ecs::ItemOwner>(item);
        if (invalid == 3) g_registry.remove<ecs::ItemLocation>(item);
        Check(!ItemSystem::DestroyLoadedDuplicateItem(item) &&
            !ItemSystem::DestroyLoadedDuplicateItem(entt::null) && g_registry.valid(item) &&
            groundCalls == 0 && factoryCalls == 0, "incomplete duplicate entered destruction");
    }
}

void DuplicateLoadFailuresAndReentry() {
    for (const bool skipSave : {false, true}) for (int failure = 0; failure < 5; ++failure) {
        Reset(); Manager manager; const auto item = Item(manager); Owner(item);
        g_registry.get<ecs::ItemFlags>(item).skipSave = skipSave;
        if (failure == 0) rejectDetach = true;
        if (failure == 1) rejectFactory = true;
        const auto fail = [](entt::entity) { throw std::runtime_error("duplicate callback failure"); };
        if (failure == 2) onGround = fail;
        if (failure == 3) onDetach = fail;
        if (failure == 4) onFactory = fail;
        bool threw = false, destroyed = false;
        try { destroyed = ItemSystem::DestroyLoadedDuplicateItem(item); }
        catch (const std::runtime_error&) { threw = true; }
        Check(!destroyed && threw == (failure >= 2) && g_registry.valid(item) && manager.Indexed(item) &&
            !manager.Busy(item) && ItemSystem::GetItemSkipSave(item) == skipSave,
            "failed duplicate cleanup reported success or lost save policy/index/guard");
        rejectDetach = rejectFactory = false; onGround = onDetach = onFactory = {};
        Check(ItemSystem::DestroyLoadedDuplicateItem(item) && !manager.Indexed(item),
            "duplicate cleanup could not retry after failure");
    }
    for (int stage = 0; stage < 3; ++stage) {
        Reset(); Manager manager; const auto item = Item(manager); Owner(item);
        g_registry.get<ecs::ItemFlags>(item).skipSave = false;
        const auto recurse = [&](entt::entity current) {
            Check(manager.Busy(current) && !ItemSystem::DestroyLoadedDuplicateItem(current) &&
                g_registry.valid(current) && ItemSystem::GetItemSkipSave(current),
                "recursive duplicate bypassed manager guard or changed outer save policy");
        };
        if (stage == 0) onGround = recurse;
        if (stage == 1) onDetach = recurse;
        if (stage == 2) onFactory = recurse;
        Check(ItemSystem::DestroyLoadedDuplicateItem(item) && groundCalls == 1 &&
            detachCalls == 1 && factoryCalls == 1, "reentrant duplicate cleanup ran more than once");
    }
}

void DuplicateLoadTransfersAndGenerations() {
    for (int stage = 0; stage < 2; ++stage) for (const bool staleOwner : {false, true}) {
        Reset(); Manager manager; const auto item = Item(manager); Owner(item);
        g_registry.get<ecs::ItemFlags>(item).skipSave = false;
        entt::entity newOwner = entt::null;
        const auto transfer = [&](entt::entity current) {
            newOwner = Owner(current, INVENTORY, 7);
            g_registry.get<ecs::ItemFlags>(current).isLocked = true;
            if (staleOwner) g_registry.destroy(newOwner);
        };
        if (stage == 0) onGround = transfer; else onDetach = transfer;
        Check(!ItemSystem::DestroyLoadedDuplicateItem(item) && manager.Indexed(item) &&
            g_registry.valid(item) && factoryCalls == 0 &&
            g_registry.get<ecs::ItemOwner>(item).owner == newOwner &&
            inventory.at({newOwner, 7}) == item && !ItemSystem::GetItemSkipSave(item) &&
            g_registry.get<ecs::ItemFlags>(item).isLocked,
            "duplicate cleanup destroyed transferred state or left saving disabled");
        onGround = onDetach = {}; ItemSystem::SetItemOwnerEntity(item, entt::null);
        Check(ItemSystem::DestroyLoadedDuplicateItem(item), "transferred duplicate could not be retired later");
    }
    for (int stage = 0; stage < 2; ++stage) {
        Reset(); Manager manager; const auto item = Item(manager); Owner(item);
        g_registry.get<ecs::ItemFlags>(item).skipSave = false;
        entt::entity replacement = entt::null;
        const auto recycle = [&](entt::entity current) {
            g_registry.destroy(current); replacement = Item(manager);
            Check(current != replacement && entt::to_entity(current) == entt::to_entity(replacement),
                "duplicate test did not recycle entity generation");
        };
        if (stage == 0) onGround = recycle; else onDetach = recycle;
        Check(ItemSystem::DestroyLoadedDuplicateItem(item) && g_registry.valid(replacement) &&
            manager.Indexed(replacement) && !manager.Indexed(item) && factoryCalls == 0 &&
            ItemSystem::GetItemSkipSave(replacement),
            "retired generation removed replacement identity or overwrote its save policy");
        onGround = onDetach = {};
        Check(ItemSystem::DestroyLoadedDuplicateItem(replacement), "replacement cleanup failed");
    }
    for (const bool staleOwner : {false, true}) {
        Reset(); Manager manager; const auto item = Item(manager); auto owner = Owner(item);
        g_registry.get<ecs::ItemFlags>(item).skipSave = false;
        if (staleOwner) {
            g_registry.destroy(owner); owner = g_registry.create();
            g_registry.emplace<Player>(owner);
        }
        const auto occupant = Item(manager, 11, 21); inventory[{owner, 5}] = occupant;
        Check(ItemSystem::DestroyLoadedDuplicateItem(item) && detachCalls == 0 &&
            g_registry.valid(occupant) && manager.Indexed(occupant) && inventory.at({owner, 5}) == occupant,
            "duplicate cleanup cleared a foreign occupant or resolved a recycled owner");
        Check(ItemSystem::DestroyLoadedDuplicateItem(occupant), "foreign occupant cleanup failed");
    }
}

void NativeShutdownAndReload() {
    {
        Reset(); Manager manager;
        const auto first = Item(manager, 11, 21), second = Item(manager, 12, 22), gold = Item(manager, 0, 23);
        Owner(first); Owner(second, MOUNT_INVENTORY, 3);
        g_registry.get<ecs::ItemFlags>(first).skipSave = false;
        onGround = [&](entt::entity item) {
            if (item != gold && g_registry.valid(gold)) manager.DestroyItem(gold);
        };
        manager.Destroy();
        Check(!g_registry.valid(first) && !g_registry.valid(second) && !g_registry.valid(gold) &&
            !manager.Indexed(first) && !manager.Indexed(second) && !manager.Indexed(gold) &&
            factoryCalls == 3, "shutdown skipped native items or deleted DB rows");
        manager.Destroy();
        Check(factoryCalls == 3, "repeated shutdown destroyed native items twice");
    }
    {
        Reset(); Manager manager; const auto item = Item(manager); Owner(item);
        g_registry.get<ecs::ItemFlags>(item).skipSave = false; rejectDetach = true;
        manager.Destroy();
        Check(g_registry.valid(item) && manager.Indexed(item) && !ItemSystem::GetItemSkipSave(item),
            "failed shutdown dropped a live item or lost its save policy");
        rejectDetach = false; manager.Destroy();
        Check(!g_registry.valid(item), "shutdown could not retry rejected detachment");
    }
    {
        Reset(); Manager manager;
        const auto item = Item(manager), ranged = Item(manager, 11, 21);
        g_registry.get<ecs::ItemIdentity>(item).originalVnum = 100;
        g_registry.get<ecs::ItemIdentity>(ranged).originalVnum = 105;
        TItemTable proto {}; proto.dwVnum = 100; proto.dwVnumRange = 10; proto.bType = ITEM_WEAPON; proto.bSize = 1;
        Check(manager.Initialize(&proto, 1) && Proto(item).bType == ITEM_WEAPON && Proto(ranged).bSize == 1 &&
            manager.GetTable(105) != nullptr, "prototype load did not rebind native/ranged items");
        proto.dwVnumRange = 0; proto.bSize = 2;
        Check(manager.Initialize(&proto, 1) && Proto(item).bSize == 2 && Proto(ranged).bSize == 0 &&
            manager.GetTable(105) == nullptr && manager.GetVIDMap().at(100).bSize == 2,
            "reload retained stale range pointers, copied tables or item metadata");
        Check(manager.Initialize(nullptr, 0) && Proto(item).bSize == 0 && manager.GetTable(100) == nullptr &&
            manager.GetVIDMap().empty(), "empty reload retained obsolete prototype pointers");
        Check(!manager.Initialize(nullptr, 1) && !manager.Initialize(&proto, -1), "invalid reload input accepted");
    }
}

void RetiredPrototypeReload() {
    Reset(); Manager manager;
    std::vector<TItemTable> rows(5);
    for (size_t i = 0; i < rows.size(); ++i) {
        rows[i].dwVnum = uint32_t(100 + i); rows[i].bSize = 1;
        rows[i].bType = i < 3 ? uint8_t(29 + i) : uint8_t(ITEM_WEAPON);
    }
#ifdef ENABLE_SOUL_SYSTEM
    rows.back().bType = ITEM_SOUL;
#endif
    Check(manager.Initialize(rows.data(), int(rows.size())), "mixed prototype reload rejected");
    for (uint32_t vnum : {100u, 101u, 102u})
        Check(manager.GetTable(vnum) == nullptr && !manager.GetVIDMap().contains(vnum),
            "retired prototype remained in the catalogue");
    Check(manager.GetTable(103) && manager.GetTable(104) && manager.GetVIDMap().size() == 2,
        "retirement filtered unrelated prototypes or broke the index");
}

}

int main() {
    try {
        LogManager log;
        CSkillManager skills;

        SkillBookSelection();
        NativeLifecycle(); ReentryAndExceptions(); FactoryFailures(); RecycledAndTransferred(); SlotAndPersistenceGuards();
        NativeRemoval(); RemovalCallbacks(); RemovalFailures(); StorageRemovalIntegration();
        DuplicateLoadRetirement(); DuplicateLoadFailuresAndReentry(); DuplicateLoadTransfersAndGenerations();
        NativeShutdownAndReload(); RetiredPrototypeReload();
        std::cout << "Item-manager lifecycle checks passed: " << checks << '\n'; return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
