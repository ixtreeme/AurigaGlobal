# The old C++ Launcher (and Mysql2Proto, which builds on its libraries) is
# retired: the Unity client replaced it. The sources stay in the tree.
if (MSVC)
	add_subdirectory(3rd/F_FS)
	add_subdirectory(3rd/Granny)
	add_subdirectory(3rd/SpeedTree)

	add_subdirectory(Tools/MSAMaker)
	add_subdirectory(Tools/IXAC)
	add_subdirectory(Tools/IXAC_Updater)
endif()

option(AURIGA_BUILD_LEGACY_QC "Build the historical Lua 5.0 quest compiler for comparison" OFF)
if(AURIGA_BUILD_LEGACY_QC)
    add_subdirectory(3rd/LUA)
    add_subdirectory(Tools/QC)
endif()
#add_subdirectory(Tools/ServerAttrTool)
add_subdirectory(3rd/LUA55)
add_subdirectory(Server)
#add_subdirectory(Tools)
