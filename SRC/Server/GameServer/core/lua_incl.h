#if !defined(_MSC_VER) && defined(__cplusplus)
extern "C" {
#endif


#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>


// #ifdef ENABLE_NEWSTUFF
#ifndef lua_String
#define lua_String const char*
#endif

#ifndef ALUA
#define ALUA(name) int name(lua_State* L)
#endif
// #endif

#if !defined(_MSC_VER) && defined(__cplusplus)
}
#endif
