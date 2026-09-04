#include "stdafx.h"
#include "ecs/systems/PointSystem.hpp"
#include "ecs/systems/MovementSystem.hpp"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/SocialSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "ecs/systems/QuestSystem.hpp"

#include "EasterDungeon.h"

#include <unordered_map>
#include <functional>
#include <cstdio>
#include <cstdarg>

#include "char_interface.hpp"
#include "char_manager.h"
#include "config.h"
#include "desc.h"
#include "party.h"
#include "sectree_manager.h"
#include "dungeon.h"
#include "cmd.h"
#include "log.h"
#include "item_manager.h" // item name from vnum
#include "ecs/EventDispatcher.hpp"
#include "ecs/events.hpp"
#include "ecs/CharacterAccessors.hpp"

namespace
{
    // ---------- CONFIG ----------
    constexpr int32_t kEasterOriginalMap = 366;
    constexpr int32_t kPrivateMin = 3660000;
    constexpr int32_t kPrivateMax = 3670000;

    // Separate NPC recommended so Valentine and Easter can coexist.
    // Change this if 20013 is already used in your mob_proto.
    constexpr uint32_t kEntryNpcVnum = 9308;

    // Same entry item as Valentine by default. Change if needed.
    constexpr uint32_t kEntryItemVnum = 50180;

    // Gameplay (same as Valentine)
    constexpr uint32_t kFloor1StoneVnum = 8461;  // stones
    constexpr uint32_t kFloor2MetinVnum = 8462;  // metins
    constexpr uint32_t kFloor2MobVnum = 4097;    // monsters
    constexpr uint32_t kBossVnum = 4103;

    // Easter map template:
    // BasePosition 716800 25600 -> base global cell: 7168 / 256
    constexpr int32_t kBaseCellX = 7168;
    constexpr int32_t kBaseCellY = 256;

    // Valentine's local layout ported onto map 366.
    // These local coordinates were reconstructed from the Valentine dungeon layout.
    constexpr int32_t kEnterLocalX = 89;
    constexpr int32_t kEnterLocalY = 74;

    constexpr int32_t kFloor2LocalX = 303;
    constexpr int32_t kFloor2LocalY = 358;

    constexpr int32_t kBossLocalX = 352;
    constexpr int32_t kBossLocalY = 357;

    // JOIN / WARP coords must be GLOBAL CELL coordinates on this source.
    constexpr int32_t kEnterX = kBaseCellX + kEnterLocalX;   // 7249
    constexpr int32_t kEnterY = kBaseCellY + kEnterLocalY;   // 491

    constexpr int32_t kFloor2X = kBaseCellX + kFloor2LocalX; // 7468
    constexpr int32_t kFloor2Y = kBaseCellY + kFloor2LocalY; // 545

    constexpr int32_t kBossX = kBaseCellX + kBossLocalX;     // 7476
    constexpr int32_t kBossY = kBaseCellY + kBossLocalY;     // 569

    constexpr int32_t kPrepareDelay = 1;
    constexpr int32_t kRejoinSeconds = 300;
    constexpr int32_t kCooldownSeconds = 1;

    constexpr int32_t kMinLevel = 40;
    constexpr int32_t kMaxLevel = 0; // 0 = no max

    // Dungeon flags (stored in CDungeon)
    constexpr const char* kFlagFloor = "easter_floor";
    constexpr const char* kFlagStep = "easter_step";
    constexpr const char* kFlagBossVid = "easter_boss";
    constexpr const char* kFlagCompleted = "easter_done";

    // Added: for broadcast message
    constexpr const char* kFlagIsParty = "easter_party";
    constexpr const char* kFlagLeaderPid = "easter_leader_pid";

    // Floor2 settle/retry
    constexpr const char* kFlagF2Retry = "easter_f2_retry";

    // Floor1 -> Floor2 delayed transition
    constexpr const char* kFlagF1ToF2 = "easter_f1_to_f2";

    inline bool IsInRange(int32_t v, int32_t lo, int32_t hi) { return v >= lo && v < hi; }

    // ---- Chat helpers ----
    inline void ChatToChar(LPCHARACTER ch, const char* fmt, ...)
    {
        if (!ch)
            return;

        char buf[512];
        va_list ap;
        va_start(ap, fmt);
        std::vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);

        ecs::ChatSystem::Send(((ch) ? (ch)->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, "%s", buf);
    }

