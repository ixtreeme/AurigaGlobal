#include "stdafx.h"
#include <Core/Logging.hpp>
#include "constants.h"
#include "config.h"
#include "event.h"
#include "minilzo.h"
#include "packet.h"
#include "desc_manager.h"
#include "item_manager.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "mob_manager.h"
#include "motion.h"
#include "sectree_manager.h"
#include "shop_manager.h"
#include "regen.h"
#include "text_file_loader.h"
#include "skill.h"
#include "pvp.h"
#include "party.h"
#include "questmanager.h"
#include "profiler.h"
#include "lzo_manager.h"
#include "messenger_manager.h"
#include "db.h"
#include "log.h"
#include "p2p.h"
#include "guild_manager.h"
#include "dungeon.h"
#include "cmd.h"
#include "refine.h"
#include "banword.h"
#include "priv_manager.h"
#include "war_map.h"
#include "building.h"
#include "login_sim.h"
#include "target.h"
#include "marriage.h"
#include "wedding.h"
#include "fishing.h"
#include "item_addon.h"
#include "locale_service.h"
#include "arena.h"
#include "OXEvent.h"
#include "polymorph.h"
#include "blend_item.h"
#include "ani.h"
#include "BattleArena.h"
#include "over9refine.h"
#include "horsename_manager.h"
#include "pcbang.h"
#include "MarkManager.h"
#include "spam.h"
#include "skill_power.h"
#include "DragonSoul.h"
#include "ecs/Registry.hpp"
#include "ecs/EventDispatcher.hpp"
#include "ecs/systems/MovementSystem.hpp"
#include "ecs/systems/CombatSystem.hpp"
#include "ecs/systems/VitalRegenSystem.hpp"
#include "ecs/systems/AISystem.hpp"
#include "ecs/systems/AffectSystem.hpp"
#include "ecs/systems/ActivitySystem.hpp"
#include "ecs/systems/NetworkSyncSystem.hpp"
#include <boost/bind.hpp>
#ifdef __ENABLE_NEW_OFFLINESHOP__
#include "new_offlineshop.h"
#include "new_offlineshop_manager.h"
#include "Core/Logging.hpp"
#endif

#ifdef ENABLE_WHISPER_ADMIN_SYSTEM
	#include "whisper_admin.h"
#endif


#ifdef __NEWPET_SYSTEM__
	#include "fstream"
#endif

#ifdef ENABLE_BATTLE_PASS
#include "battle_pass.h"
#endif

//#define __FILEMONITOR__

#if defined (__FreeBSD__) && defined(__FILEMONITOR__)
	#include "FileMonitor_FreeBSD.h"
#endif

#ifdef ENABLE_GOOGLE_TEST
#ifndef _WIN32
#include <gtest/gtest.h>
#endif
#endif

#ifdef USE_STACKTRACE
#include <execinfo.h>
#endif
#ifdef ENABLE_SWITCHBOT
#include "new_switchbot.h"
#endif
//// 윈도우에서 테스트할 때는 항상 서버키 체크
#ifdef _WIN32
	#define _USE_SERVER_KEY_
#endif
#ifdef __NEW_EVENT_HANDLER__
#include "EventFunctionHandler.h"
#endif
#ifdef ENABLE_HWID
#include "hwidmanager.h"
#endif


#ifdef _WIN64
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "secur32.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ws2_32.lib")
#endif



extern void WriteVersion();
////extern const char * _malloc_options;
//#if defined(__FreeBSD__) && defined(DEBUG_ALLOC)
//extern void (*_malloc_message)(const char* p1, const char* p2, const char* p3, const char* p4);
//// FreeBSD _malloc_message replacement
//void WriteMallocMessage(const char* p1, const char* p2, const char* p3, const char* p4) {
//	FILE* fp = ::fopen(DBGALLOC_LOG_FILENAME, "a");
//	if (fp == NULL) {
//		return;
//	}
//	::fprintf(fp, "%s %s %s %s\n", p1, p2, p3, p4);
//	::fclose(fp);
//}
//#endif

