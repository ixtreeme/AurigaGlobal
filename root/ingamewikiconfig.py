import app
import player
import localeinfo

# Melyik modbillentyu nyissa a lada-loot betekintot a wikin: "CTRL" vagy "ALT"
WIKI_CHEST_LOOT_MODIFIER = "ALT"

#/!\/!\/!\/!\/!\/!\/!\/!\/!\/!\/!\/!\/!\/!\/!\/!\/!\/!\/!\/!\
#/!\/!\/!\ CATEGORIES && SUB CATEGORIES MANAGEMENTS /!\/!\/!\

STOLE_VNUMS = [
				85001,
				85002,
				85003,
				85004,
				85005,
				85006,
				85007,
				85008,
				85011,
				85012,
				85013,
				85014,
				85015,
				85016,
				85017,
				85018,
				85071,
				85072,
				85073,
				85074,
				85075,
				85076,
				85077,
				85078,
				85079,
				85080,
				85081,
				85082,
				85153
]

BOSS_CHEST_VNUMS = [

					50139,
					50140,
					50141,
					50142,
					50070,
					38057,
					50071,
					38050,#	nefrit ládája
					38052,#kiralyok_ladaja
					50076,
					50138,
					50099,#Elementale
					#50077,
					50073,
					50078,
					50079,
					50065,
					50069,
					50081,
					50080,
					50254,
					50285
					#halloween
					#, 48701,
					#48706
					#end halloween
					, 71057
					, 48706#Biztonsagos Agares
]

DUNGEON_CHEST_VNUMS = [
					31067,
					50072,
					50075,
					50077,#nightmare dngi
					50074,
					50082,
					50033,
					30300,
					71160,
					50100,
					50102,
					50101,
					50097,
					50098,
					50270,
					50271,
					50063,
					50064,
					50204,
					50266,
					50203,
					30806,
					30934,
					33017,

]

ALT_CHEST_VNUMS = [
					50037,
					50011,
					8885,
					10960,
					10961,
					10962,
					10963,
					10964,
					10965,
					10966,
					10967,
					10968,
					50067,
					39048,
					39050,
					39052,
					39054,
					55009,
					50099,
					50066,
					71203,
					71204,
					71205,
					71206,
					50323,
					50324,
					50143,
					50259
					#christmas
					, 78101
					, 78102
					, 78103
					, 78104
					, 78105
					, 78106
					, 78107
					, 30893
					, 78109
					, 8882
					#end christmas
					,71150
]

COSTUME_WEAPON_VNUMS = [
						40106,#Purple
						40105,#Purple
						40104,#Purple
						40103,#Purple
						40102,#Purple
						40101,#Purple
						40095,
						40096,
						40097,
						40098,
						40099,
						40100,
						40108,
						40109,
						40110,
						40111,
						40112,
						40113,
						40129,
						40130,
						40131,
						40132,
						40133,
						40134
						#halloween
						, 48601,
						48602,
						48603,
						48604,
						48605,
						48606
						#end halloween
						#patch3
						, 49001,
						49002,
						49003,
						49004,
						49005,
						49006,
						49011,
						49012,
						49013,
						49014,
						49015,
						49016
						#end patch3
						#patch4
						, 49021,
						49022,
						49023,
						49024,
						49025,
						49026,
						49031,
						49032,
						49033,
						49034,
						49035,
						49036,
						49041,
						49042,
						49043,
						49044,
						49045,
						49046
						#end patch4
						#christmas
						, 49051
						, 49052
						, 49053
						, 49054
						, 49055
						, 49056
						, 49071
						, 49072
						, 49073
						, 49074
						, 49075
						, 49076
						, 49091
						, 49092
						, 49093
						, 49094
						, 49095
						, 49096
						, 49111
						, 49112
						, 49113
						, 49114
						, 49115
						, 49116
						, 41600
						, 41601
						, 41602
						, 41603
						, 41604
						, 41605
						, 41610
						, 41611
						, 41612
						, 41613
						, 41614
						, 41615

]

COSTUME_ARMOR_VNUMS = [
						41021,#Purple Men
						41022,#Purple Woman
						40192,
						40193,
						40194,
						40195,
						40196,
						40197,
						40198,
						40199,
						41029,
						41030,
						41117,
						41118,
						41119,
						41120
						#halloween
						, 48501,
						48502
						#end halloween
						#patch3
						, 49007,
						49008,
						49017,
						49018
						#end patch3
						#patch4
						, 49027,
						49028,
						49037,
						49038,
						49047,
						49048
						#end patch4
						#christmas
						, 49057
						, 49058
						, 49077
						, 49078
						, 49097
						, 49098
						, 49117
						, 49118
						, 41606
						, 41607
						, 41616
						, 41617
]

