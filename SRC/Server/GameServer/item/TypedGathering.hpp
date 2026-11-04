#pragma once
#include <entt/entt.hpp>
#include <expected>
#include <string>
std::expected<void,std::string> LoadTypedGathering(const std::string&);
bool HandleTypedGatheringPacket(entt::entity,const char*);
bool TypedResourceActive(entt::entity);
void TickTypedGathering();
