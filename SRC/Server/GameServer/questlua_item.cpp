#include "stdafx.h"
#include <Core/Logging.hpp>
#include "questmanager.h"
#include "char_interface.hpp"
#include "item_manager.h"
#include "ecs/Registry.hpp"
#include "ecs/systems/ItemSystem.hpp"
#include "over9refine.h"
#include "log.h"
#include "db.h"

#undef sys_err
#ifndef _WIN32
#define sys_err(fmt, args...) quest::CQuestManager::instance().QuestErrorFmt(__FUNCTION__, __LINE__, FMT_STRING(fmt), ##args)
#else
#define sys_err(fmt, ...) quest::CQuestManager::instance().QuestErrorFmt(__FUNCTION__, __LINE__, FMT_STRING(fmt), __VA_ARGS__)
#endif

namespace quest
{
	//
	// "item" Lua functions
	//

	ALUA(item_get_cell)
	{
		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();
		lua_pushnumber(L, ItemSystem::IsValidItem(item) ? ItemSystem::GetItemCell(item) : 0);
		return 1;
	}

	ALUA(item_select_cell)
	{
		lua_pushboolean(L, 0);
		if (!lua_isnumber(L, 1))
		{
			return 1;
		}
		uint32_t cell = (uint32_t) lua_tonumber(L, 1);

		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		const entt::entity item = ItemSystem::GetInventoryItem(chEntity, cell);

		if (!ItemSystem::IsValidItem(item))
		{
			return 1;
		}

		CQuestManager::instance().SetCurrentItem(item);
		lua_pushboolean(L, 1);

		return 1;
	}

	ALUA(item_select)
	{
		lua_pushboolean(L, 0);
		if (!lua_isnumber(L, 1))
		{
			return 1;
		}
		uint32_t id = (uint32_t) lua_tonumber(L, 1);
		const entt::entity item = ItemSystem::FindItemByID(CQuestManager::instance().GetCurrentPCEntity(), id);

		if (!ItemSystem::IsValidItem(item))
		{
			return 1;
		}

		CQuestManager::instance().SetCurrentItem(item);
		lua_pushboolean(L, 1);

		return 1;
	}

	ALUA(item_get_id)
	{
		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();
		lua_pushnumber(L, ItemSystem::IsValidItem(item) ? ItemSystem::GetItemID(item) : 0);
		return 1;
	}

	ALUA(item_remove)
	{
		CQuestManager& q = CQuestManager::instance();
		const entt::entity item = q.GetCurrentItemEntity();
		if (ItemSystem::IsValidItem(item)) {
			if (q.GetCurrentPCEntity() == ItemSystem::GetItemOwner(item)) {
				ItemSystem::ConsumeItem(item, ItemSystem::GetItemCount(item));
			} else {
				sys_err("Tried to remove invalid item entity {}", static_cast<uint32_t>(item));
			}
			q.ClearCurrentItem();
		}

		return 0;
	}

	ALUA(item_get_socket)
	{
		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();
		if (ItemSystem::IsValidItem(item) && lua_isnumber(L, 1))
		{
			int idx = (int) lua_tonumber(L, 1);
			lua_pushnumber(L, ItemSystem::GetItemSocket(item, idx));
		}
		else
		{
			lua_pushnumber(L,0);
		}
		return 1;
	}

	ALUA(item_set_socket)
	{
		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();
		if (ItemSystem::IsValidItem(item) && lua_isnumber(L,1) && lua_isnumber(L,2))
		{
			int idx = (int) lua_tonumber(L, 1);
			int value = (int) lua_tonumber(L, 2);
			ItemSystem::SetItemSocketEcs(item, idx, value);
		}
		return 0;
	}

	ALUA(item_get_vnum)
	{
		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();
		lua_pushnumber(L, ItemSystem::IsValidItem(item) ? ItemSystem::GetItemVnum(item) : 0);
		return 1;
	}

	ALUA(item_has_flag)
	{
		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();

		if (!lua_isnumber(L, 1))
		{
			sys_err("flag is not a number.");
			lua_pushboolean(L, 0);
			return 1;
		}

		if (!ItemSystem::IsValidItem(item))
		{
			lua_pushboolean(L, 0);
			return 1;
		}

		int32_t lCheckFlag = (int32_t) lua_tonumber(L, 1);
		lua_pushboolean(L, IS_SET(ItemSystem::GetItemFlags(item), lCheckFlag));

		return 1;
	}

