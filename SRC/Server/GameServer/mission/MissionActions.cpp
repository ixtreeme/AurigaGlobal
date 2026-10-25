#include "MissionActions.hpp"
#include "MissionPackage.hpp"
#include <stdexcept>

namespace auriga::mission
{
ContentDigest ActionIdentity(const ActionKey &key)
{
    if (!key.character || !key.version || !key.instance || !IsIdentifier(key.mission) ||
        !IsIdentifier(key.node) || !IsIdentifier(key.reward))
        throw std::invalid_argument("Invalid mission action identity");
    std::vector<uint8_t> bytes{'M', 'E', 'A', 'C', 'T', 1, 0, 0};
    const auto number = [&](uint64_t value, size_t width)
    {
        for (size_t i = 0; i < width; ++i)
            bytes.push_back(uint8_t(value >> (8 * i)));
    };
    const auto text = [&](std::string_view value)
    {
        number(value.size(), 4);
        bytes.insert(bytes.end(), value.begin(), value.end());
    };
    number(key.character, 4);
    text(key.mission);
    number(key.version, 4);
    number(key.instance, 8);
    text(key.node);
    text(key.reward);
    return ContentHash(bytes);
}
ActionBatch CheckpointActionBatch(const MissionRuntime::Checkpoint &checkpoint,
                                  const DefinitionRegistry &registry)
{
    ActionBatch result;
    try
    {
        if (!MissionRuntime::ValidateCheckpoint(registry, checkpoint))
            throw std::invalid_argument("Invalid action checkpoint");
        std::map<std::pair<std::string, uint32_t>, ContentDigest> fingerprints;
        for (const auto &[id, instance] : checkpoint.instances)
        {
            const auto graph = registry.Get(instance.mission, instance.version);
            for (NodeIndex node = 0; node < instance.nodes.size(); ++node)
            {
                const auto &progress = instance.nodes[node];
                const auto &definition = graph->Nodes()[node].value;
                if (definition.kind != NodeKind::Reward ||
                    (progress.status != NodeStatus::Active && !progress.receipt))
                    continue;
                if (result.actions.size() == CheckpointActions)
                    throw std::invalid_argument("Checkpoint action capacity exceeded");
                const auto identity = std::pair{instance.mission, instance.version};
                auto fingerprint = fingerprints.find(identity);
                if (fingerprint == fingerprints.end())
                    fingerprint = fingerprints.emplace(identity, FingerprintDefinition(*graph)).first;
                SavedAction action;
                action.key = {checkpoint.character, instance.mission, instance.version, id,
                              definition.id,        definition.reward};
                action.id = ActionIdentity(action.key);
                action.definition = fingerprint->second;
                action.acknowledged = progress.receipt;
                result.actions.push_back(std::move(action));
            }
        }
    }
    catch (const std::exception &error)
    {
        result.actions.clear();
        result.errors.push_back({"actions", error.what()});
    }
    return result;
}
} // namespace auriga::mission
