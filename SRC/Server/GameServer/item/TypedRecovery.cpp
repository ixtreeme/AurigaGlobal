#include "stdafx.h"
#include "TypedRecovery.hpp"
#include "TypedPolymorph.hpp"
#include "TypedTrade.hpp"
#include "TypedEquipment.hpp"
#include "../social/marriage.h"
#include "TypedInventorySync.hpp"
#include "TypedDrops.hpp"
#include "ItemCatalogRuntime.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/ResourceAdmission.hpp"
#include "../ecs/components/status_components.hpp"
#include "../ecs/components/quest_components.hpp"
#include "../quest/questpc.h"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/InventorySystem.hpp"
#include "../ecs/systems/SessionSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../entity/char_manager.h"
#include "../network/desc.h"
#include "../core/packet.h"
#include <item/RecoveryRuntime.hpp>
#include <item/AutomaticRuntime.hpp>
#include <item/SocketMaintenance.hpp>
#include <openssl/rand.h>

namespace
{
namespace items=auriga::item;namespace store=items::runtime;using Time=std::chrono::steady_clock;
enum class Status:std::uint8_t{Closed,Ready,Pending,Applied,Invalid,Denied,Busy,Stale,Failed};
struct RecoverySession
{
    std::uint64_t control=0,preview=0;std::optional<items::PersistentItem> source;
    std::array<std::uint8_t,16> operation{};bool attempted=false,pending=false,visible=false;
    Status status=Status::Closed;std::uint8_t error=255;
};
struct EffectSession { RecoverySession value; };
RecoverySession* LookupSession(entt::entity e,bool effects=false){if(effects){auto* s=g_registry.try_get<EffectSession>(e);return s ? &s->value : nullptr;}return g_registry.try_get<RecoverySession>(e);}
struct RecoveryClock
{
    items::persistence::LoadedItemOwner state;std::uint64_t token=0;
    Time::time_point anchor=Time::now(),end{};bool pending=false,stopped=false,active=false,died=false,clearPolymorph=false;
    items::InstantRecoveryAdmission actor;items::ItemEffectFacts effects;
    std::vector<std::function<void()>> waiters;
};
using Key=std::pair<items::ItemOwner,std::uint64_t>;
std::map<Key,std::shared_ptr<RecoveryClock>> clocks;
std::uint64_t Nonce(){std::uint64_t n=0;while(!n)if(RAND_bytes(reinterpret_cast<unsigned char*>(&n),8)!=1)return 0;return n;}
bool Operation(items::ItemCommit& c){return RAND_bytes(c.operationId.data(),16)==1 && std::ranges::any_of(c.operationId,[](auto b){return b!=0;});}
entt::entity Current(const RecoveryClock& c)
{const auto e=CHARACTER_MANAGER::instance().FindEntityByPID(static_cast<std::uint32_t>(c.state.fence.owner.id));const auto* l=e!=entt::null && g_registry.valid(e) ? g_registry.try_get<ecs::TypedItemLogin>(e) : nullptr;return l && l->token==c.token ? e : entt::null;}
bool InGame(entt::entity e){const auto* d=e!=entt::null && g_registry.valid(e) ? ecs::PlayerRuntime::GetDesc(e) : nullptr;return d && d->IsPhase(PHASE_GAME);}
std::shared_ptr<RecoveryClock> Find(entt::entity e)
{const auto* t=g_registry.valid(e) ? g_registry.try_get<ecs::TypedItems>(e) : nullptr;if(!t)return {};const auto i=clocks.find({t->state.fence.owner,t->state.fence.sessionGeneration});return i==clocks.end() ? nullptr : i->second;}
std::uint64_t Elapsed(const RecoveryClock& c)
{if(!c.active)return 0;const auto now=c.stopped ? c.end : Time::now();return static_cast<std::uint64_t>(std::max<std::int64_t>(0,std::chrono::duration_cast<std::chrono::milliseconds>(now-c.anchor).count()));}
bool Timed(const items::persistence::LoadedItemOwner& state){return !state.recovery.doses.empty() || !state.recovery.cooldowns.empty() || !state.recovery.effects.empty() || state.recovery.polymorph.has_value() || std::ranges::any_of(state.items,[](const auto& i){return (i.automatic && i.automatic->active) || (i.toggle && i.toggle->active);});}
bool AutomaticAllowed(const items::InstantRecoveryAdmission& a){return a.context==items::RecoveryContext::World || a.context==items::RecoveryContext::Duel || a.context==items::RecoveryContext::GuildWar;}
bool SameAutomaticPreview(items::PersistentItem a,items::PersistentItem b)
{if(!a.automatic || !b.automatic || a.automatic->active!=b.automatic->active)return false;a.revision=b.revision;a.automatic=b.automatic;return a==b;}
bool Admitted(entt::entity e,bool off=false)
{
    if(!InGame(e) || (!off && (CombatSystem::IsDead(e) || !InventorySystem::CanHandleItems(e))) || ecs::SocialSystem::HasExchange(e) || ecs::SessionSystem::IsSafeboxOpen(e) || ecs::SessionSystem::IsCubeOpen(e) || ecs::SocialSystem::GetMyShop(e)!=entt::null || ecs::SocialSystem::GetShopOwner(e)!=entt::null)return false;
    const auto* quest=g_registry.try_get<ecs::QuestPCState>(e);if(!quest || !quest->pc || quest->pc->IsRunning())return false;
    const auto* flags=g_registry.try_get<ecs::StatusFlags>(e);return !flags || (off || (!flags->isObserverMode && !flags->isStunned));
}
const items::RecoveryProfile* Profile(const items::ItemCatalog& c,const items::PersistentItem& item)
{const auto* e=c.Find(item.key);const auto* row=e ? std::get_if<items::ItemConsumableRow>(&e->category) : nullptr;const auto* p=row && row->recoveryProfile ? c.FindRecovery(row->recoveryProfile) : nullptr;return p ? &p->Data() : nullptr;}
void Reply(entt::entity e,Status status,std::uint8_t error=255,bool effects=false)
{
    if(!InGame(e))return;const auto c=store::CurrentCatalog();const auto* t=g_registry.try_get<ecs::TypedItems>(e);auto* s=LookupSession(e,effects);if(!c || !t || !s)return;s->status=status;s->error=error;
    const auto projected=items::AdvanceRecoverySchedule(*c,t->state.recovery,TypedRecoveryElapsed(e));if(!projected)return;
    if(effects)
    {
        auto state=*projected;const auto facts=TypedItemEffectFacts(e);items::NormalizeTimedItems(*c,state,TypedItemEffectNow(),&facts);const auto all=items::ProjectToggleItems(*c,t->state.items,TypedRecoveryElapsed(e),facts);if(!all)return;
        const auto found=s->source ? std::ranges::find(*all,s->source->id,&items::PersistentItem::id) : all->end();const auto* source=found!=all->end() ? &*found : s->source ? &*s->source : nullptr;
        const auto packet=items::EncodeItemEffectsRuntime(*c,{static_cast<std::uint8_t>(status),error,s->control,source ? s->preview : 0,source,&state,*all,TypedItemEffectNow()});if(packet)if(auto* desc=ecs::PlayerRuntime::GetDesc(e))desc->LargePacket(packet->data(),static_cast<int>(packet->size()));return;
    }
    const auto actor=TypedRecoveryAdmission(e,{},0);const auto found=s->source ? std::ranges::find(t->state.items,s->source->id,&items::PersistentItem::id) : t->state.items.end();const auto* source=found!=t->state.items.end() ? &*found : nullptr;const auto* profile=source ? Profile(*c,*source) : nullptr;
    const auto packet=items::EncodeRecoveryRuntime(*c,{static_cast<std::uint8_t>(status),error,s->control,source ? s->preview : 0,source,actor.pools,*projected,profile && profile->cooldownProfile ? TypedRecoveryCooldown(e,*profile->cooldownProfile) : 0,t->state.items});
    if(packet)if(auto* desc=ecs::PlayerRuntime::GetDesc(e))desc->LargePacket(packet->data(),static_cast<int>(packet->size()));
}
RecoverySession* Session(entt::entity e,bool effects=false)
{auto* s=LookupSession(e,effects);if(!s){RecoverySession next;next.control=Nonce();if(!next.control)return nullptr;if(effects)g_registry.emplace<EffectSession>(e,EffectSession{std::move(next)});else g_registry.emplace<RecoverySession>(e,std::move(next));s=LookupSession(e,effects);}return s;}
void Complete(const std::shared_ptr<RecoveryClock>& clock,store::CommitOutcome outcome,items::InstantRecoveryPlan healing,bool ownBusy,bool explicitAction,bool effectsAction=false)
{
    const auto e=Current(*clock);if(e==entt::null || clock->stopped)return;
    bool inventory=false;const auto* old=g_registry.try_get<ecs::TypedItems>(e);if(outcome.state){inventory=!old || old->state.items!=outcome.state->items || old->state.wallets!=outcome.state->wallets;g_registry.emplace_or_replace<ecs::TypedItems>(e,std::move(*outcome.state));}
    if(Current(*clock)!=e)return;const bool releaseBusy=ownBusy || !TypedTradeAllowsRecoveryTick(e);g_registry.remove<TypedRecoveryBusy>(e);if(releaseBusy)g_registry.remove<ecs::TypedItemBusy>(e);
    if(outcome.write && InGame(e))
    {
        const auto now=TypedRecoveryAdmission(e,{},0);const auto* flags=g_registry.try_get<ecs::StatusFlags>(e);
        if(explicitAction || (!clock->died && now.context==clock->actor.context && now.inPvp==clock->actor.inPvp && (!flags || (!flags->isStunned && !flags->isObserverMode))))ApplyTypedRecovery(e,healing);
    }
    if(explicitAction){SendTypedInventory(e);if(auto* s=LookupSession(e,effectsAction)){s->pending=false;Reply(e,outcome.write ? Status::Applied : Status::Failed,255,effectsAction);}}
    else{if(inventory)SendTypedInventory(e);else{RefreshTypedEquipment(e);SendTypedEquipment(e);}if(const auto* s=LookupSession(e);s && s->visible)Reply(e,s->status,s->error);if(const auto* s=LookupSession(e,true);s && s->visible)Reply(e,s->status,s->error,true);}
    SendTypedPolymorph(e);
    if(releaseBusy)PumpTypedItemGrants(e);
}
}
void StartTypedRecoveryClock(entt::entity e)
{const auto* t=g_registry.try_get<ecs::TypedItems>(e);const auto* l=g_registry.try_get<ecs::TypedItemLogin>(e);if(!t || !l)return;auto c=std::make_shared<RecoveryClock>();c->state=t->state;c->token=l->token;c->actor.pools={1000,1000,0,0};c->actor.context=items::RecoveryContext::World;clocks[{c->state.fence.owner,c->state.fence.sessionGeneration}]=std::move(c);}
std::uint64_t TypedItemEffectNow(){return static_cast<std::uint64_t>(std::max<std::int64_t>(0,std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()));}
items::ItemEffectFacts TypedItemEffectFacts(entt::entity e)
{items::ItemEffectFacts f;f.actor=TypedRecoveryAdmission(e,{},0);f.alive=f.actor.pools.hpRaw>0 && !CombatSystem::IsDead(e);if(const auto clock=Find(e);clock && clock->died)f.alive=false;f.allowed=f.actor.context==items::RecoveryContext::World || f.actor.context==items::RecoveryContext::Duel || f.actor.context==items::RecoveryContext::Arena || f.actor.context==items::RecoveryContext::GuildWar;if(const auto clock=Find(e);clock && clock->clearPolymorph)f.polymorphAllowed=false;f.married=marriage::CManager::instance().IsMarried(ecs::PlayerRuntime::GetPlayerID(e));f.partyLeader=ecs::SocialSystem::GetPartyLeader(e)==e;return f;}
std::uint64_t TypedRecoveryElapsed(entt::entity e){const auto c=Find(e);if(c && !c->active && !c->stopped && InGame(e)){c->active=true;c->anchor=Time::now();}return c ? Elapsed(*c) : 0;}
std::uint64_t TypedRecoveryCooldown(entt::entity e,std::uint32_t id)
{const auto* t=g_registry.try_get<ecs::TypedItems>(e);if(!t)return 0;const auto remaining=items::RecoveryCooldownRemaining(t->state.recovery,id),elapsed=TypedRecoveryElapsed(e);return remaining-std::min(remaining,elapsed);}
void SendTypedRecovery(entt::entity e){if(auto* s=Session(e)){s->visible=true;Reply(e,s->status==Status::Closed ? Status::Ready : s->status,s->error);}}
void NotifyTypedRecoveryDeath(entt::entity e){const auto c=Find(e);if(c){c->died=true;TickTypedRecovery();}}
bool RequestTypedPolymorphClear(entt::entity e)
{const auto c=Find(e);const auto* t=g_registry.valid(e) ? g_registry.try_get<ecs::TypedItems>(e) : nullptr;if(!c || !t || !t->state.recovery.polymorph)return false;c->clearPolymorph=true;RefreshTypedPolymorph(e);TickTypedRecovery();return true;}
void StopTypedRecoveryClock(entt::entity e)
{const auto c=Find(e);if(!c || c->stopped)return;c->stopped=true;c->end=Time::now();if(g_registry.valid(e)){c->actor=TypedRecoveryAdmission(e,{},0);c->effects=TypedItemEffectFacts(e);}if(!c->pending)c->state=g_registry.get<ecs::TypedItems>(e).state;TickTypedRecovery();}
namespace auriga::item::runtime
{
bool DeferRecoveryLogin(std::uint32_t pid,std::function<void()> resume)
{for(auto& [key,c]:clocks)if(key.first==ItemOwner{ItemOwnerKind::Character,pid} && c->stopped){c->waiters.push_back(std::move(resume));return true;}return false;}
std::expected<std::function<void(MultiCommitOutcome*)>,ItemPersistenceError> BeginRecoveryClockCommit(const std::vector<OwnerRead>& owners,ItemCommit& commit)
{
    struct Mark{std::shared_ptr<RecoveryClock> clock;std::size_t index;std::optional<Time::time_point> boundary;};std::vector<Mark> marks;
    for(std::size_t i=0;i<owners.size();++i){const auto f=std::ranges::find(commit.owners,owners[i].owner,&ItemOwnerFence::owner);if(f==commit.owners.end())return std::unexpected(ItemPersistenceError::InvalidFence);const auto found=clocks.find({f->owner,f->sessionGeneration});if(found==clocks.end())continue;const auto c=found->second;if(c->pending)return std::unexpected(ItemPersistenceError::Conflict);
        const auto e=Current(*c);if(!c->stopped && e!=entt::null){c->state=g_registry.get<ecs::TypedItems>(e).state;c->effects=TypedItemEffectFacts(e);}
        if(c->state.fence!=*f)return std::unexpected(ItemPersistenceError::Conflict);std::optional<Time::time_point> boundary;
        if(commit.recovery && commit.recovery->owner==f->owner){if(commit.recovery->before!=c->state.recovery || commit.recovery->elapsedMs>Elapsed(*c))return std::unexpected(ItemPersistenceError::Conflict);boundary=c->anchor+std::chrono::milliseconds(commit.recovery->elapsedMs);const auto settled=SettleAutomaticTime(commit,c->state.storages,c->state.items,commit.recovery->elapsedMs);if(!settled)return std::unexpected(settled.error());
            auto facts=c->effects;if(c->died)facts.alive=false;NormalizeTimedItems(*CurrentCatalog(),commit.recovery->after,TypedItemEffectNow(),&facts);const auto toggles=SettleToggleTime(*CurrentCatalog(),commit,c->state.storages,c->state.items,commit.recovery->elapsedMs,facts);if(!toggles)return std::unexpected(toggles.error());}
        marks.push_back({c,i,boundary});}
    for(const auto& m:marks)m.clock->pending=true;
    return std::function<void(MultiCommitOutcome*)>{[marks=std::move(marks)](MultiCommitOutcome* out){for(const auto& m:marks){auto& c=*m.clock;c.pending=false;if(!out)continue;if(out->write && m.boundary)c.anchor=*m.boundary;if(m.index<out->states.size() && out->states[m.index])c.state=*out->states[m.index];else if(m.index<out->states.size() && out->states[m.index].error()!=ItemPersistenceError::SqlFailure){c.stopped=true;c.end=c.anchor;c.state.recovery={};}}}};
}
}
void TickTypedRecovery()
{
    const auto catalog=store::CurrentCatalog();if(!catalog)return;std::vector<std::function<void()>> resume;
    for(auto it=clocks.begin();it!=clocks.end();)
    {
        const auto c=it->second;const auto e=Current(*c);if(!c->stopped && e==entt::null){c->stopped=true;c->end=Time::now();}
        if(!c->active && !c->stopped && InGame(e)){c->active=true;c->anchor=Time::now();}
        if(c->pending){++it;continue;}
        if(!c->stopped && e!=entt::null){if(g_registry.all_of<ecs::TypedItemBusy>(e) && !TypedTradeAllowsRecoveryTick(e)){++it;continue;}c->state=g_registry.get<ecs::TypedItems>(e).state;if(c->active){c->actor=TypedRecoveryAdmission(e,{},0);c->effects=TypedItemEffectFacts(e);}}
        if(c->clearPolymorph && !c->state.recovery.polymorph)c->clearPolymorph=false;
        if(c->died && c->state.recovery.doses.empty() && c->state.recovery.creditRaw==std::array<std::uint16_t,2>{} && !items::HasDeathItemEffects(*catalog,c->state.recovery,c->state.items))c->died=false;
        const auto elapsed=Elapsed(*c);
        if(c->stopped && (!Timed(c->state) || !elapsed)){for(auto& waiter:c->waiters)resume.push_back(std::move(waiter));it=clocks.erase(it);continue;}++it;
        if(!Timed(c->state) || (!c->stopped && (!c->active || (elapsed<1000 && !c->died && !c->clearPolymorph))))continue;
        const auto* flags=e!=entt::null && g_registry.valid(e) ? g_registry.try_get<ecs::StatusFlags>(e) : nullptr;
        const bool dead=c->died || (e!=entt::null && CombatSystem::IsDead(e));const bool blocked=c->stopped || !InGame(e) || (flags && (flags->isStunned || flags->isObserverMode));
        auto plan=items::PlanRecoveryScheduleTick(*catalog,c->state.recovery,elapsed,c->actor,dead,blocked);
        if(!plan && plan.error()==items::RecoveryUseError::NoEffect && Timed(c->state)){auto next=items::AdvanceRecoverySchedule(*catalog,c->state.recovery,elapsed);if(next && next->revision!=UINT64_MAX){++next->revision;plan=items::RecoverySchedulePlan{*next};}}
        if(!plan)continue;
        items::ItemCommit commit;commit.kind=items::ItemOperationKind::Service;commit.catalogRevision=catalog->Revision();commit.catalogHash=catalog->ContentHash();commit.owners={c->state.fence};commit.outcomeCode=0x2003;commit.result={'R','T','M',1};commit.recovery=items::ItemRecoveryChange{c->state.fence.owner,c->state.recovery,plan->after,elapsed};if(!Operation(commit))continue;
        if(!items::TickAutomaticOwner(*catalog,commit,c->state.storages,c->state.items,c->actor,*plan,elapsed,blocked || dead,AutomaticAllowed(c->actor)))continue;
        auto facts=c->effects;if(dead)facts.alive=false;items::NormalizeTimedItems(*catalog,plan->after,TypedItemEffectNow(),&facts);commit.recovery->after=plan->after;
        bool ownBusy=false;std::shared_ptr<auriga::resource_admission::Permit> held;
        if(!c->stopped && e!=entt::null){auto permit=ecs::resource_admission::Enter({e},auriga::resource_admission::Operation::ItemUse);if(!permit || !permit->Current() || Current(*c)!=e)continue;held=std::make_shared<auriga::resource_admission::Permit>(std::move(*permit));ownBusy=!g_registry.all_of<ecs::TypedItemBusy>(e);if(ownBusy)g_registry.emplace_or_replace<ecs::TypedItemBusy>(e);g_registry.emplace_or_replace<TypedRecoveryBusy>(e);}
        items::InstantRecoveryPlan healing;healing.hpRestoredRaw=plan->hpRestoredRaw;healing.spRestoredRaw=plan->spRestoredRaw;
        if(!store::RequestCommit(c->state.fence.owner,c->state.fence.sessionGeneration,std::move(commit),[c,healing,ownBusy,held](store::CommitOutcome out){Complete(c,std::move(out),healing,ownBusy,false);}))if(e!=entt::null && !c->stopped){g_registry.remove<TypedRecoveryBusy>(e);if(ownBusy)g_registry.remove<ecs::TypedItemBusy>(e);}
    }
    for(auto& waiter:resume)waiter();
}
void FinishTypedRecoveryClocks()
{for(auto& [key,c]:clocks){if(!c->stopped){c->stopped=true;c->end=Time::now();}c->waiters.clear();}while(!clocks.empty()){store::ItemStore::Instance().Drain();TickTypedRecovery();if(!clocks.empty())std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
void PreviewTypedRecovery(entt::entity e,std::uint64_t id)
{
    auto* s=Session(e);const auto c=store::CurrentCatalog();const auto* t=g_registry.try_get<ecs::TypedItems>(e);if(!s || !c || !t)return;if(s->pending){Reply(e,Status::Pending);return;}
    const auto source=std::ranges::find_if(t->state.items,[&](const auto& i){return i.id.Value()==id;});const auto* p=source!=t->state.items.end() ? Profile(*c,*source) : nullptr;
    if(!p || (p->Delivery()!=items::RecoveryDelivery::Reserve && p->Delivery()!=items::RecoveryDelivery::Periodic && p->Delivery()!=items::RecoveryDelivery::Automatic) || (!items::PlainInventoryItem(*source,t->state.fence.owner) && !(source->automatic && source->automatic->active))){Reply(e,Status::Invalid);return;}
    if(!Admitted(e,source->automatic && source->automatic->active) || g_registry.all_of<ecs::TypedItemBusy>(e)){Reply(e,Status::Denied);return;}
    s->source=*source;s->preview=Nonce();s->attempted=false;s->visible=true;s->pending=false;if(!s->preview || RAND_bytes(s->operation.data(),16)!=1){Reply(e,Status::Failed);return;}
    if(source->automatic){const auto plan=items::PlanAutomaticRecoveryToggle(*c,source->key,source->id,items::AutomaticActor(*c,t->state.items,*source,TypedRecoveryAdmission(e,p->cooldownProfile,1),false,AutomaticAllowed(TypedRecoveryAdmission(e,{},0))));Reply(e,plan ? Status::Ready : Status::Denied,plan ? 255 : 32+static_cast<std::uint8_t>(plan.error()));return;}
    const auto plan=items::PlanScheduledRecoveryUse(*c,source->key,TypedRecoveryAdmission(e,p->cooldownProfile,source->count),t->state.recovery,TypedRecoveryElapsed(e),{Nonce(),Nonce()});Reply(e,plan ? Status::Ready : Status::Denied,plan ? 255 : static_cast<std::uint8_t>(plan.error()));
}
bool HandleTypedRecoveryPacket(entt::entity e,const char* data)
{
    static_assert(sizeof(TPacketCGTypedRecovery)==26);const auto r=*reinterpret_cast<const TPacketCGTypedRecovery*>(data);if(!g_registry.valid(e) || !InGame(e))return true;
    auto* s=Session(e);const auto catalog=store::CurrentCatalog();const auto clock=Find(e);if(!s || !catalog || !clock)return true;if(s->pending){Reply(e,Status::Pending);return true;}
    if(r.action==1){if(r.session || r.target){Reply(e,Status::Invalid);return true;}PreviewTypedRecovery(e,r.source);return true;}
    if(r.action==5){if(r.session || r.source || r.target){Reply(e,Status::Invalid);return true;}s->source.reset();s->preview=0;s->attempted=false;s->status=Status::Ready;s->error=255;SendTypedRecovery(e);return true;}
    if(r.action==3){if(r.session!=s->control){Reply(e,Status::Stale);return true;}s->visible=false;s->source.reset();s->preview=0;Reply(e,Status::Closed);return true;}
    if(!Admitted(e,r.action==2 && s->source && s->source->automatic && s->source->automatic->active)){Reply(e,Status::Denied);return true;}if(g_registry.all_of<ecs::TypedItemBusy>(e)){Reply(e,Status::Busy);return true;}
    auto permit=ecs::resource_admission::Enter({e},auriga::resource_admission::Operation::ItemUse);if(!permit || !permit->Current() || Current(*clock)!=e){Reply(e,Status::Denied);return true;}
    s=g_registry.try_get<RecoverySession>(e);const auto* typed=g_registry.try_get<ecs::TypedItems>(e);if(!s || !typed)return true;const auto state=typed->state;
    const auto elapsed=TypedRecoveryElapsed(e);std::expected<items::RecoverySchedulePlan,items::RecoveryUseError> plan=std::unexpected(items::RecoveryUseError::InvalidState);std::optional<items::ItemCommit> commit;
    if(r.action==2)
    {
        if(!s->source || r.session!=s->preview || r.source!=s->source->id.Value() || r.target){Reply(e,Status::Stale);return true;}if(s->attempted){Reply(e,s->status,s->error);return true;}s->attempted=true;
        const auto source=std::ranges::find(state.items,s->source->id,&items::PersistentItem::id);if(source==state.items.end() || (*source!=*s->source && !SameAutomaticPreview(*source,*s->source))){Reply(e,Status::Stale);return true;}
        const auto* profile=Profile(*catalog,*source);if(!profile){Reply(e,Status::Invalid);return true;}
        if(source->automatic)
        {
            const auto actor=TypedRecoveryAdmission(e,profile->cooldownProfile,1);
            const auto toggle=items::PlanAutomaticRecoveryToggle(*catalog,source->key,source->id,items::AutomaticActor(*catalog,state.items,*source,actor,false,AutomaticAllowed(actor)));
            if(!toggle){Reply(e,Status::Denied,32+static_cast<std::uint8_t>(toggle.error()));return true;}
            auto schedule=items::AdvanceRecoverySchedule(*catalog,state.recovery,elapsed);if(!schedule || schedule->revision==UINT64_MAX){Reply(e,Status::Failed);return true;}++schedule->revision;
            if(toggle->cooldown && !items::SetRecoveryCooldown(*catalog,*schedule,toggle->cooldown->profileId,toggle->cooldown->durationMs)){Reply(e,Status::Failed);return true;}
            items::ItemCommit next;next.operationId=s->operation;next.kind=items::ItemOperationKind::Service;next.catalogRevision=catalog->Revision();next.catalogHash=catalog->ContentHash();next.owners={state.fence};next.outcomeCode=0x2001;next.result={'A','U','T',1};
            if(!items::ChangeAutomatic(next,state.storages,*source,toggle->next)){Reply(e,Status::Failed);return true;}
            if(toggle->deactivateOther){const auto old=std::ranges::find(state.items,*toggle->deactivateOther,&items::PersistentItem::id);if(old==state.items.end() || !old->automatic || !items::ChangeAutomatic(next,state.storages,*old,{old->automatic->remainingRaw,false,0})){Reply(e,Status::Failed);return true;}}
            commit=std::move(next);plan=items::RecoverySchedulePlan{*schedule};
        }
        else plan=items::PlanScheduledRecoveryUse(*catalog,source->key,TypedRecoveryAdmission(e,profile->cooldownProfile,source->count),state.recovery,elapsed,{Nonce(),Nonce()});
        if(plan && !source->automatic){auto spend=items::MakeConsumeCommit(*catalog,s->operation,state.fence,state.storages,state.items,source->id,plan->units);if(!spend){Reply(e,Status::Invalid);return true;}commit=std::move(*spend);commit->outcomeCode=0x1901;}
    }
    else if(r.action==4)
    {
        if(r.session!=s->control){Reply(e,Status::Stale);return true;}plan=items::PlanRecoveryScheduleCancel(*catalog,state.recovery,elapsed,r.source,r.target);
        if(plan){items::ItemCommit next;next.kind=items::ItemOperationKind::Service;next.catalogRevision=catalog->Revision();next.catalogHash=catalog->ContentHash();next.owners={state.fence};next.outcomeCode=0x1902;next.result={'R','C','N',1};if(!Operation(next)){Reply(e,Status::Failed);return true;}commit=std::move(next);}
    }
    else{Reply(e,Status::Invalid);return true;}
    if(!plan || !commit){Reply(e,Status::Denied,static_cast<std::uint8_t>(plan.error()));return true;}
    commit->recovery=items::ItemRecoveryChange{state.fence.owner,state.recovery,plan->after,elapsed};items::InstantRecoveryPlan quota;quota.arenaQuotaToConsume=plan->arenaQuota;
    auto held=std::make_shared<auriga::resource_admission::Permit>(std::move(*permit));s->pending=true;s->status=Status::Pending;g_registry.emplace_or_replace<ecs::TypedItemBusy>(e);g_registry.emplace_or_replace<TypedRecoveryBusy>(e);
    if(!store::RequestCommit(state.fence.owner,state.fence.sessionGeneration,std::move(*commit),[clock,quota,held](store::CommitOutcome out){Complete(clock,std::move(out),quota,true,true);}))
    {g_registry.remove<ecs::TypedItemBusy,TypedRecoveryBusy>(e);if(auto* current=g_registry.try_get<RecoverySession>(e))current->pending=false;Reply(e,Status::Failed);}
    return true;
}

void SendTypedEffects(entt::entity e){if(auto* s=Session(e,true)){s->visible=true;Reply(e,s->status==Status::Closed ? Status::Ready : s->status,s->error,true);}}
bool HandleTypedEffectsPacket(entt::entity e,const char* data)
{
    const auto r=*reinterpret_cast<const TPacketCGTypedRecovery*>(data);if(!g_registry.valid(e) || !InGame(e))return true;
    auto* s=Session(e,true);const auto catalog=store::CurrentCatalog();const auto clock=Find(e);if(!s || !catalog || !clock)return true;
    const auto reply=[&](Status status,std::uint8_t error=255){Reply(e,status,error,true);};
    if(s->pending){reply(Status::Pending);return true;}
    if(r.action==5){if(r.session || r.source || r.target){reply(Status::Invalid);return true;}s->source.reset();s->preview=0;s->attempted=false;s->status=Status::Ready;s->error=255;SendTypedEffects(e);return true;}
    if(r.action==3){if(r.session!=s->control || r.source || r.target){reply(Status::Stale);return true;}s->source.reset();s->preview=0;s->visible=false;reply(Status::Closed);return true;}
    const auto* t=g_registry.try_get<ecs::TypedItems>(e);if(!t)return true;
    if(r.action==1)
    {
        if(r.session || r.target || !r.source){reply(Status::Invalid);return true;}
        const auto source=std::ranges::find_if(t->state.items,[&](const auto& i){return i.id.Value()==r.source;});
        if(source==t->state.items.end() || (!items::TimedItemProfile(*catalog,source->key) && !items::ToggleItemProfile(*catalog,source->key)) || (!items::PlainInventoryItem(*source,t->state.fence.owner) && !(source->toggle && source->toggle->active))){reply(Status::Invalid);return true;}
        if(!Admitted(e,source->toggle && source->toggle->active) || g_registry.all_of<ecs::TypedItemBusy>(e)){reply(Status::Busy);return true;}
        s->source=*source;s->preview=Nonce();s->visible=true;s->attempted=false;if(!s->preview || RAND_bytes(s->operation.data(),16)!=1){reply(Status::Failed);return true;}
        const auto plan=items::MakeItemEffectUse(*catalog,s->operation,t->state.fence,t->state.storages,t->state.items,t->state.recovery,source->id,TypedItemEffectFacts(e),TypedRecoveryElapsed(e),TypedItemEffectNow(),Nonce());reply(plan ? Status::Ready : Status::Denied,plan ? 255 : plan.error().code);return true;
    }
    if(!Admitted(e,r.action==4 || (s->source && s->source->toggle && s->source->toggle->active))){reply(Status::Denied);return true;}if(g_registry.all_of<ecs::TypedItemBusy>(e)){reply(Status::Busy);return true;}
    auto permit=ecs::resource_admission::Enter({e},auriga::resource_admission::Operation::ItemUse);if(!permit || !permit->Current() || Current(*clock)!=e){reply(Status::Denied);return true;}
    s=LookupSession(e,true);t=g_registry.try_get<ecs::TypedItems>(e);if(!s || !t)return true;const auto state=t->state;std::expected<items::ItemCommit,items::ItemEffectError> commit=std::unexpected(items::ItemEffectError{4});
    if(r.action==2)
    {
        if(!s->source || r.session!=s->preview || r.source!=s->source->id.Value() || r.target){reply(Status::Stale);return true;}if(s->attempted){reply(s->status,s->error);return true;}s->attempted=true;
        const auto source=std::ranges::find(state.items,s->source->id,&items::PersistentItem::id);bool same=source!=state.items.end() && *source==*s->source;
        if(!same && source!=state.items.end() && source->toggle && s->source->toggle && source->toggle->active==s->source->toggle->active){auto copy=*s->source;copy.revision=source->revision;copy.toggle=source->toggle;same=copy==*source;}
        if(!same){reply(Status::Stale);return true;}
        commit=items::MakeItemEffectUse(*catalog,s->operation,state.fence,state.storages,state.items,state.recovery,source->id,TypedItemEffectFacts(e),TypedRecoveryElapsed(e),TypedItemEffectNow(),Nonce());
    }
    else if(r.action==4)
    {if(r.session!=s->control){reply(Status::Stale);return true;}items::ItemCommit operation;if(!Operation(operation)){reply(Status::Failed);return true;}commit=items::MakeItemEffectCancel(*catalog,operation.operationId,state.fence,state.recovery,TypedRecoveryElapsed(e),TypedItemEffectNow(),r.source,r.target);}
    else{reply(Status::Invalid);return true;}
    if(!commit){reply(Status::Denied,commit.error().code);return true;}
    auto held=std::make_shared<auriga::resource_admission::Permit>(std::move(*permit));s->pending=true;s->status=Status::Pending;g_registry.emplace_or_replace<ecs::TypedItemBusy>(e);g_registry.emplace_or_replace<TypedRecoveryBusy>(e);
    if(!store::RequestCommit(state.fence.owner,state.fence.sessionGeneration,std::move(*commit),[clock,held](store::CommitOutcome out){Complete(clock,std::move(out),{},true,true,true);}))
    {g_registry.remove<ecs::TypedItemBusy,TypedRecoveryBusy>(e);if(auto* current=LookupSession(e,true))current->pending=false;reply(Status::Failed);}
    return true;
}
