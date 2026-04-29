#include "stdafx.h"
#include "ecs/systems/SocialSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "ecs/systems/QuestSystem.hpp"

#include "PyramidDungeonRazor93.h"

#include <unordered_map>
#include <functional>
#include <vector>

#include "char_interface.hpp"
#include "char_manager.h"
#include "config.h"
#include "desc.h"
#include "party.h"
#include "sectree_manager.h"
#include "dungeon.h"
#include "questmanager.h"
#include "cmd.h"
#include "log.h"

namespace
{
    // ---------- CONFIG ----------
    constexpr int32_t kOriginalMap = 357;
    constexpr int32_t kPrivateMin = 3570000;
    constexpr int32_t kPrivateMax = 3580000;

    constexpr uint32_t kEntryNpcVnum = 9331;

    // Entry requirements
    constexpr int32_t kMinLevel = 72;
    constexpr int32_t kMaxLevel = 82;
    constexpr uint32_t kEntryItemVnum = 30798;
    constexpr uint32_t kRemoveAllItem = 89104; // removed in the quest's d.remove_item call
    constexpr int32_t kCooldownSeconds = 1200;
    constexpr int32_t kRejoinSeconds = 300;

    // Anti spam flag: razor93_357_%d (channel)
    constexpr int32_t kAntiSpamSeconds = 1;

    // Entry coords (Lua: d.join_cords(357, 9796, 23660))
    constexpr int32_t kJoinX = 9796;
    constexpr int32_t kJoinY = 23660;

    // Rejoin warp coords (Lua: pc.warp(218600, 348900, rejoinIDX))
    constexpr int32_t kRejoinWarpX100 = 218600;
    constexpr int32_t kRejoinWarpY100 = 348900;

    // Exit / lobby warp location (Lua: pc.set_warp_location(219, 5354, 14284))
    constexpr int32_t kLobbyMap = 219;
    constexpr int32_t kLobbyX = 5354;
    constexpr int32_t kLobbyY = 14284;

    // Dungeon flow (Lua)
    constexpr uint32_t kMetinVnum = 8474;
    constexpr uint32_t kStoneVnum = 4157;
    constexpr uint32_t kBossVnum = 4158;
    constexpr uint32_t kBonusMobVnum = 4203;

    // Spawn coords (tile coords as in Lua d.spawn_mob)
    // Prepare metins (7)
    constexpr int32_t kMetinPos[7][2] = {
        {924, 1121}, {957, 1100}, {990, 1106}, {991, 1151},
        {973, 1176}, {937, 1171}, {927, 1151}
    };

    // Stone spawn / NPC spawn
    constexpr int32_t kStoneX = 947;
    constexpr int32_t kStoneY = 1138;

    // Boss spawn
    constexpr int32_t kBossX = 940;
    constexpr int32_t kBossY = 1140;

    // Regen file (Lua: data/dungeon/pyramide/regen3.txt)
    constexpr const char* kRegen3 = "data/dungeon/pyramide/regen3.txt";

    // Dungeon flags (stored in CDungeon)
    constexpr const char* kFlagFloor = "floor";
    constexpr const char* kFlagStep = "step";
    constexpr const char* kFlagWasCompleted = "was_completed";
    constexpr const char* kFlagBossSpawned = "boss_spawned";

    // Per-character quest flags (persisted by core, no Lua required)
    constexpr const char* kQfDisconnect = "dungeonpyramid_razor93.disconnect";
    constexpr const char* kQfIdx = "dungeonpyramid_razor93.idx";
    constexpr const char* kQfCh = "dungeonpyramid_razor93.ch";
    constexpr const char* kQfCooldown = "dungeonpyramid_razor93.cooldown";


    // Iterate all PCs on a specific map (used to replace Lua d.notice/syschat).
    static void ForEachPcOnMap(int32_t mapIndex, const std::function<void(LPCHARACTER)>& fn)
    {
        LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(mapIndex);
        if (!map)
            return;

        // Don't depend on core-specific FCollectEntity helpers.
        // Collect entities via a local functor, then filter PC characters.
        struct FEntityCollector
        {
            typedef std::vector<LPENTITY> ListType;
            ListType list;
            void operator()(LPENTITY ent)
            {
                if (ent)
                    list.push_back(ent);
            }
        };

        FEntityCollector collector;
        map->for_each(collector);

        const FEntityCollector::ListType& entities = collector.list;
        for (FEntityCollector::ListType::const_iterator it = entities.begin(); it != entities.end(); ++it)
        {
            LPENTITY ent = *it;
            if (!ent || !ent->IsType(ENTITY_CHARACTER))
                continue;

            LPCHARACTER c = (LPCHARACTER)ent;
            if (!c || !c->IsPC())
                continue;

            fn(c);
        }
    }

