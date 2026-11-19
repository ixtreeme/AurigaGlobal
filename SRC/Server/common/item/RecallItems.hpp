#pragma once
#include "ItemKey.hpp"
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace auriga::item {
enum class RecallMode : std::uint8_t { None=0,Town=1,Memory=2 };
class ItemCatalog;
RecallMode RecallItemMode(const ItemCatalog&,ItemKey);
bool RecallItemSummonGates(const ItemCatalog&,ItemKey);
struct RecallPosition {
    std::int32_t map=0,x=0,y=0;
    bool operator==(const RecallPosition&) const=default;
};
// This is a concrete scroll's saved destination, never a client coordinate or
// a reinterpretation of another item's sockets. An unstamped scroll has None.
struct RecallItemState {
    std::optional<RecallPosition> destination;
    bool operator==(const RecallItemState&) const=default;
};
std::vector<std::uint8_t> EncodeRecallItemState(const RecallItemState&);
std::optional<RecallItemState> DecodeRecallItemState(std::span<const std::uint8_t>);
}
