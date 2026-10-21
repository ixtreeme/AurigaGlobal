#include "stdafx.h"
#include "questlua.h"
#include "questmanager.h"
#include <common/event_flag.h>

namespace quest {
	ALUA(game_set_event_flag)
	{
		CQuestManager & q = CQuestManager::instance();

		int64_t value = 0;
		bool valid = false;
		if (lua_type(L, 2) == LUA_TSTRING) {
			const size_t length = lua_strlen(L, 2);
			const char* text = lua_tostring(L, 2);
			valid = EventFlag::ParseValue(std::string_view(text, length), value);
		} else if (lua_isinteger(L, 2)) {
			value = lua_tointeger(L, 2); valid = true;
		} else if (lua_isnumber(L, 2)) {
			valid = EventFlag::FromNumber(lua_tonumber(L, 2), value);
		}
		if (lua_isstring(L, 1) && valid) {
			const size_t length = lua_strlen(L, 1);
			const char* name = lua_tostring(L, 1);
			q.RequestSetEventFlag(std::string(name, length), value);
		}

		return 0;
	}

	ALUA(game_get_event_flag)
	{
		CQuestManager& q = CQuestManager::instance();

		if (lua_isstring(L,1))
			lua_pushinteger(L, q.GetEventFlag(lua_tostring(L,1)));
		else
			lua_pushnumber(L, 0);

		return 1;
	}

	// Exact decimal result for values outside the bundled Lua's number precision.
	ALUA(game_get_event_flag_string)
	{
		const int64_t value = lua_isstring(L, 1)
			? CQuestManager::instance().GetEventFlag(lua_tostring(L, 1)) : 0;
		const auto text = std::to_string(value);
		lua_pushlstring(L, text.data(), text.size());
		return 1;
	}
} // namespace quest
