#include "stdafx.h"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/SocialSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "ecs/systems/QuestSystem.hpp"

#include "RuneDungeon.h"

#include <unordered_map>
#include <vector>
#include <algorithm>
#include <random>
#include <memory>

#include "char_interface.hpp"
#include "char_manager.h"
#include "config.h"
#include "desc.h"
#include "party.h"
#include "p2p.h"
#include "sectree_manager.h"
#include "dungeon.h"
#include "questmanager.h"
#include "cmd.h"
#include "db.h"
#include "log.h"
#include <common/tables.h>
#include "item.h"
#include "item_manager.h"
#include "ecs/systems/ItemSystem.hpp"
#include "ecs/Registry.hpp"
#include "ecs/EntityFactory.hpp"

#ifdef ENABLE_BATTLE_PASS
#include "battle_pass.h"
#endif

namespace
{
    // ---------- CONFIG ----------
    constexpr int32_t kRuneOriginalMap = 218;
    constexpr int32_t kPrivateMin = 2180000;
    constexpr int32_t kPrivateMax = 2190000;

    constexpr uint32_t kEntryNpcVnum = 20506;
    constexpr uint32_t kKeyNpcVnum = 20507;
    constexpr uint32_t kExitNpcVnum = 2092;

    // Floor coordinates (quest-style coords, NOT *100)
    constexpr int32_t kEnterFloor1X = 6245;
    constexpr int32_t kEnterFloor1Y = 14152;

    constexpr int32_t kEnterFloor2X = 6273;
    constexpr int32_t kEnterFloor2Y = 14468;

    constexpr int32_t kEnterFloor3X = 6732;
    constexpr int32_t kEnterFloor3Y = 14440;

    constexpr int32_t kEnterFloor4X = 6554;
    constexpr int32_t kEnterFloor4Y = 14212;

    constexpr int32_t kEnterFloor5X = 6955;
    constexpr int32_t kEnterFloor5Y = 14213;

    // VNUMs
    constexpr uint32_t kFloor1StoneVnum = 8202;

    constexpr uint32_t kBossFloor2 = 3997;
    constexpr uint32_t kBossFloor3 = 3998;
    constexpr uint32_t kBossFloor4 = 3996;

    constexpr uint32_t kBossFloor5_First = 4012;  // invincible until mobs die (type 4 -> 5)
    constexpr uint32_t kBossFloor5_Gate = 4013;  // invincible until 5 keys are offered (type 6 -> 7)
    constexpr uint32_t kBossFloor5_Final = 4011;  // step phases (type 8)

    constexpr uint32_t kKeyDropMobA = 4009;
    constexpr uint32_t kKeyDropMobB = 4010;

    // Type 1 mobs (drop fragments)
    constexpr uint32_t kType1Mobs[] = { 4003, 4004, 4005, 4006, 4007, 4008 };

    // Items
    constexpr uint32_t kRequiredItem = 89101; // entry ticket
    constexpr uint32_t kRemoveAllItem = 89107; // remove-all item (as in Lua)
    constexpr uint32_t kKeyFragment = 89102; // fragments (10 -> key)
    constexpr uint32_t kFloorKey = 89103; // completion key / offering key
    constexpr uint32_t kCooldownReset = 89100; // cooldown reset item

    // Timers (seconds)
    constexpr int32_t kPrepareDelay = 1;
    constexpr int32_t kWarpDelay = 10;
    constexpr int32_t kTotalTimeLimit = 3899;
    constexpr int32_t kStoneTimeLimit = 899;
    constexpr int32_t kFloorTimeLimit = 1199;
    constexpr int32_t kFinalFloorTimeLimit = 1799;

    constexpr int32_t kAntiSpamDelay = 1;
    constexpr int32_t kRejoinSeconds = 120;
    constexpr int32_t kCooldownSeconds = 10; // rune_zone.complete(..., 120, ...)

    constexpr int32_t kMinLevel = 120;
    constexpr int32_t kMaxLevel = 160;

    // Regen files
    constexpr const char* kRegenFloor1 = "data/dungeon/rune/regen1.txt";

    constexpr const char* kRegenFloor2_Type1 = "data/dungeon/rune/regen2_type1.txt";
    constexpr const char* kRegenFloor2_Type3a = "data/dungeon/rune/regen2_type3a.txt";
    constexpr const char* kRegenFloor2_Type3b = "data/dungeon/rune/regen2_type3b.txt";

    constexpr const char* kRegenFloor3_Type1 = "data/dungeon/rune/regen3_type1.txt";
    constexpr const char* kRegenFloor3_Type3a = "data/dungeon/rune/regen3_type3a.txt";
    constexpr const char* kRegenFloor3_Type3b = "data/dungeon/rune/regen3_type3b.txt";

    constexpr const char* kRegenFloor4_Type1 = "data/dungeon/rune/regen4_type1.txt";
    constexpr const char* kRegenFloor4_Type3a = "data/dungeon/rune/regen4_type3a.txt";
    constexpr const char* kRegenFloor4_Type3b = "data/dungeon/rune/regen4_type3b.txt";

    constexpr const char* kRegenFloor5 = "data/dungeon/rune/regen5.txt";
    constexpr const char* kRegenFloor6 = "data/dungeon/rune/regen6.txt";
    constexpr const char* kRegenFloor7 = "data/dungeon/rune/regen7.txt";

    // Dungeon flags
    constexpr const char* kFlagFloor = "floor";
    constexpr const char* kFlagType = "type";
    constexpr const char* kFlagStep = "step";
    constexpr const char* kFlagBossVid = "boss";
    constexpr const char* kFlagCount = "count";
    constexpr const char* kFlagOpened = "opened";
    constexpr const char* kFlagWasCompleted = "was_completed";
    constexpr const char* kFlagF1Unlocked = "f1_unlocked";

    // Quest flags
    constexpr const char* kQfCooldown = "rune_zone.cooldown";
    constexpr const char* kQfDisconnect = "rune_zone.disconnect";
    constexpr const char* kQfIdx = "rune_zone.idx";
    constexpr const char* kQfCh = "rune_zone.ch";
    constexpr const char* kQfEnterTime = "rune_zone.enter_time";

