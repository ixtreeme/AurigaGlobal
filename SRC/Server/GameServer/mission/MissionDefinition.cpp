#include "MissionDefinition.hpp"
#include "MissionValidator.hpp"
#include <algorithm>
#include <initializer_list>
#include <mutex>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace auriga::mission
{
bool IsIdentifier(std::string_view s)
{
    if (s.empty() || s.size() > Limits::IdentifierBytes)
        return false;
    return std::all_of(s.begin(), s.end(),
                       [](unsigned char c)
                       {
                           return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                                  (c >= '0' && c <= '9') || c == '_' || c == '.' || c == ':' || c == '-' ||
                                  c == '/';
                       });
}
NodeIndex CompiledDefinition::Find(std::string_view id) const
{
    const auto found = indices_.find(id);
    return found == indices_.end() ? NoNode : found->second;
}
Publication DefinitionRegistry::Publish(const Definition &source)
{
    std::unique_lock lock(mutex_);
    if (catalogManaged_)
        return {Publication::Status::Invalid,
                {{"catalog", "use whole-package activation after catalog management begins"}}};
    if (auto found = definitions_.find(source.id);
        found != definitions_.end() && found->second.contains(source.version))
        return {Publication::Status::VersionExists, {}};
    if (versions_ >= 4096)
        return {Publication::Status::CapacityExceeded, {}};
    auto result = CompileDefinition(source);
    if (!result)
        return {Publication::Status::Invalid, std::move(result.errors)};
    const auto dependencies = result.definition->DependencyEntries();
    if (dependencies > Limits::RegistryDependencies - dependencyEntries_)
        return {Publication::Status::CapacityExceeded, {{"registry", "derived dependency limit"}}};
    for (const auto &[key, values] : source.choices)
    {
        const std::set<std::string> domain(values.begin(), values.end());
        const auto existing = choices_.find(key);
        if (existing != choices_.end() && existing->second != domain)
            return {Publication::Status::Invalid,
                    {{"choices/" + key, "incompatible domain for a persistent choice key"}}};
    }
    for (const auto &[key, values] : source.choices)
        choices_.try_emplace(key, values.begin(), values.end());
    definitions_[source.id].emplace(source.version, std::move(result.definition));
    auto &active = active_[source.id];
    active = std::max(active, source.version);
    ++versions_;
    dependencyEntries_ += dependencies;
    return {Publication::Status::Published, {}};
}
std::shared_ptr<const CompiledDefinition> DefinitionRegistry::Get(std::string_view id, uint32_t version) const
{
    std::shared_lock lock(mutex_);
    const auto mission = definitions_.find(id);
    if (mission == definitions_.end())
        return {};
    const auto found = mission->second.find(version);
    return found == mission->second.end() ? nullptr : found->second;
}
std::shared_ptr<const CompiledDefinition> DefinitionRegistry::Latest(std::string_view id) const
{
    std::shared_lock lock(mutex_);
    const auto active = active_.find(id);
    return active == active_.end() ? nullptr : definitions_.at(std::string(id)).at(active->second);
}
bool DefinitionRegistry::IsChoiceValue(std::string_view key, std::string_view value) const
{
    std::shared_lock lock(mutex_);
    const auto found = choices_.find(std::string(key));
    return found != choices_.end() && found->second.contains(std::string(value));
}
namespace
{
using Json = nlohmann::json;
[[noreturn]] void Bad(const std::string &path, std::string_view message)
{
    throw std::invalid_argument(path + ": " + std::string(message));
}
void Fields(const Json &j, const std::string &path, std::initializer_list<std::string_view> required,
            std::initializer_list<std::string_view> optional = {})
{
    if (!j.is_object())
        Bad(path, "expected object");
    for (auto key : required)
        if (!j.contains(key))
            Bad(path, "missing field " + std::string(key));
    for (auto it = j.begin(); it != j.end(); ++it)
        if (std::find(required.begin(), required.end(), it.key()) == required.end() &&
            std::find(optional.begin(), optional.end(), it.key()) == optional.end())
            Bad(path, "unknown field " + it.key());
}
std::string Text(const Json &j, const std::string &path, bool wildcard = false)
{
    if (!j.is_string())
        Bad(path, "expected identifier string");
    auto value = j.get<std::string>();
    if (!(wildcard && value == "*") && !IsIdentifier(value))
        Bad(path, "invalid identifier");
    return value;
}
uint64_t Number(const Json &j, const std::string &path, uint64_t max = UINT64_MAX)
{
    if ((!j.is_number_unsigned() && !j.is_number_integer()) ||
        (j.is_number_integer() && !j.is_number_unsigned() && j.get<int64_t>() < 0))
        Bad(path, "expected nonnegative integer");
    const auto n = j.get<uint64_t>();
    if (n > max)
        Bad(path, "integer outside allowed range");
    return n;
}
bool Boolean(const Json &j, const std::string &path)
{
    if (!j.is_boolean())
        Bad(path, "expected boolean");
    return j.get<bool>();
}
RewardBundle ReadRewardBundle(const Json &j, const std::string &path)
{
    Fields(j, path, {"schema"}, {"items", "experience", "gold", "consume_items"});
    RewardBundle bundle;
    bundle.schema = uint16_t(Number(j.at("schema"), path + "/schema", UINT16_MAX));
    for (const auto &[field, table] :
         std::initializer_list<std::pair<std::string_view, std::map<uint32_t, uint32_t> *>>{
             {"items", &bundle.items}, {"consume_items", &bundle.consumeItems}})
    {
        if (!j.contains(field))
            continue;
        const auto &entries = j.at(field);
        if (!entries.is_array() || entries.size() > RewardLimits::ItemEntries)
            Bad(path, "reward item entry limit");
        for (const auto &item : entries)
        {
            Fields(item, path, {"vnum", "count"});
            const auto vnum = uint32_t(Number(item.at("vnum"), path + "/vnum", UINT32_MAX));
            const auto count = uint32_t(Number(item.at("count"), path + "/count", RewardLimits::ItemCount));
            if (!table->emplace(vnum, count).second)
                Bad(path, "duplicate reward item prototype");
        }
    }
    if (j.contains("experience"))
        bundle.experience = Number(j.at("experience"), path + "/experience", RewardLimits::Amount);
    if (j.contains("gold"))
        bundle.gold = Number(j.at("gold"), path + "/gold", RewardLimits::Amount);
    if (!ValidRewardBundle(bundle))
        Bad(path, "invalid, empty or unsupported reward bundle");
    return bundle;
}
template <class Enum>
Enum Named(const Json &j, const std::string &path,
           std::initializer_list<std::pair<std::string_view, Enum>> values)
{
    if (!j.is_string())
        Bad(path, "expected kind string");
    const auto key = j.get<std::string>();
    for (const auto &[name, value] : values)
        if (name == key)
            return value;
    Bad(path, "unsupported kind " + key);
}
EventKind Event(const Json &j, const std::string &p)
{
    return Named<EventKind>(j, p,
                            {{"mob_killed", EventKind::MobKilled},
                             {"item_collected", EventKind::ItemCollected},
                             {"item_crafted", EventKind::ItemCrafted},
                             {"npc_interacted", EventKind::NpcInteracted},
                             {"area_discovered", EventKind::AreaDiscovered},
                             {"dialogue_choice", EventKind::DialogueChoiceCommitted},
                             {"boss_defeated", EventKind::BossDefeated},
                             {"puzzle_solved", EventKind::PuzzleSolved},
                             {"escort_completed", EventKind::EscortCompleted}});
}
Condition Predicate(const Json &j, const std::string &p, size_t depth = 0)
{
    if (depth > Limits::Depth)
        Bad(p, "condition nesting limit");
    if (!j.is_object() || !j.contains("op"))
        Bad(p, "expected condition with op");
    Condition c;
    c.kind = Named<ConditionKind>(j.at("op"), p,
                                  {{"always", ConditionKind::Always},
                                   {"all_of", ConditionKind::AllOf},
                                   {"any_of", ConditionKind::AnyOf},
                                   {"at_least_n", ConditionKind::AtLeastN},
                                   {"has_flag", ConditionKind::HasFlag},
                                   {"not_flag", ConditionKind::NotFlag},
                                   {"choice_is", ConditionKind::ChoiceIs},
                                   {"time_window", ConditionKind::TimeWindow},
                                   {"subgraph", ConditionKind::Subgraph},
                                   {"not", ConditionKind::Not},
                                   {"xor", ConditionKind::Xor},
                                   {"exactly_n", ConditionKind::ExactlyN}});
    switch (c.kind)
    {
    case ConditionKind::Always:
        Fields(j, p, {"op"});
        break;
    case ConditionKind::AllOf:
    case ConditionKind::AnyOf:
    case ConditionKind::AtLeastN:
    case ConditionKind::ExactlyN:
    case ConditionKind::Not:
    case ConditionKind::Xor:
        if (c.kind == ConditionKind::AtLeastN || c.kind == ConditionKind::ExactlyN)
        {
            Fields(j, p, {"op", "children", "required"});
            c.required = uint32_t(Number(j.at("required"), p, UINT32_MAX));
        }
        else
            Fields(j, p, {"op", "children"});
        if (!j.at("children").is_array() || j.at("children").size() > Limits::Conditions)
            Bad(p, "invalid condition children");
        for (const auto &child : j.at("children"))
            c.children.push_back(Predicate(child, p + "/children", depth + 1));
        break;
    case ConditionKind::HasFlag:
    case ConditionKind::NotFlag:
    case ConditionKind::Subgraph:
        Fields(j, p, {"op", "key"});
        c.key = Text(j.at("key"), p + "/key");
        break;
    case ConditionKind::ChoiceIs:
        Fields(j, p, {"op", "key", "value"});
        c.key = Text(j.at("key"), p + "/key");
        c.value = Text(j.at("value"), p + "/value");
        break;
    case ConditionKind::TimeWindow:
        Fields(j, p, {"op", "begins_at", "ends_at"});
        c.beginsAt = Timestamp(Number(j.at("begins_at"), p, INT64_MAX));
        c.endsAt = Timestamp(Number(j.at("ends_at"), p, INT64_MAX));
        break;
    }
    return c;
}
Node ReadNode(const Json &j, const std::string &p)
{
    Fields(j, p, {"id", "kind"},
           {"children", "objective", "condition", "branches", "fallback", "parallel", "duration", "reward",
            "completion_flags"});
    Node n;
    n.id = Text(j.at("id"), p + "/id");
    n.kind = Named<NodeKind>(j.at("kind"), p,
                             {{"objective", NodeKind::Objective},
                              {"condition", NodeKind::Condition},
                              {"branch", NodeKind::Branch},
                              {"sequence", NodeKind::Sequence},
                              {"parallel", NodeKind::Parallel},
                              {"timer", NodeKind::Timer},
                              {"reward", NodeKind::Reward},
                              {"sub_mission", NodeKind::SubMission},
                              {"success", NodeKind::Success},
                              {"failure", NodeKind::Failure},
                              {"recovery", NodeKind::Recovery}});
    const auto strings = [&](std::string_view key, std::vector<std::string> &out)
    {
        if (!j.contains(key))
            return;
        const auto &values = j.at(key);
        if (!values.is_array() || values.size() > Limits::Nodes)
            Bad(p, "invalid identifier array");
        for (const auto &value : values)
            out.push_back(Text(value, p + "/" + std::string(key)));
    };
    strings("children", n.children);
    strings("completion_flags", n.completionFlags);
    if (j.contains("objective"))
    {
        const auto &o = j.at("objective");
        Fields(o, p + "/objective", {"event", "subject", "goal"}, {"distinct"});
        n.objective.event = Event(o.at("event"), p);
        n.objective.subject = Text(o.at("subject"), p, true);
        n.objective.goal = Number(o.at("goal"), p, Limits::Counter);
        if (o.contains("distinct"))
            n.objective.distinctSubjects = Boolean(o.at("distinct"), p);
    }
    if (j.contains("condition"))
        n.condition = Predicate(j.at("condition"), p + "/condition");
    if (j.contains("branches"))
    {
        if (!j.at("branches").is_array() || j.at("branches").size() > Limits::Nodes)
            Bad(p, "invalid branch array");
        for (const auto &b : j.at("branches"))
        {
            Fields(b, p + "/branches", {"when", "child"});
            n.branches.push_back({Predicate(b.at("when"), p + "/branches/when"), Text(b.at("child"), p)});
        }
    }
    if (j.contains("fallback"))
        n.fallback = Text(j.at("fallback"), p);
    if (j.contains("parallel"))
        n.parallel = Named<ParallelPolicy>(j.at("parallel"), p,
                                           {{"all", ParallelPolicy::All},
                                            {"any", ParallelPolicy::Any},
                                            {"race", ParallelPolicy::Race},
                                            {"first_terminal", ParallelPolicy::FirstTerminal}});
    if (j.contains("duration"))
        n.duration = Timestamp(Number(j.at("duration"), p, INT64_MAX));
    if (j.contains("reward"))
        n.reward = Text(j.at("reward"), p);
    // Reject misplaced fields instead of silently ignoring an author's typo.
    const auto only = [&](std::string_view key, bool allowed)
    {
        if (j.contains(key) && !allowed)
            Bad(p, "field not valid for node kind: " + std::string(key));
    };
    only("children", n.kind == NodeKind::Sequence || n.kind == NodeKind::Parallel ||
                         n.kind == NodeKind::Recovery || n.kind == NodeKind::SubMission);
    only("objective", n.kind == NodeKind::Objective);
    only("condition", n.kind == NodeKind::Condition);
    only("branches", n.kind == NodeKind::Branch);
    only("fallback", n.kind == NodeKind::Branch);
    only("parallel", n.kind == NodeKind::Parallel);
    only("duration", n.kind == NodeKind::Timer);
    only("reward", n.kind == NodeKind::Reward);
    if (n.kind == NodeKind::Objective && !j.contains("objective"))
        Bad(p, "missing objective");
    if (n.kind == NodeKind::Condition && !j.contains("condition"))
        Bad(p, "missing condition");
    return n;
}
} // namespace
DefinitionParseResult ParseDefinition(std::string_view text)
{
    DefinitionParseResult result;
    if (text.size() > Limits::DefinitionBytes)
    {
        result.errors.push_back({"json", "definition byte limit"});
        return result;
    }
    try
    {
        std::vector<std::set<std::string>> keys;
        auto callback = [&](int depth, Json::parse_event_t event, Json &parsed)
        {
            if (depth > int(2 * Limits::Depth + 8))
                Bad("json", "nesting limit");
            if (event == Json::parse_event_t::object_start)
                keys.emplace_back();
            if (event == Json::parse_event_t::key && !keys.back().insert(parsed.get<std::string>()).second)
                Bad("json", "duplicate object key");
            if (event == Json::parse_event_t::object_end)
                keys.pop_back();
            return true;
        };
        const auto j = Json::parse(text.begin(), text.end(), callback);
        Fields(j, "definition", {"schema", "mission", "version", "root", "nodes"},
               {"repeatable", "prerequisite", "predicates", "choices", "rewards"});
        const auto schema = Number(j.at("schema"), "schema");
        if (schema != 1 && schema != 2)
            Bad("schema", "unsupported schema version");
        if (j.contains("rewards") && schema != 2)
            Bad("schema", "reward payloads require definition schema 2");
        auto &d = result.value;
        d.id = Text(j.at("mission"), "mission");
        d.version = uint32_t(Number(j.at("version"), "version", UINT32_MAX));
        d.root = Text(j.at("root"), "root");
        if (j.contains("rewards"))
        {
            const auto &rewards = j.at("rewards");
            if (!rewards.is_object() || rewards.size() > RewardLimits::Bundles)
                Bad("rewards", "invalid reward table");
            size_t rewardItems = 0;
            for (auto it = rewards.begin(); it != rewards.end(); ++it)
            {
                if (!IsIdentifier(it.key()))
                    Bad("rewards", "invalid reward key");
                auto bundle = ReadRewardBundle(it.value(), "rewards/" + it.key());
                const auto items = bundle.items.size() + bundle.consumeItems.size();
                if (items > RewardLimits::DefinitionItems - rewardItems)
                    Bad("rewards", "definition reward item memory limit");
                rewardItems += items;
                d.rewards.emplace(it.key(), std::move(bundle));
            }
        }
        if (j.contains("repeatable"))
            d.repeatable = Boolean(j.at("repeatable"), "repeatable");
        if (j.contains("prerequisite"))
            d.prerequisite = Predicate(j.at("prerequisite"), "prerequisite");
        if (j.contains("predicates"))
        {
            if (!j.at("predicates").is_object() || j.at("predicates").size() > Limits::Conditions)
                Bad("predicates", "invalid predicate table");
            for (auto it = j.at("predicates").begin(); it != j.at("predicates").end(); ++it)
                d.predicates.emplace(it.key(), Predicate(it.value(), "predicates/" + it.key()));
        }
        if (j.contains("choices"))
        {
            if (!j.at("choices").is_object() || j.at("choices").size() > Limits::Choices)
                Bad("choices", "invalid choice table");
            for (auto it = j.at("choices").begin(); it != j.at("choices").end(); ++it)
            {
                if (!it.value().is_array() || it.value().size() > Limits::ChoiceValues)
                    Bad("choices/" + it.key(), "invalid choice domain");
                for (const auto &value : it.value())
                    d.choices[it.key()].push_back(Text(value, "choices/" + it.key()));
            }
        }
        if (!j.at("nodes").is_array() || j.at("nodes").size() > Limits::Nodes)
            Bad("nodes", "node limit");
        for (const auto &node : j.at("nodes"))
            d.nodes.push_back(ReadNode(node, "nodes/" + std::to_string(d.nodes.size())));
    }
    catch (const std::exception &e)
    {
        result.value = {};
        result.errors.push_back({"json", e.what()});
    }
    return result;
}
} // namespace auriga::mission
