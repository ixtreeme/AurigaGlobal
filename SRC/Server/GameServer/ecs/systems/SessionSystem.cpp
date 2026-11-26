#include "TypedShop.hpp"
#include "../../core/stdafx.h"
#include "../components/skill_components.hpp"
#include "../../item/TypedBooks.hpp"
#include "../components/session_components.hpp"
#include "PlayerItemActions.hpp"
#include <utility>
#include "ViewSystem.hpp"
#include "PointSystem.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "../CharacterAccessors.hpp"

#include "SessionSystem.hpp"
#include "../../item/TypedAccessoryClock.hpp"
#include "../../item/TypedCompanions.hpp"
#include "../../item/TypedRecovery.hpp"
#include "../../item/TypedCraft.hpp"
#include "SocialSystem.hpp"
#include "SkillSystem.hpp"
#include "AffectSystem.hpp"
#include "MovementSystem.hpp"

#include "../../entity/char.h"
#include "../../entity/char_manager.h"
#include "../../command/cmd.h"
#include "../../core/config.h"
#include "../../network/db.h"
#include "../../network/desc.h"
#include "../../network/desc_client.h"
#include "../../world/arena.h"
#include "../../guild/guild.h"
#include "../../core/log.h"
#include "../../world/map_location.h"
#include "../../social/marriage.h"
#include "../../social/messenger_manager.h"
#include "../../entity/mob_manager.h"
#include "../../core/packet.h"
#include "../../network/p2p.h"
#include "../../social/party.h"
#include "../../combat/pvp.h"
#include "../EventDispatcher.hpp"
#include "../EntityFactory.hpp"
#include "../EntityInvariants.hpp"
#include "../PositionSync.hpp"
#include "../SpatialHelpers.hpp"
#include "../VIDRegistry.hpp"
#include "../Registry.hpp"
#include "../events.hpp"
#include "../services/EntityNetworkDispatch.hpp"
#include "../components/appearance_components.hpp"
#include "../components/dirty_components.hpp"
#include "../components/inventory_components.hpp"
#include "../components/social_components.hpp"
#include "../components/status_components.hpp"
#include "../components/vital_components.hpp"
#include "../../quest/questmanager.h"
#include "../../world/sectree.h"
#include "../../world/sectree_manager.h"
#include "../../social/shop.h"
#include "../../world/start_position.h"
#include "../../entity/target.h"
#include "../../core/utils.h"
#ifdef ENABLE_CPP_DUNGEON_RAZOR93
#include "../../dungeon/OrcsDungeon.h"
#include "../../dungeon/TritonTempleDungeon.h"
#include "../../dungeon/ValentineDungeon.h"
#include "../../dungeon/RuneDungeon.h"
#include "../components/visibility_components.hpp"
#include "../../dungeon/PyramidDungeonRazor93.h"
#include "../../dungeon/NightmareDungeonRazor93.h"
#include "../../dungeon/LostCastleDungeon.h"
#include "../../dungeon/Halloween2022Dungeon.h"
#include "../../dungeon/VikingDungeon.h"
#include "../../dungeon/EasterDungeon.h"
#endif
#ifdef ENABLE_SWITCHBOT
#include <Core/Logging.hpp>
#include "CombatSystem.hpp"
#include "MountSystem.hpp"
#endif

EVENTFUNC(save_event);
extern bool IS_SUMMONABLE_ZONE(int map_index);

bool CAN_ENTER_ZONE(entt::entity character, int map_index)
{
    switch (map_index)
    {
    case 301:
    case 302:
    case 303:
    case 304:
        if (ecs::PointSystem::GetLevel(character) < 90)
            return false;
    }
    return true;
}

namespace
{
} // namespace