COSTUME_HAIR_VNUMS = [
						45137,#Purple Men
						45138,#Purple Woman
						45147,
						45148,
						45053,
						45054,
						45135,
						45136,
						45151,
						45152,
						45242,
						45243
						#patch3
						, 49901,
						49902,
						49903,
						49904
						#end patch3
						#patch4
						, 49905,
						49906
						#end patch4
						#christmas
						, 49907
						, 49908
						, 49911
						, 49912
						, 49915
						, 49916
						, 49919
						, 49920
						, 41608
						, 41609
						, 41618
						, 41619
]

SUPPORT_PET_VNUMS = [
						53001,
						53003,
						53004,
						53005
]

COSTUME_PET_VNUMS = [
						53293,
						53025,
						53256,
						53250,
						55701,
						55702,
						55703,
						55704,
						55705,
						55706,
						55707,
						55708,
						55709,
						55710,
						55711,
						53263,
						53031,
						53028,
						53031,
						53253,
						#halloween
						48301,
						#end halloween
						#patch3
						#end patch3
						#patch4
						49050,
						#end patch4
						#christmas
						60101,
						#, 60102
						60103,
						#, 60104
						#end christmas
]

COSTUME_PET_VNUMS_COSTUMES = [
						88011,
						88013,
						88015,
						88017,
						88019,
						88021,
						88023,
						88025,
						88027,
						88029,
						88031
]

COSTUME_MOUNT_VNUMS = [
						14590,	# Triton     
						14591,	# Beran
						14592,	# Azrael
						14593,	# Beran
						52040,	# Ork// Vad oroszlán
						60001,	# 
						48421,	# 
						49009,	# 
						49049,	# Nemere
						60003,	# 
						71223,	# 
						71253,	# Hidra
						71224,	# Hidra
						71228,	# Nemere
						71251,	# Szfinx
						71125,	# Kaszás
						71126,	# 
						71127,	# Monkey King   Vihar tigris
						71139,	# 
						71166,	# 
						71171,	# 
						71176,	# 
						71177,	# MAJOM Run
						71221,	# 
						71222,	# 
						71252,	# Azrael
						71256,	# 
						71225,	# 
						71226,	# Dungeon Slime Equus Nocturnus
						71227,	# 
						71255,	# 
						71254,	# ORK Fehér oroszlán
						71233,	# SLIME Szellem ló 
						71250,	# 
						23014,	# 
						23015,	# 
						23016,	# 
						71137,	# 
						71140,	# 
						611501,	# Vízi bestia
						611502,	# Démoni Sárkány            nightmare run
						611503,	# Démoni Vadállat
						611504,	# Razador Kiskedvence
						611505,	# Oroszlánkirály
						611506,	# Túlvilági ló
						611512,	# Razor Lova
						611513,	# Fekete Motor
						611515,	# Lucky
						611516,	# Szopósszájú
						611517,	# Páncélos Cápa
						611518,	# Pigi
						611520,	# Ragadozó
						611521,	# Cuki
						611522,	# Cyber Bike
						611523,	# UniBoy
						611524,	# Mennyei Farkas
						611525,	# Gilien
						611526,	# Óriás Bálna
						611527,	# Páncélos Hal
						611528,	# Kalapos nyúl
						611530,	# Tüzes Róka
						611531,	# Grinder
						611532,	# Pingvin király
						611533,	# Kék Csibe
						611534,	# Sárga Csibe
						611535,	# Tűzmadár
						611536,	# Hello Kitty Pigi
						611537,	# Alvilági Ló           nightmare rn
						611538,	# Arany Many
						611540,	# Egyszarvú
						611542,	# Dacia
						611543,	# Motor
						611544,	# Szellemlovas Motorja
						611545,
						611546,
						611547,
						611549,#azrael
						611550,#azrael
						611551#sés
						
						
]



COSTUME_MOUNT_VNUMS_COSTUMES = [
						88111,
						88113,
						88115,
						88117
]

