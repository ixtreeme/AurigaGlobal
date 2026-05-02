#pragma once

#include <climits>
#include <cstdint>

#include <entt/entt.hpp>

namespace ecs::MovementSystem {

bool Show(entt::entity e, int32_t mapIndex, int32_t x, int32_t y, int32_t z = LONG_MAX, bool showSpawnMotion = false);
bool WarpSet(entt::entity e, int32_t x, int32_t y, int32_t privateMapIndex = 0);
void ExitToSavedLocation(entt::entity e);
bool Move(entt::entity e, int32_t x, int32_t y);
void OnMove(entt::entity e, bool isAttack = false);
bool Goto(entt::entity e, int32_t x, int32_t y);
void Stop(entt::entity e);

} // namespace ecs::MovementSystem

void MovementSystem_Update(entt::registry& reg, uint32_t tick);
