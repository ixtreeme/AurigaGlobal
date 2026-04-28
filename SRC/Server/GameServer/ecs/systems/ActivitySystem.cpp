#include "../../stdafx.h"

#include "ActivitySystem.hpp"
#include "ItemSystem.hpp"

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
#include "../AIHelpers.hpp"
#include "../SpatialHelpers.hpp"
#include "../Registry.hpp"
#include "../VIDRegistry.hpp"
#include "../components/activity_components.hpp"
#include "../components/dirty_components.hpp"
#include "../components/identity_components.hpp"

namespace
{

LPCHARACTER LegacyCharacter(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    auto* vid = g_registry.try_get<ecs::VIDComponent>(e);
    if (!vid)
        return nullptr;

    return CHARACTER_MANAGER::instance().Find(vid->value);
}

ecs::FishingState* GetFishingState(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    return &g_registry.get_or_emplace<ecs::FishingState>(e);
}

void SyncLegacyFishing(LPCHARACTER ch, const ecs::FishingState& state)
{
    if (!ch)
        return;

    ch->m_pkFishingNewEvent = state.fishingNewEvent;
    ch->SetFishCatch(state.catchCount);
    ch->SetFishCatchFailed(state.catchFailed);
    ch->SetLastCatchTime(state.lastCatchTime);
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

EVENTFUNC(ecs_fishing_event)
{
    fishingnew_event_info* info = dynamic_cast<fishingnew_event_info*>(event->info);
    if (info == nullptr)
        return 0;

    LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(info->pid);
    if (!ch)
        return 0;

    const entt::entity fisher = AIHelpers::EcsOf(ch);
    if (fisher == entt::null || !g_registry.valid(fisher))
        return 0;

    auto* state = GetFishingState(fisher);
    if (!state)
        return 0;

    if (state->catchCount >= FISHING_NEED_CATCH) {
        ActivitySystem::CatchDecision(fisher, info->vnum);
        return 0;
    }

    LPITEM rod = ch->GetWear(WEAR_WEAPON);
    if (!(rod && rod->GetType() == ITEM_ROD)) {
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
            if (LPDESC d = ch->GetDesc())
                lang = d->GetLanguage();
            ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 896, "%s", pTable->szLocaleName[lang]);
#else
            ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 896, "%s", pTable->szLocaleName);
#endif
#endif
        }
        else
        {
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 897, "");
#endif
        }
    }

    if (state->catchFailed > 0) {
        info->sec += state->catchFailed;
        state->catchFailed = 0;
        SyncLegacyFishing(ch, *state);
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
    LPCHARACTER ch = LegacyCharacter(fisher);
    auto* state = GetFishingState(fisher);
    if (!ch || !state || state->fishingNewEvent)
        return;

    const int x = ch->GetX();
    const int y = ch->GetY();
    LPSECTREE tree = ecs::SectorAt(ch->GetMapIndex(), x, y);
    if (!tree)
        return;

    if (tree->IsAttr(x, y, ATTR_BLOCK)) {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 894, "");
#endif
        return;
    }

    if (ch->GetEmptyInventory(1) == -1) {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 899, "");
#endif
        return;
    }

    LPITEM rod = ch->GetWear(WEAR_WEAPON);
    if (!rod || rod->GetType() != ITEM_ROD) {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 895, "");
#endif
        return;
    }

    if (rod->GetSocket(2) == 0) {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 281, "");
#endif
        return;
    }

    float fx = 0.0f;
    float fy = 0.0f;
    GetDeltaByDegree(ch->GetRotation(), 400.0f, &fx, &fy);

    const uint32_t rodVnum = rod->GetVnum();
    const bool second = !(rodVnum >= 27400 && rodVnum <= 27490);

    fishingnew_event_info* info = AllocEventInfo<fishingnew_event_info>();
    info->pid = ch->GetPlayerID();
    info->vnum = fishingnew::GetFishCatchedVnum(
        100,
        15 + ch->GetPoint(POINT_FISHING_RARE) + rod->GetSocket(2),
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

    SyncLegacyFishing(ch, *state);
    MarkFishing(fisher, true);

    TPacketFishingNew p;
    p.header = HEADER_GC_FISHING_NEW;
    p.subheader = FISHING_SUBHEADER_NEW_START;
    p.vid = ch->GetPacketVID();
    p.dir = static_cast<uint8_t>(ch->GetRotation() / 5);
    p.need = FISHING_NEED_CATCH;
    p.count = 0;
    ch->PacketAround(&p, sizeof(p));
}

