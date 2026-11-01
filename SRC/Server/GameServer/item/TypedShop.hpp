#pragma once
#include <cstdint>
#include <expected>
#include <string>
#include <entt/entity/fwd.hpp>

std::expected<void,std::string> LoadTypedShops(const std::string& file);
// False only when the NPC has no typed listing (the legacy caller may continue).
bool TryOpenTypedShop(entt::entity character,entt::entity npc);
void CloseTypedShop(entt::entity character);
void TickTypedShops();
bool HandleTypedShopPacket(entt::entity character,const char* data);
