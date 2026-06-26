add_subdirectory(Base)
add_subdirectory(Core)
add_subdirectory(Database)
add_subdirectory(GameServer)
add_subdirectory(Poly)
add_subdirectory(SQL)
add_subdirectory(common)



set_target_properties(S_Base PROPERTIES FOLDER Server)
set_target_properties(S_Core PROPERTIES FOLDER Server)
set_target_properties(Database PROPERTIES FOLDER Server)
set_target_properties(GameServer PROPERTIES FOLDER Server)
set_target_properties(S_Poly PROPERTIES FOLDER Server)
set_target_properties(S_SQL PROPERTIES FOLDER Server)



add_library(SERVER INTERFACE)
#file(GLOB_RECURSE SERVER_HEADER CONFIGURE_DEPENDS "*.h")
target_include_directories(SERVER INTERFACE ${CMAKE_CURRENT_LIST_DIR})