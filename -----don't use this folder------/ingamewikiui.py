import app
import player
import item
import nonplayer
import ui
import wndMgr
import grp
import math
import wiki
import ingamewikiconfig
import uitooltip
import types
import constinfo
import localeinfo
import uiscriptlocale
import dbg
import sys
import chat

from _weakref import proxy
from operator import truediv


import re

_RE_REFINE_SUFFIX = re.compile(r"\s*([+xX])\s*(\d+)\s*$")

def StripRefineSuffix(name):
	# Levgja a vgrl: " +9", "+9", " x13", "x13" (kis/nagy X is)
	m = _RE_REFINE_SUFFIX.search(name)
	if not m:
		return name
	return name[:m.start()].strip()

def FormatRefineLevel(level):
	try:
		lvl = int(level)
	except:
		return "+" + str(level)

	if lvl <= 9:
		return "+" + str(lvl)
	return "x" + str(lvl)



"""Static pages size values!
	Please dont touch on them."""
categoryPeakWindowSize = [109, 314]
mainBoardPos = [148, 106]
mainBoardSize = [555, 361]
monsterBonusInfoPageSize = [539, 157]
itemOriginPageSize = [351, 142]

stole_costumes_effect = {
							85019 : 68,
							85020 : 68,
							85021 : 68,
							85022 : 68,
							85023 : 69,
							85024 : 69,
							85025 : 69,
							85026 : 69,
							85027 : 70,
							85028 : 70,
							85029 : 70,
							85030 : 70,
							85031 : 71,
							85032 : 71,
							85033 : 71,
							85034 : 71,
							85035 : 72,
							85036 : 72,
							85037 : 72,
							85038 : 72,
							85039 : 73,
							85040 : 73,
							85041 : 73,
							85042 : 73,
							85043 : 74,
							85044 : 74,
							85045 : 74,
							85046 : 74,
							85047 : 84,
							85048 : 84,
							85049 : 84,
							85050 : 84,
							85051 : 85,
							85052 : 85,
							85053 : 85,
							85054 : 85,
							85055 : 86,
							85056 : 86,
							85057 : 86,
							85058 : 86,
							85059 : 87,
							85060 : 87,
							85061 : 87,
							85062 : 87,
							85063 : 88,
							85064 : 88,
							85065 : 88,
							85066 : 88,
							85067 : 89,
							85068 : 89,
							85069 : 89,
							85070 : 89,
							85083 : 90
							#halloween
							, 85088 : 743,
							85092 : 743,
							85096 : 743,
							85100 : 743
							#end halloween
							#patch3
							, 85104 : 752,
							85108 : 761
							#end patch3
							#patch4
							, 85112 : 771
							, 85116 : 771
							, 85120 : 73
							#end patch4
							#christmas
							, 85124 : 72
							, 85132 : 74
							, 85140 : 812
							, 85148 : 814
							#end christmas
}

mob_to_map = {
				#mobVnum : mapIdx
				2598 : 216,
}

def HAS_FLAG(value, flag):
	return (value & flag) == flag

# def MakeMoneyText(money):
	# money = str(money)
	# (original, sLen, kLen) = (money, len(money), 3)
	
	# while sLen > kLen and original[sLen-kLen:] == "000":
		# money = money[::-1].replace("000"[::-1], "k"[::-1], 1)[::-1]
		# original = original[:sLen - kLen]
		# sLen -= kLen
	
	# return money
def MakeMoneyText(money):
	money = float(money)

	if money >= 1000000000:
		return "{:.1f} mlrd".format(money / 1000000000)

	if money >= 1000000:
		return "{:.1f} m".format(money / 1000000)

	if money >= 1000:
		return "{:.1f} k".format(money / 1000)

	return str(int(money))

class WikiRenderTarget(ui.WikiRenderTarget):
	def __init__(self, width, height):
		self.moduleID = wiki.WIKI_RENDER_MODULE_DELETE
		
		ui.WikiRenderTarget.__init__(self)
		self.SetSize(width, height)
		
		self.ManageModelViewWindow(False)
	
	def __del__(self):
		self.ManageModelViewWindow(True)
		
		ui.WikiRenderTarget.__del__(self)
	
	def ManageModelViewWindow(self, deleteModule = False):
		self.moduleID = (wiki.GetFreeModelViewID() if deleteModule is False else wiki.WIKI_RENDER_MODULE_DELETE)
		wiki.RegisterModelViewWindow(self.moduleID, self.hWnd)
	
	def IsValidModuleID(self):
		if self.moduleID == wiki.WIKI_RENDER_MODULE_DELETE or\
			self.moduleID < wiki.WIKI_RENDER_MODULE_START:
			return False
		
		return True
	
	def SetModel(self, model_vnum):
		if self.IsValidModuleID():
			wiki.RenderTargetSetBackground(self.moduleID, "d:/ymir work/ui/game/myshop_deco/model_view_bg.sub")
			wiki.SetModelViewModel(self.moduleID, model_vnum)

	def SetWeaponModel(self, weapon_vnum):
		if self.IsValidModuleID():
			wiki.SetWeaponModel(self.moduleID, weapon_vnum)

	def SetModelForm(self, main_vnum):
		if self.IsValidModuleID():
			wiki.SetModelForm(self.moduleID, main_vnum)

	def SetModelHair(self, hair_vnum):
		if self.IsValidModuleID():
			wiki.SetModelHair(self.moduleID, hair_vnum)

	def SetModelSash(self, sash_vnum):
		if self.IsValidModuleID():
			wiki.SetModelSash(self.moduleID, sash_vnum)

	def SetModelWeaponEffect(self, weapon_effect_vnum):
		if self.IsValidModuleID():
			wiki.SetModelWeaponEffect(self.moduleID, weapon_effect_vnum)

	def SetModelArmorEffect(self, armor_effect_vnum):
		if self.IsValidModuleID():
			wiki.SetModelArmorEffect(self.moduleID, armor_effect_vnum)

	def SetModelV3Eye(self, x, y, z):
		if self.IsValidModuleID():
			wiki.SetModelV3Eye(self.moduleID, x, y, z)
	
	def SetModelV3Target(self, x, y, z):
		if self.IsValidModuleID():
			wiki.SetModelV3Target(self.moduleID, x, y, z)
	
	def Show(self):
		wndMgr.Show(self.hWnd)
		
		if self.IsValidModuleID():
			wiki.ManageModelViewVisibility(self.moduleID, True)
	
	def Hide(self):
		wndMgr.Hide(self.hWnd)
		
		if self.IsValidModuleID():
			wiki.ManageModelViewVisibility(self.moduleID, False)

	# A legtobb UI elem (pl. ExpandedImageBox) tamogatja a SetStringEvent-et.
	# A WikiRenderTarget C++ oldali wrapper-e nem feltetlenul exportalja ezt,
	# viszont a wndMgr igen, ezert itt potoljuk, hogy tooltip/hover eventek
	# ugyanugy mukodjenek.
	def SetStringEvent(self, eventName, func, *args):
		try:
			# Metin2 UI: wndMgr.SetStringEvent(hWnd, "MOUSE_OVER_IN", func, arg1, ...)
			wndMgr.SetStringEvent(self.hWnd, eventName, func, *args)
		except:
			# Bizonyos kliensekben mas API van; ilyenkor legalabb ne crasheljen.
			try:
				wndMgr.SetEvent(self.hWnd, eventName, func, *args)
			except:
				pass

class WikiScrollBar(ui.Window):
	BASE_COLOR = grp.GenerateColor(0.0, 0.0, 0.0, 1.0)
	CORNERS_AND_LINES_COLOR = grp.GenerateColor(63.0 / 255.0, 63.0 / 255.0, 63.0 / 255.0, 1.0)
	#ui.SELECT_COLOR_WIKI
	
	BAR_NUMB = 9 #This is static value! Please dont touch in him.
	
	class MiddleBar(ui.DragButton):
		MIDDLE_BAR_COLOR = grp.GenerateColor(88.4 / 255.0, 0.0 / 255.0, 0.0 / 255.0, 1.0)
		# MIDDLE_BAR_COLOR = grp.GenerateColor(219.0 / 66.0, 0.0 / 255.0, 0.0 / 255.0, 1.0)
		
		def __init__(self, horizontal_scroll):
			ui.DragButton.__init__(self)
			self.AddFlag("movable")
			
			self.horizontal_scroll = horizontal_scroll
			
			self.middle = ui.Bar()
			self.middle.SetParent(self)
			self.middle.AddFlag("attach")
			self.middle.AddFlag("not_pick")
			self.middle.SetColor(self.MIDDLE_BAR_COLOR)
			self.middle.SetSize(1, 1)
			self.middle.Show()
		
		def SetStaticScale(self, size):
			(base_width, base_height) = (self.middle.GetWidth(), self.middle.GetHeight())
			
			if not self.horizontal_scroll:
				ui.DragButton.SetSize(self, base_width, size)
				self.middle.SetSize(base_width, size)
			else:
				ui.DragButton.SetSize(self, size, base_height)
				self.middle.SetSize(size, base_height)
		
		def SetSize(self, selfSize, fullSize):
			(base_width, base_height) = (self.middle.GetWidth(), self.middle.GetHeight())
			
			if not self.horizontal_scroll:
				ui.DragButton.SetSize(self, base_width, truediv(int(selfSize), int(fullSize)) * selfSize)
				self.middle.SetSize(base_width, truediv(int(selfSize), int(fullSize)) * selfSize)
			else:
				ui.DragButton.SetSize(self, truediv(int(selfSize), int(fullSize)) * selfSize, base_height)
				self.middle.SetSize(truediv(int(selfSize), int(fullSize)) * selfSize, base_height)
		
		def SetStaticSize(self, size):
			size = max(2, size)
			
			if not self.horizontal_scroll:
				ui.DragButton.SetSize(self, size, self.middle.GetHeight())
				self.middle.SetSize(size, self.middle.GetHeight())
			else:
				ui.DragButton.SetSize(self, self.middle.GetWidth(), size)
				self.middle.SetSize(self.middle.GetWidth(), size)
	
	def __init__(self, horizontal_scroll = False):
		ui.Window.__init__(self)
		
		self.horizontal_scroll = horizontal_scroll
		
		self.scrollEvent = None
		self.scrollSpeed = 1
		self.sizeScale = 1.0
		
		self.bars = []
		for i in xrange(self.BAR_NUMB):
			br = ui.Bar()
			br.SetParent(self)
			br.AddFlag("attach")
			br.AddFlag("not_pick")
			br.SetColor([self.CORNERS_AND_LINES_COLOR, self.BASE_COLOR][i == (self.BAR_NUMB-1)])
			if not (i % 2 == 0): br.SetSize(1, 1)
			br.Show()
			
			self.bars.append(br)
		
		self.middleBar = self.MiddleBar(self.horizontal_scroll)
		self.middleBar.SetParent(self)
		self.middleBar.SetMoveEvent(ui.__mem_func__(self.OnScrollMove))
		self.middleBar.Show()
	
	def OnScrollMove(self):
		if not self.scrollEvent:
			return
		
		arg = float(self.middleBar.GetLocalPosition()[1] - 1) / float(self.GetHeight() - 2 - self.middleBar.GetHeight()) if not self.horizontal_scroll else\
				float(self.middleBar.GetLocalPosition()[0] - 1) / float(self.GetWidth() - 2 - self.middleBar.GetWidth())
		
		self.scrollEvent(arg)
	
	def SetScrollEvent(self, func):
		self.scrollEvent = ui.__mem_func__(func)
	
	def SetScrollSpeed(self, speed):
		self.scrollSpeed = speed
	
	def OnRunMouseWheel(self, length):
		if self.IsInPosition():
			if not self.horizontal_scroll:
				val = min(max(1, self.middleBar.GetLocalPosition()[1] - (length * 0.01) * self.scrollSpeed * self.sizeScale), self.GetHeight() - self.middleBar.GetHeight() - 1)
				self.middleBar.SetPosition(2, val)
			else:
				val = min(max(1, self.middleBar.GetLocalPosition()[0] - (length * 0.01) *  self.scrollSpeed * self.sizeScale), self.GetWidth() - self.middleBar.GetWidth() - 1)
				self.middleBar.SetPosition(val, 1)
			
			self.OnScrollMove()
			return True
		
		return False
	
	def OnMouseLeftButtonDown(self):
		(xMouseLocalPosition, yMouseLocalPosition) = self.GetMouseLocalPosition()
		
		if not self.horizontal_scroll:
			if xMouseLocalPosition == 0 or xMouseLocalPosition == self.GetWidth():
				return
			
			y_pos = (yMouseLocalPosition - self.middleBar.GetHeight() / 2)
			self.middleBar.SetPosition(2, y_pos)
		else:
			if yMouseLocalPosition == 0 or yMouseLocalPosition == self.GetHeight():
				return
			
			x_pos = (xMouseLocalPosition - self.middleBar.GetWidth() / 2)
			self.middleBar.SetPosition(x_pos, 1)
		
		self.OnScrollMove()
	
	def SetSize(self, w, h):
		(width, height) = (max(3, w), max(3, h))
		
		ui.Window.SetSize(self, width, height)
		
		self.bars[0].SetSize(1, (height - 2))
		self.bars[0].SetPosition(0, 1)
		self.bars[2].SetSize((width - 2), 1)
		self.bars[2].SetPosition(1, 0)
		self.bars[4].SetSize(1, (height - 2))
		self.bars[4].SetPosition((width - 1), 1)
		self.bars[6].SetSize((width - 2), 1)
		self.bars[6].SetPosition(1, (height - 1))
		self.bars[8].SetSize((width - 2), (height - 2))
		self.bars[8].SetPosition(1, 1)
		
		self.bars[1].SetPosition(0, 0)
		self.bars[3].SetPosition((width - 1), 0)
		self.bars[5].SetPosition((width - 1), (height - 1))
		self.bars[7].SetPosition(0, (height - 1))
		
		if not self.horizontal_scroll:
			self.middleBar.SetStaticSize(width - 4)
			self.middleBar.SetSize(10, self.GetHeight())
		else:
			self.middleBar.SetStaticSize(height - 2)
			self.middleBar.SetSize(12, self.GetWidth())
		
		if not self.horizontal_scroll:
			self.middleBar.SetRestrictMovementArea(2, 1, width - 4, height - 2)
		else:
			self.middleBar.SetRestrictMovementArea(1, 1, width - 2, height - 2)
	
	def SetScale(self, selfSize, fullSize):
		self.sizeScale = float(selfSize/fullSize)
		self.middleBar.SetSize(selfSize, fullSize)
	
	def SetStaticScale(self, r_size):
		self.middleBar.SetStaticScale(r_size)
	
	def SetPosScale(self, fScale):
		pos = (math.ceil((self.GetHeight() - 2 - self.middleBar.GetHeight()) * fScale) + 1) if not self.horizontal_scroll else\
				(math.ceil((self.GetWidth() - 2 - self.middleBar.GetWidth()) * fScale) + 1)
		
		self.SetPos(pos)

	def SetPos(self, pos):
		wPos = (2, pos) if not self.horizontal_scroll else (pos, 1)
		self.middleBar.SetPosition(*wPos)

class WikiBuildGrid(ui.ScriptWindow):
	"""Static values!
		Please dont touch on them."""
	GRID_WIDTH = 541
	AUTO_BUILD_GRID_COUNT = 3
	
	IMAGES_PATH = "d:/ymir work/ui/wiki/grid/"
	AUTO_BUILD_GRID_IMAGES = [IMAGES_PATH + "detail_item_3.tga", IMAGES_PATH + "detail_item.tga"]
	
	def __init__(self, parent, size, flags, desired_width=None):
		ui.ScriptWindow.__init__(self)
		self.SetWindowName("WikiGridWindow")
		
		self.SetParent(parent)
		for flag in flags:
			self.AddFlag(flag)
		
		self.grid_elements = []
		self._desired_width = desired_width
		self._scale_x = 1.0
		self.LoadBackgroundParent()
		self.LoadGrid(size)
	
	def __del__(self):
		ui.ScriptWindow.__del__(self)
	
	def LoadBackgroundParent(self):
		self.backgroundParent = ui.Window()
		self.backgroundParent.SetParent(self)
		self.backgroundParent.AddFlag("attach")
		self.backgroundParent.AddFlag("not_pick")
		self.backgroundParent.SetPosition(0, 0)
		self.backgroundParent.Show()
	
	def SetBackgroundGridSize(self, size):
		self.SetSize(*size)
		self.backgroundParent.SetSize(*size)
	
	def SetGridSize(self, new_size):
		self.LoadGrid(new_size)
	
	def CleanOldGrid(self):
		for grid in self.grid_elements:
			grid.Hide()
		
		del self.grid_elements[:]
	
	def LoadGrid(self, size):
		self.CleanOldGrid()
		
		if size <= self.AUTO_BUILD_GRID_COUNT:
			self.grid_elements.append(ui.ExpandedImageBox())
			self.grid_elements[0].SetParent(self.backgroundParent)
			self.grid_elements[0].AddFlag("attach")
			self.grid_elements[0].AddFlag("not_pick")
			self.grid_elements[0].LoadImage(self.AUTO_BUILD_GRID_IMAGES[abs(self.AUTO_BUILD_GRID_COUNT - size)])
			self.grid_elements[0].SetPosition(0,0)
			self.grid_elements[0].Show()
			
			grid_w = int(self._desired_width) if self._desired_width else self.GRID_WIDTH
			# Scale background to fit width
			try:
				img_w = self.grid_elements[0].GetWidth()
				if img_w and grid_w != img_w:
					self.grid_elements[0].SetScale(float(grid_w) / float(img_w), 1.0)
			except:
				pass
			# store horizontal scale for alignment
			try:
				self._scale_x = float(grid_w) / float(img_w) if img_w else 1.0
			except:
				self._scale_x = 1.0
			self_size = (grid_w, self.grid_elements[0].GetHeight())
			self.SetBackgroundGridSize(self_size)
		else:
			REFACT_COUNT = (size - self.AUTO_BUILD_GRID_COUNT)
			
			self.grid_elements.append(ui.ExpandedImageBox())
			self.grid_elements[0].SetParent(self.backgroundParent)
			self.grid_elements[0].AddFlag("attach")
			self.grid_elements[0].AddFlag("not_pick")
			self.grid_elements[0].LoadImage(self.IMAGES_PATH + "initial_grid.tga")
			self.grid_elements[0].SetPosition(0,0)
			self.grid_elements[0].Hide()
			
			(main_grid_pos_x, main_grid_pos_y) = self.grid_elements[0].GetLocalPosition()
			main_grid_height = self.grid_elements[0].GetHeight()
			main_grid_final_pos = main_grid_height + main_grid_pos_y
			
			for i in xrange(REFACT_COUNT):
				tmpGrid = ui.ExpandedImageBox()
				tmpGrid.SetParent(self.backgroundParent)
				tmpGrid.AddFlag("attach")
				tmpGrid.AddFlag("not_pick")
				tmpGrid.LoadImage(self.IMAGES_PATH + "grid_line.tga")
				tmpGrid.SetPosition(main_grid_pos_x, main_grid_final_pos + (tmpGrid.GetHeight() * i))
				tmpGrid.Hide()
				
				self.grid_elements.append(tmpGrid)
			
			(last_grid_pos_x, last_grid_pos_y) = self.grid_elements[-1].GetLocalPosition()
			last_grid_height = self.grid_elements[-1].GetHeight()
			
			self.grid_elements.append(ui.ExpandedImageBox())
			self.grid_elements[-1].SetParent(self.backgroundParent)
			self.grid_elements[-1].AddFlag("attach")
			self.grid_elements[-1].AddFlag("not_pick")
			self.grid_elements[-1].LoadImage(self.IMAGES_PATH + "last_grid.tga")
			self.grid_elements[-1].SetPosition(last_grid_pos_x, last_grid_pos_y + last_grid_height)
			self.grid_elements[-1].Hide()
			
			for grid in self.grid_elements:
				grid.Show()

			(last_grid_pos_x, last_grid_pos_y) = self.grid_elements[-1].GetLocalPosition()
			last_grid_height = self.grid_elements[-1].GetHeight()
			
			grid_w = int(self._desired_width) if self._desired_width else self.GRID_WIDTH
			# Scale all grid parts horizontally
			try:
				for g in self.grid_elements:
					img_w = g.GetWidth()
					if img_w and grid_w != img_w:
						g.SetScale(float(grid_w) / float(img_w), 1.0)
			except:
				pass
			# store horizontal scale for alignment
			try:
				# use first element width as reference
				ref_w = self.grid_elements[0].GetWidth()
				self._scale_x = float(grid_w) / float(ref_w) if ref_w else 1.0
			except:
				self._scale_x = 1.0
			self_size = (grid_w, last_grid_pos_y + last_grid_height)
			self.SetBackgroundGridSize(self_size)

class SubCategObject(ui.Window):
	def __init__(self, text):
		ui.Window.__init__(self)
		self.SetWindowName("SubCategObject_ListBoxEx_Item")
		
		self.mArgs = ()
		self.parent = None
		
		new_text = ""
		self.bannerFileName = None
		
		if type(text) == types.ListType:
			new_text = text[0]
			self.mArgs = text[1]
			if len(text) > 2:
				self.bannerFileName = text[2]
		
		self.textWindow = ui.Window()
		self.textWindow.SetParent(self)
		self.textWindow.AddFlag("attach")
		self.textWindow.AddFlag("not_pick")
		self.textWindow.SetPosition(2, 3)
		self.textWindow.Show()
		
		self.textLine = ui.TextLine()
		self.textLine.AddFlag("attach")
		self.textLine.AddFlag("not_pick")
		self.textLine.SetParent(self.textWindow)
		self.textLine.SetText(new_text)
		self.textLine.Show()
		
		self.cutTextLine = ui.TextLine()
		self.cutTextLine.SetParent(self.textWindow)
		self.cutTextLine.SetText(new_text)
		self.cutTextLine.Hide()
		
		self.originText = new_text
		self.cutedText = new_text
	
	def SetSize(self, w, h):
		ui.Window.SetSize(self, w, h)
		self.textWindow.SetSize(w - 2, h - 3)
		
		self.cutedText = self.GetCutText()
	
	def SetParent(self, parent):
		self.parent = proxy(parent)
		
		ui.Window.SetParent(self, parent)
	
	def OnMouseLeftButtonDown(self):
		if self.parent:
			self.parent.SelectItem(self)
	
	def GetCutText(self):
		(wL, wT, wR, wB) = self.cutTextLine.GetRenderBox()
		if wR > 0:
			i = (len(self.originText) - 1)
			
			while wR > 0:
				self.cutTextLine.SetText(self.originText[:i])
				(wL, wT, wR, wB) = self.cutTextLine.GetRenderBox()
				i -= 1
			
			self.cutTextLine.SetText(self.originText[:i].strip() + "..")
			
			(wL, wT, wR, wB) = self.cutTextLine.GetRenderBox()
			if wR > 0:
				i -= 1
			
			cut_text = (self.originText[:i] + "..")
			self.cutTextLine.SetText(self.originText)
			self.textLine.SetText(cut_text)
			
			return cut_text
		
		self.cutTextLine.SetText(self.originText)
		self.textLine.SetText(self.originText)
		
		return self.originText
	
	def OnUpdate(self):
		(wL, wT, wR, wB) = self.cutTextLine.GetRenderBox()
		currX = self.textLine.GetLocalPosition()[0]
		
		if self.IsInPosition():
			if wR > 0:
				if self.originText != self.textLine.GetText():
					self.textLine.SetText(self.originText)
				
				(wL, wT, wR, wB) = self.textLine.GetRenderBox()
				if wR > 0:
					self.textLine.SetPosition(currX - 1, 0)
		elif currX < 0:
			self.textLine.SetPosition(currX + 1, 0)
		elif self.textLine.GetText() != self.cutedText:
			self.textLine.SetText(self.cutedText)
	
	def OnRender(self):
		if self.parent.GetSelectedItem()==self:
			grp.SetColor(ui.SELECT_COLOR_WIKI)
			self.OnSelectedRender()
		elif self.IsIn():
			grp.SetColor(ui.HALF_WHITE_COLOR)
			self.OnSelectedRender()
	
	def OnSelectedRender(self):
		(rLeft, rTop, rRight, rBottom) = self.GetRenderBox()
		x, y = self.GetGlobalPosition()
		
		grp.RenderBar(x + rLeft, y + rTop, self.GetWidth() - rLeft - rRight, self.GetHeight() - rBottom - rTop)

