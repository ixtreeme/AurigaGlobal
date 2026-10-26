#pragma once
#include "MissionPersistence.hpp"

namespace auriga::mission::persistence
{
struct ResourceWriterIdentity
{
    Owner owner;
    uint64_t coreGeneration = 0, sessionGeneration = 0;
    Token generation{};
    bool operator==(const ResourceWriterIdentity &) const = default;
};
struct ResourcePublicationVitals
{
    uint64_t hp = 0, sp = 0;
    uint16_t stamina = 0;
    bool operator==(const ResourcePublicationVitals &) const = default;
};
enum class ResourceFenceStage
{
    Draining,
    Ready,
    Submitted,
    Reconciling,
    Committed,
    Rebased,
    Published,
    Blocked,
    Invalidated
};
// Main-thread protocol state, not an ECS pointer or a SQL worker callback.
// The future native adapter must actually stop that session's producers and
// enqueue a barrier on BOTH legacy SQL lanes before acknowledging them here.
class ResourceFence final
{
  public:
    ResourceFence(ResourceWriterIdentity identity, PreparedResources resources, Token challenge,
                  std::array<uint64_t, 2> barriers);
    bool SourceStopped(ResourceWriterIdentity identity, Token challenge);
    bool LaneDrained(size_t lane, uint64_t barrier, SqlTransactionOutcome outcome);
    bool Submitted(Token operation);
    bool Complete(ResourceWriterIdentity identity, Token operation, SqlTransactionOutcome outcome);
    bool Reconciled(ResourceWriterIdentity identity, Token operation, Resolution outcome,
                    std::optional<ResourceEffectReceipt> receipt = {});
    bool Published(ResourceWriterIdentity identity, ContentDigest decision, uint64_t revision);
    bool Rebased(ResourceWriterIdentity identity, ContentDigest decision, uint64_t revision,
                 ResourcePublicationVitals vitals);
    void Invalidate();
    ResourceFenceStage Stage() const
    {
        return stage_;
    }
    const ResourceWriterIdentity &Identity() const
    {
        return identity_;
    }
    const PreparedResources &Resources() const
    {
        return resources_;
    }
    const Token &Challenge() const
    {
        return challenge_;
    }
    const std::optional<ResourcePublicationVitals> &Vitals() const
    {
        return vitals_;
    }

  private:
    void Ready();
    ResourceWriterIdentity identity_;
    PreparedResources resources_;
    Token challenge_{}, operation_{};
    std::array<uint64_t, 2> barriers_{};
    std::array<bool, 2> drained_{};
    bool stopped_ = false;
    std::optional<ResourcePublicationVitals> vitals_;
    ResourceFenceStage stage_ = ResourceFenceStage::Draining;
};
} // namespace auriga::mission::persistence
