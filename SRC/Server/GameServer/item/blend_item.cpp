/*********************************************************************
 * date        : 2007.02.24
 * file        : blend_item.cpp
 * author      : mhh
 * description :
 */

#define _blend_item_cpp_

#include "stdafx.h"
#include "constants.h"
#include "log.h"
#include "dev_log.h"
#include <Core/Logging.hpp>
#include "locale_service.h"
#include "JsonFile.h"
#include "item.h"
#include "blend_item.h"

#include <nlohmann/json.hpp>
#include "../ecs/EntityFactory.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/systems/ItemSystem.hpp"

#define DO_ALL_BLEND_INFO(iter)	for ((iter)=s_blend_info.begin(); (iter)!=s_blend_info.end(); ++(iter))


struct BLEND_ITEM_INFO
{
	uint32_t	item_vnum;
	int		apply_type;
	int		apply_value[MAX_BLEND_ITEM_VALUE];
	int		apply_duration[MAX_BLEND_ITEM_VALUE];
};


typedef std::vector<BLEND_ITEM_INFO*>	T_BLEND_ITEM_INFO;
static T_BLEND_ITEM_INFO	s_blend_info;

bool	Blend_Item_init()
{
	T_BLEND_ITEM_INFO::iterator			iter;
	char	file_name[256];
	snprintf (file_name, sizeof(file_name), "%s/blend.json", LocaleService_GetBasePath().c_str());

	LOG_INFO("Blend_Item_init {} ", file_name);

	DO_ALL_BLEND_INFO(iter)
	{
		BLEND_ITEM_INFO* blend_item_info = *iter;
		delete blend_item_info;
	}
	s_blend_info.clear();

	if (false==Blend_Item_load(file_name))
	{
		LOG_ERROR("<Blend_Item_init> fail");
		return false;
	}
	return true;
}

bool	Blend_Item_load(char *file)
{
	if (nullptr == file || 0 == file[0])
		return false;

	nlohmann::json doc;

	if (!JsonFile::Load(file, doc))
		return false;

	if (!doc.contains("blends") || !doc.at("blends").is_array())
	{
		LOG_ERROR("Blend_Item_load: {} needs a 'blends' array", file);
		return false;
	}

	for (const auto& entry : doc.at("blends"))
	{
		BLEND_ITEM_INFO	*blend_item_info = new BLEND_ITEM_INFO;
		memset(blend_item_info, 0x00, sizeof(BLEND_ITEM_INFO));

		blend_item_info->item_vnum = entry.value("item_vnum", 0u);

		if (entry.contains("apply_type"))
			blend_item_info->apply_type = FN_get_apply_type(JsonFile::ToToken(entry.at("apply_type")).c_str());

		if (entry.contains("apply_value"))
		{
			const auto& values = entry.at("apply_value");

			if (!values.is_array() || values.size() < MAX_BLEND_ITEM_VALUE)
			{
				LOG_ERROR("Blend_Item_load: 'apply_value' needs {} values (item_vnum {})", MAX_BLEND_ITEM_VALUE, blend_item_info->item_vnum);
				delete blend_item_info;
				return false;
			}

			for (int i = 0; i < MAX_BLEND_ITEM_VALUE; ++i)
				str_to_number(blend_item_info->apply_value[i], JsonFile::ToToken(values.at(i)).c_str());
		}

		if (entry.contains("apply_duration"))
		{
			const auto& durations = entry.at("apply_duration");

			if (!durations.is_array() || durations.size() < MAX_BLEND_ITEM_VALUE)
			{
				LOG_ERROR("Blend_Item_load: 'apply_duration' needs {} values (item_vnum {})", MAX_BLEND_ITEM_VALUE, blend_item_info->item_vnum);
				delete blend_item_info;
				return false;
			}

			for (int i = 0; i < MAX_BLEND_ITEM_VALUE; ++i)
				str_to_number(blend_item_info->apply_duration[i], JsonFile::ToToken(durations.at(i)).c_str());
		}

		s_blend_info.push_back(blend_item_info);
	}

	return true;
}

static int FN_random_index()
{
	int	percent = number(1,100);

	if (percent<=10)			// level 1 :10%
		return 0;
	else if (percent<=30)		// level 2 : 20%
		return 1;
	else if (percent<=70)		// level 3 : 40%
		return 2;
	else if (percent<=90)		// level 4 : 20%
		return 3;
	else
		return 4;				// level 5 : 10%

	return 0;
}

// by rtsummit

static int FN_ECS_random_index()
{
	int	percent = number(1,100);

	if (percent<=5)			// level 1 : 5%
		return 0;
	else if (percent<=15)		// level 2 : 10%
		return 1;
	else if (percent<=60)		// level 3 : 45%
		return 2;
	else if (percent<=85)		// level 4 : 25%
		return 3;
	else
		return 4;				// level 5 : 15%

	return 0;
}


bool	Blend_Item_set_value(entt::entity itemEntity)
{
	if (!ItemSystem::IsValidItem(itemEntity) || !g_registry.all_of<ecs::ItemSockets>(itemEntity)) return false;
	BLEND_ITEM_INFO	*blend_info;
	T_BLEND_ITEM_INFO::iterator	iter;

	DO_ALL_BLEND_INFO(iter)
	{
		blend_info = *iter;
		if (blend_info->item_vnum == ItemSystem::GetItemVnum(itemEntity))
		{
			int	apply_type;
			int	apply_value;
			int	apply_duration;

			if (ItemSystem::GetItemVnum(itemEntity) == 51002)
			{
				apply_type		= blend_info->apply_type;
				apply_value		= blend_info->apply_value		[FN_ECS_random_index()];
				apply_duration	= blend_info->apply_duration	[FN_ECS_random_index()];
			}
			else
			{
				apply_type		= blend_info->apply_type;
				apply_value		= blend_info->apply_value		[FN_random_index()];
				apply_duration	= blend_info->apply_duration	[FN_random_index()];
			}
			LOG_INFO("blend_item : type : {}, value : {}, du : {}", apply_type, apply_value, apply_duration);
			// Creation-only setup: publish the complete value with the finished item.
			auto& sockets = g_registry.get<ecs::ItemSockets>(itemEntity).sockets;
			sockets[0] = apply_type;
			sockets[1] = apply_value;
			sockets[2] = apply_duration;
			return true;
		}

	}
	return false;
}

bool	Blend_Item_find(uint32_t item_vnum)
{
	BLEND_ITEM_INFO	*blend_info;
	T_BLEND_ITEM_INFO::iterator	iter;

	DO_ALL_BLEND_INFO(iter)
	{
		blend_info = *iter;
		if (blend_info->item_vnum == item_vnum)
			return true;
	}
	return false;
}

