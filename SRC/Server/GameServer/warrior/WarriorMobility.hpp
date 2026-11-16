#pragma once
#ifndef ENABLE_WARRIOR_MOBILITY
#define ENABLE_WARRIOR_MOBILITY 1
#endif
#include "../core/stdafx.h"
#include "../core/packet.h"
#include "../core/utils.h"
#include "../core/config.h"
#include "../world/event.h"
#include "../ecs/Registry.hpp"
#include "../ecs/SpatialHelpers.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/MovementSystem.hpp"
#include "../ecs/systems/ViewSystem.hpp"
#include "../ecs/systems/MountSystem.hpp"
#include "../ecs/systems/AffectSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../network/desc.h"
#include "WarriorSkillSystem.hpp"
#include "MobilityCollision.hpp"
#include <cmath>
namespace WarriorMobility {
constexpr uint8_t Move=0x20,Target=0x21,Cancel=0x22,Capability=0x23;
struct State {uint32_t began=0,duration=0,lastQuery=0,lastReject=0;int32_t map=0,sx=0,sy=0,ex=0,ey=0,lastX=0,lastY=0;uint8_t kind=0,rotation=0;bool moving=false;};
inline bool Active(entt::entity e){
#if ENABLE_WARRIOR_MOBILITY
 auto* s=g_registry.valid(e)?g_registry.try_get<State>(e):nullptr;return s&&s->moving;
#else
 return false;
#endif
}
inline void ForgetCompleted(entt::entity e){if(auto* s=g_registry.try_get<State>(e);s&&!s->moving)s->duration=0;}
inline double Progress(double t){t=std::clamp(t,0.,1.);return 1.25*t-.25*t*t;}
inline void Packet(entt::entity e,uint8_t function,int32_t x,int32_t y,uint8_t kind,uint8_t rotation,uint32_t time,uint32_t duration,bool broadcast,entt::entity viewer=entt::null){
 TPacketGCMove p{};p.bHeader=HEADER_GC_MOVE;p.bFunc=function;p.bArg=kind;p.bRot=rotation;p.dwVID=ecs::PlayerRuntime::GetPacketVID(e);p.lX=x;p.lY=y;p.dwTime=time;p.dwDuration=duration;
 if(broadcast)ecs::ViewSystem::PacketView(e,&p,sizeof(p),e);
 auto receiver=viewer==entt::null?e:viewer;if(auto* d=ecs::PlayerRuntime::GetDesc(receiver))d->Packet(&p,sizeof(p));
}
inline void Snapshot(entt::entity e,entt::entity viewer){if(!Active(e))return;auto& s=g_registry.get<State>(e);Packet(e,Move,s.sx,s.sy,s.kind,s.rotation,s.began,s.duration,false,viewer);Packet(e,Target,s.ex,s.ey,s.kind,s.rotation,s.began,s.duration,false,viewer);}
inline bool Clear(int32_t map,int32_t x,int32_t y){for(int dx=-18;dx<=18;dx+=18)for(int dy=-18;dy<=18;dy+=18)if(!ecs::IsMovablePosition(map,x+dx,y+dy))return false;return true;}
inline bool Trace(int32_t map,int32_t sx,int32_t sy,int32_t ex,int32_t ey,int32_t& ax,int32_t& ay){return TracePath(sx,sy,ex,ey,ax,ay,[map](int32_t x,int32_t y){return Clear(map,x,y);});}
inline bool Allowed(entt::entity e){return ecs::PlayerRuntime::IsPC(e)&&ecs::PlayerRuntime::GetRaceNum(e)<8&&ecs::PlayerRuntime::GetDesc(e)&&!CombatSystem::IsDead(e)&&ecs::MovementSystem::CanMove(e)&&!MountSystem::IsRiding(e)&&!AffectSystem::IsPolymorphed(e)&&!ecs::PlayerRuntime::IsObserverMode(e)&&!WarriorExpansion::IsCasting(e);}
EVENTINFO(MotionEvent){entt::entity actor=entt::null;uint32_t serial=0;};
inline EVENTFUNC(Tick){auto* info=dynamic_cast<MotionEvent*>(event->info);if(!info||!g_registry.valid(info->actor))return 0;auto e=info->actor;auto* s=g_registry.try_get<State>(e);if(!s||!s->moving||s->began!=info->serial)return 0;
 if(!Allowed(e)||ecs::PlayerRuntime::GetMapIndex(e)!=s->map||std::abs(ecs::PlayerRuntime::GetX(e)-s->lastX)+std::abs(ecs::PlayerRuntime::GetY(e)-s->lastY)>25){s->moving=false;Packet(e,Cancel,ecs::PlayerRuntime::GetX(e),ecs::PlayerRuntime::GetY(e),s->kind,s->rotation,s->began,0,true);return 0;}
 double t=std::min(1.,uint32_t(get_dword_time()-s->began)/double(s->duration)),q=Progress(t);int32_t x=int32_t(std::lround(s->sx+(s->ex-s->sx)*q)),y=int32_t(std::lround(s->sy+(s->ey-s->sy)*q));
 int32_t ax=s->lastX,ay=s->lastY;bool clear=Trace(s->map,s->lastX,s->lastY,x,y,ax,ay);
 if(!clear||!ecs::MovementSystem::Sync(e,x,y)){if(!clear&&(ax!=s->lastX||ay!=s->lastY))ecs::MovementSystem::Sync(e,ax,ay);ecs::MovementSystem::SetRotation(e,s->rotation*5.f);s->moving=false;Packet(e,Cancel,ecs::PlayerRuntime::GetX(e),ecs::PlayerRuntime::GetY(e),s->kind,s->rotation,s->began,0,true);return 0;}
 ecs::MovementSystem::SetRotation(e,s->rotation*5.f);s->lastX=x;s->lastY=y;
 if(t>=1){s->moving=false;return 0;}return std::max<int32_t>(1,PASSES_PER_SEC(.04));
}
inline void Request(entt::entity e,uint8_t kind,uint8_t rotation,uint8_t function,int32_t requestedX,int32_t requestedY,uint32_t serial){
 if(!ecs::PlayerRuntime::IsPC(e)||!ecs::PlayerRuntime::GetDesc(e))return;
 auto& s=g_registry.get_or_emplace<State>(e);uint32_t now=get_dword_time();
 if(function==Capability){if(s.lastQuery&&uint32_t(now-s.lastQuery)<2000)return;s.lastQuery=now;Packet(e,Capability,ecs::PlayerRuntime::GetX(e),ecs::PlayerRuntime::GetY(e),ENABLE_WARRIOR_MOBILITY&&ecs::PlayerRuntime::GetRaceNum(e)<8?1:0,0,now,0,false);return;}
#if ENABLE_WARRIOR_MOBILITY
 if(function==Cancel){
  if(!s.duration||!Allowed(e)||serial!=s.began||uint32_t(now-s.began)>s.duration+1500||ecs::PlayerRuntime::GetMapIndex(e)!=s.map)return;
  double progress=Progress(std::min(1.,uint32_t(now-s.began)/double(s.duration)));int32_t ax=s.sx,ay=s.sy;
  if(StopOnPath(s.sx,s.sy,s.ex,s.ey,requestedX,requestedY,progress)&&Trace(s.map,s.sx,s.sy,requestedX,requestedY,ax,ay)&&ecs::MovementSystem::Sync(e,ax,ay)){
   s.moving=false;s.duration=0;s.lastX=ax;s.lastY=ay;ecs::MovementSystem::SetRotation(e,s.rotation*5.f);Packet(e,Cancel,ax,ay,s.kind,s.rotation,s.began,0,true);
  }
  return;
 }
 if(s.moving&&uint32_t(now-s.began)>=s.duration){int32_t ax=s.lastX,ay=s.lastY;if(Allowed(e)&&ecs::PlayerRuntime::GetMapIndex(e)==s.map&&std::abs(ecs::PlayerRuntime::GetX(e)-s.lastX)+std::abs(ecs::PlayerRuntime::GetY(e)-s.lastY)<=25){Trace(s.map,s.lastX,s.lastY,s.ex,s.ey,ax,ay);ecs::MovementSystem::Sync(e,ax,ay);ecs::MovementSystem::SetRotation(e,s.rotation*5.f);}s.moving=false;}
 if(kind<=4&&rotation<72&&Allowed(e)&&!Active(e)){
  int32_t maximum=kind==0?0:1000;
  // The client may shorten travel for its authored object collision, never extend it.
  int32_t distance=std::clamp(requestedX,0,maximum);double angle=(rotation*5.+(kind==2?90:kind==3?-90:kind==4?180:0))*3.14159265358979323846/180;
  // ActorYaw is 180 - packet angle; map-local Z is -(global Y - baseY).
  // Therefore forward maps to (+sin(angle), +cos(angle)) in server XY.
  int32_t sx=ecs::PlayerRuntime::GetX(e),sy=ecs::PlayerRuntime::GetY(e),dx=int32_t(std::lround(std::sin(angle)*distance)),dy=int32_t(std::lround(std::cos(angle)*distance));int32_t ex=sx,ey=sy,map=ecs::PlayerRuntime::GetMapIndex(e);
  Trace(map,sx,sy,sx+dx,sy+dy,ex,ey);
  ecs::MovementSystem::Stop(e);ecs::MovementSystem::OnMove(e);ecs::MovementSystem::SetRotation(e,rotation*5.f);
  s.began=now;s.duration=kind==0?950:1200;s.map=map;s.sx=sx;s.sy=sy;s.ex=ex;s.ey=ey;s.lastX=sx;s.lastY=sy;s.kind=kind;s.rotation=rotation;s.moving=true;
  Packet(e,Move,sx,sy,kind,rotation,now,s.duration,true);Packet(e,Target,ex,ey,kind,rotation,now,s.duration,true);
  auto* info=AllocEventInfo<MotionEvent>();info->actor=e;info->serial=now;event_create(Tick,info,1);return;
 }
#endif
 if(!s.lastReject||uint32_t(now-s.lastReject)>=200){s.lastReject=now;Packet(e,Cancel,ecs::PlayerRuntime::GetX(e),ecs::PlayerRuntime::GetY(e),kind,rotation,now,0,false);}
}
}
