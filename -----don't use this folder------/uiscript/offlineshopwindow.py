import uiscriptlocale
import localeinfo

WINDOW_WIDTH	= 627
WINDOW_HEIGHT	= 572

SEARCH_AND_FILTER_BACKGROUND = "offlineshop/searchfilter/base_image.png"
SAFEBOX_WITHDRAW_BUTTON	= "offlineshop/safebox/withdrawyang_%s.png"

if ENABLE_CHEQUE_SYSTEM:
	SAFEBOX_WITHDRAW_BUTTON = "offlineshop/safebox/withdrawyang_%s_cheque.png"




if ENABLE_CHEQUE_SYSTEM:
	SAFEBOX_CHILDREN = (
		{
			"name": "BackgroundShopSafeboxPage",
			"type": "image",

			"x": 0, "y": 0,

			"image": "offlineshop/safebox/safebox_base_image.png",
		},
		{
			"name" : "ShopSafeboxWithdrawYangButton",
			"type" : "button",

			"x" : 447-205 - 38,
			"y" : 16-4,

			"default_image" :  SAFEBOX_WITHDRAW_BUTTON%"default",
			"over_image" 	:  SAFEBOX_WITHDRAW_BUTTON%"over",
			"down_image" 	:  SAFEBOX_WITHDRAW_BUTTON%"down",
		},

		{
			"name" : "ShopSafeboxValuteText",
			"type" : "text",

			"x" : 468-154 - 38,
			"y" : 22-8,

			"text_horizontal_align" : "center",
			"text" : "000000",
		},
		
		{
			"name" : "ShopSafeboxValuteTextCheque",
			"type" : "text",

			"x" : 468-154 - 38 + 115,
			"y" : 22-8,

			"text_horizontal_align" : "center",
			"text" : "000000",
		},
		{
			"name" : "3mybtnslot1",
			"type" : "bar",
			"style" : ("attach",),
			"x" : 3,
			"y" : 523,
			"width" : 66,
			"height" : 12,
			"color" : 0x00000000,
			"children" :
			(
				{
					"name" : "3mybtntext1",
					"type" : "text",
					"x" : 0,
					"y" : -1,
					"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN1,
					"horizontal_align" : "center",
					"text_horizontal_align" : "center"
				},
			)
		},
		{
			"name" : "3mybtnslot2",
			"type" : "bar",
			"style" : ("attach",),
			"x" : 72,
			"y" : 523,
			"width" : 66,
			"height" : 12,
			"color" : 0x00000000,
			"children" :
			(
				{
					"name" : "3mybtntext2",
					"type" : "text",
					"x" : 0,
					"y" : -1,
					"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN2,
					"horizontal_align" : "center",
					"text_horizontal_align" : "center"
				},
			)
		},
		{
			"name" : "3mybtnslot3",
			"type" : "bar",
			"style" : ("attach",),
			"x" : 140,
			"y" : 523,
			"width" : 66,
			"height" : 12,
			"color" : 0x00000000,
			"children" :
			(
				{
					"name" : "3mybtntext3",
					"type" : "text",
					"x" : 0,
					"y" : -1,
					"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN3,
					"color" : 0xffcca714,
					"horizontal_align" : "center",
					"text_horizontal_align" : "center"
				},
			)
		},
		{
			"name" : "3mybtnslot4",
			"type" : "bar",
			"style" : ("attach",),
			"x" : 209,
			"y" : 523,
			"width" : 66,
			"height" : 12,
			"color" : 0x00000000,
			"children" :
			(
				{
					"name" : "3mybtntext4",
					"type" : "text",
					"x" : 0,
					"y" : -1,
					"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN4,
					"horizontal_align" : "center",
					"text_horizontal_align" : "center"
				},
			)
		},
		{
			"name" : "3mybtnslot5",
			"type" : "bar",
			"style" : ("attach",),
			"x" : 278,
			"y" : 523,
			"width" : 66,
			"height" : 12,
			"color" : 0x00000000,
			"children" :
			(
				{
					"name" : "3mybtntext5",
					"type" : "text",
					"x" : 0,
					"y" : -1,
					"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN5,
					"horizontal_align" : "center",
					"text_horizontal_align" : "center"
				},
			)
		},
		{
			"name" : "3mybtnslot6",
			"type" : "bar",
			"style" : ("attach",),
			"x" : 347,
			"y" : 523,
			"width" : 66,
			"height" : 12,
			"color" : 0x00000000,
			"children" :
			(
				{
					"name" : "3mybtntext6",
					"type" : "text",
					"x" : 0,
					"y" : -1,
					"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN6,
					"horizontal_align" : "center",
					"text_horizontal_align" : "center"
				},
			)
		},
		{
			"name" : "3mybtnslot7",
			"type" : "bar",
			"style" : ("attach",),
			"x" : 415,
			"y" : 523,
			"width" : 66,
			"height" : 12,
			"color" : 0x00000000,
			"children" :
			(
				{
					"name" : "3mybtntext7",
					"type" : "text",
					"x" : 0,
					"y" : -1,
					"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN7,
					"horizontal_align" : "center",
					"text_horizontal_align" : "center"
				},
			)
		},
		{
			"name" : "3mybtnslot8",
			"type" : "bar",
			"style" : ("attach",),
			"x" : 484,
			"y" : 523,
			"width" : 66,
			"height" : 12,
			"color" : 0x00000000,
			"children" :
			(
				{
					"name" : "3mybtnslot8",
					"type" : "text",
					"x" : 0,
					"y" : -1,
					"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN8,
					"horizontal_align" : "center",
					"text_horizontal_align" : "center"
				},
			)
		},
		{
			"name" : "3mybtnslot9",
			"type" : "bar",
			"style" : ("attach",),
			"x" : 552,
			"y" : 523,
			"width" : 66,
			"height" : 12,
			"color" : 0x00000000,
			"children" :
			(
				{
					"name" : "3mybtnslot9",
					"type" : "text",
					"x" : 0,
					"y" : -1,
					"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN9,
					"horizontal_align" : "center",
					"text_horizontal_align" : "center"
				},
			)
		},
	)

