#include "../../SRC/Server/GameServer/core/stdafx.h"
#include "../../SRC/Server/GameServer/social/marriage.h"
#include "../../SRC/Server/GameServer/social/wedding.h"
#include "../../SRC/Server/GameServer/entity/char_manager.h"
#include "../../SRC/Server/GameServer/network/desc.h"
#include "../../SRC/Server/GameServer/network/desc_client.h"
#include "../../SRC/Server/GameServer/network/p2p.h"
#include "../../SRC/Server/GameServer/world/sectree_manager.h"
#include "../../SRC/Server/GameServer/world/event.h"
#include "../../SRC/Server/GameServer/quest/questmanager.h"
#include "../../SRC/Server/GameServer/core/config.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/PIDRegistry.hpp"
#include "../../SRC/Server/GameServer/ecs/CharacterAccessors.hpp"
#include "../../SRC/Server/GameServer/ecs/components/identity_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/social_components.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/AffectSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ChatSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ItemSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/MovementSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SocialSystem.hpp"
#include <Core/Logging.hpp>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

// The production marriage.cpp, driven through a couple's lifecycle: the
// player-id index, logins and logouts, the near-check timer, love points and
// the DB save, the wedding map and the divorce. Characters are bare entities;
// descriptors, the DB link, chat and warps are recording doubles.
entt::registry g_registry;
int passes_per_sec = 25;
int test_server = 0;
bool g_bShutdown = false;
namespace { int dbMarker = 0; }
LPCLIENT_DESC db_clientdesc = reinterpret_cast<LPCLIENT_DESC>(&dbMarker);

namespace {
int checks = 0, liveEvents = 0, cancels = 0;
time_t now = 1'000'000;
bool allowWeddingMap = true, weddingEndSucceeds = true;
std::vector<LPEVENT> created;
std::vector<uint8_t> dbHeaders;
std::vector<TPacketMarriageUpdate> dbUpdates;
std::vector<std::pair<entt::entity, std::string>> chats;
std::vector<std::pair<LPDESC, std::string>> descCommands;
std::vector<std::pair<LPDESC, int>> descPackets;
std::map<entt::entity, entt::entity> partners;
std::vector<std::pair<entt::entity, int32_t>> warps;
std::vector<uint32_t> endedMaps;
std::unordered_map<entt::entity, uint32_t> playerIds;
std::unordered_map<entt::entity, int32_t> mapIndices;
std::unordered_map<uint32_t, entt::entity> online;

void Check(bool value, const char* message)
{
    ++checks;
    if (!value) throw std::runtime_error(message);
}

[[noreturn]] void Unexpected(const char* service)
{
    std::cerr << "Unexpected service in a marriage lifecycle test: " << service << "\n";
    std::abort();
}

LPDESC DescOf(entt::entity e) { return reinterpret_cast<LPDESC>(static_cast<uintptr_t>(0x1000 + entt::to_integral(e))); }
entt::entity OwnerOf(LPDESC desc) { return static_cast<entt::entity>(reinterpret_cast<uintptr_t>(desc) - 0x1000); }
}

CHARACTER_MANAGER character_manager;
P2P_MANAGER p2p_manager;
SECTREE_MANAGER sectree_manager;
marriage::WeddingManager wedding_manager;
marriage::CManager marriages;

// --- recording doubles --------------------------------------------------------
std::shared_ptr<spdlog::logger> logging::GetErrorLogger()
{
    static auto logger = std::make_shared<spdlog::logger>("marriage-error-test");
    return logger;
}
std::shared_ptr<spdlog::logger> logging::GetLogger()
{
    static auto logger = std::make_shared<spdlog::logger>("marriage-test");
    return logger;
}

void intrusive_ptr_add_ref(EVENT* e) { ++e->ref_count; }
void intrusive_ptr_release(EVENT* e) { if (--e->ref_count == 0) { --liveEvents; delete e; } }
LPEVENT event_create_ex(TEVENTFUNC func, event_info_data* info, int32_t)
{
    LPEVENT result(new EVENT);
    ++liveEvents;
    result->func = func;
    result->info = info;
    created.push_back(result);
    return result;
}
void event_cancel(LPEVENT* event)
{
    if (*event) { ++cancels; (*event)->is_force_to_end = true; }
    event->reset();
}
time_t get_global_time() { return now; }
// Core/utils.cpp's, which the love point formula uses.
int MIN(int a, int b) { return a < b ? a : b; }
int MINMAX(int min, int value, int max) { const int tv = min > value ? min : value; return max < tv ? max : tv; }

