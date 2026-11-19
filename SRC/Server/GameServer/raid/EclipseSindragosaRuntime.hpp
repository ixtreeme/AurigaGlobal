#pragma once
// Source spell/timing adapter; all temporary combat state belongs to this run.
class SindragosaRuntime final:public FestergutRuntime,public ICC::SindragosaPort {
 ICC::Sindragosa dragon;bool air=false;uint32_t nextPulse=0,nextAura=0,nextChill=0,coldAt=0;
 struct Debuff {uint32_t unchained=0,backlash=0,buffetEnd=0,chillEnd=0,lastSkill=0;unsigned instability=0,buffet=0,chill=0;};
 std::map<uint32_t,Debuff> debuffs;
 struct Tomb {entt::entity actor=entt::null;uint32_t pid=0,at=0,asphyx=0;Vec point;};std::vector<Tomb> tombs;
 struct Bomb {Vec point;uint32_t at;};std::vector<Bomb> bombs;
 std::vector<entt::entity> Choices(){auto list=Living(run);auto tank=Tank();if(list.size()>1)list.erase(std::remove_if(list.begin(),list.end(),[&](auto p){return P::GetPlayerID(p)==tank;}),list.end());list.erase(std::remove_if(list.begin(),list.end(),[&](auto p){return Trapped(P::GetPlayerID(p));}),list.end());for(size_t i=list.size();i>1;i--)std::swap(list[i-1],list[random.Uniform(0,uint32_t(i-1))]);return list;}
 bool Trapped(uint32_t pid)const{for(auto& t:tombs)if(t.pid==pid&&Valid(t.actor)&&!CombatSystem::IsDead(t.actor))return true;return false;}
 bool Covered(Vec from,Vec to)const{auto dx=to.x-from.x,dy=to.y-from.y;auto length=dx*dx+dy*dy;if(length<.01f)return false;for(auto& t:tombs)if(Valid(t.actor)&&!CombatSystem::IsDead(t.actor)){float u=((t.point.x-from.x)*dx+(t.point.y-from.y)*dy)/length;if(u<=0||u>1.05f)continue;Vec closest{from.x+u*dx,from.y+u*dy};if(Distance(closest,t.point)<=2.2f)return true;}return false;}
 void Hit(entt::entity p,int64_t amount){auto& d=debuffs[P::GetPlayerID(p)];Damage(p,amount*(100+15*d.buffet)/100*(berserk?6:1));}
 void Free(Tomb& t,bool destroy=true){auto p=Resolve(t.pid);run.rooted.erase(t.pid);if(Present(run,p)){ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"er_root 0");Fx(70157,4,p,Position(p),0);Fx(70126,4,p,Position(p),0);if(auto m=Participant(run,t.pid)){m->aura=Mechanic::Strike;m->auraEnd=0;}}if(destroy)Destroy(t.actor);t.actor=entt::null;t.pid=0;}
 void MeleeStrike(){if(air||coldAt||!Due(now,nextMelee))return;auto p=Resolve(Tank());if(!Alive(run,p)||Trapped(P::GetPlayerID(p)))return;nextMelee=now+2000;if(Distance(Position(p),Position(boss))>6)M::Goto(boss,P::GetX(p),P::GetY(p));else{M::Stop(boss);M::SetRotationToXY(boss,P::GetX(p),P::GetY(p));M::SendMovePacket(boss,FUNC_ATTACK,0,P::GetX(boss),P::GetY(boss),0);Damage(p,CalcMeleeDamage(boss,p)*(berserk?6:1));}}