    struct FForEachPC
    {
        std::function<void(entt::entity)> fn;
        void operator()(LPENTITY ent)
        {
            if (!ent || !ent->IsType(ENTITY_CHARACTER))
                return;
            LPCHARACTER ch = static_cast<LPCHARACTER>(ent);
            if (ch && ecs::PlayerRuntime::IsPC(ch->GetEntityHandle()))
                fn(ch->GetEntityHandle());
        }
    };

    inline void ForEachPcOnMap(int32_t mapIndex, const std::function<void(entt::entity)>& fn)
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
        std::vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);

        ForEachPcOnMap(mapIndex, [&](entt::entity pc){
                if (ecs::PlayerRuntime::IsValid(pc))
                    ecs::ChatSystem::Send(pc, CHAT_TYPE_INFO, "%s", buf);
            });
    }

    inline const char* GetItemNameByVnum(uint32_t vnum)
    {
        const TItemTable* t = ITEM_MANAGER::instance().GetTable(vnum);
        if (t && t->szName[0])
            return t->szName;
        return nullptr;
    }

    inline bool GetMapBaseTiles(int32_t mapIndex, int32_t& baseX, int32_t& baseY)
    {
        LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(mapIndex);
        if (!map)
            return false;
        baseX = map->m_setting.iBaseX / 100;
        baseY = map->m_setting.iBaseY / 100;
        return true;
    }

    // CDungeon::SpawnMob() expects LOCAL tile coords, but we only know GLOBAL coords.
    inline LPCHARACTER SpawnMobGlobal(LPDUNGEON d, uint32_t vnum, int32_t gx, int32_t gy, int32_t dir = 0)
    {
        if (!d)
            return nullptr;

        int32_t baseX = 0, baseY = 0;
        if (!GetMapBaseTiles(d->GetMapIndex(), baseX, baseY))
            return nullptr;

        const int32_t lx = gx - baseX;
        const int32_t ly = gy - baseY;
        return d->SpawnMob((int32_t)vnum, lx, ly, dir);
    }

    // ---- Count helper: how many mobs/metins remain on map (by vnum) ----
    struct FCountMobVnum
    {
        uint32_t vnum;
        int count;
        explicit FCountMobVnum(uint32_t v) : vnum(v), count(0) {}

        void operator()(LPENTITY ent)
        {
            if (!ent || !ent->IsType(ENTITY_CHARACTER))
                return;

            LPCHARACTER ch = static_cast<LPCHARACTER>(ent);
            const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;

            if (!ch)
                return;

            if (!(ch->IsMonster() || ecs::PlayerRuntime::IsStone(chEntity)))
                return;

            if (ecs::PlayerRuntime::GetRaceNum(chEntity) == vnum)
                ++count;
        }
    };

    inline int CountMobVnumOnMap(int32_t mapIndex, uint32_t vnum)
    {
        LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(mapIndex);
        if (!map)
            return 0;

        FCountMobVnum f(vnum);
        map->for_each(f);
        return f.count;
    }

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
            const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;
            if (!ch || !ecs::PlayerRuntime::IsPC(chEntity))
                return;

            const int32_t until = ecs::QuestSystem::GetFlag(chEntity, qfCooldown);
            if (until > now && ok)
            {
                ok = false;
                name = ecs::PlayerRuntime::GetName(chEntity).data();
                remain = until - now;
            }
        }
    };

    struct FEntryItemCheck
    {
        uint32_t vnum;
        bool ok;
        const char* name;

        explicit FEntryItemCheck(uint32_t v) : vnum(v), ok(true), name(nullptr) {}

        void operator()(LPCHARACTER ch)
        {
            const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;
            if (!ch || !ecs::PlayerRuntime::IsPC(chEntity))
                return;

            if (ch->CountSpecifyItem(vnum) < 1 && ok)
            {
                ok = false;
                name = ecs::PlayerRuntime::GetName(chEntity).data();
            }
        }
    };
}

// ------------------ Event plumbing ------------------
namespace
{
    EVENTINFO(easter_dungeon_event_info)
    {
        int32_t mapIndex;
        easter_dungeon_event_info() : mapIndex(0) {}
    };
}

