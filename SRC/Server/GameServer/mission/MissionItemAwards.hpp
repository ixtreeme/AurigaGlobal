#pragma once
#include "MissionPersistence.hpp"

namespace auriga::mission::persistence
{
struct ItemAwardTarget
{
    std::string playerSchema, accountSchema, playerPostfix;
};
struct ItemAwardRecipient
{
    uint32_t account = 0;
    std::string login; // Canonical lower-case authenticated account login.
};
struct ItemAwardsPlan
{
    Plan transaction;
    ItemAwardTarget target;
    ItemAwardRecipient recipient;
    ActionKey action;
    ContentDigest definition{}, payload{};
    std::map<uint32_t, uint32_t> items;
    explicit operator bool() const
    {
        return bool(transaction);
    }
};
// Pure owned item-only adapter, submitted on the sql.mission worker connection.
// All three schemas must be on that MariaDB. The trusted coordinator supplies
// authenticated identity and verified prototype/stack caps, never client data.
// Rejects the ENTIRE mixed XP/gold/consume bundle; no partial entitlement.
// SAFEBOX delivery: this creates awards, never writes an open ECS inventory.
ItemAwardsPlan IssueItemAwards(Owner owner, const PinnedReward &reward, uint64_t storageRevision,
                               ContentDigest checkpoint, Token operation, ItemAwardTarget target,
                               ItemAwardRecipient recipient,
                               const std::map<uint32_t, uint32_t> &verifiedMaximumCounts);

struct IssuedItemAward
{
    uint32_t id = 0, vnum = 0, count = 0;
    bool operator==(const IssuedItemAward &) const = default;
};
enum class ItemAwardsStatus
{
    Issued,
    NotIssued,
    Cancelled,
    Conflict,
    Invalid,
    SqlError
};
struct ItemAwardsReceipt
{
    ItemAwardsStatus status = ItemAwardsStatus::Invalid;
    std::vector<IssuedItemAward> awards;
};
// Current locking reads wait for the original writer. Works with a new operation
// token after reconnect: a complete matching action receipt still returns the
// original IDs. Historical receipt != authorization to mutate a live session.
SqlTransaction ResolveItemAwards(const ItemAwardsPlan &plan);
ItemAwardsReceipt DecodeItemAwards(_SQLMsg &message, const ItemAwardsPlan &plan, size_t expectedSteps = 4);
} // namespace auriga::mission::persistence
