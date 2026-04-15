import app
import chr
import net
import player
import ui
import uiscriptlocale
import dbg
import snd
import mousemodule
import wndMgr
import skill
import playersettingmodule
import quest
import uitooltip
import constinfo
import uicharacterdetails
import emotion
if app.ENABLE_GUILD_ATTRIBUTE:
	import guild
	import grp
import localeinfo
if app.ENABLE_SKILL_COLOR_SYSTEM:
	import uiskillcolor

if app.ENABLE_QUEST_RENEWAL:
	import math, uiquest

ENABLE_GUI_SELECT = True

SHOW_ONLY_ACTIVE_SKILL = False
SHOW_LIMIT_SUPPORT_SKILL_LIST = []
HIDE_SUPPORT_SKILL_POINT = False
ALIGNMENT_BONUS_LIST = {
	0 : (
		"Point: 0 - 4999",
		"Bonus:",
		localeinfo.TOOLTIP_MAX_HP(500),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_MONSTER(1),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN(1),

	),
	1 : (
		"Point: 5000 - 14999",

		"Bonus:",
		localeinfo.TOOLTIP_MAX_HP(1000),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_MONSTER(3),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN(3),
	),
	2 : (
		"Point: 15000 - 19999",

		"Bonus:",
		localeinfo.TOOLTIP_MAX_HP(1500),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_MONSTER(5),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN(5),
	),
	3 : (
		"Point: 20000 - 29999",

		"Bonus:",
		localeinfo.TOOLTIP_MAX_HP(2000),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_MONSTER(7),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN(7),
	),
	4 : (
		"Point: 30000 - 49999",

		"Bonus:",
		localeinfo.TOOLTIP_MAX_HP(2500),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_MONSTER(9),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN(9),
	),
	5 : (
		"Point: 50000 - 74999",

		"Bonus:",
		localeinfo.TOOLTIP_MAX_HP(4000),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_MONSTER(12),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN(12),
	),
	6 : (
		"Point: 75000 - 99999",

		"Bonus:",
		localeinfo.TOOLTIP_MAX_HP(6000),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_MONSTER(15),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN(15),
	),
	7 : (
		"Point: 100000 - 124999",

		"Bonus:",
		localeinfo.TOOLTIP_MAX_HP(8000),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_MONSTER(18),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN(18),
	),
	8 : (
		"Point: 125000 - 174999",

		"Bonus:",
		localeinfo.TOOLTIP_MAX_HP(10000),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_MONSTER(21),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN(21),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_METIN(5),
	),
	9 : (
		"Point: 175000 - 249999",

		"Bonus:",
		localeinfo.TOOLTIP_MAX_HP(12000),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_MONSTER(25),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN(25),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_METIN(10),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_BOSS(5),
	),
	10 : (
		"Point: 250000 - 499999",

		"Bonus:",
		localeinfo.TOOLTIP_MAX_HP(14000),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_MONSTER(25),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN(25),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_METIN(15),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_BOSS(10),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_MEDI_PVM(5),
	),
	11 : (
		"Rangpont: 500000 - 749999",

		"Bonus:",
		localeinfo.TOOLTIP_MAX_HP(16000),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_MONSTER(30),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN(30),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_METIN(20),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_BOSS(15),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_MEDI_PVM(10),

	),
	12 : (
		"Point: 750000 - 999999",

		"Bonus:",
		localeinfo.TOOLTIP_MAX_HP(18000),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_MONSTER(35),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN(35),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_METIN(25),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_BOSS(20),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_MEDI_PVM(15),
	),
	13 : (
		"Point: 1000000 - 1499999",

		"Bonus:",
		localeinfo.TOOLTIP_MAX_HP(20000),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_MONSTER(40),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN(40),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_METIN(30),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_BOSS(25),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_MEDI_PVM(20),
		localeinfo.TOOLTIP_NORMAL_HIT_DAMAGE_BONUS(5),
		localeinfo.TOOLTIP_SKILL_DAMAGE_BONUS(5),
	),
	14 : (
		"Point: 1500000 - 2499999",

		"Bonus:",

		localeinfo.TOOLTIP_MAX_HP(25000),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_MONSTER(50),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN(50),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_METIN(35),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_BOSS(30),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_MEDI_PVM(25),
		localeinfo.TOOLTIP_NORMAL_HIT_DAMAGE_BONUS(10),
		localeinfo.TOOLTIP_SKILL_DAMAGE_BONUS(10),
	),
	15 : (
		"Point: 2500000-2999999",
		"Bonus:",
		localeinfo.TOOLTIP_MAX_HP(30000),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_MONSTER(55),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN(55),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_METIN(40),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_BOSS(35),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_MEDI_PVM(30),
		localeinfo.TOOLTIP_NORMAL_HIT_DAMAGE_BONUS(15),
		localeinfo.TOOLTIP_SKILL_DAMAGE_BONUS(15),
	),
	16 : (
		"Point: 3000000 - 3499999",
		"Bonus:",
		localeinfo.TOOLTIP_MAX_HP(35000),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_MONSTER(60),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN(60),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_METIN(45),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_BOSS(40),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_MEDI_PVM(35),
		localeinfo.TOOLTIP_NORMAL_HIT_DAMAGE_BONUS(20),
		localeinfo.TOOLTIP_SKILL_DAMAGE_BONUS(20),
	),
	17 : (
		"Point: 3500000 - 3999999",
		"Bonus:",
		localeinfo.TOOLTIP_MAX_HP(40000),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_MONSTER(65),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN(65),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_METIN(50),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_BOSS(45),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_MEDI_PVM(40),
		localeinfo.TOOLTIP_NORMAL_HIT_DAMAGE_BONUS(25),
		localeinfo.TOOLTIP_SKILL_DAMAGE_BONUS(25),
	),
	18 : (
		"Point: 4000000 - 4499999",
		"Bonus:",
		localeinfo.TOOLTIP_MAX_HP(45000),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_MONSTER(70),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN(70),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_METIN(55),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_BOSS(50),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_MEDI_PVM(45),
		localeinfo.TOOLTIP_NORMAL_HIT_DAMAGE_BONUS(30),
		localeinfo.TOOLTIP_SKILL_DAMAGE_BONUS(30),
	),
	19 : (
		"Point: 4500000 - 4999999",
		"Bonus:",
		localeinfo.TOOLTIP_MAX_HP(50000),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_MONSTER(75),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN(75),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_METIN(60),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_BOSS(55),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_MEDI_PVM(50),
		localeinfo.TOOLTIP_NORMAL_HIT_DAMAGE_BONUS(35),
		localeinfo.TOOLTIP_SKILL_DAMAGE_BONUS(35),
	),
	20 : (
		"Point: 5000000: MAX",
		"Bonus:",
		localeinfo.TOOLTIP_MAX_HP(60000),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_MONSTER(85),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN(85),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_METIN(70),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_BOSS(65),
		localeinfo.TOOLTIP_APPLY_ATTBONUS_MEDI_PVM(60),
		localeinfo.TOOLTIP_NORMAL_HIT_DAMAGE_BONUS(45),
		localeinfo.TOOLTIP_SKILL_DAMAGE_BONUS(45),
	),
}


if localeinfo.IsYMIR():
	SHOW_LIMIT_SUPPORT_SKILL_LIST = [121, 122, 123, 124, 126, 127, 129, 128, 131, 137, 138, 139, 140,141,142]
	if not localeinfo.IsCHEONMA():
		HIDE_SUPPORT_SKILL_POINT = True
		SHOW_LIMIT_SUPPORT_SKILL_LIST = [121, 122, 123, 124, 126, 127, 129, 128, 131, 137, 138, 139, 140,141,142]
elif localeinfo.IsJAPAN() or (localeinfo.IsEUROPE() and app.GetLocalePath() != "locale/ca") and (localeinfo.IsEUROPE() and app.GetLocalePath() != "locale/br"):
	HIDE_SUPPORT_SKILL_POINT = True
	if not app.ENABLE_NEW_SECONDARY_SKILLS:
		SHOW_LIMIT_SUPPORT_SKILL_LIST = [121, 122, 123, 124, 126, 127, 129, 128, 131, 137, 138, 139, 140]
else:
	HIDE_SUPPORT_SKILL_POINT = True

FACE_IMAGE_DICT = {
	playersettingmodule.RACE_WARRIOR_M	: "icon/face/warrior_m.tga",
	playersettingmodule.RACE_WARRIOR_W	: "icon/face/warrior_w.tga",
	playersettingmodule.RACE_ASSASSIN_M	: "icon/face/assassin_m.tga",
	playersettingmodule.RACE_ASSASSIN_W	: "icon/face/assassin_w.tga",
	playersettingmodule.RACE_SURA_M		: "icon/face/sura_m.tga",
	playersettingmodule.RACE_SURA_W		: "icon/face/sura_w.tga",
	playersettingmodule.RACE_SHAMAN_M	: "icon/face/shaman_m.tga",
	playersettingmodule.RACE_SHAMAN_W	: "icon/face/shaman_w.tga",
}
if app.ENABLE_WOLFMAN_CHARACTER:
	FACE_IMAGE_DICT.update({playersettingmodule.RACE_WOLFMAN_M  : "icon/face/wolfman_m.tga",})

def unsigned32(n):
	return n & 0xFFFFFFFFL



if app.ENABLE_QUEST_RENEWAL:
	quest_slot_listbar = {
		"name" : "Quest_Slot",
		"type" : "listbar",

		"x" : 0,
		"y" : 0,

		"width" : 150,
		"height" : 20,

		"text" : "Quest title",
		"align" : "left",

		"horizontal_align" : "left",
		"vertical_align" : "left",
		"text_horizontal_align" : "left",
		"all_align" : "left",

		"text_height": 40
	}

	quest_lable_expend_img_path_dict = {
		0: "d:/ymir work/ui/quest_re/tabcolor_1_main.tga",
		1: "d:/ymir work/ui/quest_re/tabcolor_2_sub.tga",
		2: "d:/ymir work/ui/quest_re/tabcolor_3_levelup.tga",
		3: "d:/ymir work/ui/quest_re/tabcolor_4_event.tga",
		4: "d:/ymir work/ui/quest_re/tabcolor_5_collection.tga",
		5: "d:/ymir work/ui/quest_re/tabcolor_6_system.tga",
		6: "d:/ymir work/ui/quest_re/tabcolor_7_scroll.tga",
		7: "d:/ymir work/ui/quest_re/tabcolor_8_daily.tga"
	}

	quest_label_dict = {
		0 : uiscriptlocale.QUEST_UI_TEXT_MAIN,
		1 : uiscriptlocale.QUEST_UI_TEXT_SUB,
		2 : uiscriptlocale.QUEST_UI_TEXT_LEVELUP,
		3 : uiscriptlocale.QUEST_UI_TEXT_EVENT,
		4 : uiscriptlocale.QUEST_UI_TEXT_COLLECTION,
		5 : uiscriptlocale.QUEST_UI_TEXT_SYSTEM,
		6 : uiscriptlocale.QUEST_UI_TEXT_SCROLL,
		7 : uiscriptlocale.QUEST_UI_TEXT_DAILY,
	}



