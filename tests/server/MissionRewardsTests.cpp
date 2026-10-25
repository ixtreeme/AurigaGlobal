#include "../../SRC/Server/GameServer/mission/MissionCheckpointCodec.hpp"
#include "../../SRC/Server/GameServer/mission/MissionRewardBinding.hpp"
#include "../../SRC/Server/GameServer/mission/MissionValidator.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <random>
#include <source_location>

using namespace auriga::mission;
using Json = nlohmann::json;
namespace
{
size_t checks = 0;
void Check(bool condition, std::string_view why, std::source_location where = std::source_location::current())
{
    ++checks;
    if (!condition)
        throw std::runtime_error(std::string(why) + " at " + std::to_string(where.line()));
}
std::vector<uint8_t> Read(const char *path)
{
    std::ifstream file(path, std::ios::binary);
    Check(bool(file), "fixture not found");
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
PackageSource SourceOf(const MissionPackage &package)
{
    PackageSource source;
    source.manifest = package.Manifest();
    source.presentation = package.Presentation();
    for (const auto &graph : package.Graphs())
        source.missions.push_back(graph->Source());
    return source;
}
std::string Hex(const ContentDigest &digest)
{
    std::string text;
    for (uint8_t byte : digest)
    {
        text += "0123456789abcdef"[byte >> 4];
        text += "0123456789abcdef"[byte & 15];
    }
    return text;
}
uint32_t Number(std::span<const uint8_t> bytes, size_t offset, size_t width)
{
    Check(offset + width <= bytes.size(), "fixture header truncated");
    uint32_t value = 0;
    for (size_t i = 0; i < width; ++i)
        value |= uint32_t(bytes[offset + i]) << (8 * i);
    return value;
}
void Put32(std::vector<uint8_t> &bytes, size_t offset, uint32_t value)
{
    for (size_t i = 0; i < 4; ++i)
        bytes.at(offset + i) = uint8_t(value >> (8 * i));
}
void Rehash(std::vector<uint8_t> &bytes)
{
    const ContentDigest zero{};
    const auto digest = ContentHashParts({std::span(bytes).first(24), zero, std::span(bytes).subspan(56)});
    std::copy(digest.begin(), digest.end(), bytes.begin() + 24);
}
void Legacy(const char *path)
{
    const auto bytes = Read(path);
    Check(Hex(ContentHash(bytes)) == "fd83bfeffc45e97d4bd8364fa6ccc17e7ffc0c811e3145a4030078a4f2b8a335",
          "frozen pre-upgrade package changed");
    const auto loaded = LoadPackage(bytes);
    Check(bool(loaded) && Number(bytes, 16, 2) == 1, "real old package no longer loads");
    Check(CompilePackage(SourceOf(*loaded.package)).bytes == bytes,
          "v1 compilation/fingerprint layout changed");
    auto graph = loaded.package->Graphs().front();
    size_t cursor = 64;
    bool found = false;
    while (cursor < bytes.size())
    {
        const auto type = Number(bytes, cursor, 2), length = Number(bytes, cursor + 8, 4);
        if (type == 2)
        {
            const auto graphSize = Number(bytes, cursor + 16, 4);
            Check(FingerprintDefinition(*graph) ==
                      ContentHash(std::span(bytes).subspan(cursor + 20, graphSize)),
                  "old graph pin changed from original persisted bytes");
            found = true;
        }
        cursor += 12 + length;
    }
    Check(found && graph->Source().rewards.empty(), "legacy rewards were fabricated");
    DefinitionRegistry live;
    RewardBindingPolicy attested;
    attested.atomicBundles = true;
    MissionPackageManager manager(live, MissionPackageManager::Mode::Gameplay,
                                  loaded.package->Manifest().requiresTypes, attested);
    Check(manager.Activate(loaded).status == MissionPackageManager::Status::Invalid,
          "verified handler IDs bypassed missing legacy payloads");
    for (const auto &node : graph->Nodes())
        if (node.value.kind == NodeKind::Reward)
            Check(!PinReward(
                      {42, graph->Source().id, graph->Source().version, 1, node.value.id, node.value.reward},
                      FingerprintDefinition(*graph), *graph),
                  "old symbolic reward acquired a fabricated payload");
}
void InvalidSources(const Json &source)
{
    auto invalid = [&](Json changed, bool parseOnly = true)
    {
        const auto parsed = ParsePackageSource(changed.dump());
        if (parseOnly)
            Check(!parsed, "malformed reward JSON accepted");
        else
            Check(!parsed || !CompilePackage(parsed.source), "invalid reward semantics compiled");
    };
    auto changed = source;
    changed["missions"][0]["schema"] = 1;
    invalid(changed);
    changed = source;
    changed["missions"][0]["schema"] = 3;
    invalid(changed);
    auto &base = source["missions"][0]["rewards"]["demo.bundle"];
    for (const auto &value : std::vector<Json>{0, -1, 50001, 1.5, "2", nullptr})
    {
        changed = source;
        changed["missions"][0]["rewards"]["demo.bundle"]["items"][0]["count"] = value;
        invalid(changed);
    }
    for (const auto &value : std::vector<Json>{-1, 1.5, "100", uint64_t(INT64_MAX) + 1})
    {
        changed = source;
        changed["missions"][0]["rewards"]["demo.bundle"]["gold"] = value;
        invalid(changed);
    }
    changed = source;
    changed["missions"][0]["rewards"]["demo.bundle"]["schema"] = 2;
    invalid(changed);
    changed = source;
    changed["missions"][0]["rewards"]["demo.bundle"] = {{"schema", 1}};
    invalid(changed);
    changed = source;
    changed["missions"][0]["rewards"]["demo.bundle"]["script"] = "give_item()";
    invalid(changed);
    changed = source;
    changed["missions"][0]["rewards"]["demo.bundle"]["items"][0]["socket0"] = 1;
    invalid(changed);
    changed = source;
    changed["missions"][0]["rewards"]["demo.bundle"]["items"].push_back(base["items"][0]);
    invalid(changed);
    changed = source;
    changed["missions"][0]["rewards"]["orphan"] = base;
    invalid(changed, false);
    changed = source;
    changed["missions"][0]["nodes"][0]["reward"] = "missing";
    invalid(changed, false);
    changed = source;
    changed["missions"][0]["rewards"]["demo.bundle"]["items"] = Json::object();
    invalid(changed);
    changed = source;
    changed["missions"][0]["rewards"]["demo.bundle"]["consume_items"][0]["vnum"] = 0;
    invalid(changed);
    std::string duplicate = source.dump();
    const auto key = duplicate.find("\"gold\":100");
    Check(key != duplicate.npos, "JSON fixture gold missing");
    duplicate.replace(key, 10, "\"gold\":100,\"gold\":200");
    Check(!ParsePackageSource(duplicate), "duplicate reward object field accepted");
}
void BindingsAndPins(const PackageSource &source, const PackageLoad &loaded)
{
    const auto graph = loaded.package->Graphs().front();
    const auto fingerprint = FingerprintDefinition(*graph);
    ActionKey key{42, graph->Source().id, graph->Source().version, 1, "reward", "demo.bundle"};
    const auto pin = PinReward(key, fingerprint, *graph);
    Check(bool(pin) && pin.reward->Payload() == source.missions[0].rewards.at(key.reward),
          "reward pin lost payload");
    auto wrong = fingerprint;
    wrong[0] ^= 1;
    Check(!PinReward(key, wrong, *graph), "wrong graph fingerprint pinned");
    for (size_t field = 0; field < 6; ++field)
    {
        auto bad = key;
        switch (field)
        {
        case 0:
            bad.character = 0;
            break;
        case 1:
            bad.mission += ".wrong";
            break;
        case 2:
            ++bad.version;
            break;
        case 3:
            bad.instance = 0;
            break;
        case 4:
            bad.node = "wrong";
            break;
        case 5:
            bad.reward = "wrong";
            break;
        }
        Check(!PinReward(bad, fingerprint, *graph), "wrong action key pinned");
    }
    auto changed = source;
    ++changed.missions[0].rewards.at(key.reward).items.begin()->second;
    auto other = CompileDefinition(changed.missions[0]);
    Check(bool(other) && FingerprintDefinition(*other.definition) != fingerprint &&
              !PinReward(key, fingerprint, *other.definition),
          "payload change escaped graph identity");
    auto otherPin = PinReward(key, FingerprintDefinition(*other.definition), *other.definition);
    Check(bool(otherPin) && otherPin.reward->PayloadDigest() != pin.reward->PayloadDigest() &&
              pin.reward->Payload().items.at(50011) == 2,
          "pinned payload changed or hash omitted item count");
    for (size_t field = 0; field < 4; ++field)
    {
        auto bundle = pin.reward->Payload();
        if (field == 0)
            ++bundle.experience;
        if (field == 1)
            ++bundle.gold;
        if (field == 2)
            ++bundle.consumeItems.begin()->second;
        if (field == 3)
            bundle.items.emplace(50014, 1);
        Check(FingerprintReward(bundle) != pin.reward->PayloadDigest(),
              "payload hash omitted a reward field");
    }
    DefinitionRegistry original, altered;
    Check(original.Activate(source.missions).status == Publication::Status::Published &&
              altered.Activate(changed.missions).status == Publication::Status::Published,
          "pin registries invalid");
    MissionRuntime runtime(original, {42, 1}, 0);
    Check(runtime.Start(key.mission, key.version, 1, 0).code == ResultCode::Applied, "reward start failed");
    auto checkpoint = EncodeCheckpoint(runtime.Save(), original);
    Check(bool(checkpoint) && !DecodeCheckpoint(checkpoint.bytes, altered),
          "checkpoint accepted changed reward bytes");
    Check(original.Activate(changed.missions).status == Publication::Status::Invalid,
          "same graph version accepted different reward payload");
    changed.missions[0].version = 2;
    changed.manifest.version = 2;
    Check(original.Activate(changed.missions).status == Publication::Status::Published &&
              runtime.Inspect(1)->definition->Source().version == 1 &&
              pin.reward->Payload().items.at(50011) == 2,
          "activation changed old reward obligation");

    RewardBindingPolicy policy;
    policy.items = {
        {50011, {RewardLimits::ItemCount, 0}}, {50012, {1, 0}}, {50013, {0, RewardLimits::ItemCount}}};
    policy.maxExperience = RewardLimits::Amount;
    policy.maxGold = RewardLimits::Amount;
    policy.atomicBundles = true;
    using Mode = MissionPackageManager::Mode;
    using Status = MissionPackageManager::Status;
    const auto required = loaded.package->Manifest().requiresTypes;
    const auto accept = [&](NativeRequirements capabilities, RewardBindingPolicy bindings)
    {
        DefinitionRegistry registry;
        MissionPackageManager manager(registry, Mode::Gameplay, std::move(capabilities), std::move(bindings));
        return manager.Activate(loaded).status;
    };
    Check(accept(required, policy) == Status::Activated, "verified payload binding contract rejected");
    auto incomplete = required;
    incomplete.erase(NativeType(RewardPayloadKind::ConsumeItem));
    Check(accept(incomplete, policy) == Status::Invalid, "unsupported consumption capability activated");
    auto invalid = policy;
    invalid.atomicBundles = false;
    Check(accept(required, invalid) == Status::Invalid, "unverified mixed-effect atomicity activated");
    invalid = policy;
    invalid.items.erase(50013);
    Check(accept(required, invalid) == Status::Invalid, "unknown cost item activated");
    invalid = policy;
    invalid.maxGold = 99;
    Check(accept(required, invalid) == Status::Invalid, "gold binding limit ignored");
    invalid = policy;
    invalid.maxExperience = 999;
    Check(accept(required, invalid) == Status::Invalid, "experience binding limit ignored");
    invalid = policy;
    invalid.items.at(50013).maxConsumeCount = 2;
    Check(accept(required, invalid) == Status::Invalid, "cost item count binding ignored");
    invalid = policy;
    invalid.items.at(50011).maxGrantCount = 1;
    Check(accept(required, invalid) == Status::Invalid, "per-prototype grant count binding ignored");
    invalid = policy;
    invalid.items.at(50011) = {0, RewardLimits::ItemCount};
    Check(accept(required, invalid) == Status::Invalid, "consume-only prototype accepted as a grant");
}
void MutationChecks(const std::vector<uint8_t> &valid)
{
    for (size_t length = 0; length < valid.size(); ++length)
        Check(!LoadPackage(std::span(valid).first(length)), "truncated graph-v2 package loaded");
    std::mt19937 random(83021);
    for (size_t iteration = 0; iteration < 1500; ++iteration)
    {
        auto changed = valid;
        const size_t offset = 64 + random() % (changed.size() - 64);
        changed[offset] ^= uint8_t(1u << (random() % 8));
        Rehash(changed);
        const auto loaded = LoadPackage(changed);
        if (loaded)
            Check(bool(CompilePackage(SourceOf(*loaded.package))),
                  "accepted payload package cannot be recompiled");
        else
            Check(!loaded.errors.empty(), "invalid payload package has no diagnostic");
    }
    auto wrong = valid;
    wrong[16] = 3;
    Rehash(wrong);
    Check(!LoadPackage(wrong), "future graph schema loaded");
    wrong = valid;
    wrong[16] = 1;
    Rehash(wrong);
    Check(!LoadPackage(wrong), "v2 payload masqueraded as a v1 graph");
}
void PayloadLimits(const PackageSource &example)
{
    auto source = example;
    source.missions.clear();
    source.presentation.missions.clear();
    for (size_t graph = 0; graph < 9; ++graph)
    {
        Definition definition;
        definition.id = "bulk.reward." + std::to_string(graph);
        definition.version = 1;
        definition.root = "root";
        Node root;
        root.id = "root";
        root.kind = NodeKind::Sequence;
        for (size_t bundle = 0; bundle < 16; ++bundle)
        {
            const auto key = "bundle." + std::to_string(bundle);
            RewardBundle reward;
            for (uint32_t item = 1; item <= 32; ++item)
                reward.items.emplace(item, 1);
            definition.rewards.emplace(key, reward);
            Node node;
            node.id = key;
            node.kind = NodeKind::Reward;
            node.reward = key;
            root.children.push_back(key);
            definition.nodes.push_back(node);
        }
        definition.nodes.insert(definition.nodes.begin(), root);
        source.missions.push_back(definition);
        source.presentation.missions[definition.id] = example.presentation.missions.begin()->second;
    }
    Check(!CompilePackage(source), "aggregate package reward memory bound ignored");
    source.presentation.missions.erase(source.missions.back().id);
    source.missions.pop_back();
    const auto maximum = CompilePackage(source);
    Check(bool(maximum) && bool(LoadPackage(maximum.bytes)), "exact aggregate reward bound rejected");
    // Construct an oversized *decoded* catalog without using the compiler that
    // correctly refuses it. Repeating a whole graph crosses the aggregate item
    // budget before later duplicate-ID/presentation checks can mask the result.
    auto oversized = maximum.bytes;
    size_t section = 64;
    while (Number(oversized, section, 2) != 2)
        section += 12 + Number(oversized, section + 8, 4);
    const auto sectionSize = Number(oversized, section + 8, 4);
    const auto firstSize = Number(oversized, section + 16, 4);
    std::vector<uint8_t> repeated(oversized.begin() + section + 16,
                                  oversized.begin() + section + 20 + firstSize);
    oversized.insert(oversized.begin() + section + 12 + sectionSize, repeated.begin(), repeated.end());
    Put32(oversized, 12, uint32_t(oversized.size() - 64));
    Put32(oversized, section + 8, sectionSize + uint32_t(repeated.size()));
    Put32(oversized, section + 12, 9);
    Rehash(oversized);
    const auto decoded = LoadPackage(oversized);
    Check(!decoded &&
              decoded.errors.front().message.find("Decoded reward item memory limit") != std::string::npos,
          "binary aggregate budget was not enforced while decoding");
    auto &definition = source.missions.front();
    definition.rewards.begin()->second.consumeItems.emplace(99, 1);
    Check(!CompileDefinition(definition), "per-bundle item bound ignored");
}
} // namespace
int main(int argc, char **argv)
{
    try
    {
        Check(argc == 3, "usage: MissionRewardsTests LEGACY_PACKAGE REWARD_SOURCE");
        Legacy(argv[1]);
        const auto bytes = Read(argv[2]);
        const std::string text(bytes.begin(), bytes.end());
        const auto parsed = ParsePackageSource(text);
        Check(bool(parsed), "typed reward example did not parse");
        const auto compiled = CompilePackage(parsed.source);
        Check(bool(compiled) && Number(compiled.bytes, 16, 2) == 2,
              "typed payload did not produce graph schema 2");
        Check(CompilePackage(parsed.source).bytes == compiled.bytes, "reward compilation nondeterministic");
        const auto loaded = LoadPackage(compiled.bytes);
        Check(bool(loaded) && CompilePackage(SourceOf(*loaded.package)).bytes == compiled.bytes,
              "typed package roundtrip changed bytes");
        auto reversed = Json::parse(text);
        std::reverse(reversed["missions"][0]["rewards"]["demo.bundle"]["items"].begin(),
                     reversed["missions"][0]["rewards"]["demo.bundle"]["items"].end());
        const auto reordered = ParsePackageSource(reversed.dump());
        Check(bool(reordered) && CompilePackage(reordered.source).bytes == compiled.bytes,
              "item order changed bundle identity");
        InvalidSources(Json::parse(text));
        BindingsAndPins(parsed.source, loaded);
        MutationChecks(compiled.bytes);
        PayloadLimits(parsed.source);
        Check(NativeType(RewardPayloadKind::Item) == 0x5001 && !FindNativeSchema(0x4001),
              "payload schema advertised a gameplay action implementation");
        const auto registry = Json::parse(ExportNativeSchemas());
        Check(!registry["reward_payloads"]["delivery_implemented"].get<bool>(),
              "schema falsely advertised reward delivery");
        std::cout << "Mission reward payloads: " << checks << " checks passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
