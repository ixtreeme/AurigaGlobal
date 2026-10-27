#pragma once
#include "CatalogSnapshot.hpp"
#include <SQL/PreparedTransaction.h>
class CAsyncSQL;
struct _SQLMsg;
namespace auriga::item::sql
{
inline constexpr std::size_t ReadGuardSteps = 4;
// One owned prepared transaction, fixed SELECTs only. Requires a dedicated
// direct connection with REPEATABLE READ and a SELECT-only database account.
SqlTransaction PrepareRead();
struct CapturedCatalog
{
    Snapshot rows;
    std::shared_ptr<const ItemCatalog> catalog;
};
std::expected<CapturedCatalog, ReadError> CaptureCompleted(_SQLMsg& message, ReadPin pin);
// No results are exposed until a confirmed commit. Also useful for consumers
// of the existing async SQL queue: call after that queue returns its owned job.
std::expected<std::shared_ptr<const ItemCatalog>, ReadError> ReadCompleted(_SQLMsg& message, ReadPin pin);
std::expected<std::shared_ptr<const ItemCatalog>, ReadError> ReadCatalog(CAsyncSQL& connection, ReadPin pin);
} // namespace auriga::item::sql
