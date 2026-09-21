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

add_executable(OXEventTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/OXEventTests.cpp
    OXEvent.cpp
    buffer_manager.cpp
    event.cpp
    event_queue.cpp
)
target_link_libraries(OXEventTests PRIVATE SERVER EnTT::EnTT LUA S_Core)
if(MSVC)
    target_compile_options(OXEventTests PRIVATE /Gy)
    target_link_options(OXEventTests PRIVATE /OPT:REF /OPT:NOICF)
endif()
add_test(NAME ox_event COMMAND OXEventTests)

add_executable(QuestRuntimeTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/QuestRuntimeTests.cpp
    questmanager.cpp
    questpc.cpp
    questlua_npc.cpp
    trigger.cpp
    ecs/VIDRegistry.cpp
    ecs/systems/QuestSystem.cpp
    buffer_manager.cpp
    event.cpp
    event_queue.cpp
    utils.cpp
)
target_link_libraries(QuestRuntimeTests PRIVATE SERVER EnTT::EnTT LUA S_Core S_Poly)
if(MSVC)
    target_compile_options(QuestRuntimeTests PRIVATE /Gy)
    target_link_options(QuestRuntimeTests PRIVATE /OPT:REF /OPT:NOICF)
endif()
if(WIN32)
    target_link_libraries(QuestRuntimeTests PRIVATE ws2_32 bcrypt secur32 crypt32 shlwapi)
endif()
add_test(NAME quest_runtime COMMAND QuestRuntimeTests)

add_executable(HorseRuntimeTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/HorseRuntimeTests.cpp
    horse_rider.cpp
    event.cpp
    event_queue.cpp
)
target_link_libraries(HorseRuntimeTests PRIVATE SERVER EnTT::EnTT LUA)
add_test(NAME horse_runtime COMMAND HorseRuntimeTests)

add_executable(WeddingLifecycleTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/WeddingLifecycleTests.cpp
    wedding.cpp
)
target_link_libraries(WeddingLifecycleTests PRIVATE SERVER EnTT::EnTT LUA)
add_test(NAME wedding_lifecycle COMMAND WeddingLifecycleTests)

add_executable(PartyLifecycleTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/PartyLifecycleTests.cpp
    party.cpp
)
target_link_libraries(PartyLifecycleTests PRIVATE SERVER EnTT::EnTT LUA)
add_test(NAME party_lifecycle COMMAND PartyLifecycleTests)

add_executable(DungeonLifecycleTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/DungeonLifecycleTests.cpp
    dungeon.cpp
)
target_link_libraries(DungeonLifecycleTests PRIVATE SERVER EnTT::EnTT LUA)
add_test(NAME dungeon_lifecycle COMMAND DungeonLifecycleTests)

add_executable(ShopLifecycleTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/ShopLifecycleTests.cpp
    shop.cpp
    shop_manager.cpp
    group_text_parse_tree.cpp
    ../Base/grid.cpp
    file_loader.cpp
    buffer_manager.cpp
    event.cpp
    event_queue.cpp
)
target_link_libraries(ShopLifecycleTests PRIVATE SERVER EnTT::EnTT LUA S_Core)
add_test(NAME shop_lifecycle COMMAND ShopLifecycleTests)

add_executable(MountLifecycleTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/MountLifecycleTests.cpp
    PetSystem.cpp
    New_PetSystem.cpp
)
target_link_libraries(MountLifecycleTests PRIVATE SERVER EnTT::EnTT)
add_test(NAME mount_lifecycle COMMAND MountLifecycleTests)

add_executable(OfflineShopSafeboxTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/OfflineShopSafeboxTests.cpp
    ecs/systems/OfflineShopSystem.cpp
)
target_link_libraries(OfflineShopSafeboxTests PRIVATE SERVER EnTT::EnTT)
add_test(NAME offline_shop_safebox COMMAND OfflineShopSafeboxTests)

add_executable(SocialRelationTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/SocialRelationTests.cpp
    ecs/systems/SocialSystem.cpp
    event.cpp
    event_queue.cpp
)
target_link_libraries(SocialRelationTests PRIVATE SERVER EnTT::EnTT LUA)
if(MSVC)
    target_compile_options(SocialRelationTests PRIVATE /Gy)
    target_link_options(SocialRelationTests PRIVATE /OPT:REF /OPT:NOICF)
endif()
if(WIN32)
    target_link_libraries(SocialRelationTests PRIVATE ws2_32 bcrypt secur32 crypt32 shlwapi)
endif()
add_test(NAME social_relation COMMAND SocialRelationTests)

add_executable(SafeboxLifecycleTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/SafeboxLifecycleTests.cpp
    safebox.cpp
    ../Base/grid.cpp
)
target_link_libraries(SafeboxLifecycleTests PRIVATE SERVER EnTT::EnTT)
add_test(NAME safebox_lifecycle COMMAND SafeboxLifecycleTests)

