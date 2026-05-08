import ui, item, os, constinfo, localeinfo
from _weakref import proxy

import app
import net
import player



itemshopData = {}
playerLog = []

UPDATE_TIME = -1

def ItemShopLogClear():
	global playerLog
	playerLog=[]

def ItemShopAppendLog(dateText, dateTime, playerName, ipAdress, itemVnum, itemCount, itemPrice):
	dateTimeReal = "None"
	dateSplit = dateText.split(" ")
	if len(dateSplit) > 1:
		dateFirst = dateSplit[0].split("-")
		if len(dateFirst) == 3:
			dateTimeReal = dateFirst[2]+"/"+dateFirst[1]+"/"+dateFirst[0]
			dateTimeReal+=" "
			dateSecond = dateSplit[1].split(":")
			if len(dateSecond) == 3:
				dateTimeReal += dateSecond[0]+":"+dateSecond[1]

	global playerLog
	playerLog.append([dateTimeReal,dateTime,playerName,ipAdress,itemVnum,itemCount,itemPrice])

def ItemShopClear(time):
	global itemshopData
	global UPDATE_TIME
	if time != UPDATE_TIME:
		itemshopData.clear()
		UPDATE_TIME = time

def ItemShopAppendItem(categoryIndex, categorySubIndex, itemID, itemVnum, itemPrice, itemDiscount, itemOffertime, itemTopSelling, itemAddedTime, itemSellingCount):
	if not itemshopData.has_key(categoryIndex):
		subCategoryList=[]
		for x in xrange(10):
			subCategoryList.append([])
		itemshopData[categoryIndex] = subCategoryList
	itemshopData[categoryIndex][categorySubIndex].append([itemID, itemVnum, itemPrice, itemDiscount, itemOffertime, itemTopSelling, itemAddedTime, itemSellingCount])


buyQuestionDialog = None

LIST_ITEM_ID=0
LIST_ITEM_VNUM=1
LIST_ITEM_PRICE=2
LIST_ITEM_DISCOUNT=3
LIST_ITEM_TIME=4
LIST_ITEM_TOP_SELLING_INDEX=5
LIST_ITEM_ADDED_TIME=6
LIST_ITEM_SELLING_COUNT=7

LOG_DATE_TEXT = 0
LOG_DATE = 1
LOG_ACCOUNT = 2
LOG_IP = 3
LOG_ITEM = 4
LOG_COUNT = 5
LOG_PRICE = 6


HOT_ITEM=1
NEW_ITEM=2
OFFER_ITEM=3

#Config

WEBSITE_LINK = "https://bwmt2-global.eu"
eventCategory = [
	# [[FILENAMES],[CATEGORY, SUB_CATEGORY]]
	[["costume_banner_0.tga","costume_banner_1.tga"],[0,0]],
]

IMG_DIR ="d:/ymir work/ui/game/ishop/"

itemShopCategoryData = {
	0:{#föoldal
		"name":localeinfo.CATEGORY_0_NAME,#föoldal
		"subCategory":[],
		},
	1:{#fejlödés
		"name":localeinfo.CATEGORY_1_NAME,#
		"subCategory":[],
		},
	2:{#tárgy fejlesztés
		"name":localeinfo.CATEGORY_2_NAME,
		"subCategory":[],
		},
	3:{#Belépök
		"name":localeinfo.CATEGORY_3_NAME,
		"subCategory":[],
		},
	4:{#jelmez
		"name":localeinfo.CATEGORY_4_NAME,
		"subCategory":[localeinfo.CATEGORY_4_SUB_0_NAME, localeinfo.CATEGORY_4_SUB_1_NAME, localeinfo.CATEGORY_4_SUB_2_NAME, localeinfo.CATEGORY_4_SUB_3_NAME, localeinfo.CATEGORY_4_SUB_4_NAME, localeinfo.CATEGORY_4_SUB_5_NAME, localeinfo.CATEGORY_4_SUB_6_NAME],
		},
	# 5:{#kisérö
		# "name":localeinfo.CATEGORY_5_NAME,
		# "subCategory":[localeinfo.CATEGORY_5_SUB_0_NAME, localeinfo.CATEGORY_5_SUB_1_NAME],
		# },
	6:{#állatok
		"name":localeinfo.CATEGORY_6_NAME,
		"subCategory":[localeinfo.CATEGORY_6_SUB_0_NAME, localeinfo.CATEGORY_6_SUB_1_NAME],
		},
	7:{#egyéb
		"name":localeinfo.CATEGORY_7_NAME,
		"subCategory":[],
		},
	# 8:{#kovácsolás
		# "name":localeinfo.CATEGORY_8_NAME,
		# "subCategory":[localeinfo.CATEGORY_8_SUB_0_NAME, localeinfo.CATEGORY_8_SUB_1_NAME],
		# },
	# 9:{#fejleszö
		# "name":localeinfo.CATEGORY_9_NAME,
		# "subCategory":[],
		# },
	10:{#csomagok
		"name":localeinfo.CATEGORY_10_NAME,
		"subCategory":["PVP", "PVM", "ANIME", "Egyeb"],
		},
}