// 게임과 연결되는 소켓
volatile int	num_events_called = 0;
int             max_bytes_written = 0;
int             current_bytes_written = 0;
int             total_bytes_written = 0;
uint8_t		g_bLogLevel = 0;

socket_t	tcp_socket = 0;
socket_t	udp_socket = 0;
socket_t	p2p_socket = 0;

LPFDWATCH	main_fdw = nullptr;

int		io_loop(LPFDWATCH fdw);

int		start(int argc, char **argv);
int		idle();
void	destroy();

void 	test();

enum EProfile
{
	PROF_EVENT,
	PROF_CHR_UPDATE,
	PROF_IO,
	PROF_HEARTBEAT,
	PROF_MAX_NUM
};

static uint32_t s_dwProfiler[PROF_MAX_NUM];

int g_shutdown_disconnect_pulse;
int g_shutdown_disconnect_force_pulse;
int g_shutdown_core_pulse;
bool g_bShutdown=false;

extern void CancelReloadSpamEvent();

void ContinueOnFatalError()
{
#ifdef USE_STACKTRACE
	void* array[200];
	std::size_t size;
	char** symbols;

	size = backtrace(array, 200);
	symbols = backtrace_symbols(array, size);

	std::ostringstream oss;
	oss << std::endl;
	for (std::size_t i = 0; i < size; ++i) {
		oss << "  Stack> " << symbols[i] << std::endl;
	}

	free(symbols);

	LOG_ERROR("FatalError on {}", oss.str().c_str());
#else
	LOG_ERROR("FatalError");
#endif
}

void ShutdownOnFatalError()
{
	if (!g_bShutdown)
	{
		LOG_ERROR("ShutdownOnFatalError!!!!!!!!!!");
#ifdef TEXTS_IMPROVEMENT
		SendNoticeNew(CHAT_TYPE_NOTICE, 0, 0, 570, "");
		SendNoticeNew(CHAT_TYPE_NOTICE, 0, 0, 571, "");
		SendNoticeNew(CHAT_TYPE_NOTICE, 0, 0, 572, "");
#endif

		g_bShutdown = true;
		g_bNoMoreClient = true;

		g_shutdown_disconnect_pulse = thecore_pulse() + PASSES_PER_SEC(10);
		g_shutdown_disconnect_force_pulse = thecore_pulse() + PASSES_PER_SEC(20);
		g_shutdown_core_pulse = thecore_pulse() + PASSES_PER_SEC(30);
	}
}

namespace
{
	struct SendDisconnectFunc
	{
		void operator () (LPDESC d)
		{
			if (d->GetCharacter())
			{
				if (d->GetCharacter()->GetGMLevel() == GM_PLAYER)
					ecs::ChatSystem::Send(AIHelpers::EcsOf(d->GetCharacter()), CHAT_TYPE_COMMAND, "quit Shutdown(SendDisconnectFunc)");
			}
		}
	};

	struct DisconnectFunc
	{
		void operator () (LPDESC d)
		{
			if (d->GetType() == DESC_TYPE_CONNECTOR)
				return;

			if (d->IsPhase(PHASE_P2P))
				return;

			d->SetPhase(PHASE_CLOSE);
		}
	};
}

extern std::map<uint32_t, CLoginSim *> g_sim; // first: AID
extern std::map<uint32_t, CLoginSim *> g_simByPID;
extern std::vector<TPlayerTable> g_vec_save;
unsigned int save_idx = 0;