void CLIENT_DESC::DBPacket(uint8_t header, uint32_t, const void* data, uint32_t size)
{
    dbHeaders.push_back(header);
    if (header == HEADER_GD_MARRIAGE_UPDATE)
    {
        Check(size == sizeof(TPacketMarriageUpdate), "marriage update has the wrong size");
        TPacketMarriageUpdate update;
        std::memcpy(&update, data, sizeof(update));
        dbUpdates.push_back(update);
    }
}
void DESC::Packet(const void*, int size) { descPackets.emplace_back(this, size); }
void DESC::ChatPacket(uint8_t, const char* format, ...) { descCommands.emplace_back(this, format); }
void DESC::SetRelay(const char*) { Unexpected("DESC::SetRelay"); }

CHARACTER_MANAGER::CHARACTER_MANAGER() = default;
CHARACTER_MANAGER::~CHARACTER_MANAGER() = default;
entt::entity CHARACTER_MANAGER::FindEntityByPID(uint32_t pid)
{
    const auto it = online.find(pid);
    return it == online.end() ? entt::null : it->second;
}
P2P_MANAGER::P2P_MANAGER() = default;
P2P_MANAGER::~P2P_MANAGER() = default;
CCI* P2P_MANAGER::FindByPID(uint32_t) { return nullptr; }
SECTREE_MANAGER::SECTREE_MANAGER() = default;
SECTREE_MANAGER::~SECTREE_MANAGER() = default;
bool SECTREE_MANAGER::GetRecallPositionByEmpire(int32_t, uint8_t, PIXEL_POSITION& pos)
{
    pos.x = 100;
    pos.y = 200;
    pos.z = 0;
    return true;
}
marriage::WeddingManager::WeddingManager() = default;
marriage::WeddingManager::~WeddingManager() = default;
bool marriage::WeddingManager::End(uint32_t mapIndex) { endedMaps.push_back(mapIndex); return weddingEndSucceeds; }
bool map_allow_find(int32_t) { return allowWeddingMap; }

bool ecs::PlayerRuntime::IsPC(entt::entity e) { return ecs::IsCharacter(e); }
uint32_t ecs::PlayerRuntime::GetPlayerID(entt::entity e)
{
    const auto it = playerIds.find(e);
    Check(it != playerIds.end(), "player id asked for an unknown entity");
    return it->second;
}
int32_t ecs::PlayerRuntime::GetMapIndex(entt::entity e) { return mapIndices.at(e); }
LPDESC ecs::PlayerRuntime::GetDesc(entt::entity e) { return ecs::IsCharacter(e) ? DescOf(e) : nullptr; }
int ecs::PlayerRuntime::GetPremiumRemainSeconds(entt::entity, uint8_t) { return 0; }
void ecs::SocialSystem::SetMarryPartner(entt::entity e, entt::entity partner) { partners[e] = partner; }
void ecs::ChatSystem::Send(entt::entity e, uint8_t, const char* format, ...) { chats.emplace_back(e, format); }
void ecs::MovementSystem::SaveExitLocation(entt::entity) {}
bool ecs::MovementSystem::WarpSet(entt::entity e, int32_t, int32_t, int32_t mapIndex) { warps.emplace_back(e, mapIndex); return true; }

// No character carries a wedding potion in these tests.
CAffect* AffectSystem::FindAffect(entt::entity, uint32_t, uint8_t) { return nullptr; }

// --- services only other paths use --------------------------------------------
int quest::CQuestManager::GetEventFlag(const std::string&) { Unexpected("CQuestManager::GetEventFlag"); }
bool AffectSystem::RemoveAffect(entt::entity, uint32_t) { Unexpected("AffectSystem::RemoveAffect"); }
entt::entity ItemSystem::FindItemByID(entt::entity, uint32_t) { Unexpected("ItemSystem::FindItemByID"); }
bool ItemSystem::IsValidItem(entt::entity) { Unexpected("ItemSystem::IsValidItem"); }
bool ItemSystem::UnlockItem(entt::entity) { Unexpected("ItemSystem::UnlockItem"); }
bool ItemSystem::SetItemSocket(entt::entity, int, uint32_t, bool) { Unexpected("ItemSystem::SetItemSocket"); }

