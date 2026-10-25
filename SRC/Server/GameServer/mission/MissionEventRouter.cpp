#include "MissionEventRouter.hpp"

namespace auriga::mission
{
void MissionEventRouter::WatchEvent(NodeToken token, EventKind kind, std::string subject)
{
    Remove(token);
    EventKey key{kind, std::move(subject)};
    events_[key].insert(token);
    eventsByToken_.emplace(token, std::move(key));
}
void MissionEventRouter::WatchFacts(NodeToken token, const Dependencies &d, Timestamp now)
{
    Remove(token);
    if (!d.facts.empty())
    {
        for (const auto &key : d.facts)
            facts_[key].insert(token);
        factsByToken_[token] = d.facts;
    }
    for (const auto deadline : d.boundaries)
        if (deadline > now)
            WatchDeadline(token, deadline);
}
void MissionEventRouter::WatchDeadline(NodeToken token, Timestamp deadline)
{
    deadlines_[deadline].insert(token);
    timesByToken_[token].insert(deadline);
}
void MissionEventRouter::Remove(NodeToken token)
{
    if (auto found = eventsByToken_.find(token); found != eventsByToken_.end())
    {
        auto &tokens = events_.at(found->second);
        tokens.erase(token);
        if (tokens.empty())
            events_.erase(found->second);
        eventsByToken_.erase(found);
    }
    if (auto found = factsByToken_.find(token); found != factsByToken_.end())
    {
        for (const auto &key : found->second)
        {
            auto &tokens = facts_.at(key);
            tokens.erase(token);
            if (tokens.empty())
                facts_.erase(key);
        }
        factsByToken_.erase(found);
    }
    if (auto found = timesByToken_.find(token); found != timesByToken_.end())
    {
        for (auto deadline : found->second)
        {
            auto &tokens = deadlines_.at(deadline);
            tokens.erase(token);
            if (tokens.empty())
                deadlines_.erase(deadline);
        }
        timesByToken_.erase(found);
    }
}
void MissionEventRouter::RemoveInstance(InstanceId id)
{
    std::set<NodeToken> tokens;
    const auto collect = [&](const auto &index)
    {
        for (auto it = index.lower_bound({id, 0}); it != index.end() && it->first.instance == id; ++it)
            tokens.insert(it->first);
    };
    collect(eventsByToken_);
    collect(factsByToken_);
    collect(timesByToken_);
    for (auto token : tokens)
        Remove(token);
}
std::vector<NodeToken> MissionEventRouter::Match(EventKind kind, std::string_view subject) const
{
    std::set<NodeToken> result;
    for (const auto &key : {EventKey{kind, std::string(subject)}, EventKey{kind, "*"}})
        if (auto it = events_.find(key); it != events_.end())
            result.insert(it->second.begin(), it->second.end());
    return {result.begin(), result.end()};
}
std::vector<NodeToken> MissionEventRouter::MatchFact(const FactKey &key) const
{
    const auto found = facts_.find(key);
    if (found == facts_.end())
        return {};
    return {found->second.begin(), found->second.end()};
}
std::vector<NodeToken> MissionEventRouter::Due(Timestamp now) const
{
    std::set<NodeToken> result;
    for (auto it = deadlines_.begin(); it != deadlines_.end() && it->first <= now; ++it)
        result.insert(it->second.begin(), it->second.end());
    return {result.begin(), result.end()};
}
std::optional<Timestamp> MissionEventRouter::NextDeadline() const
{
    if (deadlines_.empty())
        return std::nullopt;
    return deadlines_.begin()->first;
}
} // namespace auriga::mission
