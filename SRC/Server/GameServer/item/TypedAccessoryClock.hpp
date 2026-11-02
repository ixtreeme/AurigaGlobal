#pragma once
#include "ItemStore.hpp"
#include <entt/entity/entity.hpp>

void StartTypedAccessoryClock(entt::entity);
void StopTypedAccessoryClock(entt::entity);
void TickTypedAccessoryClocks();
void FinishTypedAccessoryClocks(); // shutdown only: drain final persisted remainders
std::uint64_t TypedAccessoryElapsed(entt::entity);
bool OnlyTypedAccessoryClockChanges(auriga::item::ItemOwnerFence,auriga::item::ItemOwnerFence);
namespace auriga::item::runtime
{
// Game-thread hooks. A null outcome cancels a submission that was not queued.
std::expected<std::function<void(MultiCommitOutcome*)>,ItemPersistenceError> BeginAccessoryClockCommit(
    const std::vector<OwnerRead>&,ItemCommit&);
bool DeferAccessoryLogin(std::uint32_t,std::function<void()>);
}
