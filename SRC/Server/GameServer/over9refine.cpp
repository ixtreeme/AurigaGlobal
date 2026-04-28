#include "stdafx.h"
#include "ecs/EntityFactory.hpp"
#include "ecs/Registry.hpp"
#include "ecs/systems/ItemSystem.hpp"
#include "constants.h"
#include "log.h"
#include "char_interface.hpp"
#include "ecs/CharacterAccessors.hpp"
#include "item_manager.h"
#include "item.h"
#include "over9refine.h"

void COver9RefineManager::enableOver9Refine(uint32_t dwVnumFrom, uint32_t dwVnumTo)
{
	m_mapItem.insert(std::make_pair(dwVnumFrom, dwVnumTo));
}

int COver9RefineManager::canOver9Refine(uint32_t dwVnum)
{
	OVER9ITEM_MAP::iterator iter = m_mapItem.find(dwVnum);

	if (iter != m_mapItem.end())
		return 1;

	if (dwVnum % 10 == 9)
		return 0;

	dwVnum -= dwVnum % 10;

	for (iter = m_mapItem.begin(); iter != m_mapItem.end(); ++iter)
		if (iter->second == dwVnum)
			return 2;

	return 0;
}

bool COver9RefineManager::Change9ToOver9(LPCHARACTER pChar, LPITEM item)
{
	OVER9ITEM_MAP::iterator iter = m_mapItem.find(ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, item)));

	if (iter == m_mapItem.end())
		return false;

	uint32_t dwVnum = iter->second;

	LPITEM over9 = ITEM_MANAGER::instance().CreateItem(dwVnum, 1);

	if (over9 == nullptr)
		return false;

	item->CopySocketTo(over9);
	item->CopyAttributeTo(over9);

	int iEmptyCell = pChar->GetEmptyInventory(over9->GetSize());

	if (iEmptyCell == -1)
		return false;

	item->RemoveFromCharacter();

	over9->AddToCharacter(pChar, TItemPos(INVENTORY, iEmptyCell));

	char szBuf[256];
	snprintf(szBuf, sizeof(szBuf), "SUCCESS %u %s %u", ItemSystem::GetItemID(EntityFactory::CreateItemEntity(g_registry, over9)), over9->GetName(), over9->GetOriginalVnum());
	LogManager::instance().ItemLog(pChar, item, "REFINE OVER9", szBuf);
	return true;
}

bool COver9RefineManager::Over9Refine(LPCHARACTER pChar, LPITEM item)
{
	uint32_t dwVnum = item->GetRefinedVnum();

	if (dwVnum == 0)
		return false;

	LPITEM over9 = ITEM_MANAGER::instance().CreateItem(dwVnum, 1);

	if (over9 == nullptr)
		return false;

	item->CopySocketTo(over9);
	item->CopyAttributeTo(over9);

	int iEmptyCell = pChar->GetEmptyInventory(over9->GetSize());

	if (iEmptyCell == -1)
		return false;

	item->RemoveFromCharacter();

	over9->AddToCharacter(pChar, TItemPos(INVENTORY, iEmptyCell));

	char szBuf[256];
	snprintf(szBuf, sizeof(szBuf), "SUCCESS %u %s %u", ItemSystem::GetItemID(EntityFactory::CreateItemEntity(g_registry, over9)), over9->GetName(), over9->GetOriginalVnum());
	LogManager::instance().ItemLog(pChar, item, "REFINE OVER9", szBuf);
	return true;
}

uint32_t COver9RefineManager::GetMaterialVnum(uint32_t baseVnum)
{
	OVER9ITEM_MAP::iterator iter = m_mapItem.find(baseVnum);

	if (iter != m_mapItem.end())
		return (baseVnum - (baseVnum % 10));

	baseVnum -= baseVnum % 10;

	for (iter = m_mapItem.begin(); iter != m_mapItem.end(); ++iter)
		if (iter->second == baseVnum)
			return (iter->first - (iter->first % 10));

	return 0;
}


