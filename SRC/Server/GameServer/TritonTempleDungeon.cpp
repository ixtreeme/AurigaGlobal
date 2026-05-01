#include "stdafx.h"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/SocialSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "ecs/systems/QuestSystem.hpp"

#include "TritonTempleDungeon.h"

#ifdef ENABLE_CPP_DUNGEON_RAZOR93

#include <cmath>
#include <unordered_map>
#include <functional>
#include <memory>

#include "config.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "desc.h"
#include "party.h"
#include "p2p.h"
#include "sectree_manager.h"
#include "dungeon.h"
#include "questmanager.h"
#include "ecs/EventDispatcher.hpp"
#include "ecs/events.hpp"
#include "cmd.h"
#include "db.h"
#include "log.h"

#ifdef ENABLE_BATTLE_PASS
#include "battle_pass.h"
#endif

namespace
{
    // ---------- CONFIG ----------
    constexpr int32_t kTritonOriginalMap = 369;
    constexpr int32_t kPrivateMin = 3690000;
    constexpr int32_t kPrivateMax = 3700000;

    constexpr uint32_t kEntryNpcVnum = 20094;
    constexpr uint32_t kBossVnum = 3491;
    constexpr uint32_t kSealMobVnum = 8054;
    constexpr uint32_t kBonusMobVnum = 2493;

    // join cords (d.join_cords(369, 601, 2758))
    constexpr int32_t kEnterX = 601;
    constexpr int32_t kEnterY = 2758;

    // rejoin warp (pc.warp(60100, 275800, idx))
    constexpr int32_t kRejoinWarpX = 60100;
    constexpr int32_t kRejoinWarpY = 275800;

    // completion spawn
    constexpr int32_t kCompleteNpcX = 88;
    constexpr int32_t kCompleteNpcY = 116;

    // boss spawn
    constexpr int32_t kBossX = 90;
    constexpr int32_t kBossY = 116;

    constexpr int32_t kEndSeconds = 1799;      // 29:59
    constexpr int32_t kPrepareDelay = 1;
    constexpr int32_t kRejoinSeconds = 300;
    constexpr int32_t kCooldownSeconds = 900;

    constexpr int32_t kMinLevel = 60;
    constexpr int32_t kMaxLevel = 75;
    constexpr uint32_t kRequiredItem = 76019;
    constexpr uint32_t kRemoveAllItem = 89104;

    // Dungeon flags (stored in CDungeon)
    constexpr const char* kFlagFloor = "floor";
    constexpr const char* kFlagStep = "step";
    constexpr const char* kFlagBossVid = "boss";
    constexpr const char* kFlagWasCompleted = "was_completed";

    // Quest flag storage (per character)
    constexpr const char* kQfDisconnect = "tritontemple_razor93.disconnect";
    constexpr const char* kQfIdx = "tritontemple_razor93.idx";
    constexpr const char* kQfCh = "tritontemple_razor93.ch";
    constexpr const char* kQfEnterTime = "tritontemple_razor93.enter_time";
    constexpr const char* kQfCooldown = "tritontemple_razor93.cooldown";

    inline bool IsInRange(int32_t v, int32_t lo, int32_t hi) { return v >= lo && v < hi; }

    struct FForEachPC
    {
        std::function<void(LPCHARACTER)> fn;
        void operator()(LPENTITY ent)
        {
            if (!ent || !ent->IsType(ENTITY_CHARACTER))
                return;
            LPCHARACTER ch = static_cast<LPCHARACTER>(ent);
            if (ch && ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(ch)))
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

    struct FRequireItem
    {
        uint32_t vnumReq;
        bool ok;

        explicit FRequireItem(uint32_t req) : vnumReq(req), ok(true) {}

        void operator()(LPCHARACTER ch)
        {
            if (!ch || !ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(ch)))
                return;
            if (ch->CountSpecifyItem(vnumReq) < 1)
                ok = false;
        }
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
            if (!ch || !ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(ch)))
                return;

            const int32_t until = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), qfCooldown);
            if (until > now && ok)
            {
                ok = false;
                name = ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data();
                remain = until - now;
            }
        }
    };

    static void DungeonCompleteForMap(int32_t dungeonMapIdx, int32_t dungeonIndex, uint32_t bossVnum, int32_t cooldownSeconds,
        const char* qfEnterTime, const char* qfCh, const char* qfCooldown)
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

                const int32_t enter_time = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), qfEnterTime);
                ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), qfEnterTime, 0);
                ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), qfCh, 0);
                ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), qfCooldown, now + cooldownSeconds);

                int32_t elapsed = now - enter_time;
                if (elapsed < 0)
                    elapsed = 0;

                int32_t damage = 0;
