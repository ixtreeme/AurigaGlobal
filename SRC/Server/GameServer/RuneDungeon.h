#pragma once

#include <cstdint>

class CHARACTER;
class CItem; // forward declaration (LPITEM == CItem*)

/*
    Rune Dungeon (Lua-free C++ implementation based on rune_zone.lua)
    Private maps: 2180000..2190000
*/
class CRuneDungeon
{
public:
    static CRuneDungeon& instance();

    void OnPlayerDisconnect(CHARACTER* ch);
    void OnPlayerLogin(CHARACTER* ch);
    void OnMobKilled(CHARACTER* killer, CHARACTER* victim);

    // Entry NPC click handler (20506)
    bool OnClickNpc(CHARACTER* ch);

    // Item use handlers
    bool OnUseItem89103(CHARACTER* ch); // floor key
    bool OnUseItem89102(CHARACTER* ch); // fragment -> key
    bool OnUseItem89100(CHARACTER* ch); // cooldown reset

    // NPC "give item" handler (called from CHARACTER::ReceiveItem)
    bool OnNpcTakeItem(CHARACTER* from, CHARACTER* npc, CItem* item);

    bool IsRuneDungeonMap(int32_t mapIndex) const;
};
