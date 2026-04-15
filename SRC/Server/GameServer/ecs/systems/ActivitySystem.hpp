#pragma once

#include <cstdint>

#include <entt/entt.hpp>

namespace ActivitySystem {

void StartFishing(entt::entity fisher, uint32_t tick);
void StopFishing(entt::entity fisher);
void CatchFishing(entt::entity fisher, uint32_t tick);
void CatchFishingFailed(entt::entity fisher);
void CatchDecision(entt::entity fisher, uint32_t itemVnum);
void UpdateFishing(entt::registry& reg, uint32_t tick);

} // namespace ActivitySystem
