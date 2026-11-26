#pragma once
#include <cstdint>
#include <entt/entity/entity.hpp>

// Normal commands share the same persistence/admission path as CG_TYPED_COMPANION.
enum class TypedMountCommand : std::uint8_t { ToggleRide, Dismount, Recall };
bool HasTypedMountIntent(entt::entity);
bool HandleTypedMountCommand(entt::entity, TypedMountCommand);
