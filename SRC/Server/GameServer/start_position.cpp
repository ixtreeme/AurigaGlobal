#include "stdafx.h"
#include "start_position.h"


#include <common/CommonDefines.h>


char g_nation_name[4][32] =
{
	"",
	"Shinsoo",
	"Chunjo",
	"Jinno",
};

#ifdef __ENABLE_CAPITALE_MAP__
UINT g_start_map[4] =
{
	0,	// reserved
	1,	// 신수국
	21,	// 천조국
	41	// 진노국
};

uint32_t g_start_position[4][2] =
{
	{      0,      0 },
	{984298, 265283 },	// piros
	{984298, 265283 },	// sarga
	{984298, 265283 }	// kek
};

uint32_t arena_return_position[4][2] =
{
{      0,      0 },
	{ 984157, 265025 },
	{ 984157, 265025 },//map1 varos
	{ 984157, 265025 }
};




uint32_t g_create_position[4][2] =
{
	{		0,		0 },
	{984298, 265283 },	// piros
	{984298, 265283 },	// sarga
	{984298, 265283 }	// kek
};

uint32_t g_create_position_canada[4][2] =
{
	{		0,		0 },
	{984298, 265283 },	// piros
	{984298, 265283 },	// sarga
	{984298, 265283 }	// kek
};
#else
int32_t g_start_map[4] =
{
	0,	// reserved
	1,	// 신수국
	21,	// 천조국
	41	// 진노국
};

uint32_t g_start_position[4][2] =
{
	{      0,      0 },	// reserved
	{984298, 265283 },	// piros
	{984298, 265283 },	// sarga
	{984298, 265283 }	// kek
};

uint32_t arena_return_position[4][2] =
{
	{      0,      0 },	// reserved
	{984298, 265283 },	// piros
	{984298, 265283 },	// sarga
	{984298, 265283 }	// kek
};


uint32_t g_create_position[4][2] =
{
	{      0,      0 },	// reserved
	{984298, 265283 },	// piros
	{984298, 265283 },	// sarga
	{984298, 265283 }	// kek
};

uint32_t g_create_position_canada[4][2] =
{
	{      0,      0 },	// reserved
	{984298, 265283 },	// piros
	{984298, 265283 },	// sarga
	{984298, 265283 }	// kek
};
#endif