class WikiCategory(ui.Window):
	"""Static values!
		Please dont touch on them."""
	TICK_COUNT = 6
	MIN_HEIGHT = 20
	ARROW_IMG = ["d:/ymir work/ui/wiki/arrow.tga", "d:/ymir work/ui/wiki/arrow_up.tga"]
	# UI polish: diffuse szinek (szorozza a texturat) + hover
	# (ha a category.tga tul zold/eros, ezzel vissza lehet szurkitani)
	TITLE_DIFFUSE_NORMAL = (0.70, 0.70, 0.70, 1.0)
	TITLE_DIFFUSE_HOVER  = (1.00, 1.00, 1.00, 1.0)
	TITLE_DIFFUSE_OPEN   = (0.85, 0.85, 0.85, 1.0)
	HOVER_BAR_COLOR	  = grp.GenerateColor(0.10, 0.90, 0.25, 0.00)  # alpha animalt
	OPEN_BAR_COLOR	   = grp.GenerateColor(0.10, 0.90, 0.25, 0.00)  # alpha animalt
	
	LINES_COLOR = grp.GenerateColor(8.6 / 255.0, 4.2 / 255.0, 0.0 / 255.0, 1.0)
	CORNER_COLOR = grp.GenerateColor(9.1 / 255.0, 4.4 / 255.0, 0.0 / 255.0, 1.0)
	BASE_COLOR = grp.GenerateColor(9.1 / 255.0, 4.4 / 255.0, 0.0 / 255.0, 1.0)
	
	def __init__(self, owner = None):
		ui.Window.__init__(self)
		
		self.owner = (None if not owner else proxy(owner))
		
		self.expectedSize = 0
		self.currHeight = self.MIN_HEIGHT
		
		self.isAnimating = False
		self.isOpening = False
		self.isHover = False
		
		# Hover fade animacio (green highlight)
		self.__hoverAnim = 0.0
		self.__openGlowTarget = 0.35  # amikor nyitva van, ennyire vilagitson
		self.__lastAnimTime = 0
		try:
			self.__lastAnimTime = app.GetTime()
		except:
			self.__lastAnimTime = 0
		
		self.clickEvent = None
		
		# --- Category header button (szebb, inventory-stilus) ---
		# Nem a category.tga-t hasznaljuk, hanem a public middle_button texturakat.
		self.titleImg = ui.Button()
		self.titleImg.SetParent(self)
		try:
			w = int(categoryPeakWindowSize[0]) - 18
		except:
			w = 220
		if w < 120:
			w = 120
		h = 22
		self.titleImg.SetSize(w, h)
		self.titleImg.SetUpVisual("d:/ymir work/razor93/wiki/wiki_btn_normal.tga")
		self.titleImg.SetOverVisual("d:/ymir work/razor93/wiki/wiki_btn_hover.tga")
		self.titleImg.SetDownVisual("d:/ymir work/razor93/wiki/wiki_btn_down.tga")

		self.titleImg.SetEvent(ui.__mem_func__(self.ClickExpand))
		# hover esemenyek (biztos, nem SetStringEvent)
		self.titleImg.SetOverEvent(ui.__mem_func__(self.__OnTitleOverIn))
		self.titleImg.SetOverOutEvent(ui.__mem_func__(self.__OnTitleOverOut))
		self.titleImg.Show()

		# Hover/active zold overlay (fade in/out)
		self.hoverBar = ui.Bar()
		self.hoverBar.SetParent(self.titleImg)
		self.hoverBar.AddFlag("attach")
		self.hoverBar.AddFlag("not_pick")
		self.hoverBar.SetPosition(2, 2)
		self.hoverBar.SetSize(self.titleImg.GetWidth() - 4, self.titleImg.GetHeight() - 4)
		self.hoverBar.SetColor(grp.GenerateColor(0.10, 0.90, 0.25, 0.0))
		self.hoverBar.Hide()

		self.__ApplyTitleDiffuse()

		self.titleText = ui.TextLine()
		self.titleText.SetParent(self.titleImg)
		self.titleText.AddFlag("attach")
		self.titleText.AddFlag("not_pick")
		# bal padding + kozepre igazitas
		self.titleText.SetPosition(12, (self.titleImg.GetHeight() - 14) / 2)
		self.titleText.Show()

		self.arrow = ui.ExpandedImageBox()
		self.arrow.SetParent(self.titleImg)
		self.arrow.AddFlag("attach")
		self.arrow.AddFlag("not_pick")
		self.arrow.LoadImage(self.ARROW_IMG[0])
		self.arrow.SetPosition(self.titleImg.GetWidth() - self.arrow.GetWidth() - 12, (self.titleImg.GetHeight() - self.arrow.GetHeight()) / 2)
		self.arrow.Show()

		self.expandWnd = ui.Window()
		self.expandWnd.SetParent(self)
		self.expandWnd.AddFlag("attach")
		self.expandWnd.AddFlag("not_pick")
		self.expandWnd.SetPosition(0, self.titleImg.GetHeight())
		self.expandWnd.SetSize(self.titleImg.GetWidth(), 0)
		self.expandWnd.Show()
		
		self.bars = []
		for i in xrange(self.TICK_COUNT):
			br = ui.Bar()
			br.SetParent(self.expandWnd)
			br.AddFlag("attach")
			br.AddFlag("not_pick")
			br.SetColor([self.LINES_COLOR, self.CORNER_COLOR, self.BASE_COLOR][2 if i == (self.TICK_COUNT - 1) else int(i % 2 == 0)])
			br.Show()
			
			self.bars.append(br)
		
		self.categList = ui.ListBoxEx()
		self.categList.SetParent(self.expandWnd)
		self.categList.SetItemSize(self.titleImg.GetWidth() - 6, 17)
		self.categList.SetItemStep(17)
		self.categList.SetSize(self.titleImg.GetWidth() - 6, self.MIN_HEIGHT - 1)
		self.categList.SetPosition(3, 1)
		self.categList.SetSelectEvent(ui.__mem_func__(self.OnSelectSubCategory))
		self.categList.Show()
		
		ui.Window.SetSize(self, self.titleImg.GetWidth(), self.titleImg.GetHeight())
		self.ArrangeBars(self.MIN_HEIGHT)
	
	def OnSelectSubCategory(self, elem):
		if self.owner:
			self.owner.NotifyCategorySelect(self)
		
		wikiClass = wiki.GetBaseClass()
		if wikiClass and hasattr(wikiClass, 'header'):
			wikiClass.header.Hide()

		if self.clickEvent:
			apply(self.clickEvent, elem.mArgs)
	
	def UnselectSubCategory(self):
		self.categList.SelectIndex(-1)
	
	def AddSubCategory(self, key, text):
		self.currHeight = (17 * (self.categList.GetItemCount() + 1) + 3)
		
		if self.expandWnd.GetHeight() > 0:
			self.ArrangeBars(self.currHeight)
			self.isAnimating = True
		
		self.categList.SetViewItemCount(self.categList.GetItemCount() + 1)
		
		item = SubCategObject(text)
		self.categList.AppendItem(item)
		
		self.categList.SetSize(self.categList.GetWidth(), self.currHeight - 1)
	
	def ClickExpand(self):
		self.arrow.LoadImage(self.ARROW_IMG[(0 if self.isOpening else 1)])
		
		self.isOpening = not self.isOpening
		self.isAnimating = True
		
		self.expandWnd.Show()
		self.__ApplyTitleDiffuse()
		
		self.ArrangeBars(self.currHeight if self.isOpening else 0)

	def __ApplyTitleDiffuse(self):
		"""Csak a texturat/diffuse-t es a szoveg szinet allitja.
		A zold hover hatter animacio a OnUpdate-ben fut.
		"""
		# Szoveg szin
		try:
			if self.isHover:
				self.titleText.SetFontColor(0.20, 1.00, 0.35)
			else:
				self.titleText.SetFontColor(0.85, 0.85, 0.85)
		except:
			pass

		# Texture diffuse (ha elerheto a kliensedben)
		try:
			if self.isHover:
				self.titleImg.SetDiffuseColor(*self.TITLE_DIFFUSE_HOVER)
			elif self.isOpening:
				self.titleImg.SetDiffuseColor(*self.TITLE_DIFFUSE_OPEN)
			else:
				self.titleImg.SetDiffuseColor(*self.TITLE_DIFFUSE_NORMAL)
		except:
			pass

	def __OnTitleOverIn(self):
		self.isHover = True
		self.__ApplyTitleDiffuse()

	def __OnTitleOverOut(self):
		self.isHover = False
		self.__ApplyTitleDiffuse()
	
	def OnUpdate(self):
		# Hover zold animacio (fade in/out)
		try:
			now = app.GetTime()
			dt = 0.0
			if self.__lastAnimTime:
				dt = float(now - self.__lastAnimTime) / 1000.0
			self.__lastAnimTime = now
			# ha dt tul nagy (pl. lag / tab-out), fogjuk meg
			dt = min(dt, 0.05) if dt > 0 else 0.016
			# cel ertek: hover > nyitva > semmi
			target = 1.0 if self.isHover else (self.__openGlowTarget if self.isOpening else 0.0)
			# sima lerp
			speed = 14.0
			self.__hoverAnim += (target - self.__hoverAnim) * min(1.0, dt * speed)
			a = 0.35 * self.__hoverAnim
			if a > 0.01:
				self.hoverBar.SetColor(grp.GenerateColor(0.10, 0.90, 0.25, a))
				self.hoverBar.Show()
			else:
				self.hoverBar.Hide()
		except:
			pass

		# Expand/collapse animacio
		if not self.isAnimating:
			return
		
		h = self.expandWnd.GetHeight()
		
		if h == self.expectedSize:
			self.isAnimating = False
			
			if h == 0:
				self.expandWnd.Hide()
			
			return
		
		step = (self.currHeight / self.TICK_COUNT)
		newSize = 0
		isOpening = (False if h > self.expectedSize else True)
		
		if isOpening:
			newSize = min(self.currHeight, self.expandWnd.GetHeight() + step)
		else:
			newSize = max(0, self.expandWnd.GetHeight() - step)
		
		change = newSize - self.expandWnd.GetHeight()
		
		self.expandWnd.SetSize(self.titleImg.GetWidth(), newSize)
		ui.Window.SetSize(self, self.titleImg.GetWidth(), self.titleImg.GetHeight() + newSize)
		self.owner.NotifySizeChange(self, change, (newSize > h))
	
	def SetSize(self, width, height):
		dbg.LogBox("WikiCategory -> SetSize - unsupported function")
	
	def ArrangeBars(self, currHeight):
		self.expectedSize = currHeight
		if currHeight < self.MIN_HEIGHT:
			return
		
		currWidth = self.expandWnd.GetWidth()
		
		self.bars[0].SetSize(1, (currHeight - 1))
		self.bars[1].SetSize(1, 1)
		self.bars[1].SetPosition(0, (currHeight - 1))
		self.bars[2].SetSize((currWidth - 2), 1)
		self.bars[2].SetPosition(1, (currHeight - 1))
		self.bars[3].SetSize(1, 1)
		self.bars[3].SetPosition((currWidth - 1), (currHeight - 1))
		self.bars[4].SetSize(1, (currHeight - 1))
		self.bars[4].SetPosition((currWidth - 1), 0)
		self.bars[5].SetSize((currWidth - 2), (currHeight - 1))
		self.bars[5].SetPosition(1, 0)
	
	def SetTitleName(self, text):
		self.titleText.SetText(text)
		
		pos_y = (self.titleImg.GetHeight() / 2 - self.titleText.GetTextSize()[1] / 2 - 1)
		self.titleText.SetPosition(12, pos_y)

class WikiCategories(ui.Window):
	"""Static values!
		Please dont touch on them."""
	CATEGORY_PADDING = 5
	SCROLL_SPEED = 17
	
	def __init__(self):
		ui.Window.__init__(self)
		
		self.SetSize(*categoryPeakWindowSize)
		self.SetInsideRender(True)
		
		self.scrollBar = None
		self.hideWindowsEvent = None
		self.elements = []
		
		self.scrollBoard = ui.Window()
		self.scrollBoard.SetParent(self)
		self.scrollBoard.AddFlag("attach")
		self.scrollBoard.Show()
		self.SetOnRunMouseWheelEvent(self.OnRunMouseWheel)

	def AddCategory(self, text):
		tmp = WikiCategory(self)
		tmp.SetParent(self.scrollBoard)
		tmp.SetTitleName(text)
		tmp.Show()
		
		addPadding = 0
		if len(self.elements) > 0:
			pos_y = (self.elements[-1].GetLocalPosition()[1] + self.elements[-1].GetHeight() + self.CATEGORY_PADDING)
			tmp.SetPosition(0, pos_y)
			
			addPadding = self.CATEGORY_PADDING
		
		(width, height) = (tmp.GetWidth(), self.scrollBoard.GetHeight() + addPadding + tmp.GetHeight())
		self.scrollBoard.SetSize(width, height)
		
		self.elements.append(tmp)
		return tmp
	
	def NotifySizeChange(self, obj, amount, expanded):
		ind = len(self.elements)
		if obj in self.elements:
			ind = (self.elements.index(obj) + 1)
		
		if ind < len(self.elements):
			for i in xrange(ind, len(self.elements)):
				pos_y = (self.elements[(i - 1)].GetLocalPosition()[1] + self.elements[(i - 1)].GetHeight() + self.CATEGORY_PADDING)
				self.elements[i].SetPosition(0, pos_y)
		
		(width, height) = (self.scrollBoard.GetWidth(), (self.scrollBoard.GetHeight() + amount))
		self.scrollBoard.SetSize(width, height)
		self.UpdateScrollbar()
	
	def NotifyCategorySelect(self, obj):
		if self.hideWindowsEvent:
			self.hideWindowsEvent()
		
		for i in self.elements:
			if obj != proxy(i) and obj != i:
				i.UnselectSubCategory()
	
	def OnRunMouseWheel(self, length):
		if self.IsInPosition():
			self.UpdateScrollbar(int((length * 0.01) * self.SCROLL_SPEED))
			return True
		
		return False
	
	def OnScrollBar(self, fScale):
		curr = min(0, max(math.ceil((self.scrollBoard.GetHeight() - categoryPeakWindowSize[1]) * fScale * -1.0), -self.scrollBoard.GetHeight() + categoryPeakWindowSize[1]))
		self.scrollBoard.SetPosition(0, curr)
	
	def ChangeScrollbar(self):
		if not self.scrollBar:
			return
		
		scrollBoardHeight = self.scrollBoard.GetHeight()
		selfHeight = self.GetHeight()
		
		if scrollBoardHeight <= selfHeight:
			self.scrollBar.Hide()
		else:
			self.scrollBar.SetScale(selfHeight, scrollBoardHeight)
			
			new_pos = self.GetNewScrollBarPosition()
			if new_pos != 0:
				self.scrollBar.SetPos(new_pos)
			else:
				pos_scale = truediv(abs(self.scrollBoard.GetLocalPosition()[1]), (scrollBoardHeight - selfHeight))
				self.scrollBar.SetPosScale(pos_scale)
			
			self.scrollBar.Show()
	
	def GetNewScrollBarPosition(self):
		if self.scrollBoard.GetLocalPosition()[1] >= 0:
			return 0
		
		out_perc = truediv(self.scrollBoard.GetLocalPosition()[1], self.scrollBoard.GetHeight())
		scroll_max_size = abs(self.GetHeight() * out_perc)
		
		return scroll_max_size
	
	def UpdateScrollbar(self, val = 0):
		curr = min(0, max(self.scrollBoard.GetLocalPosition()[1] + val, -self.scrollBoard.GetHeight() + categoryPeakWindowSize[1]))
		self.scrollBoard.SetPosition(0, curr)
		
		self.ChangeScrollbar()
	
	def RegisterScrollBar(self, scroll):
		self.scrollBar = proxy(scroll)
		self.scrollBar.SetScrollEvent(self.OnScrollBar)
		self.scrollBar.SetScrollSpeed(self.SCROLL_SPEED)
		
		self.ChangeScrollbar()

