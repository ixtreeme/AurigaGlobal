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
#include "../../exchange.h"
#include "../../gm.h"
#include "../../item.h"
#include "../../item_manager.h"
#include "../../log.h"
#include "../../marriage.h"
#include "../../mining.h"
#include "../../mob_manager.h"
#include "../../MountSystem.h"
#include "../../new_offlineshop.h"
#include "../../New_PetSystem.h"
#include "../../PetSystem.h"
#include "../../questmanager.h"
#include "../../regen.h"
#include "../../shop.h"
#include "../../skill_power.h"
#include "../../target.h"
#include "../../war_map.h"
#include "../../wedding.h"
#include "../../../common/rune_length.h"
#include "../../../common/stole_length.h"
#ifdef ENABLE_ANTICHEAT
#include "../../hwidmanager.h"
#endif

extern bool RaceToJob(unsigned race, unsigned* ret_job);
EVENTFUNC(kill_ore_load_event);

namespace
{
#ifdef ENABLE_PVP_ADVANCED
int GetDuelImpl(const CHARACTER* ch, const char* type)
{
    const char* szTableStaticPvP[] = { BLOCK_CHANGEITEM, BLOCK_BUFF, BLOCK_POTION, BLOCK_RIDE, BLOCK_PET, BLOCK_POLY, BLOCK_PARTY, BLOCK_EXCHANGE_, BET_WINNER, CHECK_IS_FIGHT };

    int m_nDuelTable[] = { (ch->GetQuestFlag(szTableStaticPvP[0])), (ch->GetQuestFlag(szTableStaticPvP[1])), (ch->GetQuestFlag(szTableStaticPvP[2])), (ch->GetQuestFlag(szTableStaticPvP[3])), (ch->GetQuestFlag(szTableStaticPvP[4])), (ch->GetQuestFlag(szTableStaticPvP[5])), (ch->GetQuestFlag(szTableStaticPvP[6])), (ch->GetQuestFlag(szTableStaticPvP[7])), (ch->GetQuestFlag(szTableStaticPvP[8])), (ch->GetQuestFlag(szTableStaticPvP[9])) };

    if (!strcmp(type, "BlockChangeItem") && m_nDuelTable[0] > 0) {
        return true;
    }
    if (!strcmp(type, "BlockBuff") && m_nDuelTable[1] > 0) {
        return true;
    }
    if (!strcmp(type, "BlockPotion") && m_nDuelTable[2] > 0) {
        return true;
    }
    if (!strcmp(type, "BlockRide") && m_nDuelTable[3] > 0) {
        return true;
    }
    if (!strcmp(type, "BlockPet") && m_nDuelTable[4] > 0) {
        return true;
    }
    if (!strcmp(type, "BlockPoly") && m_nDuelTable[5] > 0) {
        return true;
    }
    if (!strcmp(type, "BlockParty") && m_nDuelTable[6] > 0) {
        return true;
    }
    if (!strcmp(type, "BlockExchange") && m_nDuelTable[7] > 0) {
        return true;
    }
    if (!strcmp(type, "BetMoney") && m_nDuelTable[8] > 0) {
        return true;
    }
    if (!strcmp(type, "IsFight") && m_nDuelTable[9] > 0) {
        return true;
    }
    return false;
}

void SetDuelImpl(CHARACTER* ch, const char* type, int value)
{
    const char* szTableStaticPvP[] = { BLOCK_CHANGEITEM, BLOCK_BUFF, BLOCK_POTION, BLOCK_RIDE, BLOCK_PET, BLOCK_POLY, BLOCK_PARTY, BLOCK_EXCHANGE_, BET_WINNER, CHECK_IS_FIGHT };

    if (!strcmp(type, "BlockChangeItem")) {
        ch->SetQuestFlag(szTableStaticPvP[0], value);
    }
    if (!strcmp(type, "BlockBuff")) {
        ch->SetQuestFlag(szTableStaticPvP[1], value);
    }
    if (!strcmp(type, "BlockPotion")) {
        ch->SetQuestFlag(szTableStaticPvP[2], value);
    }
    if (!strcmp(type, "BlockRide")) {
        ch->SetQuestFlag(szTableStaticPvP[3], value);
    }
    if (!strcmp(type, "BlockPet")) {
        ch->SetQuestFlag(szTableStaticPvP[4], value);
    }
    if (!strcmp(type, "BlockPoly")) {
        ch->SetQuestFlag(szTableStaticPvP[5], value);
    }
    if (!strcmp(type, "BlockParty")) {
        ch->SetQuestFlag(szTableStaticPvP[6], value);
    }
    if (!strcmp(type, "BlockExchange")) {
        ch->SetQuestFlag(szTableStaticPvP[7], value);
    }
    if (!strcmp(type, "BetMoney")) {
        ch->SetQuestFlag(szTableStaticPvP[8], value);
    }
    if (!strcmp(type, "IsFight")) {
        ch->SetQuestFlag(szTableStaticPvP[9], value);
    }
}
#endif
}

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

