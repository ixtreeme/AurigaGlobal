#include "../../stdafx.h"

#include "EntityNetworkDispatch.hpp"

#include <algorithm>
#include <cstring>

#include "../../buffer_manager.h"
#include "../../packet.h"
#include "../../utils.h"
#include "../NetworkService.hpp"
#include "../Registry.hpp"
#include "../components/appearance_components.hpp"
#include "../components/character_runtime_components.hpp"
#include "../components/combat_components.hpp"
#include "../components/identity_components.hpp"
#include "../components/item_components.hpp"
#include "../components/movement_components.hpp"
#include "../components/social_components.hpp"
#include "../components/spatial_components.hpp"
#include "../components/status_components.hpp"
#include "../components/transform_components.hpp"
#include "../systems/MountSystem.hpp"
#include "../systems/NetworkSyncSystem.hpp"
#include "../systems/PlayerRuntimeSystem.hpp"
#include "../systems/PointSystem.hpp"
#include "../systems/SocialSystem.hpp"
#include "SpatialService.hpp"
#include "VisibilityService.hpp"

namespace {

void CopyText(char* dest, std::size_t destSize, const std::string& value)
{
    if (!dest || destSize == 0)
        return;

    const std::size_t len = std::min(destSize - 1, value.size());
    if (len > 0)
        std::memcpy(dest, value.data(), len);
    dest[len] = '\0';
}

void EncodeMovePacket(TPacketGCMove& packet,
    uint32_t vid,
    uint8_t func,
    uint8_t arg,
    uint32_t x,
    uint32_t y,
    uint32_t duration,
    uint32_t time,
    float rotation)
{
    packet.bHeader = HEADER_GC_MOVE;
    packet.bFunc = func;
    packet.bArg = arg;
    packet.dwVID = vid;
    packet.dwTime = time ? time : get_dword_time();
    packet.bRot = rotation;
    packet.lX = x;
    packet.lY = y;
    packet.dwDuration = duration;
}

uint8_t CharacterTypeFor(entt::registry& reg, entt::entity e)
{
    if (const auto* type = reg.try_get<ecs::CharacterType>(e))
        return type->value;
    if (reg.all_of<ecs::TagPC>(e))
        return CHAR_TYPE_PC;
    if (reg.all_of<ecs::TagNPC>(e))
        return CHAR_TYPE_NPC;
    if (reg.all_of<ecs::TagStone>(e))
        return CHAR_TYPE_STONE;
    return CHAR_TYPE_MONSTER;
}

float RotationFor(entt::registry& reg, entt::entity e)
{
    if (const auto* rotation = reg.try_get<ecs::RotationComponent>(e))
        return rotation->yaw;
    if (const auto* runtime = reg.try_get<ecs::CharacterRuntimeFlagsComponent>(e))
        return runtime->rotation;
    return 0.0f;
}

TAffectFlag AffectFlagsFor(entt::registry& reg, entt::entity e)
{
    TAffectFlag flags {};
    if (const auto* affect = reg.try_get<ecs::AffectList>(e))
        flags = affect->flags;

#ifdef ENABLE_SOUL_SYSTEM
    if (flags.IsSet(AFF_SOUL_RED) && flags.IsSet(AFF_SOUL_BLUE)) {
        flags.Reset(AFF_SOUL_RED);
        flags.Reset(AFF_SOUL_BLUE);
        flags.Set(AFF_SOUL_MIX);
    }
#endif

    return flags;
}

uint8_t StateFlagsFor(entt::registry& reg, entt::entity e)
{
    uint8_t state = 0;
    if (const auto* status = reg.try_get<ecs::StatusFlags>(e)) {
        if (status->isDead)
            SET_BIT(state, ADD_CHARACTER_STATE_DEAD);
        if (status->isSpawnState)
            SET_BIT(state, ADD_CHARACTER_STATE_SPAWN);
        if (status->isKillerMode)
            SET_BIT(state, ADD_CHARACTER_STATE_KILLER);
        if (status->isPartyState)
            SET_BIT(state, ADD_CHARACTER_STATE_PARTY);
    }
    if (ecs::SocialSystem::GetParty(e))
        SET_BIT(state, ADD_CHARACTER_STATE_PARTY);
    return state;
}

uint16_t LimitedPoint(entt::entity e, uint8_t type)
{
    int64_t value = ecs::PointSystem::Get(e, type);
    int64_t limit = INT64_MAX;
    switch (type) {
    case POINT_ATT_SPEED:
        limit = ecs::PlayerRuntime::IsPC(e) ? 170 : 250;
        break;
    case POINT_MOV_SPEED:
        limit = 350;
        break;
    default:
        break;
    }
    return static_cast<uint16_t>(std::clamp<int64_t>(value, 0, limit));
}

bool BuildCharacterInsert(entt::registry& reg, entt::entity source, TPacketGCCharacterAdd& packet)
{
    const auto* vid = reg.try_get<ecs::VIDComponent>(source);
    const auto* pos = reg.try_get<ecs::Position>(source);
    if (!vid || !pos)
        return false;

    const auto* status = reg.try_get<ecs::StatusFlags>(source);
    const uint32_t race = ecs::PlayerRuntime::GetRaceNum(source);
    const bool specialMountLikeRace = (race >= 20101 && race <= 20109)
        || (status && (status->isPet || status->isNewPet || status->isMount));

    packet = {};
    packet.header = HEADER_GC_CHARACTER_ADD;
    packet.dwVID = vid->value;
    packet.bType = CharacterTypeFor(reg, source);
    packet.angle = RotationFor(reg, source);
    packet.x = pos->x;
    packet.y = pos->y;
    packet.z = pos->z;
    packet.wRaceNum = static_cast<uint16_t>(race);

#ifdef ENABLE_MULTI_NAMES
    packet.transname = !specialMountLikeRace;
#endif

    if (specialMountLikeRace) {
        if (status && status->isMount)
            packet.bMovingSpeed = LimitedPoint(source, POINT_MOV_SPEED);
        else
            packet.bMovingSpeed = ecs::PlayerRuntime::IsPC(source) ? LimitedPoint(source, POINT_MOV_SPEED) : 150;
    } else {
        packet.bMovingSpeed = LimitedPoint(source, POINT_MOV_SPEED);
    }

    packet.bAttackSpeed = LimitedPoint(source, POINT_ATT_SPEED);
    const TAffectFlag affect = AffectFlagsFor(reg, source);
    packet.dwAffectFlag[0] = affect.bits[0];
    packet.dwAffectFlag[1] = affect.bits[1];
    packet.bStateFlag = StateFlagsFor(reg, source);

    if (const auto* dest = reg.try_get<ecs::MovementDestination>(source)) {
        if (const auto* movement = reg.try_get<ecs::MovementState>(source)) {
            const int duration = static_cast<int>((movement->moveStartTime + movement->moveDuration) - get_dword_time());
            if (duration <= 0) {
                packet.x = dest->x;
                packet.y = dest->y;
            }
        }
    }

    return true;
}

void SendCharacterInsert(entt::registry& reg, entt::entity source, entt::entity viewer)
{
    TPacketGCCharacterAdd packet {};
    if (!BuildCharacterInsert(reg, source, packet))
        return;

    ecs::NetworkService::Send(viewer, &packet, sizeof(packet));
    NetworkSyncSystem::SendCharAdditionalInfo(reg, source, viewer);

    const auto* dest = reg.try_get<ecs::MovementDestination>(source);
    const auto* movement = reg.try_get<ecs::MovementState>(source);
    const auto* vid = reg.try_get<ecs::VIDComponent>(source);
    if (dest && movement && vid) {
        const int duration = static_cast<int>((movement->moveStartTime + movement->moveDuration) - get_dword_time());
        if (duration > 0) {
            TPacketGCMove move {};
            EncodeMovePacket(move, vid->value, FUNC_MOVE, 0, dest->x, dest->y, static_cast<uint32_t>(duration), 0, RotationFor(reg, source) / 5.0f);
            ecs::NetworkService::Send(viewer, &move, sizeof(move));

            TPacketGCWalkMode walk {};
            walk.header = HEADER_GC_WALK_MODE;
            walk.vid = vid->value;
            walk.mode = movement->isNowWalking ? WALKMODE_WALK : WALKMODE_RUN;
            ecs::NetworkService::Send(viewer, &walk, sizeof(walk));
        }
    }

    const auto* viewerMovement = reg.try_get<ecs::MovementState>(viewer);
    const auto* viewerVID = reg.try_get<ecs::VIDComponent>(viewer);
    if (viewerMovement && viewerMovement->isNowWalking && viewerVID) {
        TPacketGCWalkMode walk {};
        walk.header = HEADER_GC_WALK_MODE;
        walk.vid = viewerVID->value;
        walk.mode = WALKMODE_WALK;
        ecs::NetworkService::Send(source, &walk, sizeof(walk));
    }

    if (const auto* shop = reg.try_get<ecs::ShopState>(source); shop && shop->myShop && !shop->shopSign.empty()) {
        TPacketGCShopSign sign {};
        sign.bHeader = HEADER_GC_SHOP_SIGN;
        sign.dwVID = packet.dwVID;
#ifdef KASMIR_PAKET_SYSTEM
        // LPENTITY.4-fixup.2.g: read mirror of legacy m_bKasmirPaketBaslik
        sign.bShopKasmirTitle = shop->kasmirTitle;
#endif
        strlcpy(sign.szSign, shop->shopSign.c_str(), sizeof(sign.szSign));
        ecs::NetworkService::Send(viewer, &sign, sizeof(sign));
    }
}

void SendCharacterRemove(entt::registry& reg, entt::entity source, entt::entity viewer)
{
    const auto* vid = reg.try_get<ecs::VIDComponent>(source);
    if (!vid)
        return;

    TPacketGCCharacterDelete packet {};
    packet.header = HEADER_GC_CHARACTER_DEL;
    packet.id = vid->value;
    ecs::NetworkService::Send(viewer, &packet, sizeof(packet));
}

bool EncodeItemGroundInsert(entt::registry& reg, entt::entity item, TPacketGCItemGroundAdd& packet)
{
    const auto* identity = reg.try_get<ecs::ItemIdentity>(item);
    const auto* position = reg.try_get<ecs::ItemGroundPosition>(item);
    if (!identity || !position)
        return false;

    packet = {};
    packet.bHeader = HEADER_GC_ITEM_GROUND_ADD;
    packet.x = position->x;
    packet.y = position->y;
    packet.z = position->z;
    packet.dwVID = identity->vid;
    packet.dwVnum = identity->vnum;
    return true;
}

void SendItemInsert(entt::registry& reg, entt::entity item, entt::entity viewer)
{
    TPacketGCItemGroundAdd packet {};
    if (!EncodeItemGroundInsert(reg, item, packet))
        return;

    ecs::NetworkService::Send(viewer, &packet, sizeof(packet));

    const auto* ownership = reg.try_get<ecs::ItemOwnershipDisplay>(item);
    const auto* identity = reg.try_get<ecs::ItemIdentity>(item);
    if (!ownership || ownership->ownerName.empty() || !identity)
        return;

    TPacketGCItemOwnership owner {};
    owner.bHeader = HEADER_GC_ITEM_OWNERSHIP;
    owner.dwVID = identity->vid;
    strlcpy(owner.szName, ownership->ownerName.c_str(), sizeof(owner.szName));
    ecs::NetworkService::Send(viewer, &owner, sizeof(owner));
}

void SendItemRemove(entt::registry& reg, entt::entity item, entt::entity viewer)
{
    const auto* identity = reg.try_get<ecs::ItemIdentity>(item);
    if (!identity)
        return;

    TPacketGCItemGroundDel packet {};
    packet.bHeader = HEADER_GC_ITEM_GROUND_DEL;
    packet.dwVID = identity->vid;
    ecs::NetworkService::Send(viewer, &packet, sizeof(packet));
}

void SendBuildingInsert(entt::registry& reg, entt::entity building, entt::entity viewer)
{
    const auto* vid = reg.try_get<ecs::VIDComponent>(building);
    const auto* position = reg.try_get<ecs::Position>(building);
    const auto* state = reg.try_get<ecs::BuildingState>(building);
    if (!vid || !position || !state)
        return;

    LOG_INFO("ObjectInsertPacket vid {} vnum {} rot {:f} {:f} {:f}", vid->value, state->vnum, state->rotationX, state->rotationY, state->rotationZ);

    TPacketGCCharacterAdd packet {};
    packet.header = HEADER_GC_CHARACTER_ADD;
    packet.dwVID = vid->value;
    packet.bType = CHAR_TYPE_BUILDING;
    packet.angle = state->rotationZ;
    packet.x = position->x;
    packet.y = position->y;
    packet.z = position->z;
    packet.wRaceNum = static_cast<uint16_t>(state->vnum);
#ifdef ENABLE_MULTI_NAMES
    packet.transname = true;
#endif
    packet.dwAffectFlag[0] = static_cast<uint32_t>(state->rotationX);
    packet.dwAffectFlag[1] = static_cast<uint32_t>(state->rotationY);
    ecs::NetworkService::Send(viewer, &packet, sizeof(packet));
}

void SendBuildingRemove(entt::registry& reg, entt::entity building, entt::entity viewer)
{
    const auto* vid = reg.try_get<ecs::VIDComponent>(building);
    if (!vid)
        return;

    LOG_INFO("ObjectRemovePacket vid {}", vid->value);

    TPacketGCCharacterDelete packet {};
    packet.header = HEADER_GC_CHARACTER_DEL;
    packet.id = vid->value;
    ecs::NetworkService::Send(viewer, &packet, sizeof(packet));
}

void SendShopInsert(entt::registry& reg, entt::entity shop, entt::entity viewer)
{
#ifdef ENABLE_NEW_SHOP_IN_CITIES
    const auto* state = reg.try_get<ecs::OfflineShopState>(shop);
    const auto* position = reg.try_get<ecs::Position>(shop);
    if (!state || !position)
        return;

    TPacketGCNewOfflineshop header {};
    header.bHeader = HEADER_GC_NEW_OFFLINESHOP;
    header.bSubHeader = offlineshop::SUBHEADER_GC_INSERT_SHOP_ENTITY;
    header.wSize = sizeof(header) + sizeof(TSubPacketGCInsertShopEntity);

    TSubPacketGCInsertShopEntity payload {};
    payload.dwVID = state->vid;
    payload.iType = state->shopType;
    payload.x = position->x;
    payload.y = position->y;
    payload.z = position->z;
#ifdef KASMIR_PAKET_SYSTEM
    payload.dwKasmirNpc = state->race;
#endif
    strlcpy(payload.szName, state->name.c_str(), sizeof(payload.szName));

    ecs::NetworkService::SendBufferedPair(viewer, &header, sizeof(header), &payload, sizeof(payload));
#endif
}

void SendShopRemove(entt::registry& reg, entt::entity shop, entt::entity viewer)
{
#ifdef ENABLE_NEW_SHOP_IN_CITIES
    const auto* state = reg.try_get<ecs::OfflineShopState>(shop);
    if (!state)
        return;

    TPacketGCNewOfflineshop header {};
    header.bHeader = HEADER_GC_NEW_OFFLINESHOP;
    header.bSubHeader = offlineshop::SUBHEADER_GC_REMOVE_SHOP_ENTITY;
    header.wSize = sizeof(header) + sizeof(TSubPacketGCRemoveShopEntity);

    TSubPacketGCRemoveShopEntity payload {};
    payload.dwVID = state->vid;

    ecs::NetworkService::SendBufferedPair(viewer, &header, sizeof(header), &payload, sizeof(payload));
#endif
}

} // namespace

