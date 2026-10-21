#include <Core/stdafx.h>
#include "../../SRC/Server/Database/player_create.h"
#include <algorithm>
#include <iostream>
#include <limits>
#include <map>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {
int checks = 0;
void Check(bool ok, const char* why) { ++checks; if (!ok) throw std::runtime_error(why); }
std::map<std::string, SqlParam> Columns(const PlayerCreateQuery& query)
{
    const auto begin = query.sql.find('(');
    const auto end = query.sql.find(')', begin);
    std::istringstream names(query.sql.substr(begin + 1, end - begin - 1));
    std::map<std::string, SqlParam> result;
    std::string name;
    size_t index = 0;
    while (std::getline(names, name, ',')) {
        name.erase(0, name.find_first_not_of(' '));
        Check(index < query.params.size(), "column without parameter");
        Check(result.emplace(name, query.params[index++]).second, "duplicate column");
    }
    Check(index == query.params.size(), "parameter without column");
    Check(std::count(query.sql.begin() + end, query.sql.end(), '?') == index, "placeholder count mismatch");
    return result;
}

// Explicit opt-in: PlayerCreateTests --sql <port> <db/schema.sql>.
// Uses a dedicated database and a temporary table from the checked-in schema.
void SqlRoundTrip(const PlayerCreateQuery& query, unsigned int port, const char* schemaPath)
{
    std::ifstream schemaFile(schemaPath);
    Check(bool(schemaFile), "schema file missing");
    const std::string schema((std::istreambuf_iterator<char>(schemaFile)), {});
    const auto begin = schema.find("CREATE TABLE IF NOT EXISTS `player`.`player`");
    Check(begin != std::string::npos, "player schema missing");
    const auto columns = schema.find('(', begin);
    const auto engine = schema.find(") ENGINE=InnoDB", columns);
    const auto end = schema.find(';', engine);
    Check(columns != std::string::npos && end != std::string::npos, "incomplete player schema");
    const std::string create = "CREATE TEMPORARY TABLE player_test " + schema.substr(columns, end + 1 - columns);

    CAsyncSQL sql;
    sql.SetTimeouts(5);
    Check(sql.Setup("127.0.0.1", "root", "", "auriga_r1_r4_test", "utf8mb4", true, port), "isolated SQL connection failed");
    std::unique_ptr<SQLMsg> table(sql.DirectQuery(create.c_str()));
    Check(table && !table->uiSQLErrno, "temporary player table creation failed");
    std::unique_ptr<SQLMsg> inserted(sql.DirectQueryPrepared(query.sql.c_str(), query.params));
    Check(inserted && !inserted->uiSQLErrno && inserted->Get()->uiAffectedRows == 1 && inserted->Get()->uiInsertID,
        "production character INSERT failed");

    const auto first = query.sql.find('(');
    std::istringstream names(query.sql.substr(first + 1, query.sql.find(')', first) - first - 1));
    std::string select = "SELECT ", name;
    size_t index = 0;
    while (std::getline(names, name, ',')) {
        name.erase(0, name.find_first_not_of(' '));
        if (index) select += ", ";
        select += query.params[index++].type == SqlParamType::Blob ? "HEX(" + name + ")" : name;
    }
    select += " FROM player_test WHERE id=?";
    std::unique_ptr<SQLMsg> read(sql.DirectQueryPrepared(select.c_str(), { SqlParam::UInt(inserted->Get()->uiInsertID) }));
    Check(read && !read->uiSQLErrno && read->Get()->uiNumRows == 1, "created character could not be read back");
    MYSQL_ROW row = read->Get()->FetchRow();
    Check(row && row[0] && std::stoull(row[0]) == inserted->Get()->uiInsertID, "inserted identity changed");
    for (size_t i = 1; i < query.params.size(); ++i) {
        const auto& value = query.params[i];
        std::string expected;
        switch (value.type) {
        case SqlParamType::Signed: expected = std::to_string(value.i64); break;
        case SqlParamType::Unsigned: expected = std::to_string(value.u64); break;
        case SqlParamType::Text: expected = value.bytes; break;
        case SqlParamType::Blob:
            for (unsigned char byte : value.bytes) {
                expected += "0123456789ABCDEF"[byte >> 4];
                expected += "0123456789ABCDEF"[byte & 15];
            }
            break;
        default: throw std::runtime_error("unexpected player parameter type");
        }
        Check(row[i] && std::string(row[i]) == expected, "SQL round-trip changed a character field");
    }

    // Reuse a cached statement after terminating only this test connection.
    // The temporary table has already been verified; use a table-free SELECT
    // because connection-local tables disappear on reconnect.
    const SqlParams probeParams { SqlParam::UInt(123456789012345ULL) };
    std::unique_ptr<SQLMsg> cached(sql.DirectQueryPrepared("SELECT ?", probeParams));
    Check(cached && !cached->uiSQLErrno, "reconnect probe did not prepare");
    std::unique_ptr<SQLMsg> identity(sql.DirectQuery("SELECT CONNECTION_ID()"));
    Check(identity && !identity->uiSQLErrno && identity->Get()->uiNumRows == 1, "test SQL identity missing");
    const auto connectionId = std::stoull(identity->Get()->FetchRow()[0]);
    CAsyncSQL controller;
    controller.SetTimeouts(5);
    Check(controller.Setup("127.0.0.1", "root", "", "auriga_r1_r4_test", "utf8mb4", true, port), "test SQL controller failed");
    std::unique_ptr<SQLMsg> killed(controller.DirectQuery(("KILL CONNECTION " + std::to_string(connectionId)).c_str()));
    Check(killed && !killed->uiSQLErrno, "could not terminate the test's own SQL connection");
    std::unique_ptr<SQLMsg> reconnected(sql.DirectQueryPrepared("SELECT ?", probeParams));
    if (reconnected->uiSQLErrno)
        reconnected.reset(sql.DirectQueryPrepared("SELECT ?", probeParams));
    Check(reconnected && !reconnected->uiSQLErrno && reconnected->Get()->uiNumRows == 1 &&
        std::string(reconnected->Get()->FetchRow()[0]) == "123456789012345", "fresh query did not recover after SQL disconnect");
}
}
int main(int argc, char** argv)
{
    try {
        TPlayerTable player {};
        strlcpy(player.name, "O'Brien", sizeof(player.name));
        player.level = 123; player.st = 91; player.ht = 92; player.dx = 93; player.iq = 94;
        player.job = 3; player.voice = 2; player.dir = 7;
        player.x = -100; player.y = 200; player.z = -300;
        player.hp = (std::numeric_limits<uint64_t>::max)(); player.sp = (uint64_t(1) << 40) + 17;
        player.gold = (int64_t(1) << 45) + 39;
        player.sRandomHP = 301; player.sRandomSP = -302;
        player.stat_point = 47; player.stamina = 251; player.part_base = 8;
#ifdef ENABLE_GAYA_SYSTEM
        player.gaya = 419;
#endif
#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
        player.envanter = 12;
#endif
        std::memset(player.skills, 0xFF, sizeof(player.skills));
        player.skills[0].bLevel = 0;
        std::memset(player.quickslot, 0x80, sizeof(player.quickslot));
        player.quickslot[0] = {};
        const auto query = BuildPlayerCreateQuery("_test", 4000000000u, player);
        Check(query.sql.find("INSERT INTO player_test (") == 0, "table postfix lost");
        Check(query.sql.find(player.name) == std::string::npos, "name interpolated into SQL");
        const auto fields = Columns(query);
        auto u = [&](const char* col, uint64_t expected) {
            const auto& value = fields.at(col);
            Check(value.type == SqlParamType::Unsigned && value.u64 == expected, col);
        };
        auto i = [&](const char* col, int64_t expected) {
            const auto& value = fields.at(col);
            Check(value.type == SqlParamType::Signed && value.i64 == expected, col);
        };
        u("id", 0); u("account_id", 4000000000u); u("level", 123);
        u("st", 91); u("ht", 92); u("dx", 93); u("iq", 94);
        u("job", 3); u("voice", 2); u("dir", 7);
        i("x", -100); i("y", 200); i("z", -300);
        u("hp", player.hp); u("mp", player.sp); i("gold", player.gold);
        i("random_hp", 301); i("random_sp", -302); i("stat_point", 47); i("stamina", 251);
        u("part_base", 8); u("part_main", 8); u("part_hair", 0); u("playtime", 0);
#ifdef ENABLE_ACCE_SYSTEM
        u("part_acce", 0);
#endif
#ifdef ENABLE_GAYA_SYSTEM
        i("gaya", 419);
#endif
#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
        i("envanter", 12);
#endif
        Check(fields.at("name").type == SqlParamType::Text && fields.at("name").bytes == player.name, "name changed");
        auto blob = [&](const char* column, const void* data, size_t length) {
            const auto& value = fields.at(column);
            Check(value.type == SqlParamType::Blob && value.bytes.size() == length &&
                std::memcmp(value.bytes.data(), data, length) == 0, column);
        };
        blob("skill_level", player.skills, sizeof(player.skills));
        blob("quickslot", player.quickslot, sizeof(player.quickslot));
        if (argc == 4 && std::string(argv[1]) == "--sql")
            SqlRoundTrip(query, static_cast<unsigned int>(std::stoul(argv[2])), argv[3]);
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    std::cout << "player create: " << checks << " checks passed\n";
}
