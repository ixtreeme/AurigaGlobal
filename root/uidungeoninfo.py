import app
import player
import ui
import uitooltip
import grp
import item
import uiscriptlocale
import uicommon
import constinfo
import item
import localeinfo

class DungeonInfo(ui.ScriptWindow):
	AFFECT_DICT = {
		item.APPLY_ATTBONUS_ANIMAL : localeinfo.TOOLTIP_DUNGEON_APPLY_ATTBONUS_ANIMAL,
		item.APPLY_ATTBONUS_ORC : localeinfo.TOOLTIP_DUNGEON_APPLY_ATTBONUS_ORC,
		item.APPLY_ATTBONUS_UNDEAD : localeinfo.TOOLTIP_DUNGEON_APPLY_ATTBONUS_UNDEAD,
		item.APPLY_ATTBONUS_DEVIL : localeinfo.TOOLTIP_DUNGEON_APPLY_ATTBONUS_DEVIL,
		item.APPLY_RESIST_FIRE : localeinfo.TOOLTIP_DUNGEON_APPLY_RESIST_FIRE,
		item.APPLY_RESIST_ELEC : localeinfo.TOOLTIP_DUNGEON_APPLY_RESIST_ELEC,
		item.APPLY_RESIST_ICE : localeinfo.TOOLTIP_DUNGEON_APPLY_RESIST_ICE,
		item.APPLY_RESIST_WIND : localeinfo.TOOLTIP_DUNGEON_APPLY_RESIST_WIND,
		item.APPLY_RESIST_EARTH : localeinfo.TOOLTIP_DUNGEON_APPLY_RESIST_EARTH,
		item.APPLY_RESIST_DARK : localeinfo.TOOLTIP_DUNGEON_APPLY_RESIST_DARK,
		item.APPLY_ATTBONUS_ELEC : localeinfo.TOOLTIP_DUNGEON_APPLY_ATTBONUS_ELEC,
		item.APPLY_ATTBONUS_FIRE : localeinfo.TOOLTIP_DUNGEON_APPLY_ATTBONUS_FIRE,
		item.APPLY_ATTBONUS_ICE : localeinfo.TOOLTIP_DUNGEON_APPLY_ATTBONUS_ICE,
		item.APPLY_ATTBONUS_WIND : localeinfo.TOOLTIP_DUNGEON_APPLY_ATTBONUS_WIND,
		item.APPLY_ATTBONUS_EARTH : localeinfo.TOOLTIP_DUNGEON_APPLY_ATTBONUS_EARTH,
		item.APPLY_ATTBONUS_DARK : localeinfo.TOOLTIP_DUNGEON_APPLY_ATTBONUS_DARK,
		item.APPLY_DESIGN_1 : localeinfo.TOOLTIP_DUNGEON_APPLY_DESIGN_1,
	}
	
	if app.ENABLE_DS_RUNE:
		AFFECT_DICT.update({item.APPLY_RUNE_MONSTERS : localeinfo.TOOLTIP_DUNGEON_APPLY_ATTBONUS_RUNE,})
	
	TOOLTIP_NORMAL_COLOR = grp.GenerateColor(0.9490, 0.9058, 0.7568, 1.0)
	TOOLTIP_SPECIAL_COLOR = grp.GenerateColor(1.0, 0.7843, 0.0, 1.0)
	ITEM_SLOT_ENABLE = "d:/ymir work/ui/pet/skill_button/skill_enable_button.sub"
	MIN_SCROLLBAR_LIST = 10

	DUNGEON_TYPE = {
		0 : localeinfo.DUNGEON_INFO_TYPE0,
		1 : localeinfo.DUNGEON_INFO_TYPE1,
		2 : localeinfo.DUNGEON_INFO_TYPE2
	}

	DUNGEON_ORGANIZATION = {
		0 : localeinfo.DUNGEON_INFO_ORGANIZATION0,
		1 : localeinfo.DUNGEON_INFO_ORGANIZATION1,
		2 : localeinfo.DUNGEON_INFO_ORGANIZATION2
	}

	DUNGEON_STATUS = {
		0 : localeinfo.DUNGEON_INFO_STATUS0,
		1 : localeinfo.DUNGEON_INFO_STATUS1,
		2 : localeinfo.DUNGEON_INFO_STATUS2
	}

	def __init__(self):
		ui.ScriptWindow.__init__(self)

		self.dungeonIndex = 0
		self.last = 0
		self.dungeonButton = {}
		self.dungeonImage = {}
		self.dungeonImageIcon = {}
		self.dungeonName = {}
		self.dungeonStatus = {}
		self.dungeonItemToolTip = None
		self.questionDialog = None
		self.dungeonRankingIndex = None
		self.itemToolTip = None
		self.isLoaded = False

	def __del__(self):
		ui.ScriptWindow.__del__(self)

		self.dungeonIndex = 0
		self.last = 0
		self.dungeonButton = {}
		self.dungeonImage = {}
		self.dungeonImageIcon = {}
		self.dungeonName = {}
		self.dungeonStatus = {}
		self.dungeonItemToolTip = None
		self.questionDialog = None
		self.dungeonRankingIndex = None
		self.itemToolTip = None
		self.isLoaded = False

	def LoadDialog(self):
		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "UIScript/dungeoninfowindow.py")
		except:
			import exception
			exception.Abort("DungeonInfo.LoadDialog.LoadScript")

		try:
			self.dungeonBoard = self.GetChild("DungeonBoard")
			self.dungeonBoardTitleBar = self.GetChild("DungeonBoardTitleBar")

			self.dungeonButtonBoard = self.GetChild("DungeonButtonBoard")
			#self.dungeonButtonThinBoard = self.GetChild("DungeonButtonThinBoard")

			self.dungeonInfoItem = self.GetChild("DungeonInfoItem")
			self.dungeonInfoItemSlot = self.GetChild("DungeonInfoItemSlot")

			self.dungeonScrollBar = self.GetChild("ScrollBar")
			self.dungeonInfoBoard = self.GetChild("DungeonInfoBoard")

			self.dungeonInfoName = self.GetChild("DungeonInfoName")
			self.dungeonInfoType = self.GetChild("DungeonInfoType")
			self.dungeonInfoOrganization = self.GetChild("DungeonInfoOrganization")
			self.dungeonInfoLevelLimit = self.GetChild("DungeonInfoLevelLimit")
			self.dungeonInfoPartyMembers = self.GetChild("DungeonInfoPartyMembers")
			self.dungeonInfoCooldown = self.GetChild("DungeonInfoCooldown")
			self.dungeonInfoDuration = self.GetChild("DungeonInfoDuration")
			self.dungeonInfoEntrance = self.GetChild("DungeonInfoEntrance")
			self.dungeonInfoStrengthBonus = self.GetChild("DungeonInfoStrengthBonus")
			self.dungeonInfoStrengthBonus.SetLimitWidth(170)
			self.dungeonInfoStrengthBonus.SetMultiLine()
			self.dungeonInfoResistanceBonus = self.GetChild("DungeonInfoResistanceBonus")
			self.dungeonInfoResistanceBonus.SetLimitWidth(170)
			self.dungeonInfoResistanceBonus.SetMultiLine()
			self.dungeonInfoTotalFinished = self.GetChild("DungeonInfoTotalFinished")
			self.dungeonInfoFastestTime = self.GetChild("DungeonInfoFastestTime")
			self.dungeonInfoHighestDamage = self.GetChild("DungeonInfoHighestDamage")

			self.dungeonInfoTeleportButton = self.GetChild("DungeonInfoTeleportButton")
			self.closeDungeonBoard = self.GetChild("CloseDungeonBoard")

			self.dungeonRank1Button = self.GetChild("DungeonRank1Button")
			self.dungeonRank2Button = self.GetChild("DungeonRank2Button")
			self.dungeonRank3Button = self.GetChild("DungeonRank3Button")

		except:
			import exception
			exception.Abort("DungeonInfo.LoadDialog.GetChild")

		self.dungeonBoardTitleBar.SetCloseEvent(ui.__mem_func__(self.Close))
		self.closeDungeonBoard.SetEvent(ui.__mem_func__(self.Close))
		self.dungeonInfoTeleportButton.SetEvent(self.TeleportDungeon)

		self.dungeonInfoItemSlot.SetOverInItemEvent(ui.__mem_func__(self.OverInItem))
		self.dungeonInfoItemSlot.SetOverOutItemEvent(ui.__mem_func__(self.OverOutItem))

		self.dungeonRank1Button.SetEvent(lambda arg = 1: self.OpenRankingBoard(arg))
		self.dungeonRank2Button.SetEvent(lambda arg = 2: self.OpenRankingBoard(arg))
		self.dungeonRank3Button.SetEvent(lambda arg = 3: self.OpenRankingBoard(arg))

		self.LoadDungeonButtons()
		self.LoadDungeonInfoBoard(self.dungeonIndex)

		self.isLoaded = True

	def __GetAffectString(self, affectType):
		try:
			return self.AFFECT_DICT[affectType]
		except:
			return localeinfo.TOOLTIP_BONUS_UNKNOWN

	def Close(self):
		#self.dungeonItemToolTip:
		#self.dungeonItemToolTip.HideToolTip()

		self.isLoaded = False
		self.Hide()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def Open(self):
		if not self.isLoaded:
			self.LoadDialog()

		self.SetCenterPosition()
		self.SetTop()
		self.Show()

	def LoadDungeonButtons(self):
		if not constinfo.dungeonInfo:
			return

		for index in xrange(min(self.MIN_SCROLLBAR_LIST, len(constinfo.dungeonInfo))):
			self.AppendDungeonButton(
				index,\
				self.dungeonButtonBoard,\
				3, 3 + (38 * index)
			)

		if len(constinfo.dungeonInfo) <= self.MIN_SCROLLBAR_LIST:
			self.dungeonScrollBar.SetMiddleBarSize(float(len(constinfo.dungeonInfo)) / float(len(constinfo.dungeonInfo)))
			self.dungeonScrollBar.Show()
		else:
			self.dungeonScrollBar.SetMiddleBarSize(float(self.MIN_SCROLLBAR_LIST) / float(len(constinfo.dungeonInfo)))
			self.dungeonScrollBar.Show()

		self.dungeonScrollBar.SetScrollEvent(self.OnScroll)

	def OnScroll(self):
		button_count = len(self.dungeonButton)
		pos = int(self.dungeonScrollBar.GetPos() * (len(constinfo.dungeonInfo) - button_count))

		self.dungeonButton = {}
		self.dungeonImage = {}
		self.dungeonImageIcon = {}
		self.dungeonName = {}
		self.dungeonStatus = {}

		for idx in xrange(min(self.MIN_SCROLLBAR_LIST, button_count)):
			realPos = idx + pos

			self.AppendDungeonButton(
				realPos,\
				self.dungeonButtonBoard,\
				3, 3 + (38 * idx)
			)

			if realPos != self.dungeonIndex:
				self.dungeonButton[realPos].SetUpVisual("d:/ymir work/ui/game/mailbox/post_default.sub")
				self.dungeonButton[realPos].SetOverVisual("d:/ymir work/ui/game/mailbox/post_over.sub")
				self.dungeonButton[realPos].SetDownVisual("d:/ymir work/ui/game/mailbox/post_select.sub")

	def OnRunMouseWheel(self, nLen):
		if not self.dungeonScrollBar.IsShow():
			return False
		
		if nLen > 0:
			self.dungeonScrollBar.OnUp()
		else:
			self.dungeonScrollBar.OnDown()
		
		return True

	def AppendDungeonButton(self, index, parent, x, y):
		self.dungeonButton[index] = ui.Button()
		self.dungeonButton[index].SetParent(parent)
		self.dungeonButton[index].SetUpVisual("d:/ymir work/ui/game/mailbox/post_select.sub")
		self.dungeonButton[index].SetOverVisual("d:/ymir work/ui/game/mailbox/post_select.sub")
		self.dungeonButton[index].SetDownVisual("d:/ymir work/ui/game/mailbox/post_select.sub")
		self.dungeonButton[index].SetPosition(x, y)
		self.dungeonButton[index].SetEvent(lambda: self.LoadDungeonInfoBoard(index))
		self.dungeonButton[index].Show()

		self.dungeonImage[index] = ui.ImageBox()
		self.dungeonImage[index].SetParent(self.dungeonButton[index])
		self.dungeonImage[index].LoadImage("d:/ymir work/ui/game/mailbox/mailbox_icon_empty.sub")
		self.dungeonImage[index].SetPosition(1, 2)
		self.dungeonImage[index].Show()

		dungeonMapIndex = constinfo.dungeonInfo[index]['map_index']
		dungeonMapName = constinfo.MapNameByIndex(dungeonMapIndex)

		self.dungeonImageIcon[index] = ui.Button()
		self.dungeonImageIcon[index].SetParent(self.dungeonImage[index])
		self.dungeonImageIcon[index].SetUpVisual("d:/ymir work/ui/game/dungeon_info/icons/%d.tga" % dungeonMapIndex)
		self.dungeonImageIcon[index].SetOverVisual("d:/ymir work/ui/game/dungeon_info/icons/%d.tga" % dungeonMapIndex)
		self.dungeonImageIcon[index].SetDownVisual("d:/ymir work/ui/game/dungeon_info/icons/%d.tga" % dungeonMapIndex)
		self.dungeonImageIcon[index].SetEvent(lambda: self.LoadDungeonInfoBoard(index))
		self.dungeonImageIcon[index].SetPosition(0, 0)
		self.dungeonImageIcon[index].Show()

		self.dungeonName[index] = ui.TextLine()
		self.dungeonName[index].SetParent(self.dungeonButton[index])
		self.dungeonName[index].SetPosition(40, 10)
		self.dungeonName[index].SetText("%s" % dungeonMapName)
		self.dungeonName[index].Show()

		self.dungeonStatus[index] = ui.TextLine()
		self.dungeonStatus[index].SetParent(self.dungeonButton[index])
		self.dungeonStatus[index].SetPosition(210 - 35, 10)
		self.dungeonStatus[index].Show()
		self.CheckDungeonStatus(index)

	def CheckDungeonStatus(self, index):
		try:
			dungeonStatus = constinfo.dungeonInfo[index]['status']
			dungeonMinLevel = constinfo.dungeonInfo[index]['level_limit'][0]
			dungeonMaxLevel = constinfo.dungeonInfo[index]['level_limit'][1]
			
			try:
				t1 = constinfo.dungeonInfo[self.dungeonIndex]['wait_time'] - app.GetGlobalTimeStamp()
				c1 = constinfo.dungeonInfo[self.dungeonIndex]['cooldown']
				if c1 > 0 and t1 > 0:
					self.dungeonInfoCooldown.SetText("%s : %s" % (uiscriptlocale.DUNGEON_INFO_COOLDOWN, self.FormatTime(t1)))
				else:
					self.dungeonInfoCooldown.SetText("%s : %s" % (uiscriptlocale.DUNGEON_INFO_COOLDOWN, self.FormatTime(c1)))
			except:
				pass
			
			if player.GetLevel() < dungeonMinLevel:
				try:
					self.dungeonStatus[index].SetText(localeinfo.DUNGEON_LEVEL_LIMIT_MIN_PASS)
					self.dungeonStatus[index].SetFontColor((255.00 / 255), (0.00 / 255), (0.00 / 255))
				except:
					pass
			elif player.GetLevel() > dungeonMaxLevel:
				try:
					self.dungeonStatus[index].SetText(localeinfo.DUNGEON_LEVEL_LIMIT_MAX_PASS)
					self.dungeonStatus[index].SetFontColor((255.00 / 255), (0.00 / 255), (0.00 / 255))
				except:
					pass
			else:
				try:
					t1 = constinfo.dungeonInfo[index]['wait_time'] - app.GetGlobalTimeStamp()
					if t1 > 0:
						self.dungeonStatus[index].SetText(localeinfo.COOLDOWN_IN_PROGRESS)
						self.dungeonStatus[index].SetFontColor((255.00 / 255), (0.00 / 255), (0.00 / 255))
					else:
						self.dungeonStatus[index].SetText(self.DUNGEON_STATUS[dungeonStatus])
						if dungeonStatus == 1:
							self.dungeonStatus[index].SetFontColor((255.00 / 255), (255.00 / 255), (51.00 / 255))
						elif dungeonStatus == 2:
							self.dungeonStatus[index].SetFontColor((255.00 / 255), (0.00 / 255), (0.00 / 255))
						else:
							self.dungeonStatus[index].SetFontColor((0.00 / 255), (255.00 / 255), (0.00 / 255))
				except:
					pass
		except:
			pass

	def LoadDungeonInfoBoard(self, index, retry = False):
		old = self.dungeonIndex
		self.dungeonIndex = index
		dungeonInfo = constinfo.dungeonInfo
		try:
			self.dungeonButton[self.dungeonIndex].SetUpVisual("d:/ymir work/ui/game/mailbox/post_select.sub")
			self.dungeonButton[self.dungeonIndex].SetOverVisual("d:/ymir work/ui/game/mailbox/post_select.sub")
			self.dungeonButton[self.dungeonIndex].SetDownVisual("d:/ymir work/ui/game/mailbox/post_select.sub")

			pos = int(self.dungeonScrollBar.GetPos() * (len(dungeonInfo) - len(self.dungeonButton)))
			for idx in xrange(len(self.dungeonButton)):
				realPos = idx + pos
				if realPos != self.dungeonIndex:
					self.dungeonButton[realPos].SetUpVisual("d:/ymir work/ui/game/mailbox/post_default.sub")
					self.dungeonButton[realPos].SetOverVisual("d:/ymir work/ui/game/mailbox/post_over.sub")
					self.dungeonButton[realPos].SetDownVisual("d:/ymir work/ui/game/mailbox/post_select.sub")

			dungeonStatus = dungeonInfo[self.dungeonIndex]['status']
			dungeonType = dungeonInfo[self.dungeonIndex]['type']
			dungeonOrganization = dungeonInfo[self.dungeonIndex]['organization']
			dungeonLevelMinLimit = dungeonInfo[self.dungeonIndex]['level_limit'][0]
			dungeonLevelMaxLimit = dungeonInfo[self.dungeonIndex]['level_limit'][1]
			dungeonPartyMemberMinLimit = dungeonInfo[self.dungeonIndex]['party_member_limit'][0]
			dungeonPartyMemberMaxLimit = dungeonInfo[self.dungeonIndex]['party_member_limit'][1]
			dungeonMapIndex = dungeonInfo[self.dungeonIndex]['map_index']
			dungeonCooldown = dungeonInfo[self.dungeonIndex]['cooldown']
			dungeonDuration = dungeonInfo[self.dungeonIndex]['duration']
			dungeonEntranceMapIndex = dungeonInfo[self.dungeonIndex]['entrance_map_index']
			dungeonStrengthBonus = self.__GetAffectString(int(dungeonInfo[self.dungeonIndex]['strength_bonus']))
			dungeonResistanceBonus = self.__GetAffectString(int(dungeonInfo[self.dungeonIndex]['resistance_bonus']))
			
			dungeonItemVnum = dungeonInfo[self.dungeonIndex]['required_item'][0]
			dungeonItemCount = dungeonInfo[self.dungeonIndex]['required_item'][1]
			dungeonFinished = dungeonInfo[self.dungeonIndex]['finished']
			dungeonFastestTime = dungeonInfo[self.dungeonIndex]['fastest_time']
			dungeonHighestDamage = dungeonInfo[self.dungeonIndex]['highest_damage']

			dungeonMapName = constinfo.MapNameByIndex(dungeonMapIndex)
			dungeonEntranceMapName = constinfo.MapNameByIndex(dungeonEntranceMapIndex)

			self.dungeonInfoBoard.LoadImage("d:/ymir work/ui/game/dungeon_info/background/%d.tga" % dungeonMapIndex)

			self.dungeonInfoName.SetText("%s" % dungeonMapName)
			self.dungeonInfoType.SetText("%s : %s" % (uiscriptlocale.DUNGEON_INFO_TYPE, self.DUNGEON_TYPE[dungeonType]))
			self.dungeonInfoOrganization.SetText("%s : %s" % (uiscriptlocale.DUNGEON_INFO_ORGANIZATION, self.DUNGEON_ORGANIZATION[dungeonOrganization]))
			self.dungeonInfoLevelLimit.SetText("%s : %d - %d" % (uiscriptlocale.DUNGEON_INFO_LEVEL_LIMIT, dungeonLevelMinLimit, dungeonLevelMaxLimit))
			self.dungeonInfoPartyMembers.SetText("%s : %d - %d" % (uiscriptlocale.DUNGEON_INFO_PARTY_MEMBERS, dungeonPartyMemberMinLimit, dungeonPartyMemberMaxLimit))
			self.dungeonInfoCooldown.SetText("%s : %s" % (uiscriptlocale.DUNGEON_INFO_COOLDOWN, self.FormatTime(dungeonCooldown)))
			self.dungeonInfoDuration.SetText("%s : %s" % (uiscriptlocale.DUNGEON_INFO_DURATION, self.FormatTime(dungeonDuration)))
			self.dungeonInfoEntrance.SetText("%s : %s" % (uiscriptlocale.DUNGEON_INFO_ENTRANCE, dungeonMapName))
			self.dungeonInfoStrengthBonus.SetText("%s : %s" % (uiscriptlocale.DUNGEON_INFO_STRENGTH, dungeonStrengthBonus))
			self.dungeonInfoResistanceBonus.SetText("%s : %s" % (uiscriptlocale.DUNGEON_INFO_RESISTANCE, dungeonResistanceBonus))
			self.dungeonInfoTotalFinished.SetText("%s : %d" % (uiscriptlocale.DUNGEON_INFO_TOTAL_FINISHED, dungeonFinished))
			self.dungeonInfoFastestTime.SetText("%s : %s" % (uiscriptlocale.DUNGEON_INFO_FASTEST_TIME, self.FormatTime(dungeonFastestTime)))
			self.dungeonInfoHighestDamage.SetText("%s : %d" % (uiscriptlocale.DUNGEON_INFO_HIGHEST_DAMAGE, dungeonHighestDamage))

			if dungeonItemVnum > 0:
				self.SetItemSlot(dungeonItemVnum, dungeonItemCount)
			else:
				self.SetItemSlot(0, 0)
		except KeyError:
			if retry:
				return
			
			self.LoadDungeonInfoBoard(old, True)

	def SetItemSlot(self, itemVnum, itemCount):
		itemIndex = itemVnum
		itemCount = itemCount

		if itemIndex == 0:
			self.dungeonInfoItemSlot.ClearSlot(0)
			self.dungeonInfoItemSlot.ShowSlotBaseImage(0)
			return

		self.dungeonInfoItemSlot.SetItemSlot(0, itemIndex, itemCount)
		self.dungeonInfoItemSlot.HideSlotBaseImage(0)
		item.SelectItem(itemVnum)

		self.dungeonItemToolTip = uitooltip.ItemToolTip()
		self.dungeonItemToolTip.AppendTextLine(item.GetItemName(), self.TOOLTIP_SPECIAL_COLOR)
		self.dungeonItemToolTip.AppendDescription(item.GetItemDescription(), 26)
		self.dungeonItemToolTip.AlignHorizonalCenter()

	def FormatTime(self, seconds):
		if seconds == 0:
			return localeinfo.DUNGEON_INFO_NONE

		m, s = divmod(seconds, 60)
		h, m = divmod(m, 60)

		return "%d:%02d:%02d" % (h, m, s)

	def OverInItem(self):
		if 0 != self.dungeonItemToolTip:
			self.dungeonItemToolTip.ShowToolTip()

	def OverOutItem(self):
		if 0 != self.dungeonItemToolTip:
			self.dungeonItemToolTip.HideToolTip()

	def TeleportDungeon(self):
		mapIndex = constinfo.dungeonInfo[self.dungeonIndex]['map_index']
		mapName = constinfo.MapNameByIndex(mapIndex)

		self.questionDialog = uicommon.QuestionDialogWithTimeLimit()
		self.questionDialog.Open(localeinfo.DUNGEON_INFO_DO_YOU_TELEPORT % mapName, 5)
		self.questionDialog.SetAcceptText(localeinfo.UI_ACCEPT)
		self.questionDialog.SetCancelText(localeinfo.UI_DENY)
		self.questionDialog.SetAcceptEvent(lambda arg = True: self.AnswerTeleport(arg))
		self.questionDialog.SetCancelEvent(lambda arg = False: self.AnswerTeleport(arg))
		self.questionDialog.SetCancelOnTimeOver()
		self.questionDialog.SetTop()

	def AnswerTeleport(self, answer):
		if not self.questionDialog:
			return

		if answer == True:
			import event

			dungeonMapCoords = constinfo.dungeonInfo[self.dungeonIndex]['map_coords']
			x, y = int(dungeonMapCoords[0]), int(dungeonMapCoords[1])

			constinfo.dungeonData["quest_cmd"] = "WARP#%d#%d" % (x, y)
			event.QuestButtonClick(int(constinfo.dungeonData["quest_index"]))

		self.questionDialog.Close()
		self.questionDialog = None

	def OpenRankingBoard(self, rankType):
		if rankType > 0:
			mapIndex = constinfo.dungeonInfo[self.dungeonIndex]['map_index']

			import event
			constinfo.dungeonData["quest_cmd"] = "RANKING#%d#%d" % (mapIndex, rankType)
			constinfo.dungeonRanking["ranking_type"] = rankType
			event.QuestButtonClick(int(constinfo.dungeonData["quest_index"]))

	def OnUpdate(self):
		if constinfo.dungeonInfo and self.dungeonIndex >= 0:
			for idx in xrange(len(self.dungeonButton)):
				self.CheckDungeonStatus(idx)

