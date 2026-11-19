#pragma once
#include "player_cache_protocol.h"
#include <common/tables.h>

namespace auriga::player_cache_snapshot
{
struct Snapshot
{
    uint32_t account = 0;
    TPlayerTable player{};
};
// Player save followed by Begin. Native items use their own fenced SQL writer;
// they must never be serialized into a second Database item cache.
std::optional<std::vector<uint8_t>> Batch(const player_session::Key &key, player_session::Token challenge,
                                          uint32_t descriptor, const Snapshot &snapshot);
} // namespace auriga::player_cache_snapshot
