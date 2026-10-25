#pragma once
#include "../../SRC/Server/GameServer/mission/MissionPackage.hpp"
#include "../../SRC/Server/GameServer/mission/MissionRuntime.hpp"
#include <stdexcept>

namespace mission_fixture
{
using namespace auriga::mission;
inline Node NodeOf(std::string id, NodeKind kind)
{
    Node node;
    node.id = std::move(id);
    node.kind = kind;
    return node;
}
inline std::vector<Definition> Definitions(uint32_t version = 1)
{
    Definition story;
    story.id = "persist.story";
    story.version = version;
    story.root = "root";
    story.repeatable = true;
    story.choices["persist.road"] = {"forest", "river"};
    auto root = NodeOf("root", NodeKind::Sequence);
    root.children = {"collect", "choose", "reward"};
    auto collect = NodeOf("collect", NodeKind::Objective);
    collect.objective = {EventKind::ItemCollected, "*", version == 1 ? 3u : 4u, true};
    collect.completionFlags = {"persist.collected"};
    auto choose = NodeOf("choose", NodeKind::Objective);
    choose.objective = {EventKind::DialogueChoiceCommitted, "persist.road", 1, false};
    auto reward = NodeOf("reward", NodeKind::Reward);
    reward.reward = "persist.reward";
    reward.completionFlags = {"persist.finished"};
    story.nodes = {root, collect, choose, reward};
    RewardBundle bundle;
    bundle.items = {{50011, version == 1 ? 2u : 3u}};
    bundle.experience = 1000;
    bundle.gold = version == 1 ? 100 : 200;
    story.rewards.emplace("persist.reward", bundle);
    Definition timer;
    timer.id = "persist.timer";
    timer.version = version;
    timer.root = "root";
    root.children = {"sleep", "done"};
    auto sleep = NodeOf("sleep", NodeKind::Timer);
    sleep.duration = version == 1 ? 5000 : 6000;
    auto done = NodeOf("done", NodeKind::Success);
    done.completionFlags = {"persist.clock_done"};
    timer.nodes = {root, sleep, done};
    return {story, timer};
}
inline PackageSource Source(uint32_t packageVersion = 1, uint32_t definitionVersion = 1)
{
    PackageSource source;
    source.manifest = {"persist.campaign", packageVersion, "hu", {}};
    source.missions = Definitions(definitionVersion);
    source.presentation.locales["hu"] = {{"story.title", "Story"}, {"story.summary", "Summary"}};
    for (const auto &definition : source.missions)
        source.presentation.missions[definition.id] = {"story.title", "story.summary", {}};
    return source;
}
inline PackageBuild Package(uint32_t packageVersion = 1, uint32_t definitionVersion = 1)
{
    return CompilePackage(Source(packageVersion, definitionVersion));
}
inline void Accepted(const OperationResult &result)
{
    if (result.code != ResultCode::Applied && result.code != ResultCode::NoChange)
        throw std::runtime_error("fixture runtime operation rejected: " + std::to_string(int(result.code)));
}
inline void Progress(MissionRuntime &runtime, CharacterSession owner)
{
    Accepted(runtime.Start("persist.story", 1, 10, 1000));
    Accepted(runtime.Start("persist.timer", 1, 20, 1000));
    Accepted(runtime.SetFlag("persist.external", 1000));
    Accepted(runtime.Dispatch({owner, {9, 300}, 1001, EventKind::ItemCollected, "iron.a", 1, {}}, 1001));
    Accepted(runtime.Dispatch({owner, {9, 298}, 1001, EventKind::ItemCollected, "iron.b", 1, {}}, 1001));
    Accepted(runtime.Dispatch({owner, {9, 299}, 1001, EventKind::ItemCollected, "iron.c", 1, {}}, 1001));
    Accepted(runtime.Dispatch(
        {owner, {9, 301}, 1002, EventKind::DialogueChoiceCommitted, "persist.road", 1, "forest"}, 1002));
}
} // namespace mission_fixture
