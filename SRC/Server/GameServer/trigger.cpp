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
#include "ecs/components/quest_components.hpp"
#include "ecs/components/spatial_components.hpp"
#include <limits>
#include "ecs/systems/SessionSystem.hpp"
#include "questmanager.h"
#include "target.h"
#include "shop.h"
#include "attr_transfer.h"
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

void ecs::PlayerRuntime::AssignClickTrigger(entt::entity target, uint8_t type)
{
    if (!IsValid(target)) return;
    if (type >= ON_CLICK_MAX_NUM) {
        LOG_ERROR("{} has invalid OnClick value {}", GetName(target), type);
        g_registry.remove<ecs::ClickTrigger>(target);
        return;
    }
    if (!g_registry.all_of<ecs::ClickTrigger>(target))
        g_registry.insert<ecs::ClickTrigger>(&target, &target + 1);
    if (!IsValid(target)) return;
    if (auto* trigger = g_registry.try_get<ecs::ClickTrigger>(target)) {
        trigger->type = type;
        trigger->callback = OnClickTriggers[type].func;
    }
}

void ecs::PlayerRuntime::OnClick(entt::entity target, entt::entity causer)
{
    if (!IsValid(target) || !IsPC(causer)) return;

    uint32_t vid = GetPacketVID(target);
    LOG_INFO("OnClick {}[vnum: {} vid: {}] by {}", GetName(target).data(), ecs::PlayerRuntime::GetRaceNum(target), vid, GetName(causer).data());

    {
        if (ecs::SocialSystem::GetMyShop(causer) != entt::null && causer != target)
        {
            LOG_ERROR("OnClick Fail ({}->{}) - pc has shop", GetName(causer).data(), GetName(target).data());
            return;
        }
    }

    {
        if (ecs::SocialSystem::HasExchange(causer))
        {
            LOG_ERROR("OnClick Fail ({}->{}) - pc is exchanging", GetName(causer).data(), GetName(target).data());
            return;
        }
    }

    if (IsPC(target))
    {
        if (!CTargetManager::instance().GetTargetInfo(GetPlayerID(causer), TARGET_TYPE_VID, GetPacketVID(target)))
        {
            if (ecs::SocialSystem::GetMyShop(target) != entt::null)
            {
                if (CombatSystem::IsDead(causer) == true)
                    return;

                if (causer == target)
                {
                    if ((ecs::SocialSystem::HasExchange(target) || ecs::SessionSystem::IsSafeboxOpen(target) || ecs::SocialSystem::GetShopOwner(target) != entt::null) || ecs::SessionSystem::IsCubeOpen(target))
                    {
#ifdef TEXTS_IMPROVEMENT
                        ecs::ChatSystem::SendNew(causer, CHAT_TYPE_INFO, 291, "");
#endif
                        return;
                    }

#ifdef __ATTR_TRANSFER_SYSTEM__
                    if (AttrTransfer_is_open(target))
                    {
#ifdef TEXTS_IMPROVEMENT
                        ecs::ChatSystem::SendNew(causer, CHAT_TYPE_INFO, 291, "");
#endif
                        return;
                    }
#endif
                }
                else
                {
                    if ((ecs::SocialSystem::HasExchange(causer) || ecs::SessionSystem::IsSafeboxOpen(causer) || ecs::SocialSystem::GetMyShop(causer) != entt::null || ecs::SocialSystem::GetShopOwner(causer) != entt::null) || ecs::SessionSystem::IsCubeOpen(causer))
                    {
#ifdef TEXTS_IMPROVEMENT
                        ecs::ChatSystem::SendNew(causer, CHAT_TYPE_INFO, 291, "");
#endif
                        return;
                    }

#ifdef __ATTR_TRANSFER_SYSTEM__
                    if (AttrTransfer_is_open(causer))
                    {
#ifdef TEXTS_IMPROVEMENT
                        ecs::ChatSystem::SendNew(causer, CHAT_TYPE_INFO, 291, "");
#endif
                        return;
                    }
#endif

                    if ((ecs::SocialSystem::HasExchange(target) || ecs::SessionSystem::IsSafeboxOpen(target) || ecs::SessionSystem::IsCubeOpen(target)))
                    {
#ifdef TEXTS_IMPROVEMENT
                        ecs::ChatSystem::SendNew(causer, CHAT_TYPE_INFO, 369, "%s", GetName(target).data());
#endif
                        return;
                    }

#ifdef __ATTR_TRANSFER_SYSTEM__
                    if (AttrTransfer_is_open(target))
                    {
#ifdef TEXTS_IMPROVEMENT
                        ecs::ChatSystem::SendNew(causer, CHAT_TYPE_INFO, 369, "%s", GetName(target).data());
#endif
                        return;
                    }
#endif
                }

                const entt::entity currentShop = ecs::SocialSystem::GetShop(causer);
                if (currentShop != entt::null)
                {
                    ShopSystem::RemoveGuest(currentShop, causer);
                    if (!IsValid(target) || !IsPC(causer)) return;
                    ecs::SocialSystem::SetShop(causer, entt::null);
                }

                if (!IsValid(target) || !IsPC(causer)) return;
                const entt::entity myShop = ecs::SocialSystem::GetMyShop(target);
                if (myShop != entt::null) {
                    ShopSystem::AddGuest(myShop, causer, GetPacketVID(target), false);
                    if (IsValid(target) && IsPC(causer) && ecs::SocialSystem::GetShop(causer) == myShop &&
                        ecs::SocialSystem::GetMyShop(target) == myShop)
                        ecs::SocialSystem::SetShopOwner(causer, target);
                }
                return;
            }

            if (test_server)
                LOG_ERROR("{}.OnClickFailure({}) - target is PC", GetName(causer).data(), GetName(target).data());

            return;
        }
    }

    if (!SetQuestNPC(causer, target) || !IsPC(causer)) return;

    if (quest::CQuestManager::instance().Click(causer, target))
    {
        return;
    }

    if (!IsValid(target) || !IsPC(causer)) return;
    if (!IsPC(target)) {
        const auto* trigger = g_registry.try_get<ecs::ClickTrigger>(target);
        const auto callback = trigger ? trigger->callback : nullptr;
        if (callback) callback(target, causer);
    }
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
		|| ecs::SocialSystem::GetMyShop(causer) != entt::null
		|| ecs::SocialSystem::GetShopOwner(causer) != entt::null
		|| ecs::SessionSystem::IsSafeboxOpen(causer)
		|| ecs::SessionSystem::IsCubeOpen(causer))
		return 0;

	ecs::PlayerRuntime::SetQuestNPC(causer, ch);
	ecs::ChatSystem::Send(causer, CHAT_TYPE_COMMAND, "stone_craft_open");
	return 1;
}
#endif

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

