#include "stdafx.h"
#include "TypedCostumes.hpp"
#include "TypedInventorySync.hpp"
#include "TypedRecovery.hpp"
#include "TypedDrops.hpp"
#include "ItemCatalogRuntime.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/MountFlightSystem.hpp"
#include "../ecs/components/visibility_components.hpp"
#include "../ecs/NetworkService.hpp"
#include "../entity/char_manager.h"
#include "../network/desc.h"
#include <chrono>
#include <map>
#include <set>
#include <openssl/rand.h>

static_assert(HEADER_GC_TYPED_APPEARANCE!=HEADER_GC_TIME_SYNC && HEADER_GC_TYPED_APPEARANCE!=HEADER_GC_PHASE && HEADER_GC_TYPED_APPEARANCE!=HEADER_GC_HANDSHAKE);
namespace {
namespace item=auriga::item;namespace store=item::runtime;using Clock=std::chrono::steady_clock;
using Key=std::tuple<item::ItemOwner,std::uint64_t,std::uint64_t>;
struct WearClock {
    std::shared_ptr<item::persistence::LoadedItemOwner> state;
    std::uint64_t id=0,token=0;Clock::duration accrued{};std::optional<Clock::time_point> since;
    bool worn=false,stopped=false,pending=false;std::vector<std::function<void()>> waiters;
};
std::map<Key,std::shared_ptr<WearClock>> clocks;
bool finishing=false;
Key Identity(item::ItemOwnerFence f,std::uint64_t id){return {f.owner,f.sessionGeneration,id};}
std::uint64_t Elapsed(const WearClock& c){return std::chrono::duration_cast<std::chrono::milliseconds>(c.accrued+(c.since ? Clock::now()-*c.since : Clock::duration{})).count();}
const item::PersistentItem* Costume(const WearClock& c){const auto i=std::ranges::find(c.state->items,c.id,[](const auto& p){return p.id.Value();});return i!=c.state->items.end() && !i->destroyed && i->costume && i->placement && (i->placement->kind==item::ItemStorageKind::Inventory || i->placement->kind==item::ItemStorageKind::Equipment) ? &*i : nullptr;}
entt::entity Owner(const WearClock& c){const auto e=CHARACTER_MANAGER::instance().FindEntityByPID(static_cast<std::uint32_t>(c.state->fence.owner.id));const auto* login=g_registry.valid(e) ? g_registry.try_get<ecs::TypedItemLogin>(e) : nullptr;return login && login->token==c.token ? e : entt::null;}
void Worn(WearClock& c,bool worn,bool timed){c.worn=worn;if(worn && timed && !c.since)c.since=Clock::now();else if((!worn || !timed) && c.since){c.accrued+=Clock::now()-*c.since;c.since.reset();}}
void Stop(WearClock& c){Worn(c,false,false);c.stopped=true;}
struct Appearance {
    std::uint8_t weapon=0,tool=0;std::array<std::string,5> models;
    bool operator==(const Appearance&) const=default;
};
struct CostumeProjection {Appearance data;std::uint64_t revision=1;};
Appearance Models(entt::entity owner) {
    Appearance out;const auto* typed=g_registry.try_get<ecs::TypedItems>(owner);const auto c=store::CurrentCatalog();if(!typed || !c)return out;
    auto effective=typed->state.items;ProjectTypedCostumes(owner,effective);
    for(const auto& p:effective)if(!p.destroyed && p.placement && p.placement->kind==item::ItemStorageKind::Equipment && p.placement->anchor<8) {
        const auto* e=c->Find(p.key);if(p.placement->anchor==0){if(const auto* w=std::get_if<item::ItemWeaponRow>(&e->category))out.weapon=static_cast<unsigned>(w->subtype);if(const auto* t=std::get_if<item::ItemToolRow>(&e->category))out.tool=static_cast<unsigned>(t->kind);}
        if(p.placement->anchor<2 && e->base.appearanceProfile)out.models[p.placement->anchor]=c->FindAppearance(*e->base.appearanceProfile)->modelAsset;
    }
    for(const auto& p:effective)if(!p.destroyed && p.costume && p.placement && p.placement->kind==item::ItemStorageKind::Equipment) {
        const auto& row=std::get<item::ItemCostumeRow>(c->Find(p.key)->category);if(row.weapon && static_cast<unsigned>(*row.weapon)!=out.weapon)continue;
        const auto* visual=c->FindCostumeVisual(p.key,ecs::PlayerRuntime::GetJob(owner)+1,ecs::PlayerRuntime::GetSex(owner)+1);if(visual)out.models[static_cast<unsigned>(row.kind)+1]=visual->modelAsset;
    }
    return out;
}
void Broadcast(entt::entity owner){SendTypedCostumeAppearance(owner,owner);if(const auto* view=g_registry.try_get<ecs::ViewerMap>(owner))for(auto e:view->viewers)SendTypedCostumeAppearance(owner,e);}
}

