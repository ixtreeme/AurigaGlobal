#pragma once
#include <entt/entt.hpp>
#include <cstdint>
#include <memory>
#include "../ecs/components/identity_components.hpp"
#include "../ecs/Registry.hpp"
class CMob;
namespace SpyQuest {
struct Actor {uint32_t owner=0;uint8_t role=0;bool fighting=false;std::shared_ptr<CMob> prototype;};
struct CinematicLock {};
inline bool CanView(entt::registry& reg,entt::entity source,entt::entity viewer){
 const auto* a=reg.valid(source)?reg.try_get<Actor>(source):nullptr;if(!a)return true;
 const auto* id=reg.valid(viewer)?reg.try_get<ecs::PlayerID>(viewer):nullptr;return id&&id->pid==a->owner;
}
inline bool Locked(entt::registry& reg,entt::entity p){return reg.valid(p)&&reg.all_of<CinematicLock>(p);}
inline bool Locked(entt::entity p){return Locked(g_registry,p);}
inline const CMob* Prototype(entt::entity e){const auto* a=g_registry.valid(e)?g_registry.try_get<Actor>(e):nullptr;return a&&a->prototype?a->prototype.get():nullptr;}
inline bool AllowCombat(entt::entity attacker,entt::entity victim){
 if(Locked(attacker)||Locked(victim))return false;
 for(auto pair:{std::pair{attacker,victim},std::pair{victim,attacker}})if(g_registry.valid(pair.first))if(auto* a=g_registry.try_get<Actor>(pair.first)){
  const auto* id=g_registry.valid(pair.second)?g_registry.try_get<ecs::PlayerID>(pair.second):nullptr;
  if(!a->fighting||!id||!g_registry.all_of<ecs::TagPC>(pair.second)||id->pid!=a->owner)return false;
 }return true;
}
bool Click(entt::entity player,entt::entity npc);
void Command(entt::entity player,const char* arguments);
void Tick();
void OnDeath(entt::entity victim,entt::entity killer);
namespace Hooks {inline void(*death)(entt::entity,entt::entity)=nullptr;}
}
