if(WIN32 AND DEFINED VCPKG_ROOT AND DEFINED VCPKG_TARGET_TRIPLET)
    set(_auriga_entt_dir "${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/share/entt")
    if(EXISTS "${_auriga_entt_dir}/EnTTConfig.cmake")
        set(EnTT_DIR "${_auriga_entt_dir}")
    endif()
endif()

find_package(EnTT CONFIG REQUIRED)

auriga_add_server_executable(GameServer LIBS LUA)
target_link_libraries(GameServer PRIVATE EnTT::EnTT)

add_executable(WoodcuttingRulesTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/WoodcuttingRulesTests.cpp)
add_test(NAME woodcutting_rules COMMAND WoodcuttingRulesTests)

add_executable(ServerConfigTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/ServerConfigTests.cpp
    core/GameConfig.cpp
    ../Database/DatabaseConfig.cpp
    ../LoginServer/login_config.cpp
)
target_link_libraries(ServerConfigTests PRIVATE SERVER S_Core)
add_test(NAME server_config COMMAND ServerConfigTests)

# Independent Mission Engine domain kernel; no live ECS, SQL, Lua or client hooks.
find_path(AURIGA_MISSION_JSON_INCLUDE NAMES nlohmann/json.hpp REQUIRED)
find_package(Threads REQUIRED)
target_include_directories(GameServer PRIVATE ${AURIGA_MISSION_JSON_INCLUDE})
add_executable(MissionCoreTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/MissionCoreTests.cpp
    mission/MissionDefinition.cpp
    mission/MissionValidator.cpp
    mission/MissionConditionEvaluator.cpp
    mission/MissionEventRouter.cpp
    mission/MissionRuntime.cpp
    mission/MissionCheckpoint.cpp
)
target_include_directories(MissionCoreTests PRIVATE ${AURIGA_MISSION_JSON_INCLUDE})
target_link_libraries(MissionCoreTests PRIVATE Threads::Threads)
if(TARGET auriga_sanitizers)
    target_link_libraries(MissionCoreTests PRIVATE auriga_sanitizers)
endif()
add_test(NAME mission_core COMMAND MissionCoreTests)
add_executable(MissionCheck EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/SRC/Tools/MissionCheck.cpp
    mission/MissionDefinition.cpp
    mission/MissionValidator.cpp
)
target_include_directories(MissionCheck PRIVATE ${AURIGA_MISSION_JSON_INCLUDE})
target_link_libraries(MissionCheck PRIVATE Threads::Threads)
if(TARGET auriga_sanitizers)
    target_link_libraries(MissionCheck PRIVATE auriga_sanitizers)
endif()
set(_mission_package_sources
    mission/MissionContentHash.cpp
    mission/MissionDefinition.cpp
    mission/MissionCatalog.cpp
    mission/MissionValidator.cpp
    mission/MissionNativeSchema.cpp
    mission/MissionPackageCodec.cpp
    mission/MissionPackageSource.cpp
    mission/MissionPackageManager.cpp
)
add_executable(MissionCompiler EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/SRC/Tools/MissionCompiler.cpp
    ${_mission_package_sources}
)
add_executable(MissionPackageTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/MissionPackageTests.cpp
    ${_mission_package_sources}
    mission/MissionConditionEvaluator.cpp
    mission/MissionEventRouter.cpp
    mission/MissionRuntime.cpp
    mission/MissionCheckpoint.cpp
)
foreach(_mission_package_target MissionCompiler MissionPackageTests)
    target_include_directories(${_mission_package_target} PRIVATE ${AURIGA_MISSION_JSON_INCLUDE})
    target_link_libraries(${_mission_package_target} PRIVATE Threads::Threads OpenSSL::Crypto)
    if(TARGET auriga_sanitizers)
        target_link_libraries(${_mission_package_target} PRIVATE auriga_sanitizers)
    endif()
endforeach()
add_test(NAME mission_package COMMAND MissionPackageTests
    ${CMAKE_SOURCE_DIR}/config/mission_engine_package.example.json)

set(_mission_persistence_sources
    ${_mission_package_sources}
    mission/MissionConditionEvaluator.cpp
    mission/MissionEventRouter.cpp
    mission/MissionRuntime.cpp
    mission/MissionCheckpoint.cpp
    mission/MissionCheckpointCodec.cpp
    mission/MissionActions.cpp
    mission/MissionRewardBinding.cpp
    mission/MissionResourcePlan.cpp
)
add_executable(MissionCheckpointTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/MissionCheckpointTests.cpp
    ${_mission_persistence_sources}
)
target_include_directories(MissionCheckpointTests PRIVATE ${AURIGA_MISSION_JSON_INCLUDE})
target_link_libraries(MissionCheckpointTests PRIVATE Threads::Threads OpenSSL::Crypto)
if(TARGET auriga_sanitizers)
    target_link_libraries(MissionCheckpointTests PRIVATE auriga_sanitizers)
endif()
add_test(NAME mission_checkpoint COMMAND MissionCheckpointTests)

add_executable(MissionRewardsTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/MissionRewardsTests.cpp
    ${_mission_persistence_sources}
)
target_include_directories(MissionRewardsTests PRIVATE ${AURIGA_MISSION_JSON_INCLUDE})
target_link_libraries(MissionRewardsTests PRIVATE Threads::Threads OpenSSL::Crypto)
if(TARGET auriga_sanitizers)
    target_link_libraries(MissionRewardsTests PRIVATE auriga_sanitizers)
endif()
add_test(NAME mission_rewards COMMAND MissionRewardsTests
    ${CMAKE_SOURCE_DIR}/tests/server/fixtures/mission_package_v1.aurmission
    ${CMAKE_SOURCE_DIR}/config/mission_engine_reward_package.example.json)

