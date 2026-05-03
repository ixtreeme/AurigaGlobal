import app
import chr
import chrmgr
import player
import net
import skill
import item
import effect
import constinfo
import localeinfo
import emotion

JOB_WARRIOR = 0
JOB_ASSASSIN = 1
JOB_SURA = 2
JOB_SHAMAN = 3

RACE_WARRIOR_M = 0
RACE_ASSASSIN_W = 1
RACE_SURA_M = 2
RACE_SHAMAN_W = 3
RACE_WARRIOR_W = 4
RACE_ASSASSIN_M = 5
RACE_SURA_W = 6
RACE_SHAMAN_M = 7

COMBO_TYPE_1 = 0
COMBO_TYPE_2 = 1
COMBO_TYPE_3 = 2

COMBO_INDEX_1 = 0
COMBO_INDEX_2 = 1
COMBO_INDEX_3 = 2
COMBO_INDEX_4 = 3
COMBO_INDEX_5 = 4
COMBO_INDEX_6 = 5

HORSE_SKILL_WILDATTACK = chr.MOTION_SKILL+121
HORSE_SKILL_CHARGE = chr.MOTION_SKILL+122
HORSE_SKILL_SPLASH = chr.MOTION_SKILL+123

GUILD_SKILL_DRAGONBLOOD = chr.MOTION_SKILL+101
GUILD_SKILL_DRAGONBLESS = chr.MOTION_SKILL+102
GUILD_SKILL_BLESSARMOR = chr.MOTION_SKILL+103
GUILD_SKILL_SPPEDUP = chr.MOTION_SKILL+104
GUILD_SKILL_DRAGONWRATH = chr.MOTION_SKILL+105
GUILD_SKILL_MAGICUP = chr.MOTION_SKILL+106

PASSIVE_GUILD_SKILL_INDEX_LIST = ( 151, )
ACTIVE_GUILD_SKILL_INDEX_LIST = ( 152, 153, 154, 155, 156, 157, )

SKILL_INDEX_DICT = []

def DefineSkillIndexDict():
	global SKILL_INDEX_DICT
	
	if app.ENABLE_NEW_PASSIVE_SKILLS:
		passiveBoost = (236, 240, 237, 241, 238, 242, 239, 243)
	else:
		passiveBoost = (0, 0, 0, 0, 0, 0, 0, 0)
	
	SKILL_INDEX_DICT = {
		JOB_WARRIOR : {
			1 : (1, 2, 3, 4, 5, 6, passiveBoost[0], 0, 137, 0, 138, 0, 139, 0,),
			2 : (16, 17, 18, 19, 20, 21, passiveBoost[1], 0, 137, 0, 138, 0, 139, 0,),
			"SUPPORT" : (122, 123, 121, 124, 125, 0, 0, 0, 130, 131, 0, 0,),
		},
		JOB_ASSASSIN : {
			1 : (31, 32, 33, 34, 35, 36, passiveBoost[2], 0, 137, 0, 138, 0, 139, 0, 140,),
			2 : (46, 47, 48, 49, 50, 51, passiveBoost[3], 0, 137, 0, 138, 0, 139, 0, 140,),
			"SUPPORT" : (122, 123, 121, 124, 125, 0, 0, 0, 130, 131, 0, 0,),
		},
		JOB_SURA : {
			1 : (61, 62, 63, 64, 65, 66, passiveBoost[4], 0, 137, 0, 138, 0, 139, 0,),
			2 : (76, 77, 78, 79, 80, 81, passiveBoost[5], 0, 137, 0, 138, 0, 139, 0,),
			"SUPPORT" : (122, 123, 121, 124, 125, 0, 0, 0, 130, 131, 0, 0,),
		},
		JOB_SHAMAN : {
			1 : (91, 92, 93, 94, 95, 96, passiveBoost[6], 0, 137, 0, 138, 0, 139, 0,),
			2 : (106, 107, 108, 109, 110, 111, passiveBoost[7], 0, 137, 0, 138, 0, 139, 0,),
			"SUPPORT" : (122, 123, 121, 124, 125, 0, 0, 0, 130, 131, 0, 0,),
		},
	}

def RegisterSkill(race, group, empire=0):
	DefineSkillIndexDict()
	
	job = chr.RaceToJob(race)
	if app.ENABLE_NEW_PASSIVE_SKILLS:
		passiveDefense = (221, 222, 223, 224, 225, 226, 227, 228)
		for i in xrange(len(passiveDefense)):
			player.SetSkill(i+221, passiveDefense[i])
	
	if SKILL_INDEX_DICT.has_key(job):
		if SKILL_INDEX_DICT[job].has_key(group):

			activeSkillList = SKILL_INDEX_DICT[job][group]

			for i in xrange(len(activeSkillList)):
				skillIndex = activeSkillList[i]

				## 7번 8번 스킬은 여기서 설정하면 안됨
				if i != 6 and i != 7 and not app.ENABLE_NEW_PASSIVE_SKILLS:
					player.SetSkill(i+1, skillIndex)
				else:
					player.SetSkill(i+1, skillIndex)

			if app.ENABLE_NEW_SECONDARY_SKILLS:
				supportSkillList = (122, 123, 121, 130, 131, 143, 144, 145, 146, 124, 0, 0,)
			else:
				supportSkillList = SKILL_INDEX_DICT[job]["SUPPORT"]

			for i in xrange(len(supportSkillList)):
				player.SetSkill(i+100+1, supportSkillList[i])

	## Language Skill
	if 0 != empire and not app.ENABLE_NEW_SECONDARY_SKILLS:
		languageSkillList = []
		for i in xrange(3):
			if (i+1) != empire:
				languageSkillList.append(player.SKILL_INDEX_LANGUAGE1+i)
		for i in xrange(len(languageSkillList)):
			player.SetSkill(107+i, languageSkillList[i])

	## Guild Skill
	for i in xrange(len(PASSIVE_GUILD_SKILL_INDEX_LIST)):
		player.SetSkill(200+i, PASSIVE_GUILD_SKILL_INDEX_LIST[i])

	for i in xrange(len(ACTIVE_GUILD_SKILL_INDEX_LIST)):
		player.SetSkill(210+i, ACTIVE_GUILD_SKILL_INDEX_LIST[i])

def RegisterSkillAt(race, group, pos, num):

	DefineSkillIndexDict()

	job = chr.RaceToJob(race)
	tmp = list(SKILL_INDEX_DICT[job][group])
	tmp[pos] = num
	SKILL_INDEX_DICT[job][group] = tuple(tmp)
	player.SetSkill(pos+1, num)

FACE_IMAGE_DICT = {
	RACE_WARRIOR_M	: "d:/ymir work/ui/game/windows/face_warrior.sub",
	RACE_ASSASSIN_W	: "d:/ymir work/ui/game/windows/face_assassin.sub",
	RACE_SURA_M	: "d:/ymir work/ui/game/windows/face_sura.sub",
	RACE_SHAMAN_W	: "d:/ymir work/ui/game/windows/face_shaman.sub",
}

isLoaded1 = 0
isLoaded2 = 0
isLoaded3 = 0
isLoaded4 = 0
isLoaded5 = 0
isLoaded6 = 0
isLoaded7 = 0
isLoaded8 = 0

def InitData():
	global isLoaded1
	if isLoaded1:
		return
	
	isLoaded1 = 1
	
	chrmgr.SetDustGap(250)
	chrmgr.SetHorseDustGap(500)
	
	chrmgr.CreateRace(RACE_WARRIOR_M)
	chrmgr.SelectRace(RACE_WARRIOR_M)
	chrmgr.LoadLocalRaceData("data/shapes/warrior_m.msm")
	SetIntroMotions(chr.MOTION_MODE_GENERAL, "d:/ymir work/pc/warrior/intro/")
	
	chrmgr.CreateRace(RACE_WARRIOR_W)
	chrmgr.SelectRace(RACE_WARRIOR_W)
	chrmgr.LoadLocalRaceData("data/shapes/warrior_w.msm")
	SetIntroMotions(chr.MOTION_MODE_GENERAL, "d:/ymir work/pc2/warrior/intro/")
	
	chrmgr.CreateRace(RACE_ASSASSIN_W)
	chrmgr.SelectRace(RACE_ASSASSIN_W)
	chrmgr.LoadLocalRaceData("data/shapes/assassin_w.msm")
	SetIntroMotions(chr.MOTION_MODE_GENERAL, "d:/ymir work/pc/assassin/intro/")
	
	chrmgr.CreateRace(RACE_ASSASSIN_M)
	chrmgr.SelectRace(RACE_ASSASSIN_M)
	chrmgr.LoadLocalRaceData("data/shapes/assassin_m.msm")
	SetIntroMotions(chr.MOTION_MODE_GENERAL, "d:/ymir work/pc2/assassin/intro/")
	
	chrmgr.CreateRace(RACE_SURA_M)
	chrmgr.SelectRace(RACE_SURA_M)
	chrmgr.LoadLocalRaceData("data/shapes/sura_m.msm")
	SetIntroMotions(chr.MOTION_MODE_GENERAL, "d:/ymir work/pc/sura/intro/")
	
	chrmgr.CreateRace(RACE_SURA_W)
	chrmgr.SelectRace(RACE_SURA_W)
	chrmgr.LoadLocalRaceData("data/shapes/sura_w.msm")
	SetIntroMotions(chr.MOTION_MODE_GENERAL, "d:/ymir work/pc2/sura/intro/")
	
	chrmgr.CreateRace(RACE_SHAMAN_W)
	chrmgr.SelectRace(RACE_SHAMAN_W)
	chrmgr.LoadLocalRaceData("data/shapes/shaman_w.msm")
	SetIntroMotions(chr.MOTION_MODE_GENERAL, "d:/ymir work/pc/shaman/intro/")
	
	chrmgr.CreateRace(RACE_SHAMAN_M)
	chrmgr.SelectRace(RACE_SHAMAN_M)
	chrmgr.LoadLocalRaceData("data/shapes/shaman_m.msm")
	SetIntroMotions(chr.MOTION_MODE_GENERAL, "d:/ymir work/pc2/shaman/intro/")

def LoadGameSound():
	global isLoaded2
	if isLoaded2:
		return
	
	isLoaded2 = 1
	
	item.SetUseSoundFileName(item.USESOUND_DEFAULT, "sound/ui/drop.wav")
	item.SetUseSoundFileName(item.USESOUND_ACCESSORY, "sound/ui/equip_ring_amulet.wav")
	item.SetUseSoundFileName(item.USESOUND_ARMOR, "sound/ui/equip_metal_armor.wav")
	item.SetUseSoundFileName(item.USESOUND_BOW, "sound/ui/equip_bow.wav")
	item.SetUseSoundFileName(item.USESOUND_WEAPON, "sound/ui/equip_metal_weapon.wav")
	item.SetUseSoundFileName(item.USESOUND_POTION, "sound/ui/eat_potion.wav")
	item.SetUseSoundFileName(item.USESOUND_PORTAL, "sound/ui/potal_scroll.wav")
	
	item.SetDropSoundFileName(item.DROPSOUND_DEFAULT, "sound/ui/drop.wav")
	item.SetDropSoundFileName(item.DROPSOUND_ACCESSORY, "sound/ui/equip_ring_amulet.wav")
	item.SetDropSoundFileName(item.DROPSOUND_ARMOR, "sound/ui/equip_metal_armor.wav")
	item.SetDropSoundFileName(item.DROPSOUND_BOW, "sound/ui/equip_bow.wav")
	item.SetDropSoundFileName(item.DROPSOUND_WEAPON, "sound/ui/equip_metal_weapon.wav")

