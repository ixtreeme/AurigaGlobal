# cmake/AurigaSystemDeps.cmake
#
# System dependency discovery for platforms where vcpkg is not used
# (FreeBSD, Linux, ...). Creates the INTERFACE target `auriga_system_deps`
# that carries the include directories and link libraries required by the
# server and tool targets.
#
# FreeBSD packages:
#   devel/entt                      (EnTT CMake config, seen by GameServer)
#   devel/stb                       (stb_image.h / stb_image_write.h)
#   archivers/lzo2                  (lzo/lzo1x.h)
#   archivers/lz4                   (lz4.h)
#   security/cryptopp               (cryptopp/cryptlib.h)
#   devel/boost-libs                (boost/unordered_map.hpp, ...)
#   databases/mariadb-connector-c   (mariadb/mysql.h, libmariadb)
#     or databases/mysql80-client   (mysql/mysql.h, libmysqlclient)
#   devel/pkgconf                   (optional, faster discovery)
#
# Base system: libexecinfo (backtrace), clang/libc++ with
# C++23 <print> support (FreeBSD 14.1+ / 15.x).

include(FindPackageHandleStandardArgs)

find_package(Threads REQUIRED)

add_library(auriga_system_deps INTERFACE)
target_link_libraries(auriga_system_deps INTERFACE Threads::Threads)

# --- Boost: header-only usage (unordered_map, algorithm, lexical_cast, ...) ---
find_path(AURIGA_BOOST_INCLUDE_DIR NAMES boost/unordered_map.hpp REQUIRED)
target_include_directories(auriga_system_deps INTERFACE ${AURIGA_BOOST_INCLUDE_DIR})

# --- stb: header-only image codecs (GameServer/guild/MarkImage.cpp) ---
find_path(AURIGA_STB_INCLUDE_DIR NAMES stb/stb_image.h REQUIRED)
target_include_directories(auriga_system_deps INTERFACE ${AURIGA_STB_INCLUDE_DIR})

# --- LZ4 (GameServer/guild/MarkImage.cpp) ---
find_path(AURIGA_LZ4_INCLUDE_DIR NAMES lz4.h REQUIRED)
find_library(AURIGA_LZ4_LIBRARY NAMES lz4 REQUIRED)
target_include_directories(auriga_system_deps INTERFACE ${AURIGA_LZ4_INCLUDE_DIR})
target_link_libraries(auriga_system_deps INTERFACE ${AURIGA_LZ4_LIBRARY})

# --- LZO2 (GameServer/core/lzo_manager.*) ---
find_path(AURIGA_LZO_INCLUDE_DIR NAMES lzo/lzo1x.h REQUIRED)
find_library(AURIGA_LZO_LIBRARY NAMES lzo2 lzo REQUIRED)
target_include_directories(auriga_system_deps INTERFACE ${AURIGA_LZO_INCLUDE_DIR})
target_link_libraries(auriga_system_deps INTERFACE ${AURIGA_LZO_LIBRARY})

# --- Crypto++ (GameServer/core/cipher.*) ---
find_path(AURIGA_CRYPTOPP_INCLUDE_DIR NAMES cryptopp/cryptlib.h REQUIRED)
find_library(AURIGA_CRYPTOPP_LIBRARY NAMES cryptopp REQUIRED)
target_include_directories(auriga_system_deps INTERFACE ${AURIGA_CRYPTOPP_INCLUDE_DIR})
target_link_libraries(auriga_system_deps INTERFACE ${AURIGA_CRYPTOPP_LIBRARY})

# The stock FreeBSD package (security/cryptopp) is built with
# -DCRYPTOPP_DISABLE_ASM (the amd64 SIMD option is not a default), which sets
# CRYPTOPP_BOOL_ALIGN16 to 0 and changes the class layout. The application must
# be compiled with the same define, otherwise Crypto++ objects are allocated by
# one layout and destroyed by another (SIGBUS/SEGV in member_ptr destructors).
# Set AURIGA_CRYPTOPP_SIMD=ON when linking a self-built SIMD-enabled library.
option(AURIGA_CRYPTOPP_SIMD "Crypto++ was built with SIMD/ASM (FreeBSD port SIMD option)" OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD" AND NOT AURIGA_CRYPTOPP_SIMD)
    target_compile_definitions(auriga_system_deps INTERFACE CRYPTOPP_DISABLE_ASM)
    message(STATUS "auriga system deps: Crypto++ compiled with CRYPTOPP_DISABLE_ASM "
        "(match the stock FreeBSD package; use -DAURIGA_CRYPTOPP_SIMD=ON for a SIMD build)")
endif()

# --- MariaDB / MySQL client (SQL/*, Database/*, GameServer/core/utils.cpp) ---
# The sources include <mysql/mysql.h>, <mysql/errmsg.h> and
# <mysql/mysqld_error.h>. MariaDB Connector/C ships these headers as
# <mariadb/...> only, so generate thin forwarding headers when the classic
# <mysql/...> layout is not installed.
set(AURIGA_MYSQL_INCLUDE_DIR "" CACHE PATH "MariaDB/MySQL client include directory")
find_path(AURIGA_MYSQL_INCLUDE_DIR NAMES mysql/mysql.h)
if(NOT AURIGA_MYSQL_INCLUDE_DIR)
    find_path(AURIGA_MARIADB_INCLUDE_DIR NAMES mariadb/mysql.h REQUIRED)
    set(_auriga_mysql_compat_dir "${CMAKE_BINARY_DIR}/auriga_mysql_compat")
    file(MAKE_DIRECTORY "${_auriga_mysql_compat_dir}/mysql")
    foreach(_auriga_mysql_header mysql.h errmsg.h mysqld_error.h)
        file(WRITE "${_auriga_mysql_compat_dir}/mysql/${_auriga_mysql_header}"
            "#include <mariadb/${_auriga_mysql_header}>\n")
    endforeach()
    set(AURIGA_MYSQL_INCLUDE_DIR "${_auriga_mysql_compat_dir}")
endif()
find_library(AURIGA_MYSQL_LIBRARY
    NAMES mariadb mariadbclient mysqlclient
    HINTS /usr/local/lib/mariadb /usr/local/lib/mysql
)
if(NOT AURIGA_MYSQL_LIBRARY)
    message(FATAL_ERROR
        "MariaDB/MySQL client library not found. Install databases/mariadb-connector-c "
        "or databases/mysql80-client.")
endif()
target_include_directories(auriga_system_deps INTERFACE ${AURIGA_MYSQL_INCLUDE_DIR})
target_link_libraries(auriga_system_deps INTERFACE ${AURIGA_MYSQL_LIBRARY})

# --- FreeBSD base libraries ---
if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    # backtrace()/backtrace_symbols() live in libexecinfo (base system).
    find_library(AURIGA_EXECINFO_LIBRARY NAMES execinfo)
    if(AURIGA_EXECINFO_LIBRARY)
        target_link_libraries(auriga_system_deps INTERFACE ${AURIGA_EXECINFO_LIBRARY})
    endif()
endif()

message(STATUS "auriga system deps: mysql=${AURIGA_MYSQL_LIBRARY} "
    "lzo=${AURIGA_LZO_LIBRARY} lz4=${AURIGA_LZ4_LIBRARY} cryptopp=${AURIGA_CRYPTOPP_LIBRARY}")
