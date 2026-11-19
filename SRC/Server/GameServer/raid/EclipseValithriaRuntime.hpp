#pragma once
// Metin-specific healing ritual replaces WoW class spells/vehicle phasing.
// Defenders fight source-stat adds while portal healers collect dream vigor.
class ValithriaRuntime final:public FestergutRuntime,public ICC::ValithriaPort {
 ICC::Valithria dreamwalker;uint32_t nextWorld=0,finishAt=0;bool ruined=false;
 struct Add {entt::entity entity;unsigned kind=0;uint32_t next=0,special=0,castEnd=0;bool exploded=false;std::map<uint32_t,uint64_t> hate;};std::vector<Add> adds;
 struct Portal {entt::entity entity;uint32_t opens=0,end=0;};std::vector<Portal> portals;
 struct Cloud {entt::entity entity;uint32_t end=0;};std::vector<Cloud> clouds;
 struct Dream {uint32_t end=0,buffEnd=0,stacks=0,healAt=0;Vec returnTo;};std::map<uint32_t,Dream> dreams;
 struct Zone {Vec point;uint32_t at=0,end=0,spell=0;float radius=5;bool mana=false;};std::vector<Zone> zones;
 std::vector<Vec> worms;
 static constexpr uint32_t Creatures[6]={37886,37863,37934,37868,36791,37907};
 unsigned Suppressors()const{unsigned n=0;for(auto& a:adds)if(a.kind==1&&Valid(a.entity)&&!CombatSystem::IsDead(a.entity)&&Distance(Position(a.entity),Position(boss))<10)n++;return n;}
 std::vector<entt::entity> Defenders(){auto list=Living(run);list.erase(std::remove_if(list.begin(),list.end(),[&](auto p){return dreams[P::GetPlayerID(p)].end&&!Due(now,dreams[P::GetPlayerID(p)].end);}),list.end());return list;}
 void DamageAlly(entt::entity source,int64_t amount){if(ruined||finishAt||amount<=0||!Valid(boss))return;auto hp=P::GetHP(boss);if(amount>=hp){P::SetHP(boss,1);ruined=true;Notice(run,"ER_DREAM_DEFEAT");return;}P::SetHP(boss,hp-amount);}
 void AddActor(unsigned kind,Vec point){if(adds.size()>=64||kind>=6)return;auto e=Spawn(run,62035+kind,point,1,0,Creatures[kind]);if(Valid(e))adds.push_back({e,kind,now+2000,now+(kind==4?15000u:10000u),0,false,{}});}
 void ExitDream(uint32_t pid){auto it=dreams.find(pid);if(it==dreams.end()||!it->second.end)return;auto p=Resolve(pid);it->second.end=0;if(Present(run,p)){Place(run,p,it->second.returnTo);Fx(70766,4,p,Position(p),0);Broadcast(run,"er_actor_tint "+std::to_string(P::GetPacketVID(p))+" 255 255 255");}if(auto member=Participant(run,pid)){member->aura=Mechanic::Heal;member->auraEnd=it->second.buffEnd;member->stacks=it->second.stacks;}}
 void CloudCollected(entt::entity p,entt::entity cloud){auto pid=P::GetPlayerID(p);auto& d=dreams[pid];d.stacks=std::min(50u,d.stacks+1);d.buffEnd=now+40000;Fx(ICC::Heroic(run.mode)?71941:70873,3,p,Position(p),40000);Points::Change(p,POINT_SP,50);if(ICC::Heroic(run.mode))Damage(p,std::max<int64_t>(1,Value(71940)));Destroy(cloud);}
 entt::entity Target(Add& a){entt::entity target=entt::null;uint64_t best=0;for(auto p:Defenders()){auto pid=P::GetPlayerID(p);auto hate=a.hate.find(pid);if(hate!=a.hate.end()&&hate->second>best){best=hate->second;target=p;}}return target;}
 void Explode(Add& a){if(a.exploded||!Valid(a.entity))return;a.exploded=true;auto point=Position(a.entity);Fx(70744,5,entt::null,point,2000,10,a.entity);for(auto p:Defenders())if(Distance(Position(p),point)<=10)Damage(p,Value(70744));}
 void Special(Add& a){auto source=a.entity;auto p=Target(a);if(!Alive(run,p)){auto list=Defenders();if(!list.empty())p=list[random.Uniform(0,uint32_t(list.size()-1))];}
  if(a.kind==3&&Alive(run,p)){auto point=Position(p);if(zones.size()<24){bool mana=(now/1000)%2==0;zones.push_back({point,now+3000,now+(mana?18000:4000),mana?71179u:70702u,5,mana});Emit(point,3000,mana?15000:1000,int(Mechanic::FrostBomb),5);Fx(mana?71179:70704,5,entt::null,point,mana?18000:3000,5,source);}for(auto other:Defenders()){Fx(70759,1,other,Position(other),750,0,source);Damage(other,Value(70759));}}
  if(a.kind==4){a.castEnd=now+8000;Fx(69325,3,source,Position(source),8000,0,source);Notice(run,"ER_DREAM_LEY_WASTE");}
  if(a.kind==0){auto point=Position(source);Fx(70633,2,source,point,1800,0,source);for(auto other:Defenders())if(Distance(Position(other),point)<8)Damage(other,Value(70633));if(Distance(point,Position(boss))<8)DamageAlly(source,Value(70633));}
 }
public:
 explicit ValithriaRuntime(Run& r):FestergutRuntime(r,false),dreamwalker(r.id^0xa01){P::SetHP(boss,Points::GetMaxHP(boss)/2);CombatSystem::SetInvincible(boss,false);M::Stop(boss);dreamwalker.Reset(*this);Fx(70904,3,boss,Position(boss),600000);Notice(run,"ER_DREAM_BEGIN");}
 ~ValithriaRuntime()override{for(auto& m:run.members)ExitDream(m.pid);if(Valid(boss))Broadcast(run,"er_lift "+std::to_string(P::GetPacketVID(boss))+" 0");}
 ICC::Difficulty Mode()const override{return run.mode;}
 uint32_t SpawnPeriod(unsigned kind)const override{static constexpr uint32_t ids[5]={70915,70912,70914,70916,70913};auto s=Spell(ids[kind]);return s?std::max(5000u,s->effects[0].period):60000;}
 void Wave(unsigned kind,unsigned count)override{auto c=Arenas[9].center;for(unsigned i=0;i<count;i++){float side=(i+random.Uniform(0,1))%2?1.f:-1.f;Vec point{c.x+side*25,c.y-16+float(i%4)*10};if(!Walk(run,point))point=c;AddActor(kind,point);}}
 void Portals(unsigned count)override{auto c=Arenas[9].center;for(unsigned i=0;i<count&&portals.size()<16;i++){float angle=6.2831853f*i/count;Vec point{c.x+std::cos(angle)*15,c.y+std::sin(angle)*15};auto e=Spawn(run,62033,point,2,0,0);if(Valid(e)){portals.push_back({e,now+15000,now+21000});Fx(ICC::Heroic(run.mode)?71986:71304,3,e,point,21000);}}Notice(run,"ER_DREAM_PORTALS");}
 void HealthNotice(unsigned percent)override{Notice(run,percent==75?"ER_DREAM_75":"ER_DREAM_25");}
 void Victory()override{finishAt=now+3500;Fx(71189,2,boss,Position(boss),3500,35);Fx(71196,3,boss,Position(boss),3500);Broadcast(run,"er_lift "+std::to_string(P::GetPacketVID(boss))+" 1000");Notice(run,"ER_DREAM_VICTORY");}
 void Defeat()override{ruined=true;Notice(run,"ER_DREAM_DEFEAT");}
 bool Action(entt::entity p,const std::string& action,uint32_t target)override{auto pid=P::GetPlayerID(p);auto& d=dreams[pid];if(action=="heal"){
   if(finishAt||ruined||d.end&&!Due(now,d.end)||!Due(now,d.healAt)||Distance(Position(p),Position(boss))>35||P::GetSP(p)<100){Notice(p,"ER_DREAM_HEAL_UNAVAILABLE");return true;}
   Points::Change(p,POINT_SP,-100);d.healAt=now+2000;
   // The Metin ritual must scale with the source dragon's health. A flat 10k
   // was invisible against raid HP and could never keep up with the adds.
   int64_t base=std::max<int64_t>(1,Points::GetMaxHP(boss)/100);unsigned suppression=std::min(100u,Suppressors()*10);unsigned stacks=d.buffEnd&&!Due(now,d.buffEnd)?d.stacks:0;auto amount=base*(100+10*stacks)/100*(100-suppression)/100;auto hp=std::min<int64_t>(Points::GetMaxHP(boss),P::GetHP(boss)+amount);P::SetHP(boss,hp);Fx(70873,1,boss,Position(boss),750,0,p);dreamwalker.Health(*this,hp,Points::GetMaxHP(boss));return true;
  }if(action!="interact")return false;
  if(d.end&&!Due(now,d.end)){for(auto& c:clouds)if(Valid(c.entity)&&P::GetPacketVID(c.entity)==target&&Distance(Position(c.entity),Position(p))<5){CloudCollected(p,c.entity);return true;}Notice(p,"ER_DREAM_NEAR_CLOUD");return true;}
  for(auto& portal:portals)if(Valid(portal.entity)&&P::GetPacketVID(portal.entity)==target&&Distance(Position(p),Position(portal.entity))<6){if(!Due(now,portal.opens)||Due(now,portal.end)){Notice(p,"ER_DREAM_PORTAL_WAIT");return true;}d.returnTo=Position(p);d.end=now+15000;auto c=Arenas[9].center;Vec point{c.x+22,c.y-10};Place(run,p,point);Destroy(portal.entity);Fx(70766,3,p,point,15000);Broadcast(run,"er_actor_tint "+std::to_string(P::GetPacketVID(p))+" 100 230 160");Aura(run,p,Mechanic::Portal,15000);
   for(unsigned i=0;i<10&&clouds.size()<24;i++){Vec at{c.x+14+float(i%3)*7,c.y-20+float(i/3)*12};auto e=Spawn(run,62034,at,2,0,0);if(Valid(e)){clouds.push_back({e,now+20000});Fx(ICC::Heroic(run.mode)?71939:70873,3,e,at,20000);}}Notice(p,"ER_DREAM_ENTERED");return true;}
  Notice(p,"ER_DREAM_NEAR_PORTAL");return true;
 }
 int64_t TargetDamage(entt::entity attacker,entt::entity target,int64_t amount)override{if(target==boss){if(P::IsPC(attacker)||finishAt)return 0;if(amount>=P::GetHP(boss)){ruined=true;return 0;}return amount;}auto pid=P::GetPlayerID(attacker);for(auto& a:adds)if(a.entity==target&&Participant(run,pid)&&amount>0)a.hate[pid]+=uint64_t(amount);return amount;}
 int64_t Outgoing(entt::entity p,int64_t amount)override{auto pid=P::GetPlayerID(p);auto& d=dreams[pid];return d.end&&!Due(now,d.end)?0:amount*(100+10*(d.buffEnd&&!Due(now,d.buffEnd)?d.stacks:0))/100;}
 int64_t Healing(entt::entity p,int64_t amount)override{auto& d=dreams[P::GetPlayerID(p)];return amount*(100+10*(d.buffEnd&&!Due(now,d.buffEnd)?d.stacks:0))/100;}
 void Died(entt::entity actor)override{if(actor==boss){ruined=true;Notice(run,"ER_DREAM_DEFEAT");return;}for(auto& a:adds)if(a.entity==actor){if(a.kind==0&&worms.size()<32)for(unsigned i=0;i<4;i++)worms.push_back(Position(actor));if(a.kind==2)Explode(a);break;}if(P::IsPC(actor))ExitDream(P::GetPlayerID(actor));}
 void Status(entt::entity p)override{auto& d=dreams[P::GetPlayerID(p)];ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"er_dream %u %u %u %u %u",run.id,Suppressors(),d.stacks,d.end&&!Due(now,d.end)?(d.end-now+999)/1000:0,unsigned(uint64_t(P::GetHP(boss))*100/std::max<int64_t>(1,Points::GetMaxHP(boss))));}
 bool Update(uint32_t time)override{now=time;auto elapsed=now-last;last=now;if(ruined)return false;if(finishAt){if(Due(now,finishAt))run.completionPending=true;return true;}dreamwalker.Health(*this,P::GetHP(boss),Points::GetMaxHP(boss));dreamwalker.Tick(*this,elapsed);
  for(auto& member:run.members){auto pid=member.pid;auto p=Player(member);auto& d=dreams[pid];if(d.end&&(Due(now,d.end)||!Alive(run,p)))ExitDream(pid);if(d.buffEnd&&Due(now,d.buffEnd)){d.stacks=0;d.buffEnd=0;}if(d.end&&Alive(run,p))for(auto& c:clouds)if(Valid(c.entity)&&Distance(Position(c.entity),Position(p))<3)CloudCollected(p,c.entity);}
  for(size_t i=0;i<portals.size();)if(!Valid(portals[i].entity)||Due(now,portals[i].end)){Destroy(portals[i].entity);portals.erase(portals.begin()+i);}else ++i;
  for(size_t i=0;i<clouds.size();)if(!Valid(clouds[i].entity)||Due(now,clouds[i].end)){Destroy(clouds[i].entity);clouds.erase(clouds.begin()+i);}else ++i;
  for(size_t i=0;i<zones.size();){auto& z=zones[i];if(Due(now,z.end)){zones.erase(zones.begin()+i);continue;}if(Due(now,z.at)){z.at=now+500;Fx(z.spell,5,entt::null,z.point,800,z.radius);for(auto p:Defenders())if(Distance(Position(p),z.point)<z.radius){if(z.mana)Points::Change(p,POINT_SP,-int64_t(std::min<uint64_t>(100,P::GetSP(p))));else Damage(p,Value(z.spell));}}++i;}
  for(size_t i=0;i<adds.size();){auto& a=adds[i];if(!Valid(a.entity)||CombatSystem::IsDead(a.entity)){Destroy(a.entity);adds.erase(adds.begin()+i);continue;}auto e=a.entity;auto p=Target(a);if(a.kind==1)p=boss;if(!Alive(run,p)&&p!=boss)p=boss;auto point=Position(e),to=Position(p);float range=a.kind==1?8:a.kind==3||a.kind==4?25:3;
   if(Due(now,a.next)){a.next=now+2000;if(Distance(point,to)>range){if(Walk(run,to))M::Goto(e,P::GetX(p),P::GetY(p));}else{M::Stop(e);M::SetRotationToXY(e,P::GetX(p),P::GetY(p));if(a.kind==1){Fx(70588,3,boss,Position(boss),2500,0,e);}else{M::SendMovePacket(e,a.kind==3||a.kind==4?FUNC_MOB_SKILL:FUNC_ATTACK,0,P::GetX(e),P::GetY(e),0);if(p==boss)DamageAlly(e,CalcMeleeDamage(e,boss));else if(a.kind==4){Fx(70754,1,p,to,750,0,e);Damage(p,Value(70754));}else Damage(p,CalcMeleeDamage(e,p));}}}
   if(Due(now,a.special)){a.special=now+(a.kind==4?18000:20000);Special(a);}
   if(a.castEnd&&!Due(now,a.castEnd)&&Due(now,nextWorld)){DamageAlly(e,Value(69325));for(auto other:Defenders())Damage(other,Value(69325));}++i;
  }
  if(Due(now,nextWorld))nextWorld=now+1000;auto queued=std::move(worms);worms.clear();for(auto point:queued)AddActor(5,point);return !ruined;
 }
};
