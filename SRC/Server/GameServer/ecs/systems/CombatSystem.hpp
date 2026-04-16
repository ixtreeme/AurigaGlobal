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
void Reward(entt::entity victim, bool bItemDrop);
void ItemDropPenalty(entt::entity victim, entt::entity killer);
void DistributeSP(entt::entity victim, entt::entity killer, int iMethod);
uint32_t GetAlignment(entt::entity e);
uint32_t GetRealAlignment(entt::entity e);
uint8_t GetAlignmentGrade(entt::entity e);
void ApplyAlignmentBonus(entt::entity e);
void UpdateAlignment(entt::entity e, uint32_t amount);
void SetKillerMode(entt::entity e, bool isOn);
bool IsKillerMode(entt::entity e);
void UpdateKillerMode(entt::entity e);
void SetPKMode(entt::entity e, uint8_t bPKMode);
uint8_t GetPKMode(entt::entity e);
void ForgetMyAttacker(entt::entity e);
void AggregateMonster(entt::entity e);
void AggregateMonsterPlus(entt::entity e);
void AttractRanger(entt::entity e);
void PullMonster(entt::entity e);
void SendLeaderboardData(entt::entity e);
void SendLeaderboardDataSkillMob(entt::entity e, entt::entity viewer);
void SendLeaderboardDataGuild(entt::entity e);
void CheckLeaderboardSkillMobChanges(entt::entity e);

} // namespace CombatSystem

void CombatSystem_Update(entt::registry& reg, uint32_t tick);
