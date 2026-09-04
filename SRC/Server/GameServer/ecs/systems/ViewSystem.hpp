#pragma once

#include <entt/entt.hpp>

namespace ecs::ViewSystem {

// Drop `other` from `self`'s view, unhook the reverse ViewerMap edge and tell
// `other` the source is gone. `recursive` does the same in the other direction.
void ViewRemove(entt::entity self, entt::entity other, bool recursive);

// Tear down every view edge this entity is part of, in whichever direction
// applies to its kind, and clear its own maps.
void ViewCleanup(entt::entity self);

// Add `other` to `self`'s view, hook the reverse ViewerMap edge and announce
// the insert. `recursive` does the same in the other direction.
void ViewInsert(entt::entity self, entt::entity other, bool recursive);

// Re-send this entity's view state: itself, then every entity it can see.
void ViewReencode(entt::entity self);

} // namespace ecs::ViewSystem