class WikiMainWeaponWindow(ui.Window):
	unique_vnum = [
					8579,
					8589,
					8599,
					8609,
					8619,
					8629,
					8639,
					8509,
					8519,
					8529,
					8539,
					8549,
					8559,
					8569,
					329,
					339,
					349,
					359,
					369,
					379,
					389,
					399,
					409,
					419,
					429,
					439,
					449,
					459,
					529,
					539,
					549,
					559,
					569,
					579,
					589,
					599,
					609,
					619,
					629,
					639,
					649,
					659,
					669,
					679,
					689,
					699,
					709,
					719,
					729,
					739,
					749,
					759,
					769,
					779,
					1199,
					1209,
					1219,
					1229,
					1519,
					1529,
					1539,
					1549,
					2219,
					2229,
					2239,
					2249,
					2519,
					2529,
					2539,
					2549,
					3239,
					3249,
					3259,
					3269,
					3519,
					3529,
					3539,
					3549,
					5179,
					5189,
					5199,
					5209,
					5519,
					5529,
					5539,
					5549,
					7319,
					7329,
					7339,
					7349,
					7519,
					7529,
					7539,
					7549,
					12100,
					12101,
					12102,
					12103,
					12104,
					12105,
					12106,
					12107,
					12108,
					12109,
					12110,
					12111,
					12112,
					12113,
					12114,
					12115,
					12790,
					12791,
					12792,
					12793,
					12810,
					12811,
					12812,
					12813,
					12830,
					12831,
					12832,
					12833,
					12850,
					12851,
					12852,
					12853,
					12854,
					12855,
					12856,
					12857,
					12858,
					12859,
					12860,
					12861,
					12862,
					12863,
					12864,
					12865,
					12866,
					12867,
					12868,
					12869,
					13070,
					13071,
					13072,
					13073,
					13090,
					13091,
					13092,
					13093,
					13110,
					13111,
					13112,
					13113,
					13130,
					13131,
					13132,
					13133,
					13150,
					13151,
					13152,
					13153,
					13170,
					13171,
					13172,
					13173,
					14230,
					14231,
					14232,
					14233,
					15010,
					15011,
					15012,
					15013,
					15460,
					15461,
					15462,
					15463,
					15464,
					15465,
					15466,
					15467,
					16230,
					16231,
					16232,
					16233,
					16590,
					16591,
					16592,
					16593,
					17230,
					17231,
					17232,
					17233,
					17580,
					17581,
					17582,
					17583,
					19309,
					19310,
					19311,
					19312,
					19509,
					19510,
					19511,
					19512,
					19709,
					19710,
					19711,
					19712,
					19909,
					19910,
					19911,
					19912
	]

	class WikiItem(ui.Window):
		"""Static values!
			Please dont touch on them."""
		SCROLL_SPEED = 50
		
		TABLE_COLS = [
			[131,21,40,134],
			[172,21,40,134],
			[213,21,41,134],
			[255,21,40,134],
			[296,21,40,134],
			[337,21,41,134],
			[379,21,40,134],
			[420,21,40,134],
			[461,21,41,134],
			[503,21,36,134]
		]
		
		STARTING_MAT_COUNT = 2
		
		ROW_HEIGHTS = [21, 44, 51, 17]
		ROW_START_Y = [0, 22, 67, 119]
		
		ROW_HEIGHTS_EXPANDED = {}
		for idx in range(STARTING_MAT_COUNT + 1, wiki.REFINE_MATERIAL_MAX_NUM + 1):
			ROW_HEIGHTS_EXPANDED.update({idx : ([21, 44, 42, 22], [])})
			
			for insert_values in xrange(idx - STARTING_MAT_COUNT):
				ROW_HEIGHTS_EXPANDED[idx][0].insert((2 + insert_values), 47)
			
			for build_pos in xrange(len(ROW_HEIGHTS_EXPANDED[idx][0])):
				new_pos = 0
				for idx_pos in xrange(build_pos):
					new_pos += (ROW_HEIGHTS_EXPANDED[idx][0][idx_pos] + 1)
				
				ROW_HEIGHTS_EXPANDED[idx][1].append(new_pos)
		
		def __init__(self, vnum, parent, allow_special_page = True):
			ui.Window.__init__(self)
			
			self.parent = proxy(parent)
			
			wikiBase = wiki.GetBaseClass()
			if id(self) not in wikiBase.objList:
				wikiBase.objList[long(id(self))] = proxy(self)
			
			self.scrollBoard = None
			self.scrollBar = None
			
			self.additionalLoaded = False
			self.vnum = vnum
			self.levelLimit = 0
			
			grid_w = None
			try:
				if hasattr(self.parent, 'peekWindow') and self.parent.peekWindow:
					grid_w = self.parent.peekWindow.GetWidth()
				elif hasattr(self.parent, 'GetWidth'):
					grid_w = self.parent.GetWidth()
			except:
				grid_w = None
			self.base = WikiBuildGrid(self, 2, ["attach", "not_pick"], grid_w)
			self.base.Show()
			
			item.SelectItem(self.vnum)
			allow_increase_vnum = wiki.CanIncrRefineLevel()
			
			self.cols = []
			# Column positions must match the (scaled) background grid.
			scale_x = 1.0
			try:
				scale_x = float(getattr(self.base, '_scale_x', 1.0))
			except:
				scale_x = 1.0

			# Build scaled column definitions from the original TABLE_COLS.
			cols_def = []
			try:
				for (ox, oy, ow, oh) in self.TABLE_COLS:
					x = int(round(ox * scale_x))
					w = int(round(ow * scale_x))
					cols_def.append((x, oy, w, oh))
			except:
				cols_def = self.TABLE_COLS

			# Fix last column width to reach the right edge cleanly.
			right_pad = int(round(2 * scale_x))
			try:
				last_x, last_y, last_w, last_h = cols_def[-1]
				end_x = self.base.GetWidth() - right_pad
				cols_def[-1] = (last_x, last_y, max(1, end_x - last_x), last_h)
			except:
				pass

			for (pos_x, pos_y, width, height) in cols_def:
				
				tmp = ui.Window()
				tmp.SetParent(self.base)
				tmp.AddFlag("attach")
				tmp.AddFlag("not_pick")
				tmp.SetPosition(pos_x, pos_y)
				tmp.SetSize(width, height)
				tmp.Show()
				
				tmp.titleWnd = ui.Window()
				tmp.titleWnd.SetParent(tmp)
				tmp.titleWnd.SetPosition(0, self.ROW_START_Y[0])
				tmp.titleWnd.SetSize(tmp.GetWidth(), self.ROW_HEIGHTS[0])
				tmp.titleWnd.SAFE_SetOverInEvent(self.parent.OnOverIn, self.vnum + (len(self.cols) if allow_increase_vnum else 0))
				tmp.titleWnd.SAFE_SetOverOutEvent(self.parent.OnOverOut)
				if wiki.GetBaseClass() and allow_special_page:
					tmp.titleWnd.SetMouseLeftButtonDownEvent(ui.__mem_func__(wiki.GetBaseClass().OpenSpecialPage), self.parent, self.vnum, False)
				tmp.titleWnd.Show()
				
				tmp.refineText = ui.TextLine()
				tmp.refineText.SetParent(tmp.titleWnd)
				tmp.refineText.SetText(FormatRefineLevel(len(self.cols)))
				tmp.refineText.SetPosition(tmp.titleWnd.GetWidth() / 2 - tmp.refineText.GetTextSize()[0] / 2, tmp.titleWnd.GetHeight() / 2 - tmp.refineText.GetTextSize()[1] / 2 - 1)
				tmp.refineText.Show()
				
				tmp.refineMat = []
				tmp.refineMatText = []
				
				for j in xrange(self.STARTING_MAT_COUNT):
					tmp.refineMat.append(ui.ExpandedImageBox())
					tmp.refineMat[j].SetParent(tmp)
					tmp.refineMat[j].Show()
					
					tmp.refineMatText.append(ui.TextLine())
					tmp.refineMatText[j].SetParent(tmp)
					tmp.refineMatText[j].SetText("-")
					tmp.refineMatText[j].SetPosition(tmp.GetWidth() / 2 - tmp.refineMatText[j].GetTextSize()[0] / 2, self.ROW_START_Y[(1 + j)] + self.ROW_HEIGHTS[(1 + j)] / 2 - tmp.refineMatText[j].GetTextSize()[1] / 2 - 1)
					tmp.refineMatText[j].Show()
				
				tmp.moneyText = ui.TextLine()
				tmp.moneyText.SetParent(tmp)
				tmp.moneyText.SetText("-")
				tmp.moneyText.SetPosition(tmp.GetWidth() / 2 - tmp.moneyText.GetTextSize()[0] / 2, self.ROW_START_Y[(self.STARTING_MAT_COUNT+1)] + self.ROW_HEIGHTS[(self.STARTING_MAT_COUNT+1)] / 2 - tmp.moneyText.GetTextSize()[1] / 2 - 1)
				tmp.moneyText.Show()
				
				self.cols.append(tmp)
			self.goldText = ui.TextLine()
			self.goldText.SetParent(self.base)
			self.goldText.SetText(localeinfo.WIKI_REFINEINFO_YANG_COSTS)
			self.goldText.SetPosition(int(round((49 + 81 / 2) * scale_x)) - self.goldText.GetTextSize()[0] / 2, 21 + self.ROW_START_Y[(self.STARTING_MAT_COUNT+1)] + self.ROW_HEIGHTS[(self.STARTING_MAT_COUNT+1)] / 2 - self.goldText.GetTextSize()[1] / 2 - 1)
			self.goldText.Show()
			
			self.upgradeText = ui.TextLine()
			self.upgradeText.SetParent(self.base)
			self.upgradeText.SetText(localeinfo.WIKI_REFINEINFO_UPGRADE_COSTS)
			self.upgradeText.SetPosition(int(round((49 + 81 / 2) * scale_x)) - self.upgradeText.GetTextSize()[0] / 2, 21 + self.ROW_START_Y[0] + self.ROW_HEIGHTS[0] / 2 - self.upgradeText.GetTextSize()[1] / 2 - 1)
			self.upgradeText.Show()
			
			for i in xrange(item.LIMIT_MAX_NUM):
				(limitType, limitValue) = item.GetLimit(i)
				if item.LIMIT_LEVEL == limitType:
					self.levelLimit = limitValue
					break
			
			self.levelText = ui.TextLine()
			self.levelText.SetParent(self.base)
			self.levelText.SetText(localeinfo.WIKI_REFINEINFO_ITEM_LEVEL % self.levelLimit)
			self.levelText.SetPosition(self.base.GetWidth() - self.levelText.GetTextSize()[0] - 8, 1 + 19 / 2 - self.levelText.GetTextSize()[1] / 2)
			self.levelText.Show()
			"""
			itemName = item.GetItemName()
			fnd = itemName.find("+")
			if fnd >= 0:
				itemName = itemName[:fnd].strip()
				"""
			itemName = StripRefineSuffix(item.GetItemName())
			
			self.itemNameText = ui.TextLine()
			self.itemNameText.SetParent(self.base)
			self.itemNameText.SetText(itemName)
			self.itemNameText.SetPosition(5, 1 + 19 / 2 - self.itemNameText.GetTextSize()[1] / 2)
			self.itemNameText.Show()
			
			self.itemImage = ui.ExpandedImageBox()
			self.itemImage.SetParent(self.base)
			self.itemImage.LoadImage(item.GetIconImageFileName())
			self.itemImage.SetPosition(int(round((1 + 47 / 2) * scale_x)) - self.itemImage.GetWidth() / 2, 21 + 134 / 2 - self.itemImage.GetHeight() / 2)
			self.itemImage.Show()
			self.itemImage.SetStringEvent("MOUSE_OVER_IN",ui.__mem_func__( self.parent.OnOverIn), self.vnum + (9 if wiki.CanIncrRefineLevel() else 0))
			self.itemImage.SetStringEvent("MOUSE_OVER_OUT",ui.__mem_func__( self.parent.OnOverOut))
			if wiki.GetBaseClass() and allow_special_page:
				self.itemImage.SetStringEvent("MOUSE_LEFT_DOWN", ui.__mem_func__(wiki.GetBaseClass().OpenSpecialPage), self.parent, self.vnum, False)
			
			self.SetSize(self.base.GetWidth(), self.base.GetHeight())
			
			self.scrollBoard = ui.Window()
			self.scrollBoard.SetParent(self)
			self.scrollBoard.AddFlag("attach")
			self.scrollBoard.AddFlag("not_pick")
			self.scrollBoard.Hide()
			
			self.RegisterHorizontalScrollBar()
		
		def __del__(self):
			if wiki.GetBaseClass():
				wiki.GetBaseClass().objList.pop(long(id(self)))
			
			ui.Window.__del__(self)
		
		def GetMaterialCount(self, refine_count, retInfo):
			maxMat = self.STARTING_MAT_COUNT
			
			for i in xrange(refine_count):
				curr = 0
				
				for j in retInfo[i][1]:
					if j[0] == 0:
						continue
					
					if curr >= wiki.REFINE_MATERIAL_MAX_NUM:
						break
					
					curr += 1
				
				maxMat = max(maxMat, curr)
			
			return maxMat
		
		def ProcessRetInfo(self, retInfo, maxMat, moneyRow, useHeight, useStart, starting_col = 0):
			for i in xrange(wiki.MAX_REFINE_COUNT):
				money = MakeMoneyText(retInfo[(i+starting_col)][0])
				
				currWindow = self.cols[i+1]
				
				if self.parent:
					item.SelectItem(self.vnum)
					
					currWindow.titleWnd.SAFE_SetOverInEvent(self.parent.OnOverIn, self.vnum + ((1+i+starting_col) if wiki.CanIncrRefineLevel() else 0))
					currWindow.titleWnd.SAFE_SetOverOutEvent(self.parent.OnOverOut)
				
				currWindow.refineText.SetText(FormatRefineLevel((i+1) + starting_col))
				currWindow.refineText.SetPosition(currWindow.titleWnd.GetWidth() / 2 - currWindow.refineText.GetTextSize()[0] / 2,\
										currWindow.titleWnd.GetHeight() / 2 - currWindow.refineText.GetTextSize()[1] / 2 - 1)
				
				currWindow.moneyText.SetText(money)
				currWindow.moneyText.SetPosition(currWindow.GetWidth() / 2 - currWindow.moneyText.GetTextSize()[0] / 2,\
										useStart[moneyRow] + useHeight[moneyRow] / 2 - currWindow.moneyText.GetTextSize()[1] / 2 - 1)
				
				curr = 0
				
				for j in retInfo[(i+starting_col)][1]:
					if curr >= maxMat:
						break
					
					if j[0] != 0:
						item.SelectItem(j[0])
						
						currWindow.refineMat[curr].LoadImage(item.GetIconImageFileName())
						currWindow.refineMat[curr].SetPosition(currWindow.GetWidth() / 2 - currWindow.refineMat[curr].GetWidth() / 2, useStart[curr + 1] + useHeight[curr + 1] / 2 - currWindow.refineMat[curr].GetHeight() / 2)
						currWindow.refineMat[curr].SetStringEvent("MOUSE_OVER_IN",ui.__mem_func__( self.parent.OnOverIn), j[0])
						currWindow.refineMat[curr].SetStringEvent("MOUSE_OVER_OUT",ui.__mem_func__( self.parent.OnOverOut))
						
						currWindow.refineMatText[curr].SetText(str(j[1]))
						currWindow.refineMatText[curr].SetFontColor(1.0, 1.0, 1.0)
						currWindow.refineMatText[curr].SetPosition(currWindow.refineMat[curr].GetLocalPosition()[0] + currWindow.refineMat[curr].GetWidth() - currWindow.refineMatText[curr].GetTextSize()[0],\
															currWindow.refineMat[curr].GetLocalPosition()[1] + currWindow.refineMat[curr].GetHeight() - currWindow.refineMatText[curr].GetTextSize()[1])
					
					else:
						currWindow.refineMat[curr].UnloadImage()
						
						currWindow.refineMatText[curr].SetText("-")
						currWindow.refineMatText[curr].SetFontColor(0.78, 0.78, 0.78)
						currWindow.refineMatText[curr].SetPosition(currWindow.GetWidth() / 2 - currWindow.refineMatText[curr].GetTextSize()[0] / 2,\
													useStart[curr + 1] + useHeight[curr + 1] / 2 - currWindow.refineMatText[curr].GetTextSize()[1] / 2 - 1)
					curr += 1
		
		def NoticeMe(self, noticeType = [False, 0]):
			(scrollingNoticeType, starting_col) = noticeType
			(max_refine_count, retInfo) = wiki.GetRefineInfo(self.vnum)
			if not retInfo:
				return
			#chat.AppendChat(chat.CHAT_TYPE_INFO, "max_refine_count=%d, MAX_REFINE_COUNT=%d" % (max_refine_count, wiki.MAX_REFINE_COUNT))

			visible_cols = len(self.cols) - 1
			if not scrollingNoticeType and max_refine_count > visible_cols:
				starting_col = max_refine_count - visible_cols

			if self.itemImage and self.parent:
				item.SelectItem(self.vnum)
				
				self.itemImage.SetStringEvent("MOUSE_OVER_IN",ui.__mem_func__( self.parent.OnOverIn), self.vnum + (max_refine_count if wiki.CanIncrRefineLevel() else 0))
				self.itemImage.SetStringEvent("MOUSE_OVER_OUT",ui.__mem_func__( self.parent.OnOverOut))
			
			maxMat = self.GetMaterialCount(max_refine_count, retInfo)
			moneyRow = (maxMat + 1)
			
			if maxMat > self.STARTING_MAT_COUNT:
				if not scrollingNoticeType:
					self.SetExpandedRow(maxMat)
				
				useHeight = self.ROW_HEIGHTS_EXPANDED[maxMat][0]
				useStart = self.ROW_HEIGHTS_EXPANDED[maxMat][1]
			else:
				useHeight = self.ROW_HEIGHTS
				useStart = self.ROW_START_Y
			
			if not scrollingNoticeType:
				self.LoadHorizontalScrollBoard(max_refine_count, starting_col)
			
			self.ProcessRetInfo(retInfo, maxMat, moneyRow, useHeight, useStart, starting_col)
		
		def SetExpandedRow(self, materialCount):
			(useHeight, useStart) = self.ROW_HEIGHTS_EXPANDED[materialCount]
			oldSize = self.base.GetHeight()
			
			self.base.SetGridSize(materialCount)
			self.SetSize(self.base.GetWidth(), self.base.GetHeight())
			
			for tmp in self.cols:
				tmp.SetSize(tmp.GetWidth(), ((useStart[-1] - 2) + useHeight[-1]))
				
				tmp.refineMat = []
				tmp.refineMatText = []
				
				for j in xrange(materialCount):
					tmp.refineMat.append(ui.ExpandedImageBox())
					tmp.refineMat[j].SetParent(tmp)
					tmp.refineMat[j].Show()
				
					tmp.refineMatText.append(ui.TextLine())
					tmp.refineMatText[j].SetParent(tmp)
					tmp.refineMatText[j].SetText("-")
					tmp.refineMatText[j].SetPosition(tmp.GetWidth() / 2 - tmp.refineMatText[j].GetTextSize()[0] / 2,\
													useStart[j + 1] + useHeight[j + 1] / 2 - tmp.refineMatText[j].GetTextSize()[1] / 2 - 1)
					tmp.refineMatText[j].Show()
				
				tmp.moneyText = ui.TextLine()
				tmp.moneyText.SetParent(tmp)
				tmp.moneyText.SetText("-")
				tmp.moneyText.SetPosition(tmp.GetWidth() / 2 - tmp.moneyText.GetTextSize()[0] / 2,\
										useStart[(materialCount + 1)] + useHeight[(materialCount + 1)] / 2 - tmp.moneyText.GetTextSize()[1] / 2 - 1)
				tmp.moneyText.Show()
			
			self.goldText.SetPosition(49 + 81 / 2 - self.goldText.GetTextSize()[0] / 2, 21 + useStart[(materialCount + 1)] + useHeight[(materialCount + 1)] / 2 - self.goldText.GetTextSize()[1] / 2 - 1)
			self.itemImage.SetPosition(1 + 47 / 2 - self.itemImage.GetWidth() / 2, 10 + (self.base.GetHeight() / 2) - (self.itemImage.GetHeight() / 2))
			
			if self.parent:
				self.parent.ChangeElementSize(self, self.base.GetHeight() - oldSize)
		
		def LoadHorizontalScrollBoard(self, max_refine_count, starting_col=0):
			(baseWidth, baseHeight) = (self.base.GetWidth(), self.base.GetHeight())
			scrollBoardHeight = 7
			
			visible_cols = len(self.cols) - 1
			if max_refine_count > visible_cols and self.scrollBoard and self.scrollBar:
				size_perc = truediv(visible_cols, max_refine_count)
				denom = float(max_refine_count - visible_cols)
				
				self.SetSize(baseWidth, baseHeight + (scrollBoardHeight - 1))
				
				self.scrollBoard.SetSize(baseWidth, scrollBoardHeight)
				self.scrollBoard.SetPosition(0, (baseHeight - 1))
				self.scrollBoard.Show()
				
				size_perc = truediv(wiki.MAX_REFINE_COUNT, max_refine_count)
				new_size = int(size_perc * self.scrollBoard.GetWidth())
				
				self.scrollBar.SetSize(self.scrollBoard.GetWidth(), self.scrollBoard.GetHeight())
				self.scrollBar.SetStaticScale(new_size)

				denom = float(max_refine_count - wiki.MAX_REFINE_COUNT)
				pos_scale = (float(starting_col) / denom) if denom > 0.0 else 0.0

				self.scrollBar.SetPosScale(pos_scale)
				self.scrollBar.Show()
				
				if self.parent:
					self.parent.ChangeElementSize(self, (scrollBoardHeight - 1))
			else:
				self.SetSize(baseWidth, baseHeight)
				self.scrollBoard.Hide()
				
				if self.parent:
					self.parent.ChangeElementSize(self, self.GetHeight() - baseHeight)
		
		def RegisterHorizontalScrollBar(self):
			if not self.scrollBoard:
				return
			
			self.scrollBar = WikiScrollBar(True)
			self.scrollBar.SetParent(self.scrollBoard)
			self.scrollBar.SetPosition(0, 0)
			self.scrollBar.SetScrollEvent(self.OnScrollBar)
			self.scrollBar.SetScrollSpeed(self.SCROLL_SPEED)
			self.scrollBar.Show()
		
		def OnScrollBar(self, fScale):
			(max_refine_count, _) = wiki.GetRefineInfo(self.vnum)
			
			if max_refine_count <= wiki.MAX_REFINE_COUNT:
				return
			
			visible_cols = len(self.cols) - 1
			starting_col = int(fScale * (max_refine_count - visible_cols))
			self.NoticeMe([True, starting_col])
		
		def OnRender(self):
			if self.additionalLoaded:
				return
			
			if wiki.IsSet(self.vnum) or not hasattr(self.parent, "loadFrom") or\
				self.parent.loadFrom == self.parent.loadTo:
				self.additionalLoaded = True
				wiki.LoadInfo(long(id(self)), self.vnum)

	"""Static values!
		Please dont touch on them."""
	ELEM_PADDING = 5
	SCROLL_SPEED = 50
	ITEM_LOAD_PER_UPDATE = 1
	
	classButtonsPath = "d:/ymir work/ui/wiki/class_buttons/"
	
	CLASS_BUTTONS = [
		[
			item.ITEM_ANTIFLAG_WARRIOR,
			classButtonsPath + "class_w_normal.tga",
			classButtonsPath + "class_w_hover.tga",
			classButtonsPath + "class_w_selected.tga"
		],
		[
			item.ITEM_ANTIFLAG_ASSASSIN,
			classButtonsPath + "class_n_normal.tga",
			classButtonsPath + "class_n_hover.tga",
			classButtonsPath + "class_n_selected.tga"
		],
		[
			item.ITEM_ANTIFLAG_SHAMAN,
			classButtonsPath + "class_s_normal.tga",
			classButtonsPath + "class_s_hover.tga",
			classButtonsPath + "class_s_selected.tga"
		],
		[
			item.ITEM_ANTIFLAG_SURA,
			classButtonsPath + "class_su_normal.tga",
			classButtonsPath + "class_su_hover.tga",
			classButtonsPath + "class_su_selected.tga"
		]
	]

	def GetRandomChar(self):
		ASSASSINS 		= [ player.MAIN_RACE_ASSASSIN_W, player.MAIN_RACE_ASSASSIN_M ]
		WARRIORS 		= [ player.MAIN_RACE_WARRIOR_W, player.MAIN_RACE_WARRIOR_M ]
		SURAS 			= [ player.MAIN_RACE_SURA_W, player.MAIN_RACE_SURA_M ]
		SHAMANS 		= [ player.MAIN_RACE_SHAMAN_W, player.MAIN_RACE_SHAMAN_M ]
		ITEM_CHARACTERS = [ ASSASSINS, WARRIORS, SURAS, SHAMANS ]
		
		SEX_FEMALE		= 0
		SEX_MALE		= 1
		ITEM_SEX		= [ SEX_FEMALE, SEX_MALE ]
		
		#-#
		
		if item.IsAntiFlag( item.ITEM_ANTIFLAG_MALE ):
			ITEM_SEX.remove( SEX_MALE )
		
		if item.IsAntiFlag( item.ITEM_ANTIFLAG_FEMALE ):
			ITEM_SEX.remove( SEX_FEMALE )
		
		#-#
		
		if item.IsAntiFlag( item.ITEM_ANTIFLAG_WARRIOR ):
			ITEM_CHARACTERS.remove( WARRIORS )
		
		if item.IsAntiFlag( item.ITEM_ANTIFLAG_SURA ):
			ITEM_CHARACTERS.remove( SURAS )
		
		if item.IsAntiFlag( item.ITEM_ANTIFLAG_ASSASSIN ):
			ITEM_CHARACTERS.remove( ASSASSINS )
		
		if item.IsAntiFlag( item.ITEM_ANTIFLAG_SHAMAN ):
			ITEM_CHARACTERS.remove( SHAMANS )
		
		return ITEM_CHARACTERS[app.GetRandom(0, len(ITEM_CHARACTERS) - 1)][ITEM_SEX[app.GetRandom(0, len(ITEM_SEX) - 1)]]

	def __init__(self):
		ui.Window.__init__(self)
		
		self.toolTip = uitooltip.ItemToolTip()
		
		self.SetSize(*mainBoardSize)
		
		self.elements = []
		self.scrollBar = None
		
		self.isOpened = False
		self.loadFrom = 0
		self.loadTo = 0
		
		self.wikiRenderTarget = WikiRenderTarget(150, 200)
		self.wikiRenderTarget.SetParent(self.toolTip)
		self.wikiRenderTarget.AddFlag("not_pick")
		self.wikiRenderTarget.SetPosition(5, self.toolTip.toolTipHeight)
		self.wikiRenderTarget.Hide()
		
		self.currFlag = 0
		self.currCateg = 0
		
		self.peekWindow = ui.Window()
		self.peekWindow.SetParent(self)
		self.peekWindow.AddFlag("attach")
		self.peekWindow.AddFlag("not_pick")
		# Modern layout compatibility: avoid hardcoded widths (541) which cause overflow on custom sizes
		self.peekWindow.SetSize(self.GetWidth() - 14, self.GetHeight() - 50 - 10)
		self.peekWindow.SetPosition(5, 50)
		self.peekWindow.SetInsideRender(True)
		self.peekWindow.Show()
		
		self.categWindow = ui.Window()
		self.categWindow.SetParent(self)
		self.categWindow.AddFlag("attach")
		self.categWindow.AddFlag("not_pick")
		self.categWindow.Show()
		
		self.classBtns = []
		for info in self.CLASS_BUTTONS:
			idx = len(self.classBtns)
			
			self.classBtns.append(ui.RadioButton())
			self.classBtns[idx].SetParent(self.categWindow)
			self.classBtns[idx].SetUpVisual(info[1])
			self.classBtns[idx].SetOverVisual(info[2])
			self.classBtns[idx].SetDownVisual(info[3])
			self.classBtns[idx].SetEvent(ui.__mem_func__(self.OnSelectCateg), proxy(self.classBtns[idx]), info[0])
			self.classBtns[idx].SetPosition(self.categWindow.GetWidth() + 5 * self.CLASS_BUTTONS.index(info), 0)
			self.classBtns[idx].Show()
			
			self.categWindow.SetSize(self.categWindow.GetWidth() + self.classBtns[idx].GetWidth(), self.classBtns[idx].GetHeight())
		
		self.categWindow.SetSize(self.categWindow.GetWidth() + 5 * (len(self.CLASS_BUTTONS) - 1), self.categWindow.GetHeight())
		self.categWindow.SetPosition(self.GetWidth() / 2 - self.categWindow.GetWidth() / 2, 10)
		
		self.scrollBoard = ui.Window()
		self.scrollBoard.SetParent(self.peekWindow)
		self.scrollBoard.AddFlag("attach")
		self.scrollBoard.AddFlag("not_pick")
		self.scrollBoard.Show()
		
		self.RegisterScrollBar()
	
	def OpenWindow(self):
		wndMgr.Show(self.hWnd)
	
	def Show(self, categID):
		wndMgr.Show(self.hWnd)
		
		isChanged = (not categID == self.currCateg)
		self.currCateg = categID
		
		if not self.isOpened:
			self.isOpened = True
			
			if len(self.classBtns):
				self.OnSelectCateg(proxy(self.classBtns[0]), self.CLASS_BUTTONS[0][0])
		else:
			self.loadTo = wiki.LoadClassItems(self.currCateg, self.currFlag)
			
			if self.loadFrom > self.loadTo or isChanged:
				del self.elements[:]
				
				self.scrollBoard.SetSize(0, 0)
				self.UpdateScrollbar()
				self.loadFrom = 0
	
	def OnOverIn(self, vnum):
		if not self.toolTip:
			return
		
		item.SelectItem(vnum)
		
		itemType = item.GetItemType()
		subType = item.GetItemSubType()
		
		if self.wikiRenderTarget:
			self.wikiRenderTarget.Hide()
		
		self.toolTip.ClearToolTip()
		metinSlot = [0 for i in xrange(player.METIN_SOCKET_MAX_NUM)]
		if (itemType == item.ITEM_TYPE_WEAPON and subType != 6) or (itemType == item.ITEM_TYPE_ARMOR and subType == 0):
			self.toolTip.SetThinBoardSize(self.wikiRenderTarget.GetWidth() + 10, self.wikiRenderTarget.GetHeight() + self.toolTip.toolTipHeight)
			self.toolTip.childrenList.append(self.wikiRenderTarget)
			self.toolTip.ResizeToolTip()
			
			char_type = self.GetRandomChar()
			item.SelectItem(vnum)
			
			itemType = item.GetItemType()
			subType = item.GetItemSubType()
			char_type = self.GetRandomChar()
			
			self.wikiRenderTarget.SetModel(char_type)
			if (itemType == item.ITEM_TYPE_WEAPON or itemType == item.ITEM_TYPE_WEAPON and subType != 6):
				self.wikiRenderTarget.SetWeaponModel(vnum)
			elif itemType == item.ITEM_TYPE_ARMOR and subType == 0:
				self.wikiRenderTarget.SetModelForm(vnum)
			
			self.toolTip.AddItemData(vnum, metinSlot, 0, wiki = 1)
			self.wikiRenderTarget.SetPosition(self.toolTip.GetWidth() / 2 - self.wikiRenderTarget.GetWidth() / 2, self.wikiRenderTarget.GetLocalPosition()[1])
			self.wikiRenderTarget.Show()
		else:
			self.toolTip.AddItemData(vnum, metinSlot, 0, wiki = 1)

	def OnOverOut(self):
		if not self.toolTip:
			return
		
		self.toolTip.Hide()
	
	def OnSelectCateg(self, btn, flag):
		self.currFlag = flag
		
		for tmpBtn in self.classBtns:
			tmpBtn.SetUp() if proxy(tmpBtn) != btn else tmpBtn.Down()
		
		del self.elements[:]
		
		self.scrollBoard.SetSize(0, 0)
		self.UpdateScrollbar()
		
		self.loadTo = wiki.LoadClassItems(self.currCateg, self.currFlag)
		self.loadFrom = 0
	
	def OnUpdate(self):
		if self.loadFrom < self.loadTo:
			for i in wiki.ChangePage(self.loadFrom, min(self.loadTo, self.loadFrom + self.ITEM_LOAD_PER_UPDATE)):
				self.AddItem(i)
				self.loadFrom += 1
	
	def ChangeElementSize(self, elem, sizeDiff):
		foundItem = False
		
		for i in self.elements:
			if elem != i and not foundItem:
				continue
			elif elem == i:
				foundItem = True
				continue
			
			i.SetPosition(i.GetLocalPosition()[0],\
							i.GetLocalPosition()[1] + sizeDiff)
		
		if foundItem:
			self.scrollBoard.SetSize(elem.GetWidth(), self.scrollBoard.GetHeight() + sizeDiff)
			self.UpdateScrollbar()
	
	def AddItem(self, vnum):
		startRefineVnum = wiki.GetWikiItemStartRefineVnum(vnum)
		vnum = startRefineVnum if startRefineVnum != 0 else (int(vnum / 10) * 10)
		
		for i in self.elements:
			if vnum == i.vnum:
				return None
		
		item.SelectItem(vnum)
		
		tmp = self.WikiItem(vnum, self)
		tmp.SetParent(self.scrollBoard)
		tmp.AddFlag("attach")
		
		addPadding = 0
		totalElem = len(self.elements)
		if totalElem > 0:
			lastIndex = 0
			
			for i in xrange(totalElem):
				if self.elements[i].levelLimit < tmp.levelLimit or self.elements[i].levelLimit == tmp.levelLimit and self.elements[i].vnum < tmp.vnum:
					break
				
				lastIndex += 1
			
			self.elements.insert(lastIndex, tmp)
			totalElem += 1
			
			for i in xrange(lastIndex, totalElem):
				self.elements[i].SetPosition(0,\
							(0 if i == 0 else self.elements[i - 1].GetLocalPosition()[1] + self.elements[i - 1].GetHeight() + self.ELEM_PADDING))
			
			addPadding = self.ELEM_PADDING
		else:
			self.elements.append(tmp)
		
		tmp.Show()
		
		self.scrollBoard.SetSize(tmp.GetWidth(),\
								self.scrollBoard.GetHeight() + addPadding + tmp.GetHeight())
		self.UpdateScrollbar()
		
		return tmp
	
	def OnRunMouseWheel(self, length):
		if self.peekWindow.IsInPosition():
			val = int((length * 0.01) * self.SCROLL_SPEED)
			self.UpdateScrollbar(val)
			
			return True
		
		return False
	
	def OnScrollBar(self, fScale):
		curr = min(0, max(math.ceil((self.scrollBoard.GetHeight() - self.peekWindow.GetHeight()) * fScale * -1.0), -self.scrollBoard.GetHeight() + self.peekWindow.GetHeight()))
		self.scrollBoard.SetPosition(0, curr)
	
	def ChangeScrollbar(self):
		if not self.scrollBar:
			return
		
		if self.scrollBoard.GetHeight() <= self.peekWindow.GetHeight():
			self.scrollBar.Hide()
		else:
			self.scrollBar.SetScale(self.peekWindow.GetHeight(), self.scrollBoard.GetHeight())
			self.scrollBar.SetPosScale(truediv(abs(self.scrollBoard.GetLocalPosition()[1]), (self.scrollBoard.GetHeight() - self.peekWindow.GetHeight())))
			self.scrollBar.Show()
	
	def UpdateScrollbar(self, val = 0):
		curr = min(0, max(self.scrollBoard.GetLocalPosition()[1] + val, -self.scrollBoard.GetHeight() + self.peekWindow.GetHeight()))
		self.scrollBoard.SetPosition(0, curr)
		
		self.ChangeScrollbar()
	
	def RegisterScrollBar(self):
		self.scrollBar = WikiScrollBar()
		self.scrollBar.SetParent(self)
		self.scrollBar.SetPosition(self.peekWindow.GetLocalPosition()[0] + self.peekWindow.GetWidth() + 1, self.peekWindow.GetLocalPosition()[1])
		self.scrollBar.SetSize(10, self.peekWindow.GetHeight())
		self.scrollBar.SetScrollEvent(self.OnScrollBar)
		self.scrollBar.SetScrollSpeed(self.SCROLL_SPEED)
		self.scrollBar.Show()
		
		self.ChangeScrollbar()

class ChestPeekWindow(ui.Window):
	"""Static values!
		Please dont touch on them."""
	ELEM_X_PADDING = 0
	ELEM_PADDING = 0
	SCROLL_SPEED = 25
	ELEM_PER_LINE = 11
	
	def __init__(self, parent, width, height, sendParent = True):
		ui.Window.__init__(self)
		
		self.SetSize(width, height)
		
		self.parent = proxy(parent)
		self.sendParent = sendParent
		
		self.elements = []
		self.posMap = {}
		self.scrollBar = None
		
		self.mOverInEvent = None
		self.mOverOutEvent = None
		self.mParent = None
		
		self.peekWindow = ui.Window()
		self.peekWindow.SetParent(self)
		self.peekWindow.AddFlag("attach")
		self.peekWindow.AddFlag("not_pick")
		self.peekWindow.SetSize(width-6, self.GetHeight())
		self.peekWindow.SetPosition(0, 0)
		self.peekWindow.Show()
		
		self.scrollBoard = ui.Window()
		self.scrollBoard.SetParent(self.peekWindow)
		self.scrollBoard.AddFlag("attach")
		self.scrollBoard.AddFlag("not_pick")
		self.scrollBoard.Show()
	
	def AddItem(self, vnumFrom, vnumTo):
		if not self.scrollBar:
			self.RegisterScrollBar()
		
		metinSlot = [0 for i in xrange(player.METIN_SOCKET_MAX_NUM)]
		if vnumFrom == 8:
			vnumFrom = 70104
			metinSlot[0] = vnumTo
		
		item.SelectItem(vnumFrom)
		
		tmp = ui.ExpandedImageBox()
		tmp.SetParent(self.scrollBoard)
		tmp.LoadImage(item.GetIconImageFileName())
		tmp.SetStringEvent("MOUSE_OVER_IN", self.mOverInEvent, vnumFrom, metinSlot)
		tmp.SetStringEvent("MOUSE_OVER_OUT", self.mOverOutEvent)
		
		if wiki.GetBaseClass():
			vnumEvent = -1
			
			if (item.GetItemType() == item.ITEM_TYPE_WEAPON or item.GetItemType() == item.ITEM_TYPE_ARMOR):
				startRefineVnum = wiki.GetWikiItemStartRefineVnum(vnumFrom)
				vnumEvent = startRefineVnum if startRefineVnum != 0 else (int(vnumFrom / 10) * 10)
			else:
				vnumEvent = vnumFrom
			
			if vnumEvent != -1:
				if not self.sendParent:
					tmp.SetStringEvent("MOUSE_LEFT_DOWN", ui.__mem_func__(wiki.GetBaseClass().OpenSpecialPage), None, vnumEvent, False)
				else:
					if hasattr(self.parent, "parent"):
						tmp.SetStringEvent("MOUSE_LEFT_DOWN", ui.__mem_func__(wiki.GetBaseClass().OpenSpecialPage), self.parent.parent, vnumEvent, False)
					else:
						tmp.SetStringEvent("MOUSE_LEFT_DOWN", ui.__mem_func__(wiki.GetBaseClass().OpenSpecialPage), self.parent, vnumEvent, False)
			
		tmp.itemSize = item.GetItemSize()[1]
		tmp.vnum = vnumFrom
		
		tmpDropText = ui.TextLine()
		tmpDropText.SetParent(tmp)
		tmpDropText.AddFlag("attach")
		tmpDropText.AddFlag("not_pick")
		tmpDropText.SetText((MakeMoneyText(vnumTo) if vnumFrom == 1 else (str(vnumTo) if vnumTo else "")))
		tmpDropText.SetPosition(tmp.GetWidth() - tmpDropText.GetTextSize()[0],\
									tmp.GetHeight() - tmpDropText.GetTextSize()[1])
		
		totalElem = len(self.elements)
		if totalElem > 0:
			currAdd = 0
			
			while True:
				if currAdd in self.posMap:
					currAdd += 1
					continue
				
				break
			
			totalLine = (currAdd % self.ELEM_PER_LINE)
			currH = (math.floor(currAdd / self.ELEM_PER_LINE) * (32 + self.ELEM_PADDING))
			
			for i in xrange(tmp.itemSize):
				self.posMap[currAdd + i * self.ELEM_PER_LINE] = True
			
			tmp.SetPosition(1 + totalLine * (36 + self.ELEM_X_PADDING), 0 + currH)
		else:
			for i in xrange(tmp.itemSize):
				self.posMap[i * self.ELEM_PER_LINE] = True
			
			tmp.SetPosition(1, 0)
		
		tmp.Show()
		tmpDropText.Show()
		
		self.elements.append((tmp, tmpDropText))
		
		self.scrollBoard.SetSize(self.peekWindow.GetWidth(), max(self.scrollBoard.GetHeight(), tmp.GetLocalPosition()[1] + tmp.GetHeight()))
		self.UpdateScrollbar()
		
		return tmp
	
	def OnRunMouseWheel(self, length):
		if self.peekWindow.IsInPosition():
			if self.scrollBar and self.scrollBar.IsShow():
				self.UpdateScrollbar(int((length * 0.01) * self.SCROLL_SPEED))
				return True
		
		return False
	
	def OnScrollBar(self, fScale):
		curr = min(0, max(math.ceil((self.scrollBoard.GetHeight() - self.peekWindow.GetHeight()) * fScale * -1.0), -self.scrollBoard.GetHeight() + self.peekWindow.GetHeight()))
		self.scrollBoard.SetPosition(0, curr)
	
	def ChangeScrollbar(self):
		if not self.scrollBar:
			return
		
		if self.scrollBoard.GetHeight() <= self.GetHeight():
			self.scrollBar.Hide()
		else:
			self.scrollBar.SetScale(self.GetHeight(), self.scrollBoard.GetHeight())
			self.scrollBar.SetPosScale(truediv(abs(self.scrollBoard.GetLocalPosition()[1]), (self.scrollBoard.GetHeight() - self.peekWindow.GetHeight())))
			self.scrollBar.Show()
	
	def UpdateScrollbar(self, val = 0):
		curr = min(0, max(self.scrollBoard.GetLocalPosition()[1] + val, -self.scrollBoard.GetHeight() + self.peekWindow.GetHeight()))
		self.scrollBoard.SetPosition(0, curr)
		
		self.ChangeScrollbar()
	
	def RegisterScrollBar(self):
		self.scrollBar = WikiScrollBar()
		self.scrollBar.SetParent(self)
		self.scrollBar.SetPosition(self.peekWindow.GetLocalPosition()[0] + self.peekWindow.GetWidth() + 1,\
									self.peekWindow.GetLocalPosition()[1])
		self.scrollBar.SetSize(5, self.peekWindow.GetHeight())
		self.scrollBar.SetScrollEvent(self.OnScrollBar)
		self.scrollBar.SetScrollSpeed(self.SCROLL_SPEED)
		self.scrollBar.Show()
		
		self.ChangeScrollbar()

