#include "stdafx.h"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/CombatSystem.hpp"
#include "ecs/systems/SocialSystem.hpp"
#include "ecs/systems/QuestSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "ecs/systems/PointSystem.hpp"
#include "ecs/systems/MovementSystem.hpp"
#include "VikingDungeon.h"

#include <unordered_map>
#include <string>
#include <cstdio>
#include <algorithm>

#include "char_interface.hpp"
#include "char_manager.h"
#include "config.h"
#include "party.h"
#include "sectree_manager.h"
#include "dungeon.h"
#include "item.h"
#include "questmanager.h"
#include "event.h"
#include "ecs/EntityFactory.hpp"
#include "ecs/Registry.hpp"
#include "ecs/systems/ItemSystem.hpp"
#include "ecs/CharacterAccessors.hpp"

namespace
{
    constexpr int32_t kOriginalMap = 179;
    constexpr int32_t kPrivateMin  = 1790000;
    constexpr int32_t kPrivateMax  = 1800000;

    constexpr uint32_t kEntryNpcVnum    = 9615;
    constexpr uint32_t kRewardChestVnum = 9626;
    constexpr uint32_t kResetItemVnum   = 33018;

    constexpr int32_t kBaseCellX = 19712;
    constexpr int32_t kBaseCellY = 25088;

    constexpr int32_t kEnterLocalX = 320;
    constexpr int32_t kEnterLocalY = 395;
    constexpr int32_t kEnterGlobalX = kBaseCellX + kEnterLocalX;
    constexpr int32_t kEnterGlobalY = kBaseCellY + kEnterLocalY;

    constexpr int32_t kMinLevel = 140;
    constexpr int32_t kMaxLevel = 160;
    constexpr int32_t kMinMembers = 2;

    constexpr uint32_t kEntryItemVnum  = 33014;
    constexpr int32_t  kEntryItemCount = 1;

    constexpr uint32_t kGateNpc           = 9616;
    constexpr uint32_t kCompassEmptyNpc   = 9617;
    constexpr uint32_t kCompassSmallNpc   = 9618;
    constexpr uint32_t kCompassMediumNpc  = 9619;
    constexpr uint32_t kCompassLargeNpc   = 9620;
    constexpr uint32_t kMemorialNpc1      = 9621;
    constexpr uint32_t kMemorialNpc2      = 9622;
    constexpr uint32_t kMemorialNpc3      = 9623;
    constexpr uint32_t kMemorialNpc4      = 9624;
    constexpr uint32_t kStoneProtectorNpc = 9625;

    constexpr uint32_t kFloor1ItemVnum = 33015;
    constexpr uint32_t kFloor3ItemVnum = 33016;
    constexpr uint32_t kRewardItemVnum = 33017;
    constexpr int32_t  kRewardItemCount = 1;

    constexpr uint32_t kFloor1LowBossVnum  = 4812;
    constexpr uint32_t kFloor1MainBossVnum = 4813;
    constexpr uint32_t kFloor2StoneVnum    = 8821;
    constexpr uint32_t kFloor3StoneVnum    = 8822;
    constexpr uint32_t kFloor3BossVnum     = 4814;
    constexpr uint32_t kFinalBossVnum      = 4815;

    constexpr int32_t kStartingDelaySec      = 10;
    constexpr int32_t kPhaseCheckIntervalSec = 4;
    constexpr int32_t kNextFloorDelaySec     = 10;
    constexpr int32_t kFloor2TimerSec        = 4 * 60;
    constexpr int32_t kTimeOutSec            = 20 * 60;
    constexpr int32_t kTimeOutNoticeStepSec  = 2 * 60;
    constexpr int32_t kOutRoomSec            = 2 * 60;
    constexpr int32_t kEntranceCooldownSec   = 60 * 60;
    constexpr int32_t kRejoinSec             = 5 * 60;
    constexpr int32_t kAntiSpamSec           = 1;

    constexpr int64_t kFinalBossNormalHP  = 10000000000;
    constexpr int64_t kFinalBossPenaltyHP = 15000000000;

    constexpr const char* kRegen1FloorA = "data/dungeon/viking_dungeon/regen_1f_a.txt";
    constexpr const char* kRegen1FloorB = "data/dungeon/viking_dungeon/regen_1f_b.txt";
    constexpr const char* kRegen2FloorA = "data/dungeon/viking_dungeon/regen_2f_a.txt";

    constexpr const char* kQfCooldown   = "vikingdungeon_zone.cooldown";
    constexpr const char* kQfDisconnect = "vikingdungeon_zone.disconnect";
    constexpr const char* kQfIdx        = "vikingdungeon_zone.idx";
    constexpr const char* kQfCh         = "vikingdungeon_zone.ch";

    constexpr const char* kFlagInitialized       = "vk_init";
    constexpr const char* kFlagBlockRejoin       = "vk_block_rejoin";
    constexpr const char* kFlagCompleted         = "vk_completed";
    constexpr const char* kFlagFloor             = "vk_floor";
    constexpr const char* kFlagStartTime         = "vk_start_time";
    constexpr const char* kFlagTimeLimit         = "vk_time_limit";
    constexpr const char* kFlagCompassState      = "vk_compass_state";
    constexpr const char* kFlagMainBossStage     = "vk_main_boss_stage";
    constexpr const char* kFlagFloor2Remain      = "vk_floor2_remain";
    constexpr const char* kFlagFinalPenalty      = "vk_final_penalty";
    constexpr const char* kFlagFloor3NpcStage    = "vk_floor3_npc_stage";
    constexpr const char* kFlagFloor3NpcVnum     = "vk_floor3_npc_vnum";
    constexpr const char* kFlagCanKillFloor3Boss = "vk_can_kill_f3_boss";
    constexpr const char* kFlagCanUseRune        = "vk_can_use_rune";
    constexpr const char* kFlagFinalBossStage    = "vk_final_boss_stage";

    struct SPosDir
    {
        int32_t x;
        int32_t y;
        int32_t dir;
    };

    constexpr SPosDir kGatePos1          = {319, 447, 5};
    constexpr SPosDir kGatePos2          = {319, 526, 5};
    constexpr SPosDir kCompassPos        = {320, 432, 5};
    constexpr SPosDir kFloor1LowBossPos  = {320, 420, 1};
    constexpr SPosDir kFloor1MainBossPos = {320, 420, 5};
    constexpr SPosDir kMemorialPos       = {318, 577, 5};
    constexpr SPosDir kFinalBossPos      = {318, 554, 5};
    constexpr SPosDir kRewardChestPos    = {319, 566, 5};

    constexpr SPosDir kFloor2Stones[4] = {
        {319, 460, 0},
        {319, 475, 0},
        {319, 493, 0},
        {319, 511, 0},
    };

    constexpr SPosDir kFloor3Stones[3] = {
        {330, 554, 3},
        {318, 566, 1},
        {306, 554, 7},
    };

    constexpr int kMainBossHpStages[3]  = {70, 50, 15};
    constexpr int kFinalBossHpStages[3] = {60, 30, 10};

    inline const char* GetFloor3StoneKey(int idx)
    {
        switch (idx)
        {
            case 0: return "vk_f3_stone_1";
            case 1: return "vk_f3_stone_2";
            default: return "vk_f3_stone_3";
        }
    }

    inline const char* GetFloor3ProtectorKey(int idx)
    {
        switch (idx)
        {
            case 0: return "vk_f3_npc_1";
            case 1: return "vk_f3_npc_2";
            default: return "vk_f3_npc_3";
        }
    }

