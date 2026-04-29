#include "stdafx.h"
#include "ecs/AIHelpers.hpp"
#include "ecs/systems/QuestSystem.hpp"

#include "ValentineDungeon.h"

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

namespace
{
    // ---------- CONFIG ----------
    constexpr int32_t kValOriginalMap = 377;
    constexpr int32_t kPrivateMin = 3770000;
    constexpr int32_t kPrivateMax = 3780000;

    // NPC that starts the dungeon (set this to your entry NPC vnum)
    constexpr uint32_t kEntryNpcVnum = 20012;

    // Entry item (1 db / player)
    constexpr uint32_t kEntryItemVnum = 78314;

    // Gameplay
    constexpr uint32_t kFloor1StoneVnum = 171;  // stones
    constexpr uint32_t kFloor2MetinVnum = 173;  // metins
    constexpr uint32_t kFloor2MobVnum = 174;  // monsters
    constexpr uint32_t kBossVnum = 176;

    // IMPORTANT: these are GLOBAL TILE coordinates (pixel/100)
    constexpr int32_t kEnterX = 11345;
    constexpr int32_t kEnterY = 2283;

    // Floor2 target position (adjust if you want a different room)
    constexpr int32_t kFloor2X = 11564;
    constexpr int32_t kFloor2Y = 2337;

    // Boss spawn (global tile)
    constexpr int32_t kBossX = 11572;
    constexpr int32_t kBossY = 2361;

    constexpr int32_t kPrepareDelay = 1;
    constexpr int32_t kRejoinSeconds = 300;
    constexpr int32_t kCooldownSeconds = 10;

    constexpr int32_t kMinLevel = 40;
    constexpr int32_t kMaxLevel = 0; // 0 = no max

    // Dungeon flags (stored in CDungeon)
    constexpr const char* kFlagFloor = "val_floor";
    constexpr const char* kFlagStep = "val_step";
    constexpr const char* kFlagBossVid = "val_boss";
    constexpr const char* kFlagCompleted = "val_done";

    // Added: for broadcast message
    constexpr const char* kFlagIsParty = "val_party";
    constexpr const char* kFlagLeaderPid = "val_leader_pid";

    // Floor2 settle/retry
    constexpr const char* kFlagF2Retry = "val_f2_retry";

    // Floor1 -> Floor2 delayed transition
    constexpr const char* kFlagF1ToF2 = "val_f1_to_f2";

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

        ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "%s", buf);
    }

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

    inline void ChatToMap(int32_t mapIndex, const char* fmt, ...)
    {
        char buf[512];
        va_list ap;
        va_start(ap, fmt);
        std::vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);

        ForEachPcOnMap(mapIndex, [&](LPCHARACTER pc)
            {
                if (pc)
                    ecs::ChatSystem::Send(AIHelpers::EcsOf(pc), CHAT_TYPE_INFO, "%s", buf);
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
            if (!ch)
                return;

            if (!(ch->IsMonster() || ch->IsStone()))
                return;

            if (ch->GetRaceNum() == vnum)
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
            if (!ch || !ch->IsPC())
                return;

            const int32_t until = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), qfCooldown);
            if (until > now && ok)
            {
                ok = false;
                name = ch->GetName();
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
            if (!ch || !ch->IsPC())
                return;

            if (ch->CountSpecifyItem(vnum) < 1 && ok)
            {
                ok = false;
                name = ch->GetName();
            }
        }
    };
}

// ------------------ Event plumbing ------------------
namespace
{
    EVENTINFO(val_dungeon_event_info)
    {
        int32_t mapIndex;
        val_dungeon_event_info() : mapIndex(0) {}
    };
}

