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

} // namespace ecs::EntityNetworkDispatchAudit

#endif // AURIGA_LPENTITY_FIXUP_AUDIT
