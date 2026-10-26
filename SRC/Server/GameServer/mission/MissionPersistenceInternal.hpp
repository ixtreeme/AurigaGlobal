#pragma once
#include "MissionItemAwards.hpp"

namespace auriga::mission::persistence::detail
{
// Internal builders, not gameplay entry points. All appended DML stays inside
// the same owned transaction as the owner fence and final operation receipt.
Plan ResourceOperation(Owner owner, Operation kind, Token operation, ContentDigest request);
void FinishResourceOperation(Plan &plan);
ItemAwardsPlan DraftItemAwards(Owner owner, const PinnedReward &reward, uint64_t storageRevision,
                               ContentDigest checkpoint, Token operation, ItemAwardTarget target,
                               ItemAwardRecipient recipient,
                               const std::map<uint32_t, uint32_t> &verifiedMaximumCounts);
} // namespace auriga::mission::persistence::detail
