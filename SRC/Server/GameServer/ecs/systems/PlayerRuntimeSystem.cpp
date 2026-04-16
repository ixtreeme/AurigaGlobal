#include "../../stdafx.h"

#include "PlayerRuntimeSystem.hpp"

#include "../../char.h"
#include "../../char_manager.h"
#include "../../config.h"
#include "../../constants.h"
#include "../../desc.h"
#include "../../buffer_manager.h"
#include "../../db.h"
#include "../../dungeon.h"
#include "../../gm.h"
#include "../../mob_manager.h"
#include "../../questmanager.h"
#include "../../skill_power.h"
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

uint8_t CHARACTER::GetGMLevel() const
{
    if (test_server)
        return GM_IMPLEMENTOR;

    return m_pointsInstant.gm_level;
}

void CHARACTER::SetGMLevel()
{
    if (GetDesc())
        m_pointsInstant.gm_level = gm_get_level(GetName(), GetDesc()->GetHostName(), GetDesc()->GetAccountTable().login);
    else
        m_pointsInstant.gm_level = GM_PLAYER;
}

BOOL CHARACTER::IsGM() const
{
    if (m_pointsInstant.gm_level != GM_PLAYER)
        return true;

    return test_server ? true : false;
}

uint32_t CHARACTER::GetAID() const
{
    char szQuery[1024 + 1];
    uint32_t dwAID = 0;

    snprintf(szQuery, sizeof(szQuery), "SELECT id FROM player_index%s WHERE pid1=%u OR pid2=%u OR pid3=%u OR pid4=%u OR pid5=%u AND empire=%u",
        get_table_postfix(), GetPlayerID(), GetPlayerID(), GetPlayerID(), GetPlayerID(), GetPlayerID(), GetEmpire());

    std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery(szQuery));
    if (msg->Get()->uiNumRows == 0)
        return 0;

    MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult);
    str_to_number(dwAID, row[0]);
    return dwAID;
}

void CHARACTER::SetQuestNPCID(uint32_t vid)
{
    m_dwQuestNPCVID = vid;
}

LPCHARACTER CHARACTER::GetQuestNPC() const
{
    return CHARACTER_MANAGER::instance().Find(m_dwQuestNPCVID);
}

void CHARACTER::SetQuestItemPtr(LPITEM item)
{
    m_pQuestItem = item;
}

void CHARACTER::ClearQuestItemPtr()
{
    m_pQuestItem = nullptr;
}

LPITEM CHARACTER::GetQuestItemPtr() const
{
    return m_pQuestItem;
}

LPDUNGEON CHARACTER::GetDungeonForce() const
{
    if (m_lWarpMapIndex > 10000)
        return CDungeonManager::instance().FindByMapIndex(m_lWarpMapIndex);

    return m_pkDungeon;
}

void CHARACTER::SetBlockMode(uint8_t bFlag)
{
    m_pointsInstant.bBlockMode = bFlag;

    ChatPacket(CHAT_TYPE_COMMAND, "setblockmode %d", m_pointsInstant.bBlockMode);

    SetQuestFlag("game_option.block_exchange", bFlag & BLOCK_EXCHANGE ? 1 : 0);
    SetQuestFlag("game_option.block_party_invite", bFlag & BLOCK_PARTY_INVITE ? 1 : 0);
    SetQuestFlag("game_option.block_guild_invite", bFlag & BLOCK_GUILD_INVITE ? 1 : 0);
    SetQuestFlag("game_option.block_whisper", bFlag & BLOCK_WHISPER ? 1 : 0);
    SetQuestFlag("game_option.block_messenger_invite", bFlag & BLOCK_MESSENGER_INVITE ? 1 : 0);
    SetQuestFlag("game_option.block_party_request", bFlag & BLOCK_PARTY_REQUEST ? 1 : 0);
}

void CHARACTER::SetBlockModeForce(uint8_t bFlag)
{
    m_pointsInstant.bBlockMode = bFlag;
    ChatPacket(CHAT_TYPE_COMMAND, "setblockmode %d", m_pointsInstant.bBlockMode);
}

