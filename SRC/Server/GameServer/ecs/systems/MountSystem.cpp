#include "../../stdafx.h"

#include "MountSystem.hpp"

#include "../../config.h"
#include "../../char.h"
#include "../../char_manager.h"
#include "../../db.h"
#include "../../packet.h"
#include "../../guild.h"
#include "../../vector.h"
#include "../../questmanager.h"
#include "../../item.h"
#include "../../item_manager.h"
#include "../../MountSystem.h"
#include "../../MountInventory.h"
#include "../../mount_inventory_helper.h"
#include "../../horsename_manager.h"
#include "../../locale_service.h"
#include "../../arena.h"
#include "../../desc.h"
#include "../../PetSystem.h"
#include "../AIHelpers.hpp"
#include "../Registry.hpp"
#include "../VIDRegistry.hpp"
#include "../components/dirty_components.hpp"
#include "../components/identity_components.hpp"
#include "../components/social_components.hpp"

#include <common/VnumHelper.h>

namespace
{

LPCHARACTER LegacyCharacter(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    auto* vid = g_registry.try_get<ecs::VIDComponent>(e);
    if (!vid)
        return nullptr;

    return CHARACTER_MANAGER::instance().Find(vid->value);
}

ecs::MountState* GetMountState(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    return &g_registry.get_or_emplace<ecs::MountState>(e);
}

void SyncMountState(entt::entity e, uint32_t mountVnum, uint32_t mountTime,
    uint8_t sendHorseLevel, uint8_t sendHorseHealthGrade, uint8_t sendHorseStaminaGrade, int mountPulse)
{
    auto* state = GetMountState(e);
    if (!state)
        return;

    state->mountVnum = mountVnum;
    state->mountTime = mountTime;
    state->sendHorseLevel = sendHorseLevel;
    state->sendHorseHealthGrade = sendHorseHealthGrade;
    state->sendHorseStaminaGrade = sendHorseStaminaGrade;
    state->mountPulse = mountPulse;
    g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

} // namespace

namespace MountSystem {

bool StartRiding(entt::entity rider)
{
    LPCHARACTER ch = LegacyCharacter(rider);
    return ch ? ch->StartRiding() : false;
}

bool StopRiding(entt::entity rider)
{
    LPCHARACTER ch = LegacyCharacter(rider);
    return ch ? ch->StopRiding() : false;
}

void SetRider(entt::entity horse, entt::entity rider)
{
    LPCHARACTER chHorse = LegacyCharacter(horse);
    LPCHARACTER chRider = LegacyCharacter(rider);
    if (chHorse)
        chHorse->SetRider(chRider);
}

entt::entity GetRider(entt::entity horse)
{
    LPCHARACTER chHorse = LegacyCharacter(horse);
    if (!chHorse)
        return entt::null;

    return chHorse->GetRider() ? CVIDRegistry::Instance().Find(chHorse->GetRider()->GetVID()) : entt::null;
}

void HorseSummon(entt::entity owner, bool summon, bool fromFar, uint32_t vnum, const char* petName)
{
    LPCHARACTER ch = LegacyCharacter(owner);
    if (ch)
        ch->HorseSummon(summon, fromFar, vnum, petName);
}

uint32_t GetMyHorseVnum(entt::entity owner)
{
    LPCHARACTER ch = LegacyCharacter(owner);
    return ch ? ch->GetMyHorseVnum() : 0;
}

void HorseDie(entt::entity owner)
{
    LPCHARACTER ch = LegacyCharacter(owner);
    if (ch)
        ch->HorseDie();
}

bool ReviveHorse(entt::entity owner)
{
    LPCHARACTER ch = LegacyCharacter(owner);
    return ch ? ch->ReviveHorse() : false;
}

void ClearHorseInfo(entt::entity owner)
{
    LPCHARACTER ch = LegacyCharacter(owner);
    if (ch)
        ch->ClearHorseInfo();
}

void SendHorseInfo(entt::entity owner)
{
    LPCHARACTER ch = LegacyCharacter(owner);
    if (ch)
        ch->SendHorseInfo();
}

} // namespace MountSystem

CMountInventory* CHARACTER::GetMountInventory() const
{
    return m_pkMountInventory;
}

void CHARACTER::QueryMountInventory()
{
    if (m_bMountInventoryLoaded || !GetDesc())
        return;

    DBManager::instance().ReturnQuery(QID_MOUNT_INVENTORY_LOAD,
        GetPlayerID(),
        nullptr,
        "SELECT id, slot, vnum, count, socket0, socket1, socket2, "
        "attrtype0, attrvalue0, attrtype1, attrvalue1, attrtype2, attrvalue2, "
        "attrtype3, attrvalue3, attrtype4, attrvalue4, attrtype5, attrvalue5 "
        "FROM account_mount_inventory WHERE account_id=%u ORDER BY slot",
        GetDesc()->GetAccountTable().id);
}

void CHARACTER::LoadMountInventory(const std::vector<TMountInventoryItemTable>& items)
{
    if (m_bMountInventoryLoaded)
        return;

    const int iHeight = 16;
    m_pkMountInventory = M2_NEW CMountInventory(this, iHeight);

    for (const auto& entry : items)
    {
        LPITEM item = ITEM_MANAGER::instance().CreateItem(entry.vnum, entry.count, entry.id);
        if (!item)
            continue;

        item->SetSkipSave(true);
        item->SetSockets(entry.alSockets);
        item->SetAttributes(entry.aAttr);

        if (!m_pkMountInventory->Add(entry.slot, item, true))
            M2_DESTROY_ITEM(item);
    }

    m_bMountInventoryLoaded = true;
    SendMountInventory();
    ComputePoints();
}

void CHARACTER::SendMountInventory()
{
    if (!GetDesc() || !m_pkMountInventory)
        return;

    std::vector<TMountInventoryItemTable> items;
    m_pkMountInventory->CollectItems(items);

    TPacketGCMountInventory header{};
    header.bHeader = HEADER_GC_MOUNT_INVENTORY;
    header.size = sizeof(TPacketGCMountInventory) + static_cast<uint16_t>(items.size() * sizeof(TMountInventoryItemData));
    header.bWidth = m_pkMountInventory->GetWidth();
    header.bHeight = m_pkMountInventory->GetSize();
    header.wCount = static_cast<uint16_t>(items.size());

    TEMP_BUFFER buf;
    buf.write(&header, sizeof(header));

    for (const auto& entry : items)
    {
        TMountInventoryItemData data{};
        data.wSlot = entry.slot;
        data.dwVnum = entry.vnum;
        data.dwCount = entry.count;
        memcpy(data.alSockets, entry.alSockets, sizeof(data.alSockets));
        memcpy(data.aAttr, entry.aAttr, sizeof(data.aAttr));
        buf.write(&data, sizeof(data));
    }

    GetDesc()->Packet(buf.read_peek(), buf.size());
}

int CHARACTER::GetBeltCount() const
{
    int beltItemCount = 0;
    for (int i = BELT_INVENTORY_SLOT_START; i < BELT_INVENTORY_SLOT_END; ++i)
    {
        if (GetInventoryItem(i))
            ++beltItemCount;
    }

    return beltItemCount;
}

int CHARACTER::GetMountCount() const
{
    int mountItemCount = 0;
    if (CMountInventory* mi = GetMountInventory())
    {
        const int total = mi->GetWidth() * mi->GetSize();
        for (int pos = 0; pos < total; ++pos)
        {
            if (mi->Get(pos))
                ++mountItemCount;
        }
    }

    return mountItemCount;
}

void CHARACTER::UpdateMountInventoryCountOverhead(LPCHARACTER viewer)
{
    if (!IsPC())
        return;

    if (!viewer || !viewer->IsPC())
        return;

    if (!viewer->GetDesc())
        return;

    TPacketGCFakeShopSign p;
    p.bHeader = HEADER_GC_FAKE_SHOP_SIGN;
    p.dwVID = GetVID();
    p.iMountCount = GetMountCount();
    p.iBeltCount = GetBeltCount();

    viewer->GetDesc()->Packet(&p, sizeof(p));
}

void CHARACTER::UpdateMountCountOverheadToViewers()
{
#ifdef ENABLE_FAKE_SHOP_HEADER
    UpdateMountInventoryCountOverhead(this);

    for (const auto& it : m_map_view)
    {
        LPENTITY ent = it.first;
        if (!ent || !ent->IsType(ENTITY_CHARACTER))
            continue;

        LPCHARACTER viewer = (LPCHARACTER)ent;
        if (!viewer || viewer == this)
            continue;

        if (viewer->IsPC() && viewer->GetDesc())
            UpdateMountInventoryCountOverhead(viewer);
    }
#endif
}

namespace MountSystem {

bool CanUseHorseSkill(entt::entity owner)
{
    LPCHARACTER ch = LegacyCharacter(owner);
    return ch ? ch->CanUseHorseSkill() : false;
}

void SetHorseLevel(entt::entity owner, int level)
{
    LPCHARACTER ch = LegacyCharacter(owner);
    if (ch)
        ch->SetHorseLevel(level);
}

bool IsRiding(entt::entity rider)
{
    LPCHARACTER ch = LegacyCharacter(rider);
    return ch ? ch->IsRiding() : false;
}

uint32_t GetMountVnum(entt::entity rider)
{
    LPCHARACTER ch = LegacyCharacter(rider);
    return ch ? ch->GetMountVnum() : 0;
}

} // namespace MountSystem

bool CHARACTER::StartRiding()
{
	ChatPacket(CHAT_TYPE_INFO, "DEBUG: char_horse.cpp: bool CHARACTER::StartRiding()");
#ifdef ENABLE_BUG_FIXES
	if (IsRiding()) {
		return false;
	}
#endif

#ifdef BLOCK_RIDING_INSIDE_WAR
	if (GetWarMap()) {
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 852, "");
#endif
		RemoveAffect(AFFECT_MOUNT);
		RemoveAffect(AFFECT_MOUNT_BONUS);
		if (IsRiding())
			StopRiding();
		return false;
	}
#endif

#ifdef ENABLE_NEWSTUFF
	if (g_NoMountAtGuildWar && GetWarMap())
	{
		RemoveAffect(AFFECT_MOUNT);
		RemoveAffect(AFFECT_MOUNT_BONUS);
		if (IsRiding())
			StopRiding();
		return false;
	}
#endif
	if (IsDead() == true)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 356, "");