void CHARACTER::AddAcceMaterial(TItemPos tPos, uint8_t bPos)
{
    if (bPos >= ACCE_WINDOW_MAX_MATERIALS)
    {
        if (bPos == 255)
        {
            bPos = CheckEmptyMaterialSlot();
            if (bPos >= ACCE_WINDOW_MAX_MATERIALS)
                return;
        }
        else
            return;
    }

    LPITEM pkItem = GetItem(tPos);
    if (!pkItem)
        return;
    else if ((pkItem->GetCell() >= INVENTORY_MAX_NUM) || (pkItem->IsEquipped()) || (tPos.IsBeltInventoryPosition()) || (pkItem->IsDragonSoul()))
        return;
    else if ((pkItem->GetType() != ITEM_COSTUME) && (m_bAcceCombination))
        return;
    else if ((pkItem->GetType() != ITEM_COSTUME) && (bPos == 0) && (m_bAcceAbsorption))
        return;
    else if (pkItem->isLocked())
    {
#ifdef TEXTS_IMPROVEMENT
        ChatPacketNew(CHAT_TYPE_INFO, 519, "");
#endif
        return;
    }
    else if ((pkItem->GetType() == ITEM_ARMOR) && (pkItem->GetSubType() == ARMOR_BODY))
    {
#ifdef TEXTS_IMPROVEMENT
        ChatPacketNew(CHAT_TYPE_INFO, 519, "");
#endif
        return;
    }
#ifdef __SOULBINDING_SYSTEM__
    else if ((pkItem->IsBind()) || (pkItem->IsUntilBind()))
    {
#ifdef TEXTS_IMPROVEMENT
        ChatPacketNew(CHAT_TYPE_INFO, 519, "");
#endif
        return;
    }
#endif
#ifdef ENABLE_STOLE_COSTUME
    else if (m_bAcceAbsorption && bPos == 0 && pkItem->GetSubType() != COSTUME_ACCE)
    {
        return;
    }
#endif
    else if ((m_bAcceCombination) && (bPos == 1) && (!AcceIsSameGrade(pkItem->GetValue(ACCE_GRADE_VALUE_FIELD))))
    {
#ifdef TEXTS_IMPROVEMENT
        ChatPacketNew(CHAT_TYPE_INFO, 662, "");
#endif
        return;
    }
#ifdef ENABLE_STOLE_COSTUME
    else if ((m_bAcceCombination) && (pkItem->GetSubType() == COSTUME_STOLE) && (pkItem->GetValue(0) == 4))
    {
#ifdef TEXTS_IMPROVEMENT
        ChatPacketNew(CHAT_TYPE_INFO, 20, "%s", pkItem->GetName());
#endif
        return;
    }
#endif
    else if ((m_bAcceCombination) && (pkItem->GetSocket(ACCE_ABSORPTION_SOCKET) >= ACCE_GRADE_4_ABS_MAX))
    {
#ifdef TEXTS_IMPROVEMENT
        ChatPacketNew(CHAT_TYPE_INFO, 663, "%d", ACCE_GRADE_4_ABS_MAX);
#endif
        return;
    }
    else if ((bPos == 1) && (m_bAcceAbsorption))
    {
        if ((pkItem->GetType() != ITEM_WEAPON) && (pkItem->GetType() != ITEM_ARMOR))
        {
#ifdef TEXTS_IMPROVEMENT
            ChatPacketNew(CHAT_TYPE_INFO, 520, "");
#endif
            return;
        }
        else if ((pkItem->GetType() == ITEM_ARMOR) && (pkItem->GetSubType() != ARMOR_BODY))
        {
#ifdef TEXTS_IMPROVEMENT
            ChatPacketNew(CHAT_TYPE_INFO, 520, "");
#endif
            return;
        }
    }
    else if
#ifdef ENABLE_STOLE_COSTUME
    (
#endif
        ((pkItem->GetSubType() != COSTUME_ACCE)
#ifdef ENABLE_STOLE_COSTUME
            && (pkItem->GetSubType() != COSTUME_STOLE))
#endif
        && (m_bAcceCombination))
        return;
    else if
#ifdef ENABLE_STOLE_COSTUME
    (
#endif
        ((pkItem->GetSubType() != COSTUME_ACCE)
#ifdef ENABLE_STOLE_COSTUME
            && (pkItem->GetSubType() != COSTUME_STOLE))
#endif
        && (bPos == 0) && (m_bAcceAbsorption))
        return;
    else if ((pkItem->GetSocket(ACCE_ABSORBED_SOCKET) > 0) && (bPos == 0) && (m_bAcceAbsorption))
        return;

    LPITEM* pkItemMaterial;
    pkItemMaterial = GetAcceMaterials();
    if ((bPos == 1) && (!pkItemMaterial[0]))
        return;

#ifdef ENABLE_STOLE_COSTUME
    if ((!m_bAcceAbsorption) && (bPos == 1) && (pkItemMaterial[0]->GetSubType() != pkItem->GetSubType())) {
#ifdef TEXTS_IMPROVEMENT
        if (pkItemMaterial[0]->GetSubType() == COSTUME_STOLE) {
            ChatPacketNew(CHAT_TYPE_INFO, 18, "");
        }
        else {
            ChatPacketNew(CHAT_TYPE_INFO, 822, "");
        }
#endif
        return;
    }
    else if (!m_bAcceAbsorption && bPos == 1 && pkItemMaterial[0]->GetSubType() == COSTUME_STOLE && pkItemMaterial[0]->GetVnum() != pkItem->GetVnum()) {
#ifdef TEXTS_IMPROVEMENT
        ChatPacketNew(CHAT_TYPE_INFO, 1293, "");
#endif
        return;
    }
#endif

    if (pkItemMaterial[bPos])
        return;

    pkItemMaterial[bPos] = pkItem;
    pkItemMaterial[bPos]->Lock(true);

    uint32_t dwItemVnum, dwMinAbs, dwMaxAbs;
    GetAcceCombineResult(dwItemVnum, dwMinAbs, dwMaxAbs);

    TPacketAcce sPacket;
    sPacket.header = HEADER_GC_ACCE;
    sPacket.subheader = ACCE_SUBHEADER_GC_ADDED;
    sPacket.bWindow = m_bAcceCombination == true ? true : false;
    sPacket.dwPrice = GetAcceCombinePrice(pkItem->GetValue(ACCE_GRADE_VALUE_FIELD)
#ifdef ENABLE_STOLE_COSTUME
        , pkItem->GetSubType() == COSTUME_STOLE ? true : false
#endif
    );
    sPacket.bPos = bPos;
    sPacket.tPos = tPos;
    sPacket.dwItemVnum = dwItemVnum;
    sPacket.dwMinAbs = dwMinAbs;
    sPacket.dwMaxAbs = dwMaxAbs;
    GetDesc()->Packet(&sPacket, sizeof(TPacketAcce));
}

void CHARACTER::RemoveAcceMaterial(uint8_t bPos)
{
    if (bPos >= ACCE_WINDOW_MAX_MATERIALS)
        return;

    LPITEM* pkItemMaterial;
    pkItemMaterial = GetAcceMaterials();

    uint32_t dwPrice = 0;

    if (bPos == 1)
    {
        if (pkItemMaterial[bPos])
        {
            pkItemMaterial[bPos]->Lock(false);
            pkItemMaterial[bPos] = nullptr;
        }

        if (pkItemMaterial[0]) {
            dwPrice = GetAcceCombinePrice(pkItemMaterial[0]->GetValue(ACCE_GRADE_VALUE_FIELD)
#ifdef ENABLE_STOLE_COSTUME
                , pkItemMaterial[0]->GetSubType() == COSTUME_STOLE ? true : false
#endif
            );
        }
    }
    else
        ClearAcceMaterials();

    TItemPos tPos;
    tPos.window_type = INVENTORY;
    tPos.cell = 0;

    TPacketAcce sPacket;
    sPacket.header = HEADER_GC_ACCE;
    sPacket.subheader = ACCE_SUBHEADER_GC_REMOVED;
    sPacket.bWindow = m_bAcceCombination == true ? true : false;
    sPacket.dwPrice = dwPrice;
    sPacket.bPos = bPos;
    sPacket.tPos = tPos;
    sPacket.dwItemVnum = 0;
    sPacket.dwMinAbs = 0;
    sPacket.dwMaxAbs = 0;
    GetDesc()->Packet(&sPacket, sizeof(TPacketAcce));
}

uint8_t CHARACTER::CanRefineAcceMaterials()
{
    if (GetOfflineShopGuest() || GetAuctionGuest())
        return 0;

    if (GetExchange() || GetMyShop() || GetShopOwner() || IsOpenSafebox() || IsCubeOpen()
#ifdef __ATTR_TRANSFER_SYSTEM__
        || IsAttrTransferOpen()
#endif
        )
        return 0;

    uint8_t bReturn = 0;
    LPITEM* pkItemMaterial;
    pkItemMaterial = GetAcceMaterials();
    if (m_bAcceCombination)
    {
        for (int i = 0; i < ACCE_WINDOW_MAX_MATERIALS; ++i)
        {
            if (pkItemMaterial[i])
            {
                if ((pkItemMaterial[i]->GetType() == ITEM_COSTUME) && (pkItemMaterial[i]->GetSubType() == COSTUME_ACCE))
                    bReturn = 1;
#ifdef ENABLE_STOLE_COSTUME
                else if ((pkItemMaterial[i]->GetType() == ITEM_COSTUME) && (pkItemMaterial[i]->GetSubType() == COSTUME_STOLE))
                    bReturn = 1;
#endif
                else
                {
                    bReturn = 0;
                    break;
                }
            }
            else
            {
                bReturn = 0;
                break;
            }
        }
    }
    else if (m_bAcceAbsorption)
    {
        if ((pkItemMaterial[0]) && (pkItemMaterial[1]))
        {
            if ((pkItemMaterial[0]->GetType() == ITEM_COSTUME) && (pkItemMaterial[0]->GetSubType() == COSTUME_ACCE))
                bReturn = 2;
            else
                bReturn = 0;

            if ((pkItemMaterial[1]->GetType() == ITEM_WEAPON) || ((pkItemMaterial[1]->GetType() == ITEM_ARMOR) && (pkItemMaterial[1]->GetSubType() == ARMOR_BODY)))
                bReturn = 2;
#ifdef ATTR_LOCK
            if ((pkItemMaterial[1]->GetType() == ITEM_WEAPON) || ((pkItemMaterial[1]->GetType() == ITEM_ARMOR) && (pkItemMaterial[1]->GetSubType() == ARMOR_BODY)))
            {
                if (pkItemMaterial[1]->GetLockedAttr() != -1)
                {
                    bReturn = 0;
#ifdef TEXTS_IMPROVEMENT
                    ChatPacketNew(CHAT_TYPE_INFO, 783, "");
#endif
                }
            }
#endif
            else
                bReturn = 0;

            if (pkItemMaterial[0]->GetSocket(ACCE_ABSORBED_SOCKET) > 0)
                bReturn = 0;
        }
        else
            bReturn = 0;
    }

    return bReturn;
}

