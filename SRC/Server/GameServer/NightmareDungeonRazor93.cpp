
#include "stdafx.h"

#ifdef ENABLE_CPP_DUNGEON_RAZOR93

#include "NightmareDungeonRazor93.h"

#include <unordered_map>
#include <functional>
#include <cstdio>

#include "char.h"
#include "char_manager.h"
#include "config.h"
#include "desc.h"
#include "party.h"
#include "sectree_manager.h"
#include "dungeon.h"
#include "questmanager.h"
#include "log.h"
#include "event.h"

namespace
{
    // ---------- CONFIG (from nightmare_zone_razor93.lua) ----------
    constexpr int32_t kOriginalMap = 373;
    constexpr int32_t kPrivateMin = 3730000;
    constexpr int32_t kPrivateMax = 3740000;

    constexpr uint32_t kEntryNpcVnum = 20088;

    // Requirements
    constexpr int32_t  kMinLevel = 82;
    constexpr int32_t  kMaxLevel = 102;
    constexpr uint32_t kEntryItemVnum = 71095;
    constexpr int32_t  kEntryItemCount = 1;
    constexpr int32_t  kCooldownSeconds = 1200;

    // Rejoin
    constexpr int32_t kRejoinSeconds = 300;
    constexpr int32_t kRejoinX = 211300; // pc.warp(211300, 174100, rejoinIDX)
    constexpr int32_t kRejoinY = 174100;

    // Time limit
    constexpr int32_t kTimeLimitSeconds = 1799;

    // Waves
    constexpr uint32_t kWave1Vnum = 8042;
    constexpr uint32_t kWave2Vnum = 8043;
    constexpr uint32_t kWave3Vnum = 8041;
    constexpr uint32_t kBossVnum = 4203;

    constexpr int32_t kBossX = 568;
    constexpr int32_t kBossY = 247;

    constexpr int32_t kExitNpcX = 563;
    constexpr int32_t kExitNpcY = 247;

    constexpr uint32_t kBonusMobVnum = 2598;

    struct Pos { int32_t x; int32_t y; };
    constexpr Pos kWavePos[6] =
    {
        {604, 276},
        {574, 289},
        {542, 261},
        {550, 228},
        {570, 212},
        {602, 223},
    };

    // Dungeon flags
    constexpr const char* kFlagFloor = "floor";        // lua: floor=2
    constexpr const char* kFlagWasCompleted = "was_completed";
    constexpr const char* kFlagStep2 = "step2";        // 8042 remaining
    constexpr const char* kFlagStep = "step";         // 8043 remaining
    constexpr const char* kFlagStep1 = "step1";        // 8041 remaining

    // PC quest flags (rejoin + cooldown)
    constexpr const char* kQfCooldown = "nightmare_zone_razor93_prepare.cooldown";
    constexpr const char* kQfDisconnect = "nightmare_zone_razor93_prepare.disconnect";
    constexpr const char* kQfIdx = "nightmare_zone_razor93_prepare.idx";
    constexpr const char* kQfCh = "nightmare_zone_razor93_prepare.ch";

    constexpr int32_t kAntiSpamDelay = 1;

    inline bool IsNightmareDungeonMap(int32_t mapIndex)
    {
        return (mapIndex >= kPrivateMin && mapIndex < kPrivateMax);
    }

    // Iterate PCs on a mapIndex (same pattern as ValentineDungeon.cpp)
    struct FForEachPC
    {
        const std::function<void(LPCHARACTER)>& fn;
        void operator()(LPENTITY ent)
        {
            if (!ent || !ent->IsType(ENTITY_CHARACTER))
                return;

            LPCHARACTER ch = (LPCHARACTER)ent;
            if (ch && ch->IsPC())
                fn(ch);
        }
    };

    inline void ForEachPcOnMap(int32_t mapIndex, const std::function<void(LPCHARACTER)>& fn)
    {
        LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(mapIndex);
        if (!map)
            return;

        FForEachPC f{ fn };
        map->for_each(f);
    }

    inline void ChatToMap(int32_t mapIndex, const char* fmt, ...)
    {
        char buf[512];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);

