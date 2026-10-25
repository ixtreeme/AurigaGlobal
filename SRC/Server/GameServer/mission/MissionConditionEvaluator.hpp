#pragma once
#include "MissionDefinition.hpp"

namespace auriga::mission
{
struct StoryFacts
{
    std::set<std::string> flags;
    std::map<std::string, std::string> choices;
    bool operator==(const StoryFacts &) const = default;
};
struct Evaluation
{
    enum class Error
    {
        None,
        WorkLimit,
        InvalidDefinition
    };
    bool value = false;
    Error error = Error::None;
    size_t steps = 0;
};
// Read-only and deterministic. The caller supplies authoritative time/facts.
Evaluation Evaluate(const Condition &condition, const Definition &definition, const StoryFacts &facts,
                    Timestamp now, size_t budget = Limits::OperationSteps);
} // namespace auriga::mission