    // Forward-declare the event callback used by event_create.
    EVENTFUNC(pyramid_prepare_event);

    struct pyramid_event_info : public event_info_data
    {
        int32_t mapIndex;
    };

    class CPyramidDungeonImpl
    {
    public:
        void Clear(int32_t mapIndex)
        {
            CancelPrepare(mapIndex);

            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIndex);
            if (!d)
                return;

            d->SetFlag(kFlagWasCompleted, 1);
            d->KillAll();
            d->ClearRegen();
            d->ExitAllLobby(1);
        }

        void SchedulePrepare(int32_t mapIndex, int32_t delaySec)
        {
            if (m_evPrepare.count(mapIndex))
                return;

            pyramid_event_info* info = AllocEventInfo<pyramid_event_info>();
            info->mapIndex = mapIndex;
            m_evPrepare[mapIndex] = event_create(pyramid_prepare_event, info, PASSES_PER_SEC(delaySec));
        }

        void CancelPrepare(int32_t mapIndex)
        {
            auto it = m_evPrepare.find(mapIndex);
            if (it == m_evPrepare.end())
                return;

            event_cancel(&it->second);
            m_evPrepare.erase(it);
        }

        void StartPrepare(int32_t mapIndex)
        {
            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIndex);
            if (!d)
                return;

            d->SetFlag(kFlagFloor, 2);
            d->SetFlag(kFlagStep, 7);
            d->SetFlag(kFlagBossSpawned, 0);
            d->SetFlag(kFlagWasCompleted, 0);

            // Clean + spawn 7 metins
            d->KillAll();
            d->ClearRegen();

            for (int i = 0; i < 7; ++i)
                d->SpawnMob(kMetinVnum, kMetinPos[i][0], kMetinPos[i][1]);

            // English notice
            ForEachPcOnMap(mapIndex, [](LPCHARACTER pc) {
                if (pc) ecs::ChatSystem::Send(AIHelpers::EcsOf(pc), CHAT_TYPE_NOTICE, "[Pyramid] Destroy all metins!");
                });
        }

        std::unordered_map<int32_t, LPEVENT> m_evPrepare;
    };

    static CPyramidDungeonImpl s_pyr;

    EVENTFUNC(pyramid_prepare_event)
    {
        // Avoid RTTI/dynamic_cast (many cores build with -fno-rtti).
        pyramid_event_info* info = (pyramid_event_info*)event->info;
        if (!info)
            return 0;

        const int32_t mapIndex = info->mapIndex;
        s_pyr.m_evPrepare.erase(mapIndex);
        s_pyr.StartPrepare(mapIndex);
        return 0;
    }

    // ---- Party functors (must be lvalues for your CParty::ForEachOnMapMember signature) ----
    struct FLevelCheck
    {
        bool ok = true;
        const char* name = nullptr;
        int32_t level = 0;

        void operator()(LPCHARACTER m)
        {
            if (!ok || !m || !m->IsPC())
                return;

            const int32_t lv = m->GetLevel();
            if (lv < kMinLevel || lv > kMaxLevel)
            {
                ok = false;
                name = m->GetName();
                level = lv;
            }
        }
    };

    struct FCooldownCheck
    {
        explicit FCooldownCheck(int32_t now) : now(now) {}

        int32_t now = 0;
        bool ok = true;
        const char* name = nullptr;
        int32_t remain = 0;

        void operator()(LPCHARACTER m)
        {
            if (!ok || !m || !m->IsPC())
                return;

            const int32_t until = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(m), kQfCooldown);
            if (until > now)
            {
                ok = false;
                name = m->GetName();
                remain = until - now;
            }
        }
    };

    struct FEntryItemCheck
    {
        bool ok = true;
        const char* name = nullptr;

        void operator()(LPCHARACTER m)
        {
            if (!ok || !m || !m->IsPC())
                return;

            if (m->CountSpecifyItem(kEntryItemVnum) < 1)
            {
                ok = false;
                name = m->GetName();
            }
        }
    };

    struct FConsumeEntryItems
    {
        void operator()(LPCHARACTER m)
        {
            if (!m || !m->IsPC())
                return;

            m->RemoveSpecifyItem(kEntryItemVnum, 1);

            // remove "remove_all" item(s) if present (up to 255 as in quests)
            const int32_t cnt = m->CountSpecifyItem(kRemoveAllItem);
            if (cnt > 0)
                m->RemoveSpecifyItem(kRemoveAllItem, cnt > 255 ? 255 : cnt);

            ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(m), kQfCooldown, get_global_time() + kCooldownSeconds);
        }
    };

    inline void ResetRejoinFlags(LPCHARACTER ch)
    {
        if (!ch) return;
        ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfDisconnect, 0);
        ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfIdx, 0);
        ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfCh, 0);
    }
}

