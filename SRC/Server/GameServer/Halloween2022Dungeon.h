#pragma once

#include <cstdint>

class CHARACTER;
class CItem;

/*
    Bloody Cathedral / Halloween2022 dungeon - Lua mentes C++ verzio

    Template map index: 161
    BasePosition: 4070400 2252800  -> cell base: 40704 / 22528

    Fobb hookok:
      - trigger.cpp: NPC 9475 + reward chest 9484 -> CHalloween2022Dungeon::OnClickNpc
      - input_login.cpp: CHalloween2022Dungeon::OnPlayerLogin
      - char.cpp: CHalloween2022Dungeon::OnPlayerDisconnect
      - char_battle.cpp: CHalloween2022Dungeon::OnMobKilled
      - char_item.cpp: CHARACTER::ReceiveItem -> CHalloween2022Dungeon::OnNpcTakeItem
*/
class CHalloween2022Dungeon
{
public:
    static CHalloween2022Dungeon& instance();

    void OnPlayerDisconnect(CHARACTER* ch);
    void OnPlayerLogin(CHARACTER* ch);
    void OnMobKilled(CHARACTER* killer, CHARACTER* victim);

    // Entry NPC (9475) es reward chest (9484)
    bool OnClickNpc(CHARACTER* ch, CHARACTER* npc);

    // Drag item onto NPC: 9477 / 9478 / 9479 / 9480 / 9482
    bool OnNpcTakeItem(CHARACTER* from, CHARACTER* npc, CItem* item);

    bool IsHalloweenDungeonMap(int32_t mapIndex) const;
};
