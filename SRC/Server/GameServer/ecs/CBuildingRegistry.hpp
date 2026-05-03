#pragma once

#include <cstdint>
#include <unordered_map>

#include <entt/entt.hpp>

namespace building {
class CObject;
}

namespace ecs::CBuildingRegistry {

void Register(uint32_t buildingId, uint32_t vid, entt::entity e, building::CObject* legacyObject);
void Unregister(uint32_t buildingId);

entt::entity FindByID(uint32_t buildingId);
entt::entity FindByVID(uint32_t vid);
building::CObject* FindLegacyByEntity(entt::entity e);

}
