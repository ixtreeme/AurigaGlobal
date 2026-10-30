#pragma once
// Test-only: writes a fixture catalog into an owned MariaDB protos schema and
// pins it for player.item_persistence_format. Never used by a production path.
#include "CatalogSnapshotFixture.hpp"
#include <CatalogSqlReader.hpp>
#include <SQL/AsyncSQL.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace catalog_sql_populate
{
namespace db = auriga::item::sql;

inline void Require(bool good, const char* why)
{
    if (!good) throw std::runtime_error(why);
}

inline void Populate(CAsyncSQL& admin, const auriga::item::ItemCatalogSource& source)
{
    const auto snapshot = catalog_snapshot_fixture::Snapshot(source);
    struct Pending { std::string query; SqlParams params; };
    std::vector<Pending> pending;
    for (const auto& spec : db::Tables())
    {
        // Installed by the schema drafts themselves.
        if (spec.table == db::Table::Format || spec.table == db::Table::Categories || spec.table == db::Table::BonusDefinitions) continue;
        for (const auto& row : snapshot[spec.table])
        {
            std::string marks;
            SqlParams params;
            for (const auto& cell : row)
            {
                if (!marks.empty()) marks += ",";
                marks += "?";
                params.push_back(cell ? SqlParam::Text(*cell) : SqlParam::Null());
            }
            pending.push_back({"INSERT INTO protos." + std::string(spec.name) + "(" + std::string(spec.columns) + ") VALUES(" + marks + ")", std::move(params)});
        }
    }
    // Fixture-only FK ordering with every FK still enabled.
    while (!pending.empty())
    {
        const auto old = pending.size();
        std::vector<Pending> next;
        for (auto& row : pending)
        {
            std::unique_ptr<SQLMsg> m(admin.DirectQueryPrepared(row.query.c_str(), row.params));
            Require(m != nullptr, "fixture message");
            if (m->uiSQLErrno)
            {
                Require(m->uiSQLErrno == 1452, "fixture error other than a missing parent");
                next.push_back(std::move(row));
            }
        }
        Require(next.size() < old, "fixture FK dependency cycle");
        pending = std::move(next);
    }
}

inline void Pin(CAsyncSQL& admin, std::uint64_t revision, const auriga::item::ItemCatalogDigest& hash)
{
    const std::string digest(reinterpret_cast<const char*>(hash.data()), hash.size());
    std::unique_ptr<SQLMsg> m(admin.DirectQueryPrepared("INSERT INTO player.item_persistence_format VALUES(1,1,?,?)",
                                                        {SqlParam::UInt(revision), SqlParam::Blob(digest)}));
    Require(m && !m->uiSQLErrno, "catalog pin insert failed");
}
} // namespace catalog_sql_populate
