# Add after imports the following line
IMG_DIR = "new_messenger/"





#search for class InventoryWindow(ui.ScriptWindow):
#add under self.__LoadWindow(): 





#######Start Inventory Search ##############
		self.highlightSlotDict = {}
		self.foundItemPages = {}
		self.listHighlightedSlot2 = []
		self.listHighlightedSlot = []
		self.suggestionButtons = []
		self.suggestionBoxes = []
		self.isSearching = False
		self.lastSearchText = ""
		self.lastSearchCheckTime = 0.0
		self.inventoryPageCount = 4
		self.inventoryPageIndex = 0 

		self.searchEdit = ui.EditLine()
		self.searchEdit.SetParent(self)
		self.searchEdit.SetPosition(30, 542)
		self.searchEdit.SetSize(100, 18)
		self.searchEdit.SetMax(20)
		infoText = self.GetPageInfoText()
		
		self.searchEdit.SetText("")  
		self.searchEdit.SetText(infoText)  
		self.searchEdit.SetEscapeEvent(self.__OnSearchEscape)
		self.searchEdit.SetReturnEvent(self.__OnSearchReturn)
		self.searchEdit.SetEndPosition()
		self.searchEdit.Show()

		self.suggestionsBox = ui.Window()
		self.suggestionsBox.SetParent(self)
		self.suggestionsBox.SetPosition(30, 553)
		self.suggestionsBox.SetSize(100, 110)
		self.suggestionsBox.SetVisible(False)

		self.searchButton = ui.Button()
		self.searchButton.SetParent(self)
		self.searchButton.SetPosition(7, 539)


		self.searchButton.SetUpVisual(IMG_DIR + "btn_1.png")
		self.searchButton.SetOverVisual(IMG_DIR + "btn_2.png")
		self.searchButton.SetDownVisual(IMG_DIR + "btn_3.png")

		self.searchButton.SetEvent(self.__OnSearchReturn)
		self.searchButton.Show()

		self.lastPageInfoTime = 0
		self.pageInfoLabel = ui.TextLine()
		self.pageInfoLabel.SetParent(self.suggestionsBox)
		self.pageInfoLabel.SetSize(200, 30)  
		self.pageInfoLabel.SetPosition(50, 200)  
		self.pageInfoLabel.SetText("")
		self.pageInfoLabel.Hide()


		self.clearButton = ui.Button()
		self.clearButton.SetParent(self)
		self.clearButton.SetPosition(147, 543)
		self.clearButton.SetUpVisual(IMG_DIR + "clear_btn_0.png")
		self.clearButton.SetOverVisual(IMG_DIR + "clear_btn_1.png")
		self.clearButton.SetDownVisual(IMG_DIR + "clear_btn_2.png")
		self.clearButton.SetEvent(self.__OnSearchEscape)

	# --- Escape clears search ---
	def __OnSearchEscape(self):
		self.searchEdit.SetText("")
		self.__ClearItemHighlights()
		self.suggestionsBox.SetVisible(False)
		self.clearButton.SetVisible(False)
		self.isSearching = False


	# --- Enter triggers search ---
	def __OnSearchReturn(self):
		searchText = self.searchEdit.GetText().lower()
		if not searchText:
			return
	
		self.__HighlightItemsByName(searchText)
		self.suggestionsBox.SetVisible(False)
		self.isSearching = False
		self.clearButton.SetVisible(False)
		infoText = self.GetPageInfoText()
		self.searchEdit.SetText(infoText)
		self.clearButton.SetVisible(True)
		self.searchEdit.OnSetFocus()
	
		self.__SmartJumpBasedOnSearch(searchText)
		self.DisplayPageInfo()
		self.searchEdit.Disable()

	# --- Suggestion clicked ---
	def __OnSuggestionClick(self, globalSlot, searchText):
		itemVnum = player.GetItemIndex(globalSlot)
		item.SelectItem(itemVnum)
		itemName = item.GetItemName()
		self.__HighlightItemsByName(itemName)
		infoText = self.GetPageInfoText()
		self.searchEdit.SetText(infoText)
		#self.searchEdit.SetText("")
		#self.searchEdit.SetText(itemName)
		self.suggestionsBox.Hide()
		self.suggestionsBox.SetVisible(False)
		self.isSearching = False
		pageText = self.GetItemPages(globalSlot)
		self.clearButton.SetVisible(True)
		self.__SmartJumpBasedOnSearch(searchText, preferredSlot=globalSlot)
		self.searchEdit.Disable()
		self.clearButton.SetVisible(True)

	def __SmartJumpBasedOnSearch(self, searchText, preferredSlot=None):
		if not self.listHighlightedSlot2 or not searchText:
			return  # No matches or no search
	
		currentPage = self.inventoryPageIndex
		targetSlot = None
	
		# 1. If preferredSlot (from suggestion click), use that
		if preferredSlot is not None:
			targetSlot = preferredSlot
		else:
			searchTextLower = searchText.lower()
			exactMatchSlot = None
			firstMatchSlot = None
	
			for slot in self.listHighlightedSlot2:
				itemVnum = player.GetItemIndex(slot)
				item.SelectItem(itemVnum)
				itemName = item.GetItemName().lower()
	
				# Exact match?
				if itemName == searchTextLower:
					exactMatchSlot = slot
					break  # Prefer exact, stop search
	
				# First partial match
				if firstMatchSlot is None and searchTextLower in itemName:
					firstMatchSlot = slot
	
			# Decide which slot to jump to
			if exactMatchSlot is not None:
				targetSlot = exactMatchSlot
			elif firstMatchSlot is not None:
				targetSlot = firstMatchSlot
	
		# Jump to targetSlot page if found
		if targetSlot is not None:
			page = targetSlot // player.INVENTORY_PAGE_SIZE
			if page != currentPage:
				self.__JumpToPage(page)
	
	



