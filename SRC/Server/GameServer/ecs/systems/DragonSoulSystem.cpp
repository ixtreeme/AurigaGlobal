#include "../../stdafx.h"
#include "PlayerRuntimeSystem.hpp"
#include "AffectSystem.hpp"

#include "DragonSoulSystem.hpp"

#include "../../char.h"
#include "../../char_manager.h"
#include "../../desc.h"
#include "../../DragonSoul.h"
#include "../../item.h"
#include "../../log.h"
#include "../../packet.h"
#include "../AIHelpers.hpp"
#include "../EntityFactory.hpp"
#include "../Registry.hpp"
#include "../components/dirty_components.hpp"
#include "../components/identity_components.hpp"
#include "../components/inventory_components.hpp"
#include "../components/session_components.hpp"
#include "ItemSystem.hpp"
#include <Core/Logging.hpp>

namespace
{

LPCHARACTER LegacyCharacter(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    auto* legacy = g_registry.try_get<ecs::LegacyCharPtr>(e);
    return legacy ? legacy->ptr : nullptr;
}

ecs::DragonSoulRuntimeStateComponent* GetDragonSoulState(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    return &g_registry.get_or_emplace<ecs::DragonSoulRuntimeStateComponent>(e);
}

void MarkDirty(entt::entity e)
{
    if (e != entt::null && g_registry.valid(e))
        g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

} // namespace

namespace DragonSoulSystem {

void Initialize(entt::entity owner)
{
    auto* ch = LegacyCharacter(owner);
    auto* state = GetDragonSoulState(owner);
    if (!ch || !state)
        return;

    for (int i = DRAGON_SOUL_EQUIP_SLOT_START; i < DRAGON_SOUL_EQUIP_SLOT_END; ++i)
    {
        LPITEM item = ch->GetItem(TItemPos(INVENTORY, i));
        if (item)
            ItemSystem::SetItemSocket(EntityFactory::CreateItemEntity(g_registry, item), ITEM_SOCKET_DRAGON_SOUL_ACTIVE_IDX, 0);
    }

    state->activeDeck = -1;
    if (AffectSystem::FindAffect(owner, AFFECT_DRAGON_SOUL_DECK_0))
        ActivateDeck(owner, DRAGON_SOUL_DECK_0);
    else if (AffectSystem::FindAffect(owner, AFFECT_DRAGON_SOUL_DECK_1))
        ActivateDeck(owner, DRAGON_SOUL_DECK_1);
    else
        MarkDirty(owner);
}

int GetActiveDeck(entt::entity owner)
{
    auto* state = GetDragonSoulState(owner);
    return state ? state->activeDeck : -1;
}

bool IsDeckActivated(entt::entity owner)
{
    return GetActiveDeck(owner) >= 0;
}

bool ActivateDeck(entt::entity owner, int deckIdx)
{
    auto* ch = LegacyCharacter(owner);
    auto* state = GetDragonSoulState(owner);
    if (!ch || !state)
        return false;

    if (deckIdx < DRAGON_SOUL_DECK_0 || deckIdx >= DRAGON_SOUL_DECK_MAX_NUM)
        return false;

    if (state->activeDeck == deckIdx)
        return true;

    DeactivateAll(owner);

    AffectSystem::AddAffect(owner, AFFECT_DRAGON_SOUL_DECK_0 + deckIdx, APPLY_NONE, 0, 0, INFINITE_AFFECT_DURATION, 0, false);
    state->activeDeck = deckIdx;

#ifdef ENABLE_DS_SET
    std::vector<int> gradeList;
    std::vector<int> stepList;
    std::vector<int> strengthList;
    bool expired = false;
    for (int i = 0; i < DS_SLOT_MAX; ++i)
    {
        gradeList.push_back(0);
        stepList.push_back(0);
        strengthList.push_back(0);
    }

    int j = 0;
#endif

    for (int i = DRAGON_SOUL_EQUIP_SLOT_START + DS_SLOT_MAX * deckIdx;
         i < DRAGON_SOUL_EQUIP_SLOT_START + DS_SLOT_MAX * (deckIdx + 1); ++i)
    {
        LPITEM item = ch->GetInventoryItem(i);
        if (!item)
            continue;

        DSManager::instance().ActivateDragonSoul(EntityFactory::CreateItemEntity(g_registry, item));
#ifdef ENABLE_DS_SET
        if (!DSManager::instance().IsTimeLeftDragonSoul(EntityFactory::CreateItemEntity(g_registry, item)))
            expired = true;

        gradeList[j] = (ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, item)) / 1000) % 10;
        stepList[j] = (ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, item)) / 100) % 10;
        strengthList[j] = (ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, item)) / 10) % 10;
        ++j;