    inline std::string GetFloor3ClearedFlag(int idx)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "vk_f3_cleared_%d", idx + 1);
        return std::string(buf);
    }

    void ResetFloor3Progress(LPDUNGEON d)
    {
        if (!d)
            return;

        for (int i = 0; i < 3; ++i)
            d->SetFlag(GetFloor3ClearedFlag(i), 0);
    }

    inline bool IsFloor3SlotCleared(LPDUNGEON d, int idx)
    {
        return d && d->GetFlag(GetFloor3ClearedFlag(idx)) != 0;
    }

    inline void SetFloor3SlotCleared(LPDUNGEON d, int idx, bool value)
    {
        if (d)
            d->SetFlag(GetFloor3ClearedFlag(idx), value ? 1 : 0);
    }

    inline int FindFloor3StoneSlotByVid(LPDUNGEON d, uint32_t vid)
    {
        if (!d || vid == 0)
            return -1;

        for (int i = 0; i < 3; ++i)
        {
            if (d->GetUniqueVid(GetFloor3StoneKey(i)) == (int32_t)vid)
                return i;
        }

        return -1;
    }

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
                if (ch && ecs::PlayerRuntime::IsPC(ch->GetEntityHandle()))
                    m_f(ch->GetEntityHandle());
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

        ForEachPcOnMap(mapIndex, [&](entt::entity pc){
            ecs::ChatSystem::Send(pc, CHAT_TYPE_NOTICE, "%s", buf);
        });
    }

    void BigNoticeMap(int32_t mapIndex, const char* fmt, ...)
    {
        char buf[CHAT_MAX_LEN + 64];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);

        ForEachPcOnMap(mapIndex, [&](entt::entity pc){
            ecs::ChatSystem::Send(pc, CHAT_TYPE_BIG_NOTICE, "%s", buf);
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
            snprintf(out, outSz, "%dh %02dm %02ds", h, m, s);
        else if (m > 0)
            snprintf(out, outSz, "%dm %02ds", m, s);
        else
            snprintf(out, outSz, "%ds", s);
    }

    int32_t GetOutsideMapByEmpire(uint8_t /*empire*/)
    {
        return 219;
    }

    void GetOutsideWarpByEmpire(uint8_t /*empire*/, int32_t& mapIdx, int32_t& x, int32_t& y)
    {
        mapIdx = 219;

        // IDE a belépő NPC vagy a kívánt visszaérkezési pont koordinátája kell
        x = 5369;
        y = 14350;
    }//536904	1435017

    bool IsEntryMapForEmpire(LPCHARACTER ch)
    {
        return ch && ecs::PlayerRuntime::GetMapIndex(((ch) ? (ch)->GetEntityHandle() : entt::null)) == 219;
    }

    void SetOutsideWarpLocation(LPCHARACTER ch)
    {
        if (!ch)
            return;
        int32_t mapIdx = 1, x = 0, y = 0;
        GetOutsideWarpByEmpire(ecs::PlayerRuntime::GetEmpire(((ch) ? (ch)->GetEntityHandle() : entt::null)), mapIdx, x, y);
        ch->SetWarpLocation(mapIdx, x, y);
    }

    void WarpOut(LPCHARACTER ch)
    {
        if (!ch)
            return;
        int32_t mapIdx = 1, x = 0, y = 0;
        GetOutsideWarpByEmpire(ecs::PlayerRuntime::GetEmpire(((ch) ? (ch)->GetEntityHandle() : entt::null)), mapIdx, x, y);
        ecs::MovementSystem::WarpSet(((ch) ? (ch)->GetEntityHandle() : entt::null), x * 100, y * 100, mapIdx);
    }

    void WarpAllOut(int32_t mapIndex)
    {
        ForEachPcOnMap(mapIndex, [&](entt::entity ch){
            LPCHARACTER pkCh = ecs::LegacyCharOf(ch);
            WarpOut(pkCh);
        });
    }

    void SetCooldown(LPCHARACTER ch)
    {
        if (ch)
            ecs::QuestSystem::SetFlag(((ch) ? (ch)->GetEntityHandle() : entt::null), kQfCooldown, get_global_time() + kEntranceCooldownSec);
    }

    void SetRejoinFlags(LPCHARACTER ch, int32_t mapIndex)
    {
        if (!ch)
            return;
        ecs::QuestSystem::SetFlag(((ch) ? (ch)->GetEntityHandle() : entt::null), kQfIdx, mapIndex);
        ecs::QuestSystem::SetFlag(((ch) ? (ch)->GetEntityHandle() : entt::null), kQfCh, (int32_t)g_bChannel);
        ecs::QuestSystem::SetFlag(((ch) ? (ch)->GetEntityHandle() : entt::null), kQfDisconnect, get_global_time() + kRejoinSec);
    }

    void ClearRejoinFlags(LPCHARACTER ch)
    {
        if (!ch)
            return;
        ecs::QuestSystem::SetFlag(((ch) ? (ch)->GetEntityHandle() : entt::null), kQfDisconnect, 0);
    }

    void ClearDungeonNonPlayers(LPDUNGEON d)
    {
        if (!d)
            return;

        d->ClearRegen();
        d->KillAllMonsters();
    }

    void ApplyMapHpPctDamage(int32_t mapIndex, int pct)
    {
        if (pct <= 0)
            return;

        ForEachPcOnMap(mapIndex, [&](entt::entity ch){
            LPCHARACTER pkCh = ecs::LegacyCharOf(ch);
            if (!pkCh || pkCh->GetHP() <= 1)
                return;

            int64_t dmg = (pkCh->GetHP() * pct) / 100;
            if (dmg < 1)
                dmg = 1;
            if (dmg >= pkCh->GetHP())
                dmg = pkCh->GetHP() - 1;
            if (dmg > 0)
                ecs::PointSystem::Change(ch, POINT_HP, -dmg);
        });
    }

    LPCHARACTER FindUnique(LPDUNGEON d, const char* key)
    {
        if (!d)
            return nullptr;
        const int32_t vid = d->GetUniqueVid(key);
        if (vid <= 0)
            return nullptr;
        return CHARACTER_MANAGER::instance().Find((uint32_t)vid);
    }

    void SetDungeonReady(LPDUNGEON d)
    {
        if (!d)
            return;

        d->SetFlag(kFlagInitialized, 0);
        d->SetFlag(kFlagBlockRejoin, 0);
        d->SetFlag(kFlagCompleted, 0);
        d->SetFlag(kFlagFloor, 0);
        d->SetFlag(kFlagStartTime, 0);
        d->SetFlag(kFlagTimeLimit, 0);
        d->SetFlag(kFlagCompassState, 0);
        d->SetFlag(kFlagMainBossStage, 0);
        d->SetFlag(kFlagFloor2Remain, 0);
        d->SetFlag(kFlagFinalPenalty, 0);
        d->SetFlag(kFlagFloor3NpcStage, 0);
        d->SetFlag(kFlagFloor3NpcVnum, kMemorialNpc1);
        d->SetFlag(kFlagCanKillFloor3Boss, 0);
        d->SetFlag(kFlagCanUseRune, 0);
        d->SetFlag(kFlagFinalBossStage, 0);
        ResetFloor3Progress(d);
    }

    void SpawnFloor4Setup(LPDUNGEON d)
    {
        if (!d)
            return;

        d->SetFlag(kFlagFloor, 4);
        d->SetFlag(kFlagCanKillFloor3Boss, 0);
        d->SetFlag(kFlagCanUseRune, 0);
        d->SetFlag(kFlagFloor3NpcStage, 0);
        d->SetFlag(kFlagFloor3NpcVnum, kMemorialNpc1);
        ResetFloor3Progress(d);

        LPCHARACTER memorial = d->SpawnMob(kMemorialNpc1, kMemorialPos.x, kMemorialPos.y, kMemorialPos.dir);
        if (memorial)
		d->SetUnique("vk_memorial", ecs::PlayerRuntime::GetPacketVID(((memorial) ? (memorial)->GetEntityHandle() : entt::null)));

        for (int i = 0; i < 3; ++i)
        {
            LPCHARACTER stone = d->SpawnMob(kFloor3StoneVnum, kFloor3Stones[i].x, kFloor3Stones[i].y, kFloor3Stones[i].dir);
            if (stone)
				d->SetUnique(GetFloor3StoneKey(i), ecs::PlayerRuntime::GetPacketVID(((stone) ? (stone)->GetEntityHandle() : entt::null)));
        }
    }

    void RespawnFloor3StonesFromProtectors(LPDUNGEON d)
    {
        if (!d)
            return;

        for (int i = 0; i < 3; ++i)
        {
            if (d->GetUniqueVid(GetFloor3ProtectorKey(i)) > 0)
                d->KillUnique(GetFloor3ProtectorKey(i));

            if (d->GetUniqueVid(GetFloor3StoneKey(i)) > 0)
                d->KillUnique(GetFloor3StoneKey(i));

            if (IsFloor3SlotCleared(d, i))
                continue;

            LPCHARACTER stone = d->SpawnMob(kFloor3StoneVnum, kFloor3Stones[i].x, kFloor3Stones[i].y, kFloor3Stones[i].dir);
            if (stone)
				d->SetUnique(GetFloor3StoneKey(i), ecs::PlayerRuntime::GetPacketVID(((stone) ? (stone)->GetEntityHandle() : entt::null)));
        }
    }

    void SpawnFloor3ProtectorsForRemainingSlots(LPDUNGEON d)
    {
        if (!d)
            return;

        for (int i = 0; i < 3; ++i)
        {
            if (d->GetUniqueVid(GetFloor3StoneKey(i)) > 0)
                d->KillUnique(GetFloor3StoneKey(i));

            if (d->GetUniqueVid(GetFloor3ProtectorKey(i)) > 0)
                d->KillUnique(GetFloor3ProtectorKey(i));

            if (IsFloor3SlotCleared(d, i))
                continue;

            LPCHARACTER protector = d->SpawnMob(kStoneProtectorNpc, kFloor3Stones[i].x, kFloor3Stones[i].y, kFloor3Stones[i].dir);
            if (protector)
			d->SetUnique(GetFloor3ProtectorKey(i), ecs::PlayerRuntime::GetPacketVID(((protector) ? (protector)->GetEntityHandle() : entt::null)));
        }
    }


    void ReplaceCompass(LPDUNGEON d, LPCHARACTER npc, uint32_t newVnum)
    {
        if (!d || !npc)
            return;

        if (d->GetUniqueVid("vk_compass") > 0)
            d->KillUnique("vk_compass");

        LPCHARACTER spawned = d->SpawnMob(newVnum, kCompassPos.x, kCompassPos.y, kCompassPos.dir);
        if (spawned)
		d->SetUnique("vk_compass", ecs::PlayerRuntime::GetPacketVID(((spawned) ? (spawned)->GetEntityHandle() : entt::null)));

        npc->Dead(nullptr, true);
    }

    EVENTINFO(viking_event_info)
    {
        int32_t mapIndex;
        viking_event_info() : mapIndex(0) {}
    };

    EVENTFUNC(viking_start_event);
    EVENTFUNC(viking_timeout_event);
    EVENTFUNC(viking_floor1_check_event);
    EVENTFUNC(viking_floor1_mainboss_event);
    EVENTFUNC(viking_floor1_bosshp_event);
    EVENTFUNC(viking_floor2_timer_event);
    EVENTFUNC(viking_floor2_transition_event);
    EVENTFUNC(viking_floor4_transition_event);
    EVENTFUNC(viking_finalboss_spawn_event);
    EVENTFUNC(viking_finalboss_hp_event);
    EVENTFUNC(viking_complete_event);
    EVENTFUNC(viking_out_event);

    class CVikingDungeonImpl
    {
    public:
        std::unordered_map<int32_t, LPEVENT> evStart;
        std::unordered_map<int32_t, LPEVENT> evTimeout;
        std::unordered_map<int32_t, LPEVENT> evFloor1Check;
        std::unordered_map<int32_t, LPEVENT> evFloor1Boss;
        std::unordered_map<int32_t, LPEVENT> evFloor1BossHp;
        std::unordered_map<int32_t, LPEVENT> evFloor2Timer;
        std::unordered_map<int32_t, LPEVENT> evFloor2Transition;
        std::unordered_map<int32_t, LPEVENT> evFloor4Transition;
        std::unordered_map<int32_t, LPEVENT> evFinalSpawn;
        std::unordered_map<int32_t, LPEVENT> evFinalHp;
        std::unordered_map<int32_t, LPEVENT> evComplete;
        std::unordered_map<int32_t, LPEVENT> evOut;

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
            Cancel(evStart, idx);
            Cancel(evTimeout, idx);
            Cancel(evFloor1Check, idx);
            Cancel(evFloor1Boss, idx);
            Cancel(evFloor1BossHp, idx);
            Cancel(evFloor2Timer, idx);
            Cancel(evFloor2Transition, idx);
            Cancel(evFloor4Transition, idx);
            Cancel(evFinalSpawn, idx);
            Cancel(evFinalHp, idx);
            Cancel(evComplete, idx);
            Cancel(evOut, idx);
        }

        void Schedule(std::unordered_map<int32_t, LPEVENT>& map, TEVENTFUNC fn, int32_t idx, int32_t sec)
        {
            Cancel(map, idx);
            viking_event_info* info = AllocEventInfo<viking_event_info>();
            info->mapIndex = idx;
            map[idx] = event_create(fn, info, PASSES_PER_SEC(sec));
        }

        void SchedulePulse(std::unordered_map<int32_t, LPEVENT>& map, TEVENTFUNC fn, int32_t idx, int32_t pulses)
        {
            Cancel(map, idx);
            viking_event_info* info = AllocEventInfo<viking_event_info>();
            info->mapIndex = idx;
            map[idx] = event_create(fn, info, pulses > 0 ? pulses : 1);
        }

        void ScheduleStart(int32_t idx)            { Schedule(evStart, viking_start_event, idx, kStartingDelaySec); }
        void ScheduleTimeout(int32_t idx)          { Schedule(evTimeout, viking_timeout_event, idx, kTimeOutNoticeStepSec); }
        void ScheduleFloor1Check(int32_t idx)      { Schedule(evFloor1Check, viking_floor1_check_event, idx, kPhaseCheckIntervalSec); }
        void ScheduleFloor1Boss(int32_t idx)       { Schedule(evFloor1Boss, viking_floor1_mainboss_event, idx, kNextFloorDelaySec); }
        void ScheduleFloor1BossHp(int32_t idx)     { Schedule(evFloor1BossHp, viking_floor1_bosshp_event, idx, 1); }
        void ScheduleFloor2Timer(int32_t idx)      { Schedule(evFloor2Timer, viking_floor2_timer_event, idx, kFloor2TimerSec); }
        void ScheduleFloor2Transition(int32_t idx) { SchedulePulse(evFloor2Transition, viking_floor2_transition_event, idx, 1); }
        void ScheduleFloor4Transition(int32_t idx) { SchedulePulse(evFloor4Transition, viking_floor4_transition_event, idx, 1); }
        void ScheduleFinalSpawn(int32_t idx)       { Schedule(evFinalSpawn, viking_finalboss_spawn_event, idx, kNextFloorDelaySec); }
        void ScheduleFinalHp(int32_t idx)          { Schedule(evFinalHp, viking_finalboss_hp_event, idx, 1); }
        void ScheduleComplete(int32_t idx)         { SchedulePulse(evComplete, viking_complete_event, idx, 1); }
        void ScheduleOut(int32_t idx)              { Schedule(evOut, viking_out_event, idx, kOutRoomSec); }

        long OnStart(int32_t idx)
        {
            evStart.erase(idx);
            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
            if (!d || d->GetFlag(kFlagCompleted) != 0)
                return 0;

            d->SetFlag(kFlagFloor, 1);
            d->SetFlag(kFlagStartTime, get_global_time());
            d->SetFlag(kFlagTimeLimit, get_global_time() + kTimeOutSec);
            ScheduleTimeout(idx);
            ScheduleFloor1Check(idx);
            d->SpawnRegen(kRegen1FloorA, true);

            NoticeMap(idx, "<Frostbane Fortress> You have 20 minutes to complete the dungeon.");
            NoticeMap(idx, "<Frostbane Fortress> Eliminate all monsters to summon the low boss.");
            return 0;
        }

        long OnTimeout(int32_t idx)
        {
            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
            if (!d)
            {
                evTimeout.erase(idx);
                return 0;
            }

            if (d->GetFlag(kFlagCompleted) != 0)
            {
                evTimeout.erase(idx);
                return 0;
            }

            const int32_t now = get_global_time();
            const int32_t limit = d->GetFlag(kFlagTimeLimit);
            if (limit <= 0)
            {
                evTimeout.erase(idx);
                return 0;
            }

            if (now >= limit)
            {
                evTimeout.erase(idx);
                d->SetFlag(kFlagBlockRejoin, 1);
                NoticeMap(idx, "<Frostbane Fortress> Time expired.");
                NoticeMap(idx, "<Frostbane Fortress> You will be teleported out of the dungeon.");
                CancelAll(idx);
                WarpAllOut(idx);
                return 0;
            }

            char tmp[64];
            FormatDuration(limit - now, tmp, sizeof(tmp));
            NoticeMap(idx, "<Frostbane Fortress> Time remaining: %s.", tmp);
            return PASSES_PER_SEC(kTimeOutNoticeStepSec);
        }

        long OnFloor1Check(int32_t idx)
        {
            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
            if (!d)
            {
                evFloor1Check.erase(idx);
                return 0;
            }
            if (d->GetFlag(kFlagFloor) != 1)
            {
                evFloor1Check.erase(idx);
                return 0;
            }

            if (d->CountMonster() > 0)
                return PASSES_PER_SEC(kPhaseCheckIntervalSec);

            evFloor1Check.erase(idx);
            d->SpawnMob(kFloor1LowBossVnum, kFloor1LowBossPos.x, kFloor1LowBossPos.y, kFloor1LowBossPos.dir);
            NoticeMap(idx, "<Frostbane Fortress> The low boss has been summoned. Kill him to proceed.");
            return 0;
        }

        long OnFloor1MainBoss(int32_t idx)
        {
            evFloor1Boss.erase(idx);
            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
            if (!d)
                return 0;

            d->SetFlag(kFlagFloor, 2);
            d->SetFlag(kFlagMainBossStage, 0);
            LPCHARACTER boss = d->SpawnMob(kFloor1MainBossVnum, kFloor1MainBossPos.x, kFloor1MainBossPos.y, kFloor1MainBossPos.dir);
            if (boss)
	d->SetUnique("vk_main_boss", ecs::PlayerRuntime::GetPacketVID(((boss) ? (boss)->GetEntityHandle() : entt::null)));
            ScheduleFloor1BossHp(idx);
            BigNoticeMap(idx, "<Frostbane Fortress> The first main boss has appeared!");
            return 0;
        }

        long OnFloor1BossHp(int32_t idx)
        {
            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
            if (!d)
            {
                evFloor1BossHp.erase(idx);
                return 0;
            }
            if (d->GetFlag(kFlagFloor) != 2)
            {
                evFloor1BossHp.erase(idx);
                return 0;
            }

            LPCHARACTER boss = FindUnique(d, "vk_main_boss");
            if (!boss || CombatSystem::IsDead(((boss) ? (boss)->GetEntityHandle() : entt::null)))
            {
                evFloor1BossHp.erase(idx);
                return 0;
            }

            const int stage = d->GetFlag(kFlagMainBossStage);
            if (stage >= 3)
                return PASSES_PER_SEC(1);

            const int hpPct = boss->GetHPPct();
            if (hpPct > kMainBossHpStages[stage])
                return PASSES_PER_SEC(1);

            if (stage == 0)
            {
                d->SetFlag(kFlagMainBossStage, 1);
                d->SpawnRegen(kRegen1FloorB, true);
                NoticeMap(idx, "<Frostbane Fortress> The boss summoned many monsters. Be careful.");
            }
            else if (stage == 1)
            {
                d->SetFlag(kFlagMainBossStage, 2);
                ApplyMapHpPctDamage(idx, 35);
                NoticeMap(idx, "<Frostbane Fortress> The boss cast a dungeon-wide damage spell.");
            }
            else if (stage == 2)
            {
                d->SetFlag(kFlagMainBossStage, 3);
                boss->SetDamMul(0.5f);
                NoticeMap(idx, "<Frostbane Fortress> The boss reduced incoming damage by half.");
            }

            return PASSES_PER_SEC(1);
        }

        long OnFloor2Timer(int32_t idx)
        {
            evFloor2Timer.erase(idx);
            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
            if (!d)
                return 0;
            if (d->GetFlag(kFlagFloor) != 3)
                return 0;

            if (d->GetFlag(kFlagFloor2Remain) > 0)
                d->SetFlag(kFlagFinalPenalty, 1);

            NoticeMap(idx, "<Frostbane Fortress> You didn't destroy all stones in time.");
            NoticeMap(idx, "<Frostbane Fortress> Final boss will get double HP.");
            return 0;
        }

        long OnFloor2Transition(int32_t idx)
        {
            evFloor2Transition.erase(idx);
            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
            if (!d || d->GetFlag(kFlagCompleted) != 0)
                return 0;

            Cancel(evFloor1BossHp, idx);
            ClearDungeonNonPlayers(d);

            d->SetFlag(kFlagFloor, 3);
            d->SetFlag(kFlagFloor2Remain, 4);

            for (int i = 0; i < 4; ++i)
                d->SpawnMob(kFloor2StoneVnum, kFloor2Stones[i].x, kFloor2Stones[i].y, kFloor2Stones[i].dir);

            LPCHARACTER gate = d->SpawnMob(kGateNpc, kGatePos2.x, kGatePos2.y, kGatePos2.dir);
            if (gate)
		d->SetUnique("vk_gate_2", ecs::PlayerRuntime::GetPacketVID(((gate) ? (gate)->GetEntityHandle() : entt::null)));

            ScheduleFloor2Timer(idx);
            NoticeMap(idx, "<Frostbane Fortress> Destroy all second-floor stones within 4 minutes.");
            return 0;
        }

        long OnFloor4Transition(int32_t idx)
        {
            evFloor4Transition.erase(idx);
            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
            if (!d || d->GetFlag(kFlagCompleted) != 0)
                return 0;

            Cancel(evFloor2Timer, idx);
            ClearDungeonNonPlayers(d);
            SpawnFloor4Setup(d);
            NoticeMap(idx, "<Frostbane Fortress> All second-floor stones were destroyed.");
            NoticeMap(idx, "<Frostbane Fortress> Kill the protecting bosses, get the rune item, and use it on the memorial.");
            return 0;
        }

        long OnFinalSpawn(int32_t idx)
        {
            evFinalSpawn.erase(idx);
            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
            if (!d || d->GetFlag(kFlagCompleted) != 0)
                return 0;

            d->SetFlag(kFlagFloor, 5);
            d->SetFlag(kFlagFinalBossStage, 0);
            LPCHARACTER boss = d->SpawnMob(kFinalBossVnum, kFinalBossPos.x, kFinalBossPos.y, kFinalBossPos.dir);
            if (boss)
            {
                const int64_t hp = d->GetFlag(kFlagFinalPenalty) ? kFinalBossPenaltyHP : kFinalBossNormalHP;
                boss->SetMaxHP(hp);
                boss->SetHP(hp);
	d->SetUnique("vk_final_boss", ecs::PlayerRuntime::GetPacketVID(((boss) ? (boss)->GetEntityHandle() : entt::null)));
            }
            ScheduleFinalHp(idx);
            BigNoticeMap(idx, "<Frostbane Fortress> The final boss has appeared!");
            return 0;
        }

        long OnFinalHp(int32_t idx)
        {
            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
            if (!d)
            {
                evFinalHp.erase(idx);
                return 0;
            }
            if (d->GetFlag(kFlagFloor) != 5)
            {
                evFinalHp.erase(idx);
                return 0;
            }

            LPCHARACTER boss = FindUnique(d, "vk_final_boss");
            if (!boss || CombatSystem::IsDead(((boss) ? (boss)->GetEntityHandle() : entt::null)))
            {
                evFinalHp.erase(idx);
                return 0;
            }

            const int stage = d->GetFlag(kFlagFinalBossStage);
            if (stage >= 3)
                return PASSES_PER_SEC(1);

            const int hpPct = boss->GetHPPct();
            if (hpPct > kFinalBossHpStages[stage])
                return PASSES_PER_SEC(1);

            if (stage == 0)
            {
                d->SetFlag(kFlagFinalBossStage, 1);
                d->SpawnRegen(kRegen2FloorA, true);
                NoticeMap(idx, "<Frostbane Fortress> The final boss summoned many monsters. Be careful.");
            }
            else if (stage == 1)
            {
                d->SetFlag(kFlagFinalBossStage, 2);
                ApplyMapHpPctDamage(idx, 40);
                NoticeMap(idx, "<Frostbane Fortress> The final boss cast a dungeon-wide damage spell.");
            }
            else if (stage == 2)
            {
                d->SetFlag(kFlagFinalBossStage, 3);
                boss->SetDamMul(0.5f);
                NoticeMap(idx, "<Frostbane Fortress> The final boss reduced incoming damage by half.");
            }

            return PASSES_PER_SEC(1);
        }

        long OnComplete(int32_t idx)
        {
            evComplete.erase(idx);
            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
            if (!d)
                return 0;

            CancelAll(idx);

            ForEachPcOnMap(idx, [&](entt::entity member){
                LPCHARACTER pkMember = ecs::LegacyCharOf(member);
                SetCooldown(pkMember);
            });

            ClearDungeonNonPlayers(d);
            //LPCHARACTER entryNpc = d->SpawnMob(kEntryNpcVnum, kRewardChestPos.x, kRewardChestPos.y, kRewardChestPos.dir);
            //if (entryNpc)
            //    d->SetUnique("vk_entry_npc", entryNpc->GetVID());

            BigNoticeMap(idx, "<Frostbane Fortress> Dungeon completed!");
            NoticeMap(idx, "<Frostbane Fortress> Click the entry NPC if you want to restart immediately.");
            NoticeMap(idx, "<Frostbane Fortress> You will be teleported out in 2 minutes.");
            ScheduleOut(idx);
            return 0;
        }

        long OnOut(int32_t idx)
        {
            evOut.erase(idx);
            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
            if (d)
                d->SetFlag(kFlagBlockRejoin, 1);
            NoticeMap(idx, "<Frostbane Fortress> You are getting teleported out of the dungeon.");
            WarpAllOut(idx);
            return 0;
        }
    };

    CVikingDungeonImpl s_viking;

    EVENTFUNC(viking_start_event)
    {
        auto* info = dynamic_cast<viking_event_info*>(event->info);
        if (!info)
            return 0;
        return s_viking.OnStart(info->mapIndex);
    }

    EVENTFUNC(viking_timeout_event)
    {
        auto* info = dynamic_cast<viking_event_info*>(event->info);
        if (!info)
            return 0;
        return s_viking.OnTimeout(info->mapIndex);
    }

    EVENTFUNC(viking_floor1_check_event)
    {
        auto* info = dynamic_cast<viking_event_info*>(event->info);
        if (!info)
            return 0;
        return s_viking.OnFloor1Check(info->mapIndex);
    }

    EVENTFUNC(viking_floor1_mainboss_event)
    {
        auto* info = dynamic_cast<viking_event_info*>(event->info);
        if (!info)
            return 0;
        return s_viking.OnFloor1MainBoss(info->mapIndex);
    }

    EVENTFUNC(viking_floor1_bosshp_event)
    {
        auto* info = dynamic_cast<viking_event_info*>(event->info);
        if (!info)
            return 0;
        return s_viking.OnFloor1BossHp(info->mapIndex);
    }

    EVENTFUNC(viking_floor2_timer_event)
    {
        auto* info = dynamic_cast<viking_event_info*>(event->info);
        if (!info)
            return 0;
        return s_viking.OnFloor2Timer(info->mapIndex);
    }

    EVENTFUNC(viking_floor2_transition_event)
    {
        auto* info = dynamic_cast<viking_event_info*>(event->info);
        if (!info)
            return 0;
        return s_viking.OnFloor2Transition(info->mapIndex);
    }

    EVENTFUNC(viking_floor4_transition_event)
    {
        auto* info = dynamic_cast<viking_event_info*>(event->info);
        if (!info)
            return 0;
        return s_viking.OnFloor4Transition(info->mapIndex);
    }

    EVENTFUNC(viking_finalboss_spawn_event)
    {
        auto* info = dynamic_cast<viking_event_info*>(event->info);
        if (!info)
            return 0;
        return s_viking.OnFinalSpawn(info->mapIndex);
    }

    EVENTFUNC(viking_finalboss_hp_event)
    {
        auto* info = dynamic_cast<viking_event_info*>(event->info);
        if (!info)
            return 0;
        return s_viking.OnFinalHp(info->mapIndex);
    }

    EVENTFUNC(viking_complete_event)
    {
        auto* info = dynamic_cast<viking_event_info*>(event->info);
        if (!info)
            return 0;
        return s_viking.OnComplete(info->mapIndex);
    }

    EVENTFUNC(viking_out_event)
    {
        auto* info = dynamic_cast<viking_event_info*>(event->info);
        if (!info)
            return 0;
        return s_viking.OnOut(info->mapIndex);
    }
}

