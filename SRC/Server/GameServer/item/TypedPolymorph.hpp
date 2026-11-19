#pragma once
#include <entt/entity/entity.hpp>
#include <cstdint>
bool GiveTypedPolymorphBook(entt::entity,std::uint32_t mob,std::uint32_t practice,std::uint8_t grade);
bool HandleTypedPolymorphPacket(entt::entity,const char*);
void RefreshTypedPolymorph(entt::entity);
void SendTypedPolymorph(entt::entity);
