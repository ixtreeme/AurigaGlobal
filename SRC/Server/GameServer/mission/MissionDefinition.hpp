#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

// Standalone domain kernel. No ECS registry, network, SQL, inventory or Lua VM.
namespace auriga::mission
{
using CharacterId = uint32_t;
using InstanceId = uint64_t;
using Timestamp = int64_t; // Authoritative UTC milliseconds; never client time.
using NodeIndex = uint32_t;
inline constexpr NodeIndex NoNode = UINT32_MAX;

struct Limits
{
    static constexpr size_t DefinitionBytes = 256 * 1024;
    static constexpr size_t Nodes = 512, Depth = 32, Conditions = 4096;
    static constexpr size_t IdentifierBytes = 96, Choices = 64, ChoiceValues = 32;
    static constexpr uint64_t Counter = 1'000'000'000, Distinct = 2048;
    static constexpr size_t Instances = 256, Flags = 4096, Streams = 32;
    static constexpr size_t ActiveInstances = 32, DistinctPerCharacter = 8192;
    static constexpr size_t OperationSteps = 16'384, EventWindow = 256;
    static constexpr size_t DependencyWork = 65'536;
    static constexpr size_t CatalogDependencies = 262'144, RegistryDependencies = 1'048'576;
};

bool IsIdentifier(std::string_view value);
enum class EventKind : uint8_t
{
    MobKilled,
    ItemCollected,
    ItemCrafted,
    NpcInteracted,
    AreaDiscovered,
    DialogueChoiceCommitted,
    BossDefeated,
    PuzzleSolved,
    EscortCompleted
};
enum class ConditionKind : uint8_t
{
    Always,
    AllOf,
    AnyOf,
    AtLeastN,
    HasFlag,
    NotFlag,
    ChoiceIs,
    TimeWindow,
    Subgraph,
    Not,
    Xor,
    ExactlyN
};
enum class NodeKind : uint8_t
{
    Objective,
    Condition,
    Branch,
    Sequence,
    Parallel,
    Timer,
    Reward,
    SubMission,
    Success,
    Failure,
    Recovery
};
enum class ParallelPolicy : uint8_t
{
    All = 0,
    Any = 1,
    Race = 2,         // First success; failure only when every child has failed.
    FirstTerminal = 3 // Explicit timeout/failure race, distinct from first-success Race.
};

struct Condition
{
    ConditionKind kind = ConditionKind::Always;
    std::vector<Condition> children;
    std::string key, value; // Flag, choice, or a named predicate subgraph.
    uint32_t required = 0;
    Timestamp beginsAt = 0, endsAt = 0; // Half-open UTC interval.
    bool operator==(const Condition &) const = default;
};
struct Objective
{
    EventKind event = EventKind::MobKilled;
    std::string subject; // Symbolic content key; "*" explicitly matches any subject.
    uint64_t goal = 1;
    bool distinctSubjects = false;
    bool operator==(const Objective &) const = default;
};
struct BranchArm
{
    Condition when;
    std::string child;
    bool operator==(const BranchArm &) const = default;
};
struct Node
{
    std::string id;
    NodeKind kind = NodeKind::Success;
    std::vector<std::string> children;
    Objective objective;
    Condition condition;
    std::vector<BranchArm> branches;
    std::string fallback; // Required for a branch: no silent unmatched-choice stall.
    ParallelPolicy parallel = ParallelPolicy::All;
    Timestamp duration = 0;
    std::string reward; // Symbolic action key, not an item or client-supplied amount.
    std::vector<std::string> completionFlags;
    bool operator==(const Node &) const = default;
};
struct Definition
{
    std::string id;
    uint32_t version = 0;
    std::string root;
    bool repeatable = false;
    Condition prerequisite;
    std::map<std::string, Condition> predicates;
    std::map<std::string, std::vector<std::string>> choices;
    std::vector<Node> nodes;
    bool operator==(const Definition &) const = default;
};

struct Diagnostic
{
    std::string path, message;
};
struct DefinitionParseResult
{
    Definition value;
    std::vector<Diagnostic> errors;
    explicit operator bool() const
    {
        return errors.empty();
    }
};
DefinitionParseResult ParseDefinition(std::string_view json);

struct FactKey
{
    enum class Kind : uint8_t
    {
        Flag,
        Choice
    };
    Kind kind = Kind::Flag;
    std::string name;
    auto operator<=>(const FactKey &) const = default;
};
struct Dependencies
{
    std::set<FactKey> facts;
    std::set<Timestamp> boundaries;
};
struct CompiledNode
{
    Node value;
    NodeIndex parent = NoNode;
    std::vector<NodeIndex> children;
    Dependencies dependencies;
    uint32_t order = 0; // Tree order, independent of the flat declaration order.
};
// Constructed only by the validator, with deep-owned definitions. Instances share it.
class CompiledDefinition final
{
  public:
    const Definition &Source() const
    {
        return source_;
    }
    const std::vector<CompiledNode> &Nodes() const
    {
        return nodes_;
    }
    NodeIndex Root() const
    {
        return root_;
    }
    NodeIndex Find(std::string_view id) const;
    size_t DependencyEntries() const
    {
        return dependencyEntries_;
    }

  private:
    friend struct DefinitionCompiler;
    Definition source_;
    std::vector<CompiledNode> nodes_;
    std::map<std::string, NodeIndex, std::less<>> indices_;
    NodeIndex root_ = NoNode;
    size_t dependencyEntries_ = 0;
};
struct Compilation
{
    std::shared_ptr<const CompiledDefinition> definition;
    std::vector<Diagnostic> errors;
    explicit operator bool() const
    {
        return definition != nullptr && errors.empty();
    }
};
struct Publication
{
    enum class Status
    {
        Published,
        Invalid,
        VersionExists,
        CapacityExceeded
    };
    Status status = Status::Invalid;
    std::vector<Diagnostic> errors;
};
class DefinitionRegistry final
{
  public:
    Publication Publish(const Definition &definition);
    // Whole-catalog activation. Retains exact old versions for running instances;
    // reusing an ID/version with different content or a choice domain is rejected.
    Publication Activate(const std::vector<Definition> &definitions);
    // Atomic history import. Does not activate archived versions or alter Latest().
    Publication Retain(const std::vector<Definition> &definitions);
    std::shared_ptr<const CompiledDefinition> Get(std::string_view id, uint32_t version) const;
    std::shared_ptr<const CompiledDefinition> Latest(std::string_view id) const;
    bool IsChoiceValue(std::string_view key, std::string_view value) const;

  private:
    Publication StoreCatalog(const std::vector<Definition> &definitions, bool activate);
    std::map<std::string, std::map<uint32_t, std::shared_ptr<const CompiledDefinition>>, std::less<>>
        definitions_;
    size_t versions_ = 0;
    size_t dependencyEntries_ = 0;
    std::map<std::string, std::set<std::string>> choices_;
    std::map<std::string, uint32_t, std::less<>> active_;
    mutable std::shared_mutex mutex_;
    bool catalogManaged_ = false;
};
} // namespace auriga::mission
