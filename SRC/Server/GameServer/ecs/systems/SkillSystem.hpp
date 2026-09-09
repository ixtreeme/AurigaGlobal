#pragma once

#include <array>
#include <cstdint>
#include <ctime>

#include <entt/entt.hpp>

#include "../../typedef.h"

namespace ecs { struct SkillColor; }

namespace SkillSystem {

time_t GetSkillNextReadTime(entt::entity e, uint32_t skillId);
void SetSkillNextReadTime(entt::entity e, uint32_t skillId, time_t when);

int GetSkillLevel(entt::entity e, uint32_t skillId);
void SetSkillLevel(entt::entity e, uint32_t skillId, uint8_t level);
void SendSkillLevelPacket(entt::entity e);
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
bool CanUseMobSkill(entt::entity e, unsigned int idx);
bool CanUseSkill(entt::entity e, uint32_t skillId);
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
void DisableCooltime(entt::entity e);
void ResetMobSkillCooltime(entt::entity e);
void LearnSkill(entt::entity e, uint32_t skillId);
void SetSkillCooltime(entt::entity e, uint32_t skillId, uint32_t duration, uint32_t tick);
bool IsSkillCooltime(entt::entity e, uint32_t skillId, uint32_t tick);
int GetSkillPoint(entt::entity e);
void AddSkillPoint(entt::entity e, int amount);
int GetSkillMasterType(entt::entity e, uint32_t skillId);
int GetSkillPower(entt::entity e, uint32_t skillId, uint8_t level = 0);
void ComputeSkillPoints(entt::entity e);
void ResetSkill(entt::entity e);
void ClearSkill(entt::entity e);
void ClearSubSkill(entt::entity e);
bool ResetOneSkill(entt::entity e, uint32_t skillId);

} // namespace SkillSystem
