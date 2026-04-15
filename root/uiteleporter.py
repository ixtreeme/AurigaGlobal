import app
import net
import player
import item, event
import ui, uicommon, localeinfo, uitooltip

DIRECTORY = "d:/ymir work/ui/"
CLOSEBUTTON_POSITION = {"x" : 600, "y" : 60,}

def getIconImageByVnum(vnum):
	item.SelectItem(vnum)
	return item.GetIconImageFileName()

def getItemNamebyVnum(vnum):
	item.SelectItem(vnum)
	return item.GetItemName()

def haveMoney(money):
	return money <= player.GetMoney()
	
def haveItems(items):
	for item in items:
		found = False
		
		for i in xrange(player.INVENTORY_PAGE_SIZE * player.INVENTORY_PAGE_COUNT):
			if player.GetItemIndex(i) == item:
				found = True
				break
		
		if not found:
			return False
	
	return True

def AppendToolTipDesc(tooltip,desc):
	tokens = desc.split("<br>")
	for tok in tokens:
		tooltip.AppendTextLine(tok)

class TeleporterMap(ui.ScriptWindow):
	buttons = {}
	tooltips = {}
	lastButtonIn = None
	lastTooltip = None
	questionDlg = None
	popup = None
	selectedIdx = -1

	def __init__(self):
		#ui.ScriptWindow.__init__(self, "TOP_MOST")
		ui.ScriptWindow.__init__(self)
		self.board = None
		self.isLoaded = 0
		self.Load()

	def __del__(self):
		self.Destroy()
		ui.ScriptWindow.__del__(self)

	def Load(self):
		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "uiscript/teleport.py")
		except:
			import exception
			exception.Abort("TeleportWindow.LoadScript")
		
		try:
			self.board = self.GetChild("board")
		except:
			import exception
			exception.Abort("TeleportWindow.BindObject")
		
		self.buttons = {}
		mapCount = event.GetMapCount()
		filename = DIRECTORY + "%s.tga"
		upfile = filename % ("default")
		overfile = filename % ("over")
		downfile = filename % ("down")
		for i in xrange(mapCount):
			x, y = event.GetMapButtonPosition(i)
			button = ui.Button()
			button.SetParent(self.board)
			button.SetPosition(x,y)
			button.SAFE_SetEvent( self.__onClickTeleporterButton , i)
			button.SetWindowName("button_map_%d" % i)
			button.SetUpVisual(upfile)
			button.SetOverVisual(overfile)
			button.SetDownVisual(downfile)
			button.Show()
			self.buttons[i] = button
		
		self.tooltips = {}
		mapCount = event.GetMapCount()
		
		for i in xrange(mapCount):
			#title = event.GetMapTitle(i)
			money = event.GetMapPrice(i)
			items = event.GetMapItems(i)
			level = event.GetMapLevel(i)
			levelMax = event.GetMapLevelMax(i)
			
			tooltip = uitooltip.ToolTip()
			tooltip.SetFollow(True)
			#tooltip.SetTitle(title)
			tooltip.AppendSpace(5)
			
			if level == 0:
				level = 1
			
			tooltip.AppendTextLine(localeinfo.WINDOW_TXT1 % str(level))
			
			if levelMax == 0:
				levelMax = 120
			
			tooltip.AppendTextLine(localeinfo.WINDOW_TXT2 % str(levelMax))
			
			if money != 0:
				tooltip.AppendSpace(5)
				tooltip.AppendTextLine(localeinfo.NumberToMoneyString(money))
			
			if items:
				tooltip.AppendSpace(5)
				tooltip.AppendTextLine(localeinfo.WINDOW_TXT4)
				for item in items:
					tooltip.AppendTextLine(getItemNamebyVnum(item))
			
			tooltip.Hide()
			self.tooltips[i]=tooltip
		
		closeButton = ui.Button()
		closeButton.SetParent(self)
		closeButton.SetPosition(CLOSEBUTTON_POSITION["x"], CLOSEBUTTON_POSITION["y"])
		closeButton.SetUpVisual(DIRECTORY + "close_a.tga")
		closeButton.SetDownVisual(DIRECTORY + "close_n.tga")
		closeButton.SetOverVisual(DIRECTORY + "close_h.tga")
		closeButton.SetEvent(ui.__mem_func__(self.__onClickClose))
		closeButton.Show()
		self.close = closeButton
		self.isLoaded = 1

	def Destroy(self):
		self.ClearDictionary()
		self.board = None
		self.isLoaded = 0

	def Open(self):
		if self.IsShow():
			self.Close()
		else:
			if self.isLoaded != 1:
				return
			
			self.SetCenterPosition()
			self.Show()
			self.SetTop()

	def Close(self):
		if self.lastTooltip:
			self.lastTooltip.Hide()
		
		if self.popup:
			self.popup.Close()
			self.popup = None
		
		if self.questionDlg:
			self.questionDlg.Close()
			self.questionDlg = None
		
		self.Hide()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def OnUpdate(self):
		if self.lastButtonIn and self.lastButtonIn.IsIn() and self.lastTooltip.IsShow():
			return
		
		if self.lastButtonIn and self.lastButtonIn.IsIn() and self.lastTooltip:
			self.lastTooltip.Show()
			return
		
		if self.lastTooltip:
			self.lastTooltip.Hide()
		
		for k, button in self.buttons.items():
			if button.IsIn():
				tooltip = self.tooltips[k]
				tooltip.ShowToolTip()
				self.lastButtonIn = button
				self.lastTooltip  = tooltip
				return

	def __onClickTeleporterButton(self, idx):
		self.__questionDialog(idx)

	def __onClickClose(self):
		self.Close()

	def __popupMessage(self, text):
		if not self.popup:
			self.popup = uicommon.PopupDialog()
			self.popup.SetAcceptEvent(self.popup.Hide)
		
		self.popup.SetText(text)
		self.popup.Open()

	def __questionDialog(self, idx):
		if not self.questionDlg:
			qdlg = uicommon.QuestionDialog()
			qdlg.SetAcceptEvent(self.__onAcceptTeleporter)
			qdlg.SetCancelEvent(self.__onCancelTeleporter)
			self.questionDlg = qdlg
		
		self.questionDlg.SetText(localeinfo.WINDOW_TXT5 % str(event.GetMapTitle(idx)))
		self.questionDlg.Open()
		self.selectedIdx = idx

	def __onAcceptTeleporter(self):
		idx		= self.selectedIdx
		money	= event.GetMapPrice(idx)
		items	= event.GetMapItems(idx)

		self.questionDlg.Hide()
		
		if not haveMoney(money):
			self.__popupMessage(localeinfo.WINDOW_TXT6)
			return
		
		if not haveItems(items):
			self.__popupMessage(localeinfo.WINDOW_TXT3)
			return
		
		net.SendMapTeleporterPacket(self.selectedIdx)
		self.Close()

	def __onCancelTeleporter(self):
		self.selectedIdx = -1
		self.questionDlg.Hide()