def LoadGameEffect():
	global isLoaded3
	if isLoaded3:
		return
	
	isLoaded3 = 1
	
	chrmgr.RegisterCacheEffect(chrmgr.EFFECT_DUST, "", "d:/ymir work/effect/etc/dust/dust.mse")
	chrmgr.RegisterCacheEffect(chrmgr.EFFECT_HORSE_DUST, "", "d:/ymir work/effect/etc/dust/running_dust.mse")
	chrmgr.RegisterCacheEffect(chrmgr.EFFECT_HIT, "", "d:/ymir work/effect/hit/blow_1/blow_1_low.mse")
	chrmgr.RegisterCacheEffect(chrmgr.EFFECT_HPUP_RED, "", "d:/ymir work/effect/etc/recuperation/drugup_red.mse")
	chrmgr.RegisterCacheEffect(chrmgr.EFFECT_SPUP_BLUE, "", "d:/ymir work/effect/etc/recuperation/drugup_blue.mse")
	chrmgr.RegisterCacheEffect(chrmgr.EFFECT_SPEEDUP_GREEN, "", "d:/ymir work/effect/etc/recuperation/drugup_green.mse")
	chrmgr.RegisterCacheEffect(chrmgr.EFFECT_DXUP_PURPLE, "", "d:/ymir work/effect/etc/recuperation/drugup_purple.mse")
	chrmgr.RegisterCacheEffect(chrmgr.EFFECT_AUTO_HPUP, "", "d:/ymir work/effect/etc/recuperation/autodrugup_red.mse")
	chrmgr.RegisterCacheEffect(chrmgr.EFFECT_AUTO_SPUP, "", "d:/ymir work/effect/etc/recuperation/autodrugup_blue.mse")
	chrmgr.RegisterCacheEffect(chrmgr.EFFECT_RAMADAN_RING_EQUIP, "", "d:/ymir work/effect/etc/buff/buff_item1.mse")
	chrmgr.RegisterCacheEffect(chrmgr.EFFECT_HALLOWEEN_CANDY_EQUIP, "", "d:/ymir work/effect/etc/buff/buff_item2.mse")
	chrmgr.RegisterCacheEffect(chrmgr.EFFECT_HAPPINESS_RING_EQUIP, "", "d:/ymir work/effect/etc/buff/buff_item3.mse")
	chrmgr.RegisterCacheEffect(chrmgr.EFFECT_LOVE_PENDANT_EQUIP, "", "d:/ymir work/effect/etc/buff/buff_item4.mse")
	if app.ENABLE_ACCE_SYSTEM:
		chrmgr.RegisterCacheEffect(chrmgr.EFFECT_ACCE_SUCCEDED, "", "d:/ymir work/effect/etc/buff/buff_item6.mse")
		chrmgr.RegisterCacheEffect(chrmgr.EFFECT_ACCE_EQUIP, "", "d:/ymir work/effect/etc/buff/buff_item7.mse")
	
	if app.VERSION_162_ENABLED:
		chrmgr.RegisterCacheEffect(chrmgr.EFFECT_HEALER, "", "d:/ymir work/effect/monster2/healer/healer_effect.mse")
	
	if app.ENABLE_TALISMAN_EFFECT:
		chrmgr.RegisterCacheEffect(chrmgr.EFFECT_TALISMAN_EQUIP_FIRE, "", "d:/ymir work/talisman/effect/talismano_fuoco.mse")
		chrmgr.RegisterCacheEffect(chrmgr.EFFECT_TALISMAN_EQUIP_ICE, "", "d:/ymir work/talisman/effect/talismano_ghiaccio.mse")
		chrmgr.RegisterCacheEffect(chrmgr.EFFECT_TALISMAN_EQUIP_WIND, "", "d:/ymir work/talisman/effect/talismano_vento.mse")
		chrmgr.RegisterCacheEffect(chrmgr.EFFECT_TALISMAN_EQUIP_EARTH, "", "d:/ymir work/talisman/effect/talismano_terra.mse")	
		chrmgr.RegisterCacheEffect(chrmgr.EFFECT_TALISMAN_EQUIP_DARK, "", "d:/ymir work/talisman/effect/talismano_oscuro.mse")
		chrmgr.RegisterCacheEffect(chrmgr.EFFECT_TALISMAN_EQUIP_ELEC, "", "d:/ymir work/talisman/effect/talismano_lampo.mse")
	
	if app.EFFECT_MANTELLO:
		chrmgr.RegisterCacheEffect(chrmgr.EFFECT_MANTELLO, "", "d:/ymir work/effect/etc/buff/buff_item9_cape.mse")
	
	chrmgr.RegisterCacheEffect(chrmgr.EFFECT_PENETRATE, "Bip01", "d:/ymir work/effect/hit/gwantong.mse")
	chrmgr.RegisterCacheEffect(chrmgr.EFFECT_FIRECRACKER, "", "d:/ymir work/effect/etc/firecracker/newyear_firecracker.mse")
	chrmgr.RegisterCacheEffect(chrmgr.EFFECT_SPIN_TOP, "", "d:/ymir work/effect/etc/firecracker/paing_i.mse")
	chrmgr.RegisterCacheEffect(chrmgr.EFFECT_SELECT, "", "d:/ymir work/effect/etc/click/click_select.mse")
	chrmgr.RegisterCacheEffect(chrmgr.EFFECT_TARGET, "", "d:/ymir work/effect/etc/click/click_glow_select.mse")
	chrmgr.RegisterCacheEffect(chrmgr.EFFECT_STUN, "Bip01 Head", "d:/ymir work/effect/etc/stun/stun.mse")
	chrmgr.RegisterCacheEffect(chrmgr.EFFECT_CRITICAL, "Bip01 R Hand", "d:/ymir work/effect/hit/critical.mse")
	player.RegisterCacheEffect(player.EFFECT_PICK, "d:/ymir work/effect/etc/click/click.mse")
	chrmgr.RegisterCacheEffect(chrmgr.EFFECT_DAMAGE_TARGET, "", "d:/ymir work/effect/affect/damagevalue/target.mse")
	chrmgr.RegisterCacheEffect(chrmgr.EFFECT_DAMAGE_NOT_TARGET, "", "d:/ymir work/effect/affect/damagevalue/nontarget.mse")
	chrmgr.RegisterCacheEffect(chrmgr.EFFECT_DAMAGE_SELFDAMAGE, "", "d:/ymir work/effect/affect/damagevalue/damage.mse")
	chrmgr.RegisterCacheEffect(chrmgr.EFFECT_DAMAGE_SELFDAMAGE2, "", "d:/ymir work/effect/affect/damagevalue/damage_1.mse")
	chrmgr.RegisterCacheEffect(chrmgr.EFFECT_DAMAGE_POISON, "", "d:/ymir work/effect/affect/damagevalue/poison.mse")
	chrmgr.RegisterCacheEffect(chrmgr.EFFECT_DAMAGE_MISS, "", "d:/ymir work/effect/affect/damagevalue/miss.mse")
	chrmgr.RegisterCacheEffect(chrmgr.EFFECT_DAMAGE_TARGETMISS, "", "d:/ymir work/effect/affect/damagevalue/target_miss.mse")
	chrmgr.RegisterCacheEffect(chrmgr.EFFECT_DAMAGE_CRITICAL, "", "d:/ymir work/effect/affect/damagevalue/critical.mse")
	chrmgr.RegisterCacheEffect(chrmgr.EFFECT_LEVELUP_ON_14_FOR_GERMANY, "","season1/effect/paymessage_warning.mse")
	chrmgr.RegisterCacheEffect(chrmgr.EFFECT_LEVELUP_UNDER_15_FOR_GERMANY, "", "season1/effect/paymessage_decide.mse")
	chrmgr.RegisterCacheEffect(chrmgr.EFFECT_PERCENT_DAMAGE1, "", "d:/ymir work/effect/hit/percent_damage1.mse")
	chrmgr.RegisterCacheEffect(chrmgr.EFFECT_PERCENT_DAMAGE2, "", "d:/ymir work/effect/hit/percent_damage2.mse")
	chrmgr.RegisterCacheEffect(chrmgr.EFFECT_PERCENT_DAMAGE3, "", "d:/ymir work/effect/hit/percent_damage3.mse")
	if app.ENABLE_NEW_GYEONGGONG_SKILL:
		chrmgr.RegisterEffect(chrmgr.EFFECT_GYEONGGONG_BOOM, "Bip01", "d:/ymir work/effect/hit/gyeonggong_boom.mse")
	
	if app.ENABLE_RUNE_SYSTEM:
		chrmgr.RegisterEffect(chrmgr.EFFECT_RUNE, "Bip01", "d:/ymir work/effect/rune/ridack_effect.mse")
	
	chrmgr.RegisterEffect(chrmgr.EFFECT_SPAWN_APPEAR, "Bip01", "d:/ymir work/effect/etc/appear_die/monster_appear.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_SPAWN_DISAPPEAR, "Bip01", "d:/ymir work/effect/etc/appear_die/monster_die.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_FLAME_ATTACK, "equip_right_hand", "d:/ymir work/effect/hit/blow_flame/flame_3_weapon.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_FLAME_HIT, "", "d:/ymir work/effect/hit/blow_flame/flame_3_blow.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_FLAME_ATTACH, "", "d:/ymir work/effect/hit/blow_flame/flame_3_body.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_ELECTRIC_ATTACK, "equip_right", "d:/ymir work/effect/hit/blow_electric/light_1_weapon.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_ELECTRIC_HIT, "", "d:/ymir work/effect/hit/blow_electric/light_1_blow.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_ELECTRIC_ATTACH, "", "d:/ymir work/effect/hit/blow_electric/light_1_body.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_LEVELUP, "", "d:/ymir work/effect/etc/levelup_1/level_up.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_SKILLUP, "", "d:/ymir work/effect/etc/skillup/skillup_1.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_EMPIRE+1, "Bip01", "d:/ymir work/effect/etc/empire/empire_A.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_EMPIRE+2, "Bip01", "d:/ymir work/effect/etc/empire/empire_B.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_EMPIRE+3, "Bip01", "d:/ymir work/effect/etc/empire/empire_C.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_WEAPON+1, "equip_right_hand", "d:/ymir work/pc/warrior/effect/geom_sword_loop.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_WEAPON+2, "equip_right_hand", "d:/ymir work/pc/warrior/effect/geom_spear_loop.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_AFFECT+0, "Bip01", localeinfo.FN_GM_MARK)
	chrmgr.RegisterEffect(chrmgr.EFFECT_AFFECT+3, "Bip01", "d:/ymir work/effect/hit/blow_poison/poison_loop.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_AFFECT+4, "", "d:/ymir work/effect/affect/slow.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_AFFECT+5, "Bip01 Head", "d:/ymir work/effect/etc/stun/stun_loop.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_AFFECT+6, "", "d:/ymir work/effect/etc/ready/ready.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_AFFECT+16, "", "d:/ymir work/pc/warrior/effect/gyeokgongjang_loop.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_AFFECT+17, "", "d:/ymir work/pc/assassin/effect/gyeonggong_loop.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_AFFECT+19, "Bip01 R Finger2", "d:/ymir work/pc/sura/effect/gwigeom_loop.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_AFFECT+20, "", "d:/ymir work/pc/sura/effect/fear_loop.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_AFFECT+21, "", "d:/ymir work/pc/sura/effect/jumagap_loop.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_AFFECT+22, "", "d:/ymir work/pc/shaman/effect/3hosin_loop.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_AFFECT+23, "", "d:/ymir work/pc/shaman/effect/boho_loop.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_AFFECT+24, "", "d:/ymir work/pc/shaman/effect/10kwaesok_loop.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_AFFECT+25, "", "d:/ymir work/pc/sura/effect/heuksin_loop.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_AFFECT+26, "", "d:/ymir work/pc/sura/effect/muyeong_loop.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_AFFECT+28, "Bip01", "d:/ymir work/effect/hit/blow_flame/flame_loop.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_AFFECT+29, "Bip01 R Hand", "d:/ymir work/pc/shaman/effect/6gicheon_hand.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_AFFECT+30, "Bip01 L Hand", "d:/ymir work/pc/shaman/effect/jeungryeok_hand.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_AFFECT+32, "Bip01 Head", "d:/ymir work/pc/sura/effect/pabeop_loop.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_AFFECT+33, "", "d:/ymir work/pc/warrior/effect/gyeokgongjang_loop.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_AFFECT+35, "", "d:/ymir work/effect/etc/guild_war_flag/flag_red.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_AFFECT+36, "", "d:/ymir work/effect/etc/guild_war_flag/flag_blue.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_AFFECT+37, "", "d:/ymir work/effect/etc/guild_war_flag/flag_yellow.mse")
	if app.ENABLE_MELEY_LAIR_DUNGEON:
		chrmgr.RegisterEffect(chrmgr.EFFECT_AFFECT+46, "", "d:/ymir work/effect/monster2/redd_moojuk.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_AFFECT+47, "", "d:/ymir work/effect/monster2/redd_moojuk.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_AFFECT+48, "", "d:/ymir work/effect/monster2/redd_moojuk_blue.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_AFFECT+49, "", "d:/ymir work/effect/monster2/redd_moojuk_green.mse")
	
	chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+1, "PART_WEAPON", "d:/ymir work/pc/common/effect/sword/sword_7.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+2, "PART_WEAPON", "d:/ymir work/pc/common/effect/sword/sword_8.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+3, "PART_WEAPON", "d:/ymir work/pc/common/effect/sword/sword_9.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+4, "PART_WEAPON_LEFT", "d:/ymir work/pc/common/effect/sword/sword_7_b.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+5, "PART_WEAPON_LEFT", "d:/ymir work/pc/common/effect/sword/sword_8_b.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+6, "PART_WEAPON_LEFT", "d:/ymir work/pc/common/effect/sword/sword_9_b.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+7, "PART_WEAPON", "d:/ymir work/pc/common/effect/sword/sword_7_f.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+8, "PART_WEAPON", "d:/ymir work/pc/common/effect/sword/sword_8_f.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+9, "PART_WEAPON", "d:/ymir work/pc/common/effect/sword/sword_9_f.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+10, "PART_WEAPON", "d:/ymir work/pc/common/effect/sword/sword_7_s.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+11, "PART_WEAPON", "d:/ymir work/pc/common/effect/sword/sword_8_s.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+12, "PART_WEAPON", "d:/ymir work/pc/common/effect/sword/sword_9_s.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+13, "PART_WEAPON_LEFT", "d:/ymir work/pc/common/effect/sword/sword_7_s.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+14, "PART_WEAPON_LEFT", "d:/ymir work/pc/common/effect/sword/sword_8_s.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+15, "PART_WEAPON_LEFT", "d:/ymir work/pc/common/effect/sword/sword_9_s.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+16, "Bip01", "d:/ymir work/pc/common/effect/armor/armor_7.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+17, "Bip01", "d:/ymir work/pc/common/effect/armor/armor_8.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+18, "Bip01", "d:/ymir work/pc/common/effect/armor/armor_9.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+19, "Bip01", "d:/ymir work/pc/common/effect/armor/armor-4-2-1.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+20, "Bip01", "d:/ymir work/pc/common/effect/armor/armor-4-2-2.mse")
	chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+23, "Bip01", "D:/ymir work/effect/wings/5.mse")
	if app.ENABLE_LVL115_ARMOR_EFFECT:
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+21, "Bip01", "d:/ymir work/pc/common/effect/armor/armor-5-1.mse")
	
	if app.__ENABLE_NEW_EFFECT_CIANITE__:
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+26, "Bip01", "d:/ymir work/effect/armatura/ridack_armor1.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+27, "Bip01", "d:/ymir work/effect/armatura/ridack_armor2.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+28, "Bip01", "d:/ymir work/effect/armatura/ridack_armor3.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+29, "Bip01", "d:/ymir work/effect/armatura/ridack_armor4.mse")
	
	if app.__ENABLE_NEW_EFFECT_CIANITE_WEAPON__:
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+30, "PART_WEAPON", "d:/ymir work/effect/spada_lama/ridack_sword.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+31, "PART_WEAPON", "d:/ymir work/effect/spada_lama/ridack_sword2.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+32, "PART_WEAPON", "d:/ymir work/effect/spada_lama/ridack_sword3.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+33, "PART_WEAPON", "d:/ymir work/effect/spada_lama/ridack_sword4.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+34, "PART_WEAPON", "d:/ymir work/effect/spadone/ridack_2hand1.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+35, "PART_WEAPON", "d:/ymir work/effect/spadone/ridack_2hand2.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+36, "PART_WEAPON", "d:/ymir work/effect/spadone/ridack_2hand3.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+37, "PART_WEAPON", "d:/ymir work/effect/spadone/ridack_2hand4.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+38, "PART_WEAPON", "d:/ymir work/effect/campana/ridack_bell1.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+39, "PART_WEAPON", "d:/ymir work/effect/campana/ridack_bell2.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+40, "PART_WEAPON", "d:/ymir work/effect/campana/ridack_bell3.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+41, "PART_WEAPON", "d:/ymir work/effect/campana/ridack_bell4.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+42, "PART_WEAPON", "d:/ymir work/effect/ventaglio/ridack_fan1.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+43, "PART_WEAPON", "d:/ymir work/effect/ventaglio/ridack_fan2.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+44, "PART_WEAPON", "d:/ymir work/effect/ventaglio/ridack_fan3.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+45, "PART_WEAPON", "d:/ymir work/effect/ventaglio/ridack_fan4.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+46, "PART_WEAPON_LEFT", "d:/ymir work/effect/arco/ridack_sword_bow1.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+47, "PART_WEAPON_LEFT", "d:/ymir work/effect/arco/ridack_sword_bow2.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+48, "PART_WEAPON_LEFT", "d:/ymir work/effect/arco/ridack_sword_bow3.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+49, "PART_WEAPON_LEFT", "d:/ymir work/effect/arco/ridack_sword_bow4.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+50, "PART_WEAPON", "d:/ymir work/effect/coltello/ridack_knife1.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+51, "PART_WEAPON", "d:/ymir work/effect/coltello/ridack_knife2.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+52, "PART_WEAPON", "d:/ymir work/effect/coltello/ridack_knife3.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+53, "PART_WEAPON", "d:/ymir work/effect/coltello/ridack_knife4.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+54, "PART_WEAPON_LEFT", "d:/ymir work/effect/coltello/ridack_knife1.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+55, "PART_WEAPON_LEFT", "d:/ymir work/effect/coltello/ridack_knife2.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+56, "PART_WEAPON_LEFT", "d:/ymir work/effect/coltello/ridack_knife3.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+57, "PART_WEAPON_LEFT", "d:/ymir work/effect/coltello/ridack_knife4.mse")
		if app.__ENABLE_NEW_EFFECT_STOLE__:
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+68, "Bip01", "d:/ymir work/effect/stola/ridack_greensash.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+69, "Bip01", "d:/ymir work/effect/stola/ridack_blacksash.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+70, "Bip01", "d:/ymir work/effect/stola/ridack_orangesash.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+71, "Bip01", "d:/ymir work/effect/stola/ridack_purplesash.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+72, "Bip01", "d:/ymir work/effect/stola/ridack_redsash.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+73, "Bip01", "d:/ymir work/effect/stola/ridack_whitesash.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+74, "Bip01", "d:/ymir work/effect/stola/ridack_yellowsash.mse")
		
		if app.ENABLE_ACCE_SYSTEM:
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED + 75, "Bip01", "d:/ymir work/pc/common/effect/armor/acc_01.mse")
		
		if app.ENABLE_COSTUME_EFFECT:
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED + 76, "Bip01", "d:/ymir work/effect/shining/nero/ridack_armor_black.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED + 77, "PART_WEAPON", "d:/ymir work/effect/shining/nero/ridack_sword_black.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED + 78, "PART_WEAPON", "d:/ymir work/effect/shining/nero/ridack_spear_black.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED + 79, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining/nero/ridack_knife_black.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED + 80, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining/nero/ridack_bow_black.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED + 81, "PART_WEAPON", "d:/ymir work/effect/shining/nero/ridack_bell_black.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED + 82, "PART_WEAPON", "d:/ymir work/effect/shining/nero/ridack_fan_black.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED + 83, "PART_WEAPON", "d:/ymir work/effect/shining/nero/ridack_knife_black.mse")
		
		if app.__ENABLE_NEW_EFFECT_STOLE__:
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+84, "Bip01", "d:/ymir work/effect/stola/ridack_effect1.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+85, "Bip01", "d:/ymir work/effect/stola/ridack_effect2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+86, "Bip01", "d:/ymir work/effect/stola/ridack_effect3.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+87, "Bip01", "d:/ymir work/effect/stola/ridack_effect4.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+88, "Bip01", "d:/ymir work/effect/stola/ridack_effect5.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+89, "Bip01", "d:/ymir work/effect/stola/ridack_effect6.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+90, "Bip01 Spine2", "d:/ymir work/effect/stola/ridack_wingblack.mse")
		
		if app.__ENABLE_NEW_EFFECT_ZODIACO__:
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+91, "Bip01", "d:/ymir work/effect/armatura1/ridack_armor.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+92, "Bip01", "d:/ymir work/effect/armatura1/ridack_armor2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+93, "Bip01", "d:/ymir work/effect/armatura1/ridack_armor3.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+94, "Bip01", "d:/ymir work/effect/armatura1/ridack_armor4.mse")
		
		if app.__ENABLE_NEW_EFFECT_ZODIACO_WEAPON__:
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+95, "PART_WEAPON", "d:/ymir work/effect/spada_lama1/ridack_sword.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+96, "PART_WEAPON", "d:/ymir work/effect/spada_lama1/ridack_sword2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+97, "PART_WEAPON", "d:/ymir work/effect/spada_lama1/ridack_sword3.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+98, "PART_WEAPON", "d:/ymir work/effect/spada_lama1/ridack_sword4.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+99, "PART_WEAPON", "d:/ymir work/effect/spadone1/ridack_spear.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+100, "PART_WEAPON", "d:/ymir work/effect/spadone1/ridack_spear2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+101, "PART_WEAPON", "d:/ymir work/effect/spadone1/ridack_spear3.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+102, "PART_WEAPON", "d:/ymir work/effect/spadone1/ridack_spear4.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+103, "PART_WEAPON", "d:/ymir work/effect/campana1/ridack_bell.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+104, "PART_WEAPON", "d:/ymir work/effect/campana1/ridack_bell2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+105, "PART_WEAPON", "d:/ymir work/effect/campana1/ridack_bell3.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+106, "PART_WEAPON", "d:/ymir work/effect/campana1/ridack_bell4.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+107, "PART_WEAPON", "d:/ymir work/effect/ventaglio1/ridack_fan.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+108, "PART_WEAPON", "d:/ymir work/effect/ventaglio1/ridack_fan2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+109, "PART_WEAPON", "d:/ymir work/effect/ventaglio1/ridack_fan3.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+110, "PART_WEAPON", "d:/ymir work/effect/ventaglio1/ridack_fan4.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+111, "PART_WEAPON_LEFT", "d:/ymir work/effect/arco1/ridack_bow.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+112, "PART_WEAPON_LEFT", "d:/ymir work/effect/arco1/ridack_bow2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+113, "PART_WEAPON_LEFT", "d:/ymir work/effect/arco1/ridack_bow3.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+114, "PART_WEAPON_LEFT", "d:/ymir work/effect/arco1/ridack_bow4.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+115, "PART_WEAPON", "d:/ymir work/effect/coltello1/ridack_knife.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+116, "PART_WEAPON", "d:/ymir work/effect/coltello1/ridack_knife2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+117, "PART_WEAPON", "d:/ymir work/effect/coltello1/ridack_knife3.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+118, "PART_WEAPON", "d:/ymir work/effect/coltello1/ridack_knife4.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+119, "PART_WEAPON_LEFT", "d:/ymir work/effect/coltello1/ridack_knife.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+120, "PART_WEAPON_LEFT", "d:/ymir work/effect/coltello1/ridack_knife2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+121, "PART_WEAPON_LEFT", "d:/ymir work/effect/coltello1/ridack_knife3.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+122, "PART_WEAPON_LEFT", "d:/ymir work/effect/coltello1/ridack_knife4.mse")
		
		if app.__ENABLE_NEW_EFFECT_ZODIACO1__:
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+123, "Bip01", "d:/ymir work/effect/armatura2/ridack_armor.mse")
		
		if app.__ENABLE_NEW_EFFECT_ZODIACO_WEAPON1__:
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+124, "PART_WEAPON", "d:/ymir work/effect/zodiaco/ridack_sword.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+125, "PART_WEAPON", "d:/ymir work/effect/zodiaco/ridack_spear.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+126, "PART_WEAPON", "d:/ymir work/effect/zodiaco/ridack_bell.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+127, "PART_WEAPON", "d:/ymir work/effect/zodiaco/ridack_fan.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+128, "PART_WEAPON_LEFT", "d:/ymir work/effect/zodiaco/ridack_bow.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+129, "PART_WEAPON", "d:/ymir work/effect/zodiaco/ridack_knife.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+130, "PART_WEAPON_LEFT", "d:/ymir work/effect/zodiaco/ridack_knife.mse")
		
		if app.ENABLE_COSTUME_EFFECT:
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+131, "Bip01", "d:/ymir work/effect/shining/viola/ridack_armor_fushiia.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+132, "PART_WEAPON", "d:/ymir work/effect/shining/viola/ridack_sword_fushiia.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+133, "PART_WEAPON", "d:/ymir work/effect/shining/viola/ridack_spear_fushiia.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+134, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining/viola/ridack_knife_fushiia.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+135, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining/viola/ridack_bow_fushiia.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+136, "PART_WEAPON", "d:/ymir work/effect/shining/viola/ridack_bell_fushiia.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+137, "PART_WEAPON", "d:/ymir work/effect/shining/viola/ridack_fan_fushiia.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+138, "PART_WEAPON", "d:/ymir work/effect/shining/viola/ridack_knife_fushiia.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+139, "Bip01", "d:/ymir work/effect/shining/orange/ridack_armor_orange.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+140, "PART_WEAPON", "d:/ymir work/effect/shining/orange/ridack_sword_orange.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+141, "PART_WEAPON", "d:/ymir work/effect/shining/orange/ridack_spear_orange.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+142, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining/orange/ridack_knife_orange.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+143, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining/orange/ridack_bow_orange.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+144, "PART_WEAPON", "d:/ymir work/effect/shining/orange/ridack_bell_orange.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+145, "PART_WEAPON", "d:/ymir work/effect/shining/orange/ridack_fan_orange.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+146, "PART_WEAPON", "d:/ymir work/effect/shining/orange/ridack_knife_orange.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+147, "Bip01", "d:/ymir work/effect/shining/verdino/ridack_armor_jade.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+148, "PART_WEAPON", "d:/ymir work/effect/shining/verdino/ridack_sword_jade.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+149, "PART_WEAPON", "d:/ymir work/effect/shining/verdino/ridack_spear_jade.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+150, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining/verdino/ridack_knife_jade.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+151, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining/verdino/ridack_bow_jade.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+152, "PART_WEAPON", "d:/ymir work/effect/shining/verdino/ridack_bell_jade.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+153, "PART_WEAPON", "d:/ymir work/effect/shining/verdino/ridack_fan_jade.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+154, "PART_WEAPON", "d:/ymir work/effect/shining/verdino/ridack_knife_jade.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+155, "Bip01", "d:/ymir work/effect/shining1/viola/ridack_armor_purple2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+156, "PART_WEAPON", "d:/ymir work/effect/shining1/viola/ridack_sword_purple2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+157, "PART_WEAPON", "d:/ymir work/effect/shining1/viola/ridack_spear_purple2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+158, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining1/viola/ridack_knife_purple2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+159, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining1/viola/ridack_bow_purple2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+160, "PART_WEAPON", "d:/ymir work/effect/shining1/viola/ridack_bell_purple2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+161, "PART_WEAPON", "d:/ymir work/effect/shining1/viola/ridack_fan_purple2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+162, "PART_WEAPON", "d:/ymir work/effect/shining1/viola/ridack_knife_purple2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+163, "Bip01", "d:/ymir work/effect/shining1/blu/ridack_armor_blue2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+164, "PART_WEAPON", "d:/ymir work/effect/shining1/blu/ridack_sword_blue2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+165, "PART_WEAPON", "d:/ymir work/effect/shining1/blu/ridack_spear_blue2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+166, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining1/blu/ridack_knife_blue2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+167, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining1/blu/ridack_bow_blue2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+168, "PART_WEAPON", "d:/ymir work/effect/shining1/blu/ridack_bell_blue2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+169, "PART_WEAPON", "d:/ymir work/effect/shining1/blu/ridack_fan_blue2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+170, "PART_WEAPON", "d:/ymir work/effect/shining1/blu/ridack_knife_blue2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+171, "Bip01", "d:/ymir work/effect/shining1/rosa/ridack_armor_pink2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+172, "PART_WEAPON", "d:/ymir work/effect/shining1/rosa/ridack_sword_pink2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+173, "PART_WEAPON", "d:/ymir work/effect/shining1/rosa/ridack_spear_pink2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+174, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining1/rosa/ridack_knife_pink2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+175, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining1/rosa/ridack_bow_pink2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+176, "PART_WEAPON", "d:/ymir work/effect/shining1/rosa/ridack_bell_pink2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+177, "PART_WEAPON", "d:/ymir work/effect/shining1/rosa/ridack_fan_pink2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+178, "PART_WEAPON", "d:/ymir work/effect/shining1/rosa/ridack_knife_pink2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+179, "Bip01", "d:/ymir work/effect/shining1/bianco/ridack_armor_white2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+180, "PART_WEAPON", "d:/ymir work/effect/shining1/bianco/ridack_sword_white2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+181, "PART_WEAPON", "d:/ymir work/effect/shining1/bianco/ridack_spear_white2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+182, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining1/bianco/ridack_knife_white2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+183, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining1/bianco/ridack_bow_white2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+184, "PART_WEAPON", "d:/ymir work/effect/shining1/bianco/ridack_bell_white2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+185, "PART_WEAPON", "d:/ymir work/effect/shining1/bianco/ridack_fan_white2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+186, "PART_WEAPON", "d:/ymir work/effect/shining1/bianco/ridack_knife_white2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+187, "Bip01", "d:/ymir work/effect/shining2/verde/ridack_armor_green.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+188, "PART_WEAPON", "d:/ymir work/effect/shining2/verde/ridack_sword_green.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+189, "PART_WEAPON", "d:/ymir work/effect/shining2/verde/ridack_spear_green.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+190, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining2/verde/ridack_knife_green.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+191, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining2/verde/ridack_bow_green.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+192, "PART_WEAPON", "d:/ymir work/effect/shining2/verde/ridack_bell_green.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+193, "PART_WEAPON", "d:/ymir work/effect/shining2/verde/ridack_fan_green.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+194, "PART_WEAPON", "d:/ymir work/effect/shining2/verde/ridack_knife_green.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+195, "Bip01", "d:/ymir work/effect/shining2/rosso/ridack_armor_red.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+196, "PART_WEAPON", "d:/ymir work/effect/shining2/rosso/ridack_sword_red.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+197, "PART_WEAPON", "d:/ymir work/effect/shining2/rosso/ridack_spear_red.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+198, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining2/rosso/ridack_knife_red.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+199, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining2/rosso/ridack_bow_red.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+200, "PART_WEAPON", "d:/ymir work/effect/shining2/rosso/ridack_bell_red.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+201, "PART_WEAPON", "d:/ymir work/effect/shining2/rosso/ridack_fan_red.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+202, "PART_WEAPON", "d:/ymir work/effect/shining2/rosso/ridack_knife_red.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+203, "Bip01", "d:/ymir work/effect/shining2/blu/ridack_armor_blue.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+204, "PART_WEAPON", "d:/ymir work/effect/shining2/blu/ridack_sword_blue.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+205, "PART_WEAPON", "d:/ymir work/effect/shining2/blu/ridack_spear_blue.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+206, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining2/blu/ridack_knife_blue.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+207, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining2/blu/ridack_bow_blue.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+208, "PART_WEAPON", "d:/ymir work/effect/shining2/blu/ridack_bell_blue.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+209, "PART_WEAPON", "d:/ymir work/effect/shining2/blu/ridack_fan_blue.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+210, "PART_WEAPON", "d:/ymir work/effect/shining2/blu/ridack_knife_blue.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+211, "Bip01", "d:/ymir work/effect/shining2/giallo/ridack_armor_yellow.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+212, "PART_WEAPON", "d:/ymir work/effect/shining2/giallo/ridack_sword_yellow.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+213, "PART_WEAPON", "d:/ymir work/effect/shining2/giallo/ridack_spear_yellow.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+214, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining2/giallo/ridack_knife_yellow.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+215, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining2/giallo/ridack_bow_yellow.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+216, "PART_WEAPON", "d:/ymir work/effect/shining2/giallo/ridack_bell_yellow.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+217, "PART_WEAPON", "d:/ymir work/effect/shining2/giallo/ridack_fan_yellow.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+218, "PART_WEAPON", "d:/ymir work/effect/shining2/giallo/ridack_knife_yellow.mse")
		
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+219, "PART_WEAPON_LEFT", "d:/ymir work/pc/common/effect/sword/sword_7_f.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+220, "PART_WEAPON_LEFT", "d:/ymir work/pc/common/effect/sword/sword_8_f.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+221, "PART_WEAPON_LEFT", "d:/ymir work/pc/common/effect/sword/sword_9_f.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+222, "PART_WEAPON_LEFT", "d:/ymir work/effect/ventaglio/ridack_fan1.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+223, "PART_WEAPON_LEFT", "d:/ymir work/effect/ventaglio/ridack_fan2.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+224, "PART_WEAPON_LEFT", "d:/ymir work/effect/ventaglio/ridack_fan3.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+225, "PART_WEAPON_LEFT", "d:/ymir work/effect/ventaglio/ridack_fan4.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+226, "PART_WEAPON_LEFT", "d:/ymir work/effect/ventaglio1/ridack_fan.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+227, "PART_WEAPON_LEFT", "d:/ymir work/effect/ventaglio1/ridack_fan2.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+228, "PART_WEAPON_LEFT", "d:/ymir work/effect/ventaglio1/ridack_fan3.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+229, "PART_WEAPON_LEFT", "d:/ymir work/effect/ventaglio1/ridack_fan4.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+230, "PART_WEAPON_LEFT", "d:/ymir work/effect/zodiaco/ridack_fan.mse")
		if app.ENABLE_COSTUME_EFFECT:
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+231, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining/nero/ridack_fan_black.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+232, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining/viola/ridack_fan_fushiia.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+233, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining/orange/ridack_fan_orange.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+234, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining/verdino/ridack_fan_jade.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+235, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining1/viola/ridack_fan_purple2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+236, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining1/blu/ridack_fan_blue2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+237, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining1/rosa/ridack_fan_pink2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+238, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining1/bianco/ridack_fan_white2.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+239, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining2/verde/ridack_fan_green.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+240, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining2/rosso/ridack_fan_red.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+241, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining2/blu/ridack_fan_blue.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+242, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining2/giallo/ridack_fan_yellow.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+731, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining/nero/ridack_fan_black_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+732, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining/viola/ridack_fan_fushiia_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+733, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining/orange/ridack_fan_orange_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+734, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining/verdino/ridack_fan_jade_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+735, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining1/viola/ridack_fan_purple2_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+736, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining1/blu/ridack_fan_blue2_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+737, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining1/rosa/ridack_fan_pink2_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+738, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining1/bianco/ridack_fan_white2_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+739, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining2/verde/ridack_fan_green_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+740, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining2/rosso/ridack_fan_red_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+741, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining2/blu/ridack_fan_blue_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+742, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining2/giallo/ridack_fan_yellow_special.mse")
		
		if app.ENABLE_COSTUME_EFFECT:
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+579, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining/nero/ridack_knife_black_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+582, "PART_WEAPON", "d:/ymir work/effect/shining/nero/ridack_fan_black_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+583, "PART_WEAPON", "d:/ymir work/effect/shining/nero/ridack_knife_black_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+634, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining/viola/ridack_knife_fushiia_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+637, "PART_WEAPON", "d:/ymir work/effect/shining/viola/ridack_fan_fushiia_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+638, "PART_WEAPON", "d:/ymir work/effect/shining/viola/ridack_knife_fushiia_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+642, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining/orange/ridack_knife_orange_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+645, "PART_WEAPON", "d:/ymir work/effect/shining/orange/ridack_fan_orange_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+646, "PART_WEAPON", "d:/ymir work/effect/shining/orange/ridack_knife_orange_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+650, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining/verdino/ridack_knife_jade_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+653, "PART_WEAPON", "d:/ymir work/effect/shining/verdino/ridack_fan_jade_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+654, "PART_WEAPON", "d:/ymir work/effect/shining/verdino/ridack_knife_jade_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+658, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining1/viola/ridack_knife_purple2_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+661, "PART_WEAPON", "d:/ymir work/effect/shining1/viola/ridack_fan_purple2_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+662, "PART_WEAPON", "d:/ymir work/effect/shining1/viola/ridack_knife_purple2_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+666, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining1/blu/ridack_knife_blue2_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+669, "PART_WEAPON", "d:/ymir work/effect/shining1/blu/ridack_fan_blue2_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+670, "PART_WEAPON", "d:/ymir work/effect/shining1/blu/ridack_knife_blue2_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+674, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining1/rosa/ridack_knife_pink2_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+677, "PART_WEAPON", "d:/ymir work/effect/shining1/rosa/ridack_fan_pink2_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+678, "PART_WEAPON", "d:/ymir work/effect/shining1/rosa/ridack_knife_pink2_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+682, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining1/bianco/ridack_knife_white2_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+685, "PART_WEAPON", "d:/ymir work/effect/shining1/bianco/ridack_fan_white2_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+686, "PART_WEAPON", "d:/ymir work/effect/shining1/bianco/ridack_knife_white2_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+690, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining2/verde/ridack_knife_green_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+693, "PART_WEAPON", "d:/ymir work/effect/shining2/verde/ridack_fan_green_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+694, "PART_WEAPON", "d:/ymir work/effect/shining2/verde/ridack_knife_green_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+698, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining2/rosso/ridack_knife_red_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+701, "PART_WEAPON", "d:/ymir work/effect/shining2/rosso/ridack_fan_red_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+702, "PART_WEAPON", "d:/ymir work/effect/shining2/rosso/ridack_knife_red_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+706, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining2/blu/ridack_knife_blue_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+709, "PART_WEAPON", "d:/ymir work/effect/shining2/blu/ridack_fan_blue_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+710, "PART_WEAPON", "d:/ymir work/effect/shining2/blu/ridack_knife_blue_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+714, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining2/giallo/ridack_knife_yellow_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+717, "PART_WEAPON", "d:/ymir work/effect/shining2/giallo/ridack_fan_yellow_special.mse")
			chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+718, "PART_WEAPON", "d:/ymir work/effect/shining2/giallo/ridack_knife_yellow_special.mse")
		
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+719, "Bip01", "d:/ymir work/effect/ridack_shadow/ridack_wing.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+743, "Bip01", "d:/ymir work/effect/ridack_shadow/ridack_wing.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+744, "PART_WEAPON", "d:/ymir work/item/weapon/ridack_sword.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+745, "PART_WEAPON", "d:/ymir work/item/weapon/ridack_knife.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+746, "PART_WEAPON_LEFT", "d:/ymir work/item/weapon/ridack_knife.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+747, "PART_WEAPON_LEFT", "d:/ymir work/item/weapon/ridack_bow.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+748, "PART_WEAPON", "d:/ymir work/item/weapon/ridack_spear.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+749, "PART_WEAPON", "d:/ymir work/item/weapon/ridack_bell.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+750, "PART_WEAPON", "d:/ymir work/item/weapon/ridack_fan.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+751, "Bip01", "d:/ymir work/effect/ridack_shadow/ridack_armor.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+752, "Bip01", "d:/ymir work/effect/ridack_greentoxic/ridack_winggreen.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+753, "PART_WEAPON", "d:/ymir work/effect/ridack_greentoxic/ridack_sword.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+754, "PART_WEAPON", "d:/ymir work/effect/ridack_greentoxic/ridack_knife.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+755, "PART_WEAPON_LEFT", "d:/ymir work/effect/ridack_greentoxic/ridack_knife.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+756, "PART_WEAPON_LEFT", "d:/ymir work/effect/ridack_greentoxic/ridack_bow.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+757, "PART_WEAPON", "d:/ymir work/effect/ridack_greentoxic/ridack_spear.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+758, "PART_WEAPON", "d:/ymir work/effect/ridack_greentoxic/ridack_bell.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+759, "PART_WEAPON", "d:/ymir work/effect/ridack_greentoxic/ridack_fan.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+760, "Bip01", "d:/ymir work/effect/ridack_greentoxic/ridack_armor.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+761, "Bip01", "d:/ymir work/effect/ridack_deathset/ridack_wing.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+762, "PART_WEAPON", "d:/ymir work/effect/ridack_deathset/ridack_sword.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+763, "PART_WEAPON", "d:/ymir work/effect/ridack_deathset/ridack_knifebell.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+764, "PART_WEAPON_LEFT", "d:/ymir work/effect/ridack_deathset/ridack_knifebell.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+765, "PART_WEAPON_LEFT", "d:/ymir work/effect/ridack_deathset/ridack_bow.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+766, "PART_WEAPON", "d:/ymir work/effect/ridack_deathset/ridack_sword_2hand.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+767, "PART_WEAPON", "d:/ymir work/effect/ridack_deathset/ridack_knifebell.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+768, "PART_WEAPON", "d:/ymir work/effect/ridack_deathset/ridack_fan.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+769, "Bip01", "d:/ymir work/effect/ridack_deathset/ridack_armor.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+770, "PART_WEAPON_LEFT", "d:/ymir work/effect/ridack_deathset/ridack_fan.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+771, "Bip01", "d:/ymir work/effect/wing/ridack_knightwing.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+772, "PART_WEAPON", "d:/ymir work/effect/cavaliere/ridack_sword.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+773, "PART_WEAPON", "d:/ymir work/effect/cavaliere/ridack_knife.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+774, "PART_WEAPON_LEFT", "d:/ymir work/effect/cavaliere/ridack_knife.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+775, "PART_WEAPON_LEFT", "d:/ymir work/effect/cavaliere/ridack_bow.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+776, "PART_WEAPON", "d:/ymir work/effect/cavaliere/ridack_sword_2hand.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+777, "PART_WEAPON", "d:/ymir work/effect/cavaliere/ridack_bell.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+778, "PART_WEAPON", "d:/ymir work/effect/cavaliere/ridack_fan.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+779, "Bip01", "d:/ymir work/effect/cavaliere/ridack_armor.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+780, "PART_WEAPON_LEFT", "d:/ymir work/effect/cavaliere/ridack_fan.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+781, "Bip01", "d:/ymir work/effect/plechito/weapons/christmas2021_2/costume_glow_body.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+782, "snowflake_effect", "d:/ymir work/effect/plechito/weapons/christmas2021_2/snowflake_chest_male.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+783, "shoulder_effect_left", "d:/ymir work/effect/plechito/weapons/christmas2021_2/costume_glow_shoulder_left.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+784, "snowflake_effect", "d:/ymir work/effect/plechito/weapons/christmas2021_2/snowflake_chest_female.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+785, "shoulder_effect_left", "d:/ymir work/effect/plechito/weapons/christmas2021_2/costume_glow_shoulder_left_f.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+786, "PART_WEAPON", "d:/ymir work/effect/plechito/weapons/christmas2021_2/sura_sword_glow.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+787, "PART_WEAPON", "d:/ymir work/effect/plechito/weapons/christmas2021_2/dagger_glow.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+788, "PART_WEAPON_LEFT", "d:/ymir work/effect/plechito/weapons/christmas2021_2/dagger_glow.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+789, "PART_WEAPON_LEFT", "d:/ymir work/effect/plechito/weapons/christmas2021_2/bow_glow.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+790, "PART_WEAPON", "d:/ymir work/effect/plechito/weapons/christmas2021_2/twohand_glow.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+791, "PART_WEAPON", "d:/ymir work/effect/plechito/weapons/christmas2021_2/bell_glow.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+792, "PART_WEAPON", "d:/ymir work/effect/plechito/weapons/christmas2021_2/fan_glow.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+793, "PART_WEAPON_LEFT", "d:/ymir work/effect/plechito/weapons/christmas2021_2/fan_glow.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+794, "Bip01", "d:/ymir work/effect/shining13/ridack_armor_d.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+795, "PART_WEAPON", "d:/ymir work/effect/shining13/ridack_weapon_d.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+796, "PART_WEAPON", "d:/ymir work/effect/shining13/ridack_weapon_d_2h.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+797, "PART_WEAPON", "d:/ymir work/effect/shining13/ridack_weapon_d_s.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+798, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining13/ridack_weapon_d_s.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+799, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining13/ridack_weapon_d_b.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+800, "PART_WEAPON", "d:/ymir work/effect/shining13/ridack_weapon_d_s.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+801, "PART_WEAPON", "d:/ymir work/effect/shining13/ridack_weapon_d_f.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+802, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining13/ridack_weapon_d_f.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+803, "Bip01", "d:/ymir work/effect/plechito/weapons/christmas2021_1/costume.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+804, "PART_WEAPON", "d:/ymir work/effect/plechito/weapons/christmas2021_1/sura_sword_glow.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+805, "PART_WEAPON", "d:/ymir work/effect/plechito/weapons/christmas2021_1/dagger_glow.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+806, "PART_WEAPON_LEFT", "d:/ymir work/effect/plechito/weapons/christmas2021_1/dagger_glow.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+807, "PART_WEAPON_LEFT", "d:/ymir work/effect/plechito/weapons/christmas2021_1/bow_glow.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+808, "PART_WEAPON", "d:/ymir work/effect/plechito/weapons/christmas2021_1/twohand_glow.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+809, "PART_WEAPON", "d:/ymir work/effect/plechito/weapons/christmas2021_1/bell_glow.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+810, "PART_WEAPON", "d:/ymir work/effect/plechito/weapons/christmas2021_1/fan_glow.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+811, "PART_WEAPON_LEFT", "d:/ymir work/effect/plechito/weapons/christmas2021_1/fan_glow.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+812, "Bip01 Spine2", "d:/ymir work/effect/plechito/wings/1/wing.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+813, "Bip01", "d:/ymir work/effect/mehok/dragon_set/costume.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+814, "Bip01 Spine2", "d:/ymir work/effect/wings/1/wing.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+815, "PART_WEAPON", "d:/ymir work/effect/plechito/weapons/ice_set/sword.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+816, "PART_WEAPON", "d:/ymir work/effect/plechito/weapons/ice_set/dagger.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+817, "PART_WEAPON_LEFT", "d:/ymir work/effect/plechito/weapons/ice_set/dagger.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+818, "PART_WEAPON_LEFT", "d:/ymir work/effect/plechito/weapons/ice_set/bow.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+819, "PART_WEAPON", "d:/ymir work/effect/plechito/weapons/ice_set/twohand.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+820, "PART_WEAPON", "d:/ymir work/effect/plechito/weapons/ice_set/bell.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+821, "PART_WEAPON", "d:/ymir work/effect/plechito/weapons/ice_set/fan.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+822, "PART_WEAPON_LEFT", "d:/ymir work/effect/plechito/weapons/ice_set/fan.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+823, "Bip01", "d:/ymir work/effect/new_shinings/ridack_effect_blue/ridack_armorblue.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+824, "PART_WEAPON", "d:/ymir work/effect/new_shinings/ridack_effect_blue/weaponblue.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+825, "PART_WEAPON", "d:/ymir work/effect/new_shinings/ridack_effect_blue/weaponblue_2h.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+826, "PART_WEAPON", "d:/ymir work/effect/new_shinings/ridack_effect_blue/weaponblue_s.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+827, "PART_WEAPON_LEFT", "d:/ymir work/effect/new_shinings/ridack_effect_blue/weaponblue_s.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+828, "PART_WEAPON_LEFT", "d:/ymir work/effect/new_shinings/ridack_effect_blue/weaponblue_b.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+829, "PART_WEAPON", "d:/ymir work/effect/new_shinings/ridack_effect_blue/weaponblue_c.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+830, "PART_WEAPON", "d:/ymir work/effect/new_shinings/ridack_effect_blue/weaponblue_f.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+831, "PART_WEAPON_LEFT", "d:/ymir work/effect/new_shinings/ridack_effect_blue/weaponblue_f.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+832, "Bip01", "d:/ymir work/effect/new_shinings/ridack_effect_green/ridack_armorgreen.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+833, "PART_WEAPON", "d:/ymir work/effect/new_shinings/ridack_effect_green/weapongreen.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+834, "PART_WEAPON", "d:/ymir work/effect/new_shinings/ridack_effect_green/weapongreen_2h.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+835, "PART_WEAPON", "d:/ymir work/effect/new_shinings/ridack_effect_green/weapongreen_s.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+836, "PART_WEAPON_LEFT", "d:/ymir work/effect/new_shinings/ridack_effect_green/weapongreen_s.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+837, "PART_WEAPON_LEFT", "d:/ymir work/effect/new_shinings/ridack_effect_green/weapongreen_b.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+838, "PART_WEAPON", "d:/ymir work/effect/new_shinings/ridack_effect_green/weapongreen_c.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+839, "PART_WEAPON", "d:/ymir work/effect/new_shinings/ridack_effect_green/weapongreen_f.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+840, "PART_WEAPON_LEFT", "d:/ymir work/effect/new_shinings/ridack_effect_green/weapongreen_f.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+841, "Bip01", "d:/ymir work/effect/new_shinings/ridack_effect_red/ridack_armorred.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+842, "PART_WEAPON", "d:/ymir work/effect/new_shinings/ridack_effect_red/weaponred.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+843, "PART_WEAPON", "d:/ymir work/effect/new_shinings/ridack_effect_red/weaponred_2h.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+844, "PART_WEAPON", "d:/ymir work/effect/new_shinings/ridack_effect_red/weaponred_s.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+845, "PART_WEAPON_LEFT", "d:/ymir work/effect/new_shinings/ridack_effect_red/weaponred_s.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+846, "PART_WEAPON_LEFT", "d:/ymir work/effect/new_shinings/ridack_effect_red/weaponred_b.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+847, "PART_WEAPON", "d:/ymir work/effect/new_shinings/ridack_effect_red/weaponred_c.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+848, "PART_WEAPON", "d:/ymir work/effect/new_shinings/ridack_effect_red/weaponred_f.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+849, "PART_WEAPON_LEFT", "d:/ymir work/effect/new_shinings/ridack_effect_red/weaponred_f.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+850, "PART_WEAPON", "d:/ymir work/effect/sword_6/sword_8.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+851, "Bip01", "d:/ymir work/effect/armor_6/shining_1.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+852, "PART_WEAPON", "d:/ymir work/effect/shining1/viola/ridack_spear_purple2.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+853, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining1/viola/ridack_knife_purple2.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+854, "PART_WEAPON_LEFT", "d:/ymir work/effect/shining1/viola/ridack_bow_purple2.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+855, "PART_WEAPON", "d:/ymir work/effect/shining1/viola/ridack_bell_purple2.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+856, "PART_WEAPON", "d:/ymir work/effect/shining1/viola/ridack_fan_purple2.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+857, "PART_WEAPON", "d:/ymir work/effect/shining1/viola/ridack_knife_purple2.mse")

		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED + 858, "PART_WEAPON",      "d:/ymir work/item/weapon/halloween_2022/sword.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED + 859, "PART_WEAPON",      "d:/ymir work/item/weapon/halloween_2022/twohand.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED + 860, "PART_WEAPON_LEFT", "d:/ymir work/item/weapon/halloween_2022/dagger.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED + 861, "PART_WEAPON_LEFT", "d:/ymir work/item/weapon/halloween_2022/bow.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED + 862, "PART_WEAPON",      "d:/ymir work/item/weapon/halloween_2022/bell.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED + 863, "PART_WEAPON",      "d:/ymir work/item/weapon/halloween_2022/fan.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_REFINED+864, "Bip01", "d:/ymir work/item/weapon/halloween_2022/armor.mse")



	if app.ENABLE_SOUL_SYSTEM:
		chrmgr.RegisterEffect(chrmgr.EFFECT_AFFECT+chr.AFFECT_SOUL_RED, "", "d:/ymir work/effect/etc/soul/soul_red_001.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_AFFECT+chr.AFFECT_SOUL_BLUE, "", "d:/ymir work/effect/etc/soul/soul_blue_001.mse")
		chrmgr.RegisterEffect(chrmgr.EFFECT_AFFECT+chr.AFFECT_SOUL_MIX, "", "d:/ymir work/effect/etc/soul/soul_mix_001.mse")
	
	effect.RegisterIndexedFlyData(effect.FLY_EXP, effect.INDEX_FLY_TYPE_NORMAL, "d:/ymir work/effect/etc/gathering/ga_piece_yellow_small2.msf")
	effect.RegisterIndexedFlyData(effect.FLY_HP_MEDIUM, effect.INDEX_FLY_TYPE_NORMAL, "d:/ymir work/effect/etc/gathering/ga_piece_red_small.msf")
	effect.RegisterIndexedFlyData(effect.FLY_HP_BIG, effect.INDEX_FLY_TYPE_NORMAL, "d:/ymir work/effect/etc/gathering/ga_piece_red_big.msf")
	effect.RegisterIndexedFlyData(effect.FLY_SP_SMALL, effect.INDEX_FLY_TYPE_NORMAL, "d:/ymir work/effect/etc/gathering/ga_piece_blue_warrior_small.msf")
	effect.RegisterIndexedFlyData(effect.FLY_SP_MEDIUM, effect.INDEX_FLY_TYPE_NORMAL, "d:/ymir work/effect/etc/gathering/ga_piece_blue_small.msf")
	effect.RegisterIndexedFlyData(effect.FLY_SP_BIG, effect.INDEX_FLY_TYPE_NORMAL, "d:/ymir work/effect/etc/gathering/ga_piece_blue_big.msf")
	effect.RegisterIndexedFlyData(effect.FLY_FIREWORK1, effect.INDEX_FLY_TYPE_FIRE_CRACKER, "d:/ymir work/effect/etc/firecracker/firecracker_1.msf")
	effect.RegisterIndexedFlyData(effect.FLY_FIREWORK2, effect.INDEX_FLY_TYPE_FIRE_CRACKER, "d:/ymir work/effect/etc/firecracker/firecracker_2.msf")
	effect.RegisterIndexedFlyData(effect.FLY_FIREWORK3, effect.INDEX_FLY_TYPE_FIRE_CRACKER, "d:/ymir work/effect/etc/firecracker/firecracker_3.msf")
	effect.RegisterIndexedFlyData(effect.FLY_FIREWORK4, effect.INDEX_FLY_TYPE_FIRE_CRACKER, "d:/ymir work/effect/etc/firecracker/firecracker_4.msf")
	effect.RegisterIndexedFlyData(effect.FLY_FIREWORK5, effect.INDEX_FLY_TYPE_FIRE_CRACKER, "d:/ymir work/effect/etc/firecracker/firecracker_5.msf")
	effect.RegisterIndexedFlyData(effect.FLY_FIREWORK6, effect.INDEX_FLY_TYPE_FIRE_CRACKER, "d:/ymir work/effect/etc/firecracker/firecracker_6.msf")
	effect.RegisterIndexedFlyData(effect.FLY_FIREWORK_XMAS, effect.INDEX_FLY_TYPE_FIRE_CRACKER, "d:/ymir work/effect/etc/firecracker/firecracker_xmas.msf")
	effect.RegisterIndexedFlyData(effect.FLY_CHAIN_LIGHTNING, effect.INDEX_FLY_TYPE_NORMAL, "d:/ymir work/pc/shaman/effect/pokroe.msf")
	effect.RegisterIndexedFlyData(effect.FLY_HP_SMALL, effect.INDEX_FLY_TYPE_NORMAL, "d:/ymir work/effect/etc/gathering/ga_piece_red_smallest.msf")
	effect.RegisterIndexedFlyData(effect.FLY_SKILL_MUYEONG, effect.INDEX_FLY_TYPE_AUTO_FIRE, "d:/ymir work/pc/sura/effect/muyeong_fly.msf")
	
	chrmgr.RegisterEffect(chrmgr.EFFECT_EMOTICON+0, "", "d:/ymir work/effect/etc/emoticon/sweat.mse")
	net.RegisterEmoticonString("(황당)")
	chrmgr.RegisterEffect(chrmgr.EFFECT_EMOTICON+1, "", "d:/ymir work/effect/etc/emoticon/money.mse")
	net.RegisterEmoticonString("(돈)")
	chrmgr.RegisterEffect(chrmgr.EFFECT_EMOTICON+2, "", "d:/ymir work/effect/etc/emoticon/happy.mse")
	net.RegisterEmoticonString("(기쁨)")
	chrmgr.RegisterEffect(chrmgr.EFFECT_EMOTICON+3, "", "d:/ymir work/effect/etc/emoticon/love_s.mse")
	net.RegisterEmoticonString("(좋아)")
	chrmgr.RegisterEffect(chrmgr.EFFECT_EMOTICON+4, "", "d:/ymir work/effect/etc/emoticon/love_l.mse")
	net.RegisterEmoticonString("(사랑)")
	chrmgr.RegisterEffect(chrmgr.EFFECT_EMOTICON+5, "", "d:/ymir work/effect/etc/emoticon/angry.mse")
	net.RegisterEmoticonString("(분노)")
	chrmgr.RegisterEffect(chrmgr.EFFECT_EMOTICON+6, "", "d:/ymir work/effect/etc/emoticon/aha.mse")
	net.RegisterEmoticonString("(아하)")
	chrmgr.RegisterEffect(chrmgr.EFFECT_EMOTICON+7, "", "d:/ymir work/effect/etc/emoticon/gloom.mse")
	net.RegisterEmoticonString("(우울)")
	chrmgr.RegisterEffect(chrmgr.EFFECT_EMOTICON+8, "", "d:/ymir work/effect/etc/emoticon/sorry.mse")
	net.RegisterEmoticonString("(죄송)")
	chrmgr.RegisterEffect(chrmgr.EFFECT_EMOTICON+9, "", "d:/ymir work/effect/etc/emoticon/!_mix_back.mse")
	net.RegisterEmoticonString("(!)")
	chrmgr.RegisterEffect(chrmgr.EFFECT_EMOTICON+10, "", "d:/ymir work/effect/etc/emoticon/question.mse")
	net.RegisterEmoticonString("(?)")
	chrmgr.RegisterEffect(chrmgr.EFFECT_EMOTICON+11, "", "d:/ymir work/effect/etc/emoticon/fish.mse")
	net.RegisterEmoticonString("(fish)")
	chrmgr.RegisterEffect(chrmgr.EFFECT_EMOTICON+12, "", "d:/ymir work/effect/etc/emoticon/siren.mse")
	net.RegisterEmoticonString("(emoji1)")
	chrmgr.RegisterEffect(chrmgr.EFFECT_EMOTICON+13, "", "d:/ymir work/effect/etc/emoticon/call.mse")
	net.RegisterEmoticonString("(emoji2)")
	chrmgr.RegisterEffect(chrmgr.EFFECT_EMOTICON+14, "", "d:/ymir work/effect/etc/emoticon/charging.mse")
	net.RegisterEmoticonString("(emoji3)")
	chrmgr.RegisterEffect(chrmgr.EFFECT_EMOTICON+15, "", "d:/ymir work/effect/etc/emoticon/celebration.mse")
	net.RegisterEmoticonString("(emoji4)")
	chrmgr.RegisterEffect(chrmgr.EFFECT_EMOTICON+16, "", "d:/ymir work/effect/etc/emoticon/weather1.mse")
	net.RegisterEmoticonString("(emoji5)")
	chrmgr.RegisterEffect(chrmgr.EFFECT_EMOTICON+17, "", "d:/ymir work/effect/etc/emoticon/alcohol.mse")
	net.RegisterEmoticonString("(emoji6)")
	chrmgr.RegisterEffect(chrmgr.EFFECT_EMOTICON+18, "", "d:/ymir work/effect/etc/emoticon/hungry.mse")
	net.RegisterEmoticonString("(emoji7)")
	chrmgr.RegisterEffect(chrmgr.EFFECT_EMOTICON+19, "", "d:/ymir work/effect/etc/emoticon/siren.mse")
	net.RegisterEmoticonString("(emoji8)")

def LoadGameNPC():
	global isLoaded4
	if isLoaded4:
		return
	
	isLoaded4 = 1
	
	try:
		lines = open("data/monsters/npclist.txt", "r").readlines()
	except IOError:
		import dbg
		dbg.LogBox("LoadLocaleError(%(srcFileName)s)" % locals())
		app.Abort()
	
	for line in lines:
		tokens = line[:-1].split("\t")
		if len(tokens) == 0 or not tokens[0]:
			continue
		
		try:
			vnum = int(tokens[0])
		except ValueError:
			import dbg
			dbg.LogBox("LoadGameNPC() - %s - line #%d: %s" % (tokens, lines.index(line), line))
			app.Abort()
		
		try:
			if vnum:
				chrmgr.RegisterRaceName(vnum, tokens[1].strip())
			else:
				chrmgr.RegisterRaceSrcName(tokens[1].strip(), tokens[2].strip())
		except IndexError:
			import dbg
			dbg.LogBox("LoadGameNPC() - %d, %s - line #%d: %s " % (vnum, tokens, lines.index(line), line))
			app.Abort()

def LoadGameWarrior():
	global isLoaded5
	if isLoaded5:
		return
	
	isLoaded5 = 1
	
	LoadGameWarriorEx(RACE_WARRIOR_M, "d:/ymir work/pc/warrior/")
	LoadGameWarriorEx(RACE_WARRIOR_W, "d:/ymir work/pc2/warrior/")

def LoadGameAssassin():
	global isLoaded6
	if isLoaded6:
		return
	
	isLoaded6 = 1
	
	LoadGameAssassinEx(RACE_ASSASSIN_W, "d:/ymir work/pc/assassin/")
	LoadGameAssassinEx(RACE_ASSASSIN_M, "d:/ymir work/pc2/assassin/")

def LoadGameSura():
	global isLoaded7
	if isLoaded7:
		return
	
	isLoaded7 = 1
	
	LoadGameSuraEx(RACE_SURA_M, "d:/ymir work/pc/sura/")
	LoadGameSuraEx(RACE_SURA_W, "d:/ymir work/pc2/sura/")

def LoadGameShaman():
	global isLoaded8
	if isLoaded8:
		return
	
	isLoaded8 = 1
	
	LoadGameShamanEx(RACE_SHAMAN_W, "d:/ymir work/pc/shaman/")
	LoadGameShamanEx(RACE_SHAMAN_M, "d:/ymir work/pc2/shaman/")

def SetGeneralMotions(mode, folder):
	chrmgr.SetPathName(folder)
	chrmgr.RegisterMotionMode(mode)
	chrmgr.RegisterCacheMotionData(mode, chr.MOTION_WAIT, "wait.msa")
	chrmgr.RegisterCacheMotionData(mode, chr.MOTION_WALK, "walk.msa")
	chrmgr.RegisterCacheMotionData(mode, chr.MOTION_RUN, "run.msa")
	chrmgr.RegisterCacheMotionData(mode, chr.MOTION_DAMAGE, "damage.msa", 50)
	chrmgr.RegisterCacheMotionData(mode, chr.MOTION_DAMAGE, "damage_1.msa", 50)
	chrmgr.RegisterCacheMotionData(mode, chr.MOTION_DAMAGE_BACK, "damage_2.msa", 50)
	chrmgr.RegisterCacheMotionData(mode, chr.MOTION_DAMAGE_BACK, "damage_3.msa", 50)
	chrmgr.RegisterCacheMotionData(mode, chr.MOTION_DAMAGE_FLYING, "damage_flying.msa")
	chrmgr.RegisterCacheMotionData(mode, chr.MOTION_STAND_UP, "falling_stand.msa")
	chrmgr.RegisterCacheMotionData(mode, chr.MOTION_DAMAGE_FLYING_BACK, "back_damage_flying.msa")
	chrmgr.RegisterCacheMotionData(mode, chr.MOTION_STAND_UP_BACK, "back_falling_stand.msa")
	chrmgr.RegisterCacheMotionData(mode, chr.MOTION_DEAD, "dead.msa")
	chrmgr.RegisterCacheMotionData(mode, chr.MOTION_DIG, "dig.msa")

def SetIntroMotions(mode, folder):
	chrmgr.SetPathName(folder)
	chrmgr.RegisterMotionMode(mode)
	chrmgr.RegisterCacheMotionData(mode, chr.MOTION_INTRO_WAIT, "wait.msa")
	chrmgr.RegisterCacheMotionData(mode, chr.MOTION_INTRO_SELECTED, "selected.msa")
	chrmgr.RegisterCacheMotionData(mode, chr.MOTION_INTRO_NOT_SELECTED, "not_selected.msa")


def LoadGameWarriorEx(race, path):
	chrmgr.SelectRace(race)
	
	SetGeneralMotions(chr.MOTION_MODE_GENERAL, path + "general/")
	chrmgr.SetMotionRandomWeight(chr.MOTION_MODE_GENERAL, chr.MOTION_WAIT, 0, 70)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_WAIT, "wait_1.msa", 30)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_COMBO_ATTACK_1, "attack.msa", 50)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_COMBO_ATTACK_1, "attack_1.msa", 50)
	
	chrmgr.SetPathName(path + "skill/")
	for i in xrange(skill.SKILL_EFFECT_COUNT):
		END_STRING = ""
		if i != 0: END_STRING = "_%d" % (i+1)
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+1, "samyeon" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+2, "palbang" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+3, "jeongwi" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+4, "geomgyeong" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+5, "tanhwan" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+6, "gihyeol" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+16, "gigongcham" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+17, "gyeoksan" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+18, "daejin" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+19, "cheongeun" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+20, "geompung" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+21, "noegeom" + END_STRING + ".msa")
	
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, GUILD_SKILL_DRAGONBLOOD, "guild_yongsinuipi.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, GUILD_SKILL_DRAGONBLESS, "guild_yongsinuichukbok.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, GUILD_SKILL_BLESSARMOR, "guild_seonghwigap.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, GUILD_SKILL_SPPEDUP, "guild_gasokhwa.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, GUILD_SKILL_DRAGONWRATH, "guild_yongsinuibunno.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, GUILD_SKILL_MAGICUP, "guild_jumunsul.msa")
	
	chrmgr.ReserveComboAttackNew(chr.MOTION_MODE_GENERAL, COMBO_TYPE_1, 1)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_GENERAL, COMBO_TYPE_1, COMBO_INDEX_1, chr.MOTION_COMBO_ATTACK_1)
	
	emotion.RegisterEmotionAnis(path)
	
	chrmgr.SetPathName(path + "onehand_sword/")
	chrmgr.RegisterMotionMode(chr.MOTION_MODE_ONEHAND_SWORD)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_WAIT, "wait.msa", 50)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_WAIT, "wait_1.msa", 50)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_WALK, "walk.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_RUN, "run.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_DAMAGE, "damage.msa", 50)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_DAMAGE, "damage_1.msa", 50)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_DAMAGE_BACK, "damage_2.msa", 50)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_DAMAGE_BACK, "damage_3.msa", 50)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_COMBO_ATTACK_1, "combo_01.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_COMBO_ATTACK_2, "combo_02.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_COMBO_ATTACK_3, "combo_03.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_COMBO_ATTACK_4, "combo_04.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_COMBO_ATTACK_5, "combo_05.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_COMBO_ATTACK_6, "combo_06.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_COMBO_ATTACK_7, "combo_07.msa")
	chrmgr.ReserveComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_1, 4)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_1, COMBO_INDEX_1, chr.MOTION_COMBO_ATTACK_1)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_1, COMBO_INDEX_2, chr.MOTION_COMBO_ATTACK_2)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_1, COMBO_INDEX_3, chr.MOTION_COMBO_ATTACK_3)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_1, COMBO_INDEX_4, chr.MOTION_COMBO_ATTACK_4)
	chrmgr.ReserveComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_2, 5)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_2, COMBO_INDEX_1, chr.MOTION_COMBO_ATTACK_1)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_2, COMBO_INDEX_2, chr.MOTION_COMBO_ATTACK_2)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_2, COMBO_INDEX_3, chr.MOTION_COMBO_ATTACK_3)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_2, COMBO_INDEX_4, chr.MOTION_COMBO_ATTACK_5)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_2, COMBO_INDEX_5, chr.MOTION_COMBO_ATTACK_7)
	chrmgr.ReserveComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_3, 6)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_3, COMBO_INDEX_1, chr.MOTION_COMBO_ATTACK_1)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_3, COMBO_INDEX_2, chr.MOTION_COMBO_ATTACK_2)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_3, COMBO_INDEX_3, chr.MOTION_COMBO_ATTACK_3)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_3, COMBO_INDEX_4, chr.MOTION_COMBO_ATTACK_5)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_3, COMBO_INDEX_5, chr.MOTION_COMBO_ATTACK_6)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_3, COMBO_INDEX_6, chr.MOTION_COMBO_ATTACK_4)
	
	chrmgr.SetPathName(path + "twohand_sword/")
	chrmgr.RegisterMotionMode(chr.MOTION_MODE_TWOHAND_SWORD)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_TWOHAND_SWORD, chr.MOTION_WAIT, "wait.msa", 70)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_TWOHAND_SWORD, chr.MOTION_WAIT, "wait_1.msa", 30)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_TWOHAND_SWORD, chr.MOTION_WALK, "walk.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_TWOHAND_SWORD, chr.MOTION_RUN, "run.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_TWOHAND_SWORD, chr.MOTION_DAMAGE, "damage.msa", 50)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_TWOHAND_SWORD, chr.MOTION_DAMAGE, "damage_1.msa", 50)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_TWOHAND_SWORD, chr.MOTION_DAMAGE_BACK, "damage_2.msa", 50)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_TWOHAND_SWORD, chr.MOTION_DAMAGE_BACK, "damage_3.msa", 50)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_TWOHAND_SWORD, chr.MOTION_COMBO_ATTACK_1, "combo_01.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_TWOHAND_SWORD, chr.MOTION_COMBO_ATTACK_2, "combo_02.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_TWOHAND_SWORD, chr.MOTION_COMBO_ATTACK_3, "combo_03.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_TWOHAND_SWORD, chr.MOTION_COMBO_ATTACK_4, "combo_04.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_TWOHAND_SWORD, chr.MOTION_COMBO_ATTACK_5, "combo_05.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_TWOHAND_SWORD, chr.MOTION_COMBO_ATTACK_6, "combo_06.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_TWOHAND_SWORD, chr.MOTION_COMBO_ATTACK_7, "combo_07.msa")
	chrmgr.ReserveComboAttackNew(chr.MOTION_MODE_TWOHAND_SWORD, COMBO_TYPE_1, 4)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_TWOHAND_SWORD, COMBO_TYPE_1, COMBO_INDEX_1, chr.MOTION_COMBO_ATTACK_1)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_TWOHAND_SWORD, COMBO_TYPE_1, COMBO_INDEX_2, chr.MOTION_COMBO_ATTACK_2)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_TWOHAND_SWORD, COMBO_TYPE_1, COMBO_INDEX_3, chr.MOTION_COMBO_ATTACK_3)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_TWOHAND_SWORD, COMBO_TYPE_1, COMBO_INDEX_4, chr.MOTION_COMBO_ATTACK_4)
	chrmgr.ReserveComboAttackNew(chr.MOTION_MODE_TWOHAND_SWORD, COMBO_TYPE_2, 5)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_TWOHAND_SWORD, COMBO_TYPE_2, COMBO_INDEX_1, chr.MOTION_COMBO_ATTACK_1)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_TWOHAND_SWORD, COMBO_TYPE_2, COMBO_INDEX_2, chr.MOTION_COMBO_ATTACK_2)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_TWOHAND_SWORD, COMBO_TYPE_2, COMBO_INDEX_3, chr.MOTION_COMBO_ATTACK_3)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_TWOHAND_SWORD, COMBO_TYPE_2, COMBO_INDEX_4, chr.MOTION_COMBO_ATTACK_5)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_TWOHAND_SWORD, COMBO_TYPE_2, COMBO_INDEX_5, chr.MOTION_COMBO_ATTACK_7)
	chrmgr.ReserveComboAttackNew(chr.MOTION_MODE_TWOHAND_SWORD, COMBO_TYPE_3, 6)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_TWOHAND_SWORD, COMBO_TYPE_3, COMBO_INDEX_1, chr.MOTION_COMBO_ATTACK_1)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_TWOHAND_SWORD, COMBO_TYPE_3, COMBO_INDEX_2, chr.MOTION_COMBO_ATTACK_2)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_TWOHAND_SWORD, COMBO_TYPE_3, COMBO_INDEX_3, chr.MOTION_COMBO_ATTACK_3)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_TWOHAND_SWORD, COMBO_TYPE_3, COMBO_INDEX_4, chr.MOTION_COMBO_ATTACK_5)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_TWOHAND_SWORD, COMBO_TYPE_3, COMBO_INDEX_5, chr.MOTION_COMBO_ATTACK_6)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_TWOHAND_SWORD, COMBO_TYPE_3, COMBO_INDEX_6, chr.MOTION_COMBO_ATTACK_4)
	
	chrmgr.SetPathName(path + "fishing/")
	chrmgr.RegisterMotionMode(chr.MOTION_MODE_FISHING)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FISHING, chr.MOTION_WAIT, "wait.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FISHING, chr.MOTION_WALK, "walk.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FISHING, chr.MOTION_RUN, "run.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FISHING, chr.MOTION_FISHING_THROW, "throw.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FISHING, chr.MOTION_FISHING_WAIT, "fishing_wait.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FISHING, chr.MOTION_FISHING_STOP, "fishing_cancel.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FISHING, chr.MOTION_FISHING_REACT, "fishing_react.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FISHING, chr.MOTION_FISHING_CATCH, "fishing_catch.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FISHING, chr.MOTION_FISHING_FAIL, "fishing_fail.msa")
	
	chrmgr.SetPathName(path + "horse/")
	chrmgr.RegisterMotionMode(chr.MOTION_MODE_HORSE)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE, chr.MOTION_WAIT, "wait.msa", 90)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE, chr.MOTION_WAIT, "wait_1.msa", 9)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE, chr.MOTION_WAIT, "wait_2.msa", 1)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE, chr.MOTION_WALK, "walk.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE, chr.MOTION_RUN, "run.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE, chr.MOTION_DAMAGE, "damage.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE, chr.MOTION_DAMAGE_BACK, "damage.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE, chr.MOTION_DEAD, "dead.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE, HORSE_SKILL_CHARGE, "skill_charge.msa")
	
	chrmgr.SetPathName(path + "horse_onehand_sword/")
	chrmgr.RegisterMotionMode(chr.MOTION_MODE_HORSE_ONEHAND_SWORD)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_ONEHAND_SWORD, chr.MOTION_COMBO_ATTACK_1, "combo_01.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_ONEHAND_SWORD, chr.MOTION_COMBO_ATTACK_2, "combo_02.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_ONEHAND_SWORD, chr.MOTION_COMBO_ATTACK_3, "combo_03.msa")
	chrmgr.ReserveComboAttackNew(chr.MOTION_MODE_HORSE_ONEHAND_SWORD, COMBO_TYPE_1, 3)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_HORSE_ONEHAND_SWORD, COMBO_TYPE_1, COMBO_INDEX_1, chr.MOTION_COMBO_ATTACK_1)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_HORSE_ONEHAND_SWORD, COMBO_TYPE_1, COMBO_INDEX_2, chr.MOTION_COMBO_ATTACK_2)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_HORSE_ONEHAND_SWORD, COMBO_TYPE_1, COMBO_INDEX_3, chr.MOTION_COMBO_ATTACK_3)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_ONEHAND_SWORD, HORSE_SKILL_WILDATTACK, "skill_wildattack.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_ONEHAND_SWORD, HORSE_SKILL_SPLASH, "skill_splash.msa")
	
	chrmgr.SetPathName(path + "horse_twohand_sword/")
	chrmgr.RegisterMotionMode(chr.MOTION_MODE_HORSE_TWOHAND_SWORD)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_TWOHAND_SWORD, chr.MOTION_COMBO_ATTACK_1, "combo_01.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_TWOHAND_SWORD, chr.MOTION_COMBO_ATTACK_2, "combo_02.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_TWOHAND_SWORD, chr.MOTION_COMBO_ATTACK_3, "combo_03.msa")
	chrmgr.ReserveComboAttackNew(chr.MOTION_MODE_HORSE_TWOHAND_SWORD, COMBO_TYPE_1, 3)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_HORSE_TWOHAND_SWORD, COMBO_TYPE_1, COMBO_INDEX_1, chr.MOTION_COMBO_ATTACK_1)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_HORSE_TWOHAND_SWORD, COMBO_TYPE_1, COMBO_INDEX_2, chr.MOTION_COMBO_ATTACK_2)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_HORSE_TWOHAND_SWORD, COMBO_TYPE_1, COMBO_INDEX_3, chr.MOTION_COMBO_ATTACK_3)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_TWOHAND_SWORD, HORSE_SKILL_WILDATTACK, "skill_wildattack.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_TWOHAND_SWORD, HORSE_SKILL_SPLASH, "skill_splash.msa")
	
	chrmgr.RegisterAttachingBoneName(chr.PART_WEAPON, "equip_right_hand")
	if app.ENABLE_ACCE_SYSTEM:
		chrmgr.RegisterAttachingBoneName(chr.PART_ACCE, "Bip01 Spine2")

