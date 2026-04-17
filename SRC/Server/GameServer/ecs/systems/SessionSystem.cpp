#include "../../stdafx.h"

#include "SessionSystem.hpp"

#include "../../char.h"
#include "../../char_manager.h"
#include "../../cmd.h"
#include "../../config.h"
#include "../../db.h"
#include "../../desc.h"
#include "../../desc_client.h"
#include "../../arena.h"
#include "../../guild.h"
#include "../../item.h"
#include "../../item_manager.h"
#include "../../log.h"
#include "../../map_location.h"
#include "../../marriage.h"
#include "../../messenger_manager.h"
#include "../../mob_manager.h"
#include "../../packet.h"
#include "../../pcbang.h"
#include "../../p2p.h"
#include "../../party.h"
#include "../../pvp.h"
#include "../EventDispatcher.hpp"
#include "../SpatialHelpers.hpp"
#include "../VIDRegistry.hpp"
#include "../events.hpp"
#include "../../questmanager.h"
#include "../../safebox.h"
#include "../../sectree.h"
#include "../../sectree_manager.h"
#include "../../shop.h"
#include "../../start_position.h"
#include "../../target.h"
#include "../../utils.h"
#ifdef ENABLE_BATTLE_PASS
#include "../../battle_pass.h"
#endif
#ifdef ENABLE_CPP_DUNGEON_RAZOR93
#include "../../OrcsDungeon.h"
#include "../../TritonTempleDungeon.h"
#include "../../ValentineDungeon.h"
#include "../../RuneDungeon.h"
#include "../../PyramidDungeonRazor93.h"
#include "../../NightmareDungeonRazor93.h"
#include "../../Halloween2022Dungeon.h"
#include "../../VikingDungeon.h"
#include "../../EasterDungeon.h"
#endif
#ifdef __ENABLE_NEW_OFFLINESHOP__
#include "../../new_offlineshop.h"
#include "../../new_offlineshop_manager.h"
#endif
#ifdef ENABLE_SWITCHBOT
#include "../../new_switchbot.h"
#endif

EVENTFUNC(save_event);
extern bool IS_SUMMONABLE_ZONE(int map_index);

namespace {

inline int32_t NormalizeMapIndex(int32_t mapIndex)
{
    if (mapIndex > 10000)
        mapIndex /= 10000;

    return mapIndex;
}

bool CheckAndHandleSameHwid(LPCHARACTER ch)
{
    if (!ch || !ch->IsPC())
        return false;

    DESC* desc = ch->GetDesc();
    if (!desc)
        return false;

    const char* selfHwid = desc->GetHwid();
    const char* selfHost = desc->GetHostName();

    if (!selfHwid || !*selfHwid)
        return false;

    if (!selfHost || !*selfHost)
        return false;

    int32_t normalizedMapIndex = NormalizeMapIndex(ch->GetMapIndex());
    bool duplicateFound = false;

    CHARACTER_MANAGER::instance().for_each_pc([&](LPCHARACTER other)
        {
            if (duplicateFound || !other || other == ch)
                return;

            if (!other->IsPC())
                return;

            DESC* otherDesc = other->GetDesc();
            if (!otherDesc)
                return;

            int32_t otherMapIndex = NormalizeMapIndex(other->GetMapIndex());
            if (otherMapIndex != normalizedMapIndex)
                return;

            const char* otherHwid = otherDesc->GetHwid();
            const char* otherHost = otherDesc->GetHostName();

            if (!otherHwid || !*otherHwid)
                return;

            if (!otherHost || !*otherHost)
                return;

            if (strcmp(selfHwid, otherHwid) == 0 && strcmp(selfHost, otherHost) == 0)
                duplicateFound = true;
        });

    if (duplicateFound)
    {
        char notice[256];
        snprintf(notice, sizeof(notice),
            "[EVENTMAP]%s 2 karakterrel probalt belepni event mapra!",
            ch->GetName());

        BroadcastNotice(notice);
        ch->WarpSet(983500, 265200, 41);
        return true;
    }

    return false;
}

#ifdef ENABLE_BATTLE_PASS_STAY_ONLINE
EVENTFUNC(battle_pass_stay_online_event_session)
{
    char_event_info* info = dynamic_cast<char_event_info*>(event->info);
    if (!info || !info->ch)
        return 0;

    LPCHARACTER ch = info->ch;

    if (!ch->GetDesc())
        return PASSES_PER_SEC(60);

    const uint8_t bBattlePassId = ch->GetBattlePassId();
    if (!bBattlePassId)
        return PASSES_PER_SEC(60);

    uint32_t dwNotUsed = 0;
    uint32_t dwCount = 0;
    if (!CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, STAY_ONLINE_MINUTES, &dwNotUsed, &dwCount))
        return PASSES_PER_SEC(60);

    if (ch->IsCompletedMission(STAY_ONLINE_MINUTES))
        return PASSES_PER_SEC(60);

    if (ch->GetMissionProgress(STAY_ONLINE_MINUTES, bBattlePassId) >= dwCount)
        return PASSES_PER_SEC(60);

    ch->UpdateMissionProgress(STAY_ONLINE_MINUTES, bBattlePassId, 1, dwCount);
    return PASSES_PER_SEC(60);
}
#endif

} // namespace

