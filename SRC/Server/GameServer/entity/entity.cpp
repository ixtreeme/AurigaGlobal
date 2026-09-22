#include "stdafx.h"
#include "desc.h"
#include "../ecs/Registry.hpp"
#include "../ecs/SpatialHelpers.hpp"
#include "../ecs/components/spatial_components.hpp"
#include "../ecs/services/VisibilityService.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/ViewSystem.hpp"

namespace ecs::ViewSystem {

void PacketView(entt::entity self, const void* data, int bytes, entt::entity except)
{
    if (!g_registry.valid(self) || !ecs::SectorOf(g_registry, self)) return;
    const auto* revision = g_registry.try_get<ecs::SpatialRevision>(self);
    const uint64_t version = revision ? revision->value : 0;
    const auto recipients = ecs::VisibilityService::GetViewersOf(g_registry, self);
    for (auto target : recipients) {
        if (!g_registry.valid(self)) return;
        const auto* current = g_registry.try_get<ecs::SpatialRevision>(self);
        if ((current ? current->value : 0) != version) return;
        if (target == except || !g_registry.valid(target)) continue;
        if (auto* desc = ecs::PlayerRuntime::GetDesc(target)) desc->Packet(data, bytes);
    }
}

} // namespace ecs::ViewSystem


