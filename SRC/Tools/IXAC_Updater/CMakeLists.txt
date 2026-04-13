file(GLOB_RECURSE IXACUP_SOURCE CONFIGURE_DEPENDS "*.cpp")

add_executable(IXAC_Updater ${IXACUP_SOURCE})

set_target_properties(IXAC_Updater PROPERTIES FOLDER IXAC)