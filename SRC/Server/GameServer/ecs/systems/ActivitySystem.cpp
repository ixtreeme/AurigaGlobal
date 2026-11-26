#include "../../core/stdafx.h"
#include "../../mission_pass/MissionPassEvents.hpp"
#include "PointSystem.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "../../item/ItemStore.hpp"
#include "../../item/TypedGathering.hpp"

#include "ActivitySystem.hpp"
#include "../Registry.hpp"
#include "../../core/log.h"

#ifdef ENABLE_NEW_FISHING_SYSTEM

#include "../../entity/char.h"
#include "../../entity/char_manager.h"
#include "../../core/config.h"
#include "../../network/desc.h"
#include "../../world/fishing.h"
#include "../../core/locale_service.h"
#include "../../world/mining.h"
#include "../../core/packet.h"
#include "../../world/sectree.h"
#include "../../world/sectree_manager.h"
#include "../../item/unique_item.h"
#include "../../core/vector.h"
#include "../CharacterAccessors.hpp"
#include "../EntityFactory.hpp"
#include "../NetworkService.hpp"
#include "../SpatialHelpers.hpp"
#include "../Registry.hpp"
#include "../VIDRegistry.hpp"
#include "../components/activity_components.hpp"
#include "../components/dirty_components.hpp"
#include "../components/identity_components.hpp"
#include <Core/Logging.hpp>

namespace
{

ecs::FishingState* GetFishingState(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    return &g_registry.get_or_emplace<ecs::FishingState>(e);
}


void MarkFishing(entt::entity e, bool active)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    if (active)
        g_registry.emplace_or_replace<ecs::FishingActiveTag>(e);
    else if (g_registry.all_of<ecs::FishingActiveTag>(e))
        g_registry.remove<ecs::FishingActiveTag>(e);

    g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

} // namespace

namespace ActivitySystem {

void StopFishing(entt::entity fisher)
{
    if (fisher == entt::null || !g_registry.valid(fisher))
        return;

    auto* state = GetFishingState(fisher);
    if (!state || !state->fishingNewEvent)
        return;

    event_cancel(&state->fishingNewEvent);
    state->fishingNewEvent = nullptr;
    state->elapsedSeconds = 0;
    state->fishVnum = 0;
    state->chance = 0;


    MarkFishing(fisher, false);

    TPacketFishingNew p;
    p.header = HEADER_GC_FISHING_NEW;
    p.subheader = FISHING_SUBHEADER_NEW_STOP;
    p.vid = ecs::PlayerRuntime::GetPacketVID(fisher);
    p.dir = 0;
    p.need = 0;
    p.count = 0;
    ecs::NetworkService::BroadcastToView(g_registry, fisher, &p, sizeof(p), false);
}

bool IsFishing(entt::entity fisher)
{
    if (fisher == entt::null || !g_registry.valid(fisher))
        return false;

    const auto* state = g_registry.try_get<ecs::FishingState>(fisher);
    return state && state->fishingNewEvent;
}

bool IsMining(entt::entity miner)
{
    if (miner == entt::null || !g_registry.valid(miner))
        return false;

    const auto* state = g_registry.try_get<ecs::MiningState>(miner);
    return state && state->event;
}

void CancelMining(entt::entity miner)
{
    if (miner == entt::null || !g_registry.valid(miner))
        return;

    auto* state = g_registry.try_get<ecs::MiningState>(miner);
    if (!state || !state->event)
        return;

    LOG_INFO("XXX MINING CANCEL entity {}", static_cast<uint32_t>(miner));
    event_cancel(&state->event);
    state->event = nullptr;
    state->load = entt::null;
    g_registry.emplace_or_replace<ecs::DirtyTag>(miner);
#ifdef TEXTS_IMPROVEMENT
    ecs::ChatSystem::SendNew(miner, CHAT_TYPE_INFO, 472, "");
#endif
}

bool StartMining(entt::entity miner, entt::entity load)
{
    return TryStartTypedMining(miner, load);
}

} // namespace ActivitySystem

#endif