class CEasterDungeonImpl
{
public:
    std::unordered_map<int32_t, LPEVENT> m_evPrepare;
    std::unordered_map<int32_t, LPEVENT> m_evExit;
    std::unordered_map<int32_t, LPEVENT> m_evCheck; // floor2 settle check
    std::unordered_map<int32_t, LPEVENT> m_evToF2;  // floor1 -> floor2 delay

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
        CancelEvent(m_evExit, mapIndex);
        CancelEvent(m_evCheck, mapIndex);
        CancelEvent(m_evToF2, mapIndex);
    }

    void ClearDungeon(int32_t mapIndex)
    {
        CancelAll(mapIndex);

        LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIndex);
        if (!d)
            return;

        d->SetFlag(kFlagCompleted, 1);
        d->KillAll();
        d->ClearRegen();
        d->ExitAllLobby(1);
    }

    void StartFloor1(int32_t mapIndex)
    {
        LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIndex);
        if (!d)
        {
            CancelAll(mapIndex);
            return;
        }

        d->SetFlag(kFlagFloor, 1);
        d->SetFlag(kFlagStep, 11);
        d->SetFlag(kFlagCompleted, 0);
        d->SetFlag(kFlagBossVid, 0);

        d->SetFlag(kFlagF2Retry, 0);
        d->SetFlag(kFlagF1ToF2, 0);

        // Stones around the entry position (11 db)
        SpawnMobGlobal(d, kFloor1StoneVnum, 7291, 329);
        SpawnMobGlobal(d, kFloor1StoneVnum, 7323, 383);
        SpawnMobGlobal(d, kFloor1StoneVnum, 7278, 398);
        SpawnMobGlobal(d, kFloor1StoneVnum, 7236, 361);

        SpawnMobGlobal(d, kFloor1StoneVnum, 7289, 324);
        SpawnMobGlobal(d, kFloor1StoneVnum, 7330, 352);
        SpawnMobGlobal(d, kFloor1StoneVnum, 7306, 402);
        SpawnMobGlobal(d, kFloor1StoneVnum, 7250, 384);

        SpawnMobGlobal(d, kFloor1StoneVnum, 7279, 346);
        SpawnMobGlobal(d, kFloor1StoneVnum, 7295, 382);
        SpawnMobGlobal(d, kFloor1StoneVnum, 7267, 384);

        // TASK INFO
        ChatToMap(mapIndex, "Husvet: Feladat #1: Oljetek meg az osszes kovet (11 db).");
        ChatToMap(mapIndex, "Husvet: Ha kesz, 10 mp mulva indul a 2. emelet.");
    }

    void StartFloor2(int32_t mapIndex)
    {
        LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIndex);
        if (!d)
            return;

        d->KillAll();
        d->ClearRegen();

        d->SetFlag(kFlagFloor, 2);
        d->SetFlag(kFlagStep, 0); // floor2-n nem szamolunk, maradekot nezunk
        d->SetFlag(kFlagF2Retry, 0);

        // Teleport everyone inside the instance to floor2 location
        d->JumpAll(d->GetMapIndex(), kFloor2X, kFloor2Y);

        // Metins around floor2 center
        SpawnMobGlobal(d, kFloor2MetinVnum, 7476, 625);
        SpawnMobGlobal(d, kFloor2MetinVnum, 7545, 584);
        SpawnMobGlobal(d, kFloor2MetinVnum, 7595, 643);
        SpawnMobGlobal(d, kFloor2MetinVnum, 7572, 602);
        SpawnMobGlobal(d, kFloor2MetinVnum, 7519, 659);
        SpawnMobGlobal(d, kFloor2MetinVnum, 7485, 638);

        // Monsters - FIX POZICIOK (11 db, GLOBAL TILE)
        static const int32_t mobs[11][2] =
        {
            { 7506, 625 },
            { 7488, 582 },
            { 7556, 589 },
            { 7544, 638 },
            { 7509, 652 },
            { 7495, 609 },
            { 7532, 585 },
            { 7541, 617 },
            { 7532, 650 },
            { 7495, 647 },
            { 7480, 609 },
        };

        for (int i = 0; i < 11; ++i)
            SpawnMobGlobal(d, kFloor2MobVnum, mobs[i][0], mobs[i][1]);

        // TASK INFO
        ChatToMap(mapIndex, "Husvet: Feladat #2: Oljetek meg az osszes metint es szornyet.");
        ChatToMap(mapIndex, "Husvet: A boss csak akkor jon, ha minden le van olve.");
    }

    void SpawnBoss(int32_t mapIndex)
    {
        LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIndex);
        if (!d)
            return;

        // dupla spawn vedelem
        if (d->GetFlag(kFlagFloor) != 2)
            return;

        // ha mar valamiert be van allitva boss vid, ne spawnolj ujra
        if (d->GetFlag(kFlagBossVid) != 0)
            return;

        d->SetFlag(kFlagFloor, 3);
        d->SetFlag(kFlagStep, 0);

        LPCHARACTER boss = SpawnMobGlobal(d, kBossVnum, kBossX, kBossY);
        d->SetFlag(kFlagBossVid, boss ? (int32_t)boss->GetLegacyVID() : 0);

        ChatToMap(mapIndex, "Husvet: A boss megjelent! Feladat #3: Oljtek meg a bosst.");
    }

    void Complete(int32_t mapIndex)
    {
        LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIndex);
        if (!d)
            return;

        if (d->GetFlag(kFlagCompleted) != 0)
            return;

        d->SetFlag(kFlagCompleted, 1);

        // eventek stop (exit-et nem cancel-elj�k, mert most allitjuk be)
        CancelEvent(m_evCheck, mapIndex);
        CancelEvent(m_evToF2, mapIndex);

        const int32_t now = get_global_time();
        ForEachPcOnMap(mapIndex, [&](entt::entity ch){
                if (!ecs::PlayerRuntime::IsValid(ch))
                    return;

                ecs::QuestSystem::SetFlag(ch, "easter_dungeon.disconnect", 0);
                ecs::QuestSystem::SetFlag(ch, "easter_dungeon.idx", 0);
                ecs::QuestSystem::SetFlag(ch, "easter_dungeon.ch", 0);
                ecs::QuestSystem::SetFlag(ch, "easter_dungeon.enter_time", 0);
                ecs::QuestSystem::SetFlag(ch, "easter_dungeon.cooldown", now + kCooldownSeconds);
            });

        // --- Broadcast: solo vs party ---
        const bool isPartyRun = (d->GetFlag(kFlagIsParty) != 0);
        const int32_t leaderPid = d->GetFlag(kFlagLeaderPid);

        const char* leaderName = nullptr;
        ForEachPcOnMap(mapIndex, [&](entt::entity pc){
                if (leaderName)
                    return;
                if (!ecs::PlayerRuntime::IsPC(pc))
                    return;

                if (leaderPid <= 0 || (int32_t)ecs::PlayerRuntime::GetPlayerID(pc) == leaderPid)
                    leaderName = ecs::PlayerRuntime::GetName(pc).data();
            });

        if (!leaderName)
            leaderName = "valaki";

        char notice[256];
        if (isPartyRun)
            std::snprintf(notice, sizeof(notice), "%s es csoportja teljesitette a Husvet dungeont!", leaderName);
        else
            std::snprintf(notice, sizeof(notice), "%s befejezte a Husvet dungeont!", leaderName);

        SendNotice(notice);

        // Boss halal utan spawnolja az NPC-t, hogy ujra tudjanak menni
        SpawnMobGlobal(d, kEntryNpcVnum, kBossX, kBossY);

        // TASK INFO
        ChatToMap(mapIndex, "Husvet: Kesz! 30 mp loot/potolas, utana kidob a rendszer.");
        ChatToMap(mapIndex, "Husvet: Az NPC-n ujra tudod inditani (ha van belepo item es nincs cooldown).");

        // Loot + "kipotolas" ido (30 mp), utana kidob mindenkit
        ScheduleExit(mapIndex, 30);
    }

    void SchedulePrepare(int32_t mapIndex, int32_t seconds);
    void ScheduleExit(int32_t mapIndex, int32_t seconds);
    void ScheduleCheckFloor2(int32_t mapIndex, int32_t seconds);
    void ScheduleToFloor2(int32_t mapIndex, int32_t seconds);
};

