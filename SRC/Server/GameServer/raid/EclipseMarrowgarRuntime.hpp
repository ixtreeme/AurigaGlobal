#pragma once
// Included inside EclipseRaidSystem.cpp's private namespace: world access stays
// local to the instance owner. The controller/spell rules live in icc/ and have
// independent source-parity tests. No ordinary mob receives a new timer.
class MarrowgarRuntime final:public SourceRuntime,public ICC::MarrowgarPort,public ICC::MarrowgarFlamePort {
 Run& run;entt::entity boss;ICC::Marrowgar controller;ICC::EventMap random;ICC::AuraLedger auras;
 struct Trail {ICC::ColdflameTrail controller;Vec point;float heading=0;uint32_t id=0;};
 struct Ground {Vec point;uint32_t source=0,expires=0;};
 struct Pin {entt::entity entity=entt::null;uint32_t pid=0;ICC::BoneSpike controller;};
 std::vector<Trail> trails;std::vector<Ground> ground;std::vector<Pin> pins;
 std::map<uint32_t,uint64_t> threat;std::set<ICC::AuraKey> flameAuras;
 uint32_t stormMarker=0,now=0,last=0,castEnd=0,pendingSpell=0,pendingTarget=0,stormEnd=0,stormNext=0,nextMelee=0;
 bool passive=false,sliceQueued=false,evade=false,boned=true,enraged=false,charging=false;
 Vec chargeGoal;float chargeSpeed=25;
 entt::entity Resolve(uint32_t pid)const{for(auto& member:run.members)if(member.pid==pid)return Player(member);return entt::null;}
 uint32_t ChooseVictim()const{
  uint32_t pid=0;uint64_t value=0;float nearest=1e6f;
  for(auto& member:run.members){auto p=Player(member);if(!Alive(run,p)||run.rooted.count(member.pid))continue;
   auto row=threat.find(member.pid);auto amount=row==threat.end()?0:row->second;float d=Distance(Position(p),Position(boss));
   if(!pid||amount>value||(amount==value&&d<nearest)){pid=member.pid;value=amount;nearest=d;}
  }return pid;
 }
 void Damage(entt::entity player,int64_t amount){if(Alive(run,player)&&amount>0)CombatSystem::Damage(player,boss,amount,DAMAGE_TYPE_SPECIAL);}
 const ICC::SpellRecord* Spell(uint32_t id)const{return ICC::Spells().Resolve(id,run.mode);}
 int64_t Value(uint32_t id,size_t effect=0){auto spell=Spell(id);if(!spell||effect>=3)return 0;auto& e=spell->effects[effect];return int64_t(e.basePoints)+(e.dieSides>0?random.Uniform(1,uint32_t(e.dieSides)):e.dieSides);}
 void Root(uint32_t pid,bool locked){auto p=Resolve(pid);if(locked){run.rooted.insert(pid);if(Alive(run,p))M::Stop(p);}else run.rooted.erase(pid);
  if(Present(run,p))ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"er_root %d",locked?1:0);
  if(auto member=Participant(run,pid)){if(locked){member->aura=Mechanic::Spike;member->auraEnd=now+2000;}else if(member->aura==Mechanic::Spike){member->aura=Mechanic::Strike;member->auraEnd=member->object=0;}}
 }
 void Release(uint32_t pid){auto p=Resolve(pid);if(Present(run,p))Broadcast(run,"er_unpin "+std::to_string(P::GetPacketVID(p)));Root(pid,false);auras.Remove({ICC::MarrowgarSpell::Impaled,P::GetPacketVID(boss),pid},ICC::AuraRemoval::Script);}
 void PinPlayer(uint32_t pid){auto p=Resolve(pid);if(!Alive(run,p))return;
  auto entity=Spawn(run,62019,Position(p),3,pid,36619);if(!Valid(entity))return;
  pins.push_back({entity,pid,{}});Root(pid,true);Participant(run,pid)->object=P::GetPacketVID(entity);
  Broadcast(run,"er_pin "+std::to_string(P::GetPacketVID(p))+" "+std::to_string(P::GetPacketVID(entity)));
  const auto spell=Spell(ICC::MarrowgarSpell::Impaled);if(spell)auras.Apply({{ICC::MarrowgarSpell::Impaled,P::GetPacketVID(boss),pid},-1,1,{{spell->effects[0].period,spell->effects[1].period,spell->effects[2].period}}});
 }
 void ResolveCast(uint32_t id,uint32_t target){
  using namespace ICC::MarrowgarSpell;
  if(id==BoneStorm){auto spell=Spell(id);stormEnd=now+uint32_t(spell?spell->duration:60000);stormNext=now+2000;return;}
  if(id==Berserk){enraged=true;return;}
  if(id==SpikeGraveyard){auto chosen=controller.SelectSpikes(*this);for(auto pid:chosen)PinPlayer(pid);if(!chosen.empty())Talk(3);return;}
  if(id==Coldflame){std::vector<ICC::FlameTarget> targets;for(auto& member:run.members){auto p=Player(member);if(!Alive(run,p))continue;auto at=Position(p);targets.push_back({member.pid,{at.x,at.y},.5f,true,run.rooted.count(member.pid)!=0});}
   auto at=Position(boss);auto chosen=ICC::SelectColdflameTarget(random,{at.x,at.y},3.5f,Victim(),targets);
   if(chosen)ICC::CastColdflame(*this,{at.x,at.y},3.5f,chosen->position,uint32_t(Value(Coldflame)));return;
  }
  if(id==StormColdflame){auto at=Position(boss);ICC::CastStormColdflame(*this,{at.x,at.y},uint32_t(Value(id)));return;}
  if(id==BoneSlice){sliceQueued=true;return;}
 }
 void PublishGround(const Ground& patch,uint32_t life){
  std::ostringstream message;message<<"er_hazard "<<run.id<<' '<<run.nextHazard++<<' '<<int(Mechanic::Flame)<<" 0 "<<BaseX+int(patch.point.x*100)<<' '<<BaseY+int(patch.point.y*100)<<" 500 100 0 0 0 "<<life;Broadcast(run,message.str());
 }
 void Segment(Trail& trail){if(ground.size()>=256)return;auto spell=Spell(69146);if(!spell)return;Ground patch{trail.point,trail.id,now+uint32_t(spell->duration)};ground.push_back(patch);PublishGround(patch,uint32_t(spell->duration));}
 bool Ray(Vec from,Vec to)const{float distance=Distance(from,to);int steps=std::max(1,int(std::ceil(distance/.5f)));for(int i=1;i<=steps;i++){float t=float(i)/steps;if(!Walk(run,{from.x+(to.x-from.x)*t,from.y+(to.y-from.y)*t}))return false;}return true;}
 struct TrailWorld:ICC::ColdflamePort {
  MarrowgarRuntime& runtime;Trail& trail;
  TrailWorld(MarrowgarRuntime& r,Trail& t):runtime(r),trail(t){}
  void CastSegment()override{runtime.Segment(trail);}
  bool StepFiveMetres()override{Vec next{trail.point.x+5*std::cos(trail.heading),trail.point.y+5*std::sin(trail.heading)};if(!runtime.Ray(trail.point,next))return false;trail.point=next;return true;}
 };
 struct SpikeWorld:ICC::SpikePort {
  MarrowgarRuntime& runtime;Pin& pin;bool remove=false;
  SpikeWorld(MarrowgarRuntime& r,Pin& p):runtime(r),pin(p){}
  bool HasLivingPassenger()const override{return Valid(pin.entity)&&!CombatSystem::IsDead(pin.entity)&&Alive(runtime.run,runtime.Resolve(pin.pid));}
  bool EncounterActive()const override{return runtime.run.state==State::Combat;}
  void ReleasePassenger()override{runtime.Release(pin.pid);}
  void Despawn()override{remove=true;}
  void BonedFailed()override{runtime.boned=false;}
 };
 void Periodic(const ICC::AuraEvent& event){if(event.removed)return;auto p=Resolve(event.aura.spec.key.target);if(!Alive(run,p))return;
  if(event.aura.spec.key.spell==ICC::MarrowgarSpell::Impaled){auto spell=Spell(ICC::MarrowgarSpell::Impaled);if(spell&&spell->effects[event.effect].aura==89)Damage(p,int64_t(Points::GetMaxHP(p))*Value(ICC::MarrowgarSpell::Impaled,event.effect)/100);}
  else if(event.aura.spec.key.spell==69146)Damage(p,Value(69146));
 }
