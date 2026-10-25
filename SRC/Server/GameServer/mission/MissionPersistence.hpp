#pragma once
#include "MissionActions.hpp"
#include "MissionCheckpointCodec.hpp"
#include "MissionPackage.hpp"
#include <SQL/PreparedTransaction.h>

struct _SQLMsg;

namespace auriga::mission::persistence
{
using Token = std::array<uint8_t, 16>;
Token NewToken();
struct Owner
{
    CharacterId character = 0;
    uint64_t epoch = 0;
    Token token{};
    bool operator==(const Owner &) const = default;
};
enum class Operation : uint8_t
{
    Claim = 1,
    Save = 2,
    Release = 3,
    Entitle = 4
};
struct Plan
{
    SqlTransaction statements;
    CharacterId character = 0;
    Token operation{};
    Operation kind = Operation::Save;
    uint64_t ownerEpoch = 0;
    std::optional<uint64_t> storageRevision;
    ContentDigest request{};
    std::vector<Diagnostic> errors;
    explicit operator bool() const
    {
        return errors.empty() && !statements.empty();
    }
};
struct ReadCommand
{
    std::string query;
    SqlParams params;
};
enum class ReadStatus
{
    Found,
    Missing,
    Invalid,
    SqlError
};
struct Snapshot
{
    Owner owner;
    uint64_t storageRevision = 0;
    std::vector<uint8_t> bytes;
    CheckpointScan metadata;
};
struct SnapshotResult
{
    ReadStatus status = ReadStatus::Invalid;
    Snapshot snapshot;
    std::vector<Diagnostic> errors;
};
struct Head
{
    CharacterId character = 0;
    uint64_t epoch = 0, storageRevision = 0;
};
struct HeadResult
{
    ReadStatus status = ReadStatus::Invalid;
    Head head;
};
enum class Resolution
{
    Committed,
    Absent,
    TokenConflict,
    SqlError,
    Invalid
};
struct Resolved
{
    Resolution status = Resolution::Invalid;
    uint64_t ownerEpoch = 0, storageRevision = 0;
};

// Pure owned commands/plans: submit to CAsyncSQL::ReturnQueryPrepared or
// ReturnTransactionPrepared. Only the authoritative caller may apply completions
// to a re-found matching character/session. No main-thread blocking SQL here.
Plan ArchivePackage(std::span<const uint8_t> bytes);
Plan Claim(CharacterId character, uint64_t expectedEpoch, Token newOwner, Token operation);
Plan Save(const MissionRuntime::Checkpoint &checkpoint, const DefinitionRegistry &registry, Owner owner,
          uint64_t expectedStorageRevision, Token operation);
Plan Release(Owner owner, Token operation);
// Commits a symbolic, immutable reward entitlement, NOT an inventory/XP/money
// effect or delivery receipt. Only a trusted owner coordinator may call this,
// after saving the current authoritative decision checkpoint. No player API.
Plan RecordEntitlement(Owner owner, const ActionKey &action, uint64_t expectedStorageRevision,
                       ContentDigest checkpointDigest, Token operation);
enum class ActionState : uint8_t
{
    Pending = 1,
    Cancelled = 2,
    Entitled = 3
};
struct ActionRecord
{
    ActionKey key;
    ContentDigest id{}, definition{};
    ActionState state = ActionState::Pending;
    uint64_t firstStorageRevision = 0, currentStorageRevision = 0;
    uint64_t entitlementEpoch = 0, entitlementStorageRevision = 0;
    ContentDigest checkpoint{};
};
struct ActionRead
{
    ReadStatus status = ReadStatus::Invalid;
    ActionRecord action;
};
ReadCommand ReadAction(Owner owner, const ActionKey &action);
ActionRead DecodeAction(_SQLMsg &message, Owner owner, const ActionKey &action);
// A coordinator authorizes an explicit transfer; observing an epoch is not authorization.
ReadCommand ReadHead(CharacterId character);
HeadResult DecodeHead(_SQLMsg &message, CharacterId character);
ReadCommand ReadSnapshot(Owner owner);
SnapshotResult DecodeSnapshot(_SQLMsg &message, Owner owner);
ReadCommand ReadDefinitionArchive(const DefinitionReference &reference);
PackageLoad DecodeDefinitionArchive(_SQLMsg &message, const DefinitionReference &reference);

// Resolve after any non-confirmed write. The first locking read waits for the
// original character writer, the second is a current read even under REPEATABLE
// READ. An ordinary nonlocking SELECT is not a safe absence barrier.
SqlTransaction Resolve(const Plan &plan);
Resolved DecodeResolution(_SQLMsg &message, const Plan &plan);
bool Confirmed(const _SQLMsg &message);
} // namespace auriga::mission::persistence
