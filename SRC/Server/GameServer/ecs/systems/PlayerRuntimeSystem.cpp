#include "../../stdafx.h"

#include "PlayerRuntimeSystem.hpp"

#include "../../char.h"
#include "../../char_manager.h"
#include "../../config.h"
#include "../../constants.h"
#include "../../desc.h"
#include "../../buffer_manager.h"
#include "../../battle_pass.h"
#include "../../banword.h"
#include "../../crc32.h"
#include "../../db.h"
#include "../../desc_client.h"
#include "../../dungeon.h"
#include "../../ecs/EntityFactory.hpp"
#include "../../ecs/AIHelpers.hpp"
#include "../../ecs/Registry.hpp"
#include "../../ecs/components/combat_components.hpp"
#include "../../ecs/components/dirty_components.hpp"
#include "../../ecs/components/identity_components.hpp"
#include "../../ecs/components/movement_components.hpp"
#include "../../exchange.h"
#include "../../gm.h"
#include "../../guild_manager.h"
#include "../../item.h"
#include "../../item_manager.h"
#include "../../log.h"
#include "../../marriage.h"
#include "../../mining.h"
#include "../../mob_manager.h"
#include "../../MountSystem.h"
#include "../../MountInventory.h"
#include "../../new_offlineshop.h"
#include "../../New_PetSystem.h"
#include "../../PetSystem.h"
#include "../../party.h"
#include "../../questmanager.h"
#include "../../regen.h"
#include "../../safebox.h"
#include "../../shop.h"
#include "../../shop_manager.h"
#include "../../start_position.h"
#include "../../skill_power.h"
#include "../../target.h"
#include "../../war_map.h"
#include "../../wedding.h"
#include "../../DragonSoul.h"
#include "../../../common/rune_length.h"
#include "../../../common/stole_length.h"
#ifdef ENABLE_ANTICHEAT
#include "../../hwidmanager.h"
#endif

extern bool RaceToJob(unsigned race, unsigned* ret_job);
EVENTFUNC(drop_event);
EVENTFUNC(destroy_when_idle_event);
EVENTFUNC(kill_ore_load_event);

