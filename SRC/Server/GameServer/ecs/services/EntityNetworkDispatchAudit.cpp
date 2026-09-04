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

    // Compare the native packet field by field against what legacy
    // EncodeInsertPacket would have written.
    //
    // Two things to know before trusting a clean run of this.
    //
    // First, some of these comparisons cannot fail. GetRotation,
    // GetAddChrStateFlag and GetCurrentDestX/Y are already component-backed,
    // so those lines compare a component against itself. They are kept
    // because they will start meaning something again the moment either
    // side changes, but they are not evidence today.
    //
    // Second, a field-by-field check cannot see a missing SIDE packet.
    // Legacy EncodeInsertPacket opens with SendGuildName; the native path
    // did not, and no field here would ever have shown it.
    //
    // Every field of TPacketGCCharacterAdd that either builder sets is
    // compared here. The native builder has taken authority since, so this no
    // longer gates anything - it now watches for the reverse, a component
    // drifting away from the legacy getter it is supposed to back. The legacy
    // side of each comparison is rebuilt from getters here; CHARACTER's
    // EncodeInsertPacket itself is gone.

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

    // These four were never compared. bMovingSpeed and transname both branch
    // on the pet/mount/special-race test, which the two builders spell
    // differently - legacy reads IsPet/IsNewPet/m_bIsMount, native reads
    // StatusFlags - so they are exactly where a divergence would hide.
    {
        TAffectFlag legacyAffect = ch->GetAffectFlags();
#ifdef ENABLE_SOUL_SYSTEM
        if (legacyAffect.IsSet(AFF_SOUL_RED) && legacyAffect.IsSet(AFF_SOUL_BLUE)) {
            legacyAffect.Reset(AFF_SOUL_RED);
            legacyAffect.Reset(AFF_SOUL_BLUE);
            legacyAffect.Set(AFF_SOUL_MIX);
        }
#endif
        if (nativePack.dwAffectFlag[0] != legacyAffect.bits[0]
            || nativePack.dwAffectFlag[1] != legacyAffect.bits[1])
            LOG_WARN("[INSERT_PARITY] dwAffectFlag entity={} native=[{},{}] legacy=[{},{}]",
                entityIdx, nativePack.dwAffectFlag[0], nativePack.dwAffectFlag[1],
                legacyAffect.bits[0], legacyAffect.bits[1]);
    }

    // bMovingSpeed and transname. Both branch on the same pet/mount/special-race
    // test, and until the flags were unified the two builders read that test
    // from different stores - legacy from m_bIsPet/m_bIsNewPet/m_bIsMount on the
    // creature, native from StatusFlags, whose isMount was being set on the
    // RIDER. Comparing them before that was fixed would only have measured the
    // confusion. Now both read the creature flags, so this means something.
    {
        const bool legacySpecial = (ch->GetRaceNum() >= 20101 && ch->GetRaceNum() <= 20109)
            || ch->IsPet()
#ifdef __NEWPET_SYSTEM__
            || ch->IsNewPet()
#endif
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
            || ch->IsMount()
#endif
            ;

#ifdef ENABLE_MULTI_NAMES
        if (nativePack.transname != !legacySpecial)
            LOG_WARN("[INSERT_PARITY] transname entity={} native={} legacy={}", entityIdx,
                static_cast<int>(nativePack.transname), static_cast<int>(!legacySpecial));
#endif

        uint16_t legacySpeed = 0;
        if (legacySpecial)
        {
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
            legacySpeed = ch->IsMount()
                ? static_cast<uint16_t>(ch->GetLimitPoint(POINT_MOV_SPEED))
                : static_cast<uint16_t>(ch->IsPC() ? ch->GetLimitPoint(POINT_MOV_SPEED) : 150);
#else
            legacySpeed = 150;
#endif
        }
        else
        {
            legacySpeed = static_cast<uint16_t>(ch->GetLimitPoint(POINT_MOV_SPEED));
        }

        if (nativePack.bMovingSpeed != legacySpeed)
            LOG_WARN("[INSERT_PARITY] bMovingSpeed entity={} native={} legacy={} special={}",
                entityIdx, nativePack.bMovingSpeed, legacySpeed,
                static_cast<int>(legacySpecial));
    }

    // Native may snap pack.x/y to the active destination if the move duration has
    // expired. Legacy applies the same logic. With movement state in sync
    // they produce identical x/y; mismatches here imply movement drift.
    if (nativePack.x != ch->GetX() && nativePack.x != ch->GetCurrentDestX())
        LOG_WARN("[INSERT_PARITY] x entity={} native={} legacyX={} legacyDestX={}", entityIdx, nativePack.x, ch->GetX(), ch->GetCurrentDestX());

    if (nativePack.y != ch->GetY() && nativePack.y != ch->GetCurrentDestY())
        LOG_WARN("[INSERT_PARITY] y entity={} native={} legacyY={} legacyDestY={}", entityIdx, nativePack.y, ch->GetY(), ch->GetCurrentDestY());
}

} // namespace ecs::EntityNetworkDispatchAudit

#endif // AURIGA_LPENTITY_FIXUP_AUDIT
