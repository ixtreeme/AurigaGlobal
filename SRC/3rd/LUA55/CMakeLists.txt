# Official Lua 5.5.1, SHA256 recorded in UPSTREAM.md. C++ compilation gives
# Lua errors exception unwinding across the server's C++ API implementations.
file(GLOB LUA55_SOURCES CONFIGURE_DEPENDS "src/*.c")
list(REMOVE_ITEM LUA55_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/lua.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/luac.c" "${CMAKE_CURRENT_SOURCE_DIR}/src/onelua.c")
set_source_files_properties(${LUA55_SOURCES} PROPERTIES LANGUAGE CXX)
add_library(LUA STATIC ${LUA55_SOURCES})
add_library(lua::lua ALIAS LUA)
target_include_directories(LUA PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
set_target_properties(LUA PROPERTIES OUTPUT_NAME lua55)
if(UNIX)
    target_compile_definitions(LUA PRIVATE LUA_USE_POSIX LUA_USE_DLOPEN)
    target_link_libraries(LUA PUBLIC ${CMAKE_DL_LIBS} m)
endif()
