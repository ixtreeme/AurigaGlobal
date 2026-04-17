#include "stdafx.h"

#include "ItemUse.h"

#include "char_interface.hpp"
#include "item.h"

#include <algorithm>
#include <stdint.h>
#include <common/length.h>
#include "VikingDungeon.h"
namespace
{
	// Adds Dragon Coins safely (clamped to uint32 max) and prints an English message.
	void AddDragonCoinSafe(LPCHARACTER ch, uint32_t amount)
	{
		if (!ch || amount == 0)
			return;

		const uint32_t cur = ch->GetDragonCoin();
		const uint64_t maxCoins = 0xFFFFFFFFULL; // uint32 max

		if ((uint64_t)cur >= maxCoins)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "You cannot receive more Dragon Coins.");
			return;
		}

		const uint64_t canAdd = std::min<uint64_t>((uint64_t)amount, maxCoins - (uint64_t)cur);
		if (canAdd == 0)
			return;

		ch->SetDragonCoin(cur + (uint32_t)canAdd);
		ch->ChatPacket(CHAT_TYPE_INFO, "You received %u Dragon Coins.", (uint32_t)canAdd);
	}

	inline bool CheckCanUseNow(LPCHARACTER ch)
	{
		if (!ch)
			return false;

		if (!ch->CanWarp())
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "You cannot use this item right now.");
			return false;
		}

		return true;
	}
}

namespace item_change
{
	bool HandleUse(CHARACTER* chRaw, CItem* itemRaw)
	{
		LPCHARACTER ch = (LPCHARACTER)chRaw;
		LPITEM item = (LPITEM)itemRaw;

		if (!ch || !item)
			return false;
#ifdef ENABLE_CPP_DUNGEON_RAZOR93
		if (CVikingDungeon::instance().OnUseItem(ch, item))
			return true;
#endif
		switch (item->GetVnum())
		{
			//-----------------------------------------//
			//        EZT A CHAR_ITEM.CPP KEZELI       //
			// ----------------------------------------//
			// 39065: +1 Dragon Coin (consumes 1)
			//case 39065:
			//{
			//	if (!CheckCanUseNow(ch))
			//		return true;

			//	if (item->GetCount() < 1)
			//		return true;

			//	item->SetCount(item->GetCount() - 1);
			//	AddDragonCoinSafe(ch, 1);
			//	return true;
			//}

			// 30279: consumes 100 (across all stacks), gives 30280 x1
			case 30279:
			{
				if (!CheckCanUseNow(ch))
					return true;

				
				if (ch->CountSpecifyItem(30279) < 100)
				{
					ch->ChatPacket(CHAT_TYPE_INFO, "You need 100 crystals to exchange.");
					return true;
				}

				ch->RemoveSpecifyItem(30279, 100);
				ch->AutoGiveItem(30280, 1);
				ch->ChatPacket(CHAT_TYPE_INFO, "Exchange complete.");
				return true;
			}
			// kristaly
			case 30277:
			{
				if (!CheckCanUseNow(ch))
					return true;

				
				if (ch->CountSpecifyItem(30277) < 100)
				{
					ch->ChatPacket(CHAT_TYPE_INFO, "You need 100 crystals to exchange.");
					return true;
				}

				ch->RemoveSpecifyItem(30277, 100);
				ch->AutoGiveItem(30278, 1);
				ch->ChatPacket(CHAT_TYPE_INFO, "Exchange complete.");
				return true;
			}


			// 39067: +100 Dragon Coin (consumes 1)
			case 39067:
			{
				if (!CheckCanUseNow(ch))
					return true;

				if (item->GetCount() < 1)
					return true;

				item->SetCount(item->GetCount() - 1);
				AddDragonCoinSafe(ch, 100);
				return true;
			}

			// 39068: +10,000,000 gold (consumes 1)
			case 39068:
			{
				if (!CheckCanUseNow(ch))
					return true;

				const int32_t count = item->GetCount();
				if (count <= 0)
					return true;

				const int64_t kYangPerItem = 10000000LL;
				const int64_t maxGold = (int64_t)GOLD_MAX;

				const int64_t beforeGold = (int64_t)ch->GetGold();
				const int64_t freeSpace = maxGold - beforeGold;

				if (freeSpace <= 0)
				{
					ch->ChatPacket(CHAT_TYPE_INFO, "You can't receive more Yang (gold cap reached).");
					return true; // semmit nem vesz el
				}

				 
				int32_t wantUse = (int32_t)(freeSpace / kYangPerItem);
				if (wantUse <= 0)
				{
					ch->ChatPacket(CHAT_TYPE_INFO, "Not enough Yang capacity to redeem even 1 item.");
					return true; // semmit nem vesz el
				}

				if (wantUse > count)
					wantUse = count;

				const int64_t wantAdd = kYangPerItem * (int64_t)wantUse;

				 
				ch->PointChange(POINT_GOLD, wantAdd, true);

				 
				const int64_t afterGold = (int64_t)ch->GetGold();
				int64_t realAdded = afterGold - beforeGold;

				if (realAdded <= 0)
				{
					ch->ChatPacket(CHAT_TYPE_INFO, "You can't receive more Yang (gold cap reached).");
					return true; // semmit nem vesz el
				}

				 
				int32_t realUse = (int32_t)(realAdded / kYangPerItem);
				if (realUse <= 0)
				{
					ch->ChatPacket(CHAT_TYPE_INFO, "You can't receive more Yang (gold cap reached).");
					return true; // semmit nem vesz el
				}
				if (realUse > count)
					realUse = count;

				 
				item->SetCount(count - realUse);

				ch->ChatPacket(CHAT_TYPE_INFO, "You received %lld Yang.", (long long)(kYangPerItem * (int64_t)realUse));
				return true;
			}
		}

		return false;
	}
}
