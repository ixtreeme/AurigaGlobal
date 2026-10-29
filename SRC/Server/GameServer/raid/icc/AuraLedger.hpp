#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <tuple>
#include <vector>

namespace EclipseRaid::ICC {
enum class AuraRemoval:uint8_t {Expired,Dispel,Death,EncounterReset,SourceGone,Script};
struct AuraKey {
 uint32_t spell=0,caster=0,target=0;
 bool operator<(const AuraKey& b)const{return std::tie(spell,caster,target)<std::tie(b.spell,b.caster,b.target);}
};
struct AuraSpec {
 AuraKey key;int32_t duration=0;uint32_t stackLimit=1;
 std::array<uint32_t,3> periods{};
 bool resetPeriodicOnRefresh=false;
};
struct AuraInstance {
 AuraSpec spec;uint64_t token=0,applied=0,expires=0;
 uint32_t stacks=1;bool permanent=false;
 std::array<uint64_t,3> next{};
};
struct AuraEvent {
 AuraInstance aura;bool removed=false;uint8_t effect=0;AuraRemoval reason=AuraRemoval::Expired;uint64_t at=0;
};
// Encounter-owned aura bookkeeping. No timers are attached to ordinary NPCs.
// This is lifecycle machinery, not a generic interpretation of WoW aura IDs:
// each source spell handler must implement damage, immunities and removal effects.
class AuraLedger {
 static constexpr size_t Capacity=512;
 uint64_t clock=0,serial=0;
 std::map<AuraKey,AuraInstance> auras;
 std::vector<AuraEvent> removals;
public:
 const AuraInstance* Find(AuraKey key)const{auto i=auras.find(key);return i==auras.end()?nullptr:&i->second;}
 size_t Size()const{return auras.size();}
 std::vector<AuraInstance> ForTarget(uint32_t target)const{std::vector<AuraInstance> result;for(auto& row:auras)if(row.first.target==target)result.push_back(row.second);return result;}
 uint64_t Time()const{return clock;}
 std::optional<uint64_t> Apply(AuraSpec spec,uint32_t addStacks=1){
  if(!spec.key.spell||!spec.key.target||spec.duration==0||spec.duration< -1||!spec.stackLimit||!addStacks)return std::nullopt;
  auto i=auras.find(spec.key);bool fresh=i==auras.end();if(fresh&&auras.size()>=Capacity)return std::nullopt;
  if(fresh)i=auras.emplace(spec.key,AuraInstance{}).first;
  auto& aura=i->second;auto stacks=fresh?uint64_t(addStacks):uint64_t(aura.stacks)+addStacks;
  // A new token invalidates a pending effect from a refreshed/replaced aura.
  aura.token=++serial;aura.applied=clock;aura.permanent=spec.duration==-1;
  aura.expires=aura.permanent?0:clock+uint32_t(spec.duration);
  aura.stacks=uint32_t(stacks>spec.stackLimit?spec.stackLimit:stacks);
  for(size_t n=0;n<3;n++){
   if(!spec.periods[n])aura.next[n]=0;
   else if(fresh||spec.resetPeriodicOnRefresh||aura.spec.periods[n]!=spec.periods[n])aura.next[n]=clock+spec.periods[n];
  }
  aura.spec=spec;return aura.token;
 }
 bool Remove(AuraKey key,AuraRemoval reason){
  auto i=auras.find(key);if(i==auras.end())return false;
  // Bound deferred callbacks as well as active auras. Callers must drain after
  // reaching the bound; never erase without publishing the removal reason.
  if(removals.size()>=Capacity)return false;
  removals.push_back({i->second,true,0,reason,clock});auras.erase(i);return true;
 }
 size_t RemoveTarget(uint32_t target,AuraRemoval reason){
  std::vector<AuraKey> keys;for(auto& row:auras)if(row.first.target==target)keys.push_back(row.first);
  size_t count=0;for(auto key:keys)if(Remove(key,reason))++count;return count;
 }
 size_t RemoveCaster(uint32_t caster,AuraRemoval reason){
  std::vector<AuraKey> keys;for(auto& row:auras)if(row.first.caster==caster)keys.push_back(row.first);
  size_t count=0;for(auto key:keys)if(Remove(key,reason))++count;return count;
 }
 template<class Callback>size_t Advance(uint32_t elapsed,Callback&& callback,size_t budget=128){
  clock+=elapsed;size_t work=0;
  while(work<budget){
   if(!removals.empty()){
    // Copy before invoking external code. Callbacks may remove/apply auras.
    auto event=removals.front();removals.erase(removals.begin());++work;callback(event);continue;
   }
   auto selected=auras.end();uint64_t due=UINT64_MAX;int effect=-1;
   for(auto i=auras.begin();i!=auras.end();++i){
    auto& aura=i->second;
    for(int n=0;n<3;n++)if(aura.next[n]&&aura.next[n]<=clock&&(aura.permanent||aura.next[n]<=aura.expires)&&aura.next[n]<due){selected=i;due=aura.next[n];effect=n;}
    // At the expiry boundary the last periodic tick precedes removal.
    if(!aura.permanent&&aura.expires<=clock&&aura.expires<due){selected=i;due=aura.expires;effect=-1;}
   }
   if(selected==auras.end())break;
   AuraEvent event{selected->second,effect<0,uint8_t(effect<0?0:effect),AuraRemoval::Expired,due};
   if(effect<0)auras.erase(selected);else selected->second.next[effect]+=selected->second.spec.periods[effect];
   ++work;callback(event);
  }
  return work; // Any overdue work stays queued for the next bounded drain.
 }
};
}
