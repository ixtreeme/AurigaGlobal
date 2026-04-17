#include "stdafx.h"
#include "utils.h"
#include "config.h"
#include "char.h"
#include "desc.h"
#include "sectree_manager.h"
#include "packet.h"
#include "protocol.h"
#include "log.h"
#include "skill.h"
#include "unique_item.h"
#include "profiler.h"
#include "marriage.h"
#include "item_addon.h"
#include "dev_log.h"
#include "locale_service.h"
#include "item.h"
#include "item_manager.h"
#include "affect.h"
#include "DragonSoul.h"
#include "buff_on_attributes.h"
#include "belt_inventory_helper.h"
#include <common/VnumHelper.h>
#include <common/CommonDefines.h>
#ifdef ENABLE_RUNE_SYSTEM
#include <common/rune_length.h>
#endif
#include "MountInventory.h"

CItem::CItem(uint32_t dwVnum)
	: m_pProto(nullptr), m_dwVnum(dwVnum), m_pOwner(nullptr), m_bWindow(0), m_dwID(0), m_bEquipped(false), m_dwVID(0),
	m_wCell(0),
	m_dwCount(0),
	m_sLockedAttr(0),
	m_ExtraProto(nullptr), m_lFlag(0), m_dwLastOwnerPID(0),
	m_bExchanging(false), m_pkDestroyEvent(nullptr), m_pkExpireEvent(nullptr),

#ifdef ENABLE_SOUL_SYSTEM
	m_pkSoulItemEvent(nullptr),
#endif

	m_pkUniqueExpireEvent(nullptr), m_pkTimerBasedOnWearExpireEvent(nullptr), m_pkRealTimeExpireEvent(nullptr),
	m_pkAccessorySocketExpireEvent(nullptr), m_pkOwnershipEvent(nullptr), m_dwOwnershipPID(0), m_bSkipSave(false),
	m_isLocked(false),
	m_dwMaskVnum(0), m_dwSIGVnum(0)
{
	memset(&m_alSockets, 0, sizeof(m_alSockets));
	memset(&m_aAttr, 0, sizeof(m_aAttr));
}

CItem::~CItem()
{
	Destroy();
}




