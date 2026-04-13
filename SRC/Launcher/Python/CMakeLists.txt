file(GLOB_RECURSE PY_HEADER CONFIGURE_DEPENDS "*.h")
file(GLOB_RECURSE PY_SOURCE CONFIGURE_DEPENDS "*.cpp")

add_library(Python STATIC ${PY_HEADER} ${PY_SOURCE})

target_link_libraries(Python
	PUBLIC granny2
	PUBLIC AudioLib
	PUBLIC SpeedTreeRT
	PUBLIC FoxFS
)