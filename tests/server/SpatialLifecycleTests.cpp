#include "../../SRC/Server/GameServer/core/stdafx.h"
#include "../../SRC/Server/GameServer/world/sectree_manager.h"
#include "../../SRC/Server/GameServer/entity/char_manager.h"
#include "../../SRC/Server/GameServer/network/desc_manager.h"
#include "../../SRC/Server/GameServer/ecs/SpatialHelpers.hpp"
#include "../../SRC/Server/GameServer/ecs/EventDispatcher.hpp"
#include "../../SRC/Server/GameServer/ecs/events.hpp"
#include "../../SRC/Server/GameServer/ecs/services/SpatialService.hpp"
#include "../../SRC/Server/GameServer/ecs/services/VisibilityService.hpp"
#include "../../SRC/Server/GameServer/ecs/services/EntityNetworkDispatch.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/VisibilitySystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/MovementSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/CharacterFrame.hpp"
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
#include "../../SRC/Server/GameServer/core/packet.h"
#include "../../SRC/Server/GameServer/quest/questmanager.h"
#include "../../SRC/Server/GameServer/dungeon/dungeon.h"
#include "../../SRC/Server/GameServer/social/party.h"
#include "../../SRC/Server/GameServer/entity/motion.h"
#include "../../SRC/Server/GameServer/ecs/systems/AffectSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PointSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ItemSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/InventorySystem.hpp"
#include "../../SRC/Server/GameServer/ecs/components/item_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/identity_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/vital_components.hpp"
#include "../../SRC/Server/GameServer/world/event_queue.h"
#include "../../SRC/Server/GameServer/combat/affect.h"
#include "../../SRC/Server/GameServer/item/item_manager.h"
#include "../../SRC/Server/GameServer/ecs/ItemRegistry.hpp"
#include "../../SRC/Server/GameServer/ecs/CBuildingRegistry.hpp"
#include <Base/attribute.h>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <limits>
#include "../../SRC/Server/GameServer/pet/mount_inventory_helper.h"
#include "../../SRC/Server/GameServer/world/map_location.h"
#include "../../SRC/Server/GameServer/ecs/EcsDiagnostics.hpp"
#include "../../SRC/Server/GameServer/network/desc.h"
#include "../../SRC/Server/GameServer/core/log.h"
#include "../../SRC/Server/GameServer/network/p2p.h"
#include "../../SRC/Server/GameServer/social/new_switchbot.h"
#include "../../SRC/Server/GameServer/ecs/systems/SkillSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/NetworkSyncSystem.hpp"
#include "../../SRC/Server/GameServer/world/building.h"

entt::registry g_registry;
entt::dispatcher g_dispatcher;
int VIEW_RANGE = 5000;
int VIEW_BONUS_RANGE = 500;
namespace {
int checks = 0;
uint32_t clockNow = 123456;
HEART recoveryHeart {};
struct RecoveryProbe {
    TMobTable mob {};
    entt::entity dungeon { entt::null };
    bool dead = false, stun = false, poison = false, missingMob = false;
    int64_t bonus = 0;
};
// Only the real Show integration fixtures may reach its gameplay leaf seams.
struct ShowProbe {};
void CheckShow(entt::entity e) {
    if (!g_registry.valid(e) || !g_registry.all_of<ShowProbe>(e))
        throw std::runtime_error("Show service received unexpected entity");
}
std::function<void(entt::entity, int)> onRecovery;
int recoveryChanges = 0, recoveryNotices = 0;
RecoveryProbe& Recovery(entt::entity e) {
    if (!g_registry.valid(e) || !g_registry.all_of<RecoveryProbe>(e))
        throw std::runtime_error("recovery service received stale/unexpected entity");
    return g_registry.get<RecoveryProbe>(e);
}
void RecoveryCallback(entt::entity e, int stage) {
    Recovery(e); if (onRecovery) { auto fn = onRecovery; fn(e, stage); }
}
void Recovered(const ecs::EvRecovery& ev) { ++recoveryNotices; RecoveryCallback(ev.entity, 3); }
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
std::vector<std::pair<entt::entity, entt::entity>> movementRecipients;
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
std::unordered_map<entt::entity, DESC*> inputDescriptors;
std::map<entt::entity, TItemTable> weaponProtos;
// Baked movement speeds (px/s) by (vnum, walking); a missing entry is unknown.
std::map<std::pair<uint32_t, bool>, float> moveSpeeds;
// Only entities a test places have a position; every other entity keeps the
// "must not be asked" guarantee the plain Unexpected() doubles gave.
struct Placement { int32_t x, y, mapIndex; };
std::map<entt::entity, Placement> placements;

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
    clockNow = 123456;
    onRecovery = {}; recoveryChanges = recoveryNotices = 0;
    event_destroy(); recoveryHeart.pulse = 0; recoveryHeart.passes_per_sec = 25;
    onPacket = {}; onRetire = {}; packets.clear(); awake.clear(); retired = 0;
    movementPackets.clear(); movementRecipients.clear(); animationPackets.clear(); onAnimation = {};
    transitions.clear(); motionSettings.clear(); weaponProtos.clear();
    inputDescriptors.clear();
    moveSpeeds.clear(); placements.clear(); g_registry.clear();
}
struct Callback {
    std::function<void(entt::registry&, entt::entity)> fn;
    void Run(entt::registry& reg, entt::entity e) { fn(reg, e); }
};
}

LPHEART thecore_heart = &recoveryHeart;
void ContinueOnFatalError() { Unexpected(); }

