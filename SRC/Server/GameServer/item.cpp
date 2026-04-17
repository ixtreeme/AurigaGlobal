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

void CItem::Initialize()
{
	CEntity::Initialize(ENTITY_ITEM);

	m_bWindow = RESERVED_WINDOW;
	m_pOwner = nullptr;
	m_dwID = 0;
	m_bEquipped = false;
	m_dwVID = m_wCell = m_dwCount = m_lFlag = 0;
	m_pProto = nullptr;
	m_bExchanging = false;
#ifdef ENABLE_SOUL_SYSTEM
	m_pkSoulItemEvent = nullptr;
#endif
	m_pkUniqueExpireEvent = nullptr;
	memset(&m_alSockets, 0, sizeof(m_alSockets));
	memset(&m_aAttr, 0, sizeof(m_aAttr));
#ifdef ATTR_LOCK
	m_sLockedAttr = -1;
#endif

	m_pkDestroyEvent = nullptr;
	m_pkOwnershipEvent = nullptr;
	m_dwOwnershipPID = 0;

	m_pkTimerBasedOnWearExpireEvent = nullptr;
	m_pkRealTimeExpireEvent = nullptr;

	m_pkAccessorySocketExpireEvent = nullptr;

	m_bSkipSave = false;
	m_dwLastOwnerPID = 0;
}