bool CAN_ENTER_ZONE(const LPCHARACTER& ch, int map_index)
{
    switch (map_index)
    {
    case 301:
    case 302:
    case 303:
    case 304:
        if (ch->GetLevel() < 90)
            return false;
    }
    return true;
}

void CHARACTER::Save()
{
    if (!m_bSkipSave)
        CHARACTER_MANAGER::instance().DelayedSave(this);
}

void CHARACTER::CreatePlayerProto(TPlayerTable& tab)
{
    memset(&tab, 0, sizeof(TPlayerTable));

    if (GetNewName().empty())
    {
        strlcpy(tab.name, GetName(), sizeof(tab.name));
    }
    else
    {
        strlcpy(tab.name, GetNewName().c_str(), sizeof(tab.name));
    }

    strlcpy(tab.ip, GetDesc() ? GetDesc()->GetHostName() : "", sizeof(tab.ip));

    tab.id = m_dwPlayerID;
    tab.voice = GetPoint(POINT_VOICE);
    tab.level = GetLevel();
    tab.level_step = GetPoint(POINT_LEVEL_STEP);
    tab.exp = GetExp();
    tab.gold = GetGold();
#ifdef ENABLE_GAYA_SYSTEM
    tab.gaya = GetGaya();
#endif
    tab.job = m_points.job;
    tab.part_base = m_pointsInstant.bBasePart;
    tab.skill_group = m_points.skill_group;
#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
    tab.envanter = Inven_Point();
#endif
    uint32_t dwPlayedTime = (get_dword_time() - m_dwPlayStartTime);

    if (dwPlayedTime > 60000)
    {
        if (GetSectree() && !GetSectree()->IsAttr(GetX(), GetY(), ATTR_BANPK))
        {
            UpdateAlignment(5 * (dwPlayedTime / 60000));
        }

        SetRealPoint(POINT_PLAYTIME, GetRealPoint(POINT_PLAYTIME) + dwPlayedTime / 60000);
        ResetPlayTime(dwPlayedTime % 60000);
    }

    tab.playtime = GetRealPoint(POINT_PLAYTIME);
    tab.lAlignment = m_iRealAlignment;

    if (m_posWarp.x != 0 || m_posWarp.y != 0)
    {
        tab.x = m_posWarp.x;
        tab.y = m_posWarp.y;
        tab.z = 0;
        tab.lMapIndex = m_lWarpMapIndex;
    }
    else
    {
        tab.x = GetX();
        tab.y = GetY();
        tab.z = GetZ();
        tab.lMapIndex = GetMapIndex();
    }

    if (m_lExitMapIndex == 0)
    {
        tab.lExitMapIndex = tab.lMapIndex;
        tab.lExitX = tab.x;
        tab.lExitY = tab.y;
    }
    else
    {
        tab.lExitMapIndex = m_lExitMapIndex;
        tab.lExitX = m_posExit.x;
        tab.lExitY = m_posExit.y;
    }

    sys_log(0, "SAVE: %s %dx%d", GetName(), tab.x, tab.y);

    tab.st = GetRealPoint(POINT_ST);
    tab.ht = GetRealPoint(POINT_HT);
    tab.dx = GetRealPoint(POINT_DX);
    tab.iq = GetRealPoint(POINT_IQ);

    tab.stat_point = GetPoint(POINT_STAT);
    tab.skill_point = GetPoint(POINT_SKILL);
    tab.sub_skill_point = GetPoint(POINT_SUB_SKILL);
    tab.horse_skill_point = GetPoint(POINT_HORSE_SKILL);

    tab.stat_reset_count = GetPoint(POINT_STAT_RESET_COUNT);

    tab.hp = GetHP();
    tab.sp = GetSP();

    tab.stamina = GetStamina();

    tab.sRandomHP = m_points.iRandomHP;
    tab.sRandomSP = m_points.iRandomSP;

    for (int i = 0; i < QUICKSLOT_MAX_NUM; ++i)
        tab.quickslot[i] = m_quickslot[i];

    if (!m_stMobile.empty() && !*m_szMobileAuth)
        strlcpy(tab.szMobile, m_stMobile.c_str(), sizeof(tab.szMobile));

    memcpy(tab.parts, m_pointsInstant.parts, sizeof(tab.parts));
    memcpy(tab.skills, m_pSkillLevels, sizeof(TPlayerSkill) * SKILL_MAX_NUM);

#ifdef ENABLE_BATTLE_PASS
    tab.dwBattlePassEndTime = m_dwBattlePassEndTime;
#endif
#ifdef ENABLE_RANKING
    for (int i = 0; i < RANKING_MAX_CATEGORIES; ++i) {
        tab.lRankPoints[i] = m_lRankPoints[i];
    }
#endif
    tab.horse = GetHorseData();
}

void CHARACTER::SaveReal()
{
    if (m_bSkipSave)
        return;

    if (!GetDesc())
    {
        sys_err("Character::Save : no descriptor when saving (name: %s)", GetName());
        return;
    }

    TPlayerTable table;
    CreatePlayerProto(table);

    db_clientdesc->DBPacket(HEADER_GD_PLAYER_SAVE, GetDesc()->GetHandle(), &table, sizeof(TPlayerTable));

    quest::PC* pkQuestPC = quest::CQuestManager::instance().GetPCForce(GetPlayerID());

    if (!pkQuestPC)
        sys_err("CHARACTER::Save : null quest::PC pointer! (name %s)", GetName());
    else
    {
        pkQuestPC->Save();
    }

    marriage::TMarriage* pMarriage = marriage::CManager::instance().Get(GetPlayerID());
    if (pMarriage)
        pMarriage->Save();
}