#endif
		return false;
	}

	if (IsPolymorphed())
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 355, "");
#endif
		return false;
	}

	LPITEM armor = GetWear(WEAR_BODY);

	if (armor && (armor->GetVnum() >= 11901 && armor->GetVnum() <= 11904))
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 410, "");
#endif
		return false;
	}

	if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
		return false;

	uint32_t dwMountVnum = m_chHorse ? m_chHorse->GetRaceNum() : GetMyHorseVnum();

	if (false == CHorseRider::StartRiding())
	{
#ifdef TEXTS_IMPROVEMENT
		if (GetHorseLevel() <= 0) {
			ChatPacketNew(CHAT_TYPE_INFO, 333, "");
		} else if (GetHorseHealth() <= 0) {
			ChatPacketNew(CHAT_TYPE_INFO, 335, "");
		} else if (GetHorseStamina() <= 0) {
			ChatPacketNew(CHAT_TYPE_INFO, 334, "");
		}
#endif
		return false;
	}

	HorseSummon(false);

	MountVnum(dwMountVnum);

	if(test_server)
		sys_log(0, "Ride Horse : %s ", GetName());

#ifdef DISABLE_CORE_PULSE_RAZOR93
	SyncMountState(AIHelpers::EcsOf(this), GetMountVnum(), GetLastMountTime(), m_bSendHorseLevel, m_bSendHorseHealthGrade, m_bSendHorseStaminaGrade, m_mountPulse);
