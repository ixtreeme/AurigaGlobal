#include "stdafx.h"
#include "AccountCollectionSystem.hpp"
#include "CollectionBonusRules.hpp"
#include "../item/ItemBackendState.hpp"
#include "../item/ItemCatalogRuntime.hpp"
#include "../item/ItemStore.hpp"
#include "../item/TypedEquipment.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/components/identity_components.hpp"
#include "../ecs/systems/MountSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../entity/char_manager.h"
#include "../network/desc.h"
#include "../network/p2p.h"
#include "../core/packet.h"
#include "../core/constants.h"
#include <Core/buffer.h>
#include <StoragePersistenceSql.hpp>

namespace AccountCollectionSystem
{
namespace
{
namespace item = auriga::item;
namespace store = item::runtime;
namespace sql = item::persistence;

struct Projection
{
    std::uint64_t revision = 0;
    std::uint16_t mountCount = 0;
    std::vector<item::BonusEntry> bonuses;
};
struct PendingLoad { bool repeat = false; };

auto Binding(std::uint8_t apply)
{
    return apply < MAX_APPLY_NUM
        ? EquipmentPointBindingFor(aApplyInfo[apply].bPointType)
        : std::nullopt;
}

CollectionBonuses::Totals Collect(const item::ItemCatalog& catalog, const sql::LoadedItemOwner& account,
                                  item::StorageRoute route, std::uint8_t window)
{
    CollectionBonuses::Totals totals(window);
    for (const auto& seal : account.items)
    {
        if (seal.destroyed || !seal.placement || seal.placement->kind != item::ItemStorageKind::Safebox ||
            seal.placement->storageId != item::AccountStorageId(route) ||
            !item::CanStoreInCollection(catalog, seal, route))
            continue;
        ++totals.storedSeals;
        if (!totals.AddItem(seal.key.LocalId()))
            continue;
        const auto add = [&](const item::BonusEntry& entry) {
            for (std::uint16_t apply = 1; apply < MAX_APPLY_NUM; ++apply)
                if (const auto binding = Binding(apply); binding && binding->bonus == entry.bonus)
                {
                    totals.AddApply(apply, static_cast<double>(entry.valueScaled) / binding->divisor);
                    return;
                }
        };
        if (const auto profiles = catalog.BonusProfilesAt(seal.key, seal.refinementLevel))
            for (const auto* profile : {profiles->base, profiles->refinement})
                if (profile)
                    for (const auto& bonus : profile->Data().entries)
                        add(bonus);
        for (const auto& slot : seal.attributes.slots)
            if (slot)
                add({slot->bonus, slot->valueScaled});
    }
    return totals;
}

void Send(LPDESC desc, const CollectionBonuses::Totals& totals)
{
    TPacketGCCollectionBonuses packet{};
    packet.header = HEADER_GC_COLLECTION_BONUSES;
    packet.size = sizeof(packet) + totals.bonuses.size() * sizeof(TCollectionBonusData);
    packet.window = totals.window;
    packet.loaded = true;
    packet.itemCount = totals.Count();
    packet.tiers = totals.Tiers();
    packet.threshold = totals.threshold;
    packet.bonusCount = totals.bonuses.size();
    TEMP_BUFFER buffer;
    buffer.write(&packet, sizeof(packet));
    for (const auto& [apply, bonus] : totals.bonuses)
    {
        const TCollectionBonusData row{apply, bonus.items, bonus.perTier};
        buffer.write(&row, sizeof(row));
    }
    desc->Packet(buffer.read_peek(), buffer.size());
}
}

void Apply(entt::entity owner, const sql::LoadedItemOwner& account)
{
    if (!g_registry.valid(owner) || account.fence.owner.kind != item::ItemOwnerKind::Account ||
        account.fence.owner.id != ecs::PlayerRuntime::GetAccountID(owner))
        return;
    const auto* previous = g_registry.try_get<Projection>(owner);
    const auto catalog = store::CurrentCatalog();
    auto* desc = ecs::PlayerRuntime::GetDesc(owner);
    if (!catalog || !desc || !desc->IsPhase(PHASE_GAME) || (previous && previous->revision >= account.fence.revision))
        return;
    const auto mounts = Collect(*catalog, account, item::StorageRoute::MountCollection, CollectionBonuses::MountWindow);
    const auto pets = Collect(*catalog, account, item::StorageRoute::PetCollection, CollectionBonuses::PetWindow);
    Projection projection;
    projection.revision = account.fence.revision;
    projection.mountCount = mounts.storedSeals;
    const bool mountCountChanged = !previous || previous->mountCount != projection.mountCount;
    for (const auto* totals : {&mounts, &pets})
    {
        for (const auto& [apply, bonus] : totals->bonuses)
            if (const auto binding = Binding(apply))
                projection.bonuses.push_back({binding->bonus, std::llround(bonus.Total(totals->Tiers()) * binding->divisor)});
        Send(desc, *totals);
    }
    g_registry.emplace_or_replace<Projection>(owner, std::move(projection));
    if (mountCountChanged)
        MountSystem::UpdateMountCountOverheadToViewers(owner);
    RefreshTypedEquipment(owner);
    SendTypedEquipment(owner);
}

void Request(entt::entity owner)
{
    if (!g_registry.valid(owner))
        return;
    if (auto* pending = g_registry.try_get<PendingLoad>(owner))
    {
        pending->repeat = true;
        return;
    }
    const auto* login = g_registry.try_get<ecs::TypedItemLogin>(owner);
    const auto account = ecs::PlayerRuntime::GetAccountID(owner);
    if (!login || !account)
        return;
    const auto token = login->token;
    const auto pid = ecs::PlayerRuntime::GetPlayerID(owner);
    g_registry.emplace<PendingLoad>(owner);
    if (!store::ItemStore::Instance().Submit([pid, token, account](CAsyncSQL& db, const item::ItemCatalog& catalog)
        -> store::ItemStore::Completion {
        const item::ItemOwner key{item::ItemOwnerKind::Account, account};
        auto state = sql::LoadCurrentItemOwner(db, catalog, key);
        return [pid, token, state = std::move(state)] {
            const auto owner = CHARACTER_MANAGER::instance().FindEntityByPID(pid);
            const auto* login = g_registry.valid(owner) ? g_registry.try_get<ecs::TypedItemLogin>(owner) : nullptr;
            if (!login || login->token != token)
                return;
            const auto* pending = g_registry.try_get<PendingLoad>(owner);
            const bool repeat = pending && pending->repeat;
            g_registry.remove<PendingLoad>(owner);
            if (state)
                Apply(owner, *state);
            if (repeat)
                Request(owner);
        };
    }))
        g_registry.remove<PendingLoad>(owner);
}

void Invalidate(std::uint32_t account)
{
    std::vector<entt::entity> players;
    for (const auto owner : g_registry.view<ecs::AccountID>())
        if (ecs::PlayerRuntime::GetAccountID(owner) == account && ecs::PlayerRuntime::GetDesc(owner))
            players.push_back(owner);
    for (const auto owner : players)
        Request(owner);
}

void Notify(std::uint32_t account)
{
    TPacketGGCollectionChanged packet{HEADER_GG_COLLECTION_CHANGED, account};
    P2P_MANAGER::instance().Send(&packet, sizeof(packet));
}

std::optional<std::uint16_t> MountCount(entt::entity owner)
{
    const auto* projection = g_registry.valid(owner) ? g_registry.try_get<Projection>(owner) : nullptr;
    return projection ? std::optional{projection->mountCount} : std::nullopt;
}

std::uint64_t Revision(entt::entity owner)
{
    const auto* projection = g_registry.try_get<Projection>(owner);
    return projection ? projection->revision : 0;
}

std::span<const item::BonusEntry> Bonuses(entt::entity owner)
{
    const auto* projection = g_registry.try_get<Projection>(owner);
    return projection ? std::span<const item::BonusEntry>{projection->bonuses} : std::span<const item::BonusEntry>{};
}
}
