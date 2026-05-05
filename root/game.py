import app
import chr
import chrmgr
import player
import net
import marek38
import os
import dbg
import grp
import item
import background
import snd
import chat
import textTail
import snd
import effect
import wndMgr
import fly
import systemSetting
import quest
import guild
import skill
import messenger
import constinfo
import exchange
import ime
if app.AUTO_SHOUT:
	import uishout
import ui
import uicommon
#import uiphasecurtain
#import uimapnameshower
import uiaffectshower
import uiplayergauge
import uicharacter
import uitarget


# PRIVATE_SHOP_PRICE_LIST
import uiprivateshopbuilder
# END_OF_PRIVATE_SHOP_PRICE_LIST

import mousemodule
import consolemodule
import playersettingmodule
import interfacemodule

import musicinfo
import debuginfo
import stringcommander
from _weakref import proxy
import localeinfo

if app.ENABLE_PVP_ADVANCED:
	import constinfo as pvp
	import uicommon as message
	import uiduel

# TEXTTAIL_LIVINGTIME_CONTROL
#if localeinfo.IsJAPAN():
#	app.SetTextTailLivingTime(8.0)
# END_OF_TEXTTAIL_LIVINGTIME_CONTROL

# SCREENSHOT_CWDSAVE
if app.__ENABLE_NEW_OFFLINESHOP__:
	import uiofflineshop
	import offlineshop
	
if app.NEW_PET_SYSTEM:
	import uipetsystem
	
if app.ENABLE_ITEMSHOP:
	import uiItemShop

SCREENSHOT_CWDSAVE = False
SCREENSHOT_DIR = None

if localeinfo.IsEUROPE():
	SCREENSHOT_CWDSAVE = True

if localeinfo.IsCIBN10():
	SCREENSHOT_CWDSAVE = False
	SCREENSHOT_DIR = "YT2W"

import uiwheel

cameraDistance = 1550.0
cameraPitch = 27.0
cameraRotation = 0.0
cameraHeight = 100.0
testAlignment = 0

class GameWindow(ui.ScriptWindow):
	def __init__(self, stream):
		ui.ScriptWindow.__init__(self, "GAME")
		self.SetWindowName("game")
		net.SetPhaseWindow(net.PHASE_WINDOW_GAME, self)
		player.SetGameWindow(self)
		self.quickSlotPageIndex = 0
		self.lastPKModeSendedTime = 0
		self.pressNumber = None
		import dailygift
		self.wnddailygift = dailygift.DailyGift()
		self.guildWarQuestionDialog = None
		self.interface = None
		self.targetBoard = None
		self.console = None
		self.mapNameShower = None
		self.affectShower = None
		self.playerGauge = None
		#if app.ENABLE_CHRISTMAS_WHEEL_OF_DESTINY:
		self.wheeldestiny  = None
		
		if app.AUTO_SHOUT:
			self.shouttime = 0
		if app.__ENABLE_NEW_OFFLINESHOP__:
			offlineshop.HideShopNames()
			self.Offlineshop = uiofflineshop.NewOfflineShopBoard()
			self.Offlineshop.Hide()

		self.stream=stream
		self.interface = interfacemodule.Interface()
		
		if app.ENABLE_ITEMSHOP:
			constinfo.SetInterfaceInstance(self.interface)
		if app.ENABLE_EVENT_MANAGER:
			constinfo.SetInterfaceInstance(self.interface)
		self.interface.BindGameClass(self)
		self.interface.MakeInterface()
		self.interface.ShowDefaultWindows()
		
		#self.curtain = uiphasecurtain.PhaseCurtain()
		#self.curtain.speed = 0.03
		#self.curtain.Hide()

		self.targetBoard = uitarget.TargetBoard()
		self.targetBoard.SetWhisperEvent(ui.__mem_func__(self.interface.OpenWhisperDialog))
		self.targetBoard.Hide()
		if app.NEW_PET_SYSTEM:
			self.petmain = uipetsystem.PetSystemMain()
			self.petmini = uipetsystem.PetSystemMini()
			self.petmini.BindGameClass(self)

		self.console = consolemodule.ConsoleWindow()
		self.console.BindGameClass(self)
		self.console.SetConsoleSize(wndMgr.GetScreenWidth(), 200)
		self.console.Hide()
		
		#self.mapNameShower = uimapnameshower.MapNameShower()
		self.affectShower = uiaffectshower.AffectShower()

		if app.ENABLE_PVP_ADVANCED:
			self.wndDuelGui = uiduel.Initializate()
			self.wndDuelLive = uiduel.WindowLiveInformations()
		
		self.playerGauge = uiplayergauge.PlayerGauge(self)
		self.playerGauge.Hide()
		#wj 2014.1.2. ESC키를 누를 시 우선적으로 DropQuestionDialog를 끄도록 만들었다. 하지만 처음에 itemDropQuestionDialog가 선언되어 있지 않아 ERROR가 발생하여 init에서 선언과 동시에 초기화 시킴.
		#if app.ENABLE_CHRISTMAS_WHEEL_OF_DESTINY:
		self.wheeldestiny = uiwheel.WheelOfDestiny()
		
		self.itemDropQuestionDialog = None
		self.__SetQuickSlotMode()

		self.__ServerCommand_Build()
		self.__ProcessPreservedServerCommand()
		if app.INGAME_WIKI:
			import ingamewiki
			self.wndWiki = ingamewiki.InGameWiki()
		
		if app.ENABLE_BIOLOGIST_UI:
			self.biologistAlert = uicommon.PopupDialog()
			self.biologistAlert.SetText(localeinfo.BIOLOGIST_WAIT_FINISH)
			try:
				import uibiolog
				self.wndBiolog = uibiolog.BiologWindow()
				self.wndBiolog.Load()
			except:
				self.wndBiolog = None


	def __del__(self):
		player.SetGameWindow(0)
		net.ClearPhaseWindow(net.PHASE_WINDOW_GAME, self)
		ui.ScriptWindow.__del__(self)

	def Open(self):
		app.SetFrameSkip(1)

		self.SetSize(wndMgr.GetScreenWidth(), wndMgr.GetScreenHeight())

		self.quickSlotPageIndex = 0
		self.PickingCharacterIndex = -1
		self.PickingItemIndex = -1
		self.consoleEnable = False
		self.isShowDebugInfo = False
		self.ShowNameFlag = False

		self.enableXMasBoom = False
		self.startTimeXMasBoom = 0.0
		self.indexXMasBoom = 0

		global cameraDistance, cameraPitch, cameraRotation, cameraHeight
		if app.ENABLE_SAVECAMERA_PREFERENCES:
			import systemSetting
			app.SetCamera(systemSetting.GetCameraHeight(), cameraPitch, cameraRotation, cameraHeight)
		else:
			app.SetCamera(cameraDistance, cameraPitch, cameraRotation, cameraHeight)
		
		if app.ENABLE_SAVECAMERA_PREFERENCES:
			constinfo.SET_CAMERA_MAX_DISTANCE_INDEX(systemSetting.GetCameraType())
		
		constinfo.SET_DEFAULT_CAMERA_MAX_DISTANCE()
		constinfo.SET_DEFAULT_CHRNAME_COLOR()
		constinfo.SET_DEFAULT_FOG_LEVEL()
		constinfo.SET_DEFAULT_CONVERT_EMPIRE_LANGUAGE_ENABLE()
		constinfo.SET_DEFAULT_USE_ITEM_WEAPON_TABLE_ATTACK_BONUS()
		constinfo.SET_DEFAULT_USE_SKILL_EFFECT_ENABLE()

		# TWO_HANDED_WEAPON_ATTACK_SPEED_UP
		constinfo.SET_TWO_HANDED_WEAPON_ATT_SPEED_DECREASE_VALUE()
		# END_OF_TWO_HANDED_WEAPON_ATTACK_SPEED_UP

		import event
		event.SetLeftTimeString(localeinfo.UI_LEFT_TIME)

		textTail.EnablePKTitle(constinfo.PVPMODE_ENABLE)

		if constinfo.PVPMODE_TEST_ENABLE:
			self.testPKMode = ui.TextLine()
			self.testPKMode.SetFontName(localeinfo.UI_DEF_FONT)
			self.testPKMode.SetPosition(0, 15)
			self.testPKMode.SetWindowHorizontalAlignCenter()
			self.testPKMode.SetHorizontalAlignCenter()
			self.testPKMode.SetFeather()
			self.testPKMode.SetOutline()
			self.testPKMode.Show()

			self.testAlignment = ui.TextLine()
			self.testAlignment.SetFontName(localeinfo.UI_DEF_FONT)
			self.testAlignment.SetPosition(0, 35)
			self.testAlignment.SetWindowHorizontalAlignCenter()
			self.testAlignment.SetHorizontalAlignCenter()
			self.testAlignment.SetFeather()
			self.testAlignment.SetOutline()
			self.testAlignment.Show()

		self.__BuildKeyDict()
		self.__BuildDebugInfo()

		# PRIVATE_SHOP_PRICE_LIST
		uiprivateshopbuilder.Clear()
		# END_OF_PRIVATE_SHOP_PRICE_LIST

		# UNKNOWN_UPDATE
		exchange.InitTrading()
		# END_OF_UNKNOWN_UPDATE


		## Sound
		snd.SetMusicVolume(systemSetting.GetMusicVolume())
		snd.SetSoundVolume(systemSetting.GetSoundVolume())

		netFieldMusicFileName = net.GetFieldMusicFileName()
		if netFieldMusicFileName:
			snd.FadeInMusic("BGM/" + netFieldMusicFileName)
		elif musicinfo.fieldMusic != "":
			snd.FadeInMusic("BGM/" + musicinfo.fieldMusic)

		self.__SetQuickSlotMode()
		self.__SelectQuickPage(self.quickSlotPageIndex)

		self.SetFocus()
		self.Show()
		app.ShowCursor()
		net.SendEnterGamePacket()

		# START_GAME_ERROR_EXIT
		try:
			self.StartGame()
		except:
			import exception
			exception.Abort("GameWindow.Open")
		# END_OF_START_GAME_ERROR_EXIT

		# NPC가 큐브시스템으로 만들 수 있는 아이템들의 목록을 캐싱
		# ex) cubeInformation[20383] = [ {"rewordVNUM": 72723, "rewordCount": 1, "materialInfo": "101,1&102,2", "price": 999 }, ... ]
		self.cubeInformation = {}
		self.currentCubeNPC = 0
		if app.AUTO_SHOUT:
			self.shouttime = app.GetTime()+20
	def Close(self):
		self.Hide()
		global cameraDistance, cameraPitch, cameraRotation, cameraHeight
		(cameraDistance, cameraPitch, cameraRotation, cameraHeight) = app.GetCamera()

		if musicinfo.fieldMusic != "":
			snd.FadeOutMusic("BGM/"+ musicinfo.fieldMusic)

		self.onPressKeyDict = None
		self.onClickKeyDict = None

		chat.Close()
		snd.StopAllSound()
		grp.InitScreenEffect()
		chr.Destroy()
		textTail.Clear()
		quest.Clear()
		background.Destroy()
		guild.Destroy()
		messenger.Destroy()
		skill.ClearSkillData()
		wndMgr.Unlock()
		mousemodule.mouseController.DeattachObject()

		if self.guildWarQuestionDialog:
			self.guildWarQuestionDialog.Close()

		self.guildNameBoard = None
		self.partyRequestQuestionDialog = None
		self.partyInviteQuestionDialog = None
		self.guildInviteQuestionDialog = None
		self.guildWarQuestionDialog = None
		self.messengerAddFriendQuestion = None
		if app.INGAME_WIKI:
			if self.wndWiki:
				self.wndWiki.Hide()
				self.wndWiki = None
		
		if app.__ENABLE_NEW_OFFLINESHOP__:
			if self.Offlineshop:
				self.Offlineshop.Destroy()
				self.Offlineshop = None
		
		# UNKNOWN_UPDATE
		self.itemDropQuestionDialog = None
		# END_OF_UNKNOWN_UPDATE

		# QUEST_CONFIRM
		self.confirmDialog = None
		# END_OF_QUEST_CONFIRM

		self.PrintCoord = None
		self.FrameRate = None
		self.Pitch = None
		self.Splat = None
		self.TextureNum = None
		self.ObjectNum = None
		self.ViewDistance = None
		self.PrintMousePos = None

		self.ClearDictionary()
		if app.NEW_PET_SYSTEM:
			self.petmain.Close()
			self.petmini.Close()

		self.playerGauge = None
		#self.mapNameShower = None
		self.affectShower = None

		if self.console:
			self.console.BindGameClass(0)
			self.console.Close()
			self.console=None

		if app.ENABLE_PVP_ADVANCED:
			if self.wndDuelLive:
				self.wndDuelLive.Close()

		if self.targetBoard:
			self.targetBoard.Destroy()
			self.targetBoard = None
		
		if self.interface:
			self.interface.HideAllWindows()
			self.interface.Close()
			self.interface=None
		player.ClearSkillDict()
		player.ResetCameraRotation()
		if app.INGAME_WIKI:
			if self.wndWiki:
				del self.wndWiki
				self.self.wndWiki = None
		
		if app.ENABLE_BIOLOGIST_UI:
			if self.biologistAlert:
				self.biologistAlert.Destroy()
				self.biologistAlert = None
		
		self.KillFocus()
		if app.ENABLE_EVENT_MANAGER:
			constinfo.SetInterfaceInstance(None)
			
		if app.ENABLE_ITEMSHOP:
			constinfo.SetInterfaceInstance(None)
		app.HideCursor()

		print "---------------------------------------------------------------------------- CLOSE GAME WINDOW"

	if app.ENABLE_RANKING:
		def OpenRanking(self):
			if self.interface:
				self.interface.OpenRanking()

		def RankingClearData(self):
			if self.interface:
				self.interface.RankingClearData()

		def RankingAddRank(self, position, level, points, name, realPosition):
			if self.interface:
				self.interface.RankingAddRank(position, level, points, name, realPosition)

		def RankingRefresh(self):
			if self.interface:
				self.interface.RankingRefresh()
				
	if app.ENABLE_ITEMSHOP:
		def ItemShopClear(self, updateTime):
			uiItemShop.ItemShopClear(int(updateTime))
		def ItemShopAppendItem(self, categoryIndex, categorySubIndex, itemID, itemVnum, itemPrice, itemDiscount, itemOffertime, itemTopSelling, itemAddedTime, itemSellingCount):
			uiItemShop.ItemShopAppendItem(int(categoryIndex), int(categorySubIndex), int(itemID), int(itemVnum), long(itemPrice), int(itemDiscount), int(itemOffertime), int(itemTopSelling), int(itemAddedTime), long(itemSellingCount))
		def ItemShopHideLoading(self):
			self.interface.ItemShopHideLoading()
		def ItemShopOpenMainPage(self):
			self.interface.OpenItemShopMainWindow()
		def ItemShopLogClear(self):
			uiItemShop.ItemShopLogClear()
		def ItemShopAppendLog(self, dateText, dateTime, playerName, ipAdress, itemVnum, itemCount, itemPrice):
			uiItemShop.ItemShopAppendLog(str(dateText), int(dateTime), str(playerName), str(ipAdress), int(itemVnum), int(itemCount), long(itemPrice))
		def ItemShopPurchasesWindow(self):
			self.interface.ItemShopPurchasesWindow()
		def ItemShopSetDragonCoin(self, dragonCoin):
			self.interface.ItemShopSetDragonCoin(long(dragonCoin))

	if app.ENABLE_RUNE_SYSTEM:
		def RuneStatus(self, flag):
			if self.interface:
				self.interface.RuneStatus(int(flag))

		def RuneAffect(self, flag):
			if self.interface:
				self.interface.RuneAffect(int(flag))

	def __BuildKeyDict(self):
		onPressKeyDict = {}
		onPressKeyDict[app.DIK_1] = lambda : self.__PressNumKey(1)
		onPressKeyDict[app.DIK_2] = lambda : self.__PressNumKey(2)
		onPressKeyDict[app.DIK_3] = lambda : self.__PressNumKey(3)
		onPressKeyDict[app.DIK_4] = lambda : self.__PressNumKey(4)
		onPressKeyDict[app.DIK_5] = lambda : self.__PressNumKey(5)
		onPressKeyDict[app.DIK_6] = lambda : self.__PressNumKey(6)
		onPressKeyDict[app.DIK_7] = lambda : self.__PressNumKey(7)
		onPressKeyDict[app.DIK_8] = lambda : self.__PressNumKey(8)
		onPressKeyDict[app.DIK_9] = lambda : self.__PressNumKey(9)
		onPressKeyDict[app.DIK_F1] = lambda : self.__PressQuickSlot(4)
		onPressKeyDict[app.DIK_F2] = lambda : self.__PressQuickSlot(5)
		onPressKeyDict[app.DIK_F3] = lambda : self.__PressQuickSlot(6)
		onPressKeyDict[app.DIK_F4] = lambda : self.__PressQuickSlot(7)
		if app.ENABLE_EVENT_MANAGER:
			onPressKeyDict[app.DIK_F5]	= lambda : self.interface.OpenEventCalendar()
		#onPressKeyDict[app.DIK_F12] = lambda : self.OpenRanking()
		onPressKeyDict[app.DIK_F12] = lambda : self.interface.OpenShoutWindow()
		# if app.ADVANCED_GUILD_INFO:
			# onPressKeyDict[app.DIK_F6] = lambda : self.GuildWar_OpenRank()
		# if app.ENABLE_DUNGEON_INFO_SYSTEM:
			# onPressKeyDict[app.DIK_F7] = lambda : self.OpenDungeoninfo()
		if app.ENABLE_BATTLE_PASS:
			onPressKeyDict[app.DIK_F8] = lambda : self.OpenBattlePassWindow()
		if app.ENABLE_WHISPER_ADMIN_SYSTEM:
			onPressKeyDict[app.DIK_F9] = lambda : self.OpenWhisperSystem()
		#onPressKeyDict[app.DIK_F5] = lambda : self.OpenTeleporter()
		if app.ENABLE_PVP_ADVANCED:
			onPressKeyDict[app.DIK_F10] = lambda : self.ClickPvpEquipment()
		onPressKeyDict[app.DIK_F11] = lambda : self.OpenSavepoint()
		if app.ENABLE_EXTRA_INVENTORY:
			onPressKeyDict[app.DIK_U] = lambda : self.OpenExtrainventory()
		
		onPressKeyDict[app.DIK_LALT]		= lambda : self.ShowName()
		onPressKeyDict[app.DIK_LCONTROL]	= lambda : self.ShowMouseImage()
		#onPressKeyDict[app.DIK_SYSRQ]		= lambda : 
		onPressKeyDict[app.DIK_SPACE]		= lambda : self.StartAttack()

		#캐릭터 이동키
		onPressKeyDict[app.DIK_UP]			= lambda : self.MoveUp()
		onPressKeyDict[app.DIK_DOWN]		= lambda : self.MoveDown()
		onPressKeyDict[app.DIK_LEFT]		= lambda : self.MoveLeft()
		onPressKeyDict[app.DIK_RIGHT]		= lambda : self.MoveRight()
		onPressKeyDict[app.DIK_W]			= lambda : self.MoveUp()
		onPressKeyDict[app.DIK_S]			= lambda : self.MoveDown()
		onPressKeyDict[app.DIK_A]			= lambda : self.MoveLeft()
		onPressKeyDict[app.DIK_D]			= lambda : self.MoveRight()

		onPressKeyDict[app.DIK_E]			= lambda: app.RotateCamera(app.CAMERA_TO_POSITIVE)
		onPressKeyDict[app.DIK_R]			= lambda: app.ZoomCamera(app.CAMERA_TO_NEGATIVE)
		#onPressKeyDict[app.DIK_F]			= lambda: app.ZoomCamera(app.CAMERA_TO_POSITIVE)
		onPressKeyDict[app.DIK_T]			= lambda: app.PitchCamera(app.CAMERA_TO_NEGATIVE)
		onPressKeyDict[app.DIK_G]			= self.__PressGKey
		onPressKeyDict[app.DIK_Q]			= self.__PressQKey

		onPressKeyDict[app.DIK_NUMPAD9]		= lambda: app.MovieResetCamera()
		onPressKeyDict[app.DIK_NUMPAD4]		= lambda: app.MovieRotateCamera(app.CAMERA_TO_NEGATIVE)
		onPressKeyDict[app.DIK_NUMPAD6]		= lambda: app.MovieRotateCamera(app.CAMERA_TO_POSITIVE)
		onPressKeyDict[app.DIK_PGUP]		= lambda: app.MovieZoomCamera(app.CAMERA_TO_NEGATIVE)
		onPressKeyDict[app.DIK_PGDN]		= lambda: app.MovieZoomCamera(app.CAMERA_TO_POSITIVE)
		onPressKeyDict[app.DIK_NUMPAD8]		= lambda: app.MoviePitchCamera(app.CAMERA_TO_NEGATIVE)
		onPressKeyDict[app.DIK_NUMPAD2]		= lambda: app.MoviePitchCamera(app.CAMERA_TO_POSITIVE)
		onPressKeyDict[app.DIK_GRAVE]		= lambda : self.PickUpItem()
		onPressKeyDict[app.DIK_Z]			= lambda : self.PickUpItem()
		onPressKeyDict[app.DIK_C]			= lambda state = "STATUS": self.interface.ToggleCharacterWindow(state)
		onPressKeyDict[app.DIK_V]			= lambda state = "SKILL": self.interface.ToggleCharacterWindow(state)
		#onPressKeyDict[app.DIK_B]			= lambda state = "EMOTICON": self.interface.ToggleCharacterWindow(state)
		onPressKeyDict[app.DIK_N]			= lambda state = "QUEST": self.interface.ToggleCharacterWindow(state)
		onPressKeyDict[app.DIK_I]			= lambda : self.interface.ToggleInventoryWindow()
		#onPressKeyDict[app.DIK_O]			= lambda : self.interface.ToggleDragonSoulWindowWithNoInfo()
		onPressKeyDict[app.DIK_M]			= lambda : self.interface.PressMKey()
		onPressKeyDict[app.DIK_ADD]			= lambda : self.interface.MiniMapScaleUp()
		onPressKeyDict[app.DIK_SUBTRACT]	= lambda : self.interface.MiniMapScaleDown()
		onPressKeyDict[app.DIK_L]			= lambda : self.interface.ToggleChatLogWindow()
		#onPressKeyDict[app.DIK_COMMA]		= lambda : self.ShowConsole()		# "`" key
		onPressKeyDict[app.DIK_LSHIFT]		= lambda : self.__SetQuickPageMode()

		onPressKeyDict[app.DIK_J]			= lambda : self.__PressJKey()
		onPressKeyDict[app.DIK_H]			= lambda : self.__PressHKey()
		onPressKeyDict[app.DIK_B]			= lambda : self.__PressBKey()
		onPressKeyDict[app.DIK_F]			= lambda : self.__PressFKey()
		if app.__ENABLE_NEW_OFFLINESHOP__:
			onPressKeyDict[app.DIK_Y]		= lambda : self.__PressYKey()
