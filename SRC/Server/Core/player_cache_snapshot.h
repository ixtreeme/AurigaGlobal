#pragma once
#include "player_cache_protocol.h"
#include <common/tables.h>

namespace auriga::player_cache_snapshot
{
inline constexpr size_t MaximumItems = 1024;
struct Snapshot
{
    uint32_t account = 0;
    TPlayerTable player{};
    std::vector<TPlayerItem> items;
};
// Persisted positions, after the legacy equipment/belt normalization.
bool ValidItem(const TPlayerItem &item, uint32_t character);
// Owned legacy-ABI frames: player save, every item save, then Begin. This is
// neither a source-stop receipt nor an atomic Database transaction.
std::optional<std::vector<uint8_t>> Batch(const player_session::Key &key, player_session::Token challenge,
                                          uint32_t descriptor, const Snapshot &snapshot);
} // namespace auriga::player_cache_snapshot