namespace ecs::EntityNetworkDispatch {

void SendInsert(entt::registry& reg, entt::entity source, entt::entity viewer)
{
    if (source == entt::null || viewer == entt::null || !reg.valid(source) || !reg.valid(viewer))
        return;

    const auto* kind = reg.try_get<ecs::SpatialKindTag>(source);
    if (!kind)
        return;

    switch (kind->kind) {
    case ecs::SpatialKind::Character:
        // Character spawn/movement insert remains legacy-authoritative for now.
        // The native builder can diverge from legacy movement destination state
        // and leave two clients with different perceived character positions.
        if (LPENTITY sourceLegacy = ecs::LPENTITYFromEntity(reg, source)) {
            if (LPENTITY viewerLegacy = ecs::LPENTITYFromEntity(reg, viewer)) {
                sourceLegacy->EncodeInsertPacket(viewerLegacy);
                break;
            }
        }
        SendCharacterInsert(reg, source, viewer);
        break;
    case ecs::SpatialKind::Item:
        SendItemInsert(reg, source, viewer);
        break;
    case ecs::SpatialKind::Building:
        SendBuildingInsert(reg, source, viewer);
        break;
    case ecs::SpatialKind::OfflineShop:
        SendShopInsert(reg, source, viewer);
        break;
    }
}

void SendRemove(entt::registry& reg, entt::entity source, entt::entity viewer)
{
    if (source == entt::null || viewer == entt::null || !reg.valid(source) || !reg.valid(viewer))
        return;

    const auto* kind = reg.try_get<ecs::SpatialKindTag>(source);
    if (!kind)
        return;

    switch (kind->kind) {
    case ecs::SpatialKind::Character:
        if (LPENTITY sourceLegacy = ecs::LPENTITYFromEntity(reg, source)) {
            if (LPENTITY viewerLegacy = ecs::LPENTITYFromEntity(reg, viewer)) {
                sourceLegacy->EncodeRemovePacket(viewerLegacy);
                break;
            }
        }
        SendCharacterRemove(reg, source, viewer);
        break;
    case ecs::SpatialKind::Item:
        SendItemRemove(reg, source, viewer);
        break;
    case ecs::SpatialKind::Building:
        SendBuildingRemove(reg, source, viewer);
        break;
    case ecs::SpatialKind::OfflineShop:
        SendShopRemove(reg, source, viewer);
        break;
    }
}

void Reencode(entt::registry& reg, entt::entity viewer)
{
    if (viewer == entt::null || !reg.valid(viewer))
        return;

    SendRemove(reg, viewer, viewer);
    SendInsert(reg, viewer, viewer);

    const auto visible = ecs::VisibilityService::GetVisibleEntitiesNative(reg, viewer);
    for (const entt::entity source : visible) {
        SendRemove(reg, viewer, source);
        SendInsert(reg, viewer, source);
        SendInsert(reg, source, viewer);
    }
}

} // namespace ecs::EntityNetworkDispatch
