import app
import net
import player
import ui, uicommon, constinfo, uitooltip, localeinfo
import systemSetting

CHECKBOX_X = -34

class ItemSpecial(ui.ListBoxEx.Item):
	def __init__(self, text, idx, width = 0, height = 0):
		ui.ListBoxEx.Item.__init__(self)
		if width != 0 and height != 0:
			bar = ui.Bar()
			bar.SetParent(self)
			bar.SetSize(width, height)
			bar.SetPosition(0, 0)
			bar.SetColor(0x00000000)
			bar.Show()
			self.bar = bar

		textLine = ui.TextLine()
		if width != 0 and height != 0:
			textLine.SetParent(self.bar)
		else:
			textLine.SetParent(self)
		textLine.SetPosition(4, 0)
		textLine.SetText(text)
		textLine.Show()
		self.textLine = textLine

		self.index = idx

	def GetIndex(self):
		return self.index

	def __del__(self):
		ui.ListBoxEx.Item.__del__(self)

class BiologWindow(ui.ScriptWindow):
	missionName = [
					localeinfo.BIOLOGIST_MISSION_NAME_1,
					localeinfo.BIOLOGIST_MISSION_NAME_2,
					localeinfo.BIOLOGIST_MISSION_NAME_3,
					localeinfo.BIOLOGIST_MISSION_NAME_4,
					localeinfo.BIOLOGIST_MISSION_NAME_5,
					localeinfo.BIOLOGIST_MISSION_NAME_6,
					localeinfo.BIOLOGIST_MISSION_NAME_7,
					localeinfo.BIOLOGIST_MISSION_NAME_8,
					localeinfo.BIOLOGIST_MISSION_NAME_9,
					localeinfo.BIOLOGIST_MISSION_NAME_10,
					localeinfo.BIOLOGIST_MISSION_NAME_11,
					localeinfo.BIOLOGIST_MISSION_NAME_12,
					localeinfo.BIOLOGIST_MISSION_NAME_13,
					localeinfo.BIOLOGIST_MISSION_NAME_14,
					localeinfo.BIOLOGIST_MISSION_NAME_15,
					localeinfo.BIOLOGIST_MISSION_NAME_16,
	]

	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.isLoaded = 0
		self.board = None
		self.titleBar = None
		self.missionID = -1
		self.missionNameText = None
		self.itemVnum = -1
		self.requiredItemSlot = None
		self.itemToolTip = None
		self.countDelivered = None
		self.countBarProgress = None
		self.itemIcon1 = None
		self.itemIcon2 = None
		self.potionBox = None
		self.potionPlusBox = None
		self.timeBox = None
		self.requiredTime = -1
		self.timetext = None
		self.deliveryButton = None
		self.shopButton = None
		self.rewardSlot = None
		self.listbox = None
		self.rewardListType = []
		self.rewardListValue = []
		self.rewardTypeText = None
		self.completed = 0
		self.rewardButton = None
		self.confirmdialog = None
		self.Load()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def Load(self):
		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "uiscript/biologwindow.py")
		except:
			import exception
			exception.Abort("BiologWindow.LoadWindow.LoadObject")
		
		try:
			self.board = self.GetChild("board")
			self.titleBar = self.GetChild("TitleBar")
			self.missionNameText = self.GetChild("missionNameText")
			self.requiredItemSlot = self.GetChild("requiredItem")
			self.countDelivered = self.GetChild("countDelivered")
			self.countBarProgress = self.GetChild("countBarProgress")
			self.itemIcon1 = self.GetChild("itemIcon1")
			self.itemIcon2 = self.GetChild("itemIcon2")
			self.potionBox = ui.CheckBox()
			self.potionBox.SetParent(self)
			self.potionBox.SetPosition(CHECKBOX_X, 134)
			self.potionBox.SetWindowHorizontalAlignCenter()
			self.potionBox.SetMainParent(self, 1)
			self.potionBox.Show()
			self.potionPlusBox = ui.CheckBox()
			self.potionPlusBox.SetParent(self)
			self.potionPlusBox.SetPosition(CHECKBOX_X + 50, 134)
			self.potionPlusBox.SetWindowHorizontalAlignCenter()
			self.potionPlusBox.SetMainParent(self, 2)
			self.potionPlusBox.Show()
			self.timeBox = ui.CheckBox()
			self.timeBox.SetParent(self)
			self.timeBox.SetPosition(-45, 164)
			self.timeBox.SetIndex(3)
			self.timeBox.SetWindowHorizontalAlignCenter()
			if systemSetting.GetBiologistAlert() == True:
				self.timeBox.OnMouseLeftButtonUp()
			self.timeBox.Show()
			self.timetext = self.GetChild("TimeText")
			self.deliveryButton = self.GetChild("Deliver_Button")
			self.shopButton = self.GetChild("Shop_Button")
			self.rewardSlot = self.GetChild("rewardSlot")
			self.rewardTypeText = self.GetChild("rewardTypeText")
			self.rewardButton = self.GetChild("RewardButton")
		except:
			import exception
			exception.Abort("BiologWindow.LoadWindow.BindObject")
		
		self.titleBar.SetCloseEvent(ui.__mem_func__(self.Close))
		self.requiredItemSlot.SetOverInItemEvent(ui.__mem_func__(self.OverInItem))
		self.requiredItemSlot.SetOverOutItemEvent(ui.__mem_func__(self.OverOutItem))
		self.itemIcon1.SetStringEvent("MOUSE_OVER_IN", self.OverInImg1)
		self.itemIcon1.SetStringEvent("MOUSE_OVER_OUT", self.OverOutItem)
		self.itemIcon2.SetStringEvent("MOUSE_OVER_IN", self.OverInImg2)
		self.itemIcon2.SetStringEvent("MOUSE_OVER_OUT", self.OverOutItem)
		self.deliveryButton.SetEvent(ui.__mem_func__(self.Delivery))
		self.shopButton.SetEvent(ui.__mem_func__(self.Shop))
		self.rewardButton.SetEvent(ui.__mem_func__(self.RewardPress))
		self.isLoaded = 1

	def Destroy(self):
		self.ClearDictionary()
		self.isLoaded = 0
		self.board = None
		self.titleBar = None
		self.missionID = -1
		self.missionNameText = None
		self.itemVnum = -1
		self.requiredItemSlot = None
		self.itemToolTip = None
		self.countDelivered = None
		self.countBarProgress = None
		self.itemIcon1 = None
		self.itemIcon2 = None
		self.potionBox = None
		self.potionPlusBox = None
		self.timeBox = None
		self.requiredTime = -1
		self.timetext = None
		self.deliveryButton = None
		self.shopButton = None
		self.rewardSlot = None
		if self.listbox:
			del self.listbox
			self.listbox = None
		self.rewardListType = []
		self.rewardListValue = []
		self.rewardTypeText = None
		self.completed = 0
		self.rewardButton = None
		if self.confirmdialog:
			self.confirmdialog.Destroy()
			self.confirmdialog = None

	def OnCheck(self, index):
		if index == 1:
			if self.potionPlusBox.GetCheckStatus() == True:
				self.potionPlusBox.OnMouseLeftButtonUp()
		elif index == 2:
			if self.potionBox.GetCheckStatus() == True:
				self.potionBox.OnMouseLeftButtonUp()

	def SetItemToolTip(self, itemToolTip):
		self.itemToolTip = itemToolTip

	def OverInImg1(self):
		if self.itemToolTip != None:
			self.itemToolTip.SetItemToolTip(40144)

	def OverInImg2(self):
		if self.itemToolTip != None:
			self.itemToolTip.SetItemToolTip(40143)

	def OverInItem(self, overSlotPos):
		if self.itemToolTip != None:
			self.itemToolTip.SetItemToolTip(self.itemVnum)

	def OverOutItem(self):
		if self.itemToolTip != None:
			self.itemToolTip.HideToolTip()

	def Delivered(self, args):
		self.completed = int(args.split("#")[0])
		if self.completed == 1:
			import chat
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.BIOLOGIST_ALERT3)
		
		self.deliveredCount = int(args.split("#")[1])
		constinfo.remainBiologistTime = int(args.split("#")[2])
		name = player.GetName()
		if name in constinfo.notifiedBiologist:
			constinfo.notifiedBiologist.remove(player.GetName())
		
		self.Refresh()

	def Delivery(self):
		if self.deliveredNeedCount <= self.deliveredCount:
			import chat
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.BIOLOGIST_ALERT2)
			return
		
		is1 = 0
		if self.potionBox.GetCheckStatus():
			is1 = 1
		
		is2 = 0
		if self.potionPlusBox.GetCheckStatus():
			is2 = 1
		
		net.SendChatPacket("/delivery_biologist %d %d" % (is1, is2))

	def RequestOpen(self):
		if self.isLoaded == 0:
			return
		
		if self.IsShow():
			self.Close()
		else:
			net.SendChatPacket("/open_biologist")

	def Refresh(self):
		w = float(float(135) / float(self.deliveredNeedCount)) * float(self.deliveredCount)
		self.countBarProgress.SetSize(int(w), 13)
		self.countDelivered.SetText(localeinfo.BIOLOGIST_COUNT % (self.deliveredCount, self.deliveredNeedCount))

	def Open(self, args):
		if self.isLoaded == 0:
			return
		
		self.missionID = int(args.split("#")[0])
		try:
			self.missionNameText.SetText(self.missionName[self.missionID])
		except:
			self.Close()
			return
		
		self.completed = int(args.split("#")[6])
		if self.listbox:
			del self.listbox
			self.listbox = None
		self.listbox = ui.ListBoxEx()
		self.listbox.SetParent(self.rewardSlot)
		self.listbox.SetPosition(2, 2)
		self.listbox.SetSize(207, 62)
		self.listbox.SetItemStep(16)
		self.listbox.SetViewItemCount(4)
		self.listbox.SetItemSize(207, 16)
		self.listbox.Show()
		
		for i in xrange(4):
			type = self.rewardListType[i]
			value = self.rewardListValue[i]
			if type == 0:
				continue
			
			text = self.itemToolTip.__GetAffectString(type, value)
			if self.canchoosebonuses == 0:
				self.listbox.AppendItem(ItemSpecial(text, i, 207, 16))
			else:
				self.listbox.AppendItem(ItemSpecial(text, i))
		
		self.itemVnum = int(args.split("#")[1])
		self.requiredItemSlot.SetItemSlot(0, self.itemVnum, 0)
		self.deliveredNeedCount = int(args.split("#")[2])
		self.deliveredCount = int(args.split("#")[3])
		self.requiredTime = int(args.split("#")[4])
		constinfo.remainBiologistTime = int(args.split("#")[5])
		self.Refresh()
		self.Show()
		self.SetCenterPosition()

	def OnUpdate(self):
		t = constinfo.remainBiologistTime - app.GetGlobalTimeStamp()
		if t > 0:
			self.timetext.SetText(localeinfo.BIOLOGIST_WAIT % (localeinfo.FormatTime(t)))
		else:
			self.timetext.SetText(localeinfo.BIOLOGIST_WAIT % (localeinfo.FormatTime(0)))

	def RewardConfirm(self, arg):
		if self.confirmdialog:
			if arg == True:
				net.SendChatPacket("/reward_biologist %d" % (self.confirmdialog.dropCount))
		
		self.confirmdialog.Close()
		self.confirmdialog.Destroy()
		self.confirmdialog = None

	def Shop(self):
		net.SendOpenShopPacket(52)

	def RewardPress(self):
		if self.deliveredNeedCount > self.deliveredCount:
			import chat
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.BIOLOGIST_ALERT4)
			return
		
		if self.canchoosebonuses == 1:
			item = self.listbox.GetSelectedItem()
			if item or item == 0:
				if self.confirmdialog:
					self.confirmdialog.Destroy()
					self.confirmdialog = None
				
				self.confirmdialog = uicommon.QuestionDialog()
				self.confirmdialog.SetText(localeinfo.BIOLOGIST_CONFIRM_TYPE1)
				self.confirmdialog.SetAcceptEvent(lambda arg = True: self.RewardConfirm(arg))
				self.confirmdialog.SetCancelEvent(lambda arg = False: self.RewardConfirm(arg))
				self.confirmdialog.dropCount = item.GetIndex()
				self.confirmdialog.Open()
			else:
				import chat
				chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.BIOLOGIST_ALERT_TYPE1)
		else:
			net.SendChatPacket("/reward_biologist 999")

	def Reward(self, args):
		self.canchoosebonuses = int(args.split("#")[0])
		if self.canchoosebonuses == 0:
			self.rewardTypeText.SetText(localeinfo.BIOLOGIST_REWARD_TYPE0)
		else:
			self.rewardTypeText.SetText(localeinfo.BIOLOGIST_REWARD_TYPE1)
		self.rewardListType = []
		self.rewardListValue = []
		j = 0
		for i in xrange(4):
			self.rewardListType.append(int(args.split("#")[j + 1]))
			self.rewardListValue.append(int(args.split("#")[j + 2]))
			j += 2

	def Next(self, args):
		if not self.IsShow():
			return
		
		self.missionID = int(args.split("#")[0])
		try:
			self.missionNameText.SetText(self.missionName[self.missionID])
		except:
			self.Close()
			return
		
		self.completed = int(args.split("#")[6])
		if self.listbox:
			del self.listbox
			self.listbox = None
		self.listbox = ui.ListBoxEx()
		self.listbox.SetParent(self.rewardSlot)
		self.listbox.SetPosition(2, 2)
		self.listbox.SetSize(207, 62)
		self.listbox.SetItemStep(16)
		self.listbox.SetViewItemCount(4)
		self.listbox.SetItemSize(207, 16)
		self.listbox.Show()
		
		for i in xrange(4):
			type = self.rewardListType[i]
			value = self.rewardListValue[i]
			if type == 0:
				continue
			
			text = self.itemToolTip.__GetAffectString(type, value)
			if self.canchoosebonuses == 0:
				self.listbox.AppendItem(ItemSpecial(text, i, 207, 16))
			else:
				self.listbox.AppendItem(ItemSpecial(text, i))
		
		self.itemVnum = int(args.split("#")[1])
		self.requiredItemSlot.SetItemSlot(0, self.itemVnum, 0)
		self.deliveredNeedCount = int(args.split("#")[2])
		self.deliveredCount = int(args.split("#")[3])
		self.requiredTime = int(args.split("#")[4])
		self.Refresh()

	def Close(self):
		self.Hide()

	def OnPressEscapeKey(self):
		self.Close()
		return True

