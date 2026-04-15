import uiscriptlocale

window = {
	"name" : "PickMoneyDialog",

	"x" : 100,
	"y" : 100,

	"style" : ("movable", "float",),

	"width" : 230,
	"height" : 120,

	"children" :
	(
		{
			"name" : "board",
			"type" : "board_with_titlebar",

			"x" : 0,
			"y" : 0,

			"width" : 230,
			"height" : 120,
			"title" : uiscriptlocale.PICK_MONEY_TITLE,

			"children" :
			(
				{
					"name" : "package_count_text",
					"type" : "text",
					"x" : 36,
					"y" : 34,
					"text" : "Csomag",
				},
				{
					"name" : "package_count_slot",
					"type" : "image",
					"x" : 34,
					"y" : 50,
					"image" : "d:/ymir work/ui/public/Parameter_Slot_02.sub",
					"children" :
					(
						{
							"name" : "package_count_value",
							"type" : "editline",
							"x" : 3,
							"y" : 2,
							"width" : 60,
							"height" : 18,
							"input_limit" : 6,
							"only_number" : 1,
							"text" : "1",
						},
					),
				},
				{
					"name" : "package_count_x",
					"type" : "text",
					"x" : 110,
					"y" : 53,
					"text" : "x",
				},
				## Money Slot
				{
					"name" : "money_slot",
					"type" : "image",

					"x" : 124,
					"y" : 50,

					"image" : "d:/ymir work/ui/public/Parameter_Slot_02.sub",

					"children" :
					(
						{
							"name" : "money_value",
							"type" : "editline",

							"x" : 3,
							"y" : 2,
							"width" : 60,
							"height" : 18,
							"input_limit" : 6,
							"only_number" : 1,
							"text" : "1",
						},
						{
							"name" : "max_value",
							"type" : "text",
							"x" : 63,
							"y" : 3,
							"text" : "/ 999999",
						},
					),
				},
				{
					"name" : "package_size_text",
					"type" : "text",
					"x" : 126,
					"y" : 34,
					"text" : "Darab/csomag",
				},
				{
					"name" : "divisionBtn",
					"type" : "toggle_button",
					"x" : 36,
					"y" : 78,
					"default_image" : "d:/ymir work/ui/game/division/btn_1.png",
					"over_image" : "d:/ymir work/ui/game/division/btn_2.png",
					"down_image" : "d:/ymir work/ui/game/division/btn_3.png",
				},
				{
					"name" : "divisionText",
					"type" : "text",
					"x" : 56,
					"y" : 78,
					"text" : uiscriptlocale.DIVISION_STACK,
				},
				## Button
				{
					"name" : "accept_button",
					"type" : "button",

					"x" : 230/2 - 61 - 5,
					"y" : 92,

					"text" : uiscriptlocale.OK,

					"default_image" : "d:/ymir work/ui/public/middle_button_01.sub",
					"over_image" : "d:/ymir work/ui/public/middle_button_02.sub",
					"down_image" : "d:/ymir work/ui/public/middle_button_03.sub",
				},
				{
					"name" : "cancel_button",
					"type" : "button",

					"x" : 230/2 + 5,
					"y" : 92,

					"text" : uiscriptlocale.CANCEL,

					"default_image" : "d:/ymir work/ui/public/middle_button_01.sub",
					"over_image" : "d:/ymir work/ui/public/middle_button_02.sub",
					"down_image" : "d:/ymir work/ui/public/middle_button_03.sub",
				},
			),
		},
	),
}