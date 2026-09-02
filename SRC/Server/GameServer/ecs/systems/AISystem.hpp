#pragma once

#include <cstdint>

#include <entt/entt.hpp>

namespace ecs {
enum class AIFSMState : uint8_t;
}

namespace AISystem {

// Replaces CFSM::GotoState and CFSM::Update on CHARACTER. The state lives
// in the AIStateMachine component; the state bodies are still CHARACTER
// methods, so UpdateStateMachine resolves the legacy object once per tick
// at the pump - the same boundary every other system crosses there.
void GotoState(entt::entity e, ecs::AIFSMState state);
void UpdateStateMachine(entt::entity e);

} // namespace AISystem

void AISystem_Update(entt::registry& reg, uint32_t tick);
