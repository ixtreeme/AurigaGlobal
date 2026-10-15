#pragma once

#include <cstdint>
#include <string>

#include <entt/entity/entity.hpp>

// The daily reward's DB chains, behind the gift window's two commands. Every
// query runs on the async DB worker, which executes them in order on one
// connection; the callbacks continue the chain on the game thread.
namespace DailyReward
{
	// Resets a stale (or missing) status row, then sends the window the
	// time, the due reward and its items.
	void Reload(entt::entity character);

	// Claims the reward the status row has due and gives one of its items.
	// With the HWID limit the machine's claim for the day comes first; hwKey
	// is the HWID+host (or account) key and accountId is logged with it.
	void Claim(entt::entity character, std::string hwKey, uint32_t accountId);
}
