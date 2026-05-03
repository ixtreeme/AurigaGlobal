import app
import net
import ui
import wndMgr
import localeinfo
import time
import uiCommon
import chat
import background
import constinfo

MAX_CHARACTERS = 8
MAX_SKILLGROUP = 2

constinfo.BATTLE_ROYALE_MAP_NAME = "metin2_map_battle_royale"

PHASE_DISABLED = 0
PHASE_WAITING = 1
PHASE_WARPING = 2
PHASE_RUNNING = 3
PHASE_LAST_ROUND = 4

global_t = (0,24,60)
def CalculateTimeLeft(iTime):
	if iTime <= 0:
		return "00:00:00"
		
	A, B = divmod(iTime, global_t[2])
	C, A = divmod(A, global_t[2])
	return "%02d:%02d:%02d" % (C, A, B)
	
	
class BattleRoyaleSelectChar(ui.ScriptWindow):

	def __init__(self): 
		self.isLoaded = 0
		
		self.board = None
		self.confirmButton = None
		self.cancelButton = None
		self.charButtonList = []
		self.skillButtonList = []
		self.currentSkillGroup = 0
		self.currentCharacter = 0
		
		ui.ScriptWindow.__init__(self)
		self.__LoadWindow()

	def __LoadWindow(self):
		if self.isLoaded == 1:
			return

		self.isLoaded = 1

		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "UIScript/battleroyale_selectchar.py")
		except:
			import exception
			exception.Abort("BattleRoyaleSelectChar.__LoadWindow error")
			
		try:
			self.board = self.GetChild("board")
			self.board.SetCloseEvent(self.Hide)
			
			self.confirmButton = self.GetChild("confirm")
			self.confirmButton.SetEvent(ui.__mem_func__(self.ClickConfirm))
			
			self.cancelButton = self.GetChild("cancel")
			self.cancelButton.SetEvent(ui.__mem_func__(self.Hide))
			
			#Character Buttons
			self.charButtonList = []
			for i in xrange(MAX_CHARACTERS):
				self.charButtonList.append(self.GetChild("CharButton%d" % (i+1)))
				self.charButtonList[i].SetEvent(lambda arg=i: self.__OnSelectCharacter(arg))
				
			self.charButtonList[0].Down()
			#Character Buttons
			
			#Skill Group Buttons
			self.skillButtonList = []
			for i in xrange(MAX_SKILLGROUP):
				self.skillButtonList.append(self.GetChild("skillGroup%d" % (i+1)))
				self.skillButtonList[i].SetEvent(lambda arg=i: self.__OnSelectSkillGroup(arg))
				
			self.skillButtonList[0].Down()
			self.SetSkillGroupNames(0)
			#Skill Group Buttons
			
		except:
			import exception
			exception.Abort("BattleRoyaleSelectChar.LoadWindow.BindObject")
		
	def __OnSelectCharacter(self, charID):
		self.currentCharacter = charID

		for i in xrange(MAX_CHARACTERS):
			if i!=charID:
				self.charButtonList[i].SetUp()
				
		self.charButtonList[charID].Down()
		self.SetSkillGroupNames(charID)
		
	def SetSkillGroupNames(self, charID):
		if charID == 0 or charID == 1: #Warrior
			self.skillButtonList[0].SetText(localeinfo.JOB_WARRIOR1)
			self.skillButtonList[1].SetText(localeinfo.JOB_WARRIOR2)
		elif charID == 2 or charID == 3: #Sura
			self.skillButtonList[0].SetText(localeinfo.JOB_SURA1)
			self.skillButtonList[1].SetText(localeinfo.JOB_SURA2)
		elif charID == 4 or charID == 5: #Ninja
			self.skillButtonList[0].SetText(localeinfo.JOB_ASSASSIN1)
			self.skillButtonList[1].SetText(localeinfo.JOB_ASSASSIN2)
		elif charID == 6 or charID == 7: #Shaman
			self.skillButtonList[0].SetText(localeinfo.JOB_SHAMAN1)
			self.skillButtonList[1].SetText(localeinfo.JOB_SHAMAN2)
		
	def __OnSelectSkillGroup(self, skillID):
		self.currentSkillGroup = skillID

		for i in xrange(MAX_SKILLGROUP):
			if i!=skillID:
				self.skillButtonList[i].SetUp()
				
		self.skillButtonList[skillID].Down()
		
	def OnPressEscapeKey(self):
		self.Hide()
		return True
		
	def Show(self):
		ui.ScriptWindow.Show(self)
		
	def CharIDToRace(self, charID):
		# MAIN_RACE_WARRIOR_M, 	0
		# MAIN_RACE_ASSASSIN_W,	1
		# MAIN_RACE_SURA_M,		2
		# MAIN_RACE_SHAMAN_W,	3
		# MAIN_RACE_WARRIOR_W,	4
		# MAIN_RACE_ASSASSIN_M,	5
		# MAIN_RACE_SURA_W,		6
		# MAIN_RACE_SHAMAN_M,	7
		# MAIN_RACE_WOLFMAN_M,	8

		Races = {
			0 : 0,
			1 : 4,
			2 : 2,
			3 : 6,
			4 : 5,
			5 : 1,
			6 : 7,
			7 : 3,
		}
		
		return Races[charID]
		
	def ClickConfirm(self):
		self.Hide()
		
		race = self.CharIDToRace(self.currentCharacter)
		skillGroup = (self.currentSkillGroup+1) * 10
		resultChar = race + skillGroup
		net.SendChatPacket("/join_battle_royale " + str(resultChar))

	def Hide(self):
		ui.ScriptWindow.Hide(self)
		
	def Destroy(self):
		self.ClearDictionary()

		self.board = None
		self.confirmButton = None
		self.cancelButton = None
		self.charButtonList = []
		self.skillButtonList = []
		self.currentSkillGroup = 0
		self.currentCharacter = 0

	def __del__(self): 
		ui.ScriptWindow.__del__(self) 


