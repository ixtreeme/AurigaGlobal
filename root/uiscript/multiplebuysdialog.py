import uiscriptlocale

window = {
	"name" : "MultipleBuysDialog",
	"x" : 0,
	"y" : 0,
	"style" : ("movable", "float",),
	"width" : 170,
	"height" : 106,
	"children" :
	(
		{
			"name" : "Board",
			"type" : "board_with_titlebar",
			"x" : 0,
			"y" : 0,
			"width" : 170,
			"height" : 106,
			"title" : uiscriptlocale.MULTIPLE_BUYS_TITLE,
			"children" :
			(
				{
					"name" : "PriceMessagge",
					"type" : "text",
					"x" : 0,
					"y" : 32,
					"horizontal_align" : "center",
					"text_horizontal_align" : "center",
					"text" : "",
				},
				{
					"name" : "InputSlot",
					"type" : "slotbar",
					"x" : -12,
					"y" : 50,
					"width" : 22,
					"height" : 18,
					"horizontal_align" : "center",
					"children" :
					(
						{
							"name" : "InputValue",
							"type" : "editline",
							"x" : 3,
							"y" : 3,
							"width" : 22,
							"height" : 18,
							"input_limit" : 3,
						},
					),
				},
				{
					"name" : "maximum",
					"type" : "text",
					"x" : +15,
					"y" : 52,
					"horizontal_align" : "center",
					"text_horizontal_align" : "center",
					"text" : "/ 100",
				},
				{
					"name" : "AcceptButton",
					"type" : "button",
					"x" : - 61 - 5 + 30,
					"y" : 76,
					"horizontal_align" : "center",
					"text" : uiscriptlocale.SHOP_BUY,
					"default_image" : "d:/ymir work/ui/public/middle_button_01.sub",
					"over_image" : "d:/ymir work/ui/public/middle_button_02.sub",
					"down_image" : "d:/ymir work/ui/public/middle_button_03.sub",
				},
				{
					"name" : "CancelButton",
					"type" : "button",
					"x" : 5 + 30,
					"y" : 76,
					"horizontal_align" : "center",
					"text" : uiscriptlocale.CLOSE,
					"default_image" : "d:/ymir work/ui/public/middle_button_01.sub",
					"over_image" : "d:/ymir work/ui/public/middle_button_02.sub",
					"down_image" : "d:/ymir work/ui/public/middle_button_03.sub",
				},
			),
		},
	),
}