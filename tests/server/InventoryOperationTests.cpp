// F4.4 (IC-034): basic inventory operations as atomic write sets.
#include "PersistentItemFixture.hpp"
#include <item/InventoryOperations.hpp>
#include <iostream>
#include <source_location>
#include <stdexcept>

using namespace auriga::item;
namespace f = persistent_item_fixture;
using E = ItemPersistenceError;

namespace
{
unsigned checks = 0;
void Check(bool good, const char* why, std::source_location at = std::source_location::current())
{
    ++checks;
    if (!good) throw std::runtime_error(std::string(why) + " at " + std::to_string(at.line()));
}
const ItemOwner Owner = f::Character();
const ItemStorage Inventory{Owner, ItemStorageKind::Inventory, 1};
const ItemStorageLayout Equipment{{Owner, ItemStorageKind::Equipment, 1}, 8, 1, 1};
ItemInstanceId Id(std::uint64_t value) { return ItemInstanceId::Create(value).value(); }
ItemOperationId Op(std::uint8_t n)
{
    ItemOperationId op{};
    op[0] = n;
    op[15] = 0x44;
    return op;
}
PersistentItem Leather(std::uint64_t id, std::uint32_t count, std::uint32_t anchor)
{
    auto item = f::Leather(id);
    item.count = count;
    item.placement->anchor = anchor;
    return item;
}
PersistentItem Bracelet()
{
    auto item = f::Leather(5);
    item.key = item_catalog_fixture::Bracelet();
    item.count = 1;
    item.accessory = AccessorySocketState{3, 2, 1000, false};
    item.placement = ItemPlacement{ItemStorageKind::Equipment, 1, static_cast<unsigned>(EquipmentSlot::Wrist) - 1};
    return item;
}
struct State
{
    std::vector<ItemStorageLayout> storages{f::Inventory(), Equipment};
    std::vector<PersistentItem> items;
    ItemOwnerView View() const { return {{Owner, 7, 3}, storages, items}; }
};
State Initial()
{
    State s;
    auto bound = Leather(3, 2, 2);
    bound.boundTo = Owner;
    // Sword: cells 0,5,10. Leathers at 1 (3 units), 2 (bound), 6 (1 unit).
    s.items = {f::Sword(), Leather(2, 3, 1), bound, Leather(4, 1, 6), Bracelet()};
    return s;
}
const PersistentItem& After(const ItemCommit& r, std::uint64_t id)
{
    for (const auto& change : r.items)
        if (change.after.id.Value() == id) return change.after;
    throw std::runtime_error("change missing");
}
template <class T> bool Fails(const T& result, E code)
{
    if (result) return false;
    if (result.error() != code) std::cerr << "error=" << static_cast<int>(result.error()) << " want=" << static_cast<int>(code) << '\n';
    return result.error() == code;
}
bool Valid(const ItemCatalog& c, const std::expected<ItemCommit, E>& r)
{
    return r && ValidateItemCommit(c, *r) && ItemCommitDigest(c, *r) && r->owners.size() == 1 && r->owners[0] == ItemOwnerFence{Owner, 7, 3};
}

void Moves(const ItemCatalog& c)
{
    const auto s = Initial();
    const auto v = s.View();
    const auto sword = f::Sword().id;

    auto r = MakeMoveCommit(c, v, Op(1), Id(2), {ItemStorageKind::Inventory, 1, 3});
    Check(Valid(c, r) && r->kind == ItemOperationKind::Move && r->items.size() == 1 && r->storages == std::vector{f::Inventory()},
          "move to a free cell");
    Check(After(*r, 2).placement->anchor == 3 && After(*r, 2).revision == 2 && After(*r, 2).count == 3, "moved state");
    Check(Fails(MakeMoveCommit(c, v, Op(1), Id(2), {ItemStorageKind::Inventory, 1, 1}), E::InvalidChange), "no-op move");
    Check(Fails(MakeMoveCommit(c, v, Op(1), Id(99), {ItemStorageKind::Inventory, 1, 3}), E::MissingItem), "unknown item");
    Check(Fails(MakeMoveCommit(c, v, Op(1), Id(2), {ItemStorageKind::Inventory, 2, 3}), E::InvalidStorage), "unknown storage");
    Check(Fails(MakeMoveCommit(c, v, Op(1), Id(2), {ItemStorageKind::Inventory, 1, 90}), E::InvalidPlacement), "outside the grid");
    Check(Fails(MakeMoveCommit(c, v, Op(1), sword, {ItemStorageKind::Inventory, 1, 35}), E::InvalidPlacement), "over the page bottom");

    // Swap only onto the occupant's anchor.
    r = MakeMoveCommit(c, v, Op(2), Id(2), {ItemStorageKind::Inventory, 1, 2});
    Check(Valid(c, r) && r->items.size() == 2 && After(*r, 2).placement->anchor == 2 && After(*r, 3).placement->anchor == 1 &&
          After(*r, 3).boundTo == Owner, "swap of two stacks");
    r = MakeMoveCommit(c, v, Op(3), sword, {ItemStorageKind::Inventory, 1, 2});
    Check(Valid(c, r) && After(*r, sword.Value()).placement->anchor == 2 && After(*r, 3).placement->anchor == 0, "tall item swaps with one cell");
    Check(Fails(MakeMoveCommit(c, v, Op(3), Id(2), {ItemStorageKind::Inventory, 1, 5}), E::InvalidPlacement), "drop on a non-anchor cell");
    Check(Fails(MakeMoveCommit(c, v, Op(3), sword, {ItemStorageKind::Inventory, 1, 1}), E::InvalidPlacement), "target covers two items");

    // Equipment: the slot is the anchor.
    r = MakeMoveCommit(c, v, Op(4), sword, {ItemStorageKind::Equipment, 1, 0});
    Check(Valid(c, r) && r->storages.size() == 2 && !After(*r, sword.Value()).boundTo, "equip weapon (no binding)");
    Check(Fails(MakeMoveCommit(c, v, Op(4), sword, {ItemStorageKind::Equipment, 1, 1}), E::InvalidPlacement), "weapon in the body slot");
    r = MakeMoveCommit(c, v, Op(5), Id(5), {ItemStorageKind::Inventory, 1, 15});
    Check(Valid(c, r) && After(*r, 5).accessory == Bracelet().accessory, "unequip keeps the accessory state");

    // Bind on equip: entering Equipment binds; it never unbinds.
    auto source = f::Catalog();
    for (auto& item : source.items)
        if (item.key == item_catalog_fixture::Bracelet()) item.binding = BindingPolicy::OnEquip;
    const auto binding = BuildItemCatalog(source).value();
    auto unequipped = s;
    unequipped.items[4].placement = ItemPlacement{ItemStorageKind::Inventory, 1, 15};
    r = MakeMoveCommit(*binding, unequipped.View(), Op(6), Id(5), Bracelet().placement.value());
    Check(Valid(*binding, r) && After(*r, 5).boundTo == Owner, "equip binds an OnEquip item");
    auto equipped = unequipped;
    equipped.items[4] = After(*r, 5);
    r = MakeMoveCommit(*binding, equipped.View(), Op(7), Id(5), {ItemStorageKind::Inventory, 1, 15});
    Check(Valid(*binding, r) && After(*r, 5).boundTo == Owner, "unequip keeps the binding");
}

void Stacks(const ItemCatalog& c)
{
    const auto s = Initial();
    const auto v = s.View();
    auto r = MakeMergeCommit(c, v, Op(10), Id(2), Id(4), 2);
    Check(Valid(c, r) && After(*r, 2).count == 1 && After(*r, 4).count == 3 && r->kind == ItemOperationKind::Move, "partial merge");
    r = MakeMergeCommit(c, v, Op(11), Id(2), Id(4), 3);
    Check(Valid(c, r) && After(*r, 2).destroyed && !After(*r, 2).count && !After(*r, 2).placement && After(*r, 4).count == 4, "full merge leaves a tombstone");
    Check(Fails(MakeMergeCommit(c, v, Op(12), Id(2), Id(4), 4), E::InvalidChange), "more than the source");
    Check(Fails(MakeMergeCommit(c, v, Op(12), Id(2), Id(4), 0), E::InvalidChange), "zero units");
    Check(Fails(MakeMergeCommit(c, v, Op(12), Id(2), Id(2), 1), E::InvalidChange), "merge into itself");
    Check(Fails(MakeMergeCommit(c, v, Op(12), Id(2), Id(3), 1), E::InvalidChange), "bound and unbound stacks");
    Check(Fails(MakeMergeCommit(c, v, Op(12), f::Sword().id, Id(4), 1), E::InvalidChange), "different item types");
    auto full = s;
    full.items[3].count = 199;
    Check(Fails(MakeMergeCommit(c, full.View(), Op(12), Id(2), Id(4), 2), E::InvalidChange), "beyond the maximum stack");
    Check(Valid(c, MakeMergeCommit(c, full.View(), Op(12), Id(2), Id(4), 1)), "up to the maximum stack");

    r = MakeSplitCommit(c, v, Op(13), Id(2), 1, {ItemStorageKind::Inventory, 1, 15}, Id(100));
    Check(Valid(c, r) && After(*r, 2).count == 2 && After(*r, 100).count == 1 && After(*r, 100).revision == 1 &&
          After(*r, 100).key == f::Leather().key && After(*r, 100).placement->anchor == 15, "split into a new stack");
    Check(Fails(MakeSplitCommit(c, v, Op(13), Id(2), 3, {ItemStorageKind::Inventory, 1, 15}, Id(100)), E::InvalidChange), "split everything");
    Check(Fails(MakeSplitCommit(c, v, Op(13), Id(2), 0, {ItemStorageKind::Inventory, 1, 15}, Id(100)), E::InvalidChange), "split nothing");
    Check(Fails(MakeSplitCommit(c, v, Op(13), Id(2), 1, {ItemStorageKind::Inventory, 1, 6}, Id(100)), E::InvalidPlacement), "split onto a stack");
    Check(Fails(MakeSplitCommit(c, v, Op(13), Id(2), 1, {ItemStorageKind::Inventory, 1, 15}, Id(4)), E::InvalidChange), "reused instance id");
    Check(Fails(MakeSplitCommit(c, v, Op(13), Id(2), 1, {ItemStorageKind::Inventory, 2, 0}, Id(100)), E::InvalidStorage), "split into an unknown storage");
}

void Removal(const ItemCatalog& c)
{
    const auto s = Initial();
    const auto v = s.View();
    const auto sword = f::Sword();
    auto r = MakeDestroyCommit(c, v, Op(20), sword.id);
    const auto& gone = After(*r, sword.id.Value());
    Check(Valid(c, r) && r->kind == ItemOperationKind::Consume && gone.destroyed && !gone.count && !gone.placement &&
          gone.attributes == sword.attributes && gone.stones == sword.stones && gone.refinementLevel == sword.refinementLevel,
          "destroy keeps the last complete state");
    r = MakeConsumeCommit(c, v, Op(21), Id(2), 1);
    Check(Valid(c, r) && After(*r, 2).count == 2 && !After(*r, 2).destroyed, "consume one unit");
    r = MakeConsumeCommit(c, v, Op(22), Id(2), 3);
    Check(Valid(c, r) && After(*r, 2).destroyed, "consume the last units");
    Check(Fails(MakeConsumeCommit(c, v, Op(23), Id(2), 4), E::InvalidChange) && Fails(MakeConsumeCommit(c, v, Op(23), Id(2), 0), E::InvalidChange),
          "consume a wrong count");
    Check(Fails(MakeDestroyCommit(c, v, Op(24), Id(99)), E::MissingItem), "destroy an unknown item");

    // The view belongs to one owner; a foreign item is not part of it.
    auto foreign = s;
    foreign.items[1].owner = {ItemOwnerKind::Character, 43};
    Check(Fails(MakeDestroyCommit(c, foreign.View(), Op(25), Id(2)), E::MissingItem), "foreign item");
    // A tombstone in the view is not live.
    auto tomb = s;
    tomb.items[1].destroyed = true;
    tomb.items[1].count = 0;
    tomb.items[1].placement.reset();
    Check(Fails(MakeConsumeCommit(c, tomb.View(), Op(26), Id(2), 1), E::MissingItem), "tombstone consumed");
    Check(Fails(MakeDestroyCommit(c, v, ItemOperationId{}, Id(2)), E::InvalidOperation), "zero operation id");
}
} // namespace

int main()
{
    try
    {
        const auto catalog = BuildItemCatalog(f::Catalog()).value();
        for (const auto& item : Initial().items) Check(ValidatePersistentItem(*catalog, item).has_value(), "fixture item invalid");
        Moves(*catalog);
        Stacks(*catalog);
        Removal(*catalog);
        std::cout << checks << " Inventory operation checks passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "InventoryOperationTests failed: " << error.what() << '\n';
        return 1;
    }
}
