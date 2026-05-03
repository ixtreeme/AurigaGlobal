import uiscriptlocale

SMALL_VALUE_FILE = "d:/ymir work/ui/public/Parameter_Slot_01.sub"
LARGE_VALUE_FILE = "d:/ymir work/ui/public/Parameter_Slot_03.sub"
XLARGE_VALUE_FILE = "d:/ymir work/ui/public/Parameter_Slot_04.sub"

BUTTON_ROOT = "d:/ymir work/ui/game/guild/guildbuttons/infopage/"
PLUS_WITDH = 80
PLUS_RIGHT_WITDH = 40
PLUS_LEFT_WITDH = 40
window = {
	"name" : "GuildWindow_GuildInfoPage",

	"x" : 8,
	"y" : 30,

	"width" : 360 + PLUS_WITDH,
	"height" : 298,

	"children" :
	(

		## Guild Info Title
		{
			"name":"Guild_Info_Title_Bar", "type":"horizontalbar", "x":5, "y":10, "width":167+ PLUS_WITDH/2,
			"children" :
			(
				{ "name":"Guild_Info_Point_Value", "type":"text", "x":8, "y":3, "text":uiscriptlocale.GUILD_INFO, },

				## GuildName
				{
					"name" : "GuildName", "type" : "text", "x" : 3, "y" : 31, "text" : uiscriptlocale.GUILD_INFO_NAME,
					"children" :
					(
						{
							"name" : "GuildNameSlot",
							"type" : "image",
							"x" : 70 + PLUS_LEFT_WITDH,
							"y" : -2,
							"image" : LARGE_VALUE_FILE,
							"children" :
							(
								{"name" : "GuildNameValue", "type":"text", "text":uiscriptlocale.GUILD_INFO_NAME_VALUE, "x":0, "y":0, "all_align":"center"},
							),
						},
					),
				},

				## GuildMaster
				{
					"name" : "GuildMaster", "type" : "text", "x" : 3, "y" : 57, "text" : uiscriptlocale.GUILD_INFO_MASTER,
					"children" :
					(
						{
							"name" : "GuildMasterNameSlot",
							"type" : "image",
							"x" : 70 + PLUS_LEFT_WITDH,
							"y" : -2,
							"image" : LARGE_VALUE_FILE,
							"children" :
							(
								{"name" : "GuildMasterNameValue", "type":"text", "text":uiscriptlocale.GUILD_INFO_MASTER_VALUE, "x":0, "y":0, "all_align":"center"},
							),
						},
					),
				},

				## GuildLevel
				{
					"name" : "GuildLevel", "type" : "text", "x" : 3, "y" : 93, "text" : uiscriptlocale.GUILD_INFO_LEVEL,
					"children" :
					(
						{
							"name" : "GuildLevelSlot",
							"type" : "slotbar",
							"x" : 70 + PLUS_LEFT_WITDH,
							"y" : -2,
							"width" : 45,
							"height" : 17,
							"children" :
							(
								{"name" : "GuildLevelValue", "type":"text", "text":"30", "x":0, "y":0, "all_align":"center"},
							),
						},
					),
				},

				## CurrentExperience
				{
					"name" : "CurrentExperience", "type" : "text", "x" : 3, "y" : 119, "text" : uiscriptlocale.GUILD_INFO_CUR_EXP,
					"children" :
					(
						{
							"name" : "CurrentExperienceSlot",
							"type" : "image",
							"x" : 70 + PLUS_LEFT_WITDH,
							"y" : -2,
							"image" : LARGE_VALUE_FILE,
							"children" :
							(
								{"name" : "CurrentExperienceValue", "type":"text", "text":"10000000", "x":0, "y":0, "all_align":"center"},
							),
						},
					),
				},

				## LastExperience
				{
					"name" : "LastExperience", "type" : "text", "x" : 3, "y" : 145, "text" : uiscriptlocale.GUILD_INFO_REST_EXP,
					"children" :
					(
						{
							"name" : "LastExperienceSlot",
							"type" : "image",
							"x" : 70 + PLUS_LEFT_WITDH,
							"y" : -2,
							"image" : LARGE_VALUE_FILE,
							"children" :
							(
								{"name" : "LastExperienceValue", "type":"text", "text":"123123123123", "x":0, "y":0, "all_align":"center"},
							),
						},
					),
				},

				## GuildMemberCount
				{
					"name" : "GuildMemberCount", "type" : "text", "x" : 3, "y" : 171, "text" : uiscriptlocale.GUILD_INFO_MEMBER_NUM,
					"children" :
					(
						{
							"name" : "GuildMemberCountSlot",
							"type" : "image",
							"x" : 70 + PLUS_LEFT_WITDH,
							"y" : -2,
							"image" : LARGE_VALUE_FILE,
							"children" :
							(
								{"name" : "GuildMemberCountValue", "type":"text", "text":"30 / 32", "x":0, "y":0, "all_align":"center"},
							),
						},
					),
				},

				## GuildMemberLevelAverage
				{
					"name" : "GuildMemberLevelAverage", "type" : "text", "x" : 3, "y" : 197, "text" : uiscriptlocale.GUILD_INFO_MEMBER_AVG_LEVEL,
					"children" :
					(
						{
							"name" : "GuildMemberLevelAverageSlot",
							"type" : "image",
							"x" : 108 + PLUS_LEFT_WITDH,
							"y" : -2,
							"image" : SMALL_VALUE_FILE,
							"children" :
							(
								{"name" : "GuildMemberLevelAverageValue", "type":"text", "text":"53", "x":0, "y":0, "all_align":"center"},
							),
						},
					),
				},

				## GuildTrophies
				{
					"name" : "GuildTrophies", "type" : "text", "x" : 3, "y" : 223, "text" : "Trophies",
					"children" :
					(
						{
							"name" : "GuildTrophiesSlot",
							"type" : "image",
							"x" : 108 + PLUS_LEFT_WITDH,
							"y" : -2,
							"image" : SMALL_VALUE_FILE,
							"children" :
							(
								{"name" : "GuildTrophiesValue", "type":"text", "text":"00", "x":0, "y":0, "all_align":"center"},
							),
						},
					),
				},
				## GuildWins
				{
					"name" : "GuildWins", "type" : "text", "x" : 5, "y" : 249, "text" : "Wins",
					"children" :
					(
						{
							"name" : "GuildWinsSlot",
							"type" : "image",
							"x" : 0,
							"y" : 14,
							"image" : SMALL_VALUE_FILE,
							"children" :
							(
								{"name" : "GuildWinsValue", "type":"text", "text":"00", "x":0, "y":0, "all_align":"center"},
							),
						},
					),
				},
				## GuildLoss
				{
					"name" : "GuildLoss", "type" : "text", "x" : 113, "y" : 249, "text" : "Loss",
					"children" :
					(
						{
							"name" : "GuildLossSlot",
							"type" : "image",
							"x" : 0,
							"y" : 14,
							"image" : SMALL_VALUE_FILE,
							"children" :
							(
								{"name" : "GuildLossValue", "type":"text", "text":"00", "x":0, "y":0, "all_align":"center"},
							),
						},
					),
				},
				## GuildDraws
				{
					"name" : "GuildDraws", "type" : "text", "x" : 58, "y" : 249, "text" : "Draw",
					"children" :
					(
						{
							"name" : "GuildDrawSlot",
							"type" : "image",
							"x" : 0,
							"y" : 14,
							"image" : SMALL_VALUE_FILE,
							"children" :
							(
								{"name" : "GuildDrawValue", "type":"text", "text":"00", "x":0, "y":0, "all_align":"center"},
							),
						},
					),
				},
			),
		},

		## Button
		{
			"name" : "OfferButton",
			"type" : "button",
			"x" : 127 + PLUS_LEFT_WITDH,
			"y" : 100,
			"default_image" : BUTTON_ROOT+"OfferButton00.sub",
			"over_image" : BUTTON_ROOT+"OfferButton01.sub",
			"down_image" : BUTTON_ROOT+"OfferButton02.sub",
		},

		{
			"name" : "GuildRenewalButton",
			"type" : "button",
			"x" : 127 + PLUS_LEFT_WITDH,
			"y" : 125,
			#"text" : "Ceh fejlesztes",
			"default_image" : "d:/ymir work/razor93/guild/large_button_01.tga",
			"over_image" : "d:/ymir work/razor93/guild/large_button_02.tga",
			"down_image" : "d:/ymir work/razor93/guild/large_button_03.tga",
		},

		###############################################################################################################

		## Guild Mark Title
		{
			"name":"Guild_Mark_Title_Bar", "type":"horizontalbar", "x":188 + PLUS_RIGHT_WITDH, "y":10, "width":167,
			"children" :
			(

				{ "name":"Guild_Mark_Title", "type":"text", "x":8, "y":3, "text":uiscriptlocale.GUILD_MARK, },

				## LargeGuildMark
				{
					"name" : "LargeGuildMarkSlot",
					"type" : "slotbar",
					"x" : 5,
					"y" : 24,
					"width" : 48+1,
					"height" : 36+1,
					"children" :
					(
						{
							"name" : "LargeGuildMark",
							"type" : "mark",
							"x" : 1,
							"y" : 1,
						},
					),
				},

			),
		},

		## UploadButton
		{
			"name" : "UploadGuildMarkButton",
			"type" : "button",
			"x" : 260 + PLUS_WITDH,
			"y" : 33,
			"default_image" : BUTTON_ROOT+"UploadGuildMarkButton00.sub",
			"over_image" : BUTTON_ROOT+"UploadGuildMarkButton01.sub",
			"down_image" : BUTTON_ROOT+"UploadGuildMarkButton02.sub",
		},
		{
			"name" : "UploadGuildSymbolButton",
			"type" : "button",
			"x" : 260 + PLUS_WITDH,
			"y" : 33 + 23,
			"default_image" : BUTTON_ROOT+"UploadGuildSymbolButton00.sub",
			"over_image" : BUTTON_ROOT+"UploadGuildSymbolButton01.sub",
			"down_image" : BUTTON_ROOT+"UploadGuildSymbolButton02.sub",
		},

		## Guild Mark Title
		{
			"name":"Guild_Mark_Title_Bar", "type":"horizontalbar", "x":188 + PLUS_RIGHT_WITDH, "y":85, "width":167,
			"children" :
			(

				{ "name":"Guild_Mark_Title", "type":"text", "x":8, "y":3, "text":uiscriptlocale.GUILD_INFO_ENEMY_GUILD, },

				{
					"name" : "EnemyGuildSlot1",
					"type" : "image",
					"x" : 4,
					"y" : 27 + 26*0,
					"image" : XLARGE_VALUE_FILE,
					"children" :
					(
						{"name" : "EnemyGuildName1", "type":"text", "text":uiscriptlocale.GUILD_INFO_ENEMY_GUILD_EMPTY, "x":0, "y":0, "all_align":"center"},
					),
				},
				{
					"name" : "EnemyGuildSlot2",
					"type" : "image",
					"x" : 4,
					"y" : 27 + 26*1,
					"image" : XLARGE_VALUE_FILE,
					"children" :
					(
						{"name" : "EnemyGuildName2", "type":"text", "text":uiscriptlocale.GUILD_INFO_ENEMY_GUILD_EMPTY, "x":0, "y":0, "all_align":"center"},
					),
				},
				{
					"name" : "EnemyGuildSlot3",
					"type" : "image",
					"x" : 4,
					"y" : 27 + 26*2,
					"image" : XLARGE_VALUE_FILE,
					"children" :
					(
						{"name" : "EnemyGuildName3", "type":"text", "text":uiscriptlocale.GUILD_INFO_ENEMY_GUILD_EMPTY, "x":0, "y":0, "all_align":"center"},
					),
				},
				{
					"name" : "EnemyGuildSlot4",
					"type" : "image",
					"x" : 4,
					"y" : 27 + 26*3,
					"image" : XLARGE_VALUE_FILE,
					"children" :
					(
						{"name" : "EnemyGuildName4", "type":"text", "text":uiscriptlocale.GUILD_INFO_ENEMY_GUILD_EMPTY, "x":0, "y":0, "all_align":"center"},
					),
				},
				{
					"name" : "EnemyGuildSlot5",
					"type" : "image",
					"x" : 4,
					"y" : 27 + 26*4,
					"image" : XLARGE_VALUE_FILE,
					"children" :
					(
						{"name" : "EnemyGuildName5", "type":"text", "text":uiscriptlocale.GUILD_INFO_ENEMY_GUILD_EMPTY, "x":0, "y":0, "all_align":"center"},
					),
				},
				{
					"name" : "EnemyGuildSlot6",
					"type" : "image",
					"x" : 4,
					"y" : 27 + 26*5,
					"image" : XLARGE_VALUE_FILE,
					"children" :
					(
						{"name" : "EnemyGuildName6", "type":"text", "text":uiscriptlocale.GUILD_INFO_ENEMY_GUILD_EMPTY, "x":0, "y":0, "all_align":"center"},
					),
				},

			),
		},

		## Guild Bonuses Title
		{
			"name":"Guild_Bonuses_Title_Bar", "type":"horizontalbar", "x":188 + PLUS_RIGHT_WITDH, "y":175, "width":167,
			"children" :
			(

				{ "name":"Guild_Stat_Title", "type":"text", "x":8, "y":3, "text":"Premi Vittorie", },
				## Guild1Bonusses
				{
					"name" : "GuildBonus1", "type" : "text", "x" : 3, "y" : 21, "text" : "0/5   +10% Esperienza",
					#"name" : "GuildBonus2", "type" : "text", "x" : 3, "y" : 51, "text" : "0/10  +5%  Forte contro mostri",
					#"name" : "GuildBonus3", "type" : "text", "x" : 3, "y" : 71, "text" : "0/15  +5%  Forte contro boss",
				},
				## Guild2Bonusses
				{
					#"name" : "GuildBonus1", "type" : "text", "x" : 3, "y" : 31, "text" : "0/5   +10% Esperienza",
					"name" : "GuildBonus2", "type" : "text", "x" : 3, "y" : 31, "text" : "0/10  +5%  Forte contro mostri",
					#"name" : "GuildBonus3", "type" : "text", "x" : 3, "y" : 71, "text" : "0/15  +5%  Forte contro boss",
				},
				## Guild3Bonusses
				{
					#"name" : "GuildBonus1", "type" : "text", "x" : 3, "y" : 31, "text" : "0/5   +10% Esperienza",
					#"name" : "GuildBonus2", "type" : "text", "x" : 3, "y" : 51, "text" : "0/10  +5%  Forte contro mostri",
					"name" : "GuildBonus3", "type" : "text", "x" : 3, "y" : 41, "text" : "0/15  +5%  Forte contro boss",
				},

			),
		},

		## CancelButtons
		{
			"name" : "EnemyGuildCancel1",
			"type" : "button",
			"x" : 310,
			"y" : 111 + 26*0,
			"text" : uiscriptlocale.CANCEL,
			"default_image" : "d:/ymir work/ui/public/small_button_01.sub",
			"over_image" : "d:/ymir work/ui/public/small_button_02.sub",
			"down_image" : "d:/ymir work/ui/public/small_button_03.sub",
		},
		{
			"name" : "EnemyGuildCancel2",
			"type" : "button",
			"x" : 310,
			"y" : 111 + 26*1,
			"text" : uiscriptlocale.CANCEL,
			"default_image" : "d:/ymir work/ui/public/small_button_01.sub",
			"over_image" : "d:/ymir work/ui/public/small_button_02.sub",
			"down_image" : "d:/ymir work/ui/public/small_button_03.sub",
		},
		{
			"name" : "EnemyGuildCancel3",
			"type" : "button",
			"x" : 310,
			"y" : 111 + 26*2,
			"text" : uiscriptlocale.CANCEL,
			"default_image" : "d:/ymir work/ui/public/small_button_01.sub",
			"over_image" : "d:/ymir work/ui/public/small_button_02.sub",
			"down_image" : "d:/ymir work/ui/public/small_button_03.sub",
		},
		{
			"name" : "EnemyGuildCancel4",
			"type" : "button",
			"x" : 310,
			"y" : 111 + 26*3,
			"text" : uiscriptlocale.CANCEL,
			"default_image" : "d:/ymir work/ui/public/small_button_01.sub",
			"over_image" : "d:/ymir work/ui/public/small_button_02.sub",
			"down_image" : "d:/ymir work/ui/public/small_button_03.sub",
		},
		{
			"name" : "EnemyGuildCancel5",
			"type" : "button",
			"x" : 310,
			"y" : 111 + 26*4,
			"text" : uiscriptlocale.CANCEL,
			"default_image" : "d:/ymir work/ui/public/small_button_01.sub",
			"over_image" : "d:/ymir work/ui/public/small_button_02.sub",
			"down_image" : "d:/ymir work/ui/public/small_button_03.sub",
		},
		{
			"name" : "EnemyGuildCancel6",
			"type" : "button",
			"x" : 310,
			"y" : 111 + 26*5,
			"text" : uiscriptlocale.CANCEL,
			"default_image" : "d:/ymir work/ui/public/small_button_01.sub",
			"over_image" : "d:/ymir work/ui/public/small_button_02.sub",
			"down_image" : "d:/ymir work/ui/public/small_button_03.sub",
		},

		## DeclareWar
		{
			"name" : "DeclareWarButton",
			"type" : "button",
			"x" : 250 + 15 + PLUS_WITDH,
			"y" : 276,
			"default_image" : BUTTON_ROOT+"DeclareWarButton00.sub",
			"over_image" : BUTTON_ROOT+"DeclareWarButton01.sub",
			"down_image" : BUTTON_ROOT+"DeclareWarButton02.sub",
		},

	),
}
