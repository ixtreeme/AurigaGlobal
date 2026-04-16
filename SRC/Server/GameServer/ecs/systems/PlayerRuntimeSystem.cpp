#include "../../stdafx.h"

#include "PlayerRuntimeSystem.hpp"

#include "../../char.h"
#include "../../char_manager.h"
#include "../../config.h"
#include "../../constants.h"
#include "../../desc.h"
#include "../../buffer_manager.h"
#include "../../battle_pass.h"
#include "../../db.h"
#include "../../desc_client.h"
#include "../../dungeon.h"
#include "../../gm.h"
#include "../../item.h"
#include "../../item_manager.h"
#include "../../marriage.h"
#include "../../mob_manager.h"
#include "../../questmanager.h"
#include "../../regen.h"
#include "../../skill_power.h"
#include "../../war_map.h"
#include "../../wedding.h"
#include "../../../common/rune_length.h"
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

#ifdef ENABLE_BATTLE_PASS
void CHARACTER::EnsureFreeBattlePassActive()
{
    const uint8_t kDefaultBattlePassId = 1;

    int remain = 0;
    if (m_dwBattlePassEndTime > 0)
        remain = (int)(m_dwBattlePassEndTime - get_global_time());

    if (remain <= 0)
    {
        remain = GetSecondsTillNextMonth();
        m_dwBattlePassEndTime = get_global_time() + remain;
    }

    if (!GetBattlePassId())
        AddAffect(AFFECT_BATTLE_PASS, POINT_BATTLE_PASS_ID, kDefaultBattlePassId, 0, remain, 0, true);
    m_bIsLoadedBattlePass = true;
}
#endif

#ifdef ENABLE_BATTLE_PASS
void CHARACTER::LoadBattlePass(uint32_t dwCount, TPlayerBattlePassMission* data)
{
    m_bIsLoadedBattlePass = false;

    for (auto it = m_listBattlePass.begin(); it != m_listBattlePass.end(); ++it)
        delete (*it);
    m_listBattlePass.clear();

    const uint8_t kDefaultBattlePassId = 1;

    int remain = 0;
    if (m_dwBattlePassEndTime > 0)
        remain = (int)(m_dwBattlePassEndTime - get_global_time());

    if (remain <= 0)
    {
        remain = GetSecondsTillNextMonth();
        m_dwBattlePassEndTime = get_global_time() + remain;
    }

    if (!GetBattlePassId())
        AddAffect(AFFECT_BATTLE_PASS, POINT_BATTLE_PASS_ID, kDefaultBattlePassId, 0, remain, 0, true);

    if (dwCount == 0 || !data)
    {
        m_bIsLoadedBattlePass = true;
        return;
    }

    for (size_t i = 0; i < dwCount; ++i, ++data)
    {
        TPlayerBattlePassMission* newMission = new TPlayerBattlePassMission;
        newMission->dwPlayerId = data->dwPlayerId;
        newMission->dwMissionId = data->dwMissionId;
        newMission->dwBattlePassId = data->dwBattlePassId;
        newMission->dwExtraInfo = data->dwExtraInfo;
        newMission->bCompleted = data->bCompleted;
        newMission->bIsUpdated = data->bIsUpdated;

        m_listBattlePass.push_back(newMission);
    }

    m_bIsLoadedBattlePass = true;
}

#ifdef ENABLE_BATTLE_PASS_STAY_ONLINE
void CHARACTER::CancelStayOnlineEvent()
{
    if (m_pkStayOnlineEvent)
    {
        event_cancel(&m_pkStayOnlineEvent);
        m_pkStayOnlineEvent = nullptr;
    }
}
#endif

#ifdef ENABLE_FREE_PASS_RAZOR93
bool CHARACTER::HasBattlePassBoost(uint8_t bBattlePassId)
{
    CAffect* p = FindAffect(AFFECT_BATTLE_PASS_BOOST, POINT_BATTLE_PASS_ID);
    return (p && p->lApplyValue == bBattlePassId);
}

uint32_t CHARACTER::GetBattlePassAdjustedTotal(uint32_t dwMissionID, uint32_t dwBattlePassID, uint32_t dwBaseTotal)
{
    if (dwBaseTotal <= 1)
        return dwBaseTotal;

    if (!HasBattlePassBoost((uint8_t)dwBattlePassID))
        return dwBaseTotal;

    return (dwBaseTotal + 1) / 2;
}

