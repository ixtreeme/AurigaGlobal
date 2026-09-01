if(WIN32 AND DEFINED VCPKG_ROOT AND DEFINED VCPKG_TARGET_TRIPLET)
    set(_auriga_entt_dir "${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/share/entt")
    if(EXISTS "${_auriga_entt_dir}/EnTTConfig.cmake")
        set(EnTT_DIR "${_auriga_entt_dir}")
    endif()
endif()

find_package(EnTT CONFIG REQUIRED)

auriga_add_server_executable(GameServer LIBS LUA)
target_link_libraries(GameServer PRIVATE EnTT::EnTT)

if(ENABLE_ASAN AND EXISTS "${AURIGA_ASAN_RUNTIME_DLL}")
    add_custom_command(TARGET GameServer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${AURIGA_ASAN_RUNTIME_DLL}"
            "$<TARGET_FILE_DIR:GameServer>"
        COMMENT "Copying MSVC AddressSanitizer runtime"
    )
endif()

# LPENTITY.4-fixup.3: enable runtime drift detector in non-release builds.
# Compares legacy CHARACTER fields to ECS components inside
# EntityNetworkDispatch::BuildCharacterInsert. Removed in LPENTITY.6.
target_compile_definitions(GameServer PRIVATE
    $<$<CONFIG:Debug>:AURIGA_LPENTITY_FIXUP_AUDIT>
    $<$<CONFIG:RelWithDebInfo>:AURIGA_LPENTITY_FIXUP_AUDIT>
)
