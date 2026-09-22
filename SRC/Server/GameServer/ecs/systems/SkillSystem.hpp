#pragma once

#include <array>
#include <cstdint>
#include <ctime>
#include <unordered_set>

#include <entt/entt.hpp>

#include "../../core/typedef.h"

namespace ecs { struct SkillColor; }

namespace SkillSystem {

time_t GetSkillNextReadTime(entt::entity e, uint32_t skillId);
void SetSkillNextReadTime(entt::entity e, uint32_t skillId, time_t when);
void SetSkillNextReadTimeCapped(entt::entity e, uint32_t skillId, time_t when);

// How a skill level went up. This was an unnamed enum inside CHARACTER;
// it belongs to the level-up path, which lives here now.
enum : uint8_t
{
    SKILL_UP_BY_POINT,
    SKILL_UP_BY_BOOK,
    SKILL_UP_BY_TRAIN,

    // ADD_GRANDMASTER_SKILL
    SKILL_UP_BY_QUEST,
    // END_OF_ADD_GRANDMASTER_SKILL
};

void SkillLevelUp(entt::entity e, uint32_t dwVnum, uint8_t bMethod = SKILL_UP_BY_POINT);
bool SkillLevelDown(entt::entity e, uint32_t dwVnum);
void SkillLearnWaitMoreTimeMessage(entt::entity e, uint32_t ms);
int GetSkillLevel(entt::entity e, uint32_t skillId);
void SetSkillLevel(entt::entity e, uint32_t skillId, uint8_t level);
void SendSkillLevelPacket(entt::entity e);
void LoadSkillLevels(entt::entity e, const TPlayerSkill* src, uint8_t group);
void StoreSkillLevels(entt::entity e, TPlayerSkill* dst);
bool HasSkillLevels(entt::entity e);
uint8_t GetSkillGroup(entt::entity e);
void SetSkillGroup(entt::entity e, uint8_t skillGroup);

#ifdef ENABLE_NEW_PASSIVE_SKILLS
bool CanIncreaseSkill(entt::entity e, uint32_t skillId, bool book = false);
#endif

bool IsLearnableSkill(entt::entity e, uint32_t skillId);
bool LearnGrandMasterSkill(entt::entity e, uint32_t skillId);
bool LearnSkillByBook(entt::entity e, uint32_t skillId, uint8_t prob = 0);
bool HasMobSkill(entt::entity e);
bool UseMobSkill(entt::entity e, unsigned int idx);
const CMob* MobProtoOf(entt::entity e);
const TMobSkillInfo* GetMobSkill(entt::entity e, unsigned int idx);
uint32_t GetLastSkillTime(entt::entity e);
void SetLastSkillTime(entt::entity e, uint32_t when);
uint32_t GetMobSkillCooltime(entt::entity e, unsigned int idx);
void SetMobSkillCooltime(entt::entity e, unsigned int idx, uint32_t when);
void CancelMobSkillEvent(entt::entity e, int index);
void ForgetMobSkillEvent(entt::entity e, int index);
void CancelAllMobSkillEvents(entt::entity e);
// The muyeong and gyeonggong pulses, run while their skill affect lasts.
void StartMuyeongEvent(entt::entity e);
void StopMuyeongEvent(entt::entity e);
#ifdef ENABLE_NEW_GYEONGGONG_SKILL
void StartGyeongGongEvent(entt::entity e);
void StopGyeongGongEvent(entt::entity e);
#endif
bool CanUseMobSkill(entt::entity e, unsigned int idx);
bool CanUseSkill(entt::entity e, uint32_t skillId);
// Whether a movement packet's motion index is a skill e has learned.
bool IsUsableSkillMotion(entt::entity e, uint32_t dwMotionIndex);
// Uses a skill of e on victim: the checks, the HP or SP cost and the
// cooldown, then the skill itself.
bool UseSkill(entt::entity e, uint32_t dwVnum, entt::entity victim, bool bUseGrandMaster = true);
// Runtime skill state belongs to the entity, never to a CHARACTER mirror.
bool RegisterSkillUse(entt::entity caster, uint32_t skillId, bool grandMaster,
    entt::entity target, uint32_t cooldown, int splashCount = 1, int hitCount = -1, int range = -1);
void ResetSkillHitTargets(entt::entity caster, uint32_t skillId);
void SetSkillMainTarget(entt::entity caster, uint32_t skillId, entt::entity target);
entt::entity GetSkillMainTarget(entt::entity caster, uint32_t skillId);
uint32_t GetNextSkillUseTime(entt::entity caster, uint32_t skillId);
bool ConsumeSkillHit(entt::entity caster, uint32_t skillId);
bool CheckSkillHit(entt::entity attacker, uint8_t skillId, entt::entity target);
int GetUsedSkillMasterType(entt::entity caster, uint32_t skillId);
#ifdef __SKILL_COLOR_SYSTEM__
bool SetSkillColors(entt::entity player, const ecs::SkillColor& colors, bool persist = false);
bool ChangeSkillColor(entt::entity player, uint8_t slot, const std::array<uint32_t, 5>& colors);
bool CopyBuffSkillColor(entt::entity caster, entt::entity target, uint32_t skillId);
#endif
int ComputeCooltime(entt::entity e, int time);
bool IsCooltimeDisabled(entt::entity e);
void DisableCooltime(entt::entity e);
void ResetMobSkillCooltime(entt::entity e);
void LearnSkill(entt::entity e, uint32_t skillId);
void SetSkillCooltime(entt::entity e, uint32_t skillId, uint32_t duration, uint32_t tick);
bool IsSkillCooltime(entt::entity e, uint32_t skillId, uint32_t tick);
int GetSkillPoint(entt::entity e);
void AddSkillPoint(entt::entity e, int amount);
int GetSkillMasterType(entt::entity e, uint32_t skillId);
int GetSkillPower(entt::entity e, uint32_t skillId, uint8_t level = 0);
// Applies a skill from e to victim: damage, affects, splash and party share.
int ComputeSkill(entt::entity e, uint32_t dwVnum, entt::entity victim, uint8_t bSkillLevel = 0);
// Applies a splash skill of e centred on a map position.
int ComputeSkillAtPosition(entt::entity e, uint32_t dwVnum, const PIXEL_POSITION& posTarget, uint8_t bSkillLevel = 0);
#ifdef GROUP_BUFF
// Runs ComputeSkill with victim as the caster on each near member of e's
// party, or on e alone when it has none.
int ComputeSkillParty(entt::entity e, uint32_t dwVnum, entt::entity victim, uint8_t bSkillLevel = 0);
#endif
#ifdef ENABLE_NEW_GYEONGGONG_SKILL
// One gyeonggong pulse from e: splash damage around victim.
int ComputeGyeongGongSkill(entt::entity e, uint32_t dwVnum, entt::entity victim, uint8_t bSkillLevel = 0);
#endif
int GetChainLightningMaxCount(entt::entity e);
int GetChainLightningIndex(entt::entity e);
void IncChainLightningIndex(entt::entity e);
void AddChainLightningExcept(entt::entity e, entt::entity target);
void ResetChainLightningIndex(entt::entity e);
const std::unordered_set<entt::entity>& GetChainLightningExcepts(entt::entity e);
void SetAffectedEunhyung(entt::entity e);
void ClearAffectedEunhyung(entt::entity e);
uint32_t GetAffectedEunhyung(entt::entity e);
void ComputeSkillPoints(entt::entity e);
void ResetSkill(entt::entity e);
void ClearSkill(entt::entity e);
void ClearSubSkill(entt::entity e);
bool ResetOneSkill(entt::entity e, uint32_t skillId);

} // namespace SkillSystem
