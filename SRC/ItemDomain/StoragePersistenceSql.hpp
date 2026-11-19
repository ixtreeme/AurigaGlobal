#pragma once
#include "ItemPersistenceSql.hpp"
#include <item/StorageRuntime.hpp>
namespace auriga::item::persistence {
// Authenticated runtime bootstrap, idempotent and preserving every existing row.
std::expected<void,ItemPersistenceError> EnsureRuntimeStorageOwner(CAsyncSQL&,const ItemCatalog&,ItemOwner);
// Authenticated character login: only empty, zero-gold legacy rows may start a
// native owner. Existing native owners remain unchanged; legacy items/gold
// require the explicit importer. False means no eligible owner was created.
std::expected<bool,ItemPersistenceError> EnsureFreshCharacterOwner(CAsyncSQL&,const ItemCatalog&,std::uint32_t pid);
struct CharacterBirthGrant {
    ItemGrant item;
    std::optional<InstanceAttributes> attributes;
    std::optional<EquipmentSlot> equipped;
};
struct CharacterBirthProfile {
    std::uint8_t race=0,skillGroup=0;
    std::string contract;
    std::vector<CharacterBirthGrant> grants;
};
// Only a pristine owner (revision 1, no items or money) may receive this
// server-authored native pack. Its receipt and all births commit atomically.
std::expected<void,ItemPersistenceError> IssueFreshCharacterItems(CAsyncSQL&,const ItemCatalog&,std::uint32_t pid,
    std::span<const CharacterBirthProfile>,std::uint64_t nowUtcMs);
std::expected<LoadedItemOwner,ItemPersistenceError> LoadAuthorizedStorageOwner(CAsyncSQL&,const ItemCatalog&,ItemOwner,
    std::uint64_t session,const StorageAuthorization&);
// Once empty, close the shared gate before handing disband to the legacy guild
// coordinator. All cores' subsequent typed deposits then fail inside SQL.
std::expected<void,ItemPersistenceError> CloseEmptyTypedGuild(CAsyncSQL&,const ItemCatalog&,std::uint32_t guild,std::uint32_t leader);
}