namespace
{
inline entt::entity EcsEntityOf(const CHARACTER* ch)
{
    if (!ch)
        return entt::null;

    return CVIDRegistry::Instance().Find(ch->GetVID());
}

inline bool HasCombatState(const CHARACTER* ch)
{
    const entt::entity e = EcsEntityOf(ch);
    return e != entt::null && g_registry.valid(e) &&
        g_registry.all_of<ecs::CombatActiveTag>(e);
}

inline bool HasIdleState(const CHARACTER* ch)
{
    const entt::entity e = EcsEntityOf(ch);
    if (e == entt::null || !g_registry.valid(e))
        return true;

    return !g_registry.all_of<ecs::CombatActiveTag>(e) &&
        !g_registry.all_of<ecs::MovementDestination>(e);
}

inline void EnterIdleState(CHARACTER* ch)
{
    const entt::entity e = EcsEntityOf(ch);
    if (e == entt::null || !g_registry.valid(e))
        return;

    g_registry.remove<ecs::CombatActiveTag>(e);
    g_registry.remove<ecs::CombatTarget>(e);
    g_registry.remove<ecs::MovementDestination>(e);
}

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

void CHARACTER::Destroy()
{
    {
        entt::entity e = CVIDRegistry::Instance().Find(GetVID());
        if (e != entt::null)
            EntityFactory::Destroy(g_registry, e);
    }

    CloseMyShop();

    if (m_pkRegen)
    {
        if (m_pkDungeon) {
            if (m_pkDungeon->IsValidRegen(m_pkRegen, regen_id_)) {
                --m_pkRegen->count;
            }
        }
        else {
            --m_pkRegen->count;
        }
        m_pkRegen = nullptr;
    }

    if (m_pkDungeon)
    {
        SetDungeon(nullptr);
    }

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
    if (m_mountSystem)
    {
        m_mountSystem->Destroy();
        delete m_mountSystem;

        m_mountSystem = nullptr;
    }

    if (GetMountVnum())
    {
        RemoveAffect(AFFECT_MOUNT);
        RemoveAffect(AFFECT_MOUNT_BONUS);
    }
    HorseSummon(false);
#endif
#ifdef __PET_SYSTEM__
    if (m_petSystem)
    {
        m_petSystem->Destroy();
        delete m_petSystem;

        m_petSystem = nullptr;
    }
#endif

#ifdef __NEWPET_SYSTEM__
    if (m_newpetSystem)
    {
        m_newpetSystem->Destroy();
        delete m_newpetSystem;

        m_newpetSystem = nullptr;
    }
#endif

    HorseSummon(false);

    if (GetRider())
        GetRider()->ClearHorseInfo();

    if (GetDesc())
    {
        GetDesc()->BindCharacter(nullptr);
    }

    if (m_pkExchange)
        m_pkExchange->Cancel();

    SetVictim(nullptr);

    if (GetShop())
    {
        GetShop()->RemoveGuest(this);
        SetShop(nullptr);
    }

    ClearStone();
    ClearSync();
    ClearTarget();

    if (nullptr == m_pkMobData)
    {
        DragonSoul_CleanUp();
        ClearItem();
    }

    LPPARTY party = m_pkParty;
    if (party)
    {
        if (party->GetLeaderPID() == GetVID() && !IsPC())
        {
            M2_DELETE(party);
        }
        else
        {
            party->Unlink(this);

            if (!IsPC())
                party->Quit(GetVID());
        }

        SetParty(nullptr);
    }

    if (m_pkMobInst)
    {
        M2_DELETE(m_pkMobInst);
        m_pkMobInst = nullptr;
    }

    m_pkMobData = nullptr;

    if (m_pkSafebox)
    {
        M2_DELETE(m_pkSafebox);
        m_pkSafebox = nullptr;
    }

    if (m_pkMall)
    {
        M2_DELETE(m_pkMall);
        m_pkMall = nullptr;
    }

    for (TMapBuffOnAttrs::iterator it = m_map_buff_on_attrs.begin(); it != m_map_buff_on_attrs.end(); it++)
    {
        if (nullptr != it->second)
        {
            M2_DELETE(it->second);
        }
    }
    m_map_buff_on_attrs.clear();

    m_set_pkChrSpawnedBy.clear();

    StopMuyeongEvent();
#ifdef ENABLE_NEW_GYEONGGONG_SKILL
    StopGyeongGongEvent();
#endif
    event_cancel(&m_pkWarpNPCEvent);
    event_cancel(&m_pkRecoveryEvent);
    event_cancel(&m_pkDeadEvent);
    event_cancel(&m_pkSaveEvent);
    event_cancel(&m_pkTimedEvent);
    event_cancel(&m_pkStunEvent);
    event_cancel(&m_pkFishingEvent);
    event_cancel(&m_pkPoisonEvent);
#ifdef ENABLE_WOLFMAN_CHARACTER
    event_cancel(&m_pkBleedingEvent);
#endif
    event_cancel(&m_pkFireEvent);
    event_cancel(&m_pkPartyRequestEvent);
    event_cancel(&m_pkWarpEvent);
#ifdef ENABLE_NEW_FISHING_SYSTEM
    event_cancel(&m_pkFishingNewEvent);
#endif
#ifdef ENABLE_BATTLE_PASS_STAY_ONLINE
    if (m_pkBattlePassStayOnlineEvent)
    {
        event_cancel(&m_pkBattlePassStayOnlineEvent);
        m_pkBattlePassStayOnlineEvent = nullptr;
    }
#endif

    event_cancel(&m_pkMiningEvent);
#ifdef ENABLE_BLOCK_MULTIFARM
    if (m_pkDropEvent) {
        event_cancel(&m_pkDropEvent);
        m_pkDropEvent = nullptr;
    }
#endif
#ifdef ENABLE_BATTLE_PASS_STAY_ONLINE
    event_cancel(&m_pkStayOnlineEvent);
#endif

    for (auto it = m_mapMobSkillEvent.begin(); it != m_mapMobSkillEvent.end(); ++it)
    {
        LPEVENT pkEvent = it->second;
        event_cancel(&pkEvent);
    }
    m_mapMobSkillEvent.clear();
#ifdef __DUNGEON_INFO_SYSTEM__
    dungeonDamage.clear();
#endif
    ClearAffect();

    event_cancel(&m_pkDestroyWhenIdleEvent);

    if (m_pSkillLevels)
    {
        M2_DELETE_ARRAY(m_pSkillLevels);
        m_pSkillLevels = nullptr;
    }

    if (m_pkMountInventory)
    {
        M2_DELETE(m_pkMountInventory);
        m_pkMountInventory = nullptr;
    }
    m_bMountInventoryLoaded = false;

    CEntity::Destroy();

    if (GetSectree())
        GetSectree()->RemoveEntity(this);

    if (m_bMonsterLog)
        CHARACTER_MANAGER::instance().UnregisterForMonsterLog(this);
}

void CHARACTER::ResetPoint(int iLv)
{
    uint8_t bJob = GetJob();

    PointChange(POINT_LEVEL, iLv - GetLevel());

    SetRealPoint(POINT_ST, JobInitialPoints[bJob].st);
    SetPoint(POINT_ST, GetRealPoint(POINT_ST));

    SetRealPoint(POINT_HT, JobInitialPoints[bJob].ht);
    SetPoint(POINT_HT, GetRealPoint(POINT_HT));

    SetRealPoint(POINT_DX, JobInitialPoints[bJob].dx);
    SetPoint(POINT_DX, GetRealPoint(POINT_DX));

    SetRealPoint(POINT_IQ, JobInitialPoints[bJob].iq);
    SetPoint(POINT_IQ, GetRealPoint(POINT_IQ));

    SetRandomHP((iLv - 1) * number(JobInitialPoints[GetJob()].hp_per_lv_begin, JobInitialPoints[GetJob()].hp_per_lv_end));
    SetRandomSP((iLv - 1) * number(JobInitialPoints[GetJob()].sp_per_lv_begin, JobInitialPoints[GetJob()].sp_per_lv_end));

    int iLvl = iLv;
#ifdef ENABLE_STATUS_MAX_344_POINTS
    if (iLvl > 0)
        iLvl -= 1;
#endif
    PointChange(POINT_STAT, (MINMAX(1, iLvl, g_iStatusPointGetLevelLimit) * 3) + GetPoint(POINT_LEVEL_STEP) - GetPoint(POINT_STAT));

    ComputePoints();

    PointChange(POINT_HP, GetMaxHP() - GetHP());
    PointChange(POINT_SP, GetMaxSP() - GetSP());

    PointsPacket();

    LogManager::instance().CharLog(this, 0, "RESET_POINT", "");
}

void CHARACTER::GiveRandomSkillBook()
{
    LPITEM item = AutoGiveItem(50300);

    if (nullptr != item)
    {
        extern const uint32_t GetRandomSkillVnum(uint8_t bJob = JOB_MAX_NUM);
        uint32_t dwSkillVnum = 0;
        if (!number(0, 1))
            dwSkillVnum = GetRandomSkillVnum(GetJob());
        else
            dwSkillVnum = GetRandomSkillVnum();
        item->SetSocket(0, dwSkillVnum);
    }
}

void CHARACTER::ToggleMonsterLog()
{
    m_bMonsterLog = !m_bMonsterLog;

    if (m_bMonsterLog)
    {
        CHARACTER_MANAGER::instance().RegisterForMonsterLog(this);
    }
    else
    {
        CHARACTER_MANAGER::instance().UnregisterForMonsterLog(this);
    }
}

void CHARACTER::SendGreetMessage()
{
    auto v = DBManager::instance().GetGreetMessage();

    for (auto it = v.begin(); it != v.end(); ++it)
    {
        ChatPacket(CHAT_TYPE_NOTICE, it->c_str());
    }
}

void CHARACTER::BeginStateEmpty()
{
    MonsterLog("!");
}

int CHARACTER::ChangeEmpire(uint8_t empire)
{
    if (GetEmpire() == empire)
        return 1;

    char szQuery[1024 + 1];
    uint32_t dwAID;
    uint32_t dwPID[4];
    memset(dwPID, 0, sizeof(dwPID));

    snprintf(szQuery, sizeof(szQuery),
        "SELECT id, pid1, pid2, pid3, pid4, pid5 FROM player_index%s WHERE pid1=%u OR pid2=%u OR pid3=%u OR pid4=%u OR pid5=%u AND empire=%u",
        get_table_postfix(), GetPlayerID(), GetPlayerID(), GetPlayerID(), GetPlayerID(), GetPlayerID(), GetEmpire());

    std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery(szQuery));
    if (msg->Get()->uiNumRows == 0)
        return 0;

    MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult);
    str_to_number(dwAID, row[0]);
    str_to_number(dwPID[0], row[1]);
    str_to_number(dwPID[1], row[2]);
    str_to_number(dwPID[2], row[3]);
    str_to_number(dwPID[3], row[4]);

    for (int i = 0; i < 4; ++i)
    {
        snprintf(szQuery, sizeof(szQuery), "SELECT guild_id FROM guild_member%s WHERE pid=%u", get_table_postfix(), dwPID[i]);
        std::unique_ptr<SQLMsg> guildMsg(DBManager::instance().DirectQuery(szQuery));
        if (guildMsg->Get()->uiNumRows > 0)
        {
            uint32_t dwGuildID = 0;
            MYSQL_ROW guildRow = mysql_fetch_row(guildMsg->Get()->pSQLResult);
            str_to_number(dwGuildID, guildRow[0]);
            if (CGuildManager::instance().FindGuild(dwGuildID) != nullptr)
                return 2;
        }
    }

    for (int i = 0; i < 4; ++i)
    {
        if (marriage::CManager::instance().IsEngagedOrMarried(dwPID[i]) == true)
            return 3;
    }

    snprintf(szQuery, sizeof(szQuery), "UPDATE player_index%s SET empire=%u WHERE pid1=%u OR pid2=%u OR pid3=%u OR pid4=%u OR pid5=%u AND empire=%u",
        get_table_postfix(), empire, GetPlayerID(), GetPlayerID(), GetPlayerID(), GetPlayerID(), GetPlayerID(), GetEmpire());

    std::unique_ptr<SQLMsg> updateMsg(DBManager::instance().DirectQuery(szQuery));
    if (updateMsg->Get()->uiAffectedRows <= 0)
        return 0;

    const entt::entity e = AIHelpers::EcsOf(this);
    if (e != entt::null && g_registry.valid(e))
    {
        auto& emp = g_registry.get_or_emplace<ecs::EmpireComponent>(e);
        emp.value = empire;
        ++emp.changeCount;
        g_registry.emplace_or_replace<ecs::DirtyTag>(e);
    }

    SetChangeEmpireCount();
#ifdef ENABLE_BUG_FIXES
    SetEmpire(empire);
    UpdatePacket();
#endif
    return 999;
}

int CHARACTER::GetChangeEmpireCount() const
{
    char szQuery[1024 + 1];
    uint32_t dwAID = GetAID();

    if (dwAID == 0)
        return 0;

    snprintf(szQuery, sizeof(szQuery), "SELECT change_count FROM change_empire WHERE account_id = %u", dwAID);
    std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery(szQuery));
    if (msg->Get()->uiNumRows == 0)
        return 0;

    MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult);
    uint32_t count = 0;
    str_to_number(count, row[0]);
    return count;
}

void CHARACTER::SetChangeEmpireCount()
{
    char szQuery[1024 + 1];
    uint32_t dwAID = GetAID();

    if (dwAID == 0)
        return;

    int count = GetChangeEmpireCount();
    const entt::entity e = AIHelpers::EcsOf(this);
    if (e != entt::null && g_registry.valid(e))
    {
        auto& emp = g_registry.get_or_emplace<ecs::EmpireComponent>(e);
        emp.changeCount = static_cast<uint32_t>(count + 1);
        g_registry.emplace_or_replace<ecs::DirtyTag>(e);
    }

    if (count == 0)
    {
        ++count;
        snprintf(szQuery, sizeof(szQuery), "INSERT INTO change_empire VALUES(%u, %d, NOW())", dwAID, count);
    }
    else
    {
        ++count;
        snprintf(szQuery, sizeof(szQuery), "UPDATE change_empire SET change_count=%d WHERE account_id=%u", count, dwAID);
    }

    std::unique_ptr<SQLMsg> pmsg(DBManager::instance().DirectQuery(szQuery));
}

void CHARACTER::MountVnum(uint32_t vnum)
{
    if (m_dwMountVnum == vnum)
        return;
    if ((m_dwMountVnum != 0) && (vnum != 0))
        MountVnum(0);

    m_dwMountVnum = vnum;
    m_dwMountTime = get_dword_time();

    if (m_bIsObserver)
        return;

    m_posDest.x = m_posStart.x = GetX();
    m_posDest.y = m_posStart.y = GetY();
    EncodeInsertPacket(this);

    ENTITY_MAP::iterator it = m_map_view.begin();

    while (it != m_map_view.end())
    {
        LPENTITY entity = (it++)->first;
        EncodeInsertPacket(entity);
    }

    SetValidComboInterval(0);
    SetComboSequence(0);

    ComputePoints();
}

int64_t CHARACTER::ComputeRefineFee(int64_t iCost, int64_t iMultiply) const
{
    CGuild* pGuild = GetRefineGuild();
    if (pGuild)
    {
        if (pGuild == GetGuild())
            return iCost * iMultiply * 9 / 10;

        LPCHARACTER chRefineNPC = CHARACTER_MANAGER::instance().Find(m_dwRefineNPCVID);
        if (chRefineNPC && chRefineNPC->GetEmpire() != GetEmpire())
            return iCost * iMultiply * 3;

        return iCost * iMultiply;
    }
    else
        return iCost;
}

void CHARACTER::PayRefineFee(int64_t iTotalMoney)
{
    int64_t iFee = iTotalMoney / 10;
    CGuild* pGuild = GetRefineGuild();

    int64_t iRemain = iTotalMoney;

    if (pGuild)
    {
        if (pGuild != GetGuild())
        {
            pGuild->RequestDepositMoney(this, iFee);
            iRemain -= iFee;
        }
    }

    PointChange(POINT_GOLD, -iRemain);
}