void CHARACTER::ApplyBattlePassBoostRecalc(uint8_t bBattlePassId)
{
    auto it = m_listBattlePass.begin();
    while (it != m_listBattlePass.end())
    {
        TPlayerBattlePassMission* m = *it++;
        if (!m || m->dwBattlePassId != bBattlePassId)
            continue;

        if (m->bCompleted)
            continue;

        uint32_t dwInfo1 = 0, dwBaseNeed = 0;
        if (!CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, (uint8_t)m->dwMissionId, &dwInfo1, &dwBaseNeed))
            continue;

        const uint32_t dwNeed = GetBattlePassAdjustedTotal(m->dwMissionId, bBattlePassId, dwBaseNeed);

        if (m->dwExtraInfo >= dwNeed)
            UpdateMissionProgress(m->dwMissionId, bBattlePassId, dwNeed, dwNeed, true);
    }
}
#endif

uint32_t CHARACTER::GetMissionProgress(uint32_t dwMissionID, uint32_t dwBattlePassID)
{
    auto it = m_listBattlePass.begin();
    while (it != m_listBattlePass.end())
    {
        TPlayerBattlePassMission* pkMission = *it++;
        if (pkMission->dwMissionId == dwMissionID && pkMission->dwBattlePassId == dwBattlePassID)
            return pkMission->dwExtraInfo;
    }

    return 0;
}

bool CHARACTER::IsCompletedMission(uint8_t bMissionType)
{
    auto it = m_listBattlePass.begin();
    while (it != m_listBattlePass.end())
    {
        TPlayerBattlePassMission* pkMission = *it++;
        if (pkMission->dwMissionId == bMissionType)
            return (pkMission->bCompleted ? true : false);
    }

    return false;
}

void CHARACTER::UpdateMissionProgress(uint32_t dwMissionID, uint32_t dwBattlePassID, uint32_t dwUpdateValue, uint32_t dwTotalValue, bool isOverride)
{
    if (!m_bIsLoadedBattlePass)
        return;
#ifdef ENABLE_FREE_PASS_RAZOR93
    dwTotalValue = GetBattlePassAdjustedTotal(dwMissionID, dwBattlePassID, dwTotalValue);
#endif
    bool foundMission = false;
    uint32_t dwSaveProgress = 0;

    auto it = m_listBattlePass.begin();
    while (it != m_listBattlePass.end())
    {
        TPlayerBattlePassMission* pkMission = *it++;

        if (pkMission->dwMissionId == dwMissionID && pkMission->dwBattlePassId == dwBattlePassID)
        {
            pkMission->bIsUpdated = 1;
#ifdef ENABLE_FREE_PASS_RAZOR93
            if (pkMission->bCompleted)
                return;
#endif
            if (isOverride)
                pkMission->dwExtraInfo = dwUpdateValue;
            else
                pkMission->dwExtraInfo += dwUpdateValue;

            if (pkMission->dwExtraInfo >= dwTotalValue)
            {
                pkMission->dwExtraInfo = dwTotalValue;
                pkMission->bCompleted = 1;

#ifdef ENABLE_BATTLE_PASS_STAY_ONLINE
                if (pkMission->dwMissionId == STAY_ONLINE_MINUTES)
                    CancelStayOnlineEvent();
#endif
                CBattlePass::instance().BattlePassRewardMission(this, dwMissionID, dwBattlePassID);
            }

            dwSaveProgress = pkMission->dwExtraInfo;
            foundMission = true;
            break;
        }
    }

    if (!foundMission)
    {
        TPlayerBattlePassMission* newMission = new TPlayerBattlePassMission;
        newMission->dwPlayerId = GetPlayerID();
        newMission->dwMissionId = dwMissionID;
        newMission->dwBattlePassId = dwBattlePassID;

        if (dwUpdateValue >= dwTotalValue)
        {
            newMission->dwExtraInfo = dwTotalValue;
            newMission->bCompleted = 1;
#ifdef ENABLE_BATTLE_PASS_STAY_ONLINE
            if (newMission->dwMissionId == STAY_ONLINE_MINUTES)
                CancelStayOnlineEvent();
#endif
            CBattlePass::instance().BattlePassRewardMission(this, dwMissionID, dwBattlePassID);

            dwSaveProgress = dwTotalValue;
        }
        else
        {
            newMission->dwExtraInfo = dwUpdateValue;
            newMission->bCompleted = 0;

            dwSaveProgress = dwUpdateValue;
        }

        newMission->bIsUpdated = 1;

        m_listBattlePass.push_back(newMission);
    }

    if (!GetDesc())
        return;

    TPacketGCBattlePassUpdate packet;
    packet.bHeader = HEADER_GC_BATTLE_PASS_UPDATE;
    packet.bMissionType = dwMissionID;
    packet.dwNewProgress = dwSaveProgress;
    GetDesc()->Packet(&packet, sizeof(TPacketGCBattlePassUpdate));
}

