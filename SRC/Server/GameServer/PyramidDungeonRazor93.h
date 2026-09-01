#pragma once

#include <cstdint>

#include <entt/entt.hpp>

class CHARACTER;

// Pyramid Dungeon (map 357 private instances: 3570000..3580000)
// Lua-free implementation of dungeonpyramid_razor93 (pyramide_zone.lua).
class CPyramidDungeonRazor93
{
public:
    static CPyramidDungeonRazor93& instance();

    void OnPlayerDisconnect(entt::entity character);
    void OnPlayerLogin(entt::entity character);
    void OnMobKilled(entt::entity killer, entt::entity victim);

    // Used by NPC trigger handler (NPC vnum 9331).
    bool OnClickNpc(entt::entity character);

    bool IsPyramidDungeonMap(int32_t mapIndex) const;
};
