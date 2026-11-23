// LostCastleDungeon.cpp
#include "stdafx.h"
#include "../item/TypedDungeonEntry.hpp"
#include "../item/TypedDungeonActions.hpp"
#include "../item/TypedDungeonItems.hpp"
#include "../item/TypedLoot.hpp"
#include "../item/TypedCostumes.hpp"
#include "../item/TypedEquipment.hpp"
#include "../ecs/systems/AISystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include <Core/Logging.hpp>
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../ecs/systems/SkillSystem.hpp"
#include "../ecs/systems/MovementSystem.hpp"
#include "../ecs/AIHelpers.hpp"
#include "../ecs/systems/QuestSystem.hpp"
#include "LostCastleDungeon.h"
#include "LostCastleLayout.hpp"
#include "JsonFile.h"
#include "../mission_pass/MissionPassEvents.hpp"

#include <nlohmann/json.hpp>

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
#include "packet.h"
#include "log.h"
#include "../ecs/Registry.hpp"
#include "../ecs/EntityFactory.hpp"
#include "mob_manager.h"
#include "pvp.h"
#include "battle.h"
#include "skill.h"
#include "motion.h"
#include "../ecs/CharacterAccessors.hpp"
namespace
{
#define STR_MULTIPLE 50 //player statja szorozva ennyivel lesz a klon statja (HP/SP/DMG/STAT),
    // ---------------- CONFIG ----------------
    constexpr int32_t  kOriginalMap = LostCastleLayout::MapIndex;
    constexpr int32_t  kPrivateMin = LostCastleLayout::PrivateMin;
    constexpr int32_t  kPrivateMax = LostCastleLayout::PrivateMax;

    constexpr uint32_t kEntryNpcVnum = 20021;
    constexpr int32_t kLobbyMap = LostCastleLayout::LobbyMap;
    constexpr int32_t kLobbyX = LostCastleLayout::EntranceGuard.x;
    constexpr int32_t kLobbyY = LostCastleLayout::EntranceGuard.y;
    constexpr std::string_view kEntryTicketCode = "material.arpa";

    constexpr int32_t  kMinLevel = LostCastleLayout::MinimumLevel;
    constexpr int32_t  kMaxLevel = LostCastleLayout::MaximumLevel;

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
    constexpr const char* kFloor2Regen = "data/dungeon/elveszett_kastely/floor2.json";

    // Floor3
    constexpr uint32_t kCloneBaseMobVnum = 136;
    constexpr int32_t  kFloor3CenterX = LostCastleLayout::Entrance.x;
    constexpr int32_t  kFloor3CenterY = LostCastleLayout::Entrance.y;

    // Floor4
    constexpr const char* kFloor4Regen = "data/dungeon/elveszett_kastely/floor4.json";
    constexpr uint32_t kTotemVnum = 9298;
    constexpr int32_t  kTileDropChancePct = 3;
    constexpr int32_t  kTileStages = static_cast<int32_t>(LostCastleLayout::Tiles.size());

    // Dungeon flags
    constexpr const char* kFlagFloor = "lc_floor";
    constexpr const char* kFlagWasCompleted = "lc_done";
    constexpr const char* kFlagCorrectMetin = "lc_metin_vid";
    constexpr const char* kFlagStatueVid = "lc_statue_vid";
    constexpr const char* kFlagKeyMask = "lc_key_mask";
    constexpr const char* kFlagNextKeyDrop = "lc_next_key_drop";
    constexpr const char* kFlagTotemVid = "lc_totem_vid";
    constexpr const char* kFlagTileStage = "lc_tile_stage";
    constexpr const char* kFlagClonesRemain = "lc_clones_remain";
    constexpr const char* kFlagExitVid = "lc_exit_vid";

    // Rejoin quest flags (per character)
    constexpr const char* kQfDisconnect = "lostcastle.disconnect";
    constexpr const char* kQfIdx = "lostcastle.idx";
    constexpr const char* kQfCh = "lostcastle.ch";

    constexpr int32_t kRejoinSeconds = 300;

    inline bool IsInRange(int32_t v, int32_t lo, int32_t hi) { return v >= lo && v < hi; }

    constexpr int32_t kMetinPos[][2] = {
        { 300, 120 }, { 320, 150 }, { 340, 180 }, { 360, 210 },
        { 380, 240 }, { 400, 270 }, { 420, 300 }, { 440, 330 },
    };

    constexpr int32_t kMetinPosCount = (int32_t)(sizeof(kMetinPos) / sizeof(kMetinPos[0]));

    bool CastleRegensFit(const char* path, int width, int height)
    {
        nlohmann::json document;
        if (!JsonFile::Load(path, document) || !document.contains("regens"))
            return false;

        const auto& regens = document["regens"];
        if (!regens.is_array() || regens.empty())
            return false;

        for (const auto& row : regens)
        {
            if (!row.is_object())
                return false;
            const auto coordinate = [&](const char* field) {
                const auto position = row.find(field);
                return position != row.end() && position->is_number_integer()
                    ? position->get<std::int64_t>() : -1;
            };
            const auto x = coordinate("x"), y = coordinate("y");
            const auto rx = coordinate("range_x"), ry = coordinate("range_y");
            if (rx < 0 || ry < 0 || x < rx || y < ry
                || x >= width || y >= height || rx >= width - x || ry >= height - y)
                return false;
        }
        return true;
    }

    bool CastleLayoutAvailable(int32_t mapIndex)
    {
        auto& maps = SECTREE_MANAGER::instance();
        const auto* map = maps.GetMap(mapIndex);
        if (!map)
            return false;

        const auto& bounds = map->m_setting;
        const int width = bounds.iWidth / 100;
        const int height = bounds.iHeight / 100;
        if (width != LostCastleLayout::MapWidth || height != LostCastleLayout::MapHeight
            || !LostCastleLayout::Fits(width, height) || !CastleRegensFit(kFloor2Regen, width, height)
            || !CastleRegensFit(kFloor4Regen, width, height))
            return false;

        const auto movable = [&](LostCastleLayout::Point point) {
            return maps.IsMovablePosition(mapIndex, bounds.iBaseX + point.x * 100, bounds.iBaseY + point.y * 100);
        };
        for (const auto point : {LostCastleLayout::Entrance, LostCastleLayout::KeyRoomEntrance,
                LostCastleLayout::Statue, LostCastleLayout::TileRoomEntrance, LostCastleLayout::Totem})
            if (!movable(point))
                return false;

        for (int i = 0; i < kFloor1MetinCount; ++i)
            if (!movable({kMetinPos[i][0], kMetinPos[i][1]}))
                return false;

        // The temporary tiles may only cover initially free cells. Otherwise
        // opening one would remove collision belonging to the base map.
        for (const auto& tile : LostCastleLayout::Tiles)
            for (int y = tile.start.y * 100; y < tile.end.y * 100; y += CELL_SIZE)
                for (int x = tile.start.x * 100; x < tile.end.x * 100; x += CELL_SIZE)
                    if (!maps.IsMovablePosition(mapIndex, bounds.iBaseX + x, bounds.iBaseY + y))
                        return false;
        return true;
    }

    void JumpToRoom(entt::entity dungeon, int32_t mapIndex, LostCastleLayout::Point local)
    {
        const auto* map = SECTREE_MANAGER::instance().GetMap(mapIndex);
        if (!map)
            return;
        const auto world = LostCastleLayout::WorldCell(local, map->m_setting.iBaseX, map->m_setting.iBaseY);
        DungeonSystem::JumpAll(dungeon, mapIndex, world.x, world.y);
    }

    struct FForEachPC
    {
        std::function<void(entt::entity)> fn;
        void operator()(entt::entity ch)
        {
            if (!ecs::IsCharacter(ch) || !ecs::PlayerRuntime::IsPC(ch))
                return;
            fn(ch);
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

    inline void BigNoticeMap(int32_t mapIndex, const char* fmt, ...)
    {
        char buf[CHAT_MAX_LEN + 64];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);

        ForEachPcOnMap(mapIndex, [&](entt::entity pc){
            if (ecs::PlayerRuntime::IsValid(pc))
                ecs::ChatSystem::Send(pc, CHAT_TYPE_BIG_NOTICE, "%s", buf);
            });
    }

