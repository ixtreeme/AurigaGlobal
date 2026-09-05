#include "stdafx.h"
#include <Core/Logging.hpp>
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "ecs/systems/QuestSystem.hpp"
#include "refine.h"


#include "char_interface.hpp"
#include "item_manager.h"
#include "item.h"
#include "ecs/systems/ItemSystem.hpp"

#include "desc.h"

CRefineManager::CRefineManager()
{
}

CRefineManager::~CRefineManager()
{
}



#ifdef ENABLE_FEATURES_REFINE_SYSTEM
	int EXTRA_REFINE_POTIONS_GRADE[3] = 
	{
		REFINE_VNUM_POTION_LOW,
		REFINE_VNUM_POTION_MEDIUM,
		REFINE_VNUM_POTION_EXTRA,
	};

	int CRefineManager::Result(entt::entity ch)
	{
		int uninitialized = 0;
		int flag = ecs::QuestSystem::GetFlag(ch, REFINE_INCREASE);

		if (flag > 0)
			return flag;
		else
			return uninitialized;
	}

	bool CRefineManager::GetPercentage(LPCHARACTER ch, uint8_t lLow, uint8_t lMedium, uint8_t lExtra, uint8_t lTotal, entt::entity item)
	{
		const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;
		if (!ItemSystem::IsValidItem(item)) {
			return false;
		}

		uint8_t ar_ListType[3] = {lLow, lMedium, lExtra};

		for (int it = 0; it <= JOURNAL_MAX_NUM; it++)
		{
			if (ar_ListType[it] > 0)
			{
				//@fix 12.01.2017
				if (ItemSystem::GetItemType(item) == ITEM_METIN)
				{
					return false;
				}
				if (ch->CountSpecifyItem(EXTRA_REFINE_POTIONS_GRADE[it]) < 1)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(chEntity, CHAT_TYPE_INFO, 620, "%s",
#ifdef ENABLE_MULTI_NAMES
					ITEM_MANAGER::instance().GetTable(EXTRA_REFINE_POTIONS_GRADE[it])->szLocaleName[ecs::PlayerRuntime::GetDesc(chEntity)->GetLanguage()]
#else
					ITEM_MANAGER::instance().GetTable(EXTRA_REFINE_POTIONS_GRADE[it])->szLocaleName)
#endif
					);
#endif
					return false;
				}
			}
		}

		if (lTotal > 100 )
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(chEntity, CHAT_TYPE_INFO, 621, "");
#endif
			return false;
		}
		
		return true;
	}

	void CRefineManager::Reset(LPCHARACTER ch)
	{
		for (int it = 0; it <= JOURNAL_MAX_NUM; it++)
		{
			const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;
			char buf[MAX_HOST_LENGTH + 1];
			snprintf(buf, sizeof(buf), "refine.mode_%d", it);
			
			if (ecs::QuestSystem::GetFlag(chEntity, buf) > 0)
			{
				ch->RemoveSpecifyItem(EXTRA_REFINE_POTIONS_GRADE[it], 1);
				ecs::QuestSystem::SetFlag(chEntity, REFINE_INCREASE, 0);
				ecs::QuestSystem::SetFlag(chEntity, buf, 0);
			}
		}
	}	
	void CRefineManager::Reset_percent(LPCHARACTER ch)
	{
		for (int it = 0; it <= JOURNAL_MAX_NUM; it++)
		{
			const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;
			char buf[MAX_HOST_LENGTH + 1];
			snprintf(buf, sizeof(buf), "refine.mode_%d", it);

			if (ecs::QuestSystem::GetFlag(chEntity, buf) > 0)
			{
				//ch->RemoveSpecifyItem(EXTRA_REFINE_POTIONS_GRADE[it], 1);
				ecs::QuestSystem::SetFlag(chEntity, REFINE_INCREASE, 0);
				ecs::QuestSystem::SetFlag(chEntity, buf, 0);
			}
		}
	}
	void CRefineManager::Increase(LPCHARACTER ch, uint8_t lLow, uint8_t lMedium, uint8_t lExtra)
	{
		const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;
		int calcPercentage = 0;

		uint8_t ar_ListType[3] = {lLow, lMedium, lExtra};
		int ar_ListPercentage[3] = {REFINE_PERCENTAGE_LOW, REFINE_PERCENTAGE_MEDIUM, REFINE_PERCENTAGE_EXTRA};
		
		for (int it = 0; it <= JOURNAL_MAX_NUM; it++)
		{
			if (ar_ListType[it] > 0)
			{
				char buf[MAX_HOST_LENGTH + 1];
				snprintf(buf, sizeof(buf), "refine.mode_%d", it);
				ecs::QuestSystem::SetFlag(chEntity, buf, 1);

				calcPercentage += ar_ListPercentage[it];		
			}
		}
		
		if (ecs::QuestSystem::GetFlag(chEntity, REFINE_INCREASE) < 1)
		{
			ecs::QuestSystem::SetFlag(chEntity, REFINE_INCREASE, calcPercentage);
		}
#ifdef TEXTS_IMPROVEMENT
		else {
			ecs::ChatSystem::SendNew(chEntity, CHAT_TYPE_INFO, 622, "");
		}
#endif
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

