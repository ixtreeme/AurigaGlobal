#include "ItemRegistry.hpp"
#include "Registry.hpp"
#include "components/item_components.hpp"

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
    Register(itemID, itemVID, nullptr, e);
}

void CItemRegistry::Register(uint32_t itemID, uint32_t itemVID, const CItem* legacyItem, entt::entity e)
{
    if ((itemID == 0 && itemVID == 0) || e == entt::null || !g_registry.valid(e))
        return;

    if (itemID != 0) {
        if (const auto previousItem = m_byID.find(itemID);
            previousItem != m_byID.end() && previousItem->second != e) {
            Unregister(previousItem->second);
        }

        const auto oldVID = m_idToVID.find(itemID);
        if (oldVID != m_idToVID.end() && oldVID->second != itemVID) {
            const auto oldVIDEntity = m_byVID.find(oldVID->second);
            if (oldVIDEntity != m_byVID.end() && oldVIDEntity->second == e)
                m_byVID.erase(oldVIDEntity);
            m_vidToID.erase(oldVID->second);
        }

        m_byID[itemID] = e;
    }

    if (itemVID != 0) {
        if (const auto previousVIDEntity = m_byVID.find(itemVID);
            previousVIDEntity != m_byVID.end() && previousVIDEntity->second != e) {
            if (const auto previousOwner = m_vidToID.find(itemVID);
                previousOwner != m_vidToID.end() && previousOwner->second != 0) {
                m_idToVID.erase(previousOwner->second);
            }
        }
        m_byVID[itemVID] = e;
        if (itemID != 0)
            m_idToVID[itemID] = itemVID;
        m_vidToID[itemVID] = itemID;
    } else if (itemID != 0) {
        m_idToVID.erase(itemID);
    }

    if (legacyItem != nullptr) {
        if (const auto previousLegacy = m_entityToLegacy.find(e);
            previousLegacy != m_entityToLegacy.end() && previousLegacy->second != legacyItem) {
            m_byLegacy.erase(previousLegacy->second);
        }

        if (const auto previousEntity = m_byLegacy.find(legacyItem);
            previousEntity != m_byLegacy.end() && previousEntity->second != e) {
            m_entityToLegacy.erase(previousEntity->second);
        }

        m_byLegacy[legacyItem] = e;
        m_entityToLegacy[e] = legacyItem;
    }
}

void CItemRegistry::UnregisterLegacy(entt::entity e)
{
    const auto legacy = m_entityToLegacy.find(e);
    if (legacy == m_entityToLegacy.end())
        return;

    const auto reverse = m_byLegacy.find(legacy->second);
    if (reverse != m_byLegacy.end() && reverse->second == e)
        m_byLegacy.erase(reverse);

    m_entityToLegacy.erase(legacy);
}

void CItemRegistry::Unregister(uint32_t itemID)
{
    const auto current = m_byID.find(itemID);
    if (current != m_byID.end()) {
        Unregister(current->second);
        return;
    }

    const auto oldVID = m_idToVID.find(itemID);
    if (oldVID != m_idToVID.end()) {
        m_byVID.erase(oldVID->second);
        m_vidToID.erase(oldVID->second);
        m_idToVID.erase(oldVID);
    }

    m_byID.erase(itemID);
}

void CItemRegistry::Unregister(uint32_t itemID, entt::entity expectedEntity)
{
    const auto current = m_byID.find(itemID);
    if (current == m_byID.end() || current->second != expectedEntity)
        return;

    Unregister(itemID);
}

void CItemRegistry::Unregister(entt::entity expectedEntity)
{
    if (expectedEntity == entt::null)
        return;

    UnregisterLegacy(expectedEntity);

    for (auto it = m_byID.begin(); it != m_byID.end(); ) {
        if (it->second != expectedEntity) {
            ++it;
            continue;
        }

        const uint32_t itemID = it->first;
        if (const auto vid = m_idToVID.find(itemID); vid != m_idToVID.end()) {
            const auto vidEntity = m_byVID.find(vid->second);
            if (vidEntity != m_byVID.end() && vidEntity->second == expectedEntity)
                m_byVID.erase(vidEntity);
            m_vidToID.erase(vid->second);
            m_idToVID.erase(vid);
        }
        it = m_byID.erase(it);
    }

    for (auto it = m_byVID.begin(); it != m_byVID.end(); ) {
        if (it->second != expectedEntity) {
            ++it;
            continue;
        }

        m_vidToID.erase(it->first);
        it = m_byVID.erase(it);
    }
}

entt::entity CItemRegistry::Find(uint32_t itemID) const
{
    const auto it = m_byID.find(itemID);
    return it != m_byID.end() && g_registry.valid(it->second) ? it->second : entt::null;
}

entt::entity CItemRegistry::FindByVID(uint32_t itemVID) const
{
    const auto it = m_byVID.find(itemVID);
    return it != m_byVID.end() && g_registry.valid(it->second) ? it->second : entt::null;
}

entt::entity CItemRegistry::FindByLegacy(const CItem* legacyItem) const
{
    if (legacyItem == nullptr)
        return entt::null;

    const auto it = m_byLegacy.find(legacyItem);
    if (it == m_byLegacy.end() || !g_registry.valid(it->second))
        return entt::null;

    const auto* legacy = g_registry.try_get<ecs::LegacyItemPtr>(it->second);
    return legacy != nullptr && legacy->ptr == legacyItem ? it->second : entt::null;
}
