import uiscriptlocale
import item
uiscriptlocale = uiscriptlocale
# --- sidebar patch (MINIMAL, name-ekhez nem nyulunk) ---
MAIN_BOARD_WIDTH = 176
LEFT_PANEL_WIDTH = 34

BOARD_WIDTH = MAIN_BOARD_WIDTH + LEFT_PANEL_WIDTH
BOARD_HEIGHT = 565-2

window = {
	"name" : "InventoryWindow",

	## 600 - (width + ���������� ���� ���� 24 px)
	"x" : SCREEN_WIDTH - BOARD_WIDTH,
	"y" : SCREEN_HEIGHT - BOARD_HEIGHT - 59,
	#"style" : ("movable", "float","not_pick",),
	"style" : ("movable", "float",),
	"width" : BOARD_WIDTH,
	"height" : BOARD_HEIGHT,
	"children" :
	(
		# --- Left sidebar panel (gombok) ---		## Inventory, Equipment Slots
		{
			"name" : "board",
			"type" : "board",
			#"style" : ("attach",),

			# --- sidebar patch: a board jobbra tolodik, hogy balra legyen hely ---
			"x" : 0,
			"y" : 0,

			"width" : BOARD_WIDTH,
			"height" : BOARD_HEIGHT,

			"children" :
			(
				{
					"name" : "Sidebar",
					"type" : "window",
					"x" : 0,
					"y" : 0,
					"width" : LEFT_PANEL_WIDTH,
					"height" : BOARD_HEIGHT,
				},
				{
					"name" : "Content",
					"type" : "window",
					"x" : LEFT_PANEL_WIDTH,
					"y" : 0,
					"width" : MAIN_BOARD_WIDTH,
					"height" : BOARD_HEIGHT,
					"children" :
					(

					## Title
					{
						"name" : "TitleBar",
						"type" : "titlebar",
						"style" : ("attach",),

						#"x" : 8+38,## 38 is the width of the new sort button -> Move the titlebard to the right
						"x" : - 28,
						"y" : 7,

						#"width" : 161-38,## 38 is the width of the new sort button -> Decrease the width of the titlebar
						"width" : 200,
						"color" : "yellow",

						"children" :
						(
							{ "name":"TitleName", "type":"text", "x":0, "y":3, "text":uiscriptlocale.INVENTORY_TITLE, "text_horizontal_align":"center", "horizontal_align":"center" },
						),
					},

					# Separate
					{
						"name" : "SeparateBaseImage",
						"type" : "image",
						"style" : ("attach",),
					
						"x" : 121,
						"y" : 7,
					
						"image" : "d:/ymir work/ui/pattern/titlebar_inv_refresh_baseframe.tga",
					
						"children" :
						(
							# Separate Button (38x24) SORT_IVNENTORY
							{
								"name" : "SeparateButton",
								"type" : "button",
					
								"x" : 11,
								"y" : 3,
					
								"tooltip_text" : uiscriptlocale.INVENTORY_SEPARATE,
					
								"default_image" : "d:/ymir work/ui/game/inventory/refresh_small_button_01.sub",
								"over_image" : "d:/ymir work/ui/game/inventory/refresh_small_button_02.sub",
								"down_image" : "d:/ymir work/ui/game/inventory/refresh_small_button_03.sub",
								"disable_image" : "d:/ymir work/ui/game/inventory/refresh_small_button_04.sub",
							},
						),
					},

					## Equipment Slot
					{
						"name" : "Equipment_Base",
						"type" : "image",

						"x" : 8,
						"y" : 33,

						"image" : "d:/ymir work/ui/equipment_bg_with_talisman.tga",

						"children" :
						(

							{
								"name" : "EquipmentSlot",
								"type" : "slot",

								"x" : 3,
								"y" : 3,

								"width" : 150,
								"height" : 182,

								"slot" : (
											{"index":EQUIPMENT_START_INDEX+0, "x":39, "y":37, "width":32, "height":64},
											{"index":EQUIPMENT_START_INDEX+1, "x":39, "y":2, "width":32, "height":32},
											{"index":EQUIPMENT_START_INDEX+2, "x":39, "y":145, "width":32, "height":32},
											{"index":EQUIPMENT_START_INDEX+3, "x":75, "y":67, "width":32, "height":32},
											{"index":EQUIPMENT_START_INDEX+4, "x":3, "y":3, "width":32, "height":96},
											{"index":EQUIPMENT_START_INDEX+5, "x":114, "y":67, "width":32, "height":32},
											{"index":EQUIPMENT_START_INDEX+6, "x":114, "y":35, "width":32, "height":32},
											{"index":EQUIPMENT_START_INDEX+7, "x":2, "y":145, "width":32, "height":32},
											{"index":EQUIPMENT_START_INDEX+8, "x":75, "y":145, "width":32, "height":32},
											{"index":EQUIPMENT_START_INDEX+9, "x":114, "y":2, "width":32, "height":32},
											{"index":EQUIPMENT_START_INDEX+10, "x":75, "y":35, "width":32, "height":32},
											{"index":EQUIPMENT_START_INDEX+18, "x":75, "y":2, "width":32, "height":32},
											#Talisman Slot
											{"index":EQUIPMENT_START_INDEX+25, "x":3, "y":106, "width":32, "height":32},
											## �� ����1
											##{"index":item.EQUIPMENT_RING1, "x":2, "y":106, "width":32, "height":32},
											## �� ����2
											##{"index":item.EQUIPMENT_RING2, "x":75, "y":106, "width":32, "height":32},
											## �� ��Ʈ
											{"index":EQUIPMENT_BELT, "x":115, "y":143, "width":32, "height":32},
										),
							"children" :
							(
								##new button box
								{
									"name" : "newButtonsBox",
									"type" : "image",

									"x" : 112,
									"y" : 105,

									"image" : "d:/ymir work/ui/rune/none.tga",

									"children" :
									(
										##Dragon Soul Button
										{
											"name" : "DSSButton",
											"type" : "button",

											"x" : 2,
											"y" : 2,

											"default_image" : "d:/ymir work/ui/rune/none.tga",
											"over_image" : "d:/ymir work/ui/rune/none.tga",
											"down_image" : "d:/ymir work/ui/rune/none.tga",
										},
										##Mall Button
										{
											"name" : "ExtendIntentory",
											"type" : "button",

											"x" : 3 + 15,
											"y" : 2,

											"default_image" : "d:/ymir work/ui/rune/none.tga",
											"over_image" : "d:/ymir work/ui/rune/none.tga",
											"down_image" : "d:/ymir work/ui/rune/none.tga",
										},
										##Offline Shop
										{
											"name" : "OfflineShop",
											"type" : "button",

											"x" : 2,
											"y" : 2+16,

											"default_image" : "d:/ymir work/ui/rune/none.tga",
											"over_image" : "d:/ymir work/ui/rune/none.tga",
											"down_image" : "d:/ymir work/ui/rune/none.tga",
										},
										##Switchbot Systemm

									)
								},
								##new button box
								{
									"name" : "newButtonsBox",
									"type" : "image",

									"x" : 112,
									"y" : 137,

									"image" : "d:/ymir work/ui/rune/none.tga",

									"children" :
									(
										##BattlePass
										{
											"name" : "BattlepassButton",
											"type" : "button",

											"x" : 2,
											"y" : 2,
											"tooltip_text" : "Battle Pass",
											"default_image" : "d:/ymir work/ui/game/taskbar/Bp1.png",
											"over_image" : "d:/ymir work/ui/game/taskbar/Bp2.png",
											"down_image" : "d:/ymir work/ui/game/taskbar/Bp3.png",
										},
										##Ranking
										{
											"name" : "RankingButton",
											"type" : "button",

											"x" : 3 + 15,
											"y" : 2,

											"default_image" : "d:/ymir work/ui/rune/none.tga",
											"over_image" : "d:/ymir work/ui/rune/none.tga",
											"down_image" : "d:/ymir work/ui/rune/none.tga",
										},
									)
								},
							),
							},
							{
								"name" : "CostumeSlot",
								"type" : "slot",

								"x" : 3,
								"y" : 3,

								"width" : 150,
								"height" : 182,

								"slot" : (
											{"index":COSTUME_START_INDEX+0, "x":78, "y":37, "width":32, "height":64},
											{"index":COSTUME_START_INDEX+1, "x":78, "y": 5, "width":32, "height":32},
											{"index":COSTUME_START_INDEX+2, "x":21, "y":109, "width":32, "height":32},
											{"index":COSTUME_START_INDEX+3, "x":58, "y":124, "width":32, "height":32},
											{"index":COSTUME_SLOT_WEAPON, "x":40, "y":5, "width":32, "height":96},
											{"index":COSTUME_PETSKIN_SLOT, "x":95, "y":145, "width":32, "height":32},
											{"index":COSTUME_MOUNTSKIN_SLOT, "x":21, "y":145, "width":32, "height":32},
											{"index":COSTUME_EFFECT_BODY_SLOT, "x":5, "y":34, "width":32, "height":32},
											{"index":COSTUME_EFFECT_WEAPON_SLOT, "x":5, "y":2, "width":32, "height":32},
										),
							},
							{
								"name" : "BodyToolTipButton",
								"type" : "toggle_button",

								"x" : 100,
								"y" : 40,
								"tooltip_text" : uiscriptlocale.HIDE_COSTUME,
								"tooltip_x" : 0,
								"tooltip_y" : - 14,
								"default_image" : "d:/ymir work/ui/pattern/visible_mark_01.tga",
								"over_image" : "d:/ymir work/ui/pattern/visible_mark_02.tga",
								"down_image" : "d:/ymir work/ui/pattern/visible_mark_03.tga",
							},
							{
								"name" : "HairToolTipButton",
								"type" : "toggle_button",
								"x" : 105,
								"y" : 0,
								"tooltip_text" : uiscriptlocale.HIDE_COSTUME,
								"tooltip_x" : - 52,
								"tooltip_y" : - 1,
								"default_image" : "d:/ymir work/ui/pattern/visible_mark_01.tga",
								"over_image" : "d:/ymir work/ui/pattern/visible_mark_02.tga",
								"down_image" : "d:/ymir work/ui/pattern/visible_mark_03.tga",
							},
							{
								"name" : "AcceToolTipButton",
								"type" : "toggle_button",
								"x" : 88,
								"y" : 127,
								"tooltip_text" : uiscriptlocale.HIDE_COSTUME,
								"tooltip_x" : - 55,
								"tooltip_y" : - 10,
								"default_image" : "d:/ymir work/ui/pattern/visible_mark_01.tga",
								"over_image" : "d:/ymir work/ui/pattern/visible_mark_02.tga",
								"down_image" : "d:/ymir work/ui/pattern/visible_mark_03.tga",
							},
							{
								"name" : "WeaponToolTipButton",
								"type" : "toggle_button",
								"x" : 65,
								"y" : 0,
								"tooltip_text" : uiscriptlocale.HIDE_COSTUME,
								"tooltip_x" : - 8,
								"tooltip_y" : - 25,
								"default_image" : "d:/ymir work/ui/pattern/visible_mark_01.tga",
								"over_image" : "d:/ymir work/ui/pattern/visible_mark_02.tga",
								"down_image" : "d:/ymir work/ui/pattern/visible_mark_03.tga",
							},
							## Dragon Soul Button
							# {
								# "name" : "DSSButton",
								# "type" : "button",

								# "x" : 114,
								# "y" : 107,

								# "tooltip_text" : uiscriptlocale.TASKBAR_DRAGON_SOUL,
								# "tooltip_x" : - 56,
								# "tooltip_y" : - 12,
								# "default_image" : "d:/ymir work/ui/dragonsoul/dss_inventory_button_01.tga",
								# "over_image" : "d:/ymir work/ui/dragonsoul/dss_inventory_button_02.tga",
								# "down_image" : "d:/ymir work/ui/dragonsoul/dss_inventory_button_03.tga",
							# },
							# if app.ENABLE_EXTRA_INVENTORY:
							# {
								# "name" : "ExtendIntentory",
								# "type" : "button",

								# "x" : 114,
								# "y" : 148,

								# "tooltip_text" : uiscriptlocale.EXTRA_INVENTORY,
								# "tooltip_x" : - 33,
								# "tooltip_y" : - 12,
								# "default_image" : "d:/ymir work/ui/extra_inventory/extra_inv_01.tga",
								# "over_image" : "d:/ymir work/ui/extra_inventory/extra_inv_02.tga",
								# "down_image" : "d:/ymir work/ui/extra_inventory/extra_inv_03.tga",
							# },
							## Rune:
							{
								"name" : "Rune",
								"type" : "button",

								"x" : - 32,
								"y" : 407,

								"tooltip_text" : "Susanoo",
								"tooltip_x" : 6,
								"tooltip_y" : - 12,
								"default_image" : "d:/ymir work/ui/game/taskbar/Susanoo1.png",
								"over_image" : "d:/ymir work/ui/game/taskbar/Susanoo2.png",
								"down_image" : "d:/ymir work/ui/game/taskbar/Susanoo3.png"
							},
							##Mall Button
							{
								"name" : "ExtendIntentory",
								"type" : "button",

								"x" : - 32,
								"y" : 440,

								"tooltip_text" : "Special Inventory",
								"tooltip_x" : 6,
								"tooltip_y" : 18,
								"default_image" : "d:/ymir work/ui/game/taskbar/SpecVault1.png",
								"over_image" : "d:/ymir work/ui/game/taskbar/SpecVault2.png",
								"down_image" : "d:/ymir work/ui/game/taskbar/SpecVault3.png",
							},
							##Ranking d:/ymir work/ui/rune/none.tga dragonsoul_button_01.tga
							{
								"name" : "DSSButton",
								"type" : "button",

								"x" : 177,
								"y" : 123,#magas

								"tooltip_text" : "Dragon Soul",
								"tooltip_x" : 6,
								"tooltip_y" : 18,
								"default_image" : "d:/ymir work/ui/rune/none.tga",
								"over_image" : "d:/ymir work/ui/rune/none.tga",
								"down_image" : "d:/ymir work/ui/rune/none.tga",
							},
							 {
								"name" : "AnimalButton",
								"type" : "button",

								"x" : 127,
								"y" : 100,
								#"tooltip_text" : "Mount Inventory",
								"default_image" : "d:/ymir work/ui/game/TaskBar/Mount1.png",
								"over_image" : "d:/ymir work/ui/game/TaskBar/Mount2.png",
								"down_image" : "d:/ymir work/ui/game/TaskBar/Mount3.png",

							},

							{
								"name" : "RankingButton",
								"type" : "button",

								"x" : 77,
								"y" : 123,

								"tooltip_text" : "Ranking",
								"tooltip_x" : 6,
								"tooltip_y" : 18,
								"default_image" : "d:/ymir work/ui/game/taskbar/Top1.png",
								"over_image" : "d:/ymir work/ui/game/taskbar/Top2.png",
								"down_image" : "d:/ymir work/ui/game/taskbar/Top3.png",
							},
							{
								"name" : "SwitchbotButton",
								"type" : "button",
								"x" : 123,
								"y" : 100,

								"tooltip_text" : "Switchbot",
								"tooltip_x" : 6,
								"tooltip_y" : - 12,
								"default_image" : "d:/ymir work/ui/game/taskbar/Opt1.png",
								"over_image" : "d:/ymir work/ui/game/taskbar/Opt2.png",
								"down_image" : "d:/ymir work/ui/game/taskbar/Opt3.png",
							},
							{
								"name" : "OfflineShop",
								"type" : "button",

								"x" : 77,
								"y" : 100,

								"tooltip_text" : "Offline Shop",
								"tooltip_x" : 6,
								"tooltip_y" : - 12,
								"default_image" : "d:/ymir work/ui/game/taskbar/OffShop1.png",
								"over_image" : "d:/ymir work/ui/game/taskbar/OffShop2.png",
								"down_image" : "d:/ymir work/ui/game/taskbar/OffShop3.png",
							},
						),
					},
					{
						"name" : "Equipment_Tab_01",
						"type" : "radio_button",
						"x" : 11,
						"y" : 33 + 192,
						"default_image" : "d:/ymir work/ui/game/windows/bottone/prova.tga",
						"over_image" : "d:/ymir work/ui/game/windows/bottone/prova1.tga",
						"down_image" : "d:/ymir work/ui/game/windows/bottone/prova2.tga",
						"tooltip_text" : uiscriptlocale.EQUIPMENT_PAGE_BUTTON_TOOLTIP_1,
						"children" :
						(
							{
								"name" : "Equipment_Tab_01_Print",
								"type" : "text",
								"x" : 0,
								"y" : 0,
								"all_align" : "center",
								# "text" : "I",
							},
						),
					},
					{
						"name" : "Equipment_Tab_02",
						"type" : "radio_button",
						"x" : 88,
						"y" : 33 + 192,
						"default_image" : "d:/ymir work/ui/game/windows/bottone/prova3.tga",
						"over_image" : "d:/ymir work/ui/game/windows/bottone/prova4.tga",
						"down_image" : "d:/ymir work/ui/game/windows/bottone/prova5.tga",
						"tooltip_text" : uiscriptlocale.EQUIPMENT_PAGE_BUTTON_TOOLTIP_2,
						"children" :
						(
							{
								"name" : "Equipment_Tab_02_Print",
								"type" : "text",
								"x" : 0,
								"y" : 0,
								"all_align" : "center",
								# "text" : "II",
							},
						),
					},
					{
						"name" : "Inventory_Tab_01",
						"type" : "radio_button",

						"x" : 8,
						"y" : 53 + 191,

						"default_image" : "d:/ymir work/razor93/inventory1.dds",
						"over_image" : "d:/ymir work/razor93/inventory2.dds",
						"down_image" : "d:/ymir work/razor93/inventory3.dds",
						"tooltip_text" : uiscriptlocale.INVENTORY_PAGE_BUTTON_TOOLTIP_1,

						"children" :
						(
							{
								"name" : "Inventory_Tab_01_Print",
								"type" : "text",

								"x" : 0,
								"y" : 0,

								"all_align" : "center",

								"text" : "1",
							},
						),
					},
					{
						"name" : "Inventory_Tab_02",
						"type" : "radio_button",

						#"x" : 10 + 78,
						"x" : 8 + 20,
						"y" : 53 + 191,

						"default_image" : "d:/ymir work/razor93/inventory1.dds",
						"over_image" : "d:/ymir work/razor93/inventory2.dds",
						"down_image" : "d:/ymir work/razor93/inventory3.dds",
						"tooltip_text" : uiscriptlocale.INVENTORY_PAGE_BUTTON_TOOLTIP_2,

						"children" :
						(
							{
								"name" : "Inventory_Tab_02_Print",
								"type" : "text",

								"x" : 0,
								"y" : 0,

								"all_align" : "center",

								"text" : "2",
							},
						),
					},

					{
						"name" : "Inventory_Tab_03",
						"type" : "radio_button",

						"x" : 8 + 20 + 20,
						"y" : 53 + 191,

						"default_image" : "d:/ymir work/razor93/inventory1.dds",
						"over_image" : "d:/ymir work/razor93/inventory2.dds",
						"down_image" : "d:/ymir work/razor93/inventory3.dds",
						"tooltip_text" : uiscriptlocale.INVENTORY_PAGE_BUTTON_TOOLTIP_3,

						"children" :
						(
							{
								"name" : "Inventory_Tab_03_Print",
								"type" : "text",

								"x" : 0,
								"y" : 0,

								"all_align" : "center",

								"text" : "3",
							},
						),
					},

					{
						"name" : "Inventory_Tab_04",
						"type" : "radio_button",

						"x" : 8 + 20 + 20 + 20,
						"y" : 53 + 191,

						"default_image" : "d:/ymir work/razor93/inventory1.dds",
						"over_image" : "d:/ymir work/razor93/inventory2.dds",
						"down_image" : "d:/ymir work/razor93/inventory3.dds",
						"tooltip_text" : uiscriptlocale.INVENTORY_PAGE_BUTTON_TOOLTIP_4,

						"children" :
						(
							{
								"name" : "Inventory_Tab_04_Print",
								"type" : "text",

								"x" : 0,
								"y" : 0,

								"all_align" : "center",

								"text" : "4",
							},
						),
					},

					{
						"name" : "Inventory_Tab_05",
						"type" : "radio_button",

						"x" : 8 + 20 + 20 + 20 + 20,
						"y" : 53 + 191,

						"default_image" : "d:/ymir work/razor93/inventory1.dds",
						"over_image" : "d:/ymir work/razor93/inventory2.dds",
						"down_image" : "d:/ymir work/razor93/inventory3.dds",

						"children" :
						(
							{
								"name" : "Inventory_Tab_05_Print",
								"type" : "text",
								"x" : 0,
								"y" : 0,
								"all_align" : "center",
								"text" : "5",
							},
						),
					},

					{
						"name" : "Inventory_Tab_06",
						"type" : "radio_button",

						"x" : 8 + 20 + 20 + 20 + 20 + 20,
						"y" : 53 + 191,

						"default_image" : "d:/ymir work/razor93/inventory1.dds",
						"over_image" : "d:/ymir work/razor93/inventory2.dds",
						"down_image" : "d:/ymir work/razor93/inventory3.dds",

						"children" :
						(
							{
								"name" : "Inventory_Tab_06_Print",
								"type" : "text",
								"x" : 0,
								"y" : 0,
								"all_align" : "center",
								"text" : "6",
							},
						),
					},

					{
						"name" : "Inventory_Tab_07",
						"type" : "radio_button",

						"x" : 8 + 20 + 20 + 20 + 20 + 20 + 20,
						"y" : 53 + 191,

						"default_image" : "d:/ymir work/razor93/inventory1.dds",
						"over_image" : "d:/ymir work/razor93/inventory2.dds",
						"down_image" : "d:/ymir work/razor93/inventory3.dds",

						"children" :
						(
							{
								"name" : "Inventory_Tab_07_Print",
								"type" : "text",
								"x" : 0,
								"y" : 0,
								"all_align" : "center",
								"text" : "7",
							},
						),
					},

					{
						"name" : "Inventory_Tab_08",
						"type" : "radio_button",

						"x" : 8 + 20 + 20 + 20 + 20 + 20 + 20 + 20,
						"y" : 53 + 191,

						"default_image" : "d:/ymir work/razor93/inventory1.dds",
						"over_image" : "d:/ymir work/razor93/inventory2.dds",
						"down_image" : "d:/ymir work/razor93/inventory3.dds",

						"children" :
						(
							{
								"name" : "Inventory_Tab_08_Print",
								"type" : "text",
								"x" : 0,
								"y" : 0,
								"all_align" : "center",
								"text" : "8",
							},
						),
					},

					## Item Slot
					{
						"name" : "ItemSlot",
						"type" : "grid_table",

						"x" : 8,
						"y" : 266,

						"start_index" : 0,
						"x_count" : 5,
						"y_count" : 9,
						"x_step" : 32,
						"y_step" : 32,

						"image" : "d:/ymir work/ui/public/Slot_Base.sub"
					},
					{
						"name":"cover_open_0",
						"type":"button",
						"vertical_align":"bottom",

						"x":8,
						"y":339 - 24 -18,

						"default_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
						"over_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
						"down_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
					},
					{
						"name":"cover_close_0",
						"type":"image",
						"vertical_align":"bottom",

						"x":8,
						"y":339 - 24 -18,

						"image":"d:/ymir work/ui/ex_inven_cover_button_close.sub",
					},
					{
						"name":"cover_open_1",
						"type":"button",
						"vertical_align":"bottom",

						"x":8,
						"y":339 - 24 - 32 -18,

						"default_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
						"over_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
						"down_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
					},
					{
						"name":"cover_close_1",
						"type":"image",
						"vertical_align":"bottom",

						"x":8,
						"y":339 - 24 - 32 -18,

						"image":"d:/ymir work/ui/ex_inven_cover_button_close.sub",
					},
					{
						"name":"cover_open_2",
						"type":"button",
						"vertical_align":"bottom",

						"x":8,
						"y":339 - 24 - 64 -18,
						"default_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
						"over_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
						"down_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
					},
					{
						"name":"cover_close_2",
						"type":"image",
						"vertical_align":"bottom",

						"x":8,
						"y":339 - 24 - 64 -18,

						"image":"d:/ymir work/ui/ex_inven_cover_button_close.sub",
					},
					{
						"name":"cover_open_3",
						"type":"button",
						"vertical_align":"bottom",

						"x":8,
						"y":339 - 24 - 96 -18,

						"default_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
						"over_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
						"down_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
					},
					{
						"name":"cover_close_3",
						"type":"image",
						"vertical_align":"bottom",

						"x":8,
						"y":339 - 24 - 96 -18,

						"image":"d:/ymir work/ui/ex_inven_cover_button_close.sub",
					},
					{
						"name":"cover_open_4",
						"type":"button",
						"vertical_align":"bottom",

						"x":8,
						"y":339 - 24 - 128 -18,

						"default_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
						"over_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
						"down_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
					},
					{
						"name":"cover_close_4",
						"type":"image",
						"vertical_align":"bottom",

						"x":8,
						"y":339 - 24 - 128 -18,

						"image":"d:/ymir work/ui/ex_inven_cover_button_close.sub",
					},
					{
						"name":"cover_open_5",
						"type":"button",
						"vertical_align":"bottom",

						"x":8,
						"y":339 - 24 - 160 -18,

						"default_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
						"over_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
						"down_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
					},
					{
						"name":"cover_close_5",
						"type":"image",
						"vertical_align":"bottom",

						"x":8,
						"y":339 - 24 - 160 -18,

						"image":"d:/ymir work/ui/ex_inven_cover_button_close.sub",
					},
					{
						"name":"cover_open_6",
						"type":"button",
						"vertical_align":"bottom",

						"x":8,
						"y":339 - 24 - 192 -18,

						"default_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
						"over_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
						"down_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
					},
					{
						"name":"cover_close_6",
						"type":"image",
						"vertical_align":"bottom",

						"x":8,
						"y":339 - 24 - 192 -18,

						"image":"d:/ymir work/ui/ex_inven_cover_button_close.sub",
					},
					{
						"name":"cover_open_7",
						"type":"button",
						"vertical_align":"bottom",

						"x":8,
						"y":339 - 24 - 224 -18,

						"default_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
						"over_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
						"down_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
					},
					{
						"name":"cover_close_7",
						"type":"image",
						"vertical_align":"bottom",

						"x":8,
						"y":339 - 24 - 224 -18,

						"image":"d:/ymir work/ui/ex_inven_cover_button_close.sub",
					},
					{
						"name":"cover_open_8",
						"type":"button",
						"vertical_align":"bottom",

						"x":8,
						"y":339 - 24 - 256 -18,

						"default_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
						"over_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
						"down_image":"d:/ymir work/ui/ex_inven_cover_button_open.sub",
					},
					{
						"name":"cover_close_8",
						"type":"image",
						"vertical_align":"bottom",

						"x":8,
						"y":339 - 24 - 256 -18,

						"image":"d:/ymir work/ui/ex_inven_cover_button_close.sub",
					},

					),
				},
			),
		},
	),
}