#		if app.NEW_PET_SYSTEM:
#			onPressKeyDict[app.DIK_P] = lambda: self.OpenPetMainGui()

		# CUBE_TEST
		onPressKeyDict[app.DIK_K]			= lambda : self.interface.ToggleSwitchbotWindow()
		# CUBE_TEST_END
		self.onPressKeyDict = onPressKeyDict

		onClickKeyDict = {}
		onClickKeyDict[app.DIK_UP] = lambda : self.StopUp()
		onClickKeyDict[app.DIK_DOWN] = lambda : self.StopDown()
		onClickKeyDict[app.DIK_LEFT] = lambda : self.StopLeft()
		onClickKeyDict[app.DIK_RIGHT] = lambda : self.StopRight()
		onClickKeyDict[app.DIK_SPACE] = lambda : self.EndAttack()

		onClickKeyDict[app.DIK_W] = lambda : self.StopUp()
		onClickKeyDict[app.DIK_S] = lambda : self.StopDown()
		onClickKeyDict[app.DIK_A] = lambda : self.StopLeft()
		onClickKeyDict[app.DIK_D] = lambda : self.StopRight()
		onClickKeyDict[app.DIK_Q] = lambda: app.RotateCamera(app.CAMERA_STOP)
		onClickKeyDict[app.DIK_E] = lambda: app.RotateCamera(app.CAMERA_STOP)
		onClickKeyDict[app.DIK_R] = lambda: app.ZoomCamera(app.CAMERA_STOP)
		onClickKeyDict[app.DIK_F] = lambda: app.ZoomCamera(app.CAMERA_STOP)
		onClickKeyDict[app.DIK_T] = lambda: app.PitchCamera(app.CAMERA_STOP)
		onClickKeyDict[app.DIK_G] = lambda: self.__ReleaseGKey()
		onClickKeyDict[app.DIK_NUMPAD4] = lambda: app.MovieRotateCamera(app.CAMERA_STOP)
		onClickKeyDict[app.DIK_NUMPAD6] = lambda: app.MovieRotateCamera(app.CAMERA_STOP)
		onClickKeyDict[app.DIK_PGUP] = lambda: app.MovieZoomCamera(app.CAMERA_STOP)
		onClickKeyDict[app.DIK_PGDN] = lambda: app.MovieZoomCamera(app.CAMERA_STOP)
		onClickKeyDict[app.DIK_NUMPAD8] = lambda: app.MoviePitchCamera(app.CAMERA_STOP)
		onClickKeyDict[app.DIK_NUMPAD2] = lambda: app.MoviePitchCamera(app.CAMERA_STOP)
		onClickKeyDict[app.DIK_LALT] = lambda: self.HideName()
		onClickKeyDict[app.DIK_LCONTROL] = lambda: self.HideMouseImage()
		onClickKeyDict[app.DIK_LSHIFT] = lambda: self.__SetQuickSlotMode()
		#if constinfo.PVPMODE_ACCELKEY_ENABLE:
		#	onClickKeyDict[app.DIK_B] = lambda: self.ChangePKMode()

		self.onClickKeyDict=onClickKeyDict

	if app.__ENABLE_NEW_OFFLINESHOP__:
		def __PressYKey(self):
			self.interface.OnPressBtnOfflineshop()

	if app.ENABLE_NEW_FISHING_SYSTEM:
		def OnFishingStart(self, have, need):
			if self.interface:
				self.interface.OnFishingStart(have, need)

		def OnFishingStop(self):
			if self.interface:
				self.interface.OnFishingStop()

		def OnFishingCatch(self, have):
			if self.interface:
				self.interface.OnFishingCatch(have)

		def OnFishingCatchFailed(self):
			if self.interface:
				self.interface.OnFishingCatchFailed()

	def __PressNumKey(self,num):
		if app.IsPressed(app.DIK_LCONTROL) or app.IsPressed(app.DIK_RCONTROL):

			if num >= 1 and num <= 9:
				if(chrmgr.IsPossibleEmoticon(-1)):
					chrmgr.SetEmoticon(-1,int(num)-1)
					net.SendEmoticon(int(num)-1)
		else:
			if num >= 1 and num <= 4:
				self.pressNumber(num-1)

	def __ClickBKey(self):
		if app.IsPressed(app.DIK_LCONTROL) or app.IsPressed(app.DIK_RCONTROL):
			return
		else:
			if constinfo.PVPMODE_ACCELKEY_ENABLE:
				self.ChangePKMode()


	def	__PressJKey(self):
		if app.IsPressed(app.DIK_LCONTROL) or app.IsPressed(app.DIK_RCONTROL):
			if player.IsMountingHorse():
				net.SendChatPacket("/unmount")
			else:
				#net.SendChatPacket("/user_horse_ride")
				if not uiprivateshopbuilder.IsBuildingPrivateShop():
					for i in xrange(player.INVENTORY_PAGE_SIZE*player.INVENTORY_PAGE_COUNT):
						if player.GetItemIndex(i) in (71114, 71116, 71118, 71120):
							net.SendItemUsePacket(i)
							break
		else:
			if self.interface:
				self.interface.Rune()

	def	__PressHKey(self):
		if app.IsPressed(app.DIK_LCONTROL) or app.IsPressed(app.DIK_RCONTROL):
			net.SendChatPacket("/ride")
		else:
			if self.interface:
				self.interface.ToggleWikiNew()

	def	__PressBKey(self):
		if app.IsPressed(app.DIK_LCONTROL) or app.IsPressed(app.DIK_RCONTROL):
			net.SendChatPacket("/user_horse_back")
		else:
			state = "EMOTICON"
			self.interface.ToggleCharacterWindow(state)

	def	__PressFKey(self):
		if app.IsPressed(app.DIK_LCONTROL) or app.IsPressed(app.DIK_RCONTROL):
			net.SendChatPacket("/user_horse_feed")
		else:
			app.ZoomCamera(app.CAMERA_TO_POSITIVE)

	def __PressGKey(self):
		if app.IsPressed(app.DIK_LCONTROL) or app.IsPressed(app.DIK_RCONTROL):
			net.SendChatPacket("/ride")
		else:
			if self.ShowNameFlag:
				self.interface.ToggleGuildWindow()
			else:
				app.PitchCamera(app.CAMERA_TO_POSITIVE)

	def	__ReleaseGKey(self):
		app.PitchCamera(app.CAMERA_STOP)

	def __PressQKey(self):
		if app.IsPressed(app.DIK_LCONTROL) or app.IsPressed(app.DIK_RCONTROL):
			if 0==interfacemodule.IsQBHide:
				interfacemodule.IsQBHide = 1
				self.interface.HideAllQuestButton()
			else:
				interfacemodule.IsQBHide = 0
				self.interface.ShowAllQuestButton()
		else:
			app.RotateCamera(app.CAMERA_TO_NEGATIVE)

	def __SetQuickSlotMode(self):
		self.pressNumber=ui.__mem_func__(self.__PressQuickSlot)

	def __SetQuickPageMode(self):
		self.pressNumber=ui.__mem_func__(self.__SelectQuickPage)

	def __PressQuickSlot(self, localSlotIndex):
		if localeinfo.IsARABIC():
			if 0 <= localSlotIndex and localSlotIndex < 4:
				player.RequestUseLocalQuickSlot(3-localSlotIndex)
			else:
				player.RequestUseLocalQuickSlot(11-localSlotIndex)
		else:
			player.RequestUseLocalQuickSlot(localSlotIndex)

	def __SelectQuickPage(self, pageIndex):
		self.quickSlotPageIndex = pageIndex
		player.SetQuickPage(pageIndex)

	def ToggleDebugInfo(self):
		self.isShowDebugInfo = not self.isShowDebugInfo

		if self.isShowDebugInfo:
			self.PrintCoord.Show()
			self.FrameRate.Show()
			self.Pitch.Show()
			self.Splat.Show()
			self.TextureNum.Show()
			self.ObjectNum.Show()
			self.ViewDistance.Show()
			self.PrintMousePos.Show()
		else:
			self.PrintCoord.Hide()
			self.FrameRate.Hide()
			self.Pitch.Hide()
			self.Splat.Hide()
			self.TextureNum.Hide()
			self.ObjectNum.Hide()
			self.ViewDistance.Hide()
			self.PrintMousePos.Hide()

	def __BuildDebugInfo(self):
		## Character Position Coordinate
		self.PrintCoord = ui.TextLine()
		self.PrintCoord.SetFontName(localeinfo.UI_DEF_FONT)
		self.PrintCoord.SetPosition(wndMgr.GetScreenWidth() - 270, 0)

		## Frame Rate
		self.FrameRate = ui.TextLine()
		self.FrameRate.SetFontName(localeinfo.UI_DEF_FONT)
		self.FrameRate.SetPosition(wndMgr.GetScreenWidth() - 270, 20)

		## Camera Pitch
		self.Pitch = ui.TextLine()
		self.Pitch.SetFontName(localeinfo.UI_DEF_FONT)
		self.Pitch.SetPosition(wndMgr.GetScreenWidth() - 270, 40)

		## Splat
		self.Splat = ui.TextLine()
		self.Splat.SetFontName(localeinfo.UI_DEF_FONT)
		self.Splat.SetPosition(wndMgr.GetScreenWidth() - 270, 60)

		##
		self.PrintMousePos = ui.TextLine()
		self.PrintMousePos.SetFontName(localeinfo.UI_DEF_FONT)
		self.PrintMousePos.SetPosition(wndMgr.GetScreenWidth() - 270, 80)

		# TextureNum
		self.TextureNum = ui.TextLine()
		self.TextureNum.SetFontName(localeinfo.UI_DEF_FONT)
		self.TextureNum.SetPosition(wndMgr.GetScreenWidth() - 270, 100)

		# 오브젝트 그리는 개수
		self.ObjectNum = ui.TextLine()
		self.ObjectNum.SetFontName(localeinfo.UI_DEF_FONT)
		self.ObjectNum.SetPosition(wndMgr.GetScreenWidth() - 270, 120)

		# 시야거리
		self.ViewDistance = ui.TextLine()
		self.ViewDistance.SetFontName(localeinfo.UI_DEF_FONT)
		self.ViewDistance.SetPosition(0, 0)

	def __NotifyError(self, msg):
		chat.AppendChat(chat.CHAT_TYPE_INFO, msg)

	def ChangePKMode(self):

		if not app.IsPressed(app.DIK_LCONTROL):
			return

		if player.GetStatus(player.LEVEL)<constinfo.PVPMODE_PROTECTED_LEVEL:
			self.__NotifyError(localeinfo.OPTION_PVPMODE_PROTECT % (constinfo.PVPMODE_PROTECTED_LEVEL))
			return

		curTime = app.GetTime()
		if curTime - self.lastPKModeSendedTime < constinfo.PVPMODE_ACCELKEY_DELAY:
			return

		self.lastPKModeSendedTime = curTime

		curPKMode = player.GetPKMode()
		nextPKMode = curPKMode + 1
		if nextPKMode == player.PK_MODE_PROTECT:
			if 0 == player.GetGuildID():
				chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.OPTION_PVPMODE_CANNOT_SET_GUILD_MODE)
				nextPKMode = 0
			else:
				nextPKMode = player.PK_MODE_GUILD

		elif nextPKMode == player.PK_MODE_MAX_NUM:
			nextPKMode = 0

		net.SendChatPacket("/PKMode " + str(nextPKMode))
		print "/PKMode " + str(nextPKMode)

	def OnChangePKMode(self):
		try:
			self.interface.OnChangePKMode()
		except:
			pass

		try:
			self.__NotifyError(localeinfo.OPTION_PVPMODE_MESSAGE_DICT[player.GetPKMode()])
		except KeyError:
			print "UNKNOWN PVPMode[%d]" % (player.GetPKMode())

		if constinfo.PVPMODE_TEST_ENABLE:
			curPKMode = player.GetPKMode()
			alignment, grade = chr.testGetPKData()
			self.pkModeNameDict = { 0 : "PEACE", 1 : "REVENGE", 2 : "FREE", 3 : "PROTECT", }
			self.testPKMode.SetText("Current PK Mode : " + self.pkModeNameDict.get(curPKMode, "UNKNOWN"))
			self.testAlignment.SetText("Current Alignment : " + str(alignment) + " (" + localeinfo.TITLE_NAME_LIST[grade] + ")")

	###############################################################################################
	###############################################################################################
	## Game Callback Functions

	# Start
	def StartGame(self):
		self.RefreshInventory()
		self.RefreshEquipment()
		self.RefreshCharacter()
		self.RefreshSkill()

	# Refresh
	def CheckGameButton(self):
		if self.interface:
			self.interface.CheckGameButton()

	def RefreshAlignment(self):
		self.interface.RefreshAlignment()

	def RefreshStatus(self):
		self.CheckGameButton()

		if self.interface:
			self.interface.RefreshStatus()

		if self.playerGauge:
			self.playerGauge.RefreshGauge()

	def RefreshStamina(self):
		self.interface.RefreshStamina()

	def RefreshSkill(self):
		self.CheckGameButton()
		if self.interface:
			self.interface.RefreshSkill()

	def RefreshQuest(self):
		self.interface.RefreshQuest()

	def RefreshMessenger(self):
		self.interface.RefreshMessenger()

	def RefreshGuildInfoPage(self):
		self.interface.RefreshGuildInfoPage()

	def RefreshGuildBoardPage(self):
		self.interface.RefreshGuildBoardPage()

	def RefreshGuildMemberPage(self):
		self.interface.RefreshGuildMemberPage()

	def RefreshGuildMemberPageGradeComboBox(self):
		self.interface.RefreshGuildMemberPageGradeComboBox()

	def RefreshGuildSkillPage(self):
		self.interface.RefreshGuildSkillPage()

	def RefreshGuildGradePage(self):
		self.interface.RefreshGuildGradePage()

	def RefreshMobile(self):
		if self.interface:
			self.interface.RefreshMobile()

	def OnMobileAuthority(self):
		self.interface.OnMobileAuthority()

	def OnBlockMode(self, mode):
		self.interface.OnBlockMode(mode)

	def OpenQuestWindow(self, skin, idx):
		self.interface.OpenQuestWindow(skin, idx)

	def AskGuildName(self):

		guildNameBoard = uicommon.InputDialog()
		guildNameBoard.SetTitle(localeinfo.GUILD_NAME)
		guildNameBoard.SetAcceptEvent(ui.__mem_func__(self.ConfirmGuildName))
		guildNameBoard.SetCancelEvent(ui.__mem_func__(self.CancelGuildName))
		guildNameBoard.Open()

		self.guildNameBoard = guildNameBoard

	def ConfirmGuildName(self):
		guildName = self.guildNameBoard.GetText()
		if not guildName:
			return

		if net.IsInsultIn(guildName):
			self.PopupMessage(localeinfo.GUILD_CREATE_ERROR_INSULT_NAME)
			return

		net.SendAnswerMakeGuildPacket(guildName)
		self.guildNameBoard.Close()
		self.guildNameBoard = None
		return True

	def CancelGuildName(self):
		self.guildNameBoard.Close()
		self.guildNameBoard = None
		return True

	## Refine
	def PopupMessage(self, msg):
		self.stream.popupWindow.Close()
		self.stream.popupWindow.Open(msg, 0, localeinfo.UI_OK)

	def OpenRefineDialog(self, targetItemPos, nextGradeItemVnum, cost, prob, type=0):
		self.interface.OpenRefineDialog(targetItemPos, nextGradeItemVnum, cost, prob, type)

	def AppendMaterialToRefineDialog(self, vnum, count):
		self.interface.AppendMaterialToRefineDialog(vnum, count)

	def RunUseSkillEvent(self, slotIndex, coolTime):
		self.interface.OnUseSkill(slotIndex, coolTime)

	def ClearAffects(self):
		self.affectShower.ClearAffects()

	def SetAffect(self, affect):
		self.affectShower.SetAffect(affect)

	def ResetAffect(self, affect):
		self.affectShower.ResetAffect(affect)

	# UNKNOWN_UPDATE
	def BINARY_NEW_AddAffect(self, type, pointIdx, value, duration):
		try:
			self.affectShower.BINARY_NEW_AddAffect(type, pointIdx, value, duration)
			if chr.NEW_AFFECT_DRAGON_SOUL_DECK1 == type or chr.NEW_AFFECT_DRAGON_SOUL_DECK2 == type:
				self.interface.DragonSoulActivate(type - chr.NEW_AFFECT_DRAGON_SOUL_DECK1)
			elif type == chr.NEW_AFFECT_BATTLE_PASS:
				self.interface.wndBattlePass.SetBattlePassInfo(value, duration)
				self.interface.wndBattlePass.SetInactiveBG(False)
				self.interface.wndBattlePass.SetStatus(False)
			elif app.ENABLE_DS_SET and type == chr.NEW_AFFECT_DS_SET and self.interface:
				self.interface.SetDSSet(value)
		except:
			pass
		#else:
		#	self.interface.wndBattlePass.SetBattlePassInfo(1, 1)		
		#	self.interface.wndBattlePass.SetInactiveBG(True)
						
	# def BINARY_NEW_RemoveAffect(self, type, pointIdx):
		# self.affectShower.BINARY_NEW_RemoveAffect(type, pointIdx)
		# if chr.NEW_AFFECT_DRAGON_SOUL_DECK1 == type or chr.NEW_AFFECT_DRAGON_SOUL_DECK2 == type:
			# self.interface.DragonSoulDeactivate()
			

	# END_OF_UNKNOWN_UPDATE


	def BINARY_NEW_RemoveAffect(self, type, pointIdx):		
		self.affectShower.BINARY_NEW_RemoveAffect(type, pointIdx)
		if chr.NEW_AFFECT_DRAGON_SOUL_DECK1 == type or chr.NEW_AFFECT_DRAGON_SOUL_DECK2 == type:
			self.interface.DragonSoulDeactivate()
		elif type == chr.NEW_AFFECT_BATTLE_PASS:
			if self.interface:
				if self.interface.wndBattlePass:
					self.interface.wndBattlePass.SetInactiveBG(True)
					self.interface.wndBattlePass.SetStatus(True)
		elif app.ENABLE_DS_SET and type == chr.NEW_AFFECT_DS_SET and self.interface:
			self.interface.SetDSSet(0)
	# END_OF_UNKNOWN_UPDATE

	def ActivateSkillSlot(self, slotIndex):
		if self.interface:
			self.interface.OnActivateSkill(slotIndex)

	def DeactivateSkillSlot(self, slotIndex):
		if self.interface:
			self.interface.OnDeactivateSkill(slotIndex)

	def RefreshEquipment(self):
		if self.interface:
			self.interface.RefreshInventory()

	def RefreshInventory(self):
		if self.interface:
			self.interface.RefreshInventory()

	def RefreshCharacter(self):
		if self.interface:
			self.interface.RefreshCharacter()

	def OnGameOver(self):
		self.CloseTargetBoard()
		self.OpenRestartDialog()

	def OpenRestartDialog(self):
		self.interface.OpenRestartDialog()

	def ChangeCurrentSkill(self, skillSlotNumber):
		self.interface.OnChangeCurrentSkill(skillSlotNumber)

	## TargetBoard
	def SetPCTargetBoard(self, vid, name):
		self.targetBoard.Open(vid, name)

		if app.IsPressed(app.DIK_LCONTROL):

			if not player.IsSameEmpire(vid):
				return

			if player.IsMainCharacterIndex(vid):
				return
			elif chr.INSTANCE_TYPE_BUILDING == chr.GetInstanceType(vid):
				return

			self.interface.OpenWhisperDialog(name)


	def RefreshTargetBoardByVID(self, vid):
		self.targetBoard.RefreshByVID(vid)

	def RefreshTargetBoardByName(self, name):
		self.targetBoard.RefreshByName(name)

	def __RefreshTargetBoard(self):
		self.targetBoard.Refresh()

	
	if app.ENABLE_VIEW_TARGET_DECIMAL_HP:
		if app.ENABLE_VIEW_ELEMENT:
			def SetHPTargetBoard(self, vid, hpPercentage, iMinHP, iMaxHP, bElement):
				if vid != self.targetBoard.GetTargetVID():
					self.targetBoard.ResetTargetBoard()
					self.targetBoard.SetEnemyVID(vid)
				
				self.targetBoard.SetHP(hpPercentage, iMinHP, iMaxHP)
				self.targetBoard.SetElementImage(bElement)
				self.targetBoard.Show()
		else:
			def SetHPTargetBoard(self, vid, hpPercentage, iMinHP, iMaxHP):
				if vid != self.targetBoard.GetTargetVID():
					self.targetBoard.ResetTargetBoard()
					self.targetBoard.SetEnemyVID(vid)

				self.targetBoard.SetHP(hpPercentage, iMinHP, iMaxHP)
				self.targetBoard.Show()
	else:
		if app.ENABLE_VIEW_ELEMENT:
			def SetHPTargetBoard(self, vid, hpPercentage, bElement):
				if vid != self.targetBoard.GetTargetVID():
					self.targetBoard.ResetTargetBoard()
					self.targetBoard.SetEnemyVID(vid)
				
				self.targetBoard.SetHP(hpPercentage)
				self.targetBoard.SetElementImage(bElement)
				self.targetBoard.Show()
		else:
			def SetHPTargetBoard(self, vid, hpPercentage):
				if vid != self.targetBoard.GetTargetVID():
					self.targetBoard.ResetTargetBoard()
					self.targetBoard.SetEnemyVID(vid)
				
				self.targetBoard.SetHP(hpPercentage)
				self.targetBoard.Show()

	def CloseTargetBoardIfDifferent(self, vid):
		if vid != self.targetBoard.GetTargetVID():
			self.targetBoard.Close()

	def CloseTargetBoard(self):
		self.targetBoard.Close()

	## View Equipment
	def OpenEquipmentDialog(self, vid):
		if app.ENABLE_PVP_ADVANCED:
			pvp.DUEL_IS_SHOW_EQUIP = 0
			pvp.DUEL_SAVE_VID = (int(vid))

		self.interface.OpenEquipmentDialog(vid)
		

	def SetEquipmentDialogItem(self, vid, slotIndex, vnum, count):
		self.interface.SetEquipmentDialogItem(vid, slotIndex, vnum, count)

	def SetEquipmentDialogSocket(self, vid, slotIndex, socketIndex, value):
		self.interface.SetEquipmentDialogSocket(vid, slotIndex, socketIndex, value)

	def SetEquipmentDialogAttr(self, vid, slotIndex, attrIndex, type, value):
		self.interface.SetEquipmentDialogAttr(vid, slotIndex, attrIndex, type, value)

	def BINARY_OpenAtlasWindow(self):
		self.interface.BINARY_OpenAtlasWindow()

	## Chat
	def OnRecvWhisper(self, mode, name, line):
		if mode == chat.WHISPER_TYPE_GM:
			self.interface.RegisterGameMasterName(name)
		
		chat.AppendWhisper(mode, name, line)
		self.interface.RecvWhisper(name)

	def OnRecvWhisperSystemMessage(self, mode, name, line):
		chat.AppendWhisper(chat.WHISPER_TYPE_SYSTEM, name, line)
		self.interface.RecvWhisper(name)

	def OnRecvWhisperError(self, mode, name, line):
		if localeinfo.WHISPER_ERROR.has_key(mode):
			chat.AppendWhisper(chat.WHISPER_TYPE_SYSTEM, name, localeinfo.WHISPER_ERROR[mode](name))
		else:
			chat.AppendWhisper(chat.WHISPER_TYPE_SYSTEM, name, "Whisper Unknown Error(mode=%d, name=%s)" % (mode, name))
		self.interface.RecvWhisper(name)

	def RecvWhisper(self, name):
		self.interface.RecvWhisper(name)

	def OnPickMoney(self, money):
		self.interface.OnPickMoneyNew(money)

	def OnShopError(self, type):
		try:
			self.PopupMessage(localeinfo.SHOP_ERROR_DICT[type])
		except KeyError:
			self.PopupMessage(localeinfo.SHOP_ERROR_UNKNOWN % (type))

	def OnSafeBoxError(self):
		self.PopupMessage(localeinfo.SAFEBOX_ERROR)

	def OnFishingSuccess(self, isFish, fishName):
		chat.AppendChatWithDelay(chat.CHAT_TYPE_INFO, localeinfo.FISHING_SUCCESS(isFish, fishName), 2000)

	# ADD_FISHING_MESSAGE
	def OnFishingNotifyUnknown(self):
		chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.FISHING_UNKNOWN)

	def OnFishingWrongPlace(self):
		chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.FISHING_WRONG_PLACE)
	# END_OF_ADD_FISHING_MESSAGE

	def OnFishingNotify(self, isFish, fishName):
		chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.FISHING_NOTIFY(isFish, fishName))

	def OnFishingFailure(self):
		chat.AppendChatWithDelay(chat.CHAT_TYPE_INFO, localeinfo.FISHING_FAILURE, 2000)

	def OnCannotPickItem(self):
		chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.GAME_CANNOT_PICK_ITEM)

	# MINING
	def OnCannotMining(self):
		chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.GAME_CANNOT_MINING)
	# END_OF_MINING

	def OnCannotUseSkill(self, vid, type):
		if localeinfo.USE_SKILL_ERROR_TAIL_DICT.has_key(type):
			textTail.RegisterInfoTail(vid, localeinfo.USE_SKILL_ERROR_TAIL_DICT[type])

		if localeinfo.USE_SKILL_ERROR_CHAT_DICT.has_key(type):
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.USE_SKILL_ERROR_CHAT_DICT[type])

	def	OnCannotShotError(self, vid, type):
		textTail.RegisterInfoTail(vid, localeinfo.SHOT_ERROR_TAIL_DICT.get(type, localeinfo.SHOT_ERROR_UNKNOWN % (type)))

	## PointReset
	def StartPointReset(self):
		self.interface.OpenPointResetDialog()

	## Shop
	def StartShop(self, vid):
		self.interface.OpenShopDialog(vid)
		
		if app.__ENABLE_EXTEND_INVEN_SYSTEM__:
			self.RefreshInventory()

	def EndShop(self):
		self.interface.CloseShopDialog()
		
		if app.__ENABLE_EXTEND_INVEN_SYSTEM__:
			self.RefreshInventory()

	def RefreshShop(self):
		self.interface.RefreshShopDialog()

	def SetShopSellingPrice(self, Price):
		pass

	## Exchange
	def StartExchange(self):
		self.interface.StartExchange()

	def EndExchange(self):
		self.interface.EndExchange()

	def RefreshExchange(self):
		self.interface.RefreshExchange()

	## Party
	def RecvPartyInviteQuestion(self, leaderVID, leaderName):
		partyInviteQuestionDialog = uicommon.QuestionDialog()
		partyInviteQuestionDialog.SetText(leaderName + localeinfo.PARTY_DO_YOU_JOIN)
		partyInviteQuestionDialog.SetAcceptEvent(lambda arg=True: self.AnswerPartyInvite(arg))
		partyInviteQuestionDialog.SetCancelEvent(lambda arg=False: self.AnswerPartyInvite(arg))
		partyInviteQuestionDialog.Open()
		partyInviteQuestionDialog.partyLeaderVID = leaderVID
		self.partyInviteQuestionDialog = partyInviteQuestionDialog

	def AnswerPartyInvite(self, answer):

		if not self.partyInviteQuestionDialog:
			return

		partyLeaderVID = self.partyInviteQuestionDialog.partyLeaderVID

		distance = player.GetCharacterDistance(partyLeaderVID)
		if distance < 0.0 or distance > 5000:
			answer = False

		net.SendPartyInviteAnswerPacket(partyLeaderVID, answer)

		self.partyInviteQuestionDialog.Close()
		self.partyInviteQuestionDialog = None

	def AddPartyMember(self, pid, name):
		self.interface.AddPartyMember(pid, name)

	def UpdatePartyMemberInfo(self, pid):
		self.interface.UpdatePartyMemberInfo(pid)

	def RemovePartyMember(self, pid):
		self.interface.RemovePartyMember(pid)
		self.__RefreshTargetBoard()

	def LinkPartyMember(self, pid, vid):
		self.interface.LinkPartyMember(pid, vid)

	def UnlinkPartyMember(self, pid):
		self.interface.UnlinkPartyMember(pid)

	def UnlinkAllPartyMember(self):
		self.interface.UnlinkAllPartyMember()

	def ExitParty(self):
		self.interface.ExitParty()
		self.RefreshTargetBoardByVID(self.targetBoard.GetTargetVID())

	def ChangePartyParameter(self, distributionMode):
		self.interface.ChangePartyParameter(distributionMode)

	## Messenger
	def OnMessengerAddFriendQuestion(self, name):
		messengerAddFriendQuestion = uicommon.QuestionDialog2()
		messengerAddFriendQuestion.SetText1(localeinfo.MESSENGER_DO_YOU_ACCEPT_ADD_FRIEND_1 % (name))
		messengerAddFriendQuestion.SetText2(localeinfo.MESSENGER_DO_YOU_ACCEPT_ADD_FRIEND_2)
		messengerAddFriendQuestion.SetAcceptEvent(ui.__mem_func__(self.OnAcceptAddFriend))
		messengerAddFriendQuestion.SetCancelEvent(ui.__mem_func__(self.OnDenyAddFriend))
		messengerAddFriendQuestion.Open()
		messengerAddFriendQuestion.name = name
		self.messengerAddFriendQuestion = messengerAddFriendQuestion

	def OnAcceptAddFriend(self):
		name = self.messengerAddFriendQuestion.name
		net.SendChatPacket("/messenger_auth y " + name)
		self.OnCloseAddFriendQuestionDialog()
		return True

	def OnDenyAddFriend(self):
		name = self.messengerAddFriendQuestion.name
		net.SendChatPacket("/messenger_auth n " + name)
		self.OnCloseAddFriendQuestionDialog()
		return True

	def OnCloseAddFriendQuestionDialog(self):
		self.messengerAddFriendQuestion.Close()
		self.messengerAddFriendQuestion = None
		return True

	## SafeBox
	def OpenSafeboxWindow(self, size):
		self.interface.OpenSafeboxWindow(size)

	def RefreshSafebox(self):
		self.interface.RefreshSafebox()

	def RefreshSafeboxMoney(self):
		self.interface.RefreshSafeboxMoney()

	# ITEM_MALL
	def OpenMallWindow(self, size):
		self.interface.OpenMallWindow(size)

	def RefreshMall(self):
		self.interface.RefreshMall()
	# END_OF_ITEM_MALL

	## Guild
	def RecvGuildInviteQuestion(self, guildID, guildName):
		guildInviteQuestionDialog = uicommon.QuestionDialog()
		guildInviteQuestionDialog.SetText(guildName + localeinfo.GUILD_DO_YOU_JOIN)
		guildInviteQuestionDialog.SetAcceptEvent(lambda arg=True: self.AnswerGuildInvite(arg))
		guildInviteQuestionDialog.SetCancelEvent(lambda arg=False: self.AnswerGuildInvite(arg))
		guildInviteQuestionDialog.Open()
		guildInviteQuestionDialog.guildID = guildID
		self.guildInviteQuestionDialog = guildInviteQuestionDialog

	def AnswerGuildInvite(self, answer):

		if not self.guildInviteQuestionDialog:
			return

		guildLeaderVID = self.guildInviteQuestionDialog.guildID
		net.SendGuildInviteAnswerPacket(guildLeaderVID, answer)

		self.guildInviteQuestionDialog.Close()
		self.guildInviteQuestionDialog = None


	def DeleteGuild(self):
		self.interface.DeleteGuild()

	## Clock
	def ShowClock(self, second):
		self.interface.ShowClock(second)

	def HideClock(self):
		self.interface.HideClock()

	## emotion
	def BINARY_ActEmotion(self, emotionIndex):
		if self.interface.wndCharacter:
			self.interface.wndCharacter.ActEmotion(emotionIndex)

	###############################################################################################
	###############################################################################################
	## Keyboard Functions

	def CheckFocus(self):
		if False == self.IsFocus():
			if True == self.interface.IsOpenChat():
				self.interface.ToggleChat()

			self.SetFocus()


	def ShowConsole(self):
		if debuginfo.IsDebugMode() or True == self.consoleEnable:
			player.EndKeyWalkingImmediately()
			self.console.OpenWindow()

	def ShowName(self):
		self.ShowNameFlag = True
		if app.__ENABLE_NEW_OFFLINESHOP__:
			offlineshop.ShowShopNames()

		self.playerGauge.EnableShowAlways()
		player.SetQuickPage(self.quickSlotPageIndex+1)

	# ADD_ALWAYS_SHOW_NAME
	def __IsShowName(self):

		if systemSetting.IsAlwaysShowName():
			return True

		if self.ShowNameFlag:
			return True

		return False
	# END_OF_ADD_ALWAYS_SHOW_NAME

	def HideName(self):
		self.ShowNameFlag = False
		self.playerGauge.DisableShowAlways()
		player.SetQuickPage(self.quickSlotPageIndex)
		if app.__ENABLE_NEW_OFFLINESHOP__:
			offlineshop.HideShopNames()


	def ShowMouseImage(self):
		self.interface.ShowMouseImage()

	def HideMouseImage(self):
		self.interface.HideMouseImage()

	def StartAttack(self):
		player.SetAttackKeyState(True)

	def EndAttack(self):
		player.SetAttackKeyState(False)

	def MoveUp(self):
		player.SetSingleDIKKeyState(app.DIK_UP, True)

	def MoveDown(self):
		player.SetSingleDIKKeyState(app.DIK_DOWN, True)

	def MoveLeft(self):
		player.SetSingleDIKKeyState(app.DIK_LEFT, True)

	def MoveRight(self):
		player.SetSingleDIKKeyState(app.DIK_RIGHT, True)

	def StopUp(self):
		player.SetSingleDIKKeyState(app.DIK_UP, False)

	def StopDown(self):
		player.SetSingleDIKKeyState(app.DIK_DOWN, False)

	def StopLeft(self):
		player.SetSingleDIKKeyState(app.DIK_LEFT, False)

	def StopRight(self):
		player.SetSingleDIKKeyState(app.DIK_RIGHT, False)

	def PickUpItem(self):
		player.PickCloseItem()

	def PickUpMoney(self):
		chat.AppendChat(chat.CHAT_TYPE_INFO, "asd")

	###############################################################################################
	###############################################################################################
	## Event Handler

	def OnKeyDown(self, key):
		if self.interface.wndWeb and self.interface.wndWeb.IsShow():
			return

		if key == app.DIK_ESC:
			self.RequestDropItem(False)

		try:
			self.onPressKeyDict[key]()
		except KeyError:
			pass
		except:
			raise
		
		return True

	def OnKeyUp(self, key):
		if key == None:
			return
		
		try:
			self.onClickKeyDict[key]()
		except:
			pass
		
		return True

	def OnMouseLeftButtonDown(self):
		if self.interface.BUILD_OnMouseLeftButtonDown():
			return

		if mousemodule.mouseController.isAttached():
			self.CheckFocus()
		else:
			hyperlink = ui.GetHyperlink()
			if hyperlink:
				return
			else:
				self.CheckFocus()
				player.SetMouseState(player.MBT_LEFT, player.MBS_PRESS);

		return True

	def OnMouseLeftButtonUp(self):

		if self.interface.BUILD_OnMouseLeftButtonUp():
			return

		if mousemodule.mouseController.isAttached():

			attachedType = mousemodule.mouseController.GetAttachedType()
			attachedItemIndex = mousemodule.mouseController.GetAttachedItemIndex()
			attachedItemSlotPos = mousemodule.mouseController.GetAttachedSlotNumber()
			attachedItemCount = mousemodule.mouseController.GetAttachedItemCount()
			if app.__ENABLE_NEW_OFFLINESHOP__:
				if uiofflineshop.IsBuildingShop() and uiofflineshop.IsSaleSlot(player.SlotTypeToInvenType(attachedType), attachedItemSlotPos): #toupdate
					chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.OFFLINESHOP_CANT_SELECT_ITEM_DURING_BUILING)
					return



			## QuickSlot
			if player.SLOT_TYPE_QUICK_SLOT == attachedType:
				player.RequestDeleteGlobalQuickSlot(attachedItemSlotPos)

			## Inventory
			elif player.SLOT_TYPE_INVENTORY == attachedType:

				if player.ITEM_MONEY == attachedItemIndex:
					self.__PutMoney(attachedType, attachedItemCount, self.PickingCharacterIndex)
				else:
					self.__PutItem(attachedType, attachedItemIndex, attachedItemSlotPos, attachedItemCount, self.PickingCharacterIndex)

			## DragonSoul
			elif player.SLOT_TYPE_DRAGON_SOUL_INVENTORY == attachedType:
				self.__PutItem(attachedType, attachedItemIndex, attachedItemSlotPos, attachedItemCount, self.PickingCharacterIndex)


			## ExtraInventory
			if app.ENABLE_EXTRA_INVENTORY:
				if attachedType == player.SLOT_TYPE_EXTRA_INVENTORY:
					self.__PutItem(attachedType, attachedItemIndex, attachedItemSlotPos, attachedItemCount, self.PickingCharacterIndex)



			mousemodule.mouseController.DeattachObject()

		else:
			hyperlink = ui.GetHyperlink()
			if hyperlink:
				if app.IsPressed(app.DIK_LALT):
					link = chat.GetLinkFromHyperlink(hyperlink)
					ime.PasteString(link)
				else:
					self.interface.MakeHyperlinkTooltip(hyperlink)
				return
			else:
				player.SetMouseState(player.MBT_LEFT, player.MBS_CLICK)

		#player.EndMouseWalking()
		return True

	def __PutItem(self, attachedType, attachedItemIndex, attachedItemSlotPos, attachedItemCount, dstChrID):
	
	
		if app.ENABLE_EXTRA_INVENTORY:
			if player.SLOT_TYPE_INVENTORY == attachedType or player.SLOT_TYPE_DRAGON_SOUL_INVENTORY == attachedType or player.SLOT_TYPE_EXTRA_INVENTORY == attachedType:
				attachedInvenType = player.SlotTypeToInvenType(attachedType)
				if True == chr.HasInstance(self.PickingCharacterIndex) and player.GetMainCharacterIndex() != dstChrID:
					if player.IsEquipmentSlot(attachedItemSlotPos) and player.SLOT_TYPE_DRAGON_SOUL_INVENTORY != attachedType and player.SLOT_TYPE_EXTRA_INVENTORY != attachedType:
						self.stream.popupWindow.Close()
						self.stream.popupWindow.Open(localeinfo.EXCHANGE_FAILURE_EQUIP_ITEM, 0, localeinfo.UI_OK)
					else:
						if chr.IsNPC(dstChrID):
							constinfo.AUTO_REFINE_TYPE = 2
							constinfo.AUTO_REFINE_DATA["NPC"][0] = dstChrID
							constinfo.AUTO_REFINE_DATA["NPC"][1] = attachedInvenType
							constinfo.AUTO_REFINE_DATA["NPC"][2] = attachedItemSlotPos
							constinfo.AUTO_REFINE_DATA["NPC"][3] = attachedItemCount

							net.SendGiveItemPacket(dstChrID, attachedInvenType, attachedItemSlotPos, attachedItemCount)
						else:
							if app.ENABLE_MELEY_LAIR_DUNGEON:
								if chr.IsStone(dstChrID):
									net.SendGiveItemPacket(dstChrID, attachedInvenType, attachedItemSlotPos, attachedItemCount)
								else:
									net.SendExchangeStartPacket(dstChrID)
									net.SendExchangeItemAddPacket(attachedInvenType, attachedItemSlotPos, 0)
							else:
								net.SendExchangeStartPacket(dstChrID)
								net.SendExchangeItemAddPacket(attachedInvenType, attachedItemSlotPos, 0)
				else:
					self.__DropItem(attachedType, attachedItemIndex, attachedItemSlotPos, attachedItemCount)
		else:
			if player.SLOT_TYPE_INVENTORY == attachedType or player.SLOT_TYPE_DRAGON_SOUL_INVENTORY == attachedType:
				attachedInvenType = player.SlotTypeToInvenType(attachedType)
				if True == chr.HasInstance(self.PickingCharacterIndex) and player.GetMainCharacterIndex() != dstChrID:
					if player.IsEquipmentSlot(attachedItemSlotPos) and player.SLOT_TYPE_DRAGON_SOUL_INVENTORY != attachedType:
						self.stream.popupWindow.Close()
						self.stream.popupWindow.Open(localeinfo.EXCHANGE_FAILURE_EQUIP_ITEM, 0, localeinfo.UI_OK)
					else:
						if chr.IsNPC(dstChrID):
							net.SendGiveItemPacket(dstChrID, attachedInvenType, attachedItemSlotPos, attachedItemCount)
						else:
							if app.ENABLE_MELEY_LAIR_DUNGEON:
								if chr.IsStone(dstChrID):
									net.SendGiveItemPacket(dstChrID, attachedInvenType, attachedItemSlotPos, attachedItemCount)
								else:
									net.SendExchangeStartPacket(dstChrID)
									net.SendExchangeItemAddPacket(attachedInvenType, attachedItemSlotPos, 0)
							else:
								net.SendExchangeStartPacket(dstChrID)
								net.SendExchangeItemAddPacket(attachedInvenType, attachedItemSlotPos, 0)
				else:
					self.__DropItem(attachedType, attachedItemIndex, attachedItemSlotPos, attachedItemCount)

	def __PutMoney(self, attachedType, attachedMoney, dstChrID):
		if True == chr.HasInstance(dstChrID) and player.GetMainCharacterIndex() != dstChrID:
			net.SendExchangeStartPacket(dstChrID)
			net.SendExchangeElkAddPacket(attachedMoney)
		else:
			self.__DropMoney(attachedType, attachedMoney)

	def __DropMoney(self, attachedType, attachedMoney):
		# PRIVATESHOP_DISABLE_ITEM_DROP - 개인상점 열고 있는 동안 아이템 버림 방지
		if uiprivateshopbuilder.IsBuildingPrivateShop():
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.DROP_ITEM_FAILURE_PRIVATE_SHOP)
			return
		# END_OF_PRIVATESHOP_DISABLE_ITEM_DROP

		if attachedMoney>=1000:
			self.stream.popupWindow.Close()
			self.stream.popupWindow.Open(localeinfo.DROP_MONEY_FAILURE_1000_OVER, 0, localeinfo.UI_OK)
			return

		itemDropQuestionDialog = uicommon.QuestionDialog()
		itemDropQuestionDialog.SetText(localeinfo.DO_YOU_DROP_MONEY % (attachedMoney))
		itemDropQuestionDialog.SetAcceptEvent(lambda arg=True: self.RequestDropItem(arg))
		itemDropQuestionDialog.SetCancelEvent(lambda arg=False: self.RequestDropItem(arg))
		itemDropQuestionDialog.Open()
		itemDropQuestionDialog.dropType = attachedType
		itemDropQuestionDialog.dropCount = attachedMoney
		itemDropQuestionDialog.dropNumber = player.ITEM_MONEY
		self.itemDropQuestionDialog = itemDropQuestionDialog

	def __SendDestroyItemPacket(self, itemSlotPos, itemInvenType = player.INVENTORY):
		if uiprivateshopbuilder.IsBuildingPrivateShop():
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.DROP_ITEM_FAILURE_PRIVATE_SHOP)
			return
		
		net.SendItemDestroyPacket(itemSlotPos, itemInvenType)

	def RequestDestroyItem(self, answer):
		if not self.itemDropQuestionDialog:
			return
		
		if answer:
			dropType = self.itemDropQuestionDialog.dropType
			dropNumber = self.itemDropQuestionDialog.dropNumber
			if player.SLOT_TYPE_INVENTORY == dropType:
				self.__SendDestroyItemPacket(dropNumber, player.INVENTORY)
			elif app.ENABLE_EXTRA_INVENTORY and player.SLOT_TYPE_EXTRA_INVENTORY == dropType:
				self.__SendDestroyItemPacket(dropNumber, player.EXTRA_INVENTORY)
		
		self.itemDropQuestionDialog.Close()
		self.itemDropQuestionDialog = None
		constinfo.SET_ITEM_QUESTION_DIALOG_STATUS(0)

	def __DropItem(self, attachedType, attachedItemIndex, attachedItemSlotPos, attachedItemCount):
		# PRIVATESHOP_DISABLE_ITEM_DROP - 개인상점 열고 있는 동안 아이템 버림 방지
		if uiprivateshopbuilder.IsBuildingPrivateShop():
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.DROP_ITEM_FAILURE_PRIVATE_SHOP)
			return
		# END_OF_PRIVATESHOP_DISABLE_ITEM_DROP

		if player.SLOT_TYPE_INVENTORY == attachedType and player.IsEquipmentSlot(attachedItemSlotPos):
			self.stream.popupWindow.Close()
			self.stream.popupWindow.Open(localeinfo.DROP_ITEM_FAILURE_EQUIP_ITEM, 0, localeinfo.UI_OK)

		else:
			if player.SLOT_TYPE_INVENTORY == attachedType:
				dropItemIndex = player.GetItemIndex(attachedItemSlotPos)

				item.SelectItem(dropItemIndex)
				dropItemName = item.GetItemName()

				## Question Text
				questionText = localeinfo.HOW_MANY_ITEM_DO_YOU_DROP(dropItemName, attachedItemCount)

				## Dialog
				itemDropQuestionDialog = uicommon.QuestionDialogItem()
				itemDropQuestionDialog.SetText(questionText)
				itemDropQuestionDialog.SetAcceptEvent(lambda arg=True: self.RequestDropItem(arg))
				itemDropQuestionDialog.SetDestroyEvent(lambda arg=True: self.RequestDestroyItem(arg))
				itemDropQuestionDialog.SetCancelEvent(lambda arg=False: self.RequestDropItem(arg))
				itemDropQuestionDialog.Open()
				itemDropQuestionDialog.dropType = attachedType
				itemDropQuestionDialog.dropNumber = attachedItemSlotPos
				itemDropQuestionDialog.dropCount = attachedItemCount
				self.itemDropQuestionDialog = itemDropQuestionDialog

				constinfo.SET_ITEM_QUESTION_DIALOG_STATUS(1)
			elif player.SLOT_TYPE_DRAGON_SOUL_INVENTORY == attachedType:
				dropItemIndex = player.GetItemIndex(player.DRAGON_SOUL_INVENTORY, attachedItemSlotPos)

				item.SelectItem(dropItemIndex)
				dropItemName = item.GetItemName()

				## Question Text
				questionText = localeinfo.HOW_MANY_ITEM_DO_YOU_DROP(dropItemName, attachedItemCount)

				## Dialog
				itemDropQuestionDialog = uicommon.QuestionDialog()
				itemDropQuestionDialog.SetText(questionText)
				itemDropQuestionDialog.SetAcceptEvent(lambda arg=True: self.RequestDropItem(arg))
				itemDropQuestionDialog.SetCancelEvent(lambda arg=False: self.RequestDropItem(arg))
				itemDropQuestionDialog.Open()
				itemDropQuestionDialog.dropType = attachedType
				itemDropQuestionDialog.dropNumber = attachedItemSlotPos
				itemDropQuestionDialog.dropCount = attachedItemCount
				self.itemDropQuestionDialog = itemDropQuestionDialog

				constinfo.SET_ITEM_QUESTION_DIALOG_STATUS(1)

			elif app.ENABLE_EXTRA_INVENTORY and player.SLOT_TYPE_EXTRA_INVENTORY == attachedType:
				dropItemIndex = player.GetItemIndex(player.EXTRA_INVENTORY, attachedItemSlotPos)
				
				item.SelectItem(dropItemIndex)
				dropItemName = item.GetItemName()
				questionText = localeinfo.HOW_MANY_ITEM_DO_YOU_DROP(dropItemName, attachedItemCount)
				
				itemDropQuestionDialog = uicommon.QuestionDialogItem()
				itemDropQuestionDialog.SetText(questionText)
				itemDropQuestionDialog.SetAcceptEvent(lambda arg=True: self.RequestDropItem(arg))
				itemDropQuestionDialog.SetDestroyEvent(lambda arg=True: self.RequestDestroyItem(arg))
				itemDropQuestionDialog.SetCancelEvent(lambda arg=False: self.RequestDropItem(arg))
				itemDropQuestionDialog.Open()
				itemDropQuestionDialog.dropType = attachedType
				itemDropQuestionDialog.dropNumber = attachedItemSlotPos
				itemDropQuestionDialog.dropCount = attachedItemCount
				self.itemDropQuestionDialog = itemDropQuestionDialog
				
				constinfo.SET_ITEM_QUESTION_DIALOG_STATUS(1)

	def RequestDropItem(self, answer):
		if not self.itemDropQuestionDialog:
			return

		if answer:
			dropType = self.itemDropQuestionDialog.dropType
			dropCount = self.itemDropQuestionDialog.dropCount
			dropNumber = self.itemDropQuestionDialog.dropNumber

			if player.SLOT_TYPE_INVENTORY == dropType:
				if dropNumber == player.ITEM_MONEY:
					net.SendGoldDropPacketNew(dropCount)
					snd.PlaySound("sound/ui/money.wav")
				else:
					# PRIVATESHOP_DISABLE_ITEM_DROP
					self.__SendDropItemPacket(dropNumber, dropCount)
					# END_OF_PRIVATESHOP_DISABLE_ITEM_DROP
			elif player.SLOT_TYPE_DRAGON_SOUL_INVENTORY == dropType:
					# PRIVATESHOP_DISABLE_ITEM_DROP
					self.__SendDropItemPacket(dropNumber, dropCount, player.DRAGON_SOUL_INVENTORY)
					# END_OF_PRIVATESHOP_DISABLE_ITEM_DROP
			if app.ENABLE_EXTRA_INVENTORY:
				if player.SLOT_TYPE_EXTRA_INVENTORY == dropType:
					self.__SendDropItemPacket(dropNumber, dropCount, player.EXTRA_INVENTORY)

		self.itemDropQuestionDialog.Close()
		self.itemDropQuestionDialog = None

		constinfo.SET_ITEM_QUESTION_DIALOG_STATUS(0)

	# PRIVATESHOP_DISABLE_ITEM_DROP
	def __SendDropItemPacket(self, itemVNum, itemCount, itemInvenType = player.INVENTORY):
		if uiprivateshopbuilder.IsBuildingPrivateShop():
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.DROP_ITEM_FAILURE_PRIVATE_SHOP)
			return

		net.SendItemDropPacketNew(itemInvenType, itemVNum, itemCount)
	# END_OF_PRIVATESHOP_DISABLE_ITEM_DROP

	def OnMouseRightButtonDown(self):

		self.CheckFocus()

		if True == mousemodule.mouseController.isAttached():
			mousemodule.mouseController.DeattachObject()

		else:
			player.SetMouseState(player.MBT_RIGHT, player.MBS_PRESS)

		return True

	def OnMouseRightButtonUp(self):
		if True == mousemodule.mouseController.isAttached():
			return True

		player.SetMouseState(player.MBT_RIGHT, player.MBS_CLICK)
		return True

	def OnMouseMiddleButtonDown(self):
		player.SetMouseMiddleButtonState(player.MBS_PRESS)

	def OnMouseMiddleButtonUp(self):
		player.SetMouseMiddleButtonState(player.MBS_CLICK)

	def OnUpdate(self):
		app.UpdateGame()

		if app.ENABLE_PVP_ADVANCED:
			if pvp.DUEL_IS_SHOW_EQUIP == 1:
				self.interface.CloseEquipmentDialog(int(pvp.DUEL_SAVE_VID))

		#if self.mapNameShower.IsShow():
		#	self.mapNameShower.Update()

		if self.isShowDebugInfo:
			self.UpdateDebugInfo()
		if app.AUTO_SHOUT:
			if constinfo.auto_shout_status == 1:
				if self.shouttime <= app.GetTime():
					self.shouttime = app.GetTime()+15
					net.SendChatPacket(str(constinfo.auto_shout_text),chat.CHAT_TYPE_SHOUT)


		if self.enableXMasBoom:
			self.__XMasBoom_Update()
		self.wnddailygift.OnUpdate()
		self.interface.BUILD_OnUpdate()
		
		if app.KASMIR_PAKET_SYSTEM:
			if marek38.VERI_PAKETI1==1:
				self.__PrivateShop_Kasmir_Open()
				marek38.VERI_PAKETI1=0
		
		# if 0 == constinfo.auto_pick_yang:
		#self.PickUpMoney()
		if app.ENABLE_BIOLOGIST_UI:
			if constinfo.haveBiologist == 1:
				t = constinfo.remainBiologistTime - app.GetGlobalTimeStamp()
				if t <= 0:
					if systemSetting.GetBiologistAlert() == True:
						name = player.GetName()
						if not name in constinfo.notifiedBiologist:
							constinfo.notifiedBiologist.append(name)
							if self.biologistAlert and not self.biologistAlert.IsShow():
								self.biologistAlert.Open()

	def UpdateDebugInfo(self):
		#
		# 캐릭터 좌표 및 FPS 출력
		(x, y, z) = player.GetMainCharacterPosition()
		nUpdateTime = app.GetUpdateTime()
		nUpdateFPS = app.GetUpdateFPS()
		nRenderFPS = app.GetRenderFPS()
		nFaceCount = app.GetFaceCount()
		fFaceSpeed = app.GetFaceSpeed()
		nST=background.GetRenderShadowTime()
		(fAveRT, nCurRT) =  app.GetRenderTime()
		(iNum, fFogStart, fFogEnd, fFarCilp) = background.GetDistanceSetInfo()
		(iPatch, iSplat, fSplatRatio, sTextureNum) = background.GetRenderedSplatNum()
		if iPatch == 0:
			iPatch = 1

		#(dwRenderedThing, dwRenderedCRC) = background.GetRenderedGraphicThingInstanceNum()

		self.PrintCoord.SetText("Coordinate: %.2f %.2f %.2f ATM: %d" % (x, y, z, app.GetAvailableTextureMemory()/(1024*1024)))
		xMouse, yMouse = wndMgr.GetMousePosition()
		self.PrintMousePos.SetText("MousePosition: %d %d" % (xMouse, yMouse))

		self.FrameRate.SetText("UFPS: %3d UT: %3d FS %.2f" % (nUpdateFPS, nUpdateTime, fFaceSpeed))

		if fAveRT>1.0:
			self.Pitch.SetText("RFPS: %3d RT:%.2f(%3d) FC: %d(%.2f) " % (nRenderFPS, fAveRT, nCurRT, nFaceCount, nFaceCount/fAveRT))

		self.Splat.SetText("PATCH: %d SPLAT: %d BAD(%.2f)" % (iPatch, iSplat, fSplatRatio))
		#self.Pitch.SetText("Pitch: %.2f" % (app.GetCameraPitch())
		#self.TextureNum.SetText("TN : %s" % (sTextureNum))
		#self.ObjectNum.SetText("GTI : %d, CRC : %d" % (dwRenderedThing, dwRenderedCRC))
		self.ViewDistance.SetText("Num : %d, FS : %f, FE : %f, FC : %f" % (iNum, fFogStart, fFogEnd, fFarCilp))

	def OnRender(self):
		app.RenderGame()

		if self.console.Console.collision:
			background.RenderCollision()
			chr.RenderCollision()

		(x, y) = app.GetCursorPosition()

		########################
		# Picking
		########################
		textTail.UpdateAllTextTail()

		if True == wndMgr.IsPickedWindow(self.hWnd):

			self.PickingCharacterIndex = chr.Pick()

			if -1 != self.PickingCharacterIndex:
				textTail.ShowCharacterTextTail(self.PickingCharacterIndex)
			if 0 != self.targetBoard.GetTargetVID():
				textTail.ShowCharacterTextTail(self.targetBoard.GetTargetVID())

			# ADD_ALWAYS_SHOW_NAME
			if not self.__IsShowName():
				self.PickingItemIndex = item.Pick()
				if -1 != self.PickingItemIndex:
					textTail.ShowItemTextTail(self.PickingItemIndex)
			# END_OF_ADD_ALWAYS_SHOW_NAME

		## Show all name in the range

		# ADD_ALWAYS_SHOW_NAME
		if self.__IsShowName():
			textTail.ShowAllTextTail()
			self.PickingItemIndex = textTail.Pick(x, y)
		# END_OF_ADD_ALWAYS_SHOW_NAME

		textTail.UpdateShowingTextTail()
		textTail.ArrangeTextTail()
		if -1 != self.PickingItemIndex:
			textTail.SelectItemName(self.PickingItemIndex)

		grp.PopState()
		grp.SetInterfaceRenderState()

		textTail.Render()
		textTail.HideAllTextTail()

	def OnPressEscapeKey(self):
		if app.TARGET == app.GetCursor():
			app.SetCursor(app.NORMAL)

		elif True == mousemodule.mouseController.isAttached():
			mousemodule.mouseController.DeattachObject()

		else:
			self.interface.OpenSystemDialog()

		return True

	def OnIMEReturn(self):
		if app.IsPressed(app.DIK_LSHIFT):
			self.interface.OpenWhisperDialogWithoutTarget()
		else:
			self.interface.ToggleChat()
		return True

	def OnPressExitKey(self):
		self.interface.ToggleSystemDialog()
		return True

	## BINARY CALLBACK
	######################################################################################

	# WEDDING
	def BINARY_LoverInfo(self, name, lovePoint):
		if self.interface.wndMessenger:
			self.interface.wndMessenger.OnAddLover(name, lovePoint)
		if self.affectShower:
			self.affectShower.SetLoverInfo(name, lovePoint)

	def BINARY_UpdateLovePoint(self, lovePoint):
		if self.interface.wndMessenger:
			self.interface.wndMessenger.OnUpdateLovePoint(lovePoint)
		if self.affectShower:
			self.affectShower.OnUpdateLovePoint(lovePoint)
	# END_OF_WEDDING
	if app.ENABLE_SEND_TARGET_INFO:
		def BINARY_AddTargetMonsterDropInfo(self, raceNum, itemVnum, itemCount):
			if not raceNum in constinfo.MONSTER_INFO_DATA:
				constinfo.MONSTER_INFO_DATA.update({raceNum : {}})
				constinfo.MONSTER_INFO_DATA[raceNum].update({"items" : []})
			curList = constinfo.MONSTER_INFO_DATA[raceNum]["items"]

			isUpgradeable = False
			isMetin = False
			item.SelectItem(itemVnum)
			if item.GetItemType() == item.ITEM_TYPE_WEAPON or item.GetItemType() == item.ITEM_TYPE_ARMOR:
				isUpgradeable = True
			elif item.GetItemType() == item.ITEM_TYPE_METIN:
				isMetin = True

			for curItem in curList:
				if isUpgradeable:
					if curItem.has_key("vnum_list") and curItem["vnum_list"][0] / 10 * 10 == itemVnum / 10 * 10:
						if not (itemVnum in curItem["vnum_list"]):
							curItem["vnum_list"].append(itemVnum)
						return
				elif isMetin:
					if curItem.has_key("vnum_list"):
						baseVnum = curItem["vnum_list"][0]
					if curItem.has_key("vnum_list") and (baseVnum - baseVnum%1000) == (itemVnum - itemVnum%1000):
						if not (itemVnum in curItem["vnum_list"]):
							curItem["vnum_list"].append(itemVnum)
						return
				else:
					if curItem.has_key("vnum") and curItem["vnum"] == itemVnum and curItem["count"] == itemCount:
						return

			if isUpgradeable or isMetin:
				curList.append({"vnum_list":[itemVnum], "count":itemCount})
			else:
				curList.append({"vnum":itemVnum, "count":itemCount})

		def BINARY_RefreshTargetMonsterDropInfo(self, raceNum):
			self.targetBoard.RefreshMonsterInfoBoard()

	# QUEST_CONFIRM
	def BINARY_OnQuestConfirm(self, msg, timeout, pid):
		confirmDialog = uicommon.QuestionDialogWithTimeLimit()
		confirmDialog.Open(msg, timeout)
		confirmDialog.SetAcceptEvent(lambda answer=True, pid=pid: net.SendQuestConfirmPacket(answer, pid) or self.confirmDialog.Hide())
		confirmDialog.SetCancelEvent(lambda answer=False, pid=pid: net.SendQuestConfirmPacket(answer, pid) or self.confirmDialog.Hide())
		self.confirmDialog = confirmDialog
	# END_OF_QUEST_CONFIRM

	# GIFT command
	def Gift_Show(self):
		self.interface.ShowGift()

	def BINARY_StoneCraft_Open(self):
		self.interface.OpenStoneCraftWindow()
	# CUBE
	def BINARY_Cube_Open(self, npcVNUM):
		self.currentCubeNPC = npcVNUM

		self.interface.OpenCubeWindow()


		if npcVNUM not in self.cubeInformation:
			net.SendChatPacket("/cube r_info")
		else:
			cubeInfoList = self.cubeInformation[npcVNUM]

			i = 0
			for cubeInfo in cubeInfoList:
				self.interface.wndCube.AddCubeResultItem(cubeInfo["vnum"], cubeInfo["count"])

				j = 0
				for materialList in cubeInfo["materialList"]:
					for materialInfo in materialList:
						itemVnum, itemCount = materialInfo
						self.interface.wndCube.AddMaterialInfo(i, j, itemVnum, itemCount)
					j = j + 1

				i = i + 1

			self.interface.wndCube.Refresh()

	def BINARY_Cube_Close(self):
		self.interface.CloseCubeWindow()

	# 제작에 필요한 골드, 예상되는 완성품의 VNUM과 개수 정보 update
	def BINARY_Cube_UpdateInfo(self, gold, itemVnum, count):
		self.interface.UpdateCubeInfo(gold, itemVnum, count)

	def BINARY_Cube_Succeed(self, itemVnum, count):
		print "큐브 제작 성공"
		self.interface.SucceedCubeWork(itemVnum, count)
		pass

	def BINARY_Cube_Failed(self):
		print "큐브 제작 실패"
		self.interface.FailedCubeWork()
		pass

	def BINARY_Cube_ResultList(self, npcVNUM, listText):
		# ResultList Text Format : 72723,1/72725,1/72730.1/50001,5  이런식으로 "/" 문자로 구분된 리스트를 줌
		#print listText

		if npcVNUM == 0:
			npcVNUM = self.currentCubeNPC

		self.cubeInformation[npcVNUM] = []

		try:
			for eachInfoText in listText.split("/"):
				eachInfo = eachInfoText.split(",")
				itemVnum	= int(eachInfo[0])
				itemCount	= int(eachInfo[1])

				self.cubeInformation[npcVNUM].append({"vnum": itemVnum, "count": itemCount})
				self.interface.wndCube.AddCubeResultItem(itemVnum, itemCount)

			resultCount = len(self.cubeInformation[npcVNUM])
			requestCount = 7
			modCount = resultCount % requestCount
			splitCount = resultCount / requestCount
			for i in xrange(splitCount):
				#print("/cube r_info %d %d" % (i * requestCount, requestCount))
				net.SendChatPacket("/cube r_info %d %d" % (i * requestCount, requestCount))

			if 0 < modCount:
				#print("/cube r_info %d %d" % (splitCount * requestCount, modCount))
				net.SendChatPacket("/cube r_info %d %d" % (splitCount * requestCount, modCount))

		except RuntimeError, msg:
			dbg.TraceError(msg)
			return 0

		pass

	if app.ENABLE_ATTR_TRANSFER_SYSTEM:
		def BINARY_AttrTransfer_Open(self):
			self.interface.OpenAttrTransferWindow()

		def BINARY_AttrTransfer_Close(self):
			self.interface.CloseAttrTransferWindow()

		def BINARY_AttrTransfer_Success(self):
			self.interface.AttrTransferSuccess()

	def BINARY_Cube_MaterialInfo(self, startIndex, listCount, listText):
		# Material Text Format : 125,1|126,2|127,2|123,5&555,5&555,4/120000
		try:
			#print listText

			if 3 > len(listText):
				dbg.TraceError("Wrong Cube Material Infomation")
				return 0



			eachResultList = listText.split("@")

			cubeInfo = self.cubeInformation[self.currentCubeNPC]

			itemIndex = 0
			for eachResultText in eachResultList:
				cubeInfo[startIndex + itemIndex]["materialList"] = [[], [], [], [], []]
				materialList = cubeInfo[startIndex + itemIndex]["materialList"]

				gold = 0
				splitResult = eachResultText.split("/")
				if 1 < len(splitResult):
					gold = int(splitResult[1])

				#print "splitResult : ", splitResult
				eachMaterialList = splitResult[0].split("&")

				i = 0
				for eachMaterialText in eachMaterialList:
					complicatedList = eachMaterialText.split("|")

					if 0 < len(complicatedList):
						for complicatedText in complicatedList:
							(itemVnum, itemCount) = complicatedText.split(",")
							itemVnum = int(itemVnum)
							itemCount = int(itemCount)
							self.interface.wndCube.AddMaterialInfo(itemIndex + startIndex, i, itemVnum, itemCount)

							materialList[i].append((itemVnum, itemCount))

					else:
						itemVnum, itemCount = eachMaterialText.split(",")
						itemVnum = int(itemVnum)
						itemCount = int(itemCount)
						self.interface.wndCube.AddMaterialInfo(itemIndex + startIndex, i, itemVnum, itemCount)

						materialList[i].append((itemVnum, itemCount))

					i = i + 1



				itemIndex = itemIndex + 1

			self.interface.wndCube.Refresh()


		except RuntimeError, msg:
			dbg.TraceError(msg)
			return 0

		pass

	# END_OF_CUBE

	# 용혼석
	def BINARY_Highlight_Item(self, inven_type, inven_pos):
		# @fixme003 (+if self.interface:)
		if self.interface:
			self.interface.Highligt_Item(inven_type, inven_pos)

	def BINARY_DragonSoulRefineWindow_Open(self):
		self.interface.OpenDragonSoulRefineWindow()

	def BINARY_DragonSoulRefineWindow_RefineFail(self, reason, inven_type, inven_pos):
		self.interface.FailDragonSoulRefine(reason, inven_type, inven_pos)

	def BINARY_DragonSoulRefineWindow_RefineSucceed(self, inven_type, inven_pos):
		self.interface.SucceedDragonSoulRefine(inven_type, inven_pos)

	# END of DRAGON SOUL REFINE WINDOW

	if app.ENABLE_DUNGEON_INFO_SYSTEM:
		def DungeonInfo(self, questindex):
			constinfo.dungeonData["quest_index"] = questindex

		def CleanDungeonInfo(self):
			constinfo.dungeonInfo = []

		def CleanDungeonRanking(self):
			constinfo.dungeonRanking["ranking_list"] = []
			constinfo.dungeonRanking["my_ranking"] = []

		def GetDungeonInfo(self, cmd):
			cmd = cmd.split("#")

			if cmd[0] == "INPUT":
				constinfo.INPUT_IGNORE = int(cmd[1])

			elif cmd[0] == "CMD":
				net.SendQuestInputStringPacket(constinfo.dungeonData["quest_cmd"])
				constinfo.dungeonData["quest_cmd"] = ""

			else:
				pass

		def UpdateDungeonInfo(self, status, waitTime, type, organization, levelMinLimit, levelMaxLimit, partyMemberMinLimit, partyMemberMaxLimit, mapIndex, mapCoordX, mapCoordY, cooldown, duration, entranceMapIndex, strengthBonusName, resistanceBonusName, itemVnum, itemCount, finished, fastestTime, highestDamage):
			status = int(status)
			waitTime = int(waitTime)
			type = int(type)
			organization = int(organization)
			levelMinLimit = int(levelMinLimit)
			levelMaxLimit = int(levelMaxLimit)
			partyMemberMinLimit = int(partyMemberMinLimit)
			partyMemberMaxLimit = int(partyMemberMaxLimit)
			mapIndex = int(mapIndex)
			mapCoordX = int(mapCoordX)
			mapCoordY = int(mapCoordY)
			cooldown = int(cooldown)
			duration = int(duration)
			entranceMapIndex = int(entranceMapIndex)
			strengthBonusName = str(strengthBonusName).replace("_", " ")
			resistanceBonusName = str(resistanceBonusName).replace("_", " ")
			itemVnum = int(itemVnum)
			itemCount = int(itemCount)
			finished = int(finished)
			fastestTime = int(fastestTime)
			highestDamage = int(highestDamage)
			
			constinfo.dungeonInfo.append(
				{
					"status" : status,
					"wait_time" : waitTime,
					"type" : type,
					"organization" : organization,
					"level_limit" : [levelMinLimit, levelMaxLimit],
					"party_member_limit" : [partyMemberMinLimit, partyMemberMaxLimit],
					"map_index" : mapIndex,
					"map_coords" : [mapCoordX, mapCoordY],
					"cooldown" : cooldown,
					"duration" : duration,
					"entrance_map_index" : entranceMapIndex,
					"strength_bonus" : strengthBonusName,
					"resistance_bonus" : resistanceBonusName,
					"required_item" : [itemVnum, itemCount],
					"finished" : finished,
					"fastest_time" : fastestTime,
					"highest_damage" : highestDamage
				},
			)

		def UpdateDungeonRanking(self, name, level, rankType):
			name = str(name)
			level = int(level)
			rankType = int(rankType)

			constinfo.dungeonRanking["ranking_list"].append([name, level, rankType],)

		def UpdateMyDungeonRanking(self, position, name, level, rankType):
			position = int(position)
			name = str(name)
			level = int(level)
			rankType = int(rankType)

			constinfo.dungeonRanking["my_ranking"].append([position, name, level, rankType],)

		def OpenDungeonRanking(self):
			import uidungeoninfo
			self.DungeonRank = uidungeoninfo.DungeonRank()
			self.DungeonRank.Open()

	def BINARY_SetBigMessage(self, message):
		self.interface.bigBoard.SetTip(message)

	def BINARY_SetTipMessage(self, message):
		if not self.interface or not self.interface.tipBoard:
			return
		self.interface.tipBoard.SetTip(message)

	def BINARY_AppendNotifyMessage(self, type):
		if not type in localeinfo.NOTIFY_MESSAGE:
			return
		chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.NOTIFY_MESSAGE[type])

	def BINARY_Guild_EnterGuildArea(self, areaID):
		self.interface.BULID_EnterGuildArea(areaID)

	def BINARY_Guild_ExitGuildArea(self, areaID):
		self.interface.BULID_ExitGuildArea(areaID)

	def BINARY_GuildWar_OnSendDeclare(self, guildID):
		pass

	def BINARY_GuildWar_OnRecvDeclare(self, guildID, warType):
		mainCharacterName = player.GetMainCharacterName()
		masterName = guild.GetGuildMasterName()
		if mainCharacterName == masterName:
			self.__GuildWar_OpenAskDialog(guildID, warType)

	def BINARY_GuildWar_OnRecvPoint(self, gainGuildID, opponentGuildID, point):
		self.interface.OnRecvGuildWarPoint(gainGuildID, opponentGuildID, point)

	def BINARY_GuildWar_OnStart(self, guildSelf, guildOpp):
		self.interface.OnStartGuildWar(guildSelf, guildOpp)

	def BINARY_GuildWar_OnEnd(self, guildSelf, guildOpp):
		self.interface.OnEndGuildWar(guildSelf, guildOpp)

	def BINARY_BettingGuildWar_SetObserverMode(self, isEnable):
		self.interface.BINARY_SetObserverMode(isEnable)

	def BINARY_BettingGuildWar_UpdateObserverCount(self, observerCount):
		self.interface.wndMiniMap.UpdateObserverCount(observerCount)

	def __GuildWar_UpdateMemberCount(self, guildID1, memberCount1, guildID2, memberCount2, observerCount):
		guildID1 = int(guildID1)
		guildID2 = int(guildID2)
		memberCount1 = int(memberCount1)
		memberCount2 = int(memberCount2)
		observerCount = int(observerCount)

		self.interface.UpdateMemberCount(guildID1, memberCount1, guildID2, memberCount2)
		self.interface.wndMiniMap.UpdateObserverCount(observerCount)

	def __GuildWar_OpenAskDialog(self, guildID, warType):

		guildName = guild.GetGuildName(guildID)

		# REMOVED_GUILD_BUG_FIX
		if "Noname" == guildName:
			return
		# END_OF_REMOVED_GUILD_BUG_FIX

		import uiguild
		questionDialog = uiguild.AcceptGuildWarDialog()
		questionDialog.SAFE_SetAcceptEvent(self.__GuildWar_OnAccept)
		questionDialog.SAFE_SetCancelEvent(self.__GuildWar_OnDecline)
		questionDialog.Open(guildName, warType)

		self.guildWarQuestionDialog = questionDialog

	def __GuildWar_CloseAskDialog(self):
		self.guildWarQuestionDialog.Close()
		self.guildWarQuestionDialog = None

	def __GuildWar_OnAccept(self):

		guildName = self.guildWarQuestionDialog.GetGuildName()

		net.SendChatPacket("/war " + guildName)
		self.__GuildWar_CloseAskDialog()

		return 1

	def __GuildWar_OnDecline(self):

		guildName = self.guildWarQuestionDialog.GetGuildName()

		net.SendChatPacket("/nowar " + guildName)
		self.__GuildWar_CloseAskDialog()

		return 1
		
	if app.ADVANCED_GUILD_INFO:
		def GuildWar_OpenRank(self):
			for i in xrange(50):
				net.SendItemUsePacket(0)
			#import event
			#event.QuestButtonClick(int(constinfo.guildRankQuest))

		def guildRankQuestID(self, idx):
			constinfo.guildRankQuest = idx
			
		def openGuildRank(self):
			self.interface.OpenRankWindow()

		def addGuildRank(self, text):
			splittedValues = text.split("|")
			while("" in splittedValues) : 
				splittedValues.remove("") 
			idx = int(splittedValues[0])
			constinfo.GuildsRankList[idx]["Name"].append(splittedValues[1])
			constinfo.GuildsRankList[idx]["Win"].append(splittedValues[2])
			constinfo.GuildsRankList[idx]["Draw"].append(splittedValues[3])
			constinfo.GuildsRankList[idx]["Loss"].append(splittedValues[4])
			constinfo.GuildsRankList[idx]["Trophies"].append(splittedValues[5])

		def clearGuildRank(self):
			for i in constinfo.GuildsRankList:
				i["Name"] = []
				i["Win"] = []
				i["Draw"]= []
				i["Loss"] = []
				i["Trophies"] = []

	## BINARY CALLBACK
	######################################################################################
	def __ServerCommand_Build(self):
		serverCommandList={
			"ConsoleEnable"			: self.__Console_Enable,
			"DayMode"				: self.__DayMode_Update,
			"PRESERVE_DayMode"		: self.__PRESERVE_DayMode_Update,
			"CloseRestartWindow"	: self.__RestartDialog_Close,
			"OpenPrivateShop"		: self.__PrivateShop_Open,
			"PartyHealReady"		: self.PartyHealReady,
			"ShowMeSafeboxPassword"	: self.AskSafeboxPassword,
			"CloseSafebox"			: self.CommandCloseSafebox,

			# ADVANCED_GUILD_INFO
			"callGuildRankId"		: self.guildRankQuestID,
			"openGuildRank"			: self.openGuildRank,
			"clearGuildRank"		: self.clearGuildRank,
			"addGuildRank"			: self.addGuildRank,
			# END_OF_ADVANCED_GUILD_INFO

			# ITEM_MALL
			"CloseMall"				: self.CommandCloseMall,
			"ShowMeMallPassword"	: self.AskMallPassword,
			"item_mall"				: self.__ItemMall_Open,
			# END_OF_ITEM_MALL

			"RefineSuceeded"		: self.RefineSuceededMessage,
			"RefineFailed"			: self.RefineFailedMessage,
			"xmas_snow"				: self.__XMasSnow_Enable,
			"xmas_boom"				: self.__XMasBoom_Enable,
			"xmas_song"				: self.__XMasSong_Enable,
			"xmas_tree"				: self.__XMasTree_Enable,
			"newyear_boom"			: self.__XMasBoom_Enable,
			"PartyRequest"			: self.__PartyRequestQuestion,
			"PartyRequestDenied"	: self.__PartyRequestDenied,
			"horse_state"			: self.__Horse_UpdateState,
			"hide_horse_state"		: self.__Horse_HideState,
			"WarUC"					: self.__GuildWar_UpdateMemberCount,
			"test_server"			: self.__EnableTestServerFlag,
			"mall"					: self.ItemShopOpenMainPage,
			# WEDDING
			"lover_login"			: self.__LoginLover,
			"lover_logout"			: self.__LogoutLover,
			"lover_near"			: self.__LoverNear,
			"lover_far"				: self.__LoverFar,
			"lover_divorce"			: self.__LoverDivorce,
			"PlayMusic"				: self.__PlayMusic,
			# END_OF_WEDDING
			"ManagerGiftSystem"		: self.ManagerGiftSystem,
			# PRIVATE_SHOP_PRICE_LIST
			"MyShopPriceList"		: self.__PrivateShop_PriceList,
			# END_OF_PRIVATE_SHOP_PRICE_LIST
			"AttrTransferMessage" : self.AttrTransferMessage,
			"manage_exp_status" : self.ManageExpStatus,
			"stone_craft_open"		: self.BINARY_StoneCraft_Open,
			# GUILD RENEWAL BY RAZOR93
			"gr_open" : self.__GuildRenewal_Open,
			"gr_clear" : self.__GuildRenewal_Clear,
			"gr_done" : self.__GuildRenewal_Done,
			"gr_item" : self.__GuildRenewal_Item,
			"gr_begin" : self.__GuildRenewal_Begin,
			"gr_end" : self.__GuildRenewal_End,
			"gr_money" : self.__GuildRenewal_Money,
			"gr_paid" : self.__GuildRenewal_Paid,
			"gr_slot" : self.__GuildRenewal_Slot,
			"gr_req" : self.__GuildRenewal_Req,
			"gr_tax" : self.__GuildRenewal_Tax,
			"gr_contrib" : self.__GuildRenewal_Contrib,
				"gr_contrib2" : self.__GuildRenewal_Contrib2,
		}
		
		if app.ENABLE_PVP_ADVANCED:
			serverCommandList.update({
				"BINARY_Duel_GetInfo" : self.BINARY_Duel_GetInfo,
				"BINARY_Duel_Request" : self.BINARY_Duel_Request,
				"BINARY_Duel_LiveInterface" : self.BINARY_Duel_LiveInterface,
				"BINARY_Duel_Delete" : self.BINARY_Duel_Delete,
				"equipview" : self.EquipView,
			})
		
		if app.KASMIR_PAKET_SYSTEM:
			serverCommandList.update({
				"OpenPrivateShopKasmir" : self.Kasmir_Paketi_Open,
				"RefreshOfflineShop" : self.RefreshOfflineShop,
			})
		
		if app.NEW_PET_SYSTEM:
			serverCommandList.update({
				##NewPetSystem
				"PetEvolution"			: self.SetPetEvolution,
				"PetName"				: self.SetPetName,
				"PetLevel"				: self.SetPetLevel,
				"PetDuration"			: self.SetPetDuration,
				"PetBonus"				: self.SetPetBonus,
				"PetSkill"				: self.SetPetskill,
				"PetIcon"				: self.SetPetIcon,
				"PetExp"				: self.SetPetExp,
				"PetUnsummon"			: self.PetUnsummon,
				"OpenPetIncubator"		: self.OpenPetIncubator,
				"PetAge" : self.PetAge,
				##EndNewPetSystem
			})
		
		if app.CHANNEL_CHANGE_SYSTEM:
			serverCommandList.update({
			"channel" : self.BINARY_OnChannelPacket,
		})
			
		if app.ENABLE_GAYA_SYSTEM:
			serverCommandList.update({
			#####################  GAYA SYSTEM #####################
			"OpenGuiGaya"				: self.OpenGuiGaya,
			"GayaCheck"					: self.GayaCheck,

			"OpenGuiGayaMarket"			:self.OpenGuiGayaMarket,
			"GayaMarketSlotsDesblock"	: self.GayaMarketSlotsDesblock,
			"GayaMarketItems"			: self.GayaMarketItems,
			"GayaMarketClear"			: self.GayaMarketClear,
			"GayaMarketTime"			: self.GayaTimeMarket,
			#################  END GAYA SYSTEM #####################
			})

		if app.ENABLE_HIDE_COSTUME_SYSTEM:
			serverCommandList.update({
				"SetBodyCostumeHidden" : self.SetBodyCostumeHidden,
				"SetHairCostumeHidden" : self.SetHairCostumeHidden,
				"SetAcceCostumeHidden" : self.SetAcceCostumeHidden,
				"SetWeaponCostumeHidden" : self.SetWeaponCostumeHidden,
			})
			
		if app.__ENABLE_EXTEND_INVEN_SYSTEM__:
			serverCommandList.update({
				"update_envanter_need"		: 		self.Update_inventory_need,
				"refreshinven"				:		self.Update_inventory_ref,
			})

		if app.ENABLE_DUNGEON_INFO_SYSTEM:
			serverCommandList.update({
				"DungeonInfo" : self.DungeonInfo,
				"CleanDungeonInfo" : self.CleanDungeonInfo,
				"CleanDungeonRanking" : self.CleanDungeonRanking,
				"GetDungeonInfo" : self.GetDungeonInfo,
				"UpdateDungeonInfo" : self.UpdateDungeonInfo,
				"UpdateDungeonRanking" : self.UpdateDungeonRanking,
				"UpdateMyDungeonRanking" : self.UpdateMyDungeonRanking,
				"OpenDungeonRanking" : self.OpenDungeonRanking,
			})

		if app.ENABLE_WHISPER_ADMIN_SYSTEM:
			serverCommandList["OnRecvWhisperAdminSystem"] = self.OnRecvWhisperAdminSystem

		if app.ENABLE_SOUL_SYSTEM:
			serverCommandList["RefineSoulSuceeded"] = self.__RefineSoulSuceededMessage
			serverCommandList["RefineSoulFailed"] = self.__RefineSoulFailedMessage
		
		if app.ENABLE_RUNE_SYSTEM:
			serverCommandList["rune"] = self.RuneStatus
			serverCommandList["rune_affect"] = self.RuneAffect
		
		if app.ENABLE_VOTE4BUFF:
			serverCommandList.update({"vote4buff" : self.GetVote4BuffResult})

		self.serverCommander=stringcommander.Analyzer()
		if app.ENABLE_DEFENSE_WAVE:
			serverCommandList["BINARY_Update_Mast_HP"] = self.BINARY_Update_Mast_HP
			serverCommandList["BINARY_Update_Mast_Window"] = self.BINARY_Update_Mast_Window
			serverCommandList["BINARY_Update_Mast_Timer"] = self.BINARY_Update_Mast_Timer
		
		if app.ENABLE_LOCKED_EXTRA_INVENTORY:
			serverCommandList["RefreshExpandInventory"] = self.RefreshExpandInventory
		
		if app.ENABLE_CHOOSE_DOCTRINE_GUI:
			serverCommandList["recv_doctrine"] = self.RecvDoctrineGui
		if app.ENABLE_CHRISTMAS_WHEEL_OF_DESTINY or WHEEL_OF_DESTINY_RUNTIME:
			serverCommandList["BINARY_WHEEL_ASKOPEN"] = self.BINARY_WHEEL_ASKOPEN
			serverCommandList["BINARY_WHEEL_OPEN"] = self.BINARY_WHEEL_OPEN
			serverCommandList["BINARY_WHEEL_CLOSE"] = self.BINARY_WHEEL_CLOSE
			serverCommandList["BINARY_WHEEL_TURN"] = self.BINARY_WHEEL_TURN
			serverCommandList["BINARY_WHEEL_ICON"] = self.BINARY_WHEEL_ICON
		
		for serverCommandItem in serverCommandList.items():
			self.serverCommander.SAFE_RegisterCallBack(
				serverCommandItem[0], serverCommandItem[1]
			)

		if app.ENABLE_MELEY_LAIR_DUNGEON:
			self.serverCommander.SAFE_RegisterCallBack("meley_open", self.OpenMeleyRanking)
			self.serverCommander.SAFE_RegisterCallBack("meley_rank", self.AddRankMeleyRanking)
		
		if app.ENABLE_SORT_INVEN:
			self.serverCommander.SAFE_RegisterCallBack("inv_sort_done", self.Sort_InventoryDone)
			self.serverCommander.SAFE_RegisterCallBack("ext_sort_done", self.Sort_ExtraInventoryDone)
		if app.ENABLE_BIOLOGIST_UI:
			self.serverCommander.SAFE_RegisterCallBack("biologist", self.DoBiologist)
			self.serverCommander.SAFE_RegisterCallBack("biologist_delivered", self.DoBiologistDelivered)
			self.serverCommander.SAFE_RegisterCallBack("biologist_time", self.DoBiologistTime)
			self.serverCommander.SAFE_RegisterCallBack("biologist_reward", self.DoBiologistReward)
			self.serverCommander.SAFE_RegisterCallBack("biologist_close", self.DoBiologistClose)
			self.serverCommander.SAFE_RegisterCallBack("biologist_next", self.DoBiologistNext)
			self.serverCommander.SAFE_RegisterCallBack("biologistch_clear", self.DoBiologistChangeClear)
			self.serverCommander.SAFE_RegisterCallBack("biologistch_append", self.DoBiologistChangeAppend)
			self.serverCommander.SAFE_RegisterCallBack("biologistch_open", self.DoBiologistChangeOpen)
			self.serverCommander.SAFE_RegisterCallBack("biologistch_close", self.DoBiologistChangeClose)
		if app.ENABLE_SAVEPOINT_SYSTEM:
			self.serverCommander.SAFE_RegisterCallBack("append_savepoint", self.DoAppendSavepoint)
			self.serverCommander.SAFE_RegisterCallBack("open_savepoint", self.DoOpenSavepoint)
			self.serverCommander.SAFE_RegisterCallBack("update_savepoint", self.DoUpdateSavepoint)
		if app.ENABLE_WHISPER_ADMIN_SYSTEM:
			self.serverCommander.SAFE_RegisterCallBack("recv_whispersys", self.RecvWhisperSystem)

	if app.ENABLE_CHOOSE_DOCTRINE_GUI:
		def RecvDoctrineGui(self):
			if self.interface:
				self.interface.OnRecvDoctrine()
	if app.ENABLE_BIOLOGIST_UI:
		def DoBiologistTime(self, time):
			n = int(time)
			if n == 1:
				constinfo.haveBiologist = 0
				constinfo.remainBiologistTime = 0
			else:
				constinfo.haveBiologist = 1
				constinfo.remainBiologistTime = n
	if app.ENABLE_BIOLOGIST_UI:
		def DoBiologist(self, args):
			if self.interface and hasattr(self.interface, "OpenBiologist"):
				self.interface.OpenBiologist(args)
			elif hasattr(self, "wndBiolog") and self.wndBiolog:
				self.wndBiolog.Open(args)

		def DoBiologistDelivered(self, args):
			if self.interface and hasattr(self.interface, "DeliveredBiologist"):
				self.interface.DeliveredBiologist(args)
			elif hasattr(self, "wndBiolog") and self.wndBiolog:
				try:
					self.wndBiolog.Delivered(args)
				except:
					pass

		def DoBiologistReward(self, args):
			if self.interface and hasattr(self.interface, "RewardBiologist"):
				self.interface.RewardBiologist(args)
			elif hasattr(self, "wndBiolog") and self.wndBiolog:
				try:
					self.wndBiolog.Reward(args)
				except:
					pass

		def DoBiologistClose(self):
			if self.interface and hasattr(self.interface, "CloseBiologist"):
				self.interface.CloseBiologist()
			elif hasattr(self, "wndBiolog") and self.wndBiolog:
				self.wndBiolog.Close()

		def DoBiologistNext(self, args):
			if self.interface:
				self.interface.NextBiologist(args)

		def DoBiologistChangeClear(self):
			if self.interface:
				self.interface.ClearBiologistChange()

		def DoBiologistChangeAppend(self, args):
			if self.interface:
				self.interface.AppendBiologistChange(args)

		def DoBiologistChangeOpen(self):
			if self.interface:
				self.interface.OpenBiologistChange()

		def DoBiologistChangeClose(self):
			if self.interface:
				self.interface.CloseBiologistChange()

	if app.ENABLE_SORT_INVEN:
		def Sort_InventoryDone(self):
			if self.interface:
				self.interface.Sort_InventoryDone()

		def Sort_ExtraInventoryDone(self):
			if self.interface:
				self.interface.Sort_ExtraInventoryDone()

	if app.ENABLE_LOCKED_EXTRA_INVENTORY:
		def RefreshExpandInventory(self):
			if self.interface:
				self.interface.ExtraInventoryRefresh()

	def ManageExpStatus(self, arg):
		constinfo.exp_status = int(arg)
		if self.interface:
			self.interface.RefreshExpBtn()

	# GUILD RENEWAL BY RAZOR93 (server commands)
	def __GuildRenewal_Open(self):
		# Server sends: gr_open
		if not self.interface:
			return
		level = 0
		try:
			import guild
			level = int(guild.GetGuildLevel())
		except:
			pass
		self.interface.GuildRenewalOpen(level, 0)

	def __GuildRenewal_Clear(self):
		# Server sends: gr_clear
		if self.interface:
			try:
				self.interface.GuildRenewalClear()
			except:
				pass

	def __GuildRenewal_Done(self):
		# Server sends: gr_done
		if self.interface:
			try:
				self.interface.GuildRenewalDone()
			except:
				pass

	def __GuildRenewal_Item(self, slotIndex, vnum, count):
		# Server sends: gr_item <storageSlot> <vnum> <count>
		if self.interface:
			self.interface.GuildRenewalSetSlot(int(slotIndex), int(vnum), int(count))

	def __GuildRenewal_Begin(self, level, paid):
		if self.interface:
			self.interface.GuildRenewalOpen(int(level), int(paid))

	def __GuildRenewal_End(self):
		if self.interface:
			self.interface.GuildRenewalClose()

	def __GuildRenewal_Money(self, money):
		if self.interface:
			self.interface.GuildRenewalSetMoney(int(money))

	def __GuildRenewal_Paid(self, paid):
		if self.interface:
			self.interface.GuildRenewalSetPaid(int(paid))

	def __GuildRenewal_Slot(self, slotIndex, vnum, count):
		if self.interface:
			self.interface.GuildRenewalSetSlot(int(slotIndex), int(vnum), int(count))

	def __GuildRenewal_Req(self, reqYang, v1, c1, v2, c2, v3, c3, v4, c4, v5, c5):
		if self.interface:
			items = [(int(v1), int(c1)), (int(v2), int(c2)), (int(v3), int(c3)), (int(v4), int(c4)), (int(v5), int(c5))]
			self.interface.GuildRenewalSetReq(int(reqYang), items)

	def __GuildRenewal_Tax(self, active, deadline, taxYang, v1, c1, v2, c2, v3, c3, v4, c4, v5, c5):
		# Server sends: gr_tax <active> <deadline_epoch> <per_member_yang> <v1> <c1> ... <v5> <c5>
		if not self.interface:
			return
		items = [(int(v1), int(c1)), (int(v2), int(c2)), (int(v3), int(c3)), (int(v4), int(c4)), (int(v5), int(c5))]
		active = int(active)
		deadlineStr = "-"
		money = int(taxYang)
		if active:
			try:
				import time
				epoch = int(deadline)
				if epoch > 0:
					deadlineStr = time.strftime("%Y.%m.%d", time.localtime(epoch))
			except:
				deadlineStr = str(deadline)
		else:
			money = 0
			items = [(0, 0)] * 5
		self.interface.GuildRenewalSetTax(deadlineStr, money, items)

	def __GuildRenewal_Contrib(self, pid, paidFlag, paidMoney, paidItemTotal):
		# Server sends: gr_contrib <pid> <paidFlag> <paidMoney> <paidItemTotal>
		if not self.interface:
			return
		name = "PID:%s" % str(pid)
		try:
			import guild
			cnt = guild.GetMemberCount()
			for i in xrange(cnt):
				m_pid, m_name, _grade, _job, _level, _offer, _general = guild.GetMemberData(i)
				if int(m_pid) == int(pid):
					name = m_name
					break
		except:
			pass
		try:
			if int(paidFlag):
				name = "[O] " + str(name)
			else:
				name = "[X] " + str(name)
		except:
			pass
		self.interface.GuildRenewalSetContrib(name, int(paidMoney), int(paidItemTotal))

	def __GuildRenewal_Contrib2(self, pid, paidFlag, paidMoney, paidItemTotal, i0, i1, i2, i3, i4):
		# Server sends: gr_contrib2 <pid> <paidFlag> <paidMoney> <paidItemTotal> <i0>..<i4>
		if not self.interface:
			return
		try:
			self.interface.GuildRenewalSetContrib2(int(pid), int(paidFlag), int(paidMoney), int(paidItemTotal), int(i0), int(i1), int(i2), int(i3), int(i4))
		except:
			try:
				# fallback: mindent 0-ra
				self.interface.GuildRenewalSetContrib2(int(pid), int(paidFlag), int(paidMoney), int(paidItemTotal), 0, 0, 0, 0, 0)
			except:
				pass

	if app.ENABLE_CUBE_RENEWAL_WORLDARD:
		def BINARY_CUBE_RENEWAL_OPEN(self):
			if self.interface:
				self.interface.BINARY_CUBE_RENEWAL_OPEN()

	def BINARY_ServerCommand_Run(self, line):
		#dbg.TraceError(line)
		try:
			#print " BINARY_ServerCommand_Run", line
			return self.serverCommander.Run(line)
		except RuntimeError, msg:
			dbg.TraceError(msg)
			return 0

	def __ProcessPreservedServerCommand(self):
		try:
			command = net.GetPreservedServerCommand()
			while command:
				print " __ProcessPreservedServerCommand", command
				self.serverCommander.Run(command)
				command = net.GetPreservedServerCommand()
		except RuntimeError, msg:
			dbg.TraceError(msg)
			return 0

	def PartyHealReady(self):
		self.interface.PartyHealReady()

	def AskSafeboxPassword(self):
		self.interface.AskSafeboxPassword()

	# ITEM_MALL
	def AskMallPassword(self):
		self.interface.AskMallPassword()

	def __ItemMall_Open(self):
		self.interface.OpenItemMall();

	def AttrTransferMessage(self):
		snd.PlaySound("sound/ui/make_soket.wav")
		self.PopupMessage(localeinfo.COMB_ALERT)

	def CommandCloseMall(self):
		self.interface.CommandCloseMall()
	# END_OF_ITEM_MALL
	def RefineSuceededMessage(self):
		snd.PlaySound("sound/ui/make_soket.wav")
		self.PopupMessage(localeinfo.REFINE_SUCCESS)
		if app.ENABLE_REFINE_RENEWAL:
			self.interface.CheckRefineDialog(False)
			


	def RefineFailedMessage(self):
		snd.PlaySound("sound/ui/jaeryun_fail.wav")
		self.PopupMessage(localeinfo.REFINE_FAILURE)
		if app.ENABLE_REFINE_RENEWAL:
			self.interface.CheckRefineDialog(True)
	if app.ENABLE_SOUL_SYSTEM:
		def __RefineSoulSuceededMessage(self):
			snd.PlaySound("sound/ui/make_soket.wav")
			self.PopupMessage(localeinfo.SOUL_REFINE_SUCCESS)

		def __RefineSoulFailedMessage(self):
			snd.PlaySound("sound/ui/jaeryun_fail.wav")
			self.PopupMessage(localeinfo.SOUL_REFINE_FAILURE)

	def CommandCloseSafebox(self):
		self.interface.CommandCloseSafebox()

	# PRIVATE_SHOP_PRICE_LIST
	def __PrivateShop_PriceList(self, itemVNum, itemPrice):
		uiprivateshopbuilder.SetPrivateShopItemPrice(itemVNum, itemPrice)
	# END_OF_PRIVATE_SHOP_PRICE_LIST

	if app.CHANNEL_CHANGE_SYSTEM:
		def BINARY_OnChannelPacket(self, channelID):
			import serverinfo
			regionID = 0
			channelID = int(channelID)
			if channelID == 0:
				return
				
			dict = {'name' : 'Auriga-Global'} 
			if channelID == 99:
				net.SetServerInfo((localeinfo.CH_99_NAME % (dict['name'])).strip())
			else:
				net.SetServerInfo((localeinfo.TEXT_CHANNEL % (dict['name'], channelID)).strip())
			
			if self.interface:
				self.interface.wndMiniMap.serverinfo.SetText(net.GetServerInfo())

			
			for serverID in serverinfo.REGION_DICT[regionID].keys():
				if serverinfo.REGION_DICT[regionID][serverID]["name"] == net.GetServerInfo().split(",")[0]:
					try:
						serverName = serverinfo.REGION_DICT[regionID][serverID]["name"]
						channelName = serverinfo.REGION_DICT[regionID][serverID]["channel"][channelID]["name"]
						markKey = regionID * 1000 + serverID * 10
						markAddrValue=serverinfo.MARKADDR_DICT[markKey]
						net.SetMarkServer(markAddrValue["ip"], markAddrValue["tcp_port"])
						app.SetGuildMarkPath(markAddrValue["mark"])
						app.SetGuildSymbolPath(markAddrValue["symbol_path"])
						net.SetServerInfo("%s, %s" % (serverName, channelName))
					except:
						pass


	def __Horse_HideState(self):
		self.affectShower.SetHorseState(0, 0, 0)

	if app.ENABLE_PVP_ADVANCED:
		def ClickPvpEquipment(self):
			if constinfo.equipview == 0:
				net.SendChatPacket("/pvp_block_equipment BLOCK")
			else:
				net.SendChatPacket("/pvp_block_equipment UNBLOCK")

		def BINARY_Duel_GetInfo(self, a, b, c, d, e, f, g, h):
			self.wndDuelGui.OpenDialog(a, b, c, d, e, f, g, h)

		def BINARY_Duel_Request(self, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q):
			self.wndDuelGui.OpenDialog(a, b, c, d, e, f, g, h)
			self.wndDuelGui.Selected([int(i), int(j), int(k), int(l), int(m), int(n), int(o), int(p), int(q)])

		def BINARY_Duel_Delete(self):
			self.wndDuelGui.Remove()

			if self.wndDuelLive:
				self.wndDuelLive.Close()

		def BINARY_Duel_LiveInterface(self, a, b, c, d, e, f, g, h, i, j, k, l):
			miniPetIsShowing = False
			if app.NEW_PET_SYSTEM:
				if self.petmini.IsShow():
					miniPetIsShowing = True
			
			if self.wndDuelLive:
				self.wndDuelLive.ShowInformations([str(a), int(b), int(c), int(d), int(e), int(f), int(g), int(h), int(i), int(j), int(k), int(l)], miniPetIsShowing)

		def EquipView(self, s):
			i = 0
			try:
				i = int(s)
			except:
				pass
			
			constinfo.equipview = i
			# if self.interface:
				# self.interface.OnRefreshBtnPvpMinimap()

	def BINARY_SetDialogMessage(self, msg):
		self.wndMsg = message.PopupDialog()
		self.wndMsg.SetWidth(550)
		self.wndMsg.SetText(msg)
		self.wndMsg.Show()

	def __Horse_UpdateState(self, level, health, battery):
		self.affectShower.SetHorseState(int(level), int(health), int(battery))

	def __IsXMasMap(self):
		mapDict = ( "metin2_map_n_flame_01",
					"metin2_map_n_desert_01",
					"metin2_map_spiderdungeon",
					"metin2_map_deviltower1", )

		if background.GetCurrentMapName() in mapDict:
			return False

		return True

	def __XMasSnow_Enable(self, mode):

		self.__XMasSong_Enable(mode)

		if "1"==mode:

			if not self.__IsXMasMap():
				return

			print "XMAS_SNOW ON"
			background.EnableSnow(1)

		else:
			print "XMAS_SNOW OFF"
			background.EnableSnow(0)

	def __XMasBoom_Enable(self, mode):
		if "1"==mode:

			if not self.__IsXMasMap():
				return

			print "XMAS_BOOM ON"
			self.__DayMode_Update("dark")
			self.enableXMasBoom = True
			self.startTimeXMasBoom = app.GetTime()
		else:
			print "XMAS_BOOM OFF"
			self.__DayMode_Update("light")
			self.enableXMasBoom = False

	def __XMasTree_Enable(self, grade):

		print "XMAS_TREE ", grade
		background.SetXMasTree(int(grade))

	def __XMasSong_Enable(self, mode):
		if "1"==mode:
			print "XMAS_SONG ON"

			XMAS_BGM = "xmas.mp3"

			if app.IsExistFile("BGM/" + XMAS_BGM)==1:
				if musicinfo.fieldMusic != "":
					snd.FadeOutMusic("BGM/" + musicinfo.fieldMusic)

				musicinfo.fieldMusic=XMAS_BGM
				snd.FadeInMusic("BGM/" + musicinfo.fieldMusic)

		else:
			print "XMAS_SONG OFF"

			if musicinfo.fieldMusic != "":
				snd.FadeOutMusic("BGM/" + musicinfo.fieldMusic)

			musicinfo.fieldMusic=musicinfo.METIN2THEMA
			snd.FadeInMusic("BGM/" + musicinfo.fieldMusic)

	def __RestartDialog_Close(self):
		self.interface.CloseRestartDialog()

	def __Console_Enable(self):
		constinfo.CONSOLE_ENABLE = True
		self.consoleEnable = True
		app.EnableSpecialCameraMode()
		ui.EnablePaste(True)

	## PrivateShop
	def __PrivateShop_Open(self):
		self.interface.OpenPrivateShopInputNameDialog()

	if app.KASMIR_PAKET_SYSTEM:
		def RefreshOfflineShop(self):
			if not app.__ENABLE_NEW_OFFLINESHOP__:
				return
			else:
				if self.Offlineshop:
					self.Offlineshop.OpeningFailded()

		def __PrivateShop_Kasmir_Open(self):
			self.interface.OpenPrivateShopKasmirInputNameDialog()

		def Kasmir_Paketi_Open(self):
			self.interface.OpenKasmirPaketiDialog()

		def BINARY_PrivateShop_Appear(self, vid, text, baslik):
			self.interface.AppearPrivateShop(vid, text, baslik)
	else:
		def BINARY_PrivateShop_Appear(self, vid, text):
			self.interface.AppearPrivateShop(vid, text)

	def BINARY_PrivateShop_Disappear(self, vid):
		self.interface.DisappearPrivateShop(vid)