void CHARACTER::FlushDelayedSaveItem()
{
    LPITEM item;

    for (int i = 0; i < INVENTORY_AND_EQUIP_SLOT_MAX; ++i)
        if ((item = GetInventoryItem(i)))
            ITEM_MANAGER::instance().FlushDelayedSave(item);
}

void CHARACTER::StartSaveEvent()
{
    if (m_pkSaveEvent)
        return;

    char_event_info* info = AllocEventInfo<char_event_info>();

    info->ch = this;
    m_pkSaveEvent = event_create(save_event, info, save_event_second_cycle);
}

void CHARACTER::SetWarpLocation(int32_t lMapIndex, int32_t x, int32_t y)
{
    m_posWarp.x = x * 100;
    m_posWarp.y = y * 100;
    m_lWarpMapIndex = lMapIndex;
}

void CHARACTER::SaveExitLocation()
{
    m_posExit = GetXYZ();
    m_lExitMapIndex = GetMapIndex();
}

void CHARACTER::ExitToSavedLocation()
{
    sys_log(0, "ExitToSavedLocation");
    WarpSet(m_posWarp.x, m_posWarp.y, m_lWarpMapIndex);

    m_posExit.x = m_posExit.y = m_posExit.z = 0;
    m_lExitMapIndex = 0;
}

bool CHARACTER::CanWarp() const
{
    const int iPulse = thecore_pulse();
    const int limit_time = PASSES_PER_SEC(g_nPortalLimitTime);

    if ((iPulse - GetSafeboxLoadTime()) < limit_time)
        return false;

    if ((iPulse - GetExchangeTime()) < limit_time)
        return false;

    if ((iPulse - GetMyShopTime()) < limit_time)
        return false;

    if ((iPulse - GetRefineTime()) < limit_time)
        return false;

    if (GetExchange() || GetMyShop() || GetShopOwner() || IsOpenSafebox() || IsCubeOpen()
#ifdef ENABLE_ACCE_SYSTEM
        || IsAcceOpen()
#endif
#ifdef __ATTR_TRANSFER_SYSTEM__
        || IsAttrTransferOpen()
#endif
#if defined(ENABLE_CHRISTMAS_WHEEL_OF_DESTINY)
        || GetWheelDestiny()
#endif
        )
        return false;

#ifdef __ENABLE_NEW_OFFLINESHOP__
    if (GetOfflineShopGuest() || GetAuctionGuest())
        return false;

    if (iPulse - GetOfflineShopUseTime() < limit_time)
        return false;
#endif

    return true;
}

bool CHARACTER::WarpSet(int32_t x, int32_t y, int32_t lPrivateMapIndex)
{
    if (!IsPC())
        return false;

    uint32_t lAddr;
    int32_t lMapIndex;
    uint16_t wPort;

#ifdef ENABLE_GENERAL_CH
    uint8_t ch = GetDesc() ? GetDesc()->GetAccountTable().bChannel : 0;
    if (!CMapLocation::instance().Get(ch, x, y, lMapIndex, lAddr, wPort)) {
        sys_err("cannot find map location index %d x %d y %d name %s", lMapIndex, x, y, GetName());
        return false;
    }

    if (lPrivateMapIndex >= 10000) {
        if (lPrivateMapIndex / 10000 != lMapIndex) {
            sys_err("Invalid map index %d, must be child of %d", lPrivateMapIndex, lMapIndex);
            return false;
        }

        lMapIndex = lPrivateMapIndex;
    }
#else
    if (!CMapLocation::instance().Get(x, y, lMapIndex, lAddr, wPort))
    {
        sys_err("cannot find map location index %d x %d y %d name %s", lMapIndex, x, y, GetName());
        return false;
    }

    if (lPrivateMapIndex >= 10000)
    {
        if (lPrivateMapIndex / 10000 != lMapIndex)
        {
            sys_err("Invalid map index %d, must be child of %d", lPrivateMapIndex, lMapIndex);
            return false;
        }

        lMapIndex = lPrivateMapIndex;
    }
#endif

    Stop();
    Save();

    if (GetSectree())
    {
        GetSectree()->RemoveEntity(this);
        const entt::entity e = CVIDRegistry::Instance().Find(GetVID());
        if (e != entt::null && g_registry.valid(e))
        {
            g_registry.remove<ecs::SectorPlacement>(e);
            g_registry.remove<ecs::ViewActiveTag>(e);
        }
        ViewCleanup();

        EncodeRemovePacket(this);
    }

    m_lWarpMapIndex = lMapIndex;
    m_posWarp.x = x;
    m_posWarp.y = y;

    sys_log(0, "WarpSet %s %d %d current map %d target map %d", GetName(), x, y, GetMapIndex(), lMapIndex);

    TPacketGCWarp p;

    p.bHeader = HEADER_GC_WARP;
    p.lX = x;
    p.lY = y;
    p.lAddr = lAddr;
    p.wPort = wPort;

#ifdef ENABLE_SWITCHBOT
    CSwitchbotManager::Instance().SetIsWarping(GetPlayerID(), true);

    if (p.wPort != mother_port)
    {
        CSwitchbotManager::Instance().P2PSendSwitchbot(GetPlayerID(), p.wPort);
    }
#endif

    GetDesc()->Packet(&p, sizeof(TPacketGCWarp));

    char buf[256];
    snprintf(buf, sizeof(buf), "%s MapIdx %ld DestMapIdx%ld DestX%ld DestY%ld Empire%d", GetName(), GetMapIndex(), lPrivateMapIndex, x, y, GetEmpire());
    LogManager::instance().CharLog(this, 0, "WARP", buf);

    return true;
}

