import ui
import net
import player
import mouseModule
import uiToolTip
import uipickmoney
import localeinfo
import chat
import time
import item

try:
	import guild
except:
	guild = None


STORAGE_SLOT_X = 5
STORAGE_SLOT_Y = 3
STORAGE_SLOT_COUNT = STORAGE_SLOT_X * STORAGE_SLOT_Y


def _SafeMoneyStr(v):
	try:
		return localeinfo.NumberToMoneyString(long(v))
	except:
		try:
			return str(int(v))
		except:
			return "0"


def _ClampInt(v, lo, hi, defaultV):
	try:
		v = int(v)
	except:
		return defaultV
	if v < lo:
		return lo
	if v > hi:
		return hi
	return v


def _ItemNameByVnum(vnum):
	try:
		vnum = int(vnum)
	except:
		return "-"
	if vnum <= 0:
		return "-"
	try:
		item.SelectItem(vnum)
		n = item.GetItemName()
		return n if n else "-"
	except:
		return "-"


class GuildRenewalTaxDialog(ui.ScriptWindow):
	"""
	Ado kivetese dialog:
	- Hatarido: nap (pl 10)
	- Yang / fo
	- 5 sor: ikon + nev + DB / fo (CountBtn0..4 gombokkal)
	"""
	def __init__(self, parentWnd):
		ui.ScriptWindow.__init__(self)
		self.parentWnd = parentWnd
		self.isLoaded = False

		self.reqVnums = [0, 0, 0, 0, 0]
		self.countBtns = []
		self.nameTexts = []
		self.pickDlg = None
		self.pickIndex = -1

		self.__Load()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def __Load(self):
		if self.isLoaded:
			return

		pyLoader = ui.PythonScriptLoader()
		pyLoader.LoadScriptFile(self, "uiscript/guildrenewal_taxsetting.py")

		self.board = self.GetChild("Board")
		self.titleBar = self.GetChild("TitleBar")
		self.titleBar.SetCloseEvent(ui.__mem_func__(self.Close))

		self.daysEdit = self.GetChild("DaysValue")
		self.yangEdit = self.GetChild("YangValue")

		try:
			self.daysEdit.SetNumberMode()
			self.yangEdit.SetNumberMode()
		except:
			pass

		self.itemSlots = self.GetChild("ReqItemSlots")

		for i in xrange(5):
			self.countBtns.append(self.GetChild("CountSlot%d" % i))
			self.nameTexts.append(self.GetChild("NameText%d" % i))
			self.countBtns[i].SetEvent(ui.__mem_func__(self.__OnClickCountBtn), i)

		self.btnOK = self.GetChild("AcceptButton")
		self.btnCancel = self.GetChild("CancelButton")
		self.btnOK.SetEvent(ui.__mem_func__(self.__OnAccept))
		self.btnCancel.SetEvent(ui.__mem_func__(self.Close))

		self.isLoaded = True

	def Open(self, reqItems, defaultYangPerMember=0, defaultDays=7):
		# reqItems: [(vnum,count), ...] a kovetkezo szint igenybol
		self.reqVnums = [0, 0, 0, 0, 0]
		for i in xrange(5):
			try:
				vnum, _cnt = reqItems[i]
			except:
				vnum = 0
			self.reqVnums[i] = int(vnum)

		self.daysEdit.SetText(str(int(defaultDays)))
		try:
			self.yangEdit.SetText(str(long(defaultYangPerMember)))
		except:
			self.yangEdit.SetText("0")

		# ikon + nev, DB alapbol 0
		for i in xrange(5):
			v = self.reqVnums[i]
			if v > 0:
				self.itemSlots.SetItemSlot(i, v, 1)  # csak ikon
				self.nameTexts[i].SetText(_ItemNameByVnum(v))
				self.countBtns[i].SetText("0")
				self.countBtns[i].Enable()
			else:
				self.itemSlots.ClearSlot(i)
				self.nameTexts[i].SetText("-")
				self.countBtns[i].SetText("-")
				try:
					self.countBtns[i].Disable()
				except:
					pass

		self.itemSlots.RefreshSlot()

		self.SetCenterPosition()
		self.SetTop()
		self.Show()

	def Close(self):
		if self.pickDlg:
			self.pickDlg.Close()
			self.pickDlg = None
			self.pickIndex = -1
		self.Hide()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def __OnClickCountBtn(self, idx):
		try:
			idx = int(idx)
		except:
			return
		if idx < 0 or idx >= 5:
			return
		if int(self.reqVnums[idx]) <= 0:
			return

		if self.pickDlg:
			self.pickDlg.Close()
			self.pickDlg = None

		self.pickIndex = idx

		dlg = uipickmoney.PickMoneyDialog()
		dlg.LoadDialog()
		dlg.SetTitleName("DB / fo")
		dlg.SetAcceptEvent(ui.__mem_func__(self.__OnPickCountAcceptMoney))
		dlg.Open(999999)
		self.pickDlg = dlg

	def __OnPickCountAcceptMoney(self, money):
		if self.pickDlg:
			self.pickDlg.Close()
		self.pickDlg = None

		try:
			idx = int(self.pickIndex)
		except:
			idx = -1
		self.pickIndex = -1

		try:
			c = int(money)
		except:
			c = 0
		if c < 0:
			c = 0

		if 0 <= idx < 5:
			self.countBtns[idx].SetText(str(c))

	def __OnPickCount(self, idx):
		idx = int(idx)
		if idx < 0 or idx >= 5:
			return
		try:
			v = int(self.reqVnums[idx])
		except:
			v = 0
		if v <= 0:
			return

		# egy kis szam-bekeros ablak (PickMoneyDialog)
		try:
			if self.pickDlg:
				self.pickDlg.Close()
				self.pickDlg = None
		except:
			pass

		dlg = uipickmoney.PickMoneyDialog()
		dlg.LoadDialog()
		dlg.SetTitleName("Mennyiseg")
		dlg.SetAcceptEvent(ui.__mem_func__(self.__OnPickCountAccept))
		try:
			dlg.Open(9999999999)
		except:
			dlg.Open(999999999)

		self.pickDlg = dlg
		self.pickIndex = idx

	def __OnPickCountAccept(self, money=None):
		try:
			if money is None and self.pickDlg:
				money = self.pickDlg.GetMoney()
			val = int(money) if money is not None else 0
		except:
			val = 0
		if val < 0:
			val = 0

		idx = int(self.pickIndex)
		if 0 <= idx < 5:
			self.counts[idx] = val
			try:
				self.countBtns[idx].SetText(str(val))
			except:
				pass

		try:
			self.pickDlg.Close()
		except:
			pass
		self.pickDlg = None
		self.pickIndex = -1

	def __OnAccept(self):
		days = _ClampInt(self.daysEdit.GetText().strip(), 1, 30, 7)

		try:
			perYang = long(self.yangEdit.GetText().strip())
		except:
			perYang = 0
		if perYang < 0:
			perYang = 0

		epoch = time.time() + (days * 24 * 60 * 60)
		dateStr = time.strftime("%Y.%m.%d", time.localtime(epoch))

		cmd = "/gr_set_tax %s %d" % (dateStr, long(perYang))

		# 0 db eseten ne legyen benne: 0 0
		for i in xrange(5):
			v = int(self.reqVnums[i])
			try:
				c = int(self.countBtns[i].GetText())
			except:
				c = 0

			if c <= 0 or v <= 0:
				cmd += " 0 0"
			else:
				cmd += " %d %d" % (v, c)

		net.SendChatPacket(cmd)
		chat.AppendChat(chat.CHAT_TYPE_INFO, "Ado kovetelmeny elkuldve.")
		self.Close()




