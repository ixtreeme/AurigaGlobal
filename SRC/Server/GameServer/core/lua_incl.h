#pragma once
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>


// #ifdef ENABLE_NEWSTUFF
#ifndef lua_String
#define lua_String const char*
#endif

#ifndef ALUA
#define ALUA(name) int name(lua_State* L)
#endif
// #endif

// Transitional source compatibility for game APIs. The runtime and coroutine
// lifecycle use the modern API explicitly; no legacy VM is linked to servers.
using luaL_reg = luaL_Reg;
inline lua_State* lua_open() { return luaL_newstate(); }
inline int luaL_getn(lua_State* L, int index) { return static_cast<int>(lua_rawlen(L, index)); }
inline int lua_ref(lua_State* L, int) { return luaL_ref(L, LUA_REGISTRYINDEX); }
inline void lua_unref(lua_State* L, int ref) { luaL_unref(L, LUA_REGISTRYINDEX, ref); }
// Each event/condition gets its own budget, including after a dialog resumes.
// The hook follows nested Lua calls and C++ error unwinding restores its state.
class LuaExecutionBudget {
    inline static char registryKey;
    lua_State* state;
    lua_Hook previousHook;
    int previousMask, previousCount;
    void* previousData;
    int remaining = 5000;
    static void Count(lua_State* L, lua_Debug*) {
        lua_rawgetp(L, LUA_REGISTRYINDEX, &registryKey);
        auto* remaining = static_cast<int*>(lua_touserdata(L, -1));
        lua_pop(L, 1);
        if (remaining && --*remaining <= 0) luaL_error(L, "quest instruction budget exceeded");
    }
public:
    explicit LuaExecutionBudget(lua_State* L) : state(L), previousHook(lua_gethook(L)),
        previousMask(lua_gethookmask(L)), previousCount(lua_gethookcount(L)) {
        lua_rawgetp(L, LUA_REGISTRYINDEX, &registryKey);
        previousData = lua_touserdata(L, -1);
        lua_pop(L, 1);
        lua_pushlightuserdata(L, &remaining);
        lua_rawsetp(L, LUA_REGISTRYINDEX, &registryKey);
        lua_sethook(L, Count, LUA_MASKCOUNT, 1000);
    }
    ~LuaExecutionBudget() {
        if (!state) return;
        lua_sethook(state, previousHook, previousMask, previousCount);
        lua_pushlightuserdata(state, previousData);
        lua_rawsetp(state, LUA_REGISTRYINDEX, &registryKey);
    }
    LuaExecutionBudget(const LuaExecutionBudget&) = delete;
    LuaExecutionBudget& operator=(const LuaExecutionBudget&) = delete;
    void Detach() { state = nullptr; }
};
inline int lua_dobuffer(lua_State* L, const char* text, size_t size, const char* name) {
    LuaExecutionBudget budget(L);
    const int code = luaL_loadbufferx(L, text, size, name, "t");
    return code ? code : lua_pcall(L, 0, LUA_MULTRET, 0);
}
inline int lua_dofile(lua_State* L, const char* name) {
    LuaExecutionBudget budget(L);
    int code;
    lua_getfield(L, LUA_REGISTRYINDEX, "auriga.frozen_loadfile");
    if (lua_isfunction(L, -1)) {
        lua_pushstring(L, name);
        code = lua_pcall(L, 1, 2, 0);
        if (code) return code;
        if (!lua_isfunction(L, -2)) { lua_remove(L, -2); return LUA_ERRFILE; }
        lua_pop(L, 1);
    } else { lua_pop(L, 1); code = luaL_loadfilex(L, name, "t"); }
    return code ? code : lua_pcall(L, 0, LUA_MULTRET, 0);
}
inline bool LuaOpenQuestLibraries(lua_State* L) {
    const luaL_Reg libraries[] = {
        {"_G", luaopen_base}, {LUA_TABLIBNAME, luaopen_table},
        {LUA_STRLIBNAME, luaopen_string}, {LUA_MATHLIBNAME, luaopen_math},
        {LUA_UTF8LIBNAME, luaopen_utf8}, {LUA_COLIBNAME, luaopen_coroutine},
        {LUA_OSLIBNAME, luaopen_os}, {nullptr, nullptr}
    };
    for (const auto* library = libraries; library->name; ++library) {
        luaL_requiref(L, library->name, library->func, 1);
        lua_pop(L, 1);
    }
    const char* compatibility = R"lua(
        table.getn = function(t) return #t end
        table.foreach = function(t, f) for k,v in pairs(t) do local r=f(k,v); if r~=nil then return r end end end
        table.foreachi = function(t, f) for i=1,#t do local r=f(i,t[i]); if r~=nil then return r end end end
        math.mod = math.fmod
        string.gfind = string.gmatch
        unpack = table.unpack
        local compile, read = load, loadfile
        load = function(s, n, mode, env)
            if mode and mode~="t" then return nil, "only text Lua is allowed" end
            return compile(s, n, "t", env or _G)
        end
        loadfile = function(file, mode, env)
            if type(file)~="string" then return nil, "a Lua file path is required" end
            if mode and mode~="t" then return nil, "only text Lua is allowed" end
            return read(file, "t", env or _G)
        end
        dofile = function(file) return assert(loadfile(file, "t", _G))() end
        loadstring = function(s, n) return load(s, n, "t", _G) end
        os.execute=nil; os.exit=nil; os.remove=nil; os.rename=nil; os.tmpname=nil; os.getenv=nil
    )lua";
    return lua_dobuffer(L, compatibility, std::char_traits<char>::length(compatibility), "@lua50_compat") == LUA_OK;
}
inline int64_t LuaIntegerValue(lua_State* L, int index) {
    if (lua_isinteger(L, index)) return lua_tointeger(L, index);
    const double value = std::nearbyint(luaL_checknumber(L, index));
    if (!std::isfinite(value) || value < -9223372036854775808.0 || value >= 9223372036854775808.0)
        luaL_error(L, "integer outside signed 64-bit range");
    return static_cast<int64_t>(value);
}
