#pragma once
#include "ItemKey.hpp"
#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>

namespace auriga::item
{
enum class SocketStoneKind : std::uint8_t { Insertable = 1, BrokenShard = 2 };
enum class StoneSocketState : std::uint8_t { Closed = 1, Empty = 2, Occupied = 3, Broken = 4 };
inline constexpr std::size_t EquipmentStoneSockets = 3;
struct ItemSocketStoneRow
{
    ItemKey key;
    SocketStoneKind kind = SocketStoneKind::Insertable;
    std::uint8_t allowedTargets = 0; // Weapon=1, body armor=2. Not an equip slot.
    std::uint8_t minimumSocketGrade = 0; // Open normal=1, gold=2.
    std::string primaryGroup, secondaryGroup;
};
bool ValidSocketStone(const ItemSocketStoneRow&) noexcept;
struct StoneSocketSlot
{
    StoneSocketState state = StoneSocketState::Closed;
    std::uint8_t grade = 0;
    std::optional<ItemKey> stone;
    std::uint16_t refinementLevel = 0;
    bool operator==(const StoneSocketSlot&) const = default;
};
struct StoneItemSnapshot
{
    ItemKey item;
    std::uint64_t instanceId = 0, revision = 0, ownerId = 0;
    std::uint16_t refinementLevel = 0;
    std::uint32_t count = 0;
    bool locked = false, equipped = false, exchanging = false;
};
struct SocketStoneInsertRequest
{
    StoneItemSnapshot stone, target;
    std::array<StoneSocketSlot,EquipmentStoneSockets> sockets{};
    std::uint64_t actorId = 0, rulesetRevision = 0;
    std::array<std::uint8_t,16> operationId{};
    std::optional<std::uint16_t> chanceBp; // Server rules; no implicit 30%.
    std::uint16_t recordedRoll = 0, characterLevel = 0;
    bool inCombat = false, admittedByServer = false;
};
struct SocketStoneInsertPlan
{
    SocketStoneInsertRequest expected;
    std::uint64_t catalogRevision = 0;
    std::array<std::uint8_t,32> catalogHash{};
    std::uint8_t slot = 0;
    StoneSocketSlot after;
    std::uint16_t consumeUnits = 1;
    bool success = false;
};
enum class SocketStoneError
{
    Forbidden, InvalidOperation, InvalidSnapshot, MissingItem, NotStone, RetiredItem,
    InvalidStack, InvalidLevel, IncompatibleTarget, InvalidSocketState,
    ConflictingGroup, NoEmptySocket, InvalidDecision,
};
class ItemCatalog;
// No registry, SQL, Lua, RNG or mutation. The owner must bind the session and
// class/sex checks, then atomically receipt+consume+replace the exact slot state.
// CommitUnknown requires operation-ID reconciliation before retrying.
std::expected<SocketStoneInsertPlan,SocketStoneError> PrepareSocketStoneInsert(
    const ItemCatalog&,const SocketStoneInsertRequest&);
} // namespace auriga::item
