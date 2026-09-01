#pragma once

#include <entt/entt.hpp>
#include <string_view>

#include "../../typedef.h"

namespace ecs::PlayerRuntime {

entt::entity FindByPlayerID(uint32_t playerID);
entt::entity FindByPlayerName(std::string_view name);
LPDESC GetDesc(entt::entity e);
uint32_t GetPlayerID(entt::entity e);
uint8_t GetEmpire(entt::entity e);
uint8_t GetGMLevel(entt::entity e);
uint32_t GetPacketVID(entt::entity e);
uint32_t GetRaceNum(entt::entity e);
uint8_t GetSex(entt::entity e);
bool ChangeSex(entt::entity e);
bool SetRace(entt::entity e, uint8_t race);
std::string_view GetName(entt::entity e);
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
bool CanWarp(entt::entity e);
entt::entity GetQuestNPC(entt::entity e);

#ifdef ENABLE_VOTE4BUFF
int64_t GetVoteCoin(entt::entity e);
bool SetVoteCoin(entt::entity e, int64_t amount);
#endif

#ifdef ENABLE_RANKING
int64_t GetRankPoints(entt::entity e, int category);
bool SetRankPoints(entt::entity e, int category, int64_t value);
#endif

} // namespace ecs::PlayerRuntime