    inline void SendCommandMap(int32_t mapIndex, const char* fmt, ...)
    {
        char buf[CHAT_MAX_LEN + 64];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);

        ForEachPcOnMap(mapIndex, [&](entt::entity pc){
            if (ecs::PlayerRuntime::GetDesc(pc))
                ecs::ChatSystem::Send(pc, CHAT_TYPE_COMMAND, "%s", buf);
            });
    }

    bool DropCastleItem(entt::entity victim, entt::entity claimant, auriga::item::DungeonUseRole role)
    {
        const auto key = FindTypedDungeonUseKey(role);
        if (!key)
        {
            LOG_ERROR("Lost Castle: typed objective item {} is missing from the catalog", static_cast<unsigned>(role));
            return false;
        }

        namespace Player = ecs::PlayerRuntime;
        return DropTypedLoot({*key, 0, 1}, Player::GetMapIndex(victim),
            Player::GetX(victim), Player::GetY(victim),
            Player::GetZ(victim), claimant, 180000);
    }

    bool ApplyBlockRect(int32_t mapIndex, const LostCastleLayout::Tile& tile, bool block)
    {
        LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(mapIndex);
        if (!map)
            return false;

        const int32_t sx = map->m_setting.iBaseX + tile.start.x * 100;
        const int32_t sy = map->m_setting.iBaseY + tile.start.y * 100;
        const int32_t ex = map->m_setting.iBaseX + tile.end.x * 100;
        const int32_t ey = map->m_setting.iBaseY + tile.end.y * 100;

        return SECTREE_MANAGER::instance().ForAttrRegion(
            mapIndex,
            sx, sy, ex, ey,
            0,
            ATTR_BLOCK,
            block ? ATTR_REGION_MODE_SET : ATTR_REGION_MODE_REMOVE
        );
    }

