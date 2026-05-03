#pragma once

#include <cstddef>
#include <cstdint>

#include <entt/entt.hpp>

#include "../desc.h"
#include "Registry.hpp"
#include "components/session_components.hpp"
#include "components/transform_components.hpp"
#include "services/VisibilityService.hpp"

namespace ecs::NetworkService {

inline bool Send(entt::entity recipient, const void* packet, std::size_t size)
{
    if (recipient == entt::null || !g_registry.valid(recipient))
        return false;

    const auto* session = g_registry.try_get<ecs::NetworkSession>(recipient);
    if (!session || !session->desc)
        return false;

    session->desc->Packet(packet, static_cast<int>(size));
    return true;
}

inline uint8_t GetLanguage(entt::entity sessionOwner)
{
    if (sessionOwner == entt::null || !g_registry.valid(sessionOwner))
        return 0;

    const auto* session = g_registry.try_get<ecs::NetworkSession>(sessionOwner);
    if (!session || !session->desc)
        return 0;

    return session->desc->GetLanguage();
}

inline void Broadcast(entt::registry& reg, entt::entity source, const void* packet, std::size_t size, int32_t range, bool excludeSource = true)
{
    const auto recipients = ecs::VisibilityService::GetEntitiesInRange(reg, source, range);
    for (const auto recipient : recipients) {
        if (excludeSource && recipient == source)
            continue;
        Send(recipient, packet, size);
    }
}

inline void BroadcastToView(entt::registry& reg, entt::entity source, const void* packet, std::size_t size, bool excludeSource = true)
{
    const auto recipients = ecs::VisibilityService::GetViewersOf(reg, source);
    for (const auto recipient : recipients) {
        if (excludeSource && recipient == source)
            continue;
        Send(recipient, packet, size);
    }
}

inline void BroadcastInMap(entt::registry& reg, int32_t mapIndex, const void* packet, std::size_t size)
{
    auto view = reg.view<ecs::MapIndex, ecs::NetworkSession>();
    for (const auto entity : view) {
        const auto& currentMap = view.get<ecs::MapIndex>(entity);
        if (currentMap.value != mapIndex)
            continue;
        Send(entity, packet, size);
    }
}

} // namespace ecs::NetworkService
