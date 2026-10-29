cmake_minimum_required(VERSION 3.25)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(AurigaItemDomain LANGUAGES CXX)
    enable_testing()
endif()
get_filename_component(_item_repository "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

# Kept outside Server's recursive source glob. This has no runtime dependency
# on SERVER, EnTT, SQL, Lua, or either game's existing evaluator.
add_library(AurigaItemDomain STATIC EXCLUDE_FROM_ALL ItemWireCodec.cpp)
target_compile_features(AurigaItemDomain PUBLIC cxx_std_23)
set_target_properties(AurigaItemDomain PROPERTIES CXX_EXTENSIONS OFF FOLDER "ItemCombat")
target_include_directories(AurigaItemDomain PUBLIC "${_item_repository}/SRC/Server/common")

set(_category_generator "${_item_repository}/SRC/Tools/ItemCatalogBuilder/generate_item_categories.py")
set(_category_registry "${_item_repository}/config/item_combat/category_registry.json")
set(_category_header "${_item_repository}/SRC/Server/common/item/ItemCategory.hpp")
set(_category_seed "${_item_repository}/db/migrations/item_combat/002_category_seed.sql")
add_custom_target(ItemCategoryRegistryCheck
    COMMAND "${Python3_EXECUTABLE}" -B "${_category_generator}"
        --registry "${_category_registry}" --output "${_category_header}" --sql-output "${_category_seed}" --check
    VERBATIM)
add_dependencies(AurigaItemDomain ItemCategoryRegistryCheck)
add_custom_target(ItemBonusRegistryCheck
    COMMAND "${Python3_EXECUTABLE}" -B "${_item_repository}/SRC/Tools/ItemCatalogBuilder/generate_bonus_registry.py" --check
    VERBATIM)

if(NOT TARGET OpenSSL::Crypto)
    find_package(OpenSSL REQUIRED COMPONENTS Crypto)
endif()
find_package(Threads REQUIRED)
add_library(AurigaItemCatalog STATIC EXCLUDE_FROM_ALL ItemCatalog.cpp RefinementProfiles.cpp RefinementRecipes.cpp BonusProfiles.cpp AccessorySockets.cpp Consumables.cpp AutomaticRecovery.cpp TimedEffects.cpp ToggleEffects.cpp SkillBooks.cpp QuestItems.cpp SocketStones.cpp Costumes.cpp GatheringItems.cpp PolymorphItems.cpp SoulItems.cpp CompanionItems.cpp GrowthPets.cpp Containers.cpp CurrencyTokens.cpp CatalogHash.cpp CatalogSnapshot.cpp CatalogTables.cpp)
target_include_directories(AurigaItemCatalog PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
add_dependencies(AurigaItemCatalog ItemBonusRegistryCheck)
target_link_libraries(AurigaItemCatalog PUBLIC AurigaItemDomain PRIVATE OpenSSL::Crypto)
target_compile_features(AurigaItemCatalog PUBLIC cxx_std_23)
set_target_properties(AurigaItemCatalog PROPERTIES CXX_EXTENSIONS OFF FOLDER "ItemCombat")
foreach(_catalog_test ItemCatalogTests RefinementProfileTests BonusProfileTests AccessorySocketTests ConsumableTests AutomaticRecoveryTests TimedEffectTests ToggleEffectTests SkillBookTests RefinementRecipeTests QuestItemTests SocketStoneTests CostumeTests GatheringItemTests PolymorphItemTests SoulItemTests CompanionItemTests GrowthPetTests ContainerTests CurrencyTokenTests CatalogSnapshotTests)
    add_executable(${_catalog_test} EXCLUDE_FROM_ALL "${_item_repository}/tests/server/${_catalog_test}.cpp")
    target_link_libraries(${_catalog_test} PRIVATE AurigaItemCatalog)
endforeach()
target_link_libraries(ItemCatalogTests PRIVATE Threads::Threads)

add_executable(ItemKeyTests EXCLUDE_FROM_ALL "${_item_repository}/tests/server/ItemKeyTests.cpp")
target_link_libraries(ItemKeyTests PRIVATE AurigaItemDomain)
if(TARGET auriga_sanitizers)
    target_link_libraries(AurigaItemDomain PRIVATE auriga_sanitizers)
    target_link_libraries(ItemKeyTests PRIVATE auriga_sanitizers)
    target_link_libraries(AurigaItemCatalog PRIVATE auriga_sanitizers)
    target_link_libraries(ItemCatalogTests PRIVATE auriga_sanitizers)
    target_link_libraries(RefinementProfileTests PRIVATE auriga_sanitizers)
    target_link_libraries(BonusProfileTests PRIVATE auriga_sanitizers)
    target_link_libraries(AccessorySocketTests PRIVATE auriga_sanitizers)
    target_link_libraries(ConsumableTests PRIVATE auriga_sanitizers)
    target_link_libraries(AutomaticRecoveryTests PRIVATE auriga_sanitizers)
    target_link_libraries(TimedEffectTests PRIVATE auriga_sanitizers)
    target_link_libraries(ToggleEffectTests PRIVATE auriga_sanitizers)
    target_link_libraries(SkillBookTests PRIVATE auriga_sanitizers)
    target_link_libraries(RefinementRecipeTests PRIVATE auriga_sanitizers)
    target_link_libraries(QuestItemTests PRIVATE auriga_sanitizers)
    target_link_libraries(SocketStoneTests PRIVATE auriga_sanitizers)
    target_link_libraries(CostumeTests PRIVATE auriga_sanitizers)
    target_link_libraries(GatheringItemTests PRIVATE auriga_sanitizers)
    target_link_libraries(PolymorphItemTests PRIVATE auriga_sanitizers)
    target_link_libraries(SoulItemTests PRIVATE auriga_sanitizers)
    target_link_libraries(CompanionItemTests PRIVATE auriga_sanitizers)
    target_link_libraries(GrowthPetTests PRIVATE auriga_sanitizers)
    target_link_libraries(ContainerTests PRIVATE auriga_sanitizers)
    target_link_libraries(CurrencyTokenTests PRIVATE auriga_sanitizers)
    target_link_libraries(CatalogSnapshotTests PRIVATE auriga_sanitizers)
endif()
if(ENABLE_ASAN AND EXISTS "${AURIGA_ASAN_RUNTIME_DLL}")
    foreach(_item_test ItemKeyTests ItemCatalogTests RefinementProfileTests BonusProfileTests AccessorySocketTests ConsumableTests AutomaticRecoveryTests TimedEffectTests ToggleEffectTests SkillBookTests RefinementRecipeTests QuestItemTests SocketStoneTests CostumeTests GatheringItemTests PolymorphItemTests SoulItemTests CompanionItemTests GrowthPetTests ContainerTests CurrencyTokenTests CatalogSnapshotTests)
        add_custom_command(TARGET ${_item_test} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${AURIGA_ASAN_RUNTIME_DLL}" "$<TARGET_FILE_DIR:${_item_test}>" VERBATIM)
    endforeach()
endif()
add_test(NAME item_key COMMAND ItemKeyTests)
add_test(NAME item_catalog COMMAND ItemCatalogTests)
add_test(NAME refinement_profiles COMMAND RefinementProfileTests)
add_test(NAME bonus_profiles COMMAND BonusProfileTests)
add_test(NAME accessory_sockets COMMAND AccessorySocketTests)
add_test(NAME consumables COMMAND ConsumableTests)
add_test(NAME automatic_recovery COMMAND AutomaticRecoveryTests)
add_test(NAME timed_effects COMMAND TimedEffectTests)
add_test(NAME toggle_effects COMMAND ToggleEffectTests)
add_test(NAME skill_books COMMAND SkillBookTests)
add_test(NAME refinement_recipes COMMAND RefinementRecipeTests)
add_test(NAME quest_items COMMAND QuestItemTests)
add_test(NAME socket_stones COMMAND SocketStoneTests)
add_test(NAME costumes COMMAND CostumeTests)
add_test(NAME gathering_items COMMAND GatheringItemTests)
add_test(NAME soul_items COMMAND SoulItemTests)
add_test(NAME companion_items COMMAND CompanionItemTests)
add_test(NAME growth_pet_progression COMMAND GrowthPetTests)
add_test(NAME polymorph_items COMMAND PolymorphItemTests)
add_test(NAME legacy_item_migration COMMAND "${Python3_EXECUTABLE}" -B "${_item_repository}/tests/server/LegacyItemMigrationTests.py")
if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_test(NAME quest_token_migration COMMAND "${Python3_EXECUTABLE}" -B
        "${_item_repository}/tests/server/QuestTokenMigrationTests.py" --helper "$<TARGET_FILE:CatalogEditorTool>")
    set_tests_properties(quest_token_migration PROPERTIES LABELS server TIMEOUT 60)
endif()
add_test(NAME containers COMMAND ContainerTests)
add_test(NAME currency_tokens COMMAND CurrencyTokenTests)
add_test(NAME catalog_sql_snapshot COMMAND CatalogSnapshotTests)
add_test(NAME item_bonus_generator COMMAND "${Python3_EXECUTABLE}" -B
    "${_item_repository}/tests/server/ItemBonusGeneratorTests.py")
add_test(NAME item_catalog_digest COMMAND "${Python3_EXECUTABLE}" -B
    "${_item_repository}/tests/server/ItemCatalogDigestOracle.py" --catalog-executable "$<TARGET_FILE:ItemCatalogTests>" --refinement-executable "$<TARGET_FILE:RefinementRecipeTests>" --quest-executable "$<TARGET_FILE:QuestItemTests>" --stone-executable "$<TARGET_FILE:SocketStoneTests>" --costume-executable "$<TARGET_FILE:CostumeTests>" --gathering-executable "$<TARGET_FILE:GatheringItemTests>" --polymorph-executable "$<TARGET_FILE:PolymorphItemTests>" --soul-executable "$<TARGET_FILE:SoulItemTests>" --companion-executable "$<TARGET_FILE:CompanionItemTests>" --growth-executable "$<TARGET_FILE:GrowthPetTests>")
add_test(NAME item_category_generator COMMAND "${Python3_EXECUTABLE}" -B
    "${_item_repository}/tests/server/ItemCategoryGeneratorTests.py")
set_tests_properties(item_key item_catalog refinement_profiles bonus_profiles accessory_sockets consumables automatic_recovery timed_effects toggle_effects skill_books refinement_recipes quest_items socket_stones costumes gathering_items polymorph_items soul_items companion_items growth_pet_progression legacy_item_migration containers currency_tokens catalog_sql_snapshot item_bonus_generator item_catalog_digest item_category_generator PROPERTIES LABELS server TIMEOUT 60)

# A standalone build can be nested below the main build directory. Do not
# inherit its Directory.Build.props/targets and implicitly autolink unrelated
# vcpkg libraries or override the selected CRT. CMake owns this module's complete
# dependency graph; the vcpkg toolchain still performs explicit app-local copies.
if(MSVC)
    foreach(_item_target AurigaItemDomain AurigaItemCatalog ItemKeyTests ItemCatalogTests RefinementProfileTests BonusProfileTests AccessorySocketTests ConsumableTests AutomaticRecoveryTests TimedEffectTests ToggleEffectTests SkillBookTests RefinementRecipeTests QuestItemTests SocketStoneTests CostumeTests GatheringItemTests PolymorphItemTests SoulItemTests CompanionItemTests GrowthPetTests ContainerTests CurrencyTokenTests CatalogSnapshotTests ItemCategoryRegistryCheck ItemBonusRegistryCheck)
        set_target_properties(${_item_target} PROPERTIES
            VS_GLOBAL_ImportDirectoryBuildProps false
            VS_GLOBAL_ImportDirectoryBuildTargets false)
    endforeach()
endif()

# The pure domain and decoder remain standalone. SQL transport is a separate
# opt-in library reusing the existing server connection/transaction engine.
if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_library(AurigaItemCatalogSql STATIC EXCLUDE_FROM_ALL CatalogSqlReader.cpp)
    target_link_libraries(AurigaItemCatalogSql PUBLIC AurigaItemCatalog S_SQL PRIVATE S_Core)
    target_compile_features(AurigaItemCatalogSql PUBLIC cxx_std_23)
    add_executable(CatalogEditorTool EXCLUDE_FROM_ALL CatalogEditorTool.cpp)
    target_link_libraries(CatalogEditorTool PRIVATE AurigaItemCatalogSql)
    if(WIN32)
        target_link_libraries(CatalogEditorTool PRIVATE ws2_32 bcrypt secur32 crypt32 shlwapi)
    endif()
    if(TARGET auriga_sanitizers)
        target_link_libraries(AurigaItemCatalogSql PRIVATE auriga_sanitizers)
        target_link_libraries(CatalogEditorTool PRIVATE auriga_sanitizers)
    endif()
    add_executable(CatalogSqlReaderTests EXCLUDE_FROM_ALL "${_item_repository}/tests/server/CatalogSqlReaderTests.cpp")
    target_link_libraries(CatalogSqlReaderTests PRIVATE AurigaItemCatalogSql)
    if(WIN32)
        target_link_libraries(CatalogSqlReaderTests PRIVATE ws2_32 bcrypt secur32 crypt32 shlwapi)
    endif()
    if(TARGET auriga_sanitizers)
        target_link_libraries(CatalogSqlReaderTests PRIVATE auriga_sanitizers)
    endif()
    if(ENABLE_ASAN AND EXISTS "${AURIGA_ASAN_RUNTIME_DLL}")
        add_custom_command(TARGET CatalogEditorTool POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${AURIGA_ASAN_RUNTIME_DLL}" "$<TARGET_FILE_DIR:CatalogEditorTool>" VERBATIM)
        add_custom_command(TARGET CatalogSqlReaderTests POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${AURIGA_ASAN_RUNTIME_DLL}" "$<TARGET_FILE_DIR:CatalogSqlReaderTests>" VERBATIM)
    endif()
    add_test(NAME catalog_sql_completion COMMAND CatalogSqlReaderTests --completion-only)
    set_tests_properties(catalog_sql_completion PROPERTIES LABELS server TIMEOUT 60)
    # Editor model, native validation and (when PySide6 is present) the Qt form.
    add_test(NAME catalog_editor COMMAND "${Python3_EXECUTABLE}" -B "${_item_repository}/tests/server/CatalogEditorTests.py"
        --tool "$<TARGET_FILE:CatalogEditorTool>" OfflineModelTests EditorPageTests RefinementPanelTests)
    set_tests_properties(catalog_editor PROPERTIES LABELS server TIMEOUT 120)
endif()

set(AURIGA_TEST_MARIADB_BIN_DIR "" CACHE PATH "Isolated MariaDB test distribution bin directory (Windows)")
if(WIN32 AND AURIGA_TEST_MARIADB_BIN_DIR)
    add_test(NAME item_combat_schema_foundation
        COMMAND "${Python3_EXECUTABLE}" -B "${_item_repository}/tests/server/ItemCombatSchemaTests.py"
            --mariadb-bin "${AURIGA_TEST_MARIADB_BIN_DIR}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/item-combat-sql-isolated")
    set_tests_properties(item_combat_schema_foundation PROPERTIES LABELS "server;integration" TIMEOUT 180)
    if(TARGET CatalogSqlReaderTests)
        add_test(NAME catalog_sql_mariadb
            COMMAND "${Python3_EXECUTABLE}" -B "${_item_repository}/tests/server/CatalogSqlReaderTests.py"
                --mariadb-bin "${AURIGA_TEST_MARIADB_BIN_DIR}"
                --test-executable "$<TARGET_FILE:CatalogSqlReaderTests>"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/catalog-loader-sql-isolated")
        set_tests_properties(catalog_sql_mariadb PROPERTIES LABELS "server;integration" TIMEOUT 180)
        add_test(NAME catalog_editor_mariadb
            COMMAND "${Python3_EXECUTABLE}" -B "${_item_repository}/tests/server/CatalogEditorTests.py"
                --tool "$<TARGET_FILE:CatalogEditorTool>"
                --mariadb-bin "${AURIGA_TEST_MARIADB_BIN_DIR}"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/catalog-editor-sql-isolated" SqlCommitTests)
        set_tests_properties(catalog_editor_mariadb PROPERTIES LABELS "server;integration" TIMEOUT 180)
    endif()
endif()