class CValentineDungeonImpl
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
        SpawnMobGlobal(d, kFloor1StoneVnum, 11341, 2311);
        SpawnMobGlobal(d, kFloor1StoneVnum, 11320, 2323);
        SpawnMobGlobal(d, kFloor1StoneVnum, 11317, 2343);
        SpawnMobGlobal(d, kFloor1StoneVnum, 11315, 2358);

        SpawnMobGlobal(d, kFloor1StoneVnum, 11361, 2365);
        SpawnMobGlobal(d, kFloor1StoneVnum, 11375, 2351);
        SpawnMobGlobal(d, kFloor1StoneVnum, 11383, 2332);
        SpawnMobGlobal(d, kFloor1StoneVnum, 11374, 2313);

        SpawnMobGlobal(d, kFloor1StoneVnum, 11355, 2327);
        SpawnMobGlobal(d, kFloor1StoneVnum, 11340, 2340);
        SpawnMobGlobal(d, kFloor1StoneVnum, 11343, 2362);

        // TASK INFO
        ChatToMap(mapIndex, "Valentin: Feladat #1: Oljetek meg az osszes kovet (11 db).");
        ChatToMap(mapIndex, "Valentin: Ha kesz, 10 mp mulva indul a 2. emelet.");
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
        SpawnMobGlobal(d, kFloor2MetinVnum, 11576, 2350);
        SpawnMobGlobal(d, kFloor2MetinVnum, 11592, 2351);
        SpawnMobGlobal(d, kFloor2MetinVnum, 11579, 2337);
        SpawnMobGlobal(d, kFloor2MetinVnum, 11567, 2350);
        SpawnMobGlobal(d, kFloor2MetinVnum, 11545, 2347);
        SpawnMobGlobal(d, kFloor2MetinVnum, 11531, 2353);

        // Monsters - FIX POZICIOK (11 db, GLOBAL TILE)
        static const int32_t mobs[11][2] =
        {
            { 11566, 2344 },
            { 11570, 2343 },
            { 11549, 2347 },
            { 11537, 2343 },
            { 11576, 2325 },
            { 11588, 2337 },
            { 11546, 2346 },
            { 11547, 2343 },
            { 11552, 2342 },
            { 11551, 2342 },
            { 11557, 2336 },
        };

        for (int i = 0; i < 11; ++i)
            SpawnMobGlobal(d, kFloor2MobVnum, mobs[i][0], mobs[i][1]);

        // TASK INFO
        ChatToMap(mapIndex, "Valentin: Feladat #2: Oljetek meg az osszes metint es szornyet.");
        ChatToMap(mapIndex, "Valentin: A boss csak akkor jon, ha minden le van olve.");
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

        ChatToMap(mapIndex, "Valentin: A boss megjelent! Feladat #3: Oljtek meg a bosst.");
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
        ForEachPcOnMap(mapIndex, [&](LPCHARACTER ch)
            {
                if (!ch)
                    return;

                ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), "valentine_dungeon.disconnect", 0);
                ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), "valentine_dungeon.idx", 0);
                ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), "valentine_dungeon.ch", 0);
                ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), "valentine_dungeon.enter_time", 0);
                ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), "valentine_dungeon.cooldown", now + kCooldownSeconds);
            });

        // --- Broadcast: solo vs party ---
        const bool isPartyRun = (d->GetFlag(kFlagIsParty) != 0);
        const int32_t leaderPid = d->GetFlag(kFlagLeaderPid);

        const char* leaderName = nullptr;
        ForEachPcOnMap(mapIndex, [&](LPCHARACTER pc)
            {
                if (leaderName)
                    return;
                if (!pc || !pc->IsPC())
                    return;

                if (leaderPid <= 0 || (int32_t)pc->GetPlayerID() == leaderPid)
                    leaderName = pc->GetName();
            });

        if (!leaderName)
            leaderName = "valaki";

        char notice[256];
        if (isPartyRun)
            std::snprintf(notice, sizeof(notice), "%s es csoportja teljesitette a Valentin dungeont!", leaderName);
        else
            std::snprintf(notice, sizeof(notice), "%s befejezte a Valentin dungeont!", leaderName);

        SendNotice(notice);

        // Boss halal utan spawnolja az NPC-t, hogy ujra tudjanak menni
        SpawnMobGlobal(d, kEntryNpcVnum, kBossX, kBossY);

        // TASK INFO
        ChatToMap(mapIndex, "Valentin: Kesz! 30 mp loot/potolas, utana kidob a rendszer.");
        ChatToMap(mapIndex, "Valentin: Az NPC-n ujra tudod inditani (ha van belepo item es nincs cooldown).");

        // Loot + "kipotolas" ido (30 mp), utana kidob mindenkit
        ScheduleExit(mapIndex, 30);
    }

    void SchedulePrepare(int32_t mapIndex, int32_t seconds);
    void ScheduleExit(int32_t mapIndex, int32_t seconds);
    void ScheduleCheckFloor2(int32_t mapIndex, int32_t seconds);
    void ScheduleToFloor2(int32_t mapIndex, int32_t seconds);
};

