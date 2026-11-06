#include "stdafx.h"
#include "TypedCompanions.hpp"
#include "ItemCatalogRuntime.hpp"
#include "ItemStore.hpp"
#include "TypedInventorySync.hpp"
#include "TypedDrops.hpp"
#include "TypedRecovery.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/ResourceAdmission.hpp"
#include "../ecs/systems/AffectSystem.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/InventorySystem.hpp"
#include "../ecs/systems/MountSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/SessionSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../entity/char_manager.h"
#include "../entity/mob_manager.h"
#include "../network/desc.h"
#include "../pet/New_PetSystem.h"
#include "../pet/PetSystem.h"
#include "../core/packet.h"
#include <algorithm>
#include <openssl/rand.h>

namespace
{
namespace item = auriga::item;
namespace store = item::runtime;
using Action = item::CompanionAction;

struct CompanionProjection
{
    std::uint64_t live = 0;
    std::uint32_t checked = 0;
};

bool InGame(entt::entity owner)
{
    const auto* desc = g_registry.valid(owner) ? ecs::PlayerRuntime::GetDesc(owner) : nullptr;
    return desc && desc->IsPhase(PHASE_GAME);
}

bool WorldAllowsFollower(entt::entity owner)
{
#ifdef ENABLE_PVP_ADVANCED
    if (ecs::PlayerRuntime::GetDuelOption(owner, "BlockPet"))
        return false;
#endif
    return AffectSystem::IsLoaded(owner) && !AffectSystem::IsPolymorphed(owner)
        && !CombatSystem::IsDead(owner) && !ecs::PlayerRuntime::IsObserverMode(owner)
        && ecs::PlayerRuntime::GetMapIndex(owner) != 113;
}

bool OtherFollower(entt::entity owner, std::uint64_t live)
{
    return PetSystem::CountSummoned(owner) > (live ? 1u : 0u)
        || MountSystem::IsCostumeMountSummoned(owner) || MountSystem::IsRiding(owner)
        || MountSystem::GetSummonedHorse(owner) != entt::null
#ifdef __NEWPET_SYSTEM__
        || NewPetSystem::CountSummoned(owner)
#endif
        ;
}

bool CanSummonFromBag(entt::entity owner)
{
    return InventorySystem::CanHandleItems(owner) && !CombatSystem::IsStun(owner)
        && !ecs::SocialSystem::HasExchange(owner) && !ecs::SessionSystem::IsSafeboxOpen(owner)
        && !ecs::SessionSystem::IsCubeOpen(owner) && ecs::SocialSystem::GetShopOwner(owner) == entt::null
        && ecs::SocialSystem::GetMyShop(owner) == entt::null;
}

const item::PersistentItem* Requested(const ecs::TypedItems& typed)
{
    const auto& items = typed.state.items;
    const auto found = std::ranges::find_if(items, [](const auto& i) {
        return !i.destroyed && i.placement && i.placement->kind == item::ItemStorageKind::Inventory
            && i.companion && i.companion->recallRequested;
    });
    return found != items.end() ? &*found : nullptr;
}

const item::ItemCompanionRow* CompanionRow(const item::ItemCatalog& catalog, item::ItemKey key)
{
    const auto* entry = catalog.Find(key);
    return entry && item::SupportsPersistentItem(catalog, *entry) ? std::get_if<item::ItemCompanionRow>(&entry->category) : nullptr;
}

std::uint32_t FollowerRace(const item::ItemCatalog& catalog, const item::ItemCompanionRow& row)
{
    const auto* form = catalog.FindCompanionForm(row.form);
    if (!form || form->mobTemplateId > UINT16_MAX || !CMobManager::instance().Get(form->mobTemplateId))
        return 0;
    return form->mobTemplateId;
}

void SendRefusal(entt::entity owner, const char* message)
{
    ecs::ChatSystem::Send(owner, CHAT_TYPE_INFO, "%s", message);
    SendTypedInventory(owner);
}
}

std::uint64_t RefreshTypedCompanion(entt::entity owner)
{
    if (!InGame(owner))
        return 0;
    const auto* typed = g_registry.try_get<ecs::TypedItems>(owner);
    if (!typed && !g_registry.all_of<CompanionProjection>(owner))
        return 0;
    const auto catalog = store::CurrentCatalog();
    const auto* desired = typed ? Requested(*typed) : nullptr;
    const auto* entry = desired && catalog ? catalog->Find(desired->key) : nullptr;
    const auto* row = desired && catalog ? CompanionRow(*catalog, desired->key) : nullptr;
    auto live = PetSystem::LiveTypedSeal(owner);
    const auto id = desired ? desired->id.Value() : 0;
    const bool canFollow = row && !entry->base.retired && WorldAllowsFollower(owner) && !OtherFollower(owner, live);
    if (live && (live != id || !canFollow))
    {
        PetSystem::RecallTyped(owner, live);
        live = 0;
    }
    if (!desired)
    {
        g_registry.remove<CompanionProjection>(owner);
        return 0;
    }

    auto& projection = g_registry.get_or_emplace<CompanionProjection>(owner);
    const auto now = get_dword_time();
    // Reconcile pending intent after the login affect load or a temporary map
    // restriction. Rate-limit failed materialization without changing SQL.
    if (!live && canFollow && now - projection.checked >= 500u)
    {
        const auto race = FollowerRace(*catalog, *row);
        if (race && PetSystem::SummonTyped(owner, race, id, row->kind == item::CompanionKind::Mount))
            live = id;
    }
    if (now - projection.checked >= 500u)
        projection.checked = now;
    projection.live = live;
    return live;
}

