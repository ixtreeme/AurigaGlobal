import uiscriptlocale
import localeinfo

ELEMENTS_POS_X = 15
ELEMENTS_POS_Y = 30
LARGE_VALUE_FILE = "d:/ymir work/ui/public/Parameter_Slot_05.sub"
MONEY_ICON = "d:/ymir work/ui/game/windows/money_icon.sub"
FACE = "d:/ymir work/ui/game/windows/box_face.sub"

window = {
	"name" : "PvPInformations", "x" : 0, "y" : 0, "style" : ("float",), "width" : 290, "height" : 160,
	"children" :
	(
		{
			"name" : "Board", "type" : "thinboard", 
			"x" : 0, "y" : 0, "width" : 290, "height" : 160,
			"children" :
			(
				{
					"name" : "minimizebutton",
					"type" : "button",
					"x" : 260,
					"y" : 5,
					"tooltip_text" : uiscriptlocale.MINIMIZE,
					"default_image" : "d:/ymir work/ui/public/minimize_button_01.sub",
					"over_image" : "d:/ymir work/ui/public/minimize_button_02.sub",
					"down_image" : "d:/ymir work/ui/public/minimize_button_03.sub",
				},
				{
					"name" : "Background", "type" : "thinboard",  "style" : ("attach",), "x" : 60, "y" : 160, "width" : 160, "height" : 30,	
					"children" :
					(
						{ "name" : "Para_Slot", "type" : "button", "x" : 7, "y" : 25, "horizontal_align":"center", "vertical_align":"bottom", "default_image" : LARGE_VALUE_FILE, "over_image" : LARGE_VALUE_FILE, "down_image" : LARGE_VALUE_FILE,
							"children" :
							(
								{ "name":"Money_Icon", "type":"image", "x":-18, "y":2, "image": MONEY_ICON, },
								{ "name" : "Money", "type" : "text","x" : 3, "y" : 3, "horizontal_align" : "right", "text_horizontal_align" : "right", "text" : "99999999999", },
							),
						},
					),
				},
				{ "name":"line_1", "type":"image", "style" : ("attach",), "x" : 54, "y" : 22, "image" : "d:/ymir work/ui/game/pvp_advanced/bar_limited.tga", },
				{ "name":"line_2", "type":"image", "style" : ("attach",), "x" : 54, "y" : 48, "image" : "d:/ymir work/ui/game/pvp_advanced/bar_limited.tga", },
				
				{ "name" : "CharacterSlot", "type" : "image", "x" : 3, "y" : 20, "width" : 32, "height" : 32, "image" : FACE, },
				{ "name" : "CharacterIcon", "type" : "image",  "x" : 7, "y" : 25,  "width" : 32, "height" : 32, "image" : FACE, },	
				
				{ "name" : "NameCharacter", "type" : "text", "x" : 60, "y" : 26, "color" : 0xffF8BF24, "text" : localeinfo.DUEL_LIVE_VICTIM_NAME, "children" : (
					{ "name" : "NameCharacterSlot", "type" : "image", "x" : 95, "y" : -3, "image" : LARGE_VALUE_FILE, "children" : (
					{ "name" : "NameCharacterValue", "type":"text", "text": "99999999999", "x":0, "y":0, "all_align":"center" }, ), }, ), },
					
				{ "name" : "LevelCharacter", "type" : "text", "x" : 60, "y" : 35+15, "color" : 0xffF8BF24, "text" : localeinfo.DUEL_LIVE_VICTIM_LEVEL, "children" : (
					{ "name" : "LevelCharacterSlot", "type" : "image", "x" : 95, "y" : -1, "image" : LARGE_VALUE_FILE, "children" : (
					{ "name" : "LevelCharacterValue", "type":"text", "text": "99999999999", "x":0, "y":0, "all_align":"center" }, ), }, ), },
			),
		},
	),
}