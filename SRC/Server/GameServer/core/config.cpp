#include "stdafx.h"
#include <Core/peer_auth.h>
#include <string_view>
#ifndef _WIN32
#include <ifaddrs.h>
#endif

#include "constants.h"
#include "utils.h"
#include "log.h"
#include "desc.h"
#include "desc_manager.h"
#include "../ecs/CharacterAccessors.hpp"
#include "item_manager.h"
#include "p2p.h"
#include "char_interface.hpp"
#include "ip_ban.h"
#include "war_map.h"
#include "locale_service.h"
#include "config.h"
#include "GameConfig.h"
#include "dev_log.h"
#include <Core/Logging.hpp>
#include "db.h"
#include "skill_power.h"
#include "Core/Logging.hpp"
#include <fmt/format.h>
#include <fstream>
#include <sstream>

using std::string;

uint8_t	g_bChannel = 0;

uint16_t	mother_port = 50080;
int		passes_per_sec = 25;
uint16_t	db_port = 0;
uint16_t	p2p_port = 50900;
std::string	g_clientTlsCertFile;
std::string	g_clientTlsKeyFile;
char	db_addr[ADDRESS_MAX_LEN + 1];
int		save_event_second_cycle = passes_per_sec * 120;
int		ping_event_second_cycle = passes_per_sec * 60;
bool	g_bNoMoreClient = false;
bool	g_bNoRegen = false;
// #ifdef ENABLE_NEWSTUFF
bool	g_bEmpireShopPriceTripleDisable = false;
bool	g_bShoutAddonEnable = false;
bool	g_bGlobalShoutEnable = false;
bool	g_bDisablePrismNeed = false;
bool	g_bDisableEmotionMask = false;
#ifdef ENABLE_NEW_STACK_LIMIT
int	g_bItemCountLimit = 50000;
#else
uint8_t	g_bItemCountLimit = 200;
#endif
uint32_t	g_dwItemBonusChangeTime = 60;
bool	g_bAllMountAttack = true;
bool	g_bEnableBootaryCheck = false;
bool	g_bGMHostCheck = false;
bool	g_bGuildInviteLimit = false;
bool	g_bGuildInfiniteMembers = false;
int		g_iStatusPointGetLevelLimit = 90;
int		g_iStatusPointSetMaxValue = 90;
int		g_iShoutLimitLevel = 15;
// int		g_iShoutLimitTime = 15;
int		g_iDbLogLevel = LOG_LEVEL_MAX;
int		g_iSysLogLevel = LOG_LEVEL_MAX;
int		g_aiItemDestroyTime[ITEM_DESTROY_TIME_MAX] = {300, 150, 30}; // autoitem, dropgold, dropitem
bool	g_bDisableEmpireLanguageCheck = false;
uint32_t	g_dwSkillBookNextReadMin = 28800;
uint32_t	g_dwSkillBookNextReadMax = 43200;
// #endif

// TRAFFIC_PROFILER
bool		g_bTrafficProfileOn = false;
uint32_t		g_dwTrafficProfileFlushCycle = 3600;
// END_OF_TRAFFIC_PROFILER

int			test_server = 0;
bool		guild_mark_server = true;
uint8_t		guild_mark_min_level = 3;
bool		no_wander = false;
int			g_iUserLimit = 32768;

char		g_szPublicIP[16] = "0";
char		g_szInternalIP[16] = "0";
bool		g_bSkillDisable = false;
int			g_iFullUserCount = 1200;
int			g_iBusyUserCount = 650;
//Canada
//int			g_iFullUserCount = 600;
//int			g_iBusyUserCount = 350;
bool		g_bEmpireWhisper = true;

bool		g_bCheckClientVersion = true;
string	g_stClientVersion = "20251204";


static std::set<uint32_t> s_set_dwFileCRC;
static std::set<uint32_t> s_set_dwProcessCRC;

string g_stHostname = "";
string g_table_postfix = "";

string g_stQuestDir = "./quest";
//string g_stQuestObjectDir = "./quest/object";
string g_stDefaultQuestObjectDir = "./quest/object";
std::set<string> g_setQuestObjectDir;

string g_stBlockDate = "30000705";

extern string g_stLocale;


int VIEW_RANGE = 5000;
int VIEW_BONUS_RANGE = 500;

int g_server_id = 0;
string g_strWebMallURL = "https://www.wonder2.org/index.php/shop/login?pid=$playerID&key=$codice";

unsigned int g_uiSpamBlockDuration = 60 * 15;
unsigned int g_uiSpamBlockScore = 100;
unsigned int g_uiSpamReloadCycle = 60 * 10;

