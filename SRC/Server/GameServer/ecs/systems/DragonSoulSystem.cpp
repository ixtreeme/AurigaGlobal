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
#include "../EntityFactory.hpp"
#include "../Registry.hpp"
#include "../components/dirty_components.hpp"
#include "../components/identity_components.hpp"
#include "../components/inventory_components.hpp"
#include "../components/session_components.hpp"
#include "ItemSystem.hpp"
#include "NetworkSyncSystem.hpp"
#include <Core/Logging.hpp>

namespace {
using State = ecs::DragonSoulRuntimeStateComponent;

State* StateOf(entt::entity owner)
{
    return g_registry.valid(owner) ? g_registry.try_get<State>(owner) : nullptr;
}

State* EnsureState(entt::entity owner)
{
    if (!g_registry.valid(owner)) return nullptr;
    if (!g_registry.all_of<State>(owner))
        g_registry.insert<State>(&owner, &owner + 1);
    // insert does not return a component reference after on_construct callbacks.
    return StateOf(owner);
}

void MarkDirty(entt::entity owner)
{
    if (g_registry.valid(owner) && !g_registry.all_of<ecs::DirtyTag>(owner))
        g_registry.insert<ecs::DirtyTag>(&owner, &owner + 1);
}

struct StopRequest { bool stop = false, clearSelection = false; };
std::map<entt::entity, StopRequest> deckOperations;

struct DeckOperation {
    entt::entity owner;
    bool entered, stateLost = false;
    entt::scoped_connection lifetime;
    explicit DeckOperation(entt::entity e) : owner(e), entered(deckOperations.try_emplace(e).second)
    {
        if (entered) lifetime = g_registry.on_destroy<State>().connect<&DeckOperation::StateDestroyed>(*this);
    }
    ~DeckOperation() { if (entered) deckOperations.erase(owner); }
    DeckOperation(const DeckOperation&) = delete;
    DeckOperation& operator=(const DeckOperation&) = delete;
    void StateDestroyed(entt::registry&, entt::entity e) { if (e == owner) stateLost = true; }
    bool Live() const { return entered && !stateLost && StateOf(owner); }
    const StopRequest& Request() const { return deckOperations.at(owner); }
    bool At(int deck) const { const auto* state = Live() ? StateOf(owner) : nullptr; return state && state->activeDeck == deck; }
    bool Proceed(int deck) const { return At(deck) && !Request().stop; }
};

bool RequestStop(entt::entity owner, bool clearSelection)
{
    const auto operation = deckOperations.find(owner);
    if (operation == deckOperations.end()) return false;
    operation->second.stop = true;
    operation->second.clearSelection |= clearSelection;
    return true;
}

bool BoundSoul(entt::entity owner, entt::entity item, int cell)
{
    return g_registry.valid(owner) && ItemSystem::IsDragonSoulItem(item) &&
        ItemSystem::GetItemOwner(item) == owner && ItemSystem::IsItemEquipped(item) &&
        ItemSystem::GetItemCell(item) == cell && ItemSystem::GetInventoryItem(owner, cell) == item;
}

using Souls = std::array<entt::entity, DS_SLOT_MAX * DRAGON_SOUL_DECK_MAX_NUM>;
Souls SnapshotSouls(entt::entity owner)
{
    Souls result; result.fill(entt::null);
    for (size_t i = 0; i < result.size() && g_registry.valid(owner); ++i)
    {
        const int cell = DRAGON_SOUL_EQUIP_SLOT_START + static_cast<int>(i);
        const auto item = ItemSystem::GetInventoryItem(owner, cell);
        if (BoundSoul(owner, item, cell)) result[i] = item;
    }
    return result;
}

bool StopDeck(DeckOperation& operation, bool clearSelection)
{
    if (!operation.Live()) return false;
    const auto owner = operation.owner;
    const auto souls = SnapshotSouls(owner);
    int selection = StateOf(owner)->activeDeck;
    if (clearSelection) StateOf(owner)->activeDeck = selection = -1;
    for (size_t i = 0; i < souls.size(); ++i)
    {
        if (!operation.At(selection)) return false;
        if (BoundSoul(owner, souls[i], DRAGON_SOUL_EQUIP_SLOT_START + static_cast<int>(i)))
            DSManager::instance().DeactivateDragonSoul(souls[i], true);
    }
    if (!operation.At(selection)) return false;
    // A nested explicit shutdown takes precedence over logout's keep-selection request.
    clearSelection |= operation.Request().clearSelection;
    if (clearSelection)
    {
        StateOf(owner)->activeDeck = selection = -1;
        constexpr uint32_t affects[] = {AFFECT_DRAGON_SOUL_DECK_0, AFFECT_DRAGON_SOUL_DECK_1,
            AFFECT_DS_SET, AFFECT_DS_BNS1, AFFECT_DS_BNS2, AFFECT_DS_BNS3};
        for (const auto affect : affects)
        {
            AffectSystem::RemoveAffect(owner, affect);
            if (!operation.At(selection)) return false;
        }
    }
    MarkDirty(owner);
    if (operation.At(selection) && !clearSelection && operation.Request().clearSelection)
        return StopDeck(operation, true);
    return operation.At(selection);
}

bool CancelDeck(DeckOperation& operation)
{
    // Do not touch a replacement runtime component, even on the same entity.
    if (operation.Live())
        StopDeck(operation, !operation.Request().stop || operation.Request().clearSelection);
    return false;
}

bool StartDeck(DeckOperation& operation, int deck)
{
    const auto owner = operation.owner;
    if (!operation.Live() || deck < 0 || deck >= DRAGON_SOUL_DECK_MAX_NUM) return false;
    if (operation.At(deck)) return operation.Proceed(deck) ? true : CancelDeck(operation);
    if (!StopDeck(operation, true) || !operation.Proceed(-1)) return CancelDeck(operation);
    StateOf(owner)->activeDeck = deck;
    if (!AffectSystem::AddAffect(owner, AFFECT_DRAGON_SOUL_DECK_0 + deck,
        APPLY_NONE, 0, 0, INFINITE_AFFECT_DURATION, 0, false) || !operation.Proceed(deck))
        return CancelDeck(operation);

    const auto souls = SnapshotSouls(owner);
    for (int slot = 0; slot < DS_SLOT_MAX; ++slot)
    {
        const int index = DS_SLOT_MAX * deck + slot;
        const auto item = souls[index];
        if (BoundSoul(owner, item, DRAGON_SOUL_EQUIP_SLOT_START + index))
            DSManager::instance().ActivateDragonSoul(item);
        if (!operation.Proceed(deck)) return CancelDeck(operation);
    }

#ifdef ENABLE_DS_SET
    const auto completeSet = [&] {
        if (!operation.Proceed(deck)) return false;
        for (int slot = 0; slot < DS_SLOT_MAX; ++slot)
        {
            const int index = DS_SLOT_MAX * deck + slot;
            const auto item = souls[index];
            if (!BoundSoul(owner, item, DRAGON_SOUL_EQUIP_SLOT_START + index) ||
                !DSManager::instance().IsActiveDragonSoul(item) || !DSManager::instance().IsTimeLeftDragonSoul(item))
                return false;
            const auto vnum = ItemSystem::GetItemVnum(item);
            if ((vnum / 1000) % 10 !=
#ifdef ENABLE_DS_GRADE_MYTH
                DRAGON_SOUL_GRADE_MYTH
#else
                DRAGON_SOUL_GRADE_LEGENDARY
#endif
                || (vnum / 100) % 10 != DRAGON_SOUL_STEP_HIGHEST || (vnum / 10) % 10 != 6)
                return false;
        }
        return true;
    };
    const bool complete = completeSet();
    struct SetBonus { uint32_t type; uint8_t point; int value; };
    constexpr SetBonus bonuses[] = {{AFFECT_DS_BNS1, POINT_ATTBONUS_METIN, 10},
        {AFFECT_DS_BNS2, POINT_ATTBONUS_MONSTER, 10}, {AFFECT_DS_BNS3, POINT_MAX_HP, 1000}};
    if (!AffectSystem::AddAffect(owner, AFFECT_DS_SET, POINT_NONE, complete ? 1 : 0,
        0, INFINITE_AFFECT_DURATION, 0, false) || !operation.Proceed(deck) || (complete && !completeSet()))
        return CancelDeck(operation);
    for (const auto& bonus : bonuses)
    {
        if (complete)
        {
            if (!AffectSystem::AddAffect(owner, bonus.type, bonus.point, bonus.value,
                0, INFINITE_AFFECT_DURATION, 0, false)) return CancelDeck(operation);
        }
        else AffectSystem::RemoveAffect(owner, bonus.type);
        if (!operation.Proceed(deck) || (complete && !completeSet())) return CancelDeck(operation);
    }
#endif
    MarkDirty(owner);
    return operation.Proceed(deck) ? true : CancelDeck(operation);
}
} // namespace

