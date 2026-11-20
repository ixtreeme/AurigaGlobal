#pragma once

#include <cstdint>
#include <entt/entity/fwd.hpp>

enum class EquipmentViewContext : std::uint8_t { GameMaster = 1, Duel = 2 };

// Only the GM command and consent-checked duel handlers may send this snapshot.
void SendTypedEquipmentView(entt::entity wearer, entt::entity viewer, EquipmentViewContext context);