CPyramidDungeonRazor93& CPyramidDungeonRazor93::instance()
{
    static CPyramidDungeonRazor93 s;
    return s;
}

bool CPyramidDungeonRazor93::IsPyramidDungeonMap(int32_t mapIndex) const
{
    return mapIndex >= kPrivateMin && mapIndex < kPrivateMax;
}

void CPyramidDungeonRazor93::OnPlayerDisconnect(CHARACTER* ch)
{
    if (!ch || !ch->IsPC())
        return;

    const int32_t mapIdx = ch->GetMapIndex();
    if (!IsPyramidDungeonMap(mapIdx))
        return;

    const int32_t now = get_global_time();
    ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfDisconnect, now + kRejoinSeconds);
    ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfIdx, mapIdx);
    ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfCh, (int32_t)g_bChannel);
}

void CPyramidDungeonRazor93::OnPlayerLogin(CHARACTER* ch)
{
    if (!ch || !ch->IsPC())
        return;

    const int32_t mapIdx = ch->GetMapIndex();

    // If someone ends up on the original dungeon map, move them out (Lua: idx == 357 -> pc.warp(535400, 1428400))
    if (mapIdx == kOriginalMap)
    {
        ch->WarpSet(kLobbyX * 100, kLobbyY * 100);
        return;
    }

    if (!IsPyramidDungeonMap(mapIdx))
        return;

    // Set exit location as in Lua
    ch->SetWarpLocation(kLobbyMap, kLobbyX, kLobbyY);
    ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfIdx, mapIdx);
    ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfCh, (int32_t)g_bChannel);

    // Safety: if dungeon is uninitialized, initialize like Lua login does.
    LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIdx);
    if (!d)
        return;

    if (d->GetFlag(kFlagFloor) == 0)
    {
        LPPARTY party = ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch));
        if (!party || party->GetLeaderPID() == ch->GetPlayerID())
        {
            d->SetFlag(kFlagFloor, 2);
            d->SetFlag(kFlagWasCompleted, 0);
            d->SetFlag(kFlagBossSpawned, 0);
            s_pyr.SchedulePrepare(mapIdx, 1);
        }
    }
}

