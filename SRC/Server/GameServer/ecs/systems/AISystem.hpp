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
bool StartStateMachine(entt::entity e, int nextPulse);

// The two state bodies. Neither is a CHARACTER method any more.
void StateIdle(entt::entity e);
void StateBattle(entt::entity e);

} // namespace AISystem

void AISystem_Update(entt::registry& reg, uint32_t tick);
