import app
import chr
import chrmgr
import player
import localeinfo

EMOTION_CLAP = 1
EMOTION_CONGRATULATION = 2
EMOTION_FORGIVE = 3
EMOTION_ANGRY = 4
EMOTION_ATTRACTIVE = 5
EMOTION_SAD = 6
EMOTION_SHY = 7
EMOTION_CHEERUP = 8
EMOTION_BANTER = 9
EMOTION_JOY = 10
EMOTION_CHEERS_1 = 11
EMOTION_CHEERS_2 = 12
EMOTION_DANCE_1 = 13
EMOTION_DANCE_2 = 14
EMOTION_DANCE_3 = 15
EMOTION_DANCE_4 = 16
EMOTION_DANCE_5 = 17
EMOTION_DANCE_6 = 18
EMOTION_SELFIE = 19
EMOTION_DANCE_7 = 20
EMOTION_DOZE = 21
EMOTION_EXERCISE = 22
EMOTION_PUSHUP = 23
EMOTION_SIRENA = 24
EMOTION_CALL = 25
EMOTION_CHARGING = 26
EMOTION_CELEBRATION = 27
EMOTION_WEATHER = 28
EMOTION_ALCOHOL = 29
EMOTION_HUNGRY = 30
EMOTION_KISS = 51
EMOTION_FRENCH_KISS = 52
EMOTION_SLAP = 53

EMOTION_DICT = {
	EMOTION_CLAP : {"name": localeinfo.EMOTION_CLAP, "command":"/clap"},
	EMOTION_DANCE_1 : {"name": localeinfo.EMOTION_DANCE_1, "command":"/dance1"},
	EMOTION_DANCE_2 : {"name": localeinfo.EMOTION_DANCE_2, "command":"/dance2"},
	EMOTION_DANCE_3 : {"name": localeinfo.EMOTION_DANCE_3, "command":"/dance3"},
	EMOTION_DANCE_4 : {"name": localeinfo.EMOTION_DANCE_4, "command":"/dance4"},
	EMOTION_DANCE_5 : {"name": localeinfo.EMOTION_DANCE_5, "command":"/dance5"},
	EMOTION_DANCE_6 : {"name": localeinfo.EMOTION_DANCE_6, "command":"/dance6"},
	EMOTION_CONGRATULATION : {"name": localeinfo.EMOTION_CONGRATULATION, "command":"/congratulation"},
	EMOTION_FORGIVE : {"name": localeinfo.EMOTION_FORGIVE, "command":"/forgive"},
	EMOTION_ANGRY : {"name": localeinfo.EMOTION_ANGRY, "command":"/angry"},
	EMOTION_ATTRACTIVE : {"name": localeinfo.EMOTION_ATTRACTIVE, "command":"/attractive"},
	EMOTION_SAD : {"name": localeinfo.EMOTION_SAD, "command":"/sad"},
	EMOTION_SHY : {"name": localeinfo.EMOTION_SHY, "command":"/shy"},
	EMOTION_CHEERUP : {"name": localeinfo.EMOTION_CHEERUP, "command":"/cheerup"},
	EMOTION_BANTER : {"name": localeinfo.EMOTION_BANTER, "command":"/banter"},
	EMOTION_JOY : {"name": localeinfo.EMOTION_JOY, "command":"/joy"},
	EMOTION_CHEERS_1 : {"name": localeinfo.EMOTION_CHEERS_1, "command":"/cheer1"},
	EMOTION_CHEERS_2 : {"name": localeinfo.EMOTION_CHEERS_2, "command":"/cheer2"},
	EMOTION_KISS : {"name": localeinfo.EMOTION_CLAP_KISS, "command":"/kiss"},
	EMOTION_FRENCH_KISS : {"name": localeinfo.EMOTION_FRENCH_KISS, "command":"/french_kiss"},
	EMOTION_SLAP : {"name": localeinfo.EMOTION_SLAP, "command":"/slap"},
	EMOTION_SELFIE : {"name": localeinfo.EMOTION_SELFIE, "command":"/selfie"},
	EMOTION_DANCE_7 : {"name": localeinfo.EMOTION_DANCE_7, "command":"/dance7"},
	EMOTION_DOZE : {"name": localeinfo.EMOTION_DOZE, "command":"/doze"},
	EMOTION_EXERCISE : {"name": localeinfo.EMOTION_EXERCISE, "command":"/exercise"},
	EMOTION_PUSHUP : {"name": localeinfo.EMOTION_PUSHUP, "command":"/pushup"},
	EMOTION_SIRENA : {"name": localeinfo.EMOTION_SIRENA, "command":"(emoji1)"},
	EMOTION_CALL : {"name": localeinfo.EMOTION_CALL, "command":"(emoji2)"},
	EMOTION_CHARGING : {"name": localeinfo.EMOTION_CHARGING, "command":"(emoji3)"},
	EMOTION_CELEBRATION : {"name": localeinfo.EMOTION_CELEBRATION, "command":"(emoji4)"},
	EMOTION_WEATHER : {"name": localeinfo.EMOTION_WEATHER, "command":"(emoji5)"},
	EMOTION_ALCOHOL : {"name": localeinfo.EMOTION_ALCOHOL, "command":"(emoji6)"},
	EMOTION_HUNGRY : {"name": localeinfo.EMOTION_HUNGRY, "command":"(emoji7)"},
}

