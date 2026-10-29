#pragma once
#include "Difficulty.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <random>
#include <vector>
namespace EclipseRaid::ICC {
struct Event {uint32_t id=0,at=0,order=0;uint8_t group=0;};
class EventMap {
 uint32_t clock=0,serial=0;std::vector<Event> events;std::mt19937 random;
public:
 explicit EventMap(uint32_t seed=1):random(seed){}
 void Reset(){events.clear();clock=serial=0;}
 void Update(uint32_t diff){clock+=diff;}
 uint32_t Time()const{return clock;}
 uint32_t Uniform(uint32_t low,uint32_t high){return std::uniform_int_distribution<uint32_t>(low,high)(random);}
 void Schedule(uint32_t id,uint32_t delay){if(events.size()<64)events.push_back({id,clock+delay,serial++});}
 void Schedule(uint32_t id,uint32_t low,uint32_t high,uint8_t group=0){if(events.size()<64)events.push_back({id,clock+Uniform(low,high),serial++,group});}
 void DelayGroupToMax(uint8_t group,uint32_t delay){for(auto& e:events)if(e.group==group&&int32_t(e.at-(clock+delay))<0)e.at=clock+delay;}
 void DelayAll(uint32_t delay){for(auto& e:events)e.at+=delay;}
 void Cancel(uint32_t id){events.erase(std::remove_if(events.begin(),events.end(),[=](auto& event){return event.id==id;}),events.end());}
 void Reschedule(uint32_t id,uint32_t low,uint32_t high){Cancel(id);Schedule(id,low,high);}
 uint32_t Execute(){auto chosen=events.end();for(auto i=events.begin();i!=events.end();++i){if(int32_t(clock-i->at)<0)continue;if(chosen==events.end()||int32_t(i->at-chosen->at)<0||(i->at==chosen->at&&i->order<chosen->order))chosen=i;}if(chosen==events.end())return 0;auto id=chosen->id;events.erase(chosen);return id;}
 size_t Pending()const{return events.size();}
};
struct Candidate {uint32_t id=0;float distance=0;bool alive=true,gm=false,impaled=false,insideBoundary=true;float combatDistance=-1;};
struct Point {float x=0,y=0;};
}