class BattleRoyaleInfo(ui.ScriptWindow):

	def __init__(self): 
		self.isLoaded = 0
		
		self.endTime = app.GetTime() + 60*60*24*5
		self.refreshTerrain = 0.0
		self.remainingTime = None
		self.teleportButton = None
		self.selectChar = None
		self.phase = PHASE_DISABLED
		
		ui.ScriptWindow.__init__(self)
		self.__LoadWindow()

	def __LoadWindow(self):
		if self.isLoaded == 1:
			return

		self.isLoaded = 1

		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "UIScript/battleroyale_info.py")
		except:
			import exception
			exception.Abort("BattleRoyaleInfo.__LoadWindow error")
			
		try:
			self.board = self.GetChild("board")
			self.remainingTime = self.GetChild("remaining")
			self.infoText = self.GetChild("infoText")
			self.teleportButton = self.GetChild("JoinButton")
			self.teleportButton.SetEvent(ui.__mem_func__(self.ClickTeleport))
			
			self.toggleTerrainZoneBtn = self.GetChild("toggleTerrainZoneArea")
			self.toggleTerrainZoneBtnText = self.GetChild("toggleTerrainZoneAreaText")
			self.toggleTerrainZoneBtn.SetEvent(ui.__mem_func__(self.ClickToggleTerrainZone))
			self.toggleTerrainZoneBtn.Hide()
			
			self.lineSeparator = self.GetChild("lineSeparator1")
			self.yourKills = self.GetChild("yourKills")
			self.remainingPlayers = self.GetChild("remainingPlayers")
			
		except:
			import exception
			exception.Abort("BattleRoyaleInfo.LoadWindow.BindObject")
		
		#
		self.selectChar = BattleRoyaleSelectChar()
		self.selectChar.Hide()
		#
		
	def Show(self, remainingTime):
		# self.infoText.SetText("|cFFc7c7c7|h" + "Waiting for players..")
		self.infoText.SetText(localeinfo.WAITING)
		self.phase = PHASE_WAITING
		self.endTime = app.GetTime() + remainingTime
		self.toggleTerrainZoneBtn.Hide()
		self.lineSeparator.Hide()
		self.yourKills.Hide()
		self.remainingPlayers.Hide()
		self.board.SetSize(138, 48)
		ui.ScriptWindow.Show(self)
		
	def ClickToggleTerrainZone(self):
		if constinfo.BATTLE_ROYALE_TERRAIN_ZONE: #Hide it
			constinfo.BATTLE_ROYALE_TERRAIN_ZONE = False
			background.DisableSafeArea()
			self.toggleTerrainZoneBtnText.SetText(localeinfo.TERRAIN_ZONE)
		else: #Show it
			constinfo.BATTLE_ROYALE_TERRAIN_ZONE = True
			background.VisibleSafeArea()
			self.toggleTerrainZoneBtnText.SetText(localeinfo.TERRAIN_ZONE1)
		
	def ClickTeleport(self):
		if self.phase != PHASE_WAITING:
			return
			
		if self.selectChar.IsShow() == False:
			self.selectChar.SetCenterPosition()
			self.selectChar.SetTop()
			self.selectChar.Show()
			
	def WarpingPhase(self, iSec, playersCount):
		self.phase = PHASE_WARPING
		# self.infoText.SetText("|cFFc7c7c7|h" + "Select your position on map")
		self.infoText.SetText(localeinfo.SELECT_POSITION)
		self.endTime = app.GetTime() + iSec
		self.board.SetSize(138, 48 + 35)
		self.lineSeparator.Show()
		self.yourKills.Show()
		self.remainingPlayers.SetText(localeinfo.REMAINING_PLAYER + str(playersCount))
		self.remainingPlayers.Show()
		
	def RunningPhase(self, iSec, msgType):
		constinfo.BATTLE_ROYALE_TERRAIN_ZONE = True
		self.toggleTerrainZoneBtn.Show()
		self.phase = PHASE_RUNNING
		if msgType == 1:
			self.endTime = app.GetTime() + iSec
			self.infoText.SetText("|cFFc7c7c7|h" + localeinfo.ENTER_ZONE + "|cFFFF3333|h" + localeinfo.ENTER_NEW_ZONE + "|cFFc7c7c7|h" + " !") #red color
		elif msgType == 2:
			self.infoText.SetText("|cFFFF3333|h" + localeinfo.ENTER_NEW_ZONE + "|cFFc7c7c7|h " + localeinfo.REVALED) #red color
			self.endTime = app.GetTime() + iSec
		elif msgType == 3:
			self.phase = PHASE_LAST_ROUND
			self.infoText.SetText(localeinfo.EVENT_END_DRAW)
			self.endTime = app.GetTime() + iSec
			
	def EditKillCount(self, killCount):
		self.yourKills.SetText(localeinfo.YOUR_KILL + str(killCount))
		
	def EditPlayersCount(self, remainingPlayers):
		self.remainingPlayers.SetText(localeinfo.REMAINING_PLAYER + str(remainingPlayers))
		
	def OnUpdate(self):
		if self.IsShow():
		
			#Render terrain area for new position
			if self.phase == PHASE_RUNNING and app.GetTime() >= self.refreshTerrain and constinfo.BATTLE_ROYALE_TERRAIN_ZONE:
				background.DisableSafeArea()
				background.VisibleSafeArea()
				self.refreshTerrain = app.GetTime() + 10.0
			#Render terrain area for new position
		
			if app.GetTime() >= self.endTime:
			
				if background.GetCurrentMapName() != constinfo.BATTLE_ROYALE_MAP_NAME or self.phase == PHASE_LAST_ROUND:
					self.Hide()
				self.endTime = app.GetTime() + 60*60*24*5
				
				if self.phase == PHASE_WARPING:
					constinfo.BATTLE_ROYALE_CAN_WARP = False
			
			self.remainingTime.SetText("|cFFc7c7c7|h" + CalculateTimeLeft(self.endTime - app.GetTime()))

	def Hide(self):
		if self.selectChar:
			self.selectChar.Hide()

		ui.ScriptWindow.Hide(self)
		
	def Destroy(self):
		self.ClearDictionary()
		self.endTime = app.GetTime() + 60*60*24*5
		self.refreshTerrain = 0.0
		self.remaining = None
		self.teleportButton = None
		if self.selectChar:
			self.selectChar.Hide()
			self.selectChar = None
		self.phase = PHASE_DISABLED

	def __del__(self): 
		ui.ScriptWindow.__del__(self) 
