#include "stdafx.h"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/AIHelpers.hpp"

#include "ItemUse.h"

#include "../ecs/systems/ItemSystem.hpp"

namespace
{
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


		}

		return false;
	}
}
