// Exercise the real grant completion against logout/relogin. The SQL boundary
// is controlled here; the planner, ECS login fence and completion are real.
#include "../../SRC/Server/GameServer/core/stdafx.h"
#include "../../SRC/Server/GameServer/entity/char_manager.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/InventorySystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PointSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/CombatSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/NetworkSyncSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SessionSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SocialSystem.hpp"
#include "../../SRC/Server/GameServer/combat/pvp.h"
#include "../../SRC/Server/GameServer/quest/questmanager.h"
#include "../../SRC/Server/GameServer/world/arena.h"
#include "../../SRC/Server/GameServer/world/war_map.h"
#include "../../SRC/Server/GameServer/world/sectree_manager.h"
#include "../../SRC/Server/GameServer/item/ItemCatalogRuntime.hpp"
#include "../../SRC/Server/GameServer/item/ItemStore.hpp"
#include "../../SRC/Server/GameServer/item/TypedItemOps.hpp"
#include "PersistentItemFixture.hpp"
#include <Core/Logging.hpp>
#include <iostream>
#include <stdexcept>

entt::registry g_registry;
namespace items = auriga::item;
namespace runtime = items::runtime;
namespace f = persistent_item_fixture;

namespace
{
unsigned checks = 0, syncs = 0, pumps = 0;
entt::entity player = entt::null;
std::shared_ptr<const items::ItemCatalog> catalog;
std::function<void(runtime::CommitOutcome)> answer;
items::ItemCommit submitted;
void Check(bool good, const char* message)
{
    ++checks;
    if (!good) throw std::runtime_error(message);
}
[[noreturn]] void Unexpected() { throw std::runtime_error("grant called an unrelated item service"); }
void Complete(bool written)
{
    runtime::CommitOutcome outcome { std::unexpected(items::ItemPersistenceError::SqlFailure),
                                    std::unexpected(items::ItemPersistenceError::SqlFailure) };
    if (written) outcome.write = items::persistence::ItemWriteResult {};
    auto completion = std::move(answer);
    completion(std::move(outcome));
}
void Start()
{
    player = g_registry.create();
    items::persistence::LoadedItemOwner state { { f::Character(), 7, 1 }, { f::Inventory() }, {}, {} };
    g_registry.emplace<ecs::TypedItems>(player, std::move(state));
    g_registry.emplace<ecs::TypedItemLogin>(player, std::uint64_t { 1 });
}
void Logout()
{
    g_registry.destroy(player);
    player = entt::null;
}
}