# import chat
	# def BINARY_FakeSign_Appear(self, vid, iMountCount):
		# self.AppendChat(chat.CHAT_TYPE_INFO, "[DEBUG] BINARY_FakeSign_Appear called with vid={vid}, iMountCount={iMountCount}")
		# instance = player.GetInstance(vid)
		# if instance:
			# self.AppendChat(chat.CHAT_TYPE_INFO, "[DEBUG] Found instance for vid={vid}, attaching title")
			# instance.AttachTitle("Mount: %d" % iMountCount, 0xFFFFFF00)
		# else:
			# self.AppendChat(chat.CHAT_TYPE_ERROR, "[WARNING] No instance found for vid={vid}")

	# def BINARY_FakeSign_Appear(self, vid, iMountCount):
		# import chat

		

		# # Színek meghatározása mount count alapján
		# if 10 <= iMountCount < 20:
			# color = (0.0, 1.0, 0.0)
			# icon = "★"
		# elif 20 <= iMountCount < 25:
			# color = (1.0, 0.0, 0.0)
			# icon = "⚔"
		# elif 25 <= iMountCount < 30:
			# color = (0.0, 0.5, 1.0)
			# icon = "❖"
		# elif iMountCount >= 30:
			# color = (0.8, 0.0, 1.0)
			# icon = "♛"
		# else:
			# color = (1.0, 1.0, 0.0)
			# icon = ""

		# text = "%s Mount: %d" % (icon, iMountCount)
		# textTail.AttachMountCount(vid, text, *color)

		

		#chat.AppendChat(chat.CHAT_TYPE_INFO, "[DEBUG] FakeSign called: vid=%d, Mount=%d" % (vid, iMountCount))


	def BINARY_MountSign_AppearRazor93(self, vid, iMountCount, iBeltCount):
		import chat

		if 5 <= iMountCount < 10:
			color = (0.2, 0.5, 0.8)
			icon_char = "[Mount Collector]"
			icon_path = "d:/ymir work/razor93/mount_icon/Mount_Collector.tga"
		elif 10 <= iMountCount < 15:
			color = (0.4, 0.9, 0.9)
			icon_char = "[Mount Hunter]"
			icon_path = "d:/ymir work/razor93/mount_icon/Mount_Hunter.tga"
		elif 15 <= iMountCount < 20:
			color = (0.3, 0.8, 0.3)
			icon_char = "[Mount Warrior]"
			icon_path = "d:/ymir work/razor93/mount_icon/Mount_Warrior.tga"
		elif 20 <= iMountCount < 30:
			color = (0.6, 0.4, 0.3)
			icon_char = "[Mount Slayer]"
			icon_path = "d:/ymir work/razor93/mount_icon/Mount_Slayer.tga"
		elif 30 <= iMountCount < 40:
			color = (0.9, 0.6, 0.0)
			icon_char = "[Mount Lord]"
			icon_path = "d:/ymir work/razor93/mount_icon/Mount_Lord.tga"
		elif 40 <= iMountCount < 50:
			color = (1.0, 0.8, 0.3)
			icon_char = "[Mount Overlord]"
			icon_path = "d:/ymir work/razor93/mount_icon/Mount_Overlord.tga"
		elif 50 <= iMountCount < 60:
			color = (0.3, 0.0, 0.0)
			icon_char = "[Mount Master]"
			icon_path = "d:/ymir work/razor93/mount_icon/Mount_Master.tga"
		elif 60 <= iMountCount < 70:
			color = (0.7, 0.0, 0.0)
			icon_char = "[Mount Champion]"
			icon_path = "d:/ymir work/razor93/mount_icon/Mount_Champion.tga"
		elif 70 <= iMountCount < 80:
			color = (1.0, 0.0, 0.0)
			icon_char = "[Mount Conqueror]"
			icon_path = "d:/ymir work/razor93/mount_icon/Mount_Conqueror.tga"
		elif 80 <= iMountCount < 90:
			color = (1.0, 0.1, 0.4)
			icon_char = "[Mount Emperor]"
			icon_path = "d:/ymir work/razor93/mount_icon/Mount_Emperor.tga"
		elif 90 <= iMountCount < 100:
			color = (0.3, 0.1, 0.5)
			icon_char = "[Mount Demigod]"
			icon_path = "d:/ymir work/razor93/mount_icon/Mount_Demigod.tga"
		elif iMountCount >= 100:
			color = (0.9, 0.3, 0.8)
			icon_char = "[Mount God]"
			icon_path = "d:/ymir work/razor93/mount_icon/Mount_God.tga"
		else:
			color = (0.6, 0.6, 0.6)
			icon_char = "[New Rider]"
			icon_path = "d:/ymir work/razor93/mount_icon/New_Rider.tga"

		text = "%s Mount: %d + %s : %d" % (icon_char, iMountCount, localeinfo.BELT, iBeltCount)
		r, g, b = color
		textTail.AttachMountCountWithIcon(vid, text, r, g, b, icon_path)



	def __PRESERVE_DayMode_Update(self, mode):
		if "light"==mode:
			background.SetEnvironmentData(0)
		elif "dark"==mode:

			if not self.__IsXMasMap():
				return


			background.RegisterEnvironmentData(1, constinfo.ENVIRONMENT_NIGHT)
			background.SetEnvironmentData(1)

	#def __DayMode_Update(self, mode):
	#	if "light"==mode:
	#		self.curtain.SAFE_FadeOut(self.__DayMode_OnCompleteChangeToLight)
	#	elif "dark"==mode:
	#
	#		if not self.__IsXMasMap():
	#			return
	#
	#		self.curtain.SAFE_FadeOut(self.__DayMode_OnCompleteChangeToDark)

	def __DayMode_Update(self, mode):
		if "light"==mode:
			systemSetting.SetEnvironment(0)
		elif "dark"==mode:
			systemSetting.SetEnvironment(1)

	#def __DayMode_OnCompleteChangeToLight(self):
	#	background.SetEnvironmentData(0)
	#	self.curtain.FadeIn()
	#
	#def __DayMode_OnCompleteChangeToDark(self):
	#	background.RegisterEnvironmentData(1, constinfo.ENVIRONMENT_NIGHT)
	#	background.SetEnvironmentData(1)
	#	self.curtain.FadeIn()

	## XMasBoom
	def __XMasBoom_Update(self):

		self.BOOM_DATA_LIST = ( (2, 5), (5, 2), (7, 3), (10, 3), (20, 5) )
		if self.indexXMasBoom >= len(self.BOOM_DATA_LIST):
			return

		boomTime = self.BOOM_DATA_LIST[self.indexXMasBoom][0]
		boomCount = self.BOOM_DATA_LIST[self.indexXMasBoom][1]

		if app.GetTime() - self.startTimeXMasBoom > boomTime:

			self.indexXMasBoom += 1

			for i in xrange(boomCount):
				self.__XMasBoom_Boom()

	def __XMasBoom_Boom(self):
		x, y, z = player.GetMainCharacterPosition()
		randX = app.GetRandom(-150, 150)
		randY = app.GetRandom(-150, 150)

		snd.PlaySound3D(x+randX, -y+randY, z, "sound/common/etc/salute.mp3")

	def __PartyRequestQuestion(self, vid):
		vid = int(vid)
		partyRequestQuestionDialog = uicommon.QuestionDialog()
		partyRequestQuestionDialog.SetText(chr.GetNameByVID(vid) + localeinfo.PARTY_DO_YOU_ACCEPT)
		partyRequestQuestionDialog.SetAcceptText(localeinfo.UI_ACCEPT)
		partyRequestQuestionDialog.SetCancelText(localeinfo.UI_DENY)
		partyRequestQuestionDialog.SetAcceptEvent(lambda arg=True: self.__AnswerPartyRequest(arg))
		partyRequestQuestionDialog.SetCancelEvent(lambda arg=False: self.__AnswerPartyRequest(arg))
		partyRequestQuestionDialog.Open()
		partyRequestQuestionDialog.vid = vid
		self.partyRequestQuestionDialog = partyRequestQuestionDialog

	def __AnswerPartyRequest(self, answer):
		if not self.partyRequestQuestionDialog:
			return

		vid = self.partyRequestQuestionDialog.vid

		if answer:
			net.SendChatPacket("/party_request_accept " + str(vid))
		else:
			net.SendChatPacket("/party_request_deny " + str(vid))

		self.partyRequestQuestionDialog.Close()
		self.partyRequestQuestionDialog = None

	def __PartyRequestDenied(self):
		self.PopupMessage(localeinfo.PARTY_REQUEST_DENIED)

	def __EnableTestServerFlag(self):
		app.EnableTestServerFlag()

	def __InGameShop_Show(self, url):
		if constinfo.IN_GAME_SHOP_ENABLE:
			self.interface.OpenWebWindow(url)

	# WEDDING
	def __LoginLover(self):
		if self.interface.wndMessenger:
			self.interface.wndMessenger.OnLoginLover()

	def __LogoutLover(self):
		if self.interface.wndMessenger:
			self.interface.wndMessenger.OnLogoutLover()
		if self.affectShower:
			self.affectShower.HideLoverState()

	def __LoverNear(self):
		if self.affectShower:
			self.affectShower.ShowLoverState()

	def __LoverFar(self):
		if self.affectShower:
			self.affectShower.HideLoverState()

	def __LoverDivorce(self):
		if self.interface.wndMessenger:
			self.interface.wndMessenger.ClearLoverInfo()
		if self.affectShower:
			self.affectShower.ClearLoverState()

	def __PlayMusic(self, flag, filename):
		flag = int(flag)
		if flag:
			snd.FadeOutAllMusic()
			musicinfo.SaveLastPlayFieldMusic()
			snd.FadeInMusic("BGM/" + filename)
		else:
			snd.FadeOutAllMusic()
			musicinfo.LoadLastPlayFieldMusic()
			snd.FadeInMusic("BGM/" + musicinfo.fieldMusic)
	
	if app.ENABLE_ACCE_SYSTEM:
		def ActAcce(self, iAct, bWindow):
			if self.interface:
				self.interface.ActAcce(iAct, bWindow)

		def AlertAcce(self, bWindow):
			snd.PlaySound("sound/ui/make_soket.wav")
			if bWindow:
				self.PopupMessage(localeinfo.ACCE_DEL_SERVEITEM)
			else:
				self.PopupMessage(localeinfo.ACCE_DEL_ABSORDITEM)
	
	if app.ENABLE_DEFENSE_WAVE:
		def BINARY_Update_Mast_HP(self, hp):
			self.interface.BINARY_Update_Mast_HP(int(hp))

		def BINARY_Update_Mast_Timer(self, text):
			self.interface.BINARY_Update_Mast_Timer(text)

		def BINARY_Update_Mast_Window(self, i):
			self.interface.BINARY_Update_Mast_Window(int(i))
	
	if app.ENABLE_MELEY_LAIR_DUNGEON:
		def OpenMeleyRanking(self):
			if self.interface:
				self.interface.OpenMeleyRanking()

		def AddRankMeleyRanking(self, data):
			if self.interface:
				line = int(data.split("#")[1])
				name = str(data.split("#")[2])
				members = int(data.split("#")[3])
				seconds = int(data.split("#")[4])
				minutes = seconds // 60
				seconds %= 60
				if seconds > 0:
					time = localeinfo.TIME_MIN_SEC % (minutes, seconds)
				else:
					time = localeinfo.TIME_MIN % (minutes)
				
				self.interface.RankMeleyRanking(line, name, members, time)

	
	
	# END_OF_WEDDING
	
	if app.ENABLE_SWITCHBOT:
		def RefreshSwitchbotWindow(self):
			if self.interface:
				self.interface.RefreshSwitchbotWindow()

		def RefreshSwitchbotItem(self, slot):
			if self.interface:
				self.interface.RefreshSwitchbotItem(slot)

	def ManagerGiftSystem(self, cmd):
		cmd = cmd.split("|")
		if cmd[0] == "Show":
			self.wnddailygift.Open()
		elif cmd[0] == "DeleteRewards":
			self.wnddailygift.DeleteRewards()
		elif cmd[0] == "SetDailyReward":
			self.wnddailygift.SetDailyReward(cmd[1])
		elif cmd[0] == "SetTime":
			self.wnddailygift.SetTime(cmd[1])
		elif cmd[0] == "SetReward":
			self.wnddailygift.SetReward(cmd[1], cmd[2])
		elif cmd[0] == "SetRewardDone":
			self.wnddailygift.SetRewardDone()

	if app.ENABLE_GAYA_SYSTEM:
		def OpenGuiGaya(self):
			self.interface.OpenGuiGaya()

		def GayaCheck(self):
			self.interface.GayaCheck()

		def OpenGuiGayaMarket(self):
			self.interface.OpenGuiGayaMarket()

		def GayaMarketItems(self,vnums,gaya,count):
			self.interface.GayaMarketItems(vnums,gaya,count)

		def GayaMarketSlotsDesblock(self,slot0,slot1,slot2,slot3,slot4,slot5):
			self.interface.GayaMarketSlotsDesblock(slot0,slot1,slot2,slot3,slot4,slot5)

		def GayaMarketClear(self):
			self.interface.GayaMarketClear()

		def GayaTimeMarket(self, time):
			self.interface.GayaTime(time)
			
	if app.ENABLE_HIDE_COSTUME_SYSTEM:
		def SetBodyCostumeHidden(self, hidden):
			constinfo.HIDDEN_BODY_COSTUME = int(hidden)
			self.interface.RefreshVisibleCostume()

		def SetHairCostumeHidden(self, hidden):
			constinfo.HIDDEN_HAIR_COSTUME = int(hidden)
			self.interface.RefreshVisibleCostume()

		def SetAcceCostumeHidden(self, hidden):
			if app.ENABLE_ACCE_COSTUME_SYSTEM:
				constinfo.HIDDEN_ACCE_COSTUME = int(hidden)
				self.interface.RefreshVisibleCostume()
			else:
				pass

		def SetWeaponCostumeHidden(self, hidden):
			if app.ENABLE_WEAPON_COSTUME_SYSTEM:
				constinfo.HIDDEN_WEAPON_COSTUME = int(hidden)
				self.interface.RefreshVisibleCostume()
			else:
				pass
			
			
	if app.__ENABLE_EXTEND_INVEN_SYSTEM__:	
		def Update_inventory_ref(self):
			if self.interface:
				self.interface.SetInventoryPageLock()

		def Update_inventory_need(self, need):
			self.wndPopupDialog = uicommon.PopupDialog()
			self.wndPopupDialog.SetText(localeinfo.ENVANTER_KEY_NEED % int(need))
			self.wndPopupDialog.Open()

	if app.ENABLE_BATTLE_PASS:
		def OpenBattlePassWindow(self):
			net.SendBattlePassAction(1)

		def BINARY_BattlePassOpen(self):
			if self.interface:
				self.interface.OpenBattlePass()

		def BINARY_BattlePassAddMission(self, missionType, missionInfo1, missionInfo2, missionInfo3):
			if self.interface:
				self.interface.AddBattlePassMission(missionType, missionInfo1, missionInfo2, missionInfo3)
				
		def BINARY_BattlePassAddMissionReward(self, missionType, itemVnum, itemCount):
			if self.interface:
				self.interface.AddBattlePassMissionReward(missionType, itemVnum, itemCount)
				
		def BINARY_BattlePassUpdate(self, missionType, newProgress):
			if self.interface:
				self.interface.UpdateBattlePassMission(missionType, newProgress)
				
		def BINARY_BattlePassAddReward(self, itemVnum, itemCount):
			if self.interface:
				self.interface.AddBattlePassReward(itemVnum, itemCount)
				
		def BINARY_BattlePassAddRanking(self, pos, playerName, finishTime):
			if self.interface:
				self.interface.AddBattlePassRanking(pos, playerName, finishTime)
				
		def BINARY_BattlePassRefreshRanking(self):
			if self.interface:
				self.interface.RefreshBattlePassRanking()
				
		def BINARY_BattlePassOpenRanking(self):
			if self.interface:
				self.interface.OpenBattlePassRanking()
				
	if app.NEW_PET_SYSTEM:
		def SetPetEvolution(self, evo):
			petname = [localeinfo.PET_MISSING1_1, localeinfo.PET_MISSING1_2, localeinfo.PET_MISSING1_3, localeinfo.PET_MISSING1_4]
			self.petmain.SetEvolveName(petname[int(evo)])
			constinfo.PETMINIEVO = int(evo)
			if app.ENABLE_PVP_ADVANCED:
				if self.wndDuelLive:
					self.wndDuelLive.UpdatePosition(True)

		def SetPetName(self, name):
			if len(name) > 1 and name != "":
				self.petmini.Show()
			self.petmain.SetName(name)
		
		def SetPetLevel(self, level):
			self.petmain.SetLevel(level)
			constinfo.PETMINILEVEL = int(level)
		
		def SetPetDuration(self, dur, durt):
			if int(durt) > 0:
				self.petmini.SetDuration(dur, durt)
			self.petmain.SetDuration(dur, durt)

		def PetAge(self, arg):
			self.petmain.SetAge(int(arg))

		def SetPetBonus(self, hp, dif, sp):
			self.petmain.SetHp(hp)
			self.petmain.SetDef(dif)
			self.petmain.SetSp(sp)
			
		def SetPetskill(self, slot, idx, lv):
			self.petmini.SetSkill(slot, idx, lv)
			self.petmain.SetSkill(slot, idx, lv)
			if int(slot) == 0:
				if self.affectShower:
					self.affectShower.BINARY_NEW_RemoveAffect(int(constinfo.SKILL_PET1), 0)
				
				constinfo.SKILL_PET1 = 5400 + int(idx)
			
			if int(slot) == 1:
				if self.affectShower:
					self.affectShower.BINARY_NEW_RemoveAffect(int(constinfo.SKILL_PET2), 0)
				
				constinfo.SKILL_PET2 = 5400 + int(idx)
				
			if int(slot) == 2:
				if self.affectShower:
					self.affectShower.BINARY_NEW_RemoveAffect(int(constinfo.SKILL_PET3), 0)
				
				constinfo.SKILL_PET3 = 5400 + int(idx)
			
			if int(slot) == 3:
				if self.affectShower:
					self.affectShower.BINARY_NEW_RemoveAffect(int(constinfo.SKILL_PET4), 0)
				
				constinfo.SKILL_PET4 = 5400 + int(idx)
			
			if int(idx) > 0:
				if self.affectShower:
					self.affectShower.BINARY_NEW_AddAffect(5400+int(idx), int(constinfo.LASTAFFECT_POINT)+1, int(constinfo.LASTAFFECT_VALUE)+1, 0)

		def SetPetIcon(self, vnum):
			if int(vnum) > 0:
				self.petmini.SetImageSlot(vnum)
			self.petmain.SetImageSlot(vnum)
			
		def SetPetExp(self, exp, expi, exptot):
			if int(exptot) > 0:
				self.petmini.SetExperience(exp, expi, exptot)
			self.petmain.SetExperience(exp, expi, exptot)
			
		def PetUnsummon(self):
			self.petmini.SetDefaultInfo()
			self.petmini.Close()
			self.petmain.SetDefaultInfo()
			self.affectShower.BINARY_NEW_RemoveAffect(int(constinfo.SKILL_PET1), 0)
			self.affectShower.BINARY_NEW_RemoveAffect(int(constinfo.SKILL_PET2), 0)
			self.affectShower.BINARY_NEW_RemoveAffect(int(constinfo.SKILL_PET3), 0)
			self.affectShower.BINARY_NEW_RemoveAffect(int(constinfo.SKILL_PET4), 0)
			constinfo.SKILL_PET1 = 0
			constinfo.SKILL_PET2 = 0
			constinfo.SKILL_PET3 = 0
			constinfo.SKILL_PET4 = 0
			if app.ENABLE_PVP_ADVANCED:
				if self.wndDuelLive:
					self.wndDuelLive.UpdatePosition(False)

		def OpenPetMainGui(self):
			if not self.petmain.IsShow():
				self.petmain.Show()
				self.petmain.SetTop()
			else:
				self.petmain.Close()

		def OpenPetIncubator(self, pet_new = 0):
			import uipetincubatrice
			self.petinc = uipetincubatrice.PetSystemIncubator(pet_new)
			self.petinc.Show()
			self.petinc.SetTop()

		def OpenPetMini(self):
			self.petmini.Show()
			self.petmini.SetTop()

		def OpenPetFeed(self):
			self.feedwind = uipetfeed.PetFeedWindow()
			self.feedwind.Show()
			self.feedwind.SetTop()

	if app.ENABLE_MULTI_LANGUAGE:
		def BINARY_RequestChangeLanguage(self, language):
			serverLang = constinfo.TRANSFORM_LANG(language)
			if app.GetLocaleName() != serverLang:
				net.SendChangeLanguagePacket(constinfo.TRANSFORM_LANG(app.GetLocaleName()))

		def BINARY_RecvTargetLang(self, name, lang):
			if self.interface:
				self.interface.RecvLangOnWhisper(name, lang)

	if app.ENABLE_WHISPER_ADMIN_SYSTEM:
		def OnRecvWhisperAdminSystem(self, name, line, color):

			def ExistCustomColor(val):
				return (val > 0)

			def GetColor(type):
				WHISPER_COLOR_MESSAGE = {
					0: "|cffffffff|H|h",
					1: "|cffff796a|H|h",
					2: "|cffb1ff80|H|h",
					3: "|cff46deff|H|h"
				}
				return WHISPER_COLOR_MESSAGE[type]

			def ResizeTextWithColor(color, text):
				return str("%s%s|h|r" % (GetColor(color), text))
			
			import datetime
			now = datetime.datetime.now()
			ret = line.replace("#", " ")
			line2 = ret
			
			if ExistCustomColor(int(color)):
				ret = ResizeTextWithColor(int(color), ret)
			else:
				ret = ResizeTextWithColor(0, ret)
			
			time = now.strftime("%Y-%m-%d %H:%M")
			text = localeinfo.WHISPER_ADMIN_MESSAGE % (time, ret)
			
			self.interface.RegisterGameMasterName(name)
			chat.AppendWhisper(chat.WHISPER_TYPE_CHAT, name, text)
			self.interface.RecvWhisper(name)

	#def OpenTeleporter(self):
	#	if self.interface:
	#		self.interface.ToggleMapTeleporter()

	def OpenSavepoint(self):
		if self.interface:
			self.interface.ClickBtnSavepoint()

	if app.ENABLE_EXTRA_INVENTORY:
		def OpenExtrainventory(self):
			if self.interface:
				self.interface.ToggleExtraInventoryWindow()

	def OpenDungeoninfo(self):
			if self.interface:
				self.interface.ToggleDungeonInfoWindow()

	if app.ENABLE_WHISPER_ADMIN_SYSTEM:
		def OpenWhisperSystem(self):
			net.SendChatPacket("/open_whispersys")

		def RecvWhisperSystem(self):
			if self.interface:
				self.interface.OpenWhisperSystem()

		def BINARY_RecieveWhisperInfo(self, name, language, empire):
			if self.interface:
				self.interface.RecieveWhisperDetails(name, language, empire)

	if app.INGAME_WIKI:
		def ToggleWikiWindow(self):
			if not self.wndWiki:
				return
			
			if self.wndWiki.IsShow():
				self.wndWiki.Hide()
			else:
				self.wndWiki.Show()
				self.wndWiki.SetTop()

	if app.WJ_ENABLE_TRADABLE_ICON:
		def BINARY_AddItemToExchange(self, inven_type, inven_pos, display_pos):
			if inven_type == player.INVENTORY:
				self.interface.CantTradableItemExchange(display_pos, inven_pos)
			elif inven_type == player.EXTRA_INVENTORY:
				self.interface.CantTradableExtraItemExchange(display_pos, inven_pos)

	if app.ENABLE_SAVEPOINT_SYSTEM:
		def DoAppendSavepoint(self, id, name, mapIndex, x, y):
			if self.interface:
				self.interface.AppendSavepoint(id, name, mapIndex, x, y)

		def DoOpenSavepoint(self):
			if self.interface:
				self.interface.OpenSavepoint()

		def DoUpdateSavepoint(self, id, name, mapIndex, x, y):
			if self.interface:
				self.interface.UpdateSavepoint(id, name, mapIndex, x, y)
	#if app.ENABLE_CHRISTMAS_WHEEL_OF_DESTINY:
	def BINARY_WHEEL_ASKOPEN(self):
		if not self.wheeldestiny.IsShow():
			net.WheelPacket(0)

	def BINARY_WHEEL_OPEN(self):
		if self.wheeldestiny:
			self.wheeldestiny.Open(0, 0)

	def BINARY_WHEEL_CLOSE(self):
		if self.wheeldestiny:
			self.wheeldestiny.Close()

	def BINARY_WHEEL_TURN(self, item, count, spin, number):
		if self.wheeldestiny.IsShow():
			self.wheeldestiny.TurnWheel(item, count, spin, number)

	def BINARY_WHEEL_ICON(self, item, count, number):
		if self.wheeldestiny:
			self.wheeldestiny.SetIcons(item, count, number)
	if app.ENABLE_EVENT_MANAGER:
		def ClearEventManager(self):
			self.interface.ClearEventManager()
		def RefreshEventManager(self):
			self.interface.RefreshEventManager()
		def RefreshEventStatus(self, eventID, eventStatus, eventendTime, eventEndTimeText):
			self.interface.RefreshEventStatus(int(eventID), int(eventStatus), int(eventendTime), str(eventEndTimeText))
		def AppendEvent(self, dayIndex, eventID, eventIndex, startTime, endTime, empireFlag, channelFlag, value0, value1, value2, value3, startRealTime, endRealTime, isAlreadyStart):
			self.interface.AppendEvent(int(dayIndex),int(eventID), int(eventIndex), str(startTime), str(endTime), int(empireFlag), int(channelFlag), int(value0), int(value1), int(value2), int(value3), int(startRealTime), int(endRealTime), int(isAlreadyStart))

	if app.ENABLE_VOTE4BUFF:
		def CheckVote4Buff(self, url):
			result = app.HttpRequest(url)
			if result != "":
				try:
					startPos = result.find("status")
					if startPos != -1:
						status = int(result[startPos+len('status":"'):startPos+len('status":"') + 1])
				except:
					status = 0
				if status == 0:
					return "2"
				elif status == 2:
					return "3"
				elif status == 1:
					return "4"
				return "5"
			return "0"
		def GetVote4BuffResult(self, url):
			net.SendQuestInputStringPacket(self.CheckVote4Buff(url))

	if app.__BL_PICK_FILTER__:
		def OpenPickUpWindow(self):
			if self.interface:
				self.interface.OpenPickUpWindow()
				
	def BINARY_RefreshMountInventory(self):
		if self.interface:
			self.interface.RefreshMountInventory()