void heartbeat(LPHEART ht, int pulse)
{
	uint32_t t;

	t = get_dword_time();
	num_events_called += event_process(pulse);
	s_dwProfiler[PROF_EVENT] += (get_dword_time() - t);

	t = get_dword_time();

	// 1초마다
	if (!(pulse % ht->passes_per_sec))
	{
		if (!g_bAuthServer)
		{
			TPlayerCountPacket pack;
			pack.dwCount = DESC_MANAGER::instance().GetLocalUserCount();
			db_clientdesc->DBPacket(HEADER_GD_PLAYER_COUNT, 0, &pack, sizeof(TPlayerCountPacket));
		}
		else
		{
			DESC_MANAGER::instance().ProcessExpiredLoginKey();
			DBManager::instance().FlushBilling();
			/*
			   if (!(pulse % (ht->passes_per_sec * 600)))
			   DBManager::instance().CheckBilling();
			 */
		}

		{
			int count = 0;
			auto it = g_sim.begin();

			while (it != g_sim.end())
			{
				if (!it->second->IsCheck())
				{
					it->second->SendLogin();

					if (++count > 50)
					{
						LOG_INFO("FLUSH_SENT");
						break;
					}
				}

				it++;
			}

			if (save_idx < g_vec_save.size())
			{
				count = MIN(100, g_vec_save.size() - save_idx);

				for (int i = 0; i < count; ++i, ++save_idx)
					db_clientdesc->DBPacket(HEADER_GD_PLAYER_SAVE, 0, &g_vec_save[save_idx], sizeof(TPlayerTable));

				LOG_INFO("SAVE_FLUSH {}", count);
			}
		}
	}

	//
	// 25 PPS(Pulse per second) 라고 가정할 때
	//

	// 약 1.16초마다
	if (!(pulse % (passes_per_sec + 4)))
		CHARACTER_MANAGER::instance().ProcessDelayedSave();

	//4초 마다
#if defined (__FreeBSD__) && defined(__FILEMONITOR__)
	if (!(pulse % (passes_per_sec * 5)))
	{
		FileMonitorFreeBSD::Instance().Update(pulse);
	}
#endif

	// 약 5.08초마다
	if (!(pulse % (passes_per_sec * 5 + 2)))
	{
		ITEM_MANAGER::instance().Update();
		DESC_MANAGER::instance().UpdateLocalUserCount();
	}

	s_dwProfiler[PROF_HEARTBEAT] += (get_dword_time() - t);

	DBManager::instance().Process();
	AccountDB::instance().Process();
	CPVPManager::instance().Process();

	if (g_bShutdown)
	{
		if (thecore_pulse() > g_shutdown_disconnect_pulse)
		{
			const DESC_MANAGER::DESC_SET & c_set_desc = DESC_MANAGER::instance().GetClientSet();
			std::for_each(c_set_desc.begin(), c_set_desc.end(), ::SendDisconnectFunc());
			g_shutdown_disconnect_pulse = INT_MAX;
		}
		else if (thecore_pulse() > g_shutdown_disconnect_force_pulse)
		{
			const DESC_MANAGER::DESC_SET & c_set_desc = DESC_MANAGER::instance().GetClientSet();
			std::for_each(c_set_desc.begin(), c_set_desc.end(), ::DisconnectFunc());
		}
		else if (thecore_pulse() > g_shutdown_disconnect_force_pulse + PASSES_PER_SEC(5))
		{
			thecore_shutdown();
		}
	}
}

static void CleanUpForEarlyExit() {
	CancelReloadSpamEvent();
}