#else
	SyncMountState(AIHelpers::EcsOf(this), GetMountVnum(), GetLastMountTime(), m_bSendHorseLevel, m_bSendHorseHealthGrade, m_bSendHorseStaminaGrade, 0);
#endif
	return true;
}

bool CHARACTER::StopRiding()
{
	ChatPacket(CHAT_TYPE_INFO, "DEBUG: char_horse.cpp: bool CHARACTER::StopRiding()");

	if (CHorseRider::StopRiding())
	{
		quest::CQuestManager::instance().Unmount(GetPlayerID());

		if (!IsDead() && !IsStun())
		{
			uint32_t dwOldVnum = GetMountVnum();
			MountVnum(0);
			HorseSummon(true, false, dwOldVnum);
		}
		else
		{
			m_dwMountVnum = 0;
			ComputePoints();
			UpdatePacket();
		}

		PointChange(POINT_ST, 0);
		PointChange(POINT_DX, 0);
		PointChange(POINT_HT, 0);
		PointChange(POINT_IQ, 0);
#ifdef DISABLE_CORE_PULSE_RAZOR93
		SyncMountState(AIHelpers::EcsOf(this), GetMountVnum(), GetLastMountTime(), m_bSendHorseLevel, m_bSendHorseHealthGrade, m_bSendHorseStaminaGrade, m_mountPulse);
#else
		SyncMountState(AIHelpers::EcsOf(this), GetMountVnum(), GetLastMountTime(), m_bSendHorseLevel, m_bSendHorseHealthGrade, m_bSendHorseStaminaGrade, 0);
#endif
		return true;
	}

	return false;
}