	ALUA(item_get_value)
	{
		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();

		if (!ItemSystem::IsValidItem(item))
		{
			lua_pushnumber(L, 0);
			return 1;
		}

		if (!lua_isnumber(L, 1))
		{
			sys_err("index is not a number");
			lua_pushnumber(L, 0);
			return 1;
		}

		int index = (int) lua_tonumber(L, 1);

		if (index < 0 || index >= ITEM_VALUES_MAX_NUM)
		{
			sys_err("index({}) is out of range (0..{})", index, ITEM_VALUES_MAX_NUM);
			lua_pushnumber(L, 0);
		}
		else
			lua_pushnumber(L, ItemSystem::GetItemValue(item, index));

		return 1;
	}

	ALUA(item_set_value)
	{
		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();

		if (!ItemSystem::IsValidItem(item))
		{
			lua_pushnumber(L, 0);
			return 1;
		}

		if (false == (lua_isnumber(L, 1) && lua_isnumber(L, 2) && lua_isnumber(L, 3)))
		{
			sys_err("index is not a number");
			lua_pushnumber(L, 0);
			return 1;
		}

		ItemSystem::SetItemForceAttributeEcs(
			item,
			lua_tonumber(L, 1),		// index
			lua_tonumber(L, 2),		// apply type
			lua_tonumber(L, 3)		// apply value
		);

		return 0;
	}

	ALUA(item_get_name)
	{
		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();
		lua_pushstring(L, ItemSystem::IsValidItem(item) ? ItemSystem::GetItemName(item) : "");

		return 1;
	}

	ALUA(item_get_size)
	{
		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();
		lua_pushnumber(L, ItemSystem::IsValidItem(item) ? ItemSystem::GetItemSize(item) : 0);

		return 1;
	}

	ALUA(item_get_count)
	{
		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();
		lua_pushnumber(L, ItemSystem::IsValidItem(item) ? ItemSystem::GetItemCount(item) : 0);
		return 1;
	}

	ALUA(item_get_type)
	{
		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();
		lua_pushnumber(L, ItemSystem::IsValidItem(item) ? ItemSystem::GetItemType(item) : 0);
		return 1;
	}

	ALUA(item_get_sub_type)
	{
		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();
		lua_pushnumber(L, ItemSystem::IsValidItem(item) ? ItemSystem::GetItemSubType(item) : 0);
		return 1;
	}

	ALUA(item_get_refine_vnum)
	{
		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();
		lua_pushnumber(L, ItemSystem::IsValidItem(item) ? ItemSystem::GetItemRefineVnum(item) : 0);

		return 1;
	}

	ALUA(item_next_refine_vnum)
	{
		uint32_t vnum = 0;
		if (lua_isnumber(L, 1))
			vnum = (uint32_t) lua_tonumber(L, 1);

		TItemTable* pTable = ITEM_MANAGER::instance().GetTable(vnum);
		if (pTable)
		{
			lua_pushnumber(L, pTable->dwRefinedVnum);
		}
		else
		{
			sys_err("Cannot find item table of vnum {}", vnum);
			lua_pushnumber(L, 0);
		}
		return 1;
	}

	ALUA(item_get_level)
	{
		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();
		lua_pushnumber(L, ItemSystem::IsValidItem(item) ? ItemSystem::GetItemRefineLevel(item) : 0);

		return 1;
	}

	ALUA(item_can_over9refine)
	{
		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();
		if (!ItemSystem::IsValidItem(item))
			return 0;

		lua_pushnumber(L, COver9RefineManager::instance().canOver9Refine(ItemSystem::GetItemVnum(item)));
		return 1;
	}

	ALUA(item_change_to_over9)
	{
		const entt::entity character = CQuestManager::instance().GetCurrentPCEntity();
		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();
		if (character == entt::null || !g_registry.valid(character) || !ItemSystem::IsValidItem(item))
			return 0;

		lua_pushboolean(L, COver9RefineManager::instance().Change9ToOver9(character, item));
		return 1;
	}

