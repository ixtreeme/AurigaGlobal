#pragma once
#include <SQL/PreparedTransaction.h>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct _SQLMsg;

namespace auriga::item_award
{
struct Attribute
{
    uint8_t type = 0;
    int16_t value = 0;
    bool operator==(const Attribute &) const = default;
};
// The unmodified cached award, before trusted prototype/time/random normalization.
struct Source
{
    uint32_t id = 0, vnum = 0, count = 0;
    std::string login;
    std::array<uint32_t, 3> sockets{};
    std::array<Attribute, 8> attributes{};
    bool mall = false;
};
struct Item
{
    // Reserved ONCE by the Database process's existing item-ID range allocator.
    // Never MAX(id)+1, an auto-increment substitute or a GameServer-owned ID.
    uint32_t id = 0, account = 0, vnum = 0, count = 0;
    uint16_t position = 0;
    std::array<int32_t, 3> sockets{};
    std::array<Attribute, 8> attributes{};
};
struct Placement
{
    uint32_t id = 0, vnum = 0;
    uint16_t position = 0;
    uint8_t height = 0;
};
struct Request
{
    Source source;
    Item item;
    // Complete snapshot of this closed account container, including earlier
    // confirmed deliveries in the current load. Heights come from trusted protos.
    std::vector<Placement> storage;
    uint16_t rows = 0;
    uint8_t itemHeight = 0;
};
struct Plan
{
    SqlTransaction statements;
    std::vector<std::string> errors;
    explicit operator bool() const
    {
        return errors.empty() && !statements.empty();
    }
};
// Runs on the sole authoritative Database service, while this account container
// is closed. Locks account, then award, then verifies the complete placement
// snapshot. Insert + taken receipt are ONE owned transaction. No ECS mutation.
Plan Deliver(const Request &request, const std::string &tablePostfix = {});
bool Confirmed(const _SQLMsg &message);

enum class ReceiptState
{
    Pending,
    Taken,
    Missing,
    Invalid,
    SqlError
};
struct Receipt
{
    ReceiptState state = ReceiptState::Invalid;
    uint32_t item = 0;
};
// After any non-confirmed result: current locking read on a fresh/ready
// connection, waiting for the original award writer. Never blindly replay an
// unknown COMMIT or synthesize an inventory item from this historical receipt.
SqlTransaction Resolve(const Source &source, const std::string &tablePostfix = {});
Receipt DecodeReceipt(_SQLMsg &message);
} // namespace auriga::item_award
