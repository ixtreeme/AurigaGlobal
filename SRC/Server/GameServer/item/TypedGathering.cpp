#include "stdafx.h"
#include "TypedRecovery.hpp"
#include "TypedGathering.hpp"
#include "TypedFieldGathering.hpp"
#include "TypedTools.hpp"
#include "TypedFishProcessing.hpp"
#include "TypedInventorySync.hpp"
#include "TypedDrops.hpp"
#include "TypedLoot.hpp"
#include "ItemCatalogRuntime.hpp"
#include "ItemStore.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/ResourceAdmission.hpp"
#include "../ecs/SpatialHelpers.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../ecs/systems/MountSystem.hpp"
#include "../ecs/systems/SkillSystem.hpp"
#include "../entity/char_manager.h"
#include "../network/desc.h"
#include "../core/packet.h"
#include "../core/vector.h"
#include "../world/sectree.h"
#include "../world/fishing.h"
#include "../mission_pass/MissionPassEvents.hpp"
#include "../ecs/systems/ViewSystem.hpp"
#include <item/ResourceGathering.hpp>
#include <openssl/rand.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
namespace
{
namespace item=auriga::item;namespace store=item::runtime;using Status=item::ResourceStatus;using Time=std::chrono::steady_clock;
std::vector<item::GatheringSite> sites;
struct TypedResourceSession
{
 std::uint64_t nonce=0,token=0,ownerSession=0;std::array<std::uint8_t,16> operation{};item::ItemCatalogDigest catalog{};
 std::optional<item::PersistentItem> tool;const item::GatheringSite* site=nullptr;entt::entity node=entt::null;std::uint32_t vid=0;
 std::int32_t x=0,y=0;Time::time_point end;Status status=Status::Ready;std::uint8_t error=255;std::uint16_t chance=0;std::optional<item::ResourcePlan> result;
 bool water=false,cancelling=false;item::FishingRuntimeView fishing;Time::time_point began;
 std::optional<std::uint32_t> fishRewardRoll;item::ToolRuntimeActor castActor;
 std::uint8_t digs=0;
};
item::ToolRuntimeActor Actor(entt::entity e)
{return {static_cast<std::uint16_t>(ecs::PointSystem::GetLevel(e)),static_cast<std::uint8_t>(ecs::PlayerRuntime::GetJob(e)),static_cast<std::uint8_t>(ecs::PlayerRuntime::GetSex(e)),TypedGatheringAdmitted(e),ecs::PlayerRuntime::GetPosition(e)==POS_FIGHTING,false,false,false};}
std::uint8_t MiningSkill(entt::entity e)
{return static_cast<std::uint8_t>(std::clamp<int>(SkillSystem::GetSkillLevel(e,SKILL_MINING),0,40));}
bool Mounted(entt::entity e)
{return MountSystem::IsRiding(e) || MountSystem::GetMountVnum(e)!=0;}
std::uint32_t Duration(const TypedResourceSession& session)
{return session.digs ? 2000u*session.digs : session.site->durationMs;}
bool OnSiteMap(entt::entity e,const item::GatheringSite& site)
{
 const auto map=ecs::PlayerRuntime::GetMapIndex(e);
 return map>0 && (map==site.map || (site.mining && map>=10000 && map/10000==site.map));
}
bool Node(entt::entity e,entt::entity n,const item::GatheringSite& site)
{
 if(!g_registry.valid(n) || IsFieldGatheringNode(n) || !ecs::PlayerRuntime::IsValid(n) || ecs::PlayerRuntime::IsPC(n) || CombatSystem::IsDead(n) || !OnSiteMap(e,site) || ecs::PlayerRuntime::GetMapIndex(e)!=ecs::PlayerRuntime::GetMapIndex(n) || ecs::PlayerRuntime::GetRaceNum(n)!=site.race)return false;
 auto dx=std::abs(std::int64_t(ecs::PlayerRuntime::GetX(e))-ecs::PlayerRuntime::GetX(n)),dy=std::abs(std::int64_t(ecs::PlayerRuntime::GetY(e))-ecs::PlayerRuntime::GetY(n));return std::max(dx,dy)+std::min(dx,dy)/2<site.range;
}
std::uint32_t Elapsed(const TypedResourceSession& s)
{return s.began==Time::time_point{} ? 0u : static_cast<std::uint32_t>(std::clamp<std::int64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(Time::now()-s.began).count(),0,UINT32_MAX));}
bool Water(entt::entity e,const TypedResourceSession& s)
{
 if(!s.site || s.site->race || ecs::PlayerRuntime::GetMapIndex(e)!=s.site->map || std::abs(std::int64_t(ecs::PlayerRuntime::GetX(e))-s.x)>50 || std::abs(std::int64_t(ecs::PlayerRuntime::GetY(e))-s.y)>50)return false;
 auto* origin=ecs::SectorAt(s.site->map,ecs::PlayerRuntime::GetX(e),ecs::PlayerRuntime::GetY(e));auto* cast=ecs::SectorAt(s.site->map,s.fishing.castX,s.fishing.castY);
 if(!origin || !cast || origin->IsAttr(ecs::PlayerRuntime::GetX(e),ecs::PlayerRuntime::GetY(e),ATTR_BLOCK|ATTR_WATER) || !cast->IsAttr(s.fishing.castX,s.fishing.castY,ATTR_WATER))return false;
 const auto steps=(s.site->range+49)/50;for(std::uint32_t i=1;i<=steps;++i){auto x=s.x+static_cast<std::int32_t>(std::int64_t(s.fishing.castX-s.x)*i/steps),y=s.y+static_cast<std::int32_t>(std::int64_t(s.fishing.castY-s.y)*i/steps);auto* t=ecs::SectorAt(s.site->map,x,y);if(!t || (t->IsAttr(x,y,ATTR_BLOCK) && !t->IsAttr(x,y,ATTR_WATER)))return false;}return true;
}
void Motion(entt::entity e,std::uint8_t sub)
{TPacketGCFishing p{};p.header=HEADER_GC_FISHING;p.subheader=sub;p.info=ecs::PlayerRuntime::GetPacketVID(e);p.dir=static_cast<std::uint8_t>(ecs::PlayerRuntime::GetRotation(e)/5);ecs::ViewSystem::PacketView(e,&p,sizeof(p));}
void Reply(entt::entity e,std::optional<Status> overrideStatus={})
{
 auto c=store::CurrentCatalog();if(!g_registry.valid(e) || !c)return;auto* s=g_registry.try_get<TypedResourceSession>(e);auto* d=ecs::PlayerRuntime::GetDesc(e);if(!d || !d->IsPhase(PHASE_GAME))return;
 if(overrideStatus==Status::Stale){auto bytes=item::EncodeResourceGathering(*c,{Status::Stale});if(bytes)d->LargePacket(bytes->data(),static_cast<int>(bytes->size()));return;}
 const auto status=overrideStatus.value_or(s ? s->status : Status::Closed);auto remaining=s && status==Status::Running ? static_cast<std::uint32_t>(std::max<std::int64_t>(0,std::chrono::duration_cast<std::chrono::milliseconds>(s->end-Time::now()).count())) : 0;
 if(s && s->water){if(s->status==Status::Running)s->fishing.elapsedMs=Elapsed(*s);if(status==Status::Running)remaining=s->fishing.elapsedMs+std::uint64_t{s->fishing.state.misses}*1000>=s->fishing.state.durationMs ? 0 : s->fishing.state.durationMs-s->fishing.elapsedMs-s->fishing.state.misses*1000;}
 std::optional<item::ItemKey> prediction;
 if(s && s->water && s->site && s->fishRewardRoll && status==Status::Running && fishing::PredictFish(e))if(const auto* reward=item::SelectGatheringReward(*s->site,*s->fishRewardRoll))prediction=reward->key;
 auto bytes=item::EncodeResourceGathering(*c,{status,s ? s->error : std::uint8_t{255},s ? s->nonce : 0,s && s->tool ? &*s->tool : nullptr,s ? s->site : nullptr,s ? s->vid : 0,remaining,s ? s->chance : std::uint16_t{},status==Status::Applied && s && s->result ? &*s->result : nullptr,s && s->water ? &s->fishing : nullptr,prediction,s && s->site ? Duration(*s) : 0});
 SendTypedInventory(e);if(bytes)d->LargePacket(bytes->data(),static_cast<int>(bytes->size()));else LOG_ERROR("TypedGathering: response could not be encoded");
}
void Complete(entt::entity e,bool eligible);
bool CleanupOwner(entt::entity e,const TypedResourceSession& s){
 const auto c=store::CurrentCatalog();const auto* t=g_registry.valid(e) ? g_registry.try_get<ecs::TypedItems>(e) : nullptr;const auto* l=g_registry.valid(e) ? g_registry.try_get<ecs::TypedItemLogin>(e) : nullptr;
 if(!c || !t || !l || !s.tool || !s.site || l->token!=s.token || t->state.fence.sessionGeneration!=s.ownerSession || c->ContentHash()!=s.catalog)return false;
 const auto i=std::ranges::find(t->state.items,s.tool->id,&item::PersistentItem::id);return i!=t->state.items.end() && *i==*s.tool;
}
void Cancel(entt::entity e,std::uint8_t error=255)
{auto& s=g_registry.get<TypedResourceSession>(e);if(s.water && s.status==Status::Running && !s.cancelling && CleanupOwner(e,s)){s.cancelling=true;s.error=error;Complete(e,false);return;}if(s.water)Motion(e,FISHING_SUBHEADER_GC_STOP);s.status=Status::Cancelled;s.error=error;g_registry.remove<ecs::TypedItemBusy>(e);Reply(e);PumpTypedItemGrants(e);}
bool Current(entt::entity e,const TypedResourceSession& s)
{
 const auto c=store::CurrentCatalog();const auto* login=g_registry.try_get<ecs::TypedItemLogin>(e);const auto* state=g_registry.try_get<ecs::TypedItems>(e);if(!c || !login || !state || login->token!=s.token || state->state.fence.sessionGeneration!=s.ownerSession || c->ContentHash()!=s.catalog || !s.tool || !s.site || !(s.water ? Water(e,s) : Node(e,s.node,*s.site)) || !TypedGatheringAdmitted(e))return false;
 const auto i=std::ranges::find(state->state.items,s.tool->id,&item::PersistentItem::id);
 return i!=state->state.items.end() && *i==*s.tool && !(s.site->mining && Mounted(e)) &&
     item::CheckResourceGathering(*c,*s.site,state->state.fence.owner,state->state.items,{s.tool->id,Actor(e)}).has_value();
}
std::uint32_t Draw(std::uint32_t bound)
{if(!bound)throw std::runtime_error("empty gathering RNG range");std::uint32_t n;const std::uint64_t range=std::uint64_t{UINT32_MAX}+1,limit=range-range%bound;do{if(RAND_bytes(reinterpret_cast<unsigned char*>(&n),4)!=1)throw std::runtime_error("gathering RNG unavailable");}while(n>=limit);return n%bound;}
void Complete(entt::entity e,bool eligible=true)
{
 auto& s=g_registry.get<TypedResourceSession>(e);const auto c=store::CurrentCatalog();const auto& state=g_registry.get<ecs::TypedItems>(e).state;
 // A cancelled cast only clears the exact originally admitted rod's bait.
 // This bounded cleanup also runs while logout is closing admission; the
 // recovery clock queues it before session handoff and holds relog until SQL settles.
 auto permit=s.cancelling ? std::optional<auriga::resource_admission::Permit>{} : ecs::resource_admission::Enter({e},auriga::resource_admission::Operation::ItemUse);
 if(s.cancelling ? !CleanupOwner(e,s) : !permit || !permit->Current()){Cancel(e,1);return;}
 std::expected<item::ResourcePlan,item::ResourceError> plan=std::unexpected(item::ResourceError::Unavailable);
 item::ResourceRequest request{s.tool->id,s.cancelling ? s.castActor : Actor(e),true,eligible,TypedItemEffectNow(),s.fishRewardRoll};
 if(s.site->mining)request.miningSkill=MiningSkill(e);
 try{plan=item::MakeResourceGatheringCommit(*c,*s.site,s.operation,state.fence,state.storages,state.items,request,store::PeekInstanceIds(item::MaxItemCommitItems),Draw);}catch(const std::exception&){}
 if(!plan){Cancel(e,static_cast<std::uint8_t>(plan.error()));return;}
 std::uint32_t oreVid=0;
 std::uint32_t oreSubject=0;
 std::uint32_t oreCount=0;
 if(s.site->mining && plan->success)
 {
  const auto& ore=plan->yields.front();
  oreSubject=s.site->mining->missionSubject;
  oreCount=ore.count;
  oreVid=ReserveTypedLoot({ore.item,0,ore.count},ecs::PlayerRuntime::GetMapIndex(e),s.x,s.y,ecs::PlayerRuntime::GetZ(e),e,15'000);
  if(!oreVid){Cancel(e,static_cast<std::uint8_t>(item::ResourceError::Unavailable));return;}
 }
 const auto pid=ecs::PlayerRuntime::GetPlayerID(e);const auto token=s.token;const auto owner=state.fence;auto held=permit ? std::make_shared<auriga::resource_admission::Permit>(std::move(*permit)) : nullptr;
 if(s.water){s.fishing.elapsedMs=Elapsed(s);Motion(e,FISHING_SUBHEADER_GC_STOP);auto& receipt=plan->commit.result;receipt.insert(receipt.end(),{'M','I','N',1,s.fishing.state.hits,s.fishing.state.misses});for(auto n:{s.fishing.state.seed,s.fishing.state.sequence,s.fishing.elapsedMs})for(unsigned i=0;i<4;++i)receipt.push_back(static_cast<std::uint8_t>(n>>(8*i)));}
 s.status=Status::Pending;store::SpendInstanceIds(std::ranges::count_if(plan->commit.items,[](const auto& change){return !change.before;}));auto commit=std::move(plan->commit);plan->commit={};s.result=std::move(*plan);
 const bool queued=store::RequestCommit(owner.owner,owner.sessionGeneration,std::move(commit),[pid,token,held,oreVid,oreSubject,oreCount](store::CommitOutcome out)
 {
  const bool published=!oreVid || ResolveReservedTypedLoot(oreVid,out.write.has_value());
  auto e=CHARACTER_MANAGER::instance().FindEntityByPID(pid);if(!g_registry.valid(e))return;const auto* login=g_registry.try_get<ecs::TypedItemLogin>(e);if(!login || login->token!=token)return;
  if(!out.state){if(auto* d=ecs::PlayerRuntime::GetDesc(e))d->SetPhase(PHASE_CLOSE);return;}
  g_registry.emplace_or_replace<ecs::TypedItems>(e,std::move(*out.state));g_registry.remove<ecs::TypedItemBusy>(e);
  if(oreVid && published)AurigaMissionPass::Events::Emit(e,AurigaMissionPass::Kind::OreGather,oreSubject,oreCount);
  if(auto* s=g_registry.try_get<TypedResourceSession>(e)){s->status=out.write && published ? s->cancelling ? Status::Cancelled : Status::Applied : Status::Failed;if(out.write && s->water && s->result)Motion(e,s->result->success ? FISHING_SUBHEADER_GC_SUCCESS : FISHING_SUBHEADER_GC_FAIL);if(!out.write || !published)s->result.reset();}Reply(e);PumpTypedItemGrants(e);
 });
 if(!queued){if(oreVid)ResolveReservedTypedLoot(oreVid,false);s.result.reset();s.status=Status::Failed;g_registry.remove<ecs::TypedItemBusy>(e);Reply(e);PumpTypedItemGrants(e);}else Reply(e);
}
}
std::expected<void,std::string> LoadTypedGathering(const std::string& path)
{
 try
 {
  if(!std::filesystem::exists(path)){sites.clear();return {};}if(std::filesystem::file_size(path)>1u<<20)return std::unexpected("typed_gathering.json exceeds 1 MiB");
  std::ifstream input(path);std::string text((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());const auto c=store::CurrentCatalog();if(!c)return std::unexpected("item catalog not loaded");
  auto parsed=item::ParseGatheringSites(text,*c);if(!parsed)return std::unexpected(parsed.error());auto next=std::move(*parsed);sites=std::move(next);LOG_INFO("TypedGathering: {} sites loaded",sites.size());return {};
 }catch(const std::exception&){return std::unexpected("cannot parse typed_gathering.json");}
}
bool TypedResourceActive(entt::entity e)
{const auto* s=g_registry.valid(e) ? g_registry.try_get<TypedResourceSession>(e) : nullptr;return s && (s->status==Status::Running || s->status==Status::Pending);}
void StopTypedGathering(entt::entity e){if(g_registry.valid(e))if(auto* s=g_registry.try_get<TypedResourceSession>(e);s && s->status==Status::Running)Cancel(e);}
void TickTypedGathering()
{
 std::vector<entt::entity> active;for(auto e:g_registry.view<TypedResourceSession>())if(g_registry.get<TypedResourceSession>(e).status==Status::Running)active.push_back(e);
 for(auto e:active){auto& s=g_registry.get<TypedResourceSession>(e);if(!Current(e,s) || std::abs(std::int64_t(ecs::PlayerRuntime::GetX(e))-s.x)>50 || std::abs(std::int64_t(ecs::PlayerRuntime::GetY(e))-s.y)>50){Cancel(e,1);continue;}if(s.water){if(item::FishingExpired(s.fishing.state,Elapsed(s)))Complete(e,false);}else if(Time::now()>=s.end)Complete(e);}
}
namespace
{
void StartGathering(entt::entity e,TypedResourceSession& session)
{
 if(session.status!=Status::Ready){Reply(e);return;}
 if(!Current(e,session)){session.status=Status::Stale;Reply(e);return;}
 if(g_registry.all_of<ecs::TypedItemBusy>(e)){Reply(e,Status::Busy);return;}
 auto admission=ecs::resource_admission::Enter({e},auriga::resource_admission::Operation::ItemUse);
 if(!admission || !admission->Current()){Reply(e,Status::Denied);return;}
 session.castActor=Actor(e);
 if(session.water)
 {
  std::uint32_t total=0;
  for(const auto& reward:session.site->rewards)total+=reward.weight;
  try{session.fishRewardRoll=Draw(total);}catch(const std::exception&){Reply(e,Status::Failed);return;}
 }
 session.x=ecs::PlayerRuntime::GetX(e);
 session.y=ecs::PlayerRuntime::GetY(e);
 session.began=Time::now();
 session.end=session.began+std::chrono::milliseconds(Duration(session));
 session.status=Status::Running;
 g_registry.emplace_or_replace<ecs::TypedItemBusy>(e);
 if(session.water)Motion(e,FISHING_SUBHEADER_GC_START);
 if(session.digs)
 {
  TPacketGCDigMotion packet{};
  packet.header=HEADER_GC_DIG_MOTION;
  packet.vid=ecs::PlayerRuntime::GetPacketVID(e);
  packet.target_vid=session.vid;
  packet.count=session.digs;
  ecs::ViewSystem::PacketView(e,&packet,sizeof(packet));
 }
 Reply(e);
}

void PrepareGathering(entt::entity e,std::uint64_t source,std::uint32_t target,bool water,bool start)
{
 const auto c=store::CurrentCatalog();
 if(!c)return;
 if(TypedResourceActive(e)){Reply(e);return;}
 g_registry.remove<TypedResourceSession>(e);
 if(!source || !TypedGatheringAdmitted(e)){Reply(e,Status::Denied);return;}
 if(g_registry.all_of<ecs::TypedItemBusy>(e) || TypedGatheringActive(e)){Reply(e,Status::Busy);return;}
 const auto& state=g_registry.get<ecs::TypedItems>(e).state;
 const auto tool=std::ranges::find_if(state.items,[&](const auto& i){return i.id.Value()==source;});
 const auto* entry=tool!=state.items.end() ? c->Find(tool->key) : nullptr;
 const auto* row=entry ? std::get_if<item::ItemToolRow>(&entry->category) : nullptr;
 if(!row || !tool->tool){Reply(e,Status::Denied);return;}
 TypedResourceSession next;
 next.tool=*tool;next.token=g_registry.get<ecs::TypedItemLogin>(e).token;next.ownerSession=state.fence.sessionGeneration;
 next.catalog=c->ContentHash();next.status=Status::Denied;next.water=water;
 next.x=ecs::PlayerRuntime::GetX(e);next.y=ecs::PlayerRuntime::GetY(e);
 if(RAND_bytes(reinterpret_cast<unsigned char*>(&next.nonce),8)!=1 || !next.nonce || RAND_bytes(next.operation.data(),16)!=1 || std::ranges::all_of(next.operation,[](auto b){return !b;})){Reply(e,Status::Failed);return;}
 if(water){next.fishing.state.seed=static_cast<std::uint32_t>(next.nonce);if(!next.fishing.state.seed)next.fishing.state.seed=static_cast<std::uint32_t>(next.nonce>>32);}
 std::uint64_t bestDistance=UINT64_MAX;
 for(const auto& site:sites)
 {
  if(bool(site.race)==water || !OnSiteMap(e,site) || site.kind!=row->kind || (!row->allWaterPools && site.pool!=row->fishingPool) || (site.mining && Mounted(e)) || (start && !site.mining))continue;
  item::ResourceRequest request{tool->id,Actor(e)};
  if(site.mining)request.miningSkill=MiningSkill(e);
  const auto checked=item::CheckResourceGathering(*c,site,state.fence.owner,state.items,request);
  if(!checked){next.error=static_cast<std::uint8_t>(checked.error());continue;}
  if(water){float dx=0,dy=0;GetDeltaByDegree(ecs::PlayerRuntime::GetRotation(e),static_cast<float>(site.range),&dx,&dy);next.fishing.castX=next.x+static_cast<std::int32_t>(dx);next.fishing.castY=next.y+static_cast<std::int32_t>(dy);next.site=&site;next.fishing.state.durationMs=site.durationMs;if(Water(e,next)){next.chance=*checked;next.status=Status::Ready;}else next.site=nullptr;continue;}
  ecs::ForEachAround(g_registry,e,[&](entt::entity n){if(!Node(e,n,site))return;auto vid=ecs::PlayerRuntime::GetPacketVID(n);const auto dx=std::abs(std::int64_t(ecs::PlayerRuntime::GetX(e))-ecs::PlayerRuntime::GetX(n)),dy=std::abs(std::int64_t(ecs::PlayerRuntime::GetY(e))-ecs::PlayerRuntime::GetY(n));const auto distance=static_cast<std::uint64_t>(std::max(dx,dy)+std::min(dx,dy)/2);if((!target || target==vid) && (next.node==entt::null || distance<bestDistance || (distance==bestDistance && vid<next.vid))){next.site=&site;next.node=n;next.vid=vid;next.chance=*checked;next.status=Status::Ready;bestDistance=distance;}});
 }
 if(next.site)next.error=255;
 if(next.site && next.site->mining)
 {
  try{next.digs=static_cast<std::uint8_t>(5+Draw(11));}
  catch(const std::exception&){Reply(e,Status::Failed);return;}
 }
 auto& session=g_registry.emplace_or_replace<TypedResourceSession>(e,std::move(next));
 if(start)StartGathering(e,session);else Reply(e);
}
}

bool TryStartTypedMining(entt::entity e,entt::entity node)
{
 const auto* typed=g_registry.try_get<ecs::TypedItems>(e);
 if(!typed)return false;
 const auto race=ecs::PlayerRuntime::GetRaceNum(node);
 if(std::ranges::none_of(sites,[&](const auto& site){return site.mining && site.race==race && OnSiteMap(e,site);}))
  return false;
 if(auto* session=g_registry.try_get<TypedResourceSession>(e);session && session->digs && session->node==node && session->status==Status::Running)
 {
  Cancel(e);
  return true;
 }
 if(TypedResourceActive(e))return true;
 if(Mounted(e))
 {
  ecs::ChatSystem::Send(e,CHAT_TYPE_INFO,"Bányászat előtt szállj le a hátasról.");
  return true;
 }
 const auto c=store::CurrentCatalog();
 if(!c)return true;
 const auto& items=typed->state.items;
 const auto pick=std::ranges::find_if(items,[&](const auto& i){
     const auto* entry=c->Find(i.key);
     const auto* tool=entry ? std::get_if<item::ItemToolRow>(&entry->category) : nullptr;
     return tool && tool->kind==item::ToolKind::Pick && i.placement==item::ItemPlacement{item::ItemStorageKind::Equipment,1,0};
 });
 if(pick==items.end())
 {
  ecs::ChatSystem::Send(e,CHAT_TYPE_INFO,"A bányászathoz szerelj fel egy csákányt.");
  return true;
 }
 PrepareGathering(e,pick->id.Value(),ecs::PlayerRuntime::GetPacketVID(node),false,true);
 return true;
}

bool HandleTypedGatheringPacket(entt::entity e,const char* bytes)
{
 if(!g_registry.valid(e) || !store::CurrentCatalog())return true;
 const auto r=*reinterpret_cast<const TPacketCGTypedRecovery*>(bytes);
 if(r.action==10 || r.action==14)
 {
  if(r.session || !r.source || r.target>UINT32_MAX || (r.action==14 && r.target)){Reply(e,Status::Denied);return true;}
  PrepareGathering(e,r.source,static_cast<std::uint32_t>(r.target),r.action==14,false);
  return true;
 }
 auto* s=g_registry.try_get<TypedResourceSession>(e);
 if(!s || !s->tool || r.session!=s->nonce || r.source!=s->tool->id.Value() || (s->water ? (r.action!=16 && r.target) || r.action<15 || r.action>18 : r.target!=s->vid)){Reply(e,Status::Stale);return true;}
 if(r.action==13 || r.action==18){Reply(e);return true;}if(r.action==12 || r.action==17){if(s->status==Status::Running)Cancel(e);else Reply(e);return true;}
 if(r.action==16 && s->water){if(s->status!=Status::Running){Reply(e);return true;}if(!Current(e,*s)){Cancel(e,1);return true;}const auto elapsed=Elapsed(*s);s->fishing.state=item::ApplyFishingInput(s->fishing.state,elapsed,static_cast<std::uint32_t>(r.target>>32),static_cast<std::uint32_t>(r.target)&0x7fffffff,(r.target&0x80000000)==0);if(item::FishingWon(s->fishing.state))Complete(e);else if(item::FishingExpired(s->fishing.state,elapsed))Complete(e,false);else Reply(e);return true;}
 if(r.action!=(s->water ? 15 : 11)){Reply(e,Status::Denied);return true;}
 StartGathering(e,*s);
 return true;
}
