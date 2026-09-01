#pragma once

#include <cstdint>

#include <entt/entt.hpp>

class CHARACTER;

// Easter Dungeon (map 366 private instances: 3660000..3670000)
// Lua-free implementation.
class CEasterDungeon
{
public:
    static CEasterDungeon& instance();

    void OnPlayerDisconnect(entt::entity character);
    void OnPlayerLogin(entt::entity character);
    void OnMobKilled(entt::entity killer, entt::entity victim);

    // Used by NPC trigger handler.
    bool OnClickNpc(entt::entity character);

    bool IsEasterDungeonMap(int32_t mapIndex) const;
};