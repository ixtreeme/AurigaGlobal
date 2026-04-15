# (C) 2020 Owsap Productions

import uiscriptlocale

BOARD_WIDTH = 230
BOARD_HEIGHT = 135

window = {
	"name" : "InputPinCodeDialog",

	"x" : 0,
	"y" : 0,

	"style" : ("movable", "float",),

	"width" : BOARD_WIDTH,
	"height" : BOARD_HEIGHT,

	"children" :
	(
		{
			"name" : "Board",
			"type" : "board_with_titlebar",

			"x" : 0,
			"y" : 0,

			"width" : BOARD_WIDTH,
			"height" : BOARD_HEIGHT,

			"title" : "",

			"children" :
			(
				{
					"name" : "thin_board",
					"type" : "thinboard",
					"style" : ("attach",),

					"x" : 10,
					"y" : 31,

					"width" : BOARD_WIDTH - 20,
					"height" : BOARD_HEIGHT - 38,
				},
				{
					"name" : "Description1",
					"type" : "text",

					"x" : BOARD_WIDTH / 2,
					"y" : 35,

					"text" : uiscriptlocale.PIN_CODE_DESC_1,
					"text_horizontal_align" : "center"
				},
				{
					"name" : "Description2",
					"type" : "text",

					"x" : BOARD_WIDTH / 2,
					"y" : 35 + 12,

					"text" : uiscriptlocale.PIN_CODE_DESC_2,
					"text_horizontal_align" : "center"
				},
				{
					"name" : "PinCodeSlot",
					"type" : "slotbar",

					"x" : 0,
					"y" : 70,
					"width" : 34,
					"height" : 18,
					"horizontal_align" : "center",

					"children" :
					(
						{
							"name" : "PinCodeValue",
							"type" : "editline",

							"x" : 3,
							"y" : 3,

							"width" : 34,
							"height" : 18,

							"input_limit" : 4,
							"only_number" : 1,
							"secret_flag" : 1,
						},
					),
				},
				## Button
				{
					"name" : "AcceptButton",
					"type" : "button",

					"x" : BOARD_WIDTH / 2 - 61 - 5,
					"y" : BOARD_HEIGHT - 35,

					"text" : uiscriptlocale.OK,

					"default_image" : "d:/ymir work/ui/public/middle_button_01.sub",
					"over_image" : "d:/ymir work/ui/public/middle_button_02.sub",
					"down_image" : "d:/ymir work/ui/public/middle_button_03.sub",
				},
				{
					"name" : "CancelButton",
					"type" : "button",

					"x" : BOARD_WIDTH / 2 + 5,
					"y" : BOARD_HEIGHT - 35,

					"text" : uiscriptlocale.CANCEL,

					"default_image" : "d:/ymir work/ui/public/middle_button_01.sub",
					"over_image" : "d:/ymir work/ui/public/middle_button_02.sub",
					"down_image" : "d:/ymir work/ui/public/middle_button_03.sub",
				},
				{
					"name" : "ToolTipButton",
					"type" : "button",

					"x" : 20,
					"y" : 8,

					"default_image" : "d:/ymir work/ui/pattern/q_mark_01.tga",
					"over_image" : "d:/ymir work/ui/pattern/q_mark_02.tga",
					"down_image" : "d:/ymir work/ui/pattern/q_mark_01.tga",
				},
			),
		},
	),
}