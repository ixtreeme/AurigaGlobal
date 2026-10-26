#include "MissionResourceFence.hpp"
#include <algorithm>
#include <stdexcept>

namespace auriga::mission::persistence
{
namespace
{
bool Nonzero(const Token &value)
{
    return std::any_of(value.begin(), value.end(), [](auto b) { return b != 0; });
}
} // namespace
ResourceFence::ResourceFence(ResourceWriterIdentity identity, PreparedResources resources, Token challenge,
                             std::array<uint64_t, 2> barriers)
    : identity_(identity), resources_(std::move(resources)), challenge_(challenge), barriers_(barriers)
{
    if (!identity.owner.character || !identity.owner.epoch || identity.owner.epoch == UINT64_MAX ||
        identity.owner.character != resources_.Before().character || !Nonzero(identity.owner.token) ||
        !identity.coreGeneration || !identity.sessionGeneration || !Nonzero(identity.generation) ||
        !Nonzero(challenge) || !barriers[0] || !barriers[1] || resources_.Bytes().empty())
        throw std::invalid_argument("Invalid resource writer fence identity/decision/barriers");
}
void ResourceFence::Ready()
{
    if (stage_ == ResourceFenceStage::Draining && stopped_ && drained_[0] && drained_[1])
        stage_ = ResourceFenceStage::Ready;
}
bool ResourceFence::SourceStopped(ResourceWriterIdentity identity, Token challenge)
{
    if (identity != identity_ || challenge != challenge_ || stage_ != ResourceFenceStage::Draining)
        return false;
    stopped_ = true;
    Ready();
    return true;
}
bool ResourceFence::LaneDrained(size_t lane, uint64_t barrier, SqlTransactionOutcome outcome)
{
    if (lane >= 2 || barriers_[lane] != barrier || stage_ != ResourceFenceStage::Draining)
        return false;
    if (outcome != SqlTransactionOutcome::Committed)
    {
        stage_ = ResourceFenceStage::Blocked;
        return false;
    }
    drained_[lane] = true;
    Ready();
    return true;
}
bool ResourceFence::Submitted(Token operation)
{
    if (stage_ != ResourceFenceStage::Ready || !Nonzero(operation))
        return false;
    operation_ = operation;
    stage_ = ResourceFenceStage::Submitted;
    return true;
}
bool ResourceFence::Complete(ResourceWriterIdentity identity, Token operation, SqlTransactionOutcome outcome)
{
    if (identity != identity_ || operation != operation_ || stage_ != ResourceFenceStage::Submitted)
        return false;
    if (outcome == SqlTransactionOutcome::Committed)
        stage_ = ResourceFenceStage::Committed;
    else
        stage_ = ResourceFenceStage::Reconciling; // Even a failure needs the durable operation barrier.
    return true;
}
bool ResourceFence::Reconciled(ResourceWriterIdentity identity, Token operation, Resolution outcome,
                               std::optional<ResourceEffectReceipt> receipt)
{
    if (identity != identity_ || operation != operation_ || stage_ != ResourceFenceStage::Reconciling)
        return false;
    if ((outcome == Resolution::Committed || outcome == Resolution::Absent) && receipt &&
        receipt->action == ActionIdentity(resources_.Action()) && receipt->decision == resources_.Digest() &&
        receipt->appliedRevision == resources_.After().revision)
        stage_ = ResourceFenceStage::Committed;
    else if (outcome == Resolution::Absent && !receipt)
        stage_ = ResourceFenceStage::Ready;
    else if (outcome == Resolution::SqlError)
        return false; // Remain frozen, no timeout release.
    else
        stage_ = ResourceFenceStage::Blocked;
    return true;
}
bool ResourceFence::Published(ResourceWriterIdentity identity, ContentDigest decision, uint64_t revision)
{
    if (identity != identity_ || stage_ != ResourceFenceStage::Rebased || decision != resources_.Digest() ||
        revision != resources_.After().revision)
        return false;
    stage_ = ResourceFenceStage::Published;
    return true;
}
bool ResourceFence::Rebased(ResourceWriterIdentity identity, ContentDigest decision, uint64_t revision,
                            ResourcePublicationVitals vitals)
{
    if (identity != identity_ || stage_ != ResourceFenceStage::Committed || decision != resources_.Digest() ||
        revision != resources_.After().revision || vitals.stamina > INT16_MAX)
        return false;
    vitals_ = vitals;
    stage_ = ResourceFenceStage::Rebased;
    return true;
}
void ResourceFence::Invalidate()
{
    stage_ = ResourceFenceStage::Invalidated;
}
} // namespace auriga::mission::persistence