    struct SPos { int32_t x; int32_t y; };

    constexpr SPos kFloor1StonePos[6] = {
        {145,106}, {126,127}, {132,156}, {170,155}, {163,115}, {161,133}
    };

    constexpr SPos kBossPosFloor2 = { 146,377 };
    constexpr SPos kBossPosFloor3 = { 592,377 };
    constexpr SPos kBossPosFloor4 = { 414,121 };

    constexpr SPos kBossPosFloor5_First = { 794,107 };
    constexpr SPos kBossPosFloor5_Gate = { 794,107 };
    constexpr SPos kBossPosFloor5_Final = { 794,107 };
    constexpr SPos kExitNpcPos = { 794,107 };

    constexpr bool IsInRange(int32_t v, int32_t mn, int32_t mx)
    {
        return v >= mn && v < mx;
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

    bool SetVidInvincible(uint32_t vid, bool inv)
    {
        if (!vid)
            return false;
        LPCHARACTER ch = CHARACTER_MANAGER::instance().Find(vid);
        if (!ch)
            return false;
        return ch->SetInvincible(inv);
    }

    void RemoveAllItemOnMap(int32_t mapIndex, uint32_t vnum)
    {
        ForEachPcOnMap(mapIndex, [vnum](LPCHARACTER pc) {
            if (!pc)
                return;
            const int32_t cnt = pc->CountSpecifyItem(vnum);
            if (cnt > 0)
                pc->RemoveSpecifyItem(vnum, cnt);
            });
    }

    void DropItemOnGround(LPCHARACTER victim, LPCHARACTER owner, uint32_t vnum, uint32_t count)
    {
        if (!victim)
            return;

        LPITEM item = ITEM_MANAGER::instance().CreateItem(vnum, count);
        if (!item)
            return;

        PIXEL_POSITION pos;
        pos.x = ecs::PlayerRuntime::GetX(AIHelpers::EcsOf(victim)) + number(-200, 200);
        pos.y = ecs::PlayerRuntime::GetY(AIHelpers::EcsOf(victim)) + number(-200, 200);
        pos.z = victim->GetZ();

        item->AddToGround(ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(victim)), pos);
        item->StartDestroyEvent();

        if (owner)
            item->SetOwnership(owner, 60 * 3);
    }
    static void RuneDungeon_CompleteRankingForMap(int32_t dungeonMapIdx)
    {
        const int32_t now = get_global_time();

        ForEachPcOnMap(dungeonMapIdx, [&](LPCHARACTER ch)
            {
                if (!ch)
                    return;

                // Mimic questlua_dungeon::d.complete()
                ch->SetRankPoints(16, ch->GetRankPoints(16) + 1);

#ifdef ENABLE_BATTLE_PASS
                {
                    const uint8_t battlepassid = ch->GetBattlePassId();
                    if (battlepassid)
                    {
                        uint32_t id, count;
                        if (CBattlePass::instance().BattlePassMissionGetInfo(battlepassid, COMPLETE_DUNGEON, &id, &count))
                        {
                            if (id == 1 && ch->GetMissionProgress(COMPLETE_DUNGEON, battlepassid) < count)
                                ch->UpdateMissionProgress(COMPLETE_DUNGEON, battlepassid, 1, count);
                        }
                    }
                }
#endif

                const int32_t enter_time = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), kQfEnterTime);

                ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfEnterTime, 0);
                ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfDisconnect, 0);
                ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfIdx, 0);
                ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfCh, 0);
                ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfCooldown, now + kCooldownSeconds);

                int32_t elapsed = (enter_time > 0) ? (now - enter_time) : 0;
                if (elapsed < 0)
                    elapsed = 0;

                int32_t damage = 0;
#ifdef __DUNGEON_INFO_SYSTEM__
                damage = ch->GetQuestDamage((int)kBossFloor5_Final);
                if (damage < 0)
                    damage = 0;
#endif

                const int32_t pid = ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch));
                const int32_t dungeon_index = kRuneOriginalMap;

                std::unique_ptr<SQLMsg> msgcheck(DBManager::instance().DirectQuery(
                    "SELECT time, damage FROM dungeon_ranking WHERE pid=%u AND dungeon_index=%d",
                    pid, dungeon_index));

                if (msgcheck && msgcheck->Get() && msgcheck->Get()->uiNumRows > 0)
                {
                    MYSQL_ROW row = mysql_fetch_row(msgcheck->Get()->pSQLResult);
                    int32_t lasttime = 0, lastdamage = 0;
                    str_to_number(lasttime, row[0]);
                    str_to_number(lastdamage, row[1]);

                    lasttime = elapsed > lasttime ? lasttime : elapsed;
                    lastdamage = damage > lastdamage ? damage : lastdamage;

                    DBManager::instance().DirectQuery(
                        "UPDATE dungeon_ranking SET completed=completed+1, time=%d, damage=%d WHERE pid=%u AND dungeon_index=%d",
                        lasttime, lastdamage, pid, dungeon_index);
                }
                else
                {
                    LPDESC desc = ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch));
                    const uint32_t accId = desc ? desc->GetAccountTable().id : 0;
                    DBManager::instance().DirectQuery(
                        "INSERT INTO dungeon_ranking (acc_id, pid, dungeon_index, completed, time, damage) VALUES ('%u', '%d', '%d', '%d', '%d', '%d')",
                        accId, pid, dungeon_index, 1, elapsed, damage);
                }
            });
    }
}

// ---------------- Events ----------------

EVENTINFO(rune_dungeon_event_info)
{
    int32_t mapIndex;
    rune_dungeon_event_info() : mapIndex(0) {}
};

namespace
{
    // Forward declarations for event callbacks (must be in the SAME namespace
    // as the functions we define below, otherwise the linker looks for global
    // symbols and fails).
    EVENTFUNC(rune_prepare_event);
    EVENTFUNC(rune_end_event);
    EVENTFUNC(rune_step_limit_event);
    EVENTFUNC(rune_check_event);
    EVENTFUNC(rune_warp_event);