static CEasterDungeonImpl s_easter;

EVENTFUNC(easter_dungeon_prepare_event)
{
    auto* info = dynamic_cast<easter_dungeon_event_info*>(event->info);
    if (!info)
        return 0;

    const int32_t mapIndex = info->mapIndex;
    s_easter.m_evPrepare.erase(mapIndex);
    s_easter.StartFloor1(mapIndex);
    g_dispatcher.trigger(ecs::EvDungeonPrepare { static_cast<uint32_t>(mapIndex) });
    return 0;
}

EVENTFUNC(easter_dungeon_exit_event)
{
    auto* info = dynamic_cast<easter_dungeon_event_info*>(event->info);
    if (!info)
        return 0;

    const int32_t mapIndex = info->mapIndex;
    s_easter.m_evExit.erase(mapIndex);
    s_easter.ClearDungeon(mapIndex);
    g_dispatcher.trigger(ecs::EvDungeonEnd { static_cast<uint32_t>(mapIndex) });
    return 0;
}

// floor2 settle check event (retry, mert kill utani torles/spawn tickkel kesobb jonhet)
EVENTFUNC(easter_dungeon_check_event)
{
    auto* info = dynamic_cast<easter_dungeon_event_info*>(event->info);
    if (!info)
        return 0;

    const int32_t mapIndex = info->mapIndex;
    s_easter.m_evCheck.erase(mapIndex);

    LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIndex);
    if (!d)
        return 0;

    if (d->GetFlag("easter_floor") != 2)
        return 0;

    const int metins = CountMobVnumOnMap(mapIndex, kFloor2MetinVnum);
    const int mobs = CountMobVnumOnMap(mapIndex, kFloor2MobVnum);

    if (metins == 0 && mobs == 0)
    {
        d->SetFlag("easter_f2_retry", 0);
        s_easter.SpawnBoss(mapIndex);
        g_dispatcher.trigger(ecs::EvDungeonPrepare { static_cast<uint32_t>(mapIndex) });
        return 0;
    }

    int retry = d->GetFlag("easter_f2_retry");
    if (retry < 10)
    {
        d->SetFlag("easter_f2_retry", retry + 1);
        s_easter.ScheduleCheckFloor2(mapIndex, 1);
    }

    return 0;
}

