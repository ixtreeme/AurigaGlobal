#pragma once
// Instance-owned Auriga adaptation of the original Lady Deathwhisper encounter.
// All actors, damage, targeting and movement remain authoritative on the server.
class DeathwhisperRuntime final:public SourceRuntime,public ICC::DeathwhisperPort {
 Run& run;entt::entity boss;ICC::Deathwhisper controller;ICC::EventMap random;
 uint32_t now,last,nextMelee=0,nextMove=0,nextControl=0,pending=0,pendingAt=0,pendingPid=0;bool berserk=false,failed=false;
 uint64_t manaMax=0;std::map<uint32_t,uint64_t> threat;std::map<uint32_t,unsigned> touch;
 struct Debuff{uint32_t control=0,slow=0,torpor=0,absorbEnd=0,interruptAt=0;int64_t absorb=0;};std::map<uint32_t,Debuff> debuffs;
 struct Add{entt::entity entity=entt::null;int kind=0;uint32_t target=0,next=0,special=0,expires=0,reanimate=0,shieldEnd=0;bool empowered=false,reborn=false;uint64_t shield=0;};std::vector<Add> adds;
 struct Pool{Vec point;uint32_t expires=0,next=0;};std::vector<Pool> pools;
 struct Missile{uint32_t spell=0,pid=0,at=0;entt::entity source=entt::null;};std::vector<Missile> missiles;
 entt::entity Resolve(uint32_t pid)const{for(auto& m:run.members)if(m.pid==pid)return Player(m);return entt::null;}
 const ICC::SpellRecord* Spell(uint32_t id)const{return ICC::Spells().Resolve(id,run.mode);}
 int64_t Value(uint32_t id,size_t effect=0){auto s=Spell(id);if(!s||effect>=3){failed=true;return 0;}auto& e=s->effects[effect];return int64_t(e.basePoints)+(e.dieSides>0?random.Uniform(1,e.dieSides):e.dieSides);}
 uint32_t Tank()const{uint32_t result=0;double best=-1;for(auto& m:run.members){auto p=Player(m);auto d=debuffs.find(m.pid);if(!Alive(run,p)||Distance(Position(p),Arenas[1].center)>40||(d!=debuffs.end()&&!Due(now,d->second.control)))continue;auto t=threat.find(m.pid);auto s=touch.find(m.pid);double score=(t==threat.end()?0:double(t->second))/double(1+(s==touch.end()?0:s->second));score+=1.0/(1.0+Distance(Position(p),Position(boss)));if(score>best){best=score;result=m.pid;}}return result;}
 std::vector<entt::entity> Candidates(bool excludeTank=false){auto list=Living(run);auto tank=Tank();list.erase(std::remove_if(list.begin(),list.end(),[&](auto p){auto pid=P::GetPlayerID(p);return Distance(Position(p),Arenas[1].center)>40||(excludeTank&&pid==tank)||!Due(now,debuffs[pid].control);}),list.end());for(size_t i=list.size();i>1;--i)std::swap(list[i-1],list[random.Uniform(0,uint32_t(i-1))]);return list;}
 void Fx(uint32_t spell,int stage,entt::entity source,entt::entity target,Vec at,uint32_t life=1500,float radius=0){std::ostringstream line;line<<"er_dwfx "<<run.id<<' '<<run.nextHazard++<<' '<<spell<<' '<<stage<<' '<<(Valid(source)?P::GetPacketVID(source):0)<<' '<<(Valid(target)?P::GetPacketVID(target):0)<<' '<<BaseX+int(at.x*100)<<' '<<BaseY+int(at.y*100)<<' '<<life<<' '<<int(radius*100);Broadcast(run,line.str());}
 void Damage(entt::entity target,int64_t amount,entt::entity source=entt::null){if(Alive(run,target)&&amount>0)CombatSystem::Damage(target,Valid(source)?source:boss,amount*(berserk?6:1),DAMAGE_TYPE_SPECIAL);}
 void Slow(entt::entity p,uint32_t id,uint8_t point,int value,int seconds){AffectSystem::RemoveAffect(p,id);AffectSystem::AddAffect(p,id,point,value,0,seconds,0,true);}
 void Impact(uint32_t id,uint32_t pid,entt::entity source){auto p=Resolve(pid);if(!Alive(run,p))return;Fx(id,2,source,p,Position(p));Damage(p,Value(id),source);if(id==71420){debuffs[pid].slow=now+8000;Slow(p,62043,POINT_MOV_SPEED,-30,8);}}
 void Projectile(uint32_t id,entt::entity source,entt::entity p){if(!Alive(run,p)||!Valid(source)||missiles.size()>=96)return;auto s=Spell(id);if(!s){failed=true;return;}uint32_t travel=uint32_t(std::clamp(Distance(Position(source),Position(p))/std::max(1.f,s->missileSpeed)*1000.f,250.f,4000.f));Fx(id,1,source,p,Position(p),travel);missiles.push_back({id,P::GetPlayerID(p),now+travel,source});}
 void Finish(uint32_t id,uint32_t pid){auto p=Resolve(pid);
  if(id==71001){if(!Alive(run,p))return;auto s=Spell(id);float radius=s&&s->radius[0]>0?s->radius[0]:15;uint32_t life=s&&s->duration>0?uint32_t(s->duration):10000;if(pools.size()<12){pools.push_back({Position(p),now+life,now+1000});Fx(id,5,boss,p,Position(p),life,radius);}return;}
  if(id==72905){for(auto target:Candidates())Projectile(id,boss,target);return;}
  if(id==71254||id==71420){Projectile(id,boss,p);return;}
 }
 void Unlock(uint32_t pid){run.rooted.erase(pid);auto p=Resolve(pid);if(Present(run,p)){ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"er_root 0");Fx(71289,4,boss,p,Position(p));}if(auto m=Participant(run,pid)){m->aura=Mechanic::Strike;m->auraEnd=0;}}
 void ReleaseDebuffs(){for(auto& m:run.members){auto p=Player(m);if(!Valid(p))continue;AffectSystem::RemoveAffect(p,62043);AffectSystem::RemoveAffect(p,62044);if(run.rooted.count(m.pid))Unlock(m.pid);}}
 Add* Find(entt::entity e){for(auto& a:adds)if(a.entity==e)return &a;return nullptr;}
 void SpawnAdd(int kind,Vec point,bool reborn=false){if(adds.size()>=32)return;auto source=kind==0?37890u:kind==1?37949u:38222u;uint32_t race=kind==0?62020:kind==1?62021:62022;auto e=Spawn(run,race,point,1,0,source);if(!Valid(e)){failed=true;return;}if(kind==2){CombatSystem::SetInvincible(e,true);P::SetHP(e,Points::GetMaxHP(e));}Add a;a.entity=e;a.kind=kind;a.next=now+2000;a.special=now+10000;a.expires=now+(kind==2?35000:600000);a.reborn=reborn;a.target=Tank();adds.push_back(a);Fx(reborn?70900:52096,2,boss,e,point,2000);}
 void TickAdds(){
  for(size_t i=0;i<adds.size();){auto& a=adds[i];auto e=a.entity;
   if(a.reanimate&&Due(now,a.reanimate)){Vec at=Valid(e)?Position(e):Arenas[1].center;int kind=a.kind;Destroy(e);adds.erase(adds.begin()+i);SpawnAdd(kind,at,true);continue;}
   if(!Valid(e)||CombatSystem::IsDead(e)||Due(now,a.expires)){if(!a.reanimate){Destroy(e);adds.erase(adds.begin()+i);continue;}++i;continue;}
   auto p=Resolve(a.target);if(!Alive(run,p)||!Due(now,debuffs[a.target].control)){a.target=Tank();p=Resolve(a.target);}if(!Alive(run,p)){++i;continue;}
   if(a.shield&&Due(now,a.shieldEnd)){a.shield=0;Fx(70768,4,e,e,Position(e));}
   float distance=Distance(Position(e),Position(p));
   if(a.kind==2){if(distance<=3.5f){Fx(71544,2,e,p,Position(e),1800,8);for(auto other:Living(run))if(Distance(Position(other),Position(e))<=8)Damage(other,Value(71544),e);Destroy(e);adds.erase(adds.begin()+i);continue;}if(Due(now,a.next)){a.next=now+750;if(Walk(run,Position(p)))M::Goto(e,P::GetX(p),P::GetY(p));}++i;continue;}
   if(Due(now,a.next)){a.next=now+(a.kind==1?3500:2000);float range=a.kind==1?25:3;
    if(distance>range){if(Walk(run,Position(p)))M::Goto(e,P::GetX(p),P::GetY(p));}
    else{M::Stop(e);M::SetRotationToXY(e,P::GetX(p),P::GetY(p));M::SendMovePacket(e,a.kind==1?FUNC_MOB_SKILL:FUNC_ATTACK,0,P::GetX(e),P::GetY(e),0);if(a.kind==1)Projectile(a.empowered?70906:70594,e,p);else Damage(p,CalcMeleeDamage(e,p)*(a.empowered?2:1),e);}
   }
   if(Due(now,a.special)){a.special=now+15000;
    if(a.kind==0){Fx(70659,2,e,p,Position(p));Damage(p,CalcMeleeDamage(e,p)*Value(70659)/100,e);debuffs[a.target].absorb=std::max<int64_t>(0,Value(70659,1));debuffs[a.target].absorbEnd=now+20000;}
    else{Fx(71237,3,e,p,Position(p),10000);debuffs[a.target].torpor=now+10000;Slow(p,62044,POINT_CASTING_SPEED,-25,10);a.shield=100000;a.shieldEnd=now+12000;Fx(70768,3,e,e,Position(e),12000);}
   }++i;
  }
 }