else:
	SAFEBOX_CHILDREN = (
		{
			"name": "BackgroundShopSafeboxPage",
			"type": "image",

			"x": 0, "y": 0,

			"image": "offlineshop/safebox/safebox_base_image.png",
		},
		{
			"name" : "ShopSafeboxWithdrawYangButton",
			"type" : "button",

			"x" : 447-205,
			"y" : 16-4,

			"default_image" :  SAFEBOX_WITHDRAW_BUTTON%"default",
			"over_image" 	:  SAFEBOX_WITHDRAW_BUTTON%"over",
			"down_image" 	:  SAFEBOX_WITHDRAW_BUTTON%"down",
		},

		{
			"name" : "ShopSafeboxValuteText",
			"type" : "text",

			"x" : 468-154,
			"y" : 22-8,

			"text_horizontal_align" : "center",
			"text" : "000000",
		},
		{
			"name" : "3mybtnslot1",
			"type" : "bar",
			"style" : ("attach",),
			"x" : 3,
			"y" : 523,
			"width" : 66,
			"height" : 12,
			"color" : 0x00000000,
			"children" :
			(
				{
					"name" : "3mybtntext1",
					"type" : "text",
					"x" : 0,
					"y" : -1,
					"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN1,
					"horizontal_align" : "center",
					"text_horizontal_align" : "center"
				},
			)
		},
		{
			"name" : "3mybtnslot2",
			"type" : "bar",
			"style" : ("attach",),
			"x" : 72,
			"y" : 523,
			"width" : 66,
			"height" : 12,
			"color" : 0x00000000,
			"children" :
			(
				{
					"name" : "3mybtntext2",
					"type" : "text",
					"x" : 0,
					"y" : -1,
					"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN2,
					"horizontal_align" : "center",
					"text_horizontal_align" : "center"
				},
			)
		},
		{
			"name" : "3mybtnslot3",
			"type" : "bar",
			"style" : ("attach",),
			"x" : 140,
			"y" : 523,
			"width" : 66,
			"height" : 12,
			"color" : 0x00000000,
			"children" :
			(
				{
					"name" : "3mybtntext3",
					"type" : "text",
					"x" : 0,
					"y" : -1,
					"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN3,
					"color" : 0xffcca714,
					"horizontal_align" : "center",
					"text_horizontal_align" : "center"
				},
			)
		},
		{
			"name" : "3mybtnslot4",
			"type" : "bar",
			"style" : ("attach",),
			"x" : 209,
			"y" : 523,
			"width" : 66,
			"height" : 12,
			"color" : 0x00000000,
			"children" :
			(
				{
					"name" : "3mybtntext4",
					"type" : "text",
					"x" : 0,
					"y" : -1,
					"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN4,
					"horizontal_align" : "center",
					"text_horizontal_align" : "center"
				},
			)
		},
		{
			"name" : "3mybtnslot5",
			"type" : "bar",
			"style" : ("attach",),
			"x" : 278,
			"y" : 523,
			"width" : 66,
			"height" : 12,
			"color" : 0x00000000,
			"children" :
			(
				{
					"name" : "3mybtntext5",
					"type" : "text",
					"x" : 0,
					"y" : -1,
					"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN5,
					"horizontal_align" : "center",
					"text_horizontal_align" : "center"
				},
			)
		},
		{
			"name" : "3mybtnslot6",
			"type" : "bar",
			"style" : ("attach",),
			"x" : 347,
			"y" : 523,
			"width" : 66,
			"height" : 12,
			"color" : 0x00000000,
			"children" :
			(
				{
					"name" : "3mybtntext6",
					"type" : "text",
					"x" : 0,
					"y" : -1,
					"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN6,
					"horizontal_align" : "center",
					"text_horizontal_align" : "center"
				},
			)
		},
		{
			"name" : "3mybtnslot7",
			"type" : "bar",
			"style" : ("attach",),
			"x" : 415,
			"y" : 523,
			"width" : 66,
			"height" : 12,
			"color" : 0x00000000,
			"children" :
			(
				{
					"name" : "3mybtntext7",
					"type" : "text",
					"x" : 0,
					"y" : -1,
					"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN7,
					"horizontal_align" : "center",
					"text_horizontal_align" : "center"
				},
			)
		},
		{
			"name" : "3mybtnslot8",
			"type" : "bar",
			"style" : ("attach",),
			"x" : 484,
			"y" : 523,
			"width" : 66,
			"height" : 12,
			"color" : 0x00000000,
			"children" :
			(
				{
					"name" : "3mybtnslot8",
					"type" : "text",
					"x" : 0,
					"y" : -1,
					"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN8,
					"horizontal_align" : "center",
					"text_horizontal_align" : "center"
				},
			)
		},
		{
			"name" : "3mybtnslot9",
			"type" : "bar",
			"style" : ("attach",),
			"x" : 552,
			"y" : 523,
			"width" : 66,
			"height" : 12,
			"color" : 0x00000000,
			"children" :
			(
				{
					"name" : "3mybtnslot9",
					"type" : "text",
					"x" : 0,
					"y" : -1,
					"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN9,
					"horizontal_align" : "center",
					"text_horizontal_align" : "center"
				},
			)
		},
	)






