#pragma once
#include "MissionDefinition.hpp"
#include <optional>

namespace auriga::mission
{
struct NodeToken
{
    InstanceId instance = 0;
    NodeIndex node = NoNode;
    auto operator<=>(const NodeToken &) const = default;
};
// Derived, session-owned subscriptions. No character pointers or callbacks.
class MissionEventRouter final
{
  public:
    void WatchEvent(NodeToken token, EventKind kind, std::string subject);
    void WatchFacts(NodeToken token, const Dependencies &dependencies, Timestamp now);
    void WatchDeadline(NodeToken token, Timestamp deadline);
    void Remove(NodeToken token);
    void RemoveInstance(InstanceId id);
    std::vector<NodeToken> Match(EventKind kind, std::string_view subject) const;
    std::vector<NodeToken> MatchFact(const FactKey &fact) const;
    std::vector<NodeToken> Due(Timestamp now) const;
    size_t EventSubscriptionCount() const
    {
        return eventsByToken_.size();
    }
    size_t TimerSubscriptionCount() const
    {
        return timesByToken_.size();
    }
    std::optional<Timestamp> NextDeadline() const;

  private:
    using EventKey = std::pair<EventKind, std::string>;
    std::map<EventKey, std::set<NodeToken>> events_;
    std::map<FactKey, std::set<NodeToken>> facts_;
    std::map<Timestamp, std::set<NodeToken>> deadlines_;
    std::map<NodeToken, EventKey> eventsByToken_;
    std::map<NodeToken, std::set<FactKey>> factsByToken_;
    std::map<NodeToken, std::set<Timestamp>> timesByToken_;
};
} // namespace auriga::mission
