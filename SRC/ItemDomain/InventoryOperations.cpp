#include <item/InventoryOperations.hpp>
#include <algorithm>
#include <map>
#include <set>

namespace auriga::item
{
namespace
{
using E = ItemPersistenceError;

const PersistentItem* Find(const ItemOwnerView& view, ItemInstanceId id)
{
    const auto found = std::ranges::find_if(view.items, [&](const PersistentItem& item) { return item.id == id && !item.destroyed; });
    return found == view.items.end() || found->owner != view.fence.owner ? nullptr : &*found;
}
const ItemStorageLayout* Layout(const ItemOwnerView& view, ItemStorage storage)
{
    const auto found = std::ranges::find(view.storages, storage, &ItemStorageLayout::storage);
    return found == view.storages.end() ? nullptr : &*found;
}
ItemStorage StorageOf(const PersistentItem& item, const ItemPlacement& at)
{
    return {item.owner, at.kind, at.storageId};
}
// Cells of `storage` held by live items other than the ignored ones.
std::expected<std::map<std::uint32_t, ItemInstanceId>, E> Occupancy(const ItemCatalog& c, const ItemOwnerView& view, ItemStorage storage,
                                                                   std::initializer_list<ItemInstanceId> ignore)
{
    const auto* layout = Layout(view, storage);
    if (!layout) return std::unexpected(E::InvalidStorage);
    std::map<std::uint32_t, ItemInstanceId> cells;
    for (const auto& item : view.items)
    {
        if (item.destroyed || !item.placement || StorageOf(item, *item.placement) != storage ||
            std::ranges::find(ignore, item.id) != ignore.end()) continue;
        auto footprint = OccupiedItemCells(c, item, *layout);
        if (!footprint) return std::unexpected(E::InvalidStoredData);
        for (const auto cell : *footprint)
            if (!cells.emplace(cell, item.id).second) return std::unexpected(E::InvalidStoredData);
    }
    return cells;
}
std::expected<std::vector<std::uint32_t>, E> Cells(const ItemCatalog& c, const ItemOwnerView& view, const PersistentItem& item)
{
    const auto* layout = Layout(view, StorageOf(item, *item.placement));
    if (!layout) return std::unexpected(E::InvalidStorage);
    return OccupiedItemCells(c, item, *layout);
}
bool Free(const std::map<std::uint32_t, ItemInstanceId>& taken, const std::vector<std::uint32_t>& cells)
{
    return std::ranges::none_of(cells, [&](std::uint32_t cell) { return taken.contains(cell); });
}
ItemCommit Base(const ItemCatalog& c, const ItemOwnerView& view, const ItemOperationId& op, ItemOperationKind kind)
{
    ItemCommit r;
    r.operationId = op;
    r.catalogRevision = c.Revision();
    r.catalogHash = c.ContentHash();
    r.kind = kind;
    r.owners = {view.fence};
    r.outcomeCode = 1;
    return r;
}
PersistentItem Next(const PersistentItem& before)
{
    auto after = before;
    ++after.revision;
    return after;
}
PersistentItem Tombstone(const PersistentItem& before)
{
    auto after = Next(before);
    after.destroyed = true;
    after.count = 0;
    after.placement.reset();
    return after;
}
void BindOnEquip(const ItemCatalog& c, PersistentItem& item)
{
    if (item.placement && item.placement->kind == ItemStorageKind::Equipment && !item.boundTo &&
        c.Find(item.key)->base.binding == BindingPolicy::OnEquip)
        item.boundTo = item.owner;
}
// Lists exactly the storages the changes touch, then validates the whole set.
std::expected<ItemCommit, E> Finish(const ItemCatalog& c, const ItemOwnerView& view, ItemCommit r)
{
    std::set<ItemStorage> used;
    for (const auto& change : r.items)
        for (const auto* item : {change.before ? &*change.before : nullptr, &change.after})
            if (item && item->placement) used.insert(StorageOf(*item, *item->placement));
    for (const auto& storage : used)
    {
        const auto* layout = Layout(view, storage);
        if (!layout) return std::unexpected(E::InvalidStorage);
        r.storages.push_back(*layout);
    }
    if (auto valid = ValidateItemCommit(c, r); !valid) return std::unexpected(valid.error());
    return r;
}
bool Stackable(const ItemCatalog& c, const PersistentItem& a, const PersistentItem& b)
{
    return a.key == b.key && a.refinementLevel == b.refinementLevel && a.boundTo == b.boundTo && a.attributes == b.attributes &&
           a.stones == b.stones && !a.accessory && !b.accessory && c.Find(a.key)->base.maxStack > 1;
}
} // namespace

std::expected<ItemCommit, E> MakeMoveCommit(const ItemCatalog& c, const ItemOwnerView& view, const ItemOperationId& op,
                                            ItemInstanceId id, ItemPlacement target)
{
    const auto* item = Find(view, id);
    if (!item) return std::unexpected(E::MissingItem);
    if (!item->placement) return std::unexpected(E::InvalidPlacement);
    if (*item->placement == target) return std::unexpected(E::InvalidChange);
    auto moved = Next(*item);
    moved.placement = target;
    BindOnEquip(c, moved);
    const auto movedCells = Cells(c, view, moved);
    if (!movedCells) return std::unexpected(movedCells.error() == E::InvalidStorage ? E::InvalidStorage : E::InvalidPlacement);
    const auto targetTaken = Occupancy(c, view, StorageOf(*item, target), {id});
    if (!targetTaken) return std::unexpected(targetTaken.error());

    std::set<ItemInstanceId> occupants;
    for (const auto cell : *movedCells)
        if (const auto found = targetTaken->find(cell); found != targetTaken->end()) occupants.insert(found->second);
    auto r = Base(c, view, op, ItemOperationKind::Move);
    if (occupants.empty())
    {
        r.items.push_back({*item, moved});
        return Finish(c, view, std::move(r));
    }
    if (occupants.size() != 1) return std::unexpected(E::InvalidPlacement);

    // Swap: dropped exactly on the single occupant's anchor, the occupant
    // takes the moved item's place.
    const auto* other = Find(view, *occupants.begin());
    if (!other) return std::unexpected(E::InvalidStoredData);
    if (other->placement != target) return std::unexpected(E::InvalidPlacement);
    auto swapped = Next(*other);
    swapped.placement = item->placement;
    BindOnEquip(c, swapped);
    const auto swappedCells = Cells(c, view, swapped);
    if (!swappedCells) return std::unexpected(E::InvalidPlacement);
    const auto sourceStorage = StorageOf(*item, *item->placement), targetStorage = StorageOf(*item, target);
    const auto sourceTaken = Occupancy(c, view, sourceStorage, {id, other->id});
    const auto targetRest = Occupancy(c, view, targetStorage, {id, other->id});
    if (!sourceTaken || !targetRest) return std::unexpected(E::InvalidStoredData);
    if (!Free(*targetRest, *movedCells) || !Free(*sourceTaken, *swappedCells)) return std::unexpected(E::InvalidPlacement);
    if (sourceStorage == targetStorage &&
        std::ranges::any_of(*movedCells, [&](std::uint32_t cell) { return std::ranges::find(*swappedCells, cell) != swappedCells->end(); }))
        return std::unexpected(E::InvalidPlacement);
    r.items.push_back({*item, moved});
    r.items.push_back({*other, swapped});
    return Finish(c, view, std::move(r));
}

std::expected<ItemCommit, E> MakeMergeCommit(const ItemCatalog& c, const ItemOwnerView& view, const ItemOperationId& op,
                                             ItemInstanceId sourceId, ItemInstanceId targetId, std::uint32_t count)
{
    const auto* source = Find(view, sourceId);
    const auto* target = Find(view, targetId);
    if (!source || !target) return std::unexpected(E::MissingItem);
    if (sourceId == targetId || !count || count > source->count || !Stackable(c, *source, *target)) return std::unexpected(E::InvalidChange);
    if (std::uint64_t{target->count} + count > c.Find(target->key)->base.maxStack) return std::unexpected(E::InvalidChange);
    auto grown = Next(*target);
    grown.count += count;
    auto r = Base(c, view, op, ItemOperationKind::Move);
    if (count == source->count) r.items.push_back({*source, Tombstone(*source)});
    else
    {
        auto shrunk = Next(*source);
        shrunk.count -= count;
        r.items.push_back({*source, shrunk});
    }
    r.items.push_back({*target, grown});
    return Finish(c, view, std::move(r));
}

std::expected<ItemCommit, E> MakeSplitCommit(const ItemCatalog& c, const ItemOwnerView& view, const ItemOperationId& op,
                                             ItemInstanceId sourceId, std::uint32_t count, ItemPlacement target, ItemInstanceId created)
{
    const auto* source = Find(view, sourceId);
    if (!source) return std::unexpected(E::MissingItem);
    if (!count || count >= source->count) return std::unexpected(E::InvalidChange);
    if (std::ranges::any_of(view.items, [&](const PersistentItem& item) { return item.id == created; })) return std::unexpected(E::InvalidChange);
    auto split = *source;
    split.id = created;
    split.revision = 1;
    split.count = count;
    split.placement = target;
    BindOnEquip(c, split);
    const auto cells = Cells(c, view, split);
    if (!cells) return std::unexpected(cells.error() == E::InvalidStorage ? E::InvalidStorage : E::InvalidPlacement);
    const auto taken = Occupancy(c, view, StorageOf(split, target), {});
    if (!taken) return std::unexpected(taken.error());
    if (!Free(*taken, *cells)) return std::unexpected(E::InvalidPlacement);
    auto rest = Next(*source);
    rest.count -= count;
    auto r = Base(c, view, op, ItemOperationKind::Move);
    r.items.push_back({*source, rest});
    r.items.push_back({std::nullopt, split});
    return Finish(c, view, std::move(r));
}

std::expected<ItemCommit, E> MakeDestroyCommit(const ItemCatalog& c, const ItemOwnerView& view, const ItemOperationId& op, ItemInstanceId id)
{
    const auto* item = Find(view, id);
    if (!item) return std::unexpected(E::MissingItem);
    auto r = Base(c, view, op, ItemOperationKind::Consume);
    r.items.push_back({*item, Tombstone(*item)});
    return Finish(c, view, std::move(r));
}

std::expected<ItemCommit, E> MakeConsumeCommit(const ItemCatalog& c, const ItemOwnerView& view, const ItemOperationId& op,
                                               ItemInstanceId id, std::uint32_t count)
{
    const auto* item = Find(view, id);
    if (!item) return std::unexpected(E::MissingItem);
    if (!count || count > item->count) return std::unexpected(E::InvalidChange);
    auto r = Base(c, view, op, ItemOperationKind::Consume);
    if (count == item->count) r.items.push_back({*item, Tombstone(*item)});
    else
    {
        auto rest = Next(*item);
        rest.count -= count;
        r.items.push_back({*item, rest});
    }
    return Finish(c, view, std::move(r));
}
} // namespace auriga::item
