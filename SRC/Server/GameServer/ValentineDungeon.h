#pragma once

#include <cstdint>

#include <entt/entt.hpp>

class CHARACTER;

// Valentine Dungeon (map 377 private instances: 3770000..3780000)
// Lua-free implementation.
class CValentineDungeon
{
public:
    static CValentineDungeon& instance();

    void OnPlayerDisconnect(entt::entity character);
    void OnPlayerLogin(entt::entity character);
    void OnMobKilled(entt::entity killer, entt::entity victim);

    // Used by NPC trigger handler.
    bool OnClickNpc(entt::entity character);

    bool IsValentineDungeonMap(int32_t mapIndex) const;
};