add_executable(ItemManagerLifecycleTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/ItemManagerLifecycleTests.cpp
    item_manager.cpp
    safebox.cpp
    ../Base/grid.cpp
)
target_link_libraries(ItemManagerLifecycleTests PRIVATE SERVER EnTT::EnTT LUA)
add_test(NAME item_manager_lifecycle COMMAND ItemManagerLifecycleTests)

add_executable(ItemRuntimeTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/ItemRuntimeTests.cpp
    ecs/systems/ItemSystem.cpp
    ecs/ItemRegistry.cpp
)
target_link_libraries(ItemRuntimeTests PRIVATE SERVER EnTT::EnTT LUA)
if(MSVC)
    target_compile_options(ItemRuntimeTests PRIVATE /Gy)
    target_link_options(ItemRuntimeTests PRIVATE /OPT:REF /OPT:NOICF)
endif()
add_test(NAME item_runtime COMMAND ItemRuntimeTests)

add_executable(QuickslotTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/QuickslotTests.cpp
    ecs/systems/InventorySystem.cpp
)
target_link_libraries(QuickslotTests PRIVATE SERVER EnTT::EnTT LUA)
add_test(NAME quickslots COMMAND QuickslotTests)

add_executable(SpatialLifecycleTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/SpatialLifecycleTests.cpp
    sectree.cpp
    ecs/CBuildingRegistry.cpp
    ecs/OfflineShopEntityRegistry.cpp
    ecs/services/SpatialService.cpp
    ecs/systems/VisibilitySystem.cpp
    ecs/systems/MovementSystem.cpp
    event.cpp
    event_queue.cpp
)
target_link_libraries(SpatialLifecycleTests PRIVATE SERVER EnTT::EnTT LUA)
if(MSVC)
    target_compile_options(SpatialLifecycleTests PRIVATE /Gy)
    target_link_options(SpatialLifecycleTests PRIVATE /OPT:REF /OPT:NOICF)
endif()
add_test(NAME spatial_lifecycle COMMAND SpatialLifecycleTests)

add_executable(BuildingLifecycleTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/BuildingLifecycleTests.cpp
    building.cpp
    buffer_manager.cpp
    ecs/CBuildingRegistry.cpp
)
target_link_libraries(BuildingLifecycleTests PRIVATE SERVER EnTT::EnTT LUA S_Core)
if(MSVC)
    target_compile_options(BuildingLifecycleTests PRIVATE /Gy)
    target_link_options(BuildingLifecycleTests PRIVATE /OPT:REF /OPT:NOICF)
endif()
add_test(NAME building_lifecycle COMMAND BuildingLifecycleTests)

add_executable(BuffAttributeTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/BuffAttributeTests.cpp
    buff_on_attributes.cpp
)
target_link_libraries(BuffAttributeTests PRIVATE SERVER EnTT::EnTT LUA)
if(MSVC)
    target_compile_options(BuffAttributeTests PRIVATE /Gy)
    target_link_options(BuffAttributeTests PRIVATE /OPT:REF /OPT:NOICF)
endif()
add_test(NAME buff_attributes COMMAND BuffAttributeTests)

add_executable(EntityNetworkDispatchTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/EntityNetworkDispatchTests.cpp
    entity.cpp
    ecs/services/EntityNetworkDispatch.cpp
    ecs/systems/NetworkSyncSystem.cpp
)
target_link_libraries(EntityNetworkDispatchTests PRIVATE SERVER EnTT::EnTT LUA)
if(MSVC)
    target_compile_options(EntityNetworkDispatchTests PRIVATE /Gy)
    target_link_options(EntityNetworkDispatchTests PRIVATE /OPT:REF /OPT:NOICF)
endif()
add_test(NAME entity_network_dispatch COMMAND EntityNetworkDispatchTests)

add_executable(PointCalculationTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/PointCalculationTests.cpp
    ecs/systems/PointSystem.cpp
    ecs/systems/StatSystem.cpp
)
target_link_libraries(PointCalculationTests PRIVATE SERVER EnTT::EnTT LUA)
add_test(NAME point_calculation COMMAND PointCalculationTests)

add_executable(CombatStateTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/CombatStateTests.cpp
    ecs/systems/CombatSystem.cpp
    battle.cpp
)
target_compile_definitions(CombatStateTests PRIVATE ENABLE_ANTICHEAT ENABLE_CHECK_BATTLE)
target_link_libraries(CombatStateTests PRIVATE SERVER EnTT::EnTT LUA)
if(WIN32)
    target_link_libraries(CombatStateTests PRIVATE ws2_32 bcrypt secur32 crypt32 shlwapi)
endif()
add_test(NAME combat_state COMMAND CombatStateTests)

add_executable(AIStateTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/AIStateTests.cpp
    ecs/systems/AISystem.cpp
    ecs/Registry.cpp
)
target_link_libraries(AIStateTests PRIVATE SERVER EnTT::EnTT LUA)
if(WIN32)
    target_link_libraries(AIStateTests PRIVATE ws2_32 bcrypt secur32 crypt32 shlwapi)