static CValentineDungeonImpl s_val;

EVENTFUNC(val_dungeon_prepare_event)
{
    auto* info = dynamic_cast<val_dungeon_event_info*>(event->info);
    if (!info)
        return 0;

    const int32_t mapIndex = info->mapIndex;
    s_val.m_evPrepare.erase(mapIndex);
    s_val.StartFloor1(mapIndex);
    g_dispatcher.trigger(ecs::EvDungeonPrepare { static_cast<uint32_t>(mapIndex) });
    return 0;
}

EVENTFUNC(val_dungeon_exit_event)
{
    auto* info = dynamic_cast<val_dungeon_event_info*>(event->info);
    if (!info)
        return 0;

    const int32_t mapIndex = info->mapIndex;
    s_val.m_evExit.erase(mapIndex);
    s_val.ClearDungeon(mapIndex);
    g_dispatcher.trigger(ecs::EvDungeonEnd { static_cast<uint32_t>(mapIndex) });
    return 0;
}

// floor2 settle check event (retry, mert kill utani torles/spawn tickkel kesobb jonhet)
EVENTFUNC(val_dungeon_check_event)
{
    auto* info = dynamic_cast<val_dungeon_event_info*>(event->info);
    if (!info)
        return 0;

    const int32_t mapIndex = info->mapIndex;
    s_val.m_evCheck.erase(mapIndex);

    LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIndex);
    if (!d)
        return 0;

    if (d->GetFlag("val_floor") != 2)
        return 0;

    const int metins = CountMobVnumOnMap(mapIndex, kFloor2MetinVnum);
    const int mobs = CountMobVnumOnMap(mapIndex, kFloor2MobVnum);

    if (metins == 0 && mobs == 0)
    {
        d->SetFlag("val_f2_retry", 0);
        s_val.SpawnBoss(mapIndex);
        g_dispatcher.trigger(ecs::EvDungeonPrepare { static_cast<uint32_t>(mapIndex) });
        return 0;
    }

    int retry = d->GetFlag("val_f2_retry");
    if (retry < 10)
    {
        d->SetFlag("val_f2_retry", retry + 1);
        s_val.ScheduleCheckFloor2(mapIndex, 1);
    }

    return 0;
}

// floor1 -> floor2 delay event (10 mp)
EVENTFUNC(val_dungeon_to_floor2_event)
{
    auto* info = dynamic_cast<val_dungeon_event_info*>(event->info);
    if (!info)
        return 0;

    const int32_t mapIndex = info->mapIndex;
    s_val.m_evToF2.erase(mapIndex);

    LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIndex);
    if (!d)
        return 0;

    if (d->GetFlag("val_done") != 0)
        return 0;

    if (d->GetFlag("val_floor") != 1)
        return 0;

    s_val.StartFloor2(mapIndex);
    g_dispatcher.trigger(ecs::EvDungeonPrepare { static_cast<uint32_t>(mapIndex) });
    return 0;
}