	ALUA(item_over9refine)
	{
		const entt::entity character = CQuestManager::instance().GetCurrentPCEntity();
		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();
		if (character == entt::null || !g_registry.valid(character) || !ItemSystem::IsValidItem(item))
			return 0;

		lua_pushboolean(L, COver9RefineManager::instance().Over9Refine(character, item));
		return 1;
	}
	ALUA(item_get_over9_material_vnum)
	{
		if ( lua_isnumber(L, 1) == true )
		{
			lua_pushnumber(L, COver9RefineManager::instance().GetMaterialVnum((uint32_t)lua_tonumber(L, 1)));
			return 1;
		}

		return 0;
	}

	ALUA(item_get_level_limit)
	{
		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();

		if (ItemSystem::IsValidItem(item))
		{
			const uint8_t type = ItemSystem::GetItemType(item);
			if (type != ITEM_WEAPON && type != ITEM_ARMOR)
			{
				return 0;
			}
			lua_pushnumber(L, ItemSystem::GetItemLevelLimit(item));
			return 1;
		}
		return 0;
	}

#ifdef __NEWPET_SYSTEM__
	ALUA (item_pet_death)
	{
		CQuestManager& q = CQuestManager::instance();
		uint32_t itemid = ItemSystem::GetItemID(q.GetCurrentItemEntity());
		if (itemid != 0)
		{
			char szQuery1[1024];
			snprintf(szQuery1, sizeof(szQuery1), "SELECT duration FROM new_petsystem WHERE id = %u LIMIT 1", itemid);
			std::unique_ptr<SQLMsg> pmsg2(DBManager::instance().DirectQuery(szQuery1));
			if (pmsg2->Get()->uiNumRows > 0) {
				MYSQL_ROW row = mysql_fetch_row(pmsg2->Get()->pSQLResult);
				lua_pushboolean(L, atoi(row[0]) <= 0);
				return 0;
			}
			else{
				lua_pushboolean(L, false);
				sys_err("[NewPetSystem]Error no item founded!On item.pet.death");
				return 0;
			}
					
		}
		return 0;
	}

	ALUA (item_pet_revive)
	{
		CQuestManager& q = CQuestManager::instance();
		uint32_t itemid = ItemSystem::GetItemID(q.GetCurrentItemEntity());
		if (itemid != 0)
		{
			std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("UPDATE new_petsystem SET duration =(tduration/2) WHERE id = %d", itemid));
		}
		return 0;
	}
#endif

	ALUA(item_start_realtime_expire)
	{
		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();
		return ItemSystem::StartRealTimeExpireEventEcs(item) ? 1 : 0;
	}
	ALUA(item_copy_and_give_before_remove)
	{
		lua_pushboolean(L, 0);
		if (!lua_isnumber(L, 1))
			return 1;

		CQuestManager& q = CQuestManager::instance();
		const entt::entity source = q.GetCurrentItemEntity();
		const entt::entity character = q.GetCurrentPCEntity();
		if (character == entt::null || !g_registry.valid(character) || !ItemSystem::IsValidItem(source) ||
			ItemSystem::GetItemOwner(source) != character || !ItemSystem::IsItemInInventory(source))
			return 1;

		const uint16_t sourceCell = ItemSystem::GetItemCell(source);
		const entt::entity replacement = ItemSystem::CreateItemEcs(
			static_cast<uint32_t>(lua_tonumber(L, 1)), 1, 0, false);
		if (!ItemSystem::IsValidItem(replacement))
			return 1;

		if (!ItemSystem::CopyAllAttrToEcs(source, replacement))
		{
			ItemSystem::DestroyItemEntityEcs(replacement, "COPY_ATTR_ROLLBACK");
			return 1;
		}

		if (!ItemSystem::RemoveItemEcs(source))
		{
			ItemSystem::DestroyItemEntityEcs(replacement, "COPY_REMOVE_ROLLBACK");
			return 1;
		}

		if (!ItemSystem::PlaceItemEcs(character, replacement, INVENTORY, sourceCell))
		{
			ItemSystem::PlaceItemEcs(character, source, INVENTORY, sourceCell);
			ItemSystem::DestroyItemEntityEcs(replacement, "COPY_PLACE_ROLLBACK");
			return 1;
		}

		LogManager::instance().ItemLogEntity(
			character, replacement, "COPY SUCCESS", ItemSystem::GetItemName(replacement));

		if (!ItemSystem::DestroyItemEntityEcs(source, "REMOVE (COPY SUCCESS)"))
		{
			ItemSystem::RemoveItemEcs(replacement);
			ItemSystem::PlaceItemEcs(character, source, INVENTORY, sourceCell);
			ItemSystem::DestroyItemEntityEcs(replacement, "COPY_SOURCE_ROLLBACK");
			return 1;
		}

		ItemSystem::FlushDelayedSaveEcs(replacement);
		ItemSystem::AttrLogEcs(replacement);
		lua_pushboolean(L, 1);
		return 1;
	}