namespace ecs::SessionSystem {

// Summoning yourself to another player, if their map allows it.
bool WarpToPID(entt::entity e, uint32_t dwPID)
{
    const entt::entity victimEntity = CHARACTER_MANAGER::instance().FindEntityByPID(dwPID);
    if (ecs::IsCharacter(victimEntity))
    {
        int mapIdx = ecs::PlayerRuntime::GetMapIndex(victimEntity);
        if (IS_SUMMONABLE_ZONE(mapIdx))
        {
            if (CAN_ENTER_ZONE(e, mapIdx))
            {
                ecs::MovementSystem::WarpSet(e, ecs::PlayerRuntime::GetX(victimEntity), ecs::PlayerRuntime::GetY(victimEntity));
            }
            else
            {
#ifdef TEXTS_IMPROVEMENT
                ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 372, "");
#endif
                return false;
            }
        }
        else
        {
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 372, "");
#endif
            return false;
        }
    }
    else
    {
        CCI* pcci = P2P_MANAGER::instance().FindByPID(dwPID);

        if (!pcci)
        {
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 371, "");
#endif
            return false;
        }

        if (pcci->bChannel != g_bChannel)
        {
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 367, "%d#%d", g_bChannel, pcci->bChannel);
#endif
            return false;
        }
        else if (false == IS_SUMMONABLE_ZONE(pcci->lMapIndex))
        {
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 372, "");
#endif
            return false;
        }
        else
        {
            if (!CAN_ENTER_ZONE(e, pcci->lMapIndex))
            {
#ifdef TEXTS_IMPROVEMENT
                ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 372, "");
#endif
                return false;
            }

            TPacketGGFindPosition p;
            p.header = HEADER_GG_FIND_POSITION;
            p.dwFromPID = ecs::PlayerRuntime::GetPlayerID(e);
            p.dwTargetPID = dwPID;
            pcci->pkDesc->Packet(&p, sizeof(TPacketGGFindPosition));

            if (test_server)
                ecs::ChatSystem::Send(e, CHAT_TYPE_PARTY, "sent find position packet for teleport");
        }
    }
    return true;
}

// Queueing a save; the manager batches them.
void Save(entt::entity e)
{
    if (!GetSkipSave(e))
        CHARACTER_MANAGER::instance().DelayedSave(e);
}

// The periodic save timer.
void StartSaveEvent(entt::entity e)
{
    if (ecs::PlayerRuntime::GetCharEvent(e, ecs::PlayerRuntime::CharEvent::Save))
        return;

    char_event_info* info = AllocEventInfo<char_event_info>();

    info->ch = e;
    ecs::PlayerRuntime::SetCharEvent(e, ecs::PlayerRuntime::CharEvent::Save,
        event_create(save_event, info, save_event_second_cycle));
}

// The save itself: the player row, the quest state and the marriage.
void SaveReal(entt::entity e)
{
    if (GetSkipSave(e))
        return;

    if (!ecs::PlayerRuntime::GetDesc(e))
    {
        LOG_ERROR("Character::Save : no descriptor when saving (name: {})", ecs::PlayerRuntime::GetName(e).data());
        return;
    }

    TPlayerTable table;
    CreatePlayerProto(e, table);

    db_clientdesc->DBPacket(HEADER_GD_PLAYER_SAVE, ecs::PlayerRuntime::GetDesc(e)->GetHandle(), &table, sizeof(TPlayerTable));

    quest::PC* pkQuestPC = quest::CQuestManager::instance().GetPCForce(ecs::PlayerRuntime::GetPlayerID(e));

    if (!pkQuestPC)
        LOG_ERROR("CHARACTER::Save : null quest::PC pointer! (name {})", ecs::PlayerRuntime::GetName(e).data());
    else
    {
        pkQuestPC->Save();
    }

    const entt::entity couple = marriage::CManager::instance().Get(ecs::PlayerRuntime::GetPlayerID(e));
    if (couple != entt::null)
        marriage::MarriageSystem::Save(couple);
}

// The safebox this character has open, if any.


// The item mall storage, which is a safebox with its own window.


// Closing the mall window.


// Closing the safebox window.


// True between asking the database for a safebox and getting it back. This
// was a CHARACTER flag beside SafeboxRef::isOpening, which nothing wrote.
bool IsSafeboxLoading(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return false;
    const auto* safebox = g_registry.try_get<ecs::SafeboxRef>(e);
    return safebox && safebox->isOpening;
}

