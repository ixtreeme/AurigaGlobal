#ifndef __MINING_H
#define __MINING_H

#include <entt/entt.hpp>

namespace mining
{
	LPEVENT CreateMiningEvent(entt::entity character, entt::entity load, int count);
	uint32_t GetRawOreFromLoad(uint32_t dwLoadVnum);
	int GetFractionCount();

	// REFINE_PICK
	int RealRefinePick(entt::entity character, entt::entity item);
	void CHEAT_MAX_PICK(entt::entity character, entt::entity item);
	// END_OF_REFINE_PICK

	bool IsVeinOfOre (uint32_t vnum);
}

#endif
