#include "stdafx.h"
#include "TypedAccessoryClock.hpp"
#include "TypedInventorySync.hpp"
#include "TypedDrops.hpp"
#include "ItemCatalogRuntime.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../entity/char_manager.h"
#include <item/AccessoryRuntime.hpp>
#include <openssl/rand.h>

namespace
{
namespace items=auriga::item;namespace store=items::runtime;using Clock=std::chrono::steady_clock;
struct TypedAccessoryClockState
{
    items::persistence::LoadedItemOwner state;
    Clock::time_point anchor=Clock::now(),end{};
    std::uint64_t token=0;
    bool pending=false,stopped=false;
    std::vector<std::function<void()>> waiters;
    std::map<std::uint64_t,bool> revisions;
};
using Key=std::pair<items::ItemOwner,std::uint64_t>;
std::map<Key,std::shared_ptr<TypedAccessoryClockState>> clocks;
bool Timed(const items::persistence::LoadedItemOwner& state)
{
    return std::ranges::any_of(state.items,[](const auto& i){return !i.destroyed && i.placement && i.placement->kind==items::ItemStorageKind::Equipment && i.accessory && i.accessory->activeGrade && !i.accessory->permanent;});
}
std::uint64_t Elapsed(const TypedAccessoryClockState& clock,Clock::time_point now)
{
    if(clock.stopped)now=std::min(now,clock.end);
    return static_cast<std::uint64_t>(std::max<std::int64_t>(0,std::chrono::duration_cast<std::chrono::milliseconds>(now-clock.anchor).count()));
}
entt::entity Current(const TypedAccessoryClockState& clock)
{
    const auto e=CHARACTER_MANAGER::instance().FindEntityByPID(static_cast<std::uint32_t>(clock.state.fence.owner.id));
    const auto* login=e!=entt::null && g_registry.valid(e) ? g_registry.try_get<ecs::TypedItemLogin>(e) : nullptr;
    return login && login->token==clock.token ? e : entt::null;
}
}
void StartTypedAccessoryClock(entt::entity e)
{
    if(!g_registry.valid(e))return;const auto* typed=g_registry.try_get<ecs::TypedItems>(e);const auto* login=g_registry.try_get<ecs::TypedItemLogin>(e);if(!typed || !login)return;
    auto c=std::make_shared<TypedAccessoryClockState>();c->state=typed->state;c->token=login->token;clocks[{c->state.fence.owner,c->state.fence.sessionGeneration}]=std::move(c);
}
std::uint64_t TypedAccessoryElapsed(entt::entity e)
{
    const auto* typed=g_registry.try_get<ecs::TypedItems>(e);if(!typed)return 0;const auto found=clocks.find({typed->state.fence.owner,typed->state.fence.sessionGeneration});
    return found==clocks.end() ? 0 : Elapsed(*found->second,Clock::now());
}
bool OnlyTypedAccessoryClockChanges(items::ItemOwnerFence before,items::ItemOwnerFence after)
{
    if(before.owner!=after.owner || before.sessionGeneration!=after.sessionGeneration || after.revision<before.revision || after.revision-before.revision>64)return false;
    const auto found=clocks.find({before.owner,before.sessionGeneration});if(found==clocks.end())return false;
    for(auto revision=before.revision;revision<after.revision;){const auto r=found->second->revisions.find(++revision);if(r==found->second->revisions.end() || !r->second)return false;}
    return true;
}
void StopTypedAccessoryClock(entt::entity e)
{
    if(!g_registry.valid(e))return;const auto* typed=g_registry.try_get<ecs::TypedItems>(e);if(!typed)return;
    const auto found=clocks.find({typed->state.fence.owner,typed->state.fence.sessionGeneration});if(found==clocks.end())return;
    auto& clock=*found->second;if(!clock.stopped){clock.stopped=true;clock.end=Clock::now();if(!clock.pending)clock.state=typed->state;}
    // Queue the final remainder before a same-core reconnect can claim a session.
    TickTypedAccessoryClocks();
}
namespace auriga::item::runtime
{
bool DeferAccessoryLogin(std::uint32_t pid,std::function<void()> resume)
{
    for(auto& [key,clock]:clocks)if(key.first==ItemOwner{ItemOwnerKind::Character,pid} && clock->stopped)
    {clock->waiters.push_back(std::move(resume));return true;}
    return false;
}
std::expected<std::function<void(MultiCommitOutcome*)>,ItemPersistenceError> BeginAccessoryClockCommit(const std::vector<OwnerRead>& owners,ItemCommit& commit)
{
    struct Mark{std::shared_ptr<TypedAccessoryClockState> clock;Clock::time_point boundary;std::size_t index;std::uint64_t revision;};std::vector<Mark> marks;
    const bool timerOnly=commit.outcomeCode==0x1703 || commit.outcomeCode==0x1903;
    const auto catalog=CurrentCatalog();if(!catalog)return std::unexpected(ItemPersistenceError::CatalogMismatch);
    const auto now=Clock::now();
    for(std::size_t i=0;i<owners.size();++i)
    {
        const auto fence=std::ranges::find(commit.owners,owners[i].owner,&ItemOwnerFence::owner);if(fence==commit.owners.end())return std::unexpected(ItemPersistenceError::InvalidFence);
        const auto found=clocks.find({fence->owner,fence->sessionGeneration});if(found==clocks.end())continue;auto clock=found->second;
        if(clock->pending)return std::unexpected(ItemPersistenceError::Conflict);
        const auto e=Current(*clock);if(!clock->stopped && e!=entt::null)if(const auto* typed=g_registry.try_get<ecs::TypedItems>(e))clock->state=typed->state;
        if(clock->state.fence!=*fence)return std::unexpected(ItemPersistenceError::Conflict);
        const auto elapsed=Elapsed(*clock,now);
        if(const auto settled=SettleAccessoryTime(*catalog,commit,*fence,clock->state.storages,clock->state.items,elapsed);!settled)return std::unexpected(settled.error());
        marks.push_back({clock,clock->anchor+std::chrono::milliseconds(elapsed),i,fence->revision});
    }
    for(auto& mark:marks)mark.clock->pending=true;
    return std::function<void(MultiCommitOutcome*)>{[marks=std::move(marks),timerOnly](MultiCommitOutcome* outcome){
        for(const auto& mark:marks)
        {
            auto& clock=*mark.clock;clock.pending=false;
            if(!outcome)continue;
            if(outcome->write){clock.anchor=mark.boundary;clock.revisions[mark.revision+1]=timerOnly;while(clock.revisions.size()>64)clock.revisions.erase(clock.revisions.begin());}
            if(mark.index<outcome->states.size() && outcome->states[mark.index])clock.state=*outcome->states[mark.index];
            else if(mark.index<outcome->states.size() && outcome->states[mark.index].error()!=ItemPersistenceError::SqlFailure)
            {clock.stopped=true;clock.end=clock.anchor;clock.state.items.clear();}
        }
    }};
}
}
void TickTypedAccessoryClocks()
{
    std::vector<std::function<void()>> resume;
    for(auto it=clocks.begin();it!=clocks.end();)
    {
        auto clock=it->second;auto& c=*clock;const auto e=Current(c);
        if(!c.stopped && e==entt::null){c.stopped=true;c.end=Clock::now();}
        if(c.pending){++it;continue;}
        if(!c.stopped && e!=entt::null)
        {
            if(g_registry.all_of<ecs::TypedItemBusy>(e)){++it;continue;}
            if(const auto* typed=g_registry.try_get<ecs::TypedItems>(e))c.state=typed->state;
        }
        const auto elapsed=Elapsed(c,Clock::now());
        if(c.stopped && (!Timed(c.state) || !elapsed))
        {for(auto& waiter:c.waiters)resume.push_back(std::move(waiter));it=clocks.erase(it);continue;}
        ++it;
        if(!Timed(c.state) || (!c.stopped && elapsed<1000))continue;
        items::ItemCommit commit;commit.kind=items::ItemOperationKind::Service;commit.catalogRevision=store::CurrentCatalog()->Revision();commit.catalogHash=store::CurrentCatalog()->ContentHash();commit.owners={c.state.fence};commit.outcomeCode=0x1703;commit.result={'A','T','M',1};
        if(RAND_bytes(commit.operationId.data(),16)!=1 || std::ranges::all_of(commit.operationId,[](auto b){return b==0;}))continue;
        // BeginAccessoryClockCommit supplies the complete timed item write set.
        if(e!=entt::null && !c.stopped)g_registry.emplace_or_replace<ecs::TypedItemBusy>(e);
        const auto done=[clock](store::CommitOutcome outcome){
            const auto e=Current(*clock);if(e==entt::null || clock->stopped)return;
            if(outcome.state)g_registry.emplace_or_replace<ecs::TypedItems>(e,std::move(*outcome.state));
            if(Current(*clock)!=e)return;g_registry.remove<ecs::TypedItemBusy>(e);SendTypedInventory(e);PumpTypedItemGrants(e);
        };
        if(!store::RequestCommit(c.state.fence.owner,c.state.fence.sessionGeneration,std::move(commit),done) && e!=entt::null && !c.stopped)g_registry.remove<ecs::TypedItemBusy>(e);
    }
    for(auto& waiter:resume)waiter();
}
void FinishTypedAccessoryClocks()
{
    for(auto& [key,clock]:clocks){if(!clock->stopped){clock->stopped=true;clock->end=Clock::now();}clock->waiters.clear();}
    while(!clocks.empty()){store::ItemStore::Instance().Drain();TickTypedAccessoryClocks();if(!clocks.empty())std::this_thread::sleep_for(std::chrono::milliseconds(1));}
}