CVikingDungeon& CVikingDungeon::instance()
{
    static CVikingDungeon s;
    return s;
}

bool CVikingDungeon::IsVikingDungeonMap(int32_t mapIndex) const
{
    return IsInRange(mapIndex, kPrivateMin, kPrivateMax);
}

void CVikingDungeon::OnPlayerDisconnect(entt::entity character)
{
    LPCHARACTER ch = ecs::LegacyCharOf(character);
    if (!ch || !ecs::PlayerRuntime::IsPC(character))
        return;

    const int32_t idx = ecs::PlayerRuntime::GetMapIndex(character);
    if (!IsVikingDungeonMap(idx))
        return;

    LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
    if (!d)
        return;

    SetOutsideWarpLocation(ch);
    if (d->GetFlag(kFlagCompleted) == 0 && d->GetFlag(kFlagBlockRejoin) == 0)
        SetRejoinFlags(ch, idx);
}

void CVikingDungeon::OnPlayerLogin(entt::entity character)
{
    LPCHARACTER ch = ecs::LegacyCharOf(character);
    if (!ch || !ecs::PlayerRuntime::IsPC(character))
        return;

    const int32_t idx = ecs::PlayerRuntime::GetMapIndex(character);

    if (idx == kOriginalMap)
    {
        WarpOut(ch);
        return;
    }

    if (!IsVikingDungeonMap(idx))
        return;

    LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
    if (!d)
    {
        WarpOut(ch);
        return;
    }

    SetOutsideWarpLocation(ch);
    ecs::QuestSystem::SetFlag(character, kQfIdx, idx);
    ecs::QuestSystem::SetFlag(character, kQfCh, (int32_t)g_bChannel);

    if (d->GetFlag(kFlagBlockRejoin) != 0)
    {
        WarpOut(ch);
        return;
    }

    if (d->GetFlag(kFlagInitialized) == 0)
    {
        d->SetFlag(kFlagInitialized, 1);
        LPCHARACTER gate = d->SpawnMob(kGateNpc, kGatePos1.x, kGatePos1.y, kGatePos1.dir);
        if (gate)
	d->SetUnique("vk_gate_1", ecs::PlayerRuntime::GetPacketVID(((gate) ? (gate)->GetEntityHandle() : entt::null)));

        LPCHARACTER compass = d->SpawnMob(kCompassEmptyNpc, kCompassPos.x, kCompassPos.y, kCompassPos.dir);
        if (compass)
	d->SetUnique("vk_compass", ecs::PlayerRuntime::GetPacketVID(((compass) ? (compass)->GetEntityHandle() : entt::null)));

        NoticeMap(idx, "<Frostbane Fortress> Starting in 10 seconds. Get ready.");
        s_viking.ScheduleStart(idx);
    }

    if (ecs::QuestSystem::GetFlag(character, kQfDisconnect) > 0)
    {
        ecs::QuestSystem::SetFlag(character, kQfDisconnect, 0);
        ecs::ChatSystem::Send(character, CHAT_TYPE_BIG_NOTICE, "Welcome back.");

        const int32_t limit = d->GetFlag(kFlagTimeLimit);
        if (d->GetFlag(kFlagCompleted) != 0)
        {
            ecs::ChatSystem::Send(character, CHAT_TYPE_BIG_NOTICE, "This instance will close soon. Pick up your drops quickly.");
        }
        else if (limit > get_global_time())
        {
            char tmp[64];
            FormatDuration(limit - get_global_time(), tmp, sizeof(tmp));
            ecs::ChatSystem::Send(character, CHAT_TYPE_BIG_NOTICE, "Time remaining: %s.", tmp);
        }
    }

    SetCooldown(ch);
}

