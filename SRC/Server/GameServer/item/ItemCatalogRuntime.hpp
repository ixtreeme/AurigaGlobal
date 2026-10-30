#pragma once
// F4.1 (IC-034): the GameServer runs from the typed protos catalog that the
// durable item state is pinned to. There is no fallback to item_proto/vnum.
#include <item/ItemCatalog.hpp>
#include <CatalogSnapshot.hpp>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>

class CAsyncSQL;
struct _SQLMsg;

namespace auriga::item::runtime
{
// The catalog that player item state was written against
// (player.item_persistence_format, schema_version 1).
struct CatalogPin
{
    std::uint64_t revision = 0;
    ItemCatalogDigest hash{};
    bool operator==(const CatalogPin&) const = default;
};

enum class StartupErrorCode : std::uint8_t
{
    PinTableMissing,  // item persistence schema is not installed
    PinMissing,       // the singleton pin row is absent
    PinInvalid,       // wrong schema version, zero revision, bad hash length...
    PinQueryFailed,   // SQL error or malformed result while reading the pin
    CatalogConnection,
    CatalogSession,   // could not fix the dedicated session to RR / read-only
    CatalogRead,      // see `read`
    PinMismatch,      // loaded catalog identity differs from the pin
    AlreadyInstalled,
};
struct StartupError
{
    StartupErrorCode code{};
    std::optional<sql::ReadError> read;
    std::uint32_t sqlError = 0;
};

// Fixed, fully qualified SQL. Run it as a prepared statement so the binary
// digest keeps its exact length.
inline constexpr const char* PinQuery =
    "SELECT schema_version,catalog_revision,catalog_sha256 FROM player.item_persistence_format WHERE singleton=1";
inline constexpr std::uint32_t PinSchemaVersion = 1;

// Pure decoder of a completed PinQuery result.
std::expected<CatalogPin, StartupError> DecodePin(const _SQLMsg* message);

// Fixes the dedicated catalog session to REPEATABLE READ and READ ONLY, then
// reads the complete snapshot pinned to `pin`. The result is exactly the
// pinned catalog or an error; never a partial or unpinned catalog.
std::expected<std::shared_ptr<const ItemCatalog>, StartupError> LoadPinnedCatalog(CAsyncSQL& protos, CatalogPin pin);

// Process-wide immutable publication. Install exactly once on the main thread
// during boot, before the game loop, any worker or client session starts.
// F4.1 has no hot replacement; a new catalog needs a restart.
std::expected<void, StartupError> Install(std::shared_ptr<const ItemCatalog> catalog, CatalogPin pin);
std::shared_ptr<const ItemCatalog> Shared() noexcept;
std::optional<CatalogPin> InstalledPin() noexcept;

std::string Describe(const StartupError& error);
std::string HexDigest(const ItemCatalogDigest& digest);
} // namespace auriga::item::runtime
