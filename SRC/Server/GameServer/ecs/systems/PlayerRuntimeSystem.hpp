#pragma once

#include <entt/entt.hpp>
#include <string_view>

#include "../../typedef.h"

namespace ecs::PlayerRuntime {

LPDESC GetDesc(entt::entity e);
uint32_t GetPlayerID(entt::entity e);
uint8_t GetEmpire(entt::entity e);
uint8_t GetGMLevel(entt::entity e);
uint32_t GetPacketVID(entt::entity e);
uint32_t GetRaceNum(entt::entity e);
std::string_view GetName(entt::entity e);
int32_t GetMapIndex(entt::entity e);
int32_t GetX(entt::entity e);
int32_t GetY(entt::entity e);
LPSECTREE GetSectree(entt::entity e);
bool IsPC(entt::entity e);
bool IsNPC(entt::entity e);
bool IsStone(entt::entity e);
bool IsMonster(entt::entity e);

} // namespace ecs::PlayerRuntime