int main(int argc, char **argv)
{
//#ifdef __ENABLE_NEW_OFFLINESHOP__
//	if(!Offlineshop_InitializeLibrary("wonder2", "vgbp1q098vgtajp9")){
//		LOG_ERROR("Cannot initialize correctly offlineshop library!");
//		return 0;
//	}
//#endif
#ifdef DEBUG_ALLOC
	DebugAllocator::StaticSetUp();
#endif

#ifdef ENABLE_GOOGLE_TEST
#ifndef _WIN32
	// <Factor> start unit tests if option is set
	if ( argc > 1 )
	{
		if ( strcmp( argv[1], "unittest" ) == 0 )
		{
			::testing::InitGoogleTest(&argc, argv);
			return RUN_ALL_TESTS();
		}
	}
#endif
#endif

#ifdef __NEW_EVENT_HANDLER__
	CEventFunctionHandler EventFunctionHandler;
#endif
	//WriteVersion();
	SECTREE_MANAGER	sectree_manager;
	CHARACTER_MANAGER	char_manager;
	ITEM_MANAGER	item_manager;
	CShopManager	shop_manager;
	CMobManager		mob_manager;
	CMotionManager	motion_manager;
	CPartyManager	party_manager;
	CSkillManager	skill_manager;
	CPVPManager		pvp_manager;
	LZOManager		lzo_manager;
	DBManager		db_manager;
	AccountDB 		account_db;

#ifdef ENABLE_BATTLE_PASS
	CBattlePass	battle_pass;
#endif

	LogManager		log_manager;
	MessengerManager	messenger_manager;
	P2P_MANAGER		p2p_manager;
	CGuildManager	guild_manager;
	CGuildMarkManager mark_manager;
	CDungeonManager	dungeon_manager;
	CRefineManager	refine_manager;
	CBanwordManager	banword_manager;
	CPrivManager	priv_manager;
	CWarMapManager	war_map_manager;
	building::CManager	building_manager;
	CTargetManager	target_manager;
	marriage::CManager	marriage_manager;
	marriage::WeddingManager wedding_manager;
	CItemAddonManager	item_addon_manager;
	CArenaManager arena_manager;
	COXEventManager OXEvent_manager;
	CHorseNameManager horsename_manager;
	CPCBangManager pcbang_manager;

	DESC_MANAGER	desc_manager;

#ifdef ENABLE_WHISPER_ADMIN_SYSTEM
	CWhisperAdmin whisper;
#endif


	CTableBySkill SkillPowerByLevel;
	CPolymorphUtils polymorph_utils;
	CProfiler		profiler;
	CBattleArena	ba;
	COver9RefineManager	o9r;
	SpamManager		spam_mgr;
	DSManager dsManager;
#ifdef ENABLE_HWID
	CHwidManager	hwid_manager;
#endif
#ifdef ENABLE_SWITCHBOT
	CSwitchbotManager switchbot;
#endif

	if (!start(argc, argv)) {
		CleanUpForEarlyExit();
		return 0;
	}

#ifdef __ENABLE_NEW_OFFLINESHOP__
	offlineshop::CShopManager offshopManager;
#endif
	quest::CQuestManager quest_manager;

	if (!quest_manager.Initialize()) {
		CleanUpForEarlyExit();
		return 0;
	}

	MessengerManager::instance().Initialize();
	CGuildManager::instance().Initialize();
	fishing::Initialize();
	OXEvent_manager.Initialize();
	Cube_init();
	Blend_Item_init();
	ani_init();

//#ifdef __NEWPET_SYSTEM__
//	std::string temp_exp_line;
//
//	
//	std::string path = LocaleService_GetBasePath();
//	if (!path.empty() && path.back() != '/' && path.back() != '\\')
//		path += '/';
//
//	path += "exppettable.txt";
//
//	std::ifstream exppet_table_open(path);
//	
//
//	/*if (!exp_table_open.is_open())
//	return 0;*/
//
//	int exppet_table_counter = 0;
//	//int tmppet_exp = 0;
//	while (!exppet_table_open.eof())
//	{
//		exppet_table_open >> temp_exp_line;
//		str_to_number(exppet_table_common[exppet_table_counter], temp_exp_line.c_str());
//		if (exppet_table_common[exppet_table_counter] < 2147483647) 
//		{
//			0, "Livelli Pet caricati da exppettable.txt: %d !", exppet_table_common[exppet_table_counter]);
//			exppet_table_counter++;
//		}
//		//else 
//		//{
//		//	LOG_ERROR("[main] Impossibile caricare la tabella exp valore non valido");//razor93
//		//	break;
//		//}
//}
//#endif

	if (g_bAuthServer)
	{
#ifdef ENABLE_HWID
		hwid_manager.InitializeBlocked();
#endif
	}

	while (idle());

	LOG_INFO("<shutdown> Starting...");
	g_bShutdown = true;
	g_bNoMoreClient = true;

	if (g_bAuthServer)
	{
		DBManager::instance().FlushBilling(true);

		int iLimit = DBManager::instance().CountQuery() / 50;
		int i = 0;

		do
		{
			uint32_t dwCount = DBManager::instance().CountQuery();
			LOG_INFO("Queries {}", dwCount);

			if (dwCount == 0)
				break;

			usleep(500000);

			if (++i >= iLimit)
				if (dwCount == DBManager::instance().CountQuery())
					break;
		} while (1);
	}
//#ifdef __ENABLE_NEW_OFFLINESHOP__
//	Offlineshop_CleanUpLibrary();
//#endif

	LOG_INFO("<shutdown> Destroying CArenaManager...");
	arena_manager.Destroy();
	LOG_INFO("<shutdown> Destroying COXEventManager...");
	OXEvent_manager.Destroy();

	LOG_INFO("<shutdown> Disabling signal timer...");
	signal_timer_disable();

	LOG_INFO("<shutdown> Shutting down CHARACTER_MANAGER...");
	char_manager.GracefulShutdown();
	LOG_INFO("<shutdown> Shutting down ITEM_MANAGER...");
	item_manager.GracefulShutdown();

	LOG_INFO("<shutdown> Flushing db_clientdesc...");
	db_clientdesc->FlushOutput();
	LOG_INFO("<shutdown> Flushing p2p_manager...");
	p2p_manager.FlushOutput();

	LOG_INFO("<shutdown> Destroying CShopManager...");
	shop_manager.Destroy();
	LOG_INFO("<shutdown> Destroying CHARACTER_MANAGER...");
	char_manager.Destroy();
	LOG_INFO("<shutdown> Destroying ITEM_MANAGER...");
	item_manager.Destroy();
	LOG_INFO("<shutdown> Destroying DESC_MANAGER...");
	desc_manager.Destroy();
#ifdef __NEW_EVENT_HANDLER__
	LOG_INFO("<shutdown> Destroying CEventFunctionHandler...");
	CEventFunctionHandler::instance().Destroy();
#endif
	LOG_INFO("<shutdown> Destroying quest::CQuestManager...");
	quest_manager.Destroy();
	LOG_INFO("<shutdown> Destroying building::CManager...");
	building_manager.Destroy();

	if (g_bAuthServer)
	{
#ifdef ENABLE_HWID
		hwid_manager.CleanBlocked();
#endif
	}

	destroy();

#ifdef DEBUG_ALLOC
	DebugAllocator::StaticTearDown();
#endif

	return 1;
}

