#pragma once
#include "ItemStore.hpp"
#include <item/EquipmentStats.hpp>
#include <cstdint>
#include <entt/entity/entity.hpp>

std::uint64_t RefreshTypedCompanion(entt::entity owner);
std::uint64_t TypedGrowthPetElapsed(entt::entity owner,std::uint64_t seal);
std::uint64_t TypedFixedCompanionElapsed(entt::entity owner,std::uint64_t seal);
void UpdateTypedFixedCompanions(entt::entity,const auriga::item::persistence::LoadedItemOwner&,std::uint64_t live);
void StopTypedFixedCompanions(entt::entity);
void TickTypedFixedCompanions();
void FinishTypedFixedCompanions();
std::optional<bool> TypedMountFlightPermission(entt::entity);
auriga::item::FixedCompanionSource TypedFixedCompanionBonuses(entt::entity);
std::uint64_t TypedPetSkinElapsed(entt::entity owner,std::uint64_t skin);
std::uint32_t TypedPetSkinForm(entt::entity owner,const auriga::item::persistence::LoadedItemOwner&,std::uint64_t seal);
void UpdateTypedPetSkins(entt::entity owner,const auriga::item::persistence::LoadedItemOwner&,std::uint64_t live);
void TickTypedPetSkins();
void StopTypedPetSkins(entt::entity);
void FinishTypedPetSkins();
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
bool DeferPetSkinLogin(std::uint32_t,std::function<void()>);
bool DeferFixedCompanionLogin(std::uint32_t,std::function<void()>);
std::function<void(MultiCommitOutcome*)> BeginFixedCompanionCommit(const std::vector<OwnerRead>&,const ItemCommit&);
std::function<void(MultiCommitOutcome*)> BeginPetSkinCommit(const std::vector<OwnerRead>&,const ItemCommit&);
}