COSTUME_SASH_VNUMS = [
						85019,
						85020,
						85021,
						85022,
						85023,
						85024,
						85025,
						85026,
						85027,
						85028,
						85029,
						85030,
						85031,
						85032,
						85033,
						85034,
						85035,
						85036,
						85037,
						85038,
						85039,
						85040,
						85041,
						85042,
						85043,
						85044,
						85045,
						85046,
						85047,
						85048,
						85049,
						85050,
						85051,
						85052,
						85053,
						85054,
						85055,
						85056,
						85057,
						85058,
						85059,
						85060,
						85061,
						85062,
						85063,
						85064,
						85065,
						85066,
						85067,
						85068,
						85069,
						85070,
						85083
						#halloween
						, 85088,
						85092,
						85096,
						85100
						#end halloween
						#patch3
						, 85104,
						85108
						#end patch3
						#patch4
						, 85112
						, 85116
						, 85120
						#end patch4
						#christmas
						, 85124
						, 85132
						, 85140
						, 85148
						#end christmas
]

COSTUME_WEAPONS_EFFECT_VNUMS = [
								88258,#Purple
								88211,
								88215,
								88219,
								88223,
								88227,
								88231,
								88235,
								88239,
								88243,
								88247,
								88251,
								88255
								#patch4
								, 78001
								#end patch4
								#christmas
								, 78003
								, 78007
								, 78011
								#end christmas
]

COSTUME_ARMORS_EFFECT_VNUMS = [
								88259,#Purple
								88213,
								88217,
								88221,
								88225,
								88229,
								88233,
								88237,
								88241,
								88245,
								88249,
								88253,
								88257
								#patch4
								, 78002
								#end patch4
								#christmas
								, 78005
								, 78009
								, 78013
								#end christmas
]

#BANNERS_PATH = app.GetLocaleName()
#if BANNERS_PATH != "en" and BANNERS_PATH != "ro" and BANNERS_PATH != "it":
BANNERS_PATH = "en"

