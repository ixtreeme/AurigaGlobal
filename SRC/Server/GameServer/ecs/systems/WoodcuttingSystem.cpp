#include "../../core/stdafx.h"
#include "WoodcuttingSystem.hpp"
#include "HerbalismSystem.hpp"
#include "WoodcuttingRules.hpp"
#include "AffectSystem.hpp"
#include "ActivitySystem.hpp"
#include "ChatSystem.hpp"
#include "CombatSystem.hpp"
#include "InventorySystem.hpp"
#include "ItemSystem.hpp"
#include "MountSystem.hpp"
#include "MovementSystem.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "../Registry.hpp"
#include "../NetworkService.hpp"
#include "../components/identity_components.hpp"
#include "../components/movement_components.hpp"
#include "../components/transform_components.hpp"
#include "../services/VisibilityService.hpp"
#include "../../command/gm.h"
#include "../../core/config.h"
#include "../../core/JsonFile.h"
#include "../../core/locale_service.h"
#include "../../core/utils.h"
#include "../../network/db.h"
#include "../../entity/char_manager.h"
#include "../../entity/mob_manager.h"
#include "../../item/item_manager.h"
#include "../../world/sectree_manager.h"
#include "../../command/cmd.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <sstream>
#include <vector>

extern bool g_bShutdown;

