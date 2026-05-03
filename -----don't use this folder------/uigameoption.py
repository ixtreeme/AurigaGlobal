import ui
import snd
import systemSetting
import net
import chat
import app
import constinfo
import chrmgr
import player
import uiprivateshopbuilder
import interfacemodule
import localeinfo

if app.ENABLE_MELEY_LAIR_DUNGEON:
	import background

blockMode = 0
viewChatMode = 0

MOBILE = False

if localeinfo.IsYMIR():
	MOBILE = True


class OptionDialog(ui.ScriptWindow):
	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.__Initialize()
		self.__Load()
		self.RefreshViewChat()
		self.RefreshAlwaysShowName()
		if app.OUTLINE_NAMES_TEXTLINE:
			self.RefreshNamesType()
		self.RefreshShowDamage()
		if app.WJ_SHOW_MOB_INFO:
			self.RefreshShowMobInfo()
		self.RefreshEnvironment()
		self.RefreshTimePm()
		self.RefreshHideMode()
		self.RefreshHideMode2()
		
		if app.ENABLE_AUTO_PICKUP:
			self.RefreshPickitem()

	def __del__(self):
		ui.ScriptWindow.__del__(self)
		print " -------------------------------------- DELETE GAME OPTION DIALOG"

	def __Initialize(self):
		self.titleBar = 0
		self.btnPages = []
		self.page = 0
		self.nameColorModeButtonList = []
		self.viewTargetBoardButtonList = []
		self.pvpModeButtonDict = {}
		self.blockButtonList = []
		self.viewChatButtonList = []
		self.alwaysShowNameButtonList = []
		if app.OUTLINE_NAMES_TEXTLINE:
			self.namesTypeButtonList = []
		self.showDamageButtonList = []
		if app.WJ_SHOW_MOB_INFO:
			self.showMobInfoButtonList = []
		self.dayButtonList = []
		self.timePmButtonList = []
		self.hideModeButtonList = []
		self.hideMode2ButtonList = []
		
		if app.ENABLE_AUTO_PICKUP:
			self.autopickitem = []

	def Destroy(self):
		self.ClearDictionary()

		self.__Initialize()
		print " -------------------------------------- DESTROY GAME OPTION DIALOG"

	def __Load_LoadScript(self, fileName):
		try:
			pyScriptLoader = ui.PythonScriptLoader()
			pyScriptLoader.LoadScriptFile(self, fileName)
		except:
			import exception
			exception.Abort("OptionDialog.__Load_LoadScript")

	def __Load_BindObject(self):
		try:
			GetObject = self.GetChild
			self.titleBar = GetObject("titlebar")
			for i in xrange(3):
				self.btnPages.append(GetObject("page_btn%d" % i))
			self.nameColorModeButtonList.append(GetObject("name_color_normal"))
			self.nameColorModeButtonList.append(GetObject("name_color_empire"))
			self.viewTargetBoardButtonList.append(GetObject("target_board_no_view"))
			self.viewTargetBoardButtonList.append(GetObject("target_board_view"))
			self.pvpModeButtonDict[player.PK_MODE_PEACE] = GetObject("pvp_peace")
			self.pvpModeButtonDict[player.PK_MODE_REVENGE] = GetObject("pvp_revenge")
			self.pvpModeButtonDict[player.PK_MODE_GUILD] = GetObject("pvp_guild")
			self.pvpModeButtonDict[player.PK_MODE_FREE] = GetObject("pvp_free")
			self.blockButtonList.append(GetObject("block_exchange_button"))
			self.blockButtonList.append(GetObject("block_party_button"))
			self.blockButtonList.append(GetObject("block_guild_button"))
			self.blockButtonList.append(GetObject("block_whisper_button"))
			self.blockButtonList.append(GetObject("block_friend_button"))
			self.blockButtonList.append(GetObject("block_party_request_button"))
			self.viewChatButtonList.append(GetObject("view_chat_on_button"))
			self.viewChatButtonList.append(GetObject("view_chat_off_button"))
			self.alwaysShowNameButtonList.append(GetObject("always_show_name_on_button"))
			self.alwaysShowNameButtonList.append(GetObject("always_show_name_off_button"))
			if app.OUTLINE_NAMES_TEXTLINE:
				self.namesTypeButtonList.append(GetObject("name_type1_button"))
				self.namesTypeButtonList.append(GetObject("name_type2_button"))
			self.showDamageButtonList.append(GetObject("show_damage_on_button"))
			self.showDamageButtonList.append(GetObject("show_damage_off_button"))
			if app.WJ_SHOW_MOB_INFO:
				self.showMobInfoButtonList.append(GetObject("show_mob_level_button"))
				self.showMobInfoButtonList.append(GetObject("show_mob_AI_flag_button"))

			global MOBILE
			if MOBILE:
				self.inputMobileButton = GetObject("input_mobile_button")
				self.deleteMobileButton = GetObject("delete_mobile_button")
			if app.__BL_PICK_FILTER__:
				GetObject("pick_up_show_button").SetEvent(ui.__mem_func__(self.OpenPickUpWindow))
			if app.ENABLE_AUTO_PICKUP:
				self.autopickitem.append(GetObject("auto_pick_item_on"))
				self.autopickitem.append(GetObject("auto_pick_item_off"))
			
			for i in xrange(7):
				self.dayButtonList.append(GetObject("environment_%d" % i))
			self.timePmButtonList.append(GetObject("time_pm_on"))
			self.timePmButtonList.append(GetObject("time_pm_off"))
			for i in xrange(7):
				self.hideModeButtonList.append(GetObject("hidemode_%d" % i))
			for i in xrange(4):
				self.hideMode2ButtonList.append(GetObject("hide2mode_%d" % i))
			
			
		except:
			import exception
			exception.Abort("OptionDialog.__Load_BindObject")

	def __Load(self):
		global MOBILE
		if MOBILE:
			self.__Load_LoadScript("uiscript/gameoptiondialog_formobile.py")
		else:
			self.__Load_LoadScript("uiscript/gameoptiondialog.py")

		self.__Load_BindObject()

		self.SetCenterPosition()

		self.titleBar.SetCloseEvent(ui.__mem_func__(self.Close))

		self.nameColorModeButtonList[0].SAFE_SetEvent(self.__OnClickNameColorModeNormalButton)
		self.nameColorModeButtonList[1].SAFE_SetEvent(self.__OnClickNameColorModeEmpireButton)

		self.viewTargetBoardButtonList[0].SAFE_SetEvent(self.__OnClickTargetBoardViewButton)
		self.viewTargetBoardButtonList[1].SAFE_SetEvent(self.__OnClickTargetBoardNoViewButton)

		self.pvpModeButtonDict[player.PK_MODE_PEACE].SAFE_SetEvent(self.__OnClickPvPModePeaceButton)
		self.pvpModeButtonDict[player.PK_MODE_REVENGE].SAFE_SetEvent(self.__OnClickPvPModeRevengeButton)
		self.pvpModeButtonDict[player.PK_MODE_GUILD].SAFE_SetEvent(self.__OnClickPvPModeGuildButton)
		self.pvpModeButtonDict[player.PK_MODE_FREE].SAFE_SetEvent(self.__OnClickPvPModeFreeButton)

		self.blockButtonList[0].SetToggleUpEvent(self.__OnClickBlockExchangeButton)
		self.blockButtonList[1].SetToggleUpEvent(self.__OnClickBlockPartyButton)
		self.blockButtonList[2].SetToggleUpEvent(self.__OnClickBlockGuildButton)
		self.blockButtonList[3].SetToggleUpEvent(self.__OnClickBlockWhisperButton)
		self.blockButtonList[4].SetToggleUpEvent(self.__OnClickBlockFriendButton)
		self.blockButtonList[5].SetToggleUpEvent(self.__OnClickBlockPartyRequest)

		self.blockButtonList[0].SetToggleDownEvent(self.__OnClickBlockExchangeButton)
		self.blockButtonList[1].SetToggleDownEvent(self.__OnClickBlockPartyButton)
		self.blockButtonList[2].SetToggleDownEvent(self.__OnClickBlockGuildButton)
		self.blockButtonList[3].SetToggleDownEvent(self.__OnClickBlockWhisperButton)
		self.blockButtonList[4].SetToggleDownEvent(self.__OnClickBlockFriendButton)
		self.blockButtonList[5].SetToggleDownEvent(self.__OnClickBlockPartyRequest)

		self.viewChatButtonList[0].SAFE_SetEvent(self.__OnClickViewChatOnButton)
		self.viewChatButtonList[1].SAFE_SetEvent(self.__OnClickViewChatOffButton)

		self.alwaysShowNameButtonList[0].SAFE_SetEvent(self.__OnClickAlwaysShowNameOnButton)
		self.alwaysShowNameButtonList[1].SAFE_SetEvent(self.__OnClickAlwaysShowNameOffButton)
		if app.OUTLINE_NAMES_TEXTLINE:
			self.namesTypeButtonList[0].SAFE_SetEvent(self.__OnClickNamesType1)
			self.namesTypeButtonList[1].SAFE_SetEvent(self.__OnClickNamesType2)
		
		self.showDamageButtonList[0].SAFE_SetEvent(self.__OnClickShowDamageOnButton)
		self.showDamageButtonList[1].SAFE_SetEvent(self.__OnClickShowDamageOffButton)
		
		if app.ENABLE_AUTO_PICKUP:
			self.autopickitem[0].SAFE_SetEvent(self.__OnClickPickitemOnButton)
			self.autopickitem[1].SAFE_SetEvent(self.__OnClickPickitemOffButton)
		
		if app.WJ_SHOW_MOB_INFO:
			self.showMobInfoButtonList[0].SetToggleUpEvent(self.__OnClickShowMobLevelButton)
			self.showMobInfoButtonList[0].SetToggleDownEvent(self.__OnClickShowMobLevelButton)
			self.showMobInfoButtonList[1].SetToggleUpEvent(self.__OnClickShowMobAIFlagButton)
			self.showMobInfoButtonList[1].SetToggleDownEvent(self.__OnClickShowMobAIFlagButton)
		
		

		self.__ClickRadioButton(self.nameColorModeButtonList, constinfo.GET_CHRNAME_COLOR_INDEX())
		self.__ClickRadioButton(self.viewTargetBoardButtonList, constinfo.GET_VIEW_OTHER_EMPIRE_PLAYER_TARGET_BOARD())
		self.__SetPeacePKMode()

		#global MOBILE
		if MOBILE:
			self.inputMobileButton.SetEvent(ui.__mem_func__(self.__OnChangeMobilePhoneNumber))
			self.deleteMobileButton.SetEvent(ui.__mem_func__(self.__OnDeleteMobilePhoneNumber))
		for i in xrange(7):
			self.dayButtonList[i].SAFE_SetEvent(self.OnClickEnvironmentMode, i)
		self.timePmButtonList[0].SAFE_SetEvent(self.__OnClickTimePmOnButton)
		self.timePmButtonList[1].SAFE_SetEvent(self.__OnClickTimePmOffButton)
		for i in xrange(7):
			self.hideModeButtonList[i].SetToggleUpEvent(self.__OnClickHideOptionUp, i)
			self.hideModeButtonList[i].SetToggleDownEvent(self.__OnClickHideOptionDown, i)
		for i in xrange(4):
			self.hideMode2ButtonList[i].SetToggleUpEvent(self.__OnClickHideOptionUp2, i)
			self.hideMode2ButtonList[i].SetToggleDownEvent(self.__OnClickHideOptionDown2, i)
		for i in xrange(3):
			self.btnPages[i].SAFE_SetEvent(self.SelectPage, i)
		self.SelectPage(self.page, force = True)

	def SelectPage(self, idx, retry = False, force = False):
		if idx == self.page and force != True:
			return
		
		for i in self.btnPages:
			i.SetUp()
		
		old = self.page
		
		if idx == 0:
			self.GetChild("page1").Show()
		else:
			self.GetChild("page1").Hide()
		
		if idx == 1:
			self.GetChild("page2").Show()
		else:
			self.GetChild("page2").Hide()
		
		if idx == 2:
			self.GetChild("page3").Show()
		else:
			self.GetChild("page3").Hide()
		
		try:
			self.btnPages[idx].Down()
			self.page = idx
		except:
			if retry:
				return
			
			self.SelectPage(old)

	if app.ENABLE_AUTO_PICKUP:
		def RefreshPickitem(self):
			if systemSetting.GetPickUpMode():
				self.autopickitem[0].Down()
				self.autopickitem[1].SetUp()
			else:
				self.autopickitem[0].SetUp()
				self.autopickitem[1].Down()

		def __OnClickPickitemOnButton(self):
			systemSetting.SetPickUpMode(1)
			self.RefreshPickitem()

		def __OnClickPickitemOffButton(self):
			systemSetting.SetPickUpMode(0)
			self.RefreshPickitem()

	def __ClickRadioButton(self, buttonList, buttonIndex):
		try:
			selButton=buttonList[buttonIndex]
		except IndexError:
			return

		for eachButton in buttonList:
			eachButton.SetUp()

		selButton.Down()

	def __SetNameColorMode(self, index):
		constinfo.SET_CHRNAME_COLOR_INDEX(index)
		self.__ClickRadioButton(self.nameColorModeButtonList, index)

	def __SetTargetBoardViewMode(self, flag):
		constinfo.SET_VIEW_OTHER_EMPIRE_PLAYER_TARGET_BOARD(flag)
		self.__ClickRadioButton(self.viewTargetBoardButtonList, flag)

	def __OnClickNameColorModeNormalButton(self):
		self.__SetNameColorMode(0)

	def __OnClickNameColorModeEmpireButton(self):
		self.__SetNameColorMode(1)

	def __OnClickTargetBoardViewButton(self):
		self.__SetTargetBoardViewMode(0)

	def __OnClickTargetBoardNoViewButton(self):
		self.__SetTargetBoardViewMode(1)

	def __OnClickCameraModeShortButton(self):
		self.__SetCameraMode(0)

	def __OnClickCameraModeLongButton(self):
		self.__SetCameraMode(1)

	def __OnClickFogModeLevel0Button(self):
		self.__SetFogLevel(0)

	def __OnClickFogModeLevel1Button(self):
		self.__SetFogLevel(1)

	def __OnClickFogModeLevel2Button(self):
		self.__SetFogLevel(2)

	if app.__BL_PICK_FILTER__:
		def OpenPickUpWindow(self):
			net.OpenPickUpWindow()

	def __OnClickBlockExchangeButton(self):
		self.RefreshBlock()
		global blockMode
		net.SendChatPacket("/setblockmode " + str(blockMode ^ player.BLOCK_EXCHANGE))
	def __OnClickBlockPartyButton(self):
		self.RefreshBlock()
		global blockMode
		net.SendChatPacket("/setblockmode " + str(blockMode ^ player.BLOCK_PARTY))
	def __OnClickBlockGuildButton(self):
		self.RefreshBlock()
		global blockMode
		net.SendChatPacket("/setblockmode " + str(blockMode ^ player.BLOCK_GUILD))
	def __OnClickBlockWhisperButton(self):
		self.RefreshBlock()
		global blockMode
		net.SendChatPacket("/setblockmode " + str(blockMode ^ player.BLOCK_WHISPER))
	def __OnClickBlockFriendButton(self):
		self.RefreshBlock()
		global blockMode
		net.SendChatPacket("/setblockmode " + str(blockMode ^ player.BLOCK_FRIEND))
	def __OnClickBlockPartyRequest(self):
		self.RefreshBlock()
		global blockMode
		net.SendChatPacket("/setblockmode " + str(blockMode ^ player.BLOCK_PARTY_REQUEST))

	def __OnClickViewChatOnButton(self):
		global viewChatMode
		viewChatMode = 1
		systemSetting.SetViewChatFlag(viewChatMode)
		self.RefreshViewChat()
	def __OnClickViewChatOffButton(self):
		global viewChatMode
		viewChatMode = 0
		systemSetting.SetViewChatFlag(viewChatMode)
		self.RefreshViewChat()

	def __OnClickAlwaysShowNameOnButton(self):
		systemSetting.SetAlwaysShowNameFlag(True)
		self.RefreshAlwaysShowName()

	def __OnClickAlwaysShowNameOffButton(self):
		systemSetting.SetAlwaysShowNameFlag(False)
		self.RefreshAlwaysShowName()

	def RefreshTimePm(self):
		if systemSetting.GetTimePm():
			self.timePmButtonList[0].Down()
			self.timePmButtonList[1].SetUp()
		else:
			self.timePmButtonList[0].SetUp()
			self.timePmButtonList[1].Down()

	def __OnClickTimePmOnButton(self):
		systemSetting.SetTimePm(True)
		self.RefreshTimePm()

	def __OnClickTimePmOffButton(self):
		systemSetting.SetTimePm(False)
		self.RefreshTimePm()

	def __OnClickShowDamageOnButton(self):
		systemSetting.SetShowDamageFlag(True)
		self.RefreshShowDamage()

	def __OnClickShowDamageOffButton(self):
		systemSetting.SetShowDamageFlag(False)
		self.RefreshShowDamage()

	def RefreshEnvironment(self):
		for btn in self.dayButtonList:
			btn.SetUp()
		
		try:
			self.dayButtonList[systemSetting.GetEnvironment()].Down()
		except:
			pass

	def OnClickEnvironmentMode(self, mode):
		systemSetting.SetEnvironment(mode)
		self.RefreshEnvironment()

	def RefreshHideMode(self):
		(b1, b2, b3, b4, b5, b6, b7) = systemSetting.GetHideModeStatus()
		if b1:
			self.hideModeButtonList[0].Down()
		else:
			self.hideModeButtonList[0].SetUp()
		
		if b2:
			self.hideModeButtonList[1].Down()
		else:
			self.hideModeButtonList[1].SetUp()
		
		if b3:
			self.hideModeButtonList[2].Down()
		else:
			self.hideModeButtonList[2].SetUp()
		
		if b4:
			self.hideModeButtonList[3].Down()
		else:
			self.hideModeButtonList[3].SetUp()
		
		if b5:
			self.hideModeButtonList[4].Down()
			uiprivateshopbuilder.UpdateADBoard()
		else:
			self.hideModeButtonList[4].SetUp()
		
		if b6:
			self.hideModeButtonList[5].Down()
		else:
			self.hideModeButtonList[5].SetUp()
		
		if b7:
			self.hideModeButtonList[6].Down()
		else:
			self.hideModeButtonList[6].SetUp()

	def __OnClickHideOptionUp(self, arg):
		systemSetting.SetHideModeStatus(arg, 0)

	def __OnClickHideOptionDown(self, arg):
		systemSetting.SetHideModeStatus(arg, 1)
		if arg == 4:
			uiprivateshopbuilder.UpdateADBoard()

	def RefreshHideMode2(self):
		(b1, b2, b3, b4) = systemSetting.GetHideModeStatus2()
		if b1:
			self.hideMode2ButtonList[0].Down()
		else:
			self.hideMode2ButtonList[0].SetUp()
		
		if b2:
			self.hideMode2ButtonList[1].Down()
		else:
			self.hideMode2ButtonList[1].SetUp()
		
		if b3:
			self.hideMode2ButtonList[2].Down()
		else:
			self.hideMode2ButtonList[2].SetUp()
		
		if b4:
			self.hideMode2ButtonList[3].Down()
		else:
			self.hideMode2ButtonList[3].SetUp()

	def __OnClickHideOptionUp2(self, arg):
		systemSetting.SetHideModeStatus2(arg, 0)

	def __OnClickHideOptionDown2(self, arg):
		systemSetting.SetHideModeStatus2(arg, 1)


	if app.WJ_SHOW_MOB_INFO:
		def __OnClickShowMobLevelButton(self):
			systemSetting.SetShowMobLevel(not systemSetting.IsShowMobLevel())
			self.RefreshShowMobInfo()
		def __OnClickShowMobAIFlagButton(self):
			systemSetting.SetShowMobAIFlag(not systemSetting.IsShowMobAIFlag())
			self.RefreshShowMobInfo()

	def __CheckPvPProtectedLevelPlayer(self):
		if player.GetStatus(player.LEVEL)<constinfo.PVPMODE_PROTECTED_LEVEL:
			self.__SetPeacePKMode()
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.OPTION_PVPMODE_PROTECT % (constinfo.PVPMODE_PROTECTED_LEVEL))
			return 1

		return 0

	def __SetPKMode(self, mode):
		for btn in self.pvpModeButtonDict.values():
			btn.SetUp()
		if self.pvpModeButtonDict.has_key(mode):
			self.pvpModeButtonDict[mode].Down()

	def __SetPeacePKMode(self):
		self.__SetPKMode(player.PK_MODE_PEACE)

	def __RefreshPVPButtonList(self):
		self.__SetPKMode(player.GetPKMode())

	if app.ENABLE_MELEY_LAIR_DUNGEON:
		def setMeleyMap(self):
			mapName = background.GetCurrentMapName()
			if mapName == "metin2_map_n_flame_dragon":
				if player.GetGuildID() != 0 and player.GetPKMode() != player.PK_MODE_GUILD:
					for btn in self.pvpModeButtonDict.values():
						btn.SetUp()
					
					net.SendChatPacket("/pkmode 4", chat.CHAT_TYPE_TALKING)
					self.pvpModeButtonDict[player.PK_MODE_GUILD].Down()

		def isMeleyMap(self, button):
			mapName = background.GetCurrentMapName()
			if mapName == "metin2_map_n_flame_dragon":
				if self.pvpModeButtonDict[button]:
					self.pvpModeButtonDict[button].SetUp()
				
				chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.CANNOT_CHANGE_FIGHT_MODE)
				return 1
			
			return 0


	def __OnClickPvPModePeaceButton(self):
	
		if app.ENABLE_MELEY_LAIR_DUNGEON:
			if self.isMeleyMap(player.PK_MODE_PEACE):
				return
				
		if self.__CheckPvPProtectedLevelPlayer():
			return

		self.__RefreshPVPButtonList()

		if constinfo.PVPMODE_ENABLE:
			net.SendChatPacket("/pkmode 0", chat.CHAT_TYPE_TALKING)
		else:
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.OPTION_PVPMODE_NOT_SUPPORT)

	def __OnClickPvPModeRevengeButton(self):
		if app.ENABLE_MELEY_LAIR_DUNGEON:
			if self.isMeleyMap(player.PK_MODE_REVENGE):
				return
	
		if self.__CheckPvPProtectedLevelPlayer():
			return

		self.__RefreshPVPButtonList()

		if constinfo.PVPMODE_ENABLE:
			net.SendChatPacket("/pkmode 1", chat.CHAT_TYPE_TALKING)
		else:
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.OPTION_PVPMODE_NOT_SUPPORT)

	def __OnClickPvPModeFreeButton(self):
		
		if app.ENABLE_MELEY_LAIR_DUNGEON:
			if self.isMeleyMap(player.PK_MODE_FREE):
				return
		if self.__CheckPvPProtectedLevelPlayer():
			return

		self.__RefreshPVPButtonList()

		if constinfo.PVPMODE_ENABLE:
			net.SendChatPacket("/pkmode 2", chat.CHAT_TYPE_TALKING)
		else:
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.OPTION_PVPMODE_NOT_SUPPORT)

	def __OnClickPvPModeGuildButton(self):
		if app.ENABLE_MELEY_LAIR_DUNGEON:
			if self.isMeleyMap(player.PK_MODE_GUILD):
				return
		if self.__CheckPvPProtectedLevelPlayer():
			return

		self.__RefreshPVPButtonList()

		if 0 == player.GetGuildID():
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.OPTION_PVPMODE_CANNOT_SET_GUILD_MODE)
			return

		if constinfo.PVPMODE_ENABLE:
			net.SendChatPacket("/pkmode 4", chat.CHAT_TYPE_TALKING)
		else:
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.OPTION_PVPMODE_NOT_SUPPORT)

	def OnChangePKMode(self):
		self.__RefreshPVPButtonList()

	def __OnChangeMobilePhoneNumber(self):
		global MOBILE
		if not MOBILE:
			return

		import uicommon
		inputDialog = uicommon.InputDialog()
		inputDialog.SetTitle(localeinfo.MESSENGER_INPUT_MOBILE_PHONE_NUMBER_TITLE)
		inputDialog.SetMaxLength(13)
		inputDialog.SetAcceptEvent(ui.__mem_func__(self.OnInputMobilePhoneNumber))
		inputDialog.SetCancelEvent(ui.__mem_func__(self.OnCloseInputDialog))
		inputDialog.Open()
		self.inputDialog = inputDialog

	def __OnDeleteMobilePhoneNumber(self):
		global MOBILE
		if not MOBILE:
			return

		import uicommon
		questionDialog = uicommon.QuestionDialog()
		questionDialog.SetText(localeinfo.MESSENGER_DO_YOU_DELETE_PHONE_NUMBER)
		questionDialog.SetAcceptEvent(ui.__mem_func__(self.OnDeleteMobile))
		questionDialog.SetCancelEvent(ui.__mem_func__(self.OnCloseQuestionDialog))
		questionDialog.Open()
		self.questionDialog = questionDialog

	def OnInputMobilePhoneNumber(self):
		global MOBILE
		if not MOBILE:
			return

		text = self.inputDialog.GetText()

		if not text:
			return

		text.replace('-', '')
		net.SendChatPacket("/mobile " + text)
		self.OnCloseInputDialog()
		return True

	def OnInputMobileAuthorityCode(self):
		global MOBILE
		if not MOBILE:
			return

		text = self.inputDialog.GetText()
		net.SendChatPacket("/mobile_auth " + text)
		self.OnCloseInputDialog()
		return True

	def OnDeleteMobile(self):
		global MOBILE
		if not MOBILE:
			return

		net.SendChatPacket("/mobile")
		self.OnCloseQuestionDialog()
		return True

	def OnCloseInputDialog(self):
		self.inputDialog.Close()
		self.inputDialog = None
		return True

	def OnCloseQuestionDialog(self):
		self.questionDialog.Close()
		self.questionDialog = None
		return True

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def RefreshMobile(self):
		global MOBILE
		if not MOBILE:
			return

		if player.HasMobilePhoneNumber():
			self.inputMobileButton.Hide()
			self.deleteMobileButton.Show()
		else:
			self.inputMobileButton.Show()
			self.deleteMobileButton.Hide()

	def OnMobileAuthority(self):
		global MOBILE
		if not MOBILE:
			return

		import uicommon
		inputDialog = uicommon.InputDialogWithDescription()
		inputDialog.SetTitle(localeinfo.MESSENGER_INPUT_MOBILE_AUTHORITY_TITLE)
		inputDialog.SetDescription(localeinfo.MESSENGER_INPUT_MOBILE_AUTHORITY_DESCRIPTION)
		inputDialog.SetAcceptEvent(ui.__mem_func__(self.OnInputMobileAuthorityCode))
		inputDialog.SetCancelEvent(ui.__mem_func__(self.OnCloseInputDialog))
		inputDialog.SetMaxLength(4)
		inputDialog.SetBoardWidth(310)
		inputDialog.Open()
		self.inputDialog = inputDialog

	def RefreshBlock(self):
		global blockMode
		for i in xrange(len(self.blockButtonList)):
			if 0 != (blockMode & (1 << i)):
				self.blockButtonList[i].Down()
			else:
				self.blockButtonList[i].SetUp()

	def RefreshViewChat(self):
		if systemSetting.IsViewChat():
			self.viewChatButtonList[0].Down()
			self.viewChatButtonList[1].SetUp()
		else:
			self.viewChatButtonList[0].SetUp()
			self.viewChatButtonList[1].Down()

	def RefreshAlwaysShowName(self):
		if systemSetting.IsAlwaysShowName():
			self.alwaysShowNameButtonList[0].Down()
			self.alwaysShowNameButtonList[1].SetUp()
		else:
			self.alwaysShowNameButtonList[0].SetUp()
			self.alwaysShowNameButtonList[1].Down()

	if app.OUTLINE_NAMES_TEXTLINE:
		def RefreshNamesType(self):
			if not systemSetting.GetNamesType():
				self.namesTypeButtonList[0].Down()
				self.namesTypeButtonList[1].SetUp()
			else:
				self.namesTypeButtonList[0].SetUp()
				self.namesTypeButtonList[1].Down()

		def __OnClickNamesType1(self):
			systemSetting.SetNamesType(False)
			self.RefreshNamesType()

		def __OnClickNamesType2(self):
			systemSetting.SetNamesType(True)
			self.RefreshNamesType()

	def RefreshShowDamage(self):
		if systemSetting.IsShowDamage():
			self.showDamageButtonList[0].Down()
			self.showDamageButtonList[1].SetUp()
		else:
			self.showDamageButtonList[0].SetUp()
			self.showDamageButtonList[1].Down()

	if app.WJ_SHOW_MOB_INFO:
		def RefreshShowMobInfo(self):
			if systemSetting.IsShowMobLevel():
				self.showMobInfoButtonList[0].Down()
			else:
				self.showMobInfoButtonList[0].SetUp()
			if systemSetting.IsShowMobAIFlag():
				self.showMobInfoButtonList[1].Down()
			else:
				self.showMobInfoButtonList[1].SetUp()

	def OnBlockMode(self, mode):
		global blockMode
		blockMode = mode
		self.RefreshBlock()

	def Show(self):
		self.RefreshMobile()
		self.RefreshBlock()
		self.SelectPage(self.page)
		ui.ScriptWindow.Show(self)
		if app.ENABLE_MELEY_LAIR_DUNGEON:
			self.setMeleyMap()

	def Close(self):
		self.Hide()