void CItem::Destroy()
{
	event_cancel(&m_pkDestroyEvent);
	event_cancel(&m_pkOwnershipEvent);
	event_cancel(&m_pkUniqueExpireEvent);

#ifdef ENABLE_SOUL_SYSTEM
	event_cancel(&m_pkSoulItemEvent);
#endif

	event_cancel(&m_pkTimerBasedOnWearExpireEvent);
	event_cancel(&m_pkRealTimeExpireEvent);
	event_cancel(&m_pkAccessorySocketExpireEvent);

	CEntity::Destroy();

	if (GetSectree())
		GetSectree()->RemoveEntity(this);
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

void CItem::SetDestroyEvent(LPEVENT pkEvent)
{
	m_pkDestroyEvent = pkEvent;
}

void CItem::StartDestroyEvent(int iSec)
{
	if (m_pkDestroyEvent)
		return;

	item_event_info* info = AllocEventInfo<item_event_info>();
	info->item = this;

	SetDestroyEvent(event_create(item_destroy_event, info, PASSES_PER_SEC(iSec)));
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

void CItem::SetProto(const TItemTable* table)
{
	assert(table != NULL);
	m_pProto = table;
	SetFlag(m_pProto->dwFlags);
}

#ifdef ENABLE_ITEM_EXTRA_PROTO
void CItem::SetExtraProto(TItemExtraProto* Proto)
{
	m_ExtraProto = Proto;
}

TItemExtraProto* CItem::GetExtraProto()
{
	return m_ExtraProto;
}
#endif

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

LPITEM CItem::RemoveFromCharacter()
{
	if (!m_pOwner)
	{
		sys_err("Item::RemoveFromCharacter owner null");
		return (this);
	}

	LPCHARACTER pOwner = m_pOwner;

	if (m_bEquipped)
	{
		Unequip();
		SetWindow(RESERVED_WINDOW);
		Save();
		return (this);
	}
	else
	{
		if (GetWindow() == MOUNT_INVENTORY)
		{
			if (CMountInventory* mi = pOwner->GetMountInventory())
				mi->RemoveByItem(this);

			m_pOwner = nullptr;
			m_wCell = 0;
			SetWindow(RESERVED_WINDOW);
			Save();
			return (this);
		}

		if (GetWindow() != SAFEBOX && GetWindow() != MALL)
		{
			if (IsDragonSoul())
			{
				if (m_wCell >= DRAGON_SOUL_INVENTORY_MAX_NUM)
					sys_err("CItem::RemoveFromCharacter: pos >= DRAGON_SOUL_INVENTORY_MAX_NUM");
				else
					pOwner->SetItem(TItemPos(m_bWindow, m_wCell), nullptr);
			}
#ifdef ENABLE_EXTRA_INVENTORY
			else if (IsExtraItem())
			{
				if (m_wCell >= EXTRA_INVENTORY_MAX_NUM)
					sys_err("CItem::RemoveFromCharacter: pos >= EXTRA_INVENTORY_MAX_NUM");
				else
					pOwner->SetItem(TItemPos(m_bWindow, m_wCell), nullptr);
			}
#endif
#ifdef ENABLE_SWITCHBOT
			else if (m_bWindow == SWITCHBOT)
			{
				if (m_wCell >= SWITCHBOT_SLOT_COUNT)
				{
					sys_err("CItem::RemoveFromCharacter: pos >= SWITCHBOT_SLOT_COUNT");
				}
				else
				{
					pOwner->SetItem(TItemPos(SWITCHBOT, m_wCell), nullptr);
				}
			}
#endif
			else
			{
				TItemPos cell(INVENTORY, m_wCell);

				if (false == cell.IsDefaultInventoryPosition() && false == cell.IsBeltInventoryPosition())
					sys_err("CItem::RemoveFromCharacter: Invalid Item Position");
				else
					pOwner->SetItem(cell, nullptr);
			}
		}

		m_pOwner = nullptr;
		m_wCell = 0;

		SetWindow(RESERVED_WINDOW);
		Save();
		return (this);
	}
}

#ifdef __HIGHLIGHT_SYSTEM__
bool CItem::AddToCharacter(LPCHARACTER ch, TItemPos Cell, bool isHighLight)
#else
bool CItem::AddToCharacter(LPCHARACTER ch, TItemPos Cell)
#endif
{
	assert(GetSectree() == NULL);
	assert(m_pOwner == NULL);
	uint16_t pos = Cell.cell;
	uint8_t window_type = Cell.window_type;

	if (INVENTORY == window_type)
	{
#ifdef ENABLE_RUNE_SYSTEM
		if ((IsRune()) && (ch)) {
			int iFindCell = FindEquipCell(ch);
			LPITEM pkItem = ch->GetWear(iFindCell);
			if (pkItem) {
#ifdef TEXTS_IMPROVEMENT
				ch->ChatPacketNew(CHAT_TYPE_INFO, 35, "%s", GetName());
#endif
				M2_DESTROY_ITEM(this);
				return false;
			}
			else {
				this->EquipTo(ch, iFindCell);
				if (ch->GetDesc())
					m_dwLastOwnerPID = ch->GetPlayerID();

				event_cancel(&m_pkDestroyEvent);

				ch->SetItem(TItemPos(EQUIPMENT, iFindCell), this);
				m_pOwner = ch;
				Save();
				return true;
			}
		}
#endif
#ifdef ENABLE_MOUNT_INVENTORY_FIX_RAZOR93_egyenlore_kikapcsolva

		if (pos >= INVENTORY_MAX_NUM && BELT_INVENTORY_SLOT_START > pos)
#else
		if (m_wCell >= INVENTORY_MAX_NUM && BELT_INVENTORY_SLOT_START > m_wCell)
#endif
		{
			sys_err("CItem::AddToCharacter: cell overflow: %s to %s cell %d", m_pProto->szName, ch->GetName(), m_wCell);
			return false;
		}
	}
	else if (DRAGON_SOUL_INVENTORY == window_type)
	{
		if (m_wCell >= DRAGON_SOUL_INVENTORY_MAX_NUM)
		{
			sys_err("CItem::AddToCharacter: cell overflow: %s to %s cell %d", m_pProto->szName, ch->GetName(), m_wCell);
			return false;
		}
	}
#ifdef ENABLE_EXTRA_INVENTORY
	else if (window_type == EXTRA_INVENTORY)
	{
		if (m_wCell >= EXTRA_INVENTORY_MAX_NUM)
		{
			sys_err("CItem::AddToCharacter: EXTRA cell overflow: %s to %s cell %d", m_pProto->szName, ch->GetName(), m_wCell);
			return false;
		}
	}
#endif
#ifdef ENABLE_SWITCHBOT
	else if (SWITCHBOT == window_type)
	{
		if (m_wCell >= SWITCHBOT_SLOT_COUNT)
		{
			sys_err("CItem::AddToCharacter:switchbot cell overflow: %s to %s cell %d", m_pProto->szName, ch->GetName(), m_wCell);
			return false;
		}
	}
#endif
	if (ch->GetDesc())
		m_dwLastOwnerPID = ch->GetPlayerID();


#ifdef ENABLE_ACCE_SYSTEM
	if ((GetType() == ITEM_COSTUME) && (GetSubType() == COSTUME_ACCE) && (GetSocket(ACCE_ABSORPTION_SOCKET) == 0))
	{
		int32_t lVal = GetValue(ACCE_GRADE_VALUE_FIELD);
		switch (lVal)
		{
		case 2:
		{
			lVal = ACCE_GRADE_2_ABS;
		}
		break;
		case 3:
		{
			lVal = ACCE_GRADE_3_ABS;
		}
		break;
		case 4:
		{
			lVal = number(ACCE_GRADE_4_ABS_MIN, ACCE_GRADE_4_ABS_MAX_COMB);
		}
		break;
		default:
		{
			lVal = ACCE_GRADE_1_ABS;
		}
		break;
		}

		SetSocket(ACCE_ABSORPTION_SOCKET, lVal);
	}
#endif


	event_cancel(&m_pkDestroyEvent);

#ifdef __HIGHLIGHT_SYSTEM__
	ch->SetItem(TItemPos(window_type, pos), this, isHighLight);
#else
	ch->SetItem(TItemPos(window_type, pos), this);
#endif
	m_pOwner = ch;

	Save();
	return true;
}

LPITEM CItem::RemoveFromGround()
{
	if (GetSectree())
	{
		SetOwnership(nullptr);

		GetSectree()->RemoveEntity(this);

		ViewCleanup();

		Save();
	}

	return (this);
}

bool CItem::AddToGround(int32_t lMapIndex, const PIXEL_POSITION& pos, bool skipOwnerCheck)
{
	if (0 == lMapIndex)
	{
		sys_err("wrong map index argument: %d", lMapIndex);
		return false;
	}

	if (GetSectree())
	{
		sys_err("sectree already assigned");
		return false;
	}

	if (!skipOwnerCheck && m_pOwner)
	{
		sys_err("owner pointer not null");
		return false;
	}

	LPSECTREE tree = SECTREE_MANAGER::instance().Get(lMapIndex, pos.x, pos.y);

	if (!tree)
	{
		sys_err("cannot find sectree by %dx%d", pos.x, pos.y);
		return false;
	}

	//tree->Touch();

	SetWindow(GROUND);
	SetXYZ(pos.x, pos.y, pos.z);
	tree->InsertEntity(this);
	UpdateSectree();
	Save();
	return true;
}

bool CItem::DistanceValid(LPCHARACTER ch)
{
	if (!GetSectree())
		return false;

	int iDist = DISTANCE_APPROX(GetX() - ch->GetX(), GetY() - ch->GetY());
	if (iDist > 2400)
		return false;

	return true;
}

bool CItem::CanUsedBy(LPCHARACTER ch)
{
	// Anti flag check
	switch (ch->GetJob())
	{
	case JOB_WARRIOR:
		if (GetAntiFlag() & ITEM_ANTIFLAG_WARRIOR)
			return false;
		break;

	case JOB_ASSASSIN:
		if (GetAntiFlag() & ITEM_ANTIFLAG_ASSASSIN)
			return false;
		break;

	case JOB_SHAMAN:
		if (GetAntiFlag() & ITEM_ANTIFLAG_SHAMAN)
			return false;
		break;

	case JOB_SURA:
		if (GetAntiFlag() & ITEM_ANTIFLAG_SURA)
			return false;
		break;
#ifdef ENABLE_WOLFMAN_CHARACTER
	case JOB_WOLFMAN:
		if (GetAntiFlag() & ITEM_ANTIFLAG_WOLFMAN)
			return false;
		break;
#endif
	}

	return true;
}

int CItem::FindEquipCell(LPCHARACTER ch, int iCandidateCell)
{
	if ((0 == GetWearFlag() || ITEM_TOTEM == GetType()) && ITEM_COSTUME != GetType() && ITEM_DS != GetType() && ITEM_SPECIAL_DS != GetType() && ITEM_RING != GetType() && ITEM_BELT != GetType())
		return -1;

	if (GetType() == ITEM_DS || GetType() == ITEM_SPECIAL_DS)
	{
		if (iCandidateCell < 0)
		{
			return WEAR_MAX_NUM + GetSubType();
		}
		else
		{
			for (int i = 0; i < DRAGON_SOUL_DECK_MAX_NUM; i++)
			{
				if (WEAR_MAX_NUM + i * DS_SLOT_MAX + GetSubType() == iCandidateCell)
				{
					return iCandidateCell;
				}
			}
			return -1;
		}
	}
	else if (GetType() == ITEM_COSTUME)
	{
		if (GetSubType() == COSTUME_BODY)
			return WEAR_COSTUME_BODY;
		else if (GetSubType() == COSTUME_HAIR)
			return WEAR_COSTUME_HAIR;
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
		else if (GetSubType() == COSTUME_MOUNT)
			return WEAR_COSTUME_MOUNT;
#endif
#ifdef ENABLE_ACCE_SYSTEM
		else if (GetSubType() == COSTUME_ACCE)
			return WEAR_COSTUME_ACCE_SLOT;
#endif
#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
		else if (GetSubType() == COSTUME_WEAPON)
			return WEAR_COSTUME_WEAPON;
#endif
#ifdef ENABLE_STOLE_COSTUME
		else if (GetSubType() == COSTUME_STOLE)
			return WEAR_COSTUME_ACCE;
#endif
#ifdef ENABLE_COSTUME_PET
		else if (GetSubType() == COSTUME_PET_SKIN)
			return WEAR_COSTUME_PET_SKIN;
#endif
#ifdef ENABLE_COSTUME_MOUNT
		else if (GetSubType() == COSTUME_MOUNT_SKIN)
			return WEAR_COSTUME_MOUNT_SKIN;
#endif
#ifdef ENABLE_COSTUME_EFFECT
		else if (GetSubType() == COSTUME_EFFECT_BODY)
			return WEAR_COSTUME_EFFECT_BODY;
		else if (GetSubType() == COSTUME_EFFECT_WEAPON)
			return WEAR_COSTUME_EFFECT_WEAPON;
#endif
#ifdef ENABLE_RUNE_SYSTEM
		else if (GetSubType() == RUNE_SLOT1)
			return WEAR_RUNE1;
		else if (GetSubType() == RUNE_SLOT2)
			return WEAR_RUNE2;
		else if (GetSubType() == RUNE_SLOT3)
			return WEAR_RUNE3;
		else if (GetSubType() == RUNE_SLOT4)
			return WEAR_RUNE4;
		else if (GetSubType() == RUNE_SLOT5)
			return WEAR_RUNE5;
		else if (GetSubType() == RUNE_SLOT6)
			return WEAR_RUNE6;
		else if (GetSubType() == RUNE_SLOT7)
			return WEAR_RUNE7;
#endif
	}
#if !defined(ENABLE_MOUNT_COSTUME_SYSTEM) && !defined(ENABLE_ACCE_SYSTEM)
	else if (GetType() == ITEM_RING)
	{
		if (ch->GetWear(WEAR_RING1))
			return WEAR_RING2;
		else
			return WEAR_RING1;
	}
#endif
	else if (GetType() == ITEM_BELT)
		return WEAR_BELT;
	else if (GetWearFlag() & WEARABLE_BODY)
		return WEAR_BODY;
	else if (GetWearFlag() & WEARABLE_HEAD)
		return WEAR_HEAD;
	else if (GetWearFlag() & WEARABLE_FOOTS)
		return WEAR_FOOTS;
	else if (GetWearFlag() & WEARABLE_WRIST)
		return WEAR_WRIST;
	else if (GetWearFlag() & WEARABLE_WEAPON)
		return WEAR_WEAPON;
	else if (GetWearFlag() & WEARABLE_SHIELD)
		return WEAR_SHIELD;
	else if (GetWearFlag() & WEARABLE_NECK)
		return WEAR_NECK;
	else if (GetWearFlag() & WEARABLE_EAR)
		return WEAR_EAR;
	else if (GetWearFlag() & WEARABLE_ARROW)
		return WEAR_ARROW;
	else if (GetWearFlag() & WEARABLE_UNIQUE)
	{
#ifdef ENABLE_NEW_UNIQUE_WEAR_LIMITED
		if (GetSubType() == UNIQUE_PVM || GetSubType() == UNIQUE_PVP || GetSubType() == UNIQUE_NONE)
		{
			const int iSlot1 = WEAR_UNIQUE1;
			const int iSlot2 = WEAR_UNIQUE2;

			// ha konkrét unique slotra húztad, azt engedje
			if (iCandidateCell == iSlot1 || iCandidateCell == iSlot2)
				return iCandidateCell;

			// automatikus felszerelés: első szabad unique slot
			if (!ch->GetWear(iSlot1))
				return iSlot1;

			if (!ch->GetWear(iSlot2))
				return iSlot2;

			// ha mindkettő foglalt
			return -1;
		}
		else
		{
			return -1;
		}
#else
		if (ch->GetWear(WEAR_UNIQUE1))
			return WEAR_UNIQUE2;
		else
			return WEAR_UNIQUE1;
#endif
	}
#ifdef ENABLE_PENDANT
	else if (GetSubType() == ARMOR_PENDANT || GetWearFlag() & WEARABLE_PENDANT)
		return WEAR_PENDANT;
#endif

	else if (GetWearFlag() & WEARABLE_ABILITY)
	{
		if (!ch->GetWear(WEAR_ABILITY1))
		{
			return WEAR_ABILITY1;
		}
		else if (!ch->GetWear(WEAR_ABILITY2))
		{
			return WEAR_ABILITY2;
		}
		else if (!ch->GetWear(WEAR_ABILITY3))
		{
			return WEAR_ABILITY3;
		}
		else if (!ch->GetWear(WEAR_ABILITY4))
		{
			return WEAR_ABILITY4;
		}
		else if (!ch->GetWear(WEAR_ABILITY5))
		{
			return WEAR_ABILITY5;
		}
		else if (!ch->GetWear(WEAR_ABILITY6))
		{
			return WEAR_ABILITY6;
		}
		else if (!ch->GetWear(WEAR_ABILITY7))
		{
			return WEAR_ABILITY7;
		}
#ifndef ENABLE_STOLE_REAL
		else if (!ch->GetWear(WEAR_ABILITY8))
		{
			return WEAR_ABILITY8;
		}
#endif
		else
		{
			return -1;
		}
	}
	return -1;
}

void CItem::ModifyPoints(bool bAdd)
{
#ifdef ENABLE_BUG_FIXES
	if (!m_pOwner) {
		return;
	}
#endif

	int accessoryGrade;

	if (false == IsAccessoryForSocket())
	{
		if (m_pProto->bType == ITEM_WEAPON || m_pProto->bType == ITEM_ARMOR)
		{
			for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
			{
				uint32_t dwVnum;

				if ((dwVnum = GetSocket(i)) <= 2)
					continue;

				TItemTable* p = ITEM_MANAGER::instance().GetTable(dwVnum);

				if (!p)
				{
					sys_err("cannot find table by vnum %u", dwVnum);
					continue;
				}

				if (ITEM_METIN == p->bType)
				{
					for (auto& aApplie : p->aApplies)
					{
						if (aApplie.bType == APPLY_NONE)
							continue;

						if (aApplie.bType == APPLY_SKILL)
							m_pOwner->ApplyPoint(aApplie.bType, bAdd ? aApplie.lValue : aApplie.lValue ^ 0x00800000);
						else
							m_pOwner->ApplyPoint(aApplie.bType, bAdd ? aApplie.lValue : -aApplie.lValue);
					}
				}
			}
		}

		accessoryGrade = 0;
	}
	else
	{
		accessoryGrade = MIN(GetAccessorySocketGrade(), ITEM_ACCESSORY_SOCKET_MAX_NUM);
	}


#ifdef ENABLE_ACCE_SYSTEM
	if ((GetType() == ITEM_COSTUME) && (GetSubType() == COSTUME_ACCE) && (GetSocket(ACCE_ABSORBED_SOCKET)))
	{
		TItemTable* pkItemAbsorbed = ITEM_MANAGER::instance().GetTable(GetSocket(ACCE_ABSORBED_SOCKET));
		if (pkItemAbsorbed)
		{
			/* 			if ((pkItemAbsorbed->bType == ITEM_ARMOR) && (pkItemAbsorbed->bSubType == ARMOR_BODY))
						{
							int32_t lDefGrade = pkItemAbsorbed->alValues[1] + int32_t(pkItemAbsorbed->alValues[5] * 2);
							double dValue = lDefGrade * GetSocket(ACCE_ABSORPTION_SOCKET);
							dValue = (double)dValue / 100;
							dValue = (double)dValue + .5;
							lDefGrade = (int32_t) dValue;
							if ((pkItemAbsorbed->alValues[1] > 0 && (lDefGrade <= 0)) || (pkItemAbsorbed->alValues[5] > 0 && (lDefGrade < 1)))
								lDefGrade += 1;
							else if ((pkItemAbsorbed->alValues[1] > 0) || (pkItemAbsorbed->alValues[5] > 0))
								lDefGrade += 1;

							m_pOwner->ApplyPoint(APPLY_DEF_GRADE_BONUS, bAdd ? lDefGrade : -lDefGrade);

							int32_t lDefMagicBonus = pkItemAbsorbed->alValues[0];
							dValue = lDefMagicBonus * GetSocket(ACCE_ABSORPTION_SOCKET);
							dValue = (double)dValue / 100;
							dValue = (double)dValue + .5;
							lDefMagicBonus = (int32_t) dValue;
							if ((pkItemAbsorbed->alValues[0] > 0) && (lDefMagicBonus < 1))
								lDefMagicBonus += 1;
							else if (pkItemAbsorbed->alValues[0] > 0)
								lDefMagicBonus += 1;

							m_pOwner->ApplyPoint(APPLY_MAGIC_DEF_GRADE, bAdd ? lDefMagicBonus : -lDefMagicBonus);
						} */
			/* else  */if (pkItemAbsorbed->bType == ITEM_WEAPON)
			{
				int32_t lAttGrade = pkItemAbsorbed->alValues[4] + pkItemAbsorbed->alValues[5];
				if (pkItemAbsorbed->alValues[3] > pkItemAbsorbed->alValues[4])
					lAttGrade = pkItemAbsorbed->alValues[3] + pkItemAbsorbed->alValues[5];

				double dValue = lAttGrade * GetSocket(ACCE_ABSORPTION_SOCKET);
				dValue = dValue / 100;
				dValue = dValue + .5;
				lAttGrade = (int32_t)dValue;
				if (((pkItemAbsorbed->alValues[3] > 0) && (lAttGrade < 1)) || ((pkItemAbsorbed->alValues[4] > 0) && (lAttGrade < 1)))
					lAttGrade += 1;
				else if ((pkItemAbsorbed->alValues[3] > 0) || (pkItemAbsorbed->alValues[4] > 0))
					lAttGrade += 1;

				m_pOwner->ApplyPoint(APPLY_ATT_GRADE_BONUS, bAdd ? lAttGrade : -lAttGrade);

				int32_t lAttMagicGrade = pkItemAbsorbed->alValues[2] + pkItemAbsorbed->alValues[5];
				if (pkItemAbsorbed->alValues[1] > pkItemAbsorbed->alValues[2])
					lAttMagicGrade = pkItemAbsorbed->alValues[1] + pkItemAbsorbed->alValues[5];

				dValue = lAttMagicGrade * GetSocket(ACCE_ABSORPTION_SOCKET);
				dValue = dValue / 100;
				dValue = dValue + .5;
				lAttMagicGrade = (int32_t)dValue;
				if (((pkItemAbsorbed->alValues[1] > 0) && (lAttMagicGrade < 1)) || ((pkItemAbsorbed->alValues[2] > 0) && (lAttMagicGrade < 1)))
					lAttMagicGrade += 1;
				else if ((pkItemAbsorbed->alValues[1] > 0) || (pkItemAbsorbed->alValues[2] > 0))
					lAttMagicGrade += 1;

				m_pOwner->ApplyPoint(APPLY_MAGIC_ATT_GRADE, bAdd ? lAttMagicGrade : -lAttMagicGrade);
			}
		}
	}
#endif


	for (int i = 0; i < ITEM_APPLY_MAX_NUM; ++i)
	{
#ifdef ENABLE_ACCE_SYSTEM
		if ((m_pProto->aApplies[i].bType == APPLY_NONE) && (GetType() != ITEM_COSTUME) && (GetSubType() != COSTUME_ACCE))
#else
		if (m_pProto->aApplies[i].bType == APPLY_NONE)
#endif
			continue;

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
		if (IsMountItem())
			continue;
#endif

		int32_t value = m_pProto->aApplies[i].lValue;
#ifdef ENABLE_ACCE_SYSTEM
		if ((GetType() == ITEM_COSTUME) && (GetSubType() == COSTUME_ACCE))
		{
			TItemTable* pkItemAbsorbed = ITEM_MANAGER::instance().GetTable(GetSocket(ACCE_ABSORBED_SOCKET));
			if (pkItemAbsorbed)
			{
				if (pkItemAbsorbed->aApplies[i].bType == APPLY_NONE)
					continue;

				value = pkItemAbsorbed->aApplies[i].lValue;
				if (value < 0)
					continue;

				double dValue = value * GetSocket(ACCE_ABSORPTION_SOCKET);
				dValue = dValue / 100;
				dValue = dValue + .5;
				value = (int32_t)dValue;
				if ((pkItemAbsorbed->aApplies[i].lValue > 0) && (value <= 0))
					value += 1;
			}
			else
				continue;
		}
#endif
		if (m_pProto->aApplies[i].bType == APPLY_SKILL)
		{
			m_pOwner->ApplyPoint(m_pProto->aApplies[i].bType, bAdd ? value : value ^ 0x00800000);
		}
		else
		{
			if (0 != accessoryGrade)
				value += MAX(accessoryGrade, value * aiAccessorySocketEffectivePct[accessoryGrade] / 100);

			m_pOwner->ApplyPoint(m_pProto->aApplies[i].bType, bAdd ? value : -value);
		}
	}

#ifdef ENABLE_ITEM_EXTRA_PROTO
	if (HasExtraProto())
	{
#ifdef ENABLE_NEW_EXTRA_BONUS
		for (int i = 0; i < NEW_EXTRA_BONUS_COUNT; i++)
		{
			auto type = m_ExtraProto->ExtraBonus[i].bType;
			if (type != APPLY_NONE) {
				auto value = m_ExtraProto->ExtraBonus[i].lValue;
				m_pOwner->ApplyPoint(m_ExtraProto->ExtraBonus[i].bType, bAdd ? value : -value);
			}
		}
#endif
	}
#endif

	if (true == CItemVnumHelper::IsRamadanMoonRing(GetVnum()) || true == CItemVnumHelper::IsHalloweenCandy(GetVnum())
		|| true == CItemVnumHelper::IsHappinessRing(GetVnum()) || true == CItemVnumHelper::IsLovePendant(GetVnum()))
	{
		// Do not anything.
	}
	else
	{
		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		{
			if (GetAttributeType(i))
			{
				const TPlayerItemAttribute& ia = GetAttribute(i);
				int32_t sValue = ia.sValue;
#ifdef ENABLE_ACCE_SYSTEM
				if ((GetType() == ITEM_COSTUME) && (GetSubType() == COSTUME_ACCE)) {
					double dValue = sValue * GetSocket(ACCE_ABSORPTION_SOCKET);
					dValue = dValue / 100;
					dValue = dValue + .5;
					sValue = (int32_t)dValue;
					if ((ia.sValue > 0) && (sValue <= 0))
						sValue += 1;
				}
#endif

#ifdef ATTR_LOCK
				if (GetLockedAttr() == i) {
					continue;
				}
#endif

				if (ia.bType == APPLY_SKILL)
					m_pOwner->ApplyPoint(ia.bType, bAdd ? sValue : sValue ^ 0x00800000);
				else
					m_pOwner->ApplyPoint(ia.bType, bAdd ? sValue : -sValue);
			}
		}
	}

	switch (m_pProto->bType)
	{
	case ITEM_PICK:
	case ITEM_ROD:
	{
		if (bAdd)
		{
			if (m_wCell == INVENTORY_MAX_NUM + WEAR_WEAPON)
				m_pOwner->SetPart(PART_WEAPON, GetVnum());
		}
		else
		{
			if (m_wCell == INVENTORY_MAX_NUM + WEAR_WEAPON)
				m_pOwner->SetPart(PART_WEAPON, 0);
		}
	}
	break;

	case ITEM_WEAPON:
	{
#ifdef ENABLE_COSTUME_EFFECT
		if ((GetSubType() == WEAPON_SWORD) || (GetSubType() == WEAPON_DAGGER) || (GetSubType() == WEAPON_BOW) || (GetSubType() == WEAPON_TWO_HANDED) || (GetSubType() == WEAPON_BELL) || (GetSubType() == WEAPON_FAN)) {
			CItem* item = m_pOwner->GetWear(WEAR_COSTUME_EFFECT_WEAPON);
			if (item) {
				uint32_t toSetValueEffect;
				switch (this->GetSubType()) {
				case WEAPON_SWORD:
					toSetValueEffect = item->GetValue(0);
					break;
				case WEAPON_DAGGER:
					toSetValueEffect = item->GetValue(2);
					break;
				case WEAPON_BOW:
					toSetValueEffect = item->GetValue(3);
					break;
				case WEAPON_TWO_HANDED:
					toSetValueEffect = item->GetValue(1);
					break;
				case WEAPON_BELL:
					toSetValueEffect = item->GetValue(4);
					break;
				case WEAPON_FAN:
					toSetValueEffect = item->GetValue(5);
					break;
				default:
					toSetValueEffect = 0;
					break;
				}

				if (toSetValueEffect > 0) {
					uint32_t dwWeaponVnum = GetVnum();
					if (((dwWeaponVnum >= 1180) && (dwWeaponVnum <= 1189)) ||
						((dwWeaponVnum >= 1090) && (dwWeaponVnum <= 1099)) ||
						(dwWeaponVnum == 1199) ||
						(dwWeaponVnum == 1209) ||
						(dwWeaponVnum == 1219) ||
						(dwWeaponVnum == 1229) ||
						(dwWeaponVnum == 40099) ||
						((dwWeaponVnum >= 7190) && (dwWeaponVnum <= 7199))
						)
						toSetValueEffect += 500;
				}

				if (!bAdd)
					toSetValueEffect = 0;

				m_pOwner->SetPart(PART_EFFECT_WEAPON, toSetValueEffect);
			}
		}
#endif
#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
		if (nullptr != m_pOwner->GetWear(WEAR_COSTUME_WEAPON))
			break;
#endif

		if (bAdd)
		{
			if (m_wCell == INVENTORY_MAX_NUM + WEAR_WEAPON)
				m_pOwner->SetPart(PART_WEAPON, GetVnum());
		}
		else
		{
			if (m_wCell == INVENTORY_MAX_NUM + WEAR_WEAPON)
				m_pOwner->SetPart(PART_WEAPON, 0);
		}
	}
	break;

	case ITEM_ARMOR:
	{
#ifdef ENABLE_COSTUME_EFFECT
		if (GetSubType() == ARMOR_BODY) {
			CItem* item = m_pOwner->GetWear(WEAR_COSTUME_EFFECT_BODY);
			if (item) {
				uint32_t toSetValueEffect;
				toSetValueEffect = item->GetValue(0);
				if ((!bAdd) && (!m_pOwner->GetWear(WEAR_COSTUME_BODY)))
					toSetValueEffect = 0;

				m_pOwner->SetPart(PART_EFFECT_BODY, toSetValueEffect);
			}
		}
#endif

		if (nullptr != m_pOwner->GetWear(WEAR_COSTUME_BODY))
			break;

		if (GetSubType() == ARMOR_BODY || GetSubType() == ARMOR_HEAD || GetSubType() == ARMOR_FOOTS || GetSubType() == ARMOR_SHIELD)
		{
			if (bAdd)
			{
				if (GetProto()->bSubType == ARMOR_BODY)
					m_pOwner->SetPart(PART_MAIN, GetVnum());
			}
			else
			{
				if (GetProto()->bSubType == ARMOR_BODY)
					m_pOwner->SetPart(PART_MAIN, m_pOwner->GetOriginalPart(PART_MAIN));
			}
		}
	}
	break;

	case ITEM_COSTUME:
	{
		uint32_t toSetValue = this->GetVnum();
		EParts toSetPart = PART_MAX_NUM;

		if (GetSubType() == COSTUME_BODY)
		{
#ifdef ENABLE_COSTUME_EFFECT
			CItem* item = m_pOwner->GetWear(WEAR_COSTUME_EFFECT_BODY);
			if (item) {
				uint32_t toSetValueEffect;
				toSetValueEffect = item->GetValue(0);
				if ((!bAdd) && (!m_pOwner->GetWear(WEAR_BODY)))
					toSetValueEffect = 0;

				m_pOwner->SetPart(PART_EFFECT_BODY, toSetValueEffect);
			}
#endif
			toSetPart = PART_MAIN;

			if (false == bAdd)
			{
				const CItem* pArmor = m_pOwner->GetWear(WEAR_BODY);
				toSetValue = (nullptr != pArmor) ? pArmor->GetVnum() : m_pOwner->GetOriginalPart(PART_MAIN);
			}
		}
#ifdef ENABLE_RUNE_SYSTEM
		else if (GetSubType() == RUNE_SLOT7)
		{
			toSetPart = PART_RUNE;
			toSetValue = (true == bAdd) ? m_pOwner->GetRuneEffect() : 0;
		}
#endif
		else if (GetSubType() == COSTUME_HAIR)
		{
			toSetPart = PART_HAIR;
			toSetValue = (true == bAdd) ? this->GetValue(3) : 0;
		}

#ifdef ENABLE_ACCE_SYSTEM
		else if (GetSubType() == COSTUME_ACCE)
		{
			toSetValue -= 85000;
			if (GetSocket(ACCE_ABSORPTION_SOCKET) >= ACCE_EFFECT_FROM_ABS)
				toSetValue += 1000;

			toSetValue = (bAdd == true) ? toSetValue : 0;

#ifdef ENABLE_STOLE_COSTUME
			const CItem* pAcce = m_pOwner->GetWear(WEAR_COSTUME_ACCE);
			if (pAcce) {
				toSetValue = pAcce->GetVnum();
				toSetValue -= 85000;
				toSetValue += 1000;
			}
#endif

			toSetPart = PART_ACCE;
		}
#endif
#ifdef ENABLE_STOLE_COSTUME
		else if (GetSubType() == COSTUME_STOLE)
		{
			toSetValue -= 85000;
			if (!bAdd) {
				CItem* pAcce = m_pOwner->GetWear(WEAR_COSTUME_ACCE_SLOT);
				if (!pAcce)
					toSetValue = 0;
				else {
					toSetValue = pAcce->GetVnum();
					toSetValue -= 85000;
					if (pAcce->GetSocket(ACCE_ABSORPTION_SOCKET) >= ACCE_EFFECT_FROM_ABS)
						toSetValue += 1000;
				}
			}
			else
				toSetValue += 1000;

			toSetPart = PART_ACCE;
		}
#endif
#ifdef ENABLE_COSTUME_EFFECT
		else if (GetSubType() == COSTUME_EFFECT_BODY)
		{
			if (bAdd) {
				CItem* item = m_pOwner->GetWear(WEAR_BODY);
				toSetValue = item != nullptr ? this->GetValue(0) : 0;
				if (toSetValue == 0) {
					item = m_pOwner->GetWear(WEAR_COSTUME_BODY);
					toSetValue = item != nullptr ? this->GetValue(0) : 0;
				}
			}
			else
				toSetValue = 0;

			toSetPart = PART_EFFECT_BODY;
		}
		else if (GetSubType() == COSTUME_EFFECT_WEAPON)
		{
			if (bAdd) {
				CItem* item = m_pOwner->GetWear(WEAR_WEAPON);
				if (item) {
					switch (item->GetSubType()) {
					case WEAPON_SWORD:
						toSetValue = this->GetValue(0);
						break;
					case WEAPON_DAGGER:
						toSetValue = this->GetValue(2);
						break;
					case WEAPON_BOW:
						toSetValue = this->GetValue(3);
						break;
					case WEAPON_TWO_HANDED:
						toSetValue = this->GetValue(1);
						break;
					case WEAPON_BELL:
						toSetValue = this->GetValue(4);
						break;
					case WEAPON_FAN:
						toSetValue = this->GetValue(5);
						break;
					default:
						toSetValue = 0;
						break;
					}

					if (toSetValue > 0) {
						uint32_t dwWeaponVnum = item->GetVnum();
						if (((dwWeaponVnum >= 1180) && (dwWeaponVnum <= 1189)) ||
							((dwWeaponVnum >= 1090) && (dwWeaponVnum <= 1099)) ||
							(dwWeaponVnum == 1199) ||
							(dwWeaponVnum == 1209) ||
							(dwWeaponVnum == 1219) ||
							(dwWeaponVnum == 1229) ||
							(dwWeaponVnum == 40099) ||
							((dwWeaponVnum >= 7190) && (dwWeaponVnum <= 7199))
							)
							toSetValue += 500;
					}
				}
				else
					toSetValue = 0;
			}
			else
				toSetValue = 0;

			toSetPart = PART_EFFECT_WEAPON;
		}
#endif
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
		else if (GetSubType() == COSTUME_MOUNT)
		{
			// not need to do a thing in here
		}
#endif

#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
		else if (GetSubType() == COSTUME_WEAPON)
		{
			toSetPart = PART_WEAPON;
			if (false == bAdd)
			{
				const CItem* pWeapon = m_pOwner->GetWear(WEAR_WEAPON);
				if (pWeapon != nullptr) {
					toSetValue = (nullptr != pWeapon) ? pWeapon->GetVnum() : m_pOwner->GetOriginalPart(PART_WEAPON);
				}
				else {
					toSetValue = 0;
				}
			}
		}
#endif

		if (PART_MAX_NUM != toSetPart)
		{
			m_pOwner->SetPart((uint8_t)toSetPart, toSetValue);
			m_pOwner->UpdatePacket();

		}
	}
	break;
	case ITEM_UNIQUE:
	{
		if (0 != GetSIGVnum())
		{
			const CSpecialItemGroup* pItemGroup = ITEM_MANAGER::instance().GetSpecialItemGroup(GetSIGVnum());
			if (nullptr == pItemGroup)
				break;
			uint32_t dwAttrVnum = pItemGroup->GetAttrVnum(GetVnum());
			const CSpecialAttrGroup* pAttrGroup = ITEM_MANAGER::instance().GetSpecialAttrGroup(dwAttrVnum);
			if (nullptr == pAttrGroup)
				break;
			for (auto it = pAttrGroup->m_vecAttrs.begin(); it != pAttrGroup->m_vecAttrs.end(); ++it)
			{
				m_pOwner->ApplyPoint(it->apply_type, bAdd ? it->apply_value : it->apply_value); // -it->apply_value
			}
		}
	}
	break;
	}
}

bool CItem::IsEquipable()
{
	switch (GetType())
	{
	case ITEM_COSTUME:
	case ITEM_ARMOR:
	case ITEM_WEAPON:
	case ITEM_ROD:
	case ITEM_PICK:
	case ITEM_UNIQUE:
	case ITEM_DS:
	case ITEM_SPECIAL_DS:
	case ITEM_RING:
		return true;

	case ITEM_BELT:
		// Only belts with value5==1 are equipable (Title belts)
		return (GetValue(5) == 1);

	default:
		return false;
	}
}


#define ENABLE_IMMUNE_FIX
// return false on error state
bool CItem::EquipTo(LPCHARACTER ch, uint8_t bWearCell)
{
	if (!ch)
	{
		sys_err("EquipTo: nil character");
		return false;
	}

	if (IsDragonSoul())
	{
		if (bWearCell < WEAR_MAX_NUM || bWearCell >= WEAR_MAX_NUM + DRAGON_SOUL_DECK_MAX_NUM * DS_SLOT_MAX)
		{
			sys_err("EquipTo: invalid dragon soul cell (this: #%d %s wearflag: %d cell: %d)", GetOriginalVnum(), GetName(), GetSubType(), bWearCell - WEAR_MAX_NUM);
			return false;
		}
	}
	else
	{
		if (bWearCell >= WEAR_MAX_NUM)
		{
			sys_err("EquipTo: invalid wear cell (this: #%d %s wearflag: %d cell: %d)", GetOriginalVnum(), GetName(), GetWearFlag(), bWearCell);
			return false;
		}
	}

	if (ch->GetWear(bWearCell))
	{
		sys_err("EquipTo: item already exist (this: #%d %s cell: %d %s)", GetOriginalVnum(), GetName(), bWearCell, ch->GetWear(bWearCell)->GetName());
		return false;
	}

	if (GetOwner())
		RemoveFromCharacter();

	ch->SetWear(bWearCell, this);

	m_pOwner = ch;
	m_bEquipped = true;
	m_wCell = INVENTORY_MAX_NUM + bWearCell;

#ifndef ENABLE_IMMUNE_FIX
	uint32_t dwImmuneFlag = 0;

	for (int i = 0; i < WEAR_MAX_NUM; ++i)
	{
		if (m_pOwner->GetWear(i))
		{
			SET_BIT(dwImmuneFlag, m_pOwner->GetWear(i)->m_pProto->dwImmuneFlag);
		}
	}

	m_pOwner->SetImmuneFlag(dwImmuneFlag);
#endif

	if (IsDragonSoul())
	{
		DSManager::instance().ActivateDragonSoul(this);
	}
	else
	{
#ifdef ENABLE_RUNE_SYSTEM
		if (!IsRune())
			ModifyPoints(true);
		else if (GetSocket(1) == 1)
			ModifyPoints(true);
#else
		ModifyPoints(true);
#endif
		StartUniqueExpireEvent();
		if (-1 != GetProto()->cLimitTimerBasedOnWearIndex)
			StartTimerBasedOnWearExpireEvent();

		// ACCESSORY_REFINE
		StartAccessorySocketExpireEvent();
		// END_OF_ACCESSORY_REFINE
	}

	ch->BuffOnAttr_AddBuffsFromItem(this);

	m_pOwner->ComputeBattlePoints();

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
	if (IsMountItem())
		ch->MountSummon(this);
#endif
	m_pOwner->UpdatePacket();
#ifdef ENABLE_ITEM_ON_TITLE_RAZOR93
	if (bWearCell == WEAR_BELT)
		ch->UpdateItemOnTitleName(true);
#endif

#ifdef ENABLE_COSTUME_PET
	if ((GetType() == ITEM_COSTUME) && (GetSubType() == COSTUME_PET_SKIN)) {
		m_pOwner->UpdatePetSkin();
	}
#endif
#ifdef ENABLE_COSTUME_MOUNT
	if ((GetType() == ITEM_COSTUME) && (GetSubType() == COSTUME_MOUNT_SKIN)) {
		m_pOwner->UpdateMountSkin();
	}
#endif
	Save();
	return (true);
}

bool CItem::Unequip()
{
	if (!m_pOwner || GetCell() < INVENTORY_MAX_NUM)
	{
		// ITEM_OWNER_INVALID_PTR_BUG
		sys_err("%s %u m_pOwner %p, GetCell %d",
			GetName(), GetID(), get_pointer(m_pOwner), GetCell());
		// END_OF_ITEM_OWNER_INVALID_PTR_BUG
		return false;
	}

	if (this != m_pOwner->GetWear(GetCell() - INVENTORY_MAX_NUM))
	{
		sys_err("m_pOwner->GetWear() != this");
		return false;
	}

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
	if (IsMountItem())
		m_pOwner->MountUnsummon(this);
#endif

	//½Å±Ô ¸» ¾ÆÀÌÅÛ Á¦°Å½Ã Ã³¸®
	if (IsRideItem())
		ClearMountAttributeAndAffect();

	if (IsDragonSoul())
	{
		DSManager::instance().DeactivateDragonSoul(this);
	}
#ifdef ENABLE_RUNE_SYSTEM
	else if (IsRune()) {
		if (GetSocket(1) == 1)
			ModifyPoints(false);
	}
#endif
	else
	{
		ModifyPoints(false);
	}

	StopUniqueExpireEvent();

	if (-1 != GetProto()->cLimitTimerBasedOnWearIndex)
		StopTimerBasedOnWearExpireEvent();

	// ACCESSORY_REFINE
	StopAccessorySocketExpireEvent();
	// END_OF_ACCESSORY_REFINE


	m_pOwner->BuffOnAttr_RemoveBuffsFromItem(this);

	m_pOwner->SetWear(GetCell() - INVENTORY_MAX_NUM, nullptr);

#ifndef ENABLE_IMMUNE_FIX
	uint32_t dwImmuneFlag = 0;

	for (int i = 0; i < WEAR_MAX_NUM; ++i)
	{
		if (m_pOwner->GetWear(i))
		{
			SET_BIT(dwImmuneFlag, m_pOwner->GetWear(i)->m_pProto->dwImmuneFlag);
		}
	}

	m_pOwner->SetImmuneFlag(dwImmuneFlag);
#endif

	m_pOwner->ComputeBattlePoints();

	m_pOwner->UpdatePacket();
#ifdef ENABLE_COSTUME_PET
	if ((GetType() == ITEM_COSTUME) && (GetSubType() == COSTUME_PET_SKIN)) {
		m_pOwner->UpdatePetSkin();
	}
#endif
#ifdef ENABLE_COSTUME_MOUNT
	if ((GetType() == ITEM_COSTUME) && (GetSubType() == COSTUME_MOUNT_SKIN)) {
		m_pOwner->UpdateMountSkin();
	}
#endif
	m_pOwner = nullptr;
	m_wCell = 0;
	m_bEquipped = false;
	return true;
}

void CItem::SetExchanging(bool bOn)
{
	m_bExchanging = bOn;
}

void CItem::Save()
{
	if (m_bSkipSave)
		return;

	ITEM_MANAGER::instance().DelayedSave(this);
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

bool CItem::IsOwnership(LPCHARACTER ch)
{
	if (!m_pkOwnershipEvent)
		return true;

	return m_dwOwnershipPID == ch->GetPlayerID() ? true : false;
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

void CItem::SetOwnershipEvent(LPEVENT pkEvent)
{
	m_pkOwnershipEvent = pkEvent;
}

void CItem::SetOwnership(LPCHARACTER ch, int iSec)
{
	if (!ch)
	{
		if (m_pkOwnershipEvent)
		{
			event_cancel(&m_pkOwnershipEvent);
			m_dwOwnershipPID = 0;

			TPacketGCItemOwnership p;

			p.bHeader = HEADER_GC_ITEM_OWNERSHIP;
			p.dwVID = m_dwVID;
			p.szName[0] = '\0';

			PacketAround(&p, sizeof(p));
		}
		return;
	}

	if (m_pkOwnershipEvent)
		return;

	if (iSec <= 10)
		iSec = 30;

	m_dwOwnershipPID = ch->GetPlayerID();

	item_event_info* info = AllocEventInfo<item_event_info>();
	strlcpy(info->szOwnerName, ch->GetName(), sizeof(info->szOwnerName));
	info->item = this;

	SetOwnershipEvent(event_create(ownership_event, info, PASSES_PER_SEC(iSec)));

	TPacketGCItemOwnership p;

	p.bHeader = HEADER_GC_ITEM_OWNERSHIP;
	p.dwVID = m_dwVID;
	strlcpy(p.szName, ch->GetName(), sizeof(p.szName));

	PacketAround(&p, sizeof(p));
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

void CItem::SetUniqueExpireEvent(LPEVENT pkEvent)
{
	m_pkUniqueExpireEvent = pkEvent;
}

void CItem::SetTimerBasedOnWearExpireEvent(LPEVENT pkEvent)
{
	m_pkTimerBasedOnWearExpireEvent = pkEvent;
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

void CItem::StartRealTimeExpireEvent()
{
	if (m_pkRealTimeExpireEvent)
		return;

	for (auto aLimit : GetProto()->aLimits)
	{
		if (LIMIT_REAL_TIME == aLimit.bType || LIMIT_REAL_TIME_START_FIRST_USE == aLimit.bType)
		{
			item_vid_event_info* info = AllocEventInfo<item_vid_event_info>();
			info->item_vid = GetVID();
#ifdef ENABLE_NEW_USE_POTION
			if ((GetType() == ITEM_USE) && (GetSubType() == USE_NEW_POTIION)) {
				int32_t remainSec = GetSocket(0);
				if (remainSec <= 0) {
					if (GetSocket(1) == 1) {
						LPCHARACTER pkOwner = GetOwner();
						if (pkOwner) {
							if (pkOwner->FindAffect(GetValue(0))) {
								pkOwner->RemoveAffect(GetValue(0));
							}

#ifdef TEXTS_IMPROVEMENT
							pkOwner->ChatPacketNew(CHAT_TYPE_INFO, 27, "%s", GetName());
#endif
						}
					}

					ITEM_MANAGER::instance().RemoveItem(this, "REAL_TIME_EXPIRE");
					return;
				}

				info->newpotion = true;
				m_pkRealTimeExpireEvent = event_create(real_time_expire_event, info, PASSES_PER_SEC(remainSec > 60 ? 60 : remainSec));
			}
			else {
				info->newpotion = false;
				m_pkRealTimeExpireEvent = event_create(real_time_expire_event, info, PASSES_PER_SEC(1));
			}
#else
			m_pkRealTimeExpireEvent = event_create(real_time_expire_event, info, PASSES_PER_SEC(1));
#endif

			sys_log(0, "REAL_TIME_EXPIRE: StartRealTimeExpireEvent");
			return;
		}
	}
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

void CItem::StartUniqueExpireEvent()
{
	if (GetType() != ITEM_UNIQUE)
		return;

	if (m_pkUniqueExpireEvent)
		return;

	if (IsRealTimeItem() || IsRealTimeFirstUseItem() || IsUnlimitedTimeUnique())
		return;

	// HARD CODING
	/*if (GetVnum() == UNIQUE_ITEM_HIDE_ALIGNMENT_TITLE)
		m_pOwner->ShowAlignment(false);*/

	int iSec = GetSocket(ITEM_SOCKET_UNIQUE_SAVE_TIME);

	if (iSec == 0)
		iSec = 60;
	else
		iSec = MIN(iSec, 60);

	SetSocket(ITEM_SOCKET_UNIQUE_SAVE_TIME, 0);

	item_event_info* info = AllocEventInfo<item_event_info>();
	info->item = this;

	SetUniqueExpireEvent(event_create(unique_expire_event, info, PASSES_PER_SEC(iSec)));
}

void CItem::StartTimerBasedOnWearExpireEvent()
{
	if (m_pkTimerBasedOnWearExpireEvent)
		return;

	if (IsRealTimeItem())
		return;

	if (-1 == GetProto()->cLimitTimerBasedOnWearIndex)
		return;

	int iSec = GetSocket(0);

	if (0 != iSec)
	{
		iSec %= 60;
		if (0 == iSec)
			iSec = 60;
	}

	item_event_info* info = AllocEventInfo<item_event_info>();
	info->item = this;

	SetTimerBasedOnWearExpireEvent(event_create(timer_based_on_wear_expire_event, info, PASSES_PER_SEC(iSec)));
}

void CItem::StopUniqueExpireEvent()
{
	if (!m_pkUniqueExpireEvent)
		return;

	if (GetValue(2) != 0)
		return;

	// HARD CODING
	/*if (GetVnum() == UNIQUE_ITEM_HIDE_ALIGNMENT_TITLE)
		m_pOwner->ShowAlignment(true);*/

	SetSocket(ITEM_SOCKET_UNIQUE_SAVE_TIME, event_time(m_pkUniqueExpireEvent) / passes_per_sec);
	event_cancel(&m_pkUniqueExpireEvent);

	ITEM_MANAGER::instance().SaveSingleItem(this);
}

void CItem::StopTimerBasedOnWearExpireEvent()
{
	if (!m_pkTimerBasedOnWearExpireEvent)
		return;

	int remain_time = GetSocket(ITEM_SOCKET_REMAIN_SEC) - event_processing_time(m_pkTimerBasedOnWearExpireEvent) / passes_per_sec;

	SetSocket(ITEM_SOCKET_REMAIN_SEC, remain_time);
	event_cancel(&m_pkTimerBasedOnWearExpireEvent);

	ITEM_MANAGER::instance().SaveSingleItem(this);
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

void CItem::StartAccessorySocketExpireEvent()
{
	if (!IsAccessoryForSocket())
		return;

	if (m_pkAccessorySocketExpireEvent)
		return;

	if (GetAccessorySocketMaxGrade() == 0)
		return;

	if (GetAccessorySocketGrade() == 0)
		return;

	int iSec = GetAccessorySocketDownGradeTime();
#ifdef ENABLE_INFINITE_RAFINES
	if (iSec > 86400) {
		return;
	}
#endif
	SetAccessorySocketExpireEvent(nullptr);

	if (iSec <= 1)
		iSec = 5;
	else
		iSec = MIN(iSec, 60);

	item_vid_event_info* info = AllocEventInfo<item_vid_event_info>();
	info->item_vid = GetVID();

	SetAccessorySocketExpireEvent(event_create(accessory_socket_expire_event, info, PASSES_PER_SEC(iSec)));
}

void CItem::StopAccessorySocketExpireEvent()
{
	if (!m_pkAccessorySocketExpireEvent)
		return;

	if (!IsAccessoryForSocket())
		return;

	int new_time = GetAccessorySocketDownGradeTime() - (60 - event_time(m_pkAccessorySocketExpireEvent) / passes_per_sec);

	event_cancel(&m_pkAccessorySocketExpireEvent);

	if (new_time <= 1)
	{
		AccessorySocketDegrade();
	}
	else
	{
		SetAccessorySocketDownGradeTime(new_time);
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
void CItem::SetAccessorySocketExpireEvent(LPEVENT pkEvent)
{
	m_pkAccessorySocketExpireEvent = pkEvent;
}

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

void CItem::SetSoulItemEvent(LPEVENT pkEvent)
{
	m_pkSoulItemEvent = pkEvent;
}

void CItem::StartSoulItemEvent()
{
	if (GetType() != ITEM_SOUL)
		return;

	if (m_pkSoulItemEvent)
		return;

	int iMinutes = (GetSocket(2) / 10000);
	if (iMinutes >= GetLimitValue(1))
		return;

	item_vid_event_info* pInfo = AllocEventInfo<item_vid_event_info>();
	pInfo->item_vid = GetVID();
	SetSoulItemEvent(event_create(soul_item_event, pInfo, PASSES_PER_SEC(test_server ? 5 : 60)));
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
