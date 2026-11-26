#include "stdafx.h"
#include "SummonRules.hpp"
#include "BattleArena.h"
#include "../core/config.h"
bool IS_SUMMONABLE_ZONE(int map_index)
{
	switch (map_index)
	{
	case 66:
	case 71:
	case 72:
	case 73:
	case 193:
	case 216:
	case 217:
	case 208:

	case 113:
		return false;
	}

	if (CBattleArena::IsBattleArenaMap(map_index)) return false;

	if (map_index > 10000) return false;

	return true;
}

bool IS_BOTARYABLE_ZONE(int nMapIndex)
{
	if (!g_bEnableBootaryCheck) return true;

	switch (nMapIndex)
	{
	case 1:
	case 3:
	case 21:
	case 23:
	case 41:
	case 43:
		return true;
	}

	return false;
}
