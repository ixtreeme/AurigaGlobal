#pragma once

#include <cstdint>

#include <entt/entt.hpp>

namespace MountSystem {

bool IsRiding(entt::entity rider);
uint32_t GetMountVnum(entt::entity rider);
void SetMountVnum(entt::entity rider, uint32_t vnum);
void ForceClearRidingState(entt::entity rider);

} // namespace MountSystem
