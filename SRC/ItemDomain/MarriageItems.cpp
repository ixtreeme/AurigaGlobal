#include <item/MarriageItems.hpp>

#include <algorithm>

namespace auriga::item
{
namespace
{
using Error = ItemMissionError;
constexpr std::string_view EngagementRing = "catalog.10.engagement_ring_0b94aa15d7f4";
constexpr std::string_view WeddingRing = "catalog.10.wedding_ring_c9f43b894733";

bool ValidAction(MarriageItemChange change)
{
    return change.initiator && change.partner && change.initiator != change.partner &&
        change.action >= MarriageItemAction::Engage && change.action <= MarriageItemAction::UnilateralDivorce;
}

ItemOwner Character(std::uint32_t pid) { return {ItemOwnerKind::Character, pid}; }

std::uint64_t Price(MarriageItemChange change, ItemOwner owner)
{
    if (change.action == MarriageItemAction::MutualDivorce)
        return 500000;
    return owner == Character(change.initiator) ? 1000000 : 0;
}

std::size_t Participants(MarriageItemAction action)
{
    return action == MarriageItemAction::UnilateralDivorce ? 1 : 2;
}

const ItemCatalogEntry* Ring(const ItemCatalog& catalog, std::string_view code)
{
    const auto& entries = catalog.Entries();
    const auto found = std::ranges::find_if(entries, [code](const auto& row) { return row.second.base.contentCode == code; });
    return found == entries.end() || found->second.base.retired ? nullptr : &found->second;
}
}

std::optional<MarriageItemChange> ReadMarriageItemChange(const ItemCommit& write)
{
    const auto& data = write.result;
    if (write.outcomeCode != MarriageItemOutcome || data.size() != 13 ||
        data[0] != 'M' || data[1] != 'A' || data[2] != 'R' || data[3] != 1)
        return {};
    const auto read = [&](std::size_t offset) {
        std::uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i)
            value |= static_cast<std::uint32_t>(data[offset + i]) << (i * 8);
        return value;
    };
    MarriageItemChange change{static_cast<MarriageItemAction>(data[4]), read(5), read(9)};
    return ValidAction(change) ? std::optional{change} : std::nullopt;
}

bool ValidateMarriageItemWrite(const ItemCatalog& catalog, const ItemCommit& commit)
{
    if (commit.outcomeCode != MarriageItemOutcome)
        return true;
    const auto change = ReadMarriageItemChange(commit);
    if (!change || commit.kind != ItemOperationKind::Service || commit.owners.size() != Participants(change->action) ||
        commit.ground || commit.recovery || commit.learning || commit.petStudyCooldown || commit.currencyAward ||
        commit.nativeAward || commit.currencyDrop || commit.privateShop || commit.dungeonEntry)
        return false;

    const bool engage = change->action == MarriageItemAction::Engage;
    const auto* cost = Ring(catalog, engage ? EngagementRing : WeddingRing);
    const auto* reward = engage ? Ring(catalog, WeddingRing) : nullptr;
    if (!cost || (engage && !reward) || commit.items.size() != commit.owners.size() * (engage ? 2 : 1))
        return false;

    for (const auto& fence : commit.owners)
    {
        if (fence.owner != Character(change->initiator) &&
            (commit.owners.size() != 2 || fence.owner != Character(change->partner)))
            return false;
        unsigned taken = 0, given = 0;
        for (const auto& item : commit.items)
        {
            if (item.after.owner != fence.owner) continue;
            if (item.after.refinementLevel || (item.before && (item.before->owner != fence.owner ||
                item.before->key != item.after.key || item.before->refinementLevel))) return false;
            const auto before = item.before ? item.before->count : 0;
            if (item.after.key == cost->base.key && item.before && MissionItemUsable(*item.before, fence.owner) &&
                before > 0 && item.after.count == before - 1)
                ++taken;
            else if (reward && item.after.key == reward->base.key &&
                MissionItemUsable(item.after, fence.owner) && item.after.count > before && item.after.count - before == 1)
                ++given;
            else
                return false;
        }
        if (taken != 1 || given != (engage ? 1u : 0u)) return false;

        const auto price = Price(*change, fence.owner);
        unsigned charged = 0;
        for (const auto& wallet : commit.wallets)
        {
            if (wallet.after.owner != fence.owner) continue;
            if (!price || wallet.before.currency != WalletCurrency::Gold || wallet.before.balance < price ||
                wallet.after.balance != wallet.before.balance - price)
                return false;
            ++charged;
        }
        if (charged != (price ? 1u : 0u)) return false;
    }
    return true;
}

std::expected<ItemCommit, Error> MakeMarriageItemCommit(const ItemCatalog& catalog,
    std::array<std::uint8_t, 16> operation, MarriageItemChange change,
    std::span<const MarriageItemParticipant> participants, std::span<const ItemInstanceId> ids, std::uint64_t nowUtcMs)
{
    if (!ValidAction(change) || participants.size() != Participants(change.action) ||
        participants[0].fence.owner != Character(change.initiator) ||
        (participants.size() == 2 && participants[1].fence.owner != Character(change.partner)))
        return std::unexpected(Error::Invalid);

    const bool engage = change.action == MarriageItemAction::Engage;
    const auto* cost = Ring(catalog, engage ? EngagementRing : WeddingRing);
    const auto* reward = engage ? Ring(catalog, WeddingRing) : nullptr;
    if (!cost || (engage && (!reward || ids.size() < participants.size())))
        return std::unexpected(Error::Unavailable);

    ItemCommit result;
    result.operationId = operation;
    result.catalogRevision = catalog.Revision();
    result.catalogHash = catalog.ContentHash();
    result.kind = ItemOperationKind::Service;
    result.outcomeCode = MarriageItemOutcome;
    result.result = {'M','A','R',1,static_cast<std::uint8_t>(change.action)};
    for (const auto pid : {change.initiator, change.partner})
        for (unsigned i = 0; i < 4; ++i)
            result.result.push_back(static_cast<std::uint8_t>(pid >> (i * 8)));

    ItemMission terms;
    terms.id = terms.version = 1;
    terms.code = "marriage.item.settlement";
    terms.name = "Marriage item settlement";
    terms.costs = {{cost->base.key, 0, 1}};
    if (engage) terms.rewards = {{reward->base.key, 0, 1}};

    std::size_t assignedIds = 0;
    for (const auto& actor : participants)
    {
        ItemMissionProof proof{actor.fence.owner};
        std::ranges::copy(operation, proof.action.begin());
        terms.goldCost = Price(change, actor.fence.owner);
        auto write = MakeItemMissionCommit(catalog, terms, proof, 0, actor.fence, actor.storages, actor.items,
            actor.wallets, ids.subspan(assignedIds), nowUtcMs);
        if (!write) return std::unexpected(write.error());
        assignedIds += static_cast<std::size_t>(std::ranges::count_if(write->items, [](const auto& entry) { return !entry.before; }));
        result.owners.push_back(actor.fence);
        result.storages.insert(result.storages.end(), write->storages.begin(), write->storages.end());
        result.items.insert(result.items.end(), write->items.begin(), write->items.end());
        result.wallets.insert(result.wallets.end(), write->wallets.begin(), write->wallets.end());
    }
    if (!ValidateItemCommit(catalog, result)) return std::unexpected(Error::Invalid);
    return result;
}
}
