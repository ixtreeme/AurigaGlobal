
#pragma once

#ifdef ENABLE_CPP_DUNGEON_RAZOR93

class CHARACTER;

class CNightmareDungeonRazor93
{
public:
    static CNightmareDungeonRazor93& instance();

    // Called from core hooks
    void OnPlayerLogin(CHARACTER* ch);
    void OnPlayerDisconnect(CHARACTER* ch);
    void OnMobKilled(CHARACTER* killer, CHARACTER* victim);

    // Called from trigger.cpp OnClick handler for entry/exit NPC (20088)
    bool OnClickNpc(CHARACTER* ch);

private:
    CNightmareDungeonRazor93() = default;
    CNightmareDungeonRazor93(const CNightmareDungeonRazor93&) = delete;
    CNightmareDungeonRazor93& operator=(const CNightmareDungeonRazor93&) = delete;
};

#endif
