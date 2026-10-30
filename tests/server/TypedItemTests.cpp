// F4.3 (IC-034): typed item instances in the ECS. Load result -> entities ->
// persistence model must be identical; invalid input leaves the registry
// unchanged; the entities carry no legacy proto pointer.
#include "PersistentItemFixture.hpp"
#include "../../SRC/Server/GameServer/ecs/TypedItems.hpp"
#include "../../SRC/Server/GameServer/ecs/components/item_proto_components.hpp"
#include <algorithm>
#include <functional>
#include <iostream>
#include <source_location>
#include <stdexcept>

using namespace auriga::item;
namespace p = auriga::item::persistence;
namespace f = persistent_item_fixture;
namespace ti = ecs::item;
using E = ti::TypedItemError;

namespace
{
unsigned checks = 0;
void Check(bool good, const char* why, std::source_location at = std::source_location::current())
{
    ++checks;
    if (!good) throw std::runtime_error(std::string(why) + " at " + std::to_string(at.line()));
}
const ItemOwner Character = f::Character();
const ItemStorageLayout Equipment{{Character, ItemStorageKind::Equipment, 1}, 8, 1, 1};

PersistentItem Bracelet()
{
    auto item = f::Leather(5);
    item.key = item_catalog_fixture::Bracelet();
    item.count = 1;
    item.accessory = AccessorySocketState{3, 2, 1000, false};
    item.placement = ItemPlacement{ItemStorageKind::Equipment, 1, static_cast<unsigned>(EquipmentSlot::Wrist) - 1};
    return item;
}
PersistentItem BoundLeather()
{
    auto item = f::Leather(3);
    item.count = 2;
    item.revision = 4;
    item.boundTo = Character;
    item.placement->anchor = 2;
    return item;
}
p::LoadedItemOwner Owner()
{
    p::LoadedItemOwner owner;
    owner.fence = {Character, 7, 3};
    owner.storages = {Equipment, f::Inventory()}; // deliberately not in canonical order
    owner.wallets = {f::Gold()};
    owner.items = {BoundLeather(), f::Sword(), Bracelet(), f::Leather()};
    return owner;
}
p::LoadedItemOwner Canonical(p::LoadedItemOwner owner)
{
    std::ranges::sort(owner.storages, {}, [](const auto& l) { return l.storage; });
    std::ranges::sort(owner.wallets, {}, &ItemWallet::currency);
    std::ranges::sort(owner.items, {}, [](const auto& i) { return i.id; });
    return owner;
}
bool Same(const p::LoadedItemOwner& a, const p::LoadedItemOwner& b)
{
    return a.fence == b.fence && a.storages == b.storages && a.wallets == b.wallets && a.items == b.items;
}
std::size_t Entities(const entt::registry& r)
{
    return r.storage<entt::entity>()->free_list();
}
bool Unchanged(entt::registry& r, std::size_t before)
{
    return Entities(r) == before;
}

void RoundTrip(const ItemCatalog& c)
{
    for (const auto& item : Owner().items) Check(ValidatePersistentItem(c, item).has_value(), "fixture item invalid");
    entt::registry registry;
    const auto holder = registry.create();
    const auto owner = ti::Materialize(registry, c, Owner(), holder);
    Check(owner.has_value(), "valid owner not materialized");
    Check(ti::Verify(registry, c, *owner).has_value(), "fresh owner inconsistent");
    Check(ti::FindOwner(registry, Character) == *owner && registry.get<ti::OwnerHolder>(*owner).holder == holder, "owner index lost");
    const auto projected = ti::Project(registry, *owner);
    Check(projected && Same(*projected, Canonical(Owner())), "load -> ECS -> persistence model differs");
    Check(registry.storage<ti::Identity>().size() == 4 && registry.storage<ti::AccessorySocket>().size() == 1, "entity per item");

    // Footprints: the sword is three cells tall; equipment uses slot anchors.
    const auto& state = registry.get<ti::OwnerState>(*owner);
    const auto sword = state.items.at(f::Sword().id.Value());
    const ItemStorage inventory{Character, ItemStorageKind::Inventory, 1};
    for (const auto cell : {0u, 5u, 10u}) Check(ti::ItemAt(registry, *owner, inventory, cell) == sword, "sword footprint");
    Check(ti::ItemAt(registry, *owner, inventory, 15) == entt::null && ti::ItemAt(registry, *owner, inventory, 90) == entt::null, "free/out-of-grid cell");
    Check(ti::ItemAt(registry, *owner, Equipment.storage, 5) == state.items.at(5), "equipped bracelet");
    Check(ti::ItemAt(registry, *owner, {Character, ItemStorageKind::Inventory, 2}, 0) == entt::null, "unknown storage");
    for (const auto& [id, entity] : state.items)
        Check(!registry.all_of<ecs::ItemProtoRef>(entity) && registry.get<ti::DurableOwner>(entity).ownerEntity == *owner, "legacy proto or wrong owner");

    // ECS changes are what the projection reports.
    registry.get<ti::Refinement>(sword).level = 10;
    registry.get<ti::Revision>(sword).value = 2;
    Check(ti::Verify(registry, c, *owner).has_value(), "valid refinement change rejected");
    const auto changed = ti::ProjectItem(registry, sword);
    Check(changed && changed->refinementLevel == 10 && changed->revision == 2 && changed->attributes == f::Sword().attributes &&
          changed->stones == f::Sword().stones, "component change not projected");

    // Damage is detected, not silently re-derived.
    const auto leather = state.items.at(f::Leather().id.Value());
    registry.get<ti::Placement>(leather).at.anchor = 3;
    Check(ti::Verify(registry, c, *owner).error() == E::GridMismatch, "moved item without grid update accepted");
    registry.get<ti::Placement>(leather).at.anchor = 1;
    registry.get<ti::Refinement>(leather).level = 1;
    Check(ti::Verify(registry, c, *owner).error() == E::InvalidItem, "level outside the profile accepted");
    registry.get<ti::Refinement>(leather).level = 0;
    registry.remove<ti::Stack>(leather);
    Check(ti::Project(registry, *owner).error() == E::MissingComponent, "item without count projected");
    registry.emplace<ti::Stack>(leather, f::Leather().count);
    registry.emplace<ecs::ItemProtoRef>(leather);
    Check(ti::Verify(registry, c, *owner).error() == E::LegacyComponent, "legacy proto pointer accepted");
    registry.remove<ecs::ItemProtoRef>(leather);
    registry.get<ti::DurableOwner>(leather).ownerEntity = holder;
    Check(ti::Verify(registry, c, *owner).error() == E::InvalidItem, "item of another owner entity accepted");
    registry.get<ti::DurableOwner>(leather).ownerEntity = *owner;
    Check(ti::Verify(registry, c, *owner).has_value(), "repaired owner rejected");

    // Second copy of the same owner is refused; the registry does not change.
    const auto before = Entities(registry);
    Check(ti::Materialize(registry, c, Owner(), holder).error() == E::AlreadyLoaded && Unchanged(registry, before), "owner loaded twice");

    // Another owner in the same registry stays independent.
    const ItemOwner account{ItemOwnerKind::Account, 7};
    p::LoadedItemOwner safebox;
    safebox.fence = {account, 2, 1};
    safebox.storages = {{{account, ItemStorageKind::Safebox, 1}, 5, 9, 1}};
    safebox.wallets = {{account, WalletCurrency::DragonCoin, 1, 50}};
    auto stored = f::Leather(9);
    stored.owner = account;
    stored.placement = ItemPlacement{ItemStorageKind::Safebox, 1, 0};
    safebox.items = {stored};
    const auto accountOwner = ti::Materialize(registry, c, safebox, holder);
    Check(accountOwner && *accountOwner != *owner && ti::Verify(registry, c, *accountOwner).has_value(), "second owner not materialized");
    Check(Same(*ti::Project(registry, *accountOwner), safebox), "account owner round trip");

    // Release removes exactly one owner and its items.
    Check(ti::Release(registry, *owner) && !ti::Release(registry, *owner), "release");
    Check(ti::FindOwner(registry, Character) == entt::null && !registry.valid(sword) && !registry.valid(*owner), "released entities remain");
    Check(ti::Project(registry, *owner).error() == E::NotAnOwner && ti::ProjectItem(registry, sword).error() == E::MissingComponent, "released owner projected");
    Check(ti::FindOwner(registry, account) == *accountOwner && ti::Verify(registry, c, *accountOwner).has_value(), "release touched another owner");
    Check(registry.storage<ti::Identity>().size() == 1, "released items still have components");
    const auto again = ti::Materialize(registry, c, Owner(), holder);
    Check(again && Same(*ti::Project(registry, *again), Canonical(Owner())), "owner not reloadable after release");
    Check(ti::Project(registry, holder).error() == E::NotAnOwner, "holder projected as an owner");
}

void Rejects(const ItemCatalog& c)
{
    entt::registry registry;
    const auto before = Entities(registry);
    const auto reject = [&](E code, const char* why, const std::function<void(p::LoadedItemOwner&)>& change) {
        auto owner = Owner();
        change(owner);
        const auto result = ti::Materialize(registry, c, owner, entt::null);
        if (result || result.error() != code)
            std::cerr << "materialize code=" << (result ? -1 : static_cast<int>(result.error())) << " want=" << static_cast<int>(code) << '\n';
        Check(!result && result.error() == code && Unchanged(registry, before) && ti::FindOwner(registry, Character) == entt::null, why);
    };
    reject(E::InvalidOwner, "zero fence revision", [](auto& o) { o.fence.revision = 0; });
    reject(E::InvalidOwner, "zero session", [](auto& o) { o.fence.sessionGeneration = 0; });
    reject(E::InvalidStorage, "foreign storage", [](auto& o) { o.storages[1].storage.owner.id = 43; });
    reject(E::InvalidStorage, "duplicate storage", [](auto& o) { o.storages.push_back(f::Inventory()); });
    reject(E::InvalidStorage, "account storage on a character", [](auto& o) { o.storages[1].storage.kind = ItemStorageKind::Safebox; });
    reject(E::InvalidWallet, "account currency", [](auto& o) { o.wallets[0].currency = WalletCurrency::DragonCoin; });
    reject(E::InvalidWallet, "duplicate wallet", [](auto& o) { o.wallets.push_back(f::Gold()); });
    reject(E::InvalidWallet, "foreign wallet", [](auto& o) { o.wallets[0].owner.id = 43; });
    reject(E::DuplicateItem, "duplicate instance", [](auto& o) { o.items.push_back(f::Leather()); });
    reject(E::InvalidItem, "foreign item", [](auto& o) { o.items[0].owner.id = 43; });
    reject(E::InvalidItem, "tombstone", [](auto& o) { o.items[0].destroyed = true; o.items[0].count = 0; o.items[0].placement.reset(); });
    reject(E::InvalidItem, "unknown prototype", [](auto& o) { o.items[0].key = ItemKey::Create(ItemCategory::Material, 999).value(); });
    reject(E::InvalidItem, "level outside the profile", [](auto& o) { o.items[1].refinementLevel = 31; });
    reject(E::InvalidPlacement, "unknown storage", [](auto& o) { o.items[0].placement->storageId = 2; });
    reject(E::InvalidPlacement, "wrong equipment slot", [](auto& o) { o.items[2].placement->anchor = 0; });
    reject(E::InvalidPlacement, "outside the grid", [](auto& o) { o.items[3].placement->anchor = 90; });
    reject(E::InvalidPlacement, "crosses the page bottom", [](auto& o) { o.items[1].placement->anchor = 35; }); // 3 tall from row 7
    reject(E::Overlap, "two items in one cell", [](auto& o) { o.items[0].placement->anchor = 5; });
    // The probe itself counts: a valid owner adds one owner and four item entities.
    Check(ti::Materialize(registry, c, Owner(), entt::null) && Entities(registry) == before + 5, "entity count probe");
}
} // namespace

int main()
{
    try
    {
        const auto catalog = BuildItemCatalog(f::Catalog()).value();
        RoundTrip(*catalog);
        Rejects(*catalog);
        std::cout << "TypedItemTests passed " << checks << " checks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "TypedItemTests failed: " << error.what() << '\n';
        return 1;
    }
}