void usage()
{
	std::string options =
			"Option list"
			"-p <port>    : bind port number (port must be over 1024)"
			"-l <level>   : sets log level"
			"-n <locale>  : sets locale name";
#ifdef ENABLE_NEWSTUFF
	options += "-C <on-off>  : checkpointing check on/off";
#endif
	options +=
			"-v           : log to stdout"
			"-r           : do not load regen tables"
			"-t           : traffic profile on";
	LOG_INFO("{}", options);
}

int start(int argc, char **argv)
{
	std::string st_localeServiceName;

	bool bVerbose = false;
	char ch;

//	//_malloc_options = "A";
//#if defined(__FreeBSD__) && defined(DEBUG_ALLOC)
//	_malloc_message = WriteMallocMessage;
//#endif


#ifdef ENABLE_NEWSTUFF

	char ixtreeme[] = "npverltIC";

	while ((ch = getopt(argc, argv, &ixtreeme[1])) != -1)
#else
	while ((ch = getopt(argc, argv, "npverltI")) != -1)
#endif
	{
		char* ep = nullptr;

		switch (ch)
		{
			case 'I': // IP
				strlcpy(g_szPublicIP, argv[optind], sizeof(g_szPublicIP));

				LOG_INFO("IP {}", g_szPublicIP);

				optind++;
				optreset = 1;
				break;

			case 'p': // port
				mother_port = strtol(argv[optind], &ep, 10);

				if (mother_port <= 1024)
				{
					usage();
					return 0;
				}

				LOG_INFO("port {}", mother_port);

				optind++;
				optreset = 1;
				break;

			case 'l':
				{
				int32_t l = strtol(argv[optind], &ep, 10);

					log_set_level(l);

					optind++;
					optreset = 1;
				}
				break;

				// LOCALE_SERVICE
			case 'n':
				{
					if (optind < argc)
					{
						st_localeServiceName = argv[optind++];
						optreset = 1;
					}
				}
				break;
				// END_OF_LOCALE_SERVICE

#ifdef ENABLE_NEWSTUFF
			case 'C': // checkpoint check
				//bCheckpointCheck = strtol(argv[optind], &ep, 10);;
				// LOG_INFO("CHECKPOINT_CHECK {}", bCheckpointCheck);

				optind++;
				optreset = 1;
				break;
#endif

			case 'v': // verbose
				bVerbose = true;
				break;

			case 'r':
				g_bNoRegen = true;
				break;
		}
	}

	// LOCALE_SERVICE
	config_init(st_localeServiceName);
	// END_OF_LOCALE_SERVICE

#ifdef _WIN32
	// In Windows dev mode, "verbose" option is [on] by default.
	bVerbose = true;
#endif
	if (!bVerbose)
		freopen("stdout", "a", stdout);

	bool is_thecore_initialized = thecore_init(25, heartbeat);
	if (!is_thecore_initialized)
	{
		LOG_ERROR("Could not initialize thecore, check owner of pid, syslog");
		exit(0);
	}

	signal_timer_disable();

	main_fdw = fdwatch_new(4096);

	if ((tcp_socket = socket_tcp_bind(g_szPublicIP, mother_port)) == INVALID_SOCKET)
	{
		LOG_ERROR("{}: {}", "socket_tcp_bind: tcp_socket", strerror(errno));
		return 0;
	}


#ifndef __UDP_BLOCK__
	if ((udp_socket = socket_udp_bind(g_szPublicIP, mother_port)) == INVALID_SOCKET)
	{
		LOG_ERROR("{}: {}", "socket_udp_bind: udp_socket", strerror(errno));
		return 0;
	}
#endif

	// if internal ip exists, p2p socket uses internal ip, if not use public ip
	//if ((p2p_socket = socket_tcp_bind(*g_szInternalIP ? g_szInternalIP : g_szPublicIP, p2p_port)) == INVALID_SOCKET)
	if ((p2p_socket = socket_tcp_bind(g_szPublicIP, p2p_port)) == INVALID_SOCKET)
	{
		LOG_ERROR("{}: {}", "socket_tcp_bind: p2p_socket", strerror(errno));
		return 0;
	}

	fdwatch_add_fd(main_fdw, tcp_socket, nullptr, FDW_READ, false);
#ifndef __UDP_BLOCK__
	fdwatch_add_fd(main_fdw, udp_socket, NULL, FDW_READ, false);
#endif
	fdwatch_add_fd(main_fdw, p2p_socket, nullptr, FDW_READ, false);

	db_clientdesc = DESC_MANAGER::instance().CreateConnectionDesc(main_fdw, db_addr, db_port, PHASE_DBCLIENT, true);
	if (!g_bAuthServer) {
		db_clientdesc->UpdateChannelStatus(0, true);
	}

	if (g_bAuthServer)
	{
		if (g_stAuthMasterIP.length() != 0)
		{
			LOG_ERROR("SlaveAuth");
			g_pkAuthMasterDesc = DESC_MANAGER::instance().CreateConnectionDesc(main_fdw, g_stAuthMasterIP.c_str(), g_wAuthMasterPort, PHASE_P2P, true);
			P2P_MANAGER::instance().RegisterConnector(g_pkAuthMasterDesc);
			g_pkAuthMasterDesc->SetP2P(g_stAuthMasterIP.c_str(), g_wAuthMasterPort, g_bChannel);

		}
		else
		{
			LOG_ERROR("MasterAuth {}", LC_GetLocalType());
		}
	}
	else
	{
		LOG_INFO("SPAM_CONFIG: duration {} score {} reload cycle {}\n", g_uiSpamBlockDuration, g_uiSpamBlockScore, g_uiSpamReloadCycle);

		extern void LoadSpamDB();
		LoadSpamDB();
	}

	signal_timer_enable(30);
	return 1;
}

