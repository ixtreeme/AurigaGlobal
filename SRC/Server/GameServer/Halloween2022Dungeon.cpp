#include "stdafx.h"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/SocialSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "ecs/systems/QuestSystem.hpp"
#include "Halloween2022Dungeon.h"

#include <unordered_map>
#include <string>
#include <cstdio>
#include <algorithm>

#include "char_interface.hpp"
#include "char_manager.h"
#include "config.h"
#include "desc.h"
#include "party.h"
#include "sectree_manager.h"
#include "dungeon.h"
#include "item.h"
#include "item_manager.h"
#include "questmanager.h"
#include "event.h"
#include "ecs/systems/ItemSystem.hpp"
#include "ecs/Registry.hpp"
#include "ecs/EntityFactory.hpp"

namespace
{
    // ---------------- CONFIG ----------------
    constexpr int32_t  kOriginalMap = 161;
    constexpr int32_t  kPrivateMin  = 1610000;
    constexpr int32_t  kPrivateMax  = 1620000;

    constexpr uint32_t kEntryNpcVnum    = 9475;
    constexpr uint32_t kRewardChestVnum = 9484;

    constexpr int32_t  kBaseCellX = 40704;
    constexpr int32_t  kBaseCellY = 22528;

    constexpr int32_t  kEnterLocalX = 382;
    constexpr int32_t  kEnterLocalY = 125;
    constexpr int32_t  kRejoinFloor2LocalX = 382;
    constexpr int32_t  kRejoinFloor2LocalY = 292;

    constexpr int32_t  kEnterGlobalX = kBaseCellX + kEnterLocalX;              // 41086
    constexpr int32_t  kEnterGlobalY = kBaseCellY + kEnterLocalY;              // 22653
    constexpr int32_t  kRejoinFloor2GlobalX = kBaseCellX + kRejoinFloor2LocalX; // 41086
    constexpr int32_t  kRejoinFloor2GlobalY = kBaseCellY + kRejoinFloor2LocalY; // 22820

    constexpr int32_t  kMinLevel = 140;
    constexpr int32_t  kMaxLevel = 160;

    constexpr uint32_t kEntryItemVnum  = 30930;
    constexpr int32_t  kEntryItemCount = 1;
    constexpr bool     kEveryMemberNeedsTicket = true; // custom: every entering party member must have ticket

    // Floor 1
    constexpr uint32_t kDoorNpc         = 9476;
    constexpr uint32_t kAngelStatueNpc  = 9477;
    constexpr uint32_t kSealSmallNpc    = 9478;
    constexpr uint32_t kSealMiddleNpc   = 9479;
    constexpr uint32_t kSealFullNpc     = 9480;
    constexpr uint32_t kStoneNpc        = 9481;

    constexpr uint32_t kStoneFullVnum   = 8738;
    constexpr uint32_t kStoneHalfVnum   = 8739;
    constexpr uint32_t kFirstBossVnum   = 4582;

    constexpr uint32_t kStatueItemVnum  = 30931;
    constexpr uint32_t kActivateItemVnum = 30932;

    constexpr int32_t  kFloor1Wave1Kills = 120;
    constexpr int32_t  kFloor1Wave2Kills = 182;

    constexpr const char* kFloor1StatuesRegen  = "data/dungeon/halloween2022_dungeon/1floor_statues.txt";
    constexpr const char* kFloor1Monsters1Regen = "data/dungeon/halloween2022_dungeon/1floor_monsters_1.txt";
    constexpr const char* kFloor1Monsters2Regen = "data/dungeon/halloween2022_dungeon/1floor_monsters_2.txt";

    // Floor 2
    constexpr uint32_t kCalyxEmptyNpc   = 9482;
    constexpr uint32_t kCalyxFullNpc    = 9483;
    constexpr uint32_t kSecondBossVnum  = 4583;
    constexpr uint32_t kSecondFloorItem = 30933;
    constexpr uint32_t kSecondStoneVnum = 8740;
    constexpr uint32_t kFinalBossVnum   = 4584;

    constexpr int32_t  kFloor2KillsNeeded = 232;
    constexpr int32_t  kFinalBossSpawnDelaySec = 10;
    constexpr int32_t  kSecondStoneCountNeeded = 8;

    constexpr const char* kFloor2MonstersRegen = "data/dungeon/halloween2022_dungeon/2floor_monsters.txt";
    constexpr const char* kFloor2StonesRegen   = "data/dungeon/halloween2022_dungeon/2floor_stones.txt";

    // Reward
    constexpr uint32_t kRewardItemVnum  = 30934;
    constexpr int32_t  kRewardItemCount = 1;

    // Timers
    constexpr int32_t kTimeOutSec         = 30 * 60;
    constexpr int32_t kTimeOutNoticeStep  = 2 * 60;
    constexpr int32_t kOutRoomSec         = 2 * 60;
    constexpr int32_t kEntranceCooldownSec = 60 * 60;
    constexpr int32_t kRejoinSec          = 5 * 60;
    constexpr int32_t kAntiSpamSec        = 1;

    // Quest flags
    constexpr const char* kQfCooldown   = "halloween2022.cooldown";
    constexpr const char* kQfDisconnect = "halloween2022.disconnect";
    constexpr const char* kQfIdx        = "halloween2022.idx";
    constexpr const char* kQfCh         = "halloween2022.ch";

    // Dungeon flags
    constexpr const char* kFlagFloor                = "hw22_floor";
    constexpr const char* kFlagInitialized          = "hw22_initialized";
    constexpr const char* kFlagCompleted            = "hw22_completed";
    constexpr const char* kFlagTimeLimit            = "hw22_time_limit";

    constexpr const char* kFlagCanDestroyFirstStone = "hw22_can_destroy_first_stone";
    constexpr const char* kFlagKillFirstBoss        = "hw22_kill_first_boss";
    constexpr const char* kFlagCanDestroyStatue     = "hw22_can_destroy_statue";
    constexpr const char* kFlagCanDestroySecondStone= "hw22_can_destroy_second_stone";
    constexpr const char* kFlagCanActivateSeal      = "hw22_can_activate_seal";

