#pragma once

#include <cstdint>
#include <entt/entt.hpp>

namespace WoodcuttingSystem {
// Components are owned by this system, not MiningState. Destroying an entity
// removes its state automatically; no raw character/item pointers survive ticks.
struct TreeState {
    uint32_t remaining = 20, total = 20, fellAt = 0;
    int direction = 0;
    entt::entity worker = entt::null;
};
struct ChoppingState {
    entt::entity tree = entt::null, axe = entt::null;
    uint32_t nextHit = 0;
    int32_t startX = 0, startY = 0, map = 0;
};
void Tick(uint32_t now);
bool IsTree(entt::entity tree);
bool Start(entt::entity character, entt::entity tree);
void Cancel(entt::entity character, const char* noticeKey = "WOODCUT_INTERRUPTED");
void SendSnapshot(entt::entity tree, entt::entity viewer);
bool TryRefine(entt::entity smith, entt::entity character, entt::entity axe);
void HandleCommand(entt::entity character, const char* arguments);
}
