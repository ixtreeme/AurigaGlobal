#include "PIDRegistry.hpp"

CPIDRegistry& CPIDRegistry::Instance()
{
    static CPIDRegistry instance;
    return instance;
}

entt::entity CPIDRegistry::Find(uint32_t pid) const
{
    const auto it = m_map.find(pid);
    return it != m_map.end() ? it->second : entt::null;
}

void CPIDRegistry::Register(uint32_t pid, entt::entity e)
{
    m_map[pid] = e;
}

void CPIDRegistry::Unregister(uint32_t pid)
{
    m_map.erase(pid);
}
