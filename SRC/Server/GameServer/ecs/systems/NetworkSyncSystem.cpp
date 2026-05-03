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
#include "../AIHelpers.hpp"
#include "../EntityFactory.hpp"
#include "../NetworkService.hpp"
#include "../Registry.hpp"
#include "../components/appearance_components.hpp"
#include "../components/combat_components.hpp"
#include "../components/dirty_components.hpp"
#include "../components/identity_components.hpp"
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
#include <Core/Logging.hpp>

static inline LPCHARACTER LegacyCharOf(entt::entity e)
{
    auto* vid = g_registry.try_get<ecs::VIDComponent>(e);
    if (!vid) {
        return nullptr;
    }

    return CHARACTER_MANAGER::instance().Find(vid->value);
}

extern bool battle_is_attackable(LPCHARACTER ch, LPCHARACTER victim);

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

    // Visibility iteration remains a temporary service bridge until 1d.
    ch->PacketAround(&packet, sizeof(packet));
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
        ch->SetSyncOwner(nullptr, false);
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

    const auto* vid = reg.try_get<ecs::VIDComponent>(source);
    auto* ch = vid ? CHARACTER_MANAGER::instance().Find(vid->value) : nullptr;
    if (ch) {
        // AdditionalInfo is append-side payload on the client. Sending it
        // standalone to the owner/main actor can clear dynamic actors.
        ch->PacketAround(&packet, sizeof(packet), ch);
    }
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

    pack.bStateFlag = m_bAddChrState;

    int iDur = 0;
    if (m_posDest.x != pack.x || m_posDest.y != pack.y) {
        iDur = (m_dwMoveStartTime + m_dwMoveDuration) - get_dword_time();
        if (iDur <= 0) {
            pack.x = m_posDest.x;
            pack.y = m_posDest.y;
        }
    }

    d->Packet(&pack, sizeof(pack));
    if (entity->IsType(ENTITY_CHARACTER))
        NetworkSyncSystem::SendCharAdditionalInfo(g_registry, AIHelpers::EcsOf(this), AIHelpers::EcsOf(ch));

    if (iDur) {
        TPacketGCMove pack;
        EncodeMovePacket(pack, GetPacketVID(), FUNC_MOVE, 0, m_posDest.x, m_posDest.y, iDur, 0, (GetRotation() / 5));
        d->Packet(&pack, sizeof(pack));

        TPacketGCWalkMode p;
        p.vid = GetPacketVID();
        p.header = HEADER_GC_WALK_MODE;
        p.mode = m_bNowWalking ? WALKMODE_WALK : WALKMODE_RUN;

        d->Packet(&p, sizeof(p));
    }

    if (entity->IsType(ENTITY_CHARACTER) && GetDesc()) {
        LPCHARACTER ch = (LPCHARACTER)entity;
        if (ch->IsWalking()) {
            TPacketGCWalkMode p;
            p.vid = ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(ch));
            p.header = HEADER_GC_WALK_MODE;
            p.mode = ch->m_bNowWalking ? WALKMODE_WALK : WALKMODE_RUN;
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
        LOG_TRACE("EntityInsert {} (RaceNum {}) ({} {}) TO {}", GetName(), GetRaceNum(), GetX() / SECTREE_SIZE, GetY() / SECTREE_SIZE, ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(((LPCHARACTER)entity))).data());
    }
#ifdef ENABLE_FAKE_SHOP_HEADER
    if (IsPC() && entity->IsType(ENTITY_CHARACTER))
    {
        LPCHARACTER viewer = (LPCHARACTER)entity;
        if (ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(viewer)) && ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(viewer)))
            UpdateMountInventoryCountOverhead(viewer);
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
        LOG_TRACE("EntityRemove {}({}) FROM {}", GetName(), GetPacketVID(), ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(((LPCHARACTER)entity))).data());
}

