#pragma once

#include <cstdint>
#include <string_view>

#include <entt/entt.hpp>

namespace ecs::QuestSystem {

int32_t GetFlag(entt::entity e, std::string_view flagName);
void SetFlag(entt::entity e, std::string_view flagName, int32_t value);

} // namespace ecs::QuestSystem