class ItemShopWindow(ui.Window):
	def __del__(self):
		ui.Window.__del__(self)
	def Destroy(self):
		ItemShopLogClear()
		if self.children.has_key("categoryListBox"):
			self.children["categoryListBox"].RemoveAllItems()
		if self.children.has_key("itemListBox"):
			self.children["itemListBox"].RemoveAllItems()
		if self.children.has_key("PurchasesListBox"):
			self.children["PurchasesListBox"].RemoveAllItems()
		self.CloseWeb()
	
		global buyQuestionDialog
		if buyQuestionDialog != None:
			buyQuestionDialog.Hide()
			buyQuestionDialog.Destroy()
			buyQuestionDialog=None

		self.RealMoney = 0
		self.NewMoney = 0
		self.MoneyRange = 0
		self.MoneyFlag = False
		self.moneySize = 0

		self.children = {}
	def __init__(self):
		ui.Window.__init__(self)
		self.children = {}
		self.Destroy()
		self.LoadWindow()

	def LoadWindow(self):
		self.AddFlag("movable")
		self.AddFlag("attach")
		self.AddFlag("float")

		boardImage = ui.ImageBox()
		boardImage.SetParent(self)
		boardImage.AddFlag("attach")
		boardImage.LoadImage(IMG_DIR+"bg.tga")
		self.SetSize(boardImage.GetWidth(),boardImage.GetHeight())
		boardImage.SetPosition(0,0)
		boardImage.Show()
		self.children["boardImage"] = boardImage

		closeBtn = ui.Button()
		closeBtn.SetParent(boardImage)
		closeBtn.SetUpVisual(IMG_DIR+"close_0.tga")
		closeBtn.SetOverVisual(IMG_DIR+"close_1.tga")
		closeBtn.SetDownVisual(IMG_DIR+"close_2.tga")
		closeBtn.SetEvent(self.Close)
		closeBtn.SetPosition(boardImage.GetWidth()-closeBtn.GetWidth()-5,5)
		closeBtn.Show()
		self.children["closeBtn"] = closeBtn

		leftDragonCoinImage = ui.ImageBox()
		leftDragonCoinImage.SetParent(boardImage)
		leftDragonCoinImage.LoadImage(IMG_DIR+"coin.tga")
		leftDragonCoinImage.SetPosition(35,20)
		leftDragonCoinImage.Show()
		self.children["leftDragonCoinImage"] = leftDragonCoinImage

		dragonCoinText = MultiTextLine()
		dragonCoinText.SetParent(boardImage)
		dragonCoinText.SetPosition(35+leftDragonCoinImage.GetWidth()+10,17)
		dragonCoinText.SetTextRange(17)
		dragonCoinText.SetTextType("horizontal#left")
		dragonCoinText.SetFontName("Tahoma:14")
		dragonCoinText.SetOutline(True)
		dragonCoinText.Show()
		self.children["dragonCoinText"] = dragonCoinText


		buyCoinBtn = ui.Button()
		buyCoinBtn.SetParent(boardImage)
		buyCoinBtn.SetUpVisual(IMG_DIR+"button1_0.tga")
		buyCoinBtn.SetOverVisual(IMG_DIR+"button1_1.tga")
		buyCoinBtn.SetDownVisual(IMG_DIR+"button1_2.tga")
		buyCoinBtn.SetEvent(self.BuyDragonCoin)
		buyCoinBtn.SetPosition(180,12)
		buyCoinBtn.Show()
		self.children["buyCoinBtn"] = buyCoinBtn

		buyCoinBtnText = MultiTextLine()
		buyCoinBtnText.SetParent(buyCoinBtn)
		buyCoinBtnText.SetTextRange(17)
		buyCoinBtnText.SetPosition(buyCoinBtn.GetWidth()/2,10)
		buyCoinBtnText.SetTextType("horizontal#center")
		buyCoinBtnText.SetFontName("Tahoma:16")
		buyCoinBtnText.SetText(localeinfo.ITEMSHOP_BUY_COIN)
		buyCoinBtnText.Show()
		buyCoinBtn.multiText = buyCoinBtnText

		purchasesBtn = ui.Button()
		purchasesBtn.SetParent(boardImage)
		purchasesBtn.SetUpVisual(IMG_DIR+"button2_0.tga")
		purchasesBtn.SetOverVisual(IMG_DIR+"button2_1.tga")
		purchasesBtn.SetDownVisual(IMG_DIR+"button2_2.tga")
		purchasesBtn.SetEvent(self.PurchaseBtn)
		purchasesBtn.SetPosition(180+buyCoinBtn.GetWidth()+7,12)
		purchasesBtn.Show()
		self.children["purchasesBtn"] = purchasesBtn

		purchasesBtnText = MultiTextLine()
		purchasesBtnText.SetParent(purchasesBtn)
		purchasesBtnText.SetTextRange(17)
		purchasesBtnText.SetPosition(purchasesBtn.GetWidth()/2,10)
		purchasesBtnText.SetTextType("horizontal#center")
		purchasesBtnText.SetFontName("Tahoma:16")
		purchasesBtnText.SetText(localeinfo.ITEMSHOP_PURCHASES)
		purchasesBtnText.Show()
		purchasesBtn.multiText = purchasesBtnText

		hotItemsBtn = ui.Button()
		hotItemsBtn.SetParent(boardImage)
		hotItemsBtn.SetUpVisual(IMG_DIR+"button2_0.tga")
		hotItemsBtn.SetOverVisual(IMG_DIR+"button2_1.tga")
		hotItemsBtn.SetDownVisual(IMG_DIR+"button2_2.tga")
		hotItemsBtn.SetEvent(self.HotItems)
		hotItemsBtn.SetPosition(180+((buyCoinBtn.GetWidth()+7)*2),12)
		hotItemsBtn.Show()
		self.children["hotItemsBtn"] = hotItemsBtn

		hotItemsBtnText = MultiTextLine()
		hotItemsBtnText.SetParent(hotItemsBtn)
		hotItemsBtnText.SetTextRange(17)
		hotItemsBtnText.SetPosition(hotItemsBtn.GetWidth()/2,10)
		hotItemsBtnText.SetTextType("horizontal#center")
		hotItemsBtnText.SetFontName("Tahoma:16")
		hotItemsBtnText.SetText(localeinfo.ITEMSHOP_HOTITEMS)
		hotItemsBtnText.Show()
		hotItemsBtn.multiText = hotItemsBtnText

		offersBtn = ui.Button()
		offersBtn.SetParent(boardImage)
		offersBtn.SetUpVisual(IMG_DIR+"button2_0.tga")
		offersBtn.SetOverVisual(IMG_DIR+"button2_1.tga")
		offersBtn.SetDownVisual(IMG_DIR+"button2_2.tga")
		offersBtn.SetEvent(self.OfferItems)
		offersBtn.SetPosition(180+((buyCoinBtn.GetWidth()+7)*3),12)
		offersBtn.Show()
		self.children["offersBtn"] = offersBtn

		offersBtnText = MultiTextLine()
		offersBtnText.SetParent(offersBtn)
		offersBtnText.SetTextRange(17)
		offersBtnText.SetPosition(offersBtn.GetWidth()/2,10)
		offersBtnText.SetTextType("horizontal#center")
		offersBtnText.SetFontName("Tahoma:16")
		offersBtnText.SetText(localeinfo.ITEMSHOP_OFFERS)
		offersBtnText.Show()
		offersBtn.multiText = offersBtnText

		itemSearchBg = ui.ImageBox()
		itemSearchBg.SetParent(boardImage)
		itemSearchBg.LoadImage(IMG_DIR+"search_box.tga")
		itemSearchBg.SetPosition(612,13)
		itemSearchBg.Show()
		self.children["itemSearchBg"] = itemSearchBg

		itemSearch = ui.EditLine()
		itemSearch.SetParent(itemSearchBg)
		itemSearch.SetMax(40)
		itemSearch.SetPosition(16,10)
		itemSearch.SetSize(itemSearchBg.GetWidth()-16, itemSearchBg.GetHeight()-10)
		itemSearch.OnIMEUpdate = ui.__mem_func__(self.__OnValueUpdate)
		itemSearch.Show()
		self.children["itemSearch"] = itemSearch

		itemSearchText = MultiTextLine()
		itemSearchText.SetParent(itemSearch)
		itemSearchText.SetTextRange(15)
		itemSearchText.SetPosition(0,-2)
		itemSearchText.SetFontColor(128,128,128)
		itemSearchText.SetTextType("horizontal#left")
		itemSearchText.SetFontName("Tahoma:16")
		itemSearchText.SetText(localeinfo.ITEMSHOP_ITEMSEARCH_DEFAULT_TEXT)
		itemSearchText.Show()
		itemSearch.backText = itemSearchText

		categoryScrollBar = ScrollBar()
		categoryScrollBar.SetParent(boardImage)
		categoryScrollBar.SetPosition(169,76)
		categoryScrollBar.SetScrollBarSize(435)
		categoryScrollBar.SetScrollStep(0.07)
		categoryScrollBar.Show()
		self.children["categoryScrollBar"] = categoryScrollBar

		categoryListBox = ListBox()
		categoryListBox.SetParent(boardImage)
		categoryListBox.SetPosition(10,76)
		categoryListBox.SetSize(150,55*8)
		categoryListBox.SetItemStep(2)
		categoryListBox.SetItemSize(150,55)
		categoryListBox.SetScrollBar(categoryScrollBar)
		categoryListBox.Show()
		self.children["categoryListBox"] = categoryListBox

		itemScrollBar = ScrollBar()
		itemScrollBar.SetParent(boardImage)
		itemScrollBar.SetPosition(200+750+10,76)
		itemScrollBar.SetScrollBarSize(435)
		itemScrollBar.SetScrollStep(0.07)
		itemScrollBar.Show()
		self.children["itemScrollBar"] = itemScrollBar

		itemListBox = ListBox()
		itemListBox.SetParent(boardImage)
		itemListBox.SetPosition(200,76)
		itemListBox.SetSize(750,445)
		itemListBox.SetItemStep(8)
		itemListBox.SetItemSize(750,122)
		itemListBox.SetScrollBar(itemScrollBar)
		itemListBox.OnUpdate = ui.__mem_func__(self.OnUpdate)
		itemListBox.Show()
		self.children["itemListBox"] = itemListBox

		PurchasesScrollBar = ScrollBar()
		PurchasesScrollBar.SetParent(boardImage)
		PurchasesScrollBar.SetPosition(200+750+10,76)
		PurchasesScrollBar.SetScrollBarSize(435)
		PurchasesScrollBar.SetScrollStep(0.07)
		self.children["PurchasesScrollBar"] = PurchasesScrollBar

		PurchasesListBox = ListBox()
		PurchasesListBox.SetParent(boardImage)
		PurchasesListBox.SetPosition(16,75)
		PurchasesListBox.SetSize(927,440)
		PurchasesListBox.SetItemStep(8)
		PurchasesListBox.SetItemSize(927,28)
		PurchasesListBox.SetScrollBar(PurchasesScrollBar)
		self.children["PurchasesListBox"] = PurchasesListBox

		LoadingImage = ui.ExpandedImageBox()
		LoadingImage.SetParent(boardImage)
		LoadingImage.SetPosition(boardImage.GetWidth()/2,boardImage.GetHeight()/2)
		LoadingImage.LoadImage(IMG_DIR+"load_.tga")
		self.children["LoadingImage"] = LoadingImage

		self.children["LoadingImageRotation"] = 0

		self.SetDragonCoin(0)
		self.SetCenterPosition()
		self.LoadShopCategory()

	def __OnValueUpdate(self):
		itemSearch = self.children["itemSearch"]
		ui.EditLine.OnIMEUpdate(itemSearch)

		self.ClearItemList()
		searchItemList = []
		searchText = itemSearch.GetText()
		if len(searchText) > 0:
			for key, data in itemshopData.items():
				for itemList in data:
					for itemData in itemList:
						item.SelectItem(itemData[LIST_ITEM_VNUM])
						if item.GetItemName().find(searchText) != -1:
							searchItemList.append(itemData)
			self.RefreshItemList(searchItemList)
		else:
			self.LoadFirstOpening()

	def OnUpdate(self):
		if self.children.has_key("itemListBox"):
			listboxData = self.children["itemListBox"].GetItems()
			for itemPointer in listboxData:
				itemPointer.OnUpdate()
		if self.children.has_key("LoadingImage"):
			LoadingImage = self.children["LoadingImage"]
			if LoadingImage.IsShow():
				self.children["LoadingImageRotation"] += 15
				LoadingImage.SetRotation(self.children["LoadingImageRotation"])

		if self.MoneyFlag == True:
			if self.moneySize>=0 and self.moneySize<=15:
				if self.NewMoney > self.RealMoney:
					self.RealMoney += ((self.MoneyRange)/15)
					self.SetDragonCoinReal(self.RealMoney)
				else:
					self.RealMoney -= ((self.MoneyRange)/15)
					self.SetDragonCoinReal(self.RealMoney)
				self.moneySize+=1
				if self.moneySize ==15:
					self.RealMoney = self.NewMoney
					self.SetDragonCoinReal(self.RealMoney)
					self.MoneyFlag = False

	def LoadShopCategory(self):
		self.categoryIndex = -1
		self.categorySubIndex = -1
		categoryListBox = self.children["categoryListBox"]
		for key, data in itemShopCategoryData.items():
			categoryItem = CategoryItem()
			categoryItem.SetParent(categoryListBox)
			categoryItem.SetPosition(0,len(categoryListBox.GetItems())*57, True)
			categoryItem.SetData(key, data["name"],categoryListBox)
			categoryItem.SAFE_SetStringEvent("mouse_over_in",self.OverInCategory,key, proxy(categoryItem))
			categoryItem.SAFE_SetStringEvent("mouse_over_out",self.OverOutCategory, key, proxy(categoryItem))
			categoryItem.SetEvent(self.ClickCategory, "mouse_click", key, -1)
			categoryItem.Show()
			categoryListBox.AppendItem(categoryItem)
		categoryListBox.SetBasePos(1)

	def ClickCategory(self, emptyArg, categoryIndex, categorySubIndex = -1):
		if self.categoryIndex == categoryIndex and categorySubIndex == self.categorySubIndex:
			if categorySubIndex == -1:
				categoryItems = self.children["categoryListBox"].GetItems()
				for category in categoryItems:
					if category.categoryIndex == categoryIndex:
						self.OverInCategory(categoryIndex, category)
						break
			return

		oldIndex = self.categoryIndex
		self.categoryIndex = categoryIndex
		self.categorySubIndex = categorySubIndex

		categoryItems = self.children["categoryListBox"].GetItems()
		for category in categoryItems:
			if category.categoryIndex == categoryIndex:
				self.OverInCategory(categoryIndex, category)
			elif category.categoryIndex == oldIndex:
				self.OverOutCategory(oldIndex, category)

		self.UpdateSubCategoryWindow()
		self.RefreshItem()
		
		if categorySubIndex != -1:
			self.ClearSubCategoryWindow(True)

	def OverOutCategory(self, categoryIndex, categoryItem):
		self.ClearSubCategoryWindow()
		if categoryIndex==self.categoryIndex:
			return
		categoryItem.LoadImage(IMG_DIR+"category_no_selected.tga")
		categoryItem.OnRender()

	def OverInCategory(self, categoryIndex, categoryItem):
		subCategoryBtns = self.children.get("subCategoryBtns", None)
		if subCategoryBtns != None and subCategoryBtns.categoryIndex != categoryIndex:
			self.ClearSubCategoryWindow(True)
			subCategoryBtns = None
		if subCategoryBtns == None:
			subCategoryBtns = self.CreateSubCategoryWindow(categoryIndex)
		if subCategoryBtns != None:
			subCategoryBtns.Show()
			(px,py) = categoryItem.parent.GetLocalPosition()
			maxYPos = py+categoryItem.parent.GetHeight()
			(x, y) = categoryItem.GetLocalPosition()

			if y <= 0:
				subCategoryBtns.SetPosition(px+categoryItem.GetWidth()+2,py)
			elif (py+y+(len(subCategoryBtns.children)*39)) <= maxYPos:
				subCategoryBtns.SetPosition(px+categoryItem.GetWidth()+2,py+y)
			else:
				subCategoryBtns.SetPosition(px+categoryItem.GetWidth()+2,maxYPos-(len(subCategoryBtns.children)*39)-1)
			self.children["subCategoryBtns"]=subCategoryBtns

		categoryItem.LoadImage(IMG_DIR+"category_selected.tga")
		categoryItem.OnRender()

	def UpdateSubCategoryWindow(self):
		if self.children.has_key("subCategoryBtns"):
			subCategoryBtns = self.children["subCategoryBtns"]
			if subCategoryBtns != None:
				if subCategoryBtns.categoryIndex == self.categoryIndex:
					if self.categorySubIndex != -1:
						self.__ClickRadioButton(subCategoryBtns.children, self.categorySubIndex)

	def __IsCursorInRect(self, x, y, width, height):
		(cx, cy) = app.GetCursorPosition()
		return cx >= x and cx <= x + width and cy >= y and cy <= y + height

	def ClearSubCategoryWindow(self, force = False):
		if self.children.has_key("subCategoryBtns"):
			subCategoryBtns = self.children["subCategoryBtns"]
			if subCategoryBtns != None:
				if force == False:
					(sx,sy) = subCategoryBtns.GetGlobalPosition()
					if self.__IsCursorInRect(sx, sy, 118, len(subCategoryBtns.children)*39):
						return
						
					if self.children.has_key("categoryListBox"):
						categoryListBox = self.children["categoryListBox"]
						(lx, ly) = categoryListBox.GetGlobalPosition()
						if self.__IsCursorInRect(lx, ly, categoryListBox.GetWidth()+3, categoryListBox.GetHeight()):
							return

				if subCategoryBtns:
					for children in subCategoryBtns.children:
						children.backText = None
					subCategoryBtns.children = None
					subCategoryBtns.Hide()

				self.children["subCategoryBtns"] = None

	def CreateSubCategoryWindow(self, categoryIndex):
		if not itemShopCategoryData.has_key(categoryIndex):
			return None
		categoryData = itemShopCategoryData[categoryIndex]["subCategory"]
		if len(categoryData) == 0:
			return None

		subCategoryBtns = ui.ImageBox()
		subCategoryBtns.SetParent(self)
		children = []

		for j in xrange(len(categoryData)):
			categoryBtn = ui.RadioButton()
			categoryBtn.SetParent(subCategoryBtns)
			categoryBtn.SetUpVisual(IMG_DIR+"submenu_selected.tga")
			categoryBtn.SetOverVisual(IMG_DIR+"submenu_selected2.tga")
			categoryBtn.SetDownVisual(IMG_DIR+"submenu_selected2.tga")
			#categoryBtn.SetHideToolTipEvent(ui.__mem_func__(self.ClearSubCategoryWindow))
			categoryBtn.SetEvent(self.ClickCategory, "", categoryIndex, j)
			categoryBtn.SetPosition(0,39*j)
			categoryBtn.Show()

			categoryBtnText = MultiTextLine()
			categoryBtnText.SetParent(categoryBtn)
			categoryBtnText.SetTextRange(15)
			categoryBtnText.SetPosition(categoryBtn.GetWidth()/2,8)
			categoryBtnText.SetTextType("horizontal#center")
			categoryBtnText.SetFontName("Tahoma:16")
			categoryBtnText.SetText(categoryData[j])
			categoryBtnText.Show()
			categoryBtn.backText = categoryBtnText
			children.append(categoryBtn)
		subCategoryBtns.categoryIndex = categoryIndex
		subCategoryBtns.children = children
		if self.categoryIndex == categoryIndex:
			if self.categorySubIndex != -1:
				self.__ClickRadioButton(subCategoryBtns.children, self.categorySubIndex)
		subCategoryBtns.SetSize(115, len(children)*39)
		return subCategoryBtns

	def RefreshItem(self):
		self.ClearItemList()
		itemListBox = self.children["itemListBox"]
		if itemshopData.has_key(self.categoryIndex):
			itemData = itemshopData[self.categoryIndex]
			if self.categorySubIndex == -1:
				allList = []
				for itemList in itemData:
					for shopItem in itemList:
						allList.append(shopItem)
				self.RefreshItemList(allList)
			else:
				if self.categorySubIndex >= len(itemData):
					return
				itemList = itemData[self.categorySubIndex]
				self.RefreshItemList(itemList)

	def SortList(self, itemDataList):
		sortData = []
		for itemData in itemDataList:
			sortItem = SortClass(itemData)
			sortData.append(sortItem)
		sortData.sort()
		itemDataListEx = []
		for shortItem in sortData:
			itemDataListEx.append(shortItem.arg)
		return itemDataListEx

	def ClearItemList(self):
		itemListBox = self.children["itemListBox"]
		itemListBox.RemoveAllItems()

	def RefreshItemList(self, itemDataList):
		self.ClosePurchasesWindow()
		if len(itemDataList) == 0:
			return
		itemListBox = self.children["itemListBox"]

		itemDataList = self.SortList(itemDataList)
		itemWindowPtr = ItemWindow(itemListBox)
		itemWindowPtr.SetParent(itemListBox)
		itemWindowPtr.SetPosition(0,len(itemListBox.GetItems())*132, True)
		itemWindowPtr.Show()
		for shopItem in itemDataList:
			xItem = ItemShopItem()
			xItem.SetParent(itemWindowPtr)
			xItem.SetPosition(len(itemWindowPtr.children)*250,0)
			xItem.LoadData(shopItem)
			xItem.Show()
			itemWindowPtr.AppendWindow(xItem)
			if len(itemWindowPtr.children) == 3:
				itemListBox.AppendItem(itemWindowPtr)
				itemWindowPtr = ItemWindow(itemListBox)
				itemWindowPtr.SetParent(itemListBox)
				itemWindowPtr.SetPosition(0,len(itemListBox.GetItems())*132, True)
				itemWindowPtr.Show()
		if itemWindowPtr != None:
			if len(itemWindowPtr.children) > 0:
				itemListBox.AppendItem(itemWindowPtr)

	def InsertItemWithType(self, itemDataList, insertType):
		currentTime = app.GetGlobalTimeStamp()
		for key, data in itemshopData.items():
			for categoryItems in data:
				for itemData in categoryItems:
					if insertType == LIST_ITEM_DISCOUNT:
						if itemData[LIST_ITEM_DISCOUNT] > 0 or itemData[LIST_ITEM_TIME] > 0:
							if not itemData in itemDataList:
								itemDataList.append(itemData)
					elif insertType == LIST_ITEM_TOP_SELLING_INDEX:
						if itemData[LIST_ITEM_TOP_SELLING_INDEX] >= 0 and itemData[LIST_ITEM_TOP_SELLING_INDEX] <= 10:
							if not itemData in itemDataList:
								itemDataList.append(itemData)
					elif insertType == LIST_ITEM_ADDED_TIME:
						if itemData[LIST_ITEM_ADDED_TIME] > currentTime-(60*60*24*3):
							if not itemData in itemDataList:
								itemDataList.append(itemData)
					else:
						itemDataList.append(itemData)

		return itemDataList

	def OfferItems(self):
		self.ClearItemList()
		itemDataList = []
		itemDataList = self.InsertItemWithType(itemDataList, LIST_ITEM_DISCOUNT)
		self.RefreshItemList(itemDataList)

	def HotItems(self):
		self.ClearItemList()

		itemDataList = []
		itemDataList = self.InsertItemWithType(itemDataList, LIST_ITEM_TOP_SELLING_INDEX)
		self.RefreshItemList(itemDataList)

	def LoadFirstOpening(self):
		self.categoryIndex = -1
		self.categorySubIndex = -1
		self.ClearItemList()
		itemListBox = self.children["itemListBox"]

		if len(eventCategory) <= 3:
			itemWindowPtr = ItemWindow(itemListBox)
			itemWindowPtr.SetParent(itemListBox)
			itemWindowPtr.SetPosition(0,len(itemListBox.GetItems())*132, True)
			itemWindowPtr.Show()
			for banner in xrange(len(eventCategory)):
				xBanner = ItemShopBanner()
				xBanner.SetParent(itemWindowPtr)
				xBanner.SetPosition(len(itemWindowPtr.children)*250,0)
				xBanner.LoadData(banner)
				xBanner.SAFE_SetStringEvent("MOUSE_OVER_IN",self.OverInBanner,banner, proxy(xBanner))
				xBanner.SAFE_SetStringEvent("MOUSE_OVER_OUT",self.OverOutBanner, banner, proxy(xBanner))
				xBanner.SetEvent(self.ClickBanner, "mouse_click", eventCategory[banner][1][0],eventCategory[banner][1][1])
				xBanner.Show()
				itemWindowPtr.AppendWindow(xBanner)
			if itemWindowPtr != None:
				if len(itemWindowPtr.children) > 0:
					itemListBox.AppendItem(itemWindowPtr)

		itemDataList = []
		itemDataList = self.InsertItemWithType(itemDataList, LIST_ITEM_TOP_SELLING_INDEX)
		itemDataList = self.InsertItemWithType(itemDataList, LIST_ITEM_DISCOUNT)
		itemDataList = self.InsertItemWithType(itemDataList, LIST_ITEM_ADDED_TIME)
		if len(itemDataList) == 0:
			itemDataList = self.InsertItemWithType(itemDataList, 99)
		itemDataList = self.SortList(itemDataList)
		self.RefreshItemList(itemDataList)

	def ClickBanner(self, emptyArg, categoryIndex, categorySubIndex):
		self.categoryIndex = categoryIndex
		self.categorySubIndex = categorySubIndex
		self.RefreshItem()

	def BuyDragonCoin(self):
		self.LoadFirstOpening()
		#os.system("start \"\" "+WEBSITE_LINK)

	def OpenWeb(self, url):
		self.CloseWeb()
		webWnd = WebWindow(self)
		webWnd.Open(url)
		self.children["WebWindow"] = webWnd

	def CloseWeb(self):
		if self.children.has_key("WebWindow"):
			webWnd = self.children["WebWindow"]
			webWnd.Close()
			webWnd.Destroy()
			del self.children["WebWindow"]

	def OverOutBanner(self, index, bannerItem):
		bannerItem.LoadImage(IMG_DIR+eventCategory[index][0][0])
		bannerItem.OnRender(self.children["itemListBox"])

	def OverInBanner(self, index, bannerItem):
		bannerItem.LoadImage(IMG_DIR+eventCategory[index][0][1])
		bannerItem.OnRender(self.children["itemListBox"])

	def PurchaseBtn(self):
		self.OpenPurchasesWindow()

	def SetDragonCoinReal(self, dragonCoinValue):
		text = NumberToMoneyFormat(dragonCoinValue)+"\n|cffF8BE47"+localeinfo.ITEMSHOP_DRAGONCOINT
		self.children["dragonCoinText"].SetText(text)
	
	def SetDragonCoin(self, dragonCoinValue):
		if self.RealMoney==0:
			self.RealMoney = dragonCoinValue
			self.SetDragonCoinReal(self.RealMoney)
		else:
			self.NewMoney = dragonCoinValue
			if self.NewMoney == self.RealMoney:
				self.MoneyFlag = False
			else:
				self.MoneyFlag = True
				self.MoneyRange = self.NewMoney - self.RealMoney
				self.moneySize = 0

	def Open(self):
		if not self.children.has_key("sendPacketData"):
			self.children["sendPacketData"] = 1
			self.children["LoadingImage"].Show()
			self.LoadFirstOpening()
			global UPDATE_TIME
			net.SendChatPacket("/ishop data %d" % UPDATE_TIME)

		self.Show()
		self.SetTop()

	def CloseLoading(self):
		self.children["LoadingImage"].Hide()

	def OpenPurchasesWindow(self):
		self.children["itemListBox"].Hide()
		self.children["itemScrollBar"].Hide()
		self.children["categoryListBox"].Hide()
		self.children["categoryScrollBar"].Hide()

		self.children["PurchasesScrollBar"].Show()
		self.children["PurchasesListBox"].Show()

		if not self.children.has_key("sendPacketDataLog"):
			self.children["sendPacketDataLog"] = 1
			self.children["LoadingImage"].Show()
			net.SendChatPacket("/ishop log")
		else:
			self.RefreshPurchasesWindow()

	def SortPurchasesWindow(self):
		global playerLog
		sortData = []
		for logData in playerLog:
			sortLog = SortClass(logData,True)
			sortData.append(sortLog)
		sortData.sort()
		logDataList = []
		for sortLog in sortData:
			logDataList.append(sortLog.arg)
		return logDataList

	def RefreshPurchasesWindow(self):

		PurchasesListBox = self.children["PurchasesListBox"]
		PurchasesListBox.Show()
		PurchasesListBox.RemoveAllItems()

		global playerLog
		playerLog = self.SortPurchasesWindow()

		logTitle = PurchasesLog(PurchasesListBox)
		logTitle.SetParent(PurchasesListBox)
		logTitle.LoadData(-1)
		logTitle.SetPosition(0,0, True)
		logTitle.Show()
		PurchasesListBox.AppendItem(logTitle)

		for j in xrange(len(playerLog)):
			logItem = PurchasesLog(PurchasesListBox)
			logItem.SetParent(PurchasesListBox)
			logItem.LoadData(j)
			logItem.SetPosition(0,len(PurchasesListBox.GetItems())*35, True)
			logItem.Show()
			PurchasesListBox.AppendItem(logItem)
		PurchasesListBox.Render(0)

	def ClosePurchasesWindow(self):
		self.children["PurchasesScrollBar"].Hide()
		self.children["PurchasesListBox"].Hide()

		self.children["itemListBox"].Show()
		self.children["itemScrollBar"].Show()
		self.children["categoryListBox"].Show()
		self.children["categoryScrollBar"].Show()

	def Close(self):
		self.CloseWeb()

		global buyQuestionDialog
		if buyQuestionDialog != None:
			buyQuestionDialog.Hide()
			buyQuestionDialog.Destroy()
			buyQuestionDialog=None

		self.Hide()
		
		
	def __IsCursorInWindow(self, wnd):
		if wnd == None or not wnd.IsShow():
			return False
		(wx, wy) = wnd.GetGlobalPosition()
		return self.__IsCursorInRect(wx, wy, wnd.GetWidth(), wnd.GetHeight())

	def OnRunMouseWheel(self, nLen):
		if self.children.has_key("PurchasesListBox") and self.children["PurchasesListBox"].IsShow():
			if self.__IsCursorInWindow(self.children["PurchasesListBox"]):
				return self.children["PurchasesListBox"].OnRunMouseWheel(nLen)
			return False

		if self.children.has_key("itemListBox") and self.__IsCursorInWindow(self.children["itemListBox"]):
			return self.children["itemListBox"].OnRunMouseWheel(nLen)

		if self.children.has_key("categoryListBox") and self.__IsCursorInWindow(self.children["categoryListBox"]):
			return self.children["categoryListBox"].OnRunMouseWheel(nLen)

		return False

	def OnMouseWheel(self, nLen):
		if not self.__IsCursorInWindow(self):
			return False
		return self.OnRunMouseWheel(nLen)
		
		
	def OnPressEscapeKey(self):
		self.Close()
		return True
	def __ClickRadioButton(self, buttonList, buttonIndex):
		try:
			btn=buttonList[buttonIndex]
		except IndexError:
			return
		for eachButton in buttonList:
			eachButton.SetUp()
		btn.Down()

