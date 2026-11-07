#pragma once
#include "ItemStore.hpp"
#include <entt/entity/entity.hpp>
void TickTypedSouls();
void StopTypedSouls(entt::entity);
void FinishTypedSouls();
std::uint64_t TypedSoulElapsed(entt::entity,std::uint64_t);
bool HasTypedSoulChannel(entt::entity,unsigned channel);
bool TypedSoulDamageLocked(entt::entity);
bool DeferTypedSoulDestroy(entt::entity);
// Returns true when the accepted hit was queued, or refused while a soul write is pending.
bool QueueTypedSoulDamage(entt::entity victim,entt::entity attacker,std::int64_t,std::uint8_t,std::function<void(std::int64_t)>);
namespace auriga::item::runtime {
bool DeferSoulLogin(std::uint32_t,std::function<void()>);
std::function<void(MultiCommitOutcome*)> BeginSoulCommit(const std::vector<OwnerRead>&,const ItemCommit&);
}
