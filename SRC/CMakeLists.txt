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

add_subdirectory(Tools/QC)
#add_subdirectory(Tools/ServerAttrTool)
add_subdirectory(Server)
add_subdirectory(3rd/LUA)
#add_subdirectory(Tools)