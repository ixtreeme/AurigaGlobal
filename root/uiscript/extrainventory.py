import uiscriptlocale

WINDOW_WIDTH = 177
WINDOW_HEIGHT = 394 + 15 + 40

window = {
	"name" : "ExtraInventory",

	"x" : (SCREEN_WIDTH - WINDOW_WIDTH) / 2,
	"y" : (SCREEN_HEIGHT - WINDOW_HEIGHT) / 2,

	"style" : ("movable", "float",),

	"width" : WINDOW_WIDTH,
	"height" : WINDOW_HEIGHT,

	"children" :
	(
		{
			"name" : "board",
			"type" : "board",

			"style" : ("attach",),

			"x" : 0,
			"y" : 0,

			"width" : WINDOW_WIDTH,
			"height" : WINDOW_HEIGHT,

			"children" :
			(
				## Separate
				#{
				#	"name" : "SeparateBaseImage",
				#	"type" : "image",
				#	"style" : ("attach",),
				#
				#	"x" : 8,
				#	"y" : 7,
				#
				#	"image" : "d:/ymir work/ui/pattern/titlebar_inv_refresh_baseframe.tga",
				#
				#	"children" :
				#	(
				#		# Separate Button (38x24)
				#		{
				#			"name" : "RefreshButton",
				#			"type" : "button",
				#
				#			"x" : 11,
				#			"y" : 3,
				#
				#			"tooltip_text" : uiscriptlocale.INVENTORY_SPECIAL_SEPARATE,
				#
				#			"default_image" : "d:/ymir work/ui/game/inventory/refresh_small_button_01.sub",
				#			"over_image" : "d:/ymir work/ui/game/inventory/refresh_small_button_02.sub",
				#			"down_image" : "d:/ymir work/ui/game/inventory/refresh_small_button_03.sub",
				#			"disable_image" : "d:/ymir work/ui/game/inventory/refresh_small_button_04.sub",
				#		},
				#	),
				#},
				## Title
				{
					"name" : "TitleBar",
					"type" : "titlebar",
					"style" : ("attach",),

					#"x" : 8 +38,
					"x" : 8,
					"y" : 7,

					#"width" : WINDOW_WIDTH-15 - 38,
					"width" : WINDOW_WIDTH-15,
					"color" : "yellow",

					"children" :
					(
						{ "name" : "TitleName", "type" : "text", "x" : 0, "y" : -1, "text" : uiscriptlocale.EXTRA_INVENTORY_TITLE, "all_align" : "center" },
					),
				},
				## Item Slot
				{
					"name" : "ItemSlot",
					"type" : "grid_table",

					"x" : 8,
					"y" : 32,

					"start_index" : 0,
					"x_count" : EXTRA_INVENTORY_PAGE_COLUMN,
					"y_count" : EXTRA_INVENTORY_PAGE_ROW,
					"x_step" : 32,
					"y_step" : 32,

					"image" : "d:/ymir work/ui/public/Slot_Base.sub"
				},
				{
					"name":"cover_open_8",
					"type":"button",
					"vertical_align":"bottom",
					
					"x":8,
					"y":24 + (32 * 0)+ 137,

					"default_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
					"over_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
					"down_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
				},
				{
					"name":"cover_close_8",
					"type":"image",
					"vertical_align":"bottom",
					
					"x":8,
					"y":24 + (32 * 0)+ 137,

					"image":"d:/ymir work/ui/ex_inven_cover_button_close.sub",
				},
				{
					"name":"cover_open_7",
					"type":"button",
					"vertical_align":"bottom",
					
					"x":8,
					"y":24 + (32 * 1)+ 137,

					"default_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
					"over_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
					"down_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
				},
				{
					"name":"cover_close_7",
					"type":"image",
					"vertical_align":"bottom",
					
					"x":8,
					"y":24 + (32 * 1)+ 137,

					"image":"d:/ymir work/ui/ex_inven_cover_button_close.sub",
				},
				{
					"name":"cover_open_6",
					"type":"button",
					"vertical_align":"bottom",
					
					"x":8,
					"y":24 + (32 * 2)+ 137,

					"default_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
					"over_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
					"down_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
				},
				{
					"name":"cover_close_6",
					"type":"image",
					"vertical_align":"bottom",
					
					"x":8,
					"y":24 + (32 * 2)+ 137,

					"image":"d:/ymir work/ui/ex_inven_cover_button_close.sub",
				},
				{
					"name":"cover_open_5",
					"type":"button",
					"vertical_align":"bottom",
					
					"x":8,
					"y":24 + (32 * 3)+ 137,

					"default_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
					"over_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
					"down_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
				},
				{
					"name":"cover_close_5",
					"type":"image",
					"vertical_align":"bottom",
					
					"x":8,
					"y":24 + (32 * 3)+ 137,

					"image":"d:/ymir work/ui/ex_inven_cover_button_close.sub",
				},
				{
					"name":"cover_open_4",
					"type":"button",
					"vertical_align":"bottom",
					
					"x":8,
					"y":24 + (32 * 4)+ 137,

					"default_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
					"over_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
					"down_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
				},
				{
					"name":"cover_close_4",
					"type":"image",
					"vertical_align":"bottom",
					
					"x":8,
					"y":24 + (32 * 4)+ 137,

					"image":"d:/ymir work/ui/ex_inven_cover_button_close.sub",
				},
				{
					"name":"cover_open_3",
					"type":"button",
					"vertical_align":"bottom",
					
					"x":8,
					"y":24 + (32 * 5)+ 137,

					"default_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
					"over_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
					"down_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
				},
				{
					"name":"cover_close_3",
					"type":"image",
					"vertical_align":"bottom",
					
					"x":8,
					"y":24 + (32 * 5)+ 137,

					"image":"d:/ymir work/ui/ex_inven_cover_button_close.sub",
				},
				{
					"name":"cover_open_2",
					"type":"button",
					"vertical_align":"bottom",
					
					"x":8,
					"y":24 + (32 * 6)+ 137,

					"default_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
					"over_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
					"down_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
				},
				{
					"name":"cover_close_2",
					"type":"image",
					"vertical_align":"bottom",
					
					"x":8,
					"y":24 + (32 * 6)+ 137,

					"image":"d:/ymir work/ui/ex_inven_cover_button_close.sub",
				},
				{
					"name":"cover_open_1",
					"type":"button",
					"vertical_align":"bottom",
					
					"x":8,
					"y":24 + (32 * 7)+ 137,

					"default_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
					"over_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
					"down_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
				},
				{
					"name":"cover_close_1",
					"type":"image",
					"vertical_align":"bottom",
					
					"x":8,
					"y":24 + (32 * 7)+ 137,

					"image":"d:/ymir work/ui/ex_inven_cover_button_close.sub",
				},
				{
					"name":"cover_open_0",
					"type":"button",
					"vertical_align":"bottom",
					
					"x":8,
					"y":24 + (32 * 8)+ 137,

					"default_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
					"over_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
					"down_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
				},
				{
					"name":"cover_close_0",
					"type":"image",
					"vertical_align":"bottom",
					
					"x":8,
					"y":24 + (32 * 8)+ 137,

					"image":"d:/ymir work/ui/ex_inven_cover_button_close.sub",
				},
				## Button1
				{
					"name" : "Inventory_Tab_01",
					"type" : "radio_button",

					"x" : 10,
					"y" : 323,

					"default_image" : "d:/ymir work/ui/game/windows/tab_button_large_01.sub",
					"over_image" : "d:/ymir work/ui/game/windows/tab_button_large_02.sub",
					"down_image" : "d:/ymir work/ui/game/windows/tab_button_large_03.sub",

					"children" :
					(
						{
							"name" : "Text_Tab_01_Print",
							"type" : "text",

							"x" : 0,
							"y" : 0,

							"all_align" : "center",

							"text" : "I",
						},
					),
				},
				## Button2
				{
					"name" : "Inventory_Tab_02",
					"type" : "radio_button",

					"x" : 10 + 78,
					"y" : 323,

					"default_image" : "d:/ymir work/ui/game/windows/tab_button_large_01.sub",
					"over_image" : "d:/ymir work/ui/game/windows/tab_button_large_02.sub",
					"down_image" : "d:/ymir work/ui/game/windows/tab_button_large_03.sub",

					"children" :
					(
						{
							"name" : "Text_Tab_02_Print",
							"type" : "text",

							"x" : 0,
							"y" : 0,

							"all_align" : "center",

							"text" : "II",
						},
					),
				},
				
				## Button3
				{
					"name" : "Inventory_Tab_03",
					"type" : "radio_button",

					"x" : 10,
					"y" : 323 + 17,

					"default_image" : "d:/ymir work/ui/game/windows/tab_button_large_01.sub",
					"over_image" : "d:/ymir work/ui/game/windows/tab_button_large_02.sub",
					"down_image" : "d:/ymir work/ui/game/windows/tab_button_large_03.sub",

					"children" :
					(
						{
							"name" : "Text_Tab_02_Print",
							"type" : "text",

							"x" : 0,
							"y" : 0,

							"all_align" : "center",

							"text" : "III",
						},
					),
				},
				## Button4
				{
					"name" : "Inventory_Tab_04",
					"type" : "radio_button",

					"x" : 10 + 78,
					"y" : 323 + 17,

					"default_image" : "d:/ymir work/ui/game/windows/tab_button_large_01.sub",
					"over_image" : "d:/ymir work/ui/game/windows/tab_button_large_02.sub",
					"down_image" : "d:/ymir work/ui/game/windows/tab_button_large_03.sub",

					"children" :
					(
						{
							"name" : "Text_Tab_02_Print",
							"type" : "text",

							"x" : 0,
							"y" : 0,

							"all_align" : "center",

							"text" : "VI",
						},
					),
				},
				## CategoryButton1
				{
					"name" : "Cat_00",
					"type" : "radio_button",

					"x" : 10,
					"y" : 323 + 25 +15,
					
					
					"tooltip_text" : uiscriptlocale.BOOKS_TITLE,

					"default_image" : "d:/ymir work/ui/extra_inventory/cat_0.tga",
					"over_image" : "d:/ymir work/ui/extra_inventory/cat_0o.tga",
					"down_image" : "d:/ymir work/ui/extra_inventory/cat_0d.tga",
				},
				## CategoryButton2
				{
					"name" : "Cat_01",
					"type" : "radio_button",

					"x" : 10 + 41,
					"y" : 323 + 25 +15,
					
					
					"tooltip_text" : uiscriptlocale.UPPGRADE_TITLE,

					"default_image" : "d:/ymir work/ui/extra_inventory/cat_1.tga",
					"over_image" : "d:/ymir work/ui/extra_inventory/cat_1o.tga",
					"down_image" : "d:/ymir work/ui/extra_inventory/cat_1d.tga",
				},
				## CategoryButton3
				{
					"name" : "Cat_02",
					"type" : "radio_button",

					"x" : 10 + 82,
					"y" : 323 + 25 +15,
					
					
					"tooltip_text" : uiscriptlocale.STONES_TITLE,

					"default_image" : "d:/ymir work/ui/extra_inventory/cat_2.tga",
					"over_image" : "d:/ymir work/ui/extra_inventory/cat_2o.tga",
					"down_image" : "d:/ymir work/ui/extra_inventory/cat_2d.tga",
				},
				## CategoryButton3
				{
					"name" : "Cat_03",
					"type" : "radio_button",

					"x" : 10 + 123,
					"y" : 323 + 25 +15,
					
					
					"tooltip_text" : uiscriptlocale.COFFERS_TITLE,

					"default_image" : "d:/ymir work/ui/extra_inventory/cat_3.tga",
					"over_image" : "d:/ymir work/ui/extra_inventory/cat_3o.tga",
					"down_image" : "d:/ymir work/ui/extra_inventory/cat_3d.tga",
				},
				## CategoryButton4
				{
					"name" : "Cat_04",
					"type" : "radio_button",

					"x" : 10,
					"y" : 323 + 25 +15 + 40,
					
					
					"tooltip_text" : uiscriptlocale.ENCHANT_REINFORCE_TITLE,

					"default_image" : "new_ui/btn/cat_4.tga",
					"over_image" : "new_ui/btn/cat_4o.tga",
					"down_image" : "new_ui/btn/cat_4d.tga",
				},
				## CategoryButton5
				{
					"name" : "Cat_05",
					"type" : "radio_button",

					"x" : 51,
					"y" : 323 + 25 +15 + 40,
					
					
					"tooltip_text" : "Potions",

					"default_image" : "new_ui/btn/cat_4_razor93.tga",
					"over_image" : "new_ui/btn/cat_4_1razor93.tga",
					"down_image" : "new_ui/btn/cat_4_2razor93.tga",
				},
				## CategoryButton Safebox
				{
					"name" : "Safebox_cat",
					"type" : "button",

					"x" : 92,
					"y" : 323 + 25 +15 + 40,
					
					
					"tooltip_text" : uiscriptlocale.SAFEBOX_TITLE,

					"default_image" : "d:/ymir work/ui/extra_inventory/magazzino_normal.tga",
					"over_image" : "d:/ymir work/ui/extra_inventory/magazzino_hover.tga",
					"down_image" : "d:/ymir work/ui/extra_inventory/magazzino_active.tga",
				},
				
				# CategoryButton Mall
				{
					"name" : "Mall_cat",
					"type" : "button",

					"x" : 133,
					"y" : 323 + 25 +15 + 40,
					
					
					"tooltip_text" : uiscriptlocale.MALL_TITLE,

					"default_image" : "d:/ymir work/ui/extra_inventory/carrello_normal.tga",
					"over_image" : "d:/ymir work/ui/extra_inventory/carrello_hover.tga",
					"down_image" : "d:/ymir work/ui/extra_inventory/carrello_active.tga",
				},
			),	
		},
	),
}
