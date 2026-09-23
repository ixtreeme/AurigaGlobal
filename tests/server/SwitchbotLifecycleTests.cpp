#include "../../SRC/Server/GameServer/core/stdafx.h"
#include "../../SRC/Server/GameServer/social/new_switchbot.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/components/inventory_components.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ItemSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PointSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ChatSystem.hpp"
#include "../../SRC/Server/GameServer/entity/char_manager.h"
#include "../../SRC/Server/GameServer/network/desc.h"
#include "../../SRC/Server/GameServer/network/p2p.h"
#include "../../SRC/Server/GameServer/core/buffer_manager.h"
#include "../../SRC/Server/GameServer/core/constants.h"
#include "../../SRC/Server/GameServer/core/packet.h"
#include "../../SRC/Server/GameServer/combat/battle_pass.h"
#include "../../SRC/Server/GameServer/world/event.h"
#include <Core/Logging.hpp>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

// The production new_switchbot.cpp, driven through its job lifecycle: the
// player-id index, the warp freeze, the timer, the P2P hand-off and teardown.
// Rerolls, whispers and client packets belong to the item-attribute tests;
// every service on those paths aborts here if the lifecycle ever reaches it.
entt::registry g_registry;
int passes_per_sec = 25;
TItemAttrMap g_map_itemAttr;

namespace {
int checks = 0, liveEvents = 0, cancels = 0, updateLookups = 0;
std::vector<LPEVENT> created;
std::vector<TPacketGGSwitchbot> handedOff;
std::unordered_map<entt::entity, uint32_t> playerIds;

void Check(bool value, const char* message)
{
    ++checks;
    if (!value) throw std::runtime_error(message);
}

[[noreturn]] void Unexpected(const char* service)
{
    std::cerr << "Unexpected service in a switchbot lifecycle test: " << service << "\n";
    std::abort();
}
}

CHARACTER_MANAGER character_manager;
P2P_MANAGER p2p_manager;
CSwitchbotManager switchbots;

// --- the services the lifecycle does use ------------------------------------
std::shared_ptr<spdlog::logger> logging::GetErrorLogger()
{
    static auto logger = std::make_shared<spdlog::logger>("switchbot-error-test");
    return logger;
}
std::shared_ptr<spdlog::logger> logging::GetLogger()
{
    static auto logger = std::make_shared<spdlog::logger>("switchbot-test");
    return logger;
}

// The scheduler is a double: the tests run the timer callback themselves.
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

CHARACTER_MANAGER::CHARACTER_MANAGER() = default;
CHARACTER_MANAGER::~CHARACTER_MANAGER() = default;
// No character is online in these tests, so every client update stops at the
// lookup; the count proves when the switchbot tried to send one.
entt::entity CHARACTER_MANAGER::FindEntityByPID(uint32_t) { ++updateLookups; return entt::null; }

P2P_MANAGER::P2P_MANAGER() = default;
P2P_MANAGER::~P2P_MANAGER() = default;
void P2P_MANAGER::Send(const void* data, int size, LPDESC)
{
    Check(size == static_cast<int>(sizeof(TPacketGGSwitchbot)), "unexpected P2P packet size");
    TPacketGGSwitchbot pack;
    std::memcpy(&pack, data, sizeof(pack));
    handedOff.push_back(pack);
}

bool ecs::PlayerRuntime::IsValid(entt::entity e) { return e != entt::null && g_registry.valid(e); }
uint32_t ecs::PlayerRuntime::GetPlayerID(entt::entity e)
{
    const auto it = playerIds.find(e);
    Check(it != playerIds.end(), "player id asked for an unknown entity");
    return it->second;
}
LPDESC ecs::PlayerRuntime::GetDesc(entt::entity) { return nullptr; }

