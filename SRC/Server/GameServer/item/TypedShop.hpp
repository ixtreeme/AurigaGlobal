#pragma once
#include <cstdint>
#include <expected>
#include <string>
#include <entt/entity/fwd.hpp>

std::expected<void,std::string> LoadTypedShops(const std::string& file);
// Legacy callers may continue only for characters without a loaded native owner.
bool TryOpenTypedShop(entt::entity character,entt::entity npc);
void CloseTypedShop(entt::entity character);
void TickTypedShops();
bool HandleTypedShopPacket(entt::entity character,const char* data);
