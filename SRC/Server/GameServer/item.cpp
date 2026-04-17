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













#ifdef ATTR_LOCK

#ifdef ATTR_LOCK

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
#endif









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





// fixme
// ÀÌ°Å Áö±ÝÀº ¾È¾´µ¥... ±Ùµ¥ È¤½Ã³ª ½Í¾î¼­ ³²°ÜµÒ.
// by rtsummit




// ring¿¡ itemÀ» ¹ÚÀ» ¼ö ÀÖ´ÂÁö ¿©ºÎ¸¦ Ã¼Å©ÇØ¼­ ¸®ÅÏ
#ifdef ENABLE_INFINITE_RAFINES
#endif

// PC_BANG_ITEM_ADD
// END_PC_BANG_ITEM_ADD

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