EVENTFUNC(item_destroy_event)
{
	auto info = dynamic_cast<item_event_info*>(event->info);

	if (info == nullptr)
	{
		sys_err("item_destroy_event> <Factor> Null pointer");
		return 0;
	}

	LPITEM pkItem = info->item;

	if (pkItem->GetOwner())
		sys_err("item_destroy_event: Owner exist. (item %s owner %s)", pkItem->GetName(), pkItem->GetOwner()->GetName());

	pkItem->SetDestroyEvent(nullptr);
	M2_DESTROY_ITEM(pkItem);
	return 0;
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

#ifdef ATTR_LOCK

bool CItem::CheckHumanApply()
{
	bool bHaveHuman = false;
	TItemTable* p = ITEM_MANAGER::instance().GetTable(GetVnum());
	for (int i = 0; i < ITEM_APPLY_MAX_NUM; ++i)
		if (p->aApplies[i].bType == APPLY_ATTBONUS_HUMAN)
			bHaveHuman = true;

	return bHaveHuman;
}

#ifdef ATTR_LOCK

void CItem::AddLockedAttr()
{
	const int iCount = GetAttributeCount();
	if (iCount <= 0)
	{
		SetLockedAttr(-1);
		return;
	}

	SetLockedAttr((short)(rand() % iCount));
}

void CItem::ChangeLockedAttr()
{
	const int iCount = GetAttributeCount();
	if (iCount <= 0)
	{
		SetLockedAttr(-1);
		return;
	}

	if (iCount == 1)
	{
		SetLockedAttr(0);
		return;
	}

	int iRand = 0;
	do
	{
		iRand = rand() % iCount;
	} while (iRand == (int)GetLockedAttr());

	SetLockedAttr((short)iRand);
}

#endif

//void CItem::AddLockedAttr()// komment:Razor93
//{
//	int iCount = GetAttributeCount();
//	int iRand = rand() % (iCount);
//	bool bCheckHuman = CheckHumanApply();
//
//	if (bCheckHuman)
//	{
//		while (GetAttributeType(iRand) == APPLY_ATTBONUS_HUMAN || GetAttributeType(iRand) == APPLY_SKILL_DAMAGE_BONUS || GetAttributeType(iRand) == APPLY_NORMAL_HIT_DAMAGE_BONUS)
//			iRand = rand() % (iCount);
//	}
//	else
//	{
//		while (GetAttributeType(iRand) == APPLY_SKILL_DAMAGE_BONUS || GetAttributeType(iRand) == APPLY_NORMAL_HIT_DAMAGE_BONUS)
//			iRand = rand() % (iCount);
//	}
//
//	SetLockedAttr((short)iRand);
//}
//void CItem::ChangeLockedAttr()
//{
//	int iCount = GetAttributeCount();
//	int iRand = rand() % (iCount);
//	bool bCheckHuman = CheckHumanApply();
//
//	if (bCheckHuman)
//	{
//		while (iRand == (int)GetLockedAttr() || GetAttributeType(iRand) == APPLY_ATTBONUS_HUMAN || GetAttributeType(iRand) == APPLY_SKILL_DAMAGE_BONUS || GetAttributeType(iRand) == APPLY_NORMAL_HIT_DAMAGE_BONUS)
//			iRand = rand() % (iCount);
//	}
//	else
//	{
//		while (iRand == (int)GetLockedAttr() || GetAttributeType(iRand) == APPLY_SKILL_DAMAGE_BONUS || GetAttributeType(iRand) == APPLY_NORMAL_HIT_DAMAGE_BONUS)
//			iRand = rand() % (iCount);
//	}
//
//	SetLockedAttr((short)iRand);
//}
void CItem::RemoveLockedAttr()
{
	SetLockedAttr(-1);
}
void CItem::SetLockedAttr(short sIndex)
{
	m_sLockedAttr = sIndex;
	UpdatePacket();
	Save();
}

#endif









void CItem::SetExchanging(bool bOn)
{
	m_bExchanging = bOn;
}



bool CItem::CreateSocket(uint8_t bSlot, uint8_t bGold)
{
	assert(bSlot < ITEM_SOCKET_MAX_NUM);

	if (m_alSockets[bSlot] != 0)
	{
		sys_err("Item::CreateSocket : socket already exist %s %d", GetName(), bSlot);
		return false;
	}

	if (bGold)
		m_alSockets[bSlot] = 2;
	else
		m_alSockets[bSlot] = 1;

	UpdatePacket();

	Save();
	return true;
}


EVENTFUNC(ownership_event)
{
	auto info = dynamic_cast<item_event_info*>(event->info);

	if (info == nullptr)
	{
		sys_err("ownership_event> <Factor> Null pointer");
		return 0;
	}

	LPITEM pkItem = info->item;

	pkItem->SetOwnershipEvent(nullptr);

	TPacketGCItemOwnership p;

	p.bHeader = HEADER_GC_ITEM_OWNERSHIP;
	p.dwVID = pkItem->GetVID();
	p.szName[0] = '\0';

	pkItem->PacketAround(&p, sizeof(p));
	return 0;
}



void CItem::AlterToSocketItem(int iSocketCount)
{
	if (iSocketCount >= ITEM_SOCKET_MAX_NUM)
	{
		sys_log(0, "Invalid Socket Count %d, set to maximum", ITEM_SOCKET_MAX_NUM);
		iSocketCount = ITEM_SOCKET_MAX_NUM;
	}

	for (int i = 0; i < iSocketCount; ++i)
		SetSocket(i, 1);
}

void CItem::AlterToMagicItem()
{
	if (GetAttributeSetIndex() < 0)
	{
		return;
	}

	int iSecondPct;
	int iThirdPct;

	switch (GetType())
	{
	case ITEM_WEAPON:
	{
		iSecondPct = 20;
		iThirdPct = 5;
	}
	break;

	case ITEM_ARMOR:
	{
		if (GetSubType() == ARMOR_BODY)
		{
			iSecondPct = 10;
			iThirdPct = 2;
		}
		else
		{
			iSecondPct = 10;
			iThirdPct = 1;
		}
	}
	break;
#ifdef ENABLE_ATTR_COSTUMES
	case ITEM_COSTUME:
	{
		uint8_t subtype = GetSubType();
		iSecondPct = subtype == COSTUME_BODY || subtype == COSTUME_HAIR
#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
			|| subtype == COSTUME_WEAPON
#endif
			? 100 : 0;
		iThirdPct = subtype == COSTUME_BODY || subtype == COSTUME_HAIR
#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
			|| subtype == COSTUME_WEAPON
#endif
			? 100 : 0;
	}
	break;
#endif
	default:
	{
		iSecondPct = 0;
		iThirdPct = 0;
	}
	break;
	}

	if (iSecondPct == 0 && iThirdPct == 0)
	{
		return;
	}

	PutAttribute(aiItemMagicAttributePercentHigh);
	if (number(1, 100) <= iSecondPct)
	{
		PutAttribute(aiItemMagicAttributePercentLow);
	}

	if (number(1, 100) <= iThirdPct)
	{
		PutAttribute(aiItemMagicAttributePercentLow);
	}
}

uint32_t CItem::GetRefineFromVnum()
{
	return ITEM_MANAGER::instance().GetRefineFromVnum(GetVnum());
}

int CItem::GetRefineLevel()
{
	const char* name = GetBaseName();
	char* p = const_cast<char*>(strrchr(name, '+'));

	if (!p)
		return 0;

	int	rtn = 0;
	str_to_number(rtn, p + 1);

	const char* locale_name = GetName();
	p = const_cast<char*>(strrchr(locale_name, '+'));

	if (p)
	{
		int	locale_rtn = 0;
		str_to_number(locale_rtn, p + 1);
		if (locale_rtn != rtn)
		{
			sys_err("refine_level_based_on_NAME(%d) is not equal to refine_level_based_on_LOCALE_NAME(%d).", rtn, locale_rtn);
		}
	}

	return rtn;
}

bool CItem::IsPolymorphItem()
{
	return GetType() == ITEM_POLYMORPH;
}

EVENTFUNC(unique_expire_event)
{
	auto info = dynamic_cast<item_event_info*>(event->info);

	if (info == nullptr)
	{
		sys_err("unique_expire_event> <Factor> Null pointer");
		return 0;
	}

	LPITEM pkItem = info->item;

	if (pkItem->GetValue(2) == 0)
	{
		if (pkItem->GetSocket(ITEM_SOCKET_UNIQUE_REMAIN_TIME) <= 1)
		{
			sys_log(0, "UNIQUE_ITEM: expire %s %u", pkItem->GetName(), pkItem->GetID());
			pkItem->SetUniqueExpireEvent(nullptr);
			ITEM_MANAGER::instance().RemoveItem(pkItem, "UNIQUE_EXPIRE");
			return 0;
		}
		else
		{
			pkItem->SetSocket(ITEM_SOCKET_UNIQUE_REMAIN_TIME, pkItem->GetSocket(ITEM_SOCKET_UNIQUE_REMAIN_TIME) - 1);
			return PASSES_PER_SEC(60);
		}
	}
	else
	{
		time_t cur = get_global_time();

		if (pkItem->GetSocket(ITEM_SOCKET_UNIQUE_REMAIN_TIME) <= cur)
		{
			pkItem->SetUniqueExpireEvent(nullptr);
			ITEM_MANAGER::instance().RemoveItem(pkItem, "UNIQUE_EXPIRE");
			return 0;
		}
		else
		{
			if (pkItem->GetSocket(ITEM_SOCKET_UNIQUE_REMAIN_TIME) - cur < 600)
				return PASSES_PER_SEC(pkItem->GetSocket(ITEM_SOCKET_UNIQUE_REMAIN_TIME) - cur);
			else
				return PASSES_PER_SEC(600);
		}
	}
}

EVENTFUNC(timer_based_on_wear_expire_event)
{
	auto info = dynamic_cast<item_event_info*>(event->info);

	if (info == nullptr)
	{
		sys_err("expire_event <Factor> Null pointer");
		return 0;
	}

	LPITEM pkItem = info->item;
	int remain_time = pkItem->GetSocket(ITEM_SOCKET_REMAIN_SEC) - processing_time / passes_per_sec;
#ifdef ENABLE_RUNE_SYSTEM
	if (pkItem->IsRune()) {
		if (remain_time <= 0) {
			pkItem->SetSocket(ITEM_SOCKET_REMAIN_SEC, 0);
			pkItem->DeactivateRune();
			return 0;
		}

		if (int32_t(remain_time / (pkItem->GetValue(0) / 100)) < 50) {
			pkItem->DeactivateRuneBonus();
		}

		if ((pkItem->GetSubType() == RUNE_SLOT7) || (pkItem->GetSocket(1) != 1))
			return PASSES_PER_SEC(MIN(60, remain_time));

		if (pkItem->GetSocket(1) == 1)
			pkItem->ChangeRuneAttr(remain_time);
	}
#endif

	if (remain_time <= 0)
	{
		sys_log(0, "ITEM EXPIRED : expired %s %u", pkItem->GetName(), pkItem->GetID());
		pkItem->SetTimerBasedOnWearExpireEvent(nullptr);
		pkItem->SetSocket(ITEM_SOCKET_REMAIN_SEC, 0);

		if (pkItem->IsDragonSoul())
		{
			DSManager::instance().DeactivateDragonSoul(pkItem);
		}
		else
		{
			ITEM_MANAGER::instance().RemoveItem(pkItem, "TIMER_BASED_ON_WEAR_EXPIRE");
		}
		return 0;
	}

	pkItem->SetSocket(ITEM_SOCKET_REMAIN_SEC, remain_time);
	return PASSES_PER_SEC(MIN(60, remain_time));
}





EVENTFUNC(real_time_expire_event)
{
	auto info = reinterpret_cast<const item_vid_event_info*>(event->info);

	if (nullptr == info)
		return 0;

	const LPITEM item = ITEM_MANAGER::instance().FindByVID(info->item_vid);

	if (nullptr == item)
		return 0;

#ifdef ENABLE_NEW_USE_POTION
	if (info->newpotion) {
		int32_t remainSec = item->GetSocket(0);
		if (remainSec <= 0) {
			if (item->GetSocket(1) == 1) {
				LPCHARACTER pkOwner = item->GetOwner();
				if (pkOwner) {
					if (pkOwner->FindAffect(item->GetValue(0))) {
						pkOwner->RemoveAffect(item->GetValue(0));
					}

#ifdef TEXTS_IMPROVEMENT
					pkOwner->ChatPacketNew(CHAT_TYPE_INFO, 27, "%s", item->GetName());
#endif
				}
			}

			ITEM_MANAGER::instance().RemoveItem(item, "REAL_TIME_EXPIRE");
			return 0;
		}

		if (item->GetSocket(1) != 1) {
			return PASSES_PER_SEC(60);
		}
		else {
			int32_t nextSec = (remainSec - 60) > 0 ? (remainSec - 60) : 0;
			item->SetSocket(0, nextSec);
			if (nextSec <= 0) {
				LPCHARACTER pkOwner = item->GetOwner();
				if (pkOwner) {
					if (pkOwner->FindAffect(item->GetValue(0))) {
						pkOwner->RemoveAffect(item->GetValue(0));
					}

#ifdef TEXTS_IMPROVEMENT
					pkOwner->ChatPacketNew(CHAT_TYPE_INFO, 27, "%s", item->GetName());
#endif
				}

				ITEM_MANAGER::instance().RemoveItem(item, "REAL_TIME_EXPIRE");
				return 0;
			}
			else {
				return PASSES_PER_SEC(nextSec > 60 ? 60 : nextSec);
			}
		}
	}
#endif

	const time_t current = get_global_time();
	if (current > item->GetSocket(0))
	{
		LPCHARACTER pkOwner = item->GetOwner();

		if (pkOwner && pkOwner->GetDesc() && item->GetWindow() == MOUNT_INVENTORY)
		{
			TPacketGCWhisper pack;
			char msg[CHAT_MAX_LEN + 1];

			const int len = snprintf(msg, sizeof(msg), "Mount expired in mountinventory: %s", item->GetName());

			pack.bHeader = HEADER_GC_WHISPER;
			pack.bType = WHISPER_TYPE_SYSTEM;
			pack.wSize = static_cast<uint16_t>(sizeof(TPacketGCWhisper) + len + 1);
			strlcpy(pack.szNameFrom, "[MountInventory]", sizeof(pack.szNameFrom));

			pkOwner->GetDesc()->BufferedPacket(&pack, sizeof(pack));
			pkOwner->GetDesc()->Packet(msg, len + 1);
		}

		if (item->IsNewMountItem()) {
			if (item->GetSocket(2) != 0)
				item->ClearMountAttributeAndAffect();
		}

		ITEM_MANAGER::instance().RemoveItem(item, "REAL_TIME_EXPIRE");
		return 0;
	}

	return PASSES_PER_SEC(1);
}



bool CItem::IsRealTimeItem()
{
	if (!GetProto())
		return false;

	for (auto aLimit : GetProto()->aLimits)
	{
		if (LIMIT_REAL_TIME == aLimit.bType)
			return true;
	}
	return false;
}

bool CItem::IsRealTimeFirstUseItem()
{
	if (GetProto()) {
		for (auto aLimit : GetProto()->aLimits)
		{
			if (LIMIT_REAL_TIME_START_FIRST_USE == aLimit.bType)
				return true;
		}
	}

	return false;
}

bool CItem::IsUnlimitedTimeUnique()
{
	if (GetProto()) {
		for (auto aLimit : GetProto()->aLimits)
		{
			if (LIMIT_UNIQUE_UNLIMITED == aLimit.bType)
				return true;
		}
	}

	return false;
}








void CItem::ApplyAddon(int iAddonType)
{
	CItemAddonManager::instance().ApplyAddonTo(iAddonType, this);
}

bool CItem::IsAccessoryForSocket()
{
	return (m_pProto->bType == ITEM_ARMOR && (m_pProto->bSubType == ARMOR_WRIST || m_pProto->bSubType == ARMOR_NECK || m_pProto->bSubType == ARMOR_EAR)) || (m_pProto->bType == ITEM_BELT);	
}

void CItem::SetAccessorySocketGrade(int iGrade
#ifdef ENABLE_INFINITE_RAFINES
	, bool infinite
#endif
)
{
	SetSocket(0, MINMAX(0, iGrade, GetAccessorySocketMaxGrade()));

	int iDownTime =
#ifdef ENABLE_INFINITE_RAFINES
		infinite == true ? 86410 : aiAccessorySocketDegradeTime[GetAccessorySocketGrade()];
#else
		aiAccessorySocketDegradeTime[GetAccessorySocketGrade()]
#endif
		;

	//if (test_server)
	//	iDownTime /= 60;

	SetAccessorySocketDownGradeTime(iDownTime);
}

void CItem::SetAccessorySocketMaxGrade(int iMaxGrade)
{
	SetSocket(1, MINMAX(0, iMaxGrade, ITEM_ACCESSORY_SOCKET_MAX_NUM));
}

void CItem::SetAccessorySocketDownGradeTime(uint32_t time)
{
	SetSocket(2, time);
}

EVENTFUNC(accessory_socket_expire_event)
{
	item_vid_event_info* info = dynamic_cast<item_vid_event_info*>(event->info);

	if (info == nullptr)
	{
		sys_err("accessory_socket_expire_event> <Factor> Null pointer");
		return 0;
	}

	LPITEM item = ITEM_MANAGER::instance().FindByVID(info->item_vid);
	if (item->GetAccessorySocketDownGradeTime() <= 1)
	{
	degrade:
		item->SetAccessorySocketExpireEvent(nullptr);
		item->AccessorySocketDegrade();
		return 0;
	}
	else
	{
		int iTime = item->GetAccessorySocketDownGradeTime() - 60;

		if (iTime <= 1)
			goto degrade;

		item->SetAccessorySocketDownGradeTime(iTime);

		if (iTime > 60)
			return PASSES_PER_SEC(60);
		else
			return PASSES_PER_SEC(iTime);
	}
}





bool CItem::IsRamadanRing()
{
	if (GetVnum() == UNIQUE_ITEM_RAMADAN_RING)
		return true;
	return false;
}

void CItem::ClearMountAttributeAndAffect()
{
	LPCHARACTER ch = GetOwner();

	ch->RemoveAffect(AFFECT_MOUNT);
	ch->RemoveAffect(AFFECT_MOUNT_BONUS);

	ch->MountVnum(0);

	ch->PointChange(POINT_ST, 0);
	ch->PointChange(POINT_DX, 0);
	ch->PointChange(POINT_HT, 0);
	ch->PointChange(POINT_IQ, 0);
}

// fixme
// ÀÌ°Å Áö±ÝÀº ¾È¾´µ¥... ±Ùµ¥ È¤½Ã³ª ½Í¾î¼­ ³²°ÜµÒ.
// by rtsummit


void CItem::AccessorySocketDegrade()
{
	if (GetAccessorySocketGrade() > 0)
	{
		LPCHARACTER ch = GetOwner();
#ifdef TEXTS_IMPROVEMENT
		if (ch) {
			ch->ChatPacketNew(CHAT_TYPE_INFO, 117, "%s", GetName());
		}
#endif

		ModifyPoints(false);
		SetAccessorySocketGrade(GetAccessorySocketGrade() - 1);
		ModifyPoints(true);

		int iDownTime = aiAccessorySocketDegradeTime[GetAccessorySocketGrade()];

		if (test_server)
			iDownTime /= 60;

		SetAccessorySocketDownGradeTime(iDownTime);

		if (iDownTime)
			StartAccessorySocketExpireEvent();
	}
}

// ring¿¡ itemÀ» ¹ÚÀ» ¼ö ÀÖ´ÂÁö ¿©ºÎ¸¦ Ã¼Å©ÇØ¼­ ¸®ÅÏ
static const bool CanPutIntoRing(LPITEM ring, LPITEM item)
{
	//const uint32_t vnum = item->GetVnum();
	return false;
}

bool CItem::CanPutInto(LPITEM item)
{
	//if (item->GetType() == ITEM_BELT) {
	//	if (GetSubType() == USE_PUT_INTO_BELT_SOCKET && GetValue(0) != 1) {
	//		return true;
	//	}
	//	else {
	//		return false;
	//	}
	//}
	/*else*/ if (item->GetType() == ITEM_RING)
		return CanPutIntoRing(item, this);

	else if (item->GetType() != ITEM_ARMOR)
		return false;

	uint32_t vnum = item->GetVnum();

	if (GetVnum() == 50634) {
		return (vnum >= 14220 && vnum <= 14233) || (vnum >= 16220 && vnum <= 16233) || (vnum >= 17220 && vnum <= 17233) ? true : false;
	}

	if (GetVnum() == 50640) {
		return (vnum >= 14580 && vnum <= 14589) || (vnum >= 15010 && vnum <= 15013) || (vnum >= 16580 && vnum <= 16593) || (vnum >= 17570 && vnum <= 17583) ? true : false;
	}

	if (GetVnum() == 50641) //limites koho aqua
	{
		return (vnum >= 8210 && vnum <= 8223) || (vnum >= 8250 && vnum <= 8263) || (vnum >= 8270 && vnum <= 8283) ? true : false;
	}

	if (GetVnum() == 50645) //limites koho aqua
	{
		return (vnum >= 8780 && vnum <= 8789) || (vnum >= 8760 && vnum <= 8769) || (vnum >= 8790 && vnum <= 8799) ? true : false;//Fagyos aqua itemek
	}


	if (GetVnum() == 50646) //limites koho isteni
	{
		return (vnum >= 8730 && vnum <= 8739) || (vnum >= 8700 && vnum <= 8709) || (vnum >= 8780 && vnum <= 8789) ? true : false;//véres zodiák itemek
	}




	if (GetVnum() == 50643) //limites koho isteni
	{
		return (vnum >= 1740 && vnum <= 1753) || (vnum >= 1780 && vnum <= 1793) || (vnum >= 1800 && vnum <= 1813) ? true : false;
	}
	struct JewelAccessoryInfo
	{
		uint32_t jewel;
		uint32_t wrist;
		uint32_t neck;
		uint32_t ear;
	};
	const static JewelAccessoryInfo infos[] = {
		{ 50634, 14220, 16220, 17220 },
		{ 50635, 14500, 16500, 17500 },
		{ 50636, 14520, 16520, 17520 },
		{ 50637, 14540, 16540, 17540 },
		{ 50638, 14560, 16560, 17560 },
		{ 50639, 14570, 16570, 17570 },
		{ 50641, 8210, 8250, 8270 },
		{ 50645, 8780, 8760, 8790 },
		{ 50643, 1740, 1780, 1800 },
		{ 50646, 8730, 8720, 8730 },
	};

	uint32_t item_type = (item->GetVnum() / 10) * 10;
	for (size_t i = 0; i < sizeof(infos) / sizeof(infos[0]); i++)
	{
		const JewelAccessoryInfo& info = infos[i];
		switch (item->GetSubType())
		{
		case ARMOR_WRIST:
			if (info.wrist == item_type)
			{
				if (info.jewel == GetVnum())
				{
					return true;
				}
				else
				{
					return false;
				}
			}
			break;
		case ARMOR_NECK:
			if (info.neck == item_type)
			{
				if (info.jewel == GetVnum())
				{
					return true;
				}
				else
				{
					return false;
				}
			}
			break;
		case ARMOR_EAR:
			if (info.ear == item_type)
			{
				if (info.jewel == GetVnum())
				{
					return true;
				}
				else
				{
					return false;
				}
			}
			break;
		}
	}
	if (item->GetSubType() == ARMOR_WRIST)
		vnum -= 14000;
	else if (item->GetSubType() == ARMOR_NECK)
		vnum -= 16000;
	else if (item->GetSubType() == ARMOR_EAR)
		vnum -= 17000;
	else
		return false;

	uint32_t type = vnum / 20;

	if (type < 0 || type > 11)
	{
		type = (vnum - 170) / 20;

		if (50623 + type != GetVnum())
			return false;
		else
			return true;
	}
	else if (item->GetVnum() >= 16210 && item->GetVnum() <= 16219)
	{
		if (50625 != GetVnum())
			return false;
		else
			return true;
	}
	else if (item->GetVnum() >= 16230 && item->GetVnum() <= 16239)
	{
		if (50626 != GetVnum())
			return false;
		else
			return true;
	}

	return 50623 + type == GetVnum();
}

#ifdef ENABLE_INFINITE_RAFINES
bool CItem::CanPutInto2(LPITEM item)
{
/*	if (item->GetType() == ITEM_BELT) {
		if (GetSubType() == USE_PUT_INTO_BELT_SOCKET && GetValue(0) == 1) {
			return true;
		}
		else {
			return false;
		}
	}

	else*/ if (item->GetType() == ITEM_RING)
		return CanPutIntoRing(item, this);

	else if (item->GetType() != ITEM_ARMOR)
		return false;

	uint32_t vnum = item->GetVnum();

	if (GetVnum() == 50684) {
		return (vnum >= 14220 && vnum <= 14233) || (vnum >= 16220 && vnum <= 16233) || (vnum >= 17220 && vnum <= 17233) ? true : false;
	}

	if (GetVnum() == 50690) {
		return (vnum >= 14580 && vnum <= 14589) || (vnum >= 15010 && vnum <= 15013) || (vnum >= 16580 && vnum <= 16593) || (vnum >= 17570 && vnum <= 17583) ? true : false;
	}

	if (GetVnum() == 50642) //perma koho aqua
	{
		return (vnum >= 8210 && vnum <= 8223) || (vnum >= 8250 && vnum <= 8263) || (vnum >= 8270 && vnum <= 8283) ? true : false;
	}


	if (GetVnum() == 50644) //perma koho isteni
	{
		return (vnum >= 1740 && vnum <= 1753) || (vnum >= 1780 && vnum <= 1793) || (vnum >= 1800 && vnum <= 1813) ? true : false;
	}

	struct JewelAccessoryInfo
	{
		uint32_t jewel;
		uint32_t wrist;
		uint32_t neck;
		uint32_t ear;
	};
	const static JewelAccessoryInfo infos[] = {
		{ 50684, 14220, 16220, 17220 },
		{ 50685, 14500, 16500, 17500 },
		{ 50686, 14520, 16520, 17520 },
		{ 50687, 14540, 16540, 17540 },
		{ 50688, 14560, 16560, 17560 },
		{ 50689, 14570, 16570, 17570 },
		{ 50642, 8210, 8250, 8270 },
		{ 50644, 1740, 1780, 1800 },
	};

	uint32_t item_type = (item->GetVnum() / 10) * 10;
	for (size_t i = 0; i < sizeof(infos) / sizeof(infos[0]); i++)
	{
		const JewelAccessoryInfo& info = infos[i];
		switch (item->GetSubType())
		{
		case ARMOR_WRIST:
			if (info.wrist == item_type)
			{
				if (info.jewel == GetVnum())
				{
					return true;
				}
				else
				{
					return false;
				}
			}
			break;
		case ARMOR_NECK:
			if (info.neck == item_type)
			{
				if (info.jewel == GetVnum())
				{
					return true;
				}
				else
				{
					return false;
				}
			}
			break;
		case ARMOR_EAR:
			if (info.ear == item_type)
			{
				if (info.jewel == GetVnum())
				{
					return true;
				}
				else
				{
					return false;
				}
			}
			break;
		}
	}
	if (item->GetSubType() == ARMOR_WRIST)
		vnum -= 14000;
	else if (item->GetSubType() == ARMOR_NECK)
		vnum -= 16000;
	else if (item->GetSubType() == ARMOR_EAR)
		vnum -= 17000;
	else
		return false;

	uint32_t type = vnum / 20;

	if (type < 0 || type > 11)
	{
		type = (vnum - 170) / 20;

		if (50673 + type != GetVnum())
			return false;
		else
			return true;
	}
	else if (item->GetVnum() >= 16210 && item->GetVnum() <= 16219)
	{
		if (50675 != GetVnum())
			return false;
		else
			return true;
	}
	else if (item->GetVnum() >= 16230 && item->GetVnum() <= 16239)
	{
		if (50676 != GetVnum())
			return false;
		else
			return true;
	}

	return 50673 + type == GetVnum();
}
#endif

// PC_BANG_ITEM_ADD
// END_PC_BANG_ITEM_ADD

int32_t CItem::FindApplyValue(uint8_t bApplyType)
{
	if (m_pProto == nullptr)
		return 0;

	for (int i = 0; i < ITEM_APPLY_MAX_NUM; ++i)
	{
		if (m_pProto->aApplies[i].bType == bApplyType)
			return m_pProto->aApplies[i].lValue;
	}

	return 0;
}

void CItem::CopySocketTo(LPITEM pItem)
{
	for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
	{
		pItem->m_alSockets[i] = m_alSockets[i];
	}
}

int CItem::GetAccessorySocketGrade()
{
	return MINMAX(0, GetSocket(0), GetAccessorySocketMaxGrade());
}

int CItem::GetAccessorySocketMaxGrade()
{
	return MINMAX(0, GetSocket(1), ITEM_ACCESSORY_SOCKET_MAX_NUM);
}

int CItem::GetAccessorySocketDownGradeTime()
{
#ifdef ENABLE_INFINITE_RAFINES
	return GetSocket(2);
#else
	return MINMAX(0, GetSocket(2), aiAccessorySocketDegradeTime[GetAccessorySocketGrade()]);
#endif
}

void CItem::AttrLog()
{
	const char* pszIP = nullptr;

	if (GetOwner() && GetOwner()->GetDesc())
		pszIP = GetOwner()->GetDesc()->GetHostName();

	for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
	{
		if (m_alSockets[i])
		{
#ifdef ENABLE_NEWSTUFF
			if (g_iDbLogLevel >= LOG_LEVEL_MAX)
#endif
				LogManager::instance().ItemLog(i, m_alSockets[i], 0, GetID(), "INFO_SOCKET", "", pszIP ? pszIP : "", GetOriginalVnum());
		}
	}

	for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
	{
		int	type = m_aAttr[i].bType;
		int value = m_aAttr[i].sValue;

		if (type)
		{
#ifdef ENABLE_NEWSTUFF
			if (g_iDbLogLevel >= LOG_LEVEL_MAX)
#endif
				LogManager::instance().ItemLog(i, type, value, GetID(), "INFO_ATTR", "", pszIP ? pszIP : "", GetOriginalVnum());
		}
	}
}

int CItem::GiveMoreTime_Per(float fPercent)
{
	if (IsDragonSoul())
	{
		uint32_t duration = DSManager::instance().GetDuration(this);
		uint32_t remain_sec = GetSocket(ITEM_SOCKET_REMAIN_SEC);
		uint32_t given_time = fPercent * duration / 100u;
		if (remain_sec == duration)
			return false;
		if ((given_time + remain_sec) >= duration)
		{
			SetSocket(ITEM_SOCKET_REMAIN_SEC, duration);
			return duration - remain_sec;
		}
		else
		{
			SetSocket(ITEM_SOCKET_REMAIN_SEC, given_time + remain_sec);
			return given_time;
		}
	}
	// ¿ì¼± ¿ëÈ¥¼®¿¡ °üÇØ¼­¸¸ ÇÏµµ·Ï ÇÑ´Ù.
	else
		return 0;
}

int CItem::GiveMoreTime_Fix(uint32_t dwTime)
{
	if (IsDragonSoul())
	{
		uint32_t duration = DSManager::instance().GetDuration(this);
		uint32_t remain_sec = GetSocket(ITEM_SOCKET_REMAIN_SEC);
		if (remain_sec == duration)
			return false;
		if ((dwTime + remain_sec) >= duration)
		{
			SetSocket(ITEM_SOCKET_REMAIN_SEC, duration);
			return duration - remain_sec;
		}
		else
		{
			SetSocket(ITEM_SOCKET_REMAIN_SEC, dwTime + remain_sec);
			return dwTime;
		}
	}
	// ¿ì¼± ¿ëÈ¥¼®¿¡ °üÇØ¼­¸¸ ÇÏµµ·Ï ÇÑ´Ù.
	else
		return 0;
}


int	CItem::GetDuration()
{
	if (!GetProto())
		return -1;

	for (int i = 0; i < ITEM_LIMIT_MAX_NUM; i++)
	{
		if (LIMIT_REAL_TIME == GetProto()->aLimits[i].bType)
			return GetProto()->aLimits[i].lValue;
	}

	if (GetProto()->cLimitTimerBasedOnWearIndex >= 0)
	{
		uint8_t cLTBOWI = GetProto()->cLimitTimerBasedOnWearIndex;
		return GetProto()->aLimits[cLTBOWI].lValue;
	}

	return -1;
}


#ifdef ENABLE_EXTRA_INVENTORY
namespace
{
	bool IsExtraEnchantUseSubtype(uint8_t subtype)
	{
		switch (subtype)
		{
		case USE_CHANGE_ATTRIBUTE:
		case USE_ADD_ATTRIBUTE:
		case USE_ADD_ATTRIBUTE2:
		case USE_CHANGE_ATTRIBUTE2:
		case USE_CHANGE_COSTUME_ATTR:
		case USE_RESET_COSTUME_ATTR:
		case USE_CHANGE_ATTRIBUTE_PLUS:
#ifdef ATTR_LOCK
		case USE_ADD_ATTRIBUTE_LOCK:
		case USE_CHANGE_ATTRIBUTE_LOCK:
		case USE_DELETE_ATTRIBUTE_LOCK:
#endif
#ifdef ENABLE_ATTR_COSTUMES
		case USE_CHANGE_ATTR_COSTUME:
		case USE_ADD_ATTR_COSTUME1:
		case USE_ADD_ATTR_COSTUME2:
		case USE_REMOVE_ATTR_COSTUME:
#endif
#ifdef ENABLE_DS_ENCHANT
		case USE_DS_ENCHANT:
		case USE_ENCHANT_STOLE:
#endif
			return true;
		}

		return false;
	}

	bool IsExtraPotionUseSubtype(uint8_t subtype)
	{
		switch (subtype)
		{
		case USE_POTION:
		case USE_POTION_NODELAY:
		case USE_POTION_CONTINUE:
		case USE_ABILITY_UP:
		case USE_AFFECT:
#ifdef ENABLE_NEW_USE_POTION
		case USE_NEW_POTIION:
#endif
			return true;
		}

		return false;
	}
}


#endif

#ifdef ENABLE_SOUL_SYSTEM
EVENTFUNC(soul_item_event)
{
	const item_vid_event_info* pInfo = reinterpret_cast<item_vid_event_info*>(event->info);
	if (!pInfo)
		return 0;

	const LPITEM pItem = ITEM_MANAGER::instance().FindByVID(pInfo->item_vid);
	if (!pItem)
		return 0;

	int iCurrentMinutes = (pItem->GetSocket(2) / 10000);
	int iCurrentStrike = (pItem->GetSocket(2) % 10000);
	int iNextMinutes = iCurrentMinutes + 1;

	if (iNextMinutes >= pItem->GetLimitValue(1))
	{
		if (pItem->GetValue(0) != 1)
		{
			pItem->SetSocket(2, (pItem->GetLimitValue(1) * 10000 + iCurrentStrike)); // just in case
			pItem->SetSoulItemEvent(nullptr);
			return 0;
		}
	}

	pItem->SetSocket(2, (iNextMinutes * 10000 + iCurrentStrike));

	if (test_server)
		return PASSES_PER_SEC(5);

	return PASSES_PER_SEC(60);
}


#endif

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
#endif

#ifdef ENABLE_RUNE_SYSTEM
int32_t CItem::GetRuneAttrType(int c) {
	int32_t v = 0;
	uint8_t bSubType = GetSubType();
	if (bSubType == RUNE_SLOT1)
		v = c == 1 ? aApplyRuneInfo[1][0] : aApplyRuneInfo[0][0];
	else if (bSubType == RUNE_SLOT2)
		v = c == 1 ? aApplyRuneInfo[3][0] : aApplyRuneInfo[2][0];
	else if (bSubType == RUNE_SLOT3)
		v = c == 1 ? aApplyRuneInfo[5][0] : aApplyRuneInfo[4][0];
	else if (bSubType == RUNE_SLOT4)
		v = c == 1 ? aApplyRuneInfo[7][0] : aApplyRuneInfo[6][0];
	else if (bSubType == RUNE_SLOT5)
		v = c == 1 ? aApplyRuneInfo[9][0] : aApplyRuneInfo[8][0];
	else if (bSubType == RUNE_SLOT6)
		v = c == 1 ? aApplyRuneInfo[11][0] : aApplyRuneInfo[10][0];
	else if (bSubType == RUNE_SLOT7)
		v = c == 1 ? aApplyRuneInfo[13][0] : aApplyRuneInfo[12][0];

	return v;
}

int32_t CItem::GetRuneAttrValue(int c, int32_t lTime) {
	int32_t v = 0;
	int32_t t = 1;
	int32_t lMaxTime = GetValue(0);
	int32_t lOnePercent = lMaxTime / 100;
	int32_t lRemainPercent = lTime / lOnePercent;
	if (lRemainPercent >= 81)
		t = 7;
	else if (lRemainPercent >= 61)
		t = 6;
	else if (lRemainPercent >= 41)
		t = 5;
	else if (lRemainPercent >= 21)
		t = 4;
	else if (lRemainPercent >= 11)
		t = 3;
	else if (lRemainPercent >= 6)
		t = 2;
	else if (lRemainPercent >= 0)
		t = 1;

	uint8_t bSubType = GetSubType();
	if (bSubType == RUNE_SLOT1)
		v = c == 1 ? aApplyRuneInfo[1][t] : aApplyRuneInfo[0][t];
	else if (bSubType == RUNE_SLOT2)
		v = c == 1 ? aApplyRuneInfo[3][t] : aApplyRuneInfo[2][t];
	else if (bSubType == RUNE_SLOT3)
		v = c == 1 ? aApplyRuneInfo[5][t] : aApplyRuneInfo[4][t];
	else if (bSubType == RUNE_SLOT4)
		v = c == 1 ? aApplyRuneInfo[7][t] : aApplyRuneInfo[6][t];
	else if (bSubType == RUNE_SLOT5)
		v = c == 1 ? aApplyRuneInfo[9][t] : aApplyRuneInfo[8][t];
	else if (bSubType == RUNE_SLOT6)
		v = c == 1 ? aApplyRuneInfo[11][t] : aApplyRuneInfo[10][t];
	else if (bSubType == RUNE_SLOT7)
		v = c == 1 ? aApplyRuneInfo[13][t] : aApplyRuneInfo[12][t];

	return v;
}

void CItem::InitializeRune() {
	if ((GetType() == ITEM_USE) && (GetSubType() == USE_RUNE_PERC_CHARGE)) {
		SetSocket(0, GetValue(0));
		UpdatePacket();
		return;
	}

	if (!IsRune())
		return;

	int32_t lTime = 0, lAttr = 0, lValue = 0;
	for (int i = 0; i < RUNE_ATTR_EACH; ++i) {
		lTime = GetSocket(0);
		lAttr = GetRuneAttrType(i);
		lValue = GetRuneAttrValue(i, lTime);
		if ((lAttr > 0) && (lValue > 0)) {
			SetForceAttribute(i, lAttr, lValue);
		}
	}
}

void CItem::ChangeRuneAttr(int32_t lTime) {
	int32_t lValue = GetRuneAttrValue(0, lTime);
	bool bChange = lValue != GetAttributeValue(0) ? true : false;
	if (!bChange)
		return;

	bool isActive = GetSocket(1) == 1 ? true : false;
	if (isActive)
		ModifyPoints(false);

	for (int i = 0; i < RUNE_ATTR_EACH; ++i) {
		lValue = GetRuneAttrValue(i, lTime);
		SetForceAttribute(i, GetAttributeType(i), lValue);
	}

	if (isActive)
		ModifyPoints(true);

	UpdatePacket();
}

void CItem::ActivateRuneBonus() {
	if (!m_pOwner)
		return;

	LPITEM pkItem1 = m_pOwner->GetWear(WEAR_RUNE7);
	if (!pkItem1)
		return;

	if (pkItem1->GetSocket(1) == 1)
		return;

	bool bCan = true;
	int iMaxSubTypes = RUNE_SUBTYPES - 1;
	LPITEM pkItem2 = nullptr;
	for (int i = 0; i < iMaxSubTypes; i++) {
		pkItem2 = m_pOwner->GetWear(WEAR_RUNE1 + i);
		if (pkItem2) {
			if (pkItem2->GetSocket(1) != 1) {
				bCan = false;
				break;
			}
			else {
				if (int32_t(pkItem2->GetSocket(0) / (pkItem2->GetValue(0) / 100)) < 50) {
					bCan = false;
					break;
				}
			}
		}
		else {
			bCan = false;
			break;
		}
	}

	if (!bCan) {
		if (m_pOwner->FindAffect(AFFECT_RUNE2))
			m_pOwner->RemoveAffect(AFFECT_RUNE2);

		if (!m_pOwner->FindAffect(AFFECT_RUNE1))
			m_pOwner->AddAffect(AFFECT_RUNE1, APPLY_NONE, 0, 0, INFINITE_AFFECT_DURATION, false, false);

		return;
	}
	else {
		if (m_pOwner->FindAffect(AFFECT_RUNE1))
			m_pOwner->RemoveAffect(AFFECT_RUNE1);

		if (!m_pOwner->FindAffect(AFFECT_RUNE2))
			m_pOwner->AddAffect(AFFECT_RUNE2, APPLY_NONE, 0, 0, INFINITE_AFFECT_DURATION, false, false);
	}

	pkItem1->SetSocket(1, 1);
	pkItem1->ModifyPoints(true);
	pkItem1->UpdatePacket();
#ifdef TEXTS_IMPROVEMENT
	m_pOwner->ChatPacketNew(CHAT_TYPE_INFO, 31, "%s", pkItem1->GetName());
#endif
}

void CItem::DeactivateRuneBonus() {
	if (!m_pOwner)
		return;

	LPITEM pkItem1 = m_pOwner->GetWear(WEAR_RUNE7);
	if (!pkItem1)
		return;

	if (pkItem1->GetSocket(1) != 1)
		return;

	if (m_pOwner->FindAffect(AFFECT_RUNE2))
		m_pOwner->RemoveAffect(AFFECT_RUNE2);

	pkItem1->SetSocket(1, 0);
	pkItem1->ModifyPoints(false);
	pkItem1->UpdatePacket();
#ifdef TEXTS_IMPROVEMENT
	m_pOwner->ChatPacketNew(CHAT_TYPE_INFO, 901, "%s", pkItem1->GetName());
#endif
}

void CItem::DeactivateRuneBonusRefresh() {
	int iMaxSubTypes = RUNE_SUBTYPES - 1;
	bool bAdd = false;
	LPITEM pkItem2 = nullptr;
	if (!m_pOwner->FindAffect(AFFECT_RUNE1)) {
		for (int i = 0; i < iMaxSubTypes; i++) {
			pkItem2 = m_pOwner->GetWear(WEAR_RUNE1 + i);
			if (pkItem2) {
				if (pkItem2->GetSocket(1) != 0) {
					bAdd = true;
					break;
				}
			}
			else {
				bAdd = true;
				break;
			}
		}

		if (bAdd)
			m_pOwner->AddAffect(AFFECT_RUNE1, APPLY_NONE, 0, 0, INFINITE_AFFECT_DURATION, false, false);
	}
	else {
		for (int i = 0; i < iMaxSubTypes; i++) {
			pkItem2 = m_pOwner->GetWear(WEAR_RUNE1 + i);
			if (pkItem2) {
				if (pkItem2->GetSocket(1) != 0) {
					bAdd = true;
					break;
				}
			}
			else {
				bAdd = true;
				break;
			}
		}

		if (!bAdd)
			m_pOwner->RemoveAffect(AFFECT_RUNE1);
	}
}

void CItem::ActivateRune() {
	if (!IsRune())
		return;

	if (GetSocket(1) == 1)
		return;

	if (GetSocket(ITEM_SOCKET_REMAIN_SEC) <= 0) {
#ifdef TEXTS_IMPROVEMENT
		if (m_pOwner) {
			m_pOwner->ChatPacketNew(CHAT_TYPE_INFO, 30, "%s", GetName());
		}
#endif
		return;
	}

	SetSocket(1, 1);
	ModifyPoints(true);
	UpdatePacket();
#ifdef TEXTS_IMPROVEMENT
	if (m_pOwner) {
		m_pOwner->ChatPacketNew(CHAT_TYPE_INFO, 31, "%s", GetName());
	}
#endif

	ActivateRuneBonus();
}

void CItem::DeactivateRune() {
	if (!IsRune())
		return;

	if (GetSocket(1) == 0)
		return;

	DeactivateRuneBonus();

	SetSocket(1, 0);
	ModifyPoints(false);
	UpdatePacket();
	DeactivateRuneBonusRefresh();
#ifdef TEXTS_IMPROVEMENT
	if (m_pOwner) {
		m_pOwner->ChatPacketNew(CHAT_TYPE_INFO, 32, "%s", GetName());
	}
#endif
}
#endif
//#ifdef ENABLE_MULTI_NAMES
//
// 
//// Ha 0..N-1 a valid index, akkor N legyen a locale count.
//static constexpr uint8_t ITEM_LOCALE_MAX = 10; //   10 nyelv  
//static constexpr uint8_t ITEM_LOCALE_FALLBACK = 1;
//
//static inline uint8_t ClampItemLang(int lang)
//{
//	// 0 azt jelenti: "auto" (desc alapján) -> ezt nem itt clampeljük
//	if (lang <= 0)
//		return 0;
//
//	// Ha biztosan tudod a maxot, clamp:
//	if (lang >= ITEM_LOCALE_MAX)
//		return ITEM_LOCALE_FALLBACK;
//
//	return (uint8_t)lang;
//}
//
//
//#endif

#ifdef ENABLE_MULTI_NAMES
#endif
