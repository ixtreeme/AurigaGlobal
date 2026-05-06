#include "../../stdafx.h"

#ifdef AURIGA_LPENTITY_FIXUP_AUDIT

#include "EntityNetworkDispatchAudit.hpp"

#include "../../char.h"
#include "../../packet.h"
#include "../../utils.h"
#include "../components/appearance_components.hpp"
#include "../components/character_runtime_components.hpp"
#include "../components/identity_components.hpp"
#include "../components/movement_components.hpp"
#include "../components/social_components.hpp"
#include "../components/spatial_components.hpp"
#include "../components/status_components.hpp"
#include "../components/transform_components.hpp"
#include "../systems/PointSystem.hpp"
#include "../systems/SocialSystem.hpp"
#include "../systems/PlayerRuntimeSystem.hpp"
#include "EntityNetworkDispatch.hpp"
#include "SpatialService.hpp"

#include <Core/Logging.hpp>

#include <cstring>
#include <unordered_map>

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

    // Phase C.2 removed timing + walking comparisons.
    // Phase C.3 removed the m_posDest dest comparison: GetCurrentDestX/Y now
    // reads ECS MovementDestination (with GetX/Y fallback) - same source as
    // the comparison RHS, tautological.
    // Audit retained for m_bAddChrState until C.4 write migration.
    const uint8_t legacyAddChrState = ch->GetAddChrStateForAudit();

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

void CheckCharacterInsertParity(entt::registry& reg, entt::entity source)
{
    // Only inspect characters; non-characters do not have a parallel legacy
    // packet structure for this packet kind.
    const auto* kind = reg.try_get<ecs::SpatialKindTag>(source);
    if (!kind || kind->kind != ecs::SpatialKind::Character)
        return;

    LPENTITY entity = ecs::LPENTITYFromEntity(reg, source);
    if (!entity || !entity->IsType(ENTITY_CHARACTER))
        return;

    LPCHARACTER ch = static_cast<LPCHARACTER>(entity);
    if (!ch)
        return;

    TPacketGCCharacterAdd nativePack {};
    if (!ecs::EntityNetworkDispatch::BuildCharacterInsertForAudit(reg, source, nativePack))
        return;

    const auto entityIdx = static_cast<uint32_t>(source);

    // Compare native packet field-by-field against the values legacy
    // EncodeInsertPacket would have written. This is a sanity check: with
    // movement state in sync (verified by CheckMovementDrift), all packet
    // fields should already match.

    if (nativePack.dwVID != ch->GetPacketVID())
        LOG_WARN("[INSERT_PARITY] dwVID entity={} native={} legacy={}", entityIdx, nativePack.dwVID, ch->GetPacketVID());

    if (nativePack.bType != ch->GetCharType())
        LOG_WARN("[INSERT_PARITY] bType entity={} native={} legacy={}", entityIdx, static_cast<int>(nativePack.bType), static_cast<int>(ch->GetCharType()));

    if (nativePack.angle != ch->GetRotation())
        LOG_WARN("[INSERT_PARITY] angle entity={} native={} legacy={}", entityIdx, nativePack.angle, ch->GetRotation());

    if (nativePack.z != ch->GetZ())
        LOG_WARN("[INSERT_PARITY] z entity={} native={} legacy={}", entityIdx, nativePack.z, ch->GetZ());

    if (nativePack.wRaceNum != ch->GetRaceNum())
        LOG_WARN("[INSERT_PARITY] wRaceNum entity={} native={} legacy={}", entityIdx, nativePack.wRaceNum, ch->GetRaceNum());

    if (nativePack.bAttackSpeed != ch->GetLimitPoint(POINT_ATT_SPEED))
        LOG_WARN("[INSERT_PARITY] bAttackSpeed entity={} native={} legacy={}", entityIdx, nativePack.bAttackSpeed, ch->GetLimitPoint(POINT_ATT_SPEED));

    if (nativePack.bStateFlag != ch->GetAddChrStateForAudit())
        LOG_WARN("[INSERT_PARITY] bStateFlag entity={} native={} legacy={}", entityIdx,
            static_cast<int>(nativePack.bStateFlag),
            static_cast<int>(ch->GetAddChrStateForAudit()));

    // Native may snap pack.x/y to m_posDest if the move duration has
    // expired. Legacy applies the same logic. With movement state in sync
    // they produce identical x/y; mismatches here imply movement drift.
    if (nativePack.x != ch->GetX() && nativePack.x != ch->GetCurrentDestX())
        LOG_WARN("[INSERT_PARITY] x entity={} native={} legacyX={} legacyDestX={}", entityIdx, nativePack.x, ch->GetX(), ch->GetCurrentDestX());

    if (nativePack.y != ch->GetY() && nativePack.y != ch->GetCurrentDestY())
        LOG_WARN("[INSERT_PARITY] y entity={} native={} legacyY={} legacyDestY={}", entityIdx, nativePack.y, ch->GetY(), ch->GetCurrentDestY());
}

} // namespace ecs::EntityNetworkDispatchAudit

#endif // AURIGA_LPENTITY_FIXUP_AUDIT
