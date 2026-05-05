import offlineshop
import ui
import app
import item
import player
import dbg
import uicommon
import uitooltip
import ime
import snd
import uipickmoney
import grp
import mousemodule
#import renderTarget
import wndMgr
import localeinfo
import os
ENABLE_ITEM_WITHDRAW_QUESTION_SHOP_SAFEBOX = False

SUBTYPE_NOSET = 255
SEARCH_RESULT_LIMIT = 1000
COLOR_TEXT_SHORTCUT = grp.GenerateColor(1.0,1.0,0.5,1.0)
COLOR_SHOP_VIEWED_NAME = grp.GenerateColor(0.45, 0.9, 0.45, 1.0)
YANG_PER_CHEQUE = 1000000000
# Must match server GOLD_MAX (e.g. GOLD_MAX = 999000000000LL)
OFFLINESHOP_GOLD_MAX_YANG = 999000000000

ENABLE_CHEQUE_SYSTEM = 0
ENABLE_WOLFMAN_CHARACTER = 0
ENABLE_MOUNT_COSTUME_SYSTEM = 0
ENABLE_ACCE_COSTUME_SYSTEM = 0
ENABLE_WEAPON_COSTUME_SYSTEM = 0
ENABLE_MAGIC_REDUCTION_SYSTEM = 0

try:
	if app.ENABLE_CHEQUE_SYSTEM:
		ENABLE_CHEQUE_SYSTEM = 1
except:
	pass
try:
	if app.ENABLE_WOLFMAN_CHARACTER:
		ENABLE_WOLFMAN_CHARACTER = 1
except:
	pass
try:
	if app.ENABLE_MOUNT_COSTUME_SYSTEM:
		ENABLE_MOUNT_COSTUME_SYSTEM = 1
except:
	pass
try:
	if app.ENABLE_ACCE_COSTUME_SYSTEM:
		ENABLE_ACCE_COSTUME_SYSTEM = 1
except:
	pass
try:
	if app.ENABLE_WEAPON_COSTUME_SYSTEM:
		ENABLE_WEAPON_COSTUME_SYSTEM = 1
except:
	pass
try:
	if app.ENABLE_MAGIC_REDUCTION_SYSTEM:
		ENABLE_MAGIC_REDUCTION_SYSTEM = 1
except:
	pass
	

#globals methods used to know if are making a new shop (inventory stuff)
def IsBuildingShop():
	interface = offlineshop.GetOfflineshopBoard()
	if not interface:
		return False
	return interface.IsBuildingShop()


def IsSaleSlot(win,slot):
	interface = offlineshop.GetOfflineshopBoard()
	if not interface:
		return False
	
	return interface.IsForSaleSlot(win,slot)

#auctions
def IsForAuctionSlot(win,slot):
	interface = offlineshop.GetOfflineshopBoard()
	if not interface:
		return False

	return interface.IsForAuctionSlot(win, slot)

def IsBuildingAuction():
	interface = offlineshop.GetOfflineshopBoard()
	if not interface:
		return False

	return interface.IsBuildingAuction()


def PutsError(line):
	dbg.TraceError("offlineshop interface error : %s "%line)



def NumberToString(num):
	if num < 0:
		return "-" + NumberToString(-num)
	parts = []
	while num >= 1000:
		parts.insert(0,"%03d"%(num%1000))
		num = num//1000

	parts.insert(0,"%d"%num)
	return '.'.join(parts)

def GetShopDurationString(dur):
	if dur == 0:
		return getattr(localeinfo, "OFFLINESHOP_INFINITE_DURATION", "Infinite")

	return GetDurationString(dur)


