import ui, constinfo
import wndMgr

class ListItem(ui.ListBoxEx.Item):
	def __init__(self, _name, _win, _loss, _draw, _trophies):
		ui.ListBoxEx.Item.__init__(self)
		self.SetSize(503, 22)
		bar = ui.Bar()
		bar.SetParent(self)
		bar.SetSize(503, 22)
		bar.SetPosition(0, 0)
		bar.SetColor(0x00000000)
		bar.Show()
		self.bar = bar
		
		itemImage = ui.ExpandedImageBox()
		itemImage.SetParent(self.bar)
		itemImage.LoadImage("d:/ymir work/ui/game/guild/guildrankinglist/tab_01.tga")
		itemImage.SetPosition(0, 0)
		itemImage.SetScale(0.945, 0.875)
		itemImage.SetTop()
		itemImage.Show()
		self.itemImage = itemImage
		
		text = ui.TextLine()
		text.SetParent(self.itemImage)
		text.SetPosition(10, 0)
		text.SetText(_name)
		text.Show()
		self.text = text
		
		win = ui.TextLine()
		win.SetParent(self.itemImage)
		win.SetPosition(190, 0)
		win.SetText(_win)
		win.Show()
		self.win = win
		
		draw = ui.TextLine()
		draw.SetParent(self.itemImage)
		draw.SetPosition(272, 0)
		draw.SetText(_draw)
		draw.Show()
		self.draw = draw
		
		loss = ui.TextLine()
		loss.SetParent(self.itemImage)
		loss.SetPosition(352, 0)
		loss.SetText(_loss)
		loss.Show()
		self.loss = loss
		trophies = ui.TextLine()
		trophies.SetParent(self.itemImage)
		trophies.SetPosition(431, 0)
		trophies.SetText(_trophies)
		trophies.Show()
		self.trophies = trophies

	def __del__(self):
		ui.ListBoxEx.Item.__del__(self)

	def SetSize(self, width, height):
		ui.ListBoxEx.Item.SetSize(self, width, height)

class GuildRankWindow(ui.ScriptWindow):
	MAX_LEAGUES = 3
	
	def __init__(self ):
		ui.ScriptWindow.__init__(self)
		self.isLoaded = 0
		self.board = None
		self.myButton = []
		self.scrollbar = None
		self.listbox = None

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def Load(self):
		if self.isLoaded == 1:
			return
		
		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "uiscript/rankingboardwindow.py")
		except:
			import exception
			exception.Abort("GuildRankWindow.Load.LoadScript")
		
		try:
			self.board = self.GetChild("Board")
			for i in xrange(self.MAX_LEAGUES):
				self.myButton.append(self.GetChild("league%d_btn" % i))
				self.myButton[i].SetEvent(ui.__mem_func__(self.Choose), i)
			self.scrollbar = self.GetChild("scrollbar")
		except:
			import exception
			exception.Abort("GuildRankWindow.Load.BindObject")
		
		self.board.SetCloseEvent(self.Close)
		self.isLoaded = 1

	def Destroy(self):
		self.myList = []
		del self.myButton
		self.myButton = None

	def Choose(self, idx):
		if idx >= self.MAX_LEAGUES:
			return
		
		for item in self.myButton:
			item.SetUp()
		
		self.myButton[idx].Down()
		if self.listbox:
			del self.listbox
			self.listbox = None
		
		self.listbox = ui.ListBoxEx()
		self.listbox.SetParent(self.board)
		self.listbox.SetPosition(10, 57)
		self.listbox.SetSize(503, 200)
		self.listbox.SetItemStep(22)
		self.listbox.SetViewItemCount(10)
		self.listbox.SetItemSize(503, 22)
		self.listbox.SetScrollBar(self.scrollbar)
		self.listbox.Show()
		
		total = 0
		listcount = len(constinfo.GuildsRankList[idx]["Name"])
		maxLen = len(constinfo.GuildsRankList[idx]["Name"])
		for i in xrange(maxLen):
				self.listbox.AppendItem(ListItem(constinfo.GuildsRankList[idx]["Name"][i], constinfo.GuildsRankList[idx]["Win"][i], constinfo.GuildsRankList[idx]["Loss"][i], constinfo.GuildsRankList[idx]["Draw"][i], constinfo.GuildsRankList[idx]["Trophies"][i]))
				total += 1
		
		if total > 10:
			self.SetSize(548, 316)
			self.board.SetSize(548, 316)
			size = float(10) / (float(total))
			self.scrollbar.SetMiddleBarSize(size)
			self.scrollbar.Show()
		else:
			self.SetSize(526, 316)
			self.board.SetSize(526, 316)
			self.scrollbar.Hide()
		self.scrollbar.SetPos(0)

	def OnRunMouseWheel(self, nLen):
		if self.isLoaded == 0:
			return False
		
		x, y = self.GetGlobalPosition()
		xMouse, yMouse = wndMgr.GetMousePosition()
		difX = xMouse - x
		difY = yMouse - y
		if difX >= 6 and difX <= 518 and difY >= 10 and difY <= 306:
			if nLen > 0:
				if self.scrollbar.IsShow():
					self.scrollbar.OnUp()
					return True
			else:
				if self.scrollbar.IsShow():
					self.scrollbar.OnDown()
					return True
		
		return False

	def Close(self):
		self.Hide()

	def Open(self):
		if self.isLoaded == 0:
			self.Load()
		
		self.Choose(2)
		self.Show()
		self.SetTop()
		self.SetCenterPosition()

	def OnPressEscapeKey(self):
		self.Close()
		return True

