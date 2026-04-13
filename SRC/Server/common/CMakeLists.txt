file(GLOB_RECURSE COMMON_HEADER CONFIGURE_DEPENDS "*.h")

add_library(COMMON INTERFACE)
target_include_directories(COMMON INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_sources(COMMON INTERFACE INTERFACE ${COMMON_HEADER})
set_target_properties(COMMON PROPERTIES FOLDER Common)

# Make sure the common headers appear in IDEs such as Visual Studio using the
# on-disk folder layout. Some Visual Studio generators hide INTERFACE targets
# without sources, so also create a custom target that explicitly lists the
# headers to ensure the "common" folder is visible in the solution explorer.
if(COMMON_HEADER)
    add_custom_target(common_headers SOURCES ${COMMON_HEADER})
    set_target_properties(common_headers PROPERTIES FOLDER Common)
    source_group(TREE ${CMAKE_CURRENT_LIST_DIR} FILES ${COMMON_HEADER})
endif()