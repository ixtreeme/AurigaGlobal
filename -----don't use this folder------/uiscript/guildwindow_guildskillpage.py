import uiscriptlocale

ADD_HEIGHT = 7
LOCALE_PATH = uiscriptlocale.WINDOWS_PATH

SMALL_VALUE_FILE = "d:/ymir work/ui/public/Parameter_Slot_01.sub"
MIDDLE_VALUE_FILE = "d:/ymir work/ui/public/Parameter_Slot_02.sub"
LARGE_VALUE_FILE = "d:/ymir work/ui/public/Parameter_Slot_03.sub"
XLARGE_VALUE_FILE = "d:/ymir work/ui/public/Parameter_Slot_04.sub"

BUTTON_ROOT = "d:/ymir work/ui/game/guild/guildbuttons/skillpage/"
PLUS_WITDH = 80
window = {
	"name" : "GuildWindow_GuildSkillPage",

	"x" : 8,
	"y" : 30,

	"width" : 360+PLUS_WITDH,
	"height" : 298,

	"children" :
	(
		### 길드전 전적 및 기록
		#{
		#	"name":"Guild_War_Report_Bar",
		#	"type":"horizontalbar",
		#	"x":5,
		#	"y": 245,
		#	"width":180 + PLUS_WITDH/2,
		#	"children" :
		#	(
		#		## 길드 전적 및 기록
		#		{
		#			"name":"Guild_War_Report_Title",
		#			"type":"text",
		#			"x" : 0,
		#			"y" : 0,
		#			"all_align" : "center",
		#			"text" : uiscriptlocale.GUILD_WAR_ALLSCORE,
		#		},
		#	),
		#},
		## 전
		#{
		#	"name" : "guildwar_all_score_slot", "type" : "image", "x" : 5, "y" : 270, "image" : SMALL_VALUE_FILE,
		#	"children" :
		#	(
		#		{ "name" : "guildwar_all_score", "type":"text", "x" : 0, "y" : 0, "text" : "", "all_align" : "center",},
		#	),
		#},
		## 승
		#{
		#	"name" : "guildwar_win_score_slot", "type" : "image", "x" : 60, "y" : 270, "image" : SMALL_VALUE_FILE,
		#	"children" :
		#	(
		#		{ "name" : "guildwar_win_score", "type":"text", "x" : 0, "y" : 0, "text" : "", "all_align" : "center", },
		#	),
		#},
		## 패
		#{
		#	"name" : "guildwar_lose_score_slot", "type" : "image", "x" : 60+55, "y" : 270, "image" : SMALL_VALUE_FILE,
		#	"children" :
		#	(
		#		{ "name" : "guildwar_lose_score", "type":"text", "x" : 0, "y" : 0, "text" : "", "all_align" : "center", },
		#	),
		#},
		## 무
		#{
		#	"name" : "guildwar_draw_score_slot", "type" : "image", "x" : 60+55+55, "y" : 270, "image" : SMALL_VALUE_FILE,
		#	"children" :
		#	(
		#		{ "name" : "guildwar_draw_score", "type":"text", "x" : 0, "y" : 0, "text" : "", "all_align" : "center", },
		#	),
		#},
		### 길드전 전적 및 기록
		#{
		#	"name":"Guild_War_RankingLadder",
		#	"type":"horizontalbar",
		#	"x":5+230,
		#	"y":245,
		#	"width":160 + PLUS_WITDH/2,
		#	"children" :
		#	(
		#		{
		#			"name":"Guild_War_RadderPoint",
		#			"type":"text",
		#			"x" : 0,
		#			"y" : 1,
		#			"all_align" : "center",
		#			"text" : uiscriptlocale.GUILD_SKILLPAGE_LADDER+"  &  "+uiscriptlocale.GUILD_SKILLPAGE_RANK,
		#		},
		#	),
		#},
		## 래더점수
		#{
		#	"name" : "guildwar_RadderPoint_slot", "type" : "image", "x" : 240, "y" : 270, "image" : LARGE_VALUE_FILE,
		#	"children" :
		#	(
		#		{ "name" : "guildwar_RadderPoint", "type":"text", "x" : 0, "y" : 0, "text" : "","all_align" : "center", },
		#	),
		#},
		## 순위
		#{
		#	"name" : "guildwar_Ranking_slot", "type" : "image", "x" : 240+100, "y" : 270, "image" : LARGE_VALUE_FILE,
		#	"children" :
		#	(
		#		{ "name" : "guildwar_Ranking", "type":"text", "x" : 0, "y" : 0, "text" : "","all_align" : "center", },
		#	),
		#},
		## 길드전 전적 및 기록
		#{
		#	"name":"Guild_War_RankingLadder",
		#	"type":"horizontalbar",
		#	"x":0,
		#	"y":115 + ADD_HEIGHT,
		#	"width":320+PLUS_WITDH,
		#	"horizontal_align" : "center",
		#	"children" :
		#	(
		#		## 전적
		#		{
		#			"name" : "Guild_landscape", 
		#			"type" : "text", 
		#			"x" : 0, "y" : 0, 
		#			"all_align" : "center",
		#			"text" : uiscriptlocale.GUILD_WAR_REPORTLIST,
		#		},
		#	),
		#},
		## 전적 길드전 이름 리스트 박스
		#{
		#	"name" : "Guild_landscape_List",
		#	"type" : "slotbar",
		#	#"x" : 20 + 10+PLUS_WITDH/2,
		#	"x" : 20,
		#	"y" : 115 + ADD_HEIGHT+25,
		#	"width" : 77+PLUS_WITDH,
		#	"height" : 90,
		#
		#	"children" :
		#	(
		#		{
		#			"name" : "GuildWarListName",
		#			"type" : "listbox",
		#			"x" : 0,
		#			"y" : 1,
		#			"width" : 77+PLUS_WITDH,
		#			"height" : 85,
		#			"horizontal_align" : "center",
		#		},
		#	),
		#},		
		## 전적 리스트 박스
		#{
		#	"name" : "Guild_landscape_List",
		#	"type" : "slotbar",
		#	"x" : 140+PLUS_WITDH/2,
		#	"y" : 115 + ADD_HEIGHT+25,
		#	"width" : 195+PLUS_WITDH/2,
		#	"height" : 90,
		#
		#	"children" :
		#	(
		#		{
		#			"name" : "GuildWarList",
		#			"type" : "listbox",
		#			"x" : 0,
		#			"y" : 1,
		#			"width" : 170+PLUS_WITDH/2,
		#			"height" : 85,
		#			"horizontal_align" : "left",
		#		},
		#		{
		#			"name" : "GuildWarScrollBar",
		#			"type" : "scrollbar",
		#			"x" : 15,
		#			"y" : 2,
		#			"size" : 85,
		#			"horizontal_align" : "right",
		#		},
		#	),
		#},
	
		## 길드 액티브 스킬
		{
			"name":"Active_Skill_Bar",
			"type":"horizontalbar",
			"x":0,
			"y":3 + ADD_HEIGHT,
			"width":320+PLUS_WITDH,
			"horizontal_align" : "center",
			"children" :
			(
				{
					"name":"Active_Skill_Title",
					"type":"text",
					"x" : 0,
					"y" : 0,
					"all_align" : "center",
					"text" : uiscriptlocale.GUILD_WAR_SKILL,
				},
				{ 
					"name":"Passive_Skill_Plus_Label",
					"type":"image",
					"x":240+PLUS_WITDH,
					"y":2,
					"image":LOCALE_PATH+"label_uppt.sub", 
					"children" :
					(
						{
							"name":"Skill_Plus_Value",
							"type":"text",
							"x":61,
							"y":0,
							"text":"99",
							"text_horizontal_align":"center"
						},
					),
				},
			),
		}, ## end of ActiveSkill's horizontal bar

		{
			"name" : "Active_Skill_Slot_Table",
			"type" : "grid_table",

			"x" : 20 + 16+PLUS_WITDH/2,
			"y" : 6 + 23 + ADD_HEIGHT,

			"start_index" : 210,#item.GUILD_SLOT_START_INDEX,
			"x_count" : 9,
			"y_count" : 1,
			"x_step" : 32,
			"y_step" : 32,

			"image" : "d:/ymir work/ui/public/Slot_Base.sub"
		},
		
		###########################################################################################
		# 용신 력.
		{
			"name":"Dragon_God_Power_Title",
			"type":"text",
			"x" : 10+PLUS_WITDH/2,
			"y" : 73 + ADD_HEIGHT + 5,
			"text" : uiscriptlocale.GUILD_SKILL_POWER,
		},
		{
			"name":"Dragon_God_Power_Gauge_Slot",
			"type":"image",
			"x" : 65+PLUS_WITDH/2,
			"y" : 73 + ADD_HEIGHT + 5,
			"image" : "d:/ymir work/ui/game/guild/gauge.sub",
		},
		{
			"name" : "Dragon_God_Power_Gauge",
			"type" : "ani_image",

			"x" : 69+PLUS_WITDH/2,
			"y" : 73 + ADD_HEIGHT + 5,

			"delay" : 6,

			"images" :
			(
				"D:/Ymir Work/UI/Pattern/SPGauge/01.tga",
				"D:/Ymir Work/UI/Pattern/SPGauge/02.tga",
				"D:/Ymir Work/UI/Pattern/SPGauge/03.tga",
				"D:/Ymir Work/UI/Pattern/SPGauge/04.tga",
				"D:/Ymir Work/UI/Pattern/SPGauge/05.tga",
				"D:/Ymir Work/UI/Pattern/SPGauge/06.tga",
				"D:/Ymir Work/UI/Pattern/SPGauge/07.tga",
			),
		},
		{
			"name" : "Dragon_God_Power_Slot",
			"type" : "image",
			"x" : 255+PLUS_WITDH/2,
			"y" : 71 + ADD_HEIGHT - 4,
			"image" : "d:/ymir work/ui/public/Parameter_Slot_03.sub",
			"children" :
			(

				{
					"name":"Dragon_God_Power_Value",
					"type":"text",
					"x" : 0,
					"y" : 0,
					"all_align" : "center",
					"text" : "3000 / 3000",
				},

			),
		},
		## OfferButton
		{
			"name" : "Heal_GSP_Button",
			"type" : "button",
			"x" : 257+PLUS_WITDH/2,
			"y" : 71 + ADD_HEIGHT + 17,
			"default_image" : BUTTON_ROOT+"Heal_GSP_Button00.sub",
			"over_image" : BUTTON_ROOT+"Heal_GSP_Button01.sub",
			"down_image" : BUTTON_ROOT+"Heal_GSP_Button02.sub",
		},

	),
}