namespace WoodcuttingSystem {
namespace {
struct Settings {
    bool enabled = false;
    int population = 12, hits = 20, hitSeconds = 2, fallenSeconds = 180;
    int respawnSeconds = 300, logChance = 30, pointChance = 20;
    int refineChance = 80;
} settings;
bool loaded = false;
uint32_t nextSpawn = 0;

void Notice(entt::entity character, const char* key) {
    ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "woodcut_notice %s", key);
}

void LoadSettings() {
    loaded = true;
    settings = Settings{};
    const auto path = LocaleService_GetBasePath() + "/woodcutting.json";
    std::error_code configError;
    if (!std::filesystem::exists(path, configError)) return; // absent config is fail-closed
    nlohmann::json doc;
    if (!JsonFile::Load(path, doc)) return;
    try {
        if (doc.at("schema_version").get<int>() != 1 ||
            doc.at("map").get<std::string>() != "metin2_map_main_razor93")
            throw std::runtime_error("unsupported schema or map");
        Settings candidate;
        candidate.enabled = doc.at("enabled").get<bool>();
        auto read = [&](const char* name, int fallback, int low, int high) {
            int value = doc.value(name, fallback);
            if (value < low || value > high) throw std::runtime_error(name);
            return value;
        };
        candidate.population = read("population", 12, 0, 100);
        candidate.hits = read("hits_per_tree", 20, 1, 200);
        candidate.hitSeconds = read("hit_seconds", 2, 2, 10);
        candidate.fallenSeconds = read("fallen_seconds", 180, 10, 600);
        candidate.respawnSeconds = read("respawn_seconds", 300, 10, 3600);
        candidate.logChance = read("log_chance_percent", 30, 0, 100);
        candidate.pointChance = read("point_chance_percent", 20, 0, 100);
        candidate.refineChance = read("refine_chance_percent", 80, 1, 100);
        settings = candidate;
        LOG_INFO("WOODCUT_CONFIG enabled={} map=metin2_map_main_razor93 trees={}", settings.enabled, settings.population);
    } catch (const std::exception& error) {
        settings.enabled = false;
        LOG_ERROR("WOODCUT_CONFIG_REJECTED {}", error.what());
    }
}

bool AllowedMap(int32_t index) {
    const auto* region = SECTREE_MANAGER::instance().GetMapRegion(index);
    return index < 10000 && map_allow_find(index) && region &&
        region->strMapName == "metin2_map_main_razor93";
}

bool InReach(entt::entity worker, entt::entity tree) {
    const int64_t dx = static_cast<int64_t>(ecs::PlayerRuntime::GetX(worker)) - ecs::PlayerRuntime::GetX(tree);
    const int64_t dy = static_cast<int64_t>(ecs::PlayerRuntime::GetY(worker)) - ecs::PlayerRuntime::GetY(tree);
    return ecs::PlayerRuntime::GetMapIndex(worker) == ecs::PlayerRuntime::GetMapIndex(tree) &&
        dx * dx + dy * dy <= 300LL * 300; // three metres, centre to centre
}

bool CanWork(entt::entity worker) {
    return g_registry.valid(worker) && ecs::PlayerRuntime::IsPC(worker) &&
        ecs::PlayerRuntime::GetDesc(worker) && !CombatSystem::IsDead(worker) &&
        !CombatSystem::IsStun(worker) && !AffectSystem::IsPolymorphed(worker) &&
        MountSystem::GetMountVnum(worker) == 0 && InventorySystem::CanHandleItems(worker) &&
        !ActivitySystem::IsMining(worker) && !ActivitySystem::IsFishing(worker) &&
        !g_registry.all_of<HerbalismSystem::PruningState>(worker);
}

void InitializeTraining(entt::entity axe) {
    if (ItemSystem::GetItemSocket(axe, 1) == WoodcuttingRules::TrainingMarker) return;
    // The IDs used to be swords. Old socket stones must not instantly qualify
    // an axe for an upgrade; neither do proto sentinel values such as -1.
    ItemSystem::SetItemSocket(axe, 0, 0);
    ItemSystem::SetItemSocket(axe, 1, WoodcuttingRules::TrainingMarker);
}

std::vector<entt::entity> ViewersIncludingWorker(entt::entity tree, entt::entity worker) {
    auto viewers = ecs::VisibilityService::GetViewersOf(g_registry, tree);
    if (g_registry.valid(worker) && std::find(viewers.begin(), viewers.end(), worker) == viewers.end()) viewers.push_back(worker);
    return viewers;
}

void BroadcastSnapshot(entt::entity tree) {
    for (const auto viewer : ViewersIncludingWorker(tree, g_registry.get<TreeState>(tree).worker))
        SendSnapshot(tree, viewer);
}

void HitMotion(entt::entity tree, entt::entity worker) {
    const auto treeVid = ecs::PlayerRuntime::GetPacketVID(tree);
    const auto workerVid = ecs::PlayerRuntime::GetPacketVID(worker);
    for (const auto viewer : ViewersIncludingWorker(tree, worker))
        ecs::ChatSystem::Send(viewer, CHAT_TYPE_COMMAND, "woodcut_swing %u %u %d", treeVid, workerVid, settings.hitSeconds);
}

bool DropLog(entt::entity worker, entt::entity tree) {
    auto item = ITEM_MANAGER::instance().CreateItem(WoodcuttingRules::LogItem, 1);
    if (!ItemSystem::IsValidItem(item)) return false;
    PIXEL_POSITION position {};
    // Use the worker's validated position so loot never lands beyond a wall at
    // the far side of the trunk. All quantities and ownership are server-owned.
    position.x = ecs::PlayerRuntime::GetX(worker);
    position.y = ecs::PlayerRuntime::GetY(worker);
    if (!ItemSystem::PlaceItemOnGround(item, ecs::PlayerRuntime::GetMapIndex(tree), position)) {
        ItemSystem::DestroyItemEntityEcs(item, "WOODCUT_GROUND_FAIL");
        return false;
    }
    if (!ItemSystem::SetGroundOwnership(item, worker, 30)) {
        ItemSystem::DestroyItemEntityEcs(item, "WOODCUT_CLAIM_FAIL");
        return false;
    }
    DBManager::instance().SendMoneyLog(MONEY_LOG_DROP, WoodcuttingRules::LogItem, 1);
    return true;
}

bool GoodLocation(int32_t map, int32_t x, int32_t y) {
    auto* sector = SECTREE_MANAGER::instance().Get(map, x, y);
    if (!sector || (sector->GetAttribute(x, y) & (ATTR_BLOCK | ATTR_OBJECT | ATTR_BANPK))) return false;
    bool clear = true;
    auto inspect = [&](entt::entity entity) {
        const auto* position = g_registry.try_get<ecs::Position>(entity);
        const auto* location = g_registry.try_get<ecs::MapIndex>(entity);
        if (!position || !location || location->value != map) return;
        const auto dx = static_cast<int64_t>(position->x) - x;
        const auto dy = static_cast<int64_t>(position->y) - y;
        if (dx * dx + dy * dy < 600LL * 600) clear = false;
    };
    sector->ForEachAround(inspect);
    return clear;
}

entt::entity Spawn(int32_t map, int32_t x, int32_t y) {
    if (!AllowedMap(map) || !GoodLocation(map, x, y)) return entt::null;
    if (g_registry.view<TreeState>().size() >= 200) return entt::null;
    auto tree = CHARACTER_MANAGER::instance().SpawnMobEntity(WoodcuttingRules::TreeRace, map, x, y, 0, false, 0, false);
    if (tree == entt::null || !g_registry.valid(tree)) return entt::null;
    // Install state BEFORE Show: the first insert must already contain a full snapshot.
    g_registry.emplace<TreeState>(tree, TreeState{static_cast<uint32_t>(settings.hits), static_cast<uint32_t>(settings.hits)});
    if (!ecs::MovementSystem::Show(tree, map, x, y, 0, false)) {
        CHARACTER_MANAGER::instance().DestroyCharacter(tree);
        return entt::null;
    }
    return tree;
}

}

