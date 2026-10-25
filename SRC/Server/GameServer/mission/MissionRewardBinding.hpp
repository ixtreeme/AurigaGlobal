#pragma once
#include "MissionPackage.hpp"
#include "MissionRuntime.hpp"

namespace auriga::mission
{
struct RewardPin;
// Deep-owned immutable content binding. Does not attest prototype existence,
// character balance, session authorization or an implemented delivery adapter.
class PinnedReward final
{
  public:
    PinnedReward(const PinnedReward &) = default;
    PinnedReward(PinnedReward &&) = default;
    PinnedReward &operator=(const PinnedReward &) = default;
    PinnedReward &operator=(PinnedReward &&) = default;
    const ActionKey &Action() const
    {
        return action_;
    }
    const ContentDigest &DefinitionDigest() const
    {
        return definition_;
    }
    const ContentDigest &PayloadDigest() const
    {
        return digest_;
    }
    const RewardBundle &Payload() const
    {
        return payload_;
    }

  private:
    friend RewardPin PinReward(const ActionKey &, const ContentDigest &, const CompiledDefinition &);
    PinnedReward(ActionKey action, ContentDigest definition, ContentDigest digest, RewardBundle payload)
        : action_(std::move(action)), definition_(definition), digest_(digest), payload_(std::move(payload))
    {
    }
    ActionKey action_;
    ContentDigest definition_{}, digest_{};
    RewardBundle payload_;
};
struct RewardPin
{
    std::optional<PinnedReward> reward;
    std::vector<Diagnostic> errors;
    explicit operator bool() const
    {
        return reward.has_value() && errors.empty();
    }
};
RewardPin PinReward(const ActionKey &action, const ContentDigest &expectedDefinition,
                    const CompiledDefinition &definition);
} // namespace auriga::mission
