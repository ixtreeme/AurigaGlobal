import uiscriptlocale

window = {
	"name" : "AttrDialogRemoveWindow",
	"x" : 0,
	"y" : 0,
	"style" : ("movable", "float",),
	"width" : 205,
	"height" : 250,
	"children" :
	(
		{
			"name" : "board",
			"type" : "board",
			"style" : ("attach",),
			"x" : 0,
			"y" : 0,
			"width" : 205,
			"height" : 250,
			"children" :
			(
				{
					"name" : "TitleBar",
					"type" : "titlebar",
					"style" : ("attach",),
					"x" : 6,
					"y" : 6,
					"width" : 190,
					"color" : "yellow",
					"children" :
					(
						{
							"name" : "TitleName",
							"type" : "text",
							"x" : 95,
							"y" : 3,
							"text":uiscriptlocale.ATTR_COSTUME_REMOVE,
							"text_horizontal_align" : "center"
						},
					),
				},
				{
					"name" : "ItemSlot",
					"type" : "grid_table",
					"x" : 205 / 2 - 16,
					"y" : 40,
					"start_index" : 0,
					"x_count" : 1,
					"y_count" : 3,
					"x_step" : 32,
					"y_step" : 32,
					"image" : "d:/ymir work/ui/public/Slot_Base.sub"
				},
				{
					"name" : "info",
					"type" : "text",
					"x" : 205 / 2,
					"y" : 140,
					"text" : uiscriptlocale.ATTR_COSTUME_REMOVE_CHOOSE,
					"text_horizontal_align" : "center",
				},
				{
					"name" : "bns0",
					"type" : "radio_button",
					"x" : 14,
					"y" : 155,
					"text" : "",
					"default_image" : "d:/ymir work/ui/public/XLarge_Button_01.sub",
					"over_image" : "d:/ymir work/ui/public/XLarge_Button_02.sub",
					"down_image" : "d:/ymir work/ui/public/XLarge_Button_03.sub",
				},
				{
					"name" : "bns1",
					"type" : "radio_button",
					"x" : 14,
					"y" : 185,
					"text" : "",
					"default_image" : "d:/ymir work/ui/public/XLarge_Button_01.sub",
					"over_image" : "d:/ymir work/ui/public/XLarge_Button_02.sub",
					"down_image" : "d:/ymir work/ui/public/XLarge_Button_03.sub",
				},
				{
					"name" : "AcceptButton",
					"type" : "button",
					"x" : 40,
					"y" : 215,
					"text" : uiscriptlocale.ATTR_COSTUME_REMOVE_ACCEPT,
					"default_image" : "d:/ymir work/ui/public/middle_button_01.sub",
					"over_image" : "d:/ymir work/ui/public/middle_button_02.sub",
					"down_image" : "d:/ymir work/ui/public/middle_button_03.sub",
				},
				{
					"name" : "CancelButton",
					"type" : "button",
					"x" : 114,
					"y" : 215,
					"text" : uiscriptlocale.ATTR_COSTUME_REMOVE_CLOSE,
					"default_image" : "d:/ymir work/ui/public/middle_button_01.sub",
					"over_image" : "d:/ymir work/ui/public/middle_button_02.sub",
					"down_image" : "d:/ymir work/ui/public/middle_button_03.sub",
				},	
			),
		},
	),
}