class PurchasesLog(ui.ExpandedImageBox):
	def __del__(self):
		ui.ExpandedImageBox.__del__(self)
	def Destroy(self):
		self.childrenImages = {}
		self.childrenTextLine = {}
		self.parent = None
	def __init__(self, parent):
		ui.ExpandedImageBox.__init__(self)
		self.Destroy()
		self.parent = proxy(parent)
	def LoadData(self, index):
		if index >= 0:
			self.childrenImages["self"] = self
			self.LoadImage(IMG_DIR+"purch_img.tga")

		try:
			logData = playerLog[index]
		except IndexError:
			return

		if index >= 0:
			dateText = logData[LOG_DATE_TEXT]
			accountText = logData[LOG_ACCOUNT]
			ipText = logData[LOG_IP]
			item.SelectItem(logData[LOG_ITEM])
			if logData[LOG_COUNT] > 1:
				itemText = "|cffF7BD48%sx%d|r" % (item.GetItemName(),logData[LOG_COUNT])
			else:
				itemText = "|cffF7BD48%s|r" % item.GetItemName()
			priceText = localeinfo.PURCHASES_PRICE_TEXT % NumberToMoneyFormat(logData[LOG_PRICE])
		else:
			dateText = localeinfo.PURCHASES_DATE
			accountText = localeinfo.PURCHASES_ACCOUNTID
			ipText = localeinfo.PURCHASES_IP
			itemText = localeinfo.PURCHASES_ITEMBOUGHT
			priceText = localeinfo.PURCHASES_PRICE

		textList = [
			[75,5,dateText],
			[260,5,accountText],
			[400,5,ipText],
			[600,5,itemText],
			[800,5,priceText],
		]
		for j in xrange(len(textList)):
			text = textList[j]
			logText = ui.TextLine()
			logText.SetParent(self)
			logText.SetFontName("Tahoma:13")
			logText.SetText(text[2])
			logText.SetHorizontalAlignCenter()
			if index >= 0:
				logText.SetPosition(text[0],text[1])
			else:
				logText.SetPosition(text[0],0)
			logText.Show()
			self.childrenTextLine["text%d"%j] = logText

	def OnRender(self):
		listboxHeight = self.parent.GetHeight()
		(pgx,pgy) = self.parent.GetGlobalPosition()
		for key, textline in self.childrenTextLine.items():
			(igx,igy) = textline.GetGlobalPosition()
			if igy < pgy:
				textline.Hide()
			elif igy > (pgy+listboxHeight-25):
				textline.Hide()
			else:
				textline.Show()
		for key, image in self.childrenImages.items():
			(jgx,jgy) = image.GetGlobalPosition()
			imageHeight = image.GetHeight()
			if jgy < pgy:
				image.SetRenderingRect(0,ui.calculateRect(imageHeight-abs(jgy-pgy),imageHeight),0,0)
			elif jgy+imageHeight > (pgy+listboxHeight-8):
				calculate = (pgy+listboxHeight-8) - (jgy+imageHeight)
				if calculate == 0:
					return
				image.SetRenderingRect(0.0,0.0,0.0,ui.calculateRect(imageHeight-abs(calculate),imageHeight))
			else:
				image.SetRenderingRect(0,0,0,0)

