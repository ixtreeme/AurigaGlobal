#pragma once
#include <cstdint>
#include <entt/entity/entity.hpp>

std::uint64_t RefreshTypedCompanion(entt::entity owner);
void TickTypedCompanions();
bool HandleTypedCompanionPacket(entt::entity owner, const char* data);
