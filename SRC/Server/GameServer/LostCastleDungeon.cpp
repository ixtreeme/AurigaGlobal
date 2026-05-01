// LostCastleDungeon.cpp
#include "stdafx.h"
#include <Core/Logging.hpp>
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/SocialSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "ecs/systems/QuestSystem.hpp"
#include "LostCastleDungeon.h"

#include <unordered_map>
#include <vector>
#include <string>
#include <functional>
#include <cstdarg>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <utility>

#include "char_interface.hpp"
#include "char_manager.h"
#include "config.h"
#include "desc.h"
#include "party.h"
#include "sectree_manager.h"
#include "dungeon.h"
#include "item.h"
#include "item_manager.h"
#include "packet.h"
#include "log.h"
#include "ecs/systems/ItemSystem.hpp"
#include "ecs/Registry.hpp"
#include "ecs/EntityFactory.hpp"
#include "mob_manager.h"
#include "pvp.h"
#include "battle.h" 
#include "skill.h"
#include "motion.h"
namespace
{
#define STR_MULTIPLE 50 //player statja szorozva ennyivel lesz a klon statja (HP/SP/DMG/STAT),
    // ---------------- CONFIG ----------------
    constexpr int32_t  kOriginalMap = 380;
    constexpr int32_t  kPrivateMin = 3800000;
    constexpr int32_t  kPrivateMax = 3810000;

    constexpr uint32_t kEntryNpcVnum = 20021;
    constexpr uint32_t kEntryItemVnum = 30000;

    constexpr int32_t  kMinLevel = 40;
    constexpr int32_t  kMaxLevel = 140;

    // BasePosition: 307200 / 332800  -> base global cell: 3072 / 3328
    constexpr int32_t kBaseCellX = 3072;
    constexpr int32_t kBaseCellY = 3328;

    // Debug: Local Position (281 x 112)  -> local cell
    constexpr int32_t kEnterLocalX = 281;
    constexpr int32_t kEnterLocalY = 112;

    // JOIN coords must be GLOBAL CELL on your core (same as /warp)
    constexpr int32_t kJoinGlobalX = kBaseCellX + kEnterLocalX; // 3353
    constexpr int32_t kJoinGlobalY = kBaseCellY + kEnterLocalY; // 3440

    // Floor1
    constexpr uint32_t kMetinVnum = 8001;
    constexpr int32_t  kFloor1TimeSec = 600;
    constexpr int32_t  kFloor1MetinCount = 2;

    // Spawn metins around the LOCAL entry to avoid invalid coords
    constexpr int32_t  kMetinRadiusX = 350;
    constexpr int32_t  kMetinRadiusY = 350;

    // Floor2
    constexpr uint32_t kStatueVnum = 20433;
    constexpr int32_t  kFloor2TimeSec = 180;
    constexpr int32_t  kFloor2CenterX = kEnterLocalX;
    constexpr int32_t  kFloor2CenterY = kEnterLocalY;
    constexpr const char* kFloor2Regen = "data/dungeon/elveszett_kastely/floor2.txt";
    constexpr uint32_t kKeyItems[5] = { 30001, 30002, 30003, 30004, 30005 };

    // Floor3
    constexpr uint32_t kCloneBaseMobVnum = 136;
    constexpr int32_t  kFloor3CenterX = kEnterLocalX;
    constexpr int32_t  kFloor3CenterY = kEnterLocalY;

    // Floor4
    constexpr const char* kFloor4Regen = "data/dungeon/elveszett_kastely/floor4.txt";
    constexpr uint32_t kTotemVnum = 9298;
    constexpr uint32_t kTileItemVnum = 30007;
    constexpr int32_t  kTileDropChancePct = 3;
    constexpr int32_t  kFloor4CenterX = kEnterLocalX;
    constexpr int32_t  kFloor4CenterY = kEnterLocalY;

    constexpr int32_t  kTileStages = 10;

    // Dungeon flags
    constexpr const char* kFlagFloor = "lc_floor";
    constexpr const char* kFlagWasCompleted = "lc_done";
    constexpr const char* kFlagCorrectMetin = "lc_metin_vid";
    constexpr const char* kFlagStatueVid = "lc_statue_vid";
    constexpr const char* kFlagKeyMask = "lc_key_mask";
    constexpr const char* kFlagTotemVid = "lc_totem_vid";
    constexpr const char* kFlagTileStage = "lc_tile_stage";
    constexpr const char* kFlagClonesRemain = "lc_clones_remain";

    // Rejoin quest flags (per character)
    constexpr const char* kQfDisconnect = "lostcastle.disconnect";
    constexpr const char* kQfIdx = "lostcastle.idx";
    constexpr const char* kQfCh = "lostcastle.ch";

    constexpr int32_t kRejoinSeconds = 300;

    inline bool IsInRange(int32_t v, int32_t lo, int32_t hi) { return v >= lo && v < hi; }

    // FIX metin pozíciók (CELL koordináta a mapen belül)
    constexpr int32_t kMetinPos[][2] = {
        { 300, 120 }, { 320, 150 }, { 340, 180 }, { 360, 210 },
        { 380, 240 }, { 400, 270 }, { 420, 300 }, { 440, 330 },
        // ... ide írsz annyit, amennyit akarsz
    };

    constexpr int32_t kMetinPosCount = (int32_t)(sizeof(kMetinPos) / sizeof(kMetinPos[0]));

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

    inline void BigNoticeMap(int32_t mapIndex, const char* fmt, ...)
    {
        char buf[CHAT_MAX_LEN + 64];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);

