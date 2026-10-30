#include "server_config.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>
#include <cstdlib>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace auriga::config {
namespace {
using Json = nlohmann::json;
void Need(bool condition, const std::string& message) { if (!condition) throw std::runtime_error(message); }
void Object(const Json& value, const char* field) { Need(value.is_object(), std::string(field) + " must be an object"); }
Json Merge(Json base, const Json& overlay) {
    Object(overlay, "configuration section");
    for (auto it = overlay.begin(); it != overlay.end(); ++it) {
        if (it.value().is_object() && base.contains(it.key()) && base[it.key()].is_object()) base[it.key()] = Merge(base[it.key()], it.value());
        else base[it.key()] = it.value(); // Arrays replace, never append; null is not a deletion operator.
    }
    return base;
}
const Json& OptionalObject(const Json& object, const char* field) {
    static const Json empty = Json::object();
    if (!object.contains(field)) return empty;
    Object(object.at(field), field); return object.at(field);
}
long long Integer(const Json& object, const char* field, long long fallback, long long min, long long max) {
    if (!object.contains(field)) return fallback;
    const auto& value = object.at(field);
    Need(value.is_number_integer(), std::string(field) + " must be an integer");
    if (value.is_number_unsigned()) Need(value.get<unsigned long long>() <= static_cast<unsigned long long>(max), std::string(field) + " is out of range");
    const auto result = value.get<long long>();
    Need(result >= min && result <= max, std::string(field) + " is out of range"); return result;
}
std::string String(const Json& object, const char* field, const std::string& fallback = {}) {
    if (!object.contains(field)) return fallback;
    Need(object.at(field).is_string(), std::string(field) + " must be a string"); return object.at(field).get<std::string>();
}
void Sql(const Json& object) {
    Object(object, "SQL connection");
    Need(!String(object, "host").empty() && !String(object, "database").empty(), "SQL host and database are required");
    String(object, "user"); String(object, "password"); Integer(object, "port", 0, 0, 65535);
}
Json MissionSql(Json section) {
    if (!section.contains("sql") || !section["sql"].contains("player")) return section;
    auto& sql = section["sql"];
    auto base = sql["player"];
    base["database"] = "mission";
    sql["mission"] = Merge(base, OptionalObject(sql, "mission"));
    return section;
}
// GameServer typed item catalog (IC-034). Inherits the player endpoint and
// credentials; the reader's SQL is fully qualified, so the name stays protos.
Json ProtosSql(Json section) {
    if (!section.contains("sql") || !section["sql"].contains("player")) return section;
    auto& sql = section["sql"];
    auto base = sql["player"];
    base["database"] = "protos";
    sql["protos"] = Merge(base, OptionalObject(sql, "protos"));
    return section;
}
long long SqlPort(const Json& value) {
    const auto port = Integer(value, "port", 0, 0, 65535);
    return port ? port : 3306;
}
void MissionSqlCheck(const Json& section, const Json& shared) {
    const auto& sql = section.at("sql");
    const auto& mission = sql.at("mission");
    const auto& player = sql.at("player");
    Sql(mission);
    const auto name = String(mission, "database");
    Need(!name.empty() && name.size() <= 64 && name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") == std::string::npos,
         "SQL mission database must be a simple identifier");
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    Need(std::set<std::string>{"mysql", "information_schema", "performance_schema", "sys"}.count(lower) == 0,
         "SQL mission cannot use a system database");
    for (const char* role : {"player", "account", "common", "log"}) {
        if (!sql.contains(role) && !shared.contains(role)) continue;
        auto other = String(sql.contains(role) ? sql.at(role) : shared.at(role), "database");
        std::transform(other.begin(), other.end(), other.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        Need(lower != other, "SQL mission must use a separate database");
    }
    Need(String(mission, "host") == String(player, "host") && SqlPort(mission) == SqlPort(player),
         "SQL mission and player must use the same MariaDB endpoint");
}
void ProtosSqlCheck(const Json& section) {
    const auto& sql = section.at("sql");
    const auto& protos = sql.at("protos");
    const auto& player = sql.at("player");
    Sql(protos);
    Need(String(protos, "database") == "protos", "SQL protos database must be named protos");
    Need(String(protos, "host") == String(player, "host") && SqlPort(protos) == SqlPort(player),
         "SQL protos and player must use the same MariaDB endpoint");
}
void Tls(const Json& object) {
    Object(object, "TLS settings");
    Need(!String(object, "cert_file").empty() && !String(object, "key_file").empty(), "TLS cert_file and key_file are required");
}
Json Effective(const Json& root, Role role, const std::string& instance) {
    const auto& common = OptionalObject(root, "common");
    Json result = Json::object();
    if (common.contains("locale")) {
        if (role == Role::Login) result["locale"] = common.at("locale"); else result["server"]["locale"] = common.at("locale");
    }
    if (common.contains("table_postfix") && role != Role::Login) result["server"]["table_postfix"] = common.at("table_postfix");
    if (common.contains("interserver_key")) {
        if (role == Role::Database) result["server"]["interserver_key"] = common.at("interserver_key");
        else result["interserver_key"] = common.at("interserver_key");
    }
    const auto& sql = OptionalObject(common, "sql");
    if (role == Role::Login) { if (sql.contains("account")) result["account_sql"] = sql.at("account"); }
    else {
        for (const char* name : (role == Role::Database ? std::vector<const char*>{"player", "account", "common", "mission"} : std::vector<const char*>{"player", "common", "log", "mission", "protos"}))
            if (sql.contains(name)) result["sql"][name] = sql.at(name);
    }
    if (common.contains("tls") && role != Role::Database) {
        if (role == Role::Login) result["tls"] = common.at("tls"); else result["network"]["tls"] = common.at("tls");
    }
    if (role == Role::Database) return MissionSql(Merge(result, root.at("database")));
    const auto db = Effective(root, Role::Database, {});
    const auto& dbNetwork = OptionalObject(db, "network");
    Json link = {{"host", String(dbNetwork, "bind_ip", "127.0.0.1")}, {"port", Integer(dbNetwork, "bind_port", 5300, 1, 65535)}};
    if (link["host"] == "0.0.0.0" || link["host"] == "::" || link["host"] == "") link["host"] = "127.0.0.1";
    link = Merge(link, OptionalObject(common, "database_link"));
    if (role == Role::Login) { result["database_link"] = link; return Merge(result, root.at("login")); }
    result["network"]["db_addr"] = link.at("host"); result["network"]["db_port"] = link.at("port");
    result = Merge(result, OptionalObject(root, "game_defaults"));
    Need(root.at("game_servers").contains(instance), "unknown GameServer instance");
    return ProtosSql(MissionSql(Merge(result, root.at("game_servers").at(instance))));
}
void Validate(const Json& root) {
    Object(root, "server configuration");
    Need(Integer(root, "schema_version", 0, 1, 1) == 1, "schema_version 1 is required; migrate old conf.json files first");
    for (auto it = root.begin(); it != root.end(); ++it)
        Need(std::set<std::string>{"schema_version", "common", "database", "login", "game_defaults", "game_servers"}.count(it.key()) != 0, "unknown top-level configuration key");
    Need(root.contains("database") && root.contains("login") && root.contains("game_servers"), "database, login and game_servers sections are required");
    Object(root.at("database"), "database"); Object(root.at("login"), "login"); Object(root.at("game_servers"), "game_servers");
    const auto& common = OptionalObject(root, "common");
    for (auto it = common.begin(); it != common.end(); ++it)
        Need(std::set<std::string>{"locale", "table_postfix", "interserver_key", "sql", "tls", "database_link"}.count(it.key()) != 0, "unknown common configuration key");
    OptionalObject(root, "game_defaults");
    for (const char* field : {"locale", "table_postfix", "interserver_key"}) String(common, field);
    const auto& sharedSql = OptionalObject(common, "sql");
    for (auto it = sharedSql.begin(); it != sharedSql.end(); ++it) {
        Need(std::set<std::string>{"player", "account", "common", "log", "mission", "protos"}.count(it.key()) != 0, "unknown shared SQL role");
        if (it.key() == "mission" || it.key() == "protos") Object(it.value(), it.key() == "mission" ? "SQL mission" : "SQL protos"); else Sql(it.value());
    }
    if (common.contains("tls")) Tls(common.at("tls"));
    const auto db = Effective(root, Role::Database, {});
    const auto login = Effective(root, Role::Login, {});
    const auto& dbNetwork = OptionalObject(db, "network");
    const auto dbPort = Integer(dbNetwork, "bind_port", 5300, 1, 65535);
    const auto& dbServer = OptionalObject(db, "server");
    for (const char* name : {"player", "account", "common"}) Sql(db.at("sql").at(name));
    MissionSqlCheck(db, sharedSql);
    const auto& range = db.at("item_id_range");
    Need(range.is_array() && range.size() == 2, "database.item_id_range needs two IDs");
    Json ids = {{"first", range[0]}, {"last", range[1]}};
    const auto first = Integer(ids, "first", 0, 1, 0xffffffffLL);
    Need(Integer(ids, "last", 0, 1, 0xffffffffLL) >= first, "database.item_id_range is reversed");
    Integer(dbServer, "client_heart_fps", 50, 1, 1000);
    Sql(login.at("account_sql")); Tls(login.at("tls"));
    const auto loginPort = Integer(login, "port", 0, 1, 65535); Need(loginPort != 0, "login.port is required");
    const auto& loginLink = login.at("database_link"); Object(loginLink, "login.database_link");
    Need(!String(loginLink, "host").empty() && Integer(loginLink, "port", 0, 1, 65535) == dbPort, "LoginServer database link must use the configured Database port");
    const auto key = String(dbServer, "interserver_key");
    Need(String(login, "interserver_key") == key, "Database and LoginServer interserver keys differ");
    struct Endpoint { std::string ip; long long port; };
    std::vector<Endpoint> endpoints;
    auto endpoint = [&](std::string ip, long long port) {
        if (ip.empty()) ip = "0.0.0.0";
        for (const auto& other : endpoints)
            Need(port != other.port || (ip != other.ip && ip != "0.0.0.0" && ip != "::" && other.ip != "0.0.0.0" && other.ip != "::"), "overlapping TCP listening ports in server configuration");
        endpoints.push_back({ip, port});
    };
    endpoint(String(dbNetwork, "bind_ip", "0.0.0.0"), dbPort);
    endpoint("0.0.0.0", loginPort); // LoginListener currently binds all IPv4 interfaces.
    std::set<long long> serverIds;
    auto directory = [](const Json& value) {
        if (value.contains("working_directory")) Need(!String(value, "working_directory").empty(), "working_directory cannot be empty");
    };
    directory(db); directory(login);
    for (auto it = root.at("game_servers").begin(); it != root.at("game_servers").end(); ++it) {
        Need(!it.key().empty() && it.key() != "database" && it.key() != "login", "invalid or reserved GameServer instance name");
        Need(it.key().size() <= 64 && std::all_of(it.key().begin(), it.key().end(), [](unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'; }), "GameServer instance names must be simple identifiers");
        Need((it.key().front() >= 'a' && it.key().front() <= 'z') || (it.key().front() >= 'A' && it.key().front() <= 'Z') || (it.key().front() >= '0' && it.key().front() <= '9'), "GameServer instance names must start with a letter or digit");
        try {
        Object(it.value(), "GameServer instance");
        const auto game = Effective(root, Role::Game, it.key()); directory(game);
        const auto& server = game.at("server"); Object(server, "game.server");
        Need(!String(server, "hostname").empty(), "GameServer hostname is required");
        Need(Integer(server, "channel", 0, 1, 255) != 0, "GameServer channel is required");
        Integer(server, "passes_per_sec", 25, 1, 1000);
        const auto id = Integer(server, "server_id", 0, 0, 0x7fffffff);
        if (id != 0) Need(serverIds.insert(id).second, "duplicate GameServer server_id");
        Need(String(server, "table_postfix") == String(dbServer, "table_postfix"), "GameServer and Database table_postfix differ");
        Need(String(game, "interserver_key") == key, "GameServer and Database interserver keys differ");
        const auto& network = game.at("network"); Object(network, "game.network");
        Need(!String(network, "db_addr").empty() && Integer(network, "db_port", 0, 1, 65535) == dbPort, "GameServer database link must use the configured Database port");
        endpoint(String(network, "bind_ip"), Integer(network, "port", 50080, 1, 65535));
        endpoint(String(network, "bind_ip"), Integer(network, "p2p_port", 50900, 1, 65535));
        Tls(network.at("tls"));
        for (const char* name : {"player", "common", "log"}) Sql(game.at("sql").at(name));
        MissionSqlCheck(game, sharedSql);
        Need(game.at("sql").at("mission").at("host") == db.at("sql").at("mission").at("host") &&
             game.at("sql").at("mission").at("database") == db.at("sql").at("mission").at("database") &&
             SqlPort(game.at("sql").at("mission")) == SqlPort(db.at("sql").at("mission")),
             "GameServer and Database mission targets differ");
        ProtosSqlCheck(game);
        if (game.contains("map_allow")) {
            Need(game.at("map_allow").is_array(), "map_allow must be an array");
            std::set<long long> maps;
            for (const auto& map : game.at("map_allow")) { Json item = {{"map", map}}; Need(maps.insert(Integer(item, "map", 0, 1, 0x7fffffff)).second, "duplicate map in map_allow"); }
        }
        if (game.contains("guild")) Integer(game.at("guild"), "mark_min_level", 3, 0, 255);
        } catch (const Json::exception&) { throw std::runtime_error("game_servers." + it.key() + ": missing field or invalid field type"); }
          catch (const std::runtime_error& ex) { throw std::runtime_error("game_servers." + it.key() + ": " + ex.what()); }
    }
}
void Path(Json& object, const char* key, const std::filesystem::path& base) {
    if (!object.contains(key)) return;
    const auto text = String(object, key); if (text.empty()) return;
    auto path = std::filesystem::path(text); if (path.is_relative()) path = base / path;
    object[key] = path.lexically_normal().generic_string();
}
}

const char* RoleName(Role role) { return role == Role::Database ? "database" : role == Role::Login ? "login" : "game"; }
void PrintUsage(Role role) {
    std::cout << "Usage: " << (role == Role::Database ? "Database" : role == Role::Login ? "LoginServer" : "GameServer")
              << " --config <server.json>" << (role == Role::Game ? " --instance <name>" : "") << " [--check-config]\n";
}
void PrintChecked(const Section& section, Role role) {
    std::cout << "CONFIG_OK role=" << RoleName(role) << " instance=" << section.instance << " file=" << section.source.string() << '\n';
}
bool ParseArguments(Role role, int& argc, char** argv, StartupOptions& out, std::string& error) {
    StartupOptions parsed; std::set<std::string> seen; int write = 1;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--check-config") { parsed.checkOnly = true; continue; }
        if (arg == "--help" || arg == "-h") { parsed.help = true; continue; }
        if (arg == "--config" || arg == "--instance" || arg.rfind("--config=", 0) == 0 || arg.rfind("--instance=", 0) == 0) {
            const auto equal = arg.find('='); const auto name = arg.substr(0, equal);
            if (!seen.insert(name).second) { error = "duplicate startup option"; return false; }
            std::string value;
            if (equal != std::string::npos) value = arg.substr(equal + 1);
            else if (i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0) value = argv[++i];
            if (value.empty()) { error = "missing --config/--instance value"; return false; }
            if (name == "--config") parsed.file = value; else parsed.instance = value;
        } else if (role == Role::Game && arg.rfind("--", 0) != 0) argv[write++] = argv[i];
        else { error = "unknown startup argument; use --help"; return false; }
    }
    if (!parsed.help) {
        if (role == Role::Game && parsed.instance.empty()) { error = "GameServer requires --instance"; return false; }
        if (role != Role::Game) {
            if (!parsed.instance.empty() && parsed.instance != RoleName(role)) { error = "instance does not match executable role"; return false; }
            parsed.instance = RoleName(role);
        }
    }
    argc = write; argv[write] = nullptr; out = std::move(parsed); return true;
}
bool Resolve(const Json& document, const std::filesystem::path& source, Role role, const std::string& instance, Section& out, std::string& error) {
    try {
        Validate(document);
        Section selected; selected.source = std::filesystem::absolute(source).lexically_normal();
        selected.instance = role == Role::Game ? instance : RoleName(role);
        selected.values = Effective(document, role, selected.instance);
        const auto base = selected.source.parent_path();
        selected.workingDirectory = std::filesystem::path(String(selected.values, "working_directory", "run/" + selected.instance));
        if (selected.workingDirectory.is_relative()) selected.workingDirectory = base / selected.workingDirectory;
        selected.workingDirectory = selected.workingDirectory.lexically_normal();
        if (role == Role::Login) { Path(selected.values["tls"], "cert_file", base); Path(selected.values["tls"], "key_file", base); }
        if (role == Role::Game) {
            Path(selected.values["network"]["tls"], "cert_file", base); Path(selected.values["network"]["tls"], "key_file", base);
            if (!selected.values["server"].contains("data_path")) selected.values["server"]["data_path"] = "data";
            Path(selected.values["server"], "data_path", base);
            if (selected.values.contains("quest")) {
                auto& quest = selected.values["quest"]; Object(quest, "quest"); Path(quest, "dir", base);
                if (quest.contains("object_dir")) {
                    Need(quest["object_dir"].is_array(), "quest.object_dir must be an array");
                    for (auto& item : quest["object_dir"]) { Json path = {{"value", item}}; Path(path, "value", base); item = path["value"]; }
                }
            }
        }
        out = std::move(selected); error.clear(); return true;
    } catch (const Json::exception&) { error = "missing field or invalid JSON field type in server configuration"; }
      catch (const std::filesystem::filesystem_error&) { error = "cannot resolve configuration paths"; }
      catch (const std::runtime_error& ex) { error = ex.what(); }
    return false;
}
bool Load(const StartupOptions& options, Role role, Section& out, std::string& error) {
    std::ifstream input(options.file);
    if (!input) { error = "cannot open server configuration file"; return false; }
    try {
        std::vector<std::set<std::string>> keys;
        auto callback = [&](int, Json::parse_event_t event, Json& value) {
            if (event == Json::parse_event_t::object_start) keys.emplace_back();
            else if (event == Json::parse_event_t::key) Need(keys.back().insert(value.get<std::string>()).second, "duplicate JSON key in server configuration");
            else if (event == Json::parse_event_t::object_end) keys.pop_back();
            return true;
        };
        return Resolve(Json::parse(input, callback), options.file, role, options.instance, out, error);
    } catch (const Json::exception&) { error = "invalid JSON in server configuration"; }
      catch (const std::runtime_error& ex) { error = ex.what(); }
    return false;
}
bool EnterWorkingDirectory(const Section& section, std::string& error) {
    std::error_code ec;
    std::filesystem::create_directories(section.workingDirectory, ec);
    if (!ec) std::filesystem::current_path(section.workingDirectory, ec);
    if (ec) { error = "cannot create or enter configured working_directory"; return false; }
    return true;
}
bool ShutdownRequested() {
    const char* token = std::getenv("AURIGA_SHUTDOWN_TOKEN");
    if (!token || !*token) return false;
    std::ifstream input(".auriga-stop");
    long long pid = 0;
    std::string supplied;
    if (!(input >> pid >> supplied) || supplied != token) return false;
#ifdef _WIN32
    if (pid != _getpid()) return false;
#else
    if (pid != getpid()) return false;
#endif
    input.close();
    std::error_code ignored;
    std::filesystem::remove(".auriga-stop", ignored);
    return true;
}
}