ICON_DICT = {
	EMOTION_CLAP : "d:/ymir work/ui/game/windows/emotion_clap.sub",
	EMOTION_CHEERS_1 : "d:/ymir work/ui/game/windows/emotion_cheers_1.sub",
	EMOTION_CHEERS_2 : "d:/ymir work/ui/game/windows/emotion_cheers_2.sub",
	EMOTION_DANCE_1 : "icon/action/dance1.tga",
	EMOTION_DANCE_2 : "icon/action/dance2.tga",
	EMOTION_CONGRATULATION : "icon/action/congratulation.tga",
	EMOTION_FORGIVE : "icon/action/forgive.tga",
	EMOTION_ANGRY :	"icon/action/angry.tga",
	EMOTION_ATTRACTIVE : "icon/action/attractive.tga",
	EMOTION_SAD : "icon/action/sad.tga",
	EMOTION_SHY : "icon/action/shy.tga",
	EMOTION_CHEERUP : "icon/action/cheerup.tga",
	EMOTION_BANTER : "icon/action/banter.tga",
	EMOTION_JOY : "icon/action/joy.tga",
	EMOTION_DANCE_1 : "icon/action/dance1.tga",
	EMOTION_DANCE_2 : "icon/action/dance2.tga",
	EMOTION_DANCE_3 : "icon/action/dance3.tga",
	EMOTION_DANCE_4 : "icon/action/dance4.tga",
	EMOTION_DANCE_5 : "icon/action/dance5.tga",
	EMOTION_DANCE_6 : "icon/action/dance6.tga",
	EMOTION_KISS : "d:/ymir work/ui/game/windows/emotion_kiss.sub",
	EMOTION_FRENCH_KISS	: "d:/ymir work/ui/game/windows/emotion_french_kiss.sub",
	EMOTION_SLAP : "d:/ymir work/ui/game/windows/emotion_slap.sub",
	EMOTION_SELFIE : "icon/action/selfie.tga",
	EMOTION_DANCE_7 : "icon/action/dance7.tga",
	EMOTION_DOZE : "icon/action/doze.tga",
	EMOTION_EXERCISE : "icon/action/exercise.tga",
	EMOTION_PUSHUP : "icon/action/pushup.tga",
	EMOTION_SIRENA : "icon/action/siren.tga",
	EMOTION_CALL : "icon/action/call.tga",
	EMOTION_CHARGING : "icon/action/charging.tga",
	EMOTION_CELEBRATION : "icon/action/celebration.tga",
	EMOTION_WEATHER : "icon/action/weather1.tga",
	EMOTION_ALCOHOL : "icon/action/alcohol.tga",
	EMOTION_HUNGRY : "icon/action/hungry.tga",
}

