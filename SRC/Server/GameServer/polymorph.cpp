#include "stdafx.h"
#include "char_interface.hpp"
#include "ecs/CharacterAccessors.hpp"
#include "ecs/EntityFactory.hpp"
#include "ecs/Registry.hpp"
#include "ecs/systems/ItemSystem.hpp"
#include "mob_manager.h"
#include "affect.h"
#include "item.h"
#include "polymorph.h"

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

bool CPolymorphUtils::PolymorphCharacter(LPCHARACTER pChar, LPITEM pItem, const CMob* pMob)
{
	uint8_t bySkillLevel = pChar->GetSkillLevel(POLYMORPH_SKILL_ID);
	uint32_t dwDuration = 0;
	uint32_t dwBonusPercent = 0;
	int iPolyPercent = 0;

	switch (pChar->GetSkillMasterType(POLYMORPH_SKILL_ID))
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
	iPolyPercent = pChar->GetLevel() - pMob->m_table.bLevel + ItemSystem::GetItemSocket(EntityFactory::CreateItemEntity(g_registry, pItem), 2) + (29 + bySkillLevel);

	if (iPolyPercent <= 0)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pChar), CHAT_TYPE_INFO, 317, "");
#endif
		return false;
	}
	else
	{
		if (number(1, 100) > iPolyPercent)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pChar), CHAT_TYPE_INFO, 317, "");
#endif
			return false;
		}
	}

	pChar->AddAffect(AFFECT_POLYMORPH, POINT_POLYMORPH, pMob->m_table.dwVnum, AFF_POLYMORPH, dwDuration, 0, true);

	// ���� ���ʽ� = �а� ��ų ���� + �а��� ����
	dwBonusPercent = bySkillLevel + ItemSystem::GetItemSocket(EntityFactory::CreateItemEntity(g_registry, pItem), 2);

	switch (GetBonusType(pMob->m_table.dwVnum))
	{
		case POLYMORPH_ATK_BONUS:
			pChar->AddAffect(AFFECT_POLYMORPH, POINT_ATT_BONUS, dwBonusPercent, AFF_POLYMORPH, dwDuration - 1, 0, false);
			break;

		case POLYMORPH_DEF_BONUS:
			pChar->AddAffect(AFFECT_POLYMORPH, POINT_DEF_BONUS, dwBonusPercent, AFF_POLYMORPH, dwDuration - 1, 0, false);
			break;

		case POLYMORPH_SPD_BONUS:
			pChar->AddAffect(AFFECT_POLYMORPH, POINT_MOV_SPEED, dwBonusPercent, AFF_POLYMORPH, dwDuration - 1, 0, false);
			break;

		default:
		case POLYMORPH_NO_BONUS:
			break;
	}

	return true;
}

bool CPolymorphUtils::UpdateBookPracticeGrade(LPCHARACTER pChar, LPITEM pItem)
{
	if (pChar == nullptr || pItem == nullptr)
		return false;

	if (ItemSystem::GetItemSocket(EntityFactory::CreateItemEntity(g_registry, pItem), 1) > 0) {
		pItem->SetSocket(1, ItemSystem::GetItemSocket(EntityFactory::CreateItemEntity(g_registry, pItem), 1) - 1);
	}
#ifdef TEXTS_IMPROVEMENT
	else {
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pChar), CHAT_TYPE_INFO, 232, "");
	}
#endif
	return true;
}

bool CPolymorphUtils::GiveBook(LPCHARACTER pChar, uint32_t dwMobVnum, uint32_t dwPracticeCount, uint8_t BookLevel, uint8_t LevelLimit)
{
	// ����0                ����1       ����2
	// �а��� ���� ��ȣ   ��������    �а��� ����
	if (pChar == nullptr)
		return false;

	LPITEM pItem = pChar->AutoGiveItem(POLYMORPH_BOOK_ID, 1);

	if (pItem == nullptr)
		return false;

	if (CMobManager::instance().Get(dwMobVnum) == nullptr)
	{
		sys_err("Wrong Polymorph vnum passed: CPolymorphUtils::GiveBook(PID(%d), %d %d %d %d)",
				((pChar)->GetPlayerID()), dwMobVnum, dwPracticeCount, BookLevel, LevelLimit);
		return false;
	}

	pItem->SetSocket(0, dwMobVnum);			// �а��� ���� ��ȣ
	pItem->SetSocket(1, dwPracticeCount);		// �����ؾ��� Ƚ��
	pItem->SetSocket(2, BookLevel);			// ���÷���
	return true;
}

bool CPolymorphUtils::BookUpgrade(LPCHARACTER pChar, LPITEM pItem)
{
	if (pChar == nullptr || pItem == nullptr)
		return false;

	pItem->SetSocket(1, ItemSystem::GetItemSocket(EntityFactory::CreateItemEntity(g_registry, pItem), 2) * 50);
	pItem->SetSocket(2, ItemSystem::GetItemSocket(EntityFactory::CreateItemEntity(g_registry, pItem), 2)+1);
	return true;
}


