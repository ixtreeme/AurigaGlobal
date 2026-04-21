#include "stdafx.h"

#include "OrcsDungeon.h"

#include <cmath>
#include <unordered_map>

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
#include "ecs/EventDispatcher.hpp"
#include "ecs/events.hpp"

#ifdef ENABLE_BATTLE_PASS
#include "battle_pass.h"
#endif

namespace
{
    // ---------- CONFIG ----------
    constexpr int32_t kOrcOriginalMap = 355;
    constexpr int32_t kPrivateMin = 3550000;
    constexpr int32_t kPrivateMax = 3560000;

    constexpr uint32_t kEntryNpcVnum = 9239;
    constexpr uint32_t kBossVnum = 693;
    constexpr uint32_t kSealMobVnum = 8009;
    constexpr uint32_t kBonusMobVnum = 6191; // Nemere

    constexpr int32_t kEnterX = 2186;
    constexpr int32_t kEnterY = 3489;

    constexpr int32_t kBossX = 138;
    constexpr int32_t kBossY = 885;

    constexpr int32_t kSeal1X = 125;
    constexpr int32_t kSeal1Y = 892;
    constexpr int32_t kSeal2X = 149;
    constexpr int32_t kSeal2Y = 892;
    constexpr int32_t kSeal3X = 125;
    constexpr int32_t kSeal3Y = 915;
    constexpr int32_t kSeal4X = 149;
    constexpr int32_t kSeal4Y = 915;

    constexpr int32_t kEndSeconds = 1799;      // 29:59
    constexpr int32_t kPrepareDelay = 1;
    constexpr int32_t kRejoinSeconds = 300;
    constexpr int32_t kCooldownSeconds = 900;

    constexpr int32_t kMinLevel = 35;
    constexpr int32_t kMaxLevel = 65;
    constexpr uint32_t kRequiredItem = 89106;
    constexpr uint32_t kRemoveAllItem = 89104; // removed in original quest (count 255)

    // Dungeon flags (stored in CDungeon)
    constexpr const char* kFlagFloor = "floor";
    constexpr const char* kFlagStep = "step";
    constexpr const char* kFlagBossVid = "boss";
    constexpr const char* kFlagWasCompleted = "was_completed";

    // Quest flag storage (per character) - reused without Lua.
    constexpr const char* kQfDisconnect = "dungeonorchi.disconnect";
    constexpr const char* kQfIdx = "dungeonorchi.idx";
    constexpr const char* kQfCh = "dungeonorchi.ch";
    constexpr const char* kQfEnterTime = "dungeonorchi.enter_time";
    constexpr const char* kQfCooldown = "dungeonorchi.cooldown";

    inline bool IsInRange(int32_t v, int32_t lo, int32_t hi) { return v >= lo && v < hi; }

    struct FForEachPC
    {
        std::function<void(LPCHARACTER)> fn;
        void operator()(LPENTITY ent)
        {
            if (!ent || !ent->IsType(ENTITY_CHARACTER))
                return;
            LPCHARACTER ch = static_cast<LPCHARACTER>(ent);
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

    struct FRemoveItems
    {
        uint32_t vnumReq;
        uint32_t vnumRemoveAll;
        bool ok;

        FRemoveItems(uint32_t req, uint32_t rmAll) : vnumReq(req), vnumRemoveAll(rmAll), ok(true) {}

        void operator()(LPCHARACTER ch)
        {
            if (!ch || !ch->IsPC())
                return;

            // must have required item
            if (ch->CountSpecifyItem(vnumReq) < 1)
            {
                ok = false;
                return;
            }
        }
    };
}

// ------------------ Event plumbing ------------------
namespace
{
    EVENTINFO(orc_dungeon_event_info)
    {
        int32_t mapIndex;
        orc_dungeon_event_info() : mapIndex(0) {}
    };
}

class COrcsDungeonImpl
{
public:
    // mapIndex -> events
    std::unordered_map<int32_t, LPEVENT> m_evPrepare;
    std::unordered_map<int32_t, LPEVENT> m_evEnd;