#ifdef __DUNGEON_INFO_SYSTEM__
                damage = ch->GetQuestDamage((int)bossVnum);
                if (damage < 0)
                    damage = 0;
#endif

                // dungeon_ranking update (same logic as our OrcsDungeon C++ version)
                const int32_t pid = ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch));

                std::unique_ptr<SQLMsg> msgcheck(DBManager::instance().DirectQuery(
                    "SELECT time, damage FROM dungeon_ranking WHERE pid=%u AND dungeon_index=%d",
                    pid, dungeonIndex));

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
                        lasttime, lastdamage, pid, dungeonIndex);
                }
                else
                {
                    LPDESC desc = ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch));
                    const uint32_t accId = desc ? desc->GetAccountTable().id : 0;
                    DBManager::instance().DirectQuery(
                        "INSERT INTO dungeon_ranking (acc_id, pid, dungeon_index, completed, time, damage) VALUES ('%u', '%d', '%d', '%d', '%d', '%d')",
                        accId, pid, dungeonIndex, 1, elapsed, damage);
                }
            });
    }
}

// ------------------ Event plumbing ------------------

namespace
{
    EVENTINFO(triton_temple_event_info)
    {
        int32_t mapIndex;
        triton_temple_event_info() : mapIndex(0) {}
    };
}

class CTritonTempleDungeonImpl
{
public:
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

        // as quest: d.setf(step, 35)
        d->SetFlag(kFlagStep, 35);

        // spawn seals (exact coords from quest)
        const int seals[][2] = {
            {46,180},{56,180},{66,180},{76,180},{86,180},{96,180},{106,180},{116,180},{126,180},
            {128,170},{128,160},{128,150},{128,140},{128,130},{128,120},{128,110},{128,100},{128,90},
            {124,82},{116,74},{106,74},{96,74},{76,74},{66,74},{58,82},{50,90},
            {54,86},{50,100},{50,110},{50,120},{50,130},{50,140},{50,150},{50,160},{50,170}
        };

        for (const auto& p : seals)
            d->SpawnMob(kSealMobVnum, p[0], p[1]);

        LPCHARACTER boss = d->SpawnMob(kBossVnum, kBossX, kBossY);

        const uint32_t bossVid = boss ? boss->GetLegacyVID() : 0;
        d->SetFlag(kFlagBossVid, (int32_t)bossVid);

        const bool ok = boss && boss->SetInvincible(true);


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

static CTritonTempleDungeonImpl s_triton;

EVENTFUNC(triton_temple_prepare_event)
{
    auto* info = dynamic_cast<triton_temple_event_info*>(event->info);
    if (!info)
        return 0;
    const int32_t mapIndex = info->mapIndex;
    s_triton.m_evPrepare.erase(mapIndex);
    s_triton.StartPrepare(mapIndex);
    g_dispatcher.trigger(ecs::EvDungeonPrepare { static_cast<uint32_t>(mapIndex) });
    return 0;
}

EVENTFUNC(triton_temple_end_event)
{
    auto* info = dynamic_cast<triton_temple_event_info*>(event->info);
    if (!info)
        return 0;
    const int32_t mapIndex = info->mapIndex;
    s_triton.m_evEnd.erase(mapIndex);
    s_triton.EndDungeon(mapIndex);
    g_dispatcher.trigger(ecs::EvDungeonEnd { static_cast<uint32_t>(mapIndex) });
    return 0;
}

void CTritonTempleDungeonImpl::SchedulePrepare(int32_t mapIndex, int32_t seconds)
{
    CancelEvent(m_evPrepare, mapIndex);
    auto* info = AllocEventInfo<triton_temple_event_info>();
    info->mapIndex = mapIndex;
    m_evPrepare[mapIndex] = event_create(triton_temple_prepare_event, info, PASSES_PER_SEC(seconds));
}

void CTritonTempleDungeonImpl::ScheduleEnd(int32_t mapIndex, int32_t seconds)
{
    CancelEvent(m_evEnd, mapIndex);
    auto* info = AllocEventInfo<triton_temple_event_info>();
    info->mapIndex = mapIndex;
    m_evEnd[mapIndex] = event_create(triton_temple_end_event, info, PASSES_PER_SEC(seconds));
}

// ------------------ Public facade ------------------

CTritonTempleDungeon& CTritonTempleDungeon::instance()
{
    static CTritonTempleDungeon inst;
    return inst;
}

bool CTritonTempleDungeon::IsTritonTempleMap(int32_t mapIndex) const
{
    return IsInRange(mapIndex, kPrivateMin, kPrivateMax);
}

void CTritonTempleDungeon::OnPlayerDisconnect(CHARACTER* ch)
{
    if (!ch || !ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(ch)))
        return;

