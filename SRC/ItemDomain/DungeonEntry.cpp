#include <item/DungeonEntry.hpp>
#include <item/NativeMoney.hpp>
#include <algorithm>
#include <set>
#include <map>
namespace auriga::item {
namespace {
using E=ItemPersistenceError;
template<class R>bool Nonzero(const R& r){return std::ranges::any_of(r,[](auto v){return v!=0;});}
bool Gate(std::string_view s){return !s.empty() && s.size()<=64 && s.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789._")==s.npos;}
ItemCommit Base(const ItemCatalog& c,std::array<std::uint8_t,16> operation,DungeonEntryMutation m,std::span<const ItemOwnerFence> owners){ItemCommit r;r.operationId=operation;r.catalogRevision=c.Revision();r.catalogHash=c.ContentHash();r.kind=ItemOperationKind::Service;r.owners.assign(owners.begin(),owners.end());r.outcomeCode=DungeonEntryReserveOutcome+static_cast<unsigned>(m.kind)-1;r.result={'D','E','N',1,static_cast<std::uint8_t>(m.kind)};r.dungeonEntry=std::move(m);return r;}
}
std::vector<std::uint8_t> EncodeDungeonEntryMutation(const DungeonEntryMutation& m){
 std::vector<std::uint8_t> b{'D','E','M',1};const auto put=[&](std::uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)b.push_back(static_cast<std::uint8_t>(v>>(8*i)));};put(static_cast<unsigned>(m.kind),1);b.insert(b.end(),m.reservation.begin(),m.reservation.end());put(m.gate.size(),1);b.insert(b.end(),m.gate.begin(),m.gate.end());put(m.tickets.size(),1);
 auto tickets=m.tickets;std::ranges::sort(tickets,{},&DungeonEntryTicket::owner);for(const auto& t:tickets){put(static_cast<unsigned>(t.key.Category()),2);put(t.key.LocalId(),4);put(t.level,2);for(auto n:{t.owner,t.session,t.instance,t.revision,t.createdAtMs})put(n,8);put(t.count,4);put(t.units,4);b.insert(b.end(),t.stateHash.begin(),t.stateHash.end());}return b;
}
std::uint64_t DungeonEntryCleanupLimit(const ItemCatalog& c,std::string_view gate,ItemKey key){
 const auto* e=c.Find(key);if(!e)return 0;const auto& code=e->base.contentCode;
 if(gate=="dungeon.catacombe.entry" && code=="catalog.3.grimace_totem_70167e34baa5")return UINT32_MAX;
 if(gate=="dungeon.spider.entry" && code=="catalog.10.arachnids_whistle_839ac29f7f62")return UINT32_MAX;
 if(gate=="dungeon.nemere.entry" && (code=="catalog.10.frost_key_acf1fafd561d" || code=="catalog.10.north_star_2f46eb5f23d3"))return UINT32_MAX;
 if(gate=="dungeon.slime.entry" && (code=="catalog.3.slime_vial_empty_e5f2501c8a3f" || code=="catalog.3.vial_full_of_slime_2232dc315396"))return UINT32_MAX;
 if(key.Category()!=ItemCategory::Quest)return 0;
 if(gate=="dungeon.rune.entry" && (code=="dungeon.rune.fragment" || code=="dungeon.rune.floor_key" || code=="catalog.10.ticket_of_the_runes_78bafde9234a"))return UINT32_MAX;
 if(code=="dungeon.orcs.key"){if(gate=="dungeon.pyramid.entry")return 255;if(gate=="dungeon.orcs.entry" || gate=="dungeon.triton.entry")return UINT32_MAX;}
 return 0;
}
bool ValidDungeonEntryMutation(const ItemCatalog& c,const ItemCommit& r){
 if(!r.dungeonEntry)return r.outcomeCode<DungeonEntryReserveOutcome || r.outcomeCode>DungeonEntryReleaseOutcome;
 const auto& m=*r.dungeonEntry;const bool reserve=m.kind==DungeonEntryMutationKind::Reserve,consume=m.kind==DungeonEntryMutationKind::Consume;
 if(m.kind<DungeonEntryMutationKind::Reserve || m.kind>DungeonEntryMutationKind::Release || !Nonzero(m.reservation) || !Gate(m.gate) || m.tickets.empty() || m.tickets.size()>8 || r.owners.size()!=m.tickets.size() || r.kind!=ItemOperationKind::Service || r.outcomeCode!=DungeonEntryReserveOutcome+static_cast<unsigned>(m.kind)-1 || r.result!=std::vector<std::uint8_t>{'D','E','N',1,static_cast<std::uint8_t>(m.kind)} || (reserve?m.reservation!=r.operationId:m.reservation==r.operationId) || !r.wallets.empty() || r.recovery || r.learning || r.ground || r.currencyDrop || r.currencyAward || r.privateShop || r.petStudyCooldown)return false;
 std::set<std::uint64_t> owners,ids;
 for(const auto& t:m.tickets){const auto* e=c.Find(t.key);if(!e || !c.FindRefinement(e->base.refinementProfile) || !c.FindRefinement(e->base.refinementProfile)->FindLevel(t.level) || e->base.retired || !t.owner || t.owner>UINT32_MAX || !t.session || !t.instance || !t.revision || t.revision==UINT64_MAX || !t.createdAtMs || t.createdAtMs>INT64_MAX || !t.units || t.units>50000 || t.units>t.count || t.count>e->base.maxStack || !Nonzero(t.stateHash) || !owners.insert(t.owner).second || !ids.insert(t.instance).second)return false;
  const auto f=std::ranges::find_if(r.owners,[&](const auto& f){return f.owner==ItemOwner{ItemOwnerKind::Character,t.owner};});if(f==r.owners.end() || (m.kind==DungeonEntryMutationKind::Release?f->sessionGeneration<t.session:f->sessionGeneration!=t.session))return false;
  const auto ch=std::ranges::find_if(r.items,[&](const auto& ch){return ch.after.id.Value()==t.instance;});
  if(!consume){if(ch!=r.items.end())return false;continue;}
  if(ch==r.items.end() || !ch->before)return false;const auto& b=*ch->before;const auto hash=PersistentItemDigest(c,b);if(!hash || *hash!=t.stateHash || b.owner!=f->owner || b.key!=t.key || b.refinementLevel!=t.level || b.revision!=t.revision || b.count!=t.count || !PlainInventoryItem(b,f->owner))return false;auto after=b;++after.revision;after.count-=t.units;if(!after.count){after.destroyed=true;after.placement.reset();}if(after!=ch->after)return false;
 }
 std::map<std::pair<std::uint64_t,ItemKey>,std::uint64_t> purged;
 for(const auto& ch:r.items){if(consume && ids.contains(ch.after.id.Value()) || MoneyPassiveAccessoryDebit(ch))continue;
  if(!consume || !ch.before || !owners.contains(ch.before->owner.id) || !PlainInventoryItem(*ch.before,ch.before->owner))return false;
  const auto& b=*ch.before;const auto limit=DungeonEntryCleanupLimit(c,m.gate,b.key);if(!limit || ch.after.count>=b.count)return false;auto after=b;++after.revision;after.count=ch.after.count;if(!after.count){after.destroyed=true;after.placement.reset();}if(after!=ch.after)return false;
  auto& total=purged[{b.owner.id,b.key}];total+=b.count-ch.after.count;if(total>limit)return false;
 }return true;
}
std::expected<ItemCommit,E> MakeDungeonEntryReservation(const ItemCatalog& c,std::array<std::uint8_t,16> operation,std::string_view gate,ItemKey ticket,std::uint16_t level,std::uint32_t units,std::uint64_t now,std::span<const ItemOwnerFence> owners,std::span<const PersistentItem> items){
 return MakeDungeonEntryReservation(c,operation,gate,std::span(&ticket,1),level,units,now,owners,items);
}
std::expected<ItemCommit,E> MakeDungeonEntryReservation(const ItemCatalog& c,std::array<std::uint8_t,16> operation,
 std::string_view gate,std::span<const ItemKey> tickets,std::uint16_t level,std::uint32_t units,std::uint64_t now,
 std::span<const ItemOwnerFence> owners,std::span<const PersistentItem> items)
{
 if(tickets.empty() || tickets.size()>2)return std::unexpected(E::InvalidItem);
 DungeonEntryMutation m;m.kind=DungeonEntryMutationKind::Reserve;m.reservation=operation;m.gate=gate;std::set<std::uint64_t> ids;
 for(const auto& i:items)if(!ids.insert(i.id.Value()).second)return std::unexpected(E::InvalidChange);
 for(const auto& owner:owners)
 {
  const PersistentItem* selected=nullptr;
  for(const auto key:tickets)
  {
   for(const auto& item:items)
    if(item.key==key && item.refinementLevel==level && item.count>=units &&
       PlainInventoryItem(item,owner.owner) && (!selected || item.id<selected->id))
     selected=&item;
   if(selected)break;
  }
  if(!selected)return std::unexpected(E::RequirementNotMet);
  const auto hash=PersistentItemDigest(c,*selected);
  if(!hash)return std::unexpected(hash.error());
  m.tickets.push_back({selected->key,level,owner.owner.id,owner.sessionGeneration,selected->id.Value(),
   selected->revision,now,selected->count,units,*hash});
 }
 auto r=Base(c,operation,std::move(m),owners);if(auto v=ValidateItemCommit(c,r);!v)return std::unexpected(v.error());return r;
}
std::expected<ItemCommit,E> MakeDungeonEntryResolution(const ItemCatalog& c,std::array<std::uint8_t,16> operation,const DungeonEntryMutation& reservation,bool consume,std::span<const ItemOwnerFence> owners,std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> items){
 auto m=reservation;m.kind=consume?DungeonEntryMutationKind::Consume:DungeonEntryMutationKind::Release;auto r=Base(c,operation,std::move(m),owners);
 if(consume)for(const auto& t:r.dungeonEntry->tickets){const auto before=std::ranges::find_if(items,[&](const auto& i){return i.id.Value()==t.instance;});if(before==items.end() || before->count<t.units)return std::unexpected(E::MissingItem);auto after=*before;++after.revision;after.count-=t.units;if(!after.count){after.destroyed=true;after.placement.reset();}r.items.push_back({*before,after});}
 if(consume){std::set<std::uint64_t> primary;for(const auto& t:r.dungeonEntry->tickets)primary.insert(t.instance);std::map<std::pair<std::uint64_t,ItemKey>,std::uint64_t> purged;
  for(const auto& before:items){if(primary.contains(before.id.Value()) || !PlainInventoryItem(before,before.owner) || std::ranges::none_of(owners,[&](const auto& f){return f.owner==before.owner;}))continue;const auto limit=DungeonEntryCleanupLimit(c,r.dungeonEntry->gate,before.key);auto& used=purged[{before.owner.id,before.key}];if(!limit || used>=limit)continue;auto after=before;++after.revision;const auto units=std::min<std::uint64_t>(before.count,limit-used);used+=units;after.count-=static_cast<std::uint32_t>(units);if(!after.count){after.destroyed=true;after.placement.reset();}r.items.push_back({before,after});}
 }
 for(const auto& l:layouts)if(std::ranges::any_of(r.items,[&](const auto& ch){const auto match=[&](const auto& i){return i.placement && l.storage==ItemStorage{i.owner,i.placement->kind,i.placement->storageId};};return match(ch.after) || ch.before && match(*ch.before);}))r.storages.push_back(l);
 if(auto v=ValidateItemCommit(c,r);!v)return std::unexpected(v.error());return r;
}
}
