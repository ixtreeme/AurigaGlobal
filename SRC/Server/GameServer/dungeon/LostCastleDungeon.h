// LostCastleDungeon.h
#pragma once

#include <cstdint>
#include <item/DungeonItems.hpp>

#include <entt/entt.hpp>


/*
    Elveszett Kastely (LostCastle) - Lua mentes C++ dungeon

    Original map: 380
    Private maps: 3800000..3809999

    Fo hookok (mint a tobbi C++ dungeon):
      - trigger.cpp: entry NPC -> CLostCastleDungeon::OnClickNpc
      - char_battle.cpp: OnMobKilled -> CLostCastleDungeon::OnMobKilled
      - (opcionalis) char_battle.cpp/CHARACTER::Damage elejen:
            if (!CLostCastleDungeon::instance().CheckCloneDamage(pAttacker, this)) return false;
*/
class CLostCastleDungeon
{
public:
    bool OnUseTypedItem(entt::entity,const auriga::item::PersistentItem&,const auriga::item::ItemCatalog&);
    static CLostCastleDungeon& instance();

    // Helper: used from char_battle.cpp to allow LostCastle clones (fake players)
    // to execute the normal Dead() pipeline.
    bool IsCloneVID(uint32_t vid) const;

    void OnPlayerDisconnect(entt::entity character);
    void OnPlayerLogin(entt::entity character);
    void OnMobKilled(entt::entity killer, entt::entity victim);

    // Entry NPC click handler
    bool OnClickNpc(entt::entity character);

    // Floor3 clone sebzes-korlatozas (Damage hook)
    bool CheckCloneDamage(entt::entity attacker, entt::entity victim) const;

    bool IsLostCastleMap(int32_t mapIndex) const;

    // ---------------- Dungeon-fuggetlen teszt klonok (GM / item) ----------------
    // /spawn_clon sourcePlayer targetPlayer [count]
    // /p_clon [all|targetPlayer]
    bool SpawnTestClones(entt::entity source, entt::entity target, int32_t count);
    void PurgeTestClonesOnMap(int32_t mapIndex);
    void PurgeTestClonesForTargetPID(uint32_t targetPid, int32_t mapIndex /* -1 = all maps */ = -1);

};
