#pragma once
class RotfaceRuntime final:public FestergutRuntime,public ICC::RotfacePort {
 ICC::Rotface rotface;
 struct Ooze {entt::entity entity=entt::null;bool big=false;uint32_t stacks=1,sticky=0,pulse=0,melee=0,explosion=0,despawn=0;std::map<uint32_t,uint64_t> hate;};
 struct Area {Vec point;float radius=0;uint32_t spell=0,at=0,end=0,next=0;};
 struct Bomb {Vec point;uint32_t at=0;};
 std::vector<Ooze> oozes;std::vector<Area> areas;std::vector<Bomb> bombs;
 Vec sprayTarget;uint32_t sprayEnd=0,sprayNext=0,mergeNext=0,slowNext=0;bool dance=true;
 std::map<uint32_t,uint32_t> slowExpiry;
 void Zone(Vec point,uint32_t spell,float radius,uint32_t delay,uint32_t duration){if(areas.size()>=128)return;auto at=now+delay;areas.push_back({point,radius,spell,at,at+duration,at+1000});Emit(point,delay,duration,int(Mechanic::Slime),radius);}
 uint32_t Nearest(Vec point)const{uint32_t pid=0;float minimum=1e6f;for(auto& member:run.members){auto p=Player(member);if(!Alive(run,p))continue;float d=Distance(point,Position(p));if(d<minimum){minimum=d;pid=member.pid;}}return pid;}
 uint32_t OozeVictim(const Ooze& ooze)const{uint32_t target=0;uint64_t maximum=0;for(auto [pid,value]:ooze.hate){if(Alive(run,Resolve(pid))&&(!target||value>maximum)){target=pid;maximum=value;}}return target?target:Nearest(Position(ooze.entity));}
 void AddOoze(Vec point,uint32_t owner,bool big=false,uint32_t stacks=1){if(oozes.size()>=96)return;auto entity=Spawn(run,62018,point,big?6:5,owner,big?36899:36897);if(!Valid(entity))return;Ooze ooze;ooze.entity=entity;ooze.big=big;ooze.stacks=stacks;ooze.sticky=now+5000;ooze.pulse=now+2000;ooze.melee=now+2000;if(owner)ooze.hate[owner]=big?1:500000;oozes.push_back(std::move(ooze));
  std::ostringstream scale;scale<<"er_actor_scale "<<P::GetPacketVID(entity)<<' '<<(big?200:80);Broadcast(run,scale.str());
 }
 void Explosion(Ooze& ooze){if(ooze.explosion||ooze.despawn)return;dance=false;ooze.explosion=now+uint32_t(Spell(69839)->castTime);ooze.sticky=0;M::Stop(ooze.entity);Talk(4);Talk(5);M::SendMovePacket(ooze.entity,FUNC_MOB_SKILL,0,P::GetX(ooze.entity),P::GetY(ooze.entity),0);}
 bool MergeOne(){for(size_t i=0;i<oozes.size();i++)for(size_t j=i+1;j<oozes.size();j++){
  auto& a=oozes[i];auto& b=oozes[j];if(a.explosion||a.despawn||b.explosion||b.despawn||!Valid(a.entity)||!Valid(b.entity)||CombatSystem::IsDead(a.entity)||CombatSystem::IsDead(b.entity)||Distance(Position(a.entity),Position(b.entity))>8)continue;
  auto stacks=ICC::MergeOozeStacks(a.big,a.stacks,b.big,b.stacks);size_t keep=a.big?i:b.big?j:i,drop=keep==i?j:i;
  if(!a.big&&!b.big){auto point=Position(a.entity);uint32_t target=Nearest(point);Destroy(a.entity);Destroy(b.entity);oozes.erase(oozes.begin()+j);oozes.erase(oozes.begin()+i);AddOoze(point,target,true,stacks);}
  else {oozes[keep].stacks=stacks;Destroy(oozes[drop].entity);oozes.erase(oozes.begin()+drop);auto index=keep>drop?keep-1:keep;if(stacks>=4){std::ostringstream tint;tint<<"er_actor_tint "<<P::GetPacketVID(oozes[index].entity)<<" 255 70 35";Broadcast(run,tint.str());}if(stacks>=5)Explosion(oozes[index]);}
  return true;
 }return false;}
 void InfectionTick(const ICC::AuraEvent& e){if(e.aura.spec.key.spell!=ICC::RotfaceSpell::Infection){Periodic(e);return;}auto p=Resolve(e.aura.spec.key.target);
  if(e.removed){if(e.reason!=ICC::AuraRemoval::EncounterReset&&Present(run,p))AddOoze(Position(p),e.aura.spec.key.target);if(auto member=Participant(run,e.aura.spec.key.target)){member->aura=Mechanic::Strike;member->auraEnd=0;}}
  else if(Alive(run,p))Damage(p,Value(ICC::RotfaceSpell::Infection));
 }
 void FinishRotface(uint32_t id,uint32_t target){if(id==ICC::RotfaceSpell::Spray){sprayEnd=now+uint32_t(Spell(id)->duration);sprayNext=now+1000;Talk(3);return;}
  if(id==ICC::RotfaceSpell::Infection){auto candidates=Living(run);candidates.erase(std::remove_if(candidates.begin(),candidates.end(),[&](auto p){auto pid=P::GetPlayerID(p);return pid==Tank()||Effect(id,pid);}),candidates.end());if(candidates.empty())return;auto p=candidates[random.Uniform(0,uint32_t(candidates.size()-1))];auto pid=P::GetPlayerID(p);Apply(id,pid,1);if(auto member=Participant(run,pid)){member->aura=Mechanic::Infection;member->auraEnd=now+uint32_t(Spell(id)->duration);}Talk(9);return;}
  if(id==ICC::RotfaceSpell::VileGas)Finish(id,target,0);
 }
public:
 explicit RotfaceRuntime(Run& r):FestergutRuntime(r,false),rotface(r.id){rotface.Reset();rotface.Engaged(*this);}
 ~RotfaceRuntime()override{for(auto& m:run.members){auto p=Player(m);if(Valid(p))AffectSystem::RemoveAffect(p,OozeMoveSlow);}}
 ICC::Difficulty Mode()const override{return FestergutRuntime::Mode();}bool HasVictim()const override{return FestergutRuntime::HasVictim();}bool Casting()const override{return FestergutRuntime::Casting();}uint32_t Victim()const override{return Tank();}std::vector<ICC::Candidate> Targets()const override{return FestergutRuntime::Targets();}
 void Cast(uint32_t id,uint32_t target=0,bool triggered=false,int maximum=0)override{auto s=Spell(id);if(!s){failed=true;return;}if(!triggered&&Casting())return;if(!triggered&&s->castTime>0){pending=id;pendingTarget=target;pendingEnd=now+uint32_t(s->castTime);if(id==ICC::RotfaceSpell::Spray){auto from=Position(boss);auto dx=sprayTarget.x-from.x,dy=sprayTarget.y-from.y;auto length=std::hypot(dx,dy);if(length<.01f){dx=1;dy=0;length=1;}std::ostringstream cone;cone<<"er_hazard "<<run.id<<' '<<run.nextHazard++<<' '<<int(Mechanic::Slime)<<" 1 "<<P::GetX(boss)<<' '<<P::GetY(boss)<<" 6600 "<<int(dx/length*100)<<' '<<int(dy/length*100)<<" 0 "<<s->castTime<<' '<<s->duration;Broadcast(run,cone.str());}M::Stop(boss);M::SendMovePacket(boss,FUNC_MOB_SKILL,0,P::GetX(boss),P::GetY(boss),0);}else FinishRotface(id,target);}
 bool SprayAt(uint32_t target)override{auto p=Resolve(target);if(!Alive(run,p))return false;sprayTarget=Position(p);M::SetRotationToXY(boss,P::GetX(p),P::GetY(p));return true;}
 void Unroot()override{}
 bool ProfessorAvailable()const override{return true;}
 void Flood(uint32_t sector,uint32_t)override{constexpr float pi=3.14159265358979323846f;auto center=Arenas[5].center;for(int n=0;n<2;n++){float angle=sector*pi/2+(n?.375f:.125f)*pi;Vec at{center.x+28*std::cos(angle),center.y+28*std::sin(angle)};Zone(at,69789,Spell(69789)->radius[0],5000,uint32_t(Spell(69788)->duration));}}
 void ProfessorCast(uint32_t spell,uint32_t target)override{FinishRotface(spell,target);}
 void ProfessorAction(int)override{}
 void Talk(int group)override{auto key="ER_BOSS_6_LINE_"+std::to_string(group);Notice(run,key.c_str());}
 void Melee()override{FestergutRuntime::Melee();}
 int64_t Healing(entt::entity player,int64_t amount)override{return Effect(ICC::RotfaceSpell::Infection,P::GetPlayerID(player))?amount*std::max<int64_t>(0,100+Value(ICC::RotfaceSpell::Infection,1))/100:amount;}
 bool Dispel(entt::entity target)override{return auras.Remove(Key(ICC::RotfaceSpell::Infection,P::GetPlayerID(target)),ICC::AuraRemoval::Dispel);}
 int64_t TargetDamage(entt::entity attacker,entt::entity target,int64_t amount)override{auto pid=P::GetPlayerID(attacker);if(Participant(run,pid))for(auto& ooze:oozes)if(ooze.entity==target&&amount>0)ooze.hate[pid]+=uint64_t(amount);return amount;}
 void Died(entt::entity actor)override{if(actor==boss){rotface.Died(*this);if(dance)Notice(run,"ER_OOZE_DANCE_SUCCESS");}else if(P::IsPC(actor))rotface.KilledPlayer(*this);}
 void Status(entt::entity player)override{FestergutRuntime::Status(player);for(auto& ooze:oozes)if(Valid(ooze.entity)){ecs::ChatSystem::Send(player,CHAT_TYPE_COMMAND,"er_actor_scale %u %d",P::GetPacketVID(ooze.entity),ooze.big?200:80);if(ooze.stacks>=4)ecs::ChatSystem::Send(player,CHAT_TYPE_COMMAND,"er_actor_tint %u 255 70 35",P::GetPacketVID(ooze.entity));}}
 bool Update(uint32_t time)override{now=time;uint32_t elapsed=now-last;last=now;auras.Advance(elapsed,[](auto&){},0);if(!Valid(boss)||CombatSystem::IsDead(boss))return true;
  for(auto& member:run.members)if(!Alive(run,Player(member)))auras.RemoveTarget(member.pid,ICC::AuraRemoval::Death);
  auras.Advance(0,[&](const auto& e){InfectionTick(e);});
  for(auto& member:run.members){auto p=Player(member);bool lock=Effect(ICC::RotfaceSpell::VileGas,member.pid)!=nullptr;if(lock){run.rooted.insert(member.pid);if(Alive(run,p))M::Stop(p);}else run.rooted.erase(member.pid);}
  if(pending&&Due(now,pendingEnd)){auto id=pending,target=pendingTarget;pending=0;FinishRotface(id,target);}
  if(sprayEnd&&Due(sprayEnd,sprayNext)&&Due(now,sprayNext)){sprayNext=now+1000;auto from=Position(boss);Hazard cone;cone.shape=Shape::Cone;cone.center=from;cone.direction={sprayTarget.x-from.x,sprayTarget.y-from.y};if(std::hypot(cone.direction.x,cone.direction.y)<.01f)cone.direction={1,0};cone.radius=200;for(auto p:Living(run))if(Contains(cone,Position(p)))Damage(p,Value(69507),true);}
  std::map<uint32_t,int> slows;
  for(size_t i=0;i<areas.size();){auto& area=areas[i];if(Due(now,area.end)){areas.erase(areas.begin()+i);continue;}if(Due(now,area.at))for(auto p:Living(run))if(Distance(Position(p),area.point)<=area.radius){auto pid=P::GetPlayerID(p);slows[pid]=std::max(slows[pid],int(-Value(area.spell,1)));if(Due(now,area.next))Damage(p,Value(area.spell),true);}if(Due(now,area.next))area.next=now+1000;++i;}
  if(Due(now,slowNext)){slowNext=now+1000;for(auto [pid,percent]:slows){auto p=Resolve(pid);AffectSystem::RemoveAffect(p,OozeMoveSlow);auto speed=Points::Get(p,POINT_MOV_SPEED);AffectSystem::AddAffect(p,OozeMoveSlow,POINT_MOV_SPEED,-int32_t(speed*std::min(100,percent)/100),0,2,0,true);slowExpiry[pid]=now+1250;}}
  for(auto it=slowExpiry.begin();it!=slowExpiry.end();){if(Due(now,it->second)){auto p=Resolve(it->first);if(Valid(p))AffectSystem::RemoveAffect(p,OozeMoveSlow);it=slowExpiry.erase(it);}else ++it;}
  for(size_t i=0;i<oozes.size();){auto& ooze=oozes[i];if(!Valid(ooze.entity)||CombatSystem::IsDead(ooze.entity)||(ooze.despawn&&Due(now,ooze.despawn))){Destroy(ooze.entity);oozes.erase(oozes.begin()+i);continue;}
   if(ooze.explosion){if(Due(now,ooze.explosion)){auto from=Position(ooze.entity);auto speed=Spell(69832)->missileSpeed;for(auto p:Living(run)){auto point=Position(p);auto travel=uint32_t(Distance(from,point)/speed*1000);if(bombs.size()<256){bombs.push_back({point,now+travel});Emit(point,travel,500,int(Mechanic::Slime),Spell(69833)->radius[0]);}}ooze.explosion=0;ooze.despawn=now+2000;}++i;continue;}
   if(ooze.despawn){++i;continue;}
   auto target=Resolve(OozeVictim(ooze));if(Alive(run,target)){auto at=Position(ooze.entity);if(Distance(at,Position(target))>2.5f)M::Goto(ooze.entity,P::GetX(target),P::GetY(target));else if(Due(now,ooze.melee)){ooze.melee=now+2000;M::Stop(ooze.entity);M::SendMovePacket(ooze.entity,FUNC_ATTACK,0,P::GetX(ooze.entity),P::GetY(ooze.entity),0);Damage(target,int64_t(CalcMeleeDamage(ooze.entity,target))*(100+(ooze.big?20*ooze.stacks:0))/100);}
    if(ooze.sticky&&Due(now,ooze.sticky)){ooze.sticky=now+15000;Zone(Position(target),69778,Spell(69778)->radius[0],1000,30000);}
   }
   if(Due(now,ooze.pulse)){ooze.pulse=now+2000;auto id=ooze.big?69761u:69751u;for(auto p:Living(run))if(Distance(Position(p),Position(ooze.entity))<=Spell(id)->radius[0])Damage(p,Value(id)*(100+(ooze.big?20*ooze.stacks:0))/100,true);}
   ++i;
  }
  if(Due(now,mergeNext)){mergeNext=now+500;for(int budget=0;budget<32&&MergeOne();budget++){};}
  for(size_t i=0;i<bombs.size();){if(!Due(now,bombs[i].at)){++i;continue;}auto point=bombs[i].point;bombs.erase(bombs.begin()+i);for(auto p:Living(run))if(Distance(Position(p),point)<=Spell(69833)->radius[0])Damage(p,Value(69833),true);}
  rotface.Tick(*this,elapsed);return !failed;
 }
};
