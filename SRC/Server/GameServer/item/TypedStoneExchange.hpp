#pragma once

#include <entt/entity/fwd.hpp>

bool TryOpenTypedStoneExchange(entt::entity character, entt::entity npc);
void RefreshTypedStoneExchange(entt::entity character);
void HandleTypedStoneExchangePacket(entt::entity character, const char* data);
