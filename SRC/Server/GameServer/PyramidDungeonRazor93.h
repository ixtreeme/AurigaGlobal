#pragma once

#include <cstdint>

class CHARACTER;

// Pyramid Dungeon (map 357 private instances: 3570000..3580000)
// Lua-free implementation of dungeonpyramid_razor93 (pyramide_zone.lua).
class CPyramidDungeonRazor93
{
public:
    static CPyramidDungeonRazor93& instance();

    void OnPlayerDisconnect(CHARACTER* ch);
    void OnPlayerLogin(CHARACTER* ch);
    void OnMobKilled(CHARACTER* killer, CHARACTER* victim);

    // Used by NPC trigger handler (NPC vnum 9331).
    bool OnClickNpc(CHARACTER* ch);

    bool IsPyramidDungeonMap(int32_t mapIndex) const;
};