    constexpr const char* kFlagFirstStoneDamaged    = "hw22_f1_stone_damaged";
    constexpr const char* kFlagFirstStoneDestroyed  = "hw22_f1_stone_destroyed";
    constexpr const char* kFlagFirstBossCount       = "hw22_f1_first_boss_count";
    constexpr const char* kFlagAngelStatueCount     = "hw22_f1_angel_statue_count";
    constexpr const char* kFlagSealState            = "hw22_f1_seal_state";
    constexpr const char* kFlagFloor1Monsters       = "hw22_f1_monsters";
    constexpr const char* kFlagFloor1Killed         = "hw22_f1_killed";
    constexpr const char* kFlagFloor1WavesKilled    = "hw22_f1_waves_killed";

    constexpr const char* kFlagCanKillSecondBoss    = "hw22_f2_can_kill_boss";
    constexpr const char* kFlagCanFillCalyx         = "hw22_f2_can_fill_calyx";
    constexpr const char* kFlagCalyxFilled          = "hw22_f2_calyx_filled";
    constexpr const char* kFlagCanDestroySecondFloorStone = "hw22_f2_can_destroy_stone";
    constexpr const char* kFlagSecondFloorStoneCount = "hw22_f2_stone_count";
    constexpr const char* kFlagSecondFloorMonsters  = "hw22_f2_monsters";
    constexpr const char* kFlagSecondFloorKilled    = "hw22_f2_killed";
    constexpr const char* kFlagFinalBossActive      = "hw22_f2_final_boss";

    struct SPosDir { int32_t x, y, dir; };

    constexpr SPosDir kDoorPos       = { 383, 206, 1 };
    constexpr SPosDir kSealPos       = { 383, 182, 1 };
    constexpr SPosDir kFirstBossPos  = { 382, 160, 0 };
    constexpr SPosDir kSecondBossPos = { 382, 292, 5 };
    constexpr SPosDir kSecondStonePos= { 383, 287, 1 };
    constexpr SPosDir kFinalBossPos  = { 383, 331, 1 };
    constexpr SPosDir kRewardChestPos= { 382, 331, 5 };

    constexpr SPosDir kStonePos[5] = {
        { 370, 166, 3 },
        { 370, 146, 5 },
        { 382, 142, 1 },
        { 395, 166, 7 },
        { 395, 146, 4 },
    };

    constexpr SPosDir kCalyxPos[4] = {
        { 404, 322, 1 },
        { 392, 309, 1 },
        { 373, 309, 1 },
        { 361, 322, 1 },
    };

    inline bool IsInRange(int32_t v, int32_t lo, int32_t hi)
    {
        return v >= lo && v < hi;
    }

    template <typename F>
    void ForEachPcOnMap(int32_t mapIndex, F&& fn)
    {
        LPSECTREE_MAP pMap = SECTREE_MANAGER::instance().GetMap(mapIndex);
        if (!pMap)
            return;

        struct FEach
        {
            FEach(F& f) : m_f(f) {}
            F& m_f;
            void operator()(LPENTITY ent)
            {
                if (!ent || ent->GetType() != ENTITY_CHARACTER)
                    return;
                LPCHARACTER ch = (LPCHARACTER)ent;
                if (ch && ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(ch)))
                    m_f(ch);
            }
        } each(fn);

