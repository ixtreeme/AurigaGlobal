#include "stdafx.h"
#include "desc.h"
#include "../ecs/Registry.hpp"
#include "../ecs/SpatialHelpers.hpp"
#include "../ecs/components/spatial_components.hpp"
#include "../ecs/services/VisibilityService.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/ViewSystem.hpp"
#include "../story/CaravanQuestSystem.hpp"

namespace ecs::ViewSystem {

void PacketView(entt::entity self, const void* data, int bytes, entt::entity except)
{
    if (!g_registry.valid(self) || !ecs::SectorOf(g_registry, self)) return;
    // Same rule as NetworkService::BroadcastToView: once ViewCleanup has taken
    // SpatialEntity away (DestroyCharacter, until StatePost leaves the sector),
    // every client has dropped this source. Only its own client remains.
    if (!g_registry.all_of<ecs::SpatialEntity>(self)) {
        if (self != except)
            if (auto* desc = ecs::PlayerRuntime::GetDesc(self)) desc->Packet(data, bytes);
        return;
    }
    const auto* revision = g_registry.try_get<ecs::SpatialRevision>(self);
    const uint64_t version = revision ? revision->value : 0;
    auto recipients = ecs::VisibilityService::GetViewersOf(g_registry, self);
    // Story actors remain visible farther away than the ordinary nearby-client
    // packet radius. Their owner must also receive movement, not only ADD/REMOVE.
    if(const auto* actor=g_registry.try_get<SpyQuest::Actor>(self)){
        auto owner=ecs::PlayerRuntime::FindByPlayerID(actor->owner);
        if(g_registry.valid(owner)&&ecs::PlayerRuntime::GetMapIndex(owner)==ecs::PlayerRuntime::GetMapIndex(self)){
            const auto dx=std::abs(int64_t(ecs::PlayerRuntime::GetX(owner))-ecs::PlayerRuntime::GetX(self));
            const auto dy=std::abs(int64_t(ecs::PlayerRuntime::GetY(owner))-ecs::PlayerRuntime::GetY(self));
            if(std::max(dx,dy)+std::min(dx,dy)/2<=10000)ecs::VisibilityService::PushUnique(recipients,owner);
        }
    }
    if(g_registry.any_of<CaravanQuest::Actor,CaravanQuest::Rider>(self))for(auto viewer:g_registry.view<ecs::PlayerID>()){
        if(ecs::PlayerRuntime::GetMapIndex(viewer)!=ecs::PlayerRuntime::GetMapIndex(self))continue;
        const auto dx=std::abs(int64_t(ecs::PlayerRuntime::GetX(viewer))-ecs::PlayerRuntime::GetX(self)),dy=std::abs(int64_t(ecs::PlayerRuntime::GetY(viewer))-ecs::PlayerRuntime::GetY(self));
        if(std::max(dx,dy)+std::min(dx,dy)/2<=10000)ecs::VisibilityService::PushUnique(recipients,viewer);
    }
    for (auto target : recipients) {
        if (!g_registry.valid(self)) return;
        const auto* current = g_registry.try_get<ecs::SpatialRevision>(self);
        if ((current ? current->value : 0) != version) return;
        if (target == except || !g_registry.valid(target)) continue;
        if (auto* desc = ecs::PlayerRuntime::GetDesc(target)) desc->Packet(data, bytes);
    }
}

} // namespace ecs::ViewSystem


