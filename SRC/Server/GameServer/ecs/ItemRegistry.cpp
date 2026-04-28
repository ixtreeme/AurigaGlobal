#include "ItemRegistry.hpp"

CItemRegistry& CItemRegistry::Instance()
{
    static CItemRegistry instance;
    return instance;
}

void CItemRegistry::Register(uint32_t itemID, entt::entity e)
{
    Register(itemID, 0, e);
}

void CItemRegistry::Register(uint32_t itemID, uint32_t itemVID, entt::entity e)
{
    if (itemID == 0)
        return;

    const auto oldVID = m_idToVID.find(itemID);
    if (oldVID != m_idToVID.end() && oldVID->second != itemVID)
        m_byVID.erase(oldVID->second);

    m_byID[itemID] = e;

    if (itemVID != 0) {
        m_byVID[itemVID] = e;
        m_idToVID[itemID] = itemVID;
    } else {
        m_idToVID.erase(itemID);
    }
}

void CItemRegistry::Unregister(uint32_t itemID)
{
    const auto oldVID = m_idToVID.find(itemID);
    if (oldVID != m_idToVID.end()) {
        m_byVID.erase(oldVID->second);
        m_idToVID.erase(oldVID);
    }

    m_byID.erase(itemID);
}

entt::entity CItemRegistry::Find(uint32_t itemID) const
{
    const auto it = m_byID.find(itemID);
    return it != m_byID.end() ? it->second : entt::null;
}

entt::entity CItemRegistry::FindByVID(uint32_t itemVID) const
{
    const auto it = m_byVID.find(itemVID);
    return it != m_byVID.end() ? it->second : entt::null;
}
