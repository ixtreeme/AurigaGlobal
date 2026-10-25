#include "../../SRC/Server/GameServer/mission/MissionActions.hpp"
#include "../../SRC/Server/GameServer/mission/MissionCheckpointCodec.hpp"
#include "MissionPersistenceFixture.hpp"
#include <algorithm>
#include <iostream>
#include <random>
#include <source_location>

using namespace auriga::mission;
using namespace mission_fixture;
namespace
{
size_t checks = 0;
void Check(bool condition, std::string_view why,
           const std::source_location line = std::source_location::current())
{
    ++checks;
    if (!condition)
        throw std::runtime_error(std::string(why) + " at " + std::to_string(line.line()));
}
void Rehash(std::vector<uint8_t> &bytes)
{
    const ContentDigest zero{};
    auto digest = ContentHashParts({std::span(bytes).first(24), zero, std::span(bytes).subspan(56)});
    std::copy(digest.begin(), digest.end(), bytes.begin() + 24);
}
void RoundTripAndHistory()
{
    DefinitionRegistry warm;
    auto definitions = Definitions();
    Check(warm.Activate(definitions).status == Publication::Status::Published, "warm catalog rejected");
    CharacterSession owner{42, 17};
    MissionRuntime runtime(warm, owner, 1000);
    Progress(runtime, owner);
    const auto checkpoint = runtime.Save();
    const auto actions = runtime.PendingActions();
    Check(actions.size() == 1, "fixture reward not pending");
    const auto built = EncodeCheckpoint(checkpoint, warm);
    Check(bool(built) && built.metadata.definitions.size() == 2, "checkpoint failed to encode");
    Check(EncodeCheckpoint(checkpoint, warm).bytes == built.bytes, "nondeterministic checkpoint encoding");
    auto scan = ScanCheckpoint(built.bytes);
    Check(bool(scan) && scan.character == 42 && scan.revision == checkpoint.revision,
          "checkpoint scan metadata");
    DefinitionRegistry empty;
    Check(!DecodeCheckpoint(built.bytes, empty), "missing old definitions fell back");
    DefinitionRegistry latest;
    Check(latest.Activate(Definitions(2)).status == Publication::Status::Published, "new catalog rejected");
    Check(!DecodeCheckpoint(built.bytes, latest), "latest definition silently replaced version one");
    Check(latest.Retain(definitions).status == Publication::Status::Published &&
              latest.Latest("persist.story")->Source().version == 2,
          "history import changed active catalog");
    const auto loaded = DecodeCheckpoint(built.bytes, latest);
    Check(bool(loaded) && *loaded.checkpoint == checkpoint,
          "checkpoint round trip changed facts/progress/dedup");
    MissionRuntime resumed(latest, {42, 99}, checkpoint.now);
    Accepted(resumed.Restore(*loaded.checkpoint, checkpoint.now));
    Check(resumed.Save() == checkpoint && resumed.PendingActions() == actions,
          "cold restore changed reward identity or revision");
    Check(resumed.Inspect(10)->definition->Source().version == 1,
          "active instance upgraded its pinned definition");
    auto duplicate =
        resumed.Dispatch({{42, 99}, {9, 299}, 1001, EventKind::ItemCollected, "iron.c", 1, {}}, 1002);
    Check(duplicate.code == ResultCode::Duplicate, "event window not durable");
    Check(resumed.Dispatch({owner, {9, 302}, 1002, EventKind::ItemCollected, "iron.d", 1, {}}, 1002).code ==
              ResultCode::WrongOwner,
          "old session accepted after cold restore");
    Accepted(resumed.AdvanceTime(6001));
    Check(resumed.Inspect(20)->status == MissionStatus::Completed &&
              resumed.Facts().flags.contains("persist.clock_done"),
          "restored deadline did not complete");
    Accepted(resumed.Acknowledge(actions.front(), 6001));
    auto completed = EncodeCheckpoint(resumed.Save(), latest);
    Check(bool(completed) && bool(DecodeCheckpoint(completed.bytes, latest)),
          "completed reward receipt not serializable");

    DefinitionRegistry archiveOnly;
    auto history = Definitions(1);
    auto second = Definitions(2);
    history.insert(history.end(), second.begin(), second.end());
    Check(archiveOnly.Retain(history).status == Publication::Status::Published &&
              archiveOnly.Get("persist.story", 1) && archiveOnly.Get("persist.story", 2) &&
              !archiveOnly.Latest("persist.story"),
          "multiple historical versions activated or collided");
    auto changed = Definitions();
    ++changed[0].rewards.begin()->second.gold;
    DefinitionRegistry wrong;
    Check(wrong.Activate(changed).status == Publication::Status::Published,
          "changed definition fixture invalid");
    Check(MissionRuntime::ValidateCheckpoint(wrong, checkpoint) && !DecodeCheckpoint(built.bytes, wrong),
          "same ID/version with different content restored");
    auto newGraph = definitions[0];
    newGraph.id = "persist.new";
    Check(latest.Retain({newGraph, changed[0]}).status == Publication::Status::Invalid &&
              !latest.Get("persist.new", 1) && latest.Latest("persist.story")->Source().version == 2,
          "failed history import partly changed registry");
}
void InvalidAndMutated()
{
    DefinitionRegistry registry;
    Check(registry.Activate(Definitions()).status == Publication::Status::Published, "fixtures rejected");
    CharacterSession owner{42, 17};
    MissionRuntime runtime(registry, owner, 1000);
    Progress(runtime, owner);
    auto checkpoint = runtime.Save();
    auto built = EncodeCheckpoint(checkpoint, registry);
    Check(bool(built), "fixture encode");
    for (size_t size = 0; size < built.bytes.size(); ++size)
        Check(!ScanCheckpoint(std::span(built.bytes).first(size)), "truncated checkpoint accepted");
    auto bad = checkpoint;
    bad.character = 0;
    Check(!EncodeCheckpoint(bad, registry), "zero character encoded");
    bad = checkpoint;
    bad.revision = UINT64_MAX;
    Check(!EncodeCheckpoint(bad, registry), "overflow revision encoded");
    bad = checkpoint;
    bad.now = -1;
    Check(!EncodeCheckpoint(bad, registry), "negative time encoded");
    bad = checkpoint;
    bad.instances.at(10).nodes[1].counter = 100;
    Check(!EncodeCheckpoint(bad, registry), "invalid counter encoded");
    bad = checkpoint;
    bad.facts.choices["persist.road"] = "unknown";
    Check(!EncodeCheckpoint(bad, registry), "unknown choice encoded");
    bad = checkpoint;
    bad.instances.clear();
    bad.streams.clear();
    Check(!EncodeCheckpoint(bad, registry), "orphaned choice domain encoded");
    auto extra = built.bytes;
    extra.push_back(0);
    Check(!ScanCheckpoint(extra), "trailing bytes accepted");
    std::mt19937 random(0xa075c0de);
    for (size_t i = 0; i < 2500; ++i)
    {
        auto bytes = built.bytes;
        auto offset = size_t(random()) % bytes.size();
        bytes[offset] ^= uint8_t(1u << (random() % 8));
        if (i % 2 == 0)
            Rehash(bytes);
        auto loaded = DecodeCheckpoint(bytes, registry);
        if (loaded)
        {
            auto canonical = EncodeCheckpoint(*loaded.checkpoint, registry);
            Check(bool(canonical) && canonical.bytes == bytes,
                  "accepted checkpoint has noncanonical/ambiguous encoding");
        }
        else
            Check(!loaded.errors.empty(), "decoder failure has no diagnostic");
    }
    std::vector<uint8_t> excessive(CheckpointBytes + 1);
    Check(!ScanCheckpoint(excessive), "oversized checkpoint parsed");
}
void LargeCheckpoint()
{
    DefinitionRegistry registry;
    std::vector<Definition> definitions;
    for (size_t index = 0; index < 4; ++index)
    {
        Definition definition;
        definition.id = "bulk." + std::to_string(index);
        definition.version = 1;
        definition.root = "collect";
        auto objective = NodeOf("collect", NodeKind::Objective);
        objective.objective = {EventKind::MobKilled, "*", 2048, true};
        definition.nodes = {objective};
        definitions.push_back(definition);
    }
    Check(registry.Activate(definitions).status == Publication::Status::Published,
          "bulk definitions rejected");
    MissionRuntime::Checkpoint checkpoint;
    checkpoint.character = 42;
    checkpoint.revision = 1;
    for (size_t index = 0; index < definitions.size(); ++index)
    {
        NodeProgress progress;
        progress.status = NodeStatus::Completed;
        progress.counter = 2048;
        for (size_t key = 0; key < 2048; ++key)
            progress.subjects.insert("unique.subject." + std::to_string(key));
        checkpoint.instances.emplace(
            index + 1, MissionRuntime::SavedInstance{
                           index + 1, definitions[index].id, 1, MissionStatus::Completed, {progress}});
    }
    auto built = EncodeCheckpoint(checkpoint, registry);
    Check(bool(built) && built.bytes.size() > 100000, "large checkpoint not encoded");
    auto loaded = DecodeCheckpoint(built.bytes, registry);
    Check(bool(loaded) && *loaded.checkpoint == checkpoint, "large distinct checkpoint changed");
    checkpoint.instances.at(1).nodes[0].subjects.insert("overflow");
    Check(!EncodeCheckpoint(checkpoint, registry), "distinct subject capacity exceeded");
}
void ActionIdentities()
{
    DefinitionRegistry registry;
    Check(registry.Activate(Definitions()).status == Publication::Status::Published,
          "action definitions rejected");
    MissionRuntime runtime(registry, {42, 17}, 1000);
    Progress(runtime, {42, 17});
    auto batch = CheckpointActionBatch(runtime.Save(), registry);
    Check(bool(batch) && batch.actions.size() == 1 && !batch.actions[0].acknowledged &&
              batch.actions[0].key == runtime.PendingActions().front(),
          "action projection differs from runtime");
    const auto original = batch.actions.front();
    std::string golden;
    for (uint8_t byte : original.id)
    {
        golden += "0123456789abcdef"[byte >> 4];
        golden += "0123456789abcdef"[byte & 15];
    }
    Check(golden == "61015101d36be1429ab831495faa358506b922cd945a37e064c60ebf88e991ee",
          "stable action wire identity changed");
    std::set<ContentDigest> ids{original.id};
    for (size_t field = 0; field < 6; ++field)
    {
        auto key = original.key;
        switch (field)
        {
        case 0:
            ++key.character;
            break;
        case 1:
            key.mission += ".other";
            break;
        case 2:
            ++key.version;
            break;
        case 3:
            ++key.instance;
            break;
        case 4:
            key.node += ".other";
            break;
        case 5:
            key.reward += ".other";
            break;
        }
        Check(ids.insert(ActionIdentity(key)).second, "action identity omitted a tuple field");
    }
    auto left = original.key, right = original.key;
    left.node = "ab";
    left.reward = "c";
    right.node = "a";
    right.reward = "bc";
    Check(ActionIdentity(left) != ActionIdentity(right), "ambiguous action string boundaries");
    MissionRuntime resumed(registry, {42, 999}, 1002);
    Accepted(resumed.Restore(runtime.Save(), 1002));
    Check(CheckpointActionBatch(resumed.Save(), registry).actions.front().id == original.id,
          "action identity depends on session");
    Accepted(resumed.Acknowledge(original.key, 1003));
    batch = CheckpointActionBatch(resumed.Save(), registry);
    Check(bool(batch) && batch.actions.size() == 1 && batch.actions.front().acknowledged &&
              batch.actions.front().id == original.id,
          "acknowledgement changed action identity");
    Accepted(runtime.Abandon(10, 1003));
    Check(CheckpointActionBatch(runtime.Save(), registry).actions.empty(), "cancelled action stayed pending");

    Definition bulk;
    bulk.id = "bulk.actions";
    bulk.version = 1;
    bulk.root = "root";
    auto root = NodeOf("root", NodeKind::Parallel);
    root.parallel = ParallelPolicy::All;
    for (size_t i = 0; i < CheckpointActions + 1; ++i)
    {
        auto reward = NodeOf("reward." + std::to_string(i), NodeKind::Reward);
        reward.reward = "bulk.reward";
        root.children.push_back(reward.id);
        bulk.nodes.push_back(reward);
    }
    bulk.nodes.insert(bulk.nodes.begin(), root);
    DefinitionRegistry many;
    Check(many.Activate({bulk}).status == Publication::Status::Published, "bulk action graph invalid");
    MissionRuntime overflowing(many, {43, 1}, 0);
    Accepted(overflowing.Start(bulk.id, 1, 1, 0));
    auto excess = CheckpointActionBatch(overflowing.Save(), many);
    Check(!excess && excess.actions.empty(), "action capacity silently truncated a batch");
    bulk.nodes.pop_back();
    bulk.nodes.front().children.pop_back();
    DefinitionRegistry bounded;
    Check(bounded.Activate({bulk}).status == Publication::Status::Published, "bounded action graph invalid");
    MissionRuntime maximum(bounded, {43, 1}, 0);
    Accepted(maximum.Start(bulk.id, 1, 1, 0));
    batch = CheckpointActionBatch(maximum.Save(), bounded);
    Check(bool(batch) && batch.actions.size() == CheckpointActions, "exact action bound rejected");
}
} // namespace
int main()
{
    try
    {
        RoundTripAndHistory();
        InvalidAndMutated();
        LargeCheckpoint();
        ActionIdentities();
        std::cout << "Mission checkpoint: " << checks << " checks passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
