#pragma once

#include <cstdint>
#include <string_view>
#include <entt/entity/fwd.hpp>

namespace auriga::item { enum class ToolKind : std::uint8_t; }

std::uint64_t EquippedFieldTool(entt::entity character, auriga::item::ToolKind kind);
bool IsFieldGatheringNode(entt::entity node);
bool TypedFieldGatheringActive(entt::entity character);
bool FieldGatheringRewardExists(std::string_view code);
void CompleteFieldGatheringHit(entt::entity character, std::uint64_t tool,
    auriga::item::ToolKind kind, std::string_view rewardCode, unsigned rewardPercent);
