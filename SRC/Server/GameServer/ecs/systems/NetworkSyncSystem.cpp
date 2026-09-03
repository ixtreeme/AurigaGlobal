#include "../../stdafx.h"
#include "PlayerRuntimeSystem.hpp"

#include "NetworkSyncSystem.hpp"

#include <algorithm>
#include <cstring>
#include <map>
#include <string>
#include <string_view>

#include "../../char.h"
#include "../../char_manager.h"
#include "../../buffer_manager.h"
#include "../../desc.h"
#include "../../guild.h"
#include "../../item.h"
#include "../../mob_manager.h"
#include "../../packet.h"
#include "../../party.h"
#include "../../sectree.h"
#include "../EntityFactory.hpp"
#include "../ItemRegistry.hpp"
#include "../NetworkService.hpp"
#include "../Registry.hpp"
#include "../components/appearance_components.hpp"
#include "../components/combat_components.hpp"
#include "../components/dirty_components.hpp"
#include "../components/identity_components.hpp"
#include "../components/item_components.hpp"
#include "../components/item_proto_components.hpp"
#include "../components/movement_components.hpp"
#include "../components/pet_mount_components.hpp"
#include "../components/session_components.hpp"
#include "../components/skill_components.hpp"
#include "../components/social_components.hpp"
#include "../components/status_components.hpp"
#include "../components/transform_components.hpp"
#include "../components/vital_components.hpp"
#include "ItemSystem.hpp"
#include "MountSystem.hpp"
#include "PointSystem.hpp"
#include "SocialSystem.hpp"
#include "../services/VisibilityService.hpp"
#include "../services/SpatialService.hpp"
#include "../services/EntityNetworkDispatch.hpp"
#include <Core/Logging.hpp>
#include "../CharacterAccessors.hpp"

extern bool battle_is_attackable(entt::entity character, entt::entity victim);

namespace
{
struct BGMInfo
{
    std::string name;
    float vol;
};

using BGMInfoMap = std::map<unsigned, BGMInfo>;

BGMInfoMap gs_bgmInfoMap;
bool gs_bgmVolEnable = false;
}

void CHARACTER_SetBGMVolumeEnable()
{
    gs_bgmVolEnable = true;
    LOG_INFO("bgm_info.set_bgm_volume_enable");
}

void CHARACTER_AddBGMInfo(unsigned mapIndex, const char* name, float vol)
{
    BGMInfo newInfo;
    newInfo.name = name;
    newInfo.vol = vol;

    gs_bgmInfoMap[mapIndex] = newInfo;

    LOG_INFO("bgm_info.add_info({}, '{}', {:f})", mapIndex, name, vol);
}

const BGMInfo& CHARACTER_GetBGMInfo(unsigned mapIndex)
{
    const auto f = gs_bgmInfoMap.find(mapIndex);
    if (gs_bgmInfoMap.end() == f) {
        static BGMInfo s_empty = { "", 0.0f };
        return s_empty;
    }

    return f->second;
}

bool CHARACTER_IsBGMVolumeEnable()
{
    return gs_bgmVolEnable;
}

namespace {
void CopyStringView(char* dest, std::size_t destSize, std::string_view value);
uint16_t GetAppearancePart(entt::registry& reg, entt::entity e, uint8_t part);
uint16_t GetLimitedPointForPacket(entt::entity e, uint8_t type);
TAffectFlag GetPacketAffectFlags(entt::registry& reg, entt::entity e);
uint8_t GetPacketStateFlags(entt::registry& reg, entt::entity e);
}

namespace NetworkSyncSystem {

void UpdatePacket(entt::entity e)
{
    TPacketGCCharacterUpdate packet {};
    if (!BuildCharacterUpdatePacket(g_registry, e, packet))
        return;

    const auto* vid = g_registry.try_get<ecs::VIDComponent>(e);
    auto* ch = vid ? CHARACTER_MANAGER::instance().Find(vid->value) : nullptr;
    if (!ch || !ecs::PlayerRuntime::GetSectree(e))
        return;

    ecs::NetworkService::BroadcastToView(g_registry, e, &packet, sizeof(packet), false);
    BroadcastCharAdditionalInfo(g_registry, e);
}

void MainCharacterPacket(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    const auto* mapIndex = g_registry.try_get<ecs::MapIndex>(e);
    const auto* position = g_registry.try_get<ecs::Position>(e);
    const auto* vid = g_registry.try_get<ecs::VIDComponent>(e);
    if (!mapIndex || !position || !vid)
        return;

    const unsigned map = static_cast<unsigned>(mapIndex->value);
    const BGMInfo& bgmInfo = CHARACTER_GetBGMInfo(map);
    const auto name = ecs::PlayerRuntime::GetName(e);
    const uint16_t race = static_cast<uint16_t>(ecs::PlayerRuntime::GetRaceNum(e));
    const uint8_t empire = ecs::PlayerRuntime::GetEmpire(e);
    uint8_t skillGroup = 0;
    if (const auto* skills = g_registry.try_get<ecs::SkillLevels>(e))
        skillGroup = skills->group;

    if (!bgmInfo.name.empty()) {
        if (CHARACTER_IsBGMVolumeEnable()) {
            LOG_INFO("bgm_info.play_bgm_vol({}, name='{}', vol={:f})", map, bgmInfo.name.c_str(), bgmInfo.vol);
            TPacketGCMainCharacter4_BGM_VOL packet {};
            packet.header = HEADER_GC_MAIN_CHARACTER4_BGM_VOL;
            packet.dwVID = vid->value;
            packet.wRaceNum = race;
            packet.lx = position->x;
            packet.ly = position->y;
            packet.lz = position->z;
            packet.empire = empire;
            packet.skill_group = skillGroup;
            packet.fBGMVol = bgmInfo.vol;
            CopyStringView(packet.szChrName, sizeof(packet.szChrName), name);
            strlcpy(packet.szBGMName, bgmInfo.name.c_str(), sizeof(packet.szBGMName));
            ecs::NetworkService::Send(e, &packet, sizeof(packet));
        } else {
            LOG_INFO("bgm_info.play({}, '{}')", map, bgmInfo.name.c_str());
            TPacketGCMainCharacter3_BGM packet {};
            packet.header = HEADER_GC_MAIN_CHARACTER3_BGM;
            packet.dwVID = vid->value;
            packet.wRaceNum = race;
            packet.lx = position->x;
            packet.ly = position->y;
            packet.lz = position->z;
            packet.empire = empire;
            packet.skill_group = skillGroup;
            CopyStringView(packet.szChrName, sizeof(packet.szChrName), name);
            strlcpy(packet.szBGMName, bgmInfo.name.c_str(), sizeof(packet.szBGMName));
            ecs::NetworkService::Send(e, &packet, sizeof(packet));
        }
        return;
    }

    LOG_INFO("bgm_info.play({}, DEFAULT_BGM_NAME)", map);
    TPacketGCMainCharacter packet {};
    packet.header = HEADER_GC_MAIN_CHARACTER;
    packet.dwVID = vid->value;
    packet.wRaceNum = race;
    packet.lx = position->x;
    packet.ly = position->y;
    packet.lz = position->z;
    packet.empire = empire;
    packet.skill_group = skillGroup;
    CopyStringView(packet.szName, sizeof(packet.szName), name);
    ecs::NetworkService::Send(e, &packet, sizeof(packet));
}

void PointsPacket(entt::entity e)
{
    TPacketGCPoints packet {};
    if (!BuildPointsPacket(g_registry, e, packet))
        return;

    ecs::NetworkService::Send(e, &packet, sizeof(packet));
}

} // namespace NetworkSyncSystem

