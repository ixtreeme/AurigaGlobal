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

void CheckMovementDrift(entt::registry& /*reg*/, entt::entity /*source*/)
{
    // Phase C.4: state_flags comparison removed. Legacy m_bAddChrState is
    // no longer written by char paths after C.4; the ECS StatusFlags 4
    // bits are the sole source. CheckMovementDrift body is now empty -
    // function retained as a no-op shim because EntityNetworkDispatch.cpp
    // SendInsert character branch still emits the call site under
    // AURIGA_LPENTITY_FIXUP_AUDIT. The shim and the call site delete in
    // Phase G alongside the legacy field declarations.
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

    if (nativePack.bStateFlag != ch->GetAddChrStateFlag())
        LOG_WARN("[INSERT_PARITY] bStateFlag entity={} native={} legacy={}", entityIdx,
            static_cast<int>(nativePack.bStateFlag),
            static_cast<int>(ch->GetAddChrStateFlag()));

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