void SetSafeboxLoading(entt::entity e, bool loading)
{
    if (e == entt::null || !g_registry.valid(e))
        return;
    g_registry.get_or_emplace<ecs::SafeboxRef>(e).isOpening = loading;
}

// The end of a session: everything the character was part of lets go of it,
// what has to survive is written, and then it is destroyed.
// The player row as it will be written: everything the session changed,
// collected out of the components that now hold it.
void CreatePlayerProto(entt::entity e, TPlayerTable& tab)
{
    memset(&tab, 0, sizeof(TPlayerTable));

    if (e == entt::null || !g_registry.valid(e))
        return;

    if (ecs::PlayerRuntime::GetPendingName(e).empty())
    {
        strlcpy(tab.name, ecs::PlayerRuntime::GetName(e).data(), sizeof(tab.name));
    }
    else
    {
        strlcpy(tab.name, ecs::PlayerRuntime::GetPendingName(e).data(), sizeof(tab.name));
    }

    strlcpy(tab.ip, ecs::PlayerRuntime::GetDesc(e) ? ecs::PlayerRuntime::GetDesc(e)->GetHostName() : "", sizeof(tab.ip));

    tab.id = ecs::PlayerRuntime::GetPlayerID(e);
    tab.voice = ecs::PointSystem::GetPrecise(e, POINT_VOICE);
    tab.level = ecs::PointSystem::GetLevel(e);
    tab.level_step = ecs::PointSystem::Get(e, POINT_LEVEL_STEP);
    tab.exp = ecs::PlayerRuntime::GetExp(e);
    tab.gold = ecs::PointSystem::GetGold(e);
#ifdef ENABLE_GAYA_SYSTEM
    tab.gaya = ecs::PointSystem::GetGaya(e);
#endif
    tab.job = 0;
    if (g_registry.valid(e))
    {
        if (const auto* points = g_registry.try_get<ecs::CharacterPoints>(e))
            tab.job = points->base.job;
    }
    if (g_registry.valid(e))
    {
        if (const auto* appearance = g_registry.try_get<ecs::AppearancePartsComponent>(e))
            tab.part_base = appearance->basePart;
    }
    tab.skill_group = SkillSystem::GetSkillGroup(e);
#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
    tab.envanter = ecs::PointSystem::GetInventoryExpansion(e);
#endif
    uint32_t dwPlayedTime = (get_dword_time() - ecs::PlayerRuntime::GetPlayStartTime(e));

    if (dwPlayedTime > 60000)
    {
        if (ecs::PlayerRuntime::GetSectree(e) && !ecs::PlayerRuntime::GetSectree(e)->IsAttr(ecs::PlayerRuntime::GetX(e), ecs::PlayerRuntime::GetY(e), ATTR_BANPK))
        {
            CombatSystem::UpdateAlignment(e, 5 * (dwPlayedTime / 60000));
        }

        ecs::PointSystem::SetReal(e, POINT_PLAYTIME, ecs::PointSystem::GetReal(e, POINT_PLAYTIME) + dwPlayedTime / 60000);
        ecs::PlayerRuntime::ResetPlayTime(e, dwPlayedTime % 60000);
    }

    tab.playtime = ecs::PointSystem::GetReal(e, POINT_PLAYTIME);
    tab.lAlignment = CombatSystem::GetRealAlignment(e);

    const auto warp = ecs::MovementSystem::GetWarpLocation(e);
    if (warp.x != 0 || warp.y != 0)
    {
        tab.x = warp.x;
        tab.y = warp.y;
        tab.z = 0;
        tab.lMapIndex = warp.mapIndex;
    }
    else
    {
        tab.x = ecs::PlayerRuntime::GetX(e);
        tab.y = ecs::PlayerRuntime::GetY(e);
        tab.z = ecs::PlayerRuntime::GetZ(e);
        tab.lMapIndex = ecs::PlayerRuntime::GetMapIndex(e);
    }

    const auto exit = ecs::MovementSystem::GetExitLocation(e);
    if (exit.mapIndex == 0)
    {
        tab.lExitMapIndex = tab.lMapIndex;
        tab.lExitX = tab.x;
        tab.lExitY = tab.y;
    }
    else
    {
        tab.lExitMapIndex = exit.mapIndex;
        tab.lExitX = exit.x;
        tab.lExitY = exit.y;
    }

    LOG_TRACE("SAVE: {} {}x{}", ecs::PlayerRuntime::GetName(e).data(), tab.x, tab.y);

    tab.st = ecs::PointSystem::GetReal(e, POINT_ST);
    tab.ht = ecs::PointSystem::GetReal(e, POINT_HT);
    tab.dx = ecs::PointSystem::GetReal(e, POINT_DX);
    tab.iq = ecs::PointSystem::GetReal(e, POINT_IQ);

    tab.stat_point = ecs::PointSystem::Get(e, POINT_STAT);
    tab.skill_point = ecs::PointSystem::Get(e, POINT_SKILL);
    tab.sub_skill_point = ecs::PointSystem::Get(e, POINT_SUB_SKILL);
    tab.horse_skill_point = ecs::PointSystem::Get(e, POINT_HORSE_SKILL);

    tab.stat_reset_count = ecs::PointSystem::Get(e, POINT_STAT_RESET_COUNT);

    tab.hp = ecs::PlayerRuntime::GetHP(e);
    tab.sp = ecs::PlayerRuntime::GetSP(e);

    tab.stamina = ecs::PlayerRuntime::GetStamina(e);

    tab.sRandomHP = ecs::PointSystem::GetRandomHP(e);
    tab.sRandomSP = ecs::PointSystem::GetRandomSP(e);

    for (int i = 0; i < QUICKSLOT_MAX_NUM; ++i)
        PlayerItemActions::GetQuickslot(e, i, tab.quickslot[i]);

    const auto& mobile = ecs::PlayerRuntime::GetMobileAuth(e);
    if (!mobile.phone.empty() && mobile.code.empty())
        strlcpy(tab.szMobile, mobile.phone.c_str(), sizeof(tab.szMobile));

    if (g_registry.valid(e))
    {
        if (const auto* appearance = g_registry.try_get<ecs::AppearancePartsComponent>(e))
            memcpy(tab.parts, appearance->parts, sizeof(tab.parts));
    }
    SkillSystem::StoreSkillLevels(e, tab.skills);
    if(const auto* book=g_registry.try_get<ecs::TypedBookRevision>(e))tab.typed_book_revision=book->value;

#ifdef ENABLE_RANKING
    const entt::entity rankEntity = e;
    for (int i = 0; i < RANKING_MAX_CATEGORIES; ++i)
        tab.lRankPoints[i] = ecs::PlayerRuntime::GetRankPoints(rankEntity, i);
#endif
    tab.horse = MountSystem::StoreHorseData(e);
}

