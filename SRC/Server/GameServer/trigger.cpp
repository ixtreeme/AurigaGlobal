#include "stdafx.h"
#include "ecs/systems/AffectSystem.hpp"
#include "ecs/systems/SocialSystem.hpp"
#include "ecs/systems/PointSystem.hpp"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/CombatSystem.hpp"
#include <Core/Logging.hpp>
#include "utils.h"
#include "config.h"
#include "char_interface.hpp"
#include "ecs/CharacterAccessors.hpp"
#include "sectree_manager.h"
#include "battle.h"
#include "affect.h"
#include "shop_manager.h"
#include "ecs/AIHelpers.hpp"

#ifdef ENABLE_CPP_DUNGEON_RAZOR93
#include "OrcsDungeon.h"
#include "TritonTempleDungeon.h"
#include "ValentineDungeon.h"
#include "RuneDungeon.h"
#include "PyramidDungeonRazor93.h"
#include "NightmareDungeonRazor93.h"
#include "Halloween2022Dungeon.h"
#include "VikingDungeon.h"
#include "EasterDungeon.h"
//#include "LostCastleDungeon.h"
int OnClickOrcsDungeon(TRIGGERPARAM);
int OnClickTritonTempleDungeon(TRIGGERPARAM);
int OnClickValentineDungeon(TRIGGERPARAM);
int OnClickRuneDungeon(TRIGGERPARAM);
int OnClickPyramidDungeon(TRIGGERPARAM);
int OnClickNightmareDungeon(TRIGGERPARAM);
//int OnClickLostCastleDungeon(TRIGGERPARAM);
int OnClickHalloween2022Dungeon(TRIGGERPARAM);
int OnClickVikingDungeon(TRIGGERPARAM);
#ifdef ENABLE_NEW_CRAFT_SYSTEM_RAZOR93
int OnClickStoneCraft(TRIGGERPARAM);

#endif
int OnClickEasterDungeon(TRIGGERPARAM);
#endif

int OnClickShop(TRIGGERPARAM);
int OnClickTalk(TRIGGERPARAM);

int OnIdleDefault(TRIGGERPARAM);
int OnAttackDefault(TRIGGERPARAM);

typedef struct STriggerFunction
{
	int (*func)(TRIGGERPARAM);
} TTriggerFunction;

/*
	enum EOnClickEvents (length.h) according to the indexes:
	0 NONE
	1 SHOP
	2 TALK
	...
*/
TTriggerFunction OnClickTriggers[ON_CLICK_MAX_NUM] =
{
	{ nullptr }, // ON_CLICK_NONE
	{ OnClickShop }, // ON_CLICK_SHOP
	{ nullptr }, // ON_CLICK_TALK
#ifdef ENABLE_CPP_DUNGEON_RAZOR93
	{ OnClickOrcsDungeon }, // ON_CLICK_ORCS_DUNGEON
	{ OnClickTritonTempleDungeon }, // ON_CLICK_TRITON_TEMPLE
	{ OnClickValentineDungeon }, // ON_CLICK_VALENTINE_DUNGEON
	{ OnClickRuneDungeon }, // ON_CLICK_RUNE_DUNGEON
	{ OnClickPyramidDungeon }, // ON_CLICK_PYRAMID_DUNGEON
	{ OnClickNightmareDungeon }, // ON_CLICK_NIGHTMARE_DUNGEON
	//{ OnClickLostCastleDungeon }, // ON_CLICK_LOST_CASTLE_DUNGEON
	{ OnClickHalloween2022Dungeon }, // ON_CLICK_HALLOWEEN2022_DUNGEON
	{ OnClickVikingDungeon }, // ON_CLICK_VIKING_DUNGEON
#ifdef ENABLE_NEW_CRAFT_SYSTEM_RAZOR93
	{ OnClickStoneCraft }, // ON_CLICK_STONE_CRAFT
#endif
	 { OnClickEasterDungeon }, // ON_CLICK_EASTER_DUNGEON
#endif
};

void CHARACTER::AssignTriggers(const TMobTable* table)
{
	if (table->bOnClickType >= ON_CLICK_MAX_NUM)
	{
		LOG_ERROR("{} has invalid OnClick value {}", GetName(), table->bOnClickType);
		abort();
	}

	auto& triggerOnClick = GetTriggerOnClick();
	triggerOnClick.bType = table->bOnClickType;
	triggerOnClick.pFunc = OnClickTriggers[table->bOnClickType].func;
}

