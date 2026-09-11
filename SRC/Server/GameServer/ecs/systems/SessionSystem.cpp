#include "../../stdafx.h"
#include "../components/session_components.hpp"
#include "../../exchange.h"
#include "InventorySystem.hpp"
#include <utility>
#include "ViewSystem.hpp"
#include "PointSystem.hpp"
#include "PlayerRuntimeSystem.hpp"

#include "SessionSystem.hpp"
#include "OfflineShopSystem.hpp"
#include "SocialSystem.hpp"
#include "SkillSystem.hpp"
#include "AffectSystem.hpp"
#include "ItemSystem.hpp"
#include "MovementSystem.hpp"

#include "../../char.h"
#include "../../char_manager.h"
#include "../../cmd.h"
#include "../../config.h"
#include "../../db.h"
#include "../../desc.h"
#include "../../desc_client.h"
#include "../../arena.h"
#include "../../guild.h"
#include "../../item.h"
#include "../../item_manager.h"
#include "../../log.h"
#include "../../map_location.h"
#include "../../marriage.h"
#include "../../messenger_manager.h"
#include "../../mob_manager.h"
#include "../../packet.h"
#include "../../pcbang.h"
#include "../../p2p.h"
#include "../../party.h"
#include "../../pvp.h"
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
#include "../../questmanager.h"
#include "../../safebox.h"
#include "../../sectree.h"
#include "../../sectree_manager.h"
#include "../../shop.h"
#include "../../start_position.h"
#include "../../target.h"
#include "../../utils.h"
#ifdef ENABLE_BATTLE_PASS
#include "../../battle_pass.h"
#endif
#ifdef ENABLE_CPP_DUNGEON_RAZOR93
#include "../../OrcsDungeon.h"
#include "../../TritonTempleDungeon.h"
#include "../../ValentineDungeon.h"
#include "../../RuneDungeon.h"
#include "../components/visibility_components.hpp"
#include "../../PyramidDungeonRazor93.h"
#include "../../NightmareDungeonRazor93.h"
#include "../../Halloween2022Dungeon.h"
#include "../../VikingDungeon.h"
#include "../../EasterDungeon.h"
#endif
#ifdef __ENABLE_NEW_OFFLINESHOP__
#include "../../new_offlineshop.h"
#include "../../new_offlineshop_manager.h"
#endif
#ifdef ENABLE_SWITCHBOT
#include "../../new_switchbot.h"
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
ecs::WarpBlockState* EnsureWarpBlockState(entt::entity character)
{
    if (character == entt::null || !g_registry.valid(character))
        return nullptr;

    return &g_registry.get_or_emplace<ecs::WarpBlockState>(character);
}
} // namespace