void CHARACTER::RefineAcceMaterials()
{
    uint8_t bCan = CanRefineAcceMaterials();
    if (bCan == 0)
        return;

    LPITEM* pkItemMaterial;
    pkItemMaterial = GetAcceMaterials();

    uint32_t dwItemVnum, dwMinAbs, dwMaxAbs;
    GetAcceCombineResult(dwItemVnum, dwMinAbs, dwMaxAbs);

    int64_t dwPrice = GetAcceCombinePrice(pkItemMaterial[0]->GetValue(ACCE_GRADE_VALUE_FIELD)
#ifdef ENABLE_STOLE_COSTUME
        , pkItemMaterial[0]->GetSubType() == COSTUME_STOLE ? true : false
#endif
    );


    if (bCan == 1)
    {
#ifdef ENABLE_STOLE_COSTUME
        bool bStole = pkItemMaterial[0]->GetSubType() == COSTUME_STOLE ? true : false;
#endif
        int iSuccessChance = 0;
        int32_t lVal = pkItemMaterial[0]->GetValue(ACCE_GRADE_VALUE_FIELD);
        switch (lVal)
        {
        case 2:
        {
#ifdef ENABLE_STOLE_COSTUME
            if (bStole) {
                iSuccessChance = STOLA_COMBINE_GRADE_2;
                break;
            }
#endif
            iSuccessChance = ACCE_COMBINE_GRADE_2;
        }
        break;
        case 3:
        {
#ifdef ENABLE_STOLE_COSTUME
            if (bStole) {
                iSuccessChance = STOLA_COMBINE_GRADE_3;
                break;
            }
#endif
            iSuccessChance = ACCE_COMBINE_GRADE_3;
        }
        break;
        case 4:
        {
            iSuccessChance = ACCE_COMBINE_GRADE_4;
        }
        break;
        default:
        {
#ifdef ENABLE_STOLE_COSTUME
            if (bStole) {
                iSuccessChance = STOLA_COMBINE_GRADE_1;
                break;
            }
#endif
            iSuccessChance = ACCE_COMBINE_GRADE_1;
        }
        break;
        }

        if (GetGold() < dwPrice)
        {
#ifdef TEXTS_IMPROVEMENT
            ChatPacketNew(CHAT_TYPE_INFO, 232, "");
#endif
            return;
        }

        int iChance = number(1, 100);
        bool bSucces = (iChance <= iSuccessChance ? true : false);
        if (bSucces)
        {
            LPITEM pkItem = ITEM_MANAGER::instance().CreateItem(dwItemVnum, 1, 0, false);
            if (!pkItem)
            {
                sys_err("%d can't be created.", dwItemVnum);
                return;
            }

#ifdef ENABLE_STOLE_COSTUME
            if (pkItem->GetSubType() != COSTUME_STOLE)
                ITEM_MANAGER::CopyAllAttrTo(pkItemMaterial[0], pkItem);
#else
            ITEM_MANAGER::CopyAllAttrTo(pkItemMaterial[0], pkItem);
#endif
            LogManager::instance().ItemLog(this, pkItem, "COMBINE SUCCESS", pkItem->GetName());
            uint32_t dwAbs = (dwMinAbs == dwMaxAbs ? dwMinAbs : number(dwMinAbs + 1, dwMaxAbs));
            pkItem->SetSocket(ACCE_ABSORPTION_SOCKET, dwAbs);
            pkItem->SetSocket(ACCE_ABSORBED_SOCKET, pkItemMaterial[0]->GetSocket(ACCE_ABSORBED_SOCKET));

            PointChange(POINT_GOLD, -dwPrice);
            DBManager::instance().SendMoneyLog(MONEY_LOG_REFINE, pkItemMaterial[0]->GetVnum(), -dwPrice);

            uint16_t wCell = pkItemMaterial[0]->GetCell();
            ITEM_MANAGER::instance().RemoveItem(pkItemMaterial[0], "COMBINE (REFINE SUCCESS)");
            ITEM_MANAGER::instance().RemoveItem(pkItemMaterial[1], "COMBINE (REFINE SUCCESS)");

            pkItem->AddToCharacter(this, TItemPos(INVENTORY, wCell));
            ITEM_MANAGER::instance().FlushDelayedSave(pkItem);
            pkItem->AttrLog();

#ifdef TEXTS_IMPROVEMENT
            if (lVal == 4) {
                ChatPacketNew(CHAT_TYPE_INFO, 521, "%d", dwAbs);
            }
            else {
                ChatPacketNew(CHAT_TYPE_INFO, 389, "");
            }
#endif
            EffectPacket(SE_EFFECT_ACCE_SUCCEDED);
            LogManager::instance().AcceLog(GetPlayerID(), GetX(), GetY(), dwItemVnum, pkItem->GetID(), 1, dwAbs, 1);

            ClearAcceMaterials();
        }
        else
        {
            PointChange(POINT_GOLD, -dwPrice);
            DBManager::instance().SendMoneyLog(MONEY_LOG_REFINE, pkItemMaterial[0]->GetVnum(), -dwPrice);
            ITEM_MANAGER::instance().RemoveItem(pkItemMaterial[1], "COMBINE (REFINE FAIL)");
#ifdef TEXTS_IMPROVEMENT
            ChatPacketNew(CHAT_TYPE_INFO, 390, "");
#endif
            LogManager::instance().AcceLog(GetPlayerID(), GetX(), GetY(), dwItemVnum, 0, 0, 0, 0);
            pkItemMaterial[1] = nullptr;
        }

        TItemPos tPos;
        tPos.window_type = INVENTORY;
        tPos.cell = 0;

        TPacketAcce sPacket;
        sPacket.header = HEADER_GC_ACCE;
        sPacket.subheader = ACCE_SUBHEADER_CG_REFINED;
        sPacket.bWindow = m_bAcceCombination == true ? true : false;
        sPacket.dwPrice = dwPrice;
        sPacket.bPos = 0;
        sPacket.tPos = tPos;
        sPacket.dwItemVnum = 0;
        sPacket.dwMinAbs = 0;
        if (bSucces)
            sPacket.dwMaxAbs = 100;
        else
            sPacket.dwMaxAbs = 0;

        GetDesc()->Packet(&sPacket, sizeof(TPacketAcce));
    }
    else
    {
        pkItemMaterial[1]->CopyAttributeTo(pkItemMaterial[0]);
        LogManager::instance().ItemLog(this, pkItemMaterial[0], "ABSORB (REFINE SUCCESS)", pkItemMaterial[0]->GetName());
        pkItemMaterial[0]->SetSocket(ACCE_ABSORBED_SOCKET, pkItemMaterial[1]->GetOriginalVnum());
        for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
        {
            if (pkItemMaterial[0]->GetAttributeValue(i) < 0)
                pkItemMaterial[0]->SetForceAttribute(i, pkItemMaterial[0]->GetAttributeType(i), 0);
        }

        ITEM_MANAGER::instance().RemoveItem(pkItemMaterial[1], "ABSORBED (REFINE SUCCESS)");

        ITEM_MANAGER::instance().FlushDelayedSave(pkItemMaterial[0]);
        pkItemMaterial[0]->AttrLog();

#ifdef TEXTS_IMPROVEMENT
        ChatPacketNew(CHAT_TYPE_INFO, 629, "");
#endif
        ClearAcceMaterials();

        TItemPos tPos;
        tPos.window_type = INVENTORY;
        tPos.cell = 0;

        TPacketAcce sPacket;
        sPacket.header = HEADER_GC_ACCE;
        sPacket.subheader = ACCE_SUBHEADER_CG_REFINED;
        sPacket.bWindow = m_bAcceCombination == true ? true : false;
        sPacket.dwPrice = dwPrice;
        sPacket.bPos = 255;
        sPacket.tPos = tPos;
        sPacket.dwItemVnum = 0;
        sPacket.dwMinAbs = 0;
        sPacket.dwMaxAbs = 1;
        GetDesc()->Packet(&sPacket, sizeof(TPacketAcce));
    }
}

