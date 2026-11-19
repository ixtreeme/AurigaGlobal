/* Adapted from AzerothCore boss_valithria_dreamwalker.cpp.
 * Copyright AzerothCore contributors; GPL-2.0-or-later. */
#pragma once
#include "ICCEventMap.hpp"
namespace EclipseRaid::ICC {
struct ValithriaPort {
 virtual ~ValithriaPort()=default;
 virtual Difficulty Mode()const=0;
 virtual uint32_t SpawnPeriod(unsigned kind)const=0;
 virtual void Wave(unsigned kind,unsigned count)=0;
 virtual void Portals(unsigned count)=0;
 virtual void HealthNotice(unsigned)=0;
 virtual void Victory()=0;
 virtual void Defeat()=0;
};
class Valithria {
 EventMap events;std::array<unsigned,5> ticks{};bool finished=false,high=false,low=false;
 static constexpr uint32_t Offsets[5]={5000,10000,15000,20000,30000};
 static constexpr uint32_t Decay[5]={3000,2000,1000,1000,5000};
public:
 explicit Valithria(uint32_t seed=1):events(seed){}
 bool Finished()const{return finished;}
 void Reset(ValithriaPort& w){events.Reset();ticks={};finished=high=low=false;for(unsigned kind=0;kind<5;kind++)events.Schedule(kind+1,Offsets[kind]+std::max(5000u,w.SpawnPeriod(kind)));events.Schedule(6,45000,48000);if(Heroic(w.Mode()))events.Schedule(7,420000);w.Wave(3,Raid25(w.Mode())?4:4);}
 void Health(ValithriaPort& w,uint64_t hp,uint64_t maximum){if(finished||!maximum)return;if(!hp){finished=true;w.Defeat();}else if(hp>=maximum){finished=true;w.Victory();}else if(hp*100>=maximum*75&&!high){high=true;w.HealthNotice(75);}else if(hp*100<=maximum*25&&!low){low=true;w.HealthNotice(25);}}
 void Tick(ValithriaPort& w,uint32_t diff){if(finished)return;events.Update(diff);auto id=events.Execute();if(id>=1&&id<=5){unsigned kind=id-1;w.Wave(kind,kind==1?6:1);auto count=++ticks[kind];int64_t period=int64_t(w.SpawnPeriod(kind))-int64_t(count-1)*Decay[kind];events.Schedule(id,uint32_t(std::max<int64_t>(5000,period)));}else if(id==6){w.Portals(Raid25(w.Mode())?8:3);events.Schedule(6,45000,48000);}else if(id==7){finished=true;w.Defeat();}}
};
}