# --- Highlight matching items and store their locations ---
	def __HighlightItemsByName(self, searchText):
		self.__ClearItemHighlights()
		self.foundItemPages = {}
		if not searchText:
			return
	
		searchTextLower = searchText.lower()
		totalSlots = player.INVENTORY_PAGE_SIZE * 4
		self.foundItemPages = {}  # Dictionary to store the item locations by their name
	
		for globalSlot in range(totalSlots):
			itemVnum = player.GetItemIndex(globalSlot)
			if itemVnum == 0:
				continue
			item.SelectItem(itemVnum)
			itemName = item.GetItemName().lower()
			if searchTextLower in itemName:
				self.HighlightSlot(globalSlot)  # Highlight the matching slot
				self.listHighlightedSlot2.append(globalSlot)  # Add matching slot to the list
				
				# Track the pages where the item is found
				page = globalSlot // player.INVENTORY_PAGE_SIZE  # Calculate page index
				if itemName not in self.foundItemPages:
					self.foundItemPages[itemName] = []
				if page + 1 not in self.foundItemPages[itemName]:  # Avoid duplicates in the page list
					self.foundItemPages[itemName].append(page + 1)
	
		self.__HighlightSlot_Refresh()

	def __UpdateSuggestions(self, searchText):
		# Hide all existing suggestion buttons
		greenColor = 0xFF00FF00
		redColor = 0xFFFF0000  # Red
		blueColor = 0xFF0000FF  # Blue
		yellowColor = 0xFFFFFF00  # Yellow
		whiteColor = 0xFFFFFFFF  # White
		blackColor = 0xFF000000  # Black
		
		for button in self.suggestionButtons:
			button.Hide()
		self.suggestionButtons = []
		#self.seenItemNames = []

		# If no search text, hide the suggestions box
		if not searchText:
			self.suggestionsBox.SetVisible(False)
			return

		searchTextLower = searchText.lower()
		suggestionCount = 0
		totalSlots = player.INVENTORY_PAGE_SIZE * 4  # Assuming you have 4 pages of items
		maxSuggestions = 5  # The maximum number of suggestions to show
		buttonHeight = 23  # Height of each suggestion button
		maxBoxHeight = 280  # Max height for the suggestions box (if the board is small)
		minSuggestionsHeight = 0 * buttonHeight  # Minimum height for 4 suggestions
		seenItemNames = set() 

		for globalSlot in range(totalSlots):
			if suggestionCount >= maxSuggestions:
				break  
			itemVnum = player.GetItemIndex(globalSlot)
			if itemVnum == 0:
				continue  

			item.SelectItem(itemVnum)
			itemName = item.GetItemName().lower()

			if itemName in seenItemNames:
				continue  
		
		# Add item name to the set to avoid duplicates
			seenItemNames.add(itemName)

			# Check if the item name contains the search text
			if searchTextLower in itemName:
				suggestionButton = ui.Button()
				suggestionButton.SetParent(self.suggestionsBox)
				suggestionButton.SetPosition(9, suggestionCount * buttonHeight)  # Position buttons vertically #4
				suggestionButton.SetSize(120, buttonHeight)  # Button height is 15

				suggestionButton.SetUpVisual(IMG_DIR + "search_bg2.png")
				suggestionButton.SetOverVisual(IMG_DIR + "search_bg2_hover.png")
				suggestionButton.SetDownVisual(IMG_DIR + "search_bg2_down.png")

				suggestionButton.SetText(itemName)
				suggestionButton.SetTextColor(yellowColor)				# Green text


				suggestionButton.SetEvent(lambda slot=globalSlot, searchText=searchText: self.__OnSuggestionClick(slot, searchText))


				suggestionButton.Show()
				self.suggestionButtons.append(suggestionButton)  # Add to the list of buttons
				suggestionCount += 1

		boxHeight = suggestionCount * buttonHeight  # Each suggestion button has a height of 15

		if boxHeight > maxBoxHeight:
			boxHeight = maxBoxHeight  # Cap the height to the maximum allowed

		if suggestionCount <= 2:
			boxHeight = max(minSuggestionsHeight, suggestionCount * buttonHeight)  # Ensure there's a minimum height

		if suggestionCount > 0 and self.isSearching:
			self.suggestionsBox.SetVisible(True)
			self.suggestionsBox.Show()
			self.clearButton.SetVisible(True)

			self.DisplayPageInfo()

		else:
			self.suggestionsBox.SetVisible(False)
			self.suggestionsBox.Hide()
			self.clearButton.SetVisible(True)



	# --- Clear highlights across all pages ---
	def __ClearItemHighlights(self):
		for i in range(player.INVENTORY_PAGE_SIZE):
			for page in range(4):
				globalSlot = page * player.INVENTORY_PAGE_SIZE + i
				if globalSlot in self.listHighlightedSlot2:
					self.DelHighlightSlot(globalSlot)

	# --- Periodic check for input change ---
	def OnUpdate(self):
		currentTime = app.GetTime()
		if currentTime - self.lastSearchCheckTime >= 0.2:
			self.lastSearchCheckTime = currentTime
			self.__CheckSearchInputChange()

		# Check if 1.5 seconds have passed since displaying the page info
		if self.pageInfoLabel.Show():
			if currentTime - self.lastPageInfoTime >= 1.5:
				self.HidePageInfo()

	def GetPageInfoText(self):
		if hasattr(self, 'foundItemPages') and self.foundItemPages:
			pageInfoText = []
			for itemName, pages in self.foundItemPages.items():
				pageInfoText.append("'%s' Pagina: %s" % ("Gasit in", ', '.join(map(str, pages))))
			return "\n".join(pageInfoText)
		else:
			return "No items found."
	
	def DisplayPageInfo(self):
		infoText = self.GetPageInfoText()
		#self.pageInfoLabel.SetText(infoText)
		self.pageInfoLabel.Show()
		self.lastPageInfoTime = app.GetTime()


	def HidePageInfo(self):
		self.pageInfoLabel.Hide()  # Hide the label

	def GetItemPages(self, globalSlot):
		# Get pages for this item
		pages = []
		for page in range(4):  # Iterate over the 4 pages
			if globalSlot in self.listHighlightedSlot2:  # Assuming this method is used to highlight slots on pages
				pages.append(page + 1)  # Page numbers are typically 1-based
		if pages:
			return "Item found on pages: " + ", ".join(map(str, pages))
		return "Item not found on any page"

	# --- Compare input to last ---
	def __CheckSearchInputChange(self):
		currentText = self.searchEdit.GetText()
		if currentText != self.lastSearchText:
			self.lastSearchText = currentText
			self.isSearching = True if currentText else False
			self.__UpdateSuggestions(currentText.lower())


	def __JumpToFirstMatchingPage(self):
		if self.listHighlightedSlot2:
			firstSlot = min(self.listHighlightedSlot2)
			page = firstSlot // player.INVENTORY_PAGE_SIZE
			self.__JumpToPage(page)

	def __JumpToPage(self, page):
		if page < 0 or page >= self.inventoryPageCount:
			return

		if self.inventoryPageIndex != page:
			self.inventoryPageIndex = page
			self.RefreshBagSlotWindow()

		# 🔥 Force tab update every jump
		self.__UpdateInventoryTabHighlight()

	def __OnClickInventoryTab(self, index):
		self.__JumpToPage(index)
		self.__UpdateInventoryTabHighlight()
	def __UpdateInventoryTabHighlight(self):
		for i in xrange(4):
			if i == self.inventoryPageIndex:
				self.inventoryTab[i].Down()  # Visually pressed
			else:
				self.inventoryTab[i].SetUp()  # Normal state
	

