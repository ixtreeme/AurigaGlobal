#include "stdafx.h"
#include "ecs/Registry.hpp"
#include "ecs/systems/ItemSystem.hpp"
#include "constants.h"
#include "log.h"
#include "char_interface.hpp"
#include "ecs/CharacterAccessors.hpp"
#include "over9refine.h"

namespace
{
bool ReplaceItemWith(entt::entity character, entt::entity source, uint32_t resultVnum)
{
	if (character == entt::null || !g_registry.valid(character) ||
		!ItemSystem::IsValidItem(source) ||
		ItemSystem::GetItemOwner(source) != character || resultVnum == 0)
		return false;

	LPCHARACTER legacyCharacter = ecs::LegacyCharOf(character);
	if (!legacyCharacter)
		return false;

	const entt::entity result = ItemSystem::CreateItemEcs(resultVnum, 1);
	if (!ItemSystem::IsValidItem(result))
		return false;

	if (!ItemSystem::CopyItemSocketsEcs(source, result) ||
		!ItemSystem::CopyItemAttributesEcs(source, result))
	{
		ItemSystem::DestroyItemEntityEcs(result, "OVER9_COPY_ROLLBACK");
		return false;
	}

	const int emptyCell = ItemSystem::GetEmptyInventoryPositionEcs(character, result);
	if (emptyCell < 0 || !ItemSystem::PlaceItemEcs(
		character, result, INVENTORY, static_cast<uint16_t>(emptyCell)))
	{
		ItemSystem::DestroyItemEntityEcs(result, "OVER9_PLACE_ROLLBACK");
		return false;
	}

	char hint[256];
	snprintf(hint, sizeof(hint), "SUCCESS %u %s %u",
		ItemSystem::GetItemID(result), ItemSystem::GetItemName(result),
		ItemSystem::GetItemOriginalVnum(result));
	LogManager::instance().ItemLogEntity(legacyCharacter, source, "REFINE OVER9", hint);

	if (!ItemSystem::DestroyItemEntityEcs(source, "REFINE OVER9"))
	{
		ItemSystem::DestroyItemEntityEcs(result, "OVER9_SOURCE_ROLLBACK");
		return false;
	}

	ItemSystem::FlushDelayedSaveEcs(result);
	return true;
}
}

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

bool COver9RefineManager::Change9ToOver9(entt::entity character, entt::entity item)
{
	if (!ItemSystem::IsValidItem(item))
		return false;

	OVER9ITEM_MAP::iterator iter = m_mapItem.find(ItemSystem::GetItemVnum(item));

	if (iter == m_mapItem.end())
		return false;

	return ReplaceItemWith(character, item, iter->second);
}

bool COver9RefineManager::Over9Refine(entt::entity character, entt::entity item)
{
	return ItemSystem::IsValidItem(item) &&
		ReplaceItemWith(character, item, ItemSystem::GetItemRefineVnum(item));
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