bool CPyramidDungeonRazor93::OnClickNpc(CHARACTER* ch)
{
    if (!ch || !ch->IsPC())
        return false;

    if (!ch->CanWarp())
        return true;

    const int32_t now = get_global_time();

    // If clicked inside dungeon:
    // - while active: exit to lobby
    // - after completion: allow starting a fresh run
    const int32_t curMap = ch->GetMapIndex();
    if (IsPyramidDungeonMap(curMap))
    {
        LPDUNGEON cur = CDungeonManager::instance().FindByMapIndex(curMap);
        if (cur && cur->GetFlag(kFlagWasCompleted) == 0)
        {
            ch->WarpSet(kLobbyX * 100, kLobbyY * 100);
            return true;
        }
        // completed -> continue into fresh entrance flow
    }

    // Rejoin flow (Lua: 300 seconds)
    const int32_t rejoinUntil = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), kQfDisconnect);
    if (rejoinUntil > now)
    {
        const int32_t rejoinIdx = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), kQfIdx);
        const int32_t rejoinCh = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), kQfCh);

        if (IsPyramidDungeonMap(rejoinIdx))
        {
            if (rejoinCh != 0 && rejoinCh != (int32_t)g_bChannel)
            {
                ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "You were in Pyramid Dungeon on a different channel: %d", rejoinCh);
                return true;
            }

            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(rejoinIdx);
            if (d && d->GetFlag(kFlagWasCompleted) == 0)
            {
                const int32_t floor = d->GetFlag(kFlagFloor);
                if (floor == 2)
                {
                    // Lua used pc.warp(218600, 348900, rejoinIDX)
                    ch->SaveExitLocation();
                    ch->WarpSet(kRejoinWarpX100, kRejoinWarpY100, rejoinIdx);
                    ResetRejoinFlags(ch);
                    return true;
                }
            }
        }
    }

    // Anti-spam: razor93_357_%d
    char flagName[64];
    snprintf(flagName, sizeof(flagName), "razor93_357_%d", (int)g_bChannel);
    const int32_t antiUntil = quest::CQuestManager::instance().GetEventFlag(flagName);
    if (antiUntil > now)
    {
        ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Please wait %d seconds.", antiUntil - now);
        return true;
    }
    quest::CQuestManager::instance().SetEventFlag(flagName, now + kAntiSpamSeconds);

    // Leader-only if party
    LPPARTY party = ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch));
    if (party && party->GetLeaderPID() != ch->GetPlayerID())
    {
        ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Only the party leader can start the Pyramid Dungeon.");
        return true;
    }

    // Level check
    if (!party)
    {
        const int32_t lv = ch->GetLevel();
        if (lv < kMinLevel || lv > kMaxLevel)
        {
            ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Pyramid Dungeon requires level %d-%d.", kMinLevel, kMaxLevel);
            return true;
        }
    }
    else
    {
        FLevelCheck f;
        party->ForEachOnMapMember(f, ch->GetMapIndex());
        if (!f.ok)
        {
            ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Pyramid Dungeon: %s has an invalid level (Lv%d). Required: %d-%d.",
                f.name ? f.name : "Someone", f.level, kMinLevel, kMaxLevel);
            return true;
        }
    }

    // Cooldown
    if (!party)
    {
        const int32_t cdUntil = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), kQfCooldown);
        if (cdUntil > now)
        {
            ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Pyramid Dungeon is on cooldown (%d seconds).", cdUntil - now);
            return true;
        }
    }
    else
    {
        FCooldownCheck f(now);
        party->ForEachOnMapMember(f, ch->GetMapIndex());
        if (!f.ok)
        {
            ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Pyramid Dungeon: %s is on cooldown (%d seconds).",
                f.name ? f.name : "Someone", f.remain);
            return true;
        }
    }

    // Entry item (must exist for everyone who will enter)
    if (!party)
    {
        if (ch->CountSpecifyItem(kEntryItemVnum) < 1)
        {
            ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "You don't have the entry item.");
            return true;
        }
    }
    else
    {
        FEntryItemCheck f;
        party->ForEachOnMapMember(f, ch->GetMapIndex());
        if (!f.ok)
        {
            ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "%s doesn't have the entry item.", f.name ? f.name : "Someone");
            return true;
        }
    }

    // Passed checks: consume items + set cooldown (BEFORE warping)
    if (party)
    {
        FConsumeEntryItems f;
        party->ForEachOnMapMember(f, ch->GetMapIndex());
    }
    else
    {
        FConsumeEntryItems f;
        f(ch);
    }

    // Reset rejoin flags for new run
    if (party)
    {
        struct FResetRejoin { void operator()(LPCHARACTER m) { ResetRejoinFlags(m); } } f; party->ForEachOnMapMember(f, ch->GetMapIndex());
    }
    else
        ResetRejoinFlags(ch);

    // Create + join
    LPDUNGEON d = CDungeonManager::instance().Create(kOriginalMap);
    if (!d)
    {
        ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Pyramid Dungeon: failed to create dungeon.");
        return true;
    }

    d->SetFlag(kFlagFloor, 2);
    d->SetFlag(kFlagWasCompleted, 0);
    d->SetFlag(kFlagBossSpawned, 0);
    d->SetFlag(kFlagStep, 0);

    // Join party/solo at cords
    if (party)
    {
        d->JoinParty_Coords(party, kJoinX, kJoinY, ch->GetMapIndex());
    }
    else
    {
        d->Join_Coords(ch, kJoinX, kJoinY, ch->GetMapIndex());
    }

    // Prepare after 1 second (spawn metins etc.)
    s_pyr.SchedulePrepare(d->GetMapIndex(), 1);

    return true;
}