namespace {
using marriage::MarriageSystem::State;

entt::entity Character(uint32_t pid, int32_t mapIndex = 1)
{
    const auto e = g_registry.create();
    g_registry.emplace<ecs::TagCharacter>(e);
    playerIds[e] = pid;
    mapIndices[e] = mapIndex;
    online[pid] = e;
    CPIDRegistry::Instance().Register(pid, e);
    return e;
}
void Leave(entt::entity e)
{
    online.erase(playerIds.at(e));
    CPIDRegistry::Instance().Unregister(playerIds.at(e));
    g_registry.destroy(e);
}
int Count(const std::vector<std::pair<entt::entity, std::string>>& list, entt::entity e, const char* text)
{
    int n = 0;
    for (const auto& [who, what] : list)
        n += who == e && what == text;
    return n;
}
int DescCount(entt::entity e, const char* text)
{
    int n = 0;
    for (const auto& [desc, what] : descCommands)
        n += desc == DescOf(e) && what == text;
    return n;
}
int32_t Tick(const LPEVENT& timer) { return timer->func(timer, 0); }
void Reset()
{
    for (uint32_t pid : {10u, 20u, 30u, 40u})
    {
        if (marriages.Get(pid) != entt::null)
        {
            const uint32_t other = marriage::MarriageSystem::GetOther(marriages.Get(pid), pid);
            marriages.Remove(pid, other);
        }
    }
    for (const auto& [pid, e] : online)
        CPIDRegistry::Instance().Unregister(pid);
    g_registry.clear();
    created.clear();
    Check(liveEvents == 0, "a near-check timer leaked");
    cancels = 0;
    dbHeaders.clear(); dbUpdates.clear(); chats.clear(); descCommands.clear(); descPackets.clear();
    partners.clear(); warps.clear(); endedMaps.clear();
    playerIds.clear(); mapIndices.clear(); online.clear();
    allowWeddingMap = weddingEndSucceeds = true;
    now = 1'000'000;
}

// The DB hands the couple over by player id; it is its own entity, indexed by
// both ids and ordered low id first, with no character involved.
void AddIndexesByBothPlayers()
{
    Reset();
    marriages.Add(20, 10, now, "second", "first");
    const auto couple = marriages.Get(10);
    Check(couple != entt::null && marriages.Get(20) == couple, "the couple is not indexed by both players");
    Check(State(couple)->pid1 == 10 && State(couple)->pid2 == 20, "the couple is not ordered low id first");
    Check(State(couple)->name1 == "first" && State(couple)->name2 == "second", "the names did not follow the ids");
    Check(marriages.IsEngaged(10) && !marriages.IsMarried(20) && marriages.IsEngagedOrMarried(20), "a new couple is not engaged");
    Check(marriage::MarriageSystem::GetOther(couple, 10) == 20 && marriage::MarriageSystem::GetOther(couple, 30) == 0,
        "GetOther named the wrong partner");
    Check(dbHeaders.empty(), "an offline couple requested a wedding");

    marriages.Add(10, 30, now, "first", "third");
    Check(marriages.Get(30) == entt::null && marriages.Get(10) == couple, "a married player married again");
    Check(!marriages.IsEngagedOrMarried(40) && marriages.Get(40) == entt::null, "a stranger has a couple");
}

void AddWithBothOnlineRequestsTheWedding()
{
    Reset();
    Character(10);
    Character(20);
    marriages.Add(10, 20, now, "first", "second");
    Check(dbHeaders.size() == 1 && dbHeaders[0] == HEADER_GD_WEDDING_REQUEST, "an online couple requested no wedding");
}

// Logins fill the partners in; the pair is online, and the near-check timer
// runs, only while both are characters. Logout clears one side.
void LoginAndLogoutTrackThePartners()
{
    Reset();
    marriages.Add(10, 20, now, "first", "second");
    const auto couple = marriages.Get(10);
    const auto a = Character(10);
    const auto b = Character(20);

    marriages.Login(a);
    Check(State(couple)->character1 == a && !marriage::MarriageSystem::IsOnline(couple), "one login made the pair online");
    Check(created.empty() && partners.empty(), "a lone login paired the partners");
    marriages.Login(b);
    Check(State(couple)->character2 == b && marriage::MarriageSystem::IsOnline(couple), "both logins left the pair offline");
    Check(partners[a] == b && partners[b] == a, "the partners were not linked");
    Check(created.size() == 1 && State(couple)->nearCheckEvent == created[0], "the near check did not start");
    Check(descCommands.empty(), "an engaged couple announced a lover login");

    marriages.Logout(a);
    Check(State(couple)->character1 == entt::null && ecs::IsCharacter(State(couple)->character2), "logout cleared the wrong side");
    Check(partners[b] == entt::null && cancels == 1 && !State(couple)->nearCheckEvent, "logout left the pair linked");
    Check(dbUpdates.empty(), "an unchanged couple was saved");
}

// Marriage is saved once through the DB, and a married login announces the
// lover to both descriptors.
void MarriedCoupleAnnouncesAndSaves()
{
    Reset();
    marriages.Add(10, 20, now, "first", "second");
    const auto couple = marriages.Get(10);
    marriage::MarriageSystem::SetMarried(couple);
    Check(marriages.IsMarried(10) && !marriages.IsEngaged(20), "SetMarried did not marry");
    Check(dbUpdates.size() == 1 && dbUpdates[0].byMarried == 1 && dbUpdates[0].dwPID1 == 10 && dbUpdates[0].dwPID2 == 20,
        "SetMarried did not save the marriage");
    marriage::MarriageSystem::Save(couple);
    Check(dbUpdates.size() == 1, "a clean couple was saved again");

    const auto a = Character(10);
    const auto b = Character(20);
    marriages.Login(a);
    marriages.Login(b);
    Check(DescCount(a, "lover_login") == 1 && DescCount(b, "lover_login") == 1, "the lovers were not announced");

    marriages.Logout(b);
    Check(DescCount(a, "lover_logout") == 1, "the remaining lover heard no logout");
}

// Love points only grow while both are online and married; they are saved on
// the next logout.
void LovePointsNeedTheCouple()
{
    Reset();
    marriages.Add(10, 20, now, "first", "second");
    const auto couple = marriages.Get(10);
    marriages.Update(10, 20, 0, 1);
    marriage::MarriageSystem::Update(couple, 500);
    Check(State(couple)->lovePoint == 0, "an offline couple gained love points");

    const auto a = Character(10);
    const auto b = Character(20);
    marriages.Login(a);
    marriages.Login(b);
    marriage::MarriageSystem::Update(couple, 500);
    Check(State(couple)->lovePoint == 500 && State(couple)->needsSave, "an online couple gained nothing");
    marriage::MarriageSystem::Update(couple, 0);
    Check(State(couple)->lovePoint == 500, "a zero update changed the points");

    marriages.Update(10, 30, 9, 1);
    Check(State(couple)->lovePoint == 500, "an update for the wrong pair was applied");

    marriages.Logout(a);
    Check(dbUpdates.size() == 1 && dbUpdates[0].iLovePoint == 500, "the points were not saved on logout");
}

// The near-check timer holds the couple entity: it reports nearness while the
// pair is online, stops itself when one leaves, and ends for a gone couple.
void NearCheckFollowsTheCouple()
{
    Reset();
    marriages.Add(10, 20, now, "first", "second");
    const auto couple = marriages.Get(10);
    marriages.Update(10, 20, 0, 1);
    const auto a = Character(10, 41);
    const auto b = Character(20, 41);
    marriages.Login(a);
    marriages.Login(b);
    const LPEVENT timer = created.at(0);
    descPackets.clear(); // the married logins sent lover info

    Check(marriage::MarriageSystem::IsNear(couple), "a couple on one map is not near");
    Check(Tick(timer) > 0 && Count(chats, a, "lover_near") == 1 && Count(chats, b, "lover_near") == 1, "nearness was not reported");
    Check(!descPackets.empty(), "the love point was not published");
    mapIndices[b] = 42;
    Check(Tick(timer) > 0 && Count(chats, a, "lover_far") == 1, "distance was not reported");

    Leave(b);
    Check(Tick(timer) > 0 && !State(couple)->nearCheckEvent && cancels == 1, "the timer outlived the pair");

    g_registry.destroy(couple);
    Check(Tick(timer) == 0, "the timer outlived its couple");
    Check(marriages.Get(10) == entt::null && marriages.Get(20) == entt::null, "the index kept a destroyed couple");
    Check(!marriages.IsEngagedOrMarried(10), "a destroyed couple still engages");
    marriages.Add(10, 20, now, "first", "second");
    Check(marriages.Get(10) != entt::null && marriages.Get(10) != couple, "a destroyed couple blocked a new one");
}

// WeddingReady stores the map, WeddingStart warps both and lists the wedding,
// WeddingEnd ends the map and forgets it.
void WeddingMapLifecycle()
{
    Reset();
    marriages.Add(10, 20, now, "first", "second");
    const auto couple = marriages.Get(10);
    const auto a = Character(10);
    const auto b = Character(20);

    marriages.WeddingStart(10, 20);
    Check(warps.empty(), "a wedding started without a map");
    marriages.WeddingReady(10, 20, 810001);
    Check(State(couple)->weddingMapIndex && *State(couple)->weddingMapIndex == 810001, "the wedding map was not stored");
    int listed = 0;
    marriages.for_each_wedding([&](entt::entity) { ++listed; });
    Check(listed == 0, "a ready wedding was listed before it started");

    marriages.WeddingStart(10, 20);
    Check(warps.size() == 2 && warps[0] == std::make_pair(a, 810001) && warps[1] == std::make_pair(b, 810001),
        "the couple was not warped to the wedding");
    marriages.for_each_wedding([&](entt::entity c) { listed += c == couple; });
    Check(listed == 1, "a started wedding was not listed");

    marriage::MarriageSystem::RequestEndWedding(couple);
    Check(!dbHeaders.empty() && dbHeaders.back() == HEADER_GD_WEDDING_END, "the end was not requested");

    weddingEndSucceeds = false;
    marriages.WeddingEnd(10, 20);
    Check(State(couple)->weddingMapIndex.has_value(), "a failed map end forgot the wedding");
    weddingEndSucceeds = true;
    marriages.WeddingEnd(10, 20);
    Check(endedMaps.size() == 2 && endedMaps[1] == 810001, "the wedding map was not ended");
    Check(!State(couple)->weddingMapIndex, "the wedding map was kept");
    listed = 0;
    marriages.for_each_wedding([&](entt::entity) { ++listed; });
    Check(listed == 0, "an ended wedding was still listed");
}

// Divorce: the index forgets both players, the timer stops, both online
// partners hear it, and the couple entity is gone.
void RemoveRetiresTheCouple()
{
    Reset();
    marriages.Add(10, 20, now, "first", "second");
    const auto couple = marriages.Get(10);
    const auto a = Character(10);
    const auto b = Character(20);
    marriages.Login(a);
    marriages.Login(b);

    marriages.Remove(10, 30);
    Check(marriages.Get(10) == couple, "a divorce from the wrong partner was applied");
    marriages.Remove(10, 20);
    Check(!g_registry.valid(couple) && marriages.Get(10) == entt::null && marriages.Get(20) == entt::null,
        "the divorce kept the couple");
    Check(cancels == 1 && created.at(0)->is_force_to_end, "the divorce left the timer running");
    Check(Count(chats, a, "lover_divorce") == 1 && Count(chats, b, "lover_divorce") == 1, "the partners did not hear the divorce");
}
}

int main()
{
    try {
        AddIndexesByBothPlayers();
        AddWithBothOnlineRequestsTheWedding();
        LoginAndLogoutTrackThePartners();
        MarriedCoupleAnnouncesAndSaves();
        LovePointsNeedTheCouple();
        NearCheckFollowsTheCouple();
        WeddingMapLifecycle();
        RemoveRetiresTheCouple();
        Reset();
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << "\n";
        return 1;
    }
    std::cout << "marriage lifecycle: " << checks << " checks passed\n";
    return 0;
}
