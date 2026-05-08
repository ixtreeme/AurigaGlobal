import ui
import player
import mousemodule
import app
import net
import item
import grp
import constinfo
import wiki


class MountInventoryWindow(ui.ScriptWindow):
	"""
	Mount Inventory window (12x16 UI, belt-like layout).
	- Left: checklist + scrollbar (fully clickable like belt)
	- Right: slot grid
	- Expand/Minimize buttons
	"""

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

			# small "i" button -> open wiki origin
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

		# belt-like UX: clicking the row also opens info
		def OnMouseLeftButtonDown(self):
			self.__OnClickInfo()
			return False

	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.isLoaded = False

		self.tooltipItem = None
		self.board = None
		self.titleBar = None
		self.wndItem = None
		self.wndLayer = None
		self.expandBtn = None
		self.minBtn = None
		self.closeBtn = None
		self.mountListBox = None
		self.mountListScrollBar = None

		self.ORIGINAL_WIDTH = 0
		self.ORIGINAL_HEIGHT = 0

		# mount list refresh signatures
		self.__mountInvSig = None
		self.__mountCheckSig = None

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def LoadWindow(self, parent):
		if self.isLoaded:
			return
		self.isLoaded = True

		try:
			loader = ui.PythonScriptLoader()
			loader.LoadScriptFile(self, "uiscript/mountinventory.py")
		except Exception:
			import exception
			exception.Abort("MountInventoryWindow.LoadWindow.LoadObject")
			return

		try:
			self.ORIGINAL_WIDTH = self.GetWidth()
			self.ORIGINAL_HEIGHT = self.GetHeight()

			self.board = self.GetChild("MountInventoryBoard")
			self.wndLayer = self.GetChild("MountInventoryLayer")
			self.titleBar = self.GetChild("TitleBar")
			self.wndItem = self.GetChild("MountInventorySlot")

			self.expandBtn = self.GetChild("ExpandBtn")
			self.minBtn = self.GetChild("MinimizeBtn")
			self.closeBtn = self.GetChild("MountInventoryCloseButton")

			self.mountListBox = self.GetChild("MountInventoryList")
			self.mountListScrollBar = self.GetChild("MountInventoryListScroll")
		except Exception:
			import exception
			exception.Abort("MountInventoryWindow.LoadWindow.BindObject")
			return

		# --- IMPORTANT: make list fully clickable like belt inventory ---
		# This is the same "forced" list sizing logic you have in uiinventory.py (belt).
		try:
			FORCE_STEP = 16
			# slot grid height = 33 * 16 = 528 (SLOT_BLOCK * SLOT_Y_COUNT) in belt-style scripts
			FORCE_VIEW = 33
			FORCE_H = FORCE_STEP * FORCE_VIEW

			self.mountListBox.SetSize(self.mountListBox.GetWidth(), FORCE_H)

			if hasattr(self.mountListBox, "SetItemSize"):
				self.mountListBox.SetItemSize(self.mountListBox.GetWidth(), FORCE_STEP)
			if hasattr(self.mountListBox, "SetItemStep"):
				self.mountListBox.SetItemStep(FORCE_STEP)

			if hasattr(self.mountListBox, "SetViewItemCount"):
				self.mountListBox.SetViewItemCount(FORCE_VIEW)
			elif hasattr(self.mountListBox, "SetViewCount"):
				self.mountListBox.SetViewCount(FORCE_VIEW)

			self.mountListScrollBar.SetSize(FORCE_H)
		except:
			pass

		# close/expand/minimize
		self.titleBar.SetCloseEvent(ui.__mem_func__(self.Close))
		self.closeBtn.SetEvent(ui.__mem_func__(self.Close))
		self.expandBtn.SetEvent(ui.__mem_func__(self.OpenInventory))
		self.minBtn.SetEvent(ui.__mem_func__(self.CloseInventory))

		# slot events
		self.wndItem.SetSelectEmptySlotEvent(ui.__mem_func__(self.SelectEmptySlot))
		self.wndItem.SetSelectItemSlotEvent(ui.__mem_func__(self.SelectItemSlot))
		self.wndItem.SetUseSlotEvent(ui.__mem_func__(self.UseItemSlot))
		self.wndItem.SetUnselectItemSlotEvent(ui.__mem_func__(self.UseItemSlot))
		self.wndItem.SetOverInItemEvent(ui.__mem_func__(self.OverInItem))
		self.wndItem.SetOverOutItemEvent(ui.__mem_func__(self.OverOutItem))
		self.wndItem.SetOverInEmptySlotEvent(ui.__mem_func__(self.OverOutItem))
		self.wndItem.SetOverOutEmptySlotEvent(ui.__mem_func__(self.OverOutItem))

		# list + scrollbar
		self.__BuildMountList()

		self.SetParent(parent)
		self.CloseInventory()  # start minimized
		self.Refresh()

	def Destroy(self):
		self.ClearDictionary()
		self.tooltipItem = None
		self.board = None
		self.titleBar = None
		self.wndItem = None
		self.wndLayer = None
		self.expandBtn = None
		self.minBtn = None
		self.closeBtn = None
		self.mountListBox = None
		self.mountListScrollBar = None
		self.__mountInvSig = None
		self.__mountCheckSig = None

	def SetItemToolTip(self, tooltip):
		self.tooltipItem = tooltip

	def ShowWindow(self):
		self.Show()
		self.SetTop()
		self.SetCenterPosition()
		self.OpenInventory()
		self.Refresh()

	def Close(self):
		self.Hide()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	# --- Expand/Minimize ---
	def OpenInventory(self):
		if self.wndLayer:
			self.wndLayer.Show()
		if self.expandBtn:
			self.expandBtn.Hide()
		self.SetSize(self.ORIGINAL_WIDTH, self.ORIGINAL_HEIGHT)

	def CloseInventory(self):
		if self.wndLayer:
			self.wndLayer.Hide()
		if self.expandBtn:
			self.expandBtn.Show()
		self.SetSize(10, self.ORIGINAL_HEIGHT)

	# --- Wiki helper (same as belt) ---
	def __OpenWikiOriginForVnum(self, vnum):
		# Mount list 'i' gomb: wiki oldal megnyitása (render target + origin info)
		try:
			base = wiki.GetBaseClass() if hasattr(wiki, "GetBaseClass") else None
			if not base:
				if hasattr(net, "ToggleWikiWindow"):
					net.ToggleWikiWindow()
				base = wiki.GetBaseClass() if hasattr(wiki, "GetBaseClass") else None
				if not base:
					return

			# Biztosan legyen megjelenítve a wiki
			if hasattr(base, "IsShow") and hasattr(base, "Show"):
				if not base.IsShow():
					base.Show()

			# Előző oldal (back) – ha van history
			oldWindow = None
			try:
				if hasattr(base, "windowHistory") and base.windowHistory:
					oldWindow = base.windowHistory[-1]
			except:
				oldWindow = None

			# Refine normalizálás csak fegyver/páncél esetén
			try:
				item.SelectItem(vnum)
				if item.GetItemType() in (item.ITEM_TYPE_WEAPON, item.ITEM_TYPE_ARMOR):
					startRefineVnum = wiki.GetWikiItemStartRefineVnum(vnum)
					if startRefineVnum != 0:
						vnum = startRefineVnum
					else:
						vnum = (int(vnum / 10) * 10)
			except:
				pass

			# Alap oldalak elrejtése + SpecialPage nyitás.
			# Ha bármi hiba van, állítsuk vissza a landing page-et, hogy ne maradjon üresen a wiki.
			try:
				if hasattr(base, "CloseBaseWindows"):
					base.CloseBaseWindows()
				base.OpenSpecialPage(oldWindow, vnum, False)
			except:
				try:
					if hasattr(base, "GoToLanding"):
						base.GoToLanding()
				except:
					pass
				return

			if hasattr(base, "SetTop"):
				base.SetTop()
		except:
			return

	# --- Mount list ---
	def __BuildMountList(self):
		if not self.mountListBox or not self.mountListScrollBar:
			return
		self.mountListBox.SetScrollBar(self.mountListScrollBar)
		self.__RefreshMountList()

	def __GetEffectiveSlotCount(self):
		# prefer UI slot count (12x16 = 192), fall back to player constant
		try:
			uiCount = self.wndItem.GetSlotCount()
		except:
			uiCount = 0

		try:
			plCount = int(player.MOUNT_INVENTORY_SLOT_COUNT)
		except:
			plCount = 0

		return max(uiCount, plCount)

	def __RefreshMountList(self, force=False):
		if not self.mountListBox:
			return

		# checklist signature
		try:
			check_sig = tuple(vnum for _, vnum in constinfo.MOUNT_INVENTORY_CHECK_LIST)
			check_iter = constinfo.MOUNT_INVENTORY_CHECK_LIST
			is_pair_list = True
		except:
			check_sig = tuple(constinfo.MOUNT_INVENTORY_CHECK_LIST)
			check_iter = constinfo.MOUNT_INVENTORY_CHECK_LIST
			is_pair_list = False

		# inventory signature
		mount_vnums = []
		slot_count = self.__GetEffectiveSlotCount()
		for i in xrange(slot_count):
			mount_vnums.append(player.GetItemIndex(player.MOUNT_INVENTORY, i))

		inv_sig = tuple(mount_vnums)

		if not force and self.__mountInvSig == inv_sig and self.__mountCheckSig == check_sig:
			return

		self.__mountInvSig = inv_sig
		self.__mountCheckSig = check_sig

		self.mountListBox.RemoveAllItems()

		green = grp.GenerateColor(0.2, 0.8, 0.2, 1.0)
		red = grp.GenerateColor(0.9, 0.2, 0.2, 1.0)
		have_set = set(mount_vnums)

		if is_pair_list:
			for _notused, vnum in check_iter:
				color = green if (vnum in have_set) else red
				item.SelectItem(vnum)
				entryText = "%s" % (item.GetItemName())
				self.mountListBox.AppendItem(
					self.MountListItem(entryText, color, vnum, self.mountListBox.GetWidth(), ui.__mem_func__(self.__OpenWikiOriginForVnum))
				)
		else:
			for vnum in check_iter:
				color = green if (vnum in have_set) else red
				item.SelectItem(vnum)
				entryText = "%s" % (item.GetItemName())
				self.mountListBox.AppendItem(
					self.MountListItem(entryText, color, vnum, self.mountListBox.GetWidth(), ui.__mem_func__(self.__OpenWikiOriginForVnum))
				)

	# --- Client refresh entrypoint ---
	def BINARY_RefreshMountInventory(self):
		self.Refresh()

	# --- Drag & drop logic ---
	def __MarkBlockedSlot(self, slot):
		if not self.wndItem:
			return
		#self.wndItem.ActivateSlot(slot, 1.0, 0.1, 0.1, 0.7)
		self.wndItem.RefreshSlot()

	def __IsSlotVacantForMove(self, targetSlot, attachedSlotType, attachedSlotPos):
		if attachedSlotType == player.SLOT_TYPE_MOUNT_INVENTORY and attachedSlotPos == targetSlot:
			return True
		if player.GetItemIndex(player.MOUNT_INVENTORY, targetSlot):
			self.__MarkBlockedSlot(targetSlot)
			return False
		return True

	def __MoveAttachedItemToMountSlot(self, targetSlot):
		attachedSlotType = mousemodule.mouseController.GetAttachedType()
		attachedSlotPos = mousemodule.mouseController.GetAttachedSlotNumber()

		if not self.__IsSlotVacantForMove(targetSlot, attachedSlotType, attachedSlotPos):
			return False

		if attachedSlotType == player.SLOT_TYPE_INVENTORY:
			# Inventory -> MountInventory
			net.SendMountInventoryCheckinPacket(attachedSlotPos, targetSlot)

		elif attachedSlotType == player.SLOT_TYPE_MOUNT_INVENTORY:
			# MountInventory -> MountInventory
			if attachedSlotPos == targetSlot:
				mousemodule.mouseController.DeattachObject()
				return True

			if self.wndItem:
				self.wndItem.ClearSlot(attachedSlotPos)
				self.wndItem.RefreshSlot()

			net.SendMountInventoryItemMovePacket(attachedSlotPos, targetSlot)

		elif app.ENABLE_EXTRA_INVENTORY and attachedSlotType == player.SLOT_TYPE_EXTRA_INVENTORY:
			net.SendMountInventoryCheckinPacket(player.EXTRA_INVENTORY, attachedSlotPos, targetSlot)
		else:
			return False

		mousemodule.mouseController.DeattachObject()
		if self.tooltipItem:
			self.tooltipItem.HideToolTip()
		return True

	def SelectEmptySlot(self, slot):
		if mousemodule.mouseController.isAttached():
			self.__MoveAttachedItemToMountSlot(slot)

	def SelectItemSlot(self, slot):
		if mousemodule.mouseController.isAttached():
			self.__MoveAttachedItemToMountSlot(slot)
			return

		itemVnum = player.GetItemIndex(player.MOUNT_INVENTORY, slot)
		itemCount = player.GetItemCount(player.MOUNT_INVENTORY, slot)
		if itemVnum:
			mousemodule.mouseController.AttachObject(self, player.SLOT_TYPE_MOUNT_INVENTORY, slot, itemVnum, itemCount)
			if self.tooltipItem:
				self.tooltipItem.HideToolTip()

	def UseItemSlot(self, slot):
		if mousemodule.mouseController.isAttached():
			if self.__MoveAttachedItemToMountSlot(slot):
				return
			mousemodule.mouseController.DeattachObject()
			return

		itemVnum = player.GetItemIndex(player.MOUNT_INVENTORY, slot)
		itemCount = player.GetItemCount(player.MOUNT_INVENTORY, slot)
		if itemVnum:
			mousemodule.mouseController.AttachObject(self, player.SLOT_TYPE_MOUNT_INVENTORY, slot, itemVnum, itemCount)
			if self.tooltipItem:
				self.tooltipItem.HideToolTip()

	def OverInItem(self, slot):
		if self.tooltipItem:
			self.tooltipItem.SetInventoryItem(slot, player.MOUNT_INVENTORY)

	def OverOutItem(self):
		if self.tooltipItem:
			self.tooltipItem.HideToolTip()

	def OnTop(self):
		if self.tooltipItem:
			self.tooltipItem.SetTop()

	def Refresh(self):
		if not self.wndItem:
			return

		# IMPORTANT: use UI slot count (12x16=192), not a 150-limited constant
		slotCount = self.wndItem.GetSlotCount()

		for slot in xrange(slotCount):
			self.wndItem.ClearSlot(slot)

		for slot in xrange(slotCount):
			itemVnum = player.GetItemIndex(player.MOUNT_INVENTORY, slot)
			itemCount = player.GetItemCount(player.MOUNT_INVENTORY, slot)

			if itemCount <= 1:
				itemCount = 0

			if itemVnum:
				self.wndItem.SetItemSlot(slot, itemVnum, itemCount)

		self.wndItem.RefreshSlot()
		self.__RefreshMountList()
