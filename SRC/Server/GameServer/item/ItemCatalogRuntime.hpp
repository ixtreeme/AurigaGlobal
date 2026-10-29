#pragma once
// Typed item catalog of the running GameServer (item system F4, IC-034).
//
// Loaded once at boot from the protos database and shared read-only. While the
// runtime has not switched over, the legacy item_proto path is unaffected; this
// only proves at start-up that the catalog the server will run on is present,
// complete and the expected release.
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>

struct GameSqlConfig;

namespace auriga::item
{
class ItemCatalog;
}

namespace auriga::item::runtime
{
enum class CatalogMode : std::uint8_t { Off, Required };

struct CatalogBootSettings
{
	CatalogMode mode = CatalogMode::Off;
	std::uint64_t revision = 1;
	std::string sha256; // empty, or 64 lowercase hex characters
};

struct CatalogSummary
{
	std::string sha256;
	std::uint64_t revision = 0;
	std::size_t items = 0;
};

// Off: does nothing. Required: reads protos over a dedicated short-lived
// connection with the player SQL account and validates it with the game's own
// catalog builder; any failure is returned and the caller must not start.
std::expected<std::optional<CatalogSummary>, std::string> LoadCatalogAtBoot(const CatalogBootSettings& settings, const GameSqlConfig& player);

// Null when the catalog is off.
std::shared_ptr<const ItemCatalog> CurrentCatalog() noexcept;
} // namespace auriga::item::runtime
