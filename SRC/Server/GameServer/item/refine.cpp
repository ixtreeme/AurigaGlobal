#include "stdafx.h"
#include <Core/Logging.hpp>
#include "refine.h"

CRefineManager::CRefineManager()
{
}

CRefineManager::~CRefineManager()
{
}



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

