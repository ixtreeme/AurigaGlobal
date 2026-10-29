#pragma once
// Instance adapter for the source-backed Festergut controller. Creature/spell
// identifiers remain source IDs; visuals use this project's approved assets.
class FestergutRuntime:public SourceRuntime,public ICC::FestergutPort {
protected:
 Run& run;entt::entity boss;ICC::Festergut controller;ICC::EventMap random;ICC::AuraLedger auras;
 std::map<uint32_t,uint64_t> threat;uint32_t now=0,last=0,pending=0,pendingTarget=0,pendingEnd=0;
 int pendingMaximum=0;uint32_t gas=0,nextGas=0,nextMelee=0,inhaleStacks=0;bool berserk=false,failed=false;
 struct Missile {Vec point;uint32_t at=0;};std::vector<Missile> missiles;
 entt::entity Resolve(uint32_t pid)const{for(auto& m:run.members)if(m.pid==pid)return Player(m);return entt::null;}
 const ICC::SpellRecord* Spell(uint32_t id)const{return ICC::Spells().Resolve(id,run.mode);}
 int64_t Value(uint32_t id,size_t effect=0){auto s=Spell(id);if(!s||effect>=3)return 0;auto& e=s->effects[effect];return int64_t(e.basePoints)+(e.dieSides>0?random.Uniform(1,uint32_t(e.dieSides)):e.dieSides);}
 ICC::AuraKey Key(uint32_t id,uint32_t pid)const{return {id,P::GetPacketVID(boss),pid};}
 const ICC::AuraInstance* Effect(uint32_t id,uint32_t pid)const{return auras.Find(Key(id,pid));}
 uint32_t Stacks(uint32_t id,uint32_t pid)const{auto aura=Effect(id,pid);return aura?aura->stacks:0;}
 void Damage(entt::entity p,int64_t value,bool aoe=false){if(!Alive(run,p)||value<=0)return;if(aoe){auto protection=std::min<int64_t>(100,-Value(ICC::FestergutSpell::Inoculated)*Stacks(ICC::FestergutSpell::Inoculated,P::GetPlayerID(p)));value=value*(100-protection)/100;}if(value>0)CombatSystem::Damage(p,boss,value,DAMAGE_TYPE_SPECIAL);}
 void Apply(uint32_t spell,uint32_t pid,uint32_t limit=0){auto s=Spell(spell);if(!s){failed=true;return;}auras.Apply({Key(spell,pid),s->duration,limit?limit:std::max(1u,s->stackLimit),{{s->effects[0].period,s->effects[1].period,s->effects[2].period}}});}
 uint32_t Tank()const{uint32_t pid=0;uint64_t maximum=0;float nearest=1e6f;for(auto& m:run.members){auto p=Player(m);if(!Alive(run,p))continue;auto it=threat.find(m.pid);auto amount=it==threat.end()?0:it->second;float distance=Distance(Position(p),Position(boss));if(!pid||amount>maximum||(amount==maximum&&distance<nearest)){pid=m.pid;maximum=amount;nearest=distance;}}return pid;}
 void Emit(Vec at,uint32_t delay,uint32_t life,int kind=0,float radius=5){std::ostringstream line;line<<"er_hazard "<<run.id<<' '<<run.nextHazard++<<' '<<kind<<" 0 "<<BaseX+int(at.x*100)<<' '<<BaseY+int(at.y*100)<<' '<<int(radius*100)<<" 100 0 0 "<<delay<<' '<<life;Broadcast(run,line.str());}
 void Marker(entt::entity target,uint32_t life){std::ostringstream line;line<<"er_marker "<<run.id<<' '<<run.nextHazard++<<' '<<P::GetPacketVID(target)<<' '<<int(Mechanic::Spore)<<" 800 "<<life;Broadcast(run,line.str());}
 void GooSlow(entt::entity p){
  // Project-native timed effects publish real attack/casting points, and are
  // explicitly excluded from persistence. Remove before refreshing the ratio.
  AffectSystem::RemoveAffect(p,GooAttackSlow);AffectSystem::RemoveAffect(p,GooCastSlow);
  auto factor=1.f/(1.f+float(-Value(72297,1))/100.f);auto duration=Spell(72297)->duration/1000;
  for(auto pair:{std::pair<uint32_t,uint8_t>{GooAttackSlow,POINT_ATT_SPEED},{GooCastSlow,POINT_CASTING_SPEED}}){auto speed=Points::Get(p,pair.second);auto delta=int32_t(std::max<int64_t>(1,int64_t(speed*factor))-speed);AffectSystem::AddAffect(p,pair.first,pair.second,delta,0,duration,0,true);}
 }
 void Finish(uint32_t spell,uint32_t target,int maximum){using namespace ICC::FestergutSpell;
  if(spell==Berserk){berserk=true;return;}
  if(spell==Inhale){inhaleStacks++;return;}
  if(spell==Pungent){for(auto p:Living(run)){Damage(p,Value(Pungent),true);auras.Remove(Key(Inoculated,P::GetPlayerID(p)),ICC::AuraRemoval::Script);}inhaleStacks=0;Talk(6);gas=Blight[0];nextGas=now+2000;return;}
  if(std::find(Blight.begin(),Blight.end(),spell)!=Blight.end()){gas=spell;nextGas=now+2000;return;}
  if(spell==Spore){auto targets=Living(run);for(size_t n=targets.size();n>1;n--)std::swap(targets[n-1],targets[random.Uniform(0,uint32_t(n-1))]);if(maximum>0&&targets.size()>size_t(maximum))targets.resize(maximum);
   for(auto p:targets){auto pid=P::GetPlayerID(p);Apply(69279,pid,1);if(auto m=Participant(run,pid)){m->aura=Mechanic::Spore;m->auraEnd=now+12000;}Marker(p,12000);}return;
  }
  auto p=Resolve(target);if(!Alive(run,p))return;
  if(spell==GastricBloat){Damage(p,Value(GastricBloat));Apply(GastricBloat,target);if(ICC::Festergut::GastricExplodes(Stacks(GastricBloat,target))){auras.Remove(Key(GastricBloat,target),ICC::AuraRemoval::Script);for(auto other:Living(run))if(Distance(Position(other),Position(p))<=10)Damage(other,Value(GastricExplosion),true);Damage(p,int64_t(Points::GetMaxHP(p))*2);}return;}
  if(spell==VileGas){Apply(VileGas,target,1);run.rooted.insert(target);M::Stop(p);ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"er_root 1");return;}
 }
 void Periodic(const ICC::AuraEvent& event){using namespace ICC::FestergutSpell;auto id=event.aura.spec.key.spell;auto pid=event.aura.spec.key.target;auto p=Resolve(pid);if(!Alive(run,p))return;
  if(event.removed){
   if(id==69290&&event.reason==ICC::AuraRemoval::Expired){auto protection=Effect(Inoculated,pid);int remaining=protection?int(protection->expires-auras.Time()):0;int maximum=Spell(Inoculated)->duration;
    if(ICC::Festergut::GrantsInoculation(0,Spell(69290)->duration,protection!=nullptr,remaining,maximum)){Apply(Inoculated,pid);controller.Inoculated(Stacks(Inoculated,pid));}}
   if(id==69279)if(auto m=Participant(run,pid)){m->aura=Mechanic::Strike;m->auraEnd=0;}
   return;
  }
  if(id==69279){for(auto other:Living(run))if(Distance(Position(other),Position(p))<=8)Apply(69290,P::GetPlayerID(other),1);return;}
  if(id==69290){Damage(p,Value(69290),true);return;}
  if(id==VileGas){if(event.effect==0)Damage(p,Value(VileGas));else if(event.effect==2)for(auto other:Living(run))if(other!=p&&Distance(Position(other),Position(p))<=8)Damage(other,Value(69244),true);}
 }
