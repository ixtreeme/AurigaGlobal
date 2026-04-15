#include "../../stdafx.h"

#include "MovementSystem.hpp"

#include <cmath>

#include "../components/dirty_components.hpp"
#include "../components/movement_components.hpp"
#include "../components/transform_components.hpp"
#include "../events.hpp"
#include "../EventDispatcher.hpp"

void MovementSystem_Update(entt::registry& reg, uint32_t tick)
{
    // migrated from CHARACTER::Goto
    auto view = reg.view<ecs::Position, ecs::MovementDestination, ecs::MovementState>();

    view.each([&](const entt::entity entity, ecs::Position& position, ecs::MovementDestination& destination, ecs::MovementState& movementState) {
        const int32_t dx = destination.x - position.x;
        const int32_t dy = destination.y - position.y;

        if (dx == 0 && dy == 0) {
            movementState.lastMoveTime = tick;
            movementState.stopTime = tick;
            reg.remove<ecs::MovementDestination>(entity);
            reg.emplace_or_replace<ecs::DirtyTag>(entity);
            return;
        }

        int32_t step = 200;
        if (const auto* movementSpeed = reg.try_get<ecs::MovementSpeed>(entity)) {
            step = std::max<int32_t>(1, movementSpeed->run);
        }

        const double distance = std::sqrt(static_cast<double>(dx) * dx + static_cast<double>(dy) * dy);
        if (distance <= step) {
            position.x = destination.x;
            position.y = destination.y;
            movementState.lastMoveTime = tick;
            movementState.stopTime = tick;
            movementState.moveDuration = 0;
            reg.remove<ecs::MovementDestination>(entity);
        } else {
            const double ratio = static_cast<double>(step) / distance;
            position.x += static_cast<int32_t>(std::round(dx * ratio));
            position.y += static_cast<int32_t>(std::round(dy * ratio));
            movementState.moveStartTime = tick;
            movementState.moveDuration = 1;
            movementState.lastMoveTime = tick;
            movementState.isWalking = true;
            movementState.isNowWalking = true;
        }

        reg.emplace_or_replace<ecs::DirtyTag>(entity);
        g_dispatcher.trigger(ecs::EvEntityMoved { entity, position.x, position.y });
    });
}
