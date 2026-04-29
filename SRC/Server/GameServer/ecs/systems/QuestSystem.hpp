#pragma once

#include <cstdint>
#include <string>

#include <entt/entt.hpp>

namespace ecs::QuestSystem {

int32_t GetFlag(entt::entity e, const std::string& flagName);
void SetFlag(entt::entity e, const std::string& flagName, int32_t value);

} // namespace ecs::QuestSystem
