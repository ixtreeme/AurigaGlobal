#include "../../stdafx.h"

#include "SessionSystem.hpp"

#include "../../char.h"
#include "../../char_manager.h"
#include "../../db.h"
#include "../../desc_client.h"
#include "../../item.h"
#include "../../item_manager.h"
#include "../../marriage.h"
#include "../../questmanager.h"
#include "../../sectree.h"
#include "../../utils.h"

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