    void CancelEvent(std::unordered_map<int32_t, LPEVENT>& m, int32_t mapIndex)
    {
        auto it = m.find(mapIndex);
        if (it == m.end())
            return;
        event_cancel(&it->second);
        m.erase(it);
    }

    void CancelAll(int32_t mapIndex)
    {
        CancelEvent(m_evPrepare, mapIndex);
        CancelEvent(m_evEnd, mapIndex);
    }

    void ClearDungeon(int32_t mapIndex)
    {
        CancelAll(mapIndex);

        LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIndex);
        if (!d)
            return;

        d->SetFlag(kFlagWasCompleted, 1);
        d->KillAll();
        d->ClearRegen();
        d->ExitAllLobby(1);
    }

    void StartPrepare(int32_t mapIndex)
    {
        LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIndex);
        if (!d)
        {
            CancelAll(mapIndex);
            return;
        }

        d->SetFlag(kFlagStep, 4);

        d->SpawnMob(kSealMobVnum, kSeal1X, kSeal1Y);
        d->SpawnMob(kSealMobVnum, kSeal2X, kSeal2Y);
        d->SpawnMob(kSealMobVnum, kSeal3X, kSeal3Y);
        d->SpawnMob(kSealMobVnum, kSeal4X, kSeal4Y);

        LPCHARACTER boss = d->SpawnMob(kBossVnum, kBossX, kBossY);
        const uint32_t bossVid = boss ? boss->GetLegacyVID() : 0;
        d->SetFlag(kFlagBossVid, (int32_t)bossVid);

        bool ok = boss && boss->SetInvincible(true);

        if (!ok)
        {
            d->Notice(
#ifdef TEXTS_IMPROVEMENT
                1042,
#endif
                ""
#ifdef TEXTS_IMPROVEMENT
                , true
#endif
            );
            ClearDungeon(mapIndex);
            return;
        }

        d->Notice(
#ifdef TEXTS_IMPROVEMENT
            1128,
#endif
            "30"
#ifdef TEXTS_IMPROVEMENT
            , true
#endif
        );

        // schedule end
        ScheduleEnd(mapIndex, kEndSeconds);

        d->Notice(
#ifdef TEXTS_IMPROVEMENT
            1046,
#endif
            ""
#ifdef TEXTS_IMPROVEMENT
            , true
#endif
        );
    }

    void EndDungeon(int32_t mapIndex)
    {
        LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIndex);
        if (d)
        {
            d->Notice(
#ifdef TEXTS_IMPROVEMENT
                1040,
#endif
                ""
#ifdef TEXTS_IMPROVEMENT
                , true
#endif
            );
            d->Notice(
#ifdef TEXTS_IMPROVEMENT
                1041,
#endif
                ""
#ifdef TEXTS_IMPROVEMENT
                , true
#endif
            );
        }
        ClearDungeon(mapIndex);
    }

    void SchedulePrepare(int32_t mapIndex, int32_t seconds);
    void ScheduleEnd(int32_t mapIndex, int32_t seconds);
};

struct FCooldownCheck
{
    int32_t now;
    const char* qfCooldown;
    bool ok;
    const char* name;
    int32_t remain;

    FCooldownCheck(int32_t n, const char* qf) : now(n), qfCooldown(qf), ok(true), name(nullptr), remain(0) {}

    void operator()(LPCHARACTER ch)
    {
        if (!ch || !ch->IsPC())
            return;

        const int32_t until = ch->GetQuestFlag(qfCooldown);
        if (until > now && ok)
        {
            ok = false;
            name = ch->GetName();
            remain = until - now;
        }
    }
};

static COrcsDungeonImpl s_orc;

