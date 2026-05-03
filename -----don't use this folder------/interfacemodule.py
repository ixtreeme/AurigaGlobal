import os
import app
import player
import net
import uimarketsystem
import constinfo
import systemSetting
import wndMgr
import chat
import grp
import uitaskbar
import uicharacter
import uiinventory
import uidragonsoul
import uichat
import uimessenger
import guild
import game
import ui
import uiwhisper
import uipointreset
import uishop
import uiexchange
import uisystem
import uirestart
import uitooltip
import uiminimap
import uiparty
import uisafebox
import uiguild
import uiguildrenewal
import uiquest
import uiprivateshopbuilder
import uicommon
import uirefine
import uiequipmentdialog
import uigamebutton
import uitip
import uicube
import miniMap
if app.AUTO_SHOUT:
	import uishout
# ACCESSORY_REFINE_ADD_METIN_STONE
import uiselectitem
# END_OF_ACCESSORY_REFINE_ADD_METIN_STONE
import uiscriptlocale
import event
import whispermanager
import localeinfo
if app.__ENABLE_NEW_OFFLINESHOP__:
	import offlineshop
	import uiofflineshop
if app.ENABLE_SWITCHBOT:
	import uiswitchbot
if app.ENABLE_MELEY_LAIR_DUNGEON:
	import uidragonlairranking
if app.ENABLE_ACCE_SYSTEM:
	import uiacce
if app.ENABLE_BATTLE_PASS:
	import uibattlepass
if app.ENABLE_GAYA_SYSTEM:
	import uisystemgems
if app.ENABLE_EXTRA_INVENTORY:
	import uiextrainventory
	import uistonecraft
if app.ENABLE_ATTR_TRANSFER_SYSTEM:
	import uiattrtransfer
if app.ENABLE_DUNGEON_INFO_SYSTEM:
	import uidungeoninfo
if app.ENABLE_RANKING:
	import uiranking
if app.ENABLE_RUNE_SYSTEM:
	import uirune
if app.ADVANCED_GUILD_INFO:
	import uiguildrank
if app.ENABLE_BIOLOGIST_UI:
	import uibiolog
if app.ENABLE_NEW_FISHING_SYSTEM:
	import uifishing
if app.ENABLE_SAVEPOINT_SYSTEM:
	import uisavepoint
if app.ENABLE_CHOOSE_DOCTRINE_GUI:
	import uidoctrinechoose
if app.LINK_IN_CHAT:
	import os
if app.ENABLE_EVENT_MANAGER:
	import uiEventCalendar
if app.ENABLE_ITEMSHOP:
	import uiItemShop
if app.__BL_PICK_FILTER__:
	import uiPickUpFilter

import uimountinventory

