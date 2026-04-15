import uiscriptlocale
import localeinfo

ELEMENTS_POS_X = 185
ELEMENTS_POS_Y = 85

LARGE_VALUE_FILE = "d:/ymir work/ui/public/Parameter_Slot_03.sub"

window = {
	"name" : "PvpDialog2", "x" : 0, "y" : 0, "width" : 560, "height" : 310,
	"children" :
	(
		{
			"name" : "Background", "type" : "board",  "style" : ("attach",), "x" : 175, "y" : 230, "width" : 200, "height" : 80,	
			"children" :
			(
				{ "name":"TitleBar", "type":"horizontalbar", "x": 7, "y": 30, "width": 185, "children" : ( { "name":"TitleBar_Block_Value", "type":"text", "x":8, "y":2, "text": localeinfo.DUEL_INFO_5, }, ), },

				{ "name" : "Money_Slot", "type" : "button", "x" : 6, "y" : 30, "horizontal_align":"center", "vertical_align":"bottom", "default_image" : "d:/ymir work/ui/public/parameter_slot_05.sub", "over_image" : "d:/ymir work/ui/public/parameter_slot_05.sub", "down_image" : "d:/ymir work/ui/public/parameter_slot_05.sub",
					"children" :
					(
						{ "name":"Money_Icon", "type":"image", "x":-18, "y":2, "image":"d:/ymir work/ui/game/windows/money_icon.sub", },
						{ "name" : "Money", "type" : "text","x" : 3, "y" : 3, "horizontal_align" : "right", "text_horizontal_align" : "right", "text" : "", },
					),
				},			
			),	
		},
		{
			"name" : "Board", "type" : "board_with_titlebar", 
			"style" : ("attach",),
			"x" : 0, "y" : -15, "width" : 560, "height" : 280, "title" : "Duel with Go2Hell",
			"children" :
			(
				{ "name" : "CharacterSlot", "type" : "image", "x" : ELEMENTS_POS_X+7, "y" : 65, "width" : 32, "height" : 32, "image" : "d:/ymir work/ui/game/windows/box_face.sub", },
				{ "name" : "CharacterIcon", "type" : "image",  "x" : ELEMENTS_POS_X+12, "y" : 70,  "width" : 32, "height" : 32, "image" : "d:/ymir work/ui/game/windows/box_face.sub", },
				{ "name" : "percent_hp", "type" : "gauge", "x" : ELEMENTS_POS_X+60, "y" : 75, "width" : 115, "color" : "red", },
				{ "name" : "percent_sp", "type" : "gauge", "x" : ELEMENTS_POS_X+60, "y" : 95, "width" : 115, "color" : "blue", },
				{ "name": "ValueHP", "type":"text", "x": ELEMENTS_POS_X+105, "y": 62, "text": "", },
				{ "name": "ValueMP", "type":"text", "x": ELEMENTS_POS_X+105, "y": 82, "text": "", },
				{ "name" : "line", "type" : "line", "x" : 180, "y" : 30, "width" : 0, "height" : 240, "color" : 0xffffffff, },
				{ "name" : "line2", "type" : "line", "x" : 369, "y" : 30, "width" : 0, "height" : 240, "color" : 0xffffffff, },
				{
					"name":"TitleBar2", "type":"horizontalbar", "x":ELEMENTS_POS_X+5, "y":40, "width":167,
					"children" :
					(
						{ "name":"Pvp_Info_Point_Value", "type":"text", "x": 8, "y":2, "text": localeinfo.DUEL_INFO_6, },
						{
							"name" : "NameCharacter", "type" : "text", "x" : ELEMENTS_POS_X-ELEMENTS_POS_X, "y" : ELEMENTS_POS_Y, "text" : localeinfo.DUEL_INFO_1, "children" : (
								{ "name" : "NameCharacterSlot", "type" : "image", "x" : 70, "y" : -2, "image" : LARGE_VALUE_FILE, "children" : (
										{ "name" : "NameCharacterValue", "type":"text", "text": "Go2Hell", "x":0, "y":0, "all_align":"center" }, ), }, ),
						},
						{
							"name" : "GuildCharacter", "type" : "text", "x" : ELEMENTS_POS_X-ELEMENTS_POS_X, "y" : ELEMENTS_POS_Y+30, "text" : localeinfo.DUEL_INFO_2, "children" : (
								{ "name" : "GuildCharacterSlot", "type" : "image", "x" : 70, "y" : -2, "image" : LARGE_VALUE_FILE, "children" : (
										{ "name" : "GuildCharacterValue", "type":"text", "text": "-", "x":0, "y":0, "all_align":"center" }, ), }, ),
						},
						{
							"name" : "LevelCharacter", "type" : "text", "x" : ELEMENTS_POS_X-ELEMENTS_POS_X, "y" : ELEMENTS_POS_Y+30+30, "text" : localeinfo.DUEL_INFO_3, "children" : (
								{ "name" : "LevelCharacterSlot", "type" : "image", "x" : 70, "y" : -2, "image" : LARGE_VALUE_FILE, "children" : (
										{ "name" : "LevelCharacterValue", "type":"text", "text": "103", "x":0, "y":0, "all_align":"center" }, ), }, ),
						},
						{
							"name" : "PlayTimeCharacter", "type" : "text", "x" : ELEMENTS_POS_X-ELEMENTS_POS_X, "y" : ELEMENTS_POS_Y+30+30+30, "text" : localeinfo.DUEL_INFO_4, "children" : (
								{ "name" : "PlayTimeCharacterSlot", "type" : "image", "x" : 70, "y" : -2, "image" : LARGE_VALUE_FILE, "children" : (
										{ "name" : "PlayTimeCharacterValue", "type":"text", "text": "-", "x":0, "y":0, "all_align":"center" }, ), }, ),
						},
					),
				},
				{ "name" : "BlockEquip", "type" : "button", "x" : 75, "y" : 150, "text" : "", "default_image" : "d:/ymir work/ui/game/pvp_advanced/blocked_equip.tga", "over_image" : "d:/ymir work/ui/game/pvp_advanced/blocked_equip.tga", "down_image" : "d:/ymir work/ui/game/pvp_advanced/blocked_equip.tga", },
				
				{ "name":"TitleBar3", "type":"horizontalbar", "x": 15, "y": 40, "width": 159, "children" : ( { "name":"TitleBar_Block_Value", "type":"text", "x":8, "y":2, "text": localeinfo.DUEL_TITLENAME_EQUIP, }, ), },
				{ "name":"TitleBar4", "type":"horizontalbar", "x": 377, "y": 40, "width": 167, "children" : ( { "name":"TitleBar_Block_Value", "type":"text", "x":8, "y":2, "text": localeinfo.DUEL_CHOOSE_BLOCK, }, ), },

				{ "name" : "AcceptButton", "type" : "button", "x" : 200, "y" : 245, "text" : "", "default_image" : "d:/ymir work/ui/game/pvp_advanced/acceptbutton00.tga", "over_image" : "d:/ymir work/ui/game/pvp_advanced/acceptbutton01.tga", "down_image" : "d:/ymir work/ui/game/pvp_advanced/acceptbutton02.tga", },
				{ "name" : "DenyButton", "type" : "button", "x" : 300, "y" : 245, "text" : "", "default_image" : "d:/ymir work/ui/game/pvp_advanced/canclebutton00.tga", "over_image" : "d:/ymir work/ui/game/pvp_advanced/canclebutton01.tga", "down_image" : "d:/ymir work/ui/game/pvp_advanced/canclebutton02.tga", },
			),
		},
	),
}