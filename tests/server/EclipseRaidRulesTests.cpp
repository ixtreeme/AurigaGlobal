#include "../../SRC/Server/GameServer/raid/EclipseRaidRules.hpp"
#include <stdexcept>
#include <iostream>
using namespace EclipseRaid;
void Check(bool value){if(!value)throw std::runtime_error("raid rule failed");}
int main(){
 Check(Unlocked(0,1));Check(!Unlocked(0,2));Check(!Unlocked(0,0));Check(!Unlocked(0,13));uint16_t progress=0;for(int n=1;n<=12;n++){Check(Unlocked(progress,n));progress=Clear(progress,n);Check(Cleared(progress,n));}Check(RewardEligible(progress));Check(!RewardEligible(progress^1));Check(!Unlocked(0x40,12));Check(!Unlocked(0x100,12));Check(Unlocked(64|256|1024,12));
 Check(Private(4300001));Check(!Private(4200001));Check(!Private(430));
 Hazard h;h.center={0,0};h.radius=5;Check(Contains(h,{3,4}));Check(!Contains(h,{3,5}));h.shape=Shape::Cone;h.direction={1,0};Check(Contains(h,{4,0}));Check(!Contains(h,{-1,0}));h.shape=Shape::Line;h.length=10;h.radius=1;Check(Contains(h,{9,.9f}));Check(!Contains(h,{11,0}));h.shape=Shape::Ring;h.inner=3;h.radius=5;Check(!Contains(h,{1,1}));Check(Contains(h,{4,0}));
 Scheduler s;int fired=0;s.Add(0,Mechanic::Strike,10,10);s.Poll(100000,[&](Mechanic){fired++;});Check(fired==1);s.Poll(100001,[&](Mechanic){fired++;});Check(fired==1);s.Reset();s.Poll(200000,[&](Mechanic){fired++;});Check(fired==1);
 for(int n=1;n<=12;n++){Schedule(s,n,0);Check(s.timers.size()>=3&&s.timers.size()<32);int events=0;s.Poll(90000,[&](Mechanic){events++;});Check(events>=3);}
 Check(Due(10,0xfffffff0));Check(!Due(0xfffffff0,10));Check(BossHealth(30,12)>BossHealth(1,1));std::cout<<"ECLIPSE_RULES_PASS progression geometry timer bounds all_12_encounters\n";
}
