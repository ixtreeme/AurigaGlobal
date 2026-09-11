#include "../../SRC/Server/GameServer/stdafx.h"
#include "../../SRC/Server/GameServer/sectree_manager.h"
#include "../../SRC/Server/GameServer/char_manager.h"
#include "../../SRC/Server/GameServer/desc_manager.h"
#include "../../SRC/Server/GameServer/ecs/SpatialHelpers.hpp"
#include "../../SRC/Server/GameServer/ecs/EventDispatcher.hpp"
#include "../../SRC/Server/GameServer/ecs/events.hpp"
#include "../../SRC/Server/GameServer/ecs/services/SpatialService.hpp"
#include "../../SRC/Server/GameServer/ecs/services/VisibilityService.hpp"
#include "../../SRC/Server/GameServer/ecs/services/EntityNetworkDispatch.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/VisibilitySystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/MovementSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/MountSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/AIHelpers.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ViewSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/CombatSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SocialSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/AISystem.hpp"
#include "../../SRC/Server/GameServer/ecs/components/movement_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/dirty_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/character_runtime_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/combat_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/status_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/ai_components.hpp"
#include "../../SRC/Server/GameServer/packet.h"
#include "../../SRC/Server/GameServer/questmanager.h"
#include "../../SRC/Server/GameServer/dungeon.h"
#include "../../SRC/Server/GameServer/party.h"
#include "../../SRC/Server/GameServer/motion.h"
#include "../../SRC/Server/GameServer/ecs/systems/AffectSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PointSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ItemSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/components/item_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/identity_components.hpp"
#include "../../SRC/Server/GameServer/item_manager.h"
#include "../../SRC/Server/GameServer/ecs/ItemRegistry.hpp"
#include "../../SRC/Server/GameServer/ecs/CBuildingRegistry.hpp"
#include "../../SRC/Server/GameServer/ecs/OfflineShopEntityRegistry.hpp"
#include <Base/attribute.h>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <limits>
#include "../../SRC/Server/GameServer/mount_inventory_helper.h"
#include "../../SRC/Server/GameServer/map_location.h"
#include "../../SRC/Server/GameServer/battle_pass.h"
#include "../../SRC/Server/GameServer/ecs/EcsDiagnostics.hpp"
#include "../../SRC/Server/GameServer/desc.h"
#include "../../SRC/Server/GameServer/log.h"
#include "../../SRC/Server/GameServer/p2p.h"
#include "../../SRC/Server/GameServer/new_switchbot.h"

entt::registry g_registry;
entt::dispatcher g_dispatcher;
int VIEW_RANGE = 5000;
int VIEW_BONUS_RANGE = 500;
namespace {
int checks = 0;
void Check(bool good, const char* why) { ++checks; if (!good) throw std::runtime_error(why); }
[[noreturn]] void Unexpected() { throw std::runtime_error("unexpected legacy spatial service"); }
std::map<std::tuple<int, int, int>, SECTREE*> sectors;
std::unordered_set<entt::entity> awake;
struct Packet { bool add; entt::entity source, viewer; };
std::vector<Packet> packets;
std::function<void(Packet)> onPacket;
std::function<void(entt::entity)> onRetire;
int retired = 0;
std::vector<TPacketGCMove> movementPackets;
std::vector<packet_motion> animationPackets;
std::function<void(entt::entity)> onAnimation;
std::vector<std::pair<entt::entity, ecs::AIFSMState>> transitions;
struct MotionSettings {
    bool attached = false, polymorphed = false;
    uint32_t race = 0, mount = 0;
    int stamina = 100;
    int64_t movePoint = 100;
    entt::entity weapon = entt::null;
};
std::unordered_map<entt::entity, MotionSettings> motionSettings;
std::map<entt::entity, TItemTable> weaponProtos;
std::map<std::pair<uint32_t, uint32_t>, const CMotion*> motions;
std::vector<std::pair<uint32_t, uint32_t>> motionRequests;
// Only entities a test places have a position; every other entity keeps the
// "must not be asked" guarantee the plain Unexpected() doubles gave.
struct Placement { int32_t x, y, mapIndex; };
std::map<entt::entity, Placement> placements;
struct TestMotion : CMotion {
    TestMotion(float duration, float distance) { m_fDuration = duration; m_vec3Accumulation = {0, -distance, 0}; }
};

struct MapFixture {
    int index;
    SECTREE_MAP map;
    explicit MapFixture(int id = 1) : index(id) {
        for (int x = 0; x < 4; ++x) for (int y = 0; y < 2; ++y) {
            auto* tree = new SECTREE;
            map.Add(uint32_t(x + y * 4), tree);
            sectors[{id, x, y}] = tree;
        }
        map.Build();
    }
    ~MapFixture() {
        // Drain while lookup and every neighbor are live. The map owns deletion.
        auto drain = [&](entt::entity e) { ecs::SpatialService::RemoveEntity(g_registry, e); };
        map.for_each(drain);
        for (auto it = sectors.begin(); it != sectors.end();)
            if (std::get<0>(it->first) == index) it = sectors.erase(it); else ++it;
    }
    SECTREE* At(int x = 100, int y = 100) { return SECTREE_MANAGER::instance().Get(index, x, y); }
};

entt::entity Entity(ecs::SpatialKind kind = ecs::SpatialKind::Item) {
    const auto e = g_registry.create();
    g_registry.emplace<ecs::SpatialKindTag>(e, kind);
    g_registry.emplace<ecs::VIDComponent>(e, uint32_t(entt::to_integral(e) + 1));
    if (kind == ecs::SpatialKind::Item) {
        g_registry.emplace<ecs::ItemIdentity>(e);
        g_registry.emplace<ecs::ItemLocation>(e, ecs::ItemLocation {GROUND, 0});
    }
    if (kind == ecs::SpatialKind::Character) {
        g_registry.emplace<ecs::CharacterType>(e, uint8_t(CHAR_TYPE_PC));
        g_registry.emplace<ecs::TagPC>(e);
    }
    return e;
}
bool Spawn(entt::entity e, int map = 1, int x = 100, int y = 100) {
    if (!ecs::SpatialService::InsertEntity(g_registry, e, map, x, y, 0)) return false;
    ecs::SpatialService::UpdateSectree(g_registry, e);
    return true;
}
bool Visible(entt::entity source, entt::entity viewer) {
    return g_registry.valid(viewer) && g_registry.all_of<ecs::ViewMap>(viewer) &&
        g_registry.get<ecs::ViewMap>(viewer).visible.contains(source);
}
void Reset() {
    onPacket = {}; onRetire = {}; packets.clear(); awake.clear(); retired = 0;
    movementPackets.clear(); animationPackets.clear(); onAnimation = {};
    transitions.clear(); motionSettings.clear(); weaponProtos.clear();
    motions.clear(); motionRequests.clear(); placements.clear(); g_registry.clear();
}
struct Callback {
    std::function<void(entt::registry&, entt::entity)> fn;
    void Run(entt::registry& reg, entt::entity e) { fn(reg, e); }
};
}

std::shared_ptr<spdlog::logger> logging::GetLogger() {
    static auto log = std::make_shared<spdlog::logger>("spatial-test"); return log;
}
std::shared_ptr<spdlog::logger> logging::GetErrorLogger() { return logging::GetLogger(); }
SECTREE_MANAGER::SECTREE_MANAGER() {}
SECTREE_MANAGER::~SECTREE_MANAGER() {}
SECTREE* SECTREE_MANAGER::Get(int32_t map, int32_t x, int32_t y) {
    if (x < 0 || y < 0) return nullptr;
    auto it = sectors.find({map, x / SECTREE_SIZE, y / SECTREE_SIZE});
    return it == sectors.end() ? nullptr : it->second;
}
// Grid loading is a fixture, not a second production spatial implementation.
SECTREE_MAP::SECTREE_MAP() { m_setting = {}; }
SECTREE_MAP::~SECTREE_MAP() {
    for (auto [id, tree] : map_) tree->Destroy();
    for (auto [id, tree] : map_) delete tree;
}
void SECTREE_MAP::Build() {
    for (auto [id, tree] : map_) {
        tree->m_neighbor_list.clear();
        for (auto [otherID, other] : map_)
            if (std::abs(int(id % 4) - int(otherID % 4)) <= 1 &&
                std::abs(int(id / 4) - int(otherID / 4)) <= 1)
                tree->m_neighbor_list.push_back(other);
    }
}
CAttribute::~CAttribute() {}
uint32_t CAttribute::Get(uint32_t, uint32_t) { Unexpected(); }
void CAttribute::Set(uint32_t, uint32_t, uint32_t) { Unexpected(); }
void CAttribute::Remove(uint32_t, uint32_t, uint32_t) { Unexpected(); }
CHARACTER_MANAGER::CHARACTER_MANAGER() {}
CHARACTER_MANAGER::~CHARACTER_MANAGER() {}
bool CHARACTER_MANAGER::AddToStateList(entt::entity e) { Check(g_registry.valid(e), "wake stale NPC"); return awake.insert(e).second; }
void CHARACTER_MANAGER::RemoveFromStateList(entt::entity e) { awake.erase(e); }
void CHARACTER_MANAGER::DestroyCharacter(entt::entity e) { g_registry.destroy(e); }
DESC_MANAGER::DESC_MANAGER() {}
DESC_MANAGER::~DESC_MANAGER() {}
void DESC_MANAGER::DestroyDesc(LPDESC, bool) { Unexpected(); }
DESC* ecs::PlayerRuntime::GetDesc(entt::entity e) {
    // Boolean-only attachment seam: no transport object is dereferenced here.
    return motionSettings[e].attached ? reinterpret_cast<DESC*>(uintptr_t(1)) : nullptr;
}
// Gameplay leaf services are isolated here; movement, packet encoding,
// SetPosition, sectree relocation and visibility run their production code.
uint32_t get_dword_time() { return 123456; }
bool ecs::PlayerRuntime::IsPC(entt::entity e) { return g_registry.all_of<ecs::TagPC>(e); }
bool ecs::PlayerRuntime::IsStone(entt::entity e) { return g_registry.all_of<ecs::StoneAITag>(e); }
void ecs::PlayerRuntime::MonsterLog(entt::entity, const char*) {}
void ecs::PlayerRuntime::CancelCharEvent(entt::entity e, CharEvent) { Check(g_registry.valid(e), "cancel event on stale entity"); }

