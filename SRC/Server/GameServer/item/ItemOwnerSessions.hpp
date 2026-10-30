#pragma once
// F4.2 (IC-034): durable item-owner sessions. The GameServer is the only item
// writer. Opening a session claims the owner (new durable session generation)
// and loads its typed state; every write of an older session is refused by
// the database fence. No ECS pointer, descriptor or callback crosses the SQL
// worker: completions are matched by ticket, exact job and lease.
#include <Core/player_session.h>
#include <ItemPersistenceSql.hpp>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <vector>

class CAsyncSQL;
struct _SQLMsg;

namespace auriga::item::runtime
{
// One live character session as the GameServer knows it: the negotiated
// Database load key and the full EnTT entity value. Results are applied only
// while both still match.
struct ItemSessionLease
{
    player_session::Key key;
    std::uint64_t entity = 0;
    bool operator==(const ItemSessionLease&) const = default;
};

enum class ItemSessionState : std::uint8_t
{
    Claiming,   // claim job in flight
    Loading,    // claimed; snapshot read in flight
    Active,     // loaded; writes may be admitted
    Closing,    // no new writes; waiting for in-flight jobs
    Closed,     // forgotten; the next Open claims again
    Superseded, // a newer session took the owner
    Failed,     // claim or load failed; Close, then Open again
};

// Runs one owned SQL job off the main thread. The owner of the executor later
// calls ItemOwnerSessions::Complete(ticket, message) on the main thread.
class ItemSqlExecutor
{
public:
    virtual ~ItemSqlExecutor() = default;
    virtual bool Submit(std::uint64_t ticket, SqlTransaction job) = 0;
};

struct ItemSessionEvent
{
    ItemOwner owner;
    ItemSessionLease lease;
    ItemSessionState state{};
    std::optional<ItemPersistenceError> error;
    bool operator==(const ItemSessionEvent&) const = default;
};

enum class ItemSessionOpen : std::uint8_t
{
    Started,      // claim submitted
    Queued,       // the previous session is still closing; claims after it
    AlreadyOpen,  // same lease already claiming, loading or active
    Busy,         // another lease holds this owner here; Close it first
    Invalid,      // bad owner or lease
    SubmitFailed, // the SQL worker refused the job; nothing is tracked
};

// Main-thread owner. Not thread-safe by design (single owner thread, IC-003).
class ItemOwnerSessions final
{
public:
    ItemOwnerSessions(std::shared_ptr<const ItemCatalog> catalog, ItemSqlExecutor& executor);

    ItemSessionOpen Open(ItemOwner owner, const ItemSessionLease& lease);
    void Complete(std::uint64_t ticket, _SQLMsg& message);
    void Close(ItemOwner owner, const ItemSessionLease& lease);

    // Admission of one durable write of an Active session. EndWrite reports the
    // fence the commit produced (nullopt: nothing committed).
    bool BeginWrite(ItemOwner owner, const ItemSessionLease& lease);
    void EndWrite(ItemOwner owner, const ItemSessionLease& lease, std::optional<ItemOwnerFence> committed);
    // A writer or reader saw StaleSession for this lease.
    void Superseded(ItemOwner owner, const ItemSessionLease& lease);

    ItemSessionState State(ItemOwner owner) const;
    // Snapshot taken when the session became Active; only for the same lease.
    const persistence::LoadedItemOwner* Loaded(ItemOwner owner, const ItemSessionLease& lease) const;
    std::optional<ItemOwnerFence> Fence(ItemOwner owner, const ItemSessionLease& lease) const;
    std::vector<ItemSessionEvent> TakeEvents();
    std::size_t Tracked() const noexcept { return sessions_.size(); }

private:
    struct Session
    {
        ItemSessionLease lease;
        ItemSessionState state = ItemSessionState::Claiming;
        std::uint64_t ticket = 0; // claim or load job in flight
        std::optional<ItemOwnerFence> fence;
        std::optional<persistence::LoadedItemOwner> loaded;
        std::size_t writes = 0;
        std::optional<ItemSessionLease> next; // Open while Closing
    };
    bool Submit(ItemOwner owner, Session& session, SqlTransaction job);
    ItemSessionOpen StartClaim(ItemOwner owner, Session& session);
    void Finish(ItemOwner owner, ItemSessionState state, std::optional<ItemPersistenceError> error = std::nullopt);
    void TryClose(ItemOwner owner);
    Session* Find(ItemOwner owner, const ItemSessionLease& lease);
    const Session* Find(ItemOwner owner, const ItemSessionLease& lease) const;

    std::shared_ptr<const ItemCatalog> catalog_;
    ItemSqlExecutor& executor_;
    std::map<ItemOwner, Session> sessions_;
    std::map<std::uint64_t, ItemOwner> tickets_;
    std::uint64_t nextTicket_ = 1;
    std::vector<ItemSessionEvent> events_;
};

// Production executor: a dedicated asynchronous CAsyncSQL connection (Setup
// with bNoThread=false). Pump on the main thread delivers completions.
class AsyncItemSqlExecutor final : public ItemSqlExecutor
{
public:
    explicit AsyncItemSqlExecutor(CAsyncSQL& connection) : connection_(connection) {}
    bool Submit(std::uint64_t ticket, SqlTransaction job) override;
    // Delivers every finished job; returns how many were delivered.
    std::size_t Pump(ItemOwnerSessions& sessions);

private:
    CAsyncSQL& connection_;
};

// The empty owner of a new character: inventory and equipment storages from
// the game's grid constants and a gold wallet with `gold`.
persistence::ItemOwnerProvision CharacterOwnerProvision(std::uint32_t playerId, std::uint64_t gold);
} // namespace auriga::item::runtime
