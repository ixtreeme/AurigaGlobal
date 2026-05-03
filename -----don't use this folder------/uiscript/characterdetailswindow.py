import uiscriptlocale
import localeinfo

LOCALE_PATH = uiscriptlocale.WINDOWS_PATH

MAINBOARD_WIDTH = 520
MAINBOARD_HEIGHT = 405

LABEL_START_Y = 55
LABEL_HEIGHT = 17
LABEL_GAP = LABEL_HEIGHT + 7

LEFT_LABEL_X = 20
RIGHT_LABEL_X = 260

LEFT_VALUE_X = 200
RIGHT_VALUE_X = 430
VALUE_WIDTH = 60

TEXT_Y_OFF = 1
VALUE_Y_OFF = -2

def _RowY(row):
	return LABEL_START_Y + LABEL_GAP * row

window = {
	"name" : "CharacterDetailsWindow",
	"style" : ("float", "movable", ),

	"x" : 274,
	"y" : (SCREEN_HEIGHT - 398) / 2,

	"width" : MAINBOARD_WIDTH,
	"height" : MAINBOARD_HEIGHT,

	"children" :
	(
		{
			"name" : "MainBoard",
			"type" : "board",
			"style" : ("attach","ltr"),

			"x" : 0,
			"y" : 0,

			"width" : MAINBOARD_WIDTH,
			"height" : MAINBOARD_HEIGHT,

			"children" :
			(
				{
					"name" : "TitleBar",
					"type" : "titlebar",
					"style" : ("attach",),

					"x" : 6,
					"y" : 7,

					"width" : MAINBOARD_WIDTH - 13,

					"children" :
					(
						{ "name" : "TitleName", "type" : "text", "x" : 0, "y" : 0, "text": localeinfo.DETAILS_TITLE, "all_align":"center" },
					),
				},
				{
					"name" : "ScrollBar",
					"type" : "scrollbar",

					"x" : 25,
					"y" : 31,
					"size" : MAINBOARD_HEIGHT - 40,
					"horizontal_align" : "right",
				},
				{ "name":"PvmTitle", "type":"text", "x":- 50, "y":-  163, "text":"PVM BONUS", "all_align":"center", "fontsize":"LARGE", "color":0xFFFF0303 },
				{ "name":"PvpTitle", "type":"text", "x":130, "y": - 163, "text":"PVP BONUS", "all_align":"center", "fontsize":"LARGE", "color":0xFFFF0303 },
				# -------- LABEL TEXTS --------
				{ "name":"labelname0", "type":"text", "x":LEFT_LABEL_X, "y":_RowY(0)+TEXT_Y_OFF, "text":"", "color":0xFFFFFFFF },
				{ "name":"labelname1", "type":"text", "x":LEFT_LABEL_X, "y":_RowY(1)+TEXT_Y_OFF, "text":"", "color":0xFFFFFFFF },
				{ "name":"labelname2", "type":"text", "x":LEFT_LABEL_X, "y":_RowY(2)+TEXT_Y_OFF, "text":"", "color":0xFFFFFFFF },
				{ "name":"labelname3", "type":"text", "x":LEFT_LABEL_X, "y":_RowY(3)+TEXT_Y_OFF, "text":"", "color":0xFFFFFFFF },
				{ "name":"labelname4", "type":"text", "x":LEFT_LABEL_X, "y":_RowY(4)+TEXT_Y_OFF, "text":"", "color":0xFFFFFFFF },
				{ "name":"labelname5", "type":"text", "x":LEFT_LABEL_X, "y":_RowY(5)+TEXT_Y_OFF, "text":"", "color":0xFFFFFFFF },
				{ "name":"labelname6", "type":"text", "x":LEFT_LABEL_X, "y":_RowY(6)+TEXT_Y_OFF, "text":"", "color":0xFFFFFFFF },
				{ "name":"labelname7", "type":"text", "x":LEFT_LABEL_X, "y":_RowY(7)+TEXT_Y_OFF, "text":"", "color":0xFFFFFFFF },
				{ "name":"labelname8", "type":"text", "x":LEFT_LABEL_X, "y":_RowY(8)+TEXT_Y_OFF, "text":"", "color":0xFFFFFFFF },
				{ "name":"labelname9", "type":"text", "x":LEFT_LABEL_X, "y":_RowY(9)+TEXT_Y_OFF, "text":"", "color":0xFFFFFFFF },
				{ "name":"labelname10", "type":"text", "x":LEFT_LABEL_X, "y":_RowY(10)+TEXT_Y_OFF, "text":"", "color":0xFFFFFFFF },
				{ "name":"labelname11", "type":"text", "x":RIGHT_LABEL_X, "y":_RowY(0)+TEXT_Y_OFF, "text":"", "color":0xFFFFFFFF },
				{ "name":"labelname12", "type":"text", "x":RIGHT_LABEL_X, "y":_RowY(1)+TEXT_Y_OFF, "text":"", "color":0xFFFFFFFF },
				{ "name":"labelname13", "type":"text", "x":RIGHT_LABEL_X, "y":_RowY(2)+TEXT_Y_OFF, "text":"", "color":0xFFFFFFFF },
				{ "name":"labelname14", "type":"text", "x":RIGHT_LABEL_X, "y":_RowY(3)+TEXT_Y_OFF, "text":"", "color":0xFFFFFFFF },
				{ "name":"labelname15", "type":"text", "x":RIGHT_LABEL_X, "y":_RowY(4)+TEXT_Y_OFF, "text":"", "color":0xFFFFFFFF },
				{ "name":"labelname16", "type":"text", "x":RIGHT_LABEL_X, "y":_RowY(5)+TEXT_Y_OFF, "text":"", "color":0xFFFFFFFF },
				{ "name":"labelname17", "type":"text", "x":RIGHT_LABEL_X, "y":_RowY(6)+TEXT_Y_OFF, "text":"", "color":0xFFFFFFFF },
				{ "name":"labelname18", "type":"text", "x":RIGHT_LABEL_X, "y":_RowY(7)+TEXT_Y_OFF, "text":"", "color":0xFFFFFFFF },
				{ "name":"labelname19", "type":"text", "x":RIGHT_LABEL_X, "y":_RowY(8)+TEXT_Y_OFF, "text":"", "color":0xFFFFFFFF },
				{ "name":"labelname20", "type":"text", "x":RIGHT_LABEL_X, "y":_RowY(9)+TEXT_Y_OFF, "text":"", "color":0xFFFFFFFF },
				{ "name":"labelname21", "type":"text", "x":RIGHT_LABEL_X, "y":_RowY(10)+TEXT_Y_OFF, "text":"", "color":0xFFFFFFFF },

				# -------- VALUE BOXES --------
				{
					"name":"label0", "type":"thinboard_circle",
					"x":LEFT_VALUE_X, "y":_RowY(0)+VALUE_Y_OFF,
					"width":VALUE_WIDTH, "height":LABEL_HEIGHT,
					"children":(
						{ "name":"labelvalue0", "type":"text", "x":0, "y":0, "text":"", "all_align":"center" },
					),
				},
				{
					"name":"label1", "type":"thinboard_circle",
					"x":LEFT_VALUE_X, "y":_RowY(1)+VALUE_Y_OFF,
					"width":VALUE_WIDTH, "height":LABEL_HEIGHT,
					"children":(
						{ "name":"labelvalue1", "type":"text", "x":0, "y":0, "text":"", "all_align":"center" },
					),
				},
				{
					"name":"label2", "type":"thinboard_circle",
					"x":LEFT_VALUE_X, "y":_RowY(2)+VALUE_Y_OFF,
					"width":VALUE_WIDTH, "height":LABEL_HEIGHT,
					"children":(
						{ "name":"labelvalue2", "type":"text", "x":0, "y":0, "text":"", "all_align":"center" },
					),
				},
				{
					"name":"label3", "type":"thinboard_circle",
					"x":LEFT_VALUE_X, "y":_RowY(3)+VALUE_Y_OFF,
					"width":VALUE_WIDTH, "height":LABEL_HEIGHT,
					"children":(
						{ "name":"labelvalue3", "type":"text", "x":0, "y":0, "text":"", "all_align":"center" },
					),
				},
				{
					"name":"label4", "type":"thinboard_circle",
					"x":LEFT_VALUE_X, "y":_RowY(4)+VALUE_Y_OFF,
					"width":VALUE_WIDTH, "height":LABEL_HEIGHT,
					"children":(
						{ "name":"labelvalue4", "type":"text", "x":0, "y":0, "text":"", "all_align":"center" },
					),
				},
				{
					"name":"label5", "type":"thinboard_circle",
					"x":LEFT_VALUE_X, "y":_RowY(5)+VALUE_Y_OFF,
					"width":VALUE_WIDTH, "height":LABEL_HEIGHT,
					"children":(
						{ "name":"labelvalue5", "type":"text", "x":0, "y":0, "text":"", "all_align":"center" },
					),
				},
				{
					"name":"label6", "type":"thinboard_circle",
					"x":LEFT_VALUE_X, "y":_RowY(6)+VALUE_Y_OFF,
					"width":VALUE_WIDTH, "height":LABEL_HEIGHT,
					"children":(
						{ "name":"labelvalue6", "type":"text", "x":0, "y":0, "text":"", "all_align":"center" },
					),
				},
				{
					"name":"label7", "type":"thinboard_circle",
					"x":LEFT_VALUE_X, "y":_RowY(7)+VALUE_Y_OFF,
					"width":VALUE_WIDTH, "height":LABEL_HEIGHT,
					"children":(
						{ "name":"labelvalue7", "type":"text", "x":0, "y":0, "text":"", "all_align":"center" },
					),
				},
				{
					"name":"label8", "type":"thinboard_circle",
					"x":LEFT_VALUE_X, "y":_RowY(8)+VALUE_Y_OFF,
					"width":VALUE_WIDTH, "height":LABEL_HEIGHT,
					"children":(
						{ "name":"labelvalue8", "type":"text", "x":0, "y":0, "text":"", "all_align":"center" },
					),
				},
				{
					"name":"label9", "type":"thinboard_circle",
					"x":LEFT_VALUE_X, "y":_RowY(9)+VALUE_Y_OFF,
					"width":VALUE_WIDTH, "height":LABEL_HEIGHT,
					"children":(
						{ "name":"labelvalue9", "type":"text", "x":0, "y":0, "text":"", "all_align":"center" },
					),
				},
				{
					"name":"label10", "type":"thinboard_circle",
					"x":LEFT_VALUE_X, "y":_RowY(10)+VALUE_Y_OFF,
					"width":VALUE_WIDTH, "height":LABEL_HEIGHT,
					"children":(
						{ "name":"labelvalue10", "type":"text", "x":0, "y":0, "text":"", "all_align":"center" },
					),
				},
				{
					"name":"label11", "type":"thinboard_circle",
					"x":RIGHT_VALUE_X, "y":_RowY(0)+VALUE_Y_OFF,
					"width":VALUE_WIDTH, "height":LABEL_HEIGHT,
					"children":(
						{ "name":"labelvalue11", "type":"text", "x":0, "y":0, "text":"", "all_align":"center" },
					),
				},
				{
					"name":"label12", "type":"thinboard_circle",
					"x":RIGHT_VALUE_X, "y":_RowY(1)+VALUE_Y_OFF,
					"width":VALUE_WIDTH, "height":LABEL_HEIGHT,
					"children":(
						{ "name":"labelvalue12", "type":"text", "x":0, "y":0, "text":"", "all_align":"center" },
					),
				},
				{
					"name":"label13", "type":"thinboard_circle",
					"x":RIGHT_VALUE_X, "y":_RowY(2)+VALUE_Y_OFF,
					"width":VALUE_WIDTH, "height":LABEL_HEIGHT,
					"children":(
						{ "name":"labelvalue13", "type":"text", "x":0, "y":0, "text":"", "all_align":"center" },
					),
				},
				{
					"name":"label14", "type":"thinboard_circle",
					"x":RIGHT_VALUE_X, "y":_RowY(3)+VALUE_Y_OFF,
					"width":VALUE_WIDTH, "height":LABEL_HEIGHT,
					"children":(
						{ "name":"labelvalue14", "type":"text", "x":0, "y":0, "text":"", "all_align":"center" },
					),
				},
				{
					"name":"label15", "type":"thinboard_circle",
					"x":RIGHT_VALUE_X, "y":_RowY(4)+VALUE_Y_OFF,
					"width":VALUE_WIDTH, "height":LABEL_HEIGHT,
					"children":(
						{ "name":"labelvalue15", "type":"text", "x":0, "y":0, "text":"", "all_align":"center" },
					),
				},
				{
					"name":"label16", "type":"thinboard_circle",
					"x":RIGHT_VALUE_X, "y":_RowY(5)+VALUE_Y_OFF,
					"width":VALUE_WIDTH, "height":LABEL_HEIGHT,
					"children":(
						{ "name":"labelvalue16", "type":"text", "x":0, "y":0, "text":"", "all_align":"center" },
					),
				},
				{
					"name":"label17", "type":"thinboard_circle",
					"x":RIGHT_VALUE_X, "y":_RowY(6)+VALUE_Y_OFF,
					"width":VALUE_WIDTH, "height":LABEL_HEIGHT,
					"children":(
						{ "name":"labelvalue17", "type":"text", "x":0, "y":0, "text":"", "all_align":"center" },
					),
				},
				{
					"name":"label18", "type":"thinboard_circle",
					"x":RIGHT_VALUE_X, "y":_RowY(7)+VALUE_Y_OFF,
					"width":VALUE_WIDTH, "height":LABEL_HEIGHT,
					"children":(
						{ "name":"labelvalue18", "type":"text", "x":0, "y":0, "text":"", "all_align":"center" },
					),
				},
				{
					"name":"label19", "type":"thinboard_circle",
					"x":RIGHT_VALUE_X, "y":_RowY(8)+VALUE_Y_OFF,
					"width":VALUE_WIDTH, "height":LABEL_HEIGHT,
					"children":(
						{ "name":"labelvalue19", "type":"text", "x":0, "y":0, "text":"", "all_align":"center" },
					),
				},
				{
					"name":"label20", "type":"thinboard_circle",
					"x":RIGHT_VALUE_X, "y":_RowY(9)+VALUE_Y_OFF,
					"width":VALUE_WIDTH, "height":LABEL_HEIGHT,
					"children":(
						{ "name":"labelvalue20", "type":"text", "x":0, "y":0, "text":"", "all_align":"center" },
					),
				},
				{
					"name":"label21", "type":"thinboard_circle",
					"x":RIGHT_VALUE_X, "y":_RowY(10)+VALUE_Y_OFF,
					"width":VALUE_WIDTH, "height":LABEL_HEIGHT,
					"children":(
						{ "name":"labelvalue21", "type":"text", "x":0, "y":0, "text":"", "all_align":"center" },
					),
				},
			)

		},
	),
}
