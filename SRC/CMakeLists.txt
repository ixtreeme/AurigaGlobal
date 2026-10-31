# Server side only. The Unity client replaced the old C++ client, so its
# sources, its third-party libraries and its tools are gone from the tree.

add_subdirectory(3rd/LUA55)
add_subdirectory(ItemDomain EXCLUDE_FROM_ALL)
add_subdirectory(Server)
