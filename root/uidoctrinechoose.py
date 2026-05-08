import app
import net
import player
import chat
import ui, uicommon, localeinfo, mousemodule, playersettingmodule

JOB_NAME_DICT = {
					0 : [localeinfo.SKILL_SELECT_1A, localeinfo.SKILL_SELECT_1B],
					1 : [localeinfo.SKILL_SELECT_2A, localeinfo.SKILL_SELECT_2B],
					2 : [localeinfo.SKILL_SELECT_3A, localeinfo.SKILL_SELECT_3B],
					3 : [localeinfo.SKILL_SELECT_4A, localeinfo.SKILL_SELECT_4B],
}

class DoctrineWindow(ui.ScriptWindow):
	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.Initialize()
		self.LoadWindow()

	def __del__(self):
		self.ClearDictionary()
		self.Initialize()
		ui.ScriptWindow.__del__(self)

	def Initialize(self):
		self.selectedJob = 1
		self.isLoaded = 0
		self.titlebar = None
		self.leftButton = None
		self.leftChildButton = None
		self.rightButton = None
		self.rightChildButton = None
		self.selectButton = None
		self.jobLeftName = None
		self.jobRightName = None
		self.leftSkillSlot = None
		self.rightSkillSlot = None
		self.toolTipSkill = None
		self.popupDialog = None
		self.questionDialog = None

	def LoadWindow(self):
		if self.isLoaded == 1:
			return
		
		try:
			PythonScriptLoader = ui.PythonScriptLoader()
			PythonScriptLoader.LoadScriptFile(self, "uiscript/doctrinechoosewindow.py")
		except:
			import exception
			exception.Abort("DoctrineWindow.LoadWindow.LoadObject")
		
		try:
			self.titlebar = self.GetChild("TitleBar")
			self.leftButton = self.GetChild("left_button")
			self.rightButton = self.GetChild("right_button")
			self.leftChildButton = self.GetChild("left_child_button")
			self.rightChildButton = self.GetChild("right_child_button")
			self.selectButton = self.GetChild("SelectButton")
			self.jobLeftName = self.GetChild("JobLeftName")
			self.jobRightName = self.GetChild("JobRightName")
			self.leftSkillSlot = self.GetChild("left_skill_slot")
			self.rightSkillSlot = self.GetChild("right_skill_slot")
		except:
			import exception
			exception.Abort("DoctrineWindow.LoadWindow.BindObject")
		
		self.titlebar.SetCloseEvent(ui.__mem_func__(self.Close))
		
		self.leftButton.SetEvent(lambda arg = 1 : self.RefreshButtons(arg))
		self.rightButton.SetEvent(lambda arg = 2 : self.RefreshButtons(arg))
		self.leftChildButton.SetEvent(lambda arg = 1 : self.RefreshButtons(arg))
		self.rightChildButton.SetEvent(lambda arg = 2 : self.RefreshButtons(arg))
		self.selectButton.SetEvent(ui.__mem_func__(self.SelectJob))
		self.jobLeftName.SetText(JOB_NAME_DICT[self.GetRealRace()][0])
		self.jobRightName.SetText(JOB_NAME_DICT[self.GetRealRace()][1])
		
		self.leftButton.SetUpVisual("d:/ymir work/ui/doctrinechoose/select_job/%s/left_normal.tga" % self.GetRealRace())
		self.leftButton.SetOverVisual("d:/ymir work/ui/doctrinechoose/select_job/%s/left_hover.tga" % self.GetRealRace())
		self.leftButton.SetDownVisual("d:/ymir work/ui/doctrinechoose/select_job/%s/left_down.tga" % self.GetRealRace())
		self.rightButton.SetUpVisual("d:/ymir work/ui/doctrinechoose/select_job/%s/right_normal.tga" % self.GetRealRace())
		self.rightButton.SetOverVisual("d:/ymir work/ui/doctrinechoose/select_job/%s/right_hover.tga" % self.GetRealRace())
		self.rightButton.SetDownVisual("d:/ymir work/ui/doctrinechoose/select_job/%s/right_down.tga" % self.GetRealRace())
		
		self.leftSkillSlot.SetOverInItemEvent(ui.__mem_func__(self.OverInSlotLeft))
		self.leftSkillSlot.SetOverOutItemEvent(ui.__mem_func__(self.OverOutSlot))
		self.rightSkillSlot.SetOverInItemEvent(ui.__mem_func__(self.OverInSlotRight))
		self.rightSkillSlot.SetOverOutItemEvent(ui.__mem_func__(self.OverOutSlot))
		
		for i in xrange(6):
			self.leftSkillSlot.SetSkillSlotNew(i, self.GetSkillIndex() + i, 0, 0)
			self.rightSkillSlot.SetSkillSlotNew(i, self.GetSkillIndex() + i + 15, 0, 0)
		
		self.popupDialog = uicommon.PopupDialog()
		self.popupDialog.SetText(localeinfo.SKILL_SELECT_DIALOG1)
		self.popupDialog.Close()
		
		self.questionDialog = uicommon.QuestionDialog()
		self.questionDialog.SetAcceptEvent(ui.__mem_func__(self.OnSelectJob))
		self.questionDialog.SetCancelEvent(ui.__mem_func__(self.OnCloseQuestionDialog))
		self.questionDialog.Close()
		
		self.RefreshButtons(1)
		
		self.isLoaded = 1

	def OverInSlotLeft(self, index):
		if self.toolTipSkill:
			if mousemodule.mouseController.isAttached():
				return
			
			self.toolTipSkill.ClearToolTip()
			self.toolTipSkill.SetSkillOnlyName(index, self.GetSkillIndex() + index, 0)

	def OverInSlotRight(self, index):
		if self.toolTipSkill:
			if mousemodule.mouseController.isAttached():
				return
			
			self.toolTipSkill.ClearToolTip()
			self.toolTipSkill.SetSkillOnlyName(index, self.GetSkillIndex() + index + 15, 0)

	def OverOutSlot(self):
		if self.toolTipSkill:
			self.toolTipSkill.HideToolTip()

	def GetSkillIndex(self):
		return self.GetRealRace() * 30 + 1

	def GetRealRace(self):
		race = net.GetMainActorRace()
		if race >= 4:
			return race - 4
		else:
			return race

	def RefreshButtons(self, arg):
		self.questionDialog.SetText(localeinfo.SKILL_SELECT_DIALOG2 % JOB_NAME_DICT[self.GetRealRace()][arg - 1])
		if arg == 1:
			self.leftButton.Down()
			self.rightButton.SetUp()
			self.leftChildButton.Down()
			self.rightChildButton.SetUp()
			self.selectedJob = 1
		elif arg == 2:
			self.leftButton.SetUp()
			self.rightButton.Down()
			self.leftChildButton.SetUp()
			self.rightChildButton.Down()
			self.selectedJob = 2

	def OnCloseQuestionDialog(self):
		self.questionDialog.Close()

	def SelectJob(self):
		if self.selectedJob == 0:
			self.wndPopupDialog.Open()
		else:
			self.questionDialog.Open()

	def OnSelectJob(self):
		net.SendChatPacket("/doctrine_choose %d" % self.selectedJob)
		self.Close()
		self.OnCloseQuestionDialog()

	def Open(self):
		self.SetCenterPosition()
		self.SetTop()
		self.Show()

	def Close(self):
		self.OverOutSlot()
		self.Hide()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def RefreshSkill(self):
		if self.IsShow():
			group = net.GetMainActorSkillGroup()
			if group == 1 or group == 2:
				self.Close()

	def SetSkillToolTip(self, toolTip):
		self.toolTipSkill = toolTip

