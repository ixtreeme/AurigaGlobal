#pragma once
#include "MissionConditionEvaluator.hpp"
#include "MissionEventRouter.hpp"
#include <bitset>
#include <deque>
#include <functional>
#include <optional>
#include <thread>

namespace auriga::mission
{
struct CharacterSession
{
    CharacterId character = 0;
    uint64_t session = 0;
    bool operator==(const CharacterSession &) const = default;
};
struct EventId
{
    uint64_t stream = 0, sequence = 0;
    auto operator<=>(const EventId &) const = default;
};
struct GameplayEvent
{
    CharacterSession owner;
    EventId id;
    Timestamp occurredAt = 0;
    EventKind kind = EventKind::MobKilled;
    std::string subject;
    uint64_t amount = 1;
    std::string choice;
};
enum class MissionStatus : uint8_t
{
    Active,
    Completed,
    Failed,
    Abandoned
};
enum class NodeStatus : uint8_t
{
    Dormant,
    Active,
    Completed,
    Failed,
    Cancelled
};
struct NodeProgress
{
    NodeStatus status = NodeStatus::Dormant;
    uint64_t counter = 0;
    std::set<std::string> subjects;
    NodeIndex selected = NoNode;
    Timestamp activatedAt = 0, deadline = 0;
    bool receipt = false;
    uint32_t completedChildren = 0, failedChildren = 0;
    bool spawned = false;
    bool operator==(const NodeProgress &) const = default;
};
struct Instance
{
    InstanceId id = 0;
    std::shared_ptr<const CompiledDefinition> definition;
    MissionStatus status = MissionStatus::Active;
    std::vector<NodeProgress> nodes;
};
struct ActionKey
{
    CharacterId character = 0;
    std::string mission;
    uint32_t version = 0;
    InstanceId instance = 0;
    std::string node, reward;
    auto operator<=>(const ActionKey &) const = default;
};
struct Transition
{
    InstanceId instance = 0;
    std::string node;
    NodeStatus before = NodeStatus::Dormant, after = NodeStatus::Dormant;
};
enum class ResultCode
{
    Applied,
    NoChange,
    Duplicate,
    StaleEvent,
    WrongOwner,
    InvalidInput,
    MissingDefinition,
    Ineligible,
    InstanceExists,
    MissionActive,
    AlreadyCompleted,
    ChoiceConflict,
    WorkLimit,
    CapacityExceeded,
    InvalidTime
};
struct OperationResult
{
    ResultCode code = ResultCode::NoChange;
    uint64_t revision = 0;
    size_t steps = 0, matchedObjectives = 0;
    std::vector<Transition> transitions;
    std::vector<ActionKey> actions;
};
struct EventWindow
{
    uint64_t highest = 0;
    std::bitset<Limits::EventWindow> seen;
    ResultCode Check(uint64_t sequence) const;
    void Record(uint64_t sequence);
    bool operator==(const EventWindow &) const = default;
};

// Single authoritative owner thread. No live integration or reward delivery in
// this phase: ActionKey is a request that waits for a trusted durable receipt.
class MissionRuntime final
{
  public:
    MissionRuntime(const DefinitionRegistry &registry, CharacterSession owner, Timestamp now = 0,
                   size_t operationBudget = Limits::OperationSteps);
    MissionRuntime(const MissionRuntime &) = delete;
    MissionRuntime &operator=(const MissionRuntime &) = delete;

    OperationResult Start(std::string_view mission, uint32_t version, InstanceId id, Timestamp now);
    OperationResult Dispatch(const GameplayEvent &event, Timestamp now);
    OperationResult AdvanceTime(Timestamp now);
    OperationResult SetFlag(std::string flag, Timestamp now);
    OperationResult Acknowledge(const ActionKey &receipt, Timestamp now);
    OperationResult Abandon(InstanceId id, Timestamp now);
    // Immutable progress view; safe to retain across later mutations.
    std::shared_ptr<const Instance> Inspect(InstanceId id) const;
    const StoryFacts &Facts() const
    {
        CheckThread();
        return facts_;
    }
    uint64_t Revision() const
    {
        CheckThread();
        return revision_;
    }
    std::vector<ActionKey> PendingActions() const;
    std::optional<Timestamp> NextDeadline() const
    {
        CheckThread();
        return router_.NextDeadline();
    }
    size_t Subscriptions() const
    {
        CheckThread();
        return router_.EventSubscriptionCount();
    }

    // A complete, version-pinned in-memory checkpoint. SQL/CAS/outbox follows
    // in the persistence phase; exporting this object is not a durable commit.
    struct SavedInstance
    {
        InstanceId id = 0;
        std::string mission;
        uint32_t version = 0;
        MissionStatus status = MissionStatus::Active;
        std::vector<NodeProgress> nodes;
        bool operator==(const SavedInstance &) const = default;
    };
    struct Checkpoint
    {
        uint32_t format = 1;
        CharacterId character = 0;
        uint64_t revision = 0;
        Timestamp now = 0;
        StoryFacts facts;
        std::map<InstanceId, SavedInstance> instances;
        std::map<uint64_t, EventWindow> streams;
        bool operator==(const Checkpoint &) const = default;
    };
    Checkpoint Save() const;
    // Pure validation; does not restore, advance time or acknowledge actions.
    static bool ValidateCheckpoint(const DefinitionRegistry &registry, const Checkpoint &checkpoint);
    // All definitions must still exist at their exact versions. No latest-version fallback.
    OperationResult Restore(const Checkpoint &checkpoint, Timestamp now);

  private:
    struct Transaction;
    const DefinitionRegistry &registry_;
    CharacterSession owner_;
    std::thread::id thread_;
    Timestamp now_ = 0;
    size_t budget_;
    uint64_t revision_ = 0;
    size_t distinctSubjects_ = 0;
    StoryFacts facts_;
    std::map<InstanceId, std::shared_ptr<Instance>> instances_;
    std::map<uint64_t, EventWindow> streams_;
    MissionEventRouter router_;
    std::set<NodeToken> actions_;
    std::deque<NodeToken> queue_;
    std::set<NodeToken> queued_;
    Transaction *transaction_ = nullptr;
    OperationResult result_;

    void CheckThread() const;
    OperationResult Result(ResultCode code) const;
    OperationResult Mutate(Timestamp now, const std::function<void()> &operation);
    Instance &Edit(InstanceId id);
    void Spend(size_t count = 1);
    bool Test(const Condition &condition, const Definition &source);
    void Enqueue(NodeToken token, bool priority = false);
    void EnqueueDue();
    void Order(std::vector<NodeToken> &tokens) const;
    void Pump();
    void Process(NodeToken token);
    void Finish(NodeToken token, NodeStatus state);
    void Cancel(NodeToken token);
    void Flag(std::string flag);
    void Wake(const FactKey &key);
    void Index(InstanceId id, Timestamp observedAt);
    void RemoveIndex(InstanceId id);
    ActionKey Key(NodeToken token) const;
};
} // namespace auriga::mission