class ItemShopBoard(ui.Window):
	CORNER_WIDTH = 7
	CORNER_HEIGHT = 7
	LINE_WIDTH = 7
	LINE_HEIGHT = 7
	LT = 0
	LB = 1
	RT = 2
	RB = 3
	L = 0
	R = 1
	T = 2
	B = 3
	def __init__(self):
		ui.Window.__init__(self)
		self.MakeBoard()
		self.MakeBase()
	def MakeBoard(self):
		cornerPath = IMG_DIR+"board/corner_"
		linePath = IMG_DIR+"board/"
		CornerFileNames = [ cornerPath+dir+".tga" for dir in ("left_top", "left_bottom", "right_top", "right_bottom") ]
		LineFileNames = [ linePath+dir+".tga" for dir in ("left", "right", "top", "bottom") ]
		self.Corners = []
		for fileName in CornerFileNames:
			Corner = ui.ExpandedImageBox()
			Corner.AddFlag("not_pick")
			Corner.LoadImage(fileName)
			Corner.SetParent(self)
			Corner.SetPosition(0, 0)
			Corner.Show()
			self.Corners.append(Corner)
		self.Lines = []
		for fileName in LineFileNames:
			Line = ui.ExpandedImageBox()
			Line.AddFlag("not_pick")
			Line.LoadImage(fileName)
			Line.SetParent(self)
			Line.SetPosition(0, 0)
			Line.Show()
			self.Lines.append(Line)
		self.Lines[self.L].SetPosition(0, self.CORNER_HEIGHT)
		self.Lines[self.T].SetPosition(self.CORNER_WIDTH, 0)
	def MakeBase(self):
		self.Base = ui.ExpandedImageBox()
		self.Base.AddFlag("not_pick")
		self.Base.LoadImage(IMG_DIR+"board/base.tga")
		self.Base.SetParent(self)
		self.Base.SetPosition(self.CORNER_WIDTH, self.CORNER_HEIGHT)
		self.Base.Show()
	def __del__(self):
		ui.Window.__del__(self)
	def Destroy(self):
		self.Base=0
		self.Corners=0
		self.Lines=0
		self.CORNER_WIDTH = 0
		self.CORNER_HEIGHT = 0
		self.LINE_WIDTH = 0
		self.LINE_HEIGHT = 0
		self.LT = 0
		self.LB = 0
		self.RT = 0
		self.RB = 0
		self.L = 0
		self.R = 0
		self.T = 0
		self.B = 0
	def SetSize(self, width, height):
		width = max(self.CORNER_WIDTH*2, width)
		height = max(self.CORNER_HEIGHT*2, height)
		ui.Window.SetSize(self, width, height)
		self.Corners[self.LB].SetPosition(0, height - self.CORNER_HEIGHT)
		self.Corners[self.RT].SetPosition(width - self.CORNER_WIDTH, 0)
		self.Corners[self.RB].SetPosition(width - self.CORNER_WIDTH, height - self.CORNER_HEIGHT)
		self.Lines[self.R].SetPosition(width - self.CORNER_WIDTH, self.CORNER_HEIGHT)
		self.Lines[self.B].SetPosition(self.CORNER_HEIGHT, height - self.CORNER_HEIGHT)
		verticalShowingPercentage = float((height - self.CORNER_HEIGHT*2) - self.LINE_HEIGHT) / self.LINE_HEIGHT
		horizontalShowingPercentage = float((width - self.CORNER_WIDTH*2) - self.LINE_WIDTH) / self.LINE_WIDTH
		self.Lines[self.L].SetRenderingRect(0, 0, 0, verticalShowingPercentage)
		self.Lines[self.R].SetRenderingRect(0, 0, 0, verticalShowingPercentage)
		self.Lines[self.T].SetRenderingRect(0, 0, horizontalShowingPercentage, 0)
		self.Lines[self.B].SetRenderingRect(0, 0, horizontalShowingPercentage, 0)
		if self.Base:
			self.Base.SetRenderingRect(0, 0, horizontalShowingPercentage, verticalShowingPercentage)