EVENTFUNC(orcs_dungeon_prepare_event)
{
    auto* info = dynamic_cast<orc_dungeon_event_info*>(event->info);
    if (!info)
        return 0;
    const int32_t mapIndex = info->mapIndex;
    s_orc.m_evPrepare.erase(mapIndex);
    s_orc.StartPrepare(mapIndex);
    g_dispatcher.trigger(ecs::EvDungeonPrepare { static_cast<uint32_t>(mapIndex) });
    return 0;
}

EVENTFUNC(orcs_dungeon_end_event)
{
    auto* info = dynamic_cast<orc_dungeon_event_info*>(event->info);
    if (!info)
        return 0;
    const int32_t mapIndex = info->mapIndex;
    s_orc.m_evEnd.erase(mapIndex);
    s_orc.EndDungeon(mapIndex);
    g_dispatcher.trigger(ecs::EvDungeonEnd { static_cast<uint32_t>(mapIndex) });
    return 0;
}

void COrcsDungeonImpl::SchedulePrepare(int32_t mapIndex, int32_t seconds)
{
    CancelEvent(m_evPrepare, mapIndex);
    auto* info = AllocEventInfo<orc_dungeon_event_info>();
    info->mapIndex = mapIndex;
    m_evPrepare[mapIndex] = event_create(orcs_dungeon_prepare_event, info, PASSES_PER_SEC(seconds));
}

void COrcsDungeonImpl::ScheduleEnd(int32_t mapIndex, int32_t seconds)
{
    CancelEvent(m_evEnd, mapIndex);
    auto* info = AllocEventInfo<orc_dungeon_event_info>();
    info->mapIndex = mapIndex;
    m_evEnd[mapIndex] = event_create(orcs_dungeon_end_event, info, PASSES_PER_SEC(seconds));
}

// ------------------ Public facade ------------------

COrcsDungeon& COrcsDungeon::instance()
{
    static COrcsDungeon inst;
    return inst;
}

bool COrcsDungeon::IsOrcDungeonMap(int32_t mapIndex) const
{
    return IsInRange(mapIndex, kPrivateMin, kPrivateMax);
}

void COrcsDungeon::OnPlayerDisconnect(CHARACTER* ch)
{
    if (!ch || !ch->IsPC())
        return;

    const int32_t idx = ch->GetMapIndex();
    if (!IsOrcDungeonMap(idx))
        return;

    ch->SetQuestFlag(kQfDisconnect, get_global_time() + kRejoinSeconds);
    ch->SetQuestFlag(kQfIdx, idx);
    ch->SetQuestFlag(kQfCh, (int32_t)g_bChannel);
}

void COrcsDungeon::OnPlayerLogin(CHARACTER* ch)
{
    if (!ch || !ch->IsPC())
        return;

    const int32_t idx = ch->GetMapIndex();

    // Safety: if someone logs into the original dungeon map, move to lobby.
    if (idx == kOrcOriginalMap)
    {
        ch->WarpSet(535400, 1428400);
        return;
    }

    if (!IsOrcDungeonMap(idx))
        return;

    // If they logged in inside the private dungeon, ensure we have a dungeon pointer.
    LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
    if (!d)
    {
        ch->WarpSet(535400, 1428400);
        return;
    }

    ch->SetDungeon(d);

    // If dungeon was never initialized (server restart mid-run), restart the flow.
    if (d->GetFlag(kFlagFloor) == 0)
    {
        d->SetFlag(kFlagFloor, 2);
        d->SetFlag(kFlagWasCompleted, 0);
        s_orc.SchedulePrepare(idx, 1);
    }
}

