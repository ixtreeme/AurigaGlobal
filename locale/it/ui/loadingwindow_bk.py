import uiscriptlocale

MIDDLE_WIDTH = SCREEN_WIDTH / 2

window = {
	"x" : 0,
	"y" : 0,
	"width" : SCREEN_WIDTH,
	"height" : SCREEN_HEIGHT,
	"children" :
	(
		{
			"name" : "BackGround",
			"type" : "expanded_image",
			"x" : 0,
			"y" : 0,
			"image" : "d:/ymir work/ui/intro/pattern/Line_Pattern.tga",
			"x_scale" : float(SCREEN_WIDTH) / 800.0,
			"y_scale" : float(SCREEN_HEIGHT) / 600.0,
		},
		{
			"name" : "LogoWonder2",
			"type" : "expanded_image",
			"x" : MIDDLE_WIDTH - (533 / 2),
			"y" : 100,
			"image" : "locale/it/ui/loading/logo.tga",
		},
		{ 
			"name":"ErrorMessage", 
			"type":"text", "x":10, "y":10, 
			"text": uiscriptlocale.LOAD_ERROR, 
		},
		{
			"name" : "GageBoard",
			"type" : "window",
			"style" : ("ltr",),
			"x" : MIDDLE_WIDTH - (250 / 2),
			"y" : SCREEN_HEIGHT - 155,
			"width" : 400, 
			"height": 80,
			"children" :
			(
		
				{
					"name" : "BackGage",
					"type" : "expanded_image",
					"x" : 40,
					"y" : 25,
					"image" : "locale/it/ui/loading/gauge_empty.dds",
				},
				{
					"name" : "FullGage",
					"type" : "expanded_image",
					"x" : 40,
					"y" : 25,
					"image" : "locale/it/ui/loading/gauge_full.dds",
				},
			),
		},
	),  
}

