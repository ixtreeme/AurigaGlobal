import uiscriptlocale

BOARD_WIDH = 648
BOARD_HEIGHT = 512
ROOT = "d:/ymir work/ui/game/wiki/"

window = {
	"name" : "WikiWindow",
	"style" : ("movable", "float",),
	"x" : 0,
	"y" : 0,
	"width" : BOARD_WIDH,
	"height" : BOARD_HEIGHT,
	"children" :
	(
		{
			"name" : "board",
			"style" : ("attach",),
			"type" : "board_with_titlebar",
			"x" : 0,
			"y" : 0,
			"width" : BOARD_WIDH,
			"height" : BOARD_HEIGHT,
			"title" : uiscriptlocale.WIKI_TITLE,
		},
		{
			"name" : "BackGround",
			"style" : ("attach",),
			"type" : "image",
			"x" : 8,
			"y" : 30,
			"image" : ROOT + "background.tga",
			"children" : 
			(
				{
					"name" : "CategoriesSlot",
					"type" : "bar",
					"color" : 0x00000000,
					"x" : 3,
					"y" : 110,
					"width" : 135,
					"height" : 358,
					"children" :
					(
						{
							"name" : "ScrollBarBackground",
							"type" : "expanded_image",
							"x" : 13,
							"y" : 16,
							"image" : ROOT + "scrollbar_background.sub",
							"horizontal_align" : "right",
						},
						{
							"name" : "ScrollBar",
							"type" : "scrollbar_wiki",
							"x" : 14,
							"y" : 11,
							"size" : 336,
							"horizontal_align" : "right",
						},
					),
				},
				{
					"name" : "Banner",
					"style" : ("attach",),
					"type" : "image",
					"x" : 142,
					"y" : 0,
					"image" : ROOT + "banner/empty.tga",
					"children" :
					(
						{
							"name" : "bannerImage",
							"style" : ("attach",),
							"type" : "image",
							"x" : 262,
							"y" : (106 / 2) - (42 / 2),
							"width" : 198,
							"height" : 42,
							"image" : ROOT + "catbackground.sub",
							"children" :
							(
								{
									"name" : "bannerText",
									"type" : "text",
									"x" : 0,
									"y" : 10,
									"text" : "",
									"fontsize" : "LARGE_WIKI",
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							),
						},
					),
				},
				{
					"name" : "pageDescSlot",
					"type" : "bar",
					"color" : 0x64000000,
					"x" : 165,
					"y" : 109,
					"width" : 442,
					"height" : 30,
					"children" :
					(
						{
							"name" : "pageDescText",
							"type" : "text",
							"x" : 0,
							"y" : 3,
							"text" : "",
							"fontsize" : "MEDIUM_WIKI",
							"horizontal_align" : "center",
							"text_horizontal_align" : "center"
						},
					),
				},
				{
					"name" : "pageDescEndBar",
					"type" : "bar",
					"color" : 0xFF737373,
					"x" : 165,
					"y" : 137,
					"width" : 442,
					"height" : 2,
				},
				{
					"name" : "PageSlot",
					"type" : "bar",
					"color" : 0x00000000,
					"x" : 145,
					"y" : 141,
					"width" : 462,
					"height" : 314,
				},
				{
					"name" : "ScrollBarPageBackground",
					"type" : "expanded_image",
					"x" : 16,
					"y" : 128,
					"image" : ROOT + "scrollbar_background.sub",
					"horizontal_align" : "right",
				},
				{
					"name" : "ScrollBarPage",
					"type" : "scrollbar_wiki",
					"x" : 17,
					"y" : 125,
					"size" : 333,
					"horizontal_align" : "right",
				},
			),
		},
	),
}