bool CHARACTER::IsGuardNPC() const
{
    return IsNPC() && (GetRaceNum() == 11000 || GetRaceNum() == 11002 || GetRaceNum() == 11004);
}

int CHARACTER::GetQuestFlag(const std::string& flag) const
{
    int ret = 0;
    quest::CQuestManager& q = quest::CQuestManager::instance();
    quest::PC* pPC = q.GetPC(GetPlayerID());
    if (pPC)
        ret = pPC->GetFlag(flag);

    return ret;
}

void CHARACTER::SetQuestFlag(const std::string& flag, int value)
{
    quest::CQuestManager& q = quest::CQuestManager::instance();
    quest::PC* pPC = q.GetPC(GetPlayerID());
    pPC->SetFlag(flag, value);
}

#ifdef ENABLE_VOTE4BUFF
long long CHARACTER::GetVoteCoin()
{
    std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery("SELECT coins FROM account.account WHERE id = '%d';", GetDesc()->GetAccountTable().id));
    if (pMsg->Get()->uiNumRows == 0)
        return 0;
    MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);
    long long coin = 0;
    str_to_number(coin, row[0]);
    return coin;
}

void CHARACTER::SetVoteCoin(long long amount)
{
    std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery("UPDATE account.account SET coins = '%lld' WHERE id = '%d';", amount, GetDesc()->GetAccountTable().id));
}
#endif

#ifdef ENABLE_ITEMSHOP
uint32_t CHARACTER::GetDragonCoin()
{
    std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery("SELECT coins FROM account.account WHERE id = '%u';", GetDesc()->GetAccountTable().id));
    if (pMsg->Get()->uiNumRows == 0)
        return 0;
    MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);
    uint32_t dc = 0;
    str_to_number(dc, row[0]);
    return dc;
}

void CHARACTER::SetDragonCoin(uint32_t amount)
{
    std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery("UPDATE account.account SET coins = '%lld' WHERE id = '%u';", amount, GetDesc()->GetAccountTable().id));
}

void CHARACTER::SetProtectTime(const std::string& flagname, int value)
{
    auto it = m_protection_Time.find(flagname);
    if (it != m_protection_Time.end())
        it->second = value;
    else
        m_protection_Time.insert(make_pair(flagname, value));
}

int CHARACTER::GetProtectTime(const std::string& flagname) const
{
    auto it = m_protection_Time.find(flagname);
    if (it != m_protection_Time.end())
        return it->second;
    return 0;
}
#endif

const TMobTable& CHARACTER::GetMobTable() const
{
    return m_pkMobData->m_table;
}

bool CHARACTER::IsRaceFlag(uint32_t dwBit) const
{
    return m_pkMobData ? IS_SET(m_pkMobData->m_table.dwRaceFlag, dwBit) : 0;
}

uint32_t CHARACTER::GetMobDamageMin() const
{
    return m_pkMobData->m_table.dwDamageRange[0];
}

uint32_t CHARACTER::GetMobDamageMax() const
{
    return m_pkMobData->m_table.dwDamageRange[1];
}

float CHARACTER::GetMobDamageMultiply() const
{
    float fDamMultiply = GetMobTable().fDamMultiply;

    if (IsBerserk())
        fDamMultiply = fDamMultiply * 2.0f;

    return fDamMultiply;
}

uint32_t CHARACTER::GetMobDropItemVnum() const
{
    if (!m_pkMobData)
    {
        sys_err("GetMobDropItemVnum: NULL mob data (vid=%u race=%u name=%s map=%ld x=%ld y=%ld)",
            static_cast<uint32_t>(GetVID()),
            GetRaceNum(),
            GetName(),
            GetMapIndex(),
            GetX(),
            GetY());
        return 0;
    }

    return m_pkMobData->m_table.dwDropItemVnum;
}

bool CHARACTER::IsSummonMonster() const
{
    return GetSummonVnum() != 0;
}

uint32_t CHARACTER::GetSummonVnum() const
{
    return m_pkMobData ? m_pkMobData->m_table.dwSummonVnum : 0;
}

uint32_t CHARACTER::GetPolymorphItemVnum() const
{
    return m_pkMobData ? m_pkMobData->m_table.dwPolymorphItemVnum : 0;
}