LPCHARACTER CHARACTER::FindCharacterInView(const char* c_pszName, bool bFindPCOnly)
{
    ENTITY_MAP::iterator it = m_map_view.begin();

    for (; it != m_map_view.end(); ++it)
    {
        if (!it->first->IsType(ENTITY_CHARACTER))
            continue;

        LPCHARACTER tch = (LPCHARACTER)it->first;

        if (bFindPCOnly && ecs::PlayerRuntime::IsNPC(AIHelpers::EcsOf(tch)))
            continue;

        if (!strcasecmp(ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(tch)).data(), c_pszName))
            return tch;
    }

    return nullptr;
}

bool CHARACTER::SetSyncOwner(LPCHARACTER ch, bool bRemoveFromList)
{
    if (IS_SET(GetAIFlag(), AIFLAG_NOMOVE))
        return false;

    if (ch)
    {
        if (!battle_is_attackable(ch, this))
        {
            SendDamagePacket(ch, 0, DAMAGE_BLOCK);
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
            LOG_TRACE("SyncRelease {} {} from {}", GetName(), static_cast<const void*>(this), ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(m_pkChrSyncOwner)).data());

        m_pkChrSyncOwner = nullptr;
    }
    else
    {
        if (!IsSyncOwner(ch))
            return false;

        if (DISTANCE_APPROX(GetX() - ecs::PlayerRuntime::GetX(AIHelpers::EcsOf(ch)), GetY() - ecs::PlayerRuntime::GetY(AIHelpers::EcsOf(ch))) > 250)
        {
            LOG_TRACE("SetSyncOwner distance over than 250 {} {}", GetName(), ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data());

            if (m_pkChrSyncOwner == ch)
                return true;

            return false;
        }

        if (m_pkChrSyncOwner != ch)
        {
            if (m_pkChrSyncOwner)
            {
                LOG_TRACE("SyncRelease {} {} from {}", GetName(), static_cast<const void*>(this), ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(m_pkChrSyncOwner)).data());
                m_pkChrSyncOwner->m_kLst_pkChrSyncOwned.remove(this);
            }

            m_pkChrSyncOwner = ch;
            m_pkChrSyncOwner->m_kLst_pkChrSyncOwned.push_back(this);

            static const timeval zero_tv = { 0, 0 };
            SetLastSyncTime(zero_tv);

            LOG_TRACE("SetSyncOwner set {} {} to {}", GetName(), static_cast<const void*>(this), ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data());
        }

        m_fSyncTime = get_float_time();
    }

    TPacketGCOwnership pack;

    pack.bHeader = HEADER_GC_OWNERSHIP;
    pack.dwOwnerVID = ch ? ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(ch)) : 0;
    pack.dwVictimVID = GetPacketVID();

    PacketAround(&pack, sizeof(TPacketGCOwnership));
    return true;
}

void CHARACTER::ClearSync()
{
    SetSyncOwner(nullptr);
    std::for_each(m_kLst_pkChrSyncOwned.begin(), m_kLst_pkChrSyncOwned.end(), FuncClearSync());
    m_kLst_pkChrSyncOwned.clear();
}

bool CHARACTER::IsSyncOwner(LPCHARACTER ch) const
{
    if (m_pkChrSyncOwner == ch)
        return true;

    if (get_float_time() - m_fSyncTime >= 3.0f)
        return true;

    return false;
}

