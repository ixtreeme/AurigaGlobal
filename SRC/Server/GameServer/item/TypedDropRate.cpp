#include "stdafx.h"
#include "TypedDropRate.hpp"
#include "TypedUniqueAccessories.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../entity/char_manager.h"
#include "../social/priv_manager.h"
#include "../core/utils.h"
#include "../core/constants.h"
#include <Core/Logging.hpp>
using auriga::item::runtime::HasEquippedAccessory;
using auriga::item::runtime::UniqueAccessory;
bool ComputeTypedDropRate(entt::entity victim, entt::entity killer,
	double& iDeltaPercent, int& iRandRange)
{
	if (victim == entt::null || killer == entt::null ||
		!g_registry.valid(victim) || !g_registry.valid(killer))
		return false;

	const int killerLevel = ecs::PointSystem::GetLevel(killer);
	iDeltaPercent = 100;
	if (!ecs::PlayerRuntime::IsStone(victim) &&
		ecs::PlayerRuntime::GetMobRank(victim) >= MOB_RANK_BOSS)
	{
		iDeltaPercent = PERCENT_LVDELTA_BOSS(
			killerLevel, ecs::PointSystem::GetLevel(victim));
	}
	else
	{
		iDeltaPercent = PERCENT_LVDELTA(
			killerLevel, ecs::PointSystem::GetLevel(victim));
	}

	const uint8_t rank = ecs::PlayerRuntime::GetMobRank(victim);
	if (1 == number(1, 50000))
		iDeltaPercent += 1000;
	else if (1 == number(1, 10000))
		iDeltaPercent += 500;

	LOG_INFO("Drop rate for level: {} rank: {} pct: {}", killerLevel, rank, iDeltaPercent);
	iDeltaPercent = iDeltaPercent * CHARACTER_MANAGER::instance().GetMobItemRate(killer) / 100;

	double itemDropBonus = ecs::PointSystem::GetPrecise(killer, POINT_ITEM_DROP_BONUS);
	if (itemDropBonus > 0)
	{
		const double added = std::min((itemDropBonus / 10) * 5, 50.0);
		iDeltaPercent += added;
	}

#ifdef ENABLE_NEW_COMMON_BONUSES
	if (ecs::PointSystem::GetPrecise(killer, POINT_DOUBLE_DROP_ITEM) > 0)
		iDeltaPercent *= 2;
#endif

	if (ecs::PlayerRuntime::GetPremiumRemainSeconds(killer, PREMIUM_ITEM) > 0)
	{
		iDeltaPercent *= 2;
	}

	iRandRange = 4000000;
#ifdef ENABLE_EVENT_MANAGER
	int extraDrop = CPrivManager::instance().GetPriv(killer, PRIV_ITEM_DROP) +
		(HasEquippedAccessory(killer, {UniqueAccessory::ThiefGloves}) ? 100 : 0);
	const auto event = CHARACTER_MANAGER::Instance().CheckEventIsActive(
		ITEM_DROP_EVENT, ecs::PlayerRuntime::GetEmpire(killer));
	if (event != nullptr)
		extraDrop += event->value[0];
	iRandRange = iRandRange * 100 / (100 + extraDrop);
#else
	const int extraDrop = CPrivManager::instance().GetPriv(killer, PRIV_ITEM_DROP) +
		(HasEquippedAccessory(killer, {UniqueAccessory::ThiefGloves}) ? 100 : 0);
	iRandRange = iRandRange * 100 / (100 + extraDrop);
#endif

	return true;
}