class WikiMainChestWindow(ui.Window):
	class WikiItem(ui.Window):
		def __init__(self, vnum, parent):
			ui.Window.__init__(self)
			
			self.parent = proxy(parent)
			self.vnum = vnum
			self.additionalLoaded = False
			
			wikiBase = wiki.GetBaseClass()
			if id(self) not in wikiBase.objList:
				wikiBase.objList[long(id(self))] = proxy(self)
			
			# --- Modern wide row layout (fills content width, taller droplist) ---
			try:
				full_w = int(self.parent.peekWindow.GetWidth())
			except:
				full_w = 541
			
			# Layout constants
			self._leftW = 60
			self._originW = 120
			self._headerH = 20
			self._bodyY = 22
			self._dropH = 104
			
			full_h = self._bodyY + self._dropH + 2
			
			# Background panel
			try:
				panel = _MakePanel(self, 0, 0, full_w, full_h, MOD_COL_PANEL2)
				self.base = panel
			except:
				# fallback to old image if modern helpers not available
				self.base = ui.ExpandedImageBox()
				self.base.SetParent(self)
				self.base.AddFlag('attach')
				self.base.AddFlag('not_pick')
				self.base.LoadImage('d:/ymir work/ui/wiki/detail_chest.tga')
				self.base.Show()
				full_w = self.base.GetWidth()
				full_h = self.base.GetHeight()
			
			# Separators (only if modern panel used)
			if isinstance(self.base, ui.Bar):
				# header divider
				h = ui.Bar(); h.SetParent(self); h.AddFlag('attach'); h.AddFlag('not_pick')
				h.SetColor(MOD_COL_BORDER); h.SetPosition(1, self._headerH + 1); h.SetSize(full_w - 2, 1); h.Show()
				# left divider
				v1 = ui.Bar(); v1.SetParent(self); v1.AddFlag('attach'); v1.AddFlag('not_pick')
				v1.SetColor(MOD_COL_BORDER); v1.SetPosition(self._leftW, 1); v1.SetSize(1, full_h - 2); v1.Show()
				# origin divider
				originX = full_w - self._originW
				v2 = ui.Bar(); v2.SetParent(self); v2.AddFlag('attach'); v2.AddFlag('not_pick')
				v2.SetColor(MOD_COL_BORDER); v2.SetPosition(originX, 1); v2.SetSize(1, full_h - 2); v2.Show()
			
			# Compute column geometry
			originX = full_w - self._originW
			dropX = self._leftW + 2
			dropW = max(50, originX - dropX - 2)
			self._originX = originX
			self._dropX = dropX
			self._dropW = dropW
			
			# Icon
			try:
				item.SelectItem(self.vnum)
			except:
				pass
			self.chestImage = ui.ExpandedImageBox()
			self.chestImage.SetParent(self)
			self.chestImage.LoadImage(item.GetIconImageFileName())
			self.chestImage.SetPosition(1 + (self._leftW//2) - self.chestImage.GetWidth()//2, self._bodyY + self._dropH//2 - self.chestImage.GetHeight()//2)
			self.chestImage.SetStringEvent('MOUSE_OVER_IN', ui.__mem_func__(self.parent.OnOverIn), self.vnum)
			self.chestImage.SetStringEvent('MOUSE_OVER_OUT', ui.__mem_func__(self.parent.OnOverOut))
			self.chestImage.Show()
			
			# Drop list
			self.dropList = ChestPeekWindow(self, dropW, self._dropH)
			self.dropList.AddFlag('attach')
			self.dropList.SetParent(self)
			self.dropList.SetPosition(dropX, self._bodyY)
			self.dropList.mOverInEvent = ui.__mem_func__(self.parent.OnOverIn)
			self.dropList.mOverOutEvent = ui.__mem_func__(self.parent.OnOverOut)
			self.dropList.Show()
			
			# Headers
			self.originTextHead = ui.TextLine()
			self.originTextHead.SetParent(self)
			self.originTextHead.AddFlag('attach'); self.originTextHead.AddFlag('not_pick')
			self.originTextHead.SetText(localeinfo.WIKI_CHESTINFO_ORIGIN)
			self.originTextHead.SetPosition(originX + self._originW//2 - self.originTextHead.GetTextSize()[0]//2, 1 + self._headerH//2 - self.originTextHead.GetTextSize()[1]//2 - 1)
			self.originTextHead.Show()
			
			self.contentText = ui.TextLine()
			self.contentText.SetParent(self)
			self.contentText.AddFlag('attach'); self.contentText.AddFlag('not_pick')
			self.contentText.SetText(localeinfo.WIKI_CHESTINFO_CONTENTOF % item.GetItemName())
			self.contentText.SetPosition(dropX + dropW//2 - self.contentText.GetTextSize()[0]//2, 1 + self._headerH//2 - self.contentText.GetTextSize()[1]//2 - 1)
			self.contentText.Show()
			
			self.originText = ui.TextLine()
			self.originText.SetParent(self)
			self.originText.AddFlag('attach'); self.originText.AddFlag('not_pick')
			self.originText.SetText('-')
			self.originText.SetPosition(originX + self._originW//2 - self.originText.GetTextSize()[0]//2, self._bodyY + self._dropH//2 - self.originText.GetTextSize()[1]//2 - 1)
			self.originText.Show()
			
			self.SetSize(full_w, full_h)
		
		def __del__(self):
			if wiki.GetBaseClass():
				wiki.GetBaseClass().objList.pop(long(id(self)))
			
			ui.Window.__del__(self)
		
		def __GenerateMultiLine(self, text, maxWidth):
			currText = self.__GenerateSingleLine()
			textHolder = []
			
			tempText = ui.TextLine()
			tempText.Hide()
			
			splt = text.split(" ")
			currText.SetText(splt[0])
			splt = splt[1:]
			for i in splt:
				tempText.SetText(" " + i)
				if tempText.GetTextSize()[0] + currText.GetTextSize()[0] > maxWidth:
					currText.AdjustSize()
					textHolder.append(currText)
					currText = self.__GenerateSingleLine()
					currText.SetText(i)
				else:
					currText.SetText(currText.GetText() + " " + i)
			
			textHolder.append(currText)
			return textHolder
		
		def __GenerateSingleLine(self):
			text = ui.TextLine()
			text.SetParent(self.base)
			text.Show()
			return text
		
		def NoticeMe(self):
			ret = wiki.GetChestInfo(self.vnum)
			allow_lst = (len(ret) > 2)
			
			if not allow_lst:
				(dwOrigin, isCommon) = ret
			else:
				(dwOrigin, isCommon, lst) = ret
			
			self.originText.Hide()
			self.multiHolder = []
			
			if isCommon:
				self.multiHolder = self.__GenerateMultiLine(localeinfo.WIKI_CHESTINFO_COMMON_DROP, max(10, getattr(self, '_originW', 89) - 10))
			elif dwOrigin:
				self.multiHolder = self.__GenerateMultiLine(nonplayer.GetMonsterName(dwOrigin), max(10, getattr(self, '_originW', 89) - 10))
			else:
				self.originText.SetPosition(getattr(self,'_originX',451) + getattr(self,'_originW',89) / 2 - self.originText.GetTextSize()[0] / 2, getattr(self,'_bodyY',22) + getattr(self,'_dropH',66) / 2 - self.originText.GetTextSize()[1] / 2 - 1)
				self.originText.Show()
			
			for i in self.multiHolder:
				totalH = self.multiHolder[0].GetTextSize()[1] * len(self.multiHolder) + 3 * (len(self.multiHolder) - 1) if self.multiHolder else 0
				i.SetPosition(getattr(self,'_originX',451) + getattr(self,'_originW',89) / 2 - i.GetTextSize()[0] / 2, getattr(self,'_bodyY',22) + getattr(self,'_dropH',66) / 2 - totalH / 2 + 3 * self.multiHolder.index(i) + i.GetTextSize()[1] * self.multiHolder.index(i))
			
			if not allow_lst:
				return
			
			sizeLst = []
			orderedLst = []
			otherStuff = []
			
			for i in lst:
				if i[0] < wiki.CONTROL_ITEM_VNUM:
					otherStuff.append(i[:])
					continue
				
				item.SelectItem(i[0])
				size = item.GetItemSize()[1]
				
				lastPos = 0
				for k in xrange(len(sizeLst)):
					if sizeLst[k] < size:
						break
					
					lastPos += 1
				
				sizeLst.insert(lastPos, size)
				orderedLst.insert(lastPos, i[:])
			
			for i in orderedLst:
				count = (i[1] if i[1] > 1 else 0)
				self.dropList.AddItem(i[0], count)
			
			for i in otherStuff:
				self.dropList.AddItem(i[0], i[1])
		
		def OnRender(self):
			if self.additionalLoaded:
				return
			
			if wiki.IsSet(self.vnum) or self.parent.loadFrom == self.parent.loadTo:
				self.additionalLoaded = True
				wiki.LoadInfo(long(id(self)), self.vnum)
	
	"""Static values!
		Please dont touch on them."""
	ELEM_PADDING = 5
	SCROLL_SPEED = 50
	ITEM_LOAD_PER_UPDATE = 1
	
	def __init__(self):
		ui.Window.__init__(self)
		
		self.toolTip = uitooltip.ItemToolTip()
		
		self.SetSize(*mainBoardSize)
		
		self.elements = []
		self.scrollBar = None
		
		self.chestVnums = []
		
		self.isOpened = False
		self.loadFrom = 0
		self.loadTo = 0
		
		self.peekWindow = ui.Window()
		self.peekWindow.SetParent(self)
		self.peekWindow.AddFlag("attach")
		self.peekWindow.AddFlag("not_pick")
		# Modern layout compatibility
		self.peekWindow.SetSize(self.GetWidth() - 14, self.GetHeight() - 15)
		self.peekWindow.SetPosition(5, 5)
		self.peekWindow.SetInsideRender(True)
		self.peekWindow.Show()
		
		self.scrollBoard = ui.Window()
		self.scrollBoard.SetParent(self.peekWindow)
		self.scrollBoard.AddFlag("attach")
		self.scrollBoard.AddFlag("not_pick")
		self.scrollBoard.Show()
		
		self.RegisterScrollBar()
	
	def OpenWindow(self):
		wndMgr.Show(self.hWnd)
	
	def Show(self, vnums):
		wndMgr.Show(self.hWnd)
		
		isChanged = (not len(vnums) == len(self.chestVnums))
		
		if not isChanged:
			for i in vnums:
				if i not in self.chestVnums:
					isChanged = True
					break
		
		if not isChanged:
			for i in self.chestVnums:
				if i not in vnums:
					isChanged = True
					break
		
		self.chestVnums = vnums[:]
		self.loadTo = len(self.chestVnums)
		
		if not self.isOpened:
			self.isOpened = True
			self.loadFrom = 0
		
		if self.loadFrom > self.loadTo or isChanged:
			del self.elements[:]
			self.loadFrom = 0
			
			self.scrollBoard.SetSize(0, 0)
			self.UpdateScrollbar()
	
	def OnOverIn(self, vnum, metinSlot = [0 for i in xrange(player.METIN_SOCKET_MAX_NUM)]):
		if not self.toolTip:
			return
		
		self.toolTip.ClearToolTip()
		self.toolTip.AddItemData(vnum, metinSlot, 0, wiki = 1)
	
	def OnOverOut(self):
		if not self.toolTip:
			return
		
		self.toolTip.Hide()
	
	def OnUpdate(self):
		if self.loadFrom < self.loadTo:
			for i in xrange(self.loadFrom, min(self.loadTo, self.loadFrom + self.ITEM_LOAD_PER_UPDATE)):
				if type(self.chestVnums[i]) == types.ListType:
					for idx in self.chestVnums[i]:
						self.AddItem(idx)
				else:
					self.AddItem(self.chestVnums[i])
				self.loadFrom += 1
	
	def AddItem(self, vnum):
		for i in self.elements:
			if vnum == i.vnum:
				return None
		
		item.SelectItem(vnum)
		
		tmp = self.WikiItem(vnum, self)
		tmp.SetParent(self.scrollBoard)
		tmp.AddFlag("attach")
		
		addPadding = 0
		totalElem = len(self.elements)
		
		if totalElem > 0:
			lastIndex = totalElem
			
			self.elements.insert(lastIndex, tmp)
			totalElem += 1
			
			for i in xrange(lastIndex, totalElem):
				self.elements[i].SetPosition(0,\
					(0 if i == 0 else self.elements[i - 1].GetLocalPosition()[1] + self.elements[i - 1].GetHeight() + self.ELEM_PADDING))
			
			addPadding = self.ELEM_PADDING
		else:
			self.elements.append(tmp)
		
		tmp.Show()
		
		self.scrollBoard.SetSize(tmp.GetWidth(), self.scrollBoard.GetHeight() + addPadding + tmp.GetHeight())
		self.UpdateScrollbar()
		
		return tmp
	
	def OnRunMouseWheel(self, length):
		if self.peekWindow.IsInPosition():
			self.UpdateScrollbar(int((length * 0.01) * self.SCROLL_SPEED))
			return True
		
		return False
	
	def OnScrollBar(self, fScale):
		curr = min(0, max(math.ceil((self.scrollBoard.GetHeight() - self.peekWindow.GetHeight()) * fScale * -1.0), -self.scrollBoard.GetHeight() + self.peekWindow.GetHeight()))
		self.scrollBoard.SetPosition(0, curr)
	
	def ChangeScrollbar(self):
		if not self.scrollBar:
			return
		
		if self.scrollBoard.GetHeight() <= self.GetHeight():
			self.scrollBar.Hide()
		else:
			self.scrollBar.SetScale(self.GetHeight(), self.scrollBoard.GetHeight())
			self.scrollBar.SetPosScale(truediv(abs(self.scrollBoard.GetLocalPosition()[1]), (self.scrollBoard.GetHeight() - self.peekWindow.GetHeight())))
			self.scrollBar.Show()
	
	def UpdateScrollbar(self, val = 0):
		curr = min(0, max(self.scrollBoard.GetLocalPosition()[1] + val, -self.scrollBoard.GetHeight() + self.peekWindow.GetHeight()))
		self.scrollBoard.SetPosition(0, curr)
		
		self.ChangeScrollbar()
	
	def RegisterScrollBar(self):
		self.scrollBar = WikiScrollBar()
		self.scrollBar.SetParent(self)
		self.scrollBar.SetPosition(self.peekWindow.GetLocalPosition()[0] + self.peekWindow.GetWidth() + 1, self.peekWindow.GetLocalPosition()[1])
		self.scrollBar.SetSize(10, self.peekWindow.GetHeight())
		self.scrollBar.SetScrollEvent(self.OnScrollBar)
		self.scrollBar.SetScrollSpeed(self.SCROLL_SPEED)
		self.scrollBar.Show()
		
		self.ChangeScrollbar()

class WikiMainBossWindow(ui.Window):
	class WikiItem(ui.Window):
		def __init__(self, vnum, parent):
			ui.Window.__init__(self)
			
			self.parent = proxy(parent)
			self.vnum = vnum
			
			wikiBase = wiki.GetBaseClass()
			if id(self) not in wikiBase.objList:
				wikiBase.objList[long(id(self))] = proxy(self)
			
			self.additionalLoaded = False
			
			# --- Modern wide row layout (fills content width, taller droplist) ---
			try:
				full_w = int(self.parent.peekWindow.GetWidth())
			except:
				full_w = 541
			
			self._leftW = 170  # bigger for render target
			self._originW = 120
			self._headerH = 20
			self._bodyY = 22
			self._dropH = 104
			full_h = self._bodyY + self._dropH + 2
			
			try:
				panel = _MakePanel(self, 0, 0, full_w, full_h, MOD_COL_PANEL2)
				self.base = panel
			except:
				self.base = ui.ExpandedImageBox()
				self.base.SetParent(self)
				self.base.AddFlag('attach')
				self.base.AddFlag('not_pick')
				self.base.LoadImage('d:/ymir work/ui/wiki/detail_chest.tga')
				self.base.Show()
				full_w = self.base.GetWidth()
				full_h = self.base.GetHeight()
			
			if isinstance(self.base, ui.Bar):
				h = ui.Bar(); h.SetParent(self); h.AddFlag('attach'); h.AddFlag('not_pick')
				h.SetColor(MOD_COL_BORDER); h.SetPosition(1, self._headerH + 1); h.SetSize(full_w - 2, 1); h.Show()
				v1 = ui.Bar(); v1.SetParent(self); v1.AddFlag('attach'); v1.AddFlag('not_pick')
				v1.SetColor(MOD_COL_BORDER); v1.SetPosition(self._leftW, 1); v1.SetSize(1, full_h - 2); v1.Show()
				originX = full_w - self._originW
				v2 = ui.Bar(); v2.SetParent(self); v2.AddFlag('attach'); v2.AddFlag('not_pick')
				v2.SetColor(MOD_COL_BORDER); v2.SetPosition(originX, 1); v2.SetSize(1, full_h - 2); v2.Show()
			
			originX = full_w - self._originW
			dropX = self._leftW + 2
			dropW = max(50, originX - dropX - 2)
			self._originX = originX
			self._dropX = dropX
			self._dropW = dropW
			
			# Render target preview
			self.modelView = WikiRenderTarget(160, self._dropH)
			self.modelView.SetParent(self)
			self.modelView.SetPosition(6, self._bodyY)
			if wiki.GetBaseClass():
				self.modelView.SetMouseLeftButtonDownEvent(ui.__mem_func__(wiki.GetBaseClass().OpenSpecialPage), self.parent, self.vnum, True)
			self.modelView.Hide()
			
			self.dropList = ChestPeekWindow(self, dropW, self._dropH)
			self.dropList.AddFlag('attach')
			self.dropList.SetParent(self)
			self.dropList.SetPosition(dropX, self._bodyY)
			self.dropList.mOverInEvent = ui.__mem_func__(self.parent.OnOverIn)
			self.dropList.mOverOutEvent = ui.__mem_func__(self.parent.OnOverOut)
			self.dropList.mParent = self.parent
			self.dropList.Show()
			
			self.originTextHead = ui.TextLine()
			self.originTextHead.SetParent(self)
			self.originTextHead.AddFlag('attach'); self.originTextHead.AddFlag('not_pick')
			self.originTextHead.SetText(localeinfo.WIKI_CHESTINFO_ORIGIN)
			self.originTextHead.SetPosition(originX + self._originW//2 - self.originTextHead.GetTextSize()[0]//2, 1 + self._headerH//2 - self.originTextHead.GetTextSize()[1]//2 - 1)
			self.originTextHead.Show()
			
			self.contentText = ui.TextLine()
			self.contentText.SetParent(self)
			self.contentText.AddFlag('attach'); self.contentText.AddFlag('not_pick')
			self.contentText.SetText(localeinfo.WIKI_MONSTERINFO_DROPLISTOF % nonplayer.GetMonsterName(self.vnum))
			self.contentText.SetPosition(dropX + dropW//2 - self.contentText.GetTextSize()[0]//2, 1 + self._headerH//2 - self.contentText.GetTextSize()[1]//2 - 1)
			self.contentText.Show()
			
			self.originText = ui.TextLine()
			self.originText.SetParent(self)
			self.originText.AddFlag('attach'); self.originText.AddFlag('not_pick')
			self.originText.SetText('-')
			self.originText.SetPosition(originX + self._originW//2 - self.originText.GetTextSize()[0]//2, self._bodyY + self._dropH//2 - self.originText.GetTextSize()[1]//2 - 1)
			self.originText.Show()
			
			self.SetSize(full_w, full_h)
		
		def __del__(self):
			if wiki.GetBaseClass():
				wiki.GetBaseClass().objList.pop(long(id(self)))
			
			ui.Window.__del__(self)
		
		def __GenerateMultiLine(self, text, maxWidth):
			currText = self.__GenerateSingleLine()
			textHolder = []
			
			tempText = ui.TextLine()
			tempText.Hide()
			
			splt = text.split(" ")
			currText.SetText(splt[0])
			splt = splt[1:]
			for i in splt:
				tempText.SetText(" " + i)
				if tempText.GetTextSize()[0] + currText.GetTextSize()[0] > maxWidth:
					currText.AdjustSize()
					textHolder.append(currText)
					currText = self.__GenerateSingleLine()
					currText.SetText(i)
				else:
					currText.SetText(currText.GetText() + " " + i)
			
			textHolder.append(currText)
			return textHolder
		
		def __GenerateSingleLine(self):
			text = ui.TextLine()
			text.SetParent(self.base)
			text.Show()
			return text
		
		def NoticeMe(self):
			## checklistname
			
			lst = wiki.GetMobInfo(self.vnum)
			if not lst:
				return
			
			self.originText.SetPosition(getattr(self,'_originX',451) + getattr(self,'_originW',89) / 2 - self.originText.GetTextSize()[0] / 2, getattr(self,'_bodyY',22) + getattr(self,'_dropH',66) / 2 - self.originText.GetTextSize()[1] / 2 - 1)
			self.originText.Show()
			
			sizeLst = []
			orderedLst = []
			
			for i in lst:
				if i[0] < wiki.CONTROL_ITEM_VNUM:
					continue
				
				for j in orderedLst:
					if j[0] == i[0]:
						continue
				
				item.SelectItem(i[0])
				size = item.GetItemSize()[1]
				
				lastPos = 0
				for k in xrange(len(sizeLst)):
					if sizeLst[k] < size:
						break
					
					lastPos += 1
				
				sizeLst.insert(lastPos, size)
				orderedLst.insert(lastPos, i[:])
			
			for i in orderedLst:
				count = (i[1] if i[1] > 1 else 0)
				self.dropList.AddItem(i[0], count)
		
		def OnRender(self):
			if self.additionalLoaded:
				return
			
			if wiki.IsSet(self.vnum, True) or self.parent.loadFrom == self.parent.loadTo:
				self.additionalLoaded = True
				
				self.modelView.SetModel(self.vnum)
				self.modelView.Show()
				
				wiki.LoadInfo(long(id(self)), self.vnum, True)
	
	"""Static values!
		Please dont touch on them."""
	ELEM_PADDING = 5
	SCROLL_SPEED = 50
	ITEM_LOAD_PER_UPDATE = 2
	
	def __init__(self):
		ui.Window.__init__(self)
		
		self.toolTip = uitooltip.ItemToolTip()
		
		self.SetSize(*mainBoardSize)
		
		self.elements = []
		self.scrollBar = None
		
		self.isOpened = False
		self.loadFrom = 0
		self.loadTo = 0
		
		self.mobtypes = 0
		self.fromlvl = 0
		self.tolvl = 0
		
		self.peekWindow = ui.Window()
		self.peekWindow.SetParent(self)
		self.peekWindow.AddFlag("attach")
		self.peekWindow.AddFlag("not_pick")
		# Modern layout compatibility
		self.peekWindow.SetSize(self.GetWidth() - 14, self.GetHeight() - 15)
		self.peekWindow.SetPosition(5, 5)
		self.peekWindow.SetInsideRender(True)
		self.peekWindow.Show()
		
		self.scrollBoard = ui.Window()
		self.scrollBoard.SetParent(self.peekWindow)
		self.scrollBoard.AddFlag("attach")
		self.scrollBoard.AddFlag("not_pick")
		self.scrollBoard.Show()
		
		self.RegisterScrollBar()
	
	def OpenWindow(self):
		wndMgr.Show(self.hWnd)
	
	def Show(self, mobtypes, fromlvl, tolvl):
		wndMgr.Show(self.hWnd)
		
		isChanged = True if not (mobtypes == self.mobtypes and\
								fromlvl == self.fromlvl and tolvl == self.tolvl) else False
		
		self.mobtypes = mobtypes
		self.fromlvl = fromlvl
		self.tolvl = tolvl
		self.loadTo = wiki.LoadClassMobs(mobtypes, fromlvl, tolvl)
		
		if not self.isOpened:
			self.isOpened = True
			self.loadFrom = 0
		
		if self.loadFrom > self.loadTo or isChanged:
			del self.elements[:]
			self.loadFrom = 0
			
			self.scrollBoard.SetSize(0, 0)
			self.UpdateScrollbar()
	
	def Hide(self):
		wndMgr.Hide(self.hWnd)
	
	def OnOverIn(self, vnum, metinSlot = [0 for i in xrange(player.METIN_SOCKET_MAX_NUM)]):
		if not self.toolTip:
			return
		
		self.toolTip.ClearToolTip()
		self.toolTip.AddItemData(vnum, metinSlot, 0, wiki = 1)
	
	def OnOverOut(self):
		if not self.toolTip:
			return
		
		self.toolTip.Hide()
	
	def OnUpdate(self):
		if self.loadFrom < self.loadTo:
			for i in wiki.ChangePage(self.loadFrom, min(self.loadTo, self.loadFrom + self.ITEM_LOAD_PER_UPDATE), True):
				self.AddItem(i)
				self.loadFrom += 1
	
	def AddItem(self, vnum):
		for i in self.elements:
			if vnum == i.vnum:
				return None
		
		tmp = self.WikiItem(vnum, self)
		tmp.SetParent(self.scrollBoard)
		tmp.AddFlag("attach")
		
		addPadding = 0
		totalElem = len(self.elements)
		
		if totalElem > 0:
			lastIndex = 0
			
			for i in xrange(totalElem):
				if self.elements[i].vnum < tmp.vnum:
					break
				
				lastIndex += 1
			
			self.elements.insert(lastIndex, tmp)
			totalElem += 1
			
			for i in xrange(lastIndex, totalElem):
				self.elements[i].SetPosition(0,\
						(0 if i ==0 else self.elements[i - 1].GetLocalPosition()[1] + self.elements[i - 1].GetHeight() + self.ELEM_PADDING))
			
			addPadding = self.ELEM_PADDING
		else:
			self.elements.append(tmp)
		
		tmp.Show()
		
		self.scrollBoard.SetSize(tmp.GetWidth(), self.scrollBoard.GetHeight() + addPadding + tmp.GetHeight())
		self.UpdateScrollbar()
		
		return tmp
	
	def OnRunMouseWheel(self, length):
		if self.peekWindow.IsInPosition():
			self.UpdateScrollbar(int((length * 0.01) * self.SCROLL_SPEED))
			return True
		
		return False
	
	def OnScrollBar(self, fScale):
		curr = min(0, max(math.ceil((self.scrollBoard.GetHeight() - self.peekWindow.GetHeight()) * fScale * -1.0), -self.scrollBoard.GetHeight() + self.peekWindow.GetHeight()))
		self.scrollBoard.SetPosition(0, curr)
	
	def ChangeScrollbar(self):
		if not self.scrollBar:
			return
		
		if self.scrollBoard.GetHeight() <= self.GetHeight():
			self.scrollBar.Hide()
		else:
			self.scrollBar.SetScale(self.GetHeight(), self.scrollBoard.GetHeight())
			self.scrollBar.SetPosScale(truediv(abs(self.scrollBoard.GetLocalPosition()[1]), (self.scrollBoard.GetHeight() - self.peekWindow.GetHeight())))
			self.scrollBar.Show()
	
	def UpdateScrollbar(self, val = 0):
		curr = min(0, max(self.scrollBoard.GetLocalPosition()[1] + val, -self.scrollBoard.GetHeight() + self.peekWindow.GetHeight()))
		self.scrollBoard.SetPosition(0, curr)
		
		self.ChangeScrollbar()
	
	def RegisterScrollBar(self):
		self.scrollBar = WikiScrollBar()
		self.scrollBar.SetParent(self)
		self.scrollBar.SetPosition(self.peekWindow.GetLocalPosition()[0] + self.peekWindow.GetWidth() + 1,\
								self.peekWindow.GetLocalPosition()[1])
		self.scrollBar.SetSize(10, self.peekWindow.GetHeight())
		self.scrollBar.SetScrollEvent(self.OnScrollBar)
		self.scrollBar.SetScrollSpeed(self.SCROLL_SPEED)
		self.scrollBar.Show()
		
		self.ChangeScrollbar()

class WikiMonsterBonusInfoWindow(ui.Window):
	"""Static values!
		Please dont touch on them."""
	ELEM_PADDING = 5
	SCROLL_SPEED = 50
	ITEM_LOAD_PER_UPDATE = 2

	RACE_FLAG_TO_NAME = {
		nonplayer.RACE_FLAG_ANIMAL : localeinfo.WIKI_MONSTERINFO_RACE_ANIMAL,
		nonplayer.RACE_FLAG_UNDEAD : localeinfo.WIKI_MONSTERINFO_RACE_UNDEAD,
		nonplayer.RACE_FLAG_DEVIL : localeinfo.WIKI_MONSTERINFO_RACE_DEVIL,
		nonplayer.RACE_FLAG_HUMAN : localeinfo.WIKI_MONSTERINFO_RACE_HUMAN,
		nonplayer.RACE_FLAG_ORC : localeinfo.WIKI_MONSTERINFO_RACE_ORC,
		nonplayer.RACE_FLAG_MILGYO : localeinfo.WIKI_MONSTERINFO_RACE_MILGYO,
		nonplayer.RACE_FLAG_TREE : localeinfo.WIKI_MONSTERINFO_RACE_TREE,
	}
	SUB_RACE_FLAG_TO_NAME = {
		nonplayer.RACE_FLAG_ATT_ELEC : localeinfo.WIKI_MONSTERINFO_RACE_ELEC,
		nonplayer.RACE_FLAG_ATT_FIRE : localeinfo.WIKI_MONSTERINFO_RACE_FIRE,
		nonplayer.RACE_FLAG_ATT_ICE : localeinfo.WIKI_MONSTERINFO_RACE_ICE,
		nonplayer.RACE_FLAG_ATT_WIND : localeinfo.WIKI_MONSTERINFO_RACE_WIND,
		nonplayer.RACE_FLAG_ATT_EARTH : localeinfo.WIKI_MONSTERINFO_RACE_EARTH,
		nonplayer.RACE_FLAG_ATT_DARK : localeinfo.WIKI_MONSTERINFO_RACE_DARK,
	}
	IMMUNE_FLAG_TO_NAME = {
		nonplayer.IMMUNE_STUN : localeinfo.WIKI_MONSTERINFO_IMMUNE_STUN,
		nonplayer.IMMUNE_SLOW : localeinfo.WIKI_MONSTERINFO_IMMUNE_SLOW,
		nonplayer.IMMUNE_CURSE : localeinfo.WIKI_MONSTERINFO_IMMUNE_CURSE,
		nonplayer.IMMUNE_POISON : localeinfo.WIKI_MONSTERINFO_IMMUNE_POISON,
		nonplayer.IMMUNE_TERROR : localeinfo.WIKI_MONSTERINFO_IMMUNE_TERROR,
	}
	
	def __init__(self, vnum):
		ui.Window.__init__(self)
		
		self.SetSize(*monsterBonusInfoPageSize)
		
		self.vnum = vnum
		
		self.elements = []
		self.scrollBar = None
		
		self.peekWindow = ui.Window()
		self.peekWindow.SetParent(self)
		self.peekWindow.AddFlag("attach")
		self.peekWindow.AddFlag("not_pick")
		self.peekWindow.SetSize(self.GetWidth() - 8 - 6, self.GetHeight() - 6)
		self.peekWindow.SetPosition(3, 3)
		self.peekWindow.SetInsideRender(True)
		self.peekWindow.Show()
		
		self.scrollBoard = ui.Window()
		self.scrollBoard.SetParent(self.peekWindow)
		self.scrollBoard.AddFlag("attach")
		self.scrollBoard.AddFlag("not_pick")
		self.scrollBoard.Show()
		
		#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#
		
		(main_race, sub_race) = self.GetRaceStrings()
		(_dmin, _dmax) = nonplayer.GetMonsterDamage(self.vnum)
		(_gMin, _gMax) = nonplayer.GetMonsterGold(self.vnum)
		
		monster_level = nonplayer.GetMonsterLevel(self.vnum)
		immune_string = self.GetImmuneString()
		damageMin = localeinfo.NumberToMoneyString(_dmin)
		damageMax = localeinfo.NumberToMoneyString(_dmax)
		goldMin = localeinfo.NumberToMoneyString(_gMin)
		goldMax = localeinfo.NumberToMoneyString(_gMax)
		
		d_page_content = [
			((localeinfo.WIKI_MONSTERINFO_LEVEL % monster_level), 0),
			((localeinfo.WIKI_MONSTERINFO_MAINRACE % main_race) + " | " + (localeinfo.WIKI_MONSTERINFO_SUBRACE % sub_race), 0),
			((localeinfo.WIKI_MONSTERINFO_IMMUNE_TO % immune_string), 0),
			((localeinfo.WIKI_MONSTERINFO_DMG_HP % (damageMin, damageMax, localeinfo.NumberToMoneyString(nonplayer.GetMonsterMaxHP(self.vnum)))), 0),
			((localeinfo.WIKI_MONSTERINFO_GOLD_EXP % (goldMin, goldMax, localeinfo.NumberToMoneyString(nonplayer.GetMonsterExp(self.vnum)))), 0),
			(localeinfo.WIKI_MONSTERINFO_RESISTANCES, 0),
			((uitooltip.GET_AFFECT_STRING(item.APPLY_RESIST_SWORD, nonplayer.GetMonsterResistValue(self.vnum, nonplayer.MOB_RESIST_SWORD))), 10),
			((uitooltip.GET_AFFECT_STRING(item.APPLY_RESIST_TWOHAND, nonplayer.GetMonsterResistValue(self.vnum, nonplayer.MOB_RESIST_TWOHAND))), 10),
			((uitooltip.GET_AFFECT_STRING(item.APPLY_RESIST_DAGGER, nonplayer.GetMonsterResistValue(self.vnum, nonplayer.MOB_RESIST_DAGGER))), 10),
			((uitooltip.GET_AFFECT_STRING(item.APPLY_RESIST_BELL, nonplayer.GetMonsterResistValue(self.vnum, nonplayer.MOB_RESIST_BELL))), 10),
			((uitooltip.GET_AFFECT_STRING(item.APPLY_RESIST_FAN, nonplayer.GetMonsterResistValue(self.vnum, nonplayer.MOB_RESIST_FAN))), 10),
			((uitooltip.GET_AFFECT_STRING(item.APPLY_RESIST_BOW, nonplayer.GetMonsterResistValue(self.vnum, nonplayer.MOB_RESIST_BOW))), 10),
		]
		
		for (text, padding) in d_page_content:
			self.AddItem(text, padding)
		
		#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#
		
		self.RegisterScrollBar()
	
	def GetImmuneString(self):
		dwImmuneFlag = nonplayer.GetMonsterImmuneFlag(self.vnum)
		
		immuneflags = ""
		for i in xrange(nonplayer.IMMUNE_FLAG_MAX_NUM):
			curFlag = 1 << i
			
			if HAS_FLAG(dwImmuneFlag, curFlag):
				if self.IMMUNE_FLAG_TO_NAME.has_key(curFlag):
					immuneflags += self.IMMUNE_FLAG_TO_NAME[curFlag] + ", "
		
		immuneflags = (immuneflags[:-2] if len(immuneflags) else localeinfo.WIKI_MONSTERINFO_IMMUNE_NOTHING)
		return immuneflags
	
	def GetRaceStrings(self):
		dwRaceFlag = nonplayer.GetMonsterRaceFlag(self.vnum)
		(mainrace, subrace) = ("", "")
		
		for i in xrange(nonplayer.RACE_FLAG_MAX_NUM):
			curFlag = 1 << i
			
			if HAS_FLAG(dwRaceFlag, curFlag):
				if self.RACE_FLAG_TO_NAME.has_key(curFlag):
					mainrace += self.RACE_FLAG_TO_NAME[curFlag] + ", "
				elif self.SUB_RACE_FLAG_TO_NAME.has_key(curFlag):
					subrace += self.SUB_RACE_FLAG_TO_NAME[curFlag] + ", "
		
		if nonplayer.IsMonsterStone(self.vnum):
			mainrace += localeinfo.WIKI_MONSTERINFO_RACE_METIN + ", "
		
		mainrace = (mainrace[:-2] if len(mainrace) else localeinfo.WIKI_MONSTERINFO_NO_RACE)
		subrace = (subrace[:-2] if len(subrace) else localeinfo.WIKI_MONSTERINFO_NO_RACE)
		
		return (mainrace, subrace)
	
	def AddItem(self, text, padding = 0):
		tmp = ui.Window()
		tmp.SetParent(self.scrollBoard)
		tmp.AddFlag("attach")
		tmp.SetSize(self.GetWidth() - 8, 15)
		
		tmp.img = ui.ExpandedImageBox()
		tmp.img.SetParent(tmp)
		tmp.img.AddFlag("attach")
		tmp.img.AddFlag("not_pick")
		tmp.img.LoadImage("d:/ymir work/ui/wiki/arrow_2.tga")
		tmp.img.SetPosition(padding, tmp.GetHeight() / 2 - tmp.img.GetHeight() / 2)
		tmp.img.Show()
		
		tmp.txt = ui.TextLine()
		tmp.txt.SetParent(tmp)
		tmp.txt.AddFlag("attach")
		tmp.txt.AddFlag("not_pick")
		tmp.txt.SetText(text)
		tmp.txt.SetPosition(tmp.img.GetLocalPosition()[0] + tmp.img.GetWidth() + 5,\
							tmp.GetHeight() / 2 - tmp.txt.GetTextSize()[1] / 2 - 1)
		tmp.txt.Show()
		
		addPadding = 0
		totalElem = len(self.elements)
		
		if totalElem > 0:
			lastIndex = totalElem
			
			self.elements.insert(lastIndex, tmp)
			totalElem += 1
			
			for i in xrange(lastIndex, totalElem):
				self.elements[i].SetPosition(0,\
					(0 if i == 0 else self.elements[i - 1].GetLocalPosition()[1] + self.elements[i - 1].GetHeight() + self.ELEM_PADDING))
			
			addPadding = self.ELEM_PADDING
		else:
			self.elements.append(tmp)
		
		tmp.Show()
		
		self.scrollBoard.SetSize(tmp.GetWidth(), self.scrollBoard.GetHeight() + addPadding + tmp.GetHeight())
		self.UpdateScrollbar()
		
		return tmp
	
	def OnRunMouseWheel(self, length):
		if self.peekWindow.IsInPosition():
			self.UpdateScrollbar(int((length * 0.01) * self.SCROLL_SPEED))
			return True
		
		return False
	
	def OnScrollBar(self, fScale):
		curr = min(0, max(math.ceil((self.scrollBoard.GetHeight() - self.peekWindow.GetHeight()) * fScale * -1.0), -self.scrollBoard.GetHeight() + self.peekWindow.GetHeight()))
		self.scrollBoard.SetPosition(0, curr)
	
	def ChangeScrollbar(self):
		if not self.scrollBar:
			return
		
		if self.scrollBoard.GetHeight() <= self.GetHeight():
			self.scrollBar.Hide()
		else:
			self.scrollBar.SetScale(self.GetHeight(), self.scrollBoard.GetHeight())
			self.scrollBar.SetPosScale(truediv(abs(self.scrollBoard.GetLocalPosition()[1]), (self.scrollBoard.GetHeight() - self.peekWindow.GetHeight())))
			self.scrollBar.Show()
	
	def UpdateScrollbar(self, val = 0):
		curr = min(0, max(self.scrollBoard.GetLocalPosition()[1] + val, -self.scrollBoard.GetHeight() + self.peekWindow.GetHeight()))
		self.scrollBoard.SetPosition(0, curr)
		
		self.ChangeScrollbar()
	
	def RegisterScrollBar(self):
		self.scrollBar = WikiScrollBar()
		self.scrollBar.SetParent(self)
		self.scrollBar.SetPosition(self.peekWindow.GetLocalPosition()[0] + self.peekWindow.GetWidth() + 1, self.peekWindow.GetLocalPosition()[1])
		self.scrollBar.SetSize(10, self.peekWindow.GetHeight())
		self.scrollBar.SetScrollEvent(self.OnScrollBar)
		self.scrollBar.SetScrollSpeed(self.SCROLL_SPEED)
		self.scrollBar.Show()
		
		self.ChangeScrollbar()

class WikiItemOriginInfo(ui.Window):
	"""Static values!
		Please dont touch on them."""
	ELEM_PADDING = 5
	SCROLL_SPEED = 50
	ITEM_LOAD_PER_UPDATE = 2
	
	def __init__(self, vnum):
		ui.Window.__init__(self)
		
		self.SetSize(*itemOriginPageSize)
		
		self.vnum = vnum
		
		self.elements = []
		self.mainDataImage = None
		self.scrollBar = None
		
		self.peekWindow = ui.Window()
		self.peekWindow.SetParent(self)
		self.peekWindow.AddFlag("attach")
		self.peekWindow.AddFlag("not_pick")
		self.peekWindow.SetSize(self.GetWidth() - 8 - 6, self.GetHeight() - 6)
		self.peekWindow.SetPosition(3, 3)
		self.peekWindow.SetInsideRender(True)
		self.peekWindow.Show()
		
		self.scrollBoard = ui.Window()
		self.scrollBoard.SetParent(self.peekWindow)
		self.scrollBoard.AddFlag("attach")
		self.scrollBoard.AddFlag("not_pick")
		self.scrollBoard.Show()
		
		self.RegisterScrollBar()
	
	def ParseTextlines(self):
		lst = wiki.GetOriginInfo(self.vnum)
		if not lst:
			return
		
		alreadyParsed = []
		
		for (vnum, isMonster) in lst:
			bAlready = False
			
			for i in alreadyParsed:
				if i[0] == vnum and i[1] == isMonster:
					bAlready = True
					break
			
			if bAlready or not vnum:
				continue
			
			if isMonster:
				currName = nonplayer.GetMonsterName(vnum)
			else:
				item.SelectItem(vnum)
				currName = item.GetItemName()
			
			alreadyParsed.append([vnum, isMonster])
			self.AddItem(currName)
	
	def AddItem(self, text, padding = 0):
		tmp = ui.Window()
		tmp.SetParent(self.scrollBoard)
		tmp.AddFlag("attach")
		tmp.SetSize(self.GetWidth() - 8, 15)
		
		tmp.img = ui.ExpandedImageBox()
		tmp.img.SetParent(tmp)
		tmp.img.AddFlag("attach")
		tmp.img.AddFlag("not_pick")
		tmp.img.LoadImage("d:/ymir work/ui/wiki/arrow_2.tga")
		tmp.img.SetPosition(padding, tmp.GetHeight() / 2 - tmp.img.GetHeight() / 2)
		tmp.img.Show()
		
		tmp.txt = ui.TextLine()
		tmp.txt.SetParent(tmp)
		tmp.txt.AddFlag("attach")
		tmp.txt.AddFlag("not_pick")
		tmp.txt.SetText(text)
		tmp.txt.SetPosition(tmp.img.GetLocalPosition()[0] + tmp.img.GetWidth() + 5,
								tmp.GetHeight() / 2 - tmp.txt.GetTextSize()[1] / 2 - 1)
		tmp.txt.Show()
		
		addPadding = 0
		totalElem = len(self.elements)
		
		if totalElem > 0:
			lastIndex = totalElem
			
			self.elements.insert(lastIndex, tmp)
			totalElem += 1
			
			for i in xrange(lastIndex, totalElem):
				self.elements[i].SetPosition(0,
						(0 if i == 0 else self.elements[i - 1].GetLocalPosition()[1] + self.elements[i - 1].GetHeight() + self.ELEM_PADDING))
			
			addPadding = self.ELEM_PADDING
		else:
			self.elements.append(tmp)
		
		tmp.Show()
		
		self.scrollBoard.SetSize(tmp.GetWidth(), self.scrollBoard.GetHeight() + addPadding + tmp.GetHeight())
		self.UpdateScrollbar()
		
		return tmp
	
	def OnRunMouseWheel(self, length):
		if self.peekWindow.IsInPosition():
			self.UpdateScrollbar(int((length * 0.01) * self.SCROLL_SPEED))
			return True
		
		return False
	
	def OnScrollBar(self, fScale):
		curr = min(0, max(math.ceil((self.scrollBoard.GetHeight() - self.peekWindow.GetHeight()) * fScale * -1.0), -self.scrollBoard.GetHeight() + self.peekWindow.GetHeight()))
		self.scrollBoard.SetPosition(0, curr)
	
	def ChangeScrollbar(self):
		if not self.scrollBar:
			return
		
		if self.scrollBoard.GetHeight() <= self.GetHeight():
			self.scrollBar.Hide()
		else:
			self.scrollBar.SetScale(self.GetHeight(), self.scrollBoard.GetHeight())
			self.scrollBar.SetPosScale(truediv(abs(self.scrollBoard.GetLocalPosition()[1]), (self.scrollBoard.GetHeight() - self.peekWindow.GetHeight())))
			self.scrollBar.Show()
	
	def UpdateScrollbar(self, val = 0):
		curr = min(0, max(self.scrollBoard.GetLocalPosition()[1] + val, -self.scrollBoard.GetHeight() + self.peekWindow.GetHeight()))
		self.scrollBoard.SetPosition(0, curr)
		
		self.ChangeScrollbar()
	
	def RegisterScrollBar(self):
		self.scrollBar = WikiScrollBar()
		self.scrollBar.SetParent(self)
		self.scrollBar.SetPosition(self.peekWindow.GetLocalPosition()[0] + self.peekWindow.GetWidth() + 1, self.peekWindow.GetLocalPosition()[1])
		self.scrollBar.SetSize(10, self.peekWindow.GetHeight())
		self.scrollBar.SetScrollEvent(self.OnScrollBar)
		self.scrollBar.SetScrollSpeed(self.SCROLL_SPEED)
		self.scrollBar.Show()
		
		self.ChangeScrollbar()

class SpecialPageWindow(ui.Window):
	"""Static values!
		Please dont touch on them."""
	SCROLL_SPEED = 50
	
	def __init__(self, vnum, isMonster):
		ui.Window.__init__(self)
		
		self.toolTip = uitooltip.ItemToolTip()
		
		wikiBase = wiki.GetBaseClass()
		if id(self) not in wikiBase.objList:
			wikiBase.objList[long(id(self))] = proxy(self)
		
		self.vnum = vnum
		self.isMonster = isMonster
		self.bonusScrollBoard = None
		
		self.bg = ui.ExpandedImageBox()
		self.bg.SetParent(self)
		self.bg.AddFlag("attach")
		self.bg.AddFlag("not_pick")
		self.bg.LoadImage("d:/ymir work/ui/wiki/detail_monster.tga")
		self.bg.Show()
		
		self.SetSize(self.bg.GetWidth(), self.bg.GetHeight())
		self.SetInsideRender(True)
		
		self.subTitleText1 = ui.TextLine()
		self.subTitleText1.SetParent(self.bg)
		self.subTitleText1.AddFlag("attach")
		self.subTitleText1.AddFlag("not_pick")
		self.subTitleText1.Show()
		
		self.subTitleText2 = ui.TextLine()
		self.subTitleText2.SetParent(self.bg)
		self.subTitleText2.AddFlag("attach")
		self.subTitleText2.AddFlag("not_pick")
		self.subTitleText2.Show()
		
		if isMonster:
			self.subTitleText1.SetText(localeinfo.WIKI_MONSTERINFO_DROPLISTOF % nonplayer.GetMonsterName(self.vnum))
			self.subTitleText2.SetText(localeinfo.WIKI_MONSTERINFO_STATISTICSOF % nonplayer.GetMonsterName(self.vnum))
			
			self.modelView = WikiRenderTarget(163, 163)
			self.modelView.SetParent(self.bg)
			self.modelView.AddFlag("attach")
			self.modelView.SetPosition(1 + 187 / 2 - 163 / 2, 1)
			self.modelView.SetModel(vnum)
			self.modelView.Show()
			
			self.itemContainer = ChestPeekWindow(self, itemOriginPageSize[0], itemOriginPageSize[1], False)
			self.itemContainer.ELEM_PER_LINE = 10
			self.itemContainer.ELEM_X_PADDING = -2
			self.itemContainer.AddFlag("attach")
			self.itemContainer.SetParent(self.bg)
			self.itemContainer.SetPosition(189, 22)
			self.itemContainer.mOverInEvent = ui.__mem_func__(self.OnOverIn)
			self.itemContainer.mOverOutEvent = ui.__mem_func__(self.OnOverOut)
			self.itemContainer.Show()
			
			self.LoadDropData()
			
			self.bonusInfo = WikiMonsterBonusInfoWindow(self.vnum)
			self.bonusInfo.SetParent(self.bg)
			self.bonusInfo.AddFlag("attach")
			self.bonusInfo.SetPosition(1, 188)
			self.bonusInfo.Show()
		else:
			item.SelectItem(self.vnum)
			
			# mount preview (search)
			isMountItem = False
			mountModelVnum = 0
			try:
				itemType = item.GetItemType()
				subType = item.GetItemSubType()
				
				if itemType == item.ITEM_TYPE_COSTUME:
					if (hasattr(app, "ENABLE_MOUNT_COSTUME_SYSTEM") and app.ENABLE_MOUNT_COSTUME_SYSTEM and subType == 2):
						mountModelVnum = item.GetValue(1)
					elif (hasattr(app, "ENABLE_COSTUME_MOUNT") and app.ENABLE_COSTUME_MOUNT and subType == 7):
						mountModelVnum = item.GetValue(0)
					elif subType in (2, 7):
						mountModelVnum = item.GetValue(1) if item.GetValue(1) else item.GetValue(0)
				
				if not mountModelVnum:
					if hasattr(ingamewikiconfig, "COSTUME_MOUNT_VNUMS") and self.vnum in ingamewikiconfig.COSTUME_MOUNT_VNUMS:
						mountModelVnum = item.GetValue(1) if item.GetValue(1) else item.GetValue(0)
					elif hasattr(ingamewikiconfig, "COSTUME_MOUNT_VNUMS_COSTUMES") and self.vnum in ingamewikiconfig.COSTUME_MOUNT_VNUMS_COSTUMES:
						mountModelVnum = item.GetValue(1) if item.GetValue(1) else item.GetValue(0)
				
				isMountItem = (mountModelVnum != 0)
			except:
				isMountItem = False
				mountModelVnum = 0
			
			# csak NEM mount itemeknel normalizaljuk a refine vnumot
			if not isMountItem and wiki.CanIncrRefineLevel():
				startRefineVnum = wiki.GetWikiItemStartRefineVnum(self.vnum)
				self.vnum = startRefineVnum if startRefineVnum != 0 else (int(self.vnum / 10) * 10)

			# Origin info-hoz kell az item wiki adat. Ha még nincs betöltve, kérjük le.
			if not wiki.IsSet(self.vnum):
				wiki.LoadInfo(long(id(self)), self.vnum, False)
			

			self.subTitleText1.SetText(localeinfo.WIKI_ITEMINFO_OPTAINEDFROM)
			baseName = StripRefineSuffix(item.GetItemName())
			
			# mount keresésnél NE refine infót írjon
			mountPreviewH = 0
			if isMountItem:
				self.subTitleText2.SetText(baseName)
			else:
				self.subTitleText2.SetText(localeinfo.WIKI_ITEMINFO_REFINEINFO % baseName)
			
			if isMountItem:
				# Nagy mount preview (kb 5x terület) a felső részben
				bgw = self.bg.GetWidth()
				# A felső blokk magassága 187, alatta jönnek az adatok; maradjunk ez alatt
				rtW = max(163, int(bgw) - 20)
				bgh = self.bg.GetHeight()
				desiredH = 184 * 2
				# hagyjunk helyet a 'honnan szerezhető' + bónusz blokk számára
				panelMinH = 40
				maxH = max(184, bgh - panelMinH - 12)
				rtH = min(desiredH, maxH)
				mountPreviewH = rtH
				self.modelView = WikiRenderTarget(rtW, rtH)
				self.modelView.SetParent(self.bg)
				self.modelView.AddFlag("attach")
				self.modelView.SetPosition(int(bgw / 2 - rtW / 2), 1)
				self.modelView.SetModel(mountModelVnum)
				# Mount itemnél tooltiphoz az eredeti item vnum kell (ne refine offset)
				self.modelView.SetStringEvent("MOUSE_OVER_IN", ui.__mem_func__(self.OnOverIn), self.vnum)
				self.modelView.SetStringEvent("MOUSE_OVER_OUT", ui.__mem_func__(self.OnOverOut))
				self.modelView.Show()
			else:
				self.modelView = ui.ExpandedImageBox()
				self.modelView.SetParent(self.bg)
				self.modelView.AddFlag("attach")
				self.modelView.LoadImage(item.GetIconImageFileName())
				self.modelView.SetPosition(1 + 187 / 2 - self.modelView.GetWidth() / 2, 1 + 163 / 2 - self.modelView.GetHeight() / 2)
				self.modelView.SetStringEvent("MOUSE_OVER_IN", ui.__mem_func__(self.OnOverIn), self.vnum + (wiki.MAX_REFINE_COUNT if wiki.CanIncrRefineLevel() else 0))
				self.modelView.SetStringEvent("MOUSE_OVER_OUT", ui.__mem_func__(self.OnOverOut))
				self.modelView.Show()
			
			# --- Origin info (Honnan szerezhető) ---
			self._mountItemMode = bool(isMountItem and mountPreviewH)
			
			self.originInfo = WikiItemOriginInfo(self.vnum)
			self.originInfo.AddFlag("attach")
			
			if self._mountItemMode:
				bgw = self.bg.GetWidth()
				bgh = self.bg.GetHeight()

				# Panel a render target alatt: honnan szerezhető + bónuszok (görgethető)
				panelY = mountPreviewH + 6
				panelH = max(34, bgh - panelY - 6)

				self.bonusPeekWindow = ui.Window()
				self.bonusPeekWindow.SetParent(self.bg)
				self.bonusPeekWindow.AddFlag("attach")
				self.bonusPeekWindow.AddFlag("not_pick")
				self.bonusPeekWindow.SetPosition(0, panelY)
				self.bonusPeekWindow.SetSize(bgw, panelH)
				self.bonusPeekWindow.SetInsideRender(True)
				self.bonusPeekWindow.Show()

				self.bonusScrollBoard = ui.Window()
				self.bonusScrollBoard.SetParent(self.bonusPeekWindow)
				self.bonusScrollBoard.AddFlag("attach")
				self.bonusScrollBoard.AddFlag("not_pick")
				self.bonusScrollBoard.SetPosition(0, 0)
				self.bonusScrollBoard.SetSize(bgw, panelH)
				self.bonusScrollBoard.Show()

				self.originInfo.SetParent(self.bonusScrollBoard)
				self.originInfo.Show()

				# Mount item ikon (tooltip hover) – a render alatt, bal oldalon
				# (a render targeten is van hover, de az ikon sokkal kenyelmesebb)
				try:
					item.SelectItem(self.vnum)
					iconPath = item.GetIconImageFileName()
				except:
					iconPath = ""

				self.mountIcon = ui.ExpandedImageBox()
				self.mountIcon.SetParent(self.bonusScrollBoard)
				self.mountIcon.AddFlag("attach")
				if iconPath:
					self.mountIcon.LoadImage(iconPath)
				# Bal felso sarokban a scroll panelen belul
				self.mountIcon.SetPosition(10, 2)
				self.mountIcon.SetStringEvent("MOUSE_OVER_IN", ui.__mem_func__(self.OnOverIn), self.vnum)
				self.mountIcon.SetStringEvent("MOUSE_OVER_OUT", ui.__mem_func__(self.OnOverOut))
				self.mountIcon.Show()

				self.bonusInfo = self.__BuildMountBonusInfo(bgw)
				self.bonusInfo.SetParent(self.bonusScrollBoard)
				self.bonusInfo.Show()

				self.additionalLoaded = False
				self.__UpdateMountInfoLayout()
			else:
				self.originInfo.SetParent(self.bg)
				self.originInfo.SetPosition(189, 22)
				self.originInfo.Show()

				# --- Details area (refine infó) ---
				self.bonusPeekWindow = ui.Window()
				self.bonusPeekWindow.SetParent(self.bg)
				self.bonusPeekWindow.AddFlag("attach")
				self.bonusPeekWindow.AddFlag("not_pick")
				self.bonusPeekWindow.SetPosition(0, 187)
				self.bonusPeekWindow.SetInsideRender(True)
				self.bonusPeekWindow.Show()

				self.bonusScrollBoard = ui.Window()
				self.bonusScrollBoard.SetParent(self.bonusPeekWindow)
				self.bonusScrollBoard.AddFlag("attach")
				self.bonusScrollBoard.AddFlag("not_pick")
				self.bonusScrollBoard.Show()

				# Ez a blokk írja ki a tárgy refine infóját
				self.bonusInfo = WikiMainWeaponWindow.WikiItem(self.vnum, self, False)
				self.bonusInfo.SetParent(self.bonusScrollBoard)
				self.bonusInfo.Show()

				self.additionalLoaded = False

				self.bonusPeekWindow.SetSize(self.bonusInfo.GetWidth(), self.bonusInfo.GetHeight())
				self.bonusScrollBoard.SetSize(self.bonusInfo.GetWidth(), self.bonusInfo.GetHeight())
		bgw = self.bg.GetWidth()
		
		# jobb oldali fejléc kozepen (a jobb panel kezdete 189)
		right_x = 189
		right_w = bgw - right_x - 2
		self.subTitleText1.SetPosition(
			right_x + int(right_w / 2) - int(self.subTitleText1.GetTextSize()[0] / 2),
			1 + 10 - int(self.subTitleText1.GetTextSize()[1] / 2)
		)
		
		# kozepso elvalaszto (refine info) teljes szelessegben kozepen
		self.subTitleText2.SetPosition(
			1 + int(bgw / 2) - int(self.subTitleText2.GetTextSize()[0] / 2),
			165 + 11 - int(self.subTitleText2.GetTextSize()[1] / 2)
		)
	
	def __BuildMountBonusInfo(self, width):
		"""Mount oldal: bónuszok szöveges listája (refine tábla helyett)."""
		wnd = ui.Window()
		wnd.AddFlag("attach")
		wnd.AddFlag("not_pick")

		y = 0

		title = ui.TextLine()
		title.SetParent(wnd)
		title.SetPosition(10, y)
		title.SetText("Bónuszok:")
		title.Show()
		y += 16

		# Próbáljunk normális formázást (tooltip / localeinfo), különben fallback.
		def _fmt_affect(aType, aVal):
			try:
				return uitooltip.GetAffectString(aType, aVal)
			except:
				try:
					return localeinfo.GetAffectString(aType, aVal)
				except:
					sign = "+" if aVal >= 0 else ""
					return "Affect %d %s%d" % (aType, sign, aVal)

		lines = []

		# Szint követelmény
		try:
			item.SelectItem(self.vnum)
			for i in xrange(getattr(item, 'LIMIT_MAX_NUM', 2)):
				(lType, lVal) = item.GetLimit(i)
				if hasattr(item, 'LIMIT_LEVEL') and lType == item.LIMIT_LEVEL and lVal:
					lines.append("Szint követelmény: %d" % lVal)
					break
		except:
			pass

		# Bónuszok (Apply/Affect)
		try:
			maxApply = getattr(item, 'ITEM_APPLY_MAX_NUM', 3)
			for i in xrange(maxApply):
				try:
					(aType, aVal) = item.GetAffect(i)
				except:
					try:
						(aType, aVal) = item.GetApply(i)
					except:
						continue

				if not aType or not aVal:
					continue
				lines.append(_fmt_affect(aType, aVal))
		except:
			pass

		if not lines:
			lines = ["Nincs bónusz."]

		for t in lines:
			line = ui.TextLine()
			line.SetParent(wnd)
			line.SetPosition(20, y)
			line.SetText(t)
			line.Show()
			y += 16

		wnd.SetSize(width, y + 4)
		return wnd

	def __UpdateMountInfoLayout(self):
		"""Mount oldal: origin + bónusz block középre igazítása, scroll méret frissítése."""
		if not getattr(self, '_mountItemMode', False):
			return

		if not self.bonusPeekWindow or not self.bonusScrollBoard:
			return

		bgw = self.bonusPeekWindow.GetWidth()

		# Origin középre
		try:
			ox = int(bgw / 2 - self.originInfo.GetWidth() / 2)
		except:
			ox = 0
		try:
			self.originInfo.SetPosition(ox, 0)
		except:
			pass

		try:
			oh = self.originInfo.GetHeight()
		except:
			oh = 0

		by = oh + 6

		# Bónusz lista középre
		try:
			bx = int(bgw / 2 - self.bonusInfo.GetWidth() / 2)
		except:
			bx = 0
		try:
			self.bonusInfo.SetPosition(bx, by)
		except:
			pass

		contentH = by
		try:
			contentH += self.bonusInfo.GetHeight()
		except:
			pass

		contentH = max(contentH, self.bonusPeekWindow.GetHeight())
		try:
			self.bonusScrollBoard.SetSize(bgw, contentH)
		except:
			pass

		# Clamp pozíció
		try:
			self.UpdateScrollbar(0)
		except:
			pass
	def __del__(self):
		if wiki.GetBaseClass():
			wiki.GetBaseClass().objList.pop(long(id(self)))
		
		ui.Window.__del__(self)
	
	def OnRunMouseWheel(self, length):
		if not self.bonusScrollBoard:
			return False
		
		if self.bonusScrollBoard.GetHeight() == self.bonusPeekWindow.GetHeight():
			return False
		
		if self.bonusPeekWindow.IsInPosition():
			self.UpdateScrollbar(int((length * 0.01) * self.SCROLL_SPEED))
			return True
		
		return False
	
	def ChangeElementSize(self, elem, sizeDiff):
		if self.bonusScrollBoard:
			self.bonusScrollBoard.SetSize(self.bonusScrollBoard.GetWidth(), self.bonusScrollBoard.GetHeight() + sizeDiff)
	
	def UpdateScrollbar(self, val = 0):
		curr = min(0, max(self.bonusScrollBoard.GetLocalPosition()[1] + val, -self.bonusScrollBoard.GetHeight() + self.bonusPeekWindow.GetHeight()))
		if self.bonusScrollBoard:
			self.bonusScrollBoard.SetPosition(0, curr)
	
	def OnOverIn(self, vnum, metinSlot = [0 for i in xrange(player.METIN_SOCKET_MAX_NUM)]):
		if not self.toolTip:
			return
		
		self.toolTip.ClearToolTip()
		self.toolTip.AddItemData(vnum, metinSlot, 0, wiki = 1)
	
	def OnOverOut(self):
		if not self.toolTip:
			return
		
		self.toolTip.Hide()
	
	def OnRender(self):
		if not self.isMonster and not self.additionalLoaded and wiki.IsSet(self.vnum):
			self.additionalLoaded = True
			self.originInfo.ParseTextlines()
			if getattr(self, '_mountItemMode', False):
				self.__UpdateMountInfoLayout()
	
	def OpenWindow(self):
		wndMgr.Show(self.hWnd)
	
	def NoticeMe(self):
		self.LoadDropData()
	
	def LoadDropData(self):
		if not self.isMonster:
			return
		
		if not wiki.IsSet(self.vnum, True):
			wiki.LoadInfo(long(id(self)), self.vnum, True)
			return
		
		lst = wiki.GetMobInfo(self.vnum)
		if not lst:
			return
		
		sizeLst = []
		orderedLst = []
		
		for i in lst:
			if i[0] < wiki.CONTROL_ITEM_VNUM:
				continue
			
			item.SelectItem(i[0])
			size = item.GetItemSize()[1]
			
			lastPos = 0
			for k in xrange(len(sizeLst)):
				if sizeLst[k] < size:
					break
				
				lastPos += 1
			
			sizeLst.insert(lastPos, size)
			orderedLst.insert(lastPos, i[:])
		
		for i in orderedLst:
			count = (i[1] if i[1] > 1 else 0)
			self.itemContainer.AddItem(i[0], count)
	
	def Show(self):
		wndMgr.Show(self.hWnd)
	
	def Hide(self):
		wndMgr.Hide(self.hWnd)

class SimpleTextLoader(ui.Window):
	"""Static values!
		Please dont touch on them."""
	ELEM_PADDING = 0
	SCROLL_SPEED = 50
	
	def __init__(self):
		ui.Window.__init__(self)
		
		self.SetSize(*mainBoardSize)
		
		self.elements = []
		self.images = []
		self.scrollBar = None
		
		self.peekWindow = ui.Window()
		self.peekWindow.SetParent(self)
		self.peekWindow.AddFlag("attach")
		self.peekWindow.AddFlag("not_pick")
		self.peekWindow.SetSize(self.GetWidth() - 8 - 5, self.GetHeight() - 5)
		self.peekWindow.SetPosition(5, 5)
		self.peekWindow.SetInsideRender(True)
		self.peekWindow.Show()
		
		self.scrollBoard = ui.Window()
		self.scrollBoard.SetParent(self.peekWindow)
		self.scrollBoard.AddFlag("attach")
		self.scrollBoard.AddFlag("not_pick")
		self.scrollBoard.Show()
		
		self.RegisterScrollBar()
	
	def ParseToken(self, data):
		data = data.replace(chr(10), "").replace(chr(13), "")
		if not (len(data) and data[0] == "["):
			return (False, {}, data)
		
		fnd = data.find("]")
		if fnd <= 0:
			return (False, {}, data)
		
		content = data[1:fnd]
		data = data[fnd+1:]
		
		content = content.split(";")
		container = {}
		
		for i in content:
			i = i.strip()
			splt = i.split("=")
			
			if len(splt) == 1:
				container[splt[0].lower().strip()] = True
			else:
				container[splt[0].lower().strip()] = splt[1].lower().strip()
		
		return (True, container, data)
	
	def GetColorFromString(self, strCol):
		retData = []
		dNum = 4
		hCol = long(strCol, 16)
		
		if hCol <= 0xFFFFFF:
			retData.append(1.0)
			dNum = 3
		
		for i in xrange(dNum):
			retData.append(float((hCol >> (8 * i)) & 0xFF) / 255.0)
		
		retData.reverse()
		return retData
	
	def LoadFile(self, filename):
		del self.elements[:]
		del self.images[:]
		
		self.scrollBoard.SetSize(0, 0)
		self.UpdateScrollbar()
		
		open_filename = "{}/wiki/{}".format(app.GetLocalePath(), filename)
		loadF = new_open(open_filename, 'r')
		
		for i in loadF.readlines()[1:]:
			(ret, tokenMap, i) = self.ParseToken(i)
			
			if ret:
				if tokenMap.has_key("banner_img"):
					# banner disabled
					try:
						bc = wiki.GetBaseClass()
						if bc and hasattr(bc, 'header'):
							bc.header.Hide()
					except:
						pass
					tokenMap.pop("banner_img")
				
				if tokenMap.has_key("img"):
					cimg = ui.ExpandedImageBox()
					cimg.SetParent(self.scrollBoard)
					cimg.AddFlag("attach")
					cimg.AddFlag("not_pick")
					cimg.LoadImage(tokenMap["img"])
					cimg.Show()
					
					tokenMap.pop("img")
					
					x = 0
					if tokenMap.has_key("x"):
						x = int(tokenMap["x"])
						tokenMap.pop("x")
					
					y = 0
					if tokenMap.has_key("y"):
						y = int(tokenMap["y"])
						tokenMap.pop("y")
					
					if tokenMap.has_key("center_align"):
						cimg.SetPosition(self.peekWindow.GetWidth() / 2 - cimg.GetWidth() / 2, y)
						tokenMap.pop("center_align")
					elif tokenMap.has_key("right_align"):
						cimg.SetPosition(self.peekWindow.GetWidth() - cimg.GetWidth(), y)
						tokenMap.pop("right_align")
					else:
						cimg.SetPosition(x, y)
					
					self.images.append(cimg)
			
			if ret and not len(i):
				continue
			
			tmp = ui.Window()
			tmp.SetParent(self.scrollBoard)
			tmp.AddFlag("attach")
			tmp.AddFlag("not_pick")
			
			tmp.txt = ui.TextLine()
			tmp.txt.SetParent(tmp)
			if tokenMap.has_key("font_size"):
				splt = localeinfo.UI_DEF_FONT.split(":")
				tmp.txt.SetFontName(splt[0]+":"+tokenMap["font_size"])
				tokenMap.pop("font_size")
			else:
				tmp.txt.SetFontName(localeinfo.UI_DEF_FONT)
			tmp.txt.SetText(i)
			tmp.txt.Show()
			tmp.SetSize(*tmp.txt.GetTextSize())
			
			if len(i) > 0 and i[0] == "*":
				tmp.txt.SetText(i[1:])
				
				tmp.img = ui.ExpandedImageBox()
				tmp.img.SetParent(tmp)
				tmp.img.AddFlag("attach")
				tmp.img.AddFlag("not_pick")
				tmp.img.LoadImage("d:/ymir work/ui/wiki/arrow_2.tga")
				tmp.img.Show()
				
				tmp.SetSize(tmp.img.GetWidth() + 5 + tmp.txt.GetTextSize()[0], max(tmp.img.GetHeight(), tmp.txt.GetTextSize()[1]))
				tmp.img.SetPosition(0, abs(tmp.GetHeight() / 2 - tmp.img.GetHeight() / 2))
				
				tmp.txt.SetPosition(tmp.img.GetWidth() + 5, abs(tmp.GetHeight() / 2 - tmp.txt.GetTextSize()[1] / 2) - 1)
			
			if tokenMap.has_key("color"):
				fontColor = self.GetColorFromString(tokenMap["color"])
				tmp.txt.SetPackedFontColor(grp.GenerateColor(fontColor[0], fontColor[1], fontColor[2], fontColor[3]))
				
				tokenMap.pop("color")
			
			addPadding = 0
			totalElem = len(self.elements)
			
			if tokenMap.has_key("y_padding"):
				addPadding = int(tokenMap["y_padding"])
				tokenMap.pop("y_padding")
			
			if totalElem > 0:
				lastIndex = totalElem
				
				self.elements.insert(lastIndex, tmp)
				totalElem += 1
				
				for i in xrange(lastIndex, totalElem):
					self.elements[i].SetPosition(0,\
						(0 if i ==0 else self.elements[i - 1].GetLocalPosition()[1] + self.elements[i - 1].GetHeight() + addPadding))
			else:
				self.elements.append(tmp)
				tmp.SetPosition(0, addPadding)
			
			if tokenMap.has_key("center_align"):
				tmp.SetPosition(self.peekWindow.GetWidth() / 2 - tmp.GetWidth() / 2, tmp.GetLocalPosition()[1])
				tokenMap.pop("center_align")
			elif tokenMap.has_key("right_align"):
				tmp.SetPosition(self.peekWindow.GetWidth() - tmp.GetWidth(), tmp.GetLocalPosition()[1])
				tokenMap.pop("right_align")
			elif tokenMap.has_key("x_padding"):
				tmp.SetPosition(int(tokenMap["x_padding"]), tmp.GetLocalPosition()[1])
				tokenMap.pop("x_padding")
			
			tmp.Show()
			
			self.scrollBoard.SetSize(self.peekWindow.GetWidth(), self.scrollBoard.GetHeight() + addPadding + tmp.GetHeight())
		
		for i in self.images:
			mxSize = i.GetLocalPosition()[1] + i.GetHeight()
			if mxSize > self.scrollBoard.GetHeight():
				self.scrollBoard.SetSize(self.peekWindow.GetWidth(), mxSize)
		
		self.UpdateScrollbar()
		wndMgr.Show(self.hWnd)
	
	def OnRunMouseWheel(self, length):
		if self.peekWindow.IsInPosition():
			self.UpdateScrollbar(int((length * 0.01) * self.SCROLL_SPEED))
			return True
		
		return False
	
	def OnScrollBar(self, fScale):
		curr = min(0, max(math.ceil((self.scrollBoard.GetHeight() - self.peekWindow.GetHeight()) * fScale * -1.0), -self.scrollBoard.GetHeight() + self.peekWindow.GetHeight()))
		self.scrollBoard.SetPosition(0, curr)
	
	def ChangeScrollbar(self):
		if not self.scrollBar:
			return
		
		if self.scrollBoard.GetHeight() <= self.GetHeight():
			self.scrollBar.Hide()
		else:
			self.scrollBar.SetScale(self.GetHeight(), self.scrollBoard.GetHeight())
			self.scrollBar.SetPosScale(truediv(abs(self.scrollBoard.GetLocalPosition()[1]), (self.scrollBoard.GetHeight() - self.peekWindow.GetHeight())))
			self.scrollBar.Show()
	
	def UpdateScrollbar(self, val = 0):
		curr = min(0, max(self.scrollBoard.GetLocalPosition()[1] + val, -self.scrollBoard.GetHeight() + self.peekWindow.GetHeight()))
		self.scrollBoard.SetPosition(0, curr)
		
		self.ChangeScrollbar()
	
	def RegisterScrollBar(self):
		self.scrollBar = WikiScrollBar()
		self.scrollBar.SetParent(self)
		self.scrollBar.SetPosition(self.peekWindow.GetLocalPosition()[0] + self.peekWindow.GetWidth() + 1, self.peekWindow.GetLocalPosition()[1])
		self.scrollBar.SetSize(10, self.peekWindow.GetHeight())
		self.scrollBar.SetScrollEvent(self.OnScrollBar)
		self.scrollBar.SetScrollSpeed(self.SCROLL_SPEED)
		self.scrollBar.Show()
		
		self.ChangeScrollbar()

class WikiMainCostumeWindow(ui.Window):
	class WikiItem(ui.Window):
		def __init__(self, vnum, parent):
			ui.Window.__init__(self)
			
			self.vnum = vnum
			self.parent = proxy(parent)
			
			self.base = ui.ExpandedImageBox()
			self.base.SetParent(self)
			self.base.AddFlag("attach")
			self.base.AddFlag("not_pick")
			self.base.LoadImage("d:/ymir work/ui/wiki/detail_item_small.tga")
			self.base.Show()
			
			self.costumeImage = ui.ExpandedImageBox()
			self.costumeImage.SetParent(self.base)
			self.costumeImage.LoadImage(item.GetIconImageFileName())
			self.costumeImage.SetPosition(1 + 125 / 2 - self.costumeImage.GetWidth() / 2,\
										1 + 120 / 2 - self.costumeImage.GetHeight() / 2)
			self.costumeImage.SetStringEvent("MOUSE_OVER_IN",ui.__mem_func__( self.parent.OnOverIn), self.vnum)
			self.costumeImage.SetStringEvent("MOUSE_OVER_OUT",ui.__mem_func__( self.parent.OnOverOut))
			self.costumeImage.Show()
			
			self.contentText = ui.TextLine()
			self.contentText.SetParent(self.base)
			self.contentText.AddFlag("attach")
			self.contentText.AddFlag("not_pick")
			self.contentText.SetText(item.GetItemName())
			self.contentText.SetPosition(1 + 125 / 2 - self.contentText.GetTextSize()[0] / 2, 122 + 18 / 2 - self.contentText.GetTextSize()[1] / 2 - 1)
			self.contentText.Show()
			
			self.SetSize(self.base.GetWidth(), self.base.GetHeight())
	
	"""Static values!
		Please dont touch on them."""
	ELEM_X_PADDING = 10
	ELEM_PADDING = 10
	SCROLL_SPEED = 25
	ELEM_PER_LINE = 4
	ITEM_LOAD_PER_UPDATE = 1
	
	def __init__(self):
		ui.Window.__init__(self)
		
		self.toolTip = uitooltip.ItemToolTip()
		self.toolTip.AddFlag("not_pick")
		
		self.SetSize(*mainBoardSize)
		
		self.elements = []
		self.costumeVnums = []
		self.posMap = {}
		self.scrollBar = None
		self._dynElemPerLine = None
		self._dynStartX = 1
		
		self.isOpened = False
		self.loadFrom = 0
		self.loadTo = 0
		
		self.wikiRenderTarget = WikiRenderTarget(150, 200)
		self.wikiRenderTarget.SetParent(self.toolTip)
		self.wikiRenderTarget.AddFlag("not_pick")
		self.wikiRenderTarget.SetPosition(5, self.toolTip.toolTipHeight)
		self.wikiRenderTarget.Hide()
		
		self.peekWindow = ui.Window()
		self.peekWindow.SetParent(self)
		self.peekWindow.AddFlag("attach")
		self.peekWindow.AddFlag("not_pick")
		# Modern layout compatibility
		self.peekWindow.SetSize(self.GetWidth() - 14, self.GetHeight() - 15)
		self.peekWindow.SetPosition(5, 5)
		self.peekWindow.SetInsideRender(True)
		self.peekWindow.Show()
		
		self.scrollBoard = ui.Window()
		self.scrollBoard.SetParent(self.peekWindow)
		self.scrollBoard.AddFlag("attach")
		self.scrollBoard.AddFlag("not_pick")
		self.scrollBoard.Show()
		self.RegisterScrollBar()
	
	def OpenWindow(self):
		wndMgr.Show(self.hWnd)
	
	def Hide(self):
		wndMgr.Hide(self.hWnd)
	
	def Show(self, vnums):
		wndMgr.Show(self.hWnd)
		
		extractedLists = []
		for i in vnums:
			if type(i) == types.ListType:
				extractedLists.append(i)
		
		for i in extractedLists:
			pos = vnums.index(i)
			
			vnums.remove(i)
			for j in xrange(len(i)):
				vnums.insert(pos + j, i[j])
		
		isChanged = (not len(vnums) == len(self.costumeVnums))
		
		if not isChanged:
			for i in vnums:
				if i not in self.costumeVnums:
					isChanged = True
					break
		
		if not isChanged:
			for i in self.costumeVnums:
				if i not in vnums:
					isChanged = True
					break
		
		self.costumeVnums = vnums[:]
		self.loadTo = len(self.costumeVnums)
		
		if not self.isOpened:
			self.isOpened = True
			self.loadFrom = 0
		
		if self.loadFrom > self.loadTo or isChanged:
			del self.elements[:]
			self.posMap = {}
			self._dynElemPerLine = None
			self._dynStartX = 1
			
			self.loadFrom = 0
			
			self.scrollBoard.SetSize(0, 0)
			self.UpdateScrollbar()
	
	def GetRandomChar(self):
		ASSASSINS 		= [ player.MAIN_RACE_ASSASSIN_W, player.MAIN_RACE_ASSASSIN_M ]
		WARRIORS 		= [ player.MAIN_RACE_WARRIOR_W, player.MAIN_RACE_WARRIOR_M ]
		SURAS 			= [ player.MAIN_RACE_SURA_W, player.MAIN_RACE_SURA_M ]
		SHAMANS 		= [ player.MAIN_RACE_SHAMAN_W, player.MAIN_RACE_SHAMAN_M ]
		ITEM_CHARACTERS = [ ASSASSINS, WARRIORS, SURAS, SHAMANS ]
		
		SEX_FEMALE		= 0
		SEX_MALE		= 1
		ITEM_SEX		= [ SEX_FEMALE, SEX_MALE ]
		
		#-#
		
		if item.IsAntiFlag( item.ITEM_ANTIFLAG_MALE ):
			ITEM_SEX.remove( SEX_MALE )
		
		if item.IsAntiFlag( item.ITEM_ANTIFLAG_FEMALE ):
			ITEM_SEX.remove( SEX_FEMALE )
		
		#-#
		
		if item.IsAntiFlag( item.ITEM_ANTIFLAG_WARRIOR ):
			ITEM_CHARACTERS.remove( WARRIORS )
		
		if item.IsAntiFlag( item.ITEM_ANTIFLAG_SURA ):
			ITEM_CHARACTERS.remove( SURAS )
		
		if item.IsAntiFlag( item.ITEM_ANTIFLAG_ASSASSIN ):
			ITEM_CHARACTERS.remove( ASSASSINS )
		
		if item.IsAntiFlag( item.ITEM_ANTIFLAG_SHAMAN ):
			ITEM_CHARACTERS.remove( SHAMANS )
		
		return ITEM_CHARACTERS[app.GetRandom(0, len(ITEM_CHARACTERS) - 1)][ITEM_SEX[app.GetRandom(0, len(ITEM_SEX) - 1)]]

	def GetCharTypeHairCamera(self, char_type):
		if not ingamewikiconfig.HAIRSTYLE_CAMERA_CFG.has_key(char_type):
			return tuple([], [])
		
		return ingamewikiconfig.HAIRSTYLE_CAMERA_CFG[char_type]
	
	def OnOverIn(self, vnum, metinSlot = [0 for i in xrange(player.METIN_SOCKET_MAX_NUM)]):
		if not self.toolTip:
			return
		
		self.toolTip.ClearToolTip()
		self.toolTip.SetThinBoardSize(self.wikiRenderTarget.GetWidth() + 10, self.wikiRenderTarget.GetHeight() + self.toolTip.toolTipHeight)
		self.toolTip.childrenList.append(self.wikiRenderTarget)
		self.toolTip.ResizeToolTip()
		
		item.SelectItem(vnum)
		
		itemType = item.GetItemType()
		subType = item.GetItemSubType()
		char_type = self.GetRandomChar()
		
		if app.ENABLE_COSTUME_MOUNT and itemType == item.ITEM_TYPE_COSTUME and subType == 7:
			self.wikiRenderTarget.SetModel(item.GetValue(0))
		elif app.ENABLE_COSTUME_PET and itemType == item.ITEM_TYPE_COSTUME and subType == 6:
			self.wikiRenderTarget.SetModel(item.GetValue(0))
		elif app.ENABLE_MOUNT_COSTUME_SYSTEM and itemType == item.ITEM_TYPE_COSTUME and subType == 2:
			self.wikiRenderTarget.SetModel(item.GetValue(1))
		elif (app.NEW_PET_SYSTEM) and vnum in uitooltip.PetVnum:
			self.wikiRenderTarget.SetModel(uitooltip.PetVnum[vnum])
			if vnum == 53001 or vnum == 53003 or vnum == 53004 or vnum == 53005:
				self.wikiRenderTarget.SetModelForm(11899)
				self.wikiRenderTarget.SetWeaponModel(7159)
		else:
			self.wikiRenderTarget.SetModel(char_type)
			if itemType == item.ITEM_TYPE_WEAPON or itemType == item.ITEM_TYPE_COSTUME and subType == item.COSTUME_TYPE_WEAPON:
				self.wikiRenderTarget.SetWeaponModel(vnum)
			elif itemType == item.ITEM_TYPE_COSTUME and subType == item.COSTUME_TYPE_BODY:
				self.wikiRenderTarget.SetModelForm(vnum)
			elif itemType == item.ITEM_TYPE_COSTUME and subType == item.COSTUME_TYPE_HAIR:
				self.wikiRenderTarget.SetModelHair(item.GetValue(3))
				(V3Eye, V3Target) = self.GetCharTypeHairCamera(char_type)
				if len(V3Eye) and len(V3Target):
					self.wikiRenderTarget.SetModelV3Eye(*V3Eye)
					self.wikiRenderTarget.SetModelV3Target(*V3Target)
			elif app.ENABLE_ACCE_SYSTEM and itemType == item.ITEM_TYPE_COSTUME and subType == item.COSTUME_TYPE_ACCE:
				self.wikiRenderTarget.SetModelSash(vnum)
				if vnum == 85004 or vnum == 85008 or vnum == 85014 or vnum == 85018 or vnum == 85074 or vnum == 85078 or vnum == 85082:
					self.wikiRenderTarget.SetModelArmorEffect(75)
			elif app.ENABLE_STOLE_REAL and itemType == item.ITEM_TYPE_COSTUME and subType == item.COSTUME_TYPE_STOLE:
				self.wikiRenderTarget.SetModelSash(vnum)
				if app.__ENABLE_NEW_EFFECT_STOLE__ and vnum in stole_costumes_effect:
					self.wikiRenderTarget.SetModelArmorEffect(stole_costumes_effect[vnum])
			elif app.ENABLE_COSTUME_EFFECT and itemType == item.ITEM_TYPE_COSTUME and (subType == item.COSTUME_EFFECT_BODY or subType == item.COSTUME_EFFECT_WEAPON):
				if subType == item.COSTUME_EFFECT_BODY:
					armor = 11209
					if char_type == 1 or char_type == 5:
						armor == 11409
					elif char_type == 2 or char_type == 6:
						armor == 11609
					elif char_type == 3 or char_type == 7:
						armor == 11809
					
					self.wikiRenderTarget.SetModelForm(armor)
					self.wikiRenderTarget.SetModelArmorEffect(item.GetValue(0))
				else:
					weaponID = 19
					effectID = item.GetValue(0)
					if char_type == 1 or char_type == 5:
						effectID = item.GetValue(2)
						weaponID = 1009
					elif char_type == 3 or char_type == 7:
						effectID = item.GetValue(5)
						weaponID = 7009
					
					self.wikiRenderTarget.SetWeaponModel(weaponID)
					self.wikiRenderTarget.SetModelWeaponEffect(effectID)
		
		self.toolTip.AddItemData(vnum, metinSlot, 0, wiki = 1)
		self.wikiRenderTarget.SetPosition(self.toolTip.GetWidth() / 2 - self.wikiRenderTarget.GetWidth() / 2, self.wikiRenderTarget.GetLocalPosition()[1])
		self.wikiRenderTarget.Show()

	def OnOverOut(self):
		if not self.toolTip:
			return
		
		self.toolTip.Hide()
	
	def OnUpdate(self):
		if self.loadFrom < self.loadTo:
			for i in xrange(self.loadFrom, min(self.loadTo, self.loadFrom + self.ITEM_LOAD_PER_UPDATE)):
				self.AddItem(self.costumeVnums[i])
				self.loadFrom += 1
	
	def AddItem(self, vnum):
		for i in self.elements:
			if vnum == i.vnum:
				return None
		
		item.SelectItem(vnum)
		
		tmp = self.WikiItem(vnum, self)
		tmp.SetParent(self.scrollBoard)
		tmp.AddFlag("attach")
		# Dynamic columns: fill to right edge (Costumes / Animals)
		if self._dynElemPerLine is None:
			availW = self.peekWindow.GetWidth() - 2
			elemW = tmp.GetWidth()
			per = int((availW + self.ELEM_X_PADDING) / float(elemW + self.ELEM_X_PADDING)) if elemW > 0 else 1
			if per < 1:
				per = 1
			# cap to keep the design consistent
			if per > 6:
				per = 6

			self._dynElemPerLine = per
			totalW = per * elemW + (per - 1) * self.ELEM_X_PADDING
			self._dynStartX = max(1, int((availW - totalW) / 2) + 1)

		perLine = self._dynElemPerLine
		startX = self._dynStartX

		
		totalElem = len(self.elements)
		if totalElem > 0:
			currAdd = 0
			while currAdd in self.posMap:
				currAdd += 1
			
			totalLine = currAdd % perLine
			currH = math.floor(currAdd / perLine) * (tmp.GetHeight() + self.ELEM_PADDING)
			
			self.posMap[currAdd] = True
			tmp.SetPosition(startX + totalLine * (tmp.GetWidth() + self.ELEM_X_PADDING), 0 + currH)
		else:
			self.posMap[0] = True
			tmp.SetPosition(startX, 0)
		
		tmp.Show()
		self.elements.append(tmp)
		
		self.scrollBoard.SetSize(self.peekWindow.GetWidth(), max(self.scrollBoard.GetHeight(), tmp.GetLocalPosition()[1] + tmp.GetHeight()))
		self.UpdateScrollbar()

	def OnRunMouseWheel(self, length):
		if self.peekWindow.IsInPosition():
			self.UpdateScrollbar(int((length * 0.01) * self.SCROLL_SPEED))
			return True
		
		return False

	def OnScrollBar(self, fScale):
		curr = min(0, max(math.ceil((self.scrollBoard.GetHeight() - self.peekWindow.GetHeight()) * fScale * -1.0), -self.scrollBoard.GetHeight() + self.peekWindow.GetHeight()))
		self.scrollBoard.SetPosition(0, curr)

	def ChangeScrollbar(self):
		if not self.scrollBar:
			return
		
		if self.scrollBoard.GetHeight() <= self.GetHeight():
			self.scrollBar.Hide()
		else:
			self.scrollBar.SetScale(self.GetHeight(), self.scrollBoard.GetHeight())
			self.scrollBar.SetPosScale(truediv(abs(self.scrollBoard.GetLocalPosition()[1]), (self.scrollBoard.GetHeight() - self.peekWindow.GetHeight())))
			self.scrollBar.Show()
	
	def UpdateScrollbar(self, val = 0):
		curr = min(0, max(self.scrollBoard.GetLocalPosition()[1] + val, -self.scrollBoard.GetHeight() + self.peekWindow.GetHeight()))
		self.scrollBoard.SetPosition(0, curr)
		
		self.ChangeScrollbar()
	
	def RegisterScrollBar(self):
		self.scrollBar = WikiScrollBar()
		self.scrollBar.SetParent(self)
		self.scrollBar.SetPosition(self.peekWindow.GetLocalPosition()[0] + self.peekWindow.GetWidth() + 1, self.peekWindow.GetLocalPosition()[1])
		self.scrollBar.SetSize(10, self.peekWindow.GetHeight())
		self.scrollBar.SetScrollEvent(self.OnScrollBar)
		self.scrollBar.SetScrollSpeed(self.SCROLL_SPEED)
		self.scrollBar.Show()
		
		self.ChangeScrollbar()

def InitMainWindow(self):
	self.AddFlag('movable')
	self.AddFlag('float')
	
	for i in ingamewikiconfig.ITEM_BLACKLIST:
		if type(i) == types.ListType:
			for k in i:
				wiki.RegisterItemBlacklist(k)
		else:
			wiki.RegisterItemBlacklist(i)
	
	for i in ingamewikiconfig.MOB_WHITELIST:
		if type(i) == types.ListType:
			for k in i:
				wiki.RegisterMonsterBlacklist(k)
		else:
			wiki.RegisterMonsterBlacklist(i)
	
	nonplayer.BuildWikiSearchList()

def InitMainWeaponWindow(self):
	self.mainWeaponWindow = WikiMainWeaponWindow()
	self.mainWeaponWindow.SetParent(self.baseBoard)
	self.mainWeaponWindow.AddFlag("attach")
	self.mainWeaponWindow.SetPosition(*mainBoardPos)

def InitMainChestWindow(self):
	self.mainChestWindow = WikiMainChestWindow()
	self.mainChestWindow.SetParent(self.baseBoard)
	self.mainChestWindow.AddFlag("attach")
	self.mainChestWindow.SetPosition(*mainBoardPos)

def InitMainBossWindow(self):
	self.mainBossWindow = WikiMainBossWindow()
	self.mainBossWindow.SetParent(self.baseBoard)
	self.mainBossWindow.AddFlag("attach")
	self.mainBossWindow.SetPosition(*mainBoardPos)

def InitCustomPageWindow(self):
	self.customPageWindow = SimpleTextLoader()
	self.customPageWindow.SetParent(self.baseBoard)
	self.customPageWindow.AddFlag("attach")
	self.customPageWindow.SetPosition(*mainBoardPos)
	self.customPageWindow.Show()

def InitCostumePageWindow(self):
	self.costumePageWindow = WikiMainCostumeWindow()
	self.costumePageWindow.SetParent(self.baseBoard)
	self.costumePageWindow.AddFlag("attach")
	self.costumePageWindow.SetPosition(*mainBoardPos)

def InitTitleBar(self):
	self.titleBar = ui.TitleBar()
	self.titleBar.SetParent(self.baseBoard)
	self.titleBar.MakeTitleBar(0, "red")
	self.titleBar.SetWidth(self.GetWidth() - 15)
	self.titleBar.SetPosition(8, 7)
	self.titleBar.SetCloseEvent(self.Close)
	self.titleBar.Show()
	
	self.titleName = ui.TextLine()
	self.titleName.SetParent(self.titleBar)
	self.titleName.SetPosition(0, 4)
	self.titleName.SetPosition(0, 4)
	self.titleName.SetWindowHorizontalAlignCenter()
	self.titleName.SetHorizontalAlignCenter()
	self.titleName.SetText(localeinfo.WIKI_TITLENAME)
	self.titleName.Show()

def BuildSearchWindow(self):
	self.searchBG = ui.ExpandedImageBox()
	self.searchBG.SetParent(self.baseBoard)
	self.searchBG.LoadImage("d:/ymir work/ui/wiki/searchfield.tga")
	self.searchBG.SetPosition(19, 33 + 58)
	self.searchBG.Show()
	
	self.searchButton = ui.Button()
	self.searchButton.SetParent(self.searchBG)
	self.searchButton.SetUpVisual("d:/ymir work/ui/wiki/button_search_normal.tga")
	self.searchButton.SetOverVisual("d:/ymir work/ui/wiki/button_search_hover.tga")
	self.searchButton.SetDownVisual("d:/ymir work/ui/wiki/button_search_down.tga")
	self.searchButton.SetPosition(self.searchBG.GetWidth() - self.searchButton.GetWidth() - 6, self.searchBG.GetHeight() / 2 - self.searchButton.GetHeight() / 2)
	self.searchButton.SAFE_SetEvent(self.StartSearch)
	# Fallback binds (client differences)
	try:
		self.searchButton.SetEvent(self.StartSearch)
	except:
		pass
	try:
		self.searchButton.SetStringEvent("MOUSE_LEFT_DOWN", ui.__mem_func__(self.StartSearch))
	except:
		pass
	try:
		self.searchButton.SetStringEvent("MOUSE_LEFT_UP", ui.__mem_func__(self.StartSearch))
	except:
		pass
	try:
		self.searchButton.SetMouseLeftButtonUpEvent(ui.__mem_func__(self.StartSearch))
	except:
		pass
	self.searchButton.Show()
	
	self.searchEdit = ui.EditLine()
	self.searchEdit.SetParent(self.searchBG)
	self.searchEdit.SetMax(50)
	self.searchEdit.SetSize(111 - self.searchButton.GetWidth() - 2, 15)
	self.searchEdit.SetPosition(5, self.searchBG.GetHeight() / 2 - 15 / 2)
	self.searchEdit.SetOverlayText(localeinfo.WIKI_SEARCH_OVERLAY_TEXT)
	self.searchEdit.SetLimitWidth(self.searchEdit.GetWidth())
	self.searchEdit.SetEscapeEvent(ui.__mem_func__(self.OnPressNameEscapeKey))
	self.searchEdit.SetReturnEvent(ui.__mem_func__(self.StartSearch))
	self.searchEdit.SetUpdateEvent(ui.__mem_func__(self.Search_RefreshTextHint))
	self.searchEdit.SetTabEvent(ui.__mem_func__(self.Search_CompleteTextSearch))
	self.searchEdit.SetOutline()
	self.searchEdit.Show()
	
	self.searchEditHint = ui.TextLine()
	self.searchEditHint.SetParent(self.searchEdit)
	self.searchEditHint.SetPackedFontColor(grp.GenerateColor(1.0, 1.0, 1.0, 0.5))
	self.searchEditHint.Show()

def BuildBaseMain(self):
	self.baseBoard = ui.ExpandedImageBox()
	self.baseBoard.AddFlag("attach")
	self.baseBoard.SetParent(self)
	self.baseBoard.LoadImage("D:/Ymir Work/ui/wiki/bg.tga")
	self.baseBoard.SetWindowName("InGameWikiBoard")
	self.baseBoard.Show()
	
	self.SetSize(self.baseBoard.GetWidth(), self.baseBoard.GetHeight())

	# --- HEADER (fent banner) ---
	self.header = ui.ExpandedImageBox()
	self.header.SetParent(self.baseBoard)
	self.header.SetPosition(149, 37)

	# --- KERESS MEZO S GOMB ---
	BuildSearchWindow(self)
	self.exactSearch = ui.InGameWikiCheckBox()
	self.exactSearch.SetParent(self.baseBoard)
	self.exactSearch.SetPosition(19, self.searchBG.GetLocalPosition()[1] + self.searchBG.GetHeight() + 5)
	self.exactSearch.SetTextInfo(localeinfo.WIKI_EXACT_SEARCH_TEXT)
	self.exactSearch.Show()

	# --- KATEGRIK + SCROLLBR ---
	self.categ = WikiCategories()
	self.categ.hideWindowsEvent = ui.__mem_func__(self.CloseBaseWindows)
	self.categ.SetParent(self.baseBoard)
	self.categ.AddFlag("attach")
	self.categ.SetPosition(15, 107 + 41)
	self.categ.Show()

	self.scrollBar = WikiScrollBar()
	self.scrollBar.SetParent(self.baseBoard)
	self.scrollBar.SetPosition(
		self.categ.GetLocalPosition()[0] + self.categ.GetWidth() + 5,
		self.categ.GetLocalPosition()[1] + 5
	)
	self.scrollBar.SetSize(12, categoryPeakWindowSize[1] - 10)
	self.scrollBar.Show()

	self.categ.RegisterScrollBar(self.scrollBar)
	
	# --- ABLAKOK INICIALIZLSA ---
	InitTitleBar(self)
	InitMainWeaponWindow(self)
	InitMainChestWindow(self)
	InitMainBossWindow(self)
	InitCustomPageWindow(self)
	InitCostumePageWindow(self)

	# --- KATEGRIK CSATOLSA ---
	funclist = [
		ui.__mem_func__(self.mainWeaponWindow.Show),
		ui.__mem_func__(self.costumePageWindow.Show),
		ui.__mem_func__(self.costumePageWindow.Show),
		ui.__mem_func__(self.mainChestWindow.Show),
		ui.__mem_func__(self.mainBossWindow.Show),
		ui.__mem_func__(self.mainBossWindow.Show),
		ui.__mem_func__(self.mainBossWindow.Show),
		ui.__mem_func__(self.customPageWindow.LoadFile),
		ui.__mem_func__(self.customPageWindow.LoadFile),
		ui.__mem_func__(self.customPageWindow.LoadFile),
		ui.__mem_func__(self.customPageWindow.LoadFile),
	]

	for i in ingamewikiconfig.WIKI_CATEGORIES:
		ret = self.categ.AddCategory(i[0])
		for j in xrange(len(i[1])):
			ret.AddSubCategory(j, i[1][j])
		curr = ingamewikiconfig.WIKI_CATEGORIES.index(i)
		if curr < len(funclist):
			ret.clickEvent = funclist[curr]

	# --- STARTOLDAL BETLTSE ---
	self.customPageWindow.LoadFile("landingpage.txt")
	self.header.SetStringEvent("MOUSE_LEFT_DOWN", ui.__mem_func__(self.GoToLanding))
### MODERN WIKI UI OVERRIDES ###
# Textureless modern redesign (frames/panels/buttons) + dynamic sizing to avoid layout drift.
# NOTE: This block intentionally lives at the end of the file to override legacy defs/classes.

WIKI_MODERN_UI = True

# --- Layout constants (bigger, cleaner) ---
# Side bar + content panel inside the main frame.
categoryPeakWindowSize = [240, 490]
mainBoardPos = [284, 48]
mainBoardSize = [662, 518]

# Keep these sizes as-is (used in special/detail pages); only adjust if you also redesign those pages.
# monsterBonusInfoPageSize
# itemOriginPageSize

# --- Modern palette ---
MOD_COL_BG		  = grp.GenerateColor(0.07, 0.08, 0.09, 0.95)
MOD_COL_PANEL	   = grp.GenerateColor(0.10, 0.11, 0.12, 0.92)
MOD_COL_PANEL2	  = grp.GenerateColor(0.12, 0.13, 0.14, 0.92)
MOD_COL_BORDER	  = grp.GenerateColor(0.25, 0.26, 0.28, 0.85)
MOD_COL_TOPBAR	  = grp.GenerateColor(0.12, 0.13, 0.14, 0.98)
MOD_COL_ACCENT	  = grp.GenerateColor(0.28, 0.55, 0.90, 0.95)
MOD_COL_ACCENT_HOV  = grp.GenerateColor(0.36, 0.62, 0.95, 0.98)
MOD_COL_TEXT_DIM	= grp.GenerateColor(0.78, 0.80, 0.82, 0.90)


def _MakeBorder(parent, x, y, w, h, color=MOD_COL_BORDER):
	# Draw 1px border using lines
	l = ui.Line(); l.SetParent(parent); l.AddFlag('attach'); l.AddFlag('not_pick'); l.SetColor(color); l.SetPosition(x, y); l.SetSize(1, h); l.Show()
	r = ui.Line(); r.SetParent(parent); r.AddFlag('attach'); r.AddFlag('not_pick'); r.SetColor(color); r.SetPosition(x + w - 1, y); r.SetSize(1, h); r.Show()
	t = ui.Line(); t.SetParent(parent); t.AddFlag('attach'); t.AddFlag('not_pick'); t.SetColor(color); t.SetPosition(x, y); t.SetSize(w, 1); t.Show()
	b = ui.Line(); b.SetParent(parent); b.AddFlag('attach'); b.AddFlag('not_pick'); b.SetColor(color); b.SetPosition(x, y + h - 1); b.SetSize(w, 1); b.Show()
	return (l, r, t, b)


def _MakePanel(parent, x, y, w, h, color=MOD_COL_PANEL):
	wnd = ui.Window()
	wnd.SetParent(parent)
	wnd.AddFlag('attach')
	wnd.SetPosition(x, y)
	wnd.SetSize(w, h)
	wnd.Show()

	bg = ui.Bar()
	bg.SetParent(wnd)
	bg.AddFlag('attach')
	bg.AddFlag('not_pick')
	bg.SetPosition(0, 0)
	bg.SetSize(w, h)
	bg.SetColor(color)
	bg.Show()

	borders = _MakeBorder(wnd, 0, 0, w, h)
	wnd._bg = bg
	wnd._borders = borders
	return wnd



# Path for a 1x1 transparent texture used for modern flat buttons.
# Add this file to your client pack: d:/ymir work/ui/wiki/modern/blank.tga
MODERN_BLANK_TGA = "d:/ymir work/ui/wiki/modern/blank.tga"

class ModernButton(ui.ExpandedImageBox):
	"""Flat button with hover + click using ExpandedImageBox events (client-safe)."""
	def __init__(self, w=90, h=24, text=''):
		ui.ExpandedImageBox.__init__(self)
		self._event = None
		self._args = None
		self._base_pos = (0, 0)
		self._hover = False
		self._last_click_time = 0.0
		self._w = int(w)
		self._h = int(h)
		# Load a 1x1 blank; if missing, fall back to an existing wiki texture.
		try:
			self.LoadImage(MODERN_BLANK_TGA)
		except:
			self.LoadImage("d:/ymir work/ui/wiki/category.tga")
		# If blank is 1x1, scaling by (w,h) becomes exact size.
		try:
			self.SetScale(float(self._w), float(self._h))
		except:
			pass
		self.AddFlag('attach')
		try:
			self.SetStringEvent("MOUSE_OVER_IN", ui.__mem_func__(self.__OnOverIn))
			self.SetStringEvent("MOUSE_OVER_OUT", ui.__mem_func__(self.__OnOverOut))
			self.SetStringEvent("MOUSE_LEFT_DOWN", ui.__mem_func__(self.__OnClick))
			self.SetStringEvent("MOUSE_LEFT_UP", ui.__mem_func__(self.__OnClick))
		except:
			pass

		self.bg = ui.Bar()
		self.bg.SetParent(self)
		self.bg.AddFlag('attach')
		self.bg.AddFlag('not_pick')
		self.bg.SetPosition(0, 0)
		self.bg.SetSize(self._w, self._h)
		self.bg.SetColor(MOD_COL_ACCENT)
		self.bg.Show()

		self._borders = _MakeBorder(self, 0, 0, self._w, self._h, MOD_COL_BORDER)

		self.txt = ui.TextLine()
		self.txt.SetParent(self)
		self.txt.AddFlag('attach')
		self.txt.AddFlag('not_pick')
		self.txt.SetWindowHorizontalAlignCenter()
		self.txt.SetWindowVerticalAlignCenter()
		self.txt.SetHorizontalAlignCenter()
		self.txt.SetVerticalAlignCenter()
		self.txt.SetText(text)
		self.txt.SetPackedFontColor(grp.GenerateColor(1.0, 1.0, 1.0, 0.95))
		self.txt.Show()

	def SetText(self, t):
		self.txt.SetText(t)

	def SAFE_SetEvent(self, func, *args):
		self._event = ui.__mem_func__(func)
		self._args = args

	def SetEvent(self, func, *args):
		self._event = func
		self._args = args

	def SetPosition(self, x, y):
		ui.ExpandedImageBox.SetPosition(self, x, y)
		self._base_pos = (x, y)

	def __OnOverIn(self):
		self._hover = True
		self.bg.SetColor(MOD_COL_ACCENT_HOV)
		x, y = self._base_pos
		ui.ExpandedImageBox.SetPosition(self, x, y - 1)
		return True

	def __OnOverOut(self):
		self._hover = False
		self.bg.SetColor(MOD_COL_ACCENT)
		x, y = self._base_pos
		ui.ExpandedImageBox.SetPosition(self, x, y)
		return True

	def __OnClick(self):
		# Fire on mouse down/up (some clients differ). Debounced to avoid double-calls.
		try:
			now = app.GetTime()
		except:
			now = 0.0
		if now and self._last_click_time and (now - self._last_click_time) < 0.18:
			return True
		self._last_click_time = now
		if self._event:
			try:
				apply(self._event, self._args if self._args else tuple())
			except:
				pass
		return True

	def OnMouseLeftButtonDown(self):
		return self.__OnClick()

	def OnMouseLeftButtonUp(self):
		return self.__OnClick()

class ModernClickButton(ui.Window):
	def __init__(self, w, h, text):
		ui.Window.__init__(self)
		self._event = None

		try: self.SetPickAlways()
		except: pass
		self.SetSize(w, h)

		self.bg = ui.Bar()
		self.bg.SetParent(self); self.bg.AddFlag("attach"); self.bg.AddFlag("not_pick")
		self.bg.SetSize(w, h)
		self.bg.SetColor(MOD_COL_ACCENT)
		self.bg.Show()

		_MakeBorder(self, 0, 0, w, h, MOD_COL_BORDER)

		self.txt = ui.TextLine()
		self.txt.SetParent(self); self.txt.AddFlag("attach"); self.txt.AddFlag("not_pick")
		self.txt.SetWindowHorizontalAlignCenter()
		self.txt.SetWindowVerticalAlignCenter()
		self.txt.SetHorizontalAlignCenter()
		self.txt.SetVerticalAlignCenter()
		self.txt.SetText(text)
		self.txt.Show()

	def SAFE_SetEvent(self, func, *args):
		self._event = ui.__mem_func__(func)
		self._args = args

	def OnMouseLeftButtonUp(self):
		if self._event:
			try:
				apply(self._event, self._args if self._args else tuple())
			except:
				pass
		return True


# --- Modern Category Header (replaces category.tga + arrow.tga) ---
class _ClickableWindow(ui.Window):
	"""Compat clickable container.
	Some clients don't expose SetStringEvent on ui.Window.
	We handle clicks via OnMouseLeftButtonDown.
	"""
	def __init__(self):
		ui.Window.__init__(self)
		# Ensure it is pickable on all clients
		try:
			self.SetPickAlways()
		except:
			pass
		self.__click = None

	def SetClickEvent(self, func):
		self.__click = func

	def OnMouseLeftButtonDown(self):
		if self.__click:
			try:
				self.__click()
			except:
				pass
		return True

class WikiCategoryModern(ui.Window):
	TICK_COUNT = 6
	MIN_HEIGHT = 20

	# Softer / modern
	LINES_COLOR  = grp.GenerateColor(0.20, 0.21, 0.23, 0.90)
	CORNER_COLOR = grp.GenerateColor(0.20, 0.21, 0.23, 0.90)
	BASE_COLOR   = grp.GenerateColor(0.14, 0.15, 0.16, 0.95)
	TITLE_COLOR  = grp.GenerateColor(0.12, 0.13, 0.14, 0.98)

	def __init__(self, owner = None):
		ui.Window.__init__(self)

		self.owner = (None if not owner else proxy(owner))
		self.expectedSize = 0
		self.currHeight = self.MIN_HEIGHT
		self.isAnimating = False
		self.isOpening = False
		self.clickEvent = None


		# Title bar (flat) - ExpandedImageBox for reliable mouse events + hover
		w = int(categoryPeakWindowSize[0])
		h = 26
		self.titleBar = ui.ExpandedImageBox()
		self.titleBar.SetParent(self)
		self.titleBar.AddFlag('attach')
		self.titleBar.SetPosition(0, 0)
		try:
			self.titleBar.LoadImage(MODERN_BLANK_TGA)
		except:
			self.titleBar.LoadImage("d:/ymir work/ui/wiki/category.tga")
		try:
			self.titleBar.SetScale(float(w), float(h))
		except:
			pass
		try:
			self.titleBar.SetStringEvent("MOUSE_LEFT_DOWN", ui.__mem_func__(self.ClickExpand))
			self.titleBar.SetStringEvent("MOUSE_OVER_IN", ui.__mem_func__(self._OnTitleOverIn))
			self.titleBar.SetStringEvent("MOUSE_OVER_OUT", ui.__mem_func__(self._OnTitleOverOut))
		except:
			pass
		self.titleBar.Show()

		self.titleBg = ui.Bar()
		self.titleBg.SetParent(self.titleBar)
		self.titleBg.AddFlag('attach')
		self.titleBg.AddFlag('not_pick')
		self.titleBg.SetPosition(0, 0)
		self.titleBg.SetSize(w, h)
		self.titleBg.SetColor(self.TITLE_COLOR)
		self.titleBg.Show()
		self._titleBorder = _MakeBorder(self.titleBar, 0, 0, w, h, MOD_COL_BORDER)

		self.titleText = ui.TextLine()
		self.titleText.SetParent(self.titleBar)
		self.titleText.AddFlag('attach')
		self.titleText.AddFlag('not_pick')
		self.titleText.SetPosition(10, 0)
		self.titleText.SetWindowVerticalAlignCenter()
		self.titleText.SetVerticalAlignCenter()
		self.titleText.SetPackedFontColor(MOD_COL_TEXT_DIM)
		self.titleText.Show()

		self.arrowText = ui.TextLine()
		self.arrowText.SetParent(self.titleBar)
		self.arrowText.AddFlag('attach')
		self.arrowText.AddFlag('not_pick')
		self.arrowText.SetText('>')
		self.arrowText.SetWindowVerticalAlignCenter()
		self.arrowText.SetVerticalAlignCenter()
		self.arrowText.SetPackedFontColor(MOD_COL_TEXT_DIM)
		self.arrowText.Show()
		self._UpdateArrowPos()

		# Expand area

		self.expandWnd = ui.Window()
		self.expandWnd.SetParent(self)
		self.expandWnd.AddFlag('attach')
		self.expandWnd.AddFlag('not_pick')
		self.expandWnd.SetPosition(0, self.titleBar.GetHeight())
		self.expandWnd.SetSize(self.titleBar.GetWidth(), 0)
		self.expandWnd.Show()

		self.bars = []
		for i in xrange(self.TICK_COUNT):
			br = ui.Bar()
			br.SetParent(self.expandWnd)
			br.AddFlag('attach')
			br.AddFlag('not_pick')
			br.SetColor([self.LINES_COLOR, self.CORNER_COLOR, self.BASE_COLOR][2 if i == (self.TICK_COUNT - 1) else int(i % 2 == 0)])
			br.Show()
			self.bars.append(br)

		self.categList = ui.ListBoxEx()
		self.categList.SetParent(self.expandWnd)
		self.categList.SetItemSize(self.titleBar.GetWidth() - 10, 17)
		self.categList.SetItemStep(17)
		self.categList.SetSize(self.titleBar.GetWidth() - 10, self.MIN_HEIGHT - 1)
		self.categList.SetPosition(5, 1)
		self.categList.SetSelectEvent(ui.__mem_func__(self.OnSelectSubCategory))
		self.categList.Show()

		ui.Window.SetSize(self, self.titleBar.GetWidth(), self.titleBar.GetHeight())
		self.ArrangeBars(self.MIN_HEIGHT)
	def __OnTitleOverIn(self):
		if hasattr(self, '_hoverBar') and self._hoverBar:
			self._hoverBar.SetColor(0x44000000)

	def __OnTitleOverOut(self):
		if hasattr(self, '_hoverBar') and self._hoverBar:
			self._hoverBar.SetColor(0x22000000)
	def _UpdateArrowPos(self):
		# right aligned
		w = self.titleBar.GetWidth()
		self.arrowText.SetPosition(w - 14, 0)

	def OnSelectSubCategory(self, elem):
		if self.owner:
			self.owner.NotifyCategorySelect(self)

		wikiClass = wiki.GetBaseClass()
		if wikiClass and hasattr(wikiClass, 'header'):
			try:
				wikiClass.header.Hide()
			except:
				pass
		if self.clickEvent:
			apply(self.clickEvent, elem.mArgs)

	def UnselectSubCategory(self):
		self.categList.SelectIndex(-1)

	def AddSubCategory(self, key, text):
		self.currHeight = (17 * (self.categList.GetItemCount() + 1) + 3)
		if self.expandWnd.GetHeight() > 0:
			self.ArrangeBars(self.currHeight)
			self.isAnimating = True
		self.categList.SetViewItemCount(self.categList.GetItemCount() + 1)
		item = SubCategObject(text)
		self.categList.AppendItem(item)
		self.categList.SetSize(self.categList.GetWidth(), self.currHeight - 1)

	def ClickExpand(self):
		self.isOpening = not self.isOpening
		self.isAnimating = True
		self.expandWnd.Show()
		self.arrowText.SetText('v' if self.isOpening else '>')
		self._UpdateArrowPos()
		self.ArrangeBars(self.currHeight if self.isOpening else 0)

	def OnUpdate(self):
		if not self.isAnimating:
			return
		h = self.expandWnd.GetHeight()
		if h == self.expectedSize:
			self.isAnimating = False
			if h == 0:
				self.expandWnd.Hide()
			return

		step = (self.currHeight / self.TICK_COUNT)
		newSize = 0
		isOpening = (False if h > self.expectedSize else True)
		if isOpening:
			newSize = min(self.currHeight, self.expandWnd.GetHeight() + step)
		else:
			newSize = max(0, self.expandWnd.GetHeight() - step)

		change = newSize - self.expandWnd.GetHeight()
		self.expandWnd.SetSize(self.titleBar.GetWidth(), newSize)
		ui.Window.SetSize(self, self.titleBar.GetWidth(), self.titleBar.GetHeight() + newSize)
		if self.owner:
			self.owner.NotifySizeChange(self, change, (newSize > h))

	def SetSize(self, width, height):
		dbg.LogBox('WikiCategory -> SetSize - unsupported function')

	def ArrangeBars(self, currHeight):
		self.expectedSize = currHeight
		if currHeight < self.MIN_HEIGHT:
			return
		currWidth = self.expandWnd.GetWidth()
		self.bars[0].SetSize(1, (currHeight - 1))
		self.bars[1].SetSize(1, 1)
		self.bars[1].SetPosition(0, (currHeight - 1))
		self.bars[2].SetSize((currWidth - 2), 1)
		self.bars[2].SetPosition(1, (currHeight - 1))
		self.bars[3].SetSize(1, 1)
		self.bars[3].SetPosition((currWidth - 1), (currHeight - 1))
		self.bars[4].SetSize(1, (currHeight - 1))
		self.bars[4].SetPosition((currWidth - 1), 0)
		self.bars[5].SetSize((currWidth - 2), (currHeight - 1))
		self.bars[5].SetPosition(1, 0)

	def SetTitleName(self, text):
		self.titleText.SetText(text)


# --- Modern window frame + titlebar ---
def InitTitleBar(self):
	# Topbar
	self.topBar = ui.Window()
	self.topBar.SetParent(self.baseBoard)
	self.topBar.AddFlag('attach')
	self.topBar.SetPosition(0, 0)
	self.topBar.SetSize(self.baseBoard.GetWidth(), 38)
	self.topBar.Show()

	self.topBarBg = ui.Bar()
	self.topBarBg.SetParent(self.topBar)
	self.topBarBg.AddFlag('attach')
	self.topBarBg.AddFlag('not_pick')
	self.topBarBg.SetPosition(0, 0)
	self.topBarBg.SetSize(self.topBar.GetWidth(), self.topBar.GetHeight())
	self.topBarBg.SetColor(MOD_COL_TOPBAR)
	self.topBarBg.Show()
	self._topBorder = _MakeBorder(self.topBar, 0, 0, self.topBar.GetWidth(), self.topBar.GetHeight(), MOD_COL_BORDER)

	self.titleName = ui.TextLine()
	self.titleName.SetParent(self.topBar)
	self.titleName.AddFlag('attach')
	self.titleName.AddFlag('not_pick')
	self.titleName.SetPosition(14, 0)
	self.titleName.SetWindowVerticalAlignCenter()
	self.titleName.SetVerticalAlignCenter()
	self.titleName.SetText(localeinfo.WIKI_TITLENAME)
	self.titleName.SetPackedFontColor(grp.GenerateColor(1.0, 1.0, 1.0, 0.92))
	self.titleName.Show()

	self.closeBtn = ui.Button()
	self.closeBtn.SetParent(self.topBar)
	self.closeBtn.SetUpVisual('d:/ymir work/ui/public/close_button_01.sub')
	self.closeBtn.SetOverVisual('d:/ymir work/ui/public/close_button_02.sub')
	self.closeBtn.SetDownVisual('d:/ymir work/ui/public/close_button_03.sub')
	self.closeBtn.SetPosition(self.topBar.GetWidth() - 24, 10)
	self.closeBtn.SAFE_SetEvent(self.Close)
	self.closeBtn.Show()


def BuildSearchWindow(self):
	# Search container inside topbar
	w = 360
	h = 26
	pad = 10

	self.searchBG = _MakePanel(self.topBar, self.topBar.GetWidth() // 2 - w // 2, 6, w, h, MOD_COL_PANEL2)

	self.searchEdit = ui.EditLine()
	self.searchEdit.SetParent(self.searchBG)
	self.searchEdit.SetMax(50)
	self.searchEdit.SetSize(w - 110, 18)
	self.searchEdit.SetPosition(10, h // 2 - 9)
	self.searchEdit.SetOverlayText(localeinfo.WIKI_SEARCH_OVERLAY_TEXT)
	self.searchEdit.SetLimitWidth(self.searchEdit.GetWidth())
	self.searchEdit.SetEscapeEvent(ui.__mem_func__(self.OnPressNameEscapeKey))
	self.searchEdit.SetReturnEvent(ui.__mem_func__(self.StartSearch))
	self.searchEdit.SetUpdateEvent(ui.__mem_func__(self.Search_RefreshTextHint))
	self.searchEdit.SetTabEvent(ui.__mem_func__(self.Search_CompleteTextSearch))
	self.searchEdit.SetOutline()
	self.searchEdit.Show()

	self.searchEditHint = ui.TextLine()
	self.searchEditHint.SetParent(self.searchEdit)
	self.searchEditHint.SetPackedFontColor(grp.GenerateColor(1.0, 1.0, 1.0, 0.45))
	self.searchEditHint.Show()

	self.searchButton = ModernButton(90, 22, localeinfo.WIKI_SEARCH_BUTTON if hasattr(localeinfo, 'WIKI_SEARCH_BUTTON') else 'Search')
	self.searchButton.SetParent(self.searchBG)
	self.searchButton.AddFlag('attach')
	self.searchButton.SetPosition(w - 95, 2)
	self.searchButton.SAFE_SetEvent(self.StartSearch)
	# Click fallback binds (some clients ignore SAFE_SetEvent on ExpandedImageBox)
	self.searchButton = ModernClickButton(90, 22, "Search")
	self.searchButton.SetParent(self.searchBG)
	self.searchButton.SetPosition(w - 95, 2)
	self.searchButton.SAFE_SetEvent(self.StartSearch)
	self.searchButton.Show()


	try:
		self.searchButton.SetEvent(self.StartSearch)
	except:
		pass
	try:
		self.searchButton.SetStringEvent("MOUSE_LEFT_DOWN", ui.__mem_func__(self.StartSearch))
	except:
		pass
	try:
		self.searchButton.SetStringEvent("MOUSE_LEFT_UP", ui.__mem_func__(self.StartSearch))
	except:
		pass
	try:
		self.searchButton.SetMouseLeftButtonUpEvent(ui.__mem_func__(self.StartSearch))
	except:
		pass
	self.searchButton.Show()

	# Exact search toggle (now visible)
	self.exactSearch = ui.InGameWikiCheckBox()
	self.exactSearch.SetParent(self.topBar)
	self.exactSearch.SetPosition(self.searchBG.GetLocalPosition()[0] + w + 10, 9)
	self.exactSearch.SetTextInfo(localeinfo.WIKI_EXACT_SEARCH_TEXT)
	self.exactSearch.Show()


def InitMainWeaponWindow(self):
	self.mainWeaponWindow = WikiMainWeaponWindow()
	self.mainWeaponWindow.SetParent(self.contentPanel)
	self.mainWeaponWindow.AddFlag('attach')
	self.mainWeaponWindow.SetPosition(0, 0)


def InitMainChestWindow(self):
	self.mainChestWindow = WikiMainChestWindow()
	self.mainChestWindow.SetParent(self.contentPanel)
	self.mainChestWindow.AddFlag('attach')
	self.mainChestWindow.SetPosition(0, 0)


def InitMainBossWindow(self):
	self.mainBossWindow = WikiMainBossWindow()
	self.mainBossWindow.SetParent(self.contentPanel)
	self.mainBossWindow.AddFlag('attach')
	self.mainBossWindow.SetPosition(0, 0)


def InitCustomPageWindow(self):
	self.customPageWindow = SimpleTextLoader()
	self.customPageWindow.SetParent(self.contentPanel)
	self.customPageWindow.AddFlag('attach')
	self.customPageWindow.SetPosition(0, 0)
	self.customPageWindow.Show()


def InitCostumePageWindow(self):
	self.costumePageWindow = WikiMainCostumeWindow()
	self.costumePageWindow.SetParent(self.contentPanel)
	self.costumePageWindow.AddFlag('attach')
	self.costumePageWindow.SetPosition(0, 0)


def BuildBaseMain(self):
	# Base frame (textureless)
	W = 960
	H = 580
	self.SetSize(W, H)

	self.baseBoard = _MakePanel(self, 0, 0, W, H, MOD_COL_BG)
	self.baseBoard.SetWindowName('InGameWikiBoard')

	# Titlebar + search
	InitTitleBar(self)
	BuildSearchWindow(self)

	# Sidebar + content panels
	sideX = 14
	sideY = 48
	sideW = categoryPeakWindowSize[0] + 18
	sideH = H - sideY - 14

	contentX = sideX + sideW + 12
	contentY = sideY
	contentW = W - contentX - 14
	contentH = sideH

	self.sidebarPanel = _MakePanel(self.baseBoard, sideX, sideY, sideW, sideH, MOD_COL_PANEL)
	self.contentPanel = _MakePanel(self.baseBoard, contentX, contentY, contentW, contentH, MOD_COL_PANEL)

	# Banner header for categories (optional)
	self.header = ui.ExpandedImageBox()
	self.header.SetParent(self.contentPanel)
	self.header.SetPosition(10, 10)
	self.header.Hide()

	# Categories list (inside sidebar)
	self.categ = WikiCategories()
	self.categ.hideWindowsEvent = ui.__mem_func__(self.CloseBaseWindows)
	self.categ.SetParent(self.sidebarPanel)
	self.categ.AddFlag('attach')
	self.categ.SetPosition(9, 12)
	self.categ.Show()

	# Sidebar scrollbar
	self.scrollBar = WikiScrollBar()
	self.scrollBar.SetParent(self.sidebarPanel)
	self.scrollBar.SetPosition(self.categ.GetLocalPosition()[0] + self.categ.GetWidth() + 6, self.categ.GetLocalPosition()[1] + 5)
	self.scrollBar.SetSize(12, categoryPeakWindowSize[1] - 10)
	self.scrollBar.Show()
	self.categ.RegisterScrollBar(self.scrollBar)

	# Main windows
	InitMainWeaponWindow(self)
	InitMainChestWindow(self)
	InitMainBossWindow(self)
	InitCustomPageWindow(self)
	InitCostumePageWindow(self)

	funclist = [
		ui.__mem_func__(self.mainWeaponWindow.Show),
		ui.__mem_func__(self.costumePageWindow.Show),
		ui.__mem_func__(self.costumePageWindow.Show),
		ui.__mem_func__(self.mainChestWindow.Show),
		ui.__mem_func__(self.mainBossWindow.Show),
		ui.__mem_func__(self.mainBossWindow.Show),
		ui.__mem_func__(self.mainBossWindow.Show),
		ui.__mem_func__(self.customPageWindow.LoadFile),
		ui.__mem_func__(self.customPageWindow.LoadFile),
		ui.__mem_func__(self.customPageWindow.LoadFile),
		ui.__mem_func__(self.customPageWindow.LoadFile),
	]

	for i in ingamewikiconfig.WIKI_CATEGORIES:
		ret = self.categ.AddCategory(i[0])
		for j in xrange(len(i[1])):
			ret.AddSubCategory(j, i[1][j])
		curr = ingamewikiconfig.WIKI_CATEGORIES.index(i)
		if curr < len(funclist):
			ret.clickEvent = funclist[curr]

	self.customPageWindow.LoadFile('landingpage.txt')

	# Click on banner area goes to landing (optional): use title text click
	try:
		self.titleName.SetStringEvent('MOUSE_LEFT_DOWN', ui.__mem_func__(self.GoToLanding))
	except:
		pass


class WikiChestLootPopup(ui.BoardWithTitleBar):
	"""
	Wiki lada loot betekinto popup.
	Cel: a drop lista auto-meretezese ugy, hogy lehetoseg szerint ne kelljen scrollozni.
	"""
	CELL_W = 36
	CELL_H = 32

	LEFT_W = 86
	ORIGIN_W = 150
	PAD = 10
	TITLE_H = 24

	# Biztonsagi limit, hogy ne legyen tul kicsi a bal oldali ikon resz
	ICON_MIN_H = 66

	def __init__(self, parent):
		ui.BoardWithTitleBar.__init__(self)

		self.parent = (proxy(parent) if parent else None)
		self.vnum = 0
		self.dropList = None
		self._originLines = []
		self.toolTip = None
		self._didPos = False

		self.AddFlag("movable")
		self.AddFlag("float")

		if parent:
			self.AddFlag("attach")
			self.SetParent(parent)
		self.SetCloseEvent(ui.__mem_func__(self.Close))

		# regisztralas wiki callbackhoz
		wikiBase = wiki.GetBaseClass()
		if wikiBase:
				wikiBase.objList[long(id(self))] = proxy(self)

		# tooltip
		try:
			self.toolTip = uitooltip.ItemToolTip()
			self.toolTip.Hide()
		except:
			self.toolTip = None
		self._didPos = False

		# elemek
		self.chestImage = ui.ExpandedImageBox()
		self.chestImage.SetParent(self)
		self.chestImage.AddFlag("not_pick")
		self.chestImage.Show()

		self.loadingText = ui.TextLine()
		self.loadingText.SetParent(self)
		self.loadingText.AddFlag("not_pick")
		self.loadingText.SetText("Loading...")
		self.loadingText.Show()

		self.originHead = ui.TextLine()
		self.originHead.SetParent(self)
		self.originHead.AddFlag("not_pick")
		self.originHead.SetText(localeinfo.WIKI_CHESTINFO_ORIGIN if hasattr(localeinfo, "WIKI_CHESTINFO_ORIGIN") else "Origin")
		self.originHead.Show()

		self.originText = ui.TextLine()
		self.originText.SetParent(self)
		self.originText.AddFlag("not_pick")
		self.originText.SetText("-")
		self.originText.Show()

		# default size
		self.SetSize(520, 200)
		self.SetTitleName("Chest")
		self.Show()

	
	def Open(self, chestVnum):
		self.vnum = chestVnum
	
		# title + icon
		try:
			item.SelectItem(self.vnum)
			if hasattr(localeinfo, "WIKI_CHESTINFO_CONTENTOF"):
				self.SetTitleName(localeinfo.WIKI_CHESTINFO_CONTENTOF % item.GetItemName())
			else:
				self.SetTitleName("Chest: %s" % item.GetItemName())
			self.chestImage.LoadImage(item.GetIconImageFileName())
		except:
			self.SetTitleName("Chest")
			try:
				self.chestImage.LoadImage("d:/ymir work/ui/game/myshop_deco/slot.tga")
			except:
				pass
	
		self.loadingText.SetText("Loading...")
		self.loadingText.Show()
	
		# toroljuk az elozo listat
		if self.dropList:
			try:
				self.dropList.Hide()
			except:
				pass
			self.dropList = None
	
		# origin sorok torles
		self.__ClearOriginLines()
	
		# kerjuk a szervertol
		try:
			wiki.LoadInfo(long(id(self)), self.vnum)
		except:
			pass
	
		# ha mar cache-ben van, akkor azonnal kirajzoljuk
		self.NoticeMe()
		
		# *** JAVÍTÁS: Megjelenítjük az ablakot ***
		self.Show()
		# Opcionális: Középre igazítás
		self.CenterInParent()
	
	def Close(self):
		try:
			wikiBase = wiki.GetBaseClass()
			if wikiBase:
				wikiBase.objList.pop(long(id(self)), None)
		except:
			pass

		self.Hide()
		return True

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def CenterInParent(self):
		# Ha van parent (wiki foablak), akkor ahhoz igazitsuk.
		try:
			if self.parent:
				pw = self.parent.GetWidth()
				ph = self.parent.GetHeight()
				x = int((pw - self.GetWidth()) / 2)
				y = int((ph - self.GetHeight()) / 2)
				if x < 0: x = 0
				if y < 0: y = 0
				self.SetPosition(x, y)
				return
		except:
			pass

		# Parent nelkul: kepernyo kozepere, es clamp.
		try:
			sw = wndMgr.GetScreenWidth()
			sh = wndMgr.GetScreenHeight()
			w = self.GetWidth()
			h = self.GetHeight()
			x = int((sw - w) / 2)
			y = int((sh - h) / 2)
			if x < 0: x = 0
			if y < 0: y = 0
			if x > sw - w: x = sw - w
			if y > sh - h: y = sh - h
			self.SetPosition(x, y)
		except:
			pass

	def OnOverIn(self, vnum, metinSlot=[0 for i in xrange(player.METIN_SOCKET_MAX_NUM)]):
		if not self.toolTip:
			return
		self.toolTip.ClearToolTip()
		self.toolTip.AddItemData(vnum, metinSlot, 0, wiki=1)

	def OnOverOut(self):
		if not self.toolTip:
			return
		self.toolTip.Hide()

	def __ClearOriginLines(self):
		if self._originLines:
			for l in self._originLines:
				try:
					l.Hide()
				except:
					pass
		self._originLines = []

	def __GenMultiLine(self, text, maxWidth):
		text = str(text) if text else ""
		holder = []

		cur = ui.TextLine()
		cur.SetParent(self)
		cur.AddFlag("not_pick")
		cur.Show()

		tmp = ui.TextLine()
		tmp.Hide()

		splt = text.split(" ")
		if not splt:
			cur.SetText("")
			holder.append(cur)
			return holder

		cur.SetText(splt[0])
		for w in splt[1:]:
			tmp.SetText(" " + w)
			if tmp.GetTextSize()[0] + cur.GetTextSize()[0] > maxWidth:
				cur.AdjustSize()
				holder.append(cur)
				cur = ui.TextLine()
				cur.SetParent(self)
				cur.AddFlag("not_pick")
				cur.SetText(w)
				cur.Show()
			else:
				cur.SetText(cur.GetText() + " " + w)

		holder.append(cur)
		return holder

	def __CalcDropHeight(self, items, cols):
		# Szimulaljuk a ChestPeekWindow posMap logikajat (a nagy itemek helyet foglalnak, igy nem csusznak ossze)
		posMap = {}
		currAdd = 0
		maxY = 0

		for it in items:
			vnum = it[0]
			size = 1
			try:
				item.SelectItem(vnum)
				size = item.GetItemSize()[1]
			except:
				size = 1

			while currAdd in posMap:
				currAdd += 1

			currH = int(math.floor(currAdd / cols) * self.CELL_H)
			maxY = max(maxY, currH + self.CELL_H)

			for i in xrange(size):
				posMap[currAdd + i * cols] = True

			currAdd += 1

		return max(self.CELL_H, maxY)

	def NoticeMe(self):
		if not self.vnum:
			return

		ret = wiki.GetChestInfo(self.vnum)
		allow_lst = (len(ret) > 2)

		if not allow_lst:
			# meg nincs kesz a lista
			return

		(dwOrigin, isCommon, lst) = ret

		# --- origin text ---
		self.originText.Hide()
		self.__ClearOriginLines()

		originText = ""
		if isCommon:
			originText = localeinfo.WIKI_CHESTINFO_COMMON_DROP if hasattr(localeinfo, "WIKI_CHESTINFO_COMMON_DROP") else "Common drop"
		elif dwOrigin:
			originText = nonplayer.GetMonsterName(dwOrigin)
		else:
			originText = "-"

		if originText and originText != "-":
			self._originLines = self.__GenMultiLine(originText, max(10, self.ORIGIN_W - 12))
		else:
			self.originText.SetText("-")
			self.originText.Show()

		# --- sort items (same logic as main chest window) ---
		sizeLst = []
		ordered = []
		other = []

		for i in lst:
			try:
				if i[0] < wiki.CONTROL_ITEM_VNUM:
					other.append(i[:])
					continue
			except:
				other.append(i[:])
				continue

			try:
				item.SelectItem(i[0])
				size = item.GetItemSize()[1]
			except:
				size = 1

			lastPos = 0
			for k in xrange(len(sizeLst)):
				if sizeLst[k] < size:
					break
				lastPos += 1

			sizeLst.insert(lastPos, size)
			ordered.insert(lastPos, i[:])

		# A magassag szimulaciohoz ugyanabban a sorrendben adjuk hozza, mint ahogy kirajzoljuk
		allItemsForLayout = ordered[:] + other[:]

		# --- auto layout (no scrollbar if possible) ---
		sw = wndMgr.GetScreenWidth()
		sh = wndMgr.GetScreenHeight()

		maxW = min(sw - 60, 1600)
		maxH = sh - 80

		availDropW = max(240, maxW - (self.LEFT_W + self.ORIGIN_W + (self.PAD * 4)))
		
		# FIX: Dynamic square-like layout based on item count
		itemCount = len(allItemsForLayout)
		if itemCount == 0:
			itemCount = 1
		
		# Calculate optimal columns for square-like layout
		# sqrt gives us a good starting point for square proportions
		idealCols = int(math.ceil(math.sqrt(itemCount)))
		
		# Calculate max columns based on available width
		maxColsByWidth = int(availDropW / self.CELL_W)
		if maxColsByWidth < 4:
			maxColsByWidth = 4
		if maxColsByWidth > 30:
			maxColsByWidth = 30
		
		# Start with ideal square layout, but respect width constraints
		maxCols = min(idealCols, maxColsByWidth)
		
		# Ensure minimum columns for usability
		if maxCols < 4:
			maxCols = 4
		# Allow wider layouts for many items
		if itemCount > maxCols * maxCols:
			maxCols = min(maxColsByWidth, int(math.ceil(math.sqrt(itemCount * 1.2))))

		# origin height approx
		originH = 18
		if self._originLines:
			originH += (self._originLines[0].GetTextSize()[1] * len(self._originLines)) + (3 * (len(self._originLines) - 1))
		else:
			originH += self.originText.GetTextSize()[1]

		bestCols = maxCols
		bestDropH = self.__CalcDropHeight(allItemsForLayout, bestCols)
		bestBodyH = max(bestDropH, originH, self.ICON_MIN_H)
		bestTotalH = self.TITLE_H + bestBodyH + (self.PAD * 2)

		# probaljuk ugy valasztani a cols-t, hogy beleferjen a kepernyobe (scroll nelkul)
		# Also try to maintain square-like proportions
		for cols in xrange(maxCols, 3, -1):
			dropH = self.__CalcDropHeight(allItemsForLayout, cols)
			bodyH = max(dropH, originH, self.ICON_MIN_H)
			totalH = self.TITLE_H + bodyH + (self.PAD * 2)
			if totalH <= maxH:
				bestCols = cols
				bestDropH = dropH
				bestBodyH = bodyH
				bestTotalH = totalH
				break

		cols = bestCols
		dropH = bestDropH
		bodyH = bestBodyH

		dropW = (cols * self.CELL_W) + 4
		totalW = self.LEFT_W + dropW + self.ORIGIN_W + (self.PAD * 4)
		totalH = self.TITLE_H + bodyH + (self.PAD * 2)

		# apply size
		self.SetSize(totalW, totalH)

		# positions
		bodyY = self.TITLE_H + self.PAD
		leftX = self.PAD
		dropX = leftX + self.LEFT_W + self.PAD
		originX = dropX + dropW + self.PAD

		# icon center
		try:
			self.chestImage.SetPosition(leftX + int((self.LEFT_W - self.chestImage.GetWidth()) / 2), bodyY + int((bodyH - self.chestImage.GetHeight()) / 2))
		except:
			pass

		# loading text center
		self.loadingText.SetPosition(dropX + int(dropW / 2) - int(self.loadingText.GetTextSize()[0] / 2), bodyY + int(bodyH / 2) - int(self.loadingText.GetTextSize()[1] / 2))

		# origin header
		self.originHead.SetPosition(originX + int(self.ORIGIN_W / 2) - int(self.originHead.GetTextSize()[0] / 2), bodyY)

		# origin content
		if self._originLines:
			totalTxtH = self._originLines[0].GetTextSize()[1] * len(self._originLines) + 3 * (len(self._originLines) - 1)
			startY = bodyY + 18 + int((bodyH - 18 - totalTxtH) / 2)
			idx = 0
			for l in self._originLines:
				l.SetPosition(originX + int(self.ORIGIN_W / 2) - int(l.GetTextSize()[0] / 2), startY + idx * (l.GetTextSize()[1] + 3))
				idx += 1
		else:
			self.originText.SetPosition(originX + int(self.ORIGIN_W / 2) - int(self.originText.GetTextSize()[0] / 2), bodyY + int(bodyH / 2) - int(self.originText.GetTextSize()[1] / 2))
			self.originText.Show()

		# build drop list (no scrollbar by design: window height is calculated)
		try:
			self.dropList = ChestPeekWindow(self, dropW, dropH, False)
			self.dropList.AddFlag("attach")
			self.dropList.SetParent(self)
			self.dropList.ELEM_PER_LINE = cols
			self.dropList.SetPosition(dropX, bodyY)
			self.dropList.mOverInEvent = ui.__mem_func__(self.OnOverIn)
			self.dropList.mOverOutEvent = ui.__mem_func__(self.OnOverOut)
			self.dropList.Show()
		except:
			self.dropList = None

		# fill items
		if self.dropList:
			for i in ordered:
				try:
					cnt = (i[1] if i[1] > 1 else 0)
				except:
					cnt = 0
				self.dropList.AddItem(i[0], cnt)

			for i in other:
				try:
					self.dropList.AddItem(i[0], i[1])
				except:
					pass

			# ha megis lenne scrollbar, rejtsuk el
			try:
				if self.dropList.scrollBar:
					self.dropList.scrollBar.Hide()
			except:
				pass

		self.loadingText.Hide()
		self.CenterInParent()
