#include "../../stdafx.h"
#include "PointSystem.hpp"
#include "PlayerRuntimeSystem.hpp"

#include "ActivitySystem.hpp"
#include "ItemSystem.hpp"
#include "../Registry.hpp"
#include "../../log.h"

#ifdef ENABLE_NEW_FISHING_SYSTEM

#include "../../battle_pass.h"
#include "../../char.h"
#include "../../char_manager.h"
#include "../../config.h"
#include "../../desc.h"
#include "../../fishing.h"
#include "../../item.h"
#include "../../item_manager.h"
#include "../../locale_service.h"
#include "../../mining.h"
#include "../../packet.h"
#include "../../sectree.h"
#include "../../sectree_manager.h"
#include "../../unique_item.h"
#include "../../vector.h"
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

#ifdef ENABLE_BATTLE_PASS
void UpdateFishingBattlePassLegacyBoundary(entt::entity fisher)
{
    LPCHARACTER ch = ecs::LegacyCharOf(fisher);
    if (!ch)
        return;

    const uint8_t battlePassId = ch->GetBattlePassId();
    if (!battlePassId)
        return;

    uint32_t count = 0;
    uint32_t unused = 0;
    if (CBattlePass::instance().BattlePassMissionGetInfo(
            battlePassId, CATCH_FISH, &unused, &count) &&
        ch->GetMissionProgress(CATCH_FISH, battlePassId) < count)
    {
        ch->UpdateMissionProgress(CATCH_FISH, battlePassId, 1, count);
    }
}
#endif

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

EVENTFUNC(ecs_fishing_event)
{
    fishingnew_event_info* info = dynamic_cast<fishingnew_event_info*>(event->info);
    if (info == nullptr)
        return 0;

    const entt::entity fisher = ecs::PlayerRuntime::FindByPlayerID(info->pid);
    if (fisher == entt::null || !g_registry.valid(fisher))
        return 0;

    auto* state = GetFishingState(fisher);
    if (!state)
        return 0;

    if (state->catchCount >= FISHING_NEED_CATCH) {
        ActivitySystem::CatchDecision(fisher, info->vnum);
        return 0;
    }

    const entt::entity rod = ItemSystem::GetWearItem(fisher, WEAR_WEAPON);
    if (!ItemSystem::IsValidItem(rod) || ItemSystem::GetItemType(rod) != ITEM_ROD) {
        ActivitySystem::StopFishing(fisher);
        return 0;
    }

    if (info->sec == 1)
    {
        TItemTable* pTable = ITEM_MANAGER::instance().GetTable(info->vnum);
        if (pTable)
        {
#ifdef TEXTS_IMPROVEMENT
#ifdef ENABLE_MULTI_NAMES
            uint8_t lang = 0;
            if (LPDESC d = ecs::PlayerRuntime::GetDesc(fisher))
                lang = d->GetLanguage();
            ecs::ChatSystem::SendNew(fisher, CHAT_TYPE_INFO, 896, "%s", pTable->szLocaleName[lang]);
#else
            ecs::ChatSystem::SendNew(fisher, CHAT_TYPE_INFO, 896, "%s", pTable->szLocaleName);
#endif
#endif
        }
        else
        {
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(fisher, CHAT_TYPE_INFO, 897, "");
#endif
        }
    }

    if (state->catchFailed > 0) {
        info->sec += state->catchFailed;
        state->catchFailed = 0;
    }

    if (info->sec >= 15) {
        ActivitySystem::StopFishing(fisher);
        return 0;
    }

    ++info->sec;
    state->elapsedSeconds = info->sec;
    g_registry.emplace_or_replace<ecs::DirtyTag>(fisher);
    return PASSES_PER_SEC(1);
}

} // namespace