/*
 * ON_CLICK
 */
int OnClickShop(TRIGGERPARAM)
{
	CShopManager::instance().StartShopping(causer, ch);
	return 1;
}

#ifdef ENABLE_CPP_DUNGEON_RAZOR93

int OnClickOrcsDungeon(TRIGGERPARAM)
{
	if (!causer || !ecs::PlayerRuntime::IsPC(((causer) ? (causer)->GetEntityHandle() : entt::null)))
		return 0;

	// NPC vnum 9239
	if (!ch || (ecs::PlayerRuntime::GetRaceNum(((ch) ? (ch)->GetEntityHandle() : entt::null))) != 9239)
		return 0;

	COrcsDungeon::instance().OnClickNpc(causer);
	return 1;
}

int OnClickTritonTempleDungeon(TRIGGERPARAM)
{
	if (!causer || !ecs::PlayerRuntime::IsPC(((causer) ? (causer)->GetEntityHandle() : entt::null)))
		return 0;

	// NPC vnum 20094
	if (!ch || (ecs::PlayerRuntime::GetRaceNum(((ch) ? (ch)->GetEntityHandle() : entt::null))) != 20094)
		return 0;

	CTritonTempleDungeon::instance().OnClickNpc(causer);
	return 1;
}

int OnClickValentineDungeon(TRIGGERPARAM)
{
	if (!causer || !ecs::PlayerRuntime::IsPC(((causer) ? (causer)->GetEntityHandle() : entt::null)))
		return 0;

	// NPC vnum 20012
	if (!ch || (ecs::PlayerRuntime::GetRaceNum(((ch) ? (ch)->GetEntityHandle() : entt::null))) != 20012)
		return 0;

	CValentineDungeon::instance().OnClickNpc(causer);
	return 1;
}

int OnClickEasterDungeon(TRIGGERPARAM)
{
    if (!causer || !ecs::PlayerRuntime::IsPC(((causer) ? (causer)->GetEntityHandle() : entt::null)))
        return 0;

    // NPC vnum 21
    if (!ch || (ecs::PlayerRuntime::GetRaceNum(((ch) ? (ch)->GetEntityHandle() : entt::null))) != 9308)
        return 0;

    CEasterDungeon::instance().OnClickNpc(causer);
    return 1;
}

int OnClickRuneDungeon(TRIGGERPARAM)
{
	if (!causer || !ecs::PlayerRuntime::IsPC(((causer) ? (causer)->GetEntityHandle() : entt::null)))
		return 0;

	// NPC vnum 20506
	if (!ch || (ecs::PlayerRuntime::GetRaceNum(((ch) ? (ch)->GetEntityHandle() : entt::null))) != 20506)
		return 0;

	CRuneDungeon::instance().OnClickNpc(causer);
	return 1;
}

int OnClickPyramidDungeon(TRIGGERPARAM)
{
	if (!causer || !ecs::PlayerRuntime::IsPC(((causer) ? (causer)->GetEntityHandle() : entt::null)))
		return 0;

	// NPC vnum 9331
	if (!ch || (ecs::PlayerRuntime::GetRaceNum(((ch) ? (ch)->GetEntityHandle() : entt::null))) != 9331)
		return 0;

	CPyramidDungeonRazor93::instance().OnClickNpc(causer);
	return 1;
}

int OnClickNightmareDungeon(TRIGGERPARAM)
{
	if (!causer || !ecs::PlayerRuntime::IsPC(((causer) ? (causer)->GetEntityHandle() : entt::null)))
		return 0;

	if (!ch || (ecs::PlayerRuntime::GetRaceNum(((ch) ? (ch)->GetEntityHandle() : entt::null))) != 20088)
		return 0;

	CNightmareDungeonRazor93::instance().OnClickNpc(causer);
	return 1;
}
int OnClickHalloween2022Dungeon(TRIGGERPARAM)
{
	if (!causer || !ecs::PlayerRuntime::IsPC(((causer) ? (causer)->GetEntityHandle() : entt::null)))
		return 0;

	if (!ch)
		return 0;

	if ((ecs::PlayerRuntime::GetRaceNum(((ch) ? (ch)->GetEntityHandle() : entt::null))) != 9475 && (ecs::PlayerRuntime::GetRaceNum(((ch) ? (ch)->GetEntityHandle() : entt::null))) != 9484)
		return 0;

	CHalloween2022Dungeon::instance().OnClickNpc(causer, ch);
	return 1;
}