bool CHARACTER::BuildUpdatePartyPacket(TPacketGCPartyUpdate& out)
{
    if (!GetParty())
        return false;

    memset(&out, 0, sizeof(out));

    out.header = HEADER_GC_PARTY_UPDATE;
    out.pid = GetPlayerID();
    if (GetMaxHP() <= 0)
        out.percent_hp = 0;
    else
        out.percent_hp = MINMAX((int64_t)0, GetHP() * 100 / GetMaxHP(), (int64_t)100);
    out.role = GetParty()->GetRole(GetPlayerID());

    LOG_INFO("PARTY {} role is {}", GetName(), out.role);

    LPCHARACTER l = GetParty()->GetLeaderCharacter();

    if (l && DISTANCE_APPROX(GetX() - ecs::PlayerRuntime::GetX(AIHelpers::EcsOf(l)), GetY() - ecs::PlayerRuntime::GetY(AIHelpers::EcsOf(l))) < PARTY_DEFAULT_RANGE)
    {
        out.affects[0] = GetParty()->GetPartyBonusExpPercent();
        out.affects[1] = GetPoint(POINT_PARTY_ATTACKER_BONUS);
        out.affects[2] = GetPoint(POINT_PARTY_TANKER_BONUS);
        out.affects[3] = GetPoint(POINT_PARTY_BUFFER_BONUS);
        out.affects[4] = GetPoint(POINT_PARTY_SKILL_MASTER_BONUS);
        out.affects[5] = GetPoint(POINT_PARTY_HASTE_BONUS);
        out.affects[6] = GetPoint(POINT_PARTY_DEFENDER_BONUS);
    }

    return true;
}

void CHARACTER::SyncPacket()
{
    TEMP_BUFFER buf;

    TPacketCGSyncPositionElement elem;

    elem.dwVID = GetPacketVID();
    elem.lX = GetX();
    elem.lY = GetY();

    TPacketGCSyncPosition pack;

    pack.bHeader = HEADER_GC_SYNC_POSITION;
    pack.wSize = sizeof(TPacketGCSyncPosition) + sizeof(elem);

    buf.write(&pack, sizeof(pack));
    buf.write(&elem, sizeof(elem));

    PacketAround(buf.read_peek(), buf.size());
}

void NetworkSyncSystem_Update(entt::registry& reg, uint32_t tick)
{
    auto view = reg.view<ecs::TagPC, ecs::NetworkSession, ecs::Position, ecs::Health, ecs::Mana, ecs::VIDComponent, ecs::DirtyTag>();

    for (const entt::entity entity : view) {
        LPCHARACTER ch = LegacyCharOf(entity);
        if (!ch || !ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))) {
            continue;
        }

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

#ifdef ENABLE_ITEM_ON_TITLE_RAZOR93
std::string CHARACTER::GetItemOnTitlePrefix() const
{
    if (!IsPC())
        return std::string();

    LPITEM pTitleItem = GetWear(WEAR_BELT);
    if (!pTitleItem)
        return std::string();

    if (ItemSystem::GetItemType(EntityFactory::CreateItemEntity(g_registry, pTitleItem)) != ITEM_BELT)
        return std::string();

    if (ItemSystem::GetItemValue(EntityFactory::CreateItemEntity(g_registry, pTitleItem), 5) != 1)
        return std::string();

    const TItemTable* pProto = ItemSystem::GetItemProto(EntityFactory::CreateItemEntity(g_registry, pTitleItem));
    const char* szProtoName = pProto ? pProto->szName : nullptr;
    if (!szProtoName || szProtoName[0] != '[')
        return std::string();

    const char* pEnd = strchr(szProtoName, ']');
    if (!pEnd)
        return std::string();

    const int prefixLen = (int)(pEnd - szProtoName) + 1;
    if (prefixLen <= 0 || prefixLen > 24)
        return std::string();

    return std::string(szProtoName, prefixLen);
}

std::string CHARACTER::GetDisplayedNameWithItemOnTitle() const
{
    const std::string prefix = GetItemOnTitlePrefix();
    if (prefix.empty())
        return std::string(GetName());
    return prefix + std::string(GetName());
}

void CHARACTER::SendItemOnTitleNameToDesc(LPDESC d) const
{
    if (!d)
        return;

    TPacketGCItemOnTitleNameUpdate p;
    p.header = HEADER_GC_ITEM_ON_TITLE_NAME_UPDATE;
    p.dwVID = GetPacketVID();

    const std::string prefix = GetItemOnTitlePrefix();
    strlcpy(p.name, prefix.c_str(), sizeof(p.name));

    d->Packet(&p, sizeof(p));
}

