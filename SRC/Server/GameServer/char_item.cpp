#include "stdafx.h"

#include <stack>

#include "utils.h"
#include "config.h"
#include "char.h"
#include "char_manager.h"
#include "item_manager.h"
#include "desc.h"
#include "desc_client.h"
#include "desc_manager.h"
#include "packet.h"
#include "affect.h"
#include "skill.h"
#include "start_position.h"
#include "mob_manager.h"
#include "db.h"
#include "log.h"
#include "vector.h"
#include "buffer_manager.h"
#include "questmanager.h"
#include "fishing.h"
#include "party.h"
#include "dungeon.h"
#include "refine.h"
#include "unique_item.h"
#include "war_map.h"
#include "marriage.h"
#include "polymorph.h"
#include "blend_item.h"
#include "BattleArena.h"
#include "arena.h"
#include "dev_log.h"
#include "pcbang.h"
#include "MountSystem.h"//@Razor93

#include "safebox.h"
#include "shop.h"

#ifdef __NEWPET_SYSTEM__
#include "New_PetSystem.h"
#define __NEWPET_SYSTEM_CHECK
#endif
#ifdef __PET_SYSTEM__
#include "PetSystem.h"
#endif
#ifdef ENABLE_NEWSTUFF
#include "pvp.h"
#endif
#ifdef ENABLE_SWITCHBOT
#include "new_switchbot.h"
#endif

#ifdef ENABLE_BATTLE_PASS
#include "battle_pass.h"
#endif

#include <common/VnumHelper.h>
#include "DragonSoul.h"
#include "buff_on_attributes.h"
#include "belt_inventory_helper.h"
#include <common/CommonDefines.h>
#ifdef ENABLE_CPP_DUNGEON_RAZOR93
//#include "LostCastleDungeon.h"
#include "RuneDungeon.h"
#include "ItemUse.h"
#include "Halloween2022Dungeon.h"
#include "VikingDungeon.h"
#endif

//#include "../common/service.h"
//auction_temp
#ifdef ENABLE_STOLE_COSTUME
#include <common/stole_length.h>
#endif

const int ITEM_BROKEN_METIN_VNUM = 28960;
extern int stone_chance; // Chance Stone Chance
#define ENABLE_EFFECT_EXTRAPOT
#define ENABLE_BOOKS_STACKFIX
#define ENABLE_STONE_STACKFIX //USE ONLY 1 STONE TO ADD
//#define __USE_ADD_WITH_ALL_ITEMS__ //CAN ADD OR SWITH GREEN BONUS WITH ALL ITEMS (MAX LVL 40)

// CHANGE_ITEM_ATTRIBUTES
// const uint32_t CHARACTER::msc_dwDefaultChangeItemAttrCycle = 10;
const char CHARACTER::msc_szLastChangeItemAttrFlag[] = "Item.LastChangeItemAttr";
// const char CHARACTER::msc_szChangeItemAttrCycleFlag[] = "change_itemattr_cycle";
// END_OF_CHANGE_ITEM_ATTRIBUTES
const uint8_t g_aBuffOnAttrPoints[] = { POINT_ENERGY, POINT_COSTUME_ATTR_BONUS };

#ifdef ENABLE_PVP_ADVANCED
static bool IS_POTION_PVP_BLOCKED(int vnum)
{
	switch (vnum)
	{
	case 72725:
	case 72726:
		return true;
	}
	return false;
}
#endif

struct FFindStone
{
	std::map<uint32_t, LPCHARACTER> m_mapStone;

	void operator()(LPENTITY pEnt)
	{
		if (pEnt->IsType(ENTITY_CHARACTER) == true)
		{
			LPCHARACTER pChar = (LPCHARACTER)pEnt;

			if (pChar->IsStone() == true)
			{
				m_mapStone[(uint32_t)pChar->GetVID()] = pChar;
			}
		}
	}
};


//±ÍÈ¯ºÎ, ±ÍÈ¯±â¾ïºÎ, °áÈ¥¹ÝÁö
static bool IS_SUMMON_ITEM(int vnum)
{
	switch (vnum)
	{
	case 22000:
	case 22010:
	case 22011:
	case 22020:
	case ITEM_MARRIAGE_RING:
		return true;
	}

	return false;
}

bool IS_SUMMONABLE_ZONE(int map_index)
{
	switch (map_index)
	{
	case 66: // »ç±ÍÅ¸¿ö
	case 71: // °Å¹Ì ´øÀü 2Ãþ
	case 72: // ÃµÀÇ µ¿±¼
	case 73: // ÃµÀÇ µ¿±¼ 2Ãþ
	case 193: // °Å¹Ì ´øÀü 2-1Ãþ
#if 0
	case 184: // ÃµÀÇ µ¿±¼(½Å¼ö)
	case 185: // ÃµÀÇ µ¿±¼ 2Ãþ(½Å¼ö)
	case 186: // ÃµÀÇ µ¿±¼(ÃµÁ¶)
	case 187: // ÃµÀÇ µ¿±¼ 2Ãþ(ÃµÁ¶)
	case 188: // ÃµÀÇ µ¿±¼(Áø³ë)
	case 189: // ÃµÀÇ µ¿±¼ 2Ãþ(Áø³ë)
#endif
		//		case 206 : // ¾Æ±Íµ¿±¼
	case 216: // ¾Æ±Íµ¿±¼
	case 217: // °Å¹Ì ´øÀü 3Ãþ
	case 208: // ÃµÀÇ µ¿±¼ (¿ë¹æ)

	case 113: // OX Event ¸Ê
		return false;
	}

	if (CBattleArena::IsBattleArenaMap(map_index)) return false;

	// ¸ðµç private ¸ÊÀ¸·Ð ¿öÇÁ ºÒ°¡´É
	if (map_index > 10000) return false;

	return true;
}

bool IS_BOTARYABLE_ZONE(int nMapIndex)
{
	if (!g_bEnableBootaryCheck) return true;

	switch (nMapIndex)
	{
	case 1:
	case 3:
	case 21:
	case 23:
	case 41:
	case 43:
		return true;
	}

	return false;
}

// item socket ÀÌ ÇÁ·ÎÅäÅ¸ÀÔ°ú °°ÀºÁö Ã¼Å© -- by mhh
static bool FN_check_item_socket(LPITEM item)
{
#ifdef ENABLE_NEW_USE_POTION
	if (item->GetType() == ITEM_USE && item->GetSubType() == USE_NEW_POTIION)
	{
		// inactive new potionok stackelhetnek
		// active példány (socket1 != 0) ne stackeljen vissza
		return item->GetSocket(1) == 0;
	}
#endif

	for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
	{
		if (item->GetSocket(i) != item->GetProto()->alSockets[i])
			return false;
	}

	return true;
}

// item socket º¹»ç -- by mhh
static bool FN_check_item_sex(LPCHARACTER ch, LPITEM item)
{

#ifdef ENABLE_SORT_INVEN
	if (item->GetType() == ITEM_USE && item->GetSubType() == USE_AFFECT)
		return true;
#endif

	// ³²ÀÚ ±ÝÁö
	if (IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_MALE))
	{
		if (SEX_MALE == GET_SEX(ch))
			return false;
	}
	// ¿©ÀÚ±ÝÁö
	if (IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_FEMALE))
	{
		if (SEX_FEMALE == GET_SEX(ch))
			return false;
	}

	return true;
}


/////////////////////////////////////////////////////////////////////////////
// ITEM HANDLING
/////////////////////////////////////////////////////////////////////////////
bool CHARACTER::CanHandleItem(bool bSkipCheckRefine, bool bSkipObserver)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ChatPacket(CHAT_TYPE_INFO, "char_item.cpp::bool CHARACTER::CanHandleItem");//INGAME_DEBUG_RAZOR93
	sys_log(0, "Razor93 LOG:: bool CHARACTER::CanHandleItem");
#endif
	if (!bSkipObserver)
		if (m_bIsObserver)
			return false;

	if (GetMyShop())
		return false;

	if (!bSkipCheckRefine)
		if (m_bUnderRefine)
			return false;

	if (IsCubeOpen() || nullptr != DragonSoul_RefineWindow_GetOpener())
		return false;

#ifdef __ATTR_TRANSFER_SYSTEM__
	if (IsAttrTransferOpen())
		return false;
#endif

	if (IsWarping())
		return false;

#ifdef ENABLE_ACCE_SYSTEM
	if (IsAcceOpen())
		return false;
#endif

	return true;
}

#ifdef ENABLE_EXTRA_INVENTORY
#endif

#ifdef __HIGHLIGHT_SYSTEM__
void CHARACTER::SetItem(TItemPos Cell, LPITEM pItem, bool isHighLight)
#else
void CHARACTER::SetItem(TItemPos Cell, LPITEM pItem)
#endif
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ChatPacket(CHAT_TYPE_INFO, "input_main.cpp:: void CInputMain::RequestLanguage ");//INGAME_DEBUG_RAZOR93
#endif
	uint16_t wCell = Cell.cell;
	uint8_t window_type = Cell.window_type;
	if ((unsigned long)((CItem*)pItem) == 0xff || (unsigned long)((CItem*)pItem) == 0xffffffff)
	{
		sys_err("!!! FATAL ERROR !!! item == 0xff (char: %s cell: %u)", GetName(), wCell);
		core_dump();
		return;
	}

	if (pItem && pItem->GetOwner())
	{
		assert(!"GetOwner exist");
		return;
	}
	// ±âº» ÀÎº¥Åä¸®
	switch (window_type)
	{
	case INVENTORY:
	case EQUIPMENT:
	{
		if (wCell >= INVENTORY_AND_EQUIP_SLOT_MAX)
		{
			sys_err("CHARACTER::SetItem: invalid item cell %d", wCell);
			return;
		}

		LPITEM pOld = m_pointsInstant.pItems[wCell];

		if (pOld)
		{
			if (wCell < INVENTORY_MAX_NUM)
			{
				for (int i = 0; i < pOld->GetSize(); ++i)
				{
					int p = wCell + (i * 5);

					if (p >= INVENTORY_MAX_NUM)
						continue;

					if (m_pointsInstant.pItems[p] && m_pointsInstant.pItems[p] != pOld)
						continue;

					m_pointsInstant.bItemGrid[p] = 0;
				}
			}
			else
				m_pointsInstant.bItemGrid[wCell] = 0;
		}

		if (pItem)
		{
			if (wCell < INVENTORY_MAX_NUM)
			{
				for (int i = 0; i < pItem->GetSize(); ++i)
				{
					int p = wCell + (i * 5);

					if (p >= INVENTORY_MAX_NUM)
						continue;

					// wCell + 1 ·Î ÇÏ´Â °ÍÀº ºó°÷À» Ã¼Å©ÇÒ ¶§ °°Àº
					// ¾ÆÀÌÅÛÀº ¿¹¿ÜÃ³¸®ÇÏ±â À§ÇÔ
					m_pointsInstant.bItemGrid[p] = wCell + 1;
				}
			}
			else
				m_pointsInstant.bItemGrid[wCell] = wCell + 1;
		}

		m_pointsInstant.pItems[wCell] = pItem;
	}
	break;
	// ¿ëÈ¥¼® ÀÎº¥Åä¸®
	case DRAGON_SOUL_INVENTORY:
	{
		LPITEM pOld = m_pointsInstant.pDSItems[wCell];

		if (pOld)
		{
			if (wCell < DRAGON_SOUL_INVENTORY_MAX_NUM)
			{
				for (int i = 0; i < pOld->GetSize(); ++i)
				{
					int p = wCell + (i * DRAGON_SOUL_BOX_COLUMN_NUM);

					if (p >= DRAGON_SOUL_INVENTORY_MAX_NUM)
						continue;

					if (m_pointsInstant.pDSItems[p] && m_pointsInstant.pDSItems[p] != pOld)
						continue;

					m_pointsInstant.wDSItemGrid[p] = 0;
				}
			}
			else
				m_pointsInstant.wDSItemGrid[wCell] = 0;
		}

		if (pItem)
		{
			if (wCell >= DRAGON_SOUL_INVENTORY_MAX_NUM)
			{
				sys_err("CHARACTER::SetItem: invalid DS item cell %d", wCell);
				return;
			}

			if (wCell < DRAGON_SOUL_INVENTORY_MAX_NUM)
			{
				for (int i = 0; i < pItem->GetSize(); ++i)
				{
					int p = wCell + (i * DRAGON_SOUL_BOX_COLUMN_NUM);

					if (p >= DRAGON_SOUL_INVENTORY_MAX_NUM)
						continue;

					// wCell + 1 ·Î ÇÏ´Â °ÍÀº ºó°÷À» Ã¼Å©ÇÒ ¶§ °°Àº
					// ¾ÆÀÌÅÛÀº ¿¹¿ÜÃ³¸®ÇÏ±â À§ÇÔ
					m_pointsInstant.wDSItemGrid[p] = wCell + 1;
				}
			}
			else
				m_pointsInstant.wDSItemGrid[wCell] = wCell + 1;
		}

		m_pointsInstant.pDSItems[wCell] = pItem;
	}
	break;



#ifdef ENABLE_EXTRA_INVENTORY
	case EXTRA_INVENTORY:
	{
		if (wCell >= EXTRA_INVENTORY_MAX_NUM)
		{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
			ChatPacket(CHAT_TYPE_INFO, "char_item.cpp::if (wCell >= EXTRA_INVENTORY_MAX_NUM)");//INGAME_DEBUG_RAZOR93
#endif
			sys_err("CHARACTER::SetItem: invalid EXTRA item cell %d", wCell);
			return;
		}

		LPITEM pOld = m_pointsInstant.pExtraItems[wCell];

		if (pOld)
		{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
			ChatPacket(CHAT_TYPE_INFO, "char_item.cpp::if (pOld)");//INGAME_DEBUG_RAZOR93
#endif

			if (wCell < EXTRA_INVENTORY_MAX_NUM)
			{
				for (int i = 0; i < pOld->GetSize(); ++i)
				{
					int p = wCell + (i * EXTRA_INVENTORY_PAGE_COLUMN);

					if (p >= EXTRA_INVENTORY_MAX_NUM)
						continue;

					if (m_pointsInstant.pExtraItems[p] && m_pointsInstant.pExtraItems[p] != pOld)
						continue;

					m_pointsInstant.wExtraItemGrid[p] = 0;
				}
			}
			else
				m_pointsInstant.wExtraItemGrid[wCell] = 0;
		}

		if (pItem)
		{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
			ChatPacket(CHAT_TYPE_INFO, "char_item.cpp::if (pItem)");//INGAME_DEBUG_RAZOR93
#endif
			if (wCell < EXTRA_INVENTORY_MAX_NUM)
			{
				for (int i = 0; i < pItem->GetSize(); ++i)
				{
					int p = wCell + (i * EXTRA_INVENTORY_PAGE_COLUMN);

					if (p >= EXTRA_INVENTORY_MAX_NUM)
						continue;

					m_pointsInstant.wExtraItemGrid[p] = wCell + 1;
				}
			}
			else
				m_pointsInstant.wExtraItemGrid[wCell] = wCell + 1;
		}

		m_pointsInstant.pExtraItems[wCell] = pItem;
	}
	break;
#endif

#ifdef ENABLE_SWITCHBOT
	case SWITCHBOT:
	{
		LPITEM pOld = m_pointsInstant.pSwitchbotItems[wCell];
		if (pItem && pOld)
		{
			return;
		}

		if (wCell >= SWITCHBOT_SLOT_COUNT)
		{
			sys_err("CHARACTER::SetItem: invalid switchbot item cell %d", wCell);
			return;
		}

		if (pItem)
		{
			CSwitchbotManager::Instance().RegisterItem(GetPlayerID(), pItem->GetID(), wCell);
		}
		else
		{
			CSwitchbotManager::Instance().UnregisterItem(GetPlayerID(), wCell);
		}

		m_pointsInstant.pSwitchbotItems[wCell] = pItem;
	}
	break;
#endif
	default:
		sys_err("Invalid Inventory type %d", window_type);
		return;
	}

	if (GetDesc())
	{
		// È®Àå ¾ÆÀÌÅÛ: ¼­¹ö¿¡¼­ ¾ÆÀÌÅÛ ÇÃ·¡±× Á¤º¸¸¦ º¸³½´Ù
		if (pItem)
		{
			TPacketGCItemSet pack;
			pack.header = HEADER_GC_ITEM_SET;
			pack.Cell = Cell;

			pack.count = pItem->GetCount();
#ifdef ATTR_LOCK
			pack.lockedattr = pItem->GetLockedAttr();
#endif
			pack.vnum = pItem->GetVnum();
			pack.flags = pItem->GetFlag();
			pack.anti_flags = pItem->GetAntiFlag();
#ifdef __HIGHLIGHT_SYSTEM__
			pack.highlight = isHighLight;
#else
			pack.highlight = (Cell.window_type == DRAGON_SOUL_INVENTORY);
#endif

			memcpy(pack.alSockets, pItem->GetSockets(), sizeof(pack.alSockets));
			memcpy(pack.aAttr, pItem->GetAttributes(), sizeof(pack.aAttr));

			GetDesc()->Packet(&pack, sizeof(TPacketGCItemSet));
		}
		else
		{
			TPacketGCItemDelDeprecated pack;
			pack.header = HEADER_GC_ITEM_DEL;
			pack.Cell = Cell;
			pack.count = 0;
#ifdef ATTR_LOCK
			pack.lockedattr = -1;
#endif
			pack.vnum = 0;
			memset(pack.alSockets, 0, sizeof(pack.alSockets));
			memset(pack.aAttr, 0, sizeof(pack.aAttr));

			GetDesc()->Packet(&pack, sizeof(TPacketGCItemDelDeprecated));
		}
	}

	if (pItem)
	{
		pItem->SetCell(this, wCell);
		switch (window_type)
		{
		case INVENTORY:
		case EQUIPMENT:
			if (wCell >= BELT_INVENTORY_SLOT_START && wCell < BELT_INVENTORY_SLOT_END)
			{
				if (CBeltInventoryHelper::CanMoveIntoBeltInventory(pItem))
					pItem->SetWindow(INVENTORY);
				else
					pItem->SetWindow(EQUIPMENT); // vagy return is lehet, ha nem engedelyezett
			}
			else if (wCell < INVENTORY_MAX_NUM)
			{
				pItem->SetWindow(INVENTORY);
			}
			else
			{
				pItem->SetWindow(EQUIPMENT);
			}

			break;
		case DRAGON_SOUL_INVENTORY:
			pItem->SetWindow(DRAGON_SOUL_INVENTORY);
			break;
#ifdef ENABLE_EXTRA_INVENTORY
		case EXTRA_INVENTORY:
			pItem->SetWindow(EXTRA_INVENTORY);
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
			sys_log(0, "Razor93 LOG:: Called: Char_item.cpp line :653: case switch :pItem->SetWindow(EXTRA_INVENTORY);");
#endif
			break;
#endif
#ifdef ENABLE_SWITCHBOT
		case SWITCHBOT:
			pItem->SetWindow(SWITCHBOT);
			break;
#endif
		}
	}
}

void CHARACTER::ClearItem()
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ChatPacket(CHAT_TYPE_INFO, "char_item.cpp:: void CHARACTER::ClearItem ");//INGAME_DEBUG_RAZOR93
#endif
	int		i;
	LPITEM	item;

	for (i = 0; i < INVENTORY_AND_EQUIP_SLOT_MAX; ++i)
	{
		if ((item = GetInventoryItem(i)))
		{
			item->SetSkipSave(true);
			ITEM_MANAGER::instance().FlushDelayedSave(item);

			item->RemoveFromCharacter();
			M2_DESTROY_ITEM(item);

			SyncQuickslot(QUICKSLOT_TYPE_ITEM, i, 255);
		}
	}
	for (i = 0; i < DRAGON_SOUL_INVENTORY_MAX_NUM; ++i)
	{
		if ((item = GetItem(TItemPos(DRAGON_SOUL_INVENTORY, i))))
		{
			item->SetSkipSave(true);
			ITEM_MANAGER::instance().FlushDelayedSave(item);

			item->RemoveFromCharacter();
			M2_DESTROY_ITEM(item);
		}
	}

#ifdef ENABLE_EXTRA_INVENTORY
	for (i = 0; i < EXTRA_INVENTORY_MAX_NUM; ++i)
	{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
		sys_log(0, "Razor93 LOG:: Called: Char_item.cpp line :739: for (i = 0; i < EXTRA_INVENTORY_MAX_NUM; ++i)");
#endif
		if ((item = GetExtraInventoryItem(i)))
		{
			item->SetSkipSave(true);
			ITEM_MANAGER::instance().FlushDelayedSave(item);

			item->RemoveFromCharacter();
			M2_DESTROY_ITEM(item);

			SyncQuickslot(QUICKSLOT_TYPE_ITEM_EXTRA, i, 255);
		}
	}
#endif

#ifdef ENABLE_SWITCHBOT
	for (i = 0; i < SWITCHBOT_SLOT_COUNT; ++i)
	{
		if ((item = GetItem(TItemPos(SWITCHBOT, i))))
		{
			item->SetSkipSave(true);
			ITEM_MANAGER::instance().FlushDelayedSave(item);

			item->RemoveFromCharacter();
			M2_DESTROY_ITEM(item);
		}
	}
#endif
}


bool CHARACTER::IsEmptyItemGrid(TItemPos Cell, uint8_t bSize, int iExceptionCell) const
{


#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
	switch (Cell.window_type)
	{
	case INVENTORY:
	{
		int bCell = Cell.cell;

		// bItemCell? 0? false?? ???? ?? + 1 ?? ????.
		// ??? iExceptionCell? 1? ?? ????.
		++iExceptionCell;

		/* 			if (Cell.IsBeltInventoryPosition())
					{
						LPITEM beltItem = GetWear(WEAR_BELT);

						if (NULL == beltItem)
							return false;

						if (false == CBeltInventoryHelper::IsAvailableCell(bCell - BELT_INVENTORY_SLOT_START, beltItem->GetValue(0)))
							return false;

						if (m_pointsInstant.bItemGrid[bCell])
						{
							if (m_pointsInstant.bItemGrid[bCell] == iExceptionCell)
								return true;

							return false;
						}

						if (bSize == 1)
							return true;

					} */
		if (Cell.IsBeltInventoryPosition())
		{
			// NE nezd meg, hogy van-e felszerelve ov
			// NE ellen?rizd az ov tipusat
			// --> mindig engedelyezett

			if (m_pointsInstant.bItemGrid[bCell])
			{
				if (m_pointsInstant.bItemGrid[bCell] == iExceptionCell)
					return true;

				return false;
			}

			if (bSize == 1)
				return true;
		}

		//black
		else if (bCell >= Inventory_Size())
			return false;

		if (m_pointsInstant.bItemGrid[bCell])
		{
			if (m_pointsInstant.bItemGrid[bCell] == iExceptionCell)
			{
				if (bSize == 1)
					return true;

				int j = 1;
				uint8_t bPage = bCell / (INVENTORY_MAX_NUM / 4);
				do
				{
					uint8_t p = bCell + (5 * j);

					if (p >= Inventory_Size())
						return false;

					if (p / (INVENTORY_MAX_NUM / 4) != bPage)
						return false;

					if (m_pointsInstant.bItemGrid[p])
						if (m_pointsInstant.bItemGrid[p] != iExceptionCell)
							return false;
				} while (++j < bSize);

				return true;
			}
			else
				return false;
		}

		// ??? 1?? ??? ???? ???? ?? ??
		if (1 == bSize)
			return true;
		else
		{
			int j = 1;
			uint8_t bPage = bCell / (INVENTORY_MAX_NUM / 4);

			do
			{
				uint8_t p = bCell + (5 * j);

				if (p >= Inventory_Size())
					return false;
				if (p / (INVENTORY_MAX_NUM / 4) != bPage)
					return false;

				if (m_pointsInstant.bItemGrid[p])
					if (m_pointsInstant.bItemGrid[p] != iExceptionCell)
						return false;
			} while (++j < bSize);

			return true;
		}
	}
#ifdef ENABLE_EXTRA_INVENTORY
	break;
	case EXTRA_INVENTORY:
	{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
		sys_log(0, "Razor93 LOG:: Called: Char_item.cpp line :894 /case switch/ : case EXTRA_INVENTORY:");
#endif
		uint16_t bCell = Cell.cell;

		if (bCell > ExtraInventoryMaxSlots(bCell, true))
			return false;

		++iExceptionCell;

		if (m_pointsInstant.wExtraItemGrid[bCell])
		{
			if (m_pointsInstant.wExtraItemGrid[bCell] == iExceptionCell)
			{
				if (bSize == 1)
					return true;

				int j = 1;
				uint8_t bPage = bCell / (EXTRA_INVENTORY_MAX_NUM / EXTRA_INVENTORY_PAGE_COUNT);

				do
				{
					int p = bCell + (5 * j);

					if (p > ExtraInventoryMaxSlots(bCell, true))
						return false;

					if (p / (EXTRA_INVENTORY_MAX_NUM / EXTRA_INVENTORY_PAGE_COUNT) != bPage)
						return false;

					if (m_pointsInstant.wExtraItemGrid[p])
						if (m_pointsInstant.wExtraItemGrid[p] != iExceptionCell)
							return false;
				} while (++j < bSize);

				return true;
			}
			else
				return false;
		}

		if (1 == bSize)
			return true;
		else
		{
			int j = 1;
			uint8_t bPage = bCell / (EXTRA_INVENTORY_MAX_NUM / EXTRA_INVENTORY_PAGE_COUNT);

			do
			{
				int p = bCell + (5 * j);

				if (p > ExtraInventoryMaxSlots(bCell, true))
					return false;

				if (p / (EXTRA_INVENTORY_MAX_NUM / EXTRA_INVENTORY_PAGE_COUNT) != bPage)
					return false;

				if (m_pointsInstant.wExtraItemGrid[p])
					if (m_pointsInstant.wExtraItemGrid[p] != iExceptionCell)
						return false;
			} while (++j < bSize);

			return true;
		}
	}
#endif
	break;
#else
	switch (Cell.window_type)
	{
	case INVENTORY:
	{
		uint8_t bCell = Cell.cell;

		// bItemCell? 0? false?? ???? ?? + 1 ?? ????.
		// ??? iExceptionCell? 1? ?? ????.
		++iExceptionCell;

		if (Cell.IsBeltInventoryPosition())
		{
			LPITEM beltItem = GetWear(WEAR_BELT);

			if (NULL == beltItem)
				return false;

			if (false == CBeltInventoryHelper::IsAvailableCell(bCell - BELT_INVENTORY_SLOT_START, beltItem->GetValue(0)))
				return false;

			if (m_pointsInstant.bItemGrid[bCell])
			{
				if (m_pointsInstant.bItemGrid[bCell] == iExceptionCell)
					return true;

				return false;
			}

			if (bSize == 1)
				return true;

		}
		//black
		else if (bCell >= INVENTORY_MAX_NUM)
			return false;

		if (m_pointsInstant.bItemGrid[bCell])
		{
			if (m_pointsInstant.bItemGrid[bCell] == iExceptionCell)
			{
				if (bSize == 1)
					return true;

				int j = 1;
				uint8_t bPage = bCell / (INVENTORY_MAX_NUM / 4);

				do
				{
					uint8_t p = bCell + (5 * j);

					if (p >= INVENTORY_MAX_NUM)
						return false;

					if (p / (INVENTORY_MAX_NUM / 4) != bPage)
						return false;

					if (m_pointsInstant.bItemGrid[p])
						if (m_pointsInstant.bItemGrid[p] != iExceptionCell)
							return false;
				} while (++j < bSize);

				return true;
			}
			else
				return false;
		}

		// ??? 1?? ??? ???? ???? ?? ??
		if (1 == bSize)
			return true;
		else
		{
			int j = 1;
			uint8_t bPage = bCell / (INVENTORY_MAX_NUM / 4);

			do
			{
				uint8_t p = bCell + (5 * j);

				if (p >= INVENTORY_MAX_NUM)
					return false;
				if (p / (INVENTORY_MAX_NUM / 4) != bPage)
					return false;

				if (m_pointsInstant.bItemGrid[p])
					if (m_pointsInstant.bItemGrid[p] != iExceptionCell)
						return false;
			} while (++j < bSize);

			return true;
		}
	}
#ifdef ENABLE_EXTRA_INVENTORY
	break;
	case EXTRA_INVENTORY:
	{
		uint16_t bCell = Cell.cell;

		if (bCell >= EXTRA_INVENTORY_MAX_NUM)
			return false;

		++iExceptionCell;

		if (m_pointsInstant.wExtraItemGrid[bCell])
		{
			if (m_pointsInstant.wExtraItemGrid[bCell] == iExceptionCell)
			{
				if (bSize == 1)
					return true;

				int j = 1;
				uint8_t bPage = bCell / (EXTRA_INVENTORY_MAX_NUM / EXTRA_INVENTORY_PAGE_COUNT);

				do
				{
					uint8_t p = bCell + (5 * j);

					if (p >= EXTRA_INVENTORY_MAX_NUM)
						return false;

					if (p / (EXTRA_INVENTORY_MAX_NUM / EXTRA_INVENTORY_PAGE_COUNT) != bPage)
						return false;

					if (m_pointsInstant.wExtraItemGrid[p])
						if (m_pointsInstant.wExtraItemGrid[p] != iExceptionCell)
							return false;
				} while (++j < bSize);

				return true;
			}
			else
				return false;
		}

		if (1 == bSize)
			return true;
		else
		{
			int j = 1;
			uint8_t bPage = bCell / (EXTRA_INVENTORY_MAX_NUM / EXTRA_INVENTORY_PAGE_COUNT);

			do
			{
				uint8_t p = bCell + (5 * j);

				if (p >= EXTRA_INVENTORY_MAX_NUM)
					return false;

				if (p / (EXTRA_INVENTORY_MAX_NUM / EXTRA_INVENTORY_PAGE_COUNT) != bPage)
					return false;

				if (m_pointsInstant.wExtraItemGrid[p])
					if (m_pointsInstant.wExtraItemGrid[p] != iExceptionCell)
						return false;
			} while (++j < bSize);

			return true;
		}
	}
#endif
	break;
#endif


#ifdef ENABLE_SWITCHBOT
	case SWITCHBOT:
	{
		uint16_t wCell = Cell.cell;
		if (wCell >= SWITCHBOT_SLOT_COUNT)
		{
			return false;
		}

		if (m_pointsInstant.pSwitchbotItems[wCell])
		{
			return false;
		}

		return true;
	}
#endif
	case DRAGON_SOUL_INVENTORY:
	{
		uint16_t wCell = Cell.cell;
		if (wCell >= DRAGON_SOUL_INVENTORY_MAX_NUM)
			return false;

		// bItemCellÀº 0ÀÌ falseÀÓÀ» ³ªÅ¸³»±â À§ÇØ + 1 ÇØ¼­ Ã³¸®ÇÑ´Ù.
		// µû¶ó¼­ iExceptionCell¿¡ 1À» ´õÇØ ºñ±³ÇÑ´Ù.
		iExceptionCell++;

		if (m_pointsInstant.wDSItemGrid[wCell])
		{
			if (m_pointsInstant.wDSItemGrid[wCell] == iExceptionCell)
			{
				if (bSize == 1)
					return true;

				int j = 1;

				do
				{
					int p = wCell + (DRAGON_SOUL_BOX_COLUMN_NUM * j);

					if (p >= DRAGON_SOUL_INVENTORY_MAX_NUM)
						return false;

					if (m_pointsInstant.wDSItemGrid[p])
						if (m_pointsInstant.wDSItemGrid[p] != iExceptionCell)
							return false;
				} while (++j < bSize);

				return true;
			}
			else
				return false;
		}

		// Å©±â°¡ 1ÀÌ¸é ÇÑÄ­À» Â÷ÁöÇÏ´Â °ÍÀÌ¹Ç·Î ±×³É ¸®ÅÏ
		if (1 == bSize)
			return true;
		else
		{
			int j = 1;

			do
			{
				int p = wCell + (DRAGON_SOUL_BOX_COLUMN_NUM * j);

				if (p >= DRAGON_SOUL_INVENTORY_MAX_NUM)
					return false;

				if (m_pointsInstant.bItemGrid[p])
					if (m_pointsInstant.wDSItemGrid[p] != iExceptionCell)
						return false;
			} while (++j < bSize);

			return true;
		}
	}
	}
	return false;
	}

int CHARACTER::GetEmptyInventory(uint8_t size) const
{
	// NOTE: ÇöÀç ÀÌ ÇÔ¼ö´Â ¾ÆÀÌÅÛ Áö±Þ, È¹µæ µîÀÇ ÇàÀ§¸¦ ÇÒ ¶§ ÀÎº¥Åä¸®ÀÇ ºó Ä­À» Ã£±â À§ÇØ »ç¿ëµÇ°í ÀÖ´Âµ¥,
	//		º§Æ® ÀÎº¥Åä¸®´Â Æ¯¼ö ÀÎº¥Åä¸®ÀÌ¹Ç·Î °Ë»çÇÏÁö ¾Êµµ·Ï ÇÑ´Ù. (±âº» ÀÎº¥Åä¸®: INVENTORY_MAX_NUM ±îÁö¸¸ °Ë»ç)
#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
	const int inventoryLimit = std::min(Inventory_Size(), (int)INVENTORY_MAX_NUM);
	for (int i = 0; i < inventoryLimit; ++i)
#else
	for (int i = 0; i < INVENTORY_MAX_NUM; ++i)
#endif
		if (IsEmptyItemGrid(TItemPos(INVENTORY, i), size))
			return i;
	return -1;
}

#ifdef ENABLE_LOCKED_EXTRA_INVENTORY
int CHARACTER::ExtraInventoryMaxSlots(int iArg1, bool bAuto) const {

	if (bAuto) {
		if ((iArg1 >= 0) && (iArg1 < (EXTRA_INVENTORY_CATEGORY_MAX_NUM * 1)))
			iArg1 = 0;
		else if ((iArg1 >= (EXTRA_INVENTORY_CATEGORY_MAX_NUM * 1)) && (iArg1 < (EXTRA_INVENTORY_CATEGORY_MAX_NUM * 2)))
			iArg1 = 1;
		else if ((iArg1 >= (EXTRA_INVENTORY_CATEGORY_MAX_NUM * 2)) && (iArg1 < (EXTRA_INVENTORY_CATEGORY_MAX_NUM * 3)))
			iArg1 = 2;
		else if ((iArg1 >= (EXTRA_INVENTORY_CATEGORY_MAX_NUM * 3)) && (iArg1 < (EXTRA_INVENTORY_CATEGORY_MAX_NUM * 4)))
			iArg1 = 3;
		else if ((iArg1 >= (EXTRA_INVENTORY_CATEGORY_MAX_NUM * 4)) && (iArg1 < (EXTRA_INVENTORY_CATEGORY_MAX_NUM * 5)))
			iArg1 = 4;
		else if ((iArg1 >= (EXTRA_INVENTORY_CATEGORY_MAX_NUM * 5)) && (iArg1 < (EXTRA_INVENTORY_CATEGORY_MAX_NUM * 6)))
			iArg1 = 5;
	}

	if ((iArg1 < 0) || (iArg1 > 5))
		return 0;

	int iUnlock;
	switch (iArg1) {
	case 0: {
		iUnlock = GetQuestFlag("lock_extra.cat1") * 5;
		break;
	}
	case 1: {
		iUnlock = GetQuestFlag("lock_extra.cat2") * 5;
		break;
	}
	case 2: {
		iUnlock = GetQuestFlag("lock_extra.cat3") * 5;
		break;
	}
	case 3: {
		iUnlock = GetQuestFlag("lock_extra.cat4") * 5;
		break;
	}
	case 4: {
		iUnlock = GetQuestFlag("lock_extra.cat5") * 5;
		break;
	}
	case 5: {
		iUnlock = GetQuestFlag("lock_extra.cat6") * 5;
		break;
	}
	default: {
		iUnlock = 0;
		break;
	}
	}

	//int iUnlock = GetPoint(POINT_EXTRA_INVENTORY1 + iArg1) * 5;
	int iMaxUnlock = 25 + EXTRA_INVENTORY_PAGE_SIZE;
	int iStart = EXTRA_INVENTORY_CATEGORY_MAX_NUM * iArg1;
	int iFree = (EXTRA_INVENTORY_PAGE_SIZE * 2) + 20;
	return iUnlock > iMaxUnlock ? iMaxUnlock + iStart + iFree : iUnlock + iStart + iFree;
}

static int NeedKeysForExtraInventory[] = {
											1, // 20-25
											1, // 25-30
											1, // 30-35
											2, // 35-40
											2, // 40-45 : end page 3
											2, // 45-50
											3, // 50-55
											3, // 55-60
											3, // 60-65
											4, // 65-70
											4, // 70-75
											4, // 75-80
											5, // 80-85
											6, // 90-95 : end page 4
};

void CHARACTER::UnlockExtraInventory(uint8_t category) {
	if (category > 5) {
		return;
	}

#ifdef ENABLE_SPAM_CHECK
	int32_t time = GetLastUnlock() - get_global_time();
	if (time > 0) {
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 234, "%d", time);
#endif
		return;
	}
#endif

	std::string stageName;
	switch (category) {
	case 1: {
		stageName = "lock_extra.cat2";
	} break;
	case 2: {
		stageName = "lock_extra.cat3";
	} break;
	case 3: {
		stageName = "lock_extra.cat4";
	} break;
	case 4: {
		stageName = "lock_extra.cat5";
	} break;
	case 5: {
		stageName = "lock_extra.cat6";
	} break;
	default: {
		stageName = "lock_extra.cat1";
	} break;
	}

	uint8_t stage = GetQuestFlag(stageName.c_str());
	if (stage < 0 || stage >= 14)
		return;

	int needKeys = NeedKeysForExtraInventory[stage];
	if (CountSpecifyItem(72320) >= needKeys) {
		RemoveSpecifyItem(72320, needKeys);

		SetQuestFlag(stageName.c_str(), stage + 1);
		PointChange(POINT_EXTRA_INVENTORY1 + category, stage + 1);
		ChatPacket(CHAT_TYPE_COMMAND, "RefreshExpandInventory");
#ifdef ENABLE_SPAM_CHECK
		SetLastUnlock();
#endif
	}
	else {
		ChatPacket(CHAT_TYPE_COMMAND, "update_envanter_need %d", needKeys - CountSpecifyItem(72320));
	}
}
#endif

#ifdef ENABLE_EXTRA_INVENTORY
int CHARACTER::GetEmptyExtraInventory(LPITEM pItem) const
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	sys_log(0, "Razor93 LOG:: Called: Char_item.cpp  CHARACTER::GetEmptyExtraInventory(LPITEM pItem) const");
#endif
	uint8_t category = pItem->GetExtraCategory();
#ifdef ENABLE_LOCKED_EXTRA_INVENTORY
	for (int i = EXTRA_INVENTORY_CATEGORY_MAX_NUM * category; i < ExtraInventoryMaxSlots(category); ++i)
#else
	for (int i = EXTRA_INVENTORY_CATEGORY_MAX_NUM * category; i < EXTRA_INVENTORY_CATEGORY_MAX_NUM * (category + 1); ++i)
#endif
		if (IsEmptyItemGrid(TItemPos(EXTRA_INVENTORY, i), pItem->GetSize()))
			return i;

	return -1;
}

int CHARACTER::GetEmptyExtraInventory(uint8_t size, uint8_t category) const // needed for offline shop
{
#ifdef ENABLE_LOCKED_EXTRA_INVENTORY
	for (int i = EXTRA_INVENTORY_CATEGORY_MAX_NUM * category; i < ExtraInventoryMaxSlots(category); ++i)
#else
	for (int i = EXTRA_INVENTORY_CATEGORY_MAX_NUM * category; i < EXTRA_INVENTORY_CATEGORY_MAX_NUM * (category + 1); ++i)
#endif
		if (IsEmptyItemGrid(TItemPos(EXTRA_INVENTORY, i), size))
			return i;

	return -1;
}
#endif

int CHARACTER::GetEmptyDragonSoulInventory(LPITEM pItem) const
{

	if (nullptr == pItem || !pItem->IsDragonSoul())
		return -1;

	uint8_t bSize = pItem->GetSize();
	uint16_t wBaseCell = DSManager::instance().GetBasePosition(pItem);

	if (WORD_MAX == wBaseCell)
		return -1;

	for (int i = 0; i < DRAGON_SOUL_BOX_SIZE; ++i)
		if (IsEmptyItemGrid(TItemPos(DRAGON_SOUL_INVENTORY, i + wBaseCell), bSize))
			return i + wBaseCell;

	return -1;
}

void CHARACTER::CopyDragonSoulItemGrid(std::vector<uint16_t>&vDragonSoulItemGrid) const
{
	vDragonSoulItemGrid.resize(DRAGON_SOUL_INVENTORY_MAX_NUM);

	std::copy(m_pointsInstant.wDSItemGrid, m_pointsInstant.wDSItemGrid + DRAGON_SOUL_INVENTORY_MAX_NUM, vDragonSoulItemGrid.begin());
}

int CHARACTER::CountEmptyInventory() const
{
	int	count = 0;

#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
	const int inventoryLimit = std::min(Inventory_Size(), (int)INVENTORY_MAX_NUM);
	for (int i = 0; i < inventoryLimit; ++i)
#else
	for (int i = 0; i < INVENTORY_MAX_NUM; ++i)
#endif
		if (GetInventoryItem(i))
			count += GetInventoryItem(i)->GetSize();

#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
	return (inventoryLimit - count);
#else
	return (INVENTORY_MAX_NUM - count);
#endif
}

void TransformRefineItem(LPITEM pkOldItem, LPITEM pkNewItem)
{

	// ACCESSORY_REFINE
	if (pkOldItem->IsAccessoryForSocket())
	{
		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
		{
			pkNewItem->SetSocket(i, pkOldItem->GetSocket(i));
		}
		//pkNewItem->StartAccessorySocketExpireEvent();
	}
	// END_OF_ACCESSORY_REFINE
	else
	{
		// ¿©±â¼­ ±úÁø¼®ÀÌ ÀÚµ¿ÀûÀ¸·Î Ã»¼Ò µÊ
		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
		{
			if (!pkOldItem->GetSocket(i))
				break;
			else
				pkNewItem->SetSocket(i, 1);
		}

		// ¼ÒÄÏ ¼³Á¤
		int slot = 0;

		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
		{
			int32_t socket = pkOldItem->GetSocket(i);

			if (socket > 2 && socket != ITEM_BROKEN_METIN_VNUM)
				pkNewItem->SetSocket(slot++, socket);
		}

	}

	// ¸ÅÁ÷ ¾ÆÀÌÅÛ ¼³Á¤
	pkOldItem->CopyAttributeTo(pkNewItem);
}

void NotifyRefineSuccess(LPCHARACTER ch, LPITEM item, const char* way)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ch->ChatPacket(CHAT_TYPE_INFO, "char_item.cpp::void NotifyRefineSuccess ");//INGAME_DEBUG_RAZOR93
#endif
	if (nullptr != ch && item != nullptr)
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "RefineSuceeded");

		LogManager::instance().RefineLog(ch->GetPlayerID(), item->GetName(), item->GetID(), item->GetRefineLevel(), 1, way);
	}
}

void NotifyRefineFail(LPCHARACTER ch, LPITEM item, const char* way, int success = 0)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ch->ChatPacket(CHAT_TYPE_INFO, "char_item.cpp:: void NotifyRefineFail ");//INGAME_DEBUG_RAZOR93
#endif
	if (nullptr != ch && nullptr != item)
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "RefineFailed");

		LogManager::instance().RefineLog(ch->GetPlayerID(), item->GetName(), item->GetID(), item->GetRefineLevel(), success, way);
	}
}

void CHARACTER::SetRefineNPC(LPCHARACTER ch)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ch->ChatPacket(CHAT_TYPE_INFO, "char_item.cpp:: void CHARACTER::SetRefineNPC ");//INGAME_DEBUG_RAZOR93
#endif
	if (ch != nullptr)
	{
		m_dwRefineNPCVID = ch->GetVID();
	}
	else
	{
		m_dwRefineNPCVID = 0;
	}
}

bool CHARACTER::DoRefine(LPITEM item, bool bMoneyOnly)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	this->ChatPacket(CHAT_TYPE_INFO, "char_item.cpp:: bool CHARACTER::DoRefine ");
#endif
	if (!CanHandleItem(true))
	{
		ClearRefineMode();
		return false;
	}

	//°³·® ½Ã°£Á¦ÇÑ : upgrade_refine_scroll.quest ¿¡¼­ °³·®ÈÄ 5ºÐÀÌ³»¿¡ ÀÏ¹Ý °³·®À»
	//ÁøÇàÇÒ¼ö ¾øÀ½
	if (quest::CQuestManager::instance().GetEventFlag("update_refine_time") != 0)
	{
		if (get_global_time() < quest::CQuestManager::instance().GetEventFlag("update_refine_time") + (60 * 5))
		{
			sys_log(0, "can't refine %d %s", GetPlayerID(), GetName());
			return false;
		}
	}

	const TRefineTable* prt = CRefineManager::instance().GetRefineRecipe(item->GetRefineSet());

	if (!prt)
		return false;

	uint32_t result_vnum = item->GetRefinedVnum();
	int64_t cost = ComputeRefineFee(prt->cost);

	if (result_vnum == 0)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 305, "");
#endif
		return false;
	}

	if (item->GetType() == ITEM_USE && item->GetSubType() == USE_TUNING)
		return false;

	TItemTable* pProto = ITEM_MANAGER::instance().GetTable(item->GetRefinedVnum());

	if (!pProto)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 427, "");
#endif
		return false;
	}

	// REFINE_COST
	if (GetGold() < cost)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 232, "");
#ifdef ENABLE_FEATURES_REFINE_SYSTEM
		CRefineManager::instance().Reset_percent(this);
#endif
#endif
		return false;
	}

	if (!bMoneyOnly)
	{
		for (int i = 0; i < prt->material_count; ++i)
		{
			if (CountSpecifyItem(prt->materials[i].vnum) < prt->materials[i].count)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 233, "");
#endif
				return false;
			}
		}

		for (int i = 0; i < prt->material_count; ++i)
			RemoveSpecifyItem(prt->materials[i].vnum, prt->materials[i].count);
	}

	int prob = number(1, 100);


#ifdef ENABLE_FEATURES_REFINE_SYSTEM	
	if (IsRefineThroughGuild() || bMoneyOnly)
	{
		prob -= 10;
	}

	int success_prob = prt->prob;
	success_prob += CRefineManager::instance().Result(this);
#else
	if (IsRefineThroughGuild() || bMoneyOnly)
		prob -= 10;

#endif
	// END_OF_REFINE_COST
#ifdef ENABLE_FEATURES_REFINE_SYSTEM	
	if (prob <= success_prob)
#else
	if (prob <= prt->prob)
#endif
	{
		// ¼º°ø! ¸ðµç ¾ÆÀÌÅÛÀÌ »ç¶óÁö°í, °°Àº ¼Ó¼ºÀÇ ´Ù¸¥ ¾ÆÀÌÅÛ È¹µæ
		LPITEM pkNewItem = ITEM_MANAGER::instance().CreateItem(result_vnum, 1, 0, false);

		if (pkNewItem)
		{
			ITEM_MANAGER::CopyAllAttrTo(item, pkNewItem);
			LogManager::instance().ItemLog(this, pkNewItem, "REFINE SUCCESS", pkNewItem->GetName());

			uint8_t bCell = item->GetCell();


#ifdef ENABLE_BATTLE_PASS
			uint8_t bBattlePassId = GetBattlePassId();
			if (bBattlePassId)
			{
				uint32_t dwItemVnum, dwCount;
				if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, REFINE_ITEM, &dwItemVnum, &dwCount))
				{
					if (dwItemVnum == item->GetVnum() && GetMissionProgress(REFINE_ITEM, bBattlePassId) < dwCount)
						UpdateMissionProgress(REFINE_ITEM, bBattlePassId, 1, dwCount);
				}
			}
#endif

			// DETAIL_REFINE_LOG
			NotifyRefineSuccess(this, item, IsRefineThroughGuild() ? "GUILD" : "POWER");
			DBManager::instance().SendMoneyLog(MONEY_LOG_REFINE, item->GetVnum(), -cost);
			ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (REFINE SUCCESS)");
			// END_OF_DETAIL_REFINE_LOG

			pkNewItem->AddToCharacter(this, TItemPos(INVENTORY, bCell));
			ITEM_MANAGER::instance().FlushDelayedSave(pkNewItem);

			sys_log(0, "Refine Success %lld", (long long)cost);
			pkNewItem->AttrLog();
			//PointChange(POINT_GOLD, -cost);
			sys_log(0, "PayPee %lld", (long long)cost);
#ifdef ENABLE_FEATURES_REFINE_SYSTEM
			CRefineManager::instance().Reset(this);
#endif
			PayRefineFee(cost);
			sys_log(0, "PayPee End %lld", cost);
		}
		else
		{
			// DETAIL_REFINE_LOG
			// ¾ÆÀÌÅÛ »ý¼º¿¡ ½ÇÆÐ -> °³·® ½ÇÆÐ·Î °£ÁÖ
			sys_err("cannot create item %u", result_vnum);
			NotifyRefineFail(this, item, IsRefineThroughGuild() ? "GUILD" : "POWER");
			// END_OF_DETAIL_REFINE_LOG
		}
	}
	else
	{
		// ½ÇÆÐ! ¸ðµç ¾ÆÀÌÅÛÀÌ »ç¶óÁü.
		DBManager::instance().SendMoneyLog(MONEY_LOG_REFINE, item->GetVnum(), -cost);
		NotifyRefineFail(this, item, IsRefineThroughGuild() ? "GUILD" : "POWER");
		item->AttrLog();
		ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (REFINE FAIL)");

		//PointChange(POINT_GOLD, -cost);
#ifdef ENABLE_FEATURES_REFINE_SYSTEM
		CRefineManager::instance().Reset(this);
#endif
		PayRefineFee(cost);
	}

	return true;
}

enum enum_RefineScrolls
{
	CHUKBOK_SCROLL = 0,
	HYUNIRON_CHN = 1, // Áß±¹¿¡¼­¸¸ »ç¿ë
	YONGSIN_SCROLL = 2,
	MUSIN_SCROLL = 3,
	YAGONG_SCROLL = 4,
	MEMO_SCROLL = 5,
	BDRAGON_SCROLL = 6,
#ifdef ENABLE_SOUL_SYSTEM
	SOUL_SCROLL = 9,
#endif
};

//#include <set>
#ifdef ENABLE_UPGRADE_NOTICE_BY_RAZOR93

std::set<uint32_t> allowedVnums = {
	1610, 1611, 1612, 1613,
	1630, 1631, 1632, 1633,
	1650, 1651, 1652, 1653,
	1670, 1671, 1672, 1673,
	1690, 1691, 1692, 1693,
	1710, 1711, 1712, 1713,
	1730, 1731, 1732, 1733,
	1750, 1751, 1752, 1753,
	1770, 1771, 1772, 1773,
	1790, 1791, 1792, 1793,
	1810, 1811, 1812, 1813,
	1850, 1851, 1852, 1853,
	1870, 1871, 1872, 1873,
	1890, 1891, 1892, 1893,
	1910, 1911, 1912, 1913,
	1930, 1931, 1932, 1933,
	1950, 1951, 1952, 1953,

	8060, 8061, 8062, 8063,
	8080, 8081, 8082, 8083,
	8100, 8101, 8102, 8103,
	8120, 8121, 8122, 8123,
	8140, 8141, 8142, 8143,
	8160, 8161, 8162, 8163,
	8200, 8201, 8202, 8203,
	8220, 8221, 8222, 8223,
	8240, 8241, 8242, 8243,
	8260, 8261, 8262, 8263,
	8280, 8281, 8282, 8283,
	8330, 8331, 8332, 8333,
	8360, 8361, 8362, 8363,
	8380, 8381, 8382, 8383,
	8400, 8401, 8402, 8403,
	8420, 8421, 8422, 8423,
	8440, 8441, 8442, 8443,

	12100, 12101, 12102, 12103,
	12104, 12105, 12106, 12107,
	12110, 12111,
	12112, 12113, 12114, 12115,

	12790, 12791, 12792, 12793,

	12810, 12811, 12812, 12813,
	12830, 12831, 12832, 12833,
	12850, 12851, 12852, 12853,
	12854, 12855, 12856, 12857,
	12860, 12861,
	12862, 12863, 12864, 12865,
	12866, 12867,

	13070, 13071, 13072, 13073,
	13090, 13091, 13092, 13093,

	13110, 13111, 13112, 13113,
	13130, 13131, 13132, 13133,
	13150, 13151, 13152, 13153,
	13170, 13171, 13172, 13173,

	14230, 14231, 14232, 14233,
	15010, 15011, 15012, 15013,

	15460, 15461, 15462, 15463,
	15464, 15465, 15466, 15467,

	16230, 16231, 16232, 16233,
	16590, 16591, 16592, 16593,
	17230, 17231, 17232, 17233,
	17580, 17581, 17582, 17583,
	19310, 19311, 19312,
	19510, 19511, 19512,
	19710, 19711, 19712,
	19910, 19911, 19912
};
#endif ENABLE_UPGRADE_NOTICE_BY_RAZOR93

#ifdef ENABLE_MUSIN_SCROLL_REFINE_100_SUCCESS_RAZOR93

bool CHARACTER::DoRefineWithScroll(LPITEM item)
{
	
	//if (item && IsRefineBlockedVnum(item->GetVnum()))
	//{
	//	ChatPacket(CHAT_TYPE_INFO, "Ezt a targyat nem lehet fejleszteni.");
	//	ClearRefineMode();
	//	return false;
	//}

	if (!CanHandleItem(true))
	{
		ClearRefineMode();
		return false;
	}

	ClearRefineMode();

	//°³·® ½Ã°£Á¦ÇÑ : upgrade_refine_scroll.quest ¿¡¼­ °³·®ÈÄ 5ºÐÀÌ³»¿¡ ÀÏ¹Ý °³·®À»
		//ÁøÇàÇÒ¼ö ¾øÀ½
	if (quest::CQuestManager::instance().GetEventFlag("update_refine_time") != 0)
	{
		if (get_global_time() < quest::CQuestManager::instance().GetEventFlag("update_refine_time") + (60 * 5))
		{
			sys_log(0, "can't refine %d %s", GetPlayerID(), GetName());
			return false;
		}
	}

	const TRefineTable* prt = CRefineManager::instance().GetRefineRecipe(item->GetRefineSet());

	if (!prt)
		return false;

	LPITEM pkItemScroll;

	// °³·®¼­ Ã¼Å©
	if (m_iRefineAdditionalCell < 0)
		return false;

	pkItemScroll = GetInventoryItem(m_iRefineAdditionalCell);

	if (!pkItemScroll)
		return false;

	if (!(pkItemScroll->GetType() == ITEM_USE && pkItemScroll->GetSubType() == USE_TUNING))
		return false;

	if (pkItemScroll->GetVnum() == item->GetVnum())
		return false;

	uint32_t result_vnum = item->GetRefinedVnum();
	uint32_t result_fail_vnum = item->GetRefineFromVnum();

	if (result_vnum == 0)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 305, "");
#endif
		return false;
	}

	// MUSIN_SCROLL
	if (pkItemScroll->GetValue(0) == MUSIN_SCROLL)
	{
		
		//if (item->GetRefineLevel() >= 4)
		//{
		//	ChatPacket(CHAT_TYPE_INFO, "MAX +9 with this scroll!");
		//	return false;
		//}
	}
	// END_OF_MUSIC_SCROLL

	else if (pkItemScroll->GetValue(0) == MEMO_SCROLL)
	{
		if (item->GetRefineLevel() != pkItemScroll->GetValue(1))
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 417, "%s#%s", item->GetName(), pkItemScroll->GetName());
#endif
			return false;
		}
	}
	else if (pkItemScroll->GetValue(0) == BDRAGON_SCROLL)
	{
		if (item->GetType() != ITEM_METIN || item->GetRefineLevel() != 4)
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 665, "%s#%s", item->GetName(), pkItemScroll->GetName());
#endif
			return false;
		}
	}

	TItemTable* pProto = ITEM_MANAGER::instance().GetTable(item->GetRefinedVnum());

	if (!pProto)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 427, "");
#endif
		return false;
	}

	if (GetGold() < prt->cost)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 232, "");
#endif
#ifdef ENABLE_FEATURES_REFINE_SYSTEM
		CRefineManager::instance().Reset_percent(this);
#endif
		return false;
	}

	for (int i = 0; i < prt->material_count; ++i)
	{
		if (CountSpecifyItem(prt->materials[i].vnum) < prt->materials[i].count)
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 233, "");
#endif
			return false;
		}
	}

	for (int i = 0; i < prt->material_count; ++i)
		RemoveSpecifyItem(prt->materials[i].vnum, prt->materials[i].count);

	int prob = number(1, 100);
	int success_prob = prt->prob;
	bool bDestroyWhenFail = false;

	const char* szRefineType = "SCROLL";

	if (pkItemScroll->GetValue(0) == HYUNIRON_CHN ||
		pkItemScroll->GetValue(0) == YONGSIN_SCROLL ||
		pkItemScroll->GetValue(0) == YAGONG_SCROLL) // ÇöÃ¶, ¿ë½ÅÀÇ Ãàº¹¼­, ¾ß°øÀÇ ºñÀü¼­  Ã³¸®
	{
		const char hyuniron_prob[9] = { 100, 75, 65, 55, 45, 40, 35, 25, 20 };
		const char yagong_prob[9] = { 100, 100, 90, 80, 70, 60, 50, 30, 20 };

		if (pkItemScroll->GetValue(0) == YONGSIN_SCROLL)
		{
			success_prob = hyuniron_prob[MINMAX(0, item->GetRefineLevel(), 8)];
		}
		else if (pkItemScroll->GetValue(0) == YAGONG_SCROLL)
		{
			success_prob = yagong_prob[MINMAX(0, item->GetRefineLevel(), 8)];
		}
		else if (pkItemScroll->GetValue(0) == HYUNIRON_CHN) {} // @fixme121
		else
		{
			sys_err("REFINE : Unknown refine scroll item. Value0: %d", pkItemScroll->GetValue(0));
		}

		if (pkItemScroll->GetValue(0) == HYUNIRON_CHN) // ÇöÃ¶Àº ¾ÆÀÌÅÛÀÌ ºÎ¼­Á®¾ß ÇÑ´Ù.
			bDestroyWhenFail = true;

		// DETAIL_REFINE_LOG
		if (pkItemScroll->GetValue(0) == HYUNIRON_CHN)
		{
			szRefineType = "HYUNIRON";
		}
		else if (pkItemScroll->GetValue(0) == YONGSIN_SCROLL)
		{
			szRefineType = "GOD_SCROLL";
		}
		else if (pkItemScroll->GetValue(0) == YAGONG_SCROLL)
		{
			szRefineType = "YAGONG_SCROLL";
		}
		// END_OF_DETAIL_REFINE_LOG
	}
	// DETAIL_REFINE_LOG
	if (pkItemScroll->GetValue(0) == MUSIN_SCROLL)
	{
		
		success_prob += 100; // Musin izé mindig sikeres 
		if (success_prob > 100)
			success_prob = 100;

		szRefineType = "MUSIN_SCROLL";
	}
	// END_OF_DETAIL_REFINE_LOG
	else if (pkItemScroll->GetValue(0) == MEMO_SCROLL)
	{
		success_prob = 100;
		szRefineType = "MEMO_SCROLL";
	}
	else if (pkItemScroll->GetValue(0) == BDRAGON_SCROLL)
	{
		success_prob = 80;
		szRefineType = "BDRAGON_SCROLL";
	}

#ifdef ENABLE_FEATURES_REFINE_SYSTEM	
	success_prob += CRefineManager::instance().Result(this);

#endif
	pkItemScroll->SetCount(pkItemScroll->GetCount() - 1);

	if (prob <= success_prob)
	{
		// ¼º°ø! ¸ðµç ¾ÆÀÌÅÛÀÌ »ç¶óÁö°í, °°Àº ¼Ó¼ºÀÇ ´Ù¸¥ ¾ÆÀÌÅÛ È¹µæ
		LPITEM pkNewItem = ITEM_MANAGER::instance().CreateItem(result_vnum, 1, 0, false);

		if (pkNewItem)
		{
			ITEM_MANAGER::CopyAllAttrTo(item, pkNewItem);
			LogManager::instance().ItemLog(this, pkNewItem, "REFINE SUCCESS", pkNewItem->GetName());

			uint8_t bCell = item->GetCell();


#ifdef ENABLE_BATTLE_PASS
			uint8_t bBattlePassId = GetBattlePassId();
			if (bBattlePassId)
			{
				uint32_t dwItemVnum, dwCount;
				if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, REFINE_ITEM, &dwItemVnum, &dwCount))
				{
					if (dwItemVnum == item->GetVnum() && GetMissionProgress(REFINE_ITEM, bBattlePassId) < dwCount)
						UpdateMissionProgress(REFINE_ITEM, bBattlePassId, 1, dwCount);
				}
			}
#endif

			NotifyRefineSuccess(this, item, szRefineType);

			DBManager::instance().SendMoneyLog(MONEY_LOG_REFINE, item->GetVnum(), -prt->cost);
			ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (REFINE SUCCESS)");

			pkNewItem->AddToCharacter(this, TItemPos(INVENTORY, bCell));
			ITEM_MANAGER::instance().FlushDelayedSave(pkNewItem);



			pkNewItem->AttrLog();
			//PointChange(POINT_GOLD, -prt->cost);
#ifdef ENABLE_FEATURES_REFINE_SYSTEM
			CRefineManager::instance().Reset(this);
#endif
			PayRefineFee(prt->cost);
#ifdef ENABLE_UPGRADE_NOTICE_BY_RAZOR93
			if (pkNewItem->GetRefineLevel() >= 8)
			{
				char itemlink[512];
				int len = 0;

				len += snprintf(itemlink + len, sizeof(itemlink) - len, "item:%x:%x:%x:%x:%x:%x",
					pkNewItem->GetVnum(),
					pkNewItem->GetSocket(0),
					pkNewItem->GetSocket(1),
					pkNewItem->GetSocket(2),
					0, // transmute
					0  // transmute2 
				);

				// Bónuszok
				for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
				{
					uint8_t type = pkNewItem->GetAttributeType(i);
					short val = pkNewItem->GetAttributeValue(i);

					if (type != 0 && val != 0)
						len += snprintf(itemlink + len, sizeof(itemlink) - len, ":%x:%d", type, val);
				}

				// debug log:
				//sys_log(0, "ItemLink Debug: %s", itemlink);
				//sys_log(0, "Socket0=%d Socket1=%d Socket2=%d",
					//pkNewItem->GetSocket(0),
					//pkNewItem->GetSocket(1),
					//pkNewItem->GetSocket(2));

				char szChat[2048];
				snprintf(szChat, sizeof(szChat),
					"|cff00ff00[%s]|r Successfully upgraded:|cffffd700|H%s|h[%s]|h|r",
					GetName(), itemlink, pkNewItem->GetName());

				SPacketGGNotice packet;
				strlcpy(packet.szText, szChat, sizeof(packet.szText));
				//P2P_MANAGER::instance().Send(&packet, sizeof(packet));

				BroadcastNotice(szChat); // ez kell a jelenlegi ch-ra

			}


			if (allowedVnums.find(pkNewItem->GetVnum()) != allowedVnums.end())
			{
				char itemlink[512];
				int len = 0;

				len += snprintf(itemlink + len, sizeof(itemlink) - len, "item:%x:%x:%x:%x:%x:%x",
					pkNewItem->GetVnum(),
					pkNewItem->GetSocket(0),
					pkNewItem->GetSocket(1),
					pkNewItem->GetSocket(2),
					0, // transmute
					0  // transmute2 
				);

				for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
				{
					uint8_t type = pkNewItem->GetAttributeType(i);
					short val = pkNewItem->GetAttributeValue(i);

					if (type != 0 && val != 0)
						len += snprintf(itemlink + len, sizeof(itemlink) - len, ":%x:%d", type, val);
				}

				char szChat[2048];
				snprintf(szChat, sizeof(szChat),
					"|cff00ff00[%s]|r Successfully upgraded:|cffffd700|H%s|h[%s]|h|r",
					GetName(), itemlink, pkNewItem->GetName());

				ChatPacket(CHAT_TYPE_INFO, szChat);
			}
#endif ENABLE_UPGRADE_NOTICE_BY_RAZOR93
		}
		else
		{
			// ¾ÆÀÌÅÛ »ý¼º¿¡ ½ÇÆÐ -> °³·® ½ÇÆÐ·Î °£ÁÖ
			sys_err("cannot create item %u", result_vnum);
			NotifyRefineFail(this, item, szRefineType);
		}

	}
	else if (!bDestroyWhenFail && result_fail_vnum)
	{
		// ½ÇÆÐ! ¸ðµç ¾ÆÀÌÅÛÀÌ »ç¶óÁö°í, °°Àº ¼Ó¼ºÀÇ ³·Àº µî±ÞÀÇ ¾ÆÀÌÅÛ È¹µæ
		LPITEM pkNewItem = ITEM_MANAGER::instance().CreateItem(result_fail_vnum, 1, 0, false);

		if (pkNewItem)
		{
			ITEM_MANAGER::CopyAllAttrTo(item, pkNewItem);
			LogManager::instance().ItemLog(this, pkNewItem, "REFINE FAIL", pkNewItem->GetName());

			uint8_t bCell = item->GetCell();


#ifdef ENABLE_BATTLE_PASS
			uint8_t bBattlePassId = GetBattlePassId();
			if (bBattlePassId)
			{
				uint32_t dwItemVnum, dwCount;
				if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, REFINE_ITEM, &dwItemVnum, &dwCount))
				{
					if (dwItemVnum == item->GetVnum() && GetMissionProgress(REFINE_ITEM, bBattlePassId) < dwCount)
						UpdateMissionProgress(REFINE_ITEM, bBattlePassId, 1, dwCount);
				}
			}
#endif

			DBManager::instance().SendMoneyLog(MONEY_LOG_REFINE, item->GetVnum(), -prt->cost);
			NotifyRefineFail(this, item, szRefineType, -1);
			ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (REFINE FAIL)");

			pkNewItem->AddToCharacter(this, TItemPos(INVENTORY, bCell));
			ITEM_MANAGER::instance().FlushDelayedSave(pkNewItem);

			pkNewItem->AttrLog();

			//PointChange(POINT_GOLD, -prt->cost);
#ifdef ENABLE_FEATURES_REFINE_SYSTEM
			CRefineManager::instance().Reset(this);
#endif
			PayRefineFee(prt->cost);
		}
		else
		{
			// ¾ÆÀÌÅÛ »ý¼º¿¡ ½ÇÆÐ -> °³·® ½ÇÆÐ·Î °£ÁÖ
			sys_err("cannot create item %u", result_fail_vnum);
			NotifyRefineFail(this, item, szRefineType);
		}
	}
	else
	{
		NotifyRefineFail(this, item, szRefineType); // °³·®½Ã ¾ÆÀÌÅÛ »ç¶óÁöÁö ¾ÊÀ½

#ifdef ENABLE_FEATURES_REFINE_SYSTEM
		CRefineManager::instance().Reset(this);
#endif
		PayRefineFee(prt->cost);
	}

	return true;

}

#else

bool CHARACTER::DoRefineWithScroll(LPITEM item)
{
	if (!CanHandleItem(true))
	{
		ClearRefineMode();
		return false;
	}

	ClearRefineMode();

	//°³·® ½Ã°£Á¦ÇÑ : upgrade_refine_scroll.quest ¿¡¼­ °³·®ÈÄ 5ºÐÀÌ³»¿¡ ÀÏ¹Ý °³·®À»
	//ÁøÇàÇÒ¼ö ¾øÀ½
	if (quest::CQuestManager::instance().GetEventFlag("update_refine_time") != 0)
	{
		if (get_global_time() < quest::CQuestManager::instance().GetEventFlag("update_refine_time") + (60 * 5))
		{
			sys_log(0, "can't refine %d %s", GetPlayerID(), GetName());
			return false;
		}
	}

	const TRefineTable* prt = CRefineManager::instance().GetRefineRecipe(item->GetRefineSet());

	if (!prt)
		return false;

	LPITEM pkItemScroll;

	// °³·®¼­ Ã¼Å©
	if (m_iRefineAdditionalCell < 0)
		return false;

	pkItemScroll = GetInventoryItem(m_iRefineAdditionalCell);

	if (!pkItemScroll)
		return false;

	if (!(pkItemScroll->GetType() == ITEM_USE && pkItemScroll->GetSubType() == USE_TUNING))
		return false;

	if (pkItemScroll->GetVnum() == item->GetVnum())
		return false;

	uint32_t result_vnum = item->GetRefinedVnum();
	uint32_t result_fail_vnum = item->GetRefineFromVnum();

	if (result_vnum == 0)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 305, "");
#endif
		return false;
	}

	// MUSIN_SCROLL
	if (pkItemScroll->GetValue(0) == MUSIN_SCROLL)
	{
		if (item->GetRefineLevel() >= 4)
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 305, "");
#endif
			return false;
		}
	}
	// END_OF_MUSIC_SCROLL

	else if (pkItemScroll->GetValue(0) == MEMO_SCROLL)
	{
		if (item->GetRefineLevel() != pkItemScroll->GetValue(1))
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 417, "%s#%s", item->GetName(), pkItemScroll->GetName());
#endif
			return false;
		}
	}
	else if (pkItemScroll->GetValue(0) == BDRAGON_SCROLL)
	{
		if (item->GetType() != ITEM_METIN || item->GetRefineLevel() != 4)
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 665, "%s#%s", item->GetName(), pkItemScroll->GetName());
#endif
			return false;
		}
	}

	TItemTable* pProto = ITEM_MANAGER::instance().GetTable(item->GetRefinedVnum());

	if (!pProto)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 427, "");
#endif
		return false;
	}

	if (GetGold() < prt->cost)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 232, "");
#endif
#ifdef ENABLE_FEATURES_REFINE_SYSTEM
		CRefineManager::instance().Reset_percent(this);
#endif
		return false;
	}

	for (int i = 0; i < prt->material_count; ++i)
	{
		if (CountSpecifyItem(prt->materials[i].vnum) < prt->materials[i].count)
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 233, "");
#endif
			return false;
		}
	}

	for (int i = 0; i < prt->material_count; ++i)
		RemoveSpecifyItem(prt->materials[i].vnum, prt->materials[i].count);

	int prob = number(1, 100);
	int success_prob = prt->prob;
	bool bDestroyWhenFail = false;

	const char* szRefineType = "SCROLL";

	if (pkItemScroll->GetValue(0) == HYUNIRON_CHN ||
		pkItemScroll->GetValue(0) == YONGSIN_SCROLL ||
		pkItemScroll->GetValue(0) == YAGONG_SCROLL) // ÇöÃ¶, ¿ë½ÅÀÇ Ãàº¹¼­, ¾ß°øÀÇ ºñÀü¼­  Ã³¸®
	{
		const char hyuniron_prob[9] = { 100, 75, 65, 55, 45, 40, 35, 25, 20 };
		const char yagong_prob[9] = { 100, 100, 90, 80, 70, 60, 50, 30, 20 };

		if (pkItemScroll->GetValue(0) == YONGSIN_SCROLL)
		{
			success_prob = hyuniron_prob[MINMAX(0, item->GetRefineLevel(), 8)];
		}
		else if (pkItemScroll->GetValue(0) == YAGONG_SCROLL)
		{
			success_prob = yagong_prob[MINMAX(0, item->GetRefineLevel(), 8)];
		}
		else if (pkItemScroll->GetValue(0) == HYUNIRON_CHN) {} // @fixme121
		else
		{
			sys_err("REFINE : Unknown refine scroll item. Value0: %d", pkItemScroll->GetValue(0));
		}

		if (pkItemScroll->GetValue(0) == HYUNIRON_CHN) // ÇöÃ¶Àº ¾ÆÀÌÅÛÀÌ ºÎ¼­Á®¾ß ÇÑ´Ù.
			bDestroyWhenFail = true;

		// DETAIL_REFINE_LOG
		if (pkItemScroll->GetValue(0) == HYUNIRON_CHN)
		{
			szRefineType = "HYUNIRON";
		}
		else if (pkItemScroll->GetValue(0) == YONGSIN_SCROLL)
		{
			szRefineType = "GOD_SCROLL";
		}
		else if (pkItemScroll->GetValue(0) == YAGONG_SCROLL)
		{
			szRefineType = "YAGONG_SCROLL";
		}
		// END_OF_DETAIL_REFINE_LOG
	}

	// DETAIL_REFINE_LOG
	if (pkItemScroll->GetValue(0) == MUSIN_SCROLL) // ¹«½ÅÀÇ Ãàº¹¼­´Â 100% ¼º°ø (+4±îÁö¸¸)
	{
		success_prob = 100;

		szRefineType = "MUSIN_SCROLL";
	}
	// END_OF_DETAIL_REFINE_LOG
	else if (pkItemScroll->GetValue(0) == MEMO_SCROLL)
	{
		success_prob = 100;
		szRefineType = "MEMO_SCROLL";
	}
	else if (pkItemScroll->GetValue(0) == BDRAGON_SCROLL)
	{
		success_prob = 80;
		szRefineType = "BDRAGON_SCROLL";
	}

#ifdef ENABLE_FEATURES_REFINE_SYSTEM	
	success_prob += CRefineManager::instance().Result(this);

#endif
	pkItemScroll->SetCount(pkItemScroll->GetCount() - 1);

	if (prob <= success_prob)
	{
		// ¼º°ø! ¸ðµç ¾ÆÀÌÅÛÀÌ »ç¶óÁö°í, °°Àº ¼Ó¼ºÀÇ ´Ù¸¥ ¾ÆÀÌÅÛ È¹µæ
		LPITEM pkNewItem = ITEM_MANAGER::instance().CreateItem(result_vnum, 1, 0, false);

		if (pkNewItem)
		{
			ITEM_MANAGER::CopyAllAttrTo(item, pkNewItem);
			LogManager::instance().ItemLog(this, pkNewItem, "REFINE SUCCESS", pkNewItem->GetName());

			uint8_t bCell = item->GetCell();


#ifdef ENABLE_BATTLE_PASS
			uint8_t bBattlePassId = GetBattlePassId();
			if (bBattlePassId)
			{
				uint32_t dwItemVnum, dwCount;
				if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, REFINE_ITEM, &dwItemVnum, &dwCount))
				{
					if (dwItemVnum == item->GetVnum() && GetMissionProgress(REFINE_ITEM, bBattlePassId) < dwCount)
						UpdateMissionProgress(REFINE_ITEM, bBattlePassId, 1, dwCount);
				}
			}
#endif

			NotifyRefineSuccess(this, item, szRefineType);

			DBManager::instance().SendMoneyLog(MONEY_LOG_REFINE, item->GetVnum(), -prt->cost);
			ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (REFINE SUCCESS)");

			pkNewItem->AddToCharacter(this, TItemPos(INVENTORY, bCell));
			ITEM_MANAGER::instance().FlushDelayedSave(pkNewItem);



			pkNewItem->AttrLog();
			//PointChange(POINT_GOLD, -prt->cost);
#ifdef ENABLE_FEATURES_REFINE_SYSTEM
			CRefineManager::instance().Reset(this);
#endif
			PayRefineFee(prt->cost);
#ifdef ENABLE_UPGRADE_NOTICE_BY_RAZOR93
			if (pkNewItem->GetRefineLevel() >= 8)
			{
				char itemlink[512];
				int len = 0;

				len += snprintf(itemlink + len, sizeof(itemlink) - len, "item:%x:%x:%x:%x:%x:%x",
					pkNewItem->GetVnum(),
					pkNewItem->GetSocket(0),
					pkNewItem->GetSocket(1),
					pkNewItem->GetSocket(2),
					0, // transmute
					0  // transmute2 
				);

				// Bónuszok
				for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
				{
					uint8_t type = pkNewItem->GetAttributeType(i);
					short val = pkNewItem->GetAttributeValue(i);

					if (type != 0 && val != 0)
						len += snprintf(itemlink + len, sizeof(itemlink) - len, ":%x:%d", type, val);
				}

				// debug log:
				//sys_log(0, "ItemLink Debug: %s", itemlink);
				//sys_log(0, "Socket0=%d Socket1=%d Socket2=%d",
					//pkNewItem->GetSocket(0),
					//pkNewItem->GetSocket(1),
					//pkNewItem->GetSocket(2));

				char szChat[2048];
				snprintf(szChat, sizeof(szChat),
					"|cff00ff00[%s]|r Successfully upgraded:|cffffd700|H%s|h[%s]|h|r",
					GetName(), itemlink, pkNewItem->GetName());

				SPacketGGNotice packet;
				strlcpy(packet.szText, szChat, sizeof(packet.szText));
				//P2P_MANAGER::instance().Send(&packet, sizeof(packet));

				BroadcastNotice(szChat); // ez kell a jelenlegi ch-ra

			}


			if (allowedVnums.find(pkNewItem->GetVnum()) != allowedVnums.end())
			{
				char itemlink[512];
				int len = 0;

				len += snprintf(itemlink + len, sizeof(itemlink) - len, "item:%x:%x:%x:%x:%x:%x",
					pkNewItem->GetVnum(),
					pkNewItem->GetSocket(0),
					pkNewItem->GetSocket(1),
					pkNewItem->GetSocket(2),
					0, // transmute
					0  // transmute2 
				);

				for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
				{
					uint8_t type = pkNewItem->GetAttributeType(i);
					short val = pkNewItem->GetAttributeValue(i);

					if (type != 0 && val != 0)
						len += snprintf(itemlink + len, sizeof(itemlink) - len, ":%x:%d", type, val);
				}

				char szChat[2048];
				snprintf(szChat, sizeof(szChat),
					"|cff00ff00[%s]|r Successfully upgraded:|cffffd700|H%s|h[%s]|h|r",
					GetName(), itemlink, pkNewItem->GetName());

				ChatPacket(CHAT_TYPE_INFO, szChat);
			}
#endif ENABLE_UPGRADE_NOTICE_BY_RAZOR93
		}
		else
		{
			// ¾ÆÀÌÅÛ »ý¼º¿¡ ½ÇÆÐ -> °³·® ½ÇÆÐ·Î °£ÁÖ
			sys_err("cannot create item %u", result_vnum);
			NotifyRefineFail(this, item, szRefineType);
		}

	}
	else if (!bDestroyWhenFail && result_fail_vnum)
	{
		// ½ÇÆÐ! ¸ðµç ¾ÆÀÌÅÛÀÌ »ç¶óÁö°í, °°Àº ¼Ó¼ºÀÇ ³·Àº µî±ÞÀÇ ¾ÆÀÌÅÛ È¹µæ
		LPITEM pkNewItem = ITEM_MANAGER::instance().CreateItem(result_fail_vnum, 1, 0, false);

		if (pkNewItem)
		{
			ITEM_MANAGER::CopyAllAttrTo(item, pkNewItem);
			LogManager::instance().ItemLog(this, pkNewItem, "REFINE FAIL", pkNewItem->GetName());

			uint8_t bCell = item->GetCell();


#ifdef ENABLE_BATTLE_PASS
			uint8_t bBattlePassId = GetBattlePassId();
			if (bBattlePassId)
			{
				uint32_t dwItemVnum, dwCount;
				if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, REFINE_ITEM, &dwItemVnum, &dwCount))
				{
					if (dwItemVnum == item->GetVnum() && GetMissionProgress(REFINE_ITEM, bBattlePassId) < dwCount)
						UpdateMissionProgress(REFINE_ITEM, bBattlePassId, 1, dwCount);
				}
			}
#endif

			DBManager::instance().SendMoneyLog(MONEY_LOG_REFINE, item->GetVnum(), -prt->cost);
			NotifyRefineFail(this, item, szRefineType, -1);
			ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (REFINE FAIL)");

			pkNewItem->AddToCharacter(this, TItemPos(INVENTORY, bCell));
			ITEM_MANAGER::instance().FlushDelayedSave(pkNewItem);

			pkNewItem->AttrLog();

			//PointChange(POINT_GOLD, -prt->cost);
#ifdef ENABLE_FEATURES_REFINE_SYSTEM
			CRefineManager::instance().Reset(this);
#endif
			PayRefineFee(prt->cost);
		}
		else
		{
			// ¾ÆÀÌÅÛ »ý¼º¿¡ ½ÇÆÐ -> °³·® ½ÇÆÐ·Î °£ÁÖ
			sys_err("cannot create item %u", result_fail_vnum);
			NotifyRefineFail(this, item, szRefineType);
		}
	}
	else
	{
		NotifyRefineFail(this, item, szRefineType); // °³·®½Ã ¾ÆÀÌÅÛ »ç¶óÁöÁö ¾ÊÀ½

#ifdef ENABLE_FEATURES_REFINE_SYSTEM
		CRefineManager::instance().Reset(this);
#endif
		PayRefineFee(prt->cost);
	}

	return true;

}

#endif
#ifdef ENABLE_SOUL_SYSTEM
bool CHARACTER::DoRefineItemSoul(LPITEM item)
{
	if (!CanHandleItem(true))
	{
		ClearRefineMode();
		return false;
	}

	ClearRefineMode();

	LPITEM pkItemScroll;

	if (m_iRefineAdditionalCell < 0)
		return false;

	pkItemScroll = GetInventoryItem(m_iRefineAdditionalCell);

	if (!pkItemScroll)
		return false;

	if (!(pkItemScroll->GetType() == ITEM_USE && pkItemScroll->GetSubType() == USE_TUNING))
		return false;

	if (pkItemScroll->GetVnum() == item->GetVnum())
		return false;

	uint32_t resultVnum = item->GetRefinedVnum();

	if (resultVnum == 0)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 666, "%s", item->GetName());
#endif
		return false;
	}

	TItemTable* pProto = ITEM_MANAGER::instance().GetTable(item->GetRefinedVnum());

	if (!pProto)
	{
		sys_err("DoRefineWithScroll NOT GET ITEM PROTO %d", item->GetRefinedVnum());
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 305, "");
#endif
		return false;
	}

	int prob = number(1, 100);
	int successProb = pkItemScroll->GetValue(1);

	pkItemScroll->SetCount(pkItemScroll->GetCount() - 1);

	if (prob <= successProb)
	{
		LPITEM pkNewItem = ITEM_MANAGER::instance().CreateItem(resultVnum, 1, 0, false);
		if (pkNewItem)
		{
			uint8_t bCell = item->GetCell();
			ChatPacket(CHAT_TYPE_COMMAND, "RefineSoulSuceeded");
			ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (REFINE SUCCESS)");

			pkNewItem->AddToCharacter(this, TItemPos(INVENTORY, bCell));
			ITEM_MANAGER::instance().FlushDelayedSave(pkNewItem);
		}
		else
		{
			sys_err("Cannot create item soul %u", resultVnum);
			ChatPacket(CHAT_TYPE_COMMAND, "RefineSoulFailed");
		}
	}
	else
	{
		ChatPacket(CHAT_TYPE_COMMAND, "RefineSoulFailed");
	}

	return true;
}
#endif


bool CHARACTER::RefineInformation(uint8_t bCell, uint8_t bType, int iAdditionalCell)
{
	if (bCell > INVENTORY_MAX_NUM)
		return false;

	LPITEM item = GetInventoryItem(bCell);



	if (!item)
		return false;

#ifdef ATTR_LOCK
	if (item->GetLockedAttr() != -1)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 784, "");
#endif
		return false;
	}
#endif

	// REFINE_COST
	if (bType == REFINE_TYPE_MONEY_ONLY && !GetQuestFlag("deviltower_zone.can_refine"))
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 361, "");
#endif
		return false;
	}
	// END_OF_REFINE_COST

	TPacketGCRefineInformation p;

	p.header = HEADER_GC_REFINE_INFORMATION;
	p.pos = bCell;
	p.src_vnum = item->GetVnum();
	p.result_vnum = item->GetRefinedVnum();
	p.type = bType;

	if (p.result_vnum == 0)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 427, "");
#endif
		return false;
	}

	if (item->GetType() == ITEM_USE && item->GetSubType() == USE_TUNING)
	{
		if (bType == 0)
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 424, "");
#endif
			return false;
		}
		else
		{
			LPITEM itemScroll = GetInventoryItem(iAdditionalCell);
			if (!itemScroll || item->GetVnum() == itemScroll->GetVnum())
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 229, "");
#endif
				return false;
			}
		}
	}

#ifdef ENABLE_SOUL_SYSTEM
	if (bType == REFINE_TYPE_SOUL)
	{
		LPITEM itemScroll = GetInventoryItem(iAdditionalCell);
		if (!itemScroll)
			return false;

		p.cost = 0;
		p.prob = itemScroll->GetValue(1);
		p.material_count = 0;
		memset(p.materials, 0, sizeof(p.materials));

		GetDesc()->Packet(&p, sizeof(TPacketGCRefineInformation));

		SetRefineMode(iAdditionalCell);
		return true;
	}
#endif

	CRefineManager& rm = CRefineManager::instance();

	const TRefineTable* prt = rm.GetRefineRecipe(item->GetRefineSet());

	if (!prt)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 427, "");
#endif
		return false;
	}

	p.cost = ComputeRefineFee(prt->cost);
#ifdef NEW_POINT_EXP_DOUBLE_BONUS_RAZOR93
	int success_prob = prt->prob;

	// Kijelzett esély igazítása scroll típus alapján (hogy a kliens ugyanazt lássa, mint amit a szerver használ)
	if (bType != REFINE_TYPE_MONEY_ONLY)
	{
		LPITEM pkScroll = GetInventoryItem(iAdditionalCell);
		if (pkScroll && pkScroll->GetType() == ITEM_USE && pkScroll->GetSubType() == USE_TUNING)
		{
			const int scrollType = pkScroll->GetValue(0);

			if (scrollType == YONGSIN_SCROLL || scrollType == YAGONG_SCROLL || scrollType == HYUNIRON_CHN)
			{
				const char hyuniron_prob[9] = { 100, 75, 65, 55, 45, 40, 35, 25, 20 };
				const char yagong_prob[9] = { 100, 100, 90, 80, 70, 60, 50, 30, 20 };

				if (scrollType == YONGSIN_SCROLL)
					success_prob = hyuniron_prob[MINMAX(0, item->GetRefineLevel(), 8)];
				else if (scrollType == YAGONG_SCROLL)
					success_prob = yagong_prob[MINMAX(0, item->GetRefineLevel(), 8)];
				// HYUNIRON_CHN: marad a prt->prob
			}
			else if (scrollType == MUSIN_SCROLL)
			{
				//if (item->GetRefineLevel() >= 9)
				//{
				//	ChatPacket(CHAT_TYPE_INFO, "MAX +9 with this scroll!");
				//	return false;
				//}
				success_prob += 100;
				if (success_prob > 100)
					success_prob = 100;
			}
			else if (scrollType == MEMO_SCROLL)
			{
				if (item->GetRefineLevel() != pkScroll->GetValue(1))
					return false;
				success_prob = 100;
			}
			else if (scrollType == BDRAGON_SCROLL)
			{
				if (item->GetType() != ITEM_METIN || item->GetRefineLevel() != 4)
					return false;
				success_prob = 80;
			}
		}
	}

#ifdef ENABLE_FEATURES_REFINE_SYSTEM
	success_prob += CRefineManager::instance().Result(this);
#endif

	success_prob = MINMAX(0, success_prob, 100);
	p.prob = success_prob;
#else
	p.prob = prt->prob;
#endif
	if (bType == REFINE_TYPE_MONEY_ONLY)
	{
		p.material_count = 0;
		memset(p.materials, 0, sizeof(p.materials));
	}
	else
	{
		p.material_count = prt->material_count;
		memcpy(&p.materials, prt->materials, sizeof(prt->materials));
	}

	GetDesc()->Packet(&p, sizeof(TPacketGCRefineInformation));

	SetRefineMode(iAdditionalCell);
	return true;
}

bool CHARACTER::RefineItem(LPITEM pkItem, LPITEM pkTarget)
{
	if (!CanHandleItem())
		return false;

#ifdef ENABLE_SOUL_SYSTEM
	uint32_t vnum = pkItem->GetVnum();
	if ((vnum == 70602 || vnum == 70603 || vnum == 88958) && pkTarget->GetType() != ITEM_SOUL) {
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 1294, "%s", pkItem->GetName());
#endif
		return false;
	}
#endif

	if (pkItem->GetSubType() == USE_TUNING)
	{
		// XXX ¼º´É, ¼ÒÄÏ °³·®¼­´Â »ç¶óÁ³½À´Ï´Ù...
		// XXX ¼º´É°³·®¼­´Â Ãàº¹ÀÇ ¼­°¡ µÇ¾ú´Ù!
		// MUSIN_SCROLL
		if (pkItem->GetValue(0) == MUSIN_SCROLL)
			RefineInformation(pkTarget->GetCell(), REFINE_TYPE_MUSIN, pkItem->GetCell());
		// END_OF_MUSIN_SCROLL

#ifdef ENABLE_SOUL_SYSTEM
		else if (pkItem->GetValue(0) == SOUL_SCROLL)
			RefineInformation(pkTarget->GetCell(), REFINE_TYPE_SOUL, pkItem->GetCell());
#endif

		else if (pkItem->GetValue(0) == HYUNIRON_CHN)
			RefineInformation(pkTarget->GetCell(), REFINE_TYPE_HYUNIRON, pkItem->GetCell());
		else if (pkItem->GetValue(0) == BDRAGON_SCROLL)
		{
			if (pkTarget->GetRefineSet() != 702) return false;
			RefineInformation(pkTarget->GetCell(), REFINE_TYPE_BDRAGON, pkItem->GetCell());
		}
		else
		{
			if (pkTarget->GetRefineSet() == 501) return false;
			RefineInformation(pkTarget->GetCell(), REFINE_TYPE_SCROLL, pkItem->GetCell());
		}
	}
	else if (pkItem->GetSubType() == USE_DETACHMENT && IS_SET(pkTarget->GetFlag(), ITEM_FLAG_REFINEABLE))
	{
		LogManager::instance().ItemLog(this, pkTarget, "USE_DETACHMENT", pkTarget->GetName());

		bool bHasMetinStone = false;

		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; i++)
		{
			int32_t socket = pkTarget->GetSocket(i);
			if (socket > 2 && socket != ITEM_BROKEN_METIN_VNUM)
			{
				bHasMetinStone = true;
				break;
			}
		}

		if (bHasMetinStone)
		{
			for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
			{
				int32_t socket = pkTarget->GetSocket(i);
				if (socket > 2 && socket != ITEM_BROKEN_METIN_VNUM)
				{
					AutoGiveItem(socket);
					//TItemTable* pTable = ITEM_MANAGER::instance().GetTable(pkTarget->GetSocket(i));
					//pkTarget->SetSocket(i, pTable->alValues[2]);
					// ±úÁøµ¹·Î ´ëÃ¼ÇØÁØ´Ù
					pkTarget->SetSocket(i, ITEM_BROKEN_METIN_VNUM);
				}
			}
			pkItem->SetCount(pkItem->GetCount() - 1);
			return true;
		}
		else
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 360, "");
#endif
			return false;
		}
	}

	return false;
}

bool CHARACTER::GiveRecallItem(LPITEM item)
{
	int idx = GetMapIndex();
	int iEmpireByMapIndex = -1;

	if (idx < 20)
		iEmpireByMapIndex = 1;
	else if (idx < 40)
		iEmpireByMapIndex = 2;
	else if (idx < 60)
		iEmpireByMapIndex = 3;
	else if (idx < 10000)
		iEmpireByMapIndex = 0;

	switch (idx)
	{
	case 66:
	case 216:
		iEmpireByMapIndex = -1;
		break;
	}

	if (iEmpireByMapIndex && GetEmpire() != iEmpireByMapIndex)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 270, "");
#endif
		return false;
	}

	int pos;

	if (item->GetCount() == 1)	// ¾ÆÀÌÅÛÀÌ ÇÏ³ª¶ó¸é ±×³É ¼ÂÆÃ.
	{
		item->SetSocket(0, GetX());
		item->SetSocket(1, GetY());
	}
	else if ((pos = GetEmptyInventory(item->GetSize())) != -1) // ±×·¸Áö ¾Ê´Ù¸é ´Ù¸¥ ÀÎº¥Åä¸® ½½·ÔÀ» Ã£´Â´Ù.
	{
		LPITEM item2 = ITEM_MANAGER::instance().CreateItem(item->GetVnum(), 1);

		if (nullptr != item2)
		{
			item2->SetSocket(0, GetX());
			item2->SetSocket(1, GetY());
			item2->AddToCharacter(this, TItemPos(INVENTORY, pos));

			item->SetCount(item->GetCount() - 1);
		}
	}
	else
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 366, "");
#endif
		return false;
	}

	return true;
}

void CHARACTER::ProcessRecallItem(LPITEM item)
{
	int idx;

	if ((idx = SECTREE_MANAGER::instance().GetMapIndex(item->GetSocket(0), item->GetSocket(1))) == 0)
		return;

	int iEmpireByMapIndex = -1;

	if (idx < 20)
		iEmpireByMapIndex = 1;
	else if (idx < 40)
		iEmpireByMapIndex = 2;
	else if (idx < 60)
		iEmpireByMapIndex = 3;
	else if (idx < 10000)
		iEmpireByMapIndex = 0;

	switch (idx)
	{
	case 66:
	case 216:
		iEmpireByMapIndex = -1;
		break;
		// ¾Ç·æ±ºµµ ÀÏ¶§
	case 301:
	case 302:
	case 303:
	case 304:
		if (GetLevel() < 90)
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 325, "%d", 90);
#endif
			return;
		}
		else
			break;
	}

	if (iEmpireByMapIndex && GetEmpire() != iEmpireByMapIndex)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 270, "");
#endif
		item->SetSocket(0, 0);
		item->SetSocket(1, 0);
	}
	else
	{
		sys_log(1, "Recall: %s %d %d -> %d %d", GetName(), GetX(), GetY(), item->GetSocket(0), item->GetSocket(1));
		WarpSet(item->GetSocket(0), item->GetSocket(1));
		item->SetCount(item->GetCount() - 1);
	}
}

void CHARACTER::__OpenPrivateShop(
#ifdef KASMIR_PAKET_SYSTEM
	bool bKasmir
#endif
)
{
#ifdef ENABLE_OPEN_SHOP_WITH_ARMOR
#ifdef KASMIR_PAKET_SYSTEM
	if (bKasmir) {
		ChatPacket(CHAT_TYPE_COMMAND, "OpenPrivateShopKasmir");
		return;
	}
#endif
	ChatPacket(CHAT_TYPE_COMMAND, "OpenPrivateShop");
#else
	unsigned bodyPart = GetPart(PART_MAIN);
	switch (bodyPart)
	{
	case 0:
	case 1:
	case 2: {
#ifdef KASMIR_PAKET_SYSTEM
		if (bKasmir) {
			ChatPacket(CHAT_TYPE_COMMAND, "OpenPrivateShopKasmir");
			break;
		}
#endif

		ChatPacket(CHAT_TYPE_COMMAND, "OpenPrivateShop");
	}
		  break;
	default:
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 503, "");
#endif
		break;
	}
#endif
}

// MYSHOP_PRICE_LIST

void CHARACTER::SendMyShopPriceListCmd(uint32_t dwItemVnum, int64_t dwItemPrice)
{
	char szLine[256];
	snprintf(szLine, sizeof(szLine), "MyShopPriceList %u %lld", dwItemVnum, dwItemPrice);
	ChatPacket(CHAT_TYPE_COMMAND, szLine);
	sys_log(0, szLine);
}


//
// DB Ä³½Ã·Î ºÎÅÍ ¹ÞÀº ¸®½ºÆ®¸¦ User ¿¡°Ô Àü¼ÛÇÏ°í »óÁ¡À» ¿­¶ó´Â Ä¿¸Çµå¸¦ º¸³½´Ù.
//
void CHARACTER::UseSilkBotaryReal(const TPacketMyshopPricelistHeader * p)
{
	const TItemPriceInfo* pInfo = (const TItemPriceInfo*)(p + 1);

	if (!p->byCount)
		// °¡°Ý ¸®½ºÆ®°¡ ¾ø´Ù. dummy µ¥ÀÌÅÍ¸¦ ³ÖÀº Ä¿¸Çµå¸¦ º¸³»ÁØ´Ù.
		SendMyShopPriceListCmd(1, 0);
	else {
		for (int idx = 0; idx < p->byCount; idx++)
			SendMyShopPriceListCmd(pInfo[idx].dwVnum, pInfo[idx].dwPrice);
	}

#ifdef KASMIR_PAKET_SYSTEM
	__OpenPrivateShop(m_bKasmirPaketDurum);
#else
	__OpenPrivateShop();
#endif
}

//
// ÀÌ¹ø Á¢¼Ó ÈÄ Ã³À½ »óÁ¡À» Open ÇÏ´Â °æ¿ì ¸®½ºÆ®¸¦ Load ÇÏ±â À§ÇØ DB Ä³½Ã¿¡ °¡°ÝÁ¤º¸ ¸®½ºÆ® ¿äÃ» ÆÐÅ¶À» º¸³½´Ù.
// ÀÌÈÄºÎÅÍ´Â ¹Ù·Î »óÁ¡À» ¿­¶ó´Â ÀÀ´äÀ» º¸³½´Ù.
//
void CHARACTER::UseSilkBotary(void)
{
	if (m_bNoOpenedShop) {
		uint32_t dwPlayerID = GetPlayerID();
		db_clientdesc->DBPacket(HEADER_GD_MYSHOP_PRICELIST_REQ, GetDesc()->GetHandle(), &dwPlayerID, sizeof(uint32_t));
		m_bNoOpenedShop = false;
	}
	else {
#ifdef KASMIR_PAKET_SYSTEM
		__OpenPrivateShop(m_bKasmirPaketDurum);
#else
		__OpenPrivateShop();
#endif
	}
}
// END_OF_MYSHOP_PRICE_LIST


EVENTFUNC(kill_campfire_event)
{
	char_event_info* info = dynamic_cast<char_event_info*>(event->info);

	if (info == nullptr)
	{
		sys_err("kill_campfire_event> <Factor> Null pointer");
		return 0;
	}

	LPCHARACTER	ch = info->ch;

	if (ch == nullptr) { // <Factor>
		return 0;
	}
	ch->m_pkMiningEvent = nullptr;
	M2_DESTROY_CHARACTER(ch);
	return 0;
}

int CalculateConsume(LPCHARACTER ch)
{
	static const int WARP_NEED_LIFE_PERCENT = 30;
	static const int WARP_MIN_LIFE_PERCENT = 10;
	// CONSUME_LIFE_WHEN_USE_WARP_ITEM
	int consumeLife = 0;
	{
		// CheckNeedLifeForWarp
		const int curLife = ch->GetHP();
		const int needPercent = WARP_NEED_LIFE_PERCENT;
		const int needLife = ch->GetMaxHP() * needPercent / 100;
		if (curLife < needLife)
		{
#ifdef TEXTS_IMPROVEMENT
			if (ch) {
				ch->ChatPacketNew(CHAT_TYPE_INFO, 284, "");
			}
#endif
			return -1;
		}

		consumeLife = needLife;


		// CheckMinLifeForWarp: µ¶¿¡ ÀÇÇØ¼­ Á×À¸¸é ¾ÈµÇ¹Ç·Î »ý¸í·Â ÃÖ¼Ò·®´Â ³²°ÜÁØ´Ù
		const int minPercent = WARP_MIN_LIFE_PERCENT;
		const int minLife = ch->GetMaxHP() * minPercent / 100;
		if (curLife - needLife < minLife)
			consumeLife = curLife - minLife;

		if (consumeLife < 0)
			consumeLife = 0;
	}
	// END_OF_CONSUME_LIFE_WHEN_USE_WARP_ITEM
	return consumeLife;
}

int CalculateConsumeSP(LPCHARACTER lpChar)
{
	static const int NEED_WARP_SP_PERCENT = 30;

	const int curSP = lpChar->GetSP();
	const int needSP = lpChar->GetMaxSP() * NEED_WARP_SP_PERCENT / 100;

	if (curSP < needSP)
	{
#ifdef TEXTS_IMPROVEMENT
		if (lpChar) {
			lpChar->ChatPacketNew(CHAT_TYPE_INFO, 287, "");
		}
#endif
		return -1;
	}

	return needSP;
}

// #define ENABLE_FIREWORK_STUN
#define ENABLE_ADDSTONE_FAILURE
bool CHARACTER::UseItemEx(LPITEM item, TItemPos DestCell)
{
	int iLimitRealtimeStartFirstUseFlagIndex = -1;
	//int iLimitTimerBasedOnWearFlagIndex = -1;

	uint16_t wDestCell = DestCell.cell;
	uint8_t bDestInven = DestCell.window_type;
	for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
	{
		int32_t limitValue = item->GetProto()->aLimits[i].lValue;

		switch (item->GetProto()->aLimits[i].bType)
		{
		case LIMIT_LEVEL:
			if (GetLevel() < limitValue)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 325, "%d", limitValue);
#endif
				return false;
			}
			break;

		case LIMIT_REAL_TIME_START_FIRST_USE:
			iLimitRealtimeStartFirstUseFlagIndex = i;
			break;

		case LIMIT_TIMER_BASED_ON_WEAR:
			//iLimitTimerBasedOnWearFlagIndex = i;
			break;
		}
	}

	if (test_server)
	{
		sys_log(0, "USE_ITEM %s, Inven %d, Cell %d, ItemType %d, SubType %d", item->GetName(), bDestInven, wDestCell, item->GetType(), item->GetSubType());
	}

	if (CArenaManager::instance().IsLimitedItem(GetMapIndex(), item->GetVnum()) == true)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 667, "");
#endif
		return false;
	}
#ifdef ENABLE_NEWSTUFF
	else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(GetPlayerID()) && IsLimitedPotionOnPVP(item->GetVnum()))
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 667, "");
#endif
		return false;
	}
#endif

	// @fixme402 (IsLoadedAffect to block affect hacking)
	if (!IsLoadedAffect()) {
		return false;
	}

	// @fixme141 BEGIN
/* 	if (TItemPos(item->GetWindow(), item->GetCell()).IsBeltInventoryPosition())// @Razor93 GetWear(WEAR_BELT); ne legyen szukseges a wear_mount_costume hez
	{
		LPITEM beltItem = GetWear(WEAR_BELT);

		if (NULL == beltItem)
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 785, "");
#endif
			return false;
		}

		if (false == CBeltInventoryHelper::IsAvailableCell(item->GetCell() - BELT_INVENTORY_SLOT_START, beltItem->GetValue(0)))
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 786, "");
#endif
			return false;
		}
	} */
	// @fixme141 END

	// ¾ÆÀÌÅÛ ÃÖÃÊ »ç¿ë ÀÌÈÄºÎÅÍ´Â »ç¿ëÇÏÁö ¾Ê¾Æµµ ½Ã°£ÀÌ Â÷°¨µÇ´Â ¹æ½Ä Ã³¸®.
	if (-1 != iLimitRealtimeStartFirstUseFlagIndex)
	{
		// ÇÑ ¹øÀÌ¶óµµ »ç¿ëÇÑ ¾ÆÀÌÅÛÀÎÁö ¿©ºÎ´Â Socket1À» º¸°í ÆÇ´ÜÇÑ´Ù. (Socket1¿¡ »ç¿ëÈ½¼ö ±â·Ï)
		if (0 == item->GetSocket(1))
		{
			// »ç¿ë°¡´É½Ã°£Àº Default °ªÀ¸·Î Limit Value °ªÀ» »ç¿ëÇÏµÇ, Socket0¿¡ °ªÀÌ ÀÖÀ¸¸é ±× °ªÀ» »ç¿ëÇÏµµ·Ï ÇÑ´Ù. (´ÜÀ§´Â ÃÊ)
			int32_t duration = (0 != item->GetSocket(0)) ? item->GetSocket(0) : item->GetProto()->aLimits[iLimitRealtimeStartFirstUseFlagIndex].lValue;

			if (0 == duration)
				duration = 60 * 60 * 24 * 7;

			item->SetSocket(0, time(nullptr) + duration);
			item->StartRealTimeExpireEvent();
		}

		if (false == item->IsEquipped())
			item->SetSocket(1, item->GetSocket(1) + 1);
	}

#ifdef __NEWPET_SYSTEM__
	if (item->GetVnum() == 55001)
	{

		LPITEM item2;

		if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
			return false;

		if (item2->IsExchanging() || item2->IsEquipped()) // ENABLE_BUG_FIXES
			return false;

		if (item2->GetVnum() > 55711 || item2->GetVnum() < 55701)
			return false;


		char szQuery1[1024];
		snprintf(szQuery1, sizeof(szQuery1), "SELECT duration FROM new_petsystem WHERE id = %d LIMIT 1", item2->GetID());
		std::unique_ptr<SQLMsg> pmsg2(DBManager::instance().DirectQuery(szQuery1));
		if (pmsg2->Get()->uiNumRows > 0) {
			MYSQL_ROW row = mysql_fetch_row(pmsg2->Get()->pSQLResult);
			if (atoi(row[0]) > 0) {
				if (GetNewPetSystem()->IsActivePet()) {
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 787, "");
#endif
					return false;
				}

				std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("UPDATE new_petsystem SET duration =(tduration) WHERE id = %d", item2->GetID()));
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 788, "");
#endif
			}
			else {
				std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("UPDATE new_petsystem SET duration =(tduration/2) WHERE id = %d", item2->GetID()));
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 788, "");
#endif
			}
			item->SetCount(item->GetCount() - 1);
			return true;
		}
		else
			return false;
	}

	if (item->GetVnum() >= 55701 && item->GetVnum() <= 55711) {
		LPITEM box = GetItem(DestCell);
		if (box) {
			if (item->GetSocket(1) == 0) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 858, "");
#endif
				return false;
			}

			if (box->GetSocket(0) != 0) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 853, "%s", box->GetName());
#endif
				return false;
			}
			else {
				if (item->GetSocket(0) == true) {
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 854, "");
#endif
					return false;
				}
				else {
					char query[1024];
					snprintf(query, sizeof(query), "SELECT level"
#ifdef ENABLE_NEW_PET_EDITS
						", minAge "
#endif
						", evolution, bonus0, bonus1, bonus2, skill0, skill0lv, skill1, skill1lv, skill2, skill2lv, skill3, skill3lv FROM player.new_petsystem WHERE id = %d", item->GetID());
					std::unique_ptr<SQLMsg> pmsg(DBManager::instance().DirectQuery(query));
					if (pmsg->Get()->uiNumRows > 0)
					{
						MYSQL_ROW row = mysql_fetch_row(pmsg->Get()->pSQLResult);
						uint32_t evolution = atoi(row[2]);
						uint32_t petVnum = 0;
						switch (item->GetVnum()) {
						case 55701:
							petVnum = evolution == 3 ? 34042 : 34041;
							break;
						case 55702:
							petVnum = evolution == 3 ? 34046 : 34045;
							break;
						case 55703:
							petVnum = evolution == 3 ? 34050 : 34049;
							break;
						case 55704:
							petVnum = evolution == 3 ? 34054 : 34053;
							break;
						case 55705:
							petVnum = evolution == 3 ? 34037 : 34036;
							break;
						case 55706:
							petVnum = evolution == 3 ? 34065 : 34064;
							break;
						case 55707:
							petVnum = evolution == 3 ? 34074 : 34073;
							break;
						case 55708:
							petVnum = evolution == 3 ? 34076 : 34075;
							break;
						case 55709:
							petVnum = evolution == 3 ? 34081 : 34080;
							break;
						case 55710:
							petVnum = evolution == 3 ? 34083 : 34082;
							break;
						case 55711:
							petVnum = evolution == 3 ? 34096 : 34095;
							break;
						default:
							break;
						}

						if (petVnum == 0) {
							return false;
						}

						box->SetSocket(1, item->GetID());
						box->SetSocket(0, petVnum);
						ITEM_MANAGER::instance().RemoveItem(item);
#ifdef ENABLE_NEW_PET_EDITS
						box->SetSocket(2, atoi(row[1]));
#endif
						uint8_t res1 = atoi(row[0]);
						uint8_t res2 = atoi(row[2]);
						uint8_t res3 = atoi(row[3]);
						uint8_t res4 = atoi(row[4]);
						box->SetForceAttribute(0, res1, res2);
						box->SetForceAttribute(1, res3, res4);
						uint8_t dwskill1 = atoi(row[6]) == -1 ? 255 : atoi(row[6]), dwskilllv1 = atoi(row[7]);
						box->SetForceAttribute(2, atoi(row[5]), dwskill1);
						uint8_t dwskill2 = atoi(row[8]) == -1 ? 255 : atoi(row[8]), dwskilllv2 = atoi(row[9]);
						box->SetForceAttribute(3, dwskilllv1, dwskill2);
						uint8_t dwskill3 = atoi(row[10]) == -1 ? 255 : atoi(row[10]), dwskilllv3 = atoi(row[11]);
						box->SetForceAttribute(4, dwskilllv2, dwskill3);
						uint8_t dwskill4 = atoi(row[12]) == -1 ? 255 : atoi(row[12]), dwskilllv4 = atoi(row[13]);
						box->SetForceAttribute(5, dwskilllv3, dwskill4);
						box->SetForceAttribute(6, dwskilllv4, 1);
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 855, "%s", box->GetName());
#endif
						return true;
					}
					else {
						return false;
					}
				}
			}
		}
	}
	else if (item->GetVnum() == 55002) {
		if (item->GetSocket(0) != 0) {
			uint32_t itemVnum = 0;
			switch (item->GetSocket(0)) {
			case 34041:
			case 34042:
				itemVnum = 55701;
				break;
			case 34045:
			case 34046:
				itemVnum = 55702;
				break;
			case 34049:
			case 34050:
				itemVnum = 55703;
				break;
			case 34053:
			case 34054:
				itemVnum = 55704;
				break;
			case 34036:
			case 34037:
				itemVnum = 55705;
				break;
			case 34064:
			case 34065:
				itemVnum = 55706;
				break;
			case 34073:
			case 34074:
				itemVnum = 55707;
				break;
			case 34075:
			case 34076:
				itemVnum = 55708;
				break;
			case 34080:
			case 34081:
				itemVnum = 55709;
				break;
			case 34082:
			case 34083:
				itemVnum = 55710;
				break;
			case 34095:
			case 34096:
				itemVnum = 55711;
				break;
			default:
				break;
			}

			if (itemVnum == 0) {
				return false;
			}

			LPITEM petItem = AutoGiveItem(itemVnum, 1);
			if (!petItem) {
				return false;
			}

			petItem->SetSocket(0, 0);
			petItem->SetForceAttribute(0, 1, item->GetAttributeType(1));
			petItem->SetForceAttribute(1, 1, item->GetAttributeValue(1));
			petItem->SetForceAttribute(2, 1, item->GetAttributeType(2));

			char query[256];
			snprintf(query, sizeof(query), "SELECT tduration FROM player.new_petsystem WHERE id = %ld", item->GetSocket(1));
			std::unique_ptr<SQLMsg> pmsg(DBManager::instance().DirectQuery(query));
			if (pmsg->Get()->uiNumRows > 0) {
				MYSQL_ROW row = mysql_fetch_row(pmsg->Get()->pSQLResult);
#ifdef ENABLE_NEW_PET_EDITS
				petItem->SetSocket(1, atoi(row[0]));
				petItem->SetSocket(2, atoi(row[0]));
#else
				petItem->SetForceAttribute(3, 1, atoi(row[0]));
				petItem->SetForceAttribute(4, 1, atoi(row[0]));
#endif
			}
#ifdef ENABLE_NEW_PET_EDITS
			petItem->SetForceAttribute(3, 1, item->GetAttributeType(0));
#else
			petItem->SetSocket(1, item->GetAttributeType(0));
#endif
			std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("UPDATE player.new_petsystem SET id = %d WHERE id = %ld", petItem->GetID(), item->GetSocket(1)));
			ITEM_MANAGER::instance().RemoveItem(item);
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 857, "%s", item->GetName());
			return true;
#endif
		}
		else {
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 856, "%s", item->GetName());
#endif
			return false;
		}
	}
#endif

	// 30001: Teszt klonok torlese (dungeon instance-ben blokkolva)
	//switch (item->GetVnum())
	//{
	//	case 30001: return CLostCastleDungeon::instance().OnUseItem30001(this);
	//	default: break;
	//}

#ifdef ENABLE_CPP_DUNGEON_RAZOR93
	switch (item->GetVnum())
	{
	case 89103: return CRuneDungeon::instance().OnUseItem89103(this);
	case 89102: return CRuneDungeon::instance().OnUseItem89102(this);
	case 89100: return CRuneDungeon::instance().OnUseItem89100(this);
	default: break;
	}
#endif

	switch (item->GetType())
	{
#ifdef ENABLE_ITEMSHOP_ITEM
	case ITEM_TYPE_ISHOP:
	{
		uint32_t vnum = item->GetSocket(0);
		if (vnum == 0) {
			return false;
		}

		LPITEM reward = AutoGiveItem(vnum, 1);
		if (!reward) {
			return false;
		}

		item->SetCount(item->GetCount() - 1);
		return true;
	}
	break;
#endif
	case ITEM_HAIR:
		return ItemProcess_Hair(item, wDestCell);

	case ITEM_POLYMORPH:
		return ItemProcess_Polymorph(item);

	case ITEM_QUEST:
		if (GetArena() != nullptr || IsObserverMode() == true)
		{
			if (item->GetVnum() == 50051 || item->GetVnum() == 50052 || item->GetVnum() == 50053)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 667, "");
#endif
				return false;
			}
		}

		if (!IS_SET(item->GetFlag(), ITEM_FLAG_QUEST_USE | ITEM_FLAG_QUEST_USE_MULTIPLE))
		{
			if (item->GetSIGVnum() == 0)
			{
				quest::CQuestManager::instance().UseItem(GetPlayerID(), item, false);
			}
			else
			{
				quest::CQuestManager::instance().SIGUse(GetPlayerID(), item->GetSIGVnum(), item, false);
			}
		}

#ifdef __AUTO_QUQUE_ATTACK__
		if (item->GetVnum() >= 61400 && item->GetVnum() <= 61405)
		{
			if (item->isLocked() || item->IsExchanging())
				return false;

			if (FindAffect(AFFECT_AUTO_METIN_FARM)) {
				ChatPacket(CHAT_TYPE_INFO, "You has already affect.");
				return false;
			}
			ChatPacket(CHAT_TYPE_INFO, "Affect successfully added.");
			AddAffect(AFFECT_AUTO_METIN_FARM, 0, 0, AFF_NONE, item->GetValue(0) == 999 ? INFINITE_AFFECT_DURATION : 60 * 60 * 24 * item->GetValue(0), 0, false);
			item->SetCount(item->GetCount() - 1);
			return true;
		}
#endif
		break;

	case ITEM_CAMPFIRE:
	{
		float fx, fy;
		GetDeltaByDegree(GetRotation(), 100.0f, &fx, &fy);

		LPSECTREE tree = SECTREE_MANAGER::instance().Get(GetMapIndex(), (int32_t)(GetX() + fx), (int32_t)(GetY() + fy));

		if (!tree)
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 344, "");
#endif
			return false;
		}

		if (tree->IsAttr((int32_t)(GetX() + fx), (int32_t)(GetY() + fy), ATTR_WATER))
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 346, "");
#endif
			return false;
		}

#ifdef ENABLE_BUG_FIXES
		if (get_global_time() - GetQuestFlag("kamp.spawned") < 60) {
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 1246, "");
#endif
			return false;
		}
		else {
			SetQuestFlag("kamp.spawned", get_global_time());
		}
#endif

		LPCHARACTER campfire = CHARACTER_MANAGER::instance().SpawnMob(fishing::CAMPFIRE_MOB, GetMapIndex(), (int32_t)(GetX() + fx), (int32_t)(GetY() + fy), 0, false, number(0, 359));

		char_event_info* info = AllocEventInfo<char_event_info>();

		info->ch = campfire;

		campfire->m_pkMiningEvent = event_create(kill_campfire_event, info, PASSES_PER_SEC(40));

		item->SetCount(item->GetCount() - 1);
	}
	break;

	case ITEM_UNIQUE:
	{
		switch (item->GetSubType())
		{
		case USE_ABILITY_UP:
		{
			switch (item->GetValue(0))
			{
			case APPLY_MOV_SPEED:
				AddAffect(AFFECT_UNIQUE_ABILITY, POINT_MOV_SPEED, item->GetValue(2), AFF_MOV_SPEED_POTION, item->GetValue(1), 0, true, true);
				break;

			case APPLY_ATT_SPEED:
				AddAffect(AFFECT_UNIQUE_ABILITY, POINT_ATT_SPEED, item->GetValue(2), AFF_ATT_SPEED_POTION, item->GetValue(1), 0, true, true);
				break;

			case APPLY_STR:
				AddAffect(AFFECT_UNIQUE_ABILITY, POINT_ST, item->GetValue(2), 0, item->GetValue(1), 0, true, true);
				break;

			case APPLY_DEX:
				AddAffect(AFFECT_UNIQUE_ABILITY, POINT_DX, item->GetValue(2), 0, item->GetValue(1), 0, true, true);
				break;

			case APPLY_CON:
				AddAffect(AFFECT_UNIQUE_ABILITY, POINT_HT, item->GetValue(2), 0, item->GetValue(1), 0, true, true);
				break;

			case APPLY_INT:
				AddAffect(AFFECT_UNIQUE_ABILITY, POINT_IQ, item->GetValue(2), 0, item->GetValue(1), 0, true, true);
				break;

			case APPLY_CAST_SPEED:
				AddAffect(AFFECT_UNIQUE_ABILITY, POINT_CASTING_SPEED, item->GetValue(2), 0, item->GetValue(1), 0, true, true);
				break;

			case APPLY_RESIST_MAGIC:
				AddAffect(AFFECT_UNIQUE_ABILITY, POINT_RESIST_MAGIC, item->GetValue(2), 0, item->GetValue(1), 0, true, true);
				break;

			case APPLY_ATT_GRADE_BONUS:
				AddAffect(AFFECT_UNIQUE_ABILITY, POINT_ATT_GRADE_BONUS,
					item->GetValue(2), 0, item->GetValue(1), 0, true, true);
				break;

			case APPLY_DEF_GRADE_BONUS:
				AddAffect(AFFECT_UNIQUE_ABILITY, POINT_DEF_GRADE_BONUS,
					item->GetValue(2), 0, item->GetValue(1), 0, true, true);
				break;
			}
		}

		if (GetWarMap())
			GetWarMap()->UsePotion(this, item);

		item->SetCount(item->GetCount() - 1);
		break;

		default:
		{
			if (item->GetSubType() == USE_SPECIAL)
			{
				sys_log(0, "ITEM_UNIQUE: USE_SPECIAL %u", item->GetVnum());

				switch (item->GetVnum())
				{
				case 71049: // ºñ´Üº¸µû¸®
#ifdef KASMIR_PAKET_SYSTEM
				case 88901:
#endif
					if (g_bEnableBootaryCheck)
					{
						if (IS_BOTARYABLE_ZONE(GetMapIndex()) == true)
						{
#ifdef KASMIR_PAKET_SYSTEM
							m_bKasmirPaketDurum = item->GetVnum() == 88901 ? true : false;
#endif

							UseSilkBotary();
						}
#ifdef TEXTS_IMPROVEMENT
						else {
							ChatPacketNew(CHAT_TYPE_INFO, 668, "");
						}
#endif
					}
					else
					{
#ifdef KASMIR_PAKET_SYSTEM
						m_bKasmirPaketDurum = item->GetVnum() == 88901 ? true : false;
#endif

						UseSilkBotary();
					}
					break;
				}
			}
			else
			{
				if (!item->IsEquipped())
					EquipItem(item);
				else
					UnequipItem(item);
			}
		}
		break;
		}
	}
	break;

	case ITEM_COSTUME:
	case ITEM_WEAPON:
	case ITEM_ARMOR:
	case ITEM_ROD:
	case ITEM_RING:		// ½Å±Ô ¹ÝÁö ¾ÆÀÌÅÛ
	case ITEM_BELT:		// ½Å±Ô º§Æ® ¾ÆÀÌÅÛ
		//ChatPacket(CHAT_TYPE_INFO, "You can put in your Mount inventory");
		// MINING
	case ITEM_PICK:
		// END_OF_MINING
		if (!item->IsEquipped())
			EquipItem(item);
		else
			UnequipItem(item);
		break;
		// Âø¿ëÇÏÁö ¾ÊÀº ¿ëÈ¥¼®Àº »ç¿ëÇÒ ¼ö ¾ø´Ù.
		// Á¤»óÀûÀÎ Å¬¶ó¶ó¸é, ¿ëÈ¥¼®¿¡ °üÇÏ¿© item use ÆÐÅ¶À» º¸³¾ ¼ö ¾ø´Ù.
		// ¿ëÈ¥¼® Âø¿ëÀº item move ÆÐÅ¶À¸·Î ÇÑ´Ù.
		// Âø¿ëÇÑ ¿ëÈ¥¼®Àº ÃßÃâÇÑ´Ù.
	case ITEM_DS:
	{
		if (!item->IsEquipped())
			return false;
		return DSManager::instance().PullOut(this, NPOS, item);
		break;
	}
	case ITEM_SPECIAL_DS:
		if (!item->IsEquipped())
			EquipItem(item);
		else
			UnequipItem(item);
		break;

	case ITEM_FISH:
	{
		if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 667, "");
#endif
			return false;
		}
#ifdef ENABLE_NEWSTUFF
		else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(GetPlayerID()) && !IsAllowedPotionOnPVP(item->GetVnum()))
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 667, "");
#endif
			return false;
		}
#endif

		if (item->GetSubType() == FISH_ALIVE)
			fishing::UseFish(this, item);
	}
	break;

	case ITEM_TREASURE_BOX:
	{
		return false;
	}
	break;

	case ITEM_TREASURE_KEY:
	{
		LPITEM item2;

		if (!GetItem(DestCell) || !(item2 = GetItem(DestCell)))
			return false;

		if (item2->IsExchanging() || item2->IsEquipped()) // @fixme114
			return false;

		if (item2->GetType() != ITEM_TREASURE_BOX)
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 408, "");
#endif
			return false;
		}

		if (item->GetValue(0) == item2->GetValue(0))
		{
			uint32_t dwBoxVnum = item2->GetVnum();
			std::vector <uint32_t> dwVnums;
			std::vector <uint32_t> dwCounts;
			std::vector <LPITEM> item_gets(0);
			int count = 0;

			if (GiveItemFromSpecialItemGroup(dwBoxVnum, dwVnums, dwCounts, item_gets, count))
			{
				ITEM_MANAGER::instance().RemoveItem(item);
				ITEM_MANAGER::instance().RemoveItem(item2);

				for (int i = 0; i < count; i++) {
					switch (dwVnums[i])
					{
					case CSpecialItemGroup::GOLD:
						break;
					case CSpecialItemGroup::EXP:
						break;
					case CSpecialItemGroup::MOB:
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 378, "");
#endif
						break;
					case CSpecialItemGroup::SLOW:
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 377, "");
#endif
						break;
					case CSpecialItemGroup::DRAIN_HP:
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 373, "");
#endif
						break;
					case CSpecialItemGroup::POISON:
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 376, "");
#endif
						break;
#ifdef ENABLE_WOLFMAN_CHARACTER
					case CSpecialItemGroup::BLEEDING:
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 379, "");
#endif
						break;
#endif
					case CSpecialItemGroup::MOB_GROUP:
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 380, "");
#endif
						break;
					default:
						//#ifdef TEXTS_IMPROVEMENT
						//									if (item_gets[i]) {
						//										if (dwCounts[i] > 1) {
						//											ChatPacketNew(CHAT_TYPE_INFO, 374, "%d#%s", dwCounts[i], item_gets[i]->GetName());
						//										} else {
						//											ChatPacketNew(CHAT_TYPE_INFO, 375, "%s", item_gets[i]->GetName());
						//										}
						//									}
						//#endif
						break;
					}
				}
			}
			else
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 408, "");
#endif
				return false;
			}
		}
		else
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 408, "");
#endif
			return false;
		}
	}
	break;

	case ITEM_GIFTBOX:
	{
#ifdef ENABLE_NEWSTUFF
		if (0 != g_BoxUseTimeLimitValue)
		{
			if (get_dword_time() < m_dwLastBoxUseTime + g_BoxUseTimeLimitValue)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 510, "");
#endif
				return false;
			}
		}

		m_dwLastBoxUseTime = get_dword_time();
#endif
		uint32_t dwBoxVnum = item->GetVnum();

		std::vector <uint32_t> dwVnums;
		std::vector <uint32_t> dwCounts;
		std::vector <LPITEM> item_gets(0);
		int count = 0;

		if (GiveItemFromSpecialItemGroup(dwBoxVnum, dwVnums, dwCounts, item_gets, count))
		{
			item->SetCount(item->GetCount() - 1);
#ifdef ENABLE_RANKING
			SetRankPoints(17, GetRankPoints(17) + 1);
#endif

			for (int i = 0; i < count; i++) {
				switch (dwVnums[i])
				{
				case CSpecialItemGroup::GOLD:
					break;
				case CSpecialItemGroup::EXP:
					break;
				case CSpecialItemGroup::MOB:
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 378, "");
#endif
					break;
				case CSpecialItemGroup::SLOW:
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 377, "");
#endif
					break;
				case CSpecialItemGroup::DRAIN_HP:
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 373, "");
#endif
					break;
				case CSpecialItemGroup::POISON:
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 376, "");
#endif
					break;
#ifdef ENABLE_WOLFMAN_CHARACTER
				case CSpecialItemGroup::BLEEDING:
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 379, "");
#endif
					break;
#endif
				case CSpecialItemGroup::MOB_GROUP:
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 380, "");
#endif
					break;
				default:
					//#ifdef TEXTS_IMPROVEMENT
					//							if (item_gets[i]) {
					//								if (dwCounts[i] > 1) {
					//									ChatPacketNew(CHAT_TYPE_INFO, 374, "%d#%s", dwCounts[i], item_gets[i]->GetName());
					//								} else {
					//									ChatPacketNew(CHAT_TYPE_INFO, 375, "%s", item_gets[i]->GetName());
					//								}
					//							}
					//#endif
					break;
				}
			}
		}
		else
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 395, "");
#endif
			return false;
		}
	}
	break;

	case ITEM_SKILLFORGET:
	{
		if (!item->GetSocket(0))
		{
			ITEM_MANAGER::instance().RemoveItem(item);
			return false;
		}

		uint32_t dwVnum = item->GetSocket(0);

		if (SkillLevelDown(dwVnum)) {
			ITEM_MANAGER::instance().RemoveItem(item);
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 399, "");
#endif
		}
#ifdef TEXTS_IMPROVEMENT
		else {
			ChatPacketNew(CHAT_TYPE_INFO, 400, "");
		}
#endif
	}
	break;

	case ITEM_SKILLBOOK:
	{
		if (item->GetVnum() == 55003 || item->GetVnum() == 55004 || item->GetVnum() == 55005) {
			return false;
		}

		if (IsPolymorphed())
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 313, "");
#endif
			return false;
		}

		uint32_t dwVnum = 0;
		if (item->GetVnum() == 50300)
		{
			dwVnum = item->GetSocket(0);
		}
		else
		{
			dwVnum = item->GetValue(0);
		}

		dwVnum = item->GetVnum() == 50301 || item->GetVnum() == 50302 || item->GetVnum() == 50303 ? SKILL_LEADERSHIP : dwVnum;

		if (0 == dwVnum)
		{
			ITEM_MANAGER::instance().RemoveItem(item);

			return false;
		}

		if (dwVnum == SKILL_LEADERSHIP) {
			int lv = GetSkillLevel(SKILL_LEADERSHIP);
			if (lv < item->GetValue(0)) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 429, "");
#endif
				return false;
			}

			if (lv >= item->GetValue(1)) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 430, "");
#endif
				return false;
			}
		}

		if (true == LearnSkillByBook(dwVnum))
		{
#ifdef ENABLE_BOOKS_STACKFIX
			item->SetCount(item->GetCount() - 1);
#else
			ITEM_MANAGER::instance().RemoveItem(item);
#endif
			int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
			SetSkillNextReadTime(dwVnum, dwVnum == SKILL_LEADERSHIP ? get_global_time() + 18000 : get_global_time() + iReadDelay);
		}
	}
	break;
#ifdef ENABLE_NEW_PET_EDITS
	case ITEM_TYPE_PET:
	{
		if (!GetNewPetSystem())
			return false;

		if (GetNewPetSystem()->IsActivePet()) {
			GetNewPetSystem()->IncreasePetSkillByBook(item);
		}
#ifdef TEXTS_IMPROVEMENT
		else {
			ChatPacketNew(CHAT_TYPE_INFO, 53, "");
		}
#endif
	}
	break;
#endif
	case ITEM_USE:
	{
		if (item->GetVnum() > 50800 && item->GetVnum() <= 50820)
		{
			if (test_server)
				sys_log(0, "ADD addtional effect : vnum(%d) subtype(%d)", item->GetOriginalVnum(), item->GetSubType());

			int affect_type = AFFECT_EXP_BONUS_EURO_FREE;
			int apply_type = aApplyInfo[item->GetValue(0)].bPointType;
			int apply_value = item->GetValue(2);
			int apply_duration = item->GetValue(1);

			switch (item->GetSubType())
			{
			case USE_ABILITY_UP:
				if (FindAffect(affect_type, apply_type))
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 442, "");
#endif
					return false;
				}

				{
					switch (item->GetValue(0))
					{
					case APPLY_MOV_SPEED:
						AddAffect(affect_type, apply_type, apply_value, AFF_MOV_SPEED_POTION, apply_duration, 0, true, true);
						break;

					case APPLY_ATT_SPEED:
						AddAffect(affect_type, apply_type, apply_value, AFF_ATT_SPEED_POTION, apply_duration, 0, true, true);
						break;

					case APPLY_STR:
					case APPLY_DEX:
					case APPLY_CON:
					case APPLY_INT:
					case APPLY_CAST_SPEED:
					case APPLY_RESIST_MAGIC:
					case APPLY_ATT_GRADE_BONUS:
					case APPLY_DEF_GRADE_BONUS:
						AddAffect(affect_type, apply_type, apply_value, 0, apply_duration, 0, true, true);
						break;
					}
				}

				if (GetWarMap())
					GetWarMap()->UsePotion(this, item);

				item->SetCount(item->GetCount() - 1);
				break;

			case USE_AFFECT:
			{
				if (FindAffect(AFFECT_EXP_BONUS_EURO_FREE, aApplyInfo[item->GetValue(1)].bPointType))
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 442, "");
#endif
				}
				else
				{
					// PC_BANG_ITEM_ADD
					if (item->IsPCBangItem() == true)
					{
						// PC¹æÀÎÁö Ã¼Å©ÇØ¼­ Ã³¸®
						if (CPCBangManager::instance().IsPCBangIP(GetDesc()->GetHostName()) == false)
						{
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 426, "");
#endif
							return false;
						}
					}
					// END_PC_BANG_ITEM_ADD

					AddAffect(AFFECT_EXP_BONUS_EURO_FREE, aApplyInfo[item->GetValue(1)].bPointType, item->GetValue(2), 0, item->GetValue(3), 0, false, true);
					item->SetCount(item->GetCount() - 1);
				}
			}
			break;
			case USE_POTION_NODELAY:
			{
				if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
				{
					if (quest::CQuestManager::instance().GetEventFlag("arena_potion_limit") > 0)
					{
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 303, "");
#endif
						return false;
					}

					switch (item->GetVnum())
					{
					case 70020:
					case 71018:
					case 71019:
					case 71020:
						if (quest::CQuestManager::instance().GetEventFlag("arena_potion_limit_count") < 10000)
						{
							if (m_nPotionLimit <= 0)
							{
#ifdef TEXTS_IMPROVEMENT
								ChatPacketNew(CHAT_TYPE_INFO, 362, "");
#endif
								return false;
							}
						}
						break;

					default:
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 303, "");
#endif
						return false;
						break;
					}
				}
#ifdef ENABLE_NEWSTUFF
				else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(GetPlayerID()) && !IsAllowedPotionOnPVP(item->GetVnum()))
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 667, "");
#endif
					return false;
				}
#endif

				bool used = false;

				if (item->GetValue(0) != 0) // HP Àý´ë°ª È¸º¹
				{
					if (GetHP() < GetMaxHP())
					{
						PointChange(POINT_HP, item->GetValue(0) * (100 + GetPoint(POINT_POTION_BONUS)) / 100);
						EffectPacket(SE_HPUP_RED);
						used = true;
					}
				}

				if (item->GetValue(1) != 0)	// SP Àý´ë°ª È¸º¹
				{
					if (GetSP() < GetMaxSP())
					{
						PointChange(POINT_SP, item->GetValue(1) * (100 + GetPoint(POINT_POTION_BONUS)) / 100);
						EffectPacket(SE_SPUP_BLUE);
						used = true;
					}
				}

				if (item->GetValue(3) != 0) // HP % È¸º¹
				{
					if (GetHP() < GetMaxHP())
					{
						PointChange(POINT_HP, item->GetValue(3) * GetMaxHP() / 100);
						EffectPacket(SE_HPUP_RED);
						used = true;
					}
				}

				if (item->GetValue(4) != 0) // SP % È¸º¹
				{
					if (GetSP() < GetMaxSP())
					{
						PointChange(POINT_SP, item->GetValue(4) * GetMaxSP() / 100);
						EffectPacket(SE_SPUP_BLUE);
						used = true;
					}
				}

				if (used)
				{
					if (item->GetVnum() == 50085 || item->GetVnum() == 50086) {
						SetUseSeedOrMoonBottleTime();
					}

					if (GetWarMap())
						GetWarMap()->UsePotion(this, item);

					m_nPotionLimit--;

					//RESTRICT_USE_SEED_OR_MOONBOTTLE
					item->SetCount(item->GetCount() - 1);
					//END_RESTRICT_USE_SEED_OR_MOONBOTTLE
				}
			}
			break;
			}

			return true;
		}


		if (item->GetVnum() >= 27863 && item->GetVnum() <= 27883)
		{
			if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 667, "");
#endif
				return false;
			}
#ifdef ENABLE_NEWSTUFF
			else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(GetPlayerID()) && !IsAllowedPotionOnPVP(item->GetVnum()))
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 667, "");
#endif
				return false;
			}
#endif
		}

		if (test_server)
		{
			sys_log(0, "USE_ITEM %s Type %d SubType %d vnum %d", item->GetName(), item->GetType(), item->GetSubType(), item->GetOriginalVnum());
		}

		switch (item->GetSubType())
		{
		case USE_FISH:
		{
			CAffect* pAffect = nullptr;
			int type = 0, duration = item->GetValue(0);
			for (int i = 0; i < ITEM_APPLY_MAX_NUM; i++) {
				type = aApplyInfo[item->GetApplyType(i)].bPointType;
				if (type != 0) {
					pAffect = FindAffect(AFFECT_FISH_BONUS, type);
				}
			}

			if (pAffect != nullptr) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 893, "");
#endif
				return false;
			}
			else {
				item->SetCount(item->GetCount() - 1);

				for (int i = 0; i < ITEM_APPLY_MAX_NUM; i++) {
					type = item->GetApplyType(i);
					if (type != 0) {
						AddAffect(AFFECT_FISH_BONUS, aApplyInfo[type].bPointType, item->GetApplyValue(i), item->GetID(), duration, 0, false, false);
					}
				}
			}
			break;
		}
		case USE_TIME_CHARGE_PER:
		{
			LPITEM pDestItem = GetItem(DestCell);
			if (nullptr == pDestItem)
			{
				return false;
			}
			// ¿ì¼± ¿ëÈ¥¼®¿¡ °üÇØ¼­¸¸ ÇÏµµ·Ï ÇÑ´Ù.
			if (pDestItem->IsDragonSoul())
			{
#ifdef ENABLE_DS_POTION_DIFFRENT
				if (item->GetCount() > 1) {
					int pos = GetEmptyInventory(item->GetSize());
					if (pos == -1) {
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 366, "");
#endif
						return false;
					}

					item->SetCount(item->GetCount() - 1);
					LPITEM item2 = ITEM_MANAGER::instance().CreateItem(item->GetVnum(), 1);
					if (!item2)
						return false;

					item2->AddToCharacter(this, TItemPos(INVENTORY, pos), false);
					item = item2;
				}

				if (item->GetSocket(0) <= 0) {
					item->RemoveFromCharacter();
					return false;
				}
				else {
					uint32_t duration = DSManager::instance().GetDuration(pDestItem);
					uint32_t remain_sec = pDestItem->GetSocket(ITEM_SOCKET_REMAIN_SEC);
					if (remain_sec == duration)
						return false;

					uint32_t dwBottlePercent = item->GetSocket(0);
					uint32_t dwOnePercent = duration / 100;
					uint32_t dwRemainPercent = remain_sec / dwOnePercent;
					uint32_t dif = 100 - dwRemainPercent;
					dif = dif > dwBottlePercent ? dwBottlePercent : dif;
					uint32_t add = dwOnePercent * dif;
					if (remain_sec + add >= 86400) {
						pDestItem->SetSocket(ITEM_SOCKET_REMAIN_SEC, duration);
					}
					else {
						pDestItem->SetSocket(ITEM_SOCKET_REMAIN_SEC, remain_sec + add);
					}

					item->SetSocket(0, dwBottlePercent - dif);
					if (item->GetSocket(0) < 1)
						item->RemoveFromCharacter();

					return true;
				}
#else
				int ret;
				char buf[128];
				if (item->GetVnum() == DRAGON_HEART_VNUM)
				{
					ret = pDestItem->GiveMoreTime_Per((float)item->GetSocket(ITEM_SOCKET_CHARGING_AMOUNT_IDX));
				}
				else
				{
					ret = pDestItem->GiveMoreTime_Per((float)item->GetValue(ITEM_VALUE_CHARGING_AMOUNT_IDX));
				}
				if (ret > 0)
				{
					if (item->GetVnum() == DRAGON_HEART_VNUM)
					{
						sprintf(buf, "Inc %ds by item{VN:%d SOC%d:%ld}", ret, item->GetVnum(), ITEM_SOCKET_CHARGING_AMOUNT_IDX, item->GetSocket(ITEM_SOCKET_CHARGING_AMOUNT_IDX));
					}
					else
					{
						sprintf(buf, "Inc %ds by item{VN:%d VAL%d:%ld}", ret, item->GetVnum(), ITEM_VALUE_CHARGING_AMOUNT_IDX, item->GetValue(ITEM_VALUE_CHARGING_AMOUNT_IDX));
					}

#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 670, "%s#%d", pDestItem->GetName(), ret);
#endif
					item->SetCount(item->GetCount() - 1);
					LogManager::instance().ItemLog(this, item, "DS_CHARGING_SUCCESS", buf);
					return true;
				}
				else
				{
					if (item->GetVnum() == DRAGON_HEART_VNUM)
					{
						sprintf(buf, "No change by item{VN:%d SOC%d:%ld}", item->GetVnum(), ITEM_SOCKET_CHARGING_AMOUNT_IDX, item->GetSocket(ITEM_SOCKET_CHARGING_AMOUNT_IDX));
					}
					else
					{
						sprintf(buf, "No change by item{VN:%d VAL%d:%ld}", item->GetVnum(), ITEM_VALUE_CHARGING_AMOUNT_IDX, item->GetValue(ITEM_VALUE_CHARGING_AMOUNT_IDX));
					}

#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 671, "%s", pDestItem->GetName());
#endif
					LogManager::instance().ItemLog(this, item, "DS_CHARGING_FAILED", buf);
					return false;
				}
#endif
			}
			else
				return false;
		}
		break;
		case USE_TIME_CHARGE_FIX:
		{
			LPITEM pDestItem = GetItem(DestCell);
			if (nullptr == pDestItem)
			{
				return false;
			}
			// ¿ì¼± ¿ëÈ¥¼®¿¡ °üÇØ¼­¸¸ ÇÏµµ·Ï ÇÑ´Ù.
			if (pDestItem->IsDragonSoul())
			{
				int ret = pDestItem->GiveMoreTime_Fix(item->GetValue(ITEM_VALUE_CHARGING_AMOUNT_IDX));
				char buf[128];
				if (ret)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 670, "%s#%d", pDestItem->GetName(), ret);
#endif
					sprintf(buf, "Increase %ds by item{VN:%d VAL%d:%ld}", ret, item->GetVnum(), ITEM_VALUE_CHARGING_AMOUNT_IDX, item->GetValue(ITEM_VALUE_CHARGING_AMOUNT_IDX));
					LogManager::instance().ItemLog(this, item, "DS_CHARGING_SUCCESS", buf);
					item->SetCount(item->GetCount() - 1);
					return true;
				}
				else
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 671, "%s", pDestItem->GetName());
#endif
					sprintf(buf, "No change by item{VN:%d VAL%d:%ld}", item->GetVnum(), ITEM_VALUE_CHARGING_AMOUNT_IDX, item->GetValue(ITEM_VALUE_CHARGING_AMOUNT_IDX));
					LogManager::instance().ItemLog(this, item, "DS_CHARGING_FAILED", buf);
					return false;
				}
			}
			else
				return false;
		}
		break;
#ifdef ENABLE_NEW_USE_POTION
		case USE_NEW_POTIION: {
			uint32_t dwType = item->GetValue(0);
			if (dwType >= AFFECT_NEW_POTION24 && dwType <= AFFECT_NEW_POTION29 && !marriage::CManager::instance().IsMarried(GetPlayerID())) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 891, "");
#endif
				return false;
			}

			if (dwType == AFFECT_NEW_POTION31) {
				LPPARTY party = GetParty();
				if ((!party) || (party && GetPlayerID() != party->GetLeaderPID())) {
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 902, "");
#endif
					return false;
				}
			}

			CAffect* pAffect = FindAffect(dwType);
			if (pAffect && item->GetID() != pAffect->dwFlag)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 442, "");
#else
				ChatPacket(CHAT_TYPE_INFO, "Már aktiv egy ilyen harmat.");
#endif
				return false;
			}

			if (item->GetCount() > 1)
			{
#ifdef ENABLE_EXTRA_INVENTORY
				const bool bFromExtraInventory = (item->GetWindow() == EXTRA_INVENTORY);
#else
				const bool bFromExtraInventory = false;
#endif
				int pos = -1;

#ifdef ENABLE_EXTRA_INVENTORY
				if (bFromExtraInventory)
					pos = GetEmptyExtraInventory(item);
				else
#endif
					pos = GetEmptyInventory(item->GetSize());

				if (pos == -1) {
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 366, "");
#endif
					break;
				}

				item->SetCount(item->GetCount() - 1);
				LPITEM item2 = ITEM_MANAGER::instance().CreateItem(item->GetVnum(), 1);
				if (!item2)
					return true;

#ifdef ENABLE_EXTRA_INVENTORY
				if (bFromExtraInventory)
					item2->AddToCharacter(this, TItemPos(EXTRA_INVENTORY, pos), false);
				else
#endif
					item2->AddToCharacter(this, TItemPos(INVENTORY, pos), false);

				item = item2;
			}

			uint8_t bApplyOn = item->GetApplyType(0);
			int32_t lApplyValue = item->GetApplyValue(0);

			pAffect = FindAffect(dwType);
			if (pAffect) {
				uint32_t dwItemID = pAffect->dwFlag;
				if (item->GetID() == dwItemID) {
					item->Lock(false);
					item->SetSocket(1, 0);
					RemoveAffect(dwType);
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 28, "%s", item->GetName());
#endif
				}
				else {
					LPITEM pkItem = FindItemByID(dwItemID);
					if (pkItem) {
						pkItem->Lock(false);
						pkItem->SetSocket(1, 0);
					}

					RemoveAffect(dwType);
					item->Lock(true);
					item->SetSocket(1, 1);
					AddAffect(dwType, aApplyInfo[bApplyOn].bPointType, lApplyValue, item->GetID(), INFINITE_AFFECT_DURATION, 0, true, false);
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 29, "%s", item->GetName());
#endif
				}
			}
			else {
				if (dwType == AFFECT_NEW_POTION19) {
					pAffect = FindAffect(AFFECT_NEW_POTION20);
					if (pAffect) {
						LPITEM pkItem = FindItemByID(pAffect->dwFlag);
						if (pkItem) {
							pkItem->Lock(false);
							pkItem->SetSocket(1, 0);
						}

						RemoveAffect(AFFECT_NEW_POTION20);
					}
				}
				else if (dwType == AFFECT_NEW_POTION20) {
					pAffect = FindAffect(AFFECT_NEW_POTION19);
					if (pAffect) {
						LPITEM pkItem = FindItemByID(pAffect->dwFlag);
						if (pkItem) {
							pkItem->Lock(false);
							pkItem->SetSocket(1, 0);
						}

						RemoveAffect(AFFECT_NEW_POTION19);
					}
				}
				else if (dwType == AFFECT_NEW_POTION21) {
					pAffect = FindAffect(AFFECT_NEW_POTION22);
					if (pAffect) {
						LPITEM pkItem = FindItemByID(pAffect->dwFlag);
						if (pkItem) {
							pkItem->Lock(false);
							pkItem->SetSocket(1, 0);
						}

						RemoveAffect(AFFECT_NEW_POTION22);
					}
				}
				else if (dwType == AFFECT_NEW_POTION22) {
					pAffect = FindAffect(AFFECT_NEW_POTION21);
					if (pAffect) {
						LPITEM pkItem = FindItemByID(pAffect->dwFlag);
						if (pkItem) {
							pkItem->Lock(false);
							pkItem->SetSocket(1, 0);
						}

						RemoveAffect(AFFECT_NEW_POTION21);
					}
				}

				item->Lock(true);
				item->SetSocket(1, 1);
				AddAffect(dwType, aApplyInfo[bApplyOn].bPointType, lApplyValue, item->GetID(), INFINITE_AFFECT_DURATION, 0, true, false);
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 29, "%s", item->GetName());
#endif
			}
		}
							break;
#endif
		case USE_SPECIAL:

			switch (item->GetVnum())
			{
				//Å©¸®½º¸¶½º ¶õÁÖ
			case ITEM_NOG_POCKET:
			{
				/*
				¶õÁÖ´É·ÂÄ¡ : item_proto value ÀÇ¹Ì
					ÀÌµ¿¼Óµµ  value 1
					°ø°Ý·Â	  value 2
					°æÇèÄ¡    value 3
					Áö¼Ó½Ã°£  value 0 (´ÜÀ§ ÃÊ)

				*/
				if (FindAffect(AFFECT_NOG_ABILITY))
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 442, "");
#endif
					return false;
				}
				int32_t time = item->GetValue(0);
				int32_t moveSpeedPer = item->GetValue(1);
				int32_t attPer = item->GetValue(2);
				int32_t expPer = item->GetValue(3);
				AddAffect(AFFECT_NOG_ABILITY, POINT_MOV_SPEED, moveSpeedPer, AFF_MOV_SPEED_POTION, time, 0, true, true);
				AddAffect(AFFECT_NOG_ABILITY, POINT_MALL_ATTBONUS, attPer, AFF_NONE, time, 0, true, true);
				AddAffect(AFFECT_NOG_ABILITY, POINT_MALL_EXPBONUS, expPer, AFF_NONE, time, 0, true, true);
				item->SetCount(item->GetCount() - 1);
			}
			break;

			//¶ó¸¶´Ü¿ë »çÅÁ
			case ITEM_RAMADAN_CANDY:
			{
				/*
				»çÅÁ´É·ÂÄ¡ : item_proto value ÀÇ¹Ì
					ÀÌµ¿¼Óµµ  value 1
					°ø°Ý·Â	  value 2
					°æÇèÄ¡    value 3
					Áö¼Ó½Ã°£  value 0 (´ÜÀ§ ÃÊ)

				*/
				// @fixme147 BEGIN
				if (FindAffect(AFFECT_RAMADAN_ABILITY))
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 442, "");
#endif
					return false;
				}
				// @fixme147 END
				int32_t time = item->GetValue(0);
				int32_t moveSpeedPer = item->GetValue(1);
				int32_t attPer = item->GetValue(2);
				int32_t expPer = item->GetValue(3);
				AddAffect(AFFECT_RAMADAN_ABILITY, POINT_MOV_SPEED, moveSpeedPer, AFF_MOV_SPEED_POTION, time, 0, true, true);
				AddAffect(AFFECT_RAMADAN_ABILITY, POINT_MALL_ATTBONUS, attPer, AFF_NONE, time, 0, true, true);
				AddAffect(AFFECT_RAMADAN_ABILITY, POINT_MALL_EXPBONUS, expPer, AFF_NONE, time, 0, true, true);
				item->SetCount(item->GetCount() - 1);
			}
			break;
			case ITEM_MARRIAGE_RING:
			{
				marriage::TMarriage* pMarriage = marriage::CManager::instance().Get(GetPlayerID());
				if (pMarriage)
				{
					if (pMarriage->ch1 != nullptr)
					{
						if (CArenaManager::instance().IsArenaMap(pMarriage->ch1->GetMapIndex()) == true)
						{
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 672, "");
#endif
							break;
						}
					}

					if (pMarriage->ch2 != nullptr)
					{
						if (CArenaManager::instance().IsArenaMap(pMarriage->ch2->GetMapIndex()) == true)
						{
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 672, "");
#endif
							break;
						}
					}

					int consumeSP = CalculateConsumeSP(this);

					if (consumeSP < 0)
						return false;

					PointChange(POINT_SP, -consumeSP, false);

					WarpToPID(pMarriage->GetOther(GetPlayerID()));
				}
#ifdef TEXTS_IMPROVEMENT
				else {
					ChatPacketNew(CHAT_TYPE_INFO, 242, "");
				}
#endif
			}
			break;

			//±âÁ¸ ¿ë±âÀÇ ¸ÁÅä
			case UNIQUE_ITEM_CAPE_OF_COURAGE:
				// {
					// if (GetMapIndex() != 1)
					// {
	// #ifdef TEXTS_IMPROVEMENT
						// ChatPacketNew(CHAT_TYPE_INFO, 489, "");
	// #endif
						// return true;
					// }


				// }
				// break;
			case 70057:
			case REWARD_BOX_UNIQUE_ITEM_CAPE_OF_COURAGE:
#ifdef __EFFETTO_MANTELLO__
				if (GetMapIndex() != 1)
				{
					this->EffectPacket(SE_MANTELLO);
					AggregateMonster();

				}
				else
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 489, "");
					return false;
#endif
				}
#endif
#ifdef ENABLE_AGGREGATE_MONSTER_PLUS_RAZOR93
			case 70030:
#ifdef __EFFETTO_MANTELLO__
				if (GetMapIndex() != 1)
				{
					this->EffectPacket(SE_MANTELLO);
					AggregateMonsterPlus();

				}
				else
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 489, "");
					return false;
#endif
				}

#endif
#endif
				item->SetCount(item->GetCount() - 1);//@Razor93 (batorsag kopi fogyjon)
				//UpdateMountCountOverhead(this);
				break;

			case UNIQUE_ITEM_WHITE_FLAG:
				ForgetMyAttacker();
				item->SetCount(item->GetCount() - 1);
				break;

			case UNIQUE_ITEM_TREASURE_BOX:
				break;
#ifdef ENABLE_BATTLE_PASS
#ifdef ENABLE_FREE_PASS_RAZOR93
#ifdef ENABLE_BATTLE_PASS
			case 70611:
			{
				const uint8_t bBattlePassId = GetBattlePassId();
				if (!bBattlePassId)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 780, "");  
#endif
					return false;
				}

				// 1x hasznalhato ugyanarra a BP ID-re
				if (HasBattlePassBoost(bBattlePassId))
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 8, ""); 
#endif
					return false;
				}

				int remain = (int)(m_dwBattlePassEndTime - get_global_time());
				if (remain <= 0)
					remain = GetSecondsTillNextMonth();

				 
				AddAffect(AFFECT_BATTLE_PASS_BOOST, POINT_BATTLE_PASS_ID, bBattlePassId, 0, remain, 0, true);

				 
				ApplyBattlePassBoostRecalc(bBattlePassId);

				 
				CBattlePass::instance().BattlePassRequestOpen(this);

				item->SetCount(item->GetCount() - 1);
			}
			break;
#endif

#else

			case 70611://79900
			{
				char szQuery[1024];
				snprintf(szQuery, sizeof(szQuery), "SELECT * FROM battle_pass_ranking WHERE player_name = '%s' AND battle_pass_id = %d;", GetName(), 1);
				std::unique_ptr<SQLMsg> pmsg(DBManager::instance().DirectQuery(szQuery));
				if (pmsg->Get()->uiNumRows > 0) {
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 6, "");
#endif
					return false;
				}

				int iSeconds = GetSecondsTillNextMonth();
				if (iSeconds < 0) {
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 7, "");
#endif
					return false;
				}

				if (FindAffect(AFFECT_BATTLE_PASS)) {
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 8, "");
#endif
					return false;
				}
				else {
					m_dwBattlePassEndTime = get_global_time() + iSeconds;

					AddAffect(AFFECT_BATTLE_PASS, POINT_BATTLE_PASS_ID, 1, 0, iSeconds, 0, true);
					item->SetCount(item->GetCount() - 1);
				}
			}
			break;
#endif
#endif
			case 27989: // ¿µ¼®°¨Áö±â
			case 76006: // ¼±¹°¿ë ¿µ¼®°¨Áö±â
			{
				LPSECTREE_MAP pMap = SECTREE_MANAGER::instance().GetMap(GetMapIndex());

				if (pMap != nullptr)
				{
					item->SetSocket(0, item->GetSocket(0) + 1);

					FFindStone f;

					// <Factor> SECTREE::for_each -> SECTREE::for_each_entity
					pMap->for_each(f);

					if (f.m_mapStone.size() > 0)
					{
						std::map<uint32_t, LPCHARACTER>::iterator stone = f.m_mapStone.begin();

						uint32_t max = UINT_MAX;
						LPCHARACTER pTarget = stone->second;

						while (stone != f.m_mapStone.end())
						{
							uint32_t dist = (uint32_t)DISTANCE_SQRT(GetX() - stone->second->GetX(), GetY() - stone->second->GetY());

							if (dist != 0 && max > dist)
							{
								max = dist;
								pTarget = stone->second;
							}
							stone++;
						}

						if (pTarget != nullptr)
						{
							int val = 3;

							if (max < 10000) val = 2;
							else if (max < 70000) val = 1;

							ChatPacket(CHAT_TYPE_COMMAND, "StoneDetect %u %d %d", (uint32_t)GetVID(), val,
								(int)GetDegreeFromPositionXY(GetX(), pTarget->GetY(), pTarget->GetX(), GetY()));
						}
#ifdef TEXTS_IMPROVEMENT
						else {
							ChatPacketNew(CHAT_TYPE_INFO, 673, "");
						}
#endif
					}
#ifdef TEXTS_IMPROVEMENT
					else {
						ChatPacketNew(CHAT_TYPE_INFO, 673, "");
					}
#endif

					if (item->GetSocket(0) >= 6)
					{
						ChatPacket(CHAT_TYPE_COMMAND, "StoneDetect %u 0 0", (uint32_t)GetVID());
						ITEM_MANAGER::instance().RemoveItem(item);
					}
				}
				break;
			}
			break;

			case 27996: // µ¶º´
				item->SetCount(item->GetCount() - 1);
				AttackedByPoison(nullptr); // @warme008
				break;

			case 27987: // Á¶°³
				// 50  µ¹Á¶°¢ 47990
				// 30  ²Î
				// 10  ¹éÁøÁÖ 47992
				// 7   Ã»ÁøÁÖ 47993
				// 3   ÇÇÁøÁÖ 47994
			{
				item->SetCount(item->GetCount() - 1);

				int r = number(1, 100);

				if (r <= 50)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 458, "");
#endif
					AutoGiveItem(27990);
				}
				else
				{
					const int prob_table_gb2312[] =
					{
						95, 97, 99
					};

					const int* prob_table = prob_table_gb2312;

					if (r <= prob_table[0]) {
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 457, "");
#endif
					}
					else if (r <= prob_table[1])
					{
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 459, "");
#endif
						AutoGiveItem(27992);
					}
					else if (r <= prob_table[2])
					{
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 460, "");
#endif
						AutoGiveItem(27993);
					}
					else
					{
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 461, "");
#endif
						AutoGiveItem(27994);
					}
				}
			}
			break;

			case 71013: // ÃàÁ¦¿ëÆøÁ×
				CreateFly(number(FLY_FIREWORK1, FLY_FIREWORK6), this);
				item->SetCount(item->GetCount() - 1);
				break;

			case 50100: // ÆøÁ×
			case 50101:
			case 50102:
			case 50103:
			case 50104:
			case 50105:
			case 50106:
				CreateFly(item->GetVnum() - 50100 + FLY_FIREWORK1, this);
				item->SetCount(item->GetCount() - 1);
				break;

			case 50200: // º¸µû¸®
				if (g_bEnableBootaryCheck)
				{
					if (IS_BOTARYABLE_ZONE(GetMapIndex()) == true)
					{
#ifdef KASMIR_PAKET_SYSTEM
						m_bKasmirPaketDurum = false;
#endif
						__OpenPrivateShop();
					}
#ifdef TEXTS_IMPROVEMENT
					else {
						ChatPacketNew(CHAT_TYPE_INFO, 668, "");
					}
#endif
				}
				else
				{
#ifdef KASMIR_PAKET_SYSTEM
					m_bKasmirPaketDurum = false;
#endif
					__OpenPrivateShop();
				}
				break;

			case fishing::FISH_MIND_PILL_VNUM:
			{
#ifdef ENABLE_NEW_FISHING_SYSTEM
				if (FindAffect(AFFECT_FISH_MIND_PILL)) {
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 900, "");
#endif
					return false;
				}
#endif

				AddAffect(AFFECT_FISH_MIND_PILL, POINT_NONE, 0, AFF_FISH_MIND, 20 * 60, 0, true);
				item->SetCount(item->GetCount() - 1);
			}
			break;

			case 50304: // ¿¬°è±â ¼ö·Ã¼­
			case 50305:
			case 50306:
			{
				if (IsPolymorphed())
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 313, "");
#endif
					return false;

				}
				if (GetSkillLevel(SKILL_COMBO) == 0 && GetLevel() < 30)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 322, "");
#endif
					return false;
				}

				if (GetSkillLevel(SKILL_COMBO) == 1 && GetLevel() < 50)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 323, "");
#endif
					return false;
				}

				if (GetSkillLevel(SKILL_COMBO) >= 2)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 324, "");
#endif
					return false;
				}

				int iPct = item->GetValue(0);

				if (LearnSkillByBook(SKILL_COMBO, iPct))
				{
#ifdef ENABLE_BOOKS_STACKFIX
					item->SetCount(item->GetCount() - 1);
#else
					ITEM_MANAGER::instance().RemoveItem(item);
#endif

					int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
					SetSkillNextReadTime(SKILL_COMBO, get_global_time() + iReadDelay);
				}
			}
			break;

#ifdef ENABLE_NEW_SECONDARY_SKILLS
			case 50333:
			case 50334:
			case 50335:
			case 50336: {
				if (IsPolymorphed()) {
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 313, "");
#endif
					return false;

				}

				uint32_t dwSkillVnum = item->GetValue(0);
				if (GetSkillLevel(dwSkillVnum) >= 10) {
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 439, "");
#endif
					return false;
				}

				if (LearnSkillByBook(dwSkillVnum, 0)) {
					item->SetCount(item->GetCount() - 1);
					SetSkillNextReadTime(dwSkillVnum, get_global_time() + 10800);
				}
			}
					  break;
#endif

			case 50311: // ¾ð¾î ¼ö·Ã¼­
			case 50312:
			case 50313:
			{
				if (IsPolymorphed())
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 313, "");
#endif
					return false;

				}
				uint32_t dwSkillVnum = item->GetValue(0);
				int iPct = MINMAX(0, item->GetValue(1), 100);
				if (GetSkillLevel(dwSkillVnum) >= 20 || dwSkillVnum - SKILL_LANGUAGE1 + 1 == GetEmpire())
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 439, "");
#endif
					return false;
				}

				if (LearnSkillByBook(dwSkillVnum, iPct))
				{
#ifdef ENABLE_BOOKS_STACKFIX
					item->SetCount(item->GetCount() - 1);
#else
					ITEM_MANAGER::instance().RemoveItem(item);
#endif

					int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
					SetSkillNextReadTime(dwSkillVnum, get_global_time() + iReadDelay);
				}
			}
			break;

			case 50061: // ÀÏº» ¸» ¼ÒÈ¯ ½ºÅ³ ¼ö·Ã¼­
			{
				if (IsPolymorphed())
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 313, "");
#endif
					return false;

				}
				uint32_t dwSkillVnum = item->GetValue(0);
				int iPct = MINMAX(0, item->GetValue(1), 100);

				if (GetSkillLevel(dwSkillVnum) >= 10)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 306, "");
#endif
					return false;
				}

				if (LearnSkillByBook(dwSkillVnum, iPct))
				{
#ifdef ENABLE_BOOKS_STACKFIX
					item->SetCount(item->GetCount() - 1);
#else
					ITEM_MANAGER::instance().RemoveItem(item);
#endif

					int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
					SetSkillNextReadTime(dwSkillVnum, get_global_time() + iReadDelay);
				}
			}
			break;

			case 50314: case 50315: case 50316: // º¯½Å ¼ö·Ã¼­
			case 50323: case 50324: // ÁõÇ÷ ¼ö·Ã¼­
			case 50325: case 50326: // Ã¶Åë ¼ö·Ã¼­
			{
				if (IsPolymorphed() == true)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 313, "");
#endif
					return false;
				}

				int iSkillLevelLowLimit = item->GetValue(0);
				int iSkillLevelHighLimit = item->GetValue(1);
				int iPct = MINMAX(0, item->GetValue(2), 100);
				int iLevelLimit = item->GetValue(3);
				uint32_t dwSkillVnum = 0;

				switch (item->GetVnum())
				{
				case 50314: case 50315: case 50316:
					dwSkillVnum = SKILL_POLYMORPH;
					break;

				case 50323: case 50324:
					dwSkillVnum = SKILL_ADD_HP;
					break;

				case 50325: case 50326:
					dwSkillVnum = SKILL_RESIST_PENETRATE;
					break;

				default:
					return false;
				}

				if (0 == dwSkillVnum)
					return false;

				if (GetLevel() < iLevelLimit)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 431, "%d", iLevelLimit);
#endif
					return false;
				}

				if (GetSkillLevel(dwSkillVnum) >= 40)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 306, "");
#endif
					return false;
				}

				if (GetSkillLevel(dwSkillVnum) < iSkillLevelLowLimit)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 429, "");
#endif
					return false;
				}

				if (GetSkillLevel(dwSkillVnum) >= iSkillLevelHighLimit)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 306, "");
#endif
					return false;
				}

				if (LearnSkillByBook(dwSkillVnum, iPct))
				{
#ifdef ENABLE_BOOKS_STACKFIX
					item->SetCount(item->GetCount() - 1);
#else
					ITEM_MANAGER::instance().RemoveItem(item);
#endif

					int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
					SetSkillNextReadTime(dwSkillVnum, get_global_time() + iReadDelay);
				}
			}
			break;

			case 50902:
			case 50903:
			case 50904:
			{
				if (IsPolymorphed())
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 313, "");
#endif
					return false;

				}
				uint32_t dwSkillVnum = SKILL_CREATE;
				int iPct = MINMAX(0, item->GetValue(1), 100);

				if (GetSkillLevel(dwSkillVnum) >= 40)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 306, "");
#endif
					return false;
				}

				if (LearnSkillByBook(dwSkillVnum, iPct))
				{
#ifdef ENABLE_BOOKS_STACKFIX
					item->SetCount(item->GetCount() - 1);
#else
					ITEM_MANAGER::instance().RemoveItem(item);
#endif

					int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
					SetSkillNextReadTime(dwSkillVnum, get_global_time() + iReadDelay);
				}
			}
			break;
			// MINING
			case ITEM_MINING_SKILL_TRAIN_BOOK:
			{
				if (IsPolymorphed())
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 313, "");
#endif
					return false;

				}
				uint32_t dwSkillVnum = SKILL_MINING;
				int iPct = MINMAX(0, item->GetValue(1), 100);

				if (GetSkillLevel(dwSkillVnum) >= 40)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 306, "");
#endif
					return false;
				}

				if (LearnSkillByBook(dwSkillVnum, iPct))
				{
#ifdef ENABLE_BOOKS_STACKFIX
					item->SetCount(item->GetCount() - 1);
#else
					ITEM_MANAGER::instance().RemoveItem(item);
#endif

					int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
					SetSkillNextReadTime(dwSkillVnum, get_global_time() + iReadDelay);
				}
			}
			break;
			// END_OF_MINING

			case ITEM_HORSE_SKILL_TRAIN_BOOK:
			{
				if (IsPolymorphed())
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 313, "");
#endif
					return false;

				}
				uint32_t dwSkillVnum = SKILL_HORSE;
				int iPct = MINMAX(0, item->GetValue(1), 100);

				if (GetLevel() < 50)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 404, "%d", 50);
#endif
					return false;
				}

				if (!test_server && get_global_time() < GetSkillNextReadTime(dwSkillVnum))
				{
					if (FindAffect(AFFECT_SKILL_NO_BOOK_DELAY))
					{
						// ÁÖ¾È¼ú¼­ »ç¿ëÁß¿¡´Â ½Ã°£ Á¦ÇÑ ¹«½Ã
						RemoveAffect(AFFECT_SKILL_NO_BOOK_DELAY);
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 465, "");
#endif
					}
					else
					{
						SkillLearnWaitMoreTimeMessage(GetSkillNextReadTime(dwSkillVnum) - get_global_time());
						return false;
					}
				}

				if (GetPoint(POINT_HORSE_SKILL) >= 20 ||
					GetSkillLevel(SKILL_HORSE_WILDATTACK) + GetSkillLevel(SKILL_HORSE_CHARGE) + GetSkillLevel(SKILL_HORSE_ESCAPE) >= 60 ||
					GetSkillLevel(SKILL_HORSE_WILDATTACK_RANGE) + GetSkillLevel(SKILL_HORSE_CHARGE) + GetSkillLevel(SKILL_HORSE_ESCAPE) >= 60)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 307, "");
#endif
					return false;
				}

				if (number(1, 100) <= iPct)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 394, "");
#endif
					PointChange(POINT_HORSE_SKILL, 1);

					int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
					if (!test_server)
						SetSkillNextReadTime(dwSkillVnum, get_global_time() + iReadDelay);
				}
#ifdef TEXTS_IMPROVEMENT
				else {
					ChatPacketNew(CHAT_TYPE_INFO, 393, "");
				}
#endif
#ifdef ENABLE_BOOKS_STACKFIX
				item->SetCount(item->GetCount() - 1);
#else
				ITEM_MANAGER::instance().RemoveItem(item);
#endif
			}
			break;



			case 70102: // Zenbab
			{

				uint32_t max_limit = 2500000;
				uint32_t current = GetAlignment();

				if (current >= max_limit)
				{
					ChatPacket(CHAT_TYPE_INFO, "Max  250.000 with this item!");
					return false;
				}

				uint32_t add_value = item->GetValue(0); 
				uint32_t remaining = max_limit - current;

				uint32_t real_add = std::min(add_value, remaining);

				UpdateAlignment(real_add);
				item->SetCount(item->GetCount() - 1);

				ChatPacket(CHAT_TYPE_INFO, " +500 point.");
			}
			break;


			case 70100:
			{
				const uint32_t max_limit = 25000000;
				const uint32_t current = GetAlignment();

				if (current >= max_limit)
				{
					ChatPacket(CHAT_TYPE_INFO, "Max 2.500.000 with this item!");
					return false;
				}

				const uint32_t add_value = item->GetValue(0);
				const uint32_t real_add = std::min(add_value, max_limit - current);

				UpdateAlignment(real_add);
				item->SetCount(item->GetCount() - 1);

				ChatPacket(CHAT_TYPE_INFO, "+5.000 point.");
			}
			break;

			// --------------------------------------------------------------
			// 80008: ShopBuyPrice alapjan Dragon Coin (account.coins) jovairas
			// --------------------------------------------------------------
			case 39065://sé 1
			case 89003://sé 3
			case 89004://sé 50
			case 89005://sé 100
			case 89006://sé 500
			case 89007://sé 1000

			{
#ifdef ENABLE_ITEMSHOP
				const uint32_t count = item->GetCount();
				if (count == 0)
					break;

				const uint64_t unitPrice = (uint64_t)item->GetShopBuyPrice();
				if (unitPrice == 0)
					break;

				const uint64_t total = unitPrice * (uint64_t)count;

				if (GetDesc() == nullptr)
					break;

				const uint32_t curCoins = GetDragonCoin();
				const uint64_t maxCoins = 0xFFFFFFFFULL; // uint32 max

				if ((uint64_t)curCoins >= maxCoins)
					break;

				const uint64_t canAdd = ((uint64_t)curCoins + total > maxCoins) ? (maxCoins - (uint64_t)curCoins) : total;
				if (canAdd == 0)
					break;

				SetDragonCoin(curCoins + (uint32_t)canAdd);

				item->SetCount(0); // teljes stack felhasznalasa
				ChatPacket(CHAT_TYPE_INFO, "Kaptal %u Sarkanyermet.", (uint32_t)canAdd);
#else
				ChatPacket(CHAT_TYPE_INFO, "ItemShop ki van kapcsolva.");
#endif
			}
			break;

			// --------------------------------------------------------------
			// Gyümölcs – +2000 RP (1 óránként használható)
			// --------------------------------------------------------------
			case 71107:
			case 39032:
			{
				const uint32_t max_limit = 25000000;
				const uint32_t current = GetAlignment();

				if (current >= max_limit)
				{
					ChatPacket(CHAT_TYPE_INFO, "Max 2.500.000 with this item!");
					return false;
				}

				const uint32_t add_value = item->GetValue(0);
				const uint32_t real_add = std::min(add_value, max_limit - current);

				UpdateAlignment(real_add);
				item->SetCount(item->GetCount() - 1);

				ChatPacket(CHAT_TYPE_INFO, "+2000 point");
			}
			break;

			case 72101:
			{
				const uint32_t min_limit = 25000000; // 2.500.000 lathato rang
				const uint32_t max_limit = 50000000; // 5.000.000 lathato rang
				const uint32_t current = GetAlignment();

				if (current < min_limit)
				{
					ChatPacket(CHAT_TYPE_INFO, "Min point: 2.500.000 ");
					return false;
				}

				if (current >= max_limit)
				{
					ChatPacket(CHAT_TYPE_INFO, "Max 5.000.000!");
					return false;
				}

				// 10.000  rang = 100.000 belso alignment
				const uint32_t add_value = 300000;
				const uint32_t real_add = std::min(add_value, max_limit - current);

				UpdateAlignment(real_add);
				item->SetCount(item->GetCount() - 1);

				ChatPacket(CHAT_TYPE_INFO, "+30.000 point addaed.");
			}
			break;
			// --------------------------------------------------------------
			// Arany Gyümölcs – +10000 RP (1 óránként használható)
			// --------------------------------------------------------------
			case 72100:
			{
				const uint32_t max_limit = 25000000;
				const uint32_t current = GetAlignment();

				if (current >= max_limit)
				{
					ChatPacket(CHAT_TYPE_INFO, "Max 2.500.000!");
					return false;
				}

				const uint32_t add_value = item->GetValue(0);
				const uint32_t real_add = std::min(add_value, max_limit - current);

				UpdateAlignment(real_add);
				item->SetCount(item->GetCount() - 1);

				ChatPacket(CHAT_TYPE_INFO, "+10.000.");
			}
			break;
			break;
			case 39069:
			case 80003:
			case 80004:
			case 80005:
			case 80006:
			case 80007:
			case 80008:
			{
				const uint32_t count = item->GetCount();
				if (count == 0)
					break;

				const uint64_t unitPrice = (uint64_t)item->GetShopBuyPrice();
				if (unitPrice == 0)
					break;

				const uint64_t curGold = (uint64_t)GetGold();
				const uint64_t maxGold = (uint64_t)GOLD_MAX;

				if (curGold >= maxGold)
					break;

				const uint64_t freeSpace = maxGold - curGold;
				if (freeSpace < unitPrice)
					break;

				 
				uint32_t canUse = (uint32_t)(freeSpace / unitPrice);
				if (canUse > count)
					canUse = count;

				if (canUse == 0)
					break;

				const uint64_t canAdd = unitPrice * (uint64_t)canUse;

				 
				GiveGold((long long)canAdd);

				 
				item->SetCount(count - canUse);
			}
			break;


			//case 71107: // Ãµµµº¹¼þ¾Æ
//			{
//				uint32_t val = item->GetValue(0);
//				int interval = item->GetValue(1);
//				quest::PC* pPC = quest::CQuestManager::instance().GetPC(GetPlayerID());
//				int last_use_time = pPC->GetFlag("mythical_peach.last_use_time");
//
//				if (get_global_time() - last_use_time < interval * 60 * 60)
//				{
//#ifdef TEXTS_IMPROVEMENT
//					ChatPacketNew(CHAT_TYPE_INFO, 508, "");
//#endif
//					return false;
//				}
//
//				if (GetAlignment() == 25000000)
//				{
//#ifdef TEXTS_IMPROVEMENT
//					ChatPacketNew(CHAT_TYPE_INFO, 674, "%d", 25000000);
//#endif
//					return false;
//				}
//
//				if (25000000 - GetAlignment() < val * 10)
//				{
//					val = (25000000 - GetAlignment()) / 10;
//				}
//
//				uint32_t old_alignment = GetAlignment() / 10;
//
//				UpdateAlignment(val * 10);
//
//				item->SetCount(item->GetCount() - 1);
//				pPC->SetFlag("mythical_peach.last_use_time", get_global_time());
//
//#ifdef TEXTS_IMPROVEMENT
//				ChatPacketNew(CHAT_TYPE_INFO, 327, "%d", val);
//#endif
//
//				char buf[256 + 1];
//				snprintf(buf, sizeof(buf), "%u %u", old_alignment, GetAlignment() / 10);
//				LogManager::instance().CharLog(this, val, "MYTHICAL_PEACH", buf);
//			}
//			break;

			case 71109: // Å»¼®¼­
			case 72719:
			{
				LPITEM item2;

				if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
					return false;

				if (item2->IsExchanging() || item2->IsEquipped()) // @fixme114
					return false;

				if (item2->GetSocketCount() == 0)
					return false;

#ifdef ENABLE_BUG_FIXES
				if (item2->IsEquipped())
					return false;
#endif

				switch (item2->GetType())
				{
				case ITEM_WEAPON:
					break;
				case ITEM_ARMOR:
					switch (item2->GetSubType())
					{
					case ARMOR_EAR:
					case ARMOR_WRIST:
					case ARMOR_NECK:
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 675, "%s", item->GetName());
#endif
						return false;
					}
					break;

				default:
					return false;
				}

				std::stack<int32_t> socket;

				for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
					socket.push(item2->GetSocket(i));

				int idx = ITEM_SOCKET_MAX_NUM - 1;

				while (socket.size() > 0)
				{
					if (socket.top() > 2 && socket.top() != ITEM_BROKEN_METIN_VNUM)
						break;

					idx--;
					socket.pop();
				}

				if (socket.size() == 0)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 675, "%s", item2->GetName());
#endif
					return false;
				}

				LPITEM pItemReward = AutoGiveItem(socket.top());

				if (pItemReward != nullptr)
				{
					item2->SetSocket(idx, 1);

					char buf[256 + 1];
					snprintf(buf, sizeof(buf), "%s(%u) %s(%u)",
						item2->GetName(), item2->GetID(), pItemReward->GetName(), pItemReward->GetID());
					LogManager::instance().ItemLog(this, item, "USE_DETACHMENT_ONE", buf);

					item->SetCount(item->GetCount() - 1);
				}
			}
			break;

			case 70201:   // Å»»öÁ¦
			case 70202:   // ¿°»ö¾à(Èò»ö)
			case 70203:   // ¿°»ö¾à(±Ý»ö)
			case 70204:   // ¿°»ö¾à(»¡°£»ö)
			case 70205:   // ¿°»ö¾à(°¥»ö)
			case 70206:   // ¿°»ö¾à(°ËÀº»ö)
			{
				if (GetPart(PART_HAIR) < 1001)
				{
					quest::CQuestManager& q = quest::CQuestManager::instance();
					quest::PC* pPC = q.GetPC(GetPlayerID());

					if (pPC)
					{
						int last_dye_level = pPC->GetFlag("dyeing_hair.last_dye_level");

						if (last_dye_level == 0 ||
							last_dye_level + 3 <= GetLevel() ||
							item->GetVnum() == 70201)
						{
							SetPart(PART_HAIR, item->GetVnum() - 70201);

							if (item->GetVnum() == 70201)
								pPC->SetFlag("dyeing_hair.last_dye_level", 0);
							else
								pPC->SetFlag("dyeing_hair.last_dye_level", GetLevel());

							item->SetCount(item->GetCount() - 1);
							UpdatePacket();
						}
#ifdef TEXTS_IMPROVEMENT
						else {
							ChatPacketNew(CHAT_TYPE_INFO, 97, "%d", last_dye_level + 3);
						}
#endif
					}
				}
#ifdef TEXTS_IMPROVEMENT
				else {
					ChatPacketNew(CHAT_TYPE_INFO, 491, "");
				}
#endif
			}
			break;

			case ITEM_NEW_YEAR_GREETING_VNUM:
			{
				uint32_t dwBoxVnum = ITEM_NEW_YEAR_GREETING_VNUM;
				std::vector <uint32_t> dwVnums;
				std::vector <uint32_t> dwCounts;
				std::vector <LPITEM> item_gets;
				int count = 0;

				if (GiveItemFromSpecialItemGroup(dwBoxVnum, dwVnums, dwCounts, item_gets, count))
				{
#ifdef TEXTS_IMPROVEMENT
					for (int i = 0; i < count; i++) {
						if (dwVnums[i] == CSpecialItemGroup::GOLD) {
							ChatPacketNew(CHAT_TYPE_INFO, 102, "%d", dwCounts[i]);
						}
					}
#endif
					item->SetCount(item->GetCount() - 1);
				}
			}
			break;

			case ITEM_VALENTINE_ROSE:
			case ITEM_VALENTINE_CHOCOLATE:
			{
				uint32_t dwBoxVnum = item->GetVnum();
				std::vector <uint32_t> dwVnums;
				std::vector <uint32_t> dwCounts;
				std::vector <LPITEM> item_gets(0);
				int count = 0;

				if (item->GetVnum() == ITEM_VALENTINE_ROSE && SEX_MALE == GET_SEX(this)) {
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 383, "");
#endif
					return false;
				}
				else if (item->GetVnum() == ITEM_VALENTINE_CHOCOLATE && SEX_FEMALE == GET_SEX(this)) {
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 382, "");
#endif
					return false;
				}

				if (GiveItemFromSpecialItemGroup(dwBoxVnum, dwVnums, dwCounts, item_gets, count))
					item->SetCount(item->GetCount() - 1);
			}
			break;

			case ITEM_WHITEDAY_CANDY:
			case ITEM_WHITEDAY_ROSE:
			{
				uint32_t dwBoxVnum = item->GetVnum();
				std::vector <uint32_t> dwVnums;
				std::vector <uint32_t> dwCounts;
				std::vector <LPITEM> item_gets(0);
				int count = 0;

				if (item->GetVnum() == ITEM_WHITEDAY_ROSE && SEX_MALE == GET_SEX(this)) {
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 383, "");
#endif
					return false;
				}
				else if (item->GetVnum() == ITEM_WHITEDAY_CANDY && SEX_FEMALE == GET_SEX(this)) {
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 382, "");
#endif
					return false;
				}

				if (GiveItemFromSpecialItemGroup(dwBoxVnum, dwVnums, dwCounts, item_gets, count))
					item->SetCount(item->GetCount() - 1);
			}
			break;

			case 50011: // ¿ù±¤º¸ÇÕ
			{
				uint32_t dwBoxVnum = 50011;
				std::vector <uint32_t> dwVnums;
				std::vector <uint32_t> dwCounts;
				std::vector <LPITEM> item_gets(0);
				int count = 0;

				if (GiveItemFromSpecialItemGroup(dwBoxVnum, dwVnums, dwCounts, item_gets, count))
				{
					for (int i = 0; i < count; i++)
					{
						char buf[50 + 1];
						snprintf(buf, sizeof(buf), "%u %u", dwVnums[i], dwCounts[i]);
						LogManager::instance().ItemLog(this, item, "MOONLIGHT_GET", buf);

						//ITEM_MANAGER::instance().RemoveItem(item);
						item->SetCount(item->GetCount() - 1);

						switch (dwVnums[i])
						{
						case CSpecialItemGroup::GOLD:
							break;
						case CSpecialItemGroup::EXP:
							break;

						case CSpecialItemGroup::MOB:
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 378, "");
#endif
							break;

						case CSpecialItemGroup::SLOW:
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 377, "");
#endif
							break;

						case CSpecialItemGroup::DRAIN_HP:
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 373, "");
#endif
							break;

						case CSpecialItemGroup::POISON:
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 376, "");
#endif
							break;
#ifdef ENABLE_WOLFMAN_CHARACTER
						case CSpecialItemGroup::BLEEDING:
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 379, "");
#endif
							break;
#endif
						case CSpecialItemGroup::MOB_GROUP:
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 380, "");
#endif
							break;

						default:
							//#ifdef TEXTS_IMPROVEMENT
							//												if (item_gets[i]) {
							//													if (dwCounts[i] > 1) {
							//														ChatPacketNew(CHAT_TYPE_INFO, 374, "%d#%s", dwCounts[i], item_gets[i]->GetName());
							//													} else {
							//														ChatPacketNew(CHAT_TYPE_INFO, 375, "%s", item_gets[i]->GetName());
							//													}
							//												}
							//#endif
							break;
						}
					}
				}
				else
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 395, "");
#endif
					return false;
				}
			}
			break;

			case ITEM_GIVE_STAT_RESET_COUNT_VNUM:
			{
				//PointChange(POINT_GOLD, -iCost);
				PointChange(POINT_STAT_RESET_COUNT, 1);
				item->SetCount(item->GetCount() - 1);
			}
			break;

			case 50107:
			{
				if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 667, "");
#endif
					return false;
				}
#ifdef ENABLE_NEWSTUFF
				else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(GetPlayerID()) && !IsAllowedPotionOnPVP(item->GetVnum()))
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 667, "");
#endif
					return false;
				}
#endif

				EffectPacket(SE_CHINA_FIREWORK);
#ifdef ENABLE_FIREWORK_STUN
				// ½ºÅÏ °ø°ÝÀ» ¿Ã·ÁÁØ´Ù
				AddAffect(AFFECT_CHINA_FIREWORK, POINT_STUN_PCT, 30, AFF_CHINA_FIREWORK, 5 * 60, 0, true);
#endif
				item->SetCount(item->GetCount() - 1);
			}
			break;

			case 50108:
			{
				if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 667, "");
#endif
					return false;
				}
#ifdef ENABLE_NEWSTUFF
				else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(GetPlayerID()) && !IsAllowedPotionOnPVP(item->GetVnum()))
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 667, "");
#endif
					return false;
				}
#endif

				EffectPacket(SE_SPIN_TOP);
#ifdef ENABLE_FIREWORK_STUN
				// ½ºÅÏ °ø°ÝÀ» ¿Ã·ÁÁØ´Ù
				AddAffect(AFFECT_CHINA_FIREWORK, POINT_STUN_PCT, 30, AFF_CHINA_FIREWORK, 5 * 60, 0, true);
#endif
				item->SetCount(item->GetCount() - 1);
			}
			break;

			case ITEM_WONSO_BEAN_VNUM:
				PointChange(POINT_HP, GetMaxHP() - GetHP());
				item->SetCount(item->GetCount() - 1);
				break;

			case ITEM_WONSO_SUGAR_VNUM:
				PointChange(POINT_SP, GetMaxSP() - GetSP());
				item->SetCount(item->GetCount() - 1);
				break;

			case ITEM_WONSO_FRUIT_VNUM:
				PointChange(POINT_STAMINA, GetMaxStamina() - GetStamina());
				item->SetCount(item->GetCount() - 1);
				break;

			case 90008: // VCARD
			case 90009: // VCARD
				VCardUse(this, this, item);
				break;

			case ITEM_ELK_VNUM: // µ·²Ù·¯¹Ì
			{
				int iGold = item->GetSocket(0);
				ITEM_MANAGER::instance().RemoveItem(item);
				PointChange(POINT_GOLD, iGold);
			}
			break;
			case 27995:
			{
			}
			break;

			case 71092: // º¯½Å ÇØÃ¼ºÎ ÀÓ½Ã
			{
				if (m_pkChrTarget != nullptr)
				{
					if (m_pkChrTarget->IsPolymorphed())
					{
						m_pkChrTarget->SetPolymorph(0);
						m_pkChrTarget->RemoveAffect(AFFECT_POLYMORPH);
					}
				}
				else
				{
					if (IsPolymorphed())
					{
						SetPolymorph(0);
						RemoveAffect(AFFECT_POLYMORPH);
					}
				}
			}
			break;

			case 30617: // ÁøÀç°¡
			{
				// À¯·´, ½Ì°¡Æú, º£Æ®³² ÁøÀç°¡ »ç¿ë±ÝÁö
				LPITEM item2;

				if (!IsValidItemPosition(DestCell) || !(item2 = GetInventoryItem(wDestCell)))
					return false;

				if (ITEM_COSTUME == item2->GetType())
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
					return false;
				}

				if (item2->IsExchanging() || item2->IsEquipped()) // @fixme114
					return false;

				if (item2->GetAttributeSetIndex() == -1)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
					return false;
				}

				if (item2->AddRareAttribute() == true)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 389, "");
#endif
					int iAddedIdx = item2->GetRareAttrCount() + 4;
					char buf[21];
					snprintf(buf, sizeof(buf), "%u", item2->GetID());

					LogManager::instance().ItemLog(
						GetPlayerID(),
						item2->GetAttributeType(iAddedIdx),
						item2->GetAttributeValue(iAddedIdx),
						item->GetID(),
						"ADD_RARE_ATTR",
						buf,
						GetDesc()->GetHostName(),
						item->GetOriginalVnum());

					item->SetCount(item->GetCount() - 1);
				}
#ifdef TEXTS_IMPROVEMENT
				else {
					ChatPacketNew(CHAT_TYPE_INFO, 308, "");
				}
#endif
			}
			break;

			case 30618: // ÁøÀç°æ
			{
				// À¯·´, ½Ì°¡Æú, º£Æ®³² ÁøÀç°¡ »ç¿ë±ÝÁö
				LPITEM item2;

				if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
					return false;

				if (ITEM_COSTUME == item2->GetType()) // @fixme124
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
					return false;
				}

				if (item2->IsExchanging() || item2->IsEquipped()) // @fixme114
					return false;

				if (item2->GetAttributeSetIndex() == -1)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
					return false;
				}

				if (item2->ChangeRareAttribute() == true)
				{
					char buf[21];
					snprintf(buf, sizeof(buf), "%u", item2->GetID());
					LogManager::instance().ItemLog(this, item, "CHANGE_RARE_ATTR", buf);

					item->SetCount(item->GetCount() - 1);
				}
#ifdef TEXTS_IMPROVEMENT
				else {
					ChatPacketNew(CHAT_TYPE_INFO, 354, "");
				}
#endif
			}
			break;
#ifdef ENABLE_CHANGE_NORMAL_HIT_RAZOR93
			case 70251: // ÁøÀç°æ
			{
				// À¯·´, ½Ì°¡Æú, º£Æ®³² ÁøÀç°¡ »ç¿ë±ÝÁö
				LPITEM item2;

				if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
					return false;

				if (ITEM_COSTUME == item2->GetType()) // @fixme124
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
					return false;
				}

				if (item2->IsExchanging() || item2->IsEquipped()) // @fixme114
					return false;

				if (item2->GetAttributeSetIndex() == -1)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
					return false;
				}

				if (item2->ChangeKKAK() == true)
				{
					char buf[21];
					snprintf(buf, sizeof(buf), "%u", item2->GetID());
					LogManager::instance().ItemLog(this, item, "CHANGE_RARE_ATTR21", buf);

					item->SetCount(item->GetCount() - 1);
				}
#ifdef TEXTS_IMPROVEMENT
				else {
					ChatPacketNew(CHAT_TYPE_INFO, 354, "");
				}
#endif
			}
			break;

#endif

			case ITEM_AUTO_HP_RECOVERY_S:
			case ITEM_AUTO_HP_RECOVERY_M:
			case ITEM_AUTO_HP_RECOVERY_L:
			case ITEM_AUTO_HP_RECOVERY_X:
			case ITEM_AUTO_SP_RECOVERY_S:
			case ITEM_AUTO_SP_RECOVERY_M:
			case ITEM_AUTO_SP_RECOVERY_L:
			case ITEM_AUTO_SP_RECOVERY_X:
				// ¹«½Ã¹«½ÃÇÏÁö¸¸ ÀÌÀü¿¡ ÇÏ´ø °É °íÄ¡±â´Â ¹«¼·°í...
				// ±×·¡¼­ ±×³É ÇÏµå ÄÚµù. ¼±¹° »óÀÚ¿ë ÀÚµ¿¹°¾à ¾ÆÀÌÅÛµé.
			case REWARD_BOX_ITEM_AUTO_SP_RECOVERY_XS:
			case REWARD_BOX_ITEM_AUTO_SP_RECOVERY_S:
			case REWARD_BOX_ITEM_AUTO_HP_RECOVERY_XS:
			case REWARD_BOX_ITEM_AUTO_HP_RECOVERY_S:
			{
				if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 667, "");
#endif
					return false;
				}
#ifdef ENABLE_NEWSTUFF
				else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(GetPlayerID()) && !IsAllowedPotionOnPVP(item->GetVnum()))
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 667, "");
#endif
					return false;
				}
#endif

				EAffectTypes type = AFFECT_NONE;
				bool isSpecialPotion = false;

				switch (item->GetVnum())
				{
				case ITEM_AUTO_HP_RECOVERY_X:
					isSpecialPotion = true;

				case ITEM_AUTO_HP_RECOVERY_S:
				case ITEM_AUTO_HP_RECOVERY_M:
				case ITEM_AUTO_HP_RECOVERY_L:
				case REWARD_BOX_ITEM_AUTO_HP_RECOVERY_XS:
				case REWARD_BOX_ITEM_AUTO_HP_RECOVERY_S:
					type = AFFECT_AUTO_HP_RECOVERY;
					break;

				case ITEM_AUTO_SP_RECOVERY_X:
					isSpecialPotion = true;

				case ITEM_AUTO_SP_RECOVERY_S:
				case ITEM_AUTO_SP_RECOVERY_M:
				case ITEM_AUTO_SP_RECOVERY_L:
				case REWARD_BOX_ITEM_AUTO_SP_RECOVERY_XS:
				case REWARD_BOX_ITEM_AUTO_SP_RECOVERY_S:
					type = AFFECT_AUTO_SP_RECOVERY;
					break;
				}

				if (AFFECT_NONE == type)
					break;

				if (item->GetCount() > 1)
				{
#ifdef ENABLE_EXTRA_INVENTORY
					const bool bFromExtraInventory = (item->GetWindow() == EXTRA_INVENTORY);
#else
					const bool bFromExtraInventory = false;
#endif
					int pos = -1;

#ifdef ENABLE_EXTRA_INVENTORY
					if (bFromExtraInventory)
						pos = GetEmptyExtraInventory(item);
					else
#endif
						pos = GetEmptyInventory(item->GetSize());

					if (-1 == pos)
					{
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 366, "");
#endif
						break;
					}

					item->SetCount(item->GetCount() - 1);

					LPITEM item2 = ITEM_MANAGER::instance().CreateItem(item->GetVnum(), 1);

#ifdef ENABLE_EXTRA_INVENTORY
					if (bFromExtraInventory)
						item2->AddToCharacter(this, TItemPos(EXTRA_INVENTORY, pos));
					else
#endif
						item2->AddToCharacter(this, TItemPos(INVENTORY, pos));

					if (item->GetSocket(1) != 0)
					{
						item2->SetSocket(1, item->GetSocket(1));
					}

					if (FindAffect(type))
						return true;
					else if (isSpecialPotion) {
						EAffectTypes eType = type == AFFECT_AUTO_HP_RECOVERY ? AFFECT_AUTO_HP_RECOVERY2 : AFFECT_AUTO_SP_RECOVERY2;
						if (FindAffect(eType))
							return true;
					}

					item = item2;
				}

#ifdef ENABLE_NEW_USE_POTION
				EAffectTypes type2 = AFFECT_NONE;
				CAffect* pAffect2 = nullptr;
#endif
				CAffect* pAffect = FindAffect(type);

				if (nullptr == pAffect)
				{
					EPointTypes bonus = POINT_NONE;
					if (true == isSpecialPotion)
					{
						if (type == AFFECT_AUTO_HP_RECOVERY)
						{
#ifdef ENABLE_NEW_USE_POTION
							type2 = type;
							type = AFFECT_AUTO_HP_RECOVERY2;
#endif
							bonus = POINT_MAX_HP_PCT;
						}
						else if (type == AFFECT_AUTO_SP_RECOVERY)
						{
#ifdef ENABLE_NEW_USE_POTION
							type2 = type;
							type = AFFECT_AUTO_SP_RECOVERY2;
#endif
							bonus = POINT_MAX_SP_PCT;
						}
					}
#ifdef ENABLE_NEW_USE_POTION
					else {
						if (type == AFFECT_AUTO_HP_RECOVERY)
							type2 = AFFECT_AUTO_HP_RECOVERY2;
						else if (type == AFFECT_AUTO_SP_RECOVERY)
							type2 = AFFECT_AUTO_SP_RECOVERY2;
					}

					pAffect2 = FindAffect(type2);
					if (pAffect2) {
						if (item->GetID() == pAffect2->dwFlag)
						{
							RemoveAffect(pAffect2);
							item->Lock(false);
							item->SetSocket(0, false);
						}
						else
						{
							LPITEM old = FindItemByID(pAffect2->dwFlag);
							if (nullptr != old)
							{
								old->Lock(false);
								old->SetSocket(0, false);
							}

							RemoveAffect(pAffect2);
						}
					}
					else if (isSpecialPotion == true) {
						pAffect2 = FindAffect(type);
						if (pAffect2) {
							if (item->GetID() == pAffect2->dwFlag)
							{
								RemoveAffect(pAffect2);
								item->Lock(false);
								item->SetSocket(0, false);
								return true;
							}
							else {
								LPITEM old = FindItemByID(pAffect2->dwFlag);
								if (old)
								{
									old->Lock(false);
									old->SetSocket(0, false);
								}
							}
						}
					}
#endif

					AddAffect(type, bonus, 4, item->GetID(), INFINITE_AFFECT_DURATION, 0, true, false);
					item->Lock(true);
					item->SetSocket(0, true);
					AutoRecoveryItemProcess(type);
				}
				else
				{
					if (item->GetID() == pAffect->dwFlag)
					{
						RemoveAffect(pAffect);

						item->Lock(false);
						item->SetSocket(0, false);
					}
					else
					{
						LPITEM old = FindItemByID(pAffect->dwFlag);

						if (nullptr != old)
						{
							old->Lock(false);
							old->SetSocket(0, false);
						}

						RemoveAffect(pAffect);

						EPointTypes bonus = POINT_NONE;

						if (true == isSpecialPotion)
						{
							if (type == AFFECT_AUTO_HP_RECOVERY)
							{
#ifdef ENABLE_NEW_USE_POTION
								type2 = type;
								type = AFFECT_AUTO_HP_RECOVERY2;
#endif
								bonus = POINT_MAX_HP_PCT;
							}
							else if (type == AFFECT_AUTO_SP_RECOVERY)
							{
#ifdef ENABLE_NEW_USE_POTION
								type2 = type;
								type = AFFECT_AUTO_SP_RECOVERY2;
#endif
								bonus = POINT_MAX_SP_PCT;
							}
						}
#ifdef ENABLE_NEW_USE_POTION
						else {
							if (type == AFFECT_AUTO_HP_RECOVERY)
								type2 = AFFECT_AUTO_HP_RECOVERY2;
							else if (type == AFFECT_AUTO_SP_RECOVERY)
								type2 = AFFECT_AUTO_SP_RECOVERY2;
						}

						pAffect2 = FindAffect(type2);
						if (pAffect2) {
							if (item->GetID() == pAffect2->dwFlag)
							{
								RemoveAffect(pAffect2);
								item->Lock(false);
								item->SetSocket(0, false);
							}
							else
							{
								LPITEM old = FindItemByID(pAffect2->dwFlag);
								if (nullptr != old)
								{
									old->Lock(false);
									old->SetSocket(0, false);
								}

								RemoveAffect(pAffect2);
							}
						}
#endif

						AddAffect(type, bonus, 4, item->GetID(), INFINITE_AFFECT_DURATION, 0, true, false);

						item->Lock(true);
						item->SetSocket(0, true);

						AutoRecoveryItemProcess(type);
					}
				}
			}
			break;
			}
			break;

		case USE_CLEAR:
		{
			switch (item->GetVnum())
			{
#ifdef ENABLE_WOLFMAN_CHARACTER
			case 27124: // Bandage
				RemoveBleeding();
				break;
#endif
			case 27874: // Grilled Perch
			default:
				RemoveBadAffect();
				break;
			}
			item->SetCount(item->GetCount() - 1);
		}
		break;

		case USE_INVISIBILITY:
		{
			if (item->GetVnum() == 70026)
			{
				quest::CQuestManager& q = quest::CQuestManager::instance();
				quest::PC* pPC = q.GetPC(GetPlayerID());

				if (pPC != nullptr)
				{
					int last_use_time = pPC->GetFlag("mirror_of_disapper.last_use_time");

					if (get_global_time() - last_use_time < 10 * 60)
					{
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 508, "");
#endif
						return false;
					}

					pPC->SetFlag("mirror_of_disapper.last_use_time", get_global_time());
				}
			}

			AddAffect(AFFECT_INVISIBILITY, POINT_NONE, 0, AFF_INVISIBILITY, 300, 0, true);
			item->SetCount(item->GetCount() - 1);
		}
		break;

		case USE_POTION_NODELAY:
		{
			if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
			{
				if (quest::CQuestManager::instance().GetEventFlag("arena_potion_limit") > 0)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 303, "");
#endif
					return false;
				}

				switch (item->GetVnum())
				{
				case 70020:
				case 71018:
				case 71019:
				case 71020:
					if (quest::CQuestManager::instance().GetEventFlag("arena_potion_limit_count") < 10000)
					{
						if (m_nPotionLimit <= 0)
						{
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 362, "");
#endif
							return false;
						}
					}
					break;

				default:
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 303, "");
#endif
					return false;
				}
			}
#ifdef ENABLE_NEWSTUFF
			else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(GetPlayerID()) && !IsAllowedPotionOnPVP(item->GetVnum()))
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 667, "");
#endif
				return false;
			}
#endif

			bool used = false;

			if (item->GetValue(0) != 0) // HP Àý´ë°ª È¸º¹
			{
				if (GetHP() < GetMaxHP())
				{
					PointChange(POINT_HP, item->GetValue(0) * (100 + GetPoint(POINT_POTION_BONUS)) / 100);
					EffectPacket(SE_HPUP_RED);
					used = true;
				}
			}

			if (item->GetValue(1) != 0)	// SP Àý´ë°ª È¸º¹
			{
				if (GetSP() < GetMaxSP())
				{
					PointChange(POINT_SP, item->GetValue(1) * (100 + GetPoint(POINT_POTION_BONUS)) / 100);
					EffectPacket(SE_SPUP_BLUE);
					used = true;
				}
			}

			if (item->GetValue(3) != 0) // HP % È¸º¹
			{
				if (GetHP() < GetMaxHP())
				{
					PointChange(POINT_HP, item->GetValue(3) * GetMaxHP() / 100);
					EffectPacket(SE_HPUP_RED);
					used = true;
				}
			}

			if (item->GetValue(4) != 0) // SP % È¸º¹
			{
				if (GetSP() < GetMaxSP())
				{
					PointChange(POINT_SP, item->GetValue(4) * GetMaxSP() / 100);
					EffectPacket(SE_SPUP_BLUE);
					used = true;
				}
			}

			if (used)
			{
				if (item->GetVnum() == 50085 || item->GetVnum() == 50086) {
					SetUseSeedOrMoonBottleTime();
				}

				if (GetWarMap())
					GetWarMap()->UsePotion(this, item);

				m_nPotionLimit--;

				//RESTRICT_USE_SEED_OR_MOONBOTTLE
				item->SetCount(item->GetCount() - 1);
				//END_RESTRICT_USE_SEED_OR_MOONBOTTLE
			}
		}
		break;

		case USE_POTION:
			if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
			{
				if (quest::CQuestManager::instance().GetEventFlag("arena_potion_limit") > 0)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 303, "");
#endif
					return false;
				}
			}
#ifdef ENABLE_NEWSTUFF
			else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(GetPlayerID()) && !IsAllowedPotionOnPVP(item->GetVnum()))
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 667, "");
#endif
				return false;
			}
#endif

			if (item->GetValue(1) != 0)
			{
				if (GetPoint(POINT_SP_RECOVERY) + GetSP() >= GetMaxSP())
				{
					return false;
				}

				PointChange(POINT_SP_RECOVERY, item->GetValue(1) * std::min((int64_t)200, (100 + GetPoint(POINT_POTION_BONUS))) / 100);
				StartAffectEvent();
				EffectPacket(SE_SPUP_BLUE);
			}

			if (item->GetValue(0) != 0)
			{
				if (GetPoint(POINT_HP_RECOVERY) + GetHP() >= GetMaxHP())
				{
					return false;
				}

				PointChange(POINT_HP_RECOVERY, item->GetValue(0) * std::min((int64_t)200, (100 + GetPoint(POINT_POTION_BONUS))) / 100);
				StartAffectEvent();
				EffectPacket(SE_HPUP_RED);
			}

			if (GetWarMap())
				GetWarMap()->UsePotion(this, item);

			item->SetCount(item->GetCount() - 1);
			m_nPotionLimit--;
			break;

		case USE_POTION_CONTINUE:
		{
			if (item->GetValue(0) != 0)
			{
				AddAffect(AFFECT_HP_RECOVER_CONTINUE, POINT_HP_RECOVER_CONTINUE, item->GetValue(0), 0, item->GetValue(2), 0, true);
			}
			else if (item->GetValue(1) != 0)
			{
				AddAffect(AFFECT_SP_RECOVER_CONTINUE, POINT_SP_RECOVER_CONTINUE, item->GetValue(1), 0, item->GetValue(2), 0, true);
			}
			else
				return false;
		}

		if (GetWarMap())
			GetWarMap()->UsePotion(this, item);

		item->SetCount(item->GetCount() - 1);
		break;

		case USE_ABILITY_UP:
		{
			switch (item->GetValue(0))
			{
			case APPLY_MOV_SPEED:
				AddAffect(AFFECT_MOV_SPEED, POINT_MOV_SPEED, item->GetValue(2), AFF_MOV_SPEED_POTION, item->GetValue(1), 0, true);
#ifdef ENABLE_EFFECT_EXTRAPOT
				EffectPacket(SE_DXUP_PURPLE);
#endif
				break;

			case APPLY_ATT_SPEED:
				AddAffect(AFFECT_ATT_SPEED, POINT_ATT_SPEED, item->GetValue(2), AFF_ATT_SPEED_POTION, item->GetValue(1), 0, true);
#ifdef ENABLE_EFFECT_EXTRAPOT
				EffectPacket(SE_SPEEDUP_GREEN);
#endif
				break;

			case APPLY_STR:
				AddAffect(AFFECT_STR, POINT_ST, item->GetValue(2), 0, item->GetValue(1), 0, true);
				break;

			case APPLY_DEX:
				AddAffect(AFFECT_DEX, POINT_DX, item->GetValue(2), 0, item->GetValue(1), 0, true);
				break;

			case APPLY_CON:
				AddAffect(AFFECT_CON, POINT_HT, item->GetValue(2), 0, item->GetValue(1), 0, true);
				break;

			case APPLY_INT:
				AddAffect(AFFECT_INT, POINT_IQ, item->GetValue(2), 0, item->GetValue(1), 0, true);
				break;

			case APPLY_CAST_SPEED:
				AddAffect(AFFECT_CAST_SPEED, POINT_CASTING_SPEED, item->GetValue(2), 0, item->GetValue(1), 0, true);
				break;

			case APPLY_ATT_GRADE_BONUS:
				AddAffect(AFFECT_ATT_GRADE, POINT_ATT_GRADE_BONUS,
					item->GetValue(2), 0, item->GetValue(1), 0, true);
				break;

			case APPLY_DEF_GRADE_BONUS:
				AddAffect(AFFECT_DEF_GRADE, POINT_DEF_GRADE_BONUS,
					item->GetValue(2), 0, item->GetValue(1), 0, true);
				break;
			}
		}

		if (GetWarMap())
			GetWarMap()->UsePotion(this, item);

		item->SetCount(item->GetCount() - 1);
		break;

		case USE_TALISMAN:
		{
			const int TOWN_PORTAL = 1;
			const int MEMORY_PORTAL = 2;


			// gm_guild_build, oxevent ¸Ê¿¡¼­ ±ÍÈ¯ºÎ ±ÍÈ¯±â¾ïºÎ ¸¦ »ç¿ë¸øÇÏ°Ô ¸·À½
			if (GetMapIndex() == 200 || GetMapIndex() == 113)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 489, "");
#endif
				return false;
			}

			if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 667, "");
#endif
				return false;
			}
#ifdef ENABLE_NEWSTUFF
			else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(GetPlayerID()) && !IsAllowedPotionOnPVP(item->GetVnum()))
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 667, "");
#endif
				return false;
			}
#endif

			if (m_pkWarpEvent)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 434, "");
#endif
				return false;
			}

			// CONSUME_LIFE_WHEN_USE_WARP_ITEM
			int consumeLife = CalculateConsume(this);

			if (consumeLife < 0)
				return false;
			// END_OF_CONSUME_LIFE_WHEN_USE_WARP_ITEM

			if (item->GetValue(0) == TOWN_PORTAL) // ±ÍÈ¯ºÎ
			{
				if (item->GetSocket(0) == 0)
				{
					if (!GetDungeon())
						if (!GiveRecallItem(item))
							return false;

					PIXEL_POSITION posWarp;

					if (SECTREE_MANAGER::instance().GetRecallPositionByEmpire(GetMapIndex(), GetEmpire(), posWarp))
					{
						// CONSUME_LIFE_WHEN_USE_WARP_ITEM
						PointChange(POINT_HP, -consumeLife, false);
						// END_OF_CONSUME_LIFE_WHEN_USE_WARP_ITEM

						WarpSet(posWarp.x, posWarp.y);
					}
					else
					{
						sys_err("CHARACTER::UseItem : cannot find spawn position (name %s, %d x %d)", GetName(), GetX(), GetY());
					}
				}
				else
				{
#ifdef TEXTS_IMPROVEMENT
					if (test_server) {
						ChatPacketNew(CHAT_TYPE_INFO, 415, "");
					}
#endif
					ProcessRecallItem(item);
				}
			}
			else if (item->GetValue(0) == MEMORY_PORTAL) // ±ÍÈ¯±â¾ïºÎ
			{
				if (item->GetSocket(0) == 0)
				{
					if (GetDungeon())
					{
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 310, "%s", item->GetName());
#endif
						return false;
					}

					if (!GiveRecallItem(item))
						return false;
				}
				else
				{
					// CONSUME_LIFE_WHEN_USE_WARP_ITEM
					PointChange(POINT_HP, -consumeLife, false);
					// END_OF_CONSUME_LIFE_WHEN_USE_WARP_ITEM

					ProcessRecallItem(item);
				}
			}
		}
		break;
#ifdef ENABLE_ATTR_COSTUMES
		case USE_CHANGE_ATTR_COSTUME:
		{
			LPITEM item2;
			if ((!IsValidItemPosition(DestCell)) || (!(item2 = GetItem(DestCell))))
				return false;

			if (item2->IsEquipped())
				BuffOnAttr_RemoveBuffsFromItem(item2);

			if (item2->GetType() != ITEM_COSTUME)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
				return false;
			}

			if ((item2->GetSubType() != COSTUME_BODY) && (item2->GetSubType() != COSTUME_HAIR) && (item2->GetSubType() != COSTUME_WEAPON)) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
				return false;
			}

			if ((item2->IsExchanging()) || (item2->IsEquipped()))
				return false;

			if (item2->GetAttributeSetIndex() == -1)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
				return false;
			}
			else if (item2->GetAttributeCount() == 0)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 354, "");
#endif
				return false;
			}

			item2->ChangeAttribute();

			{
				char buf[21];
				snprintf(buf, sizeof(buf), "%u", item2->GetID());
				LogManager::instance().ItemLog(this, item, "CHANGE_COSTUME_ATTR", buf);
			}

#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 392, "");
#endif
			item->SetCount(item->GetCount() - 1);
			break;
		}
		case USE_ADD_ATTR_COSTUME1:
		case USE_ADD_ATTR_COSTUME2:
		{
			LPITEM item2;
			if ((!IsValidItemPosition(DestCell)) || (!(item2 = GetItem(DestCell))))
				return false;

			if (item2->IsEquipped())
				BuffOnAttr_RemoveBuffsFromItem(item2);

			if (item2->GetType() != ITEM_COSTUME)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
				return false;
			}

			if ((item2->GetSubType() != COSTUME_BODY) && (item2->GetSubType() != COSTUME_HAIR) && (item2->GetSubType() != COSTUME_WEAPON)) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
				return false;
			}

			if ((item2->IsExchanging()) || (item2->IsEquipped()))
				return false;

			if (item2->GetAttributeSetIndex() == -1)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
				return false;
			}

			if ((item2->GetAttributeType(ITEM_ATTRIBUTE_MAX_NUM - 2) != 0) && (item2->GetAttributeType(ITEM_ATTRIBUTE_MAX_NUM - 1) != 0))
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 87, "");
#endif
				return false;
			}

			uint8_t bAttrSocket = item2->GetAttributeType(ITEM_ATTRIBUTE_MAX_NUM - 2) == 0 ? ITEM_ATTRIBUTE_MAX_NUM - 2 : ITEM_ATTRIBUTE_MAX_NUM - 1;
			uint8_t bAttrSocketCheck = bAttrSocket == ITEM_ATTRIBUTE_MAX_NUM - 2 ? ITEM_ATTRIBUTE_MAX_NUM - 1 : ITEM_ATTRIBUTE_MAX_NUM - 2;
			if (item2->GetAttributeType(bAttrSocketCheck) == item->GetSocket(0))
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 88, "");
#endif
				return false;
			}

			item2->SetForceAttribute(bAttrSocket, item->GetSocket(0), item->GetSocket(1));

			{
				char buf[21];
				snprintf(buf, sizeof(buf), "%u", item2->GetID());
				LogManager::instance().ItemLog(this, item, "ADD_COSTUME_ATTR", buf);
			}

#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 677, "");
#endif
			item->SetCount(item->GetCount() - 1);
			break;
		}
		case USE_REMOVE_ATTR_COSTUME:
		{
			LPITEM item2;
			if ((!IsValidItemPosition(DestCell)) || (!(item2 = GetItem(DestCell))))
				return false;

			if (item2->IsEquipped())
				BuffOnAttr_RemoveBuffsFromItem(item2);

			if (item2->GetType() != ITEM_COSTUME)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
				return false;
			}

			if ((item2->GetSubType() != COSTUME_BODY) && (item2->GetSubType() != COSTUME_HAIR) && (item2->GetSubType() != COSTUME_WEAPON)) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
				return false;
			}

			if ((item2->IsExchanging()) || (item2->IsEquipped()))
				return false;

			if (item2->GetAttributeSetIndex() == -1)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
				return false;
			}

			if ((item2->GetAttributeType(ITEM_ATTRIBUTE_MAX_NUM - 2) == 0) && (item2->GetAttributeType(ITEM_ATTRIBUTE_MAX_NUM - 1) == 0))
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 89, "");
#endif
				return false;
			}

			int iAttrSocket = GetAttrDialogRemove();
			if ((iAttrSocket == 0) && (item2->GetAttributeType(ITEM_ATTRIBUTE_MAX_NUM - 1) != 0))
			{
				item2->SetForceAttribute(ITEM_ATTRIBUTE_MAX_NUM - 2, item2->GetAttributeType(ITEM_ATTRIBUTE_MAX_NUM - 1), item2->GetAttributeValue(ITEM_ATTRIBUTE_MAX_NUM - 1));
				item2->SetForceAttribute(ITEM_ATTRIBUTE_MAX_NUM - 1, 0, 0);
			}
			else
				item2->SetForceAttribute(ITEM_ATTRIBUTE_MAX_NUM - 2 + iAttrSocket, 0, 0);

			{
				char buf[21];
				snprintf(buf, sizeof(buf), "%u", item2->GetID());
				LogManager::instance().ItemLog(this, item, "REMOVE_COSTUME_ATTR", buf);
			}

#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 90, "");
#endif
			item->SetCount(item->GetCount() - 1);
			break;
		}
#endif
#ifdef ENABLE_STOLE_COSTUME
		case USE_ENCHANT_STOLE: {
			LPITEM item2;
			if ((!IsValidItemPosition(DestCell)) || (!(item2 = GetItem(DestCell))))
				return false;

			if ((item2->GetType() != ITEM_COSTUME) || (item2->GetSubType() != COSTUME_STOLE)) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 22, "%s", item->GetName());
#endif
				return false;
			}

			if ((item2->IsExchanging()) || (item2->IsEquipped()) || (item2->isLocked()))
				return false;

			uint8_t bGrade = item2->GetValue(0);
			if (bGrade < 1)
				return false;

			bGrade = bGrade > 4 ? 4 : bGrade;
			uint8_t bRandom = (bGrade * 4);
			for (int i = 0; i < MAX_ATTR; i++) {
				item2->SetForceAttribute(i, stoleInfoTable[i][0], stoleInfoTable[i][number(bRandom - 3, bRandom)]);
			}

#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 21, "%s", item2->GetName());
#endif
			item->SetCount(item->GetCount() - 1);
			break;
		}
#endif
#ifdef ENABLE_DS_ENCHANT
		case USE_DS_ENCHANT: {
			LPITEM item2;
			if ((!IsValidItemPosition(DestCell)) || (!(item2 = GetItem(DestCell))))
				return false;

			if (!item2->IsDragonSoul()) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 73, "");
#endif
				return false;
			}

			if ((DragonSoul_IsDeckActivated()) && (item2->IsEquipped())) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 76, "");
#endif
				return false;
			}

			if (item2->IsExchanging() /*|| item2->IsEquipped()*/) // ENABLE_BUG_FIXES
				return false;

			int iGrade = (item2->GetVnum() / 1000) % 10, iStep = (item2->GetVnum() / 100) % 10;
			if ((iGrade !=
#ifdef ENABLE_DS_GRADE_MYTH
				DRAGON_SOUL_GRADE_MYTH
#else
				DRAGON_SOUL_GRADE_LEGENDARY
#endif
				) || (iStep != DRAGON_SOUL_STEP_HIGHEST)) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 75, "");
#endif
				return false;
			}

			for (int i = 0; i < ITEM_ATTRIBUTE_RARE_END; i++)
				item2->SetForceAttribute(i, 0, 0);

			bool bRet = DSManager::instance().PutAttributes(item2);
			if (!bRet)
				return false;

			{
				char buf[21];
				snprintf(buf, sizeof(buf), "%u", item2->GetID());
				LogManager::instance().ItemLog(this, item, "USE_DS_ENCHANT", buf);
			}

#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 74, "");
#endif
			item->SetCount(item->GetCount() - 1);
			break;
		}
#endif
#ifdef ENABLE_REMOTE_ATTR_SASH_REMOVE
		case USE_ATTR_SASH_REMOVE: {
			LPITEM item2;
			if ((!IsValidItemPosition(DestCell)) || (!(item2 = GetItem(DestCell))))
				return false;

			if ((item2->IsExchanging()) || (item2->IsEquipped()))
				return false;

			if ((item2->GetType() == ITEM_COSTUME) && (item2->GetSubType() == COSTUME_ACCE)) {
				if (item2->GetSocket(ACCE_ABSORBED_SOCKET) <= 0) {
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 71, "");
#endif
					return false;
				}

				bool bClean = CleanAcceAttr(item, item2);
				if (bClean) {
					{
						char buf[21];
						snprintf(buf, sizeof(buf), "%u", item2->GetID());
						LogManager::instance().ItemLog(this, item, "USE_ATTR_SASH_REMOVE", buf);
					}

#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 72, "");
#endif
				}

				return bClean;
			}
			else {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 70, "");
#endif
				return false;
			}
		}
#endif
#ifdef ENABLE_NEW_PET_EDITS
		case USE_PET_REVIVE: {
			LPITEM item2;
			if ((!IsValidItemPosition(DestCell)) || (!(item2 = GetItem(DestCell))))
				return false;

			if ((item2->GetVnum() < 55701) || (item2->GetVnum() > 55711)) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 66, "");
#endif
				return false;
			}

			if (item2->GetSocket(0) != 0) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 67, "");
#endif
				return false;
			}

			if (item2->GetSocket(2) == 0) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 64, "");
#endif
				return false;
			}

			if (item2->IsExchanging() || item2->IsEquipped()) // ENABLE_BUG_FIXES
				return false;

			if (item2->GetSocket(1) > int(1440 * 365)) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 69, "");
#endif
				return false;
			}

			int iLimit = int(1440 * 365);
			int iValue = item->GetValue(0);
			int iNewDuration = iValue == 0 ? 1440 * 366 : 1440 * iValue;
			iNewDuration += item2->GetSocket(1);
			if ((iNewDuration >= iLimit) && (item->GetVnum() != 86074)) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 68, "");
#endif
				return false;
			}

			iNewDuration = iNewDuration > iLimit ? iLimit : iNewDuration;
			if (item->GetVnum() == 86074)
				iNewDuration = 1440 * 366;

			item2->SetSocket(1, iNewDuration);
			item2->SetSocket(2, iNewDuration);
			std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("UPDATE player.new_petsystem SET duration = %d, tduration = %d WHERE id = %lu ", iNewDuration, iNewDuration, item2->GetID()));

			{
				char buf[21];
				snprintf(buf, sizeof(buf), "%u", item2->GetID());
				LogManager::instance().ItemLog(this, item, "USE_PET_REVIVE", buf);
			}

#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 65, "");
#endif
			item->SetCount(item->GetCount() - 1);
			break;
		}
		case USE_PET_ENCHANT: {
			LPITEM item2;
			if ((!IsValidItemPosition(DestCell)) || (!(item2 = GetItem(DestCell))))
				return false;

			if ((item2->GetVnum() < 55701) || (item2->GetVnum() > 55711)) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 66, "");
#endif
				return false;
			}

			if (item2->GetSocket(0) != 0) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 67, "");
#endif
				return false;
			}

			if (item2->IsExchanging() || item2->IsEquipped()) // ENABLE_BUG_FIXES
				return false;

			int idx = GetPetEnchant();
			if ((idx < 0) || (idx > 2))
				return false;

			int iValue = item2->GetAttributeValue(idx);
			if ((idx == 0) && (iValue >= 150)) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 63, "");
#endif
				return false;
			}

			if ((idx == 1) && (iValue >= 100)) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 63, "");
#endif
				return false;
			}

			if ((idx == 2) && (iValue >= 100)) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 63, "");
#endif
				return false;
			}

			{
				char buf[21];
				snprintf(buf, sizeof(buf), "%u", item2->GetID());
				LogManager::instance().ItemLog(this, item, "USE_PET_ENCHANT", buf);
			}

			if (number(1, 100) > 70) {
				int iMax;
				if (idx == 0)
					iMax = iValue + 5 > 150 ? 150 : iValue + 5;
				else
					iMax = iValue + 5 > 100 ? 100 : iValue + 5;

				item2->SetForceAttribute(idx, 1, iMax);
				std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("UPDATE player.new_petsystem SET bonus%d = %d WHERE id = %lu ", idx, iMax, item2->GetID()));

#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 61, "");
#endif
			}
#ifdef TEXTS_IMPROVEMENT
			else {
				ChatPacketNew(CHAT_TYPE_INFO, 62, "");
			}
#endif

			item->SetCount(item->GetCount() - 1);
			break;
		}
#endif
		case USE_TUNING:
		case USE_DETACHMENT:
		{
			LPITEM item2;

			if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
				return false;

			if (item2->IsExchanging() || item2->IsEquipped()) // @fixme114
				return false;

			if (item2->GetVnum() >= 28330 && item2->GetVnum() <= 28343) // ¿µ¼®+3
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 678, "%s", item->GetName());
#endif
				return false;
			}

#ifdef ENABLE_BUG_FIXES
			if (item2->IsEquipped())
				return false;
#endif

#ifdef ENABLE_ACCE_SYSTEM
			if (item->GetValue(0) == ACCE_CLEAN_ATTR_VALUE0)
			{
				if (!CleanAcceAttr(item, item2))
					return false;

				return true;
			}
#endif
			if (item2->GetVnum() >= 28430 && item2->GetVnum() <= 28443)  // ¿µ¼®+4
			{
				if (item->GetVnum() == 71056) // Ã»·æÀÇ¼û°á
				{
					RefineItem(item, item2);
				}
#ifdef TEXTS_IMPROVEMENT
				else {
					ChatPacketNew(CHAT_TYPE_INFO, 679, "%s", item->GetName());
				}
#endif
			}
			else
			{
				RefineItem(item, item2);
			}
		}
		break;
#ifdef ATTR_LOCK						
		case USE_ADD_ATTRIBUTE_LOCK:
		{
			LPITEM item2;
			if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
				return false;

			if (ITEM_COSTUME == item2->GetType() || item2->GetWearFlag() == WEARABLE_PENDANT || item2->IsDragonSoul())
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 791, "");
#endif
				return false;
			}

			if (item2->GetAttributeCount() < 5)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 792, "");
#endif
				return false;
			}

			if (item2->GetLockedAttr() != -1)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 793, "");
#endif
				return false;
			}

			if (item2->IsEquipped())
			{
				BuffOnAttr_RemoveBuffsFromItem(item2);
			}

			if (item2->IsExchanging() || item2->IsEquipped()) // @fixme114
				return false;

			item2->AddLockedAttr();
			item->SetCount(item->GetCount() - 1);
		}
		break;
		case USE_CHANGE_ATTRIBUTE_LOCK:
		{
			LPITEM item2;
			if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
				return false;


			if (item2->GetLockedAttr() == -1)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 795, "");
#endif
				return false;
			}

			if (ITEM_COSTUME == item2->GetType() /*|| item2->GetWearFlag() == WEARABLE_PENDANT*/ || item2->IsDragonSoul())
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 791, "");
#endif
				return false;
			}

			if (item2->IsEquipped())
			{
				BuffOnAttr_RemoveBuffsFromItem(item2);
			}


			if (item2->IsExchanging() || item2->IsEquipped()) // @fixme114
				return false;


			item2->ChangeLockedAttr();
			item->SetCount(item->GetCount() - 1);
		}
		break;
		case USE_DELETE_ATTRIBUTE_LOCK:
		{
			LPITEM item2;
			if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
				return false;

			if (item2->GetLockedAttr() == -1)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 795, "");
#endif
				return false;
			}

			if (item2->IsEquipped())
			{
				BuffOnAttr_RemoveBuffsFromItem(item2);
			}

			if (ITEM_COSTUME == item2->GetType() || item2->IsDragonSoul())
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 680, "");
#endif
				return false;
			}

			if (item2->IsExchanging() || item2->IsEquipped()) // @fixme114
				return false;

			item2->RemoveLockedAttr();
			item->SetCount(item->GetCount() - 1);
		}
		break;
#endif
		case USE_CHANGE_COSTUME_ATTR:
		case USE_RESET_COSTUME_ATTR:
		{
			LPITEM item2;
			if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
				return false;

			if (item2->IsEquipped())
			{
				BuffOnAttr_RemoveBuffsFromItem(item2);
			}

			if (ITEM_COSTUME != item2->GetType())
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
				return false;
			}

			{
				uint8_t bSubType = item2->GetSubType();
#ifdef ENABLE_ACCE_SYSTEM
				if (bSubType == COSTUME_ACCE)
					return false;
#endif

#ifdef ENABLE_STOLE_COSTUME
				if (bSubType == COSTUME_STOLE)
					return false;
#endif
			}

			if (item2->IsExchanging() || item2->IsEquipped()) // @fixme114
				return false;

			if (item2->GetAttributeSetIndex() == -1)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
				return false;
			}

			if (item2->GetAttributeCount() == 0)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 354, "");
#endif
				return false;
			}

			switch (item->GetSubType())
			{
			case USE_CHANGE_COSTUME_ATTR:
				item2->ChangeAttribute();
				{
					char buf[21];
					snprintf(buf, sizeof(buf), "%u", item2->GetID());
					LogManager::instance().ItemLog(this, item, "CHANGE_COSTUME_ATTR", buf);
				}
				break;
			case USE_RESET_COSTUME_ATTR:
				item2->ClearAttribute();
				item2->AlterToMagicItem();
				{
					char buf[21];
					snprintf(buf, sizeof(buf), "%u", item2->GetID());
					LogManager::instance().ItemLog(this, item, "RESET_COSTUME_ATTR", buf);
				}
				break;
			}

#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 392, "");
#endif
			item->SetCount(item->GetCount() - 1);
			break;
		}

		//  ACCESSORY_REFINE & ADD/CHANGE_ATTRIBUTES
		case USE_PUT_INTO_BELT_SOCKET:
		case USE_PUT_INTO_RING_SOCKET:
		case USE_PUT_INTO_ACCESSORY_SOCKET:
		case USE_ADD_ACCESSORY_SOCKET:
		case USE_CLEAN_SOCKET:
		case USE_CHANGE_ATTRIBUTE:
		case USE_CHANGE_ATTRIBUTE2:
		case USE_ADD_ATTRIBUTE:
		case USE_ADD_ATTRIBUTE2:
		{
			LPITEM item2;
			if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
				return false;

			if (item2->IsEquipped())
			{
				BuffOnAttr_RemoveBuffsFromItem(item2);
			}

			// [NOTE] ÄÚ½ºÆ¬ ¾ÆÀÌÅÛ¿¡´Â ¾ÆÀÌÅÛ ÃÖÃÊ »ý¼º½Ã ·£´ý ¼Ó¼ºÀ» ºÎ¿©ÇÏµÇ, Àç°æÀç°¡ µîµîÀº ¸·¾Æ´Þ¶ó´Â ¿äÃ»ÀÌ ÀÖ¾úÀ½.
			// ¿ø·¡ ANTI_CHANGE_ATTRIBUTE °°Àº ¾ÆÀÌÅÛ Flag¸¦ Ãß°¡ÇÏ¿© ±âÈ¹ ·¹º§¿¡¼­ À¯¿¬ÇÏ°Ô ÄÁÆ®·Ñ ÇÒ ¼ö ÀÖµµ·Ï ÇÒ ¿¹Á¤ÀÌ¾úÀ¸³ª
			// ±×µý°Å ÇÊ¿ä¾øÀ¸´Ï ´ÚÄ¡°í »¡¸® ÇØ´Þ·¡¼­ ±×³É ¿©±â¼­ ¸·À½... -_-
			if (ITEM_COSTUME == item2->GetType())
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
				return false;
			}

			if (item2->IsExchanging() || item2->IsEquipped()) // @fixme114
				return false;

			switch (item->GetSubType())
			{
			case USE_CLEAN_SOCKET:
			{
				int i;
				for (i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
				{
					if (item2->GetSocket(i) == ITEM_BROKEN_METIN_VNUM)
						break;
				}

				if (i == ITEM_SOCKET_MAX_NUM)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 480, "");
#endif
					return false;
				}

				int j = 0;

				for (i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
				{
					if (item2->GetSocket(i) != ITEM_BROKEN_METIN_VNUM && item2->GetSocket(i) != 0)
						item2->SetSocket(j++, item2->GetSocket(i));
				}

				for (; j < ITEM_SOCKET_MAX_NUM; ++j)
				{
					if (item2->GetSocket(j) > 0)
						item2->SetSocket(j, 1);
				}

				{
					char buf[21];
					snprintf(buf, sizeof(buf), "%u", item2->GetID());
					LogManager::instance().ItemLog(this, item, "CLEAN_SOCKET", buf);
				}

				item->SetCount(item->GetCount() - 1);

			}
			break;

			case USE_CHANGE_ATTRIBUTE:
			case USE_CHANGE_ATTRIBUTE2: // @fixme123
				if (item2->GetAttributeSetIndex() == -1)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
					return false;
				}

				if (item2->GetAttributeCount() == 0)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 354, "");
#endif
					return false;
				}

				if ((GM_PLAYER == GetGMLevel()) && (false == test_server) && (g_dwItemBonusChangeTime > 0))
				{
					//
					// Event Flag ¸¦ ÅëÇØ ÀÌÀü¿¡ ¾ÆÀÌÅÛ ¼Ó¼º º¯°æÀ» ÇÑ ½Ã°£À¸·Î ºÎÅÍ ÃæºÐÇÑ ½Ã°£ÀÌ Èê·¶´ÂÁö °Ë»çÇÏ°í
					// ½Ã°£ÀÌ ÃæºÐÈ÷ Èê·¶´Ù¸é ÇöÀç ¼Ó¼ºº¯°æ¿¡ ´ëÇÑ ½Ã°£À» ¼³Á¤ÇØ ÁØ´Ù.
					//

					// uint32_t dwChangeItemAttrCycle = quest::CQuestManager::instance().GetEventFlag(msc_szChangeItemAttrCycleFlag);
					// if (dwChangeItemAttrCycle < msc_dwDefaultChangeItemAttrCycle)
						// dwChangeItemAttrCycle = msc_dwDefaultChangeItemAttrCycle;
					uint32_t dwChangeItemAttrCycle = g_dwItemBonusChangeTime;

					quest::PC* pPC = quest::CQuestManager::instance().GetPC(GetPlayerID());

					if (pPC)
					{
						uint32_t dwNowSec = get_global_time();

						uint32_t dwLastChangeItemAttrSec = pPC->GetFlag(msc_szLastChangeItemAttrFlag);

						if (dwLastChangeItemAttrSec + dwChangeItemAttrCycle > dwNowSec)
						{
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 391, "%d#%d", dwChangeItemAttrCycle, dwChangeItemAttrCycle - (dwNowSec - dwLastChangeItemAttrSec));
#endif
							return false;
						}

						pPC->SetFlag(msc_szLastChangeItemAttrFlag, dwNowSec);
					}
				}

#ifdef ENABLE_CHANGE_ATTRIBUTE_RULES
				{
					uint32_t dwTargetVnum = item2->GetVnum();
					bool bZodiacItem = (

#ifdef DISABLE_ZODIAC_ATT

					(dwTargetVnum == 12314141)

						)
						? true : false;
#else
						((dwTargetVnum >= 19290) && (dwTargetVnum <= 19312)) ||
						((dwTargetVnum >= 19490) && (dwTargetVnum <= 19512)) ||
						((dwTargetVnum >= 19690) && (dwTargetVnum <= 19712)) ||
						((dwTargetVnum >= 19890) && (dwTargetVnum <= 19912)) ||
						((dwTargetVnum >= 300) && (dwTargetVnum <= 319)) ||
						(dwTargetVnum == 329) ||
						(dwTargetVnum == 339) ||
						(dwTargetVnum == 349) ||
						(dwTargetVnum == 359) ||
						(dwTargetVnum == 369) ||
						(dwTargetVnum == 379) ||
						(dwTargetVnum == 389) ||
						(dwTargetVnum == 399) ||
						((dwTargetVnum >= 1180) && (dwTargetVnum <= 1189)) ||
						(dwTargetVnum == 1199) ||
						(dwTargetVnum == 1209) ||
						(dwTargetVnum == 1219) ||
						(dwTargetVnum == 1229) ||
						((dwTargetVnum >= 2200) && (dwTargetVnum <= 2209)) ||
						(dwTargetVnum == 2219) ||
						(dwTargetVnum == 2229) ||
						(dwTargetVnum == 2239) ||
						(dwTargetVnum == 2249) ||
						((dwTargetVnum >= 3220) && (dwTargetVnum <= 3229)) ||
						(dwTargetVnum == 3239) ||
						(dwTargetVnum == 3249) ||
						(dwTargetVnum == 3259) ||
						(dwTargetVnum == 3269) ||
						((dwTargetVnum >= 5160) && (dwTargetVnum <= 5169)) ||
						(dwTargetVnum == 5179) ||
						(dwTargetVnum == 5189) ||
						(dwTargetVnum == 5199) ||
						(dwTargetVnum == 5209) ||
						((dwTargetVnum >= 7300) && (dwTargetVnum <= 7309)) ||
						(dwTargetVnum == 7319) ||
						(dwTargetVnum == 7329) ||
						(dwTargetVnum == 7339) ||
						(dwTargetVnum == 7349) ||
						((dwTargetVnum >= 8500) && (dwTargetVnum <= 8569)) ||
						((dwTargetVnum >= 8640) && (dwTargetVnum <= 8739))

						)
						? true : false;




					if (item->GetVnum() != 86060) {
						if (bZodiacItem) {
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 10, "%s", item2->GetName());
#endif
							return false;
						}
					}
					else if (!bZodiacItem) {
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 9, "%s", item2->GetName());
#endif
						return false;
					}
#endif
				}

#endif

#ifdef ENABLE_TALISMAN_ATTR
				if (item->GetVnum() == 86051 || item->GetVnum() == 88965)
				{
					if (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_PENDANT)
					{
						item2->ChangeAttribute();
						item->SetCount(item->GetCount() - 1);
#ifdef ENABLE_RANKING
						SetRankPoints(13, GetRankPoints(13) + 1);
#endif
						return true;
					}
					else
					{
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 681, "");
#endif
						return false;
					}
				}
				else if (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_PENDANT)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 850, "");
#endif
					return false;
				}
#endif

				if (item->GetSubType() == USE_CHANGE_ATTRIBUTE2)
				{
					int aiChangeProb[ITEM_ATTRIBUTE_MAX_LEVEL] =
					{
						0, 0, 30, 40, 3
					};

					item2->ChangeAttribute(aiChangeProb);
				}
				else if (item->GetVnum() == 76014)
				{
					int aiChangeProb[ITEM_ATTRIBUTE_MAX_LEVEL] =
					{
						0, 10, 50, 39, 1
					};

					item2->ChangeAttribute(aiChangeProb);
				}
				else
				{
					// ¿¬Àç°æ Æ¯¼öÃ³¸®
					// Àý´ë·Î ¿¬Àç°¡ Ãß°¡ ¾ÈµÉ°Å¶ó ÇÏ¿© ÇÏµå ÄÚµùÇÔ.

					if (item->GetVnum() == 71151 || item->GetVnum() == 76023)
					{
						if ((item2->GetType() == ITEM_WEAPON)
							|| (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_BODY)
#ifdef __USE_ADD_WITH_ALL_ITEMS__
							|| (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_HEAD)
							|| (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_SHIELD)
							|| (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_WRIST)
							|| (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_FOOTS)
							|| (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_NECK)
							|| (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_EAR)
#endif
							)
						{
							bool bCanUse = true;
							for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
							{
#ifdef __ENABLE_GREEN_ITEM_LVL_30__
								if (item2->GetLimitType(i) == LIMIT_LEVEL && item2->GetLimitValue(i) > 30)
#else
								if (item2->GetLimitType(i) == LIMIT_LEVEL && item2->GetLimitValue(i) > 40)
#endif
								{
									bCanUse = false;
									break;
								}
							}
							if (false == bCanUse)
							{
#ifdef TEXTS_IMPROVEMENT
#ifdef __ENABLE_GREEN_ITEM_LVL_30__
								int iLimit = 30;
#else
								int iLimit = 40;
#endif
								ChatPacketNew(CHAT_TYPE_INFO, 682, "%d", iLimit);
#endif
								break;
							}
						}
						else
						{
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 683, "");
#endif
							break;
						}
					}


#ifdef ENABLE_BATTLE_PASS
					uint8_t bBattlePassId = GetBattlePassId();
					if (bBattlePassId)
					{
						uint32_t dwItemVnum, dwUseCount;
						if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, USE_ITEM, &dwItemVnum, &dwUseCount))
						{
							if (dwItemVnum == item->GetVnum() && GetMissionProgress(USE_ITEM, bBattlePassId) < dwUseCount)
								UpdateMissionProgress(USE_ITEM, bBattlePassId, 1, dwUseCount);
						}

						if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, USE_ITEM1, &dwItemVnum, &dwUseCount))
						{
							if (dwItemVnum == item->GetVnum() && GetMissionProgress(USE_ITEM1, bBattlePassId) < dwUseCount)
								UpdateMissionProgress(USE_ITEM1, bBattlePassId, 1, dwUseCount);
						}

						if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, USE_ITEM2, &dwItemVnum, &dwUseCount))
						{
							if (dwItemVnum == item->GetVnum() && GetMissionProgress(USE_ITEM2, bBattlePassId) < dwUseCount)
								UpdateMissionProgress(USE_ITEM2, bBattlePassId, 1, dwUseCount);
						}
					}
#endif
					item2->ChangeAttribute();
				}

#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 392, "");
#endif

				{
					char buf[21];
					snprintf(buf, sizeof(buf), "%u", item2->GetID());
					LogManager::instance().ItemLog(this, item, "CHANGE_ATTRIBUTE", buf);
				}

				item->SetCount(item->GetCount() - 1);
#ifdef ENABLE_RANKING
				if (item->GetVnum() == 86051 || item->GetVnum() == 88965)
					SetRankPoints(13, GetRankPoints(13) + 1);
				else
					SetRankPoints(12, GetRankPoints(12) + 1);
#endif
				break;

			case USE_ADD_ATTRIBUTE:
				if (item2->GetAttributeSetIndex() == -1)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
					return false;
				}

				if (item2->GetAttributeCount() < 5)
				{
#ifdef ENABLE_TALISMAN_ATTR
					if (item->GetVnum() == 86050 || item->GetVnum() == 88966) {
						if (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_PENDANT)
						{
#if defined(ENABLE_BUG_FIXES)
							if (item2->GetAttributeCount() == 4)
							{
#if defined(TEXTS_IMPROVEMENT)
								ChatPacketNew(CHAT_TYPE_INFO, 1359, "");
#endif
								return false;
							}
#endif

							item2->AddAttribute();
							item->SetCount(item->GetCount() - 1);
							return true;
						}
						else
						{
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 681, "");
#endif
							return false;
						}
					}
					else if (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_PENDANT)
					{
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 684, "");
#endif
						return false;
					}
#endif

					// ¿¬Àç°¡ Æ¯¼öÃ³¸®
					// Àý´ë·Î ¿¬Àç°¡ Ãß°¡ ¾ÈµÉ°Å¶ó ÇÏ¿© ÇÏµå ÄÚµùÇÔ.
					if (item->GetVnum() == 71152 || item->GetVnum() == 76024)
					{
						if ((item2->GetType() == ITEM_WEAPON)
							|| (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_BODY)
#ifdef __USE_ADD_WITH_ALL_ITEMS__
							|| (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_HEAD)
							|| (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_SHIELD)
							|| (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_WRIST)
							|| (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_FOOTS)
							|| (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_NECK)
							|| (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_EAR)
#endif
							)
						{
							bool bCanUse = true;
							for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
							{
#ifdef __ENABLE_GREEN_ITEM_LVL_30__
								if (item2->GetLimitType(i) == LIMIT_LEVEL && item2->GetLimitValue(i) > 30)
#else
								if (item2->GetLimitType(i) == LIMIT_LEVEL && item2->GetLimitValue(i) > 40)
#endif
								{
									bCanUse = false;
									break;
								}
							}
							if (false == bCanUse)
							{
#ifdef TEXTS_IMPROVEMENT
#ifdef __ENABLE_GREEN_ITEM_LVL_30__
								int iLimit = 30;
#else
								int iLimit = 40;
#endif
								ChatPacketNew(CHAT_TYPE_INFO, 682, "%d", iLimit);
#endif
								break;
							}
						}
						else
						{
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 683, "");
#endif
							break;
						}
					}
					char buf[21];
					snprintf(buf, sizeof(buf), "%u", item2->GetID());
#ifndef ENABLE_ENCHANT_CHANGES
					if (number(1, 100) <= aiItemAttributeAddPercent[item2->GetAttributeCount()])
#endif
					{
#ifdef ENABLE_MAX_ADD_ATTRIBUTE
						short AttributeCount = abs(1 - item->GetAttributeCount());//1 bonuszt ad hozz?a z?d er?
						for (int i = 0; i < AttributeCount; i++)
							item2->AddAttribute();
						item->SetCount(item->GetCount() - 1);// elvesz 1 db ot
#else
						item2->AddAttribute();
#endif
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 389, "");
#endif
						int iAddedIdx = item2->GetAttributeCount() - 1;
						LogManager::instance().ItemLog(
							GetPlayerID(),
							item2->GetAttributeType(iAddedIdx),
							item2->GetAttributeValue(iAddedIdx),
							item->GetID(),
							"ADD_ATTRIBUTE_SUCCESS",
							buf,
							GetDesc()->GetHostName(),
							item->GetOriginalVnum());
					}
#ifndef ENABLE_ENCHANT_CHANGES
					else
					{
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 390, "");
#endif
						LogManager::instance().ItemLog(this, item, "ADD_ATTRIBUTE_FAIL", buf);
					}

					item->SetCount(item->GetCount() - 1);
#endif
				}
#ifdef TEXTS_IMPROVEMENT
				else {
					ChatPacketNew(CHAT_TYPE_INFO, 308, "");
				}
#endif
				break;

			case USE_ADD_ATTRIBUTE2:
				// Ãàº¹ÀÇ ±¸½½
				// Àç°¡ºñ¼­¸¦ ÅëÇØ ¼Ó¼ºÀ» 4°³ Ãß°¡ ½ÃÅ² ¾ÆÀÌÅÛ¿¡ ´ëÇØ¼­ ÇÏ³ªÀÇ ¼Ó¼ºÀ» ´õ ºÙ¿©ÁØ´Ù.
				if (item2->GetAttributeSetIndex() == -1)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
					return false;
				}

				// ¼Ó¼ºÀÌ ÀÌ¹Ì 4°³ Ãß°¡ µÇ¾úÀ» ¶§¸¸ ¼Ó¼ºÀ» Ãß°¡ °¡´ÉÇÏ´Ù.
				if (item2->GetAttributeCount() == 4)
				{
#ifdef ENABLE_TALISMAN_ATTR
					if (item->GetVnum() == 86052 || item->GetVnum() == 88964)
					{
						if (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_PENDANT)
						{
							if (number(1, 100) <= 75) // % Successo di inserimeno Sfera Benedetta 75%
							{
								item2->AddAttribute();
								item->SetCount(item->GetCount() - 1);
								return true;
							}
							else
							{
								item->SetCount(item->GetCount() - 1);
#ifdef TEXTS_IMPROVEMENT
								ChatPacketNew(CHAT_TYPE_INFO, 390, "");
#endif
								return false;
							}
						}
						else
						{
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 681, "");
#endif
							return false;
						}
					}
					else if (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_PENDANT)
					{
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 684, "");
#endif
						return false;
					}
#endif


					char buf[21];
					snprintf(buf, sizeof(buf), "%u", item2->GetID());

					if (number(1, 100) <= aiItemAttributeAddPercent[item2->GetAttributeCount()])
					{
#ifdef ENABLE_MAX_ADD_ATTRIBUTE
						short AttributeCount = abs(1 - item->GetAttributeCount());
						for (int i = 0; i < AttributeCount; i++)
							item2->AddAttribute();
#else
						item2->AddAttribute();
#endif
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 389, "");
#endif
						int iAddedIdx = item2->GetAttributeCount() - 1;
						LogManager::instance().ItemLog(
							GetPlayerID(),
							item2->GetAttributeType(iAddedIdx),
							item2->GetAttributeValue(iAddedIdx),
							item->GetID(),
							"ADD_ATTRIBUTE2_SUCCESS",
							buf,
							GetDesc()->GetHostName(),
							item->GetOriginalVnum());
					}
					else
					{
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 390, "");
#endif
						LogManager::instance().ItemLog(this, item, "ADD_ATTRIBUTE2_FAIL", buf);
					}

					item->SetCount(item->GetCount() - 1);
				}
				else if (item2->GetAttributeCount() == 5)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 308, "");
#endif
				}
				else if (item2->GetAttributeCount() < 4)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 339, "%d#%d#%d", 4, item2->GetAttributeCount(), 4);
#endif
				}
				else
				{
					// wtf ?!
					sys_err("ADD_ATTRIBUTE2 : Item has wrong AttributeCount(%d)", item2->GetAttributeCount());
				}
				break;

			case USE_ADD_ACCESSORY_SOCKET:
			{
				char buf[21];
				snprintf(buf, sizeof(buf), "%u", item2->GetID());
				if (item2->GetType() == ITEM_BELT)
				{
					ChatPacket(CHAT_TYPE_INFO, "You can't add new slot's to belt items");
					return false;
					
				}
				if (item2->IsAccessoryForSocket())
				{
					if (item2->GetAccessorySocketMaxGrade() < ITEM_ACCESSORY_SOCKET_MAX_NUM)
					{
#ifdef ENABLE_ADDSTONE_FAILURE
						if (number(1, 100) <= 50)
#else
						if (1)
#endif
						{
							item2->SetAccessorySocketMaxGrade(item2->GetAccessorySocketMaxGrade() + 1);
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 387, "");
#endif
							LogManager::instance().ItemLog(this, item, "ADD_SOCKET_SUCCESS", buf);
						}
						else
						{
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 386, "");
#endif
							LogManager::instance().ItemLog(this, item, "ADD_SOCKET_FAIL", buf);
						}

						item->SetCount(item->GetCount() - 1);
					}
#ifdef TEXTS_IMPROVEMENT
					else {
						ChatPacketNew(CHAT_TYPE_INFO, 428, "");
					}
#endif
				}
				else
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 425, "");
#endif
				}
			}
			break;

			case USE_PUT_INTO_BELT_SOCKET:
			case USE_PUT_INTO_ACCESSORY_SOCKET:
				if (item2->IsAccessoryForSocket())
				{
					if (item->CanPutInto(item2)) {
#ifdef ENABLE_INFINITE_RAFINES
						if (item2->GetSocket(0) > 86400 || item2->GetSocket(1) > 86400 || item2->GetSocket(2) > 86400) {
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 859, "");
#endif
							return false;
						}
#endif
						char buf[21];
						snprintf(buf, sizeof(buf), "%u", item2->GetID());

						if (item2->GetAccessorySocketGrade() < item2->GetAccessorySocketMaxGrade())
						{
							//if (number(1, 100) <= aiAccessorySocketPutPct[item2->GetAccessorySocketGrade()])
							//{
							item2->SetAccessorySocketGrade(item2->GetAccessorySocketGrade() + 1);
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 452, "");
#endif
							LogManager::instance().ItemLog(this, item, "PUT_SOCKET_SUCCESS", buf);
							//}
							//else
							//{
//#ifdef TEXTS_IMPROVEMENT
													//ChatPacketNew(CHAT_TYPE_INFO, 453, "");
//#endif
													//LogManager::instance().ItemLog(this, item, "PUT_SOCKET_FAIL", buf);
												//}

							item->SetCount(item->GetCount() - 1);
						}
						else
						{
#ifdef TEXTS_IMPROVEMENT
							if (item2->GetAccessorySocketMaxGrade() == 0 || item2->GetAccessorySocketMaxGrade() < ITEM_ACCESSORY_SOCKET_MAX_NUM) {
								ChatPacketNew(CHAT_TYPE_INFO, 297, "");
								ChatPacketNew(CHAT_TYPE_INFO, 298, "");
							}
							else {
								ChatPacketNew(CHAT_TYPE_INFO, 337, "");
							}
#endif
						}
					}
#ifdef ENABLE_INFINITE_RAFINES
					else if (item->CanPutInto2(item2)) {
						if ((item2->GetSocket(0) > 5 && item2->GetSocket(0) <= 86400) || (item2->GetSocket(1) > 5 && item2->GetSocket(1) <= 86400) || (item2->GetSocket(2) > 5 && item2->GetSocket(2) <= 86400)) {
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 860, "");
#endif
							return false;
						}

						char buf[21];
						snprintf(buf, sizeof(buf), "%u", item2->GetID());

						if (item2->GetAccessorySocketGrade() < item2->GetAccessorySocketMaxGrade())
						{
							bool infinite = item->GetValue(0) == 1 ? true : false;
							if (infinite == true)
							{
								item2->SetAccessorySocketGrade(item2->GetAccessorySocketGrade() + 1, infinite);
#ifdef TEXTS_IMPROVEMENT
								ChatPacketNew(CHAT_TYPE_INFO, 452, "");
#endif
								LogManager::instance().ItemLog(this, item, "PUT_SOCKET_SUCCESS", buf);
							}
							else
							{
#ifdef TEXTS_IMPROVEMENT
								ChatPacketNew(CHAT_TYPE_INFO, 453, "");
#endif
								LogManager::instance().ItemLog(this, item, "PUT_SOCKET_FAIL", buf);
							}

							item->SetCount(item->GetCount() - 1);
						}
						else
						{
#ifdef TEXTS_IMPROVEMENT
							if (item2->GetAccessorySocketMaxGrade() == 0 || item2->GetAccessorySocketMaxGrade() < ITEM_ACCESSORY_SOCKET_MAX_NUM) {
								ChatPacketNew(CHAT_TYPE_INFO, 297, "");
								ChatPacketNew(CHAT_TYPE_INFO, 298, "");
							}
							else {
								ChatPacketNew(CHAT_TYPE_INFO, 337, "");
							}
#endif
						}
					}
#endif
					else {
						ChatPacketNew(CHAT_TYPE_INFO, 425, "");
					}
				}
				else {
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 425, "");
#endif
				}
				break;
			}
			if (item2->IsEquipped())
			{
				BuffOnAttr_AddBuffsFromItem(item2);
			}
		}
		break;
		//  END_OF_ACCESSORY_REFINE & END_OF_ADD_ATTRIBUTES & END_OF_CHANGE_ATTRIBUTES

		case USE_BAIT:
		{

			if (m_pkFishingEvent
#ifdef ENABLE_NEW_FISHING_SYSTEM
				|| m_pkFishingNewEvent
#endif
				)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 277, "");
#endif
				return false;
			}

			LPITEM weapon = GetWear(WEAR_WEAPON);

			if (!weapon || weapon->GetType() != ITEM_ROD)
				return false;

#ifdef TEXTS_IMPROVEMENT
			if (weapon->GetSocket(2)) {
				ChatPacketNew(CHAT_TYPE_INFO, 898, "%s", item->GetName());
			}
			else {
				ChatPacketNew(CHAT_TYPE_INFO, 282, "%s", item->GetName());
			}
#endif
			weapon->SetSocket(2, item->GetValue(0));
			item->SetCount(item->GetCount() - 1);
		}
		break;

		case USE_MOVE:
		case USE_TREASURE_BOX:
		case USE_MONEYBAG:
			break;

		case USE_AFFECT:
		{
			if (FindAffect(item->GetValue(0), aApplyInfo[item->GetValue(1)].bPointType)) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 442, "");
#endif
			}
			else
			{
				// PC_BANG_ITEM_ADD
				if (item->IsPCBangItem() == true)
				{
					// PC¹æÀÎÁö Ã¼Å©ÇØ¼­ Ã³¸®
					if (CPCBangManager::instance().IsPCBangIP(GetDesc()->GetHostName()) == false)
					{
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 426, "");
#endif
						return false;
					}
				}
				// END_PC_BANG_ITEM_ADD

				AddAffect(item->GetValue(0), aApplyInfo[item->GetValue(1)].bPointType, item->GetValue(2), 0, item->GetValue(3), 0, false);
				item->SetCount(item->GetCount() - 1);
			}
		}
		break;

		case USE_CREATE_STONE:
			AutoGiveItem(number(28000, 28013));
			item->SetCount(item->GetCount() - 1);
			break;

			// ¹°¾à Á¦Á¶ ½ºÅ³¿ë ·¹½ÃÇÇ Ã³¸®
		case USE_RECIPE:
		{
			LPITEM pSource1 = FindSpecifyItem(item->GetValue(1));
			int dwSourceCount1 = item->GetValue(2);

			LPITEM pSource2 = FindSpecifyItem(item->GetValue(3));
			int dwSourceCount2 = item->GetValue(4);

			if (dwSourceCount1 != 0)
			{
				if (pSource1 == nullptr)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 350, "");
#endif
					return false;
				}
			}

			if (dwSourceCount2 != 0)
			{
				if (pSource2 == nullptr)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 350, "");
#endif
					return false;
				}
			}

			if (pSource1 != nullptr)
			{
				if (pSource1->GetCount() < dwSourceCount1)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 454, "%s#%d#%d", pSource1->GetName(), dwSourceCount1, pSource1->GetCount());
#endif
					return false;
				}

				pSource1->SetCount(pSource1->GetCount() - dwSourceCount1);
			}

			if (pSource2 != nullptr)
			{
				if (pSource2->GetCount() < dwSourceCount2)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 454, "%s#%d#%d", pSource2->GetName(), dwSourceCount1, pSource2->GetCount());
#endif
					return false;
				}

				pSource2->SetCount(pSource2->GetCount() - dwSourceCount2);
			}

			LPITEM pBottle = FindSpecifyItem(50901);

			if (!pBottle || pBottle->GetCount() < 1)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 359, "");
#endif
				return false;
			}

			pBottle->SetCount(pBottle->GetCount() - 1);

			if (number(1, 100) > item->GetValue(5))
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 347, "");
#endif
				return false;
			}

			AutoGiveItem(item->GetValue(0));
		}
		break;
		}
	}
	break;
	case ITEM_METIN:
	{
		LPITEM item2;

		if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
			return false;

		if (item2->IsExchanging() || item2->IsEquipped()) // @fixme114
			return false;

		if (item2->GetType() == ITEM_PICK) return false;
		if (item2->GetType() == ITEM_ROD) return false;

		int i;

		for (i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
		{
			uint32_t dwVnum;

			if ((dwVnum = item2->GetSocket(i)) <= 2)
				continue;

			TItemTable* p = ITEM_MANAGER::instance().GetTable(dwVnum);

			if (!p)
				continue;
#ifdef KET_BONUSZOS_KOVEK
			const int32_t insV5 = item->GetValue(5);
			const int32_t insV4 = item->GetValue(4);

			

				const int32_t exV5 = p->alValues[5];
			const int32_t exV4 = p->alValues[4];

			// Ha barmelyi ko csopi egyezik barmelyikkel, akkor ne lehessen berakni csak ryuganak seggbe
			if ((insV5 && (insV5 == exV5 || insV5 == exV4)) ||
				(insV4 && (insV4 == exV5 || insV4 == exV4)))
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 230, "");
#endif
				return false;
			}


#else
			if (item->GetValue(5) == p->alValues[5])
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 230, "");
#endif
				return false;
			}
#endif
		}

		if (item2->GetType() == ITEM_ARMOR)
		{
			if (!IS_SET(item->GetWearFlag(), WEARABLE_BODY) || !IS_SET(item2->GetWearFlag(), WEARABLE_BODY))
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 420, "%s", item->GetName());
#endif
				return false;
			}
		}
		else if (item2->GetType() == ITEM_WEAPON)
		{
			if (!IS_SET(item->GetWearFlag(), WEARABLE_WEAPON))
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 419, "%s", item->GetName());
#endif
				return false;
			}
		}
		else
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 357, "");
#endif
			return false;
		}

		for (i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
			if (item2->GetSocket(i) >= 1 && item2->GetSocket(i) <= 2 && item2->GetSocket(i) >= item->GetValue(2))
			{
				// ¼® È®·ü
#ifdef ENABLE_ADDSTONE_FAILURE
				if (number(1, 100) <= stone_chance) // Erfolgreich
#else
				if (number(1, 100) <= stone_chance) // Erfolgreich
#endif
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 340, "");
#endif
					item2->SetSocket(i, item->GetVnum());
				}
				else
				{
					ChatPacketNew(CHAT_TYPE_INFO, 341, "");
					item2->SetSocket(i, ITEM_BROKEN_METIN_VNUM);
				}

				LogManager::instance().ItemLog(this, item2, "SOCKET", item->GetName());
#ifdef ENABLE_BUG_FIXES
				item->SetCount(item->GetCount() - 1);
#else
#ifdef ENABLE_STONE_STACKFIX
				item->SetCount(item->GetCount() - 1);
#else
				ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (METIN)");
#endif
#endif
				break;
			}

		if (i == ITEM_SOCKET_MAX_NUM)
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 357, "%s", item2->GetName());
#endif
	}
	break;

	case ITEM_AUTOUSE:
	case ITEM_MATERIAL:
	case ITEM_SPECIAL:
	case ITEM_TOOL:
	case ITEM_LOTTERY:
		break;

	case ITEM_TOTEM:
	{
		if (!item->IsEquipped())
			EquipItem(item);
	}
	break;

	case ITEM_BLEND:
		// »õ·Î¿î ¾àÃÊµé
		sys_log(0, "ITEM_BLEND!!");
		if (Blend_Item_find(item->GetVnum()))
		{
			int		affect_type = AFFECT_BLEND;
			int		apply_type = aApplyInfo[item->GetSocket(0)].bPointType;
			int		apply_value = item->GetSocket(1);
			int		apply_duration = item->GetSocket(2);

			if (FindAffect(affect_type, apply_type)) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 442, "");
#endif
			}
			else
			{
				if (FindAffect(AFFECT_EXP_BONUS_EURO_FREE, POINT_RESIST_MAGIC)) {
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 442, "");
#endif
				}
				else
				{
#ifdef ENABLE_BUG_FIXES
					if (!m_bIsLoadedAffect) {
						return false;
					}
#endif

					AddAffect(affect_type, apply_type, apply_value, 0, apply_duration, 0, false);
					item->SetCount(item->GetCount() - 1);
				}
			}
		}
		break;
	case ITEM_EXTRACT:
	{
		LPITEM pDestItem = GetItem(DestCell);
		if (nullptr == pDestItem)
		{
			return false;
		}
		switch (item->GetSubType())
		{
		case EXTRACT_DRAGON_SOUL:
			if (pDestItem->IsDragonSoul())
			{
				return DSManager::instance().PullOut(this, NPOS, pDestItem, item);
			}
			return false;
		case EXTRACT_DRAGON_HEART:
			if (pDestItem->IsDragonSoul())
			{
				return DSManager::instance().ExtractDragonHeart(this, pDestItem, item);
			}
			return false;
		default:
			return false;
		}
	}
	break;

#ifdef ENABLE_SOUL_SYSTEM
	case ITEM_SOUL:
	{
		int iCurrentMinutes = (item->GetSocket(2) / 10000);
		int iCurrentStrike = (item->GetSocket(2) % 10000);

		if (iCurrentMinutes < 60)
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 685, "");
#endif
			return false;
		}

		if (iCurrentStrike <= 0)
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 686, "");
#endif
			return false;
		}

		uint8_t bSoulType = item->GetSubType();
		if (bSoulType >= SOUL_MAX_NUM)
			return false;

		int iAffectID = AFFECT_SOUL_RED + bSoulType;
		int iAffID = AFF_SOUL_RED + bSoulType;

		bool blockUse = false;
		const CAffect* pAffect = FindAffect(iAffectID);
		if (pAffect)
		{
			uint32_t dwSPCost = pAffect->lSPCost;
			if (item->GetID() == dwSPCost)
			{
				blockUse = true;
			}

			LPITEM currentItem = FindItemByID(pAffect->lSPCost);
			if (currentItem)
			{
				currentItem->Lock(false);
				currentItem->SetSocket(1, false);
			}

			RemoveAffect(const_cast<CAffect*>(pAffect));
		}

		if (!blockUse)
		{
			item->Lock(true);
			item->SetSocket(1, true);

			AddAffect(iAffectID, APPLY_NONE, 0, iAffID, INFINITE_AFFECT_DURATION, item->GetID(), true, false);
		}
	}
	break;
#endif

	case ITEM_NONE:
		sys_err("Item type NONE %s", item->GetName());
		break;

	default:
		sys_log(0, "UseItemEx: Unknown type %s %d", item->GetName(), item->GetType());
		return false;
	}

	return true;
}

int g_nPortalLimitTime = 10;


bool CHARACTER::SwapItem(uint8_t bCell, uint8_t bDestCell)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ChatPacket(CHAT_TYPE_INFO, "char_item.cpp::bool bool CHARACTER::SwapItem ");//INGAME_DEBUG_RAZOR93
#endif
	if (!CanHandleItem())
		return false;

	TItemPos srcCell(INVENTORY, bCell), destCell(INVENTORY, bDestCell);

	// ¿Ã¹Ù¸¥ Cell ÀÎÁö °Ë»ç
	// ¿ëÈ¥¼®Àº SwapÇÒ ¼ö ¾øÀ¸¹Ç·Î, ¿©±â¼­ °É¸².
	//if (bCell >= INVENTORY_MAX_NUM + WEAR_MAX_NUM || bDestCell >= INVENTORY_MAX_NUM + WEAR_MAX_NUM)
	if (srcCell.IsDragonSoulEquipPosition() || destCell.IsDragonSoulEquipPosition())
		return false;

	// °°Àº CELL ÀÎÁö °Ë»ç
	if (bCell == bDestCell)
		return false;

	// µÑ ´Ù ÀåºñÃ¢ À§Ä¡¸é Swap ÇÒ ¼ö ¾ø´Ù.
	if (srcCell.IsEquipPosition() && destCell.IsEquipPosition())
		return false;

	LPITEM item1, item2;

	// item2°¡ ÀåºñÃ¢¿¡ ÀÖ´Â °ÍÀÌ µÇµµ·Ï.
	if (srcCell.IsEquipPosition())
	{
		item1 = GetInventoryItem(bDestCell);
		item2 = GetInventoryItem(bCell);
	}
	else
	{
		item1 = GetInventoryItem(bCell);
		item2 = GetInventoryItem(bDestCell);
	}

	if (!item1 || !item2)
		return false;

	if (item1 == item2)
	{
		sys_log(0, "[WARNING][WARNING][HACK USER!] : %s %d %d", m_stName.c_str(), bCell, bDestCell);
		return false;
	}

	// item2°¡ bCellÀ§Ä¡¿¡ µé¾î°¥ ¼ö ÀÖ´ÂÁö È®ÀÎÇÑ´Ù.
	if (!IsEmptyItemGrid(TItemPos(INVENTORY, item1->GetCell()), item2->GetSize(), item1->GetCell()))
		return false;

	// ¹Ù²Ü ¾ÆÀÌÅÛÀÌ ÀåºñÃ¢¿¡ ÀÖÀ¸¸é
	if (TItemPos(EQUIPMENT, item2->GetCell()).IsEquipPosition())
	{
		uint8_t bEquipCell = item2->GetCell() - INVENTORY_MAX_NUM;
		uint8_t bInvenCell = item1->GetCell();

		// Âø¿ëÁßÀÎ ¾ÆÀÌÅÛÀ» ¹þÀ» ¼ö ÀÖ°í, Âø¿ë ¿¹Á¤ ¾ÆÀÌÅÛÀÌ Âø¿ë °¡´ÉÇÑ »óÅÂ¿©¾ß¸¸ ÁøÇà
		if (item2->IsDragonSoul() || item2->GetType() == ITEM_BELT) // @fixme117
		{
			if (false == CanUnequipNow(item2) || false == CanEquipNow(item1))
				return false;
		}
		if (bEquipCell != item1->FindEquipCell(this, bEquipCell))
			return false;

		item2->RemoveFromCharacter();

		if (item1->EquipTo(this, bEquipCell))
		{
			item2->AddToCharacter(this, TItemPos(INVENTORY, bInvenCell)
#ifdef __HIGHLIGHT_SYSTEM__
				, false
#endif
			);
			////item2->ModifyPoints(false);
			////ComputePoints();
		}
		else {
			sys_err("SwapItem cannot equip %s! item1 %s", item2->GetName(), item1->GetName());
		}
	}
	else
	{
		uint8_t bCell1 = item1->GetCell();
		uint8_t bCell2 = item2->GetCell();

		item1->RemoveFromCharacter();
		item2->RemoveFromCharacter();

#ifdef __HIGHLIGHT_SYSTEM__
		item1->AddToCharacter(this, TItemPos(INVENTORY, bCell2), false);
		item2->AddToCharacter(this, TItemPos(INVENTORY, bCell1), false);
#else
		item1->AddToCharacter(this, TItemPos(INVENTORY, bCell2));
		item2->AddToCharacter(this, TItemPos(INVENTORY, bCell1));
#endif
	}

	return true;
}

//
// @version	05/07/05 Bang2ni - Skill »ç¿ëÈÄ 1.5 ÃÊ ÀÌ³»¿¡ Àåºñ Âø¿ë ±ÝÁö
//
void CHARACTER::BuffOnAttr_AddBuffsFromItem(LPITEM pItem)
{
	for (size_t i = 0; i < sizeof(g_aBuffOnAttrPoints) / sizeof(g_aBuffOnAttrPoints[0]); i++)
	{
		TMapBuffOnAttrs::iterator it = m_map_buff_on_attrs.find(g_aBuffOnAttrPoints[i]);
		if (it != m_map_buff_on_attrs.end())
		{
			it->second->AddBuffFromItem(pItem);
		}
	}
}

void CHARACTER::BuffOnAttr_RemoveBuffsFromItem(LPITEM pItem)
{
	for (size_t i = 0; i < sizeof(g_aBuffOnAttrPoints) / sizeof(g_aBuffOnAttrPoints[0]); i++)
	{
		TMapBuffOnAttrs::iterator it = m_map_buff_on_attrs.find(g_aBuffOnAttrPoints[i]);
		if (it != m_map_buff_on_attrs.end())
		{
			it->second->RemoveBuffFromItem(pItem);
		}
	}
}

void CHARACTER::BuffOnAttr_ClearAll()
{
	for (TMapBuffOnAttrs::iterator it = m_map_buff_on_attrs.begin(); it != m_map_buff_on_attrs.end(); it++)
	{
		CBuffOnAttributes* pBuff = it->second;
		if (pBuff)
		{
			pBuff->Initialize();
		}
	}
}

void CHARACTER::BuffOnAttr_ValueChange(uint8_t bType, uint8_t bOldValue, uint8_t bNewValue)
{
	TMapBuffOnAttrs::iterator it = m_map_buff_on_attrs.find(bType);

	if (0 == bNewValue)
	{
		if (m_map_buff_on_attrs.end() == it)
			return;
		else
			it->second->Off();
	}
	else if (0 == bOldValue)
	{
		CBuffOnAttributes* pBuff = nullptr;
		if (m_map_buff_on_attrs.end() == it)
		{
			switch (bType)
			{
			case POINT_ENERGY:
			{
				static uint8_t abSlot[] = { WEAR_BODY, WEAR_HEAD, WEAR_FOOTS, WEAR_WRIST, WEAR_WEAPON, WEAR_NECK, WEAR_EAR, WEAR_SHIELD };
				static std::vector <uint8_t> vec_slots(abSlot, abSlot + _countof(abSlot));
				pBuff = M2_NEW CBuffOnAttributes(this, bType, &vec_slots);
			}
			break;
			case POINT_COSTUME_ATTR_BONUS:
			{
				static uint8_t abSlot[] = {
					WEAR_COSTUME_BODY,
					WEAR_COSTUME_HAIR,
					WEAR_COSTUME_MOUNT,
#ifdef ENABLE_COSTUME_EFFECT_ATTR_BONUS_RAZOR93


						WEAR_COSTUME_PET_SKIN,
						WEAR_COSTUME_EFFECT_BODY,
						WEAR_COSTUME_EFFECT_WEAPON,
#endif // ENABLE_COSTUME_EFFECT_ATTR_BONUS_RAZOR93
#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
						WEAR_COSTUME_WEAPON,
#endif
#ifdef ENABLE_STOLE_COSTUME
						WEAR_COSTUME_ACCE,
#endif
						WEAR_COSTUME_ACCE_SLOT,
				};

				static std::vector <uint8_t> vec_slots(abSlot, abSlot + _countof(abSlot));
				pBuff = M2_NEW CBuffOnAttributes(this, bType, &vec_slots);
			}
			break;
			default:
				break;
			}
			m_map_buff_on_attrs.insert(TMapBuffOnAttrs::value_type(bType, pBuff));

		}
		else
			pBuff = it->second;
		if (pBuff != nullptr)
			pBuff->On(bNewValue);
	}
	else
	{
		assert(m_map_buff_on_attrs.end() != it);
		it->second->ChangeBuffValue(bNewValue);
	}
}


void CHARACTER::RemoveSpecifyTypeItem(uint8_t type, int count)
{
	if (0 == count)
		return;

#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
	for (int i = 0; i < Inventory_Size(); ++i)
#else
	for (UINT i = 0; i < INVENTORY_MAX_NUM; ++i)
#endif
	{
		LPITEM item = GetInventoryItem(i);
		if (!item)
			continue;

		if (GetInventoryItem(i)->GetType() != type)
			continue;


		if (m_pkMyShop && m_pkMyShop->IsSellingItem(item->GetID()))
			continue;

		const int itemCount = item->GetCount();
		if (count >= itemCount)
		{
			count -= itemCount;
			item->SetCount(0);

			if (0 == count)
				return;
		}
		else
		{
			item->SetCount(itemCount - count);
			return;
		}
	}
}

void CHARACTER::AutoGiveItem(LPITEM item, bool longOwnerShip
#ifdef __HIGHLIGHT_SYSTEM__
	, bool isHighLight
#endif
)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ChatPacket(CHAT_TYPE_INFO, "char_item.cpp::void CHARACTER::AutoGiveItem(LPITEM item, bool longOwnerShip,");//INGAME_DEBUG_RAZOR93
#endif
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	sys_log(0, "Razor93 LOG:: Called: void CHARACTER::AutoGiveItem(LPITEM item, bool longOwnerShip");
#endif
	if (nullptr == item)
	{
		sys_err("NULL point.");
		return;
	}
	if (item->GetOwner())
	{
		sys_err("item %d 's owner exists!", item->GetID());
		return;
	}

#ifdef ENABLE_EXTRA_INVENTORY
	if (item->IsExtraItem() && item->IsStackable() && !IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_STACK))
	{
#ifdef ENABLE_NEW_STACK_LIMIT
		int
#else
		uint8_t
#endif
			bCount = item->GetCount();
		for (int i = 0; i < EXTRA_INVENTORY_MAX_NUM; ++i)
		{
			LPITEM item2 = GetExtraInventoryItem(i);
			if (!item2)
				continue;

			if (item2->GetVnum() == item->GetVnum())
			{
				int j = 0;
				for (j = 0; j < ITEM_SOCKET_MAX_NUM; ++j)
					if (item2->GetSocket(j) != item->GetSocket(j))
						break;

				if (j != ITEM_SOCKET_MAX_NUM)
					continue;

#ifdef ENABLE_NEW_STACK_LIMIT
				int
#else
				uint8_t
#endif
					bCount2 = std::min(g_bItemCountLimit - item2->GetCount(), bCount); // change type for some
				bCount -= bCount2;
				item2->SetCount(item2->GetCount() + bCount2);
				if (bCount == 0) {
					item->SetCount(0);
					M2_DESTROY_ITEM(item);
					return;
				}
				else {
					item->SetCount(bCount);
				}
			}
		}
	}
	else if (item->IsStackable() && !IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_STACK))
#else
	if (item->IsStackable() && !IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_STACK))
#endif
	{
#ifdef ENABLE_NEW_STACK_LIMIT
		int
#else
		uint8_t
#endif
			bCount = item->GetCount();
		for (int i = 0; i < INVENTORY_MAX_NUM; ++i)
		{
			LPITEM item2 = GetInventoryItem(i);
			if (!item2)
				continue;

			if (item2->GetVnum() == item->GetVnum())
			{
				int j = 0;
				for (j = 0; j < ITEM_SOCKET_MAX_NUM; ++j)
					if (item2->GetSocket(j) != item->GetSocket(j))
						break;

				if (j != ITEM_SOCKET_MAX_NUM)
					continue;

#ifdef ENABLE_NEW_STACK_LIMIT
				int
#else
				uint8_t
#endif
					bCount2 = std::min(g_bItemCountLimit - item2->GetCount(), bCount); // change type for some
				bCount -= bCount2;
				item2->SetCount(item2->GetCount() + bCount2);
				if (bCount == 0) {
					item->SetCount(0);
					M2_DESTROY_ITEM(item);
					return;
				}
				else {
					item->SetCount(bCount);
				}
			}
		}
	}

	int cell;
	if (item->IsDragonSoul())
	{
		cell = GetEmptyDragonSoulInventory(item);
	}
#ifdef ENABLE_EXTRA_INVENTORY
	else if (item->IsExtraItem())
	{
		cell = GetEmptyExtraInventory(item);
	}
#endif
	else
	{
		cell = GetEmptyInventory(item->GetSize());
	}

	if (cell != -1)
	{
		if (item->IsDragonSoul())
			item->AddToCharacter(this, TItemPos(DRAGON_SOUL_INVENTORY, cell)
#ifdef __HIGHLIGHT_SYSTEM__
				, isHighLight
#endif
			);
#ifdef ENABLE_EXTRA_INVENTORY
		else if (item->IsExtraItem())
			item->AddToCharacter(this, TItemPos(EXTRA_INVENTORY, cell)
#ifdef __HIGHLIGHT_SYSTEM__
				, isHighLight
#endif
			);
#endif
		else
			item->AddToCharacter(this, TItemPos(INVENTORY, cell)
#ifdef __HIGHLIGHT_SYSTEM__
				, isHighLight
#endif
			);

		LogManager::instance().ItemLog(this, item, "SYSTEM", item->GetName());

		if (item->GetType() == ITEM_USE && item->GetSubType() == USE_POTION)
		{
			TQuickslot* pSlot;

			if (GetQuickslot(0, &pSlot) && pSlot->type == QUICKSLOT_TYPE_NONE)
			{
				TQuickslot slot;
				slot.type = QUICKSLOT_TYPE_ITEM;
				slot.pos = cell;
				SetQuickslot(0, slot);
			}
		}
	}
	else
	{
		item->AddToGround(GetMapIndex(), GetXYZ());
#ifdef ENABLE_NEWSTUFF
		item->StartDestroyEvent(g_aiItemDestroyTime[ITEM_DESTROY_TIME_AUTOGIVE]);
#else
		item->StartDestroyEvent();
#endif

		if (longOwnerShip)
			item->SetOwnership(this, 300);
		else
			item->SetOwnership(this, 60);
		LogManager::instance().ItemLog(this, item, "SYSTEM_DROP", item->GetName());
	}
}

#ifdef ENABLE_DS_REFINE_ALL
bool CHARACTER::AutoGiveDS(LPITEM item, bool longOwnerShip) {
	if (item == nullptr) {
		sys_err("NULL point.");
		return false;
	}

	if (item->GetOwner()) {
		sys_err("item %d 's owner exists!", item->GetID());
		return false;
	}

	if (!item->IsDragonSoul()) {
		sys_err("item %d is not alchemy!", item->GetID());
		return false;
	}

	int cell = GetEmptyDragonSoulInventory(item);
	if (cell != -1)
	{
		item->AddToCharacter(this, TItemPos(DRAGON_SOUL_INVENTORY, cell)
#ifdef __HIGHLIGHT_SYSTEM__
			, true
#endif
		);

		LogManager::instance().ItemLog(this, item, "SYSTEM", item->GetName());
	}
	else
	{
		item->AddToGround(GetMapIndex(), GetXYZ());
#ifdef ENABLE_NEWSTUFF
		item->StartDestroyEvent(g_aiItemDestroyTime[ITEM_DESTROY_TIME_AUTOGIVE]);
#else
		item->StartDestroyEvent();
#endif

		if (longOwnerShip) {
			item->SetOwnership(this, 300);
		}
		else {
			item->SetOwnership(this, 60);
		}

		LogManager::instance().ItemLog(this, item, "SYSTEM_DROP", item->GetName());
	}

	return true;
}
#endif

LPITEM CHARACTER::AutoGiveItem(uint32_t dwItemVnum,
#ifdef ENABLE_NEW_STACK_LIMIT
	int
#else
	uint8_t
#endif
	bCount, int iRarePct, bool bMsg
#ifdef __HIGHLIGHT_SYSTEM__
	, bool isHighLight
#endif
)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ChatPacket(CHAT_TYPE_INFO, "char_item.cpp::LPITEM CHARACTER::AutoGiveItem(uint32_t dwItemVnum,");//INGAME_DEBUG_RAZOR93
#endif
	TItemTable* p = ITEM_MANAGER::instance().GetTable(dwItemVnum);

	if (!p)
		return nullptr;

	DBManager::instance().SendMoneyLog(MONEY_LOG_DROP, dwItemVnum, bCount);


#ifdef ENABLE_EXTRA_INVENTORY
	if (p->dwFlags & ITEM_FLAG_STACKABLE && ITEM_MANAGER::instance().IsExtraItem(dwItemVnum))
	{
		for (int i = 0; i < EXTRA_INVENTORY_MAX_NUM; ++i)
		{
			LPITEM item = GetExtraInventoryItem(i);

			if (!item)
				continue;

			if (item->GetVnum() == dwItemVnum && FN_check_item_socket(item))
			{
				if (IS_SET(p->dwFlags, ITEM_FLAG_MAKECOUNT))
				{
					if (bCount < p->alValues[1])
						bCount = p->alValues[1];
				}

#ifdef ENABLE_NEW_STACK_LIMIT
				int
#else
				uint8_t
#endif
					bCount2 = std::min(g_bItemCountLimit - item->GetCount(), bCount); // change type for some
				bCount -= bCount2;

				item->SetCount(item->GetCount() + bCount2);

				if (bCount == 0)
				{
#ifdef TEXTS_IMPROVEMENT
					if (bMsg) {
						ChatPacketNew(
#ifdef ENABLE_NEW_CHAT
							CHAT_TYPE_INFO_ITEM
#else
							CHAT_TYPE_INFO
#endif
							, 102, "%d#%s", bCount2, item->GetName(GetDesc() ? GetDesc()->GetLanguage() : 0));
						//ChatPacket(CHAT_TYPE_INFO, "|cffffc700[Kaptál:]|r 08 |cffffff00%u jelenlegi:%u x %s|r", bCount2, item->GetCount(), item->GetName());

					}
#endif

					return item;
				}
			}
		}
	}
	else if (p->dwFlags & ITEM_FLAG_STACKABLE && p->bType != ITEM_BLEND)
#else
	if (p->dwFlags & ITEM_FLAG_STACKABLE && p->bType != ITEM_BLEND)
#endif
	{
		for (int i = 0; i < INVENTORY_MAX_NUM; ++i)
		{
			LPITEM item = GetInventoryItem(i);

			if (!item)
				continue;

#ifdef ENABLE_SORT_INVEN
			if (item->GetOriginalVnum() == dwItemVnum && FN_check_item_socket(item))
#else
			if (item->GetVnum() == dwItemVnum && FN_check_item_socket(item))
#endif
			{
				if (IS_SET(p->dwFlags, ITEM_FLAG_MAKECOUNT))
				{
					if (bCount < p->alValues[1])
						bCount = p->alValues[1];
				}

#ifdef ENABLE_NEW_STACK_LIMIT
				int
#else
				uint8_t
#endif
					bCount2 = std::min(g_bItemCountLimit - item->GetCount(), bCount);
				bCount -= bCount2;

				item->SetCount(item->GetCount() + bCount2);

				if (bCount == 0)
				{
#ifdef TEXTS_IMPROVEMENT
					if (bMsg) {
						ChatPacketNew(
#ifdef ENABLE_NEW_CHAT
							CHAT_TYPE_INFO_ITEM
#else
							CHAT_TYPE_INFO
#endif
							, 102, "%d#%s", bCount2, item->GetName(GetDesc() ? GetDesc()->GetLanguage() : 0));
						//ChatPacket(CHAT_TYPE_INFO, "|cffffc700[Kaptál:]|r 09 |cffffff00%u x %s|r", item->GetCount(), item->GetName());

					}
#endif

					return item;
				}
			}
		}
	}

	LPITEM item = ITEM_MANAGER::instance().CreateItem(dwItemVnum, bCount, 0, true);

	if (!item)
	{
		sys_err("cannot create item by vnum %u (name: %s)", dwItemVnum, GetName());
		return nullptr;
	}

	if (item->GetType() == ITEM_BLEND)
	{
		for (int i = 0; i < INVENTORY_MAX_NUM; i++)
		{
			LPITEM inv_item = GetInventoryItem(i);

			if (inv_item == nullptr) continue;

			if (inv_item->GetType() == ITEM_BLEND)
			{
				if (inv_item->GetVnum() == item->GetVnum())
				{
					if (inv_item->GetSocket(0) == item->GetSocket(0) &&
						inv_item->GetSocket(1) == item->GetSocket(1) &&
						inv_item->GetSocket(2) == item->GetSocket(2) &&
						inv_item->GetCount() < g_bItemCountLimit)
					{
						inv_item->SetCount(inv_item->GetCount() + item->GetCount());
						return inv_item;
					}
				}
			}
		}
	}

	int iEmptyCell;
	if (item->IsDragonSoul())
	{
		iEmptyCell = GetEmptyDragonSoulInventory(item);
	}
#ifdef ENABLE_EXTRA_INVENTORY
	else if (item->IsExtraItem())
		iEmptyCell = GetEmptyExtraInventory(item);
#endif
	else
		iEmptyCell = GetEmptyInventory(item->GetSize());

	if (iEmptyCell != -1)
	{
#ifdef TEXTS_IMPROVEMENT
		if (bMsg) {
			ChatPacketNew(
#ifdef ENABLE_NEW_CHAT
				CHAT_TYPE_INFO_ITEM
#else
				CHAT_TYPE_INFO
#endif
				, 102, "%d#%s", bCount, item->GetName(GetDesc() ? GetDesc()->GetLanguage() : 0));
			//ChatPacket(CHAT_TYPE_INFO, "|cffffc700[10:]|r 10 ");

		}
#endif

		if (item->IsDragonSoul())
			item->AddToCharacter(this, TItemPos(DRAGON_SOUL_INVENTORY, iEmptyCell)
#ifdef __HIGHLIGHT_SYSTEM__
				, isHighLight
#endif
			);
#ifdef ENABLE_EXTRA_INVENTORY
		else if (item->IsExtraItem())
			item->AddToCharacter(this, TItemPos(EXTRA_INVENTORY, iEmptyCell)

#ifdef __HIGHLIGHT_SYSTEM__
				, isHighLight
#endif
			);
#endif
		else
			item->AddToCharacter(this, TItemPos(INVENTORY, iEmptyCell)
#ifdef __HIGHLIGHT_SYSTEM__
				, isHighLight
#endif
			);
		LogManager::instance().ItemLog(this, item, "SYSTEM", item->GetName());

		if (item->GetType() == ITEM_USE && item->GetSubType() == USE_POTION)
		{
			TQuickslot* pSlot;

			if (GetQuickslot(0, &pSlot) && pSlot->type == QUICKSLOT_TYPE_NONE)
			{
				TQuickslot slot;
				slot.type = QUICKSLOT_TYPE_ITEM;
				slot.pos = iEmptyCell;
				SetQuickslot(0, slot);
			}
		}
	}
	else
	{
		item->AddToGround(GetMapIndex(), GetXYZ());
#ifdef ENABLE_NEWSTUFF
		item->StartDestroyEvent(g_aiItemDestroyTime[ITEM_DESTROY_TIME_AUTOGIVE]);
#else
		item->StartDestroyEvent();
#endif
		// ¾ÈÆ¼ µå¶ø flag°¡ °É·ÁÀÖ´Â ¾ÆÀÌÅÛÀÇ °æ¿ì,
		// ÀÎº¥¿¡ ºó °ø°£ÀÌ ¾ø¾î¼­ ¾îÂ¿ ¼ö ¾øÀÌ ¶³¾îÆ®¸®°Ô µÇ¸é,
		// ownershipÀ» ¾ÆÀÌÅÛÀÌ »ç¶óÁú ¶§±îÁö(300ÃÊ) À¯ÁöÇÑ´Ù.
		if (IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_DROP))
			item->SetOwnership(this, 300);
		else
			item->SetOwnership(this, 60);
		LogManager::instance().ItemLog(this, item, "SYSTEM_DROP", item->GetName());
	}

	sys_log(0,
		"7: %d %d", dwItemVnum, bCount);
	return item;
}

bool CHARACTER::GiveItem(LPCHARACTER victim, TItemPos Cell)
{
	if (!CanHandleItem())
		return false;

	// @fixme150 BEGIN
	if (quest::CQuestManager::instance().GetPCForce(GetPlayerID())->IsRunning() == true)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 740, "");
#endif
		return false;
	}
	// @fixme150 END

	LPITEM item = GetItem(Cell);

	if (item && !item->IsExchanging())
	{
		if (victim->CanReceiveItem(this, item))
		{
			victim->ReceiveItem(this, item);
			return true;
		}
	}

	return false;
}

bool CHARACTER::CanReceiveItem(LPCHARACTER from, LPITEM item) const
{
	if (IsPC())
		return false;

	// TOO_LONG_DISTANCE_EXCHANGE_BUG_FIX
	if (DISTANCE_APPROX(GetX() - from->GetX(), GetY() - from->GetY()) > 2000)
		return false;
	// END_OF_TOO_LONG_DISTANCE_EXCHANGE_BUG_FIX

	uint32_t racenum = GetRaceNum();

	if (racenum == DEVILTOWER_BLACKSMITH_WEAPON_MOB ||
		racenum == DEVILTOWER_BLACKSMITH_ARMOR_MOB ||
		racenum == DEVILTOWER_BLACKSMITH_ACCESSORY_MOB) {
		bool bCanProced = true;

		for (uint8_t i = 0; i < ITEM_LIMIT_MAX_NUM; ++i) {
			if (item->GetLimitType(i) == LIMIT_LEVEL && item->GetLimitValue(i) >= 90) {
				bCanProced = false;
				break;
			}
		}

		if (!bCanProced) {
#ifdef TEXTS_IMPROVEMENT
			from->ChatPacketNew(CHAT_TYPE_INFO, 1360, "");
#endif
			return false;
		}
	}

	switch (racenum)
	{
	case fishing::CAMPFIRE_MOB:
		if (item->GetType() == ITEM_FISH &&
			(item->GetSubType() == FISH_ALIVE || item->GetSubType() == FISH_DEAD))
			return true;
		break;

	case fishing::FISHER_MOB:
		if (item->GetType() == ITEM_ROD)
			return true;
		break;

	case BLACKSMITH_WEAPON_MOB:
	case DEVILTOWER_BLACKSMITH_WEAPON_MOB:
		if (item->GetType() == ITEM_WEAPON && item->GetRefinedVnum()) {
			return true;
		}
		else {
			return false;
		}
		break;
	case BLACKSMITH_ARMOR_MOB:
	case DEVILTOWER_BLACKSMITH_ARMOR_MOB:
		if ((item->GetType() == ITEM_BELT || (item->GetType() == ITEM_ARMOR && (item->GetSubType() == ARMOR_BODY || item->GetSubType() == ARMOR_SHIELD || item->GetSubType() == ARMOR_HEAD))) && item->GetRefinedVnum()) {
			return true;
		}
		else {
			return false;
		}
		break;
	case BLACKSMITH_ACCESSORY_MOB:
	case DEVILTOWER_BLACKSMITH_ACCESSORY_MOB:
		if (item->GetType() == ITEM_ARMOR && !(item->GetSubType() == ARMOR_BODY || item->GetSubType() == ARMOR_SHIELD || item->GetSubType() == ARMOR_HEAD
#ifdef ENABLE_PENDANT
			|| item->GetSubType() == ARMOR_PENDANT
#endif
			) && item->GetRefinedVnum()) {
			return true;
		}
		else {
			return false;
		}
		break;
	case BLACKSMITH_MOB:
	case BLACKSMITH2_MOB:
		if (item->GetRefinedVnum() && item->GetRefineSet()) {
			return true;
		}
		else {
			return false;
		}
	case ALCHEMIST_MOB:
		if (item->GetRefinedVnum())
			return true;
		break;

	case 20101:
	case 20102:
	case 20103:
		// ÃÊ±Þ ¸»
		if (item->GetVnum() == ITEM_REVIVE_HORSE_1)
		{
			if (!IsDead())
			{
#ifdef TEXTS_IMPROVEMENT
				from->ChatPacketNew(CHAT_TYPE_INFO, 467, "");
#endif
				return false;
			}
			return true;
		}
		else if (item->GetVnum() == ITEM_HORSE_FOOD_1)
		{
			if (IsDead())
			{
#ifdef TEXTS_IMPROVEMENT
				from->ChatPacketNew(CHAT_TYPE_INFO, 466, "");
#endif
				return false;
			}
			return true;
		}
		else if (item->GetVnum() == ITEM_HORSE_FOOD_2 || item->GetVnum() == ITEM_HORSE_FOOD_3)
		{
			return false;
		}
		break;
	case 20104:
	case 20105:
	case 20106:
		// Áß±Þ ¸»
		if (item->GetVnum() == ITEM_REVIVE_HORSE_2)
		{
			if (!IsDead())
			{
#ifdef TEXTS_IMPROVEMENT
				from->ChatPacketNew(CHAT_TYPE_INFO, 467, "");
#endif
				return false;
			}
			return true;
		}
		else if (item->GetVnum() == ITEM_HORSE_FOOD_2)
		{
			if (IsDead())
			{
#ifdef TEXTS_IMPROVEMENT
				from->ChatPacketNew(CHAT_TYPE_INFO, 466, "");
#endif
				return false;
			}
			return true;
		}
		else if (item->GetVnum() == ITEM_HORSE_FOOD_1 || item->GetVnum() == ITEM_HORSE_FOOD_3)
		{
			return false;
		}
		break;
	case 20107:
	case 20108:
	case 20109:
		// °í±Þ ¸»
		if (item->GetVnum() == ITEM_REVIVE_HORSE_3)
		{
			if (!IsDead())
			{
#ifdef TEXTS_IMPROVEMENT
				from->ChatPacketNew(CHAT_TYPE_INFO, 467, "");
#endif
				return false;
			}
			return true;
		}
		else if (item->GetVnum() == ITEM_HORSE_FOOD_3)
		{
			if (IsDead())
			{
#ifdef TEXTS_IMPROVEMENT
				from->ChatPacketNew(CHAT_TYPE_INFO, 466, "");
#endif
				return false;
			}
			return true;
		}
		else if (item->GetVnum() == ITEM_HORSE_FOOD_1 || item->GetVnum() == ITEM_HORSE_FOOD_2)
		{
			return false;
		}
		break;
	}

	//if (IS_SET(item->GetFlag(), ITEM_FLAG_QUEST_GIVE))
	{
		return true;
	}

	return false;
}

void CHARACTER::ReceiveItem(LPCHARACTER from, LPITEM item)
{
	if (IsPC())
		return;
#ifdef ENABLE_CPP_DUNGEON_RAZOR93
	// Rune Dungeon: key pedestal (20507) consumes 89103 and progresses floor 5
	if (CRuneDungeon::instance().OnNpcTakeItem(from, this, item))
		return;
	if (CHalloween2022Dungeon::instance().OnNpcTakeItem(from, this, item))
		return;
	if (CVikingDungeon::instance().OnNpcTakeItem(from, this, item))
		return;
	// LostCastle Dungeon: statue/totem item usage
	//if (CLostCastleDungeon::instance().OnNpcTakeItem(from, this, item))
	//	return;
#endif
	switch (GetRaceNum())
	{
	case fishing::CAMPFIRE_MOB:
		if (item->GetType() == ITEM_FISH && (item->GetSubType() == FISH_ALIVE || item->GetSubType() == FISH_DEAD))
			fishing::Grill(from, item);
		else
		{
			// TAKE_ITEM_BUG_FIX
			from->SetQuestNPCID(GetVID());
			// END_OF_TAKE_ITEM_BUG_FIX
			quest::CQuestManager::instance().TakeItem(from->GetPlayerID(), GetRaceNum(), item);
		}
		break;

		// DEVILTOWER_NPC
	case DEVILTOWER_BLACKSMITH_WEAPON_MOB:
	case DEVILTOWER_BLACKSMITH_ARMOR_MOB:
	case DEVILTOWER_BLACKSMITH_ACCESSORY_MOB: {
		int set = item->GetRefineSet();
		if (item->GetRefinedVnum() != 0 && set != 0 /*&& item->GetRefineSet() < 500*/
#ifdef ENABLE_ITEM_EXTRA_PROTO
			&& set != 1021
			&& set != 1022
			&& set != 1023
			&& set != 1024
			&& set != 19
			&& set != 20
			&& set != 21
			&& set != 22
			&& set != 28
			&& set != 29
			&& set != 30
			&& set != 31
			&& set != 32
			&& set != 396
			&& set != 397
			&& set != 398
			&& set != 399
			&& set != 640
			&& set != 641
			&& set != 642
			&& set != 643
			&& set != 370
			&& set != 371
			&& set != 372
			&& set != 373
			&& set != 461
			&& set != 462
			&& set != 463
			&& set != 464
			&& set != 474
			&& set != 475
			&& set != 476
			&& set != 477
			&& set != 487
			&& set != 488
			&& set != 489
			&& set != 490
			&& set != 235
			&& set != 236
			&& set != 237
			&& set != 238
			&& set != 383
			&& set != 384
			&& set != 385
			&& set != 386
			&& set != 769
			&& set != 770
			&& set != 771
			&& set != 772
			&& set != 995
			&& set != 996
			&& set != 997
			&& set != 998
			&& set != 1017
			&& set != 1018
			&& set != 1019
			&& set != 1020
			&& set != 448
			&& set != 449
			&& set != 450
			&& set != 451
			&& set != 430
			&& set != 431
			&& set != 432
			&& set != 433
			&& set != 325
			&& set != 326
			&& set != 327
			&& set != 328
#endif
			)
		{
			from->SetRefineNPC(this);
			from->RefineInformation(item->GetCell(), REFINE_TYPE_MONEY_ONLY);
		}
#ifdef TEXTS_IMPROVEMENT
		else {
			from->ChatPacketNew(CHAT_TYPE_INFO, 427, "");
		}
#endif
		break;
	}
											// END_OF_DEVILTOWER_NPC

	case BLACKSMITH_MOB:
	case BLACKSMITH2_MOB:
	case BLACKSMITH_WEAPON_MOB:
	case BLACKSMITH_ARMOR_MOB:
	case BLACKSMITH_ACCESSORY_MOB:
		if (item->GetRefinedVnum())
		{
			from->SetRefineNPC(this);
			from->RefineInformation(item->GetCell(), REFINE_TYPE_NORMAL);
		}
#ifdef TEXTS_IMPROVEMENT
		else {
			from->ChatPacketNew(CHAT_TYPE_INFO, 427, "");
		}
#endif
		break;
	case 20101:
	case 20102:
	case 20103:
	case 20104:
	case 20105:
	case 20106:
	case 20107:
	case 20108:
	case 20109:
		if (item->GetVnum() == ITEM_REVIVE_HORSE_1 ||
			item->GetVnum() == ITEM_REVIVE_HORSE_2 ||
			item->GetVnum() == ITEM_REVIVE_HORSE_3)
		{
			from->ReviveHorse();
			item->SetCount(item->GetCount() - 1);
#ifdef TEXTS_IMPROVEMENT
			from->ChatPacketNew(CHAT_TYPE_INFO, 329, "%s", item->GetName());
#endif
		}
		else if (item->GetVnum() == ITEM_HORSE_FOOD_1 ||
			item->GetVnum() == ITEM_HORSE_FOOD_2 ||
			item->GetVnum() == ITEM_HORSE_FOOD_3)
		{
			from->FeedHorse();
#ifdef TEXTS_IMPROVEMENT
			from->ChatPacketNew(CHAT_TYPE_INFO, 112, "%s", item->GetName());
#endif
			item->SetCount(item->GetCount() - 1);
			EffectPacket(SE_HPUP_RED);
		}
		break;

	default:
		sys_log(0, "TakeItem %s %d %s", from->GetName(), GetRaceNum(), item->GetName());
		from->SetQuestNPCID(GetVID());
		quest::CQuestManager::instance().TakeItem(from->GetPlayerID(), GetRaceNum(), item);
		break;
	}
}

// CHECK_UNIQUE_GROUP
// END_OF_CHECK_UNIQUE_GROUP

void CHARACTER::SetRefineMode(int iAdditionalCell)
{
	m_iRefineAdditionalCell = iAdditionalCell;
	m_bUnderRefine = true;
}

void CHARACTER::ClearRefineMode()
{
	m_bUnderRefine = false;
	SetRefineNPC(nullptr);
}

bool CHARACTER::GiveItemFromSpecialItemGroup(uint32_t dwGroupNum, std::vector<uint32_t> &dwItemVnums,
	std::vector<uint32_t> &dwItemCounts, std::vector <LPITEM> &item_gets, int& count)
{
	const CSpecialItemGroup* pGroup = ITEM_MANAGER::instance().GetSpecialItemGroup(dwGroupNum);

	if (!pGroup)
	{
		sys_err("cannot find special item group %d", dwGroupNum);
		return false;
	}

	std::vector <int> idxes;
	int n = pGroup->GetMultiIndex(idxes);

	bool bSuccess;

	for (int i = 0; i < n; i++)
	{
		bSuccess = false;
		int idx = idxes[i];
		uint32_t dwVnum = pGroup->GetVnum(idx);
		uint32_t dwCount = pGroup->GetCount(idx);
		int	iRarePct = pGroup->GetRarePct(idx);
		LPITEM item_get = nullptr;
		switch (dwVnum)
		{
		case CSpecialItemGroup::GOLD:
			PointChange(POINT_GOLD, dwCount);
			LogManager::instance().CharLog(this, dwCount, "TREASURE_GOLD", "");

			bSuccess = true;
			break;
		case CSpecialItemGroup::EXP:
		{
			PointChange(POINT_EXP, dwCount);
			LogManager::instance().CharLog(this, dwCount, "TREASURE_EXP", "");

			bSuccess = true;
		}
		break;

		case CSpecialItemGroup::MOB:
		{
			sys_log(0, "CSpecialItemGroup::MOB %d", dwCount);
			int x = GetX() + number(-500, 500);
			int y = GetY() + number(-500, 500);

			LPCHARACTER ch = CHARACTER_MANAGER::instance().SpawnMob(dwCount, GetMapIndex(), x, y, 0, true, -1);
			if (ch)
				ch->SetAggressive();
			bSuccess = true;
		}
		break;
		case CSpecialItemGroup::SLOW:
		{
			sys_log(0, "CSpecialItemGroup::SLOW %d", -(int)dwCount);
			AddAffect(AFFECT_SLOW, POINT_MOV_SPEED, -(int)dwCount, AFF_SLOW, 300, 0, true);
			bSuccess = true;
		}
		break;
		case CSpecialItemGroup::DRAIN_HP:
		{
			int64_t iDropHP = GetMaxHP() * dwCount / 100;
			sys_log(0, "CSpecialItemGroup::DRAIN_HP %d", -iDropHP);
			iDropHP = std::min(iDropHP, GetHP() - 1);
			sys_log(0, "CSpecialItemGroup::DRAIN_HP %d", -iDropHP);
			PointChange(POINT_HP, -iDropHP);
			bSuccess = true;
		}
		break;
		case CSpecialItemGroup::POISON:
		{
			AttackedByPoison(nullptr);
			bSuccess = true;
		}
		break;
#ifdef ENABLE_WOLFMAN_CHARACTER
		case CSpecialItemGroup::BLEEDING:
		{
			AttackedByBleeding(NULL);
			bSuccess = true;
		}
		break;
#endif
		case CSpecialItemGroup::MOB_GROUP:
		{
			int sx = GetX() - number(300, 500);
			int sy = GetY() - number(300, 500);
			int ex = GetX() + number(300, 500);
			int ey = GetY() + number(300, 500);
			CHARACTER_MANAGER::instance().SpawnGroup(dwCount, GetMapIndex(), sx, sy, ex, ey, nullptr, true);

			bSuccess = true;
		}
		break;
		default:
		{
			item_get = AutoGiveItem(dwVnum, dwCount, iRarePct);

			if (item_get)
			{
				bSuccess = true;
			}
		}
		break;
		}

		if (bSuccess)
		{
			dwItemVnums.push_back(dwVnum);
			dwItemCounts.push_back(dwCount);
			item_gets.push_back(item_get);
			count++;

		}
		else
		{
			return false;
		}
	}
	return bSuccess;
}

// NEW_HAIR_STYLE_ADD
bool CHARACTER::ItemProcess_Hair(LPITEM item, int iDestCell)
{
	if (item->CheckItemUseLevel(GetLevel()) == false)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 405, "");
#endif
		return false;
	}

	uint32_t hair = item->GetVnum();

	switch (GetJob())
	{
	case JOB_WARRIOR:
		hair -= 72000; // 73001 - 72000 = 1001 ºÎÅÍ Çì¾î ¹øÈ£ ½ÃÀÛ
		break;

	case JOB_ASSASSIN:
		hair -= 71250;
		break;

	case JOB_SURA:
		hair -= 70500;
		break;

	case JOB_SHAMAN:
		hair -= 69750;
		break;
#ifdef ENABLE_WOLFMAN_CHARACTER
	case JOB_WOLFMAN:
		break; // NOTE: ÀÌ Çì¾îÄÚµå´Â ¾È ¾²ÀÌ¹Ç·Î ÆÐ½º. (ÇöÀç Çì¾î½Ã½ºÅÛÀº ÀÌ¹Ì ÄÚ½ºÆ¬À¸·Î ´ëÃ¼ µÈ »óÅÂÀÓ)
#endif
	default:
		return false;
		break;
	}

	if (hair == GetPart(PART_HAIR))
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 311, "");
#endif
		return true;
	}

	item->SetCount(item->GetCount() - 1);

	SetPart(PART_HAIR, hair);
	UpdatePacket();

	return true;
}
// END_NEW_HAIR_STYLE_ADD

bool CHARACTER::ItemProcess_Polymorph(LPITEM item)
{

#ifdef ENABLE_PVP_ADVANCED
	if ((GetDuel("BlockPoly")))
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 516, "");
#endif
		return false;
	}
#endif

	if (IsPolymorphed()) {
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 437, "");
#endif
		return false;
	}

	if (true == IsRiding())
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 741, "");
#endif
		return false;
	}

	uint32_t dwVnum = item->GetSocket(0);

	if (dwVnum == 0)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 450, "");
#endif
		item->SetCount(item->GetCount() - 1);
		return false;
	}

	const CMob* pMob = CMobManager::instance().Get(dwVnum);

	if (pMob == nullptr)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 451, "");
#endif
		item->SetCount(item->GetCount() - 1);
		return false;
	}

	switch (item->GetVnum())
	{
	case 70104:
	case 70105:
	case 70106:
	case 70107:
	case 71093:
	{
		// µÐ°©±¸ Ã³¸®
		sys_log(0, "USE_POLYMORPH_BALL PID(%d) vnum(%d)", GetPlayerID(), dwVnum);

		// ·¹º§ Á¦ÇÑ Ã¼Å©
		int iPolymorphLevelLimit = std::max(0, 20 - GetLevel() * 3 / 10);
		if (pMob->m_table.bLevel >= GetLevel() + iPolymorphLevelLimit)
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 275, "");
#endif
			return false;
		}

		int iDuration = GetSkillLevel(POLYMORPH_SKILL_ID) == 0 ? 5 : (5 + (5 + GetSkillLevel(POLYMORPH_SKILL_ID) / 40 * 25));
		iDuration *= 60;

		uint32_t dwBonus = 0;

		dwBonus = (2 + GetSkillLevel(POLYMORPH_SKILL_ID) / 40) * 100;

		AddAffect(AFFECT_POLYMORPH, POINT_POLYMORPH, dwVnum, AFF_POLYMORPH, iDuration, 0, true);
		AddAffect(AFFECT_POLYMORPH, POINT_ATT_BONUS, dwBonus, AFF_POLYMORPH, iDuration, 0, false);

		item->SetCount(item->GetCount() - 1);
	}
	break;

	case 50322:
	{
		// º¸·ù

		// µÐ°©¼­ Ã³¸®
		// ¼ÒÄÏ0                ¼ÒÄÏ1           ¼ÒÄÏ2
		// µÐ°©ÇÒ ¸ó½ºÅÍ ¹øÈ£   ¼ö·ÃÁ¤µµ        µÐ°©¼­ ·¹º§
		sys_log(0, "USE_POLYMORPH_BOOK: %s(%u) vnum(%u)", GetName(), GetPlayerID(), dwVnum);

		if (CPolymorphUtils::instance().PolymorphCharacter(this, item, pMob) == true)
		{
			CPolymorphUtils::instance().UpdateBookPracticeGrade(this, item);
		}
		else
		{
		}
	}
	break;

	default:
		sys_err("POLYMORPH invalid item passed PID(%d) vnum(%d)", GetPlayerID(), item->GetOriginalVnum());
		return false;
	}

	return true;
}

bool CHARACTER::CanDoCube() const
{
	if (m_bIsObserver)	return false;
	if (GetShop())		return false;
	if (GetMyShop())	return false;
	if (m_bUnderRefine)	return false;
	if (IsWarping())	return false;

	return true;
}

#ifdef ENABLE_RECALL
void CHARACTER::AutoRecallProcess()
{
	if (!IsPC())
		return;

#ifdef __PET_SYSTEM__
	{
		const CAffect* pAffect = FindAffect(AFFECT_RECALL1);
		if (pAffect) {
			LPITEM pItem = FindItemByID(pAffect->dwFlag);
			if (pItem) {
				if (pItem->GetSocket(2) == false) {
					CPetSystem* petSystem = GetPetSystem();
					if (petSystem) {
						if (petSystem->CountSummoned() < 1) {
							CPetActor* pPet = petSystem->Summon(pItem->GetValue(1), pItem, "", false);
							if (!pPet)
								RemoveAffect(const_cast<CAffect*>(pAffect));
						}
					}
					else
						RemoveAffect(const_cast<CAffect*>(pAffect));
				}
			}
			else
				RemoveAffect(const_cast<CAffect*>(pAffect));
		}
	}
#endif
#ifdef __NEWPET_SYSTEM__
	{
		const CAffect* pAffect = FindAffect(AFFECT_RECALL2);
		if (pAffect) {
			LPITEM pItem = FindItemByID(pAffect->dwFlag);
			if (pItem) {
				if (pItem->GetSocket(0) == false) {
					CNewPetSystem* petSystem = GetNewPetSystem();
					if (petSystem) {
						if (petSystem->CountSummoned() < 1) {
							CNewPetActor* pPet = petSystem->Summon(pItem->GetValue(0), pItem, "", false);
							if (!pPet)
								RemoveAffect(const_cast<CAffect*>(pAffect));
						}
					}
					else
						RemoveAffect(const_cast<CAffect*>(pAffect));
				}
			}
			else
				RemoveAffect(const_cast<CAffect*>(pAffect));
		}
	}
#endif
}
#endif

void CHARACTER::AutoRecoveryItemProcess(const EAffectTypes type)
{
	if (true == IsDead() || true == IsStun())
		return;

	if (false == IsPC())
		return;

#ifdef ENABLE_PVP_ADVANCED	
	if (
#ifdef ENABLE_NEW_USE_POTION
	((type == AFFECT_AUTO_HP_RECOVERY2) ||
#endif
		(type == AFFECT_AUTO_HP_RECOVERY)
#ifdef ENABLE_NEW_USE_POTION
	)
#endif
		&& (GetDuel("BlockPotion")))
		return;
#endif

	if ((type != AFFECT_AUTO_HP_RECOVERY) && (type != AFFECT_AUTO_SP_RECOVERY)
#ifdef ENABLE_NEW_USE_POTION
		&& (type != AFFECT_AUTO_HP_RECOVERY2) && (type != AFFECT_AUTO_SP_RECOVERY2)
#endif
		)
		return;

	if (nullptr != FindAffect(AFFECT_STUN))
		return;

	{
		const uint32_t stunSkills[] = { SKILL_TANHWAN, SKILL_GEOMPUNG, SKILL_BYEURAK, SKILL_GIGUNG };

		for (size_t i = 0; i < sizeof(stunSkills) / sizeof(uint32_t); ++i)
		{
			const CAffect* p = FindAffect(stunSkills[i]);

			if (nullptr != p && AFF_STUN == p->dwFlag)
				return;
		}
	}

	const CAffect* pAffect = FindAffect(type);
	const size_t idx_of_amount_of_used = 1;
	const size_t idx_of_amount_of_full = 2;

	if (nullptr != pAffect)
	{
		LPITEM pItem = FindItemByID(pAffect->dwFlag);

		if (nullptr != pItem && true == pItem->GetSocket(0))
		{
			if (!CArenaManager::instance().IsArenaMap(GetMapIndex())
#ifdef ENABLE_NEWSTUFF
				&& !(g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(GetPlayerID()) && !IsAllowedPotionOnPVP(pItem->GetVnum()))
#endif
				)
			{
				const int32_t amount_of_used = pItem->GetSocket(idx_of_amount_of_used);
				const int32_t amount_of_full = pItem->GetSocket(idx_of_amount_of_full);

				const int32_t avail = amount_of_full - amount_of_used;

				int32_t amount = 0;
#ifdef ENABLE_NEW_USE_POTION
				if ((type == AFFECT_AUTO_HP_RECOVERY) || (type == AFFECT_AUTO_HP_RECOVERY2))
#else
				if (AFFECT_AUTO_HP_RECOVERY == type)
#endif
				{
					amount = GetMaxHP() - (GetHP() + GetPoint(POINT_HP_RECOVERY));
				}
#ifdef ENABLE_NEW_USE_POTION
				else if ((type == AFFECT_AUTO_SP_RECOVERY) || (type == AFFECT_AUTO_SP_RECOVERY2))
#else
				else if (AFFECT_AUTO_SP_RECOVERY == type)
#endif
				{
					amount = GetMaxSP() - (GetSP() + GetPoint(POINT_SP_RECOVERY));
				}

				if (amount > 0)
				{
					if (avail > amount)
					{
						const int pct_of_used = amount_of_used * 100 / amount_of_full;
						const int pct_of_will_used = (amount_of_used + amount) * 100 / amount_of_full;

						bool bLog = false;
						// »ç¿ë·®ÀÇ 10% ´ÜÀ§·Î ·Î±×¸¦ ³²±è
						// (»ç¿ë·®ÀÇ %¿¡¼­, ½ÊÀÇ ÀÚ¸®°¡ ¹Ù²ð ¶§¸¶´Ù ·Î±×¸¦ ³²±è.)
						if ((pct_of_will_used / 10) - (pct_of_used / 10) >= 1)
							bLog = true;

#ifdef ENABLE_NEW_USE_POTION
						if (pItem->GetVnum() != ITEM_AUTO_HP_RECOVERY_X && pItem->GetVnum() != ITEM_AUTO_SP_RECOVERY_X)
							pItem->SetSocket(idx_of_amount_of_used, amount_of_used + amount, bLog);
#else
						pItem->SetSocket(idx_of_amount_of_used, amount_of_used + amount, bLog);
#endif
					}
					else if (pItem->GetVnum() != ITEM_AUTO_HP_RECOVERY_X && pItem->GetVnum() != ITEM_AUTO_SP_RECOVERY_X)
					{
						amount = avail;

						ITEM_MANAGER::instance().RemoveItem(pItem);
					}

#ifdef ENABLE_NEW_USE_POTION
					if ((type == AFFECT_AUTO_HP_RECOVERY) || (type == AFFECT_AUTO_HP_RECOVERY2))
#else
					if (AFFECT_AUTO_HP_RECOVERY == type)
#endif
					{
						PointChange(POINT_HP_RECOVERY, amount);
						EffectPacket(SE_AUTO_HPUP);
					}
#ifdef ENABLE_NEW_USE_POTION
					else if ((type == AFFECT_AUTO_SP_RECOVERY) || (type == AFFECT_AUTO_SP_RECOVERY2))
#else
					else if (AFFECT_AUTO_SP_RECOVERY == type)
#endif
					{
						PointChange(POINT_SP_RECOVERY, amount);
						EffectPacket(SE_AUTO_SPUP);
					}
				}
			}
			else
			{
				pItem->Lock(false);
				pItem->SetSocket(0, false);
				RemoveAffect(const_cast<CAffect*>(pAffect));
			}
		}
		else
		{
			RemoveAffect(const_cast<CAffect*>(pAffect));
		}
	}
}

bool CHARACTER::IsValidItemPosition(TItemPos Pos) const
{

	uint8_t window_type = Pos.window_type;
	uint16_t cell = Pos.cell;

	switch (window_type)
	{
	case RESERVED_WINDOW:
		return false;

	case INVENTORY:
	case EQUIPMENT:
		return cell < (INVENTORY_AND_EQUIP_SLOT_MAX);

	case DRAGON_SOUL_INVENTORY:
		return cell < (DRAGON_SOUL_INVENTORY_MAX_NUM);
#ifdef ENABLE_SWITCHBOT
	case SWITCHBOT:
		return cell < SWITCHBOT_SLOT_COUNT;
#endif
	case SAFEBOX:
		if (nullptr != m_pkSafebox)
			return m_pkSafebox->IsValidPosition(cell);
		else
			return false;

	case MALL:
		if (nullptr != m_pkMall)
			return m_pkMall->IsValidPosition(cell);
		else
			return false;

#ifdef ENABLE_EXTRA_INVENTORY
	case EXTRA_INVENTORY:
		return cell < (EXTRA_INVENTORY_MAX_NUM);
#endif
	default:
		return false;
	}
}

/// ÇöÀç Ä³¸¯ÅÍÀÇ »óÅÂ¸¦ ¹ÙÅÁÀ¸·Î ÁÖ¾îÁø itemÀ» Âø¿ëÇÒ ¼ö ÀÖ´Â Áö È®ÀÎÇÏ°í, ºÒ°¡´É ÇÏ´Ù¸é Ä³¸¯ÅÍ¿¡°Ô ÀÌÀ¯¸¦ ¾Ë·ÁÁÖ´Â ÇÔ¼ö
#

/// ÇöÀç Ä³¸¯ÅÍÀÇ »óÅÂ¸¦ ¹ÙÅÁÀ¸·Î Âø¿ë ÁßÀÎ itemÀ» ¹þÀ» ¼ö ÀÖ´Â Áö È®ÀÎÇÏ°í, ºÒ°¡´É ÇÏ´Ù¸é Ä³¸¯ÅÍ¿¡°Ô ÀÌÀ¯¸¦ ¾Ë·ÁÁÖ´Â ÇÔ¼ö
#ifdef __ATTR_TRANSFER_SYSTEM__
bool CHARACTER::CanDoAttrTransfer() const
{
	if (m_bIsObserver)
		return false;

	if (GetShop())
		return false;

	if (GetMyShop())
		return false;

	if (m_bUnderRefine)
		return false;

	if (IsWarping())
		return false;

#ifdef ENABLE_ACCE_SYSTEM
	if ((m_bAcceCombination) || (m_bAcceAbsorption))
		return false;
#endif

	return true;
}
#endif

bool CHARACTER::DestroyItem(TItemPos Cell)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ChatPacket(CHAT_TYPE_INFO, "char_item.cpp::bool CHARACTER::DestroyItem(TItemPos Cell),");//INGAME_DEBUG_RAZOR93
#endif
	LPITEM item = nullptr;
	if (!CanHandleItem()) {
#ifdef TEXTS_IMPROVEMENT
		if (nullptr != DragonSoul_RefineWindow_GetOpener()) {
			ChatPacketNew(CHAT_TYPE_INFO, 232, "");
		}
#endif

		return false;
	}

	if (IsDead())
		return false;

	if (!IsValidItemPosition(Cell) || !(item = GetItem(Cell)))
		return false;

	if (item->IsEquipped())
		return false;

	if (item->IsExchanging())
		return false;

	if (true == item->isLocked())
		return false;

	if (quest::CQuestManager::instance().GetPCForce(GetPlayerID())->IsRunning() == true)
		return false;

	if ((item->GetVnum() >= 55701) && (item->GetVnum() <= 55711)) {
		if (item->GetSocket(0) != 0)
			return false;
	}

#ifdef ENABLE_EXTRA_INVENTORY
	if (item->IsExtraItem()) {
		SyncQuickslot(QUICKSLOT_TYPE_ITEM_EXTRA, Cell.cell, 255);
	}
	else {
		SyncQuickslot(QUICKSLOT_TYPE_ITEM, Cell.cell, 255);
	}
#else
	SyncQuickslot(QUICKSLOT_TYPE_ITEM, Cell.cell, 255);
#endif

#ifdef ENABLE_BATTLE_PASS
	uint8_t bBattlePassId = GetBattlePassId();
	if (bBattlePassId)
	{
		uint32_t dwItemVnum, dwCnt;
		if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, DESTROY_ITEM, &dwItemVnum, &dwCnt))
		{
			if (dwItemVnum == item->GetVnum() && GetMissionProgress(DESTROY_ITEM, bBattlePassId) < dwCnt)
				UpdateMissionProgress(DESTROY_ITEM, bBattlePassId, item->GetCount(), dwCnt);
		}
	}
#endif

#ifdef TEXTS_IMPROVEMENT
	ChatPacketNew(CHAT_TYPE_INFO, 47, "%s", item->GetName());
#endif
	ITEM_MANAGER::instance().RemoveItem(item, "DESTROY");
	return true;
}