class WebWindow(ItemShopBoard):
	def __del__(self):
		ItemShopBoard.__del__(self)
	def Destroy(self):
		self.oldPos=(0,0)
		self.closeBtn=None
		self.parent = None
	def __init__(self, parent):
		ItemShopBoard.__init__(self)
		self.Destroy()
		self.parent = proxy(parent)
		self.LoadWindow()
	def LoadWindow(self):
		self.AddFlag("attach")
		self.AddFlag("float")
		#self.AddFlag("movable")
		self.SetSize(450*2,450)
		closeBtn = ui.Button()
		closeBtn.SetParent(self)
		closeBtn.SetUpVisual(IMG_DIR+"close_0.tga")
		closeBtn.SetOverVisual(IMG_DIR+"close_1.tga")
		closeBtn.SetDownVisual(IMG_DIR+"close_2.tga")
		closeBtn.SetPosition(self.GetWidth()-closeBtn.GetWidth(),0)
		closeBtn.SetEvent(self.parent.CloseWeb)
		closeBtn.Show()
		self.closeBtn=closeBtn

	def Close(self):
		app.HideWebPage()
		self.Hide()

	def Open(self, url):
		self.SetSize(450*2,450)
		self.Show()
		self.SetTop()
		self.SetCenterPosition()

		x, y = self.GetGlobalPosition()
		sx, sy = x + 10, y + 30
		ex, ey = sx + self.GetWidth() - 20, sy + self.GetHeight() - 40
		app.ShowWebPage(url, (sx, sy, ex, ey))

	def OnUpdate(self):
		newPos = self.GetGlobalPosition()
		if newPos == self.oldPos:
			return
		self.oldPos = newPos
		x, y = newPos
		sx, sy = x + 10, y + 30
		ex, ey = sx + self.GetWidth() - 20, sy + self.GetHeight() - 40
		app.MoveWebPage((sx, sy, ex, ey))

class BuyWindow(ItemShopBoard):
	def __del__(self):
		ItemShopBoard.__del__(self)
	def __init__(self):
		ItemShopBoard.__init__(self)
		self.AddFlag("attach")
		self.AddFlag("float")
		self.Destroy()
		buyText = MultiTextLine()
		buyText.SetParent(self)
		buyText.SetTextRange(25)
		buyText.SetPosition(self.GetWidth()/2,28)
		buyText.SetTextType("horizontal#center")
		buyText.SetFontName("Tahoma:16")
		buyText.Show()
		self.children["buyText"] = buyText
		yesBtn = ui.Button()
		yesBtn.SetParent(self)
		yesBtn.SetUpVisual(IMG_DIR+"buy_0.tga")
		yesBtn.SetOverVisual(IMG_DIR+"buy_1.tga")
		yesBtn.SetDownVisual(IMG_DIR+"buy_2.tga")
		yesBtn.SetDownVisual(IMG_DIR+"buy_2.tga")
		yesBtn.SetText("YES")
		yesBtn.Show()
		self.children["yesBtn"] = yesBtn
		cancelBtn = ui.Button()
		cancelBtn.SetParent(self)
		cancelBtn.SetUpVisual(IMG_DIR+"buy_0.tga")
		cancelBtn.SetOverVisual(IMG_DIR+"buy_1.tga")
		cancelBtn.SetDownVisual(IMG_DIR+"buy_2.tga")
		cancelBtn.SetText(localeinfo.UI_CANCEL)
		cancelBtn.Show()
		self.children["cancelBtn"] = cancelBtn
	def Destroy(self):
		self.children={}
	def SetText(self, text):
		buyText = self.children["buyText"]
		buyText.SetText(text)
		(x, y) = buyText.GetTextSize()
		(minWidth,minHeight) = (256,142)
		if x+40 > minWidth:
			minWidth = x+40
		if not ((minWidth%2) == 0):
			minWidth+=1
		self.SetSize(minWidth,minHeight)
		buyText.SetPosition(self.GetWidth()/2,28)
		self.children["yesBtn"].SetPosition((self.GetWidth()/2)+15,94)
		self.children["cancelBtn"].SetPosition((self.GetWidth()/2)-self.children["cancelBtn"].GetWidth()-15,94)
	def SetCancelEvent(self, func):
		self.children["cancelBtn"].SetEvent(func)
	def SetAcceptEvent(self, func):
		self.children["yesBtn"].SetEvent(func)
	def OnPressEscapeKey(self):
		self.children["cancelBtn"].CallEvent()
		return True

class SortClass:
	def __init__(self, arg, isLog = False):
		self.arg = arg
		self.isLog = isLog
	def __del__(self):
		self.arg = []
	def __lt__(self, other):
		if self.isLog:
			return self.arg[LOG_DATE] >= other.arg[LOG_DATE]
		else:
			currentTime = app.GetGlobalTimeStamp()
			# hot item
			if self.arg[LIST_ITEM_TOP_SELLING_INDEX] <= 10 or other.arg[LIST_ITEM_TOP_SELLING_INDEX] <= 10:
				return self.arg[LIST_ITEM_TOP_SELLING_INDEX] <= other.arg[LIST_ITEM_TOP_SELLING_INDEX]
			# time offer
			if self.arg[LIST_ITEM_TIME] != 0 or other.arg[LIST_ITEM_TIME] != 0:
				if self.arg[LIST_ITEM_TIME] > 0 and other.arg[LIST_ITEM_TIME] > 0:
					return ((self.arg[LIST_ITEM_TIME]-currentTime) <= (other.arg[LIST_ITEM_TIME]-currentTime))
				else:
					return self.arg[LIST_ITEM_TIME] >= other.arg[LIST_ITEM_TIME]
			# discount
			if self.arg[LIST_ITEM_DISCOUNT] != 0 or other.arg[LIST_ITEM_DISCOUNT] != 0:
				return self.arg[LIST_ITEM_DISCOUNT] >= other.arg[LIST_ITEM_DISCOUNT]
			# added time
			if self.arg[LIST_ITEM_ADDED_TIME] > currentTime-(60*60*24*3) or other.arg[LIST_ITEM_ADDED_TIME] > currentTime-(60*60*24*3):
				return self.arg[LIST_ITEM_ADDED_TIME] >= other.arg[LIST_ITEM_ADDED_TIME]
			return self.arg[LIST_ITEM_ID] >= other.arg[LIST_ITEM_ID]

class ItemShopBanner(ui.ExpandedImageBox):
	def __del__(self):
		ui.ExpandedImageBox.__del__(self)
	def Destroy(self):
		self.bannerIndex = -1
		self.childrenImages = {}
	def __init__(self):
		ui.ExpandedImageBox.__init__(self)
		self.Destroy()
	def LoadData(self, bannerIndex):
		self.bannerIndex=bannerIndex
		if bannerIndex >= len(eventCategory):
			return
		self.LoadImage(IMG_DIR+eventCategory[bannerIndex][0][0])
		self.childrenImages["self"] = self
	def OnUpdate(self):
		pass
	def OnRender(self, parent):
		listboxHeight = parent.GetHeight()
		(pgx,pgy) = parent.GetGlobalPosition()
		for key, image in self.childrenImages.items():
			(jgx,jgy) = image.GetGlobalPosition()
			imageHeight = image.GetHeight()
			if jgy < pgy:
				image.SetRenderingRect(0,ui.calculateRect(imageHeight-abs(jgy-pgy),imageHeight),0,0)
			elif jgy+imageHeight > (pgy+listboxHeight-8):
				calculate = (pgy+listboxHeight-8) - (jgy+imageHeight)
				if calculate == 0:
					return
				image.SetRenderingRect(0.0,0.0,0.0,ui.calculateRect(imageHeight-abs(calculate),imageHeight))
			else:
				image.SetRenderingRect(0,0,0,0)

