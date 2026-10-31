#pragma once
// Instance-owned skybridge siege. See icc/Gunship.hpp for source attribution.
class GunshipRuntime final:public SourceRuntime,public ICC::GunshipPort {
 Run& run;entt::entity boss,mage=entt::null,firingCannon=entt::null;ICC::Gunship controller;ICC::EventMap random;
 uint32_t now,last;bool failed=false;std::map<uint32_t,uint32_t> repairAt;
 struct Add {entt::entity actor;uint32_t next,born;bool sergeant;};std::vector<Add> adds;
 struct Rocket {Vec point;uint32_t at;};std::vector<Rocket> rockets;struct Impact {uint32_t at;entt::entity cannon;};std::vector<Impact> impacts;
 std::vector<entt::entity> cannons;
 entt::entity Resolve(uint32_t pid){for(auto& m:run.members)if(m.pid==pid)return Player(m);return entt::null;}
 void Fx(uint32_t spell,int stage,entt::entity source,entt::entity target,Vec at,uint32_t life=1500,float radius=0){std::ostringstream line;line<<"er_dwfx "<<run.id<<' '<<run.nextHazard++<<' '<<spell<<' '<<stage<<' '<<(Valid(source)?P::GetPacketVID(source):0)<<' '<<(Valid(target)?P::GetPacketVID(target):0)<<' '<<BaseX+int(at.x*100)<<' '<<BaseY+int(at.y*100)<<' '<<life<<' '<<int(radius*100);Broadcast(run,line.str());}
 void Damage(entt::entity p,int64_t amount,entt::entity source){if(Alive(run,p)&&Valid(source)&&amount>0)CombatSystem::Damage(p,source,amount,DAMAGE_TYPE_SPECIAL);}
 int64_t Value(uint32_t id,int64_t fallback){auto s=ICC::Spells().Resolve(id,run.mode);if(!s)return fallback;for(auto& e:s->effects)if((e.kind==2||e.kind==9)&&e.basePoints>0)return int64_t(e.basePoints)+std::max(1,e.dieSides);return fallback;}
 entt::entity Nearest(entt::entity actor,float range){entt::entity result=entt::null;for(auto p:Living(run)){auto d=Distance(Position(actor),Position(p));if(d<range){result=p;range=d;}}return result;}
public:
 explicit GunshipRuntime(Run& r):run(r),boss(r.bosses.front()),controller(r.id^0x317),random(r.id^0x318),now(get_dword_time()),last(now){
  run.phase=1;M::Stop(boss);controller.Reset(*this,uint64_t(Points::GetMaxHP(boss)));
  for(float y:{-12.f,0.f,12.f}){Vec at{Arenas[2].center.x-13,Arenas[2].center.y+y};auto e=Spawn(run,62023,at,2,0,0);if(Valid(e))cannons.push_back(e);else failed=true;}
  Notice(run,"ER_SIEGE_BEGIN");
 }
 ICC::Difficulty Mode()const override{return run.mode;}
 void BoardingWave(unsigned count)override{for(unsigned i=0;i<count&&adds.size()<24;i++){Vec at{Arenas[2].center.x+22,Arenas[2].center.y-14+float(i)*5};auto e=Spawn(run,62024,at,1,0,i==0?36960:36957);if(Valid(e))adds.push_back({e,now+2000,now,i==0});else failed=true;}Notice(run,"ER_SIEGE_BOARDERS");}
 void SummonMage()override{Vec at{Arenas[2].center.x+15,Arenas[2].center.y+15};mage=Spawn(run,62025,at,1,0,37117);if(!Valid(mage))failed=true;else{M::SendMovePacket(mage,FUNC_MOB_SKILL,0,P::GetX(mage),P::GetY(mage),0);Fx(69705,3,mage,mage,at,600000);}Notice(run,"ER_SIEGE_MAGE");}
 void Frozen(bool value)override{for(auto e:cannons)if(Valid(e))Fx(69705,value?3:4,mage,e,Position(e),600000);}
 void RocketBarrage()override{auto list=Living(run);if(list.empty()||rockets.size()>=12)return;auto p=list[random.Uniform(0,uint32_t(list.size()-1))];auto at=Position(p);rockets.push_back({at,now+3000});Telegraph(run,Mechanic::Siege,at,4,0,3000,1);Fx(69678,1,boss,p,at,3000);}
 void CannonHit(uint32_t,int64_t damage,bool charged)override{if(!Valid(firingCannon))return;auto cannon=firingCannon;Fx(charged?69193:69678,1,cannon,boss,Position(boss),750);Fx(69193,2,cannon,cannon,Position(cannon),350);if(impacts.size()<16)impacts.push_back({now+750,cannon});P::SetHP(boss,std::max<int64_t>(1,int64_t(controller.Enemy())-damage));}
 void Victory()override{Fx(72137,2,boss,boss,Position(boss),3000);run.completionPending=true;Notice(run,"ER_SIEGE_VICTORY");}
 void Defeat()override{failed=true;Fx(72134,2,boss,boss,Arenas[2].center,4000,12);Notice(run,"ER_SIEGE_DESTROYED");}
 void Attacked(entt::entity,int64_t)override{}
 int64_t TargetDamage(entt::entity,entt::entity victim,int64_t amount)override{return victim==boss?0:amount;}
 void Died(entt::entity actor)override{if(actor==mage){mage=entt::null;controller.MageKilled(*this);Notice(run,"ER_SIEGE_THAWED");}}
 bool Action(entt::entity p,const std::string& action,uint32_t target)override{
  if(action!="interact"&&action!="cannon"&&action!="blast"&&action!="repair")return false;
  entt::entity cannon=entt::null;float distance=6;for(auto e:cannons)if(Valid(e)&&(!target||P::GetPacketVID(e)==target)){auto d=Distance(Position(p),Position(e));if(d<distance){cannon=e;distance=d;}}
  if(!Valid(cannon)){Notice(p,"ER_SIEGE_NEAR_CANNON");return true;}
  if(action=="repair"){auto pid=P::GetPlayerID(p);if(!Due(now,repairAt[pid])||P::GetSP(p)<100){Notice(p,"ER_SIEGE_REPAIR_WAIT");return true;}Points::Change(p,POINT_SP,-100);repairAt[pid]=now+10000;RepairHull();return true;}
  firingCannon=cannon;if(!controller.Fire(*this,P::GetPacketVID(cannon),P::GetPlayerID(p),action=="blast"))Notice(p,controller.IsFrozen()?"ER_SIEGE_CANNON_FROZEN":"ER_SIEGE_CANNON_WAIT");return true;
 }
 void RepairHull(){controller.Repair(controller.Maximum()/50);}
 void Status(entt::entity p)override{uint32_t nearest=0;unsigned heat=0;float distance=6;for(auto e:cannons)if(Valid(e)){float d=Distance(Position(p),Position(e));if(d<distance){distance=d;nearest=P::GetPacketVID(e);heat=controller.Heat(nearest);}}
  ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"er_siege_target %u %u",run.id,P::GetPacketVID(boss));
  ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"er_siege %u %llu %llu %llu %u %u %u",run.id,(unsigned long long)controller.Friendly(),(unsigned long long)controller.Enemy(),(unsigned long long)controller.Maximum(),controller.IsFrozen()?1:0,nearest,heat);
 }
 bool Update(uint32_t time)override{now=time;auto elapsed=now-last;last=now;controller.Tick(*this,elapsed);
  for(size_t i=0;i<impacts.size();)if(Due(now,impacts[i].at)){Fx(69193,2,impacts[i].cannon,boss,Position(boss),1800,5);impacts.erase(impacts.begin()+i);}else ++i;
  if(Valid(mage)&&CombatSystem::IsDead(mage)){mage=entt::null;controller.MageKilled(*this);Notice(run,"ER_SIEGE_THAWED");}
  for(size_t i=0;i<rockets.size();){if(!Due(now,rockets[i].at)){++i;continue;}auto point=rockets[i].point;Fx(69193,5,boss,entt::null,point,1800,4);for(auto p:Living(run))if(Distance(Position(p),point)<=4)Damage(p,Value(69193,8000),boss);controller.DamageHull(*this,controller.Maximum()/200);rockets.erase(rockets.begin()+i);}
  for(size_t i=0;i<adds.size();){auto& a=adds[i];if(!Valid(a.actor)||CombatSystem::IsDead(a.actor)||now-a.born>180000){Destroy(a.actor);adds.erase(adds.begin()+i);continue;}
   if(Due(now,a.next)){a.next=now+2000;auto p=Nearest(a.actor,12);unsigned veteran=1+std::min(3u,(now-a.born)/20000);if(Valid(p)){if(Distance(Position(p),Position(a.actor))>3){M::Goto(a.actor,P::GetX(p),P::GetY(p));}else{M::Stop(a.actor);M::SetRotationToXY(a.actor,P::GetX(p),P::GetY(p));M::SendMovePacket(a.actor,FUNC_ATTACK,0,P::GetX(a.actor),P::GetY(a.actor),0);Damage(p,CalcMeleeDamage(a.actor,p)*veteran,a.actor);if(a.sergeant){Fx(69652,2,a.actor,p,Position(p),1400);}}}
    else{auto at=Position(cannons.empty()?boss:cannons.front());if(Distance(Position(a.actor),at)>4)M::Goto(a.actor,BaseX+int(at.x*100),BaseY+int(at.y*100));else{M::SendMovePacket(a.actor,FUNC_ATTACK,0,P::GetX(a.actor),P::GetY(a.actor),0);controller.DamageHull(*this,controller.Maximum()*veteran/300);}}
   }++i;
  }return !failed;
 }
};
