#pragma once

#include <cstdint>

#include <entt/entt.hpp>

namespace ActivitySystem {

void StartFishing(entt::entity fisher, uint32_t tick);
void StopFishing(entt::entity fisher);
void CatchFishing(entt::entity fisher, uint32_t tick);
void CatchFishingFailed(entt::entity fisher);
void CatchDecision(entt::entity fisher, uint32_t itemVnum);
bool StartMining(entt::entity miner, entt::entity load);
void CancelMining(entt::entity miner);
void FinishMining(entt::entity miner);
bool IsMining(entt::entity miner);
int RefineFishingRod(entt::entity owner, entt::entity rod);

} // namespace ActivitySystem
