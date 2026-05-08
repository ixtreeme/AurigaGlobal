import app
import net
import player
import pack
import ui
import uitooltip
import uicommon
import mousemodule
import constinfo
import item
import effect
import dbg
import background
import grp
import chat
import wndMgr
import nonplayer

from datetime import datetime
from _weakref import proxy
import localeinfo

ROOT_PATH = "d:/ymir work/ui/game/battle_pass/"
MISSION_PATH = "d:/ymir work/ui/game/battle_pass/mission/"

RANKING_MAX_NUM = 9
MISSION_REWARD_COUNT = 3

BATTLE_PASS_NAMES = {
	1  : localeinfo.BATTLE_PASS_JAN,
	2  : localeinfo.BATTLE_PASS_FEB,
	3  : localeinfo.BATTLE_PASS_MAR,
	4  : localeinfo.BATTLE_PASS_APR,
	5  : localeinfo.BATTLE_PASS_MAJ,
	6  : localeinfo.BATTLE_PASS_JUN,
	7  : localeinfo.BATTLE_PASS_JUL,
	8  : localeinfo.BATTLE_PASS_AUG,
	9  : localeinfo.BATTLE_PASS_SEP,
	10 : localeinfo.BATTLE_PASS_OKT,
	11 : localeinfo.BATTLE_PASS_NOV,
	12 : localeinfo.BATTLE_PASS_DEC,
}


MISSION_INFO_DICT = {}

class MissionScrollBar(ui.DragButton):
	def __init__(self):
		ui.DragButton.__init__(self)
		self.AddFlag("float")
		self.AddFlag("movable")
		self.AddFlag("restrict_x")

		self.eventScroll = lambda *arg: None
		self.currentPos = 0.0
		self.scrollStep = 0.20

	def __del__(self):
		ui.DragButton.__del__(self)
		self.currentPos = 0.0
		self.scrollStep = 0.20
		self.eventScroll = lambda *arg: None

	def SetScrollEvent(self, event):
		self.eventScroll = event

	def SetScrollStep(self, step):
		self.scrollStep = step

	def SetPos(self, pos):
		pos = max(0.0, pos)
		pos = min(1.0, pos)

		yPos = float(pos * 165) # restrictedHeight - imageHeight - 3

		self.SetPosition(11, yPos + 3) # restrictedY
		self.OnMove()

	def GetPos(self):
		return self.currentPos

	def OnUp(self):
		self.SetPos(self.currentPos - self.scrollStep)

	def OnDown(self):
		self.SetPos(self.currentPos + self.scrollStep)

	def OnMove(self):
		(xLocal, yLocal) = self.GetLocalPosition()
		self.currentPos = float(yLocal - 3) / float(165) 

		self.eventScroll()

