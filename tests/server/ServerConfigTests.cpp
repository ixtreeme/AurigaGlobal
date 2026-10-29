#include <Core/server_config.h>
#include "../../SRC/Server/Database/DatabaseConfig.h"
#include "../../SRC/Server/LoginServer/login_config.h"
#include "../../SRC/Server/GameServer/core/GameConfig.h"
#include "../../SRC/Server/common/CommonDefines.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace {
using Json = nlohmann::json;
using auriga::config::Role;
int checks = 0;
void Check(bool result, const char* message) { ++checks; if (!result) throw std::runtime_error(message); }
Json Fixture() {
    Json sql = Json::object();
    for (const char* name : {"player", "account", "common", "log"}) sql[name] = {{"host", "127.0.0.1"}, {"port", 3306}, {"user", "fixture"}, {"password", "PASSWORD_DO_NOT_LOG"}, {"database", name}};
    return {
        {"schema_version", 1},
        {"common", {{"locale", "utf8mb4"}, {"interserver_key", "fixture-key"}, {"table_postfix", "_fixture"}, {"sql", sql}, {"tls", {{"cert_file", "tls/server.pem"}, {"key_file", "tls/server.key"}}}}},
        {"database", {{"working_directory", "run/database"}, {"item_id_range", {100, 1000}}, {"network", {{"bind_ip", "127.0.0.1"}, {"bind_port", 5300}}}}},
        {"login", {{"working_directory", "run/login"}, {"port", 30052}}},
        {"game_defaults", {{"server", {{"max_level", 160}, {"data_path", "data"}}}, {"quest", {{"dir", "data/quest"}, {"object_dir", {"data/quest/object"}}}}, {"network", {{"bind_ip", "127.0.0.1"}}}, {"map_allow", {99}}}},
        {"game_servers", {
            {"ch1-core1", {{"working_directory", "run/ch1-core1"}, {"server", {{"hostname", "core one"}, {"channel", 1}, {"server_id", 1}}}, {"network", {{"port", 30054}, {"p2p_port", 30055}}}, {"map_allow", {1, 21, 41}}}},
            {"ch1-core2", {{"server", {{"hostname", "core two"}, {"channel", 1}, {"server_id", 2}}}, {"network", {{"port", 30058}, {"p2p_port", 30059}}}, {"map_allow", {65}}}}
        }}
    };
}
}
int main() {
    try {
        const auto root = std::filesystem::temp_directory_path() / ("auriga-server-config-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(root);
        const auto source = root / "server config.json";
        const auto fixture = Fixture();
        std::string error;
        auriga::config::Section section;
        Check(auriga::config::Resolve(fixture, source, Role::Game, "ch1-core1", section, error), "valid central config rejected");
        GameConfig game;
        Check(game.Load(section, error), "typed game config rejected");
        Check(game.network.port == 30054 && game.network.dbPort == 5300 && game.network.dbAddr == "127.0.0.1", "network inheritance lost");
        Check(game.server.channel == 1 && game.server.maxLevel == 160 && game.server.hostname == "core one", "recursive defaults overwrite instance fields");
        Check(game.sql.player.password == "PASSWORD_DO_NOT_LOG" && game.sql.log.database == "log" && game.interserverKey == "fixture-key", "shared connection fields lost");
        Check(game.mapAllow == std::vector<int32_t>({1, 21, 41}), "map arrays appended instead of replaced");
        Check(game.quest.objectDir.size() == 1 && game.network.tls.certFile == (root / "tls/server.pem").generic_string(), "paths not relative to central file");
        Check(game.server.dataPath == (root / "data").generic_string() && section.workingDirectory == root / "run/ch1-core1", "data/working paths changed base");
        Check(game.gameplay.itemCountLimit == 50000, "typed default lost feature definitions");
        const auto first = section;
        Check(auriga::config::Resolve(fixture, source, Role::Game, "ch1-core2", section, error) && game.Load(section, error), "second core rejected");
        Check(section.workingDirectory == root / "run/ch1-core2", "default working directories are not isolated by instance");
        Check(game.mapAllow == std::vector<int32_t>({65}) && game.quest.objectDir.size() == 1 && game.server.hostname == "core two", "reloading appends old arrays/state");
        auto malformed = section; malformed.values["server"]["max_level"] = "PASSWORD_DO_NOT_LOG";
        Check(!game.Load(malformed, error) && game.server.hostname == "core two" && error.find("PASSWORD_DO_NOT_LOG") == std::string::npos, "typed failure mutates live config or leaks a value");
        auto routing = first;
        routing.values["map_allow"] = Json::array();
        for (int map = 1; map <= MAP_ALLOW_LIMIT; ++map)
            routing.values["map_allow"].push_back(map);
        Check(game.Load(routing, error) && game.mapAllow.size() == MAP_ALLOW_LIMIT &&
              game.mapAllow.back() == MAP_ALLOW_LIMIT, "full routing capacity rejected or truncated");
        routing.values["map_allow"].push_back(430);
        Check(!game.Load(routing, error) && game.mapAllow.size() == MAP_ALLOW_LIMIT &&
              game.mapAllow.back() == MAP_ALLOW_LIMIT && error.find("routing capacity") != std::string::npos &&
              error.find("PASSWORD_DO_NOT_LOG") == std::string::npos,
              "routing overflow accepted, changed live config or leaked credentials");
        Check(game.Load(first, error), "valid config did not recover after routing overflow");
        // Typed item catalog switch (F4): off by default, fail closed on bad values.
        Check(game.items.catalog == "off" && game.items.catalogRevision == 1 && game.items.catalogSha256.empty(), "item catalog is not off by default");
        auto items = first;
        items.values["items"] = Json{{"catalog", "required"}, {"catalog_revision", 7}, {"catalog_sha256", std::string(64, 'a')}};
        Check(game.Load(items, error) && game.items.catalog == "required" && game.items.catalogRevision == 7 &&
              game.items.catalogSha256 == std::string(64, 'a'), "required item catalog settings lost");
        for (const auto& [field, value] : std::vector<std::pair<std::string, Json>>{{"catalog", "on"}, {"catalog_revision", 0},
                 {"catalog_sha256", std::string(64, 'A')}, {"catalog_sha256", "abc"}})
        {
            auto bad = items; bad.values["items"][field] = value;
            Check(!game.Load(bad, error) && game.items.catalogRevision == 7 && error.find("items.") != std::string::npos,
                  "invalid item catalog setting accepted or changed the live config");
        }
        Check(game.Load(first, error) && game.items.catalog == "off", "item catalog setting survived a reload without it");
        DatabaseConfig database; LoginConfig login;
        Check(auriga::config::Resolve(fixture, source, Role::Database, {}, section, error) && database.Load(section, error), "database section rejected");
        Check(database.network.bindPort == 5300 && database.sql.account.database == "account" && database.server.tablePostfix == "_fixture", "database shared mapping lost");
        Check(database.sql.mission.database == "mission" && database.sql.mission.user == "fixture" &&
              game.sql.mission.database == "mission", "legacy configuration did not derive a dedicated Mission database");
        auto missionConfig = fixture;
        missionConfig["common"]["sql"]["mission"] = {{"database", "story_mission"}};
        Check(auriga::config::Resolve(missionConfig, source, Role::Game, "ch1-core1", section, error) &&
              game.Load(section, error) && game.sql.mission.database == "story_mission" &&
              game.sql.mission.password == "PASSWORD_DO_NOT_LOG", "partial Mission override lost player endpoint/credentials");
        missionConfig["database"]["sql"]["mission"]["port"] = 0;
        Check(auriga::config::Resolve(missionConfig, source, Role::Database, {}, section, error),
              "default TCP port and explicit 3306 should identify the same Mission endpoint");
        Check(auriga::config::Resolve(fixture, source, Role::Login, {}, section, error) && LoadLoginConfig(section, login, error), "login section rejected");
        Check(login.listenPort == 30052 && login.dbLink.port == 5300 && login.tls.keyFile == (root / "tls/server.key").generic_string(), "login link/TLS paths lost");
        Check(!auriga::config::Resolve(fixture, source, Role::Game, "missing", section, error), "unknown core accepted");
        Check(section.instance == "login", "failed resolver changed previous section");
        auto reject = [&](Json changed) { Check(!auriga::config::Resolve(changed, source, Role::Game, "ch1-core1", section, error), "invalid central config accepted"); Check(error.find("PASSWORD_DO_NOT_LOG") == std::string::npos, "error leaks secret value"); };
        auto bad = fixture; bad["schema_version"] = 2; reject(bad);
        for (const char* name : {"player", "common", "log", "account", "mysql", "PLAYER", "bad`name"}) {
            bad = fixture; bad["common"]["sql"]["mission"] = {{"database", name}}; reject(bad);
        }
        bad = fixture; bad["common"]["sql"]["mission"] = {{"port", 3310}}; reject(bad);
        bad = fixture; bad["common"]["sql"]["mission"] = {{"host", "192.0.2.1"}}; reject(bad);
        bad = fixture; bad["game_servers"]["ch1-core2"]["sql"]["mission"] = {{"database", "other_mission"}}; reject(bad);
        bad = fixture; bad["game_servers"] = Json::object(); bad["common"]["sql"]["mission"] = {{"database", "log"}};
        Check(!auriga::config::Resolve(bad, source, Role::Database, {}, section, error),
              "Mission shared a legacy log schema when no GameServer was configured");
        bad = fixture; bad["unexpected"] = true; reject(bad);
        bad = fixture; bad["common"]["sql"]["player"]["port"] = "PASSWORD_DO_NOT_LOG"; reject(bad);
        bad = fixture; bad["game_servers"]["ch1-core1"]["network"]["port"] = 65536; reject(bad);
        bad = fixture; bad["game_servers"]["ch1-core1"]["network"]["port"] = -1; reject(bad);
        bad = fixture; bad["game_servers"]["ch1-core1"]["server"]["channel"] = 256; reject(bad);
        bad = fixture; bad["game_servers"]["ch1-core2"]["network"]["port"] = 30054; reject(bad);
        bad = fixture; bad["game_servers"]["ch1-core2"]["server"]["server_id"] = 1; reject(bad);
        bad = fixture; bad["game_servers"]["ch1-core2"]["interserver_key"] = "other"; reject(bad);
        bad = fixture; bad["game_servers"]["ch1-core2"]["server"]["table_postfix"] = "_other"; reject(bad);
        bad = fixture; bad["login"]["database_link"]["port"] = 5400; reject(bad);
        bad = fixture; bad["database"]["item_id_range"] = {9, 1}; reject(bad);
        bad = fixture; bad["game_servers"]["ch1-core1"]["map_allow"] = {1, 1}; reject(bad);
        bad = fixture; bad["game_defaults"]["quest"]["object_dir"] = "PASSWORD_DO_NOT_LOG"; reject(bad);
        bad = fixture; bad["common"]["tls"]["key_file"] = ""; reject(bad);
        bad = fixture; bad.erase("schema_version"); reject(bad);
        auriga::config::StartupOptions options; options.file = source; options.instance = "ch1-core1";
        { std::ofstream file(source); file << fixture.dump(2); }
        Check(auriga::config::Load(options, Role::Game, section, error), "disk config rejected");
        { std::ofstream file(source); file << "{\"schema_version\":1," << fixture.dump().substr(1); }
        Check(!auriga::config::Load(options, Role::Game, section, error), "duplicate JSON key accepted");
        { std::ofstream file(source); file << "{\"PASSWORD_DO_NOT_LOG\":"; }
        Check(!auriga::config::Load(options, Role::Game, section, error) && error.find("PASSWORD_DO_NOT_LOG") == std::string::npos, "malformed JSON leaks input");
        std::vector<std::string> args = {"GameServer", "--config", source.string(), "-v", "--instance=ch1-core1", "--check-config"};
        std::vector<char*> argv; for (auto& arg : args) argv.push_back(arg.data()); argv.push_back(nullptr); int argc = static_cast<int>(args.size());
        Check(auriga::config::ParseArguments(Role::Game, argc, argv.data(), options, error) && argc == 2 && std::string(argv[1]) == "-v" && options.checkOnly && options.instance == "ch1-core1", "CLI lost legacy arguments");
        args = {"GameServer", "--check-config"}; argv.clear(); for (auto& arg : args) argv.push_back(arg.data()); argv.push_back(nullptr); argc = 2;
        Check(!auriga::config::ParseArguments(Role::Game, argc, argv.data(), options, error), "implicit GameServer instance accepted");
        Check(auriga::config::Resolve(fixture, source, Role::Game, "ch1-core1", section, error), "fixture did not recover");
        Check(!std::filesystem::exists(section.workingDirectory), "config resolution created runtime directories");
        const auto previous = std::filesystem::current_path();
        Check(auriga::config::EnterWorkingDirectory(section, error), "explicit working directory rejected");
        Check(std::filesystem::current_path() == first.workingDirectory && section.values["network"]["tls"]["cert_file"] == (root / "tls/server.pem").generic_string(), "working directory changes TLS resolution");
        // Graceful stop is available only to managed processes and requires both
        // their exact PID and the per-child token; a stale request cannot stop one.
#ifdef _WIN32
        _putenv_s("AURIGA_SHUTDOWN_TOKEN", "config-test-token");
        const auto pid = _getpid();
#else
        setenv("AURIGA_SHUTDOWN_TOKEN", "config-test-token", 1);
        const auto pid = getpid();
#endif
        { std::ofstream request(".auriga-stop"); request << pid << " wrong-token"; }
        Check(!auriga::config::ShutdownRequested(), "shutdown accepted a foreign token");
        { std::ofstream request(".auriga-stop"); request << (pid + 1) << " config-test-token"; }
        Check(!auriga::config::ShutdownRequested(), "shutdown accepted a different process PID");
        { std::ofstream request(".auriga-stop"); request << pid << " config-test-token"; }
        Check(auriga::config::ShutdownRequested() && !std::filesystem::exists(".auriga-stop"), "owned shutdown request was not consumed");
        Check(!auriga::config::ShutdownRequested(), "consumed shutdown request repeated");
#ifdef _WIN32
        _putenv_s("AURIGA_SHUTDOWN_TOKEN", "");
#else
        unsetenv("AURIGA_SHUTDOWN_TOKEN");
#endif
        std::filesystem::current_path(previous);
        std::cout << "SERVER_CONFIG_TESTS=SUCCESS checks=" << checks << " artifacts=" << root.string() << '\n';
    } catch (const std::exception& ex) { std::cerr << "SERVER_CONFIG_TESTS=FAILED " << ex.what() << '\n'; return 1; }
    return 0;
}