bool CHARACTER::CleanAcceAttr(LPITEM pkItem, LPITEM pkTarget)
{
    if (!CanHandleItem())
        return false;
    else if ((!pkItem) || (!pkTarget))
        return false;
    else if ((pkTarget->GetType() != ITEM_COSTUME) && (pkTarget->GetSubType() != COSTUME_ACCE))
        return false;

    if (pkTarget->GetSocket(ACCE_ABSORBED_SOCKET) <= 0)
        return false;

    pkTarget->SetSocket(ACCE_ABSORBED_SOCKET, 0);
    for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
        pkTarget->SetForceAttribute(i, 0, 0);

    pkItem->SetCount(pkItem->GetCount() - 1);
    LogManager::instance().ItemLog(this, pkTarget, "USE_DETACHMENT (CLEAN ATTR)", pkTarget->GetName());
    return true;
}

#ifdef ENABLE_SORT_INVEN
static bool SortMyItems(const LPITEM& s1, const LPITEM& s2)
{
    std::string name(s1->GetName());
    std::string name2(s2->GetName());

    return name < name2;
}

void CHARACTER::EditMyInven()
{
    return;

    int iPulse = thecore_pulse() - GetSortInv1Time();
    if (iPulse < PASSES_PER_SEC(30)) {
#ifdef TEXTS_IMPROVEMENT
        ChatPacketNew(CHAT_TYPE_INFO, 1290, "");
#endif
        return;
    }

    if (IsDead() || GetExchange() || GetShopOwner() || GetMyShop() || IsOpenSafebox() || IsCubeOpen())
    {
#ifdef TEXTS_IMPROVEMENT
        ChatPacketNew(CHAT_TYPE_INFO, 540, "");
#endif
        return;
    }

#ifdef __ENABLE_NEW_OFFLINESHOP__
    if (GetOfflineShopGuest() || GetAuctionGuest())
    {
#ifdef TEXTS_IMPROVEMENT
        ChatPacketNew(CHAT_TYPE_INFO, 782, "");
#endif
        return;
    }
#endif

    static std::vector<LPITEM> v;
    LPITEM myitems;

    std::map<uint32_t, uint8_t> mapOldPosition;

    v.clear();

#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
    int size = Inventory_Size();
#else
    int size = INVENTORY_MAX_NUM;
#endif

    for (int i = 0; i < size; ++i)
    {
        if (!(myitems = GetInventoryItem(i)))
            continue;

        v.push_back(myitems);
        mapOldPosition.insert(std::make_pair(myitems->GetID(), myitems->GetCell()));
        myitems->RemoveFromCharacter();
    }
    std::sort(v.begin(), v.end(), SortMyItems);

    std::vector<TQuickslot*> vecItemQuickslot;
    for (auto& quick : m_quickslot)
        if (quick.type == QUICKSLOT_TYPE_ITEM)
            vecItemQuickslot.push_back(&quick);

    auto lambdaChecker = [&vecItemQuickslot, &mapOldPosition](LPITEM pItemLocal)
        {
            auto iter = mapOldPosition.find(pItemLocal->GetID());
            if (iter == mapOldPosition.end())
                return (TQuickslot*)nullptr;

            auto itemPos = iter->second;

            for (auto it = vecItemQuickslot.begin(); it != vecItemQuickslot.end(); it++)
            {
                TQuickslot* pQuick = *it;

                if (pQuick && pQuick->pos == itemPos)
                {
                    vecItemQuickslot.erase(it);
                    return pQuick;
                }
            }
            return (TQuickslot*)nullptr;
        };

    auto it = v.begin();
    while (it != v.end()) {
        LPITEM item = *(it++);
        if (item)
        {
            TQuickslot* pQuickSlot = lambdaChecker(item);
            bool isQuickSlotItem = pQuickSlot != nullptr;

            LPITEM newItem = item;

            TItemTable* p = ITEM_MANAGER::instance().GetTable(item->GetVnum());
            if (p && p->dwFlags & ITEM_FLAG_STACKABLE && p->bType != ITEM_BLEND
#ifdef ENABLE_NEW_USE_POTION
                && (p->bType != ITEM_USE && p->bSubType != USE_NEW_POTIION)
#endif
                )
                newItem = AutoGiveItem(item->GetVnum(), item->GetCount(), -1, false
#ifdef __HIGHLIGHT_SYSTEM__
                    , false
#endif
                );
            else
                AutoGiveItem(item, false
#ifdef __HIGHLIGHT_SYSTEM__
                    , false
#endif
                );

            if (isQuickSlotItem)
                SyncQuickslot(QUICKSLOT_TYPE_ITEM, pQuickSlot->pos, newItem->GetCell());
        }
    }

    ChatPacket(CHAT_TYPE_COMMAND, "inv_sort_done");
    SetSortInv1Time();
}

static bool SortMyExtraItems(const LPITEM& s1, const LPITEM& s2)
{
    uint32_t name(s1->GetVnum());
    uint32_t name2(s2->GetVnum());

    return name < name2;
}