    class CRuneDungeonImpl
    {
    public:
        std::unordered_map<int32_t, LPEVENT> m_evPrepare;
        std::unordered_map<int32_t, LPEVENT> m_evEnd;
        std::unordered_map<int32_t, LPEVENT> m_evStepLimit;
        std::unordered_map<int32_t, LPEVENT> m_evCheck;
        std::unordered_map<int32_t, LPEVENT> m_evWarp;

        void CancelEvent(std::unordered_map<int32_t, LPEVENT>& map, int32_t idx)
        {
            auto it = map.find(idx);
            if (it == map.end())
                return;
            if (it->second)
                event_cancel(&it->second);
            map.erase(it);
        }

        void CancelAll(int32_t mapIndex)
        {
            CancelEvent(m_evPrepare, mapIndex);
            CancelEvent(m_evEnd, mapIndex);
            CancelEvent(m_evStepLimit, mapIndex);
            CancelEvent(m_evCheck, mapIndex);
            CancelEvent(m_evWarp, mapIndex);
        }

        void ClearDungeon(int32_t mapIndex)
        {
            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIndex);
            if (d)
            {
                d->SetFlag(kFlagWasCompleted, 1);
                d->KillAll();
                d->ClearRegen();
                d->ExitAllLobby(1);
            }
            CancelAll(mapIndex);
        }

        void SchedulePrepare(int32_t mapIndex, int32_t seconds)
        {
            CancelEvent(m_evPrepare, mapIndex);
            rune_dungeon_event_info* info = AllocEventInfo<rune_dungeon_event_info>();
            info->mapIndex = mapIndex;
            LPEVENT ev = event_create(rune_prepare_event, info, PASSES_PER_SEC(seconds));
            m_evPrepare[mapIndex] = ev;
        }

        void ScheduleEnd(int32_t mapIndex, int32_t seconds)
        {
            CancelEvent(m_evEnd, mapIndex);
            rune_dungeon_event_info* info = AllocEventInfo<rune_dungeon_event_info>();
            info->mapIndex = mapIndex;
            LPEVENT ev = event_create(rune_end_event, info, PASSES_PER_SEC(seconds));
            m_evEnd[mapIndex] = ev;
        }

        void ScheduleStepLimit(int32_t mapIndex, int32_t seconds)
        {
            CancelEvent(m_evStepLimit, mapIndex);
            rune_dungeon_event_info* info = AllocEventInfo<rune_dungeon_event_info>();
            info->mapIndex = mapIndex;
            LPEVENT ev = event_create(rune_step_limit_event, info, PASSES_PER_SEC(seconds));
            m_evStepLimit[mapIndex] = ev;
        }

        void ScheduleCheck(int32_t mapIndex, int32_t seconds)
        {
            CancelEvent(m_evCheck, mapIndex);
            rune_dungeon_event_info* info = AllocEventInfo<rune_dungeon_event_info>();
            info->mapIndex = mapIndex;
            LPEVENT ev = event_create(rune_check_event, info, PASSES_PER_SEC(seconds));
            m_evCheck[mapIndex] = ev;
        }

        void ScheduleWarp(int32_t mapIndex, int32_t seconds)
        {
            CancelEvent(m_evWarp, mapIndex);
            rune_dungeon_event_info* info = AllocEventInfo<rune_dungeon_event_info>();
            info->mapIndex = mapIndex;
            LPEVENT ev = event_create(rune_warp_event, info, PASSES_PER_SEC(seconds));
            m_evWarp[mapIndex] = ev;
        }

        // ---------------- Dungeon Logic ----------------

        void StartPrepare(int32_t mapIndex)
        {
            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIndex);
            if (!d)
            {
                CancelAll(mapIndex);
                return;
            }

            // Reset state
            d->SetFlag(kFlagWasCompleted, 0);
            d->SetFlag(kFlagFloor, 1);
            d->SetFlag(kFlagType, 0);
            d->SetFlag(kFlagStep, 0);
            d->SetFlag(kFlagBossVid, 0);
            d->SetFlag(kFlagCount, 0);
            d->SetFlag(kFlagOpened, 0);
            d->SetFlag(kFlagF1Unlocked, 0);

            for (int i = 1; i <= 6; ++i)
            {
                char vidFlag[32];
                char doneFlag[32];
                snprintf(vidFlag, sizeof(vidFlag), "unique_vid%d", i);
                snprintf(doneFlag, sizeof(doneFlag), "done_vid%d", i);
                d->SetFlag(vidFlag, 0);
                d->SetFlag(doneFlag, 0);
            }

            d->KillAll();
            d->ClearRegen();

            // Global end timer (Lua: rune_zone_end)
            ScheduleEnd(mapIndex, kTotalTimeLimit);

            // Spawn floor 1
            d->SpawnRegen(kRegenFloor1, true);

            // Shuffle stone positions to emulate table_shuffle
            std::vector<SPos> pos;
            pos.reserve(6);
            for (const auto& p : kFloor1StonePos)
                pos.push_back(p);

            std::random_device rd;
            std::mt19937 gen(rd());
            std::shuffle(pos.begin(), pos.end(), gen);

