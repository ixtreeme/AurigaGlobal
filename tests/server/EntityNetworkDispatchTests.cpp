#include "../../SRC/Server/GameServer/stdafx.h"
#include "../../SRC/Server/GameServer/desc.h"
#include "../../SRC/Server/GameServer/packet.h"
#include "../../SRC/Server/GameServer/char_manager.h"
#include "../../SRC/Server/GameServer/sectree_manager.h"
#include "../../SRC/Server/GameServer/party.h"
#include "../../SRC/Server/GameServer/buffer_manager.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/NetworkService.hpp"
#include "../../SRC/Server/GameServer/ecs/services/EntityNetworkDispatch.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/NetworkSyncSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PointSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SocialSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/MountSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ItemSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/CombatSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/components/identity_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/movement_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/pet_mount_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/session_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/social_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/spatial_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/status_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/transform_components.hpp"
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>

// Real packet builders and NetworkSession transport selection, terminating at
// DESC::Packet. No character shell, actual socket, cipher or client is needed.
entt::registry g_registry;
namespace {
int checks = 0;
uint32_t now = 1000;
std::map<const DESC*, std::vector<std::vector<uint8_t>>> wire;
void Check(bool value, const char* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
[[noreturn]] void Unexpected() { throw std::runtime_error("unexpected packet-test service"); }
void Live(entt::entity e) { Check(g_registry.valid(e), "packet builder read retired entity"); }
template<class T> T Packet(const DESC& desc, size_t index, uint8_t header) {
    const auto& frames = wire.at(&desc);
    Check(index < frames.size(), "missing wire frame");
    const auto& bytes = frames[index];
    Check(bytes.size() == sizeof(T) && bytes.front() == header, "wrong wire header or byte length");
    T result {};
    std::memcpy(&result, bytes.data(), sizeof(result));
    return result;
}
entt::entity Character(uint32_t vid, bool pc, DESC* desc = nullptr) {
    const auto e = g_registry.create();
    g_registry.emplace<ecs::TagCharacter>(e);
    g_registry.emplace<ecs::SpatialEntity>(e);
    g_registry.emplace<ecs::SpatialKindTag>(e, ecs::SpatialKind::Character);
    g_registry.emplace<ecs::VIDComponent>(e, vid);
    g_registry.emplace<ecs::CharacterType>(e, uint8_t(pc ? CHAR_TYPE_PC : CHAR_TYPE_NPC));
    if (pc) g_registry.emplace<ecs::TagPC>(e); else g_registry.emplace<ecs::TagNPC>(e);
    g_registry.emplace<ecs::PlayerName>(e, pc ? "rider" : "rider's Mount");
    g_registry.emplace<ecs::RaceState>(e, pc ? 0u : 20110u, 0u);
    g_registry.emplace<ecs::Position>(e, 6200, 100, 0);
    g_registry.emplace<ecs::MapIndex>(e, 3);
    g_registry.emplace<ecs::RotationComponent>(e, 90.f);
    g_registry.emplace<ecs::MovementState>(e);
    g_registry.emplace<ecs::StatusFlags>(e).isMount = !pc;
    g_registry.emplace<ecs::MountState>(e);
    if (desc) {
        g_registry.emplace<ecs::NetworkSession>(e, desc);
        desc->SetEntity(e);
    }
    return e;
}
void MountWireRoundTrip() {
    DESC ownerDesc, observerDesc;
    const auto owner = Character(500, true, &ownerDesc);
    const auto observer = Character(501, true, &observerDesc);
    entt::entity oldFollower = entt::null;
    for (uint32_t cycle = 0; cycle != 2; ++cycle) {
        const auto follower = Character(600 + cycle, false);
        Check(oldFollower == entt::null || follower != oldFollower, "mount generation did not advance");
        auto& movement = g_registry.get<ecs::MovementState>(follower);
        movement.moveStartTime = now;
        movement.moveDuration = 900;
        g_registry.emplace<ecs::MovementDestination>(follower, 6800, 100);
        wire.clear();
        for (const auto viewer : {owner, observer}) {
            ecs::EntityNetworkDispatch::SendInsert(g_registry, follower, viewer);
            ecs::EntityNetworkDispatch::SendRemove(g_registry, follower, viewer);
        }
        for (const auto* desc : {&ownerDesc, &observerDesc}) {
            Check(wire.at(desc).size() == 5, "mount wire sequence has missing/extra frames");
            const auto add = Packet<TPacketGCCharacterAdd>(*desc, 0, HEADER_GC_CHARACTER_ADD);
            const auto info = Packet<TPacketGCCharacterAdditionalInfo>(*desc, 1, HEADER_GC_CHAR_ADDITIONAL_INFO);
            const auto move = Packet<TPacketGCMove>(*desc, 2, HEADER_GC_MOVE);
            const auto walk = Packet<TPacketGCWalkMode>(*desc, 3, HEADER_GC_WALK_MODE);
            const auto del = Packet<TPacketGCCharacterDelete>(*desc, 4, HEADER_GC_CHARACTER_DEL);
            Check(add.dwVID == 600 + cycle && info.dwVID == add.dwVID && move.dwVID == add.dwVID &&
                walk.vid == add.dwVID && del.id == add.dwVID, "mount ADD/info/MOVE/DEL VID mismatch");
            Check(add.bType == CHAR_TYPE_NPC && add.wRaceNum == 20110 && add.x == 6200 && add.y == 100,
                "mount insert lost type/race/current position");
            Check(std::string(info.name) == "rider's Mount" && info.dwMountVnum == 0,
                "follower additional info accidentally encoded rider state");
            Check(move.bFunc == FUNC_MOVE && move.lX == 6800 && move.lY == 100 && move.dwDuration == 900 &&
                move.dwTime == now && move.bRot == 18 && walk.mode == WALKMODE_RUN,
                "mount follow-up movement bytes differ from ECS state");
        }
        g_registry.destroy(follower);
        wire.clear();
        ecs::EntityNetworkDispatch::SendRemove(g_registry, follower, owner);
        Check(wire.empty(), "retired follower sent replacement-generation removal");
        oldFollower = follower;
    }
    // The ridden creature is a PC mount field, not the retired follower VID.
    g_registry.get<ecs::MountState>(owner).mountVnum = 20110;
    wire.clear();
    ecs::EntityNetworkDispatch::SendInsert(g_registry, owner, observer);
    const auto add = Packet<TPacketGCCharacterAdd>(observerDesc, 0, HEADER_GC_CHARACTER_ADD);
    const auto info = Packet<TPacketGCCharacterAdditionalInfo>(observerDesc, 1, HEADER_GC_CHAR_ADDITIONAL_INFO);
    Check(add.dwVID == 500 && info.dwVID == 500 && info.dwMountVnum == 20110 && add.bType == CHAR_TYPE_PC,
        "ridden PC reused follower VID or lost mount appearance");
    Check(wire.at(&observerDesc).size() == 2, "stationary rider emitted movement");
    for (const uint32_t mountVnum : {20110u, 0u, 20110u}) {
        g_registry.get<ecs::MountState>(owner).mountVnum = mountVnum;
        TPacketGCCharacterUpdate update {};
        Check(NetworkSyncSystem::BuildCharacterUpdatePacket(g_registry, owner, update), "rider update builder rejected PC");
        wire.clear();
        Check(ecs::NetworkService::Send(observer, &update, sizeof(update)), "rider update transport rejected session");
        const auto bytes = Packet<TPacketGCCharacterUpdate>(observerDesc, 0, HEADER_GC_CHARACTER_UPDATE);
        Check(bytes.dwVID == 500 && bytes.dwMountVnum == mountVnum,
            "mount/dismount update lost rider VID or mount state");
    }
    g_registry.clear(); wire.clear();
}
void ExpiredAndMissingSession() {
    DESC desc;
    const auto owner = Character(700, true, &desc), follower = Character(701, false);
    auto& movement = g_registry.get<ecs::MovementState>(follower);
    movement.moveStartTime = now - 100;
    movement.moveDuration = 50;
    g_registry.emplace<ecs::MovementDestination>(follower, 6500, 300);
    ecs::EntityNetworkDispatch::SendInsert(g_registry, follower, owner);
    Check(wire.at(&desc).size() == 2, "expired move emitted unsigned huge duration");
    const auto add = Packet<TPacketGCCharacterAdd>(desc, 0, HEADER_GC_CHARACTER_ADD);
    Check(add.x == 6500 && add.y == 300, "expired move did not snap insert to destination");
    wire.clear();
    g_registry.get<ecs::NetworkSession>(owner).desc = nullptr;
    const TPacketGCCharacterDelete probe {HEADER_GC_CHARACTER_DEL, 701};
    Check(!ecs::NetworkService::Send(owner, &probe, sizeof(probe)), "null session reported successful send");
    ecs::EntityNetworkDispatch::SendInsert(g_registry, follower, owner);
    ecs::EntityNetworkDispatch::SendRemove(g_registry, follower, owner);
    Check(wire.empty(), "null session wrote packets");
    g_registry.clear();
}
}

uint32_t get_dword_time() { return now; }
std::shared_ptr<spdlog::logger> logging::GetLogger() {
    static auto logger = std::make_shared<spdlog::logger>("entity-wire-test"); return logger;
}
std::shared_ptr<spdlog::logger> logging::GetErrorLogger() { return logging::GetLogger(); }
DESC::DESC() { m_sock = 1; m_accountTable = {}; }
DESC::~DESC() = default;
void DESC::Packet(const void* data, int size) {
    Check(data && size > 0, "invalid transport payload");
    const auto* bytes = static_cast<const uint8_t*>(data);
    wire[this].emplace_back(bytes, bytes + size);
}
void DESC::BufferedPacket(const void*, int) { Unexpected(); }
void DESC::Destroy() { Unexpected(); }
void DESC::SetPhase(int) { Unexpected(); }
CInputProcessor::CInputProcessor() = default;
bool CInputProcessor::Process(DESC*, const void*, int, int&) { Unexpected(); }
void CInputProcessor::Handshake(DESC*, const char*) { Unexpected(); }
CInputHandshake::CInputHandshake() = default;
CInputHandshake::~CInputHandshake() = default;
int CInputHandshake::Analyze(DESC*, uint8_t, const char*) { Unexpected(); }
int CInputLogin::Analyze(DESC*, uint8_t, const char*) { Unexpected(); }
int CInputMain::Analyze(DESC*, uint8_t, const char*) { Unexpected(); }
int CInputDead::Analyze(DESC*, uint8_t, const char*) { Unexpected(); }
int CInputDB::Analyze(DESC*, uint8_t, const char*) { Unexpected(); }
bool CInputDB::Process(DESC*, const void*, int, int&) { Unexpected(); }
CInputP2P::CInputP2P() = default;
CInputAuth::CInputAuth() = default;
int CInputP2P::Analyze(DESC*, uint8_t, const char*) { Unexpected(); }
int CInputAuth::Analyze(DESC*, uint8_t, const char*) { Unexpected(); }
CPacketInfo::CPacketInfo() : m_pCurrentPacket(nullptr), m_dwStartTime(0) {}
CPacketInfo::~CPacketInfo() = default;
CPacketInfoCG::CPacketInfoCG() = default;
CPacketInfoGG::CPacketInfoGG() = default;
CPacketInfoCG::~CPacketInfoCG() = default;
CPacketInfoGG::~CPacketInfoGG() = default;
Cipher::Cipher() : activated_(false), encoder_(nullptr), decoder_(nullptr), key_agreement_(nullptr) {}
Cipher::~Cipher() = default;
void intrusive_ptr_add_ref(EVENT* event) { ++event->ref_count; }
void intrusive_ptr_release(EVENT* event) { if (--event->ref_count == 0) delete event; }
namespace ecs::PlayerRuntime {
LPDESC GetDesc(entt::entity e) { Live(e); const auto* s = g_registry.try_get<ecs::NetworkSession>(e); return s ? s->desc : nullptr; }
bool IsPC(entt::entity e) { Live(e); return g_registry.all_of<ecs::TagPC>(e); }
bool IsNPCType(entt::entity e) { Live(e); return g_registry.get<ecs::CharacterType>(e).value == CHAR_TYPE_NPC; }
uint32_t GetRaceNum(entt::entity e) { Live(e); return g_registry.get<ecs::RaceState>(e).baseRace; }
std::string_view GetName(entt::entity e) { Live(e); return g_registry.get<ecs::PlayerName>(e).value; }
uint8_t GetEmpire(entt::entity e) { Live(e); return 1; }
int GetStamina(entt::entity e) { Live(e); return 100; }
}
namespace ecs::SocialSystem {
LPPARTY GetParty(entt::entity e) { Live(e); return nullptr; }
CGuild* GetGuild(entt::entity e) { Live(e); return nullptr; }
void SendGuildName(entt::entity e, CGuild*) { Live(e); }
}
namespace ecs::PointSystem {
int64_t Get(entt::entity e, uint8_t type) { Live(e); Check(type == POINT_MOV_SPEED || type == POINT_ATT_SPEED, "unexpected packet point"); return 150; }
int GetLevel(entt::entity e) { Live(e); return 30; }
}
uint32_t MountSystem::GetMountVnum(entt::entity e) { Live(e); return g_registry.get<ecs::MountState>(e).mountVnum; }
void MountSystem::UpdateMountInventoryCountOverhead(entt::entity source, entt::entity viewer) { Live(source); Live(viewer); }

// Unrelated entry points in the complete production translation units remain
// linked, but must never supply behavior to these direct encoding tests.
int VIEW_RANGE = 5000, VIEW_BONUS_RANGE = 500;
bool SectreeMember(entt::entity, const SECTREE*) { Unexpected(); }
FCollectEntity SECTREE::SnapshotAround(int) const { Unexpected(); }
SECTREE* SECTREE_MANAGER::Get(int32_t, int32_t, int32_t) { Unexpected(); }
int MINMAX(int, int, int) { Unexpected(); }
float get_float_time() { Unexpected(); }
namespace ecs::PlayerRuntime {
uint32_t GetPlayerID(entt::entity) { Unexpected(); }
uint32_t GetPacketVID(entt::entity) { Unexpected(); }
uint32_t GetAIFlag(entt::entity) { Unexpected(); }
void SetLastSyncTime(entt::entity, const timeval&) { Unexpected(); }
int32_t GetX(entt::entity) { Unexpected(); }
int32_t GetY(entt::entity) { Unexpected(); }
LPSECTREE GetSectree(entt::entity) { Unexpected(); }
bool IsNPC(entt::entity) { Unexpected(); }
bool IsStone(entt::entity) { Unexpected(); }
bool IsMonster(entt::entity) { Unexpected(); }
}
void CombatSystem::SendDamagePacket(entt::entity, entt::entity, int, uint8_t) { Unexpected(); }
TEMP_BUFFER::TEMP_BUFFER(int, bool) { Unexpected(); }
TEMP_BUFFER::~TEMP_BUFFER() = default;
const void* TEMP_BUFFER::read_peek() { Unexpected(); }
void TEMP_BUFFER::write(const void*, int) { Unexpected(); }
int TEMP_BUFFER::size() { Unexpected(); }
entt::entity CHARACTER_MANAGER::FindEntity(uint32_t) { Unexpected(); }
entt::entity CParty::GetLeader() { Unexpected(); }
uint8_t CParty::GetRole(uint32_t) { Unexpected(); }
int ItemSystem::GetItemValue(entt::entity, uint32_t) { Unexpected(); }
const char* ItemSystem::GetItemName(entt::entity) { Unexpected(); }
bool battle_is_attackable(entt::entity, entt::entity) { Unexpected(); }

int main() {
    try {
        MountWireRoundTrip(); ExpiredAndMissingSession();
        std::cout << "Entity wire checks passed: " << checks << '\n'; return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