def LoadGameAssassinEx(race, path):
	chrmgr.SelectRace(race)
	
	SetGeneralMotions(chr.MOTION_MODE_GENERAL, path + "general/")
	chrmgr.SetMotionRandomWeight(chr.MOTION_MODE_GENERAL, chr.MOTION_WAIT, 0, 70)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_WAIT, "wait_1.msa", 30)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_COMBO_ATTACK_1, "attack.msa", 50)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_COMBO_ATTACK_1, "attack_1.msa", 50)
	
	chrmgr.SetPathName(path + "skill/")
	for i in xrange(skill.SKILL_EFFECT_COUNT):
		END_STRING = ""
		if i != 0: END_STRING = "_%d" % (i+1)
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+1, "amseup" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+2, "gungsin" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+3, "charyun" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+4, "eunhyeong" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+5, "sangong" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+6, "seomjeon" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+16, "yeonsa" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+17, "gwangyeok" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+18, "hwajo" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+19, "gyeonggong" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+20, "dokgigung" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+21, "seomgwang" + END_STRING + ".msa")
	
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, GUILD_SKILL_DRAGONBLOOD, "guild_yongsinuipi.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, GUILD_SKILL_DRAGONBLESS, "guild_yongsinuichukbok.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, GUILD_SKILL_BLESSARMOR, "guild_seonghwigap.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, GUILD_SKILL_SPPEDUP, "guild_gasokhwa.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, GUILD_SKILL_DRAGONWRATH, "guild_yongsinuibunno.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, GUILD_SKILL_MAGICUP, "guild_jumunsul.msa")
	
	chrmgr.ReserveComboAttackNew(chr.MOTION_MODE_GENERAL, COMBO_TYPE_1, 1)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_GENERAL, COMBO_TYPE_1, COMBO_INDEX_1, chr.MOTION_COMBO_ATTACK_1)
	
	emotion.RegisterEmotionAnis(path)
	
	chrmgr.SetPathName(path + "onehand_sword/")
	chrmgr.RegisterMotionMode(chr.MOTION_MODE_ONEHAND_SWORD)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_WAIT, "wait.msa", 70)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_WAIT, "wait_1.msa", 30)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_WALK, "walk.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_RUN, "run.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_DAMAGE, "damage.msa", 50)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_DAMAGE, "damage_1.msa", 50)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_DAMAGE_BACK, "damage_2.msa", 50)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_DAMAGE_BACK, "damage_3.msa", 50)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_COMBO_ATTACK_1, "combo_01.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_COMBO_ATTACK_2, "combo_02.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_COMBO_ATTACK_3, "combo_03.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_COMBO_ATTACK_4, "combo_04.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_COMBO_ATTACK_5, "combo_05.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_COMBO_ATTACK_6, "combo_06.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_COMBO_ATTACK_7, "combo_07.msa")
	chrmgr.ReserveComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_1, 4)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_1, COMBO_INDEX_1, chr.MOTION_COMBO_ATTACK_1)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_1, COMBO_INDEX_2, chr.MOTION_COMBO_ATTACK_2)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_1, COMBO_INDEX_3, chr.MOTION_COMBO_ATTACK_3)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_1, COMBO_INDEX_4, chr.MOTION_COMBO_ATTACK_4)
	chrmgr.ReserveComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_2, 5)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_2, COMBO_INDEX_1, chr.MOTION_COMBO_ATTACK_1)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_2, COMBO_INDEX_2, chr.MOTION_COMBO_ATTACK_2)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_2, COMBO_INDEX_3, chr.MOTION_COMBO_ATTACK_3)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_2, COMBO_INDEX_4, chr.MOTION_COMBO_ATTACK_5)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_2, COMBO_INDEX_5, chr.MOTION_COMBO_ATTACK_7)
	chrmgr.ReserveComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_3, 6)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_3, COMBO_INDEX_1, chr.MOTION_COMBO_ATTACK_1)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_3, COMBO_INDEX_2, chr.MOTION_COMBO_ATTACK_2)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_3, COMBO_INDEX_3, chr.MOTION_COMBO_ATTACK_3)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_3, COMBO_INDEX_4, chr.MOTION_COMBO_ATTACK_5)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_3, COMBO_INDEX_5, chr.MOTION_COMBO_ATTACK_6)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_3, COMBO_INDEX_6, chr.MOTION_COMBO_ATTACK_4)
	
	chrmgr.SetPathName(path + "dualhand_sword/")
	chrmgr.RegisterMotionMode(chr.MOTION_MODE_DUALHAND_SWORD)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_DUALHAND_SWORD, chr.MOTION_WAIT, "wait.msa", 70)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_DUALHAND_SWORD, chr.MOTION_WAIT, "wait_1.msa", 30)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_DUALHAND_SWORD, chr.MOTION_WALK, "walk.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_DUALHAND_SWORD, chr.MOTION_RUN, "run.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_DUALHAND_SWORD, chr.MOTION_DAMAGE, "damage.msa", 50)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_DUALHAND_SWORD, chr.MOTION_DAMAGE, "damage_1.msa", 50)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_DUALHAND_SWORD, chr.MOTION_DAMAGE_BACK, "damage_2.msa", 50)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_DUALHAND_SWORD, chr.MOTION_DAMAGE_BACK, "damage_3.msa", 50)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_DUALHAND_SWORD, chr.MOTION_COMBO_ATTACK_1, "combo_01.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_DUALHAND_SWORD, chr.MOTION_COMBO_ATTACK_2, "combo_02.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_DUALHAND_SWORD, chr.MOTION_COMBO_ATTACK_3, "combo_03.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_DUALHAND_SWORD, chr.MOTION_COMBO_ATTACK_4, "combo_04.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_DUALHAND_SWORD, chr.MOTION_COMBO_ATTACK_5, "combo_05.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_DUALHAND_SWORD, chr.MOTION_COMBO_ATTACK_6, "combo_06.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_DUALHAND_SWORD, chr.MOTION_COMBO_ATTACK_7, "combo_07.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_DUALHAND_SWORD, chr.MOTION_COMBO_ATTACK_8, "combo_08.msa")
	chrmgr.ReserveComboAttackNew(chr.MOTION_MODE_DUALHAND_SWORD, COMBO_TYPE_1, 4)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_DUALHAND_SWORD, COMBO_TYPE_1, COMBO_INDEX_1, chr.MOTION_COMBO_ATTACK_1)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_DUALHAND_SWORD, COMBO_TYPE_1, COMBO_INDEX_2, chr.MOTION_COMBO_ATTACK_2)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_DUALHAND_SWORD, COMBO_TYPE_1, COMBO_INDEX_3, chr.MOTION_COMBO_ATTACK_3)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_DUALHAND_SWORD, COMBO_TYPE_1, COMBO_INDEX_4, chr.MOTION_COMBO_ATTACK_4)
	chrmgr.ReserveComboAttackNew(chr.MOTION_MODE_DUALHAND_SWORD, COMBO_TYPE_2, 5)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_DUALHAND_SWORD, COMBO_TYPE_2, COMBO_INDEX_1, chr.MOTION_COMBO_ATTACK_1)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_DUALHAND_SWORD, COMBO_TYPE_2, COMBO_INDEX_2, chr.MOTION_COMBO_ATTACK_2)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_DUALHAND_SWORD, COMBO_TYPE_2, COMBO_INDEX_3, chr.MOTION_COMBO_ATTACK_3)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_DUALHAND_SWORD, COMBO_TYPE_2, COMBO_INDEX_4, chr.MOTION_COMBO_ATTACK_5)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_DUALHAND_SWORD, COMBO_TYPE_2, COMBO_INDEX_5, chr.MOTION_COMBO_ATTACK_7)
	chrmgr.ReserveComboAttackNew(chr.MOTION_MODE_DUALHAND_SWORD, COMBO_TYPE_3, 6)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_DUALHAND_SWORD, COMBO_TYPE_3, COMBO_INDEX_1, chr.MOTION_COMBO_ATTACK_1)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_DUALHAND_SWORD, COMBO_TYPE_3, COMBO_INDEX_2, chr.MOTION_COMBO_ATTACK_2)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_DUALHAND_SWORD, COMBO_TYPE_3, COMBO_INDEX_3, chr.MOTION_COMBO_ATTACK_3)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_DUALHAND_SWORD, COMBO_TYPE_3, COMBO_INDEX_4, chr.MOTION_COMBO_ATTACK_5)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_DUALHAND_SWORD, COMBO_TYPE_3, COMBO_INDEX_5, chr.MOTION_COMBO_ATTACK_6)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_DUALHAND_SWORD, COMBO_TYPE_3, COMBO_INDEX_6, chr.MOTION_COMBO_ATTACK_8)
	
	chrmgr.SetPathName(path + "bow/")
	chrmgr.RegisterMotionMode(chr.MOTION_MODE_BOW)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_BOW, chr.MOTION_WAIT, "wait.msa", 70)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_BOW, chr.MOTION_WAIT, "wait_1.msa", 30)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_BOW, chr.MOTION_WALK, "walk.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_BOW, chr.MOTION_RUN, "run.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_BOW, chr.MOTION_DAMAGE, "damage.msa", 50)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_BOW, chr.MOTION_DAMAGE, "damage_1.msa", 50)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_BOW, chr.MOTION_DAMAGE_BACK, "damage_2.msa", 50)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_BOW, chr.MOTION_DAMAGE_BACK, "damage_3.msa", 50)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_BOW, chr.MOTION_COMBO_ATTACK_1, "attack.msa")
	chrmgr.ReserveComboAttackNew(chr.MOTION_MODE_BOW, COMBO_TYPE_1, 1)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_BOW, COMBO_TYPE_1, COMBO_INDEX_1, chr.MOTION_COMBO_ATTACK_1)
	
	chrmgr.SetPathName(path + "fishing/")
	chrmgr.RegisterMotionMode(chr.MOTION_MODE_FISHING)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FISHING, chr.MOTION_WAIT, "wait.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FISHING, chr.MOTION_WALK, "walk.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FISHING, chr.MOTION_RUN, "run.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FISHING, chr.MOTION_FISHING_THROW, "throw.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FISHING, chr.MOTION_FISHING_WAIT, "fishing_wait.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FISHING, chr.MOTION_FISHING_STOP, "fishing_cancel.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FISHING, chr.MOTION_FISHING_REACT, "fishing_react.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FISHING, chr.MOTION_FISHING_CATCH, "fishing_catch.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FISHING, chr.MOTION_FISHING_FAIL, "fishing_fail.msa")
	
	chrmgr.SetPathName(path + "horse/")
	chrmgr.RegisterMotionMode(chr.MOTION_MODE_HORSE)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE, chr.MOTION_WAIT, "wait.msa", 90)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE, chr.MOTION_WAIT, "wait_1.msa", 9)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE, chr.MOTION_WAIT, "wait_2.msa", 1)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE, chr.MOTION_WALK, "walk.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE, chr.MOTION_RUN, "run.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE, chr.MOTION_DAMAGE, "damage.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE, chr.MOTION_DAMAGE_BACK, "damage.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE, chr.MOTION_DEAD, "dead.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE, HORSE_SKILL_CHARGE, "skill_charge.msa")
	
	chrmgr.SetPathName(path + "horse_onehand_sword/")
	chrmgr.RegisterMotionMode(chr.MOTION_MODE_HORSE_ONEHAND_SWORD)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_ONEHAND_SWORD, chr.MOTION_COMBO_ATTACK_1, "combo_01.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_ONEHAND_SWORD, chr.MOTION_COMBO_ATTACK_2, "combo_02.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_ONEHAND_SWORD, chr.MOTION_COMBO_ATTACK_3, "combo_03.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_ONEHAND_SWORD, HORSE_SKILL_WILDATTACK, "skill_wildattack.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_ONEHAND_SWORD, HORSE_SKILL_SPLASH, "skill_splash.msa")
	chrmgr.ReserveComboAttackNew(chr.MOTION_MODE_HORSE_ONEHAND_SWORD, COMBO_TYPE_1, 3)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_HORSE_ONEHAND_SWORD, COMBO_TYPE_1, COMBO_INDEX_1, chr.MOTION_COMBO_ATTACK_1)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_HORSE_ONEHAND_SWORD, COMBO_TYPE_1, COMBO_INDEX_2, chr.MOTION_COMBO_ATTACK_2)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_HORSE_ONEHAND_SWORD, COMBO_TYPE_1, COMBO_INDEX_3, chr.MOTION_COMBO_ATTACK_3)
	
	chrmgr.SetPathName(path + "horse_dualhand_sword/")
	chrmgr.RegisterMotionMode(chr.MOTION_MODE_HORSE_DUALHAND_SWORD)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_DUALHAND_SWORD, chr.MOTION_COMBO_ATTACK_1, "combo_01.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_DUALHAND_SWORD, chr.MOTION_COMBO_ATTACK_2, "combo_02.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_DUALHAND_SWORD, chr.MOTION_COMBO_ATTACK_3, "combo_03.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_DUALHAND_SWORD, HORSE_SKILL_WILDATTACK, "skill_wildattack.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_DUALHAND_SWORD, HORSE_SKILL_SPLASH, "skill_splash.msa")
	chrmgr.ReserveComboAttackNew(chr.MOTION_MODE_HORSE_DUALHAND_SWORD, COMBO_TYPE_1, 3)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_HORSE_DUALHAND_SWORD, COMBO_TYPE_1, COMBO_INDEX_1, chr.MOTION_COMBO_ATTACK_1)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_HORSE_DUALHAND_SWORD, COMBO_TYPE_1, COMBO_INDEX_2, chr.MOTION_COMBO_ATTACK_2)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_HORSE_DUALHAND_SWORD, COMBO_TYPE_1, COMBO_INDEX_3, chr.MOTION_COMBO_ATTACK_3)
	
	chrmgr.SetPathName(path + "horse_bow/")
	chrmgr.RegisterMotionMode(chr.MOTION_MODE_HORSE_BOW)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_BOW, chr.MOTION_WAIT, "wait.msa", 90)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_BOW, chr.MOTION_WAIT, "wait_1.msa", 9)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_BOW, chr.MOTION_WAIT, "wait_2.msa", 1)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_BOW, chr.MOTION_RUN, "run.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_BOW, chr.MOTION_DAMAGE, "damage.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_BOW, chr.MOTION_DEAD, "dead.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_BOW, chr.MOTION_COMBO_ATTACK_1, "attack.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_BOW, HORSE_SKILL_WILDATTACK, "skill_wildattack.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_BOW, HORSE_SKILL_SPLASH, "skill_splash.msa")
	chrmgr.ReserveComboAttackNew(chr.MOTION_MODE_HORSE_BOW, COMBO_TYPE_1, 1)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_HORSE_BOW, COMBO_TYPE_1, COMBO_INDEX_1, chr.MOTION_COMBO_ATTACK_1)
	
	chrmgr.RegisterAttachingBoneName(chr.PART_WEAPON, "equip_right")
	chrmgr.RegisterAttachingBoneName(chr.PART_WEAPON_LEFT, "equip_left")
	if app.ENABLE_ACCE_SYSTEM:
		chrmgr.RegisterAttachingBoneName(chr.PART_ACCE, "Bip01 Spine2")

