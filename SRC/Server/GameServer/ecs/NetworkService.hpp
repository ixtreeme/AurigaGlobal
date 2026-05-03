#pragma once

#include <cstddef>
#include <cstdint>

#include <entt/entt.hpp>

#include "../desc.h"
#include <Core/Logging.hpp>
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

inline bool SendBufferedPair(entt::entity recipient,
    const void* first,
    std::size_t firstSize,
    const void* second,
    std::size_t secondSize)
{
    if (recipient == entt::null || !g_registry.valid(recipient))
        return false;

    const auto* session = g_registry.try_get<ecs::NetworkSession>(recipient);
    if (!session || !session->desc)
        return false;

    session->desc->BufferedPacket(first, static_cast<int>(firstSize));
    session->desc->Packet(second, static_cast<int>(secondSize));
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
    // Keep legacy/range visibility authoritative for live broadcasts until
    // LPENTITY native view ownership is proven under movement load. A stale
    // ViewerMap mirror causes visible multi-second client sync lag.
    const auto recipients = ecs::VisibilityService::GetViewersOf(reg, source);
#ifndef NDEBUG
    const auto nativeRecipients = ecs::VisibilityService::GetViewersOfNative(reg, source);
    if (nativeRecipients.size() != recipients.size()) {
        LOG_WARN("[BROADCAST_DRIFT] entity={} native={} legacy={}",
            static_cast<uint32_t>(source),
            nativeRecipients.size(),
            recipients.size());
    }
#endif
    for (const auto recipient : recipients) {
        if (excludeSource && recipient == source)
            continue;
        Send(recipient, packet, size);
    }

    if (!excludeSource)
        Send(source, packet, size);
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
