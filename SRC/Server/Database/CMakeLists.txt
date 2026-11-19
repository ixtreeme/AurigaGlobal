auriga_add_server_executable(Database)
get_target_property(_database_sources Database SOURCES)
list(FILTER _database_sources EXCLUDE REGEX "/ItemAward(Delivery|Scan)\\.cpp$")
set_property(TARGET Database PROPERTY SOURCES ${_database_sources})

if(ENABLE_ASAN AND EXISTS "${AURIGA_ASAN_RUNTIME_DLL}")
    add_custom_command(TARGET Database POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${AURIGA_ASAN_RUNTIME_DLL}"
            "$<TARGET_FILE_DIR:Database>"
        COMMENT "Copying MSVC AddressSanitizer runtime"
    )
endif()
