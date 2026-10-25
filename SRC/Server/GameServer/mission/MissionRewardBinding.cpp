#include "MissionRewardBinding.hpp"
#include <stdexcept>

namespace auriga::mission
{
RewardPin PinReward(const ActionKey &action, const ContentDigest &expectedDefinition,
                    const CompiledDefinition &definition)
{
    RewardPin result;
    try
    {
        const auto &source = definition.Source();
        if (!action.character || !action.instance || action.mission != source.id ||
            action.version != source.version || FingerprintDefinition(definition) != expectedDefinition)
            throw std::invalid_argument("Reward action/definition pin differs");
        const auto node = definition.Find(action.node);
        if (node == NoNode || definition.Nodes()[node].value.kind != NodeKind::Reward ||
            definition.Nodes()[node].value.reward != action.reward)
            throw std::invalid_argument("Reward action does not reference this reward node");
        const auto found = source.rewards.find(action.reward);
        if (found == source.rewards.end())
            throw std::invalid_argument("Symbolic reward has no typed, version-pinned payload");
        result.reward =
            PinnedReward(action, expectedDefinition, FingerprintReward(found->second), found->second);
    }
    catch (const std::exception &error)
    {
        result.errors.push_back({"rewards", error.what()});
    }
    return result;
}
} // namespace auriga::mission
