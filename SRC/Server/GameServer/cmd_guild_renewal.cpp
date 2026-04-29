#include "stdafx.h"
#include "ecs/AIHelpers.hpp"
#include "ecs/systems/SocialSystem.hpp"

#ifdef ENABLE_GUILD_RENEWAL_BY_RAZOR93

#include <array>
#include <vector>
#include <cctype>
#include <cerrno>
#include <cstdlib>

#include "utils.h"
#include "char_interface.hpp"
#include "desc.h"
#include "desc_manager.h"
#include "guild.h"
#include "guild_renewal.h"

namespace
{
	static int ParseDeadlineYMD(const char* ymd)
	{
		if (!ymd)
			return 0;
		int y = 0, m = 0, d = 0;
		if (sscanf(ymd, "%d.%d.%d", &y, &m, &d) != 3)
			return 0;
		tm t = {};
		t.tm_year = y - 1900;
		t.tm_mon = m - 1;
		t.tm_mday = d;
		t.tm_hour = 23;
		t.tm_min = 59;
		t.tm_sec = 59;
		return (int)mktime(&t);
	}

	static void BroadcastRenewalStateToGuild(CGuild* g)
	{
		if (!g)
			return;

		const uint32_t guildId = g->GetID();
		const DESC_MANAGER::DESC_SET& c_ref_set = DESC_MANAGER::instance().GetClientSet();

		for (DESC_MANAGER::DESC_SET::const_iterator it = c_ref_set.begin(); it != c_ref_set.end(); ++it)
		{
			LPDESC d = *it;
			if (!d)
				continue;

			LPCHARACTER member = d->GetCharacter();
			if (!member)
				continue;

			CGuild* mg = ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(member));
			if (!mg)
				continue;

			if (mg->GetID() != guildId)
				continue;

			CGuildRenewal::instance().SendFullStateTo(member);
		}
	}
}

ACMD(do_gr_open)
{
	if (!ch)
		return;
	if (!ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(ch)))
		return;

	// UI side can hook this to open the window.
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_COMMAND, "gr_open");
	CGuildRenewal::instance().SendFullStateTo(ch);
}

ACMD(do_gr_deposit_item)
{
	if (!ch)
		return;
	CGuild* g = ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(ch));
	if (!g)
		return;

	char arg1[256], arg2[256];
	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1)
		return;

	uint16_t invCell = (uint16_t)atoi(arg1);
	uint32_t count = 0;
	if (*arg2)
		count = (uint32_t)atoi(arg2);

	if (CGuildRenewal::instance().DepositItem(ch, invCell, count))
	{
		BroadcastRenewalStateToGuild(g);
		CGuildRenewal::instance().P2P_BroadcastRefresh(g->GetID());
	}
}

ACMD(do_gr_deposit_yang)
{
	if (!ch)
		return;
	CGuild* g = ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(ch));
	if (!g)
		return;

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));
	if (!*arg1)
		return;

	int64_t yang = (int64_t)atoll(arg1);
	if (yang <= 0)
		return;

	if (CGuildRenewal::instance().DepositYang(ch, yang))
	{
		BroadcastRenewalStateToGuild(g);
		CGuildRenewal::instance().P2P_BroadcastRefresh(g->GetID());
	}
}

ACMD(do_gr_set_tax)
{
	if (!ch)
		return;

	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "A kivetett ado rendszer ki van kapcsolva (kis ado rendszer van ervenyben).");
	return;
}

ACMD(do_gr_pay_tax)
{
	if (!ch)
		return;

	CGuild* g = ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(ch));
	if (!g)
		return;

	// Kis ado: befizetes a kovetkezo szint fejlesztesi igenyeire.
	// Format:
	//  1) /gr_pay_tax <yang> <db0> <db1> <db2> <db3> <db4>
	//  2) /gr_pay_tax <yang> <vnum0> <db0> ... <vnum4> <db4>
	std::vector<long long> nums;
	nums.reserve(16);

	const char* p = argument;
	while (p && *p)
	{
		while (*p && std::isspace(static_cast<unsigned char>(*p)))
			++p;
		if (!*p)
			break;
		char* end = nullptr;
		errno = 0;
		long long v = std::strtoll(p, &end, 10);
		if (end == p)
			break;
		nums.push_back(v);
		p = end;
	}

	if (nums.empty())
	{
		ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Hasznalat: /gr_pay_tax <yang> <db0> <db1> <db2> <db3> <db4>");
		ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Vagy: /gr_pay_tax <yang> <vnum0> <db0> ... <vnum4> <db4>");
		return;
	}

	int64_t yang = (int64_t)nums[0];
	if (yang < 0)
		yang = 0;

	std::array<uint32_t, 5> vnums = { {0,0,0,0,0} };
	std::array<uint32_t, 5> counts = { {0,0,0,0,0} };

	if (nums.size() >= 11)
	{
		for (int i = 0; i < 5; ++i)
		{
			size_t vi = 1 + i * 2;
			size_t ci = vi + 1;
			if (ci >= nums.size())
				break;
			long long vv = nums[vi];
			long long cc = nums[ci];
			if (vv < 0) vv = 0;
			if (cc < 0) cc = 0;
			vnums[i] = (uint32_t)vv;
			counts[i] = (uint32_t)cc;
		}
	}
	else
	{
		for (int i = 0; i < 5; ++i)
		{
			size_t ci = 1 + i;
			if (ci >= nums.size())
				break;
			long long cc = nums[ci];
			if (cc < 0) cc = 0;
			counts[i] = (uint32_t)cc;
		}
	}

	if (CGuildRenewal::instance().PayCustom(ch, yang, vnums, counts))
	{
		BroadcastRenewalStateToGuild(g);
		CGuildRenewal::instance().P2P_BroadcastRefresh(g->GetID());
	}
}

ACMD(do_gr_levelup)
{
	if (!ch)
		return;
	CGuild* g = ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(ch));
	if (!g)
		return;

	if (CGuildRenewal::instance().TryLevelUp(ch))	// itt a belso fuggveny ir ki okot is
	{
		BroadcastRenewalStateToGuild(g);
		CGuildRenewal::instance().P2P_BroadcastRefresh(g->GetID());
	}
}

#endif // ENABLE_GUILD_RENEWAL_BY_RAZOR93