#ifdef ENABLE_NEWSTUFF
	ALUA(item_get_wearflag0)
	{
		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();
		lua_pushnumber(L, ItemSystem::IsValidItem(item) ? ItemSystem::GetItemWearFlags(item) : 0);

		return 1;
	}

	ALUA(item_has_wearflag0)
	{
		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();

		if (!lua_isnumber(L, 1))
		{
			sys_err("wearflag is not a number.");
			lua_pushboolean(L, 0);
			return 1;
		}

		if (ItemSystem::IsValidItem(item))
			lua_pushboolean(L, IS_SET(ItemSystem::GetItemWearFlags(item), (int32_t)lua_tonumber(L, 1)));
		else
			lua_pushboolean(L, false);

		return 1;
	}

	ALUA(item_get_antiflag0)
	{
		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();
		lua_pushnumber(L, ItemSystem::IsValidItem(item) ? ItemSystem::GetItemAntiFlags(item) : 0);

		return 1;
	}

	ALUA(item_has_antiflag0)
	{
		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();

		if (!lua_isnumber(L, 1))
		{
			sys_err("antiflag is not a number.");
			lua_pushboolean(L, false);
			return 1;
		}

		if (ItemSystem::IsValidItem(item))
			lua_pushboolean(L, IS_SET(ItemSystem::GetItemAntiFlags(item), static_cast<uint32_t>(lua_tonumber(L, 1))));
		else
			lua_pushboolean(L, false);

		return 1;
	}

	ALUA(item_get_immuneflag0)
	{
		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();
		lua_pushnumber(L, ItemSystem::IsValidItem(item) ? ItemSystem::GetItemImmuneFlags(item) : 0);

		return 1;
	}

	ALUA(item_has_immuneflag0)
	{
		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();

		if (!lua_isnumber(L, 1))
		{
			sys_err("immuneflag is not a number.");
			lua_pushboolean(L, false);
			return 1;
		}

		if (ItemSystem::IsValidItem(item))
			lua_pushboolean(L, IS_SET(ItemSystem::GetItemImmuneFlags(item), static_cast<uint32_t>(lua_tonumber(L, 1))));
		else
			lua_pushboolean(L, false);

		return 1;
	}


