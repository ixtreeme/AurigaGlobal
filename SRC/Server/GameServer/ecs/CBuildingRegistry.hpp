#pragma once

#include <cstdint>
#include <unordered_map>

#include <entt/entt.hpp>

namespace ecs::CBuildingRegistry {

bool Register(uint32_t buildingId, uint32_t vid, entt::entity e);
void Unregister(uint32_t buildingId, entt::entity expected);

entt::entity FindByID(uint32_t buildingId);
entt::entity FindByVID(uint32_t vid);

}