        pMap->for_each(each);
    }

    void NoticeMap(int32_t mapIndex, const char* fmt, ...)
    {
        char buf[CHAT_MAX_LEN + 64];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);

        ForEachPcOnMap(mapIndex, [&](LPCHARACTER pc)
        {
            if (pc)
                ecs::ChatSystem::Send(AIHelpers::EcsOf(pc), CHAT_TYPE_NOTICE, "%s", buf);
        });
    }

    void BigNoticeMap(int32_t mapIndex, const char* fmt, ...)
    {
        char buf[CHAT_MAX_LEN + 64];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);

        ForEachPcOnMap(mapIndex, [&](LPCHARACTER pc)
        {
            if (pc)
                ecs::ChatSystem::Send(AIHelpers::EcsOf(pc), CHAT_TYPE_BIG_NOTICE, "%s", buf);
        });
    }

    void FormatDuration(int32_t sec, char* out, size_t outSz)
    {
        if (sec < 0)
            sec = 0;
        const int32_t h = sec / 3600;
        const int32_t m = (sec % 3600) / 60;
        const int32_t s = sec % 60;
        if (h > 0)
            snprintf(out, outSz, "%dh %dm %ds", h, m, s);
        else if (m > 0)
            snprintf(out, outSz, "%dm %ds", m, s);
        else
            snprintf(out, outSz, "%ds", s);
    }

    int32_t GetOutsideMapByEmpire(uint8_t empire)
    {
        switch (empire)
        {
            case 1: return 1;
            case 2: return 21;
            case 3: return 41;
        }
        return 1;
    }

    void GetOutsideCellByEmpire(uint8_t empire, int32_t& x, int32_t& y)
    {
        switch (empire)
        {
            case 1: x = 4096 + 480; y = 8960 + 736; return;
            case 2: x =    0 + 557; y = 1024 + 555; return;
            case 3: x = 9216 + 480; y = 2048 + 736; return;
            default: x = 4096 + 480; y = 8960 + 736; return;
        }
    }

    void SetOutsideWarpLocation(LPCHARACTER ch)
    {
        if (!ch)
            return;
        int32_t x = 0, y = 0;
        GetOutsideCellByEmpire(ecs::PlayerRuntime::GetEmpire(AIHelpers::EcsOf(ch)), x, y);
        ch->SetWarpLocation(GetOutsideMapByEmpire(ecs::PlayerRuntime::GetEmpire(AIHelpers::EcsOf(ch))), x, y);
    }

    void WarpOut(LPCHARACTER ch)
    {
        if (!ch)
            return;
        int32_t x = 0, y = 0;
        GetOutsideCellByEmpire(ecs::PlayerRuntime::GetEmpire(AIHelpers::EcsOf(ch)), x, y);
        ch->WarpSet(x * 100, y * 100, GetOutsideMapByEmpire(ecs::PlayerRuntime::GetEmpire(AIHelpers::EcsOf(ch))));
    }

    void WarpAllOut(int32_t mapIndex)
    {
        ForEachPcOnMap(mapIndex, [&](LPCHARACTER pc)
        {
            WarpOut(pc);
        });
    }

    void RemoveOneGivenItem(entt::entity itemEntity, const char* why)
    {
        if (itemEntity == entt::null)
            return;
        if (ItemSystem::GetItemCount(itemEntity) > 1)
            ItemSystem::ConsumeItemEcs(itemEntity);
        else
            ItemSystem::DestroyItemEntityEcs(itemEntity, why);
    }

    void DropItemOnGround(LPCHARACTER victim, LPCHARACTER owner, uint32_t vnum, uint32_t count)
    {
        if (!victim)
            return;

        LPITEM item = ITEM_MANAGER::instance().CreateItem(vnum, count);
        if (!item)
            return;

        PIXEL_POSITION pos;
        pos.x = victim->GetX() + number(-200, 200);
        pos.y = victim->GetY() + number(-200, 200);
        pos.z = victim->GetZ();

        item->AddToGround(victim->GetMapIndex(), pos);
        item->StartDestroyEvent();

        if (owner)
            item->SetOwnership(owner, 60 * 3);
    }

    bool IsEntryMapForEmpire(LPCHARACTER ch)
    {
        if (!ch)
            return false;
        return ch->GetMapIndex() == 219;
    }

    int32_t CooldownRemain(LPCHARACTER ch)
    {
        if (!ch)
            return 0;
        const int32_t now = get_global_time();
        const int32_t until = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), kQfCooldown);
        return (until > now) ? (until - now) : 0;
    }

    void SetCooldown(LPCHARACTER ch)
    {
        if (ch)
            ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfCooldown, get_global_time() + kEntranceCooldownSec);
    }

    void SetRejoinFlags(LPCHARACTER ch, int32_t mapIndex)
    {
        if (!ch)
            return;
        ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfDisconnect, get_global_time() + kRejoinSec);
        ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfIdx, mapIndex);
        ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfCh, (int32_t)g_bChannel);
    }

    void ClearRejoinFlags(LPCHARACTER ch)
    {
        if (!ch)
            return;
        ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfDisconnect, 0);
        ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfIdx, 0);
        ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfCh, 0);
    }

    int32_t GetPartyOnlineCountOnMap(LPPARTY party, int32_t mapIndex)
    {
        int32_t count = 0;
        if (!party)
            return 0;
        auto fn = [&](LPCHARACTER pc)
        {
            if (pc && ecs::SocialSystem::GetParty(AIHelpers::EcsOf(pc)) == party)
                ++count;
        };
        party->ForEachOnMapMember(fn, mapIndex);
        return count;
    }

    void SetDungeonReady(LPDUNGEON d)
    {
        if (!d)
            return;

        d->SetFlag(kFlagInitialized, 1);
        d->SetFlag(kFlagCompleted, 0);
        d->SetFlag(kFlagFloor, 1);
        d->SetFlag(kFlagTimeLimit, get_global_time() + kTimeOutSec);

        d->SetFlag(kFlagCanDestroyFirstStone, 1);
        d->SetFlag(kFlagKillFirstBoss, 0);
        d->SetFlag(kFlagCanDestroyStatue, 0);
        d->SetFlag(kFlagCanDestroySecondStone, 0);
        d->SetFlag(kFlagCanActivateSeal, 0);

        d->SetFlag(kFlagFirstStoneDamaged, 0);
        d->SetFlag(kFlagFirstStoneDestroyed, 0);
        d->SetFlag(kFlagFirstBossCount, 0);
        d->SetFlag(kFlagAngelStatueCount, 0);
        d->SetFlag(kFlagSealState, 0);
        d->SetFlag(kFlagFloor1Monsters, 0);
        d->SetFlag(kFlagFloor1Killed, 0);
        d->SetFlag(kFlagFloor1WavesKilled, 0);

        d->SetFlag(kFlagCanKillSecondBoss, 0);
        d->SetFlag(kFlagCanFillCalyx, 0);
        d->SetFlag(kFlagCalyxFilled, 0);
        d->SetFlag(kFlagCanDestroySecondFloorStone, 0);
        d->SetFlag(kFlagSecondFloorStoneCount, 0);
        d->SetFlag(kFlagSecondFloorMonsters, 0);
        d->SetFlag(kFlagSecondFloorKilled, 0);
        d->SetFlag(kFlagFinalBossActive, 0);

        for (int i = 0; i < 5; ++i)
        {
            LPCHARACTER stone = d->SpawnMob(kStoneFullVnum, kStonePos[i].x, kStonePos[i].y, kStonePos[i].dir);
            if (stone)
            {
                char key[32];
                snprintf(key, sizeof(key), "hw22_stone_%d", i + 1);
		d->SetUnique(key, ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(stone)));
            }
        }

        d->SpawnMob(kDoorNpc, kDoorPos.x, kDoorPos.y, kDoorPos.dir);
        d->SpawnMob(kSealSmallNpc, kSealPos.x, kSealPos.y, kSealPos.dir);
        d->SpawnRegen(kFloor1StatuesRegen);
    }

    void SpawnFirstBoss(int32_t mapIndex)
    {
        LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIndex);
        if (!d)
            return;

        d->SpawnMob(kFirstBossVnum, kFirstBossPos.x, kFirstBossPos.y);
        NoticeMap(mapIndex, "<Bloody cathedral> You have damaged all cursed stones.");
        NoticeMap(mapIndex, "<Bloody cathedral> Kill the first boss to obtain the required item.");
    }

    void ReplaceUniqueCalyx(LPDUNGEON d, LPCHARACTER npc)
    {
        if (!d || !npc)
            return;

        for (int i = 0; i < 4; ++i)
        {
            char key[32];
            snprintf(key, sizeof(key), "hw22_calyx_%d", i + 1);
		if (ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(npc)) == (uint32_t)d->GetUniqueVid(key))
            {
                d->SpawnMob(kCalyxFullNpc, kCalyxPos[i].x, kCalyxPos[i].y, kCalyxPos[i].dir);
                d->KillUnique(key);
                return;
            }
        }
    }

    EVENTINFO(halloween2022_event_info)
    {
        int32_t mapIndex;
        halloween2022_event_info() : mapIndex(0) {}
    };

    EVENTFUNC(halloween2022_timeout_event);
    EVENTFUNC(halloween2022_final_boss_event);
    EVENTFUNC(halloween2022_out_event);

    class CHalloween2022DungeonImpl
    {
    public:
        std::unordered_map<int32_t, LPEVENT> m_evTimeout;
        std::unordered_map<int32_t, LPEVENT> m_evFinalBoss;
        std::unordered_map<int32_t, LPEVENT> m_evOut;

        void Cancel(std::unordered_map<int32_t, LPEVENT>& map, int32_t idx)
        {
            auto it = map.find(idx);
            if (it == map.end())
                return;
            if (it->second)
                event_cancel(&it->second);
            map.erase(it);
        }

        void CancelAll(int32_t idx)
        {
            Cancel(m_evTimeout, idx);
            Cancel(m_evFinalBoss, idx);
            Cancel(m_evOut, idx);
        }

        void ScheduleTimeout(int32_t idx)
        {
            Cancel(m_evTimeout, idx);
            halloween2022_event_info* info = AllocEventInfo<halloween2022_event_info>();
            info->mapIndex = idx;
            m_evTimeout[idx] = event_create(halloween2022_timeout_event, info, PASSES_PER_SEC(kTimeOutNoticeStep));
        }

        void ScheduleFinalBoss(int32_t idx, int32_t sec)
        {
            Cancel(m_evFinalBoss, idx);
            halloween2022_event_info* info = AllocEventInfo<halloween2022_event_info>();
            info->mapIndex = idx;
            m_evFinalBoss[idx] = event_create(halloween2022_final_boss_event, info, PASSES_PER_SEC(sec));
        }

        void ScheduleOut(int32_t idx, int32_t sec)
        {
            Cancel(m_evOut, idx);
            halloween2022_event_info* info = AllocEventInfo<halloween2022_event_info>();
            info->mapIndex = idx;
            m_evOut[idx] = event_create(halloween2022_out_event, info, PASSES_PER_SEC(sec));
        }

        long OnTimeout(int32_t idx)
        {
            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
            if (!d)
            {
                m_evTimeout.erase(idx);
                return 0;
            }

            if (d->GetFlag(kFlagCompleted) != 0)
            {
                m_evTimeout.erase(idx);
                return 0;
            }

            const int32_t now = get_global_time();
            const int32_t limit = d->GetFlag(kFlagTimeLimit);
            if (limit <= 0)
            {
                m_evTimeout.erase(idx);
                return 0;
            }

            if (now >= limit)
            {
                m_evTimeout.erase(idx);
                NoticeMap(idx, "<Bloody cathedral> Time expired.");
                NoticeMap(idx, "<Bloody cathedral> You will be teleported out of the dungeon.");
                CancelAll(idx);
                WarpAllOut(idx);
                return 0;
            }

            char tmp[64];
            FormatDuration(limit - now, tmp, sizeof(tmp));
            NoticeMap(idx, "<Bloody cathedral> Time remaining: %s.", tmp);
            return PASSES_PER_SEC(kTimeOutNoticeStep);
        }

        long OnFinalBossSpawn(int32_t idx)
        {
            m_evFinalBoss.erase(idx);
            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
            if (!d)
                return 0;
            if (d->GetFlag(kFlagCompleted) != 0)
                return 0;

            d->SetFlag(kFlagFinalBossActive, 1);
            LPCHARACTER boss = d->SpawnMob(kFinalBossVnum, kFinalBossPos.x, kFinalBossPos.y, kFinalBossPos.dir);
            if (boss)
		d->SetUnique("hw22_final_boss", ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(boss)));

            BigNoticeMap(idx, "<Bloody cathedral> The final boss has appeared!");
            return 0;
        }

        long OnOut(int32_t idx)
        {
            m_evOut.erase(idx);
            NoticeMap(idx, "<Bloody cathedral> You are getting teleported out of the dungeon.");
            WarpAllOut(idx);
            return 0;
        }
    };

    CHalloween2022DungeonImpl s_hw22;

    EVENTFUNC(halloween2022_timeout_event)
    {
        auto* info = dynamic_cast<halloween2022_event_info*>(event->info);
        if (!info)
            return 0;
        return s_hw22.OnTimeout(info->mapIndex);
    }

    EVENTFUNC(halloween2022_final_boss_event)
    {
        auto* info = dynamic_cast<halloween2022_event_info*>(event->info);
        if (!info)
            return 0;
        return s_hw22.OnFinalBossSpawn(info->mapIndex);
    }

    EVENTFUNC(halloween2022_out_event)
    {
        auto* info = dynamic_cast<halloween2022_event_info*>(event->info);
        if (!info)
            return 0;
        return s_hw22.OnOut(info->mapIndex);
    }
}