public:
 bool TestSpike(uint32_t pid){if(run.rooted.count(pid)||!Alive(run,Resolve(pid)))return false;PinPlayer(pid);return run.rooted.count(pid)!=0;}
 explicit MarrowgarRuntime(Run& owner):run(owner),boss(owner.bosses.front()),controller(owner.id),random(owner.id^0x1cc),now(get_dword_time()),last(now),nextMelee(now+ICC::Creatures().Resolve(36612,owner.mode)->attackPeriod){controller.Reset(*this);controller.Engaged(*this);}
 ICC::Difficulty Mode()const override{return run.mode;}
 bool HasVictim()const override{return ChooseVictim()!=0;}
 bool Casting()const override{return pendingSpell!=0&&!Due(now,castEnd);}
 bool HasStorm()const override{return stormEnd&&!Due(now,stormEnd);}
 bool Charging()const override{return charging;}
 bool MeleeReady()const override{return Due(now,nextMelee);}
 bool MeleeSpellQueued()const override{return sliceQueued;}
 uint32_t Victim()const override{return ChooseVictim();}
 bool VictimInsideArena()const override{auto p=Resolve(Victim());return Alive(run,p)&&Distance(Position(p),Arenas[0].center)<=Arenas[0].radius+8;}
 std::vector<ICC::Candidate> Targets()const override{std::vector<ICC::Candidate> targets;for(auto& member:run.members){auto p=Player(member);if(!Present(run,p))continue;targets.push_back({member.pid,Distance(Position(p),Position(boss)),Alive(run,p),P::IsGM(p),run.rooted.count(member.pid)!=0,Distance(Position(p),Arenas[0].center)<=Arenas[0].radius+8});}return targets;}
 void Cast(uint32_t spell,uint32_t target=0,bool triggered=false)override{if(!triggered&&Casting())return;auto record=Spell(spell);if(!record){evade=true;LOG_ERROR("ECLIPSE_SOURCE_SPELL_MISSING spell={}",spell);return;}
  if(!triggered&&record->castTime>0){pendingSpell=spell;pendingTarget=target;castEnd=now+uint32_t(record->castTime);M::SendMovePacket(boss,FUNC_MOB_SKILL,0,P::GetX(boss),P::GetY(boss),0);}else ResolveCast(spell,target);
 }
 void Talk(uint32_t group)override{auto key="ER_BOSS_1_LINE_"+std::to_string(group);Notice(run,key.c_str());}
 void SetPassive(bool value)override{passive=value;}
 void StopMovement()override{charging=false;M::Stop(boss);}
 void InterruptMelee()override{sliceQueued=false;}
 void ChaseVictim()override{auto p=Resolve(Victim());if(Alive(run,p))M::Goto(boss,P::GetX(p),P::GetY(p));}
 void Charge(uint32_t target,float speed)override{auto p=Resolve(target);if(!Alive(run,p))return;chargeGoal=Position(p);chargeSpeed=speed;charging=true;M::Stop(boss);M::SetRotationToXY(boss,P::GetX(p),P::GetY(p));M::SendMovePacket(boss,FUNC_MOVE,0,P::GetX(p),P::GetY(p),uint32_t(Distance(Position(boss),chargeGoal)/speed*1000));}
 void StormDuration(uint32_t duration)override{stormEnd=now+duration;stormMarker=run.nextHazard++;std::ostringstream line;line<<"er_marker "<<run.id<<' '<<stormMarker<<' '<<P::GetPacketVID(boss)<<' '<<int(Mechanic::Storm)<<" 900 "<<duration;Broadcast(run,line.str());}
 void Melee()override{
  if(passive||Casting()||!MeleeReady())return;auto p=Resolve(Victim());if(!Alive(run,p))return;float distance=Distance(Position(p),Position(boss));if(distance>5.5f){M::Goto(boss,P::GetX(p),P::GetY(p));return;}
  M::Stop(boss);M::SetRotationToXY(boss,P::GetX(p),P::GetY(p));M::SendMovePacket(boss,FUNC_ATTACK,0,P::GetX(boss),P::GetY(boss),0);nextMelee=now+uint32_t(ICC::Creatures().Resolve(36612,run.mode)->attackPeriod/(enraged?2.5f:1.f));
  if(sliceQueued){std::vector<entt::entity> targets;for(auto player:Living(run))if(Distance(Position(player),Position(p))<5&&Distance(Position(player),Position(boss))<8)targets.push_back(player);
   std::sort(targets.begin(),targets.end(),[&](auto a,auto b){return Distance(Position(a),Position(p))<Distance(Position(b),Position(p));});auto spell=Spell(ICC::MarrowgarSpell::BoneSlice);if(spell&&targets.size()>spell->maximumTargets)targets.resize(spell->maximumTargets);
   for(auto player:targets){auto damage=int64_t(CalcMeleeDamage(boss,player))*Value(ICC::MarrowgarSpell::BoneSlice)/100;controller.SliceHit(P::GetPlayerID(player));Damage(player,ICC::Marrowgar::SliceDamage(damage*(enraged?6:1),uint32_t(targets.size()),spell?spell->maximumTargets:0));}sliceQueued=false;
  }else Damage(p,int64_t(CalcMeleeDamage(boss,p))*(enraged?6:1));
 }
 void Evade()override{evade=true;}
 void BonedEligible(bool value)override{boned=value;}
 bool LineOfSight(ICC::Point destination)const override{return Ray(Position(boss),{destination.x,destination.y});}
 void SpawnFlame(const ICC::FlameSpawn& flame)override{if(trails.size()<32)trails.push_back({{},{flame.position.x,flame.position.y},flame.heading,run.nextHazard++});}
 void Attacked(entt::entity attacker,int64_t damage)override{auto pid=P::GetPlayerID(attacker);if(Participant(run,pid)&&damage>0)threat[pid]+=uint64_t(damage);}
 void Died(entt::entity actor)override{if(actor==boss){controller.Died(*this);if(boned)Notice(run,"ER_BONED_SUCCESS");}else if(P::IsPC(actor))controller.KilledPlayer(*this);}
 void Status(entt::entity player)override{for(auto& pin:pins){auto p=Resolve(pin.pid);if(Alive(run,p)&&Valid(pin.entity))ecs::ChatSystem::Send(player,CHAT_TYPE_COMMAND,"er_pin %u %u",P::GetPacketVID(p),P::GetPacketVID(pin.entity));}ecs::ChatSystem::Send(player,CHAT_TYPE_COMMAND,"er_effects_clear 0");auto pid=P::GetPlayerID(player);if(run.rooted.count(pid))ecs::ChatSystem::Send(player,CHAT_TYPE_COMMAND,"er_effect %u 1 0",ICC::MarrowgarSpell::Impaled);if(HasStorm())ecs::ChatSystem::Send(player,CHAT_TYPE_COMMAND,"er_effect %u 1 %u",ICC::MarrowgarSpell::BoneStorm,(stormEnd-now+999)/1000);ecs::ChatSystem::Send(player,CHAT_TYPE_COMMAND,"er_effects_end 0");if(HasStorm()&&stormMarker)ecs::ChatSystem::Send(player,CHAT_TYPE_COMMAND,"er_marker %u %u %u %d 900 %u",run.id,stormMarker,P::GetPacketVID(boss),int(Mechanic::Storm),stormEnd-now);}
 bool Update(uint32_t time)override{
  now=time;uint32_t elapsed=now-last;last=now;auras.Advance(elapsed,[](const auto&){},0);if(!Valid(boss)||CombatSystem::IsDead(boss))return true;
  if(pendingSpell&&Due(now,castEnd)){auto spell=pendingSpell,target=pendingTarget;pendingSpell=0;ResolveCast(spell,target);}
  if(charging){auto at=Position(boss);float distance=Distance(at,chargeGoal),step=chargeSpeed*elapsed*.001f;auto destination=distance<=step?chargeGoal:Vec{at.x+(chargeGoal.x-at.x)*step/distance,at.y+(chargeGoal.y-at.y)*step/distance};if(!Ray(at,destination))charging=false;else{M::Sync(boss,BaseX+int(destination.x*100),BaseY+int(destination.y*100));if(distance<=step){charging=false;controller.ChargeArrived();}}}
  for(size_t i=0;i<pins.size();){auto& pin=pins[i];SpikeWorld world(*this,pin);pin.controller.Tick(world,elapsed);if(world.remove){Destroy(pin.entity);pins.erase(pins.begin()+i);}else{if(auto member=Participant(run,pin.pid))member->auraEnd=now+2000;++i;}}
  for(size_t i=0;i<trails.size();){TrailWorld world(*this,trails[i]);trails[i].controller.Tick(world,elapsed);if(!trails[i].controller.Moving())trails.erase(trails.begin()+i);else ++i;}
  ground.erase(std::remove_if(ground.begin(),ground.end(),[&](auto& patch){return Due(now,patch.expires);}),ground.end());
  std::set<ICC::AuraKey> occupied;for(auto& patch:ground)for(auto p:Living(run))if(Distance(Position(p),patch.point)<=5){ICC::AuraKey key{69146,patch.source,P::GetPlayerID(p)};if(occupied.insert(key).second)auras.Apply({key,750,1,{{1000,0,0}}});}
  for(auto key:flameAuras)if(!occupied.count(key))auras.Remove(key,ICC::AuraRemoval::Script);flameAuras=std::move(occupied);
  // Remove zone auras immediately on leaving the corresponding trail's area.
  for(auto& member:run.members){if(!Alive(run,Player(member)))auras.RemoveTarget(member.pid,ICC::AuraRemoval::Death);}
  auras.Advance(0,[&](auto& event){Periodic(event);});
  if(HasStorm()&&Due(now,stormNext)){stormNext=now+2000;for(auto p:Living(run))Damage(p,ICC::Marrowgar::StormDamage(Value(69075),Distance(Position(p),Position(boss))));}
  controller.Tick(*this,elapsed);return !evade;
 }
};

