#pragma once

#include <cstddef>

#include <entt/entt.hpp>

#include "../desc.h"
#include "../desc_manager.h"
#include "../log.h"
#include "Registry.hpp"
#include "components/session_components.hpp"

namespace ecs::NetworkService {

inline bool Send(entt::entity recipient, const void* packet, std::size_t size)
{
    if (recipient == entt::null || !g_registry.valid(recipient) || !packet || size == 0) {
        return false;
    }

    const auto* session = g_registry.try_get<ecs::NetworkSession>(recipient);
    if (!session || !session->desc) {
        return false;
    }

    session->desc->Packet(packet, static_cast<int>(size));
    return true;
}

inline void Broadcast(entt::registry& reg, entt::entity source, const void* packet, std::size_t size, int range)
{
    (void)reg;
    (void)range;
    (void)packet;
    (void)size;

    static bool warned = false;
    if (!warned) {
        warned = true;
        LOG_WARN("[NETWORK_SERVICE] Broadcast requested for entity={} before native visibility service is implemented",
                 static_cast<uint32_t>(source));
    }
}

inline void SendToAll(const void* packet, std::size_t size)
{
    if (!packet || size == 0) {
        return;
    }

    const DESC_MANAGER::DESC_SET& descSet = DESC_MANAGER::instance().GetClientSet();
    for (auto* desc : descSet) {
        if (desc) {
            desc->Packet(packet, static_cast<int>(size));
        }
    }
}

} // namespace ecs::NetworkService