IsQBHide = 0
class Interface(object):
	CHARACTER_STATUS_TAB = 1
	CHARACTER_SKILL_TAB = 2

	class NewGoldChat(ui.Window):
		def __init__(self, parent = None, x = 0, y = 0):
			ui.Window.__init__(self)
			self.texts = {}
			self.parent = parent
			self.SpaceBet = 14
			self.maxY = 0
			self.x = x
			self.y = y
			self.ColorValue = 0xFFFFFFFF
			
			self.show = self.Button('Show Yang', x, y+3, self.showYang, 'sanyed_yang/btn_expand_normal.tga', 'sanyed_yang/btn_expand_over.tga', 'sanyed_yang/btn_expand_down.tga')
			self.hide = self.Button('Hide Yang', x, y+3, self.hideYang, 'sanyed_yang/btn_minimize_normal.tga', 'sanyed_yang/btn_minimize_over.tga', 'sanyed_yang/btn_minimize_down.tga')
			self.show.Hide()
			self.Show()

		def Button(self, tooltipText, x, y, func, UpVisual, OverVisual, DownVisual):
			button = ui.Button()
			if self.parent != None:
				button.SetParent(self.parent)
			button.SetPosition(x, y)
			button.SetUpVisual(UpVisual)
			button.SetOverVisual(OverVisual)
			button.SetDownVisual(DownVisual)
			button.SetToolTipText(tooltipText)
			button.Show()
			button.SetEvent(func)
			return button

		def showYang(self):
			for i in xrange(len(self.texts)):
				self.texts[i].Show()
			self.hide.Show()
			self.show.Hide()
			self.Show()

		def hideYang(self):
			for i in xrange(len(self.texts)):
				self.texts[i].Hide()
			self.hide.Hide()
			self.show.Show()
			self.Hide()
			
		def GetMaxY(self):
			return self.maxY

		def AddGoldValue(self, text):
			for i in xrange(len(self.texts)):
				if len(self.texts) == 10 and i == 0:
					self.texts[i].Hide()
				x, y = self.texts[i].GetLocalPosition()
				self.texts[i].SetPosition(x, y-self.SpaceBet)

			i = 0
			if len(self.texts) == 10:
				for i in xrange(len(self.texts)-1):
					self.texts[i] = self.texts[i+1]
				i = 9
			else:
				i = len(self.texts)
			
			self.texts[i] = ui.TextLine("Tahoma:14")
			if self.parent != None:
				self.texts[i].SetParent(self.parent)
			self.texts[i].SetPosition(self.x, self.y)
			self.texts[i].SetPackedFontColor(self.ColorValue)
			self.texts[i].SetHorizontalAlignLeft()
			self.texts[i].SetOutline(TRUE)
			self.texts[i].SetText(text)
			if self.hide.IsShow():
				self.texts[i].Show()

			if self.hide.IsShow():
				x, y = self.texts[0].GetLocalPosition()
				x2, y2 = self.hide.GetLocalPosition()
				self.hide.SetPosition(x2, y-10)

		def ClearAll(self):
			self.Hide()
			self.texts = {}
			self.show.Hide()
			self.show = None
			self.hide.Hide()
			self.hide = None

		def OnRender(self):
			if len(self.texts) > 0 and self.hide.IsShow():
				x, y = self.hide.GetGlobalPosition()
				w, h = self.texts[0].GetTextSize()
				grp.SetColor(grp.GenerateColor(0.0, 0.0, 0.0, 0.5))
				grp.RenderBar(x, y+h-6, 108, h*len(self.texts)+4)

	def __init__(self):
		self.class_game = None
		systemSetting.SetInterfaceHandler(self)
		self.windowOpenPosition = 0
		if app.WJ_ENABLE_TRADABLE_ICON:
			self.onTopWindow = player.ON_TOP_WND_NONE
		
		self.dlgWhisperWithoutTarget = None
		self.inputDialog = None
		self.tipBoard = None
		self.bigBoard = None
		if app.ENABLE_ITEMSHOP:
			self.wndItemShop=None

		# ITEM_MALL
		self.mallPageDlg = None
		# END_OF_ITEM_MALL

		if app.ADVANCED_GUILD_INFO:
			self.wndRank = None

		self.wndWeb = None
		if app.ENABLE_CUBE_RENEWAL_WORLDARD:
			self.wndCubeRenewal = None
		self.wndTaskBar = None
		self.wndCharacter = None
		self.wndInventory = None
		self.wndExpandedTaskBar = None
		if app.ENABLE_CAPITALE_SYSTEM:
			self.wndExpandedMoneyTaskbar = None
		self.wndDragonSoul = None
		self.wndDragonSoulRefine = None
		self.wndChat = None
		self.yangText = None
		self.wndMessenger = None
		self.wndMiniMap = None
		self.wndGuild = None
		self.wndGuildRenewal = None
		self.wndGuildBuilding = None
		if app.LINK_IN_CHAT:
			self.openLinkQuestionDlg = None
		if app.__BL_PICK_FILTER__:
			self.wndPickUpFilter = None
		self.wndOfflineshop = None
		if app.ENABLE_NEW_FISHING_SYSTEM:
			self.wndFishingWindow = None
		if app.ENABLE_EXTRA_INVENTORY:
			self.wndExtraInventory = None
			self.wndStoneCraft = None
		if app.ENABLE_SWITCHBOT:
			self.wndSwitchbot = None
		if app.ENABLE_BATTLE_PASS:
			self.wndBattlePass = None
			self.wndBattlePassButton = None
		if app.ENABLE_DUNGEON_INFO_SYSTEM:
			self.wndDungeonInfo = None
		
		if app.ENABLE_RANKING:
			self.wndRanking = None
		
		if app.ENABLE_RUNE_SYSTEM:
			self.wndRune = None
		if app.ENABLE_WHISPER_ADMIN_SYSTEM:
			self.wndWhisperManager = None
		event.SetInterfaceWindow(self)
		self.listGMName = {}
		if app.ENABLE_MULTI_LANGUAGE:
			self.listLangName = {}

		self.wndMountInventory = None

		self.wndQuestWindow = {}
		self.wndQuestWindowNewKey = 0
		self.privateShopAdvertisementBoardDict = {}
		self.guildScoreBoardDict = {}
		self.equipmentDialogDict = {}

		self.wndChatLog = None
		if app.ENABLE_BIOLOGIST_UI:
			self.wndBiologist = None
		if app.ENABLE_SAVEPOINT_SYSTEM:
			self.wndSavePoint = None
		if app.ENABLE_CHOOSE_DOCTRINE_GUI:
			self.wndDoctrine = None
		if app.ENABLE_EVENT_MANAGER:
			self.wndEventManager = None
			self.wndEventIcon = None
		
		event.SetInterfaceWindow(self)

	def __del__(self):
		systemSetting.DestroyInterfaceHandler()
		event.SetInterfaceWindow(None)
	if app.AUTO_SHOUT:
		def __MakeShoutWindow(self):
			self.wndShout = uishout.ShoutManager()
			self.wndShout.LoadWindow()
			self.wndShout.Hide()
	def BindGameClass(self, game):
		self.class_game = game

	################################
	## Make Windows & Dialogs
	def __MakeUICurtain(self):
		wndUICurtain = ui.Bar("TOP_MOST")
		wndUICurtain.SetSize(wndMgr.GetScreenWidth(), wndMgr.GetScreenHeight())
		wndUICurtain.SetColor(0x77000000)
		wndUICurtain.Hide()
		self.wndUICurtain = wndUICurtain
	if app.ENABLE_REFINE_RENEWAL:
		def CheckRefineDialog(self, isFail):
			self.dlgRefineNew.CheckRefine(isFail)
			
	def __MakeMessengerWindow(self):
		self.wndMessenger = uimessenger.MessengerWindow()

		from _weakref import proxy
		self.wndMessenger.SetWhisperButtonEvent(lambda n,i=proxy(self):i.OpenWhisperDialog(n))
		self.wndMessenger.SetGuildButtonEvent(ui.__mem_func__(self.ToggleGuildWindow))

	def __MakeGuildWindow(self):
		self.wndGuild = uiguild.GuildWindow()

	def __MakeGuildRenewalWindow(self):
		self.wndGuildRenewal = uiguildrenewal.GuildRenewalWindow()
		self.wndGuildRenewal.Hide()

	def __MakeChatWindow(self):
		wndChat = uichat.ChatWindow(self)
		wndChat.SetSize(wndChat.CHAT_WINDOW_WIDTH, 0)
		wndChat.SetPosition(wndMgr.GetScreenWidth()/2 - wndChat.CHAT_WINDOW_WIDTH/2, wndMgr.GetScreenHeight() - wndChat.EDIT_LINE_HEIGHT - 37)
		wndChat.SetHeight(200)
		wndChat.Refresh()
		wndChat.Show()

		self.wndChat = wndChat
		self.wndChat.SetSendWhisperEvent(ui.__mem_func__(self.OpenWhisperDialogWithoutTarget))
		self.wndChat.SetOpenChatLogEvent(ui.__mem_func__(self.ToggleChatLogWindow))
		if self.yangText:
			self.yangText.Hide()
		yangText = self.NewGoldChat(None, wndMgr.GetScreenWidth()/2 - wndChat.CHAT_WINDOW_WIDTH/2 + 600, wndMgr.GetScreenHeight() - wndChat.EDIT_LINE_HEIGHT - 37 + 9)
		self.yangText = yangText

	def OnPickMoneyNew(self, money):
		self.yangText.AddGoldValue("+%s"%(localeinfo.NumberToMoneyString(money)))

	def __MakeTaskBar(self):
		wndTaskBar = uitaskbar.TaskBar()
		wndTaskBar.LoadWindow()
		self.wndTaskBar = wndTaskBar
		self.wndTaskBar.SetToggleButtonEvent(uitaskbar.TaskBar.BUTTON_CHARACTER, ui.__mem_func__(self.ToggleCharacterWindowStatusPage))
		self.wndTaskBar.SetToggleButtonEvent(uitaskbar.TaskBar.BUTTON_INVENTORY, ui.__mem_func__(self.ToggleInventoryWindow))
		self.wndTaskBar.SetToggleButtonEvent(uitaskbar.TaskBar.BUTTON_MESSENGER, ui.__mem_func__(self.ToggleMessenger))
		self.wndTaskBar.SetToggleButtonEvent(uitaskbar.TaskBar.BUTTON_SYSTEM, ui.__mem_func__(self.ToggleSystemDialog))
		self.wndTaskBar.SetToggleButtonEvent(uitaskbar.TaskBar.BUTTON_WEBOVE_STRANKY, ui.__mem_func__(self.OtevritWeb))
		self.wndTaskBar.SetToggleButtonEvent(uitaskbar.TaskBar.BUTTON_EXTENDED_INV, ui.__mem_func__(self.ExtInvRazor93))
		self.wndTaskBar.SetToggleButtonEvent(uitaskbar.TaskBar.BUTTON_BATTLEPASS, ui.__mem_func__(self.OpenBattlePass))
		self.wndTaskBar.SetToggleButtonEvent(uitaskbar.TaskBar.BUTTON_SWITCHBOT, ui.__mem_func__(self.ToggleSwitchbotWindow))
		self.wndTaskBar.SetToggleButtonEvent(uitaskbar.TaskBar.BUTTON_RANKING, ui.__mem_func__(self.OpenRanking))
		self.wndTaskBar.SetToggleButtonEvent(uitaskbar.TaskBar.BTN_DAILYGIFT, ui.__mem_func__(self.ToggleWikiNew))
		self.wndTaskBar.SetToggleButtonEvent(uitaskbar.TaskBar.BUTTON_BIO, ui.__mem_func__(self.ClickBiologistButton))
		self.wndTaskBar.SetToggleButtonEvent(uitaskbar.TaskBar.BUTTON_DUNGEON, ui.__mem_func__(self.ToggleDungeonInfoWindow))
		if app.ENABLE_CAPITALE_SYSTEM:
			self.wndTaskBar.SetToggleButtonEvent(uitaskbar.TaskBar.BUTTON_EXPAND_MONEY, ui.__mem_func__(self.ToggleExpandedMoneyButton))

		if uitaskbar.TaskBar.IS_EXPANDED:
			self.wndTaskBar.SetToggleButtonEvent(uitaskbar.TaskBar.BUTTON_EXPAND, ui.__mem_func__(self.ToggleExpandedButton))
			self.wndExpandedTaskBar = uitaskbar.ExpandedTaskBar()
			self.wndExpandedTaskBar.LoadWindow()
			self.wndExpandedTaskBar.SetToggleButtonEvent(uitaskbar.ExpandedTaskBar.BUTTON_DRAGON_SOUL, ui.__mem_func__(self.ToggleDragonSoulWindow))

		else:
			self.wndTaskBar.SetToggleButtonEvent(uitaskbar.TaskBar.BUTTON_CHAT, ui.__mem_func__(self.ToggleChat))
	
		self.wndEnergyBar = None
		if app.ENABLE_ENERGY_SYSTEM:
			wndEnergyBar = uitaskbar.EnergyBar()
			wndEnergyBar.LoadWindow()
			self.wndEnergyBar = wndEnergyBar

	def DailyGiftOpen(self):
		if self.uiminimap:
			dailygift = self.uiminimap.wnddailygift
			if dailygift:
				if dailygift.IsShow():
					dailygift.Close()
				else:
					dailygift.Open()

	def __MakeParty(self):
		wndParty = uiparty.PartyWindow()
		wndParty.Hide()
		self.wndParty = wndParty

	if app.ENABLE_CHOOSE_DOCTRINE_GUI:
		def OnRecvDoctrine(self):
			if self.wndDoctrine and not self.wndDoctrine.IsShow():
				self.wndDoctrine.Open()

		def __OnClickDoctrineButton(self):
			if self.wndDoctrine:
				if self.wndDoctrine.IsShow():
					self.wndDoctrine.Close()
				else:
					self.wndDoctrine.Open()


	def OtevritWeb(self):
		if self.wndOfflineshop.IsShow():
			self.wndOfflineshop.Close()
		else:
			self.wndOfflineshop.Open()

	def ExtInvRazor93(self):
		if not player.IsObserverMode():
			if not self.wndExtraInventory.IsShow():
				self.wndExtraInventory.Open()
				self.wndExtraInventory.SetTop()
			else:
				self.wndExtraInventory.Close()

		# def ToggleSwitchbotWindow(self):
			# if self.wndSwitchbot.IsShow():
				# self.wndSwitchbot.Close()
			# else:
				# self.wndSwitchbot.Open()

	def __MakeGameButtonWindow(self):
		wndGameButton = uigamebutton.GameButtonWindow()
		wndGameButton.SetTop()
		wndGameButton.Show()
		wndGameButton.SetButtonEvent("STATUS", ui.__mem_func__(self.__OnClickStatusPlusButton))
		wndGameButton.SetButtonEvent("SKILL", ui.__mem_func__(self.__OnClickSkillPlusButton))
		wndGameButton.SetButtonEvent("QUEST", ui.__mem_func__(self.__OnClickQuestButton))
		wndGameButton.SetButtonEvent("BUILD", ui.__mem_func__(self.__OnClickBuildButton))
		if app.ENABLE_CHOOSE_DOCTRINE_GUI:
			wndGameButton.SetButtonEvent("DOCTRINE", ui.__mem_func__(self.__OnClickDoctrineButton))

		self.wndGameButton = wndGameButton

	def __IsChatOpen(self):
		return True

	def __MakeWindows(self):
		wndCharacter = uicharacter.CharacterWindow()
		wndInventory = uiinventory.InventoryWindow()
		wndInventory.BindInterfaceClass(self)
		if app.ENABLE_CAPITALE_SYSTEM:
			self.wndExpandedMoneyTaskbar = uiinventory.ExpandedMoneyTaskBar()
			self.wndExpandedMoneyTaskbar.LoadWindow()
		if app.ENABLE_DRAGON_SOUL_SYSTEM:
			wndDragonSoul = uidragonsoul.DragonSoulWindow()
			wndDragonSoulRefine = uidragonsoul.DragonSoulRefineWindow()
		else:
			wndDragonSoul = None
			wndDragonSoulRefine = None
		if app.ENABLE_SWITCHBOT:
			self.wndSwitchbot = uiswitchbot.SwitchbotWindow()
		self.wndOfflineshop = uiofflineshop.NewOfflineShopBoard()
		wndMiniMap = uiminimap.MiniMap()
		
		wndSafebox = uisafebox.SafeboxWindow()
		if app.WJ_ENABLE_TRADABLE_ICON:
			wndSafebox.BindInterface(self)
		
		# ITEM_MALL
		wndMall = uisafebox.MallWindow()
		self.wndMall = wndMall
		# END_OF_ITEM_MALL

		wndChatLog = uichat.ChatLogWindow(self)
		self.wndCharacter = wndCharacter
		self.wndInventory = wndInventory
		self.wndDragonSoul = wndDragonSoul
		self.wndDragonSoulRefine = wndDragonSoulRefine
		self.wndMiniMap = wndMiniMap
		self.wndSafebox = wndSafebox
		self.wndChatLog = wndChatLog
		if app.__BL_PICK_FILTER__:
			self.wndPickUpFilter = uiPickUpFilter.PickUpFilterWindow()

		if app.ENABLE_DRAGON_SOUL_SYSTEM:
			self.wndDragonSoul.SetDragonSoulRefineWindow(self.wndDragonSoulRefine)
			self.wndDragonSoulRefine.SetInventoryWindows(self.wndInventory, self.wndDragonSoul)
			self.wndInventory.SetDragonSoulRefineWindow(self.wndDragonSoulRefine)
		
		if app.ENABLE_NEW_FISHING_SYSTEM:
			self.wndFishingWindow = uifishing.FishingWindow()
		else:
			self.wndFishingWindow = None
		
		if app.ENABLE_RANKING:
			self.wndRanking = uiranking.RankingWindow()
		
		if app.ENABLE_BATTLE_PASS:
			self.wndBattlePass = uibattlepass.BattlePassWindow()
			self.wndBattlePassButton = uibattlepass.BattlePassButton()
			self.wndBattlePassButton.BindInterface(self)

		if app.ENABLE_DUNGEON_INFO_SYSTEM:
			self.wndDungeonInfo = uidungeoninfo.DungeonInfo()
			self.wndMiniMap.BindInterfaceClass(self)
			self.wndMiniMap.BindGameClass(self.class_game)
		
		if app.ENABLE_RUNE_SYSTEM:
			self.wndRune = uirune.wnd()
		if app.ENABLE_WHISPER_ADMIN_SYSTEM:
			self.wndWhisperManager = whispermanager.wnd()
		if app.ENABLE_BIOLOGIST_UI:
			self.wndBiologist = uibiolog.BiologWindow()
			self.wndBiologistChange = uibiolog.BiologWindow_Change()
		if app.ENABLE_SAVEPOINT_SYSTEM:
			self.wndSavePoint = uisavepoint.Window()
		if app.ENABLE_CHOOSE_DOCTRINE_GUI:
			self.wndDoctrine = uidoctrinechoose.DoctrineWindow()

	def __MakeDialogs(self):
		self.dlgExchange = uiexchange.ExchangeDialog()
		if app.WJ_ENABLE_TRADABLE_ICON:
			self.dlgExchange.BindInterface(self)
			self.dlgExchange.SetInven(self.wndInventory)
			self.wndInventory.BindWindow(self.dlgExchange)
		
		self.dlgExchange.LoadDialog()
		self.dlgExchange.SetCenterPosition()
		self.dlgExchange.Hide()

		self.dlgPointReset = uipointreset.PointResetDialog()
		self.dlgPointReset.LoadDialog()
		self.dlgPointReset.Hide()

		self.dlgShop = uishop.ShopDialog()
		if app.WJ_ENABLE_TRADABLE_ICON:
			self.dlgShop.BindInterface(self)
		
		self.dlgShop.LoadDialog()
		self.dlgShop.Hide()

		self.dlgRestart = uirestart.RestartDialog()
		self.dlgRestart.LoadDialog()
		self.dlgRestart.Hide()

		self.dlgSystem = uisystem.SystemDialog()
		self.dlgSystem.LoadDialog()

		self.dlgSystem.Hide()

		self.dlgPassword = uisafebox.PasswordDialog()
		self.dlgPassword.Hide()

		self.hyperlinkItemTooltip = uitooltip.HyperlinkItemToolTip()
		self.hyperlinkItemTooltip.Hide()

		self.tooltipItem = uitooltip.ItemToolTip()
		self.tooltipItem.Hide()

		self.tooltipSkill = uitooltip.SkillToolTip()
		self.tooltipSkill.Hide()

		self.privateShopBuilder = uiprivateshopbuilder.PrivateShopBuilder()
		self.privateShopBuilder.Hide()
		if app.WJ_ENABLE_TRADABLE_ICON:
			self.privateShopBuilder.BindInterface(self)
			self.privateShopBuilder.SetInven(self.wndInventory)
			self.wndInventory.BindWindow(self.privateShopBuilder)
		
		self.dlgRefineNew = uirefine.RefineDialogNew()
		if app.WJ_ENABLE_TRADABLE_ICON:
			self.dlgRefineNew.SetInven(self.wndInventory)
			self.wndInventory.BindWindow(self.dlgRefineNew)
		
		self.dlgRefineNew.Hide()

	def __MakeTipBoard(self):
		self.tipBoard = uitip.TipBoard()
		self.tipBoard.Hide()

		self.bigBoard = uitip.BigBoard()
		self.bigBoard.Hide()

	if app.ADVANCED_GUILD_INFO:
		def __MakeRankWindow(self):
			self.wndRank = uiguildrank.GuildRankWindow()
			self.wndRank.Close()

	if app.ENABLE_ATTR_TRANSFER_SYSTEM:
		def __MakeAttrTransferWindow(self):
			self.wndAttrTransfer = uiattrtransfer.TransferWindow()
			self.wndAttrTransfer.BindInventory(self.wndInventory)
			self.wndAttrTransfer.LoadWindow()
			self.wndAttrTransfer.Hide()

	# def __MakeWebWindow(self):
	# 	if constinfo.IN_GAME_SHOP_ENABLE:
	# 		import uiweb
	# 		self.wndWeb = uiweb.WebWindow()
	# 		self.wndWeb.LoadWindow()
	# 		self.wndWeb.Hide()

	if app.ENABLE_MELEY_LAIR_DUNGEON:
		def __MakeMeleyRanking(self):
			self.wndMeleyRanking = uidragonlairranking.Window()
			self.wndMeleyRanking.LoadWindow()
			self.wndMeleyRanking.Hide()

	def __MakeCubeWindow(self):
		self.wndCube = uicube.CubeWindow()
		self.wndCube.LoadWindow()
		self.wndCube.Hide()

	if app.ENABLE_ACCE_SYSTEM:
		def __MakeAcceWindow(self):
			self.wndAcceCombine = uiacce.CombineWindow()
			self.wndAcceCombine.LoadWindow()
			self.wndAcceCombine.Hide()
			
			self.wndAcceAbsorption = uiacce.AbsorbWindow()
			self.wndAcceAbsorption.LoadWindow()
			self.wndAcceAbsorption.Hide()
			
			if self.wndInventory:
				self.wndInventory.SetAcceWindow(self.wndAcceCombine, self.wndAcceAbsorption)
	
	def __MakeCubeResultWindow(self):
		self.wndCubeResult = uicube.CubeResultWindow()
		self.wndCubeResult.LoadWindow()
		self.wndCubeResult.Hide()

	# ACCESSORY_REFINE_ADD_METIN_STONE
	def __MakeItemSelectWindow(self):
		self.wndItemSelect = uiselectitem.SelectItemWindow()
		self.wndItemSelect.Hide()
	# END_OF_ACCESSORY_REFINE_ADD_METIN_STONE

	if app.ENABLE_EXTRA_INVENTORY:
		def __MakeExtraInventoryWindow(self, parent):
			self.wndExtraInventory = uiextrainventory.ExtraInventoryWindow()
			if app.WJ_ENABLE_TRADABLE_ICON:
				self.wndExtraInventory.BindInterfaceClass(self)
				self.wndExtraInventory.BindWindow(self.dlgExchange)
				if self.privateShopBuilder:
					self.privateShopBuilder.SetExtraInven(self.wndExtraInventory)
					self.wndExtraInventory.BindWindow(self.privateShopBuilder)
					
				self.dlgExchange.BindInterface(self)
				self.dlgExchange.SetExtraInven(self.wndExtraInventory)
			
			self.wndExtraInventory.LoadWindow(parent)

		if app.ENABLE_LOCKED_EXTRA_INVENTORY:
			def ExtraInventoryRefresh(self):
				if self.wndExtraInventory:
					self.wndExtraInventory.UpdateInven()


	def __MakeMountInventoryWindow(self, parent):
		self.wndMountInventory = uimountinventory.MountInventoryWindow()
		self.wndMountInventory.LoadWindow(None)

	def RefreshMountInventory(self):
		if self.wndMountInventory:
			self.wndMountInventory.Refresh()
	if app.ENABLE_EXTRA_INVENTORY:
		def __MakeStoneCraftWindow(self):
			self.wndStoneCraft = uistonecraft.StoneCraftWindow()
			self.wndStoneCraft.Hide()
	if app.ENABLE_CUBE_RENEWAL_WORLDARD:
		def __MakeCubeRenewal(self):
			import uicuberenewal
			self.wndCubeRenewal = uicuberenewal.CubeRenewalWindows()
			self.wndCubeRenewal.Hide()


	def MakeInterface(self):
		self.__MakeMessengerWindow()
		self.__MakeGuildWindow()
		self.__MakeGuildRenewalWindow()
		self.__MakeChatWindow()
		self.__MakeParty()
		self.__MakeWindows()
		self.__MakeDialogs()
		if app.ENABLE_GAYA_SYSTEM:
			self.__MakeGayaGui()

		self.__MakeUICurtain()
		self.__MakeTaskBar()
		self.__MakeGameButtonWindow()
		self.__MakeTipBoard()
		#self.__MakeWebWindow()
		if app.ENABLE_ATTR_TRANSFER_SYSTEM:
			self.__MakeAttrTransferWindow()
		
		if app.ADVANCED_GUILD_INFO:
			self.__MakeRankWindow()
		if app.ENABLE_CUBE_RENEWAL_WORLDARD:
			self.__MakeCubeRenewal()
		if app.ENABLE_MELEY_LAIR_DUNGEON:
			self.__MakeMeleyRanking()
		self.__MakeCubeWindow()
		self.__MakeCubeResultWindow()
		if app.AUTO_SHOUT:
			self.__MakeShoutWindow()


		if app.ENABLE_ACCE_SYSTEM:
			self.__MakeAcceWindow()

		# ACCESSORY_REFINE_ADD_METIN_STONE
		self.__MakeItemSelectWindow()
		# END_OF_ACCESSORY_REFINE_ADD_METIN_STONE

		self.questButtonList = []
		self.whisperButtonList = []
		self.whisperDialogDict = {}
		self.privateShopAdvertisementBoardDict = {}

		self.wndInventory.SetItemToolTip(self.tooltipItem)
		if app.ENABLE_DRAGON_SOUL_SYSTEM:
			self.wndDragonSoul.SetItemToolTip(self.tooltipItem)
			self.wndDragonSoulRefine.SetItemToolTip(self.tooltipItem)
		if app.ENABLE_EXTRA_INVENTORY:
			self.__MakeExtraInventoryWindow(self.wndDragonSoulRefine)
			self.__MakeStoneCraftWindow()
		self.wndSafebox.SetItemToolTip(self.tooltipItem)
		self.__MakeMountInventoryWindow(self.wndDragonSoulRefine)
		if app.ENABLE_ATTR_TRANSFER_SYSTEM:
			self.wndAttrTransfer.SetItemToolTip(self.tooltipItem)
		
		self.wndCube.SetItemToolTip(self.tooltipItem)
		self.wndCubeResult.SetItemToolTip(self.tooltipItem)

		if app.ENABLE_BATTLE_PASS:
			self.wndBattlePass.SetItemToolTip(self.tooltipItem)
		self.wndSafebox.SetItemToolTip(self.tooltipItem)

		if app.ENABLE_ACCE_SYSTEM:
			self.wndAcceCombine.SetItemToolTip(self.tooltipItem)
			self.wndAcceAbsorption.SetItemToolTip(self.tooltipItem)
		
		if app.ENABLE_SWITCHBOT:
			self.wndSwitchbot.SetItemToolTip(self.tooltipItem)
		# ITEM_MALL
		self.wndMall.SetItemToolTip(self.tooltipItem)
		# END_OF_ITEM_MALL

		self.wndCharacter.SetSkillToolTip(self.tooltipSkill)
		self.wndTaskBar.SetItemToolTip(self.tooltipItem)
		self.wndTaskBar.SetSkillToolTip(self.tooltipSkill)
		self.wndGuild.SetSkillToolTip(self.tooltipSkill)

		# ACCESSORY_REFINE_ADD_METIN_STONE
		self.wndItemSelect.SetItemToolTip(self.tooltipItem)
		# END_OF_ACCESSORY_REFINE_ADD_METIN_STONE

		self.dlgShop.SetItemToolTip(self.tooltipItem)
		self.dlgExchange.SetItemToolTip(self.tooltipItem)
		self.privateShopBuilder.SetItemToolTip(self.tooltipItem)
		
		if app.ENABLE_EXTRA_INVENTORY:
			self.wndExtraInventory.SetItemToolTip(self.tooltipItem)
			self.wndStoneCraft.SetItemToolTip(self.tooltipItem)
		self.__InitWhisper()
		if app.ENABLE_RUNE_SYSTEM:
			self.wndRune.SetItemToolTip(self.tooltipItem)
		
		self.wndMountInventory.SetItemToolTip(self.tooltipItem)

		if app.ENABLE_BIOLOGIST_UI:
			self.wndBiologist.SetItemToolTip(self.tooltipItem)
			self.wndBiologistChange.SetItemToolTip(self.tooltipItem)
		if app.ENABLE_CHOOSE_DOCTRINE_GUI:
			self.wndDoctrine.SetSkillToolTip(self.tooltipSkill)

	def MakeHyperlinkTooltip(self, hyperlink):
		tokens = hyperlink.split(":")
		if tokens and len(tokens):
			type = tokens[0]
			
			if "item" == type:
				self.hyperlinkItemTooltip.SetHyperlinkItem(tokens)
			elif "msg" == type:# and str(tokens[1]) != player.GetMainCharacterName():
				self.OpenWhisperDialog(str(tokens[1]))
			elif app.LINK_IN_CHAT and type == "web" and (tokens[1].startswith("httpXxX") or tokens[1].startswith("httpsXxX")):
				link = "start " + tokens[1].replace("XxX", "://").replace("&","^&")
				os.system(link)
			elif app.LINK_IN_CHAT and "sysweb" == type:
				os.system("start " + tokens[1].replace("XxX", "://"))

	if app.ENABLE_GAYA_SYSTEM:
		def __MakeGayaGui(self):
			self.wndGayaG = uisystemgems.SelectGems()
			self.wndGayaG.LoadWindow()
			self.wndGayaG.Hide()

			self.wndGayaM = uisystemgems.SelectGemsShop()
			self.wndGayaM.LoadWindow()
			self.wndGayaM.Hide()
			
	if app.ENABLE_ITEMSHOP:
		def MakeItemShopWindow(self):
			if self.wndItemShop == None:
				self.wndItemShop = uiItemShop.ItemShopWindow()
		def OpenItemShopWindow(self):
			self.MakeItemShopWindow()
			if self.wndItemShop.IsShow():
				self.wndItemShop.Close()
			else:
				self.wndItemShop.Open()
		def OpenItemShopMainWindow(self):
			self.MakeItemShopWindow()
			self.wndItemShop.Open()
			self.wndItemShop.LoadFirstOpening()
		def ItemShopHideLoading(self):
			self.MakeItemShopWindow()
			self.wndItemShop.Open()
			self.wndItemShop.CloseLoading()
		def ItemShopPurchasesWindow(self):
			self.MakeItemShopWindow()
			self.wndItemShop.Open()
			self.wndItemShop.OpenPurchasesWindow()
		def ItemShopSetDragonCoin(self,dragonCoin):
			self.MakeItemShopWindow()
			self.wndItemShop.SetDragonCoin(dragonCoin)

	## Make Windows & Dialogs
	################################

	def Close(self):
		if app.ENABLE_ITEMSHOP:
			if self.wndItemShop:
				self.wndItemShop.Hide()
				self.wndItemShop.Destroy()
				self.wndItemShop = None
		if self.dlgWhisperWithoutTarget:
			self.dlgWhisperWithoutTarget.Destroy()
			del self.dlgWhisperWithoutTarget

		if uiquest.QuestDialog.__dict__.has_key("QuestCurtain"):
			uiquest.QuestDialog.QuestCurtain.Close()

		if self.wndQuestWindow:
			for key, eachQuestWindow in self.wndQuestWindow.items():
				eachQuestWindow.nextCurtainMode = -1
				eachQuestWindow.CloseSelf()
				eachQuestWindow = None
		self.wndQuestWindow = {}

		if self.wndTaskBar:
			self.wndTaskBar.Destroy()

		if app.ENABLE_CAPITALE_SYSTEM:
			if self.wndExpandedMoneyTaskbar:
				self.wndExpandedMoneyTaskbar.Destroy()


		if self.wndExpandedTaskBar:
			self.wndExpandedTaskBar.Destroy()

		if self.wndEnergyBar:
			self.wndEnergyBar.Destroy()

		if app.ENABLE_SKILL_COLOR_SYSTEM:
			if self.wndCharacter:
				self.wndCharacter.Close()
		else:
			if self.wndCharacter:
				self.wndCharacter.Hide()

		if self.wndInventory:
			self.wndInventory.Hide()
			self.wndInventory.Destroy()

		if self.wndDragonSoul:
			self.wndDragonSoul.Destroy()

		if self.wndDragonSoulRefine:
			self.wndDragonSoulRefine.Destroy()

		if self.dlgExchange:
			self.dlgExchange.Destroy()

		if self.dlgPointReset:
			self.dlgPointReset.Destroy()

		if self.dlgShop:
			self.dlgShop.Destroy()

		if self.dlgRestart:
			self.dlgRestart.Destroy()

		if self.dlgSystem:
			self.dlgSystem.Destroy()

		if self.dlgPassword:
			self.dlgPassword.Destroy()

		if self.wndMiniMap:
			self.wndMiniMap.Destroy()

		if self.wndSafebox:
			self.wndSafebox.Destroy()

		if self.wndWeb:
			self.wndWeb.Destroy()
			self.wndWeb = None

		if app.ADVANCED_GUILD_INFO:
			if self.wndRank:
				self.wndRank.Destroy()
				self.wndRank = None

		if self.wndMall:
			self.wndMall.Destroy()

		if self.wndParty:
			self.wndParty.Destroy()

		if app.ENABLE_ATTR_TRANSFER_SYSTEM:
			if self.wndAttrTransfer:
				self.wndAttrTransfer.Destroy()
		
		if app.ENABLE_MELEY_LAIR_DUNGEON:
			if self.wndMeleyRanking:
				self.wndMeleyRanking.Destroy()

		if self.wndCube:
			self.wndCube.Destroy()
		
		if app.ENABLE_ACCE_SYSTEM and  self.wndAcceCombine:
			self.wndAcceCombine.Destroy()
			
		if app.ENABLE_ACCE_SYSTEM and self.wndAcceAbsorption:
			self.wndAcceAbsorption.Destroy()


		if self.wndCubeResult:
			self.wndCubeResult.Destroy()
		if self.wndStoneCraft:
			self.wndStoneCraft.Destroy()
		if self.wndMessenger:
			self.wndMessenger.Destroy()

		if self.wndGuild:
			self.wndGuild.Destroy()
		if self.wndGuildRenewal:
			self.wndGuildRenewal.Destroy()
		if app.AUTO_SHOUT:
			if self.wndShout:
				self.wndShout.Destroy()
		if app.ENABLE_GAYA_SYSTEM:
			if self.wndGayaG:
				self.wndGayaG.Destroy()

			if self.wndGayaM:
				self.wndGayaM.Destroy()

		if self.privateShopBuilder:
			self.privateShopBuilder.Destroy()

		if self.dlgRefineNew:
			self.dlgRefineNew.Destroy()

		if self.wndGuildBuilding:
			self.wndGuildBuilding.Destroy()

		if self.wndGameButton:
			self.wndGameButton.Destroy()

		if app.__BL_PICK_FILTER__:
			if self.wndPickUpFilter:
				self.wndPickUpFilter.Destroy()
				del self.wndPickUpFilter

		if app.ENABLE_SWITCHBOT:
			if self.wndSwitchbot:
				self.wndSwitchbot.Destroy()

		if self.wndOfflineshop:
			self.wndOfflineshop.Destroy()

		# ITEM_MALL
		if self.mallPageDlg:
			self.mallPageDlg.Destroy()
		# END_OF_ITEM_MALL

		# ACCESSORY_REFINE_ADD_METIN_STONE
		if self.wndItemSelect:
			self.wndItemSelect.Destroy()
		# END_OF_ACCESSORY_REFINE_ADD_METIN_STONE
		
		if app.ENABLE_NEW_FISHING_SYSTEM:
			if self.wndFishingWindow:
				self.wndFishingWindow.Destroy()
				del self.wndFishingWindow
		
		if app.ENABLE_EXTRA_INVENTORY:
			if self.wndExtraInventory:
				self.wndExtraInventory.Destroy()

		if app.ENABLE_CUBE_RENEWAL_WORLDARD:
			if self.wndCubeRenewal:
				self.wndCubeRenewal.Destroy()
				self.wndCubeRenewal.Close()

		if app.ENABLE_BATTLE_PASS:
			if self.wndBattlePass:
				self.wndBattlePass.Destroy()

		if app.ENABLE_DUNGEON_INFO_SYSTEM:
			if self.wndDungeonInfo:
				self.wndDungeonInfo.Destroy()

		self.wndChatLog.Destroy()
		for btn in self.questButtonList:
			btn.SetEvent(0)
		for btn in self.whisperButtonList:
			btn.SetEvent(0)
		for dlg in self.whisperDialogDict.itervalues():
			dlg.Destroy()
		for brd in self.guildScoreBoardDict.itervalues():
			brd.Destroy()
		for dlg in self.equipmentDialogDict.itervalues():
			dlg.Destroy()

		# ITEM_MALL
		del self.mallPageDlg
		# END_OF_ITEM_MALL

		del self.wndGuild
		del self.wndGuildRenewal
		del self.wndMessenger
		del self.wndUICurtain
		if self.wndChat:
			self.wndChat.Destroy()

		if self.yangText:
			self.yangText.ClearAll()

		del self.wndChat
		del self.yangText

		
		
		del self.wndTaskBar
		if app.ENABLE_CUBE_RENEWAL_WORLDARD:
			del self.wndCubeRenewal
		if app.ENABLE_CAPITALE_SYSTEM:
			if self.wndExpandedMoneyTaskbar:
				del self.wndExpandedMoneyTaskbar
		if self.wndExpandedTaskBar:
			del self.wndExpandedTaskBar
		del self.wndEnergyBar
		del self.wndCharacter
		del self.wndInventory
		if self.wndDragonSoul:
			del self.wndDragonSoul
		if self.wndDragonSoulRefine:
			del self.wndDragonSoulRefine
		del self.dlgExchange
		del self.dlgPointReset
		del self.dlgShop
		del self.dlgRestart
		del self.dlgSystem
		del self.dlgPassword
		del self.hyperlinkItemTooltip
		del self.tooltipItem
		del self.tooltipSkill
		del self.wndMiniMap
		del self.wndSafebox
		del self.wndMall
		del self.wndParty
		if app.ENABLE_ATTR_TRANSFER_SYSTEM:
			del self.wndAttrTransfer
		
		if app.ENABLE_MELEY_LAIR_DUNGEON:
			del self.wndMeleyRanking
		del self.wndCube
		del self.wndCubeResult
		if app.ENABLE_GAYA_SYSTEM:
			del self.wndGayaG
			del self.wndGayaM
		del self.privateShopBuilder
		del self.inputDialog
		del self.wndChatLog
		del self.dlgRefineNew
		del self.wndGuildBuilding
		del self.wndGameButton
		del self.tipBoard
		del self.bigBoard
		del self.wndItemSelect
		if app.AUTO_SHOUT:
			del self.wndShout
		if app.ENABLE_RANKING:
			if self.wndRanking:
				self.wndRanking.Destroy()
				del self.wndRanking
		
		if app.ENABLE_EXTRA_INVENTORY:
			del self.wndExtraInventory
			del self.wndStoneCraft
		
		if app.ENABLE_ACCE_SYSTEM:
			del self.wndAcceCombine
			del self.wndAcceAbsorption

		if app.ENABLE_SWITCHBOT:
			del self.wndSwitchbot

		del self.wndOfflineshop

		if app.ENABLE_BATTLE_PASS:
			if self.wndBattlePass:
				del self.wndBattlePass
				
			if self.wndBattlePassButton:
				del self.wndBattlePassButton

		if app.ENABLE_DUNGEON_INFO_SYSTEM:
			if self.wndDungeonInfo:
				del self.wndDungeonInfo

		self.questButtonList = []
		self.whisperButtonList = []
		self.whisperDialogDict = {}
		self.privateShopAdvertisementBoardDict = {}
		self.guildScoreBoardDict = {}
		self.equipmentDialogDict = {}

		uichat.DestroyChatInputSetWindow()
		if app.ENABLE_RUNE_SYSTEM:
			if self.wndRune:
				self.wndRune.Destroy()
			del self.wndRune
		if app.ENABLE_WHISPER_ADMIN_SYSTEM:
			if self.wndWhisperManager:
				self.wndWhisperManager.Destroy()
			del self.wndWhisperManager
		
		if app.ENABLE_BIOLOGIST_UI:
			if self.wndBiologist:
				self.wndBiologist.Destroy()
			del self.wndBiologist
			if self.wndBiologistChange:
				self.wndBiologistChange.Destroy()
			del self.wndBiologistChange
		if app.ENABLE_SAVEPOINT_SYSTEM:
			if self.wndSavePoint:
				self.wndSavePoint.Destroy()
			del self.wndSavePoint
		if app.ENABLE_CHOOSE_DOCTRINE_GUI:
			if self.wndDoctrine:
				self.wndDoctrine.Close()
				del self.wndDoctrine
		if app.ENABLE_EVENT_MANAGER:
			if self.wndEventManager:
				self.wndEventManager.Hide()
				self.wndEventManager.Destroy()
				self.wndEventManager = None

			if self.wndEventIcon:
				self.wndEventIcon.Hide()
				self.wndEventIcon.Destroy()
				self.wndEventIcon = None

	if app.ADVANCED_GUILD_INFO:
		def OpenRankWindow(self):
			self.wndRank.Open()

	## Skill
	def OnUseSkill(self, slotIndex, coolTime):
		self.wndCharacter.OnUseSkill(slotIndex, coolTime)
		self.wndTaskBar.OnUseSkill(slotIndex, coolTime)
		self.wndGuild.OnUseSkill(slotIndex, coolTime)

	def OnActivateSkill(self, slotIndex):
		self.wndCharacter.OnActivateSkill(slotIndex)
		self.wndTaskBar.OnActivateSkill(slotIndex)

	def OnDeactivateSkill(self, slotIndex):
		self.wndCharacter.OnDeactivateSkill(slotIndex)
		self.wndTaskBar.OnDeactivateSkill(slotIndex)

	def OnChangeCurrentSkill(self, skillSlotNumber):
		self.wndTaskBar.OnChangeCurrentSkill(skillSlotNumber)

	def SelectMouseButtonEvent(self, dir, event):
		self.wndTaskBar.SelectMouseButtonEvent(dir, event)

	## Refresh
	def RefreshAlignment(self):
		self.wndCharacter.RefreshAlignment()
	## Céh tooltip
	if app.ENABLE_GUILD_ATTRIBUTE:
		def RefreshGuild(self):
			self.wndCharacter.RefreshGuild()
	def RefreshStatus(self):
		self.wndTaskBar.RefreshStatus()
		self.wndCharacter.RefreshStatus()
		self.wndInventory.RefreshStatus()
		if self.wndEnergyBar:
			self.wndEnergyBar.RefreshStatus()
		if app.ENABLE_DRAGON_SOUL_SYSTEM:
			self.wndDragonSoul.RefreshStatus()
			
		if app.ENABLE_CAPITALE_SYSTEM:
			if self.wndExpandedMoneyTaskbar:
				self.wndExpandedMoneyTaskbar.RefreshStatus()


	def RefreshStamina(self):
		self.wndTaskBar.RefreshStamina()

	def RefreshSkill(self):
		self.wndCharacter.RefreshSkill()
		self.wndTaskBar.RefreshSkill()
		if app.ENABLE_CHOOSE_DOCTRINE_GUI:
			self.wndDoctrine.RefreshSkill()

	def RefreshInventory(self):
		if self.wndCharacter:
			self.wndCharacter.RefreshItemSlot()
		
		self.wndTaskBar.RefreshQuickSlot()
		self.wndInventory.RefreshItemSlot()
		if app.ENABLE_DRAGON_SOUL_SYSTEM:
			self.wndDragonSoul.RefreshItemSlot()
		if app.ENABLE_EXTRA_INVENTORY:
			self.wndExtraInventory.RefreshItemSlot()

		self.RefreshMountInventory()
		
		if app.ENABLE_RUNE_SYSTEM:
			self.wndRune.RefreshItemSlot()

	if app.__ENABLE_EXTEND_INVEN_SYSTEM__:
		def SetInventoryPageLock(self):
			self.wndInventory.UpdateInven()

	def RefreshCharacter(self): ## Character �������� ����, Inventory �������� ���� ���� ���� Refresh
		self.wndCharacter.RefreshCharacter()
		self.wndTaskBar.RefreshQuickSlot()

	def RefreshQuest(self):
		self.wndCharacter.RefreshQuest()

	def RefreshSafebox(self):
		self.wndSafebox.RefreshSafebox()

	# ITEM_MALL
	def RefreshMall(self):
		self.wndMall.RefreshMall()

	def OpenItemMall(self):
		if not self.mallPageDlg:
			self.mallPageDlg = uishop.MallPageDialog()

		self.mallPageDlg.Open()
	# END_OF_ITEM_MALL

	def RefreshMessenger(self):
		self.wndMessenger.RefreshMessenger()

	def RefreshGuildInfoPage(self):
		self.wndGuild.RefreshGuildInfoPage()

	def RefreshGuildBoardPage(self):
		self.wndGuild.RefreshGuildBoardPage()

	def RefreshGuildMemberPage(self):
		self.wndGuild.RefreshGuildMemberPage()

	def RefreshGuildMemberPageGradeComboBox(self):
		self.wndGuild.RefreshGuildMemberPageGradeComboBox()

	def RefreshGuildSkillPage(self):
		self.wndGuild.RefreshGuildSkillPage()

	def RefreshGuildGradePage(self):
		self.wndGuild.RefreshGuildGradePage()

	def DeleteGuild(self):
		self.wndMessenger.ClearGuildMember()
		self.wndGuild.DeleteGuild()

	def RefreshMobile(self):
		self.dlgSystem.RefreshMobile()

	def OnMobileAuthority(self):
		self.dlgSystem.OnMobileAuthority()

	def OnBlockMode(self, mode):
		self.dlgSystem.OnBlockMode(mode)

	## Calling Functions
	# PointReset
	def OpenPointResetDialog(self):
		self.dlgPointReset.Show()
		self.dlgPointReset.SetTop()

	def ClosePointResetDialog(self):
		self.dlgPointReset.Close()

	# Shop
	def OpenShopDialog(self, vid):
		self.wndInventory.Show()
		self.wndInventory.SetTop()
		self.dlgShop.Open(vid)
		self.dlgShop.SetTop()

	def CloseShopDialog(self):
		self.dlgShop.Close()

	def RefreshShopDialog(self):
		self.dlgShop.Refresh()

	## Quest
	def OpenCharacterWindowQuestPage(self):
		self.wndCharacter.Show()
		self.wndCharacter.SetState("QUEST")

	def OpenQuestWindow(self, skin, idx):
		if app.ENABLE_NEW_BUGFIXES and constinfo.INPUT_IGNORE == 1:
			return
		
		wnds = ()

		q = uiquest.QuestDialog(skin, idx)
		q.SetWindowName("QuestWindow" + str(idx))
		q.Show()
		if skin:
			q.Lock()
			wnds = self.__HideWindows()

			# UNKNOWN_UPDATE
			q.AddOnDoneEvent(lambda tmp_self, args=wnds: self.__ShowWindows(args))
			# END_OF_UNKNOWN_UPDATE

		if skin:
			q.AddOnCloseEvent(q.Unlock)
		q.AddOnCloseEvent(lambda key = self.wndQuestWindowNewKey:ui.__mem_func__(self.RemoveQuestDialog)(key))
		self.wndQuestWindow[self.wndQuestWindowNewKey] = q

		self.wndQuestWindowNewKey = self.wndQuestWindowNewKey + 1

		# END_OF_UNKNOWN_UPDATE

	def RemoveQuestDialog(self, key):
		del self.wndQuestWindow[key]

	## Exchange
	def StartExchange(self):
		self.dlgExchange.OpenDialog()
		self.dlgExchange.Refresh()

	def EndExchange(self):
		self.dlgExchange.CloseDialog()

	if app.WJ_ENABLE_TRADABLE_ICON:
		def CantTradableItemExchange(self, dstSlotIndex, srcSlotIndex):
			self.dlgExchange.CantTradableItem(dstSlotIndex, srcSlotIndex)

		def CantTradableExtraItemExchange(self, dstSlotIndex, srcSlotIndex):
			self.dlgExchange.CantTradableExtraItem(dstSlotIndex, srcSlotIndex)

	def RefreshExchange(self):
		self.dlgExchange.Refresh()

	## Party
	def AddPartyMember(self, pid, name):
		self.wndParty.AddPartyMember(pid, name)

		self.__ArrangeQuestButton()

	def UpdatePartyMemberInfo(self, pid):
		self.wndParty.UpdatePartyMemberInfo(pid)

	def RemovePartyMember(self, pid):
		self.wndParty.RemovePartyMember(pid)

		##!! 20061026.levites.������_����_����
		self.__ArrangeQuestButton()

	def LinkPartyMember(self, pid, vid):
		self.wndParty.LinkPartyMember(pid, vid)

	def UnlinkPartyMember(self, pid):
		self.wndParty.UnlinkPartyMember(pid)

	def UnlinkAllPartyMember(self):
		self.wndParty.UnlinkAllPartyMember()

	def ExitParty(self):
		self.wndParty.ExitParty()

		##!! 20061026.levites.������_����_����
		self.__ArrangeQuestButton()

	def PartyHealReady(self):
		self.wndParty.PartyHealReady()

	def ChangePartyParameter(self, distributionMode):
		self.wndParty.ChangePartyParameter(distributionMode)

	## Safebox
	def AskSafeboxPassword(self):
		if self.wndSafebox.IsShow():
			return

		# SAFEBOX_PASSWORD
		self.dlgPassword.SetTitle(localeinfo.PASSWORD_TITLE)
		self.dlgPassword.SetSendMessage("/safebox_password ")
		# END_OF_SAFEBOX_PASSWORD

		self.dlgPassword.ShowDialog()
		self.dlgPassword.SetTop()

	def OpenSafeboxWindow(self, size):
		self.dlgPassword.CloseDialog()
		self.wndSafebox.ShowWindow(size)

	def RefreshSafeboxMoney(self):
		self.wndSafebox.RefreshSafeboxMoney()

	def CommandCloseSafebox(self):
		self.wndSafebox.CommandCloseSafebox()

	# ITEM_MALL
	def AskMallPassword(self):
		if self.wndMall.IsShow():
			return
		self.dlgPassword.SetTitle(localeinfo.MALL_PASSWORD_TITLE)
		self.dlgPassword.SetSendMessage("/mall_password ")
		self.dlgPassword.ShowDialog()
		self.dlgPassword.SetTop()

	def OpenMallWindow(self, size):
		self.dlgPassword.CloseDialog()
		self.wndMall.ShowWindow(size)

	def CommandCloseMall(self):
		self.wndMall.CommandCloseMall()
	# END_OF_ITEM_MALL

	## Guild
	def OnStartGuildWar(self, guildSelf, guildOpp):
		self.wndGuild.OnStartGuildWar(guildSelf, guildOpp)

		guildWarScoreBoard = uiguild.GuildWarScoreBoard()
		guildWarScoreBoard.Open(guildSelf, guildOpp)
		guildWarScoreBoard.Show()
		self.guildScoreBoardDict[uiguild.GetGVGKey(guildSelf, guildOpp)] = guildWarScoreBoard

	def OnEndGuildWar(self, guildSelf, guildOpp):
		self.wndGuild.OnEndGuildWar(guildSelf, guildOpp)

		key = uiguild.GetGVGKey(guildSelf, guildOpp)

		if not self.guildScoreBoardDict.has_key(key):
			return

		self.guildScoreBoardDict[key].Destroy()
		del self.guildScoreBoardDict[key]

	# GUILDWAR_MEMBER_COUNT
	def UpdateMemberCount(self, gulidID1, memberCount1, guildID2, memberCount2):
		key = uiguild.GetGVGKey(gulidID1, guildID2)

		if not self.guildScoreBoardDict.has_key(key):
			return

		self.guildScoreBoardDict[key].UpdateMemberCount(gulidID1, memberCount1, guildID2, memberCount2)
	# END_OF_GUILDWAR_MEMBER_COUNT

	def OnRecvGuildWarPoint(self, gainGuildID, opponentGuildID, point):
		key = uiguild.GetGVGKey(gainGuildID, opponentGuildID)
		if not self.guildScoreBoardDict.has_key(key):
			return

		guildBoard = self.guildScoreBoardDict[key]
		guildBoard.SetScore(gainGuildID, opponentGuildID, point)

	## PK Mode
	def OnChangePKMode(self):
		self.wndCharacter.RefreshAlignment()
		self.dlgSystem.OnChangePKMode()

	## Refine
	def OpenRefineDialog(self, targetItemPos, nextGradeItemVnum, cost, prob, type):
		self.dlgRefineNew.Open(targetItemPos, nextGradeItemVnum, cost, prob, type)

	def CloseRefineDialog(self):
		if self.dlgRefineNew and self.dlgRefineNew.IsShow():
			self.dlgRefineNew.Close()

	def AppendMaterialToRefineDialog(self, vnum, count):
		self.dlgRefineNew.AppendMaterial(vnum, count)

	## Show & Hide
	def ShowDefaultWindows(self):
		self.wndTaskBar.Show()
		self.wndMiniMap.Show()
		self.wndMiniMap.ShowMiniMap()
		if self.wndEnergyBar:
			self.wndEnergyBar.Show()
		
		if app.ENABLE_CAPITALE_SYSTEM:
			if self.wndExpandedMoneyTaskbar and constinfo.wndExpandedMoneyTaskbar == 0:
				self.wndExpandedMoneyTaskbar.Show()
				self.wndExpandedMoneyTaskbar.SetTop()

	def ShowAllWindows(self):
		self.wndTaskBar.Show()
		self.wndCharacter.Show()
		self.wndInventory.Show()
		if app.ENABLE_DRAGON_SOUL_SYSTEM:
			self.wndDragonSoul.Show()
			self.wndDragonSoulRefine.Show()
		self.wndChat.Show()

		self.yangText.Show()
		self.wndMiniMap.Show()
		if app.ENABLE_RANKING:
			if self.wndRanking:
				self.wndRanking.Open()
		
		if self.wndEnergyBar:
			self.wndEnergyBar.Show()
		if self.wndExpandedTaskBar:
			self.wndExpandedTaskBar.Show()
			self.wndExpandedTaskBar.SetTop()
		if app.ENABLE_CAPITALE_SYSTEM:
			if self.wndExpandedMoneyTaskbar:
				self.wndExpandedMoneyTaskbar.Show()
				self.wndExpandedMoneyTaskbar.SetTop()

		if app.ENABLE_RUNE_SYSTEM:
			self.wndRune.Show()
		if app.ENABLE_WHISPER_ADMIN_SYSTEM:
			self.wndWhisperManager.Show()
		if app.ENABLE_NEW_FISHING_SYSTEM:
			if self.wndFishingWindow:
				self.wndFishingWindow.Open()

	def HideAllWindows(self):
		if self.wndTaskBar:
			self.wndTaskBar.Hide()

		if self.wndEnergyBar:
			self.wndEnergyBar.Hide()

		if self.wndCharacter:
			self.wndCharacter.Hide()

		if self.wndInventory:
			if app.ENABLE_ATTR_COSTUMES:
				self.wndInventory.OnCloseRemoveAttrCostume()
			
			self.wndInventory.Close()

		if app.ENABLE_DRAGON_SOUL_SYSTEM:
			self.wndDragonSoul.Hide()
			self.wndDragonSoulRefine.Hide()

		if self.wndChat:
			self.wndChat.Hide()

		if self.yangText:
			self.yangText.Hide()

		if self.wndMiniMap:
			self.wndMiniMap.Hide()

		if self.wndMessenger:
			self.wndMessenger.Hide()

		if self.wndGuild:
			self.wndGuild.Hide()

		if app.__BL_PICK_FILTER__:
			if self.wndPickUpFilter:
				self.wndPickUpFilter.Hide()

		if app.ENABLE_CAPITALE_SYSTEM:
			if self.wndExpandedMoneyTaskbar:
				self.wndExpandedMoneyTaskbar.Hide()

		if self.wndExpandedTaskBar:
			self.wndExpandedTaskBar.Hide()
		
		if app.ENABLE_RANKING:
			if self.wndRanking:
				self.wndRanking.Close()
		
		if app.ENABLE_SWITCHBOT:
			if self.wndSwitchbot:
				self.wndSwitchbot.Hide()

		if self.wndOfflineshop:
			self.wndOfflineshop.Hide()

		if app.ENABLE_DUNGEON_INFO_SYSTEM:
			if self.wndDungeonInfo:
				self.wndDungeonInfo.Hide()
		
		if app.ENABLE_RUNE_SYSTEM:
			self.wndRune.Hide()
		if app.ENABLE_WHISPER_ADMIN_SYSTEM:
			self.wndWhisperManager.Hide()
		if app.ENABLE_BIOLOGIST_UI:
			self.wndBiologist.Hide()
			self.wndBiologistChange.Hide()
		if app.ENABLE_NEW_FISHING_SYSTEM:
			if self.wndFishingWindow:
				self.wndFishingWindow.Close()
		if app.ENABLE_SAVEPOINT_SYSTEM:
			self.wndSavePoint.Hide()

	def ShowMouseImage(self):
		self.wndTaskBar.ShowMouseImage()

	def HideMouseImage(self):
		self.wndTaskBar.HideMouseImage()

	def ToggleChat(self):
		if True == self.wndChat.IsEditMode():
			self.wndChat.CloseChat()
		else:
			# ���������� ���������� ���� ������ ����
			if self.wndWeb and self.wndWeb.IsShow():
				pass
			else:
				self.wndChat.OpenChat()

	def IsOpenChat(self):
		return self.wndChat.IsEditMode()

	def SetChatFocus(self):
		self.wndChat.SetChatFocus()

	def OpenRestartDialog(self):
		self.dlgRestart.OpenDialog()
		self.dlgRestart.SetTop()

	def CloseRestartDialog(self):
		self.dlgRestart.Close()

	def ToggleSystemDialog(self):
		if False == self.dlgSystem.IsShow():
			self.dlgSystem.OpenDialog()
			self.dlgSystem.SetTop()
		else:
			self.dlgSystem.Close()

	def OpenSystemDialog(self):
		self.dlgSystem.OpenDialog()
		self.dlgSystem.SetTop()

	def ToggleMessenger(self):
		if self.wndMessenger.IsShow():
			self.wndMessenger.Hide()
		else:
			self.wndMessenger.SetTop()
			self.wndMessenger.Show()

	def ToggleMiniMap(self):
		if app.IsPressed(app.DIK_LSHIFT) or app.IsPressed(app.DIK_RSHIFT):
			if False == self.wndMiniMap.isShowMiniMap():
				self.wndMiniMap.ShowMiniMap()
				self.wndMiniMap.SetTop()
			else:
				self.wndMiniMap.HideMiniMap()

		else:
			self.wndMiniMap.ToggleAtlasWindow()

	def PressMKey(self):
		if app.IsPressed(app.DIK_RSHIFT):
			self.ToggleMountInventory()

		elif app.IsPressed(app.DIK_LALT) or app.IsPressed(app.DIK_RALT):
			self.ToggleMessenger()

		else:
			self.ToggleMiniMap()

	def MiniMapScaleUp(self):
		self.wndMiniMap.ScaleUp()

	def MiniMapScaleDown(self):
		self.wndMiniMap.ScaleDown()

	def ToggleMountInventory(self):
		if not self.wndMountInventory:
			return
		if self.wndMountInventory.IsShow():
			self.wndMountInventory.Close()
		else:
			self.wndMountInventory.Refresh()
			self.wndMountInventory.ShowWindow()

	def MiniMapScaleDown(self):
		self.wndMiniMap.ScaleDown()

	def ToggleCharacterWindow(self, state):
		if False == player.IsObserverMode():
			if False == self.wndCharacter.IsShow():
				self.OpenCharacterWindowWithState(state)
			else:
				if state == self.wndCharacter.GetState():
					self.wndCharacter.OverOutItem()
					if app.ENABLE_SKILL_COLOR_SYSTEM:
						self.wndCharacter.Close()
					else:
						self.wndCharacter.Hide()
				else:
					self.wndCharacter.SetState(state)

	def OpenCharacterWindowWithState(self, state):
		if False == player.IsObserverMode():
			self.wndCharacter.SetState(state)
			self.wndCharacter.Show()
			self.wndCharacter.SetTop()

	def ToggleCharacterWindowStatusPage(self):
		self.ToggleCharacterWindow("STATUS")

	def ToggleInventoryWindow(self):
		if False == player.IsObserverMode():
			if False == self.wndInventory.IsShow():
				self.wndInventory.Show()
				self.wndInventory.SetTop()
			else:
				self.wndInventory.OverOutItem()
				self.wndInventory.Close()

	if app.ENABLE_CAPITALE_SYSTEM:
		def ToggleExpandedMoneyButton(self):
			if False == player.IsObserverMode():
				if False == self.wndExpandedMoneyTaskbar.IsShow():
					constinfo.wndExpandedMoneyTaskbar = 0
					self.wndExpandedMoneyTaskbar.Show()
					self.wndExpandedMoneyTaskbar.SetTop()
				else:
					constinfo.wndExpandedMoneyTaskbar = 1
					self.wndExpandedMoneyTaskbar.Close()

	def ToggleExpandedButton(self):
		if False == player.IsObserverMode():
			if False == self.wndExpandedTaskBar.IsShow():
				self.wndExpandedTaskBar.Show()
				self.wndExpandedTaskBar.SetTop()
			else:
				self.wndExpandedTaskBar.Close()
			
	# ������
	if app.ENABLE_DS_SET:
		def SetDSSet(self, idx):
			if self.wndDragonSoul:
				self.wndDragonSoul.SetDSSet(idx)

	def DragonSoulActivate(self, deck):
		if app.ENABLE_DRAGON_SOUL_SYSTEM:
			self.wndDragonSoul.ActivateDragonSoulByExtern(deck)

	def DragonSoulDeactivate(self):
		if app.ENABLE_DRAGON_SOUL_SYSTEM:
			self.wndDragonSoul.DeactivateDragonSoul()

	def Highligt_Item(self, inven_type, inven_pos):
		if not app.ENABLE_HIGHLIGHT_SYSTEM:
			if player.DRAGON_SOUL_INVENTORY == inven_type:
				if app.ENABLE_DRAGON_SOUL_SYSTEM:
					self.wndDragonSoul.HighlightSlot(inven_pos)
		else:
			if inven_type == player.INVENTORY:
				self.wndInventory.HighlightSlot(inven_pos)
			elif inven_type == player.DRAGON_SOUL_INVENTORY:
				if app.ENABLE_DRAGON_SOUL_SYSTEM:
					self.wndDragonSoul.HighlightSlot(inven_pos)
			elif app.ENABLE_EXTRA_INVENTORY and inven_type == player.EXTRA_INVENTORY and self.wndExtraInventory:
				self.wndExtraInventory.HighlightSlot(inven_pos)

	def ToggleDragonSoulWindow(self):
		if False == player.IsObserverMode():
			if app.ENABLE_DRAGON_SOUL_SYSTEM:
				if False == self.wndDragonSoul.IsShow():
					self.wndDragonSoul.Show()
				else:
					self.wndDragonSoul.Close()

	def ToggleDragonSoulWindowWithNoInfo(self):
		if False == player.IsObserverMode():
			if app.ENABLE_DRAGON_SOUL_SYSTEM:
				if False == self.wndDragonSoul.IsShow():
					self.wndDragonSoul.Show()
				else:
					self.wndDragonSoul.Close()

	if app.ENABLE_DUNGEON_INFO_SYSTEM:
		def ToggleDungeonInfoWindow(self):
			if False == player.IsObserverMode():
				if False == self.wndDungeonInfo.IsShow():
					self.wndDungeonInfo.Open()
				else:
					self.wndDungeonInfo.Close()

	def FailDragonSoulRefine(self, reason, inven_type, inven_pos):
		if False == player.IsObserverMode():
			if app.ENABLE_DRAGON_SOUL_SYSTEM:
				if True == self.wndDragonSoulRefine.IsShow():
					self.wndDragonSoulRefine.RefineFail(reason, inven_type, inven_pos)

	def SucceedDragonSoulRefine(self, inven_type, inven_pos):
		if False == player.IsObserverMode():
			if app.ENABLE_DRAGON_SOUL_SYSTEM:
				if True == self.wndDragonSoulRefine.IsShow():
					self.wndDragonSoulRefine.RefineSucceed(inven_type, inven_pos)

	def OpenDragonSoulRefineWindow(self):
		if False == player.IsObserverMode():
			if app.ENABLE_DRAGON_SOUL_SYSTEM:
				if False == self.wndDragonSoulRefine.IsShow():
					self.wndDragonSoulRefine.Show()
					if None != self.wndDragonSoul:
						if False == self.wndDragonSoul.IsShow():
							self.wndDragonSoul.Show()

	def CloseDragonSoulRefineWindow(self):
	

		if False == player.IsObserverMode():
			if app.ENABLE_DRAGON_SOUL_SYSTEM:
				if True == self.wndDragonSoulRefine.IsShow():
					self.wndDragonSoulRefine.Close()

	# ������ ��

	if app.ENABLE_BATTLE_PASS:
		def OpenBattlePass(self):
			if False == player.IsObserverMode():
				if not self.wndBattlePass.IsShow():
					self.wndBattlePass.Open()
					self.wndBattlePassButton.CompleteLoading()
				else:
					self.wndBattlePass.Close()

		def AddBattlePassMission(self, missionType, missionInfo1, missionInfo2, missionInfo3):
			if self.wndBattlePass:
				self.wndBattlePass.AddMission(missionType, missionInfo1, missionInfo2, missionInfo3)
				
		def UpdateBattlePassMission(self, missionType, newProgress):
			if self.wndBattlePass:
				self.wndBattlePass.UpdateMission(missionType, newProgress)
				
		def AddBattlePassMissionReward(self, missionType, itemVnum, itemCount):
			if self.wndBattlePass:
				self.wndBattlePass.AddMissionReward(missionType, itemVnum, itemCount)
				
		def AddBattlePassReward(self, itemVnum, itemCount):
			if self.wndBattlePass:
				self.wndBattlePass.AddReward(itemVnum, itemCount)
				
		def AddBattlePassRanking(self, pos, playerName, finishTime):
			if self.wndBattlePass:
				self.wndBattlePass.AddRanking(pos, playerName, finishTime)
				
		def RefreshBattlePassRanking(self):
			if self.wndBattlePass:
				self.wndBattlePass.RefreshRanking()
				
		def OpenBattlePassRanking(self):
			if self.wndBattlePass:
				self.wndBattlePass.OpenRanking()

	def ToggleGuildWindow(self):
		if not self.wndGuild.IsShow():
			if self.wndGuild.CanOpen():
				self.wndGuild.Open()
			else:
				chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.GUILD_YOU_DO_NOT_JOIN)
		else:
			self.wndGuild.OverOutItem()
			self.wndGuild.Hide()

	# GUILD RENEWAL BY RAZOR93
	def GuildRenewalOpen(self, level, paid):
		if self.wndGuildRenewal:
			self.wndGuildRenewal.Open()
			self.wndGuildRenewal.SetGuildLevel(level)
			self.wndGuildRenewal.SetPaidStatus(paid)

	def GuildRenewalClose(self):
		if self.wndGuildRenewal:
			self.wndGuildRenewal.Close()

	def GuildRenewalSetMoney(self, money):
		if self.wndGuildRenewal:
			self.wndGuildRenewal.SetStorageMoney(money)

	def GuildRenewalSetPaid(self, paid):
		if self.wndGuildRenewal:
			self.wndGuildRenewal.SetPaidStatus(paid)

	def GuildRenewalSetSlot(self, slotIndex, vnum, count):
		if self.wndGuildRenewal:
			self.wndGuildRenewal.SetStorageSlot(slotIndex, vnum, count)

	def GuildRenewalSetReq(self, reqYang, items):
		if self.wndGuildRenewal:
			self.wndGuildRenewal.SetRequired(reqYang, items)

	def GuildRenewalSetTax(self, deadline, taxYang, items):
		if self.wndGuildRenewal:
			self.wndGuildRenewal.SetTax(deadline, taxYang, items)

	def GuildRenewalSetContrib(self, name, yang, items):
		if self.wndGuildRenewal:
			self.wndGuildRenewal.SetContribution(name, yang, items)

	def GuildRenewalSetContrib2(self, pid, paidFlag, paidMoney, paidItemTotal, i0, i1, i2, i3, i4):
		if self.wndGuildRenewal:
			try:
				self.wndGuildRenewal.SetContribution2(pid, paidFlag, paidMoney, paidItemTotal, i0, i1, i2, i3, i4)
			except:
				pass

	def GuildRenewalClear(self):
		if self.wndGuildRenewal:
			self.wndGuildRenewal.ClearData()

	def GuildRenewalDone(self):
		if self.wndGuildRenewal:
			try:
				self.wndGuildRenewal.Done()
			except:
				pass

	def ToggleChatLogWindow(self):
		if self.wndChatLog.IsShow():
			self.wndChatLog.Hide()
		else:
			self.wndChatLog.Show()

	if app.ENABLE_SWITCHBOT:
		def ToggleSwitchbotWindow(self):
			if self.wndSwitchbot.IsShow():
				self.wndSwitchbot.Close()
			else:
				self.wndSwitchbot.Open()
				
		def RefreshSwitchbotWindow(self):
			if self.wndSwitchbot and self.wndSwitchbot.IsShow():
				self.wndSwitchbot.RefreshSwitchbotWindow()

		def RefreshSwitchbotItem(self, slot):
			if self.wndSwitchbot and self.wndSwitchbot.IsShow():
				self.wndSwitchbot.RefreshSwitchbotItem(slot)

	def OnPressBtnOfflineshop(self):
		if self.wndOfflineshop.IsShow():
			self.wndOfflineshop.Close()
		else:
			self.wndOfflineshop.Open()

	def CheckGameButton(self):
		if self.wndGameButton:
			self.wndGameButton.CheckGameButton()

	def __OnClickStatusPlusButton(self):
		self.ToggleCharacterWindow("STATUS")

	def __OnClickSkillPlusButton(self):
		self.ToggleCharacterWindow("SKILL")

	def __OnClickQuestButton(self):
		self.ToggleCharacterWindow("QUEST")

	def __OnClickBuildButton(self):
		self.BUILD_OpenWindow()

	if app.ENABLE_ATTR_TRANSFER_SYSTEM:
		def OpenAttrTransferWindow(self):
			self.wndAttrTransfer.Open()
			if self.wndInventory.IsShow() == False:
				self.wndInventory.Show()

		def CloseAttrTransferWindow(self):
			self.wndAttrTransfer.Close()

		def AttrTransferSuccess(self):
			self.wndAttrTransfer.Success()

	def OpenWebWindow(self, url):
		self.wndWeb.Open(url)

		# ���������� ���� ������ ������
		self.wndChat.CloseChat()


	if app.ENABLE_MELEY_LAIR_DUNGEON:
		def OpenMeleyRanking(self):
			self.wndMeleyRanking.Open()

		def RankMeleyRanking(self, line, name, members, time):
			self.wndMeleyRanking.AddRank(line, name, members, time)

	# show GIFT
	def ShowGift(self):
		self.wndTaskBar.ShowGift()
		
	if app.ENABLE_GAYA_SYSTEM:
		def OpenGuiGaya(self):
			self.wndGayaG.Open()

		def GayaCheck(self):
			self.wndGayaG.SucceedGaya()

		def OpenGuiGayaMarket(self):
			self.wndGayaM.Open()

		def GayaMarketItems(self,vnums,gaya,count):
			self.wndGayaM.Information(vnums,gaya,count)
			self.wndGayaM.LoadInformation()

		def GayaMarketSlotsDesblock(self,slot0,slot1,slot2,slot3,slot4,slot5):
			self.wndGayaM.SlotsDesblock(slot0,slot1,slot2,slot3,slot4,slot5)

		def GayaMarketClear(self):
			self.wndGayaM.Clear()

		def GayaTime(self,time):
			self.wndGayaM.Time(time)

	if app.ENABLE_CUBE_RENEWAL_WORLDARD:
		def BINARY_CUBE_RENEWAL_OPEN(self):
			self.wndCubeRenewal.Show()
			self.wndCubeRenewal.SetCenterPosition()

	if app.ADVANCED_GUILD_INFO:
		def CloseWbWindow(self):
			self.wndRank.Close()

	def CloseWbWindow(self):
		self.wndWeb.Close()

	def OpenCubeWindow(self):
		self.wndCube.Open()
	if app.ENABLE_EXTRA_INVENTORY:
		def OpenStoneCraftWindow(self):
			if self.wndStoneCraft:
				self.wndStoneCraft.Open()

		def CloseStoneCraftWindow(self):
			if self.wndStoneCraft:
				self.wndStoneCraft.Close()
			if False == self.wndInventory.IsShow():
				self.wndInventory.Show()


	if app.ENABLE_ACCE_SYSTEM:
		# def ActAcce(self, iAct, bWindow):
			# if iAct == 1:
				# if bWindow == True:
					# if not self.wndAcceCombine.IsOpened():
						# self.wndAcceCombine.Open()
					
					# if not self.wndInventory.IsShow():
						# self.wndInventory.Show()
				# else:
					# if not self.wndAcceAbsorption.IsOpened():
						# self.wndAcceAbsorption.Open()
					
					# if not self.wndInventory.IsShow():
						# self.wndInventory.Show()
				
				# self.wndInventory.RefreshBagSlotWindow()
			# elif iAct == 2:
				# if bWindow == True:
					# if self.wndAcceCombine.IsOpened():
						# self.wndAcceCombine.Close()
				# else:
					# if self.wndAcceAbsorption.IsOpened():
						# self.wndAcceAbsorption.Close()
				
				# self.wndInventory.RefreshBagSlotWindow()
			# elif iAct == 3 or iAct == 4:
				# if bWindow == True:
					# if self.wndAcceCombine.IsOpened():
						# self.wndAcceCombine.Refresh(iAct)
				# else:
					# if self.wndAcceAbsorption.IsOpened():
						# self.wndAcceAbsorption.Refresh(iAct)
				
				# self.wndInventory.RefreshBagSlotWindow()
		
		#@ikd 
		def ActAcce(self, iAct, bWindow):
			board = (self.wndAcceAbsorption,self.wndAcceCombine)[int(bWindow)]
			if iAct == 1:
				self.ActAcceOpen(board)
				
			elif iAct == 2:
				self.ActAcceClose(board)
			
			
			elif iAct == 3 or iAct == 4:
				self.ActAcceRefresh(board, iAct)
	
		
		def ActAcceOpen(self,board):
			if not board.IsOpened():
				board.Open()
			if not self.wndInventory.IsShow():
				self.wndInventory.Show()
			self.wndInventory.RefreshBagSlotWindow()
		
		
		def ActAcceClose(self,board):
			if board.IsOpened():
				board.Close()
			self.wndInventory.RefreshBagSlotWindow()
		
		def ActAcceRefresh(self,board,iAct):
			if board.IsOpened():
				board.Refresh(iAct)
			self.wndInventory.RefreshBagSlotWindow()


	def UpdateCubeInfo(self, gold, itemVnum, count):
		self.wndCube.UpdateInfo(gold, itemVnum, count)

	def CloseCubeWindow(self):
		self.wndCube.Close()

	def FailedCubeWork(self):
		self.wndCube.Refresh()

	def SucceedCubeWork(self, itemVnum, count):
		self.wndCube.Clear()

		print "���� ���� ����! [%d:%d]" % (itemVnum, count)

		if 0: # ���� ������ ������ ���� ����
			self.wndCubeResult.SetPosition(*self.wndCube.GetGlobalPosition())
			self.wndCubeResult.SetCubeResultItem(itemVnum, count)
			self.wndCubeResult.Open()
			self.wndCubeResult.SetTop()

	def __HideWindows(self):
		hideWindows = self.wndTaskBar,\
						self.wndCharacter,\
						self.wndInventory,\
						self.wndMiniMap,\
						self.wndGuild,\
						self.wndMessenger,\
						self.wndChat,\
						self.yangText,\
						self.wndParty,\
						self.wndGameButton,
		
		if self.wndEnergyBar:
			hideWindows += self.wndEnergyBar,
		
		if app.ENABLE_CAPITALE_SYSTEM:
			if self.wndExpandedMoneyTaskbar:
				hideWindows += self.wndExpandedMoneyTaskbar,
		
		if self.wndExpandedTaskBar:
			hideWindows += self.wndExpandedTaskBar,
		
		if app.ENABLE_DRAGON_SOUL_SYSTEM:
			hideWindows += self.wndDragonSoul,\
						self.wndDragonSoulRefine,
		
		if app.ENABLE_SWITCHBOT and self.wndSwitchbot:
			hideWindows += self.wndSwitchbot,

		if self.wndOfflineshop:
			hideWindows += self.wndOfflineshop,

		if app.ENABLE_DUNGEON_INFO_SYSTEM:
			if self.wndDungeonInfo:
				hideWindows += self.wndDungeonInfo,
		
		if app.ENABLE_RANKING:
			if self.wndRanking:
				hideWindows += self.wndRanking,

		if app.ENABLE_BATTLE_PASS:
			if self.wndBattlePass and self.wndBattlePassButton:
				hideWindows += self.wndBattlePass,\
							self.wndBattlePassButton,
		
		hideWindows = filter(lambda x:x.IsShow(), hideWindows)
		map(lambda x:x.Hide(), hideWindows)
		import sys
		
		self.HideAllQuestButton()
		self.HideAllWhisperButton()
		
		if self.wndChat.IsEditMode():
			self.wndChat.CloseChat()
		
		return hideWindows

	def __ShowWindows(self, wnds):
		import sys
		map(lambda x:x.Show(), wnds)
		global IsQBHide
		if not IsQBHide:
			self.ShowAllQuestButton()
		else:
			self.HideAllQuestButton()

		self.ShowAllWhisperButton()

	def BINARY_OpenAtlasWindow(self):
		if self.wndMiniMap:
			self.wndMiniMap.ShowAtlas()

	def BINARY_SetObserverMode(self, flag):
		self.wndGameButton.SetObserverMode(flag)

	# ACCESSORY_REFINE_ADD_METIN_STONE
	def BINARY_OpenSelectItemWindow(self):
		self.wndItemSelect.Open()
	# END_OF_ACCESSORY_REFINE_ADD_METIN_STONE

	#####################################################################################
	### Private Shop ###

	if app.KASMIR_PAKET_SYSTEM:
		def OpenKasmirPaketiDialog(self):
			KasmirDialog = uimarketsystem.KasmirMarketiDialog()
			KasmirDialog.Open()
			self.KasmirDialog = KasmirDialog

	if app.KASMIR_PAKET_SYSTEM:
		def OpenPrivateShopInputNameDialog(self):
			inputDialog = uimarketsystem.InputDialogKasmir()
			inputDialog.SetTitle(localeinfo.PRIVATE_SHOP_INPUT_NAME_DIALOG_TITLE)
			inputDialog.SetMaxLength(32)
			inputDialog.SetAcceptEvent(ui.__mem_func__(self.OpenPrivateShopBuilder))
			inputDialog.SetCancelEvent(ui.__mem_func__(self.ClosePrivateShopInputNameDialog))
			inputDialog.Open()
			self.inputDialog = inputDialog

		def ClosePrivateShopInputNameDialog(self):
			self.inputDialog = None
			return True

		def OpenPrivateShopBuilder(self):

			if not self.inputDialog:
				return True

			if not len(self.inputDialog.GetText()):
				return True

			self.privateShopBuilder.Open(self.inputDialog.GetText(), 30000, 1)
			self.ClosePrivateShopInputNameDialog()
			return True

		def OpenPrivateShopKasmirInputNameDialog(self):
			inputKasmirDialog = uimarketsystem.InputDialogKasmir()
			inputKasmirDialog.SetTitle(localeinfo.PRIVATE_SHOP_INPUT_NAME_DIALOG_TITLE)
			inputKasmirDialog.SetMaxLength(32)
			inputKasmirDialog.SetAcceptEvent(ui.__mem_func__(self.OpenPrivateShopKasmirBuilder))
			inputKasmirDialog.SetCancelEvent(ui.__mem_func__(self.ClosePrivateShopKasmirInputNameDialog))
			inputKasmirDialog.Open()
			self.inputKasmirDialog = inputKasmirDialog

		def ClosePrivateShopKasmirInputNameDialog(self):
			self.inputKasmirDialog = None
			return True

		def OpenPrivateShopKasmirBuilder(self):

			if not self.inputKasmirDialog:
				return True

			if not len(self.inputKasmirDialog.GetText()):
				return True

			self.privateShopBuilder.Open(self.inputKasmirDialog.GetText(), self.inputKasmirDialog.GetNpcKasmir(), self.inputKasmirDialog.GetBaslikKasmir())
			self.ClosePrivateShopKasmirInputNameDialog()
			return True
	else:
		def OpenPrivateShopInputNameDialog(self):
			#if player.IsInSafeArea():
			#	chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.CANNOT_OPEN_PRIVATE_SHOP_IN_SAFE_AREA)
			#	return

			inputDialog = uicommon.InputDialog()
			inputDialog.SetTitle(localeinfo.PRIVATE_SHOP_INPUT_NAME_DIALOG_TITLE)
			inputDialog.SetMaxLength(32)
			inputDialog.SetAcceptEvent(ui.__mem_func__(self.OpenPrivateShopBuilder))
			inputDialog.SetCancelEvent(ui.__mem_func__(self.ClosePrivateShopInputNameDialog))
			inputDialog.Open()
			self.inputDialog = inputDialog

		def ClosePrivateShopInputNameDialog(self):
			self.inputDialog = None
			return True

		def OpenPrivateShopBuilder(self):

			if not self.inputDialog:
				return True

			if not len(self.inputDialog.GetText()):
				return True

			self.privateShopBuilder.Open(self.inputDialog.GetText())
			self.ClosePrivateShopInputNameDialog()
			return True

	def AppearPrivateShop(self, vid, text, baslik = 0):
		if app.KASMIR_PAKET_SYSTEM:
			if baslik == 1:
				board = uiprivateshopbuilder.PrivateShopAdvertisementBoardNormal()
			elif baslik == 2:
				board = uiprivateshopbuilder.PrivateShopAdvertisementBoardFire()
			elif baslik == 3:
				board = uiprivateshopbuilder.PrivateShopAdvertisementBoardIce()
			elif baslik == 4:
				board = uiprivateshopbuilder.PrivateShopAdvertisementBoardPaper()
			elif baslik == 5:
				board = uiprivateshopbuilder.PrivateShopAdvertisementBoardRibon()
			elif baslik == 6:
				board = uiprivateshopbuilder.PrivateShopAdvertisementBoardWing()
			else:
				board = uiprivateshopbuilder.PrivateShopAdvertisementBoardNormal()
		else:
			board = uiprivateshopbuilder.PrivateShopAdvertisementBoard()
		
		board.Open(vid, text)

		self.privateShopAdvertisementBoardDict[vid] = board

	def DisappearPrivateShop(self, vid):

		if not self.privateShopAdvertisementBoardDict.has_key(vid):
			return

		del self.privateShopAdvertisementBoardDict[vid]
		uiprivateshopbuilder.DeleteADBoard(vid)

	#####################################################################################
	### Equipment ###

	def OpenEquipmentDialog(self, vid):
		import uiequipmentdialog

		if app.ENABLE_PVP_ADVANCED:
			if self.equipmentDialogDict.has_key(vid):
				self.equipmentDialogDict[vid].Destroy()
				self.CloseEquipmentDialog(vid)

		dlg = uiequipmentdialog.EquipmentDialog()
		dlg.SetItemToolTip(self.tooltipItem)
		dlg.SetCloseEvent(ui.__mem_func__(self.CloseEquipmentDialog))
		dlg.Open(vid)

		self.equipmentDialogDict[vid] = dlg

	def SetEquipmentDialogItem(self, vid, slotIndex, vnum, count):
		if not vid in self.equipmentDialogDict:
			return
		self.equipmentDialogDict[vid].SetEquipmentDialogItem(slotIndex, vnum, count)

	def SetEquipmentDialogSocket(self, vid, slotIndex, socketIndex, value):
		if not vid in self.equipmentDialogDict:
			return
		self.equipmentDialogDict[vid].SetEquipmentDialogSocket(slotIndex, socketIndex, value)

	def SetEquipmentDialogAttr(self, vid, slotIndex, attrIndex, type, value):
		if not vid in self.equipmentDialogDict:
			return
		self.equipmentDialogDict[vid].SetEquipmentDialogAttr(slotIndex, attrIndex, type, value)

	def CloseEquipmentDialog(self, vid):
		if not vid in self.equipmentDialogDict:
			return

		if app.ENABLE_PVP_ADVANCED:
			if self.equipmentDialogDict.has_key(vid):
				self.equipmentDialogDict[vid].Destroy()

		del self.equipmentDialogDict[vid]

	#####################################################################################

	#####################################################################################
	### Quest ###
	def BINARY_ClearQuest(self, index):
		btn = self.__FindQuestButton(index)
		if 0 != btn:
			self.__DestroyQuestButton(btn)

	def RecvQuest(self, index, name):
		# QUEST_LETTER_IMAGE
		self.BINARY_RecvQuest(index, name, "file", localeinfo.GetLetterImageName())
		# END_OF_QUEST_LETTER_IMAGE

	def BINARY_RecvQuest(self, index, name, iconType, iconName):

		btn = self.__FindQuestButton(index)
		if 0 != btn:
			self.__DestroyQuestButton(btn)

		btn = uiwhisper.WhisperButton()

		# QUEST_LETTER_IMAGE
		##!! 20061026.levites.������_������_����
		import item
		if "item"==iconType:
			item.SelectItem(int(iconName))
			buttonImageFileName=item.GetIconImageFileName()
		else:
			buttonImageFileName=iconName

		if iconName and (iconType not in ("item", "file")): # type "ex" implied
			btn.SetUpVisual("d:/ymir work/ui/game/quest/questicon/%s" % (iconName.replace("open", "close")))
			btn.SetOverVisual("d:/ymir work/ui/game/quest/questicon/%s" % (iconName))
			btn.SetDownVisual("d:/ymir work/ui/game/quest/questicon/%s" % (iconName))
		else:
			if localeinfo.IsEUROPE():
				btn.SetUpVisual(localeinfo.GetLetterCloseImageName())
				btn.SetOverVisual(localeinfo.GetLetterOpenImageName())
				btn.SetDownVisual(localeinfo.GetLetterOpenImageName())
			else:
				btn.SetUpVisual(buttonImageFileName)
				btn.SetOverVisual(buttonImageFileName)
				btn.SetDownVisual(buttonImageFileName)
				btn.Flash()
		# END_OF_QUEST_LETTER_IMAGE

		if localeinfo.IsARABIC():
			btn.SetToolTipText(name, 0, 35)
			btn.ToolTipText.SetHorizontalAlignCenter()
		else:
			btn.SetToolTipText(name, -20, 35)
			btn.ToolTipText.SetHorizontalAlignLeft()

		listOfTypes = iconType.split(",")
		if "blink" in listOfTypes:
			btn.Flash()

		listOfColors = {
			"golden":	0xFFffa200,
			"green":	0xFF00e600,
			"blue":		0xFF0099ff,
			"purple":	0xFFcc33ff,

			"fucsia":	0xFFcc0099,
			"aqua":		0xFF00ffff,
		}
		for k,v in listOfColors.iteritems():
			if k in listOfTypes:
				btn.ToolTipText.SetPackedFontColor(v)

		if not app.ENABLE_QUEST_RENEWAL:
			if localeinfo.IsARABIC():
				btn.SetToolTipText(name, -20, 35)
				btn.ToolTipText.SetHorizontalAlignRight()
			else:
				btn.SetToolTipText(name, -20, 35)
				btn.ToolTipText.SetHorizontalAlignLeft()

			btn.SetEvent(ui.__mem_func__(self.__StartQuest), btn)
			btn.Show()
		else:
			btn.SetEvent(ui.__mem_func__(self.__StartQuest), btn)
			
		btn.Show()

		btn.index = index
		btn.name = name

		self.questButtonList.insert(0, btn)
		self.__ArrangeQuestButton()

	def __ArrangeQuestButton(self):

		screenWidth = wndMgr.GetScreenWidth()
		screenHeight = wndMgr.GetScreenHeight()

		##!! 20061026.levites.������_����_����
		if self.wndParty.IsShow():
			xPos = 100 + 30
		else:
			xPos = 20

		if localeinfo.IsARABIC():
			xPos = xPos + 15

		yPos = 170 * screenHeight / 600
		yCount = (screenHeight - 330) / 63

		count = 0
		for btn in self.questButtonList:
		
			if app.ENABLE_QUEST_RENEWAL:
				btn.SetToolTipText(str(len(self.questButtonList)))
				btn.ToolTipText.SetPosition(13, 37)


			btn.SetPosition(xPos + (int(count/yCount) * 100), yPos + (count%yCount * 63))
			count += 1
			global IsQBHide
			if IsQBHide:
				btn.Hide()
			else:
				if app.ENABLE_QUEST_RENEWAL and count > 0:
					btn.Hide()
				else:
					btn.Show()

	def __StartQuest(self, btn):
		if app.ENABLE_QUEST_RENEWAL:
			self.__OnClickQuestButton()
			self.HideAllQuestButton()
		else:
			event.QuestButtonClick(btn.index)
			self.__DestroyQuestButton(btn)

	def __FindQuestButton(self, index):
		for btn in self.questButtonList:
			if btn.index == index:
				return btn

		return 0

	def __DestroyQuestButton(self, btn):
		btn.SetEvent(0)
		self.questButtonList.remove(btn)
		self.__ArrangeQuestButton()

	def HideAllQuestButton(self):
		for btn in self.questButtonList:
			btn.Hide()

	def ShowAllQuestButton(self):
		for btn in self.questButtonList:
			btn.Show()
			if app.ENABLE_QUEST_RENEWAL:
				break
	#####################################################################################

	#####################################################################################
	### Whisper ###

	def __InitWhisper(self):
		chat.InitWhisper(self)

	## �������� "������ ������"�� �������� ���� ���� �������� ���� ����
	## ������ ���� ������ ������ WhisperDialogDict �� ������ ��������.
	def OpenWhisperDialogWithoutTarget(self):
		if not self.dlgWhisperWithoutTarget:
			dlgWhisper = uiwhisper.WhisperDialog(self.MinimizeWhisperDialog, self.CloseWhisperDialog)
			dlgWhisper.BindInterface(self)
			dlgWhisper.LoadDialog()
			dlgWhisper.OpenWithoutTarget(self.RegisterTemporaryWhisperDialog)
			dlgWhisper.SetPosition(self.windowOpenPosition*30,self.windowOpenPosition*30)
			dlgWhisper.Show()
			self.dlgWhisperWithoutTarget = dlgWhisper

			self.windowOpenPosition = (self.windowOpenPosition+1) % 5

		else:
			self.dlgWhisperWithoutTarget.SetTop()
			self.dlgWhisperWithoutTarget.OpenWithoutTarget(self.RegisterTemporaryWhisperDialog)

	## ���� ���� ���������� ������ ���������� WhisperDialogDict�� ���� �������� ����
	def RegisterTemporaryWhisperDialog(self, name):
		if not self.dlgWhisperWithoutTarget:
			return

		btn = self.__FindWhisperButton(name)
		if 0 != btn:
			self.__DestroyWhisperButton(btn)

		elif self.whisperDialogDict.has_key(name):
			oldDialog = self.whisperDialogDict[name]
			oldDialog.Destroy()
			del self.whisperDialogDict[name]

		self.whisperDialogDict[name] = self.dlgWhisperWithoutTarget
		self.dlgWhisperWithoutTarget.OpenWithTarget(name)
		self.dlgWhisperWithoutTarget = None
		self.__CheckGameMaster(name)
		if app.ENABLE_MULTI_LANGUAGE:
			self.CheckLangOnWhisper(name)

	## ������ ������ 1:1 ���� ������ �������� ������ ������ ���� ���� ���� ����
	def OpenWhisperDialog(self, name):
		if not self.whisperDialogDict.has_key(name):
			dlg = self.__MakeWhisperDialog(name)
			dlg.OpenWithTarget(name)
			dlg.chatLine.SetFocus()
			dlg.Show()
			
			self.__CheckGameMaster(name)
			if app.ENABLE_MULTI_LANGUAGE:
				self.CheckLangOnWhisper(name)
			
			btn = self.__FindWhisperButton(name)
			if 0 != btn:
				self.__DestroyWhisperButton(btn)

	## ���� ������������ �������� �������� ���� ������ ���� ���� ����
	def RecvWhisper(self, name):
		if not self.whisperDialogDict.has_key(name):
			btn = self.__FindWhisperButton(name)
			if 0 == btn:
				btn = self.__MakeWhisperButton(name)
				btn.Flash()
				chat.AppendChat(chat.CHAT_TYPE_NOTICE, localeinfo.RECEIVE_MESSAGE % (name))
			else:
				btn.Flash()
		elif self.IsGameMasterName(name):
			dlg = self.whisperDialogDict[name]
			dlg.SetGameMasterLook()
		
		if app.ENABLE_MULTI_LANGUAGE:
			self.CheckLangOnWhisper(name)

	if app.ENABLE_MULTI_LANGUAGE:
		def RecvLangOnWhisper(self, name, lang):
			self.listLangName[name] = lang
			
			try:
				dlg = self.whisperDialogDict[name]
				dlg.RefreshLangMark(lang)
			except:
				pass

		def CheckLangOnWhisper(self, name):
			try:
				dlg = self.whisperDialogDict[name]
				lang = 0
				if self.listLangName.has_key(name):
					lang = self.listLangName[name]
					dlg.RefreshLangMark(lang)
				else:
					dlg.HideLangMark()
					net.RequestTargetLang(name)
			except:
				pass

	def MakeWhisperButton(self, name):
		self.__MakeWhisperButton(name)

	## ������ �������� ���� ���� ����
	def ShowWhisperDialog(self, btn):
		try:
			self.__MakeWhisperDialog(btn.name)
			dlgWhisper = self.whisperDialogDict[btn.name]
			dlgWhisper.OpenWithTarget(btn.name)
			dlgWhisper.Show()
			self.__CheckGameMaster(btn.name)
			if app.ENABLE_MULTI_LANGUAGE:
				self.CheckLangOnWhisper(btn.name)
		except:
			import dbg
			dbg.TraceError("interface.ShowWhisperDialog - Failed to find key")

		## ���� ������
		self.__DestroyWhisperButton(btn)

	## WhisperDialog ������ ������ ������ ���������� �������� ����
	## ���� ������ ������.
	def MinimizeWhisperDialog(self, name):

		if 0 != name:
			self.__MakeWhisperButton(name)

		self.CloseWhisperDialog(name)

	## WhisperDialog ������ ���� ������ ���������� �������� ����
	## ���� ��������.
	def CloseWhisperDialog(self, name):

		if 0 == name:

			if self.dlgWhisperWithoutTarget:
				self.dlgWhisperWithoutTarget.Destroy()
				self.dlgWhisperWithoutTarget = None

			return

		try:
			dlgWhisper = self.whisperDialogDict[name]
			dlgWhisper.Destroy()
			del self.whisperDialogDict[name]
		except:
			import dbg
			dbg.TraceError("interface.CloseWhisperDialog - Failed to find key")

	## ������ ������ ���������� ������ ������ ���� ����
	def __ArrangeWhisperButton(self):

		screenWidth = wndMgr.GetScreenWidth()
		screenHeight = wndMgr.GetScreenHeight()

		xPos = screenWidth - 70
		yPos = (170 * screenHeight / 600) + 16
		yCount = (screenHeight - 330) / 63
		#yCount = (screenHeight - 285) / 63

		count = 0
		for button in self.whisperButtonList:

			button.SetPosition(xPos + (int(count/yCount) * -50), yPos + (count%yCount * 63))
			count += 1

	## �������� Whisper ������ ���� ������ ���� ����
	## ������ ���������� ���� ���� ���� ���� ���� ���� ������ ���� ���� ������
	## ���� ���� ToolTip���� ���� �������� ���� �������� ��������.
	def __FindWhisperButton(self, name):
		for button in self.whisperButtonList:
			if button.name == name:
				return button

		return 0

	## ���� ��������.
	def __MakeWhisperDialog(self, name):
		dlgWhisper = uiwhisper.WhisperDialog(self.MinimizeWhisperDialog, self.CloseWhisperDialog)
		dlgWhisper.BindInterface(self)
		dlgWhisper.LoadDialog()
		dlgWhisper.SetPosition(self.windowOpenPosition*30,self.windowOpenPosition*30)
		self.whisperDialogDict[name] = dlgWhisper

		self.windowOpenPosition = (self.windowOpenPosition+1) % 5

		return dlgWhisper

	## ������ ��������.
	def __MakeWhisperButton(self, name):
		whisperButton = uiwhisper.WhisperButton()
		whisperButton.SetUpVisual("d:/ymir work/ui/game/windows/btn_mail_up.sub")
		whisperButton.SetOverVisual("d:/ymir work/ui/game/windows/btn_mail_up.sub")
		whisperButton.SetDownVisual("d:/ymir work/ui/game/windows/btn_mail_up.sub")
		if self.IsGameMasterName(name):
			whisperButton.SetToolTipTextWithColor(name, 0xffffa200)
		else:
			whisperButton.SetToolTipText(name)
		whisperButton.ToolTipText.SetHorizontalAlignCenter()
		whisperButton.SetEvent(ui.__mem_func__(self.ShowWhisperDialog), whisperButton)
		whisperButton.Show()
		whisperButton.name = name

		self.whisperButtonList.insert(0, whisperButton)
		self.__ArrangeWhisperButton()

		return whisperButton

	def __DestroyWhisperButton(self, button):
		button.SetEvent(0)
		self.whisperButtonList.remove(button)
		self.__ArrangeWhisperButton()

	def HideAllWhisperButton(self):
		for btn in self.whisperButtonList:
			btn.Hide()

	def ShowAllWhisperButton(self):
		for btn in self.whisperButtonList:
			btn.Show()

	def __CheckGameMaster(self, name):
		if not self.listGMName.has_key(name):
			return
		if self.whisperDialogDict.has_key(name):
			dlg = self.whisperDialogDict[name]
			dlg.SetGameMasterLook()

	def RegisterGameMasterName(self, name):
		if self.listGMName.has_key(name):
			return
		self.listGMName[name] = "GM"

	def IsGameMasterName(self, name):
		if self.listGMName.has_key(name):
			return True
		else:
			return False

	#####################################################################################

	#####################################################################################
	### Guild Building ###

	def BUILD_OpenWindow(self):
		self.wndGuildBuilding = uiguild.BuildGuildBuildingWindow()
		self.wndGuildBuilding.Open()
		self.wndGuildBuilding.wnds = self.__HideWindows()
		self.wndGuildBuilding.SetCloseEvent(ui.__mem_func__(self.BUILD_CloseWindow))

	def BUILD_CloseWindow(self):
		self.__ShowWindows(self.wndGuildBuilding.wnds)
		self.wndGuildBuilding = None

	def BUILD_OnUpdate(self):
		if not self.wndGuildBuilding:
			return

		if self.wndGuildBuilding.IsPositioningMode():
			import background
			x, y, z = background.GetPickingPoint()
			self.wndGuildBuilding.SetBuildingPosition(x, y, z)

	def BUILD_OnMouseLeftButtonDown(self):
		if not self.wndGuildBuilding:
			return

		# GUILD_BUILDING
		if self.wndGuildBuilding.IsPositioningMode():
			self.wndGuildBuilding.SettleCurrentPosition()
			return True
		elif self.wndGuildBuilding.IsPreviewMode():
			pass
		else:
			return True
		# END_OF_GUILD_BUILDING
		return False

	def BUILD_OnMouseLeftButtonUp(self):
		if not self.wndGuildBuilding:
			return

		if not self.wndGuildBuilding.IsPreviewMode():
			return True

		return False

	def BULID_EnterGuildArea(self, areaID):
		# GUILD_BUILDING
		mainCharacterName = player.GetMainCharacterName()
		masterName = guild.GetGuildMasterName()

		if mainCharacterName != masterName:
			return

		if areaID != player.GetGuildID():
			return
		# END_OF_GUILD_BUILDING

		self.wndGameButton.ShowBuildButton()

	def BULID_ExitGuildArea(self, areaID):
		self.wndGameButton.HideBuildButton()
	if app.__BL_PICK_FILTER__:
		def OpenPickUpWindow(self):
			if self.wndPickUpFilter:
				self.wndPickUpFilter.Open()
	#####################################################################################

	def IsEditLineFocus(self):
		if self.ChatWindow.chatLine.IsFocus():
			return 1

		if self.ChatWindow.chatToLine.IsFocus():
			return 1

		return 0
	if app.AUTO_SHOUT:
		def OpenShoutWindow(self):
			if self.wndShout.IsShow():
				self.wndShout.Hide()
			else:
				self.wndShout.Open()
	if app.ENABLE_DEFENSE_WAVE:
		def BINARY_Update_Mast_HP(self, hp):
			self.wndMiniMap.SetMastHP(hp)

		def BINARY_Update_Mast_Timer(self, text):
			self.wndMiniMap.setMastTimer(text)

		def BINARY_Update_Mast_Window(self, i):
			self.wndMiniMap.SetMastWindow(i)

	def GetInventoryPageIndex(self):
		if self.wndInventory:
			return self.wndInventory.GetInventoryPageIndex()
		else:
			return -1

	if app.WJ_ENABLE_TRADABLE_ICON:
		def SetOnTopWindow(self, onTopWnd):
			self.onTopWindow = onTopWnd

		def GetOnTopWindow(self):
			return self.onTopWindow

		def RefreshMarkInventoryBag(self):
			self.wndInventory.RefreshMarkSlots()
			self.wndExtraInventory.RefreshMarkSlots()

	def EmptyFunction(self):
		pass

	if app.ENABLE_EXTRA_INVENTORY:
		def ToggleExtraInventoryWindow(self):
			if not player.IsObserverMode():
				if not self.wndExtraInventory.IsShow():
					self.wndExtraInventory.Open()
					self.wndExtraInventory.SetTop()
				else:
					self.wndExtraInventory.Close()


	if app.ENABLE_HIDE_COSTUME_SYSTEM:
		def RefreshVisibleCostume(self):
			self.wndInventory.RefreshVisibleCostume()

	if app.ENABLE_RANKING:
		def OpenRanking(self):
			if self.wndRanking:
				self.wndRanking.Open()

		def RankingClearData(self):
			if self.wndRanking:
				self.wndRanking.ClearData()

		def RankingAddRank(self, position, level, points, name, realPosition):
			if self.wndRanking:
				self.wndRanking.AddRank(position, name, points, level, realPosition)

		def RankingRefresh(self):
			if self.wndRanking:
				self.wndRanking.RefreshList()
				self.wndRanking.OnScroll()

	def RefreshExpBtn(self):
		if self.wndTaskBar:
			self.wndTaskBar.RefreshExpBtn()

	if app.ENABLE_WHISPER_ADMIN_SYSTEM:
		def OpenWhisperSystem(self):
			if not self.wndWhisperManager:
				return
			
			if self.wndWhisperManager.IsShow():
				self.wndWhisperManager.Hide()
			else:
				self.wndWhisperManager.Show()

	if app.ENABLE_RUNE_SYSTEM:
		def RuneStatus(self, flag):
			if not self.wndRune:
				return
			
			self.wndRune.RuneStatus(flag)

		def RuneAffect(self, flag):
			if not self.wndRune:
				return
			
			self.wndRune.RuneAffect(flag)

		def Rune(self):
			if not self.wndRune:
				return
			
			if self.wndRune.IsShow():
				self.wndRune.Close()
			else:
				self.wndRune.Open()

	# if app.ENABLE_PVP_ADVANCED:
		# def OnRefreshBtnPvpMinimap(self):
			# if self.wndMiniMap:
				# self.wndMiniMap.OnRefreshBtnPvp()

	if app.ENABLE_SORT_INVEN:
		def Sort_InventoryDone(self):
			if self.wndInventory:
				self.wndInventory.Sort_InventoryDone()

		def Sort_ExtraInventoryDone(self):
			if self.wndExtraInventory:
				self.wndExtraInventory.Sort_ExtraInventoryDone()

	if app.ENABLE_NEW_CHAT:
		def ChatLogStat(self):
			stat = False
			if self.wndChatLog:
				stat = self.wndChatLog.IsShow()
			return stat

		def ChatChatStat(self):
			stat = False
			if self.wndChat:
				stat = self.wndChat.IsShow()
			return stat

	if app.ENABLE_BIOLOGIST_UI:
		def ClickBiologistButton(self):
			if self.wndBiologist != None:
				self.wndBiologist.RequestOpen()

		def OpenBiologist(self, args):
			if self.wndBiologist != None:
				self.wndBiologist.Open(args)

		def DeliveredBiologist(self, args):
			if self.wndBiologist != None:
				self.wndBiologist.Delivered(args)

		def RewardBiologist(self, args):
			if self.wndBiologist != None:
				self.wndBiologist.Reward(args)

		def CloseBiologist(self):
			if self.wndBiologist != None:
				self.wndBiologist.Close()

		def NextBiologist(self, args):
			if self.wndBiologist != None:
				self.wndBiologist.Next(args)

		def OpenBiologistChange(self):
			if self.wndBiologistChange != None:
				self.wndBiologistChange.Open()

		def AppendBiologistChange(self, args):
			if self.wndBiologistChange != None:
				self.wndBiologistChange.Append(args)

		def ClearBiologistChange(self):
			if self.wndBiologistChange != None:
				self.wndBiologistChange.Clear()

		def CloseBiologistChange(self):
			if self.wndBiologistChange != None:
				self.wndBiologistChange.Close()

	if app.ENABLE_NEW_FISHING_SYSTEM:
		def OnFishingStart(self, have, need):
			if self.wndFishingWindow and not self.wndFishingWindow.IsShow():
				self.wndFishingWindow.OnOpen(have, need)

		def OnFishingStop(self):
			if self.wndFishingWindow and self.wndFishingWindow.IsShow():
				self.wndFishingWindow.OnClose()

		def OnFishingCatch(self, have):
			if self.wndFishingWindow and self.wndFishingWindow.IsShow():
				self.wndFishingWindow.OnCatch(have)

		def OnFishingCatchFailed(self):
			if self.wndFishingWindow and self.wndFishingWindow.IsShow():
				self.wndFishingWindow.OnCatchFailed()

	def ToggleWikiNew(self):
		if app.INGAME_WIKI:
			net.ToggleWikiWindow()

	if app.ENABLE_SAVEPOINT_SYSTEM:
		def ClickBtnSavepoint(self):
			if self.wndSavePoint != None:
				self.wndSavePoint.RequestOpen()

		def AppendSavepoint(self, id, name, mapIndex, x, y):
			if self.wndSavePoint != None:
				self.wndSavePoint.Append(id, name, mapIndex, x, y)

		def UpdateSavepoint(self, id, name, mapIndex, x, y):
			if self.wndSavePoint != None:
				self.wndSavePoint.Update(id, name, mapIndex, x, y)

		def OpenSavepoint(self):
			if self.wndSavePoint != None:
				self.wndSavePoint.Open()

	if app.ENABLE_EVENT_MANAGER:
		def MakeEventIcon(self):
			if self.wndEventIcon == None:
				self.wndEventIcon = uiEventCalendar.MovableImage()
				self.wndEventIcon.Show()
		def MakeEventCalendar(self):
			if self.wndEventManager == None:
				self.wndEventManager = uiEventCalendar.EventCalendarWindow()
		def OpenEventCalendar(self):
			self.MakeEventCalendar()
			if self.wndEventManager.IsShow():
				self.wndEventManager.Close()
			else:
				self.wndEventManager.Open()
		def RefreshEventStatus(self, eventID, eventStatus, eventendTime, eventEndTimeText):
			if eventendTime != 0:
				eventendTime += app.GetGlobalTimeStamp()
			uiEventCalendar.SetEventStatus(eventID, eventStatus, eventendTime, eventEndTimeText)
			self.RefreshEventManager()
		def ClearEventManager(self):
			uiEventCalendar.server_event_data={}
		def RefreshEventManager(self):
			if self.wndEventManager:
				self.wndEventManager.Refresh()
			if self.wndEventIcon:
				self.wndEventIcon.Refresh()
		def AppendEvent(self, dayIndex, eventID, eventIndex, startTime, endTime, empireFlag, channelFlag, value0, value1, value2, value3, startRealTime, endRealTime, isAlreadyStart):
			self.MakeEventCalendar()
			self.MakeEventIcon()
			if startRealTime != 0:
				startRealTime += app.GetGlobalTimeStamp()
			if endRealTime != 0:
				endRealTime += app.GetGlobalTimeStamp()
			uiEventCalendar.SetServerData(dayIndex, eventID, eventIndex, startTime, endTime, empireFlag, channelFlag, value0, value1, value2, value3, startRealTime, endRealTime, isAlreadyStart)