void StopFishing(entt::entity fisher)
{
    LPCHARACTER ch = LegacyCharacter(fisher);
    auto* state = GetFishingState(fisher);
    if (!ch || !state || !state->fishingNewEvent)
        return;

    event_cancel(&state->fishingNewEvent);
    state->fishingNewEvent = nullptr;
    state->elapsedSeconds = 0;
    state->fishVnum = 0;
    state->chance = 0;

    LPITEM rod = ch->GetWear(WEAR_WEAPON);
    if (rod && rod->GetType() == ITEM_ROD)
        rod->SetSocket(2, 0);

    SyncLegacyFishing(ch, *state);
    MarkFishing(fisher, false);

    TPacketFishingNew p;
    p.header = HEADER_GC_FISHING_NEW;
    p.subheader = FISHING_SUBHEADER_NEW_STOP;
    p.vid = ch->GetPacketVID();
    p.dir = 0;
    p.need = 0;
    p.count = 0;
    ch->PacketAround(&p, sizeof(p));
}

void CatchFishing(entt::entity fisher, uint32_t)
{
    LPCHARACTER ch = LegacyCharacter(fisher);
    auto* state = GetFishingState(fisher);
    if (!ch || !state || !state->fishingNewEvent)
        return;

    if (state->lastCatchTime > get_global_time())
        return;

    state->catchCount = static_cast<uint8_t>(state->catchCount + 1);
    state->lastCatchTime = get_global_time() + 1;
    SyncLegacyFishing(ch, *state);
    g_registry.emplace_or_replace<ecs::DirtyTag>(fisher);

    TPacketFishingNew p;
    p.header = HEADER_GC_FISHING_NEW;
    p.subheader = FISHING_SUBHEADER_NEW_CATCH;
    p.vid = ch->GetPacketVID();
    p.dir = 0;
    p.need = 0;
    p.count = state->catchCount;
    ch->PacketAround(&p, sizeof(p));
}

void CatchFishingFailed(entt::entity fisher)
{
    LPCHARACTER ch = LegacyCharacter(fisher);
    auto* state = GetFishingState(fisher);
    if (!ch || !state || !state->fishingNewEvent)
        return;

    ++state->catchFailed;
    SyncLegacyFishing(ch, *state);
    g_registry.emplace_or_replace<ecs::DirtyTag>(fisher);

    TPacketFishingNew p;
    p.header = HEADER_GC_FISHING_NEW;
    p.subheader = FISHING_SUBHEADER_NEW_CATCH_FAILED;
    p.vid = ch->GetPacketVID();
    p.dir = 0;
    p.need = 0;
    p.count = 0;
    ch->PacketAround(&p, sizeof(p));
}

void CatchDecision(entt::entity fisher, uint32_t itemVnum)
{
    LPCHARACTER ch = LegacyCharacter(fisher);
    auto* state = GetFishingState(fisher);
    if (!ch || !state || !state->fishingNewEvent)
        return;

    event_cancel(&state->fishingNewEvent);
    state->fishingNewEvent = nullptr;
    state->elapsedSeconds = 0;
    state->fishVnum = 0;
    state->chance = 0;
    MarkFishing(fisher, false);

    LPITEM rod = ch->GetWear(WEAR_WEAPON);
    if (!rod)
        return;

    if (rod->GetType() == ITEM_ROD)
    {
        if (rod->GetRefinedVnum() > 0 && rod->GetSocket(0) < rod->GetValue(2) && number(1, rod->GetValue(1)) == 1)
        {
            rod->SetSocket(0, rod->GetSocket(0) + 1);
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 283, "%d#%d", rod->GetSocket(0), rod->GetValue(2));
#endif
            if (rod->GetSocket(0) == rod->GetValue(2))
            {
#ifdef TEXTS_IMPROVEMENT
                ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 279, "");
                ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 280, "");
#endif
            }
        }

        rod->SetSocket(2, 0);
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

    if (ch->GetPoint(POINT_FISHING_RARE) > 0 && chance == 5)
        chance += 20;

    const uint32_t rodVnum = rod->GetVnum();
    if (rodVnum >= 27400 && rodVnum <= 27490)
        chance += (rod->GetValue(0) / 10) * 2;
    else
        chance += rod->GetValue(0) / 10;

    SyncLegacyFishing(ch, *state);

    TPacketFishingNew p;
    p.header = HEADER_GC_FISHING_NEW;
    if (number(1, 100) >= chance) {
        p.subheader = FISHING_SUBHEADER_NEW_CATCH_FAIL;
    } else {
#ifdef ENABLE_RANKING
        ch->SetRankPoints(14, ch->GetRankPoints(14) + 1);
#endif
#ifdef ENABLE_BATTLE_PASS
        uint8_t bBattlePassId = ch->GetBattlePassId();
        if (bBattlePassId) {
            uint32_t dwCount, dwNotUsed;
            if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, CATCH_FISH, &dwNotUsed, &dwCount)) {
                if (ch->GetMissionProgress(CATCH_FISH, bBattlePassId) < dwCount)
                    ch->UpdateMissionProgress(CATCH_FISH, bBattlePassId, 1, dwCount);
            }
        }
