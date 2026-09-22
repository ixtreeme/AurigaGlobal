#ifndef __HORSE_H
#define __HORSE_H

#include <entt/entt.hpp>

#include "constants.h"
#include <common/tables.h>
// #ifdef ENABLE_NEWSTUFF
// #include <lua.h>
// #include "stdafx.h"
// #endif

const int HORSE_MAX_LEVEL = 30;

struct THorseStat
{
	int iMinLevel;
	int iNPCRace;
	int iMaxHealth;
	int iMaxStamina;
	int iST;
	int iDX;
	int iHT;
	int iIQ;
	int iDamMean;
	int iDamMin;
	int iDamMax;
	int iArmor;
	int iAttack;
};

extern THorseStat c_aHorseStat[HORSE_MAX_LEVEL+1];

// Native rider state and timers live in ECS; no polymorphic rider object.
namespace MountSystem {
int GetHorseLevel(entt::entity rider);
int GetHorseGrade(entt::entity rider);
int GetHorseHealth(entt::entity rider);
int GetHorseStamina(entt::entity rider);
int GetHorseMaxHealth(entt::entity rider);
int GetHorseMaxStamina(entt::entity rider);
int GetHorseArmor(entt::entity rider);
void SetHorseLevel(entt::entity rider, int level);
void LoadHorseData(entt::entity rider, const THorseInfo& data, uint32_t logoffSeconds = 0);
THorseInfo StoreHorseData(entt::entity rider);
void EnterHorse(entt::entity rider);
bool StartRiding(entt::entity rider);
bool StopRiding(entt::entity rider);
bool ReviveHorse(entt::entity rider);
void FeedHorse(entt::entity rider);
void HorseDie(entt::entity rider);
void ChangeHorseHealth(entt::entity rider, int64_t delta, bool send = true);
void ChangeHorseStamina(entt::entity rider, int64_t delta, bool send = true);
void CheckHorseHealthDropTime(entt::entity rider, bool send = true);
void StopHorseTimers(entt::entity rider);
void ClearHorseInfo(entt::entity rider);
void SendHorseInfo(entt::entity rider);
bool CanUseHorseSkill(entt::entity rider);
} // namespace MountSystem
#endif
