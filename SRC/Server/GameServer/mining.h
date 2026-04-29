#ifndef __MINING_H
#define __MINING_H

#include <entt/entt.hpp>

namespace mining
{
	LPEVENT CreateMiningEvent(LPCHARACTER ch, LPCHARACTER load, int count);
	uint32_t GetRawOreFromLoad(uint32_t dwLoadVnum);
	bool OreRefine(LPCHARACTER ch, LPCHARACTER npc, entt::entity item, int cost, int pct, entt::entity metinstone_item);
	int GetFractionCount();

	// REFINE_PICK
	int RealRefinePick(LPCHARACTER ch, entt::entity item);
	void CHEAT_MAX_PICK(LPCHARACTER ch, entt::entity item);
	// END_OF_REFINE_PICK

	bool IsVeinOfOre (uint32_t vnum);
}

#endif