void CHARACTER::UpdateItemOnTitleName(bool bForce)
{
    const std::string newPrefix = GetItemOnTitlePrefix();
    if (!bForce && m_lastItemOnTitlePrefix == newPrefix)
        return;

    m_lastItemOnTitlePrefix = newPrefix;

    TPacketGCItemOnTitleNameUpdate p;
    p.header = HEADER_GC_ITEM_ON_TITLE_NAME_UPDATE;
    p.dwVID = GetPacketVID();
    strlcpy(p.name, newPrefix.c_str(), sizeof(p.name));

    if (GetDesc())
        GetDesc()->Packet(&p, sizeof(p));

    PacketAround(&p, sizeof(p));
}
#endif

void CHARACTER::EffectPacket(uint8_t enumEffectType)
{
    TPacketGCSpecialEffect p;

    p.header = HEADER_GC_SEPCIAL_EFFECT;
    p.type = enumEffectType;
    p.vid = GetPacketVID();

    PacketAround(&p, sizeof(TPacketGCSpecialEffect));
}

void CHARACTER::SpecificEffectPacket(const char filename[MAX_EFFECT_FILE_NAME])
{
    TPacketGCSpecificEffect p;

    p.header = HEADER_GC_SPECIFIC_EFFECT;
    p.vid = GetPacketVID();
    strlcpy(p.effect_file, filename, MAX_EFFECT_FILE_NAME);

    PacketAround(&p, sizeof(TPacketGCSpecificEffect));
}

void CHARACTER::SendEquipment(LPCHARACTER ch)
{
    TPacketViewEquip p;
    p.header = HEADER_GC_VIEW_EQUIP;
    p.vid = GetPacketVID();

#ifdef EQUIP_ENABLE_VIEW_SASH
#ifdef ENABLE_COSTUME_PET
    int pos[23] = { WEAR_BODY, WEAR_HEAD, WEAR_FOOTS, WEAR_WRIST, WEAR_WEAPON, WEAR_NECK, WEAR_EAR, WEAR_UNIQUE1,
                WEAR_UNIQUE2, WEAR_ARROW, WEAR_SHIELD, WEAR_COSTUME_BODY, WEAR_COSTUME_HAIR, WEAR_RING1, WEAR_RING2, WEAR_BELT, WEAR_COSTUME_ACCE_SLOT, WEAR_COSTUME_ACCE, WEAR_COSTUME_WEAPON, WEAR_COSTUME_PET_SKIN, WEAR_COSTUME_MOUNT_SKIN, WEAR_COSTUME_EFFECT_BODY, WEAR_COSTUME_EFFECT_WEAPON };
    for (int i = 0; i < 23; i++)
#else
    int pos[22] = { WEAR_BODY, WEAR_HEAD, WEAR_FOOTS, WEAR_WRIST, WEAR_WEAPON, WEAR_NECK, WEAR_EAR, WEAR_UNIQUE1,
                    WEAR_UNIQUE2, WEAR_ARROW, WEAR_SHIELD, WEAR_COSTUME_BODY, WEAR_COSTUME_HAIR, WEAR_RING1, WEAR_RING2, WEAR_BELT, WEAR_COSTUME_ACCE_SLOT, WEAR_COSTUME_ACCE, WEAR_COSTUME_WEAPON, WEAR_COSTUME_MOUNT_SKIN, WEAR_COSTUME_EFFECT_BODY, WEAR_COSTUME_EFFECT_WEAPON };
    for (int i = 0; i < 22; i++)
#endif
#else
    int pos[16] = { WEAR_BODY, WEAR_HEAD, WEAR_FOOTS, WEAR_WRIST, WEAR_WEAPON, WEAR_NECK, WEAR_EAR, WEAR_UNIQUE1,
                    WEAR_UNIQUE2, WEAR_ARROW, WEAR_SHIELD, WEAR_COSTUME_BODY, WEAR_COSTUME_HAIR, WEAR_RING1, WEAR_RING2, WEAR_BELT };
    for (int i = 0; i < 16; i++)
#endif
    {
        LPITEM item = GetWear(pos[i]);
        if (item) {
            p.equips[i].vnum = ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, item));
            p.equips[i].count = ItemSystem::GetItemCount(EntityFactory::CreateItemEntity(g_registry, item));

            memcpy(p.equips[i].alSockets, item->GetSockets(), sizeof(p.equips[i].alSockets));
            memcpy(p.equips[i].aAttr, item->GetAttributes(), sizeof(p.equips[i].aAttr));
        }
        else {
            p.equips[i].vnum = 0;
        }
    }
    ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->Packet(&p, sizeof(p));
}

