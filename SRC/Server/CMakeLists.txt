set(AURIGA_SERVER_PLATFORM_LIBS
    $<$<PLATFORM_ID:Windows>:ws2_32>
    $<$<NOT:$<PLATFORM_ID:Windows>>:md>
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

add_library(SERVER INTERFACE)
target_include_directories(SERVER INTERFACE ${CMAKE_CURRENT_LIST_DIR})
if(TARGET auriga_sanitizers)
    target_link_libraries(SERVER INTERFACE auriga_sanitizers)
endif()


add_subdirectory(Base)
add_subdirectory(Core)
add_subdirectory(Database)
add_subdirectory(GameServer)
add_subdirectory(Poly)
add_subdirectory(SQL)
add_subdirectory(common)
