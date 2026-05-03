import app
import net
import player
import item
import ui
import uitooltip
import mousemodule
import uicommon
import constinfo
import localeinfo
import grp

class Window(ui.ScriptWindow):
	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.__Initialize()
		self.isLoaded = False

	def __Initialize(self):
		self.dlgQuestion = None
		self.children = []
		self.dialogHeight = 0

	def __LoadScript(self):

		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "uiscript/petevownd.py")

		except:
			import exception
			exception.Abort("PetEvoWindow.__LoadScript.LoadObject")

		try:
			self.board = self.GetChild("Board")
			self.titleBar = self.GetChild("TitleBar")
			self.GetChild("AcceptButton").SetEvent(self.Accept)
			self.GetChild("CancelButton").SetEvent(self.CancelRefine)
		except:
			import exception
			exception.Abort("PetEvoWindow.__LoadScript.BindObject")
		
		toolTip = uitooltip.ItemToolTip()
		toolTip.SetParent(self)
		toolTip.SetFollow(False)
		toolTip.SetPosition(15, 38)
		toolTip.Hide()
		self.toolTip = toolTip

		self.slotList = []
		for i in xrange(3):
			slot = self.__MakeSlot()
			slot.SetParent(toolTip)
			slot.SetWindowVerticalAlignCenter()
			self.slotList.append(slot)
		
		itemImage = self.__MakeItemImage()
		itemImage.SetParent(toolTip)
		itemImage.SetWindowVerticalAlignCenter()
		itemImage.SetPosition(-35, 0)
		self.itemImage = itemImage
		self.titleBar.SetCloseEvent(ui.__mem_func__(self.CancelRefine))
		self.isLoaded = True

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def __MakeSlot(self):
		slot = ui.ImageBox()
		slot.LoadImage("d:/ymir work/ui/public/slot_base.sub")
		slot.Show()
		self.children.append(slot)
		return slot

	def __MakeItemImage(self):
		itemImage = ui.ImageBox()
		itemImage.Show()
		self.children.append(itemImage)
		return itemImage

	def __MakeThinBoard(self):
		thinBoard = ui.ThinBoard()
		thinBoard.SetParent(self)
		thinBoard.Show()
		self.children.append(thinBoard)
		return thinBoard

	def Destroy(self):
		self.ClearDictionary()
		self.dlgQuestion = None
		self.board = 0
		self.titleBar = 0
		self.toolTip = 0
		self.slotList = []
		self.children = []

	def Open(self):
		if False == self.isLoaded:
			self.__LoadScript()
		
		self.__Initialize()
		self.toolTip.ClearToolTip()
		
		self.dialogHeight = self.toolTip.GetHeight() + 46
		self.UpdateDialog()
		
		self.SetTop()
		self.Show()

	def Close(self):
		self.dlgQuestion = None
		self.Hide()

	def ClearMaterial(self):
		self.children = []

	def AppendMaterial(self, vnum, count, windowType):
		slot = self.__MakeSlot()
		slot.SetParent(self)
		slot.SetPosition(15, self.dialogHeight)
		
		itemImage = self.__MakeItemImage()
		itemImage.SetParent(slot)
		item.SelectItem(vnum)
		itemImage.LoadImage(item.GetIconImageFileName())
	
		thinBoard = self.__MakeThinBoard()
		thinBoard.SetPosition(50, self.dialogHeight)
		thinBoard.SetSize(191, 20)
		
		textLine = ui.TextLine()
		textLine.SetParent(thinBoard)
		textLine.SetFontName(localeinfo.UI_DEF_FONT)
		textLine.SetPackedFontColor(0xffdddddd)
		textLine.SetText("%s x %02d" % (item.GetItemName(), count))
		textLine.SetOutline()
		textLine.SetFeather(False)
		textLine.SetWindowVerticalAlignCenter()
		textLine.SetVerticalAlignCenter()
		textLine.SetPosition(15, 0)
		textLine.Show()
		
		textLine2 = ui.TextLine()
		textLine2.SetParent(thinBoard)
		textLine2.SetFontName(localeinfo.UI_DEF_FONT)
		countOwn = 0
		if app.ENABLE_EXTRA_INVENTORY and windowType == player.EXTRA_INVENTORY:
			countOwn = player.GetItemCountbyVnumExtraInventory(vnum)
		elif windowType == player.INVENTORY:
			countOwn = player.GetItemCountByVnum(vnum)
		
		if countOwn >= count:
			textLine2.SetPackedFontColor(0xFF42f557)
		else:
			textLine2.SetPackedFontColor(0xFFff0000)
		
		textLine2.SetText("(%d)" % (countOwn))
		textLine2.SetOutline()
		textLine2.SetFeather(False)
		x, y = textLine.GetTextSize()
		textLine2.SetPosition(15 + x + 4, 9)
		textLine2.Show()
		
		self.children.append(textLine2)
		self.children.append(textLine)
		self.dialogHeight += 34
		self.UpdateDialog()

	def UpdateDialog(self):
		newWidth = self.toolTip.GetWidth() + 262
		newHeight = self.dialogHeight + 47
		self.board.SetSize(newWidth, newHeight)
		self.toolTip.SetPosition(15 + 35, 38)
		self.titleBar.SetWidth(newWidth-15)
		self.SetSize(newWidth, newHeight)

		(x, y) = self.GetLocalPosition()
		self.SetPosition(x, y)

	def Accept(self):
		net.SendChatPacket("/petvoincrease")
		self.Close()

	def CancelRefine(self):
		self.Close()

	def OnPressEscapeKey(self):
		self.CancelRefine()
		return True