void CHARACTER::ConfirmWithMsg(const char* szMsg, int iTimeout, uint32_t dwRequestPID)
{
    if (!IsPC())
        return;

    TPacketGCQuestConfirm p;

    p.header = HEADER_GC_QUEST_CONFIRM;
    p.requestPID = dwRequestPID;
    p.timeout = iTimeout;
    strlcpy(p.msg, szMsg, sizeof(p.msg));

    GetDesc()->Packet(&p, sizeof(p));
}

void CItem::EncodeInsertPacket(LPENTITY ent)
{
	LPDESC d;

	if (!(d = ent->GetDesc()))
		return;

	const PIXEL_POSITION& c_pos = GetXYZ();

	packet_item_ground_add pack;

	pack.bHeader = HEADER_GC_ITEM_GROUND_ADD;
	pack.x = c_pos.x;
	pack.y = c_pos.y;
	pack.z = c_pos.z;
	pack.dwVnum = GetVnum();
	pack.dwVID = m_dwVID;
	//pack.count	= m_dwCount;

	d->Packet(&pack, sizeof(pack));

	if (m_pkOwnershipEvent != nullptr)
	{
		auto info = dynamic_cast<item_event_info*>(m_pkOwnershipEvent->info);

		if (info == nullptr)
		{
			LOG_ERROR("CItem::EncodeInsertPacket> <Factor> Null pointer");
			return;
		}

		TPacketGCItemOwnership p;

		p.bHeader = HEADER_GC_ITEM_OWNERSHIP;
		p.dwVID = m_dwVID;
		strlcpy(p.szName, info->szOwnerName, sizeof(p.szName));

		d->Packet(&p, sizeof(TPacketGCItemOwnership));
	}
}

void CItem::EncodeRemovePacket(LPENTITY ent)
{
	LPDESC d;

	if (!(d = ent->GetDesc()))
		return;

	packet_item_ground_del pack;

	pack.bHeader = HEADER_GC_ITEM_GROUND_DEL;
	pack.dwVID = m_dwVID;

	d->Packet(&pack, sizeof(pack));
	LOG_TRACE("Item::EncodeRemovePacket {} to {}", GetName(), ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(((LPCHARACTER)ent))).data());
}

void CItem::UsePacketEncode(LPCHARACTER ch, LPCHARACTER victim, packet_item_use* packet)
{
	if (!GetVnum())
		return;

	packet->header = HEADER_GC_ITEM_USE;
	packet->ch_vid = ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(ch));
	packet->victim_vid = ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(victim));
	packet->Cell = TItemPos(GetWindow(), m_wCell);
	packet->vnum = GetVnum();
}

void CItem::UpdatePacket()
{
	if (!m_pOwner || !ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(m_pOwner)))
		return;

#ifdef ENABLE_SWITCHBOT
	if (m_bWindow == SWITCHBOT)
		return;
#endif

	TPacketGCItemUpdate pack;

	pack.header = HEADER_GC_ITEM_UPDATE;
	pack.Cell = TItemPos(GetWindow(), m_wCell);
	pack.count = m_dwCount;
#ifdef ATTR_LOCK
	pack.lockedattr = m_sLockedAttr;
#endif

	for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
		pack.alSockets[i] = m_alSockets[i];

	memcpy(pack.aAttr, GetAttributes(), sizeof(pack.aAttr));

	LOG_TRACE("UpdatePacket {} -> {}", GetName(), ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(m_pOwner)).data());
	ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(m_pOwner))->Packet(&pack, sizeof(pack));
}

