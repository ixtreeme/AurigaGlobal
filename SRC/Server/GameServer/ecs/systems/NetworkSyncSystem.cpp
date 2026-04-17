#include "../../stdafx.h"

#include "NetworkSyncSystem.hpp"

#include <map>
#include <string>

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
#include "../components/combat_components.hpp"
#include "../components/dirty_components.hpp"
#include "../components/identity_components.hpp"
#include "../components/session_components.hpp"
#include "../components/transform_components.hpp"
#include "../components/vital_components.hpp"

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
    sys_log(0, "bgm_info.set_bgm_volume_enable");
}

void CHARACTER_AddBGMInfo(unsigned mapIndex, const char* name, float vol)
{
    BGMInfo newInfo;
    newInfo.name = name;
    newInfo.vol = vol;

    gs_bgmInfoMap[mapIndex] = newInfo;

    sys_log(0, "bgm_info.add_info(%d, '%s', %f)", mapIndex, name, vol);
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

namespace NetworkSyncSystem {

void UpdatePacket(entt::entity e)
{
    if (LPCHARACTER ch = LegacyCharOf(e)) {
        ch->UpdatePacket();
    }
}

void MainCharacterPacket(entt::entity e)
{
    if (LPCHARACTER ch = LegacyCharOf(e)) {
        ch->MainCharacterPacket();
    }
}

void PointsPacket(entt::entity e)
{
    if (LPCHARACTER ch = LegacyCharOf(e)) {
        ch->PointsPacket();
    }
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
    pack.dwVID = m_vid;
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
    if (IsPC() == true || m_bCharType == CHAR_TYPE_NPC) {
        TPacketGCCharacterAdditionalInfo addPacket;
        addPacket.dwLevel = 0;
        addPacket.sAlignment = 0;
        addPacket.dwMountVnum = 0;
#ifdef ENABLE_MULTI_LANGUAGE
        addPacket.bLanguage = 0;
#endif
        if (!IsPC()) {
            memcpy(addPacket.dwSkillColor, GetSkillColor(), sizeof(addPacket.dwSkillColor));
        }

        addPacket.header = HEADER_GC_CHAR_ADDITIONAL_INFO;
        addPacket.dwVID = m_vid;
        addPacket.bPKMode = m_bPKMode;
        addPacket.bEmpire = m_bEmpire;
        addPacket.dwGuildID = 0;
        strlcpy(addPacket.name, GetName(), sizeof(addPacket.name));
        addPacket.awPart[CHR_EQUIPPART_ARMOR] = GetPart(PART_MAIN);
        addPacket.awPart[CHR_EQUIPPART_WEAPON] = GetPart(PART_WEAPON);
        addPacket.awPart[CHR_EQUIPPART_HEAD] = GetPart(PART_HEAD);
        addPacket.awPart[CHR_EQUIPPART_HAIR] = GetPart(PART_HAIR);
#ifdef ENABLE_RUNE_SYSTEM
        addPacket.awPart[CHR_EQUIPPART_RUNE] = GetPart(PART_RUNE);
#endif
#ifdef ENABLE_ACCE_SYSTEM
        addPacket.awPart[CHR_EQUIPPART_ACCE] = GetPart(PART_ACCE);
#endif
#ifdef ENABLE_COSTUME_EFFECT
        addPacket.awPart[CHR_EQUIPPART_EFFECT_BODY] = GetPart(PART_EFFECT_BODY);
        addPacket.awPart[CHR_EQUIPPART_EFFECT_WEAPON] = GetPart(PART_EFFECT_WEAPON);
#endif
        if (IsPC()) {
            addPacket.dwLevel = GetLevel();
            addPacket.dwMountVnum = GetMountVnum();
            addPacket.dwGuildID = GetGuild() ? GetGuild()->GetID() : 0;
            addPacket.sAlignment = m_iAlignment / 10;

#ifdef __SKILL_COLOR_SYSTEM__
            memcpy(addPacket.dwSkillColor, GetSkillColor(), sizeof(addPacket.dwSkillColor));
#endif
        }
#ifdef __NEWPET_SYSTEM__
        if (IsNewPet()) {
            addPacket.dwLevel = GetLevel();
        }
#endif
        d->Packet(&addPacket, sizeof(TPacketGCCharacterAdditionalInfo));
    }

    if (iDur) {
        TPacketGCMove pack;
        EncodeMovePacket(pack, GetVID(), FUNC_MOVE, 0, m_posDest.x, m_posDest.y, iDur, 0, (GetRotation() / 5));
        d->Packet(&pack, sizeof(pack));

        TPacketGCWalkMode p;
        p.vid = GetVID();
        p.header = HEADER_GC_WALK_MODE;
        p.mode = m_bNowWalking ? WALKMODE_WALK : WALKMODE_RUN;

        d->Packet(&p, sizeof(p));
    }

    if (entity->IsType(ENTITY_CHARACTER) && GetDesc()) {
        LPCHARACTER ch = (LPCHARACTER)entity;
        if (ch->IsWalking()) {
            TPacketGCWalkMode p;
            p.vid = ch->GetVID();
            p.header = HEADER_GC_WALK_MODE;
            p.mode = ch->m_bNowWalking ? WALKMODE_WALK : WALKMODE_RUN;
            GetDesc()->Packet(&p, sizeof(p));
        }
    }

    if (GetMyShop()) {
        TPacketGCShopSign p;
        p.bHeader = HEADER_GC_SHOP_SIGN;
        p.dwVID = GetVID();
#ifdef KASMIR_PAKET_SYSTEM
        p.bShopKasmirTitle = m_bKasmirPaketBaslik;
#endif
        strlcpy(p.szSign, m_stShopSign.c_str(), sizeof(p.szSign));

        d->Packet(&p, sizeof(TPacketGCShopSign));
    }

    if (entity->IsType(ENTITY_CHARACTER)) {
        sys_log(3, "EntityInsert %s (RaceNum %d) (%d %d) TO %s",
            GetName(), GetRaceNum(), GetX() / SECTREE_SIZE, GetY() / SECTREE_SIZE, ((LPCHARACTER)entity)->GetName());
    }
#ifdef ENABLE_FAKE_SHOP_HEADER
    if (IsPC() && entity->IsType(ENTITY_CHARACTER))
    {
        LPCHARACTER viewer = (LPCHARACTER)entity;
        if (viewer->IsPC() && viewer->GetDesc())
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
    pack.id = m_vid;

    d->Packet(&pack, sizeof(TPacketGCCharacterDelete));

    if (entity->IsType(ENTITY_CHARACTER))
        sys_log(3, "EntityRemove %s(%d) FROM %s", GetName(), (uint32_t)m_vid, ((LPCHARACTER)entity)->GetName());
}

LPCHARACTER CHARACTER::FindCharacterInView(const char* c_pszName, bool bFindPCOnly)
{
    ENTITY_MAP::iterator it = m_map_view.begin();

    for (; it != m_map_view.end(); ++it)
    {
        if (!it->first->IsType(ENTITY_CHARACTER))
            continue;

        LPCHARACTER tch = (LPCHARACTER)it->first;

        if (bFindPCOnly && tch->IsNPC())
            continue;

        if (!strcasecmp(tch->GetName(), c_pszName))
            return tch;
    }

    return nullptr;
}

bool CHARACTER::SetSyncOwner(LPCHARACTER ch, bool bRemoveFromList)
{
    if (IS_SET(m_pointsInstant.dwAIFlag, AIFLAG_NOMOVE))
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
        sys_err("SetSyncOwner owner == this (%p)", this);
        return false;
    }

    if (!ch)
    {
        if (bRemoveFromList && m_pkChrSyncOwner)
        {
            m_pkChrSyncOwner->m_kLst_pkChrSyncOwned.remove(this);
        }

        if (m_pkChrSyncOwner)
            sys_log(1, "SyncRelease %s %p from %s", GetName(), this, m_pkChrSyncOwner->GetName());

        m_pkChrSyncOwner = nullptr;
    }
    else
    {
        if (!IsSyncOwner(ch))
            return false;

        if (DISTANCE_APPROX(GetX() - ch->GetX(), GetY() - ch->GetY()) > 250)
        {
            sys_log(1, "SetSyncOwner distance over than 250 %s %s", GetName(), ch->GetName());

            if (m_pkChrSyncOwner == ch)
                return true;

            return false;
        }

        if (m_pkChrSyncOwner != ch)
        {
            if (m_pkChrSyncOwner)
            {
                sys_log(1, "SyncRelease %s %p from %s", GetName(), this, m_pkChrSyncOwner->GetName());
                m_pkChrSyncOwner->m_kLst_pkChrSyncOwned.remove(this);
            }

            m_pkChrSyncOwner = ch;
            m_pkChrSyncOwner->m_kLst_pkChrSyncOwned.push_back(this);

            static const timeval zero_tv = { 0, 0 };
            SetLastSyncTime(zero_tv);

            sys_log(1, "SetSyncOwner set %s %p to %s", GetName(), this, ch->GetName());
        }

        m_fSyncTime = get_float_time();
    }

    TPacketGCOwnership pack;

    pack.bHeader = HEADER_GC_OWNERSHIP;
    pack.dwOwnerVID = ch ? ch->GetVID() : 0;
    pack.dwVictimVID = GetVID();

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

    sys_log(1, "PARTY %s role is %d", GetName(), out.role);

    LPCHARACTER l = GetParty()->GetLeaderCharacter();

    if (l && DISTANCE_APPROX(GetX() - l->GetX(), GetY() - l->GetY()) < PARTY_DEFAULT_RANGE)
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

    elem.dwVID = GetVID();
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
        if (!ch || !ch->GetDesc()) {
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

void CHARACTER::UpdatePacket()
{
    if (GetSectree() == nullptr)
        return;

#ifdef ENABLE_SOUL_SYSTEM
    TAffectFlag sendAffectFlag = m_afAffectFlag;
    if (sendAffectFlag.IsSet(AFF_SOUL_RED) && sendAffectFlag.IsSet(AFF_SOUL_BLUE))
    {
        sendAffectFlag.Reset(AFF_SOUL_RED);
        sendAffectFlag.Reset(AFF_SOUL_BLUE);
        sendAffectFlag.Set(AFF_SOUL_MIX);
    }
#endif

    TPacketGCCharacterUpdate pack;
    TPacketGCCharacterUpdate pack2;

    pack.header = HEADER_GC_CHARACTER_UPDATE;
    pack.dwVID = m_vid;

    pack.awPart[CHR_EQUIPPART_ARMOR] = GetPart(PART_MAIN);
    pack.awPart[CHR_EQUIPPART_WEAPON] = GetPart(PART_WEAPON);
    pack.awPart[CHR_EQUIPPART_HEAD] = GetPart(PART_HEAD);
    pack.awPart[CHR_EQUIPPART_HAIR] = GetPart(PART_HAIR);
#ifdef ENABLE_RUNE_SYSTEM
    pack.awPart[CHR_EQUIPPART_RUNE] = GetPart(PART_RUNE);
#endif
#ifdef ENABLE_ACCE_SYSTEM
    pack.awPart[CHR_EQUIPPART_ACCE] = GetPart(PART_ACCE);
#endif
#ifdef ENABLE_COSTUME_EFFECT
    pack.awPart[CHR_EQUIPPART_EFFECT_BODY] = GetPart(PART_EFFECT_BODY);
    pack.awPart[CHR_EQUIPPART_EFFECT_WEAPON] = GetPart(PART_EFFECT_WEAPON);
#endif
#ifdef __SKILL_COLOR_SYSTEM__
    memcpy(pack.dwSkillColor, GetSkillColor(), sizeof(pack.dwSkillColor));
#endif

    pack.bMovingSpeed = GetLimitPoint(POINT_MOV_SPEED);
    pack.bAttackSpeed = GetLimitPoint(POINT_ATT_SPEED);
    pack.bStateFlag = m_bAddChrState;
#ifdef ENABLE_SOUL_SYSTEM
    pack.dwAffectFlag[0] = sendAffectFlag.bits[0];
    pack.dwAffectFlag[1] = sendAffectFlag.bits[1];
#else
    pack.dwAffectFlag[0] = m_afAffectFlag.bits[0];
    pack.dwAffectFlag[1] = m_afAffectFlag.bits[1];
#endif
    pack.dwGuildID = 0;
    pack.sAlignment = m_iAlignment / 10;
#ifdef ENABLE_MULTI_LANGUAGE
    pack.bLanguage = (IsPC() && GetDesc()) ? GetDesc()->GetLanguage() : 0;
#endif
    pack.bPKMode = m_bPKMode;

    if (GetGuild())
        pack.dwGuildID = GetGuild()->GetID();

    pack.dwMountVnum = GetMountVnum();

    pack2 = pack;
    pack2.dwGuildID = 0;
    pack2.sAlignment = 0;
    if (false)
    {
        if (m_bIsObserver != true)
        {
            for (ENTITY_MAP::iterator iter = m_map_view.begin(); iter != m_map_view.end(); iter++)
            {
                LPENTITY pEntity = iter->first;

                if (pEntity != nullptr)
                {
                    if (pEntity->IsType(ENTITY_CHARACTER) == true)
                    {
                        if (pEntity->GetDesc() != nullptr)
                        {
                            LPCHARACTER pChar = (LPCHARACTER)pEntity;

                            if (GetEmpire() == pChar->GetEmpire() || pChar->GetGMLevel() > GM_PLAYER)
                            {
                                pEntity->GetDesc()->Packet(&pack, sizeof(pack));
                            }
                            else
                            {
                                pEntity->GetDesc()->Packet(&pack2, sizeof(pack2));
                            }
                        }
                    }
                    else
                    {
                        if (pEntity->GetDesc() != nullptr)
                        {
                            pEntity->GetDesc()->Packet(&pack, sizeof(pack));
                        }
                    }
                }
            }
        }

        if (GetDesc() != nullptr)
        {
            GetDesc()->Packet(&pack, sizeof(pack));
        }
    }
    else
    {
        PacketAround(&pack, sizeof(pack));
    }
}

void CHARACTER::MainCharacterPacket()
{
    const unsigned mapIndex = GetMapIndex();
    const BGMInfo& bgmInfo = CHARACTER_GetBGMInfo(mapIndex);

    if (!bgmInfo.name.empty())
    {
        if (CHARACTER_IsBGMVolumeEnable())
        {
            sys_log(1, "bgm_info.play_bgm_vol(%d, name='%s', vol=%f)", mapIndex, bgmInfo.name.c_str(), bgmInfo.vol);
            TPacketGCMainCharacter4_BGM_VOL mainChrPacket;
            mainChrPacket.header = HEADER_GC_MAIN_CHARACTER4_BGM_VOL;
            mainChrPacket.dwVID = m_vid;
            mainChrPacket.wRaceNum = GetRaceNum();
            mainChrPacket.lx = GetX();
            mainChrPacket.ly = GetY();
            mainChrPacket.lz = GetZ();
            mainChrPacket.empire = GetDesc()->GetEmpire();
            mainChrPacket.skill_group = GetSkillGroup();
            strlcpy(mainChrPacket.szChrName, GetName(), sizeof(mainChrPacket.szChrName));

            mainChrPacket.fBGMVol = bgmInfo.vol;
            strlcpy(mainChrPacket.szBGMName, bgmInfo.name.c_str(), sizeof(mainChrPacket.szBGMName));
            GetDesc()->Packet(&mainChrPacket, sizeof(TPacketGCMainCharacter4_BGM_VOL));
        }
        else
        {
            sys_log(1, "bgm_info.play(%d, '%s')", mapIndex, bgmInfo.name.c_str());
            TPacketGCMainCharacter3_BGM mainChrPacket;
            mainChrPacket.header = HEADER_GC_MAIN_CHARACTER3_BGM;
            mainChrPacket.dwVID = m_vid;
            mainChrPacket.wRaceNum = GetRaceNum();
            mainChrPacket.lx = GetX();
            mainChrPacket.ly = GetY();
            mainChrPacket.lz = GetZ();
            mainChrPacket.empire = GetDesc()->GetEmpire();
            mainChrPacket.skill_group = GetSkillGroup();
            strlcpy(mainChrPacket.szChrName, GetName(), sizeof(mainChrPacket.szChrName));
            strlcpy(mainChrPacket.szBGMName, bgmInfo.name.c_str(), sizeof(mainChrPacket.szBGMName));
            GetDesc()->Packet(&mainChrPacket, sizeof(TPacketGCMainCharacter3_BGM));
        }
    }
    else
    {
        sys_log(0, "bgm_info.play(%d, DEFAULT_BGM_NAME)", mapIndex);

        TPacketGCMainCharacter pack;
        pack.header = HEADER_GC_MAIN_CHARACTER;
        pack.dwVID = m_vid;
        pack.wRaceNum = GetRaceNum();
        pack.lx = GetX();
        pack.ly = GetY();
        pack.lz = GetZ();
        pack.empire = GetDesc()->GetEmpire();
        pack.skill_group = GetSkillGroup();
        strlcpy(pack.szName, GetName(), sizeof(pack.szName));
        GetDesc()->Packet(&pack, sizeof(TPacketGCMainCharacter));

        if (m_stMobile.length())
            ChatPacket(CHAT_TYPE_COMMAND, "sms");
    }
}

void CHARACTER::PointsPacket()
{
    if (!GetDesc())
        return;

    TPacketGCPoints pack;

    pack.header = HEADER_GC_CHARACTER_POINTS;

    pack.points[POINT_LEVEL] = GetLevel();
    pack.points[POINT_EXP] = GetExp();
    pack.points[POINT_NEXT_EXP] = GetNextExp();
    pack.points[POINT_HP] = GetHP();
    pack.points[POINT_MAX_HP] = GetMaxHP();
    pack.points[POINT_SP] = GetSP();
    pack.points[POINT_MAX_SP] = GetMaxSP();
    pack.points[POINT_GOLD] = GetGold();
    pack.points[POINT_STAMINA] = GetStamina();
    pack.points[POINT_MAX_STAMINA] = GetMaxStamina();
#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
    pack.points[POINT_INVEN] = Inven_Point();
#endif

    for (int i = POINT_ST; i < POINT_MAX_NUM; ++i)
        pack.points[i] = GetPoint(i);
#ifdef ENABLE_GAYA_SYSTEM
    pack.points[POINT_GAYA] = GetGaya();
#endif
    GetDesc()->Packet(&pack, sizeof(TPacketGCPoints));
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

    if (pTitleItem->GetType() != ITEM_BELT)
        return std::string();

    if (pTitleItem->GetValue(5) != 1)
        return std::string();

    const TItemTable* pProto = pTitleItem->GetProto();
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
    p.dwVID = GetVID();

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
    p.dwVID = GetVID();
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
    p.vid = GetVID();

    PacketAround(&p, sizeof(TPacketGCSpecialEffect));
}

void CHARACTER::SpecificEffectPacket(const char filename[MAX_EFFECT_FILE_NAME])
{
    TPacketGCSpecificEffect p;

    p.header = HEADER_GC_SPECIFIC_EFFECT;
    p.vid = GetVID();
    strlcpy(p.effect_file, filename, MAX_EFFECT_FILE_NAME);

    PacketAround(&p, sizeof(TPacketGCSpecificEffect));
}

void CHARACTER::SendEquipment(LPCHARACTER ch)
{
    TPacketViewEquip p;
    p.header = HEADER_GC_VIEW_EQUIP;
    p.vid = GetVID();

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
            p.equips[i].vnum = item->GetVnum();
            p.equips[i].count = item->GetCount();

            memcpy(p.equips[i].alSockets, item->GetSockets(), sizeof(p.equips[i].alSockets));
            memcpy(p.equips[i].aAttr, item->GetAttributes(), sizeof(p.equips[i].aAttr));
        }
        else {
            p.equips[i].vnum = 0;
        }
    }
    ch->GetDesc()->Packet(&p, sizeof(p));
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
			sys_err("CItem::EncodeInsertPacket> <Factor> Null pointer");
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
	sys_log(2, "Item::EncodeRemovePacket %s to %s", GetName(), ((LPCHARACTER)ent)->GetName());
}

void CItem::UsePacketEncode(LPCHARACTER ch, LPCHARACTER victim, packet_item_use* packet)
{
	if (!GetVnum())
		return;

	packet->header = HEADER_GC_ITEM_USE;
	packet->ch_vid = ch->GetVID();
	packet->victim_vid = victim->GetVID();
	packet->Cell = TItemPos(GetWindow(), m_wCell);
	packet->vnum = GetVnum();
}

void CItem::UpdatePacket()
{
	if (!m_pOwner || !m_pOwner->GetDesc())
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

	sys_log(2, "UpdatePacket %s -> %s", GetName(), m_pOwner->GetName());
	m_pOwner->GetDesc()->Packet(&pack, sizeof(pack));
}
