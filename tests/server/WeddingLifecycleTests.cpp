#include "../../SRC/Server/GameServer/stdafx.h"
#include "../../SRC/Server/GameServer/wedding.h"
#include "../../SRC/Server/GameServer/desc_manager.h"
#include "../../SRC/Server/GameServer/desc_client.h"
#include "../../SRC/Server/GameServer/sectree_manager.h"
#include "../../SRC/Server/GameServer/regen.h"
#include "../../SRC/Server/GameServer/locale_service.h"
#include "../../SRC/Server/GameServer/config.h"
#include "../../SRC/Server/GameServer/event.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/components/social_components.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ItemSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PointSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ChatSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/MovementSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include <Core/Logging.hpp>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

// Real wedding.cpp with entity-only fixtures. Sectree/desc/DB services are
// doubles; no private map file, socket or database is touched.
entt::registry g_registry;
int passes_per_sec = 25;
namespace { int dbMarker = 0; }
LPCLIENT_DESC db_clientdesc = reinterpret_cast<LPCLIENT_DESC>(&dbMarker);

namespace {
int checks = 0, liveEvents = 0, cancels = 0, dbPackets = 0, descDestroyed = 0;
int privateMapsCreated = 0, privateMapsDestroyed = 0, privateMapIndex = 810001;
bool allowFind = true;
std::vector<std::pair<entt::entity, std::string>> chat;
std::vector<entt::entity> warped, observerOn, observerOff;
struct Grant { entt::entity character; uint32_t vnum; uint32_t count; };
std::vector<Grant> granted;
std::vector<int32_t> destroyedMapIndices;
std::unordered_map<LPDESC, entt::entity> descOwners;
std::function<void(entt::entity)> onDestroyDesc;
LPEVENT scheduled;
std::vector<LPEVENT> retainedEvents;

struct Player { uint32_t pid = 0; uint8_t level = 10; };

void Check(bool value, const char* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
entt::entity PlayerEntity(uint32_t pid, uint8_t level = 10) {
    const auto entity = g_registry.create();
    g_registry.emplace<Player>(entity).pid = pid;
    g_registry.get<Player>(entity).level = level;
    return entity;
}
void Reset() {
    g_registry.clear();
    scheduled.reset();
    retainedEvents.clear();
    Check(liveEvents == 0, "event leaked");
    chat.clear(); warped.clear(); observerOn.clear(); observerOff.clear(); granted.clear();
    descOwners.clear(); destroyedMapIndices.clear();
    onDestroyDesc = {};
    dbPackets = descDestroyed = privateMapsCreated = privateMapsDestroyed = 0;
    privateMapIndex = 810001;
    allowFind = true;
}
LPDESC DescToken(entt::entity e) { return reinterpret_cast<LPDESC>(static_cast<uintptr_t>(0x1000 + entt::to_entity(e))); }
SECTREE_MAP testMap;
}

// Production WeddingManager instance (main.cpp constructs one as well).
marriage::WeddingManager wedding_manager;
DESC_MANAGER desc_manager;
SECTREE_MANAGER sectree_manager;

std::shared_ptr<spdlog::logger> logging::GetErrorLogger() {
    static auto logger = std::make_shared<spdlog::logger>("wedding-error-test");
    return logger;
}
std::shared_ptr<spdlog::logger> logging::GetLogger() {
    static auto logger = std::make_shared<spdlog::logger>("wedding-test");
    return logger;
}
void DESC::Packet(const void*, int) {}

// Event scheduler doubles: the test drives callbacks explicitly.
void intrusive_ptr_add_ref(EVENT* e) { ++e->ref_count; }
void intrusive_ptr_release(EVENT* e) { if (--e->ref_count == 0) { --liveEvents; delete e; } }
LPEVENT event_create_ex(TEVENTFUNC func, event_info_data* info, int32_t) {
    LPEVENT result(new EVENT);
    ++liveEvents;
    result->func = func; result->info = info;
    scheduled = result;
    retainedEvents.push_back(result);
    return result;
}
void event_cancel(LPEVENT* event) {
    if (*event) { ++cancels; (*event)->is_force_to_end = true; }
    event->reset();
}
void CLIENT_DESC::DBPacket(uint8_t, uint32_t, const void*, uint32_t) { ++dbPackets; }

SECTREE_MAP::SECTREE_MAP() = default;
SECTREE_MAP::~SECTREE_MAP() = default;
SECTREE_MAP::SECTREE_MAP(SECTREE_MAP&) = default;
SECTREE_MANAGER::SECTREE_MANAGER() = default;
SECTREE_MANAGER::~SECTREE_MANAGER() = default;
DESC_MANAGER::DESC_MANAGER() = default;
DESC_MANAGER::~DESC_MANAGER() = default;
int32_t SECTREE_MANAGER::CreatePrivateMap(int32_t) { ++privateMapsCreated; return privateMapIndex; }
void SECTREE_MANAGER::DestroyPrivateMap(int32_t index) { ++privateMapsDestroyed; destroyedMapIndices.push_back(index); }
LPSECTREE_MAP SECTREE_MANAGER::GetMap(int32_t) { return &testMap; }

bool map_allow_find(int32_t) { return allowFind; }
bool regen_do(const char*, int32_t, int, int, LPDUNGEON, bool) { return true; }
const std::string& LocaleService_GetMapPath() { static const std::string path = "/never/read"; return path; }

namespace ecs::PlayerRuntime {
bool IsValid(entt::entity e) { return e != entt::null && g_registry.valid(e); }
bool IsPC(entt::entity e) { return g_registry.valid(e) && g_registry.all_of<Player>(e); }
LPDESC GetDesc(entt::entity e) {
    if (!IsPC(e)) return nullptr;
    const auto token = DescToken(e);
    descOwners[token] = e;
    return token;
}
uint32_t GetPlayerID(entt::entity e) {
    const auto* player = g_registry.try_get<Player>(e);
    return player ? player->pid : 0;
}
std::string_view GetName(entt::entity e) { return IsPC(e) ? "player" : "other"; }
void DestroyCharacter(entt::entity e) {
    marriage::WeddingSystem::SetMemberMap(e, entt::null);
    if (g_registry.valid(e)) g_registry.destroy(e);
}
void SetObserverMode(entt::entity e, bool on) {
    Check(g_registry.valid(e), "observer mode on a stale entity");
    (on ? observerOn : observerOff).push_back(e);
}
}

namespace ecs::PointSystem {
int GetLevel(entt::entity e) {
    const auto* player = g_registry.try_get<Player>(e);
    return player ? player->level : 10;
}
}

namespace ecs {
void ChatSystem::Send(entt::entity e, uint8_t, const char* format, ...) {
    chat.emplace_back(e, format ? format : "");
}
#ifdef TEXTS_IMPROVEMENT
void ChatSystem::SendNew(entt::entity e, uint8_t, uint32_t, const char* format, ...) {
    chat.emplace_back(e, format ? format : "");
}
#endif
}

namespace ecs::MovementSystem {
void ExitToSavedLocation(entt::entity e) { warped.push_back(e); }
}

namespace ItemSystem {
entt::entity AutoGiveItemEcs(entt::entity character, uint32_t vnum, uint32_t count, int, bool, bool) {
    // Non-player members (mob parties) have no inventory to receive into.
    if (ecs::PlayerRuntime::IsPC(character))
        granted.push_back({character, vnum, count});
    return entt::null;
}
}

void DESC_MANAGER::DestroyDesc(LPDESC desc, bool) {
    ++descDestroyed;
    const auto it = descOwners.find(desc);
    if (it == descOwners.end())
        return;
    const entt::entity owner = it->second;
    descOwners.erase(it);
    if (onDestroyDesc)
        onDestroyDesc(owner);
}

namespace {
marriage::WeddingManager& Manager() { return marriage::WeddingManager::instance(); }
ecs::WeddingMapState& State(entt::entity map) { return g_registry.get<ecs::WeddingMapState>(map); }

bool HasChat(entt::entity character, const std::string& text) {
    for (const auto& [target, message] : chat)
        if (target == character && message == text)
            return true;
    return false;
}

void RequestFailureAndRecovery() {
    Reset();
    allowFind = false;
    Manager().Request(1, 2);
    Check(privateMapsCreated == 0 && Manager().Find(privateMapIndex) == entt::null, "disallowed wedding map was created");
    allowFind = true;
    Manager().Request(1, 2);
    const auto map = Manager().Find(privateMapIndex);
    Check(map != entt::null && privateMapsCreated == 1 && dbPackets == 1, "wedding request did not publish a map");
    Check(State(map).mapIndex == privateMapIndex && State(map).pid1 == 1 && State(map).pid2 == 2,
        "wedding map lost its durable identifiers");

    // A directly retired map must not be reachable, and a new request on the
    // same private index replaces the stale index entry.
    g_registry.destroy(map);
    Check(Manager().Find(privateMapIndex) == entt::null, "stale map handle stayed reachable");
    Manager().Request(3, 4);
    const auto replacement = Manager().Find(privateMapIndex);
    Check(replacement != entt::null && replacement != map && State(replacement).pid1 == 3,
        "stale manager entry was not replaced");
}

void MembershipLifecycle() {
    Reset();
    Manager().Request(101, 102);
    const auto map = Manager().Find(privateMapIndex);
    Check(map != entt::null, "wedding map missing");

    const auto leader = PlayerEntity(101, 20);
    const auto guest = PlayerEntity(102, 5);
    const auto other = PlayerEntity(103, 20);

    marriage::WeddingSystem::SetMemberMap(leader, map);
    marriage::WeddingSystem::SetMemberMap(guest, map);
    Check(marriage::WeddingSystem::IsMember(map, leader) && marriage::WeddingSystem::IsMember(map, guest)
        && !marriage::WeddingSystem::IsMember(map, other), "membership set is wrong");
    Check(State(map).members.size() == 2, "member count mismatch");
    Check(marriage::WeddingSystem::GetMemberMap(leader) == map
        && marriage::WeddingSystem::GetMemberMap(other) == entt::null, "character relation mismatch");
    Check(observerOn.size() == 1 && observerOn.front() == guest, "low level guest did not become an observer");
    Check(HasChat(guest, "DayMode dark") == false, "local event was sent before it was set");

    // Repeating the same map is idempotent.
    marriage::WeddingSystem::SetMemberMap(leader, map);
    Check(State(map).members.size() == 2 && observerOn.size() == 1, "repeated join changed membership");

    // Null map releases the relation and clears the observer flag.
    marriage::WeddingSystem::SetMemberMap(guest, entt::null);
    Check(!marriage::WeddingSystem::IsMember(map, guest) && State(map).members.size() == 1,
        "leaving the map did not remove the member");
    Check(observerOff.size() == 1 && observerOff.front() == guest, "observer flag was not cleared");
    Check(marriage::WeddingSystem::GetMemberMap(guest) == entt::null, "stale relation survived the leave");

    // A retired map handle never joins and never touches the character.
    const auto stale = map;
    Manager().DestroyWeddingMap(map);
    marriage::WeddingSystem::SetMemberMap(other, stale);
    Check(marriage::WeddingSystem::GetMemberMap(other) == entt::null
        && g_registry.get<ecs::MarriageState>(other).weddingMap == entt::null,
        "stale map handle entered the relation");
}

void FlagsAndCommands() {
    Reset();
    Manager().Request(1, 2);
    const auto map = Manager().Find(privateMapIndex);
    const auto first = PlayerEntity(1, 20), second = PlayerEntity(2, 20);
    marriage::WeddingSystem::SetMemberMap(first, map);
    marriage::WeddingSystem::SetMemberMap(second, map);
    chat.clear();

    marriage::WeddingSystem::SetDark(map, true);
    Check(chat.size() == 2 && HasChat(first, "DayMode dark") && HasChat(second, "DayMode dark"),
        "dark mode was not broadcast");
    marriage::WeddingSystem::SetDark(map, true);
    Check(chat.size() == 2, "repeated dark mode rebroadcast");

    marriage::WeddingSystem::SetSnow(map, true);
    Check(chat.size() == 4 && HasChat(first, "xmas_snow 1"), "snow mode was not broadcast");

    marriage::WeddingSystem::SetMusic(map, true, "wedding.mp3");
    Check(marriage::WeddingSystem::IsPlayingMusic(map) && HasChat(second, "PlayMusic 1 wedding.mp3"),
        "music start was not broadcast");
    marriage::WeddingSystem::SetMusic(map, false, "wedding.mp3");
    Check(!marriage::WeddingSystem::IsPlayingMusic(map) && HasChat(first, "PlayMusic 0 default"),
        "music stop did not restore the default track");

    // A late joiner receives the current local state.
    chat.clear();
    const auto late = PlayerEntity(3, 20);
    marriage::WeddingSystem::SetMemberMap(late, map);
    Check(HasChat(late, "DayMode dark") && HasChat(late, "xmas_snow 1"), "late joiner missed the local events");
    Check(!HasChat(late, "PlayMusic 1 wedding.mp3"), "stopped music was replayed to the late joiner");
}

void WarpAndEndEvent() {
    Reset();
    Manager().Request(1, 2);
    const auto map = Manager().Find(privateMapIndex);
    const auto leader = PlayerEntity(1, 20), partner = PlayerEntity(2, 20);
    const auto guest = PlayerEntity(3, 20), lowLevel = PlayerEntity(4, 5);
    const auto npc = g_registry.create(); // Not a player: members can be NPCs.
    marriage::WeddingSystem::SetMemberMap(leader, map);
    marriage::WeddingSystem::SetMemberMap(partner, map);
    marriage::WeddingSystem::SetMemberMap(guest, map);
    marriage::WeddingSystem::SetMemberMap(lowLevel, map);
    marriage::WeddingSystem::SetMemberMap(npc, map);

    marriage::WeddingSystem::WarpAll(map);
    Check(warped.size() == 4 && warped[0] == leader && warped[1] == partner && warped[2] == guest
        && warped[3] == lowLevel, "warp touched the wrong members");
    Check(liveEvents == 0, "warp scheduled an event");

    marriage::WeddingSystem::SetEnded(map);
    Check(State(map).endEvent != nullptr && liveEvents == 1, "end event was not scheduled");
    const auto event = State(map).endEvent;
    marriage::WeddingSystem::SetEnded(map);
    Check(liveEvents == 1 && State(map).endEvent == event, "second end event was scheduled");

    // The couple and sub-level-10 members are excluded from the ceremony gift.
    Check(granted.size() == 1 && granted.front().character == guest
        && granted.front().vnum == 27002 && granted.front().count == 5, "ceremony gift policy changed");

    warped.clear();
    Check(event->func(event, 0) == PASSES_PER_SEC(15), "end event did not enter the warp step");
    Check(warped.size() == 4, "end event warp step missed members");

    Check(event->func(event, 0) == 0, "end event did not finish");
    Check(Manager().Find(privateMapIndex) == entt::null && !g_registry.valid(map)
        && privateMapsDestroyed == 1 && destroyedMapIndices.size() == 1
        && destroyedMapIndices.front() == privateMapIndex, "finished ceremony was not destroyed");
    Check(event->func(event, 0) == 0 && privateMapsDestroyed == 1, "stale end event acted again");
}

void DestroyAllReentrancy() {
    Reset();
    Manager().Request(1, 2);
    const auto map = Manager().Find(privateMapIndex);
    const auto first = PlayerEntity(1, 20), second = PlayerEntity(2, 20), third = PlayerEntity(3, 20);
    marriage::WeddingSystem::SetMemberMap(first, map);
    marriage::WeddingSystem::SetMemberMap(second, map);
    marriage::WeddingSystem::SetMemberMap(third, map);

    // The production disconnect path removes the member from the map while the
    // map is being torn down; the loop must terminate and visit each once.
    onDestroyDesc = [](entt::entity e) {
        marriage::WeddingSystem::SetMemberMap(e, entt::null);
        if (g_registry.valid(e)) g_registry.destroy(e);
    };
    marriage::WeddingSystem::DestroyAll(map);
    Check(descDestroyed == 3 && State(map).members.empty(), "destroy-all did not finish");
    Check(!g_registry.valid(first) && !g_registry.valid(second) && !g_registry.valid(third),
        "destroy-all left characters behind");
    Check(g_registry.valid(map), "destroy-all retired the map entity early");
    marriage::WeddingSystem::DestroyAll(map);
    Check(descDestroyed == 3, "repeated destroy-all ran callbacks again");

    // The manager-level destroy removes the map and its private map.
    Manager().DestroyWeddingMap(map);
    Check(!g_registry.valid(map) && Manager().Find(privateMapIndex) == entt::null && privateMapsDestroyed == 1,
        "manager destroy left map state");
    onDestroyDesc = {};
}
}

int main() {
    try {
        RequestFailureAndRecovery();
        MembershipLifecycle();
        FlagsAndCommands();
        WarpAndEndEvent();
        DestroyAllReentrancy();
        Reset();
        std::cout << "Wedding lifecycle checks passed: " << checks << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