ANI_DICT = {
	chr.MOTION_CLAP : "clap.msa",
	chr.MOTION_CHEERS_1 : "cheers_1.msa",
	chr.MOTION_CHEERS_2 : "cheers_2.msa",
	chr.MOTION_DANCE_1 : "dance_1.msa",
	chr.MOTION_DANCE_2 : "dance_2.msa",
	chr.MOTION_DANCE_3 : "dance_3.msa",
	chr.MOTION_DANCE_4 : "dance_4.msa",
	chr.MOTION_DANCE_5 : "dance_5.msa",
	chr.MOTION_DANCE_6 : "dance_6.msa",
	chr.MOTION_CONGRATULATION : "congratulation.msa",
	chr.MOTION_CONGRATULATION : "congratulation.msa",
	chr.MOTION_FORGIVE : "forgive.msa",
	chr.MOTION_ANGRY : "angry.msa",
	chr.MOTION_ATTRACTIVE : "attractive.msa",
	chr.MOTION_SAD : "sad.msa",
	chr.MOTION_SHY : "shy.msa",
	chr.MOTION_CHEERUP : "cheerup.msa",
	chr.MOTION_BANTER : "banter.msa",
	chr.MOTION_JOY : "joy.msa",
	chr.MOTION_FRENCH_KISS_WITH_WARRIOR : "french_kiss_with_warrior.msa",
	chr.MOTION_FRENCH_KISS_WITH_ASSASSIN : "french_kiss_with_assassin.msa",
	chr.MOTION_FRENCH_KISS_WITH_SURA : "french_kiss_with_sura.msa",
	chr.MOTION_FRENCH_KISS_WITH_SHAMAN : "french_kiss_with_shaman.msa",
	chr.MOTION_KISS_WITH_WARRIOR : "kiss_with_warrior.msa",
	chr.MOTION_KISS_WITH_ASSASSIN : "kiss_with_assassin.msa",
	chr.MOTION_KISS_WITH_SURA : "kiss_with_sura.msa",
	chr.MOTION_KISS_WITH_SHAMAN : "kiss_with_shaman.msa",
	chr.MOTION_SLAP_HIT_WITH_WARRIOR : "slap_hit.msa",
	chr.MOTION_SLAP_HIT_WITH_ASSASSIN : "slap_hit.msa",
	chr.MOTION_SLAP_HIT_WITH_SURA : "slap_hit.msa",
	chr.MOTION_SLAP_HIT_WITH_SHAMAN : "slap_hit.msa",
	chr.MOTION_SLAP_HURT_WITH_WARRIOR : "slap_hurt.msa",
	chr.MOTION_SLAP_HURT_WITH_ASSASSIN : "slap_hurt.msa",
	chr.MOTION_SLAP_HURT_WITH_SURA : "slap_hurt.msa",
	chr.MOTION_SLAP_HURT_WITH_SHAMAN : "slap_hurt.msa",
	chr.MOTION_SELFIE : "selfie.msa",
	chr.MOTION_DANCE_7 : "dance_7.msa",
	chr.MOTION_DOZE : "doze.msa",
	chr.MOTION_EXERCISE : "exercise.msa",
	chr.MOTION_PUSHUP : "pushup.msa",
}

if app.ENABLE_WOLFMAN_CHARACTER:
	ANI_DICT.update({
		chr.MOTION_FRENCH_KISS_WITH_WOLFMAN : "french_kiss_with_wolfman.msa",
		chr.MOTION_KISS_WITH_WOLFMAN : "kiss_with_wolfman.msa",
		chr.MOTION_SLAP_HIT_WITH_WOLFMAN : "slap_hit.msa",
		chr.MOTION_SLAP_HURT_WITH_WOLFMAN : "slap_hurt.msa",
	})

def __RegisterSharedEmotionAnis(mode, path):
	chrmgr.SetPathName(path)
	chrmgr.RegisterMotionMode(mode)
	for key, val in ANI_DICT.items():
		chrmgr.RegisterMotionData(mode, key, val)

def RegisterEmotionAnis(path):
	actionPath = path + "action/"
	weddingPath = path + "wedding/"
	__RegisterSharedEmotionAnis(chr.MOTION_MODE_GENERAL, actionPath)
	__RegisterSharedEmotionAnis(chr.MOTION_MODE_WEDDING_DRESS, actionPath)
	
	chrmgr.SetPathName(weddingPath)
	chrmgr.RegisterMotionMode(chr.MOTION_MODE_WEDDING_DRESS)
	chrmgr.RegisterMotionData(chr.MOTION_MODE_WEDDING_DRESS, chr.MOTION_WAIT, "wait.msa")
	chrmgr.RegisterMotionData(chr.MOTION_MODE_WEDDING_DRESS, chr.MOTION_WALK, "walk.msa")
	chrmgr.RegisterMotionData(chr.MOTION_MODE_WEDDING_DRESS, chr.MOTION_RUN, "walk.msa")

def RegisterEmotionIcons():
	for key, val in ICON_DICT.items():
		player.RegisterEmotionIcon(key, val)

