#pragma once

#include <cstddef>
#include <entt/entity/fwd.hpp>

inline constexpr std::size_t TypedWikiSourcesRequestSize = 12;
void HandleTypedWikiSources(entt::entity player, const char* data);

inline constexpr std::size_t TypedWikiLootRequestSize = 13;
void HandleTypedWikiLoot(entt::entity player, const char* data);

inline constexpr std::size_t TypedWikiRefinesRequestSize = 12;
void HandleTypedWikiRefines(entt::entity player, const char* data);