// CHARACTER::Show moved into MovementSystem.cpp too, and brought the battle
// pass, the leaderboards and the invariant logger with it. Nothing here calls
// Show - it needs a sectree table, a descriptor and a character manager - so
// every one of these must stay unreached.
UINT g_start_map[4] = {};
void BroadcastNotice(const char*, bool) { Unexpected(); }
std::string ecs::diag::Describe(entt::entity) { Unexpected(); }
int32_t ecs::PlayerRuntime::GetZ(entt::entity) { Unexpected(); }
bool ecs::PlayerRuntime::IsNPC(entt::entity) { Unexpected(); }
int64_t ecs::PlayerRuntime::GetMaxStamina(entt::entity) { Unexpected(); }
bool AffectSystem::StartAffectEvent(entt::entity) { Unexpected(); }
void AffectSystem::SetFlag(entt::entity, uint32_t, bool) { Unexpected(); }
ecs::MobInstanceState* CombatSystem::MobState(entt::entity) { Unexpected(); }
void CombatSystem::SetValidComboInterval(entt::entity, int) { Unexpected(); }
void CombatSystem::SendLeaderboardData(entt::entity) { Unexpected(); }
void CombatSystem::SendLeaderboardDataGuild(entt::entity) { Unexpected(); }
void CombatSystem::SendLeaderboardDataSkillMob(entt::entity, entt::entity) { Unexpected(); }
void MountSystem::UpdateMountInventoryCountOverhead(entt::entity, entt::entity) { Unexpected(); }
void CombatSystem::UpdateKillerMode(entt::entity) { Unexpected(); }
void CEntity::UpdateSectree() { Unexpected(); }
void CHARACTER::ComputePoints() { Unexpected(); }
uint8_t ecs::PlayerRuntime::GetBattlePassId(entt::entity) { Unexpected(); }
bool ecs::PlayerRuntime::IsCompletedMission(entt::entity, uint8_t) { Unexpected(); }
uint32_t ecs::PlayerRuntime::GetMissionProgress(entt::entity, uint32_t, uint32_t) { Unexpected(); }
bool ecs::PlayerRuntime::UpdateMissionProgress(entt::entity, uint32_t, uint32_t, uint32_t, uint32_t, bool) { Unexpected(); }
bool CBattlePass::BattlePassMissionGetInfo(uint8_t, uint8_t, uint32_t*, uint32_t*) { Unexpected(); }
CPIDRegistry& CPIDRegistry::Instance() { Unexpected(); }
std::vector<entt::entity> CPIDRegistry::Snapshot() const { Unexpected(); }

