#include "stdafx.h"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../ecs/systems/QuestSystem.hpp"
#include "../ecs/AIHelpers.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/MovementSystem.hpp"
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
#include "../ecs/EntityFactory.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/systems/ItemSystem.hpp"
#include "../ecs/CharacterAccessors.hpp"

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

    void ResetFloor3Progress(entt::entity d)
    {
        if (d == entt::null)
            return;

        for (int i = 0; i < 3; ++i)
            DungeonSystem::SetFlag(d, GetFloor3ClearedFlag(i), 0);
    }

    inline bool IsFloor3SlotCleared(entt::entity d, int idx)
    {
        return d != entt::null && DungeonSystem::GetFlag(d, GetFloor3ClearedFlag(idx)) != 0;
    }

    inline void SetFloor3SlotCleared(entt::entity d, int idx, bool value)
    {
        if (d != entt::null)
            DungeonSystem::SetFlag(d, GetFloor3ClearedFlag(idx), value ? 1 : 0);
    }

    inline int FindFloor3StoneSlotByVid(entt::entity d, uint32_t vid)
    {
        if (d == entt::null || vid == 0)
            return -1;

        for (int i = 0; i < 3; ++i)
        {
            if (DungeonSystem::GetUniqueVid(d, GetFloor3StoneKey(i)) == (int32_t)vid)
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
            void operator()(entt::entity ch)
            {
                if (!ecs::IsCharacter(ch) || !ecs::PlayerRuntime::IsPC(ch))
                    return;
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

    void GetOutsideWarpByEmpire(uint8_t /*empire*/, int32_t& mapIdx, int32_t& x, int32_t& y)
    {
        mapIdx = 219;

        x = 5369;
        y = 14350;
    }//536904	1435017

    void SetOutsideWarpLocation(entt::entity ch)
    {
        if (!ecs::PlayerRuntime::IsValid(ch))
            return;
        int32_t mapIdx = 1, x = 0, y = 0;
        GetOutsideWarpByEmpire(ecs::PlayerRuntime::GetEmpire(ch), mapIdx, x, y);
        ecs::MovementSystem::SetWarpLocation(ch, mapIdx, x, y);
    }

    void WarpOut(entt::entity ch)
    {
        if (!ecs::PlayerRuntime::IsValid(ch))
            return;
        int32_t mapIdx = 1, x = 0, y = 0;
        GetOutsideWarpByEmpire(ecs::PlayerRuntime::GetEmpire(ch), mapIdx, x, y);
        ecs::MovementSystem::WarpSet(ch, x * 100, y * 100, mapIdx);
    }

    void WarpAllOut(int32_t mapIndex)
    {
        ForEachPcOnMap(mapIndex, [&](entt::entity ch){
            WarpOut(ch);
        });
    }

    void SetCooldown(entt::entity ch)
    {
        if (ecs::PlayerRuntime::IsValid(ch))
            ecs::QuestSystem::SetFlag(ch, kQfCooldown, get_global_time() + kEntranceCooldownSec);
    }

    void SetRejoinFlags(entt::entity ch, int32_t mapIndex)
    {
        if (!ecs::PlayerRuntime::IsValid(ch))
            return;
        ecs::QuestSystem::SetFlag(ch, kQfIdx, mapIndex);
        ecs::QuestSystem::SetFlag(ch, kQfCh, (int32_t)g_bChannel);
        ecs::QuestSystem::SetFlag(ch, kQfDisconnect, get_global_time() + kRejoinSec);
    }

    void ClearRejoinFlags(entt::entity ch)
    {
        if (!ecs::PlayerRuntime::IsValid(ch))
            return;
        ecs::QuestSystem::SetFlag(ch, kQfDisconnect, 0);
    }

    void ClearDungeonNonPlayers(entt::entity d)
    {
        if (d == entt::null)
            return;

        DungeonSystem::ClearRegen(d);
        DungeonSystem::KillAllMonsters(d);
    }

    void ApplyMapHpPctDamage(int32_t mapIndex, int pct)
    {
        if (pct <= 0)
            return;

        ForEachPcOnMap(mapIndex, [&](entt::entity ch){
            if (!ecs::PlayerRuntime::IsValid(ch) || ecs::PlayerRuntime::GetHP(ch) <= 1)
                return;

            int64_t dmg = (ecs::PlayerRuntime::GetHP(ch) * pct) / 100;
            if (dmg < 1)
                dmg = 1;
            if (dmg >= ecs::PlayerRuntime::GetHP(ch))
                dmg = ecs::PlayerRuntime::GetHP(ch) - 1;
            if (dmg > 0)
                ecs::PointSystem::Change(ch, POINT_HP, -dmg);
        });
    }

    entt::entity FindUnique(entt::entity d, const char* key)
    {
        if (d == entt::null)
            return entt::null;
        const int32_t vid = DungeonSystem::GetUniqueVid(d, key);
        if (vid <= 0)
            return entt::null;
        return CHARACTER_MANAGER::instance().FindEntity((uint32_t)vid);
    }

    void SetDungeonReady(entt::entity d)
    {
        if (d == entt::null)
            return;

        DungeonSystem::SetFlag(d, kFlagInitialized, 0);
        DungeonSystem::SetFlag(d, kFlagBlockRejoin, 0);
        DungeonSystem::SetFlag(d, kFlagCompleted, 0);
        DungeonSystem::SetFlag(d, kFlagFloor, 0);
        DungeonSystem::SetFlag(d, kFlagStartTime, 0);
        DungeonSystem::SetFlag(d, kFlagTimeLimit, 0);
        DungeonSystem::SetFlag(d, kFlagCompassState, 0);
        DungeonSystem::SetFlag(d, kFlagMainBossStage, 0);
        DungeonSystem::SetFlag(d, kFlagFloor2Remain, 0);
        DungeonSystem::SetFlag(d, kFlagFinalPenalty, 0);
        DungeonSystem::SetFlag(d, kFlagFloor3NpcStage, 0);
        DungeonSystem::SetFlag(d, kFlagFloor3NpcVnum, kMemorialNpc1);
        DungeonSystem::SetFlag(d, kFlagCanKillFloor3Boss, 0);
        DungeonSystem::SetFlag(d, kFlagCanUseRune, 0);
        DungeonSystem::SetFlag(d, kFlagFinalBossStage, 0);
        ResetFloor3Progress(d);
    }

    void SpawnFloor4Setup(entt::entity d)
    {
        if (d == entt::null)
            return;

        DungeonSystem::SetFlag(d, kFlagFloor, 4);
        DungeonSystem::SetFlag(d, kFlagCanKillFloor3Boss, 0);
        DungeonSystem::SetFlag(d, kFlagCanUseRune, 0);
        DungeonSystem::SetFlag(d, kFlagFloor3NpcStage, 0);
        DungeonSystem::SetFlag(d, kFlagFloor3NpcVnum, kMemorialNpc1);
        ResetFloor3Progress(d);

        const entt::entity memorial = DungeonSystem::SpawnMob(d, kMemorialNpc1, kMemorialPos.x, kMemorialPos.y, kMemorialPos.dir);
        if (memorial != entt::null)
		DungeonSystem::SetUnique(d, "vk_memorial", ecs::PlayerRuntime::GetPacketVID(memorial));

        for (int i = 0; i < 3; ++i)
        {
            const entt::entity stone = DungeonSystem::SpawnMob(d, kFloor3StoneVnum, kFloor3Stones[i].x, kFloor3Stones[i].y, kFloor3Stones[i].dir);
            if (stone != entt::null)
				DungeonSystem::SetUnique(d, GetFloor3StoneKey(i), ecs::PlayerRuntime::GetPacketVID(stone));
        }
    }

    void RespawnFloor3StonesFromProtectors(entt::entity d)
    {
        if (d == entt::null)
            return;

        for (int i = 0; i < 3; ++i)
        {
            if (DungeonSystem::GetUniqueVid(d, GetFloor3ProtectorKey(i)) > 0)
                DungeonSystem::KillUnique(d, GetFloor3ProtectorKey(i));

            if (DungeonSystem::GetUniqueVid(d, GetFloor3StoneKey(i)) > 0)
                DungeonSystem::KillUnique(d, GetFloor3StoneKey(i));

            if (IsFloor3SlotCleared(d, i))
                continue;

            const entt::entity stone = DungeonSystem::SpawnMob(d, kFloor3StoneVnum, kFloor3Stones[i].x, kFloor3Stones[i].y, kFloor3Stones[i].dir);
            if (stone != entt::null)
				DungeonSystem::SetUnique(d, GetFloor3StoneKey(i), ecs::PlayerRuntime::GetPacketVID(stone));
        }
    }

    void SpawnFloor3ProtectorsForRemainingSlots(entt::entity d)
    {
        if (d == entt::null)
            return;

        for (int i = 0; i < 3; ++i)
        {
            if (DungeonSystem::GetUniqueVid(d, GetFloor3StoneKey(i)) > 0)
                DungeonSystem::KillUnique(d, GetFloor3StoneKey(i));

            if (DungeonSystem::GetUniqueVid(d, GetFloor3ProtectorKey(i)) > 0)
                DungeonSystem::KillUnique(d, GetFloor3ProtectorKey(i));

            if (IsFloor3SlotCleared(d, i))
                continue;

            const entt::entity protector = DungeonSystem::SpawnMob(d, kStoneProtectorNpc, kFloor3Stones[i].x, kFloor3Stones[i].y, kFloor3Stones[i].dir);
            if (protector != entt::null)
			DungeonSystem::SetUnique(d, GetFloor3ProtectorKey(i), ecs::PlayerRuntime::GetPacketVID(protector));
        }
    }


    void ReplaceCompass(entt::entity d, entt::entity npc, uint32_t newVnum)
    {
        if (d == entt::null || !ecs::PlayerRuntime::IsValid(npc))
            return;

        if (DungeonSystem::GetUniqueVid(d, "vk_compass") > 0)
            DungeonSystem::KillUnique(d, "vk_compass");

        const entt::entity spawned = DungeonSystem::SpawnMob(d, newVnum, kCompassPos.x, kCompassPos.y, kCompassPos.dir);
        if (spawned != entt::null)
		DungeonSystem::SetUnique(d, "vk_compass", ecs::PlayerRuntime::GetPacketVID(spawned));

        CombatSystem::Dead(npc, entt::null, true);
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
            const entt::entity d = CDungeonManager::instance().FindByMapIndex(idx);
            if (d == entt::null || DungeonSystem::GetFlag(d, kFlagCompleted) != 0)
                return 0;

            DungeonSystem::SetFlag(d, kFlagFloor, 1);
            DungeonSystem::SetFlag(d, kFlagStartTime, get_global_time());
            DungeonSystem::SetFlag(d, kFlagTimeLimit, get_global_time() + kTimeOutSec);
            ScheduleTimeout(idx);
            ScheduleFloor1Check(idx);
            DungeonSystem::SpawnRegen(d, kRegen1FloorA, true);

            NoticeMap(idx, "<Frostbane Fortress> You have 20 minutes to complete the dungeon.");
            NoticeMap(idx, "<Frostbane Fortress> Eliminate all monsters to summon the low boss.");
            return 0;
        }

        long OnTimeout(int32_t idx)
        {
            const entt::entity d = CDungeonManager::instance().FindByMapIndex(idx);
            if (d == entt::null)
            {
                evTimeout.erase(idx);
                return 0;
            }

            if (DungeonSystem::GetFlag(d, kFlagCompleted) != 0)
            {
                evTimeout.erase(idx);
                return 0;
            }

            const int32_t now = get_global_time();
            const int32_t limit = DungeonSystem::GetFlag(d, kFlagTimeLimit);
            if (limit <= 0)
            {
                evTimeout.erase(idx);
                return 0;
            }

            if (now >= limit)
            {
                evTimeout.erase(idx);
                DungeonSystem::SetFlag(d, kFlagBlockRejoin, 1);
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
            const entt::entity d = CDungeonManager::instance().FindByMapIndex(idx);
            if (d == entt::null)
            {
                evFloor1Check.erase(idx);
                return 0;
            }
            if (DungeonSystem::GetFlag(d, kFlagFloor) != 1)
            {
                evFloor1Check.erase(idx);
                return 0;
            }

            if (DungeonSystem::CountMonster(d) > 0)
                return PASSES_PER_SEC(kPhaseCheckIntervalSec);

            evFloor1Check.erase(idx);
            DungeonSystem::SpawnMob(d, kFloor1LowBossVnum, kFloor1LowBossPos.x, kFloor1LowBossPos.y, kFloor1LowBossPos.dir);
            NoticeMap(idx, "<Frostbane Fortress> The low boss has been summoned. Kill him to proceed.");
            return 0;
        }

        long OnFloor1MainBoss(int32_t idx)
        {
            evFloor1Boss.erase(idx);
            const entt::entity d = CDungeonManager::instance().FindByMapIndex(idx);
            if (d == entt::null)
                return 0;

            DungeonSystem::SetFlag(d, kFlagFloor, 2);
            DungeonSystem::SetFlag(d, kFlagMainBossStage, 0);
            const entt::entity boss = DungeonSystem::SpawnMob(d, kFloor1MainBossVnum, kFloor1MainBossPos.x, kFloor1MainBossPos.y, kFloor1MainBossPos.dir);
            if (boss != entt::null)
	DungeonSystem::SetUnique(d, "vk_main_boss", ecs::PlayerRuntime::GetPacketVID(boss));
            ScheduleFloor1BossHp(idx);
            BigNoticeMap(idx, "<Frostbane Fortress> The first main boss has appeared!");
            return 0;
        }

        long OnFloor1BossHp(int32_t idx)
        {
            const entt::entity d = CDungeonManager::instance().FindByMapIndex(idx);
            if (d == entt::null)
            {
                evFloor1BossHp.erase(idx);
                return 0;
            }
            if (DungeonSystem::GetFlag(d, kFlagFloor) != 2)
            {
                evFloor1BossHp.erase(idx);
                return 0;
            }

            const entt::entity boss = FindUnique(d, "vk_main_boss");
            if (!ecs::IsCharacter(boss) || CombatSystem::IsDead(boss))
            {
                evFloor1BossHp.erase(idx);
                return 0;
            }

            const int stage = DungeonSystem::GetFlag(d, kFlagMainBossStage);
            if (stage >= 3)
                return PASSES_PER_SEC(1);

            const int hpPct = ecs::PlayerRuntime::GetHPPct(boss);
            if (hpPct > kMainBossHpStages[stage])
                return PASSES_PER_SEC(1);

            if (stage == 0)
            {
                DungeonSystem::SetFlag(d, kFlagMainBossStage, 1);
                DungeonSystem::SpawnRegen(d, kRegen1FloorB, true);
                NoticeMap(idx, "<Frostbane Fortress> The boss summoned many monsters. Be careful.");
            }
            else if (stage == 1)
            {
                DungeonSystem::SetFlag(d, kFlagMainBossStage, 2);
                ApplyMapHpPctDamage(idx, 35);
                NoticeMap(idx, "<Frostbane Fortress> The boss cast a dungeon-wide damage spell.");
            }
            else if (stage == 2)
            {
                DungeonSystem::SetFlag(d, kFlagMainBossStage, 3);
                CombatSystem::SetDamageMultiplier(boss, 0.5f);
                NoticeMap(idx, "<Frostbane Fortress> The boss reduced incoming damage by half.");
            }

            return PASSES_PER_SEC(1);
        }

        long OnFloor2Timer(int32_t idx)
        {
            evFloor2Timer.erase(idx);
            const entt::entity d = CDungeonManager::instance().FindByMapIndex(idx);
            if (d == entt::null)
                return 0;
            if (DungeonSystem::GetFlag(d, kFlagFloor) != 3)
                return 0;

            if (DungeonSystem::GetFlag(d, kFlagFloor2Remain) > 0)
                DungeonSystem::SetFlag(d, kFlagFinalPenalty, 1);

            NoticeMap(idx, "<Frostbane Fortress> You didn't destroy all stones in time.");
            NoticeMap(idx, "<Frostbane Fortress> Final boss will get double HP.");
            return 0;
        }

        long OnFloor2Transition(int32_t idx)
        {
            evFloor2Transition.erase(idx);
            const entt::entity d = CDungeonManager::instance().FindByMapIndex(idx);
            if (d == entt::null || DungeonSystem::GetFlag(d, kFlagCompleted) != 0)
                return 0;

            Cancel(evFloor1BossHp, idx);
            ClearDungeonNonPlayers(d);

            DungeonSystem::SetFlag(d, kFlagFloor, 3);
            DungeonSystem::SetFlag(d, kFlagFloor2Remain, 4);

            for (int i = 0; i < 4; ++i)
                DungeonSystem::SpawnMob(d, kFloor2StoneVnum, kFloor2Stones[i].x, kFloor2Stones[i].y, kFloor2Stones[i].dir);

            const entt::entity gate = DungeonSystem::SpawnMob(d, kGateNpc, kGatePos2.x, kGatePos2.y, kGatePos2.dir);
            if (gate != entt::null)
		DungeonSystem::SetUnique(d, "vk_gate_2", ecs::PlayerRuntime::GetPacketVID(gate));

            ScheduleFloor2Timer(idx);
            NoticeMap(idx, "<Frostbane Fortress> Destroy all second-floor stones within 4 minutes.");
            return 0;
        }

        long OnFloor4Transition(int32_t idx)
        {
            evFloor4Transition.erase(idx);
            const entt::entity d = CDungeonManager::instance().FindByMapIndex(idx);
            if (d == entt::null || DungeonSystem::GetFlag(d, kFlagCompleted) != 0)
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
            const entt::entity d = CDungeonManager::instance().FindByMapIndex(idx);
            if (d == entt::null || DungeonSystem::GetFlag(d, kFlagCompleted) != 0)
                return 0;

            DungeonSystem::SetFlag(d, kFlagFloor, 5);
            DungeonSystem::SetFlag(d, kFlagFinalBossStage, 0);
            const entt::entity boss = DungeonSystem::SpawnMob(d, kFinalBossVnum, kFinalBossPos.x, kFinalBossPos.y, kFinalBossPos.dir);
            if (boss != entt::null)
            {
                const int64_t hp = DungeonSystem::GetFlag(d, kFlagFinalPenalty) ? kFinalBossPenaltyHP : kFinalBossNormalHP;
                ecs::PlayerRuntime::SetMaxHP(boss, hp);
                ecs::PlayerRuntime::SetHP(boss, hp);
	DungeonSystem::SetUnique(d, "vk_final_boss", ecs::PlayerRuntime::GetPacketVID(boss));
            }
            ScheduleFinalHp(idx);
            BigNoticeMap(idx, "<Frostbane Fortress> The final boss has appeared!");
            return 0;
        }

        long OnFinalHp(int32_t idx)
        {
            const entt::entity d = CDungeonManager::instance().FindByMapIndex(idx);
            if (d == entt::null)
            {
                evFinalHp.erase(idx);
                return 0;
            }
            if (DungeonSystem::GetFlag(d, kFlagFloor) != 5)
            {
                evFinalHp.erase(idx);
                return 0;
            }

            const entt::entity boss = FindUnique(d, "vk_final_boss");
            if (!ecs::IsCharacter(boss) || CombatSystem::IsDead(boss))
            {
                evFinalHp.erase(idx);
                return 0;
            }

            const int stage = DungeonSystem::GetFlag(d, kFlagFinalBossStage);
            if (stage >= 3)
                return PASSES_PER_SEC(1);

            const int hpPct = ecs::PlayerRuntime::GetHPPct(boss);
            if (hpPct > kFinalBossHpStages[stage])
                return PASSES_PER_SEC(1);

            if (stage == 0)
            {
                DungeonSystem::SetFlag(d, kFlagFinalBossStage, 1);
                DungeonSystem::SpawnRegen(d, kRegen2FloorA, true);
                NoticeMap(idx, "<Frostbane Fortress> The final boss summoned many monsters. Be careful.");
            }
            else if (stage == 1)
            {
                DungeonSystem::SetFlag(d, kFlagFinalBossStage, 2);
                ApplyMapHpPctDamage(idx, 40);
                NoticeMap(idx, "<Frostbane Fortress> The final boss cast a dungeon-wide damage spell.");
            }
            else if (stage == 2)
            {
                DungeonSystem::SetFlag(d, kFlagFinalBossStage, 3);
                CombatSystem::SetDamageMultiplier(boss, 0.5f);
                NoticeMap(idx, "<Frostbane Fortress> The final boss reduced incoming damage by half.");
            }

            return PASSES_PER_SEC(1);
        }

        long OnComplete(int32_t idx)
        {
            evComplete.erase(idx);
            const entt::entity d = CDungeonManager::instance().FindByMapIndex(idx);
            if (d == entt::null)
                return 0;

            CancelAll(idx);

            ForEachPcOnMap(idx, [&](entt::entity member){
                SetCooldown(member);
            });

            ClearDungeonNonPlayers(d);
            //    DungeonSystem::SetUnique(d, "vk_entry_npc", ecs::PlayerRuntime::GetPacketVID(entryNpc));

            BigNoticeMap(idx, "<Frostbane Fortress> Dungeon completed!");
            NoticeMap(idx, "<Frostbane Fortress> Click the entry NPC if you want to restart immediately.");
            NoticeMap(idx, "<Frostbane Fortress> You will be teleported out in 2 minutes.");
            ScheduleOut(idx);
            return 0;
        }

        long OnOut(int32_t idx)
        {
            evOut.erase(idx);
            const entt::entity d = CDungeonManager::instance().FindByMapIndex(idx);
            if (d != entt::null)
                DungeonSystem::SetFlag(d, kFlagBlockRejoin, 1);
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
    if (!ecs::PlayerRuntime::IsPC(character))
        return;

    const int32_t idx = ecs::PlayerRuntime::GetMapIndex(character);
    if (!IsVikingDungeonMap(idx))
        return;

    const entt::entity d = CDungeonManager::instance().FindByMapIndex(idx);
    if (d == entt::null)
        return;

    SetOutsideWarpLocation(character);
    if (DungeonSystem::GetFlag(d, kFlagCompleted) == 0 && DungeonSystem::GetFlag(d, kFlagBlockRejoin) == 0)
        SetRejoinFlags(character, idx);
}

void CVikingDungeon::OnPlayerLogin(entt::entity character)
{
    if (!ecs::PlayerRuntime::IsPC(character))
        return;

    const int32_t idx = ecs::PlayerRuntime::GetMapIndex(character);

    if (idx == kOriginalMap)
    {
        WarpOut(character);
        return;
    }

    if (!IsVikingDungeonMap(idx))
        return;

    const entt::entity d = CDungeonManager::instance().FindByMapIndex(idx);
    if (d == entt::null)
    {
        WarpOut(character);
        return;
    }

    SetOutsideWarpLocation(character);
    ecs::QuestSystem::SetFlag(character, kQfIdx, idx);
    ecs::QuestSystem::SetFlag(character, kQfCh, (int32_t)g_bChannel);

    if (DungeonSystem::GetFlag(d, kFlagBlockRejoin) != 0)
    {
        WarpOut(character);
        return;
    }

    if (DungeonSystem::GetFlag(d, kFlagInitialized) == 0)
    {
        DungeonSystem::SetFlag(d, kFlagInitialized, 1);
        const entt::entity gate = DungeonSystem::SpawnMob(d, kGateNpc, kGatePos1.x, kGatePos1.y, kGatePos1.dir);
        if (gate != entt::null)
	DungeonSystem::SetUnique(d, "vk_gate_1", ecs::PlayerRuntime::GetPacketVID(gate));

        const entt::entity compass = DungeonSystem::SpawnMob(d, kCompassEmptyNpc, kCompassPos.x, kCompassPos.y, kCompassPos.dir);
        if (compass != entt::null)
	DungeonSystem::SetUnique(d, "vk_compass", ecs::PlayerRuntime::GetPacketVID(compass));

        NoticeMap(idx, "<Frostbane Fortress> Starting in 10 seconds. Get ready.");
        s_viking.ScheduleStart(idx);
    }

    if (ecs::QuestSystem::GetFlag(character, kQfDisconnect) > 0)
    {
        ecs::QuestSystem::SetFlag(character, kQfDisconnect, 0);
        ecs::ChatSystem::Send(character, CHAT_TYPE_BIG_NOTICE, "Welcome back.");

        const int32_t limit = DungeonSystem::GetFlag(d, kFlagTimeLimit);
        if (DungeonSystem::GetFlag(d, kFlagCompleted) != 0)
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

    SetCooldown(character);
}

bool CVikingDungeon::OnUseItem(entt::entity character, entt::entity item)
{
    if (!ecs::PlayerRuntime::IsValid(character) || !ItemSystem::CanConsumeOwnedItem(character, item))
        return false;

    if (ItemSystem::GetItemVnum(item) != kResetItemVnum)
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

    const ItemSystem::ItemCost cost {item, 1};
    if (!ItemSystem::ConsumeOwnedItemCosts(character, std::span(&cost, 1)))
        return true;
    ecs::QuestSystem::SetFlag(character, kQfCooldown, 0);
    ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Dungeon cooldown reset successfully.");
    return true;
}

bool CVikingDungeon::OnClickNpc(entt::entity character, entt::entity npc)
{
    if (!ecs::PlayerRuntime::IsValid(npc) || !ecs::PlayerRuntime::IsPC(character))
        return false;

    const uint32_t race = ecs::PlayerRuntime::GetRaceNum(npc);
    const int32_t now = get_global_time();

    const int32_t currentIdx = ecs::PlayerRuntime::GetMapIndex(character);
    const bool isInsideViking = IsVikingDungeonMap(currentIdx);
    const entt::entity currentDungeon = isInsideViking ? CDungeonManager::instance().FindByMapIndex(currentIdx)  : entt::null;
    const bool quickRestart = (race == kEntryNpcVnum && currentDungeon != entt::null && DungeonSystem::GetFlag(currentDungeon, kFlagCompleted) != 0);

    if (race == kRewardChestVnum)
    {
        const int32_t idx = ecs::PlayerRuntime::GetMapIndex(character);
        if (!IsVikingDungeonMap(idx))
            return false;

        const entt::entity d = CDungeonManager::instance().FindByMapIndex(idx);
        if (d == entt::null || DungeonSystem::GetFlag(d, kFlagCompleted) == 0)
            return false;

        char rewardFlag[64];
        snprintf(rewardFlag, sizeof(rewardFlag), "vk_reward_%u", ecs::PlayerRuntime::GetPlayerID(character));
        if (DungeonSystem::GetFlag(d, rewardFlag) != 0)
        {
            ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "You already took your reward.");
            return true;
        }

        DungeonSystem::SetFlag(d, rewardFlag, 1);
        ItemSystem::AutoGiveItemEcs(character, kRewardItemVnum, kRewardItemCount);
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
        const entt::entity d = CDungeonManager::instance().FindByMapIndex(rejoinIdx);
        if (d != entt::null && DungeonSystem::GetFlag(d, kFlagCompleted) == 0 && DungeonSystem::GetFlag(d, kFlagBlockRejoin) == 0)
        {
            ecs::MovementSystem::WarpSet(character, kEnterGlobalX * 100, kEnterGlobalY * 100, rejoinIdx);
            ecs::QuestSystem::SetFlag(character, kQfDisconnect, 0);
            return true;
        }
    }


    if (!ecs::PlayerRuntime::CanWarp(character))
    {
        ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "You have to wait a bit before entering.");
        return true;
    }

    if (quest::CQuestManager::instance().GetEventFlag("vikingdungeon_zone_block") == 1 && ecs::PlayerRuntime::GetGMLevel(character) == GM_PLAYER)
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

    const entt::entity party = ecs::SocialSystem::GetParty(character);
    if (party != entt::null)
    {
        if (PartySystem::GetLeaderPID(party) != ecs::PlayerRuntime::GetPlayerID(character))
        {
            ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Only the party leader can start the dungeon.");
            return true;
        }

        if (PartySystem::GetNearMemberCount(party) != PartySystem::GetMemberCount(party))
        {
            ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Every party member must be online and near the NPC.");
            return true;
        }

        if ((int32_t)PartySystem::GetMemberCount(party) < kMinMembers)
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
        if (!ecs::PlayerRuntime::IsPC(m) || !ok)
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
            if (ItemSystem::CountItem(m, kEntryItemVnum) < kEntryItemCount)
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

    if (party != entt::null)
        PartySystem::ForEachOnMapMember(party, checkMember, ecs::PlayerRuntime::GetMapIndex(character));
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

    const entt::entity d = CDungeonManager::instance().Create(kOriginalMap);
    if (d == entt::null)
    {
        ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Failed to create dungeon instance.");
        return true;
    }

    const int32_t dungeonMapIdx = DungeonSystem::GetMapIndex(d);

    auto prepareMember = [&](entt::entity m){
        if (!ecs::PlayerRuntime::IsPC(m))
            return;

        SetOutsideWarpLocation(m);
        ClearRejoinFlags(m);
        ecs::QuestSystem::SetFlag(m, kQfIdx, dungeonMapIdx);
        ecs::QuestSystem::SetFlag(m, kQfCh, (int32_t)g_bChannel);

        if (!quickRestart)
        {
            SetCooldown(m);
            ItemSystem::RemoveSpecifyItemEcs(m, kEntryItemVnum, kEntryItemCount);
        }
    };

    if (party != entt::null)
        PartySystem::ForEachOnMapMember(party, prepareMember, ecs::PlayerRuntime::GetMapIndex(character));
    else
        prepareMember(character);

    SetDungeonReady(d);

    if (party != entt::null)
        DungeonSystem::JoinParty_Coords(d, party, kEnterGlobalX, kEnterGlobalY, ecs::PlayerRuntime::GetMapIndex(character));
    else
        DungeonSystem::Join_Coords(d, character, kEnterGlobalX, kEnterGlobalY, ecs::PlayerRuntime::GetMapIndex(character));

    BigNoticeMap(dungeonMapIdx, "<Frostbane Fortress> Dungeon instance created.");
    return true;
}

bool CVikingDungeon::OnNpcTakeItem(entt::entity from, entt::entity npc, entt::entity item)
{
    if (!ecs::PlayerRuntime::IsValid(npc) || !ItemSystem::CanConsumeOwnedItem(from, item) || !ecs::PlayerRuntime::IsPC(from))
        return false;

    const int32_t idx = ecs::PlayerRuntime::GetMapIndex(from);
    if (!IsVikingDungeonMap(idx))
        return false;

    const entt::entity d = CDungeonManager::instance().FindByMapIndex(idx);
    if (d == entt::null)
        return false;

    const uint32_t npcVnum = ecs::PlayerRuntime::GetRaceNum(npc);
    const uint32_t itemVnum = ItemSystem::GetItemVnum(item);
    const int32_t floor = DungeonSystem::GetFlag(d, kFlagFloor);

    if (floor == 1 && itemVnum == kFloor1ItemVnum)
    {
        if (npcVnum == kCompassEmptyNpc && DungeonSystem::GetFlag(d, kFlagCompassState) == 0)
        {
            const ItemSystem::ItemCost cost {item, 1};
            if (!ItemSystem::ConsumeOwnedItemCosts(from, std::span(&cost, 1)))
                return true;
            DungeonSystem::SetFlag(d, kFlagCompassState, 1);
            ReplaceCompass(d, npc, kCompassSmallNpc);
            DungeonSystem::SpawnRegen(d, kRegen1FloorA, true);
            s_viking.ScheduleFloor1Check(idx);
            NoticeMap(idx, "<Frostbane Fortress> Compass activated. Clear the monsters again.");
            return true;
        }

        if (npcVnum == kCompassSmallNpc && DungeonSystem::GetFlag(d, kFlagCompassState) == 1)
        {
            const ItemSystem::ItemCost cost {item, 1};
            if (!ItemSystem::ConsumeOwnedItemCosts(from, std::span(&cost, 1)))
                return true;
            DungeonSystem::SetFlag(d, kFlagCompassState, 2);
            ReplaceCompass(d, npc, kCompassMediumNpc);
            DungeonSystem::SpawnRegen(d, kRegen1FloorA, true);
            s_viking.ScheduleFloor1Check(idx);
            NoticeMap(idx, "<Frostbane Fortress> Compass empowered further. Clear the monsters again.");
            return true;
        }

        if (npcVnum == kCompassMediumNpc && DungeonSystem::GetFlag(d, kFlagCompassState) == 2)
        {
            const ItemSystem::ItemCost cost {item, 1};
            if (!ItemSystem::ConsumeOwnedItemCosts(from, std::span(&cost, 1)))
                return true;
            DungeonSystem::SetFlag(d, kFlagCompassState, 3);
            ReplaceCompass(d, npc, kCompassLargeNpc);
            NoticeMap(idx, "<Frostbane Fortress> The compass was ignited successfully.");
            NoticeMap(idx, "<Frostbane Fortress> The first main boss will appear soon.");
            s_viking.ScheduleFloor1Boss(idx);
            return true;
        }
    }

    if (floor == 4 && itemVnum == kFloor3ItemVnum && DungeonSystem::GetFlag(d, kFlagCanUseRune) == 1)
    {
        if (npcVnum != kMemorialNpc1 && npcVnum != kMemorialNpc2 && npcVnum != kMemorialNpc3)
            return false;

        const ItemSystem::ItemCost cost {item, 1};

        if (!ItemSystem::ConsumeOwnedItemCosts(from, std::span(&cost, 1)))

            return true;
        DungeonSystem::SetFlag(d, kFlagCanUseRune, 0);

        int32_t stage = DungeonSystem::GetFlag(d, kFlagFloor3NpcStage) + 1;
        DungeonSystem::SetFlag(d, kFlagFloor3NpcStage, stage);

        CombatSystem::Dead(npc, entt::null, true);

        uint32_t newNpc = kMemorialNpc4;
        if (stage == 1)
            newNpc = kMemorialNpc2;
        else if (stage == 2)
            newNpc = kMemorialNpc3;
        else if (stage >= 3)
            newNpc = kMemorialNpc4;

        DungeonSystem::SetFlag(d, kFlagFloor3NpcVnum, newNpc);
        const entt::entity memorial = DungeonSystem::SpawnMob(d, newNpc, kMemorialPos.x, kMemorialPos.y, kMemorialPos.dir);
        if (memorial != entt::null)
		DungeonSystem::SetUnique(d, "vk_memorial", ecs::PlayerRuntime::GetPacketVID(memorial));

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
    if (!ecs::PlayerRuntime::IsValid(victim) || !ecs::PlayerRuntime::IsPC(killer))
        return;

    const int32_t idx = ecs::PlayerRuntime::GetMapIndex(killer);
    if (!IsVikingDungeonMap(idx))
        return;

    const entt::entity d = CDungeonManager::instance().FindByMapIndex(idx);
    if (d == entt::null)
        return;

    const uint32_t vnum = ecs::PlayerRuntime::GetRaceNum(victim);
    const int32_t floor = DungeonSystem::GetFlag(d, kFlagFloor);

    if (floor == 1 && vnum == kFloor1LowBossVnum)
    {
        ItemSystem::AutoGiveItemEcs(killer, kFloor1ItemVnum, 1);
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
        const int32_t remain = std::max(0, DungeonSystem::GetFlag(d, kFlagFloor2Remain) - 1);
        DungeonSystem::SetFlag(d, kFlagFloor2Remain, remain);
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
        const entt::entity boss = DungeonSystem::SpawnMob(d, kFloor3BossVnum, bossLocalX, bossLocalY, 0);
        if (boss != entt::null)
		DungeonSystem::SetUnique(d, "vk_floor3_boss", ecs::PlayerRuntime::GetPacketVID(boss));
        DungeonSystem::SetFlag(d, kFlagCanKillFloor3Boss, 1);
        NoticeMap(idx, "<Frostbane Fortress> The protecting boss appeared. Kill it to proceed.");
        return;
    }

    if (floor == 4 && vnum == kFloor3BossVnum)
    {
        if (DungeonSystem::GetFlag(d, kFlagCanKillFloor3Boss) == 1)
        {
            DungeonSystem::SetFlag(d, kFlagCanKillFloor3Boss, 0);
            DungeonSystem::SetFlag(d, kFlagCanUseRune, 1);
            ItemSystem::AutoGiveItemEcs(killer, kFloor3ItemVnum, 1);
            NoticeMap(idx, "<Frostbane Fortress> %s received the rune item. Use it on the memorial.", ecs::PlayerRuntime::GetName(killer).data());
        }
        return;
    }

    if (floor == 5 && vnum == kFinalBossVnum && DungeonSystem::GetFlag(d, kFlagCompleted) == 0)
    {
        const char* leaderName = ecs::PlayerRuntime::GetName(killer).data();

        const entt::entity killerParty = ecs::SocialSystem::GetParty(killer);

        if (killerParty != entt::null)
        {
            const entt::entity leader = PartySystem::GetLeader(killerParty);
            if (leader != entt::null)
                leaderName = ecs::PlayerRuntime::GetName(leader).data();
        }

        char notice[256];
        if (killerParty != entt::null)
            std::snprintf(notice, sizeof(notice), "%s es csoportja teljesitette a Fagyos dungeont!", leaderName);
        else
            std::snprintf(notice, sizeof(notice), "%s befejezte a Fagyos dungeont!", ecs::PlayerRuntime::GetName(killer).data());

        BroadcastNotice(notice);

        DungeonSystem::SetFlag(d, kFlagCompleted, 1);
        DungeonSystem::SetFlag(d, kFlagBlockRejoin, 1);
        s_viking.ScheduleComplete(idx);
        return;
    }
}
