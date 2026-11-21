#pragma once

#include <cstdint>

#include <entt/entt.hpp>

namespace ActivitySystem {

void StopFishing(entt::entity fisher);
bool IsFishing(entt::entity fisher);
bool StartMining(entt::entity miner, entt::entity load);
void CancelMining(entt::entity miner);
void FinishMining(entt::entity miner);
bool IsMining(entt::entity miner);
int RefineFishingRod(entt::entity owner, entt::entity rod);

} // namespace ActivitySystem