void CPyramidDungeonRazor93::OnMobKilled(CHARACTER* killer, CHARACTER* victim)
{
    if (!killer || !victim)
        return;

    const int32_t mapIdx = killer->GetMapIndex();
    if (!IsPyramidDungeonMap(mapIdx))
        return;

    LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIdx);
    if (!d)
        return;

    if (d->GetFlag(kFlagFloor) != 2)
        return;

    const uint32_t vnum = victim->GetRaceNum();

    // Metin killed -> decrement step and spawn stone when done
    if (vnum == kMetinVnum)
    {
        int32_t s = d->GetFlag(kFlagStep);
        if (s <= 0)
            return;

        s -= 1;
        d->SetFlag(kFlagStep, s);

        ForEachPcOnMap(mapIdx, [s](LPCHARACTER pc) {
            if (pc) ecs::ChatSystem::Send(AIHelpers::EcsOf(pc), CHAT_TYPE_NOTICE, "[Pyramid] Metins remaining: %d", s);
            });

        if (s == 0)
        {
            d->SpawnMob(kStoneVnum, kStoneX, kStoneY);
            ForEachPcOnMap(mapIdx, [](LPCHARACTER pc) {
                if (pc) ecs::ChatSystem::Send(AIHelpers::EcsOf(pc), CHAT_TYPE_NOTICE, "[Pyramid] The stone has appeared!");
                });
        }
        return;
    }

    // Stone killed -> load regen3 (small mobs)
    if (vnum == kStoneVnum)
    {
        d->SetFlag(kFlagBossSpawned, 0);
        d->SpawnRegen(kRegen3, true);

        ForEachPcOnMap(mapIdx, [](LPCHARACTER pc) {
            if (pc) ecs::ChatSystem::Send(AIHelpers::EcsOf(pc), CHAT_TYPE_NOTICE, "[Pyramid] Kill all monsters!");
            });
        return;
    }

    // Small mobs killed -> if no monsters left, spawn boss (only once)
    if (vnum == 4621 || vnum == 4620 || vnum == 4618 || vnum == 4619)
    {
        if (d->GetFlag(kFlagStep) != 0)
            return;

        if (d->GetFlag(kFlagBossSpawned) == 1)
            return;

        if (d->CountMonster() == 0)
        {
            d->SetFlag(kFlagBossSpawned, 1);
            d->SpawnMob(kBossVnum, kBossX, kBossY);
            ForEachPcOnMap(mapIdx, [](LPCHARACTER pc) {
                if (pc) ecs::ChatSystem::Send(AIHelpers::EcsOf(pc), CHAT_TYPE_NOTICE, "-------- Kill the Boss! --------");
                });
        }
        return;
    }

    // Boss killed -> completion
    if (vnum == kBossVnum)
    {
        if (d->GetFlag(kFlagStep) == 0 && d->GetFlag(kFlagWasCompleted) == 0)
        {
            d->SetFlag(kFlagWasCompleted, 1);

            // Global notice
            if (ecs::SocialSystem::GetParty(AIHelpers::EcsOf(killer)))
            {
                LPCHARACTER leader = ecs::SocialSystem::GetParty(AIHelpers::EcsOf(killer))->GetLeaderCharacter();
                { char buf[256]; snprintf(buf, sizeof(buf), "[Pyramid] %s has completed the dungeon!", (leader ? leader->GetName() : killer->GetName())); SendNotice(buf); }
            }
            else
            {
                { char buf[256]; snprintf(buf, sizeof(buf), "[Pyramid] %s has completed the dungeon!", killer->GetName()); SendNotice(buf); }
            }

            d->KillAll();
            d->ClearRegen();

            // Spawn NPC + bonus mob chance
            d->SpawnMob(kEntryNpcVnum, kStoneX, kStoneY);

            const int32_t bonus = 10 + quest::CQuestManager::instance().GetEventFlag("dungeon_bonus");
            if (number(1, 100) <= bonus)
                d->SpawnMob(kBonusMobVnum, kStoneX, kStoneY);

            ForEachPcOnMap(mapIdx, [](LPCHARACTER pc) {
                if (pc) ecs::ChatSystem::Send(AIHelpers::EcsOf(pc), CHAT_TYPE_NOTICE, "[Pyramid] Dungeon completed!");
                });
        }
    }
}

