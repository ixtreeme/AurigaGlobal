file(GLOB_RECURSE IXAC_HEADER CONFIGURE_DEPENDS "*.h")
file(GLOB_RECURSE IXAC_SOURCE CONFIGURE_DEPENDS "*.cpp")

add_library(IXAC SHARED ${IXAC_HEADER} ${IXAC_SOURCE} IXAC_Resource.rc)

target_link_libraries(IXAC

	PUBLIC SecureLayer
	PUBLIC psapi
	# The static libcurl (IXAC_Net) needs Winsock and SSPI.
	PRIVATE ws2_32
	PRIVATE secur32
)

set_target_properties(IXAC PROPERTIES FOLDER IXAC)