class CharacterWindow(ui.ScriptWindow):

	ACTIVE_PAGE_SLOT_COUNT = 8
	SUPPORT_PAGE_SLOT_COUNT = 12

	PAGE_SLOT_COUNT = 12
	PAGE_HORSE = 2

	SKILL_GROUP_NAME_DICT = {
		playersettingmodule.JOB_WARRIOR	: { 1 : localeinfo.SKILL_GROUP_WARRIOR_1,	2 : localeinfo.SKILL_GROUP_WARRIOR_2, },
		playersettingmodule.JOB_ASSASSIN	: { 1 : localeinfo.SKILL_GROUP_ASSASSIN_1,	2 : localeinfo.SKILL_GROUP_ASSASSIN_2, },
		playersettingmodule.JOB_SURA		: { 1 : localeinfo.SKILL_GROUP_SURA_1,		2 : localeinfo.SKILL_GROUP_SURA_2, },
		playersettingmodule.JOB_SHAMAN		: { 1 : localeinfo.SKILL_GROUP_SHAMAN_1,	2 : localeinfo.SKILL_GROUP_SHAMAN_2, },
	}
	if app.ENABLE_WOLFMAN_CHARACTER:
		SKILL_GROUP_NAME_DICT.update({playersettingmodule.JOB_WOLFMAN		: { 1 : localeinfo.JOB_WOLFMAN1,	2 : localeinfo.JOB_WOLFMAN2, },})

	STAT_DESCRIPTION =	{
		"HTH" : localeinfo.STAT_TOOLTIP_CON,
		"INT" : localeinfo.STAT_TOOLTIP_INT,
		"STR" : localeinfo.STAT_TOOLTIP_STR,
		"DEX" : localeinfo.STAT_TOOLTIP_DEX,
	}

	if app.ENABLE_QUEST_RENEWAL:
		MAX_QUEST_PAGE_HEIGHT = 293.5


	STAT_MINUS_DESCRIPTION = localeinfo.STAT_MINUS_DESCRIPTION

	def __init__(self):
		self.chDetailsWnd = None
		self.isOpenedDetailsWnd = False
		
		if app.ENABLE_QUEST_RENEWAL:
			self.isQuestCategoryLoad = False
	

		if app.ENABLE_SKILL_COLOR_SYSTEM:
			self.skillColorWnd = None
			self.skillColorButton = []

		ui.ScriptWindow.__init__(self)
		self.state = "STATUS"
		self.isLoaded = 0

		self.toolTipSkill = 0
		self.__Initialize()
		self.__LoadWindow()

		self.statusPlusCommandDict={
			"HTH" : "/stat ht",
			"INT" : "/stat iq",
			"STR" : "/stat st",
			"DEX" : "/stat dx",
		}
		
		self.status2PlusCommandDict={
			"HTH" : "/stat2 ht",
			"INT" : "/stat2 iq",
			"STR" : "/stat2 st",
			"DEX" : "/stat2 dx",
		}
		
		self.statusMinusCommandDict={
			"HTH-" : "/stat- ht",
			"INT-" : "/stat- iq",
			"STR-" : "/stat- st",
			"DEX-" : "/stat- dx",
		}

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def __Initialize(self):
		self.refreshToolTip = 0
		self.curSelectedSkillGroup = 0
		self.canUseHorseSkill = -1
		self.toolTip = None
		self.toolTipJob = None
		self.toolTipAlignment = None
		self.toolTipSkill = None
		if app.ENABLE_GUILD_ATTRIBUTE:
			self.toolTipGuild = None

		self.faceImage = None
		self.statusPlusValue = None
		self.activeSlot = None
		self.tabDict = None
		self.tabButtonDict = None
		self.pageDict = None
		self.titleBarDict = None
		self.statusPlusButtonDict = None
		self.statusMinusButtonDict = None

		self.skillPageDict = None
		if app.ENABLE_QUEST_RENEWAL:
			self.questScrollBar = None
			self.questLastScrollPosition = 0
			self.questPage = None
			self.questTitleBar = None
			self.questSlotList = []
			self.questCategory = {}
			self.questCategoryList = []

			self.questColorList = {
				"green" : 0xFF83C055,
				"blue": 0xFF45678D,
				"golden": 0xFFCAB62F,
				"default_title": 0xFFCEC6B5
			}

			self.questOpenedCategories = []
			self.questMaxOpenedCategories = 1

			self.questClicked = []
			self.questIndexMap = {}
			self.questCounterList = []
			self.questClockList = []
			self.questSeparatorList = []

			self.displayY = 0
			self.baseCutY = 0
			self.questCategoryRenderPos = []
		else:
			self.questShowingStartIndex = 0
			self.questScrollBar = None
			self.questSlot = None
			self.questNameList = None
			self.questLastTimeList = None
			self.questLastCountList = None
		self.skillGroupButton = ()

		self.activeSlot = None
		self.activeSkillPointValue = None
		self.supportSkillPointValue = None
		self.skillGroupButton1 = None
		self.skillGroupButton2 = None
		if app.ENABLE_NEW_PASSIVE_SKILLS:
			self.skillGroupButton3 = None
		
		self.activeSkillGroupName = None

		self.guildNameSlot = None
		self.guildNameValue = None
		self.characterNameSlot = None
		self.characterNameValue = None

		self.emotionToolTip = None
		self.soloEmotionSlot = None
		self.specialEmotionSlot = None
		self.dualEmotionSlot = None

		if app.ENABLE_CONQUEROR_LEVEL:
			self.change_base = None
			self.change_base_btn = None
			self.change_conqueror = None
			self.change_conqueror_btn = None
			self.ShowToolTip = False

		if app.ENABLE_SKILL_COLOR_SYSTEM:
			self.skillColorWnd = None
			self.skillColorButton = []

	def OnTop(self):
		#if self.chDetailsWnd:
			#self.chDetailsWnd.SetTop()
		
		if app.ENABLE_SKILL_COLOR_SYSTEM:
			if self.skillColorWnd:
				self.skillColorWnd.SetTop()

	def Hide(self):
		if self.chDetailsWnd:
			self.isOpenedDetailsWnd = self.chDetailsWnd.IsShow()
			self.chDetailsWnd.Close()
		
		if app.ENABLE_SKILL_COLOR_SYSTEM:
			if self.skillColorWnd and self.skillColorWnd.IsShow():
				self.skillColorWnd.Close()
		
		wndMgr.Hide(self.hWnd)

	def Show(self):
		self.__LoadWindow()
		self.__InitCharacterDetailsUIButton()
		if self.chDetailsWnd and self.isOpenedDetailsWnd:
			self.chDetailsWnd.Show()
		
		ui.ScriptWindow.Show(self)
	def __ShowGuildToolTip(self):
		if self.toolTipGuild:
			self.toolTipGuild.AlignHorizonalCenter()
			self.toolTipGuild.ShowToolTip()

	def __HideGuildToolTip(self):
		if self.toolTipGuild:
			self.toolTipGuild.HideToolTip()

	def __LoadScript(self, fileName):
		pyScrLoader = ui.PythonScriptLoader()
		pyScrLoader.LoadScriptFile(self, fileName)

	def __BindObject(self):
		self.toolTip = uitooltip.ToolTip()
		self.toolTipJob = uitooltip.ToolTip()
		self.toolTipAlignment = uitooltip.ToolTip(130)
		if app.ENABLE_GUILD_ATTRIBUTE:
			self.toolTipGuild = uitooltip.ToolTip(130)
			

		self.faceImage = self.GetChild("Face_Image")

		faceSlot=self.GetChild("Face_Slot")
		if 949 == app.GetDefaultCodePage():
			faceSlot.SetStringEvent("MOUSE_OVER_IN", self.__ShowJobToolTip)
			faceSlot.SetStringEvent("MOUSE_OVER_OUT", self.__HideJobToolTip)

		self.statusPlusValue = self.GetChild("Status_Plus_Value")

		self.characterNameSlot = self.GetChild("Character_Name_Slot")
		self.characterNameValue = self.GetChild("Character_Name")
		self.guildNameSlot = self.GetChild("Guild_Name_Slot")
		self.guildNameValue = self.GetChild("Guild_Name")
		self.characterNameSlot.SetStringEvent("MOUSE_OVER_IN", self.__ShowAlignmentToolTip)
		self.characterNameSlot.SetStringEvent("MOUSE_OVER_OUT", self.__HideAlignmentToolTip)
		if app.ENABLE_GUILD_ATTRIBUTE:
			self.guildNameSlot.SetStringEvent("MOUSE_OVER_IN", self.__ShowGuildToolTip)
			self.guildNameSlot.SetStringEvent("MOUSE_OVER_OUT", self.__HideGuildToolTip)

		self.activeSlot = self.GetChild("Skill_Active_Slot")
		self.activeSkillPointValue = self.GetChild("Active_Skill_Point_Value")
		self.supportSkillPointValue = self.GetChild("Support_Skill_Point_Value")
		self.supportSkillPointValue.Hide()
		self.skillGroupButton1 = self.GetChild("Skill_Group_Button_1")
		self.skillGroupButton2 = self.GetChild("Skill_Group_Button_2")
		if app.ENABLE_NEW_PASSIVE_SKILLS:
			self.skillGroupButton3 = self.GetChild("Skill_Group_Button_3")

		self.activeSkillGroupName = self.GetChild("Active_Skill_Group_Name")

		if app.ENABLE_CONQUEROR_LEVEL:
			# self.GetChild("Tab_Button_01").SetShowToolTipEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_TAB_CHARACTER)
			# self.GetChild("Tab_Button_01").SetHideToolTipEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			# self.GetChild("Tab_Button_02").SetShowToolTipEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_TAB_SKILL)
			# self.GetChild("Tab_Button_02").SetHideToolTipEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			# self.GetChild("Tab_Button_03").SetShowToolTipEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_TAB_EMOTICON)
			# self.GetChild("Tab_Button_03").SetHideToolTipEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			# self.GetChild("Tab_Button_04").SetShowToolTipEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_TAB_QUEST)
			# self.GetChild("Tab_Button_04").SetHideToolTipEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			# self.GetChild("Lv_ToolTip").SetShowToolTipEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_BASE_LEVEL)
			# self.GetChild("Lv_ToolTip").SetHideToolTipEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			# self.GetChild("Exp_ToolTip").SetShowToolTipEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_EXP)
			# self.GetChild("Exp_ToolTip").SetHideToolTipEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			# self.GetChild("change_base_button").SetShowToolTipEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_BASE_LEVEL)
			# self.GetChild("change_base_button").SetHideToolTipEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			# self.GetChild("Char_Info_Status_img").SetEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_STAT)
			# self.GetChild("Char_Info_Status_img").SetEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			# self.GetChild("Status_Plus_Label").SetEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_POINT)
			# self.GetChild("Status_Plus_Label").SetEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			# self.GetChild("HTH_IMG").SetEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_IMG_CON)
			# self.GetChild("HTH_IMG").SetEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			# self.GetChild("INT_IMG").SetEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_IMG_INT)
			# self.GetChild("INT_IMG").SetEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			# self.GetChild("STR_IMG").SetEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_IMG_STR)
			# self.GetChild("STR_IMG").SetEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			# self.GetChild("DEX_IMG").SetEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_IMG_DEX)
			# self.GetChild("DEX_IMG").SetEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			# self.GetChild("SUNGMA_STR_IMG").SetEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_SUNGMA_STR)
			# self.GetChild("SUNGMA_STR_IMG").SetEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			# self.GetChild("SUNGMA_HP_IMG").SetEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_SUNGMA_HP)
			# self.GetChild("SUNGMA_HP_IMG").SetEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			# self.GetChild("SUNGMA_MOVE_IMG").SetEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_SUNGMA_MOVE)
			# self.GetChild("SUNGMA_MOVE_IMG").SetEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			# self.GetChild("SUNGMA_IMMUNE_IMG").SetEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_SUNGMA_IMMUNE)
			# self.GetChild("SUNGMA_IMMUNE_IMG").SetEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			# self.GetChild("HEL_IMG").SetEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_HP)
			# self.GetChild("HEL_IMG").SetEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			# self.GetChild("SP_IMG").SetEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_SP)
			# self.GetChild("SP_IMG").SetEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			# self.GetChild("ATT_IMG").SetEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_ATT)
			# self.GetChild("ATT_IMG").SetEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			# self.GetChild("DEF_IMG").SetEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_DEF)
			# self.GetChild("DEF_IMG").SetEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			# self.GetChild("MSPD_IMG").SetEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_MOVE_SPEED)
			# self.GetChild("MSPD_IMG").SetEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			# self.GetChild("ASPD_IMG").SetEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_ATT_SPEED)
			# self.GetChild("ASPD_IMG").SetEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			# self.GetChild("CSPD_IMG").SetEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_CAST_SPEED)
			# self.GetChild("CSPD_IMG").SetEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			# self.GetChild("MATT_IMG").SetEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_MAG_ATT)
			# self.GetChild("MATT_IMG").SetEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			# self.GetChild("MDEF_IMG").SetEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_MAG_DEF)
			# self.GetChild("MDEF_IMG").SetEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			# self.GetChild("ER_IMG").SetEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_DODGE_PER)
			# self.GetChild("ER_IMG").SetEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			# self.GetChild("HTH_Plus").SetShowToolTipEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_CON)
			# self.GetChild("HTH_Plus").SetHideToolTipEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			# self.GetChild("STR_Plus").SetShowToolTipEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_STR)
			# self.GetChild("STR_Plus").SetHideToolTipEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			# self.GetChild("INT_Plus").SetShowToolTipEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_INT)
			# self.GetChild("INT_Plus").SetHideToolTipEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			# self.GetChild("DEX_Plus").SetShowToolTipEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_DEX)
			# self.GetChild("DEX_Plus").SetHideToolTipEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			# self.GetChild("sungma_str_plus").SetShowToolTipEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_SUNGMA_STR)
			# self.GetChild("sungma_str_plus").SetHideToolTipEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			# self.GetChild("sungma_hp_plus").SetShowToolTipEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_SUNGMA_HP)
			# self.GetChild("sungma_hp_plus").SetHideToolTipEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			# self.GetChild("sungma_move_plus").SetShowToolTipEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_SUNGMA_MOVE)
			# self.GetChild("sungma_move_plus").SetHideToolTipEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			# self.GetChild("sungma_immune_plus").SetShowToolTipEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_SUNGMA_IMMUNE)
			# self.GetChild("sungma_immune_plus").SetHideToolTipEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			# self.GetChild("HTH_Minus").SetShowToolTipEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_CON)
			# self.GetChild("HTH_Minus").SetHideToolTipEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			# self.GetChild("STR_Minus").SetShowToolTipEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_STR)
			# self.GetChild("STR_Minus").SetHideToolTipEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			# self.GetChild("INT_Minus").SetShowToolTipEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_INT)
			# self.GetChild("INT_Minus").SetHideToolTipEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			# self.GetChild("DEX_Minus").SetShowToolTipEvent(ui.__mem_func__(self.__OverInImage), "mouse_over_in", localeinfo.STAT_TOOLTIP_DEX)
			# self.GetChild("DEX_Minus").SetHideToolTipEvent(ui.__mem_func__(self.__OverOutImage), "mouse_over_out")
			self.change_base = self.GetChild("base_info")
			self.change_base_btn = self.GetChild("change_base_button")
			self.change_base_btn.SetEvent(ui.__mem_func__(self.SelectChange), 0)
			self.change_conqueror = self.GetChild("sungma_info")
			self.change_conqueror_btn = self.GetChild("change_base_button")
			self.change_conqueror_btn.SetEvent(ui.__mem_func__(self.SelectChange), 1)

		if app.ENABLE_QUEST_RENEWAL:
			self.questScrollBar = self.GetChild("Quest_ScrollBar")
			self.questPage = self.GetChild("Quest_Page")
			self.questTitleBar = self.GetChild("Quest_TitleBar")
			self.quest_page_board_window = self.GetChild("quest_page_board_window")
			self.quest_object_board_window = self.GetChild("quest_object_board_window")




		self.tabDict = {
			"STATUS"	: self.GetChild("Tab_01"),
			"SKILL"		: self.GetChild("Tab_02"),
			"EMOTICON"	: self.GetChild("Tab_03"),
			"QUEST"		: self.GetChild("Tab_04"),
		}

		self.tabButtonDict = {
			"STATUS"	: self.GetChild("Tab_Button_01"),
			"SKILL"		: self.GetChild("Tab_Button_02"),
			"EMOTICON"	: self.GetChild("Tab_Button_03"),
			"QUEST"		: self.GetChild("Tab_Button_04")
		}

		self.pageDict = {
			"STATUS"	: self.GetChild("Character_Page"),
			"SKILL"		: self.GetChild("Skill_Page"),
			"EMOTICON"	: self.GetChild("Emoticon_Page"),
			"QUEST"		: self.GetChild("Quest_Page")
		}

		self.titleBarDict = {
			"STATUS"	: self.GetChild("Character_TitleBar"),
			"SKILL"		: self.GetChild("Skill_TitleBar"),
			"EMOTICON"	: self.GetChild("Emoticon_TitleBar"),
			"QUEST"		: self.GetChild("Quest_TitleBar")
		}

		self.statusPlusButtonDict = {
			"HTH"		: self.GetChild("HTH_Plus"),
			"INT"		: self.GetChild("INT_Plus"),
			"STR"		: self.GetChild("STR_Plus"),
			"DEX"		: self.GetChild("DEX_Plus"),
		}

		self.statusMinusButtonDict = {
			"HTH-"		: self.GetChild("HTH_Minus"),
			"INT-"		: self.GetChild("INT_Minus"),
			"STR-"		: self.GetChild("STR_Minus"),
			"DEX-"		: self.GetChild("DEX_Minus"),
		}

		self.skillPageDict = {
			"ACTIVE" : self.GetChild("Skill_Active_Slot"),
			"SUPPORT" : self.GetChild("Skill_ETC_Slot"),
			"HORSE" : self.GetChild("Skill_Active_Slot"),
		}

		self.skillPageStatDict = {
			"SUPPORT"	: player.SKILL_SUPPORT,
			"ACTIVE"	: player.SKILL_ACTIVE,
			"HORSE"		: player.SKILL_HORSE,
		}

		self.skillGroupButton = (
			self.GetChild("Skill_Group_Button_1"),
			self.GetChild("Skill_Group_Button_2"),
		)

		self.soloEmotionSlot = self.GetChild("SoloEmotionSlot")
		self.specialEmotionSlot = self.GetChild("SpecialEmotionSlot")
		self.dualEmotionSlot = self.GetChild("DualEmotionSlot")
		self.__SetEmotionSlot()


		if app.ENABLE_QUEST_RENEWAL:
			self.questScrollBar.SetParent(self.quest_page_board_window)
			for i in xrange(quest.QUEST_CATEGORY_MAX_NUM):
				self.questCategory = ui.SubTitleBar()
				self.questCategory.SetParent(self.questPage)
				self.questCategory.MakeSubTitleBar(210, "red")
				self.questCategory.SetText(quest_label_dict[i])
				self.questCategory.SetSize(210, 16)
				self.questCategory.SetPosition(13, 0)
				self.questCategoryList.append(self.questCategory)

				# self.questCategoryList.append(self.questCategory[i])
				# self.questCategoryList.append(self.GetChild("Quest_Category_0" + str(i)))
				self.questCategoryRenderPos.append(0)

			self.questScrollBar.SetParent(self.questPage)
			self.RearrangeQuestCategories(xrange(quest.QUEST_CATEGORY_MAX_NUM))
		else:


			self.questShowingStartIndex = 0
			self.questScrollBar = self.GetChild("Quest_ScrollBar")
			self.questScrollBar.SetScrollEvent(ui.__mem_func__(self.OnQuestScroll))
			self.questSlot = self.GetChild("Quest_Slot")
			for i in xrange(quest.QUEST_MAX_NUM):
				self.questSlot.HideSlotBaseImage(i)
				self.questSlot.SetCoverButton(i,\
												"d:/ymir work/ui/game/quest/slot_button_01.sub",\
												"d:/ymir work/ui/game/quest/slot_button_02.sub",\
												"d:/ymir work/ui/game/quest/slot_button_03.sub",\
												"d:/ymir work/ui/game/quest/slot_button_03.sub", True)
			
			self.questNameList = []
			self.questLastTimeList = []
			self.questLastCountList = []
			for i in xrange(quest.QUEST_MAX_NUM):
				self.questNameList.append(self.GetChild("Quest_Name_0" + str(i)))
				self.questLastTimeList.append(self.GetChild("Quest_LastTime_0" + str(i)))
				self.questLastCountList.append(self.GetChild("Quest_LastCount_0" + str(i)))
			
			
		self.MainBoard = self.GetChild("board")
		self.ExpandBtn = ui.MakeButton(self.MainBoard, 240, 157, "", "d:/ymir work/ui/game/belt_inventory/", "btn_minimize_normal.tga", "btn_minimize_over.tga", "btn_minimize_down.tga")
		self.ExpandBtn.SetEvent(ui.__mem_func__(self.__ClickExpandButton))
		self.MinimizeBtn = ui.MakeButton(self.MainBoard, 240, 157, "", "d:/ymir work/ui/game/belt_inventory/", "btn_expand_normal.tga", "btn_expand_over.tga", "btn_expand_down.tga")
		self.MinimizeBtn.SetEvent(ui.__mem_func__(self.__ClickMinimizeButton))

		if app.ENABLE_CONQUEROR_LEVEL:
			self.SelectChange(0)

	def __InitCharacterDetailsUIButton(self):
		self.ExpandBtn.Show()
		self.MinimizeBtn.Hide()

	def __ClickExpandButton(self):
		#print "__ClickExpandButton"	
		x, y = self.GetGlobalPosition()
		if not self.chDetailsWnd:
			self.chDetailsWnd = uicharacterdetails.CharacterDetailsUI(self)
			self.chDetailsWnd.Show()
		else:
			self.chDetailsWnd.Show()
		
		self.ExpandBtn.Hide()
		self.MinimizeBtn.Show()
		if app.ENABLE_SKILL_COLOR_SYSTEM:
			if self.skillColorWnd and self.skillColorWnd.IsShow():
				self.skillColorWnd.Hide()

	def __ClickMinimizeButton(self):
		self.chDetailsWnd.Hide()
		self.MinimizeBtn.Hide()
		self.ExpandBtn.Show()

	def AdjustPosition(self, x, y, width):
		self.SetPosition(x - width, y)

	def OnMoveWindow(self, x, y):
		#print "OnMoveWindow x %s y %s" % (x, y)
		if self.chDetailsWnd:
			self.chDetailsWnd.AdjustPosition(x, y)

	def __SetSkillSlotEvent(self):
		for skillPageValue in self.skillPageDict.itervalues():
			skillPageValue.SetSlotStyle(wndMgr.SLOT_STYLE_NONE)
			skillPageValue.SetSelectItemSlotEvent(ui.__mem_func__(self.SelectSkill))
			skillPageValue.SetSelectEmptySlotEvent(ui.__mem_func__(self.SelectEmptySlot))
			skillPageValue.SetUnselectItemSlotEvent(ui.__mem_func__(self.ClickSkillSlot))
			skillPageValue.SetUseSlotEvent(ui.__mem_func__(self.ClickSkillSlot))
			skillPageValue.SetOverInItemEvent(ui.__mem_func__(self.OverInItem))
			skillPageValue.SetOverOutItemEvent(ui.__mem_func__(self.OverOutItem))
			skillPageValue.SetPressedSlotButtonEvent(ui.__mem_func__(self.OnPressedSlotButton))
			skillPageValue.AppendSlotButton("d:/ymir work/ui/game/windows/btn_plus_up.sub",\
											"d:/ymir work/ui/game/windows/btn_plus_over.sub",\
											"d:/ymir work/ui/game/windows/btn_plus_down.sub")

	def __SetEmotionSlot(self):
		self.emotionToolTip = uitooltip.ToolTip()
		for slot in (self.soloEmotionSlot, self.dualEmotionSlot, self.specialEmotionSlot):
			slot.SetSlotStyle(wndMgr.SLOT_STYLE_NONE)
			slot.SetSelectItemSlotEvent(ui.__mem_func__(self.__SelectEmotion))
			slot.SetUnselectItemSlotEvent(ui.__mem_func__(self.__ClickEmotionSlot))
			slot.SetUseSlotEvent(ui.__mem_func__(self.__ClickEmotionSlot))
			slot.SetOverInItemEvent(ui.__mem_func__(self.__OverInEmotion))
			slot.SetOverOutItemEvent(ui.__mem_func__(self.__OverOutEmotion))
			slot.AppendSlotButton("d:/ymir work/ui/game/windows/btn_plus_up.sub", "d:/ymir work/ui/game/windows/btn_plus_over.sub", "d:/ymir work/ui/game/windows/btn_plus_down.sub")

		for slotIdx, datadict in emotion.EMOTION_DICT.items():
			emotionIdx = slotIdx

			slot = self.soloEmotionSlot
			if slotIdx > 50:
				slot = self.dualEmotionSlot
			elif slotIdx >= 19:
				slot = self.specialEmotionSlot
			
			slot.SetEmotionSlot(slotIdx, emotionIdx)
			slot.SetCoverButton(slotIdx)

	def __SelectEmotion(self, slotIndex):
		if not slotIndex in emotion.EMOTION_DICT:
			return

		if app.IsPressed(app.DIK_LCONTROL):
			player.RequestAddToEmptyLocalQuickSlot(player.SLOT_TYPE_EMOTION, slotIndex)
			return

		mousemodule.mouseController.AttachObject(self, player.SLOT_TYPE_EMOTION, slotIndex, slotIndex)

	def __ClickEmotionSlot(self, slotIndex):
		print "click emotion"
		if not slotIndex in emotion.EMOTION_DICT:
			return

		print "check acting"
		if player.IsActingEmotion():
			return

		command = emotion.EMOTION_DICT[slotIndex]["command"]
		print "command", command

		if slotIndex > 50:
			vid = player.GetTargetVID()

			if 0 == vid or vid == player.GetMainCharacterIndex() or chr.IsNPC(vid) or chr.IsEnemy(vid):
				import chat
				chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.EMOTION_CHOOSE_ONE)
				return

			command += " " + chr.GetNameByVID(vid)

		print "send_command", command
		net.SendChatPacket(command)

	def ActEmotion(self, emotionIndex):
		self.__ClickEmotionSlot(emotionIndex)

	def __OverInEmotion(self, slotIndex):
		if self.toolTip == None:
			self.Close()
			
		if self.emotionToolTip:

			if not slotIndex in emotion.EMOTION_DICT:
				return

			self.emotionToolTip.ClearToolTip()
			self.emotionToolTip.SetTitle(emotion.EMOTION_DICT[slotIndex]["name"])
			self.emotionToolTip.AlignHorizonalCenter()
			self.emotionToolTip.ShowToolTip()

	def __OverOutEmotion(self):
		if self.emotionToolTip:
			self.emotionToolTip.HideToolTip()

	def __BindEvent(self):
		for i in xrange(len(self.skillGroupButton)):
			self.skillGroupButton[i].SetEvent(lambda arg=i: self.__SelectSkillGroup(arg))
		
		if app.ENABLE_NEW_PASSIVE_SKILLS:
			self.skillGroupButton3.SetEvent(lambda arg=i: self.__SelectSkillGroup(3))

		self.RefreshQuest()
		self.__HideJobToolTip()

		for (tabKey, tabButton) in self.tabButtonDict.items():
			tabButton.SetEvent(ui.__mem_func__(self.__OnClickTabButton), tabKey)

		for (statusPlusKey, statusPlusButton) in self.statusPlusButtonDict.items():
			statusPlusButton.SAFE_SetEvent(self.__OnClickStatusPlusButton, statusPlusKey)
			statusPlusButton.ShowToolTip = lambda arg=statusPlusKey: self.__OverInStatButton(arg)
			statusPlusButton.HideToolTip = lambda arg=statusPlusKey: self.__OverOutStatButton()

		for (statusMinusKey, statusMinusButton) in self.statusMinusButtonDict.items():
			statusMinusButton.SAFE_SetEvent(self.__OnClickStatusMinusButton, statusMinusKey)
			statusMinusButton.ShowToolTip = lambda arg=statusMinusKey: self.__OverInStatMinusButton(arg)
			statusMinusButton.HideToolTip = lambda arg=statusMinusKey: self.__OverOutStatMinusButton()

		for titleBarValue in self.titleBarDict.itervalues():
			titleBarValue.SetCloseEvent(ui.__mem_func__(self.Close))

		if app.ENABLE_QUEST_RENEWAL:
			self.questTitleBar.SetCloseEvent(ui.__mem_func__(self.Close))
			self.questScrollBar.SetScrollEvent(ui.__mem_func__(self.__OnScrollQuest))

			for i in xrange(quest.QUEST_CATEGORY_MAX_NUM):
				self.questCategoryList[i].SetEvent(ui.__mem_func__(self.__OnClickQuestCategoryButton), i)
		else:
			self.questSlot.SetSelectItemSlotEvent(ui.__mem_func__(self.__SelectQuest))

	def __LoadWindow(self):
		if self.isLoaded == 1:
			return

		self.isLoaded = 1

		try:
			self.__LoadScript("uiscript/characterwindow.py")
			self.__BindObject()
			self.__BindEvent()
		except:
			import exception
			exception.Abort("CharacterWindow.__LoadWindow")

		#self.tabButtonDict["EMOTICON"].Disable()
		self.SetState("STATUS")

	def Destroy(self):
		self.ClearDictionary()

		self.__Initialize()


	def Close(self):
		if 0 != self.toolTipSkill and self.toolTipSkill != None:
			self.toolTipSkill.HideToolTip()
		if self.chDetailsWnd and self.chDetailsWnd.IsShow():
			self.chDetailsWnd.Hide()

		if app.ENABLE_SKILL_COLOR_SYSTEM:
			if self.skillColorWnd and self.skillColorWnd.IsShow():
				self.skillColorWnd.Hide()

		self.Hide()

	def SetSkillToolTip(self, toolTipSkill):
		self.toolTipSkill = toolTipSkill

	def __OnClickStatusPlusButton(self, statusKey):
		try:
			if app.IsPressed(app.DIK_LCONTROL) or app.IsPressed(app.DIK_RCONTROL):
				statusPlusCommand=self.status2PlusCommandDict[statusKey]
				net.SendChatPacket(statusPlusCommand)
			else:
				statusPlusCommand=self.statusPlusCommandDict[statusKey]
				net.SendChatPacket(statusPlusCommand)
		except KeyError, msg:
			dbg.TraceError("CharacterWindow.__OnClickStatusPlusButton KeyError: %s", msg)

	def __OnClickStatusMinusButton(self, statusKey):
		try:
			statusMinusCommand=self.statusMinusCommandDict[statusKey]
			net.SendChatPacket(statusMinusCommand)
		except KeyError, msg:
			dbg.TraceError("CharacterWindow.__OnClickStatusMinusButton KeyError: %s", msg)


	def __OnClickTabButton(self, stateKey):
		self.SetState(stateKey)

	def SetState(self, stateKey):

		self.state = stateKey


		if app.ENABLE_QUEST_RENEWAL:
			if stateKey != "QUEST":
				self.questPage.Hide()
			else:
				self.__LoadQuestCategory()

		for (tabKey, tabButton) in self.tabButtonDict.items():
			if stateKey!=tabKey:
				tabButton.SetUp()

		for tabValue in self.tabDict.itervalues():
			tabValue.Hide()

		for pageValue in self.pageDict.itervalues():
			pageValue.Hide()

		for titleBarValue in self.titleBarDict.itervalues():
			titleBarValue.Hide()

		self.titleBarDict[stateKey].Show()
		self.tabDict[stateKey].Show()
		self.pageDict[stateKey].Show()


	def GetState(self):
		return self.state

	def __GetTotalAtkText(self):
		minAtk=player.GetStatus(player.ATT_MIN)
		maxAtk=player.GetStatus(player.ATT_MAX)
		atkBonus=player.GetStatus(player.ATT_BONUS)
		attackerBonus=player.GetStatus(player.ATTACKER_BONUS)

		if minAtk==maxAtk:
			return "%d" % (minAtk+atkBonus+attackerBonus)
		else:
			return "%d-%d" % (minAtk+atkBonus+attackerBonus, maxAtk+atkBonus+attackerBonus)

	def __GetTotalMagAtkText(self):
		minMagAtk=player.GetStatus(player.MAG_ATT)+player.GetStatus(player.MIN_MAGIC_WEP)
		maxMagAtk=player.GetStatus(player.MAG_ATT)+player.GetStatus(player.MAX_MAGIC_WEP)

		if minMagAtk==maxMagAtk:
			return "%d" % (minMagAtk)
		else:
			return "%d-%d" % (minMagAtk, maxMagAtk)

	def __GetTotalDefText(self):
		defValue=player.GetStatus(player.DEF_GRADE)
		if constinfo.ADD_DEF_BONUS_ENABLE:
			defValue+=player.GetStatus(player.DEF_BONUS)
		return "%d" % (defValue)

	def RefreshStatus(self):
		if self.isLoaded==0:
			return

		try:
			self.GetChild("HP_Value").SetText(str(player.GetStatus(player.HP)) + '/' + str(player.GetStatus(player.MAX_HP)))
			self.GetChild("SP_Value").SetText(str(player.GetStatus(player.SP)) + '/' + str(player.GetStatus(player.MAX_SP)))

			if app.ENABLE_CONQUEROR_LEVEL and self.change_conqueror_btn.IsDown():
				self.GetChild("Level_Value").SetText("0")
				self.GetChild("Exp_Value").SetText("0")
				self.GetChild("RestExp_Value").SetText("0")
				self.GetChild("sungma_str_value").SetText("0")
				self.GetChild("sungma_hp_value").SetText("0")
				self.GetChild("sungma_move_value").SetText("0")
				self.GetChild("sungma_immune_value").SetText("0")
			else:
				self.GetChild("Level_Value").SetText(str(player.GetStatus(player.LEVEL)))
				self.GetChild("Exp_Value").SetText(str(unsigned32(player.GetEXP())))
				self.GetChild("RestExp_Value").SetText(str(unsigned32(player.GetStatus(player.NEXT_EXP)) - unsigned32(player.GetStatus(player.EXP))))
				self.GetChild("STR_Value").SetText(str(player.GetStatus(player.ST)))
				self.GetChild("DEX_Value").SetText(str(player.GetStatus(player.DX)))
				self.GetChild("HTH_Value").SetText(str(player.GetStatus(player.HT)))
				self.GetChild("INT_Value").SetText(str(player.GetStatus(player.IQ)))

			self.GetChild("ATT_Value").SetText(self.__GetTotalAtkText())
			self.GetChild("DEF_Value").SetText(self.__GetTotalDefText())

			self.GetChild("MATT_Value").SetText(self.__GetTotalMagAtkText())
			#self.GetChild("MATT_Value").SetText(str(player.GetStatus(player.MAG_ATT)))

			self.GetChild("MDEF_Value").SetText(str(player.GetStatus(player.MAG_DEF)))
			self.GetChild("ASPD_Value").SetText(str(player.GetStatus(player.ATT_SPEED)))
			self.GetChild("MSPD_Value").SetText(str(player.GetStatus(player.MOVING_SPEED)))
			self.GetChild("CSPD_Value").SetText(str(player.GetStatus(player.CASTING_SPEED)))
			self.GetChild("ER_Value").SetText(str(player.GetStatus(player.EVADE_RATE)))

		except:
			#import exception
			#exception.Abort("CharacterWindow.RefreshStatus.BindObject")
			## ������ ���� ����
			pass

		self.__RefreshStatusPlusButtonList()
		self.__RefreshStatusMinusButtonList()
		self.RefreshAlignment()
		if app.ENABLE_GUILD_ATTRIBUTE:
			self.RefreshGuild()
			
		if self.refreshToolTip:
			self.refreshToolTip()
		
		if self.chDetailsWnd and self.chDetailsWnd.IsShow():
			self.chDetailsWnd.RefreshLabel()

	def __RefreshStatusPlusButtonList(self):
		if self.isLoaded==0:
			return

		statusPlusPoint=player.GetStatus(player.STAT)

		if statusPlusPoint>0:
			self.statusPlusValue.SetText(uiscriptlocale.UICHAR_STATUS_VALUE % (statusPlusPoint))
			self.statusPlusValue.Show()
			self.ShowStatusPlusButtonList()
		else:
			self.statusPlusValue.SetText(uiscriptlocale.UICHAR_STATUS_VALUE % (0))
			self.statusPlusValue.Hide()
			self.HideStatusPlusButtonList()

	def __RefreshStatusMinusButtonList(self):
		if self.isLoaded==0:
			return

		statusMinusPoint=self.__GetStatMinusPoint()

		if statusMinusPoint>0:
			self.__ShowStatusMinusButtonList()
		else:
			self.__HideStatusMinusButtonList()


	def RefreshAlignment(self):
		point, grade = player.GetAlignmentData()

		if grade < 0:
			grade = 0

		maxGrade = len(localeinfo.TITLE_NAME_LIST) - 1
		if grade > maxGrade:
			grade = maxGrade

		import colorinfo
		COLOR_DICT = {
			0	: colorinfo.ALIGNMENT_COLOR_1,
			1	: colorinfo.ALIGNMENT_COLOR_2,
			2	: colorinfo.ALIGNMENT_COLOR_3,
			3	: colorinfo.ALIGNMENT_COLOR_4,
			4	: colorinfo.ALIGNMENT_COLOR_5,
			5	: colorinfo.ALIGNMENT_COLOR_6,
			6	: colorinfo.ALIGNMENT_COLOR_7,
			7	: colorinfo.ALIGNMENT_COLOR_8,
			8	: colorinfo.ALIGNMENT_COLOR_9,
			9	: colorinfo.ALIGNMENT_COLOR_10,
			10	: colorinfo.ALIGNMENT_COLOR_11,
			11	: colorinfo.ALIGNMENT_COLOR_12,
			12	: colorinfo.ALIGNMENT_COLOR_13,
			13	: colorinfo.ALIGNMENT_COLOR_14,
			14	: colorinfo.ALIGNMENT_COLOR_15,
			15	: colorinfo.ALIGNMENT_COLOR_16,
			16	: colorinfo.ALIGNMENT_COLOR_17,
			17	: colorinfo.ALIGNMENT_COLOR_18,
			18	: colorinfo.ALIGNMENT_COLOR_19,
			19	: colorinfo.ALIGNMENT_COLOR_20,
			20	: colorinfo.ALIGNMENT_COLOR_21,
		}

		gradeColor = ui.GenerateColor(*COLOR_DICT.get(grade, colorinfo.ALIGNMENT_COLOR_1))

		
		goldColor = ui.GenerateColor(255, 215, 0)

		self.toolTipAlignment.ClearToolTip()

		# Rang neve színe
		self.toolTipAlignment.AutoAppendTextLine(localeinfo.TITLE_NAME_LIST[grade], gradeColor)

		# Rangpont színe
		self.toolTipAlignment.AutoAppendTextLine(localeinfo.ALIGNMENT_NAME + str(point), gradeColor)

		# Ryuga buzin megfogalmazott bonuszai
		if grade in ALIGNMENT_BONUS_LIST:
			for line in ALIGNMENT_BONUS_LIST[grade]:
				self.toolTipAlignment.AutoAppendTextLine(line, goldColor)

		self.toolTipAlignment.AlignHorizonalCenter()

	if app.ENABLE_GUILD_ATTRIBUTE:
		def RefreshGuild(self):
			GUILD_NAME = 0xffFFFFFF
			POSITIVE_COLOR = 0xffFFFFFF
			SPECIAL_POSITIVE_COLOR = ui.GenerateColor(255, 215, 0)
			
			guild_level = guild.GetGuildLevel()
			
			self.toolTipGuild.ClearToolTip()
			self.toolTipGuild.AutoAppendTextLine(uiscriptlocale.GUILD_INFO_NAME + ": " + guild.GetGuildName(), GUILD_NAME) ##céh neve
			self.toolTipGuild.AutoAppendTextLine(uiscriptlocale.GUILD_INFO_LEVEL + ": " + str(guild.GetGuildLevel()), POSITIVE_COLOR) ## locale_interface.txt
			self.toolTipGuild.AutoAppendTextLine(uiscriptlocale.GUILD_INFO_LEADER + ": " + guild.GetGuildMasterName(), GUILD_NAME) ## céh vezére
			self.toolTipGuild.AppendSpace(5)



			if guild_level == 1:
				bonus_name0 = localeinfo.TOOLTIP_MAX_HP_S
				bonus_value0 = 500
				bonus_name1 = localeinfo.TOOLTIP_ATT_GRADE_S
				bonus_value1 = 15
				self.toolTipGuild.AutoAppendTextLine("%s: +%d" % (bonus_name0, bonus_value0), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d" % (bonus_name1, bonus_value1), SPECIAL_POSITIVE_COLOR)

			elif guild_level == 2:
				bonus_name0 = localeinfo.TOOLTIP_MAX_HP_S
				bonus_value0 = 1000
				bonus_name1 = localeinfo.TOOLTIP_ATT_GRADE_S
				bonus_value1 = 30
				self.toolTipGuild.AutoAppendTextLine("%s: +%d" % (bonus_name0, bonus_value0), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d" % (bonus_name1, bonus_value1), SPECIAL_POSITIVE_COLOR)
				
			elif guild_level == 3:
				bonus_name0 = localeinfo.TOOLTIP_MAX_HP_S
				bonus_value0 = 2000
				bonus_name1 = localeinfo.TOOLTIP_ATT_GRADE_S
				bonus_value1 = 50
				self.toolTipGuild.AutoAppendTextLine("%s: +%d" % (bonus_name0, bonus_value0), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d" % (bonus_name1, bonus_value1), SPECIAL_POSITIVE_COLOR)
				
			elif guild_level == 4:
				bonus_name0 = localeinfo.TOOLTIP_MAX_HP_S
				bonus_value0 = 3000
				bonus_name1 = localeinfo.TOOLTIP_ATT_GRADE_S
				bonus_value1 = 75
				bonus_name2 = localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN_S
				bonus_value2 = 5
				bonus_name3 = localeinfo.TOOLTIP_SKILL_DAMAGE_BONUS_S
				bonus_value3 = 1
				self.toolTipGuild.AutoAppendTextLine("%s: +%d" % (bonus_name0, bonus_value0), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d" % (bonus_name1, bonus_value1), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name2, bonus_value2), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name3, bonus_value3), SPECIAL_POSITIVE_COLOR)
				
			elif guild_level == 5:
				bonus_name0 = localeinfo.TOOLTIP_MAX_HP_S
				bonus_value0 = 4000
				bonus_name1 = localeinfo.TOOLTIP_ATT_GRADE_S
				bonus_value1 = 100
				bonus_name2 = localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN_S
				bonus_value2 = 5
				bonus_name3 = localeinfo.TOOLTIP_SKILL_DAMAGE_BONUS_S
				bonus_value3 = 2
				bonus_name4 = localeinfo.TOOLTIP_NORMAL_HIT_DAMAGE_BONUS_S
				bonus_value4 = 3
				self.toolTipGuild.AutoAppendTextLine("%s: +%d" % (bonus_name0, bonus_value0), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d" % (bonus_name1, bonus_value1), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name2, bonus_value2), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name3, bonus_value3), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name4, bonus_value4), SPECIAL_POSITIVE_COLOR)
				
			elif guild_level == 6:
				bonus_name0 = localeinfo.TOOLTIP_MAX_HP_S
				bonus_value0 = 4000
				bonus_name1 = localeinfo.TOOLTIP_ATT_GRADE_S
				bonus_value1 = 100
				bonus_name2 = localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN_S
				bonus_value2 = 10
				bonus_name3 = localeinfo.TOOLTIP_SKILL_DAMAGE_BONUS_S
				bonus_value3 = 3
				bonus_name4 = localeinfo.TOOLTIP_NORMAL_HIT_DAMAGE_BONUS_S
				bonus_value4 = 3
				bonus_name5 = localeinfo.TOOLTIP_APPLY_ATTBONUS_METIN_RAZOR93
				bonus_value5 = 5
				bonus_name6 = localeinfo.TOOLTIP_APPLY_GOLD_DOUBLE_BONUS_S
				bonus_value6 = 10
				bonus_name7 = localeinfo.TOOLTIP_APPLY_ITEM_DROP_BONUS_S
				bonus_value7 = 10
				self.toolTipGuild.AutoAppendTextLine("%s: +%d" % (bonus_name0, bonus_value0), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d" % (bonus_name1, bonus_value1), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name2, bonus_value2), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name3, bonus_value3), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name4, bonus_value4), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name5, bonus_value5), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name6, bonus_value6), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name7, bonus_value7), SPECIAL_POSITIVE_COLOR)
                
			elif guild_level == 7:
				bonus_name0 = localeinfo.TOOLTIP_MAX_HP_S
				bonus_value0 = 4000
				bonus_name1 = localeinfo.TOOLTIP_ATT_GRADE_S
				bonus_value1 = 100
				bonus_name2 = localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN_S
				bonus_value2 = 10
				bonus_name3 = localeinfo.TOOLTIP_SKILL_DAMAGE_BONUS_S
				bonus_value3 = 3
				bonus_name4 = localeinfo.TOOLTIP_NORMAL_HIT_DAMAGE_BONUS_S
				bonus_value4 = 3
				bonus_name5 = localeinfo.TOOLTIP_APPLY_ATTBONUS_METIN_RAZOR93
				bonus_value5 = 5
				bonus_name6 = localeinfo.TOOLTIP_APPLY_GOLD_DOUBLE_BONUS_S
				bonus_value6 = 10
				bonus_name7 = localeinfo.TOOLTIP_APPLY_ITEM_DROP_BONUS_S
				bonus_value7 = 10
				bonus_name8 = localeinfo.TOOLTIP_APPLY_ATTBONUS_BOSS_RAZOR93
				bonus_value8 = 5
				bonus_name9 = localeinfo.TOOLTIP_APPLY_ATTBONUS_MONSTER_S
				bonus_value9 = 5
				self.toolTipGuild.AutoAppendTextLine("%s: +%d" % (bonus_name0, bonus_value0), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d" % (bonus_name1, bonus_value1), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name2, bonus_value2), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name3, bonus_value3), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name4, bonus_value4), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name5, bonus_value5), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name6, bonus_value6), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name7, bonus_value7), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name8, bonus_value8), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name9, bonus_value9), SPECIAL_POSITIVE_COLOR)

			elif guild_level == 8:
				bonus_name0 = localeinfo.TOOLTIP_MAX_HP_S
				bonus_value0 = 5000
				bonus_name1 = localeinfo.TOOLTIP_ATT_GRADE_S
				bonus_value1 = 100
				bonus_name2 = localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN_S
				bonus_value2 = 10
				bonus_name3 = localeinfo.TOOLTIP_SKILL_DAMAGE_BONUS_S
				bonus_value3 = 5
				bonus_name4 = localeinfo.TOOLTIP_NORMAL_HIT_DAMAGE_BONUS_S
				bonus_value4 = 5
				bonus_name5 = localeinfo.TOOLTIP_APPLY_ATTBONUS_METIN_RAZOR93
				bonus_value5 = 5
				bonus_name6 = localeinfo.TOOLTIP_APPLY_GOLD_DOUBLE_BONUS_S
				bonus_value6 = 10
				bonus_name7 = localeinfo.TOOLTIP_APPLY_ITEM_DROP_BONUS_S
				bonus_value7 = 10
				bonus_name8 = localeinfo.TOOLTIP_APPLY_ATTBONUS_BOSS_RAZOR93
				bonus_value8 = 5
				bonus_name9 = localeinfo.TOOLTIP_APPLY_ATTBONUS_MONSTER_S
				bonus_value9 = 10
				self.toolTipGuild.AutoAppendTextLine("%s: +%d" % (bonus_name0, bonus_value0), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d" % (bonus_name1, bonus_value1), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name2, bonus_value2), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name3, bonus_value3), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name4, bonus_value4), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name5, bonus_value5), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name6, bonus_value6), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name7, bonus_value7), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name8, bonus_value8), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name9, bonus_value9), SPECIAL_POSITIVE_COLOR)

			elif guild_level == 9:
				bonus_name0 = localeinfo.TOOLTIP_MAX_HP_S
				bonus_value0 = 5000
				bonus_name1 = localeinfo.TOOLTIP_ATT_GRADE_S
				bonus_value1 = 100
				bonus_name2 = localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN_S
				bonus_value2 = 10
				bonus_name3 = localeinfo.TOOLTIP_SKILL_DAMAGE_BONUS_S
				bonus_value3 = 5
				bonus_name4 = localeinfo.TOOLTIP_NORMAL_HIT_DAMAGE_BONUS_S
				bonus_value4 = 5
				bonus_name5 = localeinfo.TOOLTIP_APPLY_ATTBONUS_METIN_RAZOR93
				bonus_value5 = 10
				bonus_name6 = localeinfo.TOOLTIP_APPLY_GOLD_DOUBLE_BONUS_S
				bonus_value6 = 15
				bonus_name7 = localeinfo.TOOLTIP_APPLY_ITEM_DROP_BONUS_S
				bonus_value7 = 15
				bonus_name8 = localeinfo.TOOLTIP_APPLY_ATTBONUS_BOSS_RAZOR93
				bonus_value8 = 10
				bonus_name9 = localeinfo.TOOLTIP_APPLY_ATTBONUS_MONSTER_S
				bonus_value9 = 15
				self.toolTipGuild.AutoAppendTextLine("%s: +%d" % (bonus_name0, bonus_value0), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d" % (bonus_name1, bonus_value1), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name2, bonus_value2), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name3, bonus_value3), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name4, bonus_value4), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name5, bonus_value5), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name6, bonus_value6), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name7, bonus_value7), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name8, bonus_value8), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name9, bonus_value9), SPECIAL_POSITIVE_COLOR)

			elif guild_level == 10:
				bonus_name0 = localeinfo.TOOLTIP_MAX_HP_S
				bonus_value0 = 5000
				bonus_name1 = localeinfo.TOOLTIP_ATT_GRADE_S
				bonus_value1 = 100
				bonus_name2 = localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN_S
				bonus_value2 = 15
				bonus_name3 = localeinfo.TOOLTIP_SKILL_DAMAGE_BONUS_S
				bonus_value3 = 5
				bonus_name4 = localeinfo.TOOLTIP_NORMAL_HIT_DAMAGE_BONUS_S
				bonus_value4 = 5
				bonus_name5 = localeinfo.TOOLTIP_APPLY_ATTBONUS_METIN_RAZOR93
				bonus_value5 = 15
				bonus_name6 = localeinfo.TOOLTIP_APPLY_GOLD_DOUBLE_BONUS_S
				bonus_value6 = 15
				bonus_name7 = localeinfo.TOOLTIP_APPLY_ITEM_DROP_BONUS_S
				bonus_value7 = 15
				bonus_name8 = localeinfo.TOOLTIP_APPLY_ATTBONUS_BOSS_RAZOR93
				bonus_value8 = 15
				bonus_name9 = localeinfo.TOOLTIP_APPLY_ATTBONUS_MONSTER_S
				bonus_value9 = 15
				self.toolTipGuild.AutoAppendTextLine("%s: +%d" % (bonus_name0, bonus_value0), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d" % (bonus_name1, bonus_value1), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name2, bonus_value2), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name3, bonus_value3), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name4, bonus_value4), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name5, bonus_value5), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name6, bonus_value6), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name7, bonus_value7), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name8, bonus_value8), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name9, bonus_value9), SPECIAL_POSITIVE_COLOR)

			elif guild_level == 11:
				bonus_name0 = localeinfo.TOOLTIP_MAX_HP_S
				bonus_value0 = 5000
				bonus_name1 = localeinfo.TOOLTIP_ATT_GRADE_S
				bonus_value1 = 125
				bonus_name2 = localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN_S
				bonus_value2 = 20
				bonus_name3 = localeinfo.TOOLTIP_SKILL_DAMAGE_BONUS_S
				bonus_value3 = 5
				bonus_name4 = localeinfo.TOOLTIP_NORMAL_HIT_DAMAGE_BONUS_S
				bonus_value4 = 5
				bonus_name5 = localeinfo.TOOLTIP_APPLY_ATTBONUS_METIN_RAZOR93
				bonus_value5 = 20
				bonus_name6 = localeinfo.TOOLTIP_APPLY_GOLD_DOUBLE_BONUS_S
				bonus_value6 = 20
				bonus_name7 = localeinfo.TOOLTIP_APPLY_ITEM_DROP_BONUS_S
				bonus_value7 = 20
				bonus_name8 = localeinfo.TOOLTIP_APPLY_ATTBONUS_BOSS_RAZOR93
				bonus_value8 = 15
				bonus_name9 = localeinfo.TOOLTIP_APPLY_ATTBONUS_MONSTER_S
				bonus_value9 = 15
				self.toolTipGuild.AutoAppendTextLine("%s: +%d" % (bonus_name0, bonus_value0), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d" % (bonus_name1, bonus_value1), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name2, bonus_value2), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name3, bonus_value3), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name4, bonus_value4), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name5, bonus_value5), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name6, bonus_value6), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name7, bonus_value7), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name8, bonus_value8), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name9, bonus_value9), SPECIAL_POSITIVE_COLOR)
				#self.toolTipGuild.AutoAppendTextLine("%s : %d" % (bonus_name10, bonus_value10), SPECIAL_POSITIVE_COLOR)
				#self.toolTipGuild.AutoAppendTextLine("%s : %d" % (bonus_name11, bonus_value11), SPECIAL_POSITIVE_COLOR)

			elif guild_level == 12:
				bonus_name0 = localeinfo.TOOLTIP_MAX_HP_S
				bonus_value0 = 5000
				bonus_name1 = localeinfo.TOOLTIP_ATT_GRADE_S
				bonus_value1 = 125
				bonus_name2 = localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN_S
				bonus_value2 = 20
				bonus_name3 = localeinfo.TOOLTIP_SKILL_DAMAGE_BONUS_S
				bonus_value3 = 5
				bonus_name4 = localeinfo.TOOLTIP_NORMAL_HIT_DAMAGE_BONUS_S
				bonus_value4 = 5
				bonus_name5 = localeinfo.TOOLTIP_APPLY_ATTBONUS_METIN_RAZOR93
				bonus_value5 = 20
				bonus_name6 = localeinfo.TOOLTIP_APPLY_GOLD_DOUBLE_BONUS_S
				bonus_value6 = 20
				bonus_name7 = localeinfo.TOOLTIP_APPLY_ITEM_DROP_BONUS_S
				bonus_value7 = 20
				bonus_name8 = localeinfo.TOOLTIP_APPLY_ATTBONUS_BOSS_RAZOR93
				bonus_value8 = 15
				bonus_name9 = localeinfo.TOOLTIP_APPLY_ATTBONUS_MONSTER_S
				bonus_value9 = 25
				bonus_name10 = localeinfo.TOOLTIP_MALL_ATTBONUS_GUILD
				bonus_value10 = 5
				self.toolTipGuild.AutoAppendTextLine("%s: +%d" % (bonus_name0, bonus_value0), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d" % (bonus_name1, bonus_value1), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name2, bonus_value2), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name3, bonus_value3), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name4, bonus_value4), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name5, bonus_value5), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name6, bonus_value6), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name7, bonus_value7), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name8, bonus_value8), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name9, bonus_value9), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name10, bonus_value10), SPECIAL_POSITIVE_COLOR)
				#self.toolTipGuild.AutoAppendTextLine("%s : %d" % (bonus_name11, bonus_value11), SPECIAL_POSITIVE_COLOR)

			elif guild_level == 13:
				bonus_name0 = localeinfo.TOOLTIP_MAX_HP_S
				bonus_value0 = 5000
				bonus_name1 = localeinfo.TOOLTIP_ATT_GRADE_S
				bonus_value1 = 150
				bonus_name2 = localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN_S
				bonus_value2 = 25
				bonus_name3 = localeinfo.TOOLTIP_SKILL_DAMAGE_BONUS_S
				bonus_value3 = 5
				bonus_name4 = localeinfo.TOOLTIP_NORMAL_HIT_DAMAGE_BONUS_S
				bonus_value4 = 5
				bonus_name5 = localeinfo.TOOLTIP_APPLY_ATTBONUS_METIN_RAZOR93
				bonus_value5 = 20
				bonus_name6 = localeinfo.TOOLTIP_APPLY_GOLD_DOUBLE_BONUS_S
				bonus_value6 = 20
				bonus_name7 = localeinfo.TOOLTIP_APPLY_ITEM_DROP_BONUS_S
				bonus_value7 = 20
				bonus_name8 = localeinfo.TOOLTIP_APPLY_ATTBONUS_BOSS_RAZOR93
				bonus_value8 = 20
				bonus_name9 = localeinfo.TOOLTIP_APPLY_ATTBONUS_MONSTER_S
				bonus_value9 = 20
				bonus_name10 = localeinfo.TOOLTIP_MALL_ATTBONUS_GUILD
				bonus_value10 = 5
				bonus_name11 = localeinfo.TOOLTIP_APPLY_EXP_DOUBLE_BONUS_S
				bonus_value11 = 20
				self.toolTipGuild.AutoAppendTextLine("%s: +%d" % (bonus_name0, bonus_value0), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d" % (bonus_name1, bonus_value1), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name2, bonus_value2), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name3, bonus_value3), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name4, bonus_value4), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name5, bonus_value5), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name6, bonus_value6), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name7, bonus_value7), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name8, bonus_value8), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name9, bonus_value9), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name10, bonus_value10), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name11, bonus_value11), SPECIAL_POSITIVE_COLOR)
				#self.toolTipGuild.AutoAppendTextLine("%s : %d" % (bonus_name12, bonus_value12), SPECIAL_POSITIVE_COLOR)

			elif guild_level == 14:
				bonus_name0 = localeinfo.TOOLTIP_MAX_HP_S
				bonus_value0 = 6000
				bonus_name1 = localeinfo.TOOLTIP_ATT_GRADE_S
				bonus_value1 = 150
				bonus_name2 = localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN_S
				bonus_value2 = 25
				bonus_name3 = localeinfo.TOOLTIP_SKILL_DAMAGE_BONUS_S
				bonus_value3 = 5
				bonus_name4 = localeinfo.TOOLTIP_NORMAL_HIT_DAMAGE_BONUS_S
				bonus_value4 = 5
				bonus_name5 = localeinfo.TOOLTIP_APPLY_ATTBONUS_METIN_RAZOR93
				bonus_value5 = 20
				bonus_name6 = localeinfo.TOOLTIP_APPLY_GOLD_DOUBLE_BONUS_S
				bonus_value6 = 25
				bonus_name7 = localeinfo.TOOLTIP_APPLY_ITEM_DROP_BONUS_S
				bonus_value7 = 25
				bonus_name8 = localeinfo.TOOLTIP_APPLY_ATTBONUS_BOSS_RAZOR93
				bonus_value8 = 20
				bonus_name9 = localeinfo.TOOLTIP_APPLY_ATTBONUS_MONSTER_S
				bonus_value9 = 20
				bonus_name10 = localeinfo.TOOLTIP_MALL_ATTBONUS_GUILD
				bonus_value10 = 5
				bonus_name11 = localeinfo.TOOLTIP_APPLY_EXP_DOUBLE_BONUS_S
				bonus_value11 = 25
				self.toolTipGuild.AutoAppendTextLine("%s: +%d" % (bonus_name0, bonus_value0), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d" % (bonus_name1, bonus_value1), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name2, bonus_value2), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name3, bonus_value3), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name4, bonus_value4), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name5, bonus_value5), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name6, bonus_value6), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name7, bonus_value7), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name8, bonus_value8), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name9, bonus_value9), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name10, bonus_value10), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name11, bonus_value11), SPECIAL_POSITIVE_COLOR)
				#self.toolTipGuild.AutoAppendTextLine("%s : %d" % (bonus_name12, bonus_value12), SPECIAL_POSITIVE_COLOR)

			elif guild_level == 15:
				bonus_name0 = localeinfo.TOOLTIP_MAX_HP_S
				bonus_value0 = 5000
				bonus_name1 = localeinfo.TOOLTIP_ATT_GRADE_S
				bonus_value1 = 75
				bonus_name2 = localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN_S
				bonus_value2 = 15
				bonus_name3 = localeinfo.TOOLTIP_SKILL_DAMAGE_BONUS_S
				bonus_value3 = 5
				bonus_name4 = localeinfo.TOOLTIP_NORMAL_HIT_DAMAGE_BONUS_S
				bonus_value4 = 10
				bonus_name5 = localeinfo.TOOLTIP_APPLY_ATTBONUS_METIN_RAZOR93
				bonus_value5 = 25
				bonus_name6 = localeinfo.TOOLTIP_APPLY_GOLD_DOUBLE_BONUS_S
				bonus_value6 = 25
				bonus_name7 = localeinfo.TOOLTIP_APPLY_ITEM_DROP_BONUS_S
				bonus_value7 = 25
				bonus_name8 = localeinfo.TOOLTIP_APPLY_ATTBONUS_BOSS_RAZOR93
				bonus_value8 = 25
				bonus_name9 = localeinfo.TOOLTIP_APPLY_ATTBONUS_MONSTER_S
				bonus_value9 = 25
				bonus_name10 = localeinfo.TOOLTIP_MALL_ATTBONUS_GUILD
				bonus_value10 = 5
				bonus_name11 = localeinfo.TOOLTIP_APPLY_EXP_DOUBLE_BONUS_S
				bonus_value11 = 45
				bonus_name12 = localeinfo.TOOLTIP_AGAINST_EVERYONE
				bonus_value12 = 5
				self.toolTipGuild.AutoAppendTextLine("%s: +%d" % (bonus_name0, bonus_value0), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d" % (bonus_name1, bonus_value1), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name2, bonus_value2), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name3, bonus_value3), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name4, bonus_value4), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name5, bonus_value5), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name6, bonus_value6), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name7, bonus_value7), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name8, bonus_value8), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name9, bonus_value9), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name10, bonus_value10), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name11, bonus_value11), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name12, bonus_value12), SPECIAL_POSITIVE_COLOR)



			elif guild_level == 16:
				bonus_name0 = localeinfo.TOOLTIP_MAX_HP_S
				bonus_value0 = 10000
				bonus_name1 = localeinfo.TOOLTIP_ATT_GRADE_S
				bonus_value1 = 175
				bonus_name2 = localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN_S
				bonus_value2 = 30
				bonus_name3 = localeinfo.TOOLTIP_SKILL_DAMAGE_BONUS_S
				bonus_value3 = 7
				bonus_name4 = localeinfo.TOOLTIP_NORMAL_HIT_DAMAGE_BONUS_S
				bonus_value4 = 7
				bonus_name5 = localeinfo.TOOLTIP_APPLY_ATTBONUS_METIN_RAZOR93
				bonus_value5 = 30
				bonus_name6 = localeinfo.TOOLTIP_APPLY_GOLD_DOUBLE_BONUS_S
				bonus_value6 = 30
				bonus_name7 = localeinfo.TOOLTIP_APPLY_ITEM_DROP_BONUS_S
				bonus_value7 = 30
				bonus_name8 = localeinfo.TOOLTIP_APPLY_ATTBONUS_BOSS_RAZOR93
				bonus_value8 = 30
				bonus_name9 = localeinfo.TOOLTIP_APPLY_ATTBONUS_MONSTER_S
				bonus_value9 = 30
				bonus_name10 = localeinfo.TOOLTIP_MALL_ATTBONUS_GUILD
				bonus_value10 = 6
				bonus_name11 = localeinfo.TOOLTIP_APPLY_EXP_DOUBLE_BONUS_S
				bonus_value11 = 45
				bonus_name12 = localeinfo.TOOLTIP_AGAINST_EVERYONE
				bonus_value12 = 6
				self.toolTipGuild.AutoAppendTextLine("%s: +%d" % (bonus_name0, bonus_value0), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d" % (bonus_name1, bonus_value1), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name2, bonus_value2), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name3, bonus_value3), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name4, bonus_value4), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name5, bonus_value5), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name6, bonus_value6), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name7, bonus_value7), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name8, bonus_value8), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name9, bonus_value9), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name10, bonus_value10), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name11, bonus_value11), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name12, bonus_value12), SPECIAL_POSITIVE_COLOR)

			elif guild_level == 17:
				bonus_name0 = localeinfo.TOOLTIP_MAX_HP_S
				bonus_value0 = 12000
				bonus_name1 = localeinfo.TOOLTIP_ATT_GRADE_S
				bonus_value1 = 175
				bonus_name2 = localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN_S
				bonus_value2 = 30
				bonus_name3 = localeinfo.TOOLTIP_SKILL_DAMAGE_BONUS_S
				bonus_value3 = 7
				bonus_name4 = localeinfo.TOOLTIP_NORMAL_HIT_DAMAGE_BONUS_S
				bonus_value4 = 7
				bonus_name5 = localeinfo.TOOLTIP_APPLY_ATTBONUS_METIN_RAZOR93
				bonus_value5 = 30
				bonus_name6 = localeinfo.TOOLTIP_APPLY_GOLD_DOUBLE_BONUS_S
				bonus_value6 = 30
				bonus_name7 = localeinfo.TOOLTIP_APPLY_ITEM_DROP_BONUS_S
				bonus_value7 = 30
				bonus_name8 = localeinfo.TOOLTIP_APPLY_ATTBONUS_BOSS_RAZOR93
				bonus_value8 = 30
				bonus_name9 = localeinfo.TOOLTIP_APPLY_ATTBONUS_MONSTER_S
				bonus_value9 = 30
				bonus_name10 = localeinfo.TOOLTIP_MALL_ATTBONUS_GUILD
				bonus_value10 = 7
				bonus_name11 = localeinfo.TOOLTIP_APPLY_EXP_DOUBLE_BONUS_S
				bonus_value11 = 45
				bonus_name12 = localeinfo.TOOLTIP_AGAINST_EVERYONE
				bonus_value12 = 7
				self.toolTipGuild.AutoAppendTextLine("%s: +%d" % (bonus_name0, bonus_value0), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d" % (bonus_name1, bonus_value1), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name2, bonus_value2), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name3, bonus_value3), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name4, bonus_value4), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name5, bonus_value5), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name6, bonus_value6), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name7, bonus_value7), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name8, bonus_value8), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name9, bonus_value9), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name10, bonus_value10), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name11, bonus_value11), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name12, bonus_value12), SPECIAL_POSITIVE_COLOR)

			elif guild_level == 18:
				bonus_name0 = localeinfo.TOOLTIP_MAX_HP_S
				bonus_value0 = 13000
				bonus_name1 = localeinfo.TOOLTIP_ATT_GRADE_S
				bonus_value1 = 175
				bonus_name2 = localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN_S
				bonus_value2 = 35
				bonus_name3 = localeinfo.TOOLTIP_SKILL_DAMAGE_BONUS_S
				bonus_value3 = 8
				bonus_name4 = localeinfo.TOOLTIP_NORMAL_HIT_DAMAGE_BONUS_S
				bonus_value4 = 8
				bonus_name5 = localeinfo.TOOLTIP_APPLY_ATTBONUS_METIN_RAZOR93
				bonus_value5 = 35
				bonus_name6 = localeinfo.TOOLTIP_APPLY_GOLD_DOUBLE_BONUS_S
				bonus_value6 = 30
				bonus_name7 = localeinfo.TOOLTIP_APPLY_ITEM_DROP_BONUS_S
				bonus_value7 = 30
				bonus_name8 = localeinfo.TOOLTIP_APPLY_ATTBONUS_BOSS_RAZOR93
				bonus_value8 = 30
				bonus_name9 = localeinfo.TOOLTIP_APPLY_ATTBONUS_MONSTER_S
				bonus_value9 = 35
				bonus_name10 = localeinfo.TOOLTIP_MALL_ATTBONUS_GUILD
				bonus_value10 = 8
				bonus_name11 = localeinfo.TOOLTIP_APPLY_EXP_DOUBLE_BONUS_S
				bonus_value11 = 45
				bonus_name12 = localeinfo.TOOLTIP_AGAINST_EVERYONE
				bonus_value12 = 8
				self.toolTipGuild.AutoAppendTextLine("%s: +%d" % (bonus_name0, bonus_value0), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d" % (bonus_name1, bonus_value1), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name2, bonus_value2), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name3, bonus_value3), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name4, bonus_value4), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name5, bonus_value5), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name6, bonus_value6), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name7, bonus_value7), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name8, bonus_value8), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name9, bonus_value9), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name10, bonus_value10), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name11, bonus_value11), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name12, bonus_value12), SPECIAL_POSITIVE_COLOR)

			elif guild_level == 19:
				bonus_name0 = localeinfo.TOOLTIP_MAX_HP_S
				bonus_value0 = 14000
				bonus_name1 = localeinfo.TOOLTIP_ATT_GRADE_S
				bonus_value1 = 175
				bonus_name2 = localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN_S
				bonus_value2 = 35
				bonus_name3 = localeinfo.TOOLTIP_SKILL_DAMAGE_BONUS_S
				bonus_value3 = 9
				bonus_name4 = localeinfo.TOOLTIP_NORMAL_HIT_DAMAGE_BONUS_S
				bonus_value4 = 9
				bonus_name5 = localeinfo.TOOLTIP_APPLY_ATTBONUS_METIN_RAZOR93
				bonus_value5 = 35
				bonus_name6 = localeinfo.TOOLTIP_APPLY_GOLD_DOUBLE_BONUS_S
				bonus_value6 = 30
				bonus_name7 = localeinfo.TOOLTIP_APPLY_ITEM_DROP_BONUS_S
				bonus_value7 = 30
				bonus_name8 = localeinfo.TOOLTIP_APPLY_ATTBONUS_BOSS_RAZOR93
				bonus_value8 = 30
				bonus_name9 = localeinfo.TOOLTIP_APPLY_ATTBONUS_MONSTER_S
				bonus_value9 = 35
				bonus_name10 = localeinfo.TOOLTIP_MALL_ATTBONUS_GUILD
				bonus_value10 = 9
				bonus_name11 = localeinfo.TOOLTIP_APPLY_EXP_DOUBLE_BONUS_S
				bonus_value11 = 45
				bonus_name12 = localeinfo.TOOLTIP_AGAINST_EVERYONE
				bonus_value12 = 9
				self.toolTipGuild.AutoAppendTextLine("%s: +%d" % (bonus_name0, bonus_value0), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d" % (bonus_name1, bonus_value1), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name2, bonus_value2), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name3, bonus_value3), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name4, bonus_value4), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name5, bonus_value5), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name6, bonus_value6), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name7, bonus_value7), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name8, bonus_value8), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name9, bonus_value9), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name10, bonus_value10), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name11, bonus_value11), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (bonus_name12, bonus_value12), SPECIAL_POSITIVE_COLOR)
			
			# ... itt vannak a regi if/elif guild_level == 1..20 blokkjaid ...

			elif guild_level >= 20:
				# cap 40-en
				show_level = guild_level
				if show_level > 40:
					show_level = 40

				step = show_level - 20  # 0..20

				# +10 osszesen 20 szintre, de lvl21-en mar +1 legyen
				# mintazat: +1 szintek: 21,23,25,27,29,31,33,35,37,40
				add10 = (step + 1) // 2
				if step >= 19:
					add10 = 9
				if step == 20:
					add10 = 10

				# base lvl20
				base_hp = 15000
				base_att = 200
				base_human = 40
				base_skill = 10
				base_normal = 10
				base_metin = 40
				base_gold = 30
				base_drop = 30
				base_boss = 40
				base_monster = 40
				base_mall = 10
				base_exp = 50
				base_resist = 10

				# linearisak
				hp = base_hp + 500 * step# 15000 -> 25000
				att = base_att + 10 * step# 200   -> 400

				# +10 cel (lvl40-re)
				human = base_human + add10# 40 -> 50
				skill = base_skill + add10# 10 -> 20
				normal = base_normal + add10# 10 -> 20
				mall = base_mall + add10# 10 -> 20
				exp = base_exp + add10# 50 -> 60
				resist = base_resist + add10# 10 -> 20  (tooltipben 1 sor)

				# +20 cel (lvl40-re) => +1/szint
				metin = base_metin + step# 40 -> 60
				gold = base_gold + step# 30 -> 50
				drop = base_drop + step# 30 -> 50
				boss = base_boss + step# 40 -> 60
				monster = base_monster + step# 40 -> 60

				self.toolTipGuild.AutoAppendTextLine("%s: +%d"   % (localeinfo.TOOLTIP_MAX_HP_S, hp), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d"   % (localeinfo.TOOLTIP_ATT_GRADE_S, att), SPECIAL_POSITIVE_COLOR)

				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN_S, human), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (localeinfo.TOOLTIP_SKILL_DAMAGE_BONUS_S, skill), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (localeinfo.TOOLTIP_NORMAL_HIT_DAMAGE_BONUS_S, normal), SPECIAL_POSITIVE_COLOR)

				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (localeinfo.TOOLTIP_APPLY_ATTBONUS_METIN_RAZOR93, metin), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (localeinfo.TOOLTIP_APPLY_GOLD_DOUBLE_BONUS_S, gold), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (localeinfo.TOOLTIP_APPLY_ITEM_DROP_BONUS_S, drop), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (localeinfo.TOOLTIP_APPLY_ATTBONUS_BOSS_RAZOR93, boss), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (localeinfo.TOOLTIP_APPLY_ATTBONUS_MONSTER_S, monster), SPECIAL_POSITIVE_COLOR)

				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (localeinfo.TOOLTIP_MALL_ATTBONUS_GUILD, mall), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (localeinfo.TOOLTIP_APPLY_EXP_DOUBLE_BONUS_S, exp), SPECIAL_POSITIVE_COLOR)
				self.toolTipGuild.AutoAppendTextLine("%s: +%d%%" % (localeinfo.TOOLTIP_AGAINST_EVERYONE, resist), SPECIAL_POSITIVE_COLOR)
			self.toolTipGuild.AlignHorizonalCenter()




	def __ShowStatusMinusButtonList(self):
		for (stateMinusKey, statusMinusButton) in self.statusMinusButtonDict.items():
			statusMinusButton.Show()

	def __HideStatusMinusButtonList(self):
		for (stateMinusKey, statusMinusButton) in self.statusMinusButtonDict.items():
			statusMinusButton.Hide()

	def ShowStatusPlusButtonList(self):
		for (statePlusKey, statusPlusButton) in self.statusPlusButtonDict.items():
			statusPlusButton.Show()

	def HideStatusPlusButtonList(self):
		for (statePlusKey, statusPlusButton) in self.statusPlusButtonDict.items():
			statusPlusButton.Hide()

	def SelectSkill(self, skillSlotIndex):

		if app.ENABLE_NEW_PASSIVE_SKILLS and self.skillGroupButton3.IsDown() and skillSlotIndex < 100:
			return

		mouseController = mousemodule.mouseController

		if False == mouseController.isAttached():

			srcSlotIndex = self.__RealSkillSlotToSourceSlot(skillSlotIndex)
			selectedSkillIndex = player.GetSkillIndex(srcSlotIndex)

			if skill.CanUseSkill(selectedSkillIndex):

				if app.IsPressed(app.DIK_LCONTROL):

					player.RequestAddToEmptyLocalQuickSlot(player.SLOT_TYPE_SKILL, srcSlotIndex)
					return

				mouseController.AttachObject(self, player.SLOT_TYPE_SKILL, srcSlotIndex, selectedSkillIndex)

		else:

			mouseController.DeattachObject()

	def SelectEmptySlot(self, SlotIndex):
		mousemodule.mouseController.DeattachObject()

	## ToolTip
	def OverInItem(self, slotNumber):

		if mousemodule.mouseController.isAttached():
			return

		if 0 == self.toolTipSkill or self.toolTipSkill == None:
			return

		srcSlotIndex = self.__RealSkillSlotToSourceSlot(slotNumber)
		if app.ENABLE_NEW_PASSIVE_SKILLS and self.skillGroupButton3.IsDown() and slotNumber < 100:
			srcSlotIndex = self.__RealSkillSlotToSourceSlot(slotNumber + 220)
			if srcSlotIndex-220 >= 40:
				srcSlotIndex -= 40
			elif srcSlotIndex-220 >= 20:
				srcSlotIndex -= 20
		
		skillIndex = player.GetSkillIndex(srcSlotIndex)
		skillLevel = player.GetSkillLevel(srcSlotIndex)
		skillGrade = player.GetSkillGrade(srcSlotIndex)
		skillType = skill.GetSkillType(skillIndex)

		## ACTIVE
		if skill.SKILL_TYPE_ACTIVE == skillType:
			overInSkillGrade = self.__GetSkillGradeFromSlot(slotNumber)
			if overInSkillGrade == skill.SKILL_GRADE_COUNT-1 and skillGrade == skill.SKILL_GRADE_COUNT:
				self.toolTipSkill.SetSkillNew(srcSlotIndex, skillIndex, skillGrade, skillLevel)
			elif overInSkillGrade == skillGrade:
				self.toolTipSkill.SetSkillNew(srcSlotIndex, skillIndex, overInSkillGrade, skillLevel)
			else:
				self.toolTipSkill.SetSkillOnlyName(srcSlotIndex, skillIndex, overInSkillGrade)

		else:
			self.toolTipSkill.SetSkillNew(srcSlotIndex, skillIndex, skillGrade, skillLevel)

	def OverOutItem(self):
		if self.toolTipSkill == None:
			self.Close()
		if 0 != self.toolTipSkill and self.toolTipSkill != None :
			self.toolTipSkill.HideToolTip()

	## Quest
	def __SelectQuest(self, slotIndex):
		if self.toolTip == None:
			self.Close()
		
		
		if app.ENABLE_QUEST_RENEWAL:
			questIndex = self.questIndexMap[slotIndex]

			if not questIndex in self.questClicked:
				self.questClicked.append(questIndex)
		else:
			questIndex = quest.GetQuestIndex(self.questShowingStartIndex + slotIndex)


		import event
		event.QuestButtonClick(-2147483648 + questIndex)

	def RefreshQuest(self):
		if self.toolTip == None:
			self.Close()

		if app.ENABLE_QUEST_RENEWAL:
			if self.isLoaded == 0 or self.state != "QUEST":
				return

			for cat in self.questOpenedCategories:
				self.RefreshQuestCategory(cat)

			self.RefreshQuestCategoriesCount()
		else:
			if self.isLoaded==0:
				return

		questCount = quest.GetQuestCount()
		questRange = range(quest.QUEST_MAX_NUM)

		if questCount > quest.QUEST_MAX_NUM:
			self.questScrollBar.Show()
		else:
			self.questScrollBar.Hide()

		for i in questRange[:questCount]:
			(questName, questIcon, questCounterName, questCounterValue) = quest.GetQuestData(self.questShowingStartIndex+i)
			self.questNameList[i].SetText(questName)
			self.questNameList[i].Show()
			self.questLastCountList[i].Show()
			self.questLastTimeList[i].Show()

			if len(questCounterName) > 0:
				self.questLastCountList[i].SetText("%s : %d" % (questCounterName, questCounterValue))
			else:
				self.questLastCountList[i].SetText("")

			## Icon
			self.questSlot.SetSlot(i, i, 1, 1, questIcon)

		for i in questRange[questCount:]:
			self.questNameList[i].Hide()
			self.questLastTimeList[i].Hide()
			self.questLastCountList[i].Hide()
			self.questSlot.ClearSlot(i)
			self.questSlot.HideSlotBaseImage(i)

		self.__UpdateQuestClock()

	def __UpdateQuestClock(self):
		if self.toolTip == None:
			self.Close()
		if "QUEST" == self.state:
			if app.ENABLE_QUEST_RENEWAL:
				for clock in self.questClockList:
					clockText = localeinfo.QUEST_UNLIMITED_TIME

					if clock.GetProperty("idx"):
						(lastName, lastTime) = quest.GetQuestLastTime(clock.GetProperty("idx"))

						if len(lastName) > 0:
							if lastTime <= 0:
								clockText = localeinfo.QUEST_TIMEOVER
							else:
								questLastMinute = lastTime / 60
								questLastSecond = lastTime % 60

								clockText = lastName + " : "

								if questLastMinute > 0:
									clockText += str(questLastMinute) + localeinfo.QUEST_MIN
									if questLastSecond > 0:
										clockText += " "

								if questLastSecond > 0:
									clockText += str(questLastSecond) + localeinfo.QUEST_SEC

					clock.SetText(clockText)
			else:
				# QUEST_LIMIT_COUNT_BUG_FIX
				for i in xrange(min(quest.GetQuestCount(), quest.QUEST_MAX_NUM)):
				# END_OF_QUEST_LIMIT_COUNT_BUG_FIX
					(lastName, lastTime) = quest.GetQuestLastTime(i)

					clockText = localeinfo.QUEST_UNLIMITED_TIME
					if len(lastName) > 0:

						if lastTime <= 0:
							clockText = localeinfo.QUEST_TIMEOVER

						else:
							questLastMinute = lastTime / 60
							questLastSecond = lastTime % 60

							clockText = lastName + " : "

							if questLastMinute > 0:
								clockText += str(questLastMinute) + localeinfo.QUEST_MIN
								if questLastSecond > 0:
									clockText += " "

							if questLastSecond > 0:
								clockText += str(questLastSecond) + localeinfo.QUEST_SEC

					self.questLastTimeList[i].SetText(clockText)


	def __GetStatMinusPoint(self):
		POINT_STAT_RESET_COUNT = 112
		return player.GetStatus(POINT_STAT_RESET_COUNT)

	def __OverInStatMinusButton(self, stat):
		try:
			self.__ShowStatToolTip(self.STAT_MINUS_DESCRIPTION[stat] % self.__GetStatMinusPoint())
		except KeyError:
			pass

		self.refreshToolTip = lambda arg=stat: self.__OverInStatMinusButton(arg)

	def __OverOutStatMinusButton(self):
		self.__HideStatToolTip()
		self.refreshToolTip = 0

	def __OverInStatButton(self, stat):
		try:
			self.__ShowStatToolTip(self.STAT_DESCRIPTION[stat])
		except KeyError:
			pass

	def __OverOutStatButton(self):
		self.__HideStatToolTip()

	def __ShowStatToolTip(self, statDesc):
		if self.toolTip == None:
			self.Close()
		if self.toolTip != None :
			self.toolTip.ClearToolTip()
			self.toolTip.AppendTextLine(localeinfo.INFO_INCREASE_CHAR_STAT)
			self.toolTip.AppendTextLine(statDesc)
			self.toolTip.Show()

	def __HideStatToolTip(self):
		if self.toolTip == None:
			self.Close()
		if self.toolTip != None :
			self.toolTip.Hide()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	if app.ENABLE_CONQUEROR_LEVEL:
		def SelectChange(self, arg):
			if arg == 0:
				self.change_base_btn.Down()
				self.change_conqueror_btn.SetUp()
				self.change_base.Show()
				self.change_conqueror.Hide()
			else:
				self.change_base_btn.Down()
				self.change_conqueror_btn.SetUp()
				self.change_base.Show()
				self.change_conqueror.Hide()
			
			self.RefreshStatus()

		# def __OverInImage(self, act, arg):
			# arglen = len(str(arg))
			# pos_x, pos_y = wndMgr.GetMousePosition()
			
			# self.toolTip.ClearToolTip()
			# self.toolTip.SetThinBoardSize(11 * arglen)
			# self.toolTip.SetToolTipPosition(pos_x + 50, pos_y + 50)
			# import grp
			# self.toolTip.AppendTextLine(arg, self.toolTip.FONT_COLOR)
			# self.toolTip.Show()
			# self.ShowToolTip = True

		# def __OverOutImage(self, act):
			# self.toolTip.Hide()
			# self.ShowToolTip = False

	def OnUpdate(self):
		self.__UpdateQuestClock()

		if app.ENABLE_SKILL_COLOR_SYSTEM:
			if self.skillColorWnd:
				self.__UpdateSkillColorPosition()

	## Skill Process
	def __RefreshSkillPage(self, name, slotCount):
		global SHOW_LIMIT_SUPPORT_SKILL_LIST
		
		if app.ENABLE_NEW_PASSIVE_SKILLS and self.skillGroupButton3.IsDown():
			skillPage = self.skillPageDict["ACTIVE"]
			getSkillMaxLevel = skill.GetSkillMaxLevel
			
			for i in xrange(self.ACTIVE_PAGE_SLOT_COUNT+1):
				slotIndex = i + skillPage.GetStartIndex() + 220
				skillIndex = player.GetSkillIndex(slotIndex)
				for j in xrange(skill.SKILL_GRADE_COUNT):
					skillPage.ClearSlot(self.__GetRealSkillSlot(j, i))
				
				if skillIndex == 0:
					continue
				
				skillGrade = player.GetSkillGrade(slotIndex)
				skillLevel = player.GetSkillLevel(slotIndex)
				skillType = skill.GetSkillType(skillIndex)
				for j in xrange(skill.SKILL_GRADE_COUNT):
					realSlotIndex = self.__GetRealSkillSlot(j, slotIndex - 220)
					
					skillPage.SetSkillSlotNew(realSlotIndex, skillIndex, j, skillLevel)
					skillPage.SetCoverButton(realSlotIndex)
					
					if (skillGrade == skill.SKILL_GRADE_COUNT) and j == (skill.SKILL_GRADE_COUNT-1):
						skillPage.SetSlotCountNew(realSlotIndex, skillGrade, skillLevel)
					elif (not self.__CanUseSkillNow()) or (skillGrade != j):
						skillPage.SetSlotCount(realSlotIndex, 0)
						skillPage.DisableCoverButton(realSlotIndex)
					else:
						skillPage.SetSlotCountNew(realSlotIndex, skillGrade, skillLevel)
				
				skillPage.RefreshSlot()
			
			for i in xrange(12):
				skillPage = self.skillPageDict["SUPPORT"]
				slotIndex = i + 101
				skillPage.ClearSlot(slotIndex)
				skillIndex = player.GetSkillIndex(slotIndex)
				skillGrade = player.GetSkillGrade(slotIndex)
				skillLevel = player.GetSkillLevel(slotIndex)
				if skillIndex == 0:
					continue
				
				if player.SKILL_INDEX_RIDING == skillIndex:
					if 1 == skillGrade:
						skillLevel += 19
					elif 2 == skillGrade:
						skillLevel += 29
					elif 3 == skillGrade:
						skillLevel = 40

					skillPage.SetSkillSlotNew(slotIndex, skillIndex, max(skillLevel-1, 0), skillLevel)
					skillPage.SetSlotCount(slotIndex, skillLevel)
				else:
					realSlotIndex = self.__GetETCSkillRealSlotIndex(slotIndex)
					skillPage.SetSkillSlot(realSlotIndex, skillIndex, skillLevel)
					skillPage.SetSlotCountNew(realSlotIndex, skillGrade, skillLevel)
					
					if skill.CanUseSkill(skillIndex):
						skillPage.SetCoverButton(realSlotIndex)
				
				skillPage.RefreshSlot()
			
			return
	
		skillPage = self.skillPageDict[name]

		startSlotIndex = skillPage.GetStartIndex()
		if "ACTIVE" == name:
			if self.PAGE_HORSE == self.curSelectedSkillGroup:
				startSlotIndex += slotCount

		getSkillType=skill.GetSkillType
		getSkillIndex=player.GetSkillIndex
		getSkillGrade=player.GetSkillGrade
		getSkillLevel=player.GetSkillLevel
		getSkillLevelUpPoint=skill.GetSkillLevelUpPoint
		getSkillMaxLevel=skill.GetSkillMaxLevel
		for i in xrange(slotCount+1):
			slotIndex = i + startSlotIndex
			skillIndex = getSkillIndex(slotIndex)

			for j in xrange(skill.SKILL_GRADE_COUNT):
				skillPage.ClearSlot(self.__GetRealSkillSlot(j, i))

			if 0 == skillIndex:
				continue

			skillGrade = getSkillGrade(slotIndex)
			skillLevel = getSkillLevel(slotIndex)
			skillType = getSkillType(skillIndex)

			## ���� ���� ���� ����
			if player.SKILL_INDEX_RIDING == skillIndex:
				if 1 == skillGrade:
					skillLevel += 19
				elif 2 == skillGrade:
					skillLevel += 29
				elif 3 == skillGrade:
					skillLevel = 40

				skillPage.SetSkillSlotNew(slotIndex, skillIndex, max(skillLevel-1, 0), skillLevel)
				skillPage.SetSlotCount(slotIndex, skillLevel)

			## ACTIVE
			elif skill.SKILL_TYPE_ACTIVE == skillType:
				for j in xrange(skill.SKILL_GRADE_COUNT):
					realSlotIndex = self.__GetRealSkillSlot(j, slotIndex)
					skillPage.SetSkillSlotNew(realSlotIndex, skillIndex, j, skillLevel)
					skillPage.SetCoverButton(realSlotIndex)

					if (skillGrade == skill.SKILL_GRADE_COUNT) and j == (skill.SKILL_GRADE_COUNT-1):
						skillPage.SetSlotCountNew(realSlotIndex, skillGrade, skillLevel)
					elif (not self.__CanUseSkillNow()) or (skillGrade != j):
						skillPage.SetSlotCount(realSlotIndex, 0)
						skillPage.DisableCoverButton(realSlotIndex)
					else:
						skillPage.SetSlotCountNew(realSlotIndex, skillGrade, skillLevel)

			## ����
			else:
				if not SHOW_LIMIT_SUPPORT_SKILL_LIST or skillIndex in SHOW_LIMIT_SUPPORT_SKILL_LIST:
					realSlotIndex = self.__GetETCSkillRealSlotIndex(slotIndex)
					skillPage.SetSkillSlot(realSlotIndex, skillIndex, skillLevel)
					skillPage.SetSlotCountNew(realSlotIndex, skillGrade, skillLevel)
					if skill.CanUseSkill(skillIndex):
						skillPage.SetCoverButton(realSlotIndex)

			skillPage.RefreshSlot()

		if app.ENABLE_SKILL_COLOR_SYSTEM:
			if "ACTIVE" == name:
				if self.PAGE_HORSE != self.curSelectedSkillGroup:
					self.__CreateSkillColorButton(skillPage)
				else:
					self.skillColorButton = []

	def RefreshSkill(self):

		if self.isLoaded==0:
			return

		if self.__IsChangedHorseRidingSkillLevel():
			self.RefreshCharacter()
			return


		global SHOW_ONLY_ACTIVE_SKILL
		if SHOW_ONLY_ACTIVE_SKILL:
			self.__RefreshSkillPage("ACTIVE", self.ACTIVE_PAGE_SLOT_COUNT)
		else:
			self.__RefreshSkillPage("ACTIVE", self.ACTIVE_PAGE_SLOT_COUNT)
			self.__RefreshSkillPage("SUPPORT", self.SUPPORT_PAGE_SLOT_COUNT)

		self.RefreshSkillPlusButtonList()

	def CanShowPlusButton(self, skillIndex, skillLevel, curStatPoint):

		if app.ENABLE_NEW_PASSIVE_SKILLS and self.skillGroupButton3.IsDown():
			return False

		## ������ ������
		if 0 == skillIndex:
			return False

		## ������ ������ ����������
		if not skill.CanLevelUpSkill(skillIndex, skillLevel):
			return False

		return True

	def __RefreshSkillPlusButton(self, name):
		global HIDE_SUPPORT_SKILL_POINT
		if HIDE_SUPPORT_SKILL_POINT and "SUPPORT" == name:
			return

		slotWindow = self.skillPageDict[name]
		slotWindow.HideAllSlotButton()

		slotStatType = self.skillPageStatDict[name]
		if 0 == slotStatType:
			return

		statPoint = player.GetStatus(slotStatType)
		startSlotIndex = slotWindow.GetStartIndex()
		if "HORSE" == name:
			startSlotIndex += self.ACTIVE_PAGE_SLOT_COUNT

		if statPoint > 0:
			for i in xrange(self.PAGE_SLOT_COUNT):
				slotIndex = i + startSlotIndex
				skillIndex = player.GetSkillIndex(slotIndex)
				skillGrade = player.GetSkillGrade(slotIndex)
				skillLevel = player.GetSkillLevel(slotIndex)

				if skillIndex == 0:
					continue
				if skillGrade != 0:
					continue

				if name == "HORSE":
					if player.GetStatus(player.LEVEL) >= skill.GetSkillLevelLimit(skillIndex):
						if skillLevel < 20:
							slotWindow.ShowSlotButton(self.__GetETCSkillRealSlotIndex(slotIndex))

				else:
					if "SUPPORT" == name:
						if not SHOW_LIMIT_SUPPORT_SKILL_LIST or skillIndex in SHOW_LIMIT_SUPPORT_SKILL_LIST:
							if self.CanShowPlusButton(skillIndex, skillLevel, statPoint):
								slotWindow.ShowSlotButton(slotIndex)
					else:
						if self.CanShowPlusButton(skillIndex, skillLevel, statPoint):
							slotWindow.ShowSlotButton(slotIndex)

	if app.ENABLE_SKILL_COLOR_SYSTEM:
		def __IsColorableSkill(self, skillIndex):
			if not constinfo.SKILL_COLOR_BUFF_ONLY:
				return True

			WARRIOR_SG = [
				[3, 4], # WARRIOR SG1
				[19] # WARRIOR SG2
			]
			NINJA_SG = [
				[34], # NINJA SG1
				[49] # NINJA SG2
			]
			SURA_SG = [
				[63, 64, 65], # SURA SG1
				[78, 79] # SURA SG2
			]
			SHAMAN_SG = [
				[94, 95, 96], # SHAMAN SG1
				[109, 110, 111] # SHAMAN SG1
			]

			if skillIndex in WARRIOR_SG[0] or skillIndex in WARRIOR_SG[1] or\
				skillIndex in NINJA_SG[0] or skillIndex in NINJA_SG[1] or\
				skillIndex in SURA_SG[0] or skillIndex in SURA_SG[1] or\
				skillIndex in SHAMAN_SG[0] or skillIndex in SHAMAN_SG[1]:
				return True
			return False

		def __CreateSkillColorButton(self, parent):
			self.skillColorButton = []

			xPos, yPos = 0, 0
			for idx in xrange(self.PAGE_SLOT_COUNT):
				skillSlot = idx
				if skillSlot < 6:
					if (skillSlot % 2) == 0:
						xPos = 75
						yPos = 4 + (skillSlot / 2 * 36)
					else:
						xPos = 187
						yPos = 4 + (skillSlot / 2 * 36)

					skillIndex = player.GetSkillIndex(skillSlot + 1)
					skillMaxGrade = 3

					if len(self.skillColorButton) == skillSlot:
						self.skillColorButton.append([])
						self.skillColorButton[skillSlot] = ui.Button()
						self.skillColorButton[skillSlot].SetParent(parent)
						self.skillColorButton[skillSlot].SetUpVisual("d:/ymir work/ui/skillcolor/skill_color_button_default.tga")
						self.skillColorButton[skillSlot].SetOverVisual("d:/ymir work/ui/skillcolor/skill_color_button_over.tga")
						self.skillColorButton[skillSlot].SetDownVisual("d:/ymir work/ui/skillcolor/skill_color_button_down.tga")
						self.skillColorButton[skillSlot].SetPosition(xPos, yPos)
						self.skillColorButton[skillSlot].SetEvent(lambda arg = skillSlot, arg2 = skillIndex: self.__OnPressSkillColorButton(arg, arg2))
						if player.GetSkillGrade(skillSlot + 1) >= skillMaxGrade and self.__IsColorableSkill(skillIndex):
							self.skillColorButton[skillSlot].Show()
						else:
							self.skillColorButton[skillSlot].Hide()
					else:
						self.skillColorButton[skillSlot].SetPosition(xPos, yPos)

		def __UpdateSkillColorPosition(self):
			x,y = self.GetGlobalPosition()
			self.skillColorWnd.SetPosition(x+250,y)

		def __OnPressSkillColorButton(self, skillSlot, skillIndex):
			self.skillColorWnd = uiskillcolor.SkillColorWindow(skillSlot, skillIndex)
			if self.skillColorWnd and not self.skillColorWnd.IsShow():
				self.skillColorWnd.Show()
				self.skillColorWnd.UpdateUseAvailable()
				
				if self.chDetailsWnd and self.chDetailsWnd.IsShow():
					self.chDetailsWnd.Hide()
					self.__InitCharacterDetailsUIButton()

	def RefreshItemSlot(self):
		if app.ENABLE_SKILL_COLOR_SYSTEM:
			if self.skillColorWnd and self.skillColorWnd.IsShow():
				self.skillColorWnd.UpdateUseAvailable()

	def RefreshSkillPlusButtonList(self):

		if self.isLoaded==0:
			return

		self.RefreshSkillPlusPointLabel()

		if not self.__CanUseSkillNow():
			return

		try:
			if self.PAGE_HORSE == self.curSelectedSkillGroup:
				self.__RefreshSkillPlusButton("HORSE")
			else:
				self.__RefreshSkillPlusButton("ACTIVE")

			self.__RefreshSkillPlusButton("SUPPORT")

		except:
			import exception
			exception.Abort("CharacterWindow.RefreshSkillPlusButtonList.BindObject")

	def RefreshSkillPlusPointLabel(self):
		if self.isLoaded==0:
			return

		if self.PAGE_HORSE == self.curSelectedSkillGroup:
			activeStatPoint = player.GetStatus(player.SKILL_HORSE)
			self.activeSkillPointValue.SetText(uiscriptlocale.UICHAR_STATUS_VALUE % (activeStatPoint))

		else:
			activeStatPoint = player.GetStatus(player.SKILL_ACTIVE)
			self.activeSkillPointValue.SetText(uiscriptlocale.UICHAR_STATUS_VALUE % (activeStatPoint))

		supportStatPoint = max(0, player.GetStatus(player.SKILL_SUPPORT))
		self.supportSkillPointValue.SetText(uiscriptlocale.UICHAR_STATUS_VALUE % (supportStatPoint))

	## Skill Level Up Button
	def OnPressedSlotButton(self, slotNumber):
		srcSlotIndex = self.__RealSkillSlotToSourceSlot(slotNumber)

		skillIndex = player.GetSkillIndex(srcSlotIndex)
		curLevel = player.GetSkillLevel(srcSlotIndex)
		maxLevel = skill.GetSkillMaxLevel(skillIndex)

		net.SendChatPacket("/skillup " + str(skillIndex))

	## Use Skill
	def ClickSkillSlot(self, slotIndex):
		if app.ENABLE_NEW_PASSIVE_SKILLS and self.skillGroupButton3.IsDown() and slotIndex < 100:
			return

		srcSlotIndex = self.__RealSkillSlotToSourceSlot(slotIndex)
		skillIndex = player.GetSkillIndex(srcSlotIndex)
		skillType = skill.GetSkillType(skillIndex)
		if not self.__CanUseSkillNow():
			if skill.SKILL_TYPE_ACTIVE == skillType:
				return

		for slotWindow in self.skillPageDict.values():
			if slotWindow.HasSlot(slotIndex):
				if skill.CanUseSkill(skillIndex):
					player.ClickSkillSlot(srcSlotIndex)
					return

		mousemodule.mouseController.DeattachObject()

	## FIXME : ������ ���������� ���� ������ ������ ���� ������ ������ �������� ����.
	##         ���� ������. ���� ������ �������� ����.
	def OnUseSkill(self, slotIndex, coolTime):

		skillIndex = player.GetSkillIndex(slotIndex)
		skillType = skill.GetSkillType(skillIndex)

		## ACTIVE
		if skill.SKILL_TYPE_ACTIVE == skillType:
			skillGrade = player.GetSkillGrade(slotIndex)
			slotIndex = self.__GetRealSkillSlot(skillGrade, slotIndex)
		## ETC
		else:
			slotIndex = self.__GetETCSkillRealSlotIndex(slotIndex)

		for slotWindow in self.skillPageDict.values():
			if slotWindow.HasSlot(slotIndex):
				slotWindow.SetSlotCoolTime(slotIndex, coolTime)
				return

	def OnActivateSkill(self, slotIndex):

		skillGrade = player.GetSkillGrade(slotIndex)
		slotIndex = self.__GetRealSkillSlot(skillGrade, slotIndex)

		for slotWindow in self.skillPageDict.values():
			if slotWindow.HasSlot(slotIndex):
				slotWindow.ActivateSlot(slotIndex)
				return

	def OnDeactivateSkill(self, slotIndex):

		skillGrade = player.GetSkillGrade(slotIndex)
		slotIndex = self.__GetRealSkillSlot(skillGrade, slotIndex)

		for slotWindow in self.skillPageDict.values():
			if slotWindow.HasSlot(slotIndex):
				slotWindow.DeactivateSlot(slotIndex)
				return

	def __ShowJobToolTip(self):
		self.toolTipJob.ShowToolTip()

	def __HideJobToolTip(self):
		self.toolTipJob.HideToolTip()

	def __SetJobText(self, mainJob, subJob):
		if player.GetStatus(player.LEVEL)<5:
			subJob=0

		if 949 == app.GetDefaultCodePage():
			self.toolTipJob.ClearToolTip()

			try:
				jobInfoTitle=localeinfo.JOBINFO_TITLE[mainJob][subJob]
				jobInfoData=localeinfo.JOBINFO_DATA_LIST[mainJob][subJob]
			except IndexError:
				print "uicharacter.CharacterWindow.__SetJobText(mainJob=%d, subJob=%d)" % (mainJob, subJob)
				return

			self.toolTipJob.AutoAppendTextLine(jobInfoTitle)
			self.toolTipJob.AppendSpace(5)

			for jobInfoDataLine in jobInfoData:
				self.toolTipJob.AutoAppendTextLine(jobInfoDataLine)

			self.toolTipJob.AlignHorizonalCenter()

	def __ShowAlignmentToolTip(self):
		if self.toolTipAlignment == None:
			self.Close()
		if self.toolTipAlignment != None :
			self.toolTipAlignment.ShowToolTip()

	def __HideAlignmentToolTip(self):
		if self.toolTipAlignment == None:
			self.Close()
		if self.toolTipAlignment != None :
			self.toolTipAlignment.HideToolTip()

	def RefreshCharacter(self):

		if self.isLoaded==0:
			return

		## Name
		try:
			characterName = player.GetName()
			guildName = player.GetGuildName()
			self.characterNameValue.SetText(characterName)
			self.guildNameValue.SetText(guildName)
			if not guildName:
				if localeinfo.IsARABIC():
					self.characterNameSlot.SetPosition(190, 34)
				else:
					self.characterNameSlot.SetPosition(109, 34)

				self.guildNameSlot.Hide()
			else:
				if localeinfo.IsJAPAN():
					self.characterNameSlot.SetPosition(143, 34)
				else:
					self.characterNameSlot.SetPosition(153, 34)
				self.guildNameSlot.Show()
		except:
			import exception
			exception.Abort("CharacterWindow.RefreshCharacter.BindObject")

		race = net.GetMainActorRace()
		group = net.GetMainActorSkillGroup()
		empire = net.GetMainActorEmpire()

		## Job Text
		job = chr.RaceToJob(race)
		self.__SetJobText(job, group)

		## FaceImage
		try:
			faceImageName = FACE_IMAGE_DICT[race]

			try:
				self.faceImage.LoadImage(faceImageName)
			except:
				print "CharacterWindow.RefreshCharacter(race=%d, faceImageName=%s)" % (race, faceImageName)
				self.faceImage.Hide()

		except KeyError:
			self.faceImage.Hide()

		## GroupName
		self.__SetSkillGroupName(race, group)

		## Skill
		if 0 == group:
			self.__SelectSkillGroup(0)

		else:
			self.__SetSkillSlotData(race, group, empire)

			if self.__CanUseHorseSkill():
				self.__SelectSkillGroup(0)

	def __SetSkillGroupName(self, race, group):
		if app.ENABLE_NEW_PASSIVE_SKILLS:
			if net.GetMainActorSkillGroup() == 0:
				self.skillGroupButton3.SetPosition(95, 2)
			elif self.__CanUseHorseSkill():
				self.skillGroupButton3.SetPosition(50, 2)
				self.skillGroupButton2.SetPosition(95, 2)
		
		job = chr.RaceToJob(race)

		if not self.SKILL_GROUP_NAME_DICT.has_key(job):
			return

		nameList = self.SKILL_GROUP_NAME_DICT[job]

		if 0 == group:
			self.skillGroupButton1.SetText(nameList[1])
			self.skillGroupButton2.SetText(nameList[2])
			self.skillGroupButton1.Show()
			self.skillGroupButton2.Show()
			self.activeSkillGroupName.Hide()

			## �������� ������ ���������� ������ ����
			if app.ENABLE_WOLFMAN_CHARACTER and playersettingmodule.RACE_WOLFMAN_M == race:
				self.skillGroupButton2.Hide()
		else:

			if self.__CanUseHorseSkill():
				self.activeSkillGroupName.Hide()
				self.skillGroupButton1.SetText(nameList.get(group, "Noname"))
				self.skillGroupButton2.SetText(localeinfo.SKILL_GROUP_HORSE)
				self.skillGroupButton1.Show()
				self.skillGroupButton2.Show()

			else:
				self.activeSkillGroupName.SetText(nameList.get(group, "Noname"))
				self.activeSkillGroupName.Show()
				if app.ENABLE_NEW_PASSIVE_SKILLS:
					if not self.skillGroupButton3.IsDown() and not self.skillGroupButton2.IsDown():
						self.skillGroupButton1.Down()
					
					self.skillGroupButton1.Show()
					self.skillGroupButton1.SetText("")
				else:
					self.skillGroupButton1.Hide()
				
				self.skillGroupButton2.Hide()

	def __SetSkillSlotData(self, race, group, empire=0):
		## SkillIndex
		playersettingmodule.RegisterSkill(race, group, empire)

		## Event
		self.__SetSkillSlotEvent()

		## Refresh
		self.RefreshSkill()

	def __SelectSkillGroup(self, index):
		if app.ENABLE_NEW_PASSIVE_SKILLS:
			if index == 3:
				self.skillGroupButton3.Down()
				if app.ENABLE_SKILL_COLOR_SYSTEM:
					self.skillColorButton = []
				
				skillPage = self.skillPageDict["ACTIVE"]
				getSkillMaxLevel=skill.GetSkillMaxLevel
				
				for i in xrange(self.ACTIVE_PAGE_SLOT_COUNT+1):
					slotIndex = i + skillPage.GetStartIndex() + 220
					skillIndex = player.GetSkillIndex(slotIndex)
					for j in xrange(skill.SKILL_GRADE_COUNT):
						skillPage.ClearSlot(self.__GetRealSkillSlot(j, i))
					
					if skillIndex == 0:
						continue
					
					skillGrade = player.GetSkillGrade(slotIndex)
					skillLevel = player.GetSkillLevel(slotIndex)
					skillType = skill.GetSkillType(skillIndex)
					for j in xrange(skill.SKILL_GRADE_COUNT):
						realSlotIndex = self.__GetRealSkillSlot(j, slotIndex - 220)
						
						skillPage.SetSkillSlotNew(realSlotIndex, skillIndex, j, skillLevel)
						skillPage.SetCoverButton(realSlotIndex)
						
						if (skillGrade == skill.SKILL_GRADE_COUNT) and j == (skill.SKILL_GRADE_COUNT-1):
							skillPage.SetSlotCountNew(realSlotIndex, skillGrade, skillLevel)
						elif (not self.__CanUseSkillNow()) or (skillGrade != j):
							skillPage.SetSlotCount(realSlotIndex, 0)
							skillPage.DisableCoverButton(realSlotIndex)
						else:
							skillPage.SetSlotCountNew(realSlotIndex, skillGrade, skillLevel)
				
				for btn in self.skillGroupButton:
					btn.SetUp()
				
				return
			else:
				self.skillGroupButton3.SetUp()
		
		for btn in self.skillGroupButton:
			btn.SetUp()
		self.skillGroupButton[index].Down()

		if self.__CanUseHorseSkill():
			if 0 == index:
				index = net.GetMainActorSkillGroup()-1
			elif 1 == index:
				index = self.PAGE_HORSE
		elif app.ENABLE_NEW_PASSIVE_SKILLS and 0 == index and net.GetMainActorSkillGroup() != 0:
			index = net.GetMainActorSkillGroup()-1
		
		self.curSelectedSkillGroup = index
		self.__SetSkillSlotData(net.GetMainActorRace(), index+1, net.GetMainActorEmpire())

	def __CanUseSkillNow(self):
		if 0 == net.GetMainActorSkillGroup():
			return False

		return True

	def __CanUseHorseSkill(self):

		slotIndex = player.GetSkillSlotIndex(player.SKILL_INDEX_RIDING)

		if not slotIndex:
			return False

		grade = player.GetSkillGrade(slotIndex)
		level = player.GetSkillLevel(slotIndex)
		if level < 0:
			level *= -1
		if grade >= 1 and level >= 1:
			return True

		return False

	def __IsChangedHorseRidingSkillLevel(self):
		ret = False

		if -1 == self.canUseHorseSkill:
			self.canUseHorseSkill = self.__CanUseHorseSkill()

		if self.canUseHorseSkill != self.__CanUseHorseSkill():
			ret = True

		self.canUseHorseSkill = self.__CanUseHorseSkill()
		return ret

	def __GetRealSkillSlot(self, skillGrade, skillSlot):
		return skillSlot + min(skill.SKILL_GRADE_COUNT-1, skillGrade)*skill.SKILL_GRADE_STEP_COUNT

	def __GetETCSkillRealSlotIndex(self, skillSlot):
		if skillSlot > 100:
			return skillSlot
		return skillSlot % self.ACTIVE_PAGE_SLOT_COUNT

	def __RealSkillSlotToSourceSlot(self, realSkillSlot):
		if realSkillSlot > 100:
			return realSkillSlot
		if self.PAGE_HORSE == self.curSelectedSkillGroup:
			return realSkillSlot + self.ACTIVE_PAGE_SLOT_COUNT
		return realSkillSlot % skill.SKILL_GRADE_STEP_COUNT

	def __GetSkillGradeFromSlot(self, skillSlot):
		return int(skillSlot / skill.SKILL_GRADE_STEP_COUNT)

	def SelectSkillGroup(self, index):
		self.__SelectSkillGroup(index)

	def OnQuestScroll(self):
		if self.toolTip == None:
			self.Close()
		questCount = quest.GetQuestCount()
		scrollLineCount = max(0, questCount - quest.QUEST_MAX_NUM)
		startIndex = int(scrollLineCount * self.questScrollBar.GetPos())

		if startIndex != self.questShowingStartIndex:
			self.questShowingStartIndex = startIndex
			self.RefreshQuest()
			
	if app.ENABLE_QUEST_RENEWAL:
		def __OnScrollQuest(self):
			if self.state != "QUEST":
				return

			curPos = self.questScrollBar.GetPos()
			if math.fabs(curPos - self.questLastScrollPosition) >= 0.001:
				self.RerenderQuestPage()
				self.questLastScrollPosition = curPos

		def ResetQuestScroll(self):
			self.questScrollBar.Hide()

			if self.questScrollBar.GetPos() != 0:
				self.questScrollBar.SetPos(0)

		def RerenderQuestPage(self):
			overflowingY = self.displayY - self.MAX_QUEST_PAGE_HEIGHT
			if overflowingY < 0:
				overflowingY = 0

			self.baseCutY = math.ceil(overflowingY * self.questScrollBar.GetPos())
			self.displayY = 0
			self.RearrangeQuestCategories(xrange(quest.QUEST_CATEGORY_MAX_NUM))
			self.RefreshQuestCategory()

			if overflowingY > 0:
				if (len(self.questOpenedCategories)) == 0:
					self.ResetQuestScroll()
				else:
					self.questScrollBar.Show()
			else:
				self.ResetQuestScroll()

		def __LoadQuestCategory(self):
			self.questPage.Show()

			if self.isLoaded == 0:
				return

			for i in xrange(quest.QUEST_CATEGORY_MAX_NUM):
				category = self.questCategoryList[i]

				categoryName = category.GetProperty("name")
				if not categoryName:
					category.SetProperty("name", category.GetText())
					categoryName = category.GetText()

				questCount = self.GetQuestCountInCategory(i)
				self.questCategoryList[i].SetTextAlignLeft(categoryName + " (" + str(questCount) + ")")
				self.questCategoryList[i].SetTextColor(self.GetQuestCategoryColor(i))
				self.questCategoryList[i].SetQuestLabel(quest_lable_expend_img_path_dict[i], self.GetQuestCountInCategory(i))
				self.questCategoryList[i].Show()

			self.RefreshQuestCategory()
			if self.isQuestCategoryLoad == False:
				self.questScrollBar.Hide()
			else:
				self.RerenderQuestPage()

			self.isQuestCategoryLoad = True

		def GetQuestCategoryColor(self, category):
			return self.questColorList["default_title"]

		def GetQuestProperties(self, questName):
			findString = {
				"*" : "blue",
				"&" : "green",
				"~" : "golden"
			}

			if questName[0] in findString:
				return (questName[1:], findString[questName[0]])

			return (questName, None)

		def IsQuestCategoryOpen(self, category):
			return (category in self.questOpenedCategories)

		def ToggleCategory(self, category):
			if self.IsQuestCategoryOpen(category):
				self.CloseQuestCategory(category)
			else:
				self.OpenQuestCategory(category)

		def RearrangeQuestCategories(self, categoryRange):
			i = 0
			for i in categoryRange:
				if (self.displayY - self.baseCutY) >= 0 and (self.displayY - self.baseCutY) < self.MAX_QUEST_PAGE_HEIGHT - 20:
					self.questCategoryList[i].SetPosition(13, (self.displayY - self.baseCutY) + 10)
					self.questCategoryList[i].Show()
				else:
					self.questCategoryList[i].Hide()

				self.displayY += 20
				self.questCategoryRenderPos[i] = self.displayY

		def CloseQuestCategory(self, category):
			self.questCategoryList[category].CloseCategory(self.GetQuestCountInCategory(category))

			if category in self.questOpenedCategories:
				self.questOpenedCategories.remove(category)

			for currentSlot in self.questSlotList:
				if currentSlot.GetProperty("category") == category:
					currentSlot.Hide()
					self.displayY -= currentSlot.GetHeight()

			self.RerenderQuestPage()

		def OpenQuestCategory(self, category):
			if self.GetQuestCountInCategory(category) == 0:
				return

			while len(self.questOpenedCategories) >= self.questMaxOpenedCategories:
				openedCategories = self.questOpenedCategories.pop()
				self.CloseQuestCategory(openedCategories)

			self.questCategoryList[category].OpenCategory(self.GetQuestCountInCategory(category))
			self.questOpenedCategories.append(category)
			self.RefreshQuestCategory(category)
			self.RerenderQuestPage()

		def RefreshQuestCategory(self, category = -1):
			if self.isLoaded == 0 or self.state != "QUEST":
				return

			categories = []
			if category == -1:
				categories = self.questOpenedCategories
			elif not category in self.questOpenedCategories:
				self.OpenQuestCategory(category)
				return
			else:
				categories.append(category)

			for currentCategory in categories:
				self.displayY = self.questCategoryRenderPos[currentCategory]

				self.LoadCategory(currentCategory)
				self.RearrangeQuestCategories(xrange(currentCategory + 1, quest.QUEST_CATEGORY_MAX_NUM))

		def RefreshQuestCategoriesCount(self):
			for category in xrange(quest.QUEST_CATEGORY_MAX_NUM):
				categoryName = self.questCategoryList[category].GetProperty("name")
				questCount = self.GetQuestCountInCategory(category)
				self.questCategoryList[category].SetTextAlignLeft(categoryName + " (" + str(questCount) + ")")

		def RefreshQuest(self):
			if self.isLoaded == 0 or self.state != "QUEST":
				return

			for category in self.questOpenedCategories:
				self.RefreshQuestCategory(category)

			self.RefreshQuestCategoriesCount()

		def CreateQuestSlot(self, name):
			for questSlot in self.questSlotList:
				if questSlot.GetWindowName() == name:
					return questSlot

			pyScrLoader = ui.PythonScriptLoader()
			slot = ui.ListBar()
			pyScrLoader.LoadElementListBar(slot, quest_slot_listbar, self.questPage)

			slot.SetParent(self.quest_page_board_window)
			slot.SetWindowName(name)
			slot.Hide()
			self.questSlotList.append(slot)
			return slot

		def SetQuest(self, slot, questID, questName, questCounterName, questCounterValue):
			(name, color) = self.GetQuestProperties(questName)
			slot.SetTextAlignLeft(name, 20)
			if color:
				slot.SetTextColor(self.questColorList[color])
			slot.SetEvent(ui.__mem_func__(self.__SelectQuest), questID)
			slot.SetWindowHorizontalAlignLeft()
			slot.Show()

		def LoadCategory(self, category):
			self.questIndexMap = {}
			self.questCounterList = []
			self.questClockList = []
			self.questSeparatorList = []

			for questSlot in self.questSlotList:
				questSlot.Hide()

			questCount = 0
			for questIdx in self.GetQuestsInCategory(category):
				questCount += 1
				(questID, questIndex, questName, questCategory, _, questCounterName, questCounterValue) = questIdx
				(lastName, lastTime) = quest.GetQuestLastTime(questID)

				slot = self.CreateQuestSlot("QuestSlotList_" + str(questCategory) + "_" + str(questID))

				slot.SetPosition(0, (self.displayY - self.baseCutY))
				slot.SetParent(self.quest_page_board_window)
				baseDisplayY = self.displayY

				## -- Quest Counter
				hasCounter = False
				if questCounterName != "":
					self.displayY += 15

					counter = ui.TextLine()
					counter.SetParent(slot)
					counter.SetPosition(20, 20 - 2.5)
					counter.SetText(questCounterName + ": " + str(questCounterValue))
					counter.Show()

					self.questCounterList.append(counter)
					hasCounter = True
				## -- Quest Counter

				## -- Quest Clock
				self.displayY += 15

				clockText = localeinfo.QUEST_UNLIMITED_TIME
				if len(lastName) > 0:
					if lastTime <= 0:
						clockText = localeinfo.QUEST_TIMEOVER
					else:
						questLastMinute = lastTime / 60
						questLastSecond = lastTime % 60

						clockText = lastName + " : "

						if questLastMinute > 0:
							clockText += str(questLastMinute) + localeinfo.QUEST_MIN
							if questLastSecond > 0:
								clockText += " "

						if questLastSecond > 0:
							clockText += str(questLastSecond) + localeinfo.QUEST_SEC

				clock = ui.TextLine()
				clock.SetParent(slot)
				clock.SetPosition(20, 20 + (int(hasCounter) * 14) - 2.5)
				clock.SetText(clockText)
				clock.SetProperty("idx", questID)
				self.questClockList.append(clock)
				clock.Show()
				## -- Quest Clock

				## -- Quest Separator
				self.displayY += 5
				if questCount < self.GetQuestCountInCategory(category):
					seperator = ui.ImageBox()
					seperator.SetParent(slot)
					seperator.SetPosition(4, 20 + (int(hasCounter) * 14 - 2.5) + 15)
					seperator.LoadImage("d:/ymir work/ui/quest_re/quest_list_line_01.tga")
					seperator.Show()
					self.questSeparatorList.append(seperator)
				## -- Quest Separator

				slot.SetProperty("category", questCategory)

				if questIndex in self.questClicked:
					slot.OnClickEvent()

				if (baseDisplayY - self.baseCutY) + 2 >= 0 and (baseDisplayY - self.baseCutY) + 2 < self.MAX_QUEST_PAGE_HEIGHT - 30:
					self.questIndexMap[questID] = questIndex
					self.SetQuest(slot, questID, questName, questCounterName, questCounterValue)

				self.displayY += 15

			newList = []
			for questSlot in self.questSlotList:
				if questSlot.IsShow():
					newList.append(questSlot)

			self.questSlotList = newList

		def __OnClickQuestCategoryButton(self, category):
			self.ToggleCategory(category)

		def GetQuestsInCategory(self, category, retCount = False):
			questList = []
			count = 0
			for i in xrange(quest.GetQuestCount()):
				(questIndex, questName, questCategory, questIcon, questCounterName, questCounterValue) = quest.GetQuestData(i)
				if questCategory == category:
					count += 1
					questList.append((i, questIndex, questName, questCategory, questIcon, questCounterName, questCounterValue))

			if retCount:
				return count

			return questList

		def GetQuestCountInCategory(self, category):
			return self.GetQuestsInCategory(category, True)
			

	
	
			
