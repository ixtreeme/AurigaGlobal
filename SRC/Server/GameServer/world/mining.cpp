#include "stdafx.h"
#include "../mission_pass/MissionPassEvents.hpp"
#include "../ecs/AIHelpers.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/SkillSystem.hpp"
#include "../ecs/systems/ActivitySystem.hpp"
#include <Core/Logging.hpp>
#include "mining.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "../ecs/CharacterAccessors.hpp"
#include "../ecs/EntityFactory.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/VIDRegistry.hpp"
#include "../ecs/systems/ItemSystem.hpp"
#include "item_manager.h"
#include "item.h"
#include "config.h"
#include "db.h"
#include "log.h"
#include "skill.h"

#define ENABLE_PICKAXE_RENEWAL
namespace mining
{
	enum
	{
		MAX_ORE = 19,
		MAX_FRACTION_COUNT = 9,
	};

	struct SInfo
	{
		uint32_t dwLoadVnum;
		uint32_t dwRawOreVnum;
	};

	SInfo info[MAX_ORE] =
	{
		{ 20047, 50601 },
		{ 20048, 50602 },
		{ 20049, 50603 },
		{ 20050, 50604 },
		{ 20051, 50605 },
		{ 20052, 50606 },
		{ 20053, 50607 },
		{ 20054, 50608 },
		{ 20055, 50609 },
		{ 20056, 50610 },
		{ 20057, 50611 },
		{ 20058, 50612 },
		{ 20059, 50613 },
		{ 30301, 50614 },
		{ 30302, 50615 },
		{ 30303, 50616 },
		{ 30304, 50617 },
		{ 30305, 50618 },
		{ 30306, 50619 },
	};

	int fraction_info[MAX_FRACTION_COUNT][3] =
	{
		{ 70,  10, 20 },
		{ 60, 10, 25 },
		{ 50, 10, 30 },
		{ 40, 11, 40 },
		{  30, 11, 50 },
		{  20, 11, 60 },
		{  10, 11, 70 },
		{  5, 11, 80 },
		{  1, 81, 90 },
	};

	int PickGradeAddPct[10] =
	{
		3, 5, 8, 11, 15, 20, 26, 32, 40, 50
	};

	int SkillLevelAddPct[SKILL_MAX_LEVEL + 1] =
	{
		0,
		1, 1, 1, 1,		//  1 - 4
		2, 2, 2, 2,		//  5 - 8
		3, 3, 3, 3,		//  9 - 12
		4, 4, 4, 4,		// 13 - 16
		5, 5, 5, 5,		// 17 - 20
		6, 6, 6, 6,		// 21 - 24
		7, 7, 7, 7,		// 25 - 28
		8, 8, 8, 8,		// 29 - 32
		9, 9, 9, 9,		// 33 - 36
		10, 10, 10, 	// 37 - 39
		11,				// 40
	};

	uint32_t GetRawOreFromLoad(uint32_t dwLoadVnum)
	{
		for (int i = 0; i < MAX_ORE; ++i)
		{
			if (info[i].dwLoadVnum == dwLoadVnum)
				return info[i].dwRawOreVnum;
		}
		return 0;
	}

	int GetFractionCount()
	{
		int r = number(1, 100);

		for (int i = 0; i < MAX_FRACTION_COUNT; ++i)
		{
			if (r <= fraction_info[i][0])
				return number(fraction_info[i][1], fraction_info[i][2]);
			else
				r -= fraction_info[i][0];
		}

		return 0;
	}

	void OreDrop(entt::entity character, uint32_t dwLoadVnum)
	{
		const uint32_t dwRawOreVnum = GetRawOreFromLoad(dwLoadVnum);

		const int iFractionCount = GetFractionCount();

		if (iFractionCount == 0)
		{
			LOG_ERROR("Wrong ore fraction count");
			return;
		}

		const entt::entity item = ITEM_MANAGER::instance().CreateItem(dwRawOreVnum, iFractionCount);

		if (!ItemSystem::IsValidItem(item))
		{
			LOG_ERROR("cannot create item vnum {}", dwRawOreVnum);
			return;
		}

		PIXEL_POSITION pos{};
		pos.x = ecs::PlayerRuntime::GetX(character) + number(-200, 200);
		pos.y = ecs::PlayerRuntime::GetY(character) + number(-200, 200);

		if (!ItemSystem::PlaceItemOnGround(
				item, ecs::PlayerRuntime::GetMapIndex(character), pos))
		{
			ItemSystem::DestroyItemEntityEcs(item, "MINING_ORE_GROUND_FAIL");
			return;
		}
		ItemSystem::SetGroundOwnership(item, character, 15);
		AurigaMissionPass::Events::Emit(character,AurigaMissionPass::Kind::OreGather,dwRawOreVnum,iFractionCount);

		DBManager::instance().SendMoneyLog(
			MONEY_LOG_DROP, ItemSystem::GetItemVnum(item), ItemSystem::GetItemCount(item));
	}

