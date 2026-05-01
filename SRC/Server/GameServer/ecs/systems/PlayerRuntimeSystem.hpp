#pragma once

#include <entt/entt.hpp>

#include "../../typedef.h"

namespace ecs::PlayerRuntime {

LPDESC GetDesc(entt::entity e);
uint32_t GetPlayerID(entt::entity e);
uint8_t GetEmpire(entt::entity e);
uint8_t GetGMLevel(entt::entity e);
uint32_t GetPacketVID(entt::entity e);
bool IsPC(entt::entity e);
bool IsNPC(entt::entity e);
bool IsStone(entt::entity e);
bool IsMonster(entt::entity e);

} // namespace ecs::PlayerRuntime