CHalloween2022Dungeon& CHalloween2022Dungeon::instance()
{
    static CHalloween2022Dungeon s;
    return s;
}

bool CHalloween2022Dungeon::IsHalloweenDungeonMap(int32_t mapIndex) const
{
    return IsInRange(mapIndex, kPrivateMin, kPrivateMax);
}

void CHalloween2022Dungeon::OnPlayerDisconnect(CHARACTER* ch)
{
    if (!ch || !ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(ch)))
        return;

    const int32_t idx = ch->GetMapIndex();
    if (!IsHalloweenDungeonMap(idx))
        return;

    LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
    if (!d || d->GetFlag(kFlagCompleted) != 0)
        return;

    SetRejoinFlags(ch, idx);
}

void CHalloween2022Dungeon::OnPlayerLogin(CHARACTER* ch)
{
    if (!ch || !ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(ch)))
        return;

    const int32_t idx = ch->GetMapIndex();

    if (IsHalloweenDungeonMap(idx))
    {
        LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
        if (!d)
        {
            WarpOut(ch);
            return;
        }

        SetOutsideWarpLocation(ch);
        ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfIdx, idx);
        ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfCh, (int32_t)g_bChannel);
        return;
    }

    //if (idx == kOriginalMap)
    //    WarpOut(ch);
}

