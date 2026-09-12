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
#include "ecs/services/SpatialService.hpp"
#include "ecs/AIHelpers.hpp"
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
#include "ecs/systems/SessionSystem.hpp"
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
	if (causer == entt::null || !ecs::PlayerRuntime::IsPC(causer))
		return 0;

	// NPC vnum 9239
	if (ch == entt::null || (ecs::PlayerRuntime::GetRaceNum(ch)) != 9239)
		return 0;

	COrcsDungeon::instance().OnClickNpc(causer);
	return 1;
}

int OnClickTritonTempleDungeon(TRIGGERPARAM)
{
	if (causer == entt::null || !ecs::PlayerRuntime::IsPC(causer))
		return 0;

	// NPC vnum 20094
	if (ch == entt::null || (ecs::PlayerRuntime::GetRaceNum(ch)) != 20094)
		return 0;

	CTritonTempleDungeon::instance().OnClickNpc(causer);
	return 1;
}

int OnClickValentineDungeon(TRIGGERPARAM)
{
	if (causer == entt::null || !ecs::PlayerRuntime::IsPC(causer))
		return 0;

	// NPC vnum 20012
	if (ch == entt::null || (ecs::PlayerRuntime::GetRaceNum(ch)) != 20012)
		return 0;

	CValentineDungeon::instance().OnClickNpc(causer);
	return 1;
}

int OnClickEasterDungeon(TRIGGERPARAM)
{
    if (causer == entt::null || !ecs::PlayerRuntime::IsPC(causer))
        return 0;

    // NPC vnum 21
    if (ch == entt::null || (ecs::PlayerRuntime::GetRaceNum(ch)) != 9308)
        return 0;

    CEasterDungeon::instance().OnClickNpc(causer);
    return 1;
}

int OnClickRuneDungeon(TRIGGERPARAM)
{
	if (causer == entt::null || !ecs::PlayerRuntime::IsPC(causer))
		return 0;

	// NPC vnum 20506
	if (ch == entt::null || (ecs::PlayerRuntime::GetRaceNum(ch)) != 20506)
		return 0;

	CRuneDungeon::instance().OnClickNpc(causer);
	return 1;
}

int OnClickPyramidDungeon(TRIGGERPARAM)
{
	if (causer == entt::null || !ecs::PlayerRuntime::IsPC(causer))
		return 0;

	// NPC vnum 9331
	if (ch == entt::null || (ecs::PlayerRuntime::GetRaceNum(ch)) != 9331)
		return 0;

	CPyramidDungeonRazor93::instance().OnClickNpc(causer);
	return 1;
}

int OnClickNightmareDungeon(TRIGGERPARAM)
{
	if (causer == entt::null || !ecs::PlayerRuntime::IsPC(causer))
		return 0;

	if (ch == entt::null || (ecs::PlayerRuntime::GetRaceNum(ch)) != 20088)
		return 0;

	CNightmareDungeonRazor93::instance().OnClickNpc(causer);
	return 1;
}
int OnClickHalloween2022Dungeon(TRIGGERPARAM)
{
	if (causer == entt::null || !ecs::PlayerRuntime::IsPC(causer))
		return 0;

	if (ch == entt::null)
		return 0;

	if ((ecs::PlayerRuntime::GetRaceNum(ch)) != 9475 && (ecs::PlayerRuntime::GetRaceNum(ch)) != 9484)
		return 0;

	CHalloween2022Dungeon::instance().OnClickNpc(causer, ch);
	return 1;
}

int OnClickVikingDungeon(TRIGGERPARAM)
{
	if (causer == entt::null || !ecs::PlayerRuntime::IsPC(causer))
		return 0;

	if (ch == entt::null)
		return 0;

	// entry npc: 9615
	// reward chest: 9626
	if ((ecs::PlayerRuntime::GetRaceNum(ch)) != 9615 && (ecs::PlayerRuntime::GetRaceNum(ch)) != 9626)
		return 0;

	CVikingDungeon::instance().OnClickNpc(causer, ch);
	return 1;
}
#ifdef ENABLE_NEW_CRAFT_SYSTEM_RAZOR93
int OnClickStoneCraft(TRIGGERPARAM)
{
	if (causer == entt::null || !ecs::PlayerRuntime::IsPC(causer))
		return 0;

	if (ch == entt::null || (ecs::PlayerRuntime::GetRaceNum(ch)) != 9005)
		return 0;

	if (ecs::SocialSystem::HasExchange(causer)
		|| ecs::SocialSystem::GetMyShop(causer)
		|| ecs::SocialSystem::GetShopOwner(causer) != entt::null
		|| ecs::SessionSystem::IsSafeboxOpen(causer)
		|| ecs::SessionSystem::IsCubeOpen(causer))
		return 0;

	ecs::PlayerRuntime::SetQuestNPCID(causer, ecs::PlayerRuntime::GetPacketVID(ch));
	ecs::ChatSystem::Send(causer, CHAT_TYPE_COMMAND, "stone_craft_open");
	return 1;
}
#endif
//int OnClickLostCastleDungeon(TRIGGERPARAM)
//{
//	if (!causer || !ecs::PlayerRuntime::IsPC(((causer) ? (causer)->GetEntityHandle() : entt::null)))
//		return 0;
//
//	if (ch == entt::null || (ecs::PlayerRuntime::GetRaceNum(ch)) != 20021)
//		return 0;
//
//	CLostCastleDungeon::instance().OnClickNpc(((causer) ? (causer)->GetEntityHandle() : entt::null));
//	return 1;
//}
#endif

