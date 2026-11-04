#include "stdafx.h"
#include "TypedGathering.hpp"
#include "TypedTools.hpp"
#include "TypedFishProcessing.hpp"
#include "TypedInventorySync.hpp"
#include "TypedDrops.hpp"
#include "ItemCatalogRuntime.hpp"
#include "ItemStore.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/ResourceAdmission.hpp"
#include "../ecs/SpatialHelpers.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../entity/char_manager.h"
#include "../network/desc.h"
#include "../core/packet.h"
#include <item/ResourceGathering.hpp>
#include <nlohmann/json.hpp>
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
};
item::ToolRuntimeActor Actor(entt::entity e)
{return {static_cast<std::uint16_t>(ecs::PointSystem::GetLevel(e)),static_cast<std::uint8_t>(ecs::PlayerRuntime::GetJob(e)),static_cast<std::uint8_t>(ecs::PlayerRuntime::GetSex(e)),TypedGatheringAdmitted(e),ecs::PlayerRuntime::GetPosition(e)==POS_FIGHTING,false,false,false};}
bool Node(entt::entity e,entt::entity n,const item::GatheringSite& site)
{
 if(!g_registry.valid(n) || !ecs::PlayerRuntime::IsValid(n) || ecs::PlayerRuntime::IsPC(n) || CombatSystem::IsDead(n) || ecs::PlayerRuntime::GetMapIndex(e)!=site.map || ecs::PlayerRuntime::GetMapIndex(n)!=site.map || ecs::PlayerRuntime::GetRaceNum(n)!=site.race)return false;
 auto dx=std::abs(std::int64_t(ecs::PlayerRuntime::GetX(e))-ecs::PlayerRuntime::GetX(n)),dy=std::abs(std::int64_t(ecs::PlayerRuntime::GetY(e))-ecs::PlayerRuntime::GetY(n));return std::max(dx,dy)+std::min(dx,dy)/2<site.range;
}
void Reply(entt::entity e,std::optional<Status> overrideStatus={})
{
 auto c=store::CurrentCatalog();if(!g_registry.valid(e) || !c)return;auto* s=g_registry.try_get<TypedResourceSession>(e);auto* d=ecs::PlayerRuntime::GetDesc(e);if(!d || !d->IsPhase(PHASE_GAME))return;
 const auto status=overrideStatus.value_or(s ? s->status : Status::Closed);auto remaining=s && status==Status::Running ? static_cast<std::uint32_t>(std::max<std::int64_t>(0,std::chrono::duration_cast<std::chrono::milliseconds>(s->end-Time::now()).count())) : 0;
 auto bytes=item::EncodeResourceGathering(*c,{status,s ? s->error : std::uint8_t{255},s ? s->nonce : 0,s && s->tool ? &*s->tool : nullptr,s ? s->site : nullptr,s ? s->vid : 0,remaining,s ? s->chance : std::uint16_t{},status==Status::Applied && s && s->result ? &*s->result : nullptr});
 SendTypedInventory(e);if(bytes)d->LargePacket(bytes->data(),static_cast<int>(bytes->size()));else LOG_ERROR("TypedGathering: response could not be encoded");
}
void Cancel(entt::entity e,std::uint8_t error=255)
{auto& s=g_registry.get<TypedResourceSession>(e);s.status=Status::Cancelled;s.error=error;g_registry.remove<ecs::TypedItemBusy>(e);Reply(e);PumpTypedItemGrants(e);}
bool Current(entt::entity e,const TypedResourceSession& s)
{
 const auto c=store::CurrentCatalog();const auto* login=g_registry.try_get<ecs::TypedItemLogin>(e);const auto* state=g_registry.try_get<ecs::TypedItems>(e);if(!c || !login || !state || login->token!=s.token || state->state.fence.sessionGeneration!=s.ownerSession || c->ContentHash()!=s.catalog || !s.tool || !s.site || !Node(e,s.node,*s.site) || !TypedGatheringAdmitted(e))return false;
 const auto i=std::ranges::find(state->state.items,s.tool->id,&item::PersistentItem::id);return i!=state->state.items.end() && *i==*s.tool && item::CheckResourceGathering(*c,*s.site,state->state.fence.owner,state->state.items,{s.tool->id,Actor(e)}).has_value();
}
std::uint32_t Draw(std::uint32_t bound)
{if(!bound)throw std::runtime_error("empty gathering RNG range");std::uint32_t n;const std::uint64_t range=std::uint64_t{UINT32_MAX}+1,limit=range-range%bound;do{if(RAND_bytes(reinterpret_cast<unsigned char*>(&n),4)!=1)throw std::runtime_error("gathering RNG unavailable");}while(n>=limit);return n%bound;}
void Complete(entt::entity e)
{
 auto& s=g_registry.get<TypedResourceSession>(e);const auto c=store::CurrentCatalog();const auto& state=g_registry.get<ecs::TypedItems>(e).state;
 auto permit=ecs::resource_admission::Enter({e},auriga::resource_admission::Operation::ItemUse);if(!permit || !permit->Current()){Cancel(e,1);return;}
 std::expected<item::ResourcePlan,item::ResourceError> plan=std::unexpected(item::ResourceError::Unavailable);
 try{plan=item::MakeResourceGatheringCommit(*c,*s.site,s.operation,state.fence,state.storages,state.items,{s.tool->id,Actor(e),true},store::PeekInstanceIds(item::MaxItemCommitItems),Draw);}catch(const std::exception&){}
 if(!plan){Cancel(e,static_cast<std::uint8_t>(plan.error()));return;}
 const auto pid=ecs::PlayerRuntime::GetPlayerID(e);const auto token=s.token;const auto owner=state.fence;auto held=std::make_shared<auriga::resource_admission::Permit>(std::move(*permit));
 s.status=Status::Pending;store::SpendInstanceIds(std::ranges::count_if(plan->commit.items,[](const auto& change){return !change.before;}));auto commit=std::move(plan->commit);plan->commit={};s.result=std::move(*plan);
 const bool queued=store::RequestCommit(owner.owner,owner.sessionGeneration,std::move(commit),[pid,token,held](store::CommitOutcome out)
 {
  auto e=CHARACTER_MANAGER::instance().FindEntityByPID(pid);if(!g_registry.valid(e))return;const auto* login=g_registry.try_get<ecs::TypedItemLogin>(e);if(!login || login->token!=token)return;
  if(!out.state){if(auto* d=ecs::PlayerRuntime::GetDesc(e))d->SetPhase(PHASE_CLOSE);return;}
  g_registry.emplace_or_replace<ecs::TypedItems>(e,std::move(*out.state));g_registry.remove<ecs::TypedItemBusy>(e);
  if(auto* s=g_registry.try_get<TypedResourceSession>(e)){s->status=out.write ? Status::Applied : Status::Failed;if(!out.write)s->result.reset();}Reply(e);PumpTypedItemGrants(e);
 });
 if(!queued){s.result.reset();s.status=Status::Failed;g_registry.remove<ecs::TypedItemBusy>(e);Reply(e);PumpTypedItemGrants(e);}else Reply(e);
}
}
std::expected<void,std::string> LoadTypedGathering(const std::string& path)
{
 try
 {
  if(!std::filesystem::exists(path)){sites.clear();return {};}if(std::filesystem::file_size(path)>1u<<20)return std::unexpected("typed_gathering.json exceeds 1 MiB");
  std::ifstream input(path);auto json=nlohmann::json::parse(input);if(!json.is_object() || json.size()!=2 || json.at("version")!=1 || !json.at("sites").is_array() || json.at("sites").size()>256)throw std::runtime_error("document");
  const auto readNumber=[](const nlohmann::json& j,const char* key,std::uint64_t max)->std::uint32_t{const auto& v=j.at(key);if(!v.is_number_unsigned() || v.get<std::uint64_t>()>max)throw std::runtime_error("integer");return static_cast<std::uint32_t>(v.get<std::uint64_t>());};
  std::vector<item::GatheringSite> next;
  for(const auto& j:json.at("sites"))
  {
   if(!j.is_object() || j.size()!=14 || !j.at("rewards").is_array() || j.at("rewards").size()>32)throw std::runtime_error("site");item::GatheringSite s;
   s.id=readNumber(j,"id",UINT32_MAX);s.map=readNumber(j,"map",INT32_MAX);s.race=readNumber(j,"race",UINT32_MAX);s.name=j.at("name").get<std::string>();s.kind=static_cast<item::ToolKind>(readNumber(j,"kind",4));s.pool=static_cast<item::FishingPool>(readNumber(j,"pool",3));s.range=readNumber(j,"range",1000);s.durationMs=readNumber(j,"duration_ms",120000);s.minimumPower=readNumber(j,"minimum_power",INT32_MAX);s.powerStep=readNumber(j,"power_step",INT32_MAX);s.baseChance=static_cast<std::uint16_t>(readNumber(j,"base_chance_bp",10000));s.powerChance=static_cast<std::uint16_t>(readNumber(j,"power_chance_bp",10000));s.baitChance=static_cast<std::uint16_t>(readNumber(j,"bait_chance_bp",10000));
   for(const auto& r:j.at("rewards")){if(!r.is_object() || r.size()!=4)throw std::runtime_error("reward");auto key=item::ItemKey::Create(static_cast<std::uint16_t>(readNumber(r,"category",UINT16_MAX)),readNumber(r,"local",UINT32_MAX));if(!key)throw std::runtime_error("key");s.rewards.push_back({*key,readNumber(r,"count",10000),readNumber(r,"weight",UINT32_MAX)});}next.push_back(std::move(s));
  }
  const auto c=store::CurrentCatalog();if(!c || !item::ValidateGatheringSites(*c,next))return std::unexpected("invalid typed gathering sites or reward references");sites=std::move(next);LOG_INFO("TypedGathering: {} sites loaded",sites.size());return {};
 }catch(const std::exception&){return std::unexpected("cannot parse typed_gathering.json");}
}
bool TypedResourceActive(entt::entity e)
{const auto* s=g_registry.valid(e) ? g_registry.try_get<TypedResourceSession>(e) : nullptr;return s && (s->status==Status::Running || s->status==Status::Pending);}
void TickTypedGathering()
{
 std::vector<entt::entity> active;for(auto e:g_registry.view<TypedResourceSession>())if(g_registry.get<TypedResourceSession>(e).status==Status::Running)active.push_back(e);
 for(auto e:active){auto& s=g_registry.get<TypedResourceSession>(e);if(!Current(e,s) || std::abs(std::int64_t(ecs::PlayerRuntime::GetX(e))-s.x)>50 || std::abs(std::int64_t(ecs::PlayerRuntime::GetY(e))-s.y)>50){Cancel(e,1);continue;}if(Time::now()>=s.end)Complete(e);}
}
bool HandleTypedGatheringPacket(entt::entity e,const char* bytes)
{
 if(!g_registry.valid(e))return true;const auto r=*reinterpret_cast<const TPacketCGTypedRecovery*>(bytes);const auto c=store::CurrentCatalog();if(!c)return true;auto* s=g_registry.try_get<TypedResourceSession>(e);
 if(r.action==10)
 {
  if(TypedResourceActive(e)){Reply(e);return true;}g_registry.remove<TypedResourceSession>(e);
  if(r.session || !r.source || r.target>UINT32_MAX){Reply(e,Status::Denied);return true;}
  if(!TypedGatheringAdmitted(e)){Reply(e,Status::Denied);return true;}if(g_registry.all_of<ecs::TypedItemBusy>(e) || TypedGatheringActive(e)){Reply(e,Status::Busy);return true;}
  const auto& state=g_registry.get<ecs::TypedItems>(e).state;auto tool=std::ranges::find_if(state.items,[&](const auto& i){return i.id.Value()==r.source;});if(tool==state.items.end() || !tool->tool || !std::holds_alternative<item::ItemToolRow>(c->Find(tool->key)->category)){Reply(e,Status::Denied);return true;}
  TypedResourceSession next;next.tool=*tool;next.token=g_registry.get<ecs::TypedItemLogin>(e).token;next.ownerSession=state.fence.sessionGeneration;next.catalog=c->ContentHash();next.status=Status::Denied;
  if(RAND_bytes(reinterpret_cast<unsigned char*>(&next.nonce),8)!=1 || !next.nonce || RAND_bytes(next.operation.data(),16)!=1 || std::ranges::all_of(next.operation,[](auto b){return !b;})){Reply(e,Status::Failed);return true;}
  const auto* row=std::get_if<item::ItemToolRow>(&c->Find(tool->key)->category);std::uint64_t bestDistance=UINT64_MAX;
  for(const auto& site:sites)
  {
   if(!row || site.map!=ecs::PlayerRuntime::GetMapIndex(e) || site.kind!=row->kind || site.pool!=row->fishingPool)continue;
   auto checked=item::CheckResourceGathering(*c,site,state.fence.owner,state.items,{tool->id,Actor(e)});if(!checked){next.error=static_cast<std::uint8_t>(checked.error());continue;}
   ecs::ForEachAround(g_registry,e,[&](entt::entity n){if(!Node(e,n,site))return;auto vid=ecs::PlayerRuntime::GetPacketVID(n);const auto dx=std::abs(std::int64_t(ecs::PlayerRuntime::GetX(e))-ecs::PlayerRuntime::GetX(n)),dy=std::abs(std::int64_t(ecs::PlayerRuntime::GetY(e))-ecs::PlayerRuntime::GetY(n));const auto distance=static_cast<std::uint64_t>(std::max(dx,dy)+std::min(dx,dy)/2);if((!r.target || r.target==vid) && (next.node==entt::null || distance<bestDistance || (distance==bestDistance && vid<next.vid))){next.site=&site;next.node=n;next.vid=vid;next.chance=*checked;next.status=Status::Ready;bestDistance=distance;}});
  }
  if(next.site)next.error=255;g_registry.emplace_or_replace<TypedResourceSession>(e,std::move(next));Reply(e);return true;
 }
 if(!s || !s->tool || r.session!=s->nonce || r.source!=s->tool->id.Value() || r.target!=s->vid){Reply(e,Status::Stale);return true;}
 if(r.action==13){Reply(e);return true;}if(r.action==12){if(s->status==Status::Running)Cancel(e);else Reply(e);return true;}
 if(r.action!=11){Reply(e,Status::Denied);return true;}if(s->status!=Status::Ready){Reply(e);return true;}
 if(!Current(e,*s)){s->status=Status::Stale;Reply(e);return true;}if(g_registry.all_of<ecs::TypedItemBusy>(e)){Reply(e,Status::Busy);return true;}
 s->x=ecs::PlayerRuntime::GetX(e);s->y=ecs::PlayerRuntime::GetY(e);s->end=Time::now()+std::chrono::milliseconds(s->site->durationMs);s->status=Status::Running;g_registry.emplace_or_replace<ecs::TypedItemBusy>(e);Reply(e);return true;
}
