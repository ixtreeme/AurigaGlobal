import uiscriptlocale

BOARD_VALUE_FILE = "d:/ymir work/ui/public/Parameter_Slot_06.sub"

PLUS_WITDH = 80
PLUS_RIGHT_WITDH = 40
ROOT_DIR = "d:/ymir work/ui/game/guild/guildboardpage/"
window = {
	"name" : "GuildWindow_BoardPage",

	"x" : 8,
	"y" : 30,

	"width" : 360 + PLUS_WITDH,
	"height" : 298,

	"children" :
	(
		## GuildGradeTItle
		{
			"name" : "GuildGradeTItle", "type" : "image", "x" : 10, "y" : 1, "image" : ROOT_DIR+"GuildBoardTItle.sub",
		},
		## IndexID
		{
			"name" : "IndexID", "type" : "image", "x" : 45, "y" : 4, "image" : ROOT_DIR+"IndexID.sub", #"thintooltip_text" : uiscriptlocale.GUILD_BOARD_ID,
		},
		## IndexMessages
		{
			"name" : "IndexMessages", "type" : "image", "x" : 248, "y" : 4, "image" : ROOT_DIR+"IndexMessages.sub", #"thintooltip_text" : uiscriptlocale.GUILD_BOARD_TEXT,
		},
		
		### ID
		#{
			#"name" : "IndexID", "type" : "text", "x" : 42, "y" : 8, "text" : uiscriptlocale.GUILD_BOARD_ID,
		#},
		### Messages
		#{
			#"name" : "IndexMessages", "type" : "text", "x" : 212 + PLUS_RIGHT_WITDH, "y" : 8, "text" : uiscriptlocale.GUILD_BOARD_TEXT,
		#},

		## Refresh Button
		{
			"name" : "RefreshButton",
			"type" : "button",
			"x" : 337 + PLUS_WITDH,
			"y" : 5,
			"default_image" : "d:/ymir work/ui/game/guild/Refresh_Button_01.sub",
			"over_image" : "d:/ymir work/ui/game/guild/Refresh_Button_02.sub",
			"down_image" : "d:/ymir work/ui/game/guild/Refresh_Button_03.sub",
			"tooltip_text" : uiscriptlocale.GUILD_BOARD_REFRESH,
		},
		## EditLine
		{
			"name" : "CommentSlot",
			"type" : "slotbar",
			"x" : 15,
			"y" : 272,
			"width" : 315 + PLUS_WITDH,
			"height" : 18,

			"children" :
			(
				{
					"name" : "CommentValue",
					"type" : "editline",
					"x" : 2,
					"y" : 3,
					"width" : 305 + PLUS_WITDH,
					"height" : 15,
					"input_limit" : 49,
					"check_width" : 1,
					"text" : "",
				},
			),
		},

	),
}