/*
 * \xb8\xf3\xbd\xba\xc5\xcd AI \xc7?\xf6\xb5\xe9\xc0\xbb BattleAI U\xb7\xa1\xbd\xba\xb7\xce \xbc\xf6\xc1\xa4
 */
int OnIdleDefault(TRIGGERPARAM)
{
	// CHARACTER::OnIdle was a bare return false with no side effects at all,
	// so the resolve and the call went with it.
	return PASSES_PER_SEC(1);
}

namespace {

// Target search for a mob, entity in and entity out. The sectree hands back an
// LPENTITY, so that one conversion happens once per candidate and the handle
// stays an entity from there - no candidate is ever turned into a character.
class FindMobVictim
{
public:
    FindMobVictim(entt::entity self, int maxDistance)
        : m_self(self),
          m_minDistance(~(1L << 31)),
          m_maxDistance(maxDistance),
          m_x(ecs::PlayerRuntime::GetX(self)),
          m_y(ecs::PlayerRuntime::GetY(self))
    {
    }

    bool operator()(LPENTITY ent)
    {
        if (!ent || !ent->IsType(ENTITY_CHARACTER))
            return false;

        const entt::entity candidate = ecs::SpatialService::EntityFromLPENTITY(ent);
        if (candidate == entt::null || !g_registry.valid(candidate))
            return false;

        if (ecs::PlayerRuntime::IsBuilding(candidate) &&
            (AffectSystem::IsAffectFlag(candidate, AFF_BUILDING_CONSTRUCTION_SMALL) ||
             AffectSystem::IsAffectFlag(candidate, AFF_BUILDING_CONSTRUCTION_LARGE) ||
             AffectSystem::IsAffectFlag(candidate, AFF_BUILDING_UPGRADE)))
        {
            m_building = candidate;
        }

        if (ecs::PlayerRuntime::IsNPC(candidate))
        {
            // These three flags belong to the searcher, not to what it is
            // looking at: "will this mob attack other mobs, and is it already
            // aggressive". The entity-conversion pass that came through here
            // read them off the candidate instead, which asked whether the
            // prey was an attack-mob. Restored to the searcher.
            if (!ecs::PlayerRuntime::IsMonster(candidate) ||
                !AIHelpers::IsAttackMob(m_self) ||
                AIHelpers::IsAggressive(m_self))
                return false;
        }

        if (CombatSystem::IsDead(candidate))
            return false;

        if (AffectSystem::IsAffectFlag(candidate, AFF_EUNHYUNG) ||
            AffectSystem::IsAffectFlag(candidate, AFF_INVISIBILITY) ||
            AffectSystem::IsAffectFlag(candidate, AFF_REVIVE_INVISIBLE))
            return false;

        if (AffectSystem::IsAffectFlag(candidate, AFF_TERROR) &&
            !AffectSystem::IsImmune(candidate, IMMUNE_TERROR))
        {
            // Terror only spares the candidate while the searcher is not below
            // it in level. Both sides of this comparison had become the
            // candidate, which made it true for everything and dropped every
            // terrified target regardless of level.
            if (ecs::PointSystem::GetLevel(m_self) >= ecs::PointSystem::GetLevel(candidate))
                return false;
        }

        if (AIHelpers::IsNoAttackShinsu(m_self) && ecs::PlayerRuntime::GetEmpire(candidate) == 1)
            return false;
        if (AIHelpers::IsNoAttackChunjo(m_self) && ecs::PlayerRuntime::GetEmpire(candidate) == 2)
            return false;
        if (AIHelpers::IsNoAttackJinno(m_self) && ecs::PlayerRuntime::GetEmpire(candidate) == 3)
            return false;

        const int distance = DISTANCE_APPROX(m_x - ecs::PlayerRuntime::GetX(candidate),
                                             m_y - ecs::PlayerRuntime::GetY(candidate));

        if (distance < m_minDistance && distance <= m_maxDistance)
        {
            m_victim = candidate;
            m_minDistance = distance;
        }
        return true;
    }

    entt::entity Result() const
    {
        // A construction site is worth hitting only while this mob is still
        // healthy; otherwise, and whenever nothing else was found, it is the
        // answer anyway.
        if ((m_building != entt::null &&
             ecs::PlayerRuntime::GetHP(m_self) * 2 > ecs::PointSystem::GetMaxHP(m_self)) ||
            m_victim == entt::null)
            return m_building;

        return m_victim;
    }

private:
    entt::entity m_self;
    int m_minDistance;
    int m_maxDistance;
    int32_t m_x;
    int32_t m_y;
    entt::entity m_victim { entt::null };
    entt::entity m_building { entt::null };
};

} // namespace

namespace CombatSystem {

// Lives here rather than in CombatSystem.cpp because the sectree scan and its
// helpers are already set up in this translation unit.
entt::entity FindVictim(entt::entity self, int maxDistance)
{
    if (self == entt::null || !g_registry.valid(self))
        return entt::null;

    LPSECTREE sectree = ecs::PlayerRuntime::GetSectree(self);
    if (!sectree)
        return entt::null;

    FindMobVictim finder(self, maxDistance);
    sectree->ForEachAround(finder);
    return finder.Result();
}

} // namespace CombatSystem



