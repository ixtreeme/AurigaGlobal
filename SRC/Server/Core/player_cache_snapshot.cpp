#include "player_cache_snapshot.h"

namespace auriga::player_cache_snapshot
{
std::optional<std::vector<uint8_t>> Batch(const player_session::Key &key, player_session::Token challenge,
                                          uint32_t descriptor, const Snapshot &snapshot)
{
    if (!key.Versioned() || !player_session::Nonzero(key.session) || !key.generation || !key.account ||
        !key.character || !player_session::Nonzero(challenge) || !descriptor ||
        snapshot.player.id != key.character || snapshot.account != key.account)
        return {};
    std::vector<uint8_t> bytes;
    bytes.reserve(9 + sizeof(TPlayerTable) + 9 +
                  player_cache_wire::RequestBytes);
    const auto frame = [&](uint8_t header, uint32_t handle, const void *body, uint32_t size)
    {
        bytes.push_back(header);
        for (auto value : {handle, size})
            for (int shift = 0; shift < 32; shift += 8)
                bytes.push_back(static_cast<uint8_t>(value >> shift));
        const auto *data = static_cast<const uint8_t *>(body);
        bytes.insert(bytes.end(), data, data + size);
    };
    frame(HEADER_GD_PLAYER_SAVE, descriptor, &snapshot.player, sizeof(TPlayerTable));
    const auto request = player_cache_wire::Encode({key, challenge, player_cache_wire::Action::Begin});
    if (request.size() != player_cache_wire::RequestBytes)
        return {};
    frame(HEADER_GD_PLAYER_CACHE_REQUEST, descriptor, request.data(), request.size());
    return bytes;
}
} // namespace auriga::player_cache_snapshot
