import app
import net
import player
import ui
import wndMgr
import chat
import constinfo as pvp
import uiguild as uiPvP
import uicommon as message
import playersettingmodule as face
import uitooltip, localeinfo

def CalculateTimeLeft(iTime):
	A, B = divmod(iTime, 60)
	C, A = divmod(A, 60)
	return "%02d:%02d" % (A, B)	

mCheck = 0

FACE_IMAGE_DICT = {
	face.RACE_WARRIOR_M	: "icon/face/warrior_m.tga",
	face.RACE_WARRIOR_W	: "icon/face/warrior_w.tga",
	face.RACE_ASSASSIN_M	: "icon/face/assassin_m.tga",
	face.RACE_ASSASSIN_W	: "icon/face/assassin_w.tga",
	face.RACE_SURA_M		: "icon/face/sura_m.tga",
	face.RACE_SURA_W		: "icon/face/sura_w.tga",
	face.RACE_SHAMAN_M	: "icon/face/shaman_m.tga",
	face.RACE_SHAMAN_W	: "icon/face/shaman_w.tga",
	#face.RACE_WOLFMAN_M	: "icon/face/wolfman_m.tga"
}

BINARY_DUEL_GET_ = {
	"selected"  : {
		0 : 0,
		1 : 0,
		2 : 0,
		3 : 0,
		4 : 0,
		5 : 0,
		6 : 0,
		7 : 0,
	},
	"amount"  : {
		0 : 0
	}
}

class MiniWindowLiveInformations(ui.ScriptWindow):
	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.toolTip = None
		self.maximized = None
		self.slot = None
		self.face = None
		self.isLoaded = 0
		self.CreateDialog()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def CreateDialog(self):
		if self.isLoaded != 0:
			return
		
		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "uiscript/pvp_advanced_informations_mini.py")
			self.slot = self.GetChild("CharacterSlot")
			self.face = self.GetChild("CharacterIcon")
			self.toolTip = uitooltip.ToolTip()
		except:
			import exception
			exception.Abort("MiniWindowLiveInformations.LoadWindow.LoadObject")
		
		self.slot.SetOnMouseLeftButtonUpEvent(self.OnClickMaximize)
		self.slot.SetStringEvent("MOUSE_OVER_IN", ui.__mem_func__(self.OverInImg))
		self.slot.SetStringEvent("MOUSE_OVER_OUT", ui.__mem_func__(self.OverOutImg))
		self.face.SetOnMouseLeftButtonUpEvent(self.OnClickMaximize)
		self.face.SetStringEvent("MOUSE_OVER_IN", ui.__mem_func__(self.OverInImg))
		self.face.SetStringEvent("MOUSE_OVER_OUT", ui.__mem_func__(self.OverOutImg))
		self.isLoaded = 1

	def BindMaximized(self, maximized):
		self.maximized = maximized

	def OnClickMaximize(self):
		if not self.maximized:
			return
		
		self.Hide()
		self.maximized.Open()

	def UpdatePosition(self, miniPetIsShowing):
		y = wndMgr.GetScreenHeight()
		if miniPetIsShowing:
			self.SetPosition(6, y - 210)
		else:
			self.SetPosition(6, y - 160)

	def Open(self, race, miniPetIsShowing):
		if not self.maximized:
			return False
		
		self.face.LoadImage(FACE_IMAGE_DICT[race])
		self.Show()
		y = wndMgr.GetScreenHeight()
		if miniPetIsShowing:
			self.SetPosition(8, y - 210)
		else:
			self.SetPosition(8, y - 160)
		return True

	def OverInImg(self):
		if self.toolTip:
			self.toolTip.ClearToolTip()
			self.toolTip.AppendDescriptionEqual(localeinfo.DUEL_MINIWINDOW_OVER_SLOT)
			self.toolTip.Show()

	def OverOutImg(self):
		if self.toolTip:
			self.toolTip.Hide()

