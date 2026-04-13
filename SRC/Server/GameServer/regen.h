#pragma once
#include "dungeon.h"
#include <common/service.h>

enum
{
	REGEN_TYPE_MOB,
	REGEN_TYPE_GROUP,
	REGEN_TYPE_EXCEPTION,
	REGEN_TYPE_GROUP_GROUP,
	REGEN_TYPE_ANYWHERE,
	REGEN_TYPE_MAX_NUM
};

typedef struct regen
{
	LPREGEN	prev, next;
	int32_t	lMapIndex;
	int		type;
	int		sx, sy, ex, ey;
	uint8_t	z_section;

	uint8_t	direction;

	uint32_t	time;

	int		max_count;
	int		count;
	int 	vnum;

	bool	is_aggressive;

	LPEVENT	event;

	uint64_t id; // to help dungeon regen identification

	regen() :
		prev(nullptr), next(nullptr),
		lMapIndex(0),
		type(0),
		sx(0), sy(0), ex(0), ey(0),
		z_section(0),
		direction(0),
		time(0),
		max_count(0),
		count(0),
		vnum(0),
		is_aggressive(0),
		event(nullptr),
		id(0)
	{}
} REGEN;

EVENTINFO(regen_event_info)
{
	LPREGEN 	regen;

	regen_event_info()
	: regen( nullptr )
	{
	}
};

typedef regen_event_info REGEN_EVENT_INFO;

typedef struct regen_exception
{
	LPREGEN_EXCEPTION prev, next;

	int		sx, sy, ex, ey;
	uint8_t	z_section;
} REGEN_EXCEPTION;

class CDungeon;

EVENTINFO(dungeon_regen_event_info)
{
	LPREGEN 	regen;
	CDungeon::IdType dungeon_id;

	dungeon_regen_event_info()
	: regen( nullptr )
	, dungeon_id( 0 )
	{
	}
};

#ifdef ENABLE_ATLAS_BOSS
extern bool	regen_load(const char *filename, int32_t lMapIndex, int base_x, int base_y, bool bossFile = false);
#else
extern bool	regen_load(const char *filename, int32_t lMapIndex, int base_x, int base_y);
#endif
extern bool	regen_do(const char* filename, int32_t lMapIndex, int base_x, int base_y, LPDUNGEON pDungeon, bool bOnce = true );
extern bool	regen_load_in_file(const char* filename, int32_t lMapIndex, int base_x, int base_y );
extern void	regen_free();

extern bool	is_regen_exception(int32_t x, int32_t y);
extern void	regen_reset(int x, int y);
