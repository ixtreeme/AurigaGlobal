set(AURIGA_SERVER_PLATFORM_LIBS
    $<$<PLATFORM_ID:Windows>:ws2_32>
)

function(auriga_collect_server_sources out_var)
    file(GLOB_RECURSE _headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/*.h")
    file(GLOB_RECURSE _headerspp CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/*.hpp")
    file(GLOB_RECURSE _sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/*.cpp")
    set(${out_var} ${_headers} ${_headerspp} ${_sources} PARENT_SCOPE)
endfunction()

function(auriga_add_server_library target)
    auriga_collect_server_sources(_module_sources)
    add_library(${target} STATIC ${_module_sources})
    target_link_libraries(${target} PUBLIC SERVER)
    set_target_properties(${target} PROPERTIES FOLDER Server)
	source_group(TREE ${CMAKE_CURRENT_LIST_DIR} FILES ${_module_sources})
endfunction()

function(auriga_add_server_executable target)
    cmake_parse_arguments(ARG "" "" "LIBS" ${ARGN})
    auriga_collect_server_sources(_module_sources)
    add_executable(${target} ${_module_sources})
    target_link_libraries(${target}
        PRIVATE
            S_Base
            S_Core
            S_Poly
            S_SQL
            SERVER
            ${ARG_LIBS}
            ${AURIGA_SERVER_PLATFORM_LIBS}
    )
    set_target_properties(${target} PROPERTIES FOLDER Server)
	source_group(TREE ${CMAKE_CURRENT_LIST_DIR} FILES ${_module_sources})
endfunction()

# fmt and spdlog come from vcpkg on Windows and from the system packages
# (devel/libfmt, devel/spdlog) on FreeBSD. Both are used header-only, as the
# vendored copies were. The servers link the static CRT (/MT); under the
# dynamic x64-windows triplet vcpkg's fmt and spdlog are DLLs on the dynamic
# CRT, and a string allocated in fmt.dll would be freed on another heap.
# spdlog's own header-only target links the compiled fmt, so only its include
# directory is taken.
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
get_target_property(AURIGA_SPDLOG_INCLUDE_DIRS spdlog::spdlog_header_only INTERFACE_INCLUDE_DIRECTORIES)

add_library(SERVER INTERFACE)
target_include_directories(SERVER INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/GameServer
    ${CMAKE_CURRENT_LIST_DIR}/GameServer/core
    ${CMAKE_CURRENT_LIST_DIR}/GameServer/network
    ${CMAKE_CURRENT_LIST_DIR}/GameServer/entity
    ${CMAKE_CURRENT_LIST_DIR}/GameServer/world
    ${CMAKE_CURRENT_LIST_DIR}/GameServer/combat
    ${CMAKE_CURRENT_LIST_DIR}/GameServer/item
    ${CMAKE_CURRENT_LIST_DIR}/GameServer/quest
    ${CMAKE_CURRENT_LIST_DIR}/GameServer/guild
    ${CMAKE_CURRENT_LIST_DIR}/GameServer/pet
    ${CMAKE_CURRENT_LIST_DIR}/GameServer/social
    ${CMAKE_CURRENT_LIST_DIR}/GameServer/dungeon
    ${CMAKE_CURRENT_LIST_DIR}/GameServer/command
)
target_include_directories(SERVER SYSTEM INTERFACE ${AURIGA_SPDLOG_INCLUDE_DIRS})
target_link_libraries(SERVER INTERFACE fmt::fmt-header-only)
target_compile_definitions(SERVER INTERFACE
    SPDLOG_FMT_EXTERNAL=1
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE
)
if(TARGET auriga_sanitizers)
    target_link_libraries(SERVER INTERFACE auriga_sanitizers)
endif()

# System packages (FreeBSD/Linux) discovered by cmake/AurigaSystemDeps.cmake.
# vcpkg builds keep using the MSBuild integration, so this is a no-op there.
if(TARGET auriga_system_deps)
    target_link_libraries(SERVER INTERFACE auriga_system_deps)
endif()


add_subdirectory(Base)
add_subdirectory(Core)
add_subdirectory(Database)
add_subdirectory(GameServer)
add_subdirectory(LoginServer)
add_subdirectory(Poly)
add_subdirectory(SQL)
add_subdirectory(common)
