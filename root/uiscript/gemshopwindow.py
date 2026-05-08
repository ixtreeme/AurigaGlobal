import uiscriptlocale

WINDOW_X = 171
WINDOW_Y = 250

window = {
	"name" : "GemShopWindows",

	"x" : SCREEN_WIDTH/2,
	"y" : SCREEN_HEIGHT/2,

	"style" : ("movable", "float"),

	"width" : WINDOW_X,
	"height" : WINDOW_Y,

	"children" :
	(
		{
			"name" : "board",
			"type" : "board",
			"style" : ("attach",),

			"x" : 0,
			"y" : 0,

			"width" :WINDOW_X,
			"height" : WINDOW_Y,

			"children" :
			(
				## Title
				{
					"name" : "TitleBar",
					"type" : "titlebar",
					"style" : ("attach",),

					"x" : 8,
					"y" : 7,

					"width" : WINDOW_X-15,
					"color" : "yellow",

					"children" :
					(
						{ "name":"TitleName", "type":"text", "x":75, "y":3, "text":"GEM MARKET", "text_horizontal_align":"center" },
					),
				},
				{
					"name" : "bg_slots",
					"type" : "image",

					"x" : 16,
					"y" : 33,

					"image" : "d:/ymir work/ui/gemshop/gemshop_backimg.sub",
				},
				{
					"name": "time_gaya",
					"type": "text",
					"x" : 14+105-3,
					"y" : 32+5,
					"text": "10:00",
				},
				{
					"name" : "refresh_button",
					"type" : "button",

					"x" : 14,
					"y" : 32,

					"default_image" : "d:/ymir work/ui/gemshop/gemshop_refreshbutton_down.sub",
					"over_image" : "d:/ymir work/ui/gemshop/gemshop_refreshbutton_over.sub",
					"down_image" : "d:/ymir work/ui/gemshop/gemshop_refreshbutton_up.sub",
				},	
			),
		},
	),
}