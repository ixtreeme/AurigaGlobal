import os
import ui
import player
import mousemodule
import net
import app
import snd
import item
import chat
import grp
import uiscriptlocale
import constinfo
import ime
import wndMgr
import petskill
import uipetfeed
import uipetsystem
import uitooltip
#from _weakref import proxy
import localeinfo

if app.ENABLE_NEW_PET_EDITS:
	import uipetevownd
	import uicommon

def checkdiv(n):
	x = str(n/10.0)
	if len(x) > 3:
		return str(x)[0:3]
	return str(x)

def pointop(n):
	t = int(n)
	if t / 10 < 1:
		return "0."+n
	else:		
		return n[0:len(n)-1]+"."+n[len(n)-1:]

class PetSystemMain(ui.ScriptWindow):
	if app.ENABLE_NEW_PET_EDITS:
		wndEvoWnd = None
		questionDlg = None
	
	class TextToolTip(ui.Window):
		def __init__(self, y):
			ui.Window.__init__(self, "TOP_MOST")

			textLine = ui.TextLine()
			textLine.SetParent(self)
			textLine.SetHorizontalAlignLeft()
			textLine.SetOutline()
			textLine.Show()
			self.y = y
			self.textLine = textLine

		def __del__(self):
			ui.Window.__del__(self)

		def SetText(self, text):
			self.textLine.SetText(text)

		def OnRender(self):
			(mouseX, mouseY) = wndMgr.GetMousePosition()
			self.textLine.SetPosition(mouseX, mouseY - 60 + self.y)

	def __init__(self, vnum = 0):
		ui.ScriptWindow.__init__(self)
		self.vnum = vnum
		self.toolTip = None
		self.itemToolTip = uitooltip.ItemToolTip()
		self.itemToolTip.Hide()
		if app.ENABLE_NEW_PET_EDITS:
			self.realAge = 0
			self.minAge = 0
			self.skillList = []
		
		self.__LoadWindow()
		

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def Show(self):
		ui.ScriptWindow.Show(self)

	def Close(self):
		if app.ENABLE_NEW_PET_EDITS and self.wndEvoWnd and self.wndEvoWnd.IsShow():
			self.wndEvoWnd.CancelRefine()
		
		self.Hide()
		self.feedwind.Close()
		constinfo.PETGUI = 0
		if self.toolTip and self.toolTip.IsShow():
			self.toolTip.ClearToolTip()
			self.toolTip.Hide()
		for item in self.tooltipexp:
			if item.IsShow():
				item.Hide()

	def __LoadWindow(self):
		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "uiscript/petinformationwindow.py")
		except:
			import exception
			exception.Abort("PetInformationWindow.LoadWindow.LoadObject")
			
		try:
			self.feedwind = uipetfeed.PetFeedWindow()
			self.board = self.GetChild("board")
			self.boardclose = self.GetChild("CloseButton")
			
			self.slotimgpet = self.GetChild("UpBringing_Pet_Slot")
			self.evolname = self.GetChild("EvolName")
			self.petname = self.GetChild("PetName")
			
			self.expwind = self.GetChild("UpBringing_Pet_EXP_Gauge_Board")
			self.tooltipexp = []
			if not app.ENABLE_NEW_PET_EDITS:
				for i in range(0,4):
					self.tooltipexp.append(self.TextToolTip(15*i))
					self.tooltipexp[i].Hide()
			else:
				for i in range(0,2):
					self.tooltipexp.append(self.TextToolTip(15*(i+2)))
					self.tooltipexp[i].Hide()
			
			self.petlifeg = self.GetChild("LifeGauge")
			
			self.petlevel = self.GetChild("LevelValue")
			self.petexpa = self.GetChild("UpBringing_Pet_EXPGauge_01")
			self.petexpb = self.GetChild("UpBringing_Pet_EXPGauge_02")
			self.petexpc = self.GetChild("UpBringing_Pet_EXPGauge_03")
			self.petexpd = self.GetChild("UpBringing_Pet_EXPGauge_04")
			if not app.ENABLE_NEW_PET_EDITS:
				self.petexpe = self.GetChild("UpBringing_Pet_EXPGauge_05")
			
			self.petexppages = []
			self.petexppages.append(self.petexpa)
			self.petexppages.append(self.petexpb)
			self.petexppages.append(self.petexpc)
			self.petexppages.append(self.petexpd)
			if not app.ENABLE_NEW_PET_EDITS:
				self.petexppages.append(self.petexpe)
			
			for exp in self.petexppages:
				exp.SetSize(0, 0)
				#exp.Hide()
				
			
			
			
			
			self.petages = self.GetChild("AgeValue")
			
			self.petdur = self.GetChild("LifeTextValue")
			
			#gaugehp
			
			self.sviluppobtn = self.GetChild("FeedEvolButton")
			self.itemexp = self.GetChild("FeedExpButton")
			self.itemexp.Down()
			
			self.pethp = self.GetChild("HpValue")
			self.petdef = self.GetChild("DefValue")
			self.petsp = self.GetChild("SpValue")
			
			self.petskill0 = self.GetChild("PetSkillSlot0")
			for i in xrange(4):
				self.petskill0.SetPetSkillSlotEmpty(i)
			
			self.petskill0.SetSelectItemSlotEvent(ui.__mem_func__(self.UseSkill))
			self.petskill0.SetUseSlotEvent(ui.__mem_func__(self.UseSkill))
			self.petskill0.SetOverInItemEvent(ui.__mem_func__(self.PetSkillTooltipShow))
			self.petskill0.SetOverOutItemEvent(ui.__mem_func__(self.PetSkillTooltipHide))	
			if app.ENABLE_NEW_PET_EDITS:
				self.petskill0.SetSelectEmptySlotEvent(ui.__mem_func__(self.SelectEmptySlot))
				self.petskill0.SetSelectItemSlotEvent(ui.__mem_func__(self.SelectEmptySlot))
			
			self.SetDefaultInfo()
			
			self.arrytooltip = [ [-1,-1], [-1,-1], [-1,-1], [-1,-1] ]
			PET_FILE_NAME = "%s/pet_skill.txt" % app.GetLocalePath()
			PET_FILE_SKILL = "data/common/pet_skill_bonus.txt"
			self.linespet = open(PET_FILE_NAME, "r").readlines()
			self.linespetskill = open(PET_FILE_SKILL, "r").readlines()
			self.SkillTooltip = uitooltip.ToolTip(180)
			
			
			#Event
			self.boardclose.SetEvent(ui.__mem_func__(self.Close))
			if not app.ENABLE_NEW_PET_EDITS:
				self.itemexp.SetToggleDownEvent(lambda arg=0,arg1=3: self.OpenFeedBox(arg,arg1))
				self.itemexp.SetToggleUpEvent(lambda arg=1,arg1=0: self.OpenFeedBox(arg,arg1))
			else:
				self.toolTip = uitooltip.ItemToolTip()
				self.toolTip.ClearToolTip()
				self.itemexp.ShowToolTip = lambda : self.OverInButton()
				self.itemexp.HideToolTip = lambda : self.OverOutButton()
			
			self.sviluppobtn.SetEvent(ui.__mem_func__(self.evolution))
		except:
			import exception
			exception.Abort("PetInformationWindow.LoadWindow.BindObject")

	if app.ENABLE_NEW_PET_EDITS:
		def SelectEmptySlot(self, selectedSlotPos):
			if self.skillList[selectedSlotPos] != 0:
				return
			
			if not mousemodule.mouseController.isAttached():
				return
			
			attachedSlotType = mousemodule.mouseController.GetAttachedType()
			attachedSlotPos = mousemodule.mouseController.GetAttachedSlotNumber()
			attachedItemCount = mousemodule.mouseController.GetAttachedItemCount()
			attachedItemIndex = mousemodule.mouseController.GetAttachedItemIndex()
			if app.ENABLE_EXTRA_INVENTORY:
				if player.SLOT_TYPE_EXTRA_INVENTORY != attachedSlotType:
					mousemodule.mouseController.DeattachObject()
					return
			else:
				if player.SLOT_TYPE_INVENTORY != attachedSlotType:
					mousemodule.mouseController.DeattachObject()
					return
			
			item.SelectItem(attachedItemIndex)
			if item.GetItemType() != item.ITEM_TYPE_PET:
				mousemodule.mouseController.DeattachObject()
				return
			
			if self.questionDlg != None:
				self.questionDlg.OnCloseDlg()
			
			self.questionDlg = uicommon.QuestionDialog()
			self.questionDlg.SetText(localeinfo.PET_ASK_LEARN)
			self.questionDlg.SetAcceptEvent(ui.__mem_func__(self.OnAcceptDlg))
			self.questionDlg.SetCancelEvent(ui.__mem_func__(self.OnCloseDlg))
			self.questionDlg.Open()
			self.questionDlg.src = selectedSlotPos
			self.questionDlg.dst = attachedSlotPos
			
			mousemodule.mouseController.DeattachObject()

		def OnAcceptDlg(self):
			net.SendChatPacket("/petincskill %d %d" % (self.questionDlg.src, self.questionDlg.dst))
			self.OnCloseDlg()

		def OnCloseDlg(self):
			if not self.questionDlg:
				return
			
			self.questionDlg.Close()
			self.questionDlg = None

	if app.ENABLE_NEW_PET_EDITS:
		def SetAge(self, age):
			self.realAge = age
			self.minAge = app.GetGlobalTimeStamp() - age

	def OverInButton(self):
		if self.toolTip:
			self.toolTip.ClearToolTip()
			if self.minAge == 0:
				self.toolTip.Hide()
			else:
				self.toolTip.AppendPetBonus(self.minAge)
				self.toolTip.Show()

	def OverOutButton(self):
		if self.toolTip:
			self.toolTip.Hide()

	def PetSkillTooltipShow(self, slot):
		if self.arrytooltip[slot][0] > 0:
			tokens = self.linespet[self.arrytooltip[slot][0]-1][:-1].split("\t")
			tokens2 = self.linespetskill[self.arrytooltip[slot][0]-1][:-1].split("\t")
			self.SkillTooltip.ClearToolTip()
			self.SkillTooltip.AutoAppendTextLine(tokens[1], grp.GenerateColor(0.9490, 0.9058, 0.7568, 1.0))
			self.SkillTooltip.AppendDescription(tokens[4], 26)
			self.SkillTooltip.AppendSpace(5)
			if self.itemToolTip:
				self.SkillTooltip.AutoAppendTextLine(self.itemToolTip.__GetAffectString(int(tokens2[1]), int(tokens2[self.arrytooltip[slot][1]+1])))
			
			try:
				if int(tokens[5]) > 0:
					self.SkillTooltip.AutoAppendTextLine(localeinfo.PETSKILL_COOLDOWN % (str(tokens[5])), grp.GenerateColor(1.0, 0.7843, 0.0, 1.0))
			except:
				pass
			
			self.SkillTooltip.AlignHorizonalCenter()
			self.SkillTooltip.ShowToolTip()
		
		
	def PetSkillTooltipHide(self):
		self.SkillTooltip.HideToolTip()

	def evolution(self):
		if app.ENABLE_NEW_PET_EDITS:
			if constinfo.EVOLUTION == 0:
				chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.PET_CANNOT_BE_EVOLVED)
				return
			
			if not self.wndEvoWnd:
				self.wndEvoWnd = uipetevownd.Window()
			
			if self.wndEvoWnd.IsShow():
				self.wndEvoWnd.Close()
			else:
				self.wndEvoWnd.Open()
				self.wndEvoWnd.ClearMaterial()
				evo = 0
				if constinfo.EVOLUTION == 80:
					evo = 2
				elif constinfo.EVOLUTION == 60:
					evo = 1
				
				self.wndEvoWnd.AppendMaterial(55003 + evo, 10, player.EXTRA_INVENTORY)
				self.wndEvoWnd.AppendMaterial(27992 + evo, 10, player.EXTRA_INVENTORY)
				self.wndEvoWnd.AppendMaterial(86056 + evo, 3, player.EXTRA_INVENTORY)
		else:
			if mode == 0:
				net.SendChatPacket("/petvoincrease")

	def SetDefaultInfo(self):
		if app.ENABLE_NEW_PET_EDITS:
			constinfo.EVOLUTION = 0
			if self.wndEvoWnd and self.wndEvoWnd.IsShow():
				self.wndEvoWnd.CancelRefine()
		
		self.evolname.SetText("")
		self.petname.SetText("")
		
		self.petlevel.SetText("")
		self.petages.SetText("")
		self.petdur.SetText("")
		
		self.pethp.SetText("")
		self.petdef.SetText("")
		self.petsp.SetText("")
		
		self.SetDuration("0", "0")
		
		self.slotimgpet.ClearSlot(0)
		for i in xrange(4):
			self.petskill0.ClearSlot(i)
			self.petskill0.SetPetSkillSlotEmpty(i)
		
		self.SetExperience(0,0,0)
		self.arrytooltip = [ [-1,-1], [-1,-1], [-1,-1], [-1,-1] ]
		if app.ENABLE_NEW_PET_EDITS:
			self.realAge = 0
			self.minAge = 0
			self.skillList = [-1, -1, -1, -1]

	def OpenFeedBox(self, mode, btn):
		if constinfo.FEEDWIND == btn or constinfo.FEEDWIND == 0:
			if mode == 0:
				self.feedwind.Show()
				constinfo.FEEDWIND = btn
			else:
				self.feedwind.Close()
				constinfo.FEEDWIND = 0

		else:
			self.feedwind.Close()
			constinfo.FEEDWIND = 0

	def SetImageSlot(self, vnum):
		self.slotimgpet.SetItemSlot(0, int(vnum), 0)
		self.slotimgpet.SetAlwaysRenderCoverButton(0, True)

	def SetEvolveName(self, name):
		self.evolname.SetText(name)
	
	def SetName(self, name):
		self.petname.SetText(name)

	def SetLevel(self, level):
		if int(level) == 40 or int(level) == 60 or int(level) == 80:
			constinfo.EVOLUTION = int(level)
		else:
			constinfo.EVOLUTION = 0
		self.petlevel.SetText(level)

	def SetAges(self, ages):
		self.petages.SetText(localeinfo.SecondToDHM(ages))

	def SetDuration(self, dur, durt):
		dur1 = int(dur)/60
		durt1 = int(durt)/60
		tmpage = int((int(durt)/60 -int(dur) /60)/24)
		
		if int(dur) > 0:
			self.petlifeg.SetPercentage(min(float(dur), float(durt)), float(durt))
		else:
			self.petlifeg.SetPercentage(0.0, 1.0)
		
		if app.ENABLE_NEW_PET_EDITS:
			if int(durt1*60) > 525600:
				self.petdur.SetText(localeinfo.PET_TIME_TOOLTIP_INF)
			else:
				days = (int(dur)/60)/24
				hours = (int(dur) - (days*60*24)) / 60
				mins = int(dur) - (days*60*24) - (hours*60)
				self.petdur.SetText(localeinfo.PET_TIME_GUI  % (days, hours, mins))
		else:
			self.petdur.SetText(localeinfo.PET_TIME_GUI_OLD % (str(dur1), str(durt1)))
		
		if not app.ENABLE_NEW_PET_EDITS:
			self.SetAges(str(tmpage)+"giorni")

	def SetHp(self, hp):
		self.pethp.SetText(pointop(hp)+"%")
		
	def SetDef(self, deff):
		self.petdef.SetText(pointop(deff)+"%")
		
	def SetSp(self, sp):
		self.petsp.SetText(pointop(sp)+"%")

	def SetSkill(self, slot, idx, lv):
		if app.ENABLE_NEW_PET_EDITS:
			self.skillList[int(slot)] = int(idx)
		
		if int(idx) != -1:
			self.petskill0.ClearSlot(int(slot))
			self.petskill0.SetPetSkillSlot(int(slot), int(idx), int(lv))
			self.petskill0.SetCoverButton(int(slot))
			self.petskill0.SetAlwaysRenderCoverButton(int(slot), True)
			self.arrytooltip[int(slot)][0] = int(idx)
			self.arrytooltip[int(slot)][1] = int(lv)

	def SetExperience(self, expm, expi, exptot):
		expm = int(expm)
		expi = int(expi)
		exptot = int(exptot)
		
		if exptot > 0:	
			totalexp = exptot
			totexpm = int( float(totalexp) / 100 * 90 )
			totexpi = totalexp - totexpm
			expi = min(expi, totexpi)
			expmp =  float(expm) / totexpm * 100
			expip =  float(expi) / totexpi * 100
		else:
			totalexp = 0
			totexpm = 0
			totexpi = 0
			expmp =  0
			expip =  0
			
		
		curPoint = int(min(expm, totexpm))
		curPoint = int(max(expm, 0))
		maxPoint = int(max(totexpm, 0))
		
		curPointi = int(min(expi, totexpi))
		curPointi = int(max(expi, 0))
		maxPointi = int(max(totexpi, 0))

		quarterPoint = maxPoint / 4
		quarterPointi = maxPointi 
		FullCount = 0
		FullCounti = 0

		if 0 != quarterPoint:
			FullCount = min(4, curPoint / quarterPoint)
			
		if 0 != quarterPointi:
			FullCounti = min(1, curPointi / quarterPointi)
		
		for i in xrange(4):
			self.petexppages[i].Hide()
		
		if not app.ENABLE_NEW_PET_EDITS:
			self.petexppages[4].Hide()

		for i in xrange(FullCount):
			self.petexppages[i].SetRenderingRect(0.0, 0.0, 0.0, 0.0)
			self.petexppages[i].Show()
		
		if not app.ENABLE_NEW_PET_EDITS:
			for i in xrange(FullCounti):
				self.petexppages[4].SetRenderingRect(0.0, 0.0, 0.0, 0.0)
				self.petexppages[4].Show()

		if 0 != quarterPoint:
			if FullCount < 4:
				Percentage = float(curPoint % quarterPoint) / quarterPoint - 1.0
				self.petexppages[FullCount].SetRenderingRect(0.0, Percentage, 0.0, 0.0)
				self.petexppages[FullCount].Show()
		
		if not app.ENABLE_NEW_PET_EDITS:
			if 0 != quarterPointi:
				if FullCounti < 1:
					Percentage = float(curPointi % quarterPointi) / quarterPointi - 1.0
					self.petexppages[4].SetRenderingRect(0.0, Percentage, 0.0, 0.0)
					self.petexppages[4].Show()
			
		#chat.AppendChat(chat.CHAT_TYPE_INFO, str(curPoint)+"-"+str(maxPoint)+"-"+str(FullCount)+"--"+str(quarterPoint))
		#####
		self.tooltipexp[0].SetText(localeinfo.PETSYSTEM_DIALOG1 % (expm, totexpm))
		self.tooltipexp[1].SetText(localeinfo.PETSYSTEM_DIALOG2 % expmp)
		if not app.ENABLE_NEW_PET_EDITS:
			self.tooltipexp[2].SetText(localeinfo.PETSYSTEM_DIALOG3 % (expi, totexpi))
			self.tooltipexp[3].SetText(localeinfo.PETSYSTEM_DIALOG4 % expip)
	
	def UseSkill(self, slot):
		#chat.AppendChat(chat.CHAT_TYPE_INFO, "+ --> "+str(slot))
		#chat.AppendChat(chat.CHAT_TYPE_INFO, "Skill: "+ str(petskill.GetSkillbySlot(slot)))
		net.SendChatPacket("/petskills "+str(slot))
	
	def OnUpdate(self):
		if self.realAge > 0:
			self.minAge = app.GetGlobalTimeStamp() - self.realAge
			self.SetAges(self.minAge)
		
		j = 4
		if app.ENABLE_NEW_PET_EDITS:
			j = 2
		
		if True == self.expwind.IsIn():
			for i in range(0, j):
				self.tooltipexp[i].Show()
		else:
			for i in range(0, j):
				self.tooltipexp[i].Hide()

	def OnPressEscapeKey(self):
		self.Close()
		return True

