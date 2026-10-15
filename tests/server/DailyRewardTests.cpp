#include "../../SRC/Server/GameServer/core/stdafx.h"
#include "../../SRC/Server/GameServer/command/daily_reward.h"
#include "../../SRC/Server/GameServer/network/db.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ChatSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ItemSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include <Core/Logging.hpp>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

// The production command/daily_reward.cpp against a scripted database. The
// queries go into one FIFO, the way the async worker runs them on its single
// connection, and the database answers them from its own tables; a callback
// that queues the next query puts it at the back, behind everything already
// sent. Characters are bare entities, chat and item grants are recorded.
entt::registry g_registry;

namespace {
int checks = 0;
void Check(bool value, const char* why) { ++checks; if (!value) throw std::runtime_error(why); }

struct TestPlayer { uint32_t pid { 0 }; };

struct StatusRow
{
    int64_t time { 0 };
    std::string reward;
    uint32_t total { 0 };
};

struct PendingQuery
{
    std::string sql;
    SqlParams params;
    CFuncQueryInfo* info { nullptr };
};

struct Grant
{
    entt::entity owner;
    uint32_t vnum;
    uint32_t count;
};

std::deque<PendingQuery> pending;
std::map<uint32_t, StatusRow> status;
std::set<std::pair<std::string, int64_t>> machineClaims;
std::map<std::string, std::vector<std::pair<std::string, std::string>>> rewardItems;
std::map<entt::entity, std::vector<std::string>> chat;
std::vector<Grant> grants;
std::function<void(const PendingQuery&)> beforeExecute;
int64_t now = 1'800'000'000;
constexpr int64_t DAY = 24 * 60 * 60;

void Reset()
{
    for (auto& query : pending)
        M2_DELETE(query.info);
    pending.clear();
    status.clear();
    machineClaims.clear();
    rewardItems = {
        { "0", { { "27001", "5" } } },
        { "1", { { "27002", "3" } } },
    };
    chat.clear();
    grants.clear();
    beforeExecute = {};
    g_registry.clear();
}

entt::entity Player(uint32_t pid)
{
    const auto e = g_registry.create();
    g_registry.emplace<TestPlayer>(e, TestPlayer { pid });
    return e;
}

bool StartsWith(const std::string& text, const char* prefix)
{
    return text.rfind(prefix, 0) == 0;
}

uint32_t TrailingNumber(const std::string& text)
{
    const auto at = text.find_last_not_of("0123456789");
    return static_cast<uint32_t>(std::stoul(text.substr(at + 1)));
}

// The pid of "... VALUES(<pid>, NOW(), 0, 0)".
uint32_t ValuesPid(const std::string& text)
{
    const auto open = text.find("VALUES(");
    return static_cast<uint32_t>(std::stoul(text.substr(open + 7)));
}

struct Result
{
    std::vector<std::vector<std::string>> rows;
    uint64_t affected { 0 };
};

Result Execute(const PendingQuery& query)
{
    const std::string& sql = query.sql;
    Result result;

    if (StartsWith(sql, "SELECT reward from player.daily_reward_status where (NOW() > time) and pid = ")) {
        const auto it = status.find(TrailingNumber(sql));
        if (it != status.end() && now > it->second.time)
            result.rows.push_back({ it->second.reward });
    } else if (StartsWith(sql, "INSERT INTO player.daily_reward_claim_hwid")) {
        Check(sql.find("WHERE NOT EXISTS") != std::string::npos ? query.params.size() == 4 && query.params[3].bytes == query.params[0].bytes
            : query.params.size() == 3, "the machine claim binds its key");
        const bool onlyIfFree = sql.find("WHERE NOT EXISTS") != std::string::npos;
        const auto key = std::make_pair(query.params[0].bytes, now / DAY);
        if (!onlyIfFree || !machineClaims.contains(key)) {
            machineClaims.insert(key);
            result.affected = 1;
        }
    } else if (StartsWith(sql, "UPDATE daily_reward_status SET reward = CASE")) {
        const bool dueOnly = sql.find("AND (NOW() > time)") != std::string::npos;
        const bool sameReward = sql.find("AND reward = ?") != std::string::npos;
        const auto it = status.find(static_cast<uint32_t>(query.params.at(0).u64));
        if (it != status.end() && (!dueOnly || now > it->second.time) &&
            (!sameReward || it->second.reward == query.params.at(1).bytes)) {
            it->second.reward = std::to_string((std::stoi(it->second.reward) + 1) % 7);
            it->second.total += 1;
            it->second.time = now + DAY;
            result.affected = 1;
        }
    } else if (StartsWith(sql, "SELECT items, count from player.daily_reward_items where reward = ? ORDER BY RAND() limit 1")) {
        const auto it = rewardItems.find(query.params.at(0).bytes);
        if (it != rewardItems.end() && !it->second.empty())
            result.rows.push_back({ it->second.front().first, it->second.front().second });
    } else if (StartsWith(sql, "SELECT UNIX_TIMESTAMP(time), reward FROM player.daily_reward_status WHERE pid = ")) {
        const auto it = status.find(TrailingNumber(sql));
        if (it != status.end())
            result.rows.push_back({ std::to_string(it->second.time), it->second.reward });
    } else if (StartsWith(sql, "DELETE FROM player.daily_reward_status WHERE pid = ")) {
        result.affected = status.erase(TrailingNumber(sql));
    } else if (StartsWith(sql, "INSERT INTO player.daily_reward_status (pid, time, reward, total_rewards) VALUES(")) {
        status[ValuesPid(sql)] = StatusRow { now, "0", 0 };
        result.affected = 1;
    } else if (StartsWith(sql, "SELECT items, count FROM player.daily_reward_items WHERE reward = ?")) {
        const auto it = rewardItems.find(query.params.at(0).bytes);
        if (it != rewardItems.end())
            for (const auto& [items, count] : it->second)
                result.rows.push_back({ items, count });
    } else {
        throw std::runtime_error("unexpected query: " + sql);
    }
    return result;
}

// Runs the worker until nothing is queued, answering each callback.
void RunDatabase()
{
    while (!pending.empty()) {
        PendingQuery query = std::move(pending.front());
        pending.pop_front();
        if (beforeExecute)
            beforeExecute(query);
        const Result result = Execute(query);
        if (!query.info)
            continue;

        SQLMsg msg;
        auto* res = new SQLResult;
        res->bPrepared = true;
        for (const auto& row : result.rows) {
            std::vector<char*> cells;
            for (const auto& cell : row) {
                res->preparedCells.push_back(cell);
                cells.push_back(res->preparedCells.back().data());
            }
            res->preparedRows.push_back(std::move(cells));
        }
        res->uiNumRows = result.rows.size();
        res->uiAffectedRows = result.affected;
        msg.vec_pkResult.push_back(res);
        query.info->f(&msg);
        M2_DELETE(query.info);
    }
}

size_t GrantsTo(entt::entity e)
{
    size_t count = 0;
    for (const auto& grant : grants)
        count += grant.owner == e;
    return count;
}

bool Said(entt::entity e, const std::string& text)
{
    for (const auto& line : chat[e])
        if (line == text)
            return true;
    return false;
}

const std::string kAlreadyCollected = "Napi 1 jutalom jar. // You have already collected the reward.";
const std::string kNothingDue = "#715";
}