class DungeonRank(ui.ScriptWindow):
	SLOT_RANKING = 0
	SLOT_PLAYER_NAME = 1
	SLOT_PLAYER_LEVEL = 2
	SLOT_POINT_TYPE = 3

	MAX_LINE_COUNT = 5

	def __init__(self):
		ui.ScriptWindow.__init__(self)

		self.NowStartLineNumber = 0
		self.dungeonRankingScrollBar = None
		self.ResultButtonList = []
		self.ResultSlotList = {}
		self.MyResultSlotList = []

		self.isLoaded = False

	def __del__(self):
		ui.ScriptWindow.__del__(self)

		self.NowStartLineNumber = 0
		self.dungeonRankingScrollBar = None
		self.ResultButtonList = []
		self.ResultSlotList = {}
		self.MyResultSlotList = []

		self.isLoaded = False

	def LoadDialog(self):
		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "UIScript/dungeonrankingwindow.py")
		except:
			import exception
			exception.Abort("DungeonRank.LoadDialog.LoadScript")

		try:
			self.dungeonRankingTitleBar = self.GetChild("DungeonRankingTitleBar")
			self.dungeonRankingTitleName = self.GetChild("DungeonRankingTitleName")
			self.dungeonRankingScrollBar = self.GetChild("DungeonRankingScrollBar")

			self.dungeonRankingResultPosition = self.GetChild("ResultRanking")
			self.dungeonRankingResultName = self.GetChild("ResultName")
			self.dungeonRankingResultLevel = self.GetChild("ResultLevel")
			self.dungeonRankingResultPoints = self.GetChild("ResultPoints")

		except:
			import exception
			exception.Abort("DungeonRank.__LoadWindow.SetObject")

		self.dungeonRankingTitleBar.SetCloseEvent(ui.__mem_func__(self.Close))
		self.dungeonRankingScrollBar.SetScrollEvent(ui.__mem_func__(self.OnScrollControl))

		self.MakeUiBoard()
		self.RefreshDungeonRanking()

		self.isLoaded = True

	def MakeUiBoard(self):
		try:
			yPos = 0
			for i in range(0, self.MAX_LINE_COUNT+1):
				yPos = 65 + i * 24
				if i == 5:
					yPos += 10

				RankingSlotImage = ui.MakeImageBox(self, "d:/ymir work/ui/public/parameter_slot_00.sub", 23, yPos)
				RankingSlotImage.SetAlpha(0)
				RankingSlot = ui.MakeTextLine(RankingSlotImage)
				self.Children.append(RankingSlotImage)
				self.Children.append(RankingSlot)

				GuildNameImage = ui.MakeImageBox(self, "d:/ymir work/ui/public/parameter_slot_04.sub", 77, yPos)
				GuildNameImage.SetAlpha(0)
				GuildNameSlot = ui.MakeTextLine(GuildNameImage)
				self.Children.append(GuildNameImage)
				self.Children.append(GuildNameSlot)

				MemberCountSlotImage = ui.MakeImageBox(self, "d:/ymir work/ui/public/parameter_slot_00.sub", 205, yPos)
				MemberCountSlotImage.SetAlpha(0)
				MemberCountSlot = ui.MakeTextLine(MemberCountSlotImage)
				self.Children.append(MemberCountSlotImage)
				self.Children.append(MemberCountSlot)

				ClearTimeSlotImage = ui.MakeImageBox(self, "d:/ymir work/ui/public/parameter_slot_00.sub", 270, yPos)
				ClearTimeSlotImage.SetAlpha(0)
				ClearTimeSlot = ui.MakeTextLine(ClearTimeSlotImage)
				self.Children.append(ClearTimeSlotImage)
				self.Children.append(ClearTimeSlot)

				if i < self.MAX_LINE_COUNT:
					tempguildlankingslotlist = []
					tempguildlankingslotlist.append(RankingSlot)

					tempguildlankingslotlist.append(GuildNameSlot)
					tempguildlankingslotlist.append(MemberCountSlot)
					tempguildlankingslotlist.append(ClearTimeSlot)
					self.ResultSlotList[i] = tempguildlankingslotlist
				else:
					self.MyResultSlotList.append(RankingSlot)
					self.MyResultSlotList.append(GuildNameSlot)
					self.MyResultSlotList.append(MemberCountSlot)
					self.MyResultSlotList.append(ClearTimeSlot)

				itemSlotButtonImage = ui.MakeButton(self, 21, yPos, "", "d:/ymir work/ui/game/guild/dragonlairranking/", "ranking_list_button01.sub", "ranking_list_button02.sub", "ranking_list_button02.sub")
				itemSlotButtonImage.Show()
				itemSlotButtonImage.Disable()
				self.Children.append(itemSlotButtonImage)

				if i < self.MAX_LINE_COUNT:
					self.ResultButtonList.append(itemSlotButtonImage)

		except:
			import exception
			exception.Abort("GuildWindow_GuildDragonLairWindow.MakeUiBoard")

	def RefreshDungeonRanking(self):
		self.AllClear()

		dungeonRankingType = constinfo.dungeonRanking["ranking_type"]
		dungeonRankingList = constinfo.dungeonRanking["ranking_list"]
		dungeonMyRanking = constinfo.dungeonRanking["my_ranking"]

		if not dungeonRankingList:
			return

		if dungeonRankingType == 1:
			self.dungeonRankingTitleName.SetText(uiscriptlocale.DUNGEON_RANKING_TYPE1)
			self.dungeonRankingResultPoints.SetText(uiscriptlocale.DUNGEON_RANKING_POINT_TYPE1)
		elif dungeonRankingType == 2:
			self.dungeonRankingTitleName.SetText(uiscriptlocale.DUNGEON_RANKING_TYPE2)
			self.dungeonRankingResultPoints.SetText(uiscriptlocale.DUNGEON_RANKING_POINT_TYPE2)
		elif dungeonRankingType == 3:
			self.dungeonRankingTitleName.SetText(uiscriptlocale.DUNGEON_RANKING_TYPE3)
			self.dungeonRankingResultPoints.SetText(uiscriptlocale.DUNGEON_RANKING_POINT_TYPE3)

		for line, ResultSlotList in self.ResultSlotList.items():
			nowindex = line + self.NowStartLineNumber

			if nowindex >= len(dungeonRankingList):
				break

			rankingData = dungeonRankingList[nowindex]

			ResultSlotList[self.SLOT_RANKING].SetText(str(nowindex+1))
			ResultSlotList[self.SLOT_PLAYER_NAME].SetText(str(rankingData[0]))
			ResultSlotList[self.SLOT_PLAYER_LEVEL].SetText(str(rankingData[1]))
			if dungeonRankingType == 2:
				ResultSlotList[self.SLOT_POINT_TYPE].SetText(self.FormatTime(rankingData[2]))
			else:
				ResultSlotList[self.SLOT_POINT_TYPE].SetText(str(rankingData[2]))
			self.ResultButtonList[line].Show()

		if len(dungeonMyRanking) > 0:
			myrankingData = dungeonMyRanking[0]

			self.MyResultSlotList[self.SLOT_RANKING].SetText(str(myrankingData[0]))
			self.MyResultSlotList[self.SLOT_PLAYER_NAME].SetText(str(myrankingData[1]))
			self.MyResultSlotList[self.SLOT_PLAYER_LEVEL].SetText(str(myrankingData[2]))
			if dungeonRankingType == 2:
				self.MyResultSlotList[self.SLOT_POINT_TYPE].SetText(self.FormatTime(myrankingData[3]))
			else:
				self.MyResultSlotList[self.SLOT_POINT_TYPE].SetText(str(myrankingData[3]))
		else:
			self.MyResultSlotList[self.SLOT_RANKING].SetText("-")
			self.MyResultSlotList[self.SLOT_PLAYER_NAME].SetText("-")
			self.MyResultSlotList[self.SLOT_PLAYER_LEVEL].SetText("-")
			self.MyResultSlotList[self.SLOT_POINT_TYPE].SetText("-")

		self.dungeonRankingScrollBar.SetMiddleBarSize(float(self.MAX_LINE_COUNT) / float(self.CheckNowItemCount()))

	def FormatTime(self, seconds):
		if seconds == 0:
			return 0

		m, s = divmod(seconds, 60)
		h, m = divmod(m, 60)

		return "%d:%02d:%02d" % (h, m, s)

	def AllClear(self):
		for line, ResultSlotList in self.ResultSlotList.items():
			ResultSlotList[self.SLOT_RANKING].SetText("")
			ResultSlotList[self.SLOT_PLAYER_NAME].SetText("")
			ResultSlotList[self.SLOT_PLAYER_LEVEL].SetText("")
			ResultSlotList[self.SLOT_POINT_TYPE].SetText("")
			self.ResultButtonList[line].SetUp()
			self.ResultButtonList[line].Hide()

		self.MyResultSlotList[self.SLOT_RANKING].SetText("-")
		self.MyResultSlotList[self.SLOT_PLAYER_NAME].SetText("-")
		self.MyResultSlotList[self.SLOT_PLAYER_LEVEL].SetText("-")
		self.MyResultSlotList[self.SLOT_POINT_TYPE].SetText("-")

	def Close(self):
		self.isLoaded = False
		self.Hide()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def CheckNowItemCount(self):
		if len(constinfo.dungeonRanking["ranking_list"]) <= self.MAX_LINE_COUNT:
			return self.MAX_LINE_COUNT
		else:
			return len(constinfo.dungeonRanking["ranking_list"])

	def OnScrollControl(self):
		nowitemcount = 0
		if len(constinfo.dungeonRanking["ranking_list"]) <= self.MAX_LINE_COUNT :
			nowitemcount = 0
		else:
			nowitemcount = (len(constinfo.dungeonRanking["ranking_list"]) - self.MAX_LINE_COUNT)

		pos = self.dungeonRankingScrollBar.GetPos() * nowitemcount

		if not int(pos) == self.NowStartLineNumber:
			self.NowStartLineNumber = int(pos)
			self.RefreshDungeonRanking()

	def Open(self):
		if not self.isLoaded:
			self.LoadDialog()

		self.NowStartLineNumber = 0
		self.dungeonRankingScrollBar.SetPos(0)

		self.SetCenterPosition()
		self.SetTop()
		self.Show()

	def OnRunMouseWheel(self, nLen):
		if not self.dungeonRankingScrollBar or self.dungeonRankingScrollBar and not self.dungeonRankingScrollBar.IsShow():
			return False
		
		if self.IsInPosition():
			if nLen > 0:
				self.dungeonRankingScrollBar.OnUp()
			else:
				self.dungeonRankingScrollBar.OnDown()
			
			return True
		else:
			return False