class BiologWindow_Change(ui.ScriptWindow):
	missionName = [
					localeinfo.BIOLOGIST_MISSION_NAME_1,
					localeinfo.BIOLOGIST_MISSION_NAME_2,
					localeinfo.BIOLOGIST_MISSION_NAME_3,
					localeinfo.BIOLOGIST_MISSION_NAME_4,
					localeinfo.BIOLOGIST_MISSION_NAME_5,
					localeinfo.BIOLOGIST_MISSION_NAME_6,
					localeinfo.BIOLOGIST_MISSION_NAME_7,
					localeinfo.BIOLOGIST_MISSION_NAME_8,
					localeinfo.BIOLOGIST_MISSION_NAME_9,
					localeinfo.BIOLOGIST_MISSION_NAME_10,
					localeinfo.BIOLOGIST_MISSION_NAME_11,
					localeinfo.BIOLOGIST_MISSION_NAME_12,
					localeinfo.BIOLOGIST_MISSION_NAME_13,
					localeinfo.BIOLOGIST_MISSION_NAME_14,
	]

	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.isLoaded = 0
		self.board = None
		self.titleBar = None
		self.myBonusList = {}
		self.myIdList = []
		self.itemtooltip = None
		self.missionslot = None
		self.bonusslot = None
		self.listbox = None
		self.confirmdialog = None
		self.Load()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def Load(self):
		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "uiscript/biologwindow_changebonus.py")
		except:
			import exception
			exception.Abort("BiologWindow_Change.LoadWindow.LoadObject")
		
		try:
			self.board = self.GetChild("board")
			self.titleBar = self.GetChild("TitleBar")
			self.missionslot = self.GetChild("missionslot")
			self.bonusslot = self.GetChild("bonusslot")
			self.changebtn = self.GetChild("ChangeButton")
		except:
			import exception
			exception.Abort("BiologWindow_Change.LoadWindow.BindObject")
		
		self.titleBar.SetCloseEvent(ui.__mem_func__(self.Close))
		self.changebtn.SetEvent(ui.__mem_func__(self.OnChangeBtn))
		self.isLoaded = 1

	def Destroy(self):
		self.ClearDictionary()
		self.isLoaded = 0
		self.board = None
		self.titleBar = None
		self.myBonusList = {}
		self.myIdList = []
		self.itemtooltip = None
		self.missionslot = None
		self.bonusslot = None
		self.listbox = None
		if self.confirmdialog:
			self.confirmdialog.Destroy()
			self.confirmdialog = None

	def SetItemToolTip(self, itemtooltip):
		self.itemtooltip = itemtooltip

	def Clear(self):
		self.myBonusList = {}
		self.myIdList = []

	def OnSelect(self, arg):
		for i in xrange(16):
			if i in self.myBonusList.keys():
				self.myBonusList[i].Hide()
		
		item = self.listbox.GetSelectedItem()
		self.myBonusList[item.GetIndex()].Show()
		self.myBonusList[item.GetIndex()].SelectIndex(0)

	def Append(self, args):
		exist = False
		missionID = int(args.split("#")[0])
		for i in self.myIdList:
			if i == missionID:
				exist = True
				break
		
		if exist == False:
			self.myIdList.append(missionID)
			listbox = ui.ListBoxEx()
			listbox.SetParent(self.bonusslot)
			listbox.SetPosition(2, 2)
			listbox.SetSize(207, 62)
			listbox.SetItemStep(16)
			listbox.SetViewItemCount(4)
			listbox.SetItemSize(207, 16)
			listbox.Hide()
			self.myBonusList[missionID] = listbox
		
		type = int(args.split("#")[1])
		if type != 0:
			text = self.itemtooltip.__GetAffectString(type, int(args.split("#")[2]))
			self.myBonusList[missionID].AppendItem(ItemSpecial(text, int(args.split("#")[3])))

	def Confirm(self, arg):
		if self.confirmdialog:
			if arg == True:
				net.SendChatPacket("/change_biologist %d %d" % (self.confirmdialog.dropCount, self.confirmdialog.dropCount2))
		
		self.confirmdialog.Close()
		self.confirmdialog.Destroy()
		self.confirmdialog = None

	def OnChangeBtn(self):
		item = self.listbox.GetSelectedItem()
		idx = 0
		if item or item == 0:
			idx = item.GetIndex()
			if not idx in self.myBonusList.keys():
				return
			
			item2 = self.myBonusList[idx].GetSelectedItem()
			if not item2 and item2 != 0:
				return
			
			if self.confirmdialog:
				self.confirmdialog.Destroy()
				self.confirmdialog = None
			
			self.confirmdialog = uicommon.QuestionDialog()
			self.confirmdialog.SetText(localeinfo.BIOLOGIST_CONFIRM_TYPE1)
			self.confirmdialog.SetAcceptEvent(lambda arg = True: self.Confirm(arg))
			self.confirmdialog.SetCancelEvent(lambda arg = False: self.Confirm(arg))
			idx = item.GetIndex()
			self.confirmdialog.dropCount = idx
			self.confirmdialog.dropCount2 = item2.GetIndex()
			self.confirmdialog.Open()
		else:
			import chat
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.BIOLOGIST_ALERT_TYPE1)

	def Open(self):
		if self.isLoaded == 0:
			return
		
		if self.listbox:
			del self.listbox
			self.listbox = None
		self.listbox = ui.ListBoxEx()
		self.listbox.SetParent(self.missionslot)
		self.listbox.SetPosition(2, 2)
		self.listbox.SetSize(207, 62)
		self.listbox.SetItemStep(16)
		self.listbox.SetViewItemCount(4)
		self.listbox.SetItemSize(207, 16)
		self.listbox.SetSelectEvent(self.OnSelect)
		self.listbox.Show()
		
		for i in self.myIdList:
			try:
				name = self.missionName[i]
				self.listbox.AppendItem(ItemSpecial(name, i))
			except:
				pass
		self.listbox.SelectIndex(0)
		self.OnSelect(0)
		self.Show()
		self.SetCenterPosition()

	def Close(self):
		self.Hide()

	def OnPressEscapeKey(self):
		self.Close()
		return True

