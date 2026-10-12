#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Typed GameServer configuration, loaded from conf.json.
//
// Schema (every key is optional; the defaults below match the historical
// CONFIG defaults):
// {
//   "server": {
//     "hostname": "", "channel": 0, "test": 0, "table_postfix": "",
//     "server_id": 0, "user_limit": 32768, "max_level": 99,
//     "stone_chance": 30, "block_login": "30000705",
//     "block_char_creation": false, "empire_whisper": true,
//     "protect_normal_player": false, "notice_battle_zone": false,
//     "pk_protect_level": 30, "skill_disable": false, "no_wander": false,
//     "shutdowned": false, "no_regen": false, "traffic_profile": false,
//     "attr_transfer_limit": 0, "passes_per_sec": 25, "view_range": 5000,
//     "mall_url": "https://...", "locale": "latin1", "data_path": "data"
//   },
//   "network": { "port": 50080, "p2p_port": 50900, "db_addr": "",
//                "db_port": 0, "bind_ip": "" },
//   "sql": { "player": { "host": "", "port": 0, "user": "",
//                        "password": "", "database": "" },
//            "common": { ... }, "log": { ... } },
//   "interserver_key": "",
//   "map_allow": [ 1, 21, 41 ],
//   "events": { "save_second_cycle": 120, "ping_second_cycle": 60 },
//   "shutdown": { "age": 0, "enable": 0 },
//   "logs": { "keep_days": 2, "db_level": 3, "sys_level": 3 },
//   "quest": { "dir": "./quest", "object_dir": [ "./quest/object" ] },
//   "spam_block": { "duration": 900, "score": 100,
//                   "reload_cycle": 600, "max_level": 10 },
//   "guild": { "mark_server": true, "mark_min_level": 3,
//              "invite_limit": false, "infinite_members": false },
//   "version": { "check_server": true, "value": "20251204" },
//   "item_destroy": { "autogive": 300, "dropgold": 150, "dropitem": 30 },
//   "gameplay": {
//     "item_count_limit": 50000, "empire_shop_price_triple_disable": false,
//     "shout_addon_enable": false, "global_shout_enable": false,
//     "disable_prism_need": false, "disable_emotion_mask": false,
//     "item_bonus_change_time": 60, "all_mount_attack": true,
//     "enable_bootary_check": false, "gm_host_check": false,
//     "status_point_get_level_limit": 90, "status_point_set_max_value": 90,
//     "shout_limit_level": 15, "disable_empire_language_check": false,
//     "skillbook_nextread_min": 28800, "skillbook_nextread_max": 43200
//   }
// }
struct GameSqlConfig
{
	std::string	host;
	int		port { 0 };
	std::string	user;
	std::string	password;
	std::string	database;

	bool IsValid() const { return !host.empty() && !database.empty(); }
};

struct GameConfig
{
	struct ServerConfig
	{
		std::string	hostname;
		uint8_t		channel { 0 };
		int		test { 0 };
		std::string	tablePostfix;
		int		serverId { 0 };
		int		userLimit { 32768 };
		int		maxLevel { 99 };
		int		stoneChance { 30 };
		std::string	blockLogin { "30000705" };
		bool		blockCharCreation { false };
		bool		empireWhisper { true };
		bool		protectNormalPlayer { false };
		bool		noticeBattleZone { false };
		int		pkProtectLevel { -1 }; // unset: keep the locale default
		bool		skillDisable { false };
		bool		noWander { false };
		bool		shutdowned { false };
		bool		noRegen { false };
		bool		trafficProfile { false };
		int		attrTransferLimit { 0 };
		int		passesPerSec { 25 };
		int		viewRange { 5000 };
		std::string	mallUrl { "https://www.wonder2.org/index.php/shop/login?pid=$playerID&key=$codice" };
		std::string	locale { "latin1" }; // DB charset, e.g. "latin1"
		std::string	dataPath { "data" };   // server data root (map/quest/drop/...), relative to cwd
	} server;

	struct NetworkConfig
	{
		uint16_t	port { 50080 };
		uint16_t	p2pPort { 50900 };
		std::string	dbAddr;
		uint16_t	dbPort { 0 };
		std::string	bindIp;
	} network;

	struct SqlConfig
	{
		GameSqlConfig	player;
		GameSqlConfig	common;
		GameSqlConfig	log;
	} sql;

	std::string		interserverKey;
	std::vector<int32_t>	mapAllow;

	struct EventsConfig
	{
		int	saveSecondCycle { 120 };
		int	pingSecondCycle { 60 };
	} events;

	struct ShutdownConfig
	{
		int	age { 0 };
		int	enable { 0 };
	} shutdown;

	struct LogsConfig
	{
		int	keepDays { 2 };
		int	dbLevel { 3 }; // LOG_LEVEL_MAX
		int	sysLevel { 3 };
	} logs;

	struct QuestConfig
	{
		std::string		dir; // unset: keep the locale default
		std::vector<std::string>	objectDir;
	} quest;

	struct SpamBlockConfig
	{
		unsigned int	duration { 900 };
		unsigned int	score { 100 };
		unsigned int	reloadCycle { 600 };
		int		maxLevel { 10 };
	} spamBlock;

	struct GuildConfig
	{
		bool		markServer { true };
		uint8_t		markMinLevel { 3 };
		bool		inviteLimit { false };
		bool		infiniteMembers { false };
	} guild;

	struct VersionConfig
	{
		bool		checkServer { true };
		std::string	value { "20251204" };
	} version;

	struct ItemDestroyConfig
	{
		int	autogive { 300 };
		int	dropgold { 150 };
		int	dropitem { 30 };
	} itemDestroy;

	struct GameplayConfig
	{
#ifdef ENABLE_NEW_STACK_LIMIT
		int		itemCountLimit { 50000 };
#else
		int		itemCountLimit { 200 };
#endif
		bool		empireShopPriceTripleDisable { false };
		bool		shoutAddonEnable { false };
		bool		globalShoutEnable { false };
		bool		disablePrismNeed { false };
		bool		disableEmotionMask { false };
		uint32_t	itemBonusChangeTime { 60 };
		bool		allMountAttack { true };
		bool		enableBootaryCheck { false };
		bool		gmHostCheck { false };
		int		statusPointGetLevelLimit { 90 };
		int		statusPointSetMaxValue { 90 };
		int	shoutLimitLevel { 15 };
		bool		disableEmpireLanguageCheck { false };
		uint32_t	skillBookNextReadMin { 28800 };
		uint32_t	skillBookNextReadMax { 43200 };
	} gameplay;

	bool Load(const char* path);
};
