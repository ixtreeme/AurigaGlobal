#pragma once
#include "../../SRC/Server/GameServer/mission/MissionResourcePlan.hpp"
#include "../../SRC/Server/GameServer/mission/MissionValidator.hpp"
#include "MissionPersistenceFixture.hpp"

namespace resource_fixture
{
using namespace auriga::mission;
inline PackageSource Source()
{
    auto source = mission_fixture::Source(1, 1);
    source.manifest.id = "resource.campaign";
    source.missions.resize(1);
    auto &definition = source.missions.front();
    definition.id = "resource.story";
    definition.nodes.front().children = {"collect", "reward"};
    definition.nodes.erase(definition.nodes.begin() + 2);
    definition.nodes[1].objective = {EventKind::MobKilled, "monkey", 1, false};
    definition.rewards.begin()->second = RewardBundle{1, {{50011, 2}}, 1000, 100, {{50010, 3}}};
    source.presentation.missions.clear();
    source.presentation.missions[definition.id] = {"story.title", "story.summary", {}};
    return source;
}
inline ActionKey Key(uint32_t character = 100)
{
    return {character, "resource.story", 1, 10, "reward", "persist.reward"};
}
inline PinnedReward Pin(RewardBundle bundle = Source().missions.front().rewards.begin()->second,
                        uint32_t character = 100)
{
    auto definition = Source().missions.front();
    definition.rewards.begin()->second = std::move(bundle);
    const auto compiled = CompileDefinition(definition);
    if (!compiled)
        throw std::runtime_error("resource fixture failed compile");
    auto pin = PinReward(Key(character), FingerprintDefinition(*compiled.definition), *compiled.definition);
    if (!pin)
        throw std::runtime_error("resource fixture failed pin");
    return std::move(*pin.reward);
}
inline ResourceRules Rules()
{
    ResourceRules rules;
    rules.maximumLevel = 12;
    rules.nextExperience.assign(13, 1000);
    rules.nextExperience.front() = 0;
    rules.hpMinimum = 5;
    rules.hpMaximum = 8;
    rules.spMinimum = 2;
    rules.spMaximum = 4;
    rules.items = {{50010, {10, 10}}, {50011, {2, 2}}};
    return rules;
}
inline ResourceSnapshot Snapshot(uint32_t character = 100)
{
    ResourceSnapshot snapshot;
    snapshot.character = character;
    snapshot.revision = 77;
    snapshot.progress = {9, 3, 900, 10, 4, 2, 40, 20, 0, 1, false};
    snapshot.gold = 1000;
    snapshot.items = {{10, character, 50010, 2, ResourceWindow::Inventory, 0, 3, 0},
                      {11, character, 50010, 3, ResourceWindow::ExtraInventory, 1, 5, 0}};
    return snapshot;
}
inline std::vector<ResourceLevelRoll> Rolls()
{
    return {{9, 5, 2}};
}
} // namespace resource_fixture
