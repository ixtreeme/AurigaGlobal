file(GLOB_RECURSE TR_HEADER CONFIGURE_DEPENDS "*.h")
file(GLOB_RECURSE TR_SOURCE CONFIGURE_DEPENDS "*.cpp")

add_library(Terrain STATIC ${TR_HEADER} ${TR_SOURCE})

target_link_libraries(Terrain
	PUBLIC granny2
	PUBLIC AudioLib
	PUBLIC SpeedTreeRT
	PUBLIC FoxFS
)