class GuildRenewalPayDialog(ui.ScriptWindow):
	"""
	Befizetes dialog (jatekosoknek):
	- Yang befizetes
	- 5 sor: ikon + nev + DB (CountValue0..4)
	Reused UI: uiscript/guildrenewal_taxsetting.py
	"""
	def __init__(self, parentWnd):
		ui.ScriptWindow.__init__(self)
		self.parentWnd = parentWnd
		self.isLoaded = False

		self.reqVnums = [0, 0, 0, 0, 0]
		self.countBtns = []
		self.counts = [0, 0, 0, 0, 0]
		self.pickDlg = None
		self.pickIndex = -1
		self.nameTexts = []

		self.__Load()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def __Load(self):
		if self.isLoaded:
			return

		pyLoader = ui.PythonScriptLoader()
		pyLoader.LoadScriptFile(self, "uiscript/guildrenewal_taxsetting.py")

		self.board = self.GetChild("Board")
		self.titleBar = self.GetChild("TitleBar")
		self.titleBar.SetCloseEvent(ui.__mem_func__(self.Close))

		# title
		try:
			self.GetChild("TitleName").SetText("Befizetes")
		except:
			pass

		# a script tartalmaz DaysValue-t is, itt nem kell
		self.daysEdit = self.GetChild("DaysValue")
		self.yangEdit = self.GetChild("YangValue")

		try:
			self.daysEdit.SetNumberMode()
			self.yangEdit.SetNumberMode()
		except:
			pass

		# rejtsuk el a napok mezot + labelt
		for nm in ("DaysLabel", "DaysSlot", "DaysValue"):
			try:
				self.GetChild(nm).Hide()
			except:
				pass
		try:
			self.daysEdit.SetText("0")
			self.daysEdit.Disable()
		except:
			pass

		# yang max karakter (10)
		try:
			self.yangEdit.SetMax(10)
		except:
			pass

		self.itemSlots = self.GetChild("ReqItemSlots")

		for i in xrange(5):
			self.nameTexts.append(self.GetChild("NameText%d" % i))
			btn = self.GetChild("CountSlot%d" % i)
			try:
				btn.SetText("0")
			except:
				pass
			btn.SetEvent(ui.__mem_func__(self.__OnPickCount), i)
			self.countBtns.append(btn)

		self.btnOK = self.GetChild("AcceptButton")
		self.btnCancel = self.GetChild("CancelButton")
		self.btnOK.SetEvent(ui.__mem_func__(self.__OnAccept))
		self.btnCancel.SetEvent(ui.__mem_func__(self.Close))

		self.isLoaded = True

	def Open(self, reqItems):
		self.reqVnums = [0, 0, 0, 0, 0]
		for i in xrange(5):
			try:
				vnum, _cnt = reqItems[i]
			except:
				vnum = 0
			self.reqVnums[i] = int(vnum)

		try:
			self.yangEdit.SetText("0")
		except:
			pass

		for i in xrange(5):
			v = int(self.reqVnums[i])
			if v > 0:
				self.itemSlots.SetItemSlot(i, v, 1)
				self.nameTexts[i].SetText(_ItemNameByVnum(v))
				self.counts[i] = 0
				try:
					self.countBtns[i].SetText("0")
				except:
					pass
				try:
					self.countBtns[i].Enable()
				except:
					pass
			else:
				self.itemSlots.ClearSlot(i)
				self.nameTexts[i].SetText("-")
				self.counts[i] = 0
				try:
					self.countBtns[i].SetText("0")
				except:
					pass
				try:
					self.countBtns[i].Disable()
				except:
					pass

		try:
			self.itemSlots.RefreshSlot()
		except:
			pass


		self.SetCenterPosition()
		self.SetTop()
		self.Show()

	def Close(self):
		self.Hide()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def __OnPickCount(self, idx):
		try:
			idx = int(idx)
		except:
			return
		if idx < 0 or idx >= 5 or int(self.reqVnums[idx]) <= 0:
			return
		try:
			if self.pickDlg:
				self.pickDlg.Close()
		except:
			pass
		dlg = uipickmoney.PickMoneyDialog()
		dlg.LoadDialog()
		dlg.SetTitleName("Mennyiseg")
		dlg.SetAcceptEvent(ui.__mem_func__(self.__OnPickCountAccept))
		try:
			dlg.Open(9999999999)
		except:
			dlg.Open(999999999)
		self.pickDlg = dlg
		self.pickIndex = idx

	def __OnPickCountAccept(self, money=None):
		try:
			if money is None and self.pickDlg:
				money = self.pickDlg.GetMoney()
			val = int(money) if money is not None else 0
		except:
			val = 0
		if val < 0:
			val = 0

		try:
			idx = int(self.pickIndex)
		except:
			idx = -1

		if 0 <= idx < 5:
			self.counts[idx] = val
			try:
				self.countBtns[idx].SetText(str(val))
			except:
				pass

		try:
			self.pickDlg.Close()
		except:
			pass
		self.pickDlg = None
		self.pickIndex = -1

	def __OnAccept(self):
		# yang
		try:
			y = long(self.yangEdit.GetText().strip())
		except:
			try:
				y = int(self.yangEdit.GetText().strip())
			except:
				y = 0
		if y < 0:
			y = 0

		# /gr_pay_tax <yang> <v1> <c1> ... <v5> <c5>
		cmd = "/gr_pay_tax %d" % int(y)
		for i in xrange(5):
			v = int(self.reqVnums[i])
			try:
				c = int(self.counts[i])
			except:
				c = 0
			if v <= 0 or c <= 0:
				cmd += " 0 0"
			else:
				cmd += " %d %d" % (v, c)

		net.SendChatPacket(cmd)
		self.Close()


