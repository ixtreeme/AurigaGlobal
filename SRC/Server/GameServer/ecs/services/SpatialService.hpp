#pragma once

#include <functional>

#include <entt/entt.hpp>

#include "../../typedef.h"

class SECTREE;
using LPSECTREE = SECTREE*;

namespace ecs::SpatialService {

LPENTITY LPENTITYFromEntity(entt::registry& reg, entt::entity e);
entt::entity EntityFromLPENTITY(LPENTITY entity);

bool InsertEntity(entt::registry& reg, entt::entity e, uint32_t mapIndex, int32_t x, int32_t y, int32_t z);
void RemoveEntity(entt::registry& reg, entt::entity e);
void UpdateSectree(entt::registry& reg, entt::entity e);

void ForEachAround(entt::registry& reg, entt::entity source, int32_t range, const std::function<void(entt::entity)>& callback);
LPSECTREE GetSectree(entt::registry& reg, entt::entity e);
void ForEachInMap(entt::registry& reg, uint32_t mapIndex, const std::function<void(entt::entity)>& callback);

}

namespace ecs {
inline LPENTITY LPENTITYFromEntity(entt::registry& reg, entt::entity e)
{
    return SpatialService::LPENTITYFromEntity(reg, e);
}

inline entt::entity EntityFromLPENTITY(LPENTITY entity)
{
    return SpatialService::EntityFromLPENTITY(entity);
}
}
