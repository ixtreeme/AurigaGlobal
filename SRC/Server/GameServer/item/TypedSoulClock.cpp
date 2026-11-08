#include "stdafx.h"
#include "TypedSoulClock.hpp"
#include "TypedEquipment.hpp"
#include <item/AmmoRuntime.hpp>
#include "TypedInventorySync.hpp"
#include "TypedRecovery.hpp"
#include "TypedDrops.hpp"
#include "ItemCatalogRuntime.hpp"
#include <item/SoulRuntime.hpp>
#include "../ecs/ResourceAdmission.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/AffectSystem.hpp"
#include "../entity/char_manager.h"
#include "../network/desc.h"
#include <openssl/rand.h>
#include <chrono>
#include <map>
#include <set>

namespace {
namespace item=auriga::item;namespace store=item::runtime;using Time=std::chrono::steady_clock;
using Key=std::tuple<item::ItemOwner,std::uint64_t,std::uint64_t>;
struct SoulClock {std::shared_ptr<item::persistence::LoadedItemOwner> state;std::uint64_t id=0,token=0;Time::duration accrued{};std::optional<Time::time_point> since;bool stopped=false,pending=false,died=false;std::vector<std::function<void()>> waiters;};
std::map<Key,std::shared_ptr<SoulClock>> clocks;bool finishing=false;
struct SoulHitReservation {entt::entity victim,attacker;bool finishing=false;std::set<entt::entity> deferred;std::vector<std::function<void()>> waiters;};
struct TypedBowProjectile {unsigned depth=0;};
struct TypedSoulHitLock {std::shared_ptr<SoulHitReservation> hit;};
unsigned pendingHits=0;
Key Identity(item::ItemOwnerFence f,std::uint64_t id){return {f.owner,f.sessionGeneration,id};}
entt::entity Current(item::ItemOwner owner,std::uint64_t token){const auto e=CHARACTER_MANAGER::instance().FindEntityByPID(static_cast<std::uint32_t>(owner.id));const auto* login=g_registry.valid(e) ? g_registry.try_get<ecs::TypedItemLogin>(e) : nullptr;return login && login->token==token ? e : entt::null;}
entt::entity Current(const SoulClock& c){return Current(c.state->fence.owner,c.token);}
bool Online(entt::entity e){auto* d=g_registry.valid(e) ? ecs::PlayerRuntime::GetDesc(e) : nullptr;return d && d->IsPhase(PHASE_GAME);}
bool Alive(entt::entity e){return g_registry.valid(e) && !CombatSystem::IsDead(e) && ecs::PlayerRuntime::GetHP(e)>0;}
void RunClock(SoulClock& c,bool run){if(run && !c.since)c.since=Time::now();else if(!run && c.since){c.accrued+=Time::now()-*c.since;c.since.reset();}}
std::uint64_t Elapsed(const SoulClock& c){return std::chrono::duration_cast<std::chrono::milliseconds>(c.accrued+(c.since ? Time::now()-*c.since : Time::duration{})).count();}
const item::PersistentItem* Core(const SoulClock& c){const auto p=std::ranges::find(c.state->items,c.id,[](const auto& i){return i.id.Value();});return p!=c.state->items.end() && !p->destroyed && p->soul && p->placement && p->placement->kind==item::ItemStorageKind::Inventory ? &*p : nullptr;}
item::SoulContext Context(entt::entity e){item::SoulContext c;if(g_registry.valid(e)){c.actor={static_cast<std::uint16_t>(std::max(0,ecs::PointSystem::GetLevel(e))),ecs::PlayerRuntime::GetJob(e),ecs::PlayerRuntime::GetSex(e)};c.actor.inCombat=TypedItemEffectFacts(e).actor.inCombat;c.alive=Alive(e);c.polymorphed=AffectSystem::IsPolymorphed(e);}return c;}
std::array<std::uint8_t,16> Operation(){std::array<std::uint8_t,16> op{};if(RAND_bytes(op.data(),16)!=1)op={};return op;}
void Register(entt::entity e){const auto* t=g_registry.try_get<ecs::TypedItems>(e);const auto* login=g_registry.try_get<ecs::TypedItemLogin>(e);const auto c=store::CurrentCatalog();if(!t || !login || !c)return;std::shared_ptr<item::persistence::LoadedItemOwner> shared;auto first=clocks.lower_bound(Identity(t->state.fence,0));if(first!=clocks.end() && std::get<0>(first->first)==t->state.fence.owner && std::get<1>(first->first)==t->state.fence.sessionGeneration)shared=first->second->state;if(shared && shared->fence.revision<t->state.fence.revision)*shared=t->state;
    for(const auto& p:t->state.items)if(!p.destroyed && p.soul && p.placement && p.placement->kind==item::ItemStorageKind::Inventory){auto& clock=clocks[Identity(t->state.fence,p.id.Value())];if(!clock){if(!shared)shared=std::make_shared<item::persistence::LoadedItemOwner>(t->state);clock=std::make_shared<SoulClock>(SoulClock{shared,p.id.Value(),login->token});}const auto& row=std::get<item::ItemSoulRow>(c->Find(p.key)->category);const auto& level=c->FindSoulProfile(*row.profile)->levels[p.refinementLevel];if(!clock->stopped){if(p.soul->charge==level.chargeCapacity && !clock->pending){RunClock(*clock,false);clock->accrued={};}RunClock(*clock,Online(e) && Alive(e) && p.soul->charge<level.chargeCapacity);}}
}
void Unlock(const std::shared_ptr<SoulHitReservation>& hit){auto deferred=std::move(hit->deferred);for(auto e:{hit->victim,hit->attacker})if(g_registry.valid(e))g_registry.remove<TypedSoulHitLock>(e);--pendingHits;for(auto e:deferred)if(g_registry.valid(e))CHARACTER_MANAGER::instance().DestroyCharacter(e);for(auto& action:hit->waiters)action();}
}
std::uint64_t TypedSoulElapsed(entt::entity e,std::uint64_t id){if(!g_registry.valid(e))return 0;Register(e);const auto* t=g_registry.try_get<ecs::TypedItems>(e);if(!t)return 0;const auto found=clocks.find(Identity(t->state.fence,id));return found==clocks.end() ? 0 : Elapsed(*found->second);}
bool HasTypedSoulChannel(entt::entity e,unsigned channel){const auto* t=g_registry.valid(e) ? g_registry.try_get<ecs::TypedItems>(e) : nullptr;const auto c=store::CurrentCatalog();if(!t || !c)return false;for(const auto& p:t->state.items)if(!p.destroyed && p.soul && p.soul->active){const auto& row=std::get<item::ItemSoulRow>(c->Find(p.key)->category);if(static_cast<unsigned>(*row.channel)==channel)return true;}return false;}
bool TypedSoulDamageLocked(entt::entity e){return g_registry.valid(e) && g_registry.all_of<TypedSoulHitLock>(e);}
bool DeferTypedSoulDestroy(entt::entity e){const auto* lock=g_registry.valid(e) ? g_registry.try_get<TypedSoulHitLock>(e) : nullptr;if(!lock)return false;lock->hit->deferred.insert(e);return true;}
TypedBowProjectileScope::TypedBowProjectileScope(entt::entity e){if(HasTypedEquipment(e) && CombatWeaponSubtype(e)==WEAPON_BOW){entity=e;++g_registry.get_or_emplace<TypedBowProjectile>(e).depth;}}
TypedBowProjectileScope::~TypedBowProjectileScope(){if(g_registry.valid(entity)){auto* p=g_registry.try_get<TypedBowProjectile>(entity);if(p && !--p->depth)g_registry.remove<TypedBowProjectile>(entity);}}
bool HasTypedBowAmmunition(entt::entity e){const auto c=store::CurrentCatalog();const auto* t=g_registry.valid(e)?g_registry.try_get<ecs::TypedItems>(e):nullptr;return c && t && item::FindBowAmmo(*c,t->state.fence.owner,t->state.items).has_value();}
bool CanTypedBowShoot(entt::entity e,entt::entity target){
    if(!HasTypedEquipment(e))return true;
    const auto* stats=TypedEquipmentView(e);if(e==target || !stats || stats->weapon!=item::WeaponSubtype::Bow || !Alive(e) || !Alive(target) || !Online(e) || AffectSystem::IsPolymorphed(e) || TypedSoulDamageLocked(e) || TypedSoulDamageLocked(target) || g_registry.all_of<ecs::TypedItemBusy>(e) || !HasTypedBowAmmunition(e) || ecs::PlayerRuntime::GetMapIndex(e)!=ecs::PlayerRuntime::GetMapIndex(target))return false;
    const auto dx=std::int64_t(ecs::PlayerRuntime::GetX(e))-ecs::PlayerRuntime::GetX(target),dy=std::int64_t(ecs::PlayerRuntime::GetY(e))-ecs::PlayerRuntime::GetY(target);const auto reach=stats->reachMillimeters/10;
    return reach && std::abs(dx)<=reach && std::abs(dy)<=reach && dx*dx+dy*dy<=std::int64_t(reach)*reach;
}
bool QueueTypedSoulDamage(entt::entity victim,entt::entity attacker,std::int64_t damage,std::uint8_t type,std::function<void(std::int64_t)> finish){
    using namespace item;
    const bool normal=type==DAMAGE_TYPE_NORMAL || type==DAMAGE_TYPE_NORMAL_RANGE;
    const bool skill=type==DAMAGE_TYPE_MELEE || type==DAMAGE_TYPE_RANGE || type==DAMAGE_TYPE_MAGIC || type==DAMAGE_TYPE_FIRE || type==DAMAGE_TYPE_ICE || type==DAMAGE_TYPE_ELEC;
    if(!g_registry.valid(attacker) || !g_registry.valid(victim) || !ecs::PlayerRuntime::IsPC(attacker))return false;
    const bool bow=HasTypedEquipment(attacker) && CombatWeaponSubtype(attacker)==WEAPON_BOW && (type==DAMAGE_TYPE_NORMAL_RANGE || type==DAMAGE_TYPE_RANGE || ((normal || skill) && g_registry.all_of<TypedBowProjectile>(attacker)));
    const bool soulEligible=(normal || skill) && damage>0 && (ecs::PlayerRuntime::IsMonster(victim) || ecs::PlayerRuntime::IsStone(victim)) && !AffectSystem::IsPolymorphed(attacker) && !CombatSystem::IsUndying(victim);
    if(!bow && !soulEligible)return false;
    const auto c=store::CurrentCatalog();auto* t=g_registry.try_get<ecs::TypedItems>(attacker);auto* login=g_registry.try_get<ecs::TypedItemLogin>(attacker);if(!c || !t || !login)return bow;
    const auto channel=normal?SoulChannel::NormalAttack:SoulChannel::Skill;
    std::optional<ItemInstanceId> coreId;
    if(soulEligible)for(const auto& p:t->state.items)if(!p.destroyed && p.soul && p.soul->active && std::get<ItemSoulRow>(c->Find(p.key)->category).channel==channel){coreId=p.id;break;}
    if(!bow && !coreId)return false;
    if(TypedSoulDamageLocked(attacker) || TypedSoulDamageLocked(victim) || g_registry.all_of<ecs::TypedItemBusy>(attacker))return true;
    auto permit=ecs::resource_admission::Enter({attacker,victim},auriga::resource_admission::Operation::ItemUse);if(!permit || !permit->Current())return true;
    t=g_registry.try_get<ecs::TypedItems>(attacker);login=g_registry.try_get<ecs::TypedItemLogin>(attacker);
    if(!t || !login || !Alive(attacker) || !Alive(victim) || CombatSystem::GetInvincible(victim) || (bow && !CanTypedBowShoot(attacker,victim)) || (coreId && !HasTypedSoulChannel(attacker,static_cast<unsigned>(channel))))return true;
    const auto op=Operation();SoulContext context=Context(attacker);context.channel=channel;context.targetIsNpc=ecs::PlayerRuntime::IsMonster(victim) || ecs::PlayerRuntime::IsStone(victim);context.damage=damage;context.target=std::uint64_t(entt::to_integral(victim))+1;context.targetRevision=1;for(unsigned j=0;j<8;++j)context.event|=std::uint64_t{op[j]}<<(8*j);if(!context.event)return true;
    const auto elapsed=coreId?TypedSoulElapsed(attacker,coreId->Value()):0;
    const auto planned=bow?MakeAmmoShotCommit(*c,op,t->state.fence,t->state.storages,t->state.items,context,coreId,elapsed):MakeSoulCommit(*c,op,t->state.fence,t->state.storages,t->state.items,*coreId,{},SoulAction::Strike,elapsed,0,context);
    if(!planned)return true;
    auto held=std::make_shared<auriga::resource_admission::Permit>(std::move(*permit));auto hit=std::make_shared<SoulHitReservation>(SoulHitReservation{victim,attacker});g_registry.emplace<TypedSoulHitLock>(victim,hit);g_registry.emplace<TypedSoulHitLock>(attacker,hit);++pendingHits;g_registry.emplace_or_replace<ecs::TypedItemBusy>(attacker);const auto owner=t->state.fence.owner;const auto session=t->state.fence.sessionGeneration;const auto token=login->token;const auto resultDamage=planned->damage;
    const bool queued=store::RequestCommit(owner,session,planned->commit,[owner,token,hit,held,resultDamage,finish=std::move(finish)](store::CommitOutcome result){const auto e=Current(owner,token);if(e!=entt::null){if(result.state)g_registry.emplace_or_replace<ecs::TypedItems>(e,std::move(*result.state));g_registry.remove<ecs::TypedItemBusy>(e);}if(result.write && e==hit->attacker && g_registry.valid(hit->victim)){hit->finishing=true;finish(resultDamage);}Unlock(hit);if(g_registry.valid(e)){SendTypedInventory(e);PumpTypedItemGrants(e);}});
    if(!queued){g_registry.remove<ecs::TypedItemBusy>(attacker);Unlock(hit);}return true;
}
void StopTypedSouls(entt::entity e){const auto* t=g_registry.try_get<ecs::TypedItems>(e);if(!t)return;for(auto it=clocks.lower_bound(Identity(t->state.fence,0));it!=clocks.end() && std::get<0>(it->first)==t->state.fence.owner && std::get<1>(it->first)==t->state.fence.sessionGeneration;++it){RunClock(*it->second,false);it->second->stopped=true;it->second->died=!Alive(e);}}
void TickTypedSouls(){if(!finishing){std::vector<entt::entity> owners;for(auto e:g_registry.view<ecs::TypedItems>())owners.push_back(e);for(auto e:owners)Register(e);}const auto c=store::CurrentCatalog();if(!c)return;std::vector<std::function<void()>> resume;
    for(auto it=clocks.begin();it!=clocks.end();){auto clock=it->second;const auto e=Current(*clock);if(e==entt::null){RunClock(*clock,false);clock->stopped=true;}if(clock->pending){++it;continue;}const auto* p=Core(*clock);const auto elapsed=Elapsed(*clock);if(!p || (clock->stopped && !elapsed && !(clock->died && p->soul->active))){for(auto& action:clock->waiters)resume.push_back(std::move(action));it=clocks.erase(it);continue;}++it;const auto& row=std::get<item::ItemSoulRow>(c->Find(p->key)->category);const auto& l=c->FindSoulProfile(*row.profile)->levels[p->refinementLevel];const bool died=clock->died || (!clock->stopped && !Alive(e));const bool due=(died && p->soul->active) || (elapsed && (clock->stopped || !clock->since || elapsed>=5000 || elapsed+p->soul->rechargeRemainderMs>=l.rechargeIntervalMs));if(!due || (!clock->stopped && g_registry.all_of<ecs::TypedItemBusy>(e)))continue;
        auto context=Context(e);context.alive=!died;const auto f=clock->state->fence;const auto planned=item::MakeSoulCommit(*c,Operation(),f,clock->state->storages,clock->state->items,p->id,{},item::SoulAction::Recharge,elapsed,0,context);if(!planned){if(clock->stopped){clock->accrued={};}continue;}if(!clock->stopped)g_registry.emplace_or_replace<ecs::TypedItemBusy>(e);
        if(!store::RequestCommit(f.owner,f.sessionGeneration,planned->commit,[clock](store::CommitOutcome result){const auto e=Current(*clock);if(e==entt::null || clock->stopped)return;if(result.state)g_registry.emplace_or_replace<ecs::TypedItems>(e,std::move(*result.state));g_registry.remove<ecs::TypedItemBusy>(e);SendTypedInventory(e);PumpTypedItemGrants(e);}))if(!clock->stopped)g_registry.remove<ecs::TypedItemBusy>(e);
    }for(auto& action:resume)action();
}
void FinishTypedSouls(){finishing=true;while(pendingHits){store::ItemStore::Instance().Drain();std::this_thread::sleep_for(std::chrono::milliseconds(1));}for(auto& [key,c]:clocks){const auto e=Current(*c);if(e!=entt::null)c->died=c->died || !Alive(e);RunClock(*c,false);c->stopped=true;c->waiters.clear();}while(!clocks.empty()){store::ItemStore::Instance().Drain();TickTypedSouls();if(!clocks.empty())std::this_thread::sleep_for(std::chrono::milliseconds(1));}finishing=false;}
namespace auriga::item::runtime {
bool DeferSoulLogin(std::uint32_t pid,std::function<void()> action){const auto e=CHARACTER_MANAGER::instance().FindEntityByPID(pid);if(const auto* lock=g_registry.valid(e)?g_registry.try_get<TypedSoulHitLock>(e):nullptr){lock->hit->waiters.push_back(std::move(action));return true;}for(auto& [key,c]:clocks)if(std::get<0>(key)==ItemOwner{ItemOwnerKind::Character,pid} && c->stopped){c->waiters.push_back(std::move(action));return true;}return false;}
std::function<void(MultiCommitOutcome*)> BeginSoulCommit(const std::vector<OwnerRead>& owners,const ItemCommit& commit){struct Mark{std::shared_ptr<SoulClock> clock;std::size_t owner;std::uint64_t spent;bool reset=false;};std::vector<Mark> marks;for(const auto& f:commit.owners){const auto owner=std::ranges::find(owners,f.owner,&OwnerRead::owner);if(owner==owners.end())continue;for(auto it=clocks.lower_bound(Identity(f,0));it!=clocks.end() && std::get<0>(it->first)==f.owner && std::get<1>(it->first)==f.sessionGeneration;++it){auto clock=it->second;bool reset=false;for(const auto& ch:commit.items)if(ch.after.id.Value()==clock->id && ch.before && ch.before->refinementLevel!=ch.after.refinementLevel)reset=true;clock->pending=true;marks.push_back({clock,static_cast<std::size_t>(owner-owners.begin()),SoulElapsed(commit,clock->id),reset});}}
    return [marks=std::move(marks)](MultiCommitOutcome* result){std::set<item::persistence::LoadedItemOwner*> updated;for(const auto& mark:marks){auto& c=*mark.clock;c.pending=false;if(!result)continue;if(result->write){const auto now=Time::now();const auto total=c.accrued+(c.since ? now-*c.since : Time::duration{});c.accrued=mark.reset ? Time::duration{} : std::max(Time::duration{},total-std::chrono::milliseconds(mark.spent));if(c.since)c.since=now;}if(result->states[mark.owner]){if(updated.insert(c.state.get()).second)*c.state=*result->states[mark.owner];}else{c.stopped=true;c.state->items.clear();}}};
}
}