void CHARACTER::EditMyExtraInven()
{
    return;

    int iPulse = thecore_pulse() - GetSortInv2Time();
    if (iPulse < PASSES_PER_SEC(30)) {
#ifdef TEXTS_IMPROVEMENT
        ChatPacketNew(CHAT_TYPE_INFO, 1290, "");
#endif
        return;
    }

    if (IsDead() || GetExchange() || GetShopOwner() || GetMyShop() || IsOpenSafebox() || IsCubeOpen())
    {
#ifdef TEXTS_IMPROVEMENT
        ChatPacketNew(CHAT_TYPE_INFO, 540, "");
#endif
        return;
    }

#ifdef __ENABLE_NEW_OFFLINESHOP__
    if (GetOfflineShopGuest() || GetAuctionGuest())
    {
#ifdef TEXTS_IMPROVEMENT
        ChatPacketNew(CHAT_TYPE_INFO, 782, "");
#endif
        return;
    }
#endif

    static std::vector<LPITEM> v;
    LPITEM myitems;

    std::map<uint32_t, uint8_t> mapOldPosition;

    v.clear();

    int size = EXTRA_INVENTORY_MAX_NUM;

    for (int i = 0; i < size; ++i)
    {
        if (!(myitems = GetExtraInventoryItem(i)))
            continue;

        v.push_back(myitems);
        mapOldPosition.insert(std::make_pair(myitems->GetID(), myitems->GetCell()));
        myitems->RemoveFromCharacter();
    }
    std::sort(v.begin(), v.end(), SortMyExtraItems);

    std::vector<TQuickslot*> vecItemQuickslot;
    for (auto& quick : m_quickslot)
        if (quick.type == QUICKSLOT_TYPE_ITEM)
            vecItemQuickslot.push_back(&quick);

    auto lambdaChecker = [&vecItemQuickslot, &mapOldPosition](LPITEM pItemLocal)
        {
            auto iter = mapOldPosition.find(pItemLocal->GetID());
            if (iter == mapOldPosition.end())
                return (TQuickslot*)nullptr;

            auto itemPos = iter->second;

            for (auto it = vecItemQuickslot.begin(); it != vecItemQuickslot.end(); it++)
            {
                TQuickslot* pQuick = *it;

                if (pQuick && pQuick->pos == itemPos)
                {
                    vecItemQuickslot.erase(it);
                    return pQuick;
                }
            }
            return (TQuickslot*)nullptr;
        };

    auto it = v.begin();
    while (it != v.end()) {
        LPITEM item = *(it++);
        if (item)
        {
            TQuickslot* pQuickSlot = lambdaChecker(item);
            bool isQuickSlotItem = pQuickSlot != nullptr;

            LPITEM newItem = item;

            TItemTable* p = ITEM_MANAGER::instance().GetTable(item->GetVnum());
            if (p && p->dwFlags & ITEM_FLAG_STACKABLE && p->bType != ITEM_BLEND)
                newItem = AutoGiveItem(item->GetVnum(), item->GetCount(), -1, false
#ifdef __HIGHLIGHT_SYSTEM__
                    , false
#endif
                );
            else
                AutoGiveItem(item, false
#ifdef __HIGHLIGHT_SYSTEM__
                    , false
#endif
                );

            if (isQuickSlotItem)
                SyncQuickslot(QUICKSLOT_TYPE_ITEM, pQuickSlot->pos, newItem->GetCell());
        }
    }

    ChatPacket(CHAT_TYPE_COMMAND, "ext_sort_done");
    SetSortInv2Time();
}
#endif

#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
static int NeedKeys[] = { 2,2,2,2,3,3,4,4,4,5,5,5,6,6,6,7,7,7 };
bool CHARACTER::Update_Inven()
{
#ifdef ENABLE_SPAM_CHECK
    int32_t time = GetLastUnlock() - get_global_time();
    if (time > 0) {
#ifdef TEXTS_IMPROVEMENT
        ChatPacketNew(CHAT_TYPE_INFO, 234, "%d", time);
#endif
        return false;
    }
#endif

#define key2 72320
    int needkey = NeedKeys[Inven_Point()];
    if (CountSpecifyItem(key2) >= needkey) {
        RemoveSpecifyItem(key2, needkey);
        PointChange(POINT_INVEN, 1, false);
        ChatPacket(CHAT_TYPE_COMMAND, "refreshinven");
        UpdatePacket();
#ifdef ENABLE_SPAM_CHECK
        SetLastUnlock();
#endif
        return true;
    }
    else {
        int need_key = needkey - CountSpecifyItem(key2);
        ChatPacket(CHAT_TYPE_COMMAND, "update_envanter_need %d", need_key);
        return false;
    }
}
#endif

bool CHARACTER::IsHack(bool bSendMsg, bool bCheckShopOwner, int limittime)
{
    const int iPulse = thecore_pulse();

    if (test_server)
        bSendMsg = true;

    if (iPulse - GetSafeboxLoadTime() < PASSES_PER_SEC(limittime))
    {
#ifdef TEXTS_IMPROVEMENT
        if (bSendMsg) {
            ChatPacketNew(CHAT_TYPE_INFO, 234, "%d", limittime);
        }
#endif
        return true;
    }

    if (bCheckShopOwner)
    {
        if (GetExchange() || GetMyShop() || GetShopOwner() || IsOpenSafebox() || IsCubeOpen()
#if defined(ENABLE_CHRISTMAS_WHEEL_OF_DESTINY)
            || GetWheelDestiny()
#endif
            )
        {
#ifdef TEXTS_IMPROVEMENT
            if (bSendMsg) {
                ChatPacketNew(CHAT_TYPE_INFO, 236, "");
            }
#endif
            return true;
        }
    }
    else
    {
        if (GetExchange() || GetMyShop() || IsOpenSafebox() || IsCubeOpen()
#if defined(ENABLE_CHRISTMAS_WHEEL_OF_DESTINY)
            || GetWheelDestiny()
#endif
            )
        {
#ifdef TEXTS_IMPROVEMENT
            if (bSendMsg) {
                ChatPacketNew(CHAT_TYPE_INFO, 236, "");
            }
#endif
            return true;
        }
    }

    if (iPulse - GetExchangeTime() < PASSES_PER_SEC(limittime))
    {
#ifdef TEXTS_IMPROVEMENT
        if (bSendMsg) {
            ChatPacketNew(CHAT_TYPE_INFO, 234, "%d", limittime);
        }
#endif
        return true;
    }

    if (iPulse - GetMyShopTime() < PASSES_PER_SEC(limittime))
    {
#ifdef TEXTS_IMPROVEMENT
        if (bSendMsg) {
            ChatPacketNew(CHAT_TYPE_INFO, 234, "%d", limittime);
        }
#endif
        return true;
    }

    if (iPulse - GetRefineTime() < PASSES_PER_SEC(limittime))
    {
#ifdef TEXTS_IMPROVEMENT
        if (bSendMsg) {
            ChatPacketNew(CHAT_TYPE_INFO, 234, "%d", limittime);
        }
#endif
        return true;
    }

    return false;
}

void CHARACTER::Say(const std::string& s)
{
    struct ::packet_script packet_script;

    packet_script.header = HEADER_GC_SCRIPT;
    packet_script.skin = 1;
    packet_script.src_size = s.size();
    packet_script.size = packet_script.src_size + sizeof(struct packet_script);

    TEMP_BUFFER buf;

    buf.write(&packet_script, sizeof(struct packet_script));
    buf.write(&s[0], s.size());

    if (IsPC())
    {
        GetDesc()->Packet(buf.read_peek(), buf.size());
    }
}

#ifdef __ENABLE_NEW_OFFLINESHOP__
void CHARACTER::SetShopSafebox(offlineshop::CShopSafebox* pk)
{
    if (m_pkShopSafebox && pk == nullptr)
        m_pkShopSafebox->SetOwner(nullptr);

    else if (m_pkShopSafebox == nullptr && pk)
        pk->SetOwner(this);

    m_pkShopSafebox = pk;
}
#endif

#ifdef ENABLE_RANKING
long long CHARACTER::GetRankPoints(int iArg)
{
    if ((iArg < 0) || (iArg >= RANKING_MAX_CATEGORIES))
        return 0;

    return m_lRankPoints[iArg];
}

void CHARACTER::SetRankPoints(int iArg, long long lPoint)
{
    if ((iArg < 0) || (iArg >= RANKING_MAX_CATEGORIES))
        return;

    m_lRankPoints[iArg] = lPoint;
    Save();
}

