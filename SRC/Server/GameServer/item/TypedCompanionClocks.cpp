#include "stdafx.h"
#include "TypedCompanions.hpp"
#include "TypedInventorySync.hpp"
#include "TypedDrops.hpp"
#include "TypedRecovery.hpp"
#include "ItemCatalogRuntime.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../entity/char_manager.h"
#include "../network/desc.h"
#include "../core/packet.h"
#include <chrono>
#include <map>
#include <set>
#include <openssl/rand.h>

namespace {
namespace item=auriga::item;namespace store=item::runtime;using Clock=std::chrono::steady_clock;
using Key=std::tuple<item::ItemOwner,std::uint64_t,std::uint64_t>;
struct FixedClock {
    std::shared_ptr<item::persistence::LoadedItemOwner> state;
    std::uint64_t id=0,token=0;Clock::duration accrued{};std::optional<Clock::time_point> since;
    bool active=false,stopped=false,pending=false;std::vector<std::function<void()>> waiters;
};
std::map<Key,std::shared_ptr<FixedClock>> clocks;
Key Identity(item::ItemOwnerFence fence,std::uint64_t id){return {fence.owner,fence.sessionGeneration,id};}
std::uint64_t Elapsed(const FixedClock& c){return std::chrono::duration_cast<std::chrono::milliseconds>(c.accrued+(c.since ? Clock::now()-*c.since : Clock::duration{})).count();}
const item::PersistentItem* Seal(const FixedClock& c){const auto found=std::ranges::find(c.state->items,c.id,[](const auto& i){return i.id.Value();});return found!=c.state->items.end() && !found->destroyed && found->companion ? &*found : nullptr;}
entt::entity Owner(const FixedClock& c){const auto e=CHARACTER_MANAGER::instance().FindEntityByPID(static_cast<std::uint32_t>(c.state->fence.owner.id));const auto* login=g_registry.valid(e) ? g_registry.try_get<ecs::TypedItemLogin>(e) : nullptr;return login && login->token==c.token ? e : entt::null;}
void Active(FixedClock& c,bool active,bool timed){c.active=active;if(active && timed && !c.since)c.since=Clock::now();else if((!active || !timed) && c.since){c.accrued+=Clock::now()-*c.since;c.since.reset();}}
void Stop(FixedClock& c){Active(c,false,false);c.stopped=true;}
}

