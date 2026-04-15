import uiScriptLocale
uiscriptlocale = uiScriptLocale
WIN_W = 610
WIN_H = 520

SLOT_X = 5
SLOT_Y = 3
SLOT_STEP = 32

RIGHT_X = 310
RIGHT_W = WIN_W - RIGHT_X - 15

# kis ado rendszer: nincs kivetett ado panel

window = {
	"name" : "GuildRenewalWindow",
	"x" : SCREEN_WIDTH/2 - WIN_W/2,
	"y" : SCREEN_HEIGHT/2 - WIN_H/2,
	"style" : ("movable", "float",),
	"width" : WIN_W,
	"height" : WIN_H,
	"children" :
	(
		{
			"name" : "Board",
			"type" : "board",
			"style" : ("attach",),
			"x" : 0,
			"y" : 0,
			"width" : WIN_W,
			"height" : WIN_H,
			"children" :
			(
				{
					"name" : "TitleBar",
					"type" : "titlebar",
					"style" : ("attach",),
					"x" : 8,
					"y" : 8,
					"width" : WIN_W-15,
					"children" :
					(
						{ "name":"TitleName", "type":"text", "x":0, "y":3, "text":uiscriptlocale.CEH_FEJLESZTES, "horizontal_align":"center", "text_horizontal_align":"center" },
					),
				},

				# STORAGE (5x3)
				{
					"name" : "StorageSlots",
					"type" : "grid_table",
					"x" : 15,
					"y" : 40,
					"start_index" : 0,
					"x_count" : SLOT_X,
					"y_count" : SLOT_Y,
					"x_step" : SLOT_STEP,
					"y_step" : SLOT_STEP,
					"image" : "d:/ymir work/ui/public/Slot_Base.sub",
				},

				{ "name":"StorageMoneyLabel", "type":"text", "x":15, "y":40 + SLOT_Y*SLOT_STEP + 8, "text":uiscriptlocale.CEH_LELTAR_YANG },
				{ "name":"StorageMoneyText", "type":"text", "x":140, "y":40 + SLOT_Y*SLOT_STEP + 8, "text":"0" },

				{
					"name" : "DepositYangButton",
					"type" : "button",
					"x" : 15,
					"y" : 40 + SLOT_Y*SLOT_STEP + 30,
					"text" : uiscriptlocale.CEH_YANG_BEFKTETES,
					"default_image" : "d:/ymir work/ui/public/large_button_01.sub",
					"over_image" : "d:/ymir work/ui/public/large_button_02.sub",
					"down_image" : "d:/ymir work/ui/public/large_button_03.sub",
				},

				# CONTRIBUTIONS TABLE (LEFT)
				{ "name":"ContribTitle", "type":"text", "x":15, "y":190, "text":uiscriptlocale.CEH_BEFIZETESEK },

				# header row
				{ "name":"ContribHeaderName",   "type":"text", "x":20,  "y":212, "text":uiscriptlocale.CEH_NEV },
				{ "name":"ContribHeaderYang",   "type":"text", "x":185, "y":212, "text":"Befizetett Yang" },

				# item ikonok a tablazat fejleceben (a kovetkezo fejleszteshez szukseges targyak)
				{
					"name" : "ContribHeaderItems",
					"type" : "grid_table",
					"x" : 295,
					"y" : 200,
					"start_index" : 0,
					"x_count" : 5,
					"y_count" : 1,
					"x_step" : 56,
					"y_step" : SLOT_STEP,
					"image" : "d:/ymir work/ui/public/Slot_Base.sub",
				},

				{
					"name" : "ContribList",
					"type" : "listboxex",
					"x" : 15,
					"y" : 232,
					# lista menjen ki a jobb szelig
					"width" : WIN_W - 40,
					# legyen hely az also gombnak
					"height" : 238,
					"itemsize_x" : WIN_W - 40,
					"itemsize_y" : 16,
					"itemstep" : 16,
					"viewcount" : 16,
				},
				{
					"name" : "ContribScroll",
					"type" : "scrollbar",
					"x" : WIN_W - 22,
					"y" : 232,
					"size" : 238,
				},

				# RIGHT: STATUS + NEXT LEVEL REQ
				{ "name":"GuildLevelLabel", "type":"text", "x":RIGHT_X, "y":40, "text":uiscriptlocale.CEH_LVL },
				{ "name":"GuildLevelText", "type":"text", "x":RIGHT_X+70, "y":40, "text":"0" },

				{ "name":"PaidStatusLabel", "type":"text", "x":RIGHT_X+150, "y":40, "text":uiscriptlocale.CEH_STAT },
				{ "name":"PaidStatusText", "type":"text", "x":RIGHT_X+210, "y":40, "text":uiscriptlocale.CEH_NEM_FIZETETT },

				{ "name":"ReqTitle", "type":"text", "x":RIGHT_X, "y":70, "text":uiscriptlocale.CEH_KOVI_LVL_IGENY },

				{ "name":"ReqYangLabel", "type":"text", "x":RIGHT_X, "y":92, "text":uiscriptlocale.CEH_HATRALEVO_YANG },
				{ "name":"ReqYangText", "type":"text", "x":RIGHT_X+110, "y":92, "text":uiscriptlocale.CEH_FIZETETT },

				{ "name":"ReqYangTotalLabel", "type":"text", "x":RIGHT_X, "y":112, "text":uiscriptlocale.CEH_IGENY },
				{ "name":"ReqYangTotalText", "type":"text", "x":RIGHT_X+110, "y":112, "text":"0" },

				{
					"name" : "ReqSlots",
					"type" : "grid_table",
					"x" : RIGHT_X,
					"y" : 135,
					"start_index" : 0,
					"x_count" : 5,
					"y_count" : 1,
					"x_step" : SLOT_STEP,
					"y_step" : SLOT_STEP,
					"image" : "d:/ymir work/ui/public/Slot_Base.sub",
				},

				{
					"name" : "LevelUpButton",
					"type" : "button",
					"x" : RIGHT_X,
					"y" : 175,
					"text" : uiscriptlocale.CEH_FEJLESZTES2,
					"default_image" : "d:/ymir work/ui/public/large_button_01.sub",
					"over_image" : "d:/ymir work/ui/public/large_button_02.sub",
					"down_image" : "d:/ymir work/ui/public/large_button_03.sub",
				},

				# BEFIZETES gomb (tax panel helyett)
				{
					"name" : "PayTaxButton",
					"type" : "button",
					"x" : 15,
					"y" : 480,
					"text" : "Befizetes",
					"default_image" : "d:/ymir work/ui/public/large_button_01.sub",
					"over_image" : "d:/ymir work/ui/public/large_button_02.sub",
					"down_image" : "d:/ymir work/ui/public/large_button_03.sub",
				},
			),
		},
	),
}
