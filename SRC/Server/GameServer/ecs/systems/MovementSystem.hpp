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

// LPENTITY.4-fixup helpers: mirror legacy CHARACTER movement-field writes
// onto the parallel ECS components (MovementDestination, MovementState).
// Call these immediately next to the legacy field write so that
// EntityNetworkDispatch::SendCharacterInsert observes consistent state.
//
// SyncDestinationWrite: emplace_or_replace<MovementDestination>(e, x, y)
// SyncDestinationClear: remove<MovementDestination>(e) and zero MovementState
//                       timing fields (moveStartTime, moveDuration)
// SyncTimingWrite:      patch<MovementState>(e) writing moveStartTime and
//                       moveDuration (MovementState is created by the entity
//                       factory; this updates the existing instance)
// SyncWalkingWrite:     patch<MovementState>(e) writing isNowWalking
//
// All four helpers no-op safely if the entity is null/invalid.
void SyncDestinationWrite(entt::entity e, int32_t x, int32_t y);
void SyncDestinationClear(entt::entity e);
void SyncTimingWrite(entt::entity e, uint32_t startTime, uint32_t duration);
void SyncWalkingWrite(entt::entity e, bool isNowWalking);

} // namespace ecs::MovementSystem

void MovementSystem_Update(entt::registry& reg, uint32_t tick);
