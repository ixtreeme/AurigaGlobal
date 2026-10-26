#pragma once
#include "MissionItemAwards.hpp"
#include "MissionResourceFence.hpp"

namespace auriga::mission::persistence
{
struct ResourceEffectsPlan
{
    ItemAwardsPlan awards;
    ContentDigest decision{};
    ContentDigest binding{};
    uint64_t sourceRevision = 0, appliedRevision = 0;
    ResourceWriterIdentity writer;
    explicit operator bool() const
    {
        return bool(awards);
    }
};
// Backend contract. Gameplay remains CLOSED until the legacy cache/session
// adapters actually implement every stop/drain/guard/rebase step of the fence.
// Executes on sql.mission, with canonical qualified player/account tables.
ResourceEffectsPlan ApplyResourceEffects(const ResourceFence &fence, const PinnedReward &reward,
                                         uint64_t storageRevision, ContentDigest checkpoint, Token operation,
                                         ItemAwardTarget target, ItemAwardRecipient recipient,
                                         const std::map<uint32_t, uint32_t> &verifiedMaximumCounts);
SqlTransaction ResolveResourceEffects(const ResourceEffectsPlan &plan);
enum class ResourcePublicationStatus
{
    Unknown,
    Pending,
    Published,
    Superseded
};
struct ResourceEffectsReceipt
{
    ReadStatus status = ReadStatus::Invalid;
    std::optional<ResourceEffectReceipt> effect;
    std::vector<IssuedItemAward> awards;
    uint64_t currentRevision = 0;
    ResourcePublicationStatus publication = ResourcePublicationStatus::Unknown;
    std::vector<Diagnostic> errors;
};
ResourceEffectsReceipt DecodeResourceEffects(_SQLMsg &message, const ResourceEffectsPlan &plan);
// Only after the ECS postimage, vitals and cache rebase have succeeded for the
// matching generation. A SQL effect receipt alone must never resume gameplay.
Plan PublishResourceEffects(const ResourceFence &fence, const ResourceEffectsPlan &effects, Token operation);
// Trusted fixed native cache DML only. No DDL, arbitrary client SQL or BEGIN /
// COMMIT in writes. Serialize a character's jobs and advance its durable
// revision on EVERY successful batch, including A -> B -> A changes.
Plan GuardResourceWrites(ResourceWriterIdentity writer, uint64_t revision, Token operation,
                         ItemAwardTarget target, SqlTransaction writes);
} // namespace auriga::mission::persistence
