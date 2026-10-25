#include "MissionNativeSchema.hpp"
#include <algorithm>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace auriga::mission
{
namespace
{
std::vector<NativeSchema> MakeSchemas()
{
    std::vector<NativeSchema> schemas;
    const auto flow =
        [&](uint32_t id, NodeKind kind, std::string_view name, std::vector<NativeParameter> parameters = {})
    {
        parameters.push_back({90, "completion_flags", PortType::StoryFlagKey, false, true});
        schemas.push_back({id,
                           1,
                           NativeCategory::Flow,
                           uint8_t(kind),
                           name,
                           name,
                           {{1, "enter", PortType::Exec},
                            {2, "success", PortType::Exec, false},
                            {3, "failure", PortType::Exec, false}},
                           std::move(parameters)});
    };
    flow(0x1001, NodeKind::Objective, "core.wait_event",
         {{10, "event", PortType::EventType},
          {11, "subject", PortType::StringKey},
          {12, "goal", PortType::Int64, true, false, 1, int64_t(Limits::Counter)},
          {13, "distinct", PortType::Bool, false, false, 0, 0, NativeDefault{false}}});
    flow(0x1002, NodeKind::Condition, "core.wait_condition", {{20, "condition", PortType::Predicate}});
    flow(0x1003, NodeKind::Branch, "core.branch",
         {{21, "branches", PortType::BranchArm, true, true}, {22, "fallback", PortType::NodeRef}});
    flow(0x1004, NodeKind::Sequence, "core.sequence", {{10, "children", PortType::NodeRef, true, true}});
    flow(0x1005, NodeKind::Parallel, "core.parallel",
         {{10, "children", PortType::NodeRef, true, true},
          {23, "policy", PortType::Enum, false, false, 0, 3, NativeDefault{int64_t(0)}}});
    flow(0x1006, NodeKind::Timer, "core.delay",
         {{30, "duration", PortType::TimeDuration, true, false, 1, INT64_MAX}});
    flow(0x1007, NodeKind::Reward, "core.await_reward_receipt", {{40, "reward", PortType::RewardKey}});
    flow(0x1008, NodeKind::SubMission, "core.inline_subgraph",
         {{10, "children", PortType::NodeRef, true, true}});
    flow(0x1009, NodeKind::Success, "core.success");
    flow(0x100a, NodeKind::Failure, "core.failure");
    flow(0x100b, NodeKind::Recovery, "core.recovery", {{10, "children", PortType::NodeRef, true, true}});
    const auto predicate = [&](uint32_t id, ConditionKind kind, std::string_view name,
                               std::vector<NativeParameter> parameters = {})
    {
        schemas.push_back({id,
                           1,
                           NativeCategory::Predicate,
                           uint8_t(kind),
                           name,
                           name,
                           {{1, "value", PortType::Bool, false}},
                           std::move(parameters)});
    };
    predicate(0x2001, ConditionKind::Always, "condition.always");
    predicate(0x2002, ConditionKind::AllOf, "condition.all_of",
              {{10, "children", PortType::Predicate, true, true}});
    predicate(0x2003, ConditionKind::AnyOf, "condition.any_of",
              {{10, "children", PortType::Predicate, true, true}});
    predicate(0x2004, ConditionKind::AtLeastN, "condition.at_least_n",
              {{10, "children", PortType::Predicate, true, true},
               {11, "required", PortType::Int64, true, false, 1, 64}});
    predicate(0x2005, ConditionKind::HasFlag, "condition.has_story_flag",
              {{20, "key", PortType::StoryFlagKey}});
    predicate(0x2006, ConditionKind::NotFlag, "condition.not_story_flag",
              {{20, "key", PortType::StoryFlagKey}});
    predicate(0x2007, ConditionKind::ChoiceIs, "condition.choice_is",
              {{20, "key", PortType::StringKey}, {21, "value", PortType::StringKey}});
    predicate(0x2008, ConditionKind::TimeWindow, "condition.time_window",
              {{30, "begins_at", PortType::Int64, true, false, 0, INT64_MAX},
               {31, "ends_at", PortType::Int64, true, false, 1, INT64_MAX}});
    predicate(0x2009, ConditionKind::Subgraph, "condition.subgraph", {{20, "key", PortType::StringKey}});
    predicate(0x200a, ConditionKind::Not, "condition.not", {{10, "children", PortType::Predicate}});
    predicate(0x200b, ConditionKind::Xor, "condition.xor",
              {{10, "children", PortType::Predicate, true, true}});
    predicate(0x200c, ConditionKind::ExactlyN, "condition.exactly_n",
              {{10, "children", PortType::Predicate, true, true},
               {11, "required", PortType::Int64, true, false, 0, 64}});
    const auto event = [&](uint32_t id, EventKind kind, std::string_view name)
    {
        schemas.push_back({id,
                           1,
                           NativeCategory::Event,
                           uint8_t(kind),
                           name,
                           name,
                           {{1, "payload", PortType::EventPayload, false}},
                           {}});
    };
    event(0x3001, EventKind::MobKilled, "event.mob_killed");
    event(0x3002, EventKind::ItemCollected, "event.item_obtained");
    event(0x3003, EventKind::ItemCrafted, "event.item_crafted");
    event(0x3004, EventKind::NpcInteracted, "event.npc_interacted");
    event(0x3005, EventKind::AreaDiscovered, "event.area_discovered");
    event(0x3006, EventKind::DialogueChoiceCommitted, "event.dialogue_choice");
    event(0x3007, EventKind::BossDefeated, "event.boss_defeated");
    event(0x3008, EventKind::PuzzleSolved, "event.puzzle_solved");
    event(0x3009, EventKind::EscortCompleted, "event.escort_completed");
    return schemas;
}
uint32_t Type(NativeCategory category, uint8_t kind)
{
    for (const auto &schema : NativeSchemas())
        if (schema.category == category && schema.kind == kind)
            return schema.typeId;
    throw std::invalid_argument("Unknown native mission kind");
}
uint8_t Kind(uint32_t id, uint16_t version, NativeCategory category)
{
    const auto *schema = FindNativeSchema(id);
    if (!schema || schema->category != category || version != schema->version)
        throw std::invalid_argument("Unsupported native type/category/schema version");
    return schema->kind;
}
} // namespace
std::span<const NativeSchema> NativeSchemas()
{
    static const auto schemas = MakeSchemas();
    return schemas;
}
const NativeSchema *FindNativeSchema(uint32_t id)
{
    const auto schemas = NativeSchemas();
    const auto found =
        std::find_if(schemas.begin(), schemas.end(), [&](const auto &s) { return s.typeId == id; });
    return found == schemas.end() ? nullptr : &*found;
}
uint32_t NativeType(NodeKind k)
{
    return Type(NativeCategory::Flow, uint8_t(k));
}
uint32_t NativeType(ConditionKind k)
{
    return Type(NativeCategory::Predicate, uint8_t(k));
}
uint32_t NativeType(EventKind k)
{
    return Type(NativeCategory::Event, uint8_t(k));
}
NodeKind NativeNode(uint32_t id, uint16_t v)
{
    return NodeKind(Kind(id, v, NativeCategory::Flow));
}
ConditionKind NativeCondition(uint32_t id, uint16_t v)
{
    return ConditionKind(Kind(id, v, NativeCategory::Predicate));
}
EventKind NativeEvent(uint32_t id, uint16_t v)
{
    return EventKind(Kind(id, v, NativeCategory::Event));
}
NativeRequirements RequiredNativeTypes(const Definition &definition)
{
    NativeRequirements required;
    const auto condition = [&](const auto &self, const Condition &c) -> void
    {
        required.emplace(NativeType(c.kind), 1);
        for (const auto &child : c.children)
            self(self, child);
    };
    condition(condition, definition.prerequisite);
    for (const auto &[key, c] : definition.predicates)
        condition(condition, c);
    for (const auto &node : definition.nodes)
    {
        required.emplace(NativeType(node.kind), 1);
        if (node.kind == NodeKind::Objective)
            required.emplace(NativeType(node.objective.event), 1);
        if (node.kind == NodeKind::Condition)
            condition(condition, node.condition);
        for (const auto &arm : node.branches)
            condition(condition, arm.when);
    }
    return required;
}
std::string ExportNativeSchemas()
{
    nlohmann::json output = {
        {"schema", 1}, {"scripting", "none"}, {"scope", "domain_kernel"}, {"types", nlohmann::json::array()}};
    output["graph_model"] = "structured_tree_v1";
    output["arbitrary_port_edges"] = false;
    output["condition_representation"] = "embedded_typed_ast";
    output["parallel_policies"] = {{"all", 0}, {"any", 1}, {"race", 2}, {"first_terminal", 3}};
    output["port_types"] = nlohmann::json::array();
    for (const auto &[type, name] :
         std::vector<std::pair<PortType, std::string_view>>{{PortType::Exec, "Exec"},
                                                            {PortType::Bool, "Bool"},
                                                            {PortType::Int64, "Int64"},
                                                            {PortType::StringKey, "StringKey"},
                                                            {PortType::TimeDuration, "TimeDuration"},
                                                            {PortType::StoryFlagKey, "StoryFlagKey"},
                                                            {PortType::Enum, "Enum"},
                                                            {PortType::NodeRef, "NodeRef"},
                                                            {PortType::Predicate, "Predicate"},
                                                            {PortType::EventType, "EventType"},
                                                            {PortType::RewardKey, "RewardKey"},
                                                            {PortType::EventPayload, "EventPayload"},
                                                            {PortType::BranchArm, "BranchArm"}})
        output["port_types"].push_back({{"type_id", uint16_t(type)}, {"name", name}});
    output["composite_types"] = {
        {"branch_arm",
         {{"type_id", uint16_t(PortType::BranchArm)},
          {"fields", {{"when", uint16_t(PortType::Predicate)}, {"child", uint16_t(PortType::NodeRef)}}}}}};
    for (const auto &schema : NativeSchemas())
    {
        nlohmann::json row = {{"type_id", schema.typeId},
                              {"schema_version", schema.version},
                              {"category", uint8_t(schema.category)},
                              {"name", schema.name},
                              {"handler", schema.handler},
                              {"gameplay_bound", schema.gameplayBound},
                              {"ports", nlohmann::json::array()},
                              {"parameters", nlohmann::json::array()}};
        for (const auto &port : schema.ports)
            row["ports"].push_back({{"port_id", port.id},
                                    {"name", port.name},
                                    {"type", uint16_t(port.type)},
                                    {"input", port.input},
                                    {"repeated", port.repeated}});
        for (const auto &param : schema.parameters)
        {
            nlohmann::json p = {{"parameter_id", param.id},
                                {"name", param.name},
                                {"type", uint16_t(param.type)},
                                {"required", param.required},
                                {"repeated", param.repeated}};
            if (param.maximum)
            {
                p["minimum"] = param.minimum;
                p["maximum"] = param.maximum;
            }
            if (param.defaultValue)
                std::visit([&](const auto &value) { p["default"] = value; }, *param.defaultValue);
            row["parameters"].push_back(std::move(p));
        }
        output["types"].push_back(std::move(row));
    }
    return output.dump(2) + '\n';
}
} // namespace auriga::mission