// --- services only the reroll / notice / client paths use ---------------------
bool ecs::PlayerRuntime::IsPC(entt::entity) { Unexpected("PlayerRuntime::IsPC"); }
std::string_view ecs::PlayerRuntime::GetName(entt::entity) { Unexpected("PlayerRuntime::GetName"); }
#ifdef ENABLE_BATTLE_PASS
uint8_t ecs::PlayerRuntime::GetBattlePassId(entt::entity) { Unexpected("GetBattlePassId"); }
uint32_t ecs::PlayerRuntime::GetMissionProgress(entt::entity, uint32_t, uint32_t) { Unexpected("GetMissionProgress"); }
bool ecs::PlayerRuntime::UpdateMissionProgress(entt::entity, uint32_t, uint32_t, uint32_t, uint32_t, bool) { Unexpected("UpdateMissionProgress"); }
bool CBattlePass::BattlePassMissionGetInfo(uint8_t, uint8_t, uint32_t*, uint32_t*) { Unexpected("BattlePassMissionGetInfo"); }
#endif
#ifdef ENABLE_RANKING
int64_t ecs::PlayerRuntime::GetRankPoints(entt::entity, int) { Unexpected("GetRankPoints"); }
bool ecs::PlayerRuntime::SetRankPoints(entt::entity, int, int64_t) { Unexpected("SetRankPoints"); }
#endif
#ifdef TEXTS_IMPROVEMENT
void ecs::ChatSystem::SendNew(entt::entity, uint8_t, uint32_t, const char*, ...) { Unexpected("ChatSystem::SendNew"); }
#endif
void DESC::BufferedPacket(const void*, int) { Unexpected("DESC::BufferedPacket"); }
void DESC::Packet(const void*, int) { Unexpected("DESC::Packet"); }
void BroadcastNotice(const char*, bool) { Unexpected("BroadcastNotice"); }
TEMP_BUFFER::TEMP_BUFFER(int, bool) { Unexpected("TEMP_BUFFER"); }
TEMP_BUFFER::~TEMP_BUFFER() = default;
const void* TEMP_BUFFER::read_peek() { Unexpected("TEMP_BUFFER::read_peek"); }
void TEMP_BUFFER::write(const void*, int) { Unexpected("TEMP_BUFFER::write"); }
int TEMP_BUFFER::size() { Unexpected("TEMP_BUFFER::size"); }
entt::entity ItemSystem::FindItemByID(uint32_t) { Unexpected("ItemSystem::FindItemByID"); }
entt::entity ItemSystem::GetItem(entt::entity, TItemPos) { Unexpected("ItemSystem::GetItem"); }
bool ItemSystem::IsValidItem(entt::entity) { Unexpected("ItemSystem::IsValidItem"); }
uint32_t ItemSystem::GetItemID(entt::entity) { Unexpected("ItemSystem::GetItemID"); }
uint32_t ItemSystem::GetItemVnum(entt::entity) { Unexpected("ItemSystem::GetItemVnum"); }
uint8_t ItemSystem::GetItemType(entt::entity) { Unexpected("ItemSystem::GetItemType"); }
uint8_t ItemSystem::GetItemSubType(entt::entity) { Unexpected("ItemSystem::GetItemSubType"); }
uint32_t ItemSystem::GetItemCount(entt::entity) { Unexpected("ItemSystem::GetItemCount"); }
const char* ItemSystem::GetItemName(entt::entity) { Unexpected("ItemSystem::GetItemName"); }
const TItemTable* ItemSystem::GetItemProto(entt::entity) { Unexpected("ItemSystem::GetItemProto"); }
entt::entity ItemSystem::GetItemOwner(entt::entity) { Unexpected("ItemSystem::GetItemOwner"); }
entt::entity ItemSystem::GetItemOwnerEntity(entt::entity) { Unexpected("ItemSystem::GetItemOwnerEntity"); }
uint32_t ItemSystem::GetItemSocket(entt::entity, int) { Unexpected("ItemSystem::GetItemSocket"); }
TPlayerItemAttribute ItemSystem::GetItemAttribute(entt::entity, int) { Unexpected("ItemSystem::GetItemAttribute"); }
int ItemSystem::GetItemAttributeType(entt::entity, int) { Unexpected("ItemSystem::GetItemAttributeType"); }
int ItemSystem::GetItemAttributeValue(entt::entity, int) { Unexpected("ItemSystem::GetItemAttributeValue"); }
int ItemSystem::GetItemAttributeCount(entt::entity) { Unexpected("ItemSystem::GetItemAttributeCount"); }
int ItemSystem::GetItemAttributeSetIndex(entt::entity) { Unexpected("ItemSystem::GetItemAttributeSetIndex"); }
bool ItemSystem::CanPayItemAttributeCost(entt::entity, entt::entity, uint32_t) { Unexpected("ItemSystem::CanPayItemAttributeCost"); }
bool ItemSystem::ChangeItemAttributeWithItemCost(entt::entity, entt::entity, uint32_t, const int*) { Unexpected("ItemSystem::ChangeItemAttributeWithItemCost"); }
bool ItemSystem::IsItemExchanging(entt::entity) { Unexpected("ItemSystem::IsItemExchanging"); }
bool ItemSystem::IsItemLocked(entt::entity) { Unexpected("ItemSystem::IsItemLocked"); }
uint8_t ItemSystem::GetItemWindow(entt::entity) { Unexpected("ItemSystem::GetItemWindow"); }
uint16_t ItemSystem::GetItemCell(entt::entity) { Unexpected("ItemSystem::GetItemCell"); }
bool ItemSystem::IsItemEquipped(entt::entity) { Unexpected("ItemSystem::IsItemEquipped"); }

