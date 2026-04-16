#include "../../stdafx.h"

#include "PlayerRuntimeSystem.hpp"

#include "../../char.h"
#include "../../config.h"
#include "../../desc.h"
#include "../../buffer_manager.h"
#include "../../mob_manager.h"
#ifdef ENABLE_ANTICHEAT
#include "../../hwidmanager.h"
#endif

extern bool RaceToJob(unsigned race, unsigned* ret_job);

#ifdef TEXTS_IMPROVEMENT
void CHARACTER::ChatPacketNew(uint8_t type, uint32_t idx, const char* format, ...)
{
    if (type != CHAT_TYPE_INFO &&
        type != CHAT_TYPE_NOTICE &&
        type != CHAT_TYPE_BIG_NOTICE
#ifdef ENABLE_DICE_SYSTEM
         && type != CHAT_TYPE_DICE_INFO
#endif
#ifdef ENABLE_NEW_CHAT
         && type != CHAT_TYPE_INFO_EXP
         && type != CHAT_TYPE_INFO_ITEM
         && type != CHAT_TYPE_INFO_VALUE
#endif
        && type != CHAT_TYPE_DIALOG)
        return;

    LPDESC d = GetDesc();
    if (!d)
        return;

    char chatbuf[256];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(chatbuf, sizeof(chatbuf), format, args);
    va_end(args);

    TPacketGCChatNew p;
    p.header = HEADER_GC_CHAT_NEW;
    p.type = type;
    p.idx = idx;
    p.size = sizeof(p) + len;

    TEMP_BUFFER buf;
    buf.write(&p, sizeof(p));
    if (len > 0)
        buf.write(chatbuf, len);

    d->Packet(buf.read_peek(), buf.size());
}
#endif

#ifdef __DUNGEON_INFO_SYSTEM__
void CHARACTER::SetQuestDamage(int race, int dmg)
{
    if (race != 693 &&
        race != 768 &&
        race != 1093 &&
        race != 2092 &&
        race != 2493 &&
        race != 2598 &&
        race != 3962 &&
        race != 4011 &&
        race != 4158 &&
        race != 6091 &&
        race != 6191 &&
        race != 6192 &&
        race != 6118 &&
        race != 6393)
        return;

    auto it = dungeonDamage.find(race);
    if (it == dungeonDamage.end())
        dungeonDamage.insert(dungeonDamage.begin(), std::pair(race, dmg));
    else if (dmg > it->second)
        it->second = dmg;
}

uint64_t CHARACTER::GetQuestDamage(int race)
{
    auto it = dungeonDamage.find(race);
    return it == dungeonDamage.end() ? 0 : it->second;
}
#endif

#ifdef ENABLE_ANTICHEAT
void CHARACTER::ProcessCheatCheck(int32_t time)
{
    if (GetGMLevel() == GM_PLAYER)
    {
        if (m_rewardCount == 0)
            m_firstReward = time;

        m_rewardCount++;

        if (m_rewardCount >= 7)
        {
            const int32_t n = time - m_firstReward;
            if (n <= 7)
            {
                CHwidManager::Instance().SendBlockHwid("ANTICHEAT", GetName());

                LPDESC desc = GetDesc();
                if (desc)
                    desc->DelayedDisconnect(5);
            }
            else
            {
                m_rewardCount = 0;
            }
        }
    }
}

void CHARACTER::ClearCheatChecks()
{
    m_firstReward = 0;
    m_rewardCount = 0;
    m_checkRepeated = 0;
}
#endif

bool CHARACTER::ChangeSex()
{
    int src_race = GetRaceNum();

    switch (src_race)
    {
    case MAIN_RACE_WARRIOR_M:
        m_points.job = MAIN_RACE_WARRIOR_W;
        break;

    case MAIN_RACE_WARRIOR_W:
        m_points.job = MAIN_RACE_WARRIOR_M;
        break;

    case MAIN_RACE_ASSASSIN_M:
        m_points.job = MAIN_RACE_ASSASSIN_W;
        break;

    case MAIN_RACE_ASSASSIN_W:
        m_points.job = MAIN_RACE_ASSASSIN_M;
        break;

    case MAIN_RACE_SURA_M:
        m_points.job = MAIN_RACE_SURA_W;
        break;

    case MAIN_RACE_SURA_W:
        m_points.job = MAIN_RACE_SURA_M;
        break;

    case MAIN_RACE_SHAMAN_M:
        m_points.job = MAIN_RACE_SHAMAN_W;
        break;

    case MAIN_RACE_SHAMAN_W:
        m_points.job = MAIN_RACE_SHAMAN_M;
        break;
#ifdef ENABLE_WOLFMAN_CHARACTER
    case MAIN_RACE_WOLFMAN_M:
        m_points.job = MAIN_RACE_WOLFMAN_M;
        break;
#endif
    default:
        sys_err("CHANGE_SEX: %s unknown race %d", GetName(), src_race);
        return false;
    }

    sys_log(0, "CHANGE_SEX: %s (%d -> %d)", GetName(), src_race, m_points.job);
    return true;
}

uint16_t CHARACTER::GetRaceNum() const
{
    if (m_dwPolymorphRace)
        return m_dwPolymorphRace;

    if (m_pkMobData)
        return m_pkMobData->m_table.dwVnum;

    return m_points.job;
}

void CHARACTER::SetRace(uint8_t race)
{
    if (race >= MAIN_RACE_MAX_NUM)
    {
        sys_err("CHARACTER::SetRace(name=%s, race=%d).OUT_OF_RACE_RANGE", GetName(), race);
        return;
    }

    m_points.job = race;
}

uint8_t CHARACTER::GetJob() const
{
    unsigned race = m_points.job;
    unsigned job;

    if (RaceToJob(race, &job))
        return job;

    sys_err("CHARACTER::GetJob(name=%s, race=%d).OUT_OF_RACE_RANGE", GetName(), race);
    return JOB_WARRIOR;
}

void CHARACTER::SetLevel(uint8_t level)
{
    m_points.level = level;

    if (IsPC())
    {
        if (level < PK_PROTECT_LEVEL)
            SetPKMode(PK_MODE_PROTECT);
        else if (GetGMLevel() != GM_PLAYER)
            SetPKMode(PK_MODE_PROTECT);
        else if (m_bPKMode == PK_MODE_PROTECT)
            SetPKMode(PK_MODE_PEACE);
    }
}

void CHARACTER::SetEmpire(uint8_t bEmpire)
{
    m_bEmpire = bEmpire;
}

uint8_t CHARACTER::GetCharType() const
{
    return m_bCharType;
}