class PetSystemMini(ui.ScriptWindow):
	class TextToolTip(ui.Window):
		def __init__(self, y):
			ui.Window.__init__(self, "TOP_MOST")

			textLine = ui.TextLine()
			textLine.SetParent(self)
			textLine.SetHorizontalAlignLeft()
			textLine.SetOutline()
			textLine.Show()
			self.y = y
			self.textLine = textLine

		def __del__(self):
			ui.Window.__del__(self)

		def SetText(self, text):
			self.textLine.SetText(text)

		def OnRender(self):
			(mouseX, mouseY) = wndMgr.GetMousePosition()
			self.textLine.SetPosition(mouseX, mouseY - 60 + self.y)

	def __init__(self, vnum = 0):
		ui.ScriptWindow.__init__(self)
		self.toolTip = None
		self.game = 0
		self.vnum = vnum
		self.__LoadWindow()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def BindGameClass(self, game):
		#self.game = proxy(game)
		self.game = game

	def Show(self):
		ui.ScriptWindow.Show(self)

	def Close(self):
		self.Hide()

	def __LoadWindow(self):
		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "uiscript/petminiinformationwindow.py")
		except:
			import exception
			exception.Abort("PetMiniInformationWindow.LoadWindow.LoadObject")
		
		try:
			self.toolTip = uitooltip.ToolTip()
			
			self.expwind = self.GetChild("pet_mini_info_exp_gauge_board")
			self.mainbg = self.GetChild("main_bg")
			self.mainicon = self.GetChild("main_slot_img")
			
			self.main_slot_img = self.GetChild("pet_icon_slot")
			self.main_slot_img.SetSelectItemSlotEvent(ui.__mem_func__(self.OnPressSlot))
			self.main_slot_img.SetOverInItemEvent(ui.__mem_func__(self.OverInSlot))
			self.main_slot_img.SetOverOutItemEvent(ui.__mem_func__(self.OverOutSlot))
			self.tooltipexp = []
			if not app.ENABLE_NEW_PET_EDITS:
				for i in range(0,2):
					self.tooltipexp.append(self.TextToolTip(15*i))
					self.tooltipexp[i].Hide()
			else:
				for i in range(0,4):
					self.tooltipexp.append(self.TextToolTip(15*(i+2)))
					self.tooltipexp[i].Hide()
			
			self.pet_icon_slot_ani_img = self.GetChild("pet_icon_slot_ani_img")
			self.pet_mini_exp_01 = self.GetChild("pet_mini_EXPGauge_01")
			self.pet_mini_exp_02 = self.GetChild("pet_mini_EXPGauge_02")
			self.pet_mini_exp_03 = self.GetChild("pet_mini_EXPGauge_03")
			self.pet_mini_exp_04 = self.GetChild("pet_mini_EXPGauge_04")
			if not app.ENABLE_NEW_PET_EDITS:
				self.pet_mini_exp_05 = self.GetChild("pet_mini_EXPGauge_05")
			
			self.petmini_exp = []
			self.petmini_exp.append(self.pet_mini_exp_01)
			self.petmini_exp.append(self.pet_mini_exp_02)
			self.petmini_exp.append(self.pet_mini_exp_03)
			self.petmini_exp.append(self.pet_mini_exp_04)
			if not app.ENABLE_NEW_PET_EDITS:
				self.petmini_exp.append(self.pet_mini_exp_05)
			
			self.petlifeg = self.GetChild("LifeGauge")
			if constinfo.PETMINIEVO == 0 and constinfo.PETMINILEVEL == 40:
				self.pet_icon_slot_ani_img.Show()
			elif constinfo.PETMINIEVO == 1 and constinfo.PETMINILEVEL == 60:
				self.pet_icon_slot_ani_img.Show()
			elif constinfo.PETMINIEVO == 2 and constinfo.PETMINILEVEL == 80:
				self.pet_icon_slot_ani_img.Show()
			else:
				self.pet_icon_slot_ani_img.Hide()
			
			self.skillslot = self.GetChild("mini_skill_slot0")
			for i in xrange(4):
				self.skillslot.SetSlotScale(i, 0, 16, 16, petskill.GetEmptySkill(), 0.5, 0.5)
			
			self.skillslot.SetSelectItemSlotEvent(ui.__mem_func__(self.UseSkill))
			self.skillslot.SetUseSlotEvent(ui.__mem_func__(self.UseSkill))
			self.SetDefaultInfo()
			#self.mainbg.Show()
		except:
			import exception
			exception.Abort("PetMiniInformationWindow.LoadWindow.BindObject")

	def OnPressSlot(self):
		if self.game:
			petmain = self.game.petmain
			if petmain.IsShow():
				petmain.Close()
			else:
				petmain.Show()
				petmain.SetTop()

	def SetDefaultInfo(self):
		self.SetDuration("0", "0")
		self.main_slot_img.ClearSlot(0)
		for i in xrange(4):
			self.skillslot.ClearSlot(i)
			self.skillslot.SetSlotScale(i, 0, 16, 16, petskill.GetEmptySkill(), 0.5, 0.5)
		
		self.SetExperience(0,0,0)

	def SetImageSlot(self, vnum):
		self.main_slot_img.SetItemSlot(0, int(vnum), 0)
		self.main_slot_img.SetAlwaysRenderCoverButton(0, True)

	def SetDuration(self, dur, durt):
		if int(dur) > 0:
			self.petlifeg.SetPercentage(min(float(dur), float(durt)), float(durt))
		else:
			self.petlifeg.SetPercentage(0.0, 1.0)

	def SetSkill(self, slot, idx, lv):
		if int(idx) != -1:
			self.skillslot.ClearSlot(int(slot))
			self.skillslot.SetPetSkillSlot(int(slot), int(idx), int(lv), 0.5, 0.5)
			self.skillslot.SetCoverButton(int(slot), "d:/ymir work/ui/pet/mini_window/pet_slot_corvermini.sub", "d:/ymir work/ui/pet/mini_window/pet_slot_corvermini.sub", "d:/ymir work/ui/pet/mini_window/pet_slot_corvermini.sub" , "d:/ymir work/ui/pet/mini_window/pet_slot_corvermini.sub")
			self.skillslot.SetAlwaysRenderCoverButton(int(slot), True)

	def SetExperience(self, expm, expi, exptot):
		expm = int(expm)
		expi = int(expi)
		exptot = int(exptot)
		
		if exptot > 0:	
			totalexp = exptot
			totexpm = int( float(totalexp) / 100 * 90 )
			totexpi = totalexp - totexpm
			expi = min(expi, totexpi)
			expmp =  float(expm) / totexpm * 100
			expip =  float(expi) / totexpi * 100
		else:
			totalexp = 0
			totexpm = 0
			totexpi = 0
			expmp =  0
			expip =  0
			
		
		curPoint = int(min(expm, totexpm))
		curPoint = int(max(expm, 0))
		maxPoint = int(max(totexpm, 0))
		
		curPointi = int(min(expi, totexpi))
		curPointi = int(max(expi, 0))
		maxPointi = int(max(totexpi, 0))

		quarterPoint = maxPoint / 4
		quarterPointi = maxPointi 
		FullCount = 0
		FullCounti = 0

		if 0 != quarterPoint:
			FullCount = min(4, curPoint / quarterPoint)
			
		if 0 != quarterPointi:
			FullCounti = min(1, curPointi / quarterPointi)

		for i in xrange(4):
			self.petmini_exp[i].Hide()
		
		if not app.ENABLE_NEW_PET_EDITS:
			self.petmini_exp[4].Hide()

		for i in xrange(FullCount):
			self.petmini_exp[i].SetRenderingRect(0.0, 0.0, 0.0, 0.0)
			self.petmini_exp[i].Show()
		
		if not app.ENABLE_NEW_PET_EDITS:
			for i in xrange(FullCounti):
				self.petmini_exp[4].SetRenderingRect(0.0, 0.0, 0.0, 0.0)
				self.petmini_exp[4].Show()

		if 0 != quarterPoint:
			if FullCount < 4:
				Percentage = float(curPoint % quarterPoint) / quarterPoint - 1.0
				self.petmini_exp[FullCount].SetRenderingRect(0.0, Percentage, 0.0, 0.0)
				self.petmini_exp[FullCount].Show()
		
		if not app.ENABLE_NEW_PET_EDITS:
			if 0 != quarterPointi:
				if FullCounti < 1:
					Percentage = float(curPointi % quarterPointi) / quarterPointi - 1.0
					self.petmini_exp[4].SetRenderingRect(0.0, Percentage, 0.0, 0.0)
					self.petmini_exp[4].Show()
			
		
		#####
		self.tooltipexp[0].SetText(localeinfo.PETSYSTEM_DIALOG1 % (expm, totexpm))
		self.tooltipexp[1].SetText(localeinfo.PETSYSTEM_DIALOG2 % expmp)
		if not app.ENABLE_NEW_PET_EDITS:
			self.tooltipexp[2].SetText(localeinfo.PETSYSTEM_DIALOG3 % (expi, totexpi))
			self.tooltipexp[3].SetText(localeinfo.PETSYSTEM_DIALOG4 % expip)
	
	def UseSkill(self, slot):
		net.SendChatPacket("/petskills "+str(slot))

	def OnUpdate(self):
		j = 4
		if app.ENABLE_NEW_PET_EDITS:
			j = 2
		
		if True == self.expwind.IsIn():
			for i in range(0, j):
				self.tooltipexp[i].Show()
		else:
			for i in range(0, j):
				self.tooltipexp[i].Hide()

	def OverInSlot(self):
		if self.toolTip:
			#text = localeinfo.PET_MINIWINDOW_OVER_SLOT
			#arglen = len(str(text))
			#self.toolTip.ClearToolTip()
			#self.toolTip.SetThinBoardSize(4 * arglen)
			#self.toolTip.SetToolTipPosition(pos_x + 50, pos_y + 50)
			#self.toolTip.AppendTextLine(self.text)
			self.toolTip.ClearToolTip()
			self.toolTip.AppendDescriptionEqual(localeinfo.PET_MINIWINDOW_OVER_SLOT)
			self.toolTip.Show()

	def OverOutSlot(self):
		if self.toolTip:
			self.toolTip.Hide()

