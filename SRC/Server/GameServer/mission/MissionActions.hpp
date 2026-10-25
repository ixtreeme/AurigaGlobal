#pragma once
#include "MissionCheckpointCodec.hpp"

namespace auriga::mission
{
// The SQL batch bound includes completed receipts retained in the checkpoint.
// Exceeding it rejects the entire save, never a truncated subset of actions.
inline constexpr size_t CheckpointActions = 128;
struct SavedAction
{
    ActionKey key;
    ContentDigest id{}, definition{};
    bool acknowledged = false;
};
struct ActionBatch
{
    std::vector<SavedAction> actions;
    std::vector<Diagnostic> errors;
    explicit operator bool() const
    {
        return errors.empty();
    }
};
// Stable across processes/sessions/catalog activation; length-prefixed, tagged
// fields, explicit integer widths. This is an identity, not proof of delivery.
ContentDigest ActionIdentity(const ActionKey &key);
ActionBatch CheckpointActionBatch(const MissionRuntime::Checkpoint &checkpoint,
                                  const DefinitionRegistry &registry);
} // namespace auriga::mission