bool TryRefine(entt::entity smith, entt::entity worker, entt::entity axe) {
    if (!ItemSystem::IsValidItem(axe) || !WoodcuttingRules::IsAxe(ItemSystem::GetItemVnum(axe))) return false;
    if (!g_registry.valid(smith) || ecs::PlayerRuntime::GetRaceNum(smith) != 20016) return false;
    if (!loaded) LoadSettings();
    if (!settings.enabled || !CanWork(worker) || !InReach(worker, smith) ||
        ItemSystem::GetItemOwnerEntity(axe) != worker) return true;
    Cancel(worker);
    if (ItemSystem::IsItemEquipped(axe) || ItemSystem::GetItemWindow(axe) != INVENTORY) {
        Notice(worker, "WOODCUT_REFINE_UNEQUIP"); return true;
    }
    InitializeTraining(axe);
    const auto vnum = ItemSystem::GetItemVnum(axe);
    const auto points = ItemSystem::GetItemSocket(axe, 0);
    if (!WoodcuttingRules::CanRefine(vnum, points)) { Notice(worker, "WOODCUT_NOT_READY"); return true; }
    if (number(1, 100) > settings.refineChance) {
        ItemSystem::SetItemSocket(axe, 0, points - points / 10);
        Notice(worker, "WOODCUT_REFINE_FAILED"); return true;
    }
    auto upgraded = ITEM_MANAGER::instance().CreateItem(vnum + 1, 1);
    if (!ItemSystem::IsValidItem(upgraded)) return true;
    ItemSystem::SetItemSocket(upgraded, 0, 0);
    ItemSystem::SetItemSocket(upgraded, 1, WoodcuttingRules::TrainingMarker);
    const auto cell = ItemSystem::GetItemCell(axe);
    if (!ItemSystem::RemoveItemEcs(axe) || !ItemSystem::PlaceItemEcs(worker, upgraded, INVENTORY, cell)) {
        if (ItemSystem::GetItemOwnerEntity(axe) == entt::null) ItemSystem::PlaceItemEcs(worker, axe, INVENTORY, cell);
        ItemSystem::DestroyItemEntityEcs(upgraded, "WOODCUT_REFINE_PLACE_ROLLBACK");
        return true;
    }
    if (!ItemSystem::DestroyItemEntityEcs(axe, "WOODCUT_REFINE_SUCCESS")) {
        ItemSystem::RemoveItemEcs(upgraded);
        ItemSystem::PlaceItemEcs(worker, axe, INVENTORY, cell);
        ItemSystem::DestroyItemEntityEcs(upgraded, "WOODCUT_REFINE_SOURCE_ROLLBACK");
        return true;
    }
    Notice(worker, "WOODCUT_REFINE_SUCCESS");
    return true;
}

bool IsTree(entt::entity tree) {
    return tree != entt::null && g_registry.valid(tree) && g_registry.all_of<TreeState>(tree);
}

void SendSnapshot(entt::entity tree, entt::entity viewer) {
    if (!IsTree(tree)) return;
    const auto& state = g_registry.get<TreeState>(tree);
    const uint32_t elapsed = state.fellAt ? (get_dword_time() - state.fellAt) : 0;
    ecs::ChatSystem::Send(viewer, CHAT_TYPE_COMMAND, "woodcut_tree %u %u %u %u %u %d",
        ecs::PlayerRuntime::GetPacketVID(tree), state.remaining, state.total,
        state.fellAt ? 1u : 0u, elapsed, state.direction);
}

void Cancel(entt::entity worker, const char* noticeKey) {
    if (worker == entt::null || !g_registry.valid(worker)) return;
    const auto* state = g_registry.try_get<ChoppingState>(worker);
    if (!state) return;
    const auto tree = state->tree;
    const auto viewers = IsTree(tree) ? ViewersIncludingWorker(tree, worker) : std::vector<entt::entity>{worker};
    g_registry.remove<ChoppingState>(worker);
    for (const auto viewer : viewers)
        ecs::ChatSystem::Send(viewer, CHAT_TYPE_COMMAND, "woodcut_stop %u", ecs::PlayerRuntime::GetPacketVID(worker));
    if (noticeKey) {
        Notice(worker, noticeKey);
        LOG_INFO("WOODCUT_CANCEL worker={} key={}", ecs::PlayerRuntime::GetPacketVID(worker), noticeKey);
    }
    if (IsTree(tree)) {
        auto& trunk = g_registry.get<TreeState>(tree);
        if (trunk.worker == worker) trunk.worker = entt::null;
    }
}

