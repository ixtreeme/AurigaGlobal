#include "../../core/stdafx.h"
#include "../../mission_pass/MissionPassEvents.hpp"
#include "PointSystem.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "../../item/ItemStore.hpp"

#include "ActivitySystem.hpp"
#include "ItemSystem.hpp"
#include "../Registry.hpp"
#include "../../core/log.h"

#ifdef ENABLE_NEW_FISHING_SYSTEM

#include "../../entity/char.h"
#include "../../entity/char_manager.h"
#include "../../core/config.h"
#include "../../network/desc.h"
#include "../../world/fishing.h"
#include "../../item/item.h"
#include "../../item/item_manager.h"
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

    const entt::entity rod = ItemSystem::GetWearItem(fisher, WEAR_WEAPON);
    if (ItemSystem::IsValidItem(rod) && ItemSystem::GetItemType(rod) == ITEM_ROD)
        ItemSystem::SetItemSocket(rod, 2, 0);

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

void FinishMining(entt::entity miner)
{
    if (miner == entt::null || !g_registry.valid(miner))
        return;

    auto* state = g_registry.try_get<ecs::MiningState>(miner);
    if (!state)
        return;

    state->event = nullptr;
    state->load = entt::null;
    g_registry.emplace_or_replace<ecs::DirtyTag>(miner);
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
    if(g_registry.valid(miner) && g_registry.all_of<ecs::TypedItems>(miner))return false;
    if (miner == entt::null || load == entt::null ||
        !g_registry.valid(miner) || !g_registry.valid(load))
        return false;

    if (IsMining(miner)) {
        CancelMining(miner);
        return false;
    }

    if (ecs::PlayerRuntime::GetMapIndex(miner) != ecs::PlayerRuntime::GetMapIndex(load) ||
        DISTANCE_APPROX(
            ecs::PlayerRuntime::GetX(miner) - ecs::PlayerRuntime::GetX(load),
            ecs::PlayerRuntime::GetY(miner) - ecs::PlayerRuntime::GetY(load)) > 1000 ||
        mining::GetRawOreFromLoad(ecs::PlayerRuntime::GetRaceNum(load)) == 0)
        return false;

    const entt::entity pick = ItemSystem::GetWearItem(miner, WEAR_WEAPON);
    if (!ItemSystem::IsValidItem(pick) || ItemSystem::GetItemType(pick) != ITEM_PICK)
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(miner, CHAT_TYPE_INFO, 252, "");
#endif
        return false;
    }

    const int count = number(5, 15);
    TPacketGCDigMotion packet{};
    packet.header = HEADER_GC_DIG_MOTION;
    packet.vid = ecs::PlayerRuntime::GetPacketVID(miner);
    packet.target_vid = ecs::PlayerRuntime::GetPacketVID(load);
    packet.count = count;
    ecs::NetworkService::BroadcastToView(
        g_registry, miner, &packet, sizeof(packet), false);

    LPEVENT event = mining::CreateMiningEvent(miner, load, count);
    if (!event)
        return false;

    auto& state = g_registry.get_or_emplace<ecs::MiningState>(miner);
    state.event = event;
    state.load = load;
    g_registry.emplace_or_replace<ecs::DirtyTag>(miner);
    return true;
}

int RefineFishingRod(entt::entity owner, entt::entity rod)
{
    if (owner == entt::null || !g_registry.valid(owner) ||
        !ItemSystem::IsValidItem(rod))
        return 2;

    const bool refinable = ItemSystem::GetItemType(rod) == ITEM_ROD &&
        !ItemSystem::IsItemEquipped(rod) &&
        ItemSystem::GetItemSocket(rod, 0) == ItemSystem::GetItemValue(rod, 2);
    if (!refinable)
    {
        LOG_ERROR("REFINE_ROD_HACK pid({}) item({}:{})",
            ecs::PlayerRuntime::GetPlayerID(owner), ItemSystem::GetItemName(rod),
            ItemSystem::GetItemID(rod));
        LogManager::instance().RefineLog(
            ecs::PlayerRuntime::GetPlayerID(owner), ItemSystem::GetItemName(rod),
            ItemSystem::GetItemID(rod), -1, 1, "ROD_HACK");
        return 6;
    }

    const int advance = ItemSystem::GetItemValue(rod, 0) / 10;
    const uint16_t cell = ItemSystem::GetItemCell(rod);
    const bool success = number(1, 100) <= ItemSystem::GetItemValue(rod, 3);
    LogManager::instance().RefineLog(
        ecs::PlayerRuntime::GetPlayerID(owner), ItemSystem::GetItemName(rod),
        ItemSystem::GetItemID(rod), advance, success ? 1 : 0, "ROD");

    if (success)
    {
        const entt::entity newRod = ITEM_MANAGER::instance().CreateItem(
            ItemSystem::GetItemRefineVnum(rod), 1);
        if (!ItemSystem::IsValidItem(newRod))
            return 4;

        ItemSystem::DestroyItemEntityEcs(rod, "REMOVE (REFINE FISH_ROD)");
        if (!ItemSystem::PlaceItemEcs(owner, newRod, INVENTORY, cell))
        {
            ItemSystem::DestroyItemEntityEcs(newRod, "REFINE_FISH_ROD_PLACE_FAIL");
            return 4;
        }
        LogManager::instance().ItemLogEntity(
            owner, newRod, "REFINE FISH_ROD SUCCESS",
            ItemSystem::GetItemName(newRod));
        return 1;
    }

#ifdef ENABLE_FISHINGROD_RENEWAL
    const int current = ItemSystem::GetItemSocket(rod, 0);
    ItemSystem::SetItemSocket(
        rod, 0, current > 0 ? current - (current * 20 / 100) : 0);
    LogManager::instance().ItemLogEntity(
        owner, rod, "REFINE FISH_ROD FAIL", ItemSystem::GetItemName(rod));
#else
    const entt::entity newRod = ITEM_MANAGER::instance().CreateItem(
        ItemSystem::GetItemValue(rod, 4), 1);
    if (!ItemSystem::IsValidItem(newRod))
        return 3;

    ItemSystem::DestroyItemEntityEcs(rod, "REMOVE (REFINE FISH_ROD)");
    if (!ItemSystem::PlaceItemEcs(owner, newRod, INVENTORY, cell))
    {
        ItemSystem::DestroyItemEntityEcs(newRod, "REFINE_FISH_ROD_PLACE_FAIL");
        return 3;
    }
    LogManager::instance().ItemLogEntity(
        owner, newRod, "REFINE FISH_ROD FAIL", ItemSystem::GetItemName(newRod));
#endif
    return 2;
}

} // namespace ActivitySystem

#endif


