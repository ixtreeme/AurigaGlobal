#pragma once

#include <entt/entt.hpp>

#include "../../typedef.h"

class CGuild;
class CExchange;

namespace ecs::SocialSystem {

LPPARTY GetParty(entt::entity e);
CGuild* GetGuild(entt::entity e);
CExchange* GetExchange(entt::entity e);
bool CanDeposit(entt::entity e);
void UpdateDepositPulse(entt::entity e);
bool DepositGuildMoney(entt::entity character, CGuild& guild, int gold);

} // namespace ecs::SocialSystem