namespace ecs::SessionSystem {

// Summoning yourself to another player, if their map allows it.
bool WarpToPID(entt::entity e, uint32_t dwPID)
{
    LPCHARACTER victim;
    if ((victim = (CHARACTER_MANAGER::instance().FindByPID(dwPID))))
    {
		const entt::entity victimEntity = victim->GetEntityHandle();
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

// Writing every carried item out before the character goes.
void FlushDelayedSaveItem(entt::entity e)
{
    const entt::entity owner = e;

    for (int i = 0; i < INVENTORY_AND_EQUIP_SLOT_MAX; ++i)
        if (const entt::entity item = ItemSystem::GetInventoryItem(owner, i); item != entt::null)
            ITEM_MANAGER::instance().SaveSingleItem(item);

    for (int i = 0; i < DRAGON_SOUL_INVENTORY_MAX_NUM; ++i)
        if (const entt::entity item = ItemSystem::GetItem(owner, TItemPos(DRAGON_SOUL_INVENTORY, i)); item != entt::null)
            ITEM_MANAGER::instance().SaveSingleItem(item);
#ifdef ENABLE_EXTRA_INVENTORY
    for (int i = 0; i < EXTRA_INVENTORY_MAX_NUM; ++i)
        if (const entt::entity item = ItemSystem::GetItem(owner, TItemPos(EXTRA_INVENTORY, i)); item != entt::null)
            ITEM_MANAGER::instance().SaveSingleItem(item);
#endif
#ifdef ENABLE_SWITCHBOT
    for (int i = 0; i < SWITCHBOT_SLOT_COUNT; ++i)
        if (const entt::entity item = ItemSystem::GetItem(owner, TItemPos(SWITCHBOT, i)); item != entt::null)
            ITEM_MANAGER::instance().SaveSingleItem(item);
#endif
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

    marriage::TMarriage* pMarriage = marriage::CManager::instance().Get(ecs::PlayerRuntime::GetPlayerID(e));
    if (pMarriage)
        pMarriage->Save();
}

// The safebox this character has open, if any.
CSafebox* GetSafebox(entt::entity e)
{
    return SafeboxSystem::Get(e, SAFEBOX).get();
}

// The item mall storage, which is a safebox with its own window.
// The ten second wait between mall load requests. CHARACTER::m_iMallLoadTime
// held it and SafeboxRef::mallLoadTime beside it was written by nothing.
int GetMallLoadTime(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return 0;

    const auto* safebox = g_registry.try_get<ecs::SafeboxRef>(e);
    return safebox ? safebox->mallLoadTime : 0;
}

void SetMallLoadTime(entt::entity e, int pulse)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    g_registry.get_or_emplace<ecs::SafeboxRef>(e).mallLoadTime = pulse;
    g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

CSafebox* GetMall(entt::entity e)
{
    return SafeboxSystem::Get(e, MALL).get();
}

// Closing the mall window.
void CloseMall(entt::entity e)
{
    const auto owner = e;
    if (!SafeboxSystem::Get(owner, MALL)) return;
    SafeboxSystem::Close(owner, MALL);
    if (!g_registry.valid(owner)) return;

    ecs::ChatSystem::Send(e, CHAT_TYPE_COMMAND, "CloseMall");
}

// Asking the database how many pages this safebox has.
void QuerySafeboxSize(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    // -1 is the component's own "not asked yet"; GetSafeboxSize reports 0 for
    // a character with no component at all, and 0 is a real size.
    const auto* known = g_registry.try_get<ecs::SafeboxRef>(e);
    if (!known || known->safeboxSize == -1)
    {
        DBManager::instance().ReturnQuery(QID_SAFEBOX_SIZE,
            ecs::PlayerRuntime::GetPlayerID(e),
            nullptr,
            "SELECT size FROM safebox%s WHERE account_id = %u",
            get_table_postfix(),
            ecs::PlayerRuntime::GetDesc(e)->GetAccountTable().id);
    }
}

// Buying another safebox page.
void ChangeSafeboxSize(entt::entity e, uint8_t bSize)
{
    TPacketCGSafeboxSize p;
    p.bHeader = HEADER_GC_SAFEBOX_SIZE;
    p.bSize = bSize;

    ecs::PlayerRuntime::GetDesc(e)->Packet(&p, sizeof(TPacketCGSafeboxSize));

    if (auto storage = SafeboxSystem::Get(e, SAFEBOX))
        storage->ChangeSize(bSize);

    if (e != entt::null && g_registry.valid(e))
        g_registry.get_or_emplace<ecs::SafeboxRef>(e).safeboxSize = bSize;
}

// Asking the database for the safebox contents.
void ReqSafeboxLoad(entt::entity e, const char* pszPassword)
{
    if (!*pszPassword || strlen(pszPassword) > SAFEBOX_PASSWORD_MAX_LEN)
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 188, "");
#endif
        return;
    }
    else if (GetSafebox(e))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 189, "");
#endif
        return;
    }

    int iPulse = thecore_pulse();

    if (iPulse - ecs::SocialSystem::GetSafeboxLoadTime(e) < PASSES_PER_SEC(10))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 190, "");
#endif
        return;
    }
#ifndef __OPEN_SAFEBOX_CLICK__
    else if (GetDistanceFromSafeboxOpen(e) > 1000)
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 185, "");
#endif
        return;
    }
#endif
    else if (IsSafeboxLoading(e))
    {
        LOG_INFO("Overlapped safebox load request from {}", ecs::PlayerRuntime::GetName(e).data());
        return;
    }

    ecs::SocialSystem::SetSafeboxLoadTime(e);
    SetSafeboxLoading(e, true);

    TSafeboxLoadPacket p;
    p.dwID = ecs::PlayerRuntime::GetDesc(e)->GetAccountTable().id;
    strlcpy(p.szLogin, ecs::PlayerRuntime::GetDesc(e)->GetAccountTable().login, sizeof(p.szLogin));
    strlcpy(p.szPassword, pszPassword, sizeof(p.szPassword));

    db_clientdesc->DBPacket(HEADER_GD_SAFEBOX_LOAD, ecs::PlayerRuntime::GetDesc(e)->GetHandle(), &p, sizeof(p));
}

