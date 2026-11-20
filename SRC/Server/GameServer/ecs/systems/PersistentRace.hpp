#pragma once

#include <cstdint>
#include <entt/entity/entity.hpp>

namespace ecs::PlayerRuntime
{
bool SaveRaceChange(entt::entity entity, std::uint8_t race);
}