        ForEachPcOnMap(mapIndex, [&](LPCHARACTER pc) {
            if (pc)
                ecs::ChatSystem::Send(AIHelpers::EcsOf(pc), CHAT_TYPE_BIG_NOTICE, "%s", buf);
            });
    }

    inline void SendCommandMap(int32_t mapIndex, const char* fmt, ...)
    {
        char buf[CHAT_MAX_LEN + 64];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);

        ForEachPcOnMap(mapIndex, [&](LPCHARACTER pc) {
            if (pc && ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(pc)))
                ecs::ChatSystem::Send(AIHelpers::EcsOf(pc), CHAT_TYPE_COMMAND, "%s", buf);
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
        pos.x = victim->GetX() + number(-200, 200);
        pos.y = victim->GetY() + number(-200, 200);
        pos.z = victim->GetZ();

        item->AddToGround(victim->GetMapIndex(), pos);
        item->StartDestroyEvent();

        if (owner)
            item->SetOwnership(owner, 60 * 3);
    }

    bool ConsumeOneGivenItem(entt::entity itemEntity, const char* reason)
    {
        if (itemEntity == entt::null)
            return false;

        if (ItemSystem::GetItemCount(itemEntity) > 1)
        {
            ItemSystem::ConsumeItemEcs(itemEntity);
            return true;
        }

        ItemSystem::DestroyItemEntityEcs(itemEntity, reason);
        return true;
    }

    // Block rect (cell coords) - TODO: set real block rects for your floor4 map
    struct Rect { int32_t sx, sy, ex, ey; };

    const Rect kBlockRects[kTileStages] =
    {
        { 1200, 1100, 1220, 1120 },
        { 1230, 1100, 1250, 1120 },
        { 1260, 1100, 1280, 1120 },
        { 1290, 1100, 1310, 1120 },
        { 1320, 1100, 1340, 1120 },
        { 1350, 1100, 1370, 1120 },
        { 1380, 1100, 1400, 1120 },
        { 1410, 1100, 1430, 1120 },
        { 1440, 1100, 1460, 1120 },
        { 1470, 1100, 1490, 1120 },
    };

    bool ApplyBlockRect(int32_t mapIndex, const Rect& r, bool block)
    {
        LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(mapIndex);
        if (!map)
            return false;

        const int32_t sx = map->m_setting.iBaseX + r.sx * 100;
        const int32_t sy = map->m_setting.iBaseY + r.sy * 100;
        const int32_t ex = map->m_setting.iBaseX + r.ex * 100;
        const int32_t ey = map->m_setting.iBaseY + r.ey * 100;

        return SECTREE_MANAGER::instance().ForAttrRegion(
            mapIndex,
            sx, sy, ex, ey,
            0,
            ATTR_BLOCK,
            block ? ATTR_REGION_MODE_SET : ATTR_REGION_MODE_REMOVE
        );
    }

    void SendAdditionalInfo(LPCHARACTER viewer, LPCHARACTER target, const char* name, const uint16_t parts[CHR_EQUIPPART_NUM])
    {
        if (!viewer || !ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(viewer)) || !target)
            return;

        TPacketGCCharacterAdditionalInfo p;
        memset(&p, 0, sizeof(p));
        p.header = HEADER_GC_CHAR_ADDITIONAL_INFO;
			p.dwVID = ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(target));
        strlcpy(p.name, name ? name : target->GetName(), sizeof(p.name));
        for (int i = 0; i < CHR_EQUIPPART_NUM; ++i)
            p.awPart[i] = parts ? parts[i] : 0;

        ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(viewer))->Packet(&p, sizeof(p));
    }

    void SendAdditionalInfoToMap(int32_t mapIndex, LPCHARACTER target, const char* name, const uint16_t parts[CHR_EQUIPPART_NUM])
    {
        ForEachPcOnMap(mapIndex, [&](LPCHARACTER pc) {
            SendAdditionalInfo(pc, target, name, parts);
            });
    }

    // ---------------- LostCastle clone helpers (anim + move + timed hit) ----------------
    inline int64_t ClampMul10(int64_t v)
    {
        // sanity: do not overflow (and keep within int32-ish ranges where some code still casts)
        const int64_t maxV = 2000000000LL;
        if (v > maxV) return maxV;
        if (v < -maxV) return -maxV;
        return v;
    }

    inline void MulPoint10(LPCHARACTER ch, uint8_t pt)
    {
        if (!ch)
            return;
        const int64_t v = ch->GetPoint(pt);
        const int64_t nv = ClampMul10(v * STR_MULTIPLE);
        ch->SetRealPoint(pt, nv);
        ch->SetPoint(pt, nv);
    }

    inline uint32_t CalcMeleeHitDelayMs(LPCHARACTER ch, uint16_t motionIndex)
    {
        if (!ch)
            return 220;

        const uint32_t mode = ch->GetMotionMode();
        const float durSec = CMotionManager::instance().GetMotionDuration(ch->GetRaceNum(), MAKE_MOTION_KEY(mode, motionIndex));
        uint32_t durMs = (durSec > 0.01f) ? (uint32_t)(durSec * 1000.0f) : 650;

        // Hit generally lands early-mid swing
        uint32_t hit = (durMs * 35) / 100;
        if (hit < 140) hit = 140;
        if (hit > 380) hit = 380;
        return hit;
    }

    inline uint32_t CalcAttackIntervalMs(LPCHARACTER ch)
    {
        if (!ch)
            return 700;

        // ATT_SPEED is typically 0..200
        const int as = ch->GetLimitPoint(POINT_ATT_SPEED);
        int interval = 900 - as * 3;
        if (interval < 350) interval = 350;
        if (interval > 900) interval = 900;
        return (uint32_t)interval;
    }


    inline void CloneEquipWeaponFromSource(LPCHARACTER clone, LPCHARACTER source)
    {
        if (!clone || !source)
            return;

        // A legtobb PvP skill ellenorzi a WEAR_WEAPON-t (es a weapon tipust),
        // ezert a klonnak is legyen valodi fegyver itemje, nem csak vizualis PART.
        LPITEM srcW = source->GetWear(WEAR_WEAPON);
        if (!srcW)
            return;

        if (clone->GetWear(WEAR_WEAPON))
            return;

		// RefineLevel a legtobb forrasban a vnum-bol szamolodik, igy eleg a megfelelo vnum-ot klonozni.
		// (Nincs SetRefineLevel API nalatok.)
		const uint32_t vnum = srcW->GetOriginalVnum() ? srcW->GetOriginalVnum() : ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, srcW));

		LPITEM w = ITEM_MANAGER::instance().CreateItem(vnum, 1, 0, true);
        if (!w)
            return;

		// klon item: ne menjen DB save/DelayedSave
		ItemSystem::SetItemSkipSave(EntityFactory::CreateItemEntity(g_registry, w), true);

		// Sockets + Attributes masolasa publikus API-val
		w->SetSockets(srcW->GetSockets());
		w->SetAttributes(srcW->GetAttributes());

        // Equip without inventory (direct wear)
        w->EquipTo(clone, WEAR_WEAPON);
    }

    inline void LostCastleCloneStartMove(LPCHARACTER clone, int32_t tx, int32_t ty, uint32_t now)
    {
        if (!clone)
            return;

        if (clone->GetX() == tx && clone->GetY() == ty)
            return;

        clone->StartStateMachine(1);
        clone->SetNowWalking(false);
        clone->SetRotationToXY(tx, ty);
        clone->Goto(tx, ty);

        // server-controlled chars need explicit MOVE packets
        clone->SendMovePacket(FUNC_MOVE, 0, tx, ty, clone->GetCurrentMoveDuration(), now);
    }

    inline void LostCastleCloneBroadcastMelee(LPCHARACTER clone, LPCHARACTER target, uint8_t motionIndex, uint32_t now)
    {
        if (!clone || !target)
            return;

        clone->SetRotationToXY(target->GetX(), target->GetY());
        clone->Stop();

        // PC swing is broadcast as FUNC_COMBO with motion index (13..21)
        clone->SendMovePacket(FUNC_COMBO, motionIndex, clone->GetX(), clone->GetY(), 0, now);
        clone->OnMove(true);
    }

    inline void LostCastleCloneBroadcastSkill(LPCHARACTER clone, LPCHARACTER target, uint8_t skillVnum, uint32_t now)
    {
        if (!clone || !target)
            return;

        clone->SetRotationToXY(target->GetX(), target->GetY());
        clone->Stop();

        // Skills are broadcast as FUNC_SKILL|skillVnum (see input_main.cpp)
        const uint8_t func = (uint8_t)(FUNC_SKILL | (skillVnum & 0x7F));
        clone->SendMovePacket(func, 0, clone->GetX(), clone->GetY(), 0, now);
        clone->OnMove(true);
    }
}

// ---------------- Events + impl ----------------
namespace
{
    EVENTINFO(lostcastle_timer_event_info)
    {
        int32_t mapIndex;
        int32_t remainSec;
        int32_t floor;
        lostcastle_timer_event_info() : mapIndex(0), remainSec(0), floor(0) {}
    };

    EVENTFUNC(lostcastle_timer_event);

    EVENTINFO(lostcastle_clone_ai_event_info)
    {
        int32_t mapIndex;
        lostcastle_clone_ai_event_info() : mapIndex(0) {}
    };

    EVENTFUNC(lostcastle_clone_ai_event);

    struct SClonePending
    {
        uint32_t targetVid;
        uint8_t  attackType;   // 0 = melee, else skill vnum
        uint32_t executeTime;  // ms (get_dword_time)
        uint8_t  motionArg;    // melee motion (13..21) or skill vnum
        bool     isSkill;
        SClonePending() : targetVid(0), attackType(0), executeTime(0), motionArg(0), isSkill(false) {}
    };

    class CLostCastleDungeonImpl
    {
    public:
        std::unordered_map<int32_t, LPEVENT> m_evTimer;

        std::unordered_map<uint32_t, uint32_t> m_cloneAllowedPid; // clone vid -> allowed attacker pid
        std::unordered_map<uint32_t, int32_t>  m_cloneMap;        // clone vid -> mapIndex


        // clone vid -> target vid (owner)
        std::unordered_map<uint32_t, uint32_t> m_cloneTargetVid;
        // clone vid -> available ATTACK skill vnums (<=127 so we can broadcast skill motion)
        std::unordered_map<uint32_t, std::vector<uint8_t>> m_cloneSkills;

