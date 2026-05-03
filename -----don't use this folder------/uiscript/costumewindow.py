import uiscriptlocale

if ENABLE_WEAPON_COSTUME_SYSTEM:
	window = {
		"name" : "CostumeWindow",

		"x" : SCREEN_WIDTH - 175 - 140,
		"y" : SCREEN_HEIGHT - 37 - 365,

		"style" : ("movable", "float",),

		"width" : 140,
		"height" : (180 + 47), #±âÁ¸º¸´Ù 47 ±æ¾îÁü

		"children" :
		(
			{
				"name" : "board",
				"type" : "board",
				"style" : ("attach",),

				"x" : 0,
				"y" : 0,

				"width" : 140,
				"height" : (180 + 47),
			
				"children" :
				(
					## Title
					{
						"name" : "TitleBar",
						"type" : "titlebar",
						"style" : ("attach",),

						"x" : 6,
						"y" : 6,

						"width" : 130,
						"color" : "yellow",

						"children" :
						(
							{ "name":"TitleName", "type":"text", "x":60, "y":3, "text":uiscriptlocale.COSTUME_WINDOW_TITLE, "text_horizontal_align":"center" },
						),
					},

					## Equipment Slot
					{
						"name" : "Costume_Base",
						"type" : "image",

						"x" : 13,
						"y" : 38,
					
						"image" : "locale/it/ui/costume/new_costume_bg.jpg",					

						"children" :
						(

							{
								"name" : "CostumeSlot",
								"type" : "slot",

								"x" : 3,
								"y" : 3,

								"width" : 127,
								"height" : 145,

								"slot" : (
											{"index":COSTUME_START_INDEX+0, "x":62, "y":45, "width":32, "height":64},#¸ö
											{"index":COSTUME_START_INDEX+1, "x":62, "y": 9, "width":32, "height":32},#¸Ó¸®
											{"index":COSTUME_START_INDEX+2, "x":13, "y":126, "width":32, "height":32},#¸¶¿îÆ®
											{"index":COSTUME_START_INDEX+3, "x":62, "y":126, "width":32, "height":32},#¾Ç¼¼¼­¸®
											{"index":COSTUME_SLOT_WEAPON, "x":13, "y":13, "width":32, "height":96},#¹«±â
										),
							},
							## ENABLE_HIDE_COSTUME_SYSTEM
						{
							"name" : "BodyToolTipButton",
							"type" : "toggle_button",

							"x" : 62 + 32,
							"y" : 45,
							"tooltip_text" : uiscriptlocale.HIDE_COSTUME,

							"default_image" : "d:/ymir work/ui/pattern/visible_mark_01.tga",
							"over_image" : "d:/ymir work/ui/pattern/visible_mark_02.tga",
							"down_image" : "d:/ymir work/ui/pattern/visible_mark_03.tga",
						},
						{
							"name" : "HairToolTipButton",
							"type" : "toggle_button",

							"x" : 62 + 32,
							"y" : 9,
							"tooltip_text" : uiscriptlocale.HIDE_COSTUME,

							"default_image" : "d:/ymir work/ui/pattern/visible_mark_01.tga",
							"over_image" : "d:/ymir work/ui/pattern/visible_mark_02.tga",
							"down_image" : "d:/ymir work/ui/pattern/visible_mark_03.tga",
						},
						{
							"name" : "AcceToolTipButton",
							"type" : "toggle_button",

							"x" : 62 + 32,
							"y" : 126,
							"tooltip_text" : uiscriptlocale.HIDE_COSTUME,

							"default_image" : "d:/ymir work/ui/pattern/visible_mark_01.tga",
							"over_image" : "d:/ymir work/ui/pattern/visible_mark_02.tga",
							"down_image" : "d:/ymir work/ui/pattern/visible_mark_03.tga",
						},
						{
							"name" : "WeaponToolTipButton",
							"type" : "toggle_button",

							"x" : 13 + 32,
							"y" : 13,
							"tooltip_text" : uiscriptlocale.HIDE_COSTUME,

							"default_image" : "d:/ymir work/ui/pattern/visible_mark_01.tga",
							"over_image" : "d:/ymir work/ui/pattern/visible_mark_02.tga",
							"down_image" : "d:/ymir work/ui/pattern/visible_mark_03.tga",
						},
						),
					},
				),
			},
		),
	}
