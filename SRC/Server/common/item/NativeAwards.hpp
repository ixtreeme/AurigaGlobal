#pragma once
#include "ItemKey.hpp"
#include <cstdint>

namespace auriga::item
{
class ItemCatalog;
struct ItemCommit;
struct NativeAwardClaim
{
    std::uint64_t id=0, account=0;
    ItemKey key;
    std::uint16_t level=0;
    std::uint32_t count=0;
    bool operator==(const NativeAwardClaim&) const = default;
};
bool ValidNativeAwardClaim(const ItemCatalog&, const ItemCommit&);
}