        // clone vid -> pending hit (to sync damage with swing animation)
        std::unordered_map<uint32_t, SClonePending> m_clonePending;

        // clone vid -> fixed approach offset to avoid jitter / running around
        std::unordered_map<uint32_t, std::pair<int16_t, int16_t>> m_cloneOffset;

        // clone vid -> next allowed action time (ms)
        std::unordered_map<uint32_t, uint32_t> m_cloneNextAction;

        // per-map AI tick event for clones (skill usage + aggression)
        std::unordered_map<int32_t, LPEVENT> m_evCloneAI;


        void CancelTimer(int32_t mapIndex)
        {
            auto it = m_evTimer.find(mapIndex);
            if (it == m_evTimer.end())
                return;
            event_cancel(&it->second);
            m_evTimer.erase(it);
        }

        
        void CancelCloneAI(int32_t mapIndex)
        {
            auto it = m_evCloneAI.find(mapIndex);
            if (it == m_evCloneAI.end())
                return;

            event_cancel(&it->second);
            m_evCloneAI.erase(it);
        }

        void ScheduleCloneAI(int32_t mapIndex)
        {
            // already scheduled
            if (m_evCloneAI.count(mapIndex))
                return;

            auto* info = AllocEventInfo<lostcastle_clone_ai_event_info>();
            info->mapIndex = mapIndex;
            // faster tick so swing->damage sync feels like real PvP
            int tick = PASSES_PER_SEC(1) / 10;
            if (tick < 1) tick = 1;
            m_evCloneAI[mapIndex] = event_create(lostcastle_clone_ai_event, info, tick);
        }
void ClearClonesOnMap(int32_t mapIndex)
        {
            std::vector<uint32_t> toRemove;
            for (auto& kv : m_cloneMap)
                if (kv.second == mapIndex)
                    toRemove.push_back(kv.first);

            for (uint32_t vid : toRemove)
            {
                m_cloneMap.erase(vid);
                m_cloneAllowedPid.erase(vid);
                m_cloneTargetVid.erase(vid);
                m_cloneSkills.erase(vid);
                m_clonePending.erase(vid);
                m_cloneNextAction.erase(vid);
                m_cloneOffset.erase(vid);
                if (LPCHARACTER c = CHARACTER_MANAGER::instance().Find(vid))
                    M2_DESTROY_CHARACTER(c);
            }
        }

        void ClearDungeon(int32_t mapIndex)
        {
            CancelTimer(mapIndex);
            CancelCloneAI(mapIndex);
            ClearClonesOnMap(mapIndex);

            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIndex);
            if (!d)
                return;

            d->SetFlag(kFlagWasCompleted, 1);
            d->KillAll();
            d->ClearRegen();

            for (int i = 0; i < kTileStages; ++i)
                ApplyBlockRect(mapIndex, kBlockRects[i], false);

            d->ExitAllLobby(1);
        }

        void ScheduleTimer(int32_t mapIndex, int32_t remainSec, int32_t floor)
        {
            CancelTimer(mapIndex);

            auto* info = AllocEventInfo<lostcastle_timer_event_info>();
            info->mapIndex = mapIndex;
            info->remainSec = remainSec;
            info->floor = floor;

            m_evTimer[mapIndex] = event_create(lostcastle_timer_event, info, PASSES_PER_SEC(10));
        }


        void StartFloor1(int32_t mapIndex)
        {
            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIndex);
            if (!d)
                return;

            d->SetFlag(kFlagFloor, 1);
            d->SetFlag(kFlagCorrectMetin, 0);

            std::vector<uint32_t> spawnedVids;
            spawnedVids.reserve(kFloor1MetinCount);

