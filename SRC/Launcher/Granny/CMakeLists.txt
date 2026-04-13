file(GLOB_RECURSE GR_HEADER CONFIGURE_DEPENDS "*.h")
file(GLOB_RECURSE GR_SOURCE CONFIGURE_DEPENDS "*.cpp")

add_library(Granny STATIC ${GR_HEADER} ${GR_SOURCE})

target_link_libraries(Granny
	PUBLIC F_FS
	PUBLIC granny2
)