// A private-map sector shares the origin sector's attribute; the shared
// ownership must keep it alive when the origin map goes first.
void AttributeSharing() {
    auto* origin = new SECTREE;
    auto* clone = new SECTREE;
    origin->BindAttribute(new CAttribute(nullptr, 0, 0));
    clone->CloneAttribute(origin);
    CAttribute* shared = origin->GetAttributePtr();
    Check(shared && clone->GetAttributePtr() == shared, "private-map clone did not share the attribute");
    delete origin;
    Check(clone->GetAttributePtr() == shared, "attribute died with the origin sector");
    delete clone;
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
// The constructor pair is only referenced by the attribute-sharing check; the
// accessors stay fail-fast seams because production never reads them here.
CAttribute::CAttribute(uint32_t*, uint32_t, uint32_t) {}
CAttribute::CAttribute(uint32_t, uint32_t) {}
uint32_t CAttribute::Get(uint32_t, uint32_t) { Unexpected(); }
void CAttribute::Set(uint32_t, uint32_t, uint32_t) { Unexpected(); }
void CAttribute::Remove(uint32_t, uint32_t, uint32_t) { Unexpected(); }
CHARACTER_MANAGER::CHARACTER_MANAGER() {}
CHARACTER_MANAGER::~CHARACTER_MANAGER() {}
// Out-of-line key functions emit RTTI required by singleton's guard on FreeBSD.
// These service doubles are never connected to SQL or the live quest runtime.
CSwitchbotManager::~CSwitchbotManager() = default;
quest::CQuestManager::~CQuestManager() = default;
quest::NPC::~NPC() = default;
LogManager::~LogManager() = default;
CAsyncSQL::~CAsyncSQL() = default;
CSemaphore::~CSemaphore() = default;
bool CHARACTER_MANAGER::AddToStateList(entt::entity e) { Check(g_registry.valid(e), "wake stale NPC"); return awake.insert(e).second; }
void CHARACTER_MANAGER::RemoveFromStateList(entt::entity e) { awake.erase(e); }
void CHARACTER_MANAGER::DestroyCharacter(entt::entity e) { g_registry.destroy(e); }
DESC_MANAGER::DESC_MANAGER() {}
DESC_MANAGER::~DESC_MANAGER() {}
void DESC_MANAGER::DestroyDesc(LPDESC, bool) { Unexpected(); }
DESC* ecs::PlayerRuntime::GetDesc(entt::entity e) {
    if (auto it = inputDescriptors.find(e); it != inputDescriptors.end()) return it->second;
    // Boolean-only attachment seam: no transport object is dereferenced here.
    return motionSettings[e].attached ? reinterpret_cast<DESC*>(uintptr_t(1)) : nullptr;
}
// Gameplay leaf services are isolated here; movement, packet encoding,
// SetPosition, sectree relocation and visibility run their production code.
uint32_t get_dword_time() { return clockNow; }
bool ecs::PlayerRuntime::IsPC(entt::entity e) { return g_registry.all_of<ecs::TagPC>(e); }
bool ecs::PlayerRuntime::IsStone(entt::entity e) { return g_registry.all_of<ecs::StoneAITag>(e); }
unsigned char ecs::PlayerRuntime::GetCharType(entt::entity) { Unexpected(); }
void ecs::PlayerRuntime::MonsterLog(entt::entity e, const char*) {
    if (g_registry.all_of<RecoveryProbe>(e)) RecoveryCallback(e, 5);
}
void ecs::PlayerRuntime::CancelCharEvent(entt::entity e, CharEvent) { Check(g_registry.valid(e), "cancel event on stale entity"); }

// Show runs real placement and visibility; unrelated gameplay leaves are
// admitted only for its explicitly marked fixtures, never arbitrary callers.
UINT g_start_map[4] = {};
void BroadcastNotice(const char*, bool) { Unexpected(); }
std::string ecs::diag::Describe(entt::entity) { Unexpected(); }
int32_t ecs::PlayerRuntime::GetZ(entt::entity e) { CheckShow(e); return g_registry.get<ecs::Position>(e).z; }
bool ecs::PlayerRuntime::IsNPC(entt::entity) { Unexpected(); }
bool ecs::PlayerRuntime::IsDetailLog(entt::entity) { Unexpected(); }
int64_t ecs::PlayerRuntime::GetMaxStamina(entt::entity e) { CheckShow(e); return motionSettings[e].stamina; }
bool AffectSystem::StartAffectEvent(entt::entity) { Unexpected(); }
void AffectSystem::SetFlag(entt::entity, uint32_t, bool) { Unexpected(); }
ecs::MobInstanceState* CombatSystem::MobState(entt::entity e) { CheckShow(e); return g_registry.try_get<ecs::MobInstanceState>(e); }
void CombatSystem::SetValidComboInterval(entt::entity e, int value) { CheckShow(e); Check(value == 0, "Show combo reset"); }
void CombatSystem::SendLeaderboardData(entt::entity) { Unexpected(); }
void CombatSystem::SendLeaderboardDataGuild(entt::entity) { Unexpected(); }
void CombatSystem::SendLeaderboardDataSkillMob(entt::entity, entt::entity) { Unexpected(); }
void MountSystem::UpdateMountInventoryCountOverhead(entt::entity e, entt::entity viewer) { CheckShow(e); CheckShow(viewer); }
void CombatSystem::UpdateKillerMode(entt::entity e) { RecoveryCallback(e, 1); }
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
bool InventorySystem::CanHandleItems(entt::entity, bool, bool) { Unexpected(); }
bool ecs::PlayerRuntime::IsHack(entt::entity, bool, bool, int) { Unexpected(); }
bool ecs::PlayerRuntime::IsHack(entt::entity, bool, bool) { Unexpected(); }
bool CMapLocation::Get(int, int, int&, uint32_t&, uint16_t&) { Unexpected(); }
SECTREE_MAP* SECTREE_MANAGER::GetMap(int) { Unexpected(); }
int SECTREE_MANAGER::GetMapIndex(int, int) { Unexpected(); }
void CSwitchbotManager::P2PSendSwitchbot(uint32_t, uint16_t) { Unexpected(); }
void CSwitchbotManager::SetIsWarping(uint32_t, bool) { Unexpected(); }
void DESC::Packet(const void*, int) { Unexpected(); }
// Real descriptor/input objects for the packet-to-movement integration case.
// Only their transport, dispatch and clock-sync services are replaced.
CPacketInfo::CPacketInfo() = default;
CPacketInfo::~CPacketInfo() = default;
CPacketInfoCG::CPacketInfoCG() = default;
CPacketInfoCG::~CPacketInfoCG() = default;
CInputProcessor::CInputProcessor() : m_pPacketInfo(nullptr), m_iBufferLeft(0) {}
bool CInputProcessor::Process(LPDESC, const void*, int, int&) { Unexpected(); }
void CInputProcessor::Handshake(LPDESC, const char*) { Unexpected(); }
CInputHandshake::CInputHandshake() : m_pMainPacketInfo(nullptr) {}
CInputHandshake::~CInputHandshake() = default;
int CInputHandshake::Analyze(LPDESC, uint8_t, const char*) { Unexpected(); }
int CInputLogin::Analyze(LPDESC, uint8_t, const char*) { Unexpected(); }
int CInputMain::Analyze(LPDESC, uint8_t, const char*) { Unexpected(); }
int CInputDead::Analyze(LPDESC, uint8_t, const char*) { Unexpected(); }
DESC::DESC() = default;
DESC::~DESC() = default;
void DESC::Destroy() { Unexpected(); }
void DESC::SetPhase(int) { Unexpected(); }
void DESC::OnInput() { Unexpected(); }
void DESC::OnPeerGone() { Unexpected(); }
void DESC::OnBytesWritten(std::size_t) { Unexpected(); }
bool DESC::IsHandshaking() { return true; }
uint32_t DESC::GetClientTime() { return get_dword_time(); }
bool DESC::DelayedDisconnect(int) { Unexpected(); }
bool SkillSystem::IsUsableSkillMotion(entt::entity, uint32_t) { Unexpected(); }
// Other entry points sharing movement_input.cpp are outside this fixture.
void CombatSystem::SetTarget(entt::entity, entt::entity) { Unexpected(); }
void CombatSystem::FlyTarget(entt::entity, uint32_t, int32_t, int32_t, uint8_t) { Unexpected(); }
bool NetworkSyncSystem::SetSyncOwner(entt::entity, entt::entity, bool) { Unexpected(); }
void ecs::PlayerRuntime::OnClick(entt::entity, entt::entity) { Unexpected(); }
void ecs::PlayerRuntime::SetLastSyncTime(entt::entity, const timeval&) { Unexpected(); }
const timeval& ecs::PlayerRuntime::GetLastSyncTime(entt::entity) { Unexpected(); }
int ecs::PlayerRuntime::GetSyncHackCount(entt::entity) { Unexpected(); }
void ecs::PlayerRuntime::SetSyncHackCount(entt::entity, int) { Unexpected(); }
bool ecs::PlayerRuntime::IsValid(entt::entity) { Unexpected(); }
entt::entity building::CManager::FindObjectByVID(uint32_t) { Unexpected(); }
entt::entity CHARACTER_MANAGER::FindEntity(uint32_t) { Unexpected(); }
void LogManager::HackLog(const char*, entt::entity) { Unexpected(); }
bool ITEM_MANAGER::CreateDropItemVector(entt::entity, entt::entity, std::vector<TargetInfoItem>&) { Unexpected(); }
timeval* timediff(const timeval*, const timeval*) { Unexpected(); }
#ifdef _WIN32
void gettimeofday(timeval*, timezone*) { Unexpected(); }
#endif
void LogManager::CharLog(entt::entity, uint32_t, const char*, const char*) { Unexpected(); }
void P2P_MANAGER::Send(const void*, int, LPDESC) { Unexpected(); }
uint8_t ecs::PlayerRuntime::GetEmpire(entt::entity) { Unexpected(); }
uint32_t ecs::PlayerRuntime::GetPlayerID(entt::entity) { Unexpected(); }
bool ecs::PlayerRuntime::IsGoto(entt::entity) { Unexpected(); }
bool ecs::PlayerRuntime::IsWarp(entt::entity) { Unexpected(); }
entt::entity ecs::SocialSystem::GetDungeon(entt::entity e) { return Recovery(e).dungeon; }
// MovementSystem asks for the party on a sector change now that CHARACTER
// has no getter of its own.
entt::entity ecs::SocialSystem::GetParty(entt::entity) { Unexpected(); }
SECTREE* ecs::PlayerRuntime::GetSectree(entt::entity e) { CheckShow(e); return ecs::SectorOf(g_registry, e); }
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
    if (g_registry.all_of<ShowProbe>(e))
        for (const auto viewer : ecs::VisibilityService::GetViewersOf(g_registry, e))
            if (viewer != except && ecs::PlayerRuntime::GetDesc(viewer))
                movementRecipients.emplace_back(e, viewer);
}
// Fail-fast link seams for the still-unmigrated functions in MovementSystem.cpp.
// None may be reached by the native tick or packet tests.
void ecs::ChatSystem::Send(entt::entity, uint8_t, const char*, ...) { Unexpected(); }
entt::entity ecs::SocialSystem::GetMyShop(entt::entity e) {
    if (inputDescriptors.contains(e)) return entt::null;
    Unexpected();
}
bool AffectSystem::IsPolymorphed(entt::entity e) { return motionSettings[e].polymorphed; }
uint32_t ecs::PlayerRuntime::GetPacketVID(entt::entity e) {
    if (inputDescriptors.contains(e)) return g_registry.get<ecs::VIDComponent>(e).value;
    Unexpected();
}
uint32_t ecs::PlayerRuntime::GetRaceNum(entt::entity e) { return motionSettings[e].race; }
int ecs::PlayerRuntime::GetStamina(entt::entity e) { return motionSettings[e].stamina; }
uint32_t MountSystem::GetMountVnum(entt::entity e) { return motionSettings[e].mount; }
bool MountSystem::IsRiding(entt::entity e) { return motionSettings[e].mount != 0; }
std::string_view ecs::PlayerRuntime::GetName(entt::entity e) {
    if (inputDescriptors.contains(e)) return "input-player";
    CheckShow(e); return g_registry.get<ecs::PlayerName>(e).value;
}
int32_t ecs::PlayerRuntime::GetMapIndex(entt::entity e) {
    if (inputDescriptors.contains(e) || g_registry.all_of<ShowProbe>(e)) return g_registry.get<ecs::MapIndex>(e).value;
    auto it = placements.find(e); if (it == placements.end()) Unexpected();
    return it->second.mapIndex;
}
int32_t ecs::PlayerRuntime::GetX(entt::entity e) {
    if (inputDescriptors.contains(e)) return g_registry.get<ecs::Position>(e).x;
    auto it = placements.find(e); if (it == placements.end()) Unexpected();
    return it->second.x;
}
int32_t ecs::PlayerRuntime::GetY(entt::entity e) {
    if (inputDescriptors.contains(e)) return g_registry.get<ecs::Position>(e).y;
    auto it = placements.find(e); if (it == placements.end()) Unexpected();
    return it->second.y;
}
int ecs::PlayerRuntime::GetPosition(entt::entity) { Unexpected(); }
void CombatSystem::CheckTarget(entt::entity e) { RecoveryCallback(e, 0); }
bool CombatSystem::IsStun(entt::entity e) { return Recovery(e).stun; }
bool CombatSystem::IsDead(entt::entity e) { return inputDescriptors.contains(e) ? false : Recovery(e).dead; }
int CalculateDuration(int, int) { Unexpected(); }
namespace ecs::SessionSystem {
void FlushDelayedSaveItem(entt::entity) { Unexpected(); }
void Save(entt::entity) { Unexpected(); }
}
void CombatSystem::DistributeSP(entt::entity e, entt::entity target, int) {
    Check(e == target, "recovery SP target mismatch"); RecoveryCallback(e, 2);
}
uint64_t ecs::PlayerRuntime::GetHP(entt::entity e) { Recovery(e); return g_registry.get<ecs::Health>(e).current; }
int ecs::PointSystem::GetLimitPoint(entt::entity e, uint8_t point) {
    if (inputDescriptors.contains(e) && point == POINT_MOV_SPEED) return int(motionSettings[e].movePoint);
    Unexpected();
}
void ecs::PointSystem::Compute(entt::entity e) { CheckShow(e); }
const TMobTable* ecs::PlayerRuntime::GetMobTable(entt::entity e) {
    auto& data = Recovery(e); return data.missingMob ? nullptr : &data.mob;
}
void ecs::MovementSystem::OnMove(entt::entity, bool) { Unexpected(); }
bool AffectSystem::IsAffectFlag(entt::entity e, uint32_t flag) {
    if (inputDescriptors.contains(e) && flag == AFF_STUN) return false;
    Check(flag == AFF_POISON, "recovery read unexpected affect"); return Recovery(e).poison;
}
bool ItemSystem::IsEquipUniqueItem(entt::entity, uint32_t) { Unexpected(); }
void CombatSystem::Dead(entt::entity, entt::entity, bool) { Unexpected(); }
int DungeonSystem::GetFlag(entt::entity dungeon, std::string key) {
    Check(key == "floor", "unexpected recovery dungeon flag");
    const auto* state = g_registry.try_get<ecs::DungeonState>(dungeon);
    if (!state) return 0;
    const auto it = state->flags.find(key);
    return it != state->flags.end() ? it->second : 0;
}
// Model the relation effect at the movement/service boundary. A return-point
// setter must never reach this hook; only an actual map transfer may do so.
void ecs::SocialSystem::ClearDungeonIfOtherMap(entt::entity e, int32_t mapIndex) {
    auto* membership = g_registry.try_get<ecs::DungeonMembership>(e);
    if (!membership || !g_registry.valid(membership->dungeon)) return;
    const auto* dungeon = g_registry.try_get<ecs::DungeonState>(membership->dungeon);
    if (dungeon && dungeon->mapIndex != mapIndex) membership->dungeon = entt::null;
}
float CMotionManager::GetMoveSpeed(uint32_t vnum, bool walking) const {
    const auto it = moveSpeeds.find({vnum, walking}); return it == moveSpeeds.end() ? 0.0f : it->second;
}
float GetDegreeFromPositionXY(int32_t, int32_t, int32_t, int32_t) { Unexpected(); }
void quest::CQuestManager::AttrIn(entt::entity, entt::entity, int) { Unexpected(); }
void quest::CQuestManager::AttrOut(entt::entity, entt::entity, int) { Unexpected(); }
entt::entity ItemSystem::GetWearItem(entt::entity e, uint8_t slot) {
    Check(slot == WEAR_WEAPON, "motion read wrong wear slot"); return motionSettings[e].weapon;
}
bool ItemSystem::IsValidItem(entt::entity e) { return g_registry.valid(e) && g_registry.all_of<ecs::ItemIdentity>(e); }
const TItemTable* ItemSystem::GetItemProto(entt::entity e) {
    auto it = weaponProtos.find(e); return it == weaponProtos.end() ? nullptr : &it->second;
}
uint32_t PartySystem::GetLeaderPID(entt::entity) { Unexpected(); }
entt::entity PartySystem::GetLeader(entt::entity) { Unexpected(); }
int64_t ecs::PointSystem::Get(entt::entity e, uint8_t point) {
    if (point == POINT_MOV_SPEED) return motionSettings[e].movePoint;
    Recovery(e);
    if (point == POINT_HP) return g_registry.get<ecs::Health>(e).current;
    if (point == POINT_HP_REGEN) return Recovery(e).bonus;
    Unexpected();
}
uint64_t ecs::PointSystem::GetMaxHP(entt::entity e) { Recovery(e); return g_registry.get<ecs::Health>(e).max; }
void ecs::PointSystem::Change(entt::entity e, uint8_t point, int64_t amount, bool, bool, bool) {
    Recovery(e); Check(point == POINT_HP, "recovery changed unexpected point"); ++recoveryChanges;
    auto& health = g_registry.get<ecs::Health>(e);
    health.current = static_cast<int32_t>(std::clamp<int64_t>(int64_t(health.current) + amount, 0, health.max));
    RecoveryCallback(e, 4);
}
uint32_t g_start_position[4][2] {};
int passes_per_sec = 25;
int save_event_second_cycle = 60;
int test_server = 0;
CItemRegistry& CItemRegistry::Instance() { Unexpected(); }
entt::entity CItemRegistry::Find(uint32_t) const { Unexpected(); }
entt::entity CItemRegistry::FindByVID(uint32_t) const { Unexpected(); }
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
    const auto building = Entity(ecs::SpatialKind::Building), item = Entity(ecs::SpatialKind::Item);
    Check(Spawn(building, 1, 12000) && Spawn(item), "non-character spawn");
    Check(Visible(building, a) && Visible(item, a), "building range exception / item hidden");
    const auto beforeRename = packets.size();
    ecs::VisibilitySystem::Reencode(g_registry, item);
    Check(packets.size() == beforeRename + 4 && packets.back().source == item,
        "item reencode failed to update both viewers");
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
        Visible(item, replacement), "character reinsert did not restore visibility tags");
}

