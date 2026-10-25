#include "MissionRuntime.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace auriga::mission
{
namespace
{
struct Abort
{
    ResultCode code;
};
bool Terminal(NodeStatus status)
{
    return status == NodeStatus::Completed || status == NodeStatus::Failed || status == NodeStatus::Cancelled;
}
bool Accepted(ResultCode code)
{
    return code == ResultCode::Applied || code == ResultCode::NoChange;
}
} // namespace
ResultCode EventWindow::Check(uint64_t sequence) const
{
    if (!sequence)
        return ResultCode::InvalidInput;
    if (sequence > highest)
        return ResultCode::Applied;
    const auto age = highest - sequence;
    if (age >= Limits::EventWindow)
        return ResultCode::StaleEvent;
    return seen[size_t(age)] ? ResultCode::Duplicate : ResultCode::Applied;
}
void EventWindow::Record(uint64_t sequence)
{
    if (Check(sequence) != ResultCode::Applied)
        throw std::logic_error("mission event was not eligible for dedup insertion");
    if (sequence > highest)
    {
        const auto advance = sequence - highest;
        if (advance >= Limits::EventWindow)
            seen.reset();
        else
            seen <<= size_t(advance);
        highest = sequence;
    }
    seen.set(size_t(highest - sequence));
}

struct MissionRuntime::Transaction
{
    MissionRuntime &runtime;
    Timestamp time;
    uint64_t revision;
    size_t distinct;
    std::map<InstanceId, std::shared_ptr<Instance>> originals;
    std::optional<StoryFacts> facts;
    std::optional<std::map<uint64_t, EventWindow>> streams;
    explicit Transaction(MissionRuntime &r)
        : runtime(r), time(r.now_), revision(r.revision_), distinct(r.distinctSubjects_)
    {
    }
    void Touch(InstanceId id)
    {
        if (originals.contains(id))
            return;
        auto found = runtime.instances_.find(id);
        if (found == runtime.instances_.end())
        {
            originals.emplace(id, nullptr);
            return;
        }
        originals.emplace(id, found->second);
        found->second = std::make_shared<Instance>(*found->second);
    }
    void Facts()
    {
        if (!facts)
            facts = runtime.facts_;
    }
    void Streams()
    {
        if (!streams)
            streams = runtime.streams_;
    }
    void Rollback()
    {
        for (const auto &[id, old] : originals)
            runtime.RemoveIndex(id);
        for (auto &[id, old] : originals)
        {
            if (old)
                runtime.instances_[id] = std::move(old);
            else
                runtime.instances_.erase(id);
        }
        if (facts)
            runtime.facts_ = std::move(*facts);
        if (streams)
            runtime.streams_ = std::move(*streams);
        runtime.now_ = time;
        runtime.revision_ = revision;
        runtime.distinctSubjects_ = distinct;
        for (const auto &[id, old] : originals)
            if (runtime.instances_.contains(id))
                runtime.Index(id, runtime.now_);
        runtime.queue_.clear();
        runtime.queued_.clear();
    }
};

MissionRuntime::MissionRuntime(const DefinitionRegistry &registry, CharacterSession owner, Timestamp now,
                               size_t budget)
    : registry_(registry), owner_(owner), thread_(std::this_thread::get_id()), now_(now), budget_(budget)
{
    if (!owner.character || !owner.session || now < 0 || !budget || budget > Limits::OperationSteps)
        throw std::invalid_argument("invalid mission runtime ownership/time/work budget");
}
void MissionRuntime::CheckThread() const
{
    if (std::this_thread::get_id() != thread_)
        throw std::logic_error("mission runtime used outside its authoritative owner thread");
}
OperationResult MissionRuntime::Result(ResultCode code) const
{
    OperationResult r;
    r.code = code;
    r.revision = revision_;
    return r;
}
std::shared_ptr<const Instance> MissionRuntime::Inspect(InstanceId id) const
{
    CheckThread();
    const auto found = instances_.find(id);
    return found == instances_.end() ? nullptr : found->second;
}
void MissionRuntime::Spend(size_t count)
{
    if (count > budget_ - result_.steps)
        throw Abort{ResultCode::WorkLimit};
    result_.steps += count;
}
Instance &MissionRuntime::Edit(InstanceId id)
{
    transaction_->Touch(id);
    return *instances_.at(id);
}
bool MissionRuntime::Test(const Condition &condition, const Definition &source)
{
    const auto result = Evaluate(condition, source, facts_, now_, budget_ - result_.steps);
    Spend(result.steps);
    if (result.error == Evaluation::Error::WorkLimit)
        throw Abort{ResultCode::WorkLimit};
    if (result.error != Evaluation::Error::None)
        throw std::logic_error("published mission predicate is invalid");
    return result.value;
}
OperationResult MissionRuntime::Mutate(Timestamp now, const std::function<void()> &operation)
{
    CheckThread();
    if (now < now_ || now < 0)
        return Result(ResultCode::InvalidTime);
    if (revision_ == UINT64_MAX)
        return Result(ResultCode::CapacityExceeded);
    if (transaction_)
        throw std::logic_error("reentrant mission mutation");
    Transaction transaction(*this);
    transaction_ = &transaction;
    result_ = {};
    result_.code = ResultCode::Applied;
    try
    {
        now_ = now;
        // Deadline boundaries win over input arriving at the same server time.
        EnqueueDue();
        Pump();
        operation();
        Pump();
        // A race can cancel an action activated earlier in this same operation.
        // Such an intent must never escape to a future reward/outbox service.
        std::erase_if(result_.actions,
                      [&](const ActionKey &key)
                      {
                          const auto index = instances_.at(key.instance)->definition->Find(key.node);
                          return !actions_.contains({key.instance, index});
                      });
        ++revision_;
        result_.revision = revision_;
        if (!result_.transitions.empty())
            result_.code = ResultCode::Applied;
        transaction_ = nullptr;
        return std::move(result_);
    }
    catch (const Abort &error)
    {
        const auto steps = result_.steps;
        transaction.Rollback();
        transaction_ = nullptr;
        auto result = Result(error.code);
        result.steps = steps;
        return result;
    }
    catch (...)
    {
        transaction.Rollback();
        transaction_ = nullptr;
        throw;
    }
}
void MissionRuntime::Enqueue(NodeToken token, bool priority)
{
    if (priority)
    {
        if (!queued_.insert(token).second)
            std::erase(queue_, token);
        queue_.push_front(token);
    }
    else if (queued_.insert(token).second)
        queue_.push_back(token);
}
void MissionRuntime::Order(std::vector<NodeToken> &tokens) const
{
    std::sort(tokens.begin(), tokens.end(),
              [&](NodeToken a, NodeToken b)
              {
                  if (a.instance != b.instance)
                      return a.instance < b.instance;
                  const auto &graph = instances_.at(a.instance)->definition->Nodes();
                  return graph[a.node].order < graph[b.node].order;
              });
}
void MissionRuntime::EnqueueDue()
{
    // A late scheduler wake-up may cover several different deadlines. Resolve
    // each earliest boundary before a later one can win a race. Equal deadlines
    // use authored tree order, independent of the flat node declaration order.
    for (auto deadline = router_.NextDeadline(); deadline && *deadline <= now_;
         deadline = router_.NextDeadline())
    {
        auto tokens = router_.Due(*deadline);
        Order(tokens);
        for (auto token : tokens)
            Enqueue(token);
        Pump();
    }
}
void MissionRuntime::Pump()
{
    while (!queue_.empty())
    {
        const auto token = queue_.front();
        queue_.pop_front();
        queued_.erase(token);
        Spend();
        Process(token);
    }
}
void MissionRuntime::Wake(const FactKey &fact)
{
    auto tokens = router_.MatchFact(fact);
    Order(tokens);
    for (auto token : tokens)
        Enqueue(token);
}
void MissionRuntime::Flag(std::string flag)
{
    if (facts_.flags.contains(flag))
        return;
    Spend();
    if (facts_.flags.size() >= Limits::Flags)
        throw Abort{ResultCode::CapacityExceeded};
    transaction_->Facts();
    facts_.flags.insert(flag);
    Wake({FactKey::Kind::Flag, std::move(flag)});
}
ActionKey MissionRuntime::Key(NodeToken token) const
{
    const auto &instance = *instances_.at(token.instance);
    const auto &source = instance.definition->Source();
    const auto &node = instance.definition->Nodes().at(token.node).value;
    return {owner_.character, source.id, source.version, token.instance, node.id, node.reward};
}
std::vector<ActionKey> MissionRuntime::PendingActions() const
{
    CheckThread();
    std::vector<ActionKey> result;
    for (auto token : actions_)
        result.push_back(Key(token));
    return result;
}
void MissionRuntime::RemoveIndex(InstanceId id)
{
    router_.RemoveInstance(id);
    for (auto it = actions_.lower_bound({id, 0}); it != actions_.end() && it->instance == id;)
        it = actions_.erase(it);
}
void MissionRuntime::Index(InstanceId id, Timestamp observedAt)
{
    const auto &instance = *instances_.at(id);
    if (instance.status != MissionStatus::Active)
        return;
    for (NodeIndex i = 0; i < instance.nodes.size(); ++i)
    {
        if (instance.nodes[i].status != NodeStatus::Active)
            continue;
        const NodeToken token{id, i};
        const auto &n = instance.definition->Nodes()[i];
        switch (n.value.kind)
        {
        case NodeKind::Objective:
            router_.WatchEvent(token, n.value.objective.event, n.value.objective.subject);
            break;
        case NodeKind::Condition:
            router_.WatchFacts(token, n.dependencies, observedAt);
            break;
        case NodeKind::Branch:
            if (instance.nodes[i].selected == NoNode)
                router_.WatchFacts(token, n.dependencies, observedAt);
            break;
        case NodeKind::Timer:
            router_.WatchDeadline(token, instance.nodes[i].deadline);
            break;
        case NodeKind::Reward:
            actions_.insert(token);
            break;
        default:
            break;
        }
    }
}
void MissionRuntime::Cancel(NodeToken token)
{
    const auto &current = instances_.at(token.instance)->nodes[token.node];
    if (current.status == NodeStatus::Dormant || Terminal(current.status))
        return;
    Spend();
    auto &instance = Edit(token.instance);
    auto &state = instance.nodes[token.node];
    const auto before = state.status;
    state.status = NodeStatus::Cancelled;
    router_.Remove(token);
    actions_.erase(token);
    result_.transitions.push_back(
        {token.instance, instance.definition->Nodes()[token.node].value.id, before, state.status});
    for (auto child : instance.definition->Nodes()[token.node].children)
        Cancel({token.instance, child});
}
void MissionRuntime::Finish(NodeToken token, NodeStatus status)
{
    auto &instance = Edit(token.instance);
    auto &state = instance.nodes[token.node];
    const auto &node = instance.definition->Nodes()[token.node];
    if (state.status != NodeStatus::Active)
        return;
    const auto before = state.status;
    state.status = status;
    router_.Remove(token);
    actions_.erase(token);
    result_.transitions.push_back({token.instance, node.value.id, before, status});
    for (auto child : node.children)
        Cancel({token.instance, child});
    if (status == NodeStatus::Completed)
        for (const auto &flag : node.value.completionFlags)
            Flag(flag);
    if (node.parent != NoNode)
    {
        const auto &parent = instance.definition->Nodes()[node.parent];
        if (parent.value.kind == NodeKind::Parallel)
        {
            auto &aggregate = instance.nodes[node.parent];
            if (status == NodeStatus::Completed)
                ++aggregate.completedChildren;
            else
                ++aggregate.failedChildren;
            if ((parent.value.parallel == ParallelPolicy::FirstTerminal ||
                 (parent.value.parallel == ParallelPolicy::Race && status == NodeStatus::Completed)) &&
                aggregate.selected == NoNode)
                aggregate.selected =
                    NodeIndex(std::find(parent.children.begin(), parent.children.end(), token.node) -
                              parent.children.begin());
        }
        Enqueue({token.instance, node.parent}, true);
    }
    else
        instance.status = status == NodeStatus::Completed ? MissionStatus::Completed : MissionStatus::Failed;
}
void MissionRuntime::Process(NodeToken token)
{
    auto found = instances_.find(token.instance);
    if (found == instances_.end() || found->second->status != MissionStatus::Active)
        return;
    const auto parent = found->second->definition->Nodes()[token.node].parent;
    if (parent != NoNode && found->second->nodes[parent].status != NodeStatus::Active)
        return;
    const auto status = found->second->nodes[token.node].status;
    if (Terminal(status))
        return;
    auto &instance = Edit(token.instance);
    auto &state = instance.nodes[token.node];
    const auto &compiled = instance.definition->Nodes()[token.node];
    const auto &n = compiled.value;
    if (state.status == NodeStatus::Dormant)
    {
        state.status = NodeStatus::Active;
        state.activatedAt = now_;
        result_.transitions.push_back({token.instance, n.id, NodeStatus::Dormant, NodeStatus::Active});
        if (n.kind == NodeKind::Sequence || n.kind == NodeKind::Recovery || n.kind == NodeKind::SubMission)
            state.selected = 0;
        if (n.kind == NodeKind::Timer)
        {
            if (n.duration > INT64_MAX - now_)
                throw Abort{ResultCode::InvalidTime};
            state.deadline = now_ + n.duration;
        }
        if (n.kind == NodeKind::Reward)
        {
            actions_.insert(token);
            result_.actions.push_back(Key(token));
        }
    }
    const auto childStatus = [&](NodeIndex index) { return instance.nodes[compiled.children[index]].status; };
    const auto activate = [&](NodeIndex index)
    {
        const NodeToken child{token.instance, compiled.children[index]};
        if (instance.nodes[child.node].status == NodeStatus::Dormant)
            Enqueue(child);
    };
    switch (n.kind)
    {
    case NodeKind::Objective:
        router_.WatchEvent(token, n.objective.event, n.objective.subject);
        break;
    case NodeKind::Condition:
        if (Test(n.condition, instance.definition->Source()))
            Finish(token, NodeStatus::Completed);
        else
            router_.WatchFacts(token, compiled.dependencies, now_);
        break;
    case NodeKind::Branch:
        if (state.selected == NoNode)
        {
            state.selected = NodeIndex(n.branches.size());
            for (NodeIndex i = 0; i < n.branches.size(); ++i)
                if (Test(n.branches[i].when, instance.definition->Source()))
                {
                    state.selected = i;
                    break;
                }
        }
        activate(state.selected);
        if (Terminal(childStatus(state.selected)))
            Finish(token, childStatus(state.selected) == NodeStatus::Completed ? NodeStatus::Completed
                                                                               : NodeStatus::Failed);
        break;
    case NodeKind::Sequence:
    case NodeKind::SubMission:
        while (state.selected < compiled.children.size() &&
               childStatus(state.selected) == NodeStatus::Completed)
        {
            Spend();
            ++state.selected;
        }
        if (state.selected == compiled.children.size())
            Finish(token, NodeStatus::Completed);
        else if (childStatus(state.selected) == NodeStatus::Failed ||
                 childStatus(state.selected) == NodeStatus::Cancelled)
            Finish(token, NodeStatus::Failed);
        else
            activate(state.selected);
        break;
    case NodeKind::Recovery:
        if (state.selected == 0 && childStatus(0) == NodeStatus::Failed)
            state.selected = 1;
        activate(state.selected);
        if (Terminal(childStatus(state.selected)))
            Finish(token, childStatus(state.selected) == NodeStatus::Completed ? NodeStatus::Completed
                                                                               : NodeStatus::Failed);
        break;
    case NodeKind::Parallel:
    {
        if (!state.spawned)
        {
            state.spawned = true;
            for (NodeIndex i = 0; i < compiled.children.size(); ++i)
            {
                Spend();
                activate(i);
            }
        }
        if ((n.parallel == ParallelPolicy::Race || n.parallel == ParallelPolicy::FirstTerminal) &&
            state.selected != NoNode)
        {
            Finish(token, childStatus(state.selected));
            return;
        }
        const auto completed = state.completedChildren, failed = state.failedChildren;
        if ((n.parallel == ParallelPolicy::All && completed == compiled.children.size()) ||
            (n.parallel == ParallelPolicy::Any && completed))
            Finish(token, NodeStatus::Completed);
        else if ((n.parallel == ParallelPolicy::All && failed) || failed == compiled.children.size())
            Finish(token, NodeStatus::Failed);
        break;
    }
    case NodeKind::Timer:
        if (now_ >= state.deadline)
            Finish(token, NodeStatus::Completed);
        else
            router_.WatchDeadline(token, state.deadline);
        break;
    case NodeKind::Reward:
        if (state.receipt)
            Finish(token, NodeStatus::Completed);
        break;
    case NodeKind::Success:
        Finish(token, NodeStatus::Completed);
        break;
    case NodeKind::Failure:
        Finish(token, NodeStatus::Failed);
        break;
    }
}
OperationResult MissionRuntime::Start(std::string_view mission, uint32_t version, InstanceId id,
                                      Timestamp now)
{
    CheckThread();
    if (!id || !IsIdentifier(mission))
        return Result(ResultCode::InvalidInput);
    if (instances_.contains(id))
        return Result(ResultCode::InstanceExists);
    auto definition = version ? registry_.Get(mission, version) : registry_.Latest(mission);
    if (!definition)
        return Result(ResultCode::MissingDefinition);
    return Mutate(now,
                  [&]
                  {
                      if (instances_.size() >= Limits::Instances)
                          throw Abort{ResultCode::CapacityExceeded};
                      size_t active = 0;
                      for (const auto &[key, instance] : instances_)
                          active += instance->status == MissionStatus::Active;
                      if (active >= Limits::ActiveInstances)
                          throw Abort{ResultCode::CapacityExceeded};
                      for (const auto &[key, instance] : instances_)
                          if (instance->definition->Source().id == mission)
                          {
                              if (instance->status == MissionStatus::Active)
                                  throw Abort{ResultCode::MissionActive};
                              if (!definition->Source().repeatable &&
                                  instance->status == MissionStatus::Completed)
                                  throw Abort{ResultCode::AlreadyCompleted};
                          }
                      if (!Test(definition->Source().prerequisite, definition->Source()))
                          throw Abort{ResultCode::Ineligible};
                      transaction_->Touch(id);
                      auto instance = std::make_shared<Instance>();
                      instance->id = id;
                      instance->definition = definition;
                      instance->nodes.resize(definition->Nodes().size());
                      instances_.emplace(id, std::move(instance));
                      Enqueue({id, definition->Root()});
                  });
}
OperationResult MissionRuntime::Dispatch(const GameplayEvent &event, Timestamp now)
{
    CheckThread();
    if (event.owner != owner_)
        return Result(ResultCode::WrongOwner);
    if (!event.id.stream || !event.id.sequence || !event.amount || event.occurredAt < 0 ||
        event.occurredAt > now || !IsIdentifier(event.subject) || event.kind < EventKind::MobKilled ||
        event.kind > EventKind::EscortCompleted ||
        (event.kind == EventKind::DialogueChoiceCommitted ? (!IsIdentifier(event.choice) || event.amount != 1)
                                                          : !event.choice.empty()))
        return Result(ResultCode::InvalidInput);
    auto stream = streams_.find(event.id.stream);
    if (stream != streams_.end())
    {
        const auto code = stream->second.Check(event.id.sequence);
        if (code != ResultCode::Applied)
            return Result(code);
    }
    else if (streams_.size() >= Limits::Streams)
        return Result(ResultCode::CapacityExceeded);
    return Mutate(
        now,
        [&]
        {
            // Capture subscriptions once. This event cannot feed a newly opened
            // next objective, including identical sequential kill/collect targets.
            auto matches = router_.Match(event.kind, event.subject);
            Order(matches);
            std::erase_if(
                matches, [&](NodeToken token)
                { return event.occurredAt < instances_.at(token.instance)->nodes[token.node].activatedAt; });
            result_.matchedObjectives = matches.size();
            if (matches.empty())
                result_.code = ResultCode::NoChange;
            if (!matches.empty() && event.kind == EventKind::DialogueChoiceCommitted)
            {
                const auto &domain =
                    instances_.at(matches.front().instance)->definition->Source().choices.at(event.subject);
                if (std::find(domain.begin(), domain.end(), event.choice) == domain.end())
                    throw Abort{ResultCode::InvalidInput};
                const auto current = facts_.choices.find(event.subject);
                if (current != facts_.choices.end() && current->second != event.choice)
                    throw Abort{ResultCode::ChoiceConflict};
                if (current == facts_.choices.end())
                {
                    if (facts_.choices.size() >= Limits::Flags)
                        throw Abort{ResultCode::CapacityExceeded};
                    transaction_->Facts();
                    facts_.choices.emplace(event.subject, event.choice);
                    Wake({FactKey::Kind::Choice, event.subject});
                }
            }
            for (auto token : matches)
            {
                Spend();
                auto &instance = Edit(token.instance);
                auto &state = instance.nodes[token.node];
                const auto &objective = instance.definition->Nodes()[token.node].value.objective;
                if (state.status != NodeStatus::Active)
                    continue;
                if (objective.distinctSubjects)
                {
                    if (!state.subjects.contains(event.subject))
                    {
                        if (distinctSubjects_ >= Limits::DistinctPerCharacter)
                            throw Abort{ResultCode::CapacityExceeded};
                        state.subjects.insert(event.subject);
                        ++state.counter;
                        ++distinctSubjects_;
                    }
                }
                else
                    state.counter += std::min(event.amount, objective.goal - state.counter);
                if (state.counter == objective.goal)
                    Finish(token, NodeStatus::Completed);
                // Resolve/cancel existing race branches before the next original
                // subscriber. Newly activated subscribers are not in this snapshot.
                Pump();
            }
            transaction_->Streams();
            streams_[event.id.stream].Record(event.id.sequence);
        });
}
OperationResult MissionRuntime::AdvanceTime(Timestamp now)
{
    CheckThread();
    if (now == now_ && router_.Due(now).empty())
        return Result(ResultCode::NoChange);
    return Mutate(now, [] {});
}
OperationResult MissionRuntime::SetFlag(std::string flag, Timestamp now)
{
    CheckThread();
    if (!IsIdentifier(flag))
        return Result(ResultCode::InvalidInput);
    if (facts_.flags.contains(flag))
        return AdvanceTime(now);
    return Mutate(now, [&] { Flag(std::move(flag)); });
}
OperationResult MissionRuntime::Acknowledge(const ActionKey &receipt, Timestamp now)
{
    CheckThread();
    if (receipt.character != owner_.character)
        return Result(ResultCode::WrongOwner);
    const auto found = instances_.find(receipt.instance);
    if (found == instances_.end())
        return Result(ResultCode::InvalidInput);
    const auto node = found->second->definition->Find(receipt.node);
    if (node == NoNode || found->second->definition->Nodes()[node].value.kind != NodeKind::Reward ||
        Key({receipt.instance, node}) != receipt)
        return Result(ResultCode::InvalidInput);
    if (found->second->nodes[node].receipt)
        return Result(ResultCode::Duplicate);
    if (!actions_.contains({receipt.instance, node}))
        return Result(ResultCode::InvalidInput);
    return Mutate(now,
                  [&]
                  {
                      // A deadline may have cancelled the request during the initial pump.
                      if (!actions_.contains({receipt.instance, node}))
                          throw Abort{ResultCode::InvalidInput};
                      Edit(receipt.instance).nodes[node].receipt = true;
                      Enqueue({receipt.instance, node});
                  });
}
OperationResult MissionRuntime::Abandon(InstanceId id, Timestamp now)
{
    CheckThread();
    auto found = instances_.find(id);
    if (found == instances_.end())
        return Result(ResultCode::InvalidInput);
    if (found->second->status != MissionStatus::Active)
        return Result(ResultCode::NoChange);
    return Mutate(now,
                  [&]
                  {
                      auto &instance = Edit(id);
                      Cancel({id, instance.definition->Root()});
                      instance.status = MissionStatus::Abandoned;
                  });
}
MissionRuntime::Checkpoint MissionRuntime::Save() const
{
    CheckThread();
    Checkpoint result;
    result.character = owner_.character;
    result.revision = revision_;
    result.now = now_;
    result.facts = facts_;
    result.streams = streams_;
    for (const auto &[id, instance] : instances_)
    {
        const auto &definition = instance->definition->Source();
        result.instances.emplace(
            id, SavedInstance{id, definition.id, definition.version, instance->status, instance->nodes});
    }
    return result;
}
OperationResult MissionRuntime::Restore(const Checkpoint &checkpoint, Timestamp now)
{
    CheckThread();
    if (checkpoint.character != owner_.character)
        return Result(ResultCode::WrongOwner);
    if (!instances_.empty() || !streams_.empty() || !facts_.flags.empty() || !facts_.choices.empty() ||
        revision_ || !ValidCheckpoint(checkpoint))
        return Result(ResultCode::InvalidInput);
    if (now < checkpoint.now || now < now_)
        return Result(ResultCode::InvalidTime);
    const auto result =
        Mutate(now,
               [&]
               {
                   transaction_->Facts();
                   transaction_->Streams();
                   facts_ = checkpoint.facts;
                   streams_ = checkpoint.streams;
                   for (const auto &[id, saved] : checkpoint.instances)
                   {
                       transaction_->Touch(id);
                       auto instance = std::make_shared<Instance>(Instance{
                           id, registry_.Get(saved.mission, saved.version), saved.status, saved.nodes});
                       instances_.emplace(id, std::move(instance));
                       // Retain boundaries crossed while offline, with the same
                       // deadline priority as a live scheduler wake-up.
                       Index(id, checkpoint.now);
                       for (const auto &state : saved.nodes)
                           distinctSubjects_ += state.subjects.size();
                   }
                   EnqueueDue();
               });
    if (!Accepted(result.code))
        return result;
    revision_ = checkpoint.revision + (now != checkpoint.now || !result.transitions.empty() ? 1 : 0);
    auto restored = result;
    restored.revision = revision_;
    return restored;
}
} // namespace auriga::mission
