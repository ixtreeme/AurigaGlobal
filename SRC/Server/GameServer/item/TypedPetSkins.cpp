#include "stdafx.h"
#include "TypedCompanions.hpp"
#include "ItemCatalogRuntime.hpp"
#include "TypedInventorySync.hpp"
#include "TypedRecovery.hpp"
#include "TypedDrops.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../entity/char_manager.h"
#include "../network/desc.h"
#include "../core/packet.h"
#include <chrono>
#include <map>
#include <set>
#include <openssl/rand.h>

namespace {
namespace item=auriga::item;namespace store=item::runtime;using Clock=std::chrono::steady_clock;
struct SkinClock {
    std::shared_ptr<item::persistence::LoadedItemOwner> state;std::uint64_t id=0,token=0;
    Clock::duration accrued{};std::optional<Clock::time_point> since;
    bool stopped=false,pending=false;std::vector<std::function<void()>> waiters;
};
using Key=std::tuple<item::ItemOwner,std::uint64_t,std::uint64_t>;
std::map<Key,std::shared_ptr<SkinClock>> clocks;
Key Identity(item::ItemOwnerFence f,std::uint64_t id){return {f.owner,f.sessionGeneration,id};}
std::uint64_t Elapsed(const SkinClock& c){return std::chrono::duration_cast<std::chrono::milliseconds>(c.accrued+(c.since ? Clock::now()-*c.since : Clock::duration{})).count();}
void Following(SkinClock& c,bool live){if(live && !c.since)c.since=Clock::now();else if(!live && c.since){c.accrued+=Clock::now()-*c.since;c.since.reset();}}
const item::PersistentItem* Skin(const SkinClock& c){const auto it=std::ranges::find(c.state->items,c.id,[](const auto& p){return p.id.Value();});return it!=c.state->items.end() && !it->destroyed && it->petSupply && it->petSupply->createdAtUtcMs ? &*it : nullptr;}
const item::PetSkinRule* Rule(const item::ItemCatalog& c,const item::PersistentItem& i){const auto* e=c.Find(i.key);const auto* r=e ? std::get_if<item::ItemPetSupplyRow>(&e->category) : nullptr;const auto* p=r ? c.FindPetSupplyProfile(r->profile) : nullptr;return p && p->skin ? &*p->skin : nullptr;}
entt::entity Owner(const SkinClock& c){const auto e=CHARACTER_MANAGER::instance().FindEntityByPID(static_cast<std::uint32_t>(c.state->fence.owner.id));const auto* login=g_registry.valid(e) ? g_registry.try_get<ecs::TypedItemLogin>(e) : nullptr;return login && login->token==c.token ? e : entt::null;}
void Stop(SkinClock& c){Following(c,false);c.stopped=true;}
}
std::uint64_t TypedPetSkinElapsed(entt::entity owner,std::uint64_t id){const auto* state=g_registry.try_get<ecs::TypedItems>(owner);if(!state)return 0;const auto it=clocks.find(Identity(state->state.fence,id));return it==clocks.end() ? 0 : Elapsed(*it->second);}
std::uint32_t TypedPetSkinForm(entt::entity owner,const item::persistence::LoadedItemOwner& state,std::uint64_t seal){
    const auto c=store::CurrentCatalog();if(!c)return 0;
    for(const auto& i:state.items)if(!i.destroyed && i.petSupply && i.petSupply->equippedSeal==seal) {
        const auto* p=Rule(*c,i);const auto* entry=c->Find(i.key);if(!p || entry->base.retired)return 0;
        const auto* use=c->FindUsePolicy(entry->base.usePolicy);
        if(ecs::PointSystem::GetLevel(owner)<entry->base.minimumLevel || !(use->allowedClasses&(1u<<ecs::PlayerRuntime::GetJob(owner))) || !(use->allowedSexes&(1u<<ecs::PlayerRuntime::GetSex(owner))))return 0;
        const auto time=item::AdvanceItemClock({p->clock,p->durationMs},i.petSupply->lifetime,{TypedItemEffectNow(),TypedPetSkinElapsed(owner,i.id.Value()),true,false});
        return time && !time->removeItem ? p->form : 0;
    }
    return 0;
}
void UpdateTypedPetSkins(entt::entity owner,const item::persistence::LoadedItemOwner& state,std::uint64_t live){
    const auto* login=g_registry.try_get<ecs::TypedItemLogin>(owner);const auto c=store::CurrentCatalog();if(!login || !c)return;
    std::shared_ptr<item::persistence::LoadedItemOwner> shared;
    const auto existing=clocks.lower_bound(Identity(state.fence,0));
    if(existing!=clocks.end() && std::get<0>(existing->first)==state.fence.owner && std::get<1>(existing->first)==state.fence.sessionGeneration)shared=existing->second->state;
    if(shared && shared->fence.revision<state.fence.revision)*shared=state;
    for(const auto& i:state.items)if(!i.destroyed && i.petSupply && i.petSupply->createdAtUtcMs) {
        const auto* rule=Rule(*c,i);if(!rule)continue;
        auto& clock=clocks[Identity(state.fence,i.id.Value())];if(!clock){if(!shared)shared=std::make_shared<item::persistence::LoadedItemOwner>(state);clock=std::make_shared<SkinClock>(SkinClock{shared,i.id.Value(),login->token});}
        if(!clock->stopped)Following(*clock,rule->clock==item::CostumeClock::EquippedTime && live && i.petSupply->equippedSeal==live);
    }
}
void TickTypedPetSkins(){
    const auto c=store::CurrentCatalog();if(!c)return;std::vector<std::function<void()>> resume;
    for(auto it=clocks.begin();it!=clocks.end();) {
        const auto clock=it->second;const auto owner=Owner(*clock);if(owner==entt::null)Stop(*clock);
        if(clock->pending){++it;continue;}
        const auto* skin=Skin(*clock);const auto* rule=skin ? Rule(*c,*skin) : nullptr;
        const auto elapsed=Elapsed(*clock);
        if(!skin || !rule || (clock->stopped && !elapsed)) {
            for(auto& waiter:clock->waiters)resume.push_back(std::move(waiter));it=clocks.erase(it);continue;
        }
        ++it;
        if(!clock->stopped && g_registry.all_of<ecs::TypedItemBusy>(owner))continue;
        const auto due=item::AdvanceItemClock({rule->clock,rule->durationMs},skin->petSupply->lifetime,{TypedItemEffectNow(),elapsed,skin->petSupply->equippedSeal!=0,false});
        const bool charge=rule->clock==item::CostumeClock::EquippedTime && elapsed && (!clock->since || elapsed>=5000 || elapsed>=skin->petSupply->lifetime.remainingMs);
        if(!due || (!due->removeItem && !charge))continue;
        std::array<std::uint8_t,16> op{};if(RAND_bytes(op.data(),static_cast<int>(op.size()))!=1)continue;
        const auto commit=item::MakePetSkinClockCommit(*c,op,clock->state->fence,clock->state->storages,clock->state->items,skin->id,TypedItemEffectNow(),elapsed,clock->since.has_value());
        if(!commit){LOG_ERROR("Pet skin clock plan failed owner {} error {}",clock->state->fence.owner.id,static_cast<unsigned>(commit.error()));continue;}
        if(!clock->stopped)g_registry.emplace_or_replace<ecs::TypedItemBusy>(owner);
        const bool queued=store::RequestCommit(clock->state->fence.owner,clock->state->fence.sessionGeneration,*commit,[clock](store::CommitOutcome outcome){
            const auto owner=Owner(*clock);if(owner==entt::null || clock->stopped)return;
            if(!outcome.state){if(auto* d=ecs::PlayerRuntime::GetDesc(owner))d->SetPhase(PHASE_CLOSE);return;}
            g_registry.emplace_or_replace<ecs::TypedItems>(owner,std::move(*outcome.state));g_registry.remove<ecs::TypedItemBusy>(owner);SendTypedInventory(owner);PumpTypedItemGrants(owner);
        });
        if(!queued && !clock->stopped)g_registry.remove<ecs::TypedItemBusy>(owner);
    }
    for(auto& waiter:resume)waiter();
}
void StopTypedPetSkins(entt::entity owner){const auto* t=g_registry.try_get<ecs::TypedItems>(owner);if(!t)return;for(auto& [key,c]:clocks)if(std::get<0>(key)==t->state.fence.owner && std::get<1>(key)==t->state.fence.sessionGeneration)Stop(*c);}
void FinishTypedPetSkins(){for(auto& [key,c]:clocks){Stop(*c);c->waiters.clear();}while(!clocks.empty()){store::ItemStore::Instance().Drain();TickTypedPetSkins();if(!clocks.empty())std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
namespace auriga::item::runtime {
bool DeferPetSkinLogin(std::uint32_t pid,std::function<void()> action){for(auto& [key,c]:clocks)if(std::get<0>(key)==ItemOwner{ItemOwnerKind::Character,pid} && c->stopped){c->waiters.push_back(std::move(action));return true;}return false;}
std::function<void(MultiCommitOutcome*)> BeginPetSkinCommit(const std::vector<OwnerRead>& owners,const ItemCommit& commit){
    struct Write{std::shared_ptr<SkinClock> clock;std::size_t index;std::uint64_t spent;bool reset;};std::vector<Write> writes;
    for(const auto& fence:commit.owners) {
        const auto owner=std::ranges::find(owners,fence.owner,&OwnerRead::owner);if(owner==owners.end())continue;
        for(auto& [key,c]:clocks)if(std::get<0>(key)==fence.owner && std::get<1>(key)==fence.sessionGeneration) {
            const auto change=std::ranges::find(commit.items,c->id,[](const auto& v){return v.after.id.Value();});std::uint64_t spent=0;bool reset=false;
            if(change!=commit.items.end() && change->before && change->before->petSupply && change->after.petSupply) {
                const auto& b=*change->before->petSupply;const auto& a=*change->after.petSupply;
                if(b.lifetime.remainingMs>a.lifetime.remainingMs)spent=b.lifetime.remainingMs-a.lifetime.remainingMs;
                reset=change->after.destroyed || b.equippedSeal!=a.equippedSeal;
            }
            c->pending=true;writes.push_back({c,static_cast<std::size_t>(owner-owners.begin()),spent,reset});
        }
    }
    return [writes=std::move(writes)](MultiCommitOutcome* out){std::set<item::persistence::LoadedItemOwner*> updated;for(const auto& w:writes){auto& c=*w.clock;c.pending=false;if(!out)continue;
        if(out->write){const auto now=Clock::now();auto total=c.accrued+(c.since ? now-*c.since : Clock::duration{});if(c.since)c.since=now;
            c.accrued=std::max(Clock::duration::zero(),total-std::chrono::milliseconds(w.spent));if(w.reset){c.accrued={};c.since.reset();}}
        if(out->states[w.index]){if(updated.insert(c.state.get()).second)*c.state=*out->states[w.index];}else{Stop(c);c.state->items.clear();}
    }};
}
}
