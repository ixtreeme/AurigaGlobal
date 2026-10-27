#include <Core/player_cache_snapshot.h>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace snapshot = auriga::player_cache_snapshot;
namespace session = auriga::player_session;
namespace wire = auriga::player_cache_wire;
namespace
{
int checks = 0;
void Check(bool value, const char *why)
{
    ++checks;
    if (!value)
        throw std::runtime_error(why);
}
session::Token Token(uint8_t n)
{
    session::Token token{};
    token.fill(n);
    return token;
}
session::Key Key()
{
    return {Token(1), Token(2), Token(3), 42, 900, 100};
}
snapshot::Snapshot State()
{
    snapshot::Snapshot state;
    state.player.id = 100;
    state.account = 900;
    state.player.gold = 500;
    state.player.exp = 123456;
    for (uint16_t pos = 0; pos < 2; ++pos)
    {
        TPlayerItem item{};
        item.id = 200 + pos;
        item.owner = 100;
        item.window = INVENTORY;
        item.pos = pos;
        item.vnum = 19;
        item.count = 3;
        item.alSockets[0] = 123;
        item.aAttr[0] = {1, 25};
        state.items.push_back(item);
    }
    return state;
}
void Frames(const snapshot::Snapshot &state)
{
    const auto batch = snapshot::Batch(Key(), Token(4), 77, state);
    Check(batch.has_value(), "valid native snapshot rejected");
    const auto owned = *batch;
    size_t at = 0, frames = 0;
    const auto integer = [&]()
    {
        Check(at + 4 <= owned.size(), "truncated frame header");
        uint32_t value = 0;
        for (int shift = 0; shift < 32; shift += 8)
            value |= uint32_t(owned[at++]) << shift;
        return value;
    };
    while (at < owned.size())
    {
        const auto header = owned[at++];
        const auto descriptor = integer(), size = integer();
        Check(size <= owned.size() - at, "frame body outside batch");
        if (frames == 0)
            Check(header == HEADER_GD_PLAYER_SAVE && descriptor == 77 && size == sizeof(TPlayerTable) &&
                      std::memcmp(owned.data() + at, &state.player, size) == 0,
                  "player row was not the first exact legacy save");
        else if (frames <= state.items.size())
            Check(header == HEADER_GD_ITEM_SAVE && descriptor == 0 && size == sizeof(TPlayerItem) &&
                      std::memcmp(owned.data() + at, &state.items[frames - 1], size) == 0,
                  "item save owner/position/socket/attribute changed");
        else
        {
            const auto request = wire::DecodeRequest({owned.data() + at, size});
            Check(header == HEADER_GD_PLAYER_CACHE_REQUEST && descriptor == 77 && request &&
                      request->key == Key() && request->challenge == Token(4) &&
                      request->action == wire::Action::Begin && at + size == owned.size(),
                  "drain Begin was not scoped and last");
        }
        at += size;
        ++frames;
    }
    Check(frames == state.items.size() + 2, "snapshot frame missing or repeated");
}
void Invalid()
{
    const auto state = State();
    for (int scenario = 0; scenario < 13; ++scenario)
    {
        auto key = Key();
        auto challenge = Token(4);
        uint32_t descriptor = 77;
        auto bad = state;
        switch (scenario)
        {
        case 0:
            key.link = {};
            break;
        case 1:
            key.request = {};
            break;
        case 2:
            key.session = {};
            break;
        case 3:
            key.generation = 0;
            break;
        case 4:
            key.account = 0;
            break;
        case 5:
            key.character = 0;
            break;
        case 6:
            challenge = {};
            break;
        case 7:
            descriptor = 0;
            break;
        case 8:
            ++bad.player.id;
            break;
        case 9:
            ++bad.account;
            break;
        case 10:
            ++bad.items[0].owner;
            break;
        case 11:
            bad.items[1].id = bad.items[0].id;
            break;
        case 12:
            bad.items[1].pos = bad.items[0].pos;
            break;
        }
        Check(!snapshot::Batch(key, challenge, descriptor, bad),
              "invalid identity/ownership/duplicate accepted");
    }
    for (uint8_t window :
         {uint8_t(INVENTORY), uint8_t(EQUIPMENT), uint8_t(BELT_INVENTORY)
#ifdef ENABLE_EXTRA_INVENTORY
                                                                               ,
          uint8_t(EXTRA_INVENTORY)
#endif
#ifdef ENABLE_SWITCHBOT
              ,
          uint8_t(SWITCHBOT)
#endif
         })
    {
        auto item = state.items.front();
        item.window = window;
        uint16_t firstInvalid = 0;
        while (snapshot::ValidItem(item, 100))
            item.pos = ++firstInvalid;
        Check(firstInvalid > 0 && !snapshot::ValidItem(item, 100), "window boundary not enforced");
        item.pos = firstInvalid - 1;
        Check(snapshot::ValidItem(item, 100), "last valid persisted position rejected");
    }
    for (uint16_t cell : {uint16_t(0), uint16_t(1), uint16_t(100), uint16_t(UINT16_MAX)})
    {
        auto item = state.items.front();
        item.window = RETIRED_WINDOW_6;
        item.pos = cell;
        Check(!snapshot::ValidItem(item, 100), "retired window persisted in a player snapshot");
    }
    {
        auto item = state.items.front();
        item.window = EQUIPMENT;
        item.pos = WEAR_MAX_NUM;
        Check(!snapshot::ValidItem(item, 100), "retired equipment slot persisted");
    }
    for (int window = 0; window < 256; ++window)
    {
        auto item = state.items.front();
        item.window = uint8_t(window);
        item.pos = UINT16_MAX;
        Check(!snapshot::ValidItem(item, 100), "invalid item window/cell accepted");
    }
    for (uint8_t window : {uint8_t(SAFEBOX), uint8_t(MALL), uint8_t(MOUNT_INVENTORY), uint8_t(GROUND),
                           uint8_t(RESERVED_WINDOW)})
    {
        auto bad = state;
        bad.items[0].window = window;
        Check(!snapshot::Batch(Key(), Token(4), 77, bad), "non-character storage accepted");
    }
    for (int scenario = 0; scenario < 3; ++scenario)
    {
        auto bad = state;
        if (scenario == 0)
            bad.items[0].id = 0;
        if (scenario == 1)
            bad.items[0].vnum = 0;
        if (scenario == 2)
            bad.items[0].count = 0;
        Check(!snapshot::Batch(Key(), Token(4), 77, bad), "invalid persistent item accepted");
    }
}
void Capacity()
{
    auto state = State();
    state.items.clear();
    Frames(state);
    for (uint16_t pos = 0; pos < EXTRA_INVENTORY_MAX_NUM && state.items.size() < snapshot::MaximumItems;
         ++pos)
    {
        auto item = State().items.front();
        item.id += pos;
        item.window = EXTRA_INVENTORY;
        item.pos = pos;
        state.items.push_back(item);
    }
    Check(state.items.size() == snapshot::MaximumItems, "capacity fixture does not reach limit");
    Frames(state);
    state.items.push_back(state.items.front());
    Check(!snapshot::Batch(Key(), Token(4), 77, state), "oversized snapshot accepted");
    auto before = State();
    auto batch = snapshot::Batch(Key(), Token(4), 77, before);
    const auto bytes = *batch;
    before.player.gold = 0;
    before.items.clear();
    Check(*batch == bytes, "snapshot batch retained borrowed ECS storage");
}
} // namespace
int main()
{
    try
    {
        Frames(State());
        Invalid();
        Capacity();
        std::cout << "Player cache snapshot checks passed: " << checks << '\n';
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
