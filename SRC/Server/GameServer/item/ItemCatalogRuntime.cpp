#include "stdafx.h"
#include "ItemCatalogRuntime.hpp"
#include "../core/GameConfig.h"

#include "CatalogSqlReader.hpp"
#include <SQL/AsyncSQL.h>

#include <atomic>
#include <fmt/format.h>

namespace auriga::item::runtime
{
namespace
{
std::atomic<std::shared_ptr<const ItemCatalog>> g_catalog;

constexpr const char* ReadErrorNames[] = {
	"InvalidPin", "RowLimit", "ByteBudget", "ColumnCount", "NullValue", "InvalidNumber",
	"InvalidText", "InvalidKey", "RegistryMismatch", "FormatMismatch", "OrphanRow",
	"DuplicateRow", "InvalidChild", "InvalidCatalog", "HashMismatch", "SqlFailure",
	"IsolationRequired", "SnapshotUnconfirmed", "ResultShape",
};

std::string Describe(const sql::ReadError& error)
{
	const auto code = static_cast<std::size_t>(error.code);
	std::string text = code < std::size(ReadErrorNames) ? ReadErrorNames[code] : "Unknown";
	if (error.table)
	{
		const auto table = static_cast<std::size_t>(*error.table);
		text += fmt::format(" table={} row={} column={}", sql::Tables()[table].name, error.row, error.column);
	}
	if (error.sqlError) text += fmt::format(" sql_errno={}", error.sqlError);
	if (error.catalogError) text += fmt::format(" catalog_error={}", static_cast<int>(error.catalogError->code));
	return text;
}

std::string Hex(const ItemCatalogDigest& digest)
{
	std::string out;
	for (const auto byte : digest) out += fmt::format("{:02x}", byte);
	return out;
}

std::optional<ItemCatalogDigest> Unhex(const std::string& text)
{
	if (text.size() != 64) return std::nullopt;
	ItemCatalogDigest digest {};
	for (std::size_t i = 0; i < digest.size(); ++i)
	{
		const auto nibble = [](char c) -> int { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; };
		const int high = nibble(text[i * 2]), low = nibble(text[i * 2 + 1]);
		if (high < 0 || low < 0) return std::nullopt;
		digest[i] = static_cast<std::uint8_t>(high * 16 + low);
	}
	return digest;
}
} // namespace

std::expected<std::optional<CatalogSummary>, std::string> LoadCatalogAtBoot(const CatalogBootSettings& settings, const GameSqlConfig& player)
{
	if (settings.mode == CatalogMode::Off)
	{
		g_catalog.store(nullptr);
		return std::optional<CatalogSummary> {};
	}
	sql::ReadPin pin { settings.revision, std::nullopt };
	if (!settings.sha256.empty())
	{
		pin.expectedHash = Unhex(settings.sha256);
		if (!pin.expectedHash) return std::unexpected(std::string("items.catalog_sha256 is not a valid digest"));
	}
	// A dedicated direct connection: the reader needs its own REPEATABLE READ
	// snapshot and must never share the player queue.
	CAsyncSQL connection;
	connection.SetTimeouts(10);
	connection.SetRetryConnectionErrors(false);
	if (!connection.Setup(player.host.c_str(), player.user.c_str(), player.password.c_str(), "protos", "utf8mb4", true, player.port))
		return std::unexpected(fmt::format("cannot connect to the protos catalog at {}:{}", player.host, player.port));
	auto catalog = sql::ReadCatalog(connection, pin);
	if (!catalog) return std::unexpected("the protos catalog is not usable: " + Describe(catalog.error()));
	CatalogSummary summary { Hex((*catalog)->ContentHash()), (*catalog)->Revision(), (*catalog)->Entries().size() };
	g_catalog.store(std::move(*catalog));
	return std::optional<CatalogSummary> { std::move(summary) };
}

std::shared_ptr<const ItemCatalog> CurrentCatalog() noexcept
{
	return g_catalog.load();
}
} // namespace auriga::item::runtime