#define NS_ITEM_GETMODE0(x)	\
		int x = 0;\
		if(lua_isnumber(L, 1))\
			x = MINMAX(0, lua_tonumber(L, 1), 2);

	ALUA(item_add_attr0)
	{
		NS_ITEM_GETMODE0(m_mode);

		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();
		if (!ItemSystem::IsValidItem(item))
			return 0;

		int count = 0;
		int requested = 1;
		if (lua_isnumber(L, 2))
			requested = static_cast<int>(lua_tonumber(L, 2));

		if (m_mode == 1 || m_mode == 0)
		{
			count = ITEM_ATTRIBUTE_NORM_NUM - ItemSystem::GetItemAttributeCount(item);
			if (count > requested && requested != 0)
				count = requested;
			for (int i = 0; i < count; ++i)
				ItemSystem::AddItemAttributeEcs(item);
		}
		if (m_mode == 2 || m_mode == 0)
		{
			count = ITEM_ATTRIBUTE_RARE_NUM - ItemSystem::GetItemRareAttributeCount(item);
			if (count > requested && requested != 0)
				count = requested;
			for (int i = 0; i < count; ++i)
				ItemSystem::AddItemRareAttributeEcs(item);
		}
		return 0;
	}
	ALUA(item_change_attr0)
	{
		NS_ITEM_GETMODE0(m_mode);

		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();
		if (!ItemSystem::IsValidItem(item))
			return 0;

		if (m_mode == 0 || m_mode == 1)
			ItemSystem::ChangeItemAttributeEcs(item);
		if (m_mode == 0 || m_mode == 2)
			ItemSystem::ChangeItemRareAttributeEcs(item);
		return 0;
	}
	ALUA(item_clear_attr0)
	{
		NS_ITEM_GETMODE0(m_mode);

		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();
		if (!ItemSystem::IsValidItem(item))
			return 0;

		int m_start = 0;
		int m_end = ITEM_ATTRIBUTE_MAX_NUM;

		if (m_mode==1)
			m_end = ITEM_ATTRIBUTE_NORM_NUM;
		else if (m_mode==2)
			m_start = ITEM_ATTRIBUTE_NORM_NUM;

		for (int i=m_start; i<m_end; i++)
			ItemSystem::ClearItemAttribute(item, i);
		return 0;
	}

	ALUA(item_count_attr0)
	{
		NS_ITEM_GETMODE0(m_mode);

		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();
		if (ItemSystem::IsValidItem(item))
		{
			if (m_mode == 1)
				lua_pushnumber(L, ItemSystem::GetItemAttributeCount(item));
			else if (m_mode == 2)
				lua_pushnumber(L, ItemSystem::GetItemRareAttributeCount(item));
			else
			{
				lua_newtable(L);
				lua_pushnumber(L, ItemSystem::GetItemAttributeCount(item));
				lua_rawseti(L, -2, 1);
				lua_pushnumber(L, ItemSystem::GetItemRareAttributeCount(item));
				lua_rawseti(L, -2, 2);
			}
		}
		else
			lua_pushnumber(L, 0.0);

		return 1;
	}
	ALUA(item_get_attr0)
	{
		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();

		if (ItemSystem::IsValidItem(item))
		{
			// it returns a table like:
			// {id, value, id, value, id, value, id, value, id, value, id, value, id, value}
			// es. {1, 1000, 2, 500, 73, 15, 23, 20, 0, 0, 71, 15, 72, 15}
			lua_newtable(L);
			for (int i=0; i<ITEM_ATTRIBUTE_MAX_NUM; i++)
			{
				// push type
				lua_pushnumber(L, ItemSystem::GetItemAttributeType(item, i));
				lua_rawseti(L, -2, (i*2)+1);
				// push value
				lua_pushnumber(L, ItemSystem::GetItemAttributeValue(item, i));
				lua_rawseti(L, -2, (i*2)+2);
			}
		}
		else
			lua_pushnumber(L, 0.0);

		return 1;
	}

	ALUA(item_set_attr0)
	{
		if (!lua_istable(L, 1))
			return 0;

		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();
		if (!ItemSystem::IsValidItem(item))
			return 0;

		int m_attr[ITEM_ATTRIBUTE_MAX_NUM*2] = {0};
		int m_idx = 0;
		// start
		lua_pushnil(L);
		while (lua_next(L, 1) && m_idx<(ITEM_ATTRIBUTE_MAX_NUM*2))
		{
			m_attr[m_idx++] = lua_tonumber(L, -1);
			lua_pop(L, 1);
		}
		// end
		for (int i=0; i<ITEM_ATTRIBUTE_MAX_NUM; i++)
			ItemSystem::SetItemForceAttributeEcs(item, i, m_attr[(i*2)+0], m_attr[(i*2)+1]);
		return 0;
	}

	ALUA(item_set_count0)
	{
		if(!lua_isnumber(L, 1))
			return 0;

		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();

		if (ItemSystem::IsValidItem(item))
			ItemSystem::SetItemCountEcs(item, lua_tonumber(L, 1));
			//item->SetCount(MINMAX(1, lua_tonumber(L, 1), g_bItemCountLimit));

		return 0;
	}

	// ALUA(item_equip_to0)
	// {
		// CQuestManager& q = CQuestManager::instance();
		// entt::entity item = q.GetCurrentItemEntity();
		// LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();

		// lua_pushboolean((item && ch)?item->EquipTo(ch, lua_tonumber(L, 1)):false);

		// return 1;
	// }

	// ALUA(item_unequip0)
	// {
		// CQuestManager& q = CQuestManager::instance();
		// entt::entity item = q.GetCurrentItemEntity();

		// lua_pushboolean(L, (item)?item->Unequip():false);

		// return 1;
	// }

	ALUA(item_is_available0)
	{
		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();

		lua_pushboolean(L, ItemSystem::IsValidItem(item));
		return 1;
	}