elif ENABLE_MOUNT_COSTUME_SYSTEM:
	if ENABLE_ACCE_COSTUME_SYSTEM:
		window = {
			"name" : "CostumeWindow",

			"x" : SCREEN_WIDTH - 175 - 140,
			"y" : SCREEN_HEIGHT - 37 - 565,

			"style" : ("movable", "float",),

			"width" : 140,
			"height" : (180 + 47), #±âÁ¸º¸´Ù 47 ±æ¾îÁü

			"children" :
			(
				{
					"name" : "board",
					"type" : "board",
					"style" : ("attach",),

					"x" : 0,
					"y" : 0,

					"width" : 140,
					"height" : (180 + 47),
				
					"children" :
					(
						## Title
						{
							"name" : "TitleBar",
							"type" : "titlebar",
							"style" : ("attach",),

							"x" : 6,
							"y" : 6,

							"width" : 130,
							"color" : "yellow",

							"children" :
							(
								{ "name":"TitleName", "type":"text", "x":60, "y":3, "text":uiscriptlocale.COSTUME_WINDOW_TITLE, "text_horizontal_align":"center" },
							),
						},

						## Equipment Slot
						{
							"name" : "Costume_Base",
							"type" : "image",

							"x" : 13,
							"y" : 38,
						
							"image" : "locale/it/ui/costume/new_costume_bg.jpg",					

							"children" :
							(

								{
									"name" : "CostumeSlot",
									"type" : "slot",

									"x" : 3,
									"y" : 3,

									"width" : 127,
									"height" : 145,

									"slot" : (
												{"index":COSTUME_START_INDEX+0, "x":62, "y":45, "width":32, "height":64},#¸ö
												{"index":COSTUME_START_INDEX+1, "x":62, "y": 9, "width":32, "height":32},#¸Ó¸®
												{"index":COSTUME_START_INDEX+2, "x":13, "y":126, "width":32, "height":32},#¸¶¿îÆ®
												{"index":COSTUME_START_INDEX+3, "x":62, "y":126, "width":32, "height":32},#¾Ç¼¼¼­¸®
											),
								},
							),
						},

					),
				},
			),
		}
	else:
		window = {
			"name" : "CostumeWindow",

			"x" : SCREEN_WIDTH - 175 - 140,
			"y" : SCREEN_HEIGHT - 37 - 565,

			"style" : ("movable", "float",),

			"width" : 140,
			"height" : (180 + 47), #±âÁ¸º¸´Ù 47 ±æ¾îÁü

			"children" :
			(
				{
					"name" : "board",
					"type" : "board",
					"style" : ("attach",),

					"x" : 0,
					"y" : 0,

					"width" : 140,
					"height" : (180 + 47),
				
					"children" :
					(
						## Title
						{
							"name" : "TitleBar",
							"type" : "titlebar",
							"style" : ("attach",),

							"x" : 6,
							"y" : 6,

							"width" : 130,
							"color" : "yellow",

							"children" :
							(
								{ "name":"TitleName", "type":"text", "x":60, "y":3, "text":uiscriptlocale.COSTUME_WINDOW_TITLE, "text_horizontal_align":"center" },
							),
						},

						## Equipment Slot
						{
							"name" : "Costume_Base",
							"type" : "image",

							"x" : 13,
							"y" : 38,
						
							"image" : "locale/it/ui/costume/new_costume_bg.jpg",					

							"children" :
							(

								{
									"name" : "CostumeSlot",
									"type" : "slot",

									"x" : 3,
									"y" : 3,

									"width" : 127,
									"height" : 145,

									"slot" : (
												{"index":COSTUME_START_INDEX+0, "x":62, "y":45, "width":32, "height":64},#¸ö
												{"index":COSTUME_START_INDEX+1, "x":62, "y": 9, "width":32, "height":32},#¸Ó¸®
												{"index":COSTUME_START_INDEX+2, "x":13, "y":125, "width":32, "height":32},#¸¶¿îÆ®
											),
								},
							),
						},

					),
				},
			),
		}
else:
	window = {
		"name" : "CostumeWindow",

		"x" : SCREEN_WIDTH - 175 - 140,
		"y" : SCREEN_HEIGHT - 37 - 565,

		"style" : ("movable", "float",),

		"width" : 140,
		"height" : 180,

		"children" :
		(
			{
				"name" : "board",
				"type" : "board",
				"style" : ("attach",),

				"x" : 0,
				"y" : 0,

				"width" : 140,
				"height" : 180,
				
				"children" :
				(
					## Title
					{
						"name" : "TitleBar",
						"type" : "titlebar",
						"style" : ("attach",),

						"x" : 6,
						"y" : 6,

						"width" : 130,
						"color" : "yellow",

						"children" :
						(
							{ "name":"TitleName", "type":"text", "x":60, "y":3, "text":uiscriptlocale.COSTUME_WINDOW_TITLE, "text_horizontal_align":"center" },
						),
					},

					## Equipment Slot
					{
						"name" : "Costume_Base",
						"type" : "image",

						"x" : 13,
						"y" : 38,
						
						"image" : "locale/it/ui/costume/costume_bg.jpg",					

						"children" :
						(

							{
								"name" : "CostumeSlot",
								"type" : "slot",

								"x" : 3,
								"y" : 3,

								"width" : 127,
								"height" : 145,

								"slot" : (
											{"index":COSTUME_START_INDEX+0, "x":61, "y":45, "width":32, "height":64},
											{"index":COSTUME_START_INDEX+1, "x":61, "y": 8, "width":32, "height":32},
											{"index":COSTUME_START_INDEX+2, "x":5, "y":145, "width":32, "height":32},
										),
							},
						),
					},

				),
			},
		),
	}