void CHARACTER::StartDestroyWhenIdleEvent()
{
    if (m_pkDestroyWhenIdleEvent)
        return;

    char_event_info* info = AllocEventInfo<char_event_info>();

    info->ch = this;

    m_pkDestroyWhenIdleEvent = event_create(destroy_when_idle_event, info, PASSES_PER_SEC(300));
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

void CHARACTER::DestroyPvP()
{
    if (GetDesc() != nullptr)
    {
        const char* szTableStaticPvP[] = { BLOCK_CHANGEITEM, BLOCK_BUFF, BLOCK_POTION, BLOCK_RIDE, BLOCK_PET, BLOCK_POLY, BLOCK_PARTY, BLOCK_EXCHANGE_, BET_WINNER, CHECK_IS_FIGHT };

        int moneyBet = GetQuestFlag(szTableStaticPvP[8]);
        int isDuel = GetQuestFlag(szTableStaticPvP[9]);

        if (isDuel != 0)
        {
            if (moneyBet > 0)
            {
                PointChange(POINT_GOLD, moneyBet, true);
            }

            char szBuf[CHAT_MAX_LEN + 1];
            snprintf(szBuf, sizeof(szBuf), "BINARY_Duel_Delete");
            ChatPacket(CHAT_TYPE_COMMAND, szBuf);

            for (size_t i = 0; i < _countof(szTableStaticPvP); i++)
            {
                SetQuestFlag(szTableStaticPvP[i], 0);
            }
        }
    }
}

void CHARACTER::RestartAtSamePos()
{
    if (m_bIsObserver)
        return;

    EncodeRemovePacket(this);
    EncodeInsertPacket(this);

    ENTITY_MAP::iterator it = m_map_view.begin();

    while (it != m_map_view.end())
    {
        LPENTITY entity = (it++)->first;

        EncodeRemovePacket(entity);
        if (!m_bIsObserver)
            EncodeInsertPacket(entity);

        if (entity->IsType(ENTITY_CHARACTER))
        {
            LPCHARACTER lpChar = (LPCHARACTER)entity;
            if (lpChar->IsPC() || lpChar->IsNPC() || lpChar->IsMonster())
            {
                if (!entity->IsObserverMode())
                    entity->EncodeInsertPacket(this);
            }
        }
        else
        {
            if (!entity->IsObserverMode())
            {
                entity->EncodeInsertPacket(this);
            }
        }
    }
}

#ifdef ENABLE_CHANNEL_SWITCH_SYSTEM
bool CHARACTER::SwitchChannel(int32_t newAddr, uint16_t newPort)
{
    if (!IsPC() || !GetDesc() || !CanWarp())
        return false;

    int32_t x = GetX();
    int32_t y = GetY();

    int32_t lAddr = newAddr;
    int32_t lMapIndex = GetMapIndex();
    uint16_t wPort = newPort;

    if (lMapIndex >= 10000)
    {
        sys_err("Invalid change channel request from dungeon %d!", lMapIndex);
        return false;
    }

    if (g_bChannel == 99)
    {
        sys_err("%s attempted to change channel from CH99, ignoring req.", GetName());
        return false;
    }

    Stop();
    Save();

    if (GetSectree())
    {
        GetSectree()->RemoveEntity(this);
        ViewCleanup();
        EncodeRemovePacket(this);
    }

    m_lWarpMapIndex = lMapIndex;
    m_posWarp.x = x;
    m_posWarp.y = y;

    sys_log(0, "ChangeChannel %s, %ld %ld map %ld to port %d", GetName(), x, y, GetMapIndex(), wPort);

    TPacketGCWarp p;

    p.bHeader = HEADER_GC_WARP;
    p.lX = x;
    p.lY = y;
    p.lAddr = lAddr;
    p.wPort = wPort;

    GetDesc()->Packet(&p, sizeof(p));

    char buf[256];
    snprintf(buf, sizeof(buf), "%s Port%d Map%ld x%ld y%ld", GetName(), wPort, GetMapIndex(), x, y);
    LogManager::instance().CharLog(this, 0, "CHANGE_CH", buf);

    return true;
}

EVENTINFO(switch_channel_info)
{
    DynamicCharacterPtr ch;
    int secs;
    int32_t newAddr;
    uint16_t newPort;
    switch_channel_info()
        : ch(),
        secs(0),
        newAddr(0),
        newPort(0)
    {
    }
};

EVENTFUNC(switch_channel)
{
    switch_channel_info* info = dynamic_cast<switch_channel_info*>(event->info);
    if (!info)
    {
        sys_err("No switch channel event info!");
        return 0;
    }

    LPCHARACTER ch = info->ch;
    if (!ch)
    {
        sys_err("No char to work on for the switch.");
        return 0;
    }

    if (!ch->GetDesc())
        return 0;

    if (info->secs > 0)
    {
#ifdef TEXTS_IMPROVEMENT
        ch->ChatPacketNew(CHAT_TYPE_INFO, 658, "%d", info->secs);
#endif
        --info->secs;
        return PASSES_PER_SEC(1);
    }

    ch->SwitchChannel(info->newAddr, info->newPort);
    ch->m_pkTimedEvent = nullptr;
    return 0;
}

bool CHARACTER::StartChannelSwitch(int32_t newAddr, uint16_t newPort)
{
    if (IsHack(false, true, 10))
        return false;

    switch_channel_info* info = AllocEventInfo<switch_channel_info>();
    info->ch = this;
    info->secs = CanWarp() && !IsPosition(POS_FIGHTING) ? 3 : 10;
    info->newAddr = newAddr;
    info->newPort = newPort;

    m_pkTimedEvent = event_create(switch_channel, info, 1);
    return true;
}
#endif

#ifdef ENABLE_BLOCK_MULTIFARM
void CHARACTER::BlockProcessed()
{
    if (!m_pkDropEvent) {
        sys_err("<drop_event> process failed, event is null.");
    }
    else {
#ifdef TEXTS_IMPROVEMENT
        ChatPacketNew(CHAT_TYPE_INFO, 42, "");
#endif
        event_cancel(&m_pkDropEvent);
        m_pkDropEvent = nullptr;
        sys_log(0, "<drop_event> processed.");
    }
}

void CHARACTER::BlockDrop()
{
    if (!IsPC()) {
        return;
    }

    if (GetMapIndex() != 358 && GetMapIndex() != 359 && GetMapIndex() != 360 && GetMapIndex() != 361) {
#ifdef TEXTS_IMPROVEMENT
        ChatPacketNew(CHAT_TYPE_INFO, 36, "");
#endif
        return;
    }

    if (m_pkDropEvent) {
#ifdef TEXTS_IMPROVEMENT
        ChatPacketNew(CHAT_TYPE_INFO, 44, "");
#endif
        return;
    }

    drop_event_info* info = AllocEventInfo<drop_event_info>();
    info->ch = this;
    info->time = get_global_time() + 5;
    info->drop = false;
    m_pkDropEvent = event_create(drop_event, info, PASSES_PER_SEC(1));
#ifdef TEXTS_IMPROVEMENT
    ChatPacketNew(CHAT_TYPE_INFO, 43, "%d", 5);
#endif
}

void CHARACTER::UnblockDrop()
{
    if (GetMapIndex() != 358 && GetMapIndex() != 359 && GetMapIndex() != 360 && GetMapIndex() != 361) {
#ifdef TEXTS_IMPROVEMENT
        ChatPacketNew(CHAT_TYPE_INFO, 36, "");
#endif
        return;
    }

    if (m_pkDropEvent) {
#ifdef TEXTS_IMPROVEMENT
        ChatPacketNew(CHAT_TYPE_INFO, 44, "");
#endif
        return;
    }

    drop_event_info* info = AllocEventInfo<drop_event_info>();
    info->ch = this;
    info->time = get_global_time() + 5;
    info->drop = true;
    m_pkDropEvent = event_create(drop_event, info, PASSES_PER_SEC(1));
#ifdef TEXTS_IMPROVEMENT
    ChatPacketNew(CHAT_TYPE_INFO, 43, "%d", 5);
#endif
}

void CHARACTER::SetDropStatus()
{
    if (!IsPC())
        return;

    std::string login = GetDesc()->GetAccountTable().login;
    std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("SELECT status FROM account.antifarm WHERE login='%s'", login.c_str()));
    if (msg->Get()->uiNumRows != 0) {
        MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult);
        int32_t r = atoi(row[0]);
        if (r == 1) {
            RemoveAffect(AFFECT_DROP_BLOCK);
            AddAffect(AFFECT_DROP_UNBLOCK, APPLY_NONE, 0, 0, 31536000, 0, true, false);
        }
        else {
            RemoveAffect(AFFECT_DROP_UNBLOCK);
            AddAffect(AFFECT_DROP_BLOCK, APPLY_NONE, 0, 0, 31536000, 0, true, false);
        }
    }
}
#endif

