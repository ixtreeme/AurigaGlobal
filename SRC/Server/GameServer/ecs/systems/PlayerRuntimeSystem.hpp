#pragma once

#include <entt/entt.hpp>
#include <string_view>

#include "../../typedef.h"

class CArena;
class CPetSystem;
class CNewPetSystem;

namespace ecs::PlayerRuntime {

entt::entity FindByPlayerID(uint32_t playerID);
entt::entity FindByPlayerName(std::string_view name);
entt::entity FindByVID(uint32_t vid);
entt::entity FindSpecifyPC(uint32_t jobFlag, int32_t mapIndex, entt::entity except,
	int minLevel, int maxLevel);
LPDESC GetDesc(entt::entity e);
uint32_t GetPlayerID(entt::entity e);
uint32_t GetAccountID(entt::entity e);
uint8_t GetEmpire(entt::entity e);
void SetEmpire(entt::entity e, uint8_t empire);
int ChangeEmpire(entt::entity e, uint8_t empire);
int GetChangeEmpireCount(entt::entity e);
void IncrementChangeEmpireCount(entt::entity e);
uint8_t GetGMLevel(entt::entity e);
void RefreshGMLevel(entt::entity e);
void SetBlockModeForce(entt::entity e, uint8_t flags);
uint32_t GetPacketVID(entt::entity e);
uint32_t GetRaceNum(entt::entity e);
uint8_t GetSex(entt::entity e);
uint8_t GetJob(entt::entity e);
bool ChangeSex(entt::entity e);
bool SetRace(entt::entity e, uint8_t race);
bool SetCostumeHidden(entt::entity e, uint8_t part, bool hidden, bool skipPersistence = false);
bool IsCostumeHidden(entt::entity e, uint8_t part);
std::string_view GetName(entt::entity e);
std::string_view GetPendingName(entt::entity e);
void SetPendingName(entt::entity e, std::string_view name);
int RequestNameChange(entt::entity e, std::string_view name);
int32_t GetMapIndex(entt::entity e);
int32_t GetX(entt::entity e);
int32_t GetY(entt::entity e);
float GetRotation(entt::entity e);
LPSECTREE GetSectree(entt::entity e);
bool IsPC(entt::entity e);
bool IsNPC(entt::entity e);
bool IsStone(entt::entity e);
bool IsMonster(entt::entity e);
uint8_t GetMobRank(entt::entity e);
int GetPremiumRemainSeconds(entt::entity e, uint8_t premiumType);
bool IsPCBang(entt::entity e);
bool IsObserverMode(entt::entity e);
bool IsArenaObserverMode(entt::entity e);
CArena* GetArena(entt::entity e);
void SetArena(entt::entity e, CArena* arena);
bool CanWarp(entt::entity e);
bool IsHack(entt::entity e, bool sendMessage, bool checkShopOwner, int limitTime);
bool IsHack(entt::entity e, bool sendMessage = true, bool checkShopOwner = true);
int GetDuelOption(entt::entity e, const char* option);
entt::entity GetQuestNPC(entt::entity e);
uint32_t GetQuestNPCID(entt::entity e);
bool SetQuestNPCID(entt::entity e, uint32_t id);
uint32_t GetQuestBy(entt::entity e);
bool SetQuestBy(entt::entity e, uint32_t questVnum);
void DestroyCharacter(entt::entity e);
#ifdef __PET_SYSTEM__
CPetSystem* GetPetSystem(entt::entity e);
#endif
#ifdef __NEWPET_SYSTEM__
CNewPetSystem* GetNewPetSystem(entt::entity e);
void SetEggVID(entt::entity e, int vid);
int GetEggVID(entt::entity e);
#endif
#ifdef __DUNGEON_INFO_SYSTEM__
uint64_t GetQuestDamage(entt::entity e, int race);
#endif
#ifdef ENABLE_BATTLE_PASS
uint8_t GetBattlePassID(entt::entity e);
uint32_t GetMissionProgress(entt::entity e, uint32_t missionID, uint32_t battlePassID);
bool UpdateMissionProgress(entt::entity e, uint32_t missionID, uint32_t battlePassID,
    uint32_t updateValue, uint32_t totalValue, bool overrideValue = false);
#endif

#ifdef ENABLE_VOTE4BUFF
int64_t GetVoteCoin(entt::entity e);
bool SetVoteCoin(entt::entity e, int64_t amount);
#endif

#ifdef ENABLE_RANKING
int64_t GetRankPoints(entt::entity e, int category);
bool SetRankPoints(entt::entity e, int category, int64_t value);
#endif

} // namespace ecs::PlayerRuntime
