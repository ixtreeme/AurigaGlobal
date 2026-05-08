import net
import player
import ui, uicommon, localeinfo, constinfo

class Window(ui.ScriptWindow):
	SLOTS_COUNT = 6

	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.isLoaded = 0
		self.board = None
		self.slots = []
		self.statusSlots = []
		self.mapNameSlots = []
		self.coordinatesSlots = []
		self.saveButtons = []
		self.deleteButtons = []
		self.goButtons = []
		self.slotDetails = {
								0 : [],
								1 : [],
								2 : [],
								3 : [],
								4 : [],
								5 : []
		}
		self.questionDialog = None
		self.inputDialog = None
		self.savedY = 0
		self.Load()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def Load(self):
		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "uiscript/savepointwindow.py")
		except:
			import exception
			exception.Abort("SavePointWindow.LoadWindow.LoadObject")
		
		try:
			self.board = self.GetChild("board")
			for i in xrange(self.SLOTS_COUNT):
				self.slots.append(self.GetChild("slot%d" % (i + 1)))
				self.statusSlots.append(self.GetChild("status%d" % (i + 1)))
				if i == 0:
					(x, y) = self.statusSlots[i].GetLocalPosition()
					self.savedY = y
				
				self.coordinatesSlots.append(self.GetChild("coordinates%d" % (i + 1)))
				self.mapNameSlots.append(self.GetChild("map%d" % (i + 1)))
				self.saveButtons.append(self.GetChild("save_btn%d" % (i + 1)))
				self.deleteButtons.append(self.GetChild("delete_btn%d" % (i + 1)))
				self.goButtons.append(self.GetChild("go_btn%d" % (i + 1)))
		except:
			import exception
			exception.Abort("SavePointWindow.LoadWindow.BindObject")
		
		self.board.SetCloseEvent(self.Close)
		for i in xrange(self.SLOTS_COUNT):
			self.saveButtons[i].SetEvent(ui.__mem_func__(self.OnPressButton), 1, i)
			self.deleteButtons[i].SetEvent(ui.__mem_func__(self.OnPressButton), 2, i)
			self.goButtons[i].SetEvent(ui.__mem_func__(self.OnPressButton), 3, i)
		if self.questionDialog != None:
			del self.questionDialog
		self.questionDialog = uicommon.QuestionDialog()
		self.questionDialog.SetCancelEvent(ui.__mem_func__(self.OnCancelQuestion))
		self.questionDialog.SetAcceptEvent(ui.__mem_func__(self.OnAcceptQuestion))
		self.questionDialog.act = -1
		self.questionDialog.arg = -1
		self.questionDialog.Hide()
		self.inputDialog = uicommon.InputDialog()
		self.inputDialog.SetMaxLength(8)
		self.inputDialog.SetAcceptEvent(ui.__mem_func__(self.OnAcceptDialog))
		self.inputDialog.SetCancelEvent(ui.__mem_func__(self.OnCloseDialog))
		self.inputDialog.SetNewAcceptText(localeinfo.LOCALIZATION_EDIT)
		self.inputDialog.SetNewCancelText(localeinfo.LOCALIZATION_CANCEL)
		self.inputDialog.SetTitle(localeinfo.LOCALIZATION_CHANGE_NAME)
		self.inputDialog.arg = -1
		self.inputDialog.Hide()
		self.isLoaded = 1

	def Destroy(self):
		self.ClearDictionary()
		self.isLoaded = 0
		self.board = None
		self.slots = []
		self.statusSlots = []
		self.coordinatesSlots = []
		self.mapNameSlots = []
		self.saveButtons = []
		self.deleteButtons = []
		self.goButtons = []
		self.slotDetails = {}
		if self.questionDialog != None:
			del self.questionDialog
		self.questionDialog = None
		if self.inputDialog != None:
			self.inputDialog.Close()
			del self.inputDialog
		self.inputDialog = None
		self.savedY = 0

	def OnCloseDialog(self):
		if self.inputDialog == None:
			return
		elif not self.inputDialog.IsShow():
			return
		
		self.inputDialog.arg = -1
		self.inputDialog.Hide()

	def OnAcceptDialog(self):
		if self.inputDialog == None:
			return
		
		text = self.inputDialog.GetText()
		net.SendChatPacket("/save_savepoint %d %s" % (self.inputDialog.arg, text))
		self.OnCloseDialog()
		return True

	def OnCancelQuestion(self):
		if self.questionDialog == None:
			return
		elif not self.questionDialog.IsShow():
			return
		
		self.questionDialog.act = -1
		self.questionDialog.arg = -1
		self.questionDialog.Close()

	def OnAcceptQuestion(self):
		if self.questionDialog == None:
			return
		
		act = self.questionDialog.act
		if act == 1:
			if self.inputDialog != None:
				if self.inputDialog.IsShow():
					self.inputDialog.SetTop()
				else:
					self.inputDialog.arg = self.questionDialog.arg
					self.inputDialog.EmptyText()
					self.inputDialog.Open()
		elif act == 2:
			net.SendChatPacket("/empty_savepoint %d" % (self.questionDialog.arg))
		elif act == 3:
			net.SendChatPacket("/go_savepoint %d" % (self.questionDialog.arg))
		
		self.OnCancelQuestion()

	def OnPressButton(self, act, arg):
		if self.questionDialog.IsShow():
			self.questionDialog.SetTop()
		
		if act == 1:
			self.questionDialog.SetText(localeinfo.SAVEPOINT_SAVEDIALOG)
		elif act == 2:
			self.questionDialog.SetText(localeinfo.SAVEPOINT_EMPTYITDIALOG)
		elif act == 3:
			self.questionDialog.SetText(localeinfo.SAVEPOINT_TELEPORTDIALOG)
		self.questionDialog.act = act
		self.questionDialog.arg = arg
		self.questionDialog.Open()

	def Append(self, id, name, mapIndex, x, y):
		if mapIndex == 0:
			name = localeinfo.SAVEPOINT_EMPTY
		
		id = int(id)
		mapIndex = int(mapIndex)
		x = int(x)
		y = int(y)
		self.slotDetails[id] = [name, mapIndex, x, y]

	def Update(self, id, name, mapIndex, x, y):
		id = int(id)
		mapIndex = int(mapIndex)
		x = int(x)
		y = int(y)
		if mapIndex == 0:
			self.slotDetails[id] = [name, mapIndex, x, y]
			try:
				self.saveButtons[id].Show()
				self.deleteButtons[id].Hide()
				self.goButtons[id].Hide()
				self.mapNameSlots[id].Hide()
				self.statusSlots[id].SetText(localeinfo.SAVEPOINT_EMPTY)
				self.mapNameSlots[id].Hide()
				(x, y) = self.statusSlots[id].GetLocalPosition()
				self.statusSlots[id].SetPosition(x, self.savedY)
			except Exception as e:
				import dbg
				dbg.TraceError("SavePoint::Update" + str(e))
		else:
			self.slotDetails[id] = [name, mapIndex, x, y]
			try:
				self.saveButtons[id].Hide()
				self.deleteButtons[id].Show()
				self.goButtons[id].Show()
				self.mapNameSlots[id].SetText(constinfo.MapNameByIndexSavePoint(mapIndex))
				(x, y) = self.mapNameSlots[id].GetLocalPosition()
				self.mapNameSlots[id].SetPosition(x, self.savedY)
				self.mapNameSlots[id].Show()
				self.statusSlots[id].SetText("[ " + name + " ]")
				(x, y) = self.statusSlots[id].GetLocalPosition()
				self.statusSlots[id].SetPosition(x, self.savedY - 20)
			except Exception as e:
				import dbg
				dbg.TraceError("SavePoint::Update" + str(e))

	def RequestOpen(self):
		if self.isLoaded == 0:
			return
		
		if self.IsShow():
			self.Close()
		else:
			if len(self.slotDetails[0]) > 0:
				self.Open()
			else:
				net.SendChatPacket("/open_savepoint")

	def Open(self):
		if self.isLoaded == 0:
			return
		
		for i in xrange(self.SLOTS_COUNT):
			mapIndex = self.slotDetails[i][1]
			if mapIndex == 0:
				self.saveButtons[i].Show()
				self.deleteButtons[i].Hide()
				self.goButtons[i].Hide()
				self.mapNameSlots[i].Hide()
				(x, y) = self.statusSlots[i].GetLocalPosition()
				self.statusSlots[i].SetPosition(x, self.savedY)
			else:
				self.saveButtons[i].Hide()
				self.deleteButtons[i].Show()
				self.goButtons[i].Show()
				self.mapNameSlots[i].SetText(constinfo.MapNameByIndexSavePoint(mapIndex))
				(x, y) = self.mapNameSlots[i].GetLocalPosition()
				self.mapNameSlots[i].SetPosition(x, self.savedY)
				self.mapNameSlots[i].Show()
				self.statusSlots[i].SetText("[ " + self.slotDetails[i][0] + " ]")
				(x, y) = self.statusSlots[i].GetLocalPosition()
				self.statusSlots[i].SetPosition(x, self.savedY - 20)
		
		self.Show()
		self.SetCenterPosition()

	def OnUpdate(self):
		if self.IsShow():
			for i in xrange(self.SLOTS_COUNT):
				if self.slotDetails[i][1] == 0:
					(x, y, z) = player.GetMainCharacterPosition()
					self.coordinatesSlots[i].SetText("[ " + str(int(x / 100)) + ", " + str(int(y / 100)) + " ]")
				else:
					self.coordinatesSlots[i].SetText("[ " + str(self.slotDetails[i][2]) + ", " + str(self.slotDetails[i][3]) + " ]")

	def Close(self):
		self.OnCancelQuestion()
		self.OnCloseDialog()
		self.Hide()

	def OnPressEscapeKey(self):
		self.Close()
		return True

