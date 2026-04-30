#include "stdafx.h"
#include "Config.h"
#include "Peer.h"
#include "DBManager.h"
#include "ClientManager.h"
#include "GuildManager.h"
#include "ItemAwardManager.h"
#include "PrivManager.h"
#include "MoneyLog.h"
#include "Marriage.h"
#include "ItemIDRangeManager.h"
#include <signal.h>
#include "Core/Logging.hpp"

#ifdef _WIN64
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "secur32.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ws2_32.lib")
#endif


void SetPlayerDBName(const char* c_pszPlayerDBName);
void SetTablePostfix(const char* c_pszTablePostfix);
int Start();

std::string g_stTablePostfix;
std::string g_stLocaleNameColumn = "name";
std::string g_stLocale = "utf8mb4"; // default: euckr
std::string g_stPlayerDBName = "";

BOOL g_test_server = false;

//단위 초
int g_iPlayerCacheFlushSeconds = 60*5;
int g_iItemCacheFlushSeconds = 60*5;

#ifdef ENABLE_BATTLE_PASS
int g_iCurrentBattlePassId = 1;
#endif

#ifdef __SKILL_COLOR_SYSTEM__
	int g_iSkillColorCacheFlushSeconds = 60*7;
#endif


//g_iLogoutSeconds 수치는 g_iPlayerCacheFlushSeconds 와 g_iItemCacheFlushSeconds 보다 길어야 한다.
int g_iLogoutSeconds = 60*10;

int g_log = 1;


// MYSHOP_PRICE_LIST
int g_iItemPriceListTableCacheFlushSeconds = 540;
// END_OF_MYSHOP_PRICE_LIST

#if defined(__FreeBSD__) && defined(__FreeBSD_version) && __FreeBSD_version<1000000
extern const char * _malloc_options;
#endif

extern void WriteVersion();

void emergency_sig(int sig)
{
	if (sig == SIGSEGV)
		LOG_INFO("SIGNAL: SIGSEGV");
	else if (sig == SIGUSR1)
		LOG_INFO("SIGNAL: SIGUSR1");

	if (sig == SIGSEGV)
		abort();
}

int main()
{
	//WriteVersion();

#if defined(__FreeBSD__) && defined(__FreeBSD_version) && __FreeBSD_version<1000000
	_malloc_options = "A";
#endif

	CConfig Config;
	CNetPoller poller;
	CDBManager DBManager;
	CClientManager ClientManager;
	CGuildManager GuildManager;
	CPrivManager PrivManager;
	CMoneyLog MoneyLog;
	ItemAwardManager ItemAwardManager;
	marriage::CManager MarriageManager;
	CItemIDRangeManager ItemIDRangeManager;
	if (!Start())
		return 1;

	GuildManager.Initialize();
	MarriageManager.Initialize();
	ItemIDRangeManager.Build();
	LOG_INFO("Metin2DBCacheServer Start\n");

	CClientManager::instance().MainLoop();

	signal_timer_disable();

	DBManager.Quit();
	int iCount;

	while (1)
	{
		iCount = 0;

		iCount += CDBManager::instance().CountReturnQuery(SQL_PLAYER);
		iCount += CDBManager::instance().CountAsyncQuery(SQL_PLAYER);

		if (iCount == 0)
			break;

		usleep(1000);
		LOG_INFO("WAITING_QUERY_COUNT {}", iCount);
	}

	return 1;
}

void emptybeat(LPHEART heart, int pulse)
{
	if (!(pulse % heart->passes_per_sec))	// 1초에 한번
	{
	}
}