namespace ActivitySystem {

void StartFishing(entt::entity fisher, uint32_t)
{
    if (fisher == entt::null || !g_registry.valid(fisher))
        return;

    auto* state = GetFishingState(fisher);
    if (!state || state->fishingNewEvent)
        return;

    const int x = ecs::PlayerRuntime::GetX(fisher);
    const int y = ecs::PlayerRuntime::GetY(fisher);
    LPSECTREE tree = ecs::SectorAt(ecs::PlayerRuntime::GetMapIndex(fisher), x, y);
    if (!tree)
        return;

    if (tree->IsAttr(x, y, ATTR_BLOCK)) {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(fisher, CHAT_TYPE_INFO, 894, "");
#endif
        return;
    }

    if (!ItemSystem::HasMainInventorySpaceEcs(fisher)) {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(fisher, CHAT_TYPE_INFO, 899, "");
#endif
        return;
    }

    const entt::entity rod = ItemSystem::GetWearItem(fisher, WEAR_WEAPON);
    if (!ItemSystem::IsValidItem(rod) || ItemSystem::GetItemType(rod) != ITEM_ROD) {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(fisher, CHAT_TYPE_INFO, 895, "");
#endif
        return;
    }

    if (ItemSystem::GetItemSocket(rod, 2) == 0) {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(fisher, CHAT_TYPE_INFO, 281, "");
#endif
        return;
    }

    const float rotation = ecs::PlayerRuntime::GetRotation(fisher);

    const uint32_t rodVnum = ItemSystem::GetItemVnum(rod);
    const bool second = !(rodVnum >= 27400 && rodVnum <= 27490);

    fishingnew_event_info* info = AllocEventInfo<fishingnew_event_info>();
    info->pid = ecs::PlayerRuntime::GetPlayerID(fisher);
    info->vnum = fishingnew::GetFishCatchedVnum(
        100,
        15 + ecs::PointSystem::Get(fisher, POINT_FISHING_RARE) + ItemSystem::GetItemSocket(rod, 2),
        second);
    info->chance = 100;
    info->sec = 1;

    state->fishVnum = info->vnum;
    state->chance = info->chance;
    state->elapsedSeconds = info->sec;
    state->catchCount = 0;
    state->catchFailed = 0;
    state->lastCatchTime = 0;
    state->fishingNewEvent = event_create(ecs_fishing_event, info, PASSES_PER_SEC(1));

    MarkFishing(fisher, true);

    TPacketFishingNew p;
    p.header = HEADER_GC_FISHING_NEW;
    p.subheader = FISHING_SUBHEADER_NEW_START;
    p.vid = ecs::PlayerRuntime::GetPacketVID(fisher);
    p.dir = static_cast<uint8_t>(rotation / 5);
    p.need = FISHING_NEED_CATCH;
    p.count = 0;
    ecs::NetworkService::BroadcastToView(g_registry, fisher, &p, sizeof(p), false);
}

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

void CatchFishing(entt::entity fisher, uint32_t)
{
    if (fisher == entt::null || !g_registry.valid(fisher))
        return;

    auto* state = GetFishingState(fisher);
    if (!state || !state->fishingNewEvent)
        return;

    if (state->lastCatchTime > get_global_time())
        return;

    state->catchCount = static_cast<uint8_t>(state->catchCount + 1);
    state->lastCatchTime = get_global_time() + 1;
    g_registry.emplace_or_replace<ecs::DirtyTag>(fisher);

    TPacketFishingNew p;
    p.header = HEADER_GC_FISHING_NEW;
    p.subheader = FISHING_SUBHEADER_NEW_CATCH;
    p.vid = ecs::PlayerRuntime::GetPacketVID(fisher);
    p.dir = 0;
    p.need = 0;
    p.count = state->catchCount;
    ecs::NetworkService::BroadcastToView(g_registry, fisher, &p, sizeof(p), false);
}

void CatchFishingFailed(entt::entity fisher)
{
    if (fisher == entt::null || !g_registry.valid(fisher))
        return;

    auto* state = GetFishingState(fisher);
    if (!state || !state->fishingNewEvent)
        return;

    ++state->catchFailed;
    g_registry.emplace_or_replace<ecs::DirtyTag>(fisher);

    TPacketFishingNew p;
    p.header = HEADER_GC_FISHING_NEW;
    p.subheader = FISHING_SUBHEADER_NEW_CATCH_FAILED;
    p.vid = ecs::PlayerRuntime::GetPacketVID(fisher);
    p.dir = 0;
    p.need = 0;
    p.count = 0;
    ecs::NetworkService::BroadcastToView(g_registry, fisher, &p, sizeof(p), false);
}

void CatchDecision(entt::entity fisher, uint32_t itemVnum)
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