// Building the safebox from what the database sent back.
void LoadSafebox(entt::entity e, int iSize, uint32_t dwGold, int iItemCount, TPlayerItem* pItems)
{
    const auto owner = e;
    const bool bLoaded = static_cast<bool>(SafeboxSystem::Get(owner, SAFEBOX));
    auto storage = SafeboxSystem::Open(owner, SAFEBOX, iSize, dwGold);
    if (!storage) return;
    ecs::SessionSystem::SetSafeboxOpen(e, true);
    if (bLoaded) storage->ChangeSize(iSize);

    g_registry.get_or_emplace<ecs::SafeboxRef>(owner).safeboxSize = iSize;

    TPacketCGSafeboxSize p;
    p.bHeader = HEADER_GC_SAFEBOX_SIZE;
    p.bSize = iSize;

    ecs::PlayerRuntime::GetDesc(e)->Packet(&p, sizeof(TPacketCGSafeboxSize));

    if (!bLoaded)
    {
        for (int i = 0; i < iItemCount; ++i, ++pItems)
        {
            if (!g_registry.valid(owner) || SafeboxSystem::Get(owner, SAFEBOX) != storage) return;
            if (!storage->IsValidPosition(pItems->pos))
                continue;

            const entt::entity item = ITEM_MANAGER::instance().CreateItem(pItems->vnum, pItems->count, pItems->id);

            if (!ItemSystem::IsValidItem(item))
            {
                LOG_ERROR("cannot create item vnum {} id {} (name: {})", pItems->vnum, pItems->id, ecs::PlayerRuntime::GetName(e).data());
                continue;
            }

            ItemSystem::SetItemSkipSave(item, true);
            ItemSystem::SetItemSockets(item, pItems->alSockets);
            ItemSystem::SetItemAttributes(item, pItems->aAttr);

			if (!storage->Add(pItems->pos, item))
                ItemSystem::DestroyItemEntityEcs(
                    item,
                    "SAFEBOX_LOAD_ADD_FAILED");
            else
                ItemSystem::SetItemSkipSave(item, false);
        }
    }
}

// The same for the mall.
void LoadMall(entt::entity e, int iItemCount, TPlayerItem* pItems)
{
    const auto owner = e;
    const bool bLoaded = static_cast<bool>(SafeboxSystem::Get(owner, MALL));
    auto storage = SafeboxSystem::Open(owner, MALL, 3 * SAFEBOX_PAGE_SIZE);
    if (!storage) return;
    if (bLoaded) storage->ChangeSize(3 * SAFEBOX_PAGE_SIZE);

    TPacketCGSafeboxSize p;
    p.bHeader = HEADER_GC_MALL_OPEN;
    p.bSize = 3 * SAFEBOX_PAGE_SIZE;

    ecs::PlayerRuntime::GetDesc(e)->Packet(&p, sizeof(TPacketCGSafeboxSize));

    if (!bLoaded)
    {
        for (int i = 0; i < iItemCount; ++i, ++pItems)
        {
            if (!g_registry.valid(owner) || SafeboxSystem::Get(owner, MALL) != storage) return;
            if (!storage->IsValidPosition(pItems->pos))
                continue;

            const entt::entity item = ITEM_MANAGER::instance().CreateItem(pItems->vnum, pItems->count, pItems->id);

            if (!ItemSystem::IsValidItem(item))
            {
                LOG_ERROR("cannot create item vnum {} id {} (name: {})", pItems->vnum, pItems->id, ecs::PlayerRuntime::GetName(e).data());
                continue;
            }

            ItemSystem::SetItemSkipSave(item, true);
            ItemSystem::SetItemSockets(item, pItems->alSockets);
            ItemSystem::SetItemAttributes(item, pItems->aAttr);

			if (!storage->Add(pItems->pos, item))
                ItemSystem::DestroyItemEntityEcs(
                    item,
                    "MALL_LOAD_ADD_FAILED");
            else
                ItemSystem::SetItemSkipSave(item, false);
        }
    }
}