            // ha kevesebb fix pontod van, mint 200, akkor körbe ismétli
            for (int i = 0; i < kFloor1MetinCount; ++i)
            {
                const int idx = i % kMetinPosCount;
                const int32_t x = kMetinPos[idx][0];
                const int32_t y = kMetinPos[idx][1];

                LPCHARACTER metin = d->SpawnMob((int32_t)kMetinVnum, x, y);
                if (!metin)
                {
                    LOG_ERROR("[LostCastle] metin spawn fail: vnum={} map={} x={} y={} (i={} idx={})", (unsigned)kMetinVnum, mapIndex, x, y, i, idx);
                    continue;
                }

		spawnedVids.push_back(ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(metin)));
            }

            if (spawnedVids.empty())
            {
                LOG_ERROR("[LostCastle] failed to spawn any metins (fixed positions). map={}", mapIndex);
                return;
            }

            const uint32_t correctVid = spawnedVids[number(0, (int)spawnedVids.size() - 1)];
            d->SetFlag(kFlagCorrectMetin, (int32_t)correctVid);

            BigNoticeMap(mapIndex, "Elveszett Kastely: %d mp marad meg a megfelelõ metinkõ megtalalasara!", kFloor1TimeSec);
            ScheduleTimer(mapIndex, kFloor1TimeSec, 1);
        }


        //void StartFloor1(int32_t mapIndex)
        //{
        //    LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIndex);
        //    if (!d)
        //        return;

        //    d->SetFlag(kFlagFloor, 1);
        //    d->SetFlag(kFlagCorrectMetin, 0);

        //    std::vector<uint32_t> spawnedVids;
        //    spawnedVids.reserve(kFloor1MetinCount);

        //    const int32_t minX = std::max(1, kEnterLocalX - kMetinRadiusX);
        //    const int32_t maxX = std::max(minX + 1, kEnterLocalX + kMetinRadiusX);
        //    const int32_t minY = std::max(1, kEnterLocalY - kMetinRadiusY);
        //    const int32_t maxY = std::max(minY + 1, kEnterLocalY + kMetinRadiusY);

        //    int32_t attempts = 0;
        //    const int32_t maxAttempts = kFloor1MetinCount * 60;

        //    while ((int32_t)spawnedVids.size() < kFloor1MetinCount && attempts < maxAttempts)
        //    {
        //        ++attempts;
        //        const int32_t x = number(minX, maxX);
        //        const int32_t y = number(minY, maxY);

        //        LPCHARACTER metin = d->SpawnMob((int32_t)kMetinVnum, x, y);
        //        if (!metin)
        //            continue;

        //        spawnedVids.push_back(metin->GetVID());
        //    }

        //    if (spawnedVids.empty())
        //    {
        //        "[LostCastle] failed to spawn any metins map=%d localRect=(%d,%d)-(%d,%d)", mapIndex, minX, minY, maxX, maxY);
        //        return;
        //    }

        //    const uint32_t correctVid = spawnedVids[number(0, (int)spawnedVids.size() - 1)];
        //    d->SetFlag(kFlagCorrectMetin, (int32_t)correctVid);

        //    BigNoticeMap(mapIndex, "Elveszett Kastely: %d mp marad meg a megfelelõ metinkõ megtalalasara!", kFloor1TimeSec);
        //    ScheduleTimer(mapIndex, kFloor1TimeSec, 1);
        //}

        void StartFloor2(int32_t mapIndex)
        {
            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIndex);
            if (!d)
                return;

            CancelTimer(mapIndex);
            CancelCloneAI(mapIndex);
            ClearClonesOnMap(mapIndex);

            d->KillAll();
            d->ClearRegen();

            d->SetFlag(kFlagFloor, 2);
            d->SetFlag(kFlagKeyMask, 0);
            d->SetFlag(kFlagStatueVid, 0);

            d->JumpAll(mapIndex, kFloor2CenterX, kFloor2CenterY);

            LPCHARACTER statue = d->SpawnMob((int32_t)kStatueVnum, kFloor2CenterX, kFloor2CenterY);
            if (statue)
	d->SetFlag(kFlagStatueVid, (int32_t)ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(statue)));

            d->SpawnRegen(kFloor2Regen, true);

            BigNoticeMap(mapIndex, "Elveszett Kastely: %d mp van a szobor aktivitasara (5 kulcs)!", kFloor2TimeSec);
            ScheduleTimer(mapIndex, kFloor2TimeSec, 2);
        }
    void StartFloor3(int32_t mapIndex)
    {
        LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIndex);
        if (!d)
            return;

        CancelTimer(mapIndex);

        d->KillAll();
        d->ClearRegen();

        d->SetFlag(kFlagFloor, 3);
        d->SetFlag(kFlagClonesRemain, 0);

        d->JumpAll(mapIndex, kFloor3CenterX, kFloor3CenterY);

        LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(mapIndex);
        if (!map)
            return;

        const int32_t baseCellX = map->m_setting.iBaseX / 100;
        const int32_t baseCellY = map->m_setting.iBaseY / 100;

        std::vector<LPCHARACTER> members;
        members.reserve(8);
        ForEachPcOnMap(mapIndex, [&](LPCHARACTER pc) { if (pc) members.push_back(pc); });

        std::sort(members.begin(), members.end(), [](LPCHARACTER a, LPCHARACTER b) {
            return ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(a)) < ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(b));
        });

        if (members.empty())
            return;

        // clear old
        CancelCloneAI(mapIndex);
        ClearClonesOnMap(mapIndex);

        int32_t remain = 0;
        constexpr int kMinStep = 2;
        constexpr int kMaxStep = 5;

        for (size_t i = 0; i < members.size(); ++i)
        {
            LPCHARACTER owner  = members[i];
            LPCHARACTER source = members[(i + 1) % members.size()];
            if (!owner || !source)
                continue;

            // owner GLOBAL cell -> LOCAL cell (private map uses LOCAL in many APIs)
            const int32_t ox = (owner->GetX() / 100) - baseCellX;
            const int32_t oy = (owner->GetY() / 100) - baseCellY;

            int32_t sx = ox, sy = oy;
            for (int t = 0; t < 30; ++t)
            {
                int dx = 0, dy = 0;
                switch (number(0, 7))
                {
                    case 0: dx =  1; dy =  0; break;
                    case 1: dx = -1; dy =  0; break;
                    case 2: dx =  0; dy =  1; break;
                    case 3: dx =  0; dy = -1; break;
                    case 4: dx =  1; dy =  1; break;
                    case 5: dx =  1; dy = -1; break;
                    case 6: dx = -1; dy =  1; break;
                    case 7: dx = -1; dy = -1; break;
                }

                const int step = number(kMinStep, kMaxStep);
                sx = ox + dx * step;
                sy = oy + dy * step;

                const int32_t gxTry = map->m_setting.iBaseX + sx * 100;
                const int32_t gyTry = map->m_setting.iBaseY + sy * 100;
                if (SECTREE_MANAGER::instance().Get(mapIndex, gxTry, gyTry))
                    break;
            }

            const int32_t gx = map->m_setting.iBaseX + sx * 100;
            const int32_t gy = map->m_setting.iBaseY + sy * 100;

            char evilName[CHARACTER_NAME_MAX_LEN + 1];
            snprintf(evilName, sizeof(evilName), "Gonosz%s", source->GetName());

            // FAKE PC (nem mob!) - PID=0, hogy ne keruljon bele PC name/PID map-ekbe
            LPCHARACTER clone = CHARACTER_MANAGER::instance().CreateCharacter(evilName, 0);
            if (!clone)
                continue;

            //clone->SetCharType(CHAR_TYPE_PC);
        //    clone->SetFakePlayer(true);
            clone->SetName(std::string(evilName));

            // Fontos: legyen PC race/job/empire/PK mode, hogy a kliens PvP-kent kezelje
            clone->SetRace((uint8_t)source->GetRaceNum());
            clone->SetEmpire(ecs::PlayerRuntime::GetEmpire(AIHelpers::EcsOf(source)));
            clone->SetPKMode(PK_MODE_FREE);
            clone->SetSkillGroup(source->GetSkillGroup());

            clone->SetRotation(source->GetRotation());
            clone->SetXYZ(gx, gy, 0);
            clone->SetMapIndex(mapIndex);

            // kinézet (partok)
            clone->SetPart(PART_MAIN, source->GetPart(PART_MAIN));
            clone->SetPart(PART_WEAPON, source->GetPart(PART_WEAPON));
            clone->SetPart(PART_HEAD, source->GetPart(PART_HEAD));
            clone->SetPart(PART_HAIR, source->GetPart(PART_HAIR));
#ifdef ENABLE_ACCE_SYSTEM
            clone->SetPart(PART_ACCE, source->GetPart(PART_ACCE));
#endif
#ifdef ENABLE_RUNE_SYSTEM
            clone->SetPart(PART_RUNE, source->GetPart(PART_RUNE));
#endif
#ifdef ENABLE_COSTUME_EFFECT
            clone->SetPart(PART_EFFECT_BODY, source->GetPart(PART_EFFECT_BODY));
            clone->SetPart(PART_EFFECT_WEAPON, source->GetPart(PART_EFFECT_WEAPON));
#endif

            CloneEquipWeaponFromSource(clone, source);

            // erő: pontok másolása
            for (int p = 0; p < POINT_MAX_NUM; ++p)
            {
                clone->SetRealPoint((uint8_t)p, source->GetPoint((uint8_t)p));
                clone->SetPoint((uint8_t)p, source->GetPoint((uint8_t)p));
            }

            // 10x erosites (HP/SP/DMG/STAT)
            clone->SetLevel((uint8_t)source->GetLevel());
            clone->SetMaxHP((int64_t)source->GetMaxHP() * STR_MULTIPLE);
            clone->SetMaxSP((int64_t)source->GetMaxSP() * STR_MULTIPLE);
            clone->SetHP((int64_t)source->GetMaxHP() * STR_MULTIPLE);
            clone->SetSP((int64_t)source->GetMaxSP() * STR_MULTIPLE);

            MulPoint10(clone, POINT_ST);
            MulPoint10(clone, POINT_HT);
            MulPoint10(clone, POINT_DX);
            MulPoint10(clone, POINT_IQ);
            MulPoint10(clone, POINT_ATT_GRADE);
            MulPoint10(clone, POINT_DEF_GRADE);
            MulPoint10(clone, POINT_MAGIC_ATT_GRADE);
            MulPoint10(clone, POINT_MAGIC_DEF_GRADE);
            MulPoint10(clone, POINT_WEAPON_MIN);
            MulPoint10(clone, POINT_WEAPON_MAX);
            clone->SetKillerMode(true);

            // Skillek: csak tamado skillek legyenek az AI listaban (<=127 a skill motion packet miatt)
            std::vector<uint8_t> skillList;
            skillList.reserve(32);
            for (uint32_t sv = 1; sv <= 255; ++sv)
            {
                const int lvl = source->GetSkillLevel(sv);
                if (lvl <= 0)
                    continue;

                clone->SetSkillLevel(sv, (uint8_t)lvl);

                if (sv > 127)
                    continue;

                CSkillProto* sk = CSkillManager::instance().Get(sv);
                if (!sk)
                    continue;

                if (!IS_SET(sk->dwFlag, SKILL_FLAG_ATTACK))
                    continue;
                if (IS_SET(sk->dwFlag, SKILL_FLAG_SELFONLY) || IS_SET(sk->dwFlag, SKILL_FLAG_TOGGLE))
                    continue;

                skillList.push_back((uint8_t)sv);
            }

            if (!clone->Show(mapIndex, gx, gy, 0))
            {
                M2_DESTROY_CHARACTER(clone);
                continue;
            }

            // register
	m_cloneAllowedPid[ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(clone))] = ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(owner));
	m_cloneMap[ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(clone))] = mapIndex;
	m_cloneTargetVid[ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(clone))] = ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(owner));
	m_cloneSkills[ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(clone))] = std::move(skillList);

	m_clonePending.erase(ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(clone)));
	m_cloneNextAction[ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(clone))] = get_dword_time() + 800;
	m_cloneOffset[ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(clone))] = std::make_pair((int16_t)number(-40, 40), (int16_t)number(-40, 40));

            ++remain;
        }

        d->SetFlag(kFlagClonesRemain, remain);

        BigNoticeMap(mapIndex, "Elveszett Kastely: Mindenki olje meg a sajat klonjat!");

        // AI tick: mozgas + skill + attack (NO mob state)
        ScheduleCloneAI(mapIndex);
    }



        void StartFloor4(int32_t mapIndex)
        {
            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIndex);
            if (!d)
                return;

            CancelTimer(mapIndex);
            CancelCloneAI(mapIndex);
            ClearClonesOnMap(mapIndex);

            d->KillAll();
            d->ClearRegen();

            d->SetFlag(kFlagFloor, 4);
            d->SetFlag(kFlagTileStage, 0);
            d->SetFlag(kFlagTotemVid, 0);

            for (int i = 0; i < kTileStages; ++i)
                ApplyBlockRect(mapIndex, kBlockRects[i], true);

            d->JumpAll(mapIndex, kFloor4CenterX, kFloor4CenterY);
            d->SpawnRegen(kFloor4Regen, true);

            LPCHARACTER totem = d->SpawnMob((int32_t)kTotemVnum, kFloor4CenterX, kFloor4CenterY);
            if (totem)
	d->SetFlag(kFlagTotemVid, (int32_t)ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(totem)));

            SendCommandMap(mapIndex, "lostcastle_tile 0");
            BigNoticeMap(mapIndex, "Elveszett Kastely: Mobokbol eshet %u (3%%). Huzd a totemre, hogy csempet tegyel le!", kTileItemVnum);
        }

        void OnCloneKilled(int32_t mapIndex, uint32_t cloneVid)
        {
            auto it = m_cloneAllowedPid.find(cloneVid);
            if (it == m_cloneAllowedPid.end())
                return;

            m_cloneAllowedPid.erase(it);
            m_cloneMap.erase(cloneVid);
            m_cloneTargetVid.erase(cloneVid);
            m_cloneSkills.erase(cloneVid);
            m_clonePending.erase(cloneVid);
            m_cloneNextAction.erase(cloneVid);
            m_cloneOffset.erase(cloneVid);

            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIndex);
            if (!d)
                return;

            int32_t remain = d->GetFlag(kFlagClonesRemain);
            if (remain > 0) remain -= 1;
            d->SetFlag(kFlagClonesRemain, remain);

            if (remain <= 0)
            {
                BigNoticeMap(mapIndex, "Elveszett Kastely: Minden klon elpusztult! Kovetkezik a 4. floor.");
                StartFloor4(mapIndex);
            }
        }

        bool UnlockNextTile(int32_t mapIndex)
        {
            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIndex);
            if (!d)
                return false;

            int32_t stage = d->GetFlag(kFlagTileStage);
            if (stage >= kTileStages)
                return false;

            ApplyBlockRect(mapIndex, kBlockRects[stage], false);

            stage += 1;
            d->SetFlag(kFlagTileStage, stage);

            SendCommandMap(mapIndex, "lostcastle_tile %d", stage);
            BigNoticeMap(mapIndex, "Elveszett Kastely: Letettel egy csempet (%d/%d)!", stage, kTileStages);
            return true;
        }

        bool IsClone(uint32_t vid) const
        {
            return m_cloneAllowedPid.find(vid) != m_cloneAllowedPid.end();
        }

        bool IsCloneAttackAllowed(uint32_t cloneVid, uint32_t pcPid) const
        {
            auto it = m_cloneAllowedPid.find(cloneVid);
            if (it == m_cloneAllowedPid.end())
                return true;
            return it->second == pcPid;
        }
    };

    static CLostCastleDungeonImpl s_lc;

    EVENTFUNC(lostcastle_timer_event)
    {
        // NO RTTI (same style as Pyramid)
        lostcastle_timer_event_info* info = (lostcastle_timer_event_info*)event->info;
        if (!info)
            return 0;

        const int32_t mapIndex = info->mapIndex;
        const int32_t floor = info->floor;

        LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIndex);
        if (!d)
        {
            s_lc.CancelTimer(mapIndex);
            return 0;
        }

        if (d->GetFlag(kFlagWasCompleted) != 0)
        {
            s_lc.CancelTimer(mapIndex);
            return 0;
        }

        if (d->GetFlag(kFlagFloor) != floor)
        {
            s_lc.CancelTimer(mapIndex);
            return 0;
        }

        int32_t remain = info->remainSec;

        if (remain > 0)
            BigNoticeMap(mapIndex, "Elveszett Kastely: %d mp marad meg!", remain);

        remain -= 10;
        info->remainSec = remain;

        if (remain <= 0)
        {
            BigNoticeMap(mapIndex, "Elveszett Kastely: Lejart az idõ! Kilepes a lobbyba.");
            s_lc.ClearDungeon(mapIndex);
            return 0;
        }

        return PASSES_PER_SEC(10);
    }

    EVENTFUNC(lostcastle_clone_ai_event)
    {
        lostcastle_clone_ai_event_info* info = (lostcastle_clone_ai_event_info*)event->info;
        if (!info)
            return 0;

        const int32_t mapIndex = info->mapIndex;

        // If no clones remain on this map, stop the AI event
        bool hasCloneOnMap = false;
        for (auto it = s_lc.m_cloneMap.begin(); it != s_lc.m_cloneMap.end(); ++it)
        {
            if (it->second == mapIndex) { hasCloneOnMap = true; break; }
        }
        if (!hasCloneOnMap)
        {
            s_lc.CancelCloneAI(mapIndex);
            return 0;
        }

        // LostCastle private maps: only run on Floor3 while dungeon is active
        if (IsInRange(mapIndex, kPrivateMin, kPrivateMax))
        {
            LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(mapIndex);
            if (!d || d->GetFlag(kFlagWasCompleted) != 0 || d->GetFlag(kFlagFloor) != 3)
            {
                s_lc.CancelCloneAI(mapIndex);
                return 0;
            }
        }


        const uint32_t now = get_dword_time();

        for (auto it = s_lc.m_cloneMap.begin(); it != s_lc.m_cloneMap.end(); ++it)
        {
            const uint32_t cloneVid = it->first;
            if (it->second != mapIndex)
                continue;

            LPCHARACTER clone = CHARACTER_MANAGER::instance().Find(cloneVid);
            if (!clone || clone->IsDead())
                continue;

            auto tgtIt = s_lc.m_cloneTargetVid.find(cloneVid);
            if (tgtIt == s_lc.m_cloneTargetVid.end())
                continue;

            LPCHARACTER target = CHARACTER_MANAGER::instance().Find(tgtIt->second);
            if (!target || !target->IsPC() || target->IsDead())
            {
                s_lc.m_clonePending.erase(cloneVid);
                continue;
            }

            const int32_t dist = DISTANCE_APPROX(target->GetX() - clone->GetX(), target->GetY() - clone->GetY());

            // 1) Pending hit: damage only when we previously started an animation
            auto pendIt = s_lc.m_clonePending.find(cloneVid);
            if (pendIt != s_lc.m_clonePending.end())
            {
                // IMPORTANT: unordered_map iterators can become invalid after a rehash.
                // Copy the payload and erase by KEY only (safe).
                const SClonePending p = pendIt->second;

                // cancel if target got too far
                const int32_t maxPendDist = p.isSkill ? 900 : 650;
                if (dist > maxPendDist)
                {
                    s_lc.m_clonePending.erase(cloneVid);
                    continue;
                }

                if (now >= p.executeTime)
                {
                    bool didAction = false;
                    if (p.isSkill)
                    {
                        // NOTE: nálatok az attack skillek (SKILL_FLAG_ATTACK) a UseSkill-ben általában csak
                        // "arm / cooldown"-ot csinálnak, és a tényleges sebzés a kliens attack packetjéből jön.
                        // Mivel a klónnak nincs kliens inputja, itt kézzel lefuttatjuk a ComputeSkill-t.
                        didAction = clone->UseSkill(p.attackType, target, true);

                        if (didAction)
                        {
                            CSkillProto* sk = CSkillManager::instance().Get(p.attackType);
                            if (sk && IS_SET(sk->dwFlag, SKILL_FLAG_ATTACK))
                            {
                                // Bizonyos attack skillek már a UseSkill-ben ComputeSkill-oznak (pl. charge, MUYEONG, BYEURAK),
                                // ezeket ne duplázzuk.
                                if (p.attackType != SKILL_BYEURAK && p.attackType != SKILL_MUYEONG && !sk->IsChargeSkill())
                                    clone->ComputeSkill(p.attackType, target, 0);
                            }
                        }
                    }
                    else
                    {
                        didAction = clone->Attack(target, 0);
                    }

                    if (didAction)
                    {
                        // Force victim hurt animation like real PvP
                        target->Motion(MOTION_DAMAGE, clone);
                    }

                    s_lc.m_clonePending.erase(cloneVid);
                }

                // while pending exists (not executed yet), do not chase/attack again
                continue;
            }

            // 2) Chase: tartsunk egy stabil melee tavolsagot, ne fusson at rajtad es ne jittereljen
            const int32_t desired = 60; // kb. 1.7m

            // ha túl közel van, lépjen hátra kicsit (különben "átfut" és köröz)
            if (dist < desired - 40)
            {
                const int32_t dx = clone->GetX() - target->GetX();
                const int32_t dy = clone->GetY() - target->GetY();
                float len = sqrtf((float)dx * (float)dx + (float)dy * (float)dy);
                if (len < 1.0f) len = 1.0f;

                int16_t ox = 0, oy = 0;
                auto offIt = s_lc.m_cloneOffset.find(cloneVid);
                if (offIt != s_lc.m_cloneOffset.end())
                {
                    ox = offIt->second.first;
                    oy = offIt->second.second;
                }

                int32_t tx = target->GetX() + (int32_t)((dx / len) * desired) + ox;
                int32_t ty = target->GetY() + (int32_t)((dy / len) * desired) + oy;

                if (SECTREE_MANAGER::instance().IsMovablePosition(mapIndex, tx, ty))
                    LostCastleCloneStartMove(clone, tx, ty, now);

                continue;
            }

            if (dist > desired + 90)
            {
                int16_t ox = 0, oy = 0;
                auto offIt = s_lc.m_cloneOffset.find(cloneVid);
                if (offIt != s_lc.m_cloneOffset.end())
                {
                    ox = offIt->second.first;
                    oy = offIt->second.second;
                }

                int32_t tx = target->GetX();
                int32_t ty = target->GetY();

                const int32_t dx = tx - clone->GetX();
                const int32_t dy = ty - clone->GetY();
                float len = sqrtf((float)dx * (float)dx + (float)dy * (float)dy);
                if (len < 1.0f) len = 1.0f;

                // celpont: a target ele (de nem ra), fixed offsettel
                tx = tx - (int32_t)((dx / len) * desired) + ox;
                ty = ty - (int32_t)((dy / len) * desired) + oy;

                if (!SECTREE_MANAGER::instance().IsMovablePosition(mapIndex, tx, ty))
                {
                    tx = target->GetX();
                    ty = target->GetY();
                }

                LostCastleCloneStartMove(clone, tx, ty, now);
                continue;
            }

            // 3) Cooldown gate
            uint32_t& nextTime = s_lc.m_cloneNextAction[cloneVid];
            if (now < nextTime)
                continue;

            // 4) Prefer skills sometimes
            uint8_t chosenSkill = 0;
            auto skIt = s_lc.m_cloneSkills.find(cloneVid);
            if (skIt != s_lc.m_cloneSkills.end() && !skIt->second.empty() && number(1, 100) <= 35)
            {
                const std::vector<uint8_t>& skills = skIt->second;
                for (int tries = 0; tries < 5; ++tries)
                {
                    const uint8_t sv = skills[number(0, (int)skills.size() - 1)];
                    if (sv == 0)
                        continue;
                    if (!clone->CanUseSkill(sv))
                        continue;
                    chosenSkill = sv;
                    break;
                }
            }

            if (chosenSkill)
            {
                LostCastleCloneBroadcastSkill(clone, target, chosenSkill, now);

                SClonePending p;
	p.targetVid = ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(target));
                p.attackType = chosenSkill;
                p.motionArg = chosenSkill;
                p.isSkill = true;
                p.executeTime = now + 380; // skill hit feels better with a small delay
                s_lc.m_clonePending[cloneVid] = p;

                nextTime = now + 1200;
                continue;
            }

            // 5) Melee swing
            const uint8_t motion = (uint8_t)number(MOTION_NORMAL_ATTACK, MOTION_COMBO_ATTACK_8);
            LostCastleCloneBroadcastMelee(clone, target, motion, now);

            SClonePending p;
	p.targetVid = ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(target));
            p.attackType = 0;
            p.motionArg = motion;
            p.isSkill = false;
            p.executeTime = now + CalcMeleeHitDelayMs(clone, motion);
            s_lc.m_clonePending[cloneVid] = p;

            nextTime = now + CalcAttackIntervalMs(clone);
        }

        int tick = PASSES_PER_SEC(1) / 10;
        if (tick < 1) tick = 1;
        return tick;
    }

}

