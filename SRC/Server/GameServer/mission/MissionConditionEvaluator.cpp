#include "MissionConditionEvaluator.hpp"
#include <algorithm>

namespace auriga::mission
{
namespace
{
struct Evaluator
{
    const Definition &definition;
    const StoryFacts &facts;
    Timestamp now;
    size_t budget;
    Evaluation result;
    std::set<std::string> visiting;

    bool Run(const Condition &c, size_t depth)
    {
        if (result.error != Evaluation::Error::None)
            return false;
        if (result.steps == budget)
        {
            result.error = Evaluation::Error::WorkLimit;
            return false;
        }
        ++result.steps;
        if (depth > Limits::Depth)
        {
            result.error = Evaluation::Error::InvalidDefinition;
            return false;
        }
        switch (c.kind)
        {
        case ConditionKind::Always:
            return true;
        case ConditionKind::HasFlag:
            return facts.flags.contains(c.key);
        case ConditionKind::NotFlag:
            return !facts.flags.contains(c.key);
        case ConditionKind::ChoiceIs:
        {
            const auto found = facts.choices.find(c.key);
            return found != facts.choices.end() && found->second == c.value;
        }
        case ConditionKind::TimeWindow:
            return now >= c.beginsAt && now < c.endsAt;
        case ConditionKind::AllOf:
            for (const auto &child : c.children)
                if (!Run(child, depth + 1))
                    return false;
            return true;
        case ConditionKind::AnyOf:
            for (const auto &child : c.children)
                if (Run(child, depth + 1))
                    return true;
            return false;
        case ConditionKind::AtLeastN:
        {
            uint32_t count = 0;
            for (const auto &child : c.children)
            {
                if (Run(child, depth + 1) && ++count >= c.required)
                    return true;
                if (result.error != Evaluation::Error::None)
                    return false;
            }
            return false;
        }
        case ConditionKind::ExactlyN:
        {
            uint32_t count = 0;
            for (const auto &child : c.children)
            {
                count += Run(child, depth + 1);
                if (result.error != Evaluation::Error::None)
                    return false;
            }
            return count == c.required;
        }
        case ConditionKind::Not:
        case ConditionKind::Xor:
            if (c.children.size() != (c.kind == ConditionKind::Not ? 1 : 2))
            {
                result.error = Evaluation::Error::InvalidDefinition;
                return false;
            }
            if (c.kind == ConditionKind::Not)
                return !Run(c.children[0], depth + 1);
            else
            {
                const auto first = Run(c.children[0], depth + 1);
                const auto second = Run(c.children[1], depth + 1);
                return first != second;
            }
        case ConditionKind::Subgraph:
        {
            const auto found = definition.predicates.find(c.key);
            if (found == definition.predicates.end() || !visiting.insert(c.key).second)
            {
                result.error = Evaluation::Error::InvalidDefinition;
                return false;
            }
            const bool value = Run(found->second, depth + 1);
            visiting.erase(c.key);
            return value;
        }
        }
        result.error = Evaluation::Error::InvalidDefinition;
        return false;
    }
};
} // namespace
Evaluation Evaluate(const Condition &c, const Definition &d, const StoryFacts &f, Timestamp now,
                    size_t budget)
{
    Evaluator evaluator{d, f, now, budget, {}, {}};
    evaluator.result.value = evaluator.Run(c, 0);
    if (evaluator.result.error != Evaluation::Error::None)
        evaluator.result.value = false;
    return evaluator.result;
}
} // namespace auriga::mission
