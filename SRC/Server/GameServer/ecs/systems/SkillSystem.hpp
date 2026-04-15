#pragma once

#include <cstdint>
#include <ctime>

#include <entt/entt.hpp>

#include "../../typedef.h"
#include "../../vid.h"

namespace SkillSystem {

time_t GetSkillNextReadTime(entt::entity e, uint32_t skillId);
void SetSkillNextReadTime(entt::entity e, uint32_t skillId, time_t when);

int GetSkillLevel(entt::entity e, uint32_t skillId);
void SetSkillLevel(entt::entity e, uint32_t skillId, uint8_t level);
uint8_t GetSkillGroup(entt::entity e);
void SetSkillGroup(entt::entity e, uint8_t skillGroup);

bool IsLearnableSkill(entt::entity e, uint32_t skillId);
bool LearnGrandMasterSkill(entt::entity e, uint32_t skillId);
bool LearnSkillByBook(entt::entity e, uint32_t skillId, uint8_t prob = 0);
bool CanUseMobSkill(entt::entity e, unsigned int idx);
bool CanUseSkill(entt::entity e, uint32_t skillId);
bool CheckSkillHit(entt::entity attacker, uint8_t skillId, VID targetVID);
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

} // namespace SkillSystem