void CHARACTER::WarpEnd()
{
    if (test_server)
        sys_log(0, "WarpEnd %s", GetName());

    if (m_posWarp.x == 0 && m_posWarp.y == 0)
        return;

    int32_t index = m_lWarpMapIndex;

    if (index > 10000)
        index /= 10000;

    if (!map_allow_find(index))
    {
        sys_err("location %d %d not allowed to login this server", m_posWarp.x, m_posWarp.y);
#ifdef ENABLE_GOHOME_IF_MAP_NOT_ALLOWED
        GoHome();
#else
        GetDesc()->SetPhase(PHASE_CLOSE);
#endif
        return;
    }

    sys_log(0, "WarpEnd %s %d %u %u", GetName(), m_lWarpMapIndex, m_posWarp.x, m_posWarp.y);

    Show(m_lWarpMapIndex, m_posWarp.x, m_posWarp.y, 0);
    Stop();

    m_lWarpMapIndex = 0;
    m_posWarp.x = m_posWarp.y = m_posWarp.z = 0;

    {
        TPacketGGLogin p;

        p.bHeader = HEADER_GG_LOGIN;
        strlcpy(p.szName, GetName(), sizeof(p.szName));
        p.dwPID = GetPlayerID();
        p.bEmpire = GetEmpire();
        p.lMapIndex = SECTREE_MANAGER::instance().GetMapIndex(GetX(), GetY());
        p.bChannel = g_bChannel;

        P2P_MANAGER::instance().Send(&p, sizeof(TPacketGGLogin));
    }
}

namespace {
    class FuncCheckWarp
    {
    public:
        FuncCheckWarp(LPCHARACTER pkWarp)
        {
            m_lTargetY = 0;
            m_lTargetX = 0;

            m_lX = pkWarp->GetX();
            m_lY = pkWarp->GetY();

            m_bInvalid = false;
            m_bEmpire = pkWarp->GetEmpire();

            char szTmp[64];

            if (3 != sscanf(pkWarp->GetName(), " %s %ld %ld ", szTmp, &m_lTargetX, &m_lTargetY))
            {
                if (number(1, 100) < 5)
                    sys_err("Warp NPC name wrong : vnum(%d) name(%s)", pkWarp->GetRaceNum(), pkWarp->GetName());

                m_bInvalid = true;

                return;
            }

            m_lTargetX *= 100;
            m_lTargetY *= 100;

            m_bUseWarp = true;

            if (pkWarp->IsGoto())
            {
                LPSECTREE_MAP pkSectreeMap = SECTREE_MANAGER::instance().GetMap(pkWarp->GetMapIndex());
                m_lTargetX += pkSectreeMap->m_setting.iBaseX;
                m_lTargetY += pkSectreeMap->m_setting.iBaseY;
                m_bUseWarp = false;
            }
        }

        bool Valid()
        {
            return !m_bInvalid;
        }

        void operator()(LPENTITY ent)
        {
            if (!Valid())
                return;

            if (!ent->IsType(ENTITY_CHARACTER))
                return;

            LPCHARACTER pkChr = (LPCHARACTER)ent;

            if (!pkChr->IsPC())
                return;

            int iDist = DISTANCE_APPROX(pkChr->GetX() - m_lX, pkChr->GetY() - m_lY);

            if (iDist > 300)
                return;

            if (m_bEmpire && pkChr->GetEmpire() && m_bEmpire != pkChr->GetEmpire())
                return;

            if (pkChr->IsHack())
                return;

            if (!pkChr->CanHandleItem(false, true))
                return;

            if (m_bUseWarp)
                pkChr->WarpSet(m_lTargetX, m_lTargetY);
            else
            {
                pkChr->Show(pkChr->GetMapIndex(), m_lTargetX, m_lTargetY);
                pkChr->Stop();
            }
        }

        bool m_bInvalid;
        bool m_bUseWarp;
        int32_t m_lX;
        int32_t m_lY;
        int32_t m_lTargetX;
        int32_t m_lTargetY;
        uint8_t m_bEmpire;
    };
}

