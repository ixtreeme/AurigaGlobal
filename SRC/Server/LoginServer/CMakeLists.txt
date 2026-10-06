# LoginServer: standalone login/auth server, next to Database and GameServer.
#
# It depends only on the generic layers (Core/SQL/common) and must never link
# GameServer sources. Own boot, own config, own listener; the login protocol
# is built on top of this skeleton next.
file(GLOB LOGINSERVER_SOURCES CONFIGURE_DEPENDS "*.cpp")

add_executable(LoginServer ${LOGINSERVER_SOURCES})
target_link_libraries(LoginServer PRIVATE
    S_Base
    S_Core
    S_SQL
    SERVER
    ${AURIGA_SERVER_PLATFORM_LIBS}
)
set_target_properties(LoginServer PROPERTIES FOLDER Server)
source_group(TREE ${CMAKE_CURRENT_LIST_DIR} FILES ${LOGINSERVER_SOURCES})

if(WIN32)
    # MariaDB Connector/C (TLS + crypto) system dependencies.
    target_link_libraries(LoginServer PRIVATE ws2_32 secur32 crypt32 shlwapi bcrypt)
endif()

if(ENABLE_ASAN AND EXISTS "${AURIGA_ASAN_RUNTIME_DLL}")
    add_custom_command(TARGET LoginServer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${AURIGA_ASAN_RUNTIME_DLL}"
            "$<TARGET_FILE_DIR:LoginServer>"
        COMMENT "Copying MSVC AddressSanitizer runtime"
    )
endif()
