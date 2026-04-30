#include "stdafx.h"
#include <sstream>
#ifndef _WIN32
#include <ifaddrs.h>
#endif

#include "constants.h"
#include "utils.h"
#include "log.h"
#include "desc.h"
#include "desc_manager.h"
#include "item_manager.h"
#include "p2p.h"
#include "char_interface.hpp"
#include "ip_ban.h"
#include "war_map.h"
#include "locale_service.h"
#include "config.h"
#include "dev_log.h"
#include <Core/Logging.hpp>
#include "db.h"
#include "skill_power.h"
#include "Core/Logging.hpp"
#include <fmt/format.h>

using std::string;

uint8_t	g_bChannel = 0;

uint16_t	mother_port = 50080;
int		passes_per_sec = 25;
uint16_t	db_port = 0;
uint16_t	p2p_port = 50900;
char	db_addr[ADDRESS_MAX_LEN + 1];
int		save_event_second_cycle = passes_per_sec * 120;	// 3분
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
uint8_t		g_bAuthServer = false;

bool		g_bCheckClientVersion = true;
string	g_stClientVersion = "20251204";

uint8_t		g_bBilling = false;

string	g_stAuthMasterIP;
uint16_t		g_wAuthMasterPort = 0;

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


//시야 = VIEW_RANGE + VIEW_BONUS_RANGE
//VIEW_BONUSE_RANGE : 클라이언트와 시야 처리에서너무 딱 떨어질경우 문제가 발생할수있어 500CM의 여분을 항상준다.
int VIEW_RANGE = 5000;
int VIEW_BONUS_RANGE = 500;

int g_server_id = 0;
string g_strWebMallURL = "https://www.wonder2.org/index.php/shop/login?pid=$playerID&key=$codice";

unsigned int g_uiSpamBlockDuration = 60 * 15; // 기본 15분
unsigned int g_uiSpamBlockScore = 100; // 기본 100점
unsigned int g_uiSpamReloadCycle = 60 * 10; // 기본 10분

int			g_iSpamBlockMaxLevel = 10;

void		LoadStateUserCount();
void		LoadValidCRCList();
bool		LoadClientVersion();
bool            g_protectNormalPlayer   = false;        // 범법자가 "평화모드" 인 일반유저를 공격하지 못함
bool            g_noticeBattleZone      = false;        // 중립지대에 입장하면 안내메세지를 알려줌

int gPlayerMaxLevel = 99;
int stone_chance = 30;
int gShutdownAge = 0;
int gShutdownEnable = 0;
#ifdef ENABLE_MAP_TELEPORTER
MAPCONFIG_VEC	g_vecMapConf;
void LoadMapConfig();
#endif
/*
 * NOTE : 핵 체크 On/Off. CheckIn할때 false로 수정했으면 반드시 확인하고 고쳐놓을것!
 * 이걸로 생길수있는 똥은 책임안짐 ~ ity ~
 */
bool gHackCheckEnable = false;

bool g_BlockCharCreation = false;
#ifdef __ATTR_TRANSFER_SYSTEM__
int	gAttrTransferLimit = 0;
#endif

//OPENID
int		openid_server = 0;
char	openid_host[256];
char	openid_uri[256];

bool is_string_true(const char * string)
{
	bool	result = 0;
	if (isnhdigit(*string))
	{
		str_to_number(result, string);
		return result > 0 ? true : false;
	}
	else if (LOWER(*string) == 't')
		return true;
	else
		return false;
}

static std::set<int> s_set_map_allows;