EVENTFUNC(warp_npc_event)
{
    char_event_info* info = dynamic_cast<char_event_info*>(event->info);
    if (info == nullptr)
    {
        sys_err("warp_npc_event> <Factor> Null pointer");
        return 0;
    }

    LPCHARACTER ch = info->ch;

    if (ch == nullptr) {
        return 0;
    }

    // Phase 10: WRITES_STATE - deferred until ECS component covers m_pkWarpNPCEvent

    const entt::entity e = CVIDRegistry::Instance().Find(ch->GetVID());
    if (e != entt::null)
    {
        const PIXEL_POSITION& warpPos = ch->GetWarpPosition();
        g_dispatcher.trigger(ecs::EvWarpBegin {
            e,
            static_cast<uint32_t>(ch->GetMapIndex()),
            warpPos.x,
            warpPos.y
        });
    }

    if (!ch->GetSectree())
    {
        ch->m_pkWarpNPCEvent = nullptr;
        return 0;
    }

    FuncCheckWarp f(ch);
    if (f.Valid())
        ch->GetSectree()->ForEachAround(f);

    return passes_per_sec / 2;
}

void CHARACTER::StartWarpNPCEvent()
{
    if (m_pkWarpNPCEvent)
        return;

    if (!IsWarp() && !IsGoto())
        return;

    char_event_info* info = AllocEventInfo<char_event_info>();

    info->ch = this;

    m_pkWarpNPCEvent = event_create(warp_npc_event, info, passes_per_sec / 2);
}

bool CHARACTER::WarpToPID(uint32_t dwPID)
{
    LPCHARACTER victim;
    if ((victim = (CHARACTER_MANAGER::instance().FindByPID(dwPID))))
    {
        int mapIdx = victim->GetMapIndex();
        if (IS_SUMMONABLE_ZONE(mapIdx))
        {
            if (CAN_ENTER_ZONE(this, mapIdx))
            {
                WarpSet(victim->GetX(), victim->GetY());
            }
            else
            {
#ifdef TEXTS_IMPROVEMENT
                ChatPacketNew(CHAT_TYPE_INFO, 372, "");
#endif
                return false;
            }
        }
        else
        {
#ifdef TEXTS_IMPROVEMENT
            ChatPacketNew(CHAT_TYPE_INFO, 372, "");
#endif
            return false;
        }
    }
    else
    {
        CCI* pcci = P2P_MANAGER::instance().FindByPID(dwPID);

        if (!pcci)
        {
#ifdef TEXTS_IMPROVEMENT
            ChatPacketNew(CHAT_TYPE_INFO, 371, "");
#endif
            return false;
        }

        if (pcci->bChannel != g_bChannel)
        {
#ifdef TEXTS_IMPROVEMENT
            ChatPacketNew(CHAT_TYPE_INFO, 367, "%d#%d", g_bChannel, pcci->bChannel);
#endif
            return false;
        }
        else if (false == IS_SUMMONABLE_ZONE(pcci->lMapIndex))
        {
#ifdef TEXTS_IMPROVEMENT
            ChatPacketNew(CHAT_TYPE_INFO, 372, "");
#endif
            return false;
        }
        else
        {
            if (!CAN_ENTER_ZONE(this, pcci->lMapIndex))
            {
#ifdef TEXTS_IMPROVEMENT
                ChatPacketNew(CHAT_TYPE_INFO, 372, "");
#endif
                return false;
            }

            TPacketGGFindPosition p;
            p.header = HEADER_GG_FIND_POSITION;
            p.dwFromPID = GetPlayerID();
            p.dwTargetPID = dwPID;
            pcci->pkDesc->Packet(&p, sizeof(TPacketGGFindPosition));

            if (test_server)
                ChatPacket(CHAT_TYPE_PARTY, "sent find position packet for teleport");
        }
    }
    return true;
}

