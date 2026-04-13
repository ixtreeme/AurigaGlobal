#pragma once

#include <cstdint>

class CHARACTER;

// Valentine Dungeon (map 377 private instances: 3770000..3780000)
// Lua-free implementation.
class CValentineDungeon
{
public:
    static CValentineDungeon& instance();

    void OnPlayerDisconnect(CHARACTER* ch);
    void OnPlayerLogin(CHARACTER* ch);
    void OnMobKilled(CHARACTER* killer, CHARACTER* victim);

    // Used by NPC trigger handler.
    bool OnClickNpc(CHARACTER* ch);

    bool IsValentineDungeonMap(int32_t mapIndex) const;
};