EVENTFUNC(horse_dead_event)
{
	char_event_info* info = dynamic_cast<char_event_info*>( event->info );

	if ( info == nullptr)
	{
		sys_err( "horse_dead_event> <Factor> Null pointer" );
		return 0;
	}

	LPCHARACTER ch = info->ch;
	if (ch == nullptr) {
		return 0;
	}
	ch->HorseSummon(false);
	return 0;
}

void CHARACTER::SetRider(LPCHARACTER ch)
{
	if (m_chRider)
		m_chRider->ClearHorseInfo();

	m_chRider = ch;

	if (m_chRider)
		m_chRider->SendHorseInfo();
}

LPCHARACTER CHARACTER::GetRider() const
{
	return m_chRider;
}

void CHARACTER::HorseSummon(bool bSummon, bool bFromFar, uint32_t dwVnum, const char* pPetName)
{
	if ( bSummon )
	{
		if( m_chHorse != nullptr)
			return;

		if (GetHorseLevel() <= 0)
			return;

		if (IsRiding())
			return;

		sys_log(0, "HorseSummon : %s lv:%d bSummon:%d fromFar:%d", GetName(), GetLevel(), bSummon, bFromFar);

		int32_t x = GetX();
		int32_t y = GetY();

		if (GetHorseHealth() <= 0)
			bFromFar = false;

		if (bFromFar)
		{
			x += (number(0, 1) * 2 - 1) * number(2000, 2500);
			y += (number(0, 1) * 2 - 1) * number(2000, 2500);
		}
		else
		{
			x += number(-100, 100);
			y += number(-100, 100);
		}

		m_chHorse = CHARACTER_MANAGER::instance().SpawnMob(
				(0 == dwVnum) ? GetMyHorseVnum() : dwVnum,
				GetMapIndex(),
				x, y,
				GetZ(), false, (int)(GetRotation()+180), false);

		if (!m_chHorse)
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 328, "");
#endif
			return;
		}

		if (GetHorseHealth() <= 0)
		{
			m_chHorse->SetPosition(POS_DEAD);

			char_event_info* info = AllocEventInfo<char_event_info>();
			info->ch = this;
			m_chHorse->m_pkDeadEvent = event_create(horse_dead_event, info, PASSES_PER_SEC(60));
		}

		m_chHorse->SetLevel(GetHorseLevel());

		const char* pHorseName = CHorseNameManager::instance().GetHorseName(GetPlayerID());

		if ( pHorseName != nullptr && strlen(pHorseName) != 0 )
		{
			m_chHorse->m_stName = pHorseName;
		}
		else
		{
			uint8_t bLang = 0;
			if (GetDesc()) {
				bLang = GetDesc()->GetLanguage(); 
			}
			
			m_chHorse->m_stName = GetName();
			m_chHorse->m_stName += " ";
			m_chHorse->m_stName += m_horseText[bLang];
		}

		if (!m_chHorse->Show(GetMapIndex(), x, y, GetZ()))
		{
			M2_DESTROY_CHARACTER(m_chHorse);
			sys_err("cannot show monster");
			m_chHorse = nullptr;
			return;
		}

		if ((GetHorseHealth() <= 0))
		{
			TPacketGCDead pack;
			pack.header	= HEADER_GC_DEAD;
			pack.vid    = m_chHorse->GetVID();
			PacketAround(&pack, sizeof(pack));
		}

		m_chHorse->SetRider(this);
	}
	else
	{
		if (!m_chHorse)
			return;

		LPCHARACTER chHorse = m_chHorse;

		chHorse->SetRider(nullptr);

		if ((GetHorseHealth() <= 0))
			bFromFar = false;

		if (!bFromFar)
		{
			M2_DESTROY_CHARACTER(chHorse);
		}
		else
		{
			chHorse->SetNowWalking(false);
			float fx, fy;
			chHorse->SetRotation(GetDegreeFromPositionXY(chHorse->GetX(), chHorse->GetY(), GetX(), GetY())+180);
			GetDeltaByDegree(chHorse->GetRotation(), 3500, &fx, &fy);
			chHorse->Goto((int32_t)(chHorse->GetX()+fx), (int32_t) (chHorse->GetY()+fy));
			chHorse->SendMovePacket(FUNC_WAIT, 0, 0, 0, 0);
		}

		m_chHorse = nullptr;
	}