    bool InstallCastleWalls(int32_t mapIndex)
    {
        using namespace LostCastleLayout;
        std::vector<int> xs{0, MapWidth}, ys{0, MapHeight};
        for (const auto& room : Rooms)
        {
            xs.insert(xs.end(), {room.start.x, room.end.x});
            ys.insert(ys.end(), {room.start.y, room.end.y});
        }
        const auto order = [](auto& edges) {
            std::sort(edges.begin(), edges.end());
            edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
        };
        order(xs);
        order(ys);
        for (std::size_t y = 1; y < ys.size(); ++y)
            for (std::size_t x = 1; x < xs.size(); ++x)
            {
                const Point start{xs[x - 1], ys[y - 1]};
                if (!Walkable(start) && !ApplyBlockRect(mapIndex, {start, {xs[x], ys[y]}}, true))
                    return false;
            }
        return std::ranges::all_of(Tiles, [=](const Tile& tile) {
            return ApplyBlockRect(mapIndex, tile, true);
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

    inline void MulPoint10(entt::entity ch, uint8_t pt)
    {
        if (!ecs::IsCharacter(ch))
            return;
        const int64_t v = ecs::PointSystem::Get(ch, pt);
        const int64_t nv = ClampMul10(v * STR_MULTIPLE);
        ecs::PointSystem::SetReal(ch, pt, nv);
        ecs::PointSystem::Set(ch, pt, nv);
    }

    inline uint32_t CalcMeleeHitDelayMs(entt::entity ch, uint16_t motionIndex)
    {
        if (!ecs::IsCharacter(ch))
            return 220;

        const uint32_t mode = ecs::MovementSystem::GetMotionMode(ch);
        const float durSec = CMotionManager::instance().GetMotionDuration(ecs::PlayerRuntime::GetRaceNum(ch), MAKE_MOTION_KEY(mode, motionIndex));
        uint32_t durMs = (durSec > 0.01f) ? (uint32_t)(durSec * 1000.0f) : 650;

        // Hit generally lands early-mid swing
        uint32_t hit = (durMs * 35) / 100;
        if (hit < 140) hit = 140;
        if (hit > 380) hit = 380;
        return hit;
    }

    inline uint32_t CalcAttackIntervalMs(entt::entity ch)
    {
        if (!ecs::IsCharacter(ch))
            return 700;

        // ATT_SPEED is typically 0..200
        const int as = ecs::PointSystem::GetLimitPoint(ch, POINT_ATT_SPEED);
        int interval = 900 - as * 3;
        if (interval < 350) interval = 350;
        if (interval > 900) interval = 900;
        return (uint32_t)interval;
    }


    void CopyCloneEquipment(entt::entity clone, entt::entity source)
    {
        namespace Player = ecs::PlayerRuntime;
        Player::SetPart(clone, PART_HAIR, Player::GetPart(source, PART_HAIR));
        CopyTypedCostumeAppearance(clone, source);

        if (const auto* stats = TypedEquipmentView(source); stats && stats->weapon)
            g_registry.emplace<ecs::ClonedWeaponStats>(
                clone, *stats->weapon, stats->flat.physicalAttackRaw, stats->flat.magicalAttackRaw);
    }

    inline void LostCastleCloneStartMove(entt::entity cloneEntity, int32_t tx, int32_t ty, uint32_t now)
    {
        if (!ecs::IsCharacter(cloneEntity))
            return;

        if (ecs::PlayerRuntime::GetX(cloneEntity) == tx && ecs::PlayerRuntime::GetY(cloneEntity) == ty)
            return;

        AISystem::StartStateMachine(cloneEntity, 1);
        ecs::MovementSystem::SetWalkingWithMonsterLog(cloneEntity, false);
        ecs::MovementSystem::SetRotationToXY(cloneEntity, tx, ty);
        ecs::MovementSystem::Goto(cloneEntity, tx, ty);

        // server-controlled chars need explicit MOVE packets
        ecs::MovementSystem::SendMovePacket(cloneEntity, FUNC_MOVE, 0, tx, ty, ecs::MovementSystem::GetCurrentMoveDuration(cloneEntity), now);
    }

    inline void LostCastleCloneBroadcastMelee(entt::entity cloneEntity, entt::entity targetEntity, uint8_t motionIndex, uint32_t now)
    {
        if (!ecs::IsCharacter(cloneEntity) || !ecs::IsCharacter(targetEntity))
            return;

        ecs::MovementSystem::SetRotationToXY(cloneEntity,
            ecs::PlayerRuntime::GetX(targetEntity), ecs::PlayerRuntime::GetY(targetEntity));
        ecs::MovementSystem::Stop(cloneEntity);

        // PC swing is broadcast as FUNC_COMBO with motion index (13..21)
        ecs::MovementSystem::SendMovePacket(cloneEntity, FUNC_COMBO, motionIndex, ecs::PlayerRuntime::GetX(cloneEntity), ecs::PlayerRuntime::GetY(cloneEntity), 0, now);
        ecs::MovementSystem::OnMove(cloneEntity, true);
    }

    inline void LostCastleCloneBroadcastSkill(entt::entity cloneEntity, entt::entity targetEntity, uint8_t skillVnum, uint32_t now)
    {
        if (!ecs::IsCharacter(cloneEntity) || !ecs::IsCharacter(targetEntity))
            return;

        ecs::MovementSystem::SetRotationToXY(cloneEntity,
            ecs::PlayerRuntime::GetX(targetEntity), ecs::PlayerRuntime::GetY(targetEntity));
        ecs::MovementSystem::Stop(cloneEntity);

        // Skills are broadcast as FUNC_SKILL|skillVnum (see input_main.cpp)
        const uint8_t func = (uint8_t)(FUNC_SKILL | (skillVnum & 0x7F));
        ecs::MovementSystem::SendMovePacket(cloneEntity, func, 0, ecs::PlayerRuntime::GetX(cloneEntity), ecs::PlayerRuntime::GetY(cloneEntity), 0, now);
        ecs::MovementSystem::OnMove(cloneEntity, true);
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
                m_cloneSkills.erase(vid);
                m_clonePending.erase(vid);
                m_cloneNextAction.erase(vid);
                m_cloneOffset.erase(vid);
                if (const entt::entity c = CHARACTER_MANAGER::instance().FindEntity(vid); ecs::IsCharacter(c))
                    CHARACTER_MANAGER::instance().DestroyCharacter(c);
            }
        }

        void ClearDungeon(int32_t mapIndex)
        {
            CancelTimer(mapIndex);
            CancelCloneAI(mapIndex);
            ClearClonesOnMap(mapIndex);

            const entt::entity d = CDungeonManager::instance().FindByMapIndex(mapIndex);
            if (d == entt::null)
                return;

            DungeonSystem::SetFlag(d, kFlagWasCompleted, 1);
            DungeonSystem::KillAll(d);
            DungeonSystem::ClearRegen(d);

            if (DungeonSystem::GetFlag(d, kFlagFloor) == 4)
            {
                const auto opened = DungeonSystem::GetFlag(d, kFlagTileStage);
                for (int i = opened; i < kTileStages; ++i)
                    ApplyBlockRect(mapIndex, LostCastleLayout::Tiles[i], false);
            }

            DungeonSystem::ExitAllLobby(d, 1);
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
            const entt::entity d = CDungeonManager::instance().FindByMapIndex(mapIndex);
            if (d == entt::null)
                return;

            DungeonSystem::SetFlag(d, kFlagFloor, 1);
            DungeonSystem::SetFlag(d, kFlagCorrectMetin, 0);

            std::vector<uint32_t> spawnedVids;
            spawnedVids.reserve(kFloor1MetinCount);

            for (int i = 0; i < kFloor1MetinCount; ++i)
            {
                const int idx = i % kMetinPosCount;
                const int32_t x = kMetinPos[idx][0];
                const int32_t y = kMetinPos[idx][1];

                const entt::entity metin = DungeonSystem::SpawnMob(d, (int32_t)kMetinVnum, x, y);
                if (metin == entt::null)
                {
                    LOG_ERROR("[LostCastle] metin spawn fail: vnum={} map={} x={} y={} (i={} idx={})", (unsigned)kMetinVnum, mapIndex, x, y, i, idx);
                    continue;
                }

		spawnedVids.push_back(ecs::PlayerRuntime::GetPacketVID(metin));
            }

            if (spawnedVids.empty())
            {
                LOG_ERROR("[LostCastle] failed to spawn any metins (fixed positions). map={}", mapIndex);
                return;
            }

            const uint32_t correctVid = spawnedVids[number(0, (int)spawnedVids.size() - 1)];
            DungeonSystem::SetFlag(d, kFlagCorrectMetin, (int32_t)correctVid);

            BigNoticeMap(mapIndex, "Elveszett Kastely: %d mp marad meg a megfelelõ metinkõ megtalalasara!", kFloor1TimeSec);
            ScheduleTimer(mapIndex, kFloor1TimeSec, 1);
        }



        //    DungeonSystem::SetFlag(d, kFlagFloor, 1);
        //    DungeonSystem::SetFlag(d, kFlagCorrectMetin, 0);

        //    std::vector<uint32_t> spawnedVids;
        //    spawnedVids.reserve(kFloor1MetinCount);


        //    int32_t attempts = 0;
        //        ++attempts;

        //        spawnedVids.push_back(ecs::PlayerRuntime::GetPacketVID(metin));
        //        "[LostCastle] failed to spawn any metins map=%d localRect=(%d,%d)-(%d,%d)", mapIndex, minX, minY, maxX, maxY);
        //    DungeonSystem::SetFlag(d, kFlagCorrectMetin, (int32_t)correctVid);

        //    ScheduleTimer(mapIndex, kFloor1TimeSec, 1);
        //}

        void StartFloor2(int32_t mapIndex)
        {
            const entt::entity d = CDungeonManager::instance().FindByMapIndex(mapIndex);
            if (d == entt::null)
                return;

            CancelTimer(mapIndex);
            CancelCloneAI(mapIndex);
            ClearClonesOnMap(mapIndex);

            DungeonSystem::KillAll(d);
            DungeonSystem::ClearRegen(d);

            DungeonSystem::SetFlag(d, kFlagFloor, 2);
            DungeonSystem::SetFlag(d, kFlagKeyMask, 0);
            DungeonSystem::SetFlag(d, kFlagNextKeyDrop, 0);
            DungeonSystem::SetFlag(d, kFlagStatueVid, 0);

            JumpToRoom(d, mapIndex, LostCastleLayout::KeyRoomEntrance);

            const auto point = LostCastleLayout::Statue;
            const entt::entity statue = DungeonSystem::SpawnMob(d, kStatueVnum, point.x, point.y);
            if (statue != entt::null)
	DungeonSystem::SetFlag(d, kFlagStatueVid, (int32_t)ecs::PlayerRuntime::GetPacketVID(statue));

            DungeonSystem::SpawnRegen(d, kFloor2Regen, true);

            BigNoticeMap(mapIndex, "Elveszett Kastely: %d mp van a szobor aktivitasara (5 kulcs)!", kFloor2TimeSec);
            ScheduleTimer(mapIndex, kFloor2TimeSec, 2);
        }
    void StartFloor3(int32_t mapIndex)
    {
        const entt::entity d = CDungeonManager::instance().FindByMapIndex(mapIndex);
        if (d == entt::null)
            return;

        CancelTimer(mapIndex);

        DungeonSystem::KillAll(d);
        DungeonSystem::ClearRegen(d);

        DungeonSystem::SetFlag(d, kFlagFloor, 3);
        DungeonSystem::SetFlag(d, kFlagClonesRemain, 0);

        JumpToRoom(d, mapIndex, {kFloor3CenterX, kFloor3CenterY});

        LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(mapIndex);
        if (!map)
            return;

        const int32_t baseCellX = map->m_setting.iBaseX / 100;
        const int32_t baseCellY = map->m_setting.iBaseY / 100;

        std::vector<entt::entity> members;
        members.reserve(8);
        ForEachPcOnMap(mapIndex, [&](entt::entity pc){
            if (ecs::IsCharacter(pc)) members.push_back(pc); });

        std::sort(members.begin(), members.end(), [](entt::entity a, entt::entity b) {
            return ecs::PlayerRuntime::GetPlayerID(a) < ecs::PlayerRuntime::GetPlayerID(b);
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
            const entt::entity ownerEntity = members[i];
            const entt::entity sourceEntity = members[(i + 1) % members.size()];

            if (!ecs::IsCharacter(ownerEntity) || !ecs::IsCharacter(sourceEntity))
                continue;

            // owner GLOBAL cell -> LOCAL cell (private map uses LOCAL in many APIs)
            const int32_t ox = (ecs::PlayerRuntime::GetX(ownerEntity) / 100) - baseCellX;
            const int32_t oy = (ecs::PlayerRuntime::GetY(ownerEntity) / 100) - baseCellY;

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
            snprintf(evilName, sizeof(evilName), "Gonosz%s", ecs::PlayerRuntime::GetName(sourceEntity).data());

            // FAKE PC (nem mob!) - PID=0, hogy ne keruljon bele PC name/PID map-ekbe
            const entt::entity cloneEntity = CHARACTER_MANAGER::instance().SpawnMobEntity(
                kCloneBaseMobVnum, mapIndex, gx, gy, 0, false, -1, false);

            if (cloneEntity == entt::null)
                continue;

            // Keep monster combat rules and send the player's appearance.
            g_registry.emplace<ecs::LostCastleCloneAppearance>(cloneEntity);
            ecs::PlayerRuntime::SetName(cloneEntity, evilName);
            ecs::PlayerRuntime::SetRace(cloneEntity, static_cast<uint8_t>(g_registry.get<ecs::RaceState>(sourceEntity).baseRace));
            ecs::PlayerRuntime::SetEmpire(cloneEntity, ecs::PlayerRuntime::GetEmpire(sourceEntity));
            CombatSystem::SetPKMode(cloneEntity, PK_MODE_FREE);
            SkillSystem::SetSkillGroup(cloneEntity, SkillSystem::GetSkillGroup(sourceEntity));

            ecs::MovementSystem::SetRotation(cloneEntity, ecs::PlayerRuntime::GetRotation(sourceEntity));

            CopyCloneEquipment(cloneEntity, sourceEntity);

            for (int p = 0; p < POINT_MAX_NUM; ++p)
            {
                ecs::PointSystem::SetReal(cloneEntity, (uint8_t)p, ecs::PointSystem::Get(sourceEntity, (uint8_t)p));
                ecs::PointSystem::Set(cloneEntity, (uint8_t)p, ecs::PointSystem::Get(sourceEntity, (uint8_t)p));
            }

            // 10x erosites (HP/SP/DMG/STAT)
            ecs::PlayerRuntime::SetLevel(cloneEntity, (uint8_t)ecs::PointSystem::GetLevel(sourceEntity));
            ecs::PlayerRuntime::SetMaxHP(cloneEntity, (int64_t)ecs::PointSystem::GetMaxHP(sourceEntity) * STR_MULTIPLE);
            ecs::PlayerRuntime::SetMaxSP(cloneEntity, (int64_t)ecs::PointSystem::GetMaxSP(sourceEntity) * STR_MULTIPLE);
            ecs::PlayerRuntime::SetHP(cloneEntity, (int64_t)ecs::PointSystem::GetMaxHP(sourceEntity) * STR_MULTIPLE);
            ecs::PlayerRuntime::SetSP(cloneEntity, (int64_t)ecs::PointSystem::GetMaxSP(sourceEntity) * STR_MULTIPLE);

            MulPoint10(cloneEntity, POINT_ST);
            MulPoint10(cloneEntity, POINT_HT);
            MulPoint10(cloneEntity, POINT_DX);
            MulPoint10(cloneEntity, POINT_IQ);
            MulPoint10(cloneEntity, POINT_ATT_GRADE);
            MulPoint10(cloneEntity, POINT_DEF_GRADE);
            MulPoint10(cloneEntity, POINT_MAGIC_ATT_GRADE);
            MulPoint10(cloneEntity, POINT_MAGIC_DEF_GRADE);
            MulPoint10(cloneEntity, POINT_WEAPON_MIN);
            MulPoint10(cloneEntity, POINT_WEAPON_MAX);
            CombatSystem::SetKillerMode(cloneEntity, true);

            // Skillek: csak tamado skillek legyenek az AI listaban (<=127 a skill motion packet miatt)
            std::vector<uint8_t> skillList;
            skillList.reserve(32);
            for (uint32_t sv = 1; sv <= 255; ++sv)
            {
                const int lvl = SkillSystem::GetSkillLevel(sourceEntity, sv);
                if (lvl <= 0)
                    continue;

                SkillSystem::SetSkillLevel(cloneEntity, sv, (uint8_t)lvl);

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

            if (!ecs::MovementSystem::Show(cloneEntity, mapIndex, gx, gy, 0))
            {
                CHARACTER_MANAGER::instance().DestroyCharacter(cloneEntity);
                continue;
            }

            // register
	m_cloneAllowedPid[ecs::PlayerRuntime::GetPacketVID(cloneEntity)] = ecs::PlayerRuntime::GetPlayerID(ownerEntity);
	m_cloneMap[ecs::PlayerRuntime::GetPacketVID(cloneEntity)] = mapIndex;
	m_cloneSkills[ecs::PlayerRuntime::GetPacketVID(cloneEntity)] = std::move(skillList);

	m_clonePending.erase(ecs::PlayerRuntime::GetPacketVID(cloneEntity));
	m_cloneNextAction[ecs::PlayerRuntime::GetPacketVID(cloneEntity)] = get_dword_time() + 800;
	m_cloneOffset[ecs::PlayerRuntime::GetPacketVID(cloneEntity)] = std::make_pair((int16_t)number(-40, 40), (int16_t)number(-40, 40));

            ++remain;
        }

        DungeonSystem::SetFlag(d, kFlagClonesRemain, remain);
        if (remain == 0)
        {
            ClearDungeon(mapIndex);
            return;
        }

        BigNoticeMap(mapIndex, "Elveszett Kastely: Mindenki olje meg a sajat klonjat!");

        // AI tick: mozgas + skill + attack (NO mob state)
        ScheduleCloneAI(mapIndex);
    }



        void StartFloor4(int32_t mapIndex)
        {
            const entt::entity d = CDungeonManager::instance().FindByMapIndex(mapIndex);
            if (d == entt::null)
                return;

            CancelTimer(mapIndex);
            CancelCloneAI(mapIndex);
            ClearClonesOnMap(mapIndex);

            DungeonSystem::KillAll(d);
            DungeonSystem::ClearRegen(d);

            DungeonSystem::SetFlag(d, kFlagFloor, 4);
            DungeonSystem::SetFlag(d, kFlagTileStage, 0);
            DungeonSystem::SetFlag(d, kFlagTotemVid, 0);
            DungeonSystem::SetFlag(d, kFlagExitVid, 0);

            JumpToRoom(d, mapIndex, LostCastleLayout::TileRoomEntrance);
            DungeonSystem::SpawnRegen(d, kFloor4Regen, false);

            const auto point = LostCastleLayout::Totem;
            const entt::entity totem = DungeonSystem::SpawnMob(d, kTotemVnum, point.x, point.y);
            if (totem != entt::null)
	DungeonSystem::SetFlag(d, kFlagTotemVid, (int32_t)ecs::PlayerRuntime::GetPacketVID(totem));

            SendCommandMap(mapIndex, "lostcastle_tile 0");
            BigNoticeMap(mapIndex, "Elveszett Kastely: Ork amulett eshet a szörnyekből (3%%). Használd a totem közelében!");
        }

        void OnCloneKilled(int32_t mapIndex, uint32_t cloneVid)
        {
            auto it = m_cloneAllowedPid.find(cloneVid);
            if (it == m_cloneAllowedPid.end())
                return;

            m_cloneAllowedPid.erase(it);
            m_cloneMap.erase(cloneVid);
            m_cloneSkills.erase(cloneVid);
            m_clonePending.erase(cloneVid);
            m_cloneNextAction.erase(cloneVid);
            m_cloneOffset.erase(cloneVid);

            const entt::entity d = CDungeonManager::instance().FindByMapIndex(mapIndex);
            if (d == entt::null)
                return;

            int32_t remain = DungeonSystem::GetFlag(d, kFlagClonesRemain);
            if (remain > 0) remain -= 1;
            DungeonSystem::SetFlag(d, kFlagClonesRemain, remain);

            if (remain <= 0)
            {
                BigNoticeMap(mapIndex, "Elveszett Kastely: Minden klon elpusztult! Kovetkezik a 4. floor.");
                StartFloor4(mapIndex);
            }
        }

        void SpawnExit(entt::entity dungeon, int32_t mapIndex)
        {
            if (DungeonSystem::GetFlag(dungeon, kFlagExitVid))
                return;

            const auto point = LostCastleLayout::Exit;
            const auto exit = DungeonSystem::SpawnMob(dungeon, kTotemVnum, point.x, point.y);
            if (exit == entt::null)
            {
                LOG_ERROR("Lost Castle: could not spawn the exit totem on map {}", mapIndex);
                return;
            }
            DungeonSystem::SetFlag(dungeon, kFlagExitVid, ecs::PlayerRuntime::GetPacketVID(exit));
            BigNoticeMap(mapIndex, "A híd elkészült. A túloldali totemnél fejezheted be a kazamatát.");
        }

        bool UnlockNextTile(int32_t mapIndex)
        {
            const entt::entity d = CDungeonManager::instance().FindByMapIndex(mapIndex);
            if (d == entt::null)
                return false;

            int32_t stage = DungeonSystem::GetFlag(d, kFlagTileStage);
            if (stage >= kTileStages)
                return false;

            if (!ApplyBlockRect(mapIndex, LostCastleLayout::Tiles[stage], false))
                return false;

            stage += 1;
            DungeonSystem::SetFlag(d, kFlagTileStage, stage);

            SendCommandMap(mapIndex, "lostcastle_tile %d", stage);
            BigNoticeMap(mapIndex, "Elveszett Kastely: Letettel egy csempet (%d/%d)!", stage, kTileStages);
            if (stage == kTileStages)
                SpawnExit(d, mapIndex);
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

        const entt::entity d = CDungeonManager::instance().FindByMapIndex(mapIndex);
        if (d == entt::null)
        {
            s_lc.CancelTimer(mapIndex);
            return 0;
        }

        if (DungeonSystem::GetFlag(d, kFlagWasCompleted) != 0)
        {
            s_lc.CancelTimer(mapIndex);
            return 0;
        }

        if (DungeonSystem::GetFlag(d, kFlagFloor) != floor)
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
            const entt::entity d = CDungeonManager::instance().FindByMapIndex(mapIndex);
            if (d == entt::null || DungeonSystem::GetFlag(d, kFlagWasCompleted) != 0 || DungeonSystem::GetFlag(d, kFlagFloor) != 3)
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

            const entt::entity cloneEntity = CHARACTER_MANAGER::instance().FindEntity(cloneVid);

            if (!ecs::IsCharacter(cloneEntity) || CombatSystem::IsDead(cloneEntity))
                continue;

            const auto owner = s_lc.m_cloneAllowedPid.find(cloneVid);
            if (owner == s_lc.m_cloneAllowedPid.end())
                continue;

            const entt::entity targetEntity = ecs::PlayerRuntime::FindByPlayerID(owner->second);

            if (!ecs::IsCharacter(targetEntity) || !ecs::PlayerRuntime::IsPC(targetEntity)
                || ecs::PlayerRuntime::GetMapIndex(targetEntity) != mapIndex || CombatSystem::IsDead(targetEntity))
            {
                s_lc.m_clonePending.erase(cloneVid);
                continue;
            }

            const int32_t dist = DISTANCE_APPROX(ecs::PlayerRuntime::GetX(targetEntity) - ecs::PlayerRuntime::GetX(cloneEntity), ecs::PlayerRuntime::GetY(targetEntity) - ecs::PlayerRuntime::GetY(cloneEntity));

            // 1) Pending hit: damage only when we previously started an animation
            auto pendIt = s_lc.m_clonePending.find(cloneVid);
            if (pendIt != s_lc.m_clonePending.end())
            {
                // IMPORTANT: unordered_map iterators can become invalid after a rehash.
                // Copy the payload and erase by KEY only (safe).
                const SClonePending p = pendIt->second;

                // cancel if target got too far
                const int32_t maxPendDist = p.isSkill ? 900 : 650;
                if (p.targetVid != ecs::PlayerRuntime::GetPacketVID(targetEntity) || dist > maxPendDist)
                {
                    s_lc.m_clonePending.erase(cloneVid);
                    continue;
                }

                if (now >= p.executeTime)
                {
                    bool didAction = false;
                    if (p.isSkill)
                    {
                        didAction = SkillSystem::UseSkill(cloneEntity, p.attackType, targetEntity, true);

                        if (didAction)
                        {
                            CSkillProto* sk = CSkillManager::instance().Get(p.attackType);
                            if (sk && IS_SET(sk->dwFlag, SKILL_FLAG_ATTACK))
                            {
                                if (p.attackType != SKILL_BYEURAK && p.attackType != SKILL_MUYEONG && !sk->IsChargeSkill())
                                    SkillSystem::ComputeSkill(cloneEntity, p.attackType, targetEntity, 0);
                            }
                        }
                    }
                    else
                    {
                        didAction = CombatSystem::Attack(cloneEntity,
                            targetEntity, 0);
                    }

                    if (didAction)
                    {
                        // Force victim hurt animation like real PvP
                        ecs::MovementSystem::Motion(targetEntity, MOTION_DAMAGE, cloneEntity);
                    }

                    s_lc.m_clonePending.erase(cloneVid);
                }

                // while pending exists (not executed yet), do not chase/attack again
                continue;
            }

            // 2) Chase: tartsunk egy stabil melee tavolsagot, ne fusson at rajtad es ne jittereljen
            const int32_t desired = 60; // kb. 1.7m

            if (dist < desired - 40)
            {
                const int32_t dx = ecs::PlayerRuntime::GetX(cloneEntity) - ecs::PlayerRuntime::GetX(targetEntity);
                const int32_t dy = ecs::PlayerRuntime::GetY(cloneEntity) - ecs::PlayerRuntime::GetY(targetEntity);
                float len = sqrtf((float)dx * (float)dx + (float)dy * (float)dy);
                if (len < 1.0f) len = 1.0f;

                int16_t ox = 0, oy = 0;
                auto offIt = s_lc.m_cloneOffset.find(cloneVid);
                if (offIt != s_lc.m_cloneOffset.end())
                {
                    ox = offIt->second.first;
                    oy = offIt->second.second;
                }

                int32_t tx = ecs::PlayerRuntime::GetX(targetEntity) + (int32_t)((dx / len) * desired) + ox;
                int32_t ty = ecs::PlayerRuntime::GetY(targetEntity) + (int32_t)((dy / len) * desired) + oy;

                if (SECTREE_MANAGER::instance().IsMovablePosition(mapIndex, tx, ty))
                    LostCastleCloneStartMove(cloneEntity, tx, ty, now);

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

                int32_t tx = ecs::PlayerRuntime::GetX(targetEntity);
                int32_t ty = ecs::PlayerRuntime::GetY(targetEntity);

                const int32_t dx = tx - ecs::PlayerRuntime::GetX(cloneEntity);
                const int32_t dy = ty - ecs::PlayerRuntime::GetY(cloneEntity);
                float len = sqrtf((float)dx * (float)dx + (float)dy * (float)dy);
                if (len < 1.0f) len = 1.0f;

                // celpont: a target ele (de nem ra), fixed offsettel
                tx = tx - (int32_t)((dx / len) * desired) + ox;
                ty = ty - (int32_t)((dy / len) * desired) + oy;

                if (!SECTREE_MANAGER::instance().IsMovablePosition(mapIndex, tx, ty))
                {
                    tx = ecs::PlayerRuntime::GetX(targetEntity);
                    ty = ecs::PlayerRuntime::GetY(targetEntity);
                }

                LostCastleCloneStartMove(cloneEntity, tx, ty, now);
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
                    if (!SkillSystem::CanUseSkill(cloneEntity, sv))
                        continue;
                    chosenSkill = sv;
                    break;
                }
            }

            if (chosenSkill)
            {
                LostCastleCloneBroadcastSkill(cloneEntity, targetEntity, chosenSkill, now);

                SClonePending p;
	p.targetVid = ecs::PlayerRuntime::GetPacketVID(targetEntity);
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
            LostCastleCloneBroadcastMelee(cloneEntity, targetEntity, motion, now);

            SClonePending p;
	p.targetVid = ecs::PlayerRuntime::GetPacketVID(targetEntity);
            p.attackType = 0;
            p.motionArg = motion;
            p.isSkill = false;
            p.executeTime = now + CalcMeleeHitDelayMs(cloneEntity, motion);
            s_lc.m_clonePending[cloneVid] = p;

            nextTime = now + CalcAttackIntervalMs(cloneEntity);
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


bool CLostCastleDungeon::SpawnTestClones(entt::entity source, entt::entity target, int32_t count)
{
    if (!ecs::IsCharacter(source) || !ecs::PlayerRuntime::IsValid(target))
        return false;
    if (!ecs::PlayerRuntime::IsPC(source) || !ecs::PlayerRuntime::IsPC(target))
        return false;

    if (count <= 0) count = 1;
    if (count > 20) count = 20;

    const int32_t mapIndex = ecs::PlayerRuntime::GetMapIndex(target);

    // Spawn near target (global coords)
    int32_t spawned = 0;
    for (int32_t n = 0; n < count; ++n)
    {
        int32_t gx = ecs::PlayerRuntime::GetX(target);
        int32_t gy = ecs::PlayerRuntime::GetY(target);

        // try random nearby points
        for (int t = 0; t < 25; ++t)
        {
            const int dx = number(-400, 400);
            const int dy = number(-400, 400);
            const int32_t tx = ecs::PlayerRuntime::GetX(target) + dx;
            const int32_t ty = ecs::PlayerRuntime::GetY(target) + dy;
            if (SECTREE_MANAGER::instance().Get(mapIndex, tx, ty))
            {
                gx = tx;
                gy = ty;
                break;
            }
        }

        char cloneName[CHARACTER_NAME_MAX_LEN + 1];
        // unique name to avoid collisions
        snprintf(cloneName, sizeof(cloneName), "Gonosz %s", ecs::PlayerRuntime::GetName(source).data());

        const entt::entity cloneEntity = CHARACTER_MANAGER::instance().CreateCharacterEntity(cloneName, 0);

        if (cloneEntity == entt::null)
            continue;


        ecs::PlayerRuntime::SetRace(cloneEntity, (uint8_t)ecs::PlayerRuntime::GetRaceNum(source));
        ecs::PlayerRuntime::SetEmpire(cloneEntity, ecs::PlayerRuntime::GetEmpire(source));
        CombatSystem::SetPKMode(cloneEntity, PK_MODE_FREE);
        SkillSystem::SetSkillGroup(cloneEntity, SkillSystem::GetSkillGroup(source));

        ecs::MovementSystem::SetRotation(cloneEntity, ecs::PlayerRuntime::GetRotation(source));

        // look/parts
        CopyCloneEquipment(cloneEntity, source);

        // copy points (NO 10x here - real PvP test)
        for (int p = 0; p < POINT_MAX_NUM; ++p)
        {
            ecs::PointSystem::SetReal(cloneEntity, (uint8_t)p, ecs::PointSystem::Get(source, (uint8_t)p));
            ecs::PointSystem::Set(cloneEntity, (uint8_t)p, ecs::PointSystem::Get(source, (uint8_t)p));
        }
        ecs::PlayerRuntime::SetLevel(cloneEntity, (uint8_t)ecs::PointSystem::GetLevel(source));
        ecs::PlayerRuntime::SetMaxHP(cloneEntity, ecs::PointSystem::GetMaxHP(source)* STR_MULTIPLE);
        ecs::PlayerRuntime::SetMaxSP(cloneEntity, ecs::PointSystem::GetMaxSP(source) * STR_MULTIPLE);
        ecs::PlayerRuntime::SetHP(cloneEntity, ecs::PointSystem::GetMaxHP(source) * STR_MULTIPLE);
        ecs::PlayerRuntime::SetSP(cloneEntity, ecs::PointSystem::GetMaxSP(source) * STR_MULTIPLE);
        CombatSystem::SetKillerMode(cloneEntity, true);

        // Skills (AI uses only ATTACK skills)
        std::vector<uint8_t> skillList;
        skillList.reserve(32);
        for (uint32_t sv = 1; sv <= 255; ++sv)
        {
            const int lvl = SkillSystem::GetSkillLevel(source, sv);
            if (lvl <= 0)
                continue;

            SkillSystem::SetSkillLevel(cloneEntity, sv, (uint8_t)lvl);

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

        if (!ecs::MovementSystem::Show(cloneEntity, mapIndex, gx, gy, 0))
        {
            CHARACTER_MANAGER::instance().DestroyCharacter(cloneEntity);
            continue;
        }

        // Register: only the target can fight this clone, and the clone targets the target
	s_lc.m_cloneAllowedPid[ecs::PlayerRuntime::GetPacketVID(cloneEntity)] = ecs::PlayerRuntime::GetPlayerID(target);
	s_lc.m_cloneMap[ecs::PlayerRuntime::GetPacketVID(cloneEntity)] = mapIndex;
	s_lc.m_cloneSkills[ecs::PlayerRuntime::GetPacketVID(cloneEntity)] = std::move(skillList);

	s_lc.m_clonePending.erase(ecs::PlayerRuntime::GetPacketVID(cloneEntity));
	s_lc.m_cloneNextAction[ecs::PlayerRuntime::GetPacketVID(cloneEntity)] = get_dword_time() + 800;
	s_lc.m_cloneOffset[ecs::PlayerRuntime::GetPacketVID(cloneEntity)] = std::make_pair((int16_t)number(-40, 40), (int16_t)number(-40, 40));

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
        s_lc.m_cloneSkills.erase(vid);
        s_lc.m_clonePending.erase(vid);
        s_lc.m_cloneNextAction.erase(vid);
        s_lc.m_cloneOffset.erase(vid);

        if (const entt::entity c = CHARACTER_MANAGER::instance().FindEntity(vid); ecs::IsCharacter(c))
            CHARACTER_MANAGER::instance().DestroyCharacter(c);

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

void CLostCastleDungeon::OnPlayerDisconnect(entt::entity character)
{
    if (!ecs::PlayerRuntime::IsPC(character))
        return;

    const int32_t idx = ecs::PlayerRuntime::GetMapIndex(character);
    if (!IsInRange(idx, kPrivateMin, kPrivateMax))
        return;

    const time_t now = get_global_time();
    ecs::QuestSystem::SetFlag(character, kQfDisconnect, now + kRejoinSeconds);
    ecs::QuestSystem::SetFlag(character, kQfIdx, idx);
    ecs::QuestSystem::SetFlag(character, kQfCh, (int32_t)g_bChannel);
}

bool CLostCastleDungeon::TryHandleNpcClick(entt::entity character, entt::entity npc)
{
    namespace P = ecs::PlayerRuntime;
    const auto map = P::GetMapIndex(npc);
    const auto vnum = P::GetRaceNum(npc);
    const bool entrance = map == kLobbyMap && vnum == kEntryNpcVnum
        && P::GetX(npc) == kLobbyX && P::GetY(npc) == kLobbyY;
    const auto dungeon = IsInRange(map, kPrivateMin, kPrivateMax) && vnum == kTotemVnum
        ? CDungeonManager::instance().FindByMapIndex(map) : entt::null;
    const bool exit = g_registry.valid(dungeon)
        && P::GetPacketVID(npc) == static_cast<std::uint32_t>(DungeonSystem::GetFlag(dungeon, kFlagExitVid));
    const bool bridgeTotem = g_registry.valid(dungeon)
        && DungeonSystem::GetFlag(dungeon, kFlagTileStage) == kTileStages
        && !DungeonSystem::GetFlag(dungeon, kFlagWasCompleted)
        && !DungeonSystem::GetFlag(dungeon, kFlagExitVid)
        && P::GetPacketVID(npc) == static_cast<std::uint32_t>(DungeonSystem::GetFlag(dungeon, kFlagTotemVid));
    if (!entrance && !exit && !bridgeTotem)
        return false;
    if (P::GetMapIndex(character) != map || CombatSystem::IsDead(character)
        || DISTANCE_APPROX(P::GetX(character) - P::GetX(npc), P::GetY(character) - P::GetY(npc)) > 550)
        return true;

    if (exit)
    {
        if (ecs::SocialSystem::GetDungeon(character) == dungeon
            && !DungeonSystem::GetFlag(dungeon, kFlagWasCompleted))
        {
            AurigaMissionPass::Events::Complete(map);
            s_lc.ClearDungeon(map);
        }
        return true;
    }
    if (bridgeTotem)
    {
        if (ecs::SocialSystem::GetDungeon(character) == dungeon)
            s_lc.SpawnExit(dungeon, map);
        return true;
    }
    return OnClickNpc(character);
}

void CLostCastleDungeon::OnPlayerLogin(entt::entity character)
{
    if (!ecs::PlayerRuntime::IsPC(character))
        return;

    const int32_t idx = ecs::PlayerRuntime::GetMapIndex(character);
    if (!IsInRange(idx, kPrivateMin, kPrivateMax))
        return;

    // IMPORTANT: don't set warp location to kOriginalMap here.
    // Lobby is set at entry time (npc click) to where the player came from.
    ecs::QuestSystem::SetFlag(character, kQfIdx, idx);
    ecs::QuestSystem::SetFlag(character, kQfCh, (int32_t)g_bChannel);
    const auto dungeon = CDungeonManager::instance().FindByMapIndex(idx);
    if (g_registry.valid(dungeon) && !DungeonSystem::GetFlag(dungeon, kFlagWasCompleted))
    {
        ecs::SocialSystem::SetDungeon(character, dungeon);
        ecs::QuestSystem::SetFlag(character, kQfDisconnect, 0);
    }
    ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "lostcastle_tile %d",
        DungeonSystem::GetFlag(dungeon, kFlagTileStage));
}

bool CLostCastleDungeon::OnClickNpc(entt::entity character)
{
    if(TypedDungeonEntryPending(character))return true;
    if (!ecs::PlayerRuntime::IsPC(character))
        return false;

    // save "lobby" as where the NPC was clicked (like your flow expects)
    const int32_t lobbyMap = ecs::PlayerRuntime::GetMapIndex(character);
    const int32_t lobbyX = ecs::PlayerRuntime::GetX(character) / 100;
    const int32_t lobbyY = ecs::PlayerRuntime::GetY(character) / 100;

    const auto savedMap = ecs::QuestSystem::GetFlag(character, kQfIdx);
    if (lobbyMap == kLobbyMap && LostCastleLayout::IsPrivateMap(savedMap)
        && ecs::QuestSystem::GetFlag(character, kQfDisconnect) > get_global_time())
    {
        if (ecs::QuestSystem::GetFlag(character, kQfCh) != g_bChannel)
        {
            ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "A folyamatban lévő Elveszett Kastélyhoz az eredeti csatornán csatlakozhatsz vissza.");
            return true;
        }

        const auto dungeon = CDungeonManager::instance().FindByMapIndex(savedMap);
        const auto* map = SECTREE_MANAGER::instance().GetMap(savedMap);
        if (g_registry.valid(dungeon) && map && !DungeonSystem::GetFlag(dungeon, kFlagWasCompleted))
        {
            const auto floor = DungeonSystem::GetFlag(dungeon, kFlagFloor);
            auto point = LostCastleLayout::Entrance;
            if (floor == 2)
                point = LostCastleLayout::KeyRoomEntrance;
            else if (floor == 4)
                point = LostCastleLayout::TileRoomEntrance;
            const auto world = LostCastleLayout::WorldCell(point, map->m_setting.iBaseX, map->m_setting.iBaseY);
            DungeonSystem::Join_Coords(dungeon, character, world.x, world.y, lobbyMap);
            return true;
        }
        ecs::QuestSystem::SetFlag(character, kQfDisconnect, 0);
    }

    const entt::entity party = ecs::SocialSystem::GetParty(character);

    if (party != entt::null)
    {
        if (PartySystem::GetLeaderPID(party) != ecs::PlayerRuntime::GetPlayerID(character))
        {
            ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Csak a party leader indithatja!");
            return true;
        }

        bool ok = true;
        const int32_t leaderMap = ecs::PlayerRuntime::GetMapIndex(character);

        auto check = [&](entt::entity m){
                if (!ecs::PlayerRuntime::IsPC(m))
                    return;

                if (ecs::PlayerRuntime::GetMapIndex(m) != leaderMap)
                {
                    ok = false;
                    return;
                }
                if (ecs::PointSystem::GetLevel(m) < kMinLevel || ecs::PointSystem::GetLevel(m) > kMaxLevel)
                {
                    ok = false;
                    return;
                }
                if (TypedDungeonTicketCount(m, kEntryTicketCode) < LostCastleLayout::TicketCount)
                {
                    ok = false;
                    return;
                }
            };

        PartySystem::ForEachOnlineMember(party, check);

        if (!ok)
        {
            ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Minden tagnak itt kell lennie. Szükséges szint: %d–%d. Belépő: 1 Árpa.", kMinLevel, kMaxLevel);
            return true;
        }
    }
    else
    {
        if (ecs::PointSystem::GetLevel(character) < kMinLevel || ecs::PointSystem::GetLevel(character) > kMaxLevel)
        {
            ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Csak lvl %d-%d kozott lephetsz be!", kMinLevel, kMaxLevel);
            return true;
        }
        if (TypedDungeonTicketCount(character, kEntryTicketCode) < LostCastleLayout::TicketCount)
        {
            ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Szükséges belépő: 1 Árpa.");
            return true;
        }
    }

    // Create + join (Pyramid style)
    auto prepare=[=]() -> std::optional<TypedDungeonEntryPlan> {
    const entt::entity d = CDungeonManager::instance().Create(kOriginalMap);
    if (d == entt::null)
    {
        ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Elveszett Kastely: nem sikerult letrehozni a dungeont.");
        return std::nullopt;
    }

    const auto mapIndex = DungeonSystem::GetMapIndex(d);
    if (!CastleLayoutAvailable(mapIndex) || !InstallCastleWalls(mapIndex))
    {
        LOG_ERROR("Lost Castle: invalid layout on private map {}", mapIndex);
        CancelTypedDungeonPreflight(d);
        return std::nullopt;
    }
    const auto& bounds = SECTREE_MANAGER::instance().GetMap(mapIndex)->m_setting;
    const auto entry = LostCastleLayout::WorldCell(LostCastleLayout::Entrance, bounds.iBaseX, bounds.iBaseY);

    DungeonSystem::SetFlag(d, kFlagFloor, 1);
    DungeonSystem::SetFlag(d, kFlagWasCompleted, 0);
    DungeonSystem::SetFlag(d, kFlagCorrectMetin, 0);
    DungeonSystem::SetFlag(d, kFlagKeyMask, 0);
    DungeonSystem::SetFlag(d, kFlagStatueVid, 0);
    DungeonSystem::SetFlag(d, kFlagTotemVid, 0);
    DungeonSystem::SetFlag(d, kFlagTileStage, 0);
    DungeonSystem::SetFlag(d, kFlagClonesRemain, 0);

        TypedDungeonEntryPlan plan;plan.dungeon=d;plan.cancel=[d]{CancelTypedDungeonPreflight(d);};plan.publish=[=]{
    auto applyMember = [&](entt::entity m){
            if (!ecs::PlayerRuntime::IsPC(m))
                return;


            // rejoin flags reset
            ecs::QuestSystem::SetFlag(m, kQfDisconnect, 0);
            ecs::QuestSystem::SetFlag(m, kQfIdx, DungeonSystem::GetMapIndex(d));
            ecs::QuestSystem::SetFlag(m, kQfCh, (int32_t)g_bChannel);

            // exit/lobby
            ecs::MovementSystem::SetWarpLocation(m, lobbyMap, lobbyX, lobbyY);
        };

    if (party == entt::null)
    {
        applyMember(character);

        DungeonSystem::Join_Coords(d, character, entry.x, entry.y, lobbyMap);
    }
    else
    {
        PartySystem::ForEachOnMapMember(party, applyMember, ecs::PlayerRuntime::GetMapIndex(character));

        DungeonSystem::JoinParty_Coords(d, party, entry.x, entry.y, lobbyMap);
    }

    s_lc.StartFloor1(DungeonSystem::GetMapIndex(d));
        };return plan;
    };
    if(!BeginTypedDungeonEntry(character,"dungeon.lost_castle.entry",kEntryTicketCode,LostCastleLayout::TicketCount,[character](entt::entity pc){const auto level=ecs::PointSystem::GetLevel(pc);if(level<kMinLevel || level>kMaxLevel)return false;bool together=true;const auto party=ecs::SocialSystem::GetParty(character);if(party!=entt::null){auto check=[&](entt::entity m){if(ecs::PlayerRuntime::IsPC(m) && ecs::PlayerRuntime::GetMapIndex(m)!=ecs::PlayerRuntime::GetMapIndex(character))together=false;};PartySystem::ForEachOnlineMember(party,check);}return together;},prepare))
        ecs::ChatSystem::Send(character,CHAT_TYPE_INFO,"A teljes csapatnak készen kell állnia, és a belépőnek a hátizsákban kell lennie.");
    return true;
}

void CLostCastleDungeon::OnMobKilled(entt::entity killer, entt::entity victim)
{
    if (!ecs::PlayerRuntime::IsValid(killer) || !ecs::PlayerRuntime::IsValid(victim) ||
        !ecs::PlayerRuntime::IsPC(killer) ||
        ecs::PlayerRuntime::GetMapIndex(killer) != ecs::PlayerRuntime::GetMapIndex(victim))
        return;

    const int32_t idx = ecs::PlayerRuntime::GetMapIndex(victim);
    if (!IsInRange(idx, kPrivateMin, kPrivateMax))
        return;

    const entt::entity d = CDungeonManager::instance().FindByMapIndex(idx);
    if (d == entt::null || DungeonSystem::GetFlag(d, kFlagWasCompleted))
        return;

    const int32_t floor = DungeonSystem::GetFlag(d, kFlagFloor);

    if (floor == 1 && ecs::PlayerRuntime::GetRaceNum(victim) == kMetinVnum)
    {
        const uint32_t correctVid = (uint32_t)DungeonSystem::GetFlag(d, kFlagCorrectMetin);
	if (correctVid && ecs::PlayerRuntime::GetPacketVID(victim) == correctVid)
        {
            BigNoticeMap(idx, "Elveszett Kastely: Megtalaltatok a megfelelõ metinkõvet! Floor2 kovetkezik.");
            s_lc.StartFloor2(idx);
        }
        return;
    }

    if (floor == 2 && ecs::PlayerRuntime::IsMonster(victim))
    {
        using Role = auriga::item::DungeonUseRole;
        const auto used = DungeonSystem::GetFlag(d, kFlagKeyMask);
        const auto next = DungeonSystem::GetFlag(d, kFlagNextKeyDrop);
        for (int offset = 0; offset < 5; ++offset)
        {
            const auto index = (next + offset) % 5;
            if (used & (1 << index))
                continue;
            if (DropCastleItem(victim, killer, static_cast<Role>(static_cast<int>(Role::CastleKey1) + index)))
                DungeonSystem::SetFlag(d, kFlagNextKeyDrop, (index + 1) % 5);
            break;
        }
        return;
    }

    if (floor == 4)
    {
        if (ecs::PlayerRuntime::IsPC(killer) && ecs::PlayerRuntime::IsMonster(victim))
        {
            const uint16_t vnum = ecs::PlayerRuntime::GetRaceNum(victim);
            if (vnum != kTotemVnum && vnum != kStatueVnum)
            {
                if (DungeonSystem::GetFlag(d, kFlagTileStage) < kTileStages && number(1, 100) <= kTileDropChancePct)
                    DropCastleItem(victim, killer, auriga::item::DungeonUseRole::CastleTile);
            }
        }
        return;
    }
}

static std::optional<TypedDungeonAction> PlanCastleAction(entt::entity from,entt::entity npc,auriga::item::DungeonUseRole role){
 namespace P=ecs::PlayerRuntime;using R=auriga::item::DungeonUseRole;
 if(!P::IsPC(from)||!P::IsValid(npc)||P::GetMapIndex(from)!=P::GetMapIndex(npc))return {};
 const auto idx=P::GetMapIndex(from);if(!IsInRange(idx,kPrivateMin,kPrivateMax))return {};
 const auto d=CDungeonManager::instance().FindByMapIndex(idx);if(!g_registry.valid(d))return {};
 const auto floor=DungeonSystem::GetFlag(d,kFlagFloor);
 if(floor==2&&role>=R::CastleKey1&&role<=R::CastleKey5){
  const auto statueVid=static_cast<std::uint32_t>(DungeonSystem::GetFlag(d,kFlagStatueVid));if(!statueVid||P::GetPacketVID(npc)!=statueVid)return {};
  const auto keyIndex=static_cast<unsigned>(role)-static_cast<unsigned>(R::CastleKey1);int32_t mask=DungeonSystem::GetFlag(d,kFlagKeyMask);const int32_t bit=1<<keyIndex;if(mask&bit)return {};
return TypedDungeonAction{100+static_cast<std::uint64_t>(mask), [=]()mutable{
mask |= bit;
        DungeonSystem::SetFlag(d, kFlagKeyMask, mask);

        BigNoticeMap(idx, "Elveszett Kastely: %s megtalalta az egyik kulcsot!", ecs::PlayerRuntime::GetName(from).data());

        if (mask == ((1 << 5) - 1))
        {
            BigNoticeMap(idx, "Elveszett Kastely: Megvan mind az 5 kulcs! Floor3 kovetkezik.");

            ForEachPcOnMap(idx, [&](entt::entity pc){
                if (!ecs::PlayerRuntime::IsValid(pc)) return;
                for (const auto& binding : auriga::item::DungeonUseBindings)
                    if (binding.role >= R::CastleKey1 && binding.role <= R::CastleKey5)
                        QueueTypedDungeonCleanup(pc, binding.code);
                });

            s_lc.StartFloor3(idx);
        }
}};
 }
 if(floor==4&&role==R::CastleTile){const auto vid=static_cast<std::uint32_t>(DungeonSystem::GetFlag(d,kFlagTotemVid));const auto stage=DungeonSystem::GetFlag(d,kFlagTileStage);
  if(!vid||P::GetPacketVID(npc)!=vid||stage<0||stage>=kTileStages)return {};
return TypedDungeonAction{1000+static_cast<std::uint64_t>(stage), [=]()mutable{
if (!s_lc.UnlockNextTile(idx))
            ecs::ChatSystem::Send(from, CHAT_TYPE_INFO, "Mar minden csempe le van teve!");
}};
 }return {};
}
bool CLostCastleDungeon::OnUseTypedItem(entt::entity e,const auriga::item::PersistentItem& item,const auriga::item::ItemCatalog& catalog){
 const auto role=auriga::item::DungeonItemUseRole(catalog,item.key);return UseTypedDungeonAction(e,item,catalog,[role](auto pc,auto npc){return PlanCastleAction(pc,npc,role);});
}

bool CLostCastleDungeon::CheckCloneDamage(entt::entity attacker, entt::entity victim) const
{
    if (!ecs::PlayerRuntime::IsValid(attacker) || !ecs::PlayerRuntime::IsValid(victim))
        return true;

	const uint32_t aVid = ecs::PlayerRuntime::GetPacketVID(attacker);
	const uint32_t vVid = ecs::PlayerRuntime::GetPacketVID(victim);

    const bool attackerIsClone = s_lc.IsClone(aVid);
    const bool victimIsClone = s_lc.IsClone(vVid);

    // normal damage
    if (!attackerIsClone && !victimIsClone)
        return true;

    if (attackerIsClone)
        return !victimIsClone && ecs::PlayerRuntime::IsPC(victim)
            && s_lc.IsCloneAttackAllowed(aVid, ecs::PlayerRuntime::GetPlayerID(victim));
    return ecs::PlayerRuntime::IsPC(attacker)
        && s_lc.IsCloneAttackAllowed(vVid, ecs::PlayerRuntime::GetPlayerID(attacker));
}

void CLostCastleDungeon::OnCloneDeath(entt::entity victim)
{
    s_lc.OnCloneKilled(ecs::PlayerRuntime::GetMapIndex(victim), ecs::PlayerRuntime::GetPacketVID(victim));
}