uint8_t CHARACTER::GetBattlePassId()
{
    CAffect* pAffect = FindAffect(AFFECT_BATTLE_PASS, POINT_BATTLE_PASS_ID);

    if (!pAffect)
        return 0;

    return pAffect->lApplyValue;
}

int CHARACTER::GetSecondsTillNextMonth()
{
    time_t iTime;
    time(&iTime);
    struct tm endTime = *localtime(&iTime);

    int iCurrentMonth = endTime.tm_mon;

    endTime.tm_hour = 0;
    endTime.tm_min = 0;
    endTime.tm_sec = 0;
    endTime.tm_mday = 1;

    if (iCurrentMonth == 12)
    {
        endTime.tm_mon = 0;
        endTime.tm_year = endTime.tm_year + 1;
    }
    else
    {
        endTime.tm_mon = iCurrentMonth + 1;
    }

    int seconds = difftime(mktime(&endTime), iTime);

    return seconds;
}
#endif

#if defined(BL_OFFLINE_MESSAGE)
void CHARACTER::SendOfflineMessage(const char* To, const char* Message)
{
    if (!GetDesc())
        return;

    if (strlen(To) < 1)
        return;

    TPacketGDSendOfflineMessage p;
    strlcpy(p.szFrom, GetName(), sizeof(p.szFrom));
    strlcpy(p.szTo, To, sizeof(p.szTo));
    strlcpy(p.szMessage, Message, sizeof(p.szMessage));
    db_clientdesc->DBPacket(HEADER_GD_SEND_OFFLINE_MESSAGE, GetDesc()->GetHandle(), &p, sizeof(p));

    SetLastOfflinePMTime();
}

void CHARACTER::ReadOfflineMessages()
{
    if (!GetDesc())
        return;

    TPacketGDReadOfflineMessage p;
    strlcpy(p.szName, GetName(), sizeof(p.szName));
    db_clientdesc->DBPacket(HEADER_GD_REQUEST_OFFLINE_MESSAGES, GetDesc()->GetHandle(), &p, sizeof(p));
}
#endif

#ifdef ENABLE_RUNE_SYSTEM
uint16_t CHARACTER::GetRuneEffect() {
    if (!IsPC())
        return 0;

    if (GetQuestFlag("rune.hide_effect") == 1)
        return 0;

    uint16_t r = 1;
    LPITEM pkItem = nullptr;
    int iMaxSubTypes = RUNE_SUBTYPES - 1;
    int32_t lMaxTime = 0;
    int32_t lOnePercent = 0;
    int32_t lRemainPercent = 0;

    for (int i = 0; i < iMaxSubTypes; i++) {
        pkItem = GetWear(WEAR_RUNE1 + i);
        if (!pkItem) {
            r = 0;
            break;
        }
        else {
            if (pkItem->GetSocket(1) != 1) {
                r = 0;
                break;
            }
            else {
                lMaxTime = pkItem->GetValue(0);
                lOnePercent = lMaxTime / 100;
                lRemainPercent = pkItem->GetSocket(ITEM_SOCKET_REMAIN_SEC) / lOnePercent;
                if (lRemainPercent < RUNE_EFFECT_FROM) {
                    r = 0;
                    break;
                }
            }
        }
    }

    return r;
}
#endif

bool CHARACTER::CanTakeInventoryItem(LPITEM item, TItemPos* cell)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
    ChatPacket(CHAT_TYPE_INFO, "char.cpp::bool CHARACTER::CanTakeInventoryItem");