class WindowLiveInformations(ui.ScriptWindow):
	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.race = 0
		self.miniPetIsShowing = False
		self.minimizeBtn = None
		self.wndMiniLiveInformations = None
		self.isLoaded = 0
		self.CreateDialog()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def CreateDialog(self):
		if self.isLoaded != 0:
			return
		
		pyScrLoader = ui.PythonScriptLoader()
		pyScrLoader.LoadScriptFile(self, "uiscript/pvp_advanced_informations.py")
		getObject = self.GetChild

		self.wndBoard = getObject("Board")
		self.name = self.GetChild("NameCharacterValue")
		self.level = self.GetChild("LevelCharacterValue")
		self.face = getObject("CharacterIcon")
		self.MoneyText = getObject("Money")
		box_x = 10
		text_x = 67
		color = "|cFFff5148"
		self.checkBoxTable = {
								0 : [uiPvP.CheckBox(self, box_x, 75, lambda arg = 0: self.SetType(arg)), 0, self.SetText(text_x, 75, color + localeinfo.DUEL_BLOCK_CHANGE_ITEM)],
								1 : [uiPvP.CheckBox(self, box_x, 75+20, lambda arg = 1: self.SetType(arg)), 0, self.SetText(text_x, 75+20, color + localeinfo.DUEL_BLOCK_BUFF)],
								2 : [uiPvP.CheckBox(self, box_x, 75+20+20, lambda arg = 2: self.SetType(arg)), 0, self.SetText(text_x, 75+20+20, color + localeinfo.DUEL_BLOCK_POTION_RED)],
								3 : [uiPvP.CheckBox(self, box_x, 75+20+20+20, lambda arg = 3: self.SetType(arg)), 0, self.SetText(text_x, 75+20+20+20, color + localeinfo.DUEL_BLOCK_HORSE_MOUNT)],
								4 : [uiPvP.CheckBox(self, box_x+155, 75, lambda arg = 4: self.SetType(arg)), 0, self.SetText(text_x+155, 75, color + localeinfo.DUEL_BLOCK_PET)],
								5 : [uiPvP.CheckBox(self, box_x+155, 75+20, lambda arg = 5: self.SetType(arg)), 0, self.SetText(text_x+155, 75+20, color + localeinfo.DUEL_BLOCK_POLY)],
								6 : [uiPvP.CheckBox(self, box_x+155, 75+20+20, lambda arg = 6: self.SetType(arg)), 0, self.SetText(text_x+155, 75+20+20, color + localeinfo.DUEL_BLOCK_PARTY)],
								7 : [uiPvP.CheckBox(self, box_x+155, 75+20+20+20, lambda arg = 7: self.SetType(arg)), 0, self.SetText(text_x+155, 75+20+20+20, color + localeinfo.DUEL_BLOCK_EXCHANGE)]
		}
		
		self.wndMiniLiveInformations = MiniWindowLiveInformations()
		self.wndMiniLiveInformations.BindMaximized(self)
		self.minimizeBtn = getObject("minimizebutton")
		self.minimizeBtn.SetEvent(ui.__mem_func__(self.OnClickMinimize))
		
		self.isLoaded = 1

	def SetText(self, x, y, text):
		self.textLine = ui.TextLine()
		self.textLine.SetParent(self)
		self.textLine.SetPosition(x, y)
		self.textLine.SetText(text)
		self.textLine.Show()
		return self.textLine

	def ShowInformations(self, value, miniPetIsShowing):
		self.race = int(value[2])
		
		self.name.SetText((str(value[0])))
		self.level.SetText((str(value[1])))
		self.face.LoadImage(FACE_IMAGE_DICT[self.race])
		for i in xrange(8):
			self.checkBoxTable[i][0].SetCheck((int(value[i + 3])))
		
		self.MoneyText.SetText(localeinfo.NumberToMoneyString(str(value[11])))
		self.Show()
		self.SetTop()
		
		y = wndMgr.GetScreenHeight()
		if miniPetIsShowing:
			self.SetPosition(17, y - 350)
		else:
			self.SetPosition(17, y - 300)
		self.miniPetIsShowing = miniPetIsShowing

	def UpdatePosition(self, miniPetIsShowing):
		y = wndMgr.GetScreenHeight()
		if miniPetIsShowing:
			self.SetPosition(17, y - 350)
		else:
			self.SetPosition(17, y - 300)
		self.miniPetIsShowing = miniPetIsShowing
		if self.wndMiniLiveInformations:
			self.wndMiniLiveInformations.UpdatePosition(miniPetIsShowing)

	def Open(self):
		self.Show()
		self.SetTop()

	def OnClickMinimize(self):
		if not self.wndMiniLiveInformations:
			return
		
		if self.wndMiniLiveInformations.Open(self.race, self.miniPetIsShowing) == True:
			self.Hide()

	def OnPressEscapeKey(self):
		if self.wndMiniLiveInformations:
			if not self.wndMiniLiveInformations.IsShow():
				if self.wndMiniLiveInformations.Open(self.race, self.miniPetIsShowing) == True:
					self.Hide()
					return True

	def Close(self):
		if self.wndMiniLiveInformations:
			if self.wndMiniLiveInformations.IsShow():
				self.wndMiniLiveInformations.Hide()
		
		if self.IsShow():
			self.Hide()

