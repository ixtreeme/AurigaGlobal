#include "stdafx.h"
#include "TypedMissionRuntime.hpp"
#include "TypedMissionWire.hpp"
#include "../item/ItemStore.hpp"
#include "../item/ItemCatalogRuntime.hpp"
#include "../item/TypedInventorySync.hpp"
#include "../item/TypedDrops.hpp"
#include "../item/TypedBooks.hpp"
#include "../core/config.h"
#include "../core/constants.h"
#include "../ecs/systems/SkillSystem.hpp"
#include <StoragePersistenceSql.hpp>
#include "../ecs/ResourceAdmission.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/InventorySystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/SessionSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../ecs/components/quest_components.hpp"
#include "../ecs/components/status_components.hpp"
#include "../quest/questpc.h"
#include "../entity/char_manager.h"
#include "../network/desc.h"
#include "../core/GameConfig.h"
#include <SQL/AsyncSQL.h>
#include <Core/Logging.hpp>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <charconv>

namespace {
namespace m=auriga::mission;namespace mp=m::persistence;namespace it=auriga::item;namespace store=it::runtime;
using Status=m::TypedMissionStatus;using Message=std::unique_ptr<SQLMsg>;
struct Admission {uint32_t version=0,npc=0;uint16_t level=0;};
std::shared_ptr<const m::MissionPackage> package;std::vector<uint8_t> packageBytes;std::map<std::string,Admission> admissions;GameSqlConfig connection;
#pragma pack(push,1)
struct Request {uint8_t header=202,action=1;uint32_t npc=0;uint64_t nonce=0,request=0;char mission[97]{},choice[97]{};};
#pragma pack(pop)
static_assert(sizeof(Request)==216);
struct Event {uint64_t durable=0; m::EventKind kind;std::string subject,choice;uint64_t amount=1;m::Timestamp time=0;};
struct Session {
 uint32_t pid=0,accountId=0;uint64_t login=0,native=0,nonce=0,last=0,revision=0;uint32_t npc=0;bool loaded=false,pending=false,polling=false,visible=false,replyAfterPoll=false;
 std::function<void(bool,bool)> releaseProgress;std::optional<it::persistence::LoadedItemOwner> account;
 std::shared_ptr<auriga::resource_admission::Permit> permit;Status status=Status::Loading;Request previous{};mp::Owner owner;std::shared_ptr<m::DefinitionRegistry> registry;m::MissionRuntime::Checkpoint checkpoint;std::deque<Event> events;int64_t nextPoll=0,nextLoad=0,nextDeadline=0;
};
struct Reference {std::shared_ptr<Session> value;};
int64_t Now(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();}
entt::entity Current(const std::shared_ptr<Session>& s){const auto e=CHARACTER_MANAGER::instance().FindEntityByPID(s->pid);const auto* login=g_registry.valid(e)?g_registry.try_get<ecs::TypedItemLogin>(e):nullptr;const auto* ref=g_registry.valid(e)?g_registry.try_get<Reference>(e):nullptr;return login && login->token==s->login && ref && ref->value==s?e:entt::null;}
std::shared_ptr<Session> Get(entt::entity e){const auto* ref=g_registry.valid(e)?g_registry.try_get<Reference>(e):nullptr;return ref?ref->value:nullptr;}
bool Admitted(entt::entity e,bool quest=false){if(!g_registry.valid(e) || !ecs::PlayerRuntime::IsPC(e) || CombatSystem::IsDead(e) || ecs::PlayerRuntime::IsObserverMode(e) || !InventorySystem::CanHandleItems(e) || !g_registry.all_of<ecs::TypedItems,ecs::TypedItemLogin>(e) || ecs::SessionSystem::IsSafeboxOpen(e) || ecs::SessionSystem::IsCubeOpen(e) || ecs::SocialSystem::HasExchange(e) || ecs::SocialSystem::GetMyShop(e)!=entt::null || ecs::SocialSystem::GetShopOwner(e)!=entt::null)return false;const auto* flags=g_registry.try_get<ecs::StatusFlags>(e);if(flags && flags->isStunned)return false;auto* d=ecs::PlayerRuntime::GetDesc(e);const auto* q=g_registry.try_get<ecs::QuestPCState>(e);return d && d->IsPhase(PHASE_GAME) && q && q->pc && (quest || !q->pc->IsRunning());}
bool Near(entt::entity e,uint32_t vid,uint32_t race){const auto npc=CHARACTER_MANAGER::instance().FindEntity(vid);if(!g_registry.valid(npc) || ecs::PlayerRuntime::IsPC(npc) || ecs::PlayerRuntime::GetRaceNum(npc)!=race || ecs::PlayerRuntime::GetMapIndex(e)!=ecs::PlayerRuntime::GetMapIndex(npc))return false;const auto dx=std::abs(int64_t(ecs::PlayerRuntime::GetX(e))-ecs::PlayerRuntime::GetX(npc)),dy=std::abs(int64_t(ecs::PlayerRuntime::GetY(e))-ecs::PlayerRuntime::GetY(npc));return std::max(dx,dy)+std::min(dx,dy)/2<1000;}
std::unique_ptr<m::MissionRuntime> Runtime(const std::shared_ptr<Session>& s){auto r=std::make_unique<m::MissionRuntime>(*s->registry,m::CharacterSession{s->pid,s->login},s->checkpoint.now);if(r->Restore(s->checkpoint,s->checkpoint.now).code!=m::ResultCode::Applied)throw std::runtime_error("Mission checkpoint restore failed");return r;}
const m::MissionRuntime::SavedInstance* Instance(const Session& s,const std::string& code){const m::MissionRuntime::SavedInstance* out=nullptr;for(const auto& [id,v]:s.checkpoint.instances)if(v.mission==code)out=&v;return out;}
void Reply(const std::shared_ptr<Session>& s,Status status,std::string message={}){
 const auto e=Current(s);if(e==entt::null || !s->visible)return;const auto catalog=store::CurrentCatalog();if(!catalog)return;std::vector<m::TypedMissionView> views;
 if(s->loaded)for(const auto& [code,a]:admissions){m::TypedMissionView v;v.code=code;v.version=a.version;v.npcRace=a.npc;v.minimumLevel=a.level;const auto* saved=Instance(*s,code);const auto graph=s->registry->Get(code,saved?saved->version:a.version);if(!graph)continue;v.version=graph->Source().version;v.repeatable=graph->Source().repeatable;
   std::set<std::string> allowed;if(saved){v.instance=saved->id;v.state=saved->status==m::MissionStatus::Completed?3:saved->status==m::MissionStatus::Failed?4:saved->status==m::MissionStatus::Abandoned?5:1;for(unsigned j=0;j<saved->nodes.size();j++)if(saved->nodes[j].status!=m::NodeStatus::Dormant)allowed.insert(graph->Nodes()[j].value.id);}
   auto presentation=m::ProjectPresentation(*package,code,package->Manifest().defaultLocale,allowed);v.title=presentation?presentation->title:code;v.summary=presentation?presentation->summary:"";
   if(saved)for(unsigned j=0;j<saved->nodes.size();j++){const auto& p=saved->nodes[j];const auto& n=graph->Nodes()[j].value;if(p.status==m::NodeStatus::Dormant || (n.kind!=m::NodeKind::Objective && n.kind!=m::NodeKind::Timer && n.kind!=m::NodeKind::Reward))continue;
     m::TypedMissionNodeView node;node.id=n.id;node.title=n.id;node.subject=n.objective.subject;node.kind=uint8_t(n.kind);node.state=uint8_t(p.status);node.event=uint8_t(n.objective.event);node.counter=p.counter;node.goal=n.objective.goal;node.deadline=p.deadline;
     if(presentation)for(const auto& text:presentation->objectives)if(text.node==n.id)node.title=text.title;
     if(p.status==m::NodeStatus::Active && n.kind==m::NodeKind::Objective && n.objective.event==m::EventKind::DialogueChoiceCommitted)node.choices=graph->Source().choices.at(n.objective.subject);
     if(p.status==m::NodeStatus::Active && n.kind==m::NodeKind::Reward && !v.reward){const auto pin=m::PinReward({s->pid,code,saved->version,saved->id,n.id,n.reward},m::FingerprintDefinition(*graph),*graph);if(pin){auto reward=m::TypedItemContract(*catalog,*pin.reward);if(reward){v.reward=std::move(*reward);v.experience=pin.reward->Payload().experience;v.levels=pin.reward->Payload().levels;v.accountCoins=pin.reward->Payload().accountCoins;v.state=2;}}}v.nodes.push_back(std::move(node));
   }views.push_back(std::move(v));
 }
 const auto* items=g_registry.try_get<ecs::TypedItems>(e);const auto character=TypedBookCharacter(e);uint64_t coins=0;if(s->account){const auto w=std::ranges::find(s->account->wallets,it::WalletCurrency::DragonCoin,&it::ItemWallet::currency);if(w!=s->account->wallets.end())coins=w->balance;}const auto wire=m::EncodeTypedMissions(*catalog,status,s->nonce,s->last,s->npc,views,items?&items->state:nullptr,message,character?&*character:nullptr,coins);SendTypedInventory(e);if(auto* d=ecs::PlayerRuntime::GetDesc(e);d && d->IsPhase(PHASE_GAME) && wire)d->LargePacket(wire->data(),int(wire->size()));
}
bool Connect(CAsyncSQL& db){db.SetTimeouts(5);db.SetRetryConnectionErrors(false);return db.Setup(connection.host.c_str(),connection.user.c_str(),connection.password.c_str(),connection.database.c_str(),"utf8mb4",true,connection.port);}
Message Query(CAsyncSQL& db,const mp::ReadCommand& q){return Message(db.DirectQueryPrepared(q.query.c_str(),q.params));}
bool Run(CAsyncSQL& db,const mp::Plan& p){if(!p)return false;Message msg(db.DirectTransactionPrepared(p.statements));return msg && mp::Confirmed(*msg);}
struct Attempt {std::shared_ptr<Session> session;mp::Plan plan;bool resolveOnly=false,reload=false;std::function<void(bool,std::optional<it::persistence::LoadedItemOwner>)> done;};
void QueueAttempt(const std::shared_ptr<Attempt>& a,std::chrono::milliseconds delay={}){
 store::ItemStore::Instance().Submit([a](CAsyncSQL&,const it::ItemCatalog& catalog)->store::ItemStore::Completion{
   CAsyncSQL db;std::optional<bool> outcome;std::optional<it::persistence::LoadedItemOwner> state,account;
   if(Connect(db)){
     Message resolved(db.DirectTransactionPrepared(mp::Resolve(a->plan)));const auto resolution=resolved?mp::DecodeResolution(*resolved,a->plan).status:mp::Resolution::SqlError;
     if(resolution==mp::Resolution::Committed)outcome=true;
     else if(resolution==mp::Resolution::Absent){Message msg(db.DirectTransactionPrepared(a->plan.statements));if(msg && mp::Confirmed(*msg))outcome=true;else if(msg && msg->transactionOutcome!=SqlTransactionOutcome::CommitUnknown)outcome=false;}
     else if(resolution!=mp::Resolution::SqlError)outcome=false;
     if(outcome && a->reload){auto loaded=it::persistence::LoadItemOwner(db,catalog,{it::ItemOwnerKind::Character,a->session->pid},a->session->native);if(loaded)state=std::move(*loaded);else if(*outcome)outcome.reset();if(a->session->accountId){auto loadedAccount=it::persistence::LoadCurrentItemOwner(db,catalog,{it::ItemOwnerKind::Account,a->session->accountId});if(loadedAccount)account=std::move(*loadedAccount);else outcome.reset();}}
   }
   return [a,outcome,state=std::move(state),account=std::move(account)]()mutable{if(!outcome){QueueAttempt(a,std::chrono::milliseconds(250));return;}if(Current(a->session)==entt::null){if(a->session->releaseProgress){auto finish=std::move(a->session->releaseProgress);finish(false,false);}return;}if(account)a->session->account=std::move(account);a->done(*outcome,std::move(state));};
 },delay);
}
void Persist(const std::shared_ptr<Session>& s,mp::Plan plan,m::MissionRuntime::Checkpoint after,size_t events=0,bool reload=false,std::function<void(bool)> done={},bool command=false){
 s->pending=true;auto a=std::make_shared<Attempt>();a->session=s;a->plan=std::move(plan);a->reload=reload;const auto revision=*a->plan.storageRevision;
 a->done=[s,after=std::move(after),events,revision,reload,command,done=std::move(done)](bool ok,std::optional<it::persistence::LoadedItemOwner> state)mutable{const auto e=Current(s);if(e==entt::null)return;s->pending=false;
   if(ok){s->checkpoint=std::move(after);s->nextDeadline=0;s->revision=revision;for(size_t j=0;j<events && !s->events.empty();j++)s->events.pop_front();}if(state)g_registry.emplace_or_replace<ecs::TypedItems>(e,std::move(*state));if(reload){g_registry.remove<ecs::TypedItemBusy>(e);s->permit.reset();if(s->releaseProgress){auto finish=std::move(s->releaseProgress);finish(ok,true);}if(!s->visible)SendTypedInventory(e);}
   if(command)s->status=ok?Status::Applied:Status::Failed;Reply(s,command?s->status:ok?Status::Ready:Status::Failed,ok?"":"A mentés nem történt meg; a művelet újra próbálható.");if(done)done(ok);PumpTypedItemGrants(e);
 };QueueAttempt(a);
}
void Load(const std::shared_ptr<Session>& s){
 s->pending=true;s->status=Status::Loading;const auto fence=g_registry.get<ecs::TypedItems>(Current(s)).state.fence;
 store::ItemStore::Instance().Submit([s,fence](CAsyncSQL&,const it::ItemCatalog& catalog)->store::ItemStore::Completion{
   auto registry=std::make_shared<m::DefinitionRegistry>();std::vector<m::Definition> defs;for(const auto& graph:package->Graphs())defs.push_back(graph->Source());registry->Activate(defs);bool ok=false;std::optional<it::persistence::LoadedItemOwner> account;mp::Owner owner;uint64_t revision=0;m::MissionRuntime::Checkpoint checkpoint;CAsyncSQL db;
   if(Connect(db) && s->accountId && it::persistence::EnsureRuntimeStorageOwner(db,catalog,{it::ItemOwnerKind::Account,s->accountId}) && Run(db,mp::ArchivePackage(packageBytes))){auto loadedAccount=it::persistence::LoadCurrentItemOwner(db,catalog,{it::ItemOwnerKind::Account,s->accountId});if(loadedAccount)account=std::move(*loadedAccount);auto headMessage=Query(db,mp::ReadHead(s->pid));const auto head=headMessage?mp::DecodeHead(*headMessage,s->pid):mp::HeadResult{};
     if(head.status==mp::ReadStatus::Found || head.status==mp::ReadStatus::Missing){owner={s->pid,head.head.epoch+1,mp::NewToken()};auto claim=mp::Claim(s->pid,head.head.epoch,owner.token,mp::NewToken());m::FenceTypedMissionPlan(claim,fence);
       if(Run(db,claim)){auto msg=Query(db,mp::ReadSnapshot(owner));const auto loaded=msg?mp::DecodeSnapshot(*msg,owner):mp::SnapshotResult{};
         if(loaded.status==mp::ReadStatus::Found){revision=loaded.snapshot.storageRevision;ok=true;for(const auto& ref:loaded.snapshot.metadata.definitions)if(!registry->Get(ref.mission,ref.version)){auto archived=Query(db,mp::ReadDefinitionArchive(ref));const auto old=archived?mp::DecodeDefinitionArchive(*archived,ref):m::PackageLoad{};if(!old){ok=false;break;}std::vector<m::Definition> exact;for(const auto& graph:old.package->Graphs())exact.push_back(graph->Source());if(registry->Retain(exact).status!=m::Publication::Status::Published){ok=false;break;}}
           if(ok && !loaded.snapshot.bytes.empty()){const auto restored=m::DecodeCheckpoint(loaded.snapshot.bytes,*registry);ok=bool(restored);if(ok)checkpoint=*restored.checkpoint;}
           else if(ok){m::MissionRuntime runtime(*registry,{s->pid,s->login},Now());checkpoint=runtime.Save();}
         }
       }
     }
   }
   ok=ok && account.has_value();
   return [s,ok,owner,revision,registry,account=std::move(account),checkpoint=std::move(checkpoint)]()mutable{if(Current(s)==entt::null)return;s->pending=false;s->loaded=ok;s->nextLoad=Now()+5000;if(ok){s->account=std::move(account);s->owner=owner;s->revision=revision;s->registry=std::move(registry);s->checkpoint=std::move(checkpoint);s->nextDeadline=0;s->status=Status::Ready;}else{s->status=Status::Failed;LOG_ERROR("TypedMission: durable load failed for pid {}",s->pid);}Reply(s,s->status);};
 });
}
void Poll(const std::shared_ptr<Session>& s){
 s->polling=true;store::ItemStore::Instance().Submit([s](CAsyncSQL& db,const it::ItemCatalog&)->store::ItemStore::Completion{
   Message msg(db.DirectQueryPrepared("SELECT e.event_id,e.event_kind,e.category,e.local_id,e.refinement_level,e.units,CAST(UNIX_TIMESTAMP(e.created_at)*1000 AS UNSIGNED) FROM player.item_mission_event e JOIN player.item_owner_fence f ON f.owner_kind=1 AND f.owner_id=e.character_id WHERE e.character_id=? AND f.session_generation=? AND e.delivered=0 ORDER BY e.event_id LIMIT 32",{SqlParam::UInt(s->pid),SqlParam::UInt(s->native)}));std::vector<Event> events;
   if(msg && !msg->uiSQLErrno && msg->Get())while(const auto row=msg->Get()->FetchRow()){Event e;e.durable=std::stoull(row[0]);e.kind=std::stoi(row[1])==1?m::EventKind::ItemCollected:m::EventKind::ItemCrafted;e.subject="item."+std::string(row[2])+"."+row[3]+"."+row[4];e.amount=std::stoull(row[5]);e.time=std::stoll(row[6]);events.push_back(std::move(e));}
   return [s,events=std::move(events)]()mutable{if(Current(s)==entt::null)return;s->polling=false;s->nextPoll=Now()+500;for(auto& e:events)s->events.push_back(std::move(e));if(s->events.empty() && s->replyAfterPoll){s->replyAfterPoll=false;Reply(s,Status::Ready);}};
 });
}
bool Act(const std::shared_ptr<Session>& s,const std::string& code,unsigned action,const std::string& choice,bool quest,std::function<void(bool)> done={}){
 const auto e=Current(s);const auto admission=admissions.find(code);if(e==entt::null || !s->loaded || s->pending || !s->events.empty() || admission==admissions.end() || !Admitted(e,quest) || g_registry.all_of<ecs::TypedItemBusy>(e))return false;
 const auto& allow=admission->second;if(action!=5 && (!Near(e,s->npc,allow.npc) || ecs::PointSystem::GetLevel(e)<allow.level))return false;
 auto runtime=Runtime(s);const auto now=std::max(Now(),s->checkpoint.now);m::OperationResult result;
 if(action==2){const uint64_t id=s->checkpoint.instances.empty()?1:s->checkpoint.instances.rbegin()->first+1;result=runtime->Start(code,allow.version,id,now);}
 else if(action==3){auto pending=runtime->PendingActions();const auto found=std::ranges::find(pending,code,&m::ActionKey::mission);if(found==pending.end())return false;
   auto permit=ecs::resource_admission::Enter({e},auriga::resource_admission::Operation::ItemUse);if(!permit || !permit->Current())return false;s->permit=std::make_shared<auriga::resource_admission::Permit>(std::move(*permit));
   m::MissionRewardFacts facts;facts.accountId=s->accountId;
   const auto graph=s->registry->Get(found->mission,found->version);const auto pin=m::PinReward(*found,m::FingerprintDefinition(*graph),*graph);
   if(pin.reward->Payload().schema==3){const auto character=TypedBookCharacter(e);if(!character){s->permit.reset();return false;}facts.character=*character;s->releaseProgress=HoldTypedBookCheckpoint(e);if(!s->releaseProgress){s->permit.reset();return false;}
     facts.rules.maximumLevel=uint16_t(gPlayerMaxLevel);facts.rules.statLevelLimit=uint16_t(g_iStatusPointGetLevelLimit);facts.rules.skillGroup=SkillSystem::GetSkillGroup(e)!=0;
#ifdef ENABLE_STATUS_MAX_344_POINTS
     facts.rules.cappedStats344=true;
#endif
     facts.rules.nextExperience.assign(exp_table,exp_table+std::min<int>(gPlayerMaxLevel,PLAYER_MAX_LEVEL_CONST)+1);const auto& job=JobInitialPoints[ecs::PlayerRuntime::GetJob(e)];
     for(unsigned j=ecs::PointSystem::GetLevel(e);j<unsigned(gPlayerMaxLevel);j++)facts.rules.gains.emplace_back(uint16_t(number(job.hp_per_lv_begin,job.hp_per_lv_end)),uint16_t(number(job.sp_per_lv_begin,job.sp_per_lv_end)));
   }
   s->pending=true;g_registry.emplace_or_replace<ecs::TypedItemBusy>(e);const auto level=uint16_t(ecs::PointSystem::GetLevel(e));const auto ids=store::PeekInstanceIds(it::MaxItemCommitItems);store::SpendInstanceIds(ids.size());const auto key=*found;
   store::ItemStore::Instance().Submit([s,key,level,ids,facts=std::move(facts),done=std::move(done),now](CAsyncSQL&,const it::ItemCatalog& catalog)mutable->store::ItemStore::Completion{CAsyncSQL db;std::expected<m::TypedMissionWrite,std::string> plan=std::unexpected("Mission SQL unavailable");if(Connect(db)){auto state=it::persistence::LoadItemOwner(db,catalog,{it::ItemOwnerKind::Character,s->pid},s->native);if(facts.accountId){auto account=it::persistence::LoadCurrentItemOwner(db,catalog,{it::ItemOwnerKind::Account,facts.accountId});if(account)facts.account=std::move(*account);}if(state)plan=m::PrepareTypedMissionWrite(catalog,*s->registry,s->checkpoint,s->owner,s->revision,key,*state,level,ids,now,&facts);}
     return [s,plan=std::move(plan),done=std::move(done)]()mutable{const auto e=Current(s);if(e==entt::null){if(s->releaseProgress){auto finish=std::move(s->releaseProgress);finish(false,false);}return;}if(!plan){s->pending=false;g_registry.remove<ecs::TypedItemBusy>(e);s->permit.reset();if(s->releaseProgress){auto finish=std::move(s->releaseProgress);finish(false,true);}s->status=Status::Invalid;Reply(s,s->status,"A jutalom feltételei, a szintkorlát vagy a pénztárca korlátja nem engedi az átvételt. A jutalom megmaradt.");if(done)done(false);PumpTypedItemGrants(e);return;}Persist(s,std::move(plan->plan),std::move(plan->checkpoint),0,true,std::move(done),true);};
   });return true;
 }
 else if(action==5){const auto* saved=Instance(*s,code);if(!saved)return false;result=runtime->Abandon(saved->id,now);}
 else if(action==4 || action==7){Event event;event.kind=action==4?m::EventKind::NpcInteracted:m::EventKind::DialogueChoiceCommitted;event.subject="npc."+std::to_string(allow.npc);event.choice=choice;
   if(action==7){bool allowed=false;const auto* saved=Instance(*s,code);if(saved){const auto graph=s->registry->Get(code,saved->version);for(unsigned j=0;j<saved->nodes.size();j++){const auto& n=graph->Nodes()[j].value;if(saved->nodes[j].status==m::NodeStatus::Active && n.kind==m::NodeKind::Objective && n.objective.event==m::EventKind::DialogueChoiceCommitted){const auto& domain=graph->Source().choices.at(n.objective.subject);if(std::ranges::find(domain,choice)!=domain.end()){event.subject=n.objective.subject;allowed=true;break;}}}}if(!allowed)return false;}
   result=runtime->Dispatch({{s->pid,s->login},{34,s->checkpoint.revision+1},now,event.kind,event.subject,1,event.choice},now);
 }else return false;
 if(result.code!=m::ResultCode::Applied && result.code!=m::ResultCode::NoChange)return false;
 auto checkpoint=runtime->Save();auto plan=mp::Save(checkpoint,*s->registry,s->owner,s->revision,mp::NewToken());m::FenceTypedMissionPlan(plan,g_registry.get<ecs::TypedItems>(e).state.fence);
 if(action==2)plan.statements.push_back({"INSERT IGNORE INTO player.item_mission_watch(character_id) VALUES(?)",{SqlParam::UInt(s->pid)},{},{}});
 Persist(s,std::move(plan),std::move(checkpoint),0,false,std::move(done),true);return true;
}
}
std::expected<void,std::string> LoadTypedMissionRuntime(const std::string& folder,const GameSqlConfig& sql){
 try{const auto file=std::filesystem::path(folder)/"typed_mission_runtime.json";if(!std::filesystem::exists(file)){package.reset();return {};}
   std::ifstream stream(file);const auto config=nlohmann::json::parse(stream);if(config.size()!=3 || config.at("version")!=1 || !config.at("missions").is_array() || config.at("missions").size()>32)return std::unexpected("Invalid typed Mission runtime configuration");
   const auto name=config.at("package").get<std::string>();if(name.empty() || name!=std::filesystem::path(name).filename().string())return std::unexpected("Mission package must be a local filename");const auto path=std::filesystem::path(folder)/name;auto loaded=m::LoadPackageFile(path);if(!loaded)return std::unexpected("Invalid typed Mission package");
   auto registry=std::make_shared<m::DefinitionRegistry>();m::RewardBindingPolicy policy;policy.atomicTypedItems=true;policy.maxExperience=INT64_MAX;policy.maxLevels=254;policy.maxAccountCoins=INT64_MAX;policy.maxGold=INT64_MAX;policy.maxGoldCost=INT64_MAX;const auto catalog=store::CurrentCatalog();if(!catalog)return std::unexpected("Typed catalog missing");
   for(const auto& graph:loaded.package->Graphs()){for(const auto& n:graph->Nodes())if(n.value.kind==m::NodeKind::Objective && n.value.objective.event!=m::EventKind::ItemCollected && n.value.objective.event!=m::EventKind::ItemCrafted && n.value.objective.event!=m::EventKind::MobKilled && n.value.objective.event!=m::EventKind::BossDefeated && n.value.objective.event!=m::EventKind::NpcInteracted && n.value.objective.event!=m::EventKind::DialogueChoiceCommitted)return std::unexpected("Mission event has no verified live adapter");
     for(const auto& [key,bundle]:graph->Source().rewards){const auto node=std::ranges::find_if(graph->Nodes(),[&](const auto& n){return n.value.kind==m::NodeKind::Reward && n.value.reward==key;});if(node==graph->Nodes().end())return std::unexpected("Unused Mission reward");auto pin=m::PinReward({1,graph->Source().id,graph->Source().version,1,node->value.id,key},m::FingerprintDefinition(*graph),*graph);if(!pin || !m::TypedItemContract(*catalog,*pin.reward))return std::unexpected("Mission reward needs a supported typed item contract");for(const auto* table:{&bundle.typedItems,&bundle.typedConsumeItems,&bundle.typedRequirements})for(const auto& [item,count]:*table)policy.typedItems[item]={50000,50000};}
   }
   m::MissionPackageManager manager(*registry,m::MissionPackageManager::Mode::Gameplay,loaded.package->Manifest().requiresTypes,policy);if(manager.Activate(loaded).status!=m::MissionPackageManager::Status::Activated)return std::unexpected("Mission gameplay bindings rejected");
   std::map<std::string,Admission> rules;for(const auto& row:config.at("missions")){if(row.size()!=4)return std::unexpected("Invalid Mission admission");const auto code=row.at("code").get<std::string>();Admission a{row.at("version").get<uint32_t>(),row.at("npc_race").get<uint32_t>(),row.at("min_level").get<uint16_t>()};if(!a.npc || a.level>255 || !registry->Get(code,a.version) || !rules.emplace(code,a).second)return std::unexpected("Unknown or duplicate Mission admission");}if(rules.size()!=loaded.package->Graphs().size())return std::unexpected("Every live Mission needs admission rules");
   connection=sql;package=std::move(loaded.package);admissions=std::move(rules);std::ifstream bytes(path,std::ios::binary);packageBytes.assign(std::istreambuf_iterator<char>(bytes),{});LOG_INFO("TypedMission: {} native missions enabled",admissions.size());return {};
 }catch(const std::exception&){return std::unexpected("Cannot load typed Mission package/configuration");}
}
void TickTypedMissionRuntime(){
 if(!package)return;std::vector<entt::entity> entities;for(auto e:g_registry.view<ecs::TypedItems,ecs::TypedItemLogin>())entities.push_back(e);
 for(const auto e:entities){auto s=Get(e);const auto login=g_registry.get<ecs::TypedItemLogin>(e).token;if(!s || s->login!=login){s=std::make_shared<Session>();s->pid=ecs::PlayerRuntime::GetPlayerID(e);if(auto* d=ecs::PlayerRuntime::GetDesc(e))s->accountId=d->GetAccountTable().id;s->login=login;s->native=g_registry.get<ecs::TypedItems>(e).state.fence.sessionGeneration;g_registry.emplace_or_replace<Reference>(e,s);}if(!s->loaded){if(!s->pending && Now()>=s->nextLoad)Load(s);continue;}if(s->pending || s->polling)continue;
   const auto now=std::max(Now(),s->checkpoint.now);
   // Rebuild the decision runtime only for an event or a due deadline. A fresh
   // durable checkpoint invalidates this cached deadline; idle ticks do no
   // graph traversal or instance allocation.
   if(s->events.empty() && s->nextDeadline>now){if(now>=s->nextPoll)Poll(s);continue;}
   auto runtime=Runtime(s);s->nextDeadline=runtime->NextDeadline().value_or(INT64_MAX);size_t count=0;std::vector<uint64_t> ids;
   if(!s->events.empty()){for(const auto& event:s->events){const auto result=runtime->Dispatch({{s->pid,s->login},{34,runtime->Revision()+1},std::min(now,event.time),event.kind,event.subject,event.amount,event.choice},now);if(result.code!=m::ResultCode::Applied && result.code!=m::ResultCode::NoChange)break;count++;if(event.durable)ids.push_back(event.durable);if(count==32)break;}}
   else if(const auto deadline=runtime->NextDeadline();deadline && *deadline<=now)runtime->AdvanceTime(now);
   if(runtime->Revision()!=s->checkpoint.revision){auto checkpoint=runtime->Save();auto plan=mp::Save(checkpoint,*s->registry,s->owner,s->revision,mp::NewToken());m::FenceTypedMissionPlan(plan,g_registry.get<ecs::TypedItems>(e).state.fence);for(auto id:ids)plan.statements.push_back({"UPDATE player.item_mission_event SET delivered=1 WHERE event_id=? AND character_id=? AND delivered=0",{SqlParam::UInt(id),SqlParam::UInt(s->pid)},1,{}});Persist(s,std::move(plan),std::move(checkpoint),count);}
   else if(s->events.empty() && Now()>=s->nextPoll)Poll(s);
 }
}
bool HandleTypedMissionPacket(entt::entity e,const char* bytes){
 const auto r=*reinterpret_cast<const Request*>(bytes);if(!g_registry.valid(e) || !std::memchr(r.mission,0,97) || !std::memchr(r.choice,0,97) || r.action<1 || r.action>7)return true;auto s=Get(e);if(!s){const auto* login=g_registry.try_get<ecs::TypedItemLogin>(e);const auto* items=g_registry.try_get<ecs::TypedItems>(e);if(!login || !items)return true;s=std::make_shared<Session>();s->pid=ecs::PlayerRuntime::GetPlayerID(e);if(auto* d=ecs::PlayerRuntime::GetDesc(e))s->accountId=d->GetAccountTable().id;s->login=login->token;s->native=items->state.fence.sessionGeneration;g_registry.emplace_or_replace<Reference>(e,s);if(package)Load(s);}
 if(!package){s->visible=true;Reply(s,Status::Failed,"Nincs aktív Mission-csomag.");return true;}
 if(r.action==1){if(r.nonce || r.request)return true;s->visible=true;if(!s->nonce || !s->pending){const auto token=mp::NewToken();std::memcpy(&s->nonce,token.data(),8);if(!s->nonce)s->nonce=1;s->last=0;s->npc=r.npc;}s->status=s->pending?Status::Pending:s->loaded?Status::Ready:Status::Loading;Reply(s,s->status);return true;}
 if(r.nonce!=s->nonce || !s->visible){Reply(s,Status::Stale);return true;}if(r.action==6){if(!s->pending){Reply(s,Status::Closed);s->visible=false;}return true;}
 if(r.request==s->last && std::memcmp(&r,&s->previous,sizeof r)==0){Reply(s,s->pending?Status::Pending:s->status);return true;}if(s->pending || !s->events.empty()){s->replyAfterPoll=s->polling;Reply(s,Status::Pending);return true;}if(!r.request || r.request!=s->last+1){Reply(s,Status::Stale);return true;}s->last=r.request;s->previous=r;s->npc=r.npc;
 if(!Act(s,r.mission,r.action,r.choice,false)){s->status=Status::Denied;Reply(s,s->status);}return true;
}
void TypedMissionNpcInteracted(entt::entity e,uint32_t npcVid){
 auto s=Get(e);const auto npc=CHARACTER_MANAGER::instance().FindEntity(npcVid);if(!s || !s->loaded || !g_registry.valid(npc) || s->events.size()>4096)return;const auto race=ecs::PlayerRuntime::GetRaceNum(npc);if(!Near(e,npcVid,race))return;const auto subject="npc."+std::to_string(race);
 for(const auto& [id,instance]:s->checkpoint.instances){if(instance.status!=m::MissionStatus::Active)continue;const auto graph=s->registry->Get(instance.mission,instance.version);for(unsigned j=0;j<instance.nodes.size();j++){const auto& node=graph->Nodes()[j].value;if(instance.nodes[j].status==m::NodeStatus::Active && node.kind==m::NodeKind::Objective && node.objective.event==m::EventKind::NpcInteracted && (node.objective.subject=="*" || node.objective.subject==subject)){s->events.push_back({0,m::EventKind::NpcInteracted,subject,{},1,Now()});return;}}}
}
void TypedMissionMobKilled(entt::entity e,uint32_t race,bool boss){auto s=Get(e);if(!s || !s->loaded || s->events.size()>4096)return;s->events.push_back({0,m::EventKind::MobKilled,"mob."+std::to_string(race),{},1,Now()});if(boss)s->events.push_back({0,m::EventKind::BossDefeated,"mob."+std::to_string(race),{},1,Now()});}
bool TypedMissionQuestAction(entt::entity e,const std::string& code,unsigned action,std::function<void(bool)> done){auto s=Get(e);if(!s)return false;const auto npc=ecs::PlayerRuntime::GetQuestNPC(e);if(!g_registry.valid(npc))return false;s->npc=ecs::PlayerRuntime::GetPacketVID(npc);return Act(s,code,action,"",true,std::move(done));}
int TypedMissionQuestStatus(entt::entity e,const std::string& code){auto s=Get(e);if(!s || !s->loaded || s->pending)return -1;const auto* saved=Instance(*s,code);if(!saved)return 0;if(saved->status==m::MissionStatus::Completed)return 3;if(saved->status!=m::MissionStatus::Active)return 4;auto r=Runtime(s);for(const auto& key:r->PendingActions())if(key.mission==code)return 2;return 1;}
