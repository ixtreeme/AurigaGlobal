#pragma once
// Typed item catalog of the running GameServer (item system F4, IC-034).
//
// Item definitions/persistence pin stay fixed. Drop-only releases can replace
// their own immutable view while in-flight item work retains its catalog.
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>

struct GameSqlConfig;
class CAsyncSQL;

namespace auriga::item
{
class ItemCatalog;
}

namespace auriga::item::runtime
{
enum class CatalogMode : std::uint8_t { Off, Required };

struct CatalogBootSettings
{
	CatalogMode mode = CatalogMode::Required;
	std::uint64_t revision = 1;
	std::string sha256; // empty, or 64 lowercase hex characters
};

struct CatalogSummary
{
	std::string sha256;
	std::uint64_t revision = 0;
	std::size_t items = 0;
};

// Required mode reads protos over a dedicated short-lived
// connection with the player SQL account and validates it with the game's own
// catalog builder; any failure is returned and the caller must not start.
std::expected<std::optional<CatalogSummary>, std::string> LoadCatalogAtBoot(const CatalogBootSettings& settings, const GameSqlConfig& player);

// Null until the required base catalog is loaded.
std::shared_ptr<const ItemCatalog> CurrentCatalog() noexcept;
std::shared_ptr<const ItemCatalog> CurrentDropCatalog() noexcept;
std::expected<void,std::string> LoadLiveDropCatalog(CAsyncSQL&,std::shared_ptr<const ItemCatalog>);
void ConfigureDropReloadControl(const std::string& instance);
void TickLiveDropReload();

// items.drops = "typed" (F4 C7b-3, IC-035): monster kills roll the catalog
// drop tables instead of the legacy ones. Set once at boot; true only while a
// catalog is loaded.
bool TypedDropsEnabled() noexcept;
} // namespace auriga::item::runtime