class ItemShopItem(ui.ExpandedImageBox):
	def __del__(self):
		ui.ExpandedImageBox.__del__(self)
	def __init__(self):
		ui.ExpandedImageBox.__init__(self)
		self.Destroy()
		self.buyCount=1
		self.LoadImage(IMG_DIR+"item_box.tga")
		self.childrenImages["self"] = self
	def Destroy(self):
		self.data = []
		self.buyCount=0
		self.butButtonSet = False
		self.childrenImages = {}
		self.childrenTextLine = {}

	def LoadData(self, itemData):
		self.data = itemData
		self.SetItem(itemData[LIST_ITEM_VNUM])
		self.SetCount()
		self.SetItemName()
		self.SetBuyButton(itemData[LIST_ITEM_TIME])
		self.SetPrice(itemData[LIST_ITEM_PRICE],itemData[LIST_ITEM_DISCOUNT])
		self.SetTime(itemData[LIST_ITEM_TIME])

		if itemData[LIST_ITEM_TOP_SELLING_INDEX] >= 0 and itemData[LIST_ITEM_TOP_SELLING_INDEX] <= 10:
			self.SetItemType(1)
		elif itemData[LIST_ITEM_TIME] > 0 or itemData[LIST_ITEM_DISCOUNT] > 0:
			self.SetItemType(3)
		elif itemData[LIST_ITEM_ADDED_TIME] > app.GetGlobalTimeStamp()-(60*60*24*3):
			self.SetItemType(2)

	def ClearChildren(self, childrenName):
		if self.childrenTextLine.has_key(childrenName):
			self.childrenTextLine[childrenName].Hide()
			self.childrenTextLine[childrenName].Destroy()
			del self.childrenTextLine[childrenName]
		if self.childrenImages.has_key(childrenName):
			self.childrenImages[childrenName].Hide()
			self.childrenImages[childrenName].Destroy()
			del self.childrenImages[childrenName]

	def FormatTime(self, seconds):
		#if seconds <= 0:
		#	if self.butButtonSet == False:
		#		self.SetBuyButton(self.data[LIST_ITEM_TIME])
		#	return "|cffFF7B7B0h 0m 0s"
		#m, s = divmod(seconds, 60)
		#h, m = divmod(m, 60)
		#d, h = divmod(h, 24)
		#if d > 0:
		#	return "|cffFF7B7B%dd %dh %dm %ds" % (d, h, m, s)
		#return "|cffFF7B7B%dh %dm %ds" % (h, m, s)
		return False

	def OnUpdate(self):
		if self.childrenTextLine.has_key("timeText"):
			self.childrenTextLine["timeText"].SetText(self.FormatTime(self.data[LIST_ITEM_TIME]-app.GetGlobalTimeStamp()))

	def SetTime(self, time):
		self.ClearChildren("timeText")
		if time != 0:
			self.butButtonSet = False
			
			timeText = ui.TextLine()
			timeText.SetParent(self)
			timeText.SetPosition(168,65)
			timeText.SetWindowHorizontalAlignLeft()
			timeText.SetFontName("Tahoma:13")
			timeText.Show()
			self.childrenTextLine["timeText"] = timeText

	def SetBuyButton(self, time):
		self.ClearChildren("buyButton")
		self.butButtonSet = True
		buyButton = ui.ExpandedImageBox()
		buyButton.SetParent(self)
		# if time != 0 and app.GetGlobalTimeStamp() > time:
			# buyButton.LoadImage(IMG_DIR+"button_buy_disabled.tga")
			# self.butButtonSet = True
		# else:
		buyButton.LoadImage(IMG_DIR+"buy_0.tga")
		buyButton.SAFE_SetStringEvent("MOUSE_OVER_IN",self.OverInBuyBtn)
		buyButton.SAFE_SetStringEvent("MOUSE_OVER_OUT",self.OverOutBuyBtn)
		buyButton.SetEvent(ui.__mem_func__(self.ClickBuyBtn),"mouse_click")
		buyButton.SetPosition(168,82)
		buyButton.Show()
		self.childrenImages["buyButton"] = buyButton

		buyBtnText = ui.TextLine()
		buyBtnText.SetParent(buyButton)
		buyBtnText.SetHorizontalAlignCenter()
		buyBtnText.SetVerticalAlignCenter()
		buyBtnText.SetWindowHorizontalAlignCenter()
		buyBtnText.SetWindowVerticalAlignCenter()
		buyBtnText.SetText(localeinfo.ITEMSHOP_BUY)
		buyBtnText.Show()
		self.childrenTextLine["buyBtnText"] = buyBtnText

	def ClickBuyBtn(self, emptyArg):
		#time = self.data[LIST_ITEM_TIME]
		# if time != 0 and app.GetGlobalTimeStamp() > time:
			# return
		
		global buyQuestionDialog
		if buyQuestionDialog == None:
			buyQuestionDialog = BuyWindow()

		text = ""
		
		price = self.data[LIST_ITEM_PRICE]*self.buyCount
		discount = self.data[LIST_ITEM_DISCOUNT]
		if discount > 0:
			price = int((float(price)/100.0)*float(100-discount))

		item.SelectItem(self.data[LIST_ITEM_VNUM])
		if self.buyCount > 1:
			text+=localeinfo.ITEMSHOP_BUY_TEXT_COUNT % (item.GetItemName(),self.buyCount)
		else:
			text+=localeinfo.ITEMSHOP_BUY_TEXT % item.GetItemName()

		text+="\n"
		text+=localeinfo.ITEMSHOP_PRICE % price

		buyQuestionDialog.SetText(text)
		buyQuestionDialog.SetAcceptEvent(self.AcceptBuy)
		buyQuestionDialog.SetCancelEvent(self.CloseQuestionDialog)
		buyQuestionDialog.SetCenterPosition()
		buyQuestionDialog.SetTop()
		buyQuestionDialog.Show()

	def CloseQuestionDialog(self):
		global buyQuestionDialog
		if buyQuestionDialog!= None:
			buyQuestionDialog.Hide()
			buyQuestionDialog.Destroy()
			buyQuestionDialog=None

	def AcceptBuy(self):
		net.SendChatPacket("/ishop buy %d %d"%(self.data[LIST_ITEM_ID],self.buyCount))
		self.CloseQuestionDialog()

	def EmptyFunction(self):
		pass

	def OverOutBuyBtn(self):
		if self.childrenImages.has_key("buyButton"):
			self.childrenImages["buyButton"].LoadImage(IMG_DIR+"buy_0.tga")

	def OverInBuyBtn(self):
		if self.childrenImages.has_key("buyButton"):
			self.childrenImages["buyButton"].LoadImage(IMG_DIR+"buy_1.tga")

	def SetPrice(self, price, discount):
		self.ClearChildren("lineImage")
		self.ClearChildren("itemPrice")

		itemPrice = MultiTextLine()
		itemPrice.SetParent(self)
		itemPrice.SetTextRange(20)
		itemPrice.SetPosition(95, 35)
		itemPrice.SetTextType("horizontal#left")
		itemPrice.SetFontName("Tahoma:13")

		price = price*self.buyCount

		text= ""
		if discount > 0:
			newPrice = int((float(price)/100.0)*float(100-discount))

			text = localeinfo.ITEMSHOP_PRICE_ONLY_TEXT % NumberToMoneyFormat(price)
			itemPriceEx = ui.TextLine()
			itemPriceEx.SetParent(self)
			itemPriceEx.SetText(text)
			(xSize,ySize) = itemPriceEx.GetTextSize()

			text = localeinfo.ITEMSHOP_PRICE_WITH_DISCOUNT % (NumberToMoneyFormat(price),discount)
			text+="\n"
			text+="     "
			text+=localeinfo.ITEMSHOP_PRICE_ONLY_TEXT % NumberToMoneyFormat(newPrice)

			lineImage = ui.ExpandedImageBox()
			lineImage.SetParent(itemPrice)
			lineImage.LoadImage(IMG_DIR+"line.tga")
			lineImage.SetDiffuseColor(1.0,0.0,0.0,0.9)
			scaleCalc = float(xSize)*(1.0/10.0)
			lineImage.SetScale(scaleCalc, 1.0)
			lineImage.SetPosition(17,(ySize/2)+1)
			lineImage.Show()
			self.childrenImages["lineImage"] = lineImage

			self.SetItemType(3)
		else:
			text = localeinfo.ITEMSHOP_PRICE % NumberToMoneyFormat(price)

		itemPrice.SetText(text)
		itemPrice.Show()
		self.childrenTextLine["itemPrice"] = itemPrice

	def SetItemName(self):
		self.ClearChildren("itemName")
		itemName = ui.TextLine()
		itemName.SetParent(self)
		itemName.SetWindowHorizontalAlignLeft()
		itemName.SetFontName("Tahoma:13")
		itemName.SetPosition(95,15)
		itemName.SetText("|cffF7BD48"+item.GetItemName())
		#itemName.SetFontColor(247,189,72)
		itemName.Show()
		self.childrenTextLine["itemName"] = itemName

	def SetCount(self):
		self.ClearChildren("CountBg")
		self.ClearChildren("CountText")
		self.ClearChildren("CountLeft")
		self.ClearChildren("CountRight")
		if not item.IsFlag(item.ITEM_FLAG_STACKABLE):
			return

		CountLeft = ui.ExpandedImageBox()
		CountLeft.SetParent(self)
		CountLeft.LoadImage(IMG_DIR+"count_left.tga")
		CountLeft.SetPosition(70,85)
		CountLeft.SetEvent(ui.__mem_func__(self.ClickCount),"mouse_click",0)
		CountLeft.Show()
		self.childrenImages["CountLeft"] = CountLeft

		CountBg = ui.ExpandedImageBox()
		CountBg.SetParent(self)
		CountBg.LoadImage(IMG_DIR+"count_back.tga")
		CountBg.SetPosition(70+CountLeft.GetWidth(),85)
		CountBg.Show()
		self.childrenImages["CountBg"] = CountBg

		CountText = ui.TextLine()
		CountText.SetParent(CountBg)
		CountText.SetHorizontalAlignCenter()
		CountText.SetVerticalAlignCenter()
		CountText.SetWindowHorizontalAlignCenter()
		CountText.SetWindowVerticalAlignCenter()
		CountText.SetText("1")
		CountText.Show()
		self.childrenTextLine["CountText"] = CountText

		CountRight = ui.ExpandedImageBox()
		CountRight.SetParent(self)
		CountRight.LoadImage(IMG_DIR+"count_right.tga")
		CountRight.SetPosition(70+CountLeft.GetWidth()+CountBg.GetWidth(),85)
		CountRight.SetEvent(ui.__mem_func__(self.ClickCount),"mouse_click",1)
		CountRight.Show()
		self.childrenImages["CountRight"] = CountRight

	def ClickCount(self,emptyArg, btnType):
		value = 1
		if app.IsPressed(app.DIK_LCONTROL):
			value += 4
		buyCount = self.buyCount
		if btnType == 0:
			buyCount -= value
			if buyCount <= 0:
				buyCount = 1
		else:
			buyCount += value
			if buyCount <= 0:
				buyCount = 1
			elif buyCount > 20:
				buyCount = 20
		self.buyCount = buyCount

		if self.childrenTextLine.has_key("CountText"):
			self.childrenTextLine["CountText"].SetText(str(self.buyCount))

		itemData = self.data
		self.SetPrice(itemData[LIST_ITEM_PRICE],itemData[LIST_ITEM_DISCOUNT])

	def SetItem(self, itemVnum):
		self.ClearChildren("itemImage")
		self.ClearChildren("itemCount")
		item.SelectItem(itemVnum)
		(width,height) = item.GetItemSize()
		(xPos, yPos) = (36,46)
		if height == 1:
			xPos = 30
		elif height == 2:
			yPos -= 20
		elif height == 3:
			yPos -= 30
			xPos -= 5
		itemImage = ui.ExpandedImageBox()
		itemImage.SetParent(self)
		itemImage.LoadImage(item.GetIconImageFileName())
		itemImage.SetPosition(xPos,yPos)
		itemImage.SetStringEvent("MOUSE_OVER_IN",self.OverInItem,itemVnum)
		itemImage.SetStringEvent("MOUSE_OVER_OUT",self.OverOutItem)

		itemImage.Show()
		self.childrenImages["itemImage"] = itemImage
		# import dbg
		# dbg.TraceError("def SetItem meghívva: VNUM: %d" % itemVnum)

	def OverInItem(self, itemVnum):
		interface = constinfo.GetInterfaceInstance()
		# import dbg
		# dbg.TraceError("def OverInItem meghívva: VNUM = %d" % itemVnum)

		if interface:
			if interface.tooltipItem:
				interface.tooltipItem.SetItemToolTip(itemVnum)

	def OverOutItem(self):
		interface = constinfo.GetInterfaceInstance()
		if interface:
			if interface.tooltipItem:
				interface.tooltipItem.HideToolTip()
	def SetItemType(self, itemType):
		self.ClearChildren("itemType")
		typeImages = ["hot.tga","new.tga","offer.tga"]
		if itemType == 0:
			return
		itemType-=1
		if itemType >= len(typeImages):
			return
		typeImage = ui.ExpandedImageBox()
		typeImage.SetParent(self)
		typeImage.LoadImage(IMG_DIR+typeImages[itemType])
		typeImage.SetPosition(0,0)
		typeImage.Show()
		self.childrenImages["itemType"] = typeImage

	def OnRender(self, parent):
		listboxHeight = parent.GetHeight()
		(pgx,pgy) = parent.GetGlobalPosition()
		for key, textline in self.childrenTextLine.items():
			(igx,igy) = textline.GetGlobalPosition()
			if igy < pgy:
				textline.Hide()
			elif igy > (pgy+listboxHeight-25):
				textline.Hide()
			else:
				textline.Show()
		for key, image in self.childrenImages.items():
			(jgx,jgy) = image.GetGlobalPosition()
			imageHeight = image.GetHeight()
			if jgy < pgy:
				image.SetRenderingRect(0,ui.calculateRect(imageHeight-abs(jgy-pgy),imageHeight),0,0)
			elif jgy+imageHeight > (pgy+listboxHeight-8):
				calculate = (pgy+listboxHeight-8) - (jgy+imageHeight)
				if calculate == 0:
					return
				image.SetRenderingRect(0.0,0.0,0.0,ui.calculateRect(imageHeight-abs(calculate),imageHeight))
			else:
				image.SetRenderingRect(0,0,0,0)

