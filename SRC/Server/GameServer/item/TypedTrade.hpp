#pragma once
#include <cstdint>
#include <entt/entity/fwd.hpp>
// Handles typed participants (including refusing mixed legacy/typed trades).
bool TryStartTypedTrade(entt::entity initiator,entt::entity target);
void CancelTypedTrade(entt::entity participant);
void TickTypedTrades();
bool HandleTypedTradePacket(entt::entity participant,const char* data);