bool CVikingDungeon::OnUseItem(entt::entity character, CItem* item)
{
    LPCHARACTER ch = ecs::LegacyCharOf(character);
    if (!ch || !item)
        return false;

    if (ItemSystem::GetItemVnum((item ? item->GetEntityHandle() : entt::null)) != kResetItemVnum)
        return false;

    if (IsVikingDungeonMap(ecs::PlayerRuntime::GetMapIndex(character)))
    {
        ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "You cannot use this item while inside the dungeon.");
        return true;
    }

    const int32_t cooldownUntil = ecs::QuestSystem::GetFlag(character, kQfCooldown);
    if (cooldownUntil <= get_global_time())
    {
        ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "You can already enter the dungeon.");
        return true;
    }

    ch->RemoveSpecifyItem(kResetItemVnum, 1);
    ecs::QuestSystem::SetFlag(character, kQfCooldown, 0);
    ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Dungeon cooldown reset successfully.");
    return true;
}

bool CVikingDungeon::OnClickNpc(entt::entity character, entt::entity npc)
{
    LPCHARACTER ch = ecs::LegacyCharOf(character);
    LPCHARACTER pkNpc = ecs::LegacyCharOf(npc);
    if (!ch || !pkNpc || !ecs::PlayerRuntime::IsPC(character))
        return false;

    const uint32_t race = ecs::PlayerRuntime::GetRaceNum(npc);
    const int32_t now = get_global_time();

    const int32_t currentIdx = ecs::PlayerRuntime::GetMapIndex(character);
    const bool isInsideViking = IsVikingDungeonMap(currentIdx);
    LPDUNGEON currentDungeon = isInsideViking ? CDungeonManager::instance().FindByMapIndex(currentIdx) : nullptr;
    const bool quickRestart = (race == kEntryNpcVnum && currentDungeon && currentDungeon->GetFlag(kFlagCompleted) != 0);

    if (race == kRewardChestVnum)
    {
        const int32_t idx = ecs::PlayerRuntime::GetMapIndex(character);
        if (!IsVikingDungeonMap(idx))
            return false;

        LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
        if (!d || d->GetFlag(kFlagCompleted) == 0)
            return false;

        char rewardFlag[64];
        snprintf(rewardFlag, sizeof(rewardFlag), "vk_reward_%u", ecs::PlayerRuntime::GetPlayerID(character));
        if (d->GetFlag(rewardFlag) != 0)
        {
            ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "You already took your reward.");
            return true;
        }

        d->SetFlag(rewardFlag, 1);
        ch->AutoGiveItem(kRewardItemVnum, kRewardItemCount);
        ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Reward received.");
        return true;
    }

    if (race != kEntryNpcVnum)
        return false;

    const int32_t disconnectUntil = ecs::QuestSystem::GetFlag(character, kQfDisconnect);
    const int32_t rejoinIdx = ecs::QuestSystem::GetFlag(character, kQfIdx);
    const int32_t rejoinCh = ecs::QuestSystem::GetFlag(character, kQfCh);

    if (disconnectUntil > now && rejoinIdx > 0 && rejoinCh == (int32_t)g_bChannel && IsVikingDungeonMap(rejoinIdx))
    {
        LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(rejoinIdx);
        if (d && d->GetFlag(kFlagCompleted) == 0 && d->GetFlag(kFlagBlockRejoin) == 0)
        {
            ecs::MovementSystem::WarpSet(character, kEnterGlobalX * 100, kEnterGlobalY * 100, rejoinIdx);
            ecs::QuestSystem::SetFlag(character, kQfDisconnect, 0);
            return true;
        }
    }

    //if (!IsEntryMapForEmpire(ch))
    //{
    //    ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "You must be in the correct map to enter Frostbane Fortress.");
    //    return true;
    //}

    if (!ecs::PlayerRuntime::CanWarp(character))
    {
        ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "You have to wait a bit before entering.");
        return true;
    }

    if (quest::CQuestManager::instance().GetEventFlag("vikingdungeon_zone_block") == 1 && !ch->IsGM())
    {
        ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "The dungeon is currently blocked.");
        return true;
    }

    char antiSpamFlag[64];
    snprintf(antiSpamFlag, sizeof(antiSpamFlag), "vikingdungeon_%d", (int)g_bChannel);
    const int32_t antiSpamUntil = quest::CQuestManager::instance().GetEventFlag(antiSpamFlag);
    if (antiSpamUntil > now)
    {
        ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Please wait a moment.");
        return true;
    }
    quest::CQuestManager::instance().SetEventFlag(antiSpamFlag, now + kAntiSpamSec);

    LPPARTY party = ecs::SocialSystem::GetParty(character);
    if (party)
    {
        if (party->GetLeaderPID() != ecs::PlayerRuntime::GetPlayerID(character))
        {
            ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Only the party leader can start the dungeon.");
            return true;
        }

        if (party->GetNearMemberCount() != party->GetMemberCount())
        {
            ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Every party member must be online and near the NPC.");
            return true;
        }

        if ((int32_t)party->GetMemberCount() < kMinMembers)
        {
            ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Your party needs at least %d members.", kMinMembers);
            return true;
        }
    }

    enum EBadReason
    {
        BAD_NONE,
        BAD_LEVEL,
        BAD_WARP,
        BAD_ITEM,
        BAD_COOLDOWN,
    };

    EBadReason bad = BAD_NONE;
    const char* badName = nullptr;
    int32_t badVal = 0;
    bool ok = true;

    auto checkMember = [&](entt::entity m){
        LPCHARACTER pkM = ecs::LegacyCharOf(m);
        if (!pkM || !ecs::PlayerRuntime::IsPC(m) || !ok)
            return;

        if (ecs::PointSystem::GetLevel(m) < kMinLevel || ecs::PointSystem::GetLevel(m) > kMaxLevel)
        {
            ok = false;
            bad = BAD_LEVEL;
            badName = ecs::PlayerRuntime::GetName(m).data();
            badVal = ecs::PointSystem::GetLevel(m);
            return;
        }

        if (!ecs::PlayerRuntime::CanWarp(m))
        {
            ok = false;
            bad = BAD_WARP;
            badName = ecs::PlayerRuntime::GetName(m).data();
            return;
        }

        if (!quickRestart)
        {
            if (pkM->CountSpecifyItem(kEntryItemVnum) < kEntryItemCount)
            {
                ok = false;
                bad = BAD_ITEM;
                badName = ecs::PlayerRuntime::GetName(m).data();
                return;
            }

            const int32_t cd = ecs::QuestSystem::GetFlag(m, kQfCooldown);
            if (cd > now)
            {
                ok = false;
                bad = BAD_COOLDOWN;
                badName = ecs::PlayerRuntime::GetName(m).data();
                badVal = cd - now;
                return;
            }
        }
    };
    auto checkMemberPtr = [&](LPCHARACTER pkMember) { checkMember(pkMember ? pkMember->GetEntityHandle() : entt::null); };

    if (party)
        party->ForEachOnMapMember(checkMemberPtr, ecs::PlayerRuntime::GetMapIndex(character));
    else
        checkMember(character);

    if (!ok)
    {
        char tmp[64];
        switch (bad)
        {
            case BAD_LEVEL:
                ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "%s has invalid level (Lv%d). Required: %d-%d.", badName ? badName : "A member", badVal, kMinLevel, kMaxLevel);
                break;
            case BAD_WARP:
                ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "%s cannot warp yet.", badName ? badName : "A member");
                break;
            case BAD_ITEM:
                ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "%s does not have the required entry item.", badName ? badName : "A member");
                break;
            case BAD_COOLDOWN:
                FormatDuration(badVal, tmp, sizeof(tmp));
                ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "%s is still on cooldown (%s).", badName ? badName : "A member", tmp);
                break;
            default:
                break;
        }
        return true;
    }

    LPDUNGEON d = CDungeonManager::instance().Create(kOriginalMap);
    if (!d)
    {
        ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Failed to create dungeon instance.");
        return true;
    }

    const int32_t dungeonMapIdx = d->GetMapIndex();

    auto prepareMember = [&](entt::entity m){
        LPCHARACTER pkM = ecs::LegacyCharOf(m);
        if (!pkM || !ecs::PlayerRuntime::IsPC(m))
            return;

        SetOutsideWarpLocation(pkM);
        ClearRejoinFlags(pkM);
        ecs::QuestSystem::SetFlag(m, kQfIdx, dungeonMapIdx);
        ecs::QuestSystem::SetFlag(m, kQfCh, (int32_t)g_bChannel);

        if (!quickRestart)
        {
            SetCooldown(pkM);
            pkM->RemoveSpecifyItem(kEntryItemVnum, kEntryItemCount);
        }
    };
    auto prepareMemberPtr = [&](LPCHARACTER pkMember) { prepareMember(pkMember ? pkMember->GetEntityHandle() : entt::null); };

    if (party)
        party->ForEachOnMapMember(prepareMemberPtr, ecs::PlayerRuntime::GetMapIndex(character));
    else
        prepareMember(character);

    SetDungeonReady(d);

    if (party)
        d->JoinParty_Coords(party, kEnterGlobalX, kEnterGlobalY, ecs::PlayerRuntime::GetMapIndex(character));
    else
        d->Join_Coords(ch, kEnterGlobalX, kEnterGlobalY, ecs::PlayerRuntime::GetMapIndex(character));

    BigNoticeMap(dungeonMapIdx, "<Frostbane Fortress> Dungeon instance created.");
    return true;
}

