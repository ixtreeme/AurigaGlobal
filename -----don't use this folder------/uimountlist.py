# uimountlist.py
import ui
import grp
import item
import constinfo
import wiki
import net

class MountListController(object):
	class MountListItem(ui.ListBoxEx.Item):
		def __init__(self, text, color, vnum, width, onClickInfo):
			ui.ListBoxEx.Item.__init__(self)
			self.vnum = vnum
			self.onClickInfo = onClickInfo
			self.SetSize(width, 16)

			self.textLine = ui.TextLine()
			self.textLine.SetParent(self)
			self.textLine.SetPosition(4, 0)
			self.textLine.SetText(text)
			self.textLine.SetPackedFontColor(color)
			self.textLine.Show()

			self.infoBtn = ui.Button()
			self.infoBtn.SetParent(self)
			self.infoBtn.SetUpVisual("d:/ymir work/ui/public/xsmall_button_01.sub")
			self.infoBtn.SetOverVisual("d:/ymir work/ui/public/xsmall_button_02.sub")
			self.infoBtn.SetDownVisual("d:/ymir work/ui/public/xsmall_button_03.sub")
			self.infoBtn.SetText("i")

			x = width - 40
			if x < 0:
				x = 0
			self.infoBtn.SetPosition(x, -1)
			self.infoBtn.SetEvent(ui.__mem_func__(self.__OnClickInfo))
			self.infoBtn.Show()

		def __OnClickInfo(self):
			if self.onClickInfo:
				self.onClickInfo(self.vnum)

	def __init__(self, listBox, scrollBar, getVnumsFunc, openWikiFunc=None):
		self.listBox = listBox
		self.scrollBar = scrollBar
		self.getVnumsFunc = getVnumsFunc
		self.__invSig = None
		self.__checkSig = None

		# ha nincs külső openWikiFunc, használjuk a uiinventory-s logikát (átvéve)
		self.openWikiFunc = openWikiFunc if openWikiFunc else self.__OpenWikiOriginForVnum

	def ForceLayout(self, gridHeight):
		"""Ugyanaz a 'kattintható teljes lista' fix, mint belt inventoryban."""
		if not self.listBox:
			return

		try:
			FORCE_STEP = 16
			FORCE_VIEW = int(gridHeight / FORCE_STEP)
			if FORCE_VIEW <= 0:
				FORCE_VIEW = 1
			FORCE_H = FORCE_STEP * FORCE_VIEW

			self.listBox.SetSize(self.listBox.GetWidth(), FORCE_H)

			if hasattr(self.listBox, "SetItemSize"):
				self.listBox.SetItemSize(self.listBox.GetWidth(), FORCE_STEP)
			if hasattr(self.listBox, "SetItemStep"):
				self.listBox.SetItemStep(FORCE_STEP)

			if hasattr(self.listBox, "SetViewItemCount"):
				self.listBox.SetViewItemCount(FORCE_VIEW)
			elif hasattr(self.listBox, "SetViewCount"):
				self.listBox.SetViewCount(FORCE_VIEW)
		except:
			pass

	def Build(self, force=True):
		if not self.listBox or not self.scrollBar:
			return
		self.listBox.SetScrollBar(self.scrollBar)
		self.Refresh(force=force)

	def Refresh(self, force=False):
		if not self.listBox:
			return

		# checklist signature (vnum-only)
		try:
			check_sig = tuple(vnum for _, vnum in constinfo.MOUNT_INVENTORY_CHECK_LIST)
			check_iter = constinfo.MOUNT_INVENTORY_CHECK_LIST
			is_pair_list = True
		except:
			check_sig = tuple(constinfo.MOUNT_INVENTORY_CHECK_LIST)
			check_iter = constinfo.MOUNT_INVENTORY_CHECK_LIST
			is_pair_list = False

		inv_vnums = []
		try:
			inv_vnums = list(self.getVnumsFunc())
		except:
			inv_vnums = []

		inv_sig = tuple(inv_vnums)

		if not force and self.__invSig == inv_sig and self.__checkSig == check_sig:
			return

		self.__invSig = inv_sig
		self.__checkSig = check_sig

		self.listBox.RemoveAllItems()

		green = grp.GenerateColor(0.2, 0.8, 0.2, 1.0)
		red = grp.GenerateColor(0.9, 0.2, 0.2, 1.0)

		have_set = set([v for v in inv_vnums if v])

		if is_pair_list:
			for _notused, vnum in check_iter:
				hasItem = (vnum in have_set)
				color = green if hasItem else red
				item.SelectItem(vnum)
				entryText = "%s" % (item.GetItemName())
				self.listBox.AppendItem(
					self.MountListItem(entryText, color, vnum, self.listBox.GetWidth(), ui.__mem_func__(self.openWikiFunc))
				)
		else:
			for vnum in check_iter:
				hasItem = (vnum in have_set)
				color = green if hasItem else red
				item.SelectItem(vnum)
				entryText = "%s" % (item.GetItemName())
				self.listBox.AppendItem(
					self.MountListItem(entryText, color, vnum, self.listBox.GetWidth(), ui.__mem_func__(self.openWikiFunc))
				)

	def __OpenWikiOriginForVnum(self, vnum):
		# uiinventory-ből átvett wiki-open logika (belt-független)
		try:
			base = wiki.GetBaseClass() if hasattr(wiki, "GetBaseClass") else None

			if not base:
				if hasattr(net, "ToggleWikiWindow"):
					net.ToggleWikiWindow()
				base = wiki.GetBaseClass() if hasattr(wiki, "GetBaseClass") else None
				if not base:
					return

			if hasattr(base, "IsShow") and hasattr(base, "Show"):
				if not base.IsShow():
					base.Show()

			oldWindow = None
			if hasattr(base, "CloseBaseWindows"):
				base.CloseBaseWindows()
			elif hasattr(base, "windowHistory") and base.windowHistory:
				oldWindow = base.windowHistory[-1]

			item.SelectItem(vnum)
			if item.GetItemType() in (item.ITEM_TYPE_WEAPON, item.ITEM_TYPE_ARMOR):
				startRefineVnum = wiki.GetWikiItemStartRefineVnum(vnum)
				if startRefineVnum != 0:
					vnum = startRefineVnum
				else:
					vnum = (int(vnum / 10) * 10)

			base.OpenSpecialPage(oldWindow, vnum, False)
			if hasattr(base, "SetTop"):
				base.SetTop()
		except:
			return