        ForEachPcOnMap(mapIndex, [&](LPCHARACTER ch)
            {
                ch->ChatPacket(CHAT_TYPE_INFO, "%s", buf);
            });
    }

    template <typename TMap>
    inline void CancelEvent(TMap& m, int32_t mapIndex)
    {
        auto it = m.find(mapIndex);
        if (it == m.end())
            return;
        if (it->second)
            event_cancel(&it->second);
        m.erase(it);
    }

    EVENTINFO(nightmare_event_info)
    {
        int32_t mapIndex;
    };

    EVENTFUNC(nm_prepare_event);
    EVENTFUNC(nm_end_event);

    struct CNightmareImpl
    {
        std::unordered_map<int32_t, LPEVENT> m_evPrepare;
        std::unordered_map<int32_t, LPEVENT> m_evEnd;

        void CancelAll(int32_t mapIndex)
        {
            CancelEvent(m_evPrepare, mapIndex);
            CancelEvent(m_evEnd, mapIndex);
        }

        void ClearDungeon(int32_t mapIndex, bool exitLobby)
        {
            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIndex);
            if (d)
            {
                d->KillAll();
                d->ClearRegen();
                if (exitLobby)
                    d->ExitAllLobby(1);
            }
            CancelAll(mapIndex);
        }

        void SchedulePrepare(int32_t mapIndex, int32_t delaySec)
        {
            CancelEvent(m_evPrepare, mapIndex);
            nightmare_event_info* info = AllocEventInfo<nightmare_event_info>();
            info->mapIndex = mapIndex;
            m_evPrepare[mapIndex] = event_create(nm_prepare_event, info, PASSES_PER_SEC(delaySec));
        }

        void ScheduleEnd(int32_t mapIndex, int32_t delaySec)
        {
            CancelEvent(m_evEnd, mapIndex);
            nightmare_event_info* info = AllocEventInfo<nightmare_event_info>();
            info->mapIndex = mapIndex;
            m_evEnd[mapIndex] = event_create(nm_end_event, info, PASSES_PER_SEC(delaySec));
        }

        void StartWave1(int32_t mapIndex)
        {
            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIndex);
            if (!d)
                return;

            d->SetFlag(kFlagStep2, 6);
            d->SetFlag(kFlagStep, 0);
            d->SetFlag(kFlagStep1, 0);

            for (int i = 0; i < 6; ++i)
                d->SpawnMob(kWave1Vnum, kWavePos[i].x, kWavePos[i].y);

            ChatToMap(mapIndex, "[Nightmare] Wave 1 started.");
        }

        void SpawnWave2(int32_t mapIndex)
        {
            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIndex);
            if (!d)
                return;

            d->SetFlag(kFlagStep, 6);
            for (int i = 0; i < 6; ++i)
                d->SpawnMob(kWave2Vnum, kWavePos[i].x, kWavePos[i].y);

            ChatToMap(mapIndex, "[Nightmare] Wave 2 started.");
        }

        void SpawnWave3(int32_t mapIndex)
        {
            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIndex);
            if (!d)
                return;

            d->SetFlag(kFlagStep1, 6);
            for (int i = 0; i < 6; ++i)
                d->SpawnMob(kWave3Vnum, kWavePos[i].x, kWavePos[i].y);

            ChatToMap(mapIndex, "[Nightmare] Wave 3 started.");
        }

        void SpawnBoss(int32_t mapIndex)
        {
            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIndex);
            if (!d)
                return;

            d->SpawnMob(kBossVnum, kBossX, kBossY);
            ChatToMap(mapIndex, "[Nightmare] The boss has appeared!");
        }

        void Complete(int32_t mapIndex)
        {
            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIndex);
            if (!d)
                return;

            if (d->GetFlag(kFlagWasCompleted) != 0)
                return;

            d->SetFlag(kFlagWasCompleted, 1);

            CancelAll(mapIndex);

            d->KillAll();
            d->ClearRegen();

            d->SpawnMob(kEntryNpcVnum, kExitNpcX, kExitNpcY);

            const int32_t bonus = 10 + quest::CQuestManager::instance().GetEventFlag("dungeon_bonus");
            if (number(1, 100) <= bonus)
                d->SpawnMob(kBonusMobVnum, kExitNpcX, kExitNpcY);

            ChatToMap(mapIndex, "[Nightmare] Completed!");
        }
    };

    CNightmareImpl s_nm;

    EVENTFUNC(nm_prepare_event)
    {
        nightmare_event_info* info = (nightmare_event_info*)event->info;
        if (!info)
            return 0;

        const int32_t mapIndex = info->mapIndex;
        s_nm.m_evPrepare.erase(mapIndex);
        s_nm.StartWave1(mapIndex);
        return 0;
    }

    EVENTFUNC(nm_end_event)
    {
        nightmare_event_info* info = (nightmare_event_info*)event->info;
        if (!info)
            return 0;

        const int32_t mapIndex = info->mapIndex;
        s_nm.m_evEnd.erase(mapIndex);

        ChatToMap(mapIndex, "[Nightmare] Time is over. Exiting...");
        s_nm.ClearDungeon(mapIndex, true);
        return 0;
    }

    // ---------------- Entrance checks ----------------
    inline bool CheckLevel(LPCHARACTER ch)
    {
        if (!ch)
            return false;
        const int32_t lv = ch->GetLevel();
        return (lv >= kMinLevel && lv <= kMaxLevel);
    }

    inline int32_t CooldownRemain(LPCHARACTER ch)
    {
        const int32_t now = get_global_time();
        const int32_t until = ch->GetQuestFlag(kQfCooldown);
        return (until > now) ? (until - now) : 0;
    }

    inline void SetCooldown(LPCHARACTER ch)
    {
        ch->SetQuestFlag(kQfCooldown, get_global_time() + kCooldownSeconds);
    }

    inline bool HasEntryItem(LPCHARACTER ch)
    {
        return (ch->CountSpecifyItem(kEntryItemVnum) >= kEntryItemCount);
    }

    inline void RemoveEntryItem(LPCHARACTER ch)
    {
        ch->RemoveSpecifyItem(kEntryItemVnum, kEntryItemCount);
    }

    inline void FormatCooldown(int32_t sec, char* out, size_t outSz)
    {
        const int32_t h = sec / 3600;
        const int32_t m = (sec % 3600) / 60;
        snprintf(out, outSz, "%dh %dm", h, m);
    }

    inline void SetDisconnectFlags(LPCHARACTER ch, int32_t mapIndex)
    {
        ch->SetQuestFlag(kQfDisconnect, get_global_time() + kRejoinSeconds);
        ch->SetQuestFlag(kQfIdx, mapIndex);
        ch->SetQuestFlag(kQfCh, (int32_t)g_bChannel);
    }

    inline void ClearRejoinFlags(LPCHARACTER ch)
    {
        ch->SetQuestFlag(kQfDisconnect, 0);
        ch->SetQuestFlag(kQfIdx, 0);
        ch->SetQuestFlag(kQfCh, 0);
    }
} // anon namespace