// --- doubles -----------------------------------------------------------------
std::shared_ptr<spdlog::logger> logging::GetErrorLogger()
{
    static auto logger = std::make_shared<spdlog::logger>("daily-reward-error-test");
    return logger;
}
std::shared_ptr<spdlog::logger> logging::GetLogger()
{
    static auto logger = std::make_shared<spdlog::logger>("daily-reward-test");
    return logger;
}
CSemaphore::CSemaphore() = default;
CSemaphore::~CSemaphore() = default;
CAsyncSQL::CAsyncSQL() = default;
CAsyncSQL::~CAsyncSQL() = default;
DBManager::DBManager() = default;
DBManager::~DBManager() = default;
DBManager db_manager;

void CAsyncSQL::ReturnQuery(const char* query, void* userData)
{
    pending.push_back(PendingQuery { query, {}, static_cast<CFuncQueryInfo*>(userData) });
}
void CAsyncSQL::ReturnQueryPrepared(const char* query, const SqlParams& params, void* userData)
{
    pending.push_back(PendingQuery { query, params, static_cast<CFuncQueryInfo*>(userData) });
}
void DBManager::Query(const char* format, ...)
{
    char text[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    pending.push_back(PendingQuery { text, {}, nullptr });
}
MYSQL_ROW SQLResult::FetchRow()
{
    if (preparedRowPos >= preparedRows.size())
        return nullptr;
    return preparedRows[preparedRowPos++].data();
}

bool ecs::PlayerRuntime::IsValid(entt::entity e) { return g_registry.valid(e) && g_registry.all_of<TestPlayer>(e); }
uint32_t ecs::PlayerRuntime::GetPlayerID(entt::entity e) { return IsValid(e) ? g_registry.get<TestPlayer>(e).pid : 0; }
void ecs::ChatSystem::Send(entt::entity e, uint8_t, const char* format, ...)
{
    char text[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    chat[e].emplace_back(text);
}
void ecs::ChatSystem::SendNew(entt::entity e, uint8_t, uint32_t idx, const char*, ...)
{
    chat[e].push_back("#" + std::to_string(idx));
}
entt::entity ItemSystem::AutoGiveItemEcs(entt::entity owner, uint32_t vnum, uint32_t count, int, bool, bool)
{
    grants.push_back(Grant { owner, vnum, count });
    return entt::null;
}

namespace {
void DueRow(uint32_t pid, const char* reward = "0")
{
    status[pid] = StatusRow { now - 60, reward, 0 };
}

void OneClaimGivesOneItem()
{
    Reset();
    const auto e = Player(10);
    DueRow(10);
    DailyReward::Claim(e, "hw-a|1.2.3.4", 7);
    RunDatabase();
    Check(grants.size() == 1 && grants[0].owner == e && grants[0].vnum == 27001 && grants[0].count == 5,
        "a due reward gives its item");
    Check(status[10].reward == "1" && status[10].total == 1 && status[10].time == now + DAY,
        "the claim moves the reward on and starts the next day");
    Check(machineClaims.size() == 1, "the machine's claim for the day is taken");
}

void RepeatedClaimsGiveOneItem()
{
    Reset();
    const auto e = Player(11);
    DueRow(11);
    for (int i = 0; i < 5; ++i)
        DailyReward::Claim(e, "hw-b|1.2.3.4", 8);
    RunDatabase();
    Check(GrantsTo(e) == 1, "five claims sent at once give one item");
    Check(status[11].total == 1 && status[11].reward == "1", "the reward moves on once");
}

void OneMachineOneRewardADay()
{
    Reset();
    const auto first = Player(12), second = Player(13);
    DueRow(12);
    DueRow(13);
    DailyReward::Claim(first, "hw-c|1.2.3.4", 9);
    DailyReward::Claim(second, "hw-c|1.2.3.4", 10);
    RunDatabase();
    Check(grants.size() == 1 && GrantsTo(first) == 1, "two characters on one machine get one reward");
    Check(Said(second, kAlreadyCollected), "the second is told the machine already collected");
    Check(status[13].total == 0 && status[13].reward == "0", "the refused character keeps its reward due");
}

void TwoMachinesBothClaim()
{
    Reset();
    const auto first = Player(14), second = Player(15);
    DueRow(14);
    DueRow(15, "1");
    DailyReward::Claim(first, "hw-d|1.2.3.4", 11);
    DailyReward::Claim(second, "hw-e|5.6.7.8", 12);
    RunDatabase();
    Check(GrantsTo(first) == 1 && GrantsTo(second) == 1, "different machines each claim");
    Check(grants[1].vnum == 27002, "each claim gives the item of its own reward");
}

// Another channel takes the claim between this request's status read and its
// claim: the read said due, the row no longer is.
void ClaimTakenElsewhereGivesNothing()
{
    Reset();
    const auto e = Player(16);
    DueRow(16);
    beforeExecute = [](const PendingQuery& query) {
        if (StartsWith(query.sql, "UPDATE daily_reward_status")) {
            beforeExecute = {};
            status[16] = StatusRow { now + DAY, "1", 1 };
        }
    };
    DailyReward::Claim(e, "hw-f|1.2.3.4", 13);
    RunDatabase();
    Check(grants.empty(), "a claim another channel took gives nothing here");
    Check(Said(e, kNothingDue), "the player is told nothing is due");
    Check(status[16].total == 1, "the row is claimed once");
}

// Same, but the other channel already moved the reward on and its day has
// passed again: due, yet a different reward than the one this request read.
void ClaimOfAnOlderRewardGivesNothing()
{
    Reset();
    const auto e = Player(17);
    DueRow(17);
    beforeExecute = [](const PendingQuery& query) {
        if (StartsWith(query.sql, "UPDATE daily_reward_status")) {
            beforeExecute = {};
            status[17] = StatusRow { now - 1, "1", 1 };
        }
    };
    DailyReward::Claim(e, "hw-g|1.2.3.4", 14);
    RunDatabase();
    Check(grants.empty(), "a claim for a reward that moved on gives nothing");
    Check(status[17].reward == "1" && status[17].total == 1, "the newer reward stays due");
}

void NothingDueGivesNothing()
{
    Reset();
    const auto e = Player(18);
    status[18] = StatusRow { now + 3600, "2", 4 };
    DailyReward::Claim(e, "hw-h|1.2.3.4", 15);
    RunDatabase();
    Check(grants.empty() && Said(e, kNothingDue), "a reward that is not due yet is refused");
    Check(machineClaims.empty(), "a refused claim does not use up the machine's day");
}

void NoMachineKeyIsRefused()
{
    Reset();
    const auto e = Player(19);
    DueRow(19);
    DailyReward::Claim(e, std::string(), 0);
    RunDatabase();
    Check(grants.empty() && Said(e, kAlreadyCollected), "without a machine key nothing is claimed");
    Check(status[19].total == 0, "the reward stays due");
}

void CharacterGoneMidChain()
{
    Reset();
    const auto e = Player(20);
    DueRow(20);
    beforeExecute = [e](const PendingQuery& query) {
        if (StartsWith(query.sql, "INSERT INTO player.daily_reward_claim_hwid")) {
            beforeExecute = {};
            g_registry.destroy(e);
        }
    };
    DailyReward::Claim(e, "hw-i|1.2.3.4", 16);
    RunDatabase();
    Check(grants.empty() && status[20].total == 0, "a character gone mid-chain claims nothing");
}

void ReloadResetsAMissingRow()
{
    Reset();
    const auto e = Player(21);
    DailyReward::Reload(e);
    RunDatabase();
    Check(status.contains(21) && status[21].reward == "0" && status[21].time == now, "a missing row is created");
    Check(Said(e, "ManagerGiftSystem DeleteRewards|") && Said(e, "ManagerGiftSystem SetReward|27001|5") &&
        Said(e, "ManagerGiftSystem SetDailyReward|0") && Said(e, "ManagerGiftSystem SetRewardDone|"),
        "the window gets the reward and its items");
}
}

int main()
{
    try {
        OneClaimGivesOneItem();
        RepeatedClaimsGiveOneItem();
        OneMachineOneRewardADay();
        TwoMachinesBothClaim();
        ClaimTakenElsewhereGivesNothing();
        ClaimOfAnOlderRewardGivesNothing();
        NothingDueGivesNothing();
        NoMachineKeyIsRefused();
        CharacterGoneMidChain();
        ReloadResetsAMissingRow();
        Reset();
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << "\n";
        return 1;
    }
    std::cout << "daily reward: " << checks << " checks passed\n";
    return 0;
}