int			g_iSpamBlockMaxLevel = 10;

void		LoadStateUserCount();
void		LoadValidCRCList();
bool		LoadClientVersion();
bool            g_protectNormalPlayer   = false;
bool            g_noticeBattleZone      = false;

int gPlayerMaxLevel = 99;
int stone_chance = 30;
int gShutdownAge = 0;
int gShutdownEnable = 0;
#ifdef ENABLE_MAP_TELEPORTER
MAPCONFIG_VEC	g_vecMapConf;
void LoadMapConfig();
#endif
bool gHackCheckEnable = false;

bool g_BlockCharCreation = false;
#ifdef __ATTR_TRANSFER_SYSTEM__
int	gAttrTransferLimit = 0;
#endif

static std::set<int> s_set_map_allows;

bool map_allow_find(int32_t index)
{
	if (s_set_map_allows.find(index) == s_set_map_allows.end())
		return false;

	return true;
}

void map_allow_log()
{
	std::set<int>::iterator i;

	for (i = s_set_map_allows.begin(); i != s_set_map_allows.end(); ++i)
		LOG_INFO("MAP_ALLOW: {}", *i);
}

static void map_allow_add(int32_t index)
{
	if (map_allow_find(index) == true)
	{
		LOG_ERROR("!!! FATAL ERROR !!! multiple MAP_ALLOW setting!!");
	fmt::print(stderr, "!!! FATAL ERROR !!! multiple MAP_ALLOW setting!!\n");
		exit(1);
	}

	LOG_INFO("MAP ALLOW {}", index);
	s_set_map_allows.insert(index);
}

void map_allow_copy(int32_t* pl, int size)
{
	int iCount = 0;
	std::set<int>::iterator it = s_set_map_allows.begin();

	while (it != s_set_map_allows.end())
	{
		int i = *(it++);
		*(pl++) = i;

		if (++iCount > size)
			break;
	}
}