void CHARACTER::RankingSubcategory(int iArg)
{
    if (!GetDesc())
        return;

    if ((iArg < 0) || (iArg >= RANKING_MAX_CATEGORIES))
        return;

    TPacketGCRankingTable p;
    int j = 0;

    char szQuery1[1024] = { 0 };
    snprintf(szQuery1, sizeof(szQuery1), "SELECT account_id, level, name, r%d FROM player.player%s WHERE account_id=(SELECT id FROM account.account%s WHERE status='OK' AND id=account_id) AND name not in(SELECT mName FROM common.gmlist%s) ORDER BY r%d desc, level desc, name asc LIMIT 50", iArg, get_table_postfix(), get_table_postfix(), get_table_postfix(), iArg);
    std::unique_ptr<SQLMsg> pRes1(DBManager::instance().DirectQuery(szQuery1));
    uint32_t iRes = pRes1->Get()->uiNumRows;
    if (iRes > 0) {
        MYSQL_ROW data;
        while ((data = mysql_fetch_row(pRes1->Get()->pSQLResult))) {
            int col = 1;
            p.list[j].iPosition = j;
            p.list[j].iRealPosition = 0;
            p.list[j].iLevel = atoi(data[col++]);
            strlcpy(p.list[j].szName, data[col++], sizeof(p.list[j].szName));
            p.list[j].iPoints = atoi(data[col]);
            j += 1;
        }
    }

    if (j < MAX_RANKING_LIST) {
        for (int i = j; i < MAX_RANKING_LIST; i++) {
            p.list[i].iPosition = i;
            p.list[i].iRealPosition = 0;
            p.list[i].iLevel = 0;
            p.list[i].iPoints = 0;
            strlcpy(p.list[i].szName, "", sizeof(p.list[i].szName));
        }
    }

    char szQuery2[1024] = { 0 };
    if (GetGMLevel() > GM_PLAYER) {
        snprintf(szQuery2, sizeof(szQuery2), "SELECT * FROM (SELECT @rank:=0) a, (SELECT @rank:=@rank+1 r, r%d, name, level FROM player.player%s AS res ORDER BY r%d desc, level desc, name asc) as custom WHERE name='%s'", iArg, get_table_postfix(), iArg, GetName());
    }
    else {
        snprintf(szQuery2, sizeof(szQuery2), "SELECT * FROM (SELECT @rank:=0) a, (SELECT @rank:=@rank+1 r, r%d, name, level FROM player.player%s AS res WHERE name not in(SELECT mName FROM common.gmlist) ORDER BY r%d desc, level desc, name asc) as custom WHERE name='%s'", iArg, get_table_postfix(), iArg, GetName());
    }
    std::unique_ptr<SQLMsg> pRes2(DBManager::instance().DirectQuery(szQuery2));
    iRes = pRes2->Get()->uiNumRows;
    if (iRes > 0) {
        j = MAX_RANKING_LIST - 1;
        MYSQL_ROW data = mysql_fetch_row(pRes2->Get()->pSQLResult);
        p.list[j].iPosition = j;
        p.list[j].iRealPosition = atoi(data[1]);
        p.list[j].iLevel = atoi(data[4]);
        p.list[j].iPoints = atoi(data[2]);
        strlcpy(p.list[j].szName, GetName(), sizeof(p.list[j].szName));
    }

    GetDesc()->Packet(&p, sizeof(p));
}
#endif

#ifdef ENABLE_PVP_ADVANCED
int CHARACTER::GetDuel(const char* type) const
{
    return GetDuelImpl(this, type);
}

void CHARACTER::SetDuel(const char* type, int value)
{
    SetDuelImpl(this, type, value);
}
#endif

void CHARACTER::SetPart(uint8_t bPartPos, uint16_t wVal)
{
    assert(bPartPos < PART_MAX_NUM);
    m_pointsInstant.parts[bPartPos] = wVal;
}

uint16_t CHARACTER::GetPart(uint8_t bPartPos) const
{
    assert(bPartPos < PART_MAX_NUM);

#ifdef __HIDE_COSTUME_SYSTEM__
    if (bPartPos == PART_MAIN && GetWear(WEAR_COSTUME_BODY) && IsBodyCostumeHidden() == true) {
        if (const LPITEM pArmor = GetWear(WEAR_BODY))
#ifdef __CHANGE_LOOK_SYSTEM__
            return pArmor->GetTransmutation() != 0 ? pArmor->GetTransmutation() : pArmor->GetVnum();
#else
            return pArmor->GetVnum();
#endif
        else
            return 0;
    }
    else if (bPartPos == PART_HAIR && GetWear(WEAR_COSTUME_HAIR) && IsHairCostumeHidden() == true)
        return 0;
#ifdef ENABLE_STOLE_COSTUME
    else if (bPartPos == PART_ACCE && GetWear(WEAR_COSTUME_ACCE) && IsAcceCostumeHidden() == true) {
        LPITEM pAcce = GetWear(WEAR_COSTUME_ACCE_SLOT);
        if (pAcce) {
            uint32_t toSetValue = pAcce->GetVnum();
            toSetValue -= 85000;
            if (pAcce->GetSocket(ACCE_ABSORPTION_SOCKET) >= ACCE_EFFECT_FROM_ABS)
                toSetValue += 1000;

            return toSetValue;
        }
        else
            return 0;
    }
#else
    else if (bPartPos == PART_ACCE && GetWear(WEAR_COSTUME_ACCE_SLOT) && IsAcceCostumeHidden() == true)
        return 0;
#endif
    else if (bPartPos == PART_WEAPON && GetWear(WEAR_COSTUME_WEAPON) && IsWeaponCostumeHidden() == true)
    {
        if (const LPITEM pWeapon = GetWear(WEAR_WEAPON))
#ifdef __CHANGE_LOOK_SYSTEM__
            return pWeapon->GetTransmutation() != 0 ? pWeapon->GetTransmutation() : pWeapon->GetVnum();
#else
            return pWeapon->GetVnum();
#endif
        else
            return 0;
    }
#endif

    return m_pointsInstant.parts[bPartPos];
}

uint16_t CHARACTER::GetOriginalPart(uint8_t bPartPos) const
{
    switch (bPartPos)
    {
    case PART_MAIN:
    {
        if (!IsPC())
            return GetPart(PART_MAIN);

#ifdef __HIDE_COSTUME_SYSTEM__
        if (GetWear(WEAR_COSTUME_BODY) && IsBodyCostumeHidden() == true) {
            if (const LPITEM pArmor = GetWear(WEAR_BODY))
                return pArmor->GetVnum();
        }
#endif

        return m_pointsInstant.bBasePart;
    }
    case PART_HAIR:
    {
#ifdef __HIDE_COSTUME_SYSTEM__
        if (GetWear(WEAR_COSTUME_HAIR) && IsHairCostumeHidden() == true)
            return 0;
#endif

        return GetPart(PART_HAIR);
    }
#ifdef ENABLE_ACCE_SYSTEM
    case PART_ACCE:
    {
#ifdef __HIDE_COSTUME_SYSTEM__
#ifdef ENABLE_STOLE_COSTUME
        if (GetWear(WEAR_COSTUME_ACCE) && IsAcceCostumeHidden() == true) {
            LPITEM pAcce = GetWear(WEAR_COSTUME_ACCE_SLOT);
            if (pAcce) {
                uint32_t toSetValue = pAcce->GetVnum();
                toSetValue -= 85000;
                if (pAcce->GetSocket(ACCE_ABSORPTION_SOCKET) >= ACCE_EFFECT_FROM_ABS)
                    toSetValue += 1000;

                return toSetValue;
            }
            else
                return 0;
        }
#else
        if (GetWear(WEAR_COSTUME_ACCE_SLOT) && IsAcceCostumeHidden() == true)
            return 0;
#endif
#else
        if (GetWear(WEAR_COSTUME_ACCE_SLOT))
            return 0;
#endif
        return GetPart(PART_ACCE);
    }
#endif
#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
    case PART_WEAPON:
    {
#ifdef __HIDE_COSTUME_SYSTEM__
        if (GetWear(WEAR_COSTUME_WEAPON) && IsWeaponCostumeHidden() == true) {
            if (const LPITEM pWeapon = GetWear(WEAR_WEAPON))
                return pWeapon->GetVnum();
        }
#endif
        return GetPart(PART_WEAPON);
#endif
    }
    default:
        return 0;
    }
}

