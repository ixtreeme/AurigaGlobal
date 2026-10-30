// F4.1 (IC-034): GameServer startup catalog. --completion-only runs the pure
// pin decoder and the one-time publication; --owned-sandbox runs the full
// startup read against a new owned MariaDB (see ItemCatalogRuntimeTests.py).
#include "CatalogSqlPopulate.hpp"
#include "../../SRC/Server/GameServer/item/ItemCatalogRuntime.hpp"
#include <CatalogSqlReader.hpp>
#include <SQL/AsyncSQL.h>
#include <Core/Logging.hpp>
#include <iostream>
#include <source_location>
#include <stdexcept>

using namespace auriga::item;
namespace rt = auriga::item::runtime;
namespace s = auriga::item::sql;
using U = SqlParam;
using Message = std::unique_ptr<SQLMsg>;
using C = rt::StartupErrorCode;

namespace
{
unsigned checks = 0;
void Check(bool good, const char* why, std::source_location at = std::source_location::current())
{
    ++checks;
    if (!good) throw std::runtime_error(std::string(why) + " at " + std::to_string(at.line()));
}
template <class T> bool Fails(const T& result, C code)
{
    if (result) return false;
    if (result.error().code != code)
        std::cerr << "startup code=" << static_cast<int>(result.error().code) << " want=" << static_cast<int>(code)
                  << " text=" << rt::Describe(result.error()) << '\n';
    return result.error().code == code;
}
bool ReadFails(const std::expected<std::shared_ptr<const ItemCatalog>, rt::StartupError>& result, s::ReadErrorCode code)
{
    if (!Fails(result, C::CatalogRead) || !result.error().read) return false;
    if (result.error().read->code != code)
        std::cerr << "read code=" << static_cast<int>(result.error().read->code) << " want=" << static_cast<int>(code) << '\n';
    return result.error().read->code == code;
}
std::string Bytes(const ItemCatalogDigest& digest) { return {reinterpret_cast<const char*>(digest.data()), digest.size()}; }

// A completed prepared result exactly as CAsyncSQL materializes it.
Message Result(const std::vector<std::vector<std::optional<std::string>>>& rows)
{
    auto message = std::make_unique<SQLMsg>();
    message->bPrepared = true;
    auto result = std::make_unique<SQLResult>();
    result->bPrepared = true;
    result->uiNumRows = rows.size();
    for (const auto& row : rows)
    {
        std::vector<char*> cells;
        std::vector<unsigned long> lengths;
        for (const auto& cell : row)
        {
            if (cell)
            {
                result->preparedCells.push_back(*cell);
                cells.push_back(result->preparedCells.back().data());
                lengths.push_back(static_cast<unsigned long>(cell->size()));
            }
            else
            {
                cells.push_back(nullptr);
                lengths.push_back(0);
            }
        }
        result->preparedRows.push_back(std::move(cells));
        result->preparedLengths.push_back(std::move(lengths));
    }
    message->vec_pkResult.push_back(result.release());
    return message;
}
std::string Digest()
{
    // Embedded NUL and high bytes must survive exactly.
    std::string digest(32, '\0');
    for (std::size_t i = 0; i < digest.size(); ++i) digest[i] = static_cast<char>((i * 37 + 200) & 255);
    digest[5] = '\0';
    return digest;
}

void DecodeBoundary()
{
    Check(Fails(rt::DecodePin(nullptr), C::PinQueryFailed), "missing SQL message accepted");
    for (const auto [errorNumber, code] : {std::pair{1146u, C::PinTableMissing}, std::pair{1049u, C::PinTableMissing}, std::pair{2013u, C::PinQueryFailed}})
    {
        auto failed = Result({});
        failed->uiSQLErrno = errorNumber;
        const auto pin = rt::DecodePin(failed.get());
        Check(Fails(pin, code) && pin.error().sqlError == errorNumber, "SQL failure misclassified");
    }
    Check(Fails(rt::DecodePin(Result({}).get()), C::PinMissing), "absent pin row accepted");

    const auto digest = Digest();
    const auto good = rt::DecodePin(Result({{"1", "42", digest}}).get());
    Check(good && good->revision == 42 && Bytes(good->hash) == digest, "valid pin lost its revision or exact digest");
    Check(rt::DecodePin(Result({{"1", "18446744073709551615", digest}}).get())->revision == UINT64_MAX, "maximum revision rejected");

    for (const auto& row : std::vector<std::vector<std::optional<std::string>>>{
             {"2", "42", digest}, {"0", "42", digest}, {"1", "0", digest}, {"1", "-1", digest}, {"1", "+42", digest},
             {"1", " 42", digest}, {"1", "42x", digest}, {"1", "", digest}, {"1", "18446744073709551616", digest},
             {"1", "42", std::nullopt}, {"1", "42", digest.substr(1)}, {"1", "42", digest + "x"},
             {std::nullopt, "42", digest}, {"1", std::nullopt, digest}})
        Check(Fails(rt::DecodePin(Result({row}).get()), C::PinInvalid), "malformed pin accepted");
    Check(Fails(rt::DecodePin(Result({{"1", "42", digest}, {"1", "43", digest}}).get()), C::PinInvalid), "two pins accepted");
    Check(Fails(rt::DecodePin(Result({{"1", "42"}}).get()), C::PinQueryFailed), "wrong column count accepted");

    auto unprepared = Result({{"1", "42", digest}});
    unprepared->vec_pkResult.front()->bPrepared = false;
    Check(Fails(rt::DecodePin(unprepared.get()), C::PinQueryFailed), "text-protocol result accepted for a binary digest");
    auto noResult = Result({});
    for (auto* result : noResult->vec_pkResult) delete result;
    noResult->vec_pkResult.clear();
    Check(Fails(rt::DecodePin(noResult.get()), C::PinQueryFailed), "missing result set accepted");
    auto miscounted = Result({{"1", "42", digest}});
    miscounted->vec_pkResult.front()->uiNumRows = 2;
    Check(Fails(rt::DecodePin(miscounted.get()), C::PinQueryFailed), "row count mismatch accepted");
}

void DescribeBoundary()
{
    for (const auto code : {C::PinTableMissing, C::PinMissing, C::PinInvalid, C::PinQueryFailed, C::CatalogConnection,
                            C::CatalogSession, C::CatalogRead, C::PinMismatch, C::AlreadyInstalled})
        Check(!rt::Describe({code}).empty(), "startup error without text");
    rt::StartupError read{C::CatalogRead, s::ReadError{s::ReadErrorCode::HashMismatch, s::Table::Items}, 1205};
    const auto text = rt::Describe(read);
    Check(text.find("pinned hash") != std::string::npos && text.find("protos.item_base") != std::string::npos &&
          text.find("SQL 1205") != std::string::npos, "catalog read diagnostic lost its reason, table or SQL code");
    Check(rt::HexDigest(ItemCatalogDigest{0x00, 0xab, 0x10}).starts_with("00ab10") && rt::HexDigest({}).size() == 64,
          "digest hex form changed");
}

void InstallBoundary(std::shared_ptr<const ItemCatalog> catalog, rt::CatalogPin pin)
{
    Check(!rt::Shared() && !rt::InstalledPin(), "catalog visible before installation");
    Check(Fails(rt::Install(nullptr, pin), C::PinMismatch), "null catalog installed");
    Check(Fails(rt::Install(catalog, {pin.revision + 1, pin.hash}), C::PinMismatch), "catalog installed under another revision");
    auto wrong = pin;
    wrong.hash[0] ^= 1;
    Check(Fails(rt::Install(catalog, wrong), C::PinMismatch), "catalog installed under another hash");
    Check(Fails(rt::Install(catalog, {0, pin.hash}), C::PinMismatch), "zero revision installed");
    Check(!rt::Shared(), "rejected installation became visible");
    Check(rt::Install(catalog, pin).has_value(), "pinned catalog not installed");
    Check(rt::Shared() == catalog && rt::InstalledPin() == pin, "installed catalog or pin differs");
    Check(Fails(rt::Install(catalog, pin), C::AlreadyInstalled), "catalog replaced without restart");
    Check(rt::Shared() == catalog, "failed second installation changed the published catalog");
}

// --- owned MariaDB -------------------------------------------------------
Message Query(CAsyncSQL& db, const std::string& query, const SqlParams& params = {})
{
    Message m(db.DirectQueryPrepared(query.c_str(), params));
    if (m && m->uiSQLErrno) std::cerr << "SQL fixture error=" << m->uiSQLErrno << " query=" << query << '\n';
    Check(m && !m->uiSQLErrno && m->Get(), "isolated query failed");
    return m;
}
std::string Scalar(CAsyncSQL& db, const std::string& query, const SqlParams& params = {})
{
    auto m = Query(db, query, params);
    auto row = m->Get()->FetchRow();
    auto lengths = m->Get()->FetchLengths();
    Check(row && row[0] && lengths, "scalar missing");
    return {row[0], lengths[0]};
}
void Execute(CAsyncSQL& db, const std::string& query, const SqlParams& params = {}) { Query(db, query, params); }
void Connect(CAsyncSQL& db, unsigned port, const std::string& pw, const char* user, const char* schema)
{
    db.SetTimeouts(5);
    db.SetRetryConnectionErrors(false);
    Check(db.Setup("127.0.0.1", user, pw.c_str(), schema, "utf8mb4", true, port), "owned SQL connection");
}
using catalog_sql_populate::Populate;
std::expected<rt::CatalogPin, rt::StartupError> ReadPin(CAsyncSQL& player)
{
    Message message(player.DirectQueryPrepared(rt::PinQuery, {}));
    return rt::DecodePin(message.get());
}

void OwnedSandbox(unsigned port, const std::string& pw, const std::string& token)
{
    CAsyncSQL admin, player, reader;
    Connect(admin, port, pw, "root", "player");
    Check(Scalar(admin, "SELECT token FROM protos.ic_runtime_owner") == token, "owned sandbox identity");
    Connect(player, port, pw, "item_runtime", "player");
    Connect(reader, port, pw, "catalog_reader", "protos");

    const auto reference = BuildItemCatalog(item_catalog_fixture::Catalog()).value();
    Populate(admin, item_catalog_fixture::Catalog());
    constexpr std::uint64_t PinnedRevision = 31;
    const auto digest = std::string(reinterpret_cast<const char*>(reference->ContentHash().data()), reference->ContentHash().size());

    // Schema installed but never pinned: refuse, never pick "the current" catalog.
    Check(Fails(ReadPin(player), C::PinMissing), "unpinned persistence schema accepted");
    Execute(admin, "INSERT INTO player.item_persistence_format VALUES(1,1,?,?)", {U::UInt(PinnedRevision), U::Blob(digest)});

    // Startup path: pin from player, catalog from the dedicated protos session.
    const auto pin = ReadPin(player);
    Check(pin && pin->revision == PinnedRevision && pin->hash == reference->ContentHash(), "pin read from SQL differs");
    Execute(reader, "SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED");
    auto loaded = rt::LoadPinnedCatalog(reader, *pin);
    if (!loaded) std::cerr << rt::Describe(loaded.error()) << '\n';
    Check(loaded && (*loaded)->Size() == item_catalog_fixture::FixtureItems && (*loaded)->Revision() == PinnedRevision &&
          (*loaded)->ContentHash() == reference->ContentHash(), "pinned SQL catalog identity differs");
    Check(Scalar(reader, "SELECT @@tx_isolation") == "REPEATABLE-READ" && Scalar(reader, "SELECT @@session.tx_read_only") == "1" &&
          Scalar(reader, "SELECT @@autocommit") == "1", "catalog session not fixed to RR/read-only or left a transaction open");
    Message forbidden(reader.DirectQueryPrepared("UPDATE protos.item_base SET sell_amount=sell_amount+1", {}));
    Check(forbidden && forbidden->uiSQLErrno != 0, "catalog session could write the catalog");

    InstallBoundary(*loaded, *pin);

    // The session guard holds even when the configured account could write.
    CAsyncSQL broad;
    Connect(broad, port, pw, "root", "protos");
    Check(rt::LoadPinnedCatalog(broad, *pin).has_value(), "broad account could not load the pinned catalog");
    Message blocked(broad.DirectQueryPrepared("INSERT INTO protos.ic_runtime_owner VALUES('intruder000000000000000000000000')", {}));
    Check(blocked && blocked->uiSQLErrno == 1792, "read-only catalog session accepted a write");
    Check(Scalar(admin, "SELECT COUNT(*) FROM protos.ic_runtime_owner") == "1", "blocked write changed the sandbox");

    // Content changed after the pin: refuse instead of running an unpinned catalog.
    const auto first = reference->Entries().begin()->first;
    const SqlParams key{U::UInt(static_cast<unsigned>(first.Category())), U::UInt(first.LocalId())};
    Execute(admin, "UPDATE protos.item_base SET sell_amount=sell_amount+1 WHERE category_id=? AND local_id=?", key);
    Check(ReadFails(rt::LoadPinnedCatalog(reader, *pin), s::ReadErrorCode::HashMismatch), "catalog edited after the pin accepted");
    Execute(admin, "UPDATE protos.item_base SET sell_amount=sell_amount-1 WHERE category_id=? AND local_id=?", key);
    Check(rt::LoadPinnedCatalog(reader, *pin).has_value(), "restored catalog rejected");

    // A pin that names another catalog.
    auto other = digest;
    other[0] = static_cast<char>(other[0] ^ 1);
    Execute(admin, "UPDATE player.item_persistence_format SET catalog_sha256=? WHERE singleton=1", {U::Blob(other)});
    const auto otherPin = ReadPin(player);
    Check(otherPin && ReadFails(rt::LoadPinnedCatalog(reader, *otherPin), s::ReadErrorCode::HashMismatch), "foreign pin accepted");
    Execute(admin, "UPDATE player.item_persistence_format SET catalog_sha256=? WHERE singleton=1", {U::Blob(digest)});

    // A catalog built for another schema version.
    Execute(admin, "UPDATE protos.catalog_format SET schema_version=?", {U::UInt(s::CatalogSchemaVersion + 1)});
    Check(ReadFails(rt::LoadPinnedCatalog(reader, *pin), s::ReadErrorCode::FormatMismatch), "catalog format of another build accepted");
    Execute(admin, "UPDATE protos.catalog_format SET schema_version=?", {U::UInt(s::CatalogSchemaVersion)});

    // Persistence schema absent.
    Execute(admin, "RENAME TABLE player.item_persistence_format TO player.ic_runtime_hidden_format");
    const auto missing = ReadPin(player);
    Check(Fails(missing, C::PinTableMissing) && missing.error().sqlError == 1146, "missing persistence schema misclassified");
    Execute(admin, "RENAME TABLE player.ic_runtime_hidden_format TO player.item_persistence_format");
    const auto restored = ReadPin(player);
    Check(restored && *restored == *pin, "restored pin differs");
    Check(rt::Shared() && rt::Shared()->ContentHash() == reference->ContentHash(), "published catalog changed by later reads");
}
} // namespace