#ifdef DISABLE_CORE_PULSE_RAZOR93
	SyncMountState(AIHelpers::EcsOf(this), GetMountVnum(), GetLastMountTime(), m_bSendHorseLevel, m_bSendHorseHealthGrade, m_bSendHorseStaminaGrade, m_mountPulse);
#else
	SyncMountState(AIHelpers::EcsOf(this), GetMountVnum(), GetLastMountTime(), m_bSendHorseLevel, m_bSendHorseHealthGrade, m_bSendHorseStaminaGrade, 0);
#endif
}

uint32_t CHARACTER::GetMyHorseVnum() const
{
	int delta = 0;

	if (GetGuild())
	{
		++delta;

		if (GetGuild()->GetMasterPID() == GetPlayerID())
			++delta;
	}

	return c_aHorseStat[GetHorseLevel()].iNPCRace + delta;
}

void CHARACTER::HorseDie()
{
	CHorseRider::HorseDie();
	HorseSummon(false);
}

bool CHARACTER::ReviveHorse()
{
	if (CHorseRider::ReviveHorse())
	{
		HorseSummon(false);
		HorseSummon(true);
#ifdef DISABLE_CORE_PULSE_RAZOR93
		SyncMountState(AIHelpers::EcsOf(this), GetMountVnum(), GetLastMountTime(), m_bSendHorseLevel, m_bSendHorseHealthGrade, m_bSendHorseStaminaGrade, m_mountPulse);
#else
		SyncMountState(AIHelpers::EcsOf(this), GetMountVnum(), GetLastMountTime(), m_bSendHorseLevel, m_bSendHorseHealthGrade, m_bSendHorseStaminaGrade, 0);
#endif
		return true;
	}
	return false;
}