// Closing the safebox window and writing what changed.
void CloseSafebox(entt::entity e)
{
    const auto owner = e;
    if (!SafeboxSystem::Get(owner, SAFEBOX)) return;

    if (!ecs::PlayerRuntime::IsPC(e) || !ecs::PlayerRuntime::GetDesc(e))
    {
        LOG_ERROR("CloseSafebox skipped: invalid owner (name={} vid={} race={} ispc={} desc={})", ecs::PlayerRuntime::GetName(e).data(), ecs::PlayerRuntime::GetPacketVID(e), ecs::PlayerRuntime::GetRaceNum(e), ecs::PlayerRuntime::IsPC(e), static_cast<const void*>(ecs::PlayerRuntime::GetDesc(e)));

        SafeboxSystem::Close(owner, SAFEBOX, false);
        if (!g_registry.valid(owner)) return;
        SetSafeboxLoading(owner, false);
        return;
    }

    ecs::SessionSystem::SetSafeboxOpen(e, false);
    SafeboxSystem::Close(owner, SAFEBOX);
    if (!g_registry.valid(owner)) return;

    ecs::ChatSystem::Send(e, CHAT_TYPE_COMMAND, "CloseSafebox");

    ecs::SocialSystem::SetSafeboxLoadTime(e);
    SetSafeboxLoading(e, false);

    ecs::SessionSystem::Save(e);
}

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

    // GetPoint, GetRealPoint, SetRealPoint, ResetPlayTime, Inven_Point, the
    // horse table, the play-start stamp and the mobile-auth pair have no
    // entity form yet; each is its own migration and they share this resolve.
    LPCHARACTER self = ecs::LegacyCharOf(e);
    if (!self)
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
    tab.voice = self->GetPoint(POINT_VOICE);
    tab.level = ecs::PointSystem::GetLevel(e);
    tab.level_step = self->GetPoint(POINT_LEVEL_STEP);
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
    tab.envanter = self->Inven_Point();
#endif
    uint32_t dwPlayedTime = (get_dword_time() - self->GetPlayStartTime());

    if (dwPlayedTime > 60000)
    {
        if (ecs::PlayerRuntime::GetSectree(e) && !ecs::PlayerRuntime::GetSectree(e)->IsAttr(ecs::PlayerRuntime::GetX(e), ecs::PlayerRuntime::GetY(e), ATTR_BANPK))
        {
            CombatSystem::UpdateAlignment(e, 5 * (dwPlayedTime / 60000));
        }

        self->SetRealPoint(POINT_PLAYTIME, self->GetRealPoint(POINT_PLAYTIME) + dwPlayedTime / 60000);
        self->ResetPlayTime(dwPlayedTime % 60000);
    }

    tab.playtime = self->GetRealPoint(POINT_PLAYTIME);
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

    tab.st = self->GetRealPoint(POINT_ST);
    tab.ht = self->GetRealPoint(POINT_HT);
    tab.dx = self->GetRealPoint(POINT_DX);
    tab.iq = self->GetRealPoint(POINT_IQ);

    tab.stat_point = self->GetPoint(POINT_STAT);
    tab.skill_point = self->GetPoint(POINT_SKILL);
    tab.sub_skill_point = self->GetPoint(POINT_SUB_SKILL);
    tab.horse_skill_point = self->GetPoint(POINT_HORSE_SKILL);

    tab.stat_reset_count = self->GetPoint(POINT_STAT_RESET_COUNT);

    tab.hp = ecs::PlayerRuntime::GetHP(e);
    tab.sp = ecs::PlayerRuntime::GetSP(e);

    tab.stamina = ecs::PlayerRuntime::GetStamina(e);

    tab.sRandomHP = ecs::PointSystem::GetRandomHP(e);
    tab.sRandomSP = ecs::PointSystem::GetRandomSP(e);

    for (int i = 0; i < QUICKSLOT_MAX_NUM; ++i)
        InventorySystem::GetQuickslot(e, i, tab.quickslot[i]);

    const auto& mobile = ecs::PlayerRuntime::GetMobileAuth(e);
    if (!mobile.phone.empty() && mobile.code.empty())
        strlcpy(tab.szMobile, mobile.phone.c_str(), sizeof(tab.szMobile));

    if (g_registry.valid(e))
    {
        if (const auto* appearance = g_registry.try_get<ecs::AppearancePartsComponent>(e))
            memcpy(tab.parts, appearance->parts, sizeof(tab.parts));
    }
    SkillSystem::StoreSkillLevels(e, tab.skills);

#ifdef ENABLE_BATTLE_PASS
    tab.dwBattlePassEndTime = AffectSystem::GetBattlePassDeadline(e);
