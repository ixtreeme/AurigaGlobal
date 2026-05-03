#(C) 2019 Owsap Productions

import uiscriptlocale

MAINBOARD_WIDTH = 535 + 4
MAINBOARD_HEIGHT = 431

LEFTBOARD_WIDTH = 291
LEFTBOARD_HEIGHT = 382
LEFTBOARD_X = 13
LEFTBOARD_Y = 36

RIGHTBOARD_WIDTH = 196
RIGHTBOARD_HEIGHT = 382
RIGHTBOARD_X = 328 + 1
RIGHTBOARD_Y = 36

window = {
	"name" : "DungeonInfoWindow",
	"style" : ("movable", "float",),

	"x" : 0,
	"y" : 0,

	"width" : MAINBOARD_WIDTH,
	"height" : MAINBOARD_HEIGHT,

	"children" :
	(
		#######################
		##   DUNGEON_BOARD   ##
		{
			"name" : "DungeonBoard",
			"type" : "board",
			"style" : ("attach", "ltr"),

			"x" : 0,
			"y" : 0,

			"width" : MAINBOARD_WIDTH,
			"height" : MAINBOARD_HEIGHT,

			"children" :
			(
				################################
				##   DUNGEON_BOARD_TITLEBAR   ##
				{
					"name" : "DungeonBoardTitleBar",
					"type" : "titlebar",
					"style" : ("attach",),

					"x" : 6,
					"y" : 7,

					"width" : MAINBOARD_WIDTH - 13,

					"children" :
					(
						{
							"name" : "TitleName",
							"type" : "text",

							"x" : 0,
							"y" : -2,

							"text": uiscriptlocale.DUNGEON_INFO,
							"all_align":"center"
						},
					),
				},
				##   DUNGEON_BOARD_TITLEBAR   ##
				################################

				##############################
				##   DUNGEON_BUTTON_BOARD   ##
				{
					"name" : "DungeonButtonThinBoard",
					"type" : "thinboard",

					"x" : LEFTBOARD_X - 4,
					"y" : LEFTBOARD_Y - 4,

					"width" : LEFTBOARD_WIDTH + 8,
					"height" : LEFTBOARD_HEIGHT + 8,
				},
				{
					"name" : "DungeonButtonBoard",
					"type" : "thinboard_circle",

					"x" : LEFTBOARD_X,
					"y" : LEFTBOARD_Y,

					"width" : LEFTBOARD_WIDTH,
					"height" : LEFTBOARD_HEIGHT,

					"children" : 
					(
						## logininfo.py
					),
				},
				##   DUNGEON_BUTTON_BOARD   ##
				##############################

				########################################
				##   DUNGEON_BUTTON_BOARD_SCROLLBAR   ##
				{
					"name" : "ScrollBar",
					"type" : "scrollbar",

					"x" : LEFTBOARD_WIDTH + 18,
					"y" : 32,

					"size" : LEFTBOARD_HEIGHT + 9,
				},
				##   DUNGEON_BUTTON_BOARD_SCROLLBAR   ##
				########################################

				############################
				##   DUNGEON_INFO_BOARD   ##
				{
					"name" : "DungeonInfoThinBoard",
					"type" : "thinboard",

					"x" : RIGHTBOARD_X - 4,
					"y" : RIGHTBOARD_Y - 4,

					"width" : RIGHTBOARD_WIDTH + 8,
					"height" : RIGHTBOARD_HEIGHT + 8,
				},
				{
					"name" : "DungeonInfoBoard",
					"type" : "image",

					"x" : RIGHTBOARD_X,
					"y" : RIGHTBOARD_Y,

					#"width" : RIGHTBOARD_WIDTH,
					#"height" : RIGHTBOARD_HEIGHT,

					"image" : "d:/ymir work/ui/game/dungeon_info/background/0.tga",

					"children" : 
					(
						## Title Bar
						{
							"name" : "DungeonInfoTitleBar",
							"type" : "image",

							"x" : 332 - RIGHTBOARD_X,
							"y" : 39 - RIGHTBOARD_Y,

							"image" : "d:/ymir work/ui/game/myshop_deco/model_view_title.sub",

							"children" :
							(
								{
									"name" : "DungeonInfoName",
									"type" : "text",

									"x" : 0,
									"y" : 0,

									"text" : uiscriptlocale.DUNGEON_INFO_DUNGEON,
									"color" : 0xFFFEE3AE,
									"fontname" : "Tahoma:17",
									"all_align" : "center"
								},
							),
						},

						## Dungeon Type
						{
							"name" : "DungeonInfoType",
							"type" : "text",

							"x" : 10,
							"y" : 30 * 1,

							"text" : uiscriptlocale.DUNGEON_INFO_TYPE,
						},

						## Dungeon Organization
						{
							"name" : "DungeonInfoOrganization",
							"type" : "text",

							"x" : 10,
							"y" : 30 + 20 * 1,

							"text" : uiscriptlocale.DUNGEON_INFO_ORGANIZATION,
						},

						## Dungeon Level Limit
						{
							"name" : "DungeonInfoLevelLimit",
							"type" : "text",

							"x" : 10,
							"y" : 30 + 20 * 2,

							"text" : uiscriptlocale.DUNGEON_INFO_LEVEL_LIMIT,
						},

						## Dungeon Party Members
						{
							"name" : "DungeonInfoPartyMembers",
							"type" : "text",

							"x" : 10,
							"y" : 30 + 20 * 3,

							"text" : uiscriptlocale.DUNGEON_INFO_PARTY_MEMBERS,
						},

						## Dungeon Cooldown
						{
							"name" : "DungeonInfoCooldown",
							"type" : "text",

							"x" : 10,
							"y" : 30 + 20 * 4,

							"text" : uiscriptlocale.DUNGEON_INFO_COOLDOWN,
						},

						## Dungeon Duration
						{
							"name" : "DungeonInfoDuration",
							"type" : "text",

							"x" : 10,
							"y" : 30 + 20 * 5,

							"text" : uiscriptlocale.DUNGEON_INFO_DURATION,
						},

						## Dungeon Entrance
						{
							"name" : "DungeonInfoEntrance",
							"type" : "text",

							"x" : 10,
							"y" : 30 + 20 * 6,

							"text" : uiscriptlocale.DUNGEON_INFO_ENTRANCE,
						},

						## Dungeon Strength Bonus
						{
							"name" : "DungeonInfoStrengthBonus",
							"type" : "text",

							"x" : 10,
							"y" : 30 + 20 * 7,
							"text" : uiscriptlocale.DUNGEON_INFO_STRENGTH,
						},

						## Dungeon Resistance Bonus
						{
							"name" : "DungeonInfoResistanceBonus",
							"type" : "text",

							"x" : 10,
							"y" : 30 + 20 * 8,
							"text" : uiscriptlocale.DUNGEON_INFO_RESISTANCE,
						},

						## Dungeon Personal Stats
						{
							"name" : "DungeonInfoPersonal",
							"type" : "image",

							"x" : -5,
							"y" : RIGHTBOARD_HEIGHT / 2 + 15,

							"image" : "d:/ymir work/ui/pattern/seperator.tga",

							"children" :
							(
								# Personal Stats
								{
									"name" : "DungeonInfoPersonalStats",
									"type" : "text",

									"x" : 0,
									"y" : -5,

									"text" : uiscriptlocale.DUNGEON_INFO_PERSONAL_STATS,
									"color" : 0xFFFEE3AE,
									"all_align" : "center",
								},
							),
						},

						## Personal Stats - Total Finished
						{
							"name" : "DungeonInfoTotalFinished",
							"type" : "text",

							"x" : 10,
							"y" : 30 + 20 * 9 + 30,

							"text" : uiscriptlocale.DUNGEON_INFO_TOTAL_FINISHED,
						},

						## Personal Stats - Fastest Time
						{
							"name" : "DungeonInfoFastestTime",
							"type" : "text",

							"x" : 10,
							"y" : 30 + 20 * 10 + 30,

							"text" : uiscriptlocale.DUNGEON_INFO_FASTEST_TIME,
						},

						## Personal Stats - Highest DMG
						{
							"name" : "DungeonInfoHighestDamage",
							"type" : "text",

							"x" : 10,
							"y" : 30 + 20 * 11 + 30,

							"text" : uiscriptlocale.DUNGEON_INFO_HIGHEST_DAMAGE,
						},

						## Special Item Slot BG
						{
							"name" : "DungeonInfoItem",
							"type" : "expanded_image",
							"style" : ("attach",),

							"x" : RIGHTBOARD_WIDTH - 50,
							"y" : RIGHTBOARD_HEIGHT - 50,

							"image" : "d:/ymir work/ui/minigame/fish_event/fish_special_slot.sub",

							"children" :
							(
								## Special Item Slot
								{
									"name" : "DungeonInfoItemSlot",
									"type" : "slot",

									"x" : 7,
									"y" : 7,

									"width" : 32,
									"height" : 32,

									"image" : "d:/ymir work/ui/pet/skill_button/skill_enable_button.sub",

									"slot" : (
										{ "index" : 0, "x" : 0, "y" : 0, "width" : 32, "height" : 32 },
									),
								},
							),
						},

						## Rank buttons
						{
							"name" : "DungeonRank1Button",
							"type" : "button",

							"x" : RIGHTBOARD_WIDTH /2 - 95,
							"y" : RIGHTBOARD_HEIGHT - 80,

							"tooltip_text" : uiscriptlocale.DUNGEON_RANKING_TYPE1,
							"tooltip_x" : 0,
							"tooltip_y" : -13,

							"default_image" : "d:/ymir work/ui/game/dungeon_info/button/rank_button00.sub",
							"over_image" : "d:/ymir work/ui/game/dungeon_info/button/rank_button01.sub",
							"down_image" : "d:/ymir work/ui/game/dungeon_info/button/rank_button02.sub",
						},
						{
							"name" : "DungeonRank2Button",
							"type" : "button",

							"x" : RIGHTBOARD_WIDTH /2 - 95 + 65,
							"y" : RIGHTBOARD_HEIGHT - 80,

							"tooltip_text" : uiscriptlocale.DUNGEON_RANKING_TYPE2,
							"tooltip_x" : 0,
							"tooltip_y" : -13,

							"default_image" : "d:/ymir work/ui/game/dungeon_info/button/rank_button10.sub",
							"over_image" : "d:/ymir work/ui/game/dungeon_info/button/rank_button11.sub",
							"down_image" : "d:/ymir work/ui/game/dungeon_info/button/rank_button12.sub",
						},
						{
							"name" : "DungeonRank3Button",
							"type" : "button",

							"x" : RIGHTBOARD_WIDTH /2 - 95 + 65 + 65,
							"y" : RIGHTBOARD_HEIGHT - 80,

							"tooltip_text" : uiscriptlocale.DUNGEON_RANKING_TYPE3,
							"tooltip_x" : 0,
							"tooltip_y" : -13,

							"default_image" : "d:/ymir work/ui/game/dungeon_info/button/rank_button20.sub",
							"over_image" : "d:/ymir work/ui/game/dungeon_info/button/rank_button21.sub",
							"down_image" : "d:/ymir work/ui/game/dungeon_info/button/rank_button22.sub",
						},

						# Teleport & Close buttons
						{
							"name" : "DungeonInfoTeleportButton",
							"type" : "button",

							"x" : RIGHTBOARD_WIDTH /2 - 85,
							"y" : RIGHTBOARD_HEIGHT - 25 - 25,

							"text" : uiscriptlocale.DUNGEON_INFO_TELEPORT,
							"default_image" : "d:/ymir work/ui/game/dungeon_info/button/normal_button_default.sub",
							"over_image" : "d:/ymir work/ui/game/dungeon_info/button/normal_button_over.sub",
							"down_image" : "d:/ymir work/ui/game/dungeon_info/button/normal_button_down.sub",
						},
						{
							"name" : "CloseDungeonBoard",
							"type" : "button",

							"x" : RIGHTBOARD_WIDTH /2 - 85,
							"y" : RIGHTBOARD_HEIGHT - 25,

							"text" : uiscriptlocale.DUNGEON_INFO_CLOSE,
							"default_image" : "d:/ymir work/ui/game/dungeon_info/button/normal_button_default.sub",
							"over_image" : "d:/ymir work/ui/game/dungeon_info/button/normal_button_over.sub",
							"down_image" : "d:/ymir work/ui/game/dungeon_info/button/normal_button_down.sub",
						},
					),
				},
				##   DUNGEON_INFO_BOARD   ##
				############################
			),
		},
		##   DUNGEON_BOARD   ##
		#######################
	),
}