class GuildContribRowItem(ui.ListBoxEx.Item):
	def __init__(self, width=570, idx=0):
		ui.ListBoxEx.Item.__init__(self)
		width = int(width)
		self.SetSize(width, 16)

		# column layout (relative to listbox item)
		self.COL_NAME_X = 6
		self.COL_YANG_X = 170
		self.COL_ITEM_X = 280
		self.ITEM_STEP = 56

		# row background
		try:
			self.bg = ui.Bar()
			self.bg.SetParent(self)
			self.bg.SetPosition(0, 0)
			self.bg.SetSize(width, 16)
			self.bg.SetColor(0x22000000 if (idx % 2) else 0x11000000)
			self.bg.Show()
		except:
			self.bg = None

		# bottom border
		try:
			self.bottom = ui.Bar()
			self.bottom.SetParent(self)
			self.bottom.SetPosition(0, 15)
			self.bottom.SetSize(width, 1)
			self.bottom.SetColor(0xFF2A2A2A)
			self.bottom.Show()
		except:
			pass

		# vertical separators
		self.vbars = []
		try:
			seps = [self.COL_YANG_X - 10, self.COL_ITEM_X - 10]
			for i in xrange(5):
				seps.append(self.COL_ITEM_X + i * self.ITEM_STEP - 10)
			for x in seps:
				if x <= 0 or x >= width:
					continue
				b = ui.Bar()
				b.SetParent(self)
				b.SetPosition(int(x), 0)
				b.SetSize(1, 16)
				b.SetColor(0xFF2A2A2A)
				b.Show()
				self.vbars.append(b)
		except:
			pass

		# texts
		self.nameText = ui.TextLine()
		self.nameText.SetParent(self)
		self.nameText.SetPosition(self.COL_NAME_X, 0)
		self.nameText.Show()

		self.yangText = ui.TextLine()
		self.yangText.SetParent(self)
		self.yangText.SetPosition(self.COL_YANG_X, 0)
		self.yangText.Show()

		self.itemTexts = []
		for i in xrange(5):
			t = ui.TextLine()
			t.SetParent(self)
			t.SetPosition(self.COL_ITEM_X + i * self.ITEM_STEP + 10, 0)
			t.Show()
			self.itemTexts.append(t)

	def SetData(self, name, paidFlag, paidMoney, itemCounts, mask=None):
		try:
			nm = str(name)
		except:
			nm = "?"
		if len(nm) > 22:
			nm = nm[:22] + "..."

		paid = 1 if paidFlag else 0

		self.nameText.SetText(nm)
		self.nameText.SetPackedFontColor(0xFF00FF00 if paid else 0xFFFF0000)

		# Yang column
		try:
			pm = long(paidMoney)
		except:
			try:
				pm = int(paidMoney)
			except:
				pm = 0
		self.yangText.SetText(_SafeMoneyStr(pm))
		self.yangText.SetPackedFontColor(0xFF00FF00 if pm > 0 else 0xFFFF0000)

		# Item columns
		if mask is None:
			mask = [1, 1, 1, 1, 1]

		for i in xrange(5):
			try:
				active = 1 if int(mask[i]) else 0
			except:
				active = 1
			if not active:
				self.itemTexts[i].SetText("-")
				self.itemTexts[i].SetPackedFontColor(0xFF808080)
				continue

			try:
				val = int(itemCounts[i])
			except:
				val = 0
			self.itemTexts[i].SetText(str(val))
			self.itemTexts[i].SetPackedFontColor(0xFF00FF00 if val > 0 else 0xFFFF0000)

