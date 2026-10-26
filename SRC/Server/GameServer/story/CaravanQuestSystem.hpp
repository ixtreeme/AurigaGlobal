#pragma once
#include <cstdint>
#include <memory>
#include <entt/entt.hpp>
class CMob;
namespace CaravanQuest {
struct Actor {uint32_t run=0;uint8_t role=0;std::shared_ptr<CMob> prototype;uint32_t attackAt=0;int boardMode=0,deckSlot=0;uint32_t moveAt=0,expires=0;int fromX=0,fromY=0;};
struct Rider {uint32_t run=0;};
bool Click(entt::entity player,entt::entity npc);
void Command(entt::entity player,const char* arguments);
void Tick();
bool Locked(entt::entity player);
bool Defending(entt::entity player);
bool AllowCombat(entt::entity attacker,entt::entity victim);
const CMob* Prototype(entt::entity actor);
void OnDeath(entt::entity victim,entt::entity killer);
}