void CHARACTER::SetPlayerProto(const TPlayerTable* t)
{
    if (!GetDesc() || !*GetDesc()->GetHostName())
        sys_err("cannot get desc or hostname");
    else
        SetGMLevel();

    m_bCharType = CHAR_TYPE_PC;

    m_dwPlayerID = t->id;

    m_iAlignment = t->lAlignment;
    m_iRealAlignment = t->lAlignment;

    m_points.voice = t->voice;

    m_points.skill_group = t->skill_group;

    m_pointsInstant.bBasePart = t->part_base;
    SetPart(PART_HAIR, t->parts[PART_HAIR]);
#ifdef ENABLE_ACCE_SYSTEM
    SetPart(PART_ACCE, t->parts[PART_ACCE]);
#endif

    m_points.iRandomHP = t->sRandomHP;
    m_points.iRandomSP = t->sRandomSP;

    if (m_pSkillLevels) {
        M2_DELETE_ARRAY(m_pSkillLevels);
    }

    m_pSkillLevels = M2_NEW TPlayerSkill[SKILL_MAX_NUM];
    memcpy(m_pSkillLevels, t->skills, sizeof(TPlayerSkill) * SKILL_MAX_NUM);
#ifdef ENABLE_BATTLE_PASS
    m_dwBattlePassEndTime = t->dwBattlePassEndTime;
#endif

    if (t->lMapIndex >= 10000)
    {
        m_posWarp.x = t->lExitX;
        m_posWarp.y = t->lExitY;
        m_lWarpMapIndex = t->lExitMapIndex;
    }

    SetRealPoint(POINT_PLAYTIME, t->playtime);
    m_dwLoginPlayTime = t->playtime;
    SetRealPoint(POINT_ST, t->st);
    SetRealPoint(POINT_HT, t->ht);
    SetRealPoint(POINT_DX, t->dx);
    SetRealPoint(POINT_IQ, t->iq);

    SetPoint(POINT_ST, t->st);
    SetPoint(POINT_HT, t->ht);
    SetPoint(POINT_DX, t->dx);
    SetPoint(POINT_IQ, t->iq);

    SetPoint(POINT_STAT, t->stat_point);
    SetPoint(POINT_SKILL, t->skill_point);
    SetPoint(POINT_SUB_SKILL, t->sub_skill_point);
    SetPoint(POINT_HORSE_SKILL, t->horse_skill_point);

    SetPoint(POINT_STAT_RESET_COUNT, t->stat_reset_count);

    SetPoint(POINT_LEVEL_STEP, t->level_step);
    SetRealPoint(POINT_LEVEL_STEP, t->level_step);

    SetRace(t->job);

    SetLevel(t->level);
    SetExp(t->exp);
    SetGold(t->gold);
#ifdef ENABLE_GAYA_SYSTEM
    SetGaya(t->gaya);
#endif
#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
    Set_Inventory_Point(t->envanter);
#endif

    SetMapIndex(t->lMapIndex);
    SetXYZ(t->x, t->y, t->z);

    ComputePoints();

    SetHP(t->hp);
    SetSP(t->sp);
    SetStamina(t->stamina);

#ifndef ENABLE_GM_FLAG_IF_TEST_SERVER
    if (!test_server)
#endif
    {
#ifdef ENABLE_GM_FLAG_FOR_LOW_WIZARD
        if (GetGMLevel() > GM_PLAYER)
#else
        if (GetGMLevel() > GM_LOW_WIZARD)
#endif
        {
            m_afAffectFlag.Set(AFF_YMIR);
            m_bPKMode = PK_MODE_PROTECT;
        }
    }

    if (GetLevel() < PK_PROTECT_LEVEL)
        m_bPKMode = PK_MODE_PROTECT;

    m_stMobile = t->szMobile;

    SetHorseData(t->horse);

    if (GetHorseLevel() > 0)
        UpdateHorseDataByLogoff(t->logoff_interval);

    memcpy(m_aiPremiumTimes, t->aiPremiumTimes, sizeof(t->aiPremiumTimes));

    m_dwLogOffInterval = t->logoff_interval;

    sys_log(0, "PLAYER_LOAD: %s PREMIUM %d %d, LOGGOFF_INTERVAL %u PTR: %p", t->name, m_aiPremiumTimes[0], m_aiPremiumTimes[1], t->logoff_interval, this);

    if (GetGMLevel() != GM_PLAYER)
    {
        LogManager::instance().CharLog(this, GetGMLevel(), "GM_LOGIN", "");
        sys_log(0, "GM_LOGIN(gmlevel=%d, name=%s(%d), pos=(%d, %d)", GetGMLevel(), GetName(), GetPlayerID(), GetX(), GetY());
    }

#ifdef ENABLE_RANKING
    for (int i = 0; i < RANKING_MAX_CATEGORIES; ++i)
        m_lRankPoints[i] = t->lRankPoints[i];
#endif

#ifdef __PET_SYSTEM__
    if (m_petSystem)
    {
        m_petSystem->Destroy();
        delete m_petSystem;
    }

    m_petSystem = M2_NEW CPetSystem(this);
#endif

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
    if (m_mountSystem)
    {
        m_mountSystem->Destroy();
        delete m_mountSystem;
    }

    m_mountSystem = M2_NEW CMountSystem(this);
#endif

#ifdef __NEWPET_SYSTEM__
    if (m_newpetSystem)
    {
        m_newpetSystem->Destroy();
        delete m_newpetSystem;
    }

    m_newpetSystem = M2_NEW CNewPetSystem(this);
#endif
}

void CHARACTER::SetProto(const CMob* pkMob)
{
    if (m_pkMobInst)
        M2_DELETE(m_pkMobInst);

    m_pkMobData = pkMob;
    m_pkMobInst = M2_NEW CMobInstance;

    m_bPKMode = PK_MODE_FREE;

    const TMobTable* t = &m_pkMobData->m_table;

    m_bCharType = t->bType;

    SetLevel(t->bLevel);
    SetEmpire(t->bEmpire);

    SetExp(t->dwExp);
    SetRealPoint(POINT_ST, t->bStr);
    SetRealPoint(POINT_DX, t->bDex);
    SetRealPoint(POINT_HT, t->bCon);
    SetRealPoint(POINT_IQ, t->bInt);

    ComputePoints();

    SetHP(GetMaxHP());
    SetSP(GetMaxSP());

    m_pointsInstant.dwAIFlag = t->dwAIFlag;
    SetImmuneFlag(t->dwImmuneFlag);

    AssignTriggers(t);

    ApplyMobAttribute(t);

    if (IsStone())
    {
        DetermineDropMetinStone();
    }

    if (IsWarp() || IsGoto())
    {
        StartWarpNPCEvent();
    }

    CHARACTER_MANAGER::instance().RegisterRaceNumMap(this);

    if (mining::IsVeinOfOre(GetRaceNum()))
    {
        char_event_info* info = AllocEventInfo<char_event_info>();

        info->ch = this;

        m_pkMiningEvent = event_create(kill_ore_load_event, info, PASSES_PER_SEC(number(7 * 60, 15 * 60)));
    }
}

bool CHARACTER::StartStateMachine(int iNextPulse)
{
    if (CHARACTER_MANAGER::instance().AddToStateList(this))
    {
        m_dwNextStatePulse = thecore_heart->pulse + iNextPulse;
        return true;
    }

    return false;
}

void CHARACTER::StopStateMachine()
{
    CHARACTER_MANAGER::instance().RemoveFromStateList(this);
}

void CHARACTER::UpdateStateMachine(uint32_t dwPulse)
{
    if (dwPulse < m_dwNextStatePulse)
        return;

    if (IsDead())
        return;

    Update();
    m_dwNextStatePulse = dwPulse + m_dwStateDuration;
}