	int GetOrePct(entt::entity character)
	{
		const int defaultPct = NAGYFASZU_MINING_CHANCE;
		const int iSkillLevel = SkillSystem::GetSkillLevel(character, SKILL_MINING);
		const entt::entity pick = ItemSystem::GetWearItem(character, WEAR_WEAPON);

		if (!ItemSystem::IsValidItem(pick) || ItemSystem::GetItemType(pick) != ITEM_PICK)
			return 0;

		return defaultPct + SkillLevelAddPct[MINMAX(0, iSkillLevel, 40)] +
			PickGradeAddPct[MINMAX(0, ItemSystem::GetItemRefineLevel(pick), 9)];
	}

	EVENTINFO(mining_event_info)
	{
		uint32_t pid;
		uint32_t vid_load;

		mining_event_info()
		: pid( 0 )
		, vid_load( 0 )
		{
		}
	};

	// REFINE_PICK
	bool Pick_Check(entt::entity item)
	{
		return ItemSystem::IsValidItem(item) && ItemSystem::GetItemType(item) == ITEM_PICK;
	}

	int Pick_GetMaxExp(entt::entity pick)
	{
		return ItemSystem::GetItemValue(pick, 2);
	}

	int Pick_GetCurExp(entt::entity pick)
	{
		return ItemSystem::GetItemSocket(pick, 0);
	}

	void Pick_IncCurExp(entt::entity pick)
	{
		const int cur = Pick_GetCurExp(pick);
		ItemSystem::SetItemSocket(pick, 0, cur + 1);
	}

#ifdef ENABLE_PICKAXE_RENEWAL
	void Pick_SetPenaltyExp(entt::entity pick)
	{
		const int cur = Pick_GetCurExp(pick);
		ItemSystem::SetItemSocket(pick, 0, (cur > 0) ? (cur - (cur * 10 / 100)) : 0);
	}
#endif

	void Pick_MaxCurExp(entt::entity pick)
	{
		ItemSystem::SetItemSocket(pick, 0, Pick_GetMaxExp(pick));
	}

	bool Pick_Refinable(entt::entity item)
	{
		if (Pick_GetCurExp(item) < Pick_GetMaxExp(item))
			return false;

		return true;
	}

	bool Pick_IsPracticeSuccess(entt::entity pick)
	{
		return number(1, ItemSystem::GetItemValue(pick, 1)) == 1;
	}

	bool Pick_IsRefineSuccess(entt::entity pick)
	{
		return number(1, 100) <= ItemSystem::GetItemValue(pick, 3);
	}

	int RealRefinePick(entt::entity character, entt::entity item)
	{
		if (character == entt::null || !g_registry.valid(character) ||
			!ItemSystem::IsValidItem(item))
			return 2;

		LogManager& rkLogMgr = LogManager::instance();

		if (!Pick_Check(item))
		{
			LOG_ERROR("REFINE_PICK_HACK pid({}) item({}:{}) type({})",
				ecs::PlayerRuntime::GetPlayerID(character), ItemSystem::GetItemName(item),
				ItemSystem::GetItemID(item), ItemSystem::GetItemType(item));
			rkLogMgr.RefineLog(ecs::PlayerRuntime::GetPlayerID(character),
				ItemSystem::GetItemName(item), ItemSystem::GetItemID(item), -1, 1, "PICK_HACK");
			return 2;
		}

		if (!Pick_Refinable(item))
			return 2;

		const int iAdv = ItemSystem::GetItemValue(item, 0) / 10;

		if (ItemSystem::IsItemEquipped(item))
			return 2;

		if (Pick_IsRefineSuccess(item))
		{
			rkLogMgr.RefineLog(ecs::PlayerRuntime::GetPlayerID(character),
				ItemSystem::GetItemName(item), ItemSystem::GetItemID(item), iAdv, 1, "PICK");

			const entt::entity newPick = ITEM_MANAGER::instance().CreateItem(ItemSystem::GetItemRefineVnum(item), 1);
			if (!ItemSystem::IsValidItem(newPick))
				return 2;

			const uint16_t cell = ItemSystem::GetItemCell(item);
			if (!ItemSystem::RemoveItemEcs(item) ||
				!ItemSystem::PlaceItemEcs(character, newPick, INVENTORY, cell))
			{
				ItemSystem::PlaceItemEcs(character, item, INVENTORY, cell);
				ItemSystem::DestroyItemEntityEcs(newPick, "REFINE_PICK_PLACE_ROLLBACK");
				return 2;
			}

			LogManager::instance().ItemLogEntity(
				character, newPick,
				"REFINE PICK SUCCESS", ItemSystem::GetItemName(newPick));
			if (!ItemSystem::DestroyItemEntityEcs(item, "REMOVE (REFINE PICK)"))
			{
				ItemSystem::RemoveItemEcs(newPick);
				ItemSystem::PlaceItemEcs(character, item, INVENTORY, cell);
				ItemSystem::DestroyItemEntityEcs(newPick, "REFINE_PICK_SOURCE_ROLLBACK");
				return 2;
			}
			return 1;
		}
		else
		{
			rkLogMgr.RefineLog(ecs::PlayerRuntime::GetPlayerID(character),
				ItemSystem::GetItemName(item), ItemSystem::GetItemID(item), iAdv, 0, "PICK");

#ifdef ENABLE_PICKAXE_RENEWAL
			{
				Pick_SetPenaltyExp(item);
				rkLogMgr.ItemLogEntity(character, item,
					"REFINE PICK FAIL", ItemSystem::GetItemName(item));
				return 0;
			}
#else
			const entt::entity newPick = ITEM_MANAGER::instance().CreateItem(ItemSystem::GetItemValue(item, 4), 1);

			if (ItemSystem::IsValidItem(newPick))
			{
				const uint16_t cell = ItemSystem::GetItemCell(item);
				if (!ItemSystem::RemoveItemEcs(item) ||
					!ItemSystem::PlaceItemEcs(character, newPick, INVENTORY, cell))
				{
					ItemSystem::PlaceItemEcs(character, item, INVENTORY, cell);
					ItemSystem::DestroyItemEntityEcs(newPick, "REFINE_PICK_FAIL_ROLLBACK");
					return 2;
				}
				ItemSystem::DestroyItemEntityEcs(item, "REMOVE (REFINE PICK)");
				rkLogMgr.ItemLogEntity(character, newPick,
					"REFINE PICK FAIL", ItemSystem::GetItemName(newPick));
				return 0;
			}
#endif
			return 2;
		}
	}

