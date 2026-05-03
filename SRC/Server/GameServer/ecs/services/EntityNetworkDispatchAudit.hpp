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
// Called from EntityNetworkDispatch::BuildCharacterInsert under the
// AURIGA_LPENTITY_FIXUP_AUDIT define. Disabled in production builds.
//
// This file is scheduled for deletion in LPENTITY.6 along with the legacy
// CHARACTER bodies it observes.
void CheckMovementDrift(entt::registry& reg, entt::entity source);

} // namespace ecs::EntityNetworkDispatchAudit

#endif // AURIGA_LPENTITY_FIXUP_AUDIT