int OnClickVikingDungeon(TRIGGERPARAM)
{
	if (!causer || !ecs::PlayerRuntime::IsPC(((causer) ? (causer)->GetEntityHandle() : entt::null)))
		return 0;

	if (!ch)
		return 0;

	// entry npc: 9615
	// reward chest: 9626
	if ((ecs::PlayerRuntime::GetRaceNum(((ch) ? (ch)->GetEntityHandle() : entt::null))) != 9615 && (ecs::PlayerRuntime::GetRaceNum(((ch) ? (ch)->GetEntityHandle() : entt::null))) != 9626)
		return 0;

	CVikingDungeon::instance().OnClickNpc(causer, ch);
	return 1;
}
#ifdef ENABLE_NEW_CRAFT_SYSTEM_RAZOR93
int OnClickStoneCraft(TRIGGERPARAM)
{
	if (!causer || !ecs::PlayerRuntime::IsPC(((causer) ? (causer)->GetEntityHandle() : entt::null)))
		return 0;

	if (!ch || (ecs::PlayerRuntime::GetRaceNum(((ch) ? (ch)->GetEntityHandle() : entt::null))) != 9005)
		return 0;

	if (ecs::SocialSystem::GetExchange(((causer) ? (causer)->GetEntityHandle() : entt::null)) || causer->GetMyShop() || causer->GetShopOwner() || causer->IsOpenSafebox() || causer->IsCubeOpen())
		return 0;

	causer->SetQuestNPCID(((ch)->GetLegacyVID()));
	ecs::ChatSystem::Send(((causer) ? (causer)->GetEntityHandle() : entt::null), CHAT_TYPE_COMMAND, "stone_craft_open");
	return 1;
}
#endif
//int OnClickLostCastleDungeon(TRIGGERPARAM)
//{
//	if (!causer || !ecs::PlayerRuntime::IsPC(((causer) ? (causer)->GetEntityHandle() : entt::null)))
//		return 0;
//
//	if (!ch || (ecs::PlayerRuntime::GetRaceNum(((ch) ? (ch)->GetEntityHandle() : entt::null))) != 20021)
//		return 0;
//
//	CLostCastleDungeon::instance().OnClickNpc(causer);
//	return 1;
//}
#endif

/*
 * \xb8\xf3\xbd\xba\xc5\xcd AI \xc7?\xf6\xb5\xe9\xc0\xbb BattleAI U\xb7\xa1\xbd\xba\xb7\xce \xbc\xf6\xc1\xa4
 */
int OnIdleDefault(TRIGGERPARAM)
{
	if (ch->OnIdle())
		return PASSES_PER_SEC(1);

	return PASSES_PER_SEC(1);
}

class FuncFindMobVictim
{
public:
	FuncFindMobVictim(LPCHARACTER pkChr, int iMaxDistance) :
		m_pkChr(pkChr),
		m_iMinDistance(~(1L << 31)),
		m_iMaxDistance(iMaxDistance),
		m_lx(ecs::PlayerRuntime::GetX(((pkChr) ? (pkChr)->GetEntityHandle() : entt::null))),
		m_ly(ecs::PlayerRuntime::GetY(((pkChr) ? (pkChr)->GetEntityHandle() : entt::null))),
		m_pkChrVictim(nullptr),
		m_pkChrBuilding(nullptr)
	{
	};