namespace {
void EncodeMovePacket(TPacketGCMove& pack, uint32_t dwVID, uint8_t bFunc, uint8_t bArg, uint32_t x, uint32_t y, uint32_t dwDuration, uint32_t dwTime, float bRot)
{
    pack.bHeader = HEADER_GC_MOVE;
    pack.bFunc = bFunc;
    pack.bArg = bArg;
    pack.dwVID = dwVID;
    pack.dwTime = dwTime ? dwTime : get_dword_time();
    pack.bRot = bRot;
    pack.lX = x;
    pack.lY = y;
    pack.dwDuration = dwDuration;
}

struct FuncClearSync
{
    void operator()(LPCHARACTER ch)
    {
        assert(ch != NULL);
        ch->SetSyncOwner(entt::null, false);
    }
};

int16_t AlignmentForPacket(entt::entity e, uint32_t legacyAlignment)
{
    if (e != entt::null && g_registry.valid(e)) {
        if (const auto* combat = g_registry.try_get<ecs::CombatStats>(e))
            return static_cast<int16_t>(combat->alignment / 10);
    }

    return static_cast<int16_t>(legacyAlignment / 10);
}

void CopyStringView(char* dest, std::size_t destSize, std::string_view value)
{
    if (!dest || destSize == 0)
        return;

    const std::size_t len = std::min(destSize - 1, value.size());
    if (len > 0)
        std::memcpy(dest, value.data(), len);
    dest[len] = '\0';
}

uint16_t GetAppearancePart(entt::registry& reg, entt::entity e, uint8_t part)
{
    const auto* appearance = reg.try_get<ecs::AppearancePartsComponent>(e);
    if (!appearance || part >= PART_MAX_NUM)
        return 0;

    return appearance->parts[part];
}

uint16_t GetLimitedPointForPacket(entt::entity e, uint8_t type)
{
    int64_t value = ecs::PointSystem::Get(e, type);
    int64_t minLimit = 0;
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

    value = std::clamp(value, minLimit, limit);
    return static_cast<uint16_t>(value);
}

TAffectFlag GetPacketAffectFlags(entt::registry& reg, entt::entity e)
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

uint8_t GetPacketStateFlags(entt::registry& reg, entt::entity e)
{
    uint8_t state = 0;
    const auto* status = reg.try_get<ecs::StatusFlags>(e);
    if (status) {
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
}

bool NetworkSyncSystem::BuildCharAdditionalInfo(entt::registry& reg, entt::entity source, TPacketGCCharacterAdditionalInfo& packet)
{
    if (source == entt::null || !reg.valid(source))
        return false;

    const bool isPC = ecs::PlayerRuntime::IsPC(source);
    const bool isNPC = ecs::PlayerRuntime::IsNPC(source);
    if (!isPC && !isNPC)
        return false;

    const auto* vid = reg.try_get<ecs::VIDComponent>(source);
    if (!vid)
        return false;

    packet = {};
    packet.header = HEADER_GC_CHAR_ADDITIONAL_INFO;
    packet.dwVID = vid->value;
    packet.bEmpire = ecs::PlayerRuntime::GetEmpire(source);
    packet.dwGuildID = 0;
    packet.dwLevel = 0;
    packet.sAlignment = 0;
    packet.dwMountVnum = 0;

    if (const auto* combat = reg.try_get<ecs::CombatStats>(source)) {
        packet.bPKMode = combat->pkMode;
        if (isPC)
            packet.sAlignment = combat->alignment / 10;
    }

    CopyStringView(packet.name, sizeof(packet.name), ecs::PlayerRuntime::GetName(source));

    packet.awPart[CHR_EQUIPPART_ARMOR] = GetAppearancePart(reg, source, PART_MAIN);
    packet.awPart[CHR_EQUIPPART_WEAPON] = GetAppearancePart(reg, source, PART_WEAPON);
    packet.awPart[CHR_EQUIPPART_HEAD] = GetAppearancePart(reg, source, PART_HEAD);
    packet.awPart[CHR_EQUIPPART_HAIR] = GetAppearancePart(reg, source, PART_HAIR);
#ifdef ENABLE_RUNE_SYSTEM
    packet.awPart[CHR_EQUIPPART_RUNE] = GetAppearancePart(reg, source, PART_RUNE);
#endif
#ifdef ENABLE_ACCE_SYSTEM
    packet.awPart[CHR_EQUIPPART_ACCE] = GetAppearancePart(reg, source, PART_ACCE);
#endif
#ifdef ENABLE_COSTUME_EFFECT
    packet.awPart[CHR_EQUIPPART_EFFECT_BODY] = GetAppearancePart(reg, source, PART_EFFECT_BODY);
    packet.awPart[CHR_EQUIPPART_EFFECT_WEAPON] = GetAppearancePart(reg, source, PART_EFFECT_WEAPON);
#endif

    if (isPC) {
        const int level = ecs::PointSystem::GetLevel(source);
        packet.dwLevel = level > 0 ? static_cast<uint32_t>(level) : 0;
        packet.dwMountVnum = MountSystem::GetMountVnum(source);
        if (CGuild* guild = ecs::SocialSystem::GetGuild(source))
            packet.dwGuildID = guild->GetID();
#ifdef ENABLE_MULTI_LANGUAGE
        packet.bLanguage = ecs::NetworkService::GetLanguage(source);
#endif
    }
#ifdef __NEWPET_SYSTEM__
    else if (const auto* pet = reg.try_get<ecs::PetComponent>(source)) {
        packet.dwLevel = pet->level;
    }
#endif

#ifdef __SKILL_COLOR_SYSTEM__
    if (const auto* skillColor = reg.try_get<ecs::SkillColor>(source))
        std::memcpy(packet.dwSkillColor, skillColor->data, sizeof(packet.dwSkillColor));
#endif

    return true;
}

void NetworkSyncSystem::SendCharAdditionalInfo(entt::registry& reg, entt::entity source, entt::entity recipient)
{
    TPacketGCCharacterAdditionalInfo packet {};
    if (!BuildCharAdditionalInfo(reg, source, packet))
        return;

    ecs::NetworkService::Send(recipient, &packet, sizeof(packet));
}

void NetworkSyncSystem::BroadcastCharAdditionalInfo(entt::registry& reg, entt::entity source)
{
    TPacketGCCharacterAdditionalInfo packet {};
    if (!BuildCharAdditionalInfo(reg, source, packet))
        return;

    // AdditionalInfo is append-side payload on the client. Sending it
    // standalone to the owner/main actor can clear dynamic actors.
    ecs::NetworkService::BroadcastToView(reg, source, &packet, sizeof(packet), true);
}

bool NetworkSyncSystem::BuildCharacterUpdatePacket(entt::registry& reg, entt::entity source, TPacketGCCharacterUpdate& packet)
{
    if (source == entt::null || !reg.valid(source))
        return false;

    const auto* vid = reg.try_get<ecs::VIDComponent>(source);
    if (!vid)
        return false;

    packet = {};
    packet.header = HEADER_GC_CHARACTER_UPDATE;
    packet.dwVID = vid->value;

    packet.awPart[CHR_EQUIPPART_ARMOR] = GetAppearancePart(reg, source, PART_MAIN);
    packet.awPart[CHR_EQUIPPART_WEAPON] = GetAppearancePart(reg, source, PART_WEAPON);
    packet.awPart[CHR_EQUIPPART_HEAD] = GetAppearancePart(reg, source, PART_HEAD);
    packet.awPart[CHR_EQUIPPART_HAIR] = GetAppearancePart(reg, source, PART_HAIR);
#ifdef ENABLE_RUNE_SYSTEM
    packet.awPart[CHR_EQUIPPART_RUNE] = GetAppearancePart(reg, source, PART_RUNE);
#endif
#ifdef ENABLE_ACCE_SYSTEM
    packet.awPart[CHR_EQUIPPART_ACCE] = GetAppearancePart(reg, source, PART_ACCE);
#endif
#ifdef ENABLE_COSTUME_EFFECT
    packet.awPart[CHR_EQUIPPART_EFFECT_BODY] = GetAppearancePart(reg, source, PART_EFFECT_BODY);
    packet.awPart[CHR_EQUIPPART_EFFECT_WEAPON] = GetAppearancePart(reg, source, PART_EFFECT_WEAPON);
#endif

    packet.bMovingSpeed = GetLimitedPointForPacket(source, POINT_MOV_SPEED);
    packet.bAttackSpeed = GetLimitedPointForPacket(source, POINT_ATT_SPEED);
    packet.bStateFlag = GetPacketStateFlags(reg, source);

    const TAffectFlag affectFlags = GetPacketAffectFlags(reg, source);
    packet.dwAffectFlag[0] = affectFlags.bits[0];
    packet.dwAffectFlag[1] = affectFlags.bits[1];

    if (const auto* combat = reg.try_get<ecs::CombatStats>(source)) {
        packet.sAlignment = combat->alignment / 10;
        packet.bPKMode = combat->pkMode;
    }

    if (CGuild* guild = ecs::SocialSystem::GetGuild(source))
        packet.dwGuildID = guild->GetID();

    packet.dwMountVnum = MountSystem::GetMountVnum(source);

#ifdef __SKILL_COLOR_SYSTEM__
    if (const auto* skillColor = reg.try_get<ecs::SkillColor>(source))
        std::memcpy(packet.dwSkillColor, skillColor->data, sizeof(packet.dwSkillColor));
#endif

#ifdef ENABLE_MULTI_LANGUAGE
    packet.bLanguage = ecs::NetworkService::GetLanguage(source);
#endif

    return true;
}

bool NetworkSyncSystem::BuildPointsPacket(entt::registry& reg, entt::entity source, TPacketGCPoints& packet)
{
    if (source == entt::null || !reg.valid(source))
        return false;

    packet = {};
    packet.header = HEADER_GC_CHARACTER_POINTS;
    for (uint8_t i = 0; i < POINT_MAX_NUM; ++i)
        packet.points[i] = ecs::PointSystem::Get(source, i);

    return true;
}

#ifdef ENABLE_ITEM_ON_TITLE_RAZOR93
std::string NetworkSyncSystem::GetItemOnTitlePrefix(entt::registry& reg, entt::entity source)
{
    if (!ecs::PlayerRuntime::IsPC(source))
        return {};

    const auto* playerID = reg.try_get<ecs::PlayerID>(source);
    if (!playerID)
        return {};

    auto items = reg.view<ecs::ItemOwner, ecs::ItemEquipped, ecs::ItemPrototypeMeta, ecs::ItemProtoRef>();
    for (const auto item : items) {
        const auto& owner = items.get<ecs::ItemOwner>(item);
        const auto& equipped = items.get<ecs::ItemEquipped>(item);
        if (owner.ownerPID != playerID->pid || !equipped.equipped || equipped.slot != WEAR_BELT)
            continue;

        const auto& meta = items.get<ecs::ItemPrototypeMeta>(item);
        if (meta.type != ITEM_BELT)
            return {};

        if (ItemSystem::GetItemValue(item, 5) != 1)
            return {};

        const auto& protoRef = items.get<ecs::ItemProtoRef>(item);
        const char* protoName = protoRef.proto ? protoRef.proto->szName : nullptr;
        if (!protoName || protoName[0] != '[')
            return {};

        const char* end = strchr(protoName, ']');
        if (!end)
            return {};

        const int prefixLen = static_cast<int>(end - protoName) + 1;
        if (prefixLen <= 0 || prefixLen > 24)
            return {};

        return std::string(protoName, prefixLen);
    }

    return {};
}

std::string NetworkSyncSystem::GetDisplayedNameWithItemOnTitle(entt::registry& reg, entt::entity source)
{
    const std::string prefix = GetItemOnTitlePrefix(reg, source);
    const auto name = ecs::PlayerRuntime::GetName(source);
    if (prefix.empty())
        return std::string(name);

    return prefix + std::string(name);
}

void NetworkSyncSystem::SendItemOnTitleNameToDesc(entt::registry& reg, entt::entity source, entt::entity recipient)
{
    TPacketGCItemOnTitleNameUpdate packet {};
    packet.header = HEADER_GC_ITEM_ON_TITLE_NAME_UPDATE;
    packet.dwVID = ecs::PlayerRuntime::GetPacketVID(source);

    const std::string prefix = GetItemOnTitlePrefix(reg, source);
    strlcpy(packet.name, prefix.c_str(), sizeof(packet.name));

    ecs::NetworkService::Send(recipient, &packet, sizeof(packet));
}

void NetworkSyncSystem::UpdateItemOnTitleName(entt::registry& reg, entt::entity source, bool force)
{
    if (source == entt::null || !reg.valid(source))
        return;

    const std::string prefix = GetItemOnTitlePrefix(reg, source);
    auto& cache = reg.get_or_emplace<ecs::ItemTitlePrefixCache>(source);
    if (!force && cache.prefix == prefix)
        return;

    cache.prefix = prefix;

    TPacketGCItemOnTitleNameUpdate packet {};
    packet.header = HEADER_GC_ITEM_ON_TITLE_NAME_UPDATE;
    packet.dwVID = ecs::PlayerRuntime::GetPacketVID(source);
    strlcpy(packet.name, prefix.c_str(), sizeof(packet.name));

    ecs::NetworkService::BroadcastToView(reg, source, &packet, sizeof(packet), false);
}
#endif

bool NetworkSyncSystem::BuildViewEquipmentPacket(entt::registry& reg, entt::entity wearer, TPacketViewEquip& packet)
{
    const auto* playerID = reg.try_get<ecs::PlayerID>(wearer);
    const uint32_t vid = ecs::PlayerRuntime::GetPacketVID(wearer);
    if (!playerID || vid == 0)
        return false;

    packet = {};
    packet.header = HEADER_GC_VIEW_EQUIP;
    packet.vid = vid;

#ifdef EQUIP_ENABLE_VIEW_SASH
#ifdef ENABLE_COSTUME_PET
    const int positions[23] = { WEAR_BODY, WEAR_HEAD, WEAR_FOOTS, WEAR_WRIST, WEAR_WEAPON, WEAR_NECK, WEAR_EAR, WEAR_UNIQUE1,
                WEAR_UNIQUE2, WEAR_ARROW, WEAR_SHIELD, WEAR_COSTUME_BODY, WEAR_COSTUME_HAIR, WEAR_RING1, WEAR_RING2, WEAR_BELT, WEAR_COSTUME_ACCE_SLOT, WEAR_COSTUME_ACCE, WEAR_COSTUME_WEAPON, WEAR_COSTUME_PET_SKIN, WEAR_COSTUME_MOUNT_SKIN, WEAR_COSTUME_EFFECT_BODY, WEAR_COSTUME_EFFECT_WEAPON };
    constexpr int positionCount = 23;
#else
    const int positions[22] = { WEAR_BODY, WEAR_HEAD, WEAR_FOOTS, WEAR_WRIST, WEAR_WEAPON, WEAR_NECK, WEAR_EAR, WEAR_UNIQUE1,
                    WEAR_UNIQUE2, WEAR_ARROW, WEAR_SHIELD, WEAR_COSTUME_BODY, WEAR_COSTUME_HAIR, WEAR_RING1, WEAR_RING2, WEAR_BELT, WEAR_COSTUME_ACCE_SLOT, WEAR_COSTUME_ACCE, WEAR_COSTUME_WEAPON, WEAR_COSTUME_MOUNT_SKIN, WEAR_COSTUME_EFFECT_BODY, WEAR_COSTUME_EFFECT_WEAPON };
    constexpr int positionCount = 22;
#endif
#else
    const int positions[16] = { WEAR_BODY, WEAR_HEAD, WEAR_FOOTS, WEAR_WRIST, WEAR_WEAPON, WEAR_NECK, WEAR_EAR, WEAR_UNIQUE1,
                    WEAR_UNIQUE2, WEAR_ARROW, WEAR_SHIELD, WEAR_COSTUME_BODY, WEAR_COSTUME_HAIR, WEAR_RING1, WEAR_RING2, WEAR_BELT };
    constexpr int positionCount = 16;
#endif

    auto items = reg.view<ecs::ItemOwner, ecs::ItemEquipped, ecs::ItemIdentity, ecs::ItemCount, ecs::ItemSockets, ecs::ItemAttributes>();
    for (int i = 0; i < positionCount; ++i) {
        for (const auto item : items) {
            const auto& owner = items.get<ecs::ItemOwner>(item);
            const auto& equipped = items.get<ecs::ItemEquipped>(item);
            if (owner.ownerPID != playerID->pid || !equipped.equipped || equipped.slot != positions[i])
                continue;

            const auto& identity = items.get<ecs::ItemIdentity>(item);
            const auto& count = items.get<ecs::ItemCount>(item);
            const auto& sockets = items.get<ecs::ItemSockets>(item);
            const auto& attrs = items.get<ecs::ItemAttributes>(item);

            packet.equips[i].vnum = identity.vnum;
            packet.equips[i].count = static_cast<uint8_t>(count.count);
            std::copy(sockets.sockets.begin(), sockets.sockets.end(), packet.equips[i].alSockets);
            std::copy(attrs.attrs.begin(), attrs.attrs.end(), packet.equips[i].aAttr);
            break;
        }
    }

    return true;
}

void NetworkSyncSystem::SendEquipmentToViewer(entt::registry& reg, entt::entity wearer, entt::entity viewer)
{
    TPacketViewEquip packet {};
    if (!BuildViewEquipmentPacket(reg, wearer, packet))
        return;

    ecs::NetworkService::Send(viewer, &packet, sizeof(packet));
}

void NetworkSyncSystem::BroadcastEquipmentChange(entt::registry& reg, entt::entity wearer)
{
    TPacketViewEquip packet {};
    if (!BuildViewEquipmentPacket(reg, wearer, packet))
        return;

    ecs::NetworkService::BroadcastToView(reg, wearer, &packet, sizeof(packet), true);
}

entt::entity NetworkSyncSystem::FindCharacterInView(entt::registry& reg, entt::entity source, const char* name, bool findPCOnly)
{
    if (!name || source == entt::null || !reg.valid(source))
        return entt::null;

    const auto visible = ecs::VisibilityService::GetVisibleEntities(reg, source);
    for (const entt::entity candidate : visible) {
        if (candidate == entt::null || !reg.valid(candidate))
            continue;

        if (findPCOnly && !ecs::PlayerRuntime::IsPC(candidate))
            continue;

        if (!ecs::PlayerRuntime::IsPC(candidate)
            && !ecs::PlayerRuntime::IsNPC(candidate)
            && !ecs::PlayerRuntime::IsMonster(candidate)
            && !ecs::PlayerRuntime::IsStone(candidate))
            continue;

        const auto candidateName = ecs::PlayerRuntime::GetName(candidate);
        if (!candidateName.empty() && !strcasecmp(candidateName.data(), name))
            return candidate;
    }

    return entt::null;
}

bool NetworkSyncSystem::BuildPartyUpdatePacket(entt::registry& reg, entt::entity member, TPacketGCPartyUpdate& packet)
{
    if (member == entt::null || !reg.valid(member))
        return false;

    LPPARTY party = ecs::SocialSystem::GetParty(member);
    if (!party)
        return false;

    const uint32_t playerID = ecs::PlayerRuntime::GetPlayerID(member);
    packet = {};
    packet.header = HEADER_GC_PARTY_UPDATE;
    packet.pid = playerID;

    const int64_t maxHP = ecs::PointSystem::Get(member, POINT_MAX_HP);
    if (maxHP <= 0)
        packet.percent_hp = 0;
    else
        packet.percent_hp = MINMAX((int64_t)0, ecs::PointSystem::Get(member, POINT_HP) * 100 / maxHP, (int64_t)100);

    packet.role = party->GetRole(playerID);
    LOG_INFO("PARTY {} role is {}", ecs::PlayerRuntime::GetName(member), packet.role);

    LPCHARACTER leader = party->GetLeaderCharacter();
    const entt::entity leaderEntity = leader
        ? leader->GetEntityHandle()
        : entt::null;
    if (leaderEntity != entt::null && reg.valid(leaderEntity)) {
        const auto* memberPos = reg.try_get<ecs::Position>(member);
        const auto* leaderPos = reg.try_get<ecs::Position>(leaderEntity);
        if (memberPos && leaderPos && DISTANCE_APPROX(memberPos->x - leaderPos->x, memberPos->y - leaderPos->y) < PARTY_DEFAULT_RANGE) {
            packet.affects[0] = party->GetPartyBonusExpPercent();
            packet.affects[1] = ecs::PointSystem::Get(member, POINT_PARTY_ATTACKER_BONUS);
            packet.affects[2] = ecs::PointSystem::Get(member, POINT_PARTY_TANKER_BONUS);
            packet.affects[3] = ecs::PointSystem::Get(member, POINT_PARTY_BUFFER_BONUS);
            packet.affects[4] = ecs::PointSystem::Get(member, POINT_PARTY_SKILL_MASTER_BONUS);
            packet.affects[5] = ecs::PointSystem::Get(member, POINT_PARTY_HASTE_BONUS);
            packet.affects[6] = ecs::PointSystem::Get(member, POINT_PARTY_DEFENDER_BONUS);
        }
    }

    return true;
}

void NetworkSyncSystem::BroadcastSyncPacket(entt::registry& reg, entt::entity source)
{
    if (source == entt::null || !reg.valid(source))
        return;

    const auto* vid = reg.try_get<ecs::VIDComponent>(source);
    const auto* position = reg.try_get<ecs::Position>(source);
    if (!vid || !position)
        return;

    TEMP_BUFFER buf;
    TPacketCGSyncPositionElement elem {};
    elem.dwVID = vid->value;
    elem.lX = position->x;
    elem.lY = position->y;

    TPacketGCSyncPosition packet {};
    packet.bHeader = HEADER_GC_SYNC_POSITION;
    packet.wSize = sizeof(TPacketGCSyncPosition) + sizeof(elem);

    buf.write(&packet, sizeof(packet));
    buf.write(&elem, sizeof(elem));
    ecs::NetworkService::BroadcastToView(reg, source, buf.read_peek(), buf.size(), false);
}

void NetworkSyncSystem::BroadcastEffect(entt::registry& reg, entt::entity source, uint8_t effectType)
{
    if (source == entt::null || !reg.valid(source))
        return;

    const auto* vid = reg.try_get<ecs::VIDComponent>(source);
    if (!vid)
        return;

    TPacketGCSpecialEffect packet {};
    packet.header = HEADER_GC_SEPCIAL_EFFECT;
    packet.type = effectType;
    packet.vid = vid->value;
    ecs::NetworkService::BroadcastToView(reg, source, &packet, sizeof(packet), false);
}

void NetworkSyncSystem::BroadcastSpecificEffect(entt::registry& reg, entt::entity source, const char* effectName)
{
    if (source == entt::null || !reg.valid(source))
        return;

    const auto* vid = reg.try_get<ecs::VIDComponent>(source);
    if (!vid || !effectName)
        return;

    TPacketGCSpecificEffect packet {};
    packet.header = HEADER_GC_SPECIFIC_EFFECT;
    packet.vid = vid->value;
    strlcpy(packet.effect_file, effectName, MAX_EFFECT_FILE_NAME);
    ecs::NetworkService::BroadcastToView(reg, source, &packet, sizeof(packet), false);
}

void NetworkSyncSystem::SendConfirmWithMsg(entt::registry& reg, entt::entity recipient, const char* message, int timeout, uint32_t requestPID)
{
    if (recipient == entt::null || !reg.valid(recipient))
        return;

    if (!ecs::PlayerRuntime::IsPC(recipient) || !message)
        return;

    TPacketGCQuestConfirm packet {};
    packet.header = HEADER_GC_QUEST_CONFIRM;
    packet.requestPID = requestPID;
    packet.timeout = timeout;
    strlcpy(packet.msg, message, sizeof(packet.msg));
    ecs::NetworkService::Send(recipient, &packet, sizeof(packet));
}

namespace ecs::ItemNetworkSystem {

entt::entity ResolveItemEntity(CItem* item)
{
    if (!item)
        return entt::null;

    entt::entity itemEntity = CItemRegistry::Instance().Find(item->GetID());
    if (itemEntity == entt::null)
        itemEntity = CItemRegistry::Instance().FindByVID(item->GetVID());
    return itemEntity;
}

entt::entity FindPlayerByPID(entt::registry& reg, uint32_t pid)
{
    if (pid == 0)
        return entt::null;

    auto players = reg.view<ecs::PlayerID>();
    for (const auto player : players) {
        const auto& playerID = players.get<ecs::PlayerID>(player);
        if (playerID.pid == pid)
            return player;
    }

    return entt::null;
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

bool EncodeItemGroundRemove(entt::registry& reg, entt::entity item, TPacketGCItemGroundDel& packet)
{
    const auto* identity = reg.try_get<ecs::ItemIdentity>(item);
    if (!identity)
        return false;

    packet = {};
    packet.bHeader = HEADER_GC_ITEM_GROUND_DEL;
    packet.dwVID = identity->vid;
    return true;
}

bool EncodeItemUpdate(entt::registry& reg, entt::entity item, TPacketGCItemUpdate& packet)
{
    const auto* location = reg.try_get<ecs::ItemLocation>(item);
    const auto* count = reg.try_get<ecs::ItemCount>(item);
    const auto* sockets = reg.try_get<ecs::ItemSockets>(item);
    const auto* attrs = reg.try_get<ecs::ItemAttributes>(item);
    if (!location || !count || !sockets || !attrs)
        return false;

    packet = {};
    packet.header = HEADER_GC_ITEM_UPDATE;
    packet.Cell = TItemPos(location->window, location->cell);
    packet.count = count->count;
    std::copy(sockets->sockets.begin(), sockets->sockets.end(), packet.alSockets);
    std::copy(attrs->attrs.begin(), attrs->attrs.end(), packet.aAttr);
#ifdef ATTR_LOCK
    const auto* locked = reg.try_get<ecs::ItemLockedAttribute>(item);
    packet.lockedattr = locked ? locked->index : -1;
#endif
    return true;
}

bool EncodeItemUse(entt::registry& reg, entt::entity owner, entt::entity victim, entt::entity item, packet_item_use& packet)
{
    const auto* identity = reg.try_get<ecs::ItemIdentity>(item);
    const auto* location = reg.try_get<ecs::ItemLocation>(item);
    if (!identity || !location || identity->vnum == 0)
        return false;

    packet = {};
    packet.header = HEADER_GC_ITEM_USE;
    packet.ch_vid = ecs::PlayerRuntime::GetPacketVID(owner);
    packet.victim_vid = ecs::PlayerRuntime::GetPacketVID(victim);
    packet.Cell = TItemPos(location->window, location->cell);
    packet.vnum = identity->vnum;
    return true;
}

void SendItemInsert(entt::registry& reg, entt::entity item, entt::entity recipient)
{
    TPacketGCItemGroundAdd packet {};
    if (!EncodeItemGroundInsert(reg, item, packet))
        return;

    ecs::NetworkService::Send(recipient, &packet, sizeof(packet));

    const auto* ownership = reg.try_get<ecs::ItemOwnershipDisplay>(item);
    const auto* identity = reg.try_get<ecs::ItemIdentity>(item);
    if (!ownership || ownership->ownerName.empty() || !identity)
        return;

    TPacketGCItemOwnership ownershipPacket {};
    ownershipPacket.bHeader = HEADER_GC_ITEM_OWNERSHIP;
    ownershipPacket.dwVID = identity->vid;
    strlcpy(ownershipPacket.szName, ownership->ownerName.c_str(), sizeof(ownershipPacket.szName));
    ecs::NetworkService::Send(recipient, &ownershipPacket, sizeof(ownershipPacket));
}

void SendItemRemove(entt::registry& reg, entt::entity item, entt::entity recipient)
{
    TPacketGCItemGroundDel packet {};
    if (!EncodeItemGroundRemove(reg, item, packet))
        return;

    ecs::NetworkService::Send(recipient, &packet, sizeof(packet));
}

void SendItemUpdate(entt::registry& reg, entt::entity item)
{
    const auto* owner = reg.try_get<ecs::ItemOwner>(item);
    if (!owner)
        return;

    const auto* location = reg.try_get<ecs::ItemLocation>(item);
    if (location && location->window == SWITCHBOT)
        return;

    const entt::entity ownerEntity = FindPlayerByPID(reg, owner->ownerPID);
    if (ownerEntity == entt::null)
        return;

    TPacketGCItemUpdate packet {};
    if (!EncodeItemUpdate(reg, item, packet))
        return;

    LOG_TRACE("UpdatePacket {} -> {}",
        ItemSystem::GetItemName(item),
        ecs::PlayerRuntime::GetName(ownerEntity).data());
    ecs::NetworkService::Send(ownerEntity, &packet, sizeof(packet));
}

} // namespace ecs::ItemNetworkSystem

void CHARACTER::EncodeInsertPacket(LPENTITY entity)
{
    LPDESC d;
    if (!(d = entity->GetDesc()))
        return;

    LPCHARACTER ch = (LPCHARACTER)entity;
    ch->SendGuildName(GetGuild());

#ifdef ENABLE_SOUL_SYSTEM
    TAffectFlag sendAffectFlag = m_afAffectFlag;
    if (sendAffectFlag.IsSet(AFF_SOUL_RED) && sendAffectFlag.IsSet(AFF_SOUL_BLUE))
    {
        sendAffectFlag.Reset(AFF_SOUL_RED);
        sendAffectFlag.Reset(AFF_SOUL_BLUE);
        sendAffectFlag.Set(AFF_SOUL_MIX);
    }
#endif

    TPacketGCCharacterAdd pack;
    pack.header = HEADER_GC_CHARACTER_ADD;
    pack.dwVID = GetPacketVID();
    pack.bType = GetCharType();
    pack.angle = GetRotation();
    pack.x = GetX();
    pack.y = GetY();
    pack.z = GetZ();
    pack.wRaceNum = GetRaceNum();
    if ((pack.wRaceNum >= 20101 && pack.wRaceNum <= 20109) || IsPet()
#ifdef __NEWPET_SYSTEM__
        || IsNewPet()
#endif
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
        || m_bIsMount == true
#endif
        )
    {
#ifdef ENABLE_MULTI_NAMES
        pack.transname = false;
#endif
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
        if (m_bIsMount == true) {
            pack.bMovingSpeed = GetLimitPoint(POINT_MOV_SPEED);
        }
        else {
            pack.bMovingSpeed = IsPC() ? GetLimitPoint(POINT_MOV_SPEED) : 150;
        }
#else
        pack.bMovingSpeed = 150;
#endif
    }
    else {
#ifdef ENABLE_MULTI_NAMES
        pack.transname = true;
#endif
        pack.bMovingSpeed = GetLimitPoint(POINT_MOV_SPEED);
    }
    pack.bAttackSpeed = GetLimitPoint(POINT_ATT_SPEED);
#ifdef ENABLE_SOUL_SYSTEM
    pack.dwAffectFlag[0] = sendAffectFlag.bits[0];
    pack.dwAffectFlag[1] = sendAffectFlag.bits[1];
#else
    pack.dwAffectFlag[0] = m_afAffectFlag.bits[0];
    pack.dwAffectFlag[1] = m_afAffectFlag.bits[1];
#endif

    // B.1.5: read via getter -> ECS StatusFlags 4 bits.
    pack.bStateFlag = GetAddChrStateFlag();

    int iDur = 0;
    // B.1.4: read destination via getters (ECS MovementDestination, fallback to GetX/Y).
    const int32_t destX = GetCurrentDestX();
    const int32_t destY = GetCurrentDestY();
    if (destX != pack.x || destY != pack.y) {
        // B.1.2: read timing via getters (ECS MovementState).
        iDur = (GetCurrentMoveStartTime() + GetCurrentMoveDuration()) - get_dword_time();
        if (iDur <= 0) {
            pack.x = destX;
            pack.y = destY;
        }
    }

    d->Packet(&pack, sizeof(pack));
    if (entity->IsType(ENTITY_CHARACTER))
        NetworkSyncSystem::SendCharAdditionalInfo(
            g_registry, GetEntityHandle(), ch->GetEntityHandle());

    if (iDur) {
        TPacketGCMove pack;
        // B.1.4: use the just-resolved destX/destY from above (ECS-sourced).
        EncodeMovePacket(pack, GetPacketVID(), FUNC_MOVE, 0, destX, destY, iDur, 0, (GetRotation() / 5));
        d->Packet(&pack, sizeof(pack));

        TPacketGCWalkMode p;
        p.vid = GetPacketVID();
        p.header = HEADER_GC_WALK_MODE;
        // B.1.3: read via IsNowWalking() -> ECS MovementState.isNowWalking.
        p.mode = IsNowWalking() ? WALKMODE_WALK : WALKMODE_RUN;

        d->Packet(&p, sizeof(p));
    }

    if (entity->IsType(ENTITY_CHARACTER) && GetDesc()) {
        LPCHARACTER ch = (LPCHARACTER)entity;
        if (ch->IsWalking()) {
            TPacketGCWalkMode p;
            p.vid = ecs::PlayerRuntime::GetPacketVID(ch->GetEntityHandle());
            p.header = HEADER_GC_WALK_MODE;
            // B.1.3: read via IsNowWalking() on the viewer-side character.
            p.mode = ch->IsNowWalking() ? WALKMODE_WALK : WALKMODE_RUN;
            GetDesc()->Packet(&p, sizeof(p));
        }
    }

    if (GetMyShop()) {
        TPacketGCShopSign p;
        p.bHeader = HEADER_GC_SHOP_SIGN;
        p.dwVID = GetPacketVID();
#ifdef KASMIR_PAKET_SYSTEM
        p.bShopKasmirTitle = m_bKasmirPaketBaslik;
#endif
        strlcpy(p.szSign, m_stShopSign.c_str(), sizeof(p.szSign));

        d->Packet(&p, sizeof(TPacketGCShopSign));
    }

    if (entity->IsType(ENTITY_CHARACTER)) {
        LOG_TRACE("EntityInsert {} (RaceNum {}) ({} {}) TO {}", GetName(), GetRaceNum(), GetX() / SECTREE_SIZE, GetY() / SECTREE_SIZE, ecs::PlayerRuntime::GetName(static_cast<LPCHARACTER>(entity)->GetEntityHandle()).data());
    }
#ifdef ENABLE_FAKE_SHOP_HEADER
    if (IsPC() && entity->IsType(ENTITY_CHARACTER))
    {
        LPCHARACTER viewer = (LPCHARACTER)entity;
        const entt::entity viewerEntity = viewer->GetEntityHandle();
        if (ecs::PlayerRuntime::IsPC(viewerEntity) && ecs::PlayerRuntime::GetDesc(viewerEntity))
            UpdateMountInventoryCountOverhead(viewer ? viewer->GetEntityHandle() : entt::null);
    }
#endif
}

void CHARACTER::EncodeRemovePacket(LPENTITY entity)
{
    if (entity->GetType() != ENTITY_CHARACTER)
        return;

    LPDESC d;

    if (!(d = entity->GetDesc()))
        return;

    TPacketGCCharacterDelete pack;

    pack.header = HEADER_GC_CHARACTER_DEL;
    pack.id = GetPacketVID();

    d->Packet(&pack, sizeof(TPacketGCCharacterDelete));

    if (entity->IsType(ENTITY_CHARACTER))
        LOG_TRACE("EntityRemove {}({}) FROM {}", GetName(), GetPacketVID(), ecs::PlayerRuntime::GetName(static_cast<LPCHARACTER>(entity)->GetEntityHandle()).data());
}

bool CHARACTER::SetSyncOwner(entt::entity chEntity, bool bRemoveFromList)
{
    LPCHARACTER ch = ecs::LegacyCharOf(chEntity);
    if (IS_SET(GetAIFlag(), AIFLAG_NOMOVE))
        return false;

    if (ch)
    {
        if (!battle_is_attackable(chEntity, GetEntityHandle()))
        {
            SendDamagePacket(chEntity, 0, DAMAGE_BLOCK);
            return false;
        }
    }

    if (ch == this)
    {
        LOG_ERROR("SetSyncOwner owner == this ({})", static_cast<const void*>(this));
        return false;
    }

    if (!ch)
    {
        if (bRemoveFromList && m_pkChrSyncOwner)
        {
            m_pkChrSyncOwner->m_kLst_pkChrSyncOwned.remove(this);
        }

        if (m_pkChrSyncOwner)
            LOG_TRACE("SyncRelease {} {} from {}", GetName(), static_cast<const void*>(this), ecs::PlayerRuntime::GetName(m_pkChrSyncOwner->GetEntityHandle()).data());

        m_pkChrSyncOwner = nullptr;
    }
    else
    {
		const entt::entity syncOwner = ch->GetEntityHandle();
        if (!IsSyncOwner(chEntity))
            return false;

        if (DISTANCE_APPROX(
				GetX() - ecs::PlayerRuntime::GetX(syncOwner),
				GetY() - ecs::PlayerRuntime::GetY(syncOwner)) > 250)
        {
            LOG_TRACE("SetSyncOwner distance over than 250 {} {}", GetName(), ecs::PlayerRuntime::GetName(syncOwner).data());

            if (m_pkChrSyncOwner == ch)
                return true;

            return false;
        }

        if (m_pkChrSyncOwner != ch)
        {
            if (m_pkChrSyncOwner)
            {
                LOG_TRACE("SyncRelease {} {} from {}", GetName(), static_cast<const void*>(this), ecs::PlayerRuntime::GetName(m_pkChrSyncOwner->GetEntityHandle()).data());
                m_pkChrSyncOwner->m_kLst_pkChrSyncOwned.remove(this);
            }

            m_pkChrSyncOwner = ch;
            m_pkChrSyncOwner->m_kLst_pkChrSyncOwned.push_back(this);

            static const timeval zero_tv = { 0, 0 };
            SetLastSyncTime(zero_tv);

            LOG_TRACE("SetSyncOwner set {} {} to {}", GetName(), static_cast<const void*>(this), ecs::PlayerRuntime::GetName(syncOwner).data());
        }

        m_fSyncTime = get_float_time();
    }

    TPacketGCOwnership pack;

    pack.bHeader = HEADER_GC_OWNERSHIP;
    pack.dwOwnerVID = ch
		? ecs::PlayerRuntime::GetPacketVID(ch->GetEntityHandle())
		: 0;
    pack.dwVictimVID = GetPacketVID();

    ecs::NetworkService::BroadcastToView(g_registry, GetEntityHandle(), &pack, sizeof(pack), false);
    return true;
}

void CHARACTER::ClearSync()
{
    SetSyncOwner(entt::null);
    std::for_each(m_kLst_pkChrSyncOwned.begin(), m_kLst_pkChrSyncOwned.end(), FuncClearSync());
    m_kLst_pkChrSyncOwned.clear();
}

bool CHARACTER::IsSyncOwner(entt::entity chEntity) const
{
    LPCHARACTER ch = ecs::LegacyCharOf(chEntity);
    if (m_pkChrSyncOwner == ch)
        return true;

    if (get_float_time() - m_fSyncTime >= 3.0f)
        return true;

    return false;
}

void NetworkSyncSystem_Update(entt::registry& reg, uint32_t tick)
{
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

#ifdef ENABLE_MULTI_LANGUAGE
const char* CHARACTER::GetName(uint8_t lang) const
{
    return m_stName.empty() ? (m_pkMobData ? m_pkMobData->m_table.szLocaleName[lang] : "") : m_stName.c_str();
}
#else
const char* CHARACTER::GetName() const
{
    return m_stName.empty() ? (m_pkMobData ? m_pkMobData->m_table.szLocaleName : "") : m_stName.c_str();
}
#endif

void CHARACTER::EffectPacket(uint8_t enumEffectType)
{
    NetworkSyncSystem::BroadcastEffect(g_registry, GetEntityHandle(), enumEffectType);
}

void CHARACTER::SpecificEffectPacket(const char filename[MAX_EFFECT_FILE_NAME])
{
    NetworkSyncSystem::BroadcastSpecificEffect(g_registry, GetEntityHandle(), filename);
}

void CItem::EncodeInsertPacket(LPENTITY ent)
{
    const entt::entity item = ecs::ItemNetworkSystem::ResolveItemEntity(this);
    const entt::entity recipient = ecs::EntityFromLPENTITY(ent);
    if (item == entt::null || recipient == entt::null)
        return;

    ecs::ItemNetworkSystem::SendItemInsert(g_registry, item, recipient);
}

void CItem::EncodeRemovePacket(LPENTITY ent)
{
    const entt::entity item = ecs::ItemNetworkSystem::ResolveItemEntity(this);
    const entt::entity recipient = ecs::EntityFromLPENTITY(ent);
    if (item == entt::null || recipient == entt::null)
        return;

    ecs::ItemNetworkSystem::SendItemRemove(g_registry, item, recipient);
    LOG_TRACE("Item::EncodeRemovePacket {} to {}",
        ItemSystem::GetItemName(item),
        ecs::PlayerRuntime::GetName(recipient).data());
}

void CItem::UpdatePacket()
{
    const entt::entity item = ecs::ItemNetworkSystem::ResolveItemEntity(this);
    if (item == entt::null)
        return;

    ecs::ItemNetworkSystem::SendItemUpdate(g_registry, item);
}
