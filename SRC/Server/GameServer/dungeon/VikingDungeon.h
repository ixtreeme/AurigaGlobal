#pragma once

#include <cstdint>
#include <item/DungeonItems.hpp>

#include <entt/entt.hpp>


/*
    Frostbane Fortress / Viking dungeon - Lua mentes C++ verzio

    Template map index: 179
    BasePosition: 1971200 2508800 -> cell base: 19712 / 25088

    Fobb hookok:
      - trigger.cpp: NPC 9615 + reward chest 9626 -> CVikingDungeon::OnClickNpc
      - input_login.cpp: CVikingDungeon::OnPlayerLogin
      - char.cpp: CVikingDungeon::OnPlayerDisconnect
      - char_battle.cpp: CVikingDungeon::OnMobKilled
*/
class CVikingDungeon
{
public:
    bool OnUseTypedItem(entt::entity,const auriga::item::PersistentItem&,const auriga::item::ItemCatalog&);
    static CVikingDungeon& instance();

    void OnPlayerDisconnect(entt::entity character);
    void OnPlayerLogin(entt::entity character);
    void OnMobKilled(entt::entity killer, entt::entity victim);

    bool OnClickNpc(entt::entity character, entt::entity npc);

    bool IsVikingDungeonMap(int32_t mapIndex) const;
};