class ItemWindow(ui.Window):
	def __del__(self):
		ui.Window.__del__(self)
	def __init__(self, parent):
		ui.Window.__init__(self)
		self.children=[]
		self.Destroy()
		self.parent=proxy(parent)
		self.SetSize(750,122)
	def OnUpdate(self):
		for children in self.children:
			children.OnUpdate()
	def Destroy(self):
		for children in self.children:
			children.Destroy()
			children=None
		self.children=[]
		self.parent = None
	def AppendWindow(self, window):
		self.children.append(window)
		self.OnRender()
	def OnRender(self):
		for children in self.children:
			children.OnRender(self.parent)
class CategoryItem(ui.ExpandedImageBox):
	def __del__(self):
		ui.ExpandedImageBox.__del__(self)
	def __init__(self):
		ui.ExpandedImageBox.__init__(self)
		self.Destroy()
		self.categoryIndex=-1
		self.LoadImage(IMG_DIR+"category_no_selected.tga")
	def Destroy(self):
		self.parent=None
		self.categoryIndex=0
		self.categoryText=None
	def SetData(self, categoryIndex, categoryName, categoryListBox):
		self.categoryIndex=categoryIndex
		self.parent=proxy(categoryListBox)
		categoryText = MultiTextLine()
		categoryText.SetParent(self)
		categoryText.SetPosition(self.GetWidth()/2,11)
		categoryText.SetTextRange(15)
		categoryText.SetTextType("horizontal#center")
		categoryText.SetFontName("Tahoma:16")
		categoryText.SetText(categoryName)
		categoryText.Show()
		self.categoryText = categoryText
	def OnRender(self):
		parent = self.parent
		if parent == None:
			return
		listboxHeight = parent.GetHeight()
		(pgx,pgy) = parent.GetGlobalPosition()
		(igx,igy) = self.categoryText.GetGlobalPosition()
		if igy < pgy:
			self.categoryText.Hide()
		elif igy > (pgy+listboxHeight-25):
			self.categoryText.Hide()
		else:
			self.categoryText.Show()
		(jgx,jgy) = self.GetGlobalPosition()
		imageHeight = self.GetHeight()
		if jgy < pgy:
			self.SetRenderingRect(0,ui.calculateRect(imageHeight-abs(jgy-pgy),imageHeight),0,0)
		elif jgy+imageHeight > (pgy+listboxHeight-4):
			calculate = (pgy+listboxHeight-4) - (jgy+imageHeight)
			if calculate == 0:
				return
			self.SetRenderingRect(0.0,0.0,0.0,ui.calculateRect(imageHeight-abs(calculate),imageHeight))
		else:
			self.SetRenderingRect(0,0,0,0)
class ScrollBar(ui.Window):
	SCROLLBAR_WIDTH = 13
	SCROLLBAR_MIDDLE_HEIGHT = 1
	SCROLLBAR_BUTTON_WIDTH = 17
	SCROLLBAR_BUTTON_HEIGHT = 17
	SCROLL_BTN_XDIST = 2
	SCROLL_BTN_YDIST = 2
	class MiddleBar(ui.DragButton):
		def __init__(self):
			ui.DragButton.__init__(self)
			self.AddFlag("movable")
			self.SetWindowName("scrollbar_middlebar")
		def MakeImage(self):
			top = ui.ExpandedImageBox()
			top.SetParent(self)
			top.LoadImage(IMG_DIR+"scroll_top.tga")
			top.AddFlag("not_pick")
			top.Show()
			bottom = ui.ExpandedImageBox()
			bottom.SetParent(self)
			bottom.LoadImage(IMG_DIR+"scroll_bottom.tga")
			bottom.AddFlag("not_pick")
			bottom.Show()
			middle = ui.ExpandedImageBox()
			middle.SetParent(self)
			middle.LoadImage(IMG_DIR+"scroll_mid.tga")
			middle.AddFlag("not_pick")
			middle.Show()
			self.top = top
			self.bottom = bottom
			self.middle = middle
		def SetSize(self, height):
			minHeight = self.top.GetHeight() + self.bottom.GetHeight() + self.middle.GetHeight()
			height = max(minHeight, height)
			ui.DragButton.SetSize(self, 10, height)
			scale = (height - minHeight) / 2 
			extraScale = 0
			if (height - minHeight) % 2 == 1:
				extraScale = 1
			self.middle.SetPosition(0, self.top.GetHeight() + scale)
			self.bottom.SetPosition(0, height - self.bottom.GetHeight())

	def __init__(self):
		ui.Window.__init__(self)

		self.pageSize = 1
		self.curPos = 0.0
		self.eventScroll = None
		self.eventArgs = None
		self.lockFlag = False

		self.CreateScrollBar()
		self.SetScrollBarSize(0)

		self.scrollStep = 0.03
		self.SetWindowName("NONAME_ScrollBar")

	def __del__(self):
		ui.Window.__del__(self)

	def CreateScrollBar(self):
		topImage = ui.ExpandedImageBox()
		topImage.SetParent(self)
		topImage.AddFlag("not_pick")
		topImage.LoadImage(IMG_DIR+"scrollbar_top.tga")
		topImage.Show()
		bottomImage = ui.ExpandedImageBox()
		bottomImage.SetParent(self)
		bottomImage.AddFlag("not_pick")
		bottomImage.LoadImage(IMG_DIR+"scrollbar_bottom.tga")
		bottomImage.Show()
		middleImage = ui.ExpandedImageBox()
		middleImage.SetParent(self)
		middleImage.AddFlag("not_pick")
		middleImage.SetPosition(0, topImage.GetHeight())
		middleImage.LoadImage(IMG_DIR+"scrollbar_mid.tga")
		middleImage.Show()
		self.topImage = topImage
		self.bottomImage = bottomImage
		self.middleImage = middleImage

		middleBar = self.MiddleBar()
		middleBar.SetParent(self)
		middleBar.SetMoveEvent(ui.__mem_func__(self.OnMove))
		middleBar.Show()
		middleBar.MakeImage()
		middleBar.SetSize(0) # set min height
		self.middleBar = middleBar
	def Destroy(self):
		self.eventScroll = None
		self.eventArgs = None
	def SetScrollEvent(self, event, *args):
		self.eventScroll = event
		self.eventArgs = args
	def SetMiddleBarSize(self, pageScale):
		self.middleBar.SetSize(int(pageScale * float(self.GetHeight() - self.SCROLL_BTN_YDIST*2)))
		realHeight = self.GetHeight() - self.SCROLL_BTN_YDIST*2 - self.middleBar.GetHeight()
		self.pageSize = realHeight
	def SetScrollBarSize(self, height):
		self.SetSize(self.SCROLLBAR_WIDTH, height)
		self.pageSize = height - self.SCROLL_BTN_YDIST*2 - self.middleBar.GetHeight()
		middleImageScale = float((height - self.SCROLL_BTN_YDIST*2) - self.middleImage.GetHeight()) / float(self.middleImage.GetHeight())
		self.middleImage.SetRenderingRect(0, 0, 0, middleImageScale)
		self.bottomImage.SetPosition(0, height - self.bottomImage.GetHeight())
		self.middleBar.SetRestrictMovementArea(self.SCROLL_BTN_XDIST, self.SCROLL_BTN_YDIST, self.middleBar.GetWidth(), height - self.SCROLL_BTN_YDIST * 2)
		self.middleBar.SetPosition(self.SCROLL_BTN_XDIST, self.SCROLL_BTN_YDIST)
	def SetScrollStep(self, step):
		self.scrollStep = step
	def GetScrollStep(self):
		return self.scrollStep
	def GetPos(self):
		return self.curPos
	def OnUp(self):
		self.SetPos(self.curPos-self.scrollStep)
	def OnDown(self):
		self.SetPos(self.curPos+self.scrollStep)
	def SetPos(self, pos, moveEvent = True):
		pos = max(0.0, pos)
		pos = min(1.0, pos)
		newPos = float(self.pageSize) * pos
		self.middleBar.SetPosition(self.SCROLL_BTN_XDIST, int(newPos) + self.SCROLL_BTN_YDIST)
		if moveEvent == True:
			self.OnMove()
	def OnMove(self):
		if self.lockFlag:
			return
		if 0 == self.pageSize:
			return
		(xLocal, yLocal) = self.middleBar.GetLocalPosition()
		self.curPos = float(yLocal - self.SCROLL_BTN_YDIST) / float(self.pageSize)
		if self.eventScroll:
			apply(self.eventScroll, self.eventArgs)
	def OnMouseLeftButtonDown(self):
		(xMouseLocalPosition, yMouseLocalPosition) = self.GetMouseLocalPosition()
		newPos = float(yMouseLocalPosition) / float(self.GetHeight())
		self.SetPos(newPos)
	def LockScroll(self):
		self.lockFlag = True
	def UnlockScroll(self):
		self.lockFlag = False
