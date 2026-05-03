import uiscriptlocale

MAINBOARD_WIDTH = 245
MAINBOARD_HEIGHT = 210
MAINBOARD_X = (SCREEN_WIDTH/2) - (MAINBOARD_WIDTH/2)
MAINBOARD_Y = (SCREEN_HEIGHT/2) - (MAINBOARD_HEIGHT/2)

BUTTON_STEP = 55

ROOT_PATH = "d:/ymir work/ui/public/"

window = {
	"name" : "BattleRoyaleSelectCharacter",
	"style" : ("movable", "float",),
	
	"x" : MAINBOARD_X,
	"y" : MAINBOARD_Y,	

	"width" : MAINBOARD_WIDTH,
	"height" : MAINBOARD_HEIGHT,
	
	"children" :
	(
		## MainBoard
		{
			"name" : "board",
			"type" : "board_with_titlebar",
			"style" : ("attach",),

			"x" : 0,
			"y" : 0,

			"width" : MAINBOARD_WIDTH,
			"height" : MAINBOARD_HEIGHT,
			
			"title" : uiscriptlocale.BATTLE_ROYALE_SELECT_TITLE,
			# "title" : "Select your Battle Royale character",
			
			"children" :
			(
				
				#Character controls
				{
					"name" : "CharControls",
					"type" : "window",

					"x" : 15,
					"y" : 30,

					"width" : MAINBOARD_WIDTH - 15,
					"height" : MAINBOARD_HEIGHT - 30,
					

					"children" :
					(

						#Chars
						{
							"name" : "CharButton1",
							"type" : "radio_button",

							"x" : 0,
							"y" : 0,

							"text" : "",

							"default_image" : "d:/ymir work/ui/game/battle_royale/warrior_m_03.tga",
							"over_image" : "d:/ymir work/ui/game/battle_royale/warrior_m_01.tga",
							"down_image" : "d:/ymir work/ui/game/battle_royale/warrior_m_02.tga",
						},
						{
							"name" : "CharButton2",
							"type" : "radio_button",

							"x" : 0,
							"y" : BUTTON_STEP,

							"text" : "",

							"default_image" : "d:/ymir work/ui/game/battle_royale/warrior_f_03.tga",
							"over_image" : "d:/ymir work/ui/game/battle_royale/warrior_f_01.tga",
							"down_image" : "d:/ymir work/ui/game/battle_royale/warrior_f_02.tga",
						},
						
						{
							"name" : "CharButton3",
							"type" : "radio_button",

							"x" : BUTTON_STEP*1,
							"y" : 0,

							"text" : "",

							"default_image" : "d:/ymir work/ui/game/battle_royale/sura_m_03.tga",
							"over_image" : "d:/ymir work/ui/game/battle_royale/sura_m_01.tga",
							"down_image" : "d:/ymir work/ui/game/battle_royale/sura_m_02.tga",
						},
						{
							"name" : "CharButton4",
							"type" : "radio_button",

							"x" : BUTTON_STEP*1,
							"y" : BUTTON_STEP,

							"text" : "",

							"default_image" : "d:/ymir work/ui/game/battle_royale/sura_f_03.tga",
							"over_image" : "d:/ymir work/ui/game/battle_royale/sura_f_01.tga",
							"down_image" : "d:/ymir work/ui/game/battle_royale/sura_f_02.tga",
						},
						
						{
							"name" : "CharButton5",
							"type" : "radio_button",

							"x" : BUTTON_STEP*2,
							"y" : 0,

							"text" : "",

							"default_image" : "d:/ymir work/ui/game/battle_royale/ninja_m_03.tga",
							"over_image" : "d:/ymir work/ui/game/battle_royale/ninja_m_01.tga",
							"down_image" : "d:/ymir work/ui/game/battle_royale/ninja_m_02.tga",
						},
						{
							"name" : "CharButton6",
							"type" : "radio_button",

							"x" : BUTTON_STEP*2,
							"y" : BUTTON_STEP,

							"text" : "",

							"default_image" : "d:/ymir work/ui/game/battle_royale/ninja_f_03.tga",
							"over_image" : "d:/ymir work/ui/game/battle_royale/ninja_f_01.tga",
							"down_image" : "d:/ymir work/ui/game/battle_royale/ninja_f_02.tga",
						},
						
						{
							"name" : "CharButton7",
							"type" : "radio_button",

							"x" : BUTTON_STEP*3,
							"y" : 0,

							"text" : "",

							"default_image" : "d:/ymir work/ui/game/battle_royale/shaman_m_03.tga",
							"over_image" : "d:/ymir work/ui/game/battle_royale/shaman_m_01.tga",
							"down_image" : "d:/ymir work/ui/game/battle_royale/shaman_m_02.tga",
						},
						{
							"name" : "CharButton8",
							"type" : "radio_button",

							"x" : BUTTON_STEP*3,
							"y" : BUTTON_STEP,

							"text" : "",

							"default_image" : "d:/ymir work/ui/game/battle_royale/shaman_f_03.tga",
							"over_image" : "d:/ymir work/ui/game/battle_royale/shaman_f_01.tga",
							"down_image" : "d:/ymir work/ui/game/battle_royale/shaman_f_02.tga",
						},
						
						
						{
							"name" : "skillGroup1",
							"type" : "radio_button",

							"x" : 5+10,
							"y" : 110,

							"text" : "",

							"default_image" : ROOT_PATH + "Large_Button_01.sub",
							"over_image" : ROOT_PATH + "Large_Button_02.sub",
							"down_image" : ROOT_PATH + "Large_Button_03.sub",
						},
						
						{
							"name" : "skillGroup2",
							"type" : "radio_button",

							"x" : 5 + 112 -10,
							"y" : 110,

							"text" : "",

							"default_image" : ROOT_PATH + "Large_Button_01.sub",
							"over_image" : ROOT_PATH + "Large_Button_02.sub",
							"down_image" : ROOT_PATH + "Large_Button_03.sub",
						},
						
						#Finish
						{
							"name" : "confirm",
							"type" : "button",

							"x" : 5,
							"y" : 145,

							"text" : uiscriptlocale.CONFIRM,
							# "text" : "Confirm",

							"default_image" : ROOT_PATH + "Large_Button_01.sub",
							"over_image" : ROOT_PATH + "Large_Button_02.sub",
							"down_image" : ROOT_PATH + "Large_Button_03.sub",
						},
						
						{
							"name" : "cancel",
							"type" : "button",

							"x" : 5 + 112,
							"y" : 145,

							"text" : uiscriptlocale.CANCEL,

							"default_image" : ROOT_PATH + "Large_Button_01.sub",
							"over_image" : ROOT_PATH + "Large_Button_02.sub",
							"down_image" : ROOT_PATH + "Large_Button_03.sub",
						},
						
					),
				},
				

			),
		}, ## MainBoard End
	),
}