static void OrcDungeon_CompleteRankingForMap(int32_t dungeonMapIdx)
{
    const int32_t now = get_global_time();

    ForEachPcOnMap(dungeonMapIdx, [&](LPCHARACTER ch)
        {
            if (!ch)
                return;

            // mimic questlua_dungeon::d.complete (simplified)
            ch->SetRankPoints(16, ch->GetRankPoints(16) + 1);

#ifdef ENABLE_BATTLE_PASS
            {
                uint8_t battlepassid = ch->GetBattlePassId();
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

            const int32_t enter_time = ch->GetQuestFlag(kQfEnterTime);
            ch->SetQuestFlag(kQfEnterTime, 0);
            ch->SetQuestFlag(kQfCh, 0);
            ch->SetQuestFlag(kQfCooldown, now + kCooldownSeconds);

            int32_t elapsed = now - enter_time;
            if (elapsed < 0)
                elapsed = 0;

            int32_t damage = 0;
#ifdef __DUNGEON_INFO_SYSTEM__
            damage = ch->GetQuestDamage((int)kBossVnum);
            if (damage < 0)
                damage = 0;
#endif

            // dungeon_ranking update (same logic as questlua)
            const int32_t pid = ch->GetPlayerID();
            const int32_t dungeon_index = kOrcOriginalMap;

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
                LPDESC desc = ch->GetDesc();
                const uint32_t accId = desc ? desc->GetAccountTable().id : 0;
                DBManager::instance().DirectQuery(
                    "INSERT INTO dungeon_ranking (acc_id, pid, dungeon_index, completed, time, damage) VALUES ('%u', '%d', '%d', '%d', '%d', '%d')",
                    accId, pid, dungeon_index, 1, elapsed, damage);
            }
        });
}

void COrcsDungeon::OnMobKilled(CHARACTER* killer, CHARACTER* victim)
{
    if (!killer || !victim)
        return;
    if (!killer->IsPC())
        return;
    // 8009 is often CHAR_TYPE_STONE, not monster.
    if (!(victim->IsMonster() || victim->IsStone()))
        return;

    const int32_t idx = victim->GetMapIndex();
    if (!IsOrcDungeonMap(idx))
        return;

    LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
    if (!d)
        return;

    const uint32_t vnum = victim->GetRaceNum();
    const int32_t floorNum = d->GetFlag(kFlagFloor);

    if (vnum == kBossVnum)
    {
        if (floorNum == 2 && d->GetFlag(kFlagStep) == 0 && d->GetFlag(kFlagWasCompleted) == 0)
        {
            d->SetFlag(kFlagWasCompleted, 1);

            OrcDungeon_CompleteRankingForMap(idx);

#ifdef TEXTS_IMPROVEMENT
            // Global notice with existing text IDs
            if (killer->GetParty())
            {
                LPCHARACTER leader = killer->GetParty()->GetLeaderCharacter();
                BroadcastNoticeNew(CHAT_TYPE_NOTICE, 0, 0, 1272, "%s", leader ? leader->GetName() : killer->GetName());
            }
            else
            {
                BroadcastNoticeNew(CHAT_TYPE_NOTICE, 0, 0, 1273, "%s", killer->GetName());
            }
#else
            SendNotice("Orc Dungeon completed!");
#endif

            d->Notice(
#ifdef TEXTS_IMPROVEMENT
                1054,
#endif
                ""
#ifdef TEXTS_IMPROVEMENT
                , true
#endif
            );

            d->KillAll();
            d->ClearRegen();

            d->SpawnMob(kEntryNpcVnum, kBossX, kBossY);

            const int32_t bonus = 10 + quest::CQuestManager::instance().GetEventFlag("dungeon_bonus");
            if (number(1, 100) <= bonus)
            {
                d->SpawnMob(kBonusMobVnum, kBossX, kBossY);
                // mimic Lua syschat (send to all players in this dungeon map)
                ForEachPcOnMap(idx, [&](LPCHARACTER p)
                    {
                        if (p)
                            ecs::ChatSystem::Send(AIHelpers::EcsOf(p), CHAT_TYPE_INFO, "Dungeon bonus spawn: Nemere ");
                    });
            }
        }
        return;
    }

    if (vnum == kSealMobVnum)
    {
        if (floorNum != 2)
            return;

        int32_t s = d->GetFlag(kFlagStep) - 1;
        if (s < 0)
            s = 0;

        if (s == 0)
        {
            const uint32_t bossVid = (uint32_t)d->GetFlag(kFlagBossVid);
            LPCHARACTER boss = CHARACTER_MANAGER::instance().Find(bossVid);
            bool ok = boss && boss->SetInvincible(false);
            if (!ok)
            {
                d->Notice(
#ifdef TEXTS_IMPROVEMENT
                    1050,
#endif
                    ""
#ifdef TEXTS_IMPROVEMENT
                    , true
#endif
                );
                s_orc.ClearDungeon(idx);
            }
            else
            {
                d->Notice(
#ifdef TEXTS_IMPROVEMENT
                    1051,
#endif
                    ""
#ifdef TEXTS_IMPROVEMENT
                    , true
#endif
                );
            }
        }
        else
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", s);

            d->Notice(
#ifdef TEXTS_IMPROVEMENT
                1052,
#endif
                buf
#ifdef TEXTS_IMPROVEMENT
                , true
#endif
            );
            d->Notice(
#ifdef TEXTS_IMPROVEMENT
                1053,
#endif
                ""
#ifdef TEXTS_IMPROVEMENT
                , true
#endif
            );

            const float dmgMul = (float)std::floor((6.0f - (float)s) / 1.6f);
            const uint32_t bossVid = (uint32_t)d->GetFlag(kFlagBossVid);
            LPCHARACTER boss = CHARACTER_MANAGER::instance().Find(bossVid);
            if (boss)
            {
                boss->SetAttMul(dmgMul);
                boss->SetDamMul(dmgMul);
            }
        }

        d->SetFlag(kFlagStep, s);
    }
}

