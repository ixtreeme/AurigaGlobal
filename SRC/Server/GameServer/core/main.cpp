#include "stdafx.h"
#include "GameConfig.h"
#include "../quest/quest_runtime.h"
#include <Core/server_config.h>
#include <iostream>
#include "../ecs/AIHelpers.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/CharacterFrame.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/WoodcuttingSystem.hpp"
#include "../ecs/systems/HerbalismSystem.hpp"
#include "../mission_pass/MissionPassSystem.hpp"
#include "../story/SpyQuestSystem.hpp"
#include "../story/CaravanQuestSystem.hpp"
#include "../ecs/systems/VisibilitySystem.hpp"
#include <Core/Logging.hpp>
#include <Core/tls.h>

#include <memory>
#include "constants.h"
#include "config.h"
#include "crash_report.h"
#include "event.h"
#include "minilzo.h"
#include "packet.h"
#include "desc_manager.h"
#include "../ecs/CharacterAccessors.hpp"
#include "item_manager.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "mob_manager.h"
#include "motion.h"
#include "sectree_manager.h"
#include "shop_manager.h"
#include "regen.h"
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
#include "locale_service.h"
#include "arena.h"
#include "OXEvent.h"
#include "polymorph.h"
#include "blend_item.h"


#include "BattleArena.h"
#include "over9refine.h"
#include "horsename_manager.h"
#include "MarkManager.h"
#include "spam.h"
#include "skill_power.h"
#include "DragonSoul.h"
#include "../ecs/Registry.hpp"
#include "../ecs/EventDispatcher.hpp"
#include "../ecs/systems/MovementSystem.hpp"
#include "../ecs/systems/AffectSystem.hpp"
#include "../ecs/components/status_components.hpp"
#include "../ecs/systems/ActivitySystem.hpp"
#include "../ecs/systems/NetworkSyncSystem.hpp"
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



volatile int	num_events_called = 0;
int             max_bytes_written = 0;
int             current_bytes_written = 0;
int             total_bytes_written = 0;
uint8_t		g_bLogLevel = 0;

int		io_loop();