#endif
    }

#ifdef ENABLE_DS_SET
    if ((!expired) && (gradeList[0] ==
#ifdef ENABLE_DS_GRADE_MYTH
        DRAGON_SOUL_GRADE_MYTH
#else
        DRAGON_SOUL_GRADE_LEGENDARY
#endif
        ) && (stepList[0] == DRAGON_SOUL_STEP_HIGHEST) && (strengthList[0] == 6))
    {
        if ((std::equal(gradeList.begin() + 1, gradeList.end(), gradeList.begin())) &&
            (std::equal(stepList.begin() + 1, stepList.end(), stepList.begin())) &&
            (std::equal(strengthList.begin() + 1, strengthList.end(), strengthList.begin())))
        {
            AffectSystem::AddAffect(owner, AFFECT_DS_SET, POINT_NONE, 1, 0, INFINITE_AFFECT_DURATION, 0, false);
            AffectSystem::AddAffect(owner, AFFECT_DS_BNS1, POINT_ATTBONUS_METIN, 10, 0, INFINITE_AFFECT_DURATION, 0, false);
            AffectSystem::AddAffect(owner, AFFECT_DS_BNS2, POINT_ATTBONUS_MONSTER, 10, 0, INFINITE_AFFECT_DURATION, 0, false);
            AffectSystem::AddAffect(owner, AFFECT_DS_BNS3, POINT_MAX_HP, 1000, 0, INFINITE_AFFECT_DURATION, 0, false);
        }
        else
        {
            AffectSystem::AddAffect(owner, AFFECT_DS_SET, POINT_NONE, 0, 0, INFINITE_AFFECT_DURATION, 0, false);
            AffectSystem::RemoveAffect(owner, AFFECT_DS_BNS1);
            AffectSystem::RemoveAffect(owner, AFFECT_DS_BNS2);
            AffectSystem::RemoveAffect(owner, AFFECT_DS_BNS3);
        }
    }
    else
    {
        AffectSystem::AddAffect(owner, AFFECT_DS_SET, POINT_NONE, 0, 0, INFINITE_AFFECT_DURATION, 0, false);
        AffectSystem::RemoveAffect(owner, AFFECT_DS_BNS1);
        AffectSystem::RemoveAffect(owner, AFFECT_DS_BNS2);
        AffectSystem::RemoveAffect(owner, AFFECT_DS_BNS3);
    }
#endif

    MarkDirty(owner);
    return true;
}

void DeactivateAll(entt::entity owner)
{
    auto* ch = LegacyCharacter(owner);
    auto* state = GetDragonSoulState(owner);
    if (!ch || !state)
        return;

    for (int i = DRAGON_SOUL_EQUIP_SLOT_START; i < DRAGON_SOUL_EQUIP_SLOT_END; ++i)
        DSManager::instance().DeactivateDragonSoul(EntityFactory::CreateItemEntity(g_registry, ch->GetInventoryItem(i)), true);

    state->activeDeck = -1;
    AffectSystem::RemoveAffect(owner, AFFECT_DRAGON_SOUL_DECK_0);
    AffectSystem::RemoveAffect(owner, AFFECT_DRAGON_SOUL_DECK_1);
    AffectSystem::RemoveAffect(owner, AFFECT_DS_SET);
    AffectSystem::RemoveAffect(owner, AFFECT_DS_BNS1);
    AffectSystem::RemoveAffect(owner, AFFECT_DS_BNS2);
    AffectSystem::RemoveAffect(owner, AFFECT_DS_BNS3);
    MarkDirty(owner);
}

