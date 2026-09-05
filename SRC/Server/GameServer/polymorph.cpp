#include "stdafx.h"
#include "ecs/systems/PointSystem.hpp"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include <Core/Logging.hpp>
#include "ecs/systems/AffectSystem.hpp"
#include "ecs/systems/SkillSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "char_interface.hpp"
#include "ecs/CharacterAccessors.hpp"
#include "ecs/EntityFactory.hpp"
#include "ecs/Registry.hpp"
#include "ecs/systems/ItemSystem.hpp"
#include "mob_manager.h"
#include "affect.h"
#include "item.h"
#include "polymorph.h"
#include "item_manager.h"

CPolymorphUtils::CPolymorphUtils()
{
	m_mapSPDType.insert(std::make_pair(101, 101));
	m_mapSPDType.insert(std::make_pair(1901, 1901));
}

POLYMORPH_BONUS_TYPE CPolymorphUtils::GetBonusType(uint32_t dwVnum)
{
	auto iter = m_mapSPDType.find(dwVnum);

	if (iter != m_mapSPDType.end())
		return POLYMORPH_SPD_BONUS;

	iter = m_mapATKType.find(dwVnum);

	if (iter != m_mapATKType.end())
		return POLYMORPH_ATK_BONUS;

	iter = m_mapDEFType.find(dwVnum);

	if (iter != m_mapDEFType.end())
		return POLYMORPH_DEF_BONUS;

	return POLYMORPH_NO_BONUS;
}

bool CPolymorphUtils::PolymorphCharacter(
	entt::entity character, entt::entity item, const CMob* pMob)
{
	if (character == entt::null || !g_registry.valid(character) ||
		!ItemSystem::IsValidItem(item) || !pMob)
		return false;

	const uint8_t bySkillLevel = SkillSystem::GetSkillLevel(character, POLYMORPH_SKILL_ID);
	uint32_t dwDuration = 0;
	uint32_t dwBonusPercent = 0;
	int iPolyPercent = 0;

	switch (SkillSystem::GetSkillMasterType(character, POLYMORPH_SKILL_ID))
	{
		case SKILL_NORMAL:
			dwDuration = 10;
			break;

		case SKILL_MASTER:
			dwDuration = 15;
			break;

		case SKILL_GRAND_MASTER:
			dwDuration = 20;
			break;

		case SKILL_PERFECT_MASTER:
			dwDuration = 25;
			break;

		default:
			return false;
	}

	// dwDuration *= 60;

	// ���� Ȯ�� = ĳ���� ���� - �� ���� + �а��� ���� + 29 + �а� ��ų ����
	iPolyPercent = ecs::PointSystem::GetLevel(character) - pMob->m_table.bLevel +
		ItemSystem::GetItemSocket(item, 2) + (29 + bySkillLevel);

	if (iPolyPercent <= 0)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 317, "");
#endif
		return false;
	}
	else
	{
		if (number(1, 100) > iPolyPercent)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 317, "");
#endif
			return false;
		}
	}

	AffectSystem::AddAffect(character, AFFECT_POLYMORPH, POINT_POLYMORPH, pMob->m_table.dwVnum, AFF_POLYMORPH, dwDuration, 0, true);

	// ���� ���ʽ� = �а� ��ų ���� + �а��� ����
	dwBonusPercent = bySkillLevel + ItemSystem::GetItemSocket(item, 2);

	switch (GetBonusType(pMob->m_table.dwVnum))
	{
		case POLYMORPH_ATK_BONUS:
			AffectSystem::AddAffect(character, AFFECT_POLYMORPH, POINT_ATT_BONUS, dwBonusPercent, AFF_POLYMORPH, dwDuration - 1, 0, false);
			break;

		case POLYMORPH_DEF_BONUS:
			AffectSystem::AddAffect(character, AFFECT_POLYMORPH, POINT_DEF_BONUS, dwBonusPercent, AFF_POLYMORPH, dwDuration - 1, 0, false);
			break;

		case POLYMORPH_SPD_BONUS:
			AffectSystem::AddAffect(character, AFFECT_POLYMORPH, POINT_MOV_SPEED, dwBonusPercent, AFF_POLYMORPH, dwDuration - 1, 0, false);
			break;

		default:
		case POLYMORPH_NO_BONUS:
			break;
	}

	return true;
}

bool CPolymorphUtils::UpdateBookPracticeGrade(entt::entity character, entt::entity item)
{
	if (character == entt::null || !g_registry.valid(character) ||
		!ItemSystem::IsValidItem(item))
		return false;

	if (ItemSystem::GetItemSocket(item, 1) > 0) {
		ItemSystem::SetItemSocket(item, 1, ItemSystem::GetItemSocket(item, 1) - 1);
	}
#ifdef TEXTS_IMPROVEMENT
	else {
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 232, "");
	}
#endif
	return true;
}

bool CPolymorphUtils::GiveBook(entt::entity character, uint32_t dwMobVnum, uint32_t dwPracticeCount, uint8_t BookLevel, uint8_t LevelLimit)
{
	// ����0                ����1       ����2
	// �а��� ���� ��ȣ   ��������    �а��� ����
	if (character == entt::null || !g_registry.valid(character))
		return false;

	if (CMobManager::instance().Get(dwMobVnum) == nullptr)
	{
		LOG_ERROR("Wrong Polymorph vnum passed: CPolymorphUtils::GiveBook(PID({}), {} {} {} {})", ecs::PlayerRuntime::GetPlayerID(character), dwMobVnum, dwPracticeCount, BookLevel, LevelLimit);
		return false;
	}

	const entt::entity item = ITEM_MANAGER::instance().CreateItem(POLYMORPH_BOOK_ID, 1);
	if (!ItemSystem::IsValidItem(item))
		return false;

	if (!ItemSystem::SetItemSocket(item, 0, dwMobVnum) ||
		!ItemSystem::SetItemSocket(item, 1, dwPracticeCount) ||
		!ItemSystem::SetItemSocket(item, 2, BookLevel))
	{
		ItemSystem::DestroyItemEntityEcs(item, "POLYMORPH_BOOK_INIT_FAILED");
		return false;
	}

	ItemSystem::AutoGiveItem(character, item);
	return true;
}

bool CPolymorphUtils::BookUpgrade(entt::entity character, entt::entity item)
{
	if (character == entt::null || !g_registry.valid(character) || !ItemSystem::IsValidItem(item))
		return false;

	const uint32_t bookLevel = ItemSystem::GetItemSocket(item, 2);
	return ItemSystem::SetItemSocket(item, 1, bookLevel * 50) &&
		ItemSystem::SetItemSocket(item, 2, bookLevel + 1);
}