void CHARACTER::ClearHorseInfo()
{
	if (!IsHorseRiding())
	{
		ChatPacket(CHAT_TYPE_COMMAND, "hide_horse_state");

		m_bSendHorseLevel = 0;
		m_bSendHorseHealthGrade = 0;
		m_bSendHorseStaminaGrade = 0;
	}

	m_chHorse = nullptr;
#ifdef DISABLE_CORE_PULSE_RAZOR93
	SyncMountState(AIHelpers::EcsOf(this), GetMountVnum(), GetLastMountTime(), m_bSendHorseLevel, m_bSendHorseHealthGrade, m_bSendHorseStaminaGrade, m_mountPulse);
#else
	SyncMountState(AIHelpers::EcsOf(this), GetMountVnum(), GetLastMountTime(), m_bSendHorseLevel, m_bSendHorseHealthGrade, m_bSendHorseStaminaGrade, 0);
#endif
}

void CHARACTER::SendHorseInfo()
{
	if (m_chHorse || IsHorseRiding())
	{
		int iHealthGrade;
		int iStaminaGrade;
		if (GetHorseHealth() == 0)
			iHealthGrade = 0;
		else if (GetHorseHealth() * 10 <= GetHorseMaxHealth() * 3)
			iHealthGrade = 1;
		else if (GetHorseHealth() * 10 <= GetHorseMaxHealth() * 7)
			iHealthGrade = 2;
		else
			iHealthGrade = 3;

		if (GetHorseStamina() * 10 <= GetHorseMaxStamina())
			iStaminaGrade = 0;
		else if (GetHorseStamina() * 10 <= GetHorseMaxStamina() * 3)
			iStaminaGrade = 1;
		else if (GetHorseStamina() * 10 <= GetHorseMaxStamina() * 7)
			iStaminaGrade = 2;
		else
			iStaminaGrade = 3;

		if (m_bSendHorseLevel != GetHorseLevel() ||
				m_bSendHorseHealthGrade != iHealthGrade ||
				m_bSendHorseStaminaGrade != iStaminaGrade)
		{
			ChatPacket(CHAT_TYPE_COMMAND, "horse_state %d %d %d", GetHorseLevel(), iHealthGrade, iStaminaGrade);

			m_bSendHorseLevel = GetHorseLevel();
			m_bSendHorseHealthGrade = iHealthGrade;
			m_bSendHorseStaminaGrade = iStaminaGrade;
#ifdef DISABLE_CORE_PULSE_RAZOR93
			SyncMountState(AIHelpers::EcsOf(this), GetMountVnum(), GetLastMountTime(), m_bSendHorseLevel, m_bSendHorseHealthGrade, m_bSendHorseStaminaGrade, m_mountPulse);
#else
			SyncMountState(AIHelpers::EcsOf(this), GetMountVnum(), GetLastMountTime(), m_bSendHorseLevel, m_bSendHorseHealthGrade, m_bSendHorseStaminaGrade, 0);
#endif
		}
	}
}

bool CHARACTER::CanUseHorseSkill()
{
	if(IsRiding())
	{
		if (GetHorseGrade() == 3)
			return true;
		else
			return false;

		if(GetMountVnum())
		{
			if (GetMountVnum() >= 20209 && GetMountVnum() <= 20212)
				return true;

			if (CMobVnumHelper::IsRamadanBlackHorse(GetMountVnum()))
				return true;
		}
		else
			return false;

	}

	return false;
}

