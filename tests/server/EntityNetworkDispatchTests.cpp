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
#include "../../SRC/Server/GameServer/ecs/services/SpatialService.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/NetworkSyncSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ViewSystem.hpp"
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
#include <functional>
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
std::map<const DESC*, std::vector<uint8_t>> bufferedWire;
std::function<void(const DESC*)> onWire;
SECTREE* updateSector = nullptr;
std::vector<entt::entity> updateMembers;
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
entt::entity WorldObject(ecs::SpatialKind kind, uint32_t vid) {
    const auto e = g_registry.create();
    g_registry.emplace<ecs::SpatialKindTag>(e, kind);
    g_registry.emplace<ecs::VIDComponent>(e, vid);
    g_registry.emplace<ecs::Position>(e, 12500, 678, 22);
    return e;
}
void NativeBuildingPackets() {
    DESC desc;
    const auto viewer = Character(2000, true, &desc);
    const auto building = WorldObject(ecs::SpatialKind::Building, 2001);
    ecs::BuildingState state {};
    state.vnum = 14061; state.landId = 33; state.guildId = 44;
    state.rotationX = 10; state.rotationY = 20; state.rotationZ = 90;
    g_registry.emplace<ecs::BuildingState>(building, state);
    ecs::EntityNetworkDispatch::SendInsert(g_registry, building, viewer);
    ecs::EntityNetworkDispatch::SendRemove(g_registry, building, viewer);
    Check(wire.at(&desc).size() == 2, "building emitted extra character/mount frames");
    const auto add = Packet<TPacketGCCharacterAdd>(desc, 0, HEADER_GC_CHARACTER_ADD);
    const auto del = Packet<TPacketGCCharacterDelete>(desc, 1, HEADER_GC_CHARACTER_DEL);
    Check(add.dwVID == 2001 && del.id == 2001 && add.bType == CHAR_TYPE_BUILDING && add.wRaceNum == 14061,
        "building ADD/DEL identity or type mismatch");
    Check(add.x == 12500 && add.y == 678 && add.z == 22 && add.angle == 90 &&
        add.dwAffectFlag[0] == 10 && add.dwAffectFlag[1] == 20,
        "building lost position or its three protocol rotation fields");
#ifdef ENABLE_MULTI_NAMES
    Check(add.transname, "building lost translated-name marker");
#endif
    wire.clear();
    g_registry.remove<ecs::BuildingState>(building);
    ecs::EntityNetworkDispatch::SendInsert(g_registry, building, viewer);
    Check(wire.empty(), "building without state emitted ADD");
    g_registry.emplace<ecs::BuildingState>(building, state);
    g_registry.remove<ecs::VIDComponent>(building);
    ecs::EntityNetworkDispatch::SendInsert(g_registry, building, viewer);
    ecs::EntityNetworkDispatch::SendRemove(g_registry, building, viewer);
    Check(wire.empty(), "building without identity emitted packets");
    g_registry.destroy(building);
    const auto replacement = WorldObject(ecs::SpatialKind::Building, 2002);
    g_registry.emplace<ecs::BuildingState>(replacement, state);
    Check(replacement != building, "building generation did not advance");
    ecs::EntityNetworkDispatch::SendInsert(g_registry, building, viewer);
    ecs::EntityNetworkDispatch::SendRemove(g_registry, building, viewer);
    Check(wire.empty(), "retired building emitted replacement generation packets");
    ecs::EntityNetworkDispatch::SendInsert(g_registry, replacement, viewer);
    Check(Packet<TPacketGCCharacterAdd>(desc, 0, HEADER_GC_CHARACTER_ADD).dwVID == 2002,
        "replacement building reused retired wire identity");
    wire.clear(); g_registry.get<ecs::NetworkSession>(viewer).desc = nullptr;
    ecs::EntityNetworkDispatch::SendInsert(g_registry, replacement, viewer);
    ecs::EntityNetworkDispatch::SendRemove(g_registry, replacement, viewer);
    Check(wire.empty(), "building sent to missing viewer session");
    g_registry.clear();
}
#ifdef ENABLE_NEW_SHOP_IN_CITIES
template<class T> T ShopPayload(const DESC& desc, size_t index, uint8_t subheader) {
    const auto& bytes = wire.at(&desc).at(index);
    Check(bytes.size() == sizeof(TPacketGCNewOfflineshop) + sizeof(T), "wrong shop framed packet length");
    TPacketGCNewOfflineshop header {};
    std::memcpy(&header, bytes.data(), sizeof(header));
    Check(header.bHeader == HEADER_GC_NEW_OFFLINESHOP && header.bSubHeader == subheader && header.wSize == bytes.size(),
        "wrong shop main header, subheader or advertised length");
    T result {};
    std::memcpy(&result, bytes.data() + sizeof(header), sizeof(result));
    return result;
}
void NativeShopPackets() {
    DESC desc;
    const auto viewer = Character(3000, true, &desc);
    const auto shop = WorldObject(ecs::SpatialKind::OfflineShop, 3001);
    ecs::OfflineShopState state {};
    state.vid = 3001; state.race = 30003; state.shopType = 2;
    state.name = std::string(200, 'n'); state.ownerPID = 77;
    g_registry.emplace<ecs::OfflineShopState>(shop, state);
    ecs::EntityNetworkDispatch::SendInsert(g_registry, shop, viewer);
    ecs::EntityNetworkDispatch::SendRemove(g_registry, shop, viewer);
    Check(wire.at(&desc).size() == 2 && bufferedWire.empty(), "shop header/payload pairing incomplete");
    const auto add = ShopPayload<TSubPacketGCInsertShopEntity>(desc, 0, offlineshop::SUBHEADER_GC_INSERT_SHOP_ENTITY);
    const auto del = ShopPayload<TSubPacketGCRemoveShopEntity>(desc, 1, offlineshop::SUBHEADER_GC_REMOVE_SHOP_ENTITY);
    Check(add.dwVID == 3001 && del.dwVID == 3001 && add.iType == 2,
        "shop ADD/DEL identity or type mismatch");
    Check(add.x == 12500 && add.y == 678 && add.z == 22, "shop position mismatch");
    Check(add.szName[sizeof(add.szName) - 1] == '\0' &&
        std::string(add.szName) == std::string(sizeof(add.szName) - 1, 'n'), "shop name not safely truncated");
#ifdef KASMIR_PAKET_SYSTEM
    Check(add.dwKasmirNpc == 30003, "shop race lost on the wire");
#endif
    wire.clear();
    g_registry.remove<ecs::Position>(shop);
    ecs::EntityNetworkDispatch::SendInsert(g_registry, shop, viewer);
    Check(wire.empty() && bufferedWire.empty(), "shop without position emitted partial ADD");
    g_registry.emplace<ecs::Position>(shop, 12500, 678, 22);
    g_registry.remove<ecs::OfflineShopState>(shop);
    ecs::EntityNetworkDispatch::SendInsert(g_registry, shop, viewer);
    ecs::EntityNetworkDispatch::SendRemove(g_registry, shop, viewer);
    Check(wire.empty() && bufferedWire.empty(), "shop without state emitted packets");
    g_registry.destroy(shop);
    const auto replacement = WorldObject(ecs::SpatialKind::OfflineShop, 3002);
    state.vid = 3002; g_registry.emplace<ecs::OfflineShopState>(replacement, state);
    Check(replacement != shop, "shop generation did not advance");
    ecs::EntityNetworkDispatch::SendInsert(g_registry, shop, viewer);
    ecs::EntityNetworkDispatch::SendRemove(g_registry, shop, viewer);
    Check(wire.empty() && bufferedWire.empty(), "retired shop emitted replacement generation packets");
    ecs::EntityNetworkDispatch::SendInsert(g_registry, replacement, viewer);
    Check(ShopPayload<TSubPacketGCInsertShopEntity>(desc, 0, offlineshop::SUBHEADER_GC_INSERT_SHOP_ENTITY).dwVID == 3002,
        "replacement shop reused retired wire identity");
    wire.clear(); g_registry.get<ecs::NetworkSession>(viewer).desc = nullptr;
    ecs::EntityNetworkDispatch::SendInsert(g_registry, replacement, viewer);
    ecs::EntityNetworkDispatch::SendRemove(g_registry, replacement, viewer);
    Check(wire.empty() && bufferedWire.empty(), "shop sent to missing viewer session");
    g_registry.clear();
}
#endif
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
void UpdateAfterVisibilityRemoval() {
    DESC ownerDesc, observerDesc;
    SECTREE tree;
    updateSector = &tree;
    const auto owner = Character(800, true, &ownerDesc);
    const auto observer = Character(801, true, &observerDesc);
    const auto follower = Character(802, false);
    updateMembers = {owner, observer, follower};
    for (const auto entity : updateMembers) {
        g_registry.emplace<ecs::SectorPlacement>(entity, 3, 6200u, 100u);
        g_registry.emplace<ecs::ViewActiveTag>(entity);
    }
    // Actual insert packets precede actual UpdatePacket, not a replacement
    // implementation of either production builder or transport selection.
    for (const auto viewer : {owner, observer})
        ecs::EntityNetworkDispatch::SendInsert(g_registry, follower, viewer);
    wire.clear();
    NetworkSyncSystem::UpdatePacket(follower);
    size_t activeAdditional = 0;
    for (const auto* desc : {&ownerDesc, &observerDesc}) {
        const auto update = Packet<TPacketGCCharacterUpdate>(*desc, 0, HEADER_GC_CHARACTER_UPDATE);
        Check(update.dwVID == 802, "active update lost follower identity");
        for (const auto& bytes : wire.at(desc))
            activeAdditional += bytes.front() == HEADER_GC_CHAR_ADDITIONAL_INFO;
    }
    // Fixture boundary: this is the precise state left by real ViewCleanup
    // (separately exercised by SpatialLifecycleTests) before DestroyStatePre
    // calls ClearAffect: REMOVE sent, SpatialEntity/ViewActiveTag gone, but
    // the entity, VID index and SectorPlacement still exist until StatePost.
    for (const auto viewer : {owner, observer})
        ecs::EntityNetworkDispatch::SendRemove(g_registry, follower, viewer);
    g_registry.remove<ecs::SpatialEntity, ecs::ViewActiveTag>(follower);
    Check(g_registry.valid(follower) && g_registry.all_of<ecs::VIDComponent, ecs::SectorPlacement>(follower),
        "post-removal fixture prematurely retired identity or sector placement");
    wire.clear();
    NetworkSyncSystem::UpdatePacket(follower);
    size_t afterRemoval = 0;
    for (const auto& [desc, frames] : wire) {
        afterRemoval += frames.size();
        for (const auto& bytes : frames)
            std::cerr << "post-REMOVE wire header=" << unsigned(bytes.front()) << " bytes=" << bytes.size() << '\n';
    }
    std::cerr << "active standalone AdditionalInfo=" << activeAdditional
        << ", packets after visibility removal=" << afterRemoval << '\n';
    updateMembers.clear(); updateSector = nullptr; g_registry.clear(); wire.clear();
    Check(afterRemoval == 0, "UpdatePacket emitted packets after visibility removal (ghost mount resurrection)");
    Check(activeAdditional == 0, "active UPDATE emitted append-only AdditionalInfo without ADD");
}
void NativeMoveBroadcastRouting() {
    DESC ownerDesc, observerDesc, sourceDesc;
    SECTREE tree;
    updateSector = &tree;
    const auto owner = Character(900, true, &ownerDesc);
    const auto observer = Character(901, true, &observerDesc);
    const auto follower = Character(902, false);
    updateMembers = {owner, observer, follower};
    for (const auto entity : updateMembers) {
        g_registry.emplace<ecs::SectorPlacement>(entity, 3, 6200u, 100u);
        g_registry.emplace<ecs::ViewActiveTag>(entity);
        g_registry.emplace<ecs::SpatialRevision>(entity, 1u);
    }
    // Exercise entity.cpp's real PacketView and visibility recipient selection.
    // This is a transport/routing fixture; MovementSystem's actual MOVE encoder
    // is exercised separately by SpatialLifecycleTests.
    TPacketGCMove move {};
    move.bHeader = HEADER_GC_MOVE;
    move.bFunc = FUNC_MOVE;
    move.bRot = 18;
    move.dwVID = 902;
    move.lX = 6800;
    move.lY = 100;
    move.dwTime = now;
    move.dwDuration = 900;
    ecs::ViewSystem::PacketView(follower, &move, sizeof(move), follower);
    Check(wire.size() == 2, "native NPC MOVE missed owner or observer");
    for (const auto* desc : {&ownerDesc, &observerDesc}) {
        Check(wire.at(desc).size() == 1, "native NPC MOVE sent duplicate packets");
        const auto received = Packet<TPacketGCMove>(*desc, 0, HEADER_GC_MOVE);
        Check(std::memcmp(&received, &move, sizeof(move)) == 0, "PacketView altered MOVE payload bytes");
    }
    // A source session makes the exclusion assertion meaningful: no packet
    // may reach it even though the source is present in GetViewersOf.
    g_registry.emplace<ecs::NetworkSession>(follower, &sourceDesc);
    wire.clear();
    ecs::ViewSystem::PacketView(follower, &move, sizeof(move), follower);
    Check(wire.size() == 2 && wire.count(&sourceDesc) == 0, "MOVE source exclusion was ignored");
    wire.clear();
    ecs::ViewSystem::PacketView(follower, &move, sizeof(move));
    Check(wire.size() == 3 && wire.at(&sourceDesc).size() == 1, "non-excluded source session did not receive MOVE");
    g_registry.remove<ecs::NetworkSession>(follower);

    g_registry.get<ecs::NetworkSession>(observer).desc = nullptr;
    wire.clear();
    ecs::ViewSystem::PacketView(follower, &move, sizeof(move), follower);
    Check(wire.size() == 1 && wire.at(&ownerDesc).size() == 1, "null observer session blocked owner or emitted MOVE");
    g_registry.get<ecs::NetworkSession>(observer).desc = &observerDesc;

    // A transport callback can remove/reposition a source. Remaining recipients
    // from the old snapshot must not receive its now-stale MOVE packet.
    wire.clear();
    onWire = [follower, &ownerDesc](const DESC* recipient) {
        Check(recipient == &ownerDesc, "revision test did not send to first viewer first");
        ++g_registry.get<ecs::SpatialRevision>(follower).value;
    };
    ecs::ViewSystem::PacketView(follower, &move, sizeof(move), follower);
    onWire = {};
    Check(wire.size() == 1 && wire.at(&ownerDesc).size() == 1, "spatial revision change leaked stale MOVE to later viewer");

    wire.clear();
    g_registry.remove<ecs::SectorPlacement>(follower);
    ecs::ViewSystem::PacketView(follower, &move, sizeof(move), follower);
    Check(wire.empty(), "detached source broadcast MOVE");
    g_registry.destroy(follower);
    ecs::ViewSystem::PacketView(follower, &move, sizeof(move), follower);
    Check(wire.empty(), "retired source broadcast MOVE");
    updateMembers.clear(); updateSector = nullptr; g_registry.clear(); wire.clear();
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
    auto pending = bufferedWire.find(this);
    if (pending == bufferedWire.end()) {
        wire[this].emplace_back(bytes, bytes + size);
    } else {
        pending->second.insert(pending->second.end(), bytes, bytes + size);
        wire[this].push_back(std::move(pending->second));
        bufferedWire.erase(pending);
    }
    if (onWire) onWire(this);
}
void DESC::BufferedPacket(const void* data, int size) {
    Check(data && size > 0, "invalid buffered transport payload");
    const auto* bytes = static_cast<const uint8_t*>(data);
    auto& pending = bufferedWire[this];
    pending.insert(pending.end(), bytes, bytes + size);
}
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
void ecs::ViewSystem::ViewCleanup(entt::entity) { Unexpected(); }
SECTREE::SECTREE() = default;
SECTREE::~SECTREE() = default;
bool SECTREE::Contains(entt::entity e) const {
    return this == updateSector && g_registry.valid(e) && g_registry.all_of<ecs::SectorPlacement>(e) &&
        std::find(updateMembers.begin(), updateMembers.end(), e) != updateMembers.end();
}
bool SectreeMember(entt::entity e, const SECTREE* tree) { return tree && tree->Contains(e); }
FCollectEntity SECTREE::SnapshotAround(int) const {
    Check(this == updateSector, "unexpected update broadcast sector");
    FCollectEntity result;
    for (const auto e : updateMembers) result.Add(e, this);
    return result;
}
SECTREE_MANAGER::SECTREE_MANAGER() = default;
SECTREE_MANAGER::~SECTREE_MANAGER() = default;
SECTREE* SECTREE_MANAGER::Get(int32_t map, int32_t, int32_t) { return map == 3 ? updateSector : nullptr; }
int MINMAX(int, int, int) { Unexpected(); }
float get_float_time() { Unexpected(); }
namespace ecs::PlayerRuntime {
uint32_t GetPlayerID(entt::entity) { Unexpected(); }
uint32_t GetPacketVID(entt::entity) { Unexpected(); }
uint32_t GetAIFlag(entt::entity) { Unexpected(); }
void SetLastSyncTime(entt::entity, const timeval&) { Unexpected(); }
int32_t GetX(entt::entity) { Unexpected(); }
int32_t GetY(entt::entity) { Unexpected(); }
LPSECTREE GetSectree(entt::entity e) { return ecs::SectorOf(g_registry, e); }
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
CHARACTER_MANAGER::CHARACTER_MANAGER() = default;
CHARACTER_MANAGER::~CHARACTER_MANAGER() = default;
entt::entity CHARACTER_MANAGER::FindEntity(uint32_t vid) {
    for (const auto e : g_registry.view<ecs::VIDComponent>())
        if (g_registry.get<ecs::VIDComponent>(e).value == vid) return e;
    return entt::null;
}
entt::entity CParty::GetLeader() { Unexpected(); }
uint8_t CParty::GetRole(uint32_t) { Unexpected(); }
int ItemSystem::GetItemValue(entt::entity, uint32_t) { Unexpected(); }
const char* ItemSystem::GetItemName(entt::entity) { Unexpected(); }
bool battle_is_attackable(entt::entity, entt::entity) { Unexpected(); }

int main() {
    try {
        CHARACTER_MANAGER characters; SECTREE_MANAGER maps;
        NativeBuildingPackets();
#ifdef ENABLE_NEW_SHOP_IN_CITIES
        NativeShopPackets();
#endif
        MountWireRoundTrip(); ExpiredAndMissingSession();
        UpdateAfterVisibilityRemoval();
        NativeMoveBroadcastRouting();
        std::cout << "Entity wire checks passed: " << checks << '\n'; return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
