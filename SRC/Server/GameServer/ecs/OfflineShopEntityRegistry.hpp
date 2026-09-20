#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <entt/entt.hpp>

namespace ecs::OfflineShopEntityRegistry {

// Creation commits spatial membership but does not publish visibility. Install
// the manager's owner index before calling SpatialService::UpdateSectree.
entt::entity Create(uint32_t ownerPID, std::string name, uint32_t race,
    int shopType, uint32_t mapIndex, int32_t x, int32_t y);
void Destroy(entt::entity entity);
// Process every pending retirement even when one callback throws. Failed exact
// handles stay caller-owned for retry; the first exception is rethrown last.
void DestroyPending(std::vector<entt::entity>& pending);
entt::entity FindByVID(uint32_t vid);

}