class ListBoxMissions(ui.Window):
	class Item(ui.Window):
		def __init__(self, missionId):
			ui.Window.__init__(self)
			self.SetWindowName("ListBoxMissions_Item")
			self.bIsSelected = False
			self.missionId = missionId
			self.percentActual = 0
			self.percentTotal = 0
			self.rewardCount = 0
			self.missionInfo1 = 0
			self.xBase, self.yBase = 0, 0

			self.tooltipItem = uitooltip.ItemToolTip()
			self.tooltipItem.Hide()

			self.listImages = []
			self.rewardList = [[0, 0], [0, 0], [0, 0]]
			self.rewardImages = []
			self.rewardListCount = []
			
			iconName = MISSION_PATH + "mission_%d_small.tga" % (missionId)
			if not pack.Exist(iconName): 
				iconName = ROOT_PATH + "mission_small_clean.tga"
				
			bgImage = ui.MakeExpandedImageBox(self, ROOT_PATH + "mission_bg_normal.tga", 0, 0, "not_pick")
			iconImage = ui.MakeExpandedImageBox(bgImage, iconName, 1, 2, "not_pick")
			bgGauge = ui.MakeExpandedImageBox(bgImage, ROOT_PATH + "mission_progress_empty.tga", 47, 28, "not_pick")
			bgGaugeFull = ui.MakeExpandedImageBox(bgGauge, ROOT_PATH + "mission_progress_full.tga", 8, 2, "not_pick")
			bgGaugeFull.SetWindowName("gaugeFull")

			self.listImages.append(bgImage)
			self.listImages.append(iconImage)
			self.listImages.append(bgGauge)
			self.listImages.append(bgGaugeFull)

			for i in xrange(MISSION_REWARD_COUNT):
				rewardImage = ui.MakeExpandedImageBox(self, "d:/ymir work/ui/pet/skill_button/skill_enable_button.sub", 187 + (32 * i), 6)
				rewardImage.SetEvent(ui.__mem_func__(self.OverInItem), "MOUSE_OVER_IN", i)
				rewardImage.SetEvent(ui.__mem_func__(self.OverOutItem), "MOUSE_OVER_OUT")
				self.rewardImages.append(rewardImage)

				itemCount = ui.NumberLine()
				itemCount.SetParent(rewardImage)
				itemCount.SetWindowName("itemCount_%d" % i)
				itemCount.SetHorizontalAlignRight()
				itemCount.SetPosition(32 - 4, 32 - 10)
				itemCount.Show()
				self.rewardListCount.append(itemCount)

			self.missionName = ui.AddTextLine(bgImage, 53, 6, "")

		def __del__(self):
			ui.Window.__del__(self)
			self.bIsSelected = False
			self.missionId = 0
			self.percentActual = 0
			self.percentTotal = 0
			self.rewardCount = 0
			self.missionInfo1 = 0
			self.xBase, self.yBase = 0, 0
			self.missionName = None
			self.tooltipItem = None

			self.listImages = []
			self.rewardList = [[0, 0], [0, 0], [0, 0]]
			self.rewardImages = []
			self.rewardListCount = []

		def OverInItem(self, eventType, rewardIndex):
			if self.tooltipItem:
				self.tooltipItem.ClearToolTip()
				if self.rewardList[rewardIndex][0]:
					self.tooltipItem.AddItemData(self.rewardList[rewardIndex][0], metinSlot = [0 for i in xrange(player.METIN_SOCKET_MAX_NUM)])
					self.tooltipItem.ShowToolTip()

		def OverOutItem(self):
			if self.tooltipItem:
				self.tooltipItem.HideToolTip()

		def SetBasePosition(self, x, y):
			self.xBase = x
			self.yBase = y

		def GetBasePosition(self):
			return (self.xBase, self.yBase)

		def GetMissionId(self):
			return self.missionId

		def OnMouseLeftButtonUp(self):
			self.Select()

		def Select(self):
			self.bIsSelected = True
			self.parent.SetSelectedMission(self.missionId)

			if len(self.listImages) > 0:
				self.listImages[0].LoadImage(ROOT_PATH + "mission_bg_selected.tga")

		def Deselect(self):
			self.bIsSelected = False

			if len(self.listImages) > 0:
				self.listImages[0].LoadImage(ROOT_PATH + "mission_bg_normal.tga")

		def SetProgress(self, progressActual, pregressTotal):
			self.percentActual = progressActual
			self.percentTotal = pregressTotal

			self.UpdateGauge()

		def UpdateProgress(self, newProgress):
			if newProgress >= self.percentActual:
				self.percentActual = newProgress
				self.UpdateGauge()
			else:
				chat.AppendChat(chat.CHAT_TYPE_INFO, "[Error]: Update progress error, new progress is lower than old progress.")
				
		def UpdateGauge(self):
			for image in self.listImages:
				if image.GetWindowName() == "gaugeFull":
					if self.percentActual == self.percentTotal:
						image.LoadImage(ROOT_PATH + "mission_progress_full.tga")
					else:
						if self.percentActual <= int(self.percentTotal / 2):
							image.LoadImage(ROOT_PATH + "mission_progress_low.tga")
						else:
							image.LoadImage(ROOT_PATH + "mission_progress_middle.tga")

		def IsCompleted(self):
			if self.percentActual >= self.percentTotal:
				return True

			return False

		def SetMissionName(self, missionName):
			if self.missionName:
				self.missionName.SetText(missionName)

		def SetMissionInfo1(self, missionInfo):
			self.missionInfo1 = missionInfo

		def GetMissionInfo(self):
			return (self.missionInfo1, self.percentActual, self.percentTotal)

		def AddMissionReward(self, itemVnum, itemCount):
			if self.rewardCount == -1:
				return

			if itemVnum and itemCount > 0:
				if self.rewardCount < len(self.rewardImages):
					item.SelectItem(itemVnum)
					self.rewardImages[self.rewardCount].LoadImage(item.GetIconImageFileName())
					self.rewardListCount[self.rewardCount].SetNumber(str(itemCount))
					self.rewardList[self.rewardCount] = [itemVnum, itemCount]
					self.rewardCount += 1
			else:
				self.rewardCount = -1

		def Show(self):
			ui.Window.Show(self)

		def SetParent(self, parent):
			ui.Window.SetParent(self, parent)
			self.parent = proxy(parent)

		# 2020.01.22.Owsap - Added tooltip for mission rewards
		def OnUpdate(self):
			isInMissionRewards = None
			if self.rewardImages[0].IsIn():
				isInMissionRewards = 0
			elif self.rewardImages[1].IsIn():
				isInMissionRewards = 1
			elif self.rewardImages[2].IsIn():
				isInMissionRewards = 2

			if isInMissionRewards != None:
				if self.tooltipItem:
					self.tooltipItem.ClearToolTip()
					if self.rewardList[isInMissionRewards][0] and self.rewardList[isInMissionRewards][0] != 0:
						self.tooltipItem.AddItemData(self.rewardList[isInMissionRewards][0], metinSlot = [0 for i in xrange(player.METIN_SOCKET_MAX_NUM)])
						self.tooltipItem.ShowToolTip()
					else:
						self.tooltipItem.HideToolTip()
			else:
				if self.tooltipItem:
					self.tooltipItem.HideToolTip()

		def OnRender(self):
			xList, yList = self.parent.GetGlobalPosition()
			
			for item in self.listImages + self.rewardImages:
				if item.GetWindowName() == "gaugeFull":
					if self.percentTotal == 0:
						self.percentTotal = 1
					item.SetClipRect(0.0, yList, -1.0 + float(self.percentActual) / float(self.percentTotal), yList + self.parent.GetHeight(), True)
				else:
					item.SetClipRect(xList, yList, xList + self.parent.GetWidth(), yList + self.parent.GetHeight())

			if self.missionName:
				xText, yText = self.missionName.GetGlobalPosition()
				wText, hText = self.missionName.GetTextSize()
				
				if yText < yList or (yText + hText > yList + self.parent.GetHeight()):
					self.missionName.Hide()
				else:
					self.missionName.Show()
					
			for count in self.rewardListCount:
				xList, yList = self.parent.GetGlobalPosition()
				xText, yText = count.GetGlobalPosition()
				wText, hText = count.GetWidth(), 7
				
				if yText < yList or (yText + hText > yList + self.parent.GetHeight()):
					count.Hide()
				else:
					count.Show()

	def __init__(self):
		ui.Window.__init__(self)
		self.SetWindowName("ListBoxMissions")
		self.itemList = []

		self.selectedMission = 0

	def __del__(self):
		ui.Window.__del__(self)

		self.itemList = []

		self.selectedMission = 0
		self.globalParent = None

	def SetSelectedMission(self, missionId):
		self.selectedMission = missionId

		for itemH in self.itemList:
			if itemH.GetMissionId() != self.selectedMission:
				itemH.Deselect()

		if self.globalParent:
			self.globalParent.SetMissionInfo(self.selectedMission)

	def GetSelectedMission(self):
		return self.selectedMission

	def SelectMission(self, missionId):
		for item in self.itemList:
			if missionId == item.GetMissionId():
				item.Select()

	def HaveMission(self, missionId):
		for item in self.itemList:
			if missionId == item.GetMissionId():
				return True

		return False

	def GetMissionInfo(self, missionId):
		for item in self.itemList:
			if missionId == item.GetMissionId():
				return item.GetMissionInfo()

		return (0, 0, 0)

	def GetMissionCount(self):
		return len(self.itemList)

	def GetCompletedMissionCount(self):
		completedCount = 0
		for item in self.itemList:
			if item.IsCompleted():
				completedCount += 1

		return completedCount

	def SetProgress(self, missionId, progressActual, pregressTotal):
		for item in self.itemList:
			if missionId == item.GetMissionId():
				item.SetProgress(progressActual, pregressTotal)

	def UpdateProgress(self, missionId, newProgress):
		for item in self.itemList:
			if missionId == item.GetMissionId():
				item.UpdateProgress(newProgress)

	def AddMissionReward(self, missionId, itemVnum, itemCount):
		for item in self.itemList:
			if missionId == item.GetMissionId():
				item.AddMissionReward(itemVnum, itemCount)

	def SetGlobalParent(self, parent):
		self.globalParent = proxy(parent)
		
	def OnScroll(self, scrollPos):
		totalHeight = 0
		for itemH in self.itemList:
			totalHeight += itemH.GetHeight() 
			
		totalHeight -= self.GetHeight()

		for i in xrange(len(self.itemList)):
			x, y = self.itemList[i].GetLocalPosition()
			xB, yB = self.itemList[i].GetBasePosition()
			setPos = yB - int(scrollPos * totalHeight)
			self.itemList[i].SetPosition(xB, setPos)

	def AppendMission(self, itemHeight, missionId, missionName, missionInfo1):
		item = self.Item(missionId)
		item.SetParent(self)
		item.SetSize(self.GetWidth() - 3, itemHeight)
		item.SetMissionName(missionName)
		item.SetMissionInfo1(missionInfo1)

		if len(self.itemList) == 0:
			item.SetPosition(0, 0)
			item.SetBasePosition(0, 0)
		else:
			x, y = self.itemList[-1].GetLocalPosition()
			item.SetPosition(0, y + self.itemList[-1].GetHeight())
			item.SetBasePosition(0, y + self.itemList[-1].GetHeight())

		item.Show()
		self.itemList.append(item)

	def HideToolTipMission(self):
		for item in self.itemList:
			item.OverOutItem()

