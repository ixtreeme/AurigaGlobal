#pragma once

#include <cstdint>
#include <unordered_map>
#include <entt/entt.hpp>

class CItemRegistry {
public:
    static CItemRegistry& Instance();

    // Bind only the entity's current identity. A live ID/VID collision fails
    // without replacing another item. No legacy allocation is indexed here.
    [[nodiscard]] bool Register(uint32_t itemID, entt::entity e);
    [[nodiscard]] bool Register(uint32_t itemID, uint32_t itemVID, entt::entity e);
    void Unregister(uint32_t itemID);
    void Unregister(uint32_t itemID, entt::entity expectedEntity);
    void Unregister(entt::entity expectedEntity);
    entt::entity Find(uint32_t itemID) const;
    entt::entity FindByVID(uint32_t itemVID) const;

private:
    struct Keys { uint32_t id {0}; uint32_t vid {0}; };
    std::unordered_map<uint32_t, entt::entity> m_byID;
    std::unordered_map<uint32_t, entt::entity> m_byVID;
    std::unordered_map<entt::entity, Keys> m_keys;
};