window = {

	"name" : "OfflineshopBoard",
	"style" : ("movable", "float",),

	"x" : SCREEN_WIDTH/2  - WINDOW_WIDTH/2,
	"y" : SCREEN_HEIGHT/2  - WINDOW_HEIGHT/2,

	"width" : WINDOW_WIDTH,
	"height" : WINDOW_HEIGHT,

	"children" :
	(
		{
			"name" : "MainBoard",
			"type" : "board",
			
			"style" : ("attach",),
			
			"x" : 0,
			"y" : 0,
			
			"width" 	: WINDOW_WIDTH,
			"height" 	: WINDOW_HEIGHT,
			
			"children" : 
			(
				{
					"name" : "TitleBar",
					"type" : "titlebar",
					
					"x" : 8,
					"y" : 7,
					
					"width"  : WINDOW_WIDTH - 16,
					"color"  : "red",
					
					"children" :
					(
						{ "name":"TitleName", "type":"text", "x":0, "y":-1, "text":"Offline Shop", "all_align":"center" },
					),
				},





				#MyShop_noShop
				{
					"name" : "MyShopBoardNoShop",
					"type" : "window",
					
					"width" :  622,  "height" :  544,
					
					"x" : 3, "y" : 28,
					
					"children":
					(
						{
							"name" : "BackGroundCreate",
							"type" : "image",
							
							"x" : 0, "y" : 0,
							"image": "offlineshop/createshop/base_image_1.png",
							
						},
						{
							"name" : "DecreaseStyleButton",
							"type" : "button",
							"x" : 622 - 56,
							"y" : 54,
							"default_image" : "offlineshop/scrollbar/horizontal/button2_default.png",
							"over_image" : "offlineshop/scrollbar/horizontal/button2_over.png",
							"down_image" : "offlineshop/scrollbar/horizontal/button2_down.png",
						},
						
						
						{
							"name" : "IncreaseStyleButton",
							"type" : "button",
							"x" : 622 - 178,
							"y" : 54,
							"default_image" : "offlineshop/scrollbar/horizontal/button1_default.png",
							"over_image" : "offlineshop/scrollbar/horizontal/button1_over.png",
							"down_image" : "offlineshop/scrollbar/horizontal/button1_down.png",
						},
						# {
						# 	"name" : "HowToInsertItems",
						# 	"type" : "image",
						#
						# 	"x" : 150, "y" : 300,
						#
						# 	"image" 			: "offlineshop/createshop/howto_insert_items.png",
						#
						# },
					
						{
							"name" : "ShopNameInput",
							"type" : "editline",
							
							"width" : 217 , "height" : 15 ,
							
							"input_limit" : 35,
							"x" : 205, "y" : 34,
						},
						
						
						
						#count texts
						{
							"name" : "DaysCountText",
							"type" : "text",
							
							"text" : "0",
							"text_horizontal_align" : "center",
							"x" : 255, "y" :89,
						},
						
						
						
						{
							"name" : "HoursCountText",
							"type" : "text",
							
							"text" : "0",
							"text_horizontal_align" : "center",
							"x" : 367, "y" : 89,
						},
						
						
						
						#increase-reduce buttons
						{
							"name" : "IncreaseDaysButton",
							"type" : "button",
							

							"x" : 291-10,
							"y" : 91,

							"default_image" : "offlineshop/scrollbar/horizontal/button2_default.png",
							"over_image" 	: "offlineshop/scrollbar/horizontal/button2_over.png",
							"down_image" 	: "offlineshop/scrollbar/horizontal/button2_down.png",
						},
						{
							"name" : "DecreaseDaysButton",
							"type" : "button",
							
							"x" : 218,
							"y" : 91,
							
							"default_image" : "offlineshop/scrollbar/horizontal/button1_default.png",
							"over_image" 	: "offlineshop/scrollbar/horizontal/button1_over.png",
							"down_image" 	: "offlineshop/scrollbar/horizontal/button1_down.png",
						},
						
						
						
						{
							"name" : "IncreaseHoursButton",
							"type" : "button",
							
							"x" : 400-8,
							"y" : 91,
							
							"default_image" : "offlineshop/scrollbar/horizontal/button2_default.png",
							"over_image" 	: "offlineshop/scrollbar/horizontal/button2_over.png",
							"down_image" 	: "offlineshop/scrollbar/horizontal/button2_down.png",
						},
						
						
						{
							"name" : "DecreaseHoursButton",
							"type" : "button",
							
							"x" : 330-3,
							"y" : 91,
							
							"default_image" : "offlineshop/scrollbar/horizontal/button1_default.png",
							"over_image" 	: "offlineshop/scrollbar/horizontal/button1_over.png",
							"down_image" 	: "offlineshop/scrollbar/horizontal/button1_down.png",
						},
						
						
						
						
						#next button
						{
							"name" : "CreateShopButton",
							"type" : "button",
							
							"x" : 249 + 40,
							"y" : 482 - 8,
							
							"default_image" : "d:/ymir work/ui/public/Small_Button_01.sub",
							"over_image" : "d:/ymir work/ui/public/Small_Button_02.sub",
							"down_image" : "d:/ymir work/ui/public/Small_Button_03.sub",

							"text" : localeinfo.OFFLINESHOP_SCRIPTFILE_CREATE_TEXT,
						},
						{
							"name" : "1myslot1",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 201,
							"y" : 18,
							"width" : 220,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "1mytext1",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT1,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "1myslot2",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 201,
							"y" : 58,
							"width" : 220,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "1mytext2",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT2,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "1myslot3",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 201,
							"y" : 458,
							"width" : 220,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "1mytext3",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT3,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "1myslot4",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 236,
							"y" : 75,
							"width" : 39,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "1mytext4",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT4,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "1myslot6",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 346,
							"y" : 75,
							"width" : 39,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "1mytext6",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT5,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "1mybtnslot1",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 3,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "1mybtntext1",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN1,
									"color" : 0xffcca714,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "1mybtnslot2",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 72,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "1mybtntext2",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN2,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "1mybtnslot3",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 140,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "1mybtntext3",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN3,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "1mybtnslot4",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 209,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "1mybtntext4",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN4,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "1mybtnslot5",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 278,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "1mybtntext5",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN5,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "1mybtnslot6",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 347,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "1mybtntext6",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN6,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "1mybtnslot7",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 415,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "1mybtntext7",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN7,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "1mybtnslot8",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 484,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "1mybtnslot8",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN8,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "1mybtnslot9",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 552,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "1mybtnslot9",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN9,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
					),
				},




				#MyShop_Owner
				{
					"name" : "MyShopBoard",
					"type" : "window",
					
					"width" :  622,  "height" :  544,

					"x" : 3, "y" : 28,
					
					"children":
					(
						{
							"name" : "BackgroundMySHop",
							"type" : "image",
							
							"x" : 0, "y" : 0,
							
							"image" : "offlineshop/myshop/base_image_0.png",
						},
						
						
						{
							"name" : "MyShopShopTitle",
							"type" : "text",
							
							"x" : 340, "y" : 5,
							
							"text" : "title",
							"text_horizontal_align" : "center",
						},
						
						{
							"name" : "MyShopEditTitleButton",
							"type" : "button",
							
							"x" : 450, "y" : 15,
							
							"tooltip_text"	: localeinfo.OFFLINESHOP_EDIT_SHOPNAME_TOOLTIP,
							
							"default_image" : "offlineshop/myshop/editname_default.png",
							"over_image" 	: "offlineshop/myshop/editname_over.png",
							"down_image" 	: "offlineshop/myshop/editname_down.png",
						},
						
						{
							"name" : "MyShopShopDuration",
							"type" : "text",
							
							"x" : 340, "y" : 24,
							
							"text" : "99 days",
							"text_horizontal_align" : "center",
						},
						
						
						{
							"name" : "MyShopCloseButton",
							"type" : "button",
							
							"x" : 15,
							"y" : 20,
							
							"default_image" : "d:/ymir work/ui/public/middle_button_01.sub",
							"over_image" : "d:/ymir work/ui/public/middle_button_02.sub",
							"down_image" : "d:/ymir work/ui/public/middle_button_03.sub",

							"text" : localeinfo.OFFLINESHOP_SCRIPTFILE_CLOSE_SHOP_TEXT,
						},
						{
							"name" : "b1myslot1",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 180,
							"y" : 5,
							"width" : 42,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b1mytext1",
									"type" : "text",
									"x" : 0,
									"y" : 0,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT6
								},
							)
						},
						{
							"name" : "b1myslot2",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 180,
							"y" : 25,
							"width" : 42,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b1mytext2",
									"type" : "text",
									"x" : 0,
									"y" : 0,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT7
								},
							)
						},
						{
							"name" : "b1myslot3",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 19,
							"y" : 383,
							"width" : 150,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b1mytext3",
									"type" : "text",
									"x" : 0,
									"y" : 0,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT12,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "b1myslot4",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 19 + 160,
							"y" : 383,
							"width" : 150,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b1mytext4",
									"type" : "text",
									"x" : 0,
									"y" : 0,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT13,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "b1myslot4",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 19 + (170 * 2),
							"y" : 383,
							"width" : 130,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b1mytext4",
									"type" : "text",
									"x" : 0,
									"y" : 0,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT14
								},
							)
						},
						{
							"name" : "b1mybtnslot1",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 3,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b1mybtntext1",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN1,
									"color" : 0xffcca714,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "b1mybtnslot2",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 72,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b1mybtntext2",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN2,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "b1mybtnslot3",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 140,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b1mybtntext3",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN3,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "b1mybtnslot4",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 209,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b1mybtntext4",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN4,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "b1mybtnslot5",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 278,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b1mybtntext5",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN5,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "b1mybtnslot6",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 347,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b1mybtntext6",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN6,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "b1mybtnslot7",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 415,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b1mybtntext7",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN7,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "b1mybtnslot8",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 484,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b1mybtnslot8",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN8,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "b1mybtnslot9",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 552,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b1mybtnslot9",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN9,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
					),
				},



				#shoplist_open
				{
					"name" : "ListOfShop_OpenShop",
					"type" : "window",
					
					"width" :  622,  "height" :  544,

					"x" : 3, "y" : 28,
					
					"children":
					(
						{
							"name" : "BackgroundShopListOpen",
							"type" : "image",
							
							"x" : 0, "y" : 0,
							
							"image" : "offlineshop/shoplist/base_image_open.png",
						},
						
						{
							"name" : "OpenShopBackToListButton",
							"type" : "button",
							
							"x" : 15, "y" : 5,
							
							"default_image" : "d:/ymir work/ui/public/middle_button_01.sub",
							"over_image" : "d:/ymir work/ui/public/middle_button_02.sub",
							"down_image" : "d:/ymir work/ui/public/middle_button_03.sub",

							"text" : localeinfo.OFFLINESHOP_SCRIPTFILE_CLOSE_SHOP_TEXT,
						},
						{
							"name" : "OpenShopShopTitle",
							"type" : "text",
							
							"x" : 328, "y" : 6,

							"text_horizontal_align" : "center",
							"text" : "title",
						},
						
						{
							"name" : "OpenShopShopDuration",
							"type" : "text",
							
							"x" : 328, "y" : 27,

							"text_horizontal_align" : "center",
							"text" : "99 days",
						},
						{
							"name" : "b2myslot1",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 180,
							"y" : 7,
							"width" : 220,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b2mytext1",
									"type" : "text",
									"x" : 0,
									"y" : 0,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT6,
								},
							)
						},
						{
							"name" : "b2myslot2",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 180,
							"y" : 25,
							"width" : 220,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b2mytext2",
									"type" : "text",
									"x" : 0,
									"y" : 0,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT7,
								},
							)
						},
						{
							"name" : "b2mybtnslot1",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 3,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b2mybtntext1",
									"type" : "text",
									"x" : 0,
									"y" : -3,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN1,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "b2mybtnslot2",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 72,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b2mybtntext2",
									"type" : "text",
									"x" : 0,
									"y" : -3,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN2,
									"color" : 0xffcca714,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "b2mybtnslot3",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 140,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b2mybtntext3",
									"type" : "text",
									"x" : 0,
									"y" : -3,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN3,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "b2mybtnslot4",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 209,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b2mybtntext4",
									"type" : "text",
									"x" : 0,
									"y" : -3,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN4,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "b2mybtnslot5",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 278,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b2mybtntext5",
									"type" : "text",
									"x" : 0,
									"y" : -3,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN5,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "b2mybtnslot6",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 347,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b2mybtntext6",
									"type" : "text",
									"x" : 0,
									"y" : -3,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN6,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "b2mybtnslot7",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 415,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b2mybtntext7",
									"type" : "text",
									"x" : 0,
									"y" : -3,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN7,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "b2mybtnslot8",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 484,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b2mybtnslot8",
									"type" : "text",
									"x" : 0,
									"y" : -3,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN8,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "b2mybtnslot9",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 552,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b2mybtnslot9",
									"type" : "text",
									"x" : 0,
									"y" : -3,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN9,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
					),
				},


				#shoplist_list
				{
					"name" : "ListOfShop_List",
					"type" : "window",
					
					"width" :  622,  "height" :  544,

					"x" : 3, "y" : 28,
					"children":
					(
						{
							"name" : "BackgroundShopList",
							"type" : "image",
							
							"x" : 0, "y" : 0,
							
							"image" : "offlineshop/shoplist/base_image_list.png",
						},
						{
							"name" : "2myslot1",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 28,
							"y" : 15,
							"width" : 124,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "2mytext1",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT8,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "2myslot2",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 28 + 143,
							"y" : 15,
							"width" : 124,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "2mytext2",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT9,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "2myslot3",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 28 + (143 * 2),
							"y" : 15,
							"width" : 124,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "2mytext3",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT10,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "2myslot4",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 28 + (143 * 3),
							"y" : 15,
							"width" : 124,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "2mytext4",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT11,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "2mybtnslot1",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 3,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "2mybtntext1",
									"type" : "text",
									"x" : 0,
									"y" : -3,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN1,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "2mybtnslot2",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 72,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "2mybtntext2",
									"type" : "text",
									"x" : 0,
									"y" : -3,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN2,
									"color" : 0xffcca714,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "2mybtnslot3",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 140,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "2mybtntext3",
									"type" : "text",
									"x" : 0,
									"y" : -3,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN3,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "2mybtnslot4",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 209,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "2mybtntext4",
									"type" : "text",
									"x" : 0,
									"y" : -3,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN4,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "2mybtnslot5",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 278,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "2mybtntext5",
									"type" : "text",
									"x" : 0,
									"y" : -3,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN5,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "2mybtnslot6",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 347,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "2mybtntext6",
									"type" : "text",
									"x" : 0,
									"y" : -3,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN6,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "2mybtnslot7",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 415,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "2mybtntext7",
									"type" : "text",
									"x" : 0,
									"y" : -3,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN7,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "2mybtnslot8",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 484,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "2mybtnslot8",
									"type" : "text",
									"x" : 0,
									"y" : -3,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN8,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "2mybtnslot9",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 552,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "2mybtnslot9",
									"type" : "text",
									"x" : 0,
									"y" : -3,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN9,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
					),
				},



				#searchhistory
				{
					"name" : "SearchHistoryBoard",
					"type" : "window",
					
					"width" :  622,  "height" :  544,

					"x" : 3, "y" : 28,
					"children":
					(
						{
							"name" : "BackgroundSearchHistory",
							"type" : "image",
							
							"x" : 0, "y" : 0,
							
							"image" : "offlineshop/searchhistory/base_image.png",
						},
						{
							"name" : "6myslot1",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 14,
							"y" : 15,
							"width" : 170,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "6mytext1",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT33,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "6myslot2",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 214,
							"y" : 15,
							"width" : 170,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "6mytext2",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT34,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "6myslot3",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 416,
							"y" : 15,
							"width" : 170,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "6mytext3",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT35,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "6mybtnslot1",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 3,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "6mybtntext1",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN1,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "6mybtnslot2",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 72,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "6mybtntext2",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN2,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "6mybtnslot3",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 140,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "6mybtntext3",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN3,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "6mybtnslot4",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 209,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "6mybtntext4",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN4,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "6mybtnslot5",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 278,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "6mybtntext5",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN5,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "6mybtnslot6",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 347,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "6mybtntext6",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN6,
									"color" : 0xffcca714,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "6mybtnslot7",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 415,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "6mybtntext7",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN7,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "6mybtnslot8",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 484,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "6mybtnslot8",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN8,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "6mybtnslot9",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 552,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "6mybtnslot9",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN9,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
					),
				},


				#mypatterns
				{
					"name" : "MyPatternsBoard",
					"type" : "window",
					
					"width" :  622,  "height" :  544,

					"x" : 3, "y" : 28,
					"children":
					(
						{
							"name" : "BackgroundMyPatterns",
							"type" : "image",
							
							"x" : 0, "y" : 0,
							
							"image" : "offlineshop/mypatterns/base_image.png",
						},
						{
							"name" : "7myslot1",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 27,
							"y" : 15,
							"width" : 160,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "7mytext1",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT36,
								},
							)
						},
						{
							"name" : "7myslot2",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 367,
							"y" : 15,
							"width" : 160,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "7mytext2",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT37,
								},
							)
						},
						{
							"name" : "7mybtnslot1",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 3,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "7mybtntext1",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN1,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "7mybtnslot2",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 72,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "7mybtntext2",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN2,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "7mybtnslot3",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 140,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "7mybtntext3",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN3,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "7mybtnslot4",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 209,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "7mybtntext4",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN4,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "7mybtnslot5",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 278,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "7mybtntext5",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN5,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "7mybtnslot6",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 347,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "7mybtntext6",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN6,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "7mybtnslot7",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 415,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "7mybtntext7",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN7,
									"color" : 0xffcca714,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "7mybtnslot8",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 484,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "7mybtnslot8",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN8,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "7mybtnslot9",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 552,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "7mybtnslot9",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN9,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
					),
				},




				#searchfilter
				{
					"name" : "SearchFilterBoard",
					"type" : "window",
					
					"width" :  622,  "height" :  544,

					"x" : 3, "y" : 28,
					"children":
					(
						
						{
							"name" : "BackgroundSearchFilter",
							"type" : "image",
							
							"x" : 0, "y" : 0,
							
							"image" : SEARCH_AND_FILTER_BACKGROUND,
						},
						
						{
							"name" : "SearchFilterItemNameInput",
							"type" : "editline",
							
							"width" : 150, "height" : 15,
							
							"input_limit" : 24,
							"x" : 80, "y" : 31,
						},
						
						
						{
							"name" : "SearchFilterItemLevelStart",
							"type" : "editline",
							
							"width" : 43, "height" : 14,
							
							"input_limit" : 3,
							"only_number" : 1,
							"x" : 363, "y" : 32,
						},
						
						
						
						{
							"name" : "SearchFilterItemLevelEnd",
							"type" : "editline",
							
							"width" : 43, "height" : 14,
							
							"input_limit" : 3,
							"only_number" : 1,
							"x" : 363, "y" : 52,
						},
						
						
						{
							"name" : "SearchFilterItemYangMin",
							"type" : "editline",
							
							"width" : 130, "height" : 15,
							
							"input_limit" : len("999999999999999999"),
							"only_number" : 1,
							"x" : 258, "y" : 103,
						},
						
						
						
						{
							"name" : "SearchFilterItemYangMax",
							"type" : "editline",
							
							"width" : 130, "height" : 15,
							
							"input_limit" : len("999999999999999999"),
							"only_number" : 1,
							"x" : 258, "y" : 127,
						},
						
						
						
						
						
						{
							"name" : "SearchFilterResetFilterButton",
							"type" : "button",
							
							"x" : 400, "y" : 487,
							
							"default_image" : "d:/ymir work/ui/public/large_button_01.sub",
							"over_image" : "d:/ymir work/ui/public/large_button_02.sub",
							"down_image" : "d:/ymir work/ui/public/large_button_03.sub",

							"text" : localeinfo.OFFLINESHOP_SCRIPTFILE_RESET_FILTER_TEXT,
						},
						
						
						
						{
							"name" : "SearchFilterSavePatternButton",
							"type" : "button",
							
							"x" : 139, "y" : 487,
							
							"default_image" : "d:/ymir work/ui/public/large_button_01.sub",
							"over_image" : "d:/ymir work/ui/public/large_button_02.sub",
							"down_image" : "d:/ymir work/ui/public/large_button_03.sub",

							"text" : localeinfo.OFFLINESHOP_SCRIPTFILE_SAVE_AS_PATTERN_TEXT,
						},
						
						
						{
							"name" : "SearchFilterStartSearch",
							"type" : "button",
							
							"x" : 268, "y" : 487,
							
							"default_image" : "d:/ymir work/ui/public/large_button_01.sub",
							"over_image" : "d:/ymir work/ui/public/large_button_02.sub",
							"down_image" : "d:/ymir work/ui/public/large_button_03.sub",

							"text" : localeinfo.OFFLINESHOP_SCRIPTFILE_START_SEARCH_TEXT,
						},
						
						
						{
                            "name" : "SearchFilterImportantCover",
                            "type" : "bar",
                            "x" : 392,
                            "y" : 2,
                            "width" : 206,
                            "height" : 18,
                            "color" : 0xdd000000,
                        },
                        {
                            "name" : "SearchFilterImportantTitle",
                            "type" : "text",
                            "x" : 398,
                            "y" : 1,
                            #"text" : "Fontos",
                        },
                        {
                            "name" : "SearchFilterImportantMountsButton",
                            "type" : "button",
                            "x" : 392,
                            "y" : 30,

                            "default_image" : "offlineshop/searchfilter/attribute_default.png",
                            "over_image" : "offlineshop/searchfilter/attribute_over.png",
                            "down_image" : "offlineshop/searchfilter/attribute_down.png",

                            "text" : "Hatasok",
                        },
                        {
                            "name" : "SearchFilterImportantLegendaryButton",
                            "type" : "button",
                            "x" : 392,
                            "y" : 51,

                            "default_image" : "offlineshop/searchfilter/attribute_default.png",
                            "over_image" : "offlineshop/searchfilter/attribute_over.png",
                            "down_image" : "offlineshop/searchfilter/attribute_down.png",

                            "text" : "Legendas Bonuszolo",
                        },
                        {
                            "name" : "SearchFilterImportantLegendaryChangeButton",
                            "type" : "button",
                            "x" : 392,
                            "y" : 72,

                            "default_image" : "offlineshop/searchfilter/attribute_default.png",
                            "over_image" : "offlineshop/searchfilter/attribute_over.png",
                            "down_image" : "offlineshop/searchfilter/attribute_down.png",

                            "text" : "Legendas Megvaltoztato",
                        },
                        {
                            "name" : "SearchFilterImportantTalismanButton",
                            "type" : "button",
                            "x" : 392,
                            "y" : 93,

                            "default_image" : "offlineshop/searchfilter/attribute_default.png",
                            "over_image" : "offlineshop/searchfilter/attribute_over.png",
                            "down_image" : "offlineshop/searchfilter/attribute_down.png",

                            "text" : "Talizman megerosito",
                        },
                        {
                            "name" : "SearchFilterImportantTalismanEnchantButton",
                            "type" : "button",
                            "x" : 392,
                            "y" : 114,

                            "default_image" : "offlineshop/searchfilter/attribute_default.png",
                            "over_image" : "offlineshop/searchfilter/attribute_over.png",
                            "down_image" : "offlineshop/searchfilter/attribute_down.png",

                            "text" : "Talizman buvolo",
                        },
                        {
                            "name" : "SearchFilterImportantOtherButton",
                            "type" : "button",
                            "x" : 392,
                            "y" : 135,

                            "default_image" : "offlineshop/searchfilter/attribute_default.png",
                            "over_image" : "offlineshop/searchfilter/attribute_over.png",
                            "down_image" : "offlineshop/searchfilter/attribute_down.png",

                            "text" : "Sarkany Erme",
                        },
{
							"name" : "b5myslot1",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 38,
							"y" : 11,
							"width" : 70,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b5mytext1",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT17,
									"color" : 0xffcc4214
								},
							)
						},
						{
							"name" : "b5myslot2",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 38,
							"y" : 84,
							"width" : 70,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b5mytext2",
									"type" : "text",
									"x" : 0,
									"y" : 0,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT18,
									"color" : 0xffcc4214
								},
							)
						},
						{
							"name" : "b5myslot3",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 244,
							"y" : 11,
							"width" : 70,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b5mytext3",
									"type" : "text",
									"x" : 0,
									"y" : 0,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT19,
									"color" : 0xffcc4214
								},
							)
						},
						{
							"name" : "b5myslot4",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 240,
							"y" : 84,
							"width" : 70,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b5mytext4",
									"type" : "text",
									"x" : 0,
									"y" : 0,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT20,
									"color" : 0xffcc4214
								},
							)
						},
						{
							"name" : "b5myslot5",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 356,
							"y" : 10,
							"width" : 26,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b5mytext5",
									"type" : "text",
									"x" : 0,
									"y" : 0,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT21,
									"color" : 0xffcc4214
								},
							)
						},
						{
							"name" : "b5myslot6",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 421,
							"y" : 10,
							"width" : 70,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b5mytext6",
									"type" : "text",
									"x" : 0,
									"y" : 0,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT22,
									"color" : 0xffcc4214
								},
							)
						},
						{
							"name" : "b5myslot7",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 17,
							"y" : 32,
							"width" : 57,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b5mytext7",
									"type" : "text",
									"x" : 0,
									"y" : 0,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT23,
								},
							)
						},
						{
							"name" : "b5myslot8",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 17,
							"y" : 54,
							"width" : 57,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b5mytext8",
									"type" : "text",
									"x" : 0,
									"y" : 0,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT24,
								},
							)
						},
						{
							"name" : "b5myslot7",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 17,
							"y" : 32,
							"width" : 57,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b5mytext7",
									"type" : "text",
									"x" : 0,
									"y" : 0,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT23,
								},
							)
						},
						{
							"name" : "b5myslot8",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 17,
							"y" : 54,
							"width" : 57,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b5mytext8",
									"type" : "text",
									"x" : 0,
									"y" : 0,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT24,
								},
							)
						},
						{
							"name" : "b5myslot9",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 335,
							"y" : 32,
							"width" : 24,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b5mytext9",
									"type" : "text",
									"x" : 0,
									"y" : 0,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT25,
								},
							)
						},
						{
							"name" : "b5myslot10",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 335,
							"y" : 54,
							"width" : 24,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b5mytext10",
									"type" : "text",
									"x" : 0,
									"y" : 0,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT26,
								},
							)
						},
						{
							"name" : "b5myslot11",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 92,
							"y" : 94,
							"width" : 46,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b5mytext11",
									"type" : "text",
									"x" : 0,
									"y" : 0,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT27,
								},
							)
						},
						{
							"name" : "b5myslot12",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 92,
							"y" : 120,
							"width" : 46,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b5mytext12",
									"type" : "text",
									"x" : 0,
									"y" : 0,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT28,
								},
							)
						},
						{
							"name" : "b5myslot13",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 165,
							"y" : 94,
							"width" : 46,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b5mytext13",
									"type" : "text",
									"x" : 0,
									"y" : 0,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT29,
								},
							)
						},
						{
							"name" : "b5myslot14",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 164,
							"y" : 120,
							"width" : 46,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b5mytext14",
									"type" : "text",
									"x" : 0,
									"y" : 0,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT30,
								},
							)
						},
						{
							"name" : "b5myslot15",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 223,
							"y" : 105,
							"width" : 32,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b5mytext15",
									"type" : "text",
									"x" : 0,
									"y" : 0,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT31,
								},
							)
						},
						{
							"name" : "b5myslot16",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 223,
							"y" : 127,
							"width" : 32,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b5mytext16",
									"type" : "text",
									"x" : 0,
									"y" : 0,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT32,
								},
							)
						},
						{
							"name" : "5mybtnslot1",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 3,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "5mybtntext1",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN1,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "5mybtnslot2",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 72,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "5mybtntext2",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN2,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "5mybtnslot3",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 140,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "5mybtntext3",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN3,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "5mybtnslot4",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 209,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "5mybtntext4",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN4,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "5mybtnslot5",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 278,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "5mybtntext5",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN5,
									"color" : 0xffcca714,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "5mybtnslot6",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 347,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "5mybtntext6",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN6,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "5mybtnslot7",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 415,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "5mybtntext7",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN7,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "5mybtnslot8",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 484,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "5mybtnslot8",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN8,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "5mybtnslot9",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 552,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "5mybtnslot9",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN9,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
					),
				},



				#safebox
				{
					"name": "ShopSafeboxPage",
					"type": "window",

					"width" :  622,  "height" :  544,

					"x" : 3, "y" : 28,
					"children":
					(
						SAFEBOX_CHILDREN
					),
				},




				#my offers
				{
					"name": "MyOffersPage",
					"type": "window",

					"width" :  622,  "height" :  544,

					"x" : 3, "y" : 28,

					"children":
					(
						{
							"name": "BackgroundMyOffersPage",
							"type": "image",

							"x": 0, "y": 0,

							"image": "offlineshop/myoffers/base_image.png",
						},
						{
							"name" : "4mybtnslot1",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 3,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "4mybtntext1",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN1,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "4mybtnslot2",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 72,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "4mybtntext2",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN2,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "4mybtnslot3",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 140,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "4mybtntext3",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN3,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "4mybtnslot4",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 209,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "4mybtntext4",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN4,
									"color" : 0xffcca714,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "4mybtnslot5",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 278,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "4mybtntext5",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN5,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "4mybtnslot6",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 347,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "4mybtntext6",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN6,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "4mybtnslot7",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 415,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "4mybtntext7",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN7,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "4mybtnslot8",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 484,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "4mybtnslot8",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN8,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "4mybtnslot9",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 552,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "4mybtnslot9",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN9,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
					),
				},




				# my auction
				{
					"name": "MyAuction",
					"type": "window",

					"width" :  622,  "height" :  544,

					"x" : 3, "y" : 28,

					"children":
					(
						{
							"name": "BackgroundMyAuctionPage",
							"type": "image",

							"x": 0, "y": 0,

							"image": "offlineshop/myauction/base_image.png",
						},

						{
							"name" : "MyAuction_OwnerName",
							"type" : "text",

							"x" : 235+67, "y" : 100-70,
							"text_horizontal_align" : "center",
							"text" : " noname ",
						},

						{
							"name" : "MyAuction_Duration",
							"type" : "text",

							"x" : 235+67, "y" : 145-91,
							"text_horizontal_align" : "center",
							"text" : " noname ",
						},

						{
							"name" : "MyAuction_BestOffer",
							"type" : "text",

							"x" : 235+67, "y" : 197-123,
							"text_horizontal_align" : "center",
							"text" : " noname ",
						},

						{
							"name": "MyAuction_MinRaise",
							"type": "text",

							"x": 235+67, "y": 243-147,
							"text_horizontal_align": "center",
							"text": " noname ",
						},
						{
							"name": "MyAuction_Close",
							"type": "button",
							"x" : 235+67+164, "y" : 197-84,
							"default_image": "d:/ymir work/ui/public/middle_button_01.sub",
							"over_image": "d:/ymir work/ui/public/middle_button_02.sub",
							"down_image": "d:/ymir work/ui/public/middle_button_03.sub",
							"text": localeinfo.CLOSE_AUCTION,
						},
						{
							"name" : "b8myslot1",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 162,
							"y" : 32,
							"width" : 75,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b8mytext1",
									"type" : "text",
									"x" : 0,
									"y" : 0,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT41
								},
							)
						},
						{
							"name" : "b8myslot2",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 162,
							"y" : 54,
							"width" : 75,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b8mytext2",
									"type" : "text",
									"x" : 0,
									"y" : 0,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT42
								},
							)
						},
						{
							"name" : "b8myslot3",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 162,
							"y" : 76,
							"width" : 75,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b8mytext3",
									"type" : "text",
									"x" : 0,
									"y" : 0,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT43
								},
							)
						},
						{
							"name" : "b8myslot4",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 162,
							"y" : 98,
							"width" : 75,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b8mytext4",
									"type" : "text",
									"x" : 0,
									"y" : 0,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT44
								},
							)
						},
						{
							"name" : "b8myslot5",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 160,
							"y" : 150,
							"width" : 100,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b8mytext5",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT45,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "b8myslot6",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 370,
							"y" : 150,
							"width" : 100,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b8mytext6",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT46,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "b8mybtnslot1",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 3,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b8mybtntext1",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN1,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "b8mybtnslot2",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 72,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b8mybtntext2",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN2,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "b8mybtnslot3",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 140,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b8mybtntext3",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN3,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "b8mybtnslot4",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 209,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b8mybtntext4",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN4,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "b8mybtnslot5",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 278,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b8mybtntext5",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN5,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "b8mybtnslot6",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 347,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b8mybtntext6",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN6,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "b8mybtnslot7",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 415,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b8mybtntext7",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN7,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "b8mybtnslot8",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 484,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b8mybtnslot8",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN8,
									"color" : 0xffcca714,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "b8mybtnslot9",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 552,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "b8mybtnslot9",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN9,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
					),
				},




				# open acution
				{
					"name": "OpenAuction",
					"type": "window",

					"width" :  622,  "height" :  544,

					"x" : 3, "y" : 28,

					"children":
					(
						{
							"name": "BackgroundOpenAuctionPage",
							"type": "image",

							"x": 0, "y": 0,

							"image": "offlineshop/openauction/base_image.png",
						},

						{
							"name": "OpenAuctionBackToListButton",
							"type": "button",

							"x": 15, "y": 5,

							"default_image": "d:/ymir work/ui/public/middle_button_01.sub",
							"over_image": "d:/ymir work/ui/public/middle_button_02.sub",
							"down_image": "d:/ymir work/ui/public/middle_button_03.sub",

							"text": localeinfo.OFFLINESHOP_SCRIPTFILE_CLOSE_AUCTION_TEXT,
						},

						{
							"name": "OpenAuction_OwnerName",
							"type": "text",

							"x" : 235+67, "y" : 100-70,
							"text_horizontal_align": "center",
							"text": " noname ",
						},

						{
							"name": "OpenAuction_Duration",
							"type": "text",

							"x" : 235+67, "y" : 145-91,
							"text_horizontal_align": "center",
							"text": " noname ",
						},

						{
							"name": "OpenAuction_BestOffer",
							"type": "text",

							"x": 235+67, "y": 197-123,
							"text_horizontal_align": "center",
							"text": " noname ",
						},

						{
							"name": "OpenAuction_MinRaise",
							"type": "text",

							"x": 235+67, "y": 243-147,
							"text_horizontal_align": "center",
							"text": " noname ",
						},
					),
				},




				# acutionlist
				{
					"name": "AuctionList",
					"type": "window",

					"width" :  622,  "height" :  544,

					"x" : 3, "y" : 28,

					"children":
					(
						{
							"name": "BackgroundAuctionListPage",
							"type": "image",

							"x": 0, "y": 0,

							"image": "offlineshop/auctionlist/base_image.png",
						},
						{
							"name" : "9myslot1",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 79,
							"y" : 15,
							"width" : 60,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "9mytext1",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT47
								},
							)
						},
						{
							"name" : "9myslot2",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 198,
							"y" : 15,
							"width" : 60,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "9mytext2",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT48
								},
							)
						},
						{
							"name" : "9myslot3",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 351,
							"y" : 15,
							"width" : 60,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "9mytext3",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT49
								},
							)
						},
						{
							"name" : "9myslot4",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 476,
							"y" : 15,
							"width" : 60,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "9mytext4",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT50
								},
							)
						},
						{
							"name" : "9mybtnslot1",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 3,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "9mybtntext1",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN1,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "9mybtnslot2",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 72,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "9mybtntext2",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN2,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "9mybtnslot3",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 140,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "9mybtntext3",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN3,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "9mybtnslot4",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 209,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "9mybtntext4",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN4,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "9mybtnslot5",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 278,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "9mybtntext5",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN5,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "9mybtnslot6",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 347,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "9mybtntext6",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN6,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "9mybtnslot7",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 415,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "9mybtntext7",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN7,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "9mybtnslot8",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 484,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "9mybtnslot8",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN8,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "9mybtnslot9",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 552,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "9mybtnslot9",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN9,
									"color" : 0xffcca714,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
					),
				},



				#create auction
				{
					"name": "CreateAuction",
					"type": "window",

					"width" :  622,  "height" :  544,

					"x" : 3, "y" : 28,

					"children":
					(
						{
							"name": "BackgroundCreateAuctionPage",
							"type": "image",

							"x": 0, "y": 0,

							"image": "offlineshop/createauction/base_image.png",
						},
						{
							"name": "CreateAuctionDaysInput",
							"type": "text",

							"width": 23, "height": 17,

							"text_horizontal_align" : "center",
							"text" : "0",
							"x": 299, "y": 181,
						},
						{
							"name": "CreateAuctionStartingPriceInput",
							"type": "editline",

							"width": 122, "height": 15,

							"input_limit": 10,
							"only_number": 1,
							"x": 272, "y": 210,
						},

						{
							"name": "CreateAuctionDecreaseDaysButton",
							"type": "button",

							"x": 325,
							"y": 183,

							"default_image": "offlineshop/scrollbar/horizontal/button2_default.png",
							"over_image" : "offlineshop/scrollbar/horizontal/button2_over.png",
							"down_image" 	: "offlineshop/scrollbar/horizontal/button2_down.png",
						},


						{
							"name" : "CreateAuctionIncreaseDaysButton",
							"type" : "button",

							"x" : 267-2,
							"y" : 183,

							"default_image" : "offlineshop/scrollbar/horizontal/button1_default.png",
							"over_image" 	: "offlineshop/scrollbar/horizontal/button1_over.png",
							"down_image" 	: "offlineshop/scrollbar/horizontal/button1_down.png",
						},

						{
							"name" : "CreateAuctionCreateAuctionButton",
							"type" : "button",

							"x" : 211+98, "y" : 267-33,

							"default_image" : "d:/ymir work/ui/public/Small_Button_01.sub",
							"over_image" : "d:/ymir work/ui/public/Small_Button_02.sub",
							"down_image" : "d:/ymir work/ui/public/Small_Button_03.sub",

							"text" : localeinfo.OFFLINESHOP_SCRIPTFILE_CREATE_TEXT,
						},
						{
							"name" : "7myslot1",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 281,
							"y" : 169,
							"width" : 39,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "7mytext1",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT38,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "7myslot2",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 200,
							"y" : 185,
							"width" : 65,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "7mytext2",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT39
								},
							)
						},
						{
							"name" : "7myslot3",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 200,
							"y" : 212,
							"width" : 65,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "7mytext3",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXT40
								},
							)
						},
						{
							"name" : "7mybtnslot1",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 3,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "7mybtntext1",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN1,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "7mybtnslot2",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 72,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "7mybtntext2",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN2,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "7mybtnslot3",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 140,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "7mybtntext3",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN3,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "7mybtnslot4",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 209,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "7mybtntext4",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN4,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "7mybtnslot5",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 278,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "7mybtntext5",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN5,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "7mybtnslot6",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 347,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "7mybtntext6",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN6,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "7mybtnslot7",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 415,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "7mybtntext7",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN7,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "7mybtnslot8",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 484,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "7mybtnslot8",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN8,
									"color" : 0xffcca714,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
						{
							"name" : "7mybtnslot9",
							"type" : "bar",
							"style" : ("attach",),
							"x" : 552,
							"y" : 523,
							"width" : 66,
							"height" : 12,
							"color" : 0x00000000,
							"children" :
							(
								{
									"name" : "7mybtnslot9",
									"type" : "text",
									"x" : 0,
									"y" : -1,
									"text" : uiscriptlocale.SUBTEXT_OFFLINESHOP_TEXTBTN9,
									"horizontal_align" : "center",
									"text_horizontal_align" : "center"
								},
							)
						},
					),
				},



				#menu
				{
					"name": "Menu",
					"type": "window",

					"x": 0,
					"y": WINDOW_HEIGHT-35,

					"width" : WINDOW_WIDTH,
					"height": 35,
					"children":
					(
						{
							"name": "MyShopButton",
							"type": "button",

							"x": 3, "y": 7,

							"width" : 66,
							"height": 21,
						},

						{
							"name": "ListOfShopButton",
							"type": "button",

							"x": 3 + 78, "y": 7,

							"width" : 66,
							"height": 21,
						},

						{
							"name": "ShopSafeboxButton",
							"type": "button",

							"x": 3 +141, "y": 7,

							"width" : 66,
							"height": 21,
						},

						{
							"name": "MyOffersPageButton",
							"type": "button",

							"x": 3 + 213, "y": 7,

							"width" : 66,
							"height": 21,
						},

						{
							"name": "SearchFilterButton",
							"type": "button",

							"x": 3 + 281, "y": 7,

							"width" : 66,
							"height": 21,
						},

						{
							"name": "SearchHistoryButton",
							"type": "button",

							"x": 3+351, "y": 7,

							"width" : 66,
							"height": 21,
						},

						{
							"name": "MyPatternsButton",
							"type": "button",

							"x": 3+418, "y": 7,

							"width" : 66,
							"height": 21,
						},

						{
							"name": "MyAuctionButton",
							"type": "button",

							"x": 3 + 486, "y": 7,

							"width" : 66,
							"height": 21,
						},

						{
							"name": "ListOfAuctionsButton",
							"type": "button",

							"x": 3 + 555, "y": 7,

							"width" : 66,
							"height": 21,
						},
					),
				},




				#loading image
				{
					"name" : "RefreshSymbol",
					"type" : "ani_image",

					"x" : 3, "y" : 28,

					"images": (
						"offlineshop/loading/loading_image0.png",
						"offlineshop/loading/loading_image1.png",
						"offlineshop/loading/loading_image2.png",
						"offlineshop/loading/loading_image3.png",
						"offlineshop/loading/loading_image4.png",
						"offlineshop/loading/loading_image5.png",
						"offlineshop/loading/loading_image6.png",
						"offlineshop/loading/loading_image7.png",
						"offlineshop/loading/loading_image8.png",
						"offlineshop/loading/loading_image9.png",
						"offlineshop/loading/loading_image10.png",
						"offlineshop/loading/loading_image11.png",
						"offlineshop/loading/loading_image12.png",
						"offlineshop/loading/loading_image13.png",
						"offlineshop/loading/loading_image14.png",
						"offlineshop/loading/loading_image15.png",
						"offlineshop/loading/loading_image16.png",
						"offlineshop/loading/loading_image17.png",
					),
				},
			),
		},
	),
}













