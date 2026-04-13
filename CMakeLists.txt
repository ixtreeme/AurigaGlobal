cmake_minimum_required(VERSION 3.20)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.27")
    cmake_policy(SET CMP0141 NEW)
    if(MSVC)
        set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
            "$<IF:$<CONFIG:Debug,RelWithDebInfo>,EditAndContinue,ProgramDatabase>")
    endif()
endif()

option(ENABLE_ASAN "Enable AddressSanitizer" OFF)

set(VCPKG_TRIPLET_TYPE "static" CACHE STRING "Choose vcpkg triplet type (static/dynamic)")
set_property(CACHE VCPKG_TRIPLET_TYPE PROPERTY STRINGS static dynamic)

# ============================================================
# MSVC runtime: statikus modban /MT, dinamikus modban /MD
# ASAN kotelezoen /MD-t igenyel.
# ============================================================
if(ENABLE_ASAN)
    set(CMAKE_MSVC_RUNTIME_LIBRARY
        "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
        CACHE STRING "MSVC runtime library" FORCE
    )
    if(VCPKG_TARGET_TRIPLET STREQUAL "x64-windows-static")
        set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "vcpkg triplet" FORCE)
        set(VCPKG_TRIPLET_TYPE "dynamic" CACHE STRING "" FORCE)
    endif()
elseif(VCPKG_TRIPLET_TYPE STREQUAL "static")
    set(CMAKE_MSVC_RUNTIME_LIBRARY
        "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        CACHE STRING "MSVC runtime library" FORCE
    )
else()
    set(CMAKE_MSVC_RUNTIME_LIBRARY
        "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
        CACHE STRING "MSVC runtime library" FORCE
    )
endif()

# ============================================================
# Visual Studio vcpkg globals
# ============================================================
if(MSVC AND CMAKE_GENERATOR MATCHES "Visual Studio")
    set(_auriga_vs_vcpkg_entry "VcpkgConfiguration=$(Configuration)")
    set(_auriga_vs_has_vcpkg_config OFF)
    foreach(_auriga_vs_entry IN LISTS CMAKE_VS_GLOBALS)
        if(_auriga_vs_entry STREQUAL _auriga_vs_vcpkg_entry)
            set(_auriga_vs_has_vcpkg_config ON)
            break()
        endif()
    endforeach()
    if(NOT _auriga_vs_has_vcpkg_config)
        list(APPEND CMAKE_VS_GLOBALS "${_auriga_vs_vcpkg_entry}")
    endif()
endif()

# ============================================================
# PROJECT - BootstrapVcpkg.cmake UTAN jon (eredeti sorrend)
# ============================================================
project(Auriga_Global VERSION 0.1 LANGUAGES C CXX)

# ============================================================
# vcpkg bootstrap + triplet + Directory.Build.props generalas
# ============================================================
include(cmake/BootstrapVcpkg.cmake)

# ============================================================
# Python fajlok Visual Studio-ban
# ============================================================
file(GLOB_RECURSE AURIGA_ROOT_PYTHON CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/root/*.py")
if(AURIGA_ROOT_PYTHON)
    add_custom_target(auriga_root_python_files ALL SOURCES ${AURIGA_ROOT_PYTHON})
    set_target_properties(auriga_root_python_files PROPERTIES FOLDER Root)
    source_group(TREE ${CMAKE_SOURCE_DIR}/root FILES ${AURIGA_ROOT_PYTHON})
endif()

# ============================================================
# vcpkg csomagok telepitese
# ============================================================
vcpkg_require(
    stb
    lz4
	curl
	zstd
    xxhash
    python2
    cryptopp
    libconfig
    libsodium
    rapidjson
    miniaudio
    directxsdk
    discord-rpc
    libjpeg-turbo
    nlohmann-json
    boost-foreach
    boost-exception
    boost-algorithm
    boost-property-tree
    mariadb-connector-cpp
)

# ============================================================
# C++ standard
# ============================================================
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# ============================================================
# Globalis forditasi beallitasok
# ============================================================
if(MSVC)
    add_compile_definitions(
        _CRT_SECURE_NO_WARNINGS
        _WINSOCK_DEPRECATED_NO_WARNINGS
        $<$<CONFIG:Debug>:_DEBUG>
        $<$<BOOL:${WIN32}>:WIN32>
        $<$<BOOL:${UNIX}>:UNIX>
    )

    if(ENABLE_ASAN)
        add_compile_options(
            /MP
            /GS
            /sdl
            /Qspectre
            /fsanitize=address
            /Zi
        )
        add_compile_definitions(
            _DISABLE_VECTOR_ANNOTATION
            _DISABLE_STRING_ANNOTATION
        )
        add_link_options(
            /NXCOMPAT
            /DYNAMICBASE
            /HIGHENTROPYVA
            /SAFESEH:NO
        )
    else()
        add_compile_options(
            /MP
            /GS
            /sdl
            /guard:cf
            /Qspectre
        )
        add_link_options(
            /NXCOMPAT
            /DYNAMICBASE
            /HIGHENTROPYVA
            /SAFESEH:NO
        )
        add_compile_options($<$<CONFIG:Debug>:/RTC1>)
        add_link_options(
            $<$<CONFIG:Release>:/LTCG:INCREMENTAL>
            $<$<CONFIG:RelWithDebInfo>:/LTCG:INCREMENTAL>
            $<$<CONFIG:MinSizeRel>:/LTCG:INCREMENTAL>
        )
    endif()
endif()

add_subdirectory(SRC)
