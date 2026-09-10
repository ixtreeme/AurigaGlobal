#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <entt/entt.hpp>

#ifdef LEADERBOARD_RAZOR93
// One row of a leaderboard query. It lived in char.h because the two functions
// that build it were static CHARACTER members; neither reads a character.
struct LeaderboardEntry
{
	std::string name;
	int level;
	std::string victim;
	int dmg;
};
#endif

namespace CombatSystem {

void SetAggressive(entt::entity e);
void ResetChatCounter(entt::entity e);
uint8_t GetChatCounter(entt::entity e);
uint8_t IncreaseChatCounter(entt::entity e);
void ResetMountCounter(entt::entity e);
uint8_t GetMountCounter(entt::entity e);
uint8_t IncreaseMountCounter(entt::entity e);
void SetStone(entt::entity e, entt::entity stone);
void ClearStone(entt::entity e);
entt::entity GetSelectedTarget(entt::entity e);
void SendDamagePacket(entt::entity e, entt::entity attacker, int Damage, uint8_t DamageFlag);
void SetTarget(entt::entity e, entt::entity target);
void ClearTarget(entt::entity e);
void CheckTarget(entt::entity e);
void BroadcastTargetPacket(entt::entity e);


// Mob runtime state, formerly CMobInstance. Null for anything that never
// had an instance, which is every PC.
ecs::MobInstanceState* MobState(entt::entity e);
const ecs::MobInstanceState* MobStateConst(entt::entity e);
bool IsBerserk(entt::entity e);
void SetBerserk(entt::entity e, bool value);
bool IsGodSpeed(entt::entity e);
void SetGodSpeed(entt::entity e, bool value);
bool IsRevive(entt::entity e);
void SetRevive(entt::entity e, bool value);
uint32_t GetLastAttackedTime(entt::entity e);
int32_t DistanceFromLastAttacked(entt::entity e);
void SetLastAttacked(entt::entity e, uint32_t when);
bool Return(entt::entity e);
bool Follow(entt::entity self, entt::entity target, float minDistance = 150.0f);
bool IsChangeAttackPosition(entt::entity e, entt::entity target);
void SetChangeAttackPositionTime(entt::entity e);
void ResetChangeAttackPositionTime(entt::entity e);
bool IsStoneSkinner(entt::entity e);
bool IsBerserker(entt::entity e);
bool IsGodSpeeder(entt::entity e);
// The stone this mob guards, else its party leader; entt::null for neither.
// The stone a mob was spawned from, entt::null when it has none.
entt::entity GetStone(entt::entity e);
entt::entity GetProtege(entt::entity e);
void CowardEscape(entt::entity e);
bool CanBeginFight(entt::entity e);
bool CanSummon(entt::entity e, int iLeaderShip);
bool GetDeadByMonster(entt::entity e);
void SetDeadByMonster(entt::entity e, bool value);
uint32_t GetKillerPID(entt::entity e);
void SetKillerPID(entt::entity e, uint32_t pid);
bool IsUndying(entt::entity e);
void SetUndying(entt::entity e, bool value);
bool GetInvincible(entt::entity e);
bool SetInvincible(entt::entity e, bool value);
void IncreaseMobRigHP(entt::entity e, int32_t amount);
void CreateFly(entt::entity attacker, uint8_t flyType, entt::entity victim);
uint32_t GetSkipComboAttackByTime(entt::entity e);
int GetMaxAggro(entt::entity e);
void SetMaxAggro(entt::entity e, int value);
void ChangeVictimByAggro(entt::entity self, int newAggro, entt::entity newVictim);
void BeginFight(entt::entity attacker, entt::entity victim);
bool CanFight(entt::entity e);
bool Attack(entt::entity attacker, entt::entity victim, uint8_t attackType);
bool Damage(entt::entity victim, entt::entity attacker, int64_t dam, uint8_t damageType);
bool Shoot(entt::entity attacker, uint8_t attackType);
void SetVictim(entt::entity attacker, entt::entity victim);
entt::entity GetVictim(entt::entity attacker);
uint32_t GetVictimSetTime(entt::entity attacker);
uint32_t GetLastAttackTime(entt::entity e);
void SetLastAttackTime(entt::entity e, uint32_t time);
bool IsSkillHit(entt::entity e);
void SetSkillHit(entt::entity e, bool value);
uint32_t GetMobDamageMin(entt::entity e);
uint32_t GetMobDamageMax(entt::entity e);
float GetMobDamageMultiplier(entt::entity e);
uint16_t GetMobAttackRange(entt::entity e);
uint8_t GetMobBattleType(entt::entity e);
// Nearest acceptable target within maxDistance, chosen from the sectree.
entt::entity FindVictim(entt::entity self, int maxDistance);
entt::entity GetNearestVictim(entt::entity attacker, entt::entity from);
int GetArrowAndBow(entt::entity e, entt::entity* bow, entt::entity* arrow, int arrowCount = 1);
void UseArrow(entt::entity e, entt::entity arrow, uint32_t count);
bool Shoot(entt::entity e, uint8_t type);
void FlyTarget(entt::entity e, uint32_t targetVID, int32_t x, int32_t y, uint8_t header);
void DetermineDropMetinStone(entt::entity e);
uint32_t GetDropMetinStoneVnum(entt::entity e);
uint8_t GetDropMetinStonePct(entt::entity e);
void DistributeHP(entt::entity victim, entt::entity killer);
void ReviveInvisible(entt::entity e, int duration);
bool IsStun(entt::entity e);
void Stun(entt::entity e);
bool IsDead(entt::entity e);
void Dead(entt::entity victim, entt::entity killer = entt::null, bool immediate = false);
void DeathPenalty(entt::entity e, uint8_t bTown);
void RewardGold(entt::entity victim, entt::entity attacker);
void Reward(entt::entity victim, bool bItemDrop);
void ItemDropPenalty(entt::entity victim, entt::entity killer);
void DistributeSP(entt::entity victim, entt::entity killer, int iMethod);
inline constexpr uint32_t MAX_ALIGNMENT = 50000000;
uint32_t GetAlignment(entt::entity e);
uint32_t GetRealAlignment(entt::entity e);
uint8_t GetAlignmentGrade(entt::entity e);
void UpdateAlignment(entt::entity e, int64_t amount);
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
float GetAttackMultiplier(entt::entity e);
void SetAttackMultiplier(entt::entity e, float multiplier);
float GetDamageMultiplier(entt::entity e);
void SetDamageMultiplier(entt::entity e, float multiplier);
void SendLeaderboardData(entt::entity e);
void SendLeaderboardDataSkillMob(entt::entity e, entt::entity viewer);
void SendLeaderboardDataGuild(entt::entity e);
std::vector<LeaderboardEntry> FetchTop10SkillMob();
void CheckLeaderboardSkillMobChanges();
void SetComboSequence(entt::entity e, uint8_t sequence);
uint8_t GetComboSequence(entt::entity e);
void SetLastComboTime(entt::entity e, uint32_t time);
uint32_t GetLastComboTime(entt::entity e);
void SetValidComboInterval(entt::entity e, int interval);
int GetValidComboInterval(entt::entity e);
uint8_t GetComboIndex(entt::entity e);
uint8_t ToggleComboIndex(entt::entity e, uint8_t skillLevel);

bool IsDeathBlow(entt::entity e);
bool IsDeathBlower(entt::entity e);

} // namespace CombatSystem

void CombatSystem_Update(entt::registry& reg, uint32_t tick);
