import app
import net
import player
import ui, constinfo
import datetime
import chat
import dbg
import wndMgr
import localeInfo
import re
from uiToolTip import ItemToolTip
IMG_DIR = "d:/ymir work/ui/game/event_calendar/"
IMG_ICON_DIR = "d:/ymir work/ui/game/event_calendar/icons/"
MINI_IMG_ICON_DIR = "d:/ymir work/ui/game/event_calendar/icons/mini_gui/"


events_default_data = {
	# img , eventName
	player.BONUS_EVENT:["bonus_event",localeInfo.BONUS_EVENT],
	player.DOUBLE_BOSS_LOOT_EVENT:["double_boss_loot_event",localeInfo.DOUBLE_BOSS_LOOT_EVENT],
	player.DOUBLE_METIN_LOOT_EVENT:["double_metin_loot_event", localeInfo.DOUBLE_METIN_LOOT_EVENT],
	player.DOUBLE_MISSION_BOOK_EVENT:["double_mission_book_event",localeInfo.DOUBLE_MISSION_BOOK_EVENT],
	player.DUNGEON_COOLDOWN_EVENT:["dungeon_cooldown_event",localeInfo.DUNGEON_COOLDOWN_EVENT],
	player.DUNGEON_TICKET_LOOT_EVENT:["dungeon_ticket_loot_event",localeInfo.DUNGEON_TICKET_LOOT_EVENT],
	player.EMPIRE_WAR_EVENT:["empire_war_event",""],
	player.MOONLIGHT_EVENT:["moonlight_event",localeInfo.MOONLIGHT_EVENT],
	player.TOURNAMENT_EVENT:["tournament_event",""],
	player.WHELL_OF_FORTUNE_EVENT:["whell_of_fortune_event",localeInfo.WHELL_OF_FORTUNE_EVENT],
	player.HALLOWEEN_EVENT:["halloween_event",localeInfo.HALLOWEEN_EVENT],
	player.NPC_SEARCH_EVENT:["npc_search",""],

	player.ITEM_DROP_EVENT:["item_drop_event",localeInfo.ITEM_DROP_EVENT],
	player.YANG_DROP_EVENT:["money_drop_event",localeInfo.YANG_DROP_EVENT],
	player.EXP_EVENT:["exp_event",localeInfo.EXP_EVENT],
	player.HATSZOG_LADA_EVENT:["hatszog_event",localeInfo.HATSZOG_LADA_EVENT],
	player.BUPLA_RUN_BOSS_LOOT_EVENT:["runboss_event",localeInfo.BUPLA_RUN_BOSS_LOOT_EVENT],

	player.MIKI_EVENT:["miki_event",localeInfo.MIKI_EVENT],
	player.KARI_EVENT:["kari_event",localeInfo.KARI_EVENT],
	player.DUPLA_SZILI_EVENT:["dupla_szili_event",localeInfo.DUPLA_SZILI_EVENT],
	player.COIN_EVENT:["coin_event",localeInfo.COIN_EVENT],
	player.DUPLA_BOSS_PONT_EVENT:["dupla_boss_pont_event",localeInfo.DUPLA_BOSS_PONT_EVENT],
	player.DUPLA_RUN_PONT_EVENT:["dupla_run_pont_event",localeInfo.DUPLA_RUN_PONT_EVENT],
	player.VALENTIN_EVENT:["valentin_event",localeInfo.VALENTIN_EVENT],
	player.GOLDEN_FROG_EVENT:["golden_frog_event",localeInfo.GOLDEN_FROG_EVENT],
	player.TANAKA_EVENT:["tanaka_event",localeInfo.TANAKA_EVENT],
	player.EASTER_EVENT:["easter_event",localeInfo.EASTER_EVENT],
	player.LELEKGOMB_EVENT:["lelekgomb_event",localeInfo.LELEKGOMB_EVENT],
}

#Dont touch!
def CalculateDayCount(month, year):
	if month == 2:
		if ((year%400==0) or (year%4==0 and year%100!=0)):
			return 29
		else:
			return 28
	elif month == 1 or month == 3 or month == 5 or month == 7 or month == 8 or month == 10 or month==12:
		return 31
	else:
		return 30
EVENT_DAY_INDEX = 0
EVENT_ID = 1
EVENT_INDEX = 2
EVENT_START_TEXT = 3
EVENT_END_TEXT = 4
EVENT_EMPIRE_FLAG = 5
EVENT_CHANNEL_FLAG = 6
EVENT_VALUE0 = 7
EVENT_VALUE1 = 8
EVENT_VALUE2 = 9
EVENT_VALUE3 = 10
EVENT_START_TIME = 11
EVENT_END_TIME = 12
EVENT_IS_ACTIVE = 13
server_event_data = {}


