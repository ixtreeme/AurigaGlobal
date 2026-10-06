#include "stdafx.h"
#include "Peer.h"
#include "DBManager.h"
#include "ClientManager.h"
#include "DatabaseConfig.h"
#include "GuildManager.h"
#include "ItemAwardManager.h"
#include "PrivManager.h"
#include "MoneyLog.h"
#include "Marriage.h"
#include "ItemIDRangeManager.h"
#include <signal.h>
#include "Core/Logging.hpp"
#include <Core/peer_auth.h>

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

int g_iPlayerCacheFlushSeconds = 60*5;
int g_iItemCacheFlushSeconds = 60*5;

#ifdef ENABLE_BATTLE_PASS
int g_iCurrentBattlePassId = 1;
#endif

#ifdef __SKILL_COLOR_SYSTEM__
	int g_iSkillColorCacheFlushSeconds = 60*7;
#endif


int g_iLogoutSeconds = 60*10;

int g_log = 1;


// MYSHOP_PRICE_LIST
int g_iItemPriceListTableCacheFlushSeconds = 540;
// END_OF_MYSHOP_PRICE_LIST

#if defined(__FreeBSD__) && defined(__FreeBSD_version) && __FreeBSD_version<1000000
extern const char * _malloc_options;
#endif

void emergency_sig(int sig)
{
	if (sig == SIGSEGV)
		LOG_INFO("SIGNAL: SIGSEGV");
	else if (sig == SIGUSR1)
		LOG_INFO("SIGNAL: SIGUSR1");

	if (sig == SIGSEGV)
		abort();
}

namespace
{
	// Replaces the old CNetPoller lifetime: the core timer is torn down
	// after the manager objects, exactly as the poller destructor used to.
	struct ThecoreShutdownGuard
	{
		~ThecoreShutdownGuard() { thecore_destroy(); }
	};
}

int main()
{
#if defined(__FreeBSD__) && defined(__FreeBSD_version) && __FreeBSD_version<1000000
	_malloc_options = "A";
#endif

	ThecoreShutdownGuard thecoreShutdownGuard;
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

	// The game loop is over; join the dedicated io thread.
	auriga::net::Stop();

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
	if (!(pulse % heart->passes_per_sec))
	{
	}
}

//
//
int Start()
{
	if (!DbConfig().Load("conf.json"))
	{
		LOG_ERROR("Loading conf.json failed.");
		return false;
	}

	g_test_server = DbConfig().server.test ? TRUE : FALSE;
	LOG_ERROR(DbConfig().server.test ? "Test Server" : "Real Server");

	g_log = DbConfig().server.log ? 1 : 0;
	LOG_ERROR(g_log ? "Log On" : "Log Off");

	const int heart_beat = DbConfig().server.clientHeartFps;

	log_set_expiration_days(3);

	if (const int logKeepDays = DbConfig().server.logKeepDays; logKeepDays > 0)
	{
		const int effective = MINMAX(3, logKeepDays, 30);
		log_set_expiration_days(effective);
		LOG_ERROR("Setting log keeping days to {}", effective);
	}

	thecore_init(heart_beat, emptybeat);
	signal_timer_enable(60);

	g_stLocale = DbConfig().server.locale;
	LOG_INFO("LOCALE set to {}", g_stLocale.c_str());

	SetTablePostfix(DbConfig().server.tablePostfix.c_str());

	g_iPlayerCacheFlushSeconds = DbConfig().cache.playerFlushSeconds;
	LOG_INFO("PLAYER_CACHE_FLUSH_SECONDS: {}", g_iPlayerCacheFlushSeconds);

	g_iItemCacheFlushSeconds = DbConfig().cache.itemFlushSeconds;
	LOG_INFO("ITEM_CACHE_FLUSH_SECONDS: {}", g_iItemCacheFlushSeconds);

	g_iItemPriceListTableCacheFlushSeconds = DbConfig().cache.pricelistFlushSeconds;
	LOG_INFO("ITEM_PRICELIST_CACHE_FLUSH_SECONDS: {}", g_iItemPriceListTableCacheFlushSeconds);

	CClientManager::instance().SetCacheFlushCountLimit(DbConfig().cache.flushLimitPerSecond);

	if (DbConfig().server.playerIdStart == 0)
	{
		LOG_ERROR("PLAYER_ID_START not configured");
		return false;
	}

	CClientManager::instance().SetPlayerIDStart(static_cast<int>(DbConfig().server.playerIdStart));

	g_stLocaleNameColumn = DbConfig().server.nameColumn;
	LOG_ERROR("NAME_COLUMN {}", g_stLocaleNameColumn.c_str());

	if (!DbConfig().server.interserverKey.empty())
		PeerAuth::SetKey(DbConfig().server.interserverKey);

#ifdef ENABLE_BATTLE_PASS
	g_iCurrentBattlePassId = DbConfig().server.currentBattlePassId;
	LOG_INFO("CURRENT_BATTLE_PASS_ID: {}", g_iCurrentBattlePassId);
#endif

	auto connectSql = [](int slot, const SqlConnectionConfig& sql, const char* label)
	{
		LOG_INFO("connecting to MySQL server ({})", label);

		int iRetry = 5;

		do
		{
			if (CDBManager::instance().Connect(slot, sql.host.c_str(), sql.port, sql.database.c_str(), sql.user.c_str(), sql.password.c_str()))
			{
				LOG_INFO("   OK");
				return true;
			}

			LOG_INFO("   failed, retrying in 5 seconds");
			LOG_ERROR("   failed, retrying in 5 seconds");
			sleep(5);
		} while (iRetry--);

		return false;
	};

	if (!connectSql(SQL_PLAYER, DbConfig().sql.player, "player"))
	{
		LOG_ERROR("SQL_PLAYER not configured");
		return false;
	}

	SetPlayerDBName(DbConfig().sql.player.database.c_str());

	if (!connectSql(SQL_ACCOUNT, DbConfig().sql.account, "account"))
	{
		LOG_ERROR("SQL_ACCOUNT not configured");
		return false;
	}

	if (!connectSql(SQL_COMMON, DbConfig().sql.common, "common"))
	{
		LOG_ERROR("SQL_COMMON not configured");
		return false;
	}

	LOG_INFO("ClientManager initialization.. ");

	// Socket I/O runs on the dedicated engine thread from here on.
	auriga::net::Start();

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

