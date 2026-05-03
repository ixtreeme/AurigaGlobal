#pragma once

#include <entt/entt.hpp>

namespace ecs::EntityNetworkDispatch {

void SendInsert(entt::registry& reg, entt::entity source, entt::entity viewer);
void SendRemove(entt::registry& reg, entt::entity source, entt::entity viewer);
void Reencode(entt::registry& reg, entt::entity viewer);

} // namespace ecs::EntityNetworkDispatch