    const int32_t idx = ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(ch));
    if (!IsTritonTempleMap(idx))
        return;

    ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfDisconnect, get_global_time() + kRejoinSeconds);
    ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfIdx, idx);
    ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfCh, (int32_t)g_bChannel);
}

void CTritonTempleDungeon::OnPlayerLogin(CHARACTER* ch)
{
    if (!ch || !ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(ch)))
        return;

    const int32_t idx = ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(ch));
    if (!IsTritonTempleMap(idx))
        return;

    LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
    if (!d)
        return;

    ch->SetDungeon(d);
    ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfIdx, idx);
    ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfCh, (int32_t)g_bChannel);

    // Initialize if never started (after server restart)
    if (d->GetFlag(kFlagFloor) == 0)
    {
        d->SetFlag(kFlagFloor, 2);
        d->SetFlag(kFlagWasCompleted, 0);
        s_triton.SchedulePrepare(idx, 1);
    }
}

void CTritonTempleDungeon::OnMobKilled(CHARACTER* killer, CHARACTER* victim)
{
    if (!killer || !victim)
        return;
    if (!ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(killer)))
        return;

    // 8054 may be STONE on some cores
    if (!(victim->IsMonster() || ecs::PlayerRuntime::IsStone(AIHelpers::EcsOf(victim))))
        return;

    const int32_t idx = ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(victim));
    if (!IsTritonTempleMap(idx))
        return;

    LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
    if (!d)
        return;

    const uint32_t vnum = ecs::PlayerRuntime::GetRaceNum(AIHelpers::EcsOf(victim));
    const int32_t floor = d->GetFlag(kFlagFloor);

    if (vnum == kBossVnum)
    {
        if (floor == 2 && d->GetFlag(kFlagStep) == 0 && d->GetFlag(kFlagWasCompleted) == 0)
        {
            d->SetFlag(kFlagWasCompleted, 1);

            DungeonCompleteForMap(idx, kTritonOriginalMap, kBossVnum, kCooldownSeconds, kQfEnterTime, kQfCh, kQfCooldown);

#ifdef TEXTS_IMPROVEMENT
            if (ecs::SocialSystem::GetParty(AIHelpers::EcsOf(killer)))
            {
                LPCHARACTER leader = ecs::SocialSystem::GetParty(AIHelpers::EcsOf(killer))->GetLeaderCharacter();
                BroadcastNoticeNew(CHAT_TYPE_NOTICE, 0, 0, 2173, "%s", leader ? ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(leader)).data() : ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(killer)).data());
            }
            else
            {
                BroadcastNoticeNew(CHAT_TYPE_NOTICE, 0, 0, 2174, "%s", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(killer)).data());
            }
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

            d->SpawnMob(kEntryNpcVnum, kCompleteNpcX, kCompleteNpcY);

            const int32_t bonus = 10 + quest::CQuestManager::instance().GetEventFlag("dungeon_bonus");
            if (number(1, 100) <= bonus)
            {
                d->SpawnMob(kBonusMobVnum, kCompleteNpcX, kCompleteNpcY);
            }
        }
        return;
    }

    if (vnum == kSealMobVnum)
    {
        if (floor != 2)
            return;

        int32_t s = d->GetFlag(kFlagStep) - 1;
        if (s < 0)
            s = 0;

        if (s == 0)
        {
            const uint32_t bossVid = (uint32_t)d->GetFlag(kFlagBossVid);
            LPCHARACTER boss = CHARACTER_MANAGER::instance().Find(bossVid);
            const bool ok = boss && boss->SetInvincible(false);

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
                s_triton.ClearDungeon(idx);
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

            // quest formula: floor((6 - s) / 1.6)
            float mul = std::floor((6.0f - (float)s) / 1.6f);
            // prevent negative/0 multipliers (would result in weird/healing damage)
            if (mul < 1.0f)
                mul = 1.0f;

            const uint32_t bossVid = (uint32_t)d->GetFlag(kFlagBossVid);
            LPCHARACTER boss = CHARACTER_MANAGER::instance().Find(bossVid);
            if (boss)
            {
                boss->SetAttMul(mul);
                boss->SetDamMul(mul);
            }
        }

        d->SetFlag(kFlagStep, s);
    }
}
bool CTritonTempleDungeon::OnClickNpc(CHARACTER* ch)
{
    if (!ch || !ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(ch)))
        return false;

    if (!ch->CanWarp())
        return true;

    const int32_t mapIdx = ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(ch));

    // If clicked inside dungeon:
    // - while run active (was_completed=0): behave as EXIT.
    // - after completion (was_completed=1): allow starting a NEW run from the same NPC.
    if (IsTritonTempleMap(mapIdx))
    {
        LPDUNGEON cur = CDungeonManager::instance().FindByMapIndex(mapIdx);
        if (cur && cur->GetFlag(kFlagWasCompleted) == 0)
        {
            ch->WarpSet(535400, 1428400);
            return true;
        }
        // completed -> continue below to fresh entrance flow
    }

    const int32_t now = get_global_time();

    // Rejoin flow
    const int32_t rejoinUntil = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), kQfDisconnect);
    if (rejoinUntil > now)
    {
        const int32_t rejoinIdx = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), kQfIdx);
        const int32_t rejoinCh = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), kQfCh);

        if (IsInRange(rejoinIdx, kPrivateMin, kPrivateMax))
        {
            if (rejoinCh != 0 && rejoinCh != (int32_t)g_bChannel)
            {
                ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "You were in Triton Temple on a different channel. Channel: %d", rejoinCh);
                return true;
            }

            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(rejoinIdx);
            if (d && d->GetFlag(kFlagWasCompleted) == 0 && d->GetFlag(kFlagFloor) == 2)
            {
                ch->SaveExitLocation();
                ch->WarpSet(kRejoinWarpX, kRejoinWarpY, rejoinIdx);
                return true;
            }
        }
    }

    // Fresh entrance
    if (ch->GetLevel() < kMinLevel || ch->GetLevel() > kMaxLevel)
    {
        ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Triton Temple: level requirement is %d-%d.", kMinLevel, kMaxLevel);
        return true;
    }

    // Cooldown
    const int32_t cdUntil = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), kQfCooldown);
    if (cdUntil > now)
    {
        const int32_t remain = cdUntil - now;
        ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Triton Temple: you must wait %d seconds.", remain);
        return true;
    }

    LPPARTY party = ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch));
    if (party && party->GetLeaderPID() != ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)))
    {
        ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Only the party leader can start Triton Temple.");
        return true;
    }

    // Party cooldown: everyone must be off cooldown (leader cannot bypass others)
    if (party)
    {
        FCooldownCheck f(now, kQfCooldown);
        ForEachPcOnMap(ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(ch)), [&](LPCHARACTER m) {
            if (!m || !ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(m)) || ecs::SocialSystem::GetParty(AIHelpers::EcsOf(m)) != party)
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
            ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Triton Temple: you don't have the entry item.");
            return true;
        }
    }
    else
    {
        bool ok = true;
        const char* badName = nullptr;
        int32_t badLevel = 0;
        bool missingItem = false;

        ForEachPcOnMap(ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(ch)), [&](LPCHARACTER m) {
            if (!ok || !m || !ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(m)) || ecs::SocialSystem::GetParty(AIHelpers::EcsOf(m)) != party)
                return;

            if (m->GetLevel() < kMinLevel || m->GetLevel() > kMaxLevel)
            {
                ok = false;
                badName = ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(m)).data();
                badLevel = m->GetLevel();
                missingItem = false;
                return;
            }

            if (m->CountSpecifyItem(kRequiredItem) < 1)
            {
                ok = false;
                badName = ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(m)).data();
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

    LPDUNGEON d = CDungeonManager::instance().Create(kTritonOriginalMap);
    if (!d)
    {
        ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Triton Temple: failed to create the dungeon.");
        return true;
    }

    // Initialize dungeon flags
    d->SetFlag(kFlagFloor, 2);
    d->SetFlag(kFlagWasCompleted, 0);
    d->SetFlag(kFlagStep, 0);
    d->SetFlag(kFlagBossVid, 0);

    auto applyMember = [&](LPCHARACTER m)
        {
            if (!m || !ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(m)))
                return;

            m->RemoveSpecifyItem(kRequiredItem, 1);

            const int32_t rmAll = m->CountSpecifyItem(kRemoveAllItem);
            if (rmAll > 0)
                m->RemoveSpecifyItem(kRemoveAllItem, rmAll);

            ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(m), kQfDisconnect, 0);
            ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(m), kQfIdx, d->GetMapIndex());
            ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(m), kQfCh, (int32_t)g_bChannel);
            ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(m), kQfEnterTime, now);
            // cooldown set on completion
        };

    if (!party)
    {
        applyMember(ch);
        d->Join_Coords(ch, kEnterX, kEnterY, ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(ch)));
    }
    else
    {
        auto fn = [&](LPCHARACTER m) { applyMember(m); };
        ForEachPcOnMap(ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(ch)), [&](LPCHARACTER m){ if(m && ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(m)) && ecs::SocialSystem::GetParty(AIHelpers::EcsOf(m))==party) fn(m); });
d->JoinParty_Coords(party, kEnterX, kEnterY, ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(ch)));
    }

    s_triton.SchedulePrepare(d->GetMapIndex(), kPrepareDelay);
    return true;
}

#endif