#endif
    int iEmpty = -1;

    if (item->IsDragonSoul())
    {
        cell->window_type = DRAGON_SOUL_INVENTORY;
        cell->cell = iEmpty = GetEmptyDragonSoulInventory(item);
    }

#ifdef ENABLE_EXTRA_INVENTORY
    else if (item->IsExtraItem())
    {
        cell->window_type = EXTRA_INVENTORY;
        cell->cell = iEmpty = GetEmptyExtraInventory(item);
    }
#endif
    else
    {
        cell->window_type = INVENTORY;
        cell->cell = iEmpty = GetEmptyInventory(item->GetSize());
    }

    return iEmpty != -1;
}

#ifdef ENABLE_SOUL_SYSTEM
int CHARACTER::GetSoulItemDamage(LPCHARACTER pkVictim, int iDamage, uint8_t bSoulType)
{
    if (!pkVictim)
        return 0;

    if (!IsPC() || IsPolymorphed() || pkVictim->IsPC())
        return 0;

    if (bSoulType >= SOUL_MAX_NUM)
        return 0;

    const CAffect* pAffect = FindAffect(AFFECT_SOUL_RED + bSoulType);
    int iDamageAdd = 0;
    if (pAffect)
    {
        LPITEM soulItem = FindItemByID(pAffect->lSPCost);
        if (soulItem)
        {
            int iCurrentMinutes = (soulItem->GetSocket(2) / 10000);
            int iCurrentStrike = (soulItem->GetSocket(2) % 10000);

            int valueIndex = MINMAX(3, 2 + (iCurrentMinutes / 60), 5);
            float fDamageIncrease = float(soulItem->GetValue(valueIndex) / 10.0f);

            iDamageAdd = (fDamageIncrease * iDamage) - iDamage;
            int iNextStrikes = iCurrentStrike - 1;
            if (iNextStrikes <= 0)
            {
                iCurrentMinutes = MINMAX(0, iCurrentMinutes - 60, 180);
                iNextStrikes = soulItem->GetValue(2);

                if (iCurrentMinutes < 60)
                {
                    soulItem->Lock(false);
                    soulItem->SetSocket(1, false);
                    RemoveAffect(const_cast<CAffect*>(pAffect));
                }

                soulItem->SetSocket(2, 0);
                soulItem->StartSoulItemEvent();
            }

            soulItem->SetSocket(2, (iCurrentMinutes * 10000 + iNextStrikes));
        }
    }

    return iDamageAdd;
}
#endif

#ifdef __SKILL_COLOR_SYSTEM__
void CHARACTER::SetSkillColor(uint32_t* dwSkillColor) {
    memcpy(m_dwSkillColor, dwSkillColor, sizeof(m_dwSkillColor));
    UpdatePacket();
}
#endif

void CHARACTER::SetShop(LPSHOP pkShop)
{
    if ((m_pkShop = pkShop)) {
        SET_BIT(m_pointsInstant.instant_flag, INSTANT_FLAG_SHOP);
    }
    else
    {
        REMOVE_BIT(m_pointsInstant.instant_flag, INSTANT_FLAG_SHOP);
        SetShopOwner(nullptr);
    }
}

void CHARACTER::SetExchange(CExchange* pkExchange)
{
    m_pkExchange = pkExchange;
}

void CHARACTER::SetRegen(LPREGEN pkRegen)
{
    m_pkRegen = pkRegen;
    if (pkRegen != nullptr) {
        regen_id_ = pkRegen->id;
    }
    m_fRegenAngle = GetRotation();
    m_posRegen = GetXYZ();
}

LPCHARACTER CHARACTER::GetMarryPartner() const
{
    return m_pkChrMarried;
}

void CHARACTER::SetMarryPartner(LPCHARACTER ch)
{
    m_pkChrMarried = ch;
}

void CHARACTER::SetDungeon(LPDUNGEON pkDungeon)
{
    if (pkDungeon && m_pkDungeon)
    {
        sys_err("%s is trying to reassigning dungeon (current %p, new party %p)", GetName(), get_pointer(m_pkDungeon), get_pointer(pkDungeon));
    }

    if (m_pkDungeon)
    {
        if (IsPC())
        {
            if (GetParty())
                m_pkDungeon->DecPartyMember(GetParty(), this);
            else
                m_pkDungeon->DecMember(this);
        }
    }

    m_pkDungeon = pkDungeon;

    if (pkDungeon)
    {
        if (IsPC())
        {
            if (GetParty())
                m_pkDungeon->IncPartyMember(GetParty(), this);
            else
                m_pkDungeon->IncMember(this);
        }
        else if (IsMonster() || IsStone())
        {
            m_pkDungeon->IncMonster();
        }
    }
}

