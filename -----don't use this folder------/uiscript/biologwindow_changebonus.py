import uiscriptlocale

BOARD_WIDTH = 229
BOARD_HEIGHT = 236

window = {
	"name" : "BiologWindow",
	"x" : 0,
	"y" : 0,
	"style" : ("movable", "float",),
	"width" : BOARD_WIDTH,
	"height" : BOARD_HEIGHT,
	"children" :
	(
		{
			"name" : "board",
			"type" : "board",
			"style" : ("attach",),
			"x" : 0,
			"y" : 0,
			"width" : BOARD_WIDTH,
			"height" : BOARD_HEIGHT,
			"children" :
			(
				{
					"name" : "TitleBar",
					"type" : "titlebar",
					"style" : ("attach",),
					"x" : 6,
					"y" : 6,
					"width" : BOARD_WIDTH - 10,
					"color" : "yellow",
					"children" :
					(
						{
							"name" : "TitleName",
							"type" : "text",
							"x" : 0,
							"y" : 3,
							"text" : uiscriptlocale.BIOLOG_WND_TITLE_CHOOSE,
							"horizontal_align" : "center",
							"text_horizontal_align" : "center"
						},
					),
				},
				{
					"name" : "alert1bar",
					"type" : "bar",
					"x" : 10,
					"y" : 32,
					"width" : BOARD_WIDTH - 20,
					"height" : 20,
					"color" : 0x64000000,
					"children" :
					(
						{
							"name" : "alert1text",
							"type" : "text",
							"x" : 0,
							"y" : 0,
							"fontname" : "Arial:16b",
							"text" : uiscriptlocale.BIOLOG_WND_CHOOSE_1,
							"horizontal_align" : "center",
							"text_horizontal_align" : "center"
						},
					),
				},
				{
					"name" : "missionslot",
					"type" : "slotbar",
					"x" : 9,
					"y" : 56,
					"width" : BOARD_WIDTH - 20,
					"height" : 34,
				},
				{
					"name" : "alert2bar",
					"type" : "bar",
					"x" : 10,
					"y" : 106,
					"width" : BOARD_WIDTH - 20,
					"height" : 20,
					"color" : 0x64000000,
					"children" :
					(
						{
							"name" : "alert2text",
							"type" : "text",
							"x" : 0,
							"y" : 0,
							"fontname" : "Arial:16b",
							"text" : uiscriptlocale.BIOLOG_WND_CHOOSE_2,
							"horizontal_align" : "center",
							"text_horizontal_align" : "center"
						},
					),
				},
				{
					"name" : "bonusslot",
					"type" : "slotbar",
					"x" : 9,
					"y" : 130,
					"width" : BOARD_WIDTH - 20,
					"height" : 68,
				},
				{
					"name" : "ChangeButton",
					"type" : "button",
					"x" : 85,
					"y" : 204,
					"text" : uiscriptlocale.BIOLOG_WND_CHANGE,
					"default_image" : "d:/ymir work/ui/public/middle_button_01.sub",
					"over_image" : "d:/ymir work/ui/public/middle_button_02.sub",
					"down_image" : "d:/ymir work/ui/public/middle_button_03.sub",
				},
			),
		},
	),
}