            for (int i = 0; i < 6; ++i)
            {
                LPCHARACTER stone = d->SpawnMob(kFloor1StoneVnum, pos[i].x, pos[i].y);
                if (!stone)
                {
                    d->Notice(948, "", true);
                    ClearDungeon(mapIndex);
                    return;
                }

                stone->SetInvincible(true);

                char vidFlag[32];
                snprintf(vidFlag, sizeof(vidFlag), "unique_vid%d", i + 1);
		d->SetFlag(vidFlag, (int32_t)ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(stone)));
            }

            d->SetFlag(kFlagCount, 0);

            ScheduleStepLimit(mapIndex, kStoneTimeLimit);
            ScheduleCheck(mapIndex, 2);

            d->Notice(949, "15", true);
            d->Notice(950, "", true);
            d->Notice(951, "", true);
        }

        void CreateRandomFloor(int32_t mapIndex)
        {
            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIndex);
            if (!d)
            {
                CancelAll(mapIndex);
                return;
            }

            // Clear per-floor timers
            CancelEvent(m_evStepLimit, mapIndex);
            CancelEvent(m_evCheck, mapIndex);
            CancelEvent(m_evWarp, mapIndex);

            // Clear current floor
            d->KillAll();
            d->ClearRegen();

            const int32_t curFloor = d->GetFlag(kFlagFloor);
            int32_t nextFloor = curFloor + 1;
            if (curFloor == 0)
                nextFloor = 1;

            if (curFloor == 1)
            {
                nextFloor = 2;
                d->Notice(979, "", true);
                d->Notice(980, "", true);
            }
            else if (curFloor == 2)
            {
                nextFloor = 3;
                d->Notice(952, "", true);
            }
            else if (curFloor == 3)
            {
                nextFloor = 4;
                d->Notice(953, "", true);
            }
            else if (curFloor == 4)
            {
                nextFloor = 5;
            }
            else
            {
                // No further floors
                return;
            }

            d->SetFlag(kFlagFloor, nextFloor);
            d->SetFlag(kFlagStep, 0);
            d->SetFlag(kFlagBossVid, 0);
            d->SetFlag(kFlagOpened, 0);

            // Floors 2-4: random type 1..3
            if (nextFloor >= 2 && nextFloor <= 4)
            {
                const int32_t type = number(1, 3);
                d->SetFlag(kFlagType, type);

                if (type == 1)
                {
                    const char* regen = (nextFloor == 2) ? kRegenFloor2_Type1 : (nextFloor == 3) ? kRegenFloor3_Type1 : kRegenFloor4_Type1;
                    d->SpawnRegen(regen, false); // set_regen_file
                }
                else if (type == 2)
                {
                    uint32_t bossVnum = (nextFloor == 2) ? kBossFloor2 : (nextFloor == 3) ? kBossFloor3 : kBossFloor4;
                    SPos bossPos = (nextFloor == 2) ? kBossPosFloor2 : (nextFloor == 3) ? kBossPosFloor3 : kBossPosFloor4;

                    LPCHARACTER boss = d->SpawnMob(bossVnum, bossPos.x, bossPos.y);
                    if (!boss)
                    {
                        d->Notice(981, "", true);
                        ClearDungeon(mapIndex);
                        return;
                    }
	d->SetFlag(kFlagBossVid, (int32_t)ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(boss)));
                    ScheduleCheck(mapIndex, 2);
                }
                else // type == 3
                {
                    const char* a = (nextFloor == 2) ? kRegenFloor2_Type3a : (nextFloor == 3) ? kRegenFloor3_Type3a : kRegenFloor4_Type3a;
                    const char* b = (nextFloor == 2) ? kRegenFloor2_Type3b : (nextFloor == 3) ? kRegenFloor3_Type3b : kRegenFloor4_Type3b;
                    d->SpawnRegen(a, true);
                    d->SpawnRegen(b, true);
                    ScheduleCheck(mapIndex, 2);
                }

                ScheduleStepLimit(mapIndex, kFloorTimeLimit);
                ScheduleWarp(mapIndex, kWarpDelay);
            }
            else if (nextFloor == 5)
            {
                // Floor 5 starts as type 4
                d->SetFlag(kFlagType, 4);

                LPCHARACTER boss = d->SpawnMob(kBossFloor5_First, kBossPosFloor5_First.x, kBossPosFloor5_First.y);
                if (!boss)
                {
                    d->Notice(982, "", true);
                    ClearDungeon(mapIndex);
                    return;
                }
                boss->SetInvincible(true);

	d->SetFlag(kFlagBossVid, (int32_t)ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(boss)));

                d->SpawnRegen(kRegenFloor5, true);

                ScheduleCheck(mapIndex, 2);
                ScheduleStepLimit(mapIndex, kFinalFloorTimeLimit);
                ScheduleWarp(mapIndex, kWarpDelay);
            }
        }

        void CheckFloor(int32_t mapIndex)
        {
            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIndex);
            if (!d)
            {
                CancelAll(mapIndex);
                return;
            }

            const int32_t floor = d->GetFlag(kFlagFloor);
            const int32_t type = d->GetFlag(kFlagType);

            if (floor == 1)
            {
                if (d->GetFlag(kFlagF1Unlocked) != 0)
                    return;

                if (d->CountMonster() == 6)
                {
                    const int32_t n = number(1, 6);
                    char vidFlag[32];
                    char doneFlag[32];
                    snprintf(vidFlag, sizeof(vidFlag), "unique_vid%d", n);
                    snprintf(doneFlag, sizeof(doneFlag), "done_vid%d", n);

                    const uint32_t vid = (uint32_t)d->GetFlag(vidFlag);
                    if (!SetVidInvincible(vid, false))
                    {
                        d->Notice(977, "", true);
                        ClearDungeon(mapIndex);
                        return;
                    }

                    d->SetFlag(doneFlag, 1);
                    d->SetFlag(kFlagF1Unlocked, 1);

                    // Lua clears rune_zone_check here
                    CancelEvent(m_evCheck, mapIndex);

                    d->Notice(975, "", true);
                    d->Notice(976, "", true);
                }
                return;
            }

            // Floors 2-4
            if (floor >= 2 && floor <= 4)
            {
                if (type == 2)
                {
                    const int32_t step = d->GetFlag(kFlagStep);
                    if ((step == 1 || step == 3) && d->CountMonster() == 1)
                    {
                        const uint32_t bossVid = (uint32_t)d->GetFlag(kFlagBossVid);
                        if (!SetVidInvincible(bossVid, false))
                        {
                            d->Notice(981, "", true);
                            ClearDungeon(mapIndex);
                            return;
                        }

                        d->SetFlag(kFlagStep, (step == 1) ? 2 : 4);
                        d->Notice(938, "", true);
                    }
                }
                else if (type == 3)
                {
                    if (d->CountMonster() == 0)
                    {
                        // Lua clears check timer after spawning boss
                        CancelEvent(m_evCheck, mapIndex);

                        uint32_t bossVnum = (floor == 2) ? kBossFloor2 : (floor == 3) ? kBossFloor3 : kBossFloor4;
                        SPos bossPos = (floor == 2) ? kBossPosFloor2 : (floor == 3) ? kBossPosFloor3 : kBossPosFloor4;

                        LPCHARACTER boss = d->SpawnMob(bossVnum, bossPos.x, bossPos.y);
                        if (!boss)
                        {
                            d->Notice(981, "", true);
                            ClearDungeon(mapIndex);
                            return;
                        }

                        d->Notice(939, "", true);
                        d->Notice(940, "", true);
                    }
                }

                return;
            }

            // Floor 5
            if (floor == 5)
            {
                if (type == 4)
                {
                    if (d->CountMonster() == 1)
                    {
                        CancelEvent(m_evCheck, mapIndex);

                        d->SetFlag(kFlagType, 5);

                        const uint32_t bossVid = (uint32_t)d->GetFlag(kFlagBossVid);
                        if (!SetVidInvincible(bossVid, false))
                        {
                            d->Notice(983, "", true);
                            ClearDungeon(mapIndex);
                            return;
                        }

                        d->Notice(938, "", true);
                    }
                }
                else if (type == 8)
                {
                    const int32_t step = d->GetFlag(kFlagStep);
                    if ((step == 1 || step == 3) && d->CountMonster() == 1)
                    {
                        const uint32_t bossVid = (uint32_t)d->GetFlag(kFlagBossVid);
                        if (!SetVidInvincible(bossVid, false))
                        {
                            d->Notice(983, "", true);
                            ClearDungeon(mapIndex);
                            return;
                        }

                        d->SetFlag(kFlagStep, (step == 1) ? 2 : 4);
                        d->Notice(938, "", true);
                    }
                }
            }
        }

        void WarpToNextFloor(int32_t mapIndex)
        {
            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIndex);
            if (!d)
                return;

            const int32_t floor = d->GetFlag(kFlagFloor);

            bool showTypeNotices = false;

            if (floor == 2)
            {
                d->JumpAll(mapIndex, kEnterFloor2X, kEnterFloor2Y);
                showTypeNotices = true;
            }
            else if (floor == 3)
            {
                d->JumpAll(mapIndex, kEnterFloor3X, kEnterFloor3Y);
                showTypeNotices = true;
            }
            else if (floor == 4)
            {
                d->JumpAll(mapIndex, kEnterFloor4X, kEnterFloor4Y);
                showTypeNotices = true;
            }
            else if (floor == 5)
            {
                d->JumpAll(mapIndex, kEnterFloor5X, kEnterFloor5Y);
                d->Notice(961, "30", true);
                d->Notice(962, "", true);
            }

            if (showTypeNotices)
            {
                const int32_t type = d->GetFlag(kFlagType);
                if (type == 1)
                {
                    d->Notice(957, "20", true);
                    d->Notice(958, "", true);
                }
                else if (type == 2)
                {
                    d->Notice(959, "20", true);
                }
                else if (type == 3)
                {
                    d->Notice(959, "20", true);
                    d->Notice(960, "", true);
                }
            }
        }

        void StepLimitExpired(int32_t mapIndex)
        {
            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIndex);
            if (d)
            {
                d->Notice(1040, "", true);
                d->Notice(1041, "", true);
            }
            ClearDungeon(mapIndex);
        }

        void EndDungeon(int32_t mapIndex)
        {
            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIndex);
            if (d)
            {
                d->Notice(1040, "", true);
                d->Notice(1041, "", true);
            }
            ClearDungeon(mapIndex);
        }
    };

    CRuneDungeonImpl s_rune;


    // Event callbacks (outside the class so s_rune is visible)
    EVENTFUNC(rune_prepare_event)
    {
        auto* info = (rune_dungeon_event_info*)event->info;
        if (!info)
            return 0;

        const int32_t mapIndex = info->mapIndex;
        s_rune.StartPrepare(mapIndex);
        s_rune.m_evPrepare.erase(mapIndex);
        return 0;
    }

    EVENTFUNC(rune_end_event)
    {
        auto* info = (rune_dungeon_event_info*)event->info;
        if (!info)
            return 0;

        const int32_t mapIndex = info->mapIndex;
        s_rune.EndDungeon(mapIndex);
        s_rune.m_evEnd.erase(mapIndex);
        return 0;
    }

    EVENTFUNC(rune_step_limit_event)
    {
        auto* info = (rune_dungeon_event_info*)event->info;
        if (!info)
            return 0;

        const int32_t mapIndex = info->mapIndex;
        s_rune.StepLimitExpired(mapIndex);
        s_rune.m_evStepLimit.erase(mapIndex);
        return 0;
    }

    EVENTFUNC(rune_check_event)
    {
        auto* info = (rune_dungeon_event_info*)event->info;
        if (!info)
            return 0;

        s_rune.CheckFloor(info->mapIndex);
        return PASSES_PER_SEC(2);
    }

    EVENTFUNC(rune_warp_event)
    {
        auto* info = (rune_dungeon_event_info*)event->info;
        if (!info)
            return 0;

        const int32_t mapIndex = info->mapIndex;
        s_rune.WarpToNextFloor(mapIndex);
        s_rune.m_evWarp.erase(mapIndex);
        return 0;
    }

}