    const entt::entity rod = ItemSystem::GetWearItem(fisher, WEAR_WEAPON);
    if (!ItemSystem::IsValidItem(rod))
        return;

    if (ItemSystem::GetItemType(rod) == ITEM_ROD)
    {
        if (ItemSystem::GetItemRefineVnum(rod) > 0 &&
            ItemSystem::GetItemSocket(rod, 0) < ItemSystem::GetItemValue(rod, 2) &&
            number(1, ItemSystem::GetItemValue(rod, 1)) == 1)
        {
            ItemSystem::SetItemSocket(rod, 0, ItemSystem::GetItemSocket(rod, 0) + 1);
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(fisher, CHAT_TYPE_INFO, 283, "%d#%d",
                ItemSystem::GetItemSocket(rod, 0), ItemSystem::GetItemValue(rod, 2));
#endif

            if (ItemSystem::GetItemSocket(rod, 0) == ItemSystem::GetItemValue(rod, 2))
            {
#ifdef TEXTS_IMPROVEMENT
                ecs::ChatSystem::SendNew(fisher, CHAT_TYPE_INFO, 279, "");
                ecs::ChatSystem::SendNew(fisher, CHAT_TYPE_INFO, 280, "");
#endif
            }
        }

        ItemSystem::SetItemSocket(rod, 2, 0);
    }

    uint8_t chance = 0;
    switch (itemVnum) {
        case 27803:
        case 27806:
        case 27816:
        case 27807:
        case 27818:
        case 27805:
        case 27822:
        case 27823:
        case 27824:
        case 27825:
        case 71136:
        case 39065:
        case 2870:
        case 2871:
        case 2873:
        case 99998:
        case 39066:
        case 39068:
        case 80003:
        case 80004:
        case 80005:
        case 80006:
        case 80007:
        case 89106:
        case 71175:
        case 30625:
            chance = 50;
            break;

        case 2872:
        case 2874:
        case 30179:
        case 76019:
        case 30713:
        case 30798:
        case 71095:
        case 30320:
        case 76025:
        case 30613:
        case 71174:
        case 30325:
        case 89101:
            chance = 15;
            break;

        case 2875:
        case 2876:
        case 70606:
        case 70605:
        case 30617:
        case 30618:
        case 86052:
        case 86051:
        case 71123:
        case 71129:
        case 27804:
        case 27811:
        case 27810:
        case 27809:
        case 27814:
        case 27812:
        case 27808:
        case 27826:
        case 27827:
        case 27813:
        case 27815:
        case 27819:
        case 27820:
        case 27821:
        case 2877:
        case 2878:
        case 60011:
        case 60031:
        case 60041:
        case 2858:
        case 53251:
        case 18090:
        case 55706:
        case 60010:
        case 60020:
        case 60030:
        case 60040:
        case 60050:
        case 60060:
        case 60070:
        case 60080:
        case 72726:
        case 72730:
        case 50525:
            chance = 5;
            break;

        case 611516:
            chance = 1;
            break;

        default:
            chance = 0;
            break;
    }

    if (ecs::PointSystem::Get(fisher, POINT_FISHING_RARE) > 0 && chance == 5)
        chance += 20;

    const uint32_t rodVnum = ItemSystem::GetItemVnum(rod);
    if (rodVnum >= 27400 && rodVnum <= 27490)
        chance += (ItemSystem::GetItemValue(rod, 0) / 10) * 2;
    else
        chance += ItemSystem::GetItemValue(rod, 0) / 10;