// The warp cluster moved into MovementSystem.cpp and brought the map table,
// the descriptor, the switchbot and the P2P link with it. None of that is
// reachable headless, so every one of these must stay unreached.
int g_nPortalLimitTime = 10;
uint8_t g_bChannel = 1;
uint16_t mother_port = 13000;
bool map_allow_find(int) { Unexpected(); }
int number_ex(int, int, const char*, int) { Unexpected(); }
bool CEntity::IsType(int) const { Unexpected(); }
bool CHARACTER::CanHandleItem(bool, bool) { Unexpected(); }
bool ecs::PlayerRuntime::IsHack(entt::entity, bool, bool, int) { Unexpected(); }
bool ecs::PlayerRuntime::IsHack(entt::entity, bool, bool) { Unexpected(); }
bool CMapLocation::Get(int, int, int&, uint32_t&, uint16_t&) { Unexpected(); }
SECTREE_MAP* SECTREE_MANAGER::GetMap(int) { Unexpected(); }
int SECTREE_MANAGER::GetMapIndex(int, int) { Unexpected(); }
void CSwitchbotManager::P2PSendSwitchbot(uint32_t, uint16_t) { Unexpected(); }
void CSwitchbotManager::SetIsWarping(uint32_t, bool) { Unexpected(); }
void DESC::Packet(const void*, int) { Unexpected(); }
void LogManager::CharLog(entt::entity, uint32_t, const char*, const char*) { Unexpected(); }
void P2P_MANAGER::Send(const void*, int, LPDESC) { Unexpected(); }
uint8_t ecs::PlayerRuntime::GetEmpire(entt::entity) { Unexpected(); }
uint32_t ecs::PlayerRuntime::GetPlayerID(entt::entity) { Unexpected(); }
bool ecs::PlayerRuntime::IsGoto(entt::entity) { Unexpected(); }
bool ecs::PlayerRuntime::IsWarp(entt::entity) { Unexpected(); }
LPDUNGEON ecs::SocialSystem::GetDungeon(entt::entity) { Unexpected(); }
// MovementSystem asks for the party on a sector change now that CHARACTER
// has no getter of its own.
LPPARTY ecs::SocialSystem::GetParty(entt::entity) { Unexpected(); }
SECTREE* ecs::PlayerRuntime::GetSectree(entt::entity) { Unexpected(); }
float ecs::PlayerRuntime::GetRotation(entt::entity e) {
    const auto* runtime = g_registry.try_get<ecs::CharacterRuntimeFlagsComponent>(e);
    return runtime ? runtime->rotation : 0.0f;
}
void AISystem::GotoState(entt::entity e, ecs::AIFSMState state) {
    Check(g_registry.valid(e), "AI transition on stale entity"); transitions.emplace_back(e, state);
}
entt::entity CombatSystem::GetVictim(entt::entity e) {
    const auto* target = g_registry.try_get<ecs::CombatTarget>(e);
    return target && g_registry.valid(target->target) &&
        g_registry.all_of<ecs::CharacterType>(target->target) ? target->target : entt::null;
}
void ecs::ViewSystem::PacketView(entt::entity e, const void* data, int size, entt::entity except) {
    if (*static_cast<const uint8_t*>(data) == HEADER_GC_MOTION) {
        Check(g_registry.valid(e) && except == entt::null && size == sizeof(packet_motion),
            "invalid animation broadcast or source excluded");
        animationPackets.push_back(*static_cast<const packet_motion*>(data));
        if (onAnimation) onAnimation(e);
        return;
    }
    Check(g_registry.valid(e) && e == except && size == sizeof(TPacketGCMove), "invalid movement broadcast");
    movementPackets.push_back(*static_cast<const TPacketGCMove*>(data));
}
// Fail-fast link seams for the still-unmigrated functions in MovementSystem.cpp.
// None may be reached by the native tick or packet tests.
void intrusive_ptr_add_ref(event*) { Unexpected(); }
void intrusive_ptr_release(event*) { Unexpected(); }
LPEVENT event_create_ex(TEVENTFUNC, event_info_data*, int32_t) { Unexpected(); }
void ecs::ChatSystem::Send(entt::entity, uint8_t, const char*, ...) { Unexpected(); }
LPSHOP ecs::SocialSystem::GetMyShop(entt::entity) { Unexpected(); }
bool AffectSystem::IsPolymorphed(entt::entity e) { return motionSettings[e].polymorphed; }
uint32_t ecs::PlayerRuntime::GetPacketVID(entt::entity) { Unexpected(); }
uint32_t ecs::PlayerRuntime::GetRaceNum(entt::entity e) { return motionSettings[e].race; }
int ecs::PlayerRuntime::GetStamina(entt::entity e) { return motionSettings[e].stamina; }
uint32_t MountSystem::GetMountVnum(entt::entity e) { return motionSettings[e].mount; }
std::string_view ecs::PlayerRuntime::GetName(entt::entity) { Unexpected(); }
int32_t ecs::PlayerRuntime::GetMapIndex(entt::entity e) {
    auto it = placements.find(e); if (it == placements.end()) Unexpected();
    return it->second.mapIndex;
}
int32_t ecs::PlayerRuntime::GetX(entt::entity e) {
    auto it = placements.find(e); if (it == placements.end()) Unexpected();
    return it->second.x;
}
int32_t ecs::PlayerRuntime::GetY(entt::entity e) {
    auto it = placements.find(e); if (it == placements.end()) Unexpected();
    return it->second.y;
}
LPEVENT ecs::PlayerRuntime::GetCharEvent(entt::entity, CharEvent) { Unexpected(); }
void ecs::PlayerRuntime::SetCharEvent(entt::entity, CharEvent, LPEVENT) { Unexpected(); }
int ecs::PlayerRuntime::GetPosition(entt::entity) { Unexpected(); }
void CombatSystem::CheckTarget(entt::entity) { Unexpected(); }
bool CombatSystem::IsStun(entt::entity) { Unexpected(); }
bool CombatSystem::IsDead(entt::entity) { Unexpected(); }
int32_t CEntity::GetX() const { Unexpected(); }
int32_t CEntity::GetY() const { Unexpected(); }
int32_t CEntity::GetZ() const { Unexpected(); }
LPSECTREE CEntity::GetSectree() const { Unexpected(); }
int CalculateDuration(int, int) { Unexpected(); }
namespace ecs::SessionSystem {
void FlushDelayedSaveItem(entt::entity) { Unexpected(); }
void Save(entt::entity) { Unexpected(); }
}
void CHARACTER::Save() { Unexpected(); }
const char* CHARACTER::GetName(uint8_t) const { Unexpected(); }
uint32_t CHARACTER::GetPacketVID() const { Unexpected(); }
void CombatSystem::DistributeSP(entt::entity, entt::entity, int) { Unexpected(); }
int64_t ecs::PlayerRuntime::GetHP(entt::entity) { Unexpected(); }
int ecs::PointSystem::GetLimitPoint(entt::entity, uint8_t) { Unexpected(); }
const TMobTable* ecs::PlayerRuntime::GetMobTable(entt::entity) { Unexpected(); }
void CHARACTER::PointChange(uint8_t, int64_t, bool, bool, bool) { Unexpected(); }
void CHARACTER::OnMove(bool) { Unexpected(); }
bool AffectSystem::IsAffectFlag(entt::entity, uint32_t) { Unexpected(); }
bool CHARACTER::IsEquipUniqueItem(uint32_t) const { Unexpected(); }
void CombatSystem::Dead(entt::entity, entt::entity, bool) { Unexpected(); }
void CHARACTER::MonsterLog(const char*, ...) { Unexpected(); }
int CDungeon::GetFlag(std::string) { Unexpected(); }
CMotion::CMotion() {}
CMotion::~CMotion() {}
float CMotion::GetDuration() const { return m_fDuration; }
const D3DXVECTOR3& CMotion::GetAccumVector() const { return m_vec3Accumulation; }
CMotionManager::CMotionManager() {}
CMotionManager::~CMotionManager() {}
const CMotion* CMotionManager::GetMotion(uint32_t race, uint32_t key) {
    motionRequests.emplace_back(race, key);
    auto it = motions.find({race, key}); return it == motions.end() ? nullptr : it->second;
}
float GetDegreeFromPositionXY(int32_t, int32_t, int32_t, int32_t) { Unexpected(); }
void quest::CQuestManager::AttrIn(uint32_t, LPCHARACTER, int) { Unexpected(); }
void quest::CQuestManager::AttrOut(uint32_t, LPCHARACTER, int) { Unexpected(); }
entt::entity ItemSystem::GetWearItem(entt::entity e, uint8_t slot) {
    Check(slot == WEAR_WEAPON, "motion read wrong wear slot"); return motionSettings[e].weapon;
}
bool ItemSystem::IsValidItem(entt::entity e) { return g_registry.valid(e) && g_registry.all_of<ecs::ItemIdentity>(e); }
const TItemTable* ItemSystem::GetItemProto(entt::entity e) {
    auto it = weaponProtos.find(e); return it == weaponProtos.end() ? nullptr : &it->second;
}
uint32_t CParty::GetLeaderPID() { Unexpected(); }
int64_t ecs::PointSystem::Get(entt::entity e, uint8_t point) {
    Check(point == POINT_MOV_SPEED, "motion read wrong point"); return motionSettings[e].movePoint;
}
int ecs::PointSystem::GetMaxHP(entt::entity) { Unexpected(); }
void ecs::PointSystem::Change(entt::entity, uint8_t, int64_t, bool, bool, bool) { Unexpected(); }
uint32_t g_start_position[4][2] {};
int passes_per_sec = 25;
int save_event_second_cycle = 60;
int test_server = 0;
int CEntity::GetType() const { Unexpected(); }
CItem* ITEM_MANAGER::Find(uint32_t) { Unexpected(); }
CItem* ITEM_MANAGER::FindByVID(uint32_t) { Unexpected(); }
CItemRegistry& CItemRegistry::Instance() { Unexpected(); }
entt::entity CItemRegistry::Find(uint32_t) const { Unexpected(); }
entt::entity CItemRegistry::FindByVID(uint32_t) const { Unexpected(); }
entt::entity ecs::CBuildingRegistry::FindByID(uint32_t) { Unexpected(); }
building::CObject* ecs::CBuildingRegistry::FindLegacyByEntity(entt::entity) { Unexpected(); }
entt::entity ecs::OfflineShopEntityRegistry::FindByVID(uint32_t) { Unexpected(); }
offlineshop::ShopEntity* ecs::OfflineShopEntityRegistry::FindLegacyByEntity(entt::entity) { Unexpected(); }
bool ItemSystem::DestroyItemEntityEcs(entt::entity e, const char*) {
    ++retired;
    if (onRetire) { auto callback = onRetire; callback(e); }
    if (g_registry.valid(e)) g_registry.destroy(e);
    return true;
}
namespace ecs::EntityNetworkDispatch {
void SendInsert(entt::registry& reg, entt::entity source, entt::entity viewer) {
    Check(reg.valid(source) && reg.valid(viewer), "insert stale identity");
    Check(source == viewer || Visible(source, viewer), "insert before edge commit");
    Packet packet {true, source, viewer}; packets.push_back(packet);
    if (onPacket) { auto callback = onPacket; callback(packet); }
}
void SendRemove(entt::registry& reg, entt::entity source, entt::entity viewer) {
    Check(reg.valid(source) && reg.valid(viewer), "remove stale identity");
    Packet packet {false, source, viewer}; packets.push_back(packet);
    if (onPacket) { auto callback = onPacket; callback(packet); }
}
}

