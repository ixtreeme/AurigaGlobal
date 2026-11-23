#include <item/InventorySort.hpp>
#include <algorithm>
#include <tuple>

namespace auriga::item
{
namespace
{
using Error = ItemPersistenceError;

bool ValidScope(std::uint8_t scope)
{
    return scope == SortBackpack || scope < SpecialInventoryTypes;
}

ItemStorageKind StorageKind(std::uint8_t scope)
{
    return scope == SortBackpack ? ItemStorageKind::Inventory : ItemStorageKind::SpecialInventory;
}

std::pair<std::uint32_t, std::uint32_t> Area(const ItemStorageLayout& layout, std::uint8_t scope)
{
    const auto perPage = std::uint32_t{layout.columns} * layout.rows;
    if (scope == SortBackpack)
        return {0, perPage * layout.pages};
    const auto begin = std::uint32_t{scope} * SpecialInventoryPagesPerType;
    if (begin >= layout.pages)
        return {0, 0};
    return {begin * perPage, std::min<std::uint32_t>(layout.pages, begin + SpecialInventoryPagesPerType) * perPage};
}

bool FixedSlot(const ItemCatalog& catalog, const PersistentItem& item, std::uint8_t scope)
{
    const auto clock = FindPersistentItemClock(catalog, item.key);
    return (clock && clock->clock != CostumeClock::Permanent) || item.companion || item.growthPet ||
        (item.petSupply && (item.petSupply->containedSeal || item.petSupply->equippedSeal)) ||
        item.shopPermitDeadline || item.privateShop || (item.soul && item.soul->active) ||
        (item.toggle && item.toggle->active) || (item.automatic && item.automatic->active) ||
        (scope != SortBackpack && SpecialInventoryType(item.key.Category()) != scope);
}

struct Entry
{
    const PersistentItem* original;
    const ItemBaseRow* definition;
    std::uint32_t anchor;
    bool fixed;
};

bool Pack(const ItemStorageLayout& layout, std::uint32_t begin, std::uint32_t end,
    std::vector<Entry>& entries, bool pinLarge)
{
    const auto pageSize = std::uint32_t{layout.rows} * layout.columns;
    std::vector<bool> used(end - begin, false);
    const auto fixed = [pinLarge](const Entry& entry) {
        return entry.fixed || (pinLarge && (entry.definition->width != 1 || entry.definition->height != 1));
    };
    const auto mark = [&](const Entry& entry, std::uint32_t anchor) {
        for (unsigned y = 0; y < entry.definition->height; ++y)
            for (unsigned x = 0; x < entry.definition->width; ++x)
                used[anchor - begin + y * layout.columns + x] = true;
    };
    for (auto& entry : entries)
        if (fixed(entry))
        {
            entry.anchor = entry.original->placement->anchor;
            mark(entry, entry.anchor);
        }

    for (auto& entry : entries)
    {
        if (fixed(entry))
            continue;
        auto anchor = begin;
        for (; anchor < end; ++anchor)
        {
            const auto offset = anchor % pageSize;
            if (offset % layout.columns + entry.definition->width > layout.columns ||
                offset / layout.columns + entry.definition->height > layout.rows)
                continue;
            bool free = true;
            for (unsigned y = 0; y < entry.definition->height && free; ++y)
                for (unsigned x = 0; x < entry.definition->width && free; ++x)
                    free = !used[anchor - begin + y * layout.columns + x];
            if (free)
                break;
        }
        if (anchor == end)
            return false;
        entry.anchor = anchor;
        mark(entry, anchor);
    }
    return true;
}
}

std::expected<std::optional<ItemCommit>, ItemPersistenceError> MakeInventorySortCommit(
    const ItemCatalog& catalog, std::array<std::uint8_t, 16> operationId, ItemOwnerFence owner,
    std::span<const ItemStorageLayout> layouts, std::span<const PersistentItem> inventory, std::uint8_t scope)
{
    if (!ValidScope(scope) || owner.owner.kind != ItemOwnerKind::Character)
        return std::unexpected(Error::InvalidStorage);
    ItemCommit commit;
    commit.operationId = operationId;
    commit.catalogRevision = catalog.Revision();
    commit.catalogHash = catalog.ContentHash();
    commit.kind = ItemOperationKind::Move;
    commit.outcomeCode = InventorySortOutcome;
    commit.result = {scope};
    commit.owners = {owner};
    std::size_t areaCells = 0;
    bool found = false;
    for (const auto& layout : layouts)
    {
        auto storage = layout.storage;
        if (storage.owner != owner.owner || storage.kind != StorageKind(scope))
            continue;
        if (!ValidItemStorage(layout))
            return std::unexpected(Error::InvalidStorage);
        const auto [begin, end] = Area(layout, scope);
        if (end == begin)
            continue;
        found = true;
        areaCells += end - begin;
        if (areaCells > MaxInventorySortCells)
            return std::unexpected(Error::LimitExceeded);
        std::vector<Entry> entries;
        std::vector<bool> originalCells(end - begin, false);
        for (const auto& item : inventory)
        {
            if (item.destroyed || item.owner != storage.owner || !item.placement ||
                item.placement->kind != storage.kind || item.placement->storageId != storage.id ||
                item.placement->anchor < begin || item.placement->anchor >= end)
                continue;
            const auto cells = OccupiedItemCells(catalog, item, layout);
            if (!cells)
                return std::unexpected(cells.error());
            for (auto cell : *cells)
            {
                if (cell < begin || cell >= end || originalCells[cell - begin])
                    return std::unexpected(Error::InvalidPlacement);
                originalCells[cell - begin] = true;
            }
            entries.push_back({&item, &catalog.Find(item.key)->base, item.placement->anchor, FixedSlot(catalog, item, scope)});
        }
        std::ranges::sort(entries, {}, [](const Entry& entry) {
            return std::tuple{entry.original->key.Category(), std::string_view(entry.definition->nameKey),
                -static_cast<int>(entry.original->refinementLevel), entry.original->key, entry.original->placement->anchor};
        });
        // Crowded pages can block a full repack. Keep the larger footprints in
        // their original cells in that case and sort into the remaining cells.
        if (!Pack(layout, begin, end, entries, false) && !Pack(layout, begin, end, entries, true))
            return std::unexpected(Error::InvalidPlacement);
        const auto unchanged = commit.items.size();
        for (const auto& entry : entries)
            if (entry.anchor != entry.original->placement->anchor)
            {
                auto moved = *entry.original;
                moved.placement->anchor = entry.anchor;
                ++moved.revision;
                commit.items.push_back({*entry.original, std::move(moved)});
            }
        if (commit.items.size() != unchanged)
            commit.storages.push_back(layout);
    }
    if (!found)
        return std::unexpected(Error::InvalidStorage);
    if (commit.items.empty())
        return std::nullopt;
    const auto valid = ValidateItemCommit(catalog, commit);
    if (!valid)
        return std::unexpected(valid.error());
    return std::optional{std::move(commit)};
}

bool ValidInventorySort(const ItemCatalog& catalog, const ItemCommit& commit)
{
    if (commit.kind != ItemOperationKind::Move || commit.owners.size() != 1 || commit.items.empty() ||
        commit.owners.front().owner.kind != ItemOwnerKind::Character || commit.result.size() != 1 ||
        !ValidScope(commit.result.front()) || !commit.wallets.empty() || commit.ground || commit.recovery ||
        commit.learning || commit.petStudyCooldown || commit.currencyAward || commit.nativeAward ||
        commit.currencyDrop || commit.privateShop || commit.dungeonEntry)
        return false;
    const auto scope = commit.result.front();
    const auto owner = commit.owners.front().owner;
    for (const auto& change : commit.items)
    {
        if (!change.before || change.before->destroyed || !change.before->placement ||
            change.before->owner != owner || change.before->placement->kind != StorageKind(scope) ||
            FixedSlot(catalog, *change.before, scope))
            return false;
        const auto& before = *change.before;
        const auto& after = change.after;
        const auto layout = std::ranges::find(commit.storages,
            ItemStorage{owner, before.placement->kind, before.placement->storageId}, &ItemStorageLayout::storage);
        if (layout == commit.storages.end() || !ValidItemStorage(*layout))
            return false;
        const auto [begin, end] = Area(*layout, scope);
        if (before.placement->anchor < begin || before.placement->anchor >= end || !after.placement ||
            after.placement->kind != before.placement->kind || after.placement->storageId != before.placement->storageId ||
            after.placement->anchor < begin || after.placement->anchor >= end || *after.placement == *before.placement)
            return false;
        auto expected = before;
        ++expected.revision;
        expected.placement = after.placement;
        if (expected != after)
            return false;
    }
    return true;
}
}
