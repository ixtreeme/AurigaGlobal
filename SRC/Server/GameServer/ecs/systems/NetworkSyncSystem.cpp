#include "../../stdafx.h"

#include "NetworkSyncSystem.hpp"

#include "../components/combat_components.hpp"
#include "../components/dirty_components.hpp"
#include "../components/identity_components.hpp"
#include "../components/session_components.hpp"
#include "../components/transform_components.hpp"
#include "../components/vital_components.hpp"
#include "../../desc.h"
#include "../../packet.h"

void NetworkSyncSystem_Update(entt::registry& reg, uint32_t tick)
{
    // migrated from CHARACTER::PointsPacket
    auto view = reg.view<ecs::TagPC, ecs::NetworkSession, ecs::Position, ecs::Health, ecs::Mana, ecs::VIDComponent, ecs::DirtyTag>();

    for (const entt::entity entity : view) {
        auto& session = view.get<ecs::NetworkSession>(entity);
        const auto& position = view.get<ecs::Position>(entity);
        const auto& health = view.get<ecs::Health>(entity);
        const auto& mana = view.get<ecs::Mana>(entity);
        const auto& vid = view.get<ecs::VIDComponent>(entity);

        if (!session.desc) {
            reg.remove<ecs::DirtyTag>(entity);
            continue;
        }

        TPacketGCMove movePacket {};
        movePacket.bHeader = HEADER_GC_MOVE;
        movePacket.bFunc = FUNC_WAIT;
        movePacket.bArg = 0;
        movePacket.bRot = 0.0f;
        movePacket.dwVID = vid.value;
        movePacket.lX = position.x;
        movePacket.lY = position.y;
        movePacket.dwTime = tick;
        movePacket.dwDuration = 0;
        session.desc->Packet(&movePacket, sizeof(movePacket));

        TPacketGCPoints pointsPacket {};
        pointsPacket.header = HEADER_GC_CHARACTER_POINTS;
        pointsPacket.points[POINT_HP] = health.current;
        pointsPacket.points[POINT_MAX_HP] = health.max;
        pointsPacket.points[POINT_SP] = mana.current;
        pointsPacket.points[POINT_MAX_SP] = mana.max;
        if (const auto* stamina = reg.try_get<ecs::Stamina>(entity)) {
            pointsPacket.points[POINT_STAMINA] = stamina->current;
            pointsPacket.points[POINT_MAX_STAMINA] = stamina->max;
        }
        session.desc->Packet(&pointsPacket, sizeof(pointsPacket));

        reg.remove<ecs::DirtyTag>(entity);
    }
}
