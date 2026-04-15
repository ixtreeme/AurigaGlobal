import uiscriptlocale

ROOT_PATH = "d:/ymir work/ui/public/"

TEMPORARY_X = +13
BUTTON_TEMPORARY_X = 5
PVP_X = -10

LINE_LABEL_X 	= 24
LINE_DATA_X 	= 114
LINE_BEGIN	= 40
LINE_STEP	= 25
SMALL_BUTTON_WIDTH 	= 45
MIDDLE_BUTTON_WIDTH 	= 65

BOARD_WITDH = 330
BOARD_HEIGHT = 323
window = {
	"name" : "GameOptionDialog",
	"style" : ["movable", "float",],

	"x" : 0,
	"y" : 0,

	"width" : BOARD_WITDH,
	"height" : BOARD_HEIGHT, # 46

	"children" :
	[
		{
			"name" : "board",
			"type" : "board",

			"x" : 0,
			"y" : 0,

			"width" : BOARD_WITDH,
			"height" : BOARD_HEIGHT, # 46

			"children" :
			[
				{
					"name" : "page1",
					"type" : "window",
					"style" : ["attach",],
					"x" : 0,
					"y" : 0,
					"width" : BOARD_WITDH,
					"height" : BOARD_HEIGHT,
					"children" :
					[
						## ÀÌ¸§»ö
						{
							"name" : "name_color",
							"type" : "text",

							"x" : LINE_LABEL_X,
							"y" : 40+3,

							"text" : uiscriptlocale.OPTION_NAME_COLOR,
						},
						{
							"name" : "name_color_normal",
							"type" : "radio_button",

							"x" : LINE_DATA_X+MIDDLE_BUTTON_WIDTH*0,
							"y" : 40,

							"text" : uiscriptlocale.OPTION_NAME_COLOR_NORMAL,

							"default_image" : ROOT_PATH + "Large_Button_01.sub",
							"over_image" : ROOT_PATH + "Large_Button_02.sub",
							"down_image" : ROOT_PATH + "Large_Button_03.sub",
						},
						{
							"name" : "name_color_empire",
							"type" : "radio_button",

							"x" : LINE_DATA_X+MIDDLE_BUTTON_WIDTH*1+28,
							"y" : 40,

							"text" : uiscriptlocale.OPTION_NAME_COLOR_EMPIRE,

							"default_image" : ROOT_PATH + "Large_Button_01.sub",
							"over_image" : ROOT_PATH + "Large_Button_02.sub",
							"down_image" : ROOT_PATH + "Large_Button_03.sub",
						},
						{
							"name" : "target_board",
							"type" : "text",

							"x" : LINE_LABEL_X,
							"y" : 65+3,

							"text" : uiscriptlocale.OPTION_TARGET_BOARD,
						},
						{
							"name" : "target_board_no_view",
							"type" : "radio_button",

							"x" : LINE_DATA_X+MIDDLE_BUTTON_WIDTH*0,
							"y" : 65,

							"text" : uiscriptlocale.OPTION_TARGET_BOARD_NO_VIEW,

							"default_image" : ROOT_PATH + "Middle_Button_01.sub",
							"over_image" : ROOT_PATH + "Middle_Button_02.sub",
							"down_image" : ROOT_PATH + "Middle_Button_03.sub",
						},
						{
							"name" : "target_board_view",
							"type" : "radio_button",

							"x" : LINE_DATA_X+MIDDLE_BUTTON_WIDTH*1,
							"y" : 65,

							"text" : uiscriptlocale.OPTION_TARGET_BOARD_VIEW,

							"default_image" : ROOT_PATH + "Middle_Button_01.sub",
							"over_image" : ROOT_PATH + "Middle_Button_02.sub",
							"down_image" : ROOT_PATH + "Middle_Button_03.sub",
						},
						## PvP Mode
						{
							"name" : "pvp_mode",
							"type" : "text",

							"x" : LINE_LABEL_X,
							"y" : 90+3,

							"text" : uiscriptlocale.OPTION_PVPMODE,
						},
						{
							"name" : "pvp_peace",
							"type" : "radio_button",

							"x" : LINE_DATA_X+SMALL_BUTTON_WIDTH*0,
							"y" : 90,

							"text" : uiscriptlocale.OPTION_PVPMODE_PEACE,
							"tooltip_text" : uiscriptlocale.OPTION_PVPMODE_PEACE_TOOLTIP,

							"default_image" : ROOT_PATH + "small_Button_01.sub",
							"over_image" : ROOT_PATH + "small_Button_02.sub",
							"down_image" : ROOT_PATH + "small_Button_03.sub",
						},
						{
							"name" : "pvp_revenge",
							"type" : "radio_button",

							"x" : LINE_DATA_X+SMALL_BUTTON_WIDTH*1,
							"y" : 90,

							"text" : uiscriptlocale.OPTION_PVPMODE_REVENGE,
							"tooltip_text" : uiscriptlocale.OPTION_PVPMODE_REVENGE_TOOLTIP,

							"default_image" : ROOT_PATH + "small_Button_01.sub",
							"over_image" : ROOT_PATH + "small_Button_02.sub",
							"down_image" : ROOT_PATH + "small_Button_03.sub",
						},
						{
							"name" : "pvp_guild",
							"type" : "radio_button",

							"x" : LINE_DATA_X+SMALL_BUTTON_WIDTH*2,
							"y" : 90,

							"text" : uiscriptlocale.OPTION_PVPMODE_GUILD,
							"tooltip_text" : uiscriptlocale.OPTION_PVPMODE_GUILD_TOOLTIP,

							"default_image" : ROOT_PATH + "small_Button_01.sub",
							"over_image" : ROOT_PATH + "small_Button_02.sub",
							"down_image" : ROOT_PATH + "small_Button_03.sub",
						},
						{
							"name" : "pvp_free",
							"type" : "radio_button",

							"x" : LINE_DATA_X+SMALL_BUTTON_WIDTH*3,
							"y" : 90,

							"text" : uiscriptlocale.OPTION_PVPMODE_FREE,
							"tooltip_text" : uiscriptlocale.OPTION_PVPMODE_FREE_TOOLTIP,

							"default_image" : ROOT_PATH + "small_Button_01.sub",
							"over_image" : ROOT_PATH + "small_Button_02.sub",
							"down_image" : ROOT_PATH + "small_Button_03.sub",
						},
						## Block
						{
							"name" : "block",
							"type" : "text",

							"x" : LINE_LABEL_X,
							"y" : 115+3,

							"text" : uiscriptlocale.OPTION_BLOCK,
						},
						{
							"name" : "block_exchange_button",
							"type" : "toggle_button",

							"x" : LINE_DATA_X+MIDDLE_BUTTON_WIDTH*0,
							"y" : 115,

							"text" : uiscriptlocale.OPTION_BLOCK_EXCHANGE,

							"default_image" : ROOT_PATH + "middle_button_01.sub",
							"over_image" : ROOT_PATH + "middle_button_02.sub",
							"down_image" : ROOT_PATH + "middle_button_03.sub",
						},
						{
							"name" : "block_party_button",
							"type" : "toggle_button",

							"x" : LINE_DATA_X+MIDDLE_BUTTON_WIDTH*1,
							"y" : 115,

							"text" : uiscriptlocale.OPTION_BLOCK_PARTY,

							"default_image" : ROOT_PATH + "middle_button_01.sub",
							"over_image" : ROOT_PATH + "middle_button_02.sub",
							"down_image" : ROOT_PATH + "middle_button_03.sub",
						},
						{
							"name" : "block_guild_button",
							"type" : "toggle_button",

							"x" : LINE_DATA_X+MIDDLE_BUTTON_WIDTH*2,
							"y" : 115,

							"text" : uiscriptlocale.OPTION_BLOCK_GUILD,

							"default_image" : ROOT_PATH + "middle_button_01.sub",
							"over_image" : ROOT_PATH + "middle_button_02.sub",
							"down_image" : ROOT_PATH + "middle_button_03.sub",
						},
						{
							"name" : "block_whisper_button",
							"type" : "toggle_button",

							"x" : LINE_DATA_X+MIDDLE_BUTTON_WIDTH*0,
							"y" : 140,

							"text" : uiscriptlocale.OPTION_BLOCK_WHISPER,

							"default_image" : ROOT_PATH + "middle_button_01.sub",
							"over_image" : ROOT_PATH + "middle_button_02.sub",
							"down_image" : ROOT_PATH + "middle_button_03.sub",
						},
						{
							"name" : "block_friend_button",
							"type" : "toggle_button",

							"x" : LINE_DATA_X+MIDDLE_BUTTON_WIDTH*1,
							"y" : 140,

							"text" : uiscriptlocale.OPTION_BLOCK_FRIEND,

							"default_image" : ROOT_PATH + "middle_button_01.sub",
							"over_image" : ROOT_PATH + "middle_button_02.sub",
							"down_image" : ROOT_PATH + "middle_button_03.sub",
						},
						{
							"name" : "block_party_request_button",
							"type" : "toggle_button",

							"x" : LINE_DATA_X+MIDDLE_BUTTON_WIDTH*2,
							"y" : 140,

							"text" : uiscriptlocale.OPTION_BLOCK_PARTY_REQUEST,

							"default_image" : ROOT_PATH + "middle_button_01.sub",
							"over_image" : ROOT_PATH + "middle_button_02.sub",
							"down_image" : ROOT_PATH + "middle_button_03.sub",
						},

						## Chat
						{
							"name" : "chat",
							"type" : "text",

							"x" : LINE_LABEL_X,
							"y" : 165+3,

							"text" : uiscriptlocale.OPTION_VIEW_CHAT,
						},
						{
							"name" : "view_chat_on_button",
							"type" : "radio_button",

							"x" : LINE_DATA_X,
							"y" : 165,

							"text" : uiscriptlocale.OPTION_VIEW_CHAT_ON,

							"default_image" : ROOT_PATH + "middle_button_01.sub",
							"over_image" : ROOT_PATH + "middle_button_02.sub",
							"down_image" : ROOT_PATH + "middle_button_03.sub",
						},
						{
							"name" : "view_chat_off_button",
							"type" : "radio_button",

							"x" : LINE_DATA_X+MIDDLE_BUTTON_WIDTH,
							"y" : 165,

							"text" : uiscriptlocale.OPTION_VIEW_CHAT_OFF,

							"default_image" : ROOT_PATH + "middle_button_01.sub",
							"over_image" : ROOT_PATH + "middle_button_02.sub",
							"down_image" : ROOT_PATH + "middle_button_03.sub",
						},

						## Always Show Name
						{
							"name" : "always_show_name",
							"type" : "text",

							"x" : LINE_LABEL_X,
							"y" : 190+3,

							"text" : uiscriptlocale.OPTION_ALWAYS_SHOW_NAME,
						},
						{
							"name" : "always_show_name_on_button",
							"type" : "radio_button",

							"x" : LINE_DATA_X,
							"y" : 190,

							"text" : uiscriptlocale.OPTION_ALWAYS_SHOW_NAME_ON,

							"default_image" : ROOT_PATH + "middle_button_01.sub",
							"over_image" : ROOT_PATH + "middle_button_02.sub",
							"down_image" : ROOT_PATH + "middle_button_03.sub",
						},
						{
							"name" : "always_show_name_off_button",
							"type" : "radio_button",

							"x" : LINE_DATA_X+MIDDLE_BUTTON_WIDTH,
							"y" : 190,

							"text" : uiscriptlocale.OPTION_ALWAYS_SHOW_NAME_OFF,

							"default_image" : ROOT_PATH + "middle_button_01.sub",
							"over_image" : ROOT_PATH + "middle_button_02.sub",
							"down_image" : ROOT_PATH + "middle_button_03.sub",
						},
						{
							"name" : "name_type",
							"type" : "text",
							"x" : LINE_LABEL_X,
							"y" : 215+3,
							"text" : uiscriptlocale.OPTION_NAMES_TYPE_NAME,
						},
						{
							"name" : "name_type1_button",
							"type" : "radio_button",
							"x" : LINE_DATA_X,
							"y" : 215,
							"text" : uiscriptlocale.OPTION_NAMES_TYPE_NAME_0,
							"default_image" : ROOT_PATH + "middle_button_01.sub",
							"over_image" : ROOT_PATH + "middle_button_02.sub",
							"down_image" : ROOT_PATH + "middle_button_03.sub",
						},
						{
							"name" : "name_type2_button",
							"type" : "radio_button",
							"x" : LINE_DATA_X+MIDDLE_BUTTON_WIDTH,
							"y" : 215,
							"text" : uiscriptlocale.OPTION_NAMES_TYPE_NAME_1,
							"default_image" : ROOT_PATH + "middle_button_01.sub",
							"over_image" : ROOT_PATH + "middle_button_02.sub",
							"down_image" : ROOT_PATH + "middle_button_03.sub",
						},
						{
							"name" : "show_mob_info",
							"type" : "text",
							"multi_line" : 1,
							"x" : LINE_LABEL_X,
							"y" : 240+3,
							"text" : uiscriptlocale.OPTION_MOB_INFO,
						},
						{
							"name" : "show_mob_level_button",
							"type" : "toggle_button",
							"x" : LINE_DATA_X,
							"y" : 240,
							"text" : uiscriptlocale.OPTION_MOB_INFO_LEVEL,
							"default_image" : ROOT_PATH + "middle_button_01.sub",
							"over_image" : ROOT_PATH + "middle_button_02.sub",
							"down_image" : ROOT_PATH + "middle_button_03.sub",
						},
						{
							"name" : "show_mob_AI_flag_button",
							"type" : "toggle_button",
							"x" : LINE_DATA_X+MIDDLE_BUTTON_WIDTH,
							"y" : 240,
							"text" : uiscriptlocale.OPTION_MOB_INFO_AGGR,
							"default_image" : ROOT_PATH + "middle_button_01.sub",
							"over_image" : ROOT_PATH + "middle_button_02.sub",
							"down_image" : ROOT_PATH + "middle_button_03.sub",
						},
						{
							"name" : "effect_on_off",
							"type" : "text",
							"x" : LINE_LABEL_X,
							"y" : 265+3,
							"text" : uiscriptlocale.OPTION_EFFECT,
						},
						{
							"name" : "show_damage_on_button",
							"type" : "radio_button",
							"x" : LINE_DATA_X,
							"y" : 265,
							"text" : uiscriptlocale.OPTION_VIEW_CHAT_ON,
							"default_image" : ROOT_PATH + "middle_button_01.sub",
							"over_image" : ROOT_PATH + "middle_button_02.sub",
							"down_image" : ROOT_PATH + "middle_button_03.sub",
						},
						{
							"name" : "show_damage_off_button",
							"type" : "radio_button",
							"x" : LINE_DATA_X+MIDDLE_BUTTON_WIDTH,
							"y" : 265,
							"text" : uiscriptlocale.OPTION_VIEW_CHAT_OFF,
							"default_image" : ROOT_PATH + "middle_button_01.sub",
							"over_image" : ROOT_PATH + "middle_button_02.sub",
							"down_image" : ROOT_PATH + "middle_button_03.sub",
						},
					],
				},
				{
					"name" : "page2",
					"type" : "window",
					"style" : ["attach",],
					"x" : 0,
					"y" : 0,
					"width" : BOARD_WITDH,
					"height" : BOARD_HEIGHT,
					"children" :
					[
						{
							"name" : "hide_mode",
							"type" : "text",
							"x" : LINE_LABEL_X,
							"y" : 40+(25 * 0)+3,
							"text" : uiscriptlocale.HIDE_OPTION,
						},
						{
							"name" : "hidemode_0",
							"type" : "toggle_button",
							"x" : LINE_DATA_X,
							"y" : 40+(25 * 0),
							"text" : uiscriptlocale.HIDE_OPTION0,
							"default_image" : ROOT_PATH + "middle_button_01.sub",
							"over_image" : ROOT_PATH + "middle_button_02.sub",
							"down_image" : ROOT_PATH + "middle_button_03.sub",
						},
						{
							"name" : "hidemode_1",
							"type" : "toggle_button",
							"x" : LINE_DATA_X+MIDDLE_BUTTON_WIDTH,
							"y" : 40+(25 * 0),
							"text" : uiscriptlocale.HIDE_OPTION1,
							"default_image" : ROOT_PATH + "middle_button_01.sub",
							"over_image" : ROOT_PATH + "middle_button_02.sub",
							"down_image" : ROOT_PATH + "middle_button_03.sub",
						},
						{
							"name" : "hidemode_2",
							"type" : "toggle_button",
							"x" : LINE_DATA_X+MIDDLE_BUTTON_WIDTH+MIDDLE_BUTTON_WIDTH,
							"y" : 40+(25 * 0),
							"text" : uiscriptlocale.HIDE_OPTION2,
							"default_image" : ROOT_PATH + "middle_button_01.sub",
							"over_image" : ROOT_PATH + "middle_button_02.sub",
							"down_image" : ROOT_PATH + "middle_button_03.sub",
						},
						{
							"name" : "hidemode_3",
							"type" : "toggle_button",
							"x" : LINE_DATA_X,
							"y" : 40+(25 * 1),
							"text" : uiscriptlocale.HIDE_OPTION3,
							"default_image" : ROOT_PATH + "middle_button_01.sub",
							"over_image" : ROOT_PATH + "middle_button_02.sub",
							"down_image" : ROOT_PATH + "middle_button_03.sub",
						},
						{
							"name" : "hidemode_4",
							"type" : "toggle_button",
							"x" : LINE_DATA_X+MIDDLE_BUTTON_WIDTH,
							"y" : 40+(25 * 1),
							"text" : uiscriptlocale.HIDE_OPTION4,
							"default_image" : ROOT_PATH + "middle_button_01.sub",
							"over_image" : ROOT_PATH + "middle_button_02.sub",
							"down_image" : ROOT_PATH + "middle_button_03.sub",
						},
						{
							"name" : "hidemode_5",
							"type" : "toggle_button",
							"x" : LINE_DATA_X+MIDDLE_BUTTON_WIDTH+MIDDLE_BUTTON_WIDTH,
							"y" : 40+(25 * 1),
							"text" : uiscriptlocale.HIDE_OPTION5,
							"default_image" : ROOT_PATH + "middle_button_01.sub",
							"over_image" : ROOT_PATH + "middle_button_02.sub",
							"down_image" : ROOT_PATH + "middle_button_03.sub",
						},
						{
							"name" : "hidemode_6",
							"type" : "toggle_button",
							"x" : LINE_DATA_X,
							"y" : 40+(25 * 2),
							"text" : uiscriptlocale.GRAPHICONOFF_EFFECT_LEVEL,
							"default_image" : ROOT_PATH + "middle_button_01.sub",
							"over_image" : ROOT_PATH + "middle_button_02.sub",
							"down_image" : ROOT_PATH + "middle_button_03.sub",
						},
						{
							"name" : "hide2_mode",
							"type" : "text",
							"x" : LINE_LABEL_X,
							"y" : 40+(25 * 3)+3,
							"text" : uiscriptlocale.HIDE_2_OPTION,
						},
						{
							"name" : "hide2mode_0",
							"type" : "toggle_button",
							"x" : LINE_DATA_X,
							"y" : 40+(25 * 3),
							"text" : uiscriptlocale.HIDE_2_OPTION0,
							"default_image" : ROOT_PATH + "large_button_01.sub",
							"over_image" : ROOT_PATH + "large_button_02.sub",
							"down_image" : ROOT_PATH + "large_button_03.sub",
						},
						{
							"name" : "hide2mode_2",
							"type" : "toggle_button",
							"x" : LINE_DATA_X+MIDDLE_BUTTON_WIDTH+27,
							"y" : 40+(25 * 3),
							"text" : uiscriptlocale.HIDE_2_OPTION2,
							"default_image" : ROOT_PATH + "middle_button_01.sub",
							"over_image" : ROOT_PATH + "middle_button_02.sub",
							"down_image" : ROOT_PATH + "middle_button_03.sub",
						},
						{
							"name" : "hide2mode_1",
							"type" : "toggle_button",
							"x" : LINE_DATA_X,
							"y" : 40+(25 * 4),
							"text" : uiscriptlocale.HIDE_2_OPTION1,
							"default_image" : ROOT_PATH + "large_button_01.sub",
							"over_image" : ROOT_PATH + "large_button_02.sub",
							"down_image" : ROOT_PATH + "large_button_03.sub",
						},
						{
							"name" : "hide2mode_3",
							"type" : "toggle_button",
							"x" : LINE_DATA_X+MIDDLE_BUTTON_WIDTH+27,
							"y" : 40+(25 * 4),
							"text" : uiscriptlocale.HIDE_2_OPTION3,
							"default_image" : ROOT_PATH + "middle_button_01.sub",
							"over_image" : ROOT_PATH + "middle_button_02.sub",
							"down_image" : ROOT_PATH + "middle_button_03.sub",
						},
						{
							"name" : "day_mode",
							"type" : "text",
							"x" : LINE_LABEL_X,
							"y" : 40+(25 * 6)+3,
							"text" : uiscriptlocale.ENVIRONMENT_MODE,
						},
						{
							"name" : "environment_0",
							"type" : "radio_button",
							"x" : LINE_DATA_X,
							"y" : 40+(25 * 6),
							"text" : uiscriptlocale.ENVIRONMENT_MODE0,
							"default_image" : ROOT_PATH + "middle_button_01.sub",
							"over_image" : ROOT_PATH + "middle_button_02.sub",
							"down_image" : ROOT_PATH + "middle_button_03.sub",
						},
						{
							"name" : "environment_1",
							"type" : "radio_button",
							"x" : LINE_DATA_X+MIDDLE_BUTTON_WIDTH,
							"y" : 40+(25 * 6),
							"text" : uiscriptlocale.ENVIRONMENT_MODE1,
							"default_image" : ROOT_PATH + "middle_button_01.sub",
							"over_image" : ROOT_PATH + "middle_button_02.sub",
							"down_image" : ROOT_PATH + "middle_button_03.sub",
						},
						{
							"name" : "environment_2",
							"type" : "radio_button",
							"x" : LINE_DATA_X+MIDDLE_BUTTON_WIDTH+MIDDLE_BUTTON_WIDTH,
							"y" : 40+(25 * 6),
							"text" : uiscriptlocale.ENVIRONMENT_MODE2,
							"default_image" : ROOT_PATH + "middle_button_01.sub",
							"over_image" : ROOT_PATH + "middle_button_02.sub",
							"down_image" : ROOT_PATH + "middle_button_03.sub",
						},
						{
							"name" : "environment_3",
							"type" : "radio_button",
							"x" : LINE_DATA_X,
							"y" : 40+(25 * 7),
							"text" : uiscriptlocale.ENVIRONMENT_MODE3,
							"default_image" : ROOT_PATH + "middle_button_01.sub",
							"over_image" : ROOT_PATH + "middle_button_02.sub",
							"down_image" : ROOT_PATH + "middle_button_03.sub",
						},
						{
							"name" : "environment_4",
							"type" : "radio_button",
							"x" : LINE_DATA_X+MIDDLE_BUTTON_WIDTH,
							"y" : 40+(25 * 7),
							"text" : uiscriptlocale.ENVIRONMENT_MODE4,
							"default_image" : ROOT_PATH + "middle_button_01.sub",
							"over_image" : ROOT_PATH + "middle_button_02.sub",
							"down_image" : ROOT_PATH + "middle_button_03.sub",
						},
						{
							"name" : "environment_5",
							"type" : "radio_button",
							"x" : LINE_DATA_X+MIDDLE_BUTTON_WIDTH+MIDDLE_BUTTON_WIDTH,
							"y" : 40+(25 * 7),
							"text" : uiscriptlocale.ENVIRONMENT_MODE5,
							"default_image" : ROOT_PATH + "middle_button_01.sub",
							"over_image" : ROOT_PATH + "middle_button_02.sub",
							"down_image" : ROOT_PATH + "middle_button_03.sub",
						},
						{
							"name" : "environment_6",
							"type" : "radio_button",
							"x" : LINE_DATA_X,
							"y" : 40+(25 * 8),
							"text" : uiscriptlocale.ENVIRONMENT_MODE6,
							"default_image" : ROOT_PATH + "middle_button_01.sub",
							"over_image" : ROOT_PATH + "middle_button_02.sub",
							"down_image" : ROOT_PATH + "middle_button_03.sub",
						},
						{
							"name" : "time_pm",
							"type" : "text",
							"x" : LINE_LABEL_X,
							"y" : 40+(25 * 9)+3,
							"text" : uiscriptlocale.TIME_TYPE,
						},
						{
							"name" : "time_pm_on",
							"type" : "radio_button",
							"x" : LINE_DATA_X,
							"y" : 40+(25 * 9),
							"text" : uiscriptlocale.YES,
							"default_image" : ROOT_PATH + "middle_button_01.sub",
							"over_image" : ROOT_PATH + "middle_button_02.sub",
							"down_image" : ROOT_PATH + "middle_button_03.sub",
						},
						{
							"name" : "time_pm_off",
							"type" : "radio_button",
							"x" : LINE_DATA_X+MIDDLE_BUTTON_WIDTH,
							"y" : 40+(25 * 9),
							"text" : uiscriptlocale.NO,
							"default_image" : ROOT_PATH + "middle_button_01.sub",
							"over_image" : ROOT_PATH + "middle_button_02.sub",
							"down_image" : ROOT_PATH + "middle_button_03.sub",
						},
					],
				},
				{
					"name" : "page3",
					"type" : "window",
					"style" : ["attach",],
					"x" : 0,
					"y" : 0,
					"width" : BOARD_WITDH,
					"height" : BOARD_HEIGHT,
					"children" :
					[
						{
							"name" : "auto_pick",
							"type" : "text",
							"x" : LINE_LABEL_X,
							"y" : 40+(25 * 0)+3,
							"text" : uiscriptlocale.OPTION_AUTO_PICKUP,
						},
						{
							"name" : "auto_pick_item_on",
							"type" : "radio_button",
							"x" : LINE_DATA_X,
							"y" : 40+(25 * 0),
							"text" : uiscriptlocale.OPTION_AUTO_PICKUP_ON,
							"default_image" : ROOT_PATH + "middle_button_01.sub",
							"over_image" : ROOT_PATH + "middle_button_02.sub",
							"down_image" : ROOT_PATH + "middle_button_03.sub",
						},
						{
							"name" : "auto_pick_item_off",
							"type" : "radio_button",
							"x" : LINE_DATA_X+MIDDLE_BUTTON_WIDTH,
							"y" : 40+(25 * 0),
							"text" : uiscriptlocale.OPTION_AUTO_PICKUP_OFF,
							"default_image" : ROOT_PATH + "middle_button_01.sub",
							"over_image" : ROOT_PATH + "middle_button_02.sub",
							"down_image" : ROOT_PATH + "middle_button_03.sub",
						},
						# {
							# "name" : "PickUpText",
							# "type" : "text",

							# "x" : LINE_LABEL_X,
							# "y" : 240 + 25,

							# "text" : uiscriptlocale.PICK_UP,
						# },
						# {
							# "name" : "pick_up_show_button",
							# "type" : "button",

							# "x" : LINE_DATA_X,
							# "y" : 240 + 25,

							# "text" : uiscriptlocale.OPTION_SETTING,

							# "default_image" : ROOT_PATH + "middle_button_01.sub",
							# "over_image" : ROOT_PATH + "middle_button_02.sub",
							# "down_image" : ROOT_PATH + "middle_button_03.sub",
						# },
					],
				},
				## Title
				{
					"name" : "titlebar",
					"type" : "titlebar",
					"style" : ["attach",],

					"x" : 8,
					"y" : 8,

					"width" : BOARD_WITDH - 16,
					"color" : "gray",

					"children" :
					[
						{ "name":"titlename", "type":"text", "x":0, "y":3,
						"text" : uiscriptlocale.GAMEOPTION_TITLE,
						"horizontal_align":"center", "text_horizontal_align":"center" },
					],
				},
				{
					"name" : "page_btn0",
					"type" : "radio_button",
					"x" : BOARD_WITDH / 2 - (15 / 2) - 20,
					"y" : BOARD_HEIGHT - 28,
					"default_image" : "d:/ymir work/ui/gameoption/page_btn_normal.tga",
					"over_image" : "d:/ymir work/ui/gameoption/page_btn_over.tga",
					"down_image" : "d:/ymir work/ui/gameoption/page_btn_down.tga",
				},
				{
					"name" : "page_btn1",
					"type" : "radio_button",
					"x" : BOARD_WITDH / 2 - (15 / 2),
					"y" : BOARD_HEIGHT - 28,
					"default_image" : "d:/ymir work/ui/gameoption/page_btn_normal.tga",
					"over_image" : "d:/ymir work/ui/gameoption/page_btn_over.tga",
					"down_image" : "d:/ymir work/ui/gameoption/page_btn_down.tga",
				},
				{
					"name" : "page_btn2",
					"type" : "radio_button",
					"x" : BOARD_WITDH / 2 - (15 / 2) + 20,
					"y" : BOARD_HEIGHT - 28,
					"default_image" : "d:/ymir work/ui/gameoption/page_btn_normal.tga",
					"over_image" : "d:/ymir work/ui/gameoption/page_btn_over.tga",
					"down_image" : "d:/ymir work/ui/gameoption/page_btn_down.tga",
				},
			],
		},
	],
}