// ---------------- Public API ----------------
CLostCastleDungeon& CLostCastleDungeon::instance()
{
    static CLostCastleDungeon inst;
    return inst;
}

bool CLostCastleDungeon::IsCloneVID(uint32_t vid) const
{
    return s_lc.IsClone(vid);
}

bool CLostCastleDungeon::IsLostCastleMap(int32_t mapIndex) const
{
    return (mapIndex == kOriginalMap) || IsInRange(mapIndex, kPrivateMin, kPrivateMax);
}


bool CLostCastleDungeon::SpawnTestClones(CHARACTER* source, CHARACTER* target, int32_t count)
{
    if (!source || !target)
        return false;
    if (!source->IsPC() || !target->IsPC())
        return false;

    if (count <= 0) count = 1;
    if (count > 20) count = 20;

    const int32_t mapIndex = target->GetMapIndex();

    // Spawn near target (global coords)
    int32_t spawned = 0;
    for (int32_t n = 0; n < count; ++n)
    {
        int32_t gx = target->GetX();
        int32_t gy = target->GetY();

        // try random nearby points
        for (int t = 0; t < 25; ++t)
        {
            const int dx = number(-400, 400);
            const int dy = number(-400, 400);
            const int32_t tx = target->GetX() + dx;
            const int32_t ty = target->GetY() + dy;
            if (SECTREE_MANAGER::instance().Get(mapIndex, tx, ty))
            {
                gx = tx;
                gy = ty;
                break;
            }
        }

        char cloneName[CHARACTER_NAME_MAX_LEN + 1];
        // unique name to avoid collisions
        snprintf(cloneName, sizeof(cloneName), "Gonosz %s", source->GetName());

        LPCHARACTER clone = CHARACTER_MANAGER::instance().CreateCharacter(cloneName, 0);
        if (!clone)
            continue;

       // clone->SetCharType(CHAR_TYPE_PC);
      //  clone->SetFakePlayer(true);
        clone->SetName(std::string(cloneName));

        clone->SetRace((uint8_t)source->GetRaceNum());
        clone->SetEmpire(ecs::PlayerRuntime::GetEmpire(AIHelpers::EcsOf(source)));
        clone->SetPKMode(PK_MODE_FREE);
        clone->SetSkillGroup(source->GetSkillGroup());

        clone->SetRotation(source->GetRotation());
        clone->SetXYZ(gx, gy, 0);
        clone->SetMapIndex(mapIndex);

        // look/parts
        clone->SetPart(PART_MAIN, source->GetPart(PART_MAIN));
        clone->SetPart(PART_WEAPON, source->GetPart(PART_WEAPON));
        clone->SetPart(PART_HEAD, source->GetPart(PART_HEAD));
        clone->SetPart(PART_HAIR, source->GetPart(PART_HAIR));
#ifdef ENABLE_ACCE_SYSTEM
        clone->SetPart(PART_ACCE, source->GetPart(PART_ACCE));
#endif
#ifdef ENABLE_RUNE_SYSTEM
        clone->SetPart(PART_RUNE, source->GetPart(PART_RUNE));
#endif
#ifdef ENABLE_COSTUME_EFFECT
        clone->SetPart(PART_EFFECT_BODY, source->GetPart(PART_EFFECT_BODY));
        clone->SetPart(PART_EFFECT_WEAPON, source->GetPart(PART_EFFECT_WEAPON));
#endif

        CloneEquipWeaponFromSource(clone, source);

        // copy points (NO 10x here - real PvP test)
        for (int p = 0; p < POINT_MAX_NUM; ++p)
        {
            clone->SetRealPoint((uint8_t)p, source->GetPoint((uint8_t)p));
            clone->SetPoint((uint8_t)p, source->GetPoint((uint8_t)p));
        }
        clone->SetLevel((uint8_t)source->GetLevel());
        clone->SetMaxHP(source->GetMaxHP()* STR_MULTIPLE);
        clone->SetMaxSP(source->GetMaxSP() * STR_MULTIPLE);
        clone->SetHP(source->GetMaxHP() * STR_MULTIPLE);
        clone->SetSP(source->GetMaxSP() * STR_MULTIPLE);
        clone->SetKillerMode(true);

        // Skills (AI uses only ATTACK skills)
        std::vector<uint8_t> skillList;
        skillList.reserve(32);
        for (uint32_t sv = 1; sv <= 255; ++sv)
        {
            const int lvl = source->GetSkillLevel(sv);
            if (lvl <= 0)
                continue;

            clone->SetSkillLevel(sv, (uint8_t)lvl);

            if (sv > 127)
                continue;

            CSkillProto* sk = CSkillManager::instance().Get(sv);
            if (!sk)
                continue;

            if (!IS_SET(sk->dwFlag, SKILL_FLAG_ATTACK))
                continue;
            if (IS_SET(sk->dwFlag, SKILL_FLAG_SELFONLY) || IS_SET(sk->dwFlag, SKILL_FLAG_TOGGLE))
                continue;

            skillList.push_back((uint8_t)sv);
        }

        if (!clone->Show(mapIndex, gx, gy, 0))
        {
            M2_DESTROY_CHARACTER(clone);
            continue;
        }

        // Register: only the target can fight this clone, and the clone targets the target
	s_lc.m_cloneAllowedPid[ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(clone))] = ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(target));
	s_lc.m_cloneMap[ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(clone))] = mapIndex;
	s_lc.m_cloneTargetVid[ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(clone))] = ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(target));
	s_lc.m_cloneSkills[ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(clone))] = std::move(skillList);

	s_lc.m_clonePending.erase(ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(clone)));
	s_lc.m_cloneNextAction[ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(clone))] = get_dword_time() + 800;
	s_lc.m_cloneOffset[ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(clone))] = std::make_pair((int16_t)number(-40, 40), (int16_t)number(-40, 40));

        ++spawned;
    }

    if (spawned > 0)
        s_lc.ScheduleCloneAI(mapIndex);

    return spawned > 0;
}

