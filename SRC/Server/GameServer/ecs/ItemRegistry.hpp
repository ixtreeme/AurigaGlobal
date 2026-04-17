#pragma once

#include <cstdint>
#include <unordered_map>

#include <entt/entt.hpp>

class CItemRegistry {
public:
    static CItemRegistry& Instance();

    void Register(uint32_t itemID, entt::entity e);
    void Unregister(uint32_t itemID);
    entt::entity Find(uint32_t itemID) const;

private:
    std::unordered_map<uint32_t, entt::entity> m_map;
};
