#include <item/ResourceGathering.hpp>
#include <item/TypedItemDetails.hpp>
#include <algorithm>
#include <set>
#include <tuple>
namespace auriga::item
{
namespace
{
using E=ResourceError;
std::expected<ResourcePlan,E> Build(const ItemCatalog& c,const GatheringSite& site,std::array<std::uint8_t,16> op,ItemOwnerFence fence,
 std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> items,ResourceRequest request,const GatheringReward* reward,std::span<const ItemInstanceId> ids,std::uint32_t practiceRoll)
{
 std::vector<PersistentItem> working(items.begin(),items.end());auto tool=std::ranges::find(working,request.tool,&PersistentItem::id);if(tool==working.end())return std::unexpected(E::Invalid);
 ResourcePlan out;out.success=reward!=nullptr;
 if(reward)
 {
  auto actor=request.actor;actor.completedResourceAction=true;actor.fishingActive=false;
  auto practice=MakeToolRuntimeCommit(c,op,fence,layouts,items,{tool->id,{},ToolRuntimeAction::Practice,actor,practiceRoll});
  if(!practice)return std::unexpected(E::InvalidDecision);*tool=practice->commit.items[0].after;
 }
 if(site.kind==ToolKind::Rod)tool->tool->baitPower=0;
 out.points=tool->tool->trainingPoints;out.bait=tool->tool->baitPower;
 if(reward)
 {
  auto grant=MakeGrantCommit(c,op,fence,layouts,working,{reward->key,0,reward->count},ids);
  if(!grant)return std::unexpected(grant.error()==ItemPersistenceError::InvalidPlacement ? E::NoRoom : E::Unavailable);
  for(const auto& change:grant->items)if(change.before)*std::ranges::find(working,change.after.id,&PersistentItem::id)=change.after;else working.push_back(change.after);
  out.yields.push_back({reward->key,reward->count});
 }
 auto& commit=out.commit;commit.operationId=op;commit.catalogRevision=c.Revision();commit.catalogHash=c.ContentHash();commit.kind=ItemOperationKind::Service;commit.owners={fence};commit.outcomeCode=out.success ? 0x2521 : 0x2522;
 commit.result={'R','S','C',1,static_cast<std::uint8_t>(out.success)};const auto put=[&](std::uint64_t n,unsigned bytes){for(unsigned i=0;i<bytes;++i)commit.result.push_back(static_cast<std::uint8_t>(n>>(i*8)));};
 put(site.id,4);put(request.tool.Value(),8);put(out.points,4);put(out.bait,4);put(practiceRoll,4);
 for(const auto& yield:out.yields){put(static_cast<unsigned>(yield.item.Category()),2);put(yield.item.LocalId(),4);put(yield.count,4);}
 for(std::size_t i=0;i<items.size();++i)if(working[i]!=items[i] || items[i].id==request.tool)
 {if(items[i].revision==UINT64_MAX)return std::unexpected(E::Invalid);working[i].revision=items[i].revision+1;commit.items.push_back({items[i],working[i]});}
 for(std::size_t i=items.size();i<working.size();++i){working[i].revision=1;commit.items.push_back({{},working[i]});}
 for(const auto& change:commit.items)for(const auto* item:{change.before ? &*change.before : nullptr,&change.after})if(item && item->placement)
 {
  ItemStorage storage{item->owner,item->placement->kind,item->placement->storageId};if(std::ranges::find(commit.storages,storage,&ItemStorageLayout::storage)!=commit.storages.end())continue;
  auto l=std::ranges::find(layouts,storage,&ItemStorageLayout::storage);if(l==layouts.end())return std::unexpected(E::Invalid);commit.storages.push_back(*l);
 }
 if(!ValidateItemCommit(c,commit))return std::unexpected(E::Invalid);return out;
}
}
std::expected<void,ResourceError> ValidateGatheringSites(const ItemCatalog& c,std::span<const GatheringSite> sites)
{
 if(sites.size()>256)return std::unexpected(E::Invalid);std::set<std::uint32_t> ids;std::set<std::tuple<std::uint32_t,std::uint32_t,ToolKind,FishingPool>> matches;
 for(const auto& s:sites)
 {
  if(!s.id || !ids.insert(s.id).second || !s.map || (!s.race && s.kind!=ToolKind::Rod) || !matches.emplace(s.map,s.race,s.kind,s.pool).second || s.name.empty() || s.name.size()>128 || std::ranges::any_of(s.name,[](unsigned char ch){return ch<32 || ch==127;}) ||
   s.kind<ToolKind::Pick || s.kind>ToolKind::Shears || (s.kind==ToolKind::Rod ? s.pool!=FishingPool::First && s.pool!=FishingPool::Second : s.pool!=FishingPool::None) ||
   !s.range || s.range>1000 || s.durationMs<(!s.race ? 5000u : 1000u) || s.durationMs>120000 || s.minimumPower>INT32_MAX || !s.powerStep || s.powerStep>INT32_MAX || s.baseChance>10000 || s.powerChance>10000 || s.baitChance>10000 || (s.kind!=ToolKind::Rod && s.baitChance) || s.rewards.empty() || s.rewards.size()>32)return std::unexpected(E::Invalid);
  std::uint64_t total=0;std::set<ItemKey> rewards;
  for(const auto& r:s.rewards)
  {const auto* entry=c.Find(r.key);if(!entry || entry->base.retired || !SupportsPersistentGrant(c,*entry) || !r.count || r.count>10000 || !r.weight || !rewards.insert(r.key).second)return std::unexpected(E::Invalid);total+=r.weight;}
  if(total>UINT32_MAX)return std::unexpected(E::Invalid);
 }
 return {};
}
std::expected<std::uint16_t,ResourceError> CheckResourceGathering(const ItemCatalog& c,const GatheringSite& site,ItemOwner owner,std::span<const PersistentItem> items,ResourceRequest r)
{
 if(!ValidateGatheringSites(c,{&site,1}))return std::unexpected(E::Invalid);
 const auto i=std::ranges::find(items,r.tool,&PersistentItem::id);if(i==items.end() || !i->placement || *i->placement!=ItemPlacement{ItemStorageKind::Equipment,1,0})return std::unexpected(E::WrongTool);
 auto actor=r.actor;actor.fishingActive=false;if(!CheckToolRuntime(c,owner,items,{r.tool,{},ToolRuntimeAction::Info,actor}))return std::unexpected(E::Forbidden);
 const auto* tool=std::get_if<ItemToolRow>(&c.Find(i->key)->category);if(!tool || !i->tool || tool->kind!=site.kind || tool->fishingPool!=site.pool)return std::unexpected(E::WrongTool);
 const auto& level=c.FindToolProfile(tool->profile)->levels[i->refinementLevel];if(level.power<site.minimumPower)return std::unexpected(E::WrongTool);
 if(tool->kind==ToolKind::Rod && !i->tool->baitPower)return std::unexpected(E::NoBait);
 return static_cast<std::uint16_t>(std::min<std::uint64_t>(10000,std::uint64_t{site.baseChance}+level.gatherBonusBp+std::uint64_t{level.power/site.powerStep}*site.powerChance+std::uint64_t{i->tool->baitPower}*site.baitChance));
}
std::expected<ResourcePlan,ResourceError> MakeResourceGatheringCommit(const ItemCatalog& c,const GatheringSite& site,std::array<std::uint8_t,16> op,ItemOwnerFence fence,
 std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> items,ResourceRequest r,std::span<const ItemInstanceId> ids,const LootDraw& draw)
{
 if(!r.completed)return std::unexpected(E::NotCompleted);if(items.size()>MaxLoadedOwnerItems)return std::unexpected(E::Invalid);std::set<ItemInstanceId> unique;
 for(const auto& i:items)if(i.owner!=fence.owner || !unique.insert(i.id).second || !ValidatePersistentItem(c,i))return std::unexpected(E::Invalid);
 auto chance=CheckResourceGathering(c,site,fence.owner,items,r);if(!chance)return std::unexpected(chance.error());
 const auto& tool=*std::ranges::find(items,r.tool,&PersistentItem::id);const auto& row=std::get<ItemToolRow>(c.Find(tool.key)->category);const auto& level=c.FindToolProfile(row.profile)->levels[tool.refinementLevel];
 const bool training=tool.tool->trainingPoints<level.requiredPoints && level.practiceNumerator;
 const auto probeRoll=training && level.practiceNumerator<level.practiceDenominator ? 1u : 0u;
 for(const auto& reward:site.rewards)if(auto probe=Build(c,site,op,fence,layouts,items,r,&reward,ids,probeRoll);!probe)return std::unexpected(probe.error());
 if(auto probe=Build(c,site,op,fence,layouts,items,r,nullptr,ids,0);!probe)return std::unexpected(probe.error());
 bool bad=false;std::vector<std::pair<std::uint32_t,std::uint32_t>> decisions;const auto roll=[&](std::uint32_t n){if(!draw || !n){bad=true;return 0u;}auto v=draw(n);if(v>=n){bad=true;return 0u;}decisions.emplace_back(n,v);return v;};
 const bool success=r.eligible && (*chance==10000 || (*chance && roll(10000)<*chance));const GatheringReward* reward=nullptr;std::uint32_t practice=0;
 if(success)
 {
  std::uint32_t total=0;for(const auto& reward:site.rewards)total+=reward.weight;auto index=site.rewards.size()==1 ? 0u : roll(total);
  for(const auto& candidate:site.rewards){if(index<candidate.weight){reward=&candidate;break;}index-=candidate.weight;}
  if(training && level.practiceNumerator<level.practiceDenominator)practice=roll(level.practiceDenominator)+1;
 }
 if(bad)return std::unexpected(E::InvalidDecision);auto result=Build(c,site,op,fence,layouts,items,r,reward,ids,practice);if(!result)return result;
 auto& saved=result->commit.result;saved.insert(saved.end(),{'R','N','G',1,static_cast<std::uint8_t>(decisions.size())});
 for(auto [bound,value]:decisions)for(auto n:{bound,value})for(unsigned i=0;i<4;++i)saved.push_back(static_cast<std::uint8_t>(n>>(i*8)));
 if(!ValidateItemCommit(c,result->commit))return std::unexpected(E::Invalid);return result;
}
std::expected<std::vector<std::byte>,TypedInventoryWireError> EncodeResourceGathering(const ItemCatalog& c,const ResourceView& v)
{
 using W=TypedInventoryWireError;if(static_cast<unsigned>(v.status)>9 || (v.error!=255 && v.error>7) || (v.tool ? !v.nonce : v.nonce || v.site || v.target) || (v.site && !ValidateGatheringSites(c,{v.site,1})))return std::unexpected(W::BadValue);
 std::vector<ItemGrant> extra;if(v.site)for(const auto& r:v.site->rewards)extra.push_back({r.key,0,1});auto metadata=EncodeTypedItemDetails(c,v.tool ? std::span(v.tool,1) : std::span<const PersistentItem>{},false,extra);if(!metadata)return std::unexpected(metadata.error());
 std::vector<std::byte> b;const auto put=[&](std::uint64_t n,unsigned bytes){for(unsigned i=0;i<bytes;++i)b.push_back(std::byte((n>>(8*i))&255));};const auto key=[&](ItemKey k){put(static_cast<unsigned>(k.Category()),2);put(k.LocalId(),4);};
 put(254,1);put(0,4);put(v.fishing ? 4 : 3,1);put(static_cast<unsigned>(v.status),1);put(v.error,1);put(v.nonce,8);put(v.tool ? v.tool->id.Value() : 0,8);put(v.tool ? v.tool->revision : 0,8);
 if(v.tool)key(v.tool->key);else put(0,6);put(v.tool ? v.tool->refinementLevel : 0,2);put(v.tool && v.tool->tool ? v.tool->tool->trainingPoints : 0,4);put(v.tool && v.tool->tool ? v.tool->tool->baitPower : 0,4);
 put(v.site ? v.site->id : 0,4);put(v.target,4);put(v.site ? static_cast<unsigned>(v.site->kind) : 0,1);put(v.site ? v.site->durationMs : 0,4);put(v.remainingMs,4);put(v.chance,2);
 if(v.fishing){const auto& f=*v.fishing;put(f.state.seed,4);put(f.elapsedMs,4);put(f.state.need,1);put(f.state.hits,1);put(f.state.misses,1);put(f.state.sequence,4);put(static_cast<unsigned>(f.state.input),1);put(static_cast<std::uint32_t>(f.castX),4);put(static_cast<std::uint32_t>(f.castY),4);}
 const std::string name=v.site ? v.site->name : "";put(name.size(),2);for(unsigned char ch:name)put(ch,1);
 put(v.result ? (v.result->success ? 1 : 2) : 0,1);put(v.result ? v.result->points : 0,4);put(v.result ? v.result->bait : 0,4);
 put(v.result ? v.result->yields.size() : 0,1);if(v.result)for(const auto& r:v.result->yields){key(r.item);put(r.count,4);}
 put(metadata->size(),4);b.insert(b.end(),metadata->begin(),metadata->end());for(unsigned i=0;i<4;++i)b[i+1]=std::byte((b.size()>>(8*i))&255);return b;
}
}
