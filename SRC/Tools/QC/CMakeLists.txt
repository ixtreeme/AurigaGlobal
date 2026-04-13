file(GLOB_RECURSE QC_SOURCE CONFIGURE_DEPENDS "*.cpp")

add_executable(QC ${QC_SOURCE})

target_link_libraries(QC
	PUBLIC Base
	PUBLIC LUA

)