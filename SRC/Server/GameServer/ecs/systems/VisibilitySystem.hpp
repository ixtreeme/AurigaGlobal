#pragma once

#include <entt/entt.hpp>

namespace ecs::VisibilitySystem {

// Phase 15E-final.LPENTITY.4-architect.D.3:
// Event-driven visibility maintenance. The Init() call wires
// OnPositionChanged into the global dispatcher; the handler is a no-op
// shim in D.3. D.4 fills in the diff-and-update logic; D.5 adds a
// drift detector against legacy UpdateSectree polling; D.6 disables
// the legacy polling once D.5 reports zero drift.
//
// Init() is idempotent (multiple calls do nothing on the second hit).
// Shutdown() disconnects the handler - intended for unit tests and
// orderly process exit.
void Init(entt::registry& reg);
void Shutdown(entt::registry& reg);

} // namespace ecs::VisibilitySystem
