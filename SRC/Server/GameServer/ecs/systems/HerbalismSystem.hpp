#pragma once
#include <cstdint>
#include <entt/entt.hpp>

namespace HerbalismSystem {
struct PlantState {
    uint32_t remaining = 8, total = 8, depletedAt = 0;
    entt::entity worker = entt::null;
};
struct PruningState {
    entt::entity plant = entt::null;
    std::uint64_t shears = 0;
    uint32_t nextHit = 0;
    int32_t startX = 0, startY = 0, map = 0;
};
void Tick(uint32_t now);
bool IsPlant(entt::entity plant);
bool Start(entt::entity character, entt::entity plant);
void Cancel(entt::entity character, const char* noticeKey = "HERB_INTERRUPTED");
void SendSnapshot(entt::entity plant, entt::entity viewer);
void HandleCommand(entt::entity character, const char* arguments);
}
