import uiScriptLocale
uiscriptlocale = uiScriptLocale
WIN_W = 520
WIN_H = 300

ROW_Y0 = 104

window = {
	"name" : "GuildRenewalTaxSetting",
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
						{ "name":"TitleName", "type":"text", "x":0, "y":3, "text":uiscriptlocale.CEH_ADO_KIVETESE, "horizontal_align":"center", "text_horizontal_align":"center" },
					),
				},

				{ "name":"DaysLabel", "type":"text", "x":20, "y":40, "text":uiscriptlocale.CEH_HATARIDO },
				{
					"name" : "DaysSlot",
					"type" : "slotbar",
					"x" : 140,
					"y" : 36,
					"width" : 60,
					"height" : 18,
					"children" :
					(
						{ "name":"DaysValue", "type":"editline", "x":3, "y":3, "width":60, "height":18, "input_limit":2 },
					),
				},

				{ "name":"YangLabel", "type":"text", "x":240, "y":40, "text":uiscriptlocale.CEH_YANG_PER_FO },
				{
					"name" : "YangSlot",
					"type" : "slotbar",
					"x" : 320,
					"y" : 36,
					"width" : 160,
					"height" : 18,
					"children" :
					(
						{ "name":"YangValue", "type":"editline", "x":3, "y":3, "width":160, "height":18, "input_limit":18 },
					),
				},

				{ "name":"Hint", "type":"text", "x":20, "y":60, "text":uiscriptlocale.CEH_TAX_PANEL_MSG },
				{ "name":"Hint", "type":"text", "x":20, "y":75, "text":uiscriptlocale.CEH_TAX_PANEL_MSG1 },


				{
					"name" : "ReqItemSlots",
					"type" : "grid_table",
					"x" : 20,
					"y" : ROW_Y0,
					"start_index" : 0,
					"x_count" : 1,
					"y_count" : 5,
					"x_step" : 32,
					"y_step" : 32,
					"image" : "d:/ymir work/ui/public/Slot_Base.sub",
				},

				{ "name":"DbHdr", "type":"text", "x":420, "y":ROW_Y0 - 16, "text":uiscriptlocale.CEH_DB_PER_FO },

				{ "name":"NameText0", "type":"text", "x":60, "y":ROW_Y0 + 0*32 + 8, "text":"-" },
				{ "name":"NameText1", "type":"text", "x":60, "y":ROW_Y0 + 1*32 + 8, "text":"-" },
				{ "name":"NameText2", "type":"text", "x":60, "y":ROW_Y0 + 2*32 + 8, "text":"-" },
				{ "name":"NameText3", "type":"text", "x":60, "y":ROW_Y0 + 3*32 + 8, "text":"-" },
				{ "name":"NameText4", "type":"text", "x":60, "y":ROW_Y0 + 4*32 + 8, "text":"-" },

				{
					"name" : "CountSlot0",
					"type" : "button",
					"x" : 410,
					"y" : ROW_Y0 + 0*32 + 4,
					"text" : "0",
					"default_image" : "d:/ymir work/ui/public/middle_button_01.sub",
					"over_image" : "d:/ymir work/ui/public/middle_button_02.sub",
					"down_image" : "d:/ymir work/ui/public/middle_button_03.sub",
				},
				{
					"name" : "CountSlot1",
					"type" : "button",
					"x" : 410,
					"y" : ROW_Y0 + 1*32 + 4,
					"text" : "0",
					"default_image" : "d:/ymir work/ui/public/middle_button_01.sub",
					"over_image" : "d:/ymir work/ui/public/middle_button_02.sub",
					"down_image" : "d:/ymir work/ui/public/middle_button_03.sub",
				},
				{
					"name" : "CountSlot2",
					"type" : "button",
					"x" : 410,
					"y" : ROW_Y0 + 2*32 + 4,
					"text" : "0",
					"default_image" : "d:/ymir work/ui/public/middle_button_01.sub",
					"over_image" : "d:/ymir work/ui/public/middle_button_02.sub",
					"down_image" : "d:/ymir work/ui/public/middle_button_03.sub",
				},
				{
					"name" : "CountSlot3",
					"type" : "button",
					"x" : 410,
					"y" : ROW_Y0 + 3*32 + 4,
					"text" : "0",
					"default_image" : "d:/ymir work/ui/public/middle_button_01.sub",
					"over_image" : "d:/ymir work/ui/public/middle_button_02.sub",
					"down_image" : "d:/ymir work/ui/public/middle_button_03.sub",
				},
				{
					"name" : "CountSlot4",
					"type" : "button",
					"x" : 410,
					"y" : ROW_Y0 + 4*32 + 4,
					"text" : "0",
					"default_image" : "d:/ymir work/ui/public/middle_button_01.sub",
					"over_image" : "d:/ymir work/ui/public/middle_button_02.sub",
					"down_image" : "d:/ymir work/ui/public/middle_button_03.sub",
				},

				{
					"name" : "AcceptButton",
					"type" : "button",
					"x" : - 61 - 5,
					"y" : WIN_H - 40,
					"horizontal_align" : "center",
					"text" : uiScriptLocale.OK,
					"default_image" : "d:/ymir work/ui/public/middle_button_01.sub",
					"over_image" : "d:/ymir work/ui/public/middle_button_02.sub",
					"down_image" : "d:/ymir work/ui/public/middle_button_03.sub",
				},
				{
					"name" : "CancelButton",
					"type" : "button",
					"x" : 5,
					"y" : WIN_H - 40,
					"horizontal_align" : "center",
					"text" : uiScriptLocale.CANCEL,
					"default_image" : "d:/ymir work/ui/public/middle_button_01.sub",
					"over_image" : "d:/ymir work/ui/public/middle_button_02.sub",
					"down_image" : "d:/ymir work/ui/public/middle_button_03.sub",
				},
			),
		},
	),
}
