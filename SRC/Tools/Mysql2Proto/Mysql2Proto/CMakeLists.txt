file(GLOB_RECURSE M2P_HEADER CONFIGURE_DEPENDS "*.h")
file(GLOB_RECURSE M2P_SOURCE CONFIGURE_DEPENDS "*.cpp")

add_executable(Mysql2Proto ${M2P_HEADER} ${M2P_SOURCE})

target_link_libraries(Mysql2Proto
	PUBLIC Base
	PUBLIC Game

)