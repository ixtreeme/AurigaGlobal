file(GLOB_RECURSE SERVER_CORE_HEADER CONFIGURE_DEPENDS "*.h")
file(GLOB_RECURSE SERVER_CORE_SOURCE CONFIGURE_DEPENDS "*.cpp")

add_library(S_Core STATIC ${SERVER_CORE_HEADER} ${SERVER_CORE_SOURCE})

# TLS for the client-facing sessions (PeerSession over asio ssl).
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

target_link_libraries(S_Core PUBLIC SERVER OpenSSL::SSL OpenSSL::Crypto Threads::Threads)