namespace DragonSoulSystem {

void Initialize(entt::entity owner)
{
    if (!g_registry.valid(owner)) return;
    DeckOperation operation(owner);
    if (!operation.entered || !EnsureState(owner) || !operation.Live()) return;
    const int savedDeck = AffectSystem::FindAffect(owner, AFFECT_DRAGON_SOUL_DECK_0) ? 0 :
        AffectSystem::FindAffect(owner, AFFECT_DRAGON_SOUL_DECK_1) ? 1 : -1;
    // A repeated hydration must first remove already-applied runtime bonuses.
    if (GetActiveDeck(owner) >= 0 && (!StopDeck(operation, true) || !operation.Proceed(-1)))
    { CancelDeck(operation); return; }
    const auto souls = SnapshotSouls(owner);
    // Login hydration resets persisted flags, not bonuses which have not yet been applied.
    for (const auto item : souls)
        if (ItemSystem::IsValidItem(item))
            if (auto* sockets = g_registry.try_get<ecs::ItemSockets>(item))
                sockets->sockets[ITEM_SOCKET_DRAGON_SOUL_ACTIVE_IDX] = 0;
    StateOf(owner)->activeDeck = -1;
    for (size_t i = 0; i < souls.size(); ++i)
    {
        if (!operation.Proceed(-1)) { CancelDeck(operation); return; }
        const auto item = souls[i];
        const int cell = DRAGON_SOUL_EQUIP_SLOT_START + static_cast<int>(i);
        if (BoundSoul(owner, item, cell))
        {
            ItemSystem::SaveItem(item);
            if (operation.Proceed(-1) && BoundSoul(owner, item, cell))
                ecs::ItemNetworkSystem::SendItemUpdate(g_registry, item);
        }
    }
    if (!operation.Proceed(-1)) { CancelDeck(operation); return; }
    if (savedDeck >= 0) StartDeck(operation, savedDeck);
    else MarkDirty(owner);
}

int GetActiveDeck(entt::entity owner)
{
    const auto* state = StateOf(owner);
    return state && state->activeDeck >= 0 && state->activeDeck < DRAGON_SOUL_DECK_MAX_NUM ? state->activeDeck : -1;
}

bool IsDeckActivated(entt::entity owner) { return GetActiveDeck(owner) >= 0; }

bool ActivateDeck(entt::entity owner, int deck)
{
    if (!g_registry.valid(owner) || deck < 0 || deck >= DRAGON_SOUL_DECK_MAX_NUM) return false;
    DeckOperation operation(owner);
    if (!operation.entered || !EnsureState(owner) || !operation.Live()) return false;
    return StartDeck(operation, deck);
}

void DeactivateAll(entt::entity owner)
{
    if (!g_registry.valid(owner) || RequestStop(owner, true)) return;
    DeckOperation operation(owner);
    if (operation.entered && EnsureState(owner)) StopDeck(operation, true);
}

void CleanUp(entt::entity owner)
{
    if (!g_registry.valid(owner) || RequestStop(owner, false)) return;
    DeckOperation operation(owner);
    if (operation.entered && EnsureState(owner)) StopDeck(operation, false);
}

bool OpenRefineWindow(entt::entity owner, entt::entity opener)
{
    if (!g_registry.valid(owner) || !g_registry.valid(opener))
        return false;
    LPDESC d = ecs::PlayerRuntime::GetDesc(owner);
    if (!d)
        return false;

    // A failed request must not leave an invisible but usable refine window.
    auto* state = EnsureState(owner);
    if (!state || !g_registry.valid(opener)) return false;
    if (!g_registry.valid(state->refineWindowOpener))
        state->refineWindowOpener = opener;

    TPacketGCDragonSoulRefine pack {};
    pack.header = HEADER_GC_DRAGON_SOUL_REFINE;
    pack.bSubType = DS_SUB_HEADER_OPEN;
    d->Packet(&pack, sizeof(pack));
    MarkDirty(owner);
    return CanRefine(owner);
}

bool CloseRefineWindow(entt::entity owner)
{
    if (!g_registry.valid(owner))
        return false;
    if (auto* state = g_registry.try_get<ecs::DragonSoulRuntimeStateComponent>(owner))
        state->refineWindowOpener = entt::null;
    MarkDirty(owner);
    return true;
}

bool CanRefine(entt::entity owner)
{
    return GetRefineWindowOpener(owner) != entt::null;
}

entt::entity GetRefineWindowOpener(entt::entity owner)
{
    if (!g_registry.valid(owner))
        return entt::null;
    const auto* state = g_registry.try_get<ecs::DragonSoulRuntimeStateComponent>(owner);
    return state && g_registry.valid(state->refineWindowOpener)
        ? state->refineWindowOpener : entt::null;
}

} // namespace DragonSoulSystem

bool CHARACTER::DragonSoul_RefineWindow_Close()
{
    return DragonSoulSystem::CloseRefineWindow(GetEntityHandle());
}

namespace DragonSoulSystem {
int32_t GetLastRefineTime(entt::entity owner)
{
    if (owner == entt::null || !g_registry.valid(owner))
        return 0;
    const auto* state = g_registry.try_get<ecs::DragonSoulState>(owner);
    return state ? state->lastRefineTime : 0;
}

void SetLastRefineTime(entt::entity owner)
{
    if (owner == entt::null || !g_registry.valid(owner))
        return;
    if (!g_registry.all_of<ecs::DragonSoulState>(owner))
        g_registry.insert<ecs::DragonSoulState>(&owner, &owner + 1);
    if (g_registry.valid(owner))
        if (auto* state = g_registry.try_get<ecs::DragonSoulState>(owner))
            state->lastRefineTime = get_global_time() + 3;
}
}
