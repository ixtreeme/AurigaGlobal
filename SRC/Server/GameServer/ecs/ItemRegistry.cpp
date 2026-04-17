#include "ItemRegistry.hpp"

CItemRegistry& CItemRegistry::Instance()
{
    static CItemRegistry instance;
    return instance;
}

void CItemRegistry::Register(uint32_t itemID, entt::entity e)
{
    m_map[itemID] = e;
}

void CItemRegistry::Unregister(uint32_t itemID)
{
    m_map.erase(itemID);
}

entt::entity CItemRegistry::Find(uint32_t itemID) const
{
    const auto it = m_map.find(itemID);
    return it != m_map.end() ? it->second : entt::null;
}
