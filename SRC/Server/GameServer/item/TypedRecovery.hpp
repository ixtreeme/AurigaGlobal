#pragma once
#include "ItemStore.hpp"
#include <item/EffectRuntime.hpp>
#include <entt/entity/entity.hpp>
struct TypedRecoveryBusy {};
void StartTypedRecoveryClock(entt::entity);
void StopTypedRecoveryClock(entt::entity);
void NotifyTypedRecoveryDeath(entt::entity);
void TickTypedRecovery();
void FinishTypedRecoveryClocks();
void SendTypedRecovery(entt::entity);
void SendTypedEffects(entt::entity);
bool HandleTypedEffectsPacket(entt::entity,const char*);
auriga::item::ItemEffectFacts TypedItemEffectFacts(entt::entity);
std::uint64_t TypedItemEffectNow();
void PreviewTypedRecovery(entt::entity,std::uint64_t);
bool HandleTypedRecoveryPacket(entt::entity,const char*);
std::uint64_t TypedRecoveryElapsed(entt::entity);
std::uint64_t TypedRecoveryCooldown(entt::entity,std::uint32_t);
auriga::item::InstantRecoveryAdmission TypedRecoveryAdmission(entt::entity,std::optional<std::uint32_t>,std::uint32_t);
void ApplyTypedRecovery(entt::entity,const auriga::item::InstantRecoveryPlan&);
namespace auriga::item::runtime
{
std::expected<std::function<void(MultiCommitOutcome*)>,ItemPersistenceError> BeginRecoveryClockCommit(const std::vector<OwnerRead>&,ItemCommit&);
bool DeferRecoveryLogin(std::uint32_t,std::function<void()>);
}