class BattlePassWindow(ui.ScriptWindow):
	def __init__(self):
		ui.ScriptWindow.__init__(self)

		self.Initialize()
		self.LoadMissionList()
		self.LoadWindow()

	def Initialize(self):
		self.board = None
		self.titleBar = None
		self.titleName = None
		self.scrollBar = None
		self.tooltipItem = None

		self.currentState = "MISSION"
		self.tabButtonDict = None

		self.rankingButton = None
		self.rewardButton = None

		self.scrollBarWindow = None
		self.missionWindow = None
		self.rankingWindow = None

		self.missionNameText = None
		self.totalProgressGauge = None
		self.bgImageMission = None
		self.rewardItems = None

		self.loadingBackground = None
		self.loadingAnimation = None

		self.missionInfoDict = {}
		self.generalInfoDict = {}

		self.rankingImages = []
		self.rankingSpecialImages = []
		self.rankingTexts = {}
		self.rankingInfo = {}
		self.rewardDict = {}

		self.battlePassID = 0
		self.battlePassEndTime = 0
		self.rewardSlotIndex = 0

		self.currentRankingPage = 1
		self.firstPrevButton = None
		self.prevButton = None
		self.nextButton = None
		self.lastNextButton = None
		self.pageButtonList = [ None, None, None, None, None ]
		
		self.bgBar = None
		self.isInactive = False
		
	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def SetItemToolTip(self, itemTooltip):
		self.tooltipItem = itemTooltip

	def LoadMissionList(self):
		try:
			lines = open(app.GetLocalePath() + "/battle_pass.txt", "r").readlines()
		except:
			import exception
			exception.Abort("LoadMissionList")

		for line in lines:
			tokens = line[:-1].split("\t")
			if len(tokens) == 0 or not tokens[0]:
				continue

			if tokens[0] == "#":
				continue

			MISSION_INFO_DICT[int(tokens[0])] = [ tokens[1], tokens[2], tokens[3], tokens[4], tokens[5], tokens[6] ]

	def LoadWindow(self):
		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "uiscript/battlepasswindow.py")
		except:
			import exception
			exception.Abort("BattlePassWindow.LoadDialog.LoadScript")

		try:
			self.board = self.GetChild("board")
			self.titleBar = self.GetChild("TitleBar")
			self.titleName = self.GetChild("TitleName")

			self.titleBar.SetCloseEvent(ui.__mem_func__(self.Close))

			self.tabButtonDict = {
				"MISSION" : [
					self.GetChild("tab_button_01"),
					self.GetChild("tab_01"),
					self.GetChild("BorderInfoMission")
				],
				"GENERAL" : [
					self.GetChild("tab_button_02"),
					self.GetChild("tab_02"),
					self.GetChild("BorderInfoGeneral")
				]
			}

			for i in xrange(5):
				self.generalInfoDict[i] = self.GetChild("general_text_%d" % int(i))

			for j in xrange(7):
				self.missionInfoDict[j] = self.GetChild("mission_text_%d" % int(j))

			self.missionNameText = self.GetChild("missionTitleText")
			self.totalProgressGauge = self.GetChild("gaugeImage")

			self.scrollBarWindow = self.GetChild("BorderScroll")
			self.missionWindow = self.GetChild("BorderMissions")
			self.rankingWindow = self.GetChild("BorderRanking")
				
			self.shopButton = self.GetChild("ShopButton")
			self.rewardButton = self.GetChild("RewardButton")
			self.bgImageMission = self.GetChild("bgImageMission")
			self.rewardItems = self.GetChild("RewardItems")

			self.firstPrevButton = self.GetChild("first_prev_button")
			self.prevButton = self.GetChild("prev_button")
			self.nextButton = self.GetChild("next_button")
			self.lastNextButton = self.GetChild("last_next_button")

			for i in xrange(5):
				self.pageButtonList[i] = self.GetChild("page%d_button" % int(i + 1))
		except:
			import exception
			exception.Abort("BattlePassWindow.LoadWindow.BindObject")

		self.rankingWindow.Hide()

		for (tabKey, tabButton) in self.tabButtonDict.items():
			tabButton[0].SetEvent(ui.__mem_func__(self.OnClickTabButton), tabKey)
				
		self.shopButton.SetEvent(ui.__mem_func__(self.OnClickShopWindowButton))
		self.rewardButton.SetEvent(ui.__mem_func__(self.OnClickRewardButton))
		#self.rewardButton.Disable()

		self.firstPrevButton.SetEvent(ui.__mem_func__(self.OnClickRankingFirstPrevButton))
		self.prevButton.SetEvent(ui.__mem_func__(self.OnClickRankingPrevButton))
		self.nextButton.SetEvent(ui.__mem_func__(self.OnClickRankingNextButton))
		self.lastNextButton.SetEvent(ui.__mem_func__(self.OnClickRankingLastNextButton))

		for i in xrange(len(self.pageButtonList)):
			self.pageButtonList[i].SetEvent(ui.__mem_func__(self.ClickPageButton), i)

		self.rewardItems.SetOverInItemEvent(ui.__mem_func__(self.OverInItem))
		self.rewardItems.SetOverOutItemEvent(ui.__mem_func__(self.OverOutItem))

		self.scrollBar = MissionScrollBar()
		self.scrollBar.SetParent(self.scrollBarWindow)
		self.scrollBar.SetScrollEvent(ui.__mem_func__(self.OnScroll))
		self.scrollBar.SetUpVisual(ROOT_PATH + "scroll_bar.tga")
		self.scrollBar.SetOverVisual(ROOT_PATH + "scroll_bar.tga")
		self.scrollBar.SetDownVisual(ROOT_PATH + "scroll_bar.tga")
		self.scrollBar.SetRestrictMovementArea(11, 3, 6, 249)
		self.scrollBar.SetPosition(11, 3)
		self.scrollBar.Show()

		self.missionList = ListBoxMissions()
		self.missionList.SetParent(self.missionWindow)
		self.missionList.SetGlobalParent(self)
		self.missionList.SetPosition(4, 4)
		self.missionList.SetSize(300, 249) # 290(H) without scrollbar, 249(W) with Render
		self.missionList.Show()
		
		self.bgBar = ui.Bar()
		self.bgBar.SetParent(self.board)
		self.bgBar.SetColor(grp.GenerateColor(0.0, 0.0, 0.0, 0.6))
		self.bgBar.SetPosition(10, 30)
		self.bgBar.SetSize(self.board.GetWidth() - 20, self.board.GetHeight() - 40)
		self.bgBar.Hide()

		
		self.itemSlot = ui.GridSlotWindow()
		self.itemSlot.SetParent(self.bgBar)
		self.itemSlot.SetPosition(245, 88)
		self.itemSlot.SetOverInItemEvent(ui.__mem_func__(self.POverInItem))
		self.itemSlot.SetOverOutItemEvent(ui.__mem_func__(self.OverOutItem))
		self.itemSlot.Show()
		self.itemSlot.ArrangeSlot(0, 3, 1, 32, 32, 0, 0)
		self.itemSlot.SetSlotStyle(wndMgr.SLOT_STYLE_NONE)
		self.itemSlot.RefreshSlot()
		self.itemSlot.SetItemSlot(0, 70611, 1)
		self.itemSlot.EnableCoverButton(0)

		self.infoBP = ui.TextLine()
		self.infoBP.SetParent(self.bgBar)
		self.infoBP.SetPosition(0, 0)
		self.infoBP.SetHorizontalAlignCenter()
		self.infoBP.SetWindowHorizontalAlignCenter()
		self.infoBP.SetWindowVerticalAlignCenter()
		self.infoBP.SetText(localeinfo.BATTLE_PASS_ALERT1)
		self.infoBP.Show()

		self.continueButton = ui.Button()
		self.continueButton.SetParent(self.board)
		self.continueButton.SetPosition(250, 175)
		self.continueButton.SetUpVisual("d:/ymir work/ui/public/small_button_01.sub")
		self.continueButton.SetOverVisual("d:/ymir work/ui/public/small_button_02.sub")
		self.continueButton.SetDownVisual("d:/ymir work/ui/public/small_button_03.sub")
		self.continueButton.SetText(localeinfo.BATTLE_PASS_OK)
		self.continueButton.SetEvent(ui.__mem_func__(self.SetInactiveBG), False, True)
		self.continueButton.Hide()
			
		self.CreateRankingItems()
		self.SetInactiveBG(True, True)

	def POverInItem(self, slotIndex):
		if self.tooltipItem:		
			self.tooltipItem.ClearToolTip()
			self.tooltipItem.AddItemData(70611, [0, 0, 0])
			self.tooltipItem.ShowToolTip()

	def SetInactiveBG(self, arg = False, skip = False):
		self.continueButton.Show()

		if arg:
			self.bgBar.Show()
			self.SetStatus(True)
			self.missionInfoDict[0].SetText(localeinfo.BATTLE_PASS_STATUS % ("|cffe14612|H|h", localeinfo.BATTLE_PASS_INACTIVE))
		else:
			self.bgBar.Hide()
			#self.isInactive = False
			#if not skip:
			self.missionInfoDict[0].SetText(localeinfo.BATTLE_PASS_STATUS % ("|cfffeff7d|H|h", localeinfo.BATTLE_PASS_ACTIVE))
			
			if skip:
				self.missionInfoDict[0].SetText(localeinfo.BATTLE_PASS_STATUS % ("|cffe14612|H|h", localeinfo.BATTLE_PASS_INACTIVE))
	
			self.continueButton.Hide()

	def GetStatus(self):
		return self.isInactive

	def SetStatus(self, flag = False):
		self.isInactive = flag
		if not flag:
			self.missionInfoDict[0].SetText(localeinfo.BATTLE_PASS_STATUS % ("|cfffeff7d|H|h", localeinfo.BATTLE_PASS_ACTIVE))
		else:
			self.missionInfoDict[0].SetText(localeinfo.BATTLE_PASS_STATUS % ("|cffe14612|H|h", localeinfo.BATTLE_PASS_INACTIVE))
		
		return self.isInactive
		
	def GetTextByInfo(self, missionId, info_1):
		if missionId in [1, 3]:
			return nonplayer.GetMonsterName(info_1)
		elif missionId in [2, 4]:
			return str(info_1)
		elif missionId in [5, 6, 7, 8, 9, 10, 19, 20, 21, 22]:
			item.SelectItem(info_1)
			return item.GetItemName()

		return ""

	def SetMissionInfo(self, missionId):
		missionName = MISSION_INFO_DICT[missionId][0] if MISSION_INFO_DICT.has_key(missionId) else localeinfo.BATTLE_PASS_UNKNOWN_NAME
		self.missionNameText.SetText(missionName)
			
		(info_1, info_2, info_3) = self.missionList.GetMissionInfo(missionId)

		self.missionInfoDict[0].SetText(localeinfo.BATTLE_PASS_STATUS % ("|cffe14612|H|h", localeinfo.BATTLE_PASS_INACTIVE))

		if self.bgBar and not self.bgBar.IsShow():
			if self.GetStatus():
				self.missionInfoDict[0].SetText(localeinfo.BATTLE_PASS_STATUS % ("|cffe14612|H|h", localeinfo.BATTLE_PASS_INACTIVE))
			else:
				if info_2 >= info_3:
					self.missionInfoDict[0].SetText(localeinfo.BATTLE_PASS_STATUS % ("|cff82ff7d|H|h", localeinfo.BATTLE_PASS_COMPLETED))
				else:
					self.missionInfoDict[0].SetText(localeinfo.BATTLE_PASS_STATUS % ("|cfffeff7d|H|h", localeinfo.BATTLE_PASS_ACTIVE))

		textInfo = self.GetTextByInfo(missionId, info_1)
		if textInfo != "":
			self.missionInfoDict[1].SetText(MISSION_INFO_DICT[missionId][2] % textInfo)
			self.missionInfoDict[4].SetText(MISSION_INFO_DICT[missionId][3] % textInfo)
		else:
			self.missionInfoDict[1].SetText(MISSION_INFO_DICT[missionId][2])
			self.missionInfoDict[4].SetText(MISSION_INFO_DICT[missionId][3])

		self.missionInfoDict[2].SetText(MISSION_INFO_DICT[missionId][1] % localeinfo.AddPointToNumberString(int(info_3 - info_2)))
		self.missionInfoDict[3].SetText(localeinfo.BATTLE_PASS_PROCENT % float(info_2 * 100 / info_3))
		
		self.missionInfoDict[5].SetText(MISSION_INFO_DICT[missionId][4])
		self.missionInfoDict[6].SetText(MISSION_INFO_DICT[missionId][5])

		self.missionInfoDict[4].SetTextColor(0xfff8ffce)
		self.missionInfoDict[5].SetTextColor(0xfff8ffce)
		self.missionInfoDict[6].SetTextColor(0xfff8ffce)
		
		imageName = MISSION_PATH + "mission_%d_big.tga" % (missionId)
		if not pack.Exist(imageName): 
			imageName = ROOT_PATH + "mission_big_clean.tga"

		self.bgImageMission.LoadImage(imageName)

	def AddReward(self, itemVnum, itemCount):
		if self.rewardItems:
			self.rewardItems.SetItemSlot(self.rewardSlotIndex, itemVnum, itemCount)
			self.rewardDict[self.rewardSlotIndex] = [itemVnum, itemCount]

			self.rewardSlotIndex += 1 if self.rewardSlotIndex != 2 else 2

	def AddMission(self, missionType, missionInfo1, missionInfo2, missionInfo3):
		if self.missionList:
			if self.missionList.HaveMission(missionType):
				self.missionList.SetProgress(missionType, missionInfo3, missionInfo2)
			else:
				missionName = MISSION_INFO_DICT[missionType][0] if MISSION_INFO_DICT.has_key(missionType) else "Unknown name"
				self.missionList.AppendMission(50, missionType, missionName, missionInfo1)
				self.missionList.SetProgress(missionType, missionInfo3, missionInfo2)
				
	def UpdateMission(self, missionType, newProgress):
		if self.missionList:
			if self.missionList.HaveMission(missionType):
				self.missionList.UpdateProgress(missionType, newProgress)
				if self.missionList.GetSelectedMission() == missionType:
					self.SetMissionInfo(missionType)
				self.RefreshGeneralInfo()
			#else:
			#	chat.AppendChat(chat.CHAT_TYPE_INFO, "[Error]: Can't update mission progress.")
			
	def AddMissionReward(self, missionType, itemVnum, itemCount):
		if self.missionList:
			self.missionList.AddMissionReward(missionType, itemVnum, itemCount)
		
	def OnScroll(self):
		if self.missionList:
			self.missionList.OnScroll(self.scrollBar.GetPos())

	def OnClickTabButton(self, stateKey):
		self.currentState = stateKey

		for tabKey, tabButton in self.tabButtonDict.items():
			if stateKey != tabKey:
				tabButton[0].SetUp()
				
			tabButton[1].Hide()
			tabButton[2].Hide()

		self.tabButtonDict[stateKey][1].Show()
		self.tabButtonDict[stateKey][2].Show()
		
		if stateKey == "GENERAL":
			self.RefreshGeneralInfo()
	
		self.OnClickRankingButton(True)
		
	def OnUpdate(self):
		self.RefreshGeneralInfo()

	def SetBattlePassInfo(self, battlePassID = 1, endTime = 1):
		self.battlePassID = battlePassID
		
		if endTime > 0:
			self.battlePassEndTime = app.GetGlobalTimeStamp() + endTime
		
	def RefreshGeneralInfo(self):
		if self.missionList:
			if self.generalInfoDict.has_key(0) and BATTLE_PASS_NAMES.has_key(self.battlePassID):
				self.generalInfoDict[0].SetText(localeinfo.BATTLE_PASS_NAME % str(BATTLE_PASS_NAMES[self.battlePassID]))
				
			missionCount = self.missionList.GetMissionCount()
			completedMissionCount = self.missionList.GetCompletedMissionCount()
			if self.generalInfoDict.has_key(1):
				self.generalInfoDict[1].SetText(localeinfo.BATTLE_PASS_COUNTQUEST % (missionCount-completedMissionCount))
	
			if self.generalInfoDict.has_key(2):
				self.generalInfoDict[2].SetText(localeinfo.BATTLE_PASS_COUNTQUEST_COMPELTED % (completedMissionCount))
				
			# if missionCount == completedMissionCount:
				# self.rewardButton.Enable()
	
			if self.totalProgressGauge:
				self.totalProgressGauge.SetPercentage(completedMissionCount, missionCount)
				
	def OnClickRewardButton(self):
		net.SendBattlePassAction(2)

	# def OnClickShopWindowButton(self):
		# net.SendOpenShopPacket(53)
	def OnClickShopWindowButton(self):
		import net
		net.SendChatPacket("/wheel")



	def OnClickRankingButton(self, isHide = False):
		if self.rankingWindow.IsShow() or isHide:
			self.rankingWindow.Hide()
			self.missionWindow.Show()
			self.scrollBarWindow.Show()
		else:
			self.rankingWindow.Show()
			self.missionWindow.Hide()
			self.scrollBarWindow.Hide()

			if self.rankingInfo.has_key(1):
				self.RefreshRankingInfo()
			else:
				net.SendBattlePassAction(3)

	def OnClickRankingFirstPrevButton(self):
		self.currentRankingPage = 1
		self.RefreshRankingInfo()

	def OnClickRankingPrevButton(self):
		prevPage = max(1, self.currentRankingPage - 1)
		self.currentRankingPage = prevPage
		self.RefreshRankingInfo()

	def OnClickRankingNextButton(self):
		nextPage = min(5, self.currentRankingPage + 1)
		self.currentRankingPage = nextPage
		self.RefreshRankingInfo()

	def OnClickRankingLastNextButton(self):
		self.currentRankingPage = 5
		self.RefreshRankingInfo()

	def ClickPageButton(self, pageIndex):
		toPage = int(self.pageButtonList[pageIndex].GetText())
		if toPage > 5:
			return

		if toPage == self.currentRankingPage:
			return

		self.currentRankingPage = toPage
		self.RefreshRankingInfo()

	def GetPageByPos(self, pos):
		return int(pos / 10) + 1

	def GetRealPos(self, pos):
		return ((pos - 1) if pos < 10 else (pos % 10))

	def AddRanking(self, pos, playerName, finishTime):
		page = self.GetPageByPos(pos)
		if not self.rankingInfo.has_key(page):
			self.rankingInfo[page] = {}

		realPos = self.GetRealPos(pos)
		self.rankingInfo[page][realPos] = { "pos" : pos, "name" : playerName, "time" : finishTime }

	def RefreshRanking(self):
		self.RefreshRankingInfo()

		if self.loadingBackground:
			self.loadingBackground.Hide()

		if self.loadingAnimation:
			self.loadingAnimation.Hide()

	def RefreshRankingInfo(self):
		for image in self.rankingSpecialImages:
			if self.currentRankingPage == 1:
				image.Show()
			else:
				image.Hide()

		if self.rankingInfo.has_key(self.currentRankingPage):
			for i in xrange(RANKING_MAX_NUM):
				if self.rankingInfo[self.currentRankingPage].has_key(i):
					self.rankingTexts[i]["pos"].SetText(str(self.rankingInfo[self.currentRankingPage][i]["pos"]))
					self.rankingTexts[i]["name"].SetText(self.rankingInfo[self.currentRankingPage][i]["name"])

					stringTime = datetime.fromtimestamp(self.rankingInfo[self.currentRankingPage][i]["time"]).strftime('%Y-%m-%d %H:%M')
					self.rankingTexts[i]["time"].SetText(str(stringTime))
				else:
					self.rankingTexts[i]["pos"].SetText("")
					self.rankingTexts[i]["name"].SetText("")
					self.rankingTexts[i]["time"].SetText("")
		else:
			for i in xrange(RANKING_MAX_NUM):
				self.rankingTexts[i]["pos"].SetText("")
				self.rankingTexts[i]["name"].SetText("")
				self.rankingTexts[i]["time"].SetText("")

		self.RefreshPagination()

	def RefreshPagination(self):
		for page in self.pageButtonList:
			if int(page.GetText()) == self.currentRankingPage:
				page.Down()
				page.Disable()
			else:
				page.SetUp()
				page.Enable()

	def CreateRankingItems(self):
		for i in xrange(RANKING_MAX_NUM):
			# imageName = ROOT_PATH + "ranking_item_over.tga" if name == player.GetName() else ROOT_PATH + "ranking_item_normal.tga"
			itemImage = ui.MakeImageBox(self.rankingWindow, ROOT_PATH + "ranking_item_normal.tga", 3, 25 + (i * 23))
			self.rankingImages.append(itemImage)

			if i < 3:
				imageBorder = ui.MakeImageBox(itemImage, ROOT_PATH + "place_%s_border.tga" % (int(i+1)), 0, 0)
				self.rankingSpecialImages.append(imageBorder)

			self.rankingTexts[i] = {
				"pos" : ui.MakeNewTextLine(itemImage, False, False, 21, 9),
				"name" : ui.MakeNewTextLine(itemImage, False, False, 110, 9),
				"time" : ui.MakeNewTextLine(itemImage, False, False, 240, 9)
			}

		if not self.loadingBackground:
			self.loadingBackground = ui.ImageBox()
			self.loadingBackground.SetParent(self.rankingWindow)
			self.loadingBackground.SetPosition(2, 2)
			self.loadingBackground.LoadImage(ROOT_PATH + "loading_background.tga")
			self.loadingBackground.Show()

		if not self.loadingAnimation:
			self.loadingAnimation = ui.AniImageBox()
			self.loadingAnimation.SetParent(self.rankingWindow)
			self.loadingAnimation.SetDelay(5)

			for i in xrange(12):
				self.loadingAnimation.AppendImage("d:/ymir work/ui/game/battle_pass/big_loading/%d.tga" % i)

			self.loadingAnimation.Show()
			
	def OnRunMouseWheel(self, nLen):
		if not self.scrollBar.IsShow():
			return False
		
		x, y = self.GetGlobalPosition()
		xMouse, yMouse = wndMgr.GetMousePosition()
		difX = xMouse - x
		difY = yMouse - y
		if difX >= 6 and difX <= 528 and difY >= 6 and difY <= 288:
			if nLen > 0 and self.scrollBar:
				self.scrollBar.OnUp()
				return True
			else:
				self.scrollBar.OnDown()
				return True
		
		return False

	def OverInItem(self, slotPos):
		if self.tooltipItem:
			self.tooltipItem.ClearToolTip()
			if self.rewardDict.has_key(slotPos):
				self.tooltipItem.AddItemData(self.rewardDict[slotPos][0], metinSlot = [0 for i in xrange(player.METIN_SOCKET_MAX_NUM)])
				self.tooltipItem.ShowToolTip()

	def OverOutItem(self):
		if self.tooltipItem:
			self.tooltipItem.HideToolTip()

	def OnUpdate(self):
		if self.currentState == "GENERAL":
			if self.generalInfoDict.has_key(3):
				if self.battlePassEndTime > 0:
					leftTime = localeinfo.SecondToDHM(self.battlePassEndTime - app.GetGlobalTimeStamp())
					self.generalInfoDict[3].SetText(localeinfo.BATTLE_PASS_REMAINING_TIME % str(leftTime))

	def Destroy(self):
		self.ClearDictionary()
		self.Initialize()
		
		del self.bgBar

	def Open(self):
		self.SetSize(537, 297)
		self.board.SetSize(537, 297)
		self.titleBar.SetWidth(522)
		self.titleName.SetPosition(522 / 2, 3)

		for (tabKey, tabButton) in self.tabButtonDict.items():
			for tab in tabButton:
				tab.Show()

		self.OnClickTabButton("MISSION")

		if self.missionList:
			self.missionList.SelectMission(1)

			if self.scrollBar:
				missionCount = self.missionList.GetMissionCount()
				if missionCount <= 5:
					self.scrollBar.Hide()
				else:
					self.scrollBar.SetScrollStep(float(1.0 / missionCount))

		self.Show()
		self.SetCenterPosition()
		self.SetTop()

	def OpenRanking(self):
		self.rankingWindow.Show()
		self.missionWindow.Hide()
		self.scrollBarWindow.Hide()
		self.RefreshRanking()

		self.SetSize(325, 297)
		self.board.SetSize(325, 297)
		self.titleBar.SetWidth(310)
		self.titleName.SetPosition(310 / 2, 3)

		for (tabKey, tabButton) in self.tabButtonDict.items():
			for tab in tabButton:
				tab.Hide()

		self.Show()
		self.SetCenterPosition()
		self.SetTop()	

	def RefreshRewardSlot(self):
		if self.rewardItems:
			self.rewardSlotIndex = 0
			for i in xrange(6):
				self.rewardItems.ClearSlot(i)
			self.rewardItems.RefreshSlot()

	def Close(self):
		if self.missionList:
			self.missionList.HideToolTipMission()
		
		if self.tooltipItem:
			self.tooltipItem.HideToolTip()
			self.tooltipItem.ClearToolTip()
		
		self.RefreshRewardSlot()
		self.Hide()

	def OnPressEscapeKey(self):
		self.Close()
		return True

