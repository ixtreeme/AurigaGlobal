#pragma once

#include <cstdint>
#include <unordered_map>

#include <entt/entt.hpp>

class CVIDRegistry {
public:
    static CVIDRegistry& Instance();

    entt::entity Find(uint32_t vid) const;
    void Register(uint32_t vid, entt::entity e);
    void Unregister(uint32_t vid);

private:
    std::unordered_map<uint32_t, entt::entity> m_map;
};