void CHARACTER::SetHorseLevel(int iLevel)
{
	CHorseRider::SetHorseLevel(iLevel);
	SetSkillLevel(SKILL_HORSE, GetHorseLevel());
#ifdef DISABLE_CORE_PULSE_RAZOR93
	SyncMountState(AIHelpers::EcsOf(this), GetMountVnum(), GetLastMountTime(), m_bSendHorseLevel, m_bSendHorseHealthGrade, m_bSendHorseStaminaGrade, m_mountPulse);
#else
	SyncMountState(AIHelpers::EcsOf(this), GetMountVnum(), GetLastMountTime(), m_bSendHorseLevel, m_bSendHorseHealthGrade, m_bSendHorseStaminaGrade, 0);
#endif
}

#ifdef ENABLE_FAKE_SHOP_HEADER
#ifdef DISABLE_CORE_PULSE_RAZOR93
bool CHARACTER::IsNextMountPulse() const
{
	return (m_mountPulse == 0 || (m_mountPulse < thecore_pulse()));
}

void CHARACTER::UpdateMountPulse()
{
	m_mountPulse = thecore_pulse() + THECORE_SECS_TO_PASSES(1);
}
#endif
#endif

uint8_t CHARACTER::GetMountCounter() const
{
	return m_bMountCounter;
}

void CHARACTER::ResetMountCounter()
{
	m_bMountCounter = 0;
}

uint8_t CHARACTER::IncreaseMountCounter()
{
	return ++m_bMountCounter;
}

bool CHARACTER::IsRiding() const
{
	return IsHorseRiding() || GetMountVnum();
}

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
void CHARACTER::MountSummon(LPITEM mountItem)
{
#define MOUNT_SYSTEM_FIX_POLY
#ifdef MOUNT_SYSTEM_FIX_POLY
	if (IsPolymorphed() == true)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 732, "");
#endif
		return;
	}
#endif
	if (GetMapIndex() == 113)
		return;

	if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
		return;

	CMountSystem* mountSystem = GetMountSystem();
	uint32_t mobVnum = 0;

	if (!mountSystem || !mountItem)
		return;

#ifdef __CHANGELOOK_SYSTEM__
	if (mountItem->GetTransmutation())
	{
		const TItemTable* itemTable = ITEM_MANAGER::instance().GetTable(mountItem->GetTransmutation());

		if (itemTable)
			mobVnum = itemTable->alValues[1];
		else
			mobVnum = mountItem->GetValue(1);
	}
	else
		mobVnum = mountItem->GetValue(1);
#else
	if (mountItem->GetValue(1) != 0)
		mobVnum = mountItem->GetValue(1);
#endif

	if (IsHorseRiding())
		StopRiding();

	if (GetHorse())
		HorseSummon(false);

	mountSystem->Summon(mobVnum, mountItem, false);
}

void CHARACTER::MountUnsummon(LPITEM mountItem)
{
	CMountSystem* mountSystem = GetMountSystem();
	uint32_t mobVnum = 0;

	if (!mountSystem || !mountItem)
		return;

#ifdef __CHANGELOOK_SYSTEM__
	if (mountItem->GetTransmutation())
	{
		const TItemTable* itemTable = ITEM_MANAGER::instance().GetTable(mountItem->GetTransmutation());

		if (itemTable)
			mobVnum = itemTable->alValues[1];
		else
			mobVnum = mountItem->GetValue(1);
	}
	else
		mobVnum = mountItem->GetValue(1);
#else
	if (mountItem->GetValue(1) != 0)
		mobVnum = mountItem->GetValue(1);
#endif

	if (GetMountVnum() == mobVnum)
		mountSystem->Unmount(mobVnum);

	mountSystem->Unsummon(mobVnum);
}

void CHARACTER::CheckMount()
{
	CMountSystem* mountSystem = GetMountSystem();
	LPITEM mountItem = GetWear(WEAR_COSTUME_MOUNT);
	uint32_t mobVnum = 0;

	if (!mountSystem || !mountItem)
		return;

#ifdef __CHANGELOOK_SYSTEM__
	if (mountItem->GetTransmutation())
	{
		const TItemTable* itemTable = ITEM_MANAGER::instance().GetTable(mountItem->GetTransmutation());

		if (itemTable)
			mobVnum = itemTable->alValues[1];
		else
			mobVnum = mountItem->GetValue(1);
	}
	else
		mobVnum = mountItem->GetValue(1);
#else
	if (mountItem->GetValue(1) != 0)
		mobVnum = mountItem->GetValue(1);
#endif

	if (mountSystem->CountSummoned() == 0)
	{
		mountSystem->Summon(mobVnum, mountItem, false);
	}
}