void CLostCastleDungeon::PurgeTestClonesOnMap(int32_t mapIndex)
{
    s_lc.CancelCloneAI(mapIndex);
    s_lc.ClearClonesOnMap(mapIndex);
}

void CLostCastleDungeon::PurgeTestClonesForTargetPID(uint32_t targetPid, int32_t mapIndex)
{
    if (!targetPid)
        return;

    std::vector<uint32_t> toRemove;
    toRemove.reserve(32);

    for (auto it = s_lc.m_cloneAllowedPid.begin(); it != s_lc.m_cloneAllowedPid.end(); ++it)
    {
        if (it->second != targetPid)
            continue;

        const uint32_t vid = it->first;
        auto mit = s_lc.m_cloneMap.find(vid);
        if (mit == s_lc.m_cloneMap.end())
            continue;

        if (mapIndex != -1 && mit->second != mapIndex)
            continue;

        toRemove.push_back(vid);
    }

    for (uint32_t vid : toRemove)
    {
        auto mit = s_lc.m_cloneMap.find(vid);
        const int32_t m = (mit != s_lc.m_cloneMap.end()) ? mit->second : -1;

        s_lc.m_cloneAllowedPid.erase(vid);
        s_lc.m_cloneMap.erase(vid);
        s_lc.m_cloneTargetVid.erase(vid);
        s_lc.m_cloneSkills.erase(vid);
        s_lc.m_clonePending.erase(vid);
        s_lc.m_cloneNextAction.erase(vid);
        s_lc.m_cloneOffset.erase(vid);

        if (LPCHARACTER c = CHARACTER_MANAGER::instance().Find(vid))
            M2_DESTROY_CHARACTER(c);

        // if we cleared some clones, and map has none left, stop AI
        if (m != -1)
        {
            bool anyLeft = false;
            for (auto& kv : s_lc.m_cloneMap)
            {
                if (kv.second == m) { anyLeft = true; break; }
            }
            if (!anyLeft)
                s_lc.CancelCloneAI(m);
        }
    }
}