bool CHalloween2022Dungeon::OnClickNpc(CHARACTER* ch, CHARACTER* npc)
{
    if (!ch || !ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(ch)) || !npc)
        return false;

    const uint32_t race = npc->GetRaceNum();
    const int32_t now = get_global_time();
    const int32_t currentIdx = ch->GetMapIndex();

    bool fromCompletedInside = false;
    int32_t originMapForWarp = currentIdx;

    if (IsHalloweenDungeonMap(currentIdx))
    {
        LPDUNGEON cur = CDungeonManager::instance().FindByMapIndex(currentIdx);
        if (cur && cur->GetFlag(kFlagCompleted) != 0)
        {
            fromCompletedInside = true;
            originMapForWarp = currentIdx;
        }
        else if (race == kEntryNpcVnum)
        {
            ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "You are already inside the dungeon.");
            return true;
        }
    }

    if (race == kRewardChestVnum)
    {
        const int32_t idx = ch->GetMapIndex();
        if (!IsHalloweenDungeonMap(idx))
            return false;

        LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
        if (!d || d->GetFlag(kFlagCompleted) == 0)
            return false;

        char rewardFlag[64];
        snprintf(rewardFlag, sizeof(rewardFlag), "hw22_reward_%u", ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)));
        if (d->GetFlag(rewardFlag) != 0)
        {
            ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "You already took your reward.");
            return true;
        }

        d->SetFlag(rewardFlag, 1);
        return true;
    }

    if (race != kEntryNpcVnum)
        return false;

    if (!fromCompletedInside)
    {
        const int32_t disconnectUntil = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), kQfDisconnect);
        const int32_t rejoinIdx = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), kQfIdx);
        const int32_t rejoinCh = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), kQfCh);

        if (disconnectUntil > now && rejoinIdx > 0 && rejoinCh == (int32_t)g_bChannel && IsHalloweenDungeonMap(rejoinIdx))
        {
            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(rejoinIdx);
            if (d && d->GetFlag(kFlagCompleted) == 0)
            {
                const int32_t floor = std::max(1, d->GetFlag(kFlagFloor));
                if (floor == 1)
                    ch->WarpSet(kEnterGlobalX * 100, kEnterGlobalY * 100, rejoinIdx);
                else
                    ch->WarpSet(kRejoinFloor2GlobalX * 100, kRejoinFloor2GlobalY * 100, rejoinIdx);
                ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfDisconnect, 0);
                return true;
            }
        }
    }

    if (!fromCompletedInside && !IsEntryMapForEmpire(ch))
    {
        ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "You must be in the correct map to enter Bloody cathedral.");
        return true;
    }

    if (!ch->CanWarp())
    {
        ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "You have to wait a bit before entering.");
        return true;
    }

    if (quest::CQuestManager::instance().GetEventFlag("Halloween2022Dungeon_block") == 1 && !ch->IsGM())
    {
        ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "The Bloody cathedral is currently blocked.");
        return true;
    }

    char antiSpamFlag[64];
    snprintf(antiSpamFlag, sizeof(antiSpamFlag), "halloween2022_%d", (int)g_bChannel);
    const int32_t antiSpamUntil = quest::CQuestManager::instance().GetEventFlag(antiSpamFlag);
    if (antiSpamUntil > now)
    {
        ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Please wait a moment.");
        return true;
    }
    quest::CQuestManager::instance().SetEventFlag(antiSpamFlag, now + kAntiSpamSec);

    LPPARTY party = ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch));
    if (party && party->GetLeaderPID() != ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)))
    {
        ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Only the party leader can enter.");
        return true;
    }

    bool ok = true;
    const char* badName = nullptr;
    enum EBad { BAD_NONE, BAD_LEVEL, BAD_WARP, BAD_ITEM, BAD_COOLDOWN } bad = BAD_NONE;
    int32_t badVal = 0;

    auto checkMember = [&](LPCHARACTER m)
    {
        if (!ok || !m || !ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(m)))
            return;

        const int32_t lv = m->GetLevel();
        if (lv < kMinLevel || lv > kMaxLevel)
        {
            ok = false;
            bad = BAD_LEVEL;
            badName = m->GetName();
            badVal = lv;
            return;
        }

        if (!m->CanWarp())
        {
            ok = false;
            bad = BAD_WARP;
            badName = m->GetName();
            return;
        }

        const int32_t rem = CooldownRemain(m);
        if (rem > 0)
        {
            ok = false;
            bad = BAD_COOLDOWN;
            badName = m->GetName();
            badVal = rem;
            return;
        }

        if (m->CountSpecifyItem(kEntryItemVnum) < kEntryItemCount)
        {
            ok = false;
            bad = BAD_ITEM;
            badName = m->GetName();
            return;
        }
    };

    if (!party)
        checkMember(ch);
    else
        party->ForEachOnMapMember(checkMember, originMapForWarp);

    if (!ok)
    {
        char tmp[64];
        switch (bad)
        {
            case BAD_LEVEL:
                ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "%s has invalid level (Lv%d). Required: %d-%d.", badName ? badName : "A member", badVal, kMinLevel, kMaxLevel);
                break;
            case BAD_WARP:
                ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "%s cannot warp yet.", badName ? badName : "A member");
                break;
            case BAD_ITEM:
                ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "%s does not have the required entry item.", badName ? badName : "A party member");
                break;
            case BAD_COOLDOWN:
                FormatDuration(badVal, tmp, sizeof(tmp));
                ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "%s is still on cooldown (%s).", badName ? badName : "A member", tmp);
                break;
            default:
                break;
        }
        return true;
    }

    LPDUNGEON d = CDungeonManager::instance().Create(kOriginalMap);
    if (!d)
    {
        ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Failed to create dungeon instance.");
        return true;
    }

    const int32_t dungeonMapIdx = d->GetMapIndex();

    auto prepareMember = [&](LPCHARACTER m)
    {
        if (!m || !ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(m)))
            return;

        if (!fromCompletedInside)
            SetOutsideWarpLocation(m);

        ClearRejoinFlags(m);
        ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(m), kQfIdx, dungeonMapIdx);
        ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(m), kQfCh, (int32_t)g_bChannel);
        SetCooldown(m);
        m->RemoveSpecifyItem(kEntryItemVnum, kEntryItemCount);
    };

    if (!party)
        prepareMember(ch);
    else
        party->ForEachOnMapMember(prepareMember, originMapForWarp);

    SetDungeonReady(d);
    s_hw22.ScheduleTimeout(dungeonMapIdx);

    if (party)
        d->JoinParty_Coords(party, kEnterGlobalX, kEnterGlobalY, originMapForWarp);
    else
        d->Join_Coords(ch, kEnterGlobalX, kEnterGlobalY, kOriginalMap);

    BigNoticeMap(dungeonMapIdx, "<Bloody cathedral> You have 30 minutes to complete the dungeon.");
    return true;
}

