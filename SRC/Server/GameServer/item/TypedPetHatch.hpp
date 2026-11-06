#pragma once
#include <cstdint>
#include <entt/entity/entity.hpp>

bool HandleTypedPetHatchPacket(entt::entity entity, const char* data);
void SendTypedPetProgress(entt::entity entity,std::uint64_t seal);