bool CLostCastleDungeon::OnUseItem30001(CHARACTER* ch)
{
    if (!ch || !ch->IsPC())
        return false;

    const int32_t idx = ch->GetMapIndex();

    // Block inside LostCastle instance maps to avoid skipping dungeon mechanics
    if (IsInRange(idx, kPrivateMin, kPrivateMax))
    {
        ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Dungeonban nem hasznalhato.");
        return false;
    }

    PurgeTestClonesForTargetPID(ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)), idx);
    ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Klonok torolve ezen a mapon.");
    return true;
}

void CLostCastleDungeon::OnPlayerDisconnect(CHARACTER* ch)
{
    if (!ch || !ch->IsPC())
        return;

    const int32_t idx = ch->GetMapIndex();
    if (!IsInRange(idx, kPrivateMin, kPrivateMax))
        return;

    const int32_t now = get_global_time();
    ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfDisconnect, now + kRejoinSeconds);
    ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfIdx, idx);
    ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfCh, (int32_t)g_bChannel);
}

void CLostCastleDungeon::OnPlayerLogin(CHARACTER* ch)
{
    if (!ch || !ch->IsPC())
        return;

    const int32_t idx = ch->GetMapIndex();
    if (!IsInRange(idx, kPrivateMin, kPrivateMax))
        return;

    // IMPORTANT: don't set warp location to kOriginalMap here.
    // Lobby is set at entry time (npc click) to where the player came from.
    ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfIdx, idx);
    ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), kQfCh, (int32_t)g_bChannel);
}