// floor1 -> floor2 delay event (10 mp)
EVENTFUNC(easter_dungeon_to_floor2_event)
{
    auto* info = dynamic_cast<easter_dungeon_event_info*>(event->info);
    if (!info)
        return 0;

    const int32_t mapIndex = info->mapIndex;
    s_easter.m_evToF2.erase(mapIndex);

    LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIndex);
    if (!d)
        return 0;

    if (d->GetFlag("easter_done") != 0)
        return 0;

    if (d->GetFlag("easter_floor") != 1)
        return 0;

    s_easter.StartFloor2(mapIndex);
    g_dispatcher.trigger(ecs::EvDungeonPrepare { static_cast<uint32_t>(mapIndex) });
    return 0;
}

void CEasterDungeonImpl::SchedulePrepare(int32_t mapIndex, int32_t seconds)
{
    CancelEvent(m_evPrepare, mapIndex);
    auto* info = AllocEventInfo<easter_dungeon_event_info>();
    info->mapIndex = mapIndex;
    m_evPrepare[mapIndex] = event_create(easter_dungeon_prepare_event, info, PASSES_PER_SEC(seconds));
}

void CEasterDungeonImpl::ScheduleExit(int32_t mapIndex, int32_t seconds)
{
    CancelEvent(m_evExit, mapIndex);
    auto* info = AllocEventInfo<easter_dungeon_event_info>();
    info->mapIndex = mapIndex;
    m_evExit[mapIndex] = event_create(easter_dungeon_exit_event, info, PASSES_PER_SEC(seconds));
}

void CEasterDungeonImpl::ScheduleCheckFloor2(int32_t mapIndex, int32_t seconds)
{
    CancelEvent(m_evCheck, mapIndex);
    auto* info = AllocEventInfo<easter_dungeon_event_info>();
    info->mapIndex = mapIndex;
    m_evCheck[mapIndex] = event_create(easter_dungeon_check_event, info, PASSES_PER_SEC(seconds));
}

void CEasterDungeonImpl::ScheduleToFloor2(int32_t mapIndex, int32_t seconds)
{
    CancelEvent(m_evToF2, mapIndex);
    auto* info = AllocEventInfo<easter_dungeon_event_info>();
    info->mapIndex = mapIndex;
    m_evToF2[mapIndex] = event_create(easter_dungeon_to_floor2_event, info, PASSES_PER_SEC(seconds));
}

// ------------------ Public facade ------------------

CEasterDungeon& CEasterDungeon::instance()
{
    static CEasterDungeon inst;
    return inst;
}

