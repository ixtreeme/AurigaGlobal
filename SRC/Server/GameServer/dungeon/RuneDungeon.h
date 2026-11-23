#pragma once

#include <cstdint>

#include <entt/entt.hpp>
#include <item/PersistentItems.hpp>


/*
    Rune Dungeon (Lua-free C++ implementation based on rune_zone.lua)
    Private maps: 2180000..2190000
*/
class CRuneDungeon
{
public:
    static CRuneDungeon& instance();

    void OnPlayerDisconnect(entt::entity character);
    void OnPlayerLogin(entt::entity character);
    void OnMobKilled(entt::entity killer, entt::entity victim);

    // Entry NPC click handler (20506)
    bool OnClickNpc(entt::entity character);

    // Typed fragment and floor-key use.
    bool OnUseTypedKey(entt::entity,const auriga::item::PersistentItem&,const auriga::item::ItemCatalog&);

    bool IsRuneDungeonMap(int32_t mapIndex) const;
};
