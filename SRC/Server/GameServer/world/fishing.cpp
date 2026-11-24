#include "stdafx.h"
#include "fishing.h"
#include "affect.h"
#include "../ecs/systems/AffectSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../item/TypedRecovery.hpp"

bool fishing::PredictFish(entt::entity owner)
{
    return HasTypedTimedUtility(owner, auriga::item::TimedEffectUtility::FishMind) ||
        AffectSystem::FindAffect(owner, AFFECT_FISH_MIND_PILL) ||
        ecs::PlayerRuntime::GetPremiumRemainSeconds(owner, PREMIUM_FISH_MIND) > 0;
}
