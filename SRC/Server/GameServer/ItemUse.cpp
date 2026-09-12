#include "stdafx.h"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "ecs/systems/PointSystem.hpp"

#include "ItemUse.h"
#include "char.h" // POINT_GOLD constants; item use does not access CHARACTER.

#include "ecs/systems/ItemSystem.hpp"

#include <algorithm>
#include <stdint.h>
#include <common/length.h>
#include "VikingDungeon.h"
namespace
{
	// Adds Dragon Coins safely (clamped to uint32 max) and prints an English message.
	void AddDragonCoinSafe(entt::entity chEntity, uint32_t amount)
	{
		if (!ecs::PlayerRuntime::IsValid(chEntity) || amount == 0)
			return;

		const uint32_t cur = ecs::PlayerRuntime::GetDragonCoin(chEntity);
		const uint64_t maxCoins = 0xFFFFFFFFULL; // uint32 max

		if ((uint64_t)cur >= maxCoins)
		{
			ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "You cannot receive more Dragon Coins.");
			return;
		}

		const uint64_t canAdd = std::min<uint64_t>((uint64_t)amount, maxCoins - (uint64_t)cur);
		if (canAdd == 0)
			return;

		ecs::PlayerRuntime::SetDragonCoin(chEntity, cur + (uint32_t)canAdd);
		ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "You received %u Dragon Coins.", (uint32_t)canAdd);
	}

	inline bool CheckCanUseNow(entt::entity ch)
	{
		if (!ecs::PlayerRuntime::IsValid(ch))
			return false;

		if (!ecs::PlayerRuntime::CanWarp(ch))
		{
			ecs::ChatSystem::Send(ch, CHAT_TYPE_INFO, "You cannot use this item right now.");
			return false;
		}

		return true;
	}
}

namespace item_change
{
	bool HandleUse(entt::entity chEntity, entt::entity item)
	{
		if (!ecs::PlayerRuntime::IsPC(chEntity) || !ItemSystem::CanConsumeOwnedItem(chEntity, item))
			return false;
#ifdef ENABLE_CPP_DUNGEON_RAZOR93
		if (CVikingDungeon::instance().OnUseItem(chEntity, item))
			return true;
#endif
		switch (ItemSystem::GetItemVnum(item))
		{
			// 30279: consumes 100 (across all stacks), gives 30280 x1
			case 30279:
			{
				if (!CheckCanUseNow(chEntity))
					return true;

				
				if (ItemSystem::CountItem(chEntity, 30279) < 100)
				{
					ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "You need 100 crystals to exchange.");
					return true;
				}

				if (!ItemSystem::RemoveSpecifyItemEcs(chEntity, 30279, 100))
					return true;
				ItemSystem::AutoGiveItemEcs(chEntity, 30280, 1);
				ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "Exchange complete.");
				return true;
			}
			// kristaly
			case 30277:
			{
				if (!CheckCanUseNow(chEntity))
					return true;

				
				if (ItemSystem::CountItem(chEntity, 30277) < 100)
				{
					ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "You need 100 crystals to exchange.");
					return true;
				}

				if (!ItemSystem::RemoveSpecifyItemEcs(chEntity, 30277, 100))
					return true;
				ItemSystem::AutoGiveItemEcs(chEntity, 30278, 1);
				ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "Exchange complete.");
				return true;
			}


			// 39067: +100 Dragon Coin (consumes 1)
			case 39067:
			{
				if (!CheckCanUseNow(chEntity))
					return true;

				if (ItemSystem::GetItemCount(item) < 1)
					return true;

				if (ItemSystem::ConsumeItemEcs(item))
					AddDragonCoinSafe(chEntity, 100);
				return true;
			}

			// 39068: +10,000,000 gold (consumes 1)
			case 39068:
			{
				if (!CheckCanUseNow(chEntity))
					return true;

				const int32_t count = ItemSystem::GetItemCount(item);
				if (count <= 0)
					return true;

				const int64_t kYangPerItem = 10000000LL;
				const int64_t maxGold = (int64_t)GOLD_MAX;

				const int64_t beforeGold = (int64_t)ecs::PointSystem::GetGold(chEntity);
				const int64_t freeSpace = maxGold - beforeGold;

				if (freeSpace <= 0)
				{
					ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "You can't receive more Yang (gold cap reached).");
					return true; // semmit nem vesz el
				}

				 
				int32_t wantUse = (int32_t)(freeSpace / kYangPerItem);
				if (wantUse <= 0)
				{
					ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "Not enough Yang capacity to redeem even 1 item.");
					return true; // semmit nem vesz el
				}

				if (wantUse > count)
					wantUse = count;

				const int64_t wantAdd = kYangPerItem * (int64_t)wantUse;

				 
				ecs::PointSystem::Change(chEntity, POINT_GOLD, wantAdd, true);

				 
				const int64_t afterGold = (int64_t)ecs::PointSystem::GetGold(chEntity);
				int64_t realAdded = afterGold - beforeGold;

				if (realAdded <= 0)
				{
					ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "You can't receive more Yang (gold cap reached).");
					return true; // semmit nem vesz el
				}

				 
				int32_t realUse = (int32_t)(realAdded / kYangPerItem);
				if (realUse <= 0)
				{
					ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "You can't receive more Yang (gold cap reached).");
					return true; // semmit nem vesz el
				}
				if (realUse > count)
					realUse = count;

				 
				ItemSystem::ConsumeItemEcs(item, realUse);

				ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "You received %lld Yang.", (long long)(kYangPerItem * (int64_t)realUse));
				return true;
			}
		}

		return false;
	}
}