WIKI_CATEGORIES = [
	[
		localeinfo.WIKI_CATEGORY_EQUIPEMENT,
		[
			[localeinfo.WIKI_SUBCATEGORY_WEAPONS, (0,), "d:/ymir work/ui/wiki/banners/%s/razor_fasza.png" % BANNERS_PATH],
			[localeinfo.WIKI_SUBCATEGORY_ARMOR, (1,), "d:/ymir work/ui/wiki/banners/%s/razor_fasza.png" % BANNERS_PATH],
			[localeinfo.WIKI_SUBCATEGORY_HELMET, (4,), "d:/ymir work/ui/wiki/banners/%s/razor_fasza.png" % BANNERS_PATH],
			[localeinfo.WIKI_SUBCATEGORY_SHIELD, (6,), "d:/ymir work/ui/wiki/banners/%s/razor_fasza.png" % BANNERS_PATH],
			[localeinfo.WIKI_SUBCATEGORY_EARRINGS, (2,), "d:/ymir work/ui/wiki/banners/%s/razor_fasza.png" % BANNERS_PATH],
			[localeinfo.WIKI_SUBCATEGORY_BRACELET, (7,), "d:/ymir work/ui/wiki/banners/%s/razor_fasza.png" % BANNERS_PATH],
			[localeinfo.WIKI_SUBCATEGORY_NECKLACE, (5,), "d:/ymir work/ui/wiki/banners/%s/razor_fasza.png" % BANNERS_PATH],
			[localeinfo.WIKI_SUBCATEGORY_SHOES, (3,), "d:/ymir work/ui/wiki/banners/%s/razor_fasza.png" % BANNERS_PATH],
			[localeinfo.WIKI_SUBCATEGORY_BELTS, (9,), "d:/ymir work/ui/wiki/banners/%s/razor_fasza.png" % BANNERS_PATH],
			[localeinfo.WIKI_SUBCATEGORY_TALISMANS, (10,), "d:/ymir work/ui/wiki/banners/%s/razor_fasza.png" % BANNERS_PATH],
		]
	],
	[
		localeinfo.WIKI_CATEGORY_COSTUMES,
		[
			[localeinfo.WIKI_SUBCATEGORY_WEAPONS, (COSTUME_WEAPON_VNUMS,), "d:/ymir work/ui/wiki/banners/%s/razor_fasza.png" % BANNERS_PATH],
			[localeinfo.WIKI_SUBCATEGORY_ARMOR, (COSTUME_ARMOR_VNUMS,), "d:/ymir work/ui/wiki/banners/%s/razor_fasza.png" % BANNERS_PATH],
			[localeinfo.WIKI_SUBCATEGORY_HAIRSTYLES, (COSTUME_HAIR_VNUMS,), "d:/ymir work/ui/wiki/banners/%s/razor_fasza.png" % BANNERS_PATH],
			[localeinfo.WIKI_SUBCATEGORY_STOLE, (STOLE_VNUMS,), "d:/ymir work/ui/wiki/banners/%s/razor_fasza.png" % BANNERS_PATH],
			[localeinfo.WIKI_SUBCATEGORY_STOLE_COSTUMES, (COSTUME_SASH_VNUMS,), "d:/ymir work/ui/wiki/banners/%s/razor_fasza.png" % BANNERS_PATH],
			[localeinfo.WIKI_SUBCATEGORY_WEAPONS_EFFECT, (COSTUME_WEAPONS_EFFECT_VNUMS,), "d:/ymir work/ui/wiki/banners/%s/razor_fasza.png" % BANNERS_PATH],
			[localeinfo.WIKI_SUBCATEGORY_ARMORS_EFFECT, (COSTUME_ARMORS_EFFECT_VNUMS,), "d:/ymir work/ui/wiki/banners/%s/razor_fasza.png" % BANNERS_PATH]
		]
	],
	[
		localeinfo.WIKI_CATEGORY_COMPANY,
		[
			#[localeinfo.WIKI_SUBCATEGORY_SUPPORT, (SUPPORT_PET_VNUMS,), "d:/ymir work/ui/wiki/banners/%s/support.png" % BANNERS_PATH],
			[localeinfo.WIKI_SUBCATEGORY_PET, (COSTUME_PET_VNUMS,), "d:/ymir work/ui/wiki/banners/%s/razor_fasza.png" % BANNERS_PATH],
			[localeinfo.WIKI_SUBCATEGORY_PET_COSTUMES, (COSTUME_PET_VNUMS_COSTUMES,), "d:/ymir work/ui/wiki/banners/%s/razor_fasza.png" % BANNERS_PATH],
			[localeinfo.WIKI_SUBCATEGORY_MOUNT, (COSTUME_MOUNT_VNUMS,), "d:/ymir work/ui/wiki/banners/%s/razor_fasza.png" % BANNERS_PATH],
			[localeinfo.WIKI_SUBCATEGORY_MOUNT_COSTUMES, (COSTUME_MOUNT_VNUMS_COSTUMES,), "d:/ymir work/ui/wiki/banners/%s/razor_fasza.png" % BANNERS_PATH],
		]
	],
	[
		localeinfo.WIKI_CATEGORY_CHESTS,
		[
			[localeinfo.WIKI_SUBCATEGORY_CHESTS, (BOSS_CHEST_VNUMS,), "d:/ymir work/ui/wiki/banners/%s/razor_fasza.png" % BANNERS_PATH],
			[localeinfo.WIKI_SUBCATEGORY_DUNGEONS, (DUNGEON_CHEST_VNUMS,), "d:/ymir work/ui/wiki/banners/%s/razor_fasza.png" % BANNERS_PATH],
			[localeinfo.WIKI_SUBCATEGORY_ALTERNATIVE_CHESTS, (ALT_CHEST_VNUMS,), "d:/ymir work/ui/wiki/banners/%s/razor_fasza.png" % BANNERS_PATH]
		]
	],
	[
		localeinfo.WIKI_CATEGORY_BOSSES,
		[
			[localeinfo.WIKI_SUBCATEGORY_LV1_75, (0, 1, 75), "d:/ymir work/ui/wiki/banners/%s/razor_fasza.png" % BANNERS_PATH],
			[localeinfo.WIKI_SUBCATEGORY_LV76_100, (0, 76, 100), "d:/ymir work/ui/wiki/banners/%s/razor_fasza.png" % BANNERS_PATH],
			[localeinfo.WIKI_SUBCATEGORY_LV100, (0, 100, 255), "d:/ymir work/ui/wiki/banners/%s/razor_fasza.png" % BANNERS_PATH]
		]
	],
	[
		localeinfo.WIKI_CATEGORY_MONSTERS,
		[
			[localeinfo.WIKI_SUBCATEGORY_LV1_75, (1, 1, 75), "d:/ymir work/ui/wiki/banners/%s/razor_fasza.png" % BANNERS_PATH],
			[localeinfo.WIKI_SUBCATEGORY_LV76_100, (1, 76, 100), "d:/ymir work/ui/wiki/banners/%s/razor_fasza.png" % BANNERS_PATH],
			#[localeinfo.WIKI_SUBCATEGORY_LV100, (1, 100, 255), "d:/ymir work/ui/wiki/banners/%s/monster_over100.png" % BANNERS_PATH]
		]
	],
	[
		localeinfo.WIKI_CATEGORY_METINSTONES,
		[
			[localeinfo.WIKI_SUBCATEGORY_LV1_75, (2, 1, 75), "d:/ymir work/ui/wiki/banners/%s/razor_fasza.png" % BANNERS_PATH],
			[localeinfo.WIKI_SUBCATEGORY_LV76_100, (2, 76, 100), "d:/ymir work/ui/wiki/banners/%s/razor_fasza.png" % BANNERS_PATH],
			[localeinfo.WIKI_SUBCATEGORY_LV100, (2, 100, 255), "d:/ymir work/ui/wiki/banners/%s/razor_fasza.png" % BANNERS_PATH]
		]
	],
	[
		localeinfo.WIKI_CATEGORY_SYSTEMS,
		[
			#[localeinfo.WIKI_SUBCATEGORY_DRAGONALCHEMY, ("systems/dragon_alchemy.txt",)],
			#[localeinfo.WIKI_SUBCATEGORY_BATTLEPASS, ("systems/battlepass.txt",)],
			[localeinfo.WIKI_SUBCATEGORY_BOOST, ("systems/boost.txt",)],
			[localeinfo.WIKI_SUBCATEGORY_PASSIVE, ("systems/passive.txt",)],
			[localeinfo.WIKI_SUBCATEGORY_BIOLOGIST, ("systems/biologist.txt",)],
			[localeinfo.WIKI_SUBCATEGORY_ANTIFARM, ("systems/multifarm.txt",)],
			[localeinfo.WIKI_SUBCATEGORY_SKILL_COLOR, ("systems/skill_color.txt",)],
			[localeinfo.WIKI_SUBCATEGORY_FREEZE, ("systems/freeze.txt",)],
			#[localeinfo.WIKI_SUBCATEGORY_COSTUME_SASH, ("systems/costume_sash.txt",)],
			[localeinfo.WIKI_SUBCATEGORY_COSTUMES, ("systems/costumes.txt",)],
			#[localeinfo.WIKI_SUBCATEGORY_PET, ("systems/pet.txt",)],
			#[localeinfo.WIKI_SUBCATEGORY_ANIME, ("systems/anime.txt",)],
			#[localeinfo.WIKI_SUBCATEGORY_SHOULDER_SASH, ("systems/shoulder_sash.txt",)],
			[localeinfo.WIKI_SUBCATEGORY_EPIC_SYSTEM, ("systems/epic.txt",)],
			#[localeinfo.WIKI_SUBCATEGORY_RUNES, ("systems/runes.txt",)],
			#[localeinfo.WIKI_SUBCATEGORY_SHAMY, ("systems/shamy.txt",)],
			[localeinfo.WIKI_SUBCATEGORY_TALISMANS, ("systems/talismans.txt",)],
			#[localeinfo.WIKI_SUBCATEGORY_TRANSLATE, ("systems/translate.txt",)],
			#[localeinfo.WIKI_SUBCATEGORY_GAYA, ("systems/gaya.txt",)],
			#[localeinfo.WIKI_SUBCATEGORY_GUILD, ("systems/guilds.txt",)],
			[localeinfo.WIKI_SUBCATEGORY_OFFLINESHOP, ("systems/offlineshop.txt",)]
		]
	],
	[
		localeinfo.WIKI_CATEGORY_DUNGEONS,
		[
			[localeinfo.WIKI_SUBCATEGORY_D_ORCMAZE, ("dungeons/orcmaze.txt",)],
			[localeinfo.WIKI_SUBCATEGORY_D_MELMA, ("dungeons/melma.txt",)],
			[localeinfo.WIKI_SUBCATEGORY_D_ARACNIDI, ("dungeons/aracnidi.txt",)],
			[localeinfo.WIKI_SUBCATEGORY_D_DEVILSTOWER, ("dungeons/devilstower.txt",)],
			[localeinfo.WIKI_SUBCATEGORY_D_RUNE, ("dungeons/rune_dungeon.txt",)],
			[localeinfo.WIKI_SUBCATEGORY_D_CATACOMBE, ("dungeons/catacombe.txt",)],
			[localeinfo.WIKI_SUBCATEGORY_D_BERAN, ("dungeons/beran.txt",)],
			[localeinfo.WIKI_SUBCATEGORY_D_MELEY, ("dungeons/meley.txt",)],
			[localeinfo.WIKI_SUBCATEGORY_D_RAZADOR, ("dungeons/razador.txt",)],
			[localeinfo.WIKI_SUBCATEGORY_D_NEMERE, ("dungeons/nemere.txt",)],
			[localeinfo.WIKI_SUBCATEGORY_D_OCHAO, ("dungeons/ochao.txt",)],
			[localeinfo.WIKI_SUBCATEGORY_D_JOTUN, ("dungeons/jotun.txt",)],
			[localeinfo.WIKI_SUBCATEGORY_D_NAVE, ("dungeons/nave.txt",)],#haj��v��delem
			[localeinfo.WIKI_SUBCATEGORY_D_PIRAMIDE, ("dungeons/piramide.txt",)]
		]
	],
	#[
	#	localeinfo.WIKI_CATEGORY_EVENTS,
	#	[
	#		#[localeinfo.WIKI_SUBCATEGORY_OKAY_CARD, ("events/okey_card.txt",)],
	#		#[localeinfo.WIKI_SUBCATEGORY_FISHPUZZLE, ("events/fishpuzzle.txt",)],
	#		#[localeinfo.WIKI_SUBCATEGORY_BOSS_HUNT, ("events/boss_hunt.txt",)],
	#		#[localeinfo.WIKI_SUBCATEGORY_MOONLIGHT_CHESTS, ("events/moonlight_chests.txt",)],
	#		#[localeinfo.WIKI_SUBCATEGORY_HEXAGONAL_CHESTS, ("events/hexagonal_chests.txt",)],
	#		#[localeinfo.WIKI_SUBCATEGORY_TAG_TEAM, ("events/tag_team.txt",)],
	#		#[localeinfo.WIKI_SUBCATEGORY_EMPIRE_WAR, ("events/empire_war.txt",)],
	#		#[localeinfo.WIKI_SUBCATEGORY_PVP_TOURNAMENT, ("events/pvp_tournament.txt",)]
	#	]
	#],
	[
		localeinfo.WIKI_CATEGORY_GUIDES,
		[
			[localeinfo.WIKI_SUBCATEGORY_FASTKEY, ("guides/fastkey.txt",)],
			[localeinfo.WIKI_SUBCATEGORY_FASTKEY1, ("guides/fegyo.txt",)]

		]
	],
]

