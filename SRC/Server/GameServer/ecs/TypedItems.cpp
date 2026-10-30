#include "TypedItems.hpp"
#include "components/item_proto_components.hpp"
#include <algorithm>
#include <cstdint>
#include <set>
#include <utility>

namespace ecs::item
{
namespace p = domain::persistence;
using E = TypedItemError;

namespace
{
bool ValidFence(const domain::ItemOwnerFence& fence) noexcept
{
    return domain::ValidItemOwner(fence.owner) && fence.sessionGeneration && fence.revision;
}
bool ValidWallet(const domain::ItemWallet& wallet, domain::ItemOwner owner) noexcept
{
    const bool kind = wallet.currency == domain::WalletCurrency::Gold ? owner.kind == domain::ItemOwnerKind::Character
                                                                      : wallet.currency == domain::WalletCurrency::DragonCoin &&
                                                                            owner.kind == domain::ItemOwnerKind::Account;
    return wallet.owner == owner && kind && wallet.revision && wallet.balance <= static_cast<std::uint64_t>(INT64_MAX);
}
std::size_t Cells(const domain::ItemStorageLayout& layout) noexcept
{
    return std::size_t{layout.columns} * layout.rows * layout.pages;
}
std::optional<std::size_t> GridIndex(const std::vector<StorageGrid>& grids, domain::ItemStorage storage) noexcept
{
    const auto found = std::ranges::find(grids, storage, [](const StorageGrid& grid) { return grid.layout.storage; });
    if (found == grids.end()) return std::nullopt;
    return static_cast<std::size_t>(found - grids.begin());
}
const OwnerState* State(const entt::registry& registry, entt::entity owner)
{
    return registry.valid(owner) ? registry.try_get<OwnerState>(owner) : nullptr;
}
// Footprint of one placed, valid item in its grid; nullopt when illegal.
std::optional<std::pair<std::size_t, std::vector<std::uint32_t>>> Footprint(const domain::ItemCatalog& catalog,
                                                                           const domain::PersistentItem& item,
                                                                           const std::vector<StorageGrid>& grids)
{
    if (!item.placement) return std::nullopt;
    const auto index = GridIndex(grids, {item.owner, item.placement->kind, item.placement->storageId});
    if (!index) return std::nullopt;
    auto cells = domain::OccupiedItemCells(catalog, item, grids[*index].layout);
    if (!cells) return std::nullopt;
    return std::pair{*index, std::move(*cells)};
}
} // namespace

entt::entity FindOwner(const entt::registry& registry, domain::ItemOwner owner)
{
    const auto* index = registry.ctx().find<OwnerIndex>();
    if (!index) return entt::null;
    const auto found = index->owners.find(owner);
    if (found == index->owners.end() || !State(registry, found->second)) return entt::null;
    return found->second;
}

std::expected<entt::entity, E> Materialize(entt::registry& registry, const domain::ItemCatalog& catalog,
                                           const p::LoadedItemOwner& loaded, entt::entity holder)
{
    if (!ValidFence(loaded.fence)) return std::unexpected(E::InvalidOwner);
    const auto owner = loaded.fence.owner;
    if (FindOwner(registry, owner) != entt::null) return std::unexpected(E::AlreadyLoaded);

    // Validate everything before creating any entity: all or nothing.
    std::vector<StorageGrid> grids;
    for (const auto& layout : loaded.storages)
    {
        if (!domain::ValidItemStorage(layout) || layout.storage.owner != owner) return std::unexpected(E::InvalidStorage);
        grids.push_back({layout, std::vector<entt::entity>(Cells(layout), entt::entity{entt::null})});
    }
    std::ranges::sort(grids, {}, [](const StorageGrid& grid) { return grid.layout.storage; });
    if (std::ranges::adjacent_find(grids, {}, [](const StorageGrid& grid) { return grid.layout.storage; }) != grids.end())
        return std::unexpected(E::InvalidStorage);

    auto wallets = loaded.wallets;
    std::ranges::sort(wallets, {}, &domain::ItemWallet::currency);
    for (std::size_t i = 0; i < wallets.size(); ++i)
        if (!ValidWallet(wallets[i], owner) || (i && wallets[i - 1].currency == wallets[i].currency)) return std::unexpected(E::InvalidWallet);

    struct Planned
    {
        const domain::PersistentItem* item;
        std::size_t grid;
        std::vector<std::uint32_t> cells;
    };
    std::vector<Planned> planned;
    std::set<std::uint64_t> ids;
    std::set<std::pair<std::size_t, std::uint32_t>> occupied;
    for (const auto& item : loaded.items)
    {
        if (!ids.insert(item.id.Value()).second) return std::unexpected(E::DuplicateItem);
        if (item.owner != owner || item.destroyed || !domain::ValidatePersistentItem(catalog, item)) return std::unexpected(E::InvalidItem);
        auto footprint = Footprint(catalog, item, grids);
        if (!footprint) return std::unexpected(E::InvalidPlacement);
        for (const auto cell : footprint->second)
            if (!occupied.emplace(footprint->first, cell).second) return std::unexpected(E::Overlap);
        planned.push_back({&item, footprint->first, std::move(footprint->second)});
    }

    const auto ownerEntity = registry.create();
    OwnerState state{loaded.fence, std::move(wallets), std::move(grids), {}};
    for (const auto& plan : planned)
    {
        const auto& item = *plan.item;
        const auto entity = registry.create();
        registry.emplace<Identity>(entity, item.id, item.key);
        registry.emplace<Refinement>(entity, item.refinementLevel);
        registry.emplace<Revision>(entity, item.revision);
        registry.emplace<Stack>(entity, item.count);
        registry.emplace<DurableOwner>(entity, item.owner, ownerEntity);
        registry.emplace<Binding>(entity, item.boundTo);
        registry.emplace<Placement>(entity, *item.placement);
        registry.emplace<Attributes>(entity, item.attributes);
        registry.emplace<StoneSockets>(entity, item.stones);
        if (item.accessory) registry.emplace<AccessorySocket>(entity, *item.accessory);
        state.items.emplace(item.id.Value(), entity);
        for (const auto cell : plan.cells) state.storages[plan.grid].cells[cell] = entity;
    }
    registry.emplace<OwnerState>(ownerEntity, std::move(state));
    registry.emplace<OwnerHolder>(ownerEntity, holder);
    auto* index = registry.ctx().find<OwnerIndex>();
    if (!index) index = &registry.ctx().emplace<OwnerIndex>();
    index->owners[owner] = ownerEntity;
    return ownerEntity;
}

std::expected<domain::PersistentItem, E> ProjectItem(const entt::registry& registry, entt::entity entity)
{
    if (!registry.valid(entity)) return std::unexpected(E::MissingComponent);
    const auto* identity = registry.try_get<Identity>(entity);
    const auto* refinement = registry.try_get<Refinement>(entity);
    const auto* revision = registry.try_get<Revision>(entity);
    const auto* stack = registry.try_get<Stack>(entity);
    const auto* owner = registry.try_get<DurableOwner>(entity);
    const auto* binding = registry.try_get<Binding>(entity);
    const auto* attributes = registry.try_get<Attributes>(entity);
    const auto* stones = registry.try_get<StoneSockets>(entity);
    if (!identity || !refinement || !revision || !stack || !owner || !binding || !attributes || !stones)
        return std::unexpected(E::MissingComponent);
    domain::PersistentItem item{identity->id, identity->key, owner->owner};
    item.revision = revision->value;
    item.refinementLevel = refinement->level;
    item.count = stack->count;
    item.boundTo = binding->boundTo;
    item.destroyed = false; // live entities only; tombstones stay in SQL
    if (const auto* placement = registry.try_get<Placement>(entity)) item.placement = placement->at;
    item.attributes = attributes->value;
    item.stones = stones->slots;
    if (const auto* accessory = registry.try_get<AccessorySocket>(entity)) item.accessory = accessory->state;
    return item;
}

std::expected<p::LoadedItemOwner, E> Project(const entt::registry& registry, entt::entity owner)
{
    const auto* state = State(registry, owner);
    if (!state) return std::unexpected(E::NotAnOwner);
    p::LoadedItemOwner out;
    out.fence = state->fence;
    out.wallets = state->wallets;
    for (const auto& grid : state->storages) out.storages.push_back(grid.layout);
    for (const auto& [id, entity] : state->items)
    {
        auto item = ProjectItem(registry, entity);
        if (!item) return std::unexpected(item.error());
        if (item->id.Value() != id) return std::unexpected(E::GridMismatch);
        out.items.push_back(std::move(*item));
    }
    return out;
}

std::expected<void, E> Verify(entt::registry& registry, const domain::ItemCatalog& catalog, entt::entity owner)
{
    // Assure the legacy storage before taking references into other storages.
    const auto& legacy = registry.storage<ecs::ItemProtoRef>();
    const auto projected = Project(registry, owner);
    if (!projected) return std::unexpected(projected.error());
    const auto& state = registry.get<OwnerState>(owner);
    if (FindOwner(registry, state.fence.owner) != owner) return std::unexpected(E::GridMismatch);
    std::vector<std::vector<entt::entity>> expected;
    for (const auto& grid : state.storages)
    {
        if (grid.cells.size() != Cells(grid.layout)) return std::unexpected(E::GridMismatch);
        expected.emplace_back(grid.cells.size(), entt::entity{entt::null});
    }
    for (std::size_t n = 0; n < projected->items.size(); ++n)
    {
        const auto& item = projected->items[n];
        const auto entity = state.items.at(item.id.Value());
        if (legacy.contains(entity)) return std::unexpected(E::LegacyComponent);
        const auto& durable = registry.get<DurableOwner>(entity);
        if (durable.ownerEntity != owner || item.owner != state.fence.owner) return std::unexpected(E::InvalidItem);
        if (!domain::ValidatePersistentItem(catalog, item)) return std::unexpected(E::InvalidItem);
        const auto footprint = Footprint(catalog, item, state.storages);
        if (!footprint) return std::unexpected(E::InvalidPlacement);
        for (const auto cell : footprint->second)
        {
            auto& slot = expected[footprint->first][cell];
            if (slot != entt::null) return std::unexpected(E::Overlap);
            slot = entity;
        }
    }
    for (std::size_t i = 0; i < expected.size(); ++i)
        if (expected[i] != state.storages[i].cells) return std::unexpected(E::GridMismatch);
    return {};
}

namespace
{
void Write(entt::registry& registry, entt::entity entity, const domain::PersistentItem& item, entt::entity owner)
{
    registry.emplace_or_replace<Identity>(entity, item.id, item.key);
    registry.emplace_or_replace<Refinement>(entity, item.refinementLevel);
    registry.emplace_or_replace<Revision>(entity, item.revision);
    registry.emplace_or_replace<Stack>(entity, item.count);
    registry.emplace_or_replace<DurableOwner>(entity, item.owner, owner);
    registry.emplace_or_replace<Binding>(entity, item.boundTo);
    if (item.placement) registry.emplace_or_replace<Placement>(entity, *item.placement);
    else registry.remove<Placement>(entity);
    registry.emplace_or_replace<Attributes>(entity, item.attributes);
    registry.emplace_or_replace<StoneSockets>(entity, item.stones);
    if (item.accessory) registry.emplace_or_replace<AccessorySocket>(entity, *item.accessory);
    else registry.remove<AccessorySocket>(entity);
}
} // namespace

std::expected<void, E> Apply(entt::registry& registry, const domain::ItemCatalog& catalog, entt::entity owner, const domain::ItemCommit& commit)
{
    auto* state = registry.valid(owner) ? registry.try_get<OwnerState>(owner) : nullptr;
    if (!state) return std::unexpected(E::NotAnOwner);
    // One owner per loaded state; multi-owner commits (trade) are F4.6.
    if (commit.owners.size() != 1 || commit.owners.front() != state->fence) return std::unexpected(E::Diverged);
    for (const auto& layout : commit.storages)
    {
        const auto grid = GridIndex(state->storages, layout.storage);
        if (!grid || state->storages[*grid].layout != layout) return std::unexpected(E::Diverged);
    }
    auto wallets = state->wallets;
    for (const auto& change : commit.wallets)
    {
        const auto found = std::ranges::find(wallets, change.before.currency, &domain::ItemWallet::currency);
        if (found == wallets.end() || *found != change.before || change.after.owner != state->fence.owner) return std::unexpected(E::Diverged);
        *found = change.after;
    }

    std::set<std::uint64_t> changed;
    for (const auto& change : commit.items)
        if (!changed.insert(change.after.id.Value()).second) return std::unexpected(E::Diverged);

    // Simulate the grids by instance id: release every "before" footprint,
    // then take every "after" footprint.
    std::vector<std::vector<std::uint64_t>> cells;
    for (const auto& grid : state->storages)
    {
        std::vector<std::uint64_t> ids(grid.cells.size(), 0);
        for (std::size_t i = 0; i < ids.size(); ++i)
            if (grid.cells[i] != entt::null) ids[i] = registry.get<Identity>(grid.cells[i]).id.Value();
        cells.push_back(std::move(ids));
    }
    for (const auto& change : commit.items)
    {
        const auto id = change.after.id.Value();
        const auto existing = state->items.find(id);
        if (!change.before)
        {
            if (existing != state->items.end()) return std::unexpected(E::Diverged);
            continue;
        }
        if (existing == state->items.end()) return std::unexpected(E::Diverged);
        const auto current = ProjectItem(registry, existing->second);
        if (!current || *current != *change.before) return std::unexpected(E::Diverged);
        const auto footprint = Footprint(catalog, *current, state->storages);
        if (!footprint) return std::unexpected(E::Diverged);
        for (const auto cell : footprint->second)
        {
            if (cells[footprint->first][cell] != id) return std::unexpected(E::Diverged);
            cells[footprint->first][cell] = 0;
        }
    }
    for (const auto& change : commit.items)
    {
        const auto& after = change.after;
        if (after.owner != state->fence.owner) return std::unexpected(E::Diverged);
        if (after.destroyed) continue;
        if (!domain::ValidatePersistentItem(catalog, after)) return std::unexpected(E::InvalidItem);
        const auto footprint = Footprint(catalog, after, state->storages);
        if (!footprint) return std::unexpected(E::InvalidPlacement);
        for (const auto cell : footprint->second)
        {
            auto& slot = cells[footprint->first][cell];
            if (slot) return std::unexpected(E::Overlap);
            slot = after.id.Value();
        }
    }

    // Validated: mutate.
    for (const auto& change : commit.items)
    {
        const auto id = change.after.id.Value();
        const auto existing = state->items.find(id);
        if (change.after.destroyed)
        {
            registry.destroy(existing->second);
            state->items.erase(existing);
            continue;
        }
        const auto entity = existing != state->items.end() ? existing->second : registry.create();
        Write(registry, entity, change.after, owner);
        state->items[id] = entity;
    }
    for (std::size_t g = 0; g < cells.size(); ++g)
        for (std::size_t i = 0; i < cells[g].size(); ++i)
            state->storages[g].cells[i] = cells[g][i] ? state->items.at(cells[g][i]) : entt::entity{entt::null};
    state->wallets = std::move(wallets);
    ++state->fence.revision;
    return {};
}

bool Release(entt::registry& registry, entt::entity owner)
{
    const auto* state = State(registry, owner);
    if (!state) return false;
    const auto durable = state->fence.owner;
    for (const auto& [id, entity] : state->items)
        if (registry.valid(entity)) registry.destroy(entity);
    if (auto* index = registry.ctx().find<OwnerIndex>())
        if (const auto found = index->owners.find(durable); found != index->owners.end() && found->second == owner)
            index->owners.erase(found);
    registry.destroy(owner);
    return true;
}

entt::entity ItemAt(const entt::registry& registry, entt::entity owner, domain::ItemStorage storage, std::uint32_t cell)
{
    const auto* state = State(registry, owner);
    if (!state) return entt::null;
    const auto index = GridIndex(state->storages, storage);
    if (!index || cell >= state->storages[*index].cells.size()) return entt::null;
    return state->storages[*index].cells[cell];
}
} // namespace ecs::item