public:
 explicit DeathwhisperRuntime(Run& r):run(r),boss(r.bosses.front()),controller(r.id^0xd34),random(r.id^0xa72),now(get_dword_time()),last(now){run.phase=1;manaMax=Points::GetMaxHP(boss);run.barrier=manaMax;M::Stop(boss);controller.Reset(*this);Fx(70842,3,boss,boss,Position(boss),600000);Notice(run,"ER_DW_PHASE_ONE");}
 ~DeathwhisperRuntime()override{ReleaseDebuffs();}
 ICC::Difficulty Mode()const override{return run.mode;}bool Casting()const override{return pending&&!Due(now,pendingAt);}
 void PhaseTwo()override{run.phase=2;threat.clear();pending=0;M::Stop(boss);Fx(70842,4,boss,boss,Position(boss));Notice(run,"ER_DW_PHASE_TWO");}
 void Ability(uint32_t id)override{
  if(id==26662){berserk=true;Notice(run,"ER_ENRAGE");Fx(id,3,boss,boss,Position(boss),600000);return;}
  if(id==71289){auto list=Candidates(true);auto count=std::min<size_t>(ICC::Deathwhisper::ControlTargets(run.mode),list.size());for(size_t i=0;i<count;i++){auto p=list[i];auto pid=P::GetPlayerID(p);debuffs[pid].control=now+12000;run.rooted.insert(pid);M::Stop(p);ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"er_root 1");Aura(run,p,Mechanic::Control,12000);Fx(id,3,boss,p,Position(p),12000);}if(count)Notice(run,"ER_DW_CONTROL");return;}
  if(id==71204){auto pid=Tank();if(pid){touch[pid]=std::min(5u,touch[pid]+1);Fx(id,2,boss,Resolve(pid),Position(Resolve(pid)));}return;}
  if(id==71363){auto list=Candidates(true);if(list.empty())list=Candidates();auto count=std::min<size_t>(ICC::Deathwhisper::ShadeCount(run.mode),list.size());for(size_t i=0;i<count;i++){auto at=Position(list[i]);at.y+=6;if(!Walk(run,at))at=Position(list[i]);SpawnAdd(2,at);if(!adds.empty())adds.back().target=P::GetPlayerID(list[i]);}Notice(run,"ER_DW_SHADE");return;}
  auto list=Candidates();uint32_t pid=id==71420?Tank():list.empty()?0:P::GetPlayerID(list.front());if(!pid)return;auto s=Spell(id);if(!s){failed=true;return;}uint32_t time=uint32_t(std::max(0,s->castTime));M::Stop(boss);auto p=Resolve(pid);M::SetRotationToXY(boss,P::GetX(p),P::GetY(p));M::SendMovePacket(boss,FUNC_MOB_SKILL,0,P::GetX(boss),P::GetY(boss),0);Fx(id,0,boss,p,Position(p),std::max(250u,time));if(time){pending=id;pendingPid=pid;pendingAt=now+time;}else Finish(id,pid);
 }
 void Wave(bool phaseTwo)override{auto c=Arenas[1].center;auto count=ICC::Deathwhisper::WaveSize(run.mode,phaseTwo);unsigned wave=unsigned(run.resource++);for(unsigned i=0;i<count;i++){float side=((i/3+wave)%2)?-1.f:1.f;Vec at{c.x+side*24,c.y-14+float(i%3)*12};SpawnAdd((i+wave)%2,at);}Notice(run,"ER_DW_WAVE");}
 void Empower()override{std::vector<size_t> options;for(size_t i=0;i<adds.size();i++)if(adds[i].kind<2&&!adds[i].empowered&&!adds[i].reborn&&!adds[i].reanimate&&Valid(adds[i].entity)&&!CombatSystem::IsDead(adds[i].entity))options.push_back(i);if(options.empty())return;auto& a=adds[options[random.Uniform(0,uint32_t(options.size()-1))]];a.empowered=true;
  if(random.Uniform(0,1)){Fx(70903,2,boss,a.entity,Position(a.entity),3000,8);for(auto p:Living(run))if(Distance(Position(p),Position(a.entity))<=8)Damage(p,Value(70903),a.entity);a.reanimate=now+3000;CombatSystem::Dead(a.entity,boss);}
  else{Fx(a.kind==0?70900:70901,3,boss,a.entity,Position(a.entity),600000);Broadcast(run,"er_actor_scale "+std::to_string(P::GetPacketVID(a.entity))+" 150");}Notice(run,"ER_DW_EMPOWER");
 }
 void Attacked(entt::entity attacker,int64_t amount)override{if(amount>0&&Participant(run,P::GetPlayerID(attacker)))threat[P::GetPlayerID(attacker)]+=uint64_t(amount);}
 int64_t TargetDamage(entt::entity attacker,entt::entity victim,int64_t amount)override{
  if(victim==boss){Attacked(attacker,amount);return controller.Absorb(*this,run.barrier,amount);}
  if(auto a=Find(victim)){if(a->kind==2||a->reanimate)return 0;if(a->reborn)amount=std::max<int64_t>(1,amount/2);if(a->shield){auto blocked=std::min<uint64_t>(a->shield,uint64_t(amount));a->shield-=blocked;amount-=blocked;if(!a->shield)Fx(70768,4,victim,victim,Position(victim));}}return amount;
 }
 int64_t Healing(entt::entity p,int64_t amount)override{auto& absorb=debuffs[P::GetPlayerID(p)].absorb;auto blocked=std::min(absorb,std::max<int64_t>(0,amount));absorb-=blocked;return amount-blocked;}
 bool Dispel(entt::entity p)override{auto pid=P::GetPlayerID(p);auto& d=debuffs[pid];d.slow=d.torpor=0;AffectSystem::RemoveAffect(p,62043);AffectSystem::RemoveAffect(p,62044);return true;}
 void Died(entt::entity actor)override{if(actor==boss){Notice(run,"ER_DW_DEFEATED");ReleaseDebuffs();}else if(P::IsPC(actor)){auto pid=P::GetPlayerID(actor);debuffs[pid]={};Unlock(pid);}}
 bool Interrupt(entt::entity p){auto& d=debuffs[P::GetPlayerID(p)];if(!Alive(run,p)||!controller.SecondPhase()||pending!=71420||Due(now,pendingAt)||!Due(now,d.interruptAt)||Distance(Position(p),Position(boss))>20||P::GetSP(p)<50)return false;Points::Change(p,POINT_SP,-50);d.interruptAt=now+12000;pending=0;M::Stop(boss);Fx(71420,4,boss,p,Position(boss));Notice(run,"ER_DW_INTERRUPTED");return true;}
 void Status(entt::entity p)override{ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"er_dwcast %u %u %u",run.id,pending,pending&&!Due(now,pendingAt)?pendingAt-now:0);ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"er_dwstate %u %d %llu %llu %u",run.id,run.phase,(unsigned long long)run.barrier,(unsigned long long)manaMax,P::GetPacketVID(boss));ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"er_effects_clear 0");auto pid=P::GetPlayerID(p);if(touch[pid])ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"er_effect 71204 %u 0",touch[pid]);auto& d=debuffs[pid];if(!Due(now,d.control))ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"er_effect 71289 1 %u",(d.control-now+999)/1000);if(d.absorb)ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"er_effect 70659 1 0");ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"er_effects_end 0");}
 bool Update(uint32_t time)override{now=time;uint32_t elapsed=now-last;last=now;if(!Valid(boss)||CombatSystem::IsDead(boss))return true;
  for(auto& m:run.members){auto& d=debuffs[m.pid];if(d.absorbEnd&&Due(now,d.absorbEnd)){d.absorb=0;d.absorbEnd=0;}if(d.control&&(Due(now,d.control)||!Alive(run,Player(m)))){d.control=0;Unlock(m.pid);}}
  if(Due(now,nextControl)){nextControl=now+2000;for(auto& [pid,d]:debuffs)if(d.control&&!Due(now,d.control)){auto list=Candidates();list.erase(std::remove_if(list.begin(),list.end(),[&](auto p){return P::GetPlayerID(p)==pid||Distance(Position(p),Position(Resolve(pid)))>25;}),list.end());if(!list.empty()){auto source=Resolve(pid);Fx(71254,1,source,list.front(),Position(list.front()),750);Damage(list.front(),Value(71254)/2);}}}
  if(pending&&Due(now,pendingAt)){auto id=pending,pid=pendingPid;pending=0;Finish(id,pid);}
  for(size_t i=0;i<missiles.size();){if(!Due(now,missiles[i].at)){++i;continue;}auto m=missiles[i];missiles.erase(missiles.begin()+i);if(Valid(m.source))Impact(m.spell,m.pid,m.source);}
  auto decay=Spell(71001);float radius=decay&&decay->radius[0]>0?decay->radius[0]:15;
  for(auto& pool:pools)if(!Due(now,pool.expires)&&Due(now,pool.next)){pool.next=now+1000;for(auto p:Living(run))if(Distance(Position(p),pool.point)<=radius)Damage(p,Value(71001));}pools.erase(std::remove_if(pools.begin(),pools.end(),[&](auto& p){return Due(now,p.expires);}),pools.end());
  TickAdds();controller.Tick(*this,elapsed);
  if(controller.SecondPhase()&&!Casting()){auto p=Resolve(Tank());if(Alive(run,p)){float d=Distance(Position(p),Position(boss));if(d>5&&Due(now,nextMove)){nextMove=now+750;if(Walk(run,Position(p)))M::Goto(boss,P::GetX(p),P::GetY(p));}else if(d<=5&&Due(now,nextMelee)){nextMelee=now+2000;M::Stop(boss);M::SetRotationToXY(boss,P::GetX(p),P::GetY(p));M::SendMovePacket(boss,FUNC_ATTACK,0,P::GetX(boss),P::GetY(boss),0);Damage(p,CalcMeleeDamage(boss,p));}}}
  return !failed;
 }
};
