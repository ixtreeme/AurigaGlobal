#include "stdafx.h"
#include <common/service.h>
#include <common/CommonDefines.h>
#include "char.h"
#include "item.h"
#include "questmanager.h"

void CHARACTER::ItemDivision(TItemPos Cell)
{
	return;
//	if (!CanHandleItem()) {
//#ifdef TEXTS_IMPROVEMENT
//		if (NULL != DragonSoul_RefineWindow_GetOpener()) {
//			ChatPacketNew(CHAT_TYPE_INFO, 232, "");
//		}
//#endif
//	
//		return;
//	}
//	
//	if (IsDead())
//		return;
//	
//	if (!IsValidItemPosition(Cell))
//		return;
//	
//	if (quest::CQuestManager::instance().GetPCForce(GetPlayerID())->IsRunning() == true)
//		return;
//	
//	LPITEM pItem = GetItem(Cell);
//	if (!pItem)
//		return;
//	
//	if ((pItem->IsEquipped()) || (pItem->IsExchanging()) || (pItem->isLocked()))
//		return;
//	
//	if (pItem->GetCount() <= 1)
//		return;
//	
//	uint8_t bWindowType = Cell.window_type;
//	int iCell = 0;
//	bool bDivided = false;
//	while (!bDivided) {
//		if (pItem->GetCount() > 1) {
//			if (bWindowType == INVENTORY) {
//				iCell = GetEmptyInventory(pItem->GetSize());
//				if (iCell == -1) {
//					bDivided = true;
//				}
//				else {
//					TItemPos DestCell;
//					DestCell.window_type = bWindowType;
//					DestCell.cell = iCell;
//					
//					MoveItem(Cell, DestCell, 1);
//				}
//			}
//#ifdef ENABLE_EXTRA_INVENTORY
//			else if (bWindowType == EXTRA_INVENTORY) {
//				iCell = GetEmptyExtraInventory(pItem);
//				if (iCell == -1) {
//					bDivided = true;
//				}
//				else {
//					TItemPos DestCell;
//					DestCell.window_type = bWindowType;
//					DestCell.cell = iCell;
//					
//					MoveItem(Cell, DestCell, 1);
//				}
//			}
//#endif
//			else {
//				bDivided = true;
//			}
//		}
//		else {
//			bDivided = true;
//		}
//	}
//	
//	SyncQuickslot(QUICKSLOT_TYPE_ITEM, Cell.cell, 255);
}