#class ScrollBar(ui.Window):
#	SCROLLBAR_WIDTH = 12
#	MIDDLE_BAR_POS = 5
#	MIDDLE_BAR_UPPER_PLACE = 4
#	MIDDLE_BAR_DOWNER_PLACE = 4
#	MIDDLE_BAR_PER_HEIGHT = 27
#	TEMP_SPACE = MIDDLE_BAR_UPPER_PLACE + MIDDLE_BAR_DOWNER_PLACE
#	class MiddleBar(ui.DragButton):
#		MIDDLE_BAR_PER_HEIGHT = 27
#		MIDDLE_BAR_WIDTH = 6
#		def __init__(self):
#			ui.DragButton.__init__(self)
#			self.AddFlag("movable")
#		def MakeImage(self):
#			top = ui.ImageBox()
#			top.SetParent(self)
#			top.LoadImage(IMG_DIR+"scroll_top.tga")
#			top.SetPosition(0, 0)
#			top.AddFlag("not_pick")
#			top.Show()
#			bottom = ui.ImageBox()
#			bottom.SetParent(self)
#			bottom.LoadImage(IMG_DIR+"scroll_bottom.tga")
#			bottom.AddFlag("not_pick")
#			bottom.Show()
#			middle = ui.ExpandedImageBox()
#			middle.SetParent(self)
#			middle.LoadImage(IMG_DIR+"scroll_mid.tga")
#			middle.SetPosition(0, self.MIDDLE_BAR_PER_HEIGHT)
#			middle.AddFlag("not_pick")
#			middle.Show()
#			self.top = top
#			self.bottom = bottom
#			self.middle = middle
#		def SetSize(self, height):
#			height = max(self.MIDDLE_BAR_PER_HEIGHT * 3, height)
#			ui.DragButton.SetSize(self, 10, height)
#			self.bottom.SetPosition(0, height-self.MIDDLE_BAR_PER_HEIGHT)
#			height -= self.MIDDLE_BAR_PER_HEIGHT*3
#			self.middle.SetRenderingRect(0, 0, 0, float(height)/float(self.MIDDLE_BAR_PER_HEIGHT))
#	def __init__(self):
#		ui.Window.__init__(self)
#		self.pageSize = 1
#		self.curPos = 0.0
#		self.eventScroll = lambda *arg: None
#		self.lockFlag = FALSE
#		self.scrollStep = 0.20
#		self.CreateScrollBar()
#	def __del__(self):
#		ui.Window.__del__(self)
#	def CreateScrollBar(self):
#		barSlot = ui.Bar3D()
#		barSlot.SetParent(self)
#		barSlot.AddFlag("not_pick")
#		barSlot.Show()
#		middleBar = self.MiddleBar()
#		middleBar.SetParent(self)
#		middleBar.SetMoveEvent(ui.__mem_func__(self.OnMove))
#		middleBar.Show()
#		middleBar.MakeImage()
#		middleBar.SetSize(60)
#		self.middleBar = middleBar
#		self.barSlot = barSlot
#		self.SCROLLBAR_MIDDLE_HEIGHT = self.middleBar.GetHeight()
#	def Destroy(self):
#		self.middleBar = None
#		self.eventScroll = lambda *arg: None
#	def SetScrollEvent(self, event):
#		self.eventScroll = event
#	def SetMiddleBarSize(self, pageScale):
#		realHeight = self.GetHeight()
#		self.SCROLLBAR_MIDDLE_HEIGHT = int(pageScale * float(realHeight))
#		self.middleBar.SetSize(self.SCROLLBAR_MIDDLE_HEIGHT)
#		self.pageSize = self.GetHeight() - self.SCROLLBAR_MIDDLE_HEIGHT - (self.TEMP_SPACE)
#	def SetScrollBarSize(self, height):
#		self.pageSize = height - self.SCROLLBAR_MIDDLE_HEIGHT - (self.TEMP_SPACE)
#		self.SetSize(self.SCROLLBAR_WIDTH, height)
#		self.middleBar.SetRestrictMovementArea(self.MIDDLE_BAR_POS, self.MIDDLE_BAR_UPPER_PLACE, self.MIDDLE_BAR_POS+2, height - self.TEMP_SPACE)
#		self.middleBar.SetPosition(self.MIDDLE_BAR_POS, 0)
#		self.UpdateBarSlot()
#	def UpdateBarSlot(self):
#		self.barSlot.SetPosition(0, 2)
#		self.barSlot.SetSize(self.GetWidth() - 2, self.GetHeight() - 2)
#	def GetPos(self):
#		return self.curPos
#	def SetPos(self, pos):
#		pos = max(0.0, pos)
#		pos = min(1.0, pos)
#		newPos = float(self.pageSize) * pos
#		self.middleBar.SetPosition(self.MIDDLE_BAR_POS, int(newPos) + self.MIDDLE_BAR_UPPER_PLACE)
#		self.OnMove()
#	def SetScrollStep(self, step):
#		self.scrollStep = step
#	def GetScrollStep(self):
#		return self.scrollStep
#	def OnUp(self):
#		self.SetPos(self.curPos-self.scrollStep)
#	def OnDown(self):
#		self.SetPos(self.curPos+self.scrollStep)
#	def OnMove(self):
#		if self.lockFlag:
#			return
#		if 0 == self.pageSize:
#			return
#		(xLocal, yLocal) = self.middleBar.GetLocalPosition()
#		self.curPos = float(yLocal - self.MIDDLE_BAR_UPPER_PLACE) / float(self.pageSize)
#		self.eventScroll()
#	def OnMouseLeftButtonDown(self):
#		(xMouseLocalPosition, yMouseLocalPosition) = self.GetMouseLocalPosition()
#		pickedPos = yMouseLocalPosition - self.SCROLLBAR_MIDDLE_HEIGHT/2
#		newPos = float(pickedPos) / float(self.pageSize)
#		self.SetPos(newPos)
#	def LockScroll(self):
#		self.lockFlag = TRUE
#	def UnlockScroll(self):
#		self.lockFlag = FALSE
def NumberToMoneyFormat(n):
	if n <= 0 :
		return "0"
	return "%s" % ('.'.join([ i-3<0 and str(n)[:i] or str(n)[i-3:i] for i in range(len(str(n))%3, len(str(n))+1, 3) if i ]))
class MultiTextLine(ui.Window):
	def __del__(self):
		ui.Window.__del__(self)
	def Destroy(self):
		self.children = []
		self.rangeText = 0
	def __init__(self):
		ui.Window.__init__(self)
		self.children = []
		self.rangeText = 15
		self.textType = ""
		self.fontName = ""
		self.outline = False
		self.rgb = (0,0,0)
	def SetFontColor(self, r, g, b):
		self.rgb = (r, g, b)
		for text in self.children:
			if self.rgb[0] != 0:
				text.SetFontColor(self.rgb[0],self.rgb[1],self.rgb[2])
	def SetOutline(self, flag):
		self.outline = flag
		for text in self.children:
			if self.outline:
				text.SetOutline()
	def SetTextType(self, textType):
		self.textType = textType
		for text in self.children:
			self.AddTextType(self.textType.split("#"),text)
	def SetFontName(self, fontName):
		self.fontName = fontName
		for text in self.children:
			text.SetFontName(fontName)
	def GetTextSize(self, index = 0):
		if len(self.children) == 0:
			return (0,0)
		elif index >= len(self.children):
			return (0,0)
		return self.children[index].GetTextSize()

	def SetTextRange(self, range):
		self.rangeText = range
		yPosition = 0
		for text in self.children:
			text.SetPosition(0,yPosition)
			yPosition+=self.rangeText
	def AddTextType(self, typeArg, text):
		if len(typeArg) > 1:
			if typeArg[0] == "vertical":
				if typeArg[1] == "top":
					text.SetVerticalAlignTop()
				elif typeArg[1] == "bottom":
					text.SetVerticalAlignBottom()
				elif typeArg[1] == "center":
					text.SetVerticalAlignCenter()
			elif typeArg[0] == "horizontal":
				if typeArg[1] == "left":
					text.SetHorizontalAlignLeft()
				elif typeArg[1] == "right":
					text.SetHorizontalAlignRight()
				elif typeArg[1] == "center":
					text.SetHorizontalAlignCenter()
	def SetText(self, cmd):
		if len(self.children) >= 1:
			self.children=[]
		multi_arg = cmd.split("\n")
		yPosition = 0
		for text in multi_arg:
			childText = ui.TextLine()
			childText.SetParent(self)
			childText.SetPosition(0,yPosition)
			if self.rgb[0] != 0:
				childText.SetFontColor(self.rgb[0],self.rgb[1],self.rgb[2])
			if self.textType != "":
				self.AddTextType(self.textType.split("#"),childText)
			if self.fontName != "":
				childText.SetFontName(self.fontName)
			if self.outline:
				childText.SetOutline()
			childText.SetText(str(text))
			childText.Show()
			self.children.append(childText)
			yPosition+=self.rangeText
class ListBox(ui.Window):
	def __init__(self, isHorizontal = False):
		ui.Window.__init__(self)
		self.viewItemCount=10
		self.basePos=0
		self.itemHeight=16
		self.itemStep=20
		self.itemList=[]
		self.itemWidth=100
		self.scrollBar=None
		self.isHorizontal= isHorizontal
	def __del__(self):
		ui.Window.__del__(self)
	def __GetItemCount(self):
		return len(self.itemList)
	def SetItemStep(self, itemStep):
		self.itemStep=itemStep
	def SetItemSize(self, itemWidth, itemHeight):
		self.itemWidth=itemWidth
		self.itemHeight=itemHeight
	def SetViewItemCount(self, viewItemCount):
		self.viewItemCount=viewItemCount
	def RemoveAllItems(self):
		for item in self.itemList:
			item.Destroy()
		self.itemList=[]
		if self.scrollBar:
			self.scrollBar.SetPos(0)
	def GetItems(self):
		return self.itemList
	def RemoveItem(self, delItem):
		self.itemList.remove(delItem)
	def AppendItem(self, newItem):
		self.itemList.append(newItem)
	def SetScrollBar(self, scrollBar):
		scrollBar.SetScrollEvent(ui.__mem_func__(self.__OnScroll))
		self.scrollBar=scrollBar
	def OnRunMouseWheel(self, nLen):
		if self.scrollBar:
			if self.scrollBar.IsShow():
				if nLen > 0:
					self.scrollBar.OnUp()
				else:
					self.scrollBar.OnDown()
				return True
		return False
		
	def OnMouseWheel(self, nLen):
		return self.OnRunMouseWheel(nLen)
		
	def __OnScroll(self):
		self.SetBasePos(int(self.scrollBar.GetPos()*self.__GetScrollLen()))
	def __GetScrollLen(self):
		if self.__GetItemCount() == 0:
			return 0
		(lx,ly) = self.itemList[len(self.itemList)-1].exPos
		return ly
	def Render(self,basePos):
		for item in self.itemList:
			(ex,ey) = item.exPos
			if self.isHorizontal:
				item.SetPosition(ex-(basePos),ey)
			else:
				item.SetPosition(ex,ey-(basePos))
			item.OnRender()
	def SetBasePos(self, basePos):
		if self.basePos == basePos:
			return
		self.Render(basePos)
		self.basePos=basePos

# def Simulation():
	# itemList = []
	# randomItems = [149,50300,51501,51502,11299,469,70048,70005,70043,30197]
	# itemID = 0
	# topSellingIndex = 0

	# for categoryIndex in xrange(5):
		# for x in xrange(3):
			# forList = []
			# for j in xrange(app.GetRandom(1,6)):
				# discount = 0
				# if app.GetRandom(0,15) == 3:
					# discount = app.GetRandom(0,99)
				# time = 0
				# if app.GetRandom(0,15) == 0:
					# time = app.GetGlobalTimeStamp()+app.GetRandom(1,60*60*24*7)
				# addedTime = 0
				# if app.GetRandom(0,2) == 0:
					# addedTime = app.GetGlobalTimeStamp()-app.GetRandom(1,60*60*24*7)
				# forList.append([itemID,randomItems[app.GetRandom(0,len(randomItems)-1)],app.GetRandom(10,100),discount,time,app.GetRandom(0,100),addedTime])
				# itemID+=1
			# itemList.append(forList)
		# itemshopData[categoryIndex] = itemList
		# itemList=[]
		# for j in xrange(50):
			# playerLog.append(["06/12/2021 21:59",app.GetGlobalTimeStamp()-app.GetRandom(1,200),"testaccount","127.0.0.1",randomItems[app.GetRandom(0,len(randomItems)-1)],app.GetRandom(1,200),app.GetRandom(1,200)])
# #Simulation()