def LoadGameSuraEx(race, path):
	chrmgr.SelectRace(race)
	
	SetGeneralMotions(chr.MOTION_MODE_GENERAL, path + "general/")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_COMBO_ATTACK_1, "attack.msa", 50)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_COMBO_ATTACK_1, "attack_1.msa", 50)
	
	chrmgr.SetPathName(path + "skill/")
	for i in xrange(skill.SKILL_EFFECT_COUNT):
		END_STRING = ""
		if i != 0: END_STRING = "_%d" % (i+1)
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+1, "swaeryeong" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+2, "yonggwon" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+3, "gwigeom" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+4, "gongpo" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+5, "jumagap" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+6, "pabeop" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+16, "maryeong" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+17, "hwayeom" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+18, "muyeong" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+19, "heuksin" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+20, "tusok" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+21, "mahwan" + END_STRING + ".msa")
	
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, GUILD_SKILL_DRAGONBLOOD, "guild_yongsinuipi.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, GUILD_SKILL_DRAGONBLESS, "guild_yongsinuichukbok.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, GUILD_SKILL_BLESSARMOR, "guild_seonghwigap.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, GUILD_SKILL_SPPEDUP, "guild_gasokhwa.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, GUILD_SKILL_DRAGONWRATH, "guild_yongsinuibunno.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, GUILD_SKILL_MAGICUP, "guild_jumunsul.msa")
	
	chrmgr.ReserveComboAttackNew(chr.MOTION_MODE_GENERAL, COMBO_TYPE_1, 1)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_GENERAL, COMBO_TYPE_1, COMBO_INDEX_1, chr.MOTION_COMBO_ATTACK_1)
	
	emotion.RegisterEmotionAnis(path)
	
	chrmgr.SetPathName(path + "onehand_sword/")
	chrmgr.RegisterMotionMode(chr.MOTION_MODE_ONEHAND_SWORD)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_WAIT, "wait.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_WALK, "walk.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_RUN, "run.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_DAMAGE, "damage.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_DAMAGE, "damage.msa", 50)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_DAMAGE, "damage_1.msa", 50)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_DAMAGE_BACK, "damage_2.msa", 50)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_DAMAGE_BACK, "damage_3.msa", 50)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_COMBO_ATTACK_1, "combo_01.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_COMBO_ATTACK_2, "combo_02.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_COMBO_ATTACK_3, "combo_03.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_COMBO_ATTACK_4, "combo_04.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_COMBO_ATTACK_5, "combo_05.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_COMBO_ATTACK_6, "combo_06.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_ONEHAND_SWORD, chr.MOTION_COMBO_ATTACK_7, "combo_07.msa")
	chrmgr.ReserveComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_1, 4)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_1, COMBO_INDEX_1, chr.MOTION_COMBO_ATTACK_1)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_1, COMBO_INDEX_2, chr.MOTION_COMBO_ATTACK_2)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_1, COMBO_INDEX_3, chr.MOTION_COMBO_ATTACK_3)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_1, COMBO_INDEX_4, chr.MOTION_COMBO_ATTACK_4)
	chrmgr.ReserveComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_2, 5)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_2, COMBO_INDEX_1, chr.MOTION_COMBO_ATTACK_1)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_2, COMBO_INDEX_2, chr.MOTION_COMBO_ATTACK_2)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_2, COMBO_INDEX_3, chr.MOTION_COMBO_ATTACK_3)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_2, COMBO_INDEX_4, chr.MOTION_COMBO_ATTACK_5)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_2, COMBO_INDEX_5, chr.MOTION_COMBO_ATTACK_7)
	chrmgr.ReserveComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_3, 6)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_3, COMBO_INDEX_1, chr.MOTION_COMBO_ATTACK_1)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_3, COMBO_INDEX_2, chr.MOTION_COMBO_ATTACK_2)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_3, COMBO_INDEX_3, chr.MOTION_COMBO_ATTACK_3)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_3, COMBO_INDEX_4, chr.MOTION_COMBO_ATTACK_5)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_3, COMBO_INDEX_5, chr.MOTION_COMBO_ATTACK_6)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_ONEHAND_SWORD, COMBO_TYPE_3, COMBO_INDEX_6, chr.MOTION_COMBO_ATTACK_4)
	
	chrmgr.SetPathName(path + "fishing/")
	chrmgr.RegisterMotionMode(chr.MOTION_MODE_FISHING)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FISHING, chr.MOTION_WAIT, "wait.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FISHING, chr.MOTION_WALK, "walk.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FISHING, chr.MOTION_RUN, "run.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FISHING, chr.MOTION_FISHING_THROW, "throw.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FISHING, chr.MOTION_FISHING_WAIT, "fishing_wait.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FISHING, chr.MOTION_FISHING_STOP, "fishing_cancel.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FISHING, chr.MOTION_FISHING_REACT, "fishing_react.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FISHING, chr.MOTION_FISHING_CATCH, "fishing_catch.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FISHING, chr.MOTION_FISHING_FAIL, "fishing_fail.msa")
	
	chrmgr.SetPathName(path + "horse/")
	chrmgr.RegisterMotionMode(chr.MOTION_MODE_HORSE)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE, chr.MOTION_WAIT, "wait.msa", 90)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE, chr.MOTION_WAIT, "wait_1.msa", 9)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE, chr.MOTION_WAIT, "wait_2.msa", 1)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE, chr.MOTION_WALK, "walk.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE, chr.MOTION_RUN, "run.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE, chr.MOTION_DAMAGE, "damage.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE, chr.MOTION_DAMAGE_BACK, "damage.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE, chr.MOTION_DEAD, "dead.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE, HORSE_SKILL_CHARGE, "skill_charge.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE, HORSE_SKILL_SPLASH, "skill_splash.msa")
	
	chrmgr.SetPathName(path + "horse_onehand_sword/")
	chrmgr.RegisterMotionMode(chr.MOTION_MODE_HORSE_ONEHAND_SWORD)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_ONEHAND_SWORD, chr.MOTION_COMBO_ATTACK_1, "combo_01.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_ONEHAND_SWORD, chr.MOTION_COMBO_ATTACK_2, "combo_02.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_ONEHAND_SWORD, chr.MOTION_COMBO_ATTACK_3, "combo_03.msa")
	chrmgr.ReserveComboAttackNew(chr.MOTION_MODE_HORSE_ONEHAND_SWORD, COMBO_TYPE_1, 3)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_HORSE_ONEHAND_SWORD, COMBO_TYPE_1, COMBO_INDEX_1, chr.MOTION_COMBO_ATTACK_1)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_HORSE_ONEHAND_SWORD, COMBO_TYPE_1, COMBO_INDEX_2, chr.MOTION_COMBO_ATTACK_2)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_HORSE_ONEHAND_SWORD, COMBO_TYPE_1, COMBO_INDEX_3, chr.MOTION_COMBO_ATTACK_3)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_ONEHAND_SWORD, HORSE_SKILL_WILDATTACK, "skill_wildattack.msa")
	
	chrmgr.RegisterAttachingBoneName(chr.PART_WEAPON, "equip_right")
	if app.ENABLE_ACCE_SYSTEM:
		chrmgr.RegisterAttachingBoneName(chr.PART_ACCE, "Bip01 Spine2")