int		start(int argc, char **argv, const GameConfig& config);
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
			if (ecs::IsCharacter(d->GetEntity()))
			{
				if (ecs::PlayerRuntime::GetGMLevel(d->GetEntity()) == GM_PLAYER)
					ecs::ChatSystem::Send(d->GetEntity(), CHAT_TYPE_COMMAND, "quit Shutdown(SendDisconnectFunc)");
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
    if(!g_bShutdown)SpyQuest::Tick();
    if(!g_bShutdown)CaravanQuest::Tick();
    if (!(pulse % ht->passes_per_sec) && !g_bShutdown) {
        WoodcuttingSystem::Tick(get_dword_time());
        HerbalismSystem::Tick(get_dword_time());
        AurigaMissionPass::Tick();
    }
    if (pulse % std::max(1, passes_per_sec / 5) == 0)
        if (auto* quests = quest::CQuestManager::instance_ptr()) quests->PollReloadControl();
	uint32_t t;

	t = get_dword_time();
	num_events_called += event_process(pulse);
	s_dwProfiler[PROF_EVENT] += (get_dword_time() - t);

	t = get_dword_time();

	if (!(pulse % ht->passes_per_sec))
	{
		{
			TPlayerCountPacket pack;
			pack.dwCount = DESC_MANAGER::instance().GetLocalUserCount();
			db_clientdesc->DBPacket(HEADER_GD_PLAYER_COUNT, 0, &pack, sizeof(TPlayerCountPacket));
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
	//

	if (!(pulse % (passes_per_sec + 4)))
		CHARACTER_MANAGER::instance().ProcessDelayedSave();

#if defined (__FreeBSD__) && defined(__FILEMONITOR__)
	if (!(pulse % (passes_per_sec * 5)))
	{
		FileMonitorFreeBSD::Instance().Update(pulse);
	}
#endif

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

// thecore_destroy() ends with logging::Shutdown(), so it must run after every manager
// main() owns: their destructors still log (CAsyncSQL::Destroy). Database and LoginServer
// order it the same way. Armed once thecore_init() has run, so the early exits after it
// (TLS, bind) flush the log too; before it there is no core to tear down.
static bool s_thecoreStarted = false;

struct ThecoreShutdownGuard
{
	~ThecoreShutdownGuard()
	{
		if (s_thecoreStarted)
			thecore_destroy();
	}
};

int main(int argc, char **argv)
{
	// Before anything else: an access violation otherwise leaves only a WER
	// event behind, because the async syslog queue dies with the process.
	InstallCrashReporter();

    // Run the actual game Lua bindings in preparation mode, without opening
    // sockets, connecting to SQL or applying shared C++ registrations.
    if (argc == 3 && std::string(argv[1]) == "--check-quest-runtime") {
        try {
            const auto root = std::filesystem::canonical(argv[2]).generic_string();
            if (!LocaleService_InitFlat(root, "latin1")) return 1;
            quest::CQuestManager manager;
            manager.ConfigureReloadControl(root, "quest-validator", root, root + "/quest");
            manager.CurrentRuntime()->staging = true;
            if (!manager.Initialize()) {
                auto* L = manager.GetLuaState();
                std::cerr << "QUEST_RUNTIME_INVALID: " << (L && lua_isstring(L, -1) ? lua_tostring(L, -1) : "initialization failed") << '\n';
                return 1;
            }
            std::string error;
            if (!manager.PrepareReload(root, manager.RuntimeRevision(), error)) {
                std::cerr << "QUEST_RUNTIME_INVALID: " << error << '\n'; return 1;
            }
            manager.AbortReload(manager.RuntimeRevision());
            std::cout << "QUEST_RUNTIME_OK " << manager.RuntimeRevision() << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "QUEST_RUNTIME_INVALID: " << error.what() << '\n'; return 1;
        }
    }

//		LOG_ERROR("Cannot initialize correctly offlineshop library!");
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

    auriga::config::StartupOptions options;
    auriga::config::Section section;
    std::string configError;
    if (!auriga::config::ParseArguments(auriga::config::Role::Game, argc, argv, options, configError))
    { std::cerr << configError << '\n'; auriga::config::PrintUsage(auriga::config::Role::Game); return 1; }
    if (options.help) { auriga::config::PrintUsage(auriga::config::Role::Game); return 0; }
    GameConfig gameConfig;
    if (!auriga::config::Load(options, auriga::config::Role::Game, section, configError) || !gameConfig.Load(section, configError))
    { std::cerr << "GameServer configuration: " << configError << '\n'; return 1; }
    if (options.checkOnly) { auriga::config::PrintChecked(section, auriga::config::Role::Game); return 0; }
    if (!auriga::config::EnterWorkingDirectory(section, configError)) { std::cerr << configError << '\n'; return 1; }

	// Declared before the managers, so it is destroyed after them (see the guard).
	ThecoreShutdownGuard thecoreShutdown;

#ifdef __NEW_EVENT_HANDLER__
	CEventFunctionHandler EventFunctionHandler;
#endif
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
	CArenaManager arena_manager;
	COXEventManager OXEvent_manager;
	CHorseNameManager horsename_manager;

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

	if (!start(argc, argv, gameConfig)) {
		CleanUpForEarlyExit();
		return 1;
	}

#ifdef __ENABLE_NEW_OFFLINESHOP__
	offlineshop::CShopManager offshopManager;
#endif
	quest::CQuestManager quest_manager;
    quest_manager.ConfigureReloadControl(section.source.parent_path().generic_string(), section.instance,
        LocaleService_GetBasePath(), g_stQuestDir);

	if (!quest_manager.Initialize()) {
		CleanUpForEarlyExit();
		return 0;
	}
    quest_manager.PollReloadControl();

	MessengerManager::instance().Initialize();
	CGuildManager::instance().Initialize();
	fishing::Initialize();
	OXEvent_manager.Initialize();
	Cube_init();
	Blend_Item_init();

	// Phase 15E-final.LPENTITY.4-architect.D.3: connect the
	// VisibilitySystem PositionChangedEvent handler to the global
	// dispatcher. Skeleton in D.3; D.4 fills in the diff handler.
	ecs::VisibilitySystem::Init(g_registry);

//		path += '/';
//
//	path += "exppettable.txt";
//
//	/*if (!exp_table_open.is_open())
//	//int tmppet_exp = 0;
//		exppet_table_open >> temp_exp_line;
//		str_to_number(exppet_table_common[exppet_table_counter], temp_exp_line.c_str());
//			0, "Livelli Pet caricati da exppettable.txt: %d !", exppet_table_common[exppet_table_counter]);
//			exppet_table_counter++;
//		}
//		//else 
//		//{
//		//	LOG_ERROR("[main] Impossibile caricare la tabella exp valore non valido");//razor93
//		//	break;
//		//}

	while (!auriga::config::ShutdownRequested() && idle());

	LOG_INFO("<shutdown> Starting...");
	g_bShutdown = true;
	AurigaMissionPass::Flush();
	g_bNoMoreClient = true;

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

	destroy();

	return 1;
}

void usage()
{
	std::string options =
			"Configuration: --config <server.json> --instance <name> [--check-config]\n"
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

int start(int argc, char **argv, const GameConfig& config)
{
	std::string st_localeServiceName;

	bool bVerbose = false;
	char ch;

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
	if (!st_localeServiceName.empty()) LOG_INFO("The -n locale option is ignored; use server.json");
	config_init(config);
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
	s_thecoreStarted = true;

	signal_timer_disable();

	// Socket I/O runs on the dedicated engine thread from here on.
	auriga::net::Start();

	// The client port speaks TLS only (resolved network.tls in server.json). The
	// context outlives every session.
	static std::unique_ptr<auriga::net::TlsServerContext> s_clientTls;

	if (g_clientTlsCertFile.empty() || g_clientTlsKeyFile.empty())
	{
		LOG_ERROR("network.tls needs cert_file and key_file for the client port");
		return 0;
	}

	s_clientTls = auriga::net::TlsServerContext::FromFiles(g_clientTlsCertFile, g_clientTlsKeyFile);

	if (!s_clientTls)
	{
		LOG_ERROR("the client TLS certificate could not be loaded");
		return 0;
	}

	DESC_MANAGER::instance().SetClientTls(s_clientTls.get());
	LOG_INFO("client connections use TLS ({})", g_clientTlsCertFile);

	if (!DESC_MANAGER::instance().StartAccept(g_szPublicIP, mother_port, false))
	{
		LOG_ERROR("{}: {}", "socket_tcp_bind: tcp_socket", strerror(errno));
		return 0;
	}


	// if internal ip exists, p2p socket uses internal ip, if not use public ip
	//if (!DESC_MANAGER::instance().StartAccept(*g_szInternalIP ? g_szInternalIP : g_szPublicIP, p2p_port, true))
	if (!DESC_MANAGER::instance().StartAccept(g_szPublicIP, p2p_port, true))
	{
		LOG_ERROR("{}: {}", "socket_tcp_bind: p2p_socket", strerror(errno));
		return 0;
	}

	db_clientdesc = DESC_MANAGER::instance().CreateConnectionDesc(db_addr, db_port, PHASE_DBCLIENT, true);
	db_clientdesc->UpdateChannelStatus(0, true);

	LOG_INFO("SPAM_CONFIG: duration {} score {} reload cycle {}\n", g_uiSpamBlockDuration, g_uiSpamBlockScore, g_uiSpamReloadCycle);

	extern void LoadSpamDB();
	LoadSpamDB();

	signal_timer_enable(30);
	return 1;
}

void destroy()
{
	LOG_INFO("<shutdown> Stopping AsioEngine...");
	auriga::net::Stop();

	LOG_INFO("<shutdown> Canceling ReloadSpamEvent...");
	CancelReloadSpamEvent();

	LOG_INFO("<shutdown> regen_free()...");
	regen_free();

	LOG_INFO("<shutdown> Closing acceptors...");
	DESC_MANAGER::instance().CloseAcceptors();

	LOG_INFO("<shutdown> event_destroy()...");
	// Cancel ECS-owned affect timers while their queue entries still exist,
	// including any entities not owned by the legacy character manager.
	g_registry.clear<ecs::AffectTickState>();
	event_destroy();
	// thecore_destroy() runs from main's ThecoreShutdownGuard, after the managers.
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
	ecs::UpdateCharacterFrame(g_registry, static_cast<uint32_t>(t), [] {
		CHARACTER_MANAGER::instance().Update(thecore_heart->pulse);
	});
	// The frame advances movement before CharacterManager runs the AI, so AI
	// reads current positions and cannot discard an unconsumed movement step.
	// AI and client input dispatch real attacks; there is no second damage tick.
	{
		const uint32_t tick = static_cast<uint32_t>(get_dword_time());
		AffectSystem::UpdateAffect(g_registry, tick);
		AffectSystem_Update(g_registry, tick);
		// State owners publish protocol packets when their changes commit.
		// There is no DirtyTag network replay; see docs/server_network_publication.md.
		g_dispatcher.update();
		// Phase 15E-final.LPENTITY.4-architect.D.5: drift detector self-throttles
		// to one sweep per 5s. Body is a no-op outside AURIGA_LPENTITY_FIXUP_AUDIT.
		ecs::VisibilitySystem::DriftSweep(g_registry);
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
	if (!io_loop()) return 0;
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

int io_loop()
{
	DESC_MANAGER::instance().DestroyClosed();
	DESC_MANAGER::instance().TryConnect();

	// Runs the accept/read/write completions the io thread handed over
	// since the previous pulse. Everything runs on this thread.
	auriga::net::DrainCompletions();

	// Deferred deletion of descriptors whose aborted operations have finished.
	DESC_MANAGER::instance().CollectPendingDeletes();

	// Server-to-server link keepalive / idle timeout.
	DESC_MANAGER::instance().CheckHeartbeats();

	return 1;
}





