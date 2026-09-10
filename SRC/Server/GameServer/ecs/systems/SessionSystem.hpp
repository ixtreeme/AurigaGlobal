#pragma once

#include <entt/entt.hpp>
#include "../components/transform_components.hpp"

namespace ecs::SessionSystem {

bool WarpToPID(entt::entity e, uint32_t dwPID);
bool IsSafeboxOpen(entt::entity character);
void SetSafeboxOpen(entt::entity character, bool open);
bool IsCubeOpen(entt::entity character);
void SetCubeNPC(entt::entity character, entt::entity npc);

int GetSafeboxSize(entt::entity character);
bool SetSafeboxSize(entt::entity character, int size);
bool SetSafeboxOpenPosition(entt::entity character);
float GetDistanceFromSafeboxOpen(entt::entity character);
void SetWarpLocation(entt::entity character, int32_t mapIndex, int32_t x, int32_t y);
ecs::WarpPosition GetWarpLocation(entt::entity character);

} // namespace ecs::SessionSystem