void Disconnect(entt::entity e, const char* c_pszReason)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    if (!ecs::IsCharacter(e))
        return;

    assert(ecs::PlayerRuntime::GetDesc(e) != nullptr);

    LOG_INFO("DISCONNECT: {} ({})", ecs::PlayerRuntime::GetName(e).data(), c_pszReason ? c_pszReason : "unset");
    StopTypedGrowthPetClock(e);StopTypedRecoveryClock(e);StopTypedAccessoryClock(e);
#ifdef ENABLE_CPP_DUNGEON_RAZOR93
    COrcsDungeon::instance().OnPlayerDisconnect(e);
    CTritonTempleDungeon::instance().OnPlayerDisconnect(e);
    CValentineDungeon::instance().OnPlayerDisconnect(e);
    CRuneDungeon::instance().OnPlayerDisconnect(e);
    CPyramidDungeonRazor93::instance().OnPlayerDisconnect(e);
    CNightmareDungeonRazor93::instance().OnPlayerDisconnect(e);
    CLostCastleDungeon::instance().OnPlayerDisconnect(e);
    CHalloween2022Dungeon::instance().OnPlayerDisconnect(e);
    CVikingDungeon::instance().OnPlayerDisconnect(e);
    CEasterDungeon::instance().OnPlayerDisconnect(e);