#/!\/!\/!\/!\/!\/!\/!\/!\/!\/!\
#/!\/!\/!\ OTHER STUFF /!\/!\/!\

ITEM_BLACKLIST = [
					20,
					500,
					3500,
					1920,
					1680,
					1620,
					1600,
					1640,
					1660,
					1940,
					1840,
					12050,
					20000,
					1860,
					12060,
					20250,
					1900,
					1880,
					12070,
					20500,
					13120,
					13100,
					13080,
					13060,
					17570,
					17560,
					17540,
					17520,
					17500,
					12710,
					12760,
					12720,
					12760,
					12700,
					12740,
					12690,
					12730,
					14580,
					14560,
					14500,
					14520,
					14540,
					16580,
					16560,
					16540,
					16520,
					16500,
					15390,
					15370,
					40108,#Fogas kosty Fegyő
					40109,#Fogas kosty Fegyő
					40110,#Fogas kosty Fegyő
					40111,#Fogas kosty Fegyő
					40112,#Fogas kosty Fegyő
					40113,#Fogas kosty Fegyő
					40129,#Kezdő Kosty Fegyő
					40130,#Kezdő Kosty Fegyő
					40131,#Kezdő Kosty Fegyő
					40132,#Kezdő Kosty Fegyő
					40133,#Kezdő Kosty Fegyő
					40134,#Kezdő Kosty Fegyő
					41029,#Siv Harcos
					41030,#Siv Harcos
					41117,#Muskéta
					41118,#Muskéta
					49057,#Valentin EVENT
					49058,#Valentin EVENT
					49077,#Mikulás EVENT
					49078,#Mikulás EVENT
					49097,#Fantom
					49098,#Fantom
					45147,#Urban haj
					45148,#Urban haj
					45053,#Muskéta haj
					45054,#Szabó haj
					45135,#Kobra haj
					45136,#Kobra haj
					45151,#Azrael haj
					45152,#Azrael haj
					49907,#Valentin Haj EVENT
					49908,#Valentin Haj EVENT
					49911,#Mikulás haj EVENT
					49912,#Mikulás haj EVENT
					49051,#Karácsony Fegyó EVENT
					49052,#Karácsony Fegyó EVENT
					49053,#Karácsony Fegyó EVENT
					49054,#Karácsony Fegyó EVENT
					49055,#Karácsony Fegyó EVENT
					49056,#Karácsony Fegyó EVENT
					49071,#Mikulas Fegyó EVENT
					49072,#Mikulas Fegyó EVENT
					49073,#Mikulas Fegyó EVENT
					49074,#Mikulas Fegyó EVENT
					49076,#Mikulas Fegyó EVENT
					49091,#Fantom Fegyó EVENT
					49092,#Fantom Fegyó EVENT
					49093,#Fantom Fegyó EVENT
					49094,#Fantom Fegyó EVENT
					49095,#Fantom Fegyó EVENT
					49096,#Fantom Fegyó EVENT
					12080,
					20750,
					2500,
					1500,
					7500,
					5500,
					510,
					21,
					22,
					23,
					24,
					25,
					26,
					27,
					28,
					29,
					30,
					31,
					32,
					33,
					34,
					35,
					36,
					37,
					38,
					39,
					200,
					201,
					202,
					203,
					204,
					205,
					206,
					207,
					208,
					209,
					210,
					211,
					212,
					213,
					214,
					215,
					216,
					217,
					218,
					219,
					220,
					221,
					222,
					223,
					224,
					225,
					226,
					227,
					228,
					229,
					230,
					231,
					232,
					233,
					234,
					235,
					236,
					237,
					238,
					239,
					260,
					261,
					262,
					263,
					264,
					265,
					266,
					267,
					268,
					269,
					1010,
					1011,
					1012,
					1013,
					1014,
					1015,
					1016,
					1017,
					1018,
					1019,
					1140,
					1141,
					1142,
					1143,
					1144,
					1145,
					1146,
					1147,
					1148,
					1149,
					1150,
					1151,
					1152,
					1153,
					1154,
					1155,
					1156,
					1157,
					1158,
					1159,
					1160,
					1161,
					1162,
					1163,
					1164,
					1165,
					1166,
					1167,
					1168,
					1169,
					2010,
					2011,
					2012,
					2013,
					2014,
					2015,
					2016,
					2017,
					2018,
					2019,
					2020,
					2021,
					2022,
					2023,
					2024,
					2025,
					2026,
					2027,
					2028,
					2029,
					2035,
					2065,
					2075,
					2190,
					2192,
					2193,
					2194,
					2195,
					2196,
					2197,
					2198,
					2199,
					3010,
					3011,
					3012,
					3013,
					3014,
					3015,
					3016,
					3017,
					3018,
					3019,
					3020,
					3021,
					3022,
					3023,
					3024,
					3025,
					3026,
					3027,
					3028,
					3029,
					3170,
					3171,
					3172,
					3173,
					3174,
					3175,
					3176,
					3177,
					3178,
					3179,
					3200,
					3201,
					3202,
					3203,
					3204,
					3205,
					3206,
					3207,
					3208,
					3209,
					4000,
					4001,
					4002,
					4003,
					4004,
					4005,
					4006,
					4007,
					4008,
					4009,
					4020,
					4021,
					4022,
					4023,
					4024,
					4025,
					4026,
					4027,
					4028,
					4029,
					4030,
					4031,
					4032,
					4033,
					4034,
					4035,
					4036,
					4037,
					4038,
					4039,
					5130,
					5131,
					5132,
					5133,
					5134,
					5135,
					5136,
					5137,
					5138,
					5139,
					5140,
					5141,
					5142,
					5143,
					5144,
					5145,
					5146,
					5147,
					5148,
					5149,
					5150,
					5151,
					5152,
					5153,
					5154,
					5155,
					5156,
					5157,
					5158,
					5159,
					7010,
					7011,
					7012,
					7013,
					7014,
					7015,
					7016,
					7017,
					7018,
					7019,
					7020,
					7021,
					7022,
					7023,
					7024,
					7025,
					7026,
					7027,
					7028,
					7029,
					7050,
					7051,
					7052,
					7053,
					7054,
					7055,
					7056,
					7057,
					7058,
					7059,
					7170,
					7171,
					7172,
					7173,
					7174,
					7175,
					7176,
					7177,
					7178,
					7179,
					7200,
					7201,
					7202,
					7203,
					7204,
					7205,
					7206,
					7207,
					7208,
					7209,
					12000,
					12001,
					12002,
					12003,
					12004,
					12005,
					12006,
					12007,
					12008,
					12009,
					13180,
					13181,
					13182,
					13183,
					13184,
					13185,
					13186,
					13187,
					13188,
					13189,
					13190,
					13191,
					13192,
					13193,
					13194,
					13195,
					13196,
					13197,
					13198,
					13199,
					13200,
					13201,
					13202,
					13203,
					13204,
					13205,
					13206,
					13207,
					13208,
					13209,
					14570,
					14571,
					14572,
					14573,
					14574,
					14575,
					14576,
					14577,
					14578,
					14579,
					15240,
					15241,
					15242,
					15243,
					15244,
					15245,
					15246,
					15247,
					15248,
					15249,
					16570,
					16571,
					16572,
					16573,
					16574,
					16575,
					16576,
					16577,
					16578,
					16579,
					18070,
					18071,
					18072,
					18073,
					18074,
					18075,
					18076,
					18077,
					18078,
					18079,
					20810,
					20811,
					20812,
					20813,
					20814,
					20815,
					20816,
					20817,
					20818,
					20819,
					20820,
					20821,
					20822,
					20823,
					20824,
					20825,
					20826,
					20827,
					20828,
					20829,
					20830,
					20831,
					20832,
					20833,
					20834,
					20835,
					20836,
					20837,
					20838,
					20839,
					20840,
					20841,
					20842,
					20843,
					20844,
					20845,
					20846,
					20847,
					20848,
					20849,
					20860,
					20861,
					20862,
					20863,
					20864,
					20865,
					20866,
					20867,
					20868,
					20869,
					20870,
					20871,
					20872,
					20873,
					20874,
					20875,
					20876,
					20877,
					20878,
					20879,
					20880,
					20881,
					20882,
					20883,
					20884,
					20885,
					20886,
					20887,
					20888,
					20889,
					20890,
					20891,
					20892,
					20893,
					20894,
					20895,
					20896,
					20897,
					20898,
					20899
]