	bool operator () (LPENTITY ent)
	{
		if (!ent->IsType(ENTITY_CHARACTER))
			return false;

		LPCHARACTER pkChr = (LPCHARACTER)ent;

		if (pkChr->IsBuilding() &&
			(AffectSystem::IsAffectFlag(((pkChr) ? (pkChr)->GetEntityHandle() : entt::null), AFF_BUILDING_CONSTRUCTION_SMALL) ||
				AffectSystem::IsAffectFlag(((pkChr) ? (pkChr)->GetEntityHandle() : entt::null), AFF_BUILDING_CONSTRUCTION_LARGE) ||
				AffectSystem::IsAffectFlag(((pkChr) ? (pkChr)->GetEntityHandle() : entt::null), AFF_BUILDING_UPGRADE)))
		{
			m_pkChrBuilding = pkChr;
		}

		if (ecs::PlayerRuntime::IsNPC(((pkChr) ? (pkChr)->GetEntityHandle() : entt::null)))
		{
			const entt::entity ownerEntity = ((m_pkChr) ? (m_pkChr)->GetEntityHandle() : entt::null);
			if (!pkChr->IsMonster() || !AIHelpers::IsAttackMob(ownerEntity) || AIHelpers::IsAggressive(ownerEntity))
				return false;

		}

		if (CombatSystem::IsDead(((pkChr) ? (pkChr)->GetEntityHandle() : entt::null)))
			return false;

		if (AffectSystem::IsAffectFlag(((pkChr) ? (pkChr)->GetEntityHandle() : entt::null), AFF_EUNHYUNG) ||
			AffectSystem::IsAffectFlag(((pkChr) ? (pkChr)->GetEntityHandle() : entt::null), AFF_INVISIBILITY) ||
			AffectSystem::IsAffectFlag(((pkChr) ? (pkChr)->GetEntityHandle() : entt::null), AFF_REVIVE_INVISIBLE))
			return false;

		if (AffectSystem::IsAffectFlag(((pkChr) ? (pkChr)->GetEntityHandle() : entt::null), AFF_TERROR) && AffectSystem::IsImmune(((m_pkChr) ? (m_pkChr)->GetEntityHandle() : entt::null), IMMUNE_TERROR) == false)	// \xb0\xf8\xc6\xf7 �\xb8\xae
		{
			if ((ecs::PointSystem::GetLevel(((pkChr) ? (pkChr)->GetEntityHandle() : entt::null))) >= ecs::PointSystem::GetLevel(((m_pkChr) ? (m_pkChr)->GetEntityHandle() : entt::null)))
				return false;
		}

		if (m_pkChr->IsNoAttackShinsu())
		{
			if (ecs::PlayerRuntime::GetEmpire(((pkChr) ? (pkChr)->GetEntityHandle() : entt::null)) == 1)
				return false;
		}

		if (m_pkChr->IsNoAttackChunjo())
		{
			if (ecs::PlayerRuntime::GetEmpire(((pkChr) ? (pkChr)->GetEntityHandle() : entt::null)) == 2)
				return false;
		}

		if (m_pkChr->IsNoAttackJinno())
		{
			if (ecs::PlayerRuntime::GetEmpire(((pkChr) ? (pkChr)->GetEntityHandle() : entt::null)) == 3)
				return false;
		}

		int iDistance = DISTANCE_APPROX(m_lx - ecs::PlayerRuntime::GetX(((pkChr) ? (pkChr)->GetEntityHandle() : entt::null)), m_ly - ecs::PlayerRuntime::GetY(((pkChr) ? (pkChr)->GetEntityHandle() : entt::null)));

		if (iDistance < m_iMinDistance && iDistance <= m_iMaxDistance)
		{
			m_pkChrVictim = pkChr;
			m_iMinDistance = iDistance;
		}
		return true;
	}

	LPCHARACTER GetVictim()
	{
		// \xb1\xd9�\xbf\xa1 \xb0?\xb0\xc0\xcc \xc0?\xed \xc7j\xa1 \xb8\xb9\xc0\xcc \xc0??\xe9 \xb0?\xb0\xc0\xbb \xb0\xf8\xb0\xdd\xc7?\xd9. \xb0?\xb0\xb8\xb8 \xc0?? \xb0?\xb0\xc0\xbb \xb0\xf8\xb0\xdd
		if ((m_pkChrBuilding && ((m_pkChr->GetHP() * 2) > ecs::PointSystem::GetMaxHP(((m_pkChr) ? (m_pkChr)->GetEntityHandle() : entt::null)))) || !m_pkChrVictim)
		{
			return m_pkChrBuilding;
		}

		return (m_pkChrVictim);
	}

private:
	LPCHARACTER	m_pkChr;

	int		m_iMinDistance;
	int		m_iMaxDistance;
	int32_t		m_lx;
	int32_t		m_ly;

	LPCHARACTER	m_pkChrVictim;
	LPCHARACTER	m_pkChrBuilding;
};

LPCHARACTER FindVictim(LPCHARACTER pkChr, int iMaxDistance)
{
	FuncFindMobVictim f(pkChr, iMaxDistance);
	if (ecs::PlayerRuntime::GetSectree(((pkChr) ? (pkChr)->GetEntityHandle() : entt::null)) != nullptr) {
		ecs::PlayerRuntime::GetSectree(((pkChr) ? (pkChr)->GetEntityHandle() : entt::null))->ForEachAround(f);
	}
	return f.GetVictim();
}


