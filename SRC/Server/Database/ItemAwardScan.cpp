#include "ItemAwardScan.hpp"
#include <SQL/AsyncSQL.h>
#include <charconv>
#include <string_view>

namespace auriga::item_award {
std::optional<std::string> Scan::Request(bool extendedAttributes) {
  if (pending_)
    return {};
  pending_ = true;
  extended_ = extendedAttributes;
  if (ceilingQuery_)
    return "SELECT COALESCE(MAX(id),0) FROM item_award";
  std::string query = "SELECT id,login,vnum,count,socket0,socket1,socket2,";
  if (extended_)
    for (unsigned int i = 0; i < 8; ++i)
      query += "attrtype" + std::to_string(i) + ",attrvalue" +
               std::to_string(i) + ',';
  query += "mall,why FROM item_award WHERE taken_time IS NULL AND id>" +
           std::to_string(cursor_) + " AND id<=" + std::to_string(ceiling_) +
           " ORDER BY id LIMIT " + std::to_string(PageSize);
  return query;
}
Scan::Result Scan::Complete(_SQLMsg &message) {
  if (!pending_)
    return {};
  pending_ = false;
  auto *result = message.Get();
  const auto fields = ceilingQuery_ ? 1u : (extended_ ? 25u : 9u);
  if (message.uiSQLErrno || !result || !result->pSQLResult ||
      mysql_num_fields(result->pSQLResult) != fields ||
      result->uiNumRows > (ceilingQuery_ ? 1u : PageSize) ||
      (ceilingQuery_ && result->uiNumRows != 1))
    return {};
  uint32_t last = cursor_;
  bool valid = true;
  for (uint64_t i = 0; i < result->uiNumRows; ++i) {
    auto row = mysql_fetch_row(result->pSQLResult);
    auto lengths = mysql_fetch_lengths(result->pSQLResult);
    if (!row || !lengths || !row[0]) {
      valid = false;
      break;
    }
    uint32_t id = 0;
    const auto parsed = std::from_chars(row[0], row[0] + lengths[0], id);
    if (parsed.ec != std::errc{} || parsed.ptr != row[0] + lengths[0] ||
        (!ceilingQuery_ && (id <= last || id > ceiling_))) {
      valid = false;
      break;
    }
    last = id;
  }
  mysql_data_seek(result->pSQLResult, 0);
  if (!valid)
    return {};
  if (ceilingQuery_) {
    ceiling_ = last;
    ceilingQuery_ = ceiling_ == 0;
    return {true, false, ceiling_ == 0};
  }
  cursor_ = last;
  const bool complete = result->uiNumRows < PageSize || cursor_ == ceiling_;
  if (complete) {
    cursor_ = ceiling_ = 0;
    ceilingQuery_ = true;
  }
  return {true, true, complete};
}
} // namespace auriga::item_award