public:
 explicit FestergutRuntime(Run& r,bool initialize=true):run(r),boss(r.bosses.front()),controller(r.id),random(r.id^0x5cc),now(get_dword_time()),last(now),nextMelee(now+2000){if(initialize){controller.Reset(*this);controller.Engaged(*this);}}
 ~FestergutRuntime()override{for(auto& m:run.members){auto p=Player(m);if(Valid(p)){AffectSystem::RemoveAffect(p,GooAttackSlow);AffectSystem::RemoveAffect(p,GooCastSlow);}}}
 ICC::Difficulty Mode()const override{return run.mode;}bool HasVictim()const override{return Tank()!=0;}bool Casting()const override{return pending&&!Due(now,pendingEnd);}uint32_t Victim()const override{return Tank();}
 std::vector<ICC::Candidate> Targets()const override{std::vector<ICC::Candidate> result;for(auto& m:run.members){auto p=Player(m);if(Present(run,p)){float d=Distance(Position(p),Position(boss));result.push_back({m.pid,d,Alive(run,p),P::IsGM(p),false,true,std::max(0.f,d-4.f)});}}return result;}
 void Cast(uint32_t id,uint32_t target=0,bool triggered=false,int maximum=0)override{auto s=Spell(id);if(!s){failed=true;return;}if(!triggered&&Casting())return;if(!triggered&&s->castTime>0){pending=id;pendingTarget=target;pendingMaximum=maximum;pendingEnd=now+uint32_t(s->castTime);M::SendMovePacket(boss,FUNC_MOB_SKILL,0,P::GetX(boss),P::GetY(boss),0);}else Finish(id,target,maximum);}
 void RemoveBlight()override{gas=0;}
 void ProfessorCast(uint32_t spell,uint32_t target)override{auto p=Resolve(target);auto s=Spell(spell);if(!Alive(run,p)||!s||s->missileSpeed<=0)return;auto at=Position(p),from=Arenas[4].center;from.x+=25;from.y+=20;uint32_t travel=uint32_t(Distance(from,at)/s->missileSpeed*1000);if(missiles.size()<16){missiles.push_back({at,now+travel});Emit(at,travel,500,int(Mechanic::Gas),5);}}
 void ProfessorAction(int action)override{if(action==0){gas=ICC::FestergutSpell::Blight[0];nextGas=now+2000;}}
 void Talk(int group)override{auto key="ER_BOSS_5_LINE_"+std::to_string(group);Notice(run,key.c_str());}
 void Melee()override{if(Casting()||!Due(now,nextMelee))return;auto p=Resolve(Tank());if(!Alive(run,p))return;if(Distance(Position(p),Position(boss))>5.5f){M::Goto(boss,P::GetX(p),P::GetY(p));return;}M::Stop(boss);M::SetRotationToXY(boss,P::GetX(p),P::GetY(p));M::SendMovePacket(boss,FUNC_ATTACK,0,P::GetX(boss),P::GetY(boss),0);
  nextMelee=now+uint32_t(float(ICC::Creatures().Resolve(ICC::BossCreatureIDs[run.encounter-1],run.mode)->attackPeriod)/(1.f+.2f*inhaleStacks)/(berserk?2.5f:1.f));Damage(p,int64_t(CalcMeleeDamage(boss,p))*(100+25*inhaleStacks)/100*(berserk?6:1));}
 void Attacked(entt::entity attacker,int64_t amount)override{auto pid=P::GetPlayerID(attacker);if(Participant(run,pid)&&amount>0)threat[pid]+=uint64_t(amount);}
 int64_t Outgoing(entt::entity attacker,int64_t amount)override{return amount*(100+int64_t(Stacks(ICC::FestergutSpell::GastricBloat,P::GetPlayerID(attacker)))*Value(ICC::FestergutSpell::GastricBloat,1))/100;}
 void Died(entt::entity actor)override{if(actor==boss){controller.Died(*this);if(controller.FluShotShortage())Notice(run,"ER_FLU_SHOT_SUCCESS");}else if(P::IsPC(actor))controller.KilledPlayer(*this);}
 void Status(entt::entity p)override{ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"er_effects_clear 0");auto pid=P::GetPlayerID(p);for(auto& effect:auras.ForTarget(pid))ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"er_effect %u %u %u",effect.spec.key.spell,effect.stacks,effect.permanent?0:unsigned((effect.expires>auras.Time()?effect.expires-auras.Time():0)/1000));ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"er_effects_end 0");}
 bool Update(uint32_t time)override{now=time;uint32_t elapsed=now-last;last=now;auras.Advance(elapsed,[](auto&){},0);if(!Valid(boss)||CombatSystem::IsDead(boss))return true;
  for(auto& m:run.members)if(!Alive(run,Player(m)))auras.RemoveTarget(m.pid,ICC::AuraRemoval::Death);
  auras.Advance(0,[&](const auto& e){Periodic(e);});
  for(auto& m:run.members){bool locked=Effect(ICC::FestergutSpell::VileGas,m.pid)!=nullptr;if(locked)run.rooted.insert(m.pid);else run.rooted.erase(m.pid);auto p=Player(m);if(locked&&Alive(run,p))M::Stop(p);if(Present(run,p))ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"er_root %d",locked?1:0);m.stacks=int(Stacks(ICC::FestergutSpell::Inoculated,m.pid));}
  if(pending&&Due(now,pendingEnd)){auto id=pending,target=pendingTarget;pending=0;Finish(id,target,pendingMaximum);}
  if(gas&&Due(now,nextGas)){nextGas=now+2000;auto s=Spell(gas);uint32_t damageSpell=s&&s->effects[0].trigger>0?uint32_t(s->effects[0].trigger):0;if(!Spell(damageSpell))failed=true;else for(auto p:Living(run))Damage(p,Value(damageSpell),true);}
  for(size_t i=0;i<missiles.size();){if(!Due(now,missiles[i].at)){++i;continue;}auto point=missiles[i].point;missiles.erase(missiles.begin()+i);for(auto p:Living(run))if(Distance(Position(p),point)<=5){Damage(p,Value(72297),true);Apply(72297,P::GetPlayerID(p),1);GooSlow(p);}}
  run.resource=int(controller.Inhaled());controller.Tick(*this,elapsed);return !failed;
 }
};
