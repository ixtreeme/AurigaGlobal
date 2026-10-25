#include "MissionRuntime.hpp"
#include <algorithm>

namespace auriga::mission
{
namespace
{
bool Known(NodeStatus s)
{
    return s >= NodeStatus::Dormant && s <= NodeStatus::Cancelled;
}
bool Known(MissionStatus s)
{
    return s >= MissionStatus::Active && s <= MissionStatus::Abandoned;
}
} // namespace
bool MissionRuntime::ValidCheckpoint(const Checkpoint &c) const
{
    if (c.format != 1 || c.now < 0 || c.revision == UINT64_MAX || c.instances.size() > Limits::Instances ||
        c.facts.flags.size() > Limits::Flags || c.facts.choices.size() > Limits::Flags ||
        c.streams.size() > Limits::Streams)
        return false;
    for (const auto &flag : c.facts.flags)
        if (!IsIdentifier(flag))
            return false;
    for (const auto &[key, value] : c.facts.choices)
        if (!IsIdentifier(key) || !IsIdentifier(value) || !registry_.IsChoiceValue(key, value))
            return false;
    for (const auto &[stream, window] : c.streams)
    {
        if (!stream || !window.highest || !window.seen[0])
            return false;
        if (window.highest < Limits::EventWindow)
            for (size_t bit = size_t(window.highest); bit < Limits::EventWindow; ++bit)
                if (window.seen[bit])
                    return false;
    }
    std::set<std::string> active;
    size_t distinct = 0;
    for (const auto &[id, saved] : c.instances)
    {
        if (!id || saved.id != id || !Known(saved.status))
            return false;
        const auto definition = registry_.Get(saved.mission, saved.version);
        if (!definition || saved.nodes.size() != definition->Nodes().size())
            return false;
        if (saved.status == MissionStatus::Active && !active.insert(saved.mission).second)
            return false;
        if (active.size() > Limits::ActiveInstances)
            return false;
        const auto root = saved.nodes[definition->Root()].status;
        if ((saved.status == MissionStatus::Active && root != NodeStatus::Active) ||
            (saved.status == MissionStatus::Completed && root != NodeStatus::Completed) ||
            (saved.status == MissionStatus::Failed && root != NodeStatus::Failed) ||
            (saved.status == MissionStatus::Abandoned && root != NodeStatus::Cancelled))
            return false;
        for (NodeIndex i = 0; i < saved.nodes.size(); ++i)
        {
            const auto &s = saved.nodes[i];
            const auto &compiled = definition->Nodes()[i];
            const auto &n = compiled.value;
            if (!Known(s.status))
                return false;
            distinct += s.subjects.size();
            if (distinct > Limits::DistinctPerCharacter)
                return false;
            if (s.status == NodeStatus::Dormant)
            {
                if (s != NodeProgress{})
                    return false;
            }
            else if (s.activatedAt < 0 || s.activatedAt > c.now)
                return false;
            if (s.status == NodeStatus::Active && compiled.parent != NoNode &&
                saved.nodes[compiled.parent].status != NodeStatus::Active)
                return false;
            if (s.status == NodeStatus::Completed)
                for (const auto &flag : n.completionFlags)
                    if (!c.facts.flags.contains(flag))
                        return false;
            if (n.kind != NodeKind::Parallel && (s.completedChildren || s.failedChildren || s.spawned))
                return false;
            if (n.kind == NodeKind::Objective)
            {
                if (s.counter > n.objective.goal || s.selected != NoNode || s.deadline || s.receipt ||
                    s.status == NodeStatus::Failed)
                    return false;
                if ((s.status == NodeStatus::Completed) != (s.counter == n.objective.goal))
                    return false;
                if (n.objective.distinctSubjects)
                {
                    if (s.subjects.size() != s.counter)
                        return false;
                    for (const auto &subject : s.subjects)
                        if (!IsIdentifier(subject) ||
                            (n.objective.subject != "*" && subject != n.objective.subject))
                            return false;
                }
                else if (!s.subjects.empty())
                    return false;
                if (s.status == NodeStatus::Completed &&
                    n.objective.event == EventKind::DialogueChoiceCommitted &&
                    !c.facts.choices.contains(n.objective.subject))
                    return false;
            }
            else if (s.counter || !s.subjects.empty())
                return false;
            if (n.kind == NodeKind::Timer && s.status != NodeStatus::Dormant)
            {
                if (s.activatedAt > INT64_MAX - n.duration || s.deadline != s.activatedAt + n.duration ||
                    s.status == NodeStatus::Failed)
                    return false;
                if ((s.status == NodeStatus::Completed && s.deadline > c.now) ||
                    (s.status == NodeStatus::Active && s.deadline <= c.now))
                    return false;
            }
            else if (s.deadline)
                return false;
            if (n.kind == NodeKind::Reward)
            {
                if (s.receipt != (s.status == NodeStatus::Completed) || s.status == NodeStatus::Failed)
                    return false;
            }
            else if (s.receipt)
                return false;
            if ((n.kind == NodeKind::Success && s.status != NodeStatus::Dormant &&
                 s.status != NodeStatus::Completed && s.status != NodeStatus::Cancelled) ||
                (n.kind == NodeKind::Failure && s.status != NodeStatus::Dormant &&
                 s.status != NodeStatus::Failed && s.status != NodeStatus::Cancelled))
                return false;
            if (n.kind == NodeKind::Condition && s.status != NodeStatus::Dormant &&
                s.status != NodeStatus::Completed && s.status != NodeStatus::Active &&
                s.status != NodeStatus::Cancelled)
                return false;
            if (n.kind == NodeKind::Condition && s.status == NodeStatus::Active)
            {
                // A settled checkpoint cannot leave a satisfied gate waiting.
                // Restore relies on indexed future boundaries, not a repair scan.
                const auto predicate = Evaluate(n.condition, definition->Source(), c.facts, c.now);
                if (predicate.error != Evaluation::Error::None || predicate.value)
                    return false;
            }
            const auto child = [&](size_t index) { return saved.nodes[compiled.children[index]].status; };
            if (s.status == NodeStatus::Dormant)
            {
                for (auto index : compiled.children)
                    if (saved.nodes[index].status != NodeStatus::Dormant)
                        return false;
                continue;
            }
            const auto selectedValid = [&](size_t count)
            {
                if (s.selected >= count)
                    return false;
                const auto chosen = child(s.selected);
                if (s.status == NodeStatus::Active)
                    return chosen == NodeStatus::Active;
                if (s.status == NodeStatus::Completed)
                    return chosen == NodeStatus::Completed;
                if (s.status == NodeStatus::Failed)
                    return chosen == NodeStatus::Failed;
                return chosen != NodeStatus::Active;
            };
            switch (n.kind)
            {
            case NodeKind::Sequence:
            case NodeKind::SubMission:
                if (s.selected > compiled.children.size())
                    return false;
                if (s.status == NodeStatus::Completed)
                {
                    if (s.selected != compiled.children.size())
                        return false;
                }
                else if (!selectedValid(compiled.children.size()))
                    return false;
                for (size_t k = 0; k < compiled.children.size(); ++k)
                {
                    if (k < s.selected && child(k) != NodeStatus::Completed)
                        return false;
                    if (k > s.selected && child(k) != NodeStatus::Dormant)
                        return false;
                }
                break;
            case NodeKind::Branch:
                if (!selectedValid(compiled.children.size()))
                    return false;
                for (size_t k = 0; k < compiled.children.size(); ++k)
                    if (k != s.selected && child(k) != NodeStatus::Dormant)
                        return false;
                break;
            case NodeKind::Recovery:
                if (!selectedValid(2) || (s.selected == 1 && child(0) != NodeStatus::Failed) ||
                    (s.selected == 0 && child(1) != NodeStatus::Dormant) ||
                    (s.status == NodeStatus::Failed && s.selected != 1))
                    return false;
                break;
            case NodeKind::Parallel:
            {
                if (!s.spawned || (n.parallel != ParallelPolicy::Race &&
                                   n.parallel != ParallelPolicy::FirstTerminal && s.selected != NoNode))
                    return false;
                size_t complete = 0, failed = 0, waiting = 0;
                for (auto index : compiled.children)
                {
                    const auto v = saved.nodes[index].status;
                    complete += v == NodeStatus::Completed;
                    failed += v == NodeStatus::Failed;
                    waiting += v == NodeStatus::Active;
                    if (s.status == NodeStatus::Active &&
                        (v == NodeStatus::Cancelled || v == NodeStatus::Dormant))
                        return false;
                }
                if (s.completedChildren != complete || s.failedChildren != failed)
                    return false;
                if (n.parallel == ParallelPolicy::Race)
                {
                    if (!complete)
                    {
                        if (s.selected != NoNode)
                            return false;
                    }
                    else if (s.selected >= compiled.children.size() ||
                             child(s.selected) != NodeStatus::Completed)
                        return false;
                }
                if (n.parallel == ParallelPolicy::FirstTerminal)
                {
                    if (complete + failed == 0)
                    {
                        if (s.selected != NoNode)
                            return false;
                    }
                    else if (s.selected >= compiled.children.size() ||
                             (child(s.selected) != NodeStatus::Completed &&
                              child(s.selected) != NodeStatus::Failed))
                        return false;
                }
                if (s.status == NodeStatus::Active)
                {
                    if (!waiting || (n.parallel == ParallelPolicy::All && failed) ||
                        (n.parallel == ParallelPolicy::Any && complete) ||
                        (n.parallel == ParallelPolicy::Race && complete) ||
                        (n.parallel == ParallelPolicy::FirstTerminal && (failed || complete)))
                        return false;
                }
                else
                {
                    if (waiting)
                        return false;
                    if (s.status == NodeStatus::Completed)
                    {
                        if ((n.parallel == ParallelPolicy::All && complete != compiled.children.size()) ||
                            (n.parallel == ParallelPolicy::Any && !complete) ||
                            (n.parallel == ParallelPolicy::Race && complete != 1) ||
                            (n.parallel == ParallelPolicy::FirstTerminal && (complete != 1 || failed)))
                            return false;
                    }
                    if (s.status == NodeStatus::Failed)
                    {
                        if ((n.parallel == ParallelPolicy::All && !failed) ||
                            (n.parallel == ParallelPolicy::Any && failed != compiled.children.size()) ||
                            (n.parallel == ParallelPolicy::Race && failed != compiled.children.size()) ||
                            (n.parallel == ParallelPolicy::FirstTerminal && (failed != 1 || complete)))
                            return false;
                    }
                }
                break;
            }
            default:
                if (s.selected != NoNode)
                    return false;
                break;
            }
        }
    }
    return true;
}
} // namespace auriga::mission