class GuildRenewalWindow(ui.ScriptWindow):
	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.isLoaded = False

		# server state cache
		self.storageItems = {}   # slot -> (vnum,count)
		self.storageYang = 0

		self.reqItems = [(0, 0)] * 5   # (vnum, requiredCount)
		self.reqYang = 0

		self.taxActive = 0
		self.taxDeadlineRaw = 0
		self.taxYang = 0
		self.taxItems = [(0, 0)] * 5

		self.guildLevel = 0
		self.paidStatus = 0

		# pid -> (paidFlag, paidMoney, paidItemTotal)
		self.contribByPID = {}
		# pid -> (paidFlag, paidMoney, [cnt0..4]) (gr_contrib2)
		self.contrib2ByPID = {}
		# name -> paidFlag (kliens lista szinezesehez)
		self.memberPaidByName = {}

		self.moneyDialog = None
		self.taxDialog = None
		self.payDialog = None

		self.tooltipItem = uiToolTip.ItemToolTip()

		self.__LoadWindow()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def __LoadWindow(self):
		if self.isLoaded:
			return

		pyLoader = ui.PythonScriptLoader()
		pyLoader.LoadScriptFile(self, "uiscript/guildrenewalwindow.py")

		self.board = self.GetChild("Board")
		self.titleBar = self.GetChild("TitleBar")
		self.titleBar.SetCloseEvent(ui.__mem_func__(self.Close))

		# slots
		self.storageSlot = self.GetChild("StorageSlots")
		self.reqSlot = self.GetChild("ReqSlots")
		# kis ado rendszer: tax panel ki van veve az ablakbol
		try:
			self.taxSlot = self.GetChild("TaxSlots")
		except:
			self.taxSlot = None

		# texts
		self.txtLevel = self.GetChild("GuildLevelText")
		self.txtPaid = self.GetChild("PaidStatusText")

		self.txtStorageMoney = self.GetChild("StorageMoneyText")

		self.txtReqYang = self.GetChild("ReqYangText")
		self.txtReqYangTotal = self.GetChild("ReqYangTotalText")

		try:
			self.txtTaxDeadline = self.GetChild("TaxDeadlineText")
			self.txtTaxMoney = self.GetChild("TaxMoneyText")
		except:
			self.txtTaxDeadline = None
			self.txtTaxMoney = None

		# tax table names + counts (ha letezik)
		self.taxNameTexts = []
		self.taxCountTexts = []
		if self.txtTaxDeadline:
			for i in xrange(5):
				try:
					self.taxNameTexts.append(self.GetChild("TaxNameText%d" % i))
					self.taxCountTexts.append(self.GetChild("TaxCountText%d" % i))
				except:
					pass

		# buttons
		self.btnDepositYang = self.GetChild("DepositYangButton")
		self.btnPayTax = self.GetChild("PayTaxButton")
		try:
			self.btnSetTax = self.GetChild("SetTaxButton")
		except:
			self.btnSetTax = None
		try:
			self.btnPayTax.SetText("Befizetes")
		except:
			pass
		if self.btnSetTax:
			try:
				self.btnSetTax.Hide()
			except:
				pass
		self.btnLevelUp = self.GetChild("LevelUpButton")

		self.btnDepositYang.SetEvent(ui.__mem_func__(self.__OnDepositYangButton))
		self.btnPayTax.SetEvent(ui.__mem_func__(self.__OnPayTaxButton))
		if self.btnSetTax:
			self.btnSetTax.SetEvent(ui.__mem_func__(self.__OnSetTaxButton))
		self.btnLevelUp.SetEvent(ui.__mem_func__(self.__OnLevelUpButton))

		# member list (befizetesek)
		self.contribList = self.GetChild("ContribList")
		self.contribScroll = self.GetChild("ContribScroll")
		self.contribList.SetScrollBar(self.contribScroll)

		# contrib header item icons (same as req items)
		try:
			self.contribHeaderSlots = self.GetChild("ContribHeaderItems")
			self.contribHeaderSlots.SetOverInItemEvent(ui.__mem_func__(self.__OnOverInReqItem))
			self.contribHeaderSlots.SetOverOutItemEvent(ui.__mem_func__(self.__OnOverOutAnyItem))
		except:
			self.contribHeaderSlots = None

		# storage deposit only
		self.storageSlot.SetSelectEmptySlotEvent(ui.__mem_func__(self.__OnSelectEmptyStorageSlot))
		self.storageSlot.SetSelectItemSlotEvent(ui.__mem_func__(self.__OnSelectStorageItemSlot))
		self.storageSlot.SetOverInItemEvent(ui.__mem_func__(self.__OnOverInStorageItem))
		self.storageSlot.SetOverOutItemEvent(ui.__mem_func__(self.__OnOverOutAnyItem))

		# tooltips
		self.reqSlot.SetOverInItemEvent(ui.__mem_func__(self.__OnOverInReqItem))
		self.reqSlot.SetOverOutItemEvent(ui.__mem_func__(self.__OnOverOutAnyItem))

		if self.taxSlot:
			self.taxSlot.SetOverInItemEvent(ui.__mem_func__(self.__OnOverInTaxItem))
			self.taxSlot.SetOverOutItemEvent(ui.__mem_func__(self.__OnOverOutAnyItem))

		self.isLoaded = True
		self.RefreshAll()
		self.__RefreshMemberList()
		self.Hide()

	def Open(self):
		self.Show()
		self.SetCenterPosition()
		self.SetTop()
		self.__RefreshMemberList()

	def Close(self):
		if self.moneyDialog:
			self.moneyDialog.Close()
			self.moneyDialog = None
		if self.taxDialog:
			self.taxDialog.Close()
			self.taxDialog = None
		if self.payDialog:
			try:
				self.payDialog.Close()
			except:
				pass
			self.payDialog = None

		self.Hide()
		self.__OnOverOutAnyItem()

	def Destroy(self):
		self.Close()
		self.ClearDictionary()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	# ----------------- server->client setters -----------------
	def ClearData(self):
		self.storageItems = {}
		self.storageYang = 0

		self.reqItems = [(0, 0)] * 5
		self.reqYang = 0

		self.taxActive = 0
		self.taxDeadlineRaw = 0
		self.taxYang = 0
		self.taxItems = [(0, 0)] * 5

		self.contribByPID = {}
		self.contrib2ByPID = {}
		self.memberPaidByName = {}

		self.RefreshAll()
		self.__RefreshMemberList()

	def Done(self):
		# gr_done
		self.RefreshAll()
		self.__RefreshMemberList()

	def SetGuildLevel(self, level):
		try:
			self.guildLevel = int(level)
		except:
			self.guildLevel = 0
		self.RefreshAll()

	def SetPaidStatus(self, paid):
		try:
			self.paidStatus = 1 if int(paid) else 0
		except:
			self.paidStatus = 0
		self.RefreshAll()

	def SetStorageMoney(self, money):
		try:
			self.storageYang = long(money)
		except:
			self.storageYang = 0
		self.RefreshAll()

	def SetStorageSlot(self, slotIndex, vnum, count):
		try:
			slotIndex = int(slotIndex)
			vnum = int(vnum)
			count = int(count)
		except:
			return

		# UI: 5x3 (15 slot)
		if slotIndex < 0 or slotIndex >= STORAGE_SLOT_COUNT:
			return

		if vnum <= 0 or count <= 0:
			if slotIndex in self.storageItems:
				del self.storageItems[slotIndex]
		else:
			self.storageItems[slotIndex] = (vnum, count)

		self.RefreshAll()

	def SetRequired(self, reqYang, items):
		try:
			self.reqYang = long(reqYang)
		except:
			self.reqYang = 0

		tmp = []
		for i in xrange(5):
			try:
				v, c = items[i]
				tmp.append((int(v), int(c)))
			except:
				tmp.append((0, 0))
		self.reqItems = tmp
		self.RefreshAll()

	def SetTax(self, deadline, taxYang, items, active=1):
		try:
			self.taxActive = 1 if int(active) else 0
		except:
			self.taxActive = 0

		self.taxDeadlineRaw = deadline
		try:
			self.taxYang = long(taxYang)
		except:
			self.taxYang = 0

		tmp = []
		for i in xrange(5):
			try:
				v, c = items[i]
				tmp.append((int(v), int(c)))
			except:
				tmp.append((0, 0))
		self.taxItems = tmp
		self.RefreshAll()

	def SetContribution(self, a, b, c=None, d=None):
		# gr_contrib kezeles:
		# - regi: (name, paidMoney, paidItemTotal) ahol name lehet "[O] Nev" / "[X] Nev"
		# - uj:   (pid, paidFlag, paidMoney, paidItemTotal) szerver kuldi
		if d is not None:
			# uj format
			try:
				pid = int(a)
			except:
				pid = 0
			try:
				paidFlag = int(b)
			except:
				paidFlag = 0
			name = self.__GetNameByPID(pid)
			if not name:
				try:
					name = str(pid)
				except:
					name = "?"
			self.memberPaidByName[name] = 1 if paidFlag else 0
			self.__RefreshMemberList()
			return

		# regi format
		try:
			s = str(a)
		except:
			s = "?"
		paid = 0
		if s.startswith("[O]"):
			paid = 1
			s = s[3:].lstrip()
		elif s.startswith("[X]"):
			paid = 0
			s = s[3:].lstrip()
		self.memberPaidByName[s] = 1 if paid else 0
		self.__RefreshMemberList()


	def SetContribution2(self, pid, paidFlag, paidMoney, paidItemTotal, i0, i1, i2, i3, i4):
		# szerver: gr_contrib2 <pid> <paidFlag> <paidMoney> <paidItemTotal> <i0>..<i4>
		try:
			pid = int(pid)
		except:
			pid = 0
		try:
			pf = 1 if int(paidFlag) else 0
		except:
			pf = 0
		try:
			pm = long(paidMoney)
		except:
			try:
				pm = int(paidMoney)
			except:
				pm = 0
		try:
			items = [int(i0), int(i1), int(i2), int(i3), int(i4)]
		except:
			items = [0, 0, 0, 0, 0]
		self.contrib2ByPID[pid] = (pf, pm, items)

		name = self.__GetNameByPID(pid)
		if not name:
			try:
				name = str(pid)
			except:
				name = "?"
		paid = 1 if (pf or pm > 0 or sum(items) > 0) else 0
		self.memberPaidByName[name] = paid
		self.__RefreshMemberList()

	# ----------------- helpers -----------------
	def __GetNameByPID(self, pid):
		if not guild:
			return ""
		try:
			pid = int(pid)
		except:
			return ""
		try:
			cnt = guild.GetMemberCount()
		except:
			return ""
		for i in xrange(cnt):
			try:
				m_pid, m_name, _grade, _race, _level, _offer, _general = guild.GetMemberData(i)
			except:
				continue
			try:
				if int(m_pid) == pid:
					return str(m_name)
			except:
				pass
		return ""

	def __IsLeader(self):
		if not guild:
			return True
		try:
			master = guild.GetGuildMasterName()
		except:
			return True

		my = ""
		try:
			my = player.GetMainCharacterName()
		except:
			try:
				my = player.GetName()
			except:
				my = ""
		if not my:
			return True
		return (my == master)

	def __FormatDeadline(self):
		d = self.taxDeadlineRaw
		try:
			if isinstance(d, basestring):
				return d
		except:
			pass
		try:
			epoch = int(d)
		except:
			return "-"
		if epoch <= 0:
			return "-"
		try:
			t = time.localtime(epoch)
			return "%04d.%02d.%02d" % (t.tm_year, t.tm_mon, t.tm_mday)
		except:
			return str(epoch)

	def __BuildStorageCountByVnum(self):
		m = {}
		for _slot, vc in self.storageItems.items():
			try:
				vnum, cnt = vc
				vnum = int(vnum)
				cnt = int(cnt)
			except:
				continue
			if vnum <= 0 or cnt <= 0:
				continue
			m[vnum] = m.get(vnum, 0) + cnt
		return m

	def __RefreshMemberList(self):
		if not self.isLoaded:
			return
		try:
			self.contribList.RemoveAllItems()
		except:
			return

		members = []  # [(pid, name), ...]
		if guild:
			try:
				cnt = guild.GetMemberCount()
				for i in xrange(cnt):
					try:
						m_pid, m_name, _grade, _race, _level, _offer, _general = guild.GetMemberData(i)
					except:
						continue
					try:
						members.append((int(m_pid), str(m_name)))
					except:
						pass
			except:
				pass

		# fallback
		if not members:
			try:
				for nm in self.memberPaidByName.keys():
					members.append((0, nm))
			except:
				pass

		try:
			members.sort(key=lambda x: x[1].lower())
		except:
			try:
				members.sort(key=lambda x: x[1])
			except:
				pass

		for idx, (pid, nm) in enumerate(members):
			# which item columns are active (based on next level requirements)
			mask = []
			for i in xrange(5):
				try:
					vnum, _req = self.reqItems[i]
					mask.append(1 if int(vnum) > 0 else 0)
				except:
					mask.append(1)

			# contrib2: (paidFlag, paidMoney, [cnt0..4])
			try:
				pf, pm, items = self.contrib2ByPID.get(int(pid), (0, 0, [0, 0, 0, 0, 0]))
				pm = long(pm)
			except:
				pf, pm, items = (0, 0, [0, 0, 0, 0, 0])

			try:
				items = list(items)
			except:
				items = [0, 0, 0, 0, 0]
			if len(items) < 5:
				items += [0] * (5 - len(items))
			elif len(items) > 5:
				items = items[:5]

			paid = 1 if (pm > 0 or sum(items) > 0 or int(pf)) else 0
			self.memberPaidByName[nm] = paid

			try:
				w = self.contribList.GetWidth()
			except:
				w = 270
			it = GuildContribRowItem(w, idx)
			it.SetData(nm, paid, pm, items, mask)
			self.contribList.AppendItem(it)

	def RefreshAll(self):
		if not self.isLoaded:
			return

		self.txtLevel.SetText(str(self.guildLevel))
		self.txtPaid.SetText("O" if self.paidStatus else "X")
		self.txtStorageMoney.SetText(_SafeMoneyStr(self.storageYang))

		stByV = self.__BuildStorageCountByVnum()

		remainYang = self.reqYang - self.storageYang
		if remainYang < 0:
			remainYang = 0

		self.txtReqYang.SetText(_SafeMoneyStr(remainYang))
		self.txtReqYangTotal.SetText(_SafeMoneyStr(self.reqYang))

		for i in xrange(STORAGE_SLOT_COUNT):
			if i in self.storageItems:
				vnum, cnt = self.storageItems[i]
				self.storageSlot.SetItemSlot(i, int(vnum), int(cnt))
			else:
				self.storageSlot.ClearSlot(i)
		self.storageSlot.RefreshSlot()

		for i in xrange(5):
			vnum, reqCnt = self.reqItems[i]
			vnum = int(vnum)
			reqCnt = int(reqCnt)

			if vnum > 0 and reqCnt > 0:
				have = stByV.get(vnum, 0)
				rem = reqCnt - have
				if rem < 0:
					rem = 0

				showCount = rem if rem > 0 else 0
				self.reqSlot.SetItemSlot(i, vnum, showCount)
				if self.contribHeaderSlots:
					self.contribHeaderSlots.SetItemSlot(i, vnum, showCount)

			elif vnum > 0:
				self.reqSlot.SetItemSlot(i, vnum, 0)
				if self.contribHeaderSlots:
					self.contribHeaderSlots.SetItemSlot(i, vnum, 0)
			else:
				self.reqSlot.ClearSlot(i)
				if self.contribHeaderSlots:
					self.contribHeaderSlots.ClearSlot(i)

		self.reqSlot.RefreshSlot()
		if self.contribHeaderSlots:
			try:
				self.contribHeaderSlots.RefreshSlot()
			except:
				pass

		# tax panel ki van veve a kis ado rendszerben
		if self.txtTaxDeadline and self.txtTaxMoney and self.taxSlot:
			self.txtTaxDeadline.SetText(self.__FormatDeadline())
			self.txtTaxMoney.SetText(_SafeMoneyStr(self.taxYang))

			for i in xrange(5):
				vnum, cnt = self.taxItems[i]
				vnum = int(vnum)
				cnt = int(cnt)
				if self.taxActive and vnum > 0 and cnt > 0:
					self.taxSlot.SetItemSlot(i, vnum, cnt)
					try:
						self.taxNameTexts[i].SetText(_ItemNameByVnum(vnum))
						self.taxCountTexts[i].SetText(str(cnt))
					except:
						pass
				elif self.taxActive and vnum > 0:
					self.taxSlot.SetItemSlot(i, vnum, 0)
					try:
						self.taxNameTexts[i].SetText(_ItemNameByVnum(vnum))
						self.taxCountTexts[i].SetText("0")
					except:
						pass
				else:
					self.taxSlot.ClearSlot(i)
					try:
						self.taxNameTexts[i].SetText("-")
						self.taxCountTexts[i].SetText("")
					except:
						pass
			self.taxSlot.RefreshSlot()


	# ----------------- buttons -----------------
	def __OnDepositYangButton(self):
		if self.moneyDialog:
			self.moneyDialog.Close()
			self.moneyDialog = None

		dlg = uipickmoney.PickMoneyDialog()
		dlg.LoadDialog()
		try:
			dlg.SetMax(10)
		except:
			pass
		dlg.SetTitleName("Yang betetes")
		dlg.SetAcceptEvent(ui.__mem_func__(self.__OnDepositYangAccept))
		try:
			dlg.Open(player.GetMoney())
		except:
			dlg.Open(999999999999)
		self.moneyDialog = dlg

	def __OnDepositYangAccept(self, money):
		self.moneyDialog = None
		try:
			m = long(money)
		except:
			m = 0
		if m <= 0:
			return
		net.SendChatPacket("/gr_deposit_yang %d" % m)

	def __OnPayTaxButton(self):
		# kis ado befizetes: jatekos valasztja ki a mennyiseget
		if self.payDialog:
			try:
				self.payDialog.Close()
			except:
				pass
			self.payDialog = None

		self.payDialog = GuildRenewalPayDialog(self)
		try:
			self.payDialog.board.SetTitleName("Ado befizetes")
		except:
			pass
		self.payDialog.Open(self.reqItems)

	def __OnLevelUpButton(self):
		net.SendChatPacket("/gr_levelup")

	def __OnSetTaxButton(self):
		if not self.__IsLeader():
			chat.AppendChat(chat.CHAT_TYPE_INFO, "Csak a cehvezeto vethet ki adot.")
			return

		if self.taxDialog:
			self.taxDialog.Close()
			self.taxDialog = None

		defaultDays = 7
		defaultYang = self.taxYang if self.taxActive else 0

		self.taxDialog = GuildRenewalTaxDialog(self)
		self.taxDialog.Open(self.reqItems, defaultYangPerMember=defaultYang, defaultDays=defaultDays)

	# ----------------- storage deposit only -----------------
	def __OnSelectEmptyStorageSlot(self, slotIndex):
		if not mouseModule.mouseController.isAttached():
			return

		attType = mouseModule.mouseController.GetAttachedType()
		attSlot = mouseModule.mouseController.GetAttachedSlotNumber()
		attCount = mouseModule.mouseController.GetAttachedItemCount()

		if attType != player.SLOT_TYPE_INVENTORY:
			mouseModule.mouseController.DeattachObject()
			chat.AppendChat(chat.CHAT_TYPE_INFO, "Csak inventorybol lehet betenni.")
			return

		if attCount <= 0:
			try:
				attCount = player.GetItemCount(attSlot)
			except:
				attCount = 1

		mouseModule.mouseController.DeattachObject()
		if attCount <= 0:
			return

		net.SendChatPacket("/gr_deposit_item %d %d" % (int(attSlot), int(attCount)))

	def __OnSelectStorageItemSlot(self, slotIndex):
		mouseModule.mouseController.DeattachObject()
		chat.AppendChat(chat.CHAT_TYPE_INFO, "Ebbol a leltarbol kivenni nem lehet.")

	# ----------------- tooltips -----------------
	def __OnOverInStorageItem(self, slotIndex):
		try:
			vnum, _ = self.storageItems.get(int(slotIndex), (0, 0))
		except:
			vnum = 0
		if vnum and self.tooltipItem:
			self.tooltipItem.SetItemToolTip(int(vnum))

	def __OnOverInReqItem(self, slotIndex):
		idx = int(slotIndex)
		if idx < 0 or idx >= 5:
			return
		vnum, _ = self.reqItems[idx]
		if vnum and self.tooltipItem:
			self.tooltipItem.SetItemToolTip(int(vnum))

	def __OnOverInTaxItem(self, slotIndex):
		idx = int(slotIndex)
		if idx < 0 or idx >= 5:
			return
		vnum, _ = self.taxItems[idx]
		if vnum and self.tooltipItem:
			self.tooltipItem.SetItemToolTip(int(vnum))

	def __OnOverOutAnyItem(self):
		if self.tooltipItem:
			self.tooltipItem.HideToolTip()