void CValentineDungeonImpl::SchedulePrepare(int32_t mapIndex, int32_t seconds)
{
    CancelEvent(m_evPrepare, mapIndex);
    auto* info = AllocEventInfo<val_dungeon_event_info>();
    info->mapIndex = mapIndex;
    m_evPrepare[mapIndex] = event_create(val_dungeon_prepare_event, info, PASSES_PER_SEC(seconds));
}

void CValentineDungeonImpl::ScheduleExit(int32_t mapIndex, int32_t seconds)
{
    CancelEvent(m_evExit, mapIndex);
    auto* info = AllocEventInfo<val_dungeon_event_info>();
    info->mapIndex = mapIndex;
    m_evExit[mapIndex] = event_create(val_dungeon_exit_event, info, PASSES_PER_SEC(seconds));
}

void CValentineDungeonImpl::ScheduleCheckFloor2(int32_t mapIndex, int32_t seconds)
{
    CancelEvent(m_evCheck, mapIndex);
    auto* info = AllocEventInfo<val_dungeon_event_info>();
    info->mapIndex = mapIndex;
    m_evCheck[mapIndex] = event_create(val_dungeon_check_event, info, PASSES_PER_SEC(seconds));
}

void CValentineDungeonImpl::ScheduleToFloor2(int32_t mapIndex, int32_t seconds)
{
    CancelEvent(m_evToF2, mapIndex);
    auto* info = AllocEventInfo<val_dungeon_event_info>();
    info->mapIndex = mapIndex;
    m_evToF2[mapIndex] = event_create(val_dungeon_to_floor2_event, info, PASSES_PER_SEC(seconds));
}

// ------------------ Public facade ------------------

CValentineDungeon& CValentineDungeon::instance()
{
    static CValentineDungeon inst;
    return inst;
}

bool CValentineDungeon::IsValentineDungeonMap(int32_t mapIndex) const
{
    return IsInRange(mapIndex, kPrivateMin, kPrivateMax);
}

void CValentineDungeon::OnPlayerDisconnect(CHARACTER* ch)
{
    if (!ch || !ch->IsPC())
        return;

    const int32_t idx = ch->GetMapIndex();
    if (!IsValentineDungeonMap(idx))
        return;

    ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), "valentine_dungeon.disconnect", get_global_time() + kRejoinSeconds);
    ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), "valentine_dungeon.idx", idx);
    ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), "valentine_dungeon.ch", (int32_t)g_bChannel);
}

void CValentineDungeon::OnPlayerLogin(CHARACTER* ch)
{
    if (!ch || !ch->IsPC())
        return;

    const int32_t idx = ch->GetMapIndex();
    if (!IsValentineDungeonMap(idx))
        return;

    LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
    if (!d)
    {
        ch->ExitToSavedLocation();
        return;
    }

    ch->SetDungeon(d);

    // Task reminder on login (rejoin)
    if (d->GetFlag(kFlagCompleted) == 0)
    {
        const int floor = d->GetFlag(kFlagFloor);
        if (floor == 1)
        {
            ChatToChar(ch, "Valentine: Task #1: Destroy all stones. Remaining: %d", d->GetFlag(kFlagStep));
        }
        else if (floor == 2)
        {
            const int metins = CountMobVnumOnMap(idx, kFloor2MetinVnum);
            const int mobs = CountMobVnumOnMap(idx, kFloor2MobVnum);
           // ChatToChar(ch, "Valentine: Task #2: Metins + monsters. Metins left: %d, monsters left: %d", metins, mobs);
        }
        else if (floor == 3)
        {
            ChatToChar(ch, "Valentine: Task #3: Kill the boss!");
        }
    }
    else
    {
        ChatToChar(ch, "Valentine: Dungeon complete. Click the NPC to start again.");
    }

    if (d->GetFlag(kFlagFloor) == 0)
        s_val.SchedulePrepare(idx, 1);
}