// ---------------- CRuneDungeon public API ----------------

CRuneDungeon& CRuneDungeon::instance()
{
    static CRuneDungeon inst;
    return inst;
}

bool CRuneDungeon::IsRuneDungeonMap(int32_t mapIndex) const
{
    return IsInRange(mapIndex, kPrivateMin, kPrivateMax);
}

void CRuneDungeon::OnPlayerDisconnect(CHARACTER* ch)
{
    if (!ch || !ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(ch)))
        return;

    const int32_t idx = ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(ch));
    if (!IsRuneDungeonMap(idx))
        return;

    const int32_t now = get_global_time();
    ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfDisconnect, now + kRejoinSeconds);
}

void CRuneDungeon::OnPlayerLogin(CHARACTER* ch)
{
    if (!ch || !ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(ch)))
        return;

    const int32_t idx = ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(ch));

    // If someone logs in on the base map, kick them to default location.
    if (idx == kRuneOriginalMap)
    {
        ch->WarpSet(536900, 1331400);
        return;
    }

    if (!IsRuneDungeonMap(idx))
        return;

    // Set return location
    ch->SetWarpLocation(219, 5369, 14292);

    ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfIdx, idx);
    ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfCh, (int32_t)g_bChannel);

    LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
    if (!d)
        return;

    // Start only once, only by leader / solo.
    if (d->GetFlag(kFlagFloor) == 0)
    {
        bool isLeader = true;
        if (LPPARTY party = ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch)))
        {
            if (party->GetLeaderPID() != ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)))
                isLeader = false;
        }

        if (isLeader)
        {
            d->SetFlag(kFlagFloor, 1);
            d->SetFlag(kFlagWasCompleted, 0);
            s_rune.SchedulePrepare(idx, kPrepareDelay);
        }
    }
}

