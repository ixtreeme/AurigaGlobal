#pragma once
#include "ItemStore.hpp"
#include <item/EquipmentStats.hpp>
#include <cstdint>
#include <entt/entity/entity.hpp>

std::uint64_t RefreshTypedCompanion(entt::entity owner);
std::uint64_t TypedGrowthPetElapsed(entt::entity owner,std::uint64_t seal);
auriga::item::PetPassiveSource TypedGrowthPetPassiveSource(entt::entity owner);
void TickTypedCompanions();
void AwardTypedGrowthPetMobExperience(entt::entity owner,entt::entity defeated,int reward);
void StopTypedGrowthPetClock(entt::entity owner);
void FinishTypedGrowthPetClocks();
bool HandleTypedCompanionPacket(entt::entity owner, const char* data);

namespace auriga::item::runtime
{
std::function<void(MultiCommitOutcome*)> BeginGrowthPetClockCommit(const std::vector<OwnerRead>&,const ItemCommit&);
bool DeferGrowthPetLogin(std::uint32_t,std::function<void()>);
}