//
// @version	05/06/13 Bang2ni - 아이템 가격정보 캐시 flush timeout 설정 추가.
//
int Start()
{
	if (!CConfig::instance().LoadFile("conf.txt"))
	{
		LOG_ERROR("Loading conf.txt failed.");
		return false;
	}

	if (!CConfig::instance().GetValue("TEST_SERVER", &g_test_server))
	{
		LOG_ERROR("Real Server");
	}
	else
		LOG_ERROR("Test Server");

	if (!CConfig::instance().GetValue("LOG", &g_log))
	{
		LOG_ERROR("Log Off");
		g_log= 0;
	}
	else
	{
		g_log = 1;
		LOG_ERROR("Log On");
	}


	int tmpValue;

	int heart_beat = 50;
	if (!CConfig::instance().GetValue("CLIENT_HEART_FPS", &heart_beat))
	{
		LOG_ERROR("Cannot find CLIENT_HEART_FPS configuration.");
		return false;
	}

	log_set_expiration_days(3);

	if (CConfig::instance().GetValue("LOG_KEEP_DAYS", &tmpValue))
	{
		tmpValue = MINMAX(3, tmpValue, 30);
		log_set_expiration_days(tmpValue);
		LOG_ERROR("Setting log keeping days to {}", tmpValue);
	}

	thecore_init(heart_beat, emptybeat);
	signal_timer_enable(60);

	char szBuf[256+1];

	if (CConfig::instance().GetValue("LOCALE", szBuf, 256))
	{
		g_stLocale = szBuf;
		LOG_INFO("LOCALE set to {}", g_stLocale.c_str());
	}

	if (!CConfig::instance().GetValue("TABLE_POSTFIX", szBuf, 256))
	{
		LOG_INFO("TABLE_POSTFIX not configured use default"); // @warme012
		szBuf[0] = '\0';
	}

	SetTablePostfix(szBuf);

	if (CConfig::instance().GetValue("PLAYER_CACHE_FLUSH_SECONDS", szBuf, 256))
	{
		str_to_number(g_iPlayerCacheFlushSeconds, szBuf);
		LOG_INFO("PLAYER_CACHE_FLUSH_SECONDS: {}", g_iPlayerCacheFlushSeconds);
	}

	if (CConfig::instance().GetValue("ITEM_CACHE_FLUSH_SECONDS", szBuf, 256))
	{
		str_to_number(g_iItemCacheFlushSeconds, szBuf);
		LOG_INFO("ITEM_CACHE_FLUSH_SECONDS: {}", g_iItemCacheFlushSeconds);
	}

	// MYSHOP_PRICE_LIST
	if (CConfig::instance().GetValue("ITEM_PRICELIST_CACHE_FLUSH_SECONDS", szBuf, 256))
	{
		str_to_number(g_iItemPriceListTableCacheFlushSeconds, szBuf);
		LOG_INFO("ITEM_PRICELIST_CACHE_FLUSH_SECONDS: {}", g_iItemPriceListTableCacheFlushSeconds);
	}
	// END_OF_MYSHOP_PRICE_LIST
	//
	if (CConfig::instance().GetValue("CACHE_FLUSH_LIMIT_PER_SECOND", szBuf, 256))
	{
		uint32_t dwVal = 0; str_to_number(dwVal, szBuf);
		CClientManager::instance().SetCacheFlushCountLimit(dwVal);
	}

	int iIDStart;
	if (!CConfig::instance().GetValue("PLAYER_ID_START", &iIDStart))
	{
		LOG_ERROR("PLAYER_ID_START not configured");
		return false;
	}

	CClientManager::instance().SetPlayerIDStart(iIDStart);

	if (CConfig::instance().GetValue("NAME_COLUMN", szBuf, 256))
	{
		LOG_ERROR("{} {}", g_stLocaleNameColumn.c_str(), szBuf);
		g_stLocaleNameColumn = szBuf;
	}

#ifdef ENABLE_BATTLE_PASS
	if (CConfig::instance().GetValue("CURRENT_BATTLE_PASS_ID", szBuf, 256))
	{
		str_to_number(g_iCurrentBattlePassId, szBuf);
		LOG_INFO("CURRENT_BATTLE_PASS_ID: {}", g_iCurrentBattlePassId);
	}
#endif

	char szAddr[64], szDB[64], szUser[64], szPassword[64];
	int iPort;
	char line[256+1];

	if (CConfig::instance().GetValue("SQL_PLAYER", line, 256))
	{
		sscanf(line, " %s %s %s %s %d ", szAddr, szDB, szUser, szPassword, &iPort);
		LOG_INFO("connecting to MySQL server (player)");

		int iRetry = 5;

		do
		{
			if (CDBManager::instance().Connect(SQL_PLAYER, szAddr, iPort, szDB, szUser, szPassword))
			{
				LOG_INFO("   OK");
				break;
			}

			LOG_INFO("   failed, retrying in 5 seconds");
			LOG_ERROR("   failed, retrying in 5 seconds");
			sleep(5);
		} while (iRetry--);
		LOG_ERROR("Success PLAYER");
		SetPlayerDBName(szDB);
	}
	else
	{
		LOG_ERROR("SQL_PLAYER not configured");
		return false;
	}

	if (CConfig::instance().GetValue("SQL_ACCOUNT", line, 256))
	{
		sscanf(line, " %s %s %s %s %d ", szAddr, szDB, szUser, szPassword, &iPort);
		LOG_INFO("connecting to MySQL server (account)");

		int iRetry = 5;

		do
		{
			if (CDBManager::instance().Connect(SQL_ACCOUNT, szAddr, iPort, szDB, szUser, szPassword))
			{
				LOG_INFO("   OK");
				break;
			}

			LOG_INFO("   failed, retrying in 5 seconds");
			LOG_ERROR("   failed, retrying in 5 seconds");
			sleep(5);
		} while (iRetry--);
		LOG_ERROR("Success ACCOUNT");
	}
	else
	{
		LOG_ERROR("SQL_ACCOUNT not configured");
		return false;
	}

	if (CConfig::instance().GetValue("SQL_COMMON", line, 256))
	{
		sscanf(line, " %s %s %s %s %d ", szAddr, szDB, szUser, szPassword, &iPort);
		LOG_INFO("connecting to MySQL server (common)");

		int iRetry = 5;

		do
		{
			if (CDBManager::instance().Connect(SQL_COMMON, szAddr, iPort, szDB, szUser, szPassword))
			{
				LOG_INFO("   OK");
				break;
			}

			LOG_INFO("   failed, retrying in 5 seconds");
			LOG_ERROR("   failed, retrying in 5 seconds");
			sleep(5);
		} while (iRetry--);
		LOG_ERROR("Success COMMON");
	}
	else
	{
		LOG_ERROR("SQL_COMMON not configured");
		return false;
	}

	/*if (CConfig::instance().GetValue("SQL_ITEMSHOP", line, 256))
	{
		sscanf(line, " %s %s %s %s %d ", szAddr, szDB, szUser, szPassword, &iPort);
		LOG_INFO("connecting to MySQL server (common)");

		int iRetry = 5;

		do
		{
			if (CDBManager::instance().Connect(SQL_ITEMSHOP, szAddr, iPort, szDB, szUser, szPassword))
			{
				LOG_INFO("   OK");
				break;
			}

			LOG_INFO("   failed, retrying in 5 seconds");
			LOG_INFO("   failed, retrying in 5 seconds");
			sleep(5);
		} while (iRetry--);
		LOG_INFO("Success ITEMSHOP");
	}
	else
	{
		LOG_ERROR("SQL_ITEMSHOP not configured");
		return false;
	}*/

	if (!CNetPoller::instance().Create())
	{
		LOG_ERROR("Cannot create network poller");
		return false;
	}

	LOG_INFO("ClientManager initialization.. ");

	if (!CClientManager::instance().Initialize())
	{
		LOG_INFO("   failed");
		return false;
	}

	LOG_INFO("   OK");

#ifndef _WIN32
	signal(SIGUSR1, emergency_sig);
#endif
	signal(SIGSEGV, emergency_sig);
	return true;
}

void SetTablePostfix(const char* c_pszTablePostfix)
{
	if (!c_pszTablePostfix || !*c_pszTablePostfix)
		g_stTablePostfix = "";
	else
		g_stTablePostfix = c_pszTablePostfix;
}

const char * GetTablePostfix()
{
	return g_stTablePostfix.c_str();
}

void SetPlayerDBName(const char* c_pszPlayerDBName)
{
	if (! c_pszPlayerDBName || ! *c_pszPlayerDBName)
		g_stPlayerDBName = "";
	else
	{
		g_stPlayerDBName = c_pszPlayerDBName;
		g_stPlayerDBName += ".";
	}
}

const char * GetPlayerDBName()
{
	return g_stPlayerDBName.c_str();
}