public:
 explicit SindragosaRuntime(Run& r):FestergutRuntime(r,false),dragon(r.id^0xb01){dragon.Reset(*this);Fx(70084,3,boss,Position(boss),600000);Notice(run,"ER_SINDRA_BEGIN");}
 ~SindragosaRuntime()override{for(auto& t:tombs)Free(t);if(Valid(boss))Broadcast(run,"er_lift "+std::to_string(P::GetPacketVID(boss))+" 0");}
 ICC::Difficulty Mode()const override{return run.mode;}
 void Flight(bool value)override{air=value;M::Stop(boss);Broadcast(run,"er_lift "+std::to_string(P::GetPacketVID(boss))+" "+(air?"1400":"0"));Notice(run,air?"ER_SINDRA_AIR":"ER_SINDRA_LAND");}
 void FinalPhase()override{run.phase=3;Notice(run,"ER_SINDRA_FINAL");}
 void Ability(uint32_t id)override{
  if(id==26662){berserk=true;Fx(id,3,boss,Position(boss),600000);Notice(run,"ER_ENRAGE");return;}
  if(id==70126){auto list=Choices();auto count=std::min<size_t>(list.size(),ICC::Sindragosa::TombCount(run.mode,air));for(size_t i=0;i<count&&tombs.size()<24;i++){auto p=list[i];auto pid=P::GetPlayerID(p);bool pending=false;for(auto& t:tombs)if(t.pid==pid)pending=true;if(pending)continue;tombs.push_back({entt::null,pid,now+7000,0,Position(p)});Fx(id,3,p,Position(p),7000);Marker(p,7000);}Notice(run,"ER_SINDRA_BEACON");return;}
  if(id==69845){auto c=Arenas[10].center;Vec point{c.x+float(int(random.Uniform(0,48))-24),c.y+float(int(random.Uniform(0,48))-24)};if(bombs.size()<8)bombs.push_back({point,now+5000});Emit(point,5000,1000,int(Mechanic::FrostBomb),8);Fx(70022,5,entt::null,point,5000,8);Notice(run,"ER_SINDRA_BOMB");return;}
  if(id==70128){for(auto p:Living(run)){auto pid=P::GetPlayerID(p);auto& d=debuffs[pid];if(Trapped(pid)||Covered(Position(boss),Position(p)))continue;d.buffet=std::min(99u,d.buffet+1);d.buffetEnd=now+8000;Fx(id,3,p,Position(p),8000);}return;}
  if(id==69762){auto list=Choices();auto count=std::min<size_t>(list.size(),ICC::Raid25(run.mode)?6:2);for(size_t i=0;i<count;i++){auto p=list[i];debuffs[P::GetPlayerID(p)].unchained=now+30000;Fx(id,3,p,Position(p),30000);}Notice(run,"ER_SINDRA_UNCHAINED");return;}
  if(id==70117){auto c=Position(boss);for(auto p:Living(run))if(!Trapped(P::GetPlayerID(p))){Vec at{c.x+float(int(random.Uniform(0,4))-2),c.y+float(int(random.Uniform(0,4))-2)};Place(run,p,at);Fx(id,2,p,at,1000);}Notice(run,"ER_SINDRA_GRIP");return;}
  if(id==70123){coldAt=now+5000;Fx(id,0,boss,Position(boss),5000);Emit(Position(boss),5000,750,int(Mechanic::FrostBomb),25);Notice(run,"ER_SINDRA_COLD");return;}
  auto tank=Resolve(Tank());if(!Alive(run,tank))return;M::Stop(boss);M::SetRotationToXY(boss,P::GetX(tank),P::GetY(tank));M::SendMovePacket(boss,FUNC_MOB_SKILL,0,P::GetX(boss),P::GetY(boss),0);Fx(id,2,tank,Position(tank));
  auto from=Position(boss),front=Position(tank);float length=std::max(.01f,Distance(from,front));float dx=(front.x-from.x)/length,dy=(front.y-from.y)/length;
  for(auto p:Living(run)){auto at=Position(p);float distance=Distance(from,at);float dot=((at.x-from.x)*dx+(at.y-from.y)*dy)/std::max(.01f,distance);bool hit=id==71077?distance<20&&dot<-.5f:distance<(id==19983?8:40)&&dot>.7f;if(hit&&!Trapped(P::GetPlayerID(p)))Hit(p,Value(id));}
 }
 void Skill(entt::entity p,uint32_t)override{auto pid=P::GetPlayerID(p);auto& d=debuffs[pid];d.lastSkill=now+2000;if(d.unchained&&!Due(now,d.unchained)){d.instability=std::min(99u,d.instability+1);d.backlash=now+5000;Fx(69766,3,p,Position(p),5000);}}
 void Attacked(entt::entity p,int64_t amount)override{FestergutRuntime::Attacked(p,amount);auto& d=debuffs[P::GetPlayerID(p)];if(amount>0&&(!d.lastSkill||Due(now,d.lastSkill))&&random.Uniform(0,99)<20){d.chill=std::min(99u,d.chill+1);d.chillEnd=now+8000;Fx(70106,3,p,Position(p),8000);}}
 int64_t TargetDamage(entt::entity,entt::entity target,int64_t amount)override{return target==boss&&air?0:amount;}
 void Died(entt::entity actor)override{for(auto& t:tombs)if(t.actor==actor||Resolve(t.pid)==actor)Free(t,t.actor!=actor);}
 void Status(entt::entity p)override{auto& d=debuffs[P::GetPlayerID(p)];ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"er_sindra %u %u %u %u %u %u",run.id,dragon.Final()?3:air?2:1,d.instability,d.chill,d.buffet,d.unchained&&!Due(now,d.unchained)?(d.unchained-now+999)/1000:0);ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"er_lift %u %u",P::GetPacketVID(boss),air?1400:0);}
 bool Update(uint32_t time)override{now=time;auto diff=now-last;last=now;if(!Valid(boss)||CombatSystem::IsDead(boss))return true;dragon.Health(*this,unsigned(uint64_t(P::GetHP(boss))*100/std::max<int64_t>(1,Points::GetMaxHP(boss))));dragon.Tick(*this,diff);
  for(size_t i=0;i<tombs.size();){auto& t=tombs[i];auto p=Resolve(t.pid);if(!Alive(run,p)||(t.actor!=entt::null&&(!Valid(t.actor)||CombatSystem::IsDead(t.actor)))){Free(t);tombs.erase(tombs.begin()+i);continue;}if(t.actor==entt::null&&Due(now,t.at)){t.point=Position(p);t.actor=Spawn(run,62015,t.point,3,t.pid,36980);if(!Valid(t.actor)){Free(t);tombs.erase(tombs.begin()+i);continue;}t.asphyx=now+22500;run.rooted.insert(t.pid);M::Stop(p);ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"er_root 1");Aura(run,p,Mechanic::IceTomb,600000);Fx(70157,3,p,t.point,600000);}++i;}
  for(size_t i=0;i<bombs.size();)if(Due(now,bombs[i].at)){auto point=bombs[i].point;Fx(69845,5,entt::null,point,2000,8);for(auto p:Living(run))if(!Trapped(P::GetPlayerID(p))&&!Covered(point,Position(p)))Hit(p,Value(69845));bombs.erase(bombs.begin()+i);}else ++i;
  if(coldAt&&Due(now,coldAt)){coldAt=0;Fx(70123,2,boss,Position(boss),2000,25);for(auto p:Living(run))if(!Trapped(P::GetPlayerID(p))&&Distance(Position(p),Position(boss))<=25)Hit(p,Value(70123));}
  if(Due(now,nextPulse)){nextPulse=now+1000;for(auto p:Living(run)){auto pid=P::GetPlayerID(p);auto& d=debuffs[pid];if(d.buffetEnd&&Due(now,d.buffetEnd)){d.buffet=d.buffetEnd=0;Fx(70128,4,p,Position(p),0);}if(d.chillEnd&&Due(now,d.chillEnd))d.chill=d.chillEnd=0;if(d.chill&&Due(now,nextChill))Hit(p,Value(70106)*d.chill);if(d.backlash&&Due(now,d.backlash)){auto damage=Value(69766)*d.instability;Fx(69770,2,p,Position(p));for(auto other:Living(run))if(other==p||(ICC::Heroic(run.mode)&&Distance(Position(p),Position(other))<=10))Hit(other,damage);d.backlash=d.instability=0;}if(!Trapped(pid)&&Due(now,nextAura))Hit(p,Value(70084));}for(auto& t:tombs)if(t.asphyx&&Due(now,t.asphyx))Damage(Resolve(t.pid),Points::GetMaxHP(Resolve(t.pid))/10);if(Due(now,nextAura))nextAura=now+3000;if(Due(now,nextChill))nextChill=now+2000;}
  MeleeStrike();return true;
 }
};
