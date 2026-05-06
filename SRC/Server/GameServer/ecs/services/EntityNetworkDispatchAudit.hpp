#pragma once

#ifdef AURIGA_LPENTITY_FIXUP_AUDIT

#include <entt/entt.hpp>

namespace ecs::EntityNetworkDispatchAudit {

// LPENTITY.4-fixup.3: runtime drift detector for movement state. Compares
// legacy CHARACTER fields (m_posDest, m_dwMoveStartTime, m_dwMoveDuration,
// m_bNowWalking, m_bAddChrState) against the parallel ECS components
// (MovementDestination, MovementState, StatusFlags) read by native
// EntityNetworkDispatch::SendCharacterInsert. Logs a [MOVEMENT_DRIFT]
// warning per divergent field.
//
// Called from EntityNetworkDispatch::SendInsert (Character branch) under
// AURIGA_LPENTITY_FIXUP_AUDIT. Disabled in production builds.
//
// This file is scheduled for deletion in LPENTITY.6 along with the legacy
// CHARACTER bodies it observes.
void CheckMovementDrift(entt::registry& reg, entt::entity source);

// LPENTITY.4-fixup.4: runtime byte-parity verifier for the character insert
// packet. Builds the native TPacketGCCharacterAdd via BuildCharacterInsert,
// then builds a legacy-shadow packet from CHARACTER fields directly, then
// memcmps. Logs [INSERT_PARITY] with a field summary if the two diverge.
//
// Called from SendInsert AFTER the legacy EncodeInsertPacket emission, so
// the parity check runs on the same source state the wire packet was built
// from. Native packet does NOT get sent here; this is observation only.
void CheckCharacterInsertParity(entt::registry& reg, entt::entity source);

// LPENTITY.4-architect.B.1.1.pre: runtime sweep that verifies the dual-write
// contract for Position. Iterates view<LegacyCharPtr, Position> and for each
// pair compares legacy CHARACTER::GetX/GetY/GetZ (currently still reading
// m_pos via the inline accessors) against the ECS Position component.
// Logs [POSITION_READ_DRIFT] per entity with rate-limit (one log per
// entity per 30 seconds) when the two sources disagree.
//
// Purpose: before the B.1.1 read-source flip lands (CEntity::GetX/Y/Z
// switching from m_pos to ecs::Position), this sweep proves that every
// SetXYZ caller paired with SyncPositionComponents keeps the two sources
// in sync (invariant I1 from A.2 §6). Zero drift over a full WinTest
// session is the gate for the actual flip commit.
//
// Called from the main loop periodic block in main.cpp at ~1Hz.
// Disabled in pure-release builds (#ifdef AURIGA_LPENTITY_FIXUP_AUDIT).
// Deletes in Phase G when m_pos is removed and the comparison becomes
// a tautology (ECS == ECS).
void CheckAllPositionDrift(entt::registry& reg);

} // namespace ecs::EntityNetworkDispatchAudit

#endif // AURIGA_LPENTITY_FIXUP_AUDIT