// NPC click entry/exit.
bool COrcsDungeon::OnClickNpc(CHARACTER* ch)
{
    if (!ch || !ch->IsPC())
        return false;

    if (!ch->CanWarp())
        return true;

    const int32_t mapIdx = ch->GetMapIndex();

    // If clicked inside dungeon:
    // - while run is active (was_completed=0): behave as EXIT.
    // - after completion (was_completed=1): allow starting a NEW run from the same NPC.
    if (IsOrcDungeonMap(mapIdx))
    {
        LPDUNGEON cur = CDungeonManager::instance().FindByMapIndex(mapIdx);
        if (cur && cur->GetFlag(kFlagWasCompleted) == 0)
        {
            ch->WarpSet(535400, 1428400);
            return true;
        }
        // completed -> continue below (fresh entrance flow)
    }

    const int32_t now = get_global_time();

    // Rejoin flow
    const int32_t rejoinUntil = ch->GetQuestFlag(kQfDisconnect);
    if (rejoinUntil > now)
    {
        const int32_t rejoinIdx = ch->GetQuestFlag(kQfIdx);
        const int32_t rejoinCh = ch->GetQuestFlag(kQfCh);

        if (rejoinIdx >= kPrivateMin && rejoinIdx < kPrivateMax)
        {
            if (rejoinCh != 0 && rejoinCh != (int32_t)g_bChannel)
            {
                ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "You were in Orc Dungeon on a different channel. Channel: %d", rejoinCh);
                return true;
            }

            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(rejoinIdx);
            if (d && d->GetFlag(kFlagWasCompleted) == 0)
            {
                ch->SaveExitLocation();
                ch->WarpSet(kEnterX * 100, kEnterY * 100, rejoinIdx);
                return true;
            }
        }
    }

    // Fresh entrance
    if (ch->GetLevel() < kMinLevel || ch->GetLevel() > kMaxLevel)
    {
        ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Orc Dungeon: level requirement is %d-%d.", kMinLevel, kMaxLevel);
        return true;
    }

    // Cooldown
    const int32_t cdUntil = ch->GetQuestFlag(kQfCooldown);
    if (cdUntil > now)
    {
        const int32_t remain = cdUntil - now;
        ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Orc Dungeon: you must wait %d seconds.", remain);
        return true;
    }

    LPPARTY party = ch->GetParty();
    if (party && party->GetLeaderPID() != ch->GetPlayerID())
    {
        ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Only the party leader can start Orc Dungeon.");
        return true;
    }

    // Party cooldown: everyone must be off cooldown (leader cannot bypass others)
    if (party)
    {
        FCooldownCheck f(now, kQfCooldown);
        ForEachPcOnMap(ch->GetMapIndex(), [&](LPCHARACTER m) {
            if (!m || !m->IsPC() || m->GetParty() != party)
                return;
            f(m);
        });
        if (!f.ok)
        {
            ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "%s is still on cooldown (%d seconds).", f.name ? f.name : "A party member", f.remain);
            return true;
        }
    }

    // Check level + entry item for everyone who will enter (same map as leader)
    if (!party)
    {
        if (ch->CountSpecifyItem(kRequiredItem) < 1)
        {
            ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Orc Dungeon: you don't have the entry item.");
            return true;
        }
    }
    else
    {
        bool ok = true;
        const char* badName = nullptr;
        int32_t badLevel = 0;
        bool missingItem = false;

        ForEachPcOnMap(ch->GetMapIndex(), [&](LPCHARACTER m) {
            if (!ok || !m || !m->IsPC() || m->GetParty() != party)
                return;

            if (m->GetLevel() < kMinLevel || m->GetLevel() > kMaxLevel)
            {
                ok = false;
                badName = m->GetName();
                badLevel = m->GetLevel();
                missingItem = false;
                return;
            }

            if (m->CountSpecifyItem(kRequiredItem) < 1)
            {
                ok = false;
                badName = m->GetName();
                badLevel = m->GetLevel();
                missingItem = true;
                return;
            }
        });

        if (!ok)
        {
            if (missingItem)
                ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "%s doesn't have the entry item.", badName ? badName : "A party member");
            else
                ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "%s has an invalid level (Lv%d). Required: %d-%d.", badName ? badName : "A party member", badLevel, kMinLevel, kMaxLevel);
            return true;
        }
    }

    LPDUNGEON d = CDungeonManager::instance().Create(kOrcOriginalMap);
    if (!d)
    {
        ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Orc Dungeon: failed to create the dungeon.");
        return true;
    }

    // Initialize dungeon flags
    d->SetFlag(kFlagFloor, 2);
    d->SetFlag(kFlagWasCompleted, 0);
    d->SetFlag(kFlagStep, 0);
    d->SetFlag(kFlagBossVid, 0);

    // Consume items + set per-player flags
    auto applyMember = [&](LPCHARACTER m)
        {
            if (!m || !m->IsPC())
                return;

            m->RemoveSpecifyItem(kRequiredItem, 1);
            const int32_t rmAll = m->CountSpecifyItem(kRemoveAllItem);
            if (rmAll > 0)
                m->RemoveSpecifyItem(kRemoveAllItem, rmAll);

            m->SetQuestFlag(kQfDisconnect, 0);
            m->SetQuestFlag(kQfIdx, d->GetMapIndex());
            m->SetQuestFlag(kQfCh, (int32_t)g_bChannel);
            m->SetQuestFlag(kQfEnterTime, now);
            // cooldown is set on completion, just like original quest.
        };

    if (!party)
    {
        applyMember(ch);
        d->Join_Coords(ch, kEnterX, kEnterY, kOrcOriginalMap);
    }
    else
    {
        ForEachPcOnMap(ch->GetMapIndex(), [&](LPCHARACTER m) {
            if (!m || !m->IsPC() || m->GetParty() != party)
                return;
            applyMember(m);
        });
        // IMPORTANT: the last parameter selects which map members are currently on.
        // If the party starts a new run from inside the completed instance, members are on the private map,
        // so we must pass the current map index (same as the one used above).
        d->JoinParty_Coords(party, kEnterX, kEnterY, ch->GetMapIndex());
    }

    // Prepare after 1 second
    s_orc.SchedulePrepare(d->GetMapIndex(), kPrepareDelay);
    return true;
}