bool CLostCastleDungeon::OnClickNpc(CHARACTER* ch)
{
    if (!ch || !ch->IsPC())
        return false;

    // save "lobby" as where the NPC was clicked (like your flow expects)
    const int32_t lobbyMap = ch->GetMapIndex();
    const int32_t lobbyX = ch->GetX() / 100;
    const int32_t lobbyY = ch->GetY() / 100;

    LPPARTY party = ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch));

    if (party)
    {
        if (party->GetLeaderPID() != ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)))
        {
            ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Csak a party leader indithatja!");
            return true;
        }

        bool ok = true;
        const int32_t leaderMap = ch->GetMapIndex();

        auto check = [&](LPCHARACTER m)
            {
                if (!m || !m->IsPC())
                    return;

                if (m->GetMapIndex() != leaderMap)
                {
                    ok = false;
                    return;
                }
                if (m->GetLevel() < kMinLevel || m->GetLevel() > kMaxLevel)
                {
                    ok = false;
                    return;
                }
                if (m->CountSpecifyItem(kEntryItemVnum) < 1)
                {
                    ok = false;
                    return;
                }
            };

        party->ForEachOnlineMember(check);

        if (!ok)
        {
            ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Feltetelek: minden tagnak itt kell lennie, lvl %d-%d, es kell 1 db %u.", kMinLevel, kMaxLevel, kEntryItemVnum);
            return true;
        }
    }
    else
    {
        if (ch->GetLevel() < kMinLevel || ch->GetLevel() > kMaxLevel)
        {
            ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Csak lvl %d-%d kozott lephetsz be!", kMinLevel, kMaxLevel);
            return true;
        }
        if (ch->CountSpecifyItem(kEntryItemVnum) < 1)
        {
            ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Szukseges belepo item: %u", kEntryItemVnum);
            return true;
        }
    }

    // Create + join (Pyramid style)
    LPDUNGEON d = CDungeonManager::instance().Create(kOriginalMap);
    if (!d)
    {
        ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Elveszett Kastely: nem sikerult letrehozni a dungeont.");
        return true;
    }

    d->SetFlag(kFlagFloor, 1);
    d->SetFlag(kFlagWasCompleted, 0);
    d->SetFlag(kFlagCorrectMetin, 0);
    d->SetFlag(kFlagKeyMask, 0);
    d->SetFlag(kFlagStatueVid, 0);
    d->SetFlag(kFlagTotemVid, 0);
    d->SetFlag(kFlagTileStage, 0);
    d->SetFlag(kFlagClonesRemain, 0);

    auto applyMember = [&](LPCHARACTER m)
        {
            if (!m || !m->IsPC())
                return;

            m->RemoveSpecifyItem(kEntryItemVnum, 1);

            // rejoin flags reset
            ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(m), kQfDisconnect, 0);
            ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(m), kQfIdx, d->GetMapIndex());
            ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(m), kQfCh, (int32_t)g_bChannel);

            // exit/lobby
            m->SetWarpLocation(lobbyMap, lobbyX, lobbyY);
        };

    if (!party)
    {
        applyMember(ch);

        // IMPORTANT: Join expects GLOBAL CELL on your core
        d->Join_Coords(ch, kJoinGlobalX, kJoinGlobalY, ch->GetMapIndex());
    }
    else
    {
        auto fn = [&](LPCHARACTER m) { applyMember(m); };
        party->ForEachOnMapMember(fn, ch->GetMapIndex());

        // IMPORTANT: Join expects GLOBAL CELL on your core
        d->JoinParty_Coords(party, kJoinGlobalX, kJoinGlobalY, ch->GetMapIndex());
    }

    s_lc.StartFloor1(d->GetMapIndex());
    return true;
}

void CLostCastleDungeon::OnMobKilled(CHARACTER* killer, CHARACTER* victim)
{
    if (!victim)
        return;

    const int32_t idx = victim->GetMapIndex();
    if (!IsInRange(idx, kPrivateMin, kPrivateMax))
        return;

    LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
    if (!d)
        return;

    const int32_t floor = d->GetFlag(kFlagFloor);

    if (floor == 1 && victim->GetRaceNum() == kMetinVnum)
    {
        const uint32_t correctVid = (uint32_t)d->GetFlag(kFlagCorrectMetin);
	if (correctVid && ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(victim)) == correctVid)
        {
            BigNoticeMap(idx, "Elveszett Kastely: Megtalaltatok a megfelelõ metinkõvet! Floor2 kovetkezik.");
            s_lc.StartFloor2(idx);
        }
        return;
    }

    if (floor == 3)
    {
	const uint32_t vvid = ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(victim));
        if (s_lc.IsClone(vvid))
            s_lc.OnCloneKilled(idx, vvid);
        return;
    }

    if (floor == 4)
    {
        if (killer && killer->IsPC() && victim->IsMonster())
        {
            const uint16_t vnum = victim->GetRaceNum();
            if (vnum != kTotemVnum && vnum != kStatueVnum)
            {
                if (number(1, 100) <= kTileDropChancePct)
                    DropItemOnGround(victim, killer, kTileItemVnum, 1);
            }
        }
        return;
    }
}

bool CLostCastleDungeon::OnNpcTakeItem(CHARACTER* from, CHARACTER* npc, LPITEM item)
{
    if (!from || !from->IsPC() || !npc || !item)
        return false;

    const int32_t idx = from->GetMapIndex();
    if (!IsInRange(idx, kPrivateMin, kPrivateMax))
        return false;

    LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(idx);
    if (!d)
        return false;

    const int32_t floor = d->GetFlag(kFlagFloor);

    if (floor == 2)
    {
        const uint32_t statueVid = (uint32_t)d->GetFlag(kFlagStatueVid);
	if (statueVid && ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(npc)) != statueVid)
            return false;

        const uint32_t vnum = ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, item));
        int keyIndex = -1;
        for (int i = 0; i < 5; ++i)
        {
            if (kKeyItems[i] == vnum) { keyIndex = i; break; }
        }
        if (keyIndex < 0)
            return false;

        int32_t mask = d->GetFlag(kFlagKeyMask);
        const int32_t bit = 1 << keyIndex;
        if (mask & bit)
        {
            ecs::ChatSystem::Send(AIHelpers::EcsOf(from), CHAT_TYPE_INFO, "Ez a kulcs mar be lett adva!");
            return true;
        }

        ConsumeOneGivenItem(EntityFactory::CreateItemEntity(g_registry, item), "LOSTCASTLE_KEY");
        mask |= bit;
        d->SetFlag(kFlagKeyMask, mask);

        BigNoticeMap(idx, "Elveszett Kastely: %s megtalalta az egyik kulcsot!", from->GetName());

        if (mask == ((1 << 5) - 1))
        {
            BigNoticeMap(idx, "Elveszett Kastely: Megvan mind az 5 kulcs! Floor3 kovetkezik.");

            ForEachPcOnMap(idx, [&](LPCHARACTER pc) {
                if (!pc) return;
                for (uint32_t kv : kKeyItems)
                {
                    const int32_t c = pc->CountSpecifyItem(kv);
                    if (c > 0)
                        pc->RemoveSpecifyItem(kv, c);
                }
                });

            s_lc.StartFloor3(idx);
        }

        return true;
    }

    if (floor == 4)
    {
        const uint32_t totemVid = (uint32_t)d->GetFlag(kFlagTotemVid);
	if (totemVid && ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(npc)) != totemVid)
            return false;

        if (ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, item)) != kTileItemVnum)
            return false;

        ConsumeOneGivenItem(EntityFactory::CreateItemEntity(g_registry, item), "LOSTCASTLE_TILE");

        if (!s_lc.UnlockNextTile(idx))
            ecs::ChatSystem::Send(AIHelpers::EcsOf(from), CHAT_TYPE_INFO, "Mar minden csempe le van teve!");

        return true;
    }

    return false;
}

bool CLostCastleDungeon::CheckCloneDamage(CHARACTER* attacker, CHARACTER* victim) const
{
    if (!attacker || !victim)
        return true;

	const uint32_t aVid = ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(attacker));
	const uint32_t vVid = ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(victim));

    const bool attackerIsClone = s_lc.IsClone(aVid);
    const bool victimIsClone = s_lc.IsClone(vVid);

    // normal damage
    if (!attackerIsClone && !victimIsClone)
        return true;

    // Player/NPC -> Clone
    //if (!attackerIsClone && victimIsClone)
    //{
    //    if (!(attacker->IsPC() || attacker->IsFakePlayer()))
    //        return false;

    //    return s_lc.IsCloneAttackAllowed(vVid, ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(attacker)));
    //}

    //// Clone -> Player/NPC
    //if (attackerIsClone && !victimIsClone)
    //{
    //    if (!(victim->IsPC() || victim->IsFakePlayer()))
    //        return false;

    //    return s_lc.IsCloneAttackAllowed(aVid, ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(victim)));
    //}

    // Clone -> Clone tiltás
    return false;
}
