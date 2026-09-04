#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include <entt/entt.hpp>

// PID -> entity, the ECS-side counterpart of CVIDRegistry.
//
// Without this, CHARACTER_MANAGER::FindEntityByPID had to go through
// FindByPID and take the handle off the legacy pointer it returned - a lookup
// in the pointer map followed by a conversion back. This index is written at
// the same point the PlayerID component is, so the entity is reachable from a
// PID without touching CHARACTER at all.
class CPIDRegistry {
public:
    static CPIDRegistry& Instance();

    entt::entity Find(uint32_t pid) const;
    void Register(uint32_t pid, entt::entity e);
    void Unregister(uint32_t pid);

    // A snapshot, not a view into the map: callers destroy characters while
    // iterating, which unregisters and would invalidate an iterator.
    std::vector<entt::entity> Snapshot() const;

private:
    std::unordered_map<uint32_t, entt::entity> m_map;
};
