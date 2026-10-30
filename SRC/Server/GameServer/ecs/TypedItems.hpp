#pragma once
// F4.3 (IC-034): a loaded item owner as ECS entities, and back. Main thread
// only (single registry owner). Nothing here reads SQL or the legacy item path.
#include "components/typed_item_components.hpp"
#include <ItemPersistenceSql.hpp>
#include <entt/entity/registry.hpp>
#include <expected>

namespace ecs::item
{
enum class TypedItemError : std::uint8_t
{
    AlreadyLoaded,    // this durable owner already has entities in the registry
    NotAnOwner,       // the entity is not a loaded owner
    InvalidOwner,     // bad owner or fence
    InvalidStorage,   // bad, foreign or duplicate storage layout
    InvalidWallet,    // bad, foreign or duplicate wallet
    InvalidItem,      // fails the catalog/persistence validation
    InvalidPlacement, // no placement, unknown storage or illegal footprint
    Overlap,          // two items claim one cell
    DuplicateItem,    // one instance id twice
    MissingComponent, // an item entity lost one of its typed components
    GridMismatch,     // stored cells differ from the items' footprints
    LegacyComponent,  // a typed item entity carries a legacy proto pointer
    Diverged,         // a commit's "before" state is not the current ECS state
};

// Creates one owner entity and one entity per item, all or nothing: on any
// error the registry is unchanged. `holder` is the live entity the owner was
// loaded for (character or session) and may be null.
std::expected<entt::entity, TypedItemError> Materialize(entt::registry& registry, const domain::ItemCatalog& catalog,
                                                        const domain::persistence::LoadedItemOwner& loaded, entt::entity holder);

// The current ECS state as the persistence model. Storages are sorted by
// storage, wallets by currency and items by instance id.
std::expected<domain::persistence::LoadedItemOwner, TypedItemError> Project(const entt::registry& registry, entt::entity owner);
std::expected<domain::PersistentItem, TypedItemError> ProjectItem(const entt::registry& registry, entt::entity item);

// Full consistency check: every item is valid for the catalog, belongs to this
// owner entity, has no legacy component, and the grids hold exactly the
// footprints of the items.
std::expected<void, TypedItemError> Verify(entt::registry& registry, const domain::ItemCatalog& catalog, entt::entity owner);

// Applies a COMMITTED write set to the loaded owner, all or nothing (F4.4).
// The commit must have been built on exactly this state: its fence, every
// `before` item and wallet equal the ECS state, a new item id is unused.
// Otherwise Diverged (or a placement error) and nothing changes. The fence
// revision advances by one, as the database writer does.
std::expected<void, TypedItemError> Apply(entt::registry& registry, const domain::ItemCatalog& catalog, entt::entity owner,
                                          const domain::ItemCommit& commit);

// Destroys the owner entity and all of its item entities.
bool Release(entt::registry& registry, entt::entity owner);

entt::entity FindOwner(const entt::registry& registry, domain::ItemOwner owner);
entt::entity ItemAt(const entt::registry& registry, entt::entity owner, domain::ItemStorage storage, std::uint32_t cell);
} // namespace ecs::item
