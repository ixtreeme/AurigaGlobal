import uiscriptlocale
import localeinfo

window = {
	"name" : "GameWindow",
	"style" : ("not_pick",),
	"x" : 0,
	"y" : 0,
	"width" : SCREEN_WIDTH,
	"height" : SCREEN_HEIGHT,
	"children" :
	(
		{ 
			"name" : "QuestButton", 
			"type" : "button", 
			"x" : SCREEN_WIDTH-50-32,
			"y" : SCREEN_HEIGHT-180,
			"default_image" : "d:/ymir work/ui/game/windows/btn_bigplus_up.sub",
			"over_image" : "d:/ymir work/ui/game/windows/btn_bigplus_over.sub",
			"down_image" : "d:/ymir work/ui/game/windows/btn_bigplus_down.sub",
			"tooltip_text" : uiscriptlocale.GAME_QUEST,
		},
		{
			"name" : "StatusPlusButton", 
			"type" : "button", 
			"x" : 68, 
			"y" : SCREEN_HEIGHT-110, 
			# "default_image" : "d:/ymir work/ui/game/windows/btn_bigplus_up.sub",
			# "over_image" : "d:/ymir work/ui/game/windows/btn_bigplus_over.sub",#Razor93
			# "down_image" : "d:/ymir work/ui/game/windows/btn_bigplus_down.sub",
			# "tooltip_text" : uiscriptlocale.GAME_STAT_UP,
		},
		{
			"name" : "DoctrineButton", 
			"type" : "button", 
			"x" : SCREEN_WIDTH-50-32,
			"y" : SCREEN_HEIGHT-110,
			"default_image" : "d:/ymir work/ui/game/windows/btn_bigplus_up.sub",
			"over_image" : "d:/ymir work/ui/game/windows/btn_bigplus_over.sub",
			"down_image" : "d:/ymir work/ui/game/windows/btn_bigplus_down.sub",
			"tooltip_text" : uiscriptlocale.SKILL_SELECT_1,
		},
		{
			"name" : "SkillPlusButton", 
			"type" : "button", 
			"x" : SCREEN_WIDTH-50-32,
			"y" : SCREEN_HEIGHT-110,
			# "default_image" : "d:/ymir work/ui/game/windows/btn_bigplus_up.sub",#Razor93
			# "over_image" : "d:/ymir work/ui/game/windows/btn_bigplus_over.sub",
			# "down_image" : "d:/ymir work/ui/game/windows/btn_bigplus_down.sub",
			# "tooltip_text" : uiscriptlocale.GAME_SKILL_UP,
		},
		{
			"name" : "ExitObserver", 
			"type" : "button", 
			"x" : SCREEN_WIDTH-50-32,
			"y" : SCREEN_HEIGHT-180,
			"default_image" : "d:/ymir work/ui/game/windows/btn_bigplus_up.sub",
			"over_image" : "d:/ymir work/ui/game/windows/btn_bigplus_over.sub",
			"down_image" : "d:/ymir work/ui/game/windows/btn_bigplus_down.sub",
			"tooltip_text" : uiscriptlocale.GAME_EXIT_OBSERVER,
		},
		{ 
			"name" : "BuildGuildBuilding",
			"type" : "button",
			"x" : SCREEN_WIDTH-50-32,
			"y" : SCREEN_HEIGHT-180,
			"default_image" : "d:/ymir work/ui/game/windows/btn_bigplus_up.sub",
			"over_image" : "d:/ymir work/ui/game/windows/btn_bigplus_over.sub",
			"down_image" : "d:/ymir work/ui/game/windows/btn_bigplus_down.sub",
			"tooltip_text" : uiscriptlocale.GUILD_BUILDING_TITLE,
		},
	),
}