void CValentineDungeon::OnMobKilled(CHARACTER* killer, CHARACTER* victim)
{
    if (!killer || !victim)
        return;
    if (!killer->IsPC())
        return;

    if (!(victim->IsMonster() || victim->IsStone()))
        return;

    const int32_t idx = victim->GetMapIndex();
    if (!IsValentineDungeonMap(idx))
        return;

    LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
    if (!d)
        return;

    const uint32_t vnum = victim->GetRaceNum();
    const int32_t floor = d->GetFlag(kFlagFloor);

    // ---------------- Floor 1: stones countdown -> floor2 10 mp kesleltetessel ----------------
    if (vnum == kFloor1StoneVnum && floor == 1)
    {
        int32_t s = d->GetFlag(kFlagStep) - 1;
        if (s < 0)
            s = 0;
        d->SetFlag(kFlagStep, s);

        ChatToMap(idx, "Valentin: Hatralevo kov: %d", s);

        if (s == 0)
        {
            if (d->GetFlag(kFlagF1ToF2) == 0)
            {
                d->SetFlag(kFlagF1ToF2, 1);
                ChatToMap(idx, "Valentin: Kesz! 10 mp mulva indul a 2. emelet.");
                s_val.ScheduleToFloor2(idx, 10);
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

        //ChatToChar(killer, "Valentine: Metins left: %d, monsters left: %d", metins, mobs);

        d->SetFlag(kFlagF2Retry, 0);
        s_val.ScheduleCheckFloor2(idx, 1);
        return;
    }

    // ---------------- Boss ----------------
    if (vnum == kBossVnum && floor == 3)
    {
        s_val.Complete(idx);
        return;
    }
}

bool CValentineDungeon::OnClickNpc(CHARACTER* ch)
{
    if (!ch || !ch->IsPC())
        return false;

    if (!ch->CanWarp())
        return true;

    const int32_t mapIdx = ch->GetMapIndex();

    // If clicked inside the dungeon while run is active -> exit to saved location.
    if (IsValentineDungeonMap(mapIdx))
    {
        LPDUNGEON cur = CDungeonManager::instance().FindByMapIndex(mapIdx);
        if (cur && cur->GetFlag(kFlagCompleted) == 0)
        {
            ch->ExitToSavedLocation();
            return true;
        }
        // completed -> allow starting a new run from the same NPC
    }

    const int32_t now = get_global_time();

    // Rejoin flow
    const int32_t rejoinUntil = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), "valentine_dungeon.disconnect");
    if (rejoinUntil > now)
    {
        const int32_t rejoinIdx = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), "valentine_dungeon.idx");
        const int32_t rejoinCh = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), "valentine_dungeon.ch");

        if (rejoinIdx >= kPrivateMin && rejoinIdx < kPrivateMax)
        {
            if (rejoinCh != 0 && rejoinCh != (int32_t)g_bChannel)
            {
                ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "You were in Valentine Dungeon on a different channel. Channel: %d", rejoinCh);
                return true;
            }

            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(rejoinIdx);
            if (d && d->GetFlag(kFlagCompleted) == 0)
            {
                ch->SaveExitLocation();
                ch->WarpSet(kEnterX * 100, kEnterY * 100, rejoinIdx);
                return true;
            }
        }
    }

    // Level check
    if (kMinLevel > 0 && ch->GetLevel() < kMinLevel)
    {
        ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Valentine: minimum level is %d.", kMinLevel);
        return true;
    }
    if (kMaxLevel > 0 && ch->GetLevel() > kMaxLevel)
    {
        ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Valentine: maximum level is %d.", kMaxLevel);
        return true;
    }

    // Cooldown
    const int32_t cdUntil = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), "valentine_dungeon.cooldown");
    if (cdUntil > now)
    {
        const int32_t remain = cdUntil - now;
        ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Valentine: you must wait %d seconds.", remain);
        return true;
    }

    LPPARTY party = ch->GetParty();
    if (party && party->GetLeaderPID() != ch->GetPlayerID())
    {
        ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Only the party leader can start Valentine Dungeon.");
        return true;
    }

    // Party level check: everyone who will enter must meet level requirement (same map as leader)
    if (party)
    {
        bool ok = true;
        const char* badName = nullptr;
        int32_t badLevel = 0;

        ForEachPcOnMap(ch->GetMapIndex(), [&](LPCHARACTER m) {
            if (!ok || !m || !m->IsPC() || m->GetParty() != party)
                return;

            if ((kMinLevel > 0 && m->GetLevel() < kMinLevel) || (kMaxLevel > 0 && m->GetLevel() > kMaxLevel))
            {
                ok = false;
                badName = m->GetName();
                badLevel = m->GetLevel();
                return;
            }
        });

        if (!ok)
        {
            ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "%s has an invalid level (Lv%d). Required: %d-%d.", badName ? badName : "A party member", badLevel, kMinLevel, kMaxLevel);
            return true;
        }
    }

    // Party cooldown: everyone must be off cooldown
    if (party)
    {
        FCooldownCheck f(now, "valentine_dungeon.cooldown");
        ForEachPcOnMap(ch->GetMapIndex(), [&](LPCHARACTER m) {
            if (!m || !m->IsPC() || m->GetParty() != party)
                return;
            f(m);
        });
if (!f.ok)
        {
            ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "%s is still on cooldown (%d seconds).", f.name ? f.name : "valaki", f.remain);
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
            ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Valentine: required to enter: %s (x1).", entryItemName);
            return true;
        }
    }
    else
    {
        FEntryItemCheck it(kEntryItemVnum);
        ForEachPcOnMap(ch->GetMapIndex(), [&](LPCHARACTER m) {
            if (!m || !m->IsPC() || m->GetParty() != party)
                return;
            it(m);
        });
if (!it.ok)
        {
            ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "%s doesn't have the entry item: %s (x1).",
                it.name ? it.name : "valaki", entryItemName);
            return true;
        }
    }

    // Create dungeon instance
    LPDUNGEON d = CDungeonManager::instance().Create(kValOriginalMap);
    if (!d)
    {
        ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Valentine: failed to create the dungeon.");
        return true;
    }

    // Initialize dungeon flags
    d->SetFlag(kFlagFloor, 0);
    d->SetFlag(kFlagCompleted, 0);
    d->SetFlag(kFlagStep, 0);
    d->SetFlag(kFlagBossVid, 0);
    d->SetFlag(kFlagIsParty, party ? 1 : 0);
    d->SetFlag(kFlagLeaderPid, (int32_t)ch->GetPlayerID());
    d->SetFlag(kFlagF2Retry, 0);
    d->SetFlag(kFlagF1ToF2, 0);

    // Set per-player rejoin flags + consume entry item
    auto applyMember = [&](LPCHARACTER m)
        {
            if (!m || !m->IsPC())
                return;

            // Consume entry item (already checked above)
            m->RemoveSpecifyItem(kEntryItemVnum, 1);

            ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(m), "valentine_dungeon.disconnect", 0);
            ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(m), "valentine_dungeon.idx", d->GetMapIndex());
            ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(m), "valentine_dungeon.ch", (int32_t)g_bChannel);
            ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(m), "valentine_dungeon.enter_time", now);
        };

    if (!party)
    {
        applyMember(ch);
        d->Join_Coords(ch, kEnterX, kEnterY, kValOriginalMap);
    }
    else
    {
        ForEachPcOnMap(ch->GetMapIndex(), [&](LPCHARACTER m) {
            if (!m || !m->IsPC() || m->GetParty() != party)
                return;
            applyMember(m);
        });
        d->JoinParty_Coords(party, kEnterX, kEnterY, ch->GetMapIndex());
    }

    // Small hint right after enter
    ChatToChar(ch, "Valentine: The dungeon is starting. Please wait 1 second...");

    // Start floor1 after short delay
    s_val.SchedulePrepare(d->GetMapIndex(), kPrepareDelay);
    return true;
}

