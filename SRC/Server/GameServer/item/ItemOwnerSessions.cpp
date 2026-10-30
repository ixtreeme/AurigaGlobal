// length.h before AsyncSQL.h: the SQL header #defines QUERY_MAX_LEN, an EMisc name.
#include <common/length.h>
#include "ItemOwnerSessions.hpp"
#include <SQL/AsyncSQL.h>
#include <stdexcept>
#include <utility>

namespace auriga::item::runtime
{
namespace p = auriga::item::persistence;

ItemOwnerSessions::ItemOwnerSessions(std::shared_ptr<const ItemCatalog> catalog, ItemSqlExecutor& executor)
    : catalog_(std::move(catalog)), executor_(executor)
{
    if (!catalog_) throw std::invalid_argument("item owner sessions need an installed catalog");
}

ItemSessionOpen ItemOwnerSessions::Open(ItemOwner owner, const ItemSessionLease& lease)
{
    // A character owner belongs to exactly the character the session loaded.
    if (!ValidItemOwner(owner) || !lease.entity || !lease.key.generation ||
        (owner.kind == ItemOwnerKind::Character && owner.id != lease.key.character) ||
        (owner.kind == ItemOwnerKind::Account && owner.id != lease.key.account))
        return ItemSessionOpen::Invalid;
    const auto found = sessions_.find(owner);
    if (found != sessions_.end())
    {
        auto& session = found->second;
        switch (session.state)
        {
        case ItemSessionState::Claiming:
        case ItemSessionState::Loading:
        case ItemSessionState::Active:
            return session.lease == lease ? ItemSessionOpen::AlreadyOpen : ItemSessionOpen::Busy;
        case ItemSessionState::Closing:
            session.next = lease;
            return ItemSessionOpen::Queued;
        case ItemSessionState::Closed:
        case ItemSessionState::Superseded:
        case ItemSessionState::Failed:
            if (session.ticket || session.writes)
            {
                // Still waiting for an old job: claim once it has finished.
                session.state = ItemSessionState::Closing;
                session.next = lease;
                return ItemSessionOpen::Queued;
            }
            sessions_.erase(found);
            break;
        }
    }
    auto& session = sessions_[owner];
    session.lease = lease;
    const auto started = StartClaim(owner, session);
    if (started != ItemSessionOpen::Started) sessions_.erase(owner);
    return started;
}

ItemSessionOpen ItemOwnerSessions::StartClaim(ItemOwner owner, Session& session)
{
    session.state = ItemSessionState::Claiming;
    session.fence.reset();
    session.loaded.reset();
    auto job = p::PrepareClaimItemOwner(*catalog_, owner);
    if (!job) return ItemSessionOpen::Invalid;
    return Submit(owner, session, std::move(*job)) ? ItemSessionOpen::Started : ItemSessionOpen::SubmitFailed;
}

bool ItemOwnerSessions::Submit(ItemOwner owner, Session& session, SqlTransaction job)
{
    const auto ticket = nextTicket_++;
    if (!executor_.Submit(ticket, std::move(job))) return false;
    session.ticket = ticket;
    tickets_.emplace(ticket, owner);
    return true;
}

void ItemOwnerSessions::Complete(std::uint64_t ticket, _SQLMsg& message)
{
    const auto known = tickets_.find(ticket);
    if (known == tickets_.end()) return;
    const auto owner = known->second;
    tickets_.erase(known);
    const auto found = sessions_.find(owner);
    if (found == sessions_.end() || found->second.ticket != ticket) return;
    auto& session = found->second;
    session.ticket = 0;

    switch (session.state)
    {
    case ItemSessionState::Claiming:
    {
        const auto fence = p::ClaimItemOwnerCompleted(*catalog_, owner, message);
        if (!fence) return Finish(owner, ItemSessionState::Failed, fence.error());
        session.fence = *fence;
        auto job = p::PrepareLoadItemOwner(*catalog_, owner, fence->sessionGeneration);
        if (!job) return Finish(owner, ItemSessionState::Failed, job.error());
        session.state = ItemSessionState::Loading;
        if (!Submit(owner, session, std::move(*job))) return Finish(owner, ItemSessionState::Failed, ItemPersistenceError::SqlFailure);
        return;
    }
    case ItemSessionState::Loading:
    {
        auto loaded = p::LoadItemOwnerCompleted(*catalog_, owner, session.fence->sessionGeneration, message);
        if (!loaded)
            return Finish(owner, loaded.error() == ItemPersistenceError::StaleSession ? ItemSessionState::Superseded : ItemSessionState::Failed,
                          loaded.error());
        session.fence = loaded->fence;
        session.loaded = std::move(*loaded);
        return Finish(owner, ItemSessionState::Active);
    }
    case ItemSessionState::Closing:
        // Closed while claiming or loading: the result is not used.
        return TryClose(owner);
    default:
        return;
    }
}

void ItemOwnerSessions::Finish(ItemOwner owner, ItemSessionState state, std::optional<ItemPersistenceError> error)
{
    auto& session = sessions_.at(owner);
    session.state = state;
    if (state != ItemSessionState::Active) session.loaded.reset();
    events_.push_back({owner, session.lease, state, error});
}

void ItemOwnerSessions::Close(ItemOwner owner, const ItemSessionLease& lease)
{
    auto* session = Find(owner, lease);
    if (!session) return;
    switch (session->state)
    {
    case ItemSessionState::Claiming:
    case ItemSessionState::Loading:
    case ItemSessionState::Active:
    case ItemSessionState::Superseded:
    case ItemSessionState::Failed:
        session->state = ItemSessionState::Closing;
        session->loaded.reset();
        TryClose(owner);
        return;
    default:
        return;
    }
}

void ItemOwnerSessions::TryClose(ItemOwner owner)
{
    const auto found = sessions_.find(owner);
    if (found == sessions_.end()) return;
    auto& session = found->second;
    if (session.state != ItemSessionState::Closing || session.ticket || session.writes) return;
    events_.push_back({owner, session.lease, ItemSessionState::Closed, std::nullopt});
    const auto next = session.next;
    sessions_.erase(found);
    if (!next) return;
    auto& reopened = sessions_[owner];
    reopened.lease = *next;
    if (const auto started = StartClaim(owner, reopened); started != ItemSessionOpen::Started)
    {
        events_.push_back({owner, *next, ItemSessionState::Failed, ItemPersistenceError::SqlFailure});
        sessions_.erase(owner);
    }
}

bool ItemOwnerSessions::BeginWrite(ItemOwner owner, const ItemSessionLease& lease)
{
    auto* session = Find(owner, lease);
    if (!session || session->state != ItemSessionState::Active) return false;
    ++session->writes;
    return true;
}

void ItemOwnerSessions::EndWrite(ItemOwner owner, const ItemSessionLease& lease, std::optional<ItemOwnerFence> committed)
{
    auto* session = Find(owner, lease);
    if (!session || !session->writes) return;
    --session->writes;
    // Only a newer revision of this exact session moves the fence forward.
    if (committed && session->fence && committed->owner == owner &&
        committed->sessionGeneration == session->fence->sessionGeneration && committed->revision > session->fence->revision)
        session->fence = *committed;
    TryClose(owner);
}

void ItemOwnerSessions::Superseded(ItemOwner owner, const ItemSessionLease& lease)
{
    auto* session = Find(owner, lease);
    if (!session || session->state != ItemSessionState::Active) return;
    Finish(owner, ItemSessionState::Superseded, ItemPersistenceError::StaleSession);
}

ItemSessionState ItemOwnerSessions::State(ItemOwner owner) const
{
    const auto found = sessions_.find(owner);
    return found == sessions_.end() ? ItemSessionState::Closed : found->second.state;
}

const p::LoadedItemOwner* ItemOwnerSessions::Loaded(ItemOwner owner, const ItemSessionLease& lease) const
{
    const auto* session = Find(owner, lease);
    return session && session->state == ItemSessionState::Active && session->loaded ? &*session->loaded : nullptr;
}

std::optional<ItemOwnerFence> ItemOwnerSessions::Fence(ItemOwner owner, const ItemSessionLease& lease) const
{
    const auto* session = Find(owner, lease);
    if (!session || session->state != ItemSessionState::Active) return std::nullopt;
    return session->fence;
}

std::vector<ItemSessionEvent> ItemOwnerSessions::TakeEvents()
{
    return std::exchange(events_, {});
}

ItemOwnerSessions::Session* ItemOwnerSessions::Find(ItemOwner owner, const ItemSessionLease& lease)
{
    const auto found = sessions_.find(owner);
    return found != sessions_.end() && found->second.lease == lease ? &found->second : nullptr;
}

const ItemOwnerSessions::Session* ItemOwnerSessions::Find(ItemOwner owner, const ItemSessionLease& lease) const
{
    const auto found = sessions_.find(owner);
    return found != sessions_.end() && found->second.lease == lease ? &found->second : nullptr;
}

namespace
{
struct Ticket
{
    std::uint64_t value;
};
} // namespace

bool AsyncItemSqlExecutor::Submit(std::uint64_t ticket, SqlTransaction job)
{
    auto owned = std::make_unique<Ticket>(Ticket{ticket});
    try
    {
        connection_.ReturnTransactionPrepared(std::move(job), owned.get());
    }
    catch (const std::logic_error&)
    {
        return false; // not an asynchronous connection
    }
    owned.release(); // now carried by the queued SQLMsg until Pump
    return true;
}

std::size_t AsyncItemSqlExecutor::Pump(ItemOwnerSessions& sessions)
{
    std::size_t delivered = 0;
    SQLMsg* raw = nullptr;
    while (connection_.PopResult(&raw))
    {
        std::unique_ptr<SQLMsg> message(raw);
        std::unique_ptr<Ticket> ticket(static_cast<Ticket*>(message->pvUserData));
        message->pvUserData = nullptr;
        if (ticket) sessions.Complete(ticket->value, *message);
        ++delivered;
    }
    return delivered;
}

p::ItemOwnerProvision CharacterOwnerProvision(std::uint32_t playerId, std::uint64_t gold)
{
    const ItemOwner owner{ItemOwnerKind::Character, playerId};
    // Equipment anchors are EquipmentSlot - 1 (weapon .. ear).
    constexpr std::uint16_t EquipmentSlots = static_cast<std::uint16_t>(EquipmentSlot::Ear);
    return {owner,
            {{{owner, ItemStorageKind::Inventory, 1}, INVENTORY_PAGE_COLUMN, INVENTORY_PAGE_ROW, INVENTORY_PAGE_COUNT},
             {{owner, ItemStorageKind::Equipment, 1}, EquipmentSlots, 1, 1}},
            {{owner, WalletCurrency::Gold, 1, gold}}};
}
} // namespace auriga::item::runtime