CNightmareDungeonRazor93& CNightmareDungeonRazor93::instance()
{
    static CNightmareDungeonRazor93 s;
    return s;
}

void CNightmareDungeonRazor93::OnPlayerDisconnect(CHARACTER* ch)
{
    if (!ch || !ch->IsPC())
        return;

    const int32_t idx = ch->GetMapIndex();
    if (!IsNightmareDungeonMap(idx))
        return;

    SetDisconnectFlags(ch, idx);
}

void CNightmareDungeonRazor93::OnPlayerLogin(CHARACTER* ch)
{
    if (!ch || !ch->IsPC())
        return;

    const int32_t idx = ch->GetMapIndex();
    if (!IsNightmareDungeonMap(idx))
        return;

    // store for rejoin checks (lua)
    ch->SetQuestFlag(kQfIdx, idx);
    ch->SetQuestFlag(kQfCh, (int32_t)g_bChannel);

    LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
    if (!d)
        return;

    if (d->GetFlag(kFlagFloor) == 0)
    {
        d->SetFlag(kFlagFloor, 2);
        d->SetFlag(kFlagWasCompleted, 0);

        s_nm.SchedulePrepare(idx, 1);
        s_nm.ScheduleEnd(idx, kTimeLimitSeconds);
    }
}

void CNightmareDungeonRazor93::OnMobKilled(CHARACTER* killer, CHARACTER* victim)
{
    if (!killer || !victim || !killer->IsPC())
        return;

    const int32_t idx = killer->GetMapIndex();
    if (!IsNightmareDungeonMap(idx))
        return;

    LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
    if (!d || d->GetFlag(kFlagFloor) != 2)
        return;

    const uint32_t vnum = victim->GetRaceNum();

    if (vnum == kBossVnum)
    {
        s_nm.Complete(idx);
        return;
    }

    if (vnum == kWave1Vnum)
    {
        int32_t s = d->GetFlag(kFlagStep2);
        if (s > 0)
        {
            d->SetFlag(kFlagStep2, --s);
            if (s == 0)
                s_nm.SpawnWave2(idx);
        }
        return;
    }

    if (vnum == kWave2Vnum)
    {
        int32_t s = d->GetFlag(kFlagStep);
        if (s > 0)
        {
            d->SetFlag(kFlagStep, --s);
            if (s == 0)
                s_nm.SpawnWave3(idx);
        }
        return;
    }

    if (vnum == kWave3Vnum)
    {
        int32_t s = d->GetFlag(kFlagStep1);
        if (s > 0)
        {
            d->SetFlag(kFlagStep1, --s);
            if (s == 0)
                s_nm.SpawnBoss(idx);
        }
        return;
    }
}