MOB_WHITELIST = [
					191,
					194,
					192,
					193,
					405,
					436,
					491,
					492,
					493,
					494,
					534,
					591,
					632,
					633,
					634,
					636,
					656,
					691,
					693,
					704,
					706,
					731,
					768,
					777,
					791,
					935,
					1092,
					1093,
					1192,
					1304,
					1901,
					2033,
					2034,
					2091,
					2092,
					2191,
					2205,
					2206,
					2291,
					2302,
					2304,
					2306,
					2307,
					2312,
					2314,
					2492,
					2493,
					2494,
					2495,
					2598,
					3090,
					3190,
					3191,
					3290,
					3390,
					3590,
					3490,
					3491,
					3591,
					3595,
					3690,
					3691,
					3790,
					3890,
					3910,
					3911,
					3912,
					3913,
					3965,
					3996,
					3997,
					3998,
					4011,
					4012,
					4013,
					6091,
					6191,
					6192,
					6391,
					6393,
					6407,
					6408,
					7073,
					8001,
					8002,
					8003,
					8004,
					8005,
					8006,
					8007,
					8008,
					8009,
					8010,
					8011,
					8013,
					8014,
					8024,
					8025,
					8026,
					8027,
					8051,
					8052,
					8053,
					8054,
					8055,
					8056,
					8059,
					8061,
					9337
					#halloween
					#, 4201,
					#8301,
					#8302,
					#8303,
					#8304,
					#8305,
					#8306,
					#8307,
					#8308,
					#8309,
					#8310,
					#8311,
					#4203,
					#end halloween
					#christmas
					, 4081
					, 4479
					#end christmas
]

HAIRSTYLE_CAMERA_CFG = {
	player.MAIN_RACE_WARRIOR_M : ([311.4753, -16.3934, 150.0000], [0.0000, 0.0000, 152.3934]),
	player.MAIN_RACE_ASSASSIN_W : ([344.2622, -16.3934, 150.0000], [0.0000, 0.0000, 147.3934]),
	player.MAIN_RACE_SURA_M : ([311.4753, -16.3934, 150.0000], [0.0000, 0.0000, 172.1804]),
	player.MAIN_RACE_SHAMAN_W : ([344.2622, -16.3934, 150.0000], [0.0000, 0.0000, 147.3934]),
	player.MAIN_RACE_WARRIOR_W : ([344.2622, -16.3934, 150.0000], [0.0000, 0.0000, 147.3934]),
	player.MAIN_RACE_ASSASSIN_M : ([344.2622, -16.3934, 150.0000], [0.0000, 0.0000, 156.7869]),
	player.MAIN_RACE_SURA_W : ([311.4753, -16.3934, 150.0000], [0.0000, 0.0000, 156.7869]),
	player.MAIN_RACE_SHAMAN_M : ([377.0492, -16.3934, 150.0000], [0.0000, 0.0000, 163.7869])
}