bool CHARACTER::Show(int32_t lMapIndex, int32_t x, int32_t y, int32_t z, bool bShowSpawnMotion/* = false */)
{
    if (IsPC())
    {
        const int32_t normalizedTargetMapIndex = NormalizeMapIndex(lMapIndex);

        if (normalizedTargetMapIndex == 1 && CheckAndHandleSameHwid(this))
        {
            const uint32_t startMapIndex = EMPIRE_START_MAP(GetEmpire());
            const uint32_t startX = EMPIRE_START_X(GetEmpire());
            const uint32_t startY = EMPIRE_START_Y(GetEmpire());

            if (startMapIndex && startX && startY)
            {
                sys_log(0, "HWID MAP1 restriction: %s moved to start map (%u, %u, %u)", GetName(), startMapIndex, startX, startY);
                lMapIndex = static_cast<int32_t>(startMapIndex);
                x = static_cast<int32_t>(startX);
                y = static_cast<int32_t>(startY);
            }
        }
    }

    LPSECTREE sectree = ecs::SectorAt(lMapIndex, x, y);

    if (!sectree)
    {
        sys_log(0, "cannot find sectree by %dx%d mapindex %d", x, y, lMapIndex);
        return false;
    }
#ifdef ENABLE_BATTLE_PASS_STAY_ONLINE
    if (!m_pkBattlePassStayOnlineEvent)
    {
        char_event_info* info = AllocEventInfo<char_event_info>();
        info->ch = this;
        m_pkBattlePassStayOnlineEvent = event_create(battle_pass_stay_online_event_session, info, PASSES_PER_SEC(60));
    }
#endif

    SetMapIndex(lMapIndex);

    bool bChangeTree = false;

    if (!GetSectree() || GetSectree() != sectree)
        bChangeTree = true;

    if (bChangeTree)
    {
        if (GetSectree())
        {
            GetSectree()->RemoveEntity(this);
            const entt::entity oldEntity = CVIDRegistry::Instance().Find(GetVID());
            if (oldEntity != entt::null && g_registry.valid(oldEntity))
            {
                g_registry.remove<ecs::SectorPlacement>(oldEntity);
                g_registry.remove<ecs::ViewActiveTag>(oldEntity);
            }
        }

        ViewCleanup();
    }
#ifdef LEADERBOARD_RAZOR93
    if (GetMapIndex() == 41)
    {
        SendLeaderboardData();
        SendLeaderboardDataSkillMob(this);
        SendLeaderboardDataGuild();
    }
#endif
    if (!IsNPC())
    {
        sys_log(0, "SHOW: %s %dx%dx%d", GetName(), x, y, z);
        if (GetStamina() < GetMaxStamina())
            StartAffectEvent();
    }
    else if (m_pkMobData)
    {
        m_pkMobInst->m_posLastAttacked.x = x;
        m_pkMobInst->m_posLastAttacked.y = y;
        m_pkMobInst->m_posLastAttacked.z = z;
    }

    if (bShowSpawnMotion)
    {
        SET_BIT(m_bAddChrState, ADD_CHARACTER_STATE_SPAWN);
        m_afAffectFlag.Set(AFF_SPAWN);
    }

    SetXYZ(x, y, z);

    m_posDest.x = x;
    m_posDest.y = y;
    m_posDest.z = z;

    m_posStart.x = x;
    m_posStart.y = y;
    m_posStart.z = z;

    if (bChangeTree)
    {
        EncodeInsertPacket(this);
        sectree->InsertEntity(this);

        const entt::entity e = CVIDRegistry::Instance().Find(GetVID());
        ecs::SyncSectorPlacement(g_registry, e, GetMapIndex(), GetX(), GetY());
        if (e != entt::null && g_registry.valid(e))
            g_registry.emplace_or_replace<ecs::ViewActiveTag>(e);

        UpdateSectree();
    }
    else
    {
        const entt::entity e = CVIDRegistry::Instance().Find(GetVID());
        if (e != entt::null && g_registry.valid(e))
            g_registry.emplace_or_replace<ecs::ViewActiveTag>(e);
        ViewReencode();
        sys_log(0, "      in same sectree");
    }

    REMOVE_BIT(m_bAddChrState, ADD_CHARACTER_STATE_SPAWN);

    SetValidComboInterval(0);
    ComputePoints();
#ifdef ENABLE_FAKE_SHOP_HEADER
    if (IsPC())
    {
        for (const auto& it : m_map_view)
        {
            LPENTITY ent = it.first;
            if (!ent || !ent->IsType(ENTITY_CHARACTER))
                continue;

            LPCHARACTER viewer = (LPCHARACTER)ent;
            if (viewer == this)
                continue;

            if (viewer->IsPC() && viewer->GetDesc())
                UpdateMountInventoryCountOverhead(viewer);
        }
    }
#endif

    return true;
}