class EventCalendarToolTip(ui.ThinBoard):
	"""Event calendar hover tooltip built from UI elements (not ascii text)."""
	ROW_H = 16
	SEP_H = 2
	TITLE_Y = 8
	HEADER_Y = 28
	ROWS_Y = 46
	PADDING_BOT = 10
	MAX_ROWS = 18

	# columns: Event | Time
	COL_EVENT_X = 10
	COL_TIME_X = 380

	COLOR_TITLE = 0xFFFFFFFF
	COLOR_HEADER = 0xFFBEBEBE
	COLOR_FUTURE = 0xFFFFFF99  # light yellow
	COLOR_ACTIVE = 0xFF66FF66  # green
	COLOR_ENDED  = 0xFFFF6666  # red
	COLOR_MORE   = 0xFFBEBEBE

	def __init__(self):
		ui.ThinBoard.__init__(self)
		self.AddFlag('float')
		self.AddFlag('not_pick')
		self._followMouse = False
		self._rows = []  # [[TextLine, TextLine], ...]
		self._seps = []  # row separators (ui.Bar)

		self.titleText = ui.TextLine()
		self.titleText.SetParent(self)
		self.titleText.SetPosition(10, self.TITLE_Y)
		self.titleText.SetOutline()
		self.titleText.Show()

		self.headerEvent = ui.TextLine()
		self.headerEvent.SetParent(self)
		self.headerEvent.SetPosition(self.COL_EVENT_X, self.HEADER_Y)
		self.headerEvent.SetText('Event')
		self.headerEvent.SetOutline()
		self._SetColor(self.headerEvent, self.COLOR_HEADER)
		self.headerEvent.Show()

		self.headerTime = ui.TextLine()
		self.headerTime.SetParent(self)
		self.headerTime.SetPosition(self.COL_TIME_X, self.HEADER_Y)
		self.headerTime.SetText('Time')
		self.headerTime.SetOutline()
		self._SetColor(self.headerTime, self.COLOR_HEADER)
		self.headerTime.Show()

		self.moreText = ui.TextLine()
		self.moreText.SetParent(self)
		self.moreText.SetOutline()
		self._SetColor(self.moreText, self.COLOR_MORE)
		self.moreText.Hide()

		self.SetSize(520, 80)
		self.Hide()

	def _SetColor(self, textline, packed):
		if hasattr(textline, 'SetPackedFontColor'):
			textline.SetPackedFontColor(packed)
		elif hasattr(textline, 'SetFontColor'):
			r = float((packed >> 16) & 0xFF) / 255.0
			g = float((packed >> 8) & 0xFF) / 255.0
			b = float(packed & 0xFF) / 255.0
			textline.SetFontColor(r, g, b)

	def _EnsureRow(self, idx):
		while len(self._rows) <= idx:
			cols = []
			for _ in xrange(2):
				t = ui.TextLine()
				t.SetParent(self)
				t.SetOutline()
				t.Hide()
				cols.append(t)
			self._rows.append(cols)

	def _EnsureSep(self, idx):
		while len(self._seps) <= idx:
			bar = ui.Bar()
			bar.SetParent(self)
			bar.SetColor(0xB0FFFFFF)  # light divider
			bar.SetSize(1, self.SEP_H)
			bar.Hide()
			self._seps.append(bar)

	def Clear(self):
		for cols in self._rows:
			for t in cols:
				t.Hide()
		for bar in self._seps:
			bar.Hide()
		self.moreText.Hide()

	def Build(self, title, rows):
		"""rows: [(eventName, timeText, packedColor), ...]"""
		self.Clear()
		self.titleText.SetText(title)
		self._SetColor(self.titleText, self.COLOR_TITLE)

		visible = rows
		moreCount = 0
		if len(rows) > self.MAX_ROWS:
			visible = rows[:self.MAX_ROWS]
			moreCount = len(rows) - self.MAX_ROWS

		for i, (ev, tm, col) in enumerate(visible):
			self._EnsureRow(i)
			y = self.ROWS_Y + i * self.ROW_H
			# event
			self._rows[i][0].SetPosition(self.COL_EVENT_X, y)
			self._rows[i][0].SetText(ev)
			self._SetColor(self._rows[i][0], col)
			self._rows[i][0].Show()
			# time
			self._rows[i][1].SetPosition(self.COL_TIME_X, y)
			self._rows[i][1].SetText(tm)
			self._SetColor(self._rows[i][1], col)
			self._rows[i][1].Show()

		# separators between rows (draw after sizing)
		# NOTE: keep them exactly between the rows
		need_seps = max(0, len(visible) - 1)
		for i in xrange(need_seps):
			self._EnsureSep(i)
			y = self.ROWS_Y + (i + 1) * self.ROW_H - self.SEP_H
			self._seps[i].SetPosition(8, y)
			self._seps[i].SetSize(self.GetWidth() - 16, self.SEP_H)
			self._seps[i].Show()
		for i in xrange(need_seps, len(self._seps)):
			self._seps[i].Hide()

		extraH = 0
		if moreCount > 0:
			y = self.ROWS_Y + len(visible) * self.ROW_H
			self.moreText.SetPosition(self.COL_EVENT_X, y)
			self.moreText.SetText('... +%d' % moreCount)
			self.moreText.Show()
			extraH = self.ROW_H

		h = self.ROWS_Y + len(visible) * self.ROW_H + extraH + self.PADDING_BOT
		if h < 80:
			h = 80
		self.SetSize(520, h)

	def _ClampAndSetPos(self, x, y):
		sw = wndMgr.GetScreenWidth()
		sh = wndMgr.GetScreenHeight()
		w = self.GetWidth()
		h = self.GetHeight()
		# clamp inside screen
		if x + w + 5 > sw:
			x = sw - w - 5
		if y + h + 5 > sh:
			y = sh - h - 5
		if x < 0:
			x = 0
		if y < 0:
			y = 0
		self.SetPosition(x, y)

	def _GetMouseAnchoredPos(self, mx, my):
		"""Return (x,y) so mouse is at TOP-CENTER of tooltip.
		Prefer below-cursor; if it would go off-screen, place above.
		"""
		sw = wndMgr.GetScreenWidth()
		sh = wndMgr.GetScreenHeight()
		w = self.GetWidth()
		h = self.GetHeight()
		x = int(mx - (w // 2))
		y = int(my + 18)
		# if below would clip, show above
		if y + h + 5 > sh:
			y = int(my - h - 18)
		return x, y

	def ShowAtMouse(self, follow=True):
		try:
			mx, my = wndMgr.GetMousePosition()
		except:
			mx, my = 0, 0
		x, y = self._GetMouseAnchoredPos(mx, my)
		self._ClampAndSetPos(x, y)
		self._followMouse = follow
		self.SetTop()
		self.Show()

	def HideToolTip(self):
		self._followMouse = False
		self.Hide()

	def OnUpdate(self):
		# keep tooltip anchored to mouse (top-center) while hovering
		if self._followMouse and self.IsShow():
			try:
				mx, my = wndMgr.GetMousePosition()
			except:
				return
			x, y = self._GetMouseAnchoredPos(mx, my)
			self._ClampAndSetPos(x, y)
def SetEventStatus(eventID, eventStatus, endTime, endTimeText):
	for dayIndex, eventList in server_event_data.items():
		if eventList.has_key(eventID):
			eventList[eventID][EVENT_IS_ACTIVE] = eventStatus
			eventList[eventID][EVENT_END_TIME] = endTime
			eventList[eventID][EVENT_END_TEXT] = endTimeText
def SetServerData(dayIndex, eventID, eventIndex, startTime, endTime, empireFlag, channelFlag, value0, value1, value2, value3, startRealTime, endRealTime, isAlreadyStart):
	if server_event_data.has_key(dayIndex):
		server_event_data[dayIndex][eventID] = [dayIndex, eventID, eventIndex, startTime, endTime, empireFlag, channelFlag, value0, value1, value2, value3, startRealTime, endRealTime, isAlreadyStart]
	else:
		eventList = {}
		eventList[eventID] = [dayIndex, eventID, eventIndex, startTime, endTime, empireFlag, channelFlag, value0, value1, value2, value3, startRealTime, endRealTime, isAlreadyStart]
		server_event_data[dayIndex] = eventList
def IsEventIDActive(eventID):
	for dayIndex, eventList in server_event_data.items():
		if eventList.has_key(eventID):
			return eventList[EVENT_IS_ACTIVE]
	return False
def IsEventIndexActive(eventIndex):
	for dayIndex, eventList in server_event_data.items():
		for eventID, eventData in eventList.items():
			if eventList[EVENT_IS_ACTIVE]:
				return eventList[EVENT_INDEX] == eventIndex
	return False
def GetEventIndexData(eventIndex):
	for dayIndex, eventList in server_event_data.items():
		for eventID, eventData in eventList.items():
			if eventList[EVENT_INDEX] == eventIndex:
				return eventData
	return None
def GetEventIDData(eventID):
	for dayIndex, eventList in server_event_data.items():
		if eventList.has_key(eventID):
			return eventList[eventID]
	return None
#Dont touch!

class ImageBoxSpecial(ui.ImageBox):
	def __del__(self):
		ui.ImageBox.__del__(self)
	def Destroy(self):
		self.miniIcon = None
		self.eventList=[]
		self.imageIndex = 0
		self.isMiniIcon = False

		self.waitingTime = 0.0
		self.sleepTime = 0.0
		self.alphaValue = 0.0
		self.increaseValue = 0.0
		self.minAlpha = 0.0
		self.maxAlpha = 0.0
		self.alphaStatus = False

	def __init__(self, isMiniIcon = False):
		ui.ImageBox.__init__(self)
		self.Destroy()
		self.isMiniIcon = isMiniIcon

		self.waitingTime = 2.0
		self.alphaValue = 0.3
		self.increaseValue = 0.05
		self.minAlpha = 0.3
		self.maxAlpha = 1.0

		if isMiniIcon:
			self.AddFlag("attach")
			self.AddFlag("movable")
			#self.SetSize(57, 57)
			(x,y) = (wndMgr.GetScreenWidth()-150, 200)
			self.SetPosition(x, y)
			self.SetEvent(ui.__mem_func__(self.OnClickEventIcon),"mouse_click")
			self.SetMouseLeftButtonDoubleClickEvent(ui.__mem_func__(self.OnClickDouble))
			self.SetMouseRightButtonDownEvent(ui.__mem_func__(self.NextEventWithKey))

	def OnMoveWindow(self, x, y):
		(screenWidth, screenHeight) = (wndMgr.GetScreenWidth(), wndMgr.GetScreenHeight())

		if x < 0:
			x = 0
		elif x+self.GetWidth() >= screenWidth-70:
			x = screenWidth-80 - self.GetWidth()

		if y < 0:
			y = 0
		elif y+self.GetHeight() >= screenHeight-100:
			y = screenHeight-100 - self.GetHeight()

		self.SetPosition(x, y)

	def NextEventWithKey(self):
		if len(self.eventList) > 1:
			self.sleepTime = 0
			self.alphaValue = self.maxAlpha
			self.alphaStatus = True

	def SetBackgroundImage(self, image):
		self.LoadImage(image)
		self.SetStringEvent("MOUSE_OVER_IN",ui.__mem_func__(self.OverInItem))
		self.SetStringEvent("MOUSE_OVER_OUT",ui.__mem_func__(self.OverOutItem))
		self.SetEvent(ui.__mem_func__(self.OnClickEventIcon),"mouse_click")

	def OnClickEventIcon(self):
		(_index, eventID) = self.GetNextImage(self.imageIndex)
		if GetEventIDData(eventID) != None:
			if GetEventIDData(eventID)[EVENT_IS_ACTIVE] == True:
				interface = constinfo.GetInterfaceInstance()
				if interface:
					if interface.wndEventManager:
						interface.wndEventManager.OnClick(GetEventIDData(eventID)[EVENT_INDEX])

	def SetImage(self, folder):
		if self.miniIcon == None:
			miniIcon = ui.ImageBox()
			miniIcon.SetParent(self)
			miniIcon.AddFlag("not_pick")
			self.miniIcon = miniIcon

		if self.isMiniIcon == True:
			self.miniIcon.LoadImage(MINI_IMG_ICON_DIR+folder+".tga")
		else:
			self.miniIcon.LoadImage(IMG_ICON_DIR+folder+".tga")
		self.miniIcon.SetPosition(6, 6)
		self.miniIcon.Show()

		if self.isMiniIcon:
			self.SetSize(57,57)

		self.alphaValue = self.minAlpha
		self.alphaStatus = False

	def OnClickDouble(self):
		interface = constinfo.GetInterfaceInstance()
		if interface:
			interface.OpenEventCalendar()
	def OverOutItem(self):
		interface = constinfo.GetInterfaceInstance()
		if interface:
			# hide our custom event tooltip
			if hasattr(interface, 'wndEventManager') and interface.wndEventManager:
				if hasattr(interface.wndEventManager, 'HideEventToolTip'):
					interface.wndEventManager.HideEventToolTip()
			# hide default tooltip too
			if interface.tooltipItem:
				interface.tooltipItem.HideToolTip()
	def OverInItem(self):
		interface = constinfo.GetInterfaceInstance()
		if interface:
			if interface.wndEventManager:
				interface.wndEventManager.OverInItem(self.dayIndex)
	def Clear(self):
		self.miniIcon = None
		self.eventList = []
		if self.isMiniIcon:
			self.SetSize(0,0)
			
	def DeleteImage(self, eventIndex):
		del self.eventList[eventIndex]
		if self.isMiniIcon and len(self.eventList) == 0:
			self.SetSize(0,0)

	def AppendImage(self, eventID):
		if eventID in self.eventList:
			return

		self.eventList.append(eventID)

		data = GetEventIDData(eventID)
		eventIndex = data[EVENT_INDEX]
		value0 = data[EVENT_VALUE0]  # APPLY_XXX

		# Bonus Event egyedi ikonválasztás APPLY alapján
		if eventIndex == player.BONUS_EVENT:

			icon_name = None

			if value0 == 43:				# EXP
				icon_name = "exp_event"

			elif value0 == 44:				# Dupla Yang
				icon_name = "yang_event"

			elif value0 == 45:				# Dupla Item
				icon_name = "item_drop_event"

			elif value0 == 53:				# Támadóérték
				icon_name = "tamado_ertek"

			elif value0 == 116:				# Metin erő
				icon_name = "metin_damage"

			elif value0 == 117:				# Főszörny erő
				icon_name = "boss_ero"

			elif value0 == 119:				# Átlag sebzés
				icon_name = "pvm_damage"

			else:
				# alapértelmezett bonus event ikon
				icon_name = events_default_data[eventIndex][0]

			self.SetImage(icon_name)

		else:
			# normál event ikon
			if GetEventIDData(eventID) != None:
				self.SetImage(events_default_data[eventIndex][0])

		if self.isMiniIcon:
			self.SetSize(57, 57)

		if self.isMiniIcon:
			self.SetSize(57,57)

	def GetNextImage(self, listIndex):
		if listIndex >= len(self.eventList):
			if len(self.eventList) > 0:
				return (0,self.eventList[0])
			return (0,0)
		return (listIndex, self.eventList[listIndex])
	
	def OnUpdate(self):
		if len(self.eventList) <= 1:
			self.imageIndex=0
			return
		elif self.sleepTime > app.GetTime():
			return
		if self.alphaStatus == True:
			self.alphaValue -= self.increaseValue
			if self.alphaValue < self.minAlpha:
				self.alphaValue = self.minAlpha
				self.alphaStatus = False
				(imageIndex, eventID) = self.GetNextImage(self.imageIndex+1)
				if GetEventIDData(eventID) != None:
					self.SetImage(events_default_data[GetEventIDData(eventID)[EVENT_INDEX]][0])
				self.imageIndex = imageIndex
		else:
			self.alphaValue += self.increaseValue
			if self.alphaValue > self.maxAlpha:
				self.alphaStatus = True
				self.sleepTime = app.GetTime()+self.waitingTime
		if self.miniIcon != None:
			self.miniIcon.SetAlpha(self.alphaValue)

class EventCalendarWindow(ui.BoardWithTitleBar):
	def __del__(self):
		ui.BoardWithTitleBar.__del__(self)
	def Destroy(self):
		self.children = {}
		self.currentMonth =0
		self.currentYear = 0
		self.eventToolTip = None
	def __init__(self):
		ui.BoardWithTitleBar.__init__(self)
		self.Destroy()
		self.__LoadWindow()
		self.eventToolTip = EventCalendarToolTip()

	def __LoadWindow(self):
		dt = datetime.datetime.today()
		(self.currentMonth,self.currentYear) = (dt.month,dt.year)
		dayCount = CalculateDayCount(self.currentMonth, self.currentYear)

		board = ui.ImageBox()
		board.SetParent(self)
		board.AddFlag("not_pick")
		board.LoadImage(IMG_DIR+"board.tga")
		board.SetPosition(8, 28)
		board.Show()
		self.children["board"] = board

		self.SetSize(8+board.GetWidth()+8, 294)
		self.AddFlag("movable")
		self.AddFlag("attach")
		self.SetTitleName(localeInfo.EVENT_MANAGER_WINDOW_TITLE % (self.__GetMonthName(self.currentMonth),self.currentYear))
		self.SetCloseEvent(self.Close)
		self.SetCenterPosition()

		for day in xrange(dayCount):
			yCalculate = day/8
			xCalculate = day-(yCalculate*8)

			dayImages = ImageBoxSpecial(False)
			dayImages.SetParent(board)
			if dt.day == day+1:
				dayImages.SetBackgroundImage(IMG_DIR+"today_bg.tga")
			else:
				dayImages.SetBackgroundImage(IMG_DIR+"black_bg.tga")
			dayImages.SetPosition(8 + (xCalculate*66),8+(yCalculate*62))
			dayImages.dayIndex = day+1
			dayImages.Show()
			self.children["dayImages%d"%day] = dayImages

			dayIndex = ui.NumberLine()
			dayIndex.SetParent(dayImages)
			dayIndex.SetNumber(str(day+1))
			dayIndex.SetPosition(8,8)
			dayIndex.Show()
			self.children["dayIndex%d"%day] = dayIndex
		self.Refresh()
	def Open(self):
		self.Show()
		self.Refresh()
		self.SetTop()
	def Close(self):
		self.Hide()
		self.HideEventToolTip()
	
	def HideEventToolTip(self):
		if self.eventToolTip:
			self.eventToolTip.HideToolTip()

	def OnPressEscapeKey(self):
		self.Close()
		return True
	def OnUpdate(self):
		for dayIndex in xrange(31):
			if self.children.has_key("dayImages%d"%dayIndex):
				self.children["dayImages%d"%dayIndex].OnUpdate()

	def Refresh(self):
		dt = datetime.datetime.today()
		for dayIndex in xrange(31):
			if self.children.has_key("dayImages%d"%dayIndex):
				dayEventImage = self.children["dayImages%d"%dayIndex]
				dayEventImage.Clear()
				if server_event_data.has_key(dayIndex+1):
					eventDict = server_event_data[dayIndex+1]
					if dt.day == dayIndex+1:
						dayEventImage.SetBackgroundImage(IMG_DIR+"today_bg.tga")
					else:
						dayEventImage.SetBackgroundImage(IMG_DIR+"blue_bg.tga")
					for eventID, _data in eventDict.items():
						dayEventImage.AppendImage(eventID)
				else:
					if dt.day == dayIndex+1:
						dayEventImage.SetBackgroundImage(IMG_DIR+"today_bg.tga")
					else:
						dayEventImage.SetBackgroundImage(IMG_DIR+"black_bg.tga")
				dayEventImage.Show()
	def OnClick(self, eventIndex):
		# Set Here!
		pass

	def __textToColorFull(self, text):
		return localeInfo.EVENT_COLORFULL_TEXT % text
	def __GetBonusName(self, affect, value):
		return ItemToolTip.AFFECT_DICT[affect](value)
	def __CalculateTime(self, eventIndex, startTimeText, endTimeText, endTime):
		if endTimeText == "1970-01-01 03:00:00" or endTimeText == "1970-01-01 02:00:00" or endTimeText == "1970-01-01 01:00:00" or endTimeText == "1970-01-01 00:00:00" or endTimeText == "":
			startTimeSecond = startTimeText.split(" ")[1]
			return localeInfo.PVP_EVENT_TIME % self.__textToColorFull(startTimeSecond)
		startTimeFirst = startTimeText.split(" ")[0]
		startTimeSecond = startTimeText.split(" ")[1]
		endTimeFirst = endTimeText.split(" ")[0]
		endTimeSecond = endTimeText.split(" ")[1]
		beginTimeText = ""
		endTimeText = ""
		if startTimeFirst != endTimeFirst:
			beginTimeText += startTimeFirst.split("-")[2]+"/"+startTimeFirst.split("-")[1]
			beginTimeText+=" "
			endTimeText += endTimeFirst.split("-")[2]+"/"+endTimeFirst.split("-")[1]
			endTimeText+=" "
		beginTimeText+=startTimeSecond
		endTimeText+=endTimeSecond
		return localeInfo.NORMAL_EVENT_TIME % (self.__textToColorFull(beginTimeText),self.__textToColorFull(endTimeText))
	def __GetMonthName(self, monthIndex):
		monthName = {
			1:localeInfo.EVENT_MONTH_1,
			2:localeInfo.EVENT_MONTH_2,
			3:localeInfo.EVENT_MONTH_3,
			4:localeInfo.EVENT_MONTH_4,
			5:localeInfo.EVENT_MONTH_5,
			6:localeInfo.EVENT_MONTH_6,
			7:localeInfo.EVENT_MONTH_7,
			8:localeInfo.EVENT_MONTH_8,
			9:localeInfo.EVENT_MONTH_9,
			10:localeInfo.EVENT_MONTH_10,
			11:localeInfo.EVENT_MONTH_11,
			12:localeInfo.EVENT_MONTH_12
		}
		if monthName.has_key(monthIndex):
			return monthName[monthIndex]
		return "Unknown Month Name"
	def __GetMapName(self, mapIndex):
		mapNames = {
			61:localeInfo.MOUNT_SOHAN_MAP_NAME,
			62:localeInfo.MOUNT_DOYUMHWAN_MAP_NAME,
			63:localeInfo.MOUNT_YONGBI_MAP_NAME,
		}
		if mapNames.has_key(mapIndex):
			return mapNames[mapIndex]
		return "Unknown Map Name"

	def __BuildEventNamePlain(self, eventData):
		idx = eventData[EVENT_INDEX]
		if idx == player.BONUS_EVENT:
			if eventData[EVENT_VALUE0] > 0 and eventData[EVENT_VALUE1] > 0:
				return "%s: %s" % (events_default_data[idx][1], self.__GetBonusName(eventData[EVENT_VALUE0], eventData[EVENT_VALUE1]))
			return "%s" % events_default_data[idx][1]
		elif idx == player.EMPIRE_WAR_EVENT:
			return localeInfo.EMPIRE_WAR_EVENT % (eventData[EVENT_VALUE0], eventData[EVENT_VALUE1])
		elif idx == player.TOURNAMENT_EVENT:
			warType = [localeInfo.TOURNAMENT_ALL_CHARACTER, localeInfo.CHARACTER_WARRIOR, localeInfo.CHARACTER_ASSASSIN, localeInfo.CHARACTER_SURA, localeInfo.CHARACTER_SHAMAN]
			wt = eventData[EVENT_VALUE0]
			if wt >= len(warType):
				wt = 0
			return localeInfo.TOURNAMENT_EVENT % (warType[wt], eventData[EVENT_VALUE1], eventData[EVENT_VALUE2])
		elif idx in (player.ITEM_DROP_EVENT, player.YANG_DROP_EVENT, player.EXP_EVENT):
			try:
				return events_default_data[idx][1] % eventData[EVENT_VALUE0]
			except:
				return events_default_data[idx][1]
		elif idx == player.NPC_SEARCH_EVENT:
			# append maps in the same line
			maps = []
			for j in xrange(4):
				mi = eventData[EVENT_VALUE0 + j]
				if mi > 0:
					maps.append(self.__GetMapName(mi))
			if maps:
				return "%s: %s" % (localeInfo.NPC_SEARCH, ", ".join(maps))
			return localeInfo.NPC_SEARCH
		return events_default_data[idx][1]

	def __FormatTimeShort(self, startText, endText):
		# output: HH:MM-HH:MM or DD/MM HH:MM -> DD/MM HH:MM
		if not startText:
			return ""
		# some servers send these 'empty end' placeholders
		if endText in ("", "1970-01-01 00:00:00", "1970-01-01 01:00:00", "1970-01-01 02:00:00", "1970-01-01 03:00:00"):
			try:
				return startText.split(" ")[1][:5]
			except:
				return startText
		try:
			sd, st = startText.split(" ")
			ed, et = endText.split(" ")
		except:
			return startText
		st = st[:5]
		et = et[:5]
		if sd == ed:
			return "%s-%s" % (st, et)
		# different date
		try:
			sd2 = sd.split("-")
			ed2 = ed.split("-")
			sdm = "%s/%s" % (sd2[2], sd2[1])
			edm = "%s/%s" % (ed2[2], ed2[1])
		except:
			sdm = sd
			edm = ed
		return "%s %s -> %s %s" % (sdm, st, edm, et)
	def OverInItem(self, dayIndex):
		# Build and show our custom tooltip at mouse position (follow mouse)
		if not self.eventToolTip:
			return
		# hide default tooltip if anything uses it
		interface = constinfo.GetInterfaceInstance()
		if interface and interface.tooltipItem:
			interface.tooltipItem.HideToolTip()

		# Title
		title = localeInfo.EVENT_TOOLTIP_TITLE % self.__textToColorFull("%04d-%02d-%02d" % (int(self.currentYear), int(self.currentMonth), int(dayIndex)))
		# Strip color wrapper from title (we want plain)
		try:
			title = title.replace('|cFFFFFFFF','').replace('|r','')
		except:
			pass

		rows = []
		if server_event_data.has_key(dayIndex):
			eventList = server_event_data[dayIndex]
			# sort by start time then index for stable ordering
			sortedEvents = sorted(eventList.items(), key=lambda kv: (kv[1][EVENT_START_TIME] if kv[1][EVENT_START_TIME] else 0, kv[1][EVENT_INDEX]))
			now = app.GetGlobalTimeStamp()
			for eventID, eventData in sortedEvents:
				name = self.__BuildEventNamePlain(eventData)
				tm = self.__FormatTimeShort(eventData[EVENT_START_TEXT], eventData[EVENT_END_TEXT])
				start_ts = eventData[EVENT_START_TIME]
				end_ts = eventData[EVENT_END_TIME]
				active_flag = eventData[EVENT_IS_ACTIVE]
				# Color by state
				if active_flag or (start_ts and now >= start_ts and (not end_ts or now <= end_ts)):
					color = self.eventToolTip.COLOR_ACTIVE
				elif end_ts and now > end_ts:
					color = self.eventToolTip.COLOR_ENDED
				else:
					color = self.eventToolTip.COLOR_FUTURE
				rows.append((name, tm, color))
		else:
			# no events
			rows.append((localeInfo.EVENT_TOOLTIP_DOESNT_HAVE_EVENT, '', self.eventToolTip.COLOR_ENDED))

		self.eventToolTip.Build(title, rows)
		self.eventToolTip.ShowAtMouse(True)


class MovableImage(ImageBoxSpecial):
	def __del__(self):
		ImageBoxSpecial.__del__(self)

	def Destroy(self):
		self.window = None
		self.eventCache = []
		self.timeList = []
		self.timeText = None
		ImageBoxSpecial.Destroy(self)

	def __init__(self):
		self.Destroy()
		ImageBoxSpecial.__init__(self, True)

		window = ui.Window()
		window.SetParent(self)
		window.AddFlag("not_pick")
		window.OnUpdate = ui.__mem_func__(self.OnUpdate)
		window.Show()
		self.window = window

		timeText = ui.TextLine()
		timeText.SetParent(self)
		timeText.AddFlag("not_pick")
		timeText.SetHorizontalAlignCenter()
		timeText.SetPosition(25, 55)
		timeText.SetOutline()
		timeText.Show()
		self.timeText = timeText

		timeTextEx = ui.TextLine()
		timeTextEx.SetParent(self)
		timeTextEx.AddFlag("not_pick")
		timeTextEx.SetHorizontalAlignCenter()
		timeTextEx.SetPosition(25, 70)
		timeTextEx.SetText(localeInfo.BONUS_NEXT_EVENT)
		timeTextEx.SetOutline()
		timeTextEx.Show()
		self.timeTextEx = timeTextEx

	def Clear(self):
		self.eventCache = []
		self.timeList = []
		self.timeText.SetText("")
		self.timeTextEx.Hide()
		ImageBoxSpecial.Clear(self)
		self.Hide()

	def Refresh(self):
		self.Clear()
		for dayIndex, eventList in server_event_data.items():
			for eventID, eventData in eventList.items():
				self.AppendEvent(eventData[EVENT_ID], eventData[EVENT_START_TIME], eventData[EVENT_END_TIME], eventData[EVENT_IS_ACTIVE])

	def LoadTime(self, eventID, startTime, endTime, isAlreadyStart):
		self.AppendImage(eventID)
		self.timeList.append([startTime, endTime, isAlreadyStart])

		if len(self.timeList) > 1:
			self.timeTextEx.Show()
		else:
			self.timeTextEx.Hide()
		self.Show()

	def CheckCacheEvent(self):
		if len(self.eventCache) == 0:
			return
		clientGlobalTime = app.GetGlobalTimeStamp()
		for j in xrange(len(self.eventCache)):
			startTime = self.eventCache[j][1] - clientGlobalTime
			endTime = self.eventCache[j][2] - clientGlobalTime
			if startTime >= 0 and startTime <= (60*30):
				self.Refresh()
				return

	def AppendEvent(self, eventID, startTime, endTime, isAlreadyStart):
		clientGlobalTime = app.GetGlobalTimeStamp()

		newStartTime = startTime
		newEndTime = endTime
		if newStartTime != 0:
			newStartTime -= clientGlobalTime
		if newEndTime != 0:
			newEndTime -= clientGlobalTime

		if (startTime != 0 and newStartTime <= 0 and endTime == startTime and isAlreadyStart == True) or (startTime != 0 and newStartTime <= 0 and endTime == 0 and isAlreadyStart == True):
			self.LoadTime(eventID, startTime, endTime, 2)
		elif newStartTime <= 0 and newEndTime <= 0:
			return
		elif newStartTime > 0 and newStartTime <= (60*30):#start-in
			self.LoadTime(eventID, startTime, endTime, 0)
		elif newEndTime > 0 and isAlreadyStart == True:#end-in
			self.LoadTime(eventID, startTime, endTime, 1)
		else:
			self.eventCache.append([eventID, startTime, endTime, isAlreadyStart])

		if len(self.timeList) > 1:
			self.timeTextEx.Show()
		else:
			self.timeTextEx.Hide()

	def DeleteEvent(self, index):
		self.DeleteImage(index)
		del self.timeList[index]

		if len(self.timeList) <= 1:
			self.timeTextEx.Hide()

	def FormatTime(self, seconds):
		if seconds == 0:
			return ""
		m, s = divmod(seconds, 60)
		h, m = divmod(m, 60)
		return "%02dh %02dm %02ds" % (h, m, s)

	def OnUpdate(self):
		ImageBoxSpecial.OnUpdate(self)
		self.CheckCacheEvent()

		if self.imageIndex < len(self.timeList):
			timeData = self.timeList[self.imageIndex]

			if timeData[2] == 0:
				leftTime = timeData[0] - app.GetGlobalTimeStamp()
				if leftTime > 0:
					self.timeText.SetText(localeInfo.BONUS_START_IN%self.FormatTime(leftTime))

					if not self.timeText.IsShow():
						self.timeText.Show()

			elif timeData[2] == 1:
				leftTime = timeData[1] - app.GetGlobalTimeStamp()
				if leftTime > 0:
					self.timeText.SetText(localeInfo.BONUS_END_IN%self.FormatTime(leftTime))
					if not self.timeText.IsShow():
						self.timeText.Show()

			elif timeData[2] == 2:
				if self.timeText.IsShow():
					self.timeText.Hide()

