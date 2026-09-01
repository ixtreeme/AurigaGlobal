#pragma once

#include <cstdint>

#include <entt/entt.hpp>

class CHARACTER;
class CItem;

/*
    Frostbane Fortress / Viking dungeon - Lua mentes C++ verzio

    Template map index: 179
    BasePosition: 1971200 2508800 -> cell base: 19712 / 25088

    Fobb hookok:
      - trigger.cpp: NPC 9615 + reward chest 9626 -> CVikingDungeon::OnClickNpc
      - input_login.cpp: CVikingDungeon::OnPlayerLogin
      - char.cpp: CVikingDungeon::OnPlayerDisconnect
      - char_battle.cpp: CVikingDungeon::OnMobKilled
      - char_item.cpp: CHARACTER::ReceiveItem -> CVikingDungeon::OnNpcTakeItem
      - ItemUse.cpp vagy char_item.cpp/use hook: CVikingDungeon::OnUseItem (33018)
*/
class CVikingDungeon
{
public:
    static CVikingDungeon& instance();

    void OnPlayerDisconnect(entt::entity character);
    void OnPlayerLogin(entt::entity character);
    void OnMobKilled(entt::entity killer, entt::entity victim);

    bool OnClickNpc(entt::entity character, entt::entity npc);
    bool OnNpcTakeItem(entt::entity from, entt::entity npc, CItem* item);
    bool OnUseItem(entt::entity character, CItem* item);

    bool IsVikingDungeonMap(int32_t mapIndex) const;
};