void CRuneDungeon::OnMobKilled(CHARACTER* killer, CHARACTER* victim)
{
    if (!killer || !victim)
        return;

    const int32_t idx = ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(victim));
    if (!IsRuneDungeonMap(idx))
        return;

    LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
    if (!d)
        return;

    const uint32_t vnum = ecs::PlayerRuntime::GetRaceNum(AIHelpers::EcsOf(victim));
    const int32_t floor = d->GetFlag(kFlagFloor);
    const int32_t type = d->GetFlag(kFlagType);

    // Floor 1: 6 invincible stones, unlock one at a time.
    if (floor == 1 && vnum == kFloor1StoneVnum)
    {
        // Mark killed stone
        for (int i = 1; i <= 6; ++i)
        {
            char vidFlag[32];
            char doneFlag[32];
            snprintf(vidFlag, sizeof(vidFlag), "unique_vid%d", i);
            snprintf(doneFlag, sizeof(doneFlag), "done_vid%d", i);

	if ((uint32_t)d->GetFlag(vidFlag) == ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(victim)))
            {
                d->SetFlag(doneFlag, 1);
                break;
            }
        }

        const int32_t c = d->GetFlag(kFlagCount) + 1;
        d->SetFlag(kFlagCount, c);

        if (c >= 6)
        {
            s_rune.CreateRandomFloor(idx);
            return;
        }

        // Unlock next not-yet-unlocked stone
        for (int i = 1; i <= 6; ++i)
        {
            char vidFlag[32];
            char doneFlag[32];
            snprintf(vidFlag, sizeof(vidFlag), "unique_vid%d", i);
            snprintf(doneFlag, sizeof(doneFlag), "done_vid%d", i);

            if (d->GetFlag(doneFlag) == 0)
            {
                const uint32_t vid = (uint32_t)d->GetFlag(vidFlag);
                if (!SetVidInvincible(vid, false))
                {
                    d->Notice(977, "", true);
                    s_rune.ClearDungeon(idx);
                    return;
                }
                d->SetFlag(doneFlag, 1);
                break;
            }
        }

        char buf[16];
        snprintf(buf, sizeof(buf), "%d", 6 - c);
        d->Notice(978, buf, true);
        return;
    }

    // Floors 2-4 Type 1: drop key fragments from 4003..4008 (5%)
    if (type == 1 && (floor == 2 || floor == 3 || floor == 4))
    {
        bool isType1Mob = false;
        for (uint32_t m : kType1Mobs)
        {
            if (vnum == m)
            {
                isType1Mob = true;
                break;
            }
        }

        if (isType1Mob && number(1, 100) <= 5)
        {
            LPPARTY party = ecs::SocialSystem::GetParty(AIHelpers::EcsOf(killer));
            if (!party)
            {
                if (killer->CountSpecifyItem(kKeyFragment) < 10 && killer->CountSpecifyItem(kFloorKey) < 1)
                    killer->AutoGiveItem(kKeyFragment, 1);
            }
            else
            {
                if (party->GetLeaderPID() == ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(killer)))
                {
                    if (killer->CountSpecifyItem(kKeyFragment) < 10 && killer->CountSpecifyItem(kFloorKey) < 1)
                        killer->AutoGiveItem(kKeyFragment, 1);
                }
            }
        }
    }

    // Floors 2-4: boss kill always advances (type 2 or type 3)
    if (floor >= 2 && floor <= 4)
    {
        if (vnum == kBossFloor2 || vnum == kBossFloor3 || vnum == kBossFloor4)
        {
            s_rune.CreateRandomFloor(idx);
            return;
        }
    }

    // Floor 5: first boss died (must be type 5)
    if (floor == 5 && vnum == kBossFloor5_First && type == 5)
    {
        d->SetFlag(kFlagType, 6);

        LPCHARACTER gate = d->SpawnMob(kBossFloor5_Gate, kBossPosFloor5_Gate.x, kBossPosFloor5_Gate.y);
        if (!gate)
        {
            d->Notice(983, "", true);
            s_rune.ClearDungeon(idx);
            return;
        }
        gate->SetInvincible(true);

	d->SetFlag(kFlagBossVid, (int32_t)ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(gate)));
        d->SetFlag(kFlagOpened, 0);

        d->SpawnRegen(kRegenFloor6, true);
        d->SpawnRegen(kRegenFloor7, false); // set_regen_file

        d->Notice(969, "", true);
        d->Notice(970, "", true);
        d->Notice(971, "", true);
        return;
    }

    // Floor 5 type 6: 4009/4010 drop floor key (3%)
    if (floor == 5 && type == 6 && (vnum == kKeyDropMobA || vnum == kKeyDropMobB))
    {
        if (number(1, 100) <= 3)
            DropItemOnGround(victim, killer, kFloorKey, 1);
        return;
    }

    // Floor 5 type 7: gate boss killed -> final boss stage (type 8)
    if (floor == 5 && vnum == kBossFloor5_Gate && type == 7)
    {
        d->SetFlag(kFlagStep, 0);
        d->SetFlag(kFlagType, 8);

        LPCHARACTER boss = d->SpawnMob(kBossFloor5_Final, kBossPosFloor5_Final.x, kBossPosFloor5_Final.y);
        if (!boss)
        {
            d->Notice(983, "", true);
            s_rune.ClearDungeon(idx);
            return;
        }

	d->SetFlag(kFlagBossVid, (int32_t)ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(boss)));
        s_rune.ScheduleCheck(idx, 2);

        d->Notice(965, "", true);
        d->Notice(966, "", true);
        return;
    }

    // Floor 5: final boss killed (type 8) -> completion
    if (floor == 5 && vnum == kBossFloor5_Final && type == 8)
    {
        if (d->GetFlag(kFlagWasCompleted) != 0)
            return;

        // Lua: clear_server_timer("rune_step_limit")
        s_rune.CancelEvent(s_rune.m_evStepLimit, idx);
        s_rune.CancelEvent(s_rune.m_evCheck, idx);

        d->SetFlag(kFlagWasCompleted, 1);

        RuneDungeon_CompleteRankingForMap(idx);

        // Notices
        d->Notice(963, "", true);

        // Clear remaining mobs and regen, spawn exit npc(s)
        d->KillAll();
        d->ClearRegen();
       // d->SpawnMob(kExitNpcVnum, kExitNpcPos.x, kExitNpcPos.y);

        // Bonus spawn chance (Lua: 10 + dungeon_bonus)
        const int32_t bonus = 10 + quest::CQuestManager::instance().GetEventFlag("dungeon_bonus");
        if (number(1, 100) <= bonus)
            d->SpawnMob(kExitNpcVnum, kExitNpcPos.x, kExitNpcPos.y);

        // Global broadcast
        if (LPPARTY party = ecs::SocialSystem::GetParty(AIHelpers::EcsOf(killer)))
        {
            const char* leaderName = ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(killer)).data();
            if (LPCHARACTER leader = party->GetLeaderCharacter())
                leaderName = ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(leader)).data();
            BroadcastNoticeNew(CHAT_TYPE_NOTICE, 0, 0, 1283, "%s", leaderName);
        }
        else
        {
            BroadcastNoticeNew(CHAT_TYPE_NOTICE, 0, 0, 1239, "%s", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(killer)).data());
        }

        return;
    }
}

