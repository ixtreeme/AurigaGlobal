#pragma once

#include <entt/entt.hpp>

#include <source_location>
#include <string>
#include <string_view>

// Locating an ECS bug in this codebase has repeatedly meant knowing three
// things the logs did not carry: which entity, what was wrong with it, and
// where the access happened. ecs::Invariants answers none of them - it prints
// a raw handle with a hand-written context string, and every one of its checks
// returns silently when the entity is invalid, which is the case that actually
// bites. This layer fills that gap.
//
// The call site comes from std::source_location, so it cannot go stale the way
// a hand-typed context string does, and every report is throttled per site: a
// fault on a per-frame path prints once and then at each power of ten, never
// the 15,000 lines a two-megabyte log held the last time a per-entity check ran
// unthrottled.
namespace ecs::diag {

// Decodes a handle into something you can act on. EnTT packs a slot index and
// a generation into the identifier, so "invalid" has distinct causes worth
// telling apart:
//
//   null                                        - never set
//   stale(slot=12 gen=3, slot is now at gen=7)  - a use-after-destroy. EnTT
//                                                 bumps the generation when the
//                                                 entity dies, so every handle
//                                                 to something destroyed lands
//                                                 here, recycled or not
//   never-lived(slot=12 gen=3)                  - fabricated or out of range
//   live(slot=12 gen=7 vid=904 pid=51 'Nevem' PC)
std::string Describe(entt::entity e);

// True when the handle is usable. When it is not, reports what is wrong, the
// decoded handle and the call site. `what` names the operation being refused,
// e.g. "SetProto".
bool Check(entt::entity e, std::string_view what,
    const std::source_location& where = std::source_location::current());

// Component access that explains itself. Returns nullptr and reports when the
// entity is unusable or the component is absent.
template <typename T>
T* Require(entt::entity e, std::string_view what,
    const std::source_location& where = std::source_location::current());

// Used by the Require template; also callable directly for a report that is
// neither a handle nor a component problem.
void Report(const std::source_location& where, const std::string& message);

} // namespace ecs::diag

#include "EcsDiagnostics.inl"