#endif
    const entt::entity currentShop = ecs::SocialSystem::GetShop(e);
    if (currentShop != entt::null)
    {
        CloseTypedShop(e);
        ecs::SocialSystem::SetShop(e, entt::null);
    }

    if (ecs::PlayerRuntime::GetArena(e) != nullptr)
    {
        ecs::PlayerRuntime::GetArena(e)->OnDisconnect(ecs::PlayerRuntime::GetPlayerID(e));
    }

    const entt::entity party = ecs::SocialSystem::GetParty(e);

    if (party != entt::null)
    {
        PartySystem::UpdateOfflineState(party, ecs::PlayerRuntime::GetPlayerID(e));
    }

    marriage::CManager::instance().Logout(e);

    TPacketGGLogout p;
    p.bHeader = HEADER_GG_LOGOUT;
    strlcpy(p.szName, ecs::PlayerRuntime::GetName(e).data(), sizeof(p.szName));
    P2P_MANAGER::instance().Send(&p, sizeof(TPacketGGLogout));
    LogManager::instance().CharLog(e, 0, "LOGOUT", "");

    if (ecs::SocialSystem::GetWarMap(e))
        ecs::SocialSystem::SetWarMap(e, nullptr);

    if (ecs::SocialSystem::GetWeddingMap(e) != entt::null)
        ecs::SocialSystem::SetWeddingMap(e, entt::null);


    if (ecs::SocialSystem::GetGuild(e))
        ecs::SocialSystem::GetGuild(e)->LogoutMember(e);

    quest::CQuestManager::instance().LogoutPC(e);

#ifdef ENABLE_PVP_ADVANCED
    ecs::PlayerRuntime::DestroyPvP(e);
#endif

    const entt::entity logoutParty = ecs::SocialSystem::GetParty(e);

    if (logoutParty != entt::null)
        PartySystem::Unlink(logoutParty, e);

    if (CombatSystem::IsStun(e) || CombatSystem::IsDead(e))
    {
        CombatSystem::DeathPenalty(e, 0);
        ecs::PointSystem::Change(e, POINT_HP, 50 - ecs::PlayerRuntime::GetHP(e));
    }

    if (!CHARACTER_MANAGER::instance().FlushDelayedSave(e))
        SaveReal(e);

    AffectSystem::SaveAffect(e);
    AffectSystem::SetLoaded(e, false);


    SetSkipSave(e, true);

    quest::CQuestManager::instance().DisconnectPC(e);


    CPVPManager::instance().Disconnect(e);
    CTargetManager::instance().Logout(ecs::PlayerRuntime::GetPlayerID(e));
    MessengerManager::instance().Logout(ecs::PlayerRuntime::GetName(e).data());

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
    if (MountSystem::GetMountVnum(e))
    {
        AffectSystem::RemoveAffect(e, AFFECT_MOUNT);
        AffectSystem::RemoveAffect(e, AFFECT_MOUNT_BONUS);
    }
#endif

    if (ecs::PlayerRuntime::GetDesc(e))
        ecs::PlayerRuntime::GetDesc(e)->BindCharacter(entt::null);

    CHARACTER_MANAGER::instance().DestroyCharacter(e);
}

bool GetSkipSave(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return true;
    const auto* skip = g_registry.try_get<ecs::SkipSave>(e);
    return skip && skip->value;
}

void SetSkipSave(entt::entity e, bool value)
{
    if (e == entt::null || !g_registry.valid(e))
        return;
    g_registry.get_or_emplace<ecs::SkipSave>(e).value = value;
}

bool IsSafeboxOpen(entt::entity character)
{
    if (!g_registry.valid(character))
        return false;
    const auto* safebox = g_registry.try_get<ecs::SafeboxRef>(character);
    return safebox && safebox->isOpening;
}

void SetSafeboxOpen(entt::entity character, bool open)
{
    if (!g_registry.valid(character))
        return;
    g_registry.get_or_emplace<ecs::SafeboxRef>(character).isOpening = open;
    g_registry.emplace_or_replace<ecs::DirtyTag>(character);
}

bool IsCubeOpen(entt::entity character)
{
    if (!g_registry.valid(character))
        return false;
    if (g_registry.all_of<ecs::TypedCraftActive>(character)) return true;
    return false;
}

} // namespace ecs::SessionSystem

