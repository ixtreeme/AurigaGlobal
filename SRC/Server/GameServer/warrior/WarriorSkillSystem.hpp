#pragma once
#include "WarriorSkillRules.hpp"
#include <ctime>
#include <entt/entt.hpp>
namespace WarriorExpansion
{
bool IsEnabled();
bool IsCasting(entt::entity player);
bool AllowBuff(entt::entity player, uint32_t id);
bool CommitLevel(entt::entity player, uint32_t id, uint8_t &level);
bool EnsureLoaded(entt::entity player);
bool IsChosen(entt::entity player, uint32_t id);
void LevelUp(entt::entity player, uint32_t id, uint8_t method);
void PersistLevel(entt::entity player, uint32_t id, uint8_t level);
void PersistReadTime(entt::entity player, uint32_t id, time_t next);
bool ReadBook(entt::entity player, uint32_t id, int64_t experienceCost);
bool ValidateCast(entt::entity player, uint32_t id, entt::entity target);
void AcceptedCast(entt::entity player, uint32_t id, entt::entity target);
void Command(entt::entity player, const char *arguments);
void SendState(entt::entity player);
void OnGroupChanged(entt::entity player);
bool IsServerHit(entt::entity player, uint32_t id);
int ScaleHit(entt::entity player, uint32_t id, int damage);
int64_t ReduceIncoming(entt::entity victim, entt::entity attacker, int64_t damage, uint8_t type);
} // namespace WarriorExpansion
