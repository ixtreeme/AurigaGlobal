import uiscriptlocale

BOARD_WIDTH = 229
BOARD_HEIGHT = 219 + 120 + 20

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
							"text" : uiscriptlocale.BIOLOG_WND_TITLE,
							"horizontal_align" : "center",
							"text_horizontal_align" : "center"
						},
					),
				},
				{
					"name" : "missionNameBar",
					"type" : "bar",
					"x" : 10,
					"y" : 32,
					"width" : BOARD_WIDTH - 20,
					"height" : 20,
					"color" : 0x64000000,
					"children" :
					(
						{
							"name" : "missionNameText",
							"type" : "text",
							"x" : 0,
							"y" : 0,
							"fontname" : "Arial:16b",
							"text" : "",
							"horizontal_align" : "center",
							"text_horizontal_align" : "center"
						},
					),
				},
				{
					"name" : "itembar",
					"type" : "image",
					"x" : 13,
					"y" : 58,
					"image" : "d:/ymir work/ui/game/biologist/bar1.tga",
					"children" :
					(
						{
							"name" : "requiredItem", 
							"type" : "grid_table", 
							"x" : 10, 
							"y" : 4, 
							"start_index" : 0, 
							"x_count" : 1, 
							"y_count" : 1, 
							"x_step" : 32, 
							"y_step" : 32, 
						},
						{
							"name" : "countBar",
							"type" : "bar",
							"x" : 52,
							"y" : 12,
							"width" : 139,
							"height" : 17,
							"color" : 0x64000000,
							"children" :
							(
								{
									"name" : "countBarProgress",
									"type" : "bar",
									"x" : 2,
									"y" : 2,
									"width" : 135,
									"height" : 13,
									"color" : 0xFF008326,
								},
								{
									"name" : "countDelivered",
									"type" : "text",
									"x" : 0,
									"y" : 1,
									"text" : "",
									"horizontal_align" : "center",
									"text_horizontal_align":"center"
								}
							),
						},
					),
				},
				{
					"name" : "Shop_Button",
					"type" : "button",
					"x" : - 61 - 5 + 30,
					"y" : 102,
					"horizontal_align" : "center",
					"text" : uiscriptlocale.RUNE_SHOP,
					"default_image" : "d:/ymir work/ui/public/middle_button_01.sub",
					"over_image" : "d:/ymir work/ui/public/middle_button_02.sub",
					"down_image" : "d:/ymir work/ui/public/middle_button_03.sub",
				},
				{
					"name" : "Deliver_Button",
					"type" : "button",
					"x" : 5 + 30,
					"y" : 102,
					"horizontal_align" : "center",
					"text" : uiscriptlocale.BIOLOGO_CONSEGNA_ITEM,
					"default_image" : "d:/ymir work/ui/public/middle_button_01.sub",
					"over_image" : "d:/ymir work/ui/public/middle_button_02.sub",
					"down_image" : "d:/ymir work/ui/public/middle_button_03.sub",
				},
				{
					"name" : "slotbar",
					"type" : "image",
					"x" : 51,
					"y" : 101 + 23,
					"image" : "d:/ymir work/ui/game/biologist/bar2.tga",
					"children" : 
					(
						{
							"name" : "itemIcon1",
							"type" : "image",
							"x" : 34,
							"y" : 4,
							"image" : "d:/ymir work/ui/game/biologist/40144.tga",
						},
						{
							"name" : "itemIcon2",
							"type" : "image",
							"x" : 85,
							"y" : 4,
							"image" : "d:/ymir work/ui/game/biologist/40143.tga",
						},
					),
				},
				{
					"name" : "timerbar",
					"type" : "image",
					"x" : 57,
					"y" : 136 + 23,
					"image" : "d:/ymir work/ui/game/biologist/timerbar.tga",
					"children" :
					(
						{
							"name" : "TimeNotificationText",
							"type" : "text",
							"x" : 83,
							"y" : 3,
							"text": uiscriptlocale.NOTIFICA_BIOLOGO,
							"text_horizontal_align" : "center"
						},
					),
				},
				{
					"name" : "nextrewardbar",
					"type" : "image",
					"x" : 13,
					"y" : 159 + 23,
					"image" : "d:/ymir work/ui/game/biologist/next_reward.tga",
					"children" :
					(
						{
							"name" : "TimeText",
							"type" : "text",
							"x" : 102,
							"y" : 5,
							"text" : "",
							"text_horizontal_align" : "center"
						},
					),
				},
				{
					"name" : "rewardBar",
					"type" : "bar",
					"x" : 10,
					"y" : 212,
					"width" : BOARD_WIDTH - 20,
					"height" : 20 + 16,
					"color" : 0x64000000,
					"children" :
					(
						{
							"name" : "rewardText",
							"type" : "text",
							"x" : 0,
							"y" : 0,
							"fontname" : "Arial:16b",
							"text" : uiscriptlocale.BIOLOG_WND_REWARD,
							"color" : 0xFFFF0000,
							"horizontal_align" : "center",
							"text_horizontal_align" : "center"
						},
						{
							"name" : "rewardTypeText",
							"type" : "text",
							"x" : 0,
							"y" : 16,
							"text" : "",
							"color" : 0xFFFF0000,
							"horizontal_align" : "center",
							"text_horizontal_align" : "center"
						},
					),
				},
				{
					"name" : "rewardSlot",
					"type" : "slotbar",
					"x" : 9,
					"y" : 236 + 16,
					"width" : BOARD_WIDTH - 20,
					"height" : 68,
				},
				{
					"name" : "RewardButton",
					"type" : "button",
					"x" : 85,
					"y" : 308 + 20,
					"text" : uiscriptlocale.BIOLOG_WND_REWARD_BUTTON,
					"default_image" : "d:/ymir work/ui/public/middle_button_01.sub",
					"over_image" : "d:/ymir work/ui/public/middle_button_02.sub",
					"down_image" : "d:/ymir work/ui/public/middle_button_03.sub",
				},
			),
		},
	),
}