bool CVikingDungeon::OnNpcTakeItem(entt::entity from, entt::entity npc, CItem* item)
{
    LPCHARACTER pkFrom = ecs::LegacyCharOf(from);
    LPCHARACTER pkNpc = ecs::LegacyCharOf(npc);
    if (!pkFrom || !pkNpc || !item || !ecs::PlayerRuntime::IsPC(from))
        return false;

    const int32_t idx = ecs::PlayerRuntime::GetMapIndex(from);
    if (!IsVikingDungeonMap(idx))
        return false;

    LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
    if (!d)
        return false;

    const uint32_t npcVnum = ecs::PlayerRuntime::GetRaceNum(npc);
    const uint32_t itemVnum = ItemSystem::GetItemVnum((item ? item->GetEntityHandle() : entt::null));
    const int32_t floor = d->GetFlag(kFlagFloor);

    if (floor == 1 && itemVnum == kFloor1ItemVnum)
    {
        if (npcVnum == kCompassEmptyNpc && d->GetFlag(kFlagCompassState) == 0)
        {
            pkFrom->RemoveSpecifyItem(kFloor1ItemVnum, 1);
            d->SetFlag(kFlagCompassState, 1);
            ReplaceCompass(d, pkNpc, kCompassSmallNpc);
            d->SpawnRegen(kRegen1FloorA, true);
            s_viking.ScheduleFloor1Check(idx);
            NoticeMap(idx, "<Frostbane Fortress> Compass activated. Clear the monsters again.");
            return true;
        }

        if (npcVnum == kCompassSmallNpc && d->GetFlag(kFlagCompassState) == 1)
        {
            pkFrom->RemoveSpecifyItem(kFloor1ItemVnum, 1);
            d->SetFlag(kFlagCompassState, 2);
            ReplaceCompass(d, pkNpc, kCompassMediumNpc);
            d->SpawnRegen(kRegen1FloorA, true);
            s_viking.ScheduleFloor1Check(idx);
            NoticeMap(idx, "<Frostbane Fortress> Compass empowered further. Clear the monsters again.");
            return true;
        }

        if (npcVnum == kCompassMediumNpc && d->GetFlag(kFlagCompassState) == 2)
        {
            pkFrom->RemoveSpecifyItem(kFloor1ItemVnum, 1);
            d->SetFlag(kFlagCompassState, 3);
            ReplaceCompass(d, pkNpc, kCompassLargeNpc);
            NoticeMap(idx, "<Frostbane Fortress> The compass was ignited successfully.");
            NoticeMap(idx, "<Frostbane Fortress> The first main boss will appear soon.");
            s_viking.ScheduleFloor1Boss(idx);
            return true;
        }
    }

    if (floor == 4 && itemVnum == kFloor3ItemVnum && d->GetFlag(kFlagCanUseRune) == 1)
    {
        if (npcVnum != kMemorialNpc1 && npcVnum != kMemorialNpc2 && npcVnum != kMemorialNpc3)
            return false;

        pkFrom->RemoveSpecifyItem(kFloor3ItemVnum, 1);
        d->SetFlag(kFlagCanUseRune, 0);

        int32_t stage = d->GetFlag(kFlagFloor3NpcStage) + 1;
        d->SetFlag(kFlagFloor3NpcStage, stage);

        pkNpc->Dead(nullptr, true);

        uint32_t newNpc = kMemorialNpc4;
        if (stage == 1)
            newNpc = kMemorialNpc2;
        else if (stage == 2)
            newNpc = kMemorialNpc3;
        else if (stage >= 3)
            newNpc = kMemorialNpc4;

        d->SetFlag(kFlagFloor3NpcVnum, newNpc);
        LPCHARACTER memorial = d->SpawnMob(newNpc, kMemorialPos.x, kMemorialPos.y, kMemorialPos.dir);
        if (memorial)
		d->SetUnique("vk_memorial", ecs::PlayerRuntime::GetPacketVID(((memorial) ? (memorial)->GetEntityHandle() : entt::null)));

        if (stage < 3)
        {
            RespawnFloor3StonesFromProtectors(d);
            NoticeMap(idx, "<Frostbane Fortress> The stones are vulnerable again. Destroy them to proceed.");
        }
        else
        {
            NoticeMap(idx, "<Frostbane Fortress> The last rune was summoned.");
            NoticeMap(idx, "<Frostbane Fortress> The final boss will appear in 10 seconds.");
            s_viking.ScheduleFinalSpawn(idx);
        }
        return true;
    }

    return false;
}

