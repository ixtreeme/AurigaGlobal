#pragma once
#include <cstdint>
#include <memory>
#include <entt/entt.hpp>
class CMob;
namespace EclipseRaid {
struct Actor {uint32_t run=0;int encounter=0,role=0;std::shared_ptr<CMob> prototype;uint32_t next=0,expires=0;uint32_t owner=0,sourceCreature=0;};
void Tick();
bool MovementLocked(entt::entity player);
float MovementSpeed(entt::entity actor);
int64_t FilterHealing(entt::entity player,int64_t amount);
void Dispel(entt::entity player);
void Command(entt::entity player,const char* arguments);
bool AllowCombat(entt::entity attacker,entt::entity victim);
int64_t FilterDamage(entt::entity attacker,entt::entity victim,int64_t amount);
void OnDeath(entt::entity victim,entt::entity killer);
const CMob* Prototype(entt::entity actor);
}