void CHalloween2022Dungeon::OnMobKilled(CHARACTER* killer, CHARACTER* victim)
{
    if (!killer || !victim || !ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(killer)))
        return;

    const int32_t idx = killer->GetMapIndex();
    if (!IsHalloweenDungeonMap(idx))
        return;

    LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
    if (!d)
        return;

    const uint32_t vnum = victim->GetRaceNum();
    const int32_t floor = d->GetFlag(kFlagFloor);

    // Floor 1 - full stones
    if (floor == 1 && d->GetFlag(kFlagCanDestroyFirstStone) == 1 && vnum == kStoneFullVnum)
    {
        const int32_t damaged = d->GetFlag(kFlagFirstStoneDamaged) + 1;
        d->SetFlag(kFlagFirstStoneDamaged, damaged);
        NoticeMap(idx, "<Bloody cathedral> This stone is under a spell now!");

        for (int i = 0; i < 5; ++i)
        {
            char key[32];
            snprintf(key, sizeof(key), "hw22_stone_%d", i + 1);
	if (ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(victim)) == (uint32_t)d->GetUniqueVid(key))
            {
                LPCHARACTER stoneNpc = d->SpawnMob(kStoneNpc, kStonePos[i].x, kStonePos[i].y, kStonePos[i].dir);
                if (stoneNpc)
                {
                    char u[32];
                    snprintf(u, sizeof(u), "hw22_spellstone_%d", i + 1);
		d->SetUnique(u, ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(stoneNpc)));
                }
                break;
            }
        }

        if (damaged >= 5)
        {
            d->SetFlag(kFlagCanDestroyFirstStone, 0);
            d->SetFlag(kFlagKillFirstBoss, 1);
            SpawnFirstBoss(idx);
        }
        return;
    }

    // Floor 1 - first boss
    if (floor == 1 && d->GetFlag(kFlagKillFirstBoss) == 1 && vnum == kFirstBossVnum)
    {
        d->SetFlag(kFlagKillFirstBoss, 0);
        const int32_t bossCount = d->GetFlag(kFlagFirstBossCount) + 1;
        d->SetFlag(kFlagFirstBossCount, bossCount);

        if (bossCount <= 2)
        {
            d->SetFlag(kFlagCanDestroyStatue, 1);
            DropItemOnGround(victim, killer, kStatueItemVnum, 1);
            NoticeMap(idx, "<Bloody cathedral> Use the dropped item on an Angel Statue.");
        }
        else
        {
            d->SetFlag(kFlagCanActivateSeal, 1);
            DropItemOnGround(victim, killer, kActivateItemVnum, 1);
            NoticeMap(idx, "<Bloody cathedral> Use the dropped item on the next seal.");
        }
        return;
    }

    // Floor 1 - half stones
    if (floor == 1 && d->GetFlag(kFlagCanDestroySecondStone) == 1 && vnum == kStoneHalfVnum)
    {
        const int32_t destroyed = d->GetFlag(kFlagFirstStoneDestroyed) + 1;
        d->SetFlag(kFlagFirstStoneDestroyed, destroyed);
        if (destroyed >= 5)
        {
            d->SetFlag(kFlagCanDestroySecondStone, 0);
            d->SetFlag(kFlagKillFirstBoss, 1);
            SpawnFirstBoss(idx);
        }
        return;
    }

    // Floor 1 - monster waves
    if (floor == 1 && d->GetFlag(kFlagFloor1Monsters) == 1 && !ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(victim)))
    {
        const int32_t killed = d->GetFlag(kFlagFloor1Killed) + 1;
        d->SetFlag(kFlagFloor1Killed, killed);

        const int32_t waveNum = d->GetFlag(kFlagFloor1WavesKilled) + 1;
        const int32_t need = (waveNum == 1) ? kFloor1Wave1Kills : kFloor1Wave2Kills;
        if (killed >= need)
        {
            d->SetFlag(kFlagFloor1Killed, 0);
            d->SetFlag(kFlagFloor1Monsters, 0);
            d->SetFlag(kFlagFloor1WavesKilled, d->GetFlag(kFlagFloor1WavesKilled) + 1);
            d->ClearRegen();
            d->KillAllMonsters();

            NoticeMap(idx, "<Bloody cathedral> Wave %d vanquished.", waveNum);
            if (d->GetFlag(kFlagFloor1WavesKilled) == 1)
            {
                d->SetFlag(kFlagFloor1Monsters, 1);
                d->SpawnRegen(kFloor1Monsters2Regen);
            }
            else
            {
                d->SetFlag(kFlagCanActivateSeal, 1);
                DropItemOnGround(victim, killer, kActivateItemVnum, 1);
                NoticeMap(idx, "<Bloody cathedral> Use the dropped item on the next seal.");
            }
        }
        return;
    }

    // Floor 2 - second boss
    if (floor == 2 && d->GetFlag(kFlagCanKillSecondBoss) == 1 && vnum == kSecondBossVnum)
    {
        d->SetFlag(kFlagCanKillSecondBoss, 0);
        d->SetFlag(kFlagCanFillCalyx, 1);
        DropItemOnGround(victim, killer, kSecondFloorItem, 1);
        NoticeMap(idx, "<Bloody cathedral> You got the required item. Fill a calyx now.");
        return;
    }

    // Floor 2 - stones
    if (floor == 2 && vnum == kSecondStoneVnum)
    {
        const int32_t stoneMode = d->GetFlag(kFlagCanDestroySecondFloorStone);
        if (stoneMode == 1)
        {
            d->SetFlag(kFlagCanDestroySecondFloorStone, 0);
            d->SetFlag(kFlagCanFillCalyx, 1);
            DropItemOnGround(victim, killer, kSecondFloorItem, 1);
            NoticeMap(idx, "<Bloody cathedral> You may fill another calyx now.");
            return;
        }
        else if (stoneMode == 2)
        {
            const int32_t cnt = d->GetFlag(kFlagSecondFloorStoneCount) + 1;
            d->SetFlag(kFlagSecondFloorStoneCount, cnt);
            if (cnt >= kSecondStoneCountNeeded)
            {
                d->SetFlag(kFlagCanDestroySecondFloorStone, 0);
                d->SetFlag(kFlagCanFillCalyx, 1);
                d->ClearRegen();
                DropItemOnGround(victim, killer, kSecondFloorItem, 1);
                NoticeMap(idx, "<Bloody cathedral> You destroyed all required stones. Fill another calyx.");
            }
            return;
        }
    }

    // Floor 2 - monster room
    if (floor == 2 && d->GetFlag(kFlagSecondFloorMonsters) == 1 && !ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(victim)))
    {
        const int32_t killed = d->GetFlag(kFlagSecondFloorKilled) + 1;
        d->SetFlag(kFlagSecondFloorKilled, killed);
        if (killed >= kFloor2KillsNeeded)
        {
            d->SetFlag(kFlagSecondFloorKilled, 0);
            d->SetFlag(kFlagSecondFloorMonsters, 0);
            d->SetFlag(kFlagCanFillCalyx, 1);
            d->ClearRegen();
            d->KillAllMonsters();
            DropItemOnGround(victim, killer, kSecondFloorItem, 1);
            NoticeMap(idx, "<Bloody cathedral> You killed all monsters. Fill another calyx.");
        }
        return;
    }

    // Floor 2 - final boss
    if (floor == 2 && d->GetFlag(kFlagFinalBossActive) == 1 && vnum == kFinalBossVnum)
    {
        d->SetFlag(kFlagFinalBossActive, 0);
        d->SetFlag(kFlagCompleted, 1);
        d->ClearRegen();
        d->KillAllMonsters();

        s_hw22.Cancel(s_hw22.m_evTimeout, idx);

        ForEachPcOnMap(idx, [&](LPCHARACTER pc)
        {
            SetCooldown(pc);
            ClearRejoinFlags(pc);
        });

        d->SpawnMob(kEntryNpcVnum, kRewardChestPos.x, kRewardChestPos.y, kRewardChestPos.dir);
       // NoticeMap(idx, "<Bloody cathedral> You can now take your reward from the chest.");

        char tmp[64];
        FormatDuration(kOutRoomSec, tmp, sizeof(tmp));
        NoticeMap(idx, "<Bloody cathedral> You will be teleported out in %s.", tmp);
        s_hw22.ScheduleOut(idx, kOutRoomSec);
        return;
    }
}