add_executable(MissionPersistenceTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/MissionPersistenceTests.cpp
    ${_mission_persistence_sources}
    mission/MissionPersistence.cpp
    mission/MissionItemAwards.cpp
    mission/MissionResourceEffects.cpp
    mission/MissionResourceFence.cpp
    mission/MissionResourceDrain.cpp
    ../Database/ItemAwardDelivery.cpp
    ../Database/ItemAwardScan.cpp
)

add_executable(MissionResourceTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/MissionResourceTests.cpp
    ${_mission_persistence_sources}
    mission/MissionResourceFence.cpp
)
target_include_directories(MissionResourceTests PRIVATE ${AURIGA_MISSION_JSON_INCLUDE} ${CMAKE_SOURCE_DIR}/SRC/Server)
target_link_libraries(MissionResourceTests PRIVATE Threads::Threads OpenSSL::Crypto)
if(TARGET auriga_sanitizers)
    target_link_libraries(MissionResourceTests PRIVATE auriga_sanitizers)
endif()
add_test(NAME mission_resources COMMAND MissionResourceTests)
target_include_directories(MissionPersistenceTests PRIVATE ${AURIGA_MISSION_JSON_INCLUDE})
target_link_libraries(MissionPersistenceTests PRIVATE SERVER S_SQL S_Core Threads::Threads OpenSSL::Crypto)
if(WIN32)
    target_link_libraries(MissionPersistenceTests PRIVATE ws2_32 bcrypt secur32 crypt32 shlwapi)
endif()
if(ENABLE_ASAN AND EXISTS "${AURIGA_ASAN_RUNTIME_DLL}")
    foreach(_mission_tool MissionCoreTests MissionCheck MissionCompiler MissionPackageTests MissionCheckpointTests MissionPersistenceTests MissionRewardsTests MissionResourceTests)
        add_custom_command(TARGET ${_mission_tool} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${AURIGA_ASAN_RUNTIME_DLL}" "$<TARGET_FILE_DIR:${_mission_tool}>"
        )
    endforeach()
endif()
add_test(NAME mission_definition_example COMMAND MissionCheck
    ${CMAKE_SOURCE_DIR}/config/mission_engine_core.example.json)

add_executable(LoggingShutdownTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/LoggingShutdownTests.cpp
)
target_link_libraries(LoggingShutdownTests PRIVATE SERVER S_Core)
add_test(NAME logging_shutdown COMMAND LoggingShutdownTests)

add_executable(ServerPackageCheck EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/SRC/Tools/ServerPackageCheck.cpp
    core/JsonFile.cpp
    quest/quest_data.cpp
    quest/quest_source.cpp
    quest/quest_snapshot.cpp
)
target_link_libraries(ServerPackageCheck PRIVATE SERVER LUA S_Core)

add_executable(MotionTimingTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/MotionTimingTests.cpp
    entity/motion.cpp
)
target_link_libraries(MotionTimingTests PRIVATE SERVER EnTT::EnTT LUA S_Core)
if(WIN32)
    target_link_libraries(MotionTimingTests PRIVATE ws2_32 bcrypt secur32 crypt32 shlwapi)
endif()
if(MSVC)
    target_compile_options(MotionTimingTests PRIVATE /Gy)
    target_link_options(MotionTimingTests PRIVATE /OPT:REF /OPT:NOICF)
else()
    target_compile_options(MotionTimingTests PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(MotionTimingTests PRIVATE -Wl,--gc-sections)
endif()
add_test(NAME motion_timing COMMAND MotionTimingTests)

if(ENABLE_ASAN AND EXISTS "${AURIGA_ASAN_RUNTIME_DLL}")
    add_custom_command(TARGET ServerPackageCheck POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${AURIGA_ASAN_RUNTIME_DLL}"
            "$<TARGET_FILE_DIR:ServerPackageCheck>"
    )
    add_custom_command(TARGET MotionTimingTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${AURIGA_ASAN_RUNTIME_DLL}"
            "$<TARGET_FILE_DIR:MotionTimingTests>"
    )
endif()

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME mission_database_tools COMMAND ${Python3_EXECUTABLE} -B
        ${CMAKE_SOURCE_DIR}/tests/server/MissionDatabaseToolsTests.py)
    add_test(NAME mission_package_tools COMMAND ${Python3_EXECUTABLE} -B
        ${CMAKE_SOURCE_DIR}/tests/server/MissionPackageToolsTests.py
        --compiler $<TARGET_FILE:MissionCompiler>
        --source ${CMAKE_SOURCE_DIR}/config/mission_engine_package.example.json)
    add_test(NAME server_config_tools COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/server/ServerConfigToolsTests.py)
    add_test(NAME server_package_tools COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/server/ServerPackageToolsTests.py --checker $<TARGET_FILE:ServerPackageCheck>)
    add_test(NAME quest_source_migration COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/server/QuestSourceMigrationTests.py --checker $<TARGET_FILE:ServerPackageCheck>)
    add_test(NAME server_manager_tools COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/server/ServerManagerTests.py)
    add_test(NAME server_manager_ssh COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/server/ServerManagerSshTests.py)
    add_test(NAME server_manager_password COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/server/ServerManagerPasswordTests.py)
    add_test(NAME server_backup_tools COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/server/ServerBackupTests.py)
    add_test(NAME server_manager_backups COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/server/ServerManagerBackupTests.py)
endif()

# Headless regression checks compile the production attribute and switchbot
# sources with service test doubles; no server/DB startup is needed.
add_executable(ItemAttributeTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/ItemAttributeTests.cpp
    ecs/systems/ItemAttributeSystem.cpp
    social/new_switchbot.cpp
    item/attr_transfer.cpp
)
target_link_libraries(ItemAttributeTests PRIVATE SERVER EnTT::EnTT)
add_test(NAME item_attributes COMMAND ItemAttributeTests)

