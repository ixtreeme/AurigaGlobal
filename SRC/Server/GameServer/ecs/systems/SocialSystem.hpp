#pragma once

#include <entt/entt.hpp>

#include "../../typedef.h"

class CGuild;

namespace ecs::SocialSystem {

LPPARTY GetParty(entt::entity e);
CGuild* GetGuild(entt::entity e);

} // namespace ecs::SocialSystem
