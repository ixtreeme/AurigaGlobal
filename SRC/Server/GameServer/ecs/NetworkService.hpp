#pragma once

#include <cstddef>
#include <cstdint>

#include <entt/entt.hpp>

#include "../desc.h"
#include "Registry.hpp"
#include "components/session_components.hpp"

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

} // namespace ecs::NetworkService