void CHARACTER::Disconnect(const char* c_pszReason)
{
    assert(GetDesc() != NULL);

    sys_log(0, "DISCONNECT: %s (%s)", GetName(), c_pszReason ? c_pszReason : "unset");
#ifdef ENABLE_CPP_DUNGEON_RAZOR93
    COrcsDungeon::instance().OnPlayerDisconnect(this);
    CTritonTempleDungeon::instance().OnPlayerDisconnect(this);
    CValentineDungeon::instance().OnPlayerDisconnect(this);
    CRuneDungeon::instance().OnPlayerDisconnect(this);
    CPyramidDungeonRazor93::instance().OnPlayerDisconnect(this);
    CNightmareDungeonRazor93::instance().OnPlayerDisconnect(this);
    CHalloween2022Dungeon::instance().OnPlayerDisconnect(this);
    CVikingDungeon::instance().OnPlayerDisconnect(this);
    CEasterDungeon::instance().OnPlayerDisconnect(this);
#endif
    if (GetShop())
    {
        GetShop()->RemoveGuest(this);
        SetShop(nullptr);
    }

    if (GetArena() != nullptr)
    {
        GetArena()->OnDisconnect(GetPlayerID());
    }

    if (GetParty() != nullptr)
    {
        GetParty()->UpdateOfflineState(GetPlayerID());
    }

    marriage::CManager::instance().Logout(this);

    TPacketGGLogout p;
    p.bHeader = HEADER_GG_LOGOUT;
    strlcpy(p.szName, GetName(), sizeof(p.szName));
    P2P_MANAGER::instance().Send(&p, sizeof(TPacketGGLogout));
    LogManager::instance().CharLog(this, 0, "LOGOUT", "");

#ifdef ENABLE_PCBANG_FEATURE
    {
        int32_t playTime = GetRealPoint(POINT_PLAYTIME) - m_dwLoginPlayTime;
        LogManager::instance().LoginLog(false, GetDesc()->GetAccountTable().id, GetPlayerID(), GetLevel(), GetJob(), playTime);

        if (0)
            CPCBangManager::instance().Log(GetDesc()->GetHostName(), GetPlayerID(), playTime);
    }
#endif

    if (m_pWarMap)
        SetWarMap(nullptr);

    if (m_pWeddingMap)
        SetWeddingMap(nullptr);

#ifdef __ENABLE_NEW_OFFLINESHOP__
    offlineshop::GetManager().RemoveSafeboxFromCache(GetPlayerID());
    offlineshop::GetManager().RemoveGuestFromShops(this);

    if (m_pkAuctionGuest)
        m_pkAuctionGuest->RemoveGuest(this);

    if (GetOfflineShop())
        SetOfflineShop(nullptr);

    SetShopSafebox(nullptr);

    m_pkAuction = nullptr;
    m_pkAuctionGuest = nullptr;
    m_bIsLookingOfflineshopOfferList = false;
#endif

    if (GetGuild())
        GetGuild()->LogoutMember(this);

    quest::CQuestManager::instance().LogoutPC(this);

#ifdef ENABLE_PVP_ADVANCED
    DestroyPvP();
#endif

    if (GetParty())
        GetParty()->Unlink(this);

    if (IsStun() || IsDead())
    {
        DeathPenalty(0);
        PointChange(POINT_HP, 50 - GetHP());
    }

    ITEM_MANAGER::instance().FlushDelayedSaveByOwner(this);

    if (!CHARACTER_MANAGER::instance().FlushDelayedSave(this))
        SaveReal();

    FlushDelayedSaveItem();

    SaveAffect();
    m_bIsLoadedAffect = false;

#ifdef ENABLE_BATTLE_PASS
    auto it = m_listBattlePass.begin();
    while (it != m_listBattlePass.end())
    {
        TPlayerBattlePassMission* pkMission = *it++;

        if (pkMission->bIsUpdated)
            db_clientdesc->DBPacket(HEADER_GD_SAVE_BATTLE_PASS, 0, pkMission, sizeof(TPlayerBattlePassMission));

        if (pkMission)
            M2_DELETE(pkMission);
    }
    m_bIsLoadedBattlePass = false;
#endif

    m_bSkipSave = true;

    quest::CQuestManager::instance().DisconnectPC(this);

    CloseSafebox();
    CloseMall();

    CPVPManager::instance().Disconnect(this);
    CTargetManager::instance().Logout(GetPlayerID());
    MessengerManager::instance().Logout(GetName());

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
    if (GetMountVnum())
    {
        RemoveAffect(AFFECT_MOUNT);
        RemoveAffect(AFFECT_MOUNT_BONUS);
    }
#endif

    if (GetDesc())
        GetDesc()->BindCharacter(nullptr);

    M2_DESTROY_CHARACTER(this);
}

float CHARACTER::GetDistanceFromSafeboxOpen() const
{
    return DISTANCE_APPROX(GetX() - m_posSafeboxOpen.x, GetY() - m_posSafeboxOpen.y);
}

void CHARACTER::SetSafeboxOpenPosition()
{
    m_posSafeboxOpen = GetXYZ();
}

CSafebox* CHARACTER::GetSafebox() const
{
    return m_pkSafebox;
}

void CHARACTER::ReqSafeboxLoad(const char* pszPassword)
{
    if (!*pszPassword || strlen(pszPassword) > SAFEBOX_PASSWORD_MAX_LEN)
    {
#ifdef TEXTS_IMPROVEMENT
        ChatPacketNew(CHAT_TYPE_INFO, 188, "");
#endif
        return;
    }
    else if (m_pkSafebox)
    {
#ifdef TEXTS_IMPROVEMENT
        ChatPacketNew(CHAT_TYPE_INFO, 189, "");
#endif
        return;
    }

    int iPulse = thecore_pulse();

    if (iPulse - GetSafeboxLoadTime() < PASSES_PER_SEC(10))
    {
#ifdef TEXTS_IMPROVEMENT
        ChatPacketNew(CHAT_TYPE_INFO, 190, "");
#endif
        return;
    }
#ifndef __OPEN_SAFEBOX_CLICK__
    else if (GetDistanceFromSafeboxOpen() > 1000)
    {
#ifdef TEXTS_IMPROVEMENT
        ChatPacketNew(CHAT_TYPE_INFO, 185, "");
#endif
        return;
    }
#endif
    else if (m_bOpeningSafebox)
    {
        sys_log(0, "Overlapped safebox load request from %s", GetName());
        return;
    }

    SetSafeboxLoadTime();
    m_bOpeningSafebox = true;

    TSafeboxLoadPacket p;
    p.dwID = GetDesc()->GetAccountTable().id;
    strlcpy(p.szLogin, GetDesc()->GetAccountTable().login, sizeof(p.szLogin));
    strlcpy(p.szPassword, pszPassword, sizeof(p.szPassword));

    db_clientdesc->DBPacket(HEADER_GD_SAFEBOX_LOAD, GetDesc()->GetHandle(), &p, sizeof(p));
}