std::shared_ptr<spdlog::logger> logging::GetLogger()
{
    static auto logger = std::make_shared<spdlog::logger>("item-catalog-runtime-tests");
    return logger;
}
std::shared_ptr<spdlog::logger> logging::GetErrorLogger() { return logging::GetLogger(); }

int main(int argc, char** argv)
{
    try
    {
        if (argc != 2) throw std::runtime_error("expected --completion-only or --owned-sandbox");
        const std::string_view mode = argv[1];
        DecodeBoundary();
        DescribeBoundary();
        if (mode == "--owned-sandbox")
        {
            std::string portText, pw, token;
            std::getline(std::cin, portText);
            std::getline(std::cin, pw);
            std::getline(std::cin, token);
            Check(token.size() == 32, "owned token required");
            OwnedSandbox(static_cast<unsigned>(std::stoul(portText)), pw, token);
        }
        else
        {
            Check(mode == "--completion-only", "unknown test mode");
            const auto catalog = BuildItemCatalog(item_catalog_fixture::Catalog()).value();
            InstallBoundary(catalog, {catalog->Revision(), catalog->ContentHash()});
        }
        std::cout << "ItemCatalogRuntimeTests passed " << checks << " checks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "ItemCatalogRuntimeTests failed: " << error.what() << '\n';
        return 1;
    }
}
