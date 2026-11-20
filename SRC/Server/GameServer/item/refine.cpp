#include "stdafx.h"
#include <Core/Logging.hpp>
#include "../ecs/systems/QuestSystem.hpp"
#include "refine.h"

CRefineManager::CRefineManager()
{
}

CRefineManager::~CRefineManager()
{
}



#ifdef ENABLE_FEATURES_REFINE_SYSTEM
	int CRefineManager::Result(entt::entity ch)
	{
		int uninitialized = 0;
		int flag = ecs::QuestSystem::GetFlag(ch, REFINE_INCREASE);

		if (flag > 0)
			return flag;
		else
			return uninitialized;
	}

#endif

bool CRefineManager::Initialize(TRefineTable * table, int size)
{
	for (int i = 0; i < size; ++i, ++table)
	{
		LOG_TRACE("REFINE {} prob {} cost {}", table->id, table->prob, table->cost);
		m_map_RefineRecipe.insert(std::make_pair(table->id, *table));
	}

	LOG_TRACE("REFINE: COUNT {}", m_map_RefineRecipe.size());
	return true;
}

const TRefineTable* CRefineManager::GetRefineRecipe(uint32_t vnum)
{
	if (vnum == 0)
		return nullptr;

	auto it = m_map_RefineRecipe.find(vnum);
	LOG_TRACE("REFINE: FIND {} {}", vnum, it == m_map_RefineRecipe.end() ? "FALSE" : "TRUE");

	if (it == m_map_RefineRecipe.end())
	{
		return nullptr;
	}

	return &it->second;
}

