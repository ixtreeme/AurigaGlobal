#pragma once
#include "MissionRewardPayload.hpp"
#include <item/BookLearning.hpp>
#include <expected>
#include <span>
#include <string>

namespace auriga::mission
{
struct ProgressionRules
{
    uint16_t maximumLevel = 0, statLevelLimit = 0;
    bool cappedStats344 = false, skillGroup = false;
    std::vector<uint32_t> nextExperience;
    // Server draws once before making the immutable transaction. Retries use
    // that transaction, never draw a second committed outcome.
    std::vector<std::pair<uint16_t, uint16_t>> gains;
};
std::expected<item::BookCharacter, std::string> RewardProgression(
    const item::BookCharacter &, const RewardBundle &, const ProgressionRules &);
}
