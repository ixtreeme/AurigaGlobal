#pragma once
#include "MissionDefinition.hpp"
#include <optional>
#include <span>
#include <variant>

namespace auriga::mission
{
// Wire IDs belong to this registry, never to the ordinal of a C++ enum.
enum class NativeCategory : uint8_t
{
    Flow = 1,
    Predicate = 2,
    Event = 3
};
enum class PortType : uint16_t
{
    Exec = 1,
    Bool = 2,
    Int64 = 3,
    Float64 = 4,
    StringKey = 5,
    EntityRef = 6,
    PlayerRef = 7,
    NpcRef = 8,
    MobProtoId = 9,
    ItemProtoId = 10,
    MissionId = 11,
    ObjectiveId = 12,
    RegionId = 13,
    DungeonId = 14,
    EventPayload = 15,
    TimeDuration = 16,
    StoryFlagKey = 17,
    Enum = 18,
    NodeRef = 19,
    Predicate = 20,
    EventType = 21,
    RewardKey = 22,
    BranchArm = 23
};
struct NativePort
{
    uint16_t id;
    std::string_view name;
    PortType type;
    bool input = true, repeated = false;
};
using NativeDefault = std::variant<bool, int64_t, std::string_view>;
struct NativeParameter
{
    uint16_t id;
    std::string_view name;
    PortType type;
    bool required = true, repeated = false;
    int64_t minimum = 0, maximum = 0;
    std::optional<NativeDefault> defaultValue;
};
struct NativeSchema
{
    uint32_t typeId;
    uint16_t version = 1;
    NativeCategory category;
    uint8_t kind;
    std::string_view name, handler;
    std::vector<NativePort> ports;
    std::vector<NativeParameter> parameters;
    // Implemented in the domain kernel is distinct from a bound gameplay producer.
    bool gameplayBound = false;
};
using NativeRequirements = std::map<uint32_t, uint16_t>;
std::span<const NativeSchema> NativeSchemas();
const NativeSchema *FindNativeSchema(uint32_t typeId);
uint32_t NativeType(NodeKind kind);
uint32_t NativeType(ConditionKind kind);
uint32_t NativeType(EventKind kind);
NodeKind NativeNode(uint32_t typeId, uint16_t version);
ConditionKind NativeCondition(uint32_t typeId, uint16_t version);
EventKind NativeEvent(uint32_t typeId, uint16_t version);
NativeRequirements RequiredNativeTypes(const Definition &definition);
// Machine-readable Studio schema; describes implemented primitives, not future handlers.
std::string ExportNativeSchemas();
} // namespace auriga::mission
