file(GLOB_RECURSE MAIN_HEADER CONFIGURE_DEPENDS "*.h")
file(GLOB_RECURSE MAIN_SOURCE CONFIGURE_DEPENDS "*.cpp")

add_executable(UserInterface ${MAIN_HEADER} ${MAIN_SOURCE} UserInterface.rc)

target_link_libraries(UserInterface
	PUBLIC AudioLib
	PUBLIC Base
	PUBLIC Effect
	PUBLIC FoxFS
	PUBLIC Game
	PUBLIC Granny
	PUBLIC Image
	PUBLIC Pack
	PUBLIC Python
	PUBLIC Render
	PUBLIC Script
	PUBLIC SecureLayer
	PUBLIC SpeedTree
	PUBLIC Sphere
	PUBLIC Terrain
	PUBLIC Granny
)


set_target_properties(UserInterface PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")