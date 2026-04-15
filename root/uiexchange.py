import app
import net
import player
import ui
import grp
import chat
import item
import time
import wndMgr
import uicommon
import exchange
import mousemodule
import uipickmoney
import playersettingmodule
import localeinfo

if app.__ENABLE_NEW_OFFLINESHOP__:
	import uiofflineshop

NEW_YANG_LIMIT = 1

class ExchangeDialog(ui.ScriptWindow):
	FACE_IMAGE_DICT = {
						playersettingmodule.RACE_WARRIOR_M : "icon/face/warrior_m.tga",
						playersettingmodule.RACE_WARRIOR_W : "icon/face/warrior_w.tga",
						playersettingmodule.RACE_ASSASSIN_M : "icon/face/assassin_m.tga",
						playersettingmodule.RACE_ASSASSIN_W : "icon/face/assassin_w.tga",
						playersettingmodule.RACE_SURA_M : "icon/face/sura_m.tga",
						playersettingmodule.RACE_SURA_W : "icon/face/sura_w.tga",
						playersettingmodule.RACE_SHAMAN_M : "icon/face/shaman_m.tga",
						playersettingmodule.RACE_SHAMAN_W : "icon/face/shaman_w.tga",
	}
	
	if app.ENABLE_WOLFMAN_CHARACTER:
		FACE_IMAGE_DICT.update({playersettingmodule.RACE_WOLFMAN_M : "icon/face/wolfman_m.tga",})

	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.TitleName = 0
		self.tooltipItem = 0
		self.xStart = 0
		self.yStart = 0
		if app.WJ_ENABLE_TRADABLE_ICON:
			self.interface = 0
			self.wndInventory = 0
			self.wndExtraInventory = 0
			self.lockedItems = {i:(-1,-1) for i in range(exchange.EXCHANGE_ITEM_MAX_NUM)}
			self.lockedExtraItems = {i:(-1,-1) for i in range(exchange.EXCHANGE_ITEM_MAX_NUM)}

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	class Item(ui.ListBoxEx.Item):
		def __init__(self,parent, text, value=0):
			ui.ListBoxEx.Item.__init__(self)
			self.textBox=ui.TextLine()
			self.textBox.SetParent(self)
			self.textBox.SetText(text)
			self.textBox.Show()
			self.value = value

		def GetValue(self):
			return self.value

		def __del__(self):
			ui.ListBoxEx.Item.__del__(self)

	def LoadDialog(self):
		PythonScriptLoader = ui.PythonScriptLoader()
		if app.ENABLE_NEW_EXCHANGE_WINDOW:
			PythonScriptLoader.LoadScriptFile(self, "uiscript/exchangedialog_new.py")
		else:
			PythonScriptLoader.LoadScriptFile(self, "uiscript/exchangedialog.py")
		
		self.OwnerSlot = self.GetChild("Owner_Slot")
		self.OwnerSlot.SetSelectEmptySlotEvent(ui.__mem_func__(self.SelectOwnerEmptySlot))
		self.OwnerSlot.SetSelectItemSlotEvent(ui.__mem_func__(self.SelectOwnerItemSlot))
		self.OwnerSlot.SetOverInItemEvent(ui.__mem_func__(self.OverInOwnerItem))
		self.OwnerSlot.SetOverOutItemEvent(ui.__mem_func__(self.OverOutItem))
		self.OwnerSlot.SetOverInEmptySlotEvent(ui.__mem_func__(self.OverOutItem))
		self.OwnerSlot.SetOverOutEmptySlotEvent(ui.__mem_func__(self.OverOutItem))
		self.OwnerMoney = self.GetChild("Owner_Money_Value")
		if not app.ENABLE_NEW_EXCHANGE_WINDOW:
			self.OwnerAcceptLight = self.GetChild("Owner_Accept_Light")
			self.OwnerAcceptLight.Disable()
		self.OwnerMoneyButton = self.GetChild("Owner_Money")
		self.OwnerMoneyButton.SetEvent(ui.__mem_func__(self.OpenPickMoneyDialog))
		
		self.TargetSlot = self.GetChild("Target_Slot")
		self.TargetSlot.SetOverInItemEvent(ui.__mem_func__(self.OverInTargetItem))
		self.TargetSlot.SetOverOutItemEvent(ui.__mem_func__(self.OverOutItem))
		self.TargetSlot.SetOverInEmptySlotEvent(ui.__mem_func__(self.OverOutItem))
		self.TargetSlot.SetOverOutEmptySlotEvent(ui.__mem_func__(self.OverOutItem))
		self.TargetMoney = self.GetChild("Target_Money_Value")
		if not app.ENABLE_NEW_EXCHANGE_WINDOW:
			self.TargetAcceptLight = self.GetChild("Target_Accept_Light")
			self.TargetAcceptLight.Disable()
		
		dlgPickMoney = uipickmoney.PickMoneyDialog()
		dlgPickMoney.LoadDialog()
		dlgPickMoney.SetAcceptEvent(ui.__mem_func__(self.OnPickMoney))
		dlgPickMoney.SetTitleName(localeinfo.EXCHANGE_MONEY)
		if not app.ENABLE_NEW_EXCHANGE_WINDOW:
			dlgPickMoney.SetMax(6)
		else:
			dlgPickMoney.SetMax(11)
		dlgPickMoney.Hide()
		self.dlgPickMoney = dlgPickMoney
		
		self.AcceptButton = self.GetChild("Owner_Accept_Button")
		self.AcceptButton.SetToggleDownEvent(ui.__mem_func__(self.AcceptExchange))
		
		if app.ENABLE_NEW_EXCHANGE_WINDOW:
			self.TargetAcceptButton = self.GetChild("Target_Accept_Button")
		
		self.TitleName = self.GetChild("TitleName")
		self.GetChild("TitleBar").SetCloseEvent(net.SendExchangeExitPacket)
		if app.ENABLE_NEW_EXCHANGE_WINDOW:
			self.FaceOwnerImage = self.GetChild("FaceOwner_Image")
			self.FaceTargetImage = self.GetChild("FaceTarget_Image")
			self.TargetName = self.GetChild("target_NameText")
			self.TargetLevel = self.GetChild("target_LvText")
			self.ExchangeLogs = self.GetChild("ExchangeLogs")
			self.LogsScrollBar = ui.ThinScrollBar()
			self.LogsScrollBar.SetParent(self.ExchangeLogs)
			self.LogsScrollBar.SetPosition(442 - 75, 17)
			self.LogsScrollBar.SetScrollBarSize(50)
			self.LogsScrollBar.Show()
			self.LogsDropList = ui.ListBoxEx()
			self.LogsDropList.SetParent(self.ExchangeLogs)
			self.LogsDropList.itemHeight = 12
			self.LogsDropList.itemStep = 13
			self.LogsDropList.SetPosition(35, 27)
			self.LogsDropList.SetSize(0, 45)
			self.LogsDropList.SetScrollBar(self.LogsScrollBar)
			self.LogsDropList.SetViewItemCount(2)
			self.LogsDropList.Show()
			self.LogsScrollBar.Show()
			self.listOwnerSlot = []
			self.listTargetSlot = []

	def Destroy(self):
		print "---------------------------------------------------------------------------- DESTROY EXCHANGE"
		self.ClearDictionary()
		self.dlgPickMoney.Destroy()
		self.dlgPickMoney = 0
		self.OwnerSlot = 0
		self.OwnerMoney = 0
		if not app.ENABLE_NEW_EXCHANGE_WINDOW:
			self.OwnerAcceptLight = 0
		self.OwnerMoneyButton = 0
		self.TargetSlot = 0
		self.TargetMoney = 0
		if not app.ENABLE_NEW_EXCHANGE_WINDOW:
			self.TargetAcceptLight = 0
		self.TitleName = 0
		self.AcceptButton = 0
		if app.ENABLE_NEW_EXCHANGE_WINDOW:
			self.TargetAcceptButton = 0
			self.FaceOwnerImage = None
			self.FaceTargetImage = None
			self.TargetName = None
			self.TargetLevel = None
			self.ExchangesLogsWindow = None
			self.LogsDropList.RemoveAllItems()
			self.LogsScrollBar = None
			self.LogsDropList = None
		
		self.tooltipItem = 0
		if app.WJ_ENABLE_TRADABLE_ICON:
			self.interface = 0
			self.wndInventory = 0
			self.wndExtraInventory = 0
			self.lockedItems = {i:(-1,-1) for i in range(exchange.EXCHANGE_ITEM_MAX_NUM)}
			self.lockedExtraItems = {i:(-1,-1) for i in range(exchange.EXCHANGE_ITEM_MAX_NUM)}

	def OpenDialog(self):
		if app.ENABLE_LEVEL_IN_TRADE:
			self.TitleName.SetText(localeinfo.EXCHANGE_TITLE_LEVEL % (exchange.GetNameFromTarget(), exchange.GetLevelFromTarget()))
		else:
			self.TitleName.SetText(localeinfo.EXCHANGE_TITLE % (exchange.GetNameFromTarget()))
		
		self.AcceptButton.Enable()
		self.AcceptButton.SetUp()
		if app.ENABLE_NEW_EXCHANGE_WINDOW:
			self.TargetAcceptButton.Disable()
			self.TargetAcceptButton.SetUp()
			self.FaceOwnerImage.LoadImage(self.FACE_IMAGE_DICT[exchange.GetRaceFromSelf()])
			self.FaceTargetImage.LoadImage(self.FACE_IMAGE_DICT[exchange.GetRaceFromTarget()])
			self.TargetName.SetText(exchange.GetNameFromTarget())
			self.TargetLevel.SetText(localeinfo.NEW_EXCHANGE_LEVEL % (exchange.GetLevelFromTarget()))
			self.LogsDropList.RemoveAllItems()
			self.LogsDropList.AppendItem(self.Item(self, localeinfo.NEW_EXCHANGE_YOU_READY % (str(time.strftime("[%H:%M:%S]"))), 0))
		
		self.Show()
		if app.WJ_ENABLE_TRADABLE_ICON:
			self.interface.SetOnTopWindow(player.ON_TOP_WND_EXCHANGE)
			self.interface.RefreshMarkInventoryBag()
		
		(self.xStart, self.yStart, z) = player.GetMainCharacterPosition()

	def CloseDialog(self):
		wndMgr.OnceIgnoreMouseLeftButtonUpEvent()
		if 0 != self.tooltipItem:
			self.tooltipItem.HideToolTip()
		
		self.dlgPickMoney.Close()
		self.Hide()
		if app.WJ_ENABLE_TRADABLE_ICON:
			for exchangePos, (itemInvenPage, itemSlotPos) in self.lockedItems.items():
				if itemInvenPage == self.wndInventory.GetInventoryPageIndex():
					self.wndInventory.wndItem.SetCanMouseEventSlot(itemSlotPos)
			
			for exchangePos, (itemInvenPage, itemSlotPos) in self.lockedExtraItems.items():
				if itemInvenPage == self.wndExtraInventory.GetInventoryPageIndex():
					self.wndExtraInventory.wndItem.SetCanMouseEventSlot(itemSlotPos)
			
			self.lockedItems = {i:(-1,-1) for i in range(exchange.EXCHANGE_ITEM_MAX_NUM)}
			self.lockedExtraItems = {i:(-1,-1) for i in range(exchange.EXCHANGE_ITEM_MAX_NUM)}
			self.interface.SetOnTopWindow(player.ON_TOP_WND_NONE)
			self.interface.RefreshMarkInventoryBag()

	def SetItemToolTip(self, tooltipItem):
		self.tooltipItem = tooltipItem

	def OpenPickMoneyDialog(self):
		if exchange.GetElkFromSelf() > 0:
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.EXCHANGE_CANT_EDIT_MONEY)
			return
		
		self.dlgPickMoney.Open(player.GetElk(), isExchange = True)

	def OnPickMoney(self, money):
		global NEW_YANG_LIMIT
		if NEW_YANG_LIMIT:
			net.SendExchangeElkAddPacket(long(money))
		else:
			net.SendExchangeElkAddPacket(int(money))

	def AcceptExchange(self):
		if app.ENABLE_NEW_EXCHANGE_WINDOW:
			atLeastOneItem = 0
			atLeastOneYang = 0
			for i in xrange(exchange.EXCHANGE_ITEM_MAX_NUM):
				itemCount = exchange.GetItemCountFromTarget(i)
				if itemCount >= 1:
					atLeastOneYang = 1
					break
			
			if exchange.GetElkFromTarget() >= 1:
				atLeastOneYang = 1
			
			if atLeastOneItem or atLeastOneYang:
				net.SendExchangeAcceptPacket()
				self.AcceptButton.Disable()
			else:
				atLeastOneItem = 0
				atLeastOneYang = 0
				for i in xrange(exchange.EXCHANGE_ITEM_MAX_NUM):
					itemCount = exchange.GetItemCountFromSelf(i)
					if itemCount >= 1:
						atLeastOneYang = 1
						break
				
				if exchange.GetElkFromSelf() >= 1:
					atLeastOneYang = 1
				
				if atLeastOneItem or atLeastOneYang:
					self.questionDialog = uicommon.QuestionDialog2()
					self.questionDialog.SetText1(localeinfo.NEW_EXCHANGE_ALERT1)
					self.questionDialog.SetText2(localeinfo.NEW_EXCHANGE_ALERT2)
					self.questionDialog.SetAcceptEvent(ui.__mem_func__(self.AcceptQuestion))
					self.questionDialog.SetCancelEvent(ui.__mem_func__(self.OnCloseQuestionDialog))
					self.questionDialog.Open()
				else:
					net.SendExchangeAcceptPacket()
					self.AcceptButton.Disable()
		else:
			net.SendExchangeAcceptPacket()
			self.AcceptButton.Disable()

	if app.ENABLE_NEW_EXCHANGE_WINDOW:
		def AcceptQuestion(self):
			net.SendExchangeAcceptPacket()
			self.AcceptButton.Disable()
			if self.questionDialog:
				self.questionDialog.Close()
			
			self.questionDialog = None

		def OnCloseQuestionDialog(self):
			if self.questionDialog:
				self.questionDialog.Close()
			
			self.questionDialog = None
			self.AcceptButton.Enable()
			self.AcceptButton.SetUp()

	def SelectOwnerEmptySlot(self, SlotIndex):
		if False == mousemodule.mouseController.isAttached():
			return
		
		if mousemodule.mouseController.IsAttachedMoney():
			net.SendExchangeElkAddPacket(str(mousemodule.mouseController.GetAttachedMoneyAmount()))
		else:
			attachedSlotType = mousemodule.mouseController.GetAttachedType()
			if (player.SLOT_TYPE_INVENTORY == attachedSlotType or player.SLOT_TYPE_DRAGON_SOUL_INVENTORY == attachedSlotType):
				attachedInvenType = player.SlotTypeToInvenType(attachedSlotType)
				if app.__ENABLE_NEW_OFFLINESHOP__:
					if uiofflineshop.IsBuildingShop() and uiofflineshop.IsSaleSlot(attachedInvenType, mousemodule.mouseController.GetAttachedSlotNumber()):
						chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.OFFLINESHOP_CANT_SELECT_ITEM_DURING_BUILING)
						return
				
				SrcSlotNumber = mousemodule.mouseController.GetAttachedSlotNumber()
				DstSlotNumber = SlotIndex
				itemID = player.GetItemIndex(attachedInvenType, SrcSlotNumber)
				item.SelectItem(itemID)
				if item.IsAntiFlag(item.ANTIFLAG_GIVE):
					chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.EXCHANGE_CANNOT_GIVE)
					mousemodule.mouseController.DeattachObject()
					return

			if app.ENABLE_EXTRA_INVENTORY:
				if attachedSlotType == player.SLOT_TYPE_EXTRA_INVENTORY:
					attachedInvenType = player.SlotTypeToInvenType(attachedSlotType)
					SrcSlotNumber = mousemodule.mouseController.GetAttachedSlotNumber()
					DstSlotNumber = SlotIndex
				
					#offlineshop-updated 09/08/2019
					if app.__ENABLE_NEW_OFFLINESHOP__:
						if uiofflineshop.IsBuildingShop() and uiofflineshop.IsSaleSlot(attachedSlotType, mousemodule.mouseController.GetAttachedSlotNumber()):
							chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.OFFLINESHOP_CANT_SELECT_ITEM_DURING_BUILING)
							return
					
					itemID = player.GetItemIndex(attachedInvenType, SrcSlotNumber)
					item.SelectItem(itemID)

					if item.IsAntiFlag(item.ANTIFLAG_GIVE):
						chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.EXCHANGE_CANNOT_GIVE)
						mousemodule.mouseController.DeattachObject()
						return
					
				net.SendExchangeItemAddPacket(attachedInvenType, SrcSlotNumber, DstSlotNumber)
		
		mousemodule.mouseController.DeattachObject()

	def SelectOwnerItemSlot(self, SlotIndex):
		if player.ITEM_MONEY == mousemodule.mouseController.GetAttachedItemIndex():
			money = mousemodule.mouseController.GetAttachedItemCount()
			net.SendExchangeElkAddPacket(str(money))

	def RefreshOwnerSlot(self):
		for i in xrange(exchange.EXCHANGE_ITEM_MAX_NUM):
			itemIndex = exchange.GetItemVnumFromSelf(i)
			itemCount = exchange.GetItemCountFromSelf(i)
			if 1 == itemCount:
				itemCount = 0
			
			self.OwnerSlot.SetItemSlot(i, itemIndex, itemCount)
		
		self.OwnerSlot.RefreshSlot()

	def RefreshTargetSlot(self):
		for i in xrange(exchange.EXCHANGE_ITEM_MAX_NUM):
			itemIndex = exchange.GetItemVnumFromTarget(i)
			itemCount = exchange.GetItemCountFromTarget(i)
			if 1 == itemCount:
				itemCount = 0
			
			self.TargetSlot.SetItemSlot(i, itemIndex, itemCount)
		
		self.TargetSlot.RefreshSlot()

	def Refresh(self):
		self.RefreshOwnerSlot()
		self.RefreshTargetSlot()
		if app.WJ_ENABLE_TRADABLE_ICON:
			self.RefreshLockedSlot()
			self.RefreshExtraLockedSlot()
		
		self.OwnerMoney.SetText(localeinfo.NumberToMoneyString(exchange.GetElkFromSelf()))
		self.TargetMoney.SetText(localeinfo.NumberToMoneyString(exchange.GetElkFromTarget()))
		if exchange.GetAcceptFromSelf() == True:
			if not app.ENABLE_NEW_EXCHANGE_WINDOW:
				self.OwnerAcceptLight.Down()
			else:
				self.OwnerSlot.SetSlotBaseImage("d:/ymir work/ui/public/slot_base.sub", 0.3500, 0.8500, 0.3500, 1.0)
				self.LogsDropList.AppendItem(self.Item(self, localeinfo.NEW_EXCHANGE_YOU_ACCEPT % (str((time.strftime("[%H:%M:%S]")))), 0))
		else:
			if self.AcceptButton.IsDown() == True:
				self.LogsDropList.AppendItem(self.Item(self, localeinfo.NEW_EXCHANGE_YOU_ABORT % (str((time.strftime("[%H:%M:%S]")))), 0))
			
			self.AcceptButton.Enable()
			self.AcceptButton.SetUp()
			if not app.ENABLE_NEW_EXCHANGE_WINDOW:
				self.OwnerAcceptLight.SetUp()
			else:
				self.OwnerSlot.SetSlotBaseImage("d:/ymir work/ui/public/slot_base.sub", 1.0, 1.0, 1.0, 1.0)
		
		if exchange.GetAcceptFromTarget() == True:
			if not app.ENABLE_NEW_EXCHANGE_WINDOW:
				self.TargetAcceptLight.Down()
			else:
				self.TargetAcceptButton.Down()
				self.TargetSlot.SetSlotBaseImage("d:/ymir work/ui/public/slot_base.sub", 0.3500, 0.8500, 0.3500, 1.0)
				self.LogsDropList.AppendItem(self.Item(self, localeinfo.NEW_EXCHANGE_ACCEPT % (str((time.strftime("[%H:%M:%S]"))), exchange.GetNameFromTarget()), 0))
		else:
			if not app.ENABLE_NEW_EXCHANGE_WINDOW:
				self.TargetAcceptLight.SetUp()
			else:
				if self.TargetAcceptButton.IsDown() == True:
					self.LogsDropList.AppendItem(self.Item(self, localeinfo.NEW_EXCHANGE_ABORT % (str((time.strftime("[%H:%M:%S]"))), exchange.GetNameFromTarget()), 0))
				
				self.TargetAcceptButton.SetUp()
				self.TargetSlot.SetSlotBaseImage("d:/ymir work/ui/public/slot_base.sub", 1.0, 1.0, 1.0, 1.0)

	def OverInOwnerItem(self, slotIndex):
		if 0 != self.tooltipItem:
			self.tooltipItem.SetExchangeOwnerItem(slotIndex)

	def OverInTargetItem(self, slotIndex):
		if 0 != self.tooltipItem:
			self.tooltipItem.SetExchangeTargetItem(slotIndex)

	def OverOutItem(self):
		if 0 != self.tooltipItem:
			self.tooltipItem.HideToolTip()

	def OnTop(self):
		if self.tooltipItem:
			self.tooltipItem.SetTop()
		
		if app.WJ_ENABLE_TRADABLE_ICON:
			if self.interface:
				self.interface.SetOnTopWindow(player.ON_TOP_WND_EXCHANGE)
				self.interface.RefreshMarkInventoryBag()

	def OnUpdate(self):
		USE_EXCHANGE_LIMIT_RANGE = 1000
		(x, y, z) = player.GetMainCharacterPosition()
		if abs(x - self.xStart) > USE_EXCHANGE_LIMIT_RANGE or abs(y - self.yStart) > USE_EXCHANGE_LIMIT_RANGE:
			(self.xStart, self.yStart, z) = player.GetMainCharacterPosition()
			net.SendExchangeExitPacket()

	if app.WJ_ENABLE_TRADABLE_ICON:
		def CantTradableItem(self, destSlotIndex, srcSlotIndex):
			if True == exchange.GetAcceptFromTarget():
				return
			
			itemInvenPage = srcSlotIndex / player.INVENTORY_PAGE_SIZE
			localSlotPos = srcSlotIndex - (itemInvenPage * player.INVENTORY_PAGE_SIZE)
			self.lockedItems[destSlotIndex] = (itemInvenPage, localSlotPos)
			if self.wndInventory.GetInventoryPageIndex() == itemInvenPage and self.IsShow():
				self.wndInventory.wndItem.SetCantMouseEventSlot(localSlotPos)

		def RefreshLockedSlot(self):
			if self.wndInventory:
				for exchangePos, (itemInvenPage, itemSlotPos) in self.lockedItems.items():
					if self.wndInventory.GetInventoryPageIndex() == itemInvenPage:
						self.wndInventory.wndItem.SetCantMouseEventSlot(itemSlotPos)
				
				self.wndInventory.wndItem.RefreshSlot()

		def CantTradableExtraItem(self, destSlotIndex, srcSlotIndex):
			if True == exchange.GetAcceptFromTarget():
				return
			
			itemInvenPage = srcSlotIndex / player.EXTRA_INVENTORY_PAGE_SIZE
			localSlotPos = srcSlotIndex - (itemInvenPage * player.EXTRA_INVENTORY_PAGE_SIZE)
			self.lockedExtraItems[destSlotIndex] = (itemInvenPage, localSlotPos)
			if self.wndExtraInventory.GetInventoryPageIndex() == itemInvenPage and self.IsShow():
				self.wndExtraInventory.wndItem.SetCantMouseEventSlot(localSlotPos)

		def RefreshExtraLockedSlot(self):
			if self.wndExtraInventory:
				for exchangePos, (itemInvenPage, itemSlotPos) in self.lockedExtraItems.items():
					if self.wndExtraInventory.GetInventoryPageIndex() == itemInvenPage:
						self.wndExtraInventory.wndItem.SetCantMouseEventSlot(itemSlotPos)
				
				self.wndExtraInventory.wndItem.RefreshSlot()

		def BindInterface(self, interface):
			self.interface = interface

		def SetInven(self, wndInventory):
			from _weakref import proxy
			self.wndInventory = proxy(wndInventory)

		def SetExtraInven(self, wndExtraInventory):
			from _weakref import proxy
			self.wndExtraInventory = proxy(wndExtraInventory)