def LoadGameShamanEx(race, path):
	chrmgr.SelectRace(race)
	
	SetGeneralMotions(chr.MOTION_MODE_GENERAL, path + "general/")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_COMBO_ATTACK_1, "attack.msa", 50)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_COMBO_ATTACK_1, "attack_1.msa", 50)
	
	chrmgr.SetPathName(path + "skill/")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+1, "bipabu.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+2, "yongpa.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+3, "paeryong.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+4, "hosin_target.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+5, "boho_target.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+6, "gicheon_target.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+16, "noejeon.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+17, "byeorak.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+18, "pokroe.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+19, "jeongeop_target.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+20, "kwaesok_target.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+21, "jeungryeok_target.msa")
	SHAMY_LIST = (1, 2, 3)
	for i in SHAMY_LIST:
		END_STRING = ""
		if i != 0: END_STRING = "_%d" % (i+1)
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+1, "bipabu" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+2, "yongpa" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+3, "paeryong" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+4, "hosin" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+5, "boho" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+6, "gicheon" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+16, "noejeon" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+17, "byeorak" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+18, "pokroe" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+19, "jeongeop" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+20, "kwaesok" + END_STRING + ".msa")
		chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_SKILL+(i*skill.SKILL_GRADEGAP)+21, "jeungryeok" + END_STRING + ".msa")
	
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, GUILD_SKILL_DRAGONBLOOD, "guild_yongsinuipi.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, GUILD_SKILL_DRAGONBLESS, "guild_yongsinuichukbok.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, GUILD_SKILL_BLESSARMOR, "guild_seonghwigap.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, GUILD_SKILL_SPPEDUP, "guild_gasokhwa.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, GUILD_SKILL_DRAGONWRATH, "guild_yongsinuibunno.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_GENERAL, GUILD_SKILL_MAGICUP, "guild_jumunsul.msa")
	
	chrmgr.ReserveComboAttackNew(chr.MOTION_MODE_GENERAL, COMBO_TYPE_1, 1)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_GENERAL, COMBO_TYPE_1, COMBO_INDEX_1, chr.MOTION_COMBO_ATTACK_1)
	
	emotion.RegisterEmotionAnis(path)
	
	chrmgr.SetPathName(path + "fan/")
	chrmgr.RegisterMotionMode(chr.MOTION_MODE_FAN)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FAN, chr.MOTION_WAIT, "wait.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FAN, chr.MOTION_WALK, "walk.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FAN, chr.MOTION_RUN, "run.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FAN, chr.MOTION_DAMAGE, "damage.msa", 50)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FAN, chr.MOTION_DAMAGE, "damage_1.msa", 50)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FAN, chr.MOTION_DAMAGE_BACK, "damage_2.msa", 50)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FAN, chr.MOTION_DAMAGE_BACK, "damage_3.msa", 50)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FAN, chr.MOTION_COMBO_ATTACK_1, "combo_01.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FAN, chr.MOTION_COMBO_ATTACK_2, "combo_02.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FAN, chr.MOTION_COMBO_ATTACK_3, "combo_03.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FAN, chr.MOTION_COMBO_ATTACK_4, "combo_04.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FAN, chr.MOTION_COMBO_ATTACK_5, "combo_05.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FAN, chr.MOTION_COMBO_ATTACK_6, "combo_06.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FAN, chr.MOTION_COMBO_ATTACK_7, "combo_07.msa")
	chrmgr.ReserveComboAttackNew(chr.MOTION_MODE_FAN, COMBO_TYPE_1, 4)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_FAN, COMBO_TYPE_1, COMBO_INDEX_1, chr.MOTION_COMBO_ATTACK_1)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_FAN, COMBO_TYPE_1, COMBO_INDEX_2, chr.MOTION_COMBO_ATTACK_2)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_FAN, COMBO_TYPE_1, COMBO_INDEX_3, chr.MOTION_COMBO_ATTACK_3)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_FAN, COMBO_TYPE_1, COMBO_INDEX_4, chr.MOTION_COMBO_ATTACK_4)
	chrmgr.ReserveComboAttackNew(chr.MOTION_MODE_FAN, COMBO_TYPE_2, 5)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_FAN, COMBO_TYPE_2, COMBO_INDEX_1, chr.MOTION_COMBO_ATTACK_1)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_FAN, COMBO_TYPE_2, COMBO_INDEX_2, chr.MOTION_COMBO_ATTACK_2)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_FAN, COMBO_TYPE_2, COMBO_INDEX_3, chr.MOTION_COMBO_ATTACK_3)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_FAN, COMBO_TYPE_2, COMBO_INDEX_4, chr.MOTION_COMBO_ATTACK_5)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_FAN, COMBO_TYPE_2, COMBO_INDEX_5, chr.MOTION_COMBO_ATTACK_7)
	chrmgr.ReserveComboAttackNew(chr.MOTION_MODE_FAN, COMBO_TYPE_3, 6)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_FAN, COMBO_TYPE_3, COMBO_INDEX_1, chr.MOTION_COMBO_ATTACK_1)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_FAN, COMBO_TYPE_3, COMBO_INDEX_2, chr.MOTION_COMBO_ATTACK_2)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_FAN, COMBO_TYPE_3, COMBO_INDEX_3, chr.MOTION_COMBO_ATTACK_3)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_FAN, COMBO_TYPE_3, COMBO_INDEX_4, chr.MOTION_COMBO_ATTACK_5)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_FAN, COMBO_TYPE_3, COMBO_INDEX_5, chr.MOTION_COMBO_ATTACK_6)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_FAN, COMBO_TYPE_3, COMBO_INDEX_6, chr.MOTION_COMBO_ATTACK_4)
	
	chrmgr.SetPathName(path + "Bell/")
	chrmgr.RegisterMotionMode(chr.MOTION_MODE_BELL)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_BELL, chr.MOTION_WAIT, "wait.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_BELL, chr.MOTION_WALK, "walk.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_BELL, chr.MOTION_RUN, "run.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_BELL, chr.MOTION_DAMAGE, "damage.msa", 50)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_BELL, chr.MOTION_DAMAGE, "damage_1.msa", 50)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_BELL, chr.MOTION_DAMAGE_BACK, "damage_2.msa", 50)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_BELL, chr.MOTION_DAMAGE_BACK, "damage_3.msa", 50)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_BELL, chr.MOTION_COMBO_ATTACK_1, "combo_01.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_BELL, chr.MOTION_COMBO_ATTACK_2, "combo_02.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_BELL, chr.MOTION_COMBO_ATTACK_3, "combo_03.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_BELL, chr.MOTION_COMBO_ATTACK_4, "combo_04.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_BELL, chr.MOTION_COMBO_ATTACK_5, "combo_05.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_BELL, chr.MOTION_COMBO_ATTACK_6, "combo_06.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_BELL, chr.MOTION_COMBO_ATTACK_7, "combo_07.msa")
	chrmgr.ReserveComboAttackNew(chr.MOTION_MODE_BELL, COMBO_TYPE_1, 4)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_BELL, COMBO_TYPE_1, COMBO_INDEX_1, chr.MOTION_COMBO_ATTACK_1)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_BELL, COMBO_TYPE_1, COMBO_INDEX_2, chr.MOTION_COMBO_ATTACK_2)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_BELL, COMBO_TYPE_1, COMBO_INDEX_3, chr.MOTION_COMBO_ATTACK_3)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_BELL, COMBO_TYPE_1, COMBO_INDEX_4, chr.MOTION_COMBO_ATTACK_4)
	chrmgr.ReserveComboAttackNew(chr.MOTION_MODE_BELL, COMBO_TYPE_2, 5)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_BELL, COMBO_TYPE_2, COMBO_INDEX_1, chr.MOTION_COMBO_ATTACK_1)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_BELL, COMBO_TYPE_2, COMBO_INDEX_2, chr.MOTION_COMBO_ATTACK_2)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_BELL, COMBO_TYPE_2, COMBO_INDEX_3, chr.MOTION_COMBO_ATTACK_3)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_BELL, COMBO_TYPE_2, COMBO_INDEX_4, chr.MOTION_COMBO_ATTACK_5)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_BELL, COMBO_TYPE_2, COMBO_INDEX_5, chr.MOTION_COMBO_ATTACK_7)
	chrmgr.ReserveComboAttackNew(chr.MOTION_MODE_BELL, COMBO_TYPE_3, 6)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_BELL, COMBO_TYPE_3, COMBO_INDEX_1, chr.MOTION_COMBO_ATTACK_1)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_BELL, COMBO_TYPE_3, COMBO_INDEX_2, chr.MOTION_COMBO_ATTACK_2)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_BELL, COMBO_TYPE_3, COMBO_INDEX_3, chr.MOTION_COMBO_ATTACK_3)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_BELL, COMBO_TYPE_3, COMBO_INDEX_4, chr.MOTION_COMBO_ATTACK_5)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_BELL, COMBO_TYPE_3, COMBO_INDEX_5, chr.MOTION_COMBO_ATTACK_6)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_BELL, COMBO_TYPE_3, COMBO_INDEX_6, chr.MOTION_COMBO_ATTACK_4)
	
	chrmgr.SetPathName(path + "fishing/")
	chrmgr.RegisterMotionMode(chr.MOTION_MODE_FISHING)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FISHING, chr.MOTION_WAIT, "wait.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FISHING, chr.MOTION_WALK, "walk.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FISHING, chr.MOTION_RUN, "run.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FISHING, chr.MOTION_FISHING_THROW, "throw.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FISHING, chr.MOTION_FISHING_WAIT, "fishing_wait.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FISHING, chr.MOTION_FISHING_STOP, "fishing_cancel.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FISHING, chr.MOTION_FISHING_REACT, "fishing_react.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FISHING, chr.MOTION_FISHING_CATCH, "fishing_catch.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_FISHING, chr.MOTION_FISHING_FAIL, "fishing_fail.msa")
	
	chrmgr.SetPathName(path + "horse/")
	chrmgr.RegisterMotionMode(chr.MOTION_MODE_HORSE)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE, chr.MOTION_WAIT, "wait.msa", 90)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE, chr.MOTION_WAIT, "wait_1.msa", 9)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE, chr.MOTION_WAIT, "wait_2.msa", 1)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE, chr.MOTION_WALK, "walk.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE, chr.MOTION_RUN, "run.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE, chr.MOTION_DAMAGE, "damage.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE, chr.MOTION_DAMAGE_BACK, "damage.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE, chr.MOTION_DEAD, "dead.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE, HORSE_SKILL_CHARGE, "skill_charge.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE, HORSE_SKILL_SPLASH, "skill_splash.msa")
	
	chrmgr.SetPathName(path + "horse_fan/")
	chrmgr.RegisterMotionMode(chr.MOTION_MODE_HORSE_FAN)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_FAN, chr.MOTION_COMBO_ATTACK_1, "combo_01.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_FAN, chr.MOTION_COMBO_ATTACK_2, "combo_02.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_FAN, chr.MOTION_COMBO_ATTACK_3, "combo_03.msa")
	chrmgr.ReserveComboAttackNew(chr.MOTION_MODE_HORSE_FAN, COMBO_TYPE_1, 3)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_HORSE_FAN, COMBO_TYPE_1, COMBO_INDEX_1, chr.MOTION_COMBO_ATTACK_1)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_HORSE_FAN, COMBO_TYPE_1, COMBO_INDEX_2, chr.MOTION_COMBO_ATTACK_2)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_HORSE_FAN, COMBO_TYPE_1, COMBO_INDEX_3, chr.MOTION_COMBO_ATTACK_3)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_FAN, HORSE_SKILL_WILDATTACK, "skill_wildattack.msa")
	
	chrmgr.SetPathName(path + "horse_bell/")
	chrmgr.RegisterMotionMode(chr.MOTION_MODE_HORSE_BELL)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_BELL, chr.MOTION_COMBO_ATTACK_1, "combo_01.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_BELL, chr.MOTION_COMBO_ATTACK_2, "combo_02.msa")
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_BELL, chr.MOTION_COMBO_ATTACK_3, "combo_03.msa")
	chrmgr.ReserveComboAttackNew(chr.MOTION_MODE_HORSE_BELL, COMBO_TYPE_1, 3)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_HORSE_BELL, COMBO_TYPE_1, COMBO_INDEX_1, chr.MOTION_COMBO_ATTACK_1)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_HORSE_BELL, COMBO_TYPE_1, COMBO_INDEX_2, chr.MOTION_COMBO_ATTACK_2)
	chrmgr.RegisterComboAttackNew(chr.MOTION_MODE_HORSE_BELL, COMBO_TYPE_1, COMBO_INDEX_3, chr.MOTION_COMBO_ATTACK_3)
	chrmgr.RegisterCacheMotionData(chr.MOTION_MODE_HORSE_BELL, HORSE_SKILL_WILDATTACK, "skill_wildattack.msa")
	
	chrmgr.RegisterAttachingBoneName(chr.PART_WEAPON, "equip_right")
	chrmgr.RegisterAttachingBoneName(chr.PART_WEAPON_LEFT, "equip_left")
	if app.ENABLE_ACCE_SYSTEM:
		chrmgr.RegisterAttachingBoneName(chr.PART_ACCE, "Bip01 Spine2")

