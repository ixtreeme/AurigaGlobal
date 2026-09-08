#pragma once

#include <entt/entt.hpp>

namespace ecs::VisibilitySystem {

// One directed visibility graph for characters, items, buildings and shops.
// Refresh reconciles membership/position with published edges; unchanged edges
// do not resend insert packets. Remove commits cleanup before publishing.
void Refresh(entt::registry& reg, entt::entity entity);
void Remove(entt::registry& reg, entt::entity entity);
// Blocks insertion from on_destroy signals until tag cleanup has completed.
// Released before network callbacks, where a new spawn is legal.
bool IsRemoving(const entt::registry& reg, entt::entity entity);
void Reencode(entt::registry& reg, entt::entity character);
void Reencode(entt::entity character);

// Idempotent connection to PositionChangedEvent on the global dispatcher.
void Init(entt::registry& reg);
void Shutdown(entt::registry& reg);
// Read-only reverse-edge audit, gated by AURIGA_LPENTITY_FIXUP_AUDIT.
void DriftSweep(entt::registry& reg);

} // namespace ecs::VisibilitySystem
