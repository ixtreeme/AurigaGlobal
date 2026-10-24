#pragma once
#include "MissionPassRules.hpp"
#include <entt/entt.hpp>
// Optional event boundary: isolated gameplay regression executables do not need
// the network/database runtime. GameServer installs the sink at startup tick.
namespace AurigaMissionPass::Events {
inline void(*record)(entt::entity,Kind,uint32_t,uint32_t,uint32_t,uint32_t,bool,bool)=nullptr;
inline void(*kill)(entt::entity,entt::entity)=nullptr;
inline void(*enter)(entt::entity,int32_t)=nullptr;
inline void(*complete)(int32_t,uint32_t)=nullptr;
inline void(*reset)(int32_t)=nullptr;
inline void Emit(entt::entity p,Kind k,uint32_t subject=0,uint32_t amount=1){if(record)record(p,k,subject,amount,0,1,false,true);}
inline void Kill(entt::entity p,entt::entity v){if(kill)kill(p,v);}
inline void Enter(entt::entity p,int32_t map){if(enter)enter(p,map);}
inline void Complete(int32_t map){if(complete)complete(map,static_cast<uint32_t>(map/10000));}
inline void Reset(int32_t map){if(reset)reset(map);}
}