	void CHEAT_MAX_PICK(entt::entity character, entt::entity item)
	{
		if (!Pick_Check(item))
			return;

		Pick_MaxCurExp(item);

#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 249, "%d", Pick_GetCurExp(item));
#endif
	}

	void PracticePick(entt::entity character, entt::entity pick)
	{
		if (!Pick_Check(pick))
			return;

		if (ItemSystem::GetItemRefineVnum(pick) == 0)
			return;

		if (Pick_IsPracticeSuccess(pick))
		{

			if (Pick_Refinable(pick))
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 250, "");
				ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 273, "");
#endif
			}
			else
			{
				Pick_IncCurExp(pick);
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 253, "%d#%d", Pick_GetCurExp(pick), Pick_GetMaxExp(pick));
#endif
				if (Pick_Refinable(pick))
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 250, "");
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 273, "");
#endif
				}
			}
		}
	}

	EVENTFUNC(mining_event)
	{
		mining_event_info* info = dynamic_cast<mining_event_info*>( event->info );

		if ( info == nullptr)
		{
			LOG_ERROR("mining_event_info> <Factor> Null pointer");
			return 0;
		}

		const entt::entity character = ecs::PlayerRuntime::FindByPlayerID(info->pid);
		const entt::entity load = CVIDRegistry::Instance().Find(info->vid_load);
		if (character == entt::null || !g_registry.valid(character))
			return 0;

		ActivitySystem::FinishMining(character);

		const entt::entity pick = ItemSystem::GetWearItem(character, WEAR_WEAPON);

		// REFINE_PICK
		if (!Pick_Check(pick))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 251, "");
#endif
			return 0;
		}
		// END_OF_REFINE_PICK

		if (load == entt::null || !g_registry.valid(load))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 309, "");
#endif
			return 0;
		}

		const int iPct = GetOrePct(character);

		if (number(1, 100) <= iPct)
		{
			OreDrop(character, ecs::PlayerRuntime::GetRaceNum(load));
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 470, "");
#endif
		}
#ifdef TEXTS_IMPROVEMENT
		else {
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 471, "");
		}
#endif

		PracticePick(character, pick);
		return 0;
	}

	LPEVENT CreateMiningEvent(entt::entity character, entt::entity load, int count)
	{
		if (character == entt::null || load == entt::null ||
			!g_registry.valid(character) || !g_registry.valid(load))
			return nullptr;

		mining_event_info* info = AllocEventInfo<mining_event_info>();
		info->pid = ecs::PlayerRuntime::GetPlayerID(character);
		info->vid_load = ecs::PlayerRuntime::GetPacketVID(load);

		return event_create(mining_event, info, PASSES_PER_SEC(2 * count));
	}

	bool IsVeinOfOre (uint32_t vnum)
	{
		for (int i = 0; i < MAX_ORE; i++)
		{
			if (info[i].dwLoadVnum == vnum)
				return true;
		}
		return false;
	}
}


