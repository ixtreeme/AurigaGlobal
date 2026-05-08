# (C) 2020 Owsap Productions

import uiscriptlocale

BOARD_WIDTH = 180
BOARD_HEIGHT = 175

window = {
	"name" : "InputNewPinDialog",

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
					"name" : "PinCodeSlot",
					"type" : "slotbar",

					"x" : 0,
					"y" : 60,
					"horizontal_align" : "center",

					"width" : 34,
					"height" : 18,

					"children" :
					(
						{
							"name" : "PinCodeText",
							"type" : "text",

							"x" : 0,
							"y" : -25,
							"all_align" : "center",

							"text" : uiscriptlocale.PIN_CODE_NEW,
						},
						{
							"name" : "PinCodeValue",
							"type" : "editline",

							"x" : 3,
							"y" : 3,

							"width" : 60,
							"height" : 18,

							"input_limit" : 4,
							"secret_flag" : 1,
						},
					),
				},
				{
					"name" : "PinCodeSlotCheck",
					"type" : "slotbar",

					"x" : 0,
					"y" : 110,
					"horizontal_align" : "center",

					"width" : 34,
					"height" : 18,

					"children" :
					(
						{
							"name" : "PinCodeTextCheck",
							"type" : "text",

							"x" : 0,
							"y" : -25,
							"all_align" : "center",

							"text" : uiscriptlocale.PIN_CODE_CONFIRM_NEW,
						},
						{
							"name" : "PinCodeValueCheck",
							"type" : "editline",

							"x" : 3,
							"y" : 3,

							"width" : 110,
							"height" : 18,

							"input_limit" : 4,
							"only_number" : 1,
							"secret_flag" : 1,
						},
					),
				},
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
			),
		},
	),
}