void destroy()
{
	LOG_INFO("<shutdown> Canceling ReloadSpamEvent...");
	CancelReloadSpamEvent();

	LOG_INFO("<shutdown> regen_free()...");
	regen_free();

	LOG_INFO("<shutdown> Closing sockets...");
	socket_close(tcp_socket);
#ifndef __UDP_BLOCK__
	socket_close(udp_socket);
#endif
	socket_close(p2p_socket);

	LOG_INFO("<shutdown> fdwatch_delete()...");
	fdwatch_delete(main_fdw);

	LOG_INFO("<shutdown> event_destroy()...");
	event_destroy();

	LOG_INFO("<shutdown> CTextFileLoader::DestroySystem()...");
	CTextFileLoader::DestroySystem();

	LOG_INFO("<shutdown> thecore_destroy()...");
	thecore_destroy();
}

int idle()
{
	static struct timeval	pta = { 0, 0 };
	static int			process_time_count = 0;
	struct timeval		now;

	if (pta.tv_sec == 0)
		gettimeofday(&pta, (struct timezone *) nullptr);

	int passed_pulses;

	if (!(passed_pulses = thecore_idle()))
		return 0;

	assert(passed_pulses > 0);

	uint32_t t;

	while (passed_pulses--) {
		heartbeat(thecore_heart, ++thecore_heart->pulse);

		// To reduce the possibility of abort() in checkpointing
		thecore_tick();
	}

	t = get_dword_time();
	CHARACTER_MANAGER::instance().Update(thecore_heart->pulse);
	// Phase 8 audit: the parallel ECS runtime tick is not authoritative yet.
	// Movement/combat/network sync still rely on the migrated CHARACTER:: bodies.
	// Leaving the placeholder ECS loops enabled overrides live gameplay with
	// incomplete movement/combat/points packets.
	constexpr bool kEnableParallelEcsMigrationTicks = true;
	if (kEnableParallelEcsMigrationTicks)
	{
		const uint32_t tick = static_cast<uint32_t>(get_dword_time());
		// AISystem is disabled during the migration window.
		// Legacy CHARACTER FSM handles all AI behavior.
		// Re-enable in Phase 11 after FSM removal.
		// AISystem_Update(g_registry, tick);
		MovementSystem_Update(g_registry, tick);
		CombatSystem_Update(g_registry, tick);
		// VitalRegenSystem mirrors legacy recovery_event output back into ECS.
		VitalRegenSystem_Update(g_registry, tick);
		AffectSystem::UpdateAffect(g_registry, tick);
		AffectSystem_Update(g_registry, tick);
		// NetworkSyncSystem is disabled during the migration window.
		// Legacy CHARACTER packet paths remain authoritative for full stat/bonus UI sync.
		// Re-enable after the ECS sync packet surface matches the legacy packet layout.
		// NetworkSyncSystem_Update(g_registry, tick);
		g_dispatcher.update();
		// Phase 7 verification log - REMOVE IN PHASE 9
		static uint32_t s_ecsdebug = 0;
		if (++s_ecsdebug % 3000 == 0) {
			size_t ecsCount = 0;
			for (auto entity : g_registry.storage<entt::entity>().each()) {
				(void)entity;
				++ecsCount;
			}
			LOG_INFO("ECS registry: {} alive entities", ecsCount);
		}
	}
	db_clientdesc->Update(t);
	s_dwProfiler[PROF_CHR_UPDATE] += (get_dword_time() - t);

	t = get_dword_time();
	if (!io_loop(main_fdw)) return 0;
	s_dwProfiler[PROF_IO] += (get_dword_time() - t);

	log_rotate();

	gettimeofday(&now, (struct timezone *) nullptr);
	++process_time_count;

	if (now.tv_sec - pta.tv_sec > 0)
	{
		pt_log("[%3d] event %5d/%-5d idle %-4ld event %-4ld heartbeat %-4ld I/O %-4ld chrUpate %-4ld | WRITE: %-7d | PULSE: %d",
				process_time_count,
				num_events_called,
				event_count(),
				thecore_profiler[PF_IDLE],
				s_dwProfiler[PROF_EVENT],
				s_dwProfiler[PROF_HEARTBEAT],
				s_dwProfiler[PROF_IO],
				s_dwProfiler[PROF_CHR_UPDATE],
				current_bytes_written,
				thecore_pulse());

		num_events_called = 0;
		current_bytes_written = 0;

		process_time_count = 0;
		gettimeofday(&pta, (struct timezone *) nullptr);

		memset(&thecore_profiler[0], 0, sizeof(thecore_profiler));
		memset(&s_dwProfiler[0], 0, sizeof(s_dwProfiler));
	}

#ifdef _WIN32
	if (_kbhit()) {
		int c = _getch();
		switch (c) {
			case 0x1b: // Esc
				return 0; // shutdown
				break;
			default:
				break;
		}
	}
#endif

#ifdef __NEW_EVENT_HANDLER__
	CEventFunctionHandler::instance().Process();
#endif

	return 1;
}

