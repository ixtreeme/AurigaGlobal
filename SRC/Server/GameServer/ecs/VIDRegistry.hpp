#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include <entt/entt.hpp>

class CVIDRegistry {
public:
    static CVIDRegistry& Instance();

    entt::entity Find(uint32_t vid) const;
    void Register(uint32_t vid, entt::entity e);
    void Unregister(uint32_t vid);

    // A snapshot, not a view into the map: callers destroy characters while
    // iterating, which unregisters and would invalidate an iterator.
    std::vector<entt::entity> Snapshot() const;

private:
    std::unordered_map<uint32_t, entt::entity> m_map;
};
