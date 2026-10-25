#include "MissionValidator.hpp"
#include <algorithm>
#include <functional>
#include <stdexcept>

namespace auriga::mission
{
namespace
{
struct Invalid
{
    std::string path, message;
};
[[noreturn]] void Fail(std::string path, std::string message)
{
    throw Invalid{std::move(path), std::move(message)};
}
void Need(bool condition, const std::string &path, std::string message)
{
    if (!condition)
        Fail(path, std::move(message));
}
bool Known(EventKind k)
{
    return k >= EventKind::MobKilled && k <= EventKind::EscortCompleted;
}
bool Known(NodeKind k)
{
    return k >= NodeKind::Objective && k <= NodeKind::Recovery;
}

struct Check
{
    const Definition &source;
    size_t conditions = 0;
    size_t dependencyWork = 0;
    std::map<std::string, size_t> heights;
    std::set<std::string> visiting;
    std::map<std::string, Dependencies> cache;
    void SpendDependencies(size_t amount)
    {
        Need(amount <= Limits::DependencyWork - dependencyWork, "dependencies",
             "derived dependency work/memory limit");
        dependencyWork += amount;
    }

    void Shape(const Condition &c, const std::string &p, size_t depth = 0)
    {
        Need(++conditions <= Limits::Conditions && depth <= Limits::Depth, p, "condition work/depth limit");
        Need(c.kind >= ConditionKind::Always && c.kind <= ConditionKind::ExactlyN, p,
             "unknown condition kind");
        const bool aggregate = c.kind == ConditionKind::AllOf || c.kind == ConditionKind::AnyOf ||
                               c.kind == ConditionKind::AtLeastN || c.kind == ConditionKind::ExactlyN ||
                               c.kind == ConditionKind::Not || c.kind == ConditionKind::Xor;
        Need(aggregate || c.children.empty(), p, "leaf condition has children");
        Need(!aggregate || (!c.children.empty() && c.children.size() <= 64), p,
             "aggregate needs 1..64 operands");
        Need(c.kind == ConditionKind::AtLeastN || c.kind == ConditionKind::ExactlyN || c.required == 0, p,
             "misplaced threshold");
        if (c.kind == ConditionKind::AtLeastN)
            Need(c.required > 0 && c.required <= c.children.size(), p, "invalid threshold");
        if (c.kind == ConditionKind::ExactlyN)
            Need(c.required <= c.children.size(), p, "invalid exact threshold");
        if (c.kind == ConditionKind::Not || c.kind == ConditionKind::Xor)
            Need(c.children.size() == (c.kind == ConditionKind::Not ? 1 : 2), p,
                 "NOT needs one operand; XOR needs exactly two operands");
        const bool key = c.kind == ConditionKind::HasFlag || c.kind == ConditionKind::NotFlag ||
                         c.kind == ConditionKind::ChoiceIs || c.kind == ConditionKind::Subgraph;
        Need(key ? IsIdentifier(c.key) : c.key.empty(), p, "invalid/misplaced condition key");
        Need(c.kind == ConditionKind::ChoiceIs ? IsIdentifier(c.value) : c.value.empty(), p,
             "invalid/misplaced choice value");
        if (c.kind == ConditionKind::ChoiceIs)
        {
            auto domain = source.choices.find(c.key);
            Need(domain != source.choices.end() &&
                     std::find(domain->second.begin(), domain->second.end(), c.value) != domain->second.end(),
                 p, "choice value is not declared");
        }
        if (c.kind == ConditionKind::Subgraph)
            Need(source.predicates.contains(c.key), p, "missing predicate subgraph");
        if (c.kind == ConditionKind::TimeWindow)
            Need(c.beginsAt >= 0 && c.endsAt > c.beginsAt, p, "invalid half-open time window");
        else
            Need(c.beginsAt == 0 && c.endsAt == 0, p, "misplaced time bounds");
        for (const auto &child : c.children)
            Shape(child, p + "/children", depth + 1);
    }
    size_t Height(const Condition &c, size_t depth = 0)
    {
        Need(depth <= Limits::Depth, "conditions", "expanded condition depth limit");
        if (c.kind == ConditionKind::Subgraph)
        {
            if (auto found = heights.find(c.key); found != heights.end())
            {
                Need(depth + 1 + found->second <= Limits::Depth, "predicates/" + c.key,
                     "expanded condition depth limit");
                return 1 + found->second;
            }
            Need(visiting.insert(c.key).second, "predicates/" + c.key, "predicate subgraph cycle");
            const auto height = Height(source.predicates.at(c.key), depth + 1);
            visiting.erase(c.key);
            heights.emplace(c.key, height);
            Need(height < Limits::Depth, "predicates/" + c.key, "expanded condition depth limit");
            return 1 + height;
        }
        size_t height = 0;
        for (const auto &child : c.children)
            height = std::max(height, 1 + Height(child, depth + 1));
        Need(height <= Limits::Depth, "conditions", "expanded condition depth limit");
        return height;
    }
    Dependencies Deps(const Condition &c)
    {
        SpendDependencies(1);
        Dependencies d;
        if (c.kind == ConditionKind::HasFlag || c.kind == ConditionKind::NotFlag)
            d.facts.insert({FactKey::Kind::Flag, c.key});
        if (c.kind == ConditionKind::ChoiceIs)
            d.facts.insert({FactKey::Kind::Choice, c.key});
        if (c.kind == ConditionKind::TimeWindow)
        {
            d.boundaries.insert(c.beginsAt);
            d.boundaries.insert(c.endsAt);
        }
        if (c.kind == ConditionKind::Subgraph)
        {
            auto found = cache.find(c.key);
            if (found == cache.end())
            {
                auto dependency = Deps(source.predicates.at(c.key));
                SpendDependencies(dependency.facts.size() + dependency.boundaries.size());
                found = cache.emplace(c.key, std::move(dependency)).first;
            }
            SpendDependencies(found->second.facts.size() + found->second.boundaries.size());
            return found->second;
        }
        std::set<FactKey> thresholdFacts;
        for (size_t i = 0; i < c.children.size(); ++i)
        {
            const auto child = Deps(c.children[i]);
            SpendDependencies(child.facts.size() + child.boundaries.size());
            if (c.kind == ConditionKind::AtLeastN || c.kind == ConditionKind::ExactlyN)
            {
                for (size_t prior = 0; prior < i; ++prior)
                    Need(c.children[i] != c.children[prior], "at_least_n", "duplicate threshold operand");
                for (const auto &fact : child.facts)
                    Need(thresholdFacts.insert(fact).second, "at_least_n",
                         "threshold operands reuse the same fact source");
            }
            d.facts.insert(child.facts.begin(), child.facts.end());
            d.boundaries.insert(child.boundaries.begin(), child.boundaries.end());
        }
        return d;
    }
};
} // namespace

struct DefinitionCompiler
{
    static Compilation Run(const Definition &source)
    {
        Compilation result;
        try
        {
            Need(IsIdentifier(source.id) && source.version != 0, "mission",
                 "valid independent mission ID and nonzero version required");
            Need(!source.nodes.empty() && source.nodes.size() <= Limits::Nodes, "nodes", "node count limit");
            Need(source.predicates.size() <= Limits::Conditions && source.choices.size() <= Limits::Choices,
                 "definition", "table limit");
            Need(source.rewards.size() <= RewardLimits::Bundles, "rewards", "reward bundle count limit");
            size_t rewardItems = 0;
            for (const auto &[key, bundle] : source.rewards)
            {
                Need(IsIdentifier(key) && ValidRewardBundle(bundle), "rewards/" + key,
                     "invalid reward key, schema, item, amount or empty bundle");
                rewardItems += bundle.items.size() + bundle.consumeItems.size();
                Need(rewardItems <= RewardLimits::DefinitionItems, "rewards", "reward item memory limit");
            }
            for (const auto &[key, values] : source.choices)
            {
                Need(IsIdentifier(key) && values.size() >= 2 && values.size() <= Limits::ChoiceValues,
                     "choices/" + key, "choice needs a valid key and 2..32 options");
                std::set<std::string> unique;
                for (const auto &value : values)
                    Need(IsIdentifier(value) && unique.insert(value).second, "choices/" + key,
                         "invalid or duplicate choice option");
            }
            Check check{source};
            check.Shape(source.prerequisite, "prerequisite");
            for (const auto &[id, condition] : source.predicates)
            {
                Need(IsIdentifier(id), "predicates", "invalid predicate key");
                check.Shape(condition, "predicates/" + id);
            }
            std::map<std::string, NodeIndex, std::less<>> indices;
            for (NodeIndex i = 0; i < source.nodes.size(); ++i)
            {
                const auto &n = source.nodes[i];
                const auto p = "nodes/" + n.id;
                Need(IsIdentifier(n.id) && indices.emplace(n.id, i).second, p,
                     "invalid or duplicate node ID");
                Need(Known(n.kind), p, "unknown node kind");
                check.Shape(n.condition, p + "/condition");
                for (const auto &arm : n.branches)
                    check.Shape(arm.when, p + "/branches");
                Need(n.branches.size() <= Limits::Nodes, p, "branch count limit");
                const bool children = n.kind == NodeKind::Sequence || n.kind == NodeKind::Parallel ||
                                      n.kind == NodeKind::SubMission || n.kind == NodeKind::Recovery;
                Need(children || n.children.empty(), p, "children on a leaf/branch node");
                if (n.kind == NodeKind::Sequence || n.kind == NodeKind::Parallel)
                    Need(!n.children.empty(), p, "empty control node");
                if (n.kind == NodeKind::SubMission)
                    Need(n.children.size() == 1, p, "inline sub-mission needs exactly one child root");
                if (n.kind == NodeKind::Recovery)
                    Need(n.children.size() == 2, p, "recovery needs primary and recovery child roots");
                Need(n.kind == NodeKind::Branch || (n.branches.empty() && n.fallback.empty()), p,
                     "misplaced branch data");
                if (n.kind == NodeKind::Branch)
                    Need(!n.branches.empty() && IsIdentifier(n.fallback), p,
                         "branch needs arms and an explicit fallback");
                Need(n.parallel >= ParallelPolicy::All && n.parallel <= ParallelPolicy::FirstTerminal, p,
                     "unknown parallel policy");
                Need(n.kind == NodeKind::Parallel || n.parallel == ParallelPolicy::All, p,
                     "misplaced parallel policy");
                Need(n.kind == NodeKind::Timer ? n.duration > 0 : n.duration == 0, p,
                     "invalid/misplaced timer duration");
                Need(n.kind == NodeKind::Reward ? IsIdentifier(n.reward) : n.reward.empty(), p,
                     "invalid/misplaced reward action");
                Need(n.kind == NodeKind::Condition || n.condition == Condition{}, p, "misplaced condition");
                Need(n.completionFlags.size() <= Limits::Flags, p, "too many completion flags");
                std::set<std::string> flags;
                for (const auto &flag : n.completionFlags)
                    Need(IsIdentifier(flag) && flags.insert(flag).second, p,
                         "invalid/duplicate completion flag");
                Need(n.kind != NodeKind::Failure || flags.empty(), p,
                     "failure cannot grant completion flags");
                if (n.kind == NodeKind::Objective)
                {
                    const auto &o = n.objective;
                    Need(Known(o.event) && (o.subject == "*" || IsIdentifier(o.subject)), p,
                         "invalid event/subject");
                    Need(o.goal > 0 && o.goal <= Limits::Counter, p, "invalid objective goal");
                    Need(!o.distinctSubjects || o.goal <= Limits::Distinct, p,
                         "distinct-subject memory limit");
                    Need(!o.distinctSubjects || o.subject == "*" || o.goal == 1, p,
                         "a fixed subject cannot supply multiple distinct subjects");
                    if (o.event == EventKind::DialogueChoiceCommitted)
                        Need(source.choices.contains(o.subject) && o.goal == 1 && !o.distinctSubjects, p,
                             "choice objective needs one declared choice");
                }
                else
                    Need(n.objective.subject.empty() && n.objective.goal == 1 &&
                             !n.objective.distinctSubjects && n.objective.event == EventKind::MobKilled,
                         p, "misplaced objective data");
            }
            Need(indices.contains(source.root), "root", "missing root node");
            if (!source.rewards.empty())
            {
                std::set<std::string> used;
                for (const auto &node : source.nodes)
                    if (node.kind == NodeKind::Reward)
                    {
                        Need(source.rewards.contains(node.reward), "nodes/" + node.id,
                             "missing typed reward bundle");
                        used.insert(node.reward);
                    }
                Need(used.size() == source.rewards.size(), "rewards", "unused reward bundle");
            }
            // Resolve all references before copying/compiling the immutable payload.
            std::vector<std::vector<NodeIndex>> edges(source.nodes.size());
            std::vector<NodeIndex> parents(source.nodes.size(), NoNode);
            for (NodeIndex i = 0; i < source.nodes.size(); ++i)
            {
                auto children = source.nodes[i].children;
                if (source.nodes[i].kind == NodeKind::Branch)
                {
                    for (const auto &arm : source.nodes[i].branches)
                        children.push_back(arm.child);
                    children.push_back(source.nodes[i].fallback);
                }
                for (const auto &child : children)
                {
                    auto it = indices.find(child);
                    Need(it != indices.end(), "nodes/" + source.nodes[i].id, "missing child: " + child);
                    Need(parents[it->second] == NoNode, "nodes/" + child,
                         "shared child state/multiple parents");
                    parents[it->second] = i;
                    edges[i].push_back(it->second);
                }
            }
            const auto root = indices.at(source.root);
            Need(parents[root] == NoNode, "root", "root is referenced by a child (cycle)");
            std::vector<uint8_t> colours(source.nodes.size());
            std::vector<uint32_t> order(source.nodes.size());
            uint32_t ordinal = 0;
            std::function<void(NodeIndex, size_t)> visit = [&](NodeIndex i, size_t depth)
            {
                Need(depth <= Limits::Depth, "nodes/" + source.nodes[i].id, "control graph depth limit");
                Need(colours[i] != 1, "nodes/" + source.nodes[i].id, "control graph cycle");
                if (colours[i] == 2)
                    return;
                colours[i] = 1;
                order[i] = ordinal++;
                for (auto child : edges[i])
                    visit(child, depth + 1);
                colours[i] = 2;
            };
            visit(root, 0);
            for (NodeIndex i = 0; i < colours.size(); ++i)
                Need(colours[i] == 2, "nodes/" + source.nodes[i].id,
                     "unreachable node or disconnected cycle");
            check.Height(source.prerequisite);
            check.Deps(source.prerequisite);
            for (const auto &[key, condition] : source.predicates)
            {
                check.Height(condition);
                check.Deps(condition);
            }
            auto compiled = std::make_shared<CompiledDefinition>();
            compiled->source_ = source;
            compiled->root_ = root;
            compiled->indices_ = std::move(indices);
            for (NodeIndex i = 0; i < source.nodes.size(); ++i)
            {
                const auto &n = source.nodes[i];
                check.Height(n.condition);
                auto dependencies = check.Deps(n.condition);
                for (const auto &arm : n.branches)
                {
                    check.Height(arm.when);
                    const auto deps = check.Deps(arm.when);
                    dependencies.facts.insert(deps.facts.begin(), deps.facts.end());
                    dependencies.boundaries.insert(deps.boundaries.begin(), deps.boundaries.end());
                }
                compiled->dependencyEntries_ += dependencies.facts.size() + dependencies.boundaries.size();
                compiled->nodes_.push_back(
                    {n, parents[i], std::move(edges[i]), std::move(dependencies), order[i]});
            }
            result.definition = std::move(compiled);
        }
        catch (const Invalid &error)
        {
            result.errors.push_back({error.path, error.message});
        }
        return result;
    }
};
Compilation CompileDefinition(const Definition &source)
{
    return DefinitionCompiler::Run(source);
}
} // namespace auriga::mission
