#pragma once

#include "systems/MovementSystem.hpp"
#include <utility>

namespace ecs {

template<class UpdateCharacters>
void UpdateCharacterFrame(entt::registry& registry, uint32_t tick, UpdateCharacters&& updateCharacters)
{
    // Consume elapsed time on the previous segment before AI can replace its
    // destination/start time. The reverse order starves every continuously
    // retargeted NPC: each movement tick observes a freshly reset clock.
    MovementSystem_Update(registry, tick);
    std::forward<UpdateCharacters>(updateCharacters)();
}

} // namespace ecs
