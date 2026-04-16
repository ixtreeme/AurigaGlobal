#pragma once

#include <cstdint>

#include <entt/entt.hpp>

namespace CombatSystem {

bool CanBeginFight(entt::entity e);
void BeginFight(entt::entity attacker, entt::entity victim);
bool CanFight(entt::entity e);
bool Attack(entt::entity attacker, entt::entity victim, uint8_t attackType);
bool Shoot(entt::entity attacker, uint8_t attackType);
void SetVictim(entt::entity attacker, entt::entity victim);
entt::entity GetVictim(entt::entity attacker);
entt::entity GetNearestVictim(entt::entity attacker, entt::entity from);
bool IsStun(entt::entity e);
void Stun(entt::entity e);
bool IsDead(entt::entity e);
void SetLastAttacked(entt::entity e, uint32_t tick);
void DeathPenalty(entt::entity e, uint8_t bTown);
void RewardGold(entt::entity victim, entt::entity attacker);

} // namespace CombatSystem

void CombatSystem_Update(entt::registry& reg, uint32_t tick);
