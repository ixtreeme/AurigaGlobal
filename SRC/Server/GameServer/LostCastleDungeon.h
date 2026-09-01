// LostCastleDungeon.h
#pragma once

#include <cstdint>

#include <entt/entt.hpp>

class CHARACTER;
class CItem; // forward declaration (LPITEM == CItem*)

/*
    Elveszett Kastely (LostCastle) - Lua mentes C++ dungeon

    Original map: 390
    Private maps: 3900000..3910000

    Fo hookok (mint a tobbi C++ dungeon):
      - trigger.cpp: NPC 9006 -> CLostCastleDungeon::OnClickNpc
      - char_battle.cpp: OnMobKilled -> CLostCastleDungeon::OnMobKilled
      - char_item.cpp: ReceiveItem(from,npc,item) -> CLostCastleDungeon::OnNpcTakeItem
      - (opcionalis) char_battle.cpp/CHARACTER::Damage elejen:
            if (!CLostCastleDungeon::instance().CheckCloneDamage(pAttacker, this)) return false;
*/
class CLostCastleDungeon
{
public:
    static CLostCastleDungeon& instance();

    // Helper: used from char_battle.cpp to allow LostCastle clones (fake players)
    // to execute the normal Dead() pipeline.
    bool IsCloneVID(uint32_t vid) const;

    void OnPlayerDisconnect(entt::entity character);
    void OnPlayerLogin(entt::entity character);
    void OnMobKilled(entt::entity killer, entt::entity victim);

    // Entry NPC click handler (9006)
    bool OnClickNpc(entt::entity character);

    // NPC "take item" handler (drag item onto NPC) - statue/totem
    bool OnNpcTakeItem(entt::entity from, entt::entity npc, CItem* item);

    // Floor3 clone sebzes-korlatozas (Damage hook)
    bool CheckCloneDamage(entt::entity attacker, entt::entity victim) const;

    bool IsLostCastleMap(int32_t mapIndex) const;

    // ---------------- Dungeon-fuggetlen teszt klonok (GM / item) ----------------
    // /spawn_clon sourcePlayer targetPlayer [count]
    // /p_clon [all|targetPlayer]
    bool SpawnTestClones(entt::entity source, entt::entity target, int32_t count);
    void PurgeTestClonesOnMap(int32_t mapIndex);
    void PurgeTestClonesForTargetPID(uint32_t targetPid, int32_t mapIndex /* -1 = all maps */ = -1);

    // 30001 item: torli a sajat (targetPid = player) teszt klonjait (dungeon instance-ben blokkolva)
    bool OnUseItem30001(entt::entity character);
};