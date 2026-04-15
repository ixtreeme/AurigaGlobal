import ui, constinfo, localeinfo, mousemodule, uicommon, uiscriptlocale
import net
import player
import item

class wnd(ui.ScriptWindow):
	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.status = 0
		self.flag = None
		self.effectButton = None
		self.activateButton = None
		self.negButton = None
		self.atLeast = None
		self.wndItem = None
		self.titleBar = None
		self.isLoaded = 0

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def LoadWindow(self):
		if self.isLoaded == 1:
			return
		
		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "uiscript/runewindow.py")
		except:
			import exception
			exception.Abort("runewindow.LoadWindow.LoadObject")
		
		try:
			self.titleBar = self.GetChild("TitleBar")
			self.wndItem = self.GetChild("ItemSlot")
			self.effectButton = self.GetChild("effect")
			self.activateButton = self.GetChild("activate")
			self.negButton = self.GetChild("shop")
			self.effBonus = self.GetChild("eff_bonus")
			self.eff1List = []
			self.eff2List = []
			for i in xrange(player.RUNE_SLOT_COUNT):
				self.eff1List.append(self.GetChild("eff1_rune_%d" % (i)))
				self.eff2List.append(self.GetChild("eff2_rune_%d" % (i)))
		except:
			import exception
			exception.Abort("InventoryWindow.LoadWindow.BindObject")
		
		self.titleBar.SetCloseEvent(ui.__mem_func__(self.Close))
		self.effectButton.SetToggleUpEvent(ui.__mem_func__(self.EffectButtonClick2))
		self.effectButton.SetToggleDownEvent(ui.__mem_func__(self.EffectButtonClick1))
		self.negButton.SetEvent(ui.__mem_func__(self.NegButtonClick))
		self.activateButton.SetToggleUpEvent(ui.__mem_func__(self.ActivateButtonClick))
		self.activateButton.SetToggleDownEvent(ui.__mem_func__(self.ActivateButtonClick))
		
		self.questionDialog = uicommon.QuestionDialog()
		self.questionDialog.SetText("")
		self.questionDialog.SetWidth(350)
		self.questionDialog.Close()
		
		self.wndItem.SetOverInItemEvent(ui.__mem_func__(self.OverInItem))
		self.wndItem.SetOverOutItemEvent(ui.__mem_func__(self.OverOutItem))
		self.wndItem.SetSelectItemSlotEvent(ui.__mem_func__(self.SelectItemSlot))
		self.isLoaded = 1
		self.Hide()

	def Open(self):
		self.LoadWindow()
		if self.status == 2:
			import chat
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.RUNE_IS_UNLOCKED1)
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.RUNE_IS_UNLOCKED2)
			self.Close()
			return
		elif self.status != 1:
			import chat
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.RUNE_LOCKED)
			self.Close()
			return
		
		ui.ScriptWindow.Show(self)
		self.SetTop()
		self.RefreshItemSlot()
		self.RefreshRuneEffectButton()

	def Close(self):
		if self.tooltipItem != None:
			self.tooltipItem.HideToolTip()
		
		self.Hide()

	def Destroy(self):
		self.ClearDictionary()
		self.status = 0
		self.flag = None
		self.effectButton = None
		self.activateButton = None
		self.negButton = None
		self.atLeast = None
		self.wndItem = None
		self.titleBar = None
		self.tooltipItem = None
		self.effBonus = None
		self.eff1List = None
		self.eff2List = None

	def SetItemToolTip(self, tooltipItem):
		self.tooltipItem = tooltipItem

	def RuneAffect(self, flag):
		self.flag = flag
		self.RefreshRuneEffectButton()

	def RuneStatus(self, flag):
		self.status = flag

	def RefreshRuneEffectButton(self):
		if self.effectButton != None and self.flag != None and self.IsShow():
			if self.flag == 1:
				self.effectButton.SetUp()
			else:
				self.effectButton.Down()

	def RefreshItemSlot(self):
		if self.wndItem == None:
			return
		
		self.atLeast = False
		for i in xrange(player.RUNE_SLOT_COUNT):
			slotNumber = player.RUNE_SLOT_START + i
			itemVnum = player.GetItemIndex(slotNumber)
			if not itemVnum:
				self.wndItem.ClearSlot(slotNumber)
				try:
					self.eff1List[i].Hide()
					self.eff2List[i].Hide()
					if slotNumber == player.RUNE_SLOT_START + (player.RUNE_SLOT_COUNT - 1):
						self.effBonus.Hide()
					
					continue
				except:
					continue
			
			itemCount = player.GetItemCount(slotNumber)
			if itemCount <= 1:
				itemCount = 0
			
			iconDisable = "icon/item/%d_1.tga" % (itemVnum)
			iconOver = "icon/item/%d_2.tga" % (itemVnum)
			iconNormal = iconOver
			
			self.wndItem.SetItemSlot(slotNumber, itemVnum, itemCount)
			metinSocket = [player.GetItemMetinSocket(slotNumber, j) for j in xrange(player.METIN_SOCKET_MAX_NUM)]
			isActivated = metinSocket[1] != 0
			if isActivated:
				self.atLeast = True
				try:
					self.eff1List[i].Show()
					self.eff2List[i].Show()
				except:
					continue
				
				if slotNumber == player.RUNE_SLOT_START + (player.RUNE_SLOT_COUNT - 1):
					self.effBonus.Show()
					icon = "icon/item/%d.tga" % (itemVnum)
					self.wndItem.SetCoverButton(slotNumber, icon, icon, icon, icon, False, False)
				else:
					self.wndItem.SetCoverButton(slotNumber, iconNormal, iconDisable, iconDisable, iconDisable, False, False)
			else:
				try:
					self.eff1List[i].Hide()
					self.eff2List[i].Hide()
				except:
					continue
				
				if slotNumber == player.RUNE_SLOT_START + (player.RUNE_SLOT_COUNT - 1):
					self.effBonus.Hide()
				
				self.wndItem.SetCoverButton(slotNumber, iconDisable, iconOver, iconNormal, iconDisable, False, False)
		
		if self.atLeast:
			self.activateButton.SetText(uiscriptlocale.RUNE_DEACTIVE)
			self.activateButton.Down()
		else:
			self.activateButton.SetText(uiscriptlocale.RUNE_ACTIVE)
			self.activateButton.SetUp()
		
		self.wndItem.RefreshSlot()

	def NegButtonClick(self):
		net.SendChatPacket("/rune_shop")

	def EffectButtonClick1(self):
		net.SendChatPacket("/rune_effect 0")
		self.effectButton.Down()

	def EffectButtonClick2(self):
		net.SendChatPacket("/rune_effect 1")
		self.effectButton.SetUp()

	def ActivateButtonClick(self):
		if self.atLeast == True:
			self.OnQuestionDialog("/rune l 0", localeinfo.RUNE_DEACTIVATE_ALL, True)
			self.activateButton.Down()
		else:
			self.OnQuestionDialog("/rune l 1", localeinfo.RUNE_ACTIVATE_ALL, True)
			self.activateButton.SetUp()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def OnQuestionDialog(self, event, text, btn = False):
		if self.questionDialog.IsShow():
			self.questionDialog.SetTop()
			return
		
		self.questionDialog.SetText(text)
		if btn == True:
			self.questionDialog.SetAcceptEvent(lambda arg1 = True, arg2 = event: self.OnCloseQuestionDialogBtn(arg1, arg2))
			self.questionDialog.SetCancelEvent(lambda arg1 = False, arg2 = event: self.OnCloseQuestionDialogBtn(arg1, arg2))
		else:
			self.questionDialog.SetAcceptEvent(lambda arg1 = True, arg2 = event: self.OnCloseQuestionDialog(arg1, arg2))
			self.questionDialog.SetCancelEvent(lambda arg1 = False, arg2 = event: self.OnCloseQuestionDialog(arg1, arg2))
		
		self.questionDialog.Open()

	def OnCloseQuestionDialog(self, answer, event):
		if not self.questionDialog:
			return False
		
		self.questionDialog.Close()
		if not answer:
			return False
		
		net.SendChatPacket(event)
		return True

	def OnCloseQuestionDialogBtn(self, answer, event):
		if not self.questionDialog:
			return False
		
		self.questionDialog.Close()
		self.RefreshItemSlot()
		if not answer:
			return False
		
		net.SendChatPacket(event)
		return True

	def OverInItem(self, overSlotPos):
		if self.tooltipItem != None:
			self.tooltipItem.SetInventoryItem(overSlotPos)

	def OverOutItem(self):
		if self.tooltipItem != None:
			self.tooltipItem.HideToolTip()

	def SelectItemSlot(self, slotNumber):
		runeBonus = player.RUNE_SLOT_START + (player.RUNE_SLOT_COUNT - 1)
		if mousemodule.mouseController.isAttached():
			if slotNumber == runeBonus:
				mousemodule.mouseController.DeattachObject()
				return
			
			can = False
			potionName = ""
			targetName = ""
			attachedItemType = mousemodule.mouseController.GetAttachedType()
			if player.SLOT_TYPE_INVENTORY == attachedItemType:
				attachedItemVnum = mousemodule.mouseController.GetAttachedItemIndex()
				attachedSlotPos = mousemodule.mouseController.GetAttachedSlotNumber()
				
				if attachedItemVnum:
					item.SelectItem(attachedItemVnum)
					potionName = item.GetItemName()
					if item.GetItemType() == item.ITEM_TYPE_USE and item.GetItemSubType() == item.USE_RUNE_PERC_CHARGE:
						itemVnum = player.GetItemIndex(slotNumber)
						if itemVnum:
							item.SelectItem(itemVnum)
							targetName = item.GetItemName()
							can = True
			
			if can == True:
				self.OnQuestionDialog("/rune_charge %d %d" % (slotNumber - player.EQUIPMENT_SLOT_START, attachedSlotPos), localeinfo.RUNE_CHARGE % (targetName, potionName))
			
			mousemodule.mouseController.DeattachObject()
			return
		else:
			if slotNumber == runeBonus:
				return
			
			itemVnum = player.GetItemIndex(slotNumber)
			if not itemVnum:
				return
			
			item.SelectItem(itemVnum)
			metinSocket = [player.GetItemMetinSocket(slotNumber, j) for j in xrange(player.METIN_SOCKET_MAX_NUM)]
			isActivated = metinSocket[1] != 0
			if isActivated:
				self.OnQuestionDialog("/rune d %d" % (slotNumber - player.EQUIPMENT_SLOT_START), localeinfo.RUNE_DEACTIVATE % (item.GetItemName()))
			else:
				self.OnQuestionDialog("/rune a %d" % (slotNumber - player.EQUIPMENT_SLOT_START), localeinfo.RUNE_ACTIVATE % (item.GetItemName()))

