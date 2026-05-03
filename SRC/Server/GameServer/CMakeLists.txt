if(WIN32 AND DEFINED VCPKG_ROOT AND DEFINED VCPKG_TARGET_TRIPLET)
    set(_auriga_entt_dir "${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/share/entt")
    if(EXISTS "${_auriga_entt_dir}/EnTTConfig.cmake")
        set(EnTT_DIR "${_auriga_entt_dir}")
    endif()
endif()

find_package(EnTT CONFIG REQUIRED)

auriga_add_server_executable(GameServer LIBS LUA)
target_link_libraries(GameServer PRIVATE EnTT::EnTT)

# LPENTITY.4-fixup.3: enable runtime drift detector in non-release builds.
# Compares legacy CHARACTER fields to ECS components inside
# EntityNetworkDispatch::BuildCharacterInsert. Removed in LPENTITY.6.
target_compile_definitions(GameServer PRIVATE
    $<$<CONFIG:Debug>:AURIGA_LPENTITY_FIXUP_AUDIT>
    $<$<CONFIG:RelWithDebInfo>:AURIGA_LPENTITY_FIXUP_AUDIT>
)