bool Start(entt::entity worker, entt::entity tree) {
    if (!loaded) LoadSettings();
    if (!IsTree(tree)) return false;
    if (!settings.enabled) { Notice(worker, "WOODCUT_UNAVAILABLE"); return true; }
    if (!CanWork(worker)) { Notice(worker, "WOODCUT_CANNOT_WORK"); return true; }
    if (!InReach(worker, tree)) { Notice(worker, "WOODCUT_TOO_FAR"); return true; }
    auto& trunk = g_registry.get<TreeState>(tree);
    if (trunk.fellAt || !trunk.remaining) { Notice(worker, "WOODCUT_FELLED"); return true; }
    if (trunk.worker != worker && g_registry.valid(trunk.worker) && g_registry.all_of<ChoppingState>(trunk.worker)) {
        Notice(worker, "WOODCUT_BUSY"); return true;
    }
    auto axe = ItemSystem::GetWearItem(worker, WEAR_WEAPON);
    if (!ItemSystem::IsValidItem(axe) || !WoodcuttingRules::IsAxe(ItemSystem::GetItemVnum(axe))) {
        Notice(worker, "WOODCUT_NEED_AXE"); return true;
    }
    InitializeTraining(axe);
    // Repeated clicks do not reset the deadline or grant immediate hits.
    if (const auto* current = g_registry.try_get<ChoppingState>(worker); current && current->tree == tree) return true;
    Cancel(worker, nullptr);
    ecs::MovementSystem::Stop(worker);
    g_registry.emplace<ChoppingState>(worker, ChoppingState {tree, axe,
        get_dword_time() + static_cast<uint32_t>(settings.hitSeconds * 1000),
        ecs::PlayerRuntime::GetX(worker), ecs::PlayerRuntime::GetY(worker), ecs::PlayerRuntime::GetMapIndex(worker)});
    g_registry.get<TreeState>(tree).worker = worker;
    ecs::MovementSystem::SetRotationToXY(worker, ecs::PlayerRuntime::GetX(tree), ecs::PlayerRuntime::GetY(tree));
    HitMotion(tree, worker);
    return true;
}