void CHARACTER::OpenMyShop(const char* c_pszSign, TShopItemTable* pTable, uint8_t bItemCount
#ifdef KASMIR_PAKET_SYSTEM
    , uint32_t KasmirNpc, uint8_t KasmirBaslik
#endif
)
{
    if (!CanHandleItem())
    {
#ifdef TEXTS_IMPROVEMENT
        ChatPacketNew(CHAT_TYPE_INFO, 291, "");
#endif
        return;
    }

#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
    if (GetGMLevel() > GM_PLAYER && GetGMLevel() < GM_IMPLEMENTOR) {
        return;
    }
#endif

#ifndef ENABLE_OPEN_SHOP_WITH_ARMOR
    if (GetPart(PART_MAIN) > 2)
    {
#ifdef TEXTS_IMPROVEMENT
        ChatPacketNew(CHAT_TYPE_INFO, 503, "");
#endif
        return;
    }
#endif

    if (GetMyShop())
    {
        CloseMyShop();
        return;
    }

    quest::PC* pPC = quest::CQuestManager::instance().GetPCForce(GetPlayerID());
    if (pPC->IsRunning())
        return;

    if (bItemCount == 0)
        return;

    int64_t nTotalMoney = 0;

    for (int n = 0; n < bItemCount; ++n)
    {
        nTotalMoney += static_cast<int64_t>((pTable + n)->price);
    }

    nTotalMoney += static_cast<int64_t>(GetGold());

    if (GOLD_MAX <= nTotalMoney)
    {
#ifdef TEXTS_IMPROVEMENT
        ChatPacketNew(CHAT_TYPE_INFO, 226,
            "%lld"

            , GOLD_MAX);
#endif
        return;
    }

    char szSign[SHOP_SIGN_MAX_LEN + 1];
    strlcpy(szSign, c_pszSign, sizeof(szSign));

    m_stShopSign = szSign;

    if (m_stShopSign.length() == 0)
        return;

    if (CBanwordManager::instance().CheckString(m_stShopSign.c_str(), m_stShopSign.length()))
    {
#ifdef TEXTS_IMPROVEMENT
        ChatPacketNew(CHAT_TYPE_INFO, 358, "");
#endif
        return;
    }

#ifdef KASMIR_PAKET_SYSTEM
    m_bKasmirPaketBaslik = KasmirBaslik;
    if (m_bKasmirPaketBaslik < 1 && m_bKasmirPaketBaslik > 6)
    {
#ifdef TEXTS_IMPROVEMENT
        ChatPacketNew(CHAT_TYPE_INFO, 46, "");
#endif
        return;
    }
#endif

    std::map<uint32_t, uint32_t> itemkind;

    std::set<TItemPos> cont;
    for (uint8_t i = 0; i < bItemCount; ++i)
    {
        if (cont.contains((pTable + i)->pos))
        {
            sys_err("MYSHOP: duplicate shop item detected! (name: %s)", GetName());
            return;
        }

        LPITEM pkItem = GetItem((pTable + i)->pos);

        if (pkItem)
        {
            const TItemTable* item_table = pkItem->GetProto();

            if (item_table && (IS_SET(item_table->dwAntiFlags, ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_MYSHOP)))
            {
#ifdef TEXTS_IMPROVEMENT
                ChatPacketNew(CHAT_TYPE_INFO, 416, "%s", pkItem->GetName());
#endif
                return;
            }

            if (pkItem->IsEquipped() == true)
            {
#ifdef TEXTS_IMPROVEMENT
                ChatPacketNew(CHAT_TYPE_INFO, 541, "");
#endif
                return;
            }

            if (true == pkItem->isLocked())
            {
#ifdef TEXTS_IMPROVEMENT
                ChatPacketNew(CHAT_TYPE_INFO, 656, "");
#endif
                return;
            }

            itemkind[pkItem->GetVnum()] = (pTable + i)->price / pkItem->GetCount();
        }

        cont.insert((pTable + i)->pos);
    }

    if (CountSpecifyItem(71049)
#ifdef KASMIR_PAKET_SYSTEM
        || CountSpecifyItem(88901)
#endif
        ) {
        TItemPriceListTable header;
        memset(&header, 0, sizeof(TItemPriceListTable));

        header.dwOwnerID = GetPlayerID();
        header.byCount = itemkind.size();

        size_t idx = 0;
        for (auto it = itemkind.begin(); it != itemkind.end(); ++it)
        {
            header.aPriceInfo[idx].dwVnum = it->first;
            header.aPriceInfo[idx].dwPrice = it->second;
            idx++;
        }

        db_clientdesc->DBPacket(HEADER_GD_MYSHOP_PRICELIST_UPDATE, GetDesc()->GetHandle(), &header, sizeof(TItemPriceListTable));
    }
    else if (CountSpecifyItem(50200))
        RemoveSpecifyItem(50200, 1);
    else
        return;

    if (m_pkExchange)
        m_pkExchange->Cancel();

    TPacketGCShopSign p;

    p.bHeader = HEADER_GC_SHOP_SIGN;
    p.dwVID = GetVID();
    strlcpy(p.szSign, c_pszSign, sizeof(p.szSign));
#ifdef KASMIR_PAKET_SYSTEM
    p.bShopKasmirTitle = KasmirBaslik;
#endif
    PacketAround(&p, sizeof(TPacketGCShopSign));

    m_pkMyShop = CShopManager::instance().CreatePCShop(this, pTable, bItemCount);

    if (IsPolymorphed() == true)
    {
        RemoveAffect(AFFECT_POLYMORPH);
    }

    if (GetHorse())
    {
        HorseSummon(false, true);
    }
    else if (GetMountVnum())
    {
        RemoveAffect(AFFECT_MOUNT);
        RemoveAffect(AFFECT_MOUNT_BONUS);
    }

    uint32_t dwNpcShop = 30000;
#ifdef KASMIR_PAKET_SYSTEM
    dwNpcShop = KasmirNpc >= 30000 && KasmirNpc <= 30007 ? KasmirNpc : 30000;
#endif
    SetPolymorph(dwNpcShop, true);
}

