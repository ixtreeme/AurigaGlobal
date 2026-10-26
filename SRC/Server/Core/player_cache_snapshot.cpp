#include "player_cache_snapshot.h"
#include <set>

namespace auriga::player_cache_snapshot
{
bool ValidItem(const TPlayerItem &item, uint32_t character)
{
    if (!character || !item.id || item.owner != character || !item.vnum || !item.count)
        return false;
    switch (item.window)
    {
    case INVENTORY:
        return item.pos < INVENTORY_MAX_NUM;
    case EQUIPMENT:
        return item.pos < DRAGON_SOUL_EQUIP_RESERVED_SLOT_END - INVENTORY_MAX_NUM;
    case BELT_INVENTORY:
        return item.pos < BELT_INVENTORY_SLOT_COUNT;
    case DRAGON_SOUL_INVENTORY:
        return item.pos < DRAGON_SOUL_INVENTORY_MAX_NUM;
#ifdef ENABLE_EXTRA_INVENTORY
    case EXTRA_INVENTORY:
        return item.pos < EXTRA_INVENTORY_MAX_NUM;
#endif
#ifdef ENABLE_SWITCHBOT
    case SWITCHBOT:
        return item.pos < SWITCHBOT_SLOT_COUNT;
#endif
    default:
        return false;
    }
}
std::optional<std::vector<uint8_t>> Batch(const player_session::Key &key, player_session::Token challenge,
                                          uint32_t descriptor, const Snapshot &snapshot)
{
    if (!key.Versioned() || !player_session::Nonzero(key.session) || !key.generation || !key.account ||
        !key.character || !player_session::Nonzero(challenge) || !descriptor ||
        snapshot.player.id != key.character || snapshot.account != key.account ||
        snapshot.items.size() > MaximumItems)
        return {};
    std::set<uint32_t> ids;
    std::set<std::pair<uint8_t, uint16_t>> positions;
    for (const auto &item : snapshot.items)
        if (!ValidItem(item, key.character) || !ids.insert(item.id).second ||
            !positions.emplace(item.window, item.pos).second)
            return {};
    std::vector<uint8_t> bytes;
    bytes.reserve(9 + sizeof(TPlayerTable) + (9 + sizeof(TPlayerItem)) * snapshot.items.size() + 9 +
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
    for (const auto &item : snapshot.items)
        frame(HEADER_GD_ITEM_SAVE, 0, &item, sizeof(TPlayerItem));
    const auto request = player_cache_wire::Encode({key, challenge, player_cache_wire::Action::Begin});
    if (request.size() != player_cache_wire::RequestBytes)
        return {};
    frame(HEADER_GD_PLAYER_CACHE_REQUEST, descriptor, request.data(), request.size());
    return bytes;
}
} // namespace auriga::player_cache_snapshot