bool CEasterDungeon::IsEasterDungeonMap(int32_t mapIndex) const
{
    return IsInRange(mapIndex, kPrivateMin, kPrivateMax);
}

void CEasterDungeon::OnPlayerDisconnect(entt::entity character)
{
    LPCHARACTER ch = ecs::LegacyCharOf(character);
    if (!ch || !ecs::PlayerRuntime::IsPC(character))
        return;

    const int32_t idx = ecs::PlayerRuntime::GetMapIndex(character);
    if (!IsEasterDungeonMap(idx))
        return;

    ecs::QuestSystem::SetFlag(character, "easter_dungeon.disconnect", get_global_time() + kRejoinSeconds);
    ecs::QuestSystem::SetFlag(character, "easter_dungeon.idx", idx);
    ecs::QuestSystem::SetFlag(character, "easter_dungeon.ch", (int32_t)g_bChannel);
}

void CEasterDungeon::OnPlayerLogin(entt::entity character)
{
    LPCHARACTER ch = ecs::LegacyCharOf(character);
    if (!ch || !ecs::PlayerRuntime::IsPC(character))
        return;

    const int32_t idx = ecs::PlayerRuntime::GetMapIndex(character);
    if (!IsEasterDungeonMap(idx))
        return;

    LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
    if (!d)
    {
        ecs::MovementSystem::ExitToSavedLocation(character);
        return;
    }

    ch->SetDungeon(d);

    // Task reminder on login (rejoin)
    if (d->GetFlag(kFlagCompleted) == 0)
    {
        const int floor = d->GetFlag(kFlagFloor);
        if (floor == 1)
        {
            ChatToChar(ch, "Easter: Task #1: Destroy all stones. Remaining: %d", d->GetFlag(kFlagStep));
        }
        else if (floor == 2)
        {
            const int metins = CountMobVnumOnMap(idx, kFloor2MetinVnum);
            const int mobs = CountMobVnumOnMap(idx, kFloor2MobVnum);
           // ChatToChar(ch, "Easter: Task #2: Metins + monsters. Metins left: %d, monsters left: %d", metins, mobs);
        }
        else if (floor == 3)
        {
            ChatToChar(ch, "Easter: Task #3: Kill the boss!");
        }
    }
    else
    {
        ChatToChar(ch, "Easter: Dungeon complete. Click the NPC to start again.");
    }

    if (d->GetFlag(kFlagFloor) == 0)
        s_easter.SchedulePrepare(idx, 1);
}

void CEasterDungeon::OnMobKilled(entt::entity killer, entt::entity victim)
{
    LPCHARACTER pkVictim = ecs::LegacyCharOf(victim);
    if (!ecs::PlayerRuntime::IsValid(killer) || !pkVictim)
        return;
    if (!ecs::PlayerRuntime::IsPC(killer))
        return;

    if (!(pkVictim->IsMonster() || ecs::PlayerRuntime::IsStone(victim)))
        return;

    const int32_t idx = ecs::PlayerRuntime::GetMapIndex(victim);
    if (!IsEasterDungeonMap(idx))
        return;

    LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
    if (!d)
        return;

    const uint32_t vnum = ecs::PlayerRuntime::GetRaceNum(victim);
    const int32_t floor = d->GetFlag(kFlagFloor);

    // ---------------- Floor 1: stones countdown -> floor2 10 mp kesleltetessel ----------------
    if (vnum == kFloor1StoneVnum && floor == 1)
    {
        int32_t s = d->GetFlag(kFlagStep) - 1;
        if (s < 0)
            s = 0;
        d->SetFlag(kFlagStep, s);

        ChatToMap(idx, "Husvet: Hatralevo kov: %d", s);

        if (s == 0)
        {
            if (d->GetFlag(kFlagF1ToF2) == 0)
            {
                d->SetFlag(kFlagF1ToF2, 1);
                ChatToMap(idx, "Husvet: Kesz! 10 mp mulva indul a 2. emelet.");
                s_easter.ScheduleToFloor2(idx, 10);
            }
        }
        return;
    }

    // ---------------- Floor 2: settle-check (delay) + task progress info ----------------
    if (floor == 2 && (vnum == kFloor2MetinVnum || vnum == kFloor2MobVnum))
    {
        // progress info (killernek)
        int metins = CountMobVnumOnMap(idx, kFloor2MetinVnum);
        int mobs = CountMobVnumOnMap(idx, kFloor2MobVnum);

        // ha victim meg "benne van", vonjunk le 1-et (tick miatt)
        if (vnum == kFloor2MetinVnum && metins > 0) --metins;
        if (vnum == kFloor2MobVnum && mobs > 0) --mobs;

        //ChatToChar(killer, "Easter: Metins left: %d, monsters left: %d", metins, mobs);

        d->SetFlag(kFlagF2Retry, 0);
        s_easter.ScheduleCheckFloor2(idx, 1);
        return;
    }

    // ---------------- Boss ----------------
    if (vnum == kBossVnum && floor == 3)
    {
        s_easter.Complete(idx);
        return;
    }
}