bool CHalloween2022Dungeon::OnNpcTakeItem(CHARACTER* from, CHARACTER* npc, CItem* item)
{
    if (!from || !ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(from)) || !npc || !item)
        return false;

    const int32_t idx = from->GetMapIndex();
    if (!IsHalloweenDungeonMap(idx))
        return false;

    LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
    if (!d)
        return false;

    const int32_t floor = d->GetFlag(kFlagFloor);
    const uint32_t npcVnum = npc->GetRaceNum();
    const uint32_t itemVnum = ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, item));

    // Angel statue
    if (floor == 1 && npcVnum == kAngelStatueNpc && d->GetFlag(kFlagCanDestroyStatue) == 1 && itemVnum == kStatueItemVnum)
    {
        RemoveOneGivenItem(EntityFactory::CreateItemEntity(g_registry, item), "HALLOWEEN22_STATUE");
        d->SetFlag(kFlagCanDestroyStatue, 0);
        d->SetFlag(kFlagAngelStatueCount, d->GetFlag(kFlagAngelStatueCount) + 1);
        M2_DESTROY_CHARACTER(npc);

        if (d->GetFlag(kFlagAngelStatueCount) == 1)
        {
            d->SetFlag(kFlagCanDestroySecondStone, 1);
            for (int i = 0; i < 5; ++i)
            {
                char key[32];
                snprintf(key, sizeof(key), "hw22_spellstone_%d", i + 1);
                d->KillUnique(key);
                d->SpawnMob(kStoneHalfVnum, kStonePos[i].x, kStonePos[i].y, kStonePos[i].dir);
            }
            NoticeMap(idx, "<Bloody cathedral> You broke the spell! Destroy the stones!");
        }
        else
        {
            d->SetFlag(kFlagCanActivateSeal, 1);
            from->AutoGiveItem(kActivateItemVnum, 1);
            NoticeMap(idx, "<Bloody cathedral> Use the item on the first seal.");
        }
        return true;
    }

    // Small seal -> starts wave 1
    if (floor == 1 && npcVnum == kSealSmallNpc && d->GetFlag(kFlagCanActivateSeal) == 1 && itemVnum == kActivateItemVnum)
    {
        RemoveOneGivenItem(EntityFactory::CreateItemEntity(g_registry, item), "HALLOWEEN22_SEAL_SMALL");
        d->SetFlag(kFlagCanActivateSeal, 0);
        d->SetFlag(kFlagSealState, d->GetFlag(kFlagSealState) + 1);
        d->SetFlag(kFlagFloor1Monsters, 1);
        d->SpawnMob(kSealMiddleNpc, kSealPos.x, kSealPos.y, kSealPos.dir);
        M2_DESTROY_CHARACTER(npc);
        d->SpawnRegen(kFloor1Monsters1Regen);
        NoticeMap(idx, "<Bloody cathedral> The first wave has begun.");
        return true;
    }

    // Middle seal -> spawn boss again
    if (floor == 1 && npcVnum == kSealMiddleNpc && d->GetFlag(kFlagCanActivateSeal) == 1 && itemVnum == kActivateItemVnum)
    {
        RemoveOneGivenItem(EntityFactory::CreateItemEntity(g_registry, item), "HALLOWEEN22_SEAL_MIDDLE");
        d->SetFlag(kFlagCanActivateSeal, 0);
        d->SetFlag(kFlagSealState, d->GetFlag(kFlagSealState) + 1);
        d->SetFlag(kFlagKillFirstBoss, 1);
        d->SpawnMob(kSealFullNpc, kSealPos.x, kSealPos.y, kSealPos.dir);
        M2_DESTROY_CHARACTER(npc);
        d->SpawnMob(kFirstBossVnum, kFirstBossPos.x, kFirstBossPos.y);
        return true;
    }

    // Full seal -> transition to floor 2
    if (floor == 1 && npcVnum == kSealFullNpc && d->GetFlag(kFlagCanActivateSeal) == 1 && itemVnum == kActivateItemVnum)
    {
        RemoveOneGivenItem(EntityFactory::CreateItemEntity(g_registry, item), "HALLOWEEN22_SEAL_FULL");
        d->SetFlag(kFlagCanActivateSeal, 0);
        d->SetFlag(kFlagFloor, 2);
        d->KillAll();
        d->ClearRegen();

        for (int i = 0; i < 4; ++i)
        {
            LPCHARACTER calyx = d->SpawnMob(kCalyxEmptyNpc, kCalyxPos[i].x, kCalyxPos[i].y, kCalyxPos[i].dir);
            if (calyx)
            {
                char key[32];
                snprintf(key, sizeof(key), "hw22_calyx_%d", i + 1);
	d->SetUnique(key, ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(calyx)));
            }
        }

        d->SpawnMob(kSecondBossVnum, kSecondBossPos.x, kSecondBossPos.y, kSecondBossPos.dir);
        d->SetFlag(kFlagCanKillSecondBoss, 1);

        NoticeMap(idx, "<Bloody cathedral> Fill all calyxes with blood to summon the final enemy.");
        //NoticeMap(idx, "<Bloody cathedral> Note: the original Lua quest text mentions an order, but the provided script does not actually enforce any order.");
        return true;
    }

    // Empty calyx -> floor 2 progression
    if (floor == 2 && npcVnum == kCalyxEmptyNpc && d->GetFlag(kFlagCanFillCalyx) == 1 && itemVnum == kSecondFloorItem)
    {
        RemoveOneGivenItem(EntityFactory::CreateItemEntity(g_registry, item), "HALLOWEEN22_CALYX");
        d->SetFlag(kFlagCanFillCalyx, 0);
        d->SetFlag(kFlagCalyxFilled, d->GetFlag(kFlagCalyxFilled) + 1);
        ReplaceUniqueCalyx(d, npc);

        const int32_t filled = d->GetFlag(kFlagCalyxFilled);
        if (filled == 1)
        {
            d->SetFlag(kFlagCanDestroySecondFloorStone, 1);
            d->SetFlag(kFlagSecondFloorStoneCount, 0);
            d->SpawnMob(kSecondStoneVnum, kSecondStonePos.x, kSecondStonePos.y, kSecondStonePos.dir);
            NoticeMap(idx, "<Bloody cathedral> Correct. Destroy the spawned stone now.");
        }
        else if (filled == 2)
        {
            d->SetFlag(kFlagSecondFloorMonsters, 1);
            d->SetFlag(kFlagSecondFloorKilled, 0);
            d->SpawnRegen(kFloor2MonstersRegen);
            NoticeMap(idx, "<Bloody cathedral> Kill all monsters to proceed.");
        }
        else if (filled == 3)
        {
            d->SetFlag(kFlagCanDestroySecondFloorStone, 2);
            d->SetFlag(kFlagSecondFloorStoneCount, 0);
            d->ClearRegen();
            d->SpawnRegen(kFloor2StonesRegen);
            NoticeMap(idx, "<Bloody cathedral> Destroy all required stones to proceed.");
        }
        else if (filled >= 4)
        {
            d->KillAllMonsters();
            d->ClearRegen();
            NoticeMap(idx, "<Bloody cathedral> You filled all calyxes. The final boss is coming!");
            s_hw22.ScheduleFinalBoss(idx, kFinalBossSpawnDelaySec);
        }
        return true;
    }

    return false;
}

