#pragma once
#include "ItemStore.hpp"
#include "../ecs/Registry.hpp"

void UpdateTypedCostumes(entt::entity);
void ProjectTypedCostumes(entt::entity,std::vector<auriga::item::PersistentItem>&);
std::vector<auriga::item::CostumeElapsed> TypedCostumeElapsed(entt::entity);
void TickTypedCostumes();
void StopTypedCostumes(entt::entity);
void FinishTypedCostumes();
void SendTypedCostumeAppearance(entt::entity source,entt::entity viewer);
void ReplayTypedCostumeAppearances(entt::entity viewer);
namespace auriga::item::runtime {
bool DeferCostumeLogin(std::uint32_t,std::function<void()>);
std::function<void(MultiCommitOutcome*)> BeginCostumeCommit(const std::vector<OwnerRead>&,const ItemCommit&);
}
