#include "../../SRC/Server/GameServer/mission/MissionPackage.hpp"
#include "../../SRC/Server/GameServer/mission/MissionRuntime.hpp"
#include "../../SRC/Server/GameServer/mission/MissionValidator.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <random>
#include <source_location>
#include <stdexcept>

using namespace auriga::mission;
namespace
{
size_t checks = 0;
void Check(bool ok, std::string_view message,
           const std::source_location where = std::source_location::current())
{
    ++checks;
    if (!ok)
        throw std::runtime_error(std::string(message) + " at line " + std::to_string(where.line()));
}
bool Applied(const OperationResult &r)
{
    return r.code == ResultCode::Applied || r.code == ResultCode::NoChange;
}
PackageSource Fixture(uint32_t version = 1, uint64_t goal = 2)
{
    PackageSource p;
    p.manifest = {"demo.package", version, "hu", {}};
    Definition d;
    d.id = "demo.story";
    d.version = version;
    d.root = "root";
    Node root;
    root.id = "root";
    root.kind = NodeKind::Sequence;
    root.children = {"kill", "reward"};
    Node kill;
    kill.id = "kill";
    kill.kind = NodeKind::Objective;
    kill.objective = {EventKind::MobKilled, "demo.bandit", goal, false};
    kill.completionFlags = {"server.secret.clue"};
    Node reward;
    reward.id = "reward";
    reward.kind = NodeKind::Reward;
    reward.reward = "demo.reward";
    d.nodes = {root, kill, reward};
    p.missions.push_back(d);
    p.presentation.locales["hu"] = {{"title", "Proba"},
                                    {"summary", "Nyomozas"},
                                    {"kill", "Gyozd le a banditakat"},
                                    {"hidden", "Hidden reward"}};
    p.presentation.missions[d.id] = {"title", "summary", {{"kill", "kill"}, {"reward", "hidden"}}};
    return p;
}
PackageSource SourceOf(const MissionPackage &package)
{
    PackageSource p;
    p.manifest = package.Manifest();
    p.presentation = package.Presentation();
    for (const auto &graph : package.Graphs())
        p.missions.push_back(graph->Source());
    return p;
}
PackageLoad Stage(const PackageSource &p)
{
    const auto built = CompilePackage(p);
    if (!built)
        throw std::runtime_error("Fixture build: " + built.errors.front().message);
    auto loaded = LoadPackage(built.bytes);
    if (!loaded)
        throw std::runtime_error("Fixture load: " + loaded.errors.front().message);
    return loaded;
}
uint32_t U32(const std::vector<uint8_t> &b, size_t i)
{
    return uint32_t(b.at(i)) | uint32_t(b.at(i + 1)) << 8 | uint32_t(b.at(i + 2)) << 16 |
           uint32_t(b.at(i + 3)) << 24;
}
void Put32(std::vector<uint8_t> &b, size_t i, uint32_t value)
{
    for (size_t k = 0; k < 4; ++k)
        b.at(i + k) = uint8_t(value >> (8 * k));
}
void Rehash(std::vector<uint8_t> &b)
{
    std::fill(b.begin() + 24, b.begin() + 56, 0);
    std::array<uint8_t, 32> hash{};
    unsigned size = 0;
    Check(EVP_Digest(b.data(), b.size(), hash.data(), &size, EVP_sha256(), nullptr) == 1 && size == 32,
          "test digest failed");
    std::copy(hash.begin(), hash.end(), b.begin() + 24);
}
size_t SectionOffset(const std::vector<uint8_t> &b, uint16_t type)
{
    size_t p = 64;
    for (size_t i = 0; i < U32(b, 20); ++i)
    {
        if (uint16_t(b.at(p) | uint16_t(b.at(p + 1)) << 8) == type)
            return p;
        p += 12 + U32(b, p + 8);
    }
    throw std::runtime_error("test section missing");
}
void NativeTypesAndGatePolicies()
{
    std::set<uint32_t> ids;
    for (const auto &s : NativeSchemas())
    {
        Check(ids.insert(s.typeId).second && FindNativeSchema(s.typeId) == &s, "unstable/duplicate type ID");
        std::set<uint16_t> ports, params;
        for (const auto &p : s.ports)
            Check(ports.insert(p.id).second, "duplicate port ID");
        for (const auto &p : s.parameters)
            Check(params.insert(p.id).second, "duplicate parameter ID");
        Check(!s.gameplayBound, "schema falsely advertised a bound live producer");
    }
    Check(NativeType(NodeKind::Objective) == 0x1001 && NativeType(EventKind::MobKilled) == 0x3001,
          "stable native ID drift");
    Check(!FindNativeSchema(0x4001), "unimplemented action advertised as ready");
    const auto exported = nlohmann::json::parse(ExportNativeSchemas());
    Check(exported.at("scripting") == "none" && exported.at("types").size() == NativeSchemas().size(),
          "Studio schema omitted a native type or enabled scripts");
    Check(exported.at("graph_model") == "structured_tree_v1" &&
              !exported.at("arbitrary_port_edges").get<bool>(),
          "schema advertised unsupported arbitrary dataflow");
    Definition d;
    Condition gate;
    gate.kind = ConditionKind::AnyOf;
    for (const auto *key : {"method.smith", "method.spy", "method.runes"})
    {
        Condition flag;
        flag.kind = ConditionKind::HasFlag;
        flag.key = key;
        gate.children.push_back(flag);
    }
    for (unsigned bits = 0; bits < 8; ++bits)
    {
        StoryFacts facts;
        size_t count = 0;
        for (size_t i = 0; i < 3; ++i)
            if (bits & (1u << i))
            {
                facts.flags.insert(gate.children[i].key);
                ++count;
            }
        gate.kind = ConditionKind::AnyOf;
        gate.required = 0;
        Check(Evaluate(gate, d, facts, 0).value == (count > 0), "one-method gate mismatch");
        gate.kind = ConditionKind::AtLeastN;
        gate.required = 2;
        Check(Evaluate(gate, d, facts, 0).value == (count >= 2), "two-method gate mismatch");
        gate.kind = ConditionKind::ExactlyN;
        Check(Evaluate(gate, d, facts, 0).value == (count == 2), "EXACTLY_N mismatch");
        Condition inverse;
        inverse.kind = ConditionKind::Not;
        inverse.children = {gate};
        Check(Evaluate(inverse, d, facts, 0).value == (count != 2), "NOT mismatch");
        Condition exclusive;
        exclusive.kind = ConditionKind::Xor;
        exclusive.children = {gate.children[0], gate.children[1]};
        Check(Evaluate(exclusive, d, facts, 0).value == (bool(bits & 1) != bool(bits & 2)),
              "binary XOR mismatch");
        Check(Evaluate(inverse, d, facts, 0, 1).error == Evaluation::Error::WorkLimit,
              "NOT concealed a work-budget error");
    }
    auto bad = Fixture();
    bad.missions[0].prerequisite.kind = ConditionKind::Not;
    Check(!CompilePackage(bad), "empty NOT compiled");
    bad.missions[0].prerequisite.kind = ConditionKind::Xor;
    bad.missions[0].prerequisite.children = {Condition{}};
    Check(!CompilePackage(bad), "unary XOR compiled");
}
void CodecAndPresentation(const std::string &example)
{
    const auto source = ParsePackageSource(example);
    Check(bool(source), "authoring example rejected");
    const auto first = CompilePackage(source.source), second = CompilePackage(source.source);
    Check(bool(first) && first.bytes == second.bytes, "package output is nondeterministic");
    Check(first.bytes[0] == 'A' && first.bytes[1] == 'U' && first.bytes[64] == 1,
          "package is not the typed binary format");
    const auto loaded = LoadPackage(first.bytes);
    Check(bool(loaded), "compiled example did not load");
    Check(CompilePackage(SourceOf(*loaded.package)).bytes == first.bytes,
          "typed package roundtrip changed bytes");
    Check(loaded.package->Graphs().size() == source.source.missions.size(), "lost definition");
    for (const auto &graph : loaded.package->Graphs())
    {
        auto expected = std::find_if(source.source.missions.begin(), source.source.missions.end(),
                                     [&](const auto &d) { return d.id == graph->Source().id; });
        Check(expected != source.source.missions.end() && *expected == graph->Source(),
              "binary graph altered typed data");
    }
    auto p = Fixture();
    const auto staged = Stage(p);
    auto view = ProjectPresentation(*staged.package, "demo.story", "missing.locale", {"kill"});
    Check(view && view->locale == "hu" && view->objectives.size() == 1 && view->objectives[0].node == "kill",
          "presentation failed explicit allow-list/default locale");
    Check(ProjectPresentation(*staged.package, "demo.story", "hu", {})->objectives.empty(),
          "hidden nodes leaked without authorization");
    Check(!ProjectPresentation(*staged.package, "missing.mission", "hu", {"kill"}),
          "unknown mission exposed");
    p.presentation.locales["hu"]["kill"] = "Changed input";
    Check(staged.package->Presentation().locales.at("hu").at("kill") != "Changed input",
          "package aliases mutable input");
    Check(IsPresentationText("Arvizturo\n\t") && IsPresentationText("\xc5\x91\xc5\xb1") &&
              !IsPresentationText("\xc0\xaf") && !IsPresentationText("\xed\xa0\x80") &&
              !IsPresentationText("\xf4\x90\x80\x80") && !IsPresentationText(std::string(1, '\0')),
          "invalid UTF-8/control character accepted");
    auto json = nlohmann::json::parse(example);
    json["lua"] = "return os.execute('anything')";
    Check(!ParsePackageSource(json.dump()), "script payload accepted as source field");
    Check(!ParsePackageSource("{\"schema\":1,\"schema\":1}"), "duplicate source key accepted");
    Check(!ParsePackageSource(std::string(PackageLimits::Bytes + 1, ' ')), "oversized source accepted");
    auto invalid = Fixture();
    invalid.manifest.requiresTypes[0xabcdef] = 1;
    Check(!CompilePackage(invalid), "unimplemented capability compiled");
    invalid = Fixture();
    invalid.manifest.requiresTypes[0x1001] = 2;
    Check(!CompilePackage(invalid), "future required schema compiled");
    invalid = Fixture();
    invalid.presentation.locales["hu"].erase("title");
    Check(!CompilePackage(invalid), "missing translation compiled");
    invalid = Fixture();
    invalid.presentation.missions.begin()->second.objectives[0].node = "unknown";
    Check(!CompilePackage(invalid), "bad presentation node compiled");
}
void ActivationAndPinnedState()
{
    using Status = MissionPackageManager::Status;
    DefinitionRegistry registry;
    MissionPackageManager manager(registry, MissionPackageManager::Mode::DomainSimulation);
    Check(manager.Rollback().status == Status::NothingToRollback, "empty rollback accepted");
    const auto one = Stage(Fixture());
    DefinitionRegistry liveRegistry;
    MissionPackageManager live(liveRegistry);
    Check(live.Activate(one).status == Status::Invalid && !liveRegistry.Latest("demo.story"),
          "unbound gameplay event/reward capability activated outside simulation");
    Check(manager.Activate(one).status == Status::Activated &&
              registry.Latest("demo.story")->Source().version == 1,
          "initial activation failed");
    Check(manager.Activate(one).status == Status::NoChange, "same package activated twice");
    Check(registry.Publish(Fixture(2).missions.front()).status == Publication::Status::Invalid &&
              !registry.Get("demo.story", 2),
          "direct publication bypassed package activation");
    MissionRuntime old(registry, {7, 11});
    Check(Applied(old.Start("demo.story", 0, 1, 1000)), "old mission start failed");
    GameplayEvent event{{7, 11}, {1, 1}, 1000, EventKind::MobKilled, "demo.bandit", 1, {}};
    Check(Applied(old.Dispatch(event, 1000)), "old partial progress failed");
    auto sourceTwo = Fixture(2, 5);
    const auto two = Stage(sourceTwo);
    Check(manager.Activate(two).status == Status::Activated &&
              registry.Latest("demo.story")->Source().version == 2,
          "new activation failed");
    ++event.id.sequence;
    Check(Applied(old.Dispatch(event, 1000)) && old.PendingActions().size() == 1 &&
              old.Inspect(1)->definition->Source().version == 1,
          "live instance changed definition on activation");
    const auto action = old.PendingActions().front();
    Check(Applied(old.Acknowledge(action, 1000)) && old.Inspect(1)->status == MissionStatus::Completed,
          "pinned old reward barrier failed");
    MissionRuntime newer(registry, {7, 11});
    Check(Applied(newer.Start("demo.story", 0, 2, 1000)), "new mission start failed");
    Check(Applied(newer.Dispatch(event, 1000)) && newer.PendingActions().empty(),
          "new instance ignored new goal");
    auto conflict = Fixture(3, 999);
    conflict.missions[0].version = 2;
    auto extra = conflict.missions[0];
    extra.id = "a.new";
    extra.version = 1;
    conflict.missions.push_back(extra);
    conflict.presentation.missions["a.new"] = conflict.presentation.missions.at("demo.story");
    Check(manager.Activate(Stage(conflict)).status == Status::Invalid &&
              registry.Latest("demo.story")->Source().version == 2 && !registry.Get("a.new", 1) &&
              manager.Active()->Manifest().version == 2,
          "failed batch partly published or switched active package");
    auto corrupted = CompilePackage(Fixture(3));
    corrupted.bytes.back() ^= 1;
    Check(manager.Activate(LoadPackage(corrupted.bytes)).status == Status::Invalid &&
              manager.Active()->Manifest().version == 2,
          "corrupted staging replaced active package");
    Check(manager.Rollback().status == Status::Activated &&
              registry.Latest("demo.story")->Source().version == 1 && registry.Get("demo.story", 2) &&
              newer.Inspect(2)->definition->Source().version == 2,
          "rollback lost a retained version or used highest published instead of active");
    Check(manager.Activate(two).status == Status::VersionConflict,
          "implicit content rollback/replay accepted");
    auto third = Fixture(3, 7);
    third.missions[0].id = "other.story";
    third.presentation.missions["other.story"] = third.presentation.missions.at("demo.story");
    third.presentation.missions.erase("demo.story");
    Check(manager.Activate(Stage(third)).status == Status::Activated && !registry.Latest("demo.story") &&
              registry.Get("demo.story", 1),
          "catalog removal deleted pinned history or remained available to new starts");
    bool rejected = false;
    std::thread worker(
        [&]
        {
            try
            {
                manager.Rollback();
            }
            catch (const std::logic_error &)
            {
                rejected = true;
            }
        });
    worker.join();
    Check(rejected, "activation accepted a foreign thread");
}
void FirstSuccessfulRace()
{
    DefinitionRegistry registry;
    Definition d;
    d.id = "race.success";
    d.version = 1;
    d.root = "root";
    Node root;
    root.id = "root";
    root.kind = NodeKind::Parallel;
    root.parallel = ParallelPolicy::Race;
    root.children = {"fails", "wait"};
    Node fail;
    fail.id = "fails";
    fail.kind = NodeKind::Failure;
    Node wait;
    wait.id = "wait";
    wait.kind = NodeKind::Objective;
    wait.objective.subject = "mob.bandit";
    d.nodes = {root, fail, wait};
    Check(registry.Publish(d).status == Publication::Status::Published, "race fixture failed");
    MissionRuntime runtime(registry, {7, 11});
    Check(Applied(runtime.Start(d.id, 1, 1, 0)) && runtime.Inspect(1)->status == MissionStatus::Active &&
              runtime.Subscriptions() == 1,
          "first failed child incorrectly won success race");
    MissionRuntime reconnect(registry, {7, 12});
    Check(Applied(reconnect.Restore(runtime.Save(), 0)), "partly failed success race did not restore");
    GameplayEvent event{{7, 12}, {1, 1}, 0, EventKind::MobKilled, "mob.bandit", 1, {}};
    Check(Applied(reconnect.Dispatch(event, 0)) && reconnect.Inspect(1)->status == MissionStatus::Completed,
          "successful sibling could not win race");
    d.id = "race.all_fail";
    wait.kind = NodeKind::Failure;
    wait.objective = {};
    d.nodes = {root, fail, wait};
    Check(registry.Publish(d).status == Publication::Status::Published, "failed race fixture rejected");
    MissionRuntime allFailed(registry, {7, 11});
    Check(Applied(allFailed.Start(d.id, 1, 1, 0)) && allFailed.Inspect(1)->status == MissionStatus::Failed,
          "race with all failed children stayed active");
}
void MalformedAndMutationChecks()
{
    const auto valid = CompilePackage(Fixture()).bytes;
    for (size_t size = 0; size < valid.size(); ++size)
        Check(!LoadPackage(std::span(valid).first(size)), "truncated package accepted");
    auto bad = valid;
    bad[0] ^= 1;
    Check(!LoadPackage(bad), "wrong magic accepted");
    bad = valid;
    bad.back() ^= 1;
    Check(!LoadPackage(bad), "bad digest accepted");
    bad = valid;
    bad[8] = 2;
    Rehash(bad);
    Check(!LoadPackage(bad), "future format accepted");
    bad = valid;
    bad[16] = 3;
    Rehash(bad);
    Check(!LoadPackage(bad), "future graph schema accepted");
    bad = valid;
    Put32(bad, 12, UINT32_MAX);
    Rehash(bad);
    Check(!LoadPackage(bad), "overflow length accepted");
    const auto graphs = SectionOffset(valid, 2), requirements = SectionOffset(valid, 3);
    bad = valid;
    Put32(bad, graphs + 12, UINT32_MAX);
    Rehash(bad);
    Check(!LoadPackage(bad), "unbounded graph count accepted");
    bad = valid;
    Put32(bad, requirements + 16, 0xffffffff);
    Rehash(bad);
    Check(!LoadPackage(bad), "unknown required native type accepted");
    bad = valid;
    bad[graphs] = 99;
    Rehash(bad);
    Check(!LoadPackage(bad), "unknown mandatory section accepted");
    bad = valid;
    bad[graphs] = 1;
    Rehash(bad);
    Check(!LoadPackage(bad), "duplicate section accepted");
    bad = valid;
    Put32(bad, graphs + 4, 0);
    Rehash(bad);
    Check(!LoadPackage(bad), "optional downgrade of required graph section accepted");
    bad = valid;
    bad[56] = 1;
    Rehash(bad);
    Check(!LoadPackage(bad), "reserved header bit accepted");
    Check(!LoadPackage(std::vector<uint8_t>(PackageLimits::Bytes + 1)), "oversized package accepted");
    std::mt19937 random(20261025);
    for (size_t i = 0; i < 2000; ++i)
    {
        bad = valid;
        for (size_t k = 0, count = 1 + random() % 5; k < count; ++k)
            bad[64 + random() % (bad.size() - 64)] ^= uint8_t(1 + random() % 255);
        Rehash(bad);
        const auto loaded = LoadPackage(bad);
        if (loaded)
            Check(bool(CompilePackage(SourceOf(*loaded.package))),
                  "accepted mutated package cannot be recompiled");
        else
            Check(!loaded.errors.empty(), "invalid mutation lacked diagnostic");
    }
}
void DependencyLimits()
{
    auto d = Fixture().missions.front();
    d.nodes.clear();
    d.predicates.clear();
    Condition large;
    large.kind = ConditionKind::AllOf;
    for (size_t chunk = 0; chunk < 4; ++chunk)
    {
        Condition group;
        group.kind = ConditionKind::AllOf;
        for (size_t i = 0; i < 50; ++i)
        {
            Condition flag;
            flag.kind = ConditionKind::HasFlag;
            flag.key = "flag." + std::to_string(chunk * 50 + i);
            group.children.push_back(flag);
        }
        large.children.push_back(group);
    }
    d.predicates["large"] = large;
    for (size_t i = 0; i < 256; ++i)
    {
        Condition alias;
        alias.kind = ConditionKind::Subgraph;
        alias.key = "large";
        d.predicates["alias." + std::to_string(i)] = alias;
    }
    Node root;
    root.id = "root";
    root.kind = NodeKind::Sequence;
    d.nodes = {root};
    for (size_t i = 0; i < 511; ++i)
    {
        Node gate;
        gate.id = "gate." + std::to_string(i);
        gate.kind = NodeKind::Condition;
        gate.condition.kind = ConditionKind::Subgraph;
        gate.condition.key = "alias." + std::to_string(i % 256);
        d.nodes.front().children.push_back(gate.id);
        d.nodes.push_back(gate);
    }
    const auto limited = CompileDefinition(d);
    Check(!limited && limited.errors.front().path == "dependencies",
          "dependency alias amplification bypassed compile budget");

    // Each definition is small and valid; the total catalog still has its own bound.
    d.predicates.clear();
    large.children.clear();
    for (size_t i = 0; i < 16; ++i)
    {
        Condition flag;
        flag.kind = ConditionKind::HasFlag;
        flag.key = "f." + std::to_string(i);
        large.children.push_back(flag);
    }
    d.predicates["small"] = large;
    d.nodes.resize(201);
    d.nodes.front().children.resize(200);
    for (size_t i = 1; i < d.nodes.size(); ++i)
        d.nodes[i].condition.key = "small";
    Check(bool(CompileDefinition(d)), "valid catalog-budget fixture rejected");
    std::vector<Definition> catalog;
    for (size_t i = 0; i < 83; ++i)
    {
        d.id = "budget." + std::to_string(i);
        catalog.push_back(d);
    }
    DefinitionRegistry registry;
    Check(registry.Activate(catalog).status == Publication::Status::CapacityExceeded &&
              !registry.Latest("budget.0"),
          "catalog dependency capacity partly published oversized data");
}
} // namespace
int main(int argc, char **argv)
{
    try
    {
        if (argc != 2)
            throw std::runtime_error("example source path required");
        std::ifstream file(argv[1], std::ios::binary);
        std::string example((std::istreambuf_iterator<char>(file)), {});
        NativeTypesAndGatePolicies();
        CodecAndPresentation(example);
        ActivationAndPinnedState();
        FirstSuccessfulRace();
        DependencyLimits();
        MalformedAndMutationChecks();
        std::cout << "Mission package: " << checks << " checks passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
