#pragma once

#include <cstdint>
#include <unordered_map>

#include <entt/entt.hpp>

class CItem;

class CItemRegistry {
public:
    static CItemRegistry& Instance();

    void Register(uint32_t itemID, entt::entity e);
    void Register(uint32_t itemID, uint32_t itemVID, entt::entity e);
    void Register(uint32_t itemID, uint32_t itemVID, const CItem* legacyItem, entt::entity e);
    void Unregister(uint32_t itemID);
    void Unregister(uint32_t itemID, entt::entity expectedEntity);
    void Unregister(entt::entity expectedEntity);
    entt::entity Find(uint32_t itemID) const;
    entt::entity FindByVID(uint32_t itemVID) const;
    entt::entity FindByLegacy(const CItem* legacyItem) const;

private:
    void UnregisterLegacy(entt::entity e);

    std::unordered_map<uint32_t, entt::entity> m_byID;
    std::unordered_map<uint32_t, entt::entity> m_byVID;
    std::unordered_map<uint32_t, uint32_t> m_idToVID;
    std::unordered_map<uint32_t, uint32_t> m_vidToID;
    std::unordered_map<const CItem*, entt::entity> m_byLegacy;
    std::unordered_map<entt::entity, const CItem*> m_entityToLegacy;
};