#endif

        p.subheader = FISHING_SUBHEADER_NEW_CATCH_SUCCESS;

        const entt::entity reward = ItemSystem::AutoGiveItemEcs(AIHelpers::EcsOf(ch), itemVnum, 1, -1, false);
        if (reward != entt::null)
        {
#ifdef ENABLE_MULTI_NAMES
            uint8_t lang = 0;
            if (LPDESC d = ch->GetDesc())
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
                    ch->GetName(), szName);
                BroadcastNotice(buf);
            }

            ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "%s kaptal.", szName);
        }
        else
        {
            ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "nincs hely az inventoryban.");
        }
    }

    p.vid = ch->GetPacketVID();
    p.dir = 0;
    p.need = 0;
    p.count = 0;
    ch->PacketAround(&p, sizeof(p));
}

void UpdateFishing(entt::registry& reg, uint32_t)
{
    // During the migration window, only process entities with an active fishing state.
    auto view = reg.view<ecs::FishingState, ecs::FishingActiveTag, ecs::VIDComponent>();
    view.each([](entt::entity e, ecs::FishingState& state, const ecs::VIDComponent& vid) {
        if (!state.fishingNewEvent) {
            return;
        }

        LPCHARACTER ch = CHARACTER_MANAGER::instance().Find(vid.value);
        if (!ch)
            return;

        state.fishingNewEvent = ch->m_pkFishingNewEvent;
        state.catchCount = ch->GetFishCatch();
        state.catchFailed = ch->GetFishCatchFailed();
        state.lastCatchTime = ch->GetLastCatchTime();

        if (state.fishingNewEvent)
            g_registry.emplace_or_replace<ecs::FishingActiveTag>(e);
        else if (g_registry.all_of<ecs::FishingActiveTag>(e))
            g_registry.remove<ecs::FishingActiveTag>(e);
    });
}

} // namespace ActivitySystem

void CHARACTER::fishing_new_start()
{
    ActivitySystem::StartFishing(AIHelpers::EcsOf(this), get_dword_time());
}

void CHARACTER::fishing_new_stop()
{
    ActivitySystem::StopFishing(AIHelpers::EcsOf(this));
}

void CHARACTER::fishing_new_catch()
{
    ActivitySystem::CatchFishing(AIHelpers::EcsOf(this), get_dword_time());
}

void CHARACTER::fishing_new_catch_failed()
{
    ActivitySystem::CatchFishingFailed(AIHelpers::EcsOf(this));
}

void CHARACTER::fishing_catch_decision(uint32_t itemVnum)
{
    ActivitySystem::CatchDecision(AIHelpers::EcsOf(this), itemVnum);
}

void CHARACTER::mining_take()
{
    m_pkMiningEvent = nullptr;
}

void CHARACTER::mining_cancel()
{
    if (m_pkMiningEvent)
    {
        sys_log(0, "XXX MINING CANCEL");
        event_cancel(&m_pkMiningEvent);
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 472, "");
#endif
    }
}

void CHARACTER::mining(LPCHARACTER chLoad)
{
    if (m_pkMiningEvent)
    {
        mining_cancel();
        return;
    }

    if (!chLoad)
        return;

    if (GetMapIndex() != chLoad->GetMapIndex() || DISTANCE_APPROX(GetX() - chLoad->GetX(), GetY() - chLoad->GetY()) > 1000)
        return;

    if (mining::GetRawOreFromLoad(chLoad->GetRaceNum()) == 0)
        return;

    LPITEM pick = GetWear(WEAR_WEAPON);

    if (!pick || pick->GetType() != ITEM_PICK)
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 252, "");
#endif
        return;
    }

    int count = number(5, 15);

    TPacketGCDigMotion p;
    p.header = HEADER_GC_DIG_MOTION;
    p.vid = GetPacketVID();
    p.target_vid = chLoad->GetPacketVID();
    p.count = count;

    PacketAround(&p, sizeof(p));

    m_pkMiningEvent = mining::CreateMiningEvent(this, chLoad, count);
}

void CHARACTER::fishing()
{
    if (m_pkFishingEvent)
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
            ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 657, "");
#endif
            return;
        }
    }

    LPITEM rod = GetWear(WEAR_WEAPON);

    if (!rod || rod->GetType() != ITEM_ROD)
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 281, "");
#endif
        return;
    }

    if (0 == rod->GetSocket(2))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 351, "");
#endif
        return;
    }

    float fx, fy;
    GetDeltaByDegree(GetRotation(), 400.0f, &fx, &fy);

    m_pkFishingEvent = fishing::CreateFishingEvent(this);
}

void CHARACTER::fishing_take()
{
    LPITEM rod = GetWear(WEAR_WEAPON);
    if (rod && rod->GetType() == ITEM_ROD)
    {
        using fishing::fishing_event_info;
        if (m_pkFishingEvent)
        {
            struct fishing_event_info* info = dynamic_cast<struct fishing_event_info*>(m_pkFishingEvent->info);

            if (info)
                fishing::Take(info, this);
        }
    }
}

#endif