# The switchbot job lifecycle: player-id index, warp freeze, timer, P2P
# hand-off and teardown, over the production source.
add_executable(SwitchbotLifecycleTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/SwitchbotLifecycleTests.cpp
    social/new_switchbot.cpp
)
target_link_libraries(SwitchbotLifecycleTests PRIVATE SERVER EnTT::EnTT)
add_test(NAME switchbot_lifecycle COMMAND SwitchbotLifecycleTests)

add_executable(OXEventTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/OXEventTests.cpp
    world/OXEvent.cpp
    world/event.cpp
    world/event_queue.cpp
)
target_link_libraries(OXEventTests PRIVATE SERVER EnTT::EnTT LUA S_Core)
if(MSVC)
    target_compile_options(OXEventTests PRIVATE /Gy)
    target_link_options(OXEventTests PRIVATE /OPT:REF /OPT:NOICF)
endif()
add_test(NAME ox_event COMMAND OXEventTests)

add_executable(QuestRuntimeTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/QuestRuntimeTests.cpp
    quest/questmanager.cpp
    quest/quest_data.cpp
    quest/quest_source.cpp
    quest/quest_snapshot.cpp
    quest/quest_runtime.cpp
    quest/quest_hotreload.cpp
    quest/quest_reload_control.cpp
    quest/quest_execution.cpp
    quest/questevent.cpp
    quest/questpc.cpp
    quest/questlua_npc.cpp
    quest/questlua_event_flag.cpp
    quest/trigger.cpp
    ecs/VIDRegistry.cpp
    ecs/systems/QuestSystem.cpp
    world/event.cpp
    world/event_queue.cpp
    core/utils.cpp
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
if(Python3_Interpreter_FOUND)
    add_test(NAME quest_hotreload COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tests/server/QuestHotreloadTests.py
        --core $<TARGET_FILE:QuestRuntimeTests> --checker $<TARGET_FILE:ServerPackageCheck>)
endif()

add_executable(LuaSourceTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/LuaSourceTests.cpp
    ${CMAKE_SOURCE_DIR}/tests/server/NPCSourceTests.cpp
    ${CMAKE_SOURCE_DIR}/tests/server/DungeonQuestTests.cpp
    quest/quest_source.cpp
    quest/questnpc.cpp
    quest/quest_runtime.cpp
    quest/quest_snapshot.cpp
)
target_link_libraries(LuaSourceTests PRIVATE LUA SERVER EnTT::EnTT S_Core)
if(MSVC)
    target_compile_options(LuaSourceTests PRIVATE /Gy)
    target_link_options(LuaSourceTests PRIVATE /OPT:REF /OPT:NOICF)
else()
    target_compile_options(LuaSourceTests PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(LuaSourceTests PRIVATE -Wl,--gc-sections)
endif()
add_test(NAME lua_sources COMMAND LuaSourceTests)

add_executable(QuestDataTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/QuestDataTests.cpp
    quest/quest_data.cpp
)
target_link_libraries(QuestDataTests PRIVATE SERVER)
add_test(NAME quest_data COMMAND QuestDataTests)

add_executable(HorseRuntimeTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/HorseRuntimeTests.cpp
    pet/horse_rider.cpp
    world/event.cpp
    world/event_queue.cpp
)
target_link_libraries(HorseRuntimeTests PRIVATE SERVER EnTT::EnTT LUA)
add_test(NAME horse_runtime COMMAND HorseRuntimeTests)

add_executable(WeddingLifecycleTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/WeddingLifecycleTests.cpp
    social/wedding.cpp
)
target_link_libraries(WeddingLifecycleTests PRIVATE SERVER EnTT::EnTT LUA)
add_test(NAME wedding_lifecycle COMMAND WeddingLifecycleTests)

# The guild member rows over the production guild.cpp: the player-id
# index, add/load/remove, the member packets and the row lifetime.
add_executable(GuildMemberLifecycleTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/GuildMemberLifecycleTests.cpp
    guild/guild.cpp
    ecs/PIDRegistry.cpp
)
target_link_libraries(GuildMemberLifecycleTests PRIVATE SERVER EnTT::EnTT LUA S_Core)
if(WIN32)
    target_link_libraries(GuildMemberLifecycleTests PRIVATE ws2_32 bcrypt secur32 crypt32 shlwapi)
endif()
add_test(NAME guild_member_lifecycle COMMAND GuildMemberLifecycleTests)

# The LoginServer's Database link (login_db_link.cpp) against a scripted
# Database on a loopback socket: the interserver auth handshake accepted,
# and refused in each way it can be.
add_executable(LoginDbLinkTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/LoginDbLinkTests.cpp
    ../LoginServer/login_db_link.cpp
)
target_link_libraries(LoginDbLinkTests PRIVATE SERVER S_Core)
if(WIN32)
    target_link_libraries(LoginDbLinkTests PRIVATE ws2_32 bcrypt secur32 crypt32 shlwapi)
endif()
add_test(NAME login_db_link COMMAND LoginDbLinkTests)

# The login server's account authentication from the raw LOGIN3 bytes, over a
# recording session and account database.
add_executable(LoginAuthTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/LoginAuthTests.cpp
    ../LoginServer/login_auth.cpp
    ../LoginServer/login_hwid.cpp
    ../LoginServer/login_limiter.cpp
)
target_link_libraries(LoginAuthTests PRIVATE SERVER S_Core)
if(WIN32)
    target_link_libraries(LoginAuthTests PRIVATE ws2_32 bcrypt secur32 crypt32 shlwapi)
endif()
add_test(NAME login_auth COMMAND LoginAuthTests)

add_executable(LoginLimiterTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/LoginLimiterTests.cpp
    ../LoginServer/login_limiter.cpp
)
target_link_libraries(LoginLimiterTests PRIVATE SERVER)
add_test(NAME login_limiter COMMAND LoginLimiterTests)

add_executable(CharacterSelectionTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/CharacterSelectionTests.cpp
    network/character_selection.cpp
)
target_link_libraries(CharacterSelectionTests PRIVATE SERVER EnTT::EnTT)
add_test(NAME character_selection COMMAND CharacterSelectionTests)

add_executable(PlayerLoadTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/PlayerLoadTests.cpp
    ../Database/player_load.cpp
)
target_link_libraries(PlayerLoadTests PRIVATE SERVER)
add_test(NAME player_load COMMAND PlayerLoadTests)

add_executable(PlayerSessionTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/PlayerSessionTests.cpp
    ../Database/player_load.cpp
)
target_link_libraries(PlayerSessionTests PRIVATE SERVER S_Core S_SQL)
if(WIN32)
    target_link_libraries(PlayerSessionTests PRIVATE ws2_32 bcrypt secur32 crypt32 shlwapi)
endif()
if(TARGET auriga_sanitizers)
    target_link_libraries(PlayerSessionTests PRIVATE auriga_sanitizers)
endif()
if(ENABLE_ASAN AND EXISTS "${AURIGA_ASAN_RUNTIME_DLL}")
    add_custom_command(TARGET PlayerSessionTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${AURIGA_ASAN_RUNTIME_DLL}" "$<TARGET_FILE_DIR:PlayerSessionTests>")
endif()
add_test(NAME player_session COMMAND PlayerSessionTests)

add_executable(PlayerCacheDrainTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/PlayerCacheDrainTests.cpp
    ../Database/player_cache_drain.cpp
    ../Database/Cache.cpp
    ../Database/player_save.cpp
)
target_link_libraries(PlayerCacheDrainTests PRIVATE SERVER S_Core S_SQL)
if(WIN32)
    target_link_libraries(PlayerCacheDrainTests PRIVATE ws2_32 bcrypt secur32 crypt32 shlwapi)
endif()
if(TARGET auriga_sanitizers)
    target_link_libraries(PlayerCacheDrainTests PRIVATE auriga_sanitizers)
endif()
if(ENABLE_ASAN AND EXISTS "${AURIGA_ASAN_RUNTIME_DLL}")
    add_custom_command(TARGET PlayerCacheDrainTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${AURIGA_ASAN_RUNTIME_DLL}" "$<TARGET_FILE_DIR:PlayerCacheDrainTests>")
endif()
add_test(NAME player_cache_drain COMMAND PlayerCacheDrainTests)

add_executable(PlayerCacheProtocolTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/PlayerCacheProtocolTests.cpp)
target_link_libraries(PlayerCacheProtocolTests PRIVATE SERVER S_Core)
if(TARGET auriga_sanitizers)
    target_link_libraries(PlayerCacheProtocolTests PRIVATE auriga_sanitizers)
endif()
if(ENABLE_ASAN AND EXISTS "${AURIGA_ASAN_RUNTIME_DLL}")
    add_custom_command(TARGET PlayerCacheProtocolTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${AURIGA_ASAN_RUNTIME_DLL}" "$<TARGET_FILE_DIR:PlayerCacheProtocolTests>")
endif()
add_test(NAME player_cache_protocol COMMAND PlayerCacheProtocolTests)

add_executable(PlayerCacheSnapshotTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/PlayerCacheSnapshotTests.cpp)
target_link_libraries(PlayerCacheSnapshotTests PRIVATE SERVER S_Core)
if(TARGET auriga_sanitizers)
    target_link_libraries(PlayerCacheSnapshotTests PRIVATE auriga_sanitizers)
endif()
if(ENABLE_ASAN AND EXISTS "${AURIGA_ASAN_RUNTIME_DLL}")
    add_custom_command(TARGET PlayerCacheSnapshotTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${AURIGA_ASAN_RUNTIME_DLL}" "$<TARGET_FILE_DIR:PlayerCacheSnapshotTests>")
endif()
add_test(NAME player_cache_snapshot COMMAND PlayerCacheSnapshotTests)

add_executable(ResourceAdmissionTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/ResourceAdmissionTests.cpp)
target_link_libraries(ResourceAdmissionTests PRIVATE SERVER S_Core EnTT::EnTT Threads::Threads)
if(TARGET auriga_sanitizers)
    target_link_libraries(ResourceAdmissionTests PRIVATE auriga_sanitizers)
endif()
if(ENABLE_ASAN AND EXISTS "${AURIGA_ASAN_RUNTIME_DLL}")
    add_custom_command(TARGET ResourceAdmissionTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${AURIGA_ASAN_RUNTIME_DLL}" "$<TARGET_FILE_DIR:ResourceAdmissionTests>")
endif()
add_test(NAME resource_admission COMMAND ResourceAdmissionTests)


add_executable(LoginByKeyTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/LoginByKeyTests.cpp
    ../Database/login_validation.cpp
)
target_link_libraries(LoginByKeyTests PRIVATE SERVER S_Core)
add_test(NAME login_by_key COMMAND LoginByKeyTests)

add_executable(PlayerCreateTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/PlayerCreateTests.cpp
    ../Database/player_create.cpp
)
target_link_libraries(PlayerCreateTests PRIVATE SERVER S_SQL S_Core)
if(WIN32)
    target_link_libraries(PlayerCreateTests PRIVATE ws2_32 bcrypt secur32 crypt32 shlwapi)
endif()
add_test(NAME player_create COMMAND PlayerCreateTests)

add_executable(DatabaseTimeTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/DatabaseTimeTests.cpp
    ../Database/ClientManagerEventFlag.cpp
    ../Database/player_save.cpp
    ../Database/Marriage.cpp
    ../Database/PrivManager.cpp
)
target_link_libraries(DatabaseTimeTests PRIVATE SERVER S_SQL S_Core)
if(MSVC)
    target_compile_options(DatabaseTimeTests PRIVATE /Gy)
    target_link_options(DatabaseTimeTests PRIVATE /OPT:REF /OPT:NOICF)
else()
    target_compile_options(DatabaseTimeTests PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(DatabaseTimeTests PRIVATE -Wl,--gc-sections)
endif()
if(WIN32)
    target_link_libraries(DatabaseTimeTests PRIVATE ws2_32 bcrypt secur32 crypt32 shlwapi)
endif()
add_test(NAME database_time COMMAND DatabaseTimeTests)

add_executable(PrivilegeTimeTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/PrivilegeTimeTests.cpp
    social/priv_manager.cpp
)
target_link_libraries(PrivilegeTimeTests PRIVATE SERVER EnTT::EnTT LUA)
add_test(NAME privilege_time COMMAND PrivilegeTimeTests)

add_executable(PacketStringTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/PacketStringTests.cpp
    network/packet_validation.cpp
    network/packet_info.cpp
)
target_link_libraries(PacketStringTests PRIVATE SERVER S_Core EnTT::EnTT)
add_test(NAME packet_strings COMMAND PacketStringTests)

# A login session on a loopback socket over TLS: the handshake and LOGIN3,
# with packets split across reads.
add_executable(LoginSessionTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/LoginSessionTests.cpp
    ../LoginServer/login_session.cpp
    ../LoginServer/login_auth.cpp
    ../LoginServer/login_hwid.cpp
    ../LoginServer/login_limiter.cpp
)
target_link_libraries(LoginSessionTests PRIVATE SERVER S_Core)
if(WIN32)
    target_link_libraries(LoginSessionTests PRIVATE ws2_32 bcrypt secur32 crypt32 shlwapi)
endif()
add_test(NAME login_session COMMAND LoginSessionTests)

# PeerSession over TLS on a loopback socket: a trusted client, a plaintext
# client, an outdated TLS client and a peer that never handshakes.
add_executable(TlsSessionTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/TlsSessionTests.cpp
)
target_link_libraries(TlsSessionTests PRIVATE SERVER S_Core)
if(WIN32)
    target_link_libraries(TlsSessionTests PRIVATE ws2_32 bcrypt secur32 crypt32 shlwapi)
endif()
add_test(NAME tls_session COMMAND TlsSessionTests)

# The server-to-server MAC, pinned to the value the previous implementation
# produced, plus nonces and timestamps.
add_executable(PeerAuthTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/PeerAuthTests.cpp
)
target_link_libraries(PeerAuthTests PRIVATE SERVER S_Core)
if(WIN32)
    target_link_libraries(PeerAuthTests PRIVATE ws2_32 bcrypt secur32 crypt32 shlwapi)
endif()
add_test(NAME peer_auth COMMAND PeerAuthTests)

# Core byte buffers under the asio session patterns: an output buffer that is
# rarely drained to empty, a reader left with partial packets, growth.
add_executable(BufferTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/BufferTests.cpp
)
target_link_libraries(BufferTests PRIVATE SERVER S_Core)
if(WIN32)
    target_link_libraries(BufferTests PRIVATE ws2_32 bcrypt secur32 crypt32 shlwapi)
endif()
add_test(NAME core_buffer COMMAND BufferTests)

# common/singleton.h: registration at the derived address, no copies, and a
# hard stop - also with NDEBUG - on a second instance or a use with none alive.
add_executable(SingletonTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/SingletonTests.cpp
)
target_link_libraries(SingletonTests PRIVATE SERVER)
add_test(NAME singleton COMMAND SingletonTests)

# The daily reward's DB chains over a scripted database that runs the queries
# in order, like the async worker: claims sent at once, one machine with two
# characters, a claim taken by another channel, the gift window reload.
add_executable(DailyRewardTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/DailyRewardTests.cpp
    command/daily_reward.cpp
)
target_link_libraries(DailyRewardTests PRIVATE SERVER EnTT::EnTT)
if(WIN32)
    target_link_libraries(DailyRewardTests PRIVATE ws2_32 bcrypt secur32 crypt32 shlwapi)
endif()
add_test(NAME daily_reward COMMAND DailyRewardTests)

# The SQL layer's prepared result path (StorePreparedResult) over a scripted
# double of the MySQL statement API: whole cells, NULLs, truncated values.
add_executable(AsyncSqlPreparedTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/AsyncSqlPreparedTests.cpp
)
target_link_libraries(AsyncSqlPreparedTests PRIVATE SERVER S_SQL)
if(WIN32)
    target_link_libraries(AsyncSqlPreparedTests PRIVATE ws2_32 bcrypt secur32 crypt32 shlwapi)
endif()
add_test(NAME async_sql_prepared COMMAND AsyncSqlPreparedTests)

# Canonical item_award -> item materialization, including lost COMMIT, real
# item schema without auto-increment, full boxes and a cold process after crash.
add_executable(ItemAwardDeliveryTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/ItemAwardDeliveryTests.cpp
    ../Database/ItemAwardDelivery.cpp
    ../Database/ItemAwardScan.cpp
)
target_link_libraries(ItemAwardDeliveryTests PRIVATE SERVER S_SQL S_Core)
if(WIN32)
    target_link_libraries(ItemAwardDeliveryTests PRIVATE ws2_32 bcrypt secur32 crypt32 shlwapi)
endif()
if(ENABLE_ASAN AND EXISTS "${AURIGA_ASAN_RUNTIME_DLL}")
    add_custom_command(TARGET ItemAwardDeliveryTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${AURIGA_ASAN_RUNTIME_DLL}" "$<TARGET_FILE_DIR:ItemAwardDeliveryTests>"
    )
endif()
add_test(NAME item_award_delivery COMMAND ItemAwardDeliveryTests --unit)

# Opt-in real database tests. The runner creates its own datadir and loopback
# port; it never connects to an existing MariaDB service.
add_executable(AsyncSqlMariaDbTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/AsyncSqlMariaDbTests.cpp
)
target_link_libraries(AsyncSqlMariaDbTests PRIVATE SERVER S_SQL S_Core)
if(WIN32)
    target_link_libraries(AsyncSqlMariaDbTests PRIVATE ws2_32 bcrypt secur32 crypt32 shlwapi)
endif()
set(AURIGA_TEST_MARIADB_BIN_DIR "" CACHE PATH "Isolated MariaDB test distribution bin directory (Windows)")
if(WIN32 AND AURIGA_TEST_MARIADB_BIN_DIR)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(NAME async_sql_mariadb_isolated
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/server/AsyncSqlMariaDbTests.py
            --server-bin-dir ${AURIGA_TEST_MARIADB_BIN_DIR}
            --test-executable $<TARGET_FILE:AsyncSqlMariaDbTests>
            --work-dir ${CMAKE_BINARY_DIR}/mission-sql-isolated
    )
    add_test(NAME player_session_mariadb
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/server/AsyncSqlMariaDbTests.py
            --server-bin-dir ${AURIGA_TEST_MARIADB_BIN_DIR}
            --test-executable $<TARGET_FILE:PlayerSessionTests>
            --work-dir ${CMAKE_BINARY_DIR}/player-session-isolated
            --drop-commit-count 0
    )
    set_tests_properties(player_session_mariadb PROPERTIES TIMEOUT 180 LABELS "server;integration")
    add_test(NAME player_cache_drain_mariadb
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/server/AsyncSqlMariaDbTests.py
            --server-bin-dir ${AURIGA_TEST_MARIADB_BIN_DIR}
            --test-executable $<TARGET_FILE:PlayerCacheDrainTests>
            --work-dir ${CMAKE_BINARY_DIR}/player-cache-drain-isolated
            --test-argument ${CMAKE_SOURCE_DIR}/db/schema.sql
            --drop-commit-count 0
    )
    set_tests_properties(player_cache_drain_mariadb PROPERTIES TIMEOUT 180 LABELS "server;integration")
    add_test(NAME mission_persistence_mariadb
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/server/AsyncSqlMariaDbTests.py
            --server-bin-dir ${AURIGA_TEST_MARIADB_BIN_DIR}
            --test-executable $<TARGET_FILE:MissionPersistenceTests>
            --work-dir ${CMAKE_BINARY_DIR}/mission-persistence-isolated
            --test-argument ${CMAKE_SOURCE_DIR}/db/migrations/2026-10-25_mission_engine_persistence.sql
            --test-argument ${CMAKE_SOURCE_DIR}/db/migrations/2026-10-25_mission_engine_actions.sql
            --test-argument ${CMAKE_SOURCE_DIR}/db/schema.sql
            --drop-commit-count 7
            --followup-argument=--cold-check
            --crash-server-before-followup
    )
    add_test(NAME item_award_delivery_mariadb
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/server/AsyncSqlMariaDbTests.py
            --server-bin-dir ${AURIGA_TEST_MARIADB_BIN_DIR}
            --test-executable $<TARGET_FILE:ItemAwardDeliveryTests>
            --work-dir ${CMAKE_BINARY_DIR}/item-award-delivery-isolated
            --test-argument ${CMAKE_SOURCE_DIR}/db/schema.sql
            --followup-argument=--cold-check
            --crash-server-before-followup
    )
endif()

add_executable(MarriageLifecycleTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/MarriageLifecycleTests.cpp
    social/marriage.cpp
    ecs/PIDRegistry.cpp
)
target_link_libraries(MarriageLifecycleTests PRIVATE SERVER EnTT::EnTT LUA)
add_test(NAME marriage_lifecycle COMMAND MarriageLifecycleTests)

add_executable(PartyLifecycleTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/PartyLifecycleTests.cpp
    social/party.cpp
)
target_link_libraries(PartyLifecycleTests PRIVATE SERVER EnTT::EnTT LUA)
add_test(NAME party_lifecycle COMMAND PartyLifecycleTests)

add_executable(DungeonLifecycleTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/DungeonLifecycleTests.cpp
    dungeon/dungeon.cpp
    dungeon/PyramidDungeonRazor93.cpp
    dungeon/OrcsDungeon.cpp
    dungeon/TritonTempleDungeon.cpp
    dungeon/ValentineDungeon.cpp
    dungeon/EasterDungeon.cpp
    dungeon/NightmareDungeonRazor93.cpp
    dungeon/RuneDungeon.cpp
    dungeon/Halloween2022Dungeon.cpp
    dungeon/VikingDungeon.cpp
)
target_link_libraries(DungeonLifecycleTests PRIVATE SERVER EnTT::EnTT LUA)
add_test(NAME dungeon_lifecycle COMMAND DungeonLifecycleTests)

add_executable(ShopLifecycleTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/ShopLifecycleTests.cpp
    social/shop.cpp
    social/shop_manager.cpp
    core/json_group_tree.cpp
    core/JsonFile.cpp
    ../Base/grid.cpp
    world/event.cpp
    world/event_queue.cpp
)
target_link_libraries(ShopLifecycleTests PRIVATE SERVER EnTT::EnTT LUA S_Core)
add_test(NAME shop_lifecycle COMMAND ShopLifecycleTests)

add_executable(MountLifecycleTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/MountLifecycleTests.cpp
    pet/PetSystem.cpp
    pet/New_PetSystem.cpp
)
target_link_libraries(MountLifecycleTests PRIVATE SERVER EnTT::EnTT)
add_test(NAME mount_lifecycle COMMAND MountLifecycleTests)

add_executable(SocialRelationTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/SocialRelationTests.cpp
    ecs/systems/SocialSystem.cpp
    world/event.cpp
    world/event_queue.cpp
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
    item/safebox.cpp
    ../Base/grid.cpp
)
target_link_libraries(SafeboxLifecycleTests PRIVATE SERVER EnTT::EnTT)
add_test(NAME safebox_lifecycle COMMAND SafeboxLifecycleTests)

add_executable(ItemManagerLifecycleTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/ItemManagerLifecycleTests.cpp
    item/item_manager.cpp
    item/safebox.cpp
    ../Base/grid.cpp
)
target_link_libraries(ItemManagerLifecycleTests PRIVATE SERVER EnTT::EnTT LUA S_Core)
add_test(NAME item_manager_lifecycle COMMAND ItemManagerLifecycleTests)

add_executable(ItemRuntimeTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/ItemRuntimeTests.cpp
    ecs/systems/ItemSystem.cpp
    ecs/ItemRegistry.cpp
)
target_link_libraries(ItemRuntimeTests PRIVATE SERVER EnTT::EnTT LUA S_Core)
if(MSVC)
    target_compile_options(ItemRuntimeTests PRIVATE /Gy)
    target_link_options(ItemRuntimeTests PRIVATE /OPT:REF /OPT:NOICF)
endif()
add_test(NAME item_runtime COMMAND ItemRuntimeTests)

add_executable(QuickslotTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/QuickslotTests.cpp
    ecs/systems/InventorySystem.cpp
)
target_link_libraries(QuickslotTests PRIVATE SERVER EnTT::EnTT LUA S_Core)
add_test(NAME quickslots COMMAND QuickslotTests)

add_executable(SpatialLifecycleTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/SpatialLifecycleTests.cpp
    entity/movement_input.cpp
    world/sectree.cpp
    ecs/CBuildingRegistry.cpp
    ecs/services/SpatialService.cpp
    ecs/systems/VisibilitySystem.cpp
    ecs/systems/MovementSystem.cpp
    world/event.cpp
    world/event_queue.cpp
)
target_link_libraries(SpatialLifecycleTests PRIVATE SERVER EnTT::EnTT LUA S_Core)
if(WIN32)
    target_link_libraries(SpatialLifecycleTests PRIVATE ws2_32 bcrypt secur32 crypt32 shlwapi)
endif()
if(MSVC)
    target_compile_options(SpatialLifecycleTests PRIVATE /Gy)
    target_link_options(SpatialLifecycleTests PRIVATE /OPT:REF /OPT:NOICF)
else()
    target_compile_options(SpatialLifecycleTests PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(SpatialLifecycleTests PRIVATE -Wl,--gc-sections)
endif()
add_test(NAME spatial_lifecycle COMMAND SpatialLifecycleTests)

add_executable(BuildingLifecycleTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/BuildingLifecycleTests.cpp
    world/building.cpp
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
    combat/buff_on_attributes.cpp
)
target_link_libraries(BuffAttributeTests PRIVATE SERVER EnTT::EnTT LUA)
if(MSVC)
    target_compile_options(BuffAttributeTests PRIVATE /Gy)
    target_link_options(BuffAttributeTests PRIVATE /OPT:REF /OPT:NOICF)
endif()
add_test(NAME buff_attributes COMMAND BuffAttributeTests)

add_executable(EntityNetworkDispatchTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/EntityNetworkDispatchTests.cpp
    entity/entity.cpp
    ecs/services/EntityNetworkDispatch.cpp
    ecs/systems/NetworkSyncSystem.cpp
)
target_link_libraries(EntityNetworkDispatchTests PRIVATE SERVER EnTT::EnTT LUA S_Core)
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
    combat/battle.cpp
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
    pet/horsename_manager.cpp
)
target_link_libraries(AffectLifecycleTests PRIVATE SERVER EnTT::EnTT LUA S_Core)
if(WIN32)
    target_link_libraries(AffectLifecycleTests PRIVATE ws2_32 bcrypt secur32 crypt32 shlwapi)
endif()
add_test(NAME affect_lifecycle COMMAND AffectLifecycleTests)

add_executable(EventLifecycleTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/EventLifecycleTests.cpp
    world/event.cpp
    world/event_queue.cpp
)
target_link_libraries(EventLifecycleTests PRIVATE SERVER EnTT::EnTT)
add_test(NAME event_lifecycle COMMAND EventLifecycleTests)

add_executable(CharacterManagerTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/CharacterManagerTests.cpp
    entity/char_manager.cpp
    ecs/VIDRegistry.cpp
    ecs/PIDRegistry.cpp
    world/event.cpp
    world/event_queue.cpp
)
target_link_libraries(CharacterManagerTests PRIVATE SERVER EnTT::EnTT LUA)
if(WIN32)
    target_link_libraries(CharacterManagerTests PRIVATE ws2_32 bcrypt secur32 crypt32 shlwapi)
endif()
add_test(NAME character_manager COMMAND CharacterManagerTests)


add_executable(GmCommandTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/GmCommandTests.cpp
    command/cmd_gm.cpp
    combat/skill_power.cpp
    ecs/systems/SkillSystem.cpp
    ecs/PIDRegistry.cpp
    core/utils.cpp
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
    social/exchange.cpp
)
target_link_libraries(ExchangeTests PRIVATE SERVER EnTT::EnTT LUA S_Core)
add_test(NAME exchange COMMAND ExchangeTests)

# These suites replace the SQL connection/worker, but SQLMsg destruction still
# uses the production drain cancellation helper. Do not link a live SQL worker.
foreach(_sql_seam_test CombatStateTests DailyRewardTests DungeonLifecycleTests ExchangeTests GuildMemberLifecycleTests
        ItemAttributeTests ItemManagerLifecycleTests ItemRuntimeTests LoginAuthTests LoginSessionTests
        MotionTimingTests MountLifecycleTests OXEventTests QuickslotTests ShopLifecycleTests)
    target_sources(${_sql_seam_test} PRIVATE ../SQL/QueueDrain.cpp)
    target_link_libraries(${_sql_seam_test} PRIVATE Threads::Threads)
endforeach()

# Exercise the same generation-aware diagnostic implementation as GameServer.
# The isolated event suite has no diagnostic dependency.
foreach(_ecs_test ItemAttributeTests MountLifecycleTests SafeboxLifecycleTests ItemManagerLifecycleTests QuickslotTests PointCalculationTests CombatStateTests AffectLifecycleTests CharacterManagerTests GmCommandTests ExchangeTests)
    target_sources(${_ecs_test} PRIVATE ecs/EcsDiagnostics.cpp)
endforeach()

if(ENABLE_ASAN AND EXISTS "${AURIGA_ASAN_RUNTIME_DLL}")
    foreach(_asan_target LuaSourceTests GameServer ServerConfigTests DatabaseTimeTests PrivilegeTimeTests OXEventTests QuestRuntimeTests QuestDataTests HorseRuntimeTests ItemAttributeTests MountLifecycleTests SafeboxLifecycleTests ItemManagerLifecycleTests ItemRuntimeTests QuickslotTests SpatialLifecycleTests BuildingLifecycleTests BuffAttributeTests EntityNetworkDispatchTests PointCalculationTests CombatStateTests AffectLifecycleTests EventLifecycleTests CharacterManagerTests GmCommandTests ExchangeTests SocialRelationTests AsyncSqlPreparedTests AsyncSqlMariaDbTests)
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

# Build every registered server test explicitly, including EXCLUDE_FROM_ALL
add_executable(HerbalismRulesTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/HerbalismRulesTests.cpp)
add_test(NAME herbalism_rules COMMAND HerbalismRulesTests)

add_executable(RetiredContentTests EXCLUDE_FROM_ALL ${CMAKE_SOURCE_DIR}/tests/server/RetiredContentTests.cpp)
target_link_libraries(RetiredContentTests PRIVATE SERVER)
add_test(NAME retired_content COMMAND RetiredContentTests)
if(ENABLE_ASAN AND EXISTS "${AURIGA_ASAN_RUNTIME_DLL}")
    add_custom_command(TARGET RetiredContentTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${AURIGA_ASAN_RUNTIME_DLL}"
            "$<TARGET_FILE_DIR:RetiredContentTests>")
endif()

add_executable(MissionPassRulesTests EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/server/MissionPassRulesTests.cpp)
find_path(AURIGA_MISSION_PASS_JSON_INCLUDE NAMES nlohmann/json.hpp REQUIRED)
target_include_directories(MissionPassRulesTests PRIVATE ${AURIGA_MISSION_PASS_JSON_INCLUDE})
add_test(NAME mission_pass_rules COMMAND MissionPassRulesTests
    ${CMAKE_SOURCE_DIR}/config/mission_pass.json.example)

# Build every registered server test explicitly, including EXCLUDE_FROM_ALL
# executables. CTest is enabled at the root, also for a fresh checkout.
add_executable(SpyQuestRulesTests EXCLUDE_FROM_ALL ${CMAKE_SOURCE_DIR}/tests/server/SpyQuestRulesTests.cpp)
target_link_libraries(SpyQuestRulesTests PRIVATE EnTT::EnTT)
add_test(NAME spy_quest_rules COMMAND SpyQuestRulesTests)
add_executable(CaravanRulesTests EXCLUDE_FROM_ALL ${CMAKE_SOURCE_DIR}/tests/server/CaravanRulesTests.cpp)
add_test(NAME caravan_rules COMMAND CaravanRulesTests)
get_property(_server_test_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
list(FILTER _server_test_targets INCLUDE REGEX "Tests$")
add_custom_target(ServerTests DEPENDS ${_server_test_targets})
add_dependencies(ServerTests ServerPackageCheck MissionCheck MissionCompiler ItemKeyTests ItemCatalogTests RefinementProfileTests BonusProfileTests AccessorySocketTests ConsumableTests CatalogSnapshotTests CatalogSqlReaderTests CatalogEditorTool)
get_property(_server_tests DIRECTORY PROPERTY TESTS)
set_tests_properties(${_server_tests} PROPERTIES LABELS server TIMEOUT 60)
if(TEST async_sql_mariadb_isolated)
    set_tests_properties(async_sql_mariadb_isolated PROPERTIES TIMEOUT 180 LABELS "server;integration")
    set_tests_properties(player_session_mariadb player_cache_drain_mariadb PROPERTIES TIMEOUT 180 LABELS "server;integration")
    set_tests_properties(mission_persistence_mariadb PROPERTIES TIMEOUT 180 LABELS "server;integration")
    set_tests_properties(item_award_delivery_mariadb PROPERTIES TIMEOUT 180 LABELS "server;integration")
endif()
