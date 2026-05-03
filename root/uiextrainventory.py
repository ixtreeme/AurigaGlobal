import app
import net
import player
import ui
import mousemodule
import snd
import item
import chat
import uicommon
import uiprivateshopbuilder
import constinfo
import ime
import uipickmoney
import localeinfo
import wiki

IMG_DIR = "new_messenger/"

if app.__ENABLE_NEW_OFFLINESHOP__:
	import offlineshop
	import uiofflineshop

class ExtraInventoryWindow(ui.ScriptWindow):
	if app.WJ_ENABLE_TRADABLE_ICON:
		bindWnds = []
		interface = None

	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.wndDragonSoulRefine = None
		self.searchEdit = None
		self.searchBackground = None
		self.searchButton = None
		self.clearButton = None
		self.suggestionsBox = None
		self.searchHint = None
		self.suggestionButtons = []
		self.searchHighlightedSlots = []
		self.lastSearchText = ""
		self.lastSearchCheckTime = 0.0
		self.isSearching = False

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def Open(self):
		self.Show()
		self.SetTop()
		self.SetCenterPosition()

	if app.WJ_ENABLE_TRADABLE_ICON:
		def BindWindow(self, wnd):
			self.bindWnds.append(wnd)

		def BindInterfaceClass(self, interface):
			self.interface = interface

	def LoadWindow(self, parent):
		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "uiscript/extrainventory.py")
		except:
			import exception
			exception.Abort("ExtraInventoryWindow.LoadWindow.LoadObject")

		try:
			self.GetChild("TitleBar").SetCloseEvent(ui.__mem_func__(self.Close))
			self.GetChild("RefreshButton").SetEvent(ui.__mem_func__(self.SortExtraInventory))
			self.GetChild("Mall_cat").SetEvent(ui.__mem_func__(self.ClickMallButton))
			self.GetChild("Safebox_cat").SetEvent(ui.__mem_func__(self.ClickSafeboxButton))
			
			self.wndItem = self.GetChild("ItemSlot")

			self.inventoryTab = []
			for x in xrange(player.EXTRA_INVENTORY_PAGE_COUNT / (player.EXTRA_INVENTORY_CATEGORY_COUNT)):
				self.inventoryTab.append(self.GetChild("Inventory_Tab_%02d" % (x + 1)))

			self.categoryTab = []
			for x in xrange(player.EXTRA_INVENTORY_CATEGORY_COUNT):
				self.categoryTab.append(self.GetChild("Cat_%02d" % x))
			
			if app.ENABLE_LOCKED_EXTRA_INVENTORY:
				self.EX_INVEN_COVER_IMG_CLOSE = []
				self.EX_INVEN_COVER_IMG_OPEN = []
				for i in xrange(9):
					self.EX_INVEN_COVER_IMG_OPEN.append(self.GetChild("cover_open_" + str(i)))
					self.EX_INVEN_COVER_IMG_CLOSE.append(self.GetChild("cover_close_" + str(i)))
		except:
			import exception
			exception.Abort("ExtraInventoryWindow.LoadWindow.BindObject")
		
		if app.ENABLE_LOCKED_EXTRA_INVENTORY:
			for i in xrange(9):
				self.EX_INVEN_COVER_IMG_CLOSE[i].Hide()
				self.EX_INVEN_COVER_IMG_OPEN[i].Hide()
		
		self.wndItem.SetSelectEmptySlotEvent(ui.__mem_func__(self.SelectEmptySlot))
		self.wndItem.SetSelectItemSlotEvent(ui.__mem_func__(self.SelectItemSlot))
		self.wndItem.SetUnselectItemSlotEvent(ui.__mem_func__(self.UseItemSlot))
		self.wndItem.SetUseSlotEvent(ui.__mem_func__(self.UseItemSlot))
		self.wndItem.SetOverInItemEvent(ui.__mem_func__(self.OverInItem))
		self.wndItem.SetOverOutItemEvent(ui.__mem_func__(self.OverOutItem))
		self.wndItem.SetOverInEmptySlotEvent(ui.__mem_func__(self.OverOutItem))
		self.wndItem.SetOverOutEmptySlotEvent(ui.__mem_func__(self.OverOutItem))
		
		for x in xrange(player.EXTRA_INVENTORY_PAGE_COUNT / (player.EXTRA_INVENTORY_CATEGORY_COUNT)):
			self.inventoryTab[x].SetEvent(lambda arg = x: self.SetInventoryPage(arg))
		self.inventoryTab[0].Down()

		for x in xrange(player.EXTRA_INVENTORY_CATEGORY_COUNT):
			self.categoryTab[x].SetEvent(lambda arg = x: self.SetCategory(arg))
		self.categoryTab[0].Down()

		self.inventoryPageIndex = 0
		self.category = 0
		self.sellingSlotNumber = -1
		self.questionDialog = None
		self.tooltipItem = None

		self.dlgPickMoney = uipickmoney.PickMoneyDialog()
		self.dlgPickMoney.LoadDialog()
		self.dlgPickMoney.Hide()
		if app.ENABLE_HIGHLIGHT_SYSTEM:
			self.listHighlightedSlot = []
			for i in xrange(player.EXTRA_INVENTORY_CATEGORY_COUNT):
				self.listHighlightedSlot.append([])
		
		if app.ENABLE_DRAGON_SOUL_SYSTEM:
			self.wndDragonSoulRefine = parent
		
		self.__InitExtraInventorySearch()
		self.SetInventoryPage(0)

	if app.ENABLE_HIGHLIGHT_SYSTEM:
		def HighlightSlot(self, slot):
			categorySlotCount = player.EXTRA_INVENTORY_PAGE_SIZE * (player.EXTRA_INVENTORY_PAGE_COUNT / player.EXTRA_INVENTORY_CATEGORY_COUNT)
			category = min(slot / categorySlotCount, player.EXTRA_INVENTORY_CATEGORY_COUNT - 1)
			
			if not slot in self.listHighlightedSlot[category]:
				self.listHighlightedSlot[category].append(slot)

	def __InitExtraInventorySearch(self):
		self.searchHighlightedSlots = []
		self.suggestionButtons = []
		self.lastSearchText = ""
		self.lastSearchCheckTime = 0.0
		self.isSearching = False
		
		self.searchBackground = ui.ImageBox()
		self.searchBackground.SetParent(self)
		self.searchBackground.SetPosition(30, 454)
		self.searchBackground.SetSize(100, 18)
		self.searchBackground.LoadImage("d:/ymir work/ui/public/parameter_slot_05.sub")
		self.searchBackground.Show()
		
		self.searchEdit = ui.EditLine()
		self.searchEdit.SetParent(self)
		self.searchEdit.SetPosition(34, 456)
		self.searchEdit.SetSize(100, 18)
		self.searchEdit.SetMax(20)
		self.searchEdit.SetText("")
		self.searchEdit.SetEscapeEvent(ui.__mem_func__(self.OnPressEscapeKey))
		self.searchEdit.SetReturnEvent(ui.__mem_func__(self.__OnExtraInventorySearchReturn))
		self.searchEdit.SetEndPosition()
		self.searchEdit.Show()
		
		self.searchHint = ui.TextLine()
		self.searchHint.SetParent(self)
		self.searchHint.SetPosition(34, 456)
		self.searchHint.SetPackedFontColor(0xff00ff00)
		self.searchHint.SetText("")
		self.searchHint.Hide()
		
		self.searchButton = ui.Button()
		self.searchButton.SetParent(self)
		self.searchButton.SetPosition(7, 453)
		self.searchButton.SetUpVisual(IMG_DIR + "btn_1.png")
		self.searchButton.SetOverVisual(IMG_DIR + "btn_2.png")
		self.searchButton.SetDownVisual(IMG_DIR + "btn_3.png")
		self.searchButton.SetEvent(ui.__mem_func__(self.__OnExtraInventorySearchReturn))
		self.searchButton.Show()
		
		self.clearButton = ui.Button()
		self.clearButton.SetParent(self)
		self.clearButton.SetPosition(147, 457)
		self.clearButton.SetUpVisual(IMG_DIR + "clear_btn_0.png")
		self.clearButton.SetOverVisual(IMG_DIR + "clear_btn_1.png")
		self.clearButton.SetDownVisual(IMG_DIR + "clear_btn_2.png")
		self.clearButton.SetEvent(ui.__mem_func__(self.__OnExtraInventorySearchEscape))
		self.clearButton.Hide()
		
		self.suggestionsBox = ui.ThinBoard()
		self.suggestionsBox.SetSize(138, 118)
		self.suggestionsBox.Hide()

	def __RefreshExtraInventorySearchDropdownPosition(self):
		if not self.suggestionsBox:
			return
		
		try:
			(x, y) = self.GetGlobalPosition()
			self.suggestionsBox.SetPosition(x + 30, y + 468)
		except:
			pass

	def __GetExtraInventorySearchItemName(self, slot):
		itemVnum = player.GetItemIndex(player.EXTRA_INVENTORY, slot)
		if itemVnum == 0:
			return ""
		
		item.SelectItem(itemVnum)
		return item.GetItemName()

	def __ClearExtraInventorySearchHighlights(self):
		if app.ENABLE_HIGHLIGHT_SYSTEM:
			categorySlotCount = player.EXTRA_INVENTORY_PAGE_SIZE * (player.EXTRA_INVENTORY_PAGE_COUNT / player.EXTRA_INVENTORY_CATEGORY_COUNT)
			for slot in self.searchHighlightedSlots:
				category = min(slot / categorySlotCount, player.EXTRA_INVENTORY_CATEGORY_COUNT - 1)
				if slot in self.listHighlightedSlot[category]:
					self.listHighlightedSlot[category].remove(slot)
		
		self.searchHighlightedSlots = []
		self.RefreshItemSlot()

	def __OnExtraInventorySearchEscape(self):
		if self.searchEdit:
			self.searchEdit.SetText("")
		
		self.__ClearExtraInventorySearchHighlights()
		self.__HideExtraInventorySearchSuggestions()
		if self.searchHint:
			self.searchHint.Hide()
		if self.clearButton:
			self.clearButton.Hide()
		self.isSearching = False
		self.lastSearchText = ""

	def __OnExtraInventorySearchReturn(self):
		if not self.searchEdit:
			return
		
		searchText = self.searchEdit.GetText().lower().strip()
		if not searchText:
			return
		
		self.__HighlightExtraInventorySearchItems(searchText)
		self.__HideExtraInventorySearchSuggestions()
		if self.searchHint:
			self.searchHint.Hide()
		if self.clearButton:
			self.clearButton.Show()
		self.isSearching = False
		self.__JumpToExtraInventorySearchMatch(searchText)

	def __OnExtraInventorySearchSuggestionClick(self, globalSlot, searchText):
		itemName = self.__GetExtraInventorySearchItemName(globalSlot)
		if itemName:
			self.__HighlightExtraInventorySearchItems(itemName.lower())
		
		self.__HideExtraInventorySearchSuggestions()
		if self.clearButton:
			self.clearButton.Show()
		self.isSearching = False
		self.__JumpToExtraInventorySearchMatch(searchText, globalSlot)

	def __HighlightExtraInventorySearchItems(self, searchText):
		self.__ClearExtraInventorySearchHighlights()
		if not searchText:
			return
		
		searchText = searchText.lower()
		totalSlots = player.EXTRA_INVENTORY_PAGE_SIZE * player.EXTRA_INVENTORY_PAGE_COUNT
		
		for globalSlot in xrange(totalSlots):
			itemName = self.__GetExtraInventorySearchItemName(globalSlot)
			if itemName and searchText in itemName.lower():
				if app.ENABLE_HIGHLIGHT_SYSTEM:
					self.HighlightSlot(globalSlot)
				if globalSlot not in self.searchHighlightedSlots:
					self.searchHighlightedSlots.append(globalSlot)
		
		self.RefreshItemSlot()

	def __HideExtraInventorySearchSuggestions(self):
		for button in self.suggestionButtons:
			try:
				button.Hide()
			except:
				pass
		
		self.suggestionButtons = []
		if self.suggestionsBox:
			self.suggestionsBox.Hide()

	def __UpdateExtraInventorySearchSuggestions(self, searchText):
		self.__HideExtraInventorySearchSuggestions()
		self.__UpdateExtraInventorySearchHint(searchText)
		if not searchText:
			if self.clearButton:
				self.clearButton.Hide()
			return
		
		if self.clearButton:
			self.clearButton.Show()

	def __UpdateExtraInventorySearchHint(self, searchText):
		if not self.searchHint:
			return
		
		searchText = searchText.strip()
		if len(searchText) < 2:
			self.searchHint.Hide()
			return
		
		searchTextLower = searchText.lower()
		totalSlots = player.EXTRA_INVENTORY_PAGE_SIZE * player.EXTRA_INVENTORY_PAGE_COUNT
		
		for globalSlot in xrange(totalSlots):
			itemName = self.__GetExtraInventorySearchItemName(globalSlot)
			if itemName and searchTextLower in itemName.lower():
				itemNameLower = itemName.lower()
				if itemNameLower.startswith(searchTextLower):
					self.searchHint.SetPosition(34 + min(len(searchText) * 6, 82), 456)
					self.searchHint.SetText(itemName[len(searchText):])
				else:
					self.searchHint.SetPosition(34, 456)
					self.searchHint.SetText(itemName)
				self.searchHint.Show()
				return
		
		self.searchHint.Hide()

	def __CheckExtraInventorySearchInputChange(self):
		if not self.searchEdit:
			return
		
		currentText = self.searchEdit.GetText()
		if currentText != self.lastSearchText:
			self.lastSearchText = currentText
			self.isSearching = True if currentText else False
			self.__UpdateExtraInventorySearchSuggestions(currentText)

	def __JumpToExtraInventorySearchMatch(self, searchText, preferredSlot=None):
		targetSlot = preferredSlot
		if targetSlot is None:
			searchText = searchText.lower()
			for slot in self.searchHighlightedSlots:
				itemName = self.__GetExtraInventorySearchItemName(slot)
				if itemName and searchText in itemName.lower():
					targetSlot = slot
					break
		
		if targetSlot is None:
			return
		
		categoryPageCount = player.EXTRA_INVENTORY_PAGE_COUNT / player.EXTRA_INVENTORY_CATEGORY_COUNT
		categorySlotCount = player.EXTRA_INVENTORY_PAGE_SIZE * categoryPageCount
		category = min(targetSlot / categorySlotCount, player.EXTRA_INVENTORY_CATEGORY_COUNT - 1)
		page = (targetSlot - (category * categorySlotCount)) / player.EXTRA_INVENTORY_PAGE_SIZE
		localSlot = targetSlot % player.EXTRA_INVENTORY_PAGE_SIZE
		
		self.SetCategory(category)
		self.SetInventoryPage(page)
		try:
			self.categoryTab[category].Down()
			self.inventoryTab[page].Down()
			self.wndItem.ActivateSlot(localSlot, 1.0, 0.2, 0.2, 1.0)
			self.wndItem.RefreshSlot()
		except:
			pass
		chat.AppendChat(chat.CHAT_TYPE_INFO, "Talalat: Extra %d. kategoria, %d. oldal, %d. slot" % (category + 1, page + 1, localSlot + 1))

	def OnUpdate(self):
		if not self.searchEdit:
			return
		
		currentTime = app.GetTime()
		if currentTime - self.lastSearchCheckTime >= 0.2:
			self.lastSearchCheckTime = currentTime
			self.__CheckExtraInventorySearchInputChange()
		
		if self.searchHint and self.searchEdit and not self.searchEdit.GetText():
			self.searchHint.Hide()

	def Destroy(self):
		self.ClearDictionary()
		self.tooltipItem = None
		self.wndItem = None
		self.inventoryTab = []
		self.categoryTab = []
		self.searchEdit = None
		self.searchBackground = None
		self.searchButton = None
		self.clearButton = None
		self.suggestionsBox = None
		self.searchHint = None
		self.suggestionButtons = []
		self.searchHighlightedSlots = []
		self.dlgPickMoney.Destroy()
		self.dlgPickMoney = None
		if app.ENABLE_LOCKED_EXTRA_INVENTORY:
			self.EX_INVEN_COVER_IMG_CLOSE = None
			self.EX_INVEN_COVER_IMG_OPEN = None
		self.wndDragonSoulRefine = None
		if app.WJ_ENABLE_TRADABLE_ICON:
			self.bindWnds = []

	def Close(self):
		suggestionsBox = getattr(self, "suggestionsBox", None)
		if suggestionsBox:
			suggestionsBox.Hide()
		
		if constinfo.GET_ITEM_QUESTION_DIALOG_STATUS():
			self.OnCloseQuestionDialog()
			return

		if self.tooltipItem:
			self.tooltipItem.HideToolTip()

		if self.dlgPickMoney:
			self.dlgPickMoney.Close()

		self.Hide()

	if app.ENABLE_LOCKED_EXTRA_INVENTORY:
		def UpdateInven(self):
			for i in xrange(9):
				self.EX_INVEN_COVER_IMG_OPEN[i].Hide()
				self.EX_INVEN_COVER_IMG_CLOSE[i].Hide()

		def Expansion_env(self):
			net.SendChatPacket("/unlock_extra " + str(self.category))
			self.OnCloseQuestionDialog()

		def Env_key(self):
			slotsAv = player.GetStatus(int(self.category) + player.EXTRA_INVENTORY1)
			freeSlots = (player.EXTRA_INVENTORY_PAGE_SIZE * 2) + 20
			maxStage = (player.EXTRA_INVENTORY_PAGE_SIZE * (player.EXTRA_INVENTORY_PAGE_COUNT / player.EXTRA_INVENTORY_CATEGORY_COUNT) - freeSlots) / 5
			if slotsAv < maxStage:
				needkeys = [1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 6]
				needKey = needkeys[slotsAv] if slotsAv < len(needkeys) else 6 + ((slotsAv - len(needkeys)) / 6)
				
				self.questionDialog = uicommon.QuestionDialog()
				self.questionDialog.SetText(localeinfo.ENVANTER_EXPANS_1 % needKey)
				self.questionDialog.SetAcceptEvent(ui.__mem_func__(self.Expansion_env))
				self.questionDialog.SetCancelEvent(ui.__mem_func__(self.OnCloseQuestionDialog))
				self.questionDialog.Open()

	def GetInventoryPageIndex(self):
		return self.inventoryPageIndex + (self.category * (player.EXTRA_INVENTORY_PAGE_COUNT / player.EXTRA_INVENTORY_CATEGORY_COUNT))

	def SetInventoryPage(self, page):
		self.inventoryPageIndex = page

		for x in xrange(player.EXTRA_INVENTORY_PAGE_COUNT / (player.EXTRA_INVENTORY_CATEGORY_COUNT)):
			if x != page:
				self.inventoryTab[x].SetUp()
		
		if app.ENABLE_LOCKED_EXTRA_INVENTORY:
			self.UpdateInven()
		
		self.RefreshItemSlot()

	def SetCategory(self, category):
		self.category = category

		for x in xrange(player.EXTRA_INVENTORY_CATEGORY_COUNT):
			if x != category:
				self.categoryTab[x].SetUp()
		
		if app.ENABLE_LOCKED_EXTRA_INVENTORY:
			self.UpdateInven()
		
		self.RefreshItemSlot()

	def OnPickItem(self, count):
		itemSlotIndex = self.dlgPickMoney.itemGlobalSlotIndex
		if app.__ENABLE_NEW_OFFLINESHOP__:
			if uiofflineshop.IsBuildingShop() and uiofflineshop.IsSaleSlot(player.EXTRA_INVENTORY, itemSlotIndex):
				chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.OFFLINESHOP_CANT_SELECT_ITEM_DURING_BUILING)
				return
		
		division = self.dlgPickMoney.division
		if division:
			net.SendItemDivisionPacket(itemSlotIndex, player.EXTRA_INVENTORY)
		else:
			packageCount = self.dlgPickMoney.stackPackageCount
			if packageCount > 0:
				self.__SplitItemToPackages(itemSlotIndex, packageCount, count)
			else:
				selectedItemVNum = player.GetItemIndex(player.EXTRA_INVENTORY, itemSlotIndex)
				mousemodule.mouseController.AttachObject(self, player.SLOT_TYPE_EXTRA_INVENTORY, itemSlotIndex, selectedItemVNum, count)

	def __InventoryLocalSlotPosToGlobalSlotPos(self, local):
		categoryPageCount = player.EXTRA_INVENTORY_PAGE_COUNT / player.EXTRA_INVENTORY_CATEGORY_COUNT
		return self.inventoryPageIndex * player.EXTRA_INVENTORY_PAGE_SIZE + local + (self.category * (player.EXTRA_INVENTORY_PAGE_SIZE * categoryPageCount)) 


	def __SplitItemToPackages(self, srcSlotPos, packageCount, packageSize):
		if packageCount <= 0 or packageSize <= 0:
			return

		maxPackageCount = player.GetItemCount(player.EXTRA_INVENTORY, srcSlotPos) / packageSize
		if maxPackageCount <= 0:
			return

		packageCount = min(packageCount, maxPackageCount)

		# Keep package split inside the current extra-inventory category only.
		categoryPageCount = player.EXTRA_INVENTORY_PAGE_COUNT / player.EXTRA_INVENTORY_CATEGORY_COUNT
		categoryStart = self.category * (player.EXTRA_INVENTORY_PAGE_SIZE * categoryPageCount)
		categoryEnd = categoryStart + (player.EXTRA_INVENTORY_PAGE_SIZE * categoryPageCount)

		emptySlots = []
		for slotPos in xrange(categoryStart, categoryEnd):
			if slotPos == srcSlotPos:
				continue
			if player.GetItemIndex(player.EXTRA_INVENTORY, slotPos) == 0:
				emptySlots.append(slotPos)
				if len(emptySlots) >= packageCount:
					break

		for dstSlotPos in emptySlots:
			self.__SendMoveItemPacket(srcSlotPos, dstSlotPos, packageSize)



	if app.WJ_ENABLE_TRADABLE_ICON:
		def RefreshMarkSlots(self, localIndex=None):
			if not self.interface:
				return
			
			onTopWnd = self.interface.GetOnTopWindow()
			if localIndex:
				slotNumber = self.__InventoryLocalSlotPosToGlobalSlotPos(localIndex)
				if onTopWnd == player.ON_TOP_WND_NONE:
					self.wndItem.SetUsableSlotOnTopWnd(localIndex)
				elif onTopWnd == player.ON_TOP_WND_SHOP:
					if player.IsAntiFlagBySlot(player.EXTRA_INVENTORY, slotNumber, item.ANTIFLAG_SELL):
						self.wndItem.SetUnusableSlotOnTopWnd(localIndex)
					else:
						self.wndItem.SetUsableSlotOnTopWnd(localIndex)
				elif onTopWnd == player.ON_TOP_WND_EXCHANGE:
					if player.IsAntiFlagBySlot(player.EXTRA_INVENTORY, slotNumber, item.ANTIFLAG_GIVE):
						self.wndItem.SetUnusableSlotOnTopWnd(localIndex)
					else:
						self.wndItem.SetUsableSlotOnTopWnd(localIndex)
				elif onTopWnd == player.ON_TOP_WND_PRIVATE_SHOP:
					if player.IsAntiFlagBySlot(player.EXTRA_INVENTORY, slotNumber, item.ITEM_ANTIFLAG_MYSHOP):
						self.wndItem.SetUnusableSlotOnTopWnd(localIndex)
					else:
						self.wndItem.SetUsableSlotOnTopWnd(localIndex)
				elif onTopWnd == player.ON_TOP_WND_SAFEBOX:
					if player.IsAntiFlagBySlot(player.EXTRA_INVENTORY, slotNumber, item.ANTIFLAG_SAFEBOX):
						self.wndItem.SetUnusableSlotOnTopWnd(localIndex)
					else:
						self.wndItem.SetUsableSlotOnTopWnd(localIndex)
				
				return

			for i in xrange(player.EXTRA_INVENTORY_PAGE_SIZE):
				slotNumber = self.__InventoryLocalSlotPosToGlobalSlotPos(i)
				if onTopWnd == player.ON_TOP_WND_NONE:
					self.wndItem.SetUsableSlotOnTopWnd(i)
				elif onTopWnd == player.ON_TOP_WND_SHOP:
					if player.IsAntiFlagBySlot(player.EXTRA_INVENTORY, slotNumber, item.ANTIFLAG_SELL):
						self.wndItem.SetUnusableSlotOnTopWnd(i)
					else:
						self.wndItem.SetUsableSlotOnTopWnd(i)
				elif onTopWnd == player.ON_TOP_WND_EXCHANGE:
					if player.IsAntiFlagBySlot(player.EXTRA_INVENTORY, slotNumber, item.ANTIFLAG_GIVE):
						self.wndItem.SetUnusableSlotOnTopWnd(i)
					else:
						self.wndItem.SetUsableSlotOnTopWnd(i)
				elif onTopWnd == player.ON_TOP_WND_PRIVATE_SHOP:
					if player.IsAntiFlagBySlot(player.EXTRA_INVENTORY, slotNumber, item.ITEM_ANTIFLAG_MYSHOP):
						self.wndItem.SetUnusableSlotOnTopWnd(i)
					else:
						self.wndItem.SetUsableSlotOnTopWnd(i)
				elif onTopWnd == player.ON_TOP_WND_SAFEBOX:
					if player.IsAntiFlagBySlot(player.EXTRA_INVENTORY, slotNumber, item.ANTIFLAG_SAFEBOX):
						self.wndItem.SetUnusableSlotOnTopWnd(i)
					else:
						self.wndItem.SetUsableSlotOnTopWnd(i)

	def RefreshItemSlot(self):
		getItemVNum = player.GetItemIndex
		getItemCount = player.GetItemCount
		
		for i in xrange(self.wndItem.GetSlotCount()):
			self.wndItem.DeactivateSlot(i)
		
		for i in xrange(player.EXTRA_INVENTORY_PAGE_SIZE):
			slotNumber = self.__InventoryLocalSlotPosToGlobalSlotPos(i)
			itemCount = getItemCount(player.EXTRA_INVENTORY, slotNumber)
			
			if 0 == itemCount:
				self.wndItem.ClearSlot(i)
				continue
			elif 1 == itemCount:
				itemCount = 0
			
			itemVnum = getItemVNum(player.EXTRA_INVENTORY, slotNumber)
			if not itemVnum:
				continue
			
			self.wndItem.SetItemSlot(i, itemVnum, itemCount)
			
			wasActivated = False
			
			if app.ENABLE_HIGHLIGHT_SYSTEM and slotNumber in self.listHighlightedSlot[self.category]:
				self.wndItem.ActivateSlot(i, 0.2, 0.8, 0.0, 1.0)
				wasActivated = True
			
			if not wasActivated and constinfo.IS_AUTO_POTION(itemVnum):
				metinSocket = [player.GetItemMetinSocket(player.EXTRA_INVENTORY, slotNumber, j) for j in xrange(player.METIN_SOCKET_MAX_NUM)]
				isActivated = 0 != metinSocket[0]
				if isActivated:
					if app.ENABLE_HIGHLIGHT_SYSTEM:
						self.wndItem.ActivateSlot(i)
					
					wasActivated = True
					
					potionType = 0
					if constinfo.IS_AUTO_POTION_HP(itemVnum):
						potionType = player.AUTO_POTION_TYPE_HP
					elif constinfo.IS_AUTO_POTION_SP(itemVnum):
						potionType = player.AUTO_POTION_TYPE_SP
					
					usedAmount = int(metinSocket[1])
					totalAmount = int(metinSocket[2])
					player.SetAutoPotionInfo(potionType, isActivated, (totalAmount - usedAmount), totalAmount, slotNumber)
			
			if not wasActivated and app.ENABLE_HIGHLIGHT_SYSTEM and app.ENABLE_NEW_USE_POTION and player.GetItemTypeBySlot(player.EXTRA_INVENTORY, slotNumber) == item.ITEM_TYPE_USE and player.GetItemSubTypeBySlot(player.EXTRA_INVENTORY, slotNumber) == item.USE_NEW_POTIION:
				metinSocket = [player.GetItemMetinSocket(player.EXTRA_INVENTORY, slotNumber, j) for j in xrange(player.METIN_SOCKET_MAX_NUM)]
				isActivated = 0 != metinSocket[1]
				if isActivated:
					self.wndItem.ActivateSlot(i)
					wasActivated = True
			
			if app.WJ_ENABLE_TRADABLE_ICON:
				self.RefreshMarkSlots(i)
		
		self.wndItem.RefreshSlot()
		if app.WJ_ENABLE_TRADABLE_ICON:
			map(lambda wnd:wnd.RefreshExtraLockedSlot(), self.bindWnds)

	def SetItemToolTip(self, tooltipItem):
		self.tooltipItem = tooltipItem

	def SellItem(self):
		
		# offlineshop-updated 04/08/19
		if app.__ENABLE_NEW_OFFLINESHOP__:
			if uiofflineshop.IsBuildingShop() and uiofflineshop.IsSaleSlot(player.EXTRA_INVENTORY, self.sellingSlotNumber):
				chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.OFFLINESHOP_CANT_SELECT_ITEM_DURING_BUILING)
				return
		
		if self.sellingSlotitemIndex == player.GetItemIndex(player.EXTRA_INVENTORY, self.sellingSlotNumber):
			if self.sellingSlotitemCount == player.GetItemCount(player.EXTRA_INVENTORY, self.sellingSlotNumber):
				net.SendShopSellPacketNew(self.sellingSlotNumber, self.questionDialog.count, player.EXTRA_INVENTORY)
				snd.PlaySound("sound/ui/money.wav")
		self.OnCloseQuestionDialog()

	def OnCloseQuestionDialog(self):
		if not self.questionDialog:
			return

		self.questionDialog.Close()
		self.questionDialog = None
		constinfo.SET_ITEM_QUESTION_DIALOG_STATUS(0)

	def SelectEmptySlot(self, selectedSlotPos):
		if constinfo.GET_ITEM_QUESTION_DIALOG_STATUS() == 1:
			return

		selectedSlotPos = self.__InventoryLocalSlotPosToGlobalSlotPos(selectedSlotPos)

		if mousemodule.mouseController.isAttached():
			attachedSlotType = mousemodule.mouseController.GetAttachedType()
			attachedSlotPos = mousemodule.mouseController.GetAttachedSlotNumber()
			attachedCount = mousemodule.mouseController.GetAttachedItemCount()
			
			# offlineshop-updated 04/08/19
			if app.__ENABLE_NEW_OFFLINESHOP__:
				if uiofflineshop.IsBuildingShop() and uiofflineshop.IsSaleSlot(attachedSlotType,attachedSlotPos):
					chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.OFFLINESHOP_CANT_SELECT_ITEM_DURING_BUILING)
					return
			
			if player.SLOT_TYPE_EXTRA_INVENTORY == attachedSlotType:
				self.__SendMoveItemPacket(attachedSlotPos, selectedSlotPos, attachedCount)
			elif player.SLOT_TYPE_SAFEBOX == attachedSlotType:
				net.SendSafeboxCheckoutPacket(attachedSlotPos, player.EXTRA_INVENTORY, selectedSlotPos)
			elif player.SLOT_TYPE_MALL == attachedSlotType:
				net.SendMallCheckoutPacket(attachedSlotPos, player.EXTRA_INVENTORY, selectedSlotPos)
			elif player.SLOT_TYPE_INVENTORY == attachedSlotType:#razor93
				net.SendItemMovePacket(player.INVENTORY, attachedSlotPos, player.EXTRA_INVENTORY, selectedSlotPos, attachedCount)

			mousemodule.mouseController.DeattachObject()

	def SelectItemSlot(self, itemSlotIndex):
		if constinfo.GET_ITEM_QUESTION_DIALOG_STATUS() == 1:
			return

		itemSlotIndex = self.__InventoryLocalSlotPosToGlobalSlotPos(itemSlotIndex)

		selectedItemVNum = player.GetItemIndex(player.EXTRA_INVENTORY, itemSlotIndex)
		itemCount = player.GetItemCount(player.EXTRA_INVENTORY, itemSlotIndex)

		if mousemodule.mouseController.isAttached():
			attachedSlotType = mousemodule.mouseController.GetAttachedType()
			attachedSlotPos = mousemodule.mouseController.GetAttachedSlotNumber()
			attachedItemVID = mousemodule.mouseController.GetAttachedItemIndex()

			# Normál -> Extra
			if attachedSlotType == player.SLOT_TYPE_INVENTORY:
				attachedCount = mousemodule.mouseController.GetAttachedItemCount()
				net.SendItemMovePacket(player.INVENTORY, attachedSlotPos, player.EXTRA_INVENTORY, itemSlotIndex, attachedCount)
				mousemodule.mouseController.DeattachObject()
				return

			# Extra -> Extra
			if attachedSlotType == player.SLOT_TYPE_EXTRA_INVENTORY:
				attachedCount = mousemodule.mouseController.GetAttachedItemCount()
				self.__SendMoveItemPacket(attachedSlotPos, itemSlotIndex, attachedCount)
				mousemodule.mouseController.DeattachObject()
				return

		else:
			curCursorNum = app.GetCursor()
			if app.SELL == curCursorNum:
				self.__SellItem(itemSlotIndex)
			elif app.BUY == curCursorNum:
				chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.SHOP_BUY_INFO)
			elif app.IsPressed(app.DIK_LALT) or (hasattr(app, 'DIK_RALT') and app.IsPressed(app.DIK_RALT)):
				# Wiki chest loot: ha ALT-ra van allitva, akkor Alt+bal klikk ladan -> Wiki lada tartalom (scroll nelkul)
				try:
					import ingamewikiconfig
					mod = getattr(ingamewikiconfig, 'WIKI_CHEST_LOOT_MODIFIER', 'CTRL')
					try:
						mod = mod.upper()
					except:
						mod = 'CTRL'
					if mod == 'ALT':
						itemIndex = player.GetItemIndex(player.EXTRA_INVENTORY, itemSlotIndex)
						bossList = getattr(ingamewikiconfig, 'BOSS_CHEST_VNUMS', ())
						dungeonList = getattr(ingamewikiconfig, 'DUNGEON_CHEST_VNUMS', ())
						altList = getattr(ingamewikiconfig, 'ALT_CHEST_VNUMS', ())
						if itemIndex and (itemIndex in bossList or itemIndex in dungeonList or itemIndex in altList):
							wikiBase = wiki.GetBaseClass()
							if wikiBase and hasattr(wikiBase, 'OpenChestLootPopup'):
								wikiBase.OpenChestLootPopup(itemIndex)
								return
							try:
								net.ToggleWikiWindow()
							except:
								pass
							return
				except:
					pass

				link = player.GetItemLink(player.EXTRA_INVENTORY, itemSlotIndex)
				ime.PasteString(link)
			elif app.IsPressed(app.DIK_LSHIFT):
				if itemCount > 1:
					self.dlgPickMoney.SetTitleName(localeinfo.PICK_ITEM_TITLE)
					self.dlgPickMoney.SetAcceptEvent(ui.__mem_func__(self.OnPickItem))
					self.dlgPickMoney.Open(itemCount, 1, False, False, True)
					self.dlgPickMoney.itemGlobalSlotIndex = itemSlotIndex
				else:
					mousemodule.mouseController.AttachObject(self, player.SLOT_TYPE_EXTRA_INVENTORY, itemSlotIndex, selectedItemVNum, itemCount)
			elif app.IsPressed(app.DIK_LCONTROL) or app.IsPressed(app.DIK_RCONTROL):
				# Wiki chest loot: ha CTRL-ra van allitva, akkor Ctrl+bal klikk ladan -> Wiki lada tartalom (scroll nelkul)
				try:
					import ingamewikiconfig
					mod = getattr(ingamewikiconfig, 'WIKI_CHEST_LOOT_MODIFIER', 'CTRL')
					try:
						mod = mod.upper()
					except:
						mod = 'CTRL'
					if mod == 'CTRL':
						itemIndex = player.GetItemIndex(player.EXTRA_INVENTORY, itemSlotIndex)
						bossList = getattr(ingamewikiconfig, 'BOSS_CHEST_VNUMS', ())
						dungeonList = getattr(ingamewikiconfig, 'DUNGEON_CHEST_VNUMS', ())
						altList = getattr(ingamewikiconfig, 'ALT_CHEST_VNUMS', ())
						if itemIndex and (itemIndex in bossList or itemIndex in dungeonList or itemIndex in altList):
							wikiBase = wiki.GetBaseClass()
							if wikiBase and hasattr(wikiBase, 'OpenChestLootPopup'):
								wikiBase.OpenChestLootPopup(itemIndex)
								return
							try:
								net.ToggleWikiWindow()
							except:
								pass
							return
				except:
					pass
				# Nem lada, vagy nincs CTRL-ra allitva -> viselkedjen sima kattintaskent
				mousemodule.mouseController.AttachObject(self, player.SLOT_TYPE_EXTRA_INVENTORY, itemSlotIndex, selectedItemVNum, itemCount)
				snd.PlaySound("sound/ui/pick.wav")
			else:
				mousemodule.mouseController.AttachObject(self, player.SLOT_TYPE_EXTRA_INVENTORY, itemSlotIndex, selectedItemVNum, itemCount)
				snd.PlaySound("sound/ui/pick.wav")




	def __DropSrcItemToDestItemInInventory(self, srcItemVID, srcItemSlotPos, dstItemSlotPos):
		if app.__ENABLE_NEW_OFFLINESHOP__:
			if uiofflineshop.IsBuildingShop() and (uiofflineshop.IsSaleSlot(player.EXTRA_INVENTORY, srcItemSlotPos) or uiofflineshop.IsSaleSlot(player.EXTRA_INVENTORY , dstItemSlotPos)):
				chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.OFFLINESHOP_CANT_SELECT_ITEM_DURING_BUILING)
				return
		
		
		if item.IsKey(srcItemVID):
			self.__SendUseItemToItemPacket(srcItemSlotPos, dstItemSlotPos)
		else:
			self.__SendMoveItemPacket(srcItemSlotPos, dstItemSlotPos, 0)

	def __SellItem(self, itemSlotPos):
		if app.__ENABLE_NEW_OFFLINESHOP__:
			if uiofflineshop.IsBuildingShop():
				chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.OFFLINESHOP_CANT_SELECT_ITEM_DURING_BUILING)
				return
		
		self.sellingSlotNumber = itemSlotPos
		
		itemIndex = player.GetItemIndex(player.EXTRA_INVENTORY, itemSlotPos)
		itemCount = player.GetItemCount(player.EXTRA_INVENTORY, itemSlotPos)

		self.sellingSlotitemIndex = itemIndex
		self.sellingSlotitemCount = itemCount

		item.SelectItem(itemIndex)

		if item.IsAntiFlag(item.ANTIFLAG_SELL):
			popup = uicommon.PopupDialog()
			popup.SetText(localeinfo.SHOP_CANNOT_SELL_ITEM)
			popup.SetAcceptEvent(self.__OnClosePopupDialog)
			popup.Open()
			self.popup = popup
			return

		itemPrice = item.GetISellItemPrice()

		if item.Is1GoldItem():
			itemPrice = itemCount / itemPrice# / 5
		else:
			itemPrice = itemPrice * itemCount# / 5

		item.GetItemName(itemIndex)
		itemName = item.GetItemName()

		self.questionDialog = uicommon.QuestionDialog()
		self.questionDialog.SetText(localeinfo.DO_YOU_SELL_ITEM(itemName, itemCount, itemPrice))
		self.questionDialog.SetAcceptEvent(ui.__mem_func__(self.SellItem))
		self.questionDialog.SetCancelEvent(ui.__mem_func__(self.OnCloseQuestionDialog))
		self.questionDialog.Open()
		self.questionDialog.count = itemCount

		constinfo.SET_ITEM_QUESTION_DIALOG_STATUS(1)

	def __OnClosePopupDialog(self):
		self.pop = None

	def OverOutItem(self):
		self.wndItem.SetUseMode(False)
		self.wndItem.SetUsableItem(False)
		if None != self.tooltipItem:
			self.tooltipItem.HideToolTip()

	def OverInItem(self, overSlotPos):
		if app.ENABLE_HIGHLIGHT_SYSTEM:
			slotNumber = self.__InventoryLocalSlotPosToGlobalSlotPos(overSlotPos)
			if slotNumber in self.listHighlightedSlot[self.category]:
				self.wndItem.DeactivateSlot(overSlotPos)
				self.listHighlightedSlot[self.category].remove(slotNumber)
		
		overSlotPos = self.__InventoryLocalSlotPosToGlobalSlotPos(overSlotPos)
		self.wndItem.SetUsableItem(True)
		
		if mousemodule.mouseController.isAttached():
			attachedItemType = mousemodule.mouseController.GetAttachedType()
			if attachedItemType == player.SLOT_TYPE_EXTRA_INVENTORY:
				attachedItemVNum = mousemodule.mouseController.GetAttachedItemIndex()
				if self.__CanUseSrcItemToDstItem(attachedItemVNum, overSlotPos):
					self.wndItem.SetUseMode(True)
				else:
					self.wndItem.SetUseMode(False)
		
		self.ShowToolTip(overSlotPos)

	def __CanUseSrcItemToDstItem(self, srcItemVNum, dstSlotPos):
		if item.IsKey(srcItemVNum):
			if player.CanUnlock(srcItemVNum, player.EXTRA_INVENTORY, dstSlotPos):
				return True

	def ShowToolTip(self, slotIndex):
		if None != self.tooltipItem:
			self.tooltipItem.SetInventoryItem(slotIndex, player.EXTRA_INVENTORY)
			if app.__ENABLE_NEW_OFFLINESHOP__:
				if uiofflineshop.IsBuildingShop() or uiofflineshop.IsBuildingAuction():
					self.__AddTooltipSaleMode(slotIndex)
	
	if app.__ENABLE_NEW_OFFLINESHOP__:
		def __AddTooltipSaleMode(self, slot):
			itemIndex = player.GetItemIndex(player.EXTRA_INVENTORY,slot)
			if itemIndex !=0:
				item.SelectItem(itemIndex)
				if item.IsAntiFlag(item.ANTIFLAG_MYSHOP) or item.IsAntiFlag(item.ANTIFLAG_GIVE):
					return
				
				self.tooltipItem.AddRightClickForSale()

	def OnTop(self):
		if None != self.tooltipItem:
			self.tooltipItem.SetTop()
		
		if app.WJ_ENABLE_TRADABLE_ICON:
			map(lambda wnd:wnd.RefreshExtraLockedSlot(), self.bindWnds)
			self.RefreshMarkSlots()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def UseItemSlot(self, slotIndex):
		if constinfo.DRAG_SOURCE_TYPE == constinfo.DRAG_TYPE_INVENTORY:
			srcSlot = constinfo.DRAG_SOURCE_SLOT_INDEX
			net.SendItemMovePacket(player.INVENTORY, srcSlot, player.EXTRA_INVENTORY, slotIndex, 0)
			constinfo.DRAG_SOURCE_TYPE = -1
			constinfo.DRAG_SOURCE_SLOT_INDEX = -1
			return
		curCursorNum = app.GetCursor()
		if app.SELL == curCursorNum:
			return

		if constinfo.GET_ITEM_QUESTION_DIALOG_STATUS():
			return

		slotIndex = self.__InventoryLocalSlotPosToGlobalSlotPos(slotIndex)
		
		if app.ENABLE_DRAGON_SOUL_SYSTEM:
			if self.wndDragonSoulRefine.IsShow():
				self.wndDragonSoulRefine.AutoSetItem((player.EXTRA_INVENTORY, slotIndex), 1)
				return
			
		if app.__ENABLE_NEW_OFFLINESHOP__:
			if uiofflineshop.IsBuildingShop():
				itemIndex 	= player.GetItemIndex(player.EXTRA_INVENTORY, slotIndex)
				
				item.SelectItem(itemIndex)
				
				if not item.IsAntiFlag(item.ANTIFLAG_GIVE) and not item.IsAntiFlag(item.ANTIFLAG_MYSHOP):
					offlineshop.ShopBuilding_AddItem(player.EXTRA_INVENTORY, slotIndex)
				
				else:
					chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.OFFLINESHOP_CANT_SELECT_ITEM_DURING_BUILING)
				
				return

			elif uiofflineshop.IsBuildingAuction():
				itemIndex = player.GetItemIndex(player.EXTRA_INVENTORY,slotIndex)

				item.SelectItem(itemIndex)

				if not item.IsAntiFlag(item.ANTIFLAG_GIVE) and not item.IsAntiFlag(item.ANTIFLAG_MYSHOP):
					offlineshop.AuctionBuilding_AddItem(player.EXTRA_INVENTORY,slotIndex)
				else:
					chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.OFFLINESHOP_CANT_SELECT_ITEM_DURING_BUILING)

				return		
		
		if app.IsPressed(app.DIK_LCONTROL) or app.IsPressed(app.DIK_RCONTROL):
				itemCount = player.GetItemCount(player.EXTRA_INVENTORY, slotIndex)
				if not itemCount:
					self.__UseItem(slotIndex)
				else:
					for i in xrange(itemCount):
						self.__UseItem(slotIndex)
		else:
			self.__UseItem(slotIndex)
		
		mousemodule.mouseController.DeattachObject()
		self.OverOutItem()

	def __UseItem(self, slotIndex):
		ItemVNum = player.GetItemIndex(player.EXTRA_INVENTORY, slotIndex)
		item.SelectItem(ItemVNum)
		if item.IsFlag(item.ITEM_FLAG_CONFIRM_WHEN_USE):
			self.questionDialog = uicommon.QuestionDialog()
			self.questionDialog.SetText(localeinfo.INVENTORY_REALLY_USE_ITEM)
			self.questionDialog.SetAcceptEvent(ui.__mem_func__(self.__UseItemQuestionDialog_OnAccept))
			self.questionDialog.SetCancelEvent(ui.__mem_func__(self.__UseItemQuestionDialog_OnCancel))
			self.questionDialog.Open()
			self.questionDialog.slotIndex = slotIndex

			constinfo.SET_ITEM_QUESTION_DIALOG_STATUS(1)
		else:
			self.__SendUseItemPacket(slotIndex)

	def __UseItemQuestionDialog_OnCancel(self):
		self.OnCloseQuestionDialog()

	def __UseItemQuestionDialog_OnAccept(self):
		self.__SendUseItemPacket(self.questionDialog.slotIndex)
		self.OnCloseQuestionDialog()

	def __SendUseItemPacket(self, slotPos):
		if uiprivateshopbuilder.IsBuildingPrivateShop():
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.USE_ITEM_FAILURE_PRIVATE_SHOP)
			return

		net.SendItemUsePacket(player.EXTRA_INVENTORY, slotPos)

	def __SendMoveItemPacket(self, srcSlotPos, dstSlotPos, srcItemCount):
		if uiprivateshopbuilder.IsBuildingPrivateShop():
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.MOVE_ITEM_FAILURE_PRIVATE_SHOP)
			return

		if srcSlotPos == dstSlotPos:
			return

		net.SendItemMovePacket(player.EXTRA_INVENTORY, srcSlotPos, player.EXTRA_INVENTORY, dstSlotPos, srcItemCount)
	def __SendUseItemToItemPacket(self, srcSlotPos, dstSlotPos):
		if uiprivateshopbuilder.IsBuildingPrivateShop():
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.MOVE_ITEM_FAILURE_PRIVATE_SHOP)
			return

		if srcSlotPos == dstSlotPos:
			return

		net.SendItemUseToItemPacket(player.EXTRA_INVENTORY, srcSlotPos, player.EXTRA_INVENTORY, dstSlotPos)

	if app.ENABLE_SORT_INVEN:
		def SortExtraInventory(self):
			net.SendChatPacket("/sort_extra_inventory")

		def Sort_ExtraInventoryDone(self):
			if app.ENABLE_HIGHLIGHT_SYSTEM:
				for i in xrange(player.EXTRA_INVENTORY_CATEGORY_COUNT):
					del self.listHighlightedSlot[i][:]
			self.RefreshItemSlot()

	def ClickMallButton(self):
		print "click_mall_button"
		net.SendChatPacket("/click_mall")

	def ClickSafeboxButton(self):
		print "click_safebox_button"
		net.SendChatPacket("/click_safebox")