class Initializate(ui.ScriptWindow):
	def __init__(self):
		ui.ScriptWindow.__init__(self)

		self.Name = ""
		self.Guild = ""
		self.Level = 0
		self.PlayTime = 0
		self.VID = 0
		self.acceptButton = 0
		self.declineButton = 0
		self.HP = 0
		self.SP = 0	
		self.Race = 0

		self.__CreateDialog()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def __CreateDialog(self):
		pyScrLoader = ui.PythonScriptLoader()
		pyScrLoader.LoadScriptFile(self, "uiscript/pvp_advanced.py")
		getObject = self.GetChild

		self.board = getObject("Board")
		self.board.SetCloseEvent(self.Close)
		self.acceptButton = getObject("AcceptButton")
		self.declineButton = getObject("DenyButton")

		self.name = self.GetChild("NameCharacterValue")
		self.guild = self.GetChild("GuildCharacterValue")
		self.level = self.GetChild("LevelCharacterValue")
		self.playtime = self.GetChild("PlayTimeCharacterValue")
		self.hp = self.GetChild("ValueHP")
		self.sp = self.GetChild("ValueMP")
		self.face = getObject("CharacterIcon")
		self.percent_hp = getObject("percent_hp")
		self.percent_sp = getObject("percent_sp")
		
		self.textLineTime = ui.TextLine()
		self.textLineTime.SetParent(self.board)
		self.textLineTime.SetPosition(408, 9)
		self.textLineTime.SetPackedFontColor(0xffdcf600)
		self.textLineTime.Hide()

		self.MoneyText = getObject("Money")
		self.MoneySlot = getObject("Money_Slot")
		self.MoneySlot.SetEvent(ui.__mem_func__(self.GetSlotMoney))

		self.checkBoxTable	=	{
									0	:	[uiPvP.CheckBox(self, 490, 50, lambda arg = 0: self.SetType(arg)), 0, self.SetText(377, 50, localeinfo.DUEL_BLOCK_CHANGE_ITEM)],
									1	:	[uiPvP.CheckBox(self, 490, 50+25, lambda arg = 1: self.SetType(arg)), 0, self.SetText(377, 50+25, localeinfo.DUEL_BLOCK_BUFF)],
									2	:	[uiPvP.CheckBox(self, 490, 50+25+25, lambda arg = 2: self.SetType(arg)), 0, self.SetText(377, 50+25+25, localeinfo.DUEL_BLOCK_POTION_RED)],
									3	:	[uiPvP.CheckBox(self, 490, 50+25+25+25, lambda arg = 3: self.SetType(arg)), 0, self.SetText(377, 50+25+25+25, localeinfo.DUEL_BLOCK_HORSE_MOUNT)],
									4	:	[uiPvP.CheckBox(self, 490, 50+25+25+25+25, lambda arg = 4: self.SetType(arg)), 0, self.SetText(377, 50+25+25+25+25, localeinfo.DUEL_BLOCK_PET)],
									5	:	[uiPvP.CheckBox(self, 490, 50+25+25+25+25+25, lambda arg = 5: self.SetType(arg)), 0, self.SetText(377, 50+25+25+25+25+25, localeinfo.DUEL_BLOCK_POLY)],
									6	:	[uiPvP.CheckBox(self, 490, 50+25+25+25+25+25+25, lambda arg = 6: self.SetType(arg)), 0, self.SetText(377, 50+25+25+25+25+25+25, localeinfo.DUEL_BLOCK_PARTY)],
									7	:	[uiPvP.CheckBox(self, 490, 50+25+25+25+25+25+25+25, lambda arg = 7: self.SetType(arg)), 0, self.SetText(377, 50+25+25+25+25+25+25+25, localeinfo.DUEL_BLOCK_EXCHANGE)]
								}

		if self.acceptButton:
			self.acceptButton.SetEvent(ui.__mem_func__(self.ApplyRequest))

		if self.declineButton:
			self.declineButton.SetEvent(ui.__mem_func__(self.Close))

		self.percent_hp.SetPercentage(100, 100)
		self.percent_sp.SetPercentage(100, 100)
		self.SetCenterPosition()

	def OpenDialog(self, VID, Name, Guild, Level, Race, PlayTime, HP, SP):
		self.VID   = int(VID)
		self.Name  = Name
		self.Guild  = Guild
		self.Level = str(Level)
		self.Race  = int(Race)
		self.PlayTime  = int(PlayTime)
		self.HP  = str(HP)
		self.SP  = str(SP)

		self.SetTitle(localeinfo.DUEL_TARGET_NAME + Name)
		self.name.SetText(Name)
		self.guild.SetText(self.Guild)
		self.level.SetText(self.Level)
		self.playtime.SetText(localeinfo.FormatTime(self.PlayTime))
		self.hp.SetText(self.HP)
		self.sp.SetText(self.SP)
		self.face.LoadImage(FACE_IMAGE_DICT[self.Race])

		self.Show()

	def Selected(self, value):
		for i in xrange(8):
			self.checkBoxTable[i][0].SetCheck((int(value[i])))		
		
		self.MoneyText.SetText(localeinfo.NumberToMoneyString(str(value[8])))

		global mCheck
		mCheck = 1
		
		self.textLineTime.Show()
		self.leftTime = app.GetGlobalTimeStamp() + 30

	def SetType(self, arg):
		global mCheck
		if mCheck == 1:
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.DUEL_VICTIM_ERROR)
			return
		else:
			for index in xrange(len(BINARY_DUEL_GET_["selected"])):
				if arg == index and mCheck != 1:
					if BINARY_DUEL_GET_["selected"][arg] == 1:
						self.checkBoxTable[arg][0].SetCheck(0)
						BINARY_DUEL_GET_["selected"][arg] = 0
					else:
						self.checkBoxTable[arg][0].SetCheck(1)
						BINARY_DUEL_GET_["selected"][arg] = 1

	def ApplyRequest(self):
		global mCheck
		if mCheck == 1:
			net.SendChatPacket("/pvp %d accept" % (self.VID))
		else:
			net.SendChatPacket("/pvp %d %d %d %d %d %d %d %d %d %d" % ((self.VID), (int(BINARY_DUEL_GET_["selected"][0])), (int(BINARY_DUEL_GET_["selected"][1])), (int(BINARY_DUEL_GET_["selected"][2])), (int(BINARY_DUEL_GET_["selected"][3])), (int(BINARY_DUEL_GET_["selected"][4])), (int(BINARY_DUEL_GET_["selected"][5])), (int(BINARY_DUEL_GET_["selected"][6])), (int(BINARY_DUEL_GET_["selected"][7])), (int(BINARY_DUEL_GET_["amount"][0]))))				

		self.Remove()
		pvp.DUEL_IS_SHOW_EQUIP = 1

	def Remove(self):
		global mCheck
		mCheck = 0

		for index in xrange(len(BINARY_DUEL_GET_["selected"])):
			self.checkBoxTable[index][0].SetCheck(0)
			BINARY_DUEL_GET_["selected"][index] = 0
			BINARY_DUEL_GET_["amount"][0] = 0

		self.Hide()
		self.MoneyText.SetText("")

	def SetText(self, x, y, text):
		self.textLine = ui.TextLine()
		self.textLine.SetParent(self)
		self.textLine.SetPosition(x, y)
		self.textLine.SetText(text)
		self.textLine.Show()

		return self.textLine

	def SetTitle(self, name):
		self.board.SetTitleName(name)

	def Close(self):
		net.SendChatPacket("/decline_pvp %d" % (self.VID))

		self.Remove()
		pvp.DUEL_IS_SHOW_EQUIP = 1

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def GetSlotMoney(self):
		global mCheck
		if mCheck == 1:
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.DUEL_VICTIM_ERROR)
			return
		else:
			self.inputDialog = message.Duel_InputMoneyBet()
			self.inputDialog.SetMaxLength(9)
			self.inputDialog.SetAcceptEvent(lambda arg = True: self.SetMoney())
			self.inputDialog.SetCancelEvent(ui.__mem_func__(self.Cancel))
			self.inputDialog.SetAcceptText(localeinfo.DUEL_MONEY_ADD)
			self.inputDialog.SetCancelText(localeinfo.DUEL_MONEY_CANCEL)
			self.inputDialog.SetTitle(localeinfo.DUEL_MONEY_TITLE)
			self.inputDialog.SetDescription(localeinfo.DUEL_MONEY_CURRENT + localeinfo.NumberToMoneyString(str(player.GetElk())))
			self.inputDialog.Open()

	def SetMoney(self):

		if not self.inputDialog:
			return True

		updateMoney = self.inputDialog.GetText()
		currentMoney = player.GetElk()

		if not updateMoney:
			return True

		if not updateMoney.isdigit():
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.DUEL_MONEY_ERROR_1)
			return True

		if int(updateMoney) <= 0:
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.DUEL_MONEY_ERROR_2)
			return True

		if len(updateMoney) < 0:
			return True

		if updateMoney < 0:
			return True

		global mCheck
		if updateMoney.isdigit() and mCheck != 1:
			if int(updateMoney) <= int(currentMoney):
				self.MoneyText.SetText(localeinfo.NumberToMoneyString(str(updateMoney)))
				BINARY_DUEL_GET_["amount"][0] = (int(updateMoney))
				self.inputDialog.Hide()
			else:
				chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.DUEL_MONEY_ERROR_3)
				return True

	def Cancel(self):
		self.inputDialog.Hide()

	def SetTime(self, iLeft):
		leftTime = iLeft - app.GetGlobalTimeStamp()
		
		if leftTime <= 0:
			leftTime = 0
			self.Close()

		self.textLineTime.SetText(localeinfo.PVP_DUEL_TIME_REMAINING % (CalculateTimeLeft(leftTime)))

	def OnUpdate(self):
		global mCheck
		if mCheck == 1:
			self.SetTime(int(self.leftTime))