#endif
#ifdef ENABLE_RANKING
    const entt::entity rankEntity = e;
    for (int i = 0; i < RANKING_MAX_CATEGORIES; ++i)
        tab.lRankPoints[i] = ecs::PlayerRuntime::GetRankPoints(rankEntity, i);
#endif
    tab.horse = self->GetHorseData();
}

void Disconnect(entt::entity e, const char* c_pszReason)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    // GetRealPoint, the login play time, DestroyPvP, the offline shop and
    // auction handles, the war and wedding maps and the battle-pass loaded
    // flag have no entity form yet; each is its own migration and they share
    // this one resolve.
    LPCHARACTER self = ecs::LegacyCharOf(e);
    if (!self)
        return;

    assert(ecs::PlayerRuntime::GetDesc(e) != nullptr);

    LOG_INFO("DISCONNECT: {} ({})", ecs::PlayerRuntime::GetName(e).data(), c_pszReason ? c_pszReason : "unset");
#ifdef ENABLE_CPP_DUNGEON_RAZOR93
    COrcsDungeon::instance().OnPlayerDisconnect(e);
    CTritonTempleDungeon::instance().OnPlayerDisconnect(e);
    CValentineDungeon::instance().OnPlayerDisconnect(e);
    CRuneDungeon::instance().OnPlayerDisconnect(e);
    CPyramidDungeonRazor93::instance().OnPlayerDisconnect(e);
    CNightmareDungeonRazor93::instance().OnPlayerDisconnect(e);
    CHalloween2022Dungeon::instance().OnPlayerDisconnect(e);
    CVikingDungeon::instance().OnPlayerDisconnect(e);
    CEasterDungeon::instance().OnPlayerDisconnect(e);
#endif
    if (ecs::SocialSystem::GetShop(e))
    {
        ecs::SocialSystem::GetShop(e)->RemoveGuest(e);
        ecs::SocialSystem::SetShop(e, nullptr);
    }

    if (ecs::PlayerRuntime::GetArena(e) != nullptr)
    {
        ecs::PlayerRuntime::GetArena(e)->OnDisconnect(ecs::PlayerRuntime::GetPlayerID(e));
    }

    if (ecs::SocialSystem::GetParty(e) != nullptr)
    {
        ecs::SocialSystem::GetParty(e)->UpdateOfflineState(ecs::PlayerRuntime::GetPlayerID(e));
    }

    marriage::CManager::instance().Logout(e);

    TPacketGGLogout p;
    p.bHeader = HEADER_GG_LOGOUT;
    strlcpy(p.szName, ecs::PlayerRuntime::GetName(e).data(), sizeof(p.szName));
    P2P_MANAGER::instance().Send(&p, sizeof(TPacketGGLogout));
    LogManager::instance().CharLog(e, 0, "LOGOUT", "");

#ifdef ENABLE_PCBANG_FEATURE
    {
        int32_t playTime = self->GetRealPoint(POINT_PLAYTIME) - self->m_dwLoginPlayTime;
        LogManager::instance().LoginLog(false, ecs::PlayerRuntime::GetDesc(e)->GetAccountTable().id, ecs::PlayerRuntime::GetPlayerID(e), ecs::PointSystem::GetLevel(e), ecs::PlayerRuntime::GetJob(e), playTime);

        if (0)
            CPCBangManager::instance().Log(ecs::PlayerRuntime::GetDesc(e)->GetHostName(), ecs::PlayerRuntime::GetPlayerID(e), playTime);
    }
#endif

    if (ecs::SocialSystem::GetWarMap(e))
        ecs::SocialSystem::SetWarMap(e, nullptr);

    if (ecs::SocialSystem::GetWeddingMap(e))
        ecs::SocialSystem::SetWeddingMap(e, nullptr);

#ifdef __ENABLE_NEW_OFFLINESHOP__
    offlineshop::GetManager().RemoveSafeboxFromCache(ecs::PlayerRuntime::GetPlayerID(e));
    offlineshop::GetManager().RemoveGuestFromShops(e);

    // CAuction::RemoveGuest takes the character; that is its own migration.
    if (auto* auctionGuest = ecs::OfflineShopSystem::GetAuctionGuest(e))
        auctionGuest->RemoveGuest(self);

    ecs::OfflineShopSystem::SetOfflineShop(e, nullptr);
    ecs::OfflineShopSystem::SetShopSafebox(e, nullptr);
    ecs::OfflineShopSystem::SetAuction(e, nullptr);
    ecs::OfflineShopSystem::SetAuctionGuest(e, nullptr);
    ecs::OfflineShopSystem::SetLookingOfferList(e, false);
