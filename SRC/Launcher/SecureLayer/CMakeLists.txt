file(GLOB_RECURSE SL_HEADER CONFIGURE_DEPENDS "*.h")
file(GLOB_RECURSE SL_SOURCE CONFIGURE_DEPENDS "*.cpp")

add_library(SecureLayer STATIC ${SL_HEADER} ${SL_SOURCE})

target_link_libraries(SecureLayer
	PUBLIC SpeedTreeRT
	PUBLIC FoxFS
)