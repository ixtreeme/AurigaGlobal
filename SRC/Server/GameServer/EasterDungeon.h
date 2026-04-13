#pragma once

#include <cstdint>

class CHARACTER;

// Easter Dungeon (map 366 private instances: 3660000..3670000)
// Lua-free implementation.
class CEasterDungeon
{
public:
    static CEasterDungeon& instance();

    void OnPlayerDisconnect(CHARACTER* ch);
    void OnPlayerLogin(CHARACTER* ch);
    void OnMobKilled(CHARACTER* killer, CHARACTER* victim);

    // Used by NPC trigger handler.
    bool OnClickNpc(CHARACTER* ch);

    bool IsEasterDungeonMap(int32_t mapIndex) const;
};