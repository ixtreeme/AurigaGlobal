import uiscriptlocale

WINDOW_WIDTH = 537
WINDOW_HEIGHT = 297

ROOT_PATH = "d:/ymir work/ui/game/battle_pass/"

window = {
	"name" : "DungeonListWindow",

	"x" : 0,
	"y" : 0,

	"style" : ("movable", "float",),

	"width" : WINDOW_WIDTH,
	"height" : WINDOW_HEIGHT,

	"children":
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
				## Title
				{
					"name" : "TitleBar",
					"type" : "titlebar",
					"style" : ("attach",),

					"x" : 8,
					"y" : 7,

					"width" : WINDOW_WIDTH - 15,
					"color" : "yellow",

					"children" :
					(
						{ 
							"name" : "TitleName", 
							"type" : "text", 
							
							"x" : (WINDOW_WIDTH - 15) / 2, 
							"y" : 3, 
							
							"text" : "Battle Pass", 
							"text_horizontal_align":"center" 
						},
					),
				},
				
				{
					"name" : "BorderScroll",
					"type" : "border_a",
					"size_skip" : True,

					"x" : 295,
					"y" : 32,

					"width" : 20,
					"height" : 255,

					#"children":
					#(
					# # scroll bar here
					#),
				},
				
				{
					"name" : "BorderMissions",
					"type" : "border_a",

					"x" : 8,
					"y" : 32,

					"width" : 298,
					"height" : 255,

					#"children":
					#(
					# # scroll bar here
					#),
				},
				
				{
					"name" : "BorderRanking",
					"type" : "border_a",

					"x" : 8,
					"y" : 32,

					"width" : 307,
					"height" : 255,

					"children":
					(
						{
							"name" : "RankingTitle",
							"type" : "image",
		
							"x" : 3,
							"y" : 3,
		
							"image" : ROOT_PATH + "ranking_title.tga",
						},
						
						{
							"name" : "pagination",
							"type" : "window",
		
							"x" : 25,
							"y" : 23,
							
							"vertical_align" : "bottom",
		
							"width" : 307,
							"height" : 22,
		
							"children" :
							(
								{
									"name" : "first_prev_button", 
									"type" : "button",
									
									"x" : 0, "y" : 2,
						
									"default_image" : ROOT_PATH + "pagination/btn_first_prev_default.dds",
									"over_image" 	: ROOT_PATH + "pagination/btn_first_prev_hover.dds",
									"down_image" 	: ROOT_PATH + "pagination/btn_first_prev_down.dds",
								},
								{
									"name" : "prev_button", 
									"type" : "button",
									
									"x" : 23, "y" : 2,
						
									"default_image" : ROOT_PATH + "pagination/btn_prev_default.dds",
									"over_image" 	: ROOT_PATH + "pagination/btn_prev_hover.dds",
									"down_image" 	: ROOT_PATH + "pagination/btn_prev_down.dds",
								},
								{
									"name" : "page1_button", 
									"type" : "button",

									"x" : 45, "y" : 0,
						
									"text" : "1",
						
									"default_image" : ROOT_PATH + "pagination/pagenumber_default.tga",
									"over_image" 	: ROOT_PATH + "pagination/pagenumber_default.tga",
									"down_image" 	: ROOT_PATH + "pagination/pagenumber_down.tga",
								},
								{
									"name" : "page2_button", 
									"type" : "button",

									"x" : 78, "y" : 0,
						
									"text" : "2",
						
									"default_image" : ROOT_PATH + "pagination/pagenumber_default.tga",
									"over_image" 	: ROOT_PATH + "pagination/pagenumber_default.tga",
									"down_image" 	: ROOT_PATH + "pagination/pagenumber_down.tga",
								},
								{
									"name" : "page3_button", 
									"type" : "button",

									"x" : 111, "y" : 0,
									
									"text" : "3",
						
									"default_image" : ROOT_PATH + "pagination/pagenumber_default.tga",
									"over_image" 	: ROOT_PATH + "pagination/pagenumber_default.tga",
									"down_image" 	: ROOT_PATH + "pagination/pagenumber_down.tga",
								},
								{
									"name" : "page4_button", 
									"type" : "button",

									"x" : 144, "y" : 0,
						
									"text" : "4",
						
									"default_image" : ROOT_PATH + "pagination/pagenumber_default.tga",
									"over_image" 	: ROOT_PATH + "pagination/pagenumber_default.tga",
									"down_image" 	: ROOT_PATH + "pagination/pagenumber_down.tga",
								},
								{
									"name" : "page5_button", 
									"type" : "button",

									"x" : 177, "y" : 0,
						
									"text" : "5",
						
									"default_image" : ROOT_PATH + "pagination/pagenumber_default.tga",
									"over_image" 	: ROOT_PATH + "pagination/pagenumber_default.tga",
									"down_image" 	: ROOT_PATH + "pagination/pagenumber_down.tga",
								},
								{
									"name" : "next_button", 
									"type" : "button",

									"x" : 222, "y" : 2,
						
									"default_image" : ROOT_PATH + "pagination/btn_next_default.dds",
									"over_image" 	: ROOT_PATH + "pagination/btn_next_hover.dds",
									"down_image" 	: ROOT_PATH + "pagination/btn_next_down.dds",
								},
								{
									"name" : "last_next_button", 
									"type" : "button",

									"x" : 242, "y" : 2,
						
									"default_image" : ROOT_PATH + "pagination/btn_last_next_default.dds",
									"over_image" 	: ROOT_PATH + "pagination/btn_last_next_hover.dds",
									"down_image" 	: ROOT_PATH + "pagination/btn_last_next_down.dds",
								},
							),
						},
					),
				},
				
				{
					"name" : "BorderInfoMission",
					"type" : "border_a",

					"x" : 320,
					"y" : 58,

					"width" : 207,
					"height" : 229,

					"children":
					(
						{
							"name" : "bgImageMission",
							"type" : "image",

							"x" : 3,
							"y" : 3,

							"image" : ROOT_PATH + "mission_big_clean.tga",
							
							"children":
							(
								{
									"name" : "missionImageTitle",
									"type" : "image",
		
									"x" : 0,
									"y" : 0,
		
									"image" : ROOT_PATH + "title_bar_special.tga",
									
									"children":
									(
										{
											"name" : "missionTitleText",
											"type" : "text",
				
											"x" : 100,
											"y" : 4,
				
											"text" : uiscriptlocale.BATTLEPASS_MISSIONNAME,
											"text_horizontal_align" : "center",
											"text_color" : 0xffffeea6,
										},
									),
								},
								
								{
									"name" : "mission_image_10",
									"type" : "image",
		
									"x" : 0,
									"y" : 22,
		
									"image" : ROOT_PATH + "info_bar_title.tga",
									
									"children":
									(
										{
											"name" : "mission_text_10",
											"type" : "text",
						
											"x" : 0,
											"y" : 2,
						
											"text" : uiscriptlocale.BATTLEPASS_STATUS,
											
											"text_horizontal_align" : "center",
											"horizontal_align" : "center",
										},
									),
								},
								
								{
									"name" : "mission_text_0",
									"type" : "text",
						
									"x" : 5,
									"y" : 42,
						
									"text" : "desc",
								},

								{
									"name" : "mission_image_20",
									"type" : "image",
								
									"x" : 0,
									"y" : 65,
								
									"image" : ROOT_PATH + "info_bar_title.tga",
									
									"children":
									(
										{
											"name" : "mission_text_20",
											"type" : "text",
								
											"x" : 0,
											"y" : 2,
								
											"text" : uiscriptlocale.BATTLEPASS_INFORMATIONS,
											
											"text_horizontal_align" : "center",
											"horizontal_align" : "center",
										},
									),
								},
								
								{
									"name" : "mission_text_1",
									"type" : "text",
				
									"x" : 5,
									"y" : 85,
				
									"text" : "",
								},
								
								{
									"name" : "mission_image_2",
									"type" : "image",
		
									"x" : 0,
									"y" : 101,
		
									"image" : ROOT_PATH + "info_bar_even.tga",
									
									"children":
									(
										{
											"name" : "mission_text_2",
											"type" : "text",
						
											"x" : 5,
											"y" : 2,
						
											"text" : "",
										},
									),
								},
								
								{
									"name" : "mission_text_3",
									"type" : "text",
				
									"x" : 5,
									"y" : 121,
				
									"text" : "",
								},
								
								{
									"name" : "mission_image_30",
									"type" : "image",
									"x" : 0,
									"y" : 149,
									"image" : ROOT_PATH + "info_bar_title.tga",
									
									"children":
									(
										{
											"name" : "mission_text_30",
											"type" : "text",
											"x" : 0,
											"y" : 2,
											"text" : uiscriptlocale.BATTLEPASS_DESCRIPTION,
											"text_horizontal_align" : "center",
											"horizontal_align" : "center",
										},
									),
								},
								
								{
									"name" : "mission_text_4",
									"type" : "text",
				
									"x" : 5,
									"y" : 169,
				
									"text" : "desc",
								},
								
								{
									"name" : "mission_image_5",
									"type" : "image",
		
									"x" : 0,
									"y" : 185,
		
									"image" : ROOT_PATH + "info_bar_even.tga",
									
									"children":
									(
										{
											"name" : "mission_text_5",
											"type" : "text",
						
											"x" : 5,
											"y" : 2,
						
											"text" : "desc",
										},
									),
								},
								{
									"name" : "mission_text_6",
									"type" : "text",
				
									"x" : 5,
									"y" : 205,
				
									"text" : "desc",
								},
								
							),
						},
					),
				},
				
				{
					"name" : "BorderInfoGeneral",
					"type" : "border_a",

					"x" : 320,
					"y" : 58,

					"width" : 207,
					"height" : 229,

					"children":
					(
						{
							"name" : "bgImageGeneral",
							"type" : "image",

							"x" : 3,
							"y" : 3,

							"image" : ROOT_PATH + "general_image.tga",
							
							"children":
							(
								{
									"name" : "generalImageTitle",
									"type" : "image",
		
									"x" : 0,
									"y" : 0,
		
									"image" : ROOT_PATH + "title_bar_special.tga",
									
									"children":
									(
										{
											"name" : "generalTitleText",
											"type" : "text",
				
											"x" : 100,
											"y" : 4,
				
											"text" : uiscriptlocale.BATTLEPASS_GENERAL_INFORMATIONS,
											"text_horizontal_align" : "center",
											"text_color" : 0xffffeea6,
										},
									),
								},
								
								{
									"name" : "general_text_0",
									"type" : "text",
				
									"x" : 5,
									"y" : 27,
				
									"text" : "Name: Month Year",
								},
								
								{
									"name" : "general_image_1",
									"type" : "image",
		
									"x" : 0,
									"y" : 43,
		
									"image" : ROOT_PATH + "info_bar_even.tga",
									
									"children":
									(
										{
											"name" : "general_text_1",
											"type" : "text",
						
											"x" : 5,
											"y" : 2,
						
											"text" : "",
										},
									),
								},
								
								{
									"name" : "general_text_2",
									"type" : "text",
				
									"x" : 5,
									"y" : 63,
				
									"text" : "",
								},
								
								{
									"name" : "general_image_3",
									"type" : "image",
		
									"x" : 0,
									"y" : 79,
		
									"image" : ROOT_PATH + "info_bar_even.tga",
									
									"children":
									(
										{
											"name" : "general_text_3",
											"type" : "text",
						
											"x" : 5,
											"y" : 2,
						
											"text" : "",
										},
									),
								},
								
								{
									"name" : "general_text_4",
									"type" : "text",
				
									"x" : 5,
									"y" : 99,
				
									# "text" : "Finished at: (not finished)",
								},
								
								{
									"name" : "totalProgressText",
									"type" : "text",
				
									"x" : 100,
									"y" : 110,
				
									"text" : "",
									"text_horizontal_align" : "center",
								},	

								{
									"name" : "gaugeImageBack",
									"type" : "image",
		
									"x" : 2,
									"y" : 127,
		
									"image" : ROOT_PATH + "total_progress_empty.tga",
									
									"children":
									(
										{
											"name" : "gaugeImage",
											"type" : "expanded_image",
						
											"x" : 8,
											"y" : 2,
						
											"image" : ROOT_PATH + "total_progress_full.tga",
										},
									),
								},
								
								{
									"name" : "ShopButton",
									"type" : "button",
									"x" : 5,
									"y" : 160,
									"default_image" : ROOT_PATH + "ranking_normal.tga",
									"over_image" : ROOT_PATH + "ranking_over.tga",
									"down_image" : ROOT_PATH + "ranking_down.tga",
									"tooltip_text" : uiscriptlocale.BATTLEPASS_SHOP_DESC,
									"tooltip_x" : 0,
									"tooltip_y" : -17,
								},
								
								{
									"name" : "RewardButton",
									"type" : "button",
									"x" : 5,
									"y" : 195,
									"default_image" : ROOT_PATH + "reward_normal.tga",
									"over_image" : ROOT_PATH + "reward_over.tga",
									"down_image" : ROOT_PATH + "reward_down.tga",
									"tooltip_text" : uiscriptlocale.BATTLEPASS_REWARD_DESC,
									"tooltip_x" : 0,
									"tooltip_y" : -51,
								},
								
								{
									"name" : "RewardItems",
									"type" : "grid_table",
		
									"x" : 100,
									"y" : 155,
		
									"start_index" : 0,
									"x_count" : 3,
									"y_count" : 2,
									"x_step" : 32,
									"y_step" : 32,
		
									"image" : "d:/ymir work/ui/public/Slot_Base.sub"
								},
							),
						},
					),
				},
				
				{
					"name" : "tab_control",
					"type" : "window",

					"x" : 332,
					"y" : 32,

					"width" : 171,
					"height" : 29,

					"children" :
					(
						{
							"name" : "tab_01",
							"type" : "image",

							"x" : 0,
							"y" : 0,

							"width" : 171,
							"height" : 29,

							"image" : ROOT_PATH + "category_btn_1.tga",
						},
						{
							"name" : "tab_02",
							"type" : "image",

							"x" : 0,
							"y" : 0,

							"width" : 171,
							"height" : 29,
							
							"image" : ROOT_PATH + "category_btn_2.tga",
						},

						{
							"name" : "tab_button_01",
							"type" : "radio_button",

							"x" : 3,
							"y" : 0,

							"width" : 83,
							"height" : 29,
						},
						{
							"name" : "tab_button_02",
							"type" : "radio_button",

							"x" : 86,
							"y" : 0,

							"width" : 83,
							"height" : 29,
						},
					),
				},
			),
		},
	),
}

