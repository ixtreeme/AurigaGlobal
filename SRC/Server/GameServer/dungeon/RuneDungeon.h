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

    // Item use handlers
    bool OnUseItem89103(entt::entity character); // floor key
    bool OnUseItem89102(entt::entity character); // fragment -> key
    bool OnUseItem89100(entt::entity character); // cooldown reset
    bool OnUseTypedKey(entt::entity,const auriga::item::PersistentItem&,const auriga::item::ItemCatalog&);

    // NPC "give item" handler (called from ItemSystem::ReceiveItemEcs)
    bool OnNpcTakeItem(entt::entity from, entt::entity npc, entt::entity item);

    bool IsRuneDungeonMap(int32_t mapIndex) const;
};
