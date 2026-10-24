#include "../../core/stdafx.h"
#include "../../mission_pass/MissionPassEvents.hpp"
#include "HerbalismSystem.hpp"
#include "HerbalismRules.hpp"
#include "WoodcuttingSystem.hpp"
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
#include "../components/movement_components.hpp"
#include "../components/transform_components.hpp"
#include "../services/VisibilityService.hpp"
#include "../../command/gm.h"
#include "../../command/cmd.h"
#include "../../core/config.h"
#include "../../core/JsonFile.h"
#include "../../core/locale_service.h"
#include "../../core/utils.h"
#include "../../network/db.h"
#include "../../entity/char_manager.h"
#include "../../entity/mob_manager.h"
#include "../../item/item_manager.h"
#include "../../world/sectree_manager.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <sstream>
#include <vector>

extern bool g_bShutdown;
namespace HerbalismSystem {
namespace {
struct Settings {
    bool enabled = false;
    int population = 3, attempts = 8, interval = 3, respawn = 300;
    int dropChance = 40, pointChance = 20, refineChance = 80;
} settings;
bool loaded = false;
uint32_t nextSpawn = 0;
void Notice(entt::entity worker, const char* key) {
    ecs::ChatSystem::Send(worker, CHAT_TYPE_COMMAND, "herb_notice %s", key);
}
void LoadSettings() {
    loaded = true;
    settings = Settings{};
    const auto path = LocaleService_GetBasePath() + "/herbalism.json";
    std::error_code error;
    if (!std::filesystem::exists(path, error)) return;
    nlohmann::json doc;
    if (!JsonFile::Load(path, doc)) return;
    try {
        if (doc.at("schema_version").get<int>() != 1 || doc.at("map").get<std::string>() != "metin2_map_main_razor93")
            throw std::runtime_error("unsupported schema or map");
        auto read = [&](const char* key, int fallback, int low, int high) {
            int value = doc.value(key, fallback);
            if (value < low || value > high) throw std::runtime_error(key);
            return value;
        };
        Settings candidate;
        candidate.enabled = doc.at("enabled").get<bool>();
        candidate.population = read("population_per_species", 3, 0, 10);
        candidate.attempts = read("attempts_per_plant", 8, 1, 100);
        candidate.interval = read("attempt_seconds", 3, 2, 10);
        candidate.respawn = read("respawn_seconds", 300, 30, 3600);
        candidate.dropChance = read("drop_chance_percent", 40, 0, 100);
        candidate.pointChance = read("point_chance_percent", 20, 0, 100);
        candidate.refineChance = read("refine_chance_percent", 80, 1, 100);
        settings = candidate;
        nextSpawn = get_dword_time(); // reload also works after the 32-bit clock half-period
        LOG_INFO("HERBALISM_CONFIG enabled={} population_per_species={}", settings.enabled, settings.population);
    } catch (const std::exception& e) {
        settings.enabled = false;
        LOG_ERROR("HERBALISM_CONFIG_REJECTED {}", e.what());
    }
}
bool AllowedMap(int32_t map) {
    const auto* region = SECTREE_MANAGER::instance().GetMapRegion(map);
    return map < 10000 && map_allow_find(map) && region && region->strMapName == "metin2_map_main_razor93";
}
bool InReach(entt::entity worker, entt::entity plant) {
    const int64_t x = static_cast<int64_t>(ecs::PlayerRuntime::GetX(worker)) - ecs::PlayerRuntime::GetX(plant);
    const int64_t y = static_cast<int64_t>(ecs::PlayerRuntime::GetY(worker)) - ecs::PlayerRuntime::GetY(plant);
    return ecs::PlayerRuntime::GetMapIndex(worker) == ecs::PlayerRuntime::GetMapIndex(plant) && x*x+y*y <= 200LL*200;
}
bool CanWork(entt::entity worker) {
    return g_registry.valid(worker) && ecs::PlayerRuntime::IsPC(worker) && ecs::PlayerRuntime::GetDesc(worker) &&
        !CombatSystem::IsDead(worker) && !CombatSystem::IsStun(worker) && !AffectSystem::IsPolymorphed(worker) &&
        MountSystem::GetMountVnum(worker) == 0 && InventorySystem::CanHandleItems(worker) &&
        !ActivitySystem::IsMining(worker) && !ActivitySystem::IsFishing(worker) &&
        !g_registry.all_of<WoodcuttingSystem::ChoppingState>(worker);
}
std::vector<entt::entity> Viewers(entt::entity plant, entt::entity worker) {
    auto viewers = ecs::VisibilityService::GetViewersOf(g_registry, plant);
    if (g_registry.valid(worker) && std::find(viewers.begin(), viewers.end(), worker) == viewers.end()) viewers.push_back(worker);
    return viewers;
}
void InitializeTraining(entt::entity tool) {
    if (ItemSystem::GetItemSocket(tool, 1) == HerbalismRules::TrainingMarker) return;
    ItemSystem::SetItemSocket(tool, 0, 0);
    ItemSystem::SetItemSocket(tool, 1, HerbalismRules::TrainingMarker);
}
void Swing(entt::entity plant, entt::entity worker) {
    for (auto viewer : Viewers(plant, worker))
        ecs::ChatSystem::Send(viewer, CHAT_TYPE_COMMAND, "herb_prune %u %u %d",
            ecs::PlayerRuntime::GetPacketVID(plant), ecs::PlayerRuntime::GetPacketVID(worker), settings.interval);
}
bool Drop(entt::entity worker, entt::entity plant) {
    const auto herb = HerbalismRules::HerbFor(ecs::PlayerRuntime::GetRaceNum(plant));
    if (!herb) return false;
    auto item = ITEM_MANAGER::instance().CreateItem(herb, 1);
    if (!ItemSystem::IsValidItem(item)) return false;
    PIXEL_POSITION p{};
    p.x = ecs::PlayerRuntime::GetX(worker); p.y = ecs::PlayerRuntime::GetY(worker);
    if (!ItemSystem::PlaceItemOnGround(item, ecs::PlayerRuntime::GetMapIndex(plant), p) ||
        !ItemSystem::SetGroundOwnership(item, worker, 30)) {
        ItemSystem::DestroyItemEntityEcs(item, "HERBALISM_DROP_ROLLBACK"); return false;
    }
    DBManager::instance().SendMoneyLog(MONEY_LOG_DROP, herb, 1);
    AurigaMissionPass::Events::Emit(worker,AurigaMissionPass::Kind::HerbGather,herb);
    return true;
}
bool GoodLocation(int32_t map, int32_t x, int32_t y) {
    auto* sector = SECTREE_MANAGER::instance().Get(map, x, y);
    if (!sector || (sector->GetAttribute(x,y) & (ATTR_BLOCK|ATTR_OBJECT|ATTR_BANPK))) return false;
    bool clear = true;
    auto inspect = [&](entt::entity e) {
        const auto* p = g_registry.try_get<ecs::Position>(e);
        const auto* m = g_registry.try_get<ecs::MapIndex>(e);
        if (!p || !m || m->value != map) return;
        const int64_t dx = static_cast<int64_t>(p->x)-x, dy = static_cast<int64_t>(p->y)-y;
        if (dx*dx+dy*dy < 400LL*400) clear = false;
    };
    sector->ForEachAround(inspect);
    return clear;
}
entt::entity Spawn(uint32_t race, int32_t map, int32_t x, int32_t y) {
    if (!HerbalismRules::IsPlantRace(race) || !AllowedMap(map) || !GoodLocation(map,x,y) ||
        g_registry.view<PlantState>().size() >= 160 || !CMobManager::instance().Get(race)) return entt::null;
    auto plant = CHARACTER_MANAGER::instance().SpawnMobEntity(race,map,x,y,0,false,0,false);
    if (!g_registry.valid(plant)) return entt::null;
    g_registry.emplace<PlantState>(plant, PlantState{static_cast<uint32_t>(settings.attempts), static_cast<uint32_t>(settings.attempts)});
    if (!ecs::MovementSystem::Show(plant,map,x,y,0,false)) {
        CHARACTER_MANAGER::instance().DestroyCharacter(plant); return entt::null;
    }
    return plant;
}
}
bool IsPlant(entt::entity plant) { return g_registry.valid(plant) && g_registry.all_of<PlantState>(plant); }
void SendSnapshot(entt::entity plant, entt::entity viewer) {
    if (!IsPlant(plant)) return;
    const auto& state = g_registry.get<PlantState>(plant);
    ecs::ChatSystem::Send(viewer, CHAT_TYPE_COMMAND, "herb_plant %u %u %u",
        ecs::PlayerRuntime::GetPacketVID(plant),state.remaining,state.total);
}
void Cancel(entt::entity worker, const char* key) {
    if (!g_registry.valid(worker)) return;
    const auto* state = g_registry.try_get<PruningState>(worker);
    if (!state) return;
    const auto plant = state->plant;
    auto viewers = IsPlant(plant) ? Viewers(plant,worker) : std::vector<entt::entity>{worker};
    g_registry.remove<PruningState>(worker);
    for (auto viewer : viewers) ecs::ChatSystem::Send(viewer,CHAT_TYPE_COMMAND,"herb_stop %u",ecs::PlayerRuntime::GetPacketVID(worker));
    if (IsPlant(plant) && g_registry.get<PlantState>(plant).worker == worker) g_registry.get<PlantState>(plant).worker = entt::null;
    if (key) Notice(worker,key);
}
bool Start(entt::entity worker, entt::entity plant) {
    if (!loaded) LoadSettings();
    if (!IsPlant(plant)) return false;
    if (!settings.enabled) { Notice(worker,"HERB_UNAVAILABLE"); return true; }
    if (!CanWork(worker)) { Notice(worker,"HERB_CANNOT_WORK"); return true; }
    if (!InReach(worker,plant)) { Notice(worker,"HERB_TOO_FAR"); return true; }
    auto& state = g_registry.get<PlantState>(plant);
    if (!state.remaining) { Notice(worker,"HERB_DEPLETED"); return true; }
    if (state.worker != worker && g_registry.valid(state.worker) && g_registry.all_of<PruningState>(state.worker)) {
        Notice(worker,"HERB_BUSY"); return true;
    }
    auto tool = ItemSystem::GetWearItem(worker, WEAR_WEAPON);
    if (!ItemSystem::IsValidItem(tool) || !HerbalismRules::IsShears(ItemSystem::GetItemVnum(tool))) {
        Notice(worker,"HERB_NEED_SHEARS"); return true;
    }
    if (const auto* current = g_registry.try_get<PruningState>(worker); current && current->plant == plant) return true;
    Cancel(worker,nullptr);
    InitializeTraining(tool);
    ecs::MovementSystem::Stop(worker);
    g_registry.emplace<PruningState>(worker,PruningState{plant,tool,get_dword_time()+settings.interval*1000u,
        ecs::PlayerRuntime::GetX(worker),ecs::PlayerRuntime::GetY(worker),ecs::PlayerRuntime::GetMapIndex(worker)});
    state.worker = worker;
    ecs::MovementSystem::SetRotationToXY(worker,ecs::PlayerRuntime::GetX(plant),ecs::PlayerRuntime::GetY(plant));
    Swing(plant,worker);
    return true;
}
bool TryRefine(entt::entity smith, entt::entity worker, entt::entity tool) {
    if (!ItemSystem::IsValidItem(tool) || !HerbalismRules::IsShears(ItemSystem::GetItemVnum(tool)) ||
        !g_registry.valid(smith) || ecs::PlayerRuntime::GetRaceNum(smith) != 20016) return false;
    if (!loaded) LoadSettings();
    if (!settings.enabled || !CanWork(worker) || !InReach(worker,smith) || ItemSystem::GetItemOwnerEntity(tool) != worker) return true;
    Cancel(worker);
    if (ItemSystem::IsItemEquipped(tool) || ItemSystem::GetItemWindow(tool) != INVENTORY) { Notice(worker,"HERB_REFINE_UNEQUIP"); return true; }
    InitializeTraining(tool);
    const auto vnum = ItemSystem::GetItemVnum(tool);
    const auto points = ItemSystem::GetItemSocket(tool,0);
    if (!HerbalismRules::CanRefine(vnum,points)) { Notice(worker,"HERB_NOT_READY"); return true; }
    if (number(1,100) > settings.refineChance) {
        ItemSystem::SetItemSocket(tool,0,points-points/10); Notice(worker,"HERB_REFINE_FAILED"); return true;
    }
    auto upgraded = ITEM_MANAGER::instance().CreateItem(vnum+1,1);
    if (!ItemSystem::IsValidItem(upgraded)) return true;
    ItemSystem::SetItemSocket(upgraded,0,0); ItemSystem::SetItemSocket(upgraded,1,HerbalismRules::TrainingMarker);
    const auto cell = ItemSystem::GetItemCell(tool);
    if (!ItemSystem::RemoveItemEcs(tool) || !ItemSystem::PlaceItemEcs(worker,upgraded,INVENTORY,cell)) {
        if (ItemSystem::GetItemOwnerEntity(tool) == entt::null) ItemSystem::PlaceItemEcs(worker,tool,INVENTORY,cell);
        ItemSystem::DestroyItemEntityEcs(upgraded,"HERBALISM_REFINE_ROLLBACK"); return true;
    }
    if (!ItemSystem::DestroyItemEntityEcs(tool,"HERBALISM_REFINE_SUCCESS")) {
        ItemSystem::RemoveItemEcs(upgraded); ItemSystem::PlaceItemEcs(worker,tool,INVENTORY,cell);
        ItemSystem::DestroyItemEntityEcs(upgraded,"HERBALISM_REFINE_SOURCE_ROLLBACK"); return true;
    }
    AurigaMissionPass::Events::Emit(worker,AurigaMissionPass::Kind::ToolUpgrade,vnum+1);
    Notice(worker,"HERB_REFINE_SUCCESS"); return true;
}
void Tick(uint32_t now) {
    if (!loaded) LoadSettings();
    if (g_bShutdown) return;
    std::vector<entt::entity> workers;
    for (auto worker : g_registry.view<PruningState>()) workers.push_back(worker);
    for (auto worker : workers) {
        if (!g_registry.valid(worker)) continue;
        const auto state = g_registry.get<PruningState>(worker);
        const auto* movement = g_registry.try_get<ecs::MovementState>(worker);
        if (!settings.enabled || !CanWork(worker) || !IsPlant(state.plant) || !InReach(worker,state.plant) ||
            (movement && movement->moveDuration > 0) || state.startX != ecs::PlayerRuntime::GetX(worker) ||
            state.startY != ecs::PlayerRuntime::GetY(worker) || state.map != ecs::PlayerRuntime::GetMapIndex(worker) ||
            ItemSystem::GetWearItem(worker,WEAR_WEAPON) != state.shears || !ItemSystem::IsValidItem(state.shears)) { Cancel(worker); continue; }
        auto& plant = g_registry.get<PlantState>(state.plant);
        if (!plant.remaining || plant.worker != worker) { Cancel(worker); continue; }
        if (!HerbalismRules::Due(now,state.nextHit)) continue;
        --plant.remaining;
        for (auto viewer : Viewers(state.plant,worker))
            ecs::ChatSystem::Send(viewer,CHAT_TYPE_COMMAND,"herb_hit %u %u",ecs::PlayerRuntime::GetPacketVID(state.plant),ecs::PlayerRuntime::GetPacketVID(worker));
        Notice(worker,number(1,100) <= settings.dropChance && Drop(worker,state.plant) ? "HERB_SUCCESS" : "HERB_FAILED");
        const auto vnum = ItemSystem::GetItemVnum(state.shears);
        if (vnum < HerbalismRules::LastShears && number(1,100) <= settings.pointChance) {
            auto old = ItemSystem::GetItemSocket(state.shears,0);
            auto points = HerbalismRules::AddPoint(vnum,old);
            if (points != old) {
                ItemSystem::SetItemSocket(state.shears,0,points);
                AurigaMissionPass::Events::Emit(worker,AurigaMissionPass::Kind::ToolTraining,vnum,points-old);
                if (HerbalismRules::CanRefine(vnum,points)) Notice(worker,"HERB_READY");
            }
        }
        if (!plant.remaining) plant.depletedAt = now ? now : 1;
        for (auto viewer : Viewers(state.plant,worker)) SendSnapshot(state.plant,viewer);
        if (!plant.remaining) { Cancel(worker,nullptr); Notice(worker,"HERB_DEPLETED"); }
        else { g_registry.get<PruningState>(worker).nextHit = now + settings.interval*1000u; Swing(state.plant,worker); }
    }
    std::vector<entt::entity> expired;
    std::array<int,12> counts{};
    for (auto plant : g_registry.view<PlantState>()) {
        const auto& state = g_registry.get<PlantState>(plant);
        if (!settings.enabled || (state.depletedAt && HerbalismRules::Due(now,state.depletedAt+settings.respawn*1000u))) expired.push_back(plant);
        // Count depleted plants too: a harvested node cannot immediately respawn
        // as another full node while its individual cooldown is still running.
        const auto race = ecs::PlayerRuntime::GetRaceNum(plant);
        if (HerbalismRules::IsPlantRace(race)) ++counts[race-HerbalismRules::FirstPlant];
    }
    for (auto plant : expired) {
        const auto race = ecs::PlayerRuntime::GetRaceNum(plant);
        if (HerbalismRules::IsPlantRace(race)) --counts[race-HerbalismRules::FirstPlant];
        CHARACTER_MANAGER::instance().DestroyCharacter(plant);
    }
    if (!settings.enabled || !HerbalismRules::Due(now,nextSpawn)) return;
    nextSpawn = now + 10000;
    const auto* region = SECTREE_MANAGER::instance().FindRegionByExactName("metin2_map_main_razor93");
    if (!region || !AllowedMap(region->index)) return;
    for (uint32_t species=0; species<counts.size(); ++species) {
        const int missing = settings.population-counts[species];
        for (int i=0; i<missing; ++i) for (int attempt=0; attempt<30; ++attempt) {
            auto x = number(region->sx/100+1,region->ex/100-1)*100;
            auto y = number(region->sy/100+1,region->ey/100-1)*100;
            if (Spawn(HerbalismRules::FirstPlant+species,region->index,x,y) != entt::null) break;
        }
    }
}
void HandleCommand(entt::entity worker, const char* arguments) {
    if (!loaded) LoadSettings();
    std::istringstream input(arguments ? arguments : "");
    std::string action; input >> action;
    if (action == "refine") { Notice(worker,"HERB_REFINE_NPC"); return; }
    if (ecs::PlayerRuntime::GetGMLevel(worker) < GM_HIGH_WIZARD) return;
    if (action == "reload") { LoadSettings(); Notice(worker,"HERB_RELOADED"); return; }
    uint32_t item = 0; input >> item;
    if (action == "spawn" && settings.enabled && item >= 50701 && item <= 50712) {
        for (int i=0; i<24; ++i) {
            double angle = i*3.141592653589793/12;
            auto x = ecs::PlayerRuntime::GetX(worker)+static_cast<int32_t>(std::cos(angle)*600);
            auto y = ecs::PlayerRuntime::GetY(worker)+static_cast<int32_t>(std::sin(angle)*600);
            if (Spawn(HerbalismRules::FirstPlant+item-50701,ecs::PlayerRuntime::GetMapIndex(worker),x,y) != entt::null) {
                Notice(worker,"HERB_SPAWNED"); return;
            }
        }
    }
    Notice(worker,"HERB_SPAWN_FAILED");
}
}
ACMD(do_herbalism) { HerbalismSystem::HandleCommand(character,argument); }
