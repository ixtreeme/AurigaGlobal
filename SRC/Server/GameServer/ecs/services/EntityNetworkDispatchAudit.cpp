#include "../../stdafx.h"

#ifdef AURIGA_LPENTITY_FIXUP_AUDIT

#include "EntityNetworkDispatchAudit.hpp"

#include "../../char.h"
#include "../../packet.h"
#include "../components/movement_components.hpp"
#include "../components/spatial_components.hpp"
#include "../components/status_components.hpp"
#include "SpatialService.hpp"

#include <Core/Logging.hpp>

namespace ecs::EntityNetworkDispatchAudit {

void CheckMovementDrift(entt::registry& reg, entt::entity source)
{
    // Only inspect characters; non-characters do not have legacy CHARACTER
    // shadow state.
    const auto* kind = reg.try_get<ecs::SpatialKindTag>(source);
    if (!kind || kind->kind != ecs::SpatialKind::Character)
        return;

    LPENTITY entity = ecs::LPENTITYFromEntity(reg, source);
    if (!entity || !entity->IsType(ENTITY_CHARACTER))
        return;

    LPCHARACTER ch = static_cast<LPCHARACTER>(entity);
    if (!ch)
        return;

    const auto entityIdx = static_cast<uint32_t>(source);

    const int32_t legacyDestX = ch->GetCurrentDestX();
    const int32_t legacyDestY = ch->GetCurrentDestY();
    const uint32_t legacyDuration = ch->GetCurrentMoveDuration();
    const uint32_t legacyStartTime = ch->GetMoveStartTimeForAudit();
    const bool legacyNowWalking = ch->GetNowWalkingForAudit();
    const uint8_t legacyAddChrState = ch->GetAddChrStateForAudit();

    if (const auto* dest = reg.try_get<ecs::MovementDestination>(source))
    {
        if (dest->x != legacyDestX || dest->y != legacyDestY)
        {
            LOG_WARN(
                "[MOVEMENT_DRIFT] dest entity={} ecs=({},{}) legacy=({},{})",
                entityIdx,
                dest->x,
                dest->y,
                legacyDestX,
                legacyDestY);
        }
    }
    else
    {
        // ECS thinks no active movement, but legacy may still have a stale
        // m_posDest != position. Only log when the legacy fields explicitly
        // describe an active move (timing nonzero) - else this would fire on
        // every idle character.
        if (legacyStartTime != 0 || legacyDuration != 0)
        {
            if (legacyDestX != ch->GetX() || legacyDestY != ch->GetY())
            {
                LOG_WARN(
                    "[MOVEMENT_DRIFT] dest entity={} ecs=absent legacy=({},{}) pos=({},{})",
                    entityIdx,
                    legacyDestX,
                    legacyDestY,
                    ch->GetX(),
                    ch->GetY());
            }
        }
    }

    if (const auto* state = reg.try_get<ecs::MovementState>(source))
    {
        if (state->moveStartTime != legacyStartTime ||
            state->moveDuration != legacyDuration)
        {
            LOG_WARN(
                "[MOVEMENT_DRIFT] timing entity={} ecs=(start={} dur={}) legacy=(start={} dur={})",
                entityIdx,
                state->moveStartTime,
                state->moveDuration,
                legacyStartTime,
                legacyDuration);
        }

        if (state->isNowWalking != legacyNowWalking)
        {
            LOG_WARN(
                "[MOVEMENT_DRIFT] walking entity={} ecs={} legacy={}",
                entityIdx,
                state->isNowWalking,
                legacyNowWalking);
        }
    }

    if (const auto* status = reg.try_get<ecs::StatusFlags>(source))
    {
        const bool legacyDead = (legacyAddChrState & ADD_CHARACTER_STATE_DEAD) != 0;
        const bool legacySpawn = (legacyAddChrState & ADD_CHARACTER_STATE_SPAWN) != 0;
        const bool legacyKiller = (legacyAddChrState & ADD_CHARACTER_STATE_KILLER) != 0;
        const bool legacyParty = (legacyAddChrState & ADD_CHARACTER_STATE_PARTY) != 0;

        if (status->isDead != legacyDead ||
            status->isSpawnState != legacySpawn ||
            status->isKillerMode != legacyKiller ||
            status->isPartyState != legacyParty)
        {
            LOG_WARN(
                "[MOVEMENT_DRIFT] state_flags entity={} ecs=(dead={} spawn={} killer={} party={}) legacy=(dead={} spawn={} killer={} party={})",
                entityIdx,
                status->isDead,
                status->isSpawnState,
                status->isKillerMode,
                status->isPartyState,
                legacyDead,
                legacySpawn,
                legacyKiller,
                legacyParty);
        }
    }
}

} // namespace ecs::EntityNetworkDispatchAudit

#endif // AURIGA_LPENTITY_FIXUP_AUDIT