bool CRuneDungeon::OnNpcTakeItem(CHARACTER* from, CHARACTER* npc, LPITEM item)
{
    if (!from || !ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(from)) || !npc || !item)
        return false;

    const int32_t idx = ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(from));
    if (!IsRuneDungeonMap(idx))
        return false;

    LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
    if (!d)
        return false;

    if (d->GetFlag(kFlagFloor) != 5 || d->GetFlag(kFlagType) != 6)
        return false;

    if (ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, item)) != kFloorKey)
        return false;

    // Consume the exact item that was given (Lua: item.remove())
    // NOTE: ReceiveItem() is triggered by dragging the item onto the NPC, so `item` is the actual stack being given.
    const entt::entity itemEntity = EntityFactory::CreateItemEntity(g_registry, item);
    if (ItemSystem::GetItemCount(itemEntity) > 1)
        ItemSystem::ConsumeItemEcs(itemEntity);
    else
        ItemSystem::DestroyItemEntityEcs(itemEntity, "RUNE_DUNGEON_TAKE");

    // Purge the NPC (Lua: npc.purge())
    M2_DESTROY_CHARACTER(npc);

    int32_t opened = d->GetFlag(kFlagOpened) + 1;
    d->SetFlag(kFlagOpened, opened);

    if (opened >= 5)
    {
        d->SetFlag(kFlagType, 7);

        // Remove all floor keys from everyone in the dungeon (Lua)
        RemoveAllItemOnMap(idx, kFloorKey);

        // Stop regen, make gate boss vulnerable
        d->ClearRegen();

        const uint32_t bossVid = (uint32_t)d->GetFlag(kFlagBossVid);
        if (!SetVidInvincible(bossVid, false))
        {
            d->Notice(983, "", true);
            s_rune.ClearDungeon(idx);
            return true;
        }

        d->Notice(967, "", true);
    }
    else
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", 5 - opened);
        d->Notice(968, buf, true);
    }

    return true;
}