void CHARACTER::LoadSafebox(int iSize, uint32_t dwGold, int iItemCount, TPlayerItem* pItems)
{
    bool bLoaded = false;

    SetOpenSafebox(true);

    if (m_pkSafebox)
        bLoaded = true;

    if (!m_pkSafebox)
        m_pkSafebox = M2_NEW CSafebox(this, iSize, dwGold);
    else
        m_pkSafebox->ChangeSize(iSize);

    m_iSafeboxSize = iSize;

    TPacketCGSafeboxSize p;
    p.bHeader = HEADER_GC_SAFEBOX_SIZE;
    p.bSize = iSize;

    GetDesc()->Packet(&p, sizeof(TPacketCGSafeboxSize));

    if (!bLoaded)
    {
        for (int i = 0; i < iItemCount; ++i, ++pItems)
        {
            if (!m_pkSafebox->IsValidPosition(pItems->pos))
                continue;

            LPITEM item = ITEM_MANAGER::instance().CreateItem(pItems->vnum, pItems->count, pItems->id);

            if (!item)
            {
                sys_err("cannot create item vnum %d id %u (name: %s)", pItems->vnum, pItems->id, GetName());
                continue;
            }

            item->SetSkipSave(true);
            item->SetSockets(pItems->alSockets);
            item->SetAttributes(pItems->aAttr);

            if (!m_pkSafebox->Add(pItems->pos, item))
                M2_DESTROY_ITEM(item);
            else
                item->SetSkipSave(false);
        }
    }
}

void CHARACTER::ChangeSafeboxSize(uint8_t bSize)
{
    TPacketCGSafeboxSize p;
    p.bHeader = HEADER_GC_SAFEBOX_SIZE;
    p.bSize = bSize;

    GetDesc()->Packet(&p, sizeof(TPacketCGSafeboxSize));

    if (m_pkSafebox)
        m_pkSafebox->ChangeSize(bSize);

    m_iSafeboxSize = bSize;
}

void CHARACTER::CloseSafebox()
{
    if (!m_pkSafebox)
        return;

    if (!IsPC() || !GetDesc())
    {
        sys_err("CloseSafebox skipped: invalid owner (name=%s vid=%u race=%u ispc=%d desc=%p)",
            GetName(),
            static_cast<uint32_t>(GetVID()),
            GetRaceNum(),
            IsPC(),
            GetDesc());

        M2_DELETE(m_pkSafebox);
        m_pkSafebox = nullptr;
        m_bOpeningSafebox = false;
        return;
    }

    SetOpenSafebox(false);
    m_pkSafebox->Save();

    M2_DELETE(m_pkSafebox);
    m_pkSafebox = nullptr;

    ChatPacket(CHAT_TYPE_COMMAND, "CloseSafebox");

    SetSafeboxLoadTime();
    m_bOpeningSafebox = false;

    Save();
}

CSafebox* CHARACTER::GetMall() const
{
    return m_pkMall;
}

void CHARACTER::LoadMall(int iItemCount, TPlayerItem* pItems)
{
    bool bLoaded = false;

    if (m_pkMall)
        bLoaded = true;

    if (!m_pkMall)
        m_pkMall = M2_NEW CSafebox(this, 3 * SAFEBOX_PAGE_SIZE, 0);
    else
        m_pkMall->ChangeSize(3 * SAFEBOX_PAGE_SIZE);

    m_pkMall->SetWindowMode(MALL);

    TPacketCGSafeboxSize p;
    p.bHeader = HEADER_GC_MALL_OPEN;
    p.bSize = 3 * SAFEBOX_PAGE_SIZE;

    GetDesc()->Packet(&p, sizeof(TPacketCGSafeboxSize));

    if (!bLoaded)
    {
        for (int i = 0; i < iItemCount; ++i, ++pItems)
        {
            if (!m_pkMall->IsValidPosition(pItems->pos))
                continue;

            LPITEM item = ITEM_MANAGER::instance().CreateItem(pItems->vnum, pItems->count, pItems->id);

            if (!item)
            {
                sys_err("cannot create item vnum %d id %u (name: %s)", pItems->vnum, pItems->id, GetName());
                continue;
            }

            item->SetSkipSave(true);
            item->SetSockets(pItems->alSockets);
            item->SetAttributes(pItems->aAttr);

            if (!m_pkMall->Add(pItems->pos, item))
                M2_DESTROY_ITEM(item);
            else
                item->SetSkipSave(false);
        }
    }
}

void CHARACTER::CloseMall()
{
    if (!m_pkMall)
        return;

    m_pkMall->Save();

    M2_DELETE(m_pkMall);
    m_pkMall = nullptr;

    ChatPacket(CHAT_TYPE_COMMAND, "CloseMall");
}

void CHARACTER::QuerySafeboxSize()
{
    if (m_iSafeboxSize == -1)
    {
        DBManager::instance().ReturnQuery(QID_SAFEBOX_SIZE,
            GetPlayerID(),
            nullptr,
            "SELECT size FROM safebox%s WHERE account_id = %u",
            get_table_postfix(),
            GetDesc()->GetAccountTable().id);
    }
}

void CHARACTER::SetSafeboxSize(int iSize)
{
    sys_log(1, "SetSafeboxSize: %s %d", GetName(), iSize);
    m_iSafeboxSize = iSize;
    DBManager::instance().Query("UPDATE safebox%s SET size = %d WHERE account_id = %u", get_table_postfix(), iSize / SAFEBOX_PAGE_SIZE, GetDesc()->GetAccountTable().id);
}

int CHARACTER::GetSafeboxSize() const
{
    return m_iSafeboxSize;
}
