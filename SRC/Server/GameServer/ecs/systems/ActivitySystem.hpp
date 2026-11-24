#pragma once

#include <cstdint>

#include <entt/entt.hpp>

namespace ActivitySystem {

void StopFishing(entt::entity fisher);
bool IsFishing(entt::entity fisher);
bool StartMining(entt::entity miner, entt::entity load);
void CancelMining(entt::entity miner);
bool IsMining(entt::entity miner);

} // namespace ActivitySystem
