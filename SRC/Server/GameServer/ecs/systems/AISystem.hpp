#pragma once

#include <cstdint>

#include <entt/entt.hpp>

namespace ecs {
enum class AIFSMState : uint8_t;
}

namespace AISystem {

// Replaces CFSM::GotoState and CFSM::Update on CHARACTER. The state lives
// in the AIStateMachine component.
void GotoState(entt::entity e, ecs::AIFSMState state);
void UpdateStateMachine(entt::entity e);

// The idle state body. Battle is still a CHARACTER method.
void StateIdle(entt::entity e);

} // namespace AISystem

void AISystem_Update(entt::registry& reg, uint32_t tick);