#define ENABLE_AUTODETECT_INTERNAL_IP
bool GetIPInfo()
{
#ifndef _WIN32
	struct ifaddrs* ifaddrp = NULL;

	if (0 != getifaddrs(&ifaddrp))
		return false;

	for( struct ifaddrs* ifap=ifaddrp ; NULL != ifap ; ifap = ifap->ifa_next )
	{
		struct sockaddr_in * sai = (struct sockaddr_in *) ifap->ifa_addr;

		if (!ifap->ifa_netmask ||  // ignore if no netmask
				sai->sin_addr.s_addr == 0 || // ignore if address is 0.0.0.0
				sai->sin_addr.s_addr == 16777343) // ignore if address is 127.0.0.1
			continue;
#else
	WSADATA wsa_data;
	char host_name[100];
	HOSTENT* host_ent;
	int n = 0;

	if (WSAStartup(0x0101, &wsa_data)) {
		return false;
	}

	gethostname(host_name, sizeof(host_name));
	host_ent = gethostbyname(host_name);
	if (host_ent == nullptr) {
		return false;
	}
	for ( ; host_ent->h_addr_list[n] != nullptr; ++n) {
		struct sockaddr_in addr;
		struct sockaddr_in* sai = &addr;
		memcpy(&sai->sin_addr.s_addr, host_ent->h_addr_list[n], host_ent->h_length);
#endif

		char * netip = inet_ntoa(sai->sin_addr);

		if (!strncmp(netip, "207.180.218.86", 7)) // ignore if address is starting with 192
		{
			strlcpy(g_szInternalIP, netip, sizeof(g_szInternalIP));
#ifndef _WIN32
			LOG_ERROR("INTERNAL_IP: {} interface {}", netip, ifap->ifa_name);
#else
			LOG_ERROR("INTERNAL_IP: {}", netip);
#endif
		}
		else if (!strncmp(netip, "10.", 3))
		{
			strlcpy(g_szInternalIP, netip, sizeof(g_szInternalIP));
#ifndef _WIN32
			LOG_ERROR("INTERNAL_IP: {} interface {}", netip, ifap->ifa_name);
#else
			LOG_ERROR("INTERNAL_IP: {}", netip);
#endif
		}
		else if (g_szPublicIP[0] == '0')
		{
			strlcpy(g_szPublicIP, netip, sizeof(g_szPublicIP));
#ifndef _WIN32
			LOG_ERROR("PUBLIC_IP: {} interface {}", netip, ifap->ifa_name);
#else
			LOG_ERROR("PUBLIC_IP: {}", netip);
#endif
		}
	}

#ifndef _WIN32
	freeifaddrs( ifaddrp );
#else
	WSACleanup();
#endif

	if (g_szPublicIP[0] != '0')
		return true;
	else
	{
#ifdef ENABLE_AUTODETECT_INTERNAL_IP
		if (g_szInternalIP[0] == '0')
			return false;
		else
		{
			strlcpy(g_szPublicIP, g_szInternalIP, sizeof(g_szPublicIP));
			LOG_ERROR("INTERNAL_IP -> PUBLIC_IP: {}", g_szPublicIP);
			return true;
		}
#else
		return false;
#endif
	}
}

static void __ApplyConnectConfig(const GameConfig& cfg)
{
	g_stHostname = cfg.server.hostname;
	g_bChannel = cfg.server.channel;

	LOG_INFO("HOSTNAME: {}", g_stHostname.c_str());

	AccountDB::instance().Connect(cfg.sql.common.host.c_str(), cfg.sql.common.port,
			cfg.sql.common.user.c_str(), cfg.sql.common.password.c_str(), cfg.sql.common.database.c_str());

	if (false == AccountDB::instance().IsConnected())
	{
		LOG_ERROR("cannot start server while no common sql connected");
		exit(1);
	}

	LOG_INFO("CommonSQL connected");

	if (LocaleService_InitFlat(cfg.server.dataPath, cfg.server.locale) == false)
	{
		LOG_ERROR("GameServer: invalid locale configuration (data_path '{}', locale '{}')",
				cfg.server.dataPath, cfg.server.locale);
		exit(1);
	}

	LOG_INFO("Setting DB to locale {}", g_stLocale.c_str());

	AccountDB::instance().SetLocale(g_stLocale);

	AccountDB::instance().ConnectAsync(cfg.sql.common.host.c_str(), cfg.sql.common.port,
			cfg.sql.common.user.c_str(), cfg.sql.common.password.c_str(), cfg.sql.common.database.c_str(), g_stLocale.c_str());

	DBManager::instance().Connect(cfg.sql.player.host.c_str(), cfg.sql.player.port,
			cfg.sql.player.user.c_str(), cfg.sql.player.password.c_str(), cfg.sql.player.database.c_str());

	if (!DBManager::instance().IsConnected())
	{
		LOG_ERROR("PlayerSQL.ConnectError");
		exit(1);
	}

	LOG_INFO("PlayerSQL connected");

	{
		LogManager::instance().Connect(cfg.sql.log.host.c_str(), cfg.sql.log.port,
				cfg.sql.log.user.c_str(), cfg.sql.log.password.c_str(), cfg.sql.log.database.c_str());

		if (!LogManager::instance().IsConnected())
		{
			LOG_ERROR("LogSQL.ConnectError");
			exit(1);
		}

		LOG_INFO("LogSQL connected");

		LogManager::instance().BootLog(g_stHostname.c_str(), g_bChannel);
	}

	// SKILL_POWER_BY_LEVEL
	{
		char szQuery[256];
		snprintf(szQuery, sizeof(szQuery), "SELECT mValue FROM locale WHERE mKey='SKILL_POWER_BY_LEVEL'");
		std::unique_ptr<SQLMsg> pMsg(AccountDB::instance().DirectQuery(szQuery));

		if (pMsg->Get()->uiNumRows == 0)
		{
			LOG_ERROR("[SKILL_PERCENT] Query failed: {}", szQuery);
			exit(1);
		}

		MYSQL_ROW row;

		row = pMsg->Get()->FetchRow();

		const char * p = row[0];
		int cnt = 0;
		char num[128];
		int aiBaseSkillPowerByLevelTable[SKILL_MAX_LEVEL+1];

		LOG_INFO("SKILL_POWER_BY_LEVEL {}", p);
		while (*p != '\0' && cnt < (SKILL_MAX_LEVEL + 1))
		{
			p = one_argument(p, num, sizeof(num));
			aiBaseSkillPowerByLevelTable[cnt++] = atoi(num);

			// LOG_INFO("{} {}", cnt - 1, aiBaseSkillPowerByLevelTable[cnt - 1]);
			if (*p == '\0')
			{
				if (cnt != (SKILL_MAX_LEVEL + 1))
				{
					LOG_ERROR("[SKILL_PERCENT] locale table has not enough skill information! (count: {} query: {})", cnt, szQuery);
					exit(1);
				}

				LOG_INFO("SKILL_POWER_BY_LEVEL: Done! (count {})", cnt);
				break;
			}
		}

		for (int job = 0; job < JOB_MAX_NUM * 2; ++job)
		{
			snprintf(szQuery, sizeof(szQuery), "SELECT mValue from locale where mKey='SKILL_POWER_BY_LEVEL_TYPE%d' ORDER BY CAST(mValue AS unsigned)", job);
			std::unique_ptr<SQLMsg> pMsg(AccountDB::instance().DirectQuery(szQuery));

			if (pMsg->Get()->uiNumRows == 0)
			{
				CTableBySkill::instance().SetSkillPowerByLevelFromType(job, aiBaseSkillPowerByLevelTable);
				continue;
			}

			row = pMsg->Get()->FetchRow();
			cnt = 0;
			p = row[0];
			int aiSkillTable[SKILL_MAX_LEVEL + 1];

			LOG_INFO("SKILL_POWER_BY_JOB {} {}", job, p);
			while (*p != '\0' && cnt < (SKILL_MAX_LEVEL + 1))
			{
				p = one_argument(p, num, sizeof(num));
				aiSkillTable[cnt++] = atoi(num);

				// LOG_INFO("{} {}", cnt - 1, aiBaseSkillPowerByLevelTable[cnt - 1]);
				if (*p == '\0')
				{
					if (cnt != (SKILL_MAX_LEVEL + 1))
					{
						LOG_ERROR("[SKILL_PERCENT] locale table has not enough skill information! (count: {} query: {})", cnt, szQuery);
						exit(1);
					}

					LOG_INFO("SKILL_POWER_BY_JOB: Done! (job: {} count: {})", job, cnt);
					break;
				}
			}

			CTableBySkill::instance().SetSkillPowerByLevelFromType(job, aiSkillTable);
		}
	}
	// END_SKILL_POWER_BY_LEVEL

	// LOG_KEEP_DAYS_EXTEND
	log_set_expiration_days(2);
	// END_OF_LOG_KEEP_DAYS_EXTEND
}

static void __ApplyDefaultConfig(const GameConfig& cfg)
{
	mother_port = cfg.network.port;
	p2p_port = cfg.network.p2pPort;
	g_clientTlsCertFile = cfg.network.tls.certFile;
	g_clientTlsKeyFile = cfg.network.tls.keyFile;

	for (int32_t index : cfg.mapAllow)
		map_allow_add(index);
}

static void __ApplyGeneralConfig(const GameConfig& cfg)
{
	g_stBlockDate = cfg.server.blockLogin;

	db_port = cfg.network.dbPort;

	strlcpy(db_addr, cfg.network.dbAddr.c_str(), sizeof(db_addr));

	for (int n = 0; n < ADDRESS_MAX_LEN; ++n)
	{
		if (db_addr[n] == ' ')
			db_addr[n] = '\0';
	}

	if (!cfg.interserverKey.empty())
		PeerAuth::SetKey(cfg.interserverKey);

	g_bEmpireWhisper = cfg.server.empireWhisper;

	guild_mark_server = cfg.guild.markServer;
	guild_mark_min_level = static_cast<uint8_t>(MINMAX(0, cfg.guild.markMinLevel, GUILD_MAX_LEVEL));

	log_set_expiration_days(MINMAX(1, cfg.logs.keepDays, 90));

	passes_per_sec = cfg.server.passesPerSec;
	save_event_second_cycle = cfg.events.saveSecondCycle * passes_per_sec;
	ping_event_second_cycle = cfg.events.pingSecondCycle * passes_per_sec;

	g_table_postfix = cfg.server.tablePostfix;

	LOG_INFO("-----------------------------------------------");
	LOG_INFO("TEST_SERVER");
	LOG_INFO("-----------------------------------------------");
	test_server = cfg.server.test;

	g_bNoMoreClient = cfg.server.shutdowned;
	g_bNoRegen = cfg.server.noRegen;

	g_bItemCountLimit = cfg.gameplay.itemCountLimit;
	LOG_INFO("ITEM_COUNT_LIMIT: {}", g_bItemCountLimit);

	g_bEmpireShopPriceTripleDisable = cfg.gameplay.empireShopPriceTripleDisable;
	LOG_INFO("EMPIRE_SHOP_PRICE_3x: {}", (g_bEmpireShopPriceTripleDisable) ? "DISABLED" : "ENABLED");

	g_bShoutAddonEnable = cfg.gameplay.shoutAddonEnable;

	g_bAllMountAttack = cfg.gameplay.allMountAttack;

	g_dwItemBonusChangeTime = cfg.gameplay.itemBonusChangeTime;
	LOG_INFO("CHANGE_ATTR_TIME_LIMIT: {}", g_dwItemBonusChangeTime);

	g_bDisablePrismNeed = cfg.gameplay.disablePrismNeed;
	LOG_INFO("PRISM_ITEM_REQUIREMENT: {}", (g_bDisablePrismNeed) ? "DISABLED" : "ENABLED");

	g_bGlobalShoutEnable = cfg.gameplay.globalShoutEnable;
	LOG_INFO("GLOBAL_SHOUT: {}", (g_bGlobalShoutEnable) ? "ENABLED" : "DISABLED");

	g_bDisableEmotionMask = cfg.gameplay.disableEmotionMask;
	LOG_INFO("EMOTION_MASK_REQUIREMENT: {}", (g_bDisableEmotionMask) ? "DISABLED" : "ENABLED");

	g_bEnableBootaryCheck = cfg.gameplay.enableBootaryCheck;
	LOG_INFO("BOOTARY_CHECK: {}", (g_bEnableBootaryCheck) ? "ENABLED" : "DISABLED");

	if (cfg.gameplay.statusPointGetLevelLimit > 0)
	{
		g_iStatusPointGetLevelLimit = MINMAX(0, cfg.gameplay.statusPointGetLevelLimit, PLAYER_MAX_LEVEL_CONST);
		LOG_INFO("STATUS_POINT_GET_LEVEL_LIMIT: {}", g_iStatusPointGetLevelLimit);
	}

	if (cfg.gameplay.statusPointSetMaxValue > 0)
	{
		g_iStatusPointSetMaxValue = cfg.gameplay.statusPointSetMaxValue;
		LOG_INFO("STATUS_POINT_SET_MAX_VALUE: {}", g_iStatusPointSetMaxValue);
	}

	if (cfg.gameplay.shoutLimitLevel > 0)
	{
		g_iShoutLimitLevel = cfg.gameplay.shoutLimitLevel;
		LOG_INFO("SHOUT_LIMIT_LEVEL: {}", g_iShoutLimitLevel);
	}

	g_iDbLogLevel = cfg.logs.dbLevel;
	LOG_INFO("DB_LOG_LEVEL: {}", g_iDbLogLevel);

	g_iSysLogLevel = cfg.logs.sysLevel;
	LOG_INFO("SYS_LOG_LEVEL: {}", g_iSysLogLevel);

	g_aiItemDestroyTime[ITEM_DESTROY_TIME_AUTOGIVE] = cfg.itemDestroy.autogive;
	LOG_INFO("ITEM_DESTROY_TIME_AUTOGIVE: {}", g_aiItemDestroyTime[ITEM_DESTROY_TIME_AUTOGIVE]);

	g_aiItemDestroyTime[ITEM_DESTROY_TIME_DROPGOLD] = cfg.itemDestroy.dropgold;
	LOG_INFO("ITEM_DESTROY_TIME_DROPGOLD: {}", g_aiItemDestroyTime[ITEM_DESTROY_TIME_DROPGOLD]);

	g_aiItemDestroyTime[ITEM_DESTROY_TIME_DROPITEM] = cfg.itemDestroy.dropitem;
	LOG_INFO("ITEM_DESTROY_TIME_DROPITEM: {}", g_aiItemDestroyTime[ITEM_DESTROY_TIME_DROPITEM]);

	g_bCheckClientVersion = cfg.version.checkServer;
	LOG_INFO("CHECK_VERSION_SERVER: {}", g_bCheckClientVersion);

	g_stClientVersion = cfg.version.value;
	LOG_INFO("CHECK_VERSION_VALUE: {}", g_stClientVersion.c_str());

	g_bGMHostCheck = cfg.gameplay.gmHostCheck;
	LOG_INFO("GM_HOST_CHECK: {}", g_bGMHostCheck);

	g_bGuildInviteLimit = cfg.guild.inviteLimit;
	LOG_INFO("GUILD_INVITE_LIMIT: {}", g_bGuildInviteLimit);

	g_bGuildInfiniteMembers = cfg.guild.infiniteMembers;
	LOG_INFO("GUILD_INFINITE_MEMBERS: {}", g_bGuildInfiniteMembers);

	g_bDisableEmpireLanguageCheck = cfg.gameplay.disableEmpireLanguageCheck;
	LOG_INFO("EMPIRE_LANGUAGE_CHECK: {}", (g_bDisableEmpireLanguageCheck) ? "DISABLED" : "ENABLED");

	g_dwSkillBookNextReadMin = cfg.gameplay.skillBookNextReadMin;
	LOG_INFO("SKILLBOOK_NEXTREAD_MIN: {}", g_dwSkillBookNextReadMin);

	g_dwSkillBookNextReadMax = cfg.gameplay.skillBookNextReadMax;
	LOG_INFO("SKILLBOOK_NEXTREAD_MAX: {}", g_dwSkillBookNextReadMax);

	g_bTrafficProfileOn = cfg.server.trafficProfile;
	no_wander = cfg.server.noWander;
	g_iUserLimit = cfg.server.userLimit;
	g_bSkillDisable = cfg.server.skillDisable;

	if (!cfg.quest.dir.empty())
	{
		LOG_INFO("QUEST_DIR SETTING : {}", cfg.quest.dir.c_str());
		g_stQuestDir = cfg.quest.dir;
	}

	for (const auto& dir : cfg.quest.objectDir)
	{
		LOG_INFO("QUEST_OBJECT_DIR INSERT : {}", dir.c_str());
		g_setQuestObjectDir.insert(dir);
	}

	g_server_id = cfg.server.serverId;
	g_strWebMallURL = cfg.server.mallUrl;

	if (!cfg.network.bindIp.empty())
		strlcpy(g_szPublicIP, cfg.network.bindIp.c_str(), sizeof(g_szPublicIP));

	VIEW_RANGE = cfg.server.viewRange;

	g_uiSpamBlockDuration = cfg.spamBlock.duration;
	g_uiSpamBlockScore = static_cast<unsigned int>(MAX(1, static_cast<int>(cfg.spamBlock.score)));
	g_uiSpamReloadCycle = static_cast<unsigned int>(MAX(60, static_cast<int>(cfg.spamBlock.reloadCycle)));
	g_iSpamBlockMaxLevel = cfg.spamBlock.maxLevel;

	g_protectNormalPlayer = cfg.server.protectNormalPlayer;
	g_noticeBattleZone = cfg.server.noticeBattleZone;

	if (cfg.server.pkProtectLevel >= 0)
	{
		PK_PROTECT_LEVEL = static_cast<uint8_t>(cfg.server.pkProtectLevel);
		LOG_ERROR("PK_PROTECT_LEVEL: {}", PK_PROTECT_LEVEL);
	}

#ifdef __ATTR_TRANSFER_SYSTEM__
	gAttrTransferLimit = cfg.server.attrTransferLimit;
	LOG_ERROR("ATTR_TRANSFER_LIMIT: {}", gAttrTransferLimit);
#endif

	gPlayerMaxLevel = MINMAX(1, cfg.server.maxLevel, PLAYER_MAX_LEVEL_CONST);
	LOG_ERROR("PLAYER_MAX_LEVEL: {}", gPlayerMaxLevel);

	stone_chance = MINMAX(1, cfg.server.stoneChance, 100);
	LOG_ERROR("STONE_CHANCE: {}/n", stone_chance);

	gShutdownAge = cfg.shutdown.age;
	LOG_ERROR("SHUTDOWN_AGE: {}", gShutdownAge);

	gShutdownEnable = cfg.shutdown.enable;
	LOG_ERROR("SHUTDOWN_ENABLE: {}", gShutdownEnable);

	g_BlockCharCreation = cfg.server.blockCharCreation;
}

#define ENABLE_CMD_PLAYER
static bool __LoadDefaultCMDFile(const char* cmdName)
{
	std::ifstream file(cmdName);

	if (!file)
		return false;

	std::string buf;

	while (std::getline(file, buf))
	{
		std::istringstream ins(buf);

		std::string cmd;
		std::string levelname;
		int level;

		ins >> cmd >> levelname;

		if (cmd.empty() || levelname.empty())
		{
#ifdef ENABLE_CMD_PLAYER
			LOG_ERROR("CMD syntax error: <cmd> <PLAYER | LOW_WIZARD | WIZARD | HIGH_WIZARD | GOD | IMPLEMENTOR | DISABLE>");
#else
			LOG_ERROR("CMD syntax error: <cmd> <LOW_WIZARD | WIZARD | HIGH_WIZARD | GOD | IMPLEMENTOR | DISABLE>");
#endif
			exit(1);
		}

		if (!strcasecmp(levelname.c_str(), "LOW_WIZARD"))
			level = GM_LOW_WIZARD;
		else if (!strcasecmp(levelname.c_str(), "WIZARD"))
			level = GM_WIZARD;
		else if (!strcasecmp(levelname.c_str(), "HIGH_WIZARD"))
			level = GM_HIGH_WIZARD;
		else if (!strcasecmp(levelname.c_str(), "GOD"))
			level = GM_GOD;
		else if (!strcasecmp(levelname.c_str(), "IMPLEMENTOR"))
			level = GM_IMPLEMENTOR;
#ifdef ENABLE_CMD_PLAYER
		else if (!strcasecmp(levelname.c_str(), "PLAYER"))
			level = GM_PLAYER;
#endif
		else if (!strcasecmp(levelname.c_str(), "DISABLE"))
			level = GM_DISABLE;
		else
		{
#ifdef ENABLE_CMD_PLAYER
			LOG_ERROR("CMD syntax error: <cmd> <PLAYER | LOW_WIZARD | WIZARD | HIGH_WIZARD | GOD | IMPLEMENTOR | DISABLE>");
#else
			LOG_ERROR("CMD syntax error: <cmd> <LOW_WIZARD | WIZARD | HIGH_WIZARD | GOD | IMPLEMENTOR | DISABLE>");
#endif
			exit(1);
		}

		if (test_server)
			LOG_INFO("CMD_REWRITE: [{}] [{}:{}]", cmd, levelname, level);
		interpreter_set_privilege(cmd.c_str(), level);
	}

	return true;
}

#define ENABLE_EXPTABLE_FROMDB
#ifdef ENABLE_EXPTABLE_FROMDB
static bool __LoadExpTableFromDB(void)
{
	std::unique_ptr<SQLMsg> pMsg(AccountDB::instance().DirectQuery("SELECT level, exp FROM exp_table"));
	if (pMsg->Get()->uiNumRows == 0)
		return false;

	static uint32_t new_exp_table[PLAYER_MAX_LEVEL_CONST+1];
	if (exp_table != nullptr)
		memcpy(new_exp_table, exp_table, (PLAYER_MAX_LEVEL_CONST+1)*sizeof(uint32_t));

	MYSQL_ROW row = nullptr;
	while ((row = pMsg->Get()->FetchRow()))
	{
		uint32_t level = 0;
		uint32_t exp = 0;
		str_to_number(level, row[0]);
		str_to_number(exp, row[1]);
		if (level > PLAYER_MAX_LEVEL_CONST)
			continue;
		new_exp_table[level] = exp;
		// LOG_INFO("new_exp_table[{}] = {};", level, exp);
	}
	exp_table = new_exp_table;
	return true;
}
#endif

void config_init(const GameConfig& cfg)
{
	GetIPInfo();

	__ApplyConnectConfig(cfg);
	__ApplyDefaultConfig(cfg);
	__ApplyGeneralConfig(cfg);

	if (g_setQuestObjectDir.empty())
		g_setQuestObjectDir.insert(g_stDefaultQuestObjectDir);

	if (0 == db_port)
	{
		LOG_ERROR("DB_PORT not configured");
		exit(1);
	}

	if (0 == g_bChannel)
	{
		LOG_ERROR("CHANNEL not configured");
		exit(1);
	}

	if (g_stHostname.empty())
	{
		LOG_ERROR("HOSTNAME must be configured.");
		exit(1);
	}

	// LOCALE_SERVICE
	LocaleService_TransferDefaultSetting();
	LocaleService_LoadEmpireTextConvertTables();
	// END_OF_LOCALE_SERVICE

#ifdef ENABLE_EXPTABLE_FROMDB
	if (!__LoadExpTableFromDB())
	{
		// do as you please to manage this
		LOG_ERROR("Failed to Load ExpTable from DB so exit");
		// exit(1);
	}
#endif

	std::string st_cmdFileName("CMD");
	__LoadDefaultCMDFile(st_cmdFileName.c_str());

	LoadValidCRCList();
	LoadStateUserCount();
#ifdef ENABLE_MAP_TELEPORTER
	LoadMapConfig();
#endif
	CWarMapManager::instance().LoadWarMapInfo(nullptr);

	if (g_szPublicIP[0] == '0')
	{
		LOG_ERROR("Can not get public ip address");
		exit(1);
	}
}

const char* get_table_postfix()
{
	return g_table_postfix.c_str();
}

void LoadValidCRCList()
{
	s_set_dwProcessCRC.clear();
	s_set_dwFileCRC.clear();

	std::ifstream file("CRC");

	if (!file)
		return;

	std::string buf;

	while (std::getline(file, buf))
	{
		std::istringstream ins(buf);

		uint32_t dwValidClientProcessCRC = 0;
		uint32_t dwValidClientFileCRC = 0;

		if (!(ins >> dwValidClientProcessCRC >> dwValidClientFileCRC))
			continue;

		s_set_dwProcessCRC.insert(dwValidClientProcessCRC);
		s_set_dwFileCRC.insert(dwValidClientFileCRC);

		LOG_ERROR("CLIENT_CRC: {} {}", dwValidClientProcessCRC, dwValidClientFileCRC);
	}
}

bool LoadClientVersion()
{
	std::ifstream file("VERSION");

	if (!file)
		return false;

	std::string version;

	if (!std::getline(file, version))
		return false;

	// CRLF files (Windows-edited VERSION) leave a trailing '\r'. A '\r' in
	// the version string makes CheckClientVersion reject every client with
	// a silent kick.
	if (!version.empty() && version.back() == '\r')
		version.pop_back();

	LOG_ERROR("VERSION: \"{}\"", version);

	g_stClientVersion = version;
	return true;
}

void CheckClientVersion()
{
	const DESC_MANAGER::DESC_SET & set = DESC_MANAGER::instance().GetClientSet();
	DESC_MANAGER::DESC_SET::const_iterator it = set.begin();

	while (it != set.end())
	{
		LPDESC d = *(it++);

		if (!ecs::IsCharacter(d->GetEntity()))
			continue;

		if (0 != g_stClientVersion.compare(d->GetClientVersion())) {
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(d->GetEntity(), CHAT_TYPE_INFO, 484, "");
#endif
			d->DelayedDisconnect(3);
		}
	}
}

void LoadStateUserCount()
{
	std::ifstream file("state_user_count");

	if (!file)
		return;

	file >> g_iFullUserCount >> g_iBusyUserCount;
}

bool IsValidProcessCRC(uint32_t dwCRC)
{
	return s_set_dwProcessCRC.find(dwCRC) != s_set_dwProcessCRC.end();
}

bool IsValidFileCRC(uint32_t dwCRC)
{
	return s_set_dwFileCRC.find(dwCRC) != s_set_dwFileCRC.end();
}

#ifdef ENABLE_MAP_TELEPORTER
void LoadMapConfig()
{
	g_vecMapConf.clear();
	
	const int phase_search_map		=	0;
	const int phase_read			=	1;


	auto funcSplitLine = [](std::string line, std::vector<std::string>& vec) {
		vec.clear();
		char	tok = '\t';
		size_t	pos = 0;

		while ((pos = line.find(tok)) != std::string::npos)
		{
			vec.emplace_back(line.substr(0,pos++));
			line = pos != line.length() ? line.substr(pos) : "";
		}

		if(!line.empty())
			vec.push_back(line);
	};


	auto funcTakeLines = [](std::string filename, std::vector<std::string>& vec) {
		vec.clear();

		std::ifstream file(filename);

		if (!file)
			return;

		std::string line;
		while (std::getline(file, line))
			vec.emplace_back(line);
	};


	int iPhase=0;
	TMapConfig map;

	std::vector<std::string> vecLines , vecToken;
	funcTakeLines(LocaleService_GetBasePath()+"/map/map_config.txt", vecLines);

	

	for(auto& line : vecLines)
	{
		if (iPhase == phase_search_map)
		{
			if (line.find('{') != std::string::npos)
			{
				iPhase++;
				map = {};
			}

			continue;
		}


		if (iPhase == phase_read)
		{
			if (line.find('}') != std::string::npos)
			{
				iPhase--;
				g_vecMapConf.push_back(map);
				continue;
			}
		}

		vecToken.clear();
		funcSplitLine(line, vecToken);

		if (vecToken.size() < 2)
			continue;

		if (vecToken[0] == "price")
		{
			map.price	= (uint32_t) atoll(vecToken[1].c_str());
			continue;
		}


		if (vecToken[0] == "items")
		{
			for (unsigned int i = 1; i < vecToken.size(); i++)
				map.items.emplace_back((uint32_t) atoll(vecToken[i].c_str()));
			continue;
		}


			// map.iMapIndex	= (int) atoll(vecToken[1].c_str());
		
		if (vecToken[0] == "coord_x")
		{
			map.coord_x	= (int) atoll(vecToken[1].c_str());
			continue;
		}
		
		if (vecToken[0] == "coord_y")
		{
			map.coord_y	= (int) atoll(vecToken[1].c_str());
			continue;
		}
		
		if(vecToken[0] == "level")
		{
			map.iLevel	= (int) atoll(vecToken[1].c_str());
			continue;
		}
		
		if(vecToken[0] == "levelMax")
		{
			map.iLevelMax	= (int) atoll(vecToken[1].c_str());
			continue;
		}
	}

	if (g_vecMapConf.empty())
	{
		LOG_ERROR("CANNOT LOAD MAP TELEPORTER CONFIG!!!");
		LOG_ERROR("CANNOT LOAD MAP TELEPORTER CONFIG!!!");
	}
		
}
#endif

