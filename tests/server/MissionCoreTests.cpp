#include "../../SRC/Server/GameServer/mission/MissionRuntime.hpp"
#include "../../SRC/Server/GameServer/mission/MissionValidator.hpp"
#include <algorithm>
#include <atomic>
#include <iostream>
#include <random>
#include <source_location>
#include <stdexcept>

using namespace auriga::mission;
namespace
{
size_t checks = 0;
void Check(bool value, std::string_view message,
           const std::source_location where = std::source_location::current())
{
    ++checks;
    if (!value)
        throw std::runtime_error(std::string(message) + " at line " + std::to_string(where.line()));
}
bool Applied(const OperationResult &r)
{
    return r.code == ResultCode::Applied || r.code == ResultCode::NoChange;
}
Node Leaf(std::string id, NodeKind kind = NodeKind::Success)
{
    Node n;
    n.id = std::move(id);
    n.kind = kind;
    return n;
}
Node ObjectiveNode(std::string id, EventKind event = EventKind::MobKilled, std::string subject = "mob.bandit",
                   uint64_t goal = 1, bool distinct = false)
{
    auto n = Leaf(std::move(id), NodeKind::Objective);
    n.objective = {event, std::move(subject), goal, distinct};
    return n;
}
Node Control(std::string id, NodeKind kind, std::vector<std::string> children)
{
    auto n = Leaf(std::move(id), kind);
    n.children = std::move(children);
    return n;
}
Condition Flag(std::string key, bool present = true)
{
    Condition c;
    c.kind = present ? ConditionKind::HasFlag : ConditionKind::NotFlag;
    c.key = std::move(key);
    return c;
}
Condition Choice(std::string key, std::string value)
{
    Condition c;
    c.kind = ConditionKind::ChoiceIs;
    c.key = std::move(key);
    c.value = std::move(value);
    return c;
}
Condition Ref(std::string key)
{
    Condition c;
    c.kind = ConditionKind::Subgraph;
    c.key = std::move(key);
    return c;
}
Condition Aggregate(ConditionKind kind, std::vector<Condition> children, uint32_t required = 0)
{
    Condition c;
    c.kind = kind;
    c.children = std::move(children);
    c.required = required;
    return c;
}
Definition Def(std::string id, std::vector<Node> nodes, std::string root = "root")
{
    Definition d;
    d.id = std::move(id);
    d.version = 1;
    d.root = std::move(root);
    d.nodes = std::move(nodes);
    return d;
}
void Publish(DefinitionRegistry &registry, const Definition &definition)
{
    const auto r = registry.Publish(definition);
    if (r.status != Publication::Status::Published)
    {
        std::string details;
        for (const auto &error : r.errors)
            details += error.path + ": " + error.message + "; ";
        throw std::runtime_error("Publication rejected " + definition.id + ": " + details);
    }
}
GameplayEvent Event(uint64_t seq = 1, EventKind kind = EventKind::MobKilled,
                    std::string subject = "mob.bandit", uint64_t amount = 1, Timestamp time = 1000)
{
    return {{7, 11}, {1, seq}, time, kind, std::move(subject), amount, {}};
}
const NodeProgress &Progress(const MissionRuntime &runtime, InstanceId id, std::string_view node)
{
    const auto instance = runtime.Inspect(id);
    Check(instance != nullptr, "missing instance");
    const auto index = instance->definition->Find(node);
    Check(index != NoNode, "missing node");
    return instance->nodes[index];
}
void Roundtrip(const DefinitionRegistry &registry, const MissionRuntime &runtime)
{
    const auto before = runtime.Save();
    MissionRuntime restored(registry, {before.character, 99});
    Check(Applied(restored.Restore(before, before.now)), "valid checkpoint rejected");
    const auto after = restored.Save();
    Check(before.now == after.now && before.revision == after.revision && before.facts == after.facts &&
              before.streams == after.streams,
          "checkpoint lost time/revision/facts/replay protection");
    Check(before.instances.size() == after.instances.size(), "checkpoint lost instance");
    for (const auto &[id, instance] : before.instances)
    {
        const auto &other = after.instances.at(id);
        Check(instance.id == other.id && instance.mission == other.mission &&
                  instance.version == other.version && instance.status == other.status &&
                  instance.nodes == other.nodes,
              "checkpoint changed pinned progress");
    }
}

void ParsingAndValidation()
{
    constexpr std::string_view json =
        R"({"schema":1,"mission":"core.sample","version":1,"root":"root","nodes":[{"id":"root","kind":"objective","objective":{"event":"mob_killed","subject":"mob.bandit","goal":3}}]})";
    const auto parsed = ParseDefinition(json);
    Check(bool(parsed) && bool(CompileDefinition(parsed.value)), "valid JSON definition rejected");
    for (auto text : {R"({"schema":1,"schema":1})", R"({"schema":2})",
                      R"({"schema":1,"mission":"bad","version":-1,"root":"r","nodes":[]})"})
        Check(!ParseDefinition(text), "bad/duplicate schema field accepted");
    auto changed = std::string(json);
    changed.replace(changed.find("\"goal\":3"), 8, "\"goal\":1.5");
    Check(!ParseDefinition(changed), "fractional counter accepted");
    changed = std::string(json);
    changed.insert(changed.find("\"nodes\""), "\"progress_from_client\":7,");
    Check(!ParseDefinition(changed), "unknown field accepted");
    Check(!ParseDefinition(std::string(Limits::DefinitionBytes + 1, ' ')), "oversized input accepted");
    changed = std::string(json);
    changed.replace(changed.find("\"objective\""), 11, "\"failure\"");
    Check(!ParseDefinition(changed), "misplaced node payload accepted");
    std::string deep = "{}";
    for (size_t i = 0; i < 100; ++i)
        deep = "[" + deep + "]";
    Check(!ParseDefinition(deep), "excessive JSON nesting accepted");

    auto d = parsed.value;
    d.nodes[0].objective.goal = 0;
    Check(!CompileDefinition(d), "zero goal accepted");
    d = parsed.value;
    d.nodes.push_back(d.nodes[0]);
    Check(!CompileDefinition(d), "duplicate node ID accepted");
    d = Def("cycle", {Control("root", NodeKind::Sequence, {"root"})});
    Check(!CompileDefinition(d), "control cycle accepted");
    d = Def("missing", {Control("root", NodeKind::Sequence, {"unknown"})});
    Check(!CompileDefinition(d), "missing child accepted");
    d = Def("orphan", {Leaf("root"), Leaf("unused")});
    Check(!CompileDefinition(d), "unreachable node accepted");
    d = Def("shared", {Control("root", NodeKind::Parallel, {"leaf", "leaf"}), Leaf("leaf")});
    Check(!CompileDefinition(d), "shared child state accepted");
    d = parsed.value;
    d.prerequisite = Ref("a");
    d.predicates = {{"a", Ref("b")}, {"b", Ref("a")}};
    Check(!CompileDefinition(d), "predicate cycle accepted");
    d = parsed.value;
    d.predicates = {{"unused", Ref("missing")}};
    Check(!CompileDefinition(d), "invalid unused subgraph accepted");
    d = parsed.value;
    d.prerequisite = Aggregate(ConditionKind::AtLeastN, {Flag("same"), Flag("same")}, 2);
    Check(!CompileDefinition(d), "repeated evidence counted twice");
    d = parsed.value;
    d.predicates = {{"alias", Flag("same")}};
    d.prerequisite = Aggregate(ConditionKind::AtLeastN, {Flag("same"), Ref("alias")}, 2);
    Check(!CompileDefinition(d), "aliased evidence counted twice");
    d = parsed.value;
    d.nodes[0].objective.distinctSubjects = true;
    Check(!CompileDefinition(d), "impossible fixed-subject distinct goal accepted");
    d = Def("branch", {Leaf("root", NodeKind::Branch)});
    Check(!CompileDefinition(d), "branch without fallback accepted");
    d = Def("depth", {Leaf("end")}, "n0");
    for (size_t i = 0; i < Limits::Depth + 1; ++i)
        d.nodes.push_back(Control("n" + std::to_string(i), NodeKind::Sequence,
                                  {i == Limits::Depth ? "end" : "n" + std::to_string(i + 1)}));
    Check(!CompileDefinition(d), "deep graph accepted");
    d = parsed.value;
    for (size_t i = 0; i < 100; ++i)
        d.predicates["p" + std::to_string(i)] = i == 99 ? Condition{} : Ref("p" + std::to_string(i + 1));
    Check(!CompileDefinition(d), "deep predicate-reference chain accepted");
}
void Conditions()
{
    Definition d;
    d.choices = {{"allegiance", {"smith", "spy"}}};
    d.predicates = {{"evidence", Aggregate(ConditionKind::AtLeastN, {Flag("a"), Flag("b"), Flag("c")}, 2)}};
    StoryFacts facts;
    facts.flags = {"a", "c"};
    facts.choices = {{"allegiance", "smith"}};
    Check(Evaluate(Ref("evidence"), d, facts, 0).value, "AT_LEAST_N/SUBGRAPH failed");
    auto c = Aggregate(ConditionKind::AllOf,
                       {Ref("evidence"), Flag("missing", false), Choice("allegiance", "smith")});
    Check(Evaluate(c, d, facts, 0).value, "ALL_OF/NOT_FLAG/CHOICE_IS failed");
    Check(!Evaluate(Choice("not.chosen", "spy"), d, facts, 0).value, "missing choice treated as selected");
    c = Aggregate(ConditionKind::AnyOf, {Flag("missing"), Flag("c")});
    Check(Evaluate(c, d, facts, 0).value, "ANY_OF failed");
    Condition window;
    window.kind = ConditionKind::TimeWindow;
    window.beginsAt = 100;
    window.endsAt = 200;
    Check(!Evaluate(window, d, facts, 99).value && Evaluate(window, d, facts, 100).value &&
              !Evaluate(window, d, facts, 200).value,
          "time boundary is not half-open");
    Check(Evaluate(Ref("evidence"), d, facts, 0, 1).error == Evaluation::Error::WorkLimit,
          "predicate budget not enforced");
    d.predicates = {{"a", Ref("a")}};
    Check(Evaluate(Ref("a"), d, facts, 0).error == Evaluation::Error::InvalidDefinition,
          "unsafe predicate recursion");
}
void SequencingAndReplay()
{
    DefinitionRegistry registry;
    auto d =
        Def("sequence", {Control("root", NodeKind::Sequence, {"first", "second"}), ObjectiveNode("first"),
                         ObjectiveNode("second", EventKind::MobKilled, "mob.bandit", 2)});
    Publish(registry, d);
    MissionRuntime runtime(registry, {7, 11});
    Check(Applied(runtime.Start("sequence", 0, 1, 1000)) && runtime.Subscriptions() == 1,
          "sequence start failed");
    auto event = Event();
    event.amount = UINT64_MAX;
    Check(Applied(runtime.Dispatch(event, 1000)), "first kill rejected");
    Check(Progress(runtime, 1, "first").counter == 1 && Progress(runtime, 1, "second").counter == 0,
          "one event progressed a newly activated objective");
    Check(runtime.Dispatch(event, 1000).code == ResultCode::Duplicate, "duplicate event progressed sequence");
    Roundtrip(registry, runtime);
    event = Event(3);
    Check(Applied(runtime.Dispatch(event, 1000)) && Progress(runtime, 1, "second").counter == 1,
          "second objective did not count");
    event = Event(2);
    Check(Applied(runtime.Dispatch(event, 1000)) && runtime.Inspect(1)->status == MissionStatus::Completed,
          "out-of-order event was lost");
    Check(runtime.Subscriptions() == 0 &&
              runtime.Start("sequence", 1, 2, 1000).code == ResultCode::AlreadyCompleted,
          "completed mission restarted");
    Roundtrip(registry, runtime);

    MissionRuntime idle(registry, {7, 11});
    event = Event(1);
    Check(Applied(idle.Dispatch(event, 5000)) && idle.Subscriptions() == 0, "unmatched event failed");
    Check(Applied(idle.Start("sequence", 1, 1, 5000)), "late mission start failed");
    Check(idle.Dispatch(event, 5000).code == ResultCode::Duplicate && Progress(idle, 1, "first").counter == 0,
          "unmatched event replayed after start");
    event = Event(2);
    Check(Applied(idle.Dispatch(event, 5000)) && Progress(idle, 1, "first").counter == 0,
          "pre-activation event counted");
    event = Event(300);
    event.occurredAt = 5000;
    Check(Applied(idle.Dispatch(event, 5000)), "window advance failed");
    event = Event(3);
    event.occurredAt = 5000;
    Check(idle.Dispatch(event, 5000).code == ResultCode::StaleEvent, "event outside replay window accepted");
    Roundtrip(registry, idle);
}
void FilteringDistinctAndOwnership()
{
    DefinitionRegistry registry;
    auto exact = ObjectiveNode("exact");
    auto distinct = ObjectiveNode("distinct", EventKind::MobKilled, "*", 3, true);
    Publish(registry,
            Def("filters", {Control("root", NodeKind::Parallel, {"exact", "distinct"}), exact, distinct}));
    MissionRuntime runtime(registry, {7, 11});
    Check(Applied(runtime.Start("filters", 1, 1, 1000)), "filter start failed");
    auto event = Event();
    event.owner.character = 8;
    Check(runtime.Dispatch(event, 1000).code == ResultCode::WrongOwner, "wrong character accepted");
    event = Event();
    event.owner.session = 12;
    Check(runtime.Dispatch(event, 1000).code == ResultCode::WrongOwner, "retired session accepted");
    event = Event();
    event.amount = 0;
    Check(runtime.Dispatch(event, 1000).code == ResultCode::InvalidInput, "zero amount accepted");
    event = Event();
    event.occurredAt = 1001;
    Check(runtime.Dispatch(event, 1000).code == ResultCode::InvalidInput, "future event accepted");
    event = Event();
    event.kind = EventKind::ItemCollected;
    Check(runtime.Dispatch(event, 1000).matchedObjectives == 0, "wrong event kind dispatched");
    event = Event(2);
    Check(runtime.Dispatch(event, 1000).matchedObjectives == 2, "exact+wildcard dispatch failed");
    event = Event(3);
    Check(Applied(runtime.Dispatch(event, 1000)) && Progress(runtime, 1, "distinct").counter == 1,
          "distinct subject was counted twice");
    event = Event(4, EventKind::MobKilled, "mob.wolf", UINT64_MAX);
    Check(Applied(runtime.Dispatch(event, 1000)) && Progress(runtime, 1, "distinct").counter == 2,
          "distinct counted amount instead of subjects");
    event = Event(5, EventKind::MobKilled, "mob.bear");
    Check(Applied(runtime.Dispatch(event, 1000)) && runtime.Inspect(1)->status == MissionStatus::Completed,
          "parallel/distinct did not finish");
    Roundtrip(registry, runtime);
}
void VersionPinning()
{
    DefinitionRegistry registry;
    auto d = Def("versioned", {ObjectiveNode("root", EventKind::MobKilled, "mob.bandit", 2)});
    Publish(registry, d);
    MissionRuntime first(registry, {7, 11});
    Check(Applied(first.Start("versioned", 0, 1, 1000)), "version 1 start failed");
    d.nodes[0].objective.goal = 7;
    Check(registry.Publish(d).status == Publication::Status::VersionExists,
          "same version replaced live graph");
    d.version = 2;
    Publish(registry, d);
    d.nodes[0].objective.goal = 999;
    Check(first.Inspect(1)->definition->Source().version == 1 &&
              registry.Latest("versioned")->Nodes()[0].value.objective.goal == 7,
          "published definition aliases mutable input");
    auto event = Event();
    event.amount = 2;
    const auto before = first.Inspect(1);
    Check(Applied(first.Dispatch(event, 1000)) && first.Inspect(1)->status == MissionStatus::Completed,
          "hot publication changed active objective");
    Check(before->status == MissionStatus::Active && before->nodes[0].counter == 0 &&
              first.Inspect(1)->nodes[0].counter == 2,
          "retained progress view was mutated or invalidated");
    MissionRuntime second(registry, {7, 11});
    Check(Applied(second.Start("versioned", 0, 2, 1000)), "version 2 start failed");
    Check(Applied(second.Dispatch(event, 1000)) && Progress(second, 2, "root").counter == 2 &&
              second.Inspect(2)->status == MissionStatus::Active,
          "new instance did not use latest definition");
    Roundtrip(registry, first);
    Roundtrip(registry, second);
    DefinitionRegistry missing;
    auto only = d;
    only.nodes[0].objective.goal = 7;
    Publish(missing, only);
    MissionRuntime fresh(missing, {7, 22});
    Check(!Applied(fresh.Restore(first.Save(), 1000)), "checkpoint silently migrated to latest version");
}
Definition ChoiceGraph()
{
    auto branch = Leaf("test", NodeKind::Branch);
    branch.branches = {
        {Aggregate(ConditionKind::AllOf, {Choice("route", "smith"), Choice("suspect", "correct")}), "good"}};
    branch.fallback = "wrong";
    auto reward = Leaf("reward", NodeKind::Reward);
    reward.reward = "story.chapter1";
    reward.completionFlags = {"chapter.complete"};
    auto d =
        Def("choices", {Control("root", NodeKind::Sequence, {"route", "suspect", "recover", "reward"}),
                        ObjectiveNode("route", EventKind::DialogueChoiceCommitted, "route"),
                        ObjectiveNode("suspect", EventKind::DialogueChoiceCommitted, "suspect"),
                        Control("recover", NodeKind::Recovery, {"test", "investigate"}), branch, Leaf("good"),
                        Leaf("wrong", NodeKind::Failure),
                        ObjectiveNode("investigate", EventKind::PuzzleSolved, "evidence.recover"), reward});
    d.choices = {{"route", {"smith", "spy"}}, {"suspect", {"correct", "wrong"}}};
    return d;
}
void BranchesRecoveryAndReceipts()
{
    DefinitionRegistry registry;
    Publish(registry, ChoiceGraph());
    for (const auto &route : {"smith", "spy"})
        for (const auto &suspect : {"correct", "wrong"})
        {
            MissionRuntime runtime(registry, {7, 11});
            Check(Applied(runtime.Start("choices", 1, 1, 1000)), "choice graph start failed");
            auto event = Event(1, EventKind::DialogueChoiceCommitted, "route");
            event.choice = route;
            Check(Applied(runtime.Dispatch(event, 1000)), "legal route rejected");
            Roundtrip(registry, runtime);
            event = Event(2, EventKind::DialogueChoiceCommitted, "suspect");
            event.choice = suspect;
            Check(Applied(runtime.Dispatch(event, 1000)), "legal suspect choice rejected");
            if (runtime.PendingActions().empty())
            {
                Check(Progress(runtime, 1, "investigate").status == NodeStatus::Active,
                      "wrong accusation has no recovery");
                event = Event(3, EventKind::PuzzleSolved, "evidence.recover");
                Check(Applied(runtime.Dispatch(event, 1000)), "recovery event rejected");
            }
            Check(runtime.Inspect(1)->status == MissionStatus::Active && runtime.PendingActions().size() == 1,
                  "reward was granted without receipt or legal choice locked the route");
            const auto key = runtime.PendingActions().front();
            Roundtrip(registry, runtime);
            MissionRuntime reconnect(registry, {7, 12});
            Check(Applied(reconnect.Restore(runtime.Save(), 1000)) &&
                      reconnect.PendingActions() == std::vector<ActionKey>{key},
                  "pending action identity changed on reconnect");
            auto forged = key;
            forged.character = 8;
            Check(reconnect.Acknowledge(forged, 1000).code == ResultCode::WrongOwner,
                  "cross-character reward receipt accepted");
            forged = key;
            ++forged.version;
            Check(reconnect.Acknowledge(forged, 1000).code == ResultCode::InvalidInput,
                  "wrong definition reward receipt accepted");
            Check(Applied(reconnect.Acknowledge(key, 1000)) &&
                      reconnect.Inspect(1)->status == MissionStatus::Completed &&
                      reconnect.Facts().flags.contains("chapter.complete"),
                  "trusted receipt did not release completion barrier");
            Check(reconnect.Acknowledge(key, 1000).code == ResultCode::Duplicate &&
                      reconnect.PendingActions().empty(),
                  "reward receipt was applied twice");
            Check(reconnect.Dispatch(event, 1000).code == ResultCode::WrongOwner,
                  "old session survived reconnect");
            event.owner.session = 12;
            const auto revision = reconnect.Revision();
            Check(reconnect.Dispatch(event, 1000).code == ResultCode::Duplicate &&
                      reconnect.Revision() == revision,
                  "rebound session replayed an already processed event");
            Roundtrip(registry, reconnect);
        }
    auto invalid = ChoiceGraph();
    invalid.id = "other";
    invalid.choices["route"].push_back("new");
    Check(registry.Publish(invalid).status == Publication::Status::Invalid,
          "persistent choice domain changed silently");
}
void TimersRacesAndCancellation()
{
    DefinitionRegistry registry;
    auto timer = Leaf("timer", NodeKind::Timer);
    timer.duration = 100;
    auto objective = ObjectiveNode("fight");
    objective.completionFlags = {"fight.won"};
    auto d = Def("timeout", {Control("root", NodeKind::Recovery, {"race", "retry"}),
                             Control("race", NodeKind::Parallel, {"fight", "timeout"}), objective,
                             Control("timeout", NodeKind::Sequence, {"timer", "fail"}), timer,
                             Leaf("fail", NodeKind::Failure),
                             ObjectiveNode("retry", EventKind::NpcInteracted, "npc.healer")});
    d.nodes[1].parallel = ParallelPolicy::FirstTerminal;
    Publish(registry, d);
    MissionRuntime runtime(registry, {7, 11});
    Check(Applied(runtime.Start("timeout", 1, 1, 1000)) && runtime.NextDeadline() == 1100,
          "timer was not scheduled");
    Roundtrip(registry, runtime);
    auto event = Event();
    event.occurredAt = 1100;
    Check(Applied(runtime.Dispatch(event, 1100)) && !runtime.Facts().flags.contains("fight.won") &&
              Progress(runtime, 1, "retry").status == NodeStatus::Active,
          "late input won over deadline/recovery");
    Check(!runtime.NextDeadline() && runtime.Subscriptions() == 1, "timed-out branch retained subscriptions");
    Roundtrip(registry, runtime);
    event = Event(2, EventKind::NpcInteracted, "npc.healer", 1, 1100);
    Check(Applied(runtime.Dispatch(event, 1100)) && runtime.Inspect(1)->status == MissionStatus::Completed,
          "timeout recovery did not finish");
    Roundtrip(registry, runtime);

    auto first = Leaf("first");
    first.completionFlags = {"first.won"};
    auto second = Leaf("second");
    second.completionFlags = {"second.won"};
    d = Def("race.order", {Control("root", NodeKind::Parallel, {"first", "second"}), second, first});
    d.nodes[0].parallel = ParallelPolicy::Race;
    Publish(registry, d);
    MissionRuntime ordered(registry, {7, 11});
    Check(Applied(ordered.Start("race.order", 1, 1, 1000)) && ordered.Facts().flags.contains("first.won") &&
              !ordered.Facts().flags.contains("second.won"),
          "cancelled queued child ran or declaration order changed race priority");
    Roundtrip(registry, ordered);
    auto request = Leaf("request", NodeKind::Reward);
    request.reward = "cancelled.reward";
    d = Def("race.action", {Control("root", NodeKind::Parallel, {"request", "done"}), request, Leaf("done")});
    d.nodes[0].parallel = ParallelPolicy::Race;
    Publish(registry, d);
    MissionRuntime cancelled(registry, {7, 11});
    const auto result = cancelled.Start("race.action", 1, 1, 1000);
    Check(Applied(result) && result.actions.empty() && cancelled.PendingActions().empty(),
          "cancelled same-operation reward intent escaped to caller");
    Roundtrip(registry, cancelled);
    auto one = Leaf("one", NodeKind::Timer);
    one.duration = 10;
    one.completionFlags = {"one.won"};
    auto two = one;
    two.id = "two";
    two.completionFlags = {"two.won"};
    d = Def("race.timers", {Control("root", NodeKind::Parallel, {"one", "two"}), two, one});
    d.nodes[0].parallel = ParallelPolicy::Race;
    Publish(registry, d);
    MissionRuntime timers(registry, {7, 11});
    Check(Applied(timers.Start("race.timers", 1, 1, 1000)) && Applied(timers.AdvanceTime(1010)) &&
              timers.Facts().flags.contains("one.won") && !timers.Facts().flags.contains("two.won"),
          "simultaneous timers ignored authored race order");
    Roundtrip(registry, timers);
    MissionRuntime abandoned(registry, {7, 11});
    Check(Applied(abandoned.Start("timeout", 1, 1, 1000)) && Applied(abandoned.Abandon(1, 1000)),
          "abandon failed");
    Check(abandoned.Subscriptions() == 0 && !abandoned.NextDeadline() &&
              abandoned.Inspect(1)->status == MissionStatus::Abandoned,
          "abandon leaked event/timer subscriptions");
    Roundtrip(registry, abandoned);

    auto flagFirst = Leaf("first", NodeKind::Condition);
    flagFirst.condition = Flag("go");
    flagFirst.completionFlags = {"first.won"};
    auto flagSecond = flagFirst;
    flagSecond.id = "second";
    flagSecond.completionFlags = {"second.won"};
    d = Def("race.flags", {Control("root", NodeKind::Parallel, {"first", "second"}), flagSecond, flagFirst});
    d.nodes.front().parallel = ParallelPolicy::Race;
    Publish(registry, d);
    MissionRuntime flagRace(registry, {7, 11});
    Check(Applied(flagRace.Start(d.id, 1, 1, 1000)) && Applied(flagRace.SetFlag("go", 1000)) &&
              flagRace.Facts().flags.contains("first.won") && !flagRace.Facts().flags.contains("second.won"),
          "fact wake-up ignored authored race order");
    Roundtrip(registry, flagRace);

    // Late scheduler wake-ups and reconnects must preserve deadline priority.
    // Equal boundaries use authored child order, even for a shuffled node table.
    for (int variant = 0; variant < 3; ++variant)
    {
        const bool window = variant != 0;
        auto slow = Leaf("slow", window ? NodeKind::Condition : NodeKind::Timer);
        auto fast = Leaf("fast", slow.kind);
        slow.completionFlags = {"slow.won"};
        fast.completionFlags = {"fast.won"};
        if (window)
        {
            slow.condition.kind = fast.condition.kind = ConditionKind::TimeWindow;
            slow.condition.beginsAt = variant == 1 ? 1010 : 1040;
            fast.condition.beginsAt = 1010;
            slow.condition.endsAt = fast.condition.endsAt = 2000;
        }
        else
        {
            slow.duration = 40;
            fast.duration = 10;
        }
        d = Def("race.late." + std::to_string(variant),
                {Control("root", NodeKind::Parallel, {"slow", "fast"}), fast, slow});
        d.nodes.front().parallel = ParallelPolicy::Race;
        Publish(registry, d);
        MissionRuntime late(registry, {7, 11});
        Check(Applied(late.Start(d.id, 1, 1, 1000)), "late race fixture failed");
        const auto checkpoint = late.Save();
        const auto winner = variant == 1 ? "slow.won" : "fast.won";
        const auto loser = variant == 1 ? "fast.won" : "slow.won";
        Check(Applied(late.AdvanceTime(1100)) && late.Facts().flags.contains(winner) &&
                  !late.Facts().flags.contains(loser),
              "late scheduler wake-up lost deadline/child priority");
        Roundtrip(registry, late);
        MissionRuntime resumed(registry, {7, 22});
        Check(Applied(resumed.Restore(checkpoint, 1100)) && resumed.Facts().flags.contains(winner) &&
                  !resumed.Facts().flags.contains(loser),
              "offline restore lost deadline/child priority");
        Roundtrip(registry, resumed);
    }
}
void FactsGatesAndInlineSubgraphs()
{
    DefinitionRegistry registry;
    auto gate = Leaf("root", NodeKind::Condition);
    gate.condition = Flag("clue");
    Publish(registry, Def("gate", {gate}));
    auto objective = ObjectiveNode("root");
    objective.completionFlags = {"clue"};
    Publish(registry, Def("clue", {objective}));
    MissionRuntime runtime(registry, {7, 11});
    Check(Applied(runtime.Start("gate", 1, 1, 1000)) && Applied(runtime.Start("clue", 1, 2, 1000)),
          "cross-mission gate start failed");
    Check(runtime.Dispatch(Event(), 1000).matchedObjectives == 1 &&
              runtime.Inspect(1)->status == MissionStatus::Completed &&
              runtime.Inspect(2)->status == MissionStatus::Completed,
          "flag did not wake subscribed gate");
    Roundtrip(registry, runtime);
    gate.condition = {};
    gate.condition.kind = ConditionKind::TimeWindow;
    gate.condition.beginsAt = 100;
    gate.condition.endsAt = 200;
    Publish(registry, Def("window", {gate}));
    MissionRuntime timed(registry, {7, 11});
    Check(Applied(timed.Start("window", 1, 1, 0)) && timed.NextDeadline() == 100,
          "window gate was not scheduled");
    Check(Applied(timed.AdvanceTime(100)) && timed.Inspect(1)->status == MissionStatus::Completed &&
              !timed.NextDeadline(),
          "window gate missed opening boundary");
    Roundtrip(registry, timed);
    MissionRuntime offline(registry, {7, 11});
    Check(Applied(offline.Start("window", 1, 1, 0)), "offline gate start failed");
    MissionRuntime resumed(registry, {7, 22});
    Check(Applied(resumed.Restore(offline.Save(), 150)) &&
              resumed.Inspect(1)->status == MissionStatus::Completed,
          "offline window not re-evaluated on restore");
    auto inconsistent = offline.Save();
    inconsistent.now = 150;
    MissionRuntime invalidWindow(registry, {7, 22});
    Check(invalidWindow.Restore(inconsistent, 150).code == ResultCode::InvalidInput,
          "checkpoint retained an already satisfied active gate");
    MissionRuntime missedWindow(registry, {7, 22});
    Check(Applied(missedWindow.Restore(offline.Save(), 300)) &&
              missedWindow.Inspect(1)->status == MissionStatus::Active && !missedWindow.NextDeadline(),
          "offline restore awarded a time window that was already missed");
    Roundtrip(registry, missedWindow);
    auto d = Def("sub", {Control("root", NodeKind::SubMission, {"objective"}), ObjectiveNode("objective")});
    Publish(registry, d);
    MissionRuntime sub(registry, {7, 11});
    Check(Applied(sub.Start("sub", 1, 1, 1000)) && Applied(sub.Dispatch(Event(), 1000)) &&
              sub.Inspect(1)->status == MissionStatus::Completed,
          "inline sub-mission did not propagate completion");
    Roundtrip(registry, sub);
}
void AtomicFailureAndCorruptState()
{
    DefinitionRegistry registry;
    auto leaf = ObjectiveNode("root");
    leaf.completionFlags = {"a", "b"};
    Publish(registry, Def("budget", {leaf}));
    MissionRuntime runtime(registry, {7, 11}, 0, 2);
    Check(Applied(runtime.Start("budget", 1, 1, 1000)), "budget fixture start failed");
    const auto before = runtime.Save();
    Check(runtime.Dispatch(Event(), 1000).code == ResultCode::WorkLimit,
          "completion budget was not enforced");
    Check(runtime.Revision() == before.revision && runtime.Facts().flags.empty() &&
              Progress(runtime, 1, "root").counter == 0 && runtime.Subscriptions() == 1,
          "budget failure left partial counters/facts/routes");
    MissionRuntime retry(registry, {7, 11});
    Check(Applied(retry.Restore(runtime.Save(), 1000)) && Applied(retry.Dispatch(Event(), 1000)),
          "failed transaction consumed event ID");
    Roundtrip(registry, retry);
    auto broken = retry.Save();
    broken.instances.at(1).nodes[0].counter = 0;
    MissionRuntime invalid(registry, {7, 11});
    Check(invalid.Restore(broken, 1000).code == ResultCode::InvalidInput && !invalid.Inspect(1),
          "corrupt terminal counter restored");
    broken = before;
    broken.streams[1].highest = 1;
    broken.streams[1].seen.set(200);
    Check(invalid.Restore(broken, 1000).code == ResultCode::InvalidInput, "corrupt dedup window restored");
    broken = before;
    broken.character = 8;
    Check(invalid.Restore(broken, 1000).code == ResultCode::WrongOwner, "foreign checkpoint restored");
    auto d = Def("start.budget", {Control("root", NodeKind::Sequence, {"flag"}), Leaf("flag")});
    d.nodes[1].completionFlags = {"never"};
    Publish(registry, d);
    MissionRuntime small(registry, {7, 11}, 0, 1);
    Check(small.Start("start.budget", 1, 1, 1000).code == ResultCode::WorkLimit && !small.Inspect(1) &&
              small.Save().now == 0 && small.Revision() == 0,
          "failed start was partially committed");
    auto timer = Leaf("root", NodeKind::Timer);
    timer.duration = 10;
    Publish(registry, Def("overflow", {timer}));
    MissionRuntime overflow(registry, {7, 11});
    Check(overflow.Start("overflow", 1, 1, INT64_MAX - 1).code == ResultCode::InvalidTime &&
              !overflow.Inspect(1),
          "deadline overflow partially started mission");
    Check(runtime.AdvanceTime(999).code == ResultCode::InvalidTime, "runtime clock moved backward");
    std::atomic<bool> rejected = false;
    std::thread wrongThread(
        [&]
        {
            try
            {
                runtime.SetFlag("bad.thread", 1000);
            }
            catch (const std::logic_error &)
            {
                rejected = true;
            }
        });
    wrongThread.join();
    Check(rejected && !runtime.Facts().flags.contains("bad.thread"), "foreign thread mutated mission state");
}
void CapacityAndPublication()
{
    DefinitionRegistry registry;
    for (size_t i = 0; i <= Limits::ActiveInstances; ++i)
        Publish(registry, Def("capacity." + std::to_string(i),
                              {ObjectiveNode("root", EventKind::MobKilled, "mob." + std::to_string(i))}));
    MissionRuntime runtime(registry, {7, 11});
    for (size_t i = 0; i < Limits::ActiveInstances; ++i)
        Check(Applied(runtime.Start("capacity." + std::to_string(i), 1, i + 1, 1000)),
              "valid active-mission capacity rejected");
    Check(runtime.Start("capacity." + std::to_string(Limits::ActiveInstances), 1, 1000, 1000).code ==
              ResultCode::CapacityExceeded,
          "active mission quota ignored");
    auto event = Event(1, EventKind::MobKilled, "mob.0");
    Check(runtime.Dispatch(event, 1000).matchedObjectives == 1, "unrelated active missions received event");
    Roundtrip(registry, runtime);
    auto duplicate = Def("concurrent", {Leaf("root")});
    std::atomic<int> published = 0, exists = 0;
    const auto publish = [&]
    {
        const auto r = registry.Publish(duplicate);
        if (r.status == Publication::Status::Published)
            ++published;
        else if (r.status == Publication::Status::VersionExists)
            ++exists;
    };
    std::thread a(publish), b(publish);
    a.join();
    b.join();
    Check(published == 1 && exists == 1 && registry.Get("concurrent", 1) != nullptr,
          "concurrent publication replaced immutable version");
    EventWindow window;
    window.Record(UINT64_MAX);
    window.Record(UINT64_MAX - 1);
    Check(window.Check(UINT64_MAX) == ResultCode::Duplicate &&
              window.Check(UINT64_MAX - 1) == ResultCode::Duplicate &&
              window.Check(1) == ResultCode::StaleEvent && window.Check(0) == ResultCode::InvalidInput,
          "event sequence boundary overflow");

    // A single authoritative event can satisfy a broad parallel objective set
    // within a modest work budget, without rescanning every sibling per result.
    auto broad = Def("broad.parallel", {Control("root", NodeKind::Parallel, {})});
    for (size_t i = 0; i < 400; ++i)
    {
        const auto id = "objective." + std::to_string(i);
        broad.nodes.front().children.push_back(id);
        broad.nodes.push_back(ObjectiveNode(id));
    }
    Publish(registry, broad);
    MissionRuntime wide(registry, {7, 11}, 0, 2048);
    Check(Applied(wide.Start("broad.parallel", 1, 1, 1000)), "broad parallel activation exceeded budget");
    Check(Applied(wide.Dispatch(Event(), 1000)) && wide.Inspect(1)->status == MissionStatus::Completed,
          "broad parallel completion exceeded budget");
    Roundtrip(registry, wide);
}
void GeneratedCheckpointRegressions()
{
    // Fixed seeds make a broad set of nested control/terminal/cancellation
    // combinations reproducible. Every checkpoint must survive an exact restore.
    for (uint32_t seed = 1; seed <= 64; ++seed)
    {
        std::mt19937 random(seed);
        Definition d;
        d.id = "generated." + std::to_string(seed);
        d.version = 1;
        size_t ordinal = 0;
        std::function<std::string(unsigned)> generate = [&](unsigned depth)
        {
            const auto id = "node." + std::to_string(ordinal++);
            const unsigned kind = depth ? random() % 12 : random() % 6;
            Node n = Leaf(id);
            if (kind == 0)
                n = ObjectiveNode(id, EventKind::MobKilled, "mob." + std::to_string(random() % 3),
                                  1 + random() % 3);
            else if (kind == 1)
                n.kind = NodeKind::Failure;
            else if (kind == 2)
            {
                n.kind = NodeKind::Condition;
                n.condition = Flag("flag." + std::to_string(random() % 3), random() % 2 != 0);
            }
            else if (kind == 3)
            {
                n.kind = NodeKind::Timer;
                n.duration = 1 + random() % 30;
            }
            else if (kind == 4)
            {
                n.kind = NodeKind::Reward;
                n.reward = "reward." + id;
            }
            else if (kind >= 6)
            {
                if (kind == 6)
                {
                    n.kind = NodeKind::Sequence;
                    for (int i = 0; i < 3; ++i)
                        n.children.push_back(generate(depth - 1));
                }
                else if (kind == 7 || kind == 8 || kind == 9)
                {
                    n.kind = NodeKind::Parallel;
                    n.parallel = ParallelPolicy(kind - 7);
                    for (int i = 0; i < 3; ++i)
                        n.children.push_back(generate(depth - 1));
                }
                else if (kind == 10)
                {
                    n.kind = NodeKind::Recovery;
                    n.children = {generate(depth - 1), generate(depth - 1)};
                }
                else
                {
                    n.kind = NodeKind::Branch;
                    n.branches = {{Flag("flag.0"), generate(depth - 1)}};
                    n.fallback = generate(depth - 1);
                }
            }
            if (n.kind != NodeKind::Failure && random() % 4 == 0)
                n.completionFlags = {"flag." + std::to_string(random() % 3)};
            d.nodes.push_back(std::move(n));
            return id;
        };
        d.root = generate(3);
        std::shuffle(d.nodes.begin(), d.nodes.end(), random);
        DefinitionRegistry registry;
        Publish(registry, d);
        MissionRuntime runtime(registry, {7, 11});
        Check(Applied(runtime.Start(d.id, 1, 1, 0)), "generated graph did not start");
        Roundtrip(registry, runtime);
        Timestamp now = 0;
        for (uint64_t step = 1; step <= 24; ++step)
        {
            now += random() % 8;
            const auto action = random() % 4;
            OperationResult result;
            if (action == 0)
                result = runtime.AdvanceTime(now);
            else if (action == 1)
                result = runtime.SetFlag("flag." + std::to_string(random() % 3), now);
            else if (action == 2 && !runtime.PendingActions().empty())
                result = runtime.Acknowledge(runtime.PendingActions().front(), now);
            else
                result = runtime.Dispatch(Event(step, EventKind::MobKilled,
                                                "mob." + std::to_string(random() % 3), 1 + random() % 4, now),
                                          now);
            Check(Applied(result), "generated legal operation failed");
            Roundtrip(registry, runtime);
        }
        Check(Applied(runtime.Abandon(1, now)), "generated graph abandon failed");
        Roundtrip(registry, runtime);
    }
}
} // namespace
int main()
{
    try
    {
        for (const auto &[name, test] : std::vector<std::pair<const char *, void (*)()>>{
                 {"parsing/validation", ParsingAndValidation},
                 {"conditions", Conditions},
                 {"sequence/replay", SequencingAndReplay},
                 {"filtering/ownership", FilteringDistinctAndOwnership},
                 {"version pinning", VersionPinning},
                 {"choices/recovery/receipts", BranchesRecoveryAndReceipts},
                 {"timers/races/cancellation", TimersRacesAndCancellation},
                 {"facts/gates/subgraphs", FactsGatesAndInlineSubgraphs},
                 {"atomic failure/checkpoints", AtomicFailureAndCorruptState},
                 {"capacity/publication", CapacityAndPublication},
                 {"generated checkpoint regressions", GeneratedCheckpointRegressions}})
        {
            test();
            std::cout << "PASS " << name << '\n';
        }
        std::cout << "Mission core: " << checks << " checks passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
