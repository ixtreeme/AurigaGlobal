#pragma once
// F4.4 (IC-034): the GameServer's durable item writer. One serialized lane per
// owner: an intent is turned into a write set from the CURRENT ECS state only
// when it reaches the head of its lane, the write set is committed through the
// owned SQL job, and the ECS changes only after a confirmed commit. A lost
// COMMIT acknowledgement is decided by a locking receipt read; it is never
// resent blindly. Main thread only.
#include "ItemOwnerSessions.hpp"
#include <item/InventoryOperations.hpp>
#include <entt/entity/registry.hpp>
#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <variant>
#include <vector>

namespace auriga::item::runtime
{
struct MoveIntent
{
    ItemInstanceId item;
    ItemPlacement to;
};
struct MergeIntent
{
    ItemInstanceId source, target;
    std::uint32_t count = 0;
};
struct SplitIntent
{
    ItemInstanceId source;
    std::uint32_t count = 0;
    ItemPlacement to;
};
struct DestroyIntent
{
    ItemInstanceId item;
};
struct ConsumeIntent
{
    ItemInstanceId item;
    std::uint32_t count = 0;
};
using ItemIntent = std::variant<MoveIntent, MergeIntent, SplitIntent, DestroyIntent, ConsumeIntent>;

enum class ItemWriteOutcome : std::uint8_t
{
    Committed,    // durable and applied to the ECS
    Rejected,     // refused before SQL (rules, state); nothing was sent
    NotCommitted, // SQL confirmed that nothing was written
    Superseded,   // the session lost the owner; a fresh load decides
    Cancelled,    // the lane closed before the intent started
    Diverged,     // ECS and database disagree; the session must reload
    Unresolved,   // the commit could not be decided; the session must reload
};
struct ItemWriteResult
{
    std::uint64_t request = 0;
    ItemOwner owner;
    ItemSessionLease lease;
    ItemWriteOutcome outcome{};
    std::optional<ItemPersistenceError> error;
    ItemOperationId operation{};
    bool operator==(const ItemWriteResult&) const = default;
};

class ItemWriter final
{
public:
    using OperationIds = std::function<ItemOperationId()>;
    static constexpr std::uint32_t IdBlock = 64;
    static constexpr unsigned ReconcileAttempts = 3;

    ItemWriter(std::shared_ptr<const ItemCatalog> catalog, ItemOwnerSessions& sessions, entt::registry& registry,
               ItemSqlExecutor& executor, OperationIds operationIds);

    // Queues one intent of an Active session whose owner is materialized in
    // the registry. nullopt: not admitted (nothing queued).
    std::optional<std::uint64_t> Request(ItemOwner owner, const ItemSessionLease& lease, ItemIntent intent);
    // Starts the head intent of every idle lane. Call once per tick.
    void Pump();
    void Complete(std::uint64_t ticket, _SQLMsg& message);
    // Cancels queued intents of the lease; an intent already in SQL finishes.
    void Close(ItemOwner owner, const ItemSessionLease& lease);

    std::vector<ItemWriteResult> TakeResults();
    std::size_t Queued(ItemOwner owner) const;
    bool Busy(ItemOwner owner) const;
    std::uint64_t AvailableIds() const;

private:
    enum class Phase : std::uint8_t { Idle, Writing, Reconciling, Reserving };
    struct Pending
    {
        std::uint64_t id = 0;
        ItemIntent intent;
    };
    struct Lane
    {
        ItemSessionLease lease;
        std::deque<Pending> queue;
        Phase phase = Phase::Idle;
        std::uint64_t ticket = 0;
        std::optional<Pending> current;
        std::optional<ItemCommit> commit;
        unsigned attempts = 0;
        bool closing = false;
    };
    bool Start(ItemOwner owner, Lane& lane);
    bool Submit(ItemOwner owner, Lane& lane, SqlTransaction job, Phase phase);
    void Committed(ItemOwner owner, Lane& lane);
    void Finish(ItemOwner owner, Lane& lane, ItemWriteOutcome outcome, std::optional<ItemPersistenceError> error);
    void Abandon(ItemOwner owner, Lane& lane, ItemWriteOutcome outcome);
    void Report(ItemOwner owner, const Lane& lane, const Pending& pending, ItemWriteOutcome outcome,
                std::optional<ItemPersistenceError> error, ItemOperationId operation = {});
    std::optional<ItemInstanceId> TakeId();

    std::shared_ptr<const ItemCatalog> catalog_;
    ItemOwnerSessions& sessions_;
    entt::registry& registry_;
    ItemSqlExecutor& executor_;
    OperationIds operationIds_;
    std::map<ItemOwner, Lane> lanes_;
    std::map<std::uint64_t, ItemOwner> tickets_;
    std::deque<persistence::ItemInstanceIdRange> ids_;
    std::uint64_t nextRequest_ = 1, nextTicket_ = 1;
    std::vector<ItemWriteResult> results_;
};

// The production operation ids: 16 bytes from the OpenSSL CSPRNG.
ItemOperationId RandomOperationId();
} // namespace auriga::item::runtime
