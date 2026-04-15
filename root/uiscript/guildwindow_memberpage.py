import uiscriptlocale

BUTTON_ROOT = "d:/ymir work/ui/game/guild/guildbuttons/memberpage/"
ROOT_DIR = "d:/ymir work/ui/game/guild/guildmemberpage/"
PLUS_WITDH = 80
window = {
	"name" : "GuildWindow_MemberPage",

	"x" : 8,
	"y" : 30,

	"width" : 360 + PLUS_WITDH,
	"height" : 298,

	"children" :
	(
		## GuildGradeTItle
		{
			"name" : "GuildGradeTItle", "type" : "image", "x" : 10, "y" : 1, "image" : ROOT_DIR+"GuildMemberTItle.sub",
		},
		## IndexName
		{
			"name" : "IndexName", "type" : "image", "x" : 45, "y" : 4, "image" : ROOT_DIR+"IndexName.sub", #"thintooltip_text" : uiscriptlocale.GUILD_MEMBER_NAME,
		},
		## IndexGrade
		{
			"name" : "IndexGrade", "type" : "image", "x" : 141, "y" : 4, "image" : ROOT_DIR+"IndexGrade.sub", #"thintooltip_text" : uiscriptlocale.GUILD_MEMBER_RANK,
		},
		## IndexJob
		{
			"name" : "IndexJob", "type" : "image", "x" : 229, "y" : 4, "image" : ROOT_DIR+"IndexJob.sub", #"thintooltip_text" : uiscriptlocale.GUILD_MEMBER_JOB,
		},
		## IndexLevel
		{
			"name" : "IndexLevel", "type" : "image", "x" : 294, "y" : 4, "image" : ROOT_DIR+"IndexLevel.sub", #"thintooltip_text" : uiscriptlocale.GUILD_MEMBER_LEVEL,
		},
		## IndexOffer
		{
			"name" : "IndexOffer", "type" : "image", "x" : 333, "y" : 4, "image" : ROOT_DIR+"IndexOffer.sub", #"thintooltip_text" : uiscriptlocale.GUILD_MEMBER_SPECIFIC_GRAVITY,
		},
		## IndexGeneral
		{
			"name" : "IndexGeneral", "type" : "image", "x" : 382, "y" : 4, "image" : ROOT_DIR+"IndexGeneral.sub", #"thintooltip_text" : uiscriptlocale.GUILD_MEMBER_KNIGHT,
		},
		## ScrollBar
		{
			"name" : "ScrollBar",
			"type" : "scrollbar",

			"x" : 341+PLUS_WITDH,
			"y" : 20,
			"scrollbar_type" : "normal",
			"size" : 240,
		},				
		###MemberOut Button
		#{
		#	"name" : "MemberOutButton",
		#	"type" : "button",
		#	"x" : 10,
		#	"y" : 270,
		#	"default_image" : BUTTON_ROOT+"MemberOutButton00.sub",
		#	"over_image" : BUTTON_ROOT+"MemberOutButton01.sub",
		#	"down_image" : BUTTON_ROOT+"MemberOutButton02.sub",
		#},
		###Vote Check Button
		#{
		#	"name" : "VoteCheckButton",
		#	"type" : "button",
		#	"x" : 130 + PLUS_WITDH/2,
		#	"y" : 270,
		#	"default_image" : BUTTON_ROOT+"VoteCheckButton00.sub",
		#	"over_image" : BUTTON_ROOT+"VoteCheckButton01.sub",
		#	"down_image" : BUTTON_ROOT+"VoteCheckButton02.sub",
		#},
		###Master Change Button
		#{
		#	"name" : "MasterChangeButton",
		#	"type" : "button",
		#	"x" : 250 + PLUS_WITDH,
		#	"y" : 270,
		#	"default_image" : BUTTON_ROOT+"MasterChangeButton00.sub",
		#	"over_image" : BUTTON_ROOT+"MasterChangeButton01.sub",
		#	"down_image" : BUTTON_ROOT+"MasterChangeButton02.sub",	
		#},
	),
}
