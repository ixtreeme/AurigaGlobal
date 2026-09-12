#ifndef __INC_METIN_II_GAME_ITEM_H__
#define __INC_METIN_II_GAME_ITEM_H__

#include "event.h"
#include <common/length.h>
#include <entt/entity/entity.hpp>
#include <cstring>

// Item events retain only versioned entity handles.
EVENTINFO(item_event_info)
{
	entt::entity item;
	char szOwnerName[CHARACTER_NAME_MAX_LEN];

	item_event_info()
	: item(entt::null)
	{
		::memset( szOwnerName, 0, CHARACTER_NAME_MAX_LEN );
	}
};

EVENTINFO(item_vid_event_info)
{
	entt::entity item;
#ifdef ENABLE_NEW_USE_POTION
	bool newpotion;
#endif

	item_vid_event_info()
	: item(entt::null)
#ifdef ENABLE_NEW_USE_POTION
	, newpotion(false)
#endif
	{
	}
};

#endif
