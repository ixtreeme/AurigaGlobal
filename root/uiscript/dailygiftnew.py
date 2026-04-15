import uiscriptlocale

WIDTH = 652 + 13
HEIGHT = 404 + 15

window = {
	"name" : "MallWindow",
	"x" : 0,
	"y" : 0,
	"style" : ("movable", "float",),
	"width"  : WIDTH,
	"height" : HEIGHT,
	"children" :
	(
		{
			"name" : "board_c",
			"type" : "board",
			"x" : 0,
			"y" : 0,
			"width" : WIDTH,
			"height" : HEIGHT,
			"children" :
			(
				{
					"name" : "board",
					"type" : "image",
					"style" : ("attach",),
					"x" : 6,
					"y" : 7,
					"image" : "d:/ymir work/ui/public/dailygift/background.tga",
					"width" : WIDTH - 12,
					"height" : HEIGHT - 15,
				},
			),
		},
	),
}