bool CNightmareDungeonRazor93::OnClickNpc(CHARACTER* ch)
{
    if (!ch || !ch->IsPC())
        return false;

    if (!ch->CanWarp())
        return true;

    const int32_t now = get_global_time();
    const int32_t mapIdx = ch->GetMapIndex();

    // We normally warp members from the leader's current map. If the NPC is clicked
    // inside a completed instance, we restart by warping everyone from that instance.
    int32_t originMapForWarp = mapIdx;
    bool fromCompletedInside = false;

    // If clicked inside a completed instance, allow restarting (lua behavior).
    // If clicked inside an active run, do nothing.
    if (IsNightmareDungeonMap(mapIdx))
    {
        LPDUNGEON cur = CDungeonManager::instance().FindByMapIndex(mapIdx);
        if (cur && cur->GetFlag(kFlagWasCompleted) != 0)
        {
            fromCompletedInside = true;
            ch->ChatPacket(CHAT_TYPE_INFO, "[Nightmare] Restarting the dungeon...");
            // Continue with the normal entrance flow below.
        }
        else
        {
            ch->ChatPacket(CHAT_TYPE_INFO, "[Nightmare] You are already inside.");
            return true;
        }
    }

    // Rejoin flow (lua: disconnect window + same channel + not completed)
    const int32_t disconnectUntil = ch->GetQuestFlag(kQfDisconnect);
    const int32_t rejoinIdx = ch->GetQuestFlag(kQfIdx);
    const int32_t rejoinCh = ch->GetQuestFlag(kQfCh);

    if (disconnectUntil > now && rejoinIdx > 0 && rejoinCh == (int32_t)g_bChannel)
    {
        if (IsNightmareDungeonMap(rejoinIdx))
        {
            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(rejoinIdx);
            if (d && d->GetFlag(kFlagWasCompleted) == 0 && d->GetFlag(kFlagFloor) == 2)
            {
                ch->ChatPacket(CHAT_TYPE_INFO, "[Nightmare] Rejoining...");
                ch->WarpSet(kRejoinX, kRejoinY, rejoinIdx);
                ch->SetQuestFlag(kQfDisconnect, 0);
                return true;
            }
        }
    }

    // Anti-spam per channel (simple 5s lock)
    char antiSpamFlag[64];
    snprintf(antiSpamFlag, sizeof(antiSpamFlag), "nightmare_razor93_%d", (int)g_bChannel);
    const int32_t antiSpamUntil = quest::CQuestManager::instance().GetEventFlag(antiSpamFlag);
    if (antiSpamUntil > now)
    {
        ch->ChatPacket(CHAT_TYPE_INFO, "[Nightmare] Please wait a moment.");
        return true;
    }
    quest::CQuestManager::instance().SetEventFlag(antiSpamFlag, now + kAntiSpamDelay);

    // Party rules
    LPPARTY party = ch->GetParty();
    if (party && party->GetLeaderPID() != ch->GetPlayerID())
    {
        ch->ChatPacket(CHAT_TYPE_INFO, "[Nightmare] Only the party leader can enter.");
        return true;
    }

    // Entrance checks (everyone who is on the leader's map will be warped)
    bool ok = true;
    const char* badName = nullptr;
    enum { BAD_NONE, BAD_LEVEL, BAD_ITEM, BAD_COOLDOWN } badType = BAD_NONE;
    int32_t badVal = 0;

    auto checkMember = [&](LPCHARACTER m)
        {
            if (!ok || !m || !m->IsPC())
                return;

            if (!CheckLevel(m))
            {
                ok = false;
                badName = m->GetName();
                badType = BAD_LEVEL;
                badVal = m->GetLevel();
                return;
            }

            const int32_t rem = CooldownRemain(m);
            if (rem > 0)
            {
                ok = false;
                badName = m->GetName();
                badType = BAD_COOLDOWN;
                badVal = rem;
                return;
            }

            if (!HasEntryItem(m))
            {
                ok = false;
                badName = m->GetName();
                badType = BAD_ITEM;
                return;
            }
        };

    if (!party)
        checkMember(ch);
    else
        party->ForEachOnMapMember(checkMember, originMapForWarp);

    if (!ok)
    {
        if (badType == BAD_LEVEL)
        {
            ch->ChatPacket(CHAT_TYPE_INFO, "[Nightmare] %s has an invalid level (Lv%d). Required: %d-%d.",
                badName ? badName : "Someone", badVal, kMinLevel, kMaxLevel);
            return true;
        }
        if (badType == BAD_COOLDOWN)
        {
            char cdBuf[64];
            FormatCooldown(badVal, cdBuf, sizeof(cdBuf));
            ch->ChatPacket(CHAT_TYPE_INFO, "[Nightmare] %s is still on cooldown (%s).", badName ? badName : "Someone", cdBuf);
            return true;
        }
        if (badType == BAD_ITEM)
        {
            ch->ChatPacket(CHAT_TYPE_INFO, "[Nightmare] %s doesn't have the entry item.", badName ? badName : "Someone");
            return true;
        }

        ch->ChatPacket(CHAT_TYPE_INFO, "[Nightmare] Entry check failed.");
        return true;
    }

    // Create dungeon instance
    LPDUNGEON d = CDungeonManager::instance().Create(kOriginalMap);
    if (!d)
        return true;

    d->SetFlag(kFlagFloor, 0); // OnPlayerLogin will initialize to floor 2 and schedule waves/timer
    d->SetFlag(kFlagWasCompleted, 0);

    const int32_t newMapIndex = d->GetMapIndex();

    // Consume items + set cooldown + save return location BEFORE join
    auto applyMember = [&](LPCHARACTER m)
        {
            if (!m || !m->IsPC())
                return;

            RemoveEntryItem(m);
            SetCooldown(m);

            // Save current position as return point for ExitAllLobby.
            // When restarting from inside a completed instance, keep the original return point.
            if (!fromCompletedInside)
                m->SetWarpLocation(m->GetMapIndex(), (int32_t)(m->GetX() / 100), (int32_t)(m->GetY() / 100));
        };

    if (!party)
    {
        applyMember(ch);
        d->Join_Coords(ch, 2113, 1729, kOriginalMap);
    }
    else
    {
        party->ForEachOnMapMember(applyMember, originMapForWarp);
        d->JoinParty_Coords(party, 2113, 1729, originMapForWarp);
    }

    // Clear rejoin flags for the leader (members will be set on logout if needed)
    ClearRejoinFlags(ch);

    ch->ChatPacket(CHAT_TYPE_INFO, "[Nightmare] Entering...");
    return true;
}

#endif // ENABLE_CPP_DUNGEON_RAZOR93
