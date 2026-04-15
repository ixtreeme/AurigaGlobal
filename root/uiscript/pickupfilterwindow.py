#blackdragonx61 (18.11.21)
import uiscriptlocale

PATTERN_PATH			= "d:/ymir work/ui/pattern/"
ROOT_PATH				= "d:/ymir work/ui/public/"
CHATTING_PATH			= "d:/ymir work/ui/chat/"

WINDOW_WIDTH			= 254
WINDOW_HEIGHT			= 640
PATTERN_WINDOW_WIDTH	= 232
PATTERN_WINDOW_HEIGHT	= 594

PATTERN_X_COUNT = (PATTERN_WINDOW_WIDTH - 32) / 16
PATTERN_Y_COUNT = (PATTERN_WINDOW_HEIGHT - 32) / 16

MIDDLE_BUTTON_WIDTH 	= 65
MIDDLE_BUTTON_HEIGHT 	= 25

window = {
	"name" : "ChatSettingWindow",
	"style" : ("movable", "float",),
	"x" : SCREEN_WIDTH / 2 - WINDOW_WIDTH / 2,
	"y" : SCREEN_HEIGHT / 2 - WINDOW_HEIGHT / 2,
	"width" : WINDOW_WIDTH,
	"height" : WINDOW_HEIGHT,
	"children" :
	(
		{
			"name" : "board",
			"type" : "board_with_titlebar",
			"style" : ("attach",),
			"x" : 0,
			"y" : 0,
			"width" : WINDOW_WIDTH,
			"height" : WINDOW_HEIGHT,
			"title" : uiscriptlocale.PICK_UP_TITLE,
			"children" :
			(
				## base pattern
				{
					"name" : "base_pattern",
					"type" : "window",
					"style" : ("attach", "ltr",),
					"x" : 10,
					"y" : 32,
					"width" : PATTERN_WINDOW_WIDTH,
					"height" :PATTERN_WINDOW_HEIGHT,
					"children" :
					(
						## LeftTop 1
						{
							"name" : "pattern_left_top_img",
							"type" : "image",
							"style" : ("ltr",),
							"x" : 0,
							"y" : 0,
							"image" : PATTERN_PATH + "border_A_left_top.tga",
						},
						## RightTop 2
						{
							"name" : "pattern_right_top_img",
							"type" : "image",
							"style" : ("ltr",),
							"x" : PATTERN_WINDOW_WIDTH - 16,
							"y" : 0,
							"image" : PATTERN_PATH + "border_A_right_top.tga",
						},
						## LeftBottom 3
						{
							"name" : "pattern_left_bottom_img",
							"type" : "image",
							"style" : ("ltr",),
							"x" : 0,
							"y" : PATTERN_WINDOW_HEIGHT - 16,
							"image" : PATTERN_PATH + "border_A_left_bottom.tga",
						},
						## RightBottom 4
						{
							"name" : "pattern_right_bottom_img",
							"type" : "image",
							"style" : ("ltr",),
							"x" : PATTERN_WINDOW_WIDTH - 16,
							"y" : PATTERN_WINDOW_HEIGHT - 16,
							"image" : PATTERN_PATH + "border_A_right_bottom.tga",
						},
						## topcenterImg 5
						{
							"name" : "pattern_top_cetner_img",
							"type" : "expanded_image",
							"style" : ("ltr",),
							"x" : 16,
							"y" : 0,
							"image" : PATTERN_PATH + "border_A_top.tga",
							"rect" : (0.0, 0.0, PATTERN_X_COUNT, 0),
						},
						## leftcenterImg 6
						{
							"name" : "pattern_left_center_img",
							"type" : "expanded_image",
							"style" : ("ltr",),
							"x" : 0,
							"y" : 16,
							"image" : PATTERN_PATH + "border_A_left.tga",
							"rect" : (0.0, 0.0, 0, PATTERN_Y_COUNT),
						},
						## rightcenterImg 7
						{
							"name" : "pattern_right_center_img",
							"type" : "expanded_image",
							"style" : ("ltr",),
							"x" : PATTERN_WINDOW_WIDTH - 16,
							"y" : 16,
							"image" : PATTERN_PATH + "border_A_right.tga",
							"rect" : (0.0, 0.0, 0, PATTERN_Y_COUNT),
						},
						## bottomcenterImg 8
						{
							"name" : "pattern_bottom_center_img",
							"type" : "expanded_image",
							"style" : ("ltr",),
							"x" : 16,
							"y" : PATTERN_WINDOW_HEIGHT - 16,
							"image" : PATTERN_PATH + "border_A_bottom.tga",
							"rect" : (0.0, 0.0, PATTERN_X_COUNT, 0),
						},
						## centerImg
						{
							"name" : "pattern_center_img",
							"type" : "expanded_image",
							"style" : ("ltr",),
							"x" : 16,
							"y" : 16,
							"image" : PATTERN_PATH + "border_A_center.tga",
							"rect" : (0.0, 0.0, PATTERN_X_COUNT, PATTERN_Y_COUNT),
						},
					),
				},
				#WEAPON
				{
					"name" : "weapon_setting_menu_bg", "type" : "image", "x" : 15, "y" : 37,
					"image" : CHATTING_PATH + "chattingoption_menu_bg.sub",
					"children" :
					(
						{ "name" : "weapon_setting", "type" : "text", "x" : 0, "y" : 0, "text" : uiscriptlocale.PICK_UP_TITLE_WEAPON, "all_align":"center"	},
					),
				},
				{
					"name" : "line_weapon_top",
					"type" : "line",

					"x" : 13,
					"y" : 37 + 27,

					"width" : 226,
					"height" : 0,

					"color" : 0xff777777,
				},
				{
					"name" : "line_weapon_bottom",
					"type" : "line",

					"x" : 13,
					"y" : 37 + 27 + 80,

					"width" : 226,
					"height" : 0,

					"color" : 0xff777777,
				},
				{
					"name" : "filter_0",
					"type" : "toggle_button",

					"x" : 30,
					"y" : 70,

					"text" : uiscriptlocale.PICK_UP_WEAPON_SWORD,

					"default_image" : ROOT_PATH + "Middle_Button_01.sub",
					"over_image" : ROOT_PATH + "Middle_Button_02.sub",
					"down_image" : ROOT_PATH + "Middle_Button_03.sub",
				},
				{
					"name" : "filter_1",
					"type" : "toggle_button",

					"x" : 30 + MIDDLE_BUTTON_WIDTH,
					"y" : 70,

					"text" : uiscriptlocale.PICK_UP_WEAPON_DAGGER,

					"default_image" : ROOT_PATH + "Middle_Button_01.sub",
					"over_image" : ROOT_PATH + "Middle_Button_02.sub",
					"down_image" : ROOT_PATH + "Middle_Button_03.sub",
				},
				{
					"name" : "filter_2",
					"type" : "toggle_button",

					"x" : 30 + MIDDLE_BUTTON_WIDTH * 2,
					"y" : 70,

					"text" : uiscriptlocale.PICK_UP_WEAPON_BOW,

					"default_image" : ROOT_PATH + "Middle_Button_01.sub",
					"over_image" : ROOT_PATH + "Middle_Button_02.sub",
					"down_image" : ROOT_PATH + "Middle_Button_03.sub",
				},
				{
					"name" : "filter_3",
					"type" : "toggle_button",

					"x" : 30,
					"y" : 70 + MIDDLE_BUTTON_HEIGHT,

					"text" : uiscriptlocale.PICK_UP_WEAPON_TWO_HAND,

					"default_image" : ROOT_PATH + "Middle_Button_01.sub",
					"over_image" : ROOT_PATH + "Middle_Button_02.sub",
					"down_image" : ROOT_PATH + "Middle_Button_03.sub",
				},
				{
					"name" : "filter_4",
					"type" : "toggle_button",

					"x" : 30 + MIDDLE_BUTTON_WIDTH,
					"y" : 70 + MIDDLE_BUTTON_HEIGHT,

					"text" : uiscriptlocale.PICK_UP_WEAPON_BELL,

					"default_image" : ROOT_PATH + "Middle_Button_01.sub",
					"over_image" : ROOT_PATH + "Middle_Button_02.sub",
					"down_image" : ROOT_PATH + "Middle_Button_03.sub",
				},
				{
					"name" : "filter_5",
					"type" : "toggle_button",

					"x" : 30 + MIDDLE_BUTTON_WIDTH * 2,
					"y" : 70 + MIDDLE_BUTTON_HEIGHT,

					"text" : uiscriptlocale.PICK_UP_WEAPON_FAN,

					"default_image" : ROOT_PATH + "Middle_Button_01.sub",
					"over_image" : ROOT_PATH + "Middle_Button_02.sub",
					"down_image" : ROOT_PATH + "Middle_Button_03.sub",
				},
				
				{
					"name" : "filter_6",
					"type" : "toggle_button",

					"x" : 30,
					"y" : 70 + MIDDLE_BUTTON_HEIGHT * 2,

					"text" : uiscriptlocale.PICK_UP_WEAPON_ARROW,

					"default_image" : ROOT_PATH + "Middle_Button_01.sub",
					"over_image" : ROOT_PATH + "Middle_Button_02.sub",
					"down_image" : ROOT_PATH + "Middle_Button_03.sub",
				},
				#WEAPON
				
				#ARMOR
				{
					"name" : "armor_setting_menu_bg", "type" : "image", "x" : 15, "y" : 150,
					"image" : CHATTING_PATH + "chattingoption_menu_bg.sub",
					"children" :
					(
						{ "name" : "armor_setting", "type" : "text", "x" : 0, "y" : 0, "text" : uiscriptlocale.PICK_UP_TITLE_ARMOR, "all_align":"center" },
					),
				},
				{
					"name" : "line_armor_top",
					"type" : "line",

					"x" : 13,
					"y" : 150 + 27,

					"width" : 226,
					"height" : 0,

					"color" : 0xff777777,
				},
				{
					"name" : "line_armor_bottom",
					"type" : "line",

					"x" : 13,
					"y" : 150 + 27 + 80,

					"width" : 226,
					"height" : 0,

					"color" : 0xff777777,
				},
				{
					"name" : "filter_7",
					"type" : "toggle_button",

					"x" : 30,
					"y" : 183,

					"text" : uiscriptlocale.PICK_UP_ARMOR_BODY,

					"default_image" : ROOT_PATH + "Middle_Button_01.sub",
					"over_image" : ROOT_PATH + "Middle_Button_02.sub",
					"down_image" : ROOT_PATH + "Middle_Button_03.sub",
				},
				{
					"name" : "filter_8",
					"type" : "toggle_button",

					"x" : 30 + MIDDLE_BUTTON_WIDTH,
					"y" : 183,

					"text" : uiscriptlocale.PICK_UP_ARMOR_HEAD,

					"default_image" : ROOT_PATH + "Middle_Button_01.sub",
					"over_image" : ROOT_PATH + "Middle_Button_02.sub",
					"down_image" : ROOT_PATH + "Middle_Button_03.sub",
				},
				{
					"name" : "filter_9",
					"type" : "toggle_button",

					"x" : 30 + MIDDLE_BUTTON_WIDTH * 2,
					"y" : 183,

					"text" : uiscriptlocale.PICK_UP_ARMOR_SHIELD,

					"default_image" : ROOT_PATH + "Middle_Button_01.sub",
					"over_image" : ROOT_PATH + "Middle_Button_02.sub",
					"down_image" : ROOT_PATH + "Middle_Button_03.sub",
				},
				{
					"name" : "filter_10",
					"type" : "toggle_button",

					"x" : 30,
					"y" : 183 + MIDDLE_BUTTON_HEIGHT,

					"text" : uiscriptlocale.PICK_UP_ARMOR_WRIST,

					"default_image" : ROOT_PATH + "Middle_Button_01.sub",
					"over_image" : ROOT_PATH + "Middle_Button_02.sub",
					"down_image" : ROOT_PATH + "Middle_Button_03.sub",
				},
				{
					"name" : "filter_11",
					"type" : "toggle_button",

					"x" : 30 + MIDDLE_BUTTON_WIDTH,
					"y" : 183 + MIDDLE_BUTTON_HEIGHT,

					"text" : uiscriptlocale.PICK_UP_ARMOR_FOOT,

					"default_image" : ROOT_PATH + "Middle_Button_01.sub",
					"over_image" : ROOT_PATH + "Middle_Button_02.sub",
					"down_image" : ROOT_PATH + "Middle_Button_03.sub",
				},
				{
					"name" : "filter_12",
					"type" : "toggle_button",

					"x" : 30 + MIDDLE_BUTTON_WIDTH * 2,
					"y" : 183 + MIDDLE_BUTTON_HEIGHT,

					"text" : uiscriptlocale.PICK_UP_ARMOR_NECK,

					"default_image" : ROOT_PATH + "Middle_Button_01.sub",
					"over_image" : ROOT_PATH + "Middle_Button_02.sub",
					"down_image" : ROOT_PATH + "Middle_Button_03.sub",
				},
				
				{
					"name" : "filter_13",
					"type" : "toggle_button",

					"x" : 30,
					"y" : 183 + MIDDLE_BUTTON_HEIGHT * 2,

					"text" : uiscriptlocale.PICK_UP_ARMOR_EAR,

					"default_image" : ROOT_PATH + "Middle_Button_01.sub",
					"over_image" : ROOT_PATH + "Middle_Button_02.sub",
					"down_image" : ROOT_PATH + "Middle_Button_03.sub",
				},
				#ARMOR
				
				#OTHER
				{
					"name" : "other_setting_menu_bg", "type" : "image", "x" : 15, "y" : 263,
					"image" : CHATTING_PATH + "chattingoption_menu_bg.sub",
					"children" :
					(
						{ "name" : "other_setting", "type" : "text", "x" : 0, "y" : 0, "text" : uiscriptlocale.PICK_UP_TITLE_OTHER, "all_align":"center" },
					),
				},
				{
					"name" : "line_other_top",
					"type" : "line",

					"x" : 13,
					"y" : 263 + 27,

					"width" : 226,
					"height" : 0,

					"color" : 0xff777777,
				},
				{
					"name" : "line_other_bottom",
					"type" : "line",

					"x" : 13,
					"y" : 263 + 27 + 80,

					"width" : 226,
					"height" : 0,

					"color" : 0xff777777,
				},
				{
					"name" : "filter_14",
					"type" : "toggle_button",

					"x" : 30,
					"y" : 296,

					"text" : uiscriptlocale.PICK_UP_OTHER_METIN,

					"default_image" : ROOT_PATH + "Middle_Button_01.sub",
					"over_image" : ROOT_PATH + "Middle_Button_02.sub",
					"down_image" : ROOT_PATH + "Middle_Button_03.sub",
				},
				{
					"name" : "filter_15",
					"type" : "toggle_button",

					"x" : 30 + MIDDLE_BUTTON_WIDTH,
					"y" : 296,

					"text" : uiscriptlocale.PICK_UP_OTHER_YANG,

					"default_image" : ROOT_PATH + "Middle_Button_01.sub",
					"over_image" : ROOT_PATH + "Middle_Button_02.sub",
					"down_image" : ROOT_PATH + "Middle_Button_03.sub",
				},
				{
					"name" : "filter_16",
					"type" : "toggle_button",

					"x" : 30 + MIDDLE_BUTTON_WIDTH * 2,
					"y" : 296,

					"text" : uiscriptlocale.PICK_UP_OTHER_SKILLBOOK,

					"default_image" : ROOT_PATH + "Middle_Button_01.sub",
					"over_image" : ROOT_PATH + "Middle_Button_02.sub",
					"down_image" : ROOT_PATH + "Middle_Button_03.sub",
				},
				{
					"name" : "filter_17",
					"type" : "toggle_button",

					"x" : 30,
					"y" : 296 + MIDDLE_BUTTON_HEIGHT,

					"text" : uiscriptlocale.PICK_UP_OTHER_CHEST,

					"default_image" : ROOT_PATH + "Middle_Button_01.sub",
					"over_image" : ROOT_PATH + "Middle_Button_02.sub",
					"down_image" : ROOT_PATH + "Middle_Button_03.sub",
				},
				{
					"name" : "filter_18",
					"type" : "toggle_button",

					"x" : 30 + MIDDLE_BUTTON_WIDTH,
					"y" : 296 + MIDDLE_BUTTON_HEIGHT,

					"text" : uiscriptlocale.PICK_UP_OTHER_BELT,

					"default_image" : ROOT_PATH + "Middle_Button_01.sub",
					"over_image" : ROOT_PATH + "Middle_Button_02.sub",
					"down_image" : ROOT_PATH + "Middle_Button_03.sub",
				},
				{
					"name" : "filter_19",
					"type" : "toggle_button",

					"x" : 30 + MIDDLE_BUTTON_WIDTH * 2,
					"y" : 296 + MIDDLE_BUTTON_HEIGHT,

					"text" : uiscriptlocale.PICK_UP_OTHER_POLY,

					"default_image" : ROOT_PATH + "Middle_Button_01.sub",
					"over_image" : ROOT_PATH + "Middle_Button_02.sub",
					"down_image" : ROOT_PATH + "Middle_Button_03.sub",
				},
				{
					"name" : "filter_20",
					"type" : "toggle_button",

					"x" : 30,
					"y" : 296 + MIDDLE_BUTTON_HEIGHT * 2,

					"text" : uiscriptlocale.PICK_UP_OTHER_RING,

					"default_image" : ROOT_PATH + "Middle_Button_01.sub",
					"over_image" : ROOT_PATH + "Middle_Button_02.sub",
					"down_image" : ROOT_PATH + "Middle_Button_03.sub",
				},
				{
					"name" : "filter_21",
					"type" : "toggle_button",

					"x" : 30 + MIDDLE_BUTTON_WIDTH,
					"y" : 296 + MIDDLE_BUTTON_HEIGHT * 2,

					"text" : uiscriptlocale.PICK_UP_OTHER_POTION,

					"default_image" : ROOT_PATH + "Middle_Button_01.sub",
					"over_image" : ROOT_PATH + "Middle_Button_02.sub",
					"down_image" : ROOT_PATH + "Middle_Button_03.sub",
				},
				{
					"name" : "filter_22",
					"type" : "toggle_button",

					"x" : 30 + MIDDLE_BUTTON_WIDTH * 2,
					"y" : 296 + MIDDLE_BUTTON_HEIGHT * 2,

					"text" : uiscriptlocale.PICK_UP_OTHER_MATERIAL,

					"default_image" : ROOT_PATH + "Middle_Button_01.sub",
					"over_image" : ROOT_PATH + "Middle_Button_02.sub",
					"down_image" : ROOT_PATH + "Middle_Button_03.sub",
				},
				#OTHER
				
				#MODE
				{
					"name" : "mode_setting_menu_bg", "type" : "image", "x" : 15, "y" : 375,
					"image" : CHATTING_PATH + "chattingoption_menu_bg.sub",
					"children" :
					(
						{ "name" : "mode_setting", "type" : "text", "x" : 0, "y" : 0, "text" : uiscriptlocale.PICK_UP_TITLE_MODE, "all_align":"center" },
					),
				},
				{
					"name" : "line_mod_top",
					"type" : "line",

					"x" : 13,
					"y" : 375 + 27,

					"width" : 226,
					"height" : 0,

					"color" : 0xff777777,
				},
				{
					"name" : "line_mod_bottom",
					"type" : "line",

					"x" : 13,
					"y" : 375 + 27 + 28,

					"width" : 226,
					"height" : 0,

					"color" : 0xff777777,
				},
				#{
					# "name" : "mode_all",
					# "type" : "radio_button",

					# "x" : 30,
					# "y" : 375 + 33,

					# "text" : uiscriptlocale.PICK_UP_MODE_SINGLE,

					# "default_image" : ROOT_PATH + "Large_Button_01.sub",
					# "over_image" : ROOT_PATH + "Large_Button_02.sub",
					# "down_image" : ROOT_PATH + "Large_Button_03.sub",
				# },
				{
					"name" : "mode_all",
					"type" : "radio_button",

					"x" : 30 + MIDDLE_BUTTON_WIDTH + 30,
					"y" : 375 + 33,

					"text" : uiscriptlocale.PICK_UP_MODE_ALL,

					"default_image" : ROOT_PATH + "Large_Button_01.sub",
					"over_image" : ROOT_PATH + "Large_Button_02.sub",
					"down_image" : ROOT_PATH + "Large_Button_03.sub",
				},
				#MODE
				
				#SIZE
				{
					"name" : "size_setting_menu_bg", "type" : "image", "x" : 15, "y" : 435,
					"image" : CHATTING_PATH + "chattingoption_menu_bg.sub",
					"children" :
					(
						{ "name" : "size_setting", "type" : "text", "x" : 0, "y" : 0, "text" : uiscriptlocale.PICK_UP_TITLE_SIZE, "all_align":"center" },
					),
				},
				{
					"name" : "line_size_top",
					"type" : "line",

					"x" : 13,
					"y" : 435 + 27,

					"width" : 226,
					"height" : 0,

					"color" : 0xff777777,
				},
				{
					"name" : "line_size_bottom",
					"type" : "line",

					"x" : 13,
					"y" : 435 + 27 + 30,

					"width" : 226,
					"height" : 0,

					"color" : 0xff777777,
				},
				{
					"name" : "filter_size_0",
					"type" : "toggle_button",

					"x" : 30,
					"y" : 468,

					"text" : uiscriptlocale.PICK_UP_SIZE_SMALL,

					"default_image" : ROOT_PATH + "Middle_Button_01.sub",
					"over_image" : ROOT_PATH + "Middle_Button_02.sub",
					"down_image" : ROOT_PATH + "Middle_Button_03.sub",
				},
				{
					"name" : "filter_size_1",
					"type" : "toggle_button",

					"x" : 30 + MIDDLE_BUTTON_WIDTH,
					"y" : 468,

					"text" : uiscriptlocale.PICK_UP_SIZE_MID,

					"default_image" : ROOT_PATH + "Middle_Button_01.sub",
					"over_image" : ROOT_PATH + "Middle_Button_02.sub",
					"down_image" : ROOT_PATH + "Middle_Button_03.sub",
				},
				{
					"name" : "filter_size_2",
					"type" : "toggle_button",

					"x" : 30 + MIDDLE_BUTTON_WIDTH * 2,
					"y" : 468,

					"text" : uiscriptlocale.PICK_UP_SIZE_BIG,

					"default_image" : ROOT_PATH + "Middle_Button_01.sub",
					"over_image" : ROOT_PATH + "Middle_Button_02.sub",
					"down_image" : ROOT_PATH + "Middle_Button_03.sub",
				},
				#SIZE
				
				#REFINE
				{
					"name" : "refine_setting_menu_bg", "type" : "image", "x" : 15, "y" : 497,
					"image" : CHATTING_PATH + "chattingoption_menu_bg.sub",
					"children" :
					(
						{ "name" : "refine_setting", "type" : "text", "x" : 0, "y" : 0, "text" : uiscriptlocale.PICK_UP_TITLE_REFINE, "all_align":"center" },
					),
				},
				{
					"name" : "line_refine_top",
					"type" : "line",

					"x" : 13,
					"y" : 497 + 27,

					"width" : 226,
					"height" : 0,

					"color" : 0xff777777,
				},
				{
					"name" : "line_refine_bottom",
					"type" : "line",

					"x" : 13,
					"y" : 497 + 27 + 30,

					"width" : 226,
					"height" : 0,

					"color" : 0xff777777,
				},
				{ "name" : "refinetext", "type" : "text", "x" : 115, "y" : 533, "text" : "~"},
				{
					"name" : "minrefineslot",
					"type" : "image",
					"x" : 62,
					"y" : 530,
					"image" : "d:/ymir work/ui/privatesearch/private_leftSlotHalfImg.sub",
					"children" :
					(
						{
							"name" : "minrefinevalue",
							"type" : "editline",
							"x" : 2,
							"y" : 3,
							"width" : 36,
							"height" : 15,
							"input_limit" : 1,
							"only_number" : 1,
							"text" : "0",
						},
					),
				},
				{
					"name" : "maxrefineslot",
					"type" : "image",
					"x" : 140,
					"y" : 530,
					"image" : "d:/ymir work/ui/privatesearch/private_leftSlotHalfImg.sub",

					"children" :
					(
						{
							"name" : "maxrefinevalue",
							"type" : "editline",
							"x" : 2,
							"y" : 3,
							"width" : 36,
							"height" : 15,
							"input_limit" : 1,
							"only_number" : 1,
							"text" : "9",
						},
					),
				},
				#REFINE
				
				#LEVEL
				{
					"name" : "level_setting_menu_bg", "type" : "image", "x" : 15, "y" : 559,
					"image" : CHATTING_PATH + "chattingoption_menu_bg.sub",
					"children" :
					(
						{ "name" : "level_setting", "type" : "text", "x" : 0, "y" : 0, "text" : uiscriptlocale.PICK_UP_TITLE_LEVEL, "all_align":"center" },
					),
				},
				{
					"name" : "line_level_top",
					"type" : "line",

					"x" : 13,
					"y" : 559 + 27,

					"width" : 226,
					"height" : 0,

					"color" : 0xff777777,
				},
				{
					"name" : "line_level_bottom",
					"type" : "line",

					"x" : 13,
					"y" : 559 + 27 + 34,

					"width" : 226,
					"height" : 0,

					"color" : 0xff777777,
				},
				{ "name" : "leveltext", "type" : "text", "x" : 115, "y" : 595, "text" : "~"},
				{
					"name" : "minlevelslot",
					"type" : "image",
					"x" : 62,
					"y" : 592,
					"image" : "d:/ymir work/ui/privatesearch/private_leftSlotHalfImg.sub",
					"children" :
					(
						{
							"name" : "minlevelvalue",
							"type" : "editline",
							"x" : 2,
							"y" : 3,
							"width" : 36,
							"height" : 15,
							"input_limit" : 3,
							"only_number" : 1,
							"text" : "0",
						},
					),
				},
				{
					"name" : "maxlevelslot",
					"type" : "image",
					"x" : 140,
					"y" : 592,
					"image" : "d:/ymir work/ui/privatesearch/private_leftSlotHalfImg.sub",

					"children" :
					(
						{
							"name" : "maxlevelvalue",
							"type" : "editline",
							"x" : 2,
							"y" : 3,
							"width" : 36,
							"height" : 15,
							"input_limit" : 3,
							"only_number" : 1,
							"text" : "250",
						},
					),
				},
				#LEVEL
			),
		},
	),
}