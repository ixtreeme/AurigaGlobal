import uiscriptlocale

if SCREEN_WIDTH >= 1024:
	WEB_WIDTH = 1016
	WEB_HEIGHT = 568
else:
	WEB_WIDTH = 700
	WEB_HEIGHT = 500

window = {
	"name" : "MallWindow",

	"x" : 0,
	"y" : 0,

	"style" : ("movable", "float",),

	"width"  : WEB_WIDTH  + 20,
	"height" : WEB_HEIGHT + 40,

	"children" :
	(
		{
			"name" : "board",
			"type" : "board",
			"style" : ("attach",),

			"x" : 0,
			"y" : 0,

			"width"	 : WEB_WIDTH  + 20,
			"height" : WEB_HEIGHT + 40,

			"children" :
			(
				## Title
				{
					"name" : "TitleBar",
					"type" : "titlebar",
					"style" : ("attach",),
					"x" : 8,
					"y" : 7,
					"width" : WEB_WIDTH + 10,
					"color" : "yellow",
					"children" :
					(
						{
							"name" : "TitleName",
							"type" : "text",
							"x" : 0,
							"y" : 3,
							"text" : uiscriptlocale.SYSTEM_MALL,
							"horizontal_align" : "center",
							"text_horizontal_align" : "center"
						},
					),
				},
			),
		},
	),
}