void Tick(uint32_t now) {
    if (!loaded) LoadSettings();
    if (g_bShutdown) return;
    if (!settings.enabled) {
        std::vector<entt::entity> active;
        for (auto worker : g_registry.view<ChoppingState>()) active.push_back(worker);
        for (auto worker : active) Cancel(worker);
        std::vector<entt::entity> trees;
        for (auto tree : g_registry.view<TreeState>()) trees.push_back(tree);
        for (auto tree : trees) CHARACTER_MANAGER::instance().DestroyCharacter(tree);
        return;
    }
    std::vector<entt::entity> workers;
    for (auto worker : g_registry.view<ChoppingState>()) workers.push_back(worker);
    for (auto worker : workers) {
        if (!g_registry.valid(worker)) continue;
        const auto state = g_registry.get<ChoppingState>(worker);
        const auto* movement = g_registry.try_get<ecs::MovementState>(worker);
        if (!CanWork(worker) || !IsTree(state.tree) || !InReach(worker, state.tree) ||
            (movement && movement->moveDuration > 0) ||
            !WoodcuttingRules::StayedInPlace(state.startX, state.startY, state.map,
                ecs::PlayerRuntime::GetX(worker), ecs::PlayerRuntime::GetY(worker),
                ecs::PlayerRuntime::GetMapIndex(worker)) ||
            ItemSystem::GetWearItem(worker, WEAR_WEAPON) != state.axe || !ItemSystem::IsValidItem(state.axe)) {
            Cancel(worker); continue;
        }
        auto& trunk = g_registry.get<TreeState>(state.tree);
        if (!trunk.remaining || trunk.worker != worker) { Cancel(worker); continue; }
        if (!WoodcuttingRules::Due(now, state.nextHit)) continue;
        // One hit at most per tick: never catch up missed time with burst rewards.
        --trunk.remaining;
        const double hitDx = static_cast<double>(ecs::PlayerRuntime::GetX(state.tree)) - state.startX;
        const double hitDy = static_cast<double>(ecs::PlayerRuntime::GetY(state.tree)) - state.startY;
        trunk.direction = static_cast<int>(std::atan2(hitDy, hitDx) * 180.0 / 3.141592653589793);
        for (const auto viewer : ViewersIncludingWorker(state.tree, worker))
            ecs::ChatSystem::Send(viewer, CHAT_TYPE_COMMAND, "woodcut_hit %u %u",
                ecs::PlayerRuntime::GetPacketVID(state.tree), ecs::PlayerRuntime::GetPacketVID(worker));
        const bool rewarded = number(1, 100) <= settings.logChance && DropLog(worker, state.tree);
        Notice(worker, rewarded ? "WOODCUT_LOG_SUCCESS" : "WOODCUT_NO_LOG");
        const auto vnum = ItemSystem::GetItemVnum(state.axe);
        if (vnum < WoodcuttingRules::LastAxe && number(1, 100) <= settings.pointChance) {
            const auto old = ItemSystem::GetItemSocket(state.axe, 0);
            const auto points = WoodcuttingRules::AddPoint(vnum, old);
            if (points != old) {
                ItemSystem::SetItemSocket(state.axe, 0, points);
                if (WoodcuttingRules::CanRefine(vnum, points)) Notice(worker, "WOODCUT_READY");
            }
        }
        if (!trunk.remaining) {
            trunk.fellAt = now ? now : 1;
            BroadcastSnapshot(state.tree);
            Cancel(worker, nullptr);
            Notice(worker, "WOODCUT_TREE_FELLED");
        } else {
            BroadcastSnapshot(state.tree);
            g_registry.get<ChoppingState>(worker).nextHit = now + settings.hitSeconds * 1000;
            HitMotion(state.tree, worker);
        }
    }
    std::vector<entt::entity> expired;
    int standing = 0;
    for (auto tree : g_registry.view<TreeState>()) {
        const auto& state = g_registry.get<TreeState>(tree);
        if (!state.fellAt) ++standing;
        else if (WoodcuttingRules::Due(now, state.fellAt + settings.fallenSeconds * 1000)) expired.push_back(tree);
    }
    for (auto tree : expired) CHARACTER_MANAGER::instance().DestroyCharacter(tree);
    if (!WoodcuttingRules::Due(now, nextSpawn)) return;
    nextSpawn = now + settings.respawnSeconds * 1000;
    const auto* region = SECTREE_MANAGER::instance().FindRegionByExactName("metin2_map_main_razor93");
    if (!region || !AllowedMap(region->index) || !CMobManager::instance().Get(WoodcuttingRules::TreeRace)) return;
    const int missing = settings.population - standing;
    for (int i = 0; i < missing; ++i) {
        for (int attempt = 0; attempt < 30; ++attempt) {
            const int32_t x = number(region->sx / 100 + 1, region->ex / 100 - 1) * 100;
            const int32_t y = number(region->sy / 100 + 1, region->ey / 100 - 1) * 100;
            if (Spawn(region->index, x, y) != entt::null) break;
        }
    }
}

void HandleCommand(entt::entity character, const char* arguments) {
    if (!loaded) LoadSettings();
    std::istringstream input(arguments ? arguments : "");
    std::string action; input >> action;
    if (action == "refine") { Notice(character, "WOODCUT_REFINE_NPC"); return; }
    if (ecs::PlayerRuntime::GetGMLevel(character) < GM_HIGH_WIZARD) return;
    if (action == "reload") { LoadSettings(); Notice(character, "WOODCUT_CONFIG_RELOADED"); return; }
    if (action == "spawn" && settings.enabled) {
        const auto map = ecs::PlayerRuntime::GetMapIndex(character);
        // Search a ring around the GM; do not spawn on top of a player or NPC.
        for (int i = 0; i < 24; ++i) {
            const double angle = i * 3.141592653589793 / 12;
            const auto x = ecs::PlayerRuntime::GetX(character) + static_cast<int32_t>(std::cos(angle) * 800);
            const auto y = ecs::PlayerRuntime::GetY(character) + static_cast<int32_t>(std::sin(angle) * 800);
            if (Spawn(map, x, y) != entt::null) { Notice(character, "WOODCUT_SPAWNED"); return; }
        }
    }
    Notice(character, "WOODCUT_SPAWN_FAILED");
}
}

ACMD(do_woodcut) { WoodcuttingSystem::HandleCommand(character, argument); }
