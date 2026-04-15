import uiscriptlocale

window = {
	"name" : "TeleportWindow",
	"style" : ("float",),
	"x" : 0,
	"y" : 0,
	"width" : SCREEN_WIDTH,
	"height" : SCREEN_HEIGHT,
	"children" :
	(
		{
			"name" : "mainboard",
			"type" : "bar",
			"x" : 0,
			"y" : 0,
			"width" : SCREEN_WIDTH,
			"height" : SCREEN_HEIGHT,
			"title" : uiscriptlocale.ZONE_MAP,
			"color" : 0xC8000000,
			"children" :
			(
				{
					"name" : "board",
					"type" : "image",
					"style" : ("attach",),
					"x" : (SCREEN_WIDTH / 2) - (800 / 2),
					"y" : (SCREEN_HEIGHT / 2) - (600 / 2),
					"image" : uiscriptlocale.LOCALE_UISCRIPT_PATH + "images/teleporter.tga",
					"width" : 800,
					"height" : 600,
				},
			),
		},
	),
}
