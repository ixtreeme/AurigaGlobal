#pragma once
#include <cstdint>
#include <expected>
#include <string>
#include <entt/entity/fwd.hpp>
namespace ecs { struct TypedCraftActive {}; }
std::expected<void,std::string> LoadTypedCrafting(const std::string& file);
bool TryOpenTypedCraft(entt::entity character,entt::entity npc);
void CloseTypedCraft(entt::entity character);
void TickTypedCrafting();
bool HandleTypedCraftPacket(entt::entity character,const char* data);