namespace {
entt::entity Job(uint32_t pid) { return CSwitchbotManager::Instance().FindSwitchbot(pid); }
ecs::SwitchbotState& State(uint32_t pid)
{
    const auto job = Job(pid);
    Check(job != entt::null && g_registry.all_of<ecs::SwitchbotState>(job), "no switchbot job for the player");
    return g_registry.get<ecs::SwitchbotState>(job);
}
std::vector<TSwitchbotAttributeAlternativeTable> Wanted(uint8_t type, int16_t value)
{
    std::vector<TSwitchbotAttributeAlternativeTable> alternatives(SWITCHBOT_ALTERNATIVE_COUNT);
    for (auto& alternative : alternatives)
        std::memset(&alternative, 0, sizeof(alternative));
    alternatives[0].attributes[0].bType = type;
    alternatives[0].attributes[0].sValue = value;
    return alternatives;
}
int32_t Tick(const LPEVENT& timer) { return timer->func(timer, 0); }
void Reset()
{
    switchbots.Initialize();
    g_registry.clear();
    created.clear();
    handedOff.clear();
    playerIds.clear();
    Check(liveEvents == 0, "a switchbot timer leaked");
    cancels = updateLookups = 0;
}

// The job is created by the first registration, with no character at all: it
// is its own entity, found through the player-id index.
void RegisterCreatesAJobWithoutACharacter()
{
    Reset();
    switchbots.RegisterItem(42, 1001, 0);
    const auto job = Job(42);
    Check(job != entt::null && g_registry.valid(job), "registration made no job");
    Check(g_registry.storage<ecs::SwitchbotState>().size() == 1 && playerIds.empty(), "the job is not a lone entity");
    Check(State(42).table.player_id == 42 && State(42).table.items[0] == 1001, "registration lost the item");
    Check(!State(42).warping && !State(42).switchEvent, "a new job starts frozen or running");
    Check(updateLookups == 1, "registration sent no client update");

    switchbots.RegisterItem(42, 1002, 1);
    Check(Job(42) == job && State(42).table.items[1] == 1002, "a second registration made a second job");
    switchbots.RegisterItem(42, 1003, SWITCHBOT_SLOT_COUNT);
    Check(State(42).table.items[0] == 1001 && State(42).table.items[1] == 1002, "an out-of-range cell was written");

    switchbots.UnregisterItem(42, 1);
    Check(State(42).table.items[1] == 0, "unregistration kept the item");
    Check(Job(7) == entt::null && !switchbots.IsActive(7, 0) && !switchbots.IsWarping(7), "an unknown player has a job");
}

// While a warp is in flight the character's items are destroyed and loaded
// again; the job must ignore both so the slots survive the map change.
void WarpFreezesRegistration()
{
    Reset();
    switchbots.RegisterItem(42, 1001, 0);
    switchbots.SetIsWarping(42, true);
    Check(switchbots.IsWarping(42), "the warp flag was not stored");
    const int lookups = updateLookups;
    switchbots.UnregisterItem(42, 0);
    switchbots.RegisterItem(42, 2002, 1);
    Check(State(42).table.items[0] == 1001 && State(42).table.items[1] == 0, "a frozen job accepted item changes");
    Check(updateLookups == lookups, "a frozen job sent client updates");
}

// Start arms one timer and Stop cancels it; the slot state follows.
void StartAndStopDriveTheTimer()
{
    Reset();
    switchbots.RegisterItem(42, 1001, 0);
    switchbots.Start(42, 0, Wanted(APPLY_MAX_HP, 500));
    Check(switchbots.IsActive(42, 0) && created.size() == 1 && State(42).switchEvent == created[0], "Start armed no timer");
    Check(State(42).table.alternatives[0][0].attributes[0].bType == APPLY_MAX_HP &&
        State(42).table.alternatives[0][0].attributes[0].sValue == 500, "Start lost the wanted bonus");
    switchbots.RegisterItem(42, 1002, 1);
    switchbots.Start(42, 1, Wanted(APPLY_MAX_SP, 300));
    Check(created.size() == 1, "a second slot armed a second timer");
    switchbots.Start(42, 1, Wanted(APPLY_MAX_SP, 300));
    Check(created.size() == 1, "a running slot was started again");

    switchbots.Stop(42, 1);
    Check(!switchbots.IsActive(42, 1) && State(42).switchEvent, "stopping one slot stopped the job");
    switchbots.Stop(42, 0);
    Check(!switchbots.IsActive(42, 0) && !State(42).switchEvent && cancels == 1, "the last slot left the timer running");
    Check(created[0]->is_force_to_end, "the timer was not cancelled");
}

// The timer holds the job's entity: a frozen job is skipped, and a job that
// is gone ends the timer instead of being read through a stale handle.
void TimerFollowsTheEntity()
{
    Reset();
    switchbots.RegisterItem(42, 1001, 0);
    switchbots.Start(42, 0, Wanted(APPLY_MAX_HP, 500));
    const LPEVENT timer = created.at(0);
    switchbots.SetIsWarping(42, true);
    Check(Tick(timer) > 0 && switchbots.IsActive(42, 0), "a frozen job did not wait for the warp");

    g_registry.destroy(Job(42));
    Check(Tick(timer) == 0, "the timer outlived its job");
    Check(Job(42) == entt::null, "the index kept a destroyed job");
}

// A warp to another core cancels the timer, sends the whole table and
// retires the job here; the receiving core rebuilds it from the packet.
void P2PHandOff()
{
    Reset();
    switchbots.RegisterItem(42, 1001, 0);
    switchbots.Start(42, 0, Wanted(APPLY_MAX_HP, 500));
    const auto job = Job(42);
    switchbots.SetIsWarping(42, true);
    switchbots.P2PSendSwitchbot(42, 13001);
    Check(cancels == 1 && created[0]->is_force_to_end, "the hand-off left the timer running");
    Check(!g_registry.valid(job) && Job(42) == entt::null, "the hand-off kept the job here");
    Check(handedOff.size() == 1 && handedOff[0].wPort == 13001 && handedOff[0].bHeader == HEADER_GG_SWITCHBOT,
        "the hand-off packet is wrong");
    Check(handedOff[0].table.player_id == 42 && handedOff[0].table.items[0] == 1001 && handedOff[0].table.active[0] &&
        handedOff[0].table.alternatives[0][0].attributes[0].sValue == 500, "the hand-off lost the table");
    switchbots.P2PSendSwitchbot(42, 13001);
    Check(handedOff.size() == 1, "a missing job was handed off");

    switchbots.P2PReceiveSwitchbot(handedOff[0].table);
    const auto received = Job(42);
    Check(received != entt::null && State(42).table.items[0] == 1001 && State(42).table.active[0], "the receiver lost the table");
    Check(!State(42).warping && !State(42).switchEvent, "the receiver started the job before the player arrived");

    TSwitchbotTable newer = handedOff[0].table;
    newer.items[0] = 3003;
    switchbots.P2PReceiveSwitchbot(newer);
    Check(Job(42) == received && State(42).table.items[0] == 3003, "a second hand-off made a second job");
}

// Entering the game clears the warp and restarts a job with running slots.
void EnterGameResumes()
{
    Reset();
    const auto character = g_registry.create();
    playerIds.emplace(character, 42);
    switchbots.RegisterItem(42, 1001, 0);
    switchbots.Start(42, 0, Wanted(APPLY_MAX_HP, 500));
    switchbots.SetIsWarping(42, true);
    switchbots.P2PSendSwitchbot(42, 13001);
    switchbots.P2PReceiveSwitchbot(handedOff.at(0).table);
    switchbots.SetIsWarping(42, true);

    switchbots.EnterGame(character);
    Check(!switchbots.IsWarping(42), "entering the game kept the warp flag");
    Check(created.size() == 2 && State(42).switchEvent == created[1], "entering the game did not restart the job");
    switchbots.EnterGame(character);
    Check(created.size() == 2, "a running job was restarted");

    const auto idle = g_registry.create();
    playerIds.emplace(idle, 43);
    switchbots.RegisterItem(43, 1004, 0);
    switchbots.EnterGame(idle);
    Check(created.size() == 2, "a job without running slots was started");
}

// An index entry whose job was destroyed behind the manager reads as no job,
// and the next registration starts a fresh one.
void StaleEntryIsReplaced()
{
    Reset();
    switchbots.RegisterItem(42, 1001, 0);
    const auto first = Job(42);
    g_registry.destroy(first);
    Check(Job(42) == entt::null && !switchbots.IsActive(42, 0), "a destroyed job was still found");
    switchbots.RegisterItem(42, 2002, 1);
    Check(Job(42) != entt::null && Job(42) != first && State(42).table.items[1] == 2002 && State(42).table.items[0] == 0,
        "registration wrote through the destroyed job");
}

// Initialize (the manager's constructor and destructor) retires every job and
// cancels any running timer, as deleting the CSwitchbot objects did.
void InitializeRetiresEveryJob()
{
    Reset();
    switchbots.RegisterItem(42, 1001, 0);
    switchbots.RegisterItem(43, 1002, 0);
    switchbots.Start(42, 0, Wanted(APPLY_MAX_HP, 500));
    const auto a = Job(42), b = Job(43);
    switchbots.Initialize();
    Check(!g_registry.valid(a) && !g_registry.valid(b), "Initialize kept a job");
    Check(cancels == 1 && created[0]->is_force_to_end, "Initialize left a timer running");
    Check(Job(42) == entt::null && Job(43) == entt::null, "Initialize kept an index entry");
}
}

int main()
{
    try {
        RegisterCreatesAJobWithoutACharacter();
        WarpFreezesRegistration();
        StartAndStopDriveTheTimer();
        TimerFollowsTheEntity();
        P2PHandOff();
        EnterGameResumes();
        StaleEntryIsReplaced();
        InitializeRetiresEveryJob();
        Reset();
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << "\n";
        return 1;
    }
    std::cout << "switchbot lifecycle: " << checks << " checks passed\n";
    return 0;
}
