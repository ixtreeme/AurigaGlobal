#include "ItemRegistry.hpp"
#include "Registry.hpp"
#include "components/item_components.hpp"

namespace {
const ecs::ItemIdentity* Identity(entt::entity e)
{
    return g_registry.valid(e) ? g_registry.try_get<ecs::ItemIdentity>(e) : nullptr;
}
template <typename Map>
void EraseOwned(Map& index, uint32_t key, entt::entity owner)
{
    const auto found = index.find(key);
    if (found != index.end() && found->second == owner)
        index.erase(found);
}
}

CItemRegistry& CItemRegistry::Instance()
{
    static CItemRegistry instance;
    return instance;
}

bool CItemRegistry::Register(uint32_t itemID, entt::entity e)
{
    const auto* identity = Identity(e);
    return identity && Register(itemID, identity->vid, e);
}

bool CItemRegistry::Register(uint32_t itemID, uint32_t itemVID, entt::entity e)
{
    const auto* identity = Identity(e);
    if (!identity || (!itemID && !itemVID) || identity->id != itemID || identity->vid != itemVID)
        return false;
    const auto byID = Find(itemID), byVID = FindByVID(itemVID);
    if ((byID != entt::null && byID != e) || (byVID != entt::null && byVID != e))
        return false;

    // Preallocate all nodes before publishing either index. Allocation failure
    // leaves existing bindings untouched and removes only our empty new nodes.
    bool newRecord = false, newID = false, newVID = false;
    try {
        newRecord = m_keys.try_emplace(e).second;
        if (itemID) newID = m_byID.try_emplace(itemID, entt::null).second;
        if (itemVID) newVID = m_byVID.try_emplace(itemVID, entt::null).second;
    } catch (...) {
        if (newVID) m_byVID.erase(itemVID);
        if (newID) m_byID.erase(itemID);
        if (newRecord) m_keys.erase(e);
        throw;
    }

    auto& keys = m_keys.at(e);
    if (keys.id != itemID) EraseOwned(m_byID, keys.id, e);
    if (keys.vid != itemVID) EraseOwned(m_byVID, keys.vid, e);
    if (itemID) m_byID.at(itemID) = e;
    if (itemVID) m_byVID.at(itemVID) = e;
    keys = {itemID, itemVID};
    return true;
}

void CItemRegistry::Unregister(uint32_t itemID)
{
    const auto current = m_byID.find(itemID);
    if (current != m_byID.end())
        Unregister(current->second);
}

void CItemRegistry::Unregister(uint32_t itemID, entt::entity expectedEntity)
{
    const auto current = m_byID.find(itemID);
    if (current != m_byID.end() && current->second == expectedEntity)
        Unregister(expectedEntity);
}

void CItemRegistry::Unregister(entt::entity expectedEntity)
{
    const auto current = m_keys.find(expectedEntity);
    if (current == m_keys.end())
        return;
    EraseOwned(m_byID, current->second.id, expectedEntity);
    EraseOwned(m_byVID, current->second.vid, expectedEntity);
    m_keys.erase(current);
}

entt::entity CItemRegistry::Find(uint32_t itemID) const
{
    if (!itemID) return entt::null;
    const auto found = m_byID.find(itemID);
    if (found == m_byID.end()) return entt::null;
    const auto* identity = Identity(found->second);
    return identity && identity->id == itemID ? found->second : entt::entity(entt::null);
}

entt::entity CItemRegistry::FindByVID(uint32_t itemVID) const
{
    if (!itemVID) return entt::null;
    const auto found = m_byVID.find(itemVID);
    if (found == m_byVID.end()) return entt::null;
    const auto* identity = Identity(found->second);
    return identity && identity->vid == itemVID ? found->second : entt::entity(entt::null);
}
