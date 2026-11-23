#include "stdafx.h"
#include "../ecs/AIHelpers.hpp"
#include "../ecs/CharacterAccessors.hpp"
#include "../ecs/systems/SocialSystem.hpp"

#ifdef ENABLE_GUILD_RENEWAL_BY_RAZOR93

#include <array>
#include <charconv>
#include <cstring>

#include "utils.h"
#include "char_interface.hpp"
#include "guild.h"
#include "guild_renewal.h"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"

namespace
{
	template <typename Number>
	bool ReadNumber(const char*& line, Number& value)
	{
		char token[32];
		line = one_argument(line, token, sizeof(token));
		const auto end = token + std::strlen(token);
		const auto result = std::from_chars(token, end, value);
		return result.ec == std::errc{} && result.ptr == end;
	}
}

ACMD(do_gr_open)
{
	if (!ecs::IsCharacter(character))
		return;
	if (!ecs::SocialSystem::GetGuild(character))
		return;

	// UI side can hook this to open the window.
	ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "gr_open");
	CGuildRenewal::instance().SendFullStateTo(character);
}

ACMD(do_gr_deposit_item)
{
	if (!ecs::IsCharacter(character))
		return;
	uint32_t target = 0, count = 0;
	uint64_t itemId = 0;
	const char* line = argument;
	if (ReadNumber(line, target) && ReadNumber(line, itemId) && ReadNumber(line, count))
		CGuildRenewal::instance().DepositItem(character, target, itemId, count);
}

ACMD(do_gr_deposit_yang)
{
	if (!ecs::IsCharacter(character))
		return;
	uint32_t target = 0;
	int64_t yang = 0;
	const char* line = argument;
	if (ReadNumber(line, target) && ReadNumber(line, yang) && yang > 0)
		CGuildRenewal::instance().PayCustom(character, target, yang, {});
}

ACMD(do_gr_set_tax)
{
	if (!ecs::PlayerRuntime::IsValid(character))
		return;

	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "A kivetett ado rendszer ki van kapcsolva (kis ado rendszer van ervenyben).");
	return;
}

ACMD(do_gr_pay_tax)
{
	if (!ecs::IsCharacter(character))
		return;
	uint32_t target = 0;
	int64_t yang = 0;
	const char* line = argument;
	if (!ReadNumber(line, target) || !ReadNumber(line, yang) || yang < 0)
		return;
	std::array<uint32_t,5> counts{};
	for (auto& count : counts)
		if (!ReadNumber(line, count))
			return;
	CGuildRenewal::instance().PayCustom(character, target, yang, counts);
}

ACMD(do_gr_levelup)
{
	if (ecs::IsCharacter(character))
		CGuildRenewal::instance().TryLevelUp(character);
}

#endif
