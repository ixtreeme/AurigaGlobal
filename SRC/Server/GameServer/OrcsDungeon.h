#pragma once

#include <cstdint>

class CHARACTER;

// Orc Dungeon (map 355 private instances: 3550000..3560000)
// Lua-free implementation.
class COrcsDungeon
{
public:
    static COrcsDungeon& instance();

    void OnPlayerDisconnect(CHARACTER* ch);
    void OnPlayerLogin(CHARACTER* ch);
    void OnMobKilled(CHARACTER* killer, CHARACTER* victim);

    // Used by NPC trigger handler.
    bool OnClickNpc(CHARACTER* ch);

    bool IsOrcDungeonMap(int32_t mapIndex) const;
};
