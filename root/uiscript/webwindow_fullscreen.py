import uiscriptlocale

WEB_WIDTH = SCREEN_WIDTH
WEB_HEIGHT = SCREEN_HEIGHT

window = {
	"name" : "MallWindow",
	"x" : 0,
	"y" : 0,
	"style" : ("float",),
	"width"  : WEB_WIDTH,
	"height" : WEB_HEIGHT,
	"children" :
	(
		{
			"name" : "board",
			"type" : "board",
			"style" : ("attach",),
			"x" : 0,
			"y" : 0,
			"width"	 : WEB_WIDTH,
			"height" : WEB_HEIGHT,
			"children" :
			(
				{
					"name" : "TitleBar",
					"type" : "titlebar",
					"style" : ("attach",),
					"x" : 8,
					"y" : 7,
					"width" : WEB_WIDTH - 10,
					"color" : "yellow",
					"children" :
					(
						{
							"name" : "TitleName",
							"type" : "text",
							"x" : 0,
							"y" : 3,
							"text" : uiscriptlocale.BROWSER_CHROME,
							"horizontal_align" : "center",
							"text_horizontal_align" : "center"
						},
					),
				},
			),
		},
	),
}
