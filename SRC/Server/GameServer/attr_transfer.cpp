#define _attr_transfer_cpp_
#include "stdafx.h"
#include "config.h"
#include "constants.h"
#include "utils.h"
#include "log.h"
#include "char.h"
#include "locale_service.h"
#include "item.h"
#include "item_manager.h"
#include <stdlib.h>
#include <sstream>
#define RETURN_IF_ATTR_TRANSFER_IS_NOT_OPENED(ch) if (!(ch)->IsAttrTransferOpen()) return

void AttrTransfer_open(LPCHARACTER ch)
{
	if (ch == nullptr)
		return;

	const LPCHARACTER npc = ch->GetQuestNPC();
	if (npc == nullptr)
	{
		sys_log(0, "%s has try to open the Attr Transfer window without talk to the NPC.", ch->GetName());
		return;
	}
	
	if (ch->IsAttrTransferOpen())
	{
#ifdef TEXTS_IMPROVEMENT
		ch->ChatPacketNew(CHAT_TYPE_INFO, 80, "");
#endif
		return;
	}
	
	if (ch->GetExchange() || ch->GetMyShop() || ch->GetShopOwner() || ch->IsOpenSafebox() || ch->IsCubeOpen() || ch->IsAcceOpen() || ch->IsAttrTransferOpen())
	{
#ifdef TEXTS_IMPROVEMENT
		ch->ChatPacketNew(CHAT_TYPE_INFO, 81, "");
#endif
		return;
	}
	
	int32_t distance = DISTANCE_APPROX(ch->GetX() - npc->GetX(), ch->GetY() - npc->GetY());
	if (distance >= ATTR_TRANSFER_MAX_DISTANCE)
	{
		sys_log(1, "%s is too far for can open the Attr Transfer Window. (character distance: %d, distance allowed: %d)", ch->GetName(), distance, ATTR_TRANSFER_MAX_DISTANCE);
		return;
	}
	
	AttrTransfer_clean_item(ch);
	ch->SetAttrTransferNpc(npc);
	ch->ChatPacket(CHAT_TYPE_COMMAND, "AttrTransfer open");
	if (test_server == true)
	{
		sys_log(1, "%s has open the Attr Transfer window.", ch->GetName());
	}
}

void AttrTransfer_close(LPCHARACTER ch)
{
	RETURN_IF_ATTR_TRANSFER_IS_NOT_OPENED(ch);
	AttrTransfer_clean_item(ch);
	ch->SetAttrTransferNpc(nullptr);
	ch->ChatPacket(CHAT_TYPE_COMMAND, "AttrTransfer close");
	if (test_server == true)
	{
		sys_log(1, "%s has close the Attr Transfer window.", ch->GetName());
	}
}

void AttrTransfer_clean_item(LPCHARACTER ch)
{
	LPITEM* attr_transfer_item = ch->GetAttrTransferItem();
	for (int i = 0; i < MAX_ATTR_TRANSFER_SLOT; ++i)
	{
		if (attr_transfer_item[i] == nullptr)
			continue;
		
		attr_transfer_item[i] = nullptr;
	}
}

bool AttrTransfer_make(LPCHARACTER ch)
{
	int	has_attr = 0;
	
	if (ch == nullptr)
		return false;
	
	if (!(ch)->IsAttrTransferOpen())
	{
#ifdef TEXTS_IMPROVEMENT
		ch->ChatPacketNew(CHAT_TYPE_INFO, 82, "");
#endif
		return false;
	}
	
	LPCHARACTER npc = ch->GetQuestNPC();
	if (npc == nullptr)
	{
		sys_log(0, "%s has try to open the transfer the bonuses between costumes without talk to the NPC.", ch->GetName());
		return false;
	}
	
	LPITEM* items = ch->GetAttrTransferItem();
	if (items[0] == nullptr || items[1] == nullptr || items[2] == nullptr)
	{
#ifdef TEXTS_IMPROVEMENT
		ch->ChatPacketNew(CHAT_TYPE_INFO, 83, "");
#endif
		return false;
	}
	
	if (items[0]->GetType() != ITEM_TRANSFER_SCROLL || items[1]->GetType() != ITEM_COSTUME || items[2]->GetType() != ITEM_COSTUME)
	{
#ifdef TEXTS_IMPROVEMENT
		ch->ChatPacketNew(CHAT_TYPE_INFO, 83, "");
#endif
		return false;
	}
	
	for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
	{
		if (has_attr != 1 && items[2]->GetAttributeType(i) > 0 && items[2]->GetAttributeValue(i) > 0)
		{
			has_attr = 1;
		}
	}
	
	if (has_attr != 1)
	{
#ifdef TEXTS_IMPROVEMENT
		ch->ChatPacketNew(CHAT_TYPE_INFO, 86, "");
#endif
		return false;
	}
	
	for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i) {
#ifdef ENABLE_ATTR_COSTUMES
		if ((i == 5) || (i == 6)) {
			items[1]->SetForceAttribute(i, 0, 0);
		}
		else
			items[1]->SetForceAttribute(i, items[2]->GetAttributeType(i), items[2]->GetAttributeValue(i));
#else
		items[1]->SetForceAttribute(i, items[2]->GetAttributeType(i), items[2]->GetAttributeValue(i));
#endif
	}
	
	items[0]->SetCount(items[0]->GetCount() - 1);
	items[0] = nullptr;
	items[2]->SetCount(items[2]->GetCount() - 1);
	items[2] = nullptr;
	
	ch->ChatPacket(CHAT_TYPE_COMMAND, "AttrTransfer success");
	LogManager::instance().AttrTransferLog(ch->GetPlayerID(), ch->GetX(), ch->GetY(), items[1]->GetVnum());
