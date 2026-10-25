#pragma once
#include <cstdint>
#include <optional>
#include <string>

struct _SQLMsg;
namespace auriga::item_award {
// One outstanding query, bounded pages and a fixed high ID for each sweep.
// A later-committing lower ID is discovered on the next sweep. Continuous new
// inserts cannot keep an old sweep from wrapping back to zero.
class Scan {
public:
  static constexpr uint32_t PageSize = 256;
  struct Result {
    bool success = false, page = false, sweepComplete = false;
  };
  std::optional<std::string> Request(bool extendedAttributes);
  // Validates the whole page before advancing. Rewinds the result for the
  // production cache decoder. Errors retry the same cursor, never skip rows.
  Result Complete(_SQLMsg &message);

private:
  uint32_t cursor_ = 0, ceiling_ = 0;
  bool ceilingQuery_ = true, pending_ = false, extended_ = false;
};
} // namespace auriga::item_award