def GetDurationString(dur):
	days    = dur // (24 * 60)
	hours   = (dur//60)%24
	minutes = dur%60
	
	res = " "
	
	if days > 0:
		res += localeinfo.OFFLINESHOP_DAY_TEXT%days + " "
	
	if hours > 0:
		res += localeinfo.OFFLINESHOP_HOUR_TEXT%hours + " "
	
	if minutes > 0:
		res += localeinfo.OFFLINESHOP_MINUTE_TEXT%minutes
	
	if days == 0 and hours == 0 and minutes == 0:
		return localeinfo.OFFLINESHOP_MINUTE_TEXT%0
	
	
	return res

def MakeSlotInfo(window, slotIndex, yang, cheque=0):
	res = {}
	
	itemIndex = player.GetItemIndex(window, slotIndex)
	itemCount = player.GetItemCount(window, slotIndex)
	
	res["slot"]	 = slotIndex
	res["window"]= window
	res["vnum"]	 = itemIndex
	res["count"] = itemCount
	
	res["socket"]= {}
	res["attr"]  = {}
	
	res['locked_attr'] = player.GetItemAttrLocked(window, slotIndex)

	try:
		if ENABLE_CHANGELOOK_SYSTEM:
			res['trans'] = player.GetItemTransmutation(window, slotIndex)
	except:
		pass

	if ENABLE_CHEQUE_SYSTEM:
		res['cheque'] = cheque
	
	for x in xrange(player.ATTRIBUTE_SLOT_MAX_NUM):
		attr = player.GetItemAttribute(window, slotIndex, x)
		
		res['attr'][x] = {}
		res['attr'][x]["type"]  = attr[0]
		res['attr'][x]["value"] = attr[1]
	
	for x in xrange(player.METIN_SOCKET_MAX_NUM):
		res['socket'][x] = player.GetItemMetinSocket(window, slotIndex, x)
	
	res["price"] = yang
	return res


def MakeOfferCancelButton():
	btn = ui.Button()
	btn.SetUpVisual("offlineshop/myoffers/deleteoffer_default.png")
	btn.SetDownVisual("offlineshop/myoffers/deleteoffer_down.png")
	btn.SetOverVisual("offlineshop/myoffers/deleteoffer_over.png")
	btn.Show()
	return btn



def MakeOfferViewImage(isView):
	flag = "0"
	if isView:
		flag="1"

	img = ui.ImageBox()
	img.LoadImage("offlineshop/myoffers/viewicon_%s.png"%flag)
	img.Show()
	return img


def MakeDefaultEmptySlot(event = None):
	slot = Slot(False)
	if event:
		slot.SetOnMouseLeftButtonUpEvent(event)
	return slot



def GetBestOfferPriceYang(lst):
	if not lst:
		return 0

	max =0
	for info in lst:
		if info['price_yang'] > max:
			max = info['price_yang']

	return max



def SortByDatetime(lst):
	def CustomSortByDatetime(a,b):
		def CmpFunc(a,b):
			if a > b:
				return -1
			if b > a:
				return 1
			return 0

		keys = ('year' , 'month' , 'day', 'hour', 'minute')
		for k in keys:
			if a[k] != b[k]:
				return CmpFunc(a[k] , b[k])
		return 0

	lst.sort(cmp=CustomSortByDatetime)


def SortOffersByPrice(lst):
	def CustomSortByOfferPrice(a,b):
		price_a = a.get('price_yang', 0)
		price_b = b.get('price_yang', 0)

		if price_a > price_b:
			return -1

		if price_b < price_b:
			return 1
		return 0

	lst.sort(cmp=CustomSortByOfferPrice)
	return lst





class TableWindow(ui.Window):
	
	def __init__(self, col, row, width, height, realw, realh):
		ui.Window.__init__(self)
		self.rows 			= row
		self.columns		= col

		self.width			= width
		self.height			= height

		ui.Window.SetSize(self, realw, realh)

	
	def __del__(self):
		ui.Window.__del__(self)
	

	def __GetCoordByPos(self, col,row, child):
		stepx = self.width/self.columns
		stepy = self.height/self.rows

		x = stepx * col
		y = stepy * row

		x += stepx/2 - child.GetWidth() /2
		y += stepy/2 - child.GetHeight()/2

		return x,y

	def SetTableElement(self, column , row, child= None):
		if self.rows <= row:
			PutsError("cannot SetTableElement on row %d"%row)
			return
		
		if self.columns <= column:
			PutsError("cannot SetTableElement on column %d (row %d )"%(column,row))
			return

		if child == None:
			return
		
		x,y = self.__GetCoordByPos(column, row, child)

		child.SetParent(self)
		child.SetPosition( x, y)
		child.Show()

		
	

class TableWindowWithScrollbar(TableWindow):

	SCROLLBAR_HORIZONTAL 	= 0
	SCROLLBAR_VERTICAL		= 1

	def __init__(self, w, h , columns, rows, scrollbar=0):
		htable = h if scrollbar == self.SCROLLBAR_VERTICAL else h-18
		wtable = w if scrollbar == self.SCROLLBAR_HORIZONTAL else w -18

		TableWindow.__init__(self , columns, rows , wtable, htable, w ,h)

		self.childrenDict 	= {}
		self.scrollbar 		= None
		self.rows_count 	= rows
		self.columns_count	= columns
		self.total_row		= 0
		self.elementCount	= 0

		self.defaultCreate  	= None
		self.defaultCreateArgs 	= None
		self.defaultChildren 	= []

		self.__loadScrollbar(scrollbar)
		self.ClearElement()
	
	
	def __del__(self):
		self.childrenDict = {}
		self.scrollbar = None
		self.defaultCreate  	= None
		self.defaultCreateArgs 	= None
		self.defaultChildren 	= []
		TableWindow.__del__(self)
	
	
	
	def __loadScrollbar(self, scrollbar):

		if scrollbar == self.SCROLLBAR_VERTICAL:
			template = {
				'button1' 	: {
					'default' 	: 'offlineshop/scrollbar/vertical/button1_default.png',
					'over' 		: 'offlineshop/scrollbar/vertical/button1_over.png',
					'down' 		: 'offlineshop/scrollbar/vertical/button1_down.png',
				},
				'button2' 		: {
					'default'	: 'offlineshop/scrollbar/vertical/button2_default.png',
					'over'		: 'offlineshop/scrollbar/vertical/button2_over.png',
					'down'		: 'offlineshop/scrollbar/vertical/button2_down.png',
				},
				'middle'  		: {
					'default'	: 'offlineshop/scrollbar/vertical/middle_default.png',
					'over'		: 'offlineshop/scrollbar/vertical/middle_over.png',
					'down'		: 'offlineshop/scrollbar/vertical/middle_down.png',
				},
				'base'	  		: "offlineshop/scrollbar/vertical/base_image.png",
				'onscroll'		: self.__refreshViewList,
				'parent'		: self,
				'orientation'	: ui.CustomScrollBar.VERTICAL,
				'align'			: {'mode': ui.CustomScrollBar.RIGHT,},
			}

		else:
			template = {
				'button1' 	: {
					'default' 	: 'offlineshop/scrollbar/horizontal/button1_default.png',
					'over' 		: 'offlineshop/scrollbar/horizontal/button1_over.png',
					'down' 		: 'offlineshop/scrollbar/horizontal/button1_down.png',
				},
				'button2' 		: {
					'default'	: 'offlineshop/scrollbar/horizontal/button2_default.png',
					'over'		: 'offlineshop/scrollbar/horizontal/button2_over.png',
					'down'		: 'offlineshop/scrollbar/horizontal/button2_down.png',
				},
				'middle'  		: {
					'default'	: 'offlineshop/scrollbar/horizontal/middle_default.png',
					'over'		: 'offlineshop/scrollbar/horizontal/middle_over.png',
					'down'		: 'offlineshop/scrollbar/horizontal/middle_down.png',
				},
				'base'	  		: "offlineshop/scrollbar/horizontal/base_image.png",
				'onscroll'		: self.__refreshViewList,
				'parent'		: self,

				'orientation'	: ui.CustomScrollBar.HORIZONTAL,
				'align'			: {'mode':ui.CustomScrollBar.BOTTOM,},
			}
		
		self.scrollbar = ui.CustomScrollBar(template)
	
	
	
	def SetElement(self, column, row, child):
		if not row  in self.childrenDict:
			self.childrenDict[row] = {}
		
		self.childrenDict[row][column] = child
		self.__refreshViewList()
	

	def SetDefaultCreateChild(self, func, *args):
		self.defaultCreate = func
		self.defaultCreateArgs = args

	def __refreshViewList(self):
		if self.scrollbar.orientation == ui.CustomScrollBar.VERTICAL:
			if len(self.childrenDict.keys()) <= self.rows_count:
				self.scrollbar.Hide()
			else:
				self.scrollbar.Show()

			init_row = self.__getInitRow()

			for row, dct in self.childrenDict.items():
				for column, child in dct.items():
					if row < init_row or row >= init_row + self.rows_count:
						child.Hide()
						continue
					self.SetTableElement(column, row-init_row , child)
			
			#updated 25-01-2020 #topatch
			for s in self.defaultChildren:
				s.Destroy()
			
			self.defaultChildren = []
			if self.defaultCreate:
				for row in xrange(init_row, init_row + self.rows_count):
					for col in xrange(self.columns_count):
						if not row in self.childrenDict or not col in self.childrenDict[row]:
							defaultChild = self.defaultCreate(*self.defaultCreateArgs) if self.defaultCreateArgs else self.defaultCreate()
							defaultChild.Show()
							self.SetTableElement(col, row - init_row, defaultChild)
							self.defaultChildren.append(defaultChild)



		elif self.scrollbar.orientation == ui.CustomScrollBar.HORIZONTAL:
			if len(self.childrenDict.get(0, [])) <= self.columns_count:
				self.scrollbar.Hide()
			else:
				self.scrollbar.Show()

			init_column = self.__getInitColumn()
			end_column  = init_column+ self.columns_count
			# norm_column = xrange(0, self.columns_count)

			for dct in self.childrenDict.values():
				for elm in dct.values():
					elm.Hide()

			for col in xrange(init_column, end_column):
				for row, dct in self.childrenDict.items():
					if col in dct:
						self.SetTableElement(col-init_column, row , dct[col])
						dct[col].Show()
			
			#updated 25-01-2020 #topatch
			for s in self.defaultChildren:
				s.Destroy()
			
			self.defaultChildren = []
			if self.defaultCreate:
				for row in xrange(0, self.rows_count):
					for col in xrange(init_column, end_column):
						if not row in self.childrenDict or not col in self.childrenDict[row]:
							defaultChild = self.defaultCreate(*self.defaultCreateArgs) if self.defaultCreateArgs else self.defaultCreate()
							defaultChild.Show()
							self.SetTableElement(col-init_column, row, defaultChild)
							self.defaultChildren.append(defaultChild)




	def __getInitRow(self):
		if not self.scrollbar.IsShow():
			return 0
		
		return int((len(self.childrenDict.keys()) - self.rows_count ) * self.scrollbar.GetPos())


	def __getInitColumn(self):
		if not self.scrollbar.IsShow():
			return 0

		return int((len(self.childrenDict.get(0,[])) - self.columns_count ) * self.scrollbar.GetPos())

		
	def __AdjustScrollbarStep(self):
		total_slots		= self.rows_count*self.columns_count
		extra_element 	= self.elementCount - (total_slots)
		
		if extra_element <= 0:
			return

		if self.scrollbar.orientation == ui.CustomScrollBar.HORIZONTAL:
			extra_col = int(extra_element // self.rows_count)
			if extra_element % self.rows_count != 0:
				extra_col += 1
			self.scrollbar.SetScrollStep(1.0 / float(extra_col+1))

		else:
			extra_row_count = int(extra_element/self.columns_count)
			if extra_element % self.columns_count != 0:
				extra_row_count += 1

			self.scrollbar.SetScrollStep(1.0/float(extra_row_count+1))

	def ClearElement(self):
		for a in self.childrenDict.values():
			for child in a.values():
				child.Hide()
		
		self.childrenDict = {}
		self.elementCount = 0
		self.__refreshViewList()

	
	def AddElement(self, child):
		normElement =  self.columns_count*self.rows_count

		if self.elementCount < normElement or self.scrollbar.orientation == ui.CustomScrollBar.VERTICAL:
			column 	= self.elementCount%self.columns_count
			row		= self.elementCount//self.columns_count

		else:
			index = self.elementCount - normElement
			row   = index % self.rows_count
			column= index //self.rows_count

			column += self.columns_count

		self.SetElement(column, row, child)
		self.elementCount += 1
		
		self.__AdjustScrollbarStep()
	
	def GetElementDict(self):
		return self.childrenDict

class Slot(ui.Window):
	

	
	def __init__(self, isSold=False):
		ui.Window.__init__(self)

		self.background = None
		self.background_sold = None
		self.iconImage = None
		self.countText = None
		self.upgradeImage = None

		self.slotInfo = {}
		self.index = 0
		self.eventClick = None
		self.childrens = []

		self.__loadBackground(isSold)
		
		print("initializing Slot")
	
	#updated 25-01-2020 #topatch
	def __del__(self):
		self.__clear()
		ui.Window.__del__(self)
	
	#updated 25-01-2020 #topatch
	def __clear(self):
		self.background 		= None
		self.iconImage  		= None
		self.countText			= None
		self.upgradeImage		= None
		self.background_sold	= None
		
		ui.Window.SetOnMouseLeftButtonUpEvent(self, None)
		
		if self.background:
			self.background.SetOnMouseLeftButtonUpEvent(None)
		
		if self.iconImage:
			self.iconImage.SetOnMouseLeftButtonUpEvent(None)
		
		if self.countText:
			self.countText.SetOnMouseLeftButtonUpEvent(None)
		
		if self.upgradeImage:
			self.upgradeImage.SetOnMouseLeftButtonUpEvent(None)

		if self.background_sold:
			self.background_sold.SetOnMouseLeftButtonUpEvent(None)
	
		self.slotInfo 		= {}
		self.index			= 0
		self.eventClick		= None
		self.childrens		= []
	
	#updated 25-01-2020 #topatch
	def Destroy(self):
		self.__clear()
	
	
	def __loadBackground(self, isSold):
		bg = ui.ImageBox()
		bg.LoadImage("offlineshop/slot/base_image.png")
		bg.SetParent(self)
		bg.SetPosition(0,0)
		bg.Show()

		self.SetSize(bg.GetWidth(), bg.GetHeight())
		self.background = bg

		sold = ui.ImageBox()
		sold.LoadImage("offlineshop/slot/base_image_sold.png")
		sold.SetParent(self.background)
		sold.SetPosition(0, 0)

		if isSold:
			sold.Show()
		else:
			sold.Hide()

		self.background_sold = sold



	
	def SetInfo(self, info):
		self.slotInfo = info
		self.SetSlot(info["vnum"] , info["count"])
	
	def GetInfo(self):
		return self.slotInfo
	
	def SetSlot(self, itemvnum, itemcount):
		item.SelectItem(itemvnum)
		
		image 	= item.GetIconImageFileName()
		name	= item.GetItemName()
		
		self.__SetItemIconImage(image)
		self.__SetItemCount(itemcount)
		self.__SetUpgradeByName(name)
	
	
	def SetIndex(self, index):
		self.index = index
	
	
	def GetIndex(self):
		return self.index
	

	def SetSold(self, flag):
		if flag:
			self.background_sold.Show()
		else:
			self.background_sold.Hide()


	def __GetIconPosition(self, image):
		w = image.GetWidth()
		h = image.GetHeight()
		
		return (self.background.GetWidth()/2 - w/2,  self.background.GetHeight()/2 - h/2)
	
	def __SetItemIconImage(self, icon):
		image = ui.ImageBox()
		image.LoadImage(icon)
		image.SetParent(self.background)
		
		x,y = self.__GetIconPosition(image)
		
		image.SetPosition(x,y)
		image.Show()
		
		self.iconImage = image
	
	
	def __GetCountPosition(self):
		x,y = self.iconImage.GetLocalPosition()
		x += self.iconImage.GetWidth()/2
		y += self.iconImage.GetHeight()
		
		return (x,y)
	
	
	def __SetItemCount(self, count):
		if count <= 1:
			self.countText = None
			return
		
		countText = ui.TextLine()
		countText.SetParent(self.background)
		
		x,y = self.__GetCountPosition()
		countText.SetPosition(x,y)
		countText.SetHorizontalAlignCenter()
		countText.SetText("x"+str(count))
		countText.Show()
		
		self.countText = countText
	
	
	
	def __GetUpgradeImagePosition(self,img):
		w = img.GetWidth()
		h = img.GetHeight()
		
		bw = self.background.GetWidth()
		bh = self.background.GetHeight()
		
		return (bw - (w+5), bh-(h+5))
	
	
	def __SetUpgradeByName(self, name):
		name = name.strip()
		if len(name) > 2:
			if name[-2] == '+':
				if name[-1].isdigit():
					value = int(name[-1])
					
					upgrade = ui.ImageBox()
					upgrade.LoadImage("offlineshop/slot/upgrade/%d.png"%value)
					upgrade.SetParent(self.background)
					
					x,y = self.__GetUpgradeImagePosition(upgrade)
					upgrade.SetPosition(x,y)
					upgrade.Show()
					
					self.upgradeImage = upgrade
	
	
	def SetOnMouseLeftButtonUpEvent(self, event):
		self.eventClick = event
		ui.Window.SetOnMouseLeftButtonUpEvent(self, self.__OnClick)
		
		if self.background:
			self.background.SetOnMouseLeftButtonUpEvent(self.__OnClick)
		
		if self.iconImage:
			self.iconImage.SetOnMouseLeftButtonUpEvent(self.__OnClick)
		
		if self.countText:
			self.countText.SetOnMouseLeftButtonUpEvent(self.__OnClick)
		
		if self.upgradeImage:
			self.upgradeImage.SetOnMouseLeftButtonUpEvent(self.__OnClick)

		if self.background_sold:
			self.background_sold.SetOnMouseLeftButtonUpEvent(self.__OnClick)


	def __OnClick(self):
		if self.eventClick!=None:
			self.eventClick(self)

	def IsInSlot(self):
		if not self.IsShow():
			return False
		
		if self.IsIn():
			return True
		
		if self.background:
			if self.background.IsIn():
				return True
		
		if self.iconImage:
			if self.iconImage.IsIn():
				return True
		
		if self.countText:
			if self.countText.IsIn():
				return True
		
		if self.upgradeImage:
			if self.upgradeImage.IsIn():
				return True

		if self.background_sold:
			if self.background_sold.IsShow() and self.background_sold.IsIn():
				return True

		if self.childrens:
			for child in self.childrens:
				if child.IsIn():
					return True


		return False


	def AppendChild(self, child):
		child.SetParent(self.background)
		self.childrens.append(child)




class Offer(ui.Window):
	


	def __init__(self, info):
		ui.Window.__init__(self)

		self.bgImage = None
		self.guestName = None
		self.priceText = None
		self.acceptButton = None
		self.deleteButton = None
		self.itemText = None
		self.info = {}
		self.index = 0
		self.is_accept = 0
		self.acceptEvent = None
		self.deniedEvent = None



		self.info  		= info
		self.index 		= info["id"]
		self.is_accept	= info["is_accept"]

		self.__loadBackground()
		self.__loadButtons()
		self.SetGuestName(info['buyer_name'])
		self.SetPriceYang(info["price"])


	def __del__(self):

		self.bgImage = None
		self.guestName = None
		self.priceText = None
		self.acceptButton = None
		self.deleteButton = None
		self.itemText = None
		self.info = {}
		self.index = 0
		self.is_accept = 0
		self.acceptEvent = None
		self.deniedEvent = None

		ui.Window.__del__(self)
	
	def __loadBackground(self):
		bg = ui.ImageBox()
		bg.SetParent(self)
		bg.SetPosition(0,0)
		bg.LoadImage("offlineshop/offer/base_image.png")
		bg.Show()

		self.SetSize(bg.GetWidth(), bg.GetHeight())
		self.bgImage = bg
	
	
	def SetGuestName(self, name):
		text = ui.TextLine()
		text.SetParent(self.bgImage)
		text.SetPosition(77 , 0)
		text.SetHorizontalAlignCenter()
		text.Show()
		text.SetText(name)
		self.guestName = text
	
	
	def SetPriceYang(self, yang):
		text = ui.TextLine()
		text.SetParent(self.bgImage)
		text.SetPosition(355 , 0)
		# text.SetHorizontalAlignCenter()
		text.Show()
		text.SetText(localeinfo.NumberToMoneyString(yang))
		self.priceText = text
	
	
	
	def SetItemName(self, name):
		text = ui.TextLine()
		text.SetParent(self.bgImage)
		text.SetPosition(239 , 0)
		text.SetHorizontalAlignCenter()
		text.Show()
		text.SetText(name)
		self.itemText = text
	
	def __loadButtons(self):
		if self.info["is_accept"]:
			return
		#accept
		button = ui.Button()
		button.SetParent(self.bgImage)
		button.SetPosition(self.bgImage.GetWidth() - 50, 0)
		
		path = "offlineshop/offer/accept_%s.png"
		button.SetUpVisual(path%"default")
		button.SetOverVisual(path%"over")
		button.SetDownVisual(path%"down")
		button.Show()
		
		self.acceptButton = button

		#denied
		button = ui.Button()
		button.SetParent(self.bgImage)
		button.SetPosition(self.bgImage.GetWidth() - 30, 0)
		
		path = "offlineshop/offer/cancel_%s.png"
		button.SetUpVisual(path%"default")
		button.SetOverVisual(path%"over")
		button.SetDownVisual(path%"down")
		button.Show()

		self.deleteButton = button
	
	
	def SetAcceptButtonEvent(self, event):
		if self.acceptButton:
			self.acceptButton.SAFE_SetEvent(self.__OnAccept)
		self.acceptEvent = event
		
	def SetDeleteButtonEvent(self, event):
		if self.deleteButton:
			self.deleteButton.SAFE_SetEvent(self.__OnCancel)
		self.deniedEvent = event

	def GetInfo(self):
		return self.info


	def __OnAccept(self):
		if self.acceptEvent:
			self.acceptEvent(self)

	def __OnCancel(self):
		if self.deniedEvent:
			self.deniedEvent(self)


class AuctionOffer(ui.Window):

	def __init__(self, info):
		ui.Window.__init__(self)

		self.background = None
		self.ownerName = None
		self.offerText = None

		self.__LoadWindow(info)

	def __del__(self):
		self.background = None
		self.ownerName = None
		self.offerText = None
		ui.Window.__del__(self)

	def __LoadWindow(self, info):
		name = info["buyer_name"]
		best_yang = info['price_yang']

		self.__LoadBackground()
		self.__LoadOwnerName(name)
		self.__LoadOfferText(best_yang)

	def GetInfo(self):
		return self.info



	def __LoadBackground(self):
		bg = ui.ImageBox()
		bg.LoadImage("offlineshop/shoplist/shoplist_element_default.png")
		bg.SetParent(self)
		bg.SetPosition(0, 0)
		bg.Show()

		self.SetSize(bg.GetWidth(), bg.GetHeight())
		self.background = bg

	def __LoadOwnerName(self, ownerName):
		if not ownerName:
			return

		text = ui.TextLine()
		text.SetParent(self.background)
		text.SetPosition(186, 1)
		text.SetHorizontalAlignCenter()
		text.SetText(ownerName)
		text.Show()

		self.ownerName = text

	def __LoadOfferText(self, yang):
		text = ui.TextLine()
		text.SetParent(self.background)
		text.SetPosition(403, 1)
		text.SetHorizontalAlignCenter()
		text.SetText(localeinfo.NumberToMoneyString(yang))
		text.Show()

		self.offerText = text



class MyOffer(ui.Window):

	def __init__(self, info):
		ui.Window.__init__(self)
		self.info = info
		self.clear()

		self.__LoadBackground()
		self.__LoadOwnerName()
		self.__LoadOfferText()
		self.__LoadSlot()
		self.__LoadCancelButton()


	def clear(self):
		self.background   		= None
		self.ownerName    		= None
		self.offerText    		= None
		self.slot		  		= None
		self.cancelButton 		= None
		self.cancelButtonEvent	= None
		self.bar1 = None
		self.bar2 = None
		self.text1 = None
		self.text2 = None

	def __del__(self):
		self.clear()
		ui.Window.__del__(self)

	def __LoadBackground(self):
		bg = ui.ImageBox()
		bg.SetParent(self)
		bg.SetPosition(0,0)
		bg.LoadImage("offlineshop/myoffers/offer_base_image.png")
		bg.Show()

		self.SetSize(bg.GetWidth(), bg.GetHeight())
		self.background = bg
		
		bar1 = ui.Bar()
		bar1.SetSize(59, 12)
		bar1.SetColor(0x00000000)
		bar1.SetParent(self.background)
		bar1.Show()
		self.bar1 = bar1
		self.bar1.SetPosition(59, 39)
		
		text1 = ui.TextLine()
		text1.SetParent(self.bar1)
		text1.SetPosition(0, 0)
		text1.SetText(localeinfo.SUBTEXT_OFFLINESHOP_TEXT15)
		text1.Show()
		self.text1 = text1
		
		bar2 = ui.Bar()
		bar2.SetSize(59, 12)
		bar2.SetColor(0x00000000)
		bar2.SetParent(self.background)
		bar2.Show()
		self.bar2 = bar2
		self.bar2.SetPosition(59, 63)
		
		text2 = ui.TextLine()
		text2.SetParent(self.bar2)
		text2.SetPosition(0, 0)
		text2.SetText(localeinfo.SUBTEXT_OFFLINESHOP_TEXT16)
		text2.Show()
		self.text2 = text2

	def __LoadOwnerName(self):
		text = ui.TextLine()
		text.SetParent(self.background)
		text.SetPosition(175, 38)
		text.SetHorizontalAlignCenter()
		text.SetText(self.info.get('shop_name', ""))
		text.Show()

		self.ownerName = text


	def __LoadOfferText(self):
		text = ui.TextLine()
		text.SetParent(self.background)
		text.SetPosition(175, 60)
		text.SetHorizontalAlignCenter()
		text.SetText(NumberToString(self.info.get('price', "")))
		text.Show()

		self.offerText = text

	def __LoadSlot(self):
		slot = Slot(self.info['is_accept'])
		slot.SetInfo(self.info['item'])
		slot.SetParent(self.background)
		slot.SetPosition(9,4)
		slot.Show()

		self.slot = slot


	def __LoadCancelButton(self):
		button = ui.Button()
		button.SetParent(self.background)
		button.SetPosition(self.GetWidth()/2, 46 + 40)

		button.SetUpVisual("d:/ymir work/ui/public/small_button_01.sub")
		button.SetDownVisual("d:/ymir work/ui/public/small_button_03.sub")
		button.SetOverVisual("d:/ymir work/ui/public/small_button_02.sub")

		button.SetText(localeinfo.OFFLINESHOP_MYOFFERS_CANCEL_BUTTON_TEXT)

		button.Show()
		button.SAFE_SetEvent(self.__OnClickCancelButton)
		self.cancelButton = button


	def SetCancelButtonEvent(self, event):
		self.cancelButtonEvent = event



	def __OnClickCancelButton(self):
		if self.cancelButtonEvent:
			self.cancelButtonEvent(self.info['offer_id'])


	def IsInSlot(self):
		return self.slot.IsInSlot()


	def GetIndex(self):
		return self.info['offer_id']
	
	#updated 25-01-2020 #topatch
	def DisableCancelButton(self):
		if self.cancelButton:
			self.cancelButton.Hide()
	


class AuctionListElement(ui.Window):

	def __init__(self, info):
		ui.Window.__init__(self)

		self.ownerName = None
		self.durationText = None
		self.offerCountText = None
		self.owner_id = -1
		self.info = info
		self.button = None
		self.buttonEvent = None
		self.bestYangText = None

		self.__LoadWindow(info)


	def __del__(self):
		self.ownerName = None
		self.durationText = None
		self.offerCountText = None
		self.owner_id = -1
		self.info = {}
		self.button = None
		self.buttonEvent = None
		self.bestYangText = None
		ui.Window.__del__(self)

	def __LoadWindow(self, info):
		owner_id 	= info["owner_id"]
		duration 	= info["duration"]
		count 		= info["offer_count"]
		name 		= info["owner_name"]
		best_yang	= info['best_yang']

		self.owner_id = owner_id


		self.__LoadOpenAuctionButton()
		self.__LoadOwnerName(name)
		self.__LoadDuration(duration)
		self.__LoadCount(count)
		self.__LoadBestYang(best_yang)

	def SetIndex(self,index):
		self.owner_id = index

	def GetIndex(self):
		return self.owner_id

	def GetInfo(self):
		return self.info

	def SetOnClickOpenShopButton(self, event):
		self.openShopButton.SAFE_SetEvent(self.__OnClickOpenShop)
		self.openShopButtonEvent = event

	def __LoadOwnerName(self, ownerName):
		if not ownerName:
			return

		text = ui.TextLine()
		text.SetParent(self.button)
		text.SetPosition(85, 0)
		text.SetHorizontalAlignCenter()
		text.SetText(ownerName)
		text.Show()

		self.ownerName = text



	def __LoadDuration(self, duration):
		text = ui.TextLine()
		text.SetParent(self.button)
		text.SetPosition(344, 0)
		text.SetHorizontalAlignCenter()
		text.SetText(GetDurationString(duration))
		text.Show()

		self.durationText = text

	def __LoadCount(self, count):
		text = ui.TextLine()
		text.SetParent(self.button)
		text.SetPosition(480, 0)
		text.SetHorizontalAlignCenter()
		text.SetText(str(count))
		text.Show()

		self.offerCountText = text

	def __LoadBestYang(self, yang):
		text = ui.TextLine()
		text.SetParent(self.button)
		text.SetPosition(217, 0)
		text.SetHorizontalAlignCenter()
		text.SetText(localeinfo.NumberToMoneyString(yang))
		text.Show()

		self.bestYangText = text

	def SetOnClickOpenAuctionButton(self, event):
		self.button.SAFE_SetEvent(self.__OnClickMe)
		self.buttonEvent = event


	def __OnClickMe(self):
		if self.buttonEvent:
			self.buttonEvent(self.owner_id)

	def __LoadOpenAuctionButton(self):
		button = ui.Button()
		button.SetParent(self)
		button.SetUpVisual("offlineshop/shoplist/shoplist_element_default.png")
		button.SetDownVisual("offlineshop/shoplist/shoplist_element_down.png")
		button.SetOverVisual("offlineshop/shoplist/shoplist_element_over.png")

		button.SetPosition(0, 0)
		button.Show()
		self.SetSize(button.GetWidth(), button.GetHeight())
		self.button = button


	def IsInSlot(self):
		if self.IsIn():
			return True

		if self.ownerName:
			if self.ownerName.IsIn():
				return True

		if self.durationText:
			if self.durationText.IsIn():
				return True

		if self.offerCountText:
			if self.offerCountText.IsIn():
				return True

		if self.button:
			if self.button.IsIn():
				return True

		return False













class ShopListElement(ui.Window):
	

	
	def __init__(self , info, is_viewed=False):
		ui.Window.__init__(self)

		self.ownerName = None
		self.shopName = None
		self.durationText = None
		self.countText = None
		self.openShopButton = None
		self.openShopButtonEvent = None
		self.owner_id = -1
		self.is_viewed = is_viewed

		self.__LoadWindow(info)

	def __del__(self):
		self.ownerName = None
		self.shopName = None
		self.durationText = None
		self.countText = None
		self.openShopButton = None
		self.openShopButtonEvent = None

		self.owner_id = -1
		ui.Window.__del__(self)

	def __LoadWindow(self, info):
		owner_id	= info["owner_id"]
		duration	= info["duration"]
		count		= info["count"]
		name		= info["name"]
		
		ownerName	= name[:name.find('@')] if '@' in name else "NONAME"
		name		= name[name.find('@')+1:] if '@' in name else name
		
		self.owner_id  = owner_id
		
		self.__LoadOpenShopButton()
		self.__LoadOwnerName(ownerName)
		self.__LoadShopName(name)
		self.__LoadDuration(duration)
		self.__LoadCount(count)

	
	
	
	def SetOnClickOpenShopButton(self, event):
		self.openShopButton.SAFE_SetEvent(self.__OnClickOpenShop)
		self.openShopButtonEvent = event
	
	
	def __OnClickOpenShop(self):
		if self.openShopButtonEvent:
			self.openShopButtonEvent(self.owner_id)

	def __LoadOpenShopButton(self):
		button = ui.Button()
		button.SetParent(self)
		button.SetUpVisual("offlineshop/shoplist/shoplist_element_default.png")
		button.SetDownVisual("offlineshop/shoplist/shoplist_element_down.png")
		button.SetOverVisual("offlineshop/shoplist/shoplist_element_over.png")

		button.SetPosition(0, 0)
		button.Show()

		self.SetSize(button.GetWidth(), button.GetHeight())
		self.openShopButton = button



	def __LoadOwnerName(self ,ownerName):
		if not ownerName:
			return
		
		text = ui.TextLine()
		text.SetParent(self.openShopButton)
		text.SetPosition(48 , 2)
		text.SetHorizontalAlignCenter()
		text.SetText(ownerName)
		text.Show()
		
		self.ownerName = text
	
	
	def __LoadShopName(self ,name):
		text = ui.TextLine()
		text.SetParent(self.openShopButton)
		text.SetPosition(198 , 2)
		text.SetHorizontalAlignCenter()
		text.SetText(name)
		if self.is_viewed:
			text.SetPackedFontColor(COLOR_SHOP_VIEWED_NAME)
		text.Show()
		
		self.shopName = text
	
	
	def __LoadDuration(self ,duration):
		text = ui.TextLine()
		text.SetParent(self.openShopButton)
		text.SetPosition(358 , 2)
		text.SetHorizontalAlignCenter()
		text.SetText(GetShopDurationString(duration))
		text.Show()
		
		self.durationText = text
	
	def __LoadCount(self ,count):
		text = ui.TextLine()
		text.SetParent(self.openShopButton)
		text.SetPosition(480 , 2)
		text.SetHorizontalAlignCenter()
		text.SetText(str(count))
		text.Show()
		
		self.countText = text
	





class Suggestions():
	
	def __init__(self):
		self.inputBox = None
		self.comboBox = None

		self.mainDict = {}
		self.tempDict = {}
		self.isRefreshing = False
		self.isSelecting = False
	
	def SetInputBox(self, box):
		box.OnIMEUpdate = self.__OnUpdateInputBox
		self.inputBox = box
	
	def SetComboBox(self, box):
		box.SetEvent(ui.__mem_func__(self.__OnSelectItem))
		box.ClearItem()
		box.SetCurrentItem(localeinfo.OFFLINESHOP_NAME_SUGGESTION_UNSELECT)
		
		self.comboBox = box
	
	def SetMainDict(self, inDict):
		def getUnrefined(st):
			pos = st.find('+')
			if pos == -1:
				return st
			
			if pos > len(st) -4 and pos > 4:
				return st[:pos]
			return st
		
		cleanDict = {}
		inserted  = []
		
		for name , vnum in inDict.items():
			unrefined = getUnrefined(name).lower().strip()
			if unrefined in inserted:
				continue
			
			cleanDict[unrefined] = vnum
			inserted.append(unrefined)
		
		self.mainDict = cleanDict
	
	
	def __OnUpdateInputBox(self):
		snd.PlaySound("sound/ui/type.wav")
		ui.TextLine.SetText(self.inputBox, ime.GetText(self.inputBox.bCodePage))
		
		self.__RefreshComboBox()
	
	def __RefreshComboBox(self):
		if self.isSelecting:
			return
		
		self.isRefreshing = True
		
		self.tempDict 	= {}
		inputText		= self.inputBox.GetText().lower().strip()
		
		if len(inputText) < 3:
			return
		
		self.comboBox.ClearItem()
		self.comboBox.SetCurrentItem(localeinfo.OFFLINESHOP_NAME_SUGGESTION_UNSELECT)
		
		
		idx = 0
		for k,v in self.mainDict.items():
			if inputText in k.lower():
				self.tempDict[idx] = k
				self.comboBox.InsertItem(idx, k)
				
				idx += 1
				
				if idx == 20:
					break
		
		self.isRefreshing = False
	
	def __OnSelectItem(self, index):
		if self.isRefreshing:
			return
		
		self.isSelecting = True
		self.inputBox.SetText(self.tempDict[index])
		self.isSelecting = False
	
	def __del__(self):
		self.inputBox		= None
		self.comboBox	= None
		
		self.mainDict	= {}
		self.tempDict	= {}
	
	def Clear(self):
		self.comboBox.ClearItem()
		self.comboBox.SetCurrentItem(localeinfo.OFFLINESHOP_NAME_SUGGESTION_UNSELECT)

class SuggestionElement(ui.Button):
	

	
	def __init__(self):
		self.clickEvent = None
		self.index = 0

		ui.Button.__init__(self)
	
	
	def __del__(self):
		self.clickEvent = None
		ui.Button.__del__(self)
	
	def SetClickEvent(self, event):
		self.clickEvent = event
		self.SAFE_SetEvent(self.__OnClickMe)
	
	def __OnClickMe(self):
		if self.clickEvent:
			self.clickEvent(self.index)
	
	
	def SetElement(self, index, text):
		self.index = index
		self.SetText(text)
	
	



class SuggestionSelector(ui.Window):
		
	
	def __init__(self, element = 8, template = 'big'):
		ui.Window.__init__(self)

		self.scrollbar = None
		self.background = None
		self.valuesDict = {}
		self.onSelectEvent = None

		self.elements = []
		self.template = template
		self.__loadBackground()
		self.__loadElements(element)
		self.__loadScrollbar()
	
	
	def __loadBackground(self):
		bg = ui.ImageBox()
		if self.template == 'big':
			bg.LoadImage("offlineshop/searchfilter/attribute_selector_base_image.png")
		else:
			bg.LoadImage("offlineshop/searchfilter/grade_selector_base_image.png")
		bg.SetParent(self)
		bg.SetPosition(0,0)
		
		bg.Show()
		self.background = bg
		self.SetSize(bg.GetWidth() , bg.GetHeight())
	
	
	def __loadElements(self, count):
		for x in xrange(count):
			element = SuggestionElement()
			element.SetParent(self.background)
			element.SetPosition(0, x * 16)
			
			if self.template == 'big':
				element.SetUpVisual("offlineshop/searchfilter/attribute_default.png")
				element.SetDownVisual("offlineshop/searchfilter/attribute_down.png")
				element.SetOverVisual("offlineshop/searchfilter/attribute_over.png")
			
			else:
				element.SetUpVisual("offlineshop/searchfilter/grade_element_default.png")
				element.SetDownVisual("offlineshop/searchfilter/grade_element_down.png")
				element.SetOverVisual("offlineshop/searchfilter/grade_element_over.png")
			
			
			element.Show()
			
			element.SetClickEvent(self.__OnSelectElement)
			self.elements.append(element)
	
	
	def __loadScrollbar(self):
		scroll = ui.ScrollBar()
		scroll.SetParent(self.background)
		scroll.SetPosition(self.GetWidth()-10, 0)
		scroll.SetScrollBarSize(self.GetHeight())
		scroll.SetScrollEvent(self.__OnScroll)
		scroll.Show()
		
		self.scrollbar = scroll
	
	def __OnSelectElement(self, index):
		if self.onSelectEvent:
			self.onSelectEvent(index)
	
	
	
	def __OnScroll(self):
		self.__refreshViewList()
	
	
	def __refreshViewList(self):
		if len(self.elements) >= len(self.valuesDict):
			self.scrollbar.Hide()
		else:
			self.scrollbar.Show()
		
		pos 		= self.scrollbar.GetPos()
		initIndex	= int(pos * (len(self.valuesDict) - len(self.elements) ))
		
		if len(self.elements) >= len(self.valuesDict):
			initIndex = 0
		
		sorted_key = self.valuesDict.keys()
		sorted_key.sort()
		
		for x in xrange(initIndex , initIndex + len(self.elements)):
			index 	= sorted_key[x]
			text	= self.valuesDict[index]
			
			self.elements[x-initIndex].SetElement(index, text)
	
	
	
	
	def SetValueDict(self, dct):
		self.valuesDict = dct
		self.__refreshViewList()
	
	
	def SetSelectEvent(self, event):
		self.onSelectEvent = event	
	
	
	def __del__(self):
		self.background 	= None
		self.elements		= []
		self.scrollbar		= None
		self.onSelectEvent	= None
		
		ui.Window.__del__(self)
	

class FilterHistoryElement(ui.Window):
	

	
	def __init__(self, info):
		ui.Window.__init__(self)

		self.button = None
		self.datetext = None
		self.timetext = None
		self.counttext = None
		self.buttonEvent = None
		self.info = {}



		self.__loadButton()
		self.__loadDateText(info)
		self.__loadTimeText(info)
		self.__loadCountText(info)
	
		self.info = info
	
	
	def GetInfo(self):
		return self.info
	

	def SetButtonEvent(self , event):
		self.buttonEvent = event
		self.button.SAFE_SetEvent(self.__OnClickMe)


	def __OnClickMe(self):
		if self.buttonEvent:
			self.buttonEvent(self)

	def __loadButton(self):
		button = ui.Button()
		button.SetParent(self)
		button.SetPosition(0,0)
		
		path = "offlineshop/searchhistory/element_%s.png"
		
		button.SetUpVisual(path%"default")
		button.SetDownVisual(path%"down")
		button.SetOverVisual(path%"over")
		button.Show()

		self.SetSize(button.GetWidth(), button.GetHeight())
		self.button = button
	
	def __loadDateText(self,info):
		text = ui.TextLine()
		text.SetParent(self.button)
		text.SetPosition(60, 3)
		
		day		= info["day"]
		month	= info["month"]
		year	= info["year"]
		
		text.SetText("%02d - %02d - %d "%(day, month, year))
		text.Show()

		self.datetext = text


	def __loadTimeText(self, info):
		text = ui.TextLine()
		text.SetParent(self.button)
		text.SetPosition(190+75, 3)


		hour 	= info["hour"]
		minute 	= info["minute"]

		text.SetText("%02d : %02d " % (hour, minute))
		text.Show()

		self.timetext = text
	
	def __loadCountText(self, info):
		text = ui.TextLine()
		text.SetParent(self.button)
		text.SetPosition(485, 3)

		count = info["count"]

		text.SetText(" %d " % (count))
		text.SetHorizontalAlignCenter()
		text.Show()

		self.counttext = text
	
	
	
	
	def __del__(self):
		self.button 	= None
		self.datetext	= None
		self.timetext	= None
		self.counttext	= None
		self.buttonEvent= None
		ui.Window.__del__(self)
	
	def IsInSlot(self):
		if not self.IsShow():
			return False

		if self.IsIn():
			return True
		
		if self.button:
			if self.button.IsIn():
				return True
		
		if self.datetext:
			if self.datetext.IsIn():
				return True
		
		if self.timetext:
			if self.timetext.IsIn():
				return True
		
		if self.counttext:
			if self.counttext.IsIn():
				return True
		
		return False

	def GetIndex(self):
		return self.info['id']


class FilterPatternElement(ui.Window):



	def __init__(self, info):
		ui.Window.__init__(self)

		self.button = None
		self.datetext = None
		self.nametext = None
		self.buttonEvent = None
		self.info = {}

		self.__loadButton()
		self.__loadDateText(info)
		self.__loadNameText(info)

		self.info = info


	def GetInfo(self):
		return self.info


	def SetButtonEvent(self , event):
		self.buttonEvent = event
		self.button.SAFE_SetEvent(self.__OnClickMe)


	def __OnClickMe(self):
		if self.buttonEvent:
			self.buttonEvent(self)

	def __loadButton(self):
		button = ui.Button()
		button.SetParent(self)
		button.SetPosition(0,0)

		path = "offlineshop/searchhistory/element_%s.png"

		button.SetUpVisual(path%"default")
		button.SetDownVisual(path%"down")
		button.SetOverVisual(path%"over")
		button.Show()

		self.SetSize(button.GetWidth(), button.GetHeight())
		self.button = button

	def __loadDateText(self,info):
		text = ui.TextLine()
		text.SetParent(self.button)
		text.SetPosition(352, 3)

		day		= info["day"]
		month	= info["month"]
		year	= info["year"]
		hour	= info["hour"]
		minute	= info["minute"]

		text.SetText("%02d - %02d - %d  %02d : %02d"%(day, month, year, hour, minute))
		text.Show()

		self.datetext = text


	def __loadNameText(self, info):
		text = ui.TextLine()
		text.SetParent(self.button)
		text.SetPosition(20, 3)

		text.SetText(info["name"])
		text.Show()

		self.nametext = text




	def __del__(self):
		self.button = None
		self.datetext = None
		self.nametext = None
		self.buttonEvent = None
		ui.Window.__del__(self)

	def IsInSlot(self):
		if not self.IsShow():
			return False

		if self.IsIn():
			return True

		if self.button:
			if self.button.IsIn():
				return True

		if self.datetext:
			if self.datetext.IsIn():
				return True

		if self.nametext:
			if self.nametext.IsIn():
				return True
		return False


	def GetIndex(self):
		return self.info['id']


class NewOfflineShopBoard(ui.ScriptWindow):
	
	
	#constants
	BOARD_KEYS = ("create_shop" , "my_shop", "open_shop", "shop_list", "search_history", )
	
	ITEM_TYPES = {
		item.ITEM_TYPE_NONE		: {
			"name" 		: localeinfo.OFFLINESHOP_TYPE_COMBOBOX_NOSET,
		},
		
		item.ITEM_TYPE_WEAPON		: {
			"name" : localeinfo.OFFLINESHOP_TYPE_COMBOBOX_WEAPON,
			"subtypes"	: {
				item.WEAPON_SWORD			:	localeinfo.OFFLINESHOP_COMBOBOX_WEAPON_SWORD,
				item.WEAPON_DAGGER			:	localeinfo.OFFLINESHOP_COMBOBOX_WEAPON_DAGGER,
				item.WEAPON_BOW				:	localeinfo.OFFLINESHOP_COMBOBOX_WEAPON_BOW,
				item.WEAPON_TWO_HANDED		:	localeinfo.OFFLINESHOP_COMBOBOX_WEAPON_TWO_HANDED,
				item.WEAPON_BELL			:	localeinfo.OFFLINESHOP_COMBOBOX_WEAPON_BELL,
				item.WEAPON_FAN				:	localeinfo.OFFLINESHOP_COMBOBOX_WEAPON_FAN,
			},
		},
		
		item.ITEM_TYPE_ARMOR		: {
			"name" : localeinfo.OFFLINESHOP_TYPE_COMBOBOX_ARMOR,
			"subtypes"	: {
				item.ARMOR_BODY			:	localeinfo.OFFLINESHOP_COMBOBOX_ARMOR_BODY,
				item.ARMOR_HEAD			:	localeinfo.OFFLINESHOP_COMBOBOX_ARMOR_HEAD,
				item.ARMOR_SHIELD		:	localeinfo.OFFLINESHOP_COMBOBOX_ARMOR_SHIELD,
				item.ARMOR_WRIST		:	localeinfo.OFFLINESHOP_COMBOBOX_ARMOR_WRIST,
				item.ARMOR_FOOTS		:	localeinfo.OFFLINESHOP_COMBOBOX_ARMOR_FOOTS,
				item.ARMOR_NECK			:	localeinfo.OFFLINESHOP_COMBOBOX_ARMOR_NECK,
				item.ARMOR_EAR			:	localeinfo.OFFLINESHOP_COMBOBOX_ARMOR_EAR,
			},
		},
		
		item.ITEM_TYPE_METIN		: {
			"name" : localeinfo.OFFLINESHOP_TYPE_COMBOBOX_METIN,
		},
		
		item.ITEM_TYPE_FISH		: {
			"name" : localeinfo.OFFLINESHOP_TYPE_COMBOBOX_FISH,
		},
		
		item.ITEM_TYPE_SKILLBOOK		: {
			"name" : localeinfo.OFFLINESHOP_TYPE_COMBOBOX_SKILLBOOK,
		},
		
		item.ITEM_TYPE_BLEND		: {
			"name" : localeinfo.OFFLINESHOP_TYPE_COMBOBOX_BLEND,
		},
		
		item.ITEM_TYPE_DS		: {
			"name" : localeinfo.OFFLINESHOP_TYPE_COMBOBOX_DS,
		},
		
		item.ITEM_TYPE_RING		: {
			"name" : localeinfo.OFFLINESHOP_TYPE_COMBOBOX_RING,
		},
		
		item.ITEM_TYPE_BELT		: {
			"name" : localeinfo.OFFLINESHOP_TYPE_COMBOBOX_BELT,
		},


		item.ITEM_TYPE_METIN		: {
			"name" : localeinfo.OFFLINESHOP_TYPE_COMBOBOX_METIN,
		},

		item.ITEM_TYPE_GIFTBOX		: {
			"name" : localeinfo.OFFLINESHOP_TYPE_COMBOBOX_GIFTBOX,
		},

		item.ITEM_TYPE_COSTUME		: {
			"name" : localeinfo.OFFLINESHOP_TYPE_COMBOBOX_COSTUME,
			"subtypes"	: {
				item.COSTUME_TYPE_BODY			:	localeinfo.OFFLINESHOP_COMBOBOX_COSTUME_BODY,
				item.COSTUME_TYPE_HAIR			:	localeinfo.OFFLINESHOP_COMBOBOX_COSTUME_HAIR,
			},
		},


		item.ITEM_TYPE_MATERIAL		:{
			"name": localeinfo.OFFLINESHOP_TYPE_COMBOBOX_REFINE,
		},
	}
	
	if ENABLE_WOLFMAN_CHARACTER:
		ITEM_TYPES[item.ITEM_TYPE_WEAPON]["subtypes"][item.WEAPON_CLAW] 	= localeinfo.OFFLINESHOP_COMBOBOX_WEAPON_CLAW
	
	# ensure costume subtypes dict exists
	if not ITEM_TYPES[item.ITEM_TYPE_COSTUME].has_key("subtypes"):
		ITEM_TYPES[item.ITEM_TYPE_COSTUME]["subtypes"] = {}

	if ENABLE_MOUNT_COSTUME_SYSTEM:
		ITEM_TYPES[item.ITEM_TYPE_COSTUME]["subtypes"][item.COSTUME_TYPE_MOUNT] 	= 	localeinfo.OFFLINESHOP_COMBOBOX_COSTUME_MOUNT
	
	if ENABLE_ACCE_COSTUME_SYSTEM:
		ITEM_TYPES[item.ITEM_TYPE_COSTUME]["subtypes"][item.COSTUME_TYPE_ACCE] 		= 	localeinfo.OFFLINESHOP_COMBOBOX_COSTUME_ACCE
	
	if ENABLE_WEAPON_COSTUME_SYSTEM:
		ITEM_TYPES[item.ITEM_TYPE_COSTUME]["subtypes"][item.COSTUME_TYPE_WEAPON] 	= 	localeinfo.OFFLINESHOP_COMBOBOX_COSTUME_WEAPON
	
	
	RARITY_GRADES = {
		-1 : localeinfo.OFFLINESHOP_RARITY_ITEM_UNSET,
		item.RARITY_COMMON : localeinfo.RARITY_ITEM_COMMON,
		item.RARITY_EPIC : localeinfo.RARITY_ITEM_EPIC,
		item.RARITY_LEGENDARY : localeinfo.RARITY_ITEM_LEGENDARY,
		item.RARITY_ANTIQUE : localeinfo.RARITY_ITEM_ANTIQUE,
		item.RARITY_MISTIC : localeinfo.RARITY_ITEM_MISTIC,
	}
	
	
	#members
	
	ATTRIBUTES = {
		0	: localeinfo.OFFLINESHOP_ATTR_UNSET,
		
		item.APPLY_MAX_HP 				: localeinfo.OFFLINESHOP_ATTR_MAX_HP,
		item.APPLY_MAX_SP 				: localeinfo.OFFLINESHOP_ATTR_MAX_SP,
		item.APPLY_CON 					: localeinfo.OFFLINESHOP_ATTR_CON,
		item.APPLY_INT 					: localeinfo.OFFLINESHOP_ATTR_INT,
		item.APPLY_STR 					: localeinfo.OFFLINESHOP_ATTR_STR,
		item.APPLY_DEX 					: localeinfo.OFFLINESHOP_ATTR_DEX,
		item.APPLY_ATT_SPEED 			: localeinfo.OFFLINESHOP_ATTR_ATT_SPEED,
		item.APPLY_MOV_SPEED 			: localeinfo.OFFLINESHOP_ATTR_MOV_SPEED,
		item.APPLY_CAST_SPEED 			: localeinfo.OFFLINESHOP_ATTR_CAST_SPEED,
		item.APPLY_HP_REGEN 			: localeinfo.OFFLINESHOP_ATTR_HP_REGEN,
		item.APPLY_SP_REGEN 			: localeinfo.OFFLINESHOP_ATTR_SP_REGEN,
		item.APPLY_POISON_PCT 			: localeinfo.OFFLINESHOP_ATTR_APPLY_POISON_PCT,
		item.APPLY_STUN_PCT 			: localeinfo.OFFLINESHOP_ATTR_APPLY_STUN_PCT,
		item.APPLY_SLOW_PCT 			: localeinfo.OFFLINESHOP_ATTR_APPLY_SLOW_PCT,
		item.APPLY_CRITICAL_PCT 		: localeinfo.OFFLINESHOP_ATTR_APPLY_CRITICAL_PCT,
		item.APPLY_PENETRATE_PCT 		: localeinfo.OFFLINESHOP_ATTR_APPLY_PENETRATE_PCT,

		item.APPLY_ATTBONUS_WARRIOR 	: localeinfo.OFFLINESHOP_ATTR_APPLY_ATTBONUS_WARRIOR,
		item.APPLY_ATTBONUS_ASSASSIN 	: localeinfo.OFFLINESHOP_ATTR_APPLY_ATTBONUS_ASSASSIN,
		item.APPLY_ATTBONUS_SURA 		: localeinfo.OFFLINESHOP_ATTR_APPLY_ATTBONUS_SURA,
		item.APPLY_ATTBONUS_SHAMAN 		: localeinfo.OFFLINESHOP_ATTR_APPLY_ATTBONUS_SHAMAN,
		item.APPLY_ATTBONUS_MONSTER 	: localeinfo.OFFLINESHOP_ATTR_APPLY_ATTBONUS_MONSTER,

		item.APPLY_ATTBONUS_HUMAN 		: localeinfo.OFFLINESHOP_ATTR_APPLY_ATTBONUS_HUMAN,
		item.APPLY_ATTBONUS_ANIMAL 		: localeinfo.OFFLINESHOP_ATTR_APPLY_ATTBONUS_ANIMAL,
		item.APPLY_ATTBONUS_ORC 		: localeinfo.OFFLINESHOP_ATTR_APPLY_ATTBONUS_ORC,
		item.APPLY_ATTBONUS_MILGYO 		: localeinfo.OFFLINESHOP_ATTR_APPLY_ATTBONUS_MILGYO,
		item.APPLY_ATTBONUS_UNDEAD 		: localeinfo.OFFLINESHOP_ATTR_APPLY_ATTBONUS_UNDEAD,
		item.APPLY_ATTBONUS_DEVIL 		: localeinfo.OFFLINESHOP_ATTR_APPLY_ATTBONUS_DEVIL,
		item.APPLY_STEAL_HP 			: localeinfo.OFFLINESHOP_ATTR_APPLY_STEAL_HP,
		item.APPLY_STEAL_SP 			: localeinfo.OFFLINESHOP_ATTR_APPLY_STEAL_SP,
		item.APPLY_MANA_BURN_PCT 		: localeinfo.OFFLINESHOP_ATTR_APPLY_MANA_BURN_PCT,
		item.APPLY_DAMAGE_SP_RECOVER 	: localeinfo.OFFLINESHOP_ATTR_APPLY_DAMAGE_SP_RECOVER,
		item.APPLY_BLOCK 				: localeinfo.OFFLINESHOP_ATTR_APPLY_BLOCK,
		item.APPLY_DODGE 				: localeinfo.OFFLINESHOP_ATTR_APPLY_DODGE,
		item.APPLY_RESIST_SWORD 		: localeinfo.OFFLINESHOP_ATTR_APPLY_RESIST_SWORD,
		item.APPLY_RESIST_TWOHAND 		: localeinfo.OFFLINESHOP_ATTR_APPLY_RESIST_TWOHAND,
		item.APPLY_RESIST_DAGGER 		: localeinfo.OFFLINESHOP_ATTR_APPLY_RESIST_DAGGER,
		item.APPLY_RESIST_BELL 			: localeinfo.OFFLINESHOP_ATTR_APPLY_RESIST_BELL,
		item.APPLY_RESIST_FAN 			: localeinfo.OFFLINESHOP_ATTR_APPLY_RESIST_FAN,
		item.APPLY_RESIST_BOW 			: localeinfo.OFFLINESHOP_ATTR_RESIST_BOW,
		# item.APPLY_RESIST_FIRE 		: localeinfo.OFFLINESHOP_ATTR_RESIST_FIRE,
		# item.APPLY_RESIST_ELEC 		: localeinfo.OFFLINESHOP_ATTR_RESIST_ELEC,
		item.APPLY_RESIST_MAGIC 		: localeinfo.OFFLINESHOP_ATTR_RESIST_MAGIC,
		# item.APPLY_RESIST_WIND 		: localeinfo.OFFLINESHOP_ATTR_APPLY_RESIST_WIND,
		item.APPLY_REFLECT_MELEE 		: localeinfo.OFFLINESHOP_ATTR_APPLY_REFLECT_MELEE,
		# item.APPLY_REFLECT_CURSE 		: localeinfo.OFFLINESHOP_ATTR_APPLY_REFLECT_CURSE,
		item.APPLY_POISON_REDUCE 		: localeinfo.OFFLINESHOP_ATTR_APPLY_POISON_REDUCE,
		# item.APPLY_KILL_SP_RECOVER 	: localeinfo.OFFLINESHOP_ATTR_APPLY_KILL_SP_RECOVER,
		item.APPLY_EXP_DOUBLE_BONUS 	: localeinfo.OFFLINESHOP_ATTR_APPLY_EXP_DOUBLE_BONUS,
		item.APPLY_GOLD_DOUBLE_BONUS 	: localeinfo.OFFLINESHOP_ATTR_APPLY_GOLD_DOUBLE_BONUS,
		item.APPLY_ITEM_DROP_BONUS 		: localeinfo.OFFLINESHOP_ATTR_APPLY_ITEM_DROP_BONUS,
		# item.APPLY_POTION_BONUS 		: localeinfo.OFFLINESHOP_ATTR_APPLY_POTION_BONUS,
		# item.APPLY_KILL_HP_RECOVER 	: localeinfo.OFFLINESHOP_ATTR_APPLY_KILL_HP_RECOVER,
		item.APPLY_IMMUNE_STUN 			: localeinfo.OFFLINESHOP_ATTR_APPLY_IMMUNE_STUN,
		item.APPLY_IMMUNE_SLOW 			: localeinfo.OFFLINESHOP_ATTR_APPLY_IMMUNE_SLOW,
		item.APPLY_IMMUNE_FALL 			: localeinfo.OFFLINESHOP_ATTR_APPLY_IMMUNE_FALL,
		# item.APPLY_BOW_DISTANCE 		: localeinfo.OFFLINESHOP_ATTR_BOW_DISTANCE,
		item.APPLY_DEF_GRADE_BONUS 		: localeinfo.OFFLINESHOP_ATTR_DEF_GRADE,
		item.APPLY_ATT_GRADE_BONUS 		: localeinfo.OFFLINESHOP_ATTR_ATT_GRADE,
		# item.APPLY_MAGIC_ATT_GRADE 	: localeinfo.OFFLINESHOP_ATTR_MAGIC_ATT_GRADE,
		# item.APPLY_MAGIC_DEF_GRADE 	: localeinfo.OFFLINESHOP_ATTR_MAGIC_DEF_GRADE,
		# item.APPLY_MAX_STAMINA 		: localeinfo.OFFLINESHOP_ATTR_MAX_STAMINA,
		# item.APPLY_MALL_ATTBONUS 		: localeinfo.OFFLINESHOP_ATTR_MALL_ATTBONUS,
		# item.APPLY_MALL_DEFBONUS 		: localeinfo.OFFLINESHOP_ATTR_MALL_DEFBONUS,
		# item.APPLY_MALL_EXPBONUS 		: localeinfo.OFFLINESHOP_ATTR_MALL_EXPBONUS,
		# item.APPLY_MALL_ITEMBONUS 	: localeinfo.OFFLINESHOP_ATTR_MALL_ITEMBONUS,
		# item.APPLY_MALL_GOLDBONUS 	: localeinfo.OFFLINESHOP_ATTR_MALL_GOLDBONUS,
		item.APPLY_SKILL_DAMAGE_BONUS 	: localeinfo.OFFLINESHOP_ATTR_SKILL_DAMAGE_BONUS,
		
		
		item.APPLY_NORMAL_HIT_DAMAGE_BONUS 	: localeinfo.OFFLINESHOP_ATTR_NORMAL_HIT_DAMAGE_BONUS,
		item.APPLY_SKILL_DEFEND_BONUS 		: localeinfo.OFFLINESHOP_ATTR_SKILL_DEFEND_BONUS,
		item.APPLY_NORMAL_HIT_DEFEND_BONUS 	: localeinfo.OFFLINESHOP_ATTR_NORMAL_HIT_DEFEND_BONUS,
		# item.APPLY_PC_BANG_EXP_BONUS 		: localeinfo.OFFLINESHOP_ATTR_MALL_EXPBONUS_P_STATIC,
		# item.APPLY_PC_BANG_DROP_BONUS 	: localeinfo.OFFLINESHOP_ATTR_MALL_ITEMBONUS_P_STATIC,
		# item.APPLY_RESIST_WARRIOR 		: localeinfo.OFFLINESHOP_ATTR_APPLY_RESIST_WARRIOR,
		# item.APPLY_RESIST_ASSASSIN 		: localeinfo.OFFLINESHOP_ATTR_APPLY_RESIST_ASSASSIN,
		# item.APPLY_RESIST_SURA 			: localeinfo.OFFLINESHOP_ATTR_APPLY_RESIST_SURA,
		# item.APPLY_RESIST_SHAMAN 			: localeinfo.OFFLINESHOP_ATTR_APPLY_RESIST_SHAMAN,
		# item.APPLY_MAX_HP_PCT 			: localeinfo.OFFLINESHOP_ATTR_APPLY_MAX_HP_PCT,
		# item.APPLY_MAX_SP_PCT 			: localeinfo.OFFLINESHOP_ATTR_APPLY_MAX_SP_PCT,
		# item.APPLY_ENERGY 				: localeinfo.OFFLINESHOP_ATTR_ENERGY,
		# item.APPLY_COSTUME_ATTR_BONUS 	: localeinfo.OFFLINESHOP_ATTR_COSTUME_ATTR_BONUS,
		

		# item.APPLY_MAGIC_ATTBONUS_PER 		: localeinfo.OFFLINESHOP_ATTR_MAGIC_ATTBONUS_PER,
		# item.APPLY_MELEE_MAGIC_ATTBONUS_PER 	: localeinfo.OFFLINESHOP_ATTR_MELEE_MAGIC_ATTBONUS_PER,
		# item.APPLY_RESIST_ICE 				: localeinfo.OFFLINESHOP_ATTR_RESIST_ICE,
		# item.APPLY_RESIST_EARTH 				: localeinfo.OFFLINESHOP_ATTR_RESIST_EARTH,
		# item.APPLY_RESIST_DARK 				: localeinfo.OFFLINESHOP_ATTR_RESIST_DARK,
		# item.APPLY_ANTI_CRITICAL_PCT 			: localeinfo.OFFLINESHOP_ATTR_ANTI_CRITICAL_PCT,
		# item.APPLY_ANTI_PENETRATE_PCT 		: localeinfo.OFFLINESHOP_ATTR_ANTI_PENETRATE_PCT,
	}
	if ENABLE_WOLFMAN_CHARACTER:
		ATTRIBUTES.update({
			item.APPLY_BLEEDING_PCT 		: localeinfo.OFFLINESHOP_ATTR_APPLY_BLEEDING_PCT,
			item.APPLY_BLEEDING_REDUCE 		: localeinfo.OFFLINESHOP_ATTR_APPLY_BLEEDING_REDUCE,
			item.APPLY_ATTBONUS_WOLFMAN 	: localeinfo.OFFLINESHOP_ATTR_APPLY_ATTBONUS_WOLFMAN,
			item.APPLY_RESIST_CLAW 			: localeinfo.OFFLINESHOP_ATTR_APPLY_RESIST_CLAW,
			item.APPLY_RESIST_WOLFMAN 		: localeinfo.OFFLINESHOP_ATTR_APPLY_RESIST_WOLFMAN,
		})

	if ENABLE_MAGIC_REDUCTION_SYSTEM:
		ATTRIBUTES.update({
			item.APPLY_RESIST_MAGIC_REDUCTION : localeinfo.OFFLINESHOP_ATTR_RESIST_MAGIC_REDUCTION,
		})

	try:
		if app.__ENABLE_FIFTH_STATUS__:
			ATTRIBUTES[item.APPLY_RES] 				= localeinfo.OFFLINESHOP_ATTR_RES
			ATTRIBUTES[item.APPLY_HARDNESS_GRADE] 	= localeinfo.OFFLINESHOP_ATTR_HARDNESS_GRADE
	except:
		pass


		
	
	
	
	
	




	def __init__(self):
		ui.ScriptWindow.__init__(self)

		self.clear()

		offlineshop.SetOfflineshopBoard(self)
		self.__loadWindow()
		self.__LoadLastPrices()

	def clear(self):
		# menu
		self.MyShopButton = None
		self.ListOfShopButton = None
		self.SearchFilterButton = None
		self.SearchHistoryButton = None
		self.MyPatternsButton = None

		self.MyAuctionButton = None
		self.ListOfAuctionsButton = None
		


		# common
		self.pageBoards = {}
		self.pageCategory = "my_shop"
		self.updateEvents = {}
		self.itemTooltip = None
		self.popupMessage = None
		self.ShopItemForSale = []
		self.ShopItemSold = []
		self.EditPriceSlot = None
		self.AddItemSlotIndex = -1
		self.CommonInputPriceDlg = None
		self.CommonQuestionDlg = None
		self.CommonPickValuteDlg = None
		self.TitleBar = None
		# offline shop: remember last sell price per item vnum (client-side autofill)
		self._lastPriceByVnum = {}
		self._lastPriceStorePath = ""
		# offline shop: last seen market averages (computed from search results)
		self._marketAvgCache = {}

		# offline shop: silent market avg refresh query (does not touch search UI)
		self._avgPriceQueryActive = False
		self._avgPriceQueryVnum = 0
		self._avgPriceQueryCount = 1
		self._avgPriceQueryBackupResult = None
		self._avgPriceQueryLastSend = 0.0
		self._avgPriceQueryMinInterval = 2.3  # must be >= OFFLINESHOP_SECONDS_PER_SEARCH
		self._avgPriceCacheTTL = 60.0  # seconds
		
		

		# filtering
		self.SearchFilterShopItemResult = []
		self.FilterHistory = []
		self.FilterPatterns = {}

		# shoplist
		self.ShopList = []
		self.ShopOpenInfo = {}
		self.ShopListTable = None
		self.ViewedShopOwners = set()

		# create shop member
		self.InsertedItems = []
		self.CreateShopNameEdit = None
		self.CreateShopDaysCountText = None
		self.CreateShopHoursCountText = None
		self.CreateShopItemsTable = None
		self.CreateShopItemsInfos = {}

		# myshop page member
		self.MyShopItemsTable = None
		self.MyShopShopTitle = None
		self.MyShopShopDuration = None
		self.MyShopCloseButton = None
		self.MyShopEditNameDlg = None
		self.MyShopOffers = None
		self.MyShopOffersTable = None

		# open shop page member
		self.OpenShopItemsTable = None
		self.OpenShopBackToListButton = None
		self.OpenShopShopTitle = None
		self.OpenShopShopDuration = None
		self.OpenShopBuyItemID = -1
		self.BuyPriceSeenTotal = 0

		# search & filter page
		self.SearchFilterItemsNameDict = {}
		self.SearchFilterCheckBoxesRace = {}
		self.SearchFilterCheckBoxes = {}
		self.SearchFilterAttributeButtons = []

		self.SearchFilterComboBoxSuggestion = None
		self.SearchFilterSuggestionObj = None
		self.SearchFilterItemNameInput = None
		self.SearchFilterItemLevelStart = None
		self.SearchFilterItemLevelEnd = None
		self.SearchFilterItemYangMin = None
		self.SearchFilterItemYangMax = None
		self.SearchFilterResetFilterButton = None
		self.SearchFilterSavePatternButton = None
		self.SearchFilterStartSearch = None

		# important quick filters (no bonus-search)
		self.SearchFilterImportantMountsButton = None
		self.SearchFilterImportantLegendaryButton = None
		self.SearchFilterImportantLegendaryChangeButton = None
		self.SearchFilterImportantTalismanButton = None
		self.SearchFilterImportantTalismanEnchantButton = None
		self.SearchFilterImportantOtherButton = None

		self._importantMultiActive = False
		self._importantMultiQueue = []
		self._importantMultiResults = []
		self._importantMultiSet = set()
		self._importantMultiCurrentVnum = 0
		# important multi-search pacing (server has OFFLINESHOP_SECONDS_PER_SEARCH cooldown)
		self._importantMultiWaitUntil = 0.0
		self._importantMultiDelay = 2.2


		self.ShowGradeFilterButton = None
		self.GradeFilterWindow = None
		self.HideGradeFilterButton = None
		self.GradeFilterBackground = None
		self.GradeFilterCheckBox = None
		self.GradeFilterSuggestions = None
		self.GradeFilterButtonSelect = None
		self.GradeFilterWindowSettings = {}
		self.GradeFilterSuggestionsIndex = -1
		

		self.SearchFilterResultItemsTable = None

		# Search History page
		self.SearchHistoryTable = None

		# mypattern page
		self.SearchPatternsTable = None
		self.SearchPatternsInputNameDlg = None

		# shop safebox
		self.ShopSafeboxItems = []
		self.ShopSafeboxItemsTable = None
		self.ShopSafeboxValuteAmount=0
		self.ShopSafeboxValuteText = None
		self.ShopSafeboxWithdrawYangButton = None
		if ENABLE_CHEQUE_SYSTEM:
			self.ShopSafeboxValuteTextCheque = None

		# my offers page
		self.MyOffersList = None
		self.MyOffersTable = None

		# my auction page
		self.MyAuctionInfo = {}
		self.MyAuctionOffers = []
		self.MyAuctionOfferTable = None

		self.MyAuctionOwnerName = None
		self.MyAuctionDuration = None
		self.MyAuctionBestOffer = None
		self.MyAuctionMinRaise = None
		self.MyAuctionSlot = None

		# open auction page
		self.OpenAuctionInfo = {}
		self.OpenAuctionOffers = []
		self.OpenAuctionOfferTable = None

		self.OpenAuctionOwnerName = None
		self.OpenAuctionDuration = None
		self.OpenAuctionBestOffer = None
		self.OpenAuctionMinRaise = None
		self.OpenAuctionSlot = None


		# auctionlist
		self.AuctionListInfo = {}
		self.AuctionListTable = None

		# create auction page
		self.CreateAuctionCreateAuctionButton = None
		self.CreateAuctionDaysInput = None
		self.CreateAuctionStartingPriceInput = None
		self.CreateAuctionSlot = None
		self.CreateAuctionDaysIncreaseButton = None
		self.CreateAuctionDaysDecreaseButton = None

		#refresh symbol
		self.RefreshSymbol = None
		
		if app.KASMIR_PAKET_SYSTEM:
			#self.wndRender = None
			#self.wndRenderPreview = None
			self.KasmirNpc = 0
			self.btnIncRender = None
			self.btnDecRender = None

	def Destroy(self):
		self.Hide()
		self.clear()
		self.ClearDictionary()

	#offlineshop-updated 05/08/19
	def OnPressEscapeKey(self):
		self.Close()
		return True

	def __del__(self):
		self.Destroy()
		print("------------ DESTROYED OFFLINESHOP INTERFACE ------------")
		ui.ScriptWindow.__del__(self)
	
	
	def __loadWindow(self):
		pyLoader = ui.PythonScriptLoader()
		pyLoader.LoadScriptFile(self , "uiscript/offlineshopwindow.py")
		
		childDict = {
			"create_shop"		: "MyShopBoardNoShop",
			"my_shop"			: "MyShopBoard",
			"open_shop"			: "ListOfShop_OpenShop",
			"shop_list"			: "ListOfShop_List",
			"shop_safebox"		: "ShopSafeboxPage",
			"my_offers"			: "MyOffersPage",

			"search_history"	: "SearchHistoryBoard",
			"my_patterns"		: "MyPatternsBoard",
			"search_filter"		: "SearchFilterBoard",

			"my_auction"		: "MyAuction",
			"auction_list"		: "AuctionList",
			"open_auction"		: "OpenAuction",
			"create_auction"	: "CreateAuction",

		}
		
		
		self.pageBoards = {}
		
		for k, v in childDict.items():
			self.pageBoards[k] = self.GetChild(v)
			self.pageBoards[k].Hide()
		
		if app.KASMIR_PAKET_SYSTEM:
			# Default shop skin
			self.KasmirNpc = 30000
			
			# Style buttons (optional - depends on uiscript/offlineshopwindow.py)
			try:
				self.btnIncRender = self.GetChild("IncreaseStyleButton")
				self.btnDecRender = self.GetChild("DecreaseStyleButton")
				self.btnIncRender.SAFE_SetEvent(self.OnClickIncBtn)
				self.btnDecRender.SAFE_SetEvent(self.OnClickDecBtn)
				self.btnIncRender.Show()
				self.btnDecRender.Show()
			except:
				self.btnIncRender = None
				self.btnDecRender = None
			
			# Simple image preview (NO RenderTarget)
			self.wndStylePreview = ui.ImageBox()
			self.wndStylePreview.SetParent(self.pageBoards["create_shop"])
			self.wndStylePreview.SetPosition(self.pageBoards["create_shop"].GetWidth() - 160, 12)
			self.__UpdateKasmirStylePreview()
			self.wndStylePreview.Show()

		#refresh symbol
		self.RefreshSymbol = self.GetChild("RefreshSymbol")


		#create shop page
		# self.HowToInsertItemText			= self.GetChild("HowToInsertItems")
		self.CreateShopNameEdit				= self.GetChild("ShopNameInput")
		self.CreateShopDaysCountText		= self.GetChild("DaysCountText")
		self.CreateShopHoursCountText		= self.GetChild("HoursCountText")
		
		self.CreateShopIncreaseDaysButton	= self.GetChild("IncreaseDaysButton")
		self.CreateShopDecreaseDaysButton	= self.GetChild("DecreaseDaysButton")
		
		self.CreateShopIncreaseHoursButton	= self.GetChild("IncreaseHoursButton")
		self.CreateShopDecreaseHoursButton	= self.GetChild("DecreaseHoursButton")
		
		self.CreateShopButton				= self.GetChild("CreateShopButton")
		
		self.CreateShopIncreaseDaysButton.SAFE_SetEvent(self.__OnClickCreateShopIncreaseDaysButton)
		self.CreateShopDecreaseDaysButton.SAFE_SetEvent(self.__OnClickCreateShopDecreaseDaysButton)
		
		self.CreateShopIncreaseHoursButton.SAFE_SetEvent(self.__OnClickCreateShopIncreaseHoursButton)
		self.CreateShopDecreaseHoursButton.SAFE_SetEvent(self.__OnClickCreateShopDecreaseHoursButton)
		
		
		self.CreateShopButton.SAFE_SetEvent(self.__OnClickCreateShopButton)
		self.__MakeCreateShopItemsTable()
		
		
		#myshop page
		self.MyShopShopDuration		= self.GetChild("MyShopShopDuration")
		self.MyShopShopTitle		= self.GetChild("MyShopShopTitle")
		self.MyShopCloseButton		= self.GetChild("MyShopCloseButton")
		self.MyShopEditTitleButton	= self.GetChild("MyShopEditTitleButton")
		self.__MakeMyShopItemsTable()
		self.__MakeMyShopOffersTable()
		
		self.MyShopCloseButton.SAFE_SetEvent(self.__OnClickCloseButton)
		self.MyShopEditTitleButton.SAFE_SetEvent(self.__OnClickMyShopEditNameButton)
		
		self.MyShopEditNameDlg	= uicommon.InputDialogWithDescription()
		self.MyShopEditNameDlg.SetMaxLength(35)
		self.MyShopEditNameDlg.SetDescription(localeinfo.OFFLINESHOP_EDIT_SHOPNAME_DESCRIPTION)
		self.MyShopEditNameDlg.SetAcceptEvent(self.__OnAcceptChangeShopNameDlg)
		self.MyShopEditNameDlg.SetCancelEvent(self.__OnCancelChangeShopNameDlg)
		self.MyShopEditNameDlg.SetTitle(localeinfo.OFFLINESHOP_EDIT_SHOPNAME_TITLE)
		self.MyShopEditNameDlg.Hide()
		
		
		
		
		#shoplist page
		self.__MakeShopListTable()
		
		
		#OpenShop page
		self.OpenShopBackToListButton	= self.GetChild("OpenShopBackToListButton")
		self.OpenShopShopTitle			= self.GetChild("OpenShopShopTitle")
		self.OpenShopShopDuration		= self.GetChild("OpenShopShopDuration")
		
		self.CommonQuestionDlg 	= uicommon.QuestionDialog()

		self.CommonPickValuteDlg = uipickmoney.PickMoneyDialog()
		self.CommonPickValuteDlg.LoadDialog()

		self.OpenShopBackToListButton.SAFE_SetEvent(self.__OnClickShopListPage)
		self.__MakeOpenShopItemsTable()


		self.TitleBar = self.GetChild("TitleBar")
		self.TitleBar.SetCloseEvent(self.Close)

		#changepage buttons
		self.MyShopButton			= self.GetChild("MyShopButton")
		self.ListOfShopButton		= self.GetChild("ListOfShopButton")
		self.ShopSafeboxButton		= self.GetChild("ShopSafeboxButton")
		self.MyOffersPageButton		= self.GetChild("MyOffersPageButton")

		self.SearchFilterButton		= self.GetChild("SearchFilterButton")
		self.SearchHistoryButton	= self.GetChild("SearchHistoryButton")
		self.MyPatternsButton		= self.GetChild("MyPatternsButton")
		
		self.MyAuctionButton		= self.GetChild("MyAuctionButton")
		self.ListOfAuctionsButton	= self.GetChild("ListOfAuctionsButton")
		
		
		
		
		#events setting
		self.MyShopButton.SAFE_SetEvent(self.__OnClickMyShopPage)
		self.ListOfShopButton.SAFE_SetEvent(self.__OnClickShopListPage)
		self.ShopSafeboxButton.SAFE_SetEvent(self.__OnClickShopSafeboxPage)
		self.MyOffersPageButton.SAFE_SetEvent(self.__OnClickMyOffersPage)

		self.SearchFilterButton.SAFE_SetEvent(self.__OnClickSearchFilterPage)
		self.SearchHistoryButton.SAFE_SetEvent(self.__OnClickSearchHistoryPage)
		self.MyPatternsButton.SAFE_SetEvent(self.__OnClickMyPatternsPage)
		
		self.MyAuctionButton.SAFE_SetEvent(self.__OnClickMyAuctionPage)
		self.ListOfAuctionsButton.SAFE_SetEvent(self.__OnClickAuctionListPage)
		
		
		
		

		
		
		#updateEvents
		self.updateEvents = {
			"my_shop"		: self.__OnUpdateMyShopPage,
			"create_shop"	: self.__OnUpdateCreateShopPage,
			"open_shop"		: self.__OnUpdateOpenShopPage,
			"search_filter"	: self.__OnUpdateSearchFilterPage,
			"shop_safebox"	: self.__OnUpdateShopSafeboxPage,
			"my_offers"		: self.__OnUpdateMyOffersPage,
			"search_history": self.__OnUpdateSearchHistoryPage,
			"my_patterns"	: self.__OnUpdateMyPatternPage,
			"create_auction": self.__OnUpdateCreateAuctionPage,
			"my_auction"	: self.__OnUpdateMyAuctionPage,
			"open_auction"	: self.__OnUpdateOpenAuctionPage,
			"auction_list"  : self.__OnUpdateAuctionListPage,
		}
		
		
		#item tooltip
		tooltip = uitooltip.ItemToolTip(width=300)
		tooltip.ClearToolTip()
		tooltip.SetFollow(True)
		tooltip.Hide()
		self.itemTooltip = tooltip
		
		#popup message
		popup = uicommon.PopupDialog()
		popup.SetWidth(250)
		popup.Hide()
		self.popupMessage = popup
		
		#input price
		self.CommonInputPriceDlg =  uicommon.MoneyInputDialogCheque() if ENABLE_CHEQUE_SYSTEM  else uicommon.MoneyInputDialog()
		self.CommonInputPriceDlg.SetAcceptEvent(self.__OnAcceptInputPrice)
		self.CommonInputPriceDlg.SetCancelEvent(self.__OnCancelInputPrice)
		
		
		#search filter page
		offlineshop.RefreshItemNameMap()
		self.__MakeSearchFilterResultItemsTable()

		self.SearchFilterItemNameInput 		= self.GetChild("SearchFilterItemNameInput")
		self.SearchFilterItemLevelStart		= self.GetChild("SearchFilterItemLevelStart")
		self.SearchFilterItemLevelEnd		= self.GetChild("SearchFilterItemLevelEnd")
		self.SearchFilterItemYangMin		= self.GetChild("SearchFilterItemYangMin")
		self.SearchFilterItemYangMax		= self.GetChild("SearchFilterItemYangMax")
		self.SearchFilterResetFilterButton	= self.GetChild("SearchFilterResetFilterButton")
		self.SearchFilterSavePatternButton	= self.GetChild("SearchFilterSavePatternButton")
		self.SearchFilterStartSearch		= self.GetChild("SearchFilterStartSearch")
		self.__MakeGradeFilterWindow()
		
		self.__MakeSearchFilterCheckBoxes()
		self.SearchFilterComboBoxSuggestion	= ui.ComboBox()
		self.SearchFilterComboBoxSuggestion.SetParent(self.pageBoards["search_filter"])
		self.SearchFilterComboBoxSuggestion.SetPosition(75,50)
		self.SearchFilterComboBoxSuggestion.SetSize(130,17)
		self.SearchFilterComboBoxSuggestion.Show()
		
		
		self.SearchFilterSuggestionObj = Suggestions()
		self.SearchFilterSuggestionObj.SetComboBox(self.SearchFilterComboBoxSuggestion)
		self.SearchFilterSuggestionObj.SetInputBox(self.SearchFilterItemNameInput)
		self.SearchFilterSuggestionObj.SetMainDict(self.SearchFilterItemsNameDict)
		
		self.SearchFilterResetFilterButton.SAFE_SetEvent(self.__OnClickSearchFilterResetFilterButton)
		self.SearchFilterSavePatternButton.SAFE_SetEvent(self.__OnClickSearchFilterSavePatternButton)
		self.SearchFilterStartSearch.SAFE_SetEvent(self.__OnClickSearchFilterStartSearch)
		
		
		#subtype
		self.SearchFilterSubTypeComboBox = ui.ComboBox()
		self.SearchFilterSubTypeComboBox.SetParent(self.pageBoards["search_filter"])
		self.SearchFilterSubTypeComboBox.SetPosition(220,50)
		self.SearchFilterSubTypeComboBox.SetSize(100,17)
		self.SearchFilterSubTypeComboBox.Show()
		self.SearchFilterSubTypeComboBox.InsertItem(SUBTYPE_NOSET , localeinfo.OFFLINESHOP_TYPE_COMBOBOX_NOSET)
		self.SearchFilterSubTypeComboBox.SetCurrentItem(localeinfo.OFFLINESHOP_TYPE_COMBOBOX_NOSET)
		
		#type
		self.SearchFilterTypeComboBox = ui.ComboBox()
		self.SearchFilterTypeComboBox.SetParent(self.pageBoards["search_filter"])
		self.SearchFilterTypeComboBox.SetPosition(220,29)
		self.SearchFilterTypeComboBox.SetSize(100,17)
		self.SearchFilterTypeComboBox.Show()
		
		#insert types
		for k,v in self.ITEM_TYPES.items():
			self.SearchFilterTypeComboBox.InsertItem(k, v["name"])
		
		self.SearchFilterTypeComboBox.SetCurrentItem(localeinfo.OFFLINESHOP_TYPE_COMBOBOX_NOSET)
		
		
		
		
		self.SearchFilterTypeComboBox.SetEvent(self.__OnSelectSearchFilterTypeComboBox)
		self.SearchFilterSubTypeComboBox.SetEvent(self.__OnSelectSearchFilterSubTypeComboBox)
		
		self.SearchFilterTypeComboBoxIndex = 0
		self.SearchFilterSubTypeComboBoxIndex = SUBTYPE_NOSET
		
		
		# fontos gombok (gyors szurok)
		try:
			self.SearchFilterImportantMountsButton = self.GetChild("SearchFilterImportantMountsButton")
			self.SearchFilterImportantLegendaryButton = self.GetChild("SearchFilterImportantLegendaryButton")
			self.SearchFilterImportantLegendaryChangeButton = self.GetChild("SearchFilterImportantLegendaryChangeButton")
			self.SearchFilterImportantTalismanButton = self.GetChild("SearchFilterImportantTalismanButton")
			self.SearchFilterImportantTalismanEnchantButton = self.GetChild("SearchFilterImportantTalismanEnchantButton")
			self.SearchFilterImportantOtherButton = self.GetChild("SearchFilterImportantOtherButton")

			self.SearchFilterImportantMountsButton.SAFE_SetEvent(self.__OnClickSearchFilterImportantMounts)
			self.SearchFilterImportantLegendaryButton.SAFE_SetEvent(self.__OnClickSearchFilterImportantLegendary)
			self.SearchFilterImportantLegendaryChangeButton.SAFE_SetEvent(self.__OnClickSearchFilterImportantLegendaryChange)
			self.SearchFilterImportantTalismanButton.SAFE_SetEvent(self.__OnClickSearchFilterImportantTalisman)
			self.SearchFilterImportantTalismanEnchantButton.SAFE_SetEvent(self.__OnClickSearchFilterImportantTalismanEnchant)
			self.SearchFilterImportantOtherButton.SAFE_SetEvent(self.__OnClickSearchFilterImportantOther)
		except:
			self.SearchFilterImportantMountsButton = None
			self.SearchFilterImportantLegendaryButton = None
			self.SearchFilterImportantLegendaryChangeButton = None
			self.SearchFilterImportantTalismanButton = None
			self.SearchFilterImportantTalismanEnchantButton = None
			self.SearchFilterImportantOtherButton = None

		self.SearchFilterSuggestionSelector = None
		self.SearchFilterAttributeSetting = []


		#search history page
		self.__MakeSearchHistoryTable()


		#search pattern page
		self.__MakeSearchPatternsTable()
		self.SearchPatternsInputNameDlg = uicommon.InputDialogWithDescription()
		self.SearchPatternsInputNameDlg.SetMaxLength(25)
		self.SearchPatternsInputNameDlg.SetDescription(localeinfo.OFFLINESHOP_MY_PATTERN_INSERT_NAME_DESC)
		self.SearchPatternsInputNameDlg.SetAcceptEvent(self.__OnAcceptMyPatternInputName)
		self.SearchPatternsInputNameDlg.SetCancelEvent(self.__OnCancelMyPatternInputName)
		self.SearchPatternsInputNameDlg.SetTitle(localeinfo.OFFLINESHOP_MY_PATTERN_INSERT_NAME_TITLE)
		self.SearchPatternsInputNameDlg.Hide()



		#safebox page
		self.__MakeShopSafeboxItemsTable()
		self.ShopSafeboxValuteText 			= self.GetChild("ShopSafeboxValuteText")
		self.ShopSafeboxWithdrawYangButton	= self.GetChild("ShopSafeboxWithdrawYangButton")

		self.ShopSafeboxWithdrawYangButton.SAFE_SetEvent(self.__OnClickShopSafeboxWithdrawYang)
		if ENABLE_CHEQUE_SYSTEM:
			self.ShopSafeboxValuteTextCheque= self.GetChild("ShopSafeboxValuteTextCheque")

		#myoffers page
		self.__MakeMyOffersTable()


		#myAuction page
		self.MyAuctionOwnerName 	= self.GetChild("MyAuction_OwnerName")
		self.MyAuctionDuration 		= self.GetChild("MyAuction_Duration")
		self.MyAuctionBestOffer 	= self.GetChild("MyAuction_BestOffer")
		self.MyAuctionMinRaise 		= self.GetChild("MyAuction_MinRaise")
		
		self.__MakeMyAuctionOffersTable()


		#open auction page
		self.OpenAuctionOwnerName	= self.GetChild("OpenAuction_OwnerName")
		self.OpenAuctionDuration 	= self.GetChild("OpenAuction_Duration")
		self.OpenAuctionBestOffer 	= self.GetChild("OpenAuction_BestOffer")
		self.OpenAuctionMinRaise 	= self.GetChild("OpenAuction_MinRaise")
		#updated 11.01.2020
		self.OpenAuctionBackToListButton = self.GetChild("OpenAuctionBackToListButton")
		self.OpenAuctionBackToListButton.SAFE_SetEvent(self.__OnClickAuctionListPage)

		if ENABLE_CHEQUE_SYSTEM:
			self.ShopSafeboxValuteTextCheque= self.GetChild("ShopSafeboxValuteTextCheque")
		self.__MakeOpenAuctionOffersTable()

		#auction list page
		self.__MakeAuctionListTable()


		#create auction page
		self.CreateAuctionCreateAuctionButton 	= self.GetChild("CreateAuctionCreateAuctionButton")
		self.CreateAuctionDaysInput 			= self.GetChild("CreateAuctionDaysInput")
		self.CreateAuctionStartingPriceInput 	= self.GetChild("CreateAuctionStartingPriceInput")

		self.CreateAuctionWindowPos =-1
		self.CreateAuctionSlotPos=-1

		self.CreateAuctionDaysDecreaseButton	= self.GetChild("CreateAuctionIncreaseDaysButton")
		self.CreateAuctionDaysIncreaseButton	= self.GetChild("CreateAuctionDecreaseDaysButton")

		self.CreateAuctionCreateAuctionButton.SAFE_SetEvent(self.__OnClickCreateAuctionButton)
		self.CreateAuctionDaysDecreaseButton.SAFE_SetEvent(self.__OnClickCreateAuctionDaysDecreaseButton)
		self.CreateAuctionDaysIncreaseButton.SAFE_SetEvent(self.__OnClickCreateAuctionDaysIncreaseButton)
		
		self.CloseMyAuction = self.GetChild("MyAuction_Close")
		self.CloseMyAuction.SAFE_SetEvent(self.__OnClickMyAuctionClose)
		
		self.__MakeCreateAuctionSlot()

		#fixing escape key
		items = (
			self.CreateShopNameEdit,
			self.SearchFilterItemNameInput,
			self.SearchFilterItemLevelStart,
			self.SearchFilterItemLevelEnd,
			self.SearchFilterItemYangMin,
			self.SearchFilterItemYangMax,
			self.CreateAuctionStartingPriceInput,
		)

		autokill = lambda arg: arg.KillFocus()

		for item in items:
			item.OnPressEscapeKey = lambda : autokill(item)
		
		
		self.__UpdateAttachedWindows()

	def __UpdateKasmirStylePreview(self):
		"""No RenderTarget: show a per-skin image (ui/offlineshop/kasmir_30000.tga .. kasmir_30007.tga)."""
		if not app.KASMIR_PAKET_SYSTEM:
			return
		if not getattr(self, 'wndStylePreview', None):
			return
		try:
			v = getattr(self, 'KasmirNpc', 30000)
			if v < 30000 or v > 30007:
				v = 30000
				self.KasmirNpc = 30000
			img = 'd:/ymir work/razor93/kasmirfasz/kasmir_%d.tga' % v
			self.wndStylePreview.LoadImage(img)
		except:
			pass

	if app.KASMIR_PAKET_SYSTEM:
		def OnClickIncBtn(self):
			if 'create_shop' in self.pageBoards and self.pageBoards['create_shop'].IsShow():
				if self.KasmirNpc >= 30007:
					self.KasmirNpc = 30000
				else:
					self.KasmirNpc += 1
				self.__UpdateKasmirStylePreview()

		def OnClickDecBtn(self):
			if 'create_shop' in self.pageBoards and self.pageBoards['create_shop'].IsShow():
				if self.KasmirNpc <= 30000:
					self.KasmirNpc = 30007
				else:
					self.KasmirNpc -= 1
				self.__UpdateKasmirStylePreview()

	def OnTop(self):
		if self.pageCategory == "search_filter":
			if self.ShowGradeFilterButton and self.ShowGradeFilterButton.IsShow():
				self.ShowGradeFilterButton.SetTop()


	def Open(self):
		events = {
			"my_shop"			: self.__OnClickMyShopPage,
			"open_shop"			: self.__OnClickShopListPage,
			"shop_list"			: self.__OnClickShopListPage,
			"shop_safebox"		: self.__OnClickShopSafeboxPage,
			"my_offers"			: self.__OnClickMyOffersPage,

			"search_history"	: self.__OnClickSearchHistoryPage,
			"my_patterns"		: self.__OnClickMyPatternsPage,
			"search_filter"		: self.__OnClickSearchFilterPage,

			"my_auction"		: self.__OnClickMyAuctionPage,
			"auction_list"		: self.__OnClickAuctionListPage,
			"open_auction"		: self.__OnClickAuctionListPage,
		}

		if self.pageCategory in events.keys():
			events[self.pageCategory]()
		
		if app.KASMIR_PAKET_SYSTEM:
			if not hasattr(self, 'KasmirNpc') or self.KasmirNpc < 30000 or self.KasmirNpc > 30007:
				self.KasmirNpc = 30000
			self.__UpdateKasmirStylePreview()

		self.Show()


	def Close(self):
		self.Hide()
		self.itemTooltip.Hide()
		self.MyShopEditNameDlg.Hide()
		self.CommonQuestionDlg.Hide()
		self.CommonPickValuteDlg.Hide()
		self.__HidePriceDialog()

		try:
			self.__SaveLastPrices()
		except:
			pass

		offlineshop.SendCloseBoard()
		
		if self.pageCategory == "search_filter":
			if self.ShowGradeFilterButton:
				self.ShowGradeFilterButton.Hide()
			
			if self.GradeFilterWindow:
				self.GradeFilterWindow.Hide()
			
			if self.GradeFilterSuggestions:
				self.GradeFilterSuggestions.Hide()







	def __ResetCreateShopPage(self):
		# self.HowToInsertItemText.Show()
		self.CreateShopItemsTable.ClearElement()
		self.CreateShopItemsInfos = {}
		


	def OnMoveWindow(self, x, y):
		if self.pageCategory == "search_filter":
			self.__UpdateAttachedWindows()


	
	def __MakeCreateShopItemsTable(self):
		board = self.pageBoards["create_shop"]
		
		table = TableWindowWithScrollbar(575, 325, 11,3, TableWindowWithScrollbar.SCROLLBAR_HORIZONTAL)
		table.SetParent(board)
		table.SetPosition(25, 115)
		table.SetDefaultCreateChild(MakeDefaultEmptySlot, self.__OnClickCreateShopEmptySlot)
		
		table.Show()
		self.CreateShopItemsTable = table
	
	
	def __MakeMyShopItemsTable(self):
		board = self.pageBoards["my_shop"]
		
		table = TableWindowWithScrollbar(588, 324, 11,3, TableWindowWithScrollbar.SCROLLBAR_HORIZONTAL)
		table.SetParent(board)
		table.SetPosition(17, 49)
		table.SetDefaultCreateChild(MakeDefaultEmptySlot, self.__OnClickMyShopEmptySlot)

		table.Show()
		self.MyShopItemsTable = table
	
	def __MakeMyShopOffersTable(self):
		board = self.pageBoards["my_shop"]

		table = TableWindowWithScrollbar(585, 114, 1,5 , TableWindowWithScrollbar.SCROLLBAR_VERTICAL)
		table.SetParent(board)
		table.SetPosition(20, 395)

		table.Show()
		self.MyShopOffersTable = table


	def __MakeShopListTable(self):
		board = self.pageBoards["shop_list"]
		
		table = TableWindowWithScrollbar(570, 476, 1, 23, TableWindowWithScrollbar.SCROLLBAR_VERTICAL)
		table.SetParent(board)
		table.SetPosition(30, 26)
		
		table.Show()
		self.ShopListTable = table
	
	
	def __MakeOpenShopItemsTable(self):
		board = self.pageBoards["open_shop"]
		
		table = TableWindowWithScrollbar(584, 440, 11,4, TableWindowWithScrollbar.SCROLLBAR_HORIZONTAL)
		table.SetParent(board)
		table.SetPosition(20,55)
		table.SetDefaultCreateChild(MakeDefaultEmptySlot)
		
		table.Show()
		self.OpenShopItemsTable = table

	def __MakeOpenAuctionOffersTable(self):
		board = self.pageBoards["open_auction"]

		table = TableWindowWithScrollbar(580, 340, 1, 20, TableWindowWithScrollbar.SCROLLBAR_VERTICAL)
		table.SetParent(board)
		table.SetPosition(16, 159)

		table.Show()
		self.OpenAuctionOfferTable = table

	def __MakeMyAuctionOffersTable(self):
		board = self.pageBoards["my_auction"]

		table = TableWindowWithScrollbar(580, 340, 1,20, TableWindowWithScrollbar.SCROLLBAR_VERTICAL)
		table.SetParent(board)
		table.SetPosition(16, 159)

		table.Show()
		self.MyAuctionOfferTable = table

	def __MakeAuctionListTable(self):
		board = self.pageBoards["auction_list"]

		table = TableWindowWithScrollbar(581, 471, 1, 20, TableWindowWithScrollbar.SCROLLBAR_VERTICAL)
		table.SetParent(board)
		table.SetPosition(21, 28)

		table.Show()
		self.AuctionListTable = table

	def __GradeFilterOnTop(self):
		if self.GradeFilterSuggestions and self.GradeFilterSuggestions.IsShow():
			self.GradeFilterSuggestions.SetTop()
	
	
	def __MakeGradeFilterWindow(self):
		button = ui.Button()
		button.SetUpVisual("offlineshop/searchfilter/grade_filter/expand_grade_filter_default.png")
		button.SetDownVisual("offlineshop/searchfilter/grade_filter/expand_grade_filter_down.png")
		button.SetOverVisual("offlineshop/searchfilter/grade_filter/expand_grade_filter_over.png")
		button.SAFE_SetEvent(self.__OnClickShowGradeFilterButton)
		button.Hide()
		button.AddFlag("float")
		self.ShowGradeFilterButton = button
		
		window = ui.Board()
		window.SetSize(132,86)
		window.AddFlag("float")
		window.OnTop = self.__GradeFilterOnTop
		self.GradeFilterWindow = window
		
		hide_button = ui.Button()
		hide_button.SAFE_SetEvent(self.__OnClickHideGradeFilterButton)
		hide_button.SetUpVisual("offlineshop/searchfilter/grade_filter/reduce_grade_filter_default.png")
		hide_button.SetDownVisual("offlineshop/searchfilter/grade_filter/reduce_grade_filter_down.png")
		hide_button.SetOverVisual("offlineshop/searchfilter/grade_filter/reduce_grade_filter_over.png")
		hide_button.SetParent(self.GradeFilterWindow)
		hide_button.SetPosition(self.GradeFilterWindow.GetWidth()-hide_button.GetWidth(), 4)
		hide_button.Show()
		self.HideGradeFilterButton = hide_button
		
		background = ui.ImageBox()
		background.LoadImage("offlineshop/searchfilter/grade_filter.png")
		background.Show()
		background.SetParent(self.GradeFilterWindow)
		background.SetWindowHorizontalAlignCenter()
		background.SetWindowVerticalAlignCenter()
		background.SetPosition(-1,0)
		self.GradeFilterBackground = background
		
		checkbox = ui.OfflineShopCheckBox({'base' : "offlineshop/checkbox/base.png", 'tip' : "offlineshop/checkbox/tip.png",})
		checkbox.SetParent(self.GradeFilterBackground)
		checkbox.SetPosition(9,3)
		checkbox.Show()
		self.GradeFilterCheckBox = checkbox
		
		select_button = ui.Button()
		select_button.SetUpVisual("offlineshop/searchfilter/grade_filter/grade_selector_default.png")
		select_button.SetDownVisual("offlineshop/searchfilter/grade_filter/grade_selector_down.png")
		select_button.SetOverVisual("offlineshop/searchfilter/grade_filter/grade_selector_over.png")
		select_button.SetParent(self.GradeFilterBackground)
		select_button.SetPosition(0,0)
		select_button.SetWindowHorizontalAlignCenter()
		select_button.SetWindowVerticalAlignCenter()
		select_button.SetText(self.RARITY_GRADES[-1])
		select_button.SAFE_SetEvent(self.__OnClickGradeSelectButton)
		select_button.Show()
		self.GradeFilterButtonSelect = select_button
		
		selector = SuggestionSelector(len(self.RARITY_GRADES), 'small')
		selector.AddFlag("float")
		selector.SetSelectEvent(self.__OnSelectSearchFilterGradeSelector)
		selector.SetValueDict(self.RARITY_GRADES)
		selector.Hide()
		self.GradeFilterSuggestions = selector
		
		
		self.GradeFilterWindowSettings = {'state': 'hidden', 'x' : self.GetWidth() - self.GradeFilterWindow.GetWidth() , 'y' : 50}
	
	
	def __MakeSearchFilterCheckBoxes(self):
		positions = {
			"name" 	: { "x" : 20, 	"y" : 9, 	},
			"type" 	: { "x" : 222, 	"y" : 9, 	},
			"price" : { "x" : 224, 	"y" : 83, 	},
			"level" : { "x" : 338, 	"y" : 9, 	},
			"wear" 	: { "x" : 20, 	"y" : 83, 	},
		}

		if ENABLE_WOLFMAN_CHARACTER:
			race = {
				"warrior" : { "x" 	: 66+18, 	"y" : 88, 	},
				"assassin" 	: { "x" : 66+18, 	"y" : 114, 	},
				"sura" 		: { "x" : 139+10, 	"y" : 88, 	},
				"shaman" 	: { "x" : 139+10, 	"y" : 114, 	},
				"wolfman"	: { "x" : 20, 		"y" : 114,  },
			}

		else:
			race = {
				"warrior" 	: { "x" : 66, 	"y" : 88, 	},
				"assassin" 	: { "x" : 66, 	"y" : 114, 	},
				"sura" 		: { "x" : 139, 	"y" : 88, 	},
				"shaman" 	: { "x" : 139, 	"y" : 114, 	},
			}
		
		for k, v in positions.items():
			checkbox = ui.OfflineShopCheckBox({'base' : "offlineshop/checkbox/base.png", 'tip' : "offlineshop/checkbox/tip.png",})
			checkbox.SetParent(self.pageBoards["search_filter"])
			checkbox.SetPosition(v["x"] , v["y"])
			checkbox.Show()
			
			self.SearchFilterCheckBoxes[k] = checkbox
		
		
		for k, v in race.items():
			checkbox = ui.OfflineShopCheckBox({'base' : "offlineshop/checkbox/%s_base.png"%k , 'tip' : "offlineshop/checkbox/%s_tip.png"%k,})
			checkbox.SetParent(self.pageBoards["search_filter"])
			checkbox.SetPosition(v["x"] , v["y"])
			checkbox.Show()
			checkbox.Enable()
			
			self.SearchFilterCheckBoxesRace[k] = checkbox
		
		
	def __MakeSearchFilterResultItemsTable(self):
		board = self.pageBoards["search_filter"]
		
		table = TableWindowWithScrollbar(585, 329, 11, 3, TableWindowWithScrollbar.SCROLLBAR_HORIZONTAL)
		table.SetParent(board)
		table.SetPosition(14,153)
		table.SetDefaultCreateChild(MakeDefaultEmptySlot)

		table.ClearElement()

		table.Show()
		self.SearchFilterResultItemsTable = table


	def __MakeSearchHistoryTable(self):
		board = self.pageBoards["search_history"]

		table = TableWindowWithScrollbar(585, 470, 1, 20 , TableWindowWithScrollbar.SCROLLBAR_VERTICAL)
		table.SetParent(board)
		table.SetPosition(13, 32)

		table.Show()
		self.SearchHistoryTable = table


	def __MakeSearchPatternsTable(self):
		board = self.pageBoards["my_patterns"]

		table = TableWindowWithScrollbar(585, 470, 1, 20, TableWindowWithScrollbar.SCROLLBAR_VERTICAL)
		table.SetParent(board)
		table.SetPosition(13, 36)

		table.Show()
		self.SearchPatternsTable = table


	def __MakeShopSafeboxItemsTable(self):
		board = self.pageBoards["shop_safebox"]

		table = TableWindowWithScrollbar(582, 443, 11, 4, TableWindowWithScrollbar.SCROLLBAR_HORIZONTAL)
		table.SetParent(board)
		table.SetPosition(18, 50)
		table.SetDefaultCreateChild(MakeDefaultEmptySlot)

		table.Show()
		self.ShopSafeboxItemsTable = table



	def __MakeMyOffersTable(self):
		board = self.pageBoards["my_offers"]

		table = TableWindowWithScrollbar(584, 445, 2, 4, TableWindowWithScrollbar.SCROLLBAR_VERTICAL)
		table.SetParent(board)
		table.SetPosition(18, 50)

		table.Show()
		self.MyOffersTable = table

	def OnClickAuctionSlot(self, slot):
		import chat
		self.AppendChat(chat.CHAT_TYPE_INFO, "Funkcio letiltva")
		# if app.IsPressed(app.DIK_LCONTROL) or app.IsPressed(app.DIK_RCONTROL) and self.CreateAuctionSlotPos != -1:
			# if mousemodule.mouseController.isAttached():
				# mousemodule.mouseController.DeattachObject()
			
			# self.CreateAuctionWindowPos =-1
			# self.CreateAuctionSlotPos=-1
			# self.__MakeCreateAuctionSlot()
			
			# if self.itemTooltip:
				# self.itemTooltip.ClearToolTip()
				# self.itemTooltip.Hide()
		# else:
			# if mousemodule.mouseController.isAttached():
				# attachedSlotType = mousemodule.mouseController.GetAttachedType()
				# attachedSlotPos = mousemodule.mouseController.GetAttachedSlotNumber()
				# itemIndex = player.GetItemIndex(attachedSlotPos)
				# item.SelectItem(itemIndex)
				# if not item.IsAntiFlag(item.ANTIFLAG_GIVE) and not item.IsAntiFlag(item.ANTIFLAG_MYSHOP):
					# if app.ENABLE_EXTRA_INVENTORY:
						# if attachedSlotType != player.SLOT_TYPE_INVENTORY and attachedSlotType != SLOT_TYPE_EXTRA_INVENTORY:
							# mousemodule.mouseController.DeattachObject()
							# return
					# else:
						# if attachedSlotType != player.SLOT_TYPE_INVENTORY:
							# mousemodule.mouseController.DeattachObject()
							# return
					# self.AuctionBuilding_AddInventoryItem(attachedSlotPos)
				# else:
					# chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.OFFLINESHOP_CANT_SELECT_ITEM_DURING_BUILING)
				
				# mousemodule.mouseController.DeattachObject()

	def __MakeCreateAuctionSlot(self):
		if self.CreateAuctionSlot:
			self.CreateAuctionSlot.Destroy()
			del self.CreateAuctionSlot

		slot = Slot()
		slot.Show()
		slot.SetParent(self.pageBoards['create_auction'])
		slot.SetPosition(430,152)
		slot.SetOnMouseLeftButtonUpEvent(self.OnClickAuctionSlot)

		self.CreateAuctionSlot = slot


	def __MakeMyAuctionSlot(self, info):
		if self.MyAuctionSlot:
			del self.MyAuctionSlot

		slot = Slot()
		slot.SetParent(self.pageBoards['my_auction'])
		slot.SetPosition(339+54,97-73)
		slot.SetInfo(info)
		slot.Show()

		self.MyAuctionSlot = slot

	def __MakeOpenAuctionSlot(self, info):
		if self.OpenAuctionSlot:
			del self.OpenAuctionSlot

		slot = Slot()
		slot.SetParent(self.pageBoards['open_auction'])
		slot.SetPosition(339+54,97-73)
		slot.SetInfo(info)
		slot.Show()

		slot.SetOnMouseLeftButtonUpEvent(self.__OnClickOpenAuctionMakeOffer)
		self.OpenAuctionSlot = slot
	
	def __UpdateGradeFilterEffect(self):
		state = self.GradeFilterWindowSettings.get('state', None)
		if state == 'showing':
			x,y   = self.GetGlobalPosition()
			max_x = self.GetWidth() -3
			
			xpos = self.GradeFilterWindowSettings.get('x', 0)
			ypos = self.GradeFilterWindowSettings.get('y', 0)
			xpos = min(max_x, xpos + 3)
			
			self.GradeFilterWindowSettings['x'] = xpos
			self.GradeFilterWindow.SetPosition(x + xpos, y + ypos)
			self.SetTop()
			
			if max_x == xpos:
				self.GradeFilterWindowSettings['state'] = 'on_view'
		
		elif state == 'hiding':
			x,y = self.GetGlobalPosition()			
			min_x = self.GetWidth() - self.GradeFilterWindow.GetWidth()
			
			xpos = self.GradeFilterWindowSettings.get('x', 0)
			ypos = self.GradeFilterWindowSettings.get('y', 0)
			xpos = max(min_x, xpos - 3)
			
			self.GradeFilterWindowSettings['x'] = xpos
			self.GradeFilterWindow.SetPosition(x + xpos, y + ypos)
			self.SetTop()
			
			if min_x == xpos:
				self.GradeFilterWindowSettings['state'] = 'hidden'
				self.ShowGradeFilterButton.Show()
				self.ShowGradeFilterButton.SetTop()
			
		
	
	def __UpdateAttachedWindows(self):
		x,y = self.GetGlobalPosition()
		
		if self.ShowGradeFilterButton:
			self.ShowGradeFilterButton.SetPosition(x + self.GetWidth() - 10, y + 61 - 6)
			self.ShowGradeFilterButton.SetTop()
		
		if self.GradeFilterWindow:
			xpos = self.GradeFilterWindowSettings.get('x', 0)
			ypos = self.GradeFilterWindowSettings.get('y', 0)
			self.GradeFilterWindow.SetPosition(x + xpos, y + ypos)
		
			if self.GradeFilterSuggestions and self.GradeFilterSuggestions.IsShow():
				x,y = self.GradeFilterButtonSelect.GetGlobalPosition()
				self.GradeFilterSuggestions.SetPosition(x, y + 17)
		
		
	def __OnClickCreateShopIncreaseDaysButton(self):
		days = int(self.CreateShopDaysCountText.GetText())
		
		if days == offlineshop.OFFLINESHOP_MAX_DAYS:
			self.CreateShopDaysCountText.SetText("0")
		
		else:
			self.CreateShopDaysCountText.SetText(str(days+1))
		
	
	def __OnClickCreateShopDecreaseDaysButton(self):
		days = int(self.CreateShopDaysCountText.GetText())
		
		if days == 0:
			self.CreateShopDaysCountText.SetText(str(offlineshop.OFFLINESHOP_MAX_DAYS))
		
		else:
			self.CreateShopDaysCountText.SetText(str(days-1))
	
	
		
	def __OnClickCreateShopIncreaseHoursButton(self):
		hours = int(self.CreateShopHoursCountText.GetText())
		
		if hours == offlineshop.OFFLINESHOP_MAX_HOURS:
			self.CreateShopHoursCountText.SetText("0")
		
		else:
			self.CreateShopHoursCountText.SetText(str(hours+1))
	
	def __OnClickCreateShopDecreaseHoursButton(self):
		hours = int(self.CreateShopHoursCountText.GetText())
		
		if hours == 0:
			self.CreateShopHoursCountText.SetText(str(offlineshop.OFFLINESHOP_MAX_HOURS))
		
		else:
			self.CreateShopHoursCountText.SetText(str(hours-1))
	
	
	def __OnClickCloseButton(self):
		offlineshop.SendForceCloseShop()
	
	def __OnClickMyShopEditNameButton(self):
		self.MyShopEditNameDlg.inputValue.SetText("")
		self.MyShopEditNameDlg.Open()
	
	def __OnClickMyShopPage(self):
		self.__OnChangePage()
		offlineshop.SendCloseBoard()
		offlineshop.SendOpenShopOwner()
		self.EnableRefreshSymbol()
	
	
	def __OnClickShopListPage(self):
		self.__OnChangePage()
		offlineshop.SendCloseBoard()
		offlineshop.SendRequestShopList()
		self.EnableRefreshSymbol()


	def __OnClickShopSafeboxPage(self):
		self.__OnChangePage()
		offlineshop.SendCloseBoard()
		offlineshop.SendSafeboxOpen()
		self.EnableRefreshSymbol()


	def __OnClickMyOffersPage(self):
		self.__OnChangePage()
		offlineshop.SendCloseBoard()
		offlineshop.SendOfferListRequest()
		self.EnableRefreshSymbol()


	def __OnClickSearchFilterPage(self):
		self.__OnChangePage()
		offlineshop.SendCloseBoard()
		self.pageCategory = "search_filter"
		for page in self.pageBoards.values():
			page.Hide()
		
		self.pageBoards["search_filter"].Show()
		if self.ShowGradeFilterButton and self.GradeFilterWindowSettings['state'] == 'hidden':
			self.ShowGradeFilterButton.Show()
			self.ShowGradeFilterButton.SetTop()
		
		if self.GradeFilterWindowSettings['state'] != 'hidden':
			self.GradeFilterWindow.Show()
		
		self.__UpdateAttachedWindows()
	
	
	def __OnSelectSearchFilterTypeComboBox(self, index):
		self.SearchFilterSubTypeComboBox.ClearItem()
		self.SearchFilterSubTypeComboBox.InsertItem(SUBTYPE_NOSET , localeinfo.OFFLINESHOP_TYPE_COMBOBOX_NOSET)
		
		if index == 0:
			self.SearchFilterTypeComboBoxIndex = 0
			self.SearchFilterTypeComboBox.SetCurrentItem(localeinfo.OFFLINESHOP_TYPE_COMBOBOX_NOSET)
			self.SearchFilterSubTypeComboBox.SetCurrentItem(localeinfo.OFFLINESHOP_TYPE_COMBOBOX_NOSET)
			self.SearchFilterSubTypeComboBoxIndex = SUBTYPE_NOSET
			return
		
		self.SearchFilterTypeComboBoxIndex = index
		self.SearchFilterTypeComboBox.SetCurrentItem(self.ITEM_TYPES[index]['name'])
		
		if self.ITEM_TYPES.has_key(index) and self.ITEM_TYPES[index].has_key('subtypes'):
			for sub, name in self.ITEM_TYPES[index]['subtypes'].items():
				self.SearchFilterSubTypeComboBox.InsertItem(sub, name)
		
		else:
			self.SearchFilterSubTypeComboBox.SetCurrentItem(localeinfo.OFFLINESHOP_TYPE_COMBOBOX_NOSET)
			self.SearchFilterSubTypeComboBoxIndex = SUBTYPE_NOSET
	
	def __OnSelectSearchFilterSubTypeComboBox(self, index):
		if index == SUBTYPE_NOSET:
			self.SearchFilterSubTypeComboBox.SetCurrentItem(localeinfo.OFFLINESHOP_TYPE_COMBOBOX_NOSET)
			self.SearchFilterSubTypeComboBoxIndex = SUBTYPE_NOSET
			return
		
		self.SearchFilterSubTypeComboBoxIndex = index
		# safe: subtype name might not exist in dict on some packs
		subtypes = {}
		try:
			subtypes = self.ITEM_TYPES.get(self.SearchFilterTypeComboBoxIndex, {}).get('subtypes', {})
		except:
			subtypes = {}

		name = None
		try:
			name = subtypes.get(index)
		except:
			name = None

		# legacy fallback (some clients stored costume subtypes directly under ITEM_TYPES[type])
		if name is None:
			try:
				name = self.ITEM_TYPES.get(self.SearchFilterTypeComboBoxIndex, {}).get(index, None)
			except:
				name = None

		if not name:
			name = localeinfo.OFFLINESHOP_TYPE_COMBOBOX_NOSET

		self.SearchFilterSubTypeComboBox.SetCurrentItem(name)
	
	
	def __OnSelectSearchFilterGradeSelector(self, index):
		self.GradeFilterButtonSelect.SetText(self.RARITY_GRADES[index])
		self.GradeFilterSuggestionsIndex = index
		self.GradeFilterSuggestions.Hide()
	
	
	def __OnSelectSearchFilterSuggestionSelector(self, index):
		self.SearchFilterAttributeButtons[self.SearchFilterAttributeButtonIndex].SetText(self.ATTRIBUTES[index])
		self.SearchFilterAttributeSetting[self.SearchFilterAttributeButtonIndex] = index
		
		self.SearchFilterAttributeButtonIndex = -1
		self.SearchFilterSuggestionSelector.Hide()
		
	def __OnClickSearchFilterAttributeButton(self, index):
		if self.SearchFilterSuggestionSelector.IsShow():
			self.SearchFilterSuggestionSelector.Hide()
			self.SearchFilterAttributeButtonIndex = -1
		
		else:
			self.SearchFilterAttributeButtonIndex = index
			
			x,y = self.SearchFilterAttributeButtons[index].GetLocalPosition()
			y += self.SearchFilterAttributeButtons[index].GetHeight()
			
			self.SearchFilterSuggestionSelector.SetPosition(x,y)
			self.SearchFilterSuggestionSelector.Show()
	
	
	
	
	
	
	def __OnClickSearchHistoryPage(self):
		self.__OnChangePage()
		offlineshop.SendCloseBoard()
		self.pageCategory = "search_history"

		for v in self.pageBoards.values():
			v.Hide()

		self.pageBoards["search_history"].Show()
		self.RefreshSearchHistoryPage()
		
		
	def __OnClickMyPatternsPage(self):
		self.__OnChangePage()
		offlineshop.SendCloseBoard()
		for v in self.pageBoards.values():
			v.Hide()

		self.pageBoards["my_patterns"].Show()
		self.pageCategory = "my_patterns"
		self.RefreshMyPatternsPage()


	def __OnClickSearchPatternElement(self, element):
		info = element.GetInfo()
		self.__SetSearchFilterPattern(info)

		offlineshop.UpdateLastUseFilterPattern(info["id"])
		
	def __OnClickMyAuctionPage(self):
		self.__OnChangePage()
		offlineshop.SendCloseBoard()
		offlineshop.SendAuctionOpenMy()
		self.EnableRefreshSymbol()
		self.__OnCheckCreateAuctionSlot()

	def __OnClickAuctionListPage(self):
		self.__OnChangePage()
		offlineshop.SendCloseBoard()
		offlineshop.SendAuctionListRequest()
		self.EnableRefreshSymbol()
	
	
	
	def __OnChangePage(self):
		if self.pageCategory == "search_filter":
			self.ShowGradeFilterButton.Hide()
			self.GradeFilterWindow.Hide()
			self.GradeFilterSuggestions.Hide()
			self.__UpdateAttachedWindows()
	
	
	
	def __OnClickCreateAuctionButton(self):
		if self.CreateAuctionWindowPos == -1:
			return

		if self.CreateAuctionSlotPos == -1:
			return


		daystext = self.CreateAuctionDaysInput.GetText()
		days = int(daystext) if daystext and daystext.isdigit() else 0

		if 0 == days or offlineshop.OFFLINESHOP_MAX_DAYS<days:
			self.__PopupMessage(localeinfo.OFFLINESHOP_CREATE_SHOP_INVALID_DURATION)
			return

		pricetext = self.CreateAuctionStartingPriceInput.GetText()
		price = long(pricetext) if pricetext and pricetext.isdigit() else 0

		if price <= 0:
			self.__PopupMessage(localeinfo.OFFLINESHOP_CREATE_AUCTION_INVALID_PRICE)
			return

		offlineshop.SendAuctionCreate(self.CreateAuctionWindowPos, self.CreateAuctionSlotPos, price, days*24*60)
		self.EnableRefreshSymbol()

	def __OnClickCreateAuctionDaysDecreaseButton(self):
		days = int(self.CreateAuctionDaysInput.GetText())
		if days ==0:
			self.CreateAuctionDaysInput.SetText(str(offlineshop.OFFLINESHOP_MAX_DAYS))
		else:
			self.CreateAuctionDaysInput.SetText(str(days-1))

	def __OnClickMyAuctionClose(self):
		offlineshop.CloseMyAuction()

	def __OnClickCreateAuctionDaysIncreaseButton(self):
		days = int(self.CreateAuctionDaysInput.GetText())
		if days == offlineshop.OFFLINESHOP_MAX_DAYS:
			self.CreateAuctionDaysInput.SetText("0")
		else:
			self.CreateAuctionDaysInput.SetText(str(days + 1))

	def __OnClickSearchFilterResetFilterButton(self):
		# combobox
		for k, v in self.SearchFilterCheckBoxes.items():
			v.Disable()

		# raceflags
		for v in self.SearchFilterCheckBoxesRace.values():
			v.Enable()

		# type/subtype
		self.SearchFilterTypeComboBox.SetCurrentItem(localeinfo.OFFLINESHOP_TYPE_COMBOBOX_NOSET)
		self.SearchFilterTypeComboBoxIndex = 0
		self.SearchFilterSubTypeComboBox.SetCurrentItem(localeinfo.OFFLINESHOP_TYPE_COMBOBOX_NOSET)
		self.SearchFilterSubTypeComboBoxIndex = SUBTYPE_NOSET

		# name
		self.SearchFilterItemNameInput.SetText("")
		self.SearchFilterSuggestionObj.Clear()

		# level
		self.SearchFilterItemLevelStart.SetText("")
		self.SearchFilterItemLevelEnd.SetText("")

		# price
		self.SearchFilterItemYangMin.SetText("")
		self.SearchFilterItemYangMax.SetText("")


		self.SearchFilterResultItemsTable.ClearElement()
		#offlineshop-updated 04/08/19
		self.__OnSelectSearchFilterTypeComboBox(0)
		self.__OnSelectSearchFilterSubTypeComboBox(SUBTYPE_NOSET)
	
	def __OnClickSearchFilterSavePatternButton(self):
		bActiveOne = False

		for v in self.SearchFilterCheckBoxes.values():
			if v.IsEnabled():
				bActiveOne = True
				break
				
		if self.GradeFilterWindow and self.GradeFilterWindow.IsShow() and self.GradeFilterCheckBox.IsEnabled():
			bActiveOne = True

		if not bActiveOne:
			self.__PopupMessage(localeinfo.OFFLINESHOP_NO_FILTER_ACTIVE_MESSAGE)
			return

		if not self.SearchPatternsInputNameDlg.IsShow():
			self.SearchPatternsInputNameDlg.inputValue.SetText("")
			self.SearchPatternsInputNameDlg.Open()

	
	def __OnClickSearchFilterStartSearch(self):
		bActiveOne = False
		
		for v in self.SearchFilterCheckBoxes.values():
			if v.IsEnabled():
				bActiveOne = True
				break
		
		if self.GradeFilterWindow and self.GradeFilterWindow.IsShow() and self.GradeFilterCheckBox.IsEnabled():
			bActiveOne = True
		
		if not bActiveOne:
			self.__PopupMessage(localeinfo.OFFLINESHOP_NO_FILTER_ACTIVE_MESSAGE)
			return

		if self.SearchFilterCheckBoxes["name"].IsEnabled():
			if not self.SearchFilterItemNameInput.GetText():
				self.__PopupMessage(localeinfo.OFFLINESHOP_SEARCH_FILTER_NAME_NOSET)
				return

		if self.SearchFilterCheckBoxes["price"].IsEnabled():
			if self.SearchFilterItemYangMin.GetText()=="0" and self.SearchFilterItemYangMax.GetText()=="0":
				self.__PopupMessage(localeinfo.OFFLINESHOP_SEARCH_PRICE_ERROR)
				return

		if self.SearchFilterCheckBoxes["level"].IsEnabled():
			if self.SearchFilterItemLevelStart.GetText() == "0" and self.SearchFilterItemLevelEnd.GetText() == "0":
				self.__PopupMessage(localeinfo.OFFLINESHOP_SEARCH_LEVEL_ERROR)
				return
		
		if self.GradeFilterWindow and self.GradeFilterCheckBox.IsEnabled():
			grade = self.GradeFilterSuggestionsIndex
			if grade == -1:
				self.__PopupMessage(localeinfo.OFFLINESHOP_SEARCH_RARITY_ERROR)
				return
		
		
		self.SearchFilterLastUsedSetting = self.GetSearchFilterSettings()
		offlineshop.SendFilterRequest(*self.SearchFilterLastUsedSetting)
		self.EnableRefreshSymbol()

	

	def __ClearImportantMultiSearch(self):
		self._importantMultiActive = False
		self._importantMultiQueue = []
		self._importantMultiResults = []
		self._importantMultiSet = set()
		self._importantMultiCurrentVnum = 0
		self._importantMultiWaitUntil = 0.0
		self._importantMultiWaitUntil = 0.0

	def __OnClickSearchFilterImportantMounts(self):
		"""Azonnali kereses: mutassa az osszes mountot (hatasokat)."""
		self.__ClearImportantMultiSearch()

		# Biztonsag kedveert mindig a kereso oldalra valtsunk.
		try:
			if self.pageCategory != "search_filter":
				self.__OnClickSearchFilterPage()
		except:
			pass

		 
		self.__OnClickSearchFilterResetFilterButton()

		 
		try:
			for k, v in self.SearchFilterCheckBoxes.items():
				v.Disable()
			self.SearchFilterCheckBoxes["type"].Enable()
		except:
			return

		# Costume -> Mount (hatas)
		try:
			costume_type = item.ITEM_TYPE_COSTUME
			mount_subtype = item.COSTUME_TYPE_MOUNT
		except:
			costume_type = 0
			mount_subtype = SUBTYPE_NOSET

		self.__OnSelectSearchFilterTypeComboBox(costume_type)
		self.__OnSelectSearchFilterSubTypeComboBox(mount_subtype)

		# Azonnal inditsuk a kerest (server request) - ne kelljen "Kereses inditasa" gomb.
		try:
			if self.SearchFilterResultItemsTable:
				self.SearchFilterResultItemsTable.ClearElement()
		except:
			pass

		self.SearchFilterLastUsedSetting = self.GetSearchFilterSettings()
		offlineshop.SendFilterRequest(*self.SearchFilterLastUsedSetting)
		self.EnableRefreshSymbol()


	def __ImportantInstantNameSearch(self, keyword):
		"""Gyors kereses nev alapjan: beirja a szot es azonnal elkuldi a filter kerest."""
		self.__ClearImportantMultiSearch()

		 
		try:
			if self.pageCategory != "search_filter":
				self.__OnClickSearchFilterPage()
		except:
			pass

		 
		self.__OnClickSearchFilterResetFilterButton()

		 
		try:
			for k, v in self.SearchFilterCheckBoxes.items():
				v.Disable()
			self.SearchFilterCheckBoxes["name"].Enable()
		except:
			return

		try:
			self.SearchFilterItemNameInput.SetText(keyword)
		except:
			pass

		try:
			if self.SearchFilterResultItemsTable:
				self.SearchFilterResultItemsTable.ClearElement()
		except:
			pass

		self.SearchFilterLastUsedSetting = self.GetSearchFilterSettings()
		offlineshop.SendFilterRequest(*self.SearchFilterLastUsedSetting)
		self.EnableRefreshSymbol()

	def __OnClickSearchFilterImportantLegendary(self):
		
		self.__ImportantInstantNameSearch("Legendás Bónuszoló")

	def __OnClickSearchFilterImportantLegendaryChange(self):
		self.__ImportantInstantNameSearch("Legendás Megváltoztató")

	def __OnClickSearchFilterImportantTalisman(self):
		self.__ImportantInstantNameSearch("Talizmán megerösíto")

	def __OnClickSearchFilterImportantTalismanEnchant(self):
		self.__ImportantInstantNameSearch("Talizmán büvölo")

	def __OnClickSearchFilterImportantOther(self):
		self.__ImportantInstantNameSearch("Utalvany")

	def __StartImportantVnumSearch(self, vnum_list):
		self.__ClearImportantMultiSearch()
		self.__OnClickSearchFilterResetFilterButton()

		self._importantMultiActive = True
		self._importantMultiQueue = list(vnum_list)
		self._importantMultiResults = []
		self._importantMultiSet = set()
		self._importantMultiCurrentVnum = 0
		self._importantMultiWaitUntil = 0.0

		if self.SearchFilterResultItemsTable:
			self.SearchFilterResultItemsTable.ClearElement()

		self.EnableRefreshSymbol()
		self.__SendNextImportantVnumQuery()

	def __BuildSearchSettingForVnum(self, vnum):
		attributes = tuple([(0, 0) for x in xrange(player.ATTRIBUTE_SLOT_NORM_NUM)])
		try:
			item.SelectItem(vnum)
			type = item.GetItemType()
			subtype = item.GetItemSubType()
			name = item.GetItemName()
		except:
			type = 0
			subtype = SUBTYPE_NOSET
			name = ""

		return (type, subtype, name, (0, 0), (0, 0), 0, attributes, -1)

	def __SendNextImportantVnumQuery(self):
		if not self._importantMultiQueue:
			self._importantMultiActive = False
			self.SearchFilterShopItemResult = self._importantMultiResults
			self.__RenderSearchFilterResults(True)
			return

		vnum = self._importantMultiQueue.pop(0)
		self._importantMultiCurrentVnum = vnum

		setting = self.__BuildSearchSettingForVnum(vnum)
		offlineshop.SendFilterRequest(*setting)

	def __RenderSearchFilterResults(self, skip_history = False):
		self.SearchFilterResultItemsTable.ClearElement()

		# Update local cache of market average prices from the last received search results.
		try:
			self.__UpdateMarketAvgCacheFromSearchResults(self.SearchFilterShopItemResult)
		except:
			pass

		for info in self.SearchFilterShopItemResult:
			slot = Slot()
			slot.SetInfo(info)
			slot.SetIndex((info["id"], info["owner"]))
			slot.SetOnMouseLeftButtonUpEvent(self.__OnLeftClickSearchFilterShopResultItem)
			slot.Show()

			self.SearchFilterResultItemsTable.AddElement(slot)

		if len(self.SearchFilterShopItemResult) == SEARCH_RESULT_LIMIT:
			self.__PopupMessage(localeinfo.OFFLINESHOP_SEARCH_RISE_LIMIT%SEARCH_RESULT_LIMIT)

		if not skip_history:
			setting = self.GetSearchFilterSettings()
			setting = (len(self.SearchFilterShopItemResult),) + setting
			offlineshop.AppendNewFilterHistory(*setting)

		self.DisableRefreshSymbol()

	def __UpdateMarketAvgCacheFromSearchResults(self, result_list):
		"""Client-side: build/refresh a simple vnum->average price cache.
		Prices already arrive from server in search results, we just aggregate them.
		"""
		try:
			if not result_list:
				return
		except:
			return

		by_vnum = {}
		for info in result_list:
			try:
				vnum = int(info.get('vnum', 0))
				if vnum <= 0:
					continue
				total = int(info.get('price', 0))
				if ENABLE_CHEQUE_SYSTEM:
					total += int(info.get('cheque', 0)) * YANG_PER_CHEQUE
				if total <= 0:
					continue
			except:
				continue

			data = by_vnum.get(vnum)
			if not data:
				by_vnum[vnum] = [total, total, total, 1]  # sum, min, max, count
			else:
				data[0] += total
				if total < data[1]:
					data[1] = total
				if total > data[2]:
					data[2] = total
				data[3] += 1

		if not hasattr(self, '_marketAvgCache') or self._marketAvgCache is None:
			self._marketAvgCache = {}
		for vnum, data in by_vnum.items():
			try:
				sumv, minv, maxv, cnt = data
				avg = int(sumv / cnt) if cnt > 0 else 0
				if avg <= 0:
					continue
			except:
				continue

			self._marketAvgCache[vnum] = {
				'avg': avg,
				'min': minv,
				'max': maxv,
				'count': cnt,
				'ts': app.GetTime() if hasattr(app, 'GetTime') else 0.0,
			}

	def __GetMarketAvgTextForVnum(self, vnum, count=1):
		"""Build text shown under the price input for a given item."""
		try:
			vnum = int(vnum)
			count = int(count)
		except:
			vnum = 0
			count = 1

		info = None
		try:
			info = self._marketAvgCache.get(vnum, None)
		except:
			info = None

		if not info:
			return "atlag ar: nincs adat "

		try:
			avg = int(info.get('avg', 0))
			cnt = int(info.get('count', 0))
		except:
			avg = 0
			cnt = 0

		if avg <= 0:
			return "atlag ar: nincs adat"

		try:
			avg_str = localeinfo.NumberToMoneyString(avg)
		except:
			avg_str = str(avg)

		if count > 1:
			try:
				total_str = localeinfo.NumberToMoneyString(avg * count)
			except:
				total_str = str(avg * count)
			return "Atlag ar: %s /db  " % avg_str

		return "Atlag ar: %s /db  " % avg_str

	def __SetMarketAvgOnPriceDialog(self, vnum, count=1):
		try:
			if not self.CommonInputPriceDlg:
				return
			text = self.__GetMarketAvgTextForVnum(vnum, count)
			if hasattr(self.CommonInputPriceDlg, 'SetMarketAverageText'):
				self.CommonInputPriceDlg.SetMarketAverageText(text)
		except:
			pass

	def __NeedMarketAvgRefresh(self, vnum):
		try:
			vnum = int(vnum)
		except:
			return True
		info = None
		try:
			info = self._marketAvgCache.get(vnum, None)
		except:
			info = None
		if not info:
			return True
		try:
			ts = float(info.get('ts', 0.0))
			now = app.GetTime()
			ttl = float(getattr(self, '_avgPriceCacheTTL', 60.0))
			return (now - ts) > ttl
		except:
			return True

	def __RequestMarketAvgRefresh(self, vnum, count=1):
		"""Ask server for current listings of this item and refresh avg cache.
		Python-only: uses existing filter request packet, handled silently (does not override search UI).
		"""
		try:
			vnum = int(vnum)
			count = int(count)
		except:
			return

		try:
			if getattr(self, '_avgPriceQueryActive', False):
				return
		except:
			pass

		# respect server cooldown between searches
		try:
			now = app.GetTime()
			last = float(getattr(self, '_avgPriceQueryLastSend', 0.0))
			minInt = float(getattr(self, '_avgPriceQueryMinInterval', 2.3))
			if now - last < minInt:
				return
		except:
			pass

		# show pending line
		try:
			if self.CommonInputPriceDlg and hasattr(self.CommonInputPriceDlg, 'SetMarketAverageText'):
				self.CommonInputPriceDlg.SetMarketAverageText("Piaci atlag ar: szamolas...")
		except:
			pass

		try:
			self._avgPriceQueryActive = True
			self._avgPriceQueryVnum = vnum
			self._avgPriceQueryCount = count
			self._avgPriceQueryBackupResult = self.SearchFilterShopItemResult
			self._avgPriceQueryLastSend = app.GetTime()
			setting = self.__BuildSearchSettingForVnum(vnum)
			offlineshop.SendFilterRequest(*setting)
		except:
			self._avgPriceQueryActive = False
			self._avgPriceQueryVnum = 0
			self._avgPriceQueryCount = 1
			self._avgPriceQueryBackupResult = None

	def __HidePriceDialog(self):
		"""Hide price input dialog and clear optional market average line.
		"""
		try:
			if self.CommonInputPriceDlg and hasattr(self.CommonInputPriceDlg, 'SetMarketAverageText'):
				self.CommonInputPriceDlg.SetMarketAverageText("")
		except:
			pass
		try:
			if self.CommonInputPriceDlg:
				self.CommonInputPriceDlg.Hide()
		except:
			pass

	def __OnClickShowGradeFilterButton(self):
		self.GradeFilterWindowSettings['state'] = 'showing'
		self.ShowGradeFilterButton.Hide()
		self.GradeFilterWindow.Show()
		self.SetTop()
		
	
	def __OnClickGradeSelectButton(self):
		self.GradeFilterSuggestions.Show()
		self.GradeFilterSuggestions.SetTop()
		x,y = self.GradeFilterButtonSelect.GetGlobalPosition()
		self.GradeFilterSuggestions.SetPosition(x, y + 17)
	
	def __OnClickHideGradeFilterButton(self):
		self.GradeFilterWindowSettings['state'] = 'hiding'
		if self.GradeFilterSuggestions:
			self.GradeFilterSuggestions.Hide()
	
	
	def __OnClickFilterHistoryElement(self , element):
		info = element.GetInfo()
		self.__SetSearchFilterPattern(info)



	def __OnClickDeleteMyOfferButton(self, id):
		offer = None

		for element in self.MyOffersList:
			if element["offer_id"] == id:
				offer = element
				break

		if not offer:
			return

		accepttext = localeinfo.OFFLINESHOP_CANCEL_OFFER_QUESTION_ACCEPT
		canceltext = localeinfo.OFFLINESHOP_CANCEL_OFFER_QUESTION_CANCEL

		question   = localeinfo.OFFLINESHOP_CANCEL_OFFER_QUESTION

		self.MyOffersCancelOfferInfo = (offer['offer_id'], offer['owner_id'])
		self.__OpenQuestionDialog(len(question)*6 , accepttext, canceltext, self.__OnAcceptCancelOfferQuestion, self.__OnCancelCancelOfferQuestion, question)








	def __SetSearchFilterPattern(self, info):
		self.__OnClickSearchFilterPage()

		# combobox
		for k, v in self.SearchFilterCheckBoxes.items():
			v.Disable()

		# raceflags
		raceFlags = {
			"warrior" :item.ITEM_ANTIFLAG_WARRIOR,
			"assassin" :item.ITEM_ANTIFLAG_ASSASSIN,
			"sura" :item.ITEM_ANTIFLAG_SURA,
			"shaman": item.ITEM_ANTIFLAG_SHAMAN,
		}

		flag = info["filter_wearflag"]

		for k, v in raceFlags.items():
			if flag & v != 0:
				self.SearchFilterCheckBoxesRace[k].Disable()
				if not self.SearchFilterCheckBoxes["wear"].IsEnabled():
					self.SearchFilterCheckBoxes["wear"].Enable()

			else:
				self.SearchFilterCheckBoxesRace[k].Enable()

		# type/subtype
		if info["filter_type"] == 0:
			self.SearchFilterTypeComboBox.SetCurrentItem(localeinfo.OFFLINESHOP_TYPE_COMBOBOX_NOSET)
			self.SearchFilterTypeComboBoxIndex = 0
			self.SearchFilterSubTypeComboBox.SetCurrentItem(localeinfo.OFFLINESHOP_TYPE_COMBOBOX_NOSET)
			self.SearchFilterSubTypeComboBoxIndex = SUBTYPE_NOSET

		else:
			if not self.SearchFilterCheckBoxes["type"].IsEnabled():
				self.SearchFilterCheckBoxes["type"].Enable()

			typeCategory = self.ITEM_TYPES[info["filter_type"]]
			self.SearchFilterTypeComboBox.SetCurrentItem(typeCategory["name"])
			self.SearchFilterTypeComboBoxIndex = info["filter_type"]

			subtype = info["filter_subtype"]
			if subtype != SUBTYPE_NOSET and typeCategory.has_key('subtypes'):
				self.SearchFilterSubTypeComboBox.SetCurrentItem(typeCategory['subtypes'][subtype])
				self.SearchFilterSubTypeComboBoxIndex = subtype
			else:
				self.SearchFilterSubTypeComboBox.SetCurrentItem(localeinfo.OFFLINESHOP_TYPE_COMBOBOX_NOSET)
				self.SearchFilterSubTypeComboBoxIndex = SUBTYPE_NOSET

		# name
		if info["filter_name"]:
			self.SearchFilterCheckBoxes["name"].Enable()
			self.SearchFilterItemNameInput.SetText(info["filter_name"])

		# level

		levelmin = info["level"]["start"]
		levelmax = info["level"]["end"]

		if levelmin != 0:
			self.SearchFilterCheckBoxes["level"].Enable()
			self.SearchFilterItemLevelStart.SetText(str(levelmin))
		if levelmax != 0:
			self.SearchFilterCheckBoxes["level"].Enable()
			self.SearchFilterItemLevelEnd.SetText(str(levelmax))

		# price
		yangmin = info["price"]["start"]
		yangmax = info["price"]["end"]

		if yangmin != 0:
			self.SearchFilterCheckBoxes["price"].Enable()
			self.SearchFilterItemYangMin.SetText(str(yangmin))
		if yangmax != 0:
			self.SearchFilterCheckBoxes["price"].Enable()
			self.SearchFilterItemYangMax.SetText(str(yangmax))
		
		#rarity
		rarity = info['rarity']
		
		if rarity == -1 or not rarity in self.RARITY_GRADES:
			if self.GradeFilterCheckBox and self.GradeFilterCheckBox.IsEnabled():
				self.GradeFilterCheckBox.Disable()
			if self.GradeFilterWindow and self.GradeFilterWindow.IsShow():
				self.__OnClickHideGradeFilterButton()
		else:
			if self.GradeFilterCheckBox and not self.GradeFilterCheckBox.IsEnabled():
				self.GradeFilterCheckBox.Enable()
			if self.GradeFilterButtonSelect:
				self.GradeFilterButtonSelect.SetText(self.RARITY_GRADES[rarity])
			if self.GradeFilterWindow and self.GradeFilterWindow.IsShow() == False:
				self.__OnClickShowGradeFilterButton()
			self.GradeFilterSuggestionsIndex = rarity
		


	def __PopupMessage(self, message):
		self.popupMessage.SetText(message)
		self.popupMessage.Open()


	def __OpenQuestionDialog(self , width, accepttext, canceltext, acceptevent, cancelevent, text):
		self.CommonQuestionDlg.SetAcceptText(accepttext)
		self.CommonQuestionDlg.SetCancelText(canceltext)
		self.CommonQuestionDlg.SAFE_SetAcceptEvent(acceptevent)
		self.CommonQuestionDlg.SAFE_SetCancelEvent(cancelevent)
		self.CommonQuestionDlg.SetText(text)
		self.CommonQuestionDlg.SetWidth(width)

		self.CommonQuestionDlg.Open()

	if ENABLE_CHEQUE_SYSTEM:
		def __OnPickValute(self, title, acceptEvent, maxValue, maxCheque, max=13):
			self.CommonPickValuteDlg.SetMax(max)
			self.CommonPickValuteDlg.SetTitleName(title)
			self.CommonPickValuteDlg.SetAcceptEvent(acceptEvent)
			self.CommonPickValuteDlg.Open(maxValue, maxCheque)

	
	else:
		def __OnPickValute(self, title, acceptEvent, maxValue, max=13):
			self.CommonPickValuteDlg.SetMax(max)
			self.CommonPickValuteDlg.SetTitleName(title)
			self.CommonPickValuteDlg.SetAcceptEvent(acceptEvent)
			self.CommonPickValuteDlg.Open(maxValue)
	
	
	
	def OnShowPage(self):
		showEvents = {
			"my_shop"		: self.__OnClickMyShopPage,
		}
		
		if self.pageCategory in showEvents:
			showEvents[self.pageCategory]()
	

	def BINARY_EnableRefreshSymbol(self):
		if self.IsShow():
			self.EnableRefreshSymbol()

	def EnableRefreshSymbol(self):
		self.RefreshSymbol.Show()
	
	
	def DisableRefreshSymbol(self):
		self.RefreshSymbol.Hide()

	
	
	def ShopListClear(self):
		self.ShopList = []



	def ShopListAddItem( self, owner_id, duration , count , name):
		newelement = {}
		newelement["owner_id"]		= owner_id
		newelement["duration"]		= duration
		newelement["count"]			= count
		newelement["name"]			= name
		
		self.ShopList.append(newelement)
	



	def ShopListShow(self):
		self.RefreshShopListPage()
		self.DisableRefreshSymbol()
	



	def OpenShop( self, owner_id, duration, count, name):
		self.ShopItemSold 		= []
		self.ShopItemForSale 	= []
		
		self.ShopOpenInfo["owner_id"]	= owner_id
		self.ShopOpenInfo["duration"]	= duration
		self.ShopOpenInfo["count"]		= count
		self.ShopOpenInfo["name"]		= name
		self.ShopOpenInfo["my_shop"]	= False
		self.__SetShopViewed(owner_id, name)

	def __SetShopViewed(self, owner_id, name):
		if owner_id not in self.ViewedShopOwners:
			self.ViewedShopOwners.add(owner_id)
			if self.pageCategory == "shop_list":
				self.RefreshShopListPage()
	
	
	def OpenShopItem_Alloc(self):
		self.ShopItemForSale.append({})
	
	
	def OpenShopItem_SetValue( self, key,	index,	*args):
		if key == "id":
			self.ShopItemForSale[index][key] = args[0]
		
		elif key == "vnum" or key == 'trans' or key == 'locked_attr':
			self.ShopItemForSale[index][key] = args[0]
		
		elif key == "count":
			self.ShopItemForSale[index][key] = args[0]

		elif key == "attr":
			if not key in self.ShopItemForSale[index]:
				self.ShopItemForSale[index][key] = {}
			
			attr_index = args[0]
			attr_type  = args[1]
			attr_value = args[2]
			
			self.ShopItemForSale[index][key][attr_index] = {}
			self.ShopItemForSale[index][key][attr_index]["type"]  = attr_type
			self.ShopItemForSale[index][key][attr_index]["value"] = attr_value
		
		elif key == "socket":
			if not key in self.ShopItemForSale[index]:
				self.ShopItemForSale[index][key] = {}
			
			socket_index = args[0]
			socket_val	 = args[1]
			
			self.ShopItemForSale[index][key][socket_index] = socket_val
		
		elif key == "price" or key == 'cheque':
			self.ShopItemForSale[index][key] = args[0]
	
	
	def OpenShop_End(self):
		# Cache prices from opened shop items as well (helps market average without using search).
		try:
			self.__UpdateMarketAvgCacheFromSearchResults(self.ShopItemForSale)
		except:
			pass
		self.RefreshOpenShopPage()
		self.DisableRefreshSymbol()

		if not self.IsShow():
			self.Show()

	
	def OpenShopOwner_Start( self, owner_id, duration , count , name):
		self.ShopItemSold 		= []
		self.ShopItemForSale 	= []
		self.MyShopOffers		= []
		
		
		self.ShopOpenInfo["owner_id"]	= owner_id
		self.ShopOpenInfo["duration"]	= duration
		self.ShopOpenInfo["count"]		= count
		self.ShopOpenInfo["name"]		= name
		self.ShopOpenInfo["my_shop"]	= True
	
	def OpenShopOwner_End(self):
		# Cache prices from my shop items as well (helps market average without using search).
		try:
			self.__UpdateMarketAvgCacheFromSearchResults(self.ShopItemForSale)
		except:
			pass
		self.DisableRefreshSymbol()
		self.RefreshMyShopPage()

		if not self.IsShow():
			self.Show()
	
	def OpenShopOwnerItem_Alloc(self):
		self.ShopItemForSale.append({})
	
	def OpenShopOwnerItem_SetValue( self, key, index, *args):
		if key == "id":
			self.ShopItemForSale[index][key] = args[0]
		
		elif key == "vnum" or key == 'trans' or key == 'locked_attr':
			self.ShopItemForSale[index][key] = args[0]
		
		elif key == "count":
			self.ShopItemForSale[index][key] = args[0]
		
		elif key == "attr":
			if not key in self.ShopItemForSale[index]:
				self.ShopItemForSale[index][key] = {}
			
			attr_index = args[0]
			attr_type  = args[1]
			attr_value = args[2]
			
			self.ShopItemForSale[index][key][attr_index] = {}
			self.ShopItemForSale[index][key][attr_index]["type"]  = attr_type
			self.ShopItemForSale[index][key][attr_index]["value"] = attr_value
		
		elif key == "socket":
			if not key in self.ShopItemForSale[index]:
				self.ShopItemForSale[index][key] = {}
			
			socket_index = args[0]
			socket_val	 = args[1]
			
			self.ShopItemForSale[index][key][socket_index] = socket_val
		
		elif key == "price" or key == 'cheque':
			self.ShopItemForSale[index][key] = args[0]

	def OpenShopOwner_SetOffer(self, itemid , buyerid, offerid, price, is_accept, buyer_name ):
		newoffer = {}
		newoffer["id"] 			= offerid
		newoffer["item_id"]		= itemid
		newoffer["buyer_id"]	= buyerid
		newoffer["price"]		= price
		newoffer["is_accept"]	= is_accept
		newoffer['buyer_name']  = buyer_name

		self.MyShopOffers.append(newoffer)

	def OpenShopOwnerItemSold_Alloc( self ):
		self.ShopItemSold.append({})
	
	def OpenShopOwnerItemSold_SetValue( self,  key , index , *args):
		if key == "id":
			self.ShopItemSold[index][key] = args[0]
		
		elif key == "vnum" or key == 'trans' or key == 'locked_attr':
			self.ShopItemSold[index][key] = args[0]
		
		elif key == "count":
			self.ShopItemSold[index][key] = args[0]
		
		elif key == "attr":
			if not key in self.ShopItemSold[index]:
				self.ShopItemSold[index][key] = {}
			
			attr_index = args[0]
			attr_type  = args[1]
			attr_value = args[2]
			
			self.ShopItemSold[index][key][attr_index] = {}
			self.ShopItemSold[index][key][attr_index]["type"]  = attr_type
			self.ShopItemSold[index][key][attr_index]["value"] = attr_value
		
		elif key == "socket":
			if not key in self.ShopItemSold[index]:
				self.ShopItemSold[index][key] = {}
			
			socket_index = args[0]
			socket_val	 = args[1]
			
			self.ShopItemSold[index][key][socket_index] = socket_val
		
		elif key == "price" or key == 'cheque':
			self.ShopItemSold[index][key] = args[0]
	
	def OpenShopOwnerItemSold_Show( self):
		pass
	
	
	def OpenShopOwnerNoShop(self):
		for v in self.pageBoards.values():
			v.Hide()
		
		self.pageBoards["create_shop"].Show()
		self.pageCategory = "create_shop"

		self.DisableRefreshSymbol()
		self.__ResetCreateShopPage()
	
	
	
	
	
	def ClearItemNames(self):
		self.SearchFilterItemsNameDict = {}
	
	
	def AppendItemName(self, vnum, name):
		self.SearchFilterItemsNameDict[name] = vnum
	
	
	def ShopClose( self):
		pass
	
	
	def ShopFilterResult( self , size):
		self.SearchFilterShopItemResult = []
	
	def ShopFilterResultItem_Alloc(self):
		self.SearchFilterShopItemResult.append({})
	
	
	def ShopFilterResultItem_SetValue( self,  key, index, *args):
		if key in ( "id", "vnum", "count", "price", "owner", 'trans', 'cheque', 'locked_attr'):
			self.SearchFilterShopItemResult[index][key] = args[0]
		
		elif key == "attr":
			if not key in self.SearchFilterShopItemResult[index]:
				self.SearchFilterShopItemResult[index][key] = {}
			
			attr_index = args[0]
			attr_type  = args[1]
			attr_value = args[2]
			
			self.SearchFilterShopItemResult[index][key][attr_index] = {}
			self.SearchFilterShopItemResult[index][key][attr_index]["type"]  = attr_type
			self.SearchFilterShopItemResult[index][key][attr_index]["value"] = attr_value
		
		elif key == "socket":
			if not key in self.SearchFilterShopItemResult[index]:
				self.SearchFilterShopItemResult[index][key] = {}
			
			socket_index = args[0]
			socket_val	 = args[1]
			
			self.SearchFilterShopItemResult[index][key][socket_index] = socket_val
		
		
	
	
	def ShopFilterResult_Show(self):
		# silent avg refresh query (do not touch search UI)
		if getattr(self, '_avgPriceQueryActive', False):
			try:
				self.__UpdateMarketAvgCacheFromSearchResults(self.SearchFilterShopItemResult)
				self.__SetMarketAvgOnPriceDialog(self._avgPriceQueryVnum, self._avgPriceQueryCount)
			except:
				pass

			# restore previous search results (if any) to keep UI consistent
			try:
				if self._avgPriceQueryBackupResult is not None:
					self.SearchFilterShopItemResult = self._avgPriceQueryBackupResult
			except:
				pass

			self._avgPriceQueryActive = False
			self._avgPriceQueryVnum = 0
			self._avgPriceQueryCount = 1
			self._avgPriceQueryBackupResult = None
			return

		# important multi-search (vnum list)
		if self._importantMultiActive:
			cur_vnum = self._importantMultiCurrentVnum
			for info in self.SearchFilterShopItemResult:
				try:
					if info["vnum"] != cur_vnum:
						continue
				except:
					continue

				key = (info.get("id", 0), info.get("owner", 0))
				if key in self._importantMultiSet:
					continue
				self._importantMultiSet.add(key)
				self._importantMultiResults.append(info)

			# show partial results immediately (then extend after cooldown)
			self.SearchFilterShopItemResult = self._importantMultiResults
			self.__RenderSearchFilterResults(True)


			# Server has a cooldown between searches (OFFLINESHOP_SECONDS_PER_SEARCH).
			# If we send the next request immediately, only the first vnum will work.
			if self._importantMultiQueue:
				self.EnableRefreshSymbol()
				self._importantMultiWaitUntil = app.GetTime() + self._importantMultiDelay
				return
			# last chunk received -> finalize & render
			self.__SendNextImportantVnumQuery()
			return

		# normal search
		self.__RenderSearchFilterResults()

	
	def OfferReceived( self, offer_id , notified, accepted, owner_id, offerer_id, item_id, yang):
		pass
	
	def OfferAccept( self, offer_id , notified, accepted, owner_id, offerer_id, item_id, yang):
		pass
	
	
	def ClearFilterHistory(self):
		self.FilterHistory = []
	
	def AllocFilterHistory( self):
		self.FilterHistory.append({'id' : len(self.FilterHistory)})
	
	def SetFilterHistoryValue( self, key, *args):
		elm = self.FilterHistory[-1]
		
		if key == "datetime":
			elm["minute"]	= args[0]
			elm["hour"]		= args[1]
			elm["day"]		= args[2]
			elm["month"]	= args[3]
			elm["year"]		= args[4]
		
		elif key in ('count' , 'filter_type' , 'filter_subtype', 'filter_name' ,'filter_wearflag','rarity'):
			elm[key] = args[0]
		
		elif key in ('filter_price_yang_start', 'filter_price_yang_end'):
			if not 'price' in elm:
				elm['price'] = {}
			
			elm['price'][key.replace('filter_price_yang_', '')] = args[0]		
		
		elif key in ('filter_level_start', 'filter_level_end'):
			if not 'level' in elm:
				elm['level'] = {}
			
			elm['level'][key.replace('filter_level_', '')] = args[0]		
		
		elif key in ('filter_attr_type' , 'filter_attr_value'):
			if not 'attr' in elm:
				elm['attr'] = {}
				elm['attr']['type']  = [0 for x in xrange(offlineshop.FILTER_ATTRIBUTE_NUM)]
				elm['attr']['value'] = [0 for x in xrange(offlineshop.FILTER_ATTRIBUTE_NUM)]
			
			elm['attr'][key.replace('filter_attr_', '')][args[0]] = args[1]


	
	def ClearFilterPatterns( self):
		self.FilterPatterns = {}
	
	def AllocFilterPattern( self , id):
		self.FilterPatterns[id] = {"id": id,}
	
	def SetFilterPatternValue( self, key, idx, *args):
		elm = self.FilterPatterns[idx]
		
		if key == "datetime":
			elm["minute"]	= args[0]
			elm["hour"]		= args[1]
			elm["day"]		= args[2]
			elm["month"]	= args[3]
			elm["year"]		= args[4]
		
		elif key in ('filter_type' , 'filter_subtype', 'filter_name' ,'filter_wearflag', 'name', 'rarity'):
			elm[key] = args[0]
		
		
		elif key in ('filter_price_yang_start', 'filter_price_yang_end'):
			if not 'price' in elm:
				elm['price'] = {}
			
			elm['price'][key.replace('filter_price_yang_', '')] = args[0]
		
		
		elif key in ('filter_level_start', 'filter_level_end'):
			if not 'level' in elm:
				elm['level'] = {}
			
			elm['level'][key.replace('filter_level_', '')] = args[0]
		
		
		elif key in ('filter_attr_type' , 'filter_attr_value'):
			if not 'attr' in elm:
				elm['attr'] = {}
				elm['attr']['type']  = [0 for x in xrange(offlineshop.FILTER_ATTRIBUTE_NUM)]
				elm['attr']['value'] = [0 for x in xrange(offlineshop.FILTER_ATTRIBUTE_NUM)]
			
			elm['attr'][key.replace('filter_attr_', '')][args[0]] = args[1]
	
	
	def GetSearchFilterSettings(self):
		name		= ""
		raceflag	= 0
		type		= 0
		subtype		= SUBTYPE_NOSET
		levelmin	= 0
		levelmax	= 0
		
		yangmin		= 0
		yangmax		= 0
		rarity		= -1
		attributes	= tuple([(0,0) for x in xrange(player.ATTRIBUTE_SLOT_NORM_NUM)])
		
		
		
		if self.SearchFilterCheckBoxes["name"].IsEnabled():
			name = self.SearchFilterItemNameInput.GetText()
		
		if self.SearchFilterCheckBoxes["type"].IsEnabled():
			type	= self.SearchFilterTypeComboBoxIndex
			subtype = self.SearchFilterSubTypeComboBoxIndex
		
		if self.SearchFilterCheckBoxes["price"].IsEnabled():
			yangminst 	= self.SearchFilterItemYangMin.GetText()
			yangmaxst 	= self.SearchFilterItemYangMax.GetText()
			
			if yangminst and yangminst.isdigit():
				yangmin = long(yangminst)
			if yangmaxst and yangmaxst.isdigit():
				yangmax = long(yangmaxst)
		
		if self.SearchFilterCheckBoxes["level"].IsEnabled():
			levelminst = self.SearchFilterItemLevelStart.GetText()
			levelmaxst = self.SearchFilterItemLevelEnd.GetText()
			
			if levelminst and levelminst.isdigit():
				levelmin = int(levelminst)
			if levelmaxst and levelmaxst.isdigit():
				levelmax = int(levelmaxst)
		
		
		if self.SearchFilterCheckBoxes["wear"].IsEnabled():
			raceFlagDct = {
				"warrior"	: item.ITEM_ANTIFLAG_WARRIOR,
				"assassin"	: item.ITEM_ANTIFLAG_ASSASSIN,
				"sura" 		: item.ITEM_ANTIFLAG_SURA,
				"shaman" 	: item.ITEM_ANTIFLAG_SHAMAN,
			}
			
			if ENABLE_WOLFMAN_CHARACTER:
				raceFlagDct.update({
					"wolfman" : item.ITEM_ANTIFLAG_WOLFMAN,
				})
			
			
			for k,v in raceFlagDct.items():
				if not self.SearchFilterCheckBoxesRace[k].IsEnabled():
					raceflag |= v
			
			
		
		if self.GradeFilterWindow and self.GradeFilterWindow.IsShow() and self.GradeFilterCheckBox.IsEnabled():
			rarity = self.GradeFilterSuggestionsIndex
		
		
		return (type, subtype, name, (yangmin,yangmax), (levelmin, levelmax), raceflag, attributes, rarity)



	def __IsSaleableSlot(self, win , pos):
		if win == player.INVENTORY:
			if player.IsEquipmentSlot(pos):
				return False

		if self.pageCategory in ("create_shop", "my_shop" ) and self.IsForSaleSlot(win, pos):
			return False

		if self.pageCategory == "create_auction" and self.IsForAuctionSlot(win,pos):
			return False

		
		try:
			if app.ENABLE_EXTRA_INVENTORY:
				if not win in (player.INVENTORY, player.DRAGON_SOUL_INVENTORY, player.EXTRA_INVENTORY):
					return False
			else:
				if not win in (player.INVENTORY, player.DRAGON_SOUL_INVENTORY):
					return False
		
		except:
			if not win in (player.INVENTORY, player.DRAGON_SOUL_INVENTORY):
				return False
		
		try:
			if ENABLE_SOULBIND_SYSTEM:
				itemSealDate = player.GetItemSealDate(win, pos)
				if itemSealDate != item.E_SEAL_DATE_DEFAULT_TIMESTAMP:
					return False
		except:
			pass

		itemIndex = player.GetItemIndex(win,pos)
		if itemIndex == 0:
			return False


		item.SelectItem(itemIndex)
		if item.IsAntiFlag(item.ITEM_ANTIFLAG_MYSHOP) or item.IsAntiFlag(item.ITEM_ANTIFLAG_GIVE):
			return False


		return True


	def ShopBuilding_AddInventoryItem(self, slot):
		if player.GetItemIndex(player.INVENTORY, slot) ==0:
			return

		if player.IsEquipmentSlot(slot):
			return

		if self.IsForSaleSlot(player.INVENTORY,slot):
			return
		
		if self.__IsSaleableSlot(player.INVENTORY, slot):
			self.OpenInsertPriceDialog(player.INVENTORY,slot)


	def ShopBuilding_AddItem(self, win, pos):
		if self.__IsSaleableSlot(win, pos):
			self.OpenInsertPriceDialog(win,pos)



	def AuctionBuilding_AddInventoryItem(self, slot):
		if player.GetItemIndex(player.INVENTORY, slot) ==0:
			return

		if player.IsEquipmentSlot(slot):
			return

		if self.IsForAuctionSlot(player.INVENTORY, slot):
			return

		self.__OnSetCreateAuctionSlot(player.INVENTORY, slot)


	def AuctionBuilding_AddItem(self, win, pos):
		if self.__IsSaleableSlot(win, pos):
			self.__OnSetCreateAuctionSlot(win, pos)

	def SearchFilter_BuyFromSearch(self, ownerid, itemid): #patchme
		if self.pageCategory != 'search_filter':
			return

		for item in self.SearchFilterShopItemResult:
			if item['id'] == itemid and item['owner'] == ownerid:
				self.SearchFilterShopItemResult.remove(item)
				break

		self.SearchFilterResultItemsTable.ClearElement()

		for info in self.SearchFilterShopItemResult:
			slot = Slot()
			slot.SetInfo(info)
			slot.SetIndex((info["id"], info["owner"]))
			slot.SetOnMouseLeftButtonUpEvent(self.__OnLeftClickSearchFilterShopResultItem)
			slot.Show()

			self.SearchFilterResultItemsTable.AddElement(slot)


	def ShopSafebox_Clear(self):
		self.ShopSafeboxItems = []
		self.ShopSafeboxItemsTable.ClearElement()


	if ENABLE_CHEQUE_SYSTEM:
		def ShopSafebox_SetValutes(self, yang, cheque):
			self.ShopSafeboxValuteAmount = (yang, cheque)
			self.ShopSafeboxValuteText.SetText(localeinfo.NumberToMoneyString(yang))
			self.ShopSafeboxValuteTextCheque.SetText(localeinfo.NumberToChequeString(cheque))
	else:
		def ShopSafebox_SetValutes(self, yang):
			self.ShopSafeboxValuteAmount = yang
			self.ShopSafeboxValuteText.SetText(localeinfo.NumberToMoneyString(yang))

	def ShopSafebox_AllocItem(self):
		self.ShopSafeboxItems.append({})

	def ShopSafebox_SetValue(self, key , *args):
		elm = self.ShopSafeboxItems[-1]

		if key in ("id", "vnum", "count", 'trans', 'cheque', 'locked_attr'):
			elm[key] = args[0]

		elif key == "socket":
			if not key in elm:
				elm[key] = [0 for x in xrange(player.METIN_SOCKET_MAX_NUM)]
			elm[key][args[0]] = args[1]

		elif key in ("attr_type", "attr_value"):
			if not 'attr' in elm:
				elm['attr'] = {}

			if not args[0] in elm['attr']:
				elm['attr'][args[0]] = {}

			elm['attr'][args[0]][key.replace('attr_','')] = args[1]

	def ShopSafebox_RefreshEnd(self):
		for item in self.ShopSafeboxItems:
			slot = Slot()
			slot.SetIndex(item["id"])
			slot.SetInfo(item)
			slot.SetOnMouseLeftButtonUpEvent(self.__OnLeftClickShopSafeboxItem)
			slot.Show()

			self.ShopSafeboxItemsTable.AddElement(slot)

		if not self.pageBoards["shop_safebox"].IsShow():
			self.pageCategory = "shop_safebox"

			for page in self.pageBoards.values():
				page.Hide()

			self.pageBoards["shop_safebox"].Show()

		self.DisableRefreshSymbol()



	def OfferList_Clear(self):
		self.MyOffersList = []

	def OfferList_AddOffer(self , shopname, offer_id, buyer_id, owner_id, item_id, yang, is_notified, is_accept):
		self.MyOffersList.append({})
		elm = self.MyOffersList[-1]

		if '@' in shopname:
			shopname = shopname[shopname.find('@') + 1:]

		elm["shop_name"] 	= shopname
		elm["offer_id"]		= offer_id
		elm["buyer_id"]		= buyer_id
		elm["owner_id"]		= owner_id
		elm["item_id"]		= item_id
		elm["price"]		= yang
		elm["is_notified"]	= is_notified
		elm["is_accept"]	= is_accept

	def	OfferList_ItemSetValue(self, key , *args):
		offer = self.MyOffersList[-1]
		if not offer.has_key('item'):
			offer['item']={}

		elm = offer['item']

		if key in ("id", "vnum", "count", "owner", "price", 'trans', 'locked_attr'):
			elm[key] = args[0]

		elif key == "socket":
			if not key in elm:
				elm[key] = [0 for x in xrange(player.METIN_SOCKET_MAX_NUM)]
			elm[key][args[0]] = args[1]

		elif key in ("attr_type", "attr_value"):
			if not 'attr' in elm:
				elm['attr'] = {}

			if not args[0] in elm['attr']:
				elm['attr'][args[0]] = {}

			elm['attr'][args[0]][key.replace('attr_','')] = args[1]

	def OfferList_End(self):
		self.RefreshMyOffersPage()
		self.DisableRefreshSymbol()


	def AuctionList_Clear(self):
		self.AuctionListInfo = []
		self.AuctionListTable.ClearElement()

	def AuctionList_Alloc(self):
		self.AuctionListInfo.append({"item":{},})

	def AuctionList_SetInto(self, ownerid, owner_name, duration, init_yang, best_yang, offer_count):
		elm = self.AuctionListInfo[-1]

		info = {
			"owner_id" 		: ownerid,
			"owner_name"	: owner_name,
			"duration"		: duration,
			"init_yang"		: init_yang,
			"best_yang"		: best_yang,
			"offer_count"	: offer_count,
		}

		for k,v in info.items():
			elm[k] = v



	def AuctionList_SetItemInfo(self, vnum, count, *args):
		elm = self.AuctionListInfo[-1]['item']
		elm['count'] 	= count
		elm['vnum']		= vnum

		try:
			if ENABLE_CHANGELOOK_SYSTEM and len(args) !=0 :
				elm['trans'] = args[0]
		except:
			pass

	if app.ATTR_LOCK:
		def AuctionList_SetItemLockedAttr(self, attr):
			elm = self.AuctionListInfo[-1]['item']
			elm['locked_attr'] = attr


	def AuctionList_SetItemSocket(self, index, value):
		elm = self.AuctionListInfo[-1]['item']
		if not 'socket' in elm:
			elm['socket'] = [0 for x in xrange(player.METIN_SOCKET_MAX_NUM)]

		elm['socket'][index] = value

	def AuctionList_SetItemAttribute(self, key, index, value):
		elm = self.AuctionListInfo[-1]['item']
		if not 'attr' in elm:
			elm['attr'] = [{'type' :0, 'value': 0,} for x in xrange(player.ATTRIBUTE_SLOT_MAX_NUM)]
		elm['attr'][index][key] = value


	def AuctionList_End(self):
		self.RefreshAuctionListPage()
		self.DisableRefreshSymbol()

	def AuctionList_End_Owner(self):
		self.RefreshAuctionListPage()
		self.CreateAuctionWindowPos =-1
		self.CreateAuctionSlotPos=-1
		self.CreateAuctionDaysInput.SetText("0")
		self.CreateAuctionStartingPriceInput.SetText("")
		self.__MakeCreateAuctionSlot()
		self.MyAuction_NoAuction()


	def MyAuction_Clear(self):
		self.MyAuctionInfo = {'item':{},}


	def MyAuction_SetInto(self, owner_id, owner_name, duration, init_yang):
		info = {
			"owner_id" 		: owner_id,
			"owner_name"	: owner_name,
			"duration"		: duration,
			"init_yang"		: init_yang,
		}

		for k ,v in info.items():
			self.MyAuctionInfo[k] = v


	def MyAuction_SetItemInfo(self, vnum, count, *args):
		elm = self.MyAuctionInfo['item']
		elm['vnum']  = vnum
		elm['count'] = count

		try:
			if ENABLE_CHANGELOOK_SYSTEM and len(args)>0:
				elm['trans'] = args[0]
		except:
			pass
	
	if app.ATTR_LOCK:
		def MyAuction_SetItemLockedAttr(self, attr):
			elm = self.MyAuctionInfo['item']
			elm['locked_attr'] = attr

	def MyAuction_SetItemSocket(self, index , value):
		elm = self.MyAuctionInfo['item']
		if not 'socket' in elm:
			elm['socket'] = [0 for x in xrange(player.METIN_SOCKET_MAX_NUM)]

		elm['socket'][index] = value

	def MyAuction_SetItemAttribute(self, key , index , value):
		elm = self.MyAuctionInfo['item']
		if not 'attr' in elm:
			elm['attr'] = [{'type' :0, 'value': 0,} for x in xrange(player.ATTRIBUTE_SLOT_MAX_NUM)]
		elm['attr'][index][key] = value


	def MyAuction_AddOffer(self, buyer_id, buyer_name, owner_id, price_yang):
		if not 'offers' in self.MyAuctionInfo:
			self.MyAuctionInfo['offers'] = []

		new = {}
		new['buyer_name'] = buyer_name
		new['price_yang'] = price_yang

		self.MyAuctionInfo['offers'].append(new)



	def MyAuction_End(self):
		self.RefreshMyAuctionPage()
		self.DisableRefreshSymbol()



	def OpenAuction_Clear(self):
		self.OpenAuctionInfo = {'item':{},}


	def OpenAuction_SetInto(self, owner_id, owner_name, duration, init_yang):
		info = {
			"owner_id" 		: owner_id,
			"owner_name"	: owner_name,
			"duration"		: duration,
			"init_yang"		: init_yang,
		}

		for k ,v in info.items():
			self.OpenAuctionInfo[k] = v
	
	if app.ATTR_LOCK:
		def OpenAuction_SetItemLockedAttr(self, attr):
			elm = self.OpenAuctionInfo['item']
			elm['locked_attr'] = attr

	def OpenAuction_SetItemInfo(self, vnum, count, *args):
		elm = self.OpenAuctionInfo['item']
		elm['vnum']  = vnum
		elm['count'] = count

		try:
			if ENABLE_CHANGELOOK_SYSTEM and len(args!=0):
				elm['trans'] = args[0]
		except:
			pass


	def OpenAuction_SetItemSocket(self, index , value):
		elm = self.OpenAuctionInfo['item']
		if not 'socket' in elm:
			elm['socket'] = [0 for x in xrange(player.METIN_SOCKET_MAX_NUM)]

		elm['socket'][index] = value

	def OpenAuction_SetItemAttribute(self, key , index , value):
		elm = self.OpenAuctionInfo['item']
		if not 'attr' in elm:
			elm['attr'] = [{'type' :0, 'value': 0,} for x in xrange(player.ATTRIBUTE_SLOT_MAX_NUM)]
		elm['attr'][index][key] = value


	def OpenAuction_AddOffer(self, buyer_id, buyer_name, owner_id, price_yang):
		if not 'offers' in self.OpenAuctionInfo:
			self.OpenAuctionInfo['offers'] = []

		new = {}
		new['buyer_name'] = buyer_name
		new['price_yang'] = price_yang

		self.OpenAuctionInfo['offers'].append(new)



	def OpenAuction_End(self):
		self.RefreshOpenAuctionPage()
		self.DisableRefreshSymbol()

	def MyAuction_NoAuction(self):
		self.pageCategory = "create_auction"
		for page in self.pageBoards.values():
			page.Hide()

		self.pageBoards['create_auction'].Show()
		self.DisableRefreshSymbol()



	def __GetLastPriceStorePath(self):
		# stored in the client folder inside ./offlineshop/
		# (exe dir is more stable than cwd for packed clients)
		try:
			import sys
			base = os.path.dirname(os.path.abspath(sys.argv[0]))
		except:
			base = os.getcwd()

		folder = os.path.join(base, "offlineshop")
		try:
			if not os.path.exists(folder):
				os.makedirs(folder)
		except:
			pass

		return os.path.join(folder, "offlineshop_last_prices.txt")

	def __LoadLastPrices(self):
		"""Load stored *unit* prices by vnum from a simple TXT file."""
		self._lastPriceByVnum = {}
		self._lastPriceStorePath = self.__GetLastPriceStorePath()
		try:
			if not os.path.exists(self._lastPriceStorePath):
				return
			f = open(self._lastPriceStorePath, "r")
			for line in f.readlines():
				line = line.strip()
				if not line or line.startswith("#"):
					continue
				# format: vnum\tyang\tcheque
				parts = line.split("\t")
				if len(parts) < 2:
					# allow space separated too
					parts = line.split()
				if len(parts) < 2:
					continue
				try:
					vnum = int(parts[0])
					yang = int(parts[1])
					cheque = 0
					if len(parts) >= 3:
						cheque = int(parts[2])
				except:
					continue
				if vnum <= 0:
					continue
				if yang <= 0 and cheque <= 0:
					continue
				self._lastPriceByVnum[vnum] = {"yang": yang, "cheque": cheque}
			f.close()
		except:
			pass

	def __SaveLastPrices(self):
		"""Save stored *unit* prices by vnum into a TXT file."""
		try:
			path = self._lastPriceStorePath if self._lastPriceStorePath else self.__GetLastPriceStorePath()

			# keep file small
			limit = 500
			count = 0

			f = open(path, "w")
			f.write("# vnum\tyang_unit\tcheque_unit\n")
			for vnum in sorted(self._lastPriceByVnum.keys()):
				info = self._lastPriceByVnum.get(vnum, {})
				yang = int(info.get("yang", 0))
				cheque = int(info.get("cheque", 0))
				if yang <= 0 and cheque <= 0:
					continue
				f.write("%d\t%d\t%d\n" % (int(vnum), yang, cheque))
				count += 1
				if count >= limit:
					break
			f.close()
		except:
			pass

	def __ClampYang(self, yang):
		"""Clamp yang values to [0, OFFLINESHOP_GOLD_MAX_YANG] to avoid overflows."""
		try:
			y = long(yang)
		except:
			try:
				y = int(yang)
			except:
				return 0
		if y < 0:
			return 0
		if y > OFFLINESHOP_GOLD_MAX_YANG:
			return OFFLINESHOP_GOLD_MAX_YANG
		return y

	def __RememberLastPrice(self, vnum, total_yang, total_cheque=0, count=1):
		"""Remember *unit* price by vnum so stacks can be auto-scaled later."""
		try:
			vnum = int(vnum)
			total_yang = int(total_yang)
			total_cheque = int(total_cheque)
			count = int(count)
		except:
			return

		if vnum <= 0:
			return

		if count <= 0:
			count = 1

		total_yang = self.__ClampYang(total_yang)

		if total_yang <= 0 and total_cheque <= 0:
			return

		# store per-piece price (rounded) to support stacks: price_for_stack = unit * count
		unit_yang = 0
		unit_cheque = 0

		if total_yang > 0:
			unit_yang = int((total_yang + (count // 2)) // count)
			if unit_yang <= 0:
				unit_yang = total_yang

		if ENABLE_CHEQUE_SYSTEM and total_cheque > 0:
			unit_cheque = int((total_cheque + (count // 2)) // count)
			if unit_cheque <= 0:
				unit_cheque = total_cheque

		self._lastPriceByVnum[vnum] = {"yang": unit_yang, "cheque": unit_cheque}
		self.__SaveLastPrices()

	def __MoneyDlgSetYang(self, yang):
		# uicommon MoneyInputDialog / MoneyInputDialogCheque compatibility
		try:
			self.CommonInputPriceDlg.SetText(str(int(yang)))
			return
		except:
			pass

		for attr_name in ("moneyInput", "moneyValue", "editLine", "moneyEditLine", "inputValue"):
			try:
				getattr(self.CommonInputPriceDlg, attr_name).SetText(str(int(yang)))
				return
			except:
				pass

	def __MoneyDlgSetCheque(self, cheque):
		if not ENABLE_CHEQUE_SYSTEM:
			return
		try:
			self.CommonInputPriceDlg.SetChequeText(str(int(cheque)))
			return
		except:
			pass

		for attr_name in ("chequeInput", "chequeValue", "chequeEditLine", "editLineCheque", "inputChequeValue"):
			try:
				getattr(self.CommonInputPriceDlg, attr_name).SetText(str(int(cheque)))
				return
			except:
				pass

	def __ApplyRememberedPriceToDialog(self, vnum, count=1, fallback_yang=0, fallback_cheque=0):
		"""Fill the dialog with remembered *stack* price (unit * count)."""
		try:
			count = int(count)
		except:
			count = 1
		if count <= 0:
			count = 1

		info = self._lastPriceByVnum.get(vnum, None)

		unit_yang = 0
		unit_cheque = 0
		if info:
			unit_yang = int(info.get("yang", 0))
			unit_cheque = int(info.get("cheque", 0))
		else:
			try:
				unit_yang = int(fallback_yang)
			except:
				unit_yang = 0
			try:
				unit_cheque = int(fallback_cheque)
			except:
				unit_cheque = 0

		yang = unit_yang * count if unit_yang > 0 else 0
		cheque = unit_cheque * count if unit_cheque > 0 else 0
		yang = self.__ClampYang(yang)

		if yang > 0:
			self.__MoneyDlgSetYang(yang)

		if ENABLE_CHEQUE_SYSTEM and cheque > 0:
			self.__MoneyDlgSetCheque(cheque)

	def OpenInsertPriceDialog(self, win, slot):
		self.AddItemSlotIndex = (win,slot)
		self.CommonInputPriceDlg.Open()
		try:
			vnum = player.GetItemIndex(win, slot)
			count = player.GetItemCount(win, slot)
			self.__ApplyRememberedPriceToDialog(vnum, count)
			self.__SetMarketAvgOnPriceDialog(vnum, count)
			# if we have no (or stale) info, refresh from server silently
			try:
				if self.__NeedMarketAvgRefresh(vnum):
					self.__RequestMarketAvgRefresh(vnum, count)
			except:
				pass
		except:
			pass
	
	def __OnAcceptMyShopAcceptOffer(self):
		offlineshop.SendOfferAccept(self.MyShopAcceptOfferID)
		self.CommonQuestionDlg.Hide()
		self.MyShopAcceptOfferID = -1


	def __OnCancelMyShopAcceptOffer(self):
		self.CommonQuestionDlg.Hide()
		self.MyShopAcceptOfferID = -1

	def __OnAcceptMyShopCancelOffer(self):
		offlineshop.SendOfferCancel(self.MyShopCancelOfferID, self.ShopOpenInfo['owner_id'])
		self.CommonQuestionDlg.Hide()
		self.MyShopCancelOfferID = -1

	def __OnCancelMyShopCancelOffer(self):
		self.CommonQuestionDlg.Hide()
		self.MyShopCancelOfferID = -1




	def __OnCancelCancelOfferQuestion(self):
		self.CommonQuestionDlg.Hide()
		self.MyOffersCancelOfferInfo = []


	def __OnAcceptCancelOfferQuestion(self):
		info = self.MyOffersCancelOfferInfo
		offlineshop.SendOfferCancel(info[0], info[1])

		self.CommonQuestionDlg.Hide()
		self.MyOffersCancelOfferInfo = []

	def __OnAcceptInputPrice(self):
		yang = self.CommonInputPriceDlg.GetText()
		if not yang.isdigit():
			return
		
		yang = int(yang)
		yang = self.__ClampYang(yang)
		if not ENABLE_CHEQUE_SYSTEM:
			if yang == 0:
				return
		
		if ENABLE_CHEQUE_SYSTEM:
			cheque = self.CommonInputPriceDlg.GetChequeText()
			if cheque.isdigit():
				cheque = int(cheque)
			else:
				cheque = 0
		
		
			if yang == 0 and cheque == 0:
				return
		
		

		# remember last entered price for this vnum (autofill next time)
		try:
			if self.AddItemSlotIndex != -1:
				win, slot = self.AddItemSlotIndex
				vnum = player.GetItemIndex(win, slot)
				count = player.GetItemCount(win, slot)
				self.__RememberLastPrice(vnum, yang, cheque if ENABLE_CHEQUE_SYSTEM else 0, count)
			elif self.EditPriceSlot != None:
				info = self.EditPriceSlot.GetInfo()
				self.__RememberLastPrice(info.get('vnum', 0), yang, cheque if ENABLE_CHEQUE_SYSTEM else 0, info.get('count', 1))
		except:
			pass

		if self.pageCategory == "create_shop" and self.AddItemSlotIndex != -1:
			slot = Slot()
			slot.SetIndex(self.AddItemSlotIndex)
			if ENABLE_CHEQUE_SYSTEM:
				slot.SetInfo(MakeSlotInfo(self.AddItemSlotIndex[0] , self.AddItemSlotIndex[1], yang, cheque))
			else:
				slot.SetInfo(MakeSlotInfo(self.AddItemSlotIndex[0] , self.AddItemSlotIndex[1], yang))
			slot.SetOnMouseLeftButtonUpEvent(self.__OnLeftClickInsertedItem)
			slot.Show()
			
			self.CreateShopItemsTable.AddElement(slot)
			self.CreateShopItemsInfos[slot.GetIndex()] = slot.GetInfo()
			self.__HidePriceDialog()
			
			# if self.HowToInsertItemText.IsShow():
			# 	self.HowToInsertItemText.Hide()
			
			self.AddItemSlotIndex = -1
		
		
		elif self.pageCategory == "create_shop" and self.EditPriceSlot != None:
			slot  = self.EditPriceSlot
			index = slot.GetIndex()
			
			info 			= slot.GetInfo()
			info["price"] 	= yang
			if ENABLE_CHEQUE_SYSTEM:
				info['cheque'] = cheque
			self.CreateShopItemsInfos[slot.GetIndex()] = info
			self.EditPriceSlot = None
			self.__HidePriceDialog()
		
		
		elif self.pageCategory == "my_shop" and self.AddItemSlotIndex != -1:
			if ENABLE_CHEQUE_SYSTEM:
				offlineshop.SendAddItem(self.AddItemSlotIndex[0],  self.AddItemSlotIndex[1],  yang, cheque)
			else:
				offlineshop.SendAddItem(self.AddItemSlotIndex[0],  self.AddItemSlotIndex[1],  yang)
			self.AddItemSlotIndex = -1
			self.__HidePriceDialog()
		
		elif self.pageCategory == "my_shop" and self.EditPriceSlot != None:
			slot  = self.EditPriceSlot
			index = slot.GetIndex()
			if ENABLE_CHEQUE_SYSTEM:
				offlineshop.SendEditItem(index, yang, cheque)
			else:
				offlineshop.SendEditItem(index, yang)
			self.__HidePriceDialog()
			self.EditPriceSlot = None
	
	def __OnCancelInputPrice(self):
		self.__HidePriceDialog()
		self.AddItemSlotIndex 	= -1
		self.EditPriceSlot		= None
	
	
	
	def __OnAcceptOpenShopBuyItemQuestion(self):
		self.CommonQuestionDlg.Hide()
		offlineshop.SendBuyItem(self.ShopOpenInfo["owner_id"], self.OpenShopBuyItemID, self.BuyPriceSeenTotal)
		import chat
		chat.AppendChat(1, "%s %s %s" % (self.ShopOpenInfo["owner_id"], self.OpenShopBuyItemID, str(self.BuyPriceSeenTotal)))
		self.OpenShopBuyItemID = -1

	
	def __OnCancelOpenShopBuyItemQuestion(self):
		self.CommonQuestionDlg.Hide()
		self.OpenShopBuyItemID = -1

	def __OnAcceptSearchFilterBuyItemQuestion(self):
		id,owner = self.SearchFilterResultClickedInfo
		offlineshop.SendBuyItemFromSearch(owner, id, self.BuyPriceSeenTotal)
		self.CommonQuestionDlg.Hide()
		self.SearchFilterResultClickedInfo = []

	def __OnCancelSearchFilterBuyItemQuestion(self):
		self.CommonQuestionDlg.Hide()
		self.SearchFilterResultClickedInfo = []

	def __OnAcceptChangeShopNameDlg(self):
		newname = self.MyShopEditNameDlg.GetText()
		self.MyShopEditNameDlg.Hide()
		
		offlineshop.SendChangeName(newname)
	
	def __OnCancelChangeShopNameDlg(self):
		self.MyShopEditNameDlg.Hide()


	def __OnAcceptMyPatternInputName(self):
		name = self.SearchPatternsInputNameDlg.GetText()
		self.SearchPatternsInputNameDlg.Hide()
		if name:
			setting = self.GetSearchFilterSettings()
			setting = (name,) + setting
			offlineshop.AppendNewFilterPattern(*setting)
			self.__PopupMessage(localeinfo.OFFLINESHOP_PATTERN_SAVED_SUCCESS)

	def __OnCancelMyPatternInputName(self):
		self.SearchPatternsInputNameDlg.Hide()


	def __OnAcceptShopSafeboxGetItemQuestion(self):
		self.CommonQuestionDlg.Hide()
		offlineshop.SendSafeboxGetItem(self.ShopSafeboxGetItemIndex)
		self.ShopSafeboxGetItemIndex = -1



	def __OnCancelShopSafeboxGetItemQuestion(self):
		self.CommonQuestionDlg.Hide()
		self.ShopSafeboxGetItemIndex = -1


	if ENABLE_CHEQUE_SYSTEM:
		def __OnAcceptShopSafeboxGetValuteInput(self , yang, cheque):
			offlineshop.SendSafeboxGetValutes(yang, cheque)
	else:
		def __OnAcceptShopSafeboxGetValuteInput(self , yang):
			offlineshop.SendSafeboxGetValutes(yang)

	if ENABLE_CHEQUE_SYSTEM:
		def __OnAcceptMakeOffer(self, yang,  cheque):
			offlineshop.SendOfferCreate(self.MakeOfferOwnerID, self.MakeOfferItemID, yang, cheque)
			self.MakeOfferOwnerID 	= -1
			self.MakeOfferItemID	= -1
	else:
		def __OnAcceptMakeOffer(self, yang):
			offlineshop.SendOfferCreate(self.MakeOfferOwnerID, self.MakeOfferItemID, yang)
			self.MakeOfferOwnerID 	= -1
			self.MakeOfferItemID	= -1

	def __OnClickCreateShopButton(self):
		
		
		
		
		
		
		shopname = self.CreateShopNameEdit.GetText()
		if not shopname:
			self.__PopupMessage(localeinfo.OFFLINESHOP_CREATE_SHOP_NO_NAME_INSERTED)
			return
		
		days  = int(self.CreateShopDaysCountText.GetText())
		hours = int(self.CreateShopHoursCountText.GetText())
		
		totaltime = 0
		
		elements = self.CreateShopItemsTable.GetElementDict()
		if not elements:
			self.__PopupMessage(localeinfo.OFFLINESHOP_CREATE_SHOP_NO_ITEMS_INSERTED)
			return
		
		itemLst = []
		
		for dct in elements.values():
			for item in dct.values():
				info = item.GetInfo()
				
				if ENABLE_CHEQUE_SYSTEM:
					tupleinfo = (info["window"], info["slot"] , info["price"] , info.get('cheque', 0))
				else:
					tupleinfo = (info["window"], info["slot"] , info["price"] )
				itemLst.append(tupleinfo)
		itemTuple = tuple(itemLst)
		if app.KASMIR_PAKET_SYSTEM:
			offlineshop.SendShopCreate(shopname, totaltime, self.KasmirNpc, itemTuple)
		else:
			offlineshop.SendShopCreate(shopname, totaltime, itemTuple)
		
		self.EnableRefreshSymbol()

	if app.KASMIR_PAKET_SYSTEM:
		def OpeningFailded(self):
			self.DisableRefreshSymbol()
			if self.pageBoards["create_shop"].IsShow():
				self.KasmirNpc = 30000
				#renderTarget.SelectModel(2, self.KasmirNpc)


	def __TryOpenWikiChestLootFromSlot(self, slot):
		"""ALT+bal klikk ladan -> Wiki lada loot popup (offline shop slotbol)."""
		# Ingame wiki nem elerheto / nincs engedelyezve
		try:
			if hasattr(app, "INGAME_WIKI") and not app.INGAME_WIKI:
				return False
		except:
			pass

		# Modifier: ALT
		try:
			is_alt = app.IsPressed(app.DIK_LALT) or (hasattr(app, 'DIK_RALT') and app.IsPressed(app.DIK_RALT))
		except:
			is_alt = False
		if not is_alt:
			return False

		# Tiszteljk a konfiguraciot, de ha nincs, akkor is ALT.
		try:
			import ingamewikiconfig
			mod = getattr(ingamewikiconfig, 'WIKI_CHEST_LOOT_MODIFIER', 'ALT')
			try:
				mod = mod.upper()
			except:
				mod = 'ALT'
			if mod != 'ALT':
				return False
		except:
			pass

		# Slot vnum
		try:
			info = slot.GetInfo()
			vnum = int(info.get('vnum', 0))
		except:
			vnum = 0
		if not vnum:
			return False

		# Csak ladak
		try:
			if vnum not in getattr(uitooltip, 'CHEST_VNUMS', ()):
				return False
		except:
			return False

		# Nyitas: Wiki chest loot popup
		try:
			import wiki
			wikiBase = wiki.GetBaseClass()
			if wikiBase and hasattr(wikiBase, 'OpenChestLootPopup'):
				wikiBase.OpenChestLootPopup(vnum)
				return True
		except:
			pass

		# Fallback: wiki foablak
		try:
			import net
			net.ToggleWikiWindow()
			return True
		except:
			pass

		return False


	def __OnLeftClickInsertedItem(self, slot):
		if mousemodule.mouseController.isAttached():
			if self.pageCategory == "create_shop":
				self.__OnClickCreateShopEmptySlot()
			elif self.pageCategory == "my_shop":
				self.__OnClickMyShopEmptySlot()
			return


		# Wiki chest loot: ALT+bal klikk ladan (offline shop)
		try:
			if self.__TryOpenWikiChestLootFromSlot(slot):
				return
		except:
			pass

		if app.IsPressed(app.DIK_LCONTROL) or app.IsPressed(app.DIK_RCONTROL):
			self.__OnRemoveShopItem(slot)
		
		else:
			self.EditPriceSlot = slot
			self.CommonInputPriceDlg.Open()
			try:
				info = slot.GetInfo()
				self.__ApplyRememberedPriceToDialog(info.get('vnum', 0), info.get('count', 1), info.get('price', 0), info.get('cheque', 0))
				self.__SetMarketAvgOnPriceDialog(info.get('vnum', 0), info.get('count', 1))
			except:
				pass
	
	
	def __OnLeftClickShopItem(self, slot):
		# Wiki chest loot: ALT+bal klikk ladan (offline shop)
		try:
			if self.__TryOpenWikiChestLootFromSlot(slot):
				return
		except:
			pass

		if app.IsPressed(app.DIK_LCONTROL) or app.IsPressed(app.DIK_RCONTROL):
			import chat
			chat.AppendChat(chat.CHAT_TYPE_INFO, "Licit kikapcsolva - Razor")
			# itemid  = slot.GetIndex()
			# ownerid = self.ShopOpenInfo["owner_id"]

			#self.__OnMakeShopItemOffer(itemid, ownerid)


		else:
			self.OpenShopBuyItemID = slot.GetIndex()
			info = slot.GetInfo()
			self.BuyPriceSeenTotal = info['price']
			if ENABLE_CHEQUE_SYSTEM:
				self.BuyPriceSeenTotal += info.get('cheque', 0) * YANG_PER_CHEQUE
			item.SelectItem(info["vnum"])
			name 	= item.GetItemName()
			count	= info["count"]
			yang	= info["price"]
			if ENABLE_CHEQUE_SYSTEM:
				cheque	= info["cheque"]

			if count > 1:
				if ENABLE_CHEQUE_SYSTEM:
					text	= localeinfo.OFFLINESHOP_BUY_ITEM_QUESTION_COUNT2 % (name, count, yang, cheque)
				else:
					text	= localeinfo.OFFLINESHOP_BUY_ITEM_QUESTION_COUNT % (name, count, yang)
			else:
				if ENABLE_CHEQUE_SYSTEM:
					text	= localeinfo.OFFLINESHOP_BUY_ITEM_QUESTION2 % (name, yang, cheque)
				else:
					text	= localeinfo.OFFLINESHOP_BUY_ITEM_QUESTION % (name, yang)


			width		= 6 * len(text)
			accept_text	= localeinfo.OFFLINESHOP_BUY_ITEM_QUESTION_ACCEPT
			cancel_text	= localeinfo.OFFLINESHOP_BUY_ITEM_QUESTION_CANCEL
			accept_event= self.__OnAcceptOpenShopBuyItemQuestion
			cancel_event= self.__OnCancelOpenShopBuyItemQuestion

			self.__OpenQuestionDialog(width, accept_text, cancel_text, accept_event, cancel_event, text)


	
	
	def __OnLeftClickSearchFilterShopResultItem(self, slot):
		# Wiki chest loot: ALT+bal klikk ladan (offline shop)
		try:
			if self.__TryOpenWikiChestLootFromSlot(slot):
				return
		except:
			pass

		if app.IsPressed(app.DIK_LCONTROL) or app.IsPressed(app.DIK_LCONTROL):
			itemid,ownerid = slot.GetIndex()
			self.__OnMakeShopItemOffer(itemid, ownerid)
		else:
			self.SearchFilterResultClickedInfo = slot.GetIndex()
			info = slot.GetInfo()
			
			self.BuyPriceSeenTotal = info['price']
			if ENABLE_CHEQUE_SYSTEM:
				self.BuyPriceSeenTotal += info.get('cheque', 0) * YANG_PER_CHEQUE

			item.SelectItem(info["vnum"])
			name = item.GetItemName()
			count = info["count"]
			yang = info["price"]
			if ENABLE_CHEQUE_SYSTEM:
				cheque	= info["cheque"]

			if count > 1:
				if ENABLE_CHEQUE_SYSTEM:
					text = localeinfo.OFFLINESHOP_BUY_ITEM_QUESTION_COUNT2 % (name, count, yang, cheque)
				else:
					text = localeinfo.OFFLINESHOP_BUY_ITEM_QUESTION_COUNT % (name, count, yang)

			else:
				if ENABLE_CHEQUE_SYSTEM:
					text = localeinfo.OFFLINESHOP_BUY_ITEM_QUESTION2 % (name, yang, cheque)
				else:
					text = localeinfo.OFFLINESHOP_BUY_ITEM_QUESTION % (name, yang)

			width = 6 * len(text)
			accept_text	= localeinfo.OFFLINESHOP_BUY_ITEM_QUESTION_ACCEPT
			cancel_text	= localeinfo.OFFLINESHOP_BUY_ITEM_QUESTION_CANCEL
			accept_event = self.__OnAcceptSearchFilterBuyItemQuestion
			cancel_event = self.__OnCancelSearchFilterBuyItemQuestion

			self.__OpenQuestionDialog(width, accept_text, cancel_text, accept_event, cancel_event, text)
	

	


	def __OnClickCreateShopEmptySlot(self, *args):
		if not mousemodule.mouseController.isAttached():
			return

		type = mousemodule.mouseController.GetAttachedType()
		slot = mousemodule.mouseController.GetAttachedSlotNumber()
		
		#tofix slot type to window type
		type = player.SlotTypeToInvenType(type)
		
		
		mousemodule.mouseController.DeattachObject()
		self.ShopBuilding_AddItem(type,slot)


	def __OnClickMyShopEmptySlot(self, *args):
		# todebug
		print("my shop shop empty slot")

		if not mousemodule.mouseController.isAttached():
			return

		type = mousemodule.mouseController.GetAttachedType()
		slot = mousemodule.mouseController.GetAttachedSlotNumber()
		
		#tofix slot type to window type
		type = player.SlotTypeToInvenType(type)
		
		mousemodule.mouseController.DeattachObject()
		self.ShopBuilding_AddItem(type,slot)




	def __OnClickShopListOpenShop(self, id):
		offlineshop.SendOpenShop(id)
	

	def __OnClickOpenAuctionMakeOffer(self, slot):
		if ENABLE_CHEQUE_SYSTEM:
			self.__OnPickValute(localeinfo.OFFLINESHOP_AUCTION_MAKE_OFFER, self.__OnClickAcceptOpenAuctionPickValute, player.GetElk(), player.GetCheque())
		else:
			self.__OnPickValute(localeinfo.OFFLINESHOP_AUCTION_MAKE_OFFER, self.__OnClickAcceptOpenAuctionPickValute, player.GetElk())


	if ENABLE_CHEQUE_SYSTEM:
		def __OnClickAcceptOpenAuctionPickValute(self, yang, cheque):
			if not self.OpenAuctionInfo.has_key('min_raise'):
				return

			if (yang + (cheque * YANG_PER_CHEQUE)) < self.OpenAuctionInfo['min_raise']:
				self.__PopupMessage(localeinfo.OFFLINESHOP_AUCTION_MIN_RAISE%NumberToString(self.OpenAuctionInfo['min_raise']) )
				return

			offlineshop.SendAuctionAddOffer(self.OpenAuctionInfo['owner_id'] , yang, cheque)
	else:
		def __OnClickAcceptOpenAuctionPickValute(self, yang):
			if not self.OpenAuctionInfo.has_key('min_raise'):
				return

			if yang < self.OpenAuctionInfo['min_raise']:
				self.__PopupMessage(localeinfo.OFFLINESHOP_AUCTION_MIN_RAISE%NumberToString(self.OpenAuctionInfo['min_raise']) )
				return

			offlineshop.SendAuctionAddOffer(self.OpenAuctionInfo['owner_id'] , yang)


	def __OnClickShopSafeboxWithdrawYang(self):
		maxval = self.ShopSafeboxValuteAmount
		if ENABLE_CHEQUE_SYSTEM:
			self.__OnPickValute(localeinfo.OFFLINESHOP_SAFEBOX_GET_VALUTE_TITLE, self.__OnAcceptShopSafeboxGetValuteInput, maxval[0], maxval[1])
		else:
			self.__OnPickValute(localeinfo.OFFLINESHOP_SAFEBOX_GET_VALUTE_TITLE, self.__OnAcceptShopSafeboxGetValuteInput, maxval)


	def __OnClickMyShopOfferAcceptButton(self, offer):
		self.MyShopAcceptOfferID = offer.GetInfo()['id']

		question 	= localeinfo.OFFLINESHOP_ACCEPT_OFFER_QUESTION
		accepttext 	= localeinfo.OFFLINESHOP_ACCEPT_OFFER_QUESTION_ACCEPT
		canceltext 	= localeinfo.OFFLINESHOP_ACCEPT_OFFER_QUESTION_CANCEL

		self.__OpenQuestionDialog(len(question)*6 , accepttext, canceltext, self.__OnAcceptMyShopAcceptOffer, self.__OnCancelMyShopAcceptOffer, question)


	def __OnClickMyShopOfferCancelButton(self, offer):
		self.MyShopCancelOfferID = offer.GetInfo()['id']
		question 	= localeinfo.OFFLINESHOP_CANCEL_OFFER_QUESTION
		accepttext 	= localeinfo.OFFLINESHOP_CANCEL_OFFER_QUESTION_ACCEPT
		canceltext 	= localeinfo.OFFLINESHOP_CANCEL_OFFER_QUESTION_CANCEL

		self.__OpenQuestionDialog(len(question) * 6, accepttext, canceltext, self.__OnAcceptMyShopCancelOffer, self.__OnCancelMyShopCancelOffer, question)

	def __OnClickOpenAuctionButton(self, idx):
		offlineshop.SendAuctionOpenAuction(idx)
		self.EnableRefreshSymbol()




	def __OnLeftClickShopSafeboxItem(self, slot):
		# Wiki chest loot: ALT+bal klikk ladan (offline shop)
		try:
			if self.__TryOpenWikiChestLootFromSlot(slot):
				return
		except:
			pass

		id = slot.GetIndex()
		self.ShopSafeboxGetItemIndex = id


		if ENABLE_ITEM_WITHDRAW_QUESTION_SHOP_SAFEBOX:
			info = slot.GetInfo()

			vnum = info["vnum"]
			count = info["count"]

			item.SelectItem(vnum)
			name = item.GetItemName()
			if count > 1:
				text = localeinfo.OFFLINESHOP_SHOP_SAFEBOX_QUESTION_GET_ITEM_COUNT%(name,count)
			else:
				text = localeinfo.OFFLINESHOP_SHOP_SAFEBOX_QUESTION_GET_ITEM%name

			width = len(text)*6
			self.__OpenQuestionDialog(width, localeinfo.OFFLINESHOP_SHOP_SAFEBOX_ACCEPT, localeinfo.OFFLINESHOP_SAFEBOX_CANCEL, self.__OnAcceptShopSafeboxGetItemQuestion, self.__OnCancelShopSafeboxGetItemQuestion, text)

		else:
			self.__OnAcceptShopSafeboxGetItemQuestion()


	def __OnRemoveShopItem(self, slot):
		if self.pageCategory == "create_shop":
			self.__OnRemoveShopItemCreateShopPage(slot)
		
		elif self.pageCategory == "my_shop":
			self.__OnRemoveShopItemMyShop(slot)
	
	
	def __OnRemoveShopItemCreateShopPage(self, slot):
		slot.Hide()
		del self.CreateShopItemsInfos[slot.GetIndex()]
		self.CreateShopItemsTable.ClearElement()
		
		for k, v in self.CreateShopItemsInfos.items():
			newslot = Slot()
			newslot.SetIndex(k)
			newslot.SetInfo(v)
			newslot.SetOnMouseLeftButtonUpEvent(self.__OnLeftClickInsertedItem)
			newslot.Show()
			
			self.CreateShopItemsTable.AddElement(newslot)
		
		
	
	def __OnRemoveShopItemMyShop(self, slot):
		offlineshop.SendRemoveItem(slot.GetIndex())
	

	def __OnMakeShopItemOffer(self ,  itemid , ownerid):
		self.MakeOfferItemID = itemid
		self.MakeOfferOwnerID= ownerid

		title = localeinfo.OFFLINESHOP_MAKE_OFFER_TITLE

		if ENABLE_CHEQUE_SYSTEM:
			self.__OnPickValute(title, self.__OnAcceptMakeOffer, player.GetElk(), player.GetCheque())
		else:
			self.__OnPickValute(title, self.__OnAcceptMakeOffer, player.GetElk())


	def __OnSetCreateAuctionSlot(self, win,slot):
		vnum = player.GetItemIndex(win,slot)
		if vnum ==0:
			return

		item.SelectItem(vnum)
		if item.IsAntiFlag(item.ITEM_ANTIFLAG_MYSHOP) or item.IsAntiFlag(item.ITEM_ANTIFLAG_GIVE):
			return

		self.CreateAuctionWindowPos = win
		self.CreateAuctionSlotPos	= slot


		count 	= player.GetItemCount(win,slot)
		attrs 	= [{"type" : player.GetItemAttribute(win, slot, x)[0], "value" : player.GetItemAttribute(win, slot, x)[1]} for x in xrange(player.ATTRIBUTE_SLOT_MAX_NUM)]
		sockets = [player.GetItemMetinSocket(win,slot,x) for x in xrange(player.METIN_SOCKET_MAX_NUM)]
		if app.ATTR_LOCK:
			locked_attr = player.GetItemAttrLocked(win,slot)

		try:
			if ENABLE_CHANGELOOK_SYSTEM:
				trans = player.GetItemTransmutation(win, slot)
		except:
			pass

		info = {
			"vnum" 		: vnum,
			"count" 	: count,
			'attr'		: attrs,
			'socket'	: sockets,
		}
		
		if app.ATTR_LOCK:
			info.update({ 'locked_attr'  :  locked_attr,})

		try:
			if ENABLE_CHANGELOOK_SYSTEM:
				info.update({
					'trans' : trans,
				})
		except:
			pass


		self.CreateAuctionSlot.SetInfo(info)
		self.CreateAuctionSlot.SetOnMouseLeftButtonUpEvent(self.OnClickAuctionSlot)


	def IsBuildingShop(self):
		return self.IsShow() and (self.pageCategory == "create_shop" or self.pageCategory=="my_shop")



	
	def IsForSaleSlot(self,win,slot):
		idx = (win,slot)

		if not self.pageCategory=="create_shop" or not self.IsShow():
			return False
		
		if not self.CreateShopItemsTable:
			return False
		
		elementDict = self.CreateShopItemsTable.GetElementDict()
		for dct in elementDict.values():
			for value in dct.values():
				if value.GetIndex() == idx:
					return True
		return False

	def IsBuildingAuction(self):
		if not self.IsShow():
			return False

		if self.pageCategory != "create_auction":
			return False
		return True


	def IsForAuctionSlot(self, win, slot):
		if not self.IsShow():
			return False

		if self.pageCategory != "create_auction":
			return False

		return self.CreateAuctionWindowPos == win and self.CreateAuctionSlotPos == slot



	def __SetTooltip(self,info,canRemove=False):
		infocolor = COLOR_TEXT_SHORTCUT

		is_building = self.IsBuildingShop()
		
		self.itemTooltip.ClearToolTip()
		
		sockets = [info["socket"][num] for num in xrange(player.METIN_SOCKET_MAX_NUM)]
		attrs	= [(info["attr"][num]['type'], info["attr"][num]['value']) for num in xrange(player.ATTRIBUTE_SLOT_MAX_NUM)]
		
		if app.ATTR_LOCK:
			self.itemTooltip.AddItemData(info["vnum"], sockets, attrs, lockedattr = info.get('locked_attr', -1))
		
		else:
			self.itemTooltip.AddItemData(info["vnum"], sockets, attrs)
		
		try:
			if ENABLE_CHANGELOOK_SYSTEM:
				trans = info.get('trans',0)
				if trans !=0 and trans != -1:
					self.itemTooltip.AppendTransmutation(0,0, trans)
		except:
			pass



		if self.pageCategory in ("open_shop", "my_shop", "search_filter", "create_shop"):
			self.itemTooltip.AppendPrice(info["price"])
			if ENABLE_CHEQUE_SYSTEM:
				cheque = info.get('cheque',0)
				if cheque!=0:
					self.itemTooltip.AppendSellingChequePrice(cheque)
		
		if is_building and not info.get('sold', False):
			self.itemTooltip.AppendSpace(10)
			self.itemTooltip.AppendTextLine(localeinfo.OFFLINESHOP_BUILDING_LEFT_CLICK_EDIT_PRICE, infocolor)
			self.itemTooltip.AppendTextLine(localeinfo.OFFLINESHOP_BUILDING_LEFT_CLICK_CTRL_REMOVE_ITEM, infocolor)
		
		elif self.pageCategory == "open_shop" or self.pageCategory == "search_filter":
			self.itemTooltip.AppendSpace(10)
			self.itemTooltip.AppendTextLine(localeinfo.OFFLINESHOP_OPEN_SHOP_LEFT_CLICK_BUY_ITEM, infocolor)
			#self.itemTooltip.AppendTextLine(localeinfo.OFFLINESHOP_OPEN_SHOP_LEFT_CLICK_MAKE_OFFER, infocolor)

		elif self.pageCategory == "shop_safebox":
			self.itemTooltip.AppendSpace(10)
			self.itemTooltip.AppendTextLine(localeinfo.OFFLINESHOP_SAFEBOX_TOOLTIP_GETITEM, infocolor)
		elif canRemove:
			self.itemTooltip.AppendTextLine(localeinfo.OFFLINESHOP_BUILDING_LEFT_CLICK_CTRL_REMOVE_ITEM, infocolor)

	def __SetTooltipMyOffer(self, offer):
		slot = offer.slot
		self.__SetTooltip(slot.GetInfo())
		index = offer.info['offer_id']

		for elm in self.MyOffersList:
			if elm['offer_id'] == index:
				self.itemTooltip.AppendSpace(10)
				self.itemTooltip.AppendPrice(elm['item']['price'])
				self.itemTooltip.AppendTextLine(localeinfo.OFFLINESHOP_MY_OFFER_OFFER_TEXT%localeinfo.NumberToMoneyString(elm['price']))
				break


	def __SetTooltipSearch(self, element):
		appendLine = lambda arg : (
			uitooltip.ToolTip.AutoAppendTextLine(self.itemTooltip, arg),
			self.itemTooltip.AlignHorizonalCenter())

		self.itemTooltip.ClearToolTip()


		info = element.info

		name 		= info.get('filter_name', "")
		wearflag	= info.get('filter_wearflag', 0)

		price_min	= info.get('price', {}).get('start', 0)
		price_max	= info.get('price', {}).get('end', 0)

		level_min	= info.get('level', {}).get('start',0)
		level_max	= info.get('level', {}).get('end',0)

		type		= info.get('filter_type', 0)
		subtype		= info.get('filter_subtype', 255)

		attrs		= info.get('attr')
		rarity 		= info.get('rarity', -1)

		#name
		if name:
			appendLine(localeinfo.OFFLINESHOP_TOOLTIP_NAME_INFO +" "+ name)

		#type
		if type:
			type_phrase  = localeinfo.OFFLINESHOP_TOOLTIP_TYPE_INFO+  " "
			type_phrase += self.ITEM_TYPES[type]['name']

			if subtype != SUBTYPE_NOSET and self.ITEM_TYPES[type].has_key('subtypes'):
				st_name = self.ITEM_TYPES[type]['subtypes'].get(subtype, None)
				if st_name:
					type_phrase += ", "+st_name

			appendLine(type_phrase)


		#price
		if price_min or price_max:
			price_phrase = localeinfo.OFFLINESHOP_TOOLTIP_PRICE_INFO + " "
			if price_min and price_max:
				price_phrase += NumberToString(price_min) + "  -  "+ localeinfo.NumberToMoneyString(price_max)

			elif price_min:
				price_phrase += " > "+ localeinfo.NumberToMoneyString(price_min)
			elif price_max:
				price_phrase += " < "+ localeinfo.NumberToMoneyString(price_max)

			appendLine(price_phrase)

		#level
		if level_min or level_max:
			level_phrase = localeinfo.OFFLINESHOP_TOOLTIP_LEVEL_INFO + " "
			if level_min and level_max:
				level_phrase += "%d  -  %d"%(level_min, level_max)

			elif level_min:
				level_phrase += " >= %d"%level_min

			elif level_max:
				level_phrase += " <= %d"%level_max

			appendLine(level_phrase)


		#wear
		raceFlagDct = {
			item.ITEM_ANTIFLAG_WARRIOR : localeinfo.OFFLINESHOP_WEAR_WARRIOR,
			item.ITEM_ANTIFLAG_ASSASSIN	: localeinfo.OFFLINESHOP_WEAR_ASSASSIN,
			item.ITEM_ANTIFLAG_SURA		: localeinfo.OFFLINESHOP_WEAR_SURA,
			item.ITEM_ANTIFLAG_SHAMAN	: localeinfo.OFFLINESHOP_WEAR_SHAMAN,
		}

		if ENABLE_WOLFMAN_CHARACTER:
			raceFlagDct.update({
				item.ITEM_ANTIFLAG_WOLFMAN: localeinfo.OFFLINESHOP_WEAR_WOLFMAN,
			})

		wearphrase = ""
		for k ,v in raceFlagDct.items():
			if not wearflag & k:
				if wearphrase:
					wearphrase += ", "
				wearphrase += v

		if wearphrase:
			wearphrase = localeinfo.OFFLINESHOP_TOOLTIP_WEAR_INFO + wearphrase
			appendLine(wearphrase)

		if rarity != -1 and rarity in self.RARITY_GRADES:
			appendLine(localeinfo.OFFLINESHOP_TOOLTIP_RARITY_INFO + self.RARITY_GRADES[rarity] )
		
		#attribute
		attribute_phrase = ""
		for type in attrs['type']:
			if type != 0:
				if attribute_phrase:
					attribute_phrase += ', '
				attribute_phrase += self.ATTRIBUTES[type]

		if attribute_phrase:
			attribute_phrase = localeinfo.OFFLINESHOP_TOOLTIP_ATTR_INFO +' '+attribute_phrase
			appendLine(attribute_phrase)


		self.itemTooltip.ShowToolTip()



	def __SetTooltipAuctionList(self, element):
		itemInfo = element.GetInfo()['item']
		self.__SetTooltip(itemInfo)



	def OnUpdate(self):
		if self.pageCategory in self.updateEvents:
			self.updateEvents[self.pageCategory]()

	def __OnUpdateMyShopPage(self):
		self.__RefreshingTooltip(self.MyShopItemsTable)

	
	
	def __OnUpdateCreateShopPage(self):
		self.__RefreshingTooltip(self.CreateShopItemsTable)

	def __OnUpdateOpenShopPage(self):
		self.__RefreshingTooltip(self.OpenShopItemsTable)
	
	def __OnUpdateSearchFilterPage(self):
		self.__RefreshingTooltip(self.SearchFilterResultItemsTable)
		self.__UpdateGradeFilterEffect()

		# important multi-search pacing: send next vnum query when cooldown is over
		try:
			if self._importantMultiActive and self._importantMultiWaitUntil and app.GetTime() >= self._importantMultiWaitUntil:
				self._importantMultiWaitUntil = 0.0
				self.__SendNextImportantVnumQuery()
		except:
			pass
		
	def __OnUpdateSearchHistoryPage(self):
		self.__RefreshingTooltip(self.SearchHistoryTable , self.__SetTooltipSearch)

	def __OnUpdateMyPatternPage(self):
		self.__RefreshingTooltip(self.SearchPatternsTable, self.__SetTooltipSearch)

	def __OnUpdateShopSafeboxPage(self):
		self.__RefreshingTooltip(self.ShopSafeboxItemsTable)
	

	def __OnUpdateMyOffersPage(self):
		self.__RefreshingTooltip(self.MyOffersTable, self.__SetTooltipMyOffer)

	def __OnUpdateCreateAuctionPage(self):
		if self.CreateAuctionSlot.IsShow():
			if self.itemTooltip.IsShow():
				if not self.CreateAuctionSlot.IsInSlot():
					self.itemTooltip.Hide()
			else:
				if self.CreateAuctionSlot.IsInSlot() and self.CreateAuctionSlot.GetInfo():
					self.__SetTooltip(self.CreateAuctionSlot.GetInfo(), True)
		
		if self.CreateAuctionSlot.GetInfo():
			last_check = getattr(self, "LastCreateAuctionCheck", 0)
			if last_check == 0 or app.GetTime() - 0.5 > last_check:
				self.LastCreateAuctionCheck = app.GetTime()
				self.__OnCheckCreateAuctionSlot()

	def __OnUpdateMyAuctionPage(self):
		if self.MyAuctionSlot.IsShow():
			if self.itemTooltip.IsShow():
				if not self.MyAuctionSlot.IsInSlot():
					self.itemTooltip.Hide()
			else:
				if self.MyAuctionSlot.IsInSlot() and self.MyAuctionSlot.GetInfo():
					self.__SetTooltip(self.MyAuctionSlot.GetInfo())


	def __OnUpdateOpenAuctionPage(self):
		if self.OpenAuctionSlot.IsShow():
			if self.itemTooltip.IsShow():
				if not self.OpenAuctionSlot.IsInSlot():
					self.itemTooltip.Hide()
			else:
				if self.OpenAuctionSlot.IsInSlot() and self.OpenAuctionSlot.GetInfo():
					self.__SetTooltip(self.OpenAuctionSlot.GetInfo())
					self.itemTooltip.AppendTextLine(localeinfo.OFFLINESHOP_TOOLTIP_LEFT_CLICK_MAKE_OFFER)


	def __OnUpdateAuctionListPage(self):
		self.__RefreshingTooltip(self.AuctionListTable, self.__SetTooltipAuctionList)

	def __OnCheckCreateAuctionSlot(self):
		slot = self.CreateAuctionSlotPos
		win  = self.CreateAuctionWindowPos
		info = self.CreateAuctionSlot.GetInfo()
		
		vnum  = info.get('vnum', 0)
		count = info.get('count', 0)

		slot_vnum  = player.GetItemIndex(win,slot)
		slot_count = player.GetItemCount(win,slot)

		if vnum != slot_vnum or count != slot_count:
			self.__ResetCreateAuctionSlot()

	def __ResetCreateAuctionSlot(self):
		self.__MakeCreateAuctionSlot()
		self.CreateAuctionWindowPos = -1
		self.CreateAuctionSlotPos = -1

	def __RefreshingTooltip(self, table, event = None):
		if not self.itemTooltip.IsShow():
			elementDict = table.GetElementDict()
			for dct in elementDict.values():
				for value in dct.values():
					if value.IsInSlot():
						self.slotTooltipIndex = value.GetIndex()
						if event == None:
							self.__SetTooltip(value.GetInfo())
						else:
							event(value)

						return

			self.itemTooltip.Hide()
			self.slotTooltipIndex = -1

		else:
			elementDict = table.GetElementDict()
			for dct in elementDict.values():
				for value in dct.values():
					if value.IsInSlot():
						if self.slotTooltipIndex == value.GetIndex():
							return

						self.slotTooltipIndex = value.GetIndex()
						if event == None:
							self.__SetTooltip(value.GetInfo())
						else:
							event(value)

						return

			self.itemTooltip.Hide()
			self.slotTooltipIndex = -1


	def RefreshMyShopPage(self):
		
		#page
		self.pageCategory = "my_shop"
		for board in self.pageBoards.values():
			board.Hide()
		
		self.pageBoards["my_shop"].Show()
		name = self.ShopOpenInfo["name"]


		# title&duration
		if '@' in name:
			name = name[name.find('@')+1:]

		self.MyShopShopTitle.SetText(name + "  " + localeinfo.OFFLINESHOP_ITEMS_COUNT_TEXT%self.ShopOpenInfo["count"])
		self.MyShopShopDuration.SetText(GetShopDurationString(self.ShopOpenInfo["duration"]))



		#items table
		self.MyShopItemsTable.ClearElement()
		
		for item_info in self.ShopItemForSale:
			
			slot = Slot()
			slot.SetInfo(item_info)
			slot.SetIndex(item_info["id"])
			slot.SetOnMouseLeftButtonUpEvent(self.__OnLeftClickInsertedItem)
			slot.Show()
			
			self.MyShopItemsTable.AddElement(slot)

		for item_info in self.ShopItemSold:
			item_info["sold"] = True

			slot = Slot(isSold=True)
			slot.SetInfo(item_info)
			slot.SetIndex(item_info["id"])
			slot.Show()

			self.MyShopItemsTable.AddElement(slot)

		#offers
		self.MyShopOffersTable.ClearElement()
		for info in self.MyShopOffers:
			# find item name
			itemname = ""
			for saleitem in self.ShopItemForSale:
				if saleitem["id"] == info["item_id"]:
					item.SelectItem(saleitem["vnum"])
					itemname = item.GetItemName()
					break

			if not itemname:
				continue

			offer = Offer(info)
			offer.Show()
			offer.SetItemName(itemname)
			if not info["is_accept"]:
				offer.SetAcceptButtonEvent(self.__OnClickMyShopOfferAcceptButton)
				offer.SetDeleteButtonEvent(self.__OnClickMyShopOfferCancelButton)

			self.MyShopOffersTable.AddElement(offer)
	
	def RefreshShopListPage(self):
		self.pageCategory = "shop_list"
		for board in self.pageBoards.values():
			board.Hide()
		
		self.pageBoards["shop_list"].Show()
		self.ShopListTable.ClearElement()
		
		for shop in self.ShopList:
			is_viewed = shop["owner_id"] in self.ViewedShopOwners
			element = ShopListElement(shop, is_viewed)
			element.SetOnClickOpenShopButton(self.__OnClickShopListOpenShop)
			element.Show()

			self.ShopListTable.AddElement(element)
		
		if self.itemTooltip.IsShow():
			self.itemTooltip.Hide()
	
	
	def RefreshOpenShopPage(self):
		self.pageCategory = "open_shop"
		for board in self.pageBoards.values():
			board.Hide()
		
		self.pageBoards["open_shop"].Show()
		
		self.OpenShopItemsTable.ClearElement()
		for info in self.ShopItemForSale:
			slot = Slot()
			slot.SetInfo(info)
			slot.SetIndex(info["id"])
			slot.SetOnMouseLeftButtonUpEvent(self.__OnLeftClickShopItem)
			slot.Show()
			
			self.OpenShopItemsTable.AddElement(slot)
		
		name 		= self.ShopOpenInfo["name"]
		duration	= self.ShopOpenInfo["duration"]
		
		if '@' in name:
			name = name[name.find('@')+1:]
		
		self.OpenShopShopTitle.SetText(name+ "  " + localeinfo.OFFLINESHOP_ITEMS_COUNT_TEXT%self.ShopOpenInfo["count"])
		self.OpenShopShopDuration.SetText(GetShopDurationString(duration))


	def RefreshSearchHistoryPage(self):
		self.SearchHistoryTable.ClearElement()
		offlineshop.RefreshFilterHistory()

		SortByDatetime(self.FilterHistory)

		for elm in self.FilterHistory:
			newElement = FilterHistoryElement(elm)
			newElement.SetButtonEvent(self.__OnClickFilterHistoryElement)
			newElement.Show()
			self.SearchHistoryTable.AddElement(newElement)


	def RefreshMyPatternsPage(self):
		self.SearchPatternsTable.ClearElement()
		offlineshop.RefreshFilterPatterns()

		lst = self.FilterPatterns.values()
		SortByDatetime(lst)

		for v in lst:
			pattern = FilterPatternElement(v)
			pattern.SetButtonEvent(self.__OnClickSearchPatternElement)
			pattern.Show()

			self.SearchPatternsTable.AddElement(pattern)


	def RefreshMyOffersPage(self):
		self.pageCategory = "my_offers"
		for board in self.pageBoards.values():
			board.Hide()

		self.pageBoards["my_offers"].Show()



		self.MyOffersTable.ClearElement()

		for info in self.MyOffersList:

			offer = MyOffer(info)
			offer.Show()
			offer.SetCancelButtonEvent(self.__OnClickDeleteMyOfferButton)
			slot  = offer.slot


			if not info['is_accept']:
				viewImage = MakeOfferViewImage(info['is_notified'])
				slot.AppendChild(viewImage)
				viewImage.SetPosition(5,5)
				viewImage.Show()
			
			#updated 25-01-2020 #topatch
			else:
				offer.DisableCancelButton()

			self.MyOffersTable.AddElement(offer)


	def RefreshAuctionListPage(self):
		self.pageCategory = "auction_list"
		for board in self.pageBoards.values():
			board.Hide()

		self.pageBoards["auction_list"].Show()




		self.AuctionListTable.ClearElement()

		for info in self.AuctionListInfo:
			elm = AuctionListElement(info)
			elm.Show()
			elm.SetIndex(info['owner_id'])
			elm.SetOnClickOpenAuctionButton(self.__OnClickOpenAuctionButton)

			self.AuctionListTable.AddElement(elm)



	def RefreshMyAuctionPage(self):
		self.pageCategory = "my_auction"
		for board in self.pageBoards.values():
			board.Hide()

		self.pageBoards["my_auction"].Show()



		self.MyAuctionOfferTable.ClearElement()

		if 'offers' in self.MyAuctionInfo:
			for info in SortOffersByPrice(self.MyAuctionInfo['offers']):
				elm = AuctionOffer(info)
				elm.Show()

				self.MyAuctionOfferTable.AddElement(elm)

		best_price = GetBestOfferPriceYang(self.MyAuctionInfo.get('offers', []))
		if best_price ==0:
			min_raise  = self.MyAuctionInfo['init_yang']

		else:
			if best_price < 1000:
				min_raise = best_price + 1000
			else:
				min_raise  = long(float(best_price)*1.1)






		self.MyAuctionBestOffer.SetText(localeinfo.NumberToMoneyString(best_price))
		self.MyAuctionDuration.SetText(GetDurationString(self.MyAuctionInfo['duration']))
		self.MyAuctionMinRaise.SetText(localeinfo.NumberToMoneyString(min_raise))
		self.MyAuctionOwnerName.SetText(self.MyAuctionInfo['owner_name'])

		if 'item' in self.MyAuctionInfo:
			self.__MakeMyAuctionSlot(self.MyAuctionInfo['item'])


	def RefreshOpenAuctionPage(self):
		self.pageCategory = "open_auction"
		for board in self.pageBoards.values():
			board.Hide()

		self.pageBoards["open_auction"].Show()



		self.OpenAuctionOfferTable.ClearElement()

		if 'offers' in self.OpenAuctionInfo:
			for info in SortOffersByPrice(self.OpenAuctionInfo['offers']):
				elm = AuctionOffer(info)
				elm.Show()

				self.OpenAuctionOfferTable.AddElement(elm)

		best_price = GetBestOfferPriceYang(self.OpenAuctionInfo.get('offers' , []))
		if best_price == 0:
			min_raise = self.OpenAuctionInfo['init_yang']

		else:
			if best_price < 1000:
				min_raise = best_price + 1000
			else:
				min_raise = long(float(best_price) * 1.1)

		self.OpenAuctionInfo['min_raise'] = min_raise

		self.OpenAuctionBestOffer.SetText(localeinfo.NumberToMoneyString(best_price))
		self.OpenAuctionDuration.SetText(GetDurationString(self.OpenAuctionInfo['duration']))
		self.OpenAuctionMinRaise.SetText(localeinfo.NumberToMoneyString(min_raise))
		self.OpenAuctionOwnerName.SetText(self.OpenAuctionInfo['owner_name'])

		if 'item' in self.OpenAuctionInfo:
			self.__MakeOpenAuctionSlot(self.OpenAuctionInfo['item'])
