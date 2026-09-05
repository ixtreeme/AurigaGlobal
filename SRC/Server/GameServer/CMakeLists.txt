if(WIN32 AND DEFINED VCPKG_ROOT AND DEFINED VCPKG_TARGET_TRIPLET)
    set(_auriga_entt_dir "${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/share/entt")
    if(EXISTS "${_auriga_entt_dir}/EnTTConfig.cmake")
        set(EnTT_DIR "${_auriga_entt_dir}")
    endif()
endif()

find_package(EnTT CONFIG REQUIRED)

auriga_add_server_executable(GameServer LIBS LUA)
target_link_libraries(GameServer PRIVATE EnTT::EnTT)

# Headless regression checks compile the production attribute, switchbot and DS
# sources with service test doubles; no server/DB startup is needed.
add_executable(ItemAttributeTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/ItemAttributeTests.cpp
    ecs/systems/ItemAttributeSystem.cpp
    new_switchbot.cpp
    DragonSoul.cpp
    attr_transfer.cpp
)
target_link_libraries(ItemAttributeTests PRIVATE SERVER EnTT::EnTT)
add_test(NAME item_attributes COMMAND ItemAttributeTests)

add_executable(MountLifecycleTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/MountLifecycleTests.cpp
    MountSystem.cpp
    PetSystem.cpp
    New_PetSystem.cpp
)
target_link_libraries(MountLifecycleTests PRIVATE SERVER EnTT::EnTT)
add_test(NAME mount_lifecycle COMMAND MountLifecycleTests)

if(ENABLE_ASAN AND EXISTS "${AURIGA_ASAN_RUNTIME_DLL}")
    foreach(_asan_target GameServer ItemAttributeTests MountLifecycleTests)
        add_custom_command(TARGET ${_asan_target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${AURIGA_ASAN_RUNTIME_DLL}"
                "$<TARGET_FILE_DIR:${_asan_target}>"
            COMMENT "Copying MSVC AddressSanitizer runtime"
        )
    endforeach()
endif()

# LPENTITY.4-fixup.3: enable runtime drift detector in non-release builds.
# Compares legacy CHARACTER fields to ECS components inside
# EntityNetworkDispatch::BuildCharacterInsert. Removed in LPENTITY.6.
target_compile_definitions(GameServer PRIVATE
    $<$<CONFIG:Debug>:AURIGA_LPENTITY_FIXUP_AUDIT>
    $<$<CONFIG:RelWithDebInfo>:AURIGA_LPENTITY_FIXUP_AUDIT>
)