// Consume the sectree's generation-aware entity snapshot directly.
class FindMobVictim
{
public:
    FindMobVictim(entt::entity self, int maxDistance)
        : m_self(self),
          m_minDistance(std::numeric_limits<int64_t>::max()),
          m_maxDistance(maxDistance),
          m_x(ecs::PlayerRuntime::GetX(self)),
          m_y(ecs::PlayerRuntime::GetY(self))
    {
    }

    bool operator()(entt::entity candidate)
    {
        if (!g_registry.valid(m_self) || candidate == m_self || !g_registry.valid(candidate))
            return false;
        const auto* kind = g_registry.try_get<ecs::SpatialKindTag>(candidate);
        if (!kind || kind->kind != ecs::SpatialKind::Character)
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

        // Same DISTANCE_APPROX coefficients, with wide coordinate differences
        // and intermediates so distant positions cannot wrap into a near target.
        const int64_t dx = std::abs(int64_t(m_x) - ecs::PlayerRuntime::GetX(candidate));
        const int64_t dy = std::abs(int64_t(m_y) - ecs::PlayerRuntime::GetY(candidate));
        const int64_t distance = (246 * std::max(dx, dy) + 102 * std::min(dx, dy)) >> 8;

        if (distance < m_minDistance && distance <= m_maxDistance)
        {
            m_victim = candidate;
            m_minDistance = distance;
        }
        return true;
    }

    entt::entity Result() const
    {
        if (!g_registry.valid(m_self)) return entt::null;
        const auto building = g_registry.valid(m_building) ? m_building : entt::entity(entt::null);
        const auto victim = g_registry.valid(m_victim) ? m_victim : entt::entity(entt::null);
        // Preserve the construction-site preference, without overflowing HP * 2.
        if ((building != entt::null &&
             ecs::PlayerRuntime::GetHP(m_self) > int64_t(ecs::PointSystem::GetMaxHP(m_self)) / 2) ||
            victim == entt::null)
            return building;
        return victim;
    }

private:
    entt::entity m_self;
    int64_t m_minDistance;
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
    if (self == entt::null || !g_registry.valid(self) || maxDistance < 0)
        return entt::null;

    LPSECTREE sectree = ecs::PlayerRuntime::GetSectree(self);
    if (!sectree)
        return entt::null;

    FindMobVictim finder(self, maxDistance);
    sectree->ForEachAround(finder);
    return finder.Result();
}

} // namespace CombatSystem



