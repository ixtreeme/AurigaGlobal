import uiscriptlocale

BUTTONS_COUNT = 5
BOARD_WIDTH = 37
BOARD_HEIGHT = (38 * BUTTONS_COUNT)

window = {
	"name" : "InventoryMenuWindow",
	"x" : 0,
	"y" : 0,
	"style" : ("movable", "float",),
	"width" : BOARD_WIDTH,
	"height" : BOARD_HEIGHT,
	"children" :
	(
		{
			"name" : "board",
			"type" : "bar",
			"x" : 0,
			"y" : 0,
			"width" : BOARD_WIDTH,
			"height" : BOARD_HEIGHT,
			"color" : 0x00000000,
			"children" : 
			(
				{
					"name" : "button1",
					"type" : "button",
					"x" : 0,
					"y" : 0,
					"tooltip_text" : uiscriptlocale.SUPPORT_SYSTEM,
					"tooltip_x" : -70,
					"tooltip_y" : 10,
					"default_image" : "d:/ymir work/ui/support/buff_normal.tga",
					"over_image" : "d:/ymir work/ui/support/buff_hover.tga",
					"down_image" : "d:/ymir work/ui/support/buff_active.tga",
				},	
				{
					"name" : "button2",
					"type" : "button",
					"x" : 0,
					"y" : BOARD_WIDTH,
					"tooltip_text" : uiscriptlocale.BATTLE_PASS,
					"tooltip_x" : -57,
					"tooltip_y" : 10,
					"default_image" : "d:/ymir work/ui/battle_pass/battlepass_normal.tga",
					"over_image" : "d:/ymir work/ui/battle_pass/battlepass_hover.tga",
					"down_image" : "d:/ymir work/ui/battle_pass/battlepass_active.tga",
				},
				{
					"name" : "button3",
					"type" : "button",
					"x" : 0,
					"y" : (BOARD_WIDTH * 2),
					"tooltip_text" : uiscriptlocale.SWITCH_BOT,
					"tooltip_x" : -59,
					"tooltip_y" : 10,
					"default_image" : "d:/ymir work/ui/switchbot/switch_normal.tga",
					"over_image" : "d:/ymir work/ui/switchbot/switch_hover.tga",
					"down_image" : "d:/ymir work/ui/switchbot/switch_active.tga",
				},
				{
					"name" : "button4",
					"type" : "button",
					"x" : 0,
					"y" : (BOARD_WIDTH * 3),
					"tooltip_text" : uiscriptlocale.CLASSIFICA_GILDA,
					"tooltip_x" : -67,
					"tooltip_y" : 10,
					"default_image" : "d:/ymir work/ui/Gilde/gilde_normal.tga",
					"over_image" : "d:/ymir work/ui/Gilde/gilde_hover.tga",
					"down_image" : "d:/ymir work/ui/Gilde/gilde_active.tga",
				},
				{
					"name" : "button5",
					"type" : "button",
					"x" : 0,
					"y" : (BOARD_WIDTH * 4),
					"tooltip_text" : uiscriptlocale.CLASSIFICA_GIOCATORI,
					"tooltip_x" : -75,
					"tooltip_y" : 10,
					"default_image" : "d:/ymir work/ui/Giocatori/player_normal.tga",
					"over_image" : "d:/ymir work/ui/Giocatori/player_hover.tga",
					"down_image" : "d:/ymir work/ui/Giocatori/player_active.tga",
				},
			),
		},
	),
}
