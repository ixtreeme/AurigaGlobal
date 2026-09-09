#pragma once

#include <climits>
#include <cstdint>

#include <entt/entt.hpp>

namespace ecs::MovementSystem {

void Motion(entt::entity e, uint8_t motion, entt::entity victim = entt::null);

void SendMovePacket(entt::entity e, uint8_t bFunc, uint8_t bArg, uint32_t x, uint32_t y,
                    uint32_t dwDuration, uint32_t dwTime = 0, float iRot = -1.0f);


#ifdef ENABLE_ANCIENT_PYRAMID
void SetRotation(entt::entity e, float fRot, bool bForce = false);
#else
void SetRotation(entt::entity e, float fRot);
#endif


bool Show(entt::entity e, int32_t mapIndex, int32_t x, int32_t y, int32_t z = LONG_MAX, bool showSpawnMotion = false);
bool WarpSet(entt::entity e, int32_t x, int32_t y, int32_t privateMapIndex = 0);
void SaveExitLocation(entt::entity e);
void ExitToSavedLocation(entt::entity e);
bool Move(entt::entity e, int32_t x, int32_t y);
void OnMove(entt::entity e, bool isAttack = false);
bool Goto(entt::entity e, int32_t x, int32_t y);
void Stop(entt::entity e);
uint32_t GetMotionMode(entt::entity e);
float GetMoveMotionSpeed(entt::entity e);
float GetMoveSpeed(entt::entity e);
void CalculateMoveDuration(entt::entity e);
// Stored timing, not elapsed/remaining time; absent or retired characters return 0.
uint32_t GetCurrentMoveDuration(entt::entity e);
// Changes walking state/timestamp and broadcasts the walk/run mode.
void SetNowWalking(entt::entity e, bool walking);
void SetWalkingPreference(entt::entity e, bool walking);
bool GetWalkingPreference(entt::entity e);

// ECS movement-state write helpers. After Phase 15E-final.LPENTITY.4-architect
// C.2/C.3 these are the *sole* writers for movement destination and timing
// state - the legacy CHARACTER timing/walking fields (m_dwMoveStartTime,
// m_dwMoveDuration, m_bNowWalking) are no longer maintained on character
// paths, and EntityNetworkDispatch::SendCharacterInsert reads ECS components
// directly. The helpers will keep this name through Phase G; the legacy
// fields delete then.
//
// Each helper bundles a small group of related component writes so the
// callsite stays a single line:
//
// SyncDestinationWrite: construct or directly retarget MovementDestination
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
