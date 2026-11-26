#include "stdafx.h"
#include "TypedDungeonEntry.hpp"
#include "ItemStore.hpp"
#include "ItemCatalogRuntime.hpp"
#include "TypedInventorySync.hpp"
#include "TypedDrops.hpp"
#include "TypedRecovery.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/ResourceAdmission.hpp"
#include "../ecs/components/quest_components.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PlayerItemActions.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../ecs/systems/SessionSystem.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../quest/questpc.h"
#include "../social/party.h"
#include "../entity/char_manager.h"
#include "../network/desc.h"
#include "../dungeon/dungeon.h"
#include <DungeonEntrySql.hpp>
#include <item/DungeonItems.hpp>
#include <item/DungeonEntryBindings.hpp>
#include <openssl/rand.h>
#include <Core/Logging.hpp>
namespace {
namespace it=auriga::item;namespace rt=it::runtime;namespace P=ecs::PlayerRuntime;
struct Actor {std::uint32_t pid=0;std::uint64_t token=0;int map=0;};
struct Job {std::vector<Actor> actors;entt::entity party=entt::null;std::uint32_t leader=0;it::DungeonEntryMutation reservation;std::optional<TypedDungeonEntryPlan> plan;std::function<bool(entt::entity)> eligible;std::function<std::optional<TypedDungeonEntryPlan>()> preflight;std::shared_ptr<auriga::resource_admission::Permit> held;std::vector<std::function<void()>> waiters;TypedDungeonQuestEntry quest;};
std::map<std::uint32_t,std::shared_ptr<Job>> jobs;
std::array<std::uint8_t,16> Op(){std::array<std::uint8_t,16> op{};if(RAND_bytes(op.data(),op.size())!=1)op={};return op;}
entt::entity Current(const Actor& a){const auto e=CHARACTER_MANAGER::instance().FindEntityByPID(a.pid);const auto* l=g_registry.valid(e)?g_registry.try_get<ecs::TypedItemLogin>(e):nullptr;return l && l->token==a.token?e:entt::null;}
void Status(entt::entity e,const char* suffix){if(g_registry.valid(e)){const auto command=it::DungeonEntryBusyCommand(suffix[0]=='1');ecs::ChatSystem::Send(e,CHAT_TYPE_COMMAND,"%s",command.c_str());}}
bool Live(entt::entity e){auto* d=g_registry.valid(e)?P::GetDesc(e):nullptr;return d && d->IsPhase(PHASE_GAME) && P::IsPC(e) && !CombatSystem::IsDead(e) && !CombatSystem::IsStun(e) && !P::IsObserverMode(e) && !P::IsWarping(e) && g_registry.all_of<ecs::TypedItems,ecs::TypedItemLogin>(e);}
bool Admitted(entt::entity e, lua_State* coroutine = nullptr)
{
 if(!Live(e) || !P::CanWarp(e) || !PlayerItemActions::CanHandleItems(e) || ecs::SocialSystem::HasExchange(e) ||
    ecs::SessionSystem::IsSafeboxOpen(e) || ecs::SessionSystem::IsCubeOpen(e))return false;
 const auto* q=g_registry.try_get<ecs::QuestPCState>(e);
 if(!q || !q->pc)return false;
 if(!coroutine)return !q->pc->IsRunning();
 const auto* state=q->pc->GetRunningQuestState();
 return state && state->co==coroutine;
}
std::vector<entt::entity> Participants(entt::entity leader){std::vector<entt::entity> out;const auto party=ecs::SocialSystem::GetParty(leader);if(party==entt::null){out.push_back(leader);return out;}auto collect=[&](entt::entity e){if(P::IsPC(e))out.push_back(e);};PartySystem::ForEachOnMapMember(party,collect,P::GetMapIndex(leader));std::ranges::sort(out,{},[](auto e){return P::GetPlayerID(e);});return out;}
bool StillEligible(const std::shared_ptr<Job>& job)
{
 const auto lead=std::ranges::find(job->actors,job->leader,&Actor::pid);
 if(lead==job->actors.end())return false;
 const auto leader=Current(*lead);
 if(!Admitted(leader,job->quest.coroutine) || ecs::SocialSystem::GetParty(leader)!=job->party ||
    (job->party!=entt::null && PartySystem::GetLeaderPID(job->party)!=job->leader))return false;
 const auto group=Participants(leader);
 if(group.size()!=job->actors.size())return false;
 for(const auto& actor:job->actors)
 {
  const auto e=Current(actor);
  if(e==entt::null || P::GetMapIndex(e)!=actor.map || !Admitted(e,e==leader?job->quest.coroutine:nullptr) ||
     !job->eligible(e) || std::ranges::find(group,e)==group.end())return false;
 }
 return true;
}
void ApplyStates(const std::shared_ptr<Job>& job,const rt::MultiCommitOutcome& out){for(const auto& row:out.states)if(row)for(const auto& a:job->actors)if(a.pid==row->fence.owner.id){const auto e=Current(a);const auto* old=e!=entt::null?g_registry.try_get<ecs::TypedItems>(e):nullptr;if(old && old->state.fence.sessionGeneration==row->fence.sessionGeneration && old->state.fence.revision<=row->fence.revision)g_registry.emplace_or_replace<ecs::TypedItems>(e,*row);break;}}
void ReleaseBusy(const std::shared_ptr<Job>& job){for(const auto& a:job->actors)if(const auto e=Current(a);e!=entt::null)g_registry.remove<ecs::TypedItemBusy>(e);job->held.reset();}
void CompleteQuest(const std::shared_ptr<Job>& job,bool entered)
{
 auto done=std::move(job->quest.completed);
 const auto lead=std::ranges::find(job->actors,job->leader,&Actor::pid);
 if(done && lead!=job->actors.end() && Current(*lead)!=entt::null)done(entered);
}
void Finish(const std::shared_ptr<Job>& job)
{
 for(const auto& actor:job->actors)
 {
  const auto found=jobs.find(actor.pid);
  if(found!=jobs.end() && found->second==job)jobs.erase(found);
  if(const auto e=Current(actor);e!=entt::null)
  {
   for(const auto& binding:it::DungeonEntryBindings)if(binding.gate==job->reservation.gate)
   {
    const auto count=TypedDungeonTicketCount(e,binding.ticketCode)+TypedDungeonTicketCount(e,binding.alternateTicketCode);
    const auto command=it::DungeonEntryCountCommand(binding.map,count);
    ecs::ChatSystem::Send(e,CHAT_TYPE_COMMAND,"%s",command.c_str());
    break;
   }
   Status(e,"0");SendTypedInventory(e);PumpTypedItemGrants(e);
  }
 }
 CompleteQuest(job,false);
 auto waiters=std::move(job->waiters);for(auto& done:waiters)done();
}
void Release(const std::shared_ptr<Job>& job){
 if(job->plan && job->plan->cancel){job->plan->cancel();job->plan.reset();}
 const bool queued=rt::ItemStore::Instance().Submit([job](CAsyncSQL& sql,const it::ItemCatalog& c)->rt::ItemStore::Completion{
  bool settled=true;std::vector<std::expected<it::persistence::LoadedItemOwner,it::ItemPersistenceError>> states;
  for(const auto& a:job->actors){const it::ItemOwner owner{it::ItemOwnerKind::Character,a.pid};auto pending=it::persistence::LoadPendingDungeonEntries(sql,c,a.pid);auto state=it::persistence::LoadCurrentItemOwner(sql,c,owner);if(!pending || !state){settled=false;states.push_back(std::move(state));continue;}
   for(const auto& m:*pending)if(m.reservation==job->reservation.reservation){const std::array owners{state->fence};auto commit=it::MakeDungeonEntryResolution(c,Op(),m,false,owners,{},{});if(!commit){settled=false;break;}auto written=it::persistence::CommitItems(sql,c,*commit);if(!written && written.error()==it::ItemPersistenceError::CommitUnknown){const auto receipt=it::persistence::FindItemReceipt(sql,c,*commit);if(receipt && *receipt)written=it::persistence::ItemWriteResult{**receipt,true};}if(!written)settled=false;state=it::persistence::LoadCurrentItemOwner(sql,c,owner);if(!state)settled=false;}
   states.push_back(std::move(state));
  }
  return [job,settled,states=std::move(states)](){rt::MultiCommitOutcome result{std::unexpected(it::ItemPersistenceError::Conflict),states};ApplyStates(job,result);if(!settled){LOG_ERROR("TypedDungeonEntry: reservation release retry for pid {}",job->leader);rt::ItemStore::Instance().Submit([job](CAsyncSQL&,const it::ItemCatalog&)->rt::ItemStore::Completion{return [job]{Release(job);};},std::chrono::seconds(1));return;}ReleaseBusy(job);Finish(job);};
 });if(!queued){ReleaseBusy(job);Finish(job);}
}
bool Debit(const std::shared_ptr<Job>& job){
 const auto c=rt::CurrentCatalog();if(!c || !job->held || !job->held->Current() || !StillEligible(job))return false;
 std::vector<it::ItemOwnerFence> owners;std::vector<it::ItemStorageLayout> layouts;std::vector<it::PersistentItem> items;std::vector<rt::OwnerRead> reads;
 for(const auto& a:job->actors){const auto e=Current(a);if(e==entt::null)return false;const auto& s=g_registry.get<ecs::TypedItems>(e).state;owners.push_back(s.fence);reads.push_back({s.fence.owner,s.fence.sessionGeneration});layouts.insert(layouts.end(),s.storages.begin(),s.storages.end());items.insert(items.end(),s.items.begin(),s.items.end());}
 const auto commit=it::MakeDungeonEntryResolution(*c,Op(),job->reservation,true,owners,layouts,items);if(!commit)return false;
 return rt::RequestCommitMany(std::move(reads),*commit,[job](rt::MultiCommitOutcome out){ApplyStates(job,out);if(!out.write){Release(job);return;}const bool publish=job->held && job->held->Current() && job->plan && job->plan->publish && g_registry.valid(job->plan->dungeon) && StillEligible(job);ReleaseBusy(job);if(publish){CompleteQuest(job,true);job->plan->publish();job->plan.reset();}else{if(job->plan && job->plan->cancel)job->plan->cancel();job->plan.reset();LOG_ERROR("TypedDungeonEntry: confirmed debit could not publish after session/group changed for pid {}",job->leader);}Finish(job);});
}
}
bool HasTypedDungeonInventory(entt::entity e){return g_registry.valid(e) && g_registry.all_of<ecs::TypedItems>(e);}
void CancelTypedDungeonPreflight(entt::entity e){if(DungeonSystem::IsValid(e) && !DungeonSystem::HasLivePlayers(e))CDungeonManager::instance().Destroy(DungeonSystem::GetId(e));}
bool TypedDungeonEntryPending(entt::entity e){return g_registry.valid(e) && jobs.contains(P::GetPlayerID(e));}
std::uint64_t TypedDungeonTicketCount(entt::entity e,std::string_view code){if(code.empty())return 0;const auto c=rt::CurrentCatalog();const auto* t=g_registry.valid(e)?g_registry.try_get<ecs::TypedItems>(e):nullptr;const auto key=c?it::DungeonItemKey(*c,code):std::nullopt;return t && key?it::CountMissionItems(t->state.items,t->state.fence.owner,*key,0):0;}
bool BeginTypedDungeonEntry(entt::entity leader,std::string_view gate,std::string_view code,std::uint32_t units,std::function<bool(entt::entity)> eligible,std::function<std::optional<TypedDungeonEntryPlan>()> preflight,TypedDungeonQuestEntry quest){
 if(!Admitted(leader,quest.coroutine) || TypedDungeonEntryPending(leader))return false;auto group=Participants(leader);const auto party=ecs::SocialSystem::GetParty(leader);if(group.empty() || group.size()>8 || party!=entt::null && PartySystem::GetLeaderPID(party)!=P::GetPlayerID(leader))return false;const auto c=rt::CurrentCatalog();const auto key=c?it::DungeonItemKey(*c,code):std::nullopt;if(!c || !key)return false;
 // The original quest gates briefly serialize entry on each channel.
 if(quest.coroutine && std::ranges::any_of(jobs,[&](const auto& entry){return entry.second->reservation.gate==gate;}))return false;
 std::vector keys{*key};
 if(!quest.alternateTicket.empty())
 {
  const auto alternate=it::DungeonItemKey(*c,quest.alternateTicket);
  if(!alternate)return false;
  keys.insert(keys.begin(),*alternate);
 }
 auto job=std::make_shared<Job>();job->leader=P::GetPlayerID(leader);job->party=party;job->eligible=std::move(eligible);job->preflight=std::move(preflight);std::vector<it::ItemOwnerFence> owners;std::vector<it::PersistentItem> items;std::vector<rt::OwnerRead> reads;
 job->quest=std::move(quest);
 for(const auto e:group){if(!Admitted(e,e==leader?job->quest.coroutine:nullptr) || !job->eligible(e) || !HasTypedDungeonInventory(e) || g_registry.all_of<ecs::TypedItemBusy>(e) || TypedDungeonEntryPending(e))return false;const auto& state=g_registry.get<ecs::TypedItems>(e).state;job->actors.push_back({P::GetPlayerID(e),g_registry.get<ecs::TypedItemLogin>(e).token,static_cast<int>(P::GetMapIndex(e))});owners.push_back(state.fence);reads.push_back({state.fence.owner,state.fence.sessionGeneration});items.insert(items.end(),state.items.begin(),state.items.end());}
 const auto commit=it::MakeDungeonEntryReservation(*c,Op(),gate,keys,0,units,TypedItemEffectNow(),owners,items);if(!commit)return false;job->reservation=*commit->dungeonEntry;
 // Enter accepts at most8 actors. Build the dynamic array through the common
 // native admission API, retaining no ECS component address across SQL.
 std::array<auriga::resource_admission::Actor*,8> actors{};for(unsigned n=0;n<group.size();++n){if(!ecs::resource_admission::EnsureActor(group[n]))return false;}
 if(!StillEligible(job))return false;for(unsigned n=0;n<group.size();++n){auto* a=g_registry.try_get<ecs::NativeResourceAdmission>(group[n]);if(!a || !a->actor.RecoverLegacy(entt::to_integral(group[n])))return false;actors[n]=&a->actor;}
 auto permission=auriga::resource_admission::Enter(std::span(actors).first(group.size()),auriga::resource_admission::Operation::ItemUse);if(!permission || !permission->Current())return false;job->held=std::make_shared<auriga::resource_admission::Permit>(std::move(*permission));
 for(const auto& a:job->actors)jobs[a.pid]=job;
 for(const auto& a:job->actors){const auto e=Current(a);if(e==entt::null){ReleaseBusy(job);Finish(job);return false;}g_registry.emplace_or_replace<ecs::TypedItemBusy>(e);if(Current(a)!=e){ReleaseBusy(job);Finish(job);return false;}Status(e,"1");}
 if(!job->held->Current() || !rt::RequestCommitMany(std::move(reads),*commit,[job](rt::MultiCommitOutcome out){ApplyStates(job,out);if(!out.write || !job->held || !job->held->Current() || !StillEligible(job)){Release(job);return;}job->plan=job->preflight();if(!job->plan || !job->plan->publish || !g_registry.valid(job->plan->dungeon) || !Debit(job))Release(job);})){Release(job);return false;}return true;
}
namespace auriga::item::runtime {bool DeferDungeonEntryLogin(std::uint32_t pid,std::function<void()> done){const auto found=jobs.find(pid);if(found==jobs.end())return false;found->second->waiters.push_back(std::move(done));return true;}}
