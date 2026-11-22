#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <entt/entity/fwd.hpp>

inline constexpr std::size_t TypedChatItemRequestSize = 38;

// Returns true when the message was handled, including when it was refused.
bool PrepareTypedChatLinks(entt::entity sender, std::string_view message, std::size_t maxSize,
    std::function<void(std::string)> deliver);
void HandleTypedChatItem(entt::entity player, const char* data);
