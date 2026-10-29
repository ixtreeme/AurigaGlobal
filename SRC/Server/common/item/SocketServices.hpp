#pragma once
#include "SocketStones.hpp"
#include "AccessorySockets.hpp"
#include <vector>

namespace auriga::item
{
// Named domain operations, not USE_* values. Each has fixed result semantics.
enum class SocketServiceKind : std::uint8_t { CleanBroken = 1, ExtractAll = 2, ExtractLast = 3 };
struct SocketServiceProfile
{
    std::uint32_t id = 0;
    std::string code;
    SocketServiceKind kind{};
    std::uint16_t unitsPerUse = 0;
};
bool ValidSocketServiceProfile(const SocketServiceProfile&) noexcept;

struct SocketServiceContext
{
    std::array<std::uint8_t,16> operationId{};
    std::uint64_t actorId = 0, sessionGeneration = 0, rulesetRevision = 0, inventoryRevision = 0;
    std::uint16_t characterLevel = 0;
    std::uint8_t characterClass = 0, sex = 0;
    bool admittedByServer = false, inCombat = false, observer = false;
};
struct SocketItemPair
{
    SocketServiceContext context;
    StoneItemSnapshot source, target;
    bool sourceInInventory = false, targetInInventory = false;
};
struct StoneReturnReservation
{
    // One newly reserved, durable identity and inventory placement per return.
    // The owner must validate the reservation against inventoryRevision again.
    std::uint8_t socket = 0;
    std::uint64_t instanceId = 0, placementToken = 0;
};
struct SocketServiceRequest
{
    SocketItemPair items;
    std::array<StoneSocketSlot,EquipmentStoneSockets> sockets{};
    std::vector<StoneReturnReservation> returns;
};
struct ReturnedSocketStone
{
    std::uint8_t socket;
    ItemKey item;
    std::uint16_t refinementLevel;
    std::uint64_t instanceId, placementToken;
    // Exactly one returned unit, even when its prototype is stackable.
};
struct SocketServicePlan
{
    SocketServiceRequest expected;
    std::uint64_t catalogRevision = 0;
    std::array<std::uint8_t,32> catalogHash{};
    std::array<StoneSocketSlot,EquipmentStoneSockets> after{};
    std::vector<ReturnedSocketStone> returns;
    std::uint16_t consumeUnits = 0;
};
enum class AccessoryServiceKind : std::uint8_t { Unlock = 1, Charge = 2 };
struct AccessoryServiceRequest
{
    SocketItemPair items;
    AccessorySocketState state;
    AccessoryServiceKind kind{};
    std::uint16_t recordedRoll = 0; // 1..10000; certain/disabled endpoints use 0.
};
struct AccessoryServicePlan
{
    AccessoryServiceRequest expected;
    std::uint64_t catalogRevision = 0;
    std::array<std::uint8_t,32> catalogHash{};
    AccessorySocketState after;
    std::uint16_t consumeUnits = 0;
    bool succeeded = false;
};
enum class SocketServiceError
{
    Forbidden, InvalidOperation, InvalidSnapshot, MissingItem, RetiredItem,
    InvalidStack, InvalidLevel, IncompatibleTarget, WrongSource, InvalidState,
    NoEffect, InvalidReservation, MaterialNotAllowed, InvalidDecision,
    OperationDisabled, LifetimeMismatch,
};
// Pure, owning proposals. No registry, SQL, spawning, RNG, stat or inventory write.
// The F3/F4 owner must commit receipt + consumption + complete socket replacement
// + every reserved stone together. No AutoGive/ground fallback or partial return.
// CommitUnknown requires durable operation-ID reconciliation before retry.
std::expected<SocketServicePlan,SocketServiceError> PlanSocketService(const ItemCatalog&,const SocketServiceRequest&);
std::expected<AccessoryServicePlan,SocketServiceError> PlanAccessoryService(const ItemCatalog&,const AccessoryServiceRequest&);
} // namespace auriga::item
