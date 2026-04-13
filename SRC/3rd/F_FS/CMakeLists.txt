message("Adding F_FS header-only lib")
add_library(F_FS INTERFACE)

target_include_directories(F_FS INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)