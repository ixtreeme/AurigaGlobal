#pragma once

#include <entt/entt.hpp>

#include "../../typedef.h"

class CGuild;
class CExchange;

namespace ecs::SocialSystem {

LPPARTY GetParty(entt::entity e);
CGuild* GetGuild(entt::entity e);
CExchange* GetExchange(entt::entity e);

} // namespace ecs::SocialSystem