void CHARACTER::SetNextStatePulse(int iNextPulse)
{
    CHARACTER_MANAGER::instance().AddToStateList(this);
    m_dwNextStatePulse = iNextPulse;

    if (iNextPulse < 10)
        MonsterLog("´UA1»óAÂ·Î3î1­°!AÚ");
}

void CHARACTER::UpdateCharacter(uint32_t dwPulse)
{
    CFSM::Update();
}

void CHARACTER::MonsterLog(const char* format, ...)
{
    if (!test_server)
        return;

    if (IsPC())
        return;

    char chatbuf[CHAT_MAX_LEN + 1];
    int len = snprintf(chatbuf, sizeof(chatbuf), "%lu)", static_cast<unsigned long>(GetVID()));

    if (len < 0 || len >= (int)sizeof(chatbuf))
        len = sizeof(chatbuf) - 1;

    va_list args;

    va_start(args, format);

    int len2 = vsnprintf(chatbuf + len, sizeof(chatbuf) - len, format, args);

    if (len2 < 0 || len2 >= (int)sizeof(chatbuf) - len)
        len += (sizeof(chatbuf) - len) - 1;
    else
        len += len2;

    ++len;

    va_end(args);

    TPacketGCChat pack_chat;

    pack_chat.header = HEADER_GC_CHAT;
    pack_chat.size = sizeof(TPacketGCChat) + len;
    pack_chat.type = CHAT_TYPE_TALKING;
    pack_chat.id = GetVID();
    pack_chat.bEmpire = 0;

    TEMP_BUFFER buf;
    buf.write(&pack_chat, sizeof(TPacketGCChat));
    buf.write(chatbuf, len);

    CHARACTER_MANAGER::instance().PacketMonsterLog(this, buf.read_peek(), buf.size());
}

void CHARACTER::ChatPacket(uint8_t type, const char* format, ...)
{
    LPDESC d = GetDesc();
    if (!d || !format)
        return;

    char chatbuf[CHAT_MAX_LEN + 1];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(chatbuf, sizeof(chatbuf), format, args);
    va_end(args);

    struct packet_chat pack_chat;

    pack_chat.header = HEADER_GC_CHAT;
    pack_chat.size = sizeof(struct packet_chat) + len;
    pack_chat.type = type;
    pack_chat.id = 0;
    pack_chat.bEmpire = d->GetEmpire();

    TEMP_BUFFER buf;
    buf.write(&pack_chat, sizeof(struct packet_chat));
    buf.write(chatbuf, len);

    d->Packet(buf.read_peek(), buf.size());

    if (type == CHAT_TYPE_COMMAND && test_server)
        sys_log(0, "SEND_COMMAND %s %s", GetName(), chatbuf);
}

bool CHARACTER::OnIdle()
{
    return false;
}

void CHARACTER::OnMove(bool bIsAttack)
{
    m_dwLastMoveTime = get_dword_time();

    if (bIsAttack)
    {
        m_dwLastAttackTime = m_dwLastMoveTime;

        if (IsAffectFlag(AFF_REVIVE_INVISIBLE))
            RemoveAffect(AFFECT_REVIVE_INVISIBLE);

        if (IsAffectFlag(AFF_EUNHYUNG))
        {
            RemoveAffect(SKILL_EUNHYUNG);
            SetAffectedEunhyung();
        }
        else
        {
            ClearAffectedEunhyung();
        }

        /*if (IsAffectFlag(AFF_JEONSIN))
          RemoveAffect(SKILL_JEONSINBANGEO);*/
    }

    /*if (IsAffectFlag(AFF_GUNGON))
      RemoveAffect(SKILL_GUNGON);*/

    // MINING
    mining_cancel();
    // END_OF_MINING
}

void CHARACTER::OnClick(LPCHARACTER pkChrCauser)
{
    if (!pkChrCauser)
    {
        sys_err("OnClick %s by NULL", GetName());
        return;
    }

    uint32_t vid = GetVID();
    sys_log(0, "OnClick %s[vnum: %d vid: %d] by %s", GetName(), GetRaceNum(), vid, pkChrCauser->GetName());

    {
        if (pkChrCauser->GetMyShop() && pkChrCauser != this)
        {
            sys_err("OnClick Fail (%s->%s) - pc has shop", pkChrCauser->GetName(), GetName());
            return;
        }
    }

    {
        if (pkChrCauser->GetExchange())
        {
            sys_err("OnClick Fail (%s->%s) - pc is exchanging", pkChrCauser->GetName(), GetName());
            return;
        }
    }

    if (IsPC())
    {
        if (!CTargetManager::instance().GetTargetInfo(pkChrCauser->GetPlayerID(), TARGET_TYPE_VID, GetVID()))
        {
            if (GetMyShop())
            {
                if (pkChrCauser->IsDead() == true)
                    return;

                if (pkChrCauser == this)
                {
                    if ((GetExchange() || IsOpenSafebox() || GetShopOwner()) || IsCubeOpen())
                    {
#ifdef TEXTS_IMPROVEMENT
                        pkChrCauser->ChatPacketNew(CHAT_TYPE_INFO, 291, "");
#endif
                        return;
                    }

#ifdef __ATTR_TRANSFER_SYSTEM__
                    if (IsAttrTransferOpen())
                    {
#ifdef TEXTS_IMPROVEMENT
                        pkChrCauser->ChatPacketNew(CHAT_TYPE_INFO, 291, "");
#endif
                        return;
                    }
#endif
                }
                else
                {
                    if ((pkChrCauser->GetExchange() || pkChrCauser->IsOpenSafebox() || pkChrCauser->GetMyShop() || pkChrCauser->GetShopOwner()) || pkChrCauser->IsCubeOpen())
                    {
#ifdef TEXTS_IMPROVEMENT
                        pkChrCauser->ChatPacketNew(CHAT_TYPE_INFO, 291, "");
#endif
                        return;
                    }

#ifdef __ATTR_TRANSFER_SYSTEM__
                    if (pkChrCauser->IsAttrTransferOpen())
                    {
#ifdef TEXTS_IMPROVEMENT
                        pkChrCauser->ChatPacketNew(CHAT_TYPE_INFO, 291, "");
#endif
                        return;
                    }
#endif

                    if ((GetExchange() || IsOpenSafebox() || IsCubeOpen()))
                    {
#ifdef TEXTS_IMPROVEMENT
                        pkChrCauser->ChatPacketNew(CHAT_TYPE_INFO, 369, "%s", GetName());
#endif
                        return;
                    }

#ifdef __ATTR_TRANSFER_SYSTEM__
                    if (IsAttrTransferOpen())
                    {
#ifdef TEXTS_IMPROVEMENT
                        pkChrCauser->ChatPacketNew(CHAT_TYPE_INFO, 369, "%s", GetName());
#endif
                        return;
                    }
#endif
                }

                if (pkChrCauser->GetShop())
                {
                    pkChrCauser->GetShop()->RemoveGuest(pkChrCauser);
                    pkChrCauser->SetShop(nullptr);
                }

                GetMyShop()->AddGuest(pkChrCauser, GetVID(), false);
                pkChrCauser->SetShopOwner(this);
                return;
            }

            if (test_server)
                sys_err("%s.OnClickFailure(%s) - target is PC", pkChrCauser->GetName(), GetName());

            return;
        }
    }

    pkChrCauser->SetQuestNPCID(GetVID());

    if (quest::CQuestManager::instance().Click(pkChrCauser->GetPlayerID(), this))
    {
        return;
    }

    if (!IsPC())
    {
        if (!m_triggerOnClick.pFunc)
        {
            return;
        }

        m_triggerOnClick.pFunc(this, pkChrCauser);
    }
}