void CleanUp(entt::entity owner)
{
    auto* ch = LegacyCharacter(owner);
    if (!ch)
        return;

    for (int i = DRAGON_SOUL_EQUIP_SLOT_START; i < DRAGON_SOUL_EQUIP_SLOT_END; ++i)
        DSManager::instance().DeactivateDragonSoul(EntityFactory::CreateItemEntity(g_registry, ch->GetInventoryItem(i)), true);

    MarkDirty(owner);
}

bool OpenRefineWindow(entt::entity owner, LPENTITY opener)
{
    auto* ch = LegacyCharacter(owner);
    auto* state = GetDragonSoulState(owner);
    if (!ch || !state)
        return false;

    if (!state->pRefineWindowOpener)
        state->pRefineWindowOpener = opener;

    TPacketGCDragonSoulRefine pack;
    pack.header = HEADER_GC_DRAGON_SOUL_REFINE;
    pack.bSubType = DS_SUB_HEADER_OPEN;

    LPDESC d = ecs::PlayerRuntime::GetDesc(owner);
    if (!d)
    {
        LOG_ERROR("User({})'s DESC is NULL POINT.", ecs::PlayerRuntime::GetName(owner).data());
        return false;
    }

    d->Packet(&pack, sizeof(pack));
    MarkDirty(owner);
    return true;
}

bool CloseRefineWindow(entt::entity owner)
{
    auto* state = GetDragonSoulState(owner);
    if (!state)
        return false;

    state->pRefineWindowOpener = nullptr;
    MarkDirty(owner);
    return true;
}

bool CanRefine(entt::entity owner)
{
    auto* state = GetDragonSoulState(owner);
    return state && state->pRefineWindowOpener != nullptr;
}

LPENTITY GetRefineWindowOpener(entt::entity owner)
{
    auto* state = GetDragonSoulState(owner);
    return state ? state->pRefineWindowOpener : nullptr;
}

} // namespace DragonSoulSystem

void CHARACTER::DragonSoul_Initialize()
{
    const entt::entity e = AIHelpers::EcsOf(this);
    DragonSoulSystem::Initialize(e);
}

int CHARACTER::DragonSoul_GetActiveDeck() const
{
    return DragonSoulSystem::GetActiveDeck(GetEntityHandle());
}

bool CHARACTER::DragonSoul_IsDeckActivated() const
{
    return DragonSoulSystem::IsDeckActivated(GetEntityHandle());
}

bool CHARACTER::DragonSoul_ActivateDeck(int deck_idx)
{
    const entt::entity e = AIHelpers::EcsOf(this);
    return DragonSoulSystem::ActivateDeck(e, deck_idx);
}

void CHARACTER::DragonSoul_DeactivateAll()
{
    DragonSoulSystem::DeactivateAll(AIHelpers::EcsOf(this));
}

void CHARACTER::DragonSoul_CleanUp()
{
    DragonSoulSystem::CleanUp(AIHelpers::EcsOf(this));
}

bool CHARACTER::DragonSoul_RefineWindow_Open(LPENTITY pEntity)
{
    return DragonSoulSystem::OpenRefineWindow(AIHelpers::EcsOf(this), pEntity);
}

bool CHARACTER::DragonSoul_RefineWindow_Close()
{
    return DragonSoulSystem::CloseRefineWindow(AIHelpers::EcsOf(this));
}

LPENTITY CHARACTER::DragonSoul_RefineWindow_GetOpener()
{
    return DragonSoulSystem::GetRefineWindowOpener(AIHelpers::EcsOf(this));
}

bool CHARACTER::DragonSoul_RefineWindow_CanRefine()
{
    return DragonSoulSystem::CanRefine(AIHelpers::EcsOf(this));
}