std::vector<item::CostumeElapsed> TypedCostumeElapsed(entt::entity owner) {
    std::vector<item::CostumeElapsed> out;const auto* typed=g_registry.try_get<ecs::TypedItems>(owner);if(!typed)return out;
    for(auto it=clocks.lower_bound(Identity(typed->state.fence,0));it!=clocks.end() && std::get<0>(it->first)==typed->state.fence.owner && std::get<1>(it->first)==typed->state.fence.sessionGeneration;++it)out.push_back({it->second->id,Elapsed(*it->second)});
    return out;
}
void ProjectTypedCostumes(entt::entity owner,std::vector<item::PersistentItem>& items) {
    const auto c=store::CurrentCatalog();if(!c)return;const auto elapsed=TypedCostumeElapsed(owner);const auto now=TypedItemEffectNow();
    for(auto& p:items)if(!p.destroyed && p.costume) {
        const auto& row=std::get<item::ItemCostumeRow>(c->Find(p.key)->category);const auto debt=std::ranges::find(elapsed,p.id.Value(),&item::CostumeElapsed::instance);
        const auto spent=debt==elapsed.end() ? 0 : debt->elapsedMs;const bool worn=p.placement && p.placement->kind==item::ItemStorageKind::Equipment;
        const auto plan=item::AdvanceCostumeLifetime(row,*p.costume,{now,spent,worn || spent!=0,false});
        const auto* policy=c->FindUsePolicy(c->Find(p.key)->base.usePolicy);
        const bool allowed=policy && (policy->allowedClasses&(1u<<ecs::PlayerRuntime::GetJob(owner))) && (policy->allowedSexes&(1u<<ecs::PlayerRuntime::GetSex(owner)));
        if(!plan || plan->removeItem || !allowed){p.destroyed=true;p.count=0;p.placement.reset();if(plan)p.costume=plan->after;}
        else p.costume=plan->after;
    }
}
void UpdateTypedCostumes(entt::entity owner) {
    const auto* typed=g_registry.try_get<ecs::TypedItems>(owner);const auto* login=g_registry.try_get<ecs::TypedItemLogin>(owner);const auto c=store::CurrentCatalog();if(!typed || !login || !c)return;
    auto* desc=ecs::PlayerRuntime::GetDesc(owner);const bool online=desc && desc->IsPhase(PHASE_GAME);
    std::shared_ptr<item::persistence::LoadedItemOwner> shared;auto first=clocks.lower_bound(Identity(typed->state.fence,0));
    if(first!=clocks.end() && std::get<0>(first->first)==typed->state.fence.owner && std::get<1>(first->first)==typed->state.fence.sessionGeneration)shared=first->second->state;
    if(shared && shared->fence.revision<typed->state.fence.revision)*shared=typed->state;
    for(const auto& p:typed->state.items)if(!p.destroyed && p.costume && p.placement && (p.placement->kind==item::ItemStorageKind::Inventory || p.placement->kind==item::ItemStorageKind::Equipment)) {
        const auto& row=std::get<item::ItemCostumeRow>(c->Find(p.key)->category);if(row.clock==item::CostumeClock::Permanent)continue;
        auto& clock=clocks[Identity(typed->state.fence,p.id.Value())];if(!clock){if(!shared)shared=std::make_shared<item::persistence::LoadedItemOwner>(typed->state);clock=std::make_shared<WearClock>(WearClock{shared,p.id.Value(),login->token});}
        if(!clock->stopped)Worn(*clock,online && p.placement->kind==item::ItemStorageKind::Equipment,row.clock==item::CostumeClock::EquippedTime);
    }
    const auto data=Models(owner);auto* old=g_registry.try_get<CostumeProjection>(owner);
    if(!old || old->data!=data){const auto rev=old ? old->revision+1 : 1;g_registry.emplace_or_replace<CostumeProjection>(owner,data,rev);Broadcast(owner);}
}
void SendTypedCostumeAppearance(entt::entity source,entt::entity viewer) {
    if(!g_registry.valid(source) || !g_registry.valid(viewer))return;
    const auto* support=g_registry.try_get<ecs::MountFlightSystem::Supported>(viewer);const auto* p=g_registry.try_get<CostumeProjection>(source);auto* d=ecs::PlayerRuntime::GetDesc(viewer);
    if(!support || !support->costumes || !p || !d || !d->IsPhase(PHASE_GAME))return;
    std::vector<std::uint8_t> bytes;const auto put=[&](std::uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)bytes.push_back(static_cast<std::uint8_t>(v>>(8*i)));};
    put(HEADER_GC_TYPED_APPEARANCE,1);put(0,4);put(1,1);put(ecs::PlayerRuntime::GetPacketVID(source),4);put(p->revision,8);put(p->data.weapon,1);put(p->data.tool,1);
    for(const auto& model:p->data.models){if(model.size()>512)return;put(model.size(),2);bytes.insert(bytes.end(),model.begin(),model.end());}
    for(unsigned i=0;i<4;++i)bytes[1+i]=static_cast<std::uint8_t>(bytes.size()>>(8*i));d->Packet(bytes.data(),static_cast<int>(bytes.size()));
}
void ReplayTypedCostumeAppearances(entt::entity viewer) {
    for(auto source:g_registry.view<CostumeProjection>())if(source==viewer || (g_registry.all_of<ecs::ViewerMap>(source) && g_registry.get<ecs::ViewerMap>(source).viewers.contains(viewer)))SendTypedCostumeAppearance(source,viewer);
}
void StopTypedCostumes(entt::entity owner) {
    const auto* typed=g_registry.try_get<ecs::TypedItems>(owner);if(!typed)return;
    for(auto it=clocks.lower_bound(Identity(typed->state.fence,0));it!=clocks.end() && std::get<0>(it->first)==typed->state.fence.owner && std::get<1>(it->first)==typed->state.fence.sessionGeneration;++it)Stop(*it->second);
}
void TickTypedCostumes() {
    if(!finishing){std::vector<entt::entity> owners;for(auto owner:g_registry.view<ecs::TypedItems>())owners.push_back(owner);for(auto owner:owners)UpdateTypedCostumes(owner);}
    const auto c=store::CurrentCatalog();if(!c)return;std::vector<std::function<void()>> resume;
    for(auto it=clocks.begin();it!=clocks.end();) {
        auto clock=it->second;const auto owner=Owner(*clock);if(owner==entt::null)Stop(*clock);
        if(clock->pending){++it;continue;}const auto* p=Costume(*clock);const auto elapsed=Elapsed(*clock);
        if(!p || (clock->stopped && !elapsed)){for(auto& waiter:clock->waiters)resume.push_back(std::move(waiter));it=clocks.erase(it);continue;}++it;
        const auto& row=std::get<item::ItemCostumeRow>(c->Find(p->key)->category);const auto now=TypedItemEffectNow();
        const bool due=row.clock==item::CostumeClock::EquippedTime ? elapsed && (!clock->worn || elapsed>=5000 || elapsed>=p->costume->remainingMs) : p->costume->started && now>=p->costume->deadlineUtcMs;
        if(!due || (!clock->stopped && g_registry.all_of<ecs::TypedItemBusy>(owner)))continue;
        std::array<std::uint8_t,16> operation{};if(RAND_bytes(operation.data(),16)!=1)continue;
        const auto f=clock->state->fence;const auto commit=item::MakeCostumeClockCommit(*c,operation,f,clock->state->storages,clock->state->items,p->id,now,elapsed);
        if(!commit){LOG_ERROR("Costume clock failed owner {} error {}",f.owner.id,static_cast<unsigned>(commit.error()));continue;}
        if(!clock->stopped)g_registry.emplace_or_replace<ecs::TypedItemBusy>(owner);
        const bool queued=store::RequestCommit(f.owner,f.sessionGeneration,*commit,[clock](store::CommitOutcome out){const auto owner=Owner(*clock);if(owner==entt::null || clock->stopped)return;if(!out.state){if(auto* d=ecs::PlayerRuntime::GetDesc(owner))d->SetPhase(PHASE_CLOSE);return;}g_registry.emplace_or_replace<ecs::TypedItems>(owner,std::move(*out.state));g_registry.remove<ecs::TypedItemBusy>(owner);SendTypedInventory(owner);PumpTypedItemGrants(owner);});
        if(!queued && !clock->stopped)g_registry.remove<ecs::TypedItemBusy>(owner);
    }
    for(auto& waiter:resume)waiter();
}
void FinishTypedCostumes(){finishing=true;for(auto& [key,c]:clocks){Stop(*c);c->waiters.clear();}while(!clocks.empty()){store::ItemStore::Instance().Drain();TickTypedCostumes();if(!clocks.empty())std::this_thread::sleep_for(std::chrono::milliseconds(1));}finishing=false;}
namespace auriga::item::runtime {
bool DeferCostumeLogin(std::uint32_t pid,std::function<void()> action){for(auto& [key,c]:clocks)if(std::get<0>(key)==ItemOwner{ItemOwnerKind::Character,pid} && c->stopped){c->waiters.push_back(std::move(action));return true;}return false;}
std::function<void(MultiCommitOutcome*)> BeginCostumeCommit(const std::vector<OwnerRead>& owners,const ItemCommit& commit) {
    struct Write {std::shared_ptr<WearClock> clock;std::size_t index;std::uint64_t spent;};std::vector<Write> writes;
    for(const auto& f:commit.owners){const auto owner=std::ranges::find(owners,f.owner,&OwnerRead::owner);if(owner==owners.end())continue;
        for(auto it=clocks.lower_bound(Identity(f,0));it!=clocks.end() && std::get<0>(it->first)==f.owner && std::get<1>(it->first)==f.sessionGeneration;++it){auto clock=it->second;std::uint64_t spent=0;
            if(commit.outcomeCode==CostumeOutcome && commit.result.size()>=28 && commit.result.size()==28+16u*commit.result[27])for(unsigned n=0;n<commit.result[27];++n){std::uint64_t id=0,value=0;for(unsigned j=0;j<8;++j){id|=std::uint64_t{commit.result[28+n*16+j]}<<(8*j);value|=std::uint64_t{commit.result[36+n*16+j]}<<(8*j);}if(id==clock->id)spent=value;}
            clock->pending=true;writes.push_back({clock,static_cast<std::size_t>(owner-owners.begin()),spent});}}
    return [writes=std::move(writes)](MultiCommitOutcome* out){std::set<item::persistence::LoadedItemOwner*> updated;for(const auto& w:writes){auto& c=*w.clock;c.pending=false;if(!out)continue;if(out->write){const auto now=Clock::now();const auto accrued=c.accrued+(c.since ? now-*c.since : Clock::duration{});if(c.since)c.since=now;c.accrued=std::max(Clock::duration::zero(),accrued-std::chrono::milliseconds(w.spent));}if(out->states[w.index]){if(updated.insert(c.state.get()).second)*c.state=*out->states[w.index];}else{Stop(c);c.state->items.clear();}}};
}
}