bool CHARACTER::IsRidingMount()
{
	return (GetWear(WEAR_COSTUME_MOUNT) || FindAffect(AFFECT_MOUNT));
}
#endif

#ifdef ENABLE_COSTUME_PET
void CHARACTER::UpdatePetSkin() {
	if (!m_petSystem)
		return;

	m_petSystem->UpdatePetSkin();
}

uint32_t CHARACTER::GetPetSkinVnum() {
	LPITEM item = GetWear(WEAR_COSTUME_PET_SKIN);
	return item != nullptr ? item->GetValue(0) : 0;
}
#endif

#ifdef ENABLE_COSTUME_MOUNT
void CHARACTER::UpdateMountSkin() {
	if (!m_mountSystem)
		return;

	m_mountSystem->UpdateMountSkin();

	if (IsRiding()) {
		LPITEM item = GetWear(WEAR_COSTUME_MOUNT);
		if (!item)
			return;

		uint32_t mobVnum = 0;
#ifdef __CHANGELOOK_SYSTEM__
		if (item->GetTransmutation())
		{
			const TItemTable* itemTable = ITEM_MANAGER::instance().GetTable(item->GetTransmutation());
			if (itemTable)
				mobVnum = itemTable->alValues[1];
			else
				mobVnum = item->GetValue(1);
		}
		else
			mobVnum = item->GetValue(1);
#else
		if (item->GetValue(1) != 0)
			mobVnum = item->GetValue(1);
#endif

		m_mountSystem->Unmount(mobVnum);
		m_mountSystem->Mount(mobVnum, item);
	}
}

uint32_t CHARACTER::GetMountSkinVnum() {
	LPITEM item = GetWear(WEAR_COSTUME_MOUNT_SKIN);
	return item != nullptr ? item->GetValue(0) : 0;
}
#endif

void CHARACTER::ComputeMountInventoryBonuses()
{
	std::map<uint8_t, int32_t> mount_bonus_map;

	CMountInventory* mi = GetMountInventory();
	if (!mi)
		return;

	const auto& valid_items = CMountInventoryHelper::GetAllowedItems();
	const int total = mi->GetWidth() * mi->GetSize();

	for (int pos = 0; pos < total; ++pos)
	{
		LPITEM item = mi->Get(pos);
		if (!item)
			continue;

		const uint32_t vnum = item->GetVnum();
		if (!valid_items.contains(vnum))
			continue;

		const TItemTable* proto = item->GetProto();
		if (!proto)
			continue;

		for (const auto& apply : proto->aApplies)
		{
			if (apply.bType == APPLY_NONE || apply.lValue == 0)
				continue;

			if (apply.bType >= MAX_APPLY_NUM)
				continue;

			const uint8_t pointType = aApplyInfo[apply.bType].bPointType;
			if (pointType != POINT_NONE)
				mount_bonus_map[pointType] += apply.lValue;
		}

		const TPlayerItemAttribute* attrs = item->GetAttributes();
		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		{
			const uint8_t bType = attrs[i].bType;
			const int16_t sVal = attrs[i].sValue;

			if (bType == APPLY_NONE || sVal == 0)
				continue;

			if (bType >= MAX_APPLY_NUM)
				continue;

			const uint8_t pointType = aApplyInfo[bType].bPointType;
			if (pointType != POINT_NONE)
				mount_bonus_map[pointType] += sVal;
		}
	}

	for (const auto& it : mount_bonus_map)
		PointChange(it.first, it.second);
}