CHARACTER_MANAGER::CHARACTER_MANAGER() = default;
CHARACTER_MANAGER::~CHARACTER_MANAGER() = default;
entt::entity CHARACTER_MANAGER::FindEntityByPID(uint32_t pid) { return pid == 42 ? player : entt::null; }
uint32_t ecs::PlayerRuntime::GetPlayerID(entt::entity) { return 42; }
void ecs::ChatSystem::Send(entt::entity, uint8_t, const char*, ...) {}
void SendTypedInventory(entt::entity) { ++syncs; }
void PumpTypedItemGrants(entt::entity) { ++pumps; }
// Other TypedItemOps entry points are linked into this focused executable too;
// they must never participate in the grant's admission or completion.
int64_t get_global_time() { Unexpected(); }
const char* one_argument(const char*, char*, size_t) { Unexpected(); }
const char* two_arguments(const char*, char*, size_t, char*, size_t) { Unexpected(); }
void SECTREE::VisitAround(void (*)(void*, entt::entity), void*) const { Unexpected(); }
LPSECTREE SECTREE_MANAGER::Get(int32_t, int32_t, int32_t) { Unexpected(); }
uint8_t ecs::PlayerRuntime::GetGMLevel(entt::entity) { Unexpected(); }
uint32_t ecs::PlayerRuntime::GetRaceNum(entt::entity) { Unexpected(); }
uint8_t ecs::PlayerRuntime::GetSex(entt::entity) { Unexpected(); }
uint8_t ecs::PlayerRuntime::GetJob(entt::entity) { Unexpected(); }
uint64_t ecs::PlayerRuntime::GetHP(entt::entity) { Unexpected(); }
uint64_t ecs::PlayerRuntime::GetSP(entt::entity) { Unexpected(); }
int32_t ecs::PlayerRuntime::GetMapIndex(entt::entity) { Unexpected(); }
int32_t ecs::PlayerRuntime::GetX(entt::entity) { Unexpected(); }
int32_t ecs::PlayerRuntime::GetY(entt::entity) { Unexpected(); }
bool ecs::PlayerRuntime::IsValid(entt::entity) { Unexpected(); }
bool ecs::PlayerRuntime::IsPC(entt::entity) { Unexpected(); }
void ecs::PlayerRuntime::SetPotionLimit(entt::entity, int) { Unexpected(); }
int ecs::PlayerRuntime::GetPotionLimit(entt::entity) { Unexpected(); }
int ecs::PlayerRuntime::GetPosition(entt::entity) { Unexpected(); }
bool InventorySystem::CanHandleItems(entt::entity, bool, bool) { Unexpected(); }
int64_t ecs::PointSystem::Get(entt::entity, uint8_t) { Unexpected(); }
uint64_t ecs::PointSystem::GetMaxHP(entt::entity) { Unexpected(); }
uint64_t ecs::PointSystem::GetMaxSP(entt::entity) { Unexpected(); }
int ecs::PointSystem::GetLevel(entt::entity) { Unexpected(); }
void ecs::PointSystem::Change(entt::entity, uint8_t, int64_t, bool, bool, bool) { Unexpected(); }
bool CombatSystem::IsDead(entt::entity) { Unexpected(); }
void NetworkSyncSystem::BroadcastEffect(entt::registry&, entt::entity, uint8_t) { Unexpected(); }
bool ecs::SessionSystem::IsSafeboxOpen(entt::entity) { Unexpected(); }
bool ecs::SessionSystem::IsCubeOpen(entt::entity) { Unexpected(); }
bool ecs::SocialSystem::HasExchange(entt::entity) { Unexpected(); }
entt::entity ecs::SocialSystem::GetMyShop(entt::entity) { Unexpected(); }
entt::entity ecs::SocialSystem::GetShopOwner(entt::entity) { Unexpected(); }
entt::entity CHARACTER_MANAGER::FindEntity(uint32_t) { Unexpected(); }
bool CPVPManager::IsFighting(uint32_t) { Unexpected(); }
int64_t quest::CQuestManager::GetEventFlag(const std::string&) { Unexpected(); }
bool CArenaManager::IsArenaMap(uint32_t) { Unexpected(); }
bool CWarMapManager::IsWarMap(int32_t) { Unexpected(); }
std::shared_ptr<spdlog::logger> logging::GetLogger()
{
    static auto logger = std::make_shared<spdlog::logger>("typed-grant-test"); return logger;
}
std::shared_ptr<spdlog::logger> logging::GetErrorLogger() { return logging::GetLogger(); }
namespace auriga::item::runtime
{
std::shared_ptr<const ItemCatalog> CurrentCatalog() noexcept { return catalog; }
std::vector<ItemInstanceId> PeekInstanceIds(std::size_t) { return { ItemInstanceId::Create(500).value() }; }
void SpendInstanceIds(std::size_t) {}
bool RequestCommit(ItemOwner, std::uint64_t, ItemCommit commit, std::function<void(CommitOutcome)> done)
{
    Check(!answer && ValidateItemCommit(*catalog, commit).has_value(), "grant must be valid and serialized");
    submitted = std::move(commit); answer = std::move(done); return true;
}
}

int main()
{
    try
    {
        CHARACTER_MANAGER characters;
        catalog = items::BuildItemCatalog(f::Catalog()).value();
        const items::ItemGrant grant { item_catalog_fixture::Leather(), 0, 5 };
        for (const bool written : { true, false })
        {
            Start(); std::optional<bool> got;
            Check(RequestTypedItemGrant(player, grant, [&](bool result) { got = result; }).has_value(), "grant queued");
            Check(!got && g_registry.all_of<ecs::TypedItemBusy>(player), "no ground result before persistence");
            Logout(); Complete(written);
            Check(got && *got == written && syncs == 0 && pumps == 0,
                  "logout must preserve the actual persistence result without touching a new login");
        }
        Start(); std::optional<bool> got;
        Check(RequestTypedItemGrant(player, grant, [&](bool result) { got = result; }).has_value(), "relogin grant queued");
        Logout(); Start();
        g_registry.get<ecs::TypedItemLogin>(player).token = 2;
        Complete(true);
        Check(got && *got && g_registry.get<ecs::TypedItemLogin>(player).token == 2 &&
              !g_registry.all_of<ecs::TypedItemBusy>(player) && syncs == 0 && pumps == 0,
              "a successful old-login grant removes ground loot without altering the new login");
        Logout();
        std::cout << "Typed item grant checks passed: " << checks << '\n';
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
