import uiscriptlocale


ROOT_DIR = "d:/ymir work/ui/game/guild/guildgradepage/"
PLUS_WITDH = 80
window = {
	"name" : "GuildWindow_BoardPage",

	"x" : 8+PLUS_WITDH/2,
	"y" : 30,

	"width" : 360+PLUS_WITDH,
	"height" : 298,

	"children" :
	(
		## GuildGradeTItle
		{
			"name" : "GuildGradeTItle", "type" : "image", "x" : -30, "y" : 1, "image" : ROOT_DIR+"GuildGradeTItle.sub",
		},
		## GradeNumber
		{
			"name" : "GradeNumber", "type" : "image", "x" : 10, "y" : 4, "image" : ROOT_DIR+"GradeNumber.sub", #"thintooltip_text" : uiscriptlocale.GUILD_GRADE_NUM,
		},
		## GradeName
		{
			"name" : "GradeName", "type" : "image", "x" : 76, "y" : 4, "image" : ROOT_DIR+"GradeName.sub", #"thintooltip_text" : uiscriptlocale.GUILD_GRADE_RANK,
		},
		## InviteAuthority
		{
			"name" : "InviteAuthority", "type" : "image", "x" : 145, "y" : 4, "image" : ROOT_DIR+"InviteAuthority.sub", #"thintooltip_text" : uiscriptlocale.GUILD_GRADE_PERMISSION_JOIN,
		},
		## DriveOutAuthority
		#{
		#	"name" : "DriveOutAuthority", "type" : "image", "x" : 174, "y" : 4, "image" : ROOT_DIR+"DriveOutAuthority.sub",
		#},
		## NoticeAuthority
		{
			#original is 214
			"name" : "NoticeAuthority", "type" : "image", "x" : 198, "y" : 4, "image" : ROOT_DIR+"NoticeAuthority.sub", #"thintooltip_text" : uiscriptlocale.GUILD_GRADE_PERMISSION_NOTICE,
		},
		## SkillAuthority
		{
			#original is 253
			"name" : "SkillAuthority", "type" : "image", "x" : 246, "y" : 4, "image" : ROOT_DIR+"SkillAuthority.sub", #"thintooltip_text" : uiscriptlocale.GUILD_GRADE_PERMISSION_SKILL,
		},
		## GuildWar
		{
			#original is 299
			"name" : "GuildWar", "type" : "image", "x" : 304, "y" : 4, "image" : ROOT_DIR+"GuildWar.sub", #"thintooltip_text" : uiscriptlocale.GUILD_MEMBER_WAR,
		},
		## Bank
		#{
		#	"name" : "Bank", "type" : "image", "x" : 337, "y" : 4, "image" : ROOT_DIR+"Bank.sub",
		#},
	),
}
