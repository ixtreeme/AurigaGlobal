#pragma once
#include <expected>
#include <string>
#include <entt/entity/fwd.hpp>
std::expected<void,std::string> LoadTypedItemMissions(const std::string& file);
bool HandleTypedItemMissionPacket(entt::entity,const char* data);
void TickTypedItemMissions();
