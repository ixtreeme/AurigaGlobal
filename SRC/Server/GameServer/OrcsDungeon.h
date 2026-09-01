#pragma once

#include <cstdint>

#include <entt/entt.hpp>

class CHARACTER;

// Orc Dungeon (map 355 private instances: 3550000..3560000)
// Lua-free implementation.
class COrcsDungeon
{
public:
    static COrcsDungeon& instance();

    void OnPlayerDisconnect(entt::entity character);
    void OnPlayerLogin(entt::entity character);
    void OnMobKilled(entt::entity killer, entt::entity victim);

    // Used by NPC trigger handler.
    bool OnClickNpc(entt::entity character);

    bool IsOrcDungeonMap(int32_t mapIndex) const;
};