int io_loop(LPFDWATCH fdw)
{
	LPDESC	d;
	int		num_events, event_idx;

	DESC_MANAGER::instance().DestroyClosed(); // PHASE_CLOSE인 접속들을 끊어준다.
	DESC_MANAGER::instance().TryConnect();

	if ((num_events = fdwatch(fdw, nullptr)) < 0)
		return 0;

	for (event_idx = 0; event_idx < num_events; ++event_idx)
	{
		d = (LPDESC) fdwatch_get_client_data(fdw, event_idx);

		if (!d)
		{
			if (FDW_READ == fdwatch_check_event(fdw, tcp_socket, event_idx))
			{
				DESC_MANAGER::instance().AcceptDesc(fdw, tcp_socket);
				fdwatch_clear_event(fdw, tcp_socket, event_idx);
			}
			else if (FDW_READ == fdwatch_check_event(fdw, p2p_socket, event_idx))
			{
				DESC_MANAGER::instance().AcceptP2PDesc(fdw, p2p_socket);
				fdwatch_clear_event(fdw, p2p_socket, event_idx);
			}
			/*
			else if (FDW_READ == fdwatch_check_event(fdw, udp_socket, event_idx))
			{
				char			buf[256];
				struct sockaddr_in	cliaddr;
				socklen_t		socklen = sizeof(cliaddr);

				int iBytesRead;

				if ((iBytesRead = socket_udp_read(udp_socket, buf, 256, (struct sockaddr *) &cliaddr, &socklen)) > 0)
				{
					static CInputUDP s_inputUDP;

					s_inputUDP.SetSockAddr(cliaddr);

					int iBytesProceed;
					s_inputUDP.Process(NULL, buf, iBytesRead, iBytesProceed);
				}

				fdwatch_clear_event(fdw, udp_socket, event_idx);
			}
			*/
			continue;
		}

		int iRet = fdwatch_check_event(fdw, d->GetSocket(), event_idx);

		switch (iRet)
		{
			case FDW_READ:
				if (db_clientdesc == d)
				{
					int size = d->ProcessInput();

					if (size)
						LOG_TRACE("DB_BYTES_READ: {}", size);

					if (size < 0)
					{
						d->SetPhase(PHASE_CLOSE);
					}
				}
				else if (d->ProcessInput() < 0)
				{
					d->SetPhase(PHASE_CLOSE);
				}
				break;

			case FDW_WRITE:
				if (db_clientdesc == d)
				{
					int buf_size = buffer_size(d->GetOutputBuffer());
					int sock_buf_size = fdwatch_get_buffer_size(fdw, d->GetSocket());

					int ret = d->ProcessOutput();

					if (ret < 0)
					{
						d->SetPhase(PHASE_CLOSE);
					}

					if (buf_size)
						LOG_INFO("DB_BYTES_WRITE: size {} sock_buf {} ret {}", buf_size, sock_buf_size, ret);
				}
				else if (d->ProcessOutput() < 0)
				{
					d->SetPhase(PHASE_CLOSE);
				}
				break;

			case FDW_EOF:
				{
					d->SetPhase(PHASE_CLOSE);
				}
				break;

			default:
				LOG_ERROR("fdwatch_check_event returned unknown {}", iRet);
				d->SetPhase(PHASE_CLOSE);
				break;
		}
	}

	return 1;
}