std::uint64_t TypedFixedCompanionElapsed(entt::entity owner,std::uint64_t id)
{
    const auto* state=g_registry.try_get<ecs::TypedItems>(owner);if(!state)return 0;
    const auto found=clocks.find(Identity(state->state.fence,id));return found==clocks.end() ? 0 : Elapsed(*found->second);
}
void UpdateTypedFixedCompanions(entt::entity owner,const item::persistence::LoadedItemOwner& state,std::uint64_t live)
{
    const auto* login=g_registry.try_get<ecs::TypedItemLogin>(owner);const auto catalog=store::CurrentCatalog();if(!login || !catalog)return;
    std::shared_ptr<item::persistence::LoadedItemOwner> shared;
    auto first=clocks.lower_bound(Identity(state.fence,0));
    if(first!=clocks.end() && std::get<0>(first->first)==state.fence.owner && std::get<1>(first->first)==state.fence.sessionGeneration)shared=first->second->state;
    if(shared && shared->fence.revision<state.fence.revision)*shared=state;
    for(const auto& seal:state.items)if(!seal.destroyed && seal.companion) {
        const auto* entry=catalog->Find(seal.key);const auto* row=entry ? std::get_if<item::ItemCompanionRow>(&entry->category) : nullptr;
        if(!row || row->clock==item::CompanionClock::Permanent)continue;
        auto& clock=clocks[Identity(state.fence,seal.id.Value())];
        if(!clock){if(!shared)shared=std::make_shared<item::persistence::LoadedItemOwner>(state);clock=std::make_shared<FixedClock>(FixedClock{shared,seal.id.Value(),login->token});}
        if(!clock->stopped)Active(*clock,live==seal.id.Value(),row->clock==item::CompanionClock::ActiveTime);
    }
}
void StopTypedFixedCompanions(entt::entity owner)
{
    const auto* state=g_registry.try_get<ecs::TypedItems>(owner);if(!state)return;
    for(auto it=clocks.lower_bound(Identity(state->state.fence,0));it!=clocks.end() && std::get<0>(it->first)==state->state.fence.owner && std::get<1>(it->first)==state->state.fence.sessionGeneration;++it)Stop(*it->second);
}
void TickTypedFixedCompanions()
{
    const auto catalog=store::CurrentCatalog();if(!catalog)return;
    std::vector<std::function<void()>> resume;
    for(auto it=clocks.begin();it!=clocks.end();) {
        auto clock=it->second;const auto owner=Owner(*clock);if(owner==entt::null)Stop(*clock);
        if(clock->pending){++it;continue;}
        const auto* seal=Seal(*clock);const auto elapsed=Elapsed(*clock);
        if(!seal || (clock->stopped && !elapsed)) {
            for(auto& waiter:clock->waiters)resume.push_back(std::move(waiter));it=clocks.erase(it);continue;
        }
        ++it;
        const auto& row=std::get<item::ItemCompanionRow>(catalog->Find(seal->key)->category);const auto& life=seal->companion->lifetime;
        const auto now=TypedItemEffectNow();
        const bool due=row.clock==item::CompanionClock::ActiveTime ? elapsed && (!clock->active || elapsed>=5000 || elapsed>=life.remainingMs) : life.started && now>=life.deadlineUtcMs;
        if(!due || (!clock->stopped && g_registry.all_of<ecs::TypedItemBusy>(owner)))continue;
        const auto fence=clock->state->fence;item::CompanionRequest request{seal->key};request.state=*seal->companion;
        request.instanceId=seal->id.Value();request.revision=seal->revision;request.ownerId=request.actorId=fence.owner.id;
        request.rulesetRevision=1;request.count=seal->count;request.refinementLevel=seal->refinementLevel;
        request.action=item::CompanionAction::Maintain;request.nowUtcMs=now;request.elapsedOwnerMs=elapsed;request.elapsedWasActive=elapsed!=0;request.admittedByServer=true;
        if(RAND_bytes(request.operationId.data(),static_cast<int>(request.operationId.size()))!=1)continue;
        const auto commit=item::MakeCompanionCommit(*catalog,fence,clock->state->storages,clock->state->items,request);
        if(!commit){LOG_ERROR("Fixed companion clock plan failed owner {} error {}",fence.owner.id,static_cast<unsigned>(commit.error()));continue;}
        if(!clock->stopped)g_registry.emplace_or_replace<ecs::TypedItemBusy>(owner);
        const bool queued=store::RequestCommit(fence.owner,fence.sessionGeneration,*commit,[clock](store::CommitOutcome outcome){
            const auto owner=Owner(*clock);if(owner==entt::null || clock->stopped)return;
            if(!outcome.state){if(auto* desc=ecs::PlayerRuntime::GetDesc(owner))desc->SetPhase(PHASE_CLOSE);return;}
            g_registry.emplace_or_replace<ecs::TypedItems>(owner,std::move(*outcome.state));g_registry.remove<ecs::TypedItemBusy>(owner);
            SendTypedInventory(owner);PumpTypedItemGrants(owner);
        });
        if(!queued && !clock->stopped)g_registry.remove<ecs::TypedItemBusy>(owner);
    }
    for(auto& waiter:resume)waiter();
}
void FinishTypedFixedCompanions()
{
    for(auto& [key,clock]:clocks){Stop(*clock);clock->waiters.clear();}
    while(!clocks.empty()){store::ItemStore::Instance().Drain();TickTypedFixedCompanions();if(!clocks.empty())std::this_thread::sleep_for(std::chrono::milliseconds(1));}
}
namespace auriga::item::runtime {
bool DeferFixedCompanionLogin(std::uint32_t pid,std::function<void()> action)
{
    for(auto& [key,clock]:clocks)if(std::get<0>(key)==ItemOwner{ItemOwnerKind::Character,pid} && clock->stopped){clock->waiters.push_back(std::move(action));return true;}return false;
}
std::function<void(MultiCommitOutcome*)> BeginFixedCompanionCommit(const std::vector<OwnerRead>& owners,const ItemCommit& commit)
{
    struct Write {std::shared_ptr<FixedClock> clock;std::size_t index;std::uint64_t spent;};std::vector<Write> writes;
    for(const auto& fence:commit.owners) {
        const auto owner=std::ranges::find(owners,fence.owner,&OwnerRead::owner);if(owner==owners.end())continue;
        for(auto it=clocks.lower_bound(Identity(fence,0));it!=clocks.end() && std::get<0>(it->first)==fence.owner && std::get<1>(it->first)==fence.sessionGeneration;++it) {
            auto clock=it->second;const auto change=std::ranges::find(commit.items,clock->id,[](const auto& i){return i.after.id.Value();});std::uint64_t spent=0;
            if(change!=commit.items.end() && change->before && change->before->companion && change->after.companion && commit.outcomeCode==0x2601 && commit.result.size()==28 && commit.result[3]==3)
                for(unsigned n=0;n<8;++n)spent|=std::uint64_t{commit.result[13+n]}<<(8*n);
            clock->pending=true;writes.push_back({clock,static_cast<std::size_t>(owner-owners.begin()),spent});
        }
    }
    return [writes=std::move(writes)](MultiCommitOutcome* outcome){std::set<item::persistence::LoadedItemOwner*> updated;for(const auto& w:writes){auto& clock=*w.clock;clock.pending=false;if(!outcome)continue;
        if(outcome->write){const auto now=Clock::now();const auto accrued=clock.accrued+(clock.since ? now-*clock.since : Clock::duration{});if(clock.since)clock.since=now;clock.accrued=std::max(Clock::duration::zero(),accrued-std::chrono::milliseconds(w.spent));}
        if(outcome->states[w.index]){if(updated.insert(clock.state.get()).second)*clock.state=*outcome->states[w.index];}
        else{Stop(clock);clock.state->items.clear();}
    }};
}
}
