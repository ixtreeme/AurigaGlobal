cmake_minimum_required(VERSION 3.15)

project(
    LUA
    VERSION 5.0.1
    LANGUAGES C CXX)


set(LUA_SRC
	src/lapi.c
	src/lcode.c
	src/ldebug.c
	src/ldo.c
	src/ldump.c
	src/lfunc.c
	src/lgc.c
	src/llex.c
	src/lmem.c
	src/lobject.c
	src/lopcodes.c
	src/lparser.c
	src/lstate.c
	src/lstring.c
	src/ltable.c
	src/ltests.c
	src/ltm.c
	src/lundump.c
	src/lvm.c
	src/lzio.c
	src/lib/lauxlib.c
	src/lib/lbaselib.c
	src/lib/ldblib.c
	src/lib/liolib.c
	src/lib/lmathlib.c
	src/lib/ltablib.c
	src/lib/lstrlib.c
	src/lib/loadlib.c
)

set(${PROJECT_NAME}
					include/lualib.h
					include/lua.h
					include/lauxlib.h
)

add_library(${PROJECT_NAME} STATIC ${LUA_SRC})
add_library(lua::lua ALIAS ${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION       ${${PROJECT_NAME}_VERSION}
                                                        PUBLIC_HEADER "${PROJECT_NAME}")

set_target_properties(${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX "d")
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "lua")

if(MSVC)
  # The Lua sources keep their historical .c extension, but we need Visual Studio
  # to treat them as C++ so that the generated project files pass /TP.
  set_source_files_properties(${LUA_SRC} PROPERTIES LANGUAGE CXX)
  target_compile_options(${PROJECT_NAME} PRIVATE /TP)
endif()

target_include_directories(${PROJECT_NAME}
    PUBLIC 
      $<INSTALL_INTERFACE:include>    
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/src
      ${CMAKE_CURRENT_SOURCE_DIR}/src/lib
)


#set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER Thirdparty)