endif()
add_test(NAME ai_state COMMAND AIStateTests)

add_executable(AffectLifecycleTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/AffectLifecycleTests.cpp
    ecs/systems/AffectSystem.cpp
    affect.cpp
    horsename_manager.cpp
)
target_link_libraries(AffectLifecycleTests PRIVATE SERVER EnTT::EnTT LUA)
if(WIN32)
    target_link_libraries(AffectLifecycleTests PRIVATE ws2_32 bcrypt secur32 crypt32 shlwapi)
endif()
add_test(NAME affect_lifecycle COMMAND AffectLifecycleTests)

add_executable(EventLifecycleTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/EventLifecycleTests.cpp
    event.cpp
    event_queue.cpp
)
target_link_libraries(EventLifecycleTests PRIVATE SERVER EnTT::EnTT)
add_test(NAME event_lifecycle COMMAND EventLifecycleTests)

add_executable(CharacterManagerTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/CharacterManagerTests.cpp
    char_manager.cpp
    ecs/VIDRegistry.cpp
    ecs/PIDRegistry.cpp
    event.cpp
    event_queue.cpp
)
target_link_libraries(CharacterManagerTests PRIVATE SERVER EnTT::EnTT LUA)
if(WIN32)
    target_link_libraries(CharacterManagerTests PRIVATE ws2_32 bcrypt secur32 crypt32 shlwapi)
endif()
add_test(NAME character_manager COMMAND CharacterManagerTests)

add_executable(RefineWindowTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/RefineWindowTests.cpp
    ecs/systems/DragonSoulSystem.cpp
)
target_link_libraries(RefineWindowTests PRIVATE SERVER EnTT::EnTT)
add_test(NAME refine_window COMMAND RefineWindowTests)

add_executable(GmCommandTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/GmCommandTests.cpp
    cmd_gm.cpp
    skill_power.cpp
    ecs/systems/SkillSystem.cpp
    ecs/PIDRegistry.cpp
    buffer_manager.cpp
    utils.cpp
)
target_link_libraries(GmCommandTests PRIVATE SERVER EnTT::EnTT LUA S_Core S_Poly)
if(MSVC)
    # Keep actual command bodies while dropping unrelated server entry points.
    target_compile_options(GmCommandTests PRIVATE /Gy)
    target_link_options(GmCommandTests PRIVATE /OPT:REF /OPT:NOICF)
endif()
if(WIN32)
    target_link_libraries(GmCommandTests PRIVATE ws2_32 bcrypt secur32 crypt32 shlwapi)
endif()
add_test(NAME gm_commands COMMAND GmCommandTests)

add_executable(ExchangeTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/ExchangeTests.cpp
    exchange.cpp
)
target_link_libraries(ExchangeTests PRIVATE SERVER EnTT::EnTT LUA)
add_test(NAME exchange COMMAND ExchangeTests)

# Exercise the same generation-aware diagnostic implementation as GameServer.
# The isolated event/refine-window suites have no diagnostic dependency.
foreach(_ecs_test ItemAttributeTests MountLifecycleTests SafeboxLifecycleTests ItemManagerLifecycleTests QuickslotTests PointCalculationTests CombatStateTests AffectLifecycleTests CharacterManagerTests GmCommandTests ExchangeTests)
    target_sources(${_ecs_test} PRIVATE ecs/EcsDiagnostics.cpp)
endforeach()

if(ENABLE_ASAN AND EXISTS "${AURIGA_ASAN_RUNTIME_DLL}")
    foreach(_asan_target GameServer OXEventTests QuestRuntimeTests HorseRuntimeTests ItemAttributeTests MountLifecycleTests SafeboxLifecycleTests ItemManagerLifecycleTests ItemRuntimeTests QuickslotTests SpatialLifecycleTests BuildingLifecycleTests BuffAttributeTests EntityNetworkDispatchTests PointCalculationTests CombatStateTests AffectLifecycleTests EventLifecycleTests CharacterManagerTests RefineWindowTests GmCommandTests ExchangeTests OfflineShopSafeboxTests SocialRelationTests)
        add_custom_command(TARGET ${_asan_target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${AURIGA_ASAN_RUNTIME_DLL}"
                "$<TARGET_FILE_DIR:${_asan_target}>"
            COMMENT "Copying MSVC AddressSanitizer runtime"
        )
    endforeach()
endif()

# LPENTITY.4-fixup.3: runtime drift detector. Debug only.
# Compares legacy CHARACTER fields to ECS components inside
# EntityNetworkDispatch::BuildCharacterInsert. Removed in LPENTITY.6.
# It was on for RelWithDebInfo as well, which is the configuration the live
# server runs: DriftSweep walks every character entity every five seconds and
# recomputes its viewer set from the sectree, and the same flag adds a parity
# check inside BuildCharacterInsert, on the path every insert packet takes.
target_compile_definitions(GameServer PRIVATE
    $<$<CONFIG:Debug>:AURIGA_LPENTITY_FIXUP_AUDIT>
)
