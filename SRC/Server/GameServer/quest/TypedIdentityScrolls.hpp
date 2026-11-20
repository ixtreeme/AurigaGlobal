#pragma once

struct lua_State;

namespace quest
{
int pc_typed_change_name(lua_State*);
int pc_typed_change_empire(lua_State*);
int pc_typed_change_sex(lua_State*);
int pc_typed_change_guild_master(lua_State*);
}
