#ifndef __HEADER_QUEST_LUA__
#define __HEADER_QUEST_LUA__

#include "quest.h"
#include "buffer_manager.h"

extern int test_server;

namespace quest
{
	extern void RegisterPCFunctionTable();
	extern void RegisterNPCFunctionTable();
	extern void RegisterTargetFunctionTable();
	extern void RegisterAffectFunctionTable();
	extern void RegisterBuildingFunctionTable();
	extern void RegisterMarriageFunctionTable();
	extern void RegisterITEMFunctionTable();
	extern void RegisterDungeonFunctionTable();
	extern void RegisterQuestFunctionTable();
	extern void RegisterPartyFunctionTable();
	extern void RegisterHorseFunctionTable();
	extern void RegisterPetFunctionTable();
#ifdef __NEWPET_SYSTEM__
	extern void RegisterNewPetFunctionTable();
#endif	
	extern void RegisterGuildFunctionTable();
	extern void RegisterGameFunctionTable();
	extern void RegisterArenaFunctionTable();
	extern void RegisterGlobalFunctionTable(lua_State* L);
	extern void RegisterOXEventFunctionTable();
	extern void RegisterBattleArenaFunctionTable();
	extern void RegisterDanceEventFunctionTable();
	extern void RegisterDragonSoulFunctionTable();

	extern void combine_lua_string(lua_State* L, std::ostringstream &s);

	struct FSetWarpLocation
	{
		int32_t map_index;
		int32_t x;
		int32_t y;

		FSetWarpLocation (int32_t _map_index, int32_t _x, int32_t _y) :
			map_index (_map_index), x (_x), y (_y)
		{}
		void operator () (LPCHARACTER ch) const;
	};

	struct FSetQuestFlag
	{
		std::string flagname;
		int value;

		void operator () (LPCHARACTER ch) const;
	};

	struct FPartyCheckFlagLt
	{
		std::string flagname;
		int value;

		bool operator () (LPCHARACTER ch) const;
	};

	struct FPartyChat
	{
		int iChatType;
		const char* str;

		FPartyChat(int ChatType, const char* str);
		void operator() (LPCHARACTER ch) const;
	};

	struct FPartyClearReady
	{
		void operator() (LPCHARACTER ch) const;
	};

	struct FSendPacket
	{
		TEMP_BUFFER buf;

		void operator() (LPENTITY ent);
	};

	struct FSendPacketToEmpire
	{
		TEMP_BUFFER buf;
		uint8_t bEmpire;

		void operator() (LPENTITY ent);
	};

	struct FWarpEmpire
	{
		uint8_t m_bEmpire;
		int32_t m_lMapIndexTo;
		int32_t m_x;
		int32_t m_y;

		void operator() (LPENTITY ent) const;
	};

	EVENTINFO(warp_all_to_map_my_empire_event_info)
	{
		uint8_t 	m_bEmpire;
		int32_t	m_lMapIndexFrom;
		int32_t 	m_lMapIndexTo;
		int32_t 	m_x;
		int32_t	m_y;

		warp_all_to_map_my_empire_event_info()
		: m_bEmpire( 0 )
		, m_lMapIndexFrom( 0 )
		, m_lMapIndexTo( 0 )
		, m_x( 0 )
		, m_y( 0 )
		{
		}
	};

	EVENTFUNC(warp_all_to_map_my_empire_event);

	struct FBuildLuaGuildWarList
	{
		lua_State * L;
		int m_count;

		FBuildLuaGuildWarList(lua_State * L);
		void operator() (uint32_t g1, uint32_t g2);
	};
}
#endif /*__HEADER_QUEST_LUA__*/

