
#pragma once

#include <entt/entt.hpp>

#ifdef ENABLE_CPP_DUNGEON_RAZOR93

class CHARACTER;

class CNightmareDungeonRazor93
{
public:
    static CNightmareDungeonRazor93& instance();

    // Called from core hooks
    void OnPlayerLogin(entt::entity character);
    void OnPlayerDisconnect(entt::entity character);
    void OnMobKilled(entt::entity killer, entt::entity victim);

    // Called from trigger.cpp OnClick handler for entry/exit NPC (20088)
    bool OnClickNpc(entt::entity character);

private:
    CNightmareDungeonRazor93() = default;
    CNightmareDungeonRazor93(const CNightmareDungeonRazor93&) = delete;
    CNightmareDungeonRazor93& operator=(const CNightmareDungeonRazor93&) = delete;
};

#endif
