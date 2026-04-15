import net
import app
import event
import ui, uiscriptlocale, uicommon, uitooltip, item, localeinfo

class DailyGift(ui.ScriptWindow):

	ITEMS = []
	CANTS = []
	
	def __init__(self):
		ui.ScriptWindow.__init__(self, "TOP_MOST")
		self.LoadWindow()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def Close(self):
		self.Hide()

	def Open(self):
		self.Show()
		self.SetCenterPosition()
		net.SendChatPacket("/daily_reward_reload")

	def LoadWindow(self):
		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "uiscript/dailygiftnew.py")
		except:
			import exception
			exception.Abort("test.LoadDialog.LoadScript")
	
		self.slots = {}
		self.daily = 0
		self.endTime = None
	
		self.tooltipItem = uitooltip.ItemToolTip()
		self.tooltipItem.Hide()

		
		self.bg = {}
		x_text = -275
		x_button = 13
		for i in xrange(7):
			self.bg[i] = {}

			self.bg[i]["text"] = ui.TextLine()
			self.bg[i]["text"].SetParent(self)
			self.bg[i]["text"].SetPosition(x_text+4,62+7)
			self.bg[i]["text"].SetWindowHorizontalAlignCenter()
			self.bg[i]["text"].SetHorizontalAlignCenter()
			self.bg[i]["text"].SetText(localeinfo.DAILYGIFT_TXT1 % int(i + 1))
			self.bg[i]["text"].Show()
			
			self.bg[i]["button"] = ui.Button()
			self.bg[i]["button"].SetParent(self)
			self.bg[i]["button"].SetPosition(x_button+10,169+7)
			self.bg[i]["button"].SetUpVisual("d:/ymir work/ui/public/dailygift/normal.png")
			self.bg[i]["button"].SetOverVisual("d:/ymir work/ui/public/dailygift/hover.png")
			self.bg[i]["button"].SetDownVisual("d:/ymir work/ui/public/dailygift/active.png")
			self.bg[i]["button"].SetDisableVisual("d:/ymir work/ui/public/dailygift/active.png")
			self.bg[i]["button"].SetEvent(lambda x = i : self.GetReward(x))
			self.bg[i]["button"].Disable()
			self.bg[i]["button"].Show()
			
			x_text = x_text + 90
			x_button = x_button + 90
		
		self.ts = {}
		x_text = -272
		for i in xrange(7):
			self.ts[i] = {}
			
			self.ts[i]["text"] = ui.TextLine()
			#self.ts[i]["text"].SetParent(self)
			#self.ts[i]["text"].SetPosition(x_text+4,170+7)
			self.ts[i]["text"].SetParent(self.bg[i]["button"])
			self.ts[i]["text"].SetPosition(2, 2)
			self.ts[i]["text"].SetWindowHorizontalAlignCenter()
			self.ts[i]["text"].SetHorizontalAlignCenter()
			self.ts[i]["text"].SetText(localeinfo.DAILYGIFT_TXT2)
			self.ts[i]["text"].Show()
			
			x_text = x_text + 90
		
		self.rg = ui.TextLine()
		self.rg.SetParent(self)
		self.rg.SetPosition(0,7+7)
		self.rg.SetWindowHorizontalAlignCenter()
		self.rg.SetHorizontalAlignCenter()
		self.rg.SetText(localeinfo.DAILYGIFT_TXT3)
		self.rg.Show()
		
		self.closeBtn = ui.Button()
		self.closeBtn.SetParent(self)
		self.closeBtn.SetPosition(624+10,8+6)
		self.closeBtn.SetUpVisual("d:/ymir work/ui/public/close_button_01.sub")
		self.closeBtn.SetOverVisual("d:/ymir work/ui/public/close_button_02.sub")
		self.closeBtn.SetDownVisual("d:/ymir work/ui/public/close_button_03.sub")
		self.closeBtn.SetEvent(lambda : self.Close())
		self.closeBtn.Show()
		
		self.items = ui.GridSlotWindow()
		self.items.SetParent(self)
		self.items.SetPosition(84+11, 292+7)
		self.items.ArrangeSlot(0,10,3,32,32,0,0)
		self.items.SetOverInItemEvent(ui.__mem_func__(self.OverInItem))
		self.items.SetOverOutItemEvent(ui.__mem_func__(self.OverOutItem))
		self.items.SetOverInEmptySlotEvent(ui.__mem_func__(self.OverOutItem))
		self.items.SetOverOutEmptySlotEvent(ui.__mem_func__(self.OverOutItem))
		self.items.SetSlotBaseImage("d:/ymir work/drakon2/inventory/slot.png",1.0,1.0,1.0,1.0)
		self.items.RefreshSlot()
		self.items.Show()

		self.text = {}
		self.pos = [[283,254], [145+(32*10)+15,254]]
		for i in xrange(2):
			self.text[i] = ui.TextLine()
			self.text[i].SetParent(self)
			self.text[i].SetPosition(self.pos[i][0]+11, self.pos[i][1]+7)
			self.text[i].SetText((uiscriptlocale.DAILY_REWARD04,uiscriptlocale.DAILY_REWARD05)[i])
			self.text[i].Show()

		# self.SetDailyReward(1)
		# self.SetTime(app.GetGlobalTimeStamp()*100)

	def SetDailyReward(self, idx):
		self.daily = int(idx)
		self.bg[self.daily]["button"].Enable()

	def GetActualDailyReward(self):
		return self.daily

	def SetTime(self, time):
		self.endTime = int(time)
		if self.endTime > 0:
			time = self.endTime - app.GetGlobalTimeStamp()
			day = int(int((time / 60) / 60) / 24)
			leftTime = localeinfo.SecondToDHM(self.endTime - app.GetGlobalTimeStamp())
			text = " %s : %s" % (localeinfo.LEFT_TIME, leftTime)
			self.text[1].SetText(text)
		# else:
			# self.bg[self.GetActualDailyReward()]["button"].Enable()
	
	def CheckTime(self):
		if self.endTime != None:
			time = self.endTime - app.GetGlobalTimeStamp()
			day = int(int((time / 60) / 60) / 24)
			leftTime = localeinfo.SecondToDHM(self.endTime - app.GetGlobalTimeStamp())
			text = " (%s : %s)" % (localeinfo.LEFT_TIME, leftTime)
			self.text[1].SetText(text)
			# if self.endTime <= 0:
				# self.bg[self.GetActualDailyReward()]["button"].Enable()

	def DeleteRewards(self):
		for i in xrange(len(self.ITEMS)):
			self.items.SetItemSlot(i,0)
			self.slots[i] = 0
		self.ITEMS = []
		self.CANTS = []

	def SetReward(self, items, cant):
		self.ITEMS.append(int(items))
		self.CANTS.append(int(cant))

	def SetRewardDone(self):
		for i in xrange(len(self.ITEMS)):
			self.items.SetItemSlot(i,self.ITEMS[i], self.CANTS[i])
			self.slots[i] = self.ITEMS[i]

	def GetReward(self,x):
		for i in xrange(7):
			self.bg[i]["button"].Disable()
		net.SendChatPacket("/daily_reward_get_reward")
		net.SendChatPacket("/daily_reward_reload")

	def OverInItem(self, slot):
		self.tooltipItem.SetItemToolTip(self.slots[slot])
		
	def OverOutItem(self):
		if self.tooltipItem:
			self.tooltipItem.HideToolTip()

	def OnUpdate(self):
		self.CheckTime()

	def OnPressEscapeKey(self):
		self.Close()
		return True