#endif
	void RegisterITEMFunctionTable()
	{

		luaL_reg item_functions[] =
		{
			{ "get_id",		item_get_id		},
			{ "get_cell",		item_get_cell		},
			{ "select",		item_select		},
			{ "select_cell",	item_select_cell	},
			{ "remove",		item_remove		},
			{ "get_socket",		item_get_socket		},
			{ "set_socket",		item_set_socket		},
			{ "get_vnum",		item_get_vnum		},
			{ "has_flag",		item_has_flag		},
			{ "get_value",		item_get_value		},
			{ "set_value",		item_set_value		},
			{ "get_name",		item_get_name		},
			{ "get_size",		item_get_size		},
			{ "get_count",		item_get_count		},
			{ "get_type",		item_get_type		},
			{ "get_sub_type",	item_get_sub_type	},
			{ "get_refine_vnum",	item_get_refine_vnum	},
			{ "get_level",		item_get_level		},
			{ "next_refine_vnum",	item_next_refine_vnum	},
			{ "can_over9refine",	item_can_over9refine	},
			{ "change_to_over9",		item_change_to_over9	},
			{ "over9refine",		item_over9refine	},
			{ "get_over9_material_vnum",		item_get_over9_material_vnum	},
			{ "get_level_limit", 				item_get_level_limit },
			{ "start_realtime_expire", 			item_start_realtime_expire },
			{ "copy_and_give_before_remove",	item_copy_and_give_before_remove},
#ifdef __NEWPET_SYSTEM__
			{ "petdeath",						item_pet_death},
			{ "petrevive",						item_pet_revive},
#endif		
#ifdef ENABLE_NEWSTUFF
			{ "get_wearflag0",			item_get_wearflag0},	// [return lua number]
			{ "has_wearflag0",			item_has_wearflag0},	// [return lua boolean]
			{ "get_antiflag0",			item_get_antiflag0},	// [return lua number]
			{ "has_antiflag0",			item_has_antiflag0},	// [return lua boolean]
			{ "get_immuneflag0",		item_get_immuneflag0},	// [return lua number]
			{ "has_immuneflag0",		item_has_immuneflag0},	// [return lua boolean]
			// item.add_attr0(0|1|2[, cnt]) -- (0: baseeraro, 1: base, 2: raro)
			// item.add_attr0(0) -- add one 1-5 and one 6-7 bonus
			// item.add_attr0(0, 0) -- add all 1-7 bonuses
			// item.add_attr0(1|2) -- add one 1-5|6-7 bonus
			// item.add_attr0(1|2, 0) -- add all 1-5|6-7 bonuses
			// item.add_attr0(1|2, 4) -- add four 1-5|6-7 bonuses
			{ "add_attr0",			item_add_attr0},
			// item.change_attr0(0|1|2) -- (0: baseerari, 1: base, 2: rari)
			{ "change_attr0",		item_change_attr0},
			// item.clear_attr0(0|1|2) -- (0: baseerari, 1: base, 2: rari)
			{ "clear_attr0",		item_clear_attr0},
			// item.count_attr0(0|1|2) -- (0: [cnt(base), cnt(rari)], 1: cnt(base), 2: cnt(rari))
			{ "count_attr0",		item_count_attr0},
			// item.get_attr0() -- return a table containing all the item attrs {1,11,2,22,...,7,77}
			{ "get_attr0",			item_get_attr0},	// [return lua table]
			// item.set_attr0({1,11,2,22,...,7,77}) use a table to set the item attrs
			{ "set_attr0",			item_set_attr0},	// [return nothing]
			// item.set_count(count)
			{ "set_count0",			item_set_count0},	// [return nothing]
			// { "equip_to0",			item_equip_to0},	// [return lua boolean=successfulness]
			// { "unequip0",			item_unequip0},		// [return lua boolean=successfulness]
			{ "is_available0",		item_is_available0	},	// [return lua boolean]
#endif
			{nullptr, nullptr}
		};
		CQuestManager::instance().AddLuaFunctionTable("item", item_functions);
	}
}