#ifdef TEXTS_IMPROVEMENT
	ch->ChatPacketNew(CHAT_TYPE_INFO, 84, "");
#endif
	return true;
}

void AttrTransfer_add_item(LPCHARACTER ch, int w_index, int i_index)
{
	RETURN_IF_ATTR_TRANSFER_IS_NOT_OPENED(ch);
	
	if (i_index < 0 || INVENTORY_MAX_NUM <= i_index || w_index < 0 || MAX_ATTR_TRANSFER_SLOT <= w_index)
		return;
	
	LPITEM item = ch->GetInventoryItem(i_index);
	if (item == nullptr)
		return;
	
	if (w_index == 0 && item->GetType() != ITEM_TRANSFER_SCROLL)
		return;
	
	if (((w_index == 1) || (w_index == 2)) && (item->GetType() != ITEM_COSTUME))
		return;

	int32_t vnum = item->GetVnum();
	if (vnum == 73001 ||
		vnum == 73002 ||
		vnum == 73003 ||
		vnum == 73004 ||
		vnum == 73005 ||
		vnum == 73006 ||
		vnum == 73007 ||
		vnum == 73008 ||
		vnum == 73009 ||
		vnum == 73010 ||
		vnum == 73011 ||
		vnum == 73012 ||
		vnum == 75001 ||
		vnum == 75002 ||
		vnum == 75003 ||
		vnum == 75004 ||
		vnum == 75005 ||
		vnum == 75006 ||
		vnum == 75007 ||
		vnum == 75008 ||
		vnum == 75009 ||
		vnum == 75010 ||
		vnum == 75011 ||
		vnum == 75012 ||
		vnum == 75201 ||
		vnum == 75202 ||
		vnum == 75203 ||
		vnum == 75204 ||
		vnum == 75205 ||
		vnum == 75206 ||
		vnum == 75207 ||
		vnum == 75208 ||
		vnum == 75209 ||
		vnum == 75210 ||
		vnum == 75211 ||
		vnum == 75212 ||
		vnum == 73251 ||
		vnum == 73252 ||
		vnum == 73253 ||
		vnum == 73254 ||
		vnum == 73255 ||
		vnum == 73256 ||
		vnum == 73257 ||
		vnum == 73258 ||
		vnum == 73259 ||
		vnum == 73260 ||
		vnum == 73261 ||
		vnum == 73262 ||
		vnum == 73501 ||
		vnum == 73502 ||
		vnum == 73503 ||
		vnum == 73504 ||
		vnum == 73505 ||
		vnum == 73506 ||
		vnum == 73507 ||
		vnum == 73508 ||
		vnum == 73509 ||
		vnum == 73510 ||
		vnum == 73511 ||
		vnum == 73512 ||
		vnum == 75401 ||
		vnum == 75402 ||
		vnum == 75403 ||
		vnum == 75404 ||
		vnum == 75405 ||
		vnum == 75406 ||
		vnum == 75407 ||
		vnum == 75408 ||
		vnum == 75409 ||
		vnum == 75410 ||
		vnum == 75411 ||
		vnum == 75412 ||
		vnum == 73751 ||
		vnum == 73752 ||
		vnum == 73753 ||
		vnum == 73754 ||
		vnum == 73755 ||
		vnum == 73756 ||
		vnum == 73757 ||
		vnum == 73758 ||
		vnum == 73759 ||
		vnum == 73760 ||
		vnum == 73761 ||
		vnum == 73762 ||
		vnum == 75601 ||
		vnum == 75602 ||
		vnum == 75603 ||
		vnum == 75604 ||
		vnum == 75605 ||
		vnum == 75606 ||
		vnum == 75607 ||
		vnum == 75608 ||
		vnum == 75609 ||
		vnum == 75610 ||
		vnum == 75611 ||
		vnum == 75612)
	{
		return;
	}

	if ((item->GetSubType() != COSTUME_BODY) && (item->GetSubType() != COSTUME_HAIR) && (item->GetSubType() != COSTUME_WEAPON)
#ifdef ENABLE_STOLE_COSTUME
	 && (item->GetSubType() != COSTUME_STOLE)
#endif
	)
		return;
	
	LPITEM* attr_transfer_item = ch->GetAttrTransferItem();
	for (int i = 0; i < MAX_ATTR_TRANSFER_SLOT; ++i)
	{
		if (item == attr_transfer_item[i])
		{
			attr_transfer_item[i] = nullptr;
			break;
		}
	}
	
	if (w_index != 0 && attr_transfer_item[0] == nullptr)
	{
#ifdef TEXTS_IMPROVEMENT
		ch->ChatPacketNew(CHAT_TYPE_INFO, 85, "");
#endif
		return;
	}
	else if (w_index == 1)
	{
		if (attr_transfer_item[2] == nullptr) {
#ifdef TEXTS_IMPROVEMENT
			ch->ChatPacketNew(CHAT_TYPE_INFO, 79, "");
#endif
			return;
		}
		else if (item->GetSubType() != attr_transfer_item[2]->GetSubType())
			return;
	}
	
	if (w_index == 1)
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "AttrTransferMessage");
	}
	
	attr_transfer_item[w_index] = item;
	return;
}

void AttrTransfer_delete_item(LPCHARACTER ch, int w_index)
{
	//LPITEM	item;
	RETURN_IF_ATTR_TRANSFER_IS_NOT_OPENED(ch);
	
	if (w_index < 0 || MAX_ATTR_TRANSFER_SLOT <= w_index)
		return;
	
	LPITEM* attr_transfer_item = ch->GetAttrTransferItem();
	if (attr_transfer_item[w_index] == nullptr)
		return;
	
	//attr_transfer_item[w_index];
	attr_transfer_item[w_index] = nullptr;
	return;
}