namespace {
void MembershipAndSnapshots() {
    Reset(); MapFixture map, privateMap(10001);
    const auto a = Entity(), b = Entity();
    Check(Spawn(a) && Spawn(b, 1, 6500), "native item insertion failed");
    Check(ecs::SpatialService::GetSectree(g_registry, a) == map.At(), "native sector lookup failed");
    std::vector<entt::entity> nearby;
    auto collect = [&](entt::entity e) { nearby.push_back(e); };
    map.At()->ForEachAround(collect);
    Check(nearby.size() == 2, "neighbor snapshot omitted entity-only item");
    const auto snapshot = map.At()->SnapshotAround();
    ecs::SpatialService::RemoveEntity(g_registry, b);
    Check(Spawn(b, 10001, 6500), "private-map relocation failed");
    nearby.clear(); auto saved = snapshot; saved.ForEach(collect);
    Check(nearby.size() == 1 && nearby[0] == a, "snapshot visited moved member");
    const auto stale = a; ecs::SpatialService::RemoveEntity(g_registry, a); g_registry.destroy(a);
    const auto recycled = Entity(); Check(Spawn(recycled), "recycled insertion failed");
    nearby.clear(); saved.ForEach(collect);
    Check(nearby.empty() && stale != recycled, "snapshot visited recycled generation");
    Check(!Spawn(recycled) && !Spawn(Entity(), 99), "duplicate/missing-map insertion accepted");
}
void VisibilityRoundTrip() {
    Reset(); MapFixture map;
    const auto item = Entity(), player = Entity(ecs::SpatialKind::Character);
    Check(Spawn(item) && Spawn(player), "item-first spawn failed");
    Check(Visible(item, player) && !Visible(player, item), "late viewer cannot see entity-only item");
    Check(packets.size() == 1 && packets[0].add && packets[0].source == item && packets[0].viewer == player,
        "incorrect directed item insertion");
    ecs::VisibilitySystem::Refresh(g_registry, player);
    Check(packets.size() == 1, "refresh resent unchanged item");
    g_registry.get<ecs::Position>(player).x = 15000;
    Check(map.At(15000)->InsertEntity(player), "player sector move failed");
    ecs::VisibilitySystem::Refresh(g_registry, player);
    Check(!Visible(item, player) && !packets.back().add && packets.back().source == item, "item leaving remove reversed/missed");
    g_registry.get<ecs::Position>(player).x = 100;
    Check(map.At()->InsertEntity(player), "player return failed");
    ecs::VisibilitySystem::Refresh(g_registry, player);
    Check(Visible(item, player), "returning player missed item");
    ecs::SpatialService::RemoveEntity(g_registry, item);
    Check(!map.At()->Contains(item) && !Visible(item, player) &&
        !g_registry.any_of<ecs::SpatialEntity, ecs::SectorPlacement, ecs::ViewActiveTag>(item),
        "item removal left spatial state");
    Check(!packets.back().add && packets.back().source == item, "ground remove not broadcast");
}
void ViewCallbacks() {
    for (int action = 0; action < 3; ++action) {
        Reset(); MapFixture map;
        const auto player = Entity(ecs::SpatialKind::Character), other = Entity(ecs::SpatialKind::Character);
        Check(Spawn(player) && Spawn(other), "viewer setup");
        const auto item = Entity();
        onPacket = [&](Packet packet) {
            if (!packet.add || packet.source != item) return;
            onPacket = {};
            ecs::SpatialService::RemoveEntity(g_registry, item);
            if (action == 0) g_registry.destroy(item);
            if (action == 1) Check(Spawn(item, 1, 15000), "callback relocation failed");
            if (action == 2) Check(Spawn(item), "callback same-location respawn failed");
        };
        Check(Spawn(item), "committed callback spawn reported failure");
        if (action == 0) Check(!g_registry.valid(item), "callback deletion missed");
        if (action == 1) Check(!Visible(item, player) && !Visible(item, other), "outer spawn republished relocated item");
        if (action == 2) Check(Visible(item, player) && Visible(item, other), "same-location respawn lost visibility");
    }
}
void PreparationAndPCs() {
    Reset(); MapFixture map;
    const auto npc = Entity(ecs::SpatialKind::Character);
    g_registry.remove<ecs::TagPC>(npc);
    g_registry.get<ecs::CharacterType>(npc).value = CHAR_TYPE_MONSTER;
    Check(Spawn(npc) && !awake.contains(npc), "NPC active without PCs");
    const auto pc = Entity(ecs::SpatialKind::Character);
    Check(Spawn(pc) && awake.contains(npc), "PC arrival failed to wake existing NPC");
    ecs::SpatialService::RemoveEntity(g_registry, pc);
    Check(!awake.contains(npc), "last PC departure failed to stop NPC");
    const auto item = Entity();
    Callback callback {[&](entt::registry& reg, entt::entity e) { if (e == item) reg.destroy(e); }};
    entt::scoped_connection connection = g_registry.on_construct<ecs::SectorPlacement>().connect<&Callback::Run>(callback);
    Check(!Spawn(item) && !g_registry.valid(item), "construction deletion ignored");
}

void LifetimeAndObservers() {
    Reset(); MapFixture map; MapFixture privateMap(10001);
    const auto a = Entity(ecs::SpatialKind::Character), b = Entity(ecs::SpatialKind::Character);
    Check(Spawn(a) && Spawn(b), "character spawn");
    Check(Visible(a, b) && Visible(b, a), "character edges are not mutual");
    const auto count = packets.size();
    for (int i = 0; i < 10; ++i) {
        ++g_registry.get<ecs::Position>(a).x;
        g_dispatcher.trigger<ecs::PositionChangedEvent>(ecs::PositionChangedEvent {a});
    }
    Check(packets.size() == count, "movement resent existing character insert");
    g_registry.emplace<ecs::StatusFlags>(a).isObserverMode = true;
    ecs::VisibilitySystem::Refresh(g_registry, a);
    Check(!Visible(a, b) && Visible(b, a), "observer directions wrong");
    g_registry.get<ecs::StatusFlags>(a).isObserverMode = false;
    ecs::VisibilitySystem::Refresh(g_registry, a);
    Check(Visible(a, b), "observer exit stayed hidden");
    const auto building = Entity(ecs::SpatialKind::Building), shop = Entity(ecs::SpatialKind::OfflineShop);
    Check(Spawn(building, 1, 12000) && Spawn(shop), "non-character spawn");
    Check(Visible(building, a) && Visible(shop, a), "building range exception / shop hidden");
    const auto beforeRename = packets.size();
    ecs::VisibilitySystem::Reencode(g_registry, shop);
    Check(packets.size() == beforeRename + 4 && packets.back().source == shop,
        "shop reencode failed to update both viewers");
    const auto foreign = Entity();
    Check(Spawn(foreign, 10001) && !Visible(foreign, a), "cross-map visibility leak");

    const auto npc = Entity(ecs::SpatialKind::Character);
    g_registry.remove<ecs::TagPC>(npc);
    g_registry.get<ecs::CharacterType>(npc).value = CHAR_TYPE_MONSTER;
    Check(Spawn(npc) && awake.contains(npc), "NPC did not wake");
    g_registry.destroy(a); g_registry.destroy(b);
    Check(!map.At()->Contains(a) && !map.At()->Contains(b) && !awake.contains(npc),
        "registry destruction left membership/PC count");
    const auto replacement = Entity(ecs::SpatialKind::Character);
    Check(Spawn(replacement) && awake.contains(npc), "recycled PC failed to wake NPC");
    ecs::SpatialService::RemoveEntity(g_registry, replacement);
    Check(map.At()->InsertEntity(replacement), "native character reinsert failed");
    ecs::VisibilitySystem::Refresh(g_registry, replacement);
    Check(g_registry.all_of<ecs::SpatialEntity, ecs::ViewActiveTag>(replacement) &&
        Visible(shop, replacement), "character reinsert did not restore visibility tags");
}

void RemovalCallbacksAndTeardown() {
    Reset(); MapFixture map;
    const auto player = Entity(ecs::SpatialKind::Character), item = Entity();
    Check(Spawn(player) && Spawn(item), "despawn setup");
    Callback callback {[&](entt::registry& reg, entt::entity e) {
        if (e != item) return;
        Check(!ecs::SpatialService::InsertEntity(reg, e, 1, 100, 100, 0),
            "on_destroy reinserted tag being destroyed");
    }};
    entt::scoped_connection connection =
        g_registry.on_destroy<ecs::SpatialEntity>().connect<&Callback::Run>(callback);
    onPacket = [&](Packet p) {
        if (p.add || p.source != item) return;
        onPacket = {};
        Check(Spawn(item), "despawn packet callback cannot respawn");
    };
    ecs::SpatialService::RemoveEntity(g_registry, item);
    Check(map.At()->Contains(item) && Visible(item, player) &&
        g_registry.all_of<ecs::SpatialEntity, ecs::ViewActiveTag>(item),
        "older despawn removed new spawn");
    connection.release();
    onPacket = {};
    const auto second = Entity();
    Check(Spawn(second), "teardown item spawn");
    onRetire = [&](entt::entity e) {
        Check(!Spawn(e), "destroying sector accepted reinsert");
        map.At()->Destroy(); // Reentrant region teardown is idempotent.
    };
    map.At()->Destroy();
    Check(!g_registry.valid(item) && !g_registry.valid(second) && retired == 2,
        "sector teardown skipped/doubled item retirement");
}

void PreparationMutationAndIteration() {
    Reset(); MapFixture map;
    const auto item = Entity();
    Callback callback {[&](entt::registry& reg, entt::entity e) {
        if (e == item) reg.get<ecs::ItemLocation>(e).window = INVENTORY;
    }};
    entt::scoped_connection connection =
        g_registry.on_construct<ecs::SectorPlacement>().connect<&Callback::Run>(callback);
    Check(!Spawn(item) && !map.At()->Contains(item) &&
        !g_registry.all_of<ecs::SectorPlacement>(item) &&
        g_registry.get<ecs::ItemLocation>(item).window == INVENTORY,
        "failed preparation overwrote storage or left phantom placement");
    connection.release();
    std::vector<entt::entity> original;
    for (int i = 0; i < 8; ++i) {
        auto e = Entity(); Check(Spawn(e), "map snapshot setup"); original.push_back(e);
    }
    int visits = 0;
    ecs::SpatialService::ForEachInMap(g_registry, 1, [&](entt::entity e) {
        Check(g_registry.valid(e), "map snapshot delivered stale handle");
        ++visits;
        if (visits != 1) return;
        for (auto old : original) if (old != e) g_registry.destroy(old);
        auto replacement = Entity(); Check(Spawn(replacement), "map callback replacement");
    });
    Check(visits == 1, "map iteration included deleted/replacement entities");
}

struct MovementProbe {
    std::vector<ecs::PositionChangedEvent> positions;
    std::vector<ecs::EvEntityMoved> moved;
    std::function<void(const ecs::PositionChangedEvent&)> onPosition;
    std::function<void(const ecs::EvEntityMoved&)> onMoved;
    entt::scoped_connection positionConnection, movedConnection;
    MovementProbe() :
        positionConnection(g_dispatcher.sink<ecs::PositionChangedEvent>().connect<&MovementProbe::Position>(*this)),
        movedConnection(g_dispatcher.sink<ecs::EvEntityMoved>().connect<&MovementProbe::Moved>(*this)) {}
    void Position(const ecs::PositionChangedEvent& event) {
        Check(g_registry.valid(event.entity), "position event has stale identity");
        const auto* tree = ecs::SectorOf(g_registry, event.entity);
        Check(tree && tree->Contains(event.entity) &&
            tree == ecs::SectorAt(event.newMapIndex, event.newX, event.newY),
            "position event preceded native sector commit");
        positions.push_back(event);
        if (onPosition) onPosition(event);
    }
    void Moved(const ecs::EvEntityMoved& event) {
        Check(g_registry.valid(event.entity), "movement event has stale identity");
        moved.push_back(event);
        if (onMoved) onMoved(event);
    }
};
entt::entity Moving(int x, int y, int targetX, int targetY, int speed = 200, bool npc = false) {
    const auto e = Entity(ecs::SpatialKind::Character);
    if (npc) {
        g_registry.remove<ecs::TagPC>(e);
        g_registry.emplace<ecs::TagNPC>(e);
        g_registry.get<ecs::CharacterType>(e).value = CHAR_TYPE_MONSTER;
    }
    g_registry.emplace<ecs::MovementState>(e);
    g_registry.emplace<ecs::MovementSpeed>(e, 100, speed);
    g_registry.emplace<ecs::CharacterRuntimeFlagsComponent>(e);
    Check(Spawn(e, 1, x, y), "native mover spawn");
    g_registry.emplace<ecs::MovementDestination>(e, targetX, targetY);
    Check(!g_registry.all_of<ecs::LegacyCharPtr>(e), "movement fixture acquired legacy character");
    return e;
}
void NativeMovement() {
    Reset(); MapFixture map; MovementProbe probe;
    const auto e = Moving(6300, 100, 6700, 100);
    auto* oldTree = map.At(6300, 100);
    const auto revision = g_registry.get<ecs::SpatialRevision>(e).value;
    g_registry.get<ecs::Position>(e).z = 77;
    g_registry.get<ecs::PositionZ>(e).z = 77;
    g_registry.get<ecs::MovementState>(e).walkPreference = true;
    g_registry.remove<ecs::VIDComponent>(e); // Tick has no VID-index dependency.
    MovementSystem_Update(g_registry, 10);
    Check(g_registry.get<ecs::Position>(e).x == 6500 && !oldTree->Contains(e) &&
        map.At(6500, 100)->Contains(e), "native tick did not migrate sector");
    Check(g_registry.get<ecs::SpatialRevision>(e).value == revision + 1, "sector revision not advanced");
    Check(probe.positions.size() == 1 && probe.moved.size() == 1 &&
        probe.positions[0].oldX == 6300 && probe.positions[0].newX == 6500,
        "movement old/new event values or count wrong");
    MovementSystem_Update(g_registry, 20);
    const auto& state = g_registry.get<ecs::MovementState>(e);
    Check(g_registry.get<ecs::Position>(e).x == 6700 && !g_registry.all_of<ecs::MovementDestination>(e) &&
        state.lastMoveTime == 20 && state.stopTime == 20 && !state.isWalking &&
        !state.isNowWalking && state.moveDuration == 0 && state.walkPreference, "arrival state incorrect");
    Check(probe.positions.size() == 2 && probe.moved.size() == 2 && transitions.empty(),
        "arrival duplicated publication or transitioned a PC");
    Check(g_registry.get<ecs::Position>(e).z == 77 && g_registry.get<ecs::PositionZ>(e).z == 77,
        "movement changed altitude");
    MovementSystem_Update(g_registry, 30);
    Check(probe.positions.size() == 2, "stopped entity moved again");
}
void MovementVisibilityAndBounds() {
    Reset(); MapFixture map; MovementProbe probe;
    const auto oldViewer = Entity(ecs::SpatialKind::Character), newViewer = Entity(ecs::SpatialKind::Character);
    Check(Spawn(oldViewer, 1, 500, 100) && Spawn(newViewer, 1, 10500, 100), "movement viewers");
    const auto e = Moving(1000, 100, 10000, 100, 20000);
    Check(Visible(e, oldViewer) && !Visible(e, newViewer), "initial movement visibility");
    packets.clear();
    MovementSystem_Update(g_registry, 50);
    Check(!Visible(e, oldViewer) && Visible(e, newViewer), "movement did not reconcile visibility");
    Check(std::count_if(packets.begin(), packets.end(), [&](const Packet& p) {
        return p.source == e && p.viewer == newViewer && p.add;
    }) == 1, "arrival duplicated insert packet");

    const auto edge = Moving(25500, 100, 25700, 100);
    const auto overflow = Moving(100, 500, INT32_MIN, 500, 1);
    const auto diagonal = Moving(500, 500, 800, 900, 100);
    const auto zeroSpeed = Moving(100, 700, 110, 700, 0);
    const auto detached = Moving(100, 900, 110, 900);
    ecs::SpatialService::RemoveEntity(g_registry, detached);
    const auto dead = Moving(100, 1100, 110, 1100);
    g_registry.emplace<ecs::DeadTag>(dead);
    MovementSystem_Update(g_registry, 60);
    Check(g_registry.get<ecs::Position>(edge).x == 25500 &&
        g_registry.get<ecs::MovementState>(edge).lastMoveTime == 0 &&
        map.At(25500, 100)->Contains(edge), "out-of-map step partially committed");
    Check(g_registry.get<ecs::Position>(overflow).x == 99, "destination subtraction overflowed");
    Check(g_registry.get<ecs::Position>(diagonal).x == 560 &&
        g_registry.get<ecs::Position>(diagonal).y == 580, "diagonal interpolation changed");
    Check(g_registry.get<ecs::Position>(zeroSpeed).x == 101, "nonpositive speed did not clamp");
    Check(g_registry.get<ecs::Position>(detached).x == 100 && !ecs::SectorOf(g_registry, detached),
        "tick respawned detached entity");
    Check(g_registry.get<ecs::Position>(dead).x == 100 && g_registry.all_of<ecs::DeadTag>(dead),
        "tick moved or revived dead character");
}
void MovementCallbackLifetime() {
    Reset(); MapFixture map; MovementProbe probe;
    const auto e = Moving(100, 100, 300, 100);
    entt::entity replacement = entt::null;
    probe.onPosition = [&](const ecs::PositionChangedEvent& event) {
        if (event.entity != e) return;
        g_registry.destroy(e);
        replacement = Moving(500, 100, 700, 100);
    };
    MovementSystem_Update(g_registry, 80);
    Check(!g_registry.valid(e) && g_registry.valid(replacement) && probe.moved.empty() &&
        g_registry.get<ecs::Position>(replacement).x == 500,
        "tick used retired identity or advanced callback-created replacement");
    probe.onPosition = {};

    // A visibility packet can also retire the mover, after PositionChangedEvent.
    const auto viewer = Entity(ecs::SpatialKind::Character); Check(Spawn(viewer, 1, 10000, 100), "callback viewer");
    const auto packetMover = Moving(1000, 100, 10000, 100, 20000);
    onPacket = [&](Packet p) {
        if (p.add && p.source == packetMover && p.viewer == viewer) g_registry.destroy(packetMover);
    };
    MovementSystem_Update(g_registry, 90);
    Check(!g_registry.valid(packetMover) && std::none_of(probe.moved.begin(), probe.moved.end(),
        [&](const auto& event) { return event.entity == packetMover; }), "packet callback left stale tick continuation");
    onPacket = {};
}
void MovementCallbackRetarget() {
    Reset(); MapFixture map; MovementProbe probe;
    const auto e = Moving(100, 100, 300, 100, 200, true);
    probe.onPosition = [&](const ecs::PositionChangedEvent& event) {
        if (event.entity != e) return;
        g_registry.emplace<ecs::MovementDestination>(e, 1000, 100);
        g_registry.get<ecs::MovementState>(e).moveDuration = 777;
    };
    MovementSystem_Update(g_registry, 100);
    Check(g_registry.get<ecs::MovementDestination>(e).x == 1000 &&
        g_registry.get<ecs::MovementState>(e).moveDuration == 777 &&
        transitions.empty() && probe.moved.empty(), "old arrival overrode callback retarget");

    probe.onPosition = {};
    probe.onMoved = [&](const ecs::EvEntityMoved& event) {
        if (event.entity != e) return;
        ecs::SpatialService::RemoveEntity(g_registry, e);
        Check(Spawn(e, 1, event.newX, event.newY), "same-location callback respawn");
    };
    g_registry.get<ecs::MovementDestination>(e).x = 500;
    MovementSystem_Update(g_registry, 110);
    Check(transitions.empty() && map.At(500, 100)->Contains(e), "old arrival transitioned new spawn");

    probe.onMoved = {};
    std::vector<entt::entity> original;
    for (int i = 0; i < 6; ++i) original.push_back(Moving(2000, 2000 + i * 100, 2200, 2000 + i * 100));
    bool changed = false;
    probe.onPosition = [&](const ecs::PositionChangedEvent& event) {
        if (changed) return;
        changed = true;
        for (auto other : original) {
            if (other == event.entity) continue;
            g_registry.get<ecs::MovementDestination>(other).x = 4000;
        }
    };
    const auto before = probe.moved.size();
    MovementSystem_Update(g_registry, 120);
    Check(changed && probe.moved.size() == before + 1, "tick advanced later retargeted snapshot entries");
}
void MovementArrivalAndPackets() {
    Reset(); MapFixture map; MovementProbe probe;
    const auto victim = Entity(ecs::SpatialKind::Character);
    const auto fighter = Moving(100, 100, 100, 100, 200, true);
    g_registry.emplace<ecs::CombatTarget>(fighter, victim, 0u);
    MovementSystem_Update(g_registry, 130);
    Check(g_registry.all_of<ecs::CombatActiveTag>(fighter) &&
        g_registry.get<ecs::CharacterRuntimeFlagsComponent>(fighter).position == POS_FIGHTING &&
        transitions.size() == 1 && transitions.back().second == ecs::AIFSMState::Battle &&
        probe.positions.empty() && probe.moved.empty(), "zero-distance native combat arrival");
    const auto coward = Moving(200, 100, 300, 100, 200, true);
    g_registry.emplace<ecs::CombatTarget>(coward, victim, 0u);
    g_registry.emplace<ecs::AIFlags>(coward).isCoward = true;
    MovementSystem_Update(g_registry, 140);
    Check(!g_registry.all_of<ecs::CombatTarget>(coward) &&
        g_registry.get<ecs::CharacterRuntimeFlagsComponent>(coward).position == POS_STANDING &&
        transitions.back().second == ecs::AIFSMState::Idle, "coward arrival did not enter idle");

    const auto e = Moving(1000, 1500, 1200, 1600);
    g_registry.get<ecs::MovementState>(e).moveDuration = 444;
    g_registry.get<ecs::CharacterRuntimeFlagsComponent>(e).rotation = 225;
    ecs::MovementSystem::SendMovePacket(e, FUNC_WAIT, 2, 9, 9, 9);
    Check(movementPackets.size() == 1 && movementPackets.back().dwVID == g_registry.get<ecs::VIDComponent>(e).value &&
        movementPackets.back().lX == 1200 && movementPackets.back().lY == 1600 &&
        movementPackets.back().dwDuration == 444 && movementPackets.back().bRot == 45 &&
        movementPackets.back().dwTime == 123456, "entity movement packet lost destination/timing/rotation");
    g_registry.remove<ecs::MovementDestination>(e);
    g_registry.get<ecs::MovementState>(e).moveDuration = 0;
    ecs::MovementSystem::SendMovePacket(e, FUNC_WAIT, 0, 0, 0, 9, 42, 10);
    Check(movementPackets.back().lX == 1000 && movementPackets.back().lY == 1500 &&
        movementPackets.back().dwDuration == 0 && movementPackets.back().dwTime == 42 &&
        movementPackets.back().bRot == 10, "stopped movement packet fallback");
    ecs::MovementSystem::SendMovePacket(e, FUNC_ATTACK, 3, 2000, 2500, 99, 43, 12);
    Check(movementPackets.back().bHeader == HEADER_GC_MOVE && movementPackets.back().bFunc == FUNC_ATTACK &&
        movementPackets.back().bArg == 3 && movementPackets.back().lX == 2000 &&
        movementPackets.back().lY == 2500 && movementPackets.back().dwDuration == 99,
        "explicit movement packet fields changed");
    g_registry.destroy(e);
    ecs::MovementSystem::SendMovePacket(e, FUNC_WAIT, 0, 0, 0, 0);
    ecs::MovementSystem::SendMovePacket(entt::null, FUNC_WAIT, 0, 0, 0, 0);
    ecs::MovementSystem::SendMovePacket(Entity(), FUNC_WAIT, 0, 0, 0, 0);
    Check(movementPackets.size() == 3, "invalid/non-character entity emitted movement packet");
}


void NativeAnimationPackets() {
    Reset();
    const auto source = Entity(ecs::SpatialKind::Character);
    const auto victim = Entity(ecs::SpatialKind::Character);
    ecs::MovementSystem::Motion(source, MOTION_DAMAGE, victim);
    Check(animationPackets.size() == 1 && animationPackets.back().header == HEADER_GC_MOTION &&
        animationPackets.back().vid == g_registry.get<ecs::VIDComponent>(source).value &&
        animationPackets.back().victim_vid == g_registry.get<ecs::VIDComponent>(victim).value &&
        animationPackets.back().motion == MOTION_DAMAGE, "native animation encoding");
    g_registry.destroy(victim);
    const auto recycled = Entity(ecs::SpatialKind::Character);
    ecs::MovementSystem::Motion(source, MOTION_DAMAGE, victim);
    Check(animationPackets.back().victim_vid == 0, "animation resolved recycled victim generation");
    g_registry.remove<ecs::VIDComponent>(recycled);
    ecs::MovementSystem::Motion(source, MOTION_DAMAGE, recycled);
    Check(animationPackets.back().victim_vid == 0, "missing victim VID fallback");
    ecs::MovementSystem::Motion(source, MOTION_DAMAGE);
    Check(animationPackets.back().victim_vid == 0, "null victim fallback");
    ecs::MovementSystem::Motion(recycled, MOTION_DAMAGE);
    ecs::MovementSystem::Motion(Entity(), MOTION_DAMAGE);
    ecs::MovementSystem::Motion(entt::null, MOTION_DAMAGE);
    Check(animationPackets.size() == 4, "invalid animation source broadcast");
    onAnimation = [](entt::entity e) { g_registry.destroy(e); };
    ecs::MovementSystem::Motion(source, MOTION_DAMAGE);
    ecs::MovementSystem::Motion(source, MOTION_DAMAGE);
    Check(!g_registry.valid(source) && animationPackets.size() == 5, "retired animation source reused");
}

void NativeMovementDurationReads() {
    Reset();
    const auto e = Entity(ecs::SpatialKind::Character);
    Check(ecs::MovementSystem::GetCurrentMoveDuration(e) == 0 &&
        !g_registry.all_of<ecs::MovementState>(e), "duration read created bootstrap state");
    auto& state = g_registry.emplace<ecs::MovementState>(e);
    state.moveStartTime = 0; state.moveDuration = 456;
    Check(ecs::MovementSystem::GetCurrentMoveDuration(e) == 456,
        "duration read required destination or returned remaining time");
    state.moveDuration = UINT32_MAX;
    Check(ecs::MovementSystem::GetCurrentMoveDuration(e) == UINT32_MAX, "duration read truncated stored value");
    state.moveDuration = 0;
    Check(ecs::MovementSystem::GetCurrentMoveDuration(e) == 0, "stopped duration read");
    g_registry.destroy(e);
    const auto recycled = Entity(ecs::SpatialKind::Character);
    g_registry.emplace<ecs::MovementState>(recycled).moveDuration = 999;
    Check(ecs::MovementSystem::GetCurrentMoveDuration(e) == 0 &&
        ecs::MovementSystem::GetCurrentMoveDuration(recycled) == 999, "duration read crossed entity generation");
    const auto item = Entity();
    g_registry.emplace<ecs::MovementState>(item).moveDuration = 999;
    Check(ecs::MovementSystem::GetCurrentMoveDuration(item) == 0 &&
        ecs::MovementSystem::GetCurrentMoveDuration(entt::null) == 0, "invalid duration source accepted");
}

void NativeMovementCommands() {
    Reset(); MapFixture map;
    const auto e = Moving(100, 100, 101, 100);
    ecs::MovementSystem::SyncDestinationClear(e);
    g_registry.remove<ecs::MovementState>(e); // Exercise command bootstrap too.
    motionSettings[e].attached = true;
    Check(ecs::MovementSystem::Goto(e, 400, 500), "native Goto rejected fresh target");
    g_registry.get<ecs::MovementState>(e).walkPreference = true;
    const auto initial = g_registry.get<ecs::MovementState>(e);
    Check(initial.moveStartTime == 123456 && initial.moveDuration == 1666 &&
        g_registry.get<ecs::MovementDestination>(e).x == 400 &&
        AIHelpers::GetStateDuration(e) == 4, "Goto target/timing/AI commit missing");
    Check(!ecs::MovementSystem::Goto(e, 400, 500) &&
        g_registry.get<ecs::MovementState>(e).commandRevision == initial.commandRevision,
        "repeated target restarted movement");
    Check(!ecs::MovementSystem::Goto(e, 100, 100) &&
        g_registry.get<ecs::MovementDestination>(e).x == 400, "current-position no-op changed target");

    motionSettings[e].movePoint = 200;
    ecs::MovementSystem::CalculateMoveDuration(e);
    Check(g_registry.get<ecs::MovementState>(e).moveDuration == 833, "speed-change duration calculation");
    Check(ecs::MovementSystem::GetMoveSpeed(e) == 600.0f, "native effective movement speed");
    g_registry.emplace<ecs::CombatActiveTag>(e);
    g_registry.emplace<ecs::CombatTarget>(e, Entity(ecs::SpatialKind::Character), 0u);
    ecs::MovementSystem::Stop(e);
    Check(!g_registry.any_of<ecs::MovementDestination, ecs::CombatActiveTag, ecs::CombatTarget>(e) &&
        g_registry.get<ecs::MovementState>(e).moveDuration == 0 &&
        g_registry.get<ecs::MovementState>(e).moveStartTime == 0 &&
        g_registry.get<ecs::MovementState>(e).walkPreference && transitions.empty(),
        "native PC stop did not clear combat/timing or changed preference/AI");
    ecs::MovementSystem::CalculateMoveDuration(e);
    Check(g_registry.get<ecs::MovementState>(e).moveDuration == 0, "stationary duration is nonzero");

    const auto clone = Moving(500, 500, 800, 500);
    // TagPC without an attached descriptor follows legacy NPC stop semantics.
    ecs::MovementSystem::Stop(clone);
    Check(transitions.size() == 1 && transitions.back().first == clone &&
        transitions.back().second == ecs::AIFSMState::Idle, "descriptor-free clone stop skipped AI");
    for (auto invalid : {entt::entity(entt::null), Entity()}) {
        Check(!ecs::MovementSystem::Goto(invalid, 1, 1), "invalid/item Goto accepted");
        ecs::MovementSystem::Stop(invalid);
    }
    const auto stale = e; g_registry.destroy(e);
    Check(!ecs::MovementSystem::Goto(stale, 5, 5), "stale Goto accepted");
    ecs::MovementSystem::Stop(stale);
}
void NativeMotionSelection() {
    Reset(); MapFixture map;
    const auto e = Moving(100, 100, 101, 100);
    const auto weapon = Entity();
    motionSettings[e].weapon = weapon;
    const std::pair<uint8_t, uint32_t> modes[] = {
        {WEAPON_SWORD, MOTION_MODE_ONEHAND_SWORD}, {WEAPON_TWO_HANDED, MOTION_MODE_TWOHAND_SWORD},
        {WEAPON_DAGGER, MOTION_MODE_DUALHAND_SWORD}, {WEAPON_BOW, MOTION_MODE_BOW},
        {WEAPON_BELL, MOTION_MODE_BELL}, {WEAPON_FAN, MOTION_MODE_FAN}
    };
    for (auto [subtype, mode] : modes) {
        weaponProtos[weapon].bSubType = subtype;
        Check(ecs::MovementSystem::GetMotionMode(e) == mode, "weapon motion mode mismatch");
    }
    motionSettings[e].polymorphed = true;
    Check(ecs::MovementSystem::GetMotionMode(e) == MOTION_MODE_GENERAL, "polymorph retained weapon mode");
    motionSettings[e].polymorphed = false;
    weaponProtos.erase(weapon);
    Check(ecs::MovementSystem::GetMotionMode(e) == MOTION_MODE_GENERAL, "missing weapon proto mode");
    g_registry.destroy(weapon);
    Check(ecs::MovementSystem::GetMotionMode(e) == MOTION_MODE_GENERAL, "stale weapon mode");
    motionSettings[e].weapon = entt::null;
    motionSettings[e].race = 1;
    motionSettings[e].attached = true;
    TestMotion run(2, 800), walk(2, 300), mounted(1, 900), horse(1, 700);
    motions[{1, MAKE_MOTION_KEY(MOTION_MODE_GENERAL, MOTION_RUN)}] = &run;
    motions[{1, MAKE_MOTION_KEY(MOTION_MODE_GENERAL, MOTION_WALK)}] = &walk;
    Check(ecs::MovementSystem::GetMoveMotionSpeed(e) == 400, "run motion speed");
    g_registry.get<ecs::MovementState>(e).isNowWalking = true;
    Check(ecs::MovementSystem::GetMoveMotionSpeed(e) == 150, "walk motion speed");
    g_registry.get<ecs::MovementState>(e).isNowWalking = false;
    motionSettings[e].stamina = 0;
    Check(ecs::MovementSystem::GetMoveMotionSpeed(e) == 150, "exhausted PC did not walk");
    motionSettings[e].attached = false;
    Check(ecs::MovementSystem::GetMoveMotionSpeed(e) == 400, "descriptor-free character used PC walk motion");
    motionSettings[e].attached = true; motionSettings[e].stamina = 100;
    motionSettings[e].mount = 20201;
    motions[{20201, MAKE_MOTION_KEY(MOTION_MODE_GENERAL, MOTION_RUN)}] = &mounted;
    Check(ecs::MovementSystem::GetMoveMotionSpeed(e) == 900, "mount motion speed");
    motions.erase({20201, MAKE_MOTION_KEY(MOTION_MODE_GENERAL, MOTION_RUN)});
    motions[{1, MAKE_MOTION_KEY(MOTION_MODE_HORSE, MOTION_RUN)}] = &horse;
    Check(ecs::MovementSystem::GetMoveMotionSpeed(e) == 700, "horse-mode fallback");
    motions.clear();
    Check(ecs::MovementSystem::GetMoveMotionSpeed(e) == 300, "missing mount motion fallback");
    motionSettings[e].mount = 0;
    TestMotion zeroDuration(0, 100), zeroDistance(1, 0), negative(1, -100),
        infinite(std::numeric_limits<float>::infinity(), 100), nan(1, std::numeric_limits<float>::quiet_NaN());
    for (const auto* bad : {&zeroDuration, &zeroDistance, &negative, &infinite, &nan}) {
        motions[{1, MAKE_MOTION_KEY(MOTION_MODE_GENERAL, MOTION_RUN)}] = bad;
        Check(ecs::MovementSystem::GetMoveMotionSpeed(e) == 300, "malformed motion introduced invalid speed");
    }
    motions.clear();
    ecs::MovementSystem::SyncDestinationWrite(e, INT32_MIN, INT32_MAX);
    motionSettings[e].movePoint = INT64_MIN;
    ecs::MovementSystem::CalculateMoveDuration(e);
    Check(g_registry.get<ecs::MovementState>(e).moveDuration == INT_MAX,
        "extreme duration overflowed rather than saturating");
    motionSettings[e].movePoint = INT64_MAX;
    Check(std::abs(ecs::MovementSystem::GetMoveSpeed(e) - 30000.0f / 28) < 0.01f,
        "movement point upper limit changed");
#ifdef ENABLE_MELEY_LAIR
    motionSettings[e].attached = false; motionSettings[e].race = 6193;
    Check(!ecs::MovementSystem::Goto(e, 200, 200) && ecs::MovementSystem::GetMoveMotionSpeed(e) == 100,
        "Meley immobility rule lost");
#endif
#ifdef ENABLE_ANCIENT_PYRAMID
    motionSettings[e].attached = false; motionSettings[e].race = PYRAMID_BOSSVNUM;
    Check(!ecs::MovementSystem::Goto(e, 200, 200), "pyramid immobility rule lost");
#endif
#ifdef __DEFENSE_WAVE__
    for (uint32_t race = 3960; race <= 3962; ++race) {
        motionSettings[e].attached = false; motionSettings[e].race = race;
        Check(!ecs::MovementSystem::Goto(e, 200, 200), "defense-wave immobility rule lost");
    }
#endif
}
void MovementCommandReentry() {
    Reset(); MapFixture map;
    const auto e = Moving(100, 100, 101, 100);
    ecs::MovementSystem::SyncDestinationClear(e);
    Callback arrival {[&](entt::registry& reg, entt::entity changed) {
        if (changed != e) return;
        Check(reg.get<ecs::MovementState>(e).moveDuration == 1000 &&
            reg.get<ecs::MovementDestination>(e).x == 400, "destination observer saw incomplete timing");
        ecs::MovementSystem::Stop(e);
    }};
    entt::scoped_connection connection =
        g_registry.on_construct<ecs::MovementDestination>().connect<&Callback::Run>(arrival);
    Check(!ecs::MovementSystem::Goto(e, 400, 100) &&
        !g_registry.all_of<ecs::MovementDestination>(e) &&
        g_registry.get<ecs::MovementState>(e).moveDuration == 0, "Goto overwrote observer stop");
    connection.release();

    g_registry.remove<ecs::DirtyTag>(e);
    Callback speedChange {[&](entt::registry&, entt::entity changed) {
        if (changed == e) motionSettings[e].movePoint = 200;
    }};
    connection = g_registry.on_construct<ecs::DirtyTag>().connect<&Callback::Run>(speedChange);
    Check(ecs::MovementSystem::Goto(e, 700, 100) &&
        g_registry.get<ecs::MovementState>(e).moveDuration == 1000,
        "Goto used speed from before preparation callback");
    connection.release();
    motionSettings[e].movePoint = 100;
    ecs::MovementSystem::SyncDestinationClear(e);

    g_registry.remove<ecs::DirtyTag>(e);
    Callback removePrepared {[&](entt::registry& reg, entt::entity changed) {
        if (changed == e) reg.remove<ecs::AIState>(e);
    }};
    connection = g_registry.on_construct<ecs::DirtyTag>().connect<&Callback::Run>(removePrepared);
    Check(!ecs::MovementSystem::Goto(e, 400, 100) && !g_registry.all_of<ecs::MovementDestination>(e),
        "Goto used scheduler removed by later preparation");
    connection.release();

    g_registry.remove<ecs::AIState>(e);
    bool nested = false;
    Callback prepare {[&](entt::registry&, entt::entity changed) {
        if (changed == e && !nested) {
            nested = true;
            Check(ecs::MovementSystem::Goto(e, 700, 100), "nested Goto failed");
        }
    }};
    connection = g_registry.on_construct<ecs::AIState>().connect<&Callback::Run>(prepare);
    Check(!ecs::MovementSystem::Goto(e, 400, 100) && nested &&
        g_registry.get<ecs::MovementDestination>(e).x == 700 &&
        g_registry.get<ecs::MovementState>(e).moveDuration == 2000, "preparation replaced nested command");
    connection.release();

    g_registry.emplace<ecs::CombatActiveTag>(e);
    Callback stop {[&](entt::registry&, entt::entity changed) {
        if (changed == e) Check(ecs::MovementSystem::Goto(e, 1000, 100), "retarget during stop");
    }};
    connection = g_registry.on_destroy<ecs::CombatActiveTag>().connect<&Callback::Run>(stop);
    ecs::MovementSystem::Stop(e);
    Check(g_registry.get<ecs::MovementDestination>(e).x == 1000 &&
        g_registry.get<ecs::MovementState>(e).moveDuration == 3000, "old Stop erased new Goto");
    connection.release();

    Callback clear {[&](entt::registry& reg, entt::entity changed) {
        if (changed == e) reg.get<ecs::MovementState>(e).moveDuration = 777;
    }};
    connection = g_registry.on_destroy<ecs::MovementDestination>().connect<&Callback::Run>(clear);
    ecs::MovementSystem::SyncDestinationClear(e);
    Check(g_registry.get<ecs::MovementState>(e).moveDuration == 777, "sync clear overwrote observer timing");
    connection.release();

    g_registry.remove<ecs::DirtyTag>(e);
    entt::entity replacement = entt::null;
    Callback destroy {[&](entt::registry& reg, entt::entity changed) {
        if (changed != e) return;
        reg.destroy(e);
        replacement = Entity(ecs::SpatialKind::Character);
    }};
    connection = g_registry.on_construct<ecs::DirtyTag>().connect<&Callback::Run>(destroy);
    Check(!ecs::MovementSystem::Goto(e, 400, 100) && !g_registry.valid(e) &&
        g_registry.valid(replacement) && !g_registry.all_of<ecs::MovementDestination>(replacement),
        "preparation wrote through recycled handle");
}
// The pending warp and the saved exit. Each used to be a pair of CHARACTER
// fields beside these components: the login, the channel switch, WarpSet and
// WarpEnd wrote only the fields, so anything reading the component saw a value
// left over from an unrelated write. WarpSet, WarpEnd and ExitToSavedLocation
// need a map table, a descriptor and a P2P link, so they are not reachable
// here; what is pinned is the storage every one of them now shares.
void NativeWarpLocations() {
    Reset();
    const auto e = Entity(ecs::SpatialKind::Character);

    Check(ecs::MovementSystem::GetWarpLocation(e).x == 0 &&
        ecs::MovementSystem::GetWarpLocation(e).y == 0 &&
        ecs::MovementSystem::GetWarpLocation(e).mapIndex == 0, "a fresh character has a pending warp");
    Check(ecs::MovementSystem::GetExitLocation(e).mapIndex == 0, "a fresh character has a saved exit");

    // The dungeons and the quest bindings pass map cells.
    ecs::MovementSystem::SetWarpLocation(e, 42, 300, 400);
    auto warp = ecs::MovementSystem::GetWarpLocation(e);
    Check(warp.x == 30000 && warp.y == 40000 && warp.mapIndex == 42, "cell coordinates were not scaled");

    // WarpSet and the login path pass world units.
    ecs::MovementSystem::SetWarpLocationRaw(e, 7, 300, 400);
    warp = ecs::MovementSystem::GetWarpLocation(e);
    Check(warp.x == 300 && warp.y == 400 && warp.mapIndex == 7, "world coordinates were scaled again");

    // WarpEnd clears it through the same door it was written through.
    ecs::MovementSystem::SetWarpLocationRaw(e, 0, 0, 0);
    warp = ecs::MovementSystem::GetWarpLocation(e);
    Check(warp.x == 0 && warp.y == 0 && warp.mapIndex == 0, "the pending warp survived being cleared");

    placements[e] = {700, 800, 13};
    ecs::MovementSystem::SaveExitLocation(e);
    const auto exit = ecs::MovementSystem::GetExitLocation(e);
    Check(exit.x == 700 && exit.y == 800 && exit.mapIndex == 13, "the exit was not saved where it is read");
    Check(ecs::MovementSystem::GetWarpLocation(e).mapIndex == 0, "saving an exit set a pending warp");

    // Neither accessor may create a component, and neither may touch a handle
    // that no longer names a character.
    const auto bare = Entity(ecs::SpatialKind::Character);
    Check(ecs::MovementSystem::GetWarpLocation(bare).mapIndex == 0 &&
        !g_registry.all_of<ecs::WarpPosition>(bare), "reading a warp created one");
    Check(ecs::MovementSystem::GetExitLocation(bare).mapIndex == 0 &&
        !g_registry.all_of<ecs::ExitPosition>(bare), "reading an exit created one");

    g_registry.destroy(e);
    Check(ecs::MovementSystem::GetWarpLocation(e).mapIndex == 0, "stale handle reported a pending warp");
    Check(ecs::MovementSystem::GetExitLocation(e).mapIndex == 0, "stale handle reported a saved exit");
    ecs::MovementSystem::SetWarpLocation(e, 1, 2, 3);
    ecs::MovementSystem::SetWarpLocationRaw(e, 1, 2, 3);
    ecs::MovementSystem::SaveExitLocation(e);
    Check(ecs::MovementSystem::GetWarpLocation(entt::null).mapIndex == 0, "the null handle carried a warp");
    Check(ecs::MovementSystem::GetExitLocation(entt::null).mapIndex == 0, "the null handle carried an exit");
}

// Show's height sentinel. The wrapper resolved LONG_MAX for its own
// SyncPositionComponents call and then handed the raw value to CHARACTER::Show,
// which synced a second time - so every caller that let z default wrote
// LONG_MAX into Position.z, and every read of the character's height after
// that returned it. There is one resolution now, before anything reads z.
void ShowHeightSentinel() {
    Check(ecs::MovementSystem::ResolveShowHeight(LONG_MAX, 250) == 250,
        "the sentinel was written through as a height");
    Check(ecs::MovementSystem::ResolveShowHeight(0, 250) == 0,
        "an explicit ground height was overridden");
    Check(ecs::MovementSystem::ResolveShowHeight(-120, 250) == -120,
        "an explicit height below zero was overridden");
    Check(ecs::MovementSystem::ResolveShowHeight(LONG_MAX, 0) == 0,
        "the sentinel did not resolve against a zero height");
    static_assert(ecs::MovementSystem::ResolveShowHeight(LONG_MAX, 7) == 7);
}

void NativeAIScheduleStorage() {
    Reset();
    const auto e = Entity(ecs::SpatialKind::Character);
    Check(AIHelpers::GetStateDuration(e) == 1 && AIHelpers::GetNextStatePulse(e) == 0,
        "scheduler bootstrap defaults");
    AIHelpers::SetStateDuration(e, 4);
    AIHelpers::SetNextStatePulse(e, 900);
    Check(g_registry.get<ecs::AIState>(e).stateDuration == 4 &&
        g_registry.get<ecs::AIState>(e).nextStatePulse == 900, "AI scheduler did not use components");
    g_registry.remove<ecs::AIState>(e);
    Callback callback {[&](entt::registry& reg, entt::entity changed) {
        if (changed == e) {
            Check(reg.get<ecs::AIState>(e).stateDuration == 7, "scheduler published default instead of requested state");
            AIHelpers::SetStateDuration(e, 11);
        }
    }};
    entt::scoped_connection connection = g_registry.on_construct<ecs::AIState>().connect<&Callback::Run>(callback);
    AIHelpers::SetStateDuration(e, 7);
    Check(AIHelpers::GetStateDuration(e) == 11, "scheduler overwrote reentrant duration");
    connection.release();

    g_registry.remove<ecs::AIState>(e);
    Callback remove {[&](entt::registry& reg, entt::entity changed) {
        if (changed == e) reg.remove<ecs::AIState>(e);
    }};
    connection = g_registry.on_construct<ecs::AIState>().connect<&Callback::Run>(remove);
    AIHelpers::SetStateDuration(e, 7);
    Check(!g_registry.all_of<ecs::AIState>(e), "duration setter recreated removed scheduler");
    connection.release();

    Callback destroy {[&](entt::registry& reg, entt::entity changed) {
        if (changed == e) {
            Check(reg.get<ecs::AIState>(e).nextStatePulse == 123, "pulse observer saw incomplete state");
            reg.destroy(e);
        }
    }};
    connection = g_registry.on_construct<ecs::AIState>().connect<&Callback::Run>(destroy);
    AIHelpers::SetNextStatePulse(e, 123);
    Check(!g_registry.valid(e), "pulse setter recreated retired owner");
    connection.release();
    AIHelpers::SetStateDuration(e, 3); AIHelpers::SetNextStatePulse(e, 100);
    Check(!g_registry.valid(e), "scheduler recreated stale entity");
}

}
int main() {
    try {
        SECTREE_MANAGER maps; CHARACTER_MANAGER characters; DESC_MANAGER descriptors; CMotionManager motionManager;
        ecs::VisibilitySystem::Init(g_registry);
        MembershipAndSnapshots(); VisibilityRoundTrip(); ViewCallbacks(); PreparationAndPCs();
        LifetimeAndObservers(); RemovalCallbacksAndTeardown(); PreparationMutationAndIteration();
        NativeMovement(); MovementVisibilityAndBounds(); MovementCallbackLifetime();
        MovementCallbackRetarget(); MovementArrivalAndPackets();
        NativeAnimationPackets(); NativeMovementDurationReads(); NativeMovementCommands();
        NativeMotionSelection(); MovementCommandReentry(); NativeAIScheduleStorage(); NativeWarpLocations(); ShowHeightSentinel();
        ecs::VisibilitySystem::Shutdown(g_registry);
        std::cout << "Spatial checks passed: " << checks << '\n'; return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