void CVikingDungeon::OnMobKilled(entt::entity killer, entt::entity victim)
{
    LPCHARACTER pkKiller = ecs::LegacyCharOf(killer);
    LPCHARACTER pkVictim = ecs::LegacyCharOf(victim);
    if (!pkKiller || !pkVictim || !ecs::PlayerRuntime::IsPC(killer))
        return;

    const int32_t idx = ecs::PlayerRuntime::GetMapIndex(killer);
    if (!IsVikingDungeonMap(idx))
        return;

    LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
    if (!d)
        return;

    const uint32_t vnum = ecs::PlayerRuntime::GetRaceNum(victim);
    const int32_t floor = d->GetFlag(kFlagFloor);

    if (floor == 1 && vnum == kFloor1LowBossVnum)
    {
        pkKiller->AutoGiveItem(kFloor1ItemVnum, 1);
        NoticeMap(idx, "<Frostbane Fortress> %s received the required item. Use it on the compass.", ecs::PlayerRuntime::GetName(killer).data());
        return;
    }

    if (floor == 2 && vnum == kFloor1MainBossVnum)
    {
        s_viking.Cancel(s_viking.evFloor1BossHp, idx);
        NoticeMap(idx, "<Frostbane Fortress> The first main boss was defeated.");
        s_viking.ScheduleFloor2Transition(idx);
        return;
    }

    if (floor == 3 && vnum == kFloor2StoneVnum)
    {
        const int32_t remain = std::max(0, d->GetFlag(kFlagFloor2Remain) - 1);
        d->SetFlag(kFlagFloor2Remain, remain);
        if (remain > 0)
        {
            NoticeMap(idx, "<Frostbane Fortress> Remaining stones: %d.", remain);
            return;
        }

        s_viking.ScheduleFloor4Transition(idx);
        return;
    }

    if (floor == 4 && vnum == kFloor3StoneVnum)
    {
	const int killedSlot = FindFloor3StoneSlotByVid(d, ecs::PlayerRuntime::GetPacketVID(victim));
        if (killedSlot >= 0)
            SetFloor3SlotCleared(d, killedSlot, true);

        SpawnFloor3ProtectorsForRemainingSlots(d);

        const int32_t bossLocalX = std::max<int32_t>(1, ecs::PlayerRuntime::GetX(killer) / 100 - kBaseCellX);
        const int32_t bossLocalY = std::max<int32_t>(1, ecs::PlayerRuntime::GetY(killer) / 100 - kBaseCellY);
        LPCHARACTER boss = d->SpawnMob(kFloor3BossVnum, bossLocalX, bossLocalY, 0);
        if (boss)
		d->SetUnique("vk_floor3_boss", ecs::PlayerRuntime::GetPacketVID(((boss) ? (boss)->GetEntityHandle() : entt::null)));
        d->SetFlag(kFlagCanKillFloor3Boss, 1);
        NoticeMap(idx, "<Frostbane Fortress> The protecting boss appeared. Kill it to proceed.");
        return;
    }

    if (floor == 4 && vnum == kFloor3BossVnum)
    {
        if (d->GetFlag(kFlagCanKillFloor3Boss) == 1)
        {
            d->SetFlag(kFlagCanKillFloor3Boss, 0);
            d->SetFlag(kFlagCanUseRune, 1);
            pkKiller->AutoGiveItem(kFloor3ItemVnum, 1);
            NoticeMap(idx, "<Frostbane Fortress> %s received the rune item. Use it on the memorial.", ecs::PlayerRuntime::GetName(killer).data());
        }
        return;
    }

    if (floor == 5 && vnum == kFinalBossVnum && d->GetFlag(kFlagCompleted) == 0)
    {
        const char* leaderName = ecs::PlayerRuntime::GetName(killer).data();

        if (ecs::SocialSystem::GetParty(killer))
        {
            LPCHARACTER leader = ecs::SocialSystem::GetParty(killer)->GetLeaderCharacter();
            if (leader)
                leaderName = ecs::PlayerRuntime::GetName(((leader) ? (leader)->GetEntityHandle() : entt::null)).data();
        }

        char notice[256];
        if (ecs::SocialSystem::GetParty(killer))
            std::snprintf(notice, sizeof(notice), "%s es csoportja teljesitette a Fagyos dungeont!", leaderName);
        else
            std::snprintf(notice, sizeof(notice), "%s befejezte a Fagyos dungeont!", ecs::PlayerRuntime::GetName(killer).data());

        BroadcastNotice(notice);

        d->SetFlag(kFlagCompleted, 1);
        d->SetFlag(kFlagBlockRejoin, 1);
        s_viking.ScheduleComplete(idx);
        return;
    }
}