void CHARACTER::CloseMyShop()
{
    if (GetMyShop())
    {
        m_stShopSign.clear();
        CShopManager::instance().DestroyPCShop(this);
        m_pkMyShop = nullptr;
#ifdef KASMIR_PAKET_SYSTEM
        m_bKasmirPaketBaslik = 0;
        m_bKasmirPaketDurum = false;
#endif

        TPacketGCShopSign p;

        p.bHeader = HEADER_GC_SHOP_SIGN;
        p.dwVID = GetVID();
#ifdef KASMIR_PAKET_SYSTEM
        p.bShopKasmirTitle = m_bKasmirPaketBaslik;
#endif
        p.szSign[0] = '\0';

        PacketAround(&p, sizeof(p));
#ifdef ENABLE_WOLFMAN_CHARACTER
        SetPolymorph(m_points.job, true);
#else
        SetPolymorph(GetJob(), true);
#endif
    }
}

#ifdef __HIDE_COSTUME_SYSTEM__
void CHARACTER::SetBodyCostumeHidden(bool hidden, bool pass)
{
    m_bHideBodyCostume = hidden;
    ChatPacket(CHAT_TYPE_COMMAND, "SetBodyCostumeHidden %d", m_bHideBodyCostume ? 1 : 0);
    if (!pass) {
        SetQuestFlag("costume_option.hide_body", m_bHideBodyCostume ? 1 : 0);
    }
}

void CHARACTER::SetHairCostumeHidden(bool hidden, bool pass)
{
    m_bHideHairCostume = hidden;
    ChatPacket(CHAT_TYPE_COMMAND, "SetHairCostumeHidden %d", m_bHideHairCostume ? 1 : 0);
    if (!pass) {
        SetQuestFlag("costume_option.hide_hair", m_bHideHairCostume ? 1 : 0);
    }
}

#ifdef ENABLE_ACCE_SYSTEM
void CHARACTER::SetAcceCostumeHidden(bool hidden, bool pass)
{
    m_bHideAcceCostume = hidden;
    ChatPacket(CHAT_TYPE_COMMAND, "SetAcceCostumeHidden %d", m_bHideAcceCostume ? 1 : 0);
    if (!pass) {
        SetQuestFlag("costume_option.hide_acce", m_bHideAcceCostume ? 1 : 0);
    }
}
#endif

#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
void CHARACTER::SetWeaponCostumeHidden(bool hidden, bool pass)
{
    m_bHideWeaponCostume = hidden;
    ChatPacket(CHAT_TYPE_COMMAND, "SetWeaponCostumeHidden %d", m_bHideWeaponCostume ? 1 : 0);
    if (!pass) {
        SetQuestFlag("costume_option.hide_weapon", m_bHideWeaponCostume ? 1 : 0);
    }
}
#endif
#endif