bool map_allow_find(int32_t index)
{
	if (g_bAuthServer)
		return false;

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

static bool __LoadConnectConfigFile(const char* configName)
	{
	char	buf[256];
	char	token_string[256];
	char	value_string[256];

	char db_host[2][64], db_user[2][64], db_pwd[2][64], db_db[2][64];
	// ... 아... db_port는 이미 있는데... 네이밍 어찌해야함...
	int mysql_db_port[2];

	for (int n = 0; n < 2; ++n)
	{
		*db_host[n]	= '\0';
		*db_user[n] = '\0';
		*db_pwd[n]= '\0';
		*db_db[n]= '\0';
		mysql_db_port[n] = 0;
	}

	char log_host[64], log_user[64], log_pwd[64], log_db[64];
	int log_port = 0;

	*log_host = '\0';
	*log_user = '\0';
	*log_pwd = '\0';
	*log_db = '\0';


	// DB에서 로케일정보를 세팅하기위해서는 다른 세팅값보다 선행되어서
	// DB정보만 읽어와 로케일 세팅을 한후 다른 세팅을 적용시켜야한다.
	// 이유는 로케일관련된 초기화 루틴이 곳곳에 존재하기 때문.

	bool isCommonSQL = false;
	bool isPlayerSQL = false;

	FILE* fpOnlyForDB;

	if (!(fpOnlyForDB = fopen(configName, "r")))
	{
		LOG_ERROR("Can not open [{}]", configName);
		exit(1);
	}

	while (fgets(buf, 256, fpOnlyForDB))
	{
		parse_token(buf, token_string, value_string);

		TOKEN("hostname")
		{
			g_stHostname = value_string;
			LOG_INFO("HOSTNAME: {}", g_stHostname.c_str());
			continue;
		}

		TOKEN("channel")
		{
			str_to_number(g_bChannel, value_string);
			continue;
		}

		TOKEN("player_sql")
		{
			const char * line = two_arguments(value_string, db_host[0], sizeof(db_host[0]), db_user[0], sizeof(db_user[0]));
			line = two_arguments(line, db_pwd[0], sizeof(db_pwd[0]), db_db[0], sizeof(db_db[0]));

			if ('\0' != line[0])
			{
				char buf[256];
				one_argument(line, buf, sizeof(buf));
				str_to_number(mysql_db_port[0], buf);
			}

			if (!*db_host[0] || !*db_user[0] || !*db_pwd[0] || !*db_db[0])
			{
				LOG_ERROR("PLAYER_SQL syntax: logsql <host user password db>");
				exit(1);
			}

			char buf[1024];
			snprintf(buf, sizeof(buf), "PLAYER_SQL: %s %s %s %s %d", db_host[0], db_user[0], db_pwd[0], db_db[0], mysql_db_port[0]);
			isPlayerSQL = true;
			continue;
		}

		TOKEN("common_sql")
		{
			const char * line = two_arguments(value_string, db_host[1], sizeof(db_host[1]), db_user[1], sizeof(db_user[1]));
			line = two_arguments(line, db_pwd[1], sizeof(db_pwd[1]), db_db[1], sizeof(db_db[1]));

			if ('\0' != line[0])
			{
				char buf[256];
				one_argument(line, buf, sizeof(buf));
				str_to_number(mysql_db_port[1], buf);
			}

			if (!*db_host[1] || !*db_user[1] || !*db_pwd[1] || !*db_db[1])
			{
				LOG_ERROR("COMMON_SQL syntax: logsql <host user password db>");
				exit(1);
			}

			char buf[1024];
			snprintf(buf, sizeof(buf), "COMMON_SQL: %s %s %s %s %d", db_host[1], db_user[1], db_pwd[1], db_db[1], mysql_db_port[1]);
			isCommonSQL = true;
			continue;
		}

		TOKEN("log_sql")
		{
			const char * line = two_arguments(value_string, log_host, sizeof(log_host), log_user, sizeof(log_user));
			line = two_arguments(line, log_pwd, sizeof(log_pwd), log_db, sizeof(log_db));

			if ('\0' != line[0])
			{
				char buf[256];
				one_argument(line, buf, sizeof(buf));
				str_to_number(log_port, buf);
			}

			if (!*log_host || !*log_user || !*log_pwd || !*log_db)
			{
				LOG_ERROR("LOG_SQL syntax: logsql <host user password db>");
				exit(1);
			}

			char buf[1024];
			snprintf(buf, sizeof(buf), "LOG_SQL: %s %s %s %s %d", log_host, log_user, log_pwd, log_db, log_port);
			continue;
		}
		TOKEN("itemshop_sql")
		{
			const char* line = two_arguments(value_string, log_host, sizeof(log_host), log_user, sizeof(log_user));
			line = two_arguments(line, log_pwd, sizeof(log_pwd), log_db, sizeof(log_db));

			if ('\0' != line[0])
			{
				char buf[256];
				one_argument(line, buf, sizeof(buf));
				str_to_number(log_port, buf);
			}

			if (!*log_host || !*log_user || !*log_pwd || !*log_db)
			{
				LOG_ERROR("ITEMSHOP_SQL syntax: logsql <host user password db>");
				exit(1);
			}

			char buf[1024];
			snprintf(buf, sizeof(buf), "ITEMSHOP_SQL: %s %s %s %s %d", log_host, log_user, log_pwd, log_db, log_port);
			continue;
		}
	}

	//처리가 끝났으니 파일을 닫자.
	fclose(fpOnlyForDB);

	// CONFIG_SQL_INFO_ERROR
	if (!isCommonSQL)
	{
		LOG_INFO("LOAD_COMMON_SQL_INFO_FAILURE:");
		LOG_INFO("");
		LOG_INFO("CONFIG:");
		LOG_INFO("------------------------------------------------");
		LOG_INFO("COMMON_SQL: HOST USER PASSWORD DATABASE");
		LOG_INFO("");
		exit(1);
	}

	if (!isPlayerSQL)
	{
		LOG_INFO("LOAD_PLAYER_SQL_INFO_FAILURE:");
		LOG_INFO("");
		LOG_INFO("CONFIG:");
		LOG_INFO("------------------------------------------------");
		LOG_INFO("PLAYER_SQL: HOST USER PASSWORD DATABASE");
		LOG_INFO("");
		exit(1);
	}

	// Common DB 가 Locale 정보를 가지고 있기 때문에 가장 먼저 접속해야 한다.
	AccountDB::instance().Connect(db_host[1], mysql_db_port[1], db_user[1], db_pwd[1], db_db[1]);

	if (false == AccountDB::instance().IsConnected())
	{
		LOG_ERROR("cannot start server while no common sql connected");
		exit(1);
	}

	LOG_INFO("CommonSQL connected");

	// 로케일 정보를 가져오자
	// <경고> 쿼리문에 절대 조건문(WHERE) 달지 마세요. (다른 지역에서 문제가 생길수 있습니다)
	{
		char szQuery[512];
		snprintf(szQuery, sizeof(szQuery), "SELECT mKey, mValue FROM locale");

		std::unique_ptr<SQLMsg> pMsg(AccountDB::instance().DirectQuery(szQuery));

		if (pMsg->Get()->uiNumRows == 0)
		{
			LOG_ERROR("COMMON_SQL: DirectQuery failed : {}", szQuery);
			exit(1);
		}

		MYSQL_ROW row;

		while (nullptr != (row = mysql_fetch_row(pMsg->Get()->pSQLResult)))
		{
			// 로케일 세팅
			if (strcasecmp(row[0], "LOCALE") == 0)
			{
				if (LocaleService_Init(row[1]) == false)
				{
					LOG_ERROR("COMMON_SQL: invalid locale key {}", row[1]);
					exit(1);
				}
			}
		}
	}

	// 로케일 정보를 COMMON SQL에 세팅해준다.
	// 참고로 g_stLocale 정보는 LocaleService_Init() 내부에서 세팅된다.
	LOG_INFO("Setting DB to locale {}", g_stLocale.c_str());

	AccountDB::instance().SetLocale(g_stLocale);

	AccountDB::instance().ConnectAsync(db_host[1], mysql_db_port[1], db_user[1], db_pwd[1], db_db[1], g_stLocale.c_str());

	// Player DB 접속
	DBManager::instance().Connect(db_host[0], mysql_db_port[0], db_user[0], db_pwd[0], db_db[0]);

	if (!DBManager::instance().IsConnected())
	{
		LOG_ERROR("PlayerSQL.ConnectError");
		exit(1);
	}

	LOG_INFO("PlayerSQL connected");

	if (false == g_bAuthServer) // 인증 서버가 아닐 경우
	{
		// Log DB 접속
		LogManager::instance().Connect(log_host, log_port, log_user, log_pwd, log_db);

		if (!LogManager::instance().IsConnected())
		{
			LOG_ERROR("LogSQL.ConnectError");
			exit(1);
		}

		LOG_INFO("LogSQL connected");

		LogManager::instance().BootLog(g_stHostname.c_str(), g_bChannel);
	}

	// SKILL_POWER_BY_LEVEL
	// 스트링 비교의 문제로 인해서 AccountDB::instance().SetLocale(g_stLocale) 후부터 한다.
	// 물론 국내는 별로 문제가 안된다(해외가 문제)
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

		row = mysql_fetch_row(pMsg->Get()->pSQLResult);

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

		// 종족별 스킬 세팅
		for (int job = 0; job < JOB_MAX_NUM * 2; ++job)
		{
			snprintf(szQuery, sizeof(szQuery), "SELECT mValue from locale where mKey='SKILL_POWER_BY_LEVEL_TYPE%d' ORDER BY CAST(mValue AS unsigned)", job);
			std::unique_ptr<SQLMsg> pMsg(AccountDB::instance().DirectQuery(szQuery));

			// 세팅이 안되어있으면 기본테이블을 사용한다.
			if (pMsg->Get()->uiNumRows == 0)
			{
				CTableBySkill::instance().SetSkillPowerByLevelFromType(job, aiBaseSkillPowerByLevelTable);
				continue;
			}

			row = mysql_fetch_row(pMsg->Get()->pSQLResult);
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
	return true;
}

static bool __LoadDefaultConfigFile(const char* configName)
{
	FILE	*fp;

	char	buf[256];
	char	token_string[256];
	char	value_string[256];

	if (!(fp = fopen(configName, "r")))
		return false;

	while (fgets(buf, 256, fp))
	{
		parse_token(buf, token_string, value_string);

		TOKEN("port")
		{
			str_to_number(mother_port, value_string);
			continue;
		}

		TOKEN("p2p_port")
		{
			str_to_number(p2p_port, value_string);
			continue;
		}

		TOKEN("map_allow")
		{
			char * p = value_string;
			string stNum;

			for (; *p; p++)
			{
				if (isnhspace(*p))
				{
					if (!stNum.empty())
					{
						int32_t	index = 0;
						str_to_number(index, stNum.c_str());
						map_allow_add(index);
						stNum.clear();
					}
				}
				else
					stNum += *p;
			}

			if (!stNum.empty())
		{
				int32_t	index = 0;
				str_to_number(index, stNum.c_str());
				map_allow_add(index);
			}

			continue;
		}

		TOKEN("auth_server")
		{
			char szIP[32];
			char szPort[32];

			two_arguments(value_string, szIP, sizeof(szIP), szPort, sizeof(szPort));

			if (!*szIP || (!*szPort && strcasecmp(szIP, "master")))
			{
				LOG_ERROR("AUTH_SERVER: syntax error: <ip|master> <port>");
				exit(1);
			}

			g_bAuthServer = true;

			LoadBanIP("BANIP");

			if (!strcasecmp(szIP, "master"))
				LOG_INFO("AUTH_SERVER: I am the master");
			else
		{
				g_stAuthMasterIP = szIP;
				str_to_number(g_wAuthMasterPort, szPort);

				LOG_INFO("AUTH_SERVER: master {} {}", g_stAuthMasterIP.c_str(), g_wAuthMasterPort);
			}
			continue;
		}
	}

	fclose(fp);
	return true;
}

static bool __LoadGeneralConfigFile(const char* configName)
{
	FILE	*fp;

	char	buf[256];
	char	token_string[256];
	char	value_string[256];

	if (!(fp = fopen(configName, "r")))
		return false;

	while (fgets(buf, 256, fp))
	{
		parse_token(buf, token_string, value_string);

		//OPENID
		TOKEN("WEB_AUTH")
		{
			two_arguments(value_string, openid_host, sizeof(openid_host), openid_uri, sizeof(openid_uri));

			if (!*openid_host || !*openid_uri)
			{
				LOG_ERROR("WEB_AUTH syntax error (ex: WEB_AUTH <host(metin2.co.kr) uri(/kyw/gameauth.php)>");
				exit(1);
			}

			char buf[1024];
			openid_server = 1;
			snprintf(buf, sizeof(buf), "WEB_AUTH: %s %s", openid_host, openid_uri);
			continue;
		}

		// DB_ONLY_BEGIN
		TOKEN("BLOCK_LOGIN")
		{
			g_stBlockDate = value_string;
		}
		// DB_ONLY_END

		// CONNECTION_BEGIN
		TOKEN("db_port")
		{
			str_to_number(db_port, value_string);
			continue;
		}

		TOKEN("db_addr")
		{
			strlcpy(db_addr, value_string, sizeof(db_addr));

			for (int n =0; n < ADDRESS_MAX_LEN; ++n)
			{
				if (db_addr[n] == ' ')
					db_addr[n] = '\0';
			}

			continue;
		}
		// CONNECTION_END

		TOKEN("empire_whisper")
		{
			bool b_value = 0;
			str_to_number(b_value, value_string);
			g_bEmpireWhisper = !!b_value;
			continue;
		}

		TOKEN("mark_server")
		{
			guild_mark_server = is_string_true(value_string);
			continue;
		}

		TOKEN("mark_min_level")
		{
			str_to_number(guild_mark_min_level, value_string);
			guild_mark_min_level = MINMAX(0, guild_mark_min_level, GUILD_MAX_LEVEL);
			continue;
		}

		TOKEN("log_keep_days")
		{
			int i = 0;
			str_to_number(i, value_string);
			log_set_expiration_days(MINMAX(1, i, 90));
			continue;
		}

		TOKEN("passes_per_sec")
		{
			str_to_number(passes_per_sec, value_string);
			continue;
		}

		TOKEN("save_event_second_cycle")
		{
			int	cycle = 0;
			str_to_number(cycle, value_string);
			save_event_second_cycle = cycle * passes_per_sec;
			continue;
		}

		TOKEN("ping_event_second_cycle")
		{
			int	cycle = 0;
			str_to_number(cycle, value_string);
			ping_event_second_cycle = cycle * passes_per_sec;
			continue;
		}

		TOKEN("table_postfix")
		{
			g_table_postfix = value_string;
			continue;
		}

		TOKEN("test_server")
		{
			LOG_INFO("-----------------------------------------------");
			LOG_INFO("TEST_SERVER");
			LOG_INFO("-----------------------------------------------");
			str_to_number(test_server, value_string);
			continue;
		}

		TOKEN("shutdowned")
		{
			g_bNoMoreClient = true;
			continue;
		}

		TOKEN("no_regen")
		{
			g_bNoRegen = true;
			continue;
		}

#ifdef ENABLE_NEWSTUFF
		TOKEN("item_count_limit")
		{
			str_to_number(g_bItemCountLimit, value_string);
			LOG_INFO("ITEM_COUNT_LIMIT: {}", g_bItemCountLimit);
			continue;
		}

		TOKEN("disable_shop_price_3x")
		{
			g_bEmpireShopPriceTripleDisable = true;
			LOG_INFO("EMPIRE_SHOP_PRICE_3x: DISABLED");
			continue;
		}

		TOKEN("shop_price_3x_tax") //alternative
		{
			int flag = 0;
			str_to_number(flag, value_string);
			g_bEmpireShopPriceTripleDisable = !flag;
			LOG_INFO("SHOP_PRICE_3X_TAX: {}", (!g_bEmpireShopPriceTripleDisable)?"ENABLED":"DISABLED");
			continue;
		}

		//unused
		TOKEN("enable_shout_addon")
		{
			g_bShoutAddonEnable = true;
			continue;
		}

		//unused
		TOKEN("enable_all_mount_attack")
		{
			g_bAllMountAttack = true;
			continue;
		}

		TOKEN("disable_change_attr_time")
		{
			g_dwItemBonusChangeTime = 0;
			LOG_INFO("CHANGE_ATTR_TIME_LIMIT: DISABLED");
			continue;
		}

		TOKEN("change_attr_time_limit") //alternative
		{
			uint32_t flag = 0;
			str_to_number(flag, value_string);
			g_dwItemBonusChangeTime = flag;
			LOG_INFO("CHANGE_ATTR_TIME_LIMIT: {}", g_dwItemBonusChangeTime);
			continue;
		}

		TOKEN("disable_prism_item")
		{
			g_bDisablePrismNeed = true;
			LOG_INFO("PRISM_ITEM_REQUIREMENT: DISABLED");
			continue;
		}

		TOKEN("prism_item_require") //alternative
		{
			int flag = 0;
			str_to_number(flag, value_string);
			g_bDisablePrismNeed = !flag;
			LOG_INFO("PRISM_ITEM_REQUIRE: {}", (!g_bDisablePrismNeed)?"ENABLED":"DISABLED");
			continue;
		}

		TOKEN("enable_global_shout")
		{
			g_bGlobalShoutEnable = true;
			LOG_INFO("GLOBAL_SHOUT: ENABLED");
			continue;
		}

		TOKEN("global_shout") //alternative
		{
			int flag = 0;
			str_to_number(flag, value_string);
			g_bGlobalShoutEnable = !!flag;
			LOG_INFO("GLOBAL_SHOUT: {}", (g_bGlobalShoutEnable)?"ENABLED":"DISABLED");
			continue;
		}

		TOKEN("disable_emotion_mask")
		{
			g_bDisableEmotionMask = true;
			LOG_INFO("EMOTION_MASK_REQUIREMENT: DISABLED");
			continue;
		}

		TOKEN("emotion_mask_require") //alternative
		{
			int flag = 0;
			str_to_number(flag, value_string);
			g_bDisableEmotionMask = !flag;
			LOG_INFO("EMOTION_MASK_REQUIRE: {}", (g_bDisableEmotionMask)?"ENABLED":"DISABLED");
			continue;
		}

		TOKEN("enable_bootary_check")
		{
			g_bEnableBootaryCheck = true;
			LOG_INFO("ENABLE_BOOTARY_CHECK: ENABLED");
			continue;
		}

		TOKEN("bootary_check") //alternative
		{
			int flag = 0;
			str_to_number(flag, value_string);
			g_bEnableBootaryCheck = !!flag;
			LOG_INFO("BOOTARY_CHECK: {}", (g_bEnableBootaryCheck)?"ENABLED":"DISABLED");
			continue;
		}

		TOKEN("status_point_get_level_limit")
		{
			int flag = 0;
			str_to_number(flag, value_string);
			if (flag <= 0) continue;

			g_iStatusPointGetLevelLimit = MINMAX(0, flag, PLAYER_MAX_LEVEL_CONST);
			LOG_INFO("STATUS_POINT_GET_LEVEL_LIMIT: {}", g_iStatusPointGetLevelLimit);
			continue;
		}

		TOKEN("status_point_set_max_value")
		{
			int flag = 0;
			str_to_number(flag, value_string);
			if (flag <= 0) continue;

			g_iStatusPointSetMaxValue = flag;
			LOG_INFO("STATUS_POINT_SET_MAX_VALUE: {}", g_iStatusPointSetMaxValue);
			continue;
		}

		TOKEN("shout_limit_level")
		{
			int flag = 0;
			str_to_number(flag, value_string);
			if (flag <= 0) continue;

			g_iShoutLimitLevel = flag;
			LOG_INFO("SHOUT_LIMIT_LEVEL: {}", g_iShoutLimitLevel);
			continue;
		}

		TOKEN("db_log_level")
		{
			int flag = 0;
			str_to_number(flag, value_string);

			g_iDbLogLevel = flag;
			LOG_INFO("DB_LOG_LEVEL: {}", g_iDbLogLevel);
			continue;
		}

		TOKEN("sys_log_level")
		{
			int flag = 0;
			str_to_number(flag, value_string);

			g_iSysLogLevel = flag;
			LOG_INFO("SYS_LOG_LEVEL: {}", g_iSysLogLevel);
			continue;
		}

		TOKEN("item_destroy_time_autogive")
		{
			int flag = 0;
			str_to_number(flag, value_string);

			g_aiItemDestroyTime[ITEM_DESTROY_TIME_AUTOGIVE] = flag;
			LOG_INFO("ITEM_DESTROY_TIME_AUTOGIVE: {}", g_aiItemDestroyTime[ITEM_DESTROY_TIME_AUTOGIVE]);
			continue;
		}

		TOKEN("item_destroy_time_dropgold")
		{
			int flag = 0;
			str_to_number(flag, value_string);

			g_aiItemDestroyTime[ITEM_DESTROY_TIME_DROPGOLD] = flag;
			LOG_INFO("ITEM_DESTROY_TIME_DROPGOLD: {}", g_aiItemDestroyTime[ITEM_DESTROY_TIME_DROPGOLD]);
			continue;
		}

		TOKEN("item_destroy_time_dropitem")
		{
			int flag = 0;
			str_to_number(flag, value_string);

			g_aiItemDestroyTime[ITEM_DESTROY_TIME_DROPITEM] = flag;
			LOG_INFO("ITEM_DESTROY_TIME_DROPITEM: {}", g_aiItemDestroyTime[ITEM_DESTROY_TIME_DROPITEM]);
			continue;
		}

		// TOKEN("shout_limit_time")
		// {
			// int flag = 0;
			// str_to_number(flag, value_string);
			// if (flag <= 0) continue;

			// g_iShoutLimitTime = flag;
			// LOG_INFO("SHOUT_LIMIT_TIME: {}", g_iShoutLimitTime);
			// continue;
		// }

		TOKEN("check_version_server")
		{
			int flag = 0;

			str_to_number(flag, value_string);
			g_bCheckClientVersion = !!flag;
			LOG_INFO("CHECK_VERSION_SERVER: {}", g_bCheckClientVersion);
			continue;
		}

		TOKEN("check_version_value")
		{
			g_stClientVersion = value_string;
			LOG_INFO("CHECK_VERSION_VALUE: {}", g_stClientVersion.c_str());
			continue;
		}

		TOKEN("gm_host_check")
		{
			uint32_t flag = 0;
			str_to_number(flag, value_string);

			g_bGMHostCheck = !!flag;
			LOG_INFO("GM_HOST_CHECK: {}", g_bGMHostCheck);
			continue;
		}

		TOKEN("guild_invite_limit")
		{
			uint32_t flag = 0;
			str_to_number(flag, value_string);

			g_bGuildInviteLimit = !!flag;
			LOG_INFO("GUILD_INVITE_LIMIT: {}", g_bGuildInviteLimit);
			continue;
		}

		TOKEN("guild_infinite_members")
		{
			uint32_t flag = 0;
			str_to_number(flag, value_string);

			g_bGuildInfiniteMembers = !!flag;
			LOG_INFO("GUILD_INFINITE_MEMBERS: {}", g_bGuildInfiniteMembers);
			continue;
		}

		TOKEN("empire_language_check")
		{
			int flag = 0;
			str_to_number(flag, value_string);
			g_bDisableEmpireLanguageCheck = !flag;
			LOG_INFO("EMPIRE_LANGUAGE_CHECK: {}", (g_bDisableEmpireLanguageCheck)?"DISABLED":"ENABLED");
			continue;
		}

		TOKEN("skillbook_nextread_min")
		{
			uint32_t flag = 0;
			str_to_number(flag, value_string);
			g_dwSkillBookNextReadMin = flag;
			LOG_INFO("SKILLBOOK_NEXTREAD_MIN: {}", g_dwSkillBookNextReadMin);
			continue;
		}

		TOKEN("skillbook_nextread_max")
		{
			uint32_t flag = 0;
			str_to_number(flag, value_string);
			g_dwSkillBookNextReadMax = flag;
			LOG_INFO("SKILLBOOK_NEXTREAD_MAX: {}", g_dwSkillBookNextReadMax);
			continue;
		}
#endif

		TOKEN("traffic_profile")
		{
			g_bTrafficProfileOn = true;
			continue;
		}

		TOKEN("no_wander")
		{
			no_wander = true;
			continue;
		}

		TOKEN("user_limit")
		{
			str_to_number(g_iUserLimit, value_string);
			continue;
		}

		TOKEN("skill_disable")
		{
			str_to_number(g_bSkillDisable, value_string);
			continue;
		}

		TOKEN("billing")
		{
			g_bBilling = true;
		}

		TOKEN("quest_dir")
		{
			LOG_INFO("QUEST_DIR SETTING : {}", value_string);
			g_stQuestDir = value_string;
		}

		TOKEN("quest_object_dir")
		{
			//g_stQuestObjectDir = value_string;
			std::istringstream is(value_string);
			LOG_INFO("QUEST_OBJECT_DIR SETTING : {}", value_string);
			string dir;
			while (!is.eof())
			{
				is >> dir;
				if (is.fail())
					break;
				g_setQuestObjectDir.insert(dir);
				LOG_INFO("QUEST_OBJECT_DIR INSERT : {}", dir .c_str());
			}
		}

		TOKEN("server_id")
		{
			str_to_number(g_server_id, value_string);
		}

		TOKEN("mall_url")
		{
			g_strWebMallURL = value_string;
		}

		TOKEN("bind_ip")
		{
			strlcpy(g_szPublicIP, value_string, sizeof(g_szPublicIP));
		}

		TOKEN("view_range")
		{
			str_to_number(VIEW_RANGE, value_string);
		}

		TOKEN("spam_block_duration")
		{
			str_to_number(g_uiSpamBlockDuration, value_string);
		}

		TOKEN("spam_block_score")
		{
			str_to_number(g_uiSpamBlockScore, value_string);
			g_uiSpamBlockScore = MAX(1, g_uiSpamBlockScore);
		}

		TOKEN("spam_block_reload_cycle")
		{
			str_to_number(g_uiSpamReloadCycle, value_string);
			g_uiSpamReloadCycle = MAX(60, g_uiSpamReloadCycle); // 최소 1분
		}

		TOKEN("spam_block_max_level")
		{
			str_to_number(g_iSpamBlockMaxLevel, value_string);
		}
		TOKEN("protect_normal_player")
		{
			str_to_number(g_protectNormalPlayer, value_string);
		}
		TOKEN("notice_battle_zone")
		{
			str_to_number(g_noticeBattleZone, value_string);
		}

		TOKEN("pk_protect_level")
		{
		    str_to_number(PK_PROTECT_LEVEL, value_string);
		    LOG_ERROR("PK_PROTECT_LEVEL: {}", PK_PROTECT_LEVEL);
		}
		
#ifdef __ATTR_TRANSFER_SYSTEM__
		TOKEN("ATTR_TRANSFER_LIMIT")
		{
			str_to_number(gAttrTransferLimit, value_string);
			LOG_ERROR("ATTR_TRANSFER_LIMIT: {}", gAttrTransferLimit);
			continue;
		}
#endif
		
		TOKEN("max_level")
		{
			str_to_number(gPlayerMaxLevel, value_string);

			gPlayerMaxLevel = MINMAX(1, gPlayerMaxLevel, PLAYER_MAX_LEVEL_CONST);

			LOG_ERROR("PLAYER_MAX_LEVEL: {}", gPlayerMaxLevel);
		}
		
		TOKEN("stone_chance")
		{
			str_to_number(stone_chance, value_string);
			
			stone_chance = MINMAX(1, stone_chance, 100);
			
			LOG_ERROR("STONE_CHANCE: {}/n", stone_chance);
		}
		
		TOKEN("shutdown_age")
		{
			str_to_number(gShutdownAge, value_string);
			LOG_ERROR("SHUTDOWN_AGE: {}", gShutdownAge);

		}

		TOKEN("shutdown_enable")
		{
			str_to_number(gShutdownEnable, value_string);
			LOG_ERROR("SHUTDOWN_ENABLE: {}", gShutdownEnable);
		}

		TOKEN("block_char_creation")
		{
			int tmp = 0;

			str_to_number(tmp, value_string);

			if (0 == tmp)
				g_BlockCharCreation = false;
			else
				g_BlockCharCreation = true;

			continue;
		}
	}
	fclose(fp);
	return true;
	}

#define ENABLE_CMD_PLAYER
static bool __LoadDefaultCMDFile(const char* cmdName)
	{
	FILE	*fp;
	char	buf[256];

	if ((fp = fopen(cmdName, "r")))
	{
		while (fgets(buf, 256, fp))
		{
			char cmd[32], levelname[32];
			int level;

			two_arguments(buf, cmd, sizeof(cmd), levelname, sizeof(levelname));

			if (!*cmd || !*levelname)
			{
#ifdef ENABLE_CMD_PLAYER
				LOG_ERROR("CMD syntax error: <cmd> <PLAYER | LOW_WIZARD | WIZARD | HIGH_WIZARD | GOD | IMPLEMENTOR | DISABLE>");
#else
				LOG_ERROR("CMD syntax error: <cmd> <LOW_WIZARD | WIZARD | HIGH_WIZARD | GOD | IMPLEMENTOR | DISABLE>");
#endif
				exit(1);
			}

			if (!strcasecmp(levelname, "LOW_WIZARD"))
				level = GM_LOW_WIZARD;
			else if (!strcasecmp(levelname, "WIZARD"))
				level = GM_WIZARD;
			else if (!strcasecmp(levelname, "HIGH_WIZARD"))
				level = GM_HIGH_WIZARD;
			else if (!strcasecmp(levelname, "GOD"))
				level = GM_GOD;
			else if (!strcasecmp(levelname, "IMPLEMENTOR"))
				level = GM_IMPLEMENTOR;
#ifdef ENABLE_CMD_PLAYER
			else if (!strcasecmp(levelname, "PLAYER"))
				level = GM_PLAYER;
#endif
			else if (!strcasecmp(levelname, "DISABLE"))
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
			interpreter_set_privilege(cmd, level);
		}

		fclose(fp);
		return true;
	}
	return false;
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
	while ((row = mysql_fetch_row(pMsg->Get()->pSQLResult)))
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

// #define ENABLE_GENERAL_CMD
// #define ENABLE_GENERAL_CONFIG
void config_init(const string& st_localeServiceName)
{
	// LOCALE_SERVICE
	string	st_configFileName;

	st_configFileName.reserve(32);
	st_configFileName = "CONFIG";

	if (!st_localeServiceName.empty())
	{
		st_configFileName += ".";
		st_configFileName += st_localeServiceName;
	}
	// END_OF_LOCALE_SERVICE

	// public ip가 없어도 BIND_IP하면 게임 돌아가는데에는 아무런 지장이 없기 때문에
	// 주석처리 함.
	if (!GetIPInfo())
	{
	//	LOG_ERROR("Can not get public ip address");
	//	exit(1);
	}

	// default config load (REQUIRED)
	if (!__LoadConnectConfigFile(st_configFileName.c_str()) ||
		!__LoadDefaultConfigFile(st_configFileName.c_str()) ||
		!__LoadGeneralConfigFile(st_configFileName.c_str())
	)
	{
		LOG_ERROR("Can not open [{}]", st_configFileName.c_str());
		exit(1);
	}
#ifdef ENABLE_GENERAL_CONFIG
	// general config - locale based
	{
		char szFileName[256];
		snprintf(szFileName, sizeof(szFileName), "%s/conf/GENERAL_%s", LocaleService_GetBasePath().c_str(), st_configFileName.c_str());
		if (__LoadGeneralConfigFile(szFileName))
			LOG_ERROR("GENERAL CONFIG LOAD OK [{}]", szFileName);
	}
	// general config - locale n channel based
	{
		char szFileName[256];
		snprintf(szFileName, sizeof(szFileName), "%s/conf/GENERAL_%s_CHANNEL_%d", LocaleService_GetBasePath().c_str(), st_configFileName.c_str(), g_bChannel);
		if (__LoadGeneralConfigFile(szFileName))
			LOG_ERROR("GENERAL CONFIG LOAD OK [{}]", szFileName);
	}
	// general config - locale n channel n hostname based
	{
		char szFileName[256];
		snprintf(szFileName, sizeof(szFileName), "%s/conf/GENERAL_%s_CHANNEL_%d_HOSTNAME_%s", LocaleService_GetBasePath().c_str(), st_configFileName.c_str(), g_bChannel, g_stHostname.c_str());
		if (__LoadGeneralConfigFile(szFileName))
			LOG_ERROR("GENERAL CONFIG LOAD OK [{}]", szFileName);
	}
#endif

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
#ifdef ENABLE_GENERAL_CMD
	// general cmd - locale based
	{
		char szFileName[256];
		snprintf(szFileName, sizeof(szFileName), "%s/conf/GENERAL_%s", LocaleService_GetBasePath().c_str(), st_cmdFileName.c_str());
		if (__LoadDefaultCMDFile(szFileName))
			LOG_INFO("GENERAL CMD LOAD OK [{}]", szFileName);
	}
	// general cmd - locale n channel based
	{
		char szFileName[256];
		snprintf(szFileName, sizeof(szFileName), "%s/conf/GENERAL_%s_CHANNEL_%d", LocaleService_GetBasePath().c_str(), st_cmdFileName.c_str(), g_bChannel);
		if (__LoadDefaultCMDFile(szFileName))
			LOG_INFO("GENERAL CMD LOAD OK [{}]", szFileName);
	}
	// general cmd - locale n channel n hostname based
	{
		char szFileName[256];
		snprintf(szFileName, sizeof(szFileName), "%s/conf/GENERAL_%s_CHANNEL_%d_HOSTNAME_%s", LocaleService_GetBasePath().c_str(), st_cmdFileName.c_str(), g_bChannel, g_stHostname.c_str());
		if (__LoadDefaultCMDFile(szFileName))
			LOG_INFO("GENERAL CMD LOAD OK [{}]", szFileName);
	}
#endif

	//if(!gHackCheckEnable)	// Hack 체크가 비활성화인 경우
	//{
	//	assert(test_server);	// 테스트 서버가 아니라면 assert
	//}

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

	FILE * fp;
	char buf[256];

	if ((fp = fopen("CRC", "r")))
	{
		while (fgets(buf, 256, fp))
		{
			if (!*buf)
				continue;

			uint32_t dwValidClientProcessCRC;
			uint32_t dwValidClientFileCRC;

			sscanf(buf, " %u %u ", &dwValidClientProcessCRC, &dwValidClientFileCRC);

			s_set_dwProcessCRC.insert(dwValidClientProcessCRC);
			s_set_dwFileCRC.insert(dwValidClientFileCRC);

			LOG_ERROR("CLIENT_CRC: {} {}", dwValidClientProcessCRC, dwValidClientFileCRC);
		}

		fclose(fp);
	}
}

bool LoadClientVersion()
{
	FILE * fp = fopen("VERSION", "r");

	if (!fp)
		return false;

	char buf[256];
	fgets(buf, 256, fp);

	char * p = strchr(buf, '\n');
	if (p) *p = '\0';

	LOG_ERROR("VERSION: \"{}\"", buf);

	g_stClientVersion = buf;
	fclose(fp);
	return true;
}

void CheckClientVersion()
{
	const DESC_MANAGER::DESC_SET & set = DESC_MANAGER::instance().GetClientSet();
	DESC_MANAGER::DESC_SET::const_iterator it = set.begin();

	while (it != set.end())
	{
		LPDESC d = *(it++);

		if (!d->GetCharacter())
			continue;

		if (0 != g_stClientVersion.compare(d->GetClientVersion())) {
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(d->GetCharacter()), CHAT_TYPE_INFO, 484, "");
#endif
			d->DelayedDisconnect(3);
		}
	}
}

void LoadStateUserCount()
{
	FILE * fp = fopen("state_user_count", "r");

	if (!fp)
		return;

	fscanf(fp, " %d %d ", &g_iFullUserCount, &g_iBusyUserCount);

	fclose(fp);
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

		FILE * pFile = fopen( filename.c_str() , "r" );
		if(!pFile)
			return;

		char szLine[256]="\0";
		while (fgets(szLine, sizeof(szLine), pFile))
			vec.emplace_back(std::string(szLine));

		fclose(pFile);
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


		// if (vecToken[0] == "index")
		// {
			// map.iMapIndex	= (int) atoll(vecToken[1].c_str());
			// continue;
		// }
		
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

