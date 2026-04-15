import uiscriptlocale

BOARD_WIDTH = 368
BACK_IMG_PATH = "d:/ymir work/ui/public/public_board_back/"
ROOT_PATH = "d:/ymir work/ui/game/guild/dragonlairranking/"

window = {
	"name" : "DungeonRankingWindow",
	"style" : ("movable", "float",),

	"x" : 0,
	"y" : 0,

	"width" : BOARD_WIDTH,
	"height" : 238,

	"children" :
	(
		{
			"name" : "DungeonRankingBoard",
			"type" : "board",
			"style" : ("attach",),

			"x" : 0,
			"y" : 0,

			"width" : BOARD_WIDTH,
			"height" : 238,

			"children" :
			(
				{
					"name" : "DungeonRankingTitleBar",
					"type" : "titlebar",
					"style" : ("attach",),

					"x" : 6,
					"y" : 6,

					"width" : 355,
					"color" : "yellow",

					"children" :
					(
						{
							"name" : "DungeonRankingTitleName",
							"type" : "text",
							"x" : BOARD_WIDTH / 2,
							"y" : 3,
							"text" : uiscriptlocale.DUNGEON_RANKING,
							"text_horizontal_align" : "center"
						},
					),
				},
				{
					"name" : "LeftTop",
					"type" : "image",
					"x" : 17,
					"y" : 38,
					"image" : BACK_IMG_PATH + "boardback_mainboxlefttop.sub",
				},
				{
					"name" : "RightTop",
					"type" : "image",
					"x" : 318,
					"y" : 38,
					"image" : BACK_IMG_PATH + "boardback_mainboxrighttop.sub",
				},
				{
					"name" : "LeftBottom",
					"type" : "image",
					"x" : 17,
					"y" : 173,
					"image" : BACK_IMG_PATH + "boardback_mainboxleftbottom.sub",
				},
				{
					"name" : "RightBottom",
					"type" : "image",
					"x" : 318,
					"y" : 173,
					"image" : BACK_IMG_PATH + "boardback_mainboxrightbottom.sub",
				},
				{
					"name" : "leftcenterImg",
					"type" : "expanded_image",
					"x" : 17,
					"y" : 38 + 16,
					"image" : BACK_IMG_PATH + "boardback_leftcenterImg.tga",
					"rect" : (0.0, 0.0, 0, 6),
				},
				{
					"name" : "rightcenterImg",
					"type" : "expanded_image",
					"x" : 317,
					"y" : 38 + 16,
					"image" : BACK_IMG_PATH + "boardback_rightcenterImg.tga",
					"rect" : (0.0, 0.0, 0, 6),
				},
				{
					"name" : "topcenterImg",
					"type" : "expanded_image",
					"x" : 17 + 15,
					"y" : 38,
					"image" : BACK_IMG_PATH + "boardback_topcenterImg.tga",
					"rect" : (0.0, 0.0, 16, 0),
				},
				{
					"name" : "bottomcenterImg",
					"type" : "expanded_image",
					"x" : 17 + 15,
					"y" : 173,
					"image" : BACK_IMG_PATH + "boardback_bottomcenterImg.tga",
					"rect" : (0.0, 0.0, 16, 0),
				},
				{
					"name" : "centerImg",
					"type" : "expanded_image",
					"x" : 17 + 15,
					"y" : 38 + 15,
					"image" : BACK_IMG_PATH + "boardback_centerImg.tga",
					"rect" : (0.0, 0.0, 16, 6),
				},
				{
					"name" : "DungeonRankingTiTleImg",
					"type" : "image",
					"x" : 20,
					"y" : 41,
					"image" : ROOT_PATH + "ranking_list_menu.sub",
					"children" :
					(
						{
							"name" : "ResultRanking",
							"type" : "text",
							"x" : 10,
							"y" : 4,
							"text" : uiscriptlocale.DUNGEON_RANKING_POSITION,
						},
						{
							"name" : "ResultName",
							"type" : "text",
							"x" : 95,
							"y" : 4,
							"text" : uiscriptlocale.DUNGEON_RANKING_NAME,
						},
						{
							"name" : "ResultLevel",
							"type" : "text",
							"x" : 180,
							"y" : 4,
							"text" : uiscriptlocale.DUNGEON_RANKING_LEVEL,
						},
						{
							"name" : "ResultPoints",
							"type" : "text",
							"x" : 240,
							"y" : 4,
							"text" : uiscriptlocale.DUNGEON_RANKING_POINTS,
						},
					),
				},
				{
					"name" : "DungeonRankingScrollBar",
					"type" : "scrollbar",
					"x" : 340,
					"y" : 38,
					"size" : 180,
				},
				{
					"name" : "LeftTopSelf",
					"type" : "image",
					"x" : 17,
					"y" : 190,
					"image" : BACK_IMG_PATH + "boardback_mainboxlefttop.sub",
				},
				{
					"name" : "RightTopSelf",
					"type" : "image",
					"x" : 318,
					"y" : 190,
					"image" : BACK_IMG_PATH + "boardback_mainboxrighttop.sub",
				},
				{
					"name" : "LeftBottomSelf",
					"type" : "image",
					"x" : 17,
					"y" : 190 + 15,
					"image" : BACK_IMG_PATH + "boardback_mainboxleftbottom.sub",
				},
				{
					"name" : "RightBottomSelf",
					"type" : "image",
					"x" : 318,
					"y" : 190 + 15,
					"image" : BACK_IMG_PATH + "boardback_mainboxrightbottom.sub",
				},
				{
					"name" : "topcenterImgSelf",
					"type" : "expanded_image",
					"x" : 17 + 15,
					"y" : 190,
					"image" : BACK_IMG_PATH + "boardback_topcenterImg.tga",
					"rect" : (0.0, 0.0, 16, 0),
				},
				{
					"name" : "bottomcenterImgSelf",
					"type" : "expanded_image",
					"x" : 17 + 15,
					"y" : 190 + 15,
					"image" : BACK_IMG_PATH + "boardback_bottomcenterImg.tga",
					"rect" : (0.0, 0.0, 16, 0),
				},
			),
		},
	),
}