bool CEasterDungeon::OnClickNpc(entt::entity character)
{
    LPCHARACTER ch = ecs::LegacyCharOf(character);
    if (!ch || !ecs::PlayerRuntime::IsPC(character))
        return false;

    if (!ecs::PlayerRuntime::CanWarp(character))
        return true;

    const int32_t mapIdx = ecs::PlayerRuntime::GetMapIndex(character);

    // If clicked inside the dungeon while run is active -> exit to saved location.
    if (IsEasterDungeonMap(mapIdx))
    {
        LPDUNGEON cur = CDungeonManager::instance().FindByMapIndex(mapIdx);
        if (cur && cur->GetFlag(kFlagCompleted) == 0)
        {
            ecs::MovementSystem::ExitToSavedLocation(character);
            return true;
        }
        // completed -> allow starting a new run from the same NPC
    }

    const int32_t now = get_global_time();

    // Rejoin flow
    const int32_t rejoinUntil = ecs::QuestSystem::GetFlag(character, "easter_dungeon.disconnect");
    if (rejoinUntil > now)
    {
        const int32_t rejoinIdx = ecs::QuestSystem::GetFlag(character, "easter_dungeon.idx");
        const int32_t rejoinCh = ecs::QuestSystem::GetFlag(character, "easter_dungeon.ch");

        if (rejoinIdx >= kPrivateMin && rejoinIdx < kPrivateMax)
        {
            if (rejoinCh != 0 && rejoinCh != (int32_t)g_bChannel)
            {
                ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "You were in Easter Dungeon on a different channel. Channel: %d", rejoinCh);
                return true;
            }

            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(rejoinIdx);
            if (d && d->GetFlag(kFlagCompleted) == 0)
            {
                ch->SaveExitLocation();
                ecs::MovementSystem::WarpSet(character, kEnterX * 100, kEnterY * 100, rejoinIdx);
                return true;
            }
        }
    }

    // Level check
    if (kMinLevel > 0 && ecs::PointSystem::GetLevel(character) < kMinLevel)
    {
        ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Easter: minimum level is %d.", kMinLevel);
        return true;
    }
    if (kMaxLevel > 0 && ecs::PointSystem::GetLevel(character) > kMaxLevel)
    {
        ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Easter: maximum level is %d.", kMaxLevel);
        return true;
    }

    // Cooldown
    const int32_t cdUntil = ecs::QuestSystem::GetFlag(character, "easter_dungeon.cooldown");
    if (cdUntil > now)
    {
        const int32_t remain = cdUntil - now;
        ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Easter: you must wait %d seconds.", remain);
        return true;
    }

    LPPARTY party = ecs::SocialSystem::GetParty(character);
    if (party && party->GetLeaderPID() != ecs::PlayerRuntime::GetPlayerID(character))
    {
        ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Only the party leader can start Easter Dungeon.");
        return true;
    }

    // Party level check: everyone who will enter must meet level requirement (same map as leader)
    if (party)
    {
        bool ok = true;
        const char* badName = nullptr;
        int32_t badLevel = 0;

        ForEachPcOnMap(ecs::PlayerRuntime::GetMapIndex(character), [&](entt::entity m){
            if (!ok || !ecs::PlayerRuntime::IsPC(m) || ecs::SocialSystem::GetParty(m) != party)
                return;

            if ((kMinLevel > 0 && ecs::PointSystem::GetLevel(m) < kMinLevel) || (kMaxLevel > 0 && ecs::PointSystem::GetLevel(m) > kMaxLevel))
            {
                ok = false;
                badName = ecs::PlayerRuntime::GetName(m).data();
                badLevel = ecs::PointSystem::GetLevel(m);
                return;
            }
        });

        if (!ok)
        {
            ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "%s has an invalid level (Lv%d). Required: %d-%d.", badName ? badName : "A party member", badLevel, kMinLevel, kMaxLevel);
            return true;
        }
    }

    // Party cooldown: everyone must be off cooldown
    if (party)
    {
        FCooldownCheck f(now, "easter_dungeon.cooldown");
        ForEachPcOnMap(ecs::PlayerRuntime::GetMapIndex(character), [&](entt::entity m){
            LPCHARACTER pkM = ecs::LegacyCharOf(m);
            if (!pkM || !ecs::PlayerRuntime::IsPC(m) || ecs::SocialSystem::GetParty(m) != party)
                return;
            f(pkM);
        });
if (!f.ok)
        {
            ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "%s is still on cooldown (%d seconds).", f.name ? f.name : "valaki", f.remain);
            return true;
        }
    }

    const char* entryItemName = GetItemNameByVnum(kEntryItemVnum);
    if (!entryItemName)
        entryItemName = "Entry item";

    // Entry item check (NAME not VNUM)
    if (!party)
    {
        if (ch->CountSpecifyItem(kEntryItemVnum) < 1)
        {
            ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Easter: required to enter: %s (x1).", entryItemName);
            return true;
        }
    }
    else
    {
        FEntryItemCheck it(kEntryItemVnum);
        ForEachPcOnMap(ecs::PlayerRuntime::GetMapIndex(character), [&](entt::entity m){
            LPCHARACTER pkM = ecs::LegacyCharOf(m);
            if (!pkM || !ecs::PlayerRuntime::IsPC(m) || ecs::SocialSystem::GetParty(m) != party)
                return;
            it(pkM);
        });
if (!it.ok)
        {
            ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "%s doesn't have the entry item: %s (x1).",
                it.name ? it.name : "valaki", entryItemName);
            return true;
        }
    }

    // Create dungeon instance
    LPDUNGEON d = CDungeonManager::instance().Create(kEasterOriginalMap);
    if (!d)
    {
        ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Easter: failed to create the dungeon.");
        return true;
    }

    // Initialize dungeon flags
    d->SetFlag(kFlagFloor, 0);
    d->SetFlag(kFlagCompleted, 0);
    d->SetFlag(kFlagStep, 0);
    d->SetFlag(kFlagBossVid, 0);
    d->SetFlag(kFlagIsParty, party ? 1 : 0);
    d->SetFlag(kFlagLeaderPid, (int32_t)ecs::PlayerRuntime::GetPlayerID(character));
    d->SetFlag(kFlagF2Retry, 0);
    d->SetFlag(kFlagF1ToF2, 0);

    // Set per-player rejoin flags + consume entry item
    auto applyMember = [&](entt::entity m){
            LPCHARACTER pkM = ecs::LegacyCharOf(m);
            if (!pkM || !ecs::PlayerRuntime::IsPC(m))
                return;

            // Consume entry item (already checked above)
            pkM->RemoveSpecifyItem(kEntryItemVnum, 1);

            ecs::QuestSystem::SetFlag(m, "easter_dungeon.disconnect", 0);
            ecs::QuestSystem::SetFlag(m, "easter_dungeon.idx", d->GetMapIndex());
            ecs::QuestSystem::SetFlag(m, "easter_dungeon.ch", (int32_t)g_bChannel);
            ecs::QuestSystem::SetFlag(m, "easter_dungeon.enter_time", now);
        };

    if (!party)
    {
        applyMember(character);
        d->Join_Coords(ch, kEnterX, kEnterY, kEasterOriginalMap);
    }
    else
    {
        ForEachPcOnMap(ecs::PlayerRuntime::GetMapIndex(character), [&](entt::entity m){
            if (!ecs::PlayerRuntime::IsPC(m) || ecs::SocialSystem::GetParty(m) != party)
                return;
            applyMember(m);
        });
        d->JoinParty_Coords(party, kEnterX, kEnterY, ecs::PlayerRuntime::GetMapIndex(character));
    }

    // Small hint right after enter
    ChatToChar(ch, "Easter: The dungeon is starting. Please wait 1 second...");

    // Start floor1 after short delay
    s_easter.SchedulePrepare(d->GetMapIndex(), kPrepareDelay);
    return true;
}

