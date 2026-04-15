import ui, localeinfo
import net
import player

import item

class Main(ui.ScriptWindow):
	AFFECT_DICT = {
					0 : localeinfo.PET_APPLY_MAX_HP_PCT,
					1 : localeinfo.PET_APPLY_ATTBONUS_MONSTER,
					2 : localeinfo.PET_APPLY_MEDI_PVM,
	}

	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.parent = None

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def __GetAffectString(self, affectType, affectValue):
		try:
			return self.AFFECT_DICT[affectType](affectValue)
		except TypeError:
			return "UNKNOWN_VALUE[%s] %s" % (affectType, affectValue)
		except KeyError:
			return "UNKNOWN_TYPE[%s] %s" % (affectType, affectValue)

	def pointop(self, n):
		t = int(n)
		if t / 10 < 1:
			return "0."+n
		else:		
			return n[0:len(n)-1]+"."+n[len(n)-1:]

	def Open(self, parent, srcItemSlotPos, dstItemSlotPos):
		try:
			PythonScriptLoader = ui.PythonScriptLoader()
			PythonScriptLoader.LoadScriptFile(self, "uiscript/petdialog_enchant.py")
		except:
			import exception
			exception.Abort("PetDialogRemoveWindow.LoadDialog.LoadObject")

		try:
			self.titleBar = self.GetChild("TitleBar")
			self.cancelBTN = self.GetChild("CancelButton")
			self.acceptBTN = self.GetChild("AcceptButton")
			self.wndItem = self.GetChild("ItemSlot")
			self.bns = []
			for i in xrange(3):
				self.bns.append(self.GetChild("bns%d" % (i)))
		except:
			import exception
			exception.Abort("PetDialogRemoveWindow.LoadDialog.BindObject")
		
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
		for i in xrange(3):
			(t, v) = player.GetItemAttribute(dstItemSlotPos, i)
			v = self.pointop(str(v))
			if t != 0:
				self.bns[j].SetText(self.__GetAffectString(j, str(v)))
				self.bns[j].SetEvent(lambda arg = i: self.Accept(arg))
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
		
		net.SendChatPacket("/petenchant " + str(arg))
		self.bns[arg].Down()

	def OverOutItem(self):
		if self.parent != None:
			self.parent.OverOutItem()

	def OverInItem(self, overSlotPos):
		if self.parent != None:
			self.parent.OverInItem(self.dstItemSlotPos)

	def OnAccept(self):
		net.SendItemUseToItemPacket(self.srcItemSlotPos, self.dstItemSlotPos)
		self.Close()

	def CloseAsk(self):
		self.Hide()

	def Close(self):
		self.Hide()
		self.parent.OnCloseRemoveAttrCostume()

	def OnPressEscapeKey(self):
		self.Close()
		return True

