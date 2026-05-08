import uiscriptlocale
import localeinfo

LOCALE_PATH = "d:/ymir work/ui/game/guild/guildRankingList/"
GOLD_COLOR	= 0xFFFEE3AE
ROOT_PATH = "d:/ymir work/ui/public/"

window = {
	"name" : "GuildWindow_GuildRankingPageNonGuild",
	"x" : 0,
	"y" : 0,
	"style" : ("movable", "float",),
	"width" : 548,
	"height" : 316,
	"children" :
	(
		{
			"name" : "Board",
			"type" : "board_with_titlebar",
			"x" : 0,
			"y" : 0,
			"width" : 548,
			"height" : 316,
			"title" : uiscriptlocale.NOME_RANKING,
			"children" :
			(
				{
					"name" : "left_line",
					"type" : "line",
					"x" : 7,
					"y" : 33,
					"width" : 0,
					"height" : 243,
					"color" : 0xffAAA6A1,
				},
				{
					"name" : "right_line",
					"type" : "line",
					"x" : 504 + 12,
					"y" : 33,
					"width" : 0,
					"height" : 243,
					"color" : 0xffAAA6A1,
				},
				{
					"name" : "top_line",
					"type" : "line",
					"x" : 7,
					"y" : 31,
					"width" : 509,
					"height" : 0,
					"color" : 0xffAAA6A1,
				},
				{
					"name" : "bottom_line",
					"type" : "line",
					"x" : 7,
					"y" : 275,
					"width" : 509,
					"height" : 0,
					"color" : 0xffAAA6A1,
				},
				{
					"name" : "ItemTypeImg",
					"type" : "expanded_image",
					"x" : 10.5,
					"y" : 33,
					"width" : 10,
					"image" : LOCALE_PATH + "tab_menu_01.tga",
					"x_scale" : 1.145,
					"y_scale" : 1.0,
					"children" :
					(
						{
							"name" : "ResultNameText0",
							"type" : "text",
							"x" : 10,
							"y" : 4,
							"text" : "#"
						},
						{
							"name" : "ResultNameText1",
							"type" : "text",
							"x" : 52,
							"y" : 4,
							"text" : uiscriptlocale.NOME_GILDA
						},
						{
							"name" : "ResultNameText2",
							"type" : "text",
							"x" : 177,
							"y" : 4,
							"text" : uiscriptlocale.VITTORIE_GILDA
						},
						{
							"name" : "ResultNameText3",
							"type" : "text",
							"x" : 257,
							"y" : 4,
							"text" : uiscriptlocale.PAREGGI_GILDA
						},
						{
							"name" : "ResultNameText4",
							"type" : "text",
							"x" : 335,
							"y" : 4,
							"text" : uiscriptlocale.SCONFITTE_GILDA
						},
						{
							"name" : "ResultNameText5",
							"type" : "text", "x" : 422,
							"y" : 4,
							"text" : uiscriptlocale.TROFEI_GILDA
						},
					),
				},
				{
					"name" : "league2_btn",
					"type" : "radio_button",
					"x" : 2 + 9,
					"y" : 283,
					"text" : uiscriptlocale.PRIMO_POSTO,
					"default_image" : ROOT_PATH + "Gilda/1POSTO/gilda_normal.tga",
					"over_image" : ROOT_PATH + "Gilda/1POSTO/gilda_over.tga",
					"down_image" : ROOT_PATH + "Gilda/1POSTO/gilda_down.tga"
				},
				{
					"name" : "league1_btn",
					"type" : "radio_button",
					"x" : 88 + 9,
					"y" : 283,
					"text" : uiscriptlocale.SECONDO_POSTO,
					"default_image" : ROOT_PATH + "Gilda/2POSTO/gilda_normal.tga",
					"over_image" : ROOT_PATH + "Gilda/2POSTO/gilda_over.tga",
					"down_image" : ROOT_PATH + "Gilda/2POSTO/gilda_down.tga"
				},
				{
					"name" : "league0_btn",
					"type" : "radio_button",
					"x" : 174 + 9,
					"y" : 283,
					"text" : uiscriptlocale.TERZO_POSTO,
					"default_image" : ROOT_PATH + "Gilda/3POSTO/gilda_normal.tga",
					"over_image" : ROOT_PATH + "Gilda/3POSTO/gilda_over.tga",
					"down_image" : ROOT_PATH + "Gilda/3POSTO/gilda_down.tga"
				},
				{
					"name" : "scrollbar",
					"type" : "scrollbar",
					"x" : 522,
					"y" : 36,
					"size" : 240,
				},
			),
		},
	),
}