class BattlePassButton(ui.Window):
	def __init__(self):
		ui.Window.__init__(self)

		self.SetPosition(wndMgr.GetScreenWidth() - 110, 175)
		self.SetSize(85, 23)

		self.requestEndTime = 0
		self.openButton = None
		self.loadingImage = None
		self.Initialize()

	def __del__(self):
		ui.Window.__del__(self)

	def Destroy(self):
		self.openButton = None

	def Initialize(self):
		self.openButton = ui.Button()
		self.openButton.SetParent(self)
		self.openButton.SetPosition( 0, 0)
		self.openButton.SetUpVisual(ROOT_PATH + "battle_pass_normal.tga")
		self.openButton.SetOverVisual(ROOT_PATH + "battle_pass_over.tga")
		self.openButton.SetDownVisual(ROOT_PATH + "battle_pass_down.tga")
		self.openButton.SetEvent(ui.__mem_func__(self.RequestOpenBattlePass))
		self.openButton.Hide()

		self.loadingImage = ui.AniImageBox()
		self.loadingImage.SetParent(self)
		self.loadingImage.SetDelay(6)

		for i in xrange(12):
			self.loadingImage.AppendImage(ROOT_PATH + "loading/%d.tga" % int(i))

		self.loadingImage.SetPosition(0, 0)
		self.loadingImage.Hide()

	def BindInterface(self, interface):
		self.interface = interface

	def RequestOpenBattlePass(self):
		if self.interface:
			if self.interface.wndBattlePass:
				if self.interface.wndBattlePass.IsShow():
					self.interface.wndBattlePass.Close()
					return

		net.SendBattlePassAction(1)

		self.openButton.Hide()
		self.loadingImage.Show()
		self.requestEndTime = app.GetGlobalTimeStamp() + 5

	def OnUpdate(self):
		if self.requestEndTime > 0:
			if self.requestEndTime - app.GetGlobalTimeStamp() <= 0:
				self.CompleteLoading()
				self.requestEndTime = 0

	def CompleteLoading(self):
		self.requestEndTime = 0
		self.openButton.Show()
		self.loadingImage.Hide()

	def ShowButton(self):
		if self.openButton:
			self.openButton.Show()

	def HideButton(self):
		if self.openButton:
			self.openButton.Hide()