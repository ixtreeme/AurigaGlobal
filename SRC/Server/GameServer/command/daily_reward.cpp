#include "stdafx.h"
#include "daily_reward.h"
#include "../ecs/systems/ChatSystem.hpp"
#include "../ecs/systems/ItemSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "db.h"
#include "utils.h"

#include <cstdlib>
#include <ctime>
#include <functional>
#include <string>

// Daily reward reload: the status read, the optional reset and the item
// listing run on the DB worker; every follow-up query is queued after the
// previous one on the same connection, so the ordering is kept.
static void DailyRewardReloadFinalCallback(SQLMsg* msg, entt::entity character);

static void DailyRewardReloadItemsCallback(SQLMsg* msg, entt::entity character, std::string time, std::string rewards)
{
	if (!ecs::PlayerRuntime::IsValid(character))
		return;

	if (msg && msg->Get() && msg->Get()->uiNumRows > 0)
	{
		MYSQL_ROW row;
		while ((row = msg->Get()->FetchRow()) != nullptr)
			ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "ManagerGiftSystem SetReward|%s|%s", row[0], row[1]);
	}

	ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "ManagerGiftSystem SetTime|%s", time.c_str());
	ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "ManagerGiftSystem SetDailyReward|%s", rewards.c_str());
	ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "ManagerGiftSystem SetRewardDone|");
}

static void DailyRewardReloadStatusCallback(SQLMsg* msg, entt::entity character)
{
	if (!ecs::PlayerRuntime::IsValid(character))
		return;

	const uint32_t pid = ecs::PlayerRuntime::GetPlayerID(character);

	// A stale row (older than a day) is reset; the queued DELETE/INSERT are
	// executed before the follow-up SELECT below.
	bool hasRow = msg && msg->Get() && msg->Get()->uiNumRows > 0;
	bool reset = false;

	if (hasRow)
	{
		MYSQL_ROW row = msg->Get()->FetchRow();

		if (row && row[0])
		{
			const int64_t lastTime = atoll(row[0]);
			reset = (lastTime + 24 * 60 * 60) < static_cast<int64_t>(time(nullptr));
		}
	}
	else
	{
		reset = true;
	}

	if (reset)
	{
		DBManager::instance().Query("DELETE FROM player.daily_reward_status WHERE pid = %u", pid);
		DBManager::instance().Query("INSERT INTO player.daily_reward_status (pid, time, reward, total_rewards) VALUES(%u, NOW(), 0, 0)", pid);

#ifdef TEXTS_IMPROVEMENT
		if (hasRow)
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 721, "");
#endif
	}

	DBManager::instance().FuncQuery(std::bind(&DailyRewardReloadFinalCallback, std::placeholders::_1, character),
		"SELECT UNIX_TIMESTAMP(time), reward FROM player.daily_reward_status WHERE pid = %u", pid);
}

static void DailyRewardReloadFinalCallback(SQLMsg* msg, entt::entity character)
{
	if (!ecs::PlayerRuntime::IsValid(character))
		return;

	std::string time = "";
	std::string rewards = "";

	if (msg && msg->Get() && msg->Get()->uiNumRows > 0)
	{
		MYSQL_ROW row;
		while ((row = msg->Get()->FetchRow()) != nullptr)
		{
			time = row[0] ? row[0] : "";
			rewards = row[1] ? row[1] : "";
		}
	}

	DBManager::instance().FuncQueryPrepared(std::bind(&DailyRewardReloadItemsCallback, std::placeholders::_1, character, time, rewards),
		"SELECT items, count FROM player.daily_reward_items WHERE reward = ?", { SqlParam::Text(rewards) });
}

void DailyReward::Reload(entt::entity character)
{
	if (!ecs::PlayerRuntime::IsValid(character)) {
		return;
	}

	ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "ManagerGiftSystem DeleteRewards|");

	DBManager::instance().FuncQuery(std::bind(&DailyRewardReloadStatusCallback, std::placeholders::_1, character),
		"SELECT UNIX_TIMESTAMP(time), reward FROM player.daily_reward_status WHERE pid = %u", (ecs::PlayerRuntime::GetPlayerID(character)));
}

