file(GLOB_RECURSE BASE_HEADER CONFIGURE_DEPENDS "*.h")
file(GLOB_RECURSE BASE_SOURCE CONFIGURE_DEPENDS "*.cpp")

add_library(Base STATIC ${BASE_HEADER} ${BASE_SOURCE})

# Exercise the production timer with a controlled platform clock, without
# building the graphical client or relying on wall-clock delays.
add_executable(TimerTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/client/TimerTests.cpp)
set_target_properties(TimerTests PROPERTIES FOLDER Tests)
add_test(NAME client_timer COMMAND TimerTests)
if(ENABLE_ASAN AND EXISTS "${AURIGA_ASAN_RUNTIME_DLL}")
    add_custom_command(TARGET TimerTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${AURIGA_ASAN_RUNTIME_DLL}" "$<TARGET_FILE_DIR:TimerTests>"
        COMMENT "Copying MSVC AddressSanitizer runtime")
endif()