bool CRuneDungeon::OnClickNpc(CHARACTER* ch)
{
    if (!ch || !ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(ch)))
        return false;

    // Rejoin flow
    const int32_t disconnectUntil = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), kQfDisconnect);
    const int32_t rejoinIdx = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), kQfIdx);
    const int32_t rejoinCh = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), kQfCh);

    const int32_t now = get_global_time();

    if (disconnectUntil > now && rejoinIdx > 0 && rejoinCh == (int32_t)g_bChannel)
    {
        if (IsRuneDungeonMap(rejoinIdx))
        {
            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(rejoinIdx);
            if (d && d->GetFlag(kFlagWasCompleted) == 0)
            {
                const int32_t floor = d->GetFlag(kFlagFloor);
                if (floor == 1)
                    ch->WarpSet(kEnterFloor1X * 100, kEnterFloor1Y * 100, rejoinIdx);
                else if (floor == 2)
                    ch->WarpSet(kEnterFloor2X * 100, kEnterFloor2Y * 100, rejoinIdx);
                else if (floor == 3)
                    ch->WarpSet(kEnterFloor3X * 100, kEnterFloor3Y * 100, rejoinIdx);
                else if (floor == 4)
                    ch->WarpSet(kEnterFloor4X * 100, kEnterFloor4Y * 100, rejoinIdx);
                else if (floor == 5)
                    ch->WarpSet(kEnterFloor5X * 100, kEnterFloor5Y * 100, rejoinIdx);

                ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfDisconnect, 0);
                return true;
            }
        }
    }

    // Anti spam (Lua: ww_218_%d)
    char flagName[64];
    snprintf(flagName, sizeof(flagName), "ww_218_%d", (int)g_bChannel);
    const int32_t antiSpamUntil = quest::CQuestManager::instance().GetEventFlag(flagName);
    if (antiSpamUntil > now)
    {
        ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Please wait a moment.");
        return false;
    }
    quest::CQuestManager::instance().SetEventFlag(flagName, now + kAntiSpamDelay);

    // Cooldown check
    const int32_t cooldownUntil = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), kQfCooldown);
    if (cooldownUntil > now)
    {
        ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Rune Dungeon is on cooldown.");
        return false;
    }

    // Level / entry item checks
    auto removeEntranceItems = [](LPCHARACTER pc) {
        if (!pc)
            return;

        // Entry item (1x)
        pc->RemoveSpecifyItem(kRequiredItem, 1);

        // Leftovers (Lua: d.remove_item / pc.remove_item)
        const int32_t c = pc->CountSpecifyItem(kRemoveAllItem);
        if (c > 0)
            pc->RemoveSpecifyItem(kRemoveAllItem, c);

        const int32_t f = pc->CountSpecifyItem(kKeyFragment);
        if (f > 0)
            pc->RemoveSpecifyItem(kKeyFragment, f);

        const int32_t k = pc->CountSpecifyItem(kFloorKey);
        if (k > 0)
            pc->RemoveSpecifyItem(kFloorKey, k);
        };

    LPPARTY party = ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch));

    // --- Validate party / solo requirements BEFORE creating the dungeon ---
    if (party)
    {
        // Only leader can start
        if (party->GetLeaderPID() != ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)))
        {
            ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Only the party leader can enter.");
            return false;
        }

        bool ok = true;
        const char* badName = nullptr;
        int32_t badLevel = 0;
        bool missingItem = false;

        // Check only players that will be pulled by JoinParty_Coords (same map as leader)
        ForEachPcOnMap(ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(ch)), [&](LPCHARACTER pc) {
            if (!ok || !pc || !ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(pc)))
                return;
            if (ecs::SocialSystem::GetParty(AIHelpers::EcsOf(pc)) != party)
                return;

            if (pc->GetLevel() < kMinLevel || pc->GetLevel() > kMaxLevel)
            {
                ok = false;
                badName = ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(pc)).data();
                badLevel = pc->GetLevel();
                missingItem = false;
                return;
            }

            if (pc->CountSpecifyItem(kRequiredItem) < 1)
            {
                ok = false;
                badName = ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(pc)).data();
                badLevel = pc->GetLevel();
                missingItem = true;
                return;
            }
            });

        if (!ok)
        {
            if (missingItem)
                ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "%s doesn't have the entry item.", badName ? badName : "A party member");
            else
                ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "%s has an invalid level (Lv%d).", badName ? badName : "A party member", badLevel);
            return false;
        }
    }
    else
    {
        // Solo checks
        if (ch->GetLevel() < kMinLevel || ch->GetLevel() > kMaxLevel)
        {
            ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Invalid level for Rune Dungeon.");
            return false;
        }

        if (ch->CountSpecifyItem(kRequiredItem) < 1)
        {
            ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "You need the entry item.");
            return false;
        }
    }

    // Create dungeon instance
    LPDUNGEON d = CDungeonManager::instance().Create(kRuneOriginalMap);
    if (!d)
        return false;

    d->SetFlag(kFlagFloor, 0);
    d->SetFlag(kFlagWasCompleted, 0);

    const int32_t dungeonMapIdx = d->GetMapIndex();

    auto setupMember = [&](LPCHARACTER pc)
        {
            if (!pc || !ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(pc)))
                return;

            removeEntranceItems(pc);

            ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(pc), kQfDisconnect, 0);
            ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(pc), kQfIdx, dungeonMapIdx);
            ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(pc), kQfCh, (int32_t)g_bChannel);
            ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(pc), kQfEnterTime, now);

            // Same return location as Lua.
            pc->SetWarpLocation(219, 5369, 14292);
        };

    // Consume entry items + clear leftovers BEFORE warping, and set rejoin/ranking timers
    if (party)
    {
        ForEachPcOnMap(ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(ch)), [&](LPCHARACTER pc) {
            if (!pc || !ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(pc)) || ecs::SocialSystem::GetParty(AIHelpers::EcsOf(pc)) != party)
                return;
            setupMember(pc);
            });

        d->JoinParty_Coords(party, kEnterFloor1X, kEnterFloor1Y, ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(ch)));
    }
    else
    {
        setupMember(ch);
        d->Join_Coords(ch, kEnterFloor1X, kEnterFloor1Y, ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(ch)));
    }

    return true;
}

bool CRuneDungeon::OnUseItem89103(CHARACTER* ch)
{
    if (!ch || !ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(ch)))
        return false;

    const int32_t idx = ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(ch));
    if (!IsRuneDungeonMap(idx))
        return false;

    LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
    if (!d)
        return false;

    const int32_t floor = d->GetFlag(kFlagFloor);
    if (floor != 2 && floor != 3 && floor != 4)
        return false;

    if (d->GetFlag(kFlagType) != 1)
        return false;

    // Only leader (or solo) can progress
    if (LPPARTY party = ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch)))
    {
        if (party->GetLeaderPID() != ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)))
            return false;
    }

    ch->RemoveSpecifyItem(kFloorKey, 1);
    s_rune.CreateRandomFloor(idx);
    return true;
}

bool CRuneDungeon::OnUseItem89102(CHARACTER* ch)
{
    if (!ch || !ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(ch)))
        return false;

    const int32_t idx = ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(ch));
    if (!IsRuneDungeonMap(idx))
        return false;

    LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
    if (!d)
        return false;

    const int32_t floor = d->GetFlag(kFlagFloor);
    if (floor != 2 && floor != 3 && floor != 4)
        return false;

    if (d->GetFlag(kFlagType) != 1)
        return false;

    if (ch->CountSpecifyItem(kKeyFragment) < 10)
        return false;

    if (LPPARTY party = ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch)))
    {
        if (party->GetLeaderPID() != ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)))
            return false;
    }

    ch->RemoveSpecifyItem(kKeyFragment, 10);
    ch->AutoGiveItem(kFloorKey, 1);
    return true;
}

bool CRuneDungeon::OnUseItem89100(CHARACTER* ch)
{
    if (!ch || !ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(ch)))
        return false;

    const int32_t now = get_global_time();
    const int32_t cooldownUntil = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), kQfCooldown);

    if (cooldownUntil <= now)
        return false;

    ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfDisconnect, 0);
    ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfIdx, 0);
    ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfCh, 0);
    ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfCooldown, 0);
    ch->RemoveSpecifyItem(kCooldownReset, 1);

    return true;
}