uint32_t CHARACTER::GetMonsterDrainSPPoint() const
{
    return m_pkMobData ? m_pkMobData->m_table.dwDrainSP : 0;
}

uint8_t CHARACTER::GetMobRank() const
{
    if (!m_pkMobData)
        return MOB_RANK_KNIGHT;

    return m_pkMobData->m_table.bRank;
}

uint8_t CHARACTER::GetMobSize() const
{
    if (!m_pkMobData)
        return MOBSIZE_MEDIUM;

    return m_pkMobData->m_table.bSize;
}

uint16_t CHARACTER::GetMobAttackRange() const
{
    if (!m_pkMobData)
    {
        sys_err("GetMobAttackRange: m_pkMobData NULL! (VID: %u, Name: %s, Race:%d)",
            static_cast<uint32_t>(GetVID()), GetName(), GetRaceNum());
        return 0;
    }

    switch (GetMobBattleType())
    {
    case BATTLE_TYPE_RANGE:
    case BATTLE_TYPE_MAGIC:
#ifdef __DEFENSE_WAVE__
        if (GetRaceNum() == 3960 || GetRaceNum() == 3961 || GetRaceNum() == 3962)
            return m_pkMobData->m_table.wAttackRange + GetPoint(POINT_BOW_DISTANCE) + 4000;
        else
            return m_pkMobData->m_table.wAttackRange;
#else
        return m_pkMobData->m_table.wAttackRange + GetPoint(POINT_BOW_DISTANCE);
#endif

    default:
#ifdef __DEFENSE_WAVE__
        if ((GetRaceNum() <= 3955 && GetRaceNum() >= 3950 && GetRaceNum() != 3953) ||
            (GetRaceNum() <= 3605 && GetRaceNum() >= 3601 && GetRaceNum() != 3602))
            return m_pkMobData->m_table.wAttackRange + 300;
        else
            return m_pkMobData->m_table.wAttackRange;
#else
        return m_pkMobData->m_table.wAttackRange;
#endif
    }
}

uint8_t CHARACTER::GetMobBattleType() const
{
    if (!m_pkMobData)
        return BATTLE_TYPE_MELEE;

    return m_pkMobData->m_table.bBattleType;
}

void CHARACTER::ResetPlayTime(uint32_t dwTimeRemain)
{
    m_dwPlayStartTime = get_dword_time() - dwTimeRemain;
}

int CHARACTER::GetPremiumRemainSeconds(uint8_t bType) const
{
    if (bType >= PREMIUM_MAX_NUM)
        return 0;

    return m_aiPremiumTimes[bType] - get_global_time();
}

void CHARACTER::UpdateDepositPulse()
{
    m_deposit_pulse = thecore_pulse() + PASSES_PER_SEC(60 * 5);
}

bool CHARACTER::CanDeposit() const
{
    return (m_deposit_pulse == 0 || (m_deposit_pulse < thecore_pulse()));
}

uint32_t CHARACTER::GetNextExp() const
{
    if (PLAYER_MAX_LEVEL_CONST < GetLevel())
        return 2500000000u;
    else
        return exp_table[GetLevel()];
}

#ifdef __NEWPET_SYSTEM__
uint32_t CHARACTER::PetGetNextExp() const
{
    if (IsNewPet()) {
        if (120 < GetLevel())
            return 2500000000;
        else
            return exppet_table[GetLevel()];
    }
    return 0;
}
#endif

int CHARACTER::GetSkillPowerByLevel(int level, bool bMob) const
{
    return CTableBySkill::instance().GetSkillPowerByLevelFromType(GetJob(), GetSkillGroup(), MINMAX(0, level, (int)SKILL_MAX_LEVEL), bMob);
}

#ifdef ENABLE_WHISPER_ADMIN_SYSTEM
std::string CHARACTER::GetLang() {
    auto language = GetDesc()->GetLanguage();
    std::string langs[] = { "en","en","ro","it","tr","de","pl","pt","es","cz","hu" };
    if (language == 0)
        return langs[language + 1];
    else
        return langs[language];
}
#endif
