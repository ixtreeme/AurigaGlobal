/* Source: boss_lord_marrowgar.cpp spell_marrowgar_coldflame*.
 * AzerothCore contributors, GPL-2.0-or-later; see docs/eclipse_raid.
 */
#pragma once
#include "ICCEventMap.hpp"
namespace EclipseRaid::ICC {
struct FlameTarget {
 uint32_t id=0;Point position;float combatReach=0;
 bool alive=true,impaled=false;
};
struct FlameSpawn {Point position;float heading=0;uint32_t summonSpell=0;};
struct MarrowgarFlamePort {
 virtual ~MarrowgarFlamePort()=default;
 virtual bool LineOfSight(Point destination)const=0;
 virtual void SpawnFlame(const FlameSpawn& flame)=0;
};
inline float PointDistance(Point a,Point b){return std::hypot(a.x-b.x,a.y-b.y);}
inline Point Along(Point p,float heading,float distance){return {p.x+std::cos(heading)*distance,p.y+std::sin(heading)*distance};}
// UnitAI.h DefaultTargetSelector(-1, playerOnly, withMainTank=false,
// -SPELL_IMPALED) uses Unit::IsWithinCombatRange: reach1 + reach2 + 1 metre.
// Coordinates are the Auriga encounter plane. The fallback drops these filters.
inline std::optional<FlameTarget> SelectColdflameTarget(EventMap& random,Point boss,float bossReach,uint32_t victim,const std::vector<FlameTarget>& targets){
 std::vector<FlameTarget> outside,all;
 for(const auto& t:targets)if(t.alive){all.push_back(t);if(t.id!=victim&&!t.impaled&&PointDistance(boss,t.position)>=1+bossReach+t.combatReach)outside.push_back(t);}
 auto& pool=outside.empty()?all:outside;if(pool.empty())return std::nullopt;
 return pool[random.Uniform(0,uint32_t(pool.size()-1))];
}
inline bool CastColdflame(MarrowgarFlamePort& world,Point boss,float bossSize,Point target,uint32_t summonSpell){
 float heading=std::atan2(target.y-boss.y,target.x-boss.x);
 auto start=Along(boss,heading,bossSize*.5f);
 if(!world.LineOfSight(start))start=Along(boss,heading,.5f);
 if(!world.LineOfSight(start))return false;
 world.SpawnFlame({start,heading,summonSpell});return true;
}
inline uint32_t CastStormColdflame(MarrowgarFlamePort& world,Point boss,uint32_t firstSummonSpell){
 uint32_t count=0;constexpr float pi=3.14159265358979323846f;
 for(uint32_t i=0;i<4;i++){float heading=pi/4+i*pi/2;auto start=Along(boss,heading,2.5f);if(world.LineOfSight(start)){world.SpawnFlame({start,heading,firstSummonSpell+i});++count;}}
 return count;
}
}
