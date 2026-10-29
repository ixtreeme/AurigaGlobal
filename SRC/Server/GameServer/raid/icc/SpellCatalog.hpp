#pragma once
#include "Difficulty.hpp"
#include <algorithm>
#include <array>
#include <cstddef>

namespace EclipseRaid::ICC {
struct SpellEffect {
 uint32_t kind=0,aura=0;int32_t basePoints=0,dieSides=0;
 uint32_t period=0;int32_t trigger=0;uint32_t targetA=0,targetB=0;
};
struct SpellRecord {
 uint32_t id=0;std::array<int32_t,4> variants{};
 int32_t castTime=0,duration=0;uint32_t maximumTargets=0,stackLimit=0,school=0;
 std::array<SpellEffect,3> effects{};
 float missileSpeed=0;
 std::array<float,3> radius{};
};
class SpellCatalog {
 const SpellRecord* records;size_t count;
public:
 // Records must be unique and sorted. The generator validates that invariant.
 SpellCatalog(const SpellRecord* data,size_t size):records(data),count(size){}
 const SpellRecord* Find(uint32_t id)const{
  if(!count)return nullptr;
  auto p=std::lower_bound(records,records+count,id,[](auto& row,uint32_t key){return row.id<key;});
  return p!=records+count&&p->id==id?p:nullptr;
 }
 // Adapted from SpellMgr::GetSpellIdForDifficulty / GetSpellForDifficultyFromSpell.
 uint32_t ResolveID(uint32_t id,Difficulty difficulty)const{
  auto source=Find(id);if(!source)return id;
  auto mode=uint32_t(difficulty);if(mode>=4)return id;
  auto selected=source->variants[mode];
  if(selected<=0&&mode>1)selected=source->variants[mode-2];
  return selected>0?uint32_t(selected):id;
 }
 const SpellRecord* Resolve(uint32_t id,Difficulty difficulty)const{
  auto resolved=Find(ResolveID(id,difficulty));return resolved?resolved:Find(id);
 }
};
}