def LoadGameSkill():
	try:
		skill.LoadSkillData()
	except:
		import exception
		exception.Abort("LoadGameSkill")

if app.ENABLE_RACE_HEIGHT:
	def LoadRaceHeight():
		try:
			lines = open("data/monsters/race_height.txt", "r").readlines()
		except IOError:
			return
		
		for line in lines:
			tokens = line[:-1].split("\t")
			if len(tokens) == 0 or not tokens[0]:
				continue
			
			vnum = int(tokens[0])
			height = float(tokens[1])
			riding = float(tokens[2])
			
			chrmgr.SetRaceHeight(vnum, height, riding)

def LoadGuildBuildingList(filename):
	import uiguild
	uiguild.BUILDING_DATA_LIST = []
	
	handle = app.OpenTextFile(filename)
	count = app.GetTextFileLineCount(handle)
	for i in xrange(count):
		line = app.GetTextFileLine(handle, i)
		tokens = line.split("\t")
		TOKEN_VNUM = 0
		TOKEN_TYPE = 1
		TOKEN_NAME = 2
		TOKEN_LOCAL_NAME = 3
		NO_USE_TOKEN_SIZE_1 = 4
		NO_USE_TOKEN_SIZE_2 = 5
		NO_USE_TOKEN_SIZE_3 = 6
		NO_USE_TOKEN_SIZE_4 = 7
		TOKEN_X_ROT_LIMIT = 8
		TOKEN_Y_ROT_LIMIT = 9
		TOKEN_Z_ROT_LIMIT = 10
		TOKEN_PRICE = 11
		TOKEN_MATERIAL = 12
		TOKEN_NPC = 13
		TOKEN_GROUP = 14
		TOKEN_DEPEND_GROUP = 15
		TOKEN_ENABLE_FLAG = 16
		LIMIT_TOKEN_COUNT = 17
		if not tokens[TOKEN_VNUM].isdigit():
			continue
		
		if len(tokens) < LIMIT_TOKEN_COUNT:
			import dbg
			dbg.TraceError("Strange token count [%d/%d] [%s]" % (len(tokens), LIMIT_TOKEN_COUNT, line))
			continue
		
		ENABLE_FLAG_TYPE_NOT_USE = False
		ENABLE_FLAG_TYPE_USE = True
		ENABLE_FLAG_TYPE_USE_BUT_HIDE = 2
		
		if ENABLE_FLAG_TYPE_NOT_USE == int(tokens[TOKEN_ENABLE_FLAG]):
			continue
		
		vnum = int(tokens[TOKEN_VNUM])
		type = tokens[TOKEN_TYPE]
		name = tokens[TOKEN_NAME]
		localName = tokens[TOKEN_LOCAL_NAME]
		xRotLimit = int(tokens[TOKEN_X_ROT_LIMIT])
		yRotLimit = int(tokens[TOKEN_Y_ROT_LIMIT])
		zRotLimit = int(tokens[TOKEN_Z_ROT_LIMIT])
		price = tokens[TOKEN_PRICE]
		material = tokens[TOKEN_MATERIAL]
		
		folderName = ""
		if "HEADQUARTER" == type:
			folderName = "headquarter"
		elif "FACILITY" == type:
			folderName = "facility"
		elif "OBJECT" == type:
			folderName = "object"
		elif "WALL" == type:
			folderName = "fence"
		
		materialList = ["0", "0", "0"]
		if material:
			if material[0] == "\"":
				material = material[1:]
			
			if material[-1] == "\"":
				material = material[:-1]
			
			for one in material.split("/"):
				data = one.split(",")
				if 2 != len(data):
					continue
					
				itemID = int(data[0])
				count = data[1]
				
				if itemID == uiguild.MATERIAL_STONE_ID:
					materialList[uiguild.MATERIAL_STONE_INDEX] = count
				elif itemID == uiguild.MATERIAL_LOG_ID:
					materialList[uiguild.MATERIAL_LOG_INDEX] = count
				elif itemID == uiguild.MATERIAL_PLYWOOD_ID:
					materialList[uiguild.MATERIAL_PLYWOOD_INDEX] = count
		
		chrmgr.RegisterRaceSrcName(name, folderName)
		chrmgr.RegisterRaceName(vnum, name)
		appendingData = { "VNUM":vnum,
						  "TYPE":type,
						  "NAME":name,
						  "LOCAL_NAME":localName,
						  "X_ROT_LIMIT":xRotLimit,
						  "Y_ROT_LIMIT":yRotLimit,
						  "Z_ROT_LIMIT":zRotLimit,
						  "PRICE":price,
						  "MATERIAL":materialList,
						  "SHOW" : True }
		
		if ENABLE_FLAG_TYPE_USE_BUT_HIDE == int(tokens[TOKEN_ENABLE_FLAG]):
			appendingData["SHOW"] = False
		
		uiguild.BUILDING_DATA_LIST.append(appendingData)
	
	app.CloseTextFile(handle)

loadGameDataDict={
	"INIT" : InitData,
	"SOUND" : LoadGameSound,
	"EFFECT" : LoadGameEffect,
	"NPC" : LoadGameNPC,
	"WARRIOR" : LoadGameWarrior,
	"ASSASSIN" : LoadGameAssassin,
	"SURA" : LoadGameSura,
	"SHAMAN" : LoadGameShaman,
	"SKILL" : LoadGameSkill,
}
if app.ENABLE_RACE_HEIGHT:
	loadGameDataDict["RACE_HEIGHT"] = LoadRaceHeight

def LoadGameData(name):
	global loadGameDataDict
	load = loadGameDataDict.get(name, 0)
	if load:
		loadGameDataDict[name] = 0
		try:
			load()
		except:
			print name
			import exception
			exception.Abort("LoadGameData")
			raise

