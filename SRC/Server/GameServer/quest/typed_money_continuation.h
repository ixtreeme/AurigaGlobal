#pragma once
#include "../core/lua_incl.h"
namespace quest {
inline int ResumeNativeQuestMoney(lua_State* thread,int,lua_KContext){
    if(lua_gettop(thread)<2||!lua_toboolean(thread,1))return luaL_error(thread,"Native quest money transaction refused; quest stopped before following rewards");
    lua_settop(thread,0);return 0;
}
}