void TickTypedCompanions()
{
    // Refresh may remove a projection. Keep a stable list across that pass.
    std::vector<entt::entity> due;
    const auto now = get_dword_time();
    for (auto [owner, projection] : g_registry.view<CompanionProjection>().each())
        if (now - projection.checked >= 500u)
            due.push_back(owner);
    for (auto owner : due)
    {
        const auto* projection = g_registry.try_get<CompanionProjection>(owner);
        if (!projection || !InGame(owner))
            continue;
        const auto before = projection->live;
        if (RefreshTypedCompanion(owner) != before)
            SendTypedInventory(owner);
    }
}

bool HandleTypedCompanionPacket(entt::entity owner, const char* data)
{
    static_assert(sizeof(TPacketCGTypedCompanion) == 10);
    if (!InGame(owner))
        return true;
    TPacketCGTypedCompanion packet{};
    std::memcpy(&packet, data, sizeof(packet));
    const auto action = static_cast<Action>(packet.bAction);
    if (!packet.qwItemId || (action != Action::Summon && action != Action::Dismiss))
        return true;
    const auto* typed = g_registry.try_get<ecs::TypedItems>(owner);
    const auto* login = g_registry.try_get<ecs::TypedItemLogin>(owner);
    const auto catalog = store::CurrentCatalog();
    if (!typed || !login || !catalog)
        return true;
    if (g_registry.all_of<ecs::TypedItemBusy>(owner))
    {
        SendRefusal(owner, "Az előző tárgyművelet még folyamatban van.");
        return true;
    }

    const auto& state = typed->state;
    const auto found = std::ranges::find_if(state.items, [&](const auto& i) {
        return i.id.Value() == packet.qwItemId && !i.destroyed && i.companion
            && i.placement && i.placement->kind == item::ItemStorageKind::Inventory;
    });
    if (found == state.items.end())
    {
        SendRefusal(owner, "A pecsét nincs a leltáradban.");
        return true;
    }
    const bool summon = action == Action::Summon;
    if (found->companion->recallRequested == summon)
    {
        SendTypedInventory(owner);
        return true;
    }
    const auto* row = CompanionRow(*catalog, found->key);
    const auto* active = Requested(*typed);
    const auto live = PetSystem::LiveTypedSeal(owner);
    const auto facts = TypedItemEffectFacts(owner);
    const bool worldAllowsFollower = WorldAllowsFollower(owner);
    if (!row || (summon && (!facts.allowed || !facts.alive || !worldAllowsFollower || !CanSummonFromBag(owner)
        || (active && active->id != found->id) || OtherFollower(owner, live) || !FollowerRace(*catalog, *row))))
    {
        SendRefusal(owner, "A társ ebben az állapotban nem idézhető meg.");
        return true;
    }

    item::CompanionRequest request{found->key};
    request.state = *found->companion;
    if (live == found->id.Value())
    {
        request.state.phase = item::CompanionPhase::Following;
        request.state.activeFormId = row->form;
    }
    request.instanceId = found->id.Value();
    request.revision = found->revision;
    request.ownerId = state.fence.owner.id;
    request.actorId = ecs::PlayerRuntime::GetPlayerID(owner);
    request.rulesetRevision = 1;
    request.count = found->count;
    request.refinementLevel = found->refinementLevel;
    request.characterLevel = facts.actor.level;
    request.characterClass = facts.actor.classIndex + 1;
    request.sex = facts.actor.sexIndex + 1;
    request.action = action;
    request.nowUtcMs = TypedItemEffectNow();
    request.admittedByServer = true;
    request.ownerAlive = facts.alive;
    request.inCombat = facts.actor.inCombat;
    request.worldAllowsCompanion = worldAllowsFollower;
    if (RAND_bytes(request.operationId.data(), static_cast<int>(request.operationId.size())) != 1)
    {
        SendRefusal(owner, "A társ művelete nem indítható el.");
        return true;
    }
    auto commit = item::MakeCompanionCommit(*catalog, state.fence, state.storages, state.items, request);
    auto permit = ecs::resource_admission::Enter({owner}, auriga::resource_admission::Operation::ItemUse);
    if (!commit || !permit || !permit->Current())
    {
        SendRefusal(owner, "A társ művelete nem engedélyezett.");
        return true;
    }

    const auto pid = static_cast<std::uint32_t>(request.actorId);
    const auto token = login->token;
    const auto fence = state.fence;
    auto held = std::make_shared<auriga::resource_admission::Permit>(std::move(*permit));
    g_registry.emplace_or_replace<ecs::TypedItemBusy>(owner);
    const bool queued = store::RequestCommit(fence.owner, fence.sessionGeneration, std::move(*commit),
        [pid, token, held](store::CommitOutcome outcome) {
            const auto owner = CHARACTER_MANAGER::instance().FindEntityByPID(pid);
            const auto* login = g_registry.valid(owner) ? g_registry.try_get<ecs::TypedItemLogin>(owner) : nullptr;
            if (!login || login->token != token)
                return;
            if (!outcome.state)
            {
                LOG_ERROR("Typed companion owner reload failed pid {}", pid);
                if (auto* desc = ecs::PlayerRuntime::GetDesc(owner))
                    desc->SetPhase(PHASE_CLOSE);
                return;
            }
            g_registry.emplace_or_replace<ecs::TypedItems>(owner, std::move(*outcome.state));
            g_registry.remove<ecs::TypedItemBusy>(owner);
            if (!outcome.write)
                SendRefusal(owner, "A társ művelete nem menthető.");
            else
                SendTypedInventory(owner);
            PumpTypedItemGrants(owner);
        });
    if (!queued)
    {
        g_registry.remove<ecs::TypedItemBusy>(owner);
        SendRefusal(owner, "A tárgytároló nem elérhető.");
    }
    return true;
}