void CHARACTER::SetWarMap(CWarMap* pWarMap)
{
    if (m_pWarMap)
        m_pWarMap->DecMember(this);

    m_pWarMap = pWarMap;

    if (m_pWarMap)
        m_pWarMap->IncMember(this);
}

void CHARACTER::SetWeddingMap(marriage::WeddingMap* pMap)
{
    if (m_pWeddingMap)
        m_pWeddingMap->DecMember(this);

    m_pWeddingMap = pMap;

    if (m_pWeddingMap)
        m_pWeddingMap->IncMember(this);
}

void CHARACTER::OpenAcce(bool bCombination)
{
    if (isAcceOpened(bCombination))
    {
#ifdef TEXTS_IMPROVEMENT
        ChatPacketNew(CHAT_TYPE_INFO, 659, "");
#endif
        return;
    }

    if (bCombination)
    {
        if (m_bAcceAbsorption)
        {
#ifdef TEXTS_IMPROVEMENT
            ChatPacketNew(CHAT_TYPE_INFO, 660, "");
#endif
            return;
        }

        m_bAcceCombination = true;
    }
    else
    {
        if (m_bAcceCombination)
        {
#ifdef TEXTS_IMPROVEMENT
            ChatPacketNew(CHAT_TYPE_INFO, 661, "");
#endif
            return;
        }

        m_bAcceAbsorption = true;
    }

    TItemPos tPos;
    tPos.window_type = INVENTORY;
    tPos.cell = 0;

    TPacketAcce sPacket;
    sPacket.header = HEADER_GC_ACCE;
    sPacket.subheader = ACCE_SUBHEADER_GC_OPEN;
    sPacket.bWindow = bCombination;
    sPacket.dwPrice = 0;
    sPacket.bPos = 0;
    sPacket.tPos = tPos;
    sPacket.dwItemVnum = 0;
    sPacket.dwMinAbs = 0;
    sPacket.dwMaxAbs = 0;
    GetDesc()->Packet(&sPacket, sizeof(TPacketAcce));

    ClearAcceMaterials();
}

void CHARACTER::CloseAcce()
{
    if ((!m_bAcceCombination) && (!m_bAcceAbsorption))
        return;

    bool bWindow = (m_bAcceCombination == true ? true : false);

    TItemPos tPos;
    tPos.window_type = INVENTORY;
    tPos.cell = 0;

    TPacketAcce sPacket;
    sPacket.header = HEADER_GC_ACCE;
    sPacket.subheader = ACCE_SUBHEADER_GC_CLOSE;
    sPacket.bWindow = bWindow;
    sPacket.dwPrice = 0;
    sPacket.bPos = 0;
    sPacket.tPos = tPos;
    sPacket.dwItemVnum = 0;
    sPacket.dwMinAbs = 0;
    sPacket.dwMaxAbs = 0;
    GetDesc()->Packet(&sPacket, sizeof(TPacketAcce));

    if (bWindow)
        m_bAcceCombination = false;
    else
        m_bAcceAbsorption = false;

    ClearAcceMaterials();
}

void CHARACTER::ClearAcceMaterials()
{
    LPITEM* pkItemMaterial;
    pkItemMaterial = GetAcceMaterials();
    for (int i = 0; i < ACCE_WINDOW_MAX_MATERIALS; ++i)
    {
        if (!pkItemMaterial[i])
            continue;

        pkItemMaterial[i]->Lock(false);
        pkItemMaterial[i] = nullptr;
    }
}

bool CHARACTER::AcceIsSameGrade(int32_t lGrade)
{
    LPITEM* pkItemMaterial;
    pkItemMaterial = GetAcceMaterials();
    if (!pkItemMaterial[0])
        return false;

    bool bReturn = (pkItemMaterial[0]->GetValue(ACCE_GRADE_VALUE_FIELD) == lGrade ? true : false);
    return bReturn;
}