    TPacketFishingNew p;
    p.header = HEADER_GC_FISHING_NEW;
    if (number(1, 100) >= chance) {
        p.subheader = FISHING_SUBHEADER_NEW_CATCH_FAIL;
    } else {
#ifdef ENABLE_RANKING
        ecs::PlayerRuntime::SetRankPoints(
            fisher, 14, ecs::PlayerRuntime::GetRankPoints(fisher, 14) + 1);
#endif
#ifdef ENABLE_BATTLE_PASS
        UpdateFishingBattlePassLegacyBoundary(fisher);
#endif

        p.subheader = FISHING_SUBHEADER_NEW_CATCH_SUCCESS;

        const entt::entity reward = ItemSystem::AutoGiveItemEcs(fisher, itemVnum, 1, -1, false);
        if (reward != entt::null)
        {
#ifdef ENABLE_MULTI_NAMES
            uint8_t lang = 0;
            if (LPDESC d = ecs::PlayerRuntime::GetDesc(fisher))
                lang = d->GetLanguage();

            TItemTable* pTable = ITEM_MANAGER::instance().GetTable(itemVnum);
            const char* szName = (pTable ? pTable->szLocaleName[lang] : "UNKNOWN_ITEM");
#else
            TItemTable* pTable = ITEM_MANAGER::instance().GetTable(itemVnum);
            const char* szName = (pTable ? pTable->szLocaleName : "UNKNOWN_ITEM");
#endif
            const uint32_t rewardVnum = ItemSystem::GetItemVnum(reward);

            if (rewardVnum == 611516)
            {
                char buf[256];
                snprintf(buf, sizeof(buf),
                    "%s kifogta a Legendas vizi szornyet: %s",
                    ecs::PlayerRuntime::GetName(fisher).data(), szName);
                BroadcastNotice(buf);
            }

            ecs::ChatSystem::Send(fisher, CHAT_TYPE_INFO, "%s kaptal.", szName);
        }
        else
        {
            ecs::ChatSystem::Send(fisher, CHAT_TYPE_INFO, "nincs hely az inventoryban.");
        }
    }

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

void CHARACTER::fishing_new_start()
{
    ActivitySystem::StartFishing(GetEntityHandle(), get_dword_time());
}

void CHARACTER::fishing_new_stop()
{
    ActivitySystem::StopFishing(GetEntityHandle());
}

void CHARACTER::fishing_new_catch()
{
    ActivitySystem::CatchFishing(GetEntityHandle(), get_dword_time());
}

void CHARACTER::fishing_new_catch_failed()
{
    ActivitySystem::CatchFishingFailed(GetEntityHandle());
}

void CHARACTER::mining(entt::entity load)
{
    ActivitySystem::StartMining(
        GetEntityHandle(),
        load);
}

void CHARACTER::fishing()
{
    const entt::entity character = GetEntityHandle();

    if (ecs::PlayerRuntime::GetCharEvent(character, ecs::PlayerRuntime::CharEvent::Fishing))
    {
        fishing_take();
        return;
    }

    {
        int x = GetX();
        int y = GetY();

        LPSECTREE tree = ecs::SectorAt(GetMapIndex(), x, y);
        uint32_t dwAttr = tree->GetAttribute(x, y);

        if (IS_SET(dwAttr, ATTR_BLOCK))
        {
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 657, "");
#endif
            return;
        }
    }

    const entt::entity rod = ItemSystem::GetWearItem(character, WEAR_WEAPON);

    if (!ItemSystem::IsValidItem(rod) || ItemSystem::GetItemType(rod) != ITEM_ROD)
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 281, "");
#endif
        return;
    }

    if (0 == ItemSystem::GetItemSocket(rod, 2))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 351, "");
#endif
        return;
    }

    float fx, fy;
    GetDeltaByDegree(GetRotation(), 400.0f, &fx, &fy);

    ecs::PlayerRuntime::SetCharEvent(character, ecs::PlayerRuntime::CharEvent::Fishing, fishing::CreateFishingEvent(character));
}

void CHARACTER::fishing_take()
{
    const entt::entity character = GetEntityHandle();
    const entt::entity rod = ItemSystem::GetWearItem(character, WEAR_WEAPON);
    if (ItemSystem::IsValidItem(rod) && ItemSystem::GetItemType(rod) == ITEM_ROD)
    {
        using fishing::fishing_event_info;
        if (const LPEVENT fishingEvent = ecs::PlayerRuntime::GetCharEvent(character, ecs::PlayerRuntime::CharEvent::Fishing))
        {
            struct fishing_event_info* info = dynamic_cast<struct fishing_event_info*>(fishingEvent->info);

            if (info)
                fishing::Take(info, character);
        }
    }
}

#endif


