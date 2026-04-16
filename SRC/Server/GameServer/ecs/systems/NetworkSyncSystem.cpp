#include "../../stdafx.h"

#include "NetworkSyncSystem.hpp"

#include <map>
#include <string>

#include "../../char.h"
#include "../../char_manager.h"
#include "../../desc.h"
#include "../../guild.h"
#include "../../packet.h"
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