##### end Kolen's Search Inventory items #####





#####search for  self.dlgPickMoney = dlgPickMoney or self.equipmentTab[1].Hide()

#add 
##########Inventory sugestion box search
		self.searchBackground = ui.ImageBox()
		self.searchBackground.SetParent(self)
		self.searchBackground.SetPosition(30, 540)
		self.searchBackground.SetSize(100, 5)
		self.searchBackground.LoadImage("d:/ymir work/ui/public/parameter_slot_05.sub")
		self.searchBackground.SetVisible(True)
		self.searchBackground.Show()
##########end search inventory suggestion

#Search for:

			#self.inventoryTab.append(self.GetChild("Inventory_Tab_01"))
			#self.inventoryTab.append(self.GetChild("Inventory_Tab_02"))
			#self.inventoryTab.append(self.GetChild("Inventory_Tab_03"))
			#self.inventoryTab.append(self.GetChild("Inventory_Tab_04"))

#and ADD:

			for i in xrange(4):
				self.inventoryTab[i].SetEvent(lambda idx=i: self.__OnClickInventoryTab(idx))



# Search: 		def __HighlightSlot_Refresh(self):

#and make it look like below:

		def __HighlightSlot_Refresh(self):
			for i in range(self.wndItem.GetSlotCount()):
				slotNumber = self.__InventoryLocalSlotPosToGlobalSlotPos(i)
				if slotNumber in self.listHighlightedSlot:  # Only apply highlight to these slots
					self.wndItem.ActivateSlot(i)
					#self.wndItem.SetSlotDiffuseColor(i, wndMgr.COLOR_TYPE_YELLOW)  # Yellow color
					#self.wndItem.SetSlotFlashEffect(i, True)  # Flash effect	
					if slotNumber in self.listHighlightedSlot2:
						self.wndItem.ActivateSlot(i)
						self.wndItem.SetSlotDiffuseColor(i, wndMgr.COLOR_TYPE_RED)  # Yellow color
						#self.wndItem.SetSlotFlashEffect(i, True)  # Flash effect	