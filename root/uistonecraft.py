import app
import ui
import item
import player
import net
import grp

STONE_CRAFT_REWARD_VNUM = 39066
STONE_CRAFT_NEED_COUNT = 5

STONE_CRAFT_VNUMS = (
	range(28030, 28044) +
	[28100, 28104, 28108, 28112] +
	range(28130, 28144) +
	[28200, 28204, 28208, 28212] +
	range(28230, 28244) +
	[28300, 28304, 28308, 28312] +
	range(28330, 28344) +
	[28400, 28404, 28408, 28412] +
	range(28430, 28444)
)

class StoneCraftRow(ui.ListBoxEx.Item):
	def __init__(self, parentWnd, vnum):
		ui.ListBoxEx.Item.__init__(self)

		self.parentWnd = parentWnd
		self.vnum = vnum
		self.count = 0

		self.SetSize(360, 24)

		self.icon = ui.ImageBox()
		self.icon.SetParent(self)
		self.icon.SetPosition(2, 2)
		self.icon.Show()

		self.nameText = ui.TextLine()
		self.nameText.SetParent(self)
		self.nameText.SetPosition(30, 4)
		self.nameText.Show()

		self.countText = ui.TextLine()
		self.countText.SetParent(self)
		self.countText.SetPosition(230, 4)
		self.countText.Show()

		self.button = ui.MakeButton(
			self, 300, 0, "",
			"d:/ymir work/ui/public/",
			"small_button_01.sub",
			"small_button_02.sub",
			"small_button_03.sub"
		)
		self.button.SetText("Craft")
		self.button.SetEvent(lambda argVnum=self.vnum: self.parentWnd.OnCraft(argVnum))

		self.__LoadBaseData()
		self.RefreshCount(0)

	def __LoadBaseData(self):
		try:
			item.SelectItem(self.vnum)
			self.icon.LoadImage(item.GetIconImageFileName())
			self.nameText.SetText(item.GetItemName())
		except:
			self.nameText.SetText("VNUM %d" % self.vnum)

	def RefreshCount(self, count):
		self.count = count
		self.countText.SetText("%d / %d" % (count, STONE_CRAFT_NEED_COUNT))

		if count >= STONE_CRAFT_NEED_COUNT:
			self.countText.SetPackedFontColor(grp.GenerateColor(0.54, 0.72, 0.55, 1.0))
			self.button.Enable()
		else:
			self.countText.SetPackedFontColor(grp.GenerateColor(0.85, 0.35, 0.35, 1.0))
			self.button.Enable()

	def OnMouseOverIn(self):
		self.parentWnd.OverInItem(self.vnum)

	def OnMouseOverOut(self):
		self.parentWnd.OverOutItem()


class StoneCraftWindow(ui.ScriptWindow):
	def __init__(self):
		ui.ScriptWindow.__init__(self)

		self.tooltipItem = None
		self.refreshNextTime = 0.0
		self.rows = {}

		self.__LoadWindow()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def SetItemToolTip(self, tooltipItem):
		self.tooltipItem = tooltipItem

	def __LoadWindow(self):
		self.SetSize(410, 370)

		self.board = ui.BoardWithTitleBar()
		self.board.SetParent(self)
		self.board.SetPosition(0, 0)
		self.board.SetSize(410, 370)
		self.board.SetTitleName("Stone Craft")
		self.board.SetCloseEvent(ui.__mem_func__(self.Close))
		self.board.Show()

		self.rewardIcon = ui.ImageBox()
		self.rewardIcon.SetParent(self.board)
		self.rewardIcon.SetPosition(20, 40)
		self.rewardIcon.Show()

		self.rewardName = ui.TextLine()
		self.rewardName.SetParent(self.board)
		self.rewardName.SetPosition(65, 42)
		self.rewardName.Show()

		self.infoText = ui.TextLine()
		self.infoText.SetParent(self.board)
		self.infoText.SetPosition(65, 62)
		self.infoText.SetText("Any 5 stone = 1 Gaya")
		self.infoText.Show()

		self.infoText2 = ui.TextLine()
		self.infoText2.SetParent(self.board)
		self.infoText2.SetPosition(20, 88)
		self.infoText2.SetText("")
		self.infoText2.Show()
		self.craftAllButton = ui.MakeButton(
			self.board, 250, 82, "",
			"d:/ymir work/ui/public/",
			"large_button_01.sub",
			"large_button_02.sub",
			"large_button_03.sub"
		)
		self.craftAllButton.SetText("Craft All")
		self.craftAllButton.SetEvent(ui.__mem_func__(self.OnCraftAll))
		self.listBox = ui.ListBoxEx()
		self.listBox.SetParent(self.board)
		self.listBox.SetPosition(18, 115)
		self.listBox.SetItemSize(360, 24)
		self.listBox.SetItemStep(24)
		self.listBox.SetViewItemCount(10)
		self.listBox.Show()

		self.scrollBar = ui.SmallThinScrollBar()
		self.scrollBar.SetParent(self.board)
		self.scrollBar.SetPosition(385, 115)
		self.scrollBar.SetScrollBarSize(240)
		self.scrollBar.Show()

		self.listBox.SetScrollBar(self.scrollBar)

		try:
			item.SelectItem(STONE_CRAFT_REWARD_VNUM)
			self.rewardIcon.LoadImage(item.GetIconImageFileName())
			self.rewardName.SetText(item.GetItemName())
		except:
			self.rewardName.SetText("Reward VNUM %d" % STONE_CRAFT_REWARD_VNUM)

		for vnum in STONE_CRAFT_VNUMS:
			row = StoneCraftRow(self, vnum)
			self.listBox.AppendItem(row)
			self.rows[vnum] = row

	def Open(self):
		self.RefreshItems()
		self.SetCenterPosition()
		self.Show()
		self.SetTop()

	def Close(self):
		self.OverOutItem()
		self.Hide()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def OverInItem(self, vnum):
		if self.tooltipItem:
			self.tooltipItem.SetItemToolTip(vnum)

	def OverOutItem(self):
		if self.tooltipItem:
			self.tooltipItem.HideToolTip()

	def __CollectExtraInventoryCounts(self):
		counts = {}
		maxSlot = player.EXTRA_INVENTORY_PAGE_SIZE * player.EXTRA_INVENTORY_PAGE_COUNT

		for slot in xrange(maxSlot):
			vnum = player.GetItemIndex(player.EXTRA_INVENTORY, slot)
			if vnum == 0:
				continue

			count = player.GetItemCount(player.EXTRA_INVENTORY, slot)
			if count <= 0:
				continue

			counts[vnum] = counts.get(vnum, 0) + count

		return counts

	def RefreshItems(self):
		counts = self.__CollectExtraInventoryCounts()

		for vnum, row in self.rows.items():
			row.RefreshCount(counts.get(vnum, 0))

	def OnCraft(self, vnum):
		net.SendChatPacket("/stonecraft make %d" % vnum)
	def OnCraftAll(self):
		net.SendChatPacket("/stonecraft makeall")
	def OnUpdate(self):
		if not self.IsShow():
			return

		if app.GetTime() < self.refreshNextTime:
			return

		self.refreshNextTime = app.GetTime() + 0.25
		self.RefreshItems()