entt::entity BuildingIdentity(uint32_t id, uint32_t vid) {
    const auto e = Entity(ecs::SpatialKind::Building);
    g_registry.get<ecs::VIDComponent>(e).value = vid;
    auto& state = g_registry.emplace<ecs::BuildingState>(e);
    state.objectId = id; state.vnum = 14061; state.landId = 30;
    return e;
}
void NativeBuildingIdentityAndPlacement() {
    Reset(); MapFixture map;
    const auto viewer = Entity(ecs::SpatialKind::Character);
    Check(Spawn(viewer), "building viewer setup");
    const auto first = BuildingIdentity(100, 200);
    Check(!ecs::CBuildingRegistry::Register(0, 200, first) &&
        !ecs::CBuildingRegistry::Register(100, 0, first) &&
        !ecs::CBuildingRegistry::Register(101, 200, first) &&
        !ecs::CBuildingRegistry::Register(100, 201, first), "building accepted inconsistent identity");
    Check(ecs::CBuildingRegistry::Register(100, 200, first) &&
        ecs::CBuildingRegistry::FindByID(100) == first &&
        ecs::CBuildingRegistry::FindByVID(200) == first, "building identity not indexed");
    const auto sameID = BuildingIdentity(100, 201), sameVID = BuildingIdentity(101, 200);
    Check(!ecs::CBuildingRegistry::Register(100, 201, sameID) &&
        !ecs::CBuildingRegistry::Register(101, 200, sameVID), "live building identity collision replaced owner");
    ecs::CBuildingRegistry::Unregister(100, sameID);
    Check(ecs::CBuildingRegistry::FindByID(100) == first, "unrelated building unregistered live owner");
    Check(Spawn(first, 1, 12000) && Visible(first, viewer), "native building range/placement failed");
    g_registry.get<ecs::BuildingState>(first).destroying = true;
    Check(ecs::CBuildingRegistry::FindByID(100) == entt::null &&
        ecs::CBuildingRegistry::FindByVID(200) == entt::null, "retiring building still resolves");
    ecs::SpatialService::RemoveEntity(g_registry, first);
    Check(!Visible(first, viewer) && !map.At(12000)->Contains(first), "building removal left view or sector entry");
    g_registry.destroy(first);
    const auto replacement = BuildingIdentity(100, 200);
    Check(replacement != first && ecs::CBuildingRegistry::Register(100, 200, replacement),
        "replacement building registration rejected retired generation");
    ecs::CBuildingRegistry::Unregister(100, first);
    Check(ecs::CBuildingRegistry::FindByID(100) == replacement &&
        ecs::CBuildingRegistry::FindByVID(200) == replacement, "old building teardown erased replacement indices");
    Check(Spawn(replacement) && Visible(replacement, viewer) && !Visible(first, viewer),
        "replacement building inherited stale visibility identity");
    ecs::SpatialService::RemoveEntity(g_registry, first);
    Check(Visible(replacement, viewer), "old building removal affected replacement visibility");
    g_registry.get<ecs::VIDComponent>(replacement).value = 202;
    Check(ecs::CBuildingRegistry::FindByVID(200) == entt::null &&
        ecs::CBuildingRegistry::Register(100, 202, replacement) &&
        ecs::CBuildingRegistry::FindByVID(202) == replacement, "building VID rebinding retained stale index");
    ecs::CBuildingRegistry::Unregister(100, replacement);
    Check(ecs::CBuildingRegistry::FindByID(100) == entt::null &&
        ecs::CBuildingRegistry::FindByVID(202) == entt::null, "building indices not cleared together");
    g_registry.destroy(sameID); g_registry.destroy(sameVID);
}