void CHARACTER::Initialize()
{
    CEntity::Initialize(ENTITY_CHARACTER);

    m_bNoOpenedShop = true;
#ifdef ENABLE_EVENT_MANAGER
    m_bDungeonTicketExtraMetin = false;
#endif

#ifdef ENABLE_MAP1_SKILL_MOB
    m_bSkillHit = false;
#endif
    m_bOpeningSafebox = false;
    m_lastAlignmentGrade = 255;
    m_alignBonusHP = 0;
    m_alignBonusMonster = 0;
    m_alignBonusHuman = 0;
    m_alignBonusMetin = 0;
    m_alignBonusBoss = 0;
    m_alignBonusPvm = 0;
    m_alignBonusNormal = 0;
    m_alignBonusSkill = 0;
    m_alignAppliedHP = 0;
    m_alignAppliedMonster = 0;
    m_alignAppliedHuman = 0;
    m_alignAppliedMetin = 0;
    m_alignAppliedBoss = 0;
    m_alignAppliedPvm = 0;
    m_alignAppliedNormal = 0;
    m_alignAppliedSkill = 0;

    m_fSyncTime = get_float_time() - 3;
    m_dwPlayerID = 0;
#ifdef __NEWPET_SYSTEM__
    m_stImmortalSt = 0;
    m_newpetskillcd[0] = 0;
    m_newpetskillcd[1] = 0;
    m_newpetskillcd[2] = 0;
    m_newpetskillcd[3] = 0;
#endif
    m_dwKillerPID = 0;
#ifdef __SEND_TARGET_INFO__
    dwLastTargetInfoPulse = 0;
#endif
    m_iMoveCount = 0;

    m_pkRegen = nullptr;
    regen_id_ = 0;
    m_posRegen.x = m_posRegen.y = m_posRegen.z = 0;
    m_posStart.x = m_posStart.y = 0;
    m_posDest.x = m_posDest.y = 0;
    m_fRegenAngle = 0.0f;

    m_pkMobData = nullptr;
    m_pkMobInst = nullptr;

    m_pkShop = nullptr;
    m_pkChrShopOwner = nullptr;
    m_pkMyShop = nullptr;
    m_pkExchange = nullptr;
    m_pkParty = nullptr;
    m_pkPartyRequestEvent = nullptr;

    m_pGuild = nullptr;

    m_pkChrTarget = nullptr;

    m_pkMuyeongEvent = nullptr;
#ifdef ENABLE_NEW_GYEONGGONG_SKILL
    m_pkGyeongGongEvent = nullptr;
#endif
    m_pkWarpNPCEvent = nullptr;
    m_pkDeadEvent = nullptr;
    m_pkStunEvent = nullptr;
    m_pkSaveEvent = nullptr;
    m_pkRecoveryEvent = nullptr;
    m_pkTimedEvent = nullptr;
    m_pkFishingEvent = nullptr;
    m_pkWarpEvent = nullptr;
#ifdef ENABLE_BATTLE_PASS_STAY_ONLINE
    m_pkBattlePassStayOnlineEvent = nullptr;
#endif

    m_pkMiningEvent = nullptr;

    m_pkPoisonEvent = nullptr;
#ifdef ENABLE_WOLFMAN_CHARACTER
    m_pkBleedingEvent = NULL;
#endif
    m_pkFireEvent = nullptr;
    m_pkAffectEvent = nullptr;
    m_afAffectFlag = TAffectFlag(0, 0);

    m_pkDestroyWhenIdleEvent = nullptr;

    m_pkChrSyncOwner = nullptr;

    memset(&m_points, 0, sizeof(m_points));
    memset(&m_pointsInstant, 0, sizeof(m_pointsInstant));
    memset(&m_quickslot, 0, sizeof(m_quickslot));

    m_bCharType = CHAR_TYPE_MONSTER;

    SetPosition(POS_STANDING);

    m_dwPlayStartTime = m_dwLastMoveTime = get_dword_time();

    EnterIdleState(this);
    m_dwStateDuration = 1;

    m_dwLastAttackTime = get_dword_time() - 20000;

    m_bAddChrState = 0;
#if defined(BL_OFFLINE_MESSAGE)
    dwLastOfflinePMTime = 0;
#endif
    m_pkChrStone = nullptr;

    m_pkSafebox = nullptr;
    m_iSafeboxSize = -1;
    m_iSafeboxLoadTime = 0;

    m_pkMountInventory = nullptr;
    m_bMountInventoryLoaded = false;

    m_pkMall = nullptr;
    m_iMallLoadTime = 0;

    m_posWarp.x = m_posWarp.y = m_posWarp.z = 0;
    m_lWarpMapIndex = 0;

    m_posExit.x = m_posExit.y = m_posExit.z = 0;
    m_lExitMapIndex = 0;

    m_pSkillLevels = nullptr;

    m_dwMoveStartTime = 0;
    m_dwMoveDuration = 0;

    m_dwFlyTargetID = 0;

    m_dwNextStatePulse = 0;

    m_dwLastDeadTime = get_dword_time() - 180000;

    m_bSkipSave = false;

    m_bItemLoaded = false;

    m_bHasPoisoned = false;
#ifdef ENABLE_WOLFMAN_CHARACTER
    m_bHasBled = false;
#endif
    m_pkDungeon = nullptr;
    m_iEventAttr = 0;

    m_kAttackLog.dwVID = 0;
    m_kAttackLog.dwTime = 0;

    m_bNowWalking = m_bWalking = false;
    ResetChangeAttackPositionTime();

    m_bDetailLog = false;
    m_bMonsterLog = false;

    m_bDisableCooltime = false;

    m_iAlignment = 0;
    m_iRealAlignment = 0;

    m_iKillerModePulse = 0;
    m_bPKMode = PK_MODE_PEACE;

    m_dwQuestNPCVID = 0;
    m_dwQuestByVnum = 0;
    m_pQuestItem = nullptr;

    m_szMobileAuth[0] = '\0';

    m_dwUnderGuildWarInfoMessageTime = get_dword_time() - 60000;

    m_bUnderRefine = false;

    m_dwRefineNPCVID = 0;

    m_dwPolymorphRace = 0;

    m_bStaminaConsume = false;

    ResetChainLightningIndex();

    m_dwMountVnum = 0;
    m_chHorse = nullptr;
    m_chRider = nullptr;

    m_pWarMap = nullptr;
    m_pWeddingMap = nullptr;
    m_bChatCounter = 0;
#ifdef ENABLE_FAKE_SHOP_HEADER
    m_lastBeltMountCount = -999;
#endif
#ifdef __ENABLE_NEW_OFFLINESHOP__
    m_pkOfflineShop = nullptr;
    m_pkShopSafebox = nullptr;
    m_pkAuction = nullptr;
    m_pkAuctionGuest = nullptr;
    m_pkOfflineShopGuest = nullptr;
    m_bIsLookingOfflineshopOfferList = false;
#endif

    ResetStopTime();
#ifdef ENABLE_GAYA_SYSTEM
    LOAD_GAYA();
#endif
    m_dwLastVictimSetTime = get_dword_time() - 3000;
    m_iMaxAggro = -100;

    m_bSendHorseLevel = 0;
    m_bSendHorseHealthGrade = 0;
    m_bSendHorseStaminaGrade = 0;

    m_dwLoginPlayTime = 0;

    m_pkChrMarried = nullptr;

    m_posSafeboxOpen.x = -1000;
    m_posSafeboxOpen.y = -1000;

    m_dwLastSkillTime = get_dword_time();

    memset(m_adwMobSkillCooltime, 0, sizeof(m_adwMobSkillCooltime));

    m_isinPCBang = false;

    m_pArena = nullptr;
    m_nPotionLimit = quest::CQuestManager::instance().GetEventFlag("arena_potion_limit_count");

    m_isOpenSafebox = 0;

    m_iRefineTime = 0;

    m_iSeedTime = 0;
    m_iExchangeTime = 0;
    m_iMyShopTime = 0;

    m_deposit_pulse = 0;

    m_strNewName = "";

    m_known_guild.clear();

    m_dwLogOffInterval = 0;

    m_bComboSequence = 0;
    m_dwLastComboTime = 0;
    m_bComboIndex = 0;
    m_iComboHackCount = 0;
    m_dwSkipComboAttackByTime = 0;

    m_dwMountTime = 0;

    m_dwLastGoldDropTime = 0;
#ifdef ENABLE_NEWSTUFF
    m_dwLastBoxUseTime = 0;
    m_dwLastBuySellTime = 0;
#endif

    m_bIsLoadedAffect = false;
    cannot_dead = false;

#ifdef __PET_SYSTEM__
    m_petSystem = nullptr;
    m_bIsPet = false;
#endif

#ifdef __NEWPET_SYSTEM__
    m_newpetSystem = nullptr;
    m_bIsNewPet = false;
    m_eggvid = 0;
#endif
    m_fAttMul = 1.0f;
    m_fDamMul = 1.0f;

    m_pointsInstant.iDragonSoulActiveDeck = -1;

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
    m_mountSystem = nullptr;
    m_bIsMount = false;
#endif

#ifdef ENABLE_ANTI_CMD_FLOOD
    m_dwCmdAntiFloodCount = 0;
    m_dwCmdAntiFloodPulse = 0;
#endif
    memset(&m_tvLastSyncTime, 0, sizeof(m_tvLastSyncTime));
    m_iSyncHackCount = 0;
#ifdef ENABLE_NEW_FISHING_SYSTEM
    m_pkFishingNewEvent = nullptr;
    m_bFishCatch = 0;
    m_dwLastCatch = 0;
    m_dwCatchFailed = 0;
#endif
#ifdef ENABLE_RANKING
    for (int i = 0; i < RANKING_MAX_CATEGORIES; ++i)
        m_lRankPoints[i] = 0;
#endif

#ifdef ENABLE_ATTR_COSTUMES
    attrdialog_remove = 0;
#endif
#ifdef ENABLE_BATTLE_PASS
    m_listBattlePass.clear();
    m_bIsLoadedBattlePass = false;

    m_dwBattlePassEndTime = 0;

#ifdef ENABLE_BATTLE_PASS_STAY_ONLINE
    m_pkStayOnlineEvent = nullptr;
#endif

#endif
    m_stName = "";

#ifdef __SKILL_COLOR_SYSTEM__
    memset(&m_dwSkillColor, 0, sizeof(m_dwSkillColor));
#endif
#ifdef ENABLE_ACCE_SYSTEM
    m_bAcceCombination = false;
    m_bAcceAbsorption = false;
#endif

#ifdef __HIDE_COSTUME_SYSTEM__
    m_bHideBodyCostume = false;
    m_bHideHairCostume = false;
#ifdef ENABLE_ACCE_SYSTEM
    m_bHideAcceCostume = false;
#endif
    m_bHideWeaponCostume = false;
#endif
#ifdef ENABLE_NEW_PET_EDITS
    petenchant = 0;
#endif
#ifdef KASMIR_PAKET_SYSTEM
    m_bKasmirPaketBaslik = 0;
    m_bKasmirPaketDurum = false;
#endif
    isInvincible = false;
    m_iGoToXYTime = 0;
#ifdef ENABLE_SAVEPOINT_SYSTEM
    m_iSavePointTime = 0;
#endif
#ifdef ENABLE_SORT_INVEN
    m_iSortInv1Time = 0;
    m_iSortInv2Time = 0;
#endif
#ifdef ENABLE_LIMIT_BUY_SPEED
    m_iLastBuyTime = 0;
#endif
#ifdef __DUNGEON_INFO_SYSTEM__
    dungeonDamage.clear();
#endif
#ifdef ENABLE_SPAM_CHECK
    m_iLastUnlock = 0;
    m_iLastDSRefine = 0;
#endif
#ifdef ENABLE_ANTICHEAT
    m_firstReward = 0;
    m_rewardCount = 0;
    m_checkRepeated = 0;
    m_dropitemcount = 0;
    m_lastdropitem = 0;
#endif
#ifdef ENABLE_BLOCK_MULTIFARM
    m_pkDropEvent = nullptr;
#endif
}

void CHARACTER::Create(const char* c_pszName, uint32_t vid, bool isPC)
{
    static int s_crc = 172814;

    char crc_string[128 + 1];
    snprintf(crc_string, sizeof(crc_string), "%s%p%d", c_pszName, this, ++s_crc);
    m_vid = VID(vid, GetCRC32(crc_string, strlen(crc_string)));
    if (isPC)
        m_stName = c_pszName;
}
