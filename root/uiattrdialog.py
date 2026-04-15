import ui, localeinfo
import net, item, player

class Remove(ui.ScriptWindow):
	AFFECT_DICT = {
					item.APPLY_ATTBONUS_MONSTER : localeinfo.TOOLTIP_APPLY_ATTBONUS_MONSTER,
					item.APPLY_ATTBONUS_BOSS : localeinfo.TOOLTIP_APPLY_ATTBONUS_BOSS,
					item.APPLY_ATTBONUS_METIN : localeinfo.TOOLTIP_APPLY_ATTBONUS_METIN,
					item.APPLY_ATTBONUS_HUMAN : localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN,
					item.APPLY_RESIST_MEZZIUOMINI : localeinfo.TOOLTIP_APPLY_RESIST_MEZZIUOMINI,
	}

	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.parent = None

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def __GetAffectString(self, affectType, affectValue):
		if 0 == affectType:
			return "None"
		
		if 0 == affectValue:
			return "None"
		
		try:
			return self.AFFECT_DICT[affectType](affectValue)
		except TypeError:
			return "UNKNOWN_VALUE[%s] %s" % (affectType, affectValue)
		except KeyError:
			return "UNKNOWN_TYPE[%s] %s" % (affectType, affectValue)

	def Open(self, parent, srcItemSlotPos, dstItemSlotPos):
		try:
			PythonScriptLoader = ui.PythonScriptLoader()
			PythonScriptLoader.LoadScriptFile(self, "uiscript/attrdialog_remove.py")
		except:
			import exception
			exception.Abort("AttrDialogRemove.LoadDialog.LoadObject")

		try:
			self.titleBar = self.GetChild("TitleBar")
			self.cancelBTN = self.GetChild("CancelButton")
			self.acceptBTN = self.GetChild("AcceptButton")
			self.wndItem = self.GetChild("ItemSlot")
			self.bns = []
			for i in xrange(2):
				self.bns.append(self.GetChild("bns%d" % (i)))
		except:
			import exception
			exception.Abort("AttrDialogRemove.LoadDialog.BindObject")
		
		self.parent = parent
		self.titleBar.SetCloseEvent(ui.__mem_func__(self.Close))
		self.cancelBTN.SetEvent(ui.__mem_func__(self.Close))
		self.acceptBTN.SetEvent(ui.__mem_func__(self.OnAccept))
		self.srcItemSlotPos = srcItemSlotPos
		self.dstItemSlotPos = dstItemSlotPos
		itemVnum = player.GetItemIndex(self.dstItemSlotPos)
		self.wndItem.SetItemSlot(0, itemVnum, 0)
		self.wndItem.SetOverInItemEvent(ui.__mem_func__(self.OverInItem))
		self.wndItem.SetOverOutItemEvent(ui.__mem_func__(self.OverOutItem))
		
		for item in self.bns:
			item.Hide()
		
		j = 0
		for i in xrange(player.ATTRIBUTE_SLOT_RARE_NUM):
			i += player.ATTRIBUTE_SLOT_NORM_NUM
			(t, v) = player.GetItemAttribute(dstItemSlotPos, i)
			if t != 0:
				self.bns[j].SetText(self.__GetAffectString(t, v))
				self.bns[j].SetEvent(lambda arg = i-player.ATTRIBUTE_SLOT_NORM_NUM: self.Accept(arg))
				self.bns[j].Show()
				j += 1
		
		if j >= 1:
			self.Accept(0)
		
		self.SetCenterPosition()
		self.Show()

	def Accept(self, arg):
		for item in self.bns:
			if item.IsShow():
				item.SetUp()
		
		net.SendChatPacket("/attrdialog_remove " + str(arg))
		self.bns[arg].Down()

	def OverOutItem(self):
		if self.parent != None:
			self.parent.OverOutItem()

	def OverInItem(self, overSlotPos):
		if self.parent != None:
			self.parent.OverInItem(self.dstItemSlotPos)

	def OnAccept(self):
		net.SendItemUseToItemPacket(player.EXTRA_INVENTORY, self.srcItemSlotPos, player.INVENTORY, self.dstItemSlotPos)
		self.Close()

	def CloseAsk(self):
		self.Hide()

	def Close(self):
		self.Hide()
		self.parent.OnCloseRemoveAttrCostume()

	def OnPressEscapeKey(self):
		self.Close()
		return True

