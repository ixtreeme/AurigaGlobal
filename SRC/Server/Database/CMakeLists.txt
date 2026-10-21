auriga_add_server_executable(Database)

if(ENABLE_ASAN AND EXISTS "${AURIGA_ASAN_RUNTIME_DLL}")
    add_custom_command(TARGET Database POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${AURIGA_ASAN_RUNTIME_DLL}"
            "$<TARGET_FILE_DIR:Database>"
        COMMENT "Copying MSVC AddressSanitizer runtime"
    )
endif()