uint32_t CHARACTER::GetAcceCombinePrice(int32_t lGrade
#ifdef ENABLE_STOLE_COSTUME
    , bool isCostume
#endif
)
{
    uint32_t dwPrice;
    switch (lGrade)
    {
    case 2:
    {
#ifdef ENABLE_STOLE_COSTUME
        dwPrice = isCostume ? COSTUME_STOLE_GRADE_2_PRICE : ACCE_GRADE_2_PRICE;
#else
        dwPrice = ACCE_GRADE_2_PRICE;
#endif
    }
    break;
    case 3:
    {
#ifdef ENABLE_STOLE_COSTUME
        dwPrice = isCostume ? COSTUME_STOLE_GRADE_3_PRICE : ACCE_GRADE_3_PRICE;
#else
        dwPrice = ACCE_GRADE_2_PRICE;
#endif
    }
    break;
    case 4:
    {
#ifdef ENABLE_STOLE_COSTUME
        dwPrice = isCostume ? 0 : ACCE_GRADE_4_PRICE;
#else
        dwPrice = ACCE_GRADE_2_PRICE;
#endif
    }
    break;
    default:
    {
#ifdef ENABLE_STOLE_COSTUME
        dwPrice = isCostume ? COSTUME_STOLE_GRADE_1_PRICE : ACCE_GRADE_1_PRICE;
#else
        dwPrice = ACCE_GRADE_1_PRICE;
#endif
    }
    break;
    }

    return dwPrice;
}

uint8_t CHARACTER::CheckEmptyMaterialSlot()
{
    const LPITEM* pkItemMaterial = GetAcceMaterials();
    for (int i = 0; i < ACCE_WINDOW_MAX_MATERIALS; ++i)
    {
        if (!pkItemMaterial[i])
            return i;
    }

    return 255;
}

void CHARACTER::GetAcceCombineResult(uint32_t& dwItemVnum, uint32_t& dwMinAbs, uint32_t& dwMaxAbs)
{
    const LPITEM* pkItemMaterial = GetAcceMaterials();

    if (m_bAcceCombination)
    {
        if ((pkItemMaterial[0]) && (pkItemMaterial[1]))
        {
            int32_t lVal = pkItemMaterial[0]->GetValue(ACCE_GRADE_VALUE_FIELD);
            if (lVal == 4)
            {
                dwItemVnum = pkItemMaterial[0]->GetOriginalVnum();
                dwMinAbs = pkItemMaterial[0]->GetSocket(ACCE_ABSORPTION_SOCKET);
                uint32_t dwMaxAbsCalc = (dwMinAbs + ACCE_GRADE_4_ABS_RANGE > ACCE_GRADE_4_ABS_MAX ? ACCE_GRADE_4_ABS_MAX : (dwMinAbs + ACCE_GRADE_4_ABS_RANGE));
                dwMaxAbs = dwMaxAbsCalc;
            }
            else
            {
                uint32_t dwMaskVnum = pkItemMaterial[0]->GetOriginalVnum();
                TItemTable* pTable = ITEM_MANAGER::instance().GetTable(dwMaskVnum + 1);
                if (pTable)
                    dwMaskVnum += 1;

                dwItemVnum = dwMaskVnum;
                switch (lVal)
                {
                case 2:
                {
                    dwMinAbs = ACCE_GRADE_3_ABS;
                    dwMaxAbs = ACCE_GRADE_3_ABS;
                }
                break;
                case 3:
                {
                    dwMinAbs = ACCE_GRADE_4_ABS_MIN;
                    dwMaxAbs = ACCE_GRADE_4_ABS_MAX_COMB;
                }
                break;
                default:
                {
                    dwMinAbs = ACCE_GRADE_2_ABS;
                    dwMaxAbs = ACCE_GRADE_2_ABS;
                }
                break;
                }
            }
        }
        else
        {
            dwItemVnum = 0;
            dwMinAbs = 0;
            dwMaxAbs = 0;
        }
    }
    else
    {
        if ((pkItemMaterial[0]) && (pkItemMaterial[1]))
        {
            dwItemVnum = pkItemMaterial[0]->GetOriginalVnum();
            dwMinAbs = pkItemMaterial[0]->GetSocket(ACCE_ABSORPTION_SOCKET);
            dwMaxAbs = dwMinAbs;
        }
        else
        {
            dwItemVnum = 0;
            dwMinAbs = 0;
            dwMaxAbs = 0;
        }
    }
}
