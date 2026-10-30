#include "ItemCatalogRuntime.hpp"
#include <CatalogSqlReader.hpp>
#include <SQL/AsyncSQL.h>
#include <atomic>
#include <charconv>
#include <mutex>

namespace auriga::item::runtime
{
namespace
{
// MariaDB/MySQL server error codes for a schema that is not installed.
constexpr std::uint32_t NoSuchTable = 1146, UnknownDatabase = 1049;

StartupError Error(StartupErrorCode code, std::uint32_t sqlError = 0)
{
    return StartupError{code, std::nullopt, sqlError};
}
template <class T> bool ParseUnsigned(const char* text, unsigned long length, T& out)
{
    // Digits only: no sign, blank, exponent or trailing byte is accepted.
    if (!text || !length || length > 20) return false;
    for (unsigned long i = 0; i < length; ++i)
        if (text[i] < '0' || text[i] > '9') return false;
    const auto end = text + length;
    const auto parsed = std::from_chars(text, end, out);
    return parsed.ec == std::errc() && parsed.ptr == end;
}
bool SameIdentity(const ItemCatalog& catalog, const CatalogPin& pin) noexcept
{
    return catalog.Revision() == pin.revision && catalog.ContentHash() == pin.hash;
}
std::expected<void, StartupError> Session(CAsyncSQL& connection, const char* statement)
{
    std::unique_ptr<SQLMsg> message(connection.DirectQuery(statement));
    if (!message) return std::unexpected(Error(StartupErrorCode::CatalogSession));
    if (message->uiSQLErrno) return std::unexpected(Error(StartupErrorCode::CatalogSession, message->uiSQLErrno));
    return {};
}
struct Published
{
    std::shared_ptr<const ItemCatalog> catalog;
    CatalogPin pin;
};
// Set once, then only read. The record lives for the whole process so that a
// reader never observes a destroyed catalog.
std::atomic<const Published*> published{nullptr};
std::mutex installing;

const char* ReadCodeName(sql::ReadErrorCode code)
{
    using C = sql::ReadErrorCode;
    switch (code)
    {
    case C::InvalidPin: return "invalid pin";
    case C::RowLimit: return "table row limit exceeded";
    case C::ByteBudget: return "catalog byte budget exceeded";
    case C::ColumnCount: return "unexpected column count";
    case C::NullValue: return "unexpected NULL value";
    case C::InvalidNumber: return "invalid number";
    case C::InvalidText: return "invalid text";
    case C::InvalidKey: return "invalid item key";
    case C::RegistryMismatch: return "category/bonus registry differs from this build";
    case C::FormatMismatch: return "protos.catalog_format differs from this build";
    case C::OrphanRow: return "orphan row";
    case C::DuplicateRow: return "duplicate row";
    case C::InvalidChild: return "invalid child row";
    case C::InvalidCatalog: return "catalog content failed validation";
    case C::HashMismatch: return "catalog content differs from the pinned hash";
    case C::SqlFailure: return "SQL failure";
    case C::IsolationRequired: return "REPEATABLE READ isolation required";
    case C::SnapshotUnconfirmed: return "snapshot commit unconfirmed";
    case C::ResultShape: return "malformed SQL result";
    }
    return "unknown read error";
}
} // namespace

std::expected<CatalogPin, StartupError> DecodePin(const _SQLMsg* message)
{
    if (!message) return std::unexpected(Error(StartupErrorCode::PinQueryFailed));
    if (message->uiSQLErrno)
    {
        const auto code = message->uiSQLErrno == NoSuchTable || message->uiSQLErrno == UnknownDatabase
            ? StartupErrorCode::PinTableMissing : StartupErrorCode::PinQueryFailed;
        return std::unexpected(Error(code, message->uiSQLErrno));
    }
    if (message->vec_pkResult.size() != 1 || !message->vec_pkResult.front() || !message->vec_pkResult.front()->bPrepared)
        return std::unexpected(Error(StartupErrorCode::PinQueryFailed));
    const auto& result = *message->vec_pkResult.front();
    if (result.preparedRows.size() != result.uiNumRows || result.preparedLengths.size() != result.preparedRows.size())
        return std::unexpected(Error(StartupErrorCode::PinQueryFailed));
    if (result.preparedRows.empty()) return std::unexpected(Error(StartupErrorCode::PinMissing));
    if (result.preparedRows.size() != 1) return std::unexpected(Error(StartupErrorCode::PinInvalid));
    const auto& row = result.preparedRows.front();
    const auto& lengths = result.preparedLengths.front();
    if (row.size() != 3 || lengths.size() != 3) return std::unexpected(Error(StartupErrorCode::PinQueryFailed));
    std::uint32_t schema = 0;
    CatalogPin pin;
    if (!ParseUnsigned(row[0], lengths[0], schema) || schema != PinSchemaVersion ||
        !ParseUnsigned(row[1], lengths[1], pin.revision) || pin.revision == 0 ||
        !row[2] || lengths[2] != pin.hash.size())
        return std::unexpected(Error(StartupErrorCode::PinInvalid));
    for (std::size_t i = 0; i < pin.hash.size(); ++i) pin.hash[i] = static_cast<std::uint8_t>(row[2][i]);
    return pin;
}

std::expected<std::shared_ptr<const ItemCatalog>, StartupError> LoadPinnedCatalog(CAsyncSQL& protos, CatalogPin pin)
{
    if (!pin.revision) return std::unexpected(Error(StartupErrorCode::PinInvalid));
    // The reader also verifies the isolation inside its own transaction. The
    // session is set explicitly so a server default does not block startup,
    // and READ ONLY keeps this connection from writing even with a broad account.
    if (auto set = Session(protos, "SET SESSION TRANSACTION ISOLATION LEVEL REPEATABLE READ"); !set)
        return std::unexpected(set.error());
    if (auto set = Session(protos, "SET SESSION TRANSACTION READ ONLY"); !set)
        return std::unexpected(set.error());
    auto catalog = sql::ReadCatalog(protos, sql::ReadPin{pin.revision, pin.hash});
    if (!catalog) return std::unexpected(StartupError{StartupErrorCode::CatalogRead, catalog.error(), catalog.error().sqlError});
    if (!*catalog || !SameIdentity(**catalog, pin)) return std::unexpected(Error(StartupErrorCode::PinMismatch));
    return *catalog;
}

std::expected<void, StartupError> Install(std::shared_ptr<const ItemCatalog> catalog, CatalogPin pin)
{
    if (!catalog || !pin.revision || !SameIdentity(*catalog, pin)) return std::unexpected(Error(StartupErrorCode::PinMismatch));
    std::lock_guard lock(installing);
    if (published.load(std::memory_order_acquire)) return std::unexpected(Error(StartupErrorCode::AlreadyInstalled));
    published.store(new Published{std::move(catalog), pin}, std::memory_order_release);
    return {};
}

std::shared_ptr<const ItemCatalog> Shared() noexcept
{
    const auto* current = published.load(std::memory_order_acquire);
    return current ? current->catalog : nullptr;
}

std::optional<CatalogPin> InstalledPin() noexcept
{
    const auto* current = published.load(std::memory_order_acquire);
    if (!current) return std::nullopt;
    return current->pin;
}

std::string HexDigest(const ItemCatalogDigest& digest)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string text;
    text.reserve(digest.size() * 2);
    for (const auto byte : digest)
    {
        text.push_back(digits[byte >> 4]);
        text.push_back(digits[byte & 15]);
    }
    return text;
}