#endif

    if (ecs::SocialSystem::GetGuild(e))
        ecs::SocialSystem::GetGuild(e)->LogoutMember(e);

    quest::CQuestManager::instance().LogoutPC(e);

#ifdef ENABLE_PVP_ADVANCED
    self->DestroyPvP();
#endif

    if (ecs::SocialSystem::GetParty(e))
        ecs::SocialSystem::GetParty(e)->Unlink(e);

    if (CombatSystem::IsStun(e) || CombatSystem::IsDead(e))
    {
        CombatSystem::DeathPenalty(e, 0);
        ecs::PointSystem::Change(e, POINT_HP, 50 - ecs::PlayerRuntime::GetHP(e));
    }

    ITEM_MANAGER::instance().FlushDelayedSaveByOwner(e);

    if (!CHARACTER_MANAGER::instance().FlushDelayedSave(e))
        SaveReal(e);

    FlushDelayedSaveItem(e);

    AffectSystem::SaveAffect(e);
    AffectSystem::SetLoaded(e, false);

#ifdef ENABLE_BATTLE_PASS
    auto it = ecs::PlayerRuntime::GetBattlePassMissions(e).begin();
    while (it != ecs::PlayerRuntime::GetBattlePassMissions(e).end())
    {
        TPlayerBattlePassMission* pkMission = *it++;

        if (pkMission->bIsUpdated)
            db_clientdesc->DBPacket(HEADER_GD_SAVE_BATTLE_PASS, 0, pkMission, sizeof(TPlayerBattlePassMission));

        if (pkMission)
            M2_DELETE(pkMission);
    }
    ecs::PlayerRuntime::SetBattlePassLoaded(e, false);
#endif

    SetSkipSave(e, true);

    quest::CQuestManager::instance().DisconnectPC(e);

    CloseSafebox(e);
    CloseMall(e);

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
        ecs::PlayerRuntime::GetDesc(e)->BindCharacter(nullptr);

    M2_DESTROY_CHARACTER(e);
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
    const auto* cube = g_registry.try_get<ecs::CubeWindowComponent>(character);
    return cube && g_registry.valid(cube->npc);
}

void SetCubeNPC(entt::entity character, entt::entity npc)
{
    if (!g_registry.valid(character))
        return;
    g_registry.get_or_emplace<ecs::CubeWindowComponent>(character).npc =
        g_registry.valid(npc) ? npc : entt::null;
}

int GetSafeboxSize(entt::entity character)
{
    if (character == entt::null || !g_registry.valid(character))
        return 0;

    const auto* safebox = g_registry.try_get<ecs::SafeboxRef>(character);
    return safebox ? safebox->safeboxSize : 0;
}

bool SetSafeboxSize(entt::entity character, int size)
{
    if (character == entt::null || !g_registry.valid(character))
        return false;

    LPDESC desc = ecs::PlayerRuntime::GetDesc(character);
    if (!desc)
        return false;

    auto& safebox = g_registry.get_or_emplace<ecs::SafeboxRef>(character);
    safebox.safeboxSize = size;
    g_registry.emplace_or_replace<ecs::DirtyTag>(character);

    LOG_INFO("SetSafeboxSize: {} {}", ecs::PlayerRuntime::GetName(character), size);
    DBManager::instance().Query("UPDATE safebox%s SET size = %d WHERE account_id = %u",
        get_table_postfix(), size / SAFEBOX_PAGE_SIZE, desc->GetAccountTable().id);
    return true;
}

bool SetSafeboxOpenPosition(entt::entity character)
{
    if (character == entt::null || !g_registry.valid(character))
        return false;

    auto& safebox = g_registry.get_or_emplace<ecs::SafeboxRef>(character);
    safebox.openX = ecs::PlayerRuntime::GetX(character);
    safebox.openY = ecs::PlayerRuntime::GetY(character);
    return true;
}

float GetDistanceFromSafeboxOpen(entt::entity character)
{
    if (character == entt::null || !g_registry.valid(character))
        return 0.0f;

    const auto* safebox = g_registry.try_get<ecs::SafeboxRef>(character);
    if (!safebox)
        return 0.0f;

    return DISTANCE_APPROX(
        ecs::PlayerRuntime::GetX(character) - safebox->openX,
        ecs::PlayerRuntime::GetY(character) - safebox->openY);
}

} // namespace ecs::SessionSystem

