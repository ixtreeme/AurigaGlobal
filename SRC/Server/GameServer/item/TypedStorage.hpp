#pragma once
#include <cstdint>
#include <entt/entity/fwd.hpp>
bool HandleTypedStoragePacket(entt::entity,const char*);
void OpenTypedAccountWallet(entt::entity,std::uint64_t source=0,std::uint32_t units=0);
bool BeginTypedStorageGuildDisband(std::uint32_t guild,std::uint32_t leader);