// Claiming. The status read only says a reward may be due; what claims it is
// the conditional UPDATE, which moves the reward on only while the row still
// has it due and still names the reward that was read. The worker runs every
// request's queries in order on one connection, so of several claims sent
// before the first one finished exactly one UPDATE matches, and only that one
// gives an item; a claim another channel took in between matches nothing
// either. With the HWID limit the machine's claim for the day is taken first
// the same way: the INSERT adds a row only while the key has none for today.
static void DailyRewardItemsGivenCallback(SQLMsg* msg, entt::entity character)
{
	if (!ecs::PlayerRuntime::IsValid(character))
		return;

	std::string items = "";
	std::string counts = "";

	if (msg && msg->Get() && msg->Get()->uiNumRows > 0)
	{
		MYSQL_ROW row;
		while ((row = msg->Get()->FetchRow()) != nullptr)
		{
			items = row[0] ? row[0] : "";
			counts = row[1] ? row[1] : "";
		}
	}

	uint32_t item = 0;
	uint32_t count = 0;

	str_to_number(item, items.c_str());
	str_to_number(count, counts.c_str());
	ItemSystem::AutoGiveItemEcs(character, item, count);
}

static void DailyRewardClaimedCallback(SQLMsg* msg, entt::entity character, std::string rewards)
{
	if (!ecs::PlayerRuntime::IsValid(character))
		return;

	if (!msg || !msg->Get() || msg->Get()->uiAffectedRows != 1)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 715, "");
#endif
		return;
	}

	DBManager::instance().FuncQueryPrepared(std::bind(&DailyRewardItemsGivenCallback, std::placeholders::_1, character),
		"SELECT items, count from player.daily_reward_items where reward = ? ORDER BY RAND() limit 1", { SqlParam::Text(rewards) });
}

static void DailyRewardClaimStatus(entt::entity character, const std::string& rewards)
{
	DBManager::instance().FuncQueryPrepared(std::bind(&DailyRewardClaimedCallback, std::placeholders::_1, character, rewards),
		"UPDATE daily_reward_status SET reward = CASE WHEN reward = 0 THEN '1' WHEN reward = 1 THEN '2' WHEN reward = 2 THEN '3' WHEN reward = 3 THEN '4' WHEN reward = 4 THEN '5' WHEN reward = 5 THEN '6' WHEN reward = 6 THEN '0' END, total_rewards = total_rewards +1, time = (NOW() + interval 1 day) "
		"WHERE pid = ? AND (NOW() > time) AND reward = ?",
		{ SqlParam::UInt(ecs::PlayerRuntime::GetPlayerID(character)), SqlParam::Text(rewards) });
}

#ifdef ENABLE_DAILY_REWARD_HWID_LIMIT_RAZOR93
static void DailyRewardMachineClaimedCallback(SQLMsg* msg, entt::entity character, std::string rewards)
{
	if (!ecs::PlayerRuntime::IsValid(character))
		return;

	// HWID limit: one machine, one reward a day.
	if (!msg || !msg->Get() || msg->Get()->uiAffectedRows != 1)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Napi 1 jutalom jar. // You have already collected the reward.");
		return;
	}

	DailyRewardClaimStatus(character, rewards);
}
#endif

static void DailyRewardGetCallback(SQLMsg* msg, entt::entity character, std::string hwKey, uint32_t accountId)
{
	if (!ecs::PlayerRuntime::IsValid(character))
		return;

	std::string rewards = "";
	bool reward = false;

	if (msg && msg->Get() && msg->Get()->uiNumRows > 0)
	{
		MYSQL_ROW row;
		while ((row = msg->Get()->FetchRow()) != nullptr)
			rewards = row[0] ? row[0] : "";

		reward = true;
	}

	if (!reward)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 715, "");
#endif
		return;
	}

#ifdef ENABLE_DAILY_REWARD_HWID_LIMIT_RAZOR93
	if (hwKey.empty())
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Napi 1 jutalom jar. // You have already collected the reward.");
		return;
	}

	DBManager::instance().FuncQueryPrepared(std::bind(&DailyRewardMachineClaimedCallback, std::placeholders::_1, character, rewards),
		"INSERT INTO player.daily_reward_claim_hwid (hwkey, claim_day, pid, account_id) "
			"SELECT ?, CURDATE(), ?, ? FROM DUAL "
			"WHERE NOT EXISTS (SELECT 1 FROM player.daily_reward_claim_hwid WHERE hwkey = ? AND claim_day = CURDATE())",
		{
			SqlParam::Text(hwKey),
			SqlParam::UInt(ecs::PlayerRuntime::GetPlayerID(character)),
			SqlParam::UInt(accountId),
			SqlParam::Text(hwKey)
		});
#else
	(void)hwKey;
	(void)accountId;
	DailyRewardClaimStatus(character, rewards);
#endif
}

void DailyReward::Claim(entt::entity character, std::string hwKey, uint32_t accountId)
{
	if (!ecs::PlayerRuntime::IsValid(character))
		return;

	DBManager::instance().FuncQuery(std::bind(&DailyRewardGetCallback, std::placeholders::_1, character, std::move(hwKey), accountId),
		"SELECT reward from player.daily_reward_status where (NOW() > time) and pid = %u", (ecs::PlayerRuntime::GetPlayerID(character)));
}