void ItemCompanionInsertion() {
    for (int outcome = 0; outcome < 3; ++outcome) {
        Reset(); MapFixture map;
        const auto owner = Entity(ecs::SpatialKind::Character), item = Entity();
        g_registry.emplace<ecs::ItemOwner>(item, ecs::ItemOwner{owner, 123, 123, 0});
        g_registry.get<ecs::ItemLocation>(item) = {INVENTORY, 7};
        int quantity = 10, calls = 0;
        bool threw = false, result = false;
        try {
            result = ecs::SpatialService::InsertItemWithCommit(g_registry, item, 1, 100, 100, 0, [&] {
                ++calls;
                Check(quantity == 10 && g_registry.get<ecs::ItemOwner>(item).owner == owner &&
                    g_registry.get<ecs::ItemLocation>(item).window == INVENTORY &&
                    g_registry.all_of<ecs::SectorPlacement>(item) && !ecs::SectorOf(g_registry, item) &&
                    map.At()->Contains(item) && packets.empty(), "companion ran before preparation/reservation or after publication");
                if (outcome == 1) return false;
                if (outcome == 2) throw std::runtime_error("companion pre-commit failure");
                quantity -= 3;
                g_registry.get<ecs::ItemOwner>(item) = {};
                g_registry.get<ecs::ItemLocation>(item) = {GROUND, 0};
                return true;
            });
        } catch (const std::runtime_error& error) {
            if (std::string_view(error.what()) != "companion pre-commit failure") throw;
            threw = true;
        }
        Check(calls == 1 && result == (outcome == 0) && threw == (outcome == 2) &&
            quantity == (outcome == 0 ? 7 : 10) && packets.empty(), "companion insertion lost its commit result");
        if (outcome == 0) {
            Check(ecs::SectorOf(g_registry, item) == map.At() && map.At()->Contains(item) &&
                g_registry.get<ecs::SpatialRevision>(item).value == 1, "companion failed to install native membership");
        } else {
            Check(!map.At()->Contains(item) && !g_registry.any_of<ecs::SectorPlacement, ecs::SpatialEntity>(item) &&
                g_registry.get<ecs::ItemOwner>(item).owner == owner &&
                g_registry.get<ecs::ItemLocation>(item).cell == 7, "failed companion left membership or lost stored ownership");
            Check(ecs::SpatialService::InsertItemWithCommit(g_registry, item, 1, 100, 100, 0, [&] {
                g_registry.get<ecs::ItemOwner>(item) = {};
                g_registry.get<ecs::ItemLocation>(item) = {GROUND, 0}; return true;
            }), "failed companion could not be retried");
        }
    }
    for (bool retire : {false, true}) {
        Reset(); MapFixture map;
        const auto item = Entity(); int calls = 0;
        Callback callback {[&](entt::registry& registry, entt::entity current) {
            if (current != item) return;
            if (retire) registry.destroy(item); else map.At()->Destroy();
        }};
        entt::scoped_connection connection = g_registry.on_construct<ecs::SectorPlacement>().connect<&Callback::Run>(callback);
        Check(!ecs::SpatialService::InsertItemWithCommit(g_registry, item, 1, 100, 100, 0, [&] {
            ++calls; return true;
        }) && calls == 0 && !map.At()->Contains(item), "failed sector preparation reached resource companion");
    }
    Reset(); MapFixture map;
    const auto pc = Entity(ecs::SpatialKind::Character), item = Entity(); int calls = 0;
    Check(!ecs::SpatialService::InsertItemWithCommit(g_registry, pc, 1, 100, 100, 0, [&] { ++calls; return true; }) &&
        !ecs::SpatialService::InsertItemWithCommit(g_registry, item, 1, 100, 100, 0, {}) && calls == 0,
        "non-item/empty companion accepted");
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
    // Explicit time contract: speed is world units per 10 ms in these
    // spatial fixtures. Production commands calculate their own duration.
    auto& state = g_registry.get<ecs::MovementState>(e);
    state.moveStartX = x; state.moveStartY = y;
    state.moveDuration = uint32_t(std::min<double>(INT_MAX,
        std::ceil(std::hypot(double(targetX) - x, double(targetY) - y) * 10 / std::max(1, speed))));
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
void MovementPacketInitializesTheCommand() {
    Reset(); MapFixture map;
    const auto e = Moving(100, 100, 101, 100);
    g_registry.emplace<ecs::TagCharacter>(e);
    DESC descriptor;
    descriptor.SetEntity(e);
    inputDescriptors[e] = &descriptor;
    ecs::MovementSystem::Stop(e);
    struct Input : CInputMain { using CInputMain::Move; } input;
    TPacketCGMove packet {};
    packet.bHeader = HEADER_CG_MOVE; packet.bFunc = FUNC_MOVE;
    packet.lX = 700; packet.lY = 100; packet.dwDuration = 1200; packet.dwTime = get_dword_time();
    input.Move(e, reinterpret_cast<const char*>(&packet));
    const auto& state = g_registry.get<ecs::MovementState>(e);
    Check(state.moveDuration == 1200 && state.moveStartTime == get_dword_time() &&
        state.moveStartX == 100 && g_registry.get<ecs::MovementDestination>(e).x == 700,
        "input handler prepublished the target and suppressed command initialization");
    Check(movementPackets.size() == 1 && movementPackets[0].dwDuration == 1200 && movementPackets[0].lX == 700,
        "input published a zero or stale movement duration");
    input.Move(e, reinterpret_cast<const char*>(&packet));
    Check(movementPackets.size() == 1, "refused repeated command was published again");
    MovementSystem_Update(g_registry, get_dword_time() + 600);
    Check(g_registry.get<ecs::Position>(e).x == 400, "packet timing did not drive movement");
    // A zero movement budget must leave destination and packets alone.
    packet.lX = 900; motionSettings[e].movePoint = 0;
    input.Move(e, reinterpret_cast<const char*>(&packet));
    Check(g_registry.get<ecs::MovementDestination>(e).x == 700 && movementPackets.size() == 1,
        "rejected input changed the destination or published a packet");
    inputDescriptors.erase(e);
}

void ElapsedMovementTiming() {
    {
        Reset(); MapFixture map; MovementProbe probe;
        const auto e = Moving(100, 100, 101, 100);
        motionSettings[e].attached = true;
        ecs::MovementSystem::SyncDestinationClear(e);
        Check(ecs::MovementSystem::GotoWithDuration(e, 1100, 100, 10000), "client-timed move did not start");
        const auto start = get_dword_time();
        MovementSystem_Update(g_registry, start);
        MovementSystem_Update(g_registry, start);
        Check(g_registry.get<ecs::Position>(e).x == 100 && probe.moved.empty(), "move advanced without elapsed time");
        MovementSystem_Update(g_registry, start + 5000);
        const auto& state = g_registry.get<ecs::MovementState>(e);
        Check(g_registry.get<ecs::Position>(e).x == 600 && state.moveDuration == 10000 &&
            state.moveStartTime == start, "tick changed command timing or missed halfway position");
        const auto notices = probe.moved.size();
        MovementSystem_Update(g_registry, start + 5000);
        Check(g_registry.get<ecs::Position>(e).x == 600 && probe.moved.size() == notices,
            "same timestamp moved or published twice");
        MovementSystem_Update(g_registry, start + 15000);
        Check(g_registry.get<ecs::Position>(e).x == 1100 && !g_registry.all_of<ecs::MovementDestination>(e) &&
            g_registry.get<ecs::MovementState>(e).moveDuration == 0, "lagged tick did not settle at destination");
    }
    auto atOneSecond = [](bool fine) {
        Reset(); MapFixture map;
        const auto e = Moving(100, 100, 101, 100);
        motionSettings[e].attached = true;
        ecs::MovementSystem::SyncDestinationClear(e);
        Check(ecs::MovementSystem::GotoWithDuration(e, 1100, 100, 10000), "cadence fixture did not start");
        const auto start = get_dword_time();
        if (fine) for (uint32_t ms = 1; ms < 1000; ++ms) MovementSystem_Update(g_registry, start + ms);
        MovementSystem_Update(g_registry, start + 1000);
        return g_registry.get<ecs::Position>(e).x;
    };
    const auto coarse = atOneSecond(false), fine = atOneSecond(true);
    Check(coarse == 200 && fine == coarse, "rounding made movement depend on update frequency");
    {
        Reset(); MapFixture map;
        const auto e = Moving(100, 100, 101, 100);
        motionSettings[e].attached = true;
        ecs::MovementSystem::SyncDestinationClear(e);
        clockNow = UINT32_MAX - 50;
        Check(ecs::MovementSystem::GotoWithDuration(e, 500, 100, 1000), "wrap fixture did not start");
        MovementSystem_Update(g_registry, uint32_t(clockNow + 250));
        Check(g_registry.get<ecs::Position>(e).x == 200, "millisecond rollover broke elapsed movement");
        MovementSystem_Update(g_registry, uint32_t(clockNow + 1000));
        Check(g_registry.get<ecs::Position>(e).x == 500 && !g_registry.all_of<ecs::MovementDestination>(e),
            "rollover move did not arrive");
    }
    for (bool riding : {false, true}) {
        Reset(); MapFixture map;
        const auto e = Moving(100, 100, 101, 100);
        motionSettings[e].attached = true;
        motionSettings[e].movePoint = 200;
        motionSettings[e].mount = riding ? 20110 : 0;
        ecs::MovementSystem::SyncDestinationClear(e);
        Check(ecs::MovementSystem::GotoWithDuration(e, 700, 100, 1), "speed-budget move did not start");
        const auto duration = g_registry.get<ecs::MovementState>(e).moveDuration;
        Check(duration == (riding ? 250 : 500), "speed point or mount budget ignored");
        MovementSystem_Update(g_registry, get_dword_time() + duration / 2);
        Check(g_registry.get<ecs::Position>(e).x == 400, "speed budget was not used by the tick");
    }
}

void WalkModeOwnership() {
    Reset(); MapFixture map; MovementProbe probe;
    // A runner mid-move keeps its run mode: the tick owns the interpolation
    // flag, not the walk-mode flag.
    const auto runner = Moving(100, 100, 1000, 100, 10);
    MovementSystem_Update(g_registry, 10);
    const auto& running = g_registry.get<ecs::MovementState>(runner);
    Check(running.isWalking && !running.isNowWalking, "movement tick changed the walk mode");

    // An explicit walk mode survives both a mid-move tick and an interrupted
    // move (mount change, warp, sync); the tick used to clear it on arrival
    // and force it true while moving.
    const auto walker = Moving(500, 500, 700, 500, 10);
    g_registry.get<ecs::MovementState>(walker).isNowWalking = true;
    MovementSystem_Update(g_registry, 20);
    ecs::MovementSystem::SyncDestinationClear(walker);
    Check(g_registry.get<ecs::MovementState>(walker).isNowWalking,
        "interrupted move changed the walk mode");

    const auto arriving = Moving(900, 900, 950, 900, 1000);
    g_registry.get<ecs::MovementState>(arriving).isNowWalking = true;
    MovementSystem_Update(g_registry, 30);
    Check(g_registry.get<ecs::MovementState>(arriving).isNowWalking,
        "arrival overwrote an explicit walk mode");
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
    ecs::MovementSystem::SyncTimingWrite(overflow, 59, INT_MAX);
    ecs::MovementSystem::SyncTimingWrite(diagonal, 50, 50);
    ecs::MovementSystem::SyncTimingWrite(zeroSpeed, 50, 100);
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
        ecs::MovementSystem::SyncTimingWrite(e, 100, 777);
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
    ecs::MovementSystem::SyncTimingWrite(e, 100, 10);
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
    // A server-forced player move uses the fixed 450 base, not motion data.
    Check(initial.moveStartTime == 123456 && initial.moveDuration == 1111 &&
        g_registry.get<ecs::MovementDestination>(e).x == 400 &&
        AIHelpers::GetStateDuration(e) == 4, "Goto target/timing/AI commit missing");
    Check(!ecs::MovementSystem::Goto(e, 400, 500) &&
        g_registry.get<ecs::MovementState>(e).commandRevision == initial.commandRevision,
        "repeated target restarted movement");
    Check(!ecs::MovementSystem::Goto(e, 100, 100) &&
        g_registry.get<ecs::MovementDestination>(e).x == 400, "current-position no-op changed target");

    motionSettings[e].movePoint = 200;
    ecs::MovementSystem::CalculateMoveDuration(e);
    Check(g_registry.get<ecs::MovementState>(e).moveDuration == 1111, "server recomputed client-timed PC move");
    Check(std::abs(ecs::MovementSystem::GetMoveSpeed(e) - 500 / 1.111f) < 0.01f, "committed movement speed");
    const auto mob = Moving(100, 100, 400, 500, 200, true);
    motionSettings[mob].movePoint = 200;
    ecs::MovementSystem::CalculateMoveDuration(mob);
    Check(g_registry.get<ecs::MovementState>(mob).moveDuration == 833, "speed-change duration calculation");
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
void RetargetingCannotStarveTheCharacterFrame() {
    // The live ranged-monkey trace alternated its 80%/90% stand-off points
    // every pulse. AI also retargets every pulse when a player keeps moving.
    // Both must advance the OLD segment before replacing its start time.
    for (int scenario = 0; scenario < 3; ++scenario) {
        Reset(); MapFixture map;
        if (scenario == 2) clockNow = UINT32_MAX - 200;
        const auto mob = Moving(1000, 1000, 1001, 1000, 200, true);
        ecs::MovementSystem::SyncDestinationClear(mob);
        Check(ecs::MovementSystem::Goto(mob, 4000, 1000), "initial chase did not start");
        const auto first = g_registry.get<ecs::Position>(mob);
        int lastSeen = first.x;
        for (int frame = 0; frame < 50; ++frame) {
            clockNow += 40;
            ecs::UpdateCharacterFrame(g_registry, clockNow, [&] {
                const auto position = g_registry.get<ecs::Position>(mob);
                Check(position.x > lastSeen,
                    "AI retargeting starved movement: character still at previous frame's position");
                lastSeen = position.x;
                const int goal = scenario == 1 ? 4000 + frame * 5 : 4000 + (frame % 2 ? 80 : 0);
                ecs::MovementSystem::Goto(mob, goal, 1000);
            });
        }
        Check(lastSeen > first.x + 400 && lastSeen < first.x + 800,
            "retargeted mob did not make speed-bounded progress over two seconds");
    }
}

void NativeAttackMovementStop() {
    Reset(); MapFixture map;
    const auto mob = Moving(1000, 1000, 200, 1000, 150, true);
    const auto victim = Entity(ecs::SpatialKind::Character);
    g_registry.emplace<ecs::CombatTarget>(mob, victim, 0u);
    g_registry.emplace<ecs::CombatActiveTag>(mob);
    g_registry.emplace<ecs::AIStateMachine>(mob).current = ecs::AIFSMState::Battle;
    auto& movement = g_registry.get<ecs::MovementState>(mob);
    movement.isWalking = true;
    movement.moveSpeed = 360;
    movement.walkPreference = true;
    const auto position = g_registry.get<ecs::Position>(mob);

    Check(ecs::MovementSystem::StopForAttack(mob), "attack stop failed");
    const auto stopped = g_registry.get<ecs::MovementState>(mob);
    Check(!g_registry.all_of<ecs::MovementDestination>(mob) &&
        stopped.moveStartTime == 0 && stopped.moveDuration == 0 &&
        stopped.moveSpeed == 0 && !stopped.isWalking && stopped.stopTime == clockNow,
        "attack stop left an active interpolation");
    Check(CombatSystem::GetVictim(mob) == victim &&
        g_registry.all_of<ecs::CombatActiveTag>(mob) &&
        g_registry.get<ecs::AIStateMachine>(mob).current == ecs::AIFSMState::Battle &&
        stopped.walkPreference && transitions.empty(), "attack stop discarded combat/AI/preferences");
    MovementSystem_Update(g_registry, clockNow + 10000);
    const auto after = g_registry.get<ecs::Position>(mob);
    Check(after.x == position.x && after.y == position.y,
        "movement pump carried a stopped attacker toward its old stand-off point");

    Check(ecs::MovementSystem::Goto(mob, 1800, 1000), "next chase could not start");
    g_registry.remove<ecs::DirtyTag>(mob);
    bool nested = false;
    Callback retarget {[&](entt::registry&, entt::entity changed) {
        if (changed == mob && !nested) {
            nested = true;
            Check(ecs::MovementSystem::Goto(mob, 2500, 1000), "nested chase could not start");
        }
    }};
    entt::scoped_connection connection =
        g_registry.on_construct<ecs::DirtyTag>().connect<&Callback::Run>(retarget);
    Check(!ecs::MovementSystem::StopForAttack(mob) && nested &&
        g_registry.get<ecs::MovementDestination>(mob).x == 2500 &&
        g_registry.get<ecs::MovementState>(mob).moveDuration > 0,
        "attack stop overwrote a replacement chase from its preparation callback");
    connection.release();

    g_registry.remove<ecs::DirtyTag>(mob);
    Callback retire {[&](entt::registry& reg, entt::entity changed) {
        if (changed == mob) reg.destroy(mob);
    }};
    connection = g_registry.on_construct<ecs::DirtyTag>().connect<&Callback::Run>(retire);
    Check(!ecs::MovementSystem::StopForAttack(mob) && !g_registry.valid(mob),
        "attack stop accessed an attacker retired during preparation");
    connection.release();
    Check(!ecs::MovementSystem::StopForAttack(entt::null) &&
        !ecs::MovementSystem::StopForAttack(Entity()), "attack stop accepted a non-character");
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
    // The baked speeds come by vnum and walking; the motion bake dropped the
    // mount/horse-mode motion lookups along with the .msa files.
    moveSpeeds[{1, false}] = 400;
    moveSpeeds[{1, true}] = 150;
    Check(ecs::MovementSystem::GetMoveMotionSpeed(e) == 400, "run motion speed");
    g_registry.get<ecs::MovementState>(e).isNowWalking = true;
    Check(ecs::MovementSystem::GetMoveMotionSpeed(e) == 150, "walk motion speed");
    g_registry.get<ecs::MovementState>(e).isNowWalking = false;
    motionSettings[e].stamina = 0;
    Check(ecs::MovementSystem::GetMoveMotionSpeed(e) == 150, "exhausted PC did not walk");
    motionSettings[e].attached = false;
    Check(ecs::MovementSystem::GetMoveMotionSpeed(e) == 400, "descriptor-free character used PC walk motion");
    motionSettings[e].attached = true; motionSettings[e].stamina = 100;
    moveSpeeds.clear();
    Check(ecs::MovementSystem::GetMoveMotionSpeed(e) == 300, "missing motion fallback");
    for (const float bad : {0.0f, -100.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        moveSpeeds[{1, false}] = bad;
        Check(ecs::MovementSystem::GetMoveMotionSpeed(e) == 300, "malformed motion introduced invalid speed");
    }
    moveSpeeds.clear();
    // Only monsters recompute their duration on the server.
    const auto mob = Moving(100, 100, 101, 100, 200, true);
    ecs::MovementSystem::SyncDestinationWrite(mob, INT32_MIN, INT32_MAX);
    motionSettings[mob].movePoint = INT64_MIN;
    ecs::MovementSystem::CalculateMoveDuration(mob);
    Check(g_registry.get<ecs::MovementState>(mob).moveDuration == INT_MAX,
        "extreme duration overflowed rather than saturating");
    ecs::MovementSystem::SyncDestinationWrite(mob, 400, 100);
    motionSettings[mob].movePoint = INT64_MAX;
    ecs::MovementSystem::CalculateMoveDuration(mob);
    Check(g_registry.get<ecs::MovementState>(mob).moveDuration == 1000 * 28 / 100,
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
    // A descriptor-free PC (clone) moves at the fixed 450 forced-move base.
    ecs::MovementSystem::SyncDestinationClear(e);
    Callback arrival {[&](entt::registry& reg, entt::entity changed) {
        if (changed != e) return;
        Check(reg.get<ecs::MovementState>(e).moveDuration == 666 &&
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
        g_registry.get<ecs::MovementState>(e).moveDuration == 666,
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
        g_registry.get<ecs::MovementState>(e).moveDuration == 1333, "preparation replaced nested command");
    connection.release();

    g_registry.emplace<ecs::CombatActiveTag>(e);
    Callback stop {[&](entt::registry&, entt::entity changed) {
        if (changed == e) Check(ecs::MovementSystem::Goto(e, 1000, 100), "retarget during stop");
    }};
    connection = g_registry.on_destroy<ecs::CombatActiveTag>().connect<&Callback::Run>(stop);
    ecs::MovementSystem::Stop(e);
    Check(g_registry.get<ecs::MovementDestination>(e).x == 1000 &&
        g_registry.get<ecs::MovementState>(e).moveDuration == 2000, "old Stop erased new Goto");
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

void DungeonReturnLocationKeepsMembership() {
    Reset();
    const auto player = Entity(ecs::SpatialKind::Character), dungeon = g_registry.create();
    g_registry.emplace_or_replace<ecs::TagPC>(player);
    g_registry.emplace<ecs::DungeonState>(dungeon).mapIndex = 3520000;
    g_registry.emplace<ecs::DungeonMembership>(player).dungeon = dungeon;
    // nemere_zone.login calls pc.set_warp_location(219,5354,14284).
    ecs::MovementSystem::SetWarpLocation(player, 219, 5354, 14284);
    Check(g_registry.get<ecs::DungeonMembership>(player).dungeon == dungeon,
        "Nemere return-point setup cleared pc.in_dungeon before the boss kill");
    Check(ecs::MovementSystem::GetWarpLocation(player).mapIndex == 219,
        "return-point setup lost the exit destination");
    // WarpEnd clears a consumed destination; the player has just arrived,
    // so this must not remove the new dungeon membership either.
    ecs::MovementSystem::SetWarpLocationRaw(player, 0, 0, 0);
    Check(g_registry.get<ecs::DungeonMembership>(player).dungeon == dungeon,
        "clearing the consumed warp removed dungeon membership");
}

// Show's height sentinel. The wrapper resolved LONG_MAX for its own
// SyncPositionComponents call and then handed the raw value to CHARACTER::Show,
// which synced a second time - so every caller that let z default wrote
// LONG_MAX into Position.z, and every read of the character's height after
// that returned it. There is one resolution now, before anything reads z.
void ShowHeightSentinel() {
    Check(ecs::MovementSystem::ResolveShowHeight(INT32_MAX, 250) == 250,
        "the sentinel was written through as a height");
    Check(ecs::MovementSystem::ResolveShowHeight(0, 250) == 0,
        "an explicit ground height was overridden");
    Check(ecs::MovementSystem::ResolveShowHeight(-120, 250) == -120,
        "an explicit height below zero was overridden");
    Check(ecs::MovementSystem::ResolveShowHeight(INT32_MAX, 0) == 0,
        "the sentinel did not resolve against a zero height");
    // The production API is int32_t on Windows and FreeBSD; long is not.
    static_assert(ecs::MovementSystem::ResolveShowHeight(INT32_MAX, 7) == 7);
}

void ShownMountFollowAndRemoval() {
    Reset(); MapFixture map(3); // Avoid the unrelated map-1 HWID admission rule.
    const auto character = [&](bool pc, int x) {
        const auto e = Entity(ecs::SpatialKind::Character);
        g_registry.emplace<ShowProbe>(e);
        g_registry.emplace<ecs::TagCharacter>(e);
        g_registry.emplace<ecs::PlayerName>(e, pc ? "viewer" : "owner's Mount");
        g_registry.emplace<ecs::RaceComponent>(e, uint16_t(pc ? 0 : 20110));
        g_registry.emplace<ecs::RaceState>(e, pc ? 0u : 20110u, 0u);
        g_registry.emplace<ecs::SpatialEntity>(e);
        // Match the factory: position exists before Show, but membership does not.
        g_registry.emplace<ecs::Position>(e, x, 100, 0);
        g_registry.emplace<ecs::PositionZ>(e, 0);
        g_registry.emplace<ecs::MapIndex>(e, 3);
        g_registry.emplace<ecs::ViewMap>(e);
        g_registry.emplace<ecs::ViewerMap>(e);
        g_registry.emplace<ecs::ViewAgeMap>(e);
        g_registry.emplace<ecs::MovementState>(e);
        g_registry.emplace<ecs::MovementSpeed>(e, 100, 200);
        g_registry.emplace<ecs::CharacterRuntimeFlagsComponent>(e);
        g_registry.emplace<ecs::LegacyCharEvents>(e);
        auto& status = g_registry.emplace<ecs::StatusFlags>(e);
        status.isMount = !pc;
        motionSettings[e].attached = pc;
        motionSettings[e].race = pc ? 0 : 20110;
        if (pc) {
            g_registry.emplace<ecs::PlayerID>(e, uint32_t(entt::to_integral(e) + 1));
            g_registry.emplace<ecs::AccountID>(e);
            g_registry.emplace<ecs::EmpireComponent>(e);
            g_registry.emplace<ecs::GMLevel>(e);
        } else {
            g_registry.remove<ecs::TagPC>(e);
            g_registry.emplace<ecs::TagNPC>(e);
            g_registry.get<ecs::CharacterType>(e).value = CHAR_TYPE_NPC;
            g_registry.emplace<RecoveryProbe>(e);
            g_registry.emplace<ecs::MobInstanceState>(e);
        }
        Check(!g_registry.all_of<ecs::SectorPlacement>(e), "factory fixture already placed");
        Check(ecs::MovementSystem::Show(e, 3, x, 100, 0), "real Show rejected factory-shaped actor");
        Check(SectreeMember(e, map.At(x, 100)), "Show did not commit native membership");
        return e;
    };
    const auto owner = character(true, 7000), observer = character(true, 6800);
    const auto dungeon = g_registry.create();
    g_registry.emplace<ecs::DungeonState>(dungeon).mapIndex = 3;
    g_registry.emplace<ecs::DungeonMembership>(owner).dungeon = dungeon;
    Check(ecs::MovementSystem::Show(owner, 3, 7000, 100, 0) &&
        g_registry.get<ecs::DungeonMembership>(owner).dungeon == dungeon,
        "same-map Show detached a dungeon member");
    Check(!ecs::MovementSystem::Show(owner, 4, 7000, 100, 0) &&
        g_registry.get<ecs::DungeonMembership>(owner).dungeon == dungeon,
        "rejected Show detached a dungeon member");
    entt::entity previous = entt::null;
    for (int cycle = 0; cycle < 2; ++cycle) {
        const auto follower = character(false, 6200);
        Check(previous == entt::null || follower != previous, "follow cycle reused entity generation");
        Check(awake.contains(follower), "shown follower did not join native state list");
        const auto viewers = ecs::VisibilityService::GetViewersOf(g_registry, follower);
        Check(std::find(viewers.begin(), viewers.end(), owner) != viewers.end() &&
            std::find(viewers.begin(), viewers.end(), observer) != viewers.end(),
            "shown follower omitted owner or observer from live recipients");
        Check(Visible(follower, owner) && Visible(follower, observer), "Show did not publish follower");
        Check(ecs::MovementSystem::Goto(follower, 6800, 100), "shown follower could not follow owner");
        movementRecipients.clear();
        ecs::MovementSystem::SendMovePacket(follower, FUNC_WAIT, 0, 0, 0, 0);
        Check(std::count(movementRecipients.begin(), movementRecipients.end(), std::pair{follower, owner}) == 1 &&
            std::count(movementRecipients.begin(), movementRecipients.end(), std::pair{follower, observer}) == 1,
            "follow move omitted owner or observer");
        Check(movementPackets.back().lX == 6800 && movementPackets.back().dwDuration > 0,
            "follow move lost destination or duration");
        MovementSystem_Update(g_registry, 124123);
        Check(g_registry.get<ecs::Position>(follower).x == 6400 &&
            SectreeMember(follower, map.At(6400, 100)), "shown follower did not advance across sector");
        MovementSystem_Update(g_registry, 124790);
        MovementSystem_Update(g_registry, 125456);
        Check(g_registry.get<ecs::Position>(follower).x == 6800 &&
            !g_registry.all_of<ecs::MovementDestination>(follower), "shown follower did not arrive");
        packets.clear();
        ecs::ViewSystem::ViewCleanup(follower);
        for (const auto viewer : {owner, observer}) {
            Check(std::count_if(packets.begin(), packets.end(), [&](const Packet& packet) {
                return !packet.add && packet.source == follower && packet.viewer == viewer;
            }) == 1, "mount teardown omitted or duplicated follower remove");
            Check(!Visible(follower, viewer), "mount teardown left ghost visibility edge");
        }
        g_registry.destroy(follower);
        Check(!map.At(6800, 100)->Contains(follower), "retired mount retained sector membership");
        previous = follower;
    }
    MapFixture lobby(4);
    Check(ecs::MovementSystem::Show(owner, 4, 100, 100, 0) &&
        g_registry.get<ecs::DungeonMembership>(owner).dungeon == entt::null,
        "actual map departure kept dungeon membership");
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

entt::entity RecoveryActor(bool player = false, uint8_t type = CHAR_TYPE_MONSTER) {
    const auto e = g_registry.create();
    g_registry.emplace<ecs::CharacterType>(e, player ? uint8_t(CHAR_TYPE_PC) : type);
    g_registry.emplace<ecs::Health>(e, uint64_t { 500 }, uint64_t { 1000 });
    auto& probe = g_registry.emplace<RecoveryProbe>(e);
    probe.mob.bRegenCycle = 2; probe.mob.bRegenPercent = 10;
    motionSettings[e].attached = player;
    if (player) g_registry.emplace<ecs::TagPC>(e);
    g_registry.emplace<ecs::MovementState>(e).lastMoveTime = get_dword_time();
    return e;
}
LPEVENT RecoveryTimer(entt::entity e) {
    const auto* events = g_registry.try_get<ecs::LegacyCharEvents>(e);
    return events ? events->recovery : nullptr;
}
int RecoveryRun(int pulse) { recoveryHeart.pulse = pulse; return event_process(pulse); }

void NativeRecoveryTimers() {
    Reset(); auto e = RecoveryActor();
    ecs::PlayerRuntime::StartRecoveryEvent(e); auto timer = RecoveryTimer(e);
    Check(timer && event_time(timer) == 50, "native NPC recovery was not scheduled");
    ecs::PlayerRuntime::StartRecoveryEvent(e);
    Check(RecoveryTimer(e) == timer && event_count() == 1, "recovery start duplicated a timer");
    Check(RecoveryRun(50) == 1 && g_registry.get<ecs::Health>(e).current == 600 &&
        recoveryChanges == 1 && recoveryNotices == 1 && event_time(timer) == 50, "NPC recovery tick failed");
    g_registry.get<ecs::Health>(e).current = 999;
    RecoveryRun(100);
    Check(g_registry.get<ecs::Health>(e).current == 1000 && !RecoveryTimer(e) && !timer->q_el,
        "full NPC did not release recovery slot");

    Reset(); e = RecoveryActor(true); ecs::PlayerRuntime::StartRecoveryEvent(e); timer = RecoveryTimer(e);
    Check(event_time(timer) == 75, "player initial cadence changed");
    RecoveryRun(75);
    Check(g_registry.get<ecs::Health>(e).current == 525 && RecoveryTimer(e), "moving player recovery changed");
    g_registry.get<ecs::MovementState>(e).lastMoveTime = 0; Recovery(e).bonus = 50;
    RecoveryRun(150);
    Check(g_registry.get<ecs::Health>(e).current == 622, "idle regen bonus truncation changed");
    Recovery(e).poison = true; RecoveryRun(225);
    Check(event_time(timer) == 3 && recoveryChanges == 2, "player poison cadence/heal changed");
    Recovery(e).poison = false; g_registry.get<ecs::Health>(e).current = 1000; RecoveryRun(228);
    Check(RecoveryTimer(e) && event_time(timer) == 75, "full player lost periodic maintenance");

    Reset(); e = RecoveryActor(); Recovery(e).poison = true;
    ecs::PlayerRuntime::StartRecoveryEvent(e); timer = RecoveryTimer(e); RecoveryRun(50);
    Check(recoveryChanges == 0 && event_time(timer) == 50, "poisoned NPC healed/changed cadence");
    Recovery(e).missingMob = true; RecoveryRun(100);
    Check(!RecoveryTimer(e) && !timer->q_el, "missing prototype stranded timer");
    ecs::PlayerRuntime::StartRecoveryEvent(e); timer = RecoveryTimer(e);
    Check(event_time(timer) == 25, "missing prototype initial fallback changed");
    RecoveryRun(125); Check(!RecoveryTimer(e), "missing prototype fallback did not stop");

    Reset(); e = RecoveryActor(false, CHAR_TYPE_DOOR);
    ecs::PlayerRuntime::StartRecoveryEvent(e); timer = RecoveryTimer(e); RecoveryRun(50);
    Check(recoveryChanges == 0 && RecoveryTimer(e), "door recovered HP");
    Reset(); e = RecoveryActor(); motionSettings[e].race = 6193;
    ecs::PlayerRuntime::StartRecoveryEvent(e); Check(!RecoveryTimer(e), "Meley exception lost");
    for (bool stun : {false, true}) {
        Reset(); e = RecoveryActor(); (stun ? Recovery(e).stun : Recovery(e).dead) = true;
        ecs::PlayerRuntime::StartRecoveryEvent(e); Check(!RecoveryTimer(e), "dead/stunned actor started recovery");
        Recovery(e).stun = Recovery(e).dead = false; ecs::PlayerRuntime::StartRecoveryEvent(e);
        (stun ? Recovery(e).stun : Recovery(e).dead) = true; timer = RecoveryTimer(e); RecoveryRun(50);
        Check(!RecoveryTimer(e) && !timer->q_el && recoveryChanges == 0, "dead/stunned timer did not stop");
    }
    for (int action = 0; action < 3; ++action) {
        Reset(); e = RecoveryActor(); ecs::PlayerRuntime::StartRecoveryEvent(e); timer = RecoveryTimer(e);
        if (action == 0) g_registry.remove<ecs::Health>(e);
        if (action == 1) g_registry.remove<ecs::CharacterType>(e);
        if (action == 2) g_registry.remove<ecs::LegacyCharEvents>(e);
        RecoveryRun(50);
        Check(!RecoveryTimer(e) && !timer->q_el && recoveryChanges == 0, "removed recovery component was recreated/used");
    }
    Reset(); e = RecoveryActor(); ecs::PlayerRuntime::StartRecoveryEvent(e); timer = RecoveryTimer(e);
    g_registry.destroy(e); const auto recycled = RecoveryActor();
    Check(recycled != e && entt::to_entity(recycled) == entt::to_entity(e), "fixture did not recycle generation");
    RecoveryRun(50);
    Check(!timer->q_el && !RecoveryTimer(recycled) && recoveryChanges == 0, "retired timer reached recycled entity");

    // Real event callbacks may retire the owner at any outward call boundary.
    for (int stage = 0; stage <= 5; ++stage) {
        Reset(); e = RecoveryActor(stage != 5); ecs::PlayerRuntime::StartRecoveryEvent(e); timer = RecoveryTimer(e);
        onRecovery = [=](entt::entity current, int at) { if (at == stage) g_registry.destroy(current); };
        RecoveryRun(stage == 5 ? 50 : 75);
        Check(!g_registry.valid(e) && !timer->q_el && recoveryChanges == (stage == 4 ? 1 : 0),
            "callback retirement left a stale recovery operation");
    }
    for (int stage : {0, 1, 2, 3, 4}) {
        Reset(); e = RecoveryActor(true); ecs::PlayerRuntime::StartRecoveryEvent(e); timer = RecoveryTimer(e);
        LPEVENT replacement;
        onRecovery = [&](entt::entity current, int at) {
            if (at != stage) return;
            onRecovery = {};
            event_cancel(&g_registry.get<ecs::LegacyCharEvents>(current).recovery);
            ecs::PlayerRuntime::StartRecoveryEvent(current); replacement = RecoveryTimer(current);
        };
        RecoveryRun(75);
        Check(replacement && replacement != timer && RecoveryTimer(e) == replacement && !timer->q_el &&
            recoveryChanges == (stage == 4 ? 1 : 0), "old callback cleared/continued a replacement timer");
    }

    for (auto race : {3996u, 8202u}) {
        Reset();
        const entt::entity dungeon = g_registry.create();
        g_registry.emplace<ecs::DungeonState>(dungeon).flags["floor"] = (race == 3996 ? 5 : 1);
        e = RecoveryActor(); Recovery(e).mob.dwVnum = race; Recovery(e).dungeon = dungeon;
        ecs::PlayerRuntime::StartRecoveryEvent(e); timer = RecoveryTimer(e); RecoveryRun(50);
        Check(g_registry.get<ecs::Health>(e).current == 535 && event_time(timer) == 250,
            "special dungeon HP/cadence changed");
        g_registry.get<ecs::Health>(e).current = 1000; RecoveryRun(300);
        Check(RecoveryTimer(e) == timer && event_time(timer) == 75, "full special dungeon maintenance changed");
        g_registry.get<ecs::Health>(e).current = 500; g_registry.get<ecs::DungeonState>(dungeon).flags["floor"] = 99; RecoveryRun(375);
        Check(g_registry.get<ecs::Health>(e).current == 600 && event_time(timer) == 50,
            "wrong dungeon floor did not use normal NPC recovery");
        g_registry.get<ecs::DungeonState>(dungeon).flags["floor"] = (race == 3996 ? 5 : 1);
        onRecovery = [&](entt::entity current, int stage) { if (stage == 2) g_registry.destroy(current); };
        RecoveryRun(425); Check(!g_registry.valid(e) && !timer->q_el, "special dungeon used retired SP recipient");
    }
    // An unwinding callback must not leave a completed slot blocking restart.
    for (int stage : {0, 1, 2, 3, 4}) {
        Reset(); e = RecoveryActor(true); ecs::PlayerRuntime::StartRecoveryEvent(e); timer = RecoveryTimer(e);
        onRecovery = [=](entt::entity, int at) { if (stage == at) throw std::runtime_error("injected recovery failure"); };
        bool caught = false;
        try { RecoveryRun(75); } catch (const std::runtime_error&) { caught = true; }
        Check(caught && !RecoveryTimer(e) && !timer->q_el, "throwing callback stranded recovery slot");
        onRecovery = {}; ecs::PlayerRuntime::StartRecoveryEvent(e);
        Check(RecoveryTimer(e) && RecoveryTimer(e) != timer, "recovery could not restart after exception");
    }
    Reset(); e = RecoveryActor(true); ecs::PlayerRuntime::StartRecoveryEvent(e); timer = RecoveryTimer(e);
    LPEVENT afterException;
    onRecovery = [&](entt::entity current, int stage) {
        if (stage != 3) return;
        event_cancel(&g_registry.get<ecs::LegacyCharEvents>(current).recovery);
        ecs::PlayerRuntime::StartRecoveryEvent(current); afterException = RecoveryTimer(current);
        throw std::runtime_error("failure after timer replacement");
    };
    try { RecoveryRun(75); } catch (const std::runtime_error&) {}
    Check(afterException && RecoveryTimer(e) == afterException, "unwinding cleared a replacement timer");

    // Component construction is also an external callback boundary.
    for (int action = 0; action < 3; ++action) {
        Reset(); e = RecoveryActor();
        Callback observer {[&](entt::registry& reg, entt::entity current) {
            if (action == 0) reg.destroy(current);
            if (action == 1) reg.remove<ecs::LegacyCharEvents>(current);
            if (action == 2) ecs::PlayerRuntime::StartRecoveryEvent(current);
        }};
        auto connection = entt::scoped_connection(g_registry.on_construct<ecs::LegacyCharEvents>().connect<&Callback::Run>(observer));
        ecs::PlayerRuntime::StartRecoveryEvent(e);
        Check(event_count() == (action == 2 ? 1 : 0), "construction callback stranded/duplicated a timer");
    }

    Reset(); e = RecoveryActor(); g_registry.get<ecs::Health>(e).max = INT32_MAX;
    Recovery(e).mob.bRegenPercent = 255; ecs::PlayerRuntime::StartRecoveryEvent(e); RecoveryRun(50);
    Check(g_registry.get<ecs::Health>(e).current == INT32_MAX, "NPC regen gain overflowed payload");
    for (bool negative : {false, true}) {
        Reset(); e = RecoveryActor(true);
        Recovery(e).bonus = negative ? INT64_MIN : INT64_MAX;
        ecs::PlayerRuntime::StartRecoveryEvent(e); RecoveryRun(75);
        Check(g_registry.get<ecs::Health>(e).current == (negative ? 0 : 1000), "extreme regen bonus overflowed");
    }
    Reset(); e = g_registry.create(); ecs::PlayerRuntime::StartRecoveryEvent(e);
    ecs::PlayerRuntime::StartRecoveryEvent(entt::null);
    Check(event_count() == 0, "non-character/null entity started recovery");
}

}
int main() {
    try {
        SECTREE_MANAGER maps; CHARACTER_MANAGER characters; DESC_MANAGER descriptors; CMotionManager motionManager;
        ecs::VisibilitySystem::Init(g_registry);
        auto recoveryConnection = entt::scoped_connection(g_dispatcher.sink<ecs::EvRecovery>().connect<&Recovered>());
        AttributeSharing();
        ItemCompanionInsertion();
        MembershipAndSnapshots(); VisibilityRoundTrip(); ViewCallbacks(); PreparationAndPCs();
        LifetimeAndObservers(); RemovalCallbacksAndTeardown(); PreparationMutationAndIteration();
        NativeBuildingIdentityAndPlacement();
        NativeMovement(); MovementPacketInitializesTheCommand(); ElapsedMovementTiming(); WalkModeOwnership(); MovementVisibilityAndBounds(); MovementCallbackLifetime();
        MovementCallbackRetarget(); MovementArrivalAndPackets();
        NativeAnimationPackets(); NativeMovementDurationReads(); NativeMovementCommands();
        RetargetingCannotStarveTheCharacterFrame(); NativeAttackMovementStop();
        NativeMotionSelection(); MovementCommandReentry(); NativeAIScheduleStorage(); NativeWarpLocations();
        DungeonReturnLocationKeepsMembership(); ShowHeightSentinel();
        ShownMountFollowAndRemoval();
        NativeRecoveryTimers();
        Reset(); ecs::VisibilitySystem::Shutdown(g_registry);
        std::cout << "Spatial checks passed: " << checks << '\n'; return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}

LPEVENT ecs::PlayerRuntime::GetCharEvent(entt::entity, CharEvent) { Unexpected(); }
void ecs::PlayerRuntime::SetCharEvent(entt::entity, CharEvent, LPEVENT) { Unexpected(); }
