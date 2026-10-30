#pragma once
// F4.3 (IC-034): typed item instances in the ECS. One entity per item, built
// from a loaded PersistentItem. No vnum, no TItemTable/proto pointer and no
// catalog pointer: catalog data is looked up by ItemKey when needed. These
// entities never carry the legacy ecs::Item* components.
#include <item/PersistentItems.hpp>
#include <entt/entity/entity.hpp>
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace ecs::item
{
namespace domain = auriga::item;

// --- one item instance -----------------------------------------------------
struct Identity
{
    domain::ItemInstanceId id;
    domain::ItemKey key;
};
struct Refinement
{
    std::uint16_t level = 0;
};
struct Revision
{
    std::uint64_t value = 1;
};
struct Stack
{
    std::uint32_t count = 1;
};
// The durable owner and the owner entity that indexes this item.
struct DurableOwner
{
    domain::ItemOwner owner;
    entt::entity ownerEntity{entt::null};
};
struct Binding
{
    std::optional<domain::ItemOwner> boundTo;
};
struct Placement
{
    domain::ItemPlacement at;
};
struct Attributes
{
    domain::InstanceAttributes value;
};
struct StoneSockets
{
    std::array<domain::StoneSocketSlot, domain::EquipmentStoneSockets> slots{};
};
struct AccessorySocket
{
    domain::AccessorySocketState state;
};

// --- one loaded owner (its own entity) ---------------------------------------
struct StorageGrid
{
    domain::ItemStorageLayout layout;
    std::vector<entt::entity> cells; // columns * rows * pages; null = free
};
struct OwnerState
{
    domain::ItemOwnerFence fence;
    std::vector<domain::ItemWallet> wallets;   // sorted by currency
    std::vector<StorageGrid> storages;         // sorted by storage
    std::map<std::uint64_t, entt::entity> items; // instance id -> item entity
};
// The live entity (character/session) this owner was loaded for; may be null.
struct OwnerHolder
{
    entt::entity holder{entt::null};
};
// Registry context: at most one loaded copy of each durable owner.
struct OwnerIndex
{
    std::map<domain::ItemOwner, entt::entity> owners;
};
} // namespace ecs::item
