#pragma once

#include <cstdint>
#include <unordered_map>

#include <entt/entt.hpp>

class CItemRegistry {
public:
    static CItemRegistry& Instance();

    void Register(uint32_t itemID, entt::entity e);
    void Register(uint32_t itemID, uint32_t itemVID, entt::entity e);
    void Unregister(uint32_t itemID);
    entt::entity Find(uint32_t itemID) const;
    entt::entity FindByVID(uint32_t itemVID) const;

private:
    std::unordered_map<uint32_t, entt::entity> m_byID;
    std::unordered_map<uint32_t, entt::entity> m_byVID;
    std::unordered_map<uint32_t, uint32_t> m_idToVID;
};