std::string Describe(const StartupError& error)
{
    std::string text;
    switch (error.code)
    {
    case StartupErrorCode::PinTableMissing:
        text = "player.item_persistence_format is missing; install the item persistence schema (db/migrations/item_instance)";
        break;
    case StartupErrorCode::PinMissing:
        text = "player.item_persistence_format has no catalog pin row";
        break;
    case StartupErrorCode::PinInvalid:
        text = "player.item_persistence_format holds an invalid catalog pin";
        break;
    case StartupErrorCode::PinQueryFailed:
        text = "reading the catalog pin failed";
        break;
    case StartupErrorCode::CatalogConnection:
        text = "cannot open the dedicated protos catalog connection";
        break;
    case StartupErrorCode::CatalogSession:
        text = "cannot set the catalog session to REPEATABLE READ, READ ONLY";
        break;
    case StartupErrorCode::CatalogRead:
        text = "protos catalog rejected: ";
        text += error.read ? ReadCodeName(error.read->code) : "unknown read error";
        if (error.read && error.read->table)
        {
            const auto index = static_cast<std::size_t>(*error.read->table);
            if (index < sql::TableCount) text += " (table protos." + std::string(sql::Tables()[index].name) + ")";
        }
        if (error.read && error.read->catalogError)
        {
            const auto& invalid = *error.read->catalogError;
            text += ", catalog error " + std::to_string(static_cast<unsigned>(invalid.code));
            if (!invalid.field.empty()) text += " field " + invalid.field;
            if (invalid.key)
                text += " item " + std::to_string(static_cast<unsigned>(invalid.key->Category())) + ":" + std::to_string(invalid.key->LocalId());
            if (invalid.profileId) text += " profile " + std::to_string(invalid.profileId);
        }
        break;
    case StartupErrorCode::PinMismatch:
        text = "loaded catalog identity differs from the persistence pin";
        break;
    case StartupErrorCode::AlreadyInstalled:
        text = "an item catalog is already installed";
        break;
    }
    if (error.sqlError) text += " [SQL " + std::to_string(error.sqlError) + "]";
    return text;
}
} // namespace auriga::item::runtime
