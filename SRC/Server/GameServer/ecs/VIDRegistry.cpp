#include "VIDRegistry.hpp"

CVIDRegistry& CVIDRegistry::Instance()
{
    static CVIDRegistry instance;
    return instance;
}

entt::entity CVIDRegistry::Find(uint32_t vid) const
{
    const auto it = m_map.find(vid);
    return it != m_map.end() ? it->second : entt::null;
}

void CVIDRegistry::Register(uint32_t vid, entt::entity e)
{
    m_map[vid] = e;
}

void CVIDRegistry::Unregister(uint32_t vid)
{
    m_map.erase(vid);
}

std::vector<entt::entity> CVIDRegistry::Snapshot() const
{
    std::vector<entt::entity> out;
    out.reserve(m_map.size());
    for (const auto& [key, entity] : m_map)
        out.push_back(entity);

    return out;
}
