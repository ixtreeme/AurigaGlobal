import dbg
import app
import chr
import player
import item
import grp
import wndMgr
import skill
import shop
import exchange
import grpText
import safebox
import background
import nonplayer
import ui
import mousemodule
import constinfo
import localeinfo
import dbg
if app.__ENABLE_NEW_OFFLINESHOP__:
	import uiofflineshop

if app.ENABLE_ACCE_SYSTEM:
	import acce

WARP_SCROLLS = [22011, 22000, 22010]

DESC_DEFAULT_MAX_COLS = 26
DESC_WESTERN_MAX_COLS = 35
DESC_WESTERN_MAX_WIDTH = 220
CHEST_VNUMS = (
	30893, 50077, 78104, 78103, 71036, 71260, 71261, 71262, 71263, 71264,
	71265, 71266, 71267, 71268, 71269, 71270, 71271, 71272, 71273, 50037,
	71040, 71159, 71160, 30300, 38052, 38057, 78106, 50203, 78109, 78101,
	78102, 78107, 10960, 10961, 10962, 10963, 10964, 10965, 10966, 10967,
	10968, 50067, 39048, 50138, 50139, 50140, 50141, 50142, 31067, 50070,
	50078, 50071, 50076, 50079, 50080, 50073, 50081, 50069, 50254, 50285,
	50072, 50075, 50074, 50082, 50033, 50097, 50098, 50065, 50064, 50063,
	50204, 30806, 50270, 50271, 50100, 50101, 55009, 50099, 50066, 71203,
	71204, 71205, 71206, 50323, 50324, 50143, 50259, 48701, 48706, 48707,
	71057, 50011
)
if app.NEW_PET_SYSTEM:
	def pointop(n):
		t = int(n)
		if t / 10 < 1:
			return "0."+n
		else:		
			return n[0:len(n)-1]+"."+n[len(n)-1:]

def chop(n):
	return round(n - 0.5, 1)

if app.INGAME_WIKI:
	def GET_AFFECT_STRING(affType, affValue):
		if 0 == affType:
			return None
		
		try:
			affectString = ItemToolTip.AFFECT_DICT[affType]
			if type(affectString) != str:
				return affectString(affValue)

			if affectString.find("%d") != -1:
				return affectString % affValue
			else:
				return affectString
		except KeyError:
			return "UNKNOWN_TYPE[%s] %s" % (affType, affValue)

def SplitDescription(desc, limit):
	total_tokens = desc.split()
	line_tokens = []
	line_len = 0
	lines = []
	for token in total_tokens:
		if "|" in token:
			sep_pos = token.find("|")
			line_tokens.append(token[:sep_pos])

			lines.append(" ".join(line_tokens))
			line_len = len(token) - (sep_pos + 1)
			line_tokens = [token[sep_pos+1:]]
		else:
			line_len += len(token)
			if len(line_tokens) + line_len > limit:
				lines.append(" ".join(line_tokens))
				line_len = len(token)
				line_tokens = [token]
			else:
				line_tokens.append(token)

	if line_tokens:
		lines.append(" ".join(line_tokens))

	return lines

#MountVnum = {
#	52001 : 20201,
#	52002 : 20201,
#	52003 : 20201,
#	52004 : 20201,
#	52005 : 20201,
#	52006 : 20205,
#	52007 : 20205,
#	52008 : 20205,
#	52009 : 20205,
#	52010 : 20205,
#	52011 : 20209,
#	52012 : 20209,
#	52013 : 20209,
#	52014 : 20209,
#	52015 : 20209,
#	52016 : 20202,
#	52017 : 20202,
#	52018 : 20202,
#	52019 : 20202,
#	52020 : 20202,
#	52021 : 20206,
#	52022 : 20206,
#	52023 : 20206,
#	52024 : 20206,
#	52025 : 20206,
#	52026 : 20210,
#	52027 : 20210,
#	52028 : 20210,
#	52029 : 20210,
#	52030 : 20210,
#	52031 : 20204,
#	52032 : 20204,
#	52033 : 20204,
#	52034 : 20204,
#	52035 : 20204,
#	52036 : 20208,
#	52037 : 20208,
#	52038 : 20208,
#	52039 : 20208,
#	52040 : 20113,
#	52041 : 20212,
#	52042 : 20212,
#	52043 : 20212,
#	52044 : 20212,
#	52045 : 20212,
#	52046 : 20203,
#	52047 : 20203,
#	52048 : 20203,
#	52049 : 20203,
#	52050 : 20203,
#	52051 : 20207,
#	52052 : 20207,
#	52053 : 20207,
#	52054 : 20207,
#	52055 : 20207,
#	52056 : 20211,
#	52057 : 20211,
#	52058 : 20211,
#	52059 : 20211,
#	52060 : 20211,
#	52061 : 20213,
#	52062 : 20213,
#	52063 : 20213,
#	52064 : 20213,
#	52065 : 20213,
#	52066 : 20214,
#	52067 : 20214,
#	52068 : 20214,
#	52069 : 20214,
#	52070 : 20214,
#	52071 : 20215,
#	52072 : 20215,
#	52073 : 20215,
#	52074 : 20215,
#	52075 : 20215,
#	52076 : 20216,
#	52077 : 20216,
#	52078 : 20216,
#	52079 : 20216,
#	52080 : 20216,
#	52081 : 20217,
#	52082 : 20217,
#	52083 : 20217,
#	52084 : 20217,
#	52085 : 20217,
#	52086 : 20218,
#	52087 : 20218,
#	52088 : 20218,
#	52089 : 20218,
#	52090 : 20218,
#	52091 : 20223,
#	52092 : 20223,
#	52093 : 20223,
#	52094 : 20223,
#	52095 : 20223,
#	52096 : 20224,
#	52097 : 20224,
#	52098 : 20224,
#	52099 : 20224,
#	52100 : 20224,
#	52101 : 20225,
#	52102 : 20225,
#	52103 : 20225,
#	52104 : 20225,
#	52105 : 20225,
#	52107 : 20228,
#	52106 : 20228,
#	52108 : 20228,
#	52109 : 20228,
#	52110 : 20228,
#	52111 : 20229,
#	52112 : 20229,
#	52113 : 20229,
#	52114 : 20229,
#	52115 : 20229,
#	52116 : 20230,
#	52117 : 20230,
#	52118 : 20230,
#	52119 : 20230,
#	52120 : 20230,
#	71114 : 20110,
#	71116 : 20111,
#	71118 : 20112,
#	71120 : 20113,
#	71115 : 20110,
#	71117 : 20111,
#	71119 : 20112,
#	71121 : 20113,
#	71124 : 20114,
#	71125 : 20115,
#	71126 : 20116,
#	71127 : 20117,
#	71128 : 20118,
#	71131 : 20119,
#	71132 : 20119,
#	71133 : 20119,
#	71134 : 20119,
#	71137 : 20120,
#	71138 : 20121,
#	71139 : 20122,
#	71140 : 20123,
#	71141 : 20124,
#	71142 : 20125,
#	71161 : 20219,
#	71164 : 20220,
#	71165 : 20221,
#	71166 : 20222,
#	71171 : 20227,
#	71172 : 20226,
#	71176 : 20231,
#	71177 : 20232,
#	71182 : 20233,
#	71183 : 20234,
#	71184 : 20235,
#	71185 : 20236,
#	71186 : 20237,
#	71187 : 20238,
#	71192 : 20240,
#	71193 : 20239,
#	71197 : 20241,
#	71198 : 20242,
#	71220 : 20243,
#	14593 : 60008,
#	14590 : 60005,
#	14591 : 60006,
#	71221 : 20249,
#	71222 : 20250,
#	71223 : 20251,
#	71224 : 20252,
#	71225 : 20261,
#	71226 : 20262,
#	71227 : 20265,
#	71228 : 20266,
#	71250 : 20269,
#	71251 : 20270,
#	71252 : 20248,
#	71231 : 20114,
#	71254 : 20114,
#	71229 : 20252,
#	71253 : 20252,
#	71233 : 20261,
#	71255 : 20261
#	#halloween
#	, 48401 : 20243,
#	48411 : 20243,
#	48421 : 20243
#	#end hallowee
#}

PetVnum = {
	53001 : 34001,
	53003 : 34001,
	53004 : 34002,
	53005 : 34002,
	53010 : 34008,
	53011 : 34007,
	53012 : 34005,
	53013 : 34006,
	53017 : 34016,
	53025 : 34024,
	53256 : 34066,
	53242 : 34066,
	53243 : 34066,
	53244 : 34067,
	53245 : 34068,
	53246 : 34069,
	53247 : 34070,
	53248 : 34071,
	53249 : 34072,
	53250 : 34084,
	53251 : 34085,
	55701 : 34041,
	55702 : 34045,
	55703 : 34049,
	55704 : 34053,
	55705 : 34036,
	55706 : 34064,
	55707 : 34073,
	55708 : 34075,
	55709 : 34080,
	55710 : 34082,
	55711 : 34095,
	53263 : 34093,
	53264 : 34094,
	53282 : 34114,
	53283 : 34115,
	53026 : 34008,
	53027 : 34008,
	53028 : 34008,
	53029 : 34094,
	53030 : 34094,
	53031 : 34094,
	53252 : 34085,
	53253 : 34085,
	53248 : 34084,
	53290 : 34024,
	53291 : 34202,
	53292 : 34203,
	53293 : 34204,
	53294 : 34205,
	53295 : 34206,
	53296 : 34207,
	53249 : 34084
	#halloween
	, 48301 : 34100,
	48311 : 34100,
	48321 : 34100
	#end hallowee
	#patch3
	, 49010 : 34116
	#end patch3
	#patch4
	, 49050 : 34117
	#end patch4
	# , 60101 : 34118
	# , 60102 : 34118
	# , 60103 : 34119
	# , 60104 : 34119
	# , 55701 : 34041
	# , 55702 : 34045
	# , 55703 : 34049
	# , 55704 : 34053
	# , 55705 : 34036
	# , 55706 : 34064
	# , 55707 : 34073
	# , 55708 : 34075
	# , 55709 : 34080
	# , 55710 : 34082
	# , 55711 : 34095
}

###################################################################################################
## ToolTip
##
##   NOTE : ������ Item�� Skill�� �������� ���� ����������
##          ������ ������ ������ ���� ����
##
class ToolTip(ui.ThinBoard):

	TOOL_TIP_WIDTH = 190
	TOOL_TIP_HEIGHT = 10

	TEXT_LINE_HEIGHT = 17

	TITLE_COLOR = grp.GenerateColor(0.9490, 0.9058, 0.7568, 1.0)
	SPECIAL_TITLE_COLOR = grp.GenerateColor(1.0, 0.7843, 0.0, 1.0)
	NORMAL_COLOR = grp.GenerateColor(0.7607, 0.7607, 0.7607, 1.0)
	FONT_COLOR = grp.GenerateColor(0.7607, 0.7607, 0.7607, 1.0)
	PRICE_COLOR = 0xffFFB96D

	HIGH_PRICE_COLOR = SPECIAL_TITLE_COLOR
	MIDDLE_PRICE_COLOR = grp.GenerateColor(0.85, 0.85, 0.85, 1.0)
	LOW_PRICE_COLOR = grp.GenerateColor(0.7, 0.7, 0.7, 1.0)

	ENABLE_COLOR = grp.GenerateColor(0.7607, 0.7607, 0.7607, 1.0)
	DISABLE_COLOR = grp.GenerateColor(0.9, 0.4745, 0.4627, 1.0)

	NEGATIVE_COLOR = grp.GenerateColor(0.9, 0.4745, 0.4627, 1.0)#kk szine
	POSITIVE_COLOR = grp.GenerateColor(0.5411, 0.7254, 0.5568, 1.0)#1-5 opt szine
	SPECIAL_POSITIVE_COLOR = grp.GenerateColor(0.6911, 0.8754, 0.7068, 1.0)#atlagos kar +vert opt szine
	SPECIAL_POSITIVE_COLOR2 = 0xffFF34B3#6-7 opt szine
	if app.ENABLE_GAYA_SYSTEM:
		GAYA_PRICE_COLOR = grp.GenerateColor(59.87, 61.24, 42.01, 1.0)
	if app.ENABLE_CAPITALE_SYSTEM:
		PRICE_INFO_COLOR = grp.GenerateColor(1.0, 0.88, 0.0, 1.0)

	# if app.__ENABLE_NEW_EFFECT_CIANITE_WEAPON__:
		# CIANITE_COLOR_WEAPON_EPIC = 0xff0D29E2
		# CIANITE_COLOR_WEAPON_LEGG = 0xffE20D0D
		# CIANITE_COLOR_WEAPON_ANTIC = 0xff18F610
		# CIANITE_COLOR_WEAPON_MISTIC = 0xffC22FF7
		
	if app.ENABLE_BUY_WITH_ITEM:
		SHOP_ITEM_COLOR = 0xff15d131

	CONDITION_COLOR = 0xffBEB47D
	CAN_LEVEL_UP_COLOR = 0xff8EC292
	CANNOT_LEVEL_UP_COLOR = DISABLE_COLOR
	NEED_SKILL_POINT_COLOR = 0xff9A9CDB

	def __init__(self, width = TOOL_TIP_WIDTH, isPickable=False):
		ui.ThinBoard.__init__(self, "TOP_MOST")

		if isPickable:
			pass
		else:
			self.AddFlag("not_pick")

		self.AddFlag("float")

		self.followFlag = True
		self.toolTipWidth = width
		self.lastWidthIncrease = 0
		self.xPos = -1
		self.yPos = -1

		self.defFontName = localeinfo.UI_DEF_FONT
		self.ClearToolTip()

	def __del__(self):
		ui.ThinBoard.__del__(self)

	def ClearToolTip(self):
		self.toolTipHeight = 12
		self.childrenList = []
		self.childrenListStone = []

	def SetFollow(self, flag):
		self.followFlag = flag

	def SetDefaultFontName(self, fontName):
		self.defFontName = fontName

	def AppendSpace(self, size):
		self.toolTipHeight += size
		self.ResizeToolTip()

	def AppendHorizontalLine(self):

		for i in xrange(2):
			horizontalLine = ui.Line()
			horizontalLine.SetParent(self)
			horizontalLine.SetPosition(0, self.toolTipHeight + 3 + i)
			horizontalLine.SetWindowHorizontalAlignCenter()
			horizontalLine.SetSize(150, 0)
			horizontalLine.Show()

			if 0 == i:
				horizontalLine.SetColor(0xff555555)
			else:
				horizontalLine.SetColor(0xff000000)

			self.childrenList.append(horizontalLine)

		self.toolTipHeight += 11
		self.ResizeToolTip()

	def AlignHorizonalCenter(self):
		for child in self.childrenList:
			(x, y)=child.GetLocalPosition()
			child.SetPosition(self.toolTipWidth/2, y)

		self.ResizeToolTip()


	def AutoAppendNewTextLineResize(self, text, color = FONT_COLOR, centerAlign = True):
		textLine = ui.TextLine()
		textLine.SetParent(self)
		textLine.SetFontName(self.defFontName)
		textLine.SetPackedFontColor(color)
		textLine.SetText(text)
		textLine.SetOutline()
		textLine.SetFeather(False)
		textLine.Show()
 
		(textWidth, textHeight) = textLine.GetTextSize()
		textWidth += 30
		textHeight += 10
		if self.toolTipWidth < textWidth:
			self.toolTipWidth = textWidth
 
		if centerAlign:
			textLine.SetPosition(self.toolTipWidth/2, self.toolTipHeight)
			textLine.SetHorizontalAlignCenter()
		else:
			textLine.SetPosition(10, self.toolTipHeight)
 
		self.childrenList.append(textLine)
 
		self.toolTipHeight += self.TEXT_LINE_HEIGHT
		self.AlignHorizonalCenter()
		return textLine

	def AutoAppendTextLine(self, text, color = FONT_COLOR, centerAlign = True, extraWidth = 0):
		textLine = ui.TextLine()
		textLine.SetParent(self)
		textLine.SetFontName(self.defFontName)
		textLine.SetPackedFontColor(color)
		textLine.SetText(text)
		textLine.SetOutline()
		textLine.SetFeather(False)
		textLine.Show()
		if extraWidth:
			self.toolTipWidth += extraWidth
			self.ResizeToolTip()
		
		if centerAlign:
			textLine.SetPosition(self.toolTipWidth/2, self.toolTipHeight)
			textLine.SetHorizontalAlignCenter()

		else:
			textLine.SetPosition(10, self.toolTipHeight)

		self.childrenList.append(textLine)

		(textWidth, textHeight)=textLine.GetTextSize()

		textWidth += 40
		textHeight += 5

		if self.toolTipWidth < textWidth:
			self.toolTipWidth = textWidth

		self.toolTipHeight += textHeight

		return textLine
		
	def AutoAppendNewTextLine(self, text, color = FONT_COLOR, centerAlign = True):
		textLine = ui.TextLine()
		textLine.SetParent(self)
		textLine.SetFontName(self.defFontName)
		textLine.SetPackedFontColor(color)
		textLine.SetText(text)
		textLine.SetOutline()
		textLine.SetFeather(False)
		textLine.Show()
		textLine.SetPosition(15, self.toolTipHeight)

		self.childrenList.append(textLine)
		(textWidth, textHeight) = textLine.GetTextSize()

		textWidth += 62
		textHeight += 10

		if self.toolTipWidth < textWidth:
			self.toolTipWidth = textWidth

		self.toolTipHeight += textHeight
		self.ResizeToolTipText(textWidth, self.toolTipHeight)

		return textLine

	def AppendNewTextLineForLogin(self, text, color = FONT_COLOR):
		textLine = ui.TextLine()
		textLine.SetParent(self)
		textLine.SetFontName(self.defFontName)
		textLine.SetPackedFontColor(color)
		textLine.SetText(text)
		textLine.SetOutline()
		textLine.SetFeather(False)
		textLine.Show()
		textLine.SetPosition(28, 10)

		self.childrenList.append(textLine)
		(textWidth, textHeight) = textLine.GetTextSize()

		textWidth += 62
		textHeight += 10

		if self.toolTipWidth < textWidth:
			self.toolTipWidth = textWidth

		self.toolTipHeight += textHeight
		self.ResizeToolTipText(textWidth, self.toolTipHeight)
		return textLine

	def SetThinBoardSize(self, width, height = 12) :
		self.toolTipWidth = width 
		self.toolTipHeight = height

	def AppendTextLine(self, text, color = FONT_COLOR, centerAlign = True, height = 0):
		textLine = ui.TextLine()
		textLine.SetParent(self)
		textLine.SetFontName(self.defFontName)
		textLine.SetPackedFontColor(color)
		textLine.SetText(text)
		textLine.SetOutline()
		textLine.SetFeather(False)
		textLine.Show()

		if centerAlign:
			if height != 0:
				textLine.SetPosition(self.toolTipWidth/2, self.toolTipHeight + (height / 2))
			else:
				textLine.SetPosition(self.toolTipWidth/2, self.toolTipHeight)
			textLine.SetHorizontalAlignCenter()
		else:
			textLine.SetPosition(10, self.toolTipHeight)

		self.childrenList.append(textLine)
		
		if height == 0:
			height = self.TEXT_LINE_HEIGHT
		
		if len(text) > 32:
			newWidth = (len(text) - 32) * 4
			if self.lastWidthIncrease < newWidth:
				self.toolTipWidth -= self.lastWidthIncrease
				self.toolTipWidth += newWidth
				self.lastWidthIncrease = newWidth
				j = 0
				for item in self.childrenList:
					found = False
					for i in self.childrenListStone:
						if i == j:
							found = True
							break
					
					j += 1
					if found == False:
						(x, y) = item.GetLocalPosition()
						item.SetPosition(self.toolTipWidth / 2, y)
		
		self.toolTipHeight += height
		self.ResizeToolTip()
		return textLine

	def AppendDescription(self, desc, limit, color = FONT_COLOR):
		if localeinfo.IsEUROPE():
			self.__AppendDescription_WesternLanguage(desc, color)
		else:
			self.__AppendDescription_EasternLanguage(desc, limit, color)

	def __AppendDescription_EasternLanguage(self, description, characterLimitation, color=FONT_COLOR):
		length = len(description)
		if 0 == length:
			return

		lineCount = grpText.GetSplitingTextLineCount(description, characterLimitation)
		for i in xrange(lineCount):
			if 0 == i:
				self.AppendSpace(5)
			self.AppendTextLine(grpText.GetSplitingTextLine(description, characterLimitation, i), color)

	def __AppendDescription_WesternLanguage(self, desc, color=FONT_COLOR):
		lines = SplitDescription(desc, DESC_WESTERN_MAX_COLS)
		if not lines:
			return
		
		self.AppendSpace(5)
		for line in lines:
			self.AppendTextLine(line, color)

	def AppendDescriptionEqual(self, desc, color=FONT_COLOR):
		lines = SplitDescription(desc, DESC_WESTERN_MAX_COLS)
		if not lines:
			return
		
		for line in lines:
			self.AppendTextLine(line, color)

	def ResizeToolTip(self):
		self.SetSize(self.toolTipWidth, self.TOOL_TIP_HEIGHT + self.toolTipHeight)
		
	def ResizeToolTipText(self, x, y):
		self.SetSize(x, y)

	def SetTitle(self, name):
		self.AppendTextLine(name, self.TITLE_COLOR)

	def GetLimitTextLineColor(self, curValue, limitValue):
		if curValue < limitValue:
			return self.DISABLE_COLOR

		return self.ENABLE_COLOR

	def GetChangeTextLineColor(self, value, isSpecial=False):
		if value > 0:
			if isSpecial:
				return self.SPECIAL_POSITIVE_COLOR
			else:
				return self.POSITIVE_COLOR

		if 0 == value:
			return self.NORMAL_COLOR

		return self.NEGATIVE_COLOR

	def SetToolTipPosition(self, x = -1, y = -1):
		self.xPos = x
		self.yPos = y

	def ShowToolTip(self):
		self.SetTop()
		self.Show()

		self.OnUpdate()

	def HideToolTip(self):
		self.Hide()

	def OnUpdate(self):

		if not self.followFlag:
			return

		x = 0
		y = 0
		width = self.GetWidth()
		height = self.toolTipHeight

		if -1 == self.xPos and -1 == self.yPos:

			(mouseX, mouseY) = wndMgr.GetMousePosition()

			if mouseY < wndMgr.GetScreenHeight() - 300:
				y = mouseY + 40
			else:
				y = mouseY - height - 30

			x = mouseX - width/2

		else:

			x = self.xPos - width/2
			y = self.yPos - height

		x = max(x, 0)
		y = max(y, 0)
		x = min(x + width/2, wndMgr.GetScreenWidth() - width/2) - width/2
		y = min(y + self.GetHeight(), wndMgr.GetScreenHeight()) - self.GetHeight()

		parentWindow = self.GetParentProxy()
		if parentWindow:
			(gx, gy) = parentWindow.GetGlobalPosition()
			x -= gx
			y -= gy

		self.SetPosition(x, y)

class ItemToolTip(ToolTip):
	if app.ENABLE_SEND_TARGET_INFO:
		isStone = False
		isBook = False
		isBook2 = False

	#ModelPreviewBoard = None
	#ModelPreview = None
	#ModelPreviewText = None

	CHARACTER_NAMES = (
		localeinfo.TOOLTIP_WARRIOR,
		localeinfo.TOOLTIP_ASSASSIN,
		localeinfo.TOOLTIP_SURA,
		localeinfo.TOOLTIP_SHAMAN
	)
	if app.ENABLE_WOLFMAN_CHARACTER:
		CHARACTER_NAMES += (
			localeinfo.TOOLTIP_WOLFMAN,
		)

	CHARACTER_COUNT = len(CHARACTER_NAMES)
	WEAR_NAMES = (
		localeinfo.TOOLTIP_ARMOR,
		localeinfo.TOOLTIP_HELMET,
		localeinfo.TOOLTIP_SHOES,
		localeinfo.TOOLTIP_WRISTLET,
		localeinfo.TOOLTIP_WEAPON,
		localeinfo.TOOLTIP_NECK,
		localeinfo.TOOLTIP_EAR,
		localeinfo.TOOLTIP_UNIQUE,
		localeinfo.TOOLTIP_SHIELD,
		localeinfo.TOOLTIP_ARROW,
	)
	WEAR_COUNT = len(WEAR_NAMES)

	# -----------------------------------------------------------------
	# Max bonus highlight
	# Color: 0xAARRGGBB (ARGB). Example dark green: 0xff006400
	# You can pick a color with any "RGB color picker" and use:
	#   0xffRRGGBB  (alpha fixed to ff)
	# -----------------------------------------------------------------
	MAX_BONUS_HIGHLIGHT_ENABLED = True
	MAX_BONUS_HIGHLIGHT_COLOR   = 0xffffd200  # dark green (general max highlight)
	MAX_BONUS_HIGHLIGHT_AFFECT_LIST = False  # base item applies (proto)

	# Shoulder sash (vállszalag) special highlight:
	# Only applies when self.itemVnum is in VALLSZALAG_VNUMS (defined below).
	VALLSZALAG_SPECIAL_HIGHLIGHT_ENABLED = True
	VALLSZALAG_SPECIAL_HIGHLIGHT_COLOR   = 0xffffef0d  # yellow

	VALLSZALAG_SPECIAL_THRESHOLDS = {
		item.APPLY_ATTBONUS_MONSTER: 20,
		item.APPLY_ATTBONUS_METIN: 20,
		item.APPLY_ATTBONUS_BOSS: 20,
		item.APPLY_ATTBONUS_HUMAN: 15,
	}


	# Max values (generated from item_attr.sql) - feel free to edit
	# Default (weapons/armors/jewels/etc)
	AFFECT_MAX_NAME_DICT_DEFAULT = {
		'ATTBONUS_ANIMAL': 20,
		'ATTBONUS_BOSS': 15,
		'ATTBONUS_DESERT': 20,
		'ATTBONUS_DEVIL': 20,
		'ATTBONUS_FIRE': 20,
		'ATTBONUS_HUMAN': 10,
		'ATTBONUS_ICE': 20,
		'ATTBONUS_INSECT': 20,
		'ATTBONUS_IRR_CAMPANA': 5,
		'ATTBONUS_IRR_FRECCIA': 5,
		'ATTBONUS_IRR_PUGNALE': 5,
		'ATTBONUS_IRR_SPADA': 5,
		'ATTBONUS_IRR_SPADONE': 5,
		'ATTBONUS_IRR_VENTAGLIO': 5,
		'ATTBONUS_METIN': 15,
		'ATTBONUS_MILGYO': 20,
		'ATTBONUS_ORC': 20,
		'ATTBONUS_RUNE': 15,
		'ATTBONUS_UNDEAD': 20,
		'ATT_BONUS_TO_MONSTER': 15,
		'ATT_GRADE_BONUS': 50,
		'ATT_SPEED': 10,
		'BLOCK': 10,
		'CAST_SPEED': 20,
		'CON': 12,
		'CRITICAL_PCT': 12,
		'DEF_TALISMAN': 5,
		'DEX': 12,
		'DODGE': 10,
		'EXP_DOUBLE_BONUS': 20,
		'GOLD_DOUBLE_BONUS': 20,
		'HP_REGEN': 25,
		'IMMUNE_SLOW': 1,
		'IMMUNE_STUN': 1,
		'INT': 12,
		'ITEM_DROP_BONUS': 20,
		'MANA_BURN_PCT': 10,
		'MAX_HP': 2500,
		'MAX_SP': 220,
		'MOV_SPEED': 10,
		'PENETRATE_PCT': 12,
		'POISON_PCT': 10,
		'POISON_REDUCE': 10,
		'REFLECT_MELEE': 10,
		'RESIST_BELL': 15,
		'RESIST_BOW': 15,
		'RESIST_DAGGER': 15,
		'RESIST_DARK': 15,
		'RESIST_EARTH': 15,
		'RESIST_ELEC': 15,
		'RESIST_FAN': 15,
		'RESIST_FIRE': 15,
		'RESIST_ICE': 15,
		'RESIST_MAGIC': 15,
		'RESIST_MEZZIUOMINI': 10,
		'RESIST_SWORD': 15,
		'RESIST_TWOHAND': 15,
		'RESIST_WIND': 15,
		'SLOW_PCT': 8,
		'SP_REGEN': 25,
		'STEAL_HP': 10,
		'STEAL_SP': 10,
		'STR': 12,
		'STUN_PCT': 8,
	}

	# Costumes (values are different from normal equipment)
	# - ANY: fallback if subtype-specific dict doesn't contain the apply
	AFFECT_MAX_NAME_DICT_COSTUME_ANY = {
		'ATTBONUS_ANIMAL': 15,
		'ATTBONUS_DEVIL': 15,
		'ATTBONUS_HUMAN': 10,
		'ATTBONUS_ORC': 15,
		'ATTBONUS_UNDEAD': 15,
		'ATT_GRADE_BONUS': 50,
		'ATT_SPEED': 8,
		'CON': 10,
		'CRITICAL_PCT': 8,
		'DEX': 10,
		'EXP_DOUBLE_BONUS': 15,
		'GOLD_DOUBLE_BONUS': 15,
		'HP_REGEN': 15,
		'INT': 10,
		'ITEM_DROP_BONUS': 15,
		'MAX_HP': 2000,
		'MAX_SP': 800,
		'PENETRATE_PCT': 8,
		'RESIST_MEZZIUOMINI': 10,
		'SP_REGEN': 15,
		'STEAL_HP': 8,
		'STEAL_SP': 8,
		'STR': 10,
	}

	AFFECT_MAX_NAME_DICT_COSTUME_BODY = {
		'ATT_GRADE_BONUS': 50,
		'EXP_DOUBLE_BONUS': 15,
		'ITEM_DROP_BONUS': 15,
		'MAX_HP': 2000,
		'MAX_SP': 800,
		'STEAL_HP': 8,
		'STEAL_SP': 8,
	}

	AFFECT_MAX_NAME_DICT_COSTUME_HAIR = {
		'ATT_SPEED': 8,
		'CRITICAL_PCT': 8,
		'GOLD_DOUBLE_BONUS': 15,
		'HP_REGEN': 15,
		'PENETRATE_PCT': 8,
		'RESIST_MEZZIUOMINI': 10,
		'SP_REGEN': 15,
	}

	AFFECT_MAX_NAME_DICT_COSTUME_WEAPON = {
		'ATTBONUS_ANIMAL': 15,
		'ATTBONUS_DEVIL': 15,
		'ATTBONUS_HUMAN': 10,
		'ATTBONUS_ORC': 15,
		'ATTBONUS_UNDEAD': 15,
		'CON': 10,
		'DEX': 10,
		'INT': 10,
		'STR': 10,
	}

	AFFECT_MAX_NAME_DICT_COSTUME_STOLE = {
		'ATTBONUS_ANIMAL': 15,
		'ATTBONUS_DEVIL': 15,
		'ATTBONUS_ORC': 15,
		'ATTBONUS_UNDEAD': 15,
		'ATT_GRADE_BONUS': 50,
	}

	# ------------------------------------------------------------
	# Vállszalag jelmez (special vnum-based maxes)
	# Ezeknél az itemeknél más max bónuszok vannak, ezért vnum alapján felülírjuk.
	# Bővítés: csak add hozzá a vnumot a VALLSZALAG_VNUMS tuple-hez.
	# ------------------------------------------------------------
	VALLSZALAG_VNUMS = (
						85019,
						85020,
						85021,
						85022,
						85023,
						85024,
						85025,
						85026,
						85027,
						85028,
						85029,
						85030,
						85031,
						85032,
						85033,
						85034,
						85035,
						85036,
						85037,
						85038,
						85039,
						85040,
						85041,
						85042,
						85043,
						85044,
						85045,
						85046,
						85047,
						85048,
						85049,
						85050,
						85051,
						85052,
						85053,
						85054,
						85055,
						85056,
						85057,
						85058,
						85059,
						85060,
						85061,
						85062,
						85063,
						85064,
						85065,
						85066,
						85067,
						85068,
						85069,
						85070,
						85083
						#halloween
						, 85088,
						85092,
						85096,
						85100
						#end halloween
						#patch3
						, 85104,
						85108
						#end patch3
						#patch4
						, 85112
						, 85116
						, 85120
						#end patch4
						#christmas
						, 85124
						, 85132
						, 85140
						, 85148
    )
	AFFECT_MAX_NAME_DICT_VALLSZALAG = {
		'ATTBONUS_MONSTER': 20,
		'ATT_BONUS_TO_MONSTER': 20,
		'ATTBONUS_METIN': 20,
		'ATTBONUS_BOSS': 20,
		'ATTBONUS_HUMAN': 15,
		'RESIST_MEZZIUOMINI': 15,
		'MAX_HP': 3000,
	}

	def _BuildAffectMaxDict(_src):
		_d = {}
		for _applyName, _maxVal in _src.items():
			_constName = 'APPLY_%s' % _applyName
			if hasattr(item, _constName):
				_d[getattr(item, _constName)] = _maxVal
			elif hasattr(player, _constName):
				_d[getattr(player, _constName)] = _maxVal
		return _d

	AFFECT_MAX_DICT_DEFAULT = _BuildAffectMaxDict(AFFECT_MAX_NAME_DICT_DEFAULT)
	AFFECT_MAX_DICT_COSTUME_ANY = _BuildAffectMaxDict(AFFECT_MAX_NAME_DICT_COSTUME_ANY)
	AFFECT_MAX_DICT_COSTUME_BODY = _BuildAffectMaxDict(AFFECT_MAX_NAME_DICT_COSTUME_BODY)
	AFFECT_MAX_DICT_COSTUME_HAIR = _BuildAffectMaxDict(AFFECT_MAX_NAME_DICT_COSTUME_HAIR)
	AFFECT_MAX_DICT_COSTUME_WEAPON = _BuildAffectMaxDict(AFFECT_MAX_NAME_DICT_COSTUME_WEAPON)
	AFFECT_MAX_DICT_COSTUME_STOLE = _BuildAffectMaxDict(AFFECT_MAX_NAME_DICT_COSTUME_STOLE)
	AFFECT_MAX_DICT_VALLSZALAG = _BuildAffectMaxDict(AFFECT_MAX_NAME_DICT_VALLSZALAG)
	AFFECT_MAX_VNUM_OVERRIDES = {}
	for _v in VALLSZALAG_VNUMS:
		AFFECT_MAX_VNUM_OVERRIDES[_v] = AFFECT_MAX_DICT_VALLSZALAG

	del _BuildAffectMaxDict

	AFFECT_DICT = {
		item.APPLY_MAX_HP : localeinfo.TOOLTIP_MAX_HP,
		item.APPLY_MAX_SP : localeinfo.TOOLTIP_MAX_SP,
		item.APPLY_CON : localeinfo.TOOLTIP_CON,
		item.APPLY_INT : localeinfo.TOOLTIP_INT,
		item.APPLY_STR : localeinfo.TOOLTIP_STR,
		item.APPLY_DEX : localeinfo.TOOLTIP_DEX,
		item.APPLY_ATT_SPEED : localeinfo.TOOLTIP_ATT_SPEED,
		item.APPLY_MOV_SPEED : localeinfo.TOOLTIP_MOV_SPEED,
		item.APPLY_CAST_SPEED : localeinfo.TOOLTIP_CAST_SPEED,
		item.APPLY_HP_REGEN : localeinfo.TOOLTIP_HP_REGEN,
		item.APPLY_SP_REGEN : localeinfo.TOOLTIP_SP_REGEN,
		item.APPLY_POISON_PCT : localeinfo.TOOLTIP_APPLY_POISON_PCT,
		item.APPLY_STUN_PCT : localeinfo.TOOLTIP_APPLY_STUN_PCT,
		item.APPLY_SLOW_PCT : localeinfo.TOOLTIP_APPLY_SLOW_PCT,
		item.APPLY_CRITICAL_PCT : localeinfo.TOOLTIP_APPLY_CRITICAL_PCT,
		item.APPLY_PENETRATE_PCT : localeinfo.TOOLTIP_APPLY_PENETRATE_PCT,

		item.APPLY_ATTBONUS_WARRIOR : localeinfo.TOOLTIP_APPLY_ATTBONUS_WARRIOR,
		item.APPLY_ATTBONUS_ASSASSIN : localeinfo.TOOLTIP_APPLY_ATTBONUS_ASSASSIN,
		item.APPLY_ATTBONUS_SURA : localeinfo.TOOLTIP_APPLY_ATTBONUS_SURA,
		item.APPLY_ATTBONUS_SHAMAN : localeinfo.TOOLTIP_APPLY_ATTBONUS_SHAMAN,
		item.APPLY_ATTBONUS_MONSTER : localeinfo.TOOLTIP_APPLY_ATTBONUS_MONSTER,

		item.APPLY_ATTBONUS_HUMAN : localeinfo.TOOLTIP_APPLY_ATTBONUS_HUMAN,
		item.APPLY_ATTBONUS_ANIMAL : localeinfo.TOOLTIP_APPLY_ATTBONUS_ANIMAL,
		item.APPLY_ATTBONUS_ORC : localeinfo.TOOLTIP_APPLY_ATTBONUS_ORC,
		item.APPLY_ATTBONUS_MILGYO : localeinfo.TOOLTIP_APPLY_ATTBONUS_MILGYO,
		item.APPLY_ATTBONUS_UNDEAD : localeinfo.TOOLTIP_APPLY_ATTBONUS_UNDEAD,
		item.APPLY_ATTBONUS_DEVIL : localeinfo.TOOLTIP_APPLY_ATTBONUS_DEVIL,
		item.APPLY_STEAL_HP : localeinfo.TOOLTIP_APPLY_STEAL_HP,
		item.APPLY_STEAL_SP : localeinfo.TOOLTIP_APPLY_STEAL_SP,
		item.APPLY_MANA_BURN_PCT : localeinfo.TOOLTIP_APPLY_MANA_BURN_PCT,
		item.APPLY_DAMAGE_SP_RECOVER : localeinfo.TOOLTIP_APPLY_DAMAGE_SP_RECOVER,
		item.APPLY_BLOCK : localeinfo.TOOLTIP_APPLY_BLOCK,
		item.APPLY_DODGE : localeinfo.TOOLTIP_APPLY_DODGE,
		item.APPLY_RESIST_SWORD : localeinfo.TOOLTIP_APPLY_RESIST_SWORD,
		item.APPLY_RESIST_TWOHAND : localeinfo.TOOLTIP_APPLY_RESIST_TWOHAND,
		item.APPLY_RESIST_DAGGER : localeinfo.TOOLTIP_APPLY_RESIST_DAGGER,
		item.APPLY_RESIST_BELL : localeinfo.TOOLTIP_APPLY_RESIST_BELL,
		item.APPLY_RESIST_FAN : localeinfo.TOOLTIP_APPLY_RESIST_FAN,
		item.APPLY_RESIST_BOW : localeinfo.TOOLTIP_RESIST_BOW,
		item.APPLY_RESIST_FIRE : localeinfo.TOOLTIP_RESIST_FIRE,
		item.APPLY_RESIST_ELEC : localeinfo.TOOLTIP_RESIST_ELEC,
		item.APPLY_RESIST_MAGIC : localeinfo.TOOLTIP_RESIST_MAGIC,
		item.APPLY_RESIST_WIND : localeinfo.TOOLTIP_APPLY_RESIST_WIND,
		item.APPLY_REFLECT_MELEE : localeinfo.TOOLTIP_APPLY_REFLECT_MELEE,
		item.APPLY_REFLECT_CURSE : localeinfo.TOOLTIP_APPLY_REFLECT_CURSE,
		item.APPLY_POISON_REDUCE : localeinfo.TOOLTIP_APPLY_POISON_REDUCE,
		item.APPLY_KILL_SP_RECOVER : localeinfo.TOOLTIP_APPLY_KILL_SP_RECOVER,
		item.APPLY_EXP_DOUBLE_BONUS : localeinfo.TOOLTIP_APPLY_EXP_DOUBLE_BONUS,
		item.APPLY_GOLD_DOUBLE_BONUS : localeinfo.TOOLTIP_APPLY_GOLD_DOUBLE_BONUS,
		item.APPLY_ITEM_DROP_BONUS : localeinfo.TOOLTIP_APPLY_ITEM_DROP_BONUS,
		item.APPLY_POTION_BONUS : localeinfo.TOOLTIP_APPLY_POTION_BONUS,
		item.APPLY_KILL_HP_RECOVER : localeinfo.TOOLTIP_APPLY_KILL_HP_RECOVER,
		item.APPLY_IMMUNE_STUN : localeinfo.TOOLTIP_APPLY_IMMUNE_STUN,
		item.APPLY_IMMUNE_SLOW : localeinfo.TOOLTIP_APPLY_IMMUNE_SLOW,
		item.APPLY_IMMUNE_FALL : localeinfo.TOOLTIP_APPLY_IMMUNE_FALL,
		item.APPLY_BOW_DISTANCE : localeinfo.TOOLTIP_BOW_DISTANCE,
		item.APPLY_DEF_GRADE_BONUS : localeinfo.TOOLTIP_DEF_GRADE,
		item.APPLY_ATT_GRADE_BONUS : localeinfo.TOOLTIP_ATT_GRADE,
		item.APPLY_MAGIC_ATT_GRADE : localeinfo.TOOLTIP_MAGIC_ATT_GRADE,
		item.APPLY_MAGIC_DEF_GRADE : localeinfo.TOOLTIP_MAGIC_DEF_GRADE,
		item.APPLY_MAX_STAMINA : localeinfo.TOOLTIP_MAX_STAMINA,
		item.APPLY_MALL_ATTBONUS : localeinfo.TOOLTIP_MALL_ATTBONUS,
		item.APPLY_MALL_DEFBONUS : localeinfo.TOOLTIP_MALL_DEFBONUS,
		item.APPLY_MALL_EXPBONUS : localeinfo.TOOLTIP_MALL_EXPBONUS,
		item.APPLY_MALL_ITEMBONUS : localeinfo.TOOLTIP_MALL_ITEMBONUS,
		item.APPLY_MALL_GOLDBONUS : localeinfo.TOOLTIP_MALL_GOLDBONUS,
		item.APPLY_SKILL_DAMAGE_BONUS : localeinfo.TOOLTIP_SKILL_DAMAGE_BONUS,
		item.APPLY_NORMAL_HIT_DAMAGE_BONUS : localeinfo.TOOLTIP_NORMAL_HIT_DAMAGE_BONUS,
		item.APPLY_SKILL_DEFEND_BONUS : localeinfo.TOOLTIP_SKILL_DEFEND_BONUS,
		item.APPLY_NORMAL_HIT_DEFEND_BONUS : localeinfo.TOOLTIP_NORMAL_HIT_DEFEND_BONUS,
		item.APPLY_PC_BANG_EXP_BONUS : localeinfo.TOOLTIP_MALL_EXPBONUS_P_STATIC,
		item.APPLY_PC_BANG_DROP_BONUS : localeinfo.TOOLTIP_MALL_ITEMBONUS_P_STATIC,
		item.APPLY_RESIST_WARRIOR : localeinfo.TOOLTIP_APPLY_RESIST_WARRIOR,
		item.APPLY_RESIST_ASSASSIN : localeinfo.TOOLTIP_APPLY_RESIST_ASSASSIN,
		item.APPLY_RESIST_SURA : localeinfo.TOOLTIP_APPLY_RESIST_SURA,
		item.APPLY_RESIST_SHAMAN : localeinfo.TOOLTIP_APPLY_RESIST_SHAMAN,
		item.APPLY_MAX_HP_PCT : localeinfo.TOOLTIP_APPLY_MAX_HP_PCT,
		item.APPLY_MAX_SP_PCT : localeinfo.TOOLTIP_APPLY_MAX_SP_PCT,
		item.APPLY_ENERGY : localeinfo.TOOLTIP_ENERGY,
		item.APPLY_COSTUME_ATTR_BONUS : localeinfo.TOOLTIP_COSTUME_ATTR_BONUS,

		item.APPLY_MAGIC_ATTBONUS_PER : localeinfo.TOOLTIP_MAGIC_ATTBONUS_PER,
		item.APPLY_MELEE_MAGIC_ATTBONUS_PER : localeinfo.TOOLTIP_MELEE_MAGIC_ATTBONUS_PER,
		item.APPLY_RESIST_ICE : localeinfo.TOOLTIP_RESIST_ICE,
		item.APPLY_RESIST_EARTH : localeinfo.TOOLTIP_RESIST_EARTH,
		item.APPLY_RESIST_DARK : localeinfo.TOOLTIP_RESIST_DARK,
		item.APPLY_ANTI_CRITICAL_PCT : localeinfo.TOOLTIP_ANTI_CRITICAL_PCT,
		item.APPLY_ANTI_PENETRATE_PCT : localeinfo.TOOLTIP_ANTI_PENETRATE_PCT,
		item.APPLY_DESIGN_1 : localeinfo.TOOLTIP_APPLY_DESIGN_1,
		item.APPLY_FISHING_RARE : localeinfo.TOOLTIP_APPLY_FISHING_RARE,


	}


	if app.ENABLE_WOLFMAN_CHARACTER:
		AFFECT_DICT.update({
			item.APPLY_BLEEDING_PCT : localeinfo.TOOLTIP_APPLY_BLEEDING_PCT,
			item.APPLY_BLEEDING_REDUCE : localeinfo.TOOLTIP_APPLY_BLEEDING_REDUCE,
			item.APPLY_ATTBONUS_WOLFMAN : localeinfo.TOOLTIP_APPLY_ATTBONUS_WOLFMAN,
			item.APPLY_RESIST_CLAW : localeinfo.TOOLTIP_APPLY_RESIST_CLAW,
			item.APPLY_RESIST_WOLFMAN : localeinfo.TOOLTIP_APPLY_RESIST_WOLFMAN,
		})

	if app.ENABLE_MAGIC_REDUCTION_SYSTEM:
		AFFECT_DICT.update({
			item.APPLY_RESIST_MAGIC_REDUCTION : localeinfo.TOOLTIP_RESIST_MAGIC_REDUCTION,
		})
	
	if app.ENABLE_NEW_BONUS_TALISMAN:
		AFFECT_DICT.update({
			item.APPLY_ATTBONUS_IRR_SPADA : localeinfo.TOOLTIP_APPLY_ATTBONUS_IRR_SPADA,
			item.APPLY_ATTBONUS_IRR_SPADONE : localeinfo.TOOLTIP_APPLY_ATTBONUS_IRR_SPADONE,
			item.APPLY_ATTBONUS_IRR_PUGNALE : localeinfo.TOOLTIP_APPLY_ATTBONUS_IRR_PUGNALE,
			item.APPLY_ATTBONUS_IRR_FRECCIA : localeinfo.TOOLTIP_APPLY_ATTBONUS_IRR_FRECCIA,
			item.APPLY_ATTBONUS_IRR_VENTAGLIO : localeinfo.TOOLTIP_APPLY_ATTBONUS_IRR_VENTAGLIO,
			item.APPLY_ATTBONUS_IRR_CAMPANA : localeinfo.TOOLTIP_APPLY_ATTBONUS_IRR_CAMPANA,
			item.APPLY_RESIST_MEZZIUOMINI : localeinfo.TOOLTIP_APPLY_RESIST_MEZZIUOMINI,
			item.APPLY_DEF_TALISMAN : localeinfo.TOOLTIP_APPLY_DEF_TALISMAN,
			item.APPLY_ATTBONUS_FORT_ZODIAC	: localeinfo.TOOLTIP_APPLY_ATTBONUS_FORT_ZODIAC,
			item.APPLY_ATTBONUS_DESERT : localeinfo.TOOLTIP_APPLY_ATTBONUS_DESERT,
			item.APPLY_ATTBONUS_INSECT: localeinfo.TOOLTIP_APPLY_ATTBONUS_INSECT,
			item.APPLY_ATTBONUS_ELEC : localeinfo.TOOLTIP_APPLY_ATTBONUS_ELEC,
			item.APPLY_ATTBONUS_FIRE : localeinfo.TOOLTIP_APPLY_ATTBONUS_FIRE,
			item.APPLY_ATTBONUS_ICE : localeinfo.TOOLTIP_APPLY_ATTBONUS_ICE,
			item.APPLY_ATTBONUS_WIND : localeinfo.TOOLTIP_APPLY_ATTBONUS_WIND,
			item.APPLY_ATTBONUS_EARTH : localeinfo.TOOLTIP_APPLY_ATTBONUS_EARTH,
			item.APPLY_ATTBONUS_DARK : localeinfo.TOOLTIP_APPLY_ATTBONUS_DARK,
		})
	
	if app.ENABLE_STRONG_METIN:
		AFFECT_DICT.update({
			item.APPLY_ATTBONUS_METIN : localeinfo.TOOLTIP_APPLY_ATTBONUS_METIN,
		})
		
	if app.ENABLE_STRONG_BOSS:
		AFFECT_DICT.update({
			item.APPLY_ATTBONUS_BOSS : localeinfo.TOOLTIP_APPLY_ATTBONUS_BOSS,
		})
		
	if app.ENABLE_RESIST_MONSTER:
		AFFECT_DICT.update({
			item.APPLY_RESIST_MONSTER : localeinfo.TOOLTIP_APPLY_RESIST_MONSTER,
		})
	
	if app.ENABLE_MEDI_PVM:
		AFFECT_DICT.update({
			item.APPLY_ATTBONUS_MEDI_PVM : localeinfo.TOOLTIP_APPLY_ATTBONUS_MEDI_PVM,
		})
		
	AFFECT_DICT.update({item.APPLY_PVM_CRITICAL_PCT : localeinfo.TOOLTIP_APPLY_PVM_CRITICAL_PCT,})
	if app.ENABLE_DS_RUNE:
		AFFECT_DICT.update({item.APPLY_RUNE_MONSTERS : localeinfo.TOOLTIP_APPLY_ATTBONUS_RUNE,})
	if app.ENABLE_NEW_COMMON_BONUSES:
		AFFECT_DICT.update(
							{
								item.APPLY_DOUBLE_DROP_ITEM : localeinfo.TOOLTIP_DOUBLE_DROP_ITEM,
								item.APPLY_IRR_WEAPON_DEFENSE : localeinfo.TOOLTIP_IRR_WEAPON_DEFENSE,
							},
		)
	
	if app.ENABLE_NEW_USE_POTION:
		AFFECT_DICT.update({
			item.APPLY_PARTY_DROPEXP : localeinfo.APPLY_PARTY_DROPEXP,
		})
	
	ANTI_FLAG_DICT = {
		0 : item.ITEM_ANTIFLAG_WARRIOR,
		1 : item.ITEM_ANTIFLAG_ASSASSIN,
		2 : item.ITEM_ANTIFLAG_SURA,
		3 : item.ITEM_ANTIFLAG_SHAMAN,
	}
	if app.ENABLE_WOLFMAN_CHARACTER:
		ANTI_FLAG_DICT.update({
			4 : item.ITEM_ANTIFLAG_WOLFMAN,
		})
	
	if app.ENABLE_RARITY_SYSTEM:
		RARITY_DESC = {
			item.RARITY_COMMON : localeinfo.RARITY_ITEM_COMMON,
			item.RARITY_EPIC : localeinfo.RARITY_ITEM_EPIC,
			item.RARITY_LEGENDARY : localeinfo.RARITY_ITEM_LEGENDARY,
			item.RARITY_ANTIQUE : localeinfo.RARITY_ITEM_ANTIQUE,
			item.RARITY_MISTIC : localeinfo.RARITY_ITEM_MISTIC,
		}
		
		RARITY_COLOR = {
			item.RARITY_COMMON : grp.GenerateColor(1.0,1.0,1.0,1.0),
			item.RARITY_EPIC : grp.GenerateColor(0.1,0.1,1.0,1.0),
			item.RARITY_LEGENDARY : grp.GenerateColor(1.0, 0.1, 0.1,1.0),
			item.RARITY_ANTIQUE : grp.GenerateColor(0.5,1.0,0.1,1.0),
			item.RARITY_MISTIC : grp.GenerateColor(1.0, 0.2, 1.0, 1.0),
		}

	if app.ENABLE_NEW_EXTRA_BONUS:
		NEW_EXTRA_BONUS_COLOR = grp.GenerateColor(1.0, 0.8, 0.1, 1.0)

	FONT_COLOR = grp.GenerateColor(0.7607, 0.7607, 0.7607, 1.0)
	POINT_DICT = {item.GetApplyPoint(affect): name for affect, name in AFFECT_DICT.items()}

	def __init__(self, *args, **kwargs):
		ToolTip.__init__(self, *args, **kwargs)
		self.itemVnum = 0
		self.isShopItem = False
		self.lastWidthIncrease = 0
		# ������ ������ ������ �� ���� �������� ������ �� ���� ������������ ������ Disable Color�� ���� (���� ������ �������� ������ ���� �� ������ ������)
		self.bCannotUseItemForceSetDisableColor = True

	def __del__(self):
		ToolTip.__del__(self)

	if app.ENABLE_NEW_PET_EDITS:
		def AppendPetBonus(self, secs):
			idx = 1
			if secs >= 950400 and secs < 2246400:
				idx = 2
			
			if secs >= 2246400 and secs < 4147200:
				idx = 3
			
			if secs >= 4147200:
				idx = 4
			
			val = [[0, 500, 1200, 2100, 3000], [0, 2, 4, 7, 10], [0, 2, 4, 7, 10]]
			self.AppendTextLine(self.__GetAffectString(item.APPLY_MAX_HP, val[0][idx]))
			self.AppendTextLine(self.__GetAffectString(item.APPLY_RESIST_MONSTER, val[1][idx]))
			self.AppendTextLine(self.__GetAffectString(item.APPLY_RESIST_MEZZIUOMINI, val[2][idx]))

	def CanViewRendering(self):
		race = player.GetRace()
		job = chr.RaceToJob(race)
		if not self.ANTI_FLAG_DICT.has_key(job):
			return False

		if item.IsAntiFlag(self.ANTI_FLAG_DICT[job]):
			return False

		sex = chr.RaceToSex(race)
		
		MALE = 1
		FEMALE = 0

		if item.IsAntiFlag(item.ITEM_ANTIFLAG_MALE) and sex == MALE:
			return False

		if item.IsAntiFlag(item.ITEM_ANTIFLAG_FEMALE) and sex == FEMALE:
			return False

		return True

	def CanViewRenderingSex(self):
		race = player.GetRace()
		sex = chr.RaceToSex(race)
		
		MALE = 1
		FEMALE = 0

		if item.IsAntiFlag(item.ITEM_ANTIFLAG_MALE) and sex == MALE:
			return False

		if item.IsAntiFlag(item.ITEM_ANTIFLAG_FEMALE) and sex == FEMALE:
			return False

		return True

	def SetCannotUseItemForceSetDisableColor(self, enable):
		self.bCannotUseItemForceSetDisableColor = enable

	def CanEquip(self):
		if not item.IsEquipmentVID(self.itemVnum):
			return True

		race = player.GetRace()
		job = chr.RaceToJob(race)
		if not self.ANTI_FLAG_DICT.has_key(job):
			return False

		if item.IsAntiFlag(self.ANTI_FLAG_DICT[job]):
			return False

		sex = chr.RaceToSex(race)

		MALE = 1
		FEMALE = 0

		if item.IsAntiFlag(item.ITEM_ANTIFLAG_MALE) and sex == MALE:
			return False

		if item.IsAntiFlag(item.ITEM_ANTIFLAG_FEMALE) and sex == FEMALE:
			return False

		for i in xrange(item.LIMIT_MAX_NUM):
			(limitType, limitValue) = item.GetLimit(i)

			if item.LIMIT_LEVEL == limitType:
				if player.GetStatus(player.LEVEL) < limitValue:
					return False
			"""
			elif item.LIMIT_STR == limitType:
				if player.GetStatus(player.ST) < limitValue:
					return False
			elif item.LIMIT_DEX == limitType:
				if player.GetStatus(player.DX) < limitValue:
					return False
			elif item.LIMIT_INT == limitType:
				if player.GetStatus(player.IQ) < limitValue:
					return False
			elif item.LIMIT_CON == limitType:
				if player.GetStatus(player.HT) < limitValue:
					return False
			"""

		return True

	def AppendTextLine(self, text, color = FONT_COLOR, centerAlign = True, height = 0):
		if not self.CanEquip() and self.bCannotUseItemForceSetDisableColor:
			color = self.DISABLE_COLOR
		
		if len(text) > 32:
			newWidth = (len(text) - 32) * 4
			if self.lastWidthIncrease < newWidth:
				self.toolTipWidth -= self.lastWidthIncrease
				self.toolTipWidth += newWidth
				self.lastWidthIncrease = newWidth
				j = 0
				for item in self.childrenList:
					found = False
					for i in self.childrenListStone:
						if i == j:
							found = True
							break
					
					j += 1
					if found == False:
						(x, y) = item.GetLocalPosition()
						item.SetPosition(self.toolTipWidth / 2, y)
		
		self.ResizeToolTip()
		return ToolTip.AppendTextLine(self, text, color, centerAlign, height)

	def ClearToolTip(self):
		self.isShopItem = False
		self.lastWidthIncrease = 0
		self.toolTipWidth = self.TOOL_TIP_WIDTH
		ToolTip.ClearToolTip(self)

	if app.ENABLE_DS_SET:
		def AppendDSTextLine(self, text, color = FONT_COLOR, centerAlign = True, first = False):
			textLine = ui.TextLine()
			textLine.SetParent(self)
			textLine.SetFontName(self.defFontName)
			textLine.SetPackedFontColor(color)
			textLine.SetText(text)
			textLine.SetOutline()
			textLine.SetFeather(False)
			textLine.Show()
			if first:
				self.toolTipWidth += self.toolTipWidth
			
			if centerAlign:
				textLine.SetPosition(self.toolTipWidth/2, self.toolTipHeight)
				textLine.SetHorizontalAlignCenter()
			else:
				textLine.SetPosition(10, self.toolTipHeight)
			
			self.childrenList.append(textLine)
			self.toolTipHeight += self.TEXT_LINE_HEIGHT
			
			self.ResizeToolTip()
			return textLine

		def AppendDSSetInfo(self):
			self.AppendDSTextLine(localeinfo.DS_SET_INFO, self.FONT_COLOR, True, True)
			self.AppendDSTextLine(self.__GetAffectString(item.APPLY_ATTBONUS_METIN, 10))
			self.AppendDSTextLine(self.__GetAffectString(item.APPLY_ATTBONUS_MONSTER, 10))
			self.AppendDSTextLine(self.__GetAffectString(item.APPLY_MAX_HP, 1000))

	if app.ENABLE_ATTR_TRANSFER_SYSTEM:
		def SetInventoryItemAttrTransfer(self, slotIndex_1, slotIndex_2, window_type = player.INVENTORY):
			itemVnum = player.GetItemIndex(window_type, slotIndex_1)
			itemVnum_2 = player.GetItemIndex(window_type, slotIndex_2)
			if itemVnum == 0 or itemVnum_2 == 0:
				return
			
			self.ClearToolTip()
			if shop.IsOpen():
				if not shop.IsPrivateShop():
					item.SelectItem(itemVnum)
					self.AppendSellingPrice(player.GetISellItemPrice(window_type, slotIndex_2))
			
			metinSlot = [player.GetItemMetinSocket(window_type, slotIndex_2, i) for i in xrange(player.METIN_SOCKET_MAX_NUM)]
			attrSlot = [player.GetItemAttribute(window_type, slotIndex_2, i) for i in xrange(player.ATTRIBUTE_SLOT_MAX_NUM)]
			self.AddItemDataAttrTransfer(itemVnum, itemVnum_2, metinSlot, attrSlot, 0, 0, window_type, slotIndex_2)

		def AddItemDataAttrTransfer(self, itemVnum, itemVnum_2, metinSlot, attrSlot = 0, flags = 0, unbindTime = 0, window_type = player.INVENTORY, slotIndex = -1):
			self.itemVnum = itemVnum
			item.SelectItem(itemVnum)
			itemType = item.GetItemType()
			itemSubType = item.GetItemSubType()
			itemDesc = item.GetItemDescription()
			itemSummary = item.GetItemSummary()
			
			self.__SetItemTitle(itemVnum, metinSlot, attrSlot)
			self.AppendDescription(itemDesc, 26)
			self.AppendDescription(itemSummary, 26, self.CONDITION_COLOR)
			self.__AppendLimitInformation()
			self.__AppendAffectInformation()
			
			item.SelectItem(itemVnum_2)
			self.__AppendAttributeInformation(attrSlot, 0, -1, True)
			
			item.SelectItem(itemVnum)
			self.AppendWearableInformation()
			bHasRealtimeFlag = 0
			for i in xrange(item.LIMIT_MAX_NUM):
				(limitType, limitValue) = item.GetLimit(i)
				if item.LIMIT_REAL_TIME == limitType:
					bHasRealtimeFlag = 1
			
			if 1 == bHasRealtimeFlag:
				self.AppendMallItemLastTime(metinSlot[0])
			
			self.ShowToolTip()

	def SetInventoryItem(self, slotIndex, window_type = player.INVENTORY):
		itemVnum = player.GetItemIndex(window_type, slotIndex)
		if 0 == itemVnum:
			return

		self.ClearToolTip()
		if shop.IsOpen():
			if not shop.IsPrivateShop():
				item.SelectItem(itemVnum)
				self.AppendSellingPrice(player.GetISellItemPrice(window_type, slotIndex))

		metinSlot = [player.GetItemMetinSocket(window_type, slotIndex, i) for i in xrange(player.METIN_SOCKET_MAX_NUM)]
		attrSlot = [player.GetItemAttribute(window_type, slotIndex, i) for i in xrange(player.ATTRIBUTE_SLOT_MAX_NUM)]

		if app.ATTR_LOCK:
			lockedattr = player.GetItemAttrLocked(window_type, slotIndex)
			self.AddItemData(itemVnum, metinSlot, attrSlot, 0, 0, window_type, slotIndex, lockedattr)
		else:
			self.AddItemData(itemVnum, metinSlot, attrSlot, 1, 0, 0, window_type, slotIndex)

	def SetShopItem(self, slotIndex):
		itemVnum = shop.GetItemID(slotIndex)
		if 0 == itemVnum:
			return

		price = shop.GetItemPrice(slotIndex)
		self.ClearToolTip()
		self.isShopItem = True

		item.SelectItem(itemVnum)
		itemType = item.GetItemType()
		itemSubType = item.GetItemSubType()

		metinSlot = []
		for i in xrange(player.METIN_SOCKET_MAX_NUM):
			metinSlot.append(shop.GetItemMetinSocket(slotIndex, i))
		attrSlot = []
		for i in xrange(player.ATTRIBUTE_SLOT_MAX_NUM):
			attrSlot.append(shop.GetItemAttribute(slotIndex, i))
		if app.ATTR_LOCK:
			lockedattr = shop.GetItemAttrLocked(slotIndex)
			self.AddItemData(itemVnum, metinSlot, attrSlot, 0, 0, player.INVENTORY, -1, lockedattr, shop = True)
		else:
			self.AddItemData(itemVnum, metinSlot, attrSlot, shop = True)
		
		if app.ENABLE_BUY_WITH_ITEM:
			self.AppendSpace(5)
			self.AppendTextLine(localeinfo.ITEM_PRICE_TITLE, self.GetPriceColor(price))
			priced = False
			if price > 0:
				self.AppendPrice(price)
				priced = True
			
			for i in xrange(shop.MAX_SHOP_PRICES):
				(vnum, count) = shop.GetBuyWithItem(slotIndex, i)
				if vnum > 0 and count > 0:
					if not priced:
						priced = True
					
					self.AppendPrice_WithItem(vnum, count)
			
			if not priced:
				self.AppendTextLine(localeinfo.ITEM_IS_FREE, self.GetPriceColor(price))
		else:
			self.AppendPrice(price)
		
		if app.ENABLE_BUY_STACK_FROM_SHOP:
			self.AppendSpace(5)
			self.AppendTextLine(localeinfo.MULTIPLE_BUYS)
		#if item.ITEM_TYPE_WEAPON == itemType and itemSubType != 6:
		#	race = 0
		#	if itemSubType == item.WEAPON_SWORD:
		#		if player.GetRace() == 6 or player.GetRace() == 2:
		#			race = player.GetRace()
		#		if player.GetRace() != 3 and player.GetRace() != 7 and player.GetRace() != 8:
		#			race = player.GetRace()
		#			if item.IsAntiFlag(item.ITEM_ANTIFLAG_WARRIOR) and item.IsAntiFlag(item.ITEM_ANTIFLAG_ASSASSIN) and item.IsAntiFlag(item.ITEM_ANTIFLAG_SHAMAN) and race != 6 and race != 2:
		#				if chr.RaceToSex(player.GetRace()) == 1:
		#					race = 2
		#				else:
		#					race = 6
		#		else:
		#			if chr.RaceToSex(player.GetRace()) == 1:
		#				race = 0
		#			else:
		#				race = 4
		#			
		#			if item.IsAntiFlag(item.ITEM_ANTIFLAG_WARRIOR) and item.IsAntiFlag(item.ITEM_ANTIFLAG_ASSASSIN) and item.IsAntiFlag(item.ITEM_ANTIFLAG_SHAMAN) and race != 6 and race != 2:
		#				if chr.RaceToSex(player.GetRace()) == 1:
		#					race = 2
		#				else:
		#					race = 6
		#	elif itemSubType == item.WEAPON_TWO_HANDED:
		#		if player.GetRace() != 0 and player.GetRace() != 4:
		#			if chr.RaceToSex(player.GetRace()) == 1:
		#				race = 0
		#			else:
		#				race = 4
		#		else:
		#			race = player.GetRace()
		#	elif itemSubType == item.WEAPON_DAGGER or itemSubType == item.WEAPON_BOW:
		#		if player.GetRace() != 1 and player.GetRace() != 5:
		#			if chr.RaceToSex(player.GetRace()) == 1:
		#				race = 5
		#			else:
		#				race = 1
		#		else:
		#			race = player.GetRace()
		#	elif itemSubType == item.WEAPON_BELL or itemSubType == item.WEAPON_FAN:
		#		if player.GetRace() != 3 and player.GetRace() != 7:
		#			if chr.RaceToSex(player.GetRace()) == 1:
		#				race = 7
		#			else:
		#				race = 3
		#		else:
		#			race = player.GetRace()
		#	
		#	self.__ModelPreview(itemVnum, 3, race)
		#elif item.ITEM_TYPE_ARMOR == itemType and itemSubType == 0:
		#	self.__ModelPreview(itemVnum, 2, self.__ItemGetRace(1))
		#elif item.ITEM_TYPE_COSTUME == itemType:
		#		if itemSubType == 0: #body
		#			self.__ModelPreview(itemVnum, 2, self.__ItemGetRace(1, 1))
		#		elif itemSubType == 1: #Hair 
		#			self.__ModelPreview(item.GetValue(3), 1, self.__ItemGetRace(1, 1)) #hier in der DB Value3 prufen  
		#		elif itemSubType == 4: #weapon
		#			race = 0
		#			itemRealSubType = item.GetValue(3)
		#			if itemRealSubType == item.WEAPON_SWORD:
		#				if player.GetRace() == 6 or player.GetRace() == 2:
		#					race = player.GetRace()
		#				elif player.GetRace() != 3 and player.GetRace() != 7 and player.GetRace() != 8:
		#					race = player.GetRace()
		#					if item.IsAntiFlag(item.ITEM_ANTIFLAG_WARRIOR) and item.IsAntiFlag(item.ITEM_ANTIFLAG_ASSASSIN) and item.IsAntiFlag(item.ITEM_ANTIFLAG_SHAMAN) and race != 6 and race != 2:
		#						if chr.RaceToSex(player.GetRace()) == 1:
		#							race = 2
		#						else:
		#							race = 6
		#				else:
		#					if chr.RaceToSex(player.GetRace()) == 1:
		#						race = 0
		#					else:
		#						race = 4
		#					
		#					if item.IsAntiFlag(item.ITEM_ANTIFLAG_WARRIOR) and item.IsAntiFlag(item.ITEM_ANTIFLAG_ASSASSIN) and item.IsAntiFlag(item.ITEM_ANTIFLAG_SHAMAN) and race != 6 and race != 2:
		#						if chr.RaceToSex(player.GetRace()) == 1:
		#							race = 2
		#						else:
		#							race = 6
		#			elif itemRealSubType == item.WEAPON_TWO_HANDED:
		#				if player.GetRace() != 0 and player.GetRace() != 4:
		#					if chr.RaceToSex(player.GetRace()) == 1:
		#						race = 0
		#					else:
		#						race = 4
		#				else:
		#					race = player.GetRace()
		#			elif itemRealSubType == item.WEAPON_DAGGER or itemRealSubType == item.WEAPON_BOW:
		#				if player.GetRace() != 1 and player.GetRace() != 5:
		#					if chr.RaceToSex(player.GetRace()) == 1:
		#						race = 5
		#					else:
		#						race = 1
		#				else:
		#					race = player.GetRace()
		#			elif itemRealSubType == item.WEAPON_BELL or itemRealSubType == item.WEAPON_FAN:
		#				if player.GetRace() != 3 and player.GetRace() != 7:
		#					if chr.RaceToSex(player.GetRace()) == 1:
		#						race = 7
		#					else:
		#						race = 3
		#				else:
		#					race = player.GetRace()
		#			
		#			self.__ModelPreview(itemVnum, 3, race)
		#		elif itemSubType == 2: #Mount, 
		#			if itemVnum in MountVnum:
		#				self.__ModelPreview(itemVnum, 0, MountVnum[itemVnum])
		#		elif itemSubType == 6: #pet	
		#			self.__ModelPreview(itemVnum, 0, item.GetValue(0))

	if app.ENABLE_SEND_TARGET_INFO:
		def SetItemToolTipStone(self, itemVnum):
			self.itemVnum = itemVnum
			item.SelectItem(itemVnum)
			itemType = item.GetItemType()

			itemDesc = item.GetItemDescription()
			itemSummary = item.GetItemSummary()
			attrSlot = 0
			itemName = item.GetItemName()
			realName = itemName[:itemName.find("+")]
			self.SetTitle(realName + " +0 - +4")

			## Description ###
			self.AppendDescription(itemDesc, 26)
			self.AppendDescription(itemSummary, 26, self.CONDITION_COLOR)

			if item.ITEM_TYPE_METIN == itemType:
				self.AppendMetinInformation()
				self.AppendMetinWearInformation()

			for i in xrange(item.LIMIT_MAX_NUM):
				(limitType, limitValue) = item.GetLimit(i)

				if item.LIMIT_REAL_TIME_START_FIRST_USE == limitType:
					if gaya == 1:
						self.AppendRealTimeStartFirstUseLastTime(item, limitValue, i, True)
					else:
						self.AppendRealTimeStartFirstUseLastTime(item, metinSlot, i)

				elif item.LIMIT_TIMER_BASED_ON_WEAR == limitType:
					if gaya == 1:
						self.AppendTimerBasedOnWearLastTime(limitValue, True)
					else:
						self.AppendTimerBasedOnWearLastTime(metinSlot)

			self.ShowToolTip()
			
	if app.ENABLE_BUY_WITH_ITEM:
		def AppendPrice_WithItem(self, vnum, price):
			item.SelectItem(vnum)
			icon = item.GetIconImageFileName()
			iconImage = ui.ImageBox()
			iconImage.SetParent(self)
			iconImage.LoadImage(icon)
			height = iconImage.GetHeight()
			del iconImage
			
			#self.AppendSpace(height)
			text = localeinfo.ITEM_IS_WORTH % (price, icon, item.GetItemName())
			color = grp.GenerateColor(0.7607, 0.7607, 0.7607, 1.0)
			self.AppendTextLine(text, color, True, height)

	def SetShopItemBySecondaryCoin(self, slotIndex):
		itemVnum = shop.GetItemID(slotIndex)
		if 0 == itemVnum:
			return

		price = shop.GetItemPrice(slotIndex)
		self.ClearToolTip()
		self.isShopItem = True

		metinSlot = []
		for i in xrange(player.METIN_SOCKET_MAX_NUM):
			metinSlot.append(shop.GetItemMetinSocket(slotIndex, i))
		attrSlot = []
		for i in xrange(player.ATTRIBUTE_SLOT_MAX_NUM):
			attrSlot.append(shop.GetItemAttribute(slotIndex, i))

		if app.ATTR_LOCK:
			lockedattr = shop.GetItemAttrLocked(slotIndex)
			self.AddItemData(itemVnum, metinSlot, attrSlot, 0, 0, player.INVENTORY, -1, lockedattr)
		else:
			self.AddItemData(itemVnum, metinSlot, attrSlot, 1)
		
		self.AppendPriceBySecondaryCoin(price)
		if app.ENABLE_BUY_STACK_FROM_SHOP:
			self.AppendSpace(5)
			self.AppendTextLine(localeinfo.MULTIPLE_BUYS)

	def SetExchangeOwnerItem(self, slotIndex):
		itemVnum = exchange.GetItemVnumFromSelf(slotIndex)
		if 0 == itemVnum:
			return

		self.ClearToolTip()

		metinSlot = []
		for i in xrange(player.METIN_SOCKET_MAX_NUM):
			metinSlot.append(exchange.GetItemMetinSocketFromSelf(slotIndex, i))
		attrSlot = []
		for i in xrange(player.ATTRIBUTE_SLOT_MAX_NUM):
			attrSlot.append(exchange.GetItemAttributeFromSelf(slotIndex, i))
			
		if app.ATTR_LOCK:
			lockedattr = exchange.GetItemAttrLocked(slotIndex, True)
			self.AddItemData(itemVnum, metinSlot, attrSlot, 0, 0, player.INVENTORY, -1, lockedattr)
		else:
			self.AddItemData(itemVnum, metinSlot, attrSlot)

	def SetExchangeTargetItem(self, slotIndex):
		itemVnum = exchange.GetItemVnumFromTarget(slotIndex)
		if 0 == itemVnum:
			return

		self.ClearToolTip()

		metinSlot = []
		for i in xrange(player.METIN_SOCKET_MAX_NUM):
			metinSlot.append(exchange.GetItemMetinSocketFromTarget(slotIndex, i))
		attrSlot = []
		for i in xrange(player.ATTRIBUTE_SLOT_MAX_NUM):
			attrSlot.append(exchange.GetItemAttributeFromTarget(slotIndex, i))
		if app.ATTR_LOCK:
			lockedattr = exchange.GetItemAttrLocked(slotIndex, False)
			self.AddItemData(itemVnum, metinSlot, attrSlot, 0, 0, player.INVENTORY, -1, lockedattr)
		else:
			self.AddItemData(itemVnum, metinSlot, attrSlot)

	def SetPrivateShopBuilderItem(self, invenType, invenPos, privateShopSlotIndex):
		itemVnum = player.GetItemIndex(invenType, invenPos)
		if 0 == itemVnum:
			return

		item.SelectItem(itemVnum)
		self.ClearToolTip()
		self.AppendSellingPrice(shop.GetPrivateShopItemPrice(invenType, invenPos))

		metinSlot = []
		for i in xrange(player.METIN_SOCKET_MAX_NUM):
			metinSlot.append(player.GetItemMetinSocket(invenPos, i))
		attrSlot = []
		for i in xrange(player.ATTRIBUTE_SLOT_MAX_NUM):
			attrSlot.append(player.GetItemAttribute(invenPos, i))

		self.AddItemData(itemVnum, metinSlot, attrSlot)

	def SetSafeBoxItem(self, slotIndex):
		itemVnum = safebox.GetItemID(slotIndex)
		if 0 == itemVnum:
			return

		self.ClearToolTip()
		metinSlot = []
		for i in xrange(player.METIN_SOCKET_MAX_NUM):
			metinSlot.append(safebox.GetItemMetinSocket(slotIndex, i))
		attrSlot = []
		for i in xrange(player.ATTRIBUTE_SLOT_MAX_NUM):
			attrSlot.append(safebox.GetItemAttribute(slotIndex, i))

		if app.ATTR_LOCK:
			lockedattr = safebox.GetItemAttrLocked(slotIndex)
			self.AddItemData(itemVnum, metinSlot, attrSlot, safebox.GetItemFlags(slotIndex), 0, player.SAFEBOX, -1, lockedattr)
		else:
			self.AddItemData(itemVnum, metinSlot, attrSlot, 0, safebox.GetItemFlags(slotIndex))

	def SetMallItem(self, slotIndex):
		itemVnum = safebox.GetMallItemID(slotIndex)
		if 0 == itemVnum:
			return

		self.ClearToolTip()
		metinSlot = []
		for i in xrange(player.METIN_SOCKET_MAX_NUM):
			metinSlot.append(safebox.GetMallItemMetinSocket(slotIndex, i))
		attrSlot = []
		for i in xrange(player.ATTRIBUTE_SLOT_MAX_NUM):
			attrSlot.append(safebox.GetMallItemAttribute(slotIndex, i))
		
		if app.ATTR_LOCK:
			lockedattr = safebox.GetMallItemAttrLocked(slotIndex)
			self.AddItemData(itemVnum, metinSlot, attrSlot, 0, 0, player.MALL, -1, lockedattr)
		else:
			self.AddItemData(itemVnum, metinSlot, attrSlot)

	def SetItemToolTip(self, itemVnum):
		self.ClearToolTip()
		metinSlot = []
		for i in xrange(player.METIN_SOCKET_MAX_NUM):
			metinSlot.append(0)
		attrSlot = []
		for i in xrange(player.ATTRIBUTE_SLOT_MAX_NUM):
			attrSlot.append((0, 0))

		self.AddItemData(itemVnum, metinSlot, attrSlot)

	def __AppendAttackSpeedInfo(self, item):
		atkSpd = item.GetValue(0)

		if atkSpd < 80:
			stSpd = localeinfo.TOOLTIP_ITEM_VERY_FAST
		elif atkSpd <= 95:
			stSpd = localeinfo.TOOLTIP_ITEM_FAST
		elif atkSpd <= 105:
			stSpd = localeinfo.TOOLTIP_ITEM_NORMAL
		elif atkSpd <= 120:
			stSpd = localeinfo.TOOLTIP_ITEM_SLOW
		else:
			stSpd = localeinfo.TOOLTIP_ITEM_VERY_SLOW

		self.AppendTextLine(localeinfo.TOOLTIP_ITEM_ATT_SPEED % stSpd, self.NORMAL_COLOR)

	def __AppendAttackGradeInfo(self):
		atkGrade = item.GetValue(1)
		self.AppendTextLine(localeinfo.TOOLTIP_ITEM_ATT_GRADE % atkGrade, self.GetChangeTextLineColor(atkGrade))


	if app.ENABLE_ACCE_SYSTEM:
		# def CalcAcceValue(self, value, abs):
			# if not value:
				# return 0
			
			# valueCalc = int((round(value * abs) / 100) - 0.5) + int(int((round(value * abs) / 100) - 0.5) > 0)
			# if valueCalc <= 0 and value > 0:
				# value = 1
			# else:
				# value = valueCalc
			
			# return value
		
		#@ikd
		def CalcAcceValue(self, value, abs):
			if not value:
				return 0
			
			valueCalc 	= round(value * abs) / 100
			valueCalc 	-= 0.5
			valueCalc 	= int(valueCalc) +1 if valueCalc > 0 else int(valueCalc)
			value 		= 1 if (valueCalc <= 0 and value > 0) else valueCalc
			return value
	

	def __AppendAttackPowerInfo(self ,itemAbsChance = 0):
		minPower = item.GetValue(3)
		maxPower = item.GetValue(4)
		addPower = item.GetValue(5)
		
		if app.ENABLE_ACCE_SYSTEM:
			if itemAbsChance != 0:
				minPower = self.CalcAcceValue(minPower, itemAbsChance)
				maxPower = self.CalcAcceValue(maxPower, itemAbsChance)
				addPower = self.CalcAcceValue(addPower, itemAbsChance)
		
		
		if maxPower > minPower:
			self.AppendTextLine(localeinfo.TOOLTIP_ITEM_ATT_POWER % (minPower+addPower, maxPower+addPower), self.POSITIVE_COLOR)
		else:
			self.AppendTextLine(localeinfo.TOOLTIP_ITEM_ATT_POWER_ONE_ARG % (minPower+addPower), self.POSITIVE_COLOR)

	def __AppendMagicAttackInfo(self, itemAbsChance = 0):
		minMagicAttackPower = item.GetValue(1)
		maxMagicAttackPower = item.GetValue(2)
		addPower = item.GetValue(5)
		
		if app.ENABLE_ACCE_SYSTEM:
			if itemAbsChance != 0:
				minMagicAttackPower = self.CalcAcceValue(minMagicAttackPower, itemAbsChance)
				maxMagicAttackPower = self.CalcAcceValue(maxMagicAttackPower, itemAbsChance)
				addPower 			= self.CalcAcceValue(addPower, itemAbsChance)
		
		


		if minMagicAttackPower > 0 or maxMagicAttackPower > 0:
			if maxMagicAttackPower > minMagicAttackPower:
				self.AppendTextLine(localeinfo.TOOLTIP_ITEM_MAGIC_ATT_POWER % (minMagicAttackPower+addPower, maxMagicAttackPower+addPower), self.POSITIVE_COLOR)
			else:
				self.AppendTextLine(localeinfo.TOOLTIP_ITEM_MAGIC_ATT_POWER_ONE_ARG % (minMagicAttackPower+addPower), self.POSITIVE_COLOR)

	def __AppendMagicDefenceInfo(self, itemAbsChance = 0):
		magicDefencePower = item.GetValue(0)

		if app.ENABLE_ACCE_SYSTEM:
			if itemAbsChance != 0:
				magicDefencePower = self.CalcAcceValue(magicDefencePower, itemAbsChance)
		


		if magicDefencePower > 0:
			self.AppendTextLine(localeinfo.TOOLTIP_ITEM_MAGIC_DEF_POWER % magicDefencePower, self.GetChangeTextLineColor(magicDefencePower))

	def __AppendAttributeInformation(self, attrSlot, itemAbsChance = 0, lockedattr = -1, isTransfer = False, isCostumeStole = 0):
		if 0 != attrSlot:

			for i in xrange(player.ATTRIBUTE_SLOT_MAX_NUM):
				if isTransfer:
					if i == 5 or i == 6:
						continue
				
				type = attrSlot[i][0]
				value = attrSlot[i][1]

				if 0 == value:
					continue

				affectString = self.__GetAffectString(type, value)
				
				if app.ENABLE_ACCE_SYSTEM:
					if item.GetItemType() == item.ITEM_TYPE_COSTUME:
						if item.GetItemSubType() == item.COSTUME_TYPE_ACCE:
							if itemAbsChance != 0:
								value = self.CalcAcceValue(value, itemAbsChance)
								affectString = self.__GetAffectString(type, value)
				
				
				if affectString:
					if isCostumeStole:
						if value > 0:
							affectColor = self.SPECIAL_POSITIVE_COLOR
						elif value == 0:
							affectColor = self.NORMAL_COLOR
						else:
							affectColor = self.NEGATIVE_COLOR
					else:
						affectColor = self.__GetAttributeColor(i, value)
					
					# max bonus highlight
					if self.MAX_BONUS_HIGHLIGHT_ENABLED:
						_maxV = self.__GetMaxBonusValue(type)
						if _maxV is not None and value > 0:
							if app.ENABLE_ACCE_SYSTEM:
								if item.GetItemType() == item.ITEM_TYPE_COSTUME:
									if hasattr(item, 'COSTUME_TYPE_ACCE') and item.GetItemSubType() == item.COSTUME_TYPE_ACCE:
										if itemAbsChance != 0:
											_maxV = self.CalcAcceValue(_maxV, itemAbsChance)
							if value == _maxV:
								affectColor = self.MAX_BONUS_HIGHLIGHT_COLOR


					# vállszalag special thresholds (yellow) - ONLY for selected VNUMs
					if self.VALLSZALAG_SPECIAL_HIGHLIGHT_ENABLED and self.itemVnum in self.VALLSZALAG_VNUMS and value > 0:
						_th = self.VALLSZALAG_SPECIAL_THRESHOLDS.get(type, None)
						if _th is not None and value >= _th:
							affectColor = self.VALLSZALAG_SPECIAL_HIGHLIGHT_COLOR

					if i == lockedattr and app.ATTR_LOCK:
						affectColor = 0xff4287f5
						self.AppendTextLine("<< "+affectString+" >>", affectColor)
					else:
						self.AppendTextLine(affectString, affectColor)

	def __GetMaxBonusValue(self, affectType):
		itemType = item.GetItemType()
		itemSubType = item.GetItemSubType()
		# Vnum alapú max felülírás (vállszalag jelmezek)
		try:
			_vnum = int(getattr(self, 'itemVnum', 0))
		except:
			_vnum = 0
		if _vnum and hasattr(self, 'AFFECT_MAX_VNUM_OVERRIDES') and _vnum in self.AFFECT_MAX_VNUM_OVERRIDES:
			_vdict = self.AFFECT_MAX_VNUM_OVERRIDES[_vnum]
			if affectType in _vdict:
				return _vdict[affectType]


		if itemType == item.ITEM_TYPE_COSTUME:
			# Shoulder sash / stole costume
			if app.ENABLE_STOLE_COSTUME and hasattr(item, 'COSTUME_TYPE_STOLE') and itemSubType == item.COSTUME_TYPE_STOLE:
				return self.AFFECT_MAX_DICT_COSTUME_STOLE.get(affectType, self.AFFECT_MAX_DICT_COSTUME_ANY.get(affectType, None))

			# Weapon costume
			if hasattr(item, 'COSTUME_TYPE_WEAPON') and itemSubType == item.COSTUME_TYPE_WEAPON:
				return self.AFFECT_MAX_DICT_COSTUME_WEAPON.get(affectType, self.AFFECT_MAX_DICT_COSTUME_ANY.get(affectType, None))

			# Body / hair costumes
			if hasattr(item, 'COSTUME_TYPE_BODY') and itemSubType == item.COSTUME_TYPE_BODY:
				return self.AFFECT_MAX_DICT_COSTUME_BODY.get(affectType, self.AFFECT_MAX_DICT_COSTUME_ANY.get(affectType, None))
			if hasattr(item, 'COSTUME_TYPE_HAIR') and itemSubType == item.COSTUME_TYPE_HAIR:
				return self.AFFECT_MAX_DICT_COSTUME_HAIR.get(affectType, self.AFFECT_MAX_DICT_COSTUME_ANY.get(affectType, None))

			# Some sources treat the sash as ACCE subtype
			if app.ENABLE_ACCE_SYSTEM and hasattr(item, 'COSTUME_TYPE_ACCE') and itemSubType == item.COSTUME_TYPE_ACCE:
				return self.AFFECT_MAX_DICT_COSTUME_STOLE.get(affectType, self.AFFECT_MAX_DICT_COSTUME_ANY.get(affectType, None))

			return self.AFFECT_MAX_DICT_COSTUME_ANY.get(affectType, None)

		return self.AFFECT_MAX_DICT_DEFAULT.get(affectType, None)

	def __GetAttributeColor(self, index, value):
		if value > 0:
			if index >= player.ATTRIBUTE_SLOT_RARE_START and index < player.ATTRIBUTE_SLOT_RARE_END:
				if app.ENABLE_ATTR_COSTUMES and item.GetItemType() == item.ITEM_TYPE_COSTUME and (item.GetItemSubType() == item.COSTUME_TYPE_HAIR or item.GetItemSubType() == item.COSTUME_TYPE_BODY or item.GetItemSubType() == item.COSTUME_TYPE_WEAPON):
					return grp.GenerateColor(1.0, 0.7843, 0.0, 1.0)
				
				return self.SPECIAL_POSITIVE_COLOR2
			else:
				return self.SPECIAL_POSITIVE_COLOR
		elif value == 0:
			return self.NORMAL_COLOR
		else:
			return self.NEGATIVE_COLOR

	def __IsPolymorphItem(self, itemVnum):
		if itemVnum >= 70103 and itemVnum <= 70106:
			return 1
		return 0

	def __SetPolymorphItemTitle(self, monsterVnum):
		if localeinfo.IsVIETNAM():
			itemName =item.GetItemName()
			itemName+=" "
			itemName+=nonplayer.GetMonsterName(monsterVnum)
		else:
			itemName =nonplayer.GetMonsterName(monsterVnum)
			itemName+=" "
			itemName+=item.GetItemName()
		self.SetTitle(itemName)

	def __SetNormalItemTitle(self):
		if app.ENABLE_SEND_TARGET_INFO:
			if self.isStone:
				itemName = item.GetItemName()
				realName = itemName[:itemName.find("+")]
				self.SetTitle(realName + " +0 - +4")
			else:
				self.SetTitle(item.GetItemName())
		else:
			self.SetTitle(item.GetItemName())

	def __SetSpecialItemTitle(self):
		self.AppendTextLine(item.GetItemName(), self.SPECIAL_TITLE_COLOR)

	def __SetItemTitle(self, itemVnum, metinSlot, attrSlot):
		if localeinfo.IsCANADA():
			if 72726 == itemVnum or 72730 == itemVnum:
				self.AppendTextLine(item.GetItemName(), grp.GenerateColor(1.0, 0.7843, 0.0, 1.0))
				return
		
		if app.ENABLE_RARITY_SYSTEM:
			rarity = item.GetRarity()
			if rarity != item.RARITY_COMMON:
				self.AppendTextLine(item.GetItemName(), self.RARITY_COLOR[rarity])
				return
		
		if self.__IsPolymorphItem(itemVnum):
			self.__SetPolymorphItemTitle(metinSlot[0])
		else:
			if self.__IsAttr(attrSlot):
				self.__SetSpecialItemTitle()
				return

			self.__SetNormalItemTitle()

	def __IsAttr(self, attrSlot):
		if not attrSlot:
			return False

		for i in xrange(player.ATTRIBUTE_SLOT_MAX_NUM):
			type = attrSlot[i][0]
			if 0 != type:
				return True

		return False

	if app.ENABLE_SOUL_SYSTEM:
		def AddRefineItemData(self, itemVnum, metinSlot, attrSlot = 0, type = 0):
			for i in xrange(player.METIN_SOCKET_MAX_NUM):
				metinSlotData=metinSlot[i]
				if self.GetMetinItemIndex(metinSlotData) == constinfo.ERROR_METIN_STONE:
					metinSlot[i]=player.METIN_SOCKET_TYPE_SILVER
			self.AddItemData(itemVnum, metinSlot, attrSlot, type)
	else:
		def AddRefineItemData(self, itemVnum, metinSlot, attrSlot = 0):
			for i in xrange(player.METIN_SOCKET_MAX_NUM):
				metinSlotData=metinSlot[i]
				if self.GetMetinItemIndex(metinSlotData) == constinfo.ERROR_METIN_STONE:
					metinSlot[i]=player.METIN_SOCKET_TYPE_SILVER
			self.AddItemData(itemVnum, metinSlot, attrSlot)
			

	def AddItemData_Offline(self, itemVnum, itemDesc, itemSummary, metinSlot, attrSlot):
		self.__SetItemTitle(itemVnum, metinSlot, attrSlot)

		#if self.__IsHair(itemVnum):
		#	self.__AppendHairIcon(itemVnum)

		### Description ###
		self.AppendDescription(itemDesc, 26)
		self.AppendDescription(itemSummary, 26, self.CONDITION_COLOR)

	if app.__ENABLE_NEW_OFFLINESHOP__:
		def AddRightClickForSale(self):
			self.AppendTextLine(localeinfo.OFFLINESHOP_TOOLTIP_RIGHT_CLICK_FOR_SALE, uiofflineshop.COLOR_TEXT_SHORTCUT)
	
	if app.NEW_PET_SYSTEM:
		def check_sigillo(self, item_vnum):
			for x in range(55701,55711+1):
				if x == item_vnum:
					return True	
			if item_vnum == 55801:
				return True
			return False

	if app.ENABLE_SOUL_SYSTEM:
		def __SoulItemToolTip(self, itemVnum, metinSlot, flags):
			self.itemVnum = itemVnum
			item.SelectItem(itemVnum)
			itemType = item.GetItemType()
			itemSubType = item.GetItemSubType()
			
			(limit_Type, limit_Value) = item.GetLimit(1)
			max_time = limit_Value
			
			if 0 != flags:
				metinSlot[2] = item.GetValue(2)
				
			data = metinSlot[2]
			keep_time = data / 10000
			remain_count = data % 10000
			
			value_index = 2 + keep_time / 60
			if value_index < 3:
				value_index = 3
			if value_index > 5:
				value_index = 5
			damage_value = float( item.GetValue(value_index) / 10.0 )
		
			self.ClearToolTip()
			
			soul_desc = ""
			if item.RED_SOUL == itemSubType:
				soul_desc = localeinfo.SOUL_ITEM_TOOLTIP_RED1 % (keep_time, max_time)
				soul_desc += localeinfo.SOUL_ITEM_TOOLTIP_RED2 % (damage_value)
			elif item.BLUE_SOUL == itemSubType:
				soul_desc = localeinfo.SOUL_ITEM_TOOLTIP_BLUE1 % (keep_time, max_time)
				soul_desc += localeinfo.SOUL_ITEM_TOOLTIP_BLUE2 % (damage_value)
			
			self.__SetNormalItemTitle()
			
			if keep_time < 10:
				desc_color = 0xfff15f5f	# RGB(241,95,95)
			else:
				desc_color = 0xff86e57f	# RGB(134,229,127)
				
			self.AppendDescription( soul_desc, 26, desc_color)
	
			self.AppendDescription(localeinfo.SOUL_ITEM_TOOLTIP_COMMON, 26, 0xfff15f5f)	# RGB(241,95,95)
			self.AppendSpace(10)

			if metinSlot[1] and keep_time >= 10 and remain_count > 0:
				self.AppendTextLine( localeinfo.SOUL_ITEM_TOOLTIP_APPLY, 0xff86e57f )		# RGB(134,229,127)
			else:
				self.AppendTextLine( localeinfo.SOUL_ITEM_TOOLTIP_UNAPPLIED, 0xfff15f5f )	# RGB(241,95,95)
				
			self.AppendSpace(5)
			
			self.AppendTextLine( localeinfo.SOUL_ITEM_TOOLTIP_REMAIN_COUNT % remain_count )
			self.AppendSpace(5)
			
			if 0 == flags:
				for i in xrange(item.LIMIT_MAX_NUM):
					(limitType, limitValue) = item.GetLimit(i)
					if item.LIMIT_REAL_TIME == limitType:
						self.AppendSoulItemLastTime(metinSlot[0])
			else:
				(limitType, limitValue) = item.GetLimit(0)
				self.AppendSoulItemLastTime(limitValue + app.GetGlobalTimeStamp())
					
			self.ShowToolTip()
			
		def AppendSoulItemLastTime(self, endTime):
			leftSec = max(0, endTime - app.GetGlobalTimeStamp())
			if leftSec > 0:
				self.AppendSpace(5)
				self.AppendTextLine(localeinfo.SOUL_ITEM_TOOLTIP_REMOVE_TIME + " : " + localeinfo.SecondToDHM(leftSec), self.NORMAL_COLOR)
	

	def AppendAntiflagInformation(self):
		antiFlagDict = {
			localeinfo.TOOLTIP_ANTIDROP: item.IsAntiFlag(item.ITEM_ANTIFLAG_DROP),
			localeinfo.TOOLTIP_ANTISELL: item.IsAntiFlag(item.ITEM_ANTIFLAG_SELL),
			localeinfo.TOOLTIP_ANTISHOP: item.IsAntiFlag(item.ITEM_ANTIFLAG_MYSHOP),
			localeinfo.TOOLTIP_ANTISAFEBOX: item.IsAntiFlag(item.ITEM_ANTIFLAG_SAFEBOX),
		}
		
		antiFlagNames = [name for name, flag in antiFlagDict.iteritems() if flag]
		if antiFlagNames:
			self.AppendSpace(5)
			textLine = self.AppendTextLine('{} {}'.format(', '.join(antiFlagNames), ""), self.NORMAL_COLOR)
			textLine.SetFeather()

	def AddItemData(self, itemVnum, metinSlot, attrSlot = 0, flags = 0, unbindTime = 0, window_type = player.INVENTORY, slotIndex = -1, lockedattr = -1, gaya = 0, wiki = 0, shop = False):
		#preview = 1
		self.itemVnum = itemVnum
		item.SelectItem(itemVnum)
		itemType = item.GetItemType()
		itemSubType = item.GetItemSubType()
		
		if itemType == item.ITEM_TYPE_ISHOP:
			desc = item.GetItemDescription()
			self.AppendTextLine(item.GetItemName(), self.TITLE_COLOR)
			if not metinSlot[0]:
				self.AppendDescription(desc, 26)
				self.ShowToolTip()
				return
			
			self.itemVnum = metinSlot[0]
			item.SelectItem(self.itemVnum)
			name = item.GetItemName()
			self.AppendDescription(desc, 26)
			self.AppendTextLine(name, self.CAN_LEVEL_UP_COLOR)
			self.AppendAntiflagInformation()
			self.ShowToolTip()
			return
		
		if itemType == item.ITEM_TYPE_USE and itemSubType == item.USE_FISH:
			self.AppendTextLine(item.GetItemName(), self.NORMAL_COLOR)
			self.AppendDescription(item.GetItemDescription(), 26)
			self.AppendSpace(5)
			self.__AppendAffectInformation()
			self.AppendSpace(5)
			duration = item.GetValue(0) / 60
			self.AppendTextLine(localeinfo.TOOLTIP_FISH_USE_TIME % (duration))
			self.AppendAntiflagInformation()
			self.ShowToolTip()
			return
		
		if app.NEW_PET_SYSTEM and itemVnum == 55002:
			self.AppendTextLine(item.GetItemName(), self.NORMAL_COLOR)
			self.AppendDescription(item.GetItemDescription(), 26)
			if gaya == 1:
				petVnum = 0
			else:
				petVnum = metinSlot[0]
			
			if petVnum != 0:
				name = nonplayer.GetMonsterName(petVnum)
				if name != "":
					self.AppendSpace(5)
					self.AppendTextLine(name, self.SPECIAL_TITLE_COLOR)
				
				self.AppendSpace(5)
				self.AppendTextLine(localeinfo.PETBOX_DESC1)
				time = app.GetGlobalTimeStamp() - metinSlot[2]
				minute = int((time / 60) % 60)
				hour = int((time / 60) / 60) % 24
				day = int(int((time / 60) / 60) / 24)
				self.AppendTextLine(localeinfo.PETBOX_DESC2 % (day, hour, minute))
				l = attrSlot[0][0]
				self.AppendTextLine(localeinfo.PETBOX_DESC3 % (l))
				e = attrSlot[0][1]
				evo = [localeinfo.PET_EVOLUTION1, localeinfo.PET_EVOLUTION2, localeinfo.PET_EVOLUTION3, localeinfo.PET_EVOLUTION4]
				self.AppendTextLine(localeinfo.PETBOX_DESC4 % (evo[e]))
				b1 = attrSlot[1][0]
				self.AppendTextLine(localeinfo.PETBOX_DESC5 % (pointop(str(b1))), self.SPECIAL_POSITIVE_COLOR)
				b2 = attrSlot[1][1]
				self.AppendTextLine(localeinfo.PETBOX_DESC6 % (pointop(str(b2))), self.SPECIAL_POSITIVE_COLOR)
				b3 = attrSlot[2][0]
				self.AppendTextLine(localeinfo.PETBOX_DESC7 % (pointop(str(b3))), self.SPECIAL_POSITIVE_COLOR)
				
				skillname = [localeinfo.PETSKILLNAME_1, localeinfo.PETSKILLNAME_2, localeinfo.PETSKILLNAME_3, localeinfo.PETSKILLNAME_4, localeinfo.PETSKILLNAME_5, localeinfo.PETSKILLNAME_6, localeinfo.PETSKILLNAME_7, localeinfo.PETSKILLNAME_8, localeinfo.PETSKILLNAME_9, localeinfo.PETSKILLNAME_10, localeinfo.PETSKILLNAME_11, localeinfo.PETSKILLNAME_12]
				skill1 = attrSlot[2][1]
				skilllv1 = attrSlot[3][0]
				if skill1 == 255:
					self.AppendTextLine(localeinfo.PETBOX_DESC8A)
				elif skill1 == 0:
					self.AppendTextLine(localeinfo.PETBOX_DESC8B)
				else:
					self.AppendTextLine(localeinfo.PETBOX_DESC8C % (skillname[skill1 - 1], skilllv1))
				skill2 = attrSlot[3][1]
				skilllv2 = attrSlot[4][0]
				if skill2 == 255:
					self.AppendTextLine(localeinfo.PETBOX_DESC9A)
				elif skill2 == 0:
					self.AppendTextLine(localeinfo.PETBOX_DESC9B)
				else:
					self.AppendTextLine(localeinfo.PETBOX_DESC9C % (skillname[skill2 - 1], skilllv2))
				skill3 = attrSlot[4][1]
				skilllv3 = attrSlot[5][0]
				if skill3 == 255:
					self.AppendTextLine(localeinfo.PETBOX_DESC10A)
				elif skill3 == 0:
					self.AppendTextLine(localeinfo.PETBOX_DESC10B)
				else:
					self.AppendTextLine(localeinfo.PETBOX_DESC10C % (skillname[skill3 - 1], skilllv3))
				skill4 = attrSlot[5][1]
				skilllv4 = attrSlot[6][0]
				if skill4 == 255:
					self.AppendTextLine(localeinfo.PETBOX_DESC11A)
				elif skill4 == 0:
					self.AppendTextLine(localeinfo.PETBOX_DESC11B)
				else:
					self.AppendTextLine(localeinfo.PETBOX_DESC11C % (skillname[skill4 - 1], skilllv4))
				
				#self.__ModelPreview(itemVnum, 6, petVnum)
				#if not wiki:
				#	self.AppendTextLine(localeinfo.VIEW_RENDERING)
			
			self.AppendAntiflagInformation()
			self.ShowToolTip()
			return
		
		if app.ENABLE_NEW_USE_POTION:
			if itemType == item.ITEM_TYPE_USE and itemSubType == item.USE_NEW_POTIION:
				self.AppendTextLine(item.GetItemName(), self.NORMAL_COLOR)
				self.AppendDescription(item.GetItemDescription(), 26)
				if not (itemVnum >= 89108 and itemVnum <= 89125):
					self.__AppendAffectInformation()
					if itemVnum == 72013 or itemVnum == 72014 or itemVnum == 72015:
						self.AppendTextLine(localeinfo.TOOLTIP_NEW_POTION30, self.POSITIVE_COLOR)
					
					self.AppendSpace(5)
				
				self.AppendDescription(item.GetItemSummary(), 26, self.CONDITION_COLOR)
				self.AppendSpace(5)
				if gaya == 1 or shop:
					for i in xrange(item.LIMIT_MAX_NUM):
						(limitType, limitValue) = item.GetLimit(i)
						if limitType == item.LIMIT_REAL_TIME and limitValue > 0:
							self.AppendTextLine(localeinfo.LEFT_TIME + " : " + localeinfo.SecondToDHM(limitValue), self.NORMAL_COLOR)
							break
				else:
					if metinSlot[1] == 1:
						self.AppendTextLine(localeinfo.NEW_POTION_STATUS_1, 0xFF45DB45)
					else:
						self.AppendTextLine(localeinfo.NEW_POTION_STATUS_0, 0xFFDB4545)
					
					leftTime = metinSlot[0]
					if leftTime >= 0:
						for i in xrange(item.LIMIT_MAX_NUM):
							(limitType, limitValue) = item.GetLimit(i)
							if limitType != 0:
								self.AppendTextLine(localeinfo.LEFT_TIME + " : " + localeinfo.SecondToDHM(leftTime), self.NORMAL_COLOR)
								break
					else:
						for i in xrange(item.LIMIT_MAX_NUM):
							(limitType, limitValue) = item.GetLimit(i)
							if item.LIMIT_REAL_TIME == limitType and limitValue > 0:
								self.AppendTextLine(localeinfo.LEFT_TIME + " : " + localeinfo.SecondToDHM(limitValue), self.NORMAL_COLOR)
				
				self.AppendAntiflagInformation()
				self.ShowToolTip()
				return
		
		if app.ENABLE_RUNE_SYSTEM:
			if itemType == item.ITEM_TYPE_COSTUME and (itemSubType >= item.RUNE_SLOT1 and itemSubType <= item.RUNE_SLOT7):
				maxTime = item.GetValue(0)
				onePercent = maxTime / 100;
				remainPercent = metinSlot[0] / onePercent;
				titleColor = 0xFFFBFF00
				if itemSubType == item.RUNE_SLOT7:
					titleColor = 0xFFFB00FF
				elif remainPercent >= 60:
					titleColor = 0xFFFB00FF
				elif remainPercent >= 20:
					titleColor = 0xCA00FFFF
				
				self.AppendTextLine(item.GetItemName(), titleColor)
				self.AppendDescription(item.GetItemDescription(), 26)
				self.AppendDescription(item.GetItemSummary(), 26, self.CONDITION_COLOR)
				self.AppendSpace(5)
				self.__AppendAffectInformation()
				self.__AppendAttributeInformation(attrSlot)
				if itemSubType != item.RUNE_SLOT7:
					self.AppendSpace(5)
					self.AppendTextLine(localeinfo.RUNE_POWER % remainPercent, self.NORMAL_COLOR)
				
				self.AppendAntiflagInformation()
				self.ShowToolTip()
				return
			elif itemType == item.ITEM_TYPE_USE and itemSubType == item.USE_RUNE_PERC_CHARGE:
				maxPercent = item.GetValue(0)
				remainPercent = metinSlot[0]
				if remainPercent <= 0:
					remainPercent = maxPercent
				self.AppendTextLine(item.GetItemName(), self.NORMAL_COLOR)
				self.AppendDescription(item.GetItemDescription(), 26)
				self.AppendDescription(item.GetItemSummary(), 26, self.CONDITION_COLOR)
				self.AppendSpace(5)
				self.AppendTextLine(localeinfo.RUNE_POWER_CHARGE % (remainPercent, maxPercent), self.NORMAL_COLOR)
				self.AppendAntiflagInformation()
				self.ShowToolTip()
				return
		
		if app.ENABLE_DS_POTION_DIFFRENT:
			if itemType == item.ITEM_TYPE_USE and itemSubType == item.USE_TIME_CHARGE_PER:
				maxPercent = item.GetValue(0)
				remainPercent = metinSlot[0]
				if remainPercent <= 0:
					remainPercent = maxPercent
				self.AppendTextLine(item.GetItemName(), self.NORMAL_COLOR)
				self.AppendDescription(item.GetItemDescription(), 26)
				self.AppendDescription(item.GetItemSummary(), 26, self.CONDITION_COLOR)
				self.AppendSpace(5)
				self.AppendTextLine(localeinfo.DS_TIME_CHARGE % (remainPercent, remainPercent * 864, maxPercent, maxPercent * 864), self.NORMAL_COLOR)
				self.AppendAntiflagInformation()
				self.ShowToolTip()
				return
		
		if app.ENABLE_ATTR_COSTUMES and itemType == item.ITEM_TYPE_USE and (itemSubType == item.USE_ADD_ATTR_COSTUME1 or itemSubType == item.USE_ADD_ATTR_COSTUME2):
			color = grp.GenerateColor(1.0, 0.7843, 0.0, 1.0)
			if itemSubType == item.USE_ADD_ATTR_COSTUME2:
				color = grp.GenerateColor(1.0, 0.0, 0.0, 1.0)
			
			self.AppendTextLine(item.GetItemName(), color)
			self.AppendDescription(item.GetItemDescription(), 26)
			self.AppendDescription(item.GetItemSummary(), 26, self.CONDITION_COLOR)
			self.AppendSpace(5)
			
			type = metinSlot[0]
			value = metinSlot[1]
			if type > 0:
				affectString = self.__GetAffectString(type, value)
				if affectString:
					self.AppendTextLine(affectString, self.SPECIAL_POSITIVE_COLOR)
			
			self.AppendAntiflagInformation()
			self.ShowToolTip()
			return
		
		if 50026 == itemVnum:
			if 0 != metinSlot:
				name = item.GetItemName()
				if metinSlot[0] > 0:
					name += " "
					name += localeinfo.NumberToMoneyString(metinSlot[0])
				self.SetTitle(name)
				self.__AppendSealInformation(window_type, slotIndex) ## cyh itemseal 2013 11 11
				self.AppendAntiflagInformation()
				self.ShowToolTip()
			return
			
		# if app.__ENABLE_NEW_EFFECT_CIANITE_WEAPON__:
			# if constinfo.IS_EPIC_WEAPON(itemVnum):
				# self.AppendTextLine(localeinfo.GRADE_CIANITE_EPIC, self.CIANITE_COLOR_WEAPON_EPIC)
			# if constinfo.IS_LEGG_WEAPON(itemVnum):
				# self.AppendTextLine(localeinfo.GRADE_CIANITE_LEGG, self.CIANITE_COLOR_WEAPON_LEGG)
			# if constinfo.IS_ANTIC_WEAPON(itemVnum):
				# self.AppendTextLine(localeinfo.GRADE_CIANITE_ANTIC, self.CIANITE_COLOR_WEAPON_ANTIC)
			# if constinfo.IS_MISTIC_WEAPON(itemVnum):
				# self.AppendTextLine(localeinfo.GRADE_CIANITE_MISTC, self.CIANITE_COLOR_WEAPON_MISTIC)

		### Skill Book ###
		if 50300 == itemVnum and not self.isBook:
			if 0 != metinSlot and not self.isBook:
				self.__SetSkillBookToolTip(metinSlot[0], localeinfo.TOOLTIP_SKILLBOOK_NAME, 1)
				self.AppendAntiflagInformation()
				self.ShowToolTip()
			elif self.isBook:
				self.SetTitle(item.GetItemName())
				self.AppendDescription(item.GetItemDescription(), 26)
				self.AppendDescription(item.GetItemSummary(), 26, self.CONDITION_COLOR)
				self.AppendAntiflagInformation()
				self.ShowToolTip()					
			return
		elif 70037 == itemVnum :
			if 0 != metinSlot and not self.isBook2:
				self.__SetSkillBookToolTip(metinSlot[0], localeinfo.TOOLTIP_SKILL_FORGET_BOOK_NAME, 0)
				self.AppendDescription(item.GetItemDescription(), 26)
				self.AppendDescription(item.GetItemSummary(), 26, self.CONDITION_COLOR)
				self.AppendAntiflagInformation()
				self.ShowToolTip()
			elif self.isBook2:
				self.SetTitle(item.GetItemName())
				self.AppendDescription(item.GetItemDescription(), 26)
				self.AppendDescription(item.GetItemSummary(), 26, self.CONDITION_COLOR)
				self.AppendAntiflagInformation()
				self.ShowToolTip()					
			return
		elif 70055 == itemVnum:
			if 0 != metinSlot:
				self.__SetSkillBookToolTip(metinSlot[0], localeinfo.TOOLTIP_SKILL_FORGET_BOOK_NAME, 0)
				self.AppendDescription(item.GetItemDescription(), 26)
				self.AppendDescription(item.GetItemSummary(), 26, self.CONDITION_COLOR)
				self.AppendAntiflagInformation()
				self.ShowToolTip()
			return
		elif 70037 == itemVnum:
			if 0 != metinSlot:
				self.__SetSkillBookToolTip(metinSlot[0], localeinfo.TOOLTIP_SKILL_FORGET_BOOK_NAME, 0)
				self.AppendDescription(item.GetItemDescription(), 26)
				self.AppendDescription(item.GetItemSummary(), 26, self.CONDITION_COLOR)
				self.__AppendSealInformation(window_type, slotIndex) ## cyh itemseal 2013 11 11
				self.AppendAntiflagInformation()
				self.ShowToolTip()
			return
		elif 70055 == itemVnum:
			if 0 != metinSlot:
				self.__SetSkillBookToolTip(metinSlot[0], localeinfo.TOOLTIP_SKILL_FORGET_BOOK_NAME, 0)
				self.AppendDescription(item.GetItemDescription(), 26)
				self.AppendDescription(item.GetItemSummary(), 26, self.CONDITION_COLOR)
				self.__AppendSealInformation(window_type, slotIndex) ## cyh itemseal 2013 11 11
				self.AppendAntiflagInformation()
				self.ShowToolTip()
			return

		if app.ENABLE_SOUL_SYSTEM:
			if item.ITEM_TYPE_SOUL == itemType:
				self.__SoulItemToolTip(itemVnum, metinSlot, flags)
				return
				

		###########################################################################################

		itemDesc = item.GetItemDescription()
		itemSummary = item.GetItemSummary()

		isCostumeItem = 0
		isCostumeHair = 0
		isCostumeBody = 0
		isCostumeStole = 0
		if app.ENABLE_STOLE_COSTUME:
			if item.ITEM_TYPE_COSTUME == itemType:
				isCostumeStole = item.COSTUME_TYPE_STOLE == itemSubType
		
		if app.ENABLE_COSTUME_PET:
			isCostumePetSkin = 0
			if item.ITEM_TYPE_COSTUME == itemType:
				isCostumePetSkin = item.COSTUME_PET_SKIN == itemSubType
		
		if app.ENABLE_COSTUME_MOUNT:
			isCostumeMountSkin = 0
			if item.ITEM_TYPE_COSTUME == itemType:
				isCostumeMountSkin = item.COSTUME_MOUNT_SKIN == itemSubType
		
		if app.ENABLE_COSTUME_EFFECT:
			isCostumeEffectBody = 0
			isCostumeEffectWeapon = 0
			if item.ITEM_TYPE_COSTUME == itemType:
				isCostumeEffectBody = item.COSTUME_EFFECT_BODY == itemSubType
				isCostumeEffectWeapon = item.COSTUME_EFFECT_WEAPON == itemSubType
		
		if app.ENABLE_ACCE_SYSTEM:
			isCostumeAcce = 0
		if app.ENABLE_MOUNT_COSTUME_SYSTEM:
			isCostumeMount = 0
		if app.ENABLE_ACCE_COSTUME_SYSTEM:
			isCostumeAcce = 0
		if app.ENABLE_WEAPON_COSTUME_SYSTEM:
			isCostumeWeapon = 0

		if app.ENABLE_COSTUME_SYSTEM:
			if item.ITEM_TYPE_COSTUME == itemType:
				isCostumeItem = 1
				isCostumeHair = item.COSTUME_TYPE_HAIR == itemSubType
				isCostumeBody = item.COSTUME_TYPE_BODY == itemSubType
				if app.ENABLE_ACCE_SYSTEM:
					isCostumeAcce = itemSubType == item.COSTUME_TYPE_ACCE
				
				if app.ENABLE_MOUNT_COSTUME_SYSTEM:
					isCostumeMount = item.COSTUME_TYPE_MOUNT == itemSubType
				if app.ENABLE_ACCE_COSTUME_SYSTEM:
					isCostumeAcce = item.COSTUME_TYPE_ACCE == itemSubType
				if app.ENABLE_WEAPON_COSTUME_SYSTEM:
					isCostumeWeapon = item.COSTUME_TYPE_WEAPON == itemSubType

				#dbg.TraceError("IS_COSTUME_ITEM! body(%d) hair(%d)" % (isCostumeBody, isCostumeHair))

		self.__SetItemTitle(itemVnum, metinSlot, attrSlot)

		#self.__ModelPreviewClose()

		### Hair Preview Image ###
		#if self.__IsHair(itemVnum):
		#	self.__AppendHairIcon(itemVnum)

		### Description ###
		self.AppendDescription(itemDesc, 26)
		self.AppendDescription(itemSummary, 26, self.CONDITION_COLOR)



		if app.ENABLE_FEATURES_REFINE_SYSTEM:
			if itemVnum in (player.REFINE_VNUM_POTION_LOW, player.REFINE_VNUM_POTION_MEDIUM, player.REFINE_VNUM_POTION_EXTRA):

				# self.DESCRIPTION_VNUMS = [
					# localeinfo.REFINE_TOOLTIP_ITEM_DESCRIPTION_1,
					# localeinfo.REFINE_TOOLTIP_ITEM_DESCRIPTION_2,
					# localeinfo.REFINE_TOOLTIP_ITEM_DESCRIPTION_3,
					# localeinfo.REFINE_TOOLTIP_ITEM_DESCRIPTION_4
				# ]
				
				self.PERCENTAGE_VNUMS = {
					player.REFINE_VNUM_POTION_LOW : player.REFINE_PERCENTAGE_LOW,
					player.REFINE_VNUM_POTION_MEDIUM : player.REFINE_PERCENTAGE_MEDIUM,
					player.REFINE_VNUM_POTION_EXTRA : player.REFINE_PERCENTAGE_EXTRA
				}
				
				self.COLORS = [
					self.NORMAL_COLOR, self.SPECIAL_POSITIVE_COLOR, self.DISABLE_COLOR, self.HIGH_PRICE_COLOR
				]
					
				self.AppendSpace(5)

				# for it in xrange(len(self.DESCRIPTION_VNUMS) - 1):
					# self.AppendDescription(self.DESCRIPTION_VNUMS[it], None, self.COLORS[it])
				# self.AppendDescription(self.DESCRIPTION_VNUMS[1] % (self.PERCENTAGE_VNUMS[itemVnum]), None, self.COLORS[3])

		if app.ENABLE_RARITY_SYSTEM:
			itemRarity = item.GetRarity()
			rarityDesc = self.RARITY_DESC[itemRarity]
			self.AppendTextLine(rarityDesc, self.RARITY_COLOR[itemRarity])
			self.AppendSpace(5)


		if app.NEW_PET_SYSTEM:
			if self.check_sigillo(itemVnum) and not wiki:
				if attrSlot[0][1] != 0:
					self.AppendSpace(5)
					self.AppendTextLine(localeinfo.PET_TOOLTIP_LV % (attrSlot[3][1]), self.NORMAL_COLOR)
					self.AppendTextLine(localeinfo.PET_TOOLTIP_BNS1 % (pointop(str(attrSlot[0][1]))), self.SPECIAL_POSITIVE_COLOR)
					self.AppendTextLine(localeinfo.PET_TOOLTIP_BNS2 % (pointop(str(attrSlot[1][1]))), self.SPECIAL_POSITIVE_COLOR)
					self.AppendTextLine(localeinfo.PET_TOOLTIP_BNS3 % (pointop(str(attrSlot[2][1]))), self.SPECIAL_POSITIVE_COLOR)
					self.AppendSpace(5)
					if itemVnum != 55002:
						days = (int(metinSlot[1])/60)/24
						hours = (int(metinSlot[1]) - (days*60*24)) / 60
						mins = int(metinSlot[1]) - (days*60*24) - (hours*60)
						if app.ENABLE_NEW_PET_EDITS:
							if int(metinSlot[1]) > 525600:
								self.AppendTextLine(localeinfo.PET_TIME_TOOLTIP_INF)
							else:
								self.AppendTextLine(localeinfo.PET_TIME_TOOLTIP % (days, hours, mins), self.NORMAL_COLOR)
						else:
							self.AppendTextLine(localeinfo.PET_TIME_TOOLTIP % (days, hours, mins), self.NORMAL_COLOR)

		### Weapon ###
		if item.ITEM_TYPE_WEAPON == itemType:
			
			self.__AppendLimitInformation()
			self.AppendSpace(5)
		
			## ������ ���� ������ ���� ��������.
			if item.WEAPON_FAN == itemSubType:
				self.__AppendMagicAttackInfo()
				self.__AppendAttackPowerInfo()
		
			else:
				self.__AppendAttackPowerInfo()
				self.__AppendMagicAttackInfo()
		
			self.__AppendAffectInformation()
			if app.ATTR_LOCK:		
				#lockedattr = player.GetItemAttrLocked(window_type, slotIndex)
				self.__AppendAttributeInformation(attrSlot, 0, lockedattr)
			else:
				self.__AppendAttributeInformation(attrSlot)
		
			self.AppendWearableInformation()
			self.__AppendMetinSlotInfo(metinSlot)
			
			#if preview != 0:
			#	race = 0
			#	if itemSubType == item.WEAPON_SWORD:
			#		if player.GetRace() == 6 or player.GetRace() == 2:
			#			race = player.GetRace()
			#		if player.GetRace() != 3 and player.GetRace() != 7 and player.GetRace() != 8:
			#			race = player.GetRace()
			#			if item.IsAntiFlag(item.ITEM_ANTIFLAG_WARRIOR) and item.IsAntiFlag(item.ITEM_ANTIFLAG_ASSASSIN) and item.IsAntiFlag(item.ITEM_ANTIFLAG_SHAMAN) and race != 6 and race != 2:
			#				if chr.RaceToSex(player.GetRace()) == 1:
			#					race = 2
			#				else:
			#					race = 6
			#		else:
			#			if chr.RaceToSex(player.GetRace()) == 1:
			#				race = 0
			#			else:
			#				race = 4
			#			
			#			if item.IsAntiFlag(item.ITEM_ANTIFLAG_WARRIOR) and item.IsAntiFlag(item.ITEM_ANTIFLAG_ASSASSIN) and item.IsAntiFlag(item.ITEM_ANTIFLAG_SHAMAN) and race != 6 and race != 2:
			#				if chr.RaceToSex(player.GetRace()) == 1:
			#					race = 2
			#				else:
			#					race = 6
			#	elif itemSubType == item.WEAPON_TWO_HANDED:
			#		if player.GetRace() != 0 and player.GetRace() != 4:
			#			if chr.RaceToSex(player.GetRace()) == 1:
			#				race = 0
			#			else:
			#				race = 4
			#		else:
			#			race = player.GetRace()
			#	elif itemSubType == item.WEAPON_DAGGER or itemSubType == item.WEAPON_BOW:
			#		if player.GetRace() != 1 and player.GetRace() != 5:
			#			if chr.RaceToSex(player.GetRace()) == 1:
			#				race = 5
			#			else:
			#				race = 1
			#		else:
			#			race = player.GetRace()
			#	elif itemSubType == item.WEAPON_BELL or itemSubType == item.WEAPON_FAN:
			#		if player.GetRace() != 3 and player.GetRace() != 7:
			#			if chr.RaceToSex(player.GetRace()) == 1:
			#				race = 7
			#			else:
			#				race = 3
			#		else:
			#			race = player.GetRace()
			#	
			#	if itemSubType != 6:
			#		self.__ModelPreview(itemVnum, 3, race)
		## Armor ###
		elif item.ITEM_TYPE_ARMOR == itemType:
			self.__AppendLimitInformation()

			## ������
			defGrade = item.GetValue(1)
			defBonus = item.GetValue(5)*2 ## ������ ���� ���� ���� ������ ����
			if defGrade > 0:
				self.AppendSpace(5)
				self.AppendTextLine(localeinfo.TOOLTIP_ITEM_DEF_GRADE % (defGrade+defBonus), self.GetChangeTextLineColor(defGrade))

			self.__AppendMagicDefenceInfo()
			self.__AppendAffectInformation()
			if app.ATTR_LOCK:
				#lockedattr = player.GetItemAttrLocked(window_type, slotIndex)
				self.__AppendAttributeInformation(attrSlot, 0, lockedattr)
			else:
				self.__AppendAttributeInformation(attrSlot)
			
			#if preview != 0 and itemSubType == 0:
			#	self.__ModelPreview(itemVnum, 2, self.__ItemGetRace(1))
			
			self.AppendWearableInformation()

			if itemSubType in (item.ARMOR_WRIST, item.ARMOR_NECK, item.ARMOR_EAR):
				self.__AppendAccessoryMetinSlotInfo(metinSlot, constinfo.GET_ACCESSORY_MATERIAL_VNUM(itemVnum, itemSubType))
			else:
				self.__AppendMetinSlotInfo(metinSlot)

		### Ring Slot Item (Not UNIQUE) ###
		elif item.ITEM_TYPE_RING == itemType:
			self.__AppendLimitInformation()
			self.__AppendAffectInformation()
			if  app.ATTR_LOCK:
				#lockedattr = player.GetItemAttrLocked(window_type, slotIndex)
				self.__AppendAttributeInformation(attrSlot, 0, lockedattr)
			else:
				self.__AppendAttributeInformation(attrSlot)
		elif item.COSTUME_EFFECT_BODY == itemSubType:
			self.__AppendLimitInformation()
			self.__AppendAffectInformation()
			if  app.ATTR_LOCK:
				#lockedattr = player.GetItemAttrLocked(window_type, slotIndex)
				self.__AppendAttributeInformation(attrSlot, 0, lockedattr)
			else:
				self.__AppendAttributeInformation(attrSlot)
		elif item.ITEM_TYPE_BELT == itemType:
			self.__AppendLimitInformation()
			self.__AppendAffectInformation()
			if  app.ATTR_LOCK:
				#lockedattr = player.GetItemAttrLocked(window_type, slotIndex)
				self.__AppendAttributeInformation(attrSlot, 0, lockedattr)
			else:
				self.__AppendAttributeInformation(attrSlot)
		elif item.COSTUME_EFFECT_WEAPON == itemSubType:
			self.__AppendLimitInformation()
			self.__AppendAffectInformation()
			if  app.ATTR_LOCK:
				#lockedattr = player.GetItemAttrLocked(window_type, slotIndex)
				self.__AppendAttributeInformation(attrSlot, 0, lockedattr)
			else:
				self.__AppendAttributeInformation(attrSlot)
		elif item.COSTUME_MOUNT_SKIN == itemSubType:
			self.__AppendLimitInformation()
			self.__AppendAffectInformation()
			if  app.ATTR_LOCK:
				#lockedattr = player.GetItemAttrLocked(window_type, slotIndex)
				self.__AppendAttributeInformation(attrSlot, 0, lockedattr)
			else:
				self.__AppendAttributeInformation(attrSlot)
		elif itemVnum in (2858, 55701, 55702, 55703, 55704, 55705, 55706, 55707, 55708, 55709, 55710, 55711, 53290, 53291, 53292, 53293, 53294, 53295, 53296):
			self.__AppendLimitInformation()
			self.__AppendAffectInformation()
			if app.ATTR_LOCK:
				#lockedattr = player.GetItemAttrLocked(window_type, slotIndex)
				self.__AppendAttributeInformation(attrSlot, 0, lockedattr)
			else:
				self.__AppendAttributeInformation(attrSlot)
		# elif itemVnum == 39017:

			# self.AppendTextLine("Max HP +%")

		elif itemVnum in PetVnum:
			if constinfo.IS_PET_SEAL_OLD(itemVnum):
				self.__AppendAffectInformation()
			
			for i in xrange(item.LIMIT_MAX_NUM):
				(limitType, limitValue) = item.GetLimit(i)
				if item.LIMIT_REAL_TIME == limitType:
					if gaya == 1:
						self.AppendTimerBasedOnWearLastTime(limitValue, True)
					else:
						self.AppendMallItemLastTime(metinSlot[0])
			
			#self.__ModelPreview(itemVnum, 6, PetVnum[itemVnum])
		## ������ ������ ##
		elif 0 != isCostumeItem:
			self.__AppendLimitInformation()

			## If you dont see attributes in costume uncomment this ###
			#self.__AppendAffectInformation()
			#self.__AppendAttributeInformation(attrSlot)
			##########################################################
			#if preview != 0:
			#	if itemSubType == 0: #body
			#		self.__ModelPreview(itemVnum, 2, self.__ItemGetRace(1, 1))
			#	elif itemSubType == 1: #Hair 
			#		self.__ModelPreview(item.GetValue(3), 1, self.__ItemGetRace(1, 1)) #hier in der DB Value3 prufen  
			#		
			#	elif itemSubType == 4: #weapon	
			#		race = 0
			#		itemRealSubType = item.GetValue(3)
			#		if itemRealSubType == item.WEAPON_SWORD:
			#			if player.GetRace() == 6 or player.GetRace() == 2:
			#				race = player.GetRace()
			#			elif player.GetRace() != 3 and player.GetRace() != 7 and player.GetRace() != 8:
			#				race = player.GetRace()
			#				if item.IsAntiFlag(item.ITEM_ANTIFLAG_WARRIOR) and item.IsAntiFlag(item.ITEM_ANTIFLAG_ASSASSIN) and item.IsAntiFlag(item.ITEM_ANTIFLAG_SHAMAN) and race != 6 and race != 2:
			#					if chr.RaceToSex(player.GetRace()) == 1:
			#						race = 2
			#					else:
			#						race = 6
			#			else:
			#				if chr.RaceToSex(player.GetRace()) == 1:
			#					race = 0
			#				else:
			#					race = 4
			#				
			#				if item.IsAntiFlag(item.ITEM_ANTIFLAG_WARRIOR) and item.IsAntiFlag(item.ITEM_ANTIFLAG_ASSASSIN) and item.IsAntiFlag(item.ITEM_ANTIFLAG_SHAMAN) and race != 6 and race != 2:
			#					if chr.RaceToSex(player.GetRace()) == 1:
			#						race = 2
			#					else:
			#						race = 6
			#		elif itemRealSubType == item.WEAPON_TWO_HANDED:
			#			if player.GetRace() != 0 and player.GetRace() != 4:
			#				if chr.RaceToSex(player.GetRace()) == 1:
			#					race = 0
			#				else:
			#					race = 4
			#			else:
			#				race = player.GetRace()
			#		elif itemRealSubType == item.WEAPON_DAGGER or itemRealSubType == item.WEAPON_BOW:
			#			if player.GetRace() != 1 and player.GetRace() != 5:
			#				if chr.RaceToSex(player.GetRace()) == 1:
			#					race = 5
			#				else:
			#					race = 1
			#			else:
			#				race = player.GetRace()
			#		elif itemRealSubType == item.WEAPON_BELL or itemRealSubType == item.WEAPON_FAN:
			#			if player.GetRace() != 3 and player.GetRace() != 7:
			#				if chr.RaceToSex(player.GetRace()) == 1:
			#					race = 7
			#				else:
			#					race = 3
			#			else:
			#				race = player.GetRace()
			#		
			#		self.__ModelPreview(itemVnum, 3, race)
			#	elif itemSubType == 2: #Mount, 
			#		if itemVnum in MountVnum:
			#			self.__ModelPreview(itemVnum, 0, MountVnum[itemVnum])
			#
			#if app.ENABLE_STOLE_COSTUME and isCostumeStole:
			#	if preview != 0:
			#		self.__ModelPreview(itemVnum + 1000, 4, self.__ItemGetRace())
			#
			#if app.ENABLE_COSTUME_PET and isCostumePetSkin:
			#	if preview != 0:
			#		self.__ModelPreview(itemVnum, 6, item.GetValue(0))
			#
			#if app.ENABLE_COSTUME_MOUNT and isCostumeMountSkin:
			#	if preview != 0:
			#		self.__ModelPreview(itemVnum, 6, item.GetValue(0))
			#
			#if app.ENABLE_COSTUME_EFFECT:
			#	if preview != 0:
			#		if isCostumeEffectBody:
			#			race = player.GetRace()
			#			armor = 11209
			#			if race == 1 or race == 5:
			#				armor == 11409
			#			elif race == 2 or race == 6:
			#				armor == 11609
			#			elif race == 3 or race == 7:
			#				armor == 11809
			#			
			#			self.__ModelPreview(item.GetValue(0), 5, self.__ItemGetRace(1), 0, armor)
			#		elif isCostumeEffectWeapon:
			#			weaponID = 19
			#			effectID = item.GetValue(0)
			#			race = player.GetRace()
			#			if race == 1 or race == 5:
			#				effectID = item.GetValue(2)
			#				weaponID = 1009
			#			elif race == 3 or race == 7:
			#				effectID = item.GetValue(5)
			#				weaponID = 7009
			#			
			#			self.__ModelPreview(effectID, 5, self.__ItemGetRace(), weaponID)
			
			if app.ENABLE_ACCE_SYSTEM and isCostumeAcce:
				## ABSORPTION RATE
				absChance = int(metinSlot[acce.ABSORPTION_SOCKET])
				if wiki:
					grade = item.GetValue(0)
					if grade == 1:
						absChance = 1
					elif grade == 2:
						absChance = 5
					elif grade == 3:
						absChance = 10
					
					if grade == 4:
						self.AppendTextLine(localeinfo.SASH_ABSORB_CHANCE2 % (11, 25), self.CONDITION_COLOR)
					else:
						self.AppendTextLine(localeinfo.SASH_ABSORB_CHANCE % (absChance), self.CONDITION_COLOR)
				else:
					self.AppendTextLine(localeinfo.SASH_ABSORB_CHANCE % (absChance), self.CONDITION_COLOR)
				## END ABSOPRTION RATE
				
				itemAbsorbedVnum = int(metinSlot[acce.ABSORBED_SOCKET])

				#if preview != 0:
				#	modelPreviewVnum = itemVnum
				#	if absChance >= 19:
				#		modelPreviewVnum += 1000
				#	
				#	self.__ModelPreview(modelPreviewVnum, 4, self.__ItemGetRace())
			

				if itemAbsorbedVnum:
					## ATTACK / DEFENCE
					item.SelectItem(itemAbsorbedVnum)
					if item.GetItemType() == item.ITEM_TYPE_WEAPON:
						if item.GetItemSubType() == item.WEAPON_FAN:
							self.__AppendMagicAttackInfo(metinSlot[acce.ABSORPTION_SOCKET])
							item.SelectItem(itemAbsorbedVnum)
							self.__AppendAttackPowerInfo(metinSlot[acce.ABSORPTION_SOCKET])
						else:
							self.__AppendAttackPowerInfo(metinSlot[acce.ABSORPTION_SOCKET])
							item.SelectItem(itemAbsorbedVnum)
							self.__AppendMagicAttackInfo(metinSlot[acce.ABSORPTION_SOCKET])
					elif item.GetItemType() == item.ITEM_TYPE_ARMOR:
						defGrade = item.GetValue(1)
						defBonus = item.GetValue(5) * 2
						defGrade = self.CalcAcceValue(defGrade, metinSlot[acce.ABSORPTION_SOCKET])
						defBonus = self.CalcAcceValue(defBonus, metinSlot[acce.ABSORPTION_SOCKET])
						
						if defGrade > 0:
							self.AppendSpace(5)
							self.AppendTextLine(localeinfo.TOOLTIP_ITEM_DEF_GRADE % (defGrade + defBonus), self.GetChangeTextLineColor(defGrade))
						
						item.SelectItem(itemAbsorbedVnum)
						self.__AppendMagicDefenceInfo(metinSlot[acce.ABSORPTION_SOCKET])
					## END ATTACK / DEFENCE
					
					## EFFECT
					item.SelectItem(itemAbsorbedVnum)
					for i in xrange(item.ITEM_APPLY_MAX_NUM):
						(affectType, affectValue) = item.GetAffect(i)
						affectValue = self.CalcAcceValue(affectValue, metinSlot[acce.ABSORPTION_SOCKET])
						affectString = self.__GetAffectString(affectType, affectValue)
						if affectString and affectValue > 0:
							self.AppendTextLine(affectString, self.GetChangeTextLineColor(affectValue))
						
						item.SelectItem(itemAbsorbedVnum)
					# END EFFECT
					
					item.SelectItem(itemVnum)
					## ATTR
					if app.ATTR_LOCK:
						#lockedattr = player.GetItemAttrLocked(window_type, slotIndex)
						self.__AppendAttributeInformation(attrSlot, metinSlot[acce.ABSORPTION_SOCKET], lockedattr)
					else:
						self.__AppendAttributeInformation(attrSlot, metinSlot[acce.ABSORPTION_SOCKET])
					
					# END ATTR
				else:
					# ATTR
					if app.ATTR_LOCK:
						#lockedattr = player.GetItemAttrLocked(window_type, slotIndex)
						self.__AppendAttributeInformation(attrSlot, 0, lockedattr)
					else:
						self.__AppendAttributeInformation(attrSlot)
					# END ATTR
				
				#if not wiki:
				#	self.AppendTextLine(localeinfo.VIEW_RENDERING)
			else:
				self.__AppendAffectInformation()
				if isCostumeStole:
					self.AppendTextLine(localeinfo.GRADE_STOLA % (item.GetValue(0)), self.CONDITION_COLOR)
					self.AppendSpace(5)
				
				if app.ATTR_LOCK:
					#lockedattr = player.GetItemAttrLocked(window_type, slotIndex)
					self.__AppendAttributeInformation(attrSlot, 0, lockedattr, False, isCostumeStole)
				else:
					self.__AppendAttributeInformation(attrSlot, 0, 0, False, isCostumeStole)

			self.AppendWearableInformation()

			bHasRealtimeFlag = 0
			remainTime = 0
			for i in xrange(item.LIMIT_MAX_NUM):
				(limitType, limitValue) = item.GetLimit(i)
				if item.LIMIT_REAL_TIME == limitType:
					bHasRealtimeFlag = 1
					remainTime = limitValue
			
			if 1 == bHasRealtimeFlag:
				realTime = metinSlot[0]
				if realTime == 0:
					self.AppendMallItemRealLastTime(remainTime)
				else:
					self.AppendMallItemLastTime(realTime)
		## Rod ##
		elif item.ITEM_TYPE_ROD == itemType:

			if 0 != metinSlot:
				curLevel = item.GetValue(0) / 10
				curEXP = metinSlot[0]
				maxEXP = item.GetValue(2)
				self.__AppendLimitInformation()
				self.__AppendRodInformation(itemVnum, curLevel, curEXP, maxEXP)

		## Pick ##
		elif item.ITEM_TYPE_PICK == itemType:

			if 0 != metinSlot:
				curLevel = item.GetValue(0) / 10
				curEXP = metinSlot[0]
				maxEXP = item.GetValue(2)
				self.__AppendLimitInformation()
				self.__AppendPickInformation(curLevel, curEXP, maxEXP)

		## Lottery ##
		elif item.ITEM_TYPE_LOTTERY == itemType:
			if 0 != metinSlot:

				ticketNumber = int(metinSlot[0])
				stepNumber = int(metinSlot[1])

				self.AppendSpace(5)
				self.AppendTextLine(localeinfo.TOOLTIP_LOTTERY_STEP_NUMBER % (stepNumber), self.NORMAL_COLOR)
				self.AppendTextLine(localeinfo.TOOLTIP_LOTTO_NUMBER % (ticketNumber), self.NORMAL_COLOR);

		### Metin ###
		elif item.ITEM_TYPE_METIN == itemType:
			self.AppendMetinInformation()
			self.AppendMetinWearInformation()

		### Fish ###
		elif item.ITEM_TYPE_FISH == itemType:
			if 0 != metinSlot:
				self.__AppendFishInfo(metinSlot[0])

		## item.ITEM_TYPE_BLEND
		elif item.ITEM_TYPE_BLEND == itemType:
			self.__AppendLimitInformation()

			if metinSlot:
				affectType = metinSlot[0]
				affectValue = metinSlot[1]
				time = metinSlot[2]
				self.AppendSpace(5)
				affectText = self.__GetAffectString(affectType, affectValue)

				self.AppendTextLine(affectText, self.NORMAL_COLOR)

				if time > 0:
					minute = (time / 60)
					second = (time % 60)
					timeString = localeinfo.TOOLTIP_POTION_TIME

					if minute > 0:
						timeString += str(minute) + localeinfo.TOOLTIP_POTION_MIN
					if second > 0:
						timeString += " " + str(second) + localeinfo.TOOLTIP_POTION_SEC

					self.AppendTextLine(timeString)
				else:
					self.AppendTextLine(localeinfo.BLEND_POTION_NO_TIME)
			else:
				self.AppendTextLine("BLEND_POTION_NO_INFO")

		elif item.ITEM_TYPE_UNIQUE == itemType:
			if itemVnum == 71148 or itemVnum == 40004 or itemVnum == 71136 or itemVnum == 71135 or itemVnum == 71188 or itemVnum == 71234 or itemVnum == 71235 or itemVnum == 70058 or itemVnum == 70059 or itemVnum == 70060:
				self.__AppendAffectInformation()
			
			if 0 != metinSlot:
				bHasRealtimeFlag = 0
				#bHasAnyOtherTime = 0
				remainTime = 0
				for i in xrange(item.LIMIT_MAX_NUM):
					(limitType, limitValue) = item.GetLimit(i)
					if item.LIMIT_REAL_TIME == limitType:
						bHasRealtimeFlag = 1
						remainTime = limitValue
					#elif item.LIMIT_REAL_TIME_START_FIRST_USE == limitType:
					#	bHasAnyOtherTime = 1
					#elif item.LIMIT_TIMER_BASED_ON_WEAR == limitType:
					#	bHasAnyOtherTime = 1
				
				if 1 == bHasRealtimeFlag:
					realTime = metinSlot[0]
					if realTime == 0:
						self.AppendMallItemRealLastTime(remainTime)
					else:
						self.AppendMallItemLastTime(realTime)
				#elif bHasAnyOtherTime == 1 or item.GetValue(0) > 0 or item.GetValue(1) > 0 or item.GetValue(2) > 0:
				elif 1 == item.GetValue(2):
					time = metinSlot[player.METIN_SOCKET_MAX_NUM-1]
					self.AppendMallItemLastTime(time)
					#if 1 == item.GetValue(2):
					#	self.AppendMallItemLastTime(time)
					#else:
					#	self.AppendUniqueItemLastTime(time)

		### Use ###
		elif item.ITEM_TYPE_USE == itemType:
			self.__AppendLimitInformation()

			if item.USE_POTION == itemSubType or item.USE_POTION_NODELAY == itemSubType:
				self.__AppendPotionInformation()

			elif item.USE_ABILITY_UP == itemSubType:
				self.__AppendAbilityPotionInformation()


			## ���� ������
			if 27989 == itemVnum or 76006 == itemVnum:
				if 0 != metinSlot:
					useCount = int(metinSlot[0])

					self.AppendSpace(5)
					self.AppendTextLine(localeinfo.TOOLTIP_REST_USABLE_COUNT % (6 - useCount), self.NORMAL_COLOR)

			## ������ ������
			elif 50004 == itemVnum:
				if 0 != metinSlot:
					useCount = int(metinSlot[0])

					self.AppendSpace(5)
					self.AppendTextLine(localeinfo.TOOLTIP_REST_USABLE_COUNT % (10 - useCount), self.NORMAL_COLOR)

			## ��������
			elif constinfo.IS_AUTO_POTION(itemVnum):
				if 0 != metinSlot:
					## 0: ������, 1: ������, 2: ����
					isActivated = int(metinSlot[0])
					usedAmount = float(metinSlot[1])
					totalAmount = float(metinSlot[2])

					if 0 == totalAmount:
						totalAmount = 1

					self.AppendSpace(5)

					if 0 != isActivated:
						self.AppendTextLine("(%s)" % (localeinfo.TOOLTIP_AUTO_POTION_USING), self.SPECIAL_POSITIVE_COLOR)
						self.AppendSpace(5)
					
					if constinfo.IS_INFINITE_AUTO_POTION(itemVnum) == 0:
						self.AppendTextLine(localeinfo.TOOLTIP_AUTO_POTION_REST % (100.0 - ((usedAmount / totalAmount) * 100.0)), self.POSITIVE_COLOR)

			## ���� ������
			elif itemVnum in WARP_SCROLLS:
				if 0 != metinSlot:
					xPos = int(metinSlot[0])
					yPos = int(metinSlot[1])

					if xPos != 0 and yPos != 0:
						(mapName, xBase, yBase) = background.GlobalPositionToMapInfo(xPos, yPos)

						localeMapName=localeinfo.MINIMAP_ZONE_NAME_DICT.get(mapName, "")

						self.AppendSpace(5)

						if localeMapName!="":
							self.AppendTextLine(localeinfo.TOOLTIP_MEMORIZED_POSITION % (localeMapName, int(xPos-xBase)/100, int(yPos-yBase)/100), self.NORMAL_COLOR)
						else:
							self.AppendTextLine(localeinfo.TOOLTIP_MEMORIZED_POSITION_ERROR % (int(xPos)/100, int(yPos)/100), self.NORMAL_COLOR)
							dbg.TraceError("NOT_EXIST_IN_MINIMAP_ZONE_NAME_DICT: %s" % mapName)

			#####
			if item.USE_SPECIAL == itemSubType:
				bHasRealtimeFlag = 0
				remainTime = 0
				for i in xrange(item.LIMIT_MAX_NUM):
					(limitType, limitValue) = item.GetLimit(i)
					if item.LIMIT_REAL_TIME == limitType:
						bHasRealtimeFlag = 1
						remainTime = limitValue
				
				if 1 == bHasRealtimeFlag:
					realTime = metinSlot[0]
					if realTime == 0:
						self.AppendMallItemRealLastTime(remainTime)
					else:
						self.AppendMallItemLastTime(realTime)
				else:
					if 0 != metinSlot:
						time = metinSlot[player.METIN_SOCKET_MAX_NUM-1]
						if 1 == item.GetValue(2):
							self.AppendMallItemLastTime(time)

			elif item.USE_TIME_CHARGE_PER == itemSubType:
				bHasRealtimeFlag = 0
				remainTime = 0
				for i in xrange(item.LIMIT_MAX_NUM):
					(limitType, limitValue) = item.GetLimit(i)
					if item.LIMIT_REAL_TIME == limitType:
						bHasRealtimeFlag = 1
						remainTime = limitValue
				
				if 1 == bHasRealtimeFlag:
					realTime = metinSlot[0]
					if realTime == 0:
						self.AppendMallItemRealLastTime(remainTime)
					else:
						self.AppendMallItemLastTime(realTime)
			elif item.USE_TIME_CHARGE_FIX == itemSubType:
				bHasRealtimeFlag = 0
				remainTime = 0
				for i in xrange(item.LIMIT_MAX_NUM):
					(limitType, limitValue) = item.GetLimit(i)
					if item.LIMIT_REAL_TIME == limitType:
						bHasRealtimeFlag = 1
						remainTime = limitValue
				
				if 1 == bHasRealtimeFlag:
					realTime = metinSlot[0]
					if realTime == 0:
						self.AppendMallItemRealLastTime(remainTime)
					else:
						self.AppendMallItemLastTime(realTime)
		elif item.ITEM_TYPE_QUEST == itemType:
			if constinfo.IS_PET_SEAL_OLD(itemVnum):
				self.__AppendAffectInformation()
			
			for i in xrange(item.LIMIT_MAX_NUM):
				(limitType, limitValue) = item.GetLimit(i)

				if item.LIMIT_REAL_TIME == limitType:
					self.AppendMallItemLastTime(metinSlot[0])
			
		elif item.ITEM_TYPE_DS == itemType:
			self.AppendTextLine(self.__DragonSoulInfoString(itemVnum))
			self.__AppendAttributeInformation(attrSlot)
		else:
			self.__AppendLimitInformation()

		for i in xrange(item.LIMIT_MAX_NUM):
			(limitType, limitValue) = item.GetLimit(i)
			#dbg.TraceError("LimitType : %d, limitValue : %d" % (limitType, limitValue))

			if item.LIMIT_REAL_TIME_START_FIRST_USE == limitType:
				if gaya == 1:
					self.AppendRealTimeStartFirstUseLastTime(item, limitValue, i, True)
				else:
					self.AppendRealTimeStartFirstUseLastTime(item, metinSlot, i)
				#dbg.TraceError("2) REAL_TIME_START_FIRST_USE flag On ")

			elif item.LIMIT_TIMER_BASED_ON_WEAR == limitType:
				if gaya == 1:
					self.AppendTimerBasedOnWearLastTime(limitValue, True)
				else:
					self.AppendTimerBasedOnWearLastTime(metinSlot)
				#dbg.TraceError("1) REAL_TIME flag On ")
		
		if not wiki and (itemVnum == 50070 or itemVnum == 50076 or itemVnum == 27987 or itemVnum == 30094 or itemVnum == 30095 or itemVnum == 30096 or itemVnum == 10960 or itemVnum == 10961 or itemVnum == 10962 or itemVnum == 10963 or itemVnum == 10964 or itemVnum == 10965 or itemVnum == 10966 or itemVnum == 10967 or itemVnum == 10968 or itemVnum == 30097 or itemVnum == 30098 or itemVnum == 30099 or itemVnum == 30806 or itemVnum == 39048 or itemVnum == 39050 or itemVnum == 39052 or itemVnum == 39054 or itemVnum == 39065 or itemVnum == 39066 or itemVnum == 39067 or itemVnum == 39068 or itemVnum == 39069 or itemVnum == 39070 or itemVnum == 39071 or itemVnum == 39072 or itemVnum == 39073 or itemVnum == 39074 or itemVnum == 39075 or itemVnum == 39076 or itemVnum == 39077 or itemVnum == 39078 or itemVnum == 39079 or itemVnum == 39080 or itemVnum == 39081 or itemVnum == 39082 or itemVnum == 39083 or itemVnum == 50011 or itemVnum == 50063 or itemVnum == 50064 or itemVnum == 50065 or itemVnum == 50066 or itemVnum == 50069 or itemVnum == 50070 or itemVnum == 50071 or itemVnum == 50072 or itemVnum == 50073 or itemVnum == 50074 or itemVnum == 50075 or itemVnum == 50076 or itemVnum == 50077 or itemVnum == 50078 or itemVnum == 50079 or itemVnum == 50080 or itemVnum == 50081 or itemVnum == 50082 or itemVnum == 50097 or itemVnum == 50098 or itemVnum == 50099 or itemVnum == 50100 or itemVnum == 50101 or itemVnum == 50102 or itemVnum == 50138 or itemVnum == 50139 or itemVnum == 50140 or itemVnum == 50141 or itemVnum == 50142 or itemVnum == 50143 or itemVnum == 50203 or itemVnum == 50204 or itemVnum == 50254 or itemVnum == 50259 or itemVnum == 50266 or itemVnum == 50270 or itemVnum == 50271 or itemVnum == 50285 or itemVnum == 50323 or itemVnum == 50324 or itemVnum == 55009 or itemVnum == 70102 or itemVnum == 71203 or itemVnum == 71204 or itemVnum == 71205 or itemVnum == 71206 or itemVnum == 80003 or itemVnum == 80004 or itemVnum == 80005 or itemVnum == 80006 or itemVnum == 80007 or itemVnum == 80008):
			self.AppendTextLine(localeinfo.USE_200_ITEMS)



		if not wiki and itemVnum in CHEST_VNUMS:
			self.AppendTextLine(localeinfo.CHEST_LOOT_SEGGBE)


		#if not wiki:
		#	if (itemVnum in MountVnum) or (itemVnum in PetVnum) or (item.ITEM_TYPE_WEAPON == itemType and itemSubType != 6) or (item.ITEM_TYPE_ARMOR == itemType and itemSubType == 0) or (item.ITEM_TYPE_COSTUME == itemType and (itemSubType == 4 or itemSubType == 0 or itemSubType == 1 or itemSubType == 2)):
		#		self.AppendTextLine(localeinfo.VIEW_RENDERING)
		#	
		#	if app.ENABLE_STOLE_COSTUME and isCostumeStole:
		#		self.AppendTextLine(localeinfo.VIEW_RENDERING)
		#	
		#	if app.ENABLE_COSTUME_PET and isCostumePetSkin:
		#		self.AppendTextLine(localeinfo.VIEW_RENDERING)
		#	
		#	if app.ENABLE_COSTUME_MOUNT and isCostumeMountSkin:
		#		self.AppendTextLine(localeinfo.VIEW_RENDERING)
		#	
		#	if app.ENABLE_COSTUME_EFFECT and (isCostumeEffectBody or isCostumeEffectWeapon):
		#		self.AppendTextLine(localeinfo.VIEW_RENDERING)
		
		self.__AppendSealInformation(window_type, slotIndex)
		self.AppendAntiflagInformation()
		if chr.IsGameMaster(player.GetMainCharacterIndex()):
			self.AppendTextLine("Item Vnum: %i" % itemVnum, self.PRICE_COLOR)
		self.ShowToolTip()

	if app.ENABLE_BIOLOGIST_UI:
		def _BiologWindow__GetAffectString(self, type, value):
			return self.__GetAffectString(type, value)

		def _BiologWindow_Change__GetAffectString(self, type, value):
			return self.__GetAffectString(type, value)

	def __DragonSoulInfoString (self, dwVnum):
		step = (dwVnum / 100) % 10
		refine = (dwVnum / 10) % 10
		if 0 == step:
			return localeinfo.DRAGON_SOUL_STEP_LEVEL1 + " " + localeinfo.DRAGON_SOUL_STRENGTH(refine)
		elif 1 == step:
			return localeinfo.DRAGON_SOUL_STEP_LEVEL2 + " " + localeinfo.DRAGON_SOUL_STRENGTH(refine)
		elif 2 == step:
			return localeinfo.DRAGON_SOUL_STEP_LEVEL3 + " " + localeinfo.DRAGON_SOUL_STRENGTH(refine)
		elif 3 == step:
			return localeinfo.DRAGON_SOUL_STEP_LEVEL4 + " " + localeinfo.DRAGON_SOUL_STRENGTH(refine)
		elif 4 == step:
			return localeinfo.DRAGON_SOUL_STEP_LEVEL5 + " " + localeinfo.DRAGON_SOUL_STRENGTH(refine)
		else:
			return ""

	def __IsHair(self, itemVnum):
		if app.ENABLE_DS_GRADE_MYTH:
			return (self.__IsOldHair(itemVnum) or 
			self.__IsNewHair(itemVnum) or
			self.__IsNewHair2(itemVnum) or
			self.__IsNewHair3(itemVnum))
		else:
			return (self.__IsOldHair(itemVnum) or 
			self.__IsNewHair(itemVnum) or
			self.__IsNewHair2(itemVnum) or
			self.__IsNewHair3(itemVnum) or
			self.__IsCostumeHair(itemVnum)
			)

	if not app.ENABLE_DS_GRADE_MYTH:
		def __IsCostumeHair(self, itemVnum):
			return self.__IsNewHair3(itemVnum - 100000)

	def __IsOldHair(self, itemVnum):
		return itemVnum > 73000 and itemVnum < 74000

	def __IsNewHair(self, itemVnum):
		return itemVnum > 74000 and itemVnum < 75000

	def __IsNewHair2(self, itemVnum):
		return itemVnum > 75000 and itemVnum < 76000

	def __IsNewHair3(self, itemVnum):
		return ((74012 < itemVnum and itemVnum < 74022) or
			(74262 < itemVnum and itemVnum < 74272) or
			(74512 < itemVnum and itemVnum < 74522) or
			(74544 < itemVnum and itemVnum < 74560) or
			(74762 < itemVnum and itemVnum < 74772) or
			(45000 < itemVnum and itemVnum < 47000))

	def __IsCostumeHair(self, itemVnum):
		return app.ENABLE_COSTUME_SYSTEM and self.__IsNewHair3(itemVnum - 100000)

	def __AppendHairIcon(self, itemVnum):
		itemImage = ui.ImageBox()
		itemImage.SetParent(self)
		itemImage.Show()
		
		if self.__IsOldHair(itemVnum):
			itemImage.LoadImage("d:/ymir work/item/quest/"+str(itemVnum)+".tga")
		elif self.__IsNewHair3(itemVnum):
			itemImage.LoadImage("icon/hair/%d.sub" % (itemVnum))
		elif self.__IsNewHair(itemVnum): # ���� ���� ������ ���������� ��������. ������ �������� 1000���� ������ ������.
			itemImage.LoadImage("d:/ymir work/item/quest/"+str(itemVnum-1000)+".tga")
		elif self.__IsNewHair2(itemVnum):
			itemImage.LoadImage("icon/hair/%d.sub" % (itemVnum))
		elif not app.ENABLE_DS_GRADE_MYTH and self.__IsCostumeHair(itemVnum):
			itemImage.LoadImage("icon/hair/%d.sub" % (itemVnum - 100000))
		
		itemImage.SetPosition(itemImage.GetWidth()/2, self.toolTipHeight)
		self.toolTipHeight += itemImage.GetHeight()
		#self.toolTipWidth += itemImage.GetWidth()/2
		self.childrenList.append(itemImage)
		self.ResizeToolTip()

	#def __ItemGetRace(self, preview = 0, sexFlags = 0):
	#	race = 0
	#
	#	if item.IsAntiFlag(item.ITEM_ANTIFLAG_ASSASSIN) and item.IsAntiFlag(item.ITEM_ANTIFLAG_SURA) and item.IsAntiFlag(item.ITEM_ANTIFLAG_SHAMAN):
	#		race = 9
	#	elif item.IsAntiFlag(item.ITEM_ANTIFLAG_WARRIOR) and item.IsAntiFlag(item.ITEM_ANTIFLAG_SURA) and item.IsAntiFlag(item.ITEM_ANTIFLAG_SHAMAN):
	#		race = 1
	#	elif item.IsAntiFlag(item.ITEM_ANTIFLAG_WARRIOR) and item.IsAntiFlag(item.ITEM_ANTIFLAG_ASSASSIN) and item.IsAntiFlag(item.ITEM_ANTIFLAG_SHAMAN):
	#		race = 2
	#	elif item.IsAntiFlag(item.ITEM_ANTIFLAG_WARRIOR) and item.IsAntiFlag(item.ITEM_ANTIFLAG_ASSASSIN) and item.IsAntiFlag(item.ITEM_ANTIFLAG_SURA):
	#		race = 3
	#
	#	sex = chr.RaceToSex(player.GetRace())
	#	MALE = 1
	#	FEMALE = 0
	#	if preview:
	#		if race == 0:
	#			race = player.GetRace()
	#			if race == 0 or race == 4:
	#				if sex == MALE:
	#					race = 0
	#				else:
	#					race = 4
	#			elif race == 1 or race == 5:
	#				if sex == MALE:
	#					race = 5
	#				else:
	#					race = 1
	#			elif race == 2 or race == 6:
	#				if sex == MALE:
	#					race = 2
	#				else:
	#					race = 6
	#			elif race == 3 or race == 7:
	#				if sex == MALE:
	#					race = 7
	#				else:
	#					race = 3
	#		elif race == 9:
	#			if sex == MALE:
	#				race = 0
	#			else:
	#				race = 4
	#		elif race == 1 and sex == MALE:
	#			race = 5
	#		elif race == 2 and sex == FEMALE:
	#			race = 6
	#		elif race == 3 and sex == MALE:
	#			race = 7
	#		
	#		if sexFlags:
	#			if item.IsAntiFlag(item.ITEM_ANTIFLAG_MALE):
	#				if race == 0:
	#					race = 4
	#				elif race == 2:
	#					race = 6
	#				elif race == 5:
	#					race = 1
	#				elif race == 7:
	#					race = 3
	#			elif item.IsAntiFlag(item.ITEM_ANTIFLAG_FEMALE):
	#				if race == 4:
	#					race = 0
	#				elif race == 6:
	#					race = 2
	#				elif race == 1:
	#					race = 5
	#				elif race == 3:
	#					race = 7
	#	else:
	#		if item.IsAntiFlag(item.ITEM_ANTIFLAG_MALE) and sex == MALE:
	#			race = player.GetRace() + 4
	#		
	#		if item.IsAntiFlag(item.ITEM_ANTIFLAG_FEMALE) and sex == FEMALE:
	#			race = player.GetRace()
	#		
	#		if race == 0:
	#			race = player.GetRace()
	#		
	#		if race == 9:
	#			race = 0
	#		
	#	return race

	def __SetSkillBookToolTip(self, skillIndex, bookName, skillGrade):
		skillName = skill.GetSkillName(skillIndex)

		if not skillName:
			return

		if localeinfo.IsVIETNAM():
			itemName = bookName + " " + skillName
		else:
			itemName = skillName + " " + bookName
		self.SetTitle(itemName)

	def __AppendPickInformation(self, curLevel, curEXP, maxEXP):
		self.AppendSpace(5)
		self.AppendTextLine(localeinfo.TOOLTIP_PICK_LEVEL % (curLevel), self.NORMAL_COLOR)
		self.AppendTextLine(localeinfo.TOOLTIP_PICK_EXP % (curEXP, maxEXP), self.NORMAL_COLOR)

		if curEXP == maxEXP:
			self.AppendSpace(5)
			self.AppendTextLine(localeinfo.TOOLTIP_PICK_UPGRADE1, self.NORMAL_COLOR)
			self.AppendTextLine(localeinfo.TOOLTIP_PICK_UPGRADE2, self.NORMAL_COLOR)
			self.AppendTextLine(localeinfo.TOOLTIP_PICK_UPGRADE3, self.NORMAL_COLOR)


	def __AppendRodInformation(self, itemVnum, curLevel, curEXP, maxEXP):
		self.AppendSpace(5)
		self.AppendTextLine(localeinfo.TOOLTIP_FISHINGROD_LEVEL % (curLevel), self.NORMAL_COLOR)
		if itemVnum >= 27400 and itemVnum <= 27490:
			self.AppendTextLine(localeinfo.TOOLTIP_ROD_FISHING % ((curLevel) * 2), self.NORMAL_COLOR)
		else:
			self.AppendTextLine(localeinfo.TOOLTIP_ROD_FISHING % (curLevel), self.NORMAL_COLOR)
		
		if maxEXP != 0 and itemVnum != 27390:
			self.AppendTextLine(localeinfo.TOOLTIP_FISHINGROD_EXP % (curEXP, maxEXP), self.NORMAL_COLOR)

		if curEXP == maxEXP:
			if itemVnum != 27390:
				self.AppendSpace(5)
			
			if itemVnum == 27490:
				self.AppendTextLine(localeinfo.TOOLTIP_FISHINGROD_UPGRADE4, self.NORMAL_COLOR)
				self.AppendTextLine(localeinfo.TOOLTIP_FISHINGROD_UPGRADE5, self.NORMAL_COLOR)
				self.AppendTextLine(localeinfo.TOOLTIP_FISHINGROD_UPGRADE6, self.NORMAL_COLOR)
			elif itemVnum == 27390:
				self.AppendTextLine(localeinfo.TOOLTIP_FISHINGROD_UPGRADE7, self.NORMAL_COLOR)
			else:
				self.AppendTextLine(localeinfo.TOOLTIP_FISHINGROD_UPGRADE1, self.NORMAL_COLOR)
				self.AppendTextLine(localeinfo.TOOLTIP_FISHINGROD_UPGRADE2, self.NORMAL_COLOR)
				self.AppendTextLine(localeinfo.TOOLTIP_FISHINGROD_UPGRADE3, self.NORMAL_COLOR)

	def __AppendLimitInformation(self):

		appendSpace = False

		for i in xrange(item.LIMIT_MAX_NUM):

			(limitType, limitValue) = item.GetLimit(i)

			if limitValue > 0:
				if False == appendSpace:
					self.AppendSpace(5)
					appendSpace = True

			else:
				continue

			if item.LIMIT_LEVEL == limitType:
				color = self.GetLimitTextLineColor(player.GetStatus(player.LEVEL), limitValue)
				self.AppendTextLine(localeinfo.TOOLTIP_ITEM_LIMIT_LEVEL % (limitValue), color)
			"""
			elif item.LIMIT_STR == limitType:
				color = self.GetLimitTextLineColor(player.GetStatus(player.ST), limitValue)
				self.AppendTextLine(localeinfo.TOOLTIP_ITEM_LIMIT_STR % (limitValue), color)
			elif item.LIMIT_DEX == limitType:
				color = self.GetLimitTextLineColor(player.GetStatus(player.DX), limitValue)
				self.AppendTextLine(localeinfo.TOOLTIP_ITEM_LIMIT_DEX % (limitValue), color)
			elif item.LIMIT_INT == limitType:
				color = self.GetLimitTextLineColor(player.GetStatus(player.IQ), limitValue)
				self.AppendTextLine(localeinfo.TOOLTIP_ITEM_LIMIT_INT % (limitValue), color)
			elif item.LIMIT_CON == limitType:
				color = self.GetLimitTextLineColor(player.GetStatus(player.HT), limitValue)
				self.AppendTextLine(localeinfo.TOOLTIP_ITEM_LIMIT_CON % (limitValue), color)
			"""

	## cyh itemseal 2013 11 11
	def __AppendSealInformation(self, window_type, slotIndex):
		if not app.ENABLE_SOULBIND_SYSTEM:
			return

		itemSealDate = player.GetItemSealDate(window_type, slotIndex)
		if itemSealDate == item.GetDefaultSealDate():
			return

		if itemSealDate == item.GetUnlimitedSealDate():
			self.AppendSpace(5)
			self.AppendTextLine(localeinfo.TOOLTIP_SEALED, self.NEGATIVE_COLOR)

		elif itemSealDate > 0:
			self.AppendSpace(5)
			hours, minutes = player.GetItemUnSealLeftTime(window_type, slotIndex)
			self.AppendTextLine(localeinfo.TOOLTIP_UNSEAL_LEFT_TIME % (hours, minutes), self.NEGATIVE_COLOR)

	def _PetSystemMain__GetAffectString(self, affectType, affectValue):
		if 0 == affectType:
			return None

		if 0 == affectValue:
			return None

		try:
			return self.AFFECT_DICT[affectType](affectValue)
		except TypeError:
			return "UNKNOWN_VALUE[%s] %s" % (affectType, affectValue)
		except KeyError:
			return "UNKNOWN_TYPE[%s] %s" % (affectType, affectValue)

	def _DungeonInfo__GetAffectString(self, affectType, affectValue):
		if 0 == affectType:
			return None

		if 0 == affectValue:
			return None

		try:
			return self.AFFECT_DICT[affectType](affectValue)
		except TypeError:
			return "UNKNOWN_VALUE[%s] %s" % (affectType, affectValue)
		except KeyError:
			return "UNKNOWN_TYPE[%s] %s" % (affectType, affectValue)

	def __GetAffectString(self, affectType, affectValue):
		if 0 == affectType:
			return None

		if 0 == affectValue:
			return None

		try:
			return self.AFFECT_DICT[affectType](affectValue)
		except TypeError:
			return "UNKNOWN_VALUE[%s] %s" % (affectType, affectValue)
		except KeyError:
			return "UNKNOWN_TYPE[%s] %s" % (affectType, affectValue)

	def __AppendAffectInformation(self):
		for i in xrange(item.ITEM_APPLY_MAX_NUM):

			(affectType, affectValue) = item.GetAffect(i)

			affectString = self.__GetAffectString(affectType, affectValue)
			if affectString:
				_affColor = self.GetChangeTextLineColor(affectValue)
				# max bonus highlight (Affect list) - optional (disabled by default)
				if self.MAX_BONUS_HIGHLIGHT_AFFECT_LIST and self.MAX_BONUS_HIGHLIGHT_ENABLED and affectValue > 0:
					_maxV = self.__GetMaxBonusValue(affectType)
					if _maxV is not None and affectValue == _maxV:
						_affColor = self.MAX_BONUS_HIGHLIGHT_COLOR
				self.AppendTextLine(affectString, _affColor)
		
		if app.ENABLE_NEW_EXTRA_BONUS:
			for i in xrange(item.NEW_EXTRA_BONUS_COUNT):
				(pointType, pointCount) = item.GetExtraBonus(i)
				if pointType != item.APPLY_NONE:
					affectString = self.__GetAffectString(pointType, pointCount)
					self.AppendTextLine(affectString, self.NEW_EXTRA_BONUS_COLOR)

	def AppendWearableInformation(self):

		self.AppendSpace(5)
		self.AppendTextLine(localeinfo.TOOLTIP_ITEM_WEARABLE_JOB, self.NORMAL_COLOR)

		flagList = (
			not item.IsAntiFlag(item.ITEM_ANTIFLAG_WARRIOR),
			not item.IsAntiFlag(item.ITEM_ANTIFLAG_ASSASSIN),
			not item.IsAntiFlag(item.ITEM_ANTIFLAG_SURA),
			not item.IsAntiFlag(item.ITEM_ANTIFLAG_SHAMAN))
		if app.ENABLE_WOLFMAN_CHARACTER:
			flagList += (not item.IsAntiFlag(item.ITEM_ANTIFLAG_WOLFMAN),)
		characterNames = ""
		for i in xrange(self.CHARACTER_COUNT):

			name = self.CHARACTER_NAMES[i]
			flag = flagList[i]

			if flag:
				characterNames += " "
				characterNames += name

		textLine = self.AppendTextLine(characterNames, self.NORMAL_COLOR, True)
		textLine.SetFeather()

		if item.IsAntiFlag(item.ITEM_ANTIFLAG_MALE):
			textLine = self.AppendTextLine(localeinfo.FOR_FEMALE, self.NORMAL_COLOR, True)
			textLine.SetFeather()

		if item.IsAntiFlag(item.ITEM_ANTIFLAG_FEMALE):
			textLine = self.AppendTextLine(localeinfo.FOR_MALE, self.NORMAL_COLOR, True)
			textLine.SetFeather()

	def __AppendPotionInformation(self):
		self.AppendSpace(5)

		healHP = item.GetValue(0)
		healSP = item.GetValue(1)
		healStatus = item.GetValue(2)
		healPercentageHP = item.GetValue(3)
		healPercentageSP = item.GetValue(4)

		if healHP > 0:
			self.AppendTextLine(localeinfo.TOOLTIP_POTION_PLUS_HP_POINT % healHP, self.GetChangeTextLineColor(healHP))
		if healSP > 0:
			self.AppendTextLine(localeinfo.TOOLTIP_POTION_PLUS_SP_POINT % healSP, self.GetChangeTextLineColor(healSP))
		if healStatus != 0:
			self.AppendTextLine(localeinfo.TOOLTIP_POTION_CURE)
		if healPercentageHP > 0:
			self.AppendTextLine(localeinfo.TOOLTIP_POTION_PLUS_HP_PERCENT % healPercentageHP, self.GetChangeTextLineColor(healPercentageHP))
		if healPercentageSP > 0:
			self.AppendTextLine(localeinfo.TOOLTIP_POTION_PLUS_SP_PERCENT % healPercentageSP, self.GetChangeTextLineColor(healPercentageSP))

	def __AppendAbilityPotionInformation(self):

		self.AppendSpace(5)

		abilityType = item.GetValue(0)
		time = item.GetValue(1)
		point = item.GetValue(2)

		if abilityType == item.APPLY_ATT_SPEED:
			self.AppendTextLine(localeinfo.TOOLTIP_POTION_PLUS_ATTACK_SPEED % point, self.GetChangeTextLineColor(point))
		elif abilityType == item.APPLY_MOV_SPEED:
			self.AppendTextLine(localeinfo.TOOLTIP_POTION_PLUS_MOVING_SPEED % point, self.GetChangeTextLineColor(point))

		if time > 0:
			minute = (time / 60)
			second = (time % 60)
			timeString = localeinfo.TOOLTIP_POTION_TIME

			if minute > 0:
				timeString += str(minute) + localeinfo.TOOLTIP_POTION_MIN
			if second > 0:
				timeString += " " + str(second) + localeinfo.TOOLTIP_POTION_SEC

			self.AppendTextLine(timeString)

	def GetPriceColor(self, price):
			return self.LOW_PRICE_COLOR

	def AppendPrice(self, price):
		if not app.ENABLE_BUY_WITH_ITEM:
			self.AppendSpace(5)
		
		self.AppendTextLine(localeinfo.TOOLTIP_BUYPRICE  % (localeinfo.NumberToMoneyString(price)), self.GetPriceColor(price))

	def AppendPriceBySecondaryCoin(self, price):
		self.AppendSpace(5)
		self.AppendTextLine(localeinfo.TOOLTIP_BUYPRICE  % (localeinfo.NumberToSecondaryCoinString(price)), self.GetPriceColor(price))

	def AppendSellingPrice(self, price):
		if item.IsAntiFlag(item.ITEM_ANTIFLAG_SELL):
			self.AppendTextLine(localeinfo.TOOLTIP_ANTI_SELL, self.DISABLE_COLOR)
			self.AppendSpace(5)
		else:
			self.AppendTextLine(localeinfo.TOOLTIP_SELLPRICE % (localeinfo.NumberToMoneyString(price)), self.GetPriceColor(price))
			self.AppendSpace(5)

	def AppendMetinInformation(self):
		if constinfo.ENABLE_FULLSTONE_DETAILS:
			for i in xrange(item.ITEM_APPLY_MAX_NUM):
				(affectType, affectValue) = item.GetAffect(i)
				affectString = self.__GetAffectString(affectType, affectValue)
				if affectString:
					self.AppendSpace(5)
					self.AppendTextLine(affectString, self.GetChangeTextLineColor(affectValue))

	def AppendMetinWearInformation(self):

		self.AppendSpace(5)
		self.AppendTextLine(localeinfo.TOOLTIP_SOCKET_REFINABLE_ITEM, self.NORMAL_COLOR)

		flagList = (item.IsWearableFlag(item.WEARABLE_BODY),
					item.IsWearableFlag(item.WEARABLE_HEAD),
					item.IsWearableFlag(item.WEARABLE_FOOTS),
					item.IsWearableFlag(item.WEARABLE_WRIST),
					item.IsWearableFlag(item.WEARABLE_WEAPON),
					item.IsWearableFlag(item.WEARABLE_NECK),
					item.IsWearableFlag(item.WEARABLE_EAR),
					item.IsWearableFlag(item.WEARABLE_UNIQUE),
					item.IsWearableFlag(item.WEARABLE_SHIELD),
					item.IsWearableFlag(item.WEARABLE_ARROW))

		wearNames = ""
		for i in xrange(self.WEAR_COUNT):

			name = self.WEAR_NAMES[i]
			flag = flagList[i]

			if flag:
				wearNames += "  "
				wearNames += name

		textLine = ui.TextLine()
		textLine.SetParent(self)
		textLine.SetFontName(self.defFontName)
		textLine.SetPosition(self.toolTipWidth/2, self.toolTipHeight)
		textLine.SetHorizontalAlignCenter()
		textLine.SetPackedFontColor(self.NORMAL_COLOR)
		textLine.SetText(wearNames)
		textLine.Show()
		self.childrenList.append(textLine)

		self.toolTipHeight += self.TEXT_LINE_HEIGHT
		self.ResizeToolTip()

	def GetMetinSocketType(self, number):
		if player.METIN_SOCKET_TYPE_NONE == number:
			return player.METIN_SOCKET_TYPE_NONE
		elif player.METIN_SOCKET_TYPE_SILVER == number:
			return player.METIN_SOCKET_TYPE_SILVER
		elif player.METIN_SOCKET_TYPE_GOLD == number:
			return player.METIN_SOCKET_TYPE_GOLD
		else:
			item.SelectItem(number)
			if item.METIN_NORMAL == item.GetItemSubType():
				return player.METIN_SOCKET_TYPE_SILVER
			elif item.METIN_GOLD == item.GetItemSubType():
				return player.METIN_SOCKET_TYPE_GOLD
			elif "USE_PUT_INTO_ACCESSORY_SOCKET" == item.GetUseType(number):
				return player.METIN_SOCKET_TYPE_SILVER
			elif "USE_PUT_INTO_RING_SOCKET" == item.GetUseType(number):
				return player.METIN_SOCKET_TYPE_SILVER
			elif "USE_PUT_INTO_BELT_SOCKET" == item.GetUseType(number):
				return player.METIN_SOCKET_TYPE_SILVER

		return player.METIN_SOCKET_TYPE_NONE

	def GetMetinItemIndex(self, number):
		if player.METIN_SOCKET_TYPE_SILVER == number:
			return 0
		if player.METIN_SOCKET_TYPE_GOLD == number:
			return 0

		return number

	def __AppendAccessoryMetinSlotInfo(self, metinSlot, mtrlVnum):
		ACCESSORY_SOCKET_MAX_SIZE = 3

		cur=min(metinSlot[0], ACCESSORY_SOCKET_MAX_SIZE)
		end=min(metinSlot[1], ACCESSORY_SOCKET_MAX_SIZE)

		affectType1, affectValue1 = item.GetAffect(0)
		affectList1=[0, max(1, affectValue1*10/100), max(2, affectValue1*20/100), max(3, affectValue1*40/100)]

		affectType2, affectValue2 = item.GetAffect(1)
		affectList2=[0, max(1, affectValue2*10/100), max(2, affectValue2*20/100), max(3, affectValue2*40/100)]

		affectType3, affectValue3 = item.GetAffect(2)
		affectList3=[0, max(1, affectValue3*10/100), max(2, affectValue3*20/100), max(3, affectValue3*40/100)]

		mtrlPos=0
		mtrlList=[mtrlVnum]*cur+[player.METIN_SOCKET_TYPE_SILVER]*(end-cur)
		
		for mtrl in mtrlList:
			affectString1 = self.__GetAffectString(affectType1, affectList1[mtrlPos+1]-affectList1[mtrlPos])
			affectString2 = self.__GetAffectString(affectType2, affectList2[mtrlPos+1]-affectList2[mtrlPos])
			affectString3 = self.__GetAffectString(affectType3, affectList3[mtrlPos+1]-affectList3[mtrlPos])

			leftTime = 0
			if cur == mtrlPos+1:
				leftTime=metinSlot[2]

			self.__AppendMetinSlotInfo_AppendMetinSocketData(mtrlPos, mtrl, affectString1, affectString2, affectString3, leftTime)
			mtrlPos+=1

	def __AppendMetinSlotInfo(self, metinSlot):
		if self.__AppendMetinSlotInfo_IsEmptySlotList(metinSlot):
			return

		for i in xrange(player.METIN_SOCKET_MAX_NUM):
			self.__AppendMetinSlotInfo_AppendMetinSocketData(i, metinSlot[i])

	def __AppendMetinSlotInfo_IsEmptySlotList(self, metinSlot):
		if 0 == metinSlot:
			return 1

		for i in xrange(player.METIN_SOCKET_MAX_NUM):
			metinSlotData=metinSlot[i]
			if 0 != self.GetMetinSocketType(metinSlotData):
				if 0 != self.GetMetinItemIndex(metinSlotData):
					return 0

		return 1

	def __AppendMetinSlotInfo_AppendMetinSocketData(self, index, metinSlotData, custumAffectString="", custumAffectString2="", custumAffectString3="", leftTime=0):

		slotType = self.GetMetinSocketType(metinSlotData)
		itemIndex = self.GetMetinItemIndex(metinSlotData)

		if 0 == slotType:
			return

		self.AppendSpace(5)

		slotImage = ui.ImageBox()
		slotImage.SetParent(self)
		slotImage.Show()

		## Name
		nameTextLine = ui.TextLine()
		nameTextLine.SetParent(self)
		nameTextLine.SetFontName(self.defFontName)
		nameTextLine.SetPackedFontColor(self.NORMAL_COLOR)
		nameTextLine.SetOutline()
		nameTextLine.SetFeather()
		nameTextLine.Show()
		stoneList = len(self.childrenList) - 1
		self.childrenList.append(nameTextLine)
		self.childrenListStone.append(stoneList + 1)
		if player.METIN_SOCKET_TYPE_SILVER == slotType:
			slotImage.LoadImage("d:/ymir work/ui/game/windows/metin_slot_silver.sub")
		elif player.METIN_SOCKET_TYPE_GOLD == slotType:
			slotImage.LoadImage("d:/ymir work/ui/game/windows/metin_slot_gold.sub")

		self.childrenList.append(slotImage)
		self.childrenListStone.append(stoneList + 2)
		if localeinfo.IsARABIC():
			slotImage.SetPosition(self.toolTipWidth - slotImage.GetWidth() - 9, self.toolTipHeight-1)
			nameTextLine.SetPosition(self.toolTipWidth - 50, self.toolTipHeight + 2)
		else:
			slotImage.SetPosition(9, self.toolTipHeight-1)
			nameTextLine.SetPosition(50, self.toolTipHeight + 2)

		metinImage = ui.ImageBox()
		metinImage.SetParent(self)
		metinImage.Show()
		self.childrenList.append(metinImage)
		self.childrenListStone.append(stoneList + 3)
		if itemIndex:

			item.SelectItem(itemIndex)

			## Image
			try:
				metinImage.LoadImage(item.GetIconImageFileName())
			except:
				dbg.TraceError("ItemToolTip.__AppendMetinSocketData() - Failed to find image file %d:%s" %
					(itemIndex, item.GetIconImageFileName())
				)

			nameTextLine.SetText(item.GetItemName())

			## Affect
			affectTextLine = ui.TextLine()
			affectTextLine.SetParent(self)
			affectTextLine.SetFontName(self.defFontName)
			affectTextLine.SetPackedFontColor(self.POSITIVE_COLOR)
			affectTextLine.SetOutline()
			affectTextLine.SetFeather()
			affectTextLine.Show()

			if localeinfo.IsARABIC():
				metinImage.SetPosition(self.toolTipWidth - metinImage.GetWidth() - 10, self.toolTipHeight)
				affectTextLine.SetPosition(self.toolTipWidth - 50, self.toolTipHeight + 16 + 2)
			else:
				metinImage.SetPosition(10, self.toolTipHeight)
				affectTextLine.SetPosition(50, self.toolTipHeight + 16 + 2)

			if custumAffectString:
				affectTextLine.SetText(custumAffectString)
			elif itemIndex!=constinfo.ERROR_METIN_STONE:
				affectType, affectValue = item.GetAffect(0)
				affectString = self.__GetAffectString(affectType, affectValue)
				if affectString:
					affectTextLine.SetText(affectString)
			else:
				affectTextLine.SetText(localeinfo.TOOLTIP_APPLY_NOAFFECT)

			self.childrenList.append(affectTextLine)
			self.childrenListStone.append(stoneList + 4)
			if constinfo.ENABLE_FULLSTONE_DETAILS and (not custumAffectString2) and (itemIndex!=constinfo.ERROR_METIN_STONE):
				custumAffectString2 = self.__GetAffectString(*item.GetAffect(1))

			if custumAffectString2:
				affectTextLine = ui.TextLine()
				affectTextLine.SetParent(self)
				affectTextLine.SetFontName(self.defFontName)
				affectTextLine.SetPackedFontColor(self.POSITIVE_COLOR)
				affectTextLine.SetPosition(50, self.toolTipHeight + 16 + 2 + 16 + 2)
				affectTextLine.SetOutline()
				affectTextLine.SetFeather()
				affectTextLine.Show()
				affectTextLine.SetText(custumAffectString2)
				self.childrenList.append(affectTextLine)
				self.childrenListStone.append(stoneList + 5)
				self.toolTipHeight += 16 + 2

			if constinfo.ENABLE_FULLSTONE_DETAILS and (not custumAffectString3) and (itemIndex!=constinfo.ERROR_METIN_STONE):
				custumAffectString3 = self.__GetAffectString(*item.GetAffect(2))

			if custumAffectString3:
				affectTextLine = ui.TextLine()
				affectTextLine.SetParent(self)
				affectTextLine.SetFontName(self.defFontName)
				affectTextLine.SetPackedFontColor(self.POSITIVE_COLOR)
				affectTextLine.SetPosition(50, self.toolTipHeight + 16 + 2 + 16 + 2)
				affectTextLine.SetOutline()
				affectTextLine.SetFeather()
				affectTextLine.Show()
				affectTextLine.SetText(custumAffectString3)
				self.childrenList.append(affectTextLine)
				self.childrenListStone.append(stoneList + 6)
				self.toolTipHeight += 16 + 2

			if 0 != leftTime:
				if app.ENABLE_INFINITE_RAFINES and leftTime > 86400:
					self.toolTipHeight += 35
					self.ResizeToolTip()
					return
				
				timeText = (localeinfo.LEFT_TIME + " : " + localeinfo.SecondToDHM(leftTime))

				timeTextLine = ui.TextLine()
				timeTextLine.SetParent(self)
				timeTextLine.SetFontName(self.defFontName)
				timeTextLine.SetPackedFontColor(self.POSITIVE_COLOR)
				timeTextLine.SetPosition(50, self.toolTipHeight + 16 + 2 + 16 + 2)
				timeTextLine.SetOutline()
				timeTextLine.SetFeather()
				timeTextLine.Show()
				timeTextLine.SetText(timeText)
				self.childrenList.append(timeTextLine)
				self.childrenListStone.append(stoneList + 7)
				self.toolTipHeight += 16 + 2
		else:
			nameTextLine.SetText(localeinfo.TOOLTIP_SOCKET_EMPTY)

		self.toolTipHeight += 35
		self.ResizeToolTip()

	def __AppendFishInfo(self, size):
		if size > 0:
			self.AppendSpace(5)
			self.AppendTextLine(localeinfo.TOOLTIP_FISH_LEN % (float(size) / 100.0), self.NORMAL_COLOR)

	def AppendUniqueItemLastTime(self, restMin):
		restSecond = restMin*60
		self.AppendSpace(5)
		self.AppendTextLine(localeinfo.LEFT_TIME + " : " + localeinfo.SecondToDHM(restSecond), self.NORMAL_COLOR)

	def AppendMallItemRealLastTime(self, leftSec):
		self.AppendSpace(5)
		self.AppendTextLine(localeinfo.LEFT_TIME + " : " + localeinfo.SecondToDHM(leftSec), self.NORMAL_COLOR)

	def AppendMallItemLastTime(self, endTime):
		leftSec = max(0, endTime - app.GetGlobalTimeStamp())
		self.AppendSpace(5)
		self.AppendTextLine(localeinfo.LEFT_TIME + " : " + localeinfo.SecondToDHM(leftSec), self.NORMAL_COLOR)

	def AppendTimerBasedOnWearLastTime(self, metinSlot, gaya = False):
		if gaya == True:
			if metinSlot == 0:
				self.AppendSpace(5)
				self.AppendTextLine(localeinfo.CANNOT_USE, self.DISABLE_COLOR)
			else:
				self.AppendMallItemRealLastTime(metinSlot)
		else:
			if 0 == metinSlot[0]:
				self.AppendSpace(5)
				self.AppendTextLine(localeinfo.CANNOT_USE, self.DISABLE_COLOR)
			else:
				endTime = app.GetGlobalTimeStamp() + metinSlot[0]
				self.AppendMallItemLastTime(endTime)

	def AppendRealTimeStartFirstUseLastTime(self, item, metinSlot, limitIndex, gaya = False):
		if gaya == True:
			self.AppendMallItemRealLastTime(metinSlot)
		else:
			useCount = metinSlot[1]
			endTime = metinSlot[0]
			if 0 == useCount:
				if 0 == endTime:
					(limitType, limitValue) = item.GetLimit(limitIndex)
					endTime = limitValue
				endTime += app.GetGlobalTimeStamp()
			self.AppendMallItemLastTime(endTime)

	if app.ENABLE_ACCE_SYSTEM:
		def SetAcceResultItem(self, slotIndex, window_type = player.INVENTORY):
			(itemVnum, MinAbs, MaxAbs) = acce.GetResultItem()
			if not itemVnum:
				return
			
			self.ClearToolTip()
			
			metinSlot 	= [player.GetItemMetinSocket(window_type, slotIndex, i) 	for i in xrange(player.METIN_SOCKET_MAX_NUM)	]
			attrSlot 	= [player.GetItemAttribute(window_type, slotIndex, i) 		for i in xrange(player.ATTRIBUTE_SLOT_MAX_NUM)	]
			
			item.SelectItem(itemVnum)
			itemType = item.GetItemType()
			itemSubType = item.GetItemSubType()
			isCostumeStole = False
			if app.ENABLE_STOLE_COSTUME:
				if itemType != item.ITEM_TYPE_COSTUME and itemSubType != item.COSTUME_TYPE_ACCE and itemSubType != item.COSTUME_TYPE_STOLE:
					return
			else:
				if itemType != item.ITEM_TYPE_COSTUME and itemSubType != item.COSTUME_TYPE_ACCE:
					return
			
			absChance = MaxAbs
			itemDesc = item.GetItemDescription()
			self.__SetItemTitle(itemVnum, metinSlot, attrSlot)
			self.AppendSpace(5)
			if app.ENABLE_RARITY_SYSTEM:
				item.SelectItem(itemVnum)
				if itemSubType == item.COSTUME_TYPE_STOLE:
					itemRarity = item.GetRarity()
					rarityDesc = self.RARITY_DESC[itemRarity]
					self.AppendTextLine(rarityDesc, self.RARITY_COLOR[itemRarity])
					self.AppendSpace(5)
			
			self.AppendDescription(itemDesc, 26)
			self.AppendDescription(item.GetItemSummary(), 26, self.CONDITION_COLOR)
			if app.ENABLE_RARITY_SYSTEM:
				item.SelectItem(itemVnum)
				if itemSubType == item.COSTUME_TYPE_ACCE:
					itemRarity = item.GetRarity()
					rarityDesc = self.RARITY_DESC[itemRarity]
					self.AppendTextLine(rarityDesc, self.RARITY_COLOR[itemRarity])
			
			self.__AppendLimitInformation()
			
			if app.ENABLE_STOLE_COSTUME:
				if itemSubType == item.COSTUME_TYPE_STOLE:
					isCostumeStole = True
			
			## ABSORPTION RATE
			if MinAbs == MaxAbs:
				if isCostumeStole == False:
					self.AppendTextLine(localeinfo.SASH_ABSORB_CHANCE % (MinAbs), self.CONDITION_COLOR)
			else:
				if isCostumeStole == False:
					self.AppendTextLine(localeinfo.SASH_ABSORB_CHANCE2 % (MinAbs, MaxAbs), self.CONDITION_COLOR)
			## END ABSOPRTION RATE
			
			if isCostumeStole:
				item.SelectItem(itemVnum)
				self.AppendTextLine(localeinfo.GRADE_STOLA % (item.GetValue(0)), self.CONDITION_COLOR)
				self.AppendSpace(5)
			
			itemAbsorbedVnum = int(metinSlot[acce.ABSORBED_SOCKET])
			if itemAbsorbedVnum:
				## ATTACK / DEFENCE
				item.SelectItem(itemAbsorbedVnum)
				if item.GetItemType() == item.ITEM_TYPE_WEAPON:
					if item.GetItemSubType() == item.WEAPON_FAN:
						self.__AppendMagicAttackInfo(absChance)
						item.SelectItem(itemAbsorbedVnum)
						self.__AppendAttackPowerInfo(absChance)
					else:
						self.__AppendAttackPowerInfo(absChance)
						item.SelectItem(itemAbsorbedVnum)
						self.__AppendMagicAttackInfo(absChance)
				elif item.GetItemType() == item.ITEM_TYPE_ARMOR:
					defGrade = item.GetValue(1)
					defBonus = item.GetValue(5) * 2
					defGrade = self.CalcAcceValue(defGrade, absChance)
					defBonus = self.CalcAcceValue(defBonus, absChance)
					
					if defGrade > 0:
						self.AppendSpace(5)
						self.AppendTextLine(localeinfo.TOOLTIP_ITEM_DEF_GRADE % (defGrade + defBonus), self.GetChangeTextLineColor(defGrade))
					
					item.SelectItem(itemAbsorbedVnum)
					self.__AppendMagicDefenceInfo(absChance)
				## END ATTACK / DEFENCE
				
				## EFFECT
				item.SelectItem(itemAbsorbedVnum)
				for i in xrange(item.ITEM_APPLY_MAX_NUM):
					(affectType, affectValue) = item.GetAffect(i)
					affectValue = self.CalcAcceValue(affectValue, absChance)
					affectString = self.__GetAffectString(affectType, affectValue)
					if affectString and affectValue > 0:
						self.AppendTextLine(affectString, self.GetChangeTextLineColor(affectValue))
					
					item.SelectItem(itemAbsorbedVnum)
				# END EFFECT
				
			item.SelectItem(itemVnum)
			## ATTR
			self.__AppendAttributeInformation(attrSlot, MaxAbs)
			# END ATTR
			
			self.AppendWearableInformation()
			#self.AppendTextLine(localeinfo.VIEW_RENDERING)
			self.ShowToolTip()

		def SetAcceResultAbsItem(self, slotIndex1, slotIndex2, window_type = player.INVENTORY):
			itemVnumAcce = player.GetItemIndex(window_type, slotIndex1)
			itemVnumTarget = player.GetItemIndex(window_type, slotIndex2)
			if not itemVnumAcce or not itemVnumTarget:
				return
			
			self.ClearToolTip()
			
			item.SelectItem(itemVnumAcce)
			itemType = item.GetItemType()
			itemSubType = item.GetItemSubType()
			if itemType != item.ITEM_TYPE_COSTUME and itemSubType != item.COSTUME_TYPE_ACCE:
				return
			
			metinSlot = [player.GetItemMetinSocket(window_type, slotIndex1, i) for i in xrange(player.METIN_SOCKET_MAX_NUM)]
			attrSlot = [player.GetItemAttribute(window_type, slotIndex2, i) for i in xrange(player.ATTRIBUTE_SLOT_MAX_NUM)]
			
			itemDesc = item.GetItemDescription()
			self.__SetItemTitle(itemVnumAcce, metinSlot, attrSlot)
			self.AppendDescription(itemDesc, 26)
			self.AppendDescription(item.GetItemSummary(), 26, self.CONDITION_COLOR)
			item.SelectItem(itemVnumAcce)
			self.__AppendLimitInformation()
			
			## ABSORPTION RATE
			self.AppendTextLine(localeinfo.SASH_ABSORB_CHANCE % (metinSlot[acce.ABSORPTION_SOCKET]), self.CONDITION_COLOR)
			## END ABSOPRTION RATE
			
			## ATTACK / DEFENCE
			itemAbsorbedVnum = itemVnumTarget
			item.SelectItem(itemAbsorbedVnum)
			if item.GetItemType() == item.ITEM_TYPE_WEAPON:
				if item.GetItemSubType() == item.WEAPON_FAN:
					self.__AppendMagicAttackInfo(metinSlot[acce.ABSORPTION_SOCKET])
					item.SelectItem(itemAbsorbedVnum)
					self.__AppendAttackPowerInfo(metinSlot[acce.ABSORPTION_SOCKET])
				else:
					self.__AppendAttackPowerInfo(metinSlot[acce.ABSORPTION_SOCKET])
					item.SelectItem(itemAbsorbedVnum)
					self.__AppendMagicAttackInfo(metinSlot[acce.ABSORPTION_SOCKET])
			elif item.GetItemType() == item.ITEM_TYPE_ARMOR:
				defGrade = item.GetValue(1)
				defBonus = item.GetValue(5) * 2
				defGrade = self.CalcAcceValue(defGrade, metinSlot[acce.ABSORPTION_SOCKET])
				defBonus = self.CalcAcceValue(defBonus, metinSlot[acce.ABSORPTION_SOCKET])
				
				if defGrade > 0:
					self.AppendSpace(5)
					self.AppendTextLine(localeinfo.TOOLTIP_ITEM_DEF_GRADE % (defGrade + defBonus), self.GetChangeTextLineColor(defGrade))
				
				item.SelectItem(itemAbsorbedVnum)
				self.__AppendMagicDefenceInfo(metinSlot[acce.ABSORPTION_SOCKET])
			## END ATTACK / DEFENCE
			
			## EFFECT
			item.SelectItem(itemAbsorbedVnum)
			for i in xrange(item.ITEM_APPLY_MAX_NUM):
				(affectType, affectValue) = item.GetAffect(i)
				affectValue = self.CalcAcceValue(affectValue, metinSlot[acce.ABSORPTION_SOCKET])
				affectString = self.__GetAffectString(affectType, affectValue)
				if affectString and affectValue > 0:
					self.AppendTextLine(affectString, self.GetChangeTextLineColor(affectValue))
				
				item.SelectItem(itemAbsorbedVnum)
			## END EFFECT
			
			## ATTR
			item.SelectItem(itemAbsorbedVnum)
			for i in xrange(player.ATTRIBUTE_SLOT_MAX_NUM):
				type = attrSlot[i][0]
				value = attrSlot[i][1]
				if not value:
					continue
				
				value = self.CalcAcceValue(value, metinSlot[acce.ABSORPTION_SOCKET])
				affectString = self.__GetAffectString(type, value)
				if affectString and value > 0:
					affectColor = self.__GetAttributeColor(i, value)
					self.AppendTextLine(affectString, affectColor)
				
				item.SelectItem(itemAbsorbedVnum)
			## END ATTR
			
			## WEARABLE
			item.SelectItem(itemVnumAcce)
			self.AppendSpace(5)
			self.AppendTextLine(localeinfo.TOOLTIP_ITEM_WEARABLE_JOB, self.NORMAL_COLOR)
			
			item.SelectItem(itemVnumAcce)
			flagList = (
						not item.IsAntiFlag(item.ITEM_ANTIFLAG_WARRIOR),
						not item.IsAntiFlag(item.ITEM_ANTIFLAG_ASSASSIN),
						not item.IsAntiFlag(item.ITEM_ANTIFLAG_SURA),
						not item.IsAntiFlag(item.ITEM_ANTIFLAG_SHAMAN)
			)
			
			if app.ENABLE_WOLFMAN_CHARACTER:
				flagList += (not item.IsAntiFlag(item.ITEM_ANTIFLAG_WOLFMAN),)
			
			characterNames = ""
			for i in xrange(self.CHARACTER_COUNT):
				name = self.CHARACTER_NAMES[i]
				flag = flagList[i]
				if flag:
					characterNames += " "
					characterNames += name
			
			textLine = self.AppendTextLine(characterNames, self.NORMAL_COLOR, True)
			textLine.SetFeather()
			
			item.SelectItem(itemVnumAcce)
			if item.IsAntiFlag(item.ITEM_ANTIFLAG_MALE):
				textLine = self.AppendTextLine(localeinfo.FOR_FEMALE, self.NORMAL_COLOR, True)
				textLine.SetFeather()
			
			if item.IsAntiFlag(item.ITEM_ANTIFLAG_FEMALE):
				textLine = self.AppendTextLine(localeinfo.FOR_MALE, self.NORMAL_COLOR, True)
				textLine.SetFeather()
			## END WEARABLE
			
			self.ShowToolTip()
	
	
	

class HyperlinkItemToolTip(ItemToolTip):
	def __init__(self):
		ItemToolTip.__init__(self, isPickable=True)

	def SetHyperlinkItem(self, tokens):
		minTokenCount = 3 + player.METIN_SOCKET_MAX_NUM
		maxTokenCount = minTokenCount + 2 * player.ATTRIBUTE_SLOT_MAX_NUM
		if app.ENABLE_MULTI_LANGUAGE:
			maxTokenCount += 1
	
		if tokens and len(tokens) >= minTokenCount and len(tokens) <= maxTokenCount:
			head, vnum, flag = tokens[:3]
			itemVnum = int(vnum, 16)
			metinSlot = [int(metin, 16) for metin in tokens[3:6]]
	
			import dbg
			dbg.TraceError("SetHyperlinkItem() - sockets: 0=%d, 1=%d, 2=%d" % (
				metinSlot[0], metinSlot[1], metinSlot[2]))
	
			# --- OFFLINESHOP CLICKABLE LINK INTERCEPT ---
			# Encodings supported:
			#   - flag == 0x0BADF00D (tokens[2])
			#   - socket1 == 0x0BADF00D (metinSlot[1])
			# Owner id is socket0 (metinSlot[0]).
			try:
				_flag = int(flag, 16)
			except:
				_flag = 0
	
			SENTINEL = 0x0BADF00D
			if _flag == SENTINEL or metinSlot[1] == SENTINEL:
				try:
					import offlineshop
					owner_id = metinSlot[0]
					dbg.TraceError("OFFSHOP LINK -> opening shop for owner_id=%d (flag=%d socket1=%d)" % (owner_id, _flag, metinSlot[1]))
					offlineshop.SendOpenShop(owner_id)
				except Exception as e:
					try:
						dbg.TraceError("OFFSHOP LINK -> failed to open shop: %s" % str(e))
					except:
						pass
				try:
					self.Hide()
				except:
					pass
				return
			# --- END OFFLINESHOP INTERCEPT ---
	
			if app.ENABLE_MULTI_LANGUAGE:
				rests = tokens[7:]
			else:
				rests = tokens[6:]
	
			if rests:
				attrSlot = []
				rests.reverse()
				while rests:
					key = int(rests.pop(), 16)
					if rests:
						val = int(rests.pop())
						attrSlot.append((key, val))
	
				attrSlot += [(0, 0)] * (player.ATTRIBUTE_SLOT_MAX_NUM - len(attrSlot))
			else:
				attrSlot = [(0, 0)] * player.ATTRIBUTE_SLOT_MAX_NUM
	
			self.ClearToolTip()
			self.AddItemData(itemVnum, metinSlot, attrSlot, 1)
	
			ItemToolTip.OnUpdate(self)

	def OnUpdate(self):
		pass

	def OnMouseLeftButtonDown(self):
		self.Hide()

class SkillToolTip(ToolTip):

	POINT_NAME_DICT = {
		player.LEVEL : localeinfo.SKILL_TOOLTIP_LEVEL,
		player.IQ : localeinfo.SKILL_TOOLTIP_INT,
	}

	SKILL_TOOL_TIP_WIDTH = 200
	PARTY_SKILL_TOOL_TIP_WIDTH = 340

	PARTY_SKILL_EXPERIENCE_AFFECT_LIST = (	( 2, 2,  10,),
											( 8, 3,  20,),
											(14, 4,  30,),
											(22, 5,  45,),
											(28, 6,  60,),
											(34, 7,  80,),
											(38, 8, 100,), )

	PARTY_SKILL_PLUS_GRADE_AFFECT_LIST = (	( 4, 2, 1, 0,),
											(10, 3, 2, 0,),
											(16, 4, 2, 1,),
											(24, 5, 2, 2,), )

	PARTY_SKILL_ATTACKER_AFFECT_LIST = (	( 36, 3, ),
											( 26, 1, ),
											( 32, 2, ), )

	SKILL_GRADE_NAME = {	player.SKILL_GRADE_MASTER : localeinfo.SKILL_GRADE_NAME_MASTER,
							player.SKILL_GRADE_GRAND_MASTER : localeinfo.SKILL_GRADE_NAME_GRAND_MASTER,
							player.SKILL_GRADE_PERFECT_MASTER : localeinfo.SKILL_GRADE_NAME_PERFECT_MASTER, }

	AFFECT_NAME_DICT =	{
							"HP" : localeinfo.TOOLTIP_SKILL_AFFECT_ATT_POWER,
							"ATT_GRADE" : localeinfo.TOOLTIP_SKILL_AFFECT_ATT_GRADE,
							"DEF_GRADE" : localeinfo.TOOLTIP_SKILL_AFFECT_DEF_GRADE,
							"ATT_SPEED" : localeinfo.TOOLTIP_SKILL_AFFECT_ATT_SPEED,
							"MOV_SPEED" : localeinfo.TOOLTIP_SKILL_AFFECT_MOV_SPEED,
							"DODGE" : localeinfo.TOOLTIP_SKILL_AFFECT_DODGE,
							"RESIST_NORMAL" : localeinfo.TOOLTIP_SKILL_AFFECT_RESIST_NORMAL,
							"REFLECT_MELEE" : localeinfo.TOOLTIP_SKILL_AFFECT_REFLECT_MELEE,
						}
	AFFECT_APPEND_TEXT_DICT =	{
									"DODGE" : "%",
									"RESIST_NORMAL" : "%",
									"REFLECT_MELEE" : "%",
								}

	def __init__(self):
		ToolTip.__init__(self, self.SKILL_TOOL_TIP_WIDTH)
	def __del__(self):
		ToolTip.__del__(self)

	def SetSkill(self, skillIndex, skillLevel = -1):

		if 0 == skillIndex:
			return

		if skill.SKILL_TYPE_GUILD == skill.GetSkillType(skillIndex):

			if self.SKILL_TOOL_TIP_WIDTH != self.toolTipWidth:
				self.toolTipWidth = self.SKILL_TOOL_TIP_WIDTH
				self.ResizeToolTip()

			self.AppendDefaultData(skillIndex)
			self.AppendSkillConditionData(skillIndex)
			self.AppendGuildSkillData(skillIndex, skillLevel)

		else:

			if self.SKILL_TOOL_TIP_WIDTH != self.toolTipWidth:
				self.toolTipWidth = self.SKILL_TOOL_TIP_WIDTH
				self.ResizeToolTip()

			slotIndex = player.GetSkillSlotIndex(skillIndex)
			skillGrade = player.GetSkillGrade(slotIndex)
			skillLevel = player.GetSkillLevel(slotIndex)
			skillCurrentPercentage = player.GetSkillCurrentEfficientPercentage(slotIndex)
			skillNextPercentage = player.GetSkillNextEfficientPercentage(slotIndex)

			self.AppendDefaultData(skillIndex)
			self.AppendSkillConditionData(skillIndex)
			self.AppendSkillDataNew(slotIndex, skillIndex, skillGrade, skillLevel, skillCurrentPercentage, skillNextPercentage)
			self.AppendSkillRequirement(skillIndex, skillLevel)

		self.ShowToolTip()

	def SetSkillNew(self, slotIndex, skillIndex, skillGrade, skillLevel):

		if 0 == skillIndex:
			return

		if player.SKILL_INDEX_TONGSOL == skillIndex:

			slotIndex = player.GetSkillSlotIndex(skillIndex)
			skillLevel = player.GetSkillLevel(slotIndex)

			self.AppendDefaultData(skillIndex)
			self.AppendPartySkillData(skillGrade, skillLevel)

		elif player.SKILL_INDEX_RIDING == skillIndex:

			slotIndex = player.GetSkillSlotIndex(skillIndex)
			self.AppendSupportSkillDefaultData(skillIndex, skillGrade, skillLevel, 30)

		elif player.SKILL_INDEX_SUMMON == skillIndex:

			maxLevel = 10

			self.ClearToolTip()
			self.__SetSkillTitle(skillIndex, skillGrade)

			## Description
			description = skill.GetSkillDescription(skillIndex)
			self.AppendDescription(description, 25)

			if skillLevel == 10:
				self.AppendSpace(5)
				self.AppendTextLine(localeinfo.TOOLTIP_SKILL_LEVEL_MASTER % (skillLevel), self.NORMAL_COLOR)
				self.AppendTextLine(localeinfo.SKILL_SUMMON_DESCRIPTION % (skillLevel*10), self.NORMAL_COLOR)

			else:
				self.AppendSpace(5)
				self.AppendTextLine(localeinfo.TOOLTIP_SKILL_LEVEL % (skillLevel), self.NORMAL_COLOR)
				self.__AppendSummonDescription(skillLevel, self.NORMAL_COLOR)

				self.AppendSpace(5)
				self.AppendTextLine(localeinfo.TOOLTIP_SKILL_LEVEL % (skillLevel+1), self.NEGATIVE_COLOR)
				self.__AppendSummonDescription(skillLevel+1, self.NEGATIVE_COLOR)

		elif skill.SKILL_TYPE_GUILD == skill.GetSkillType(skillIndex):

			if self.SKILL_TOOL_TIP_WIDTH != self.toolTipWidth:
				self.toolTipWidth = self.SKILL_TOOL_TIP_WIDTH
				self.ResizeToolTip()

			self.AppendDefaultData(skillIndex)
			self.AppendSkillConditionData(skillIndex)
			self.AppendGuildSkillData(skillIndex, skillLevel)
		else:
			if app.ENABLE_NEW_SECONDARY_SKILLS:
				if skillIndex > 142 and skillIndex < 147:
					self.ClearToolTip()
					self.__SetSkillTitle(skillIndex, skillGrade)
					
					descValue = (localeinfo.COMMON_SKILL_143A, localeinfo.COMMON_SKILL_144, localeinfo.COMMON_SKILL_145, localeinfo.COMMON_SKILL_146)
					numbValue = [
									(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10),
									(0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20),
									(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10),
									(0, 200, 400, 800, 1200, 1600, 2000, 2200, 2500, 2700, 3000)
					]
					
					description = skill.GetSkillDescription(skillIndex)
					self.AppendDescription(description, 25)
					
					if skillLevel == 10:
						self.AppendSpace(5)
						self.AppendTextLine(localeinfo.TOOLTIP_SKILL_LEVEL_MASTER % (skillLevel), self.NORMAL_COLOR)
						self.AppendTextLine(descValue[skillIndex-143] % (numbValue[skillIndex-143][skillLevel]), self.POSITIVE_COLOR)
						if skillIndex == 143:
							self.AppendTextLine(localeinfo.COMMON_SKILL_143B % (numbValue[skillIndex-143][skillLevel]), self.POSITIVE_COLOR)
					else:
						self.AppendSpace(5)
						self.AppendTextLine(localeinfo.TOOLTIP_SKILL_LEVEL % (skillLevel), self.NORMAL_COLOR)
						self.AppendTextLine(descValue[skillIndex-143] % (numbValue[skillIndex-143][skillLevel]), self.POSITIVE_COLOR)
						if skillIndex == 143:
							self.AppendTextLine(localeinfo.COMMON_SKILL_143B % (numbValue[skillIndex-143][skillLevel]), self.POSITIVE_COLOR)
						
						self.AppendSpace(5)
						self.AppendTextLine(localeinfo.TOOLTIP_SKILL_LEVEL % (skillLevel+1), self.NEGATIVE_COLOR)
						self.AppendTextLine(descValue[skillIndex-143] % (numbValue[skillIndex-143][skillLevel + 1]), self.NEGATIVE_COLOR)
						if skillIndex == 143:
							self.AppendTextLine(localeinfo.COMMON_SKILL_143B % (numbValue[skillIndex-143][skillLevel + 1]), self.NEGATIVE_COLOR)
					
					self.ShowToolTip()
					return
			
			if self.SKILL_TOOL_TIP_WIDTH != self.toolTipWidth:
				self.toolTipWidth = self.SKILL_TOOL_TIP_WIDTH
				self.ResizeToolTip()

			slotIndex = player.GetSkillSlotIndex(skillIndex)

			skillCurrentPercentage = player.GetSkillCurrentEfficientPercentage(slotIndex)
			skillNextPercentage = player.GetSkillNextEfficientPercentage(slotIndex)

			self.AppendDefaultData(skillIndex, skillGrade)
			self.AppendSkillConditionData(skillIndex)
			self.AppendSkillDataNew(slotIndex, skillIndex, skillGrade, skillLevel, skillCurrentPercentage, skillNextPercentage)
			self.AppendSkillRequirement(skillIndex, skillLevel)

		self.ShowToolTip()

	def __SetSkillTitle(self, skillIndex, skillGrade):
		self.SetTitle(skill.GetSkillName(skillIndex, skillGrade))
		self.__AppendSkillGradeName(skillIndex, skillGrade)

	def __AppendSkillGradeName(self, skillIndex, skillGrade):
		if self.SKILL_GRADE_NAME.has_key(skillGrade):
			self.AppendSpace(5)
			self.AppendTextLine(self.SKILL_GRADE_NAME[skillGrade] % (skill.GetSkillName(skillIndex, 0)), self.CAN_LEVEL_UP_COLOR)

	def SetSkillOnlyName(self, slotIndex, skillIndex, skillGrade):
		if 0 == skillIndex:
			return

		slotIndex = player.GetSkillSlotIndex(skillIndex)

		self.toolTipWidth = self.SKILL_TOOL_TIP_WIDTH
		self.ResizeToolTip()

		self.ClearToolTip()
		self.__SetSkillTitle(skillIndex, skillGrade)
		self.AppendDefaultData(skillIndex, skillGrade)
		self.AppendSkillConditionData(skillIndex)
		self.ShowToolTip()

	def AppendDefaultData(self, skillIndex, skillGrade = 0):
		self.ClearToolTip()
		self.__SetSkillTitle(skillIndex, skillGrade)

		## Level Limit
		levelLimit = skill.GetSkillLevelLimit(skillIndex)
		if levelLimit > 0:

			color = self.NORMAL_COLOR
			if player.GetStatus(player.LEVEL) < levelLimit:
				color = self.NEGATIVE_COLOR

			self.AppendSpace(5)
			self.AppendTextLine(localeinfo.TOOLTIP_ITEM_LIMIT_LEVEL % (levelLimit), color)

		## Description
		description = skill.GetSkillDescription(skillIndex)
		self.AppendDescription(description, 25)

	def AppendSupportSkillDefaultData(self, skillIndex, skillGrade, skillLevel, maxLevel):
		self.ClearToolTip()
		self.__SetSkillTitle(skillIndex, skillGrade)

		## Description
		description = skill.GetSkillDescription(skillIndex)
		self.AppendDescription(description, 25)

		if 1 == skillGrade:
			skillLevel += 19
		elif 2 == skillGrade:
			skillLevel += 29
		elif 3 == skillGrade:
			skillLevel = 40

		self.AppendSpace(5)
		self.AppendTextLine(localeinfo.TOOLTIP_SKILL_LEVEL_WITH_MAX % (skillLevel, maxLevel), self.NORMAL_COLOR)

	def AppendSkillConditionData(self, skillIndex):
		conditionDataCount = skill.GetSkillConditionDescriptionCount(skillIndex)
		if conditionDataCount > 0:
			self.AppendSpace(5)
			for i in xrange(conditionDataCount):
				self.AppendTextLine(skill.GetSkillConditionDescription(skillIndex, i), self.CONDITION_COLOR)

	def AppendGuildSkillData(self, skillIndex, skillLevel):
		skillMaxLevel = 7
		skillCurrentPercentage = float(skillLevel) / float(skillMaxLevel)
		skillNextPercentage = float(skillLevel+1) / float(skillMaxLevel)
		## Current Level
		if skillLevel > 0:
			if self.HasSkillLevelDescription(skillIndex, skillLevel):
				self.AppendSpace(5)
				if skillLevel == skillMaxLevel:
					self.AppendTextLine(localeinfo.TOOLTIP_SKILL_LEVEL_MASTER % (skillLevel), self.NORMAL_COLOR)
				else:
					self.AppendTextLine(localeinfo.TOOLTIP_SKILL_LEVEL % (skillLevel), self.NORMAL_COLOR)

				#####

				for i in xrange(skill.GetSkillAffectDescriptionCount(skillIndex)):
					self.AppendTextLine(skill.GetSkillAffectDescription(skillIndex, i, skillCurrentPercentage), self.ENABLE_COLOR)

				## Cooltime
				coolTime = skill.GetSkillCoolTime(skillIndex, skillCurrentPercentage)
				if coolTime > 0:
					self.AppendTextLine(localeinfo.TOOLTIP_SKILL_COOL_TIME + str(coolTime), self.ENABLE_COLOR)

				## SP
				needGSP = skill.GetSkillNeedSP(skillIndex, skillCurrentPercentage)
				if needGSP > 0:
					self.AppendTextLine(localeinfo.TOOLTIP_NEED_GSP % (needGSP), self.ENABLE_COLOR)

		## Next Level
		if skillLevel < skillMaxLevel:
			if self.HasSkillLevelDescription(skillIndex, skillLevel+1):
				self.AppendSpace(5)
				self.AppendTextLine(localeinfo.TOOLTIP_NEXT_SKILL_LEVEL_1 % (skillLevel+1, skillMaxLevel), self.DISABLE_COLOR)

				#####

				for i in xrange(skill.GetSkillAffectDescriptionCount(skillIndex)):
					self.AppendTextLine(skill.GetSkillAffectDescription(skillIndex, i, skillNextPercentage), self.DISABLE_COLOR)

				## Cooltime
				coolTime = skill.GetSkillCoolTime(skillIndex, skillNextPercentage)
				if coolTime > 0:
					self.AppendTextLine(localeinfo.TOOLTIP_SKILL_COOL_TIME + str(coolTime), self.DISABLE_COLOR)

				## SP
				needGSP = skill.GetSkillNeedSP(skillIndex, skillNextPercentage)
				if needGSP > 0:
					self.AppendTextLine(localeinfo.TOOLTIP_NEED_GSP % (needGSP), self.DISABLE_COLOR)

	def AppendSkillDataNew(self, slotIndex, skillIndex, skillGrade, skillLevel, skillCurrentPercentage, skillNextPercentage):

		self.skillMaxLevelStartDict = { 0 : 17, 1 : 7, 2 : 10, }
		self.skillMaxLevelEndDict = { 0 : 20, 1 : 10, 2 : 10, }

		skillLevelUpPoint = 1
		realSkillGrade = player.GetSkillGrade(slotIndex)
		skillMaxLevelStart = self.skillMaxLevelStartDict.get(realSkillGrade, 15)
		skillMaxLevelEnd = self.skillMaxLevelEndDict.get(realSkillGrade, 20)

		## Current Level
		if skillLevel > 0:
			if self.HasSkillLevelDescription(skillIndex, skillLevel):
				self.AppendSpace(5)
				if skillGrade == skill.SKILL_GRADE_COUNT:
					pass
				elif skillLevel == skillMaxLevelEnd:
					self.AppendTextLine(localeinfo.TOOLTIP_SKILL_LEVEL_MASTER % (skillLevel), self.NORMAL_COLOR)
				else:
					self.AppendTextLine(localeinfo.TOOLTIP_SKILL_LEVEL % (skillLevel), self.NORMAL_COLOR)
				self.AppendSkillLevelDescriptionNew(skillIndex, skillCurrentPercentage, self.ENABLE_COLOR)

		## Next Level
		if skillGrade != skill.SKILL_GRADE_COUNT:
			if skillLevel < skillMaxLevelEnd:
				if self.HasSkillLevelDescription(skillIndex, skillLevel+skillLevelUpPoint):
					self.AppendSpace(5)
					## HP����, �������� ���������� ����
					if skillIndex == 141 or skillIndex == 142:
						self.AppendTextLine(localeinfo.TOOLTIP_NEXT_SKILL_LEVEL_3 % (skillLevel+1), self.DISABLE_COLOR)
						idx = 1
						if app.ENABLE_NEW_PASSIVE_SKILLS:
							if skillIndex >= 221 and skillIndex <= 228 and skillLevel == 10:
								idx = 20
								skillNextPercentage = player.GetSkillNextEfficientPercentageByLvl(slotIndex, 30)
						
						self.AppendTextLine(localeinfo.TOOLTIP_NEXT_SKILL_LEVEL_1 % (skillLevel+idx, skillMaxLevelEnd), self.DISABLE_COLOR)
					self.AppendSkillLevelDescriptionNew(skillIndex, skillNextPercentage, self.DISABLE_COLOR)

	def AppendSkillLevelDescriptionNew(self, skillIndex, skillPercentage, color):

		affectDataCount = skill.GetNewAffectDataCount(skillIndex)
		if affectDataCount > 0:
			for i in xrange(affectDataCount):
				type, minValue, maxValue = skill.GetNewAffectData(skillIndex, i, skillPercentage)

				if not self.AFFECT_NAME_DICT.has_key(type):
					continue

				minValue = int(minValue)
				maxValue = int(maxValue)
				affectText = self.AFFECT_NAME_DICT[type]

				if "HP" == type:
					if minValue < 0 and maxValue < 0:
						minValue *= -1
						maxValue *= -1

					else:
						affectText = localeinfo.TOOLTIP_SKILL_AFFECT_HEAL

				affectText += str(minValue)
				if minValue != maxValue:
					affectText += " - " + str(maxValue)
				affectText += self.AFFECT_APPEND_TEXT_DICT.get(type, "")

				#import debuginfo
				#if debuginfo.IsDebugMode():
				#	affectText = "!!" + affectText

				self.AppendTextLine(affectText, color)

		else:
			for i in xrange(skill.GetSkillAffectDescriptionCount(skillIndex)):
				self.AppendTextLine(skill.GetSkillAffectDescription(skillIndex, i, skillPercentage), color)


		## Duration
		duration = skill.GetDuration(skillIndex, skillPercentage)
		if duration > 0:
			self.AppendTextLine(localeinfo.TOOLTIP_SKILL_DURATION % (duration), color)

		## Cooltime
		coolTime = skill.GetSkillCoolTime(skillIndex, skillPercentage)
		if coolTime > 0:
			self.AppendTextLine(localeinfo.TOOLTIP_SKILL_COOL_TIME + str(coolTime), color)

		## SP
		needSP = skill.GetSkillNeedSP(skillIndex, skillPercentage)
		if needSP != 0:
			continuationSP = skill.GetSkillContinuationSP(skillIndex, skillPercentage)

			if skill.IsUseHPSkill(skillIndex):
				self.AppendNeedHP(needSP, continuationSP, color)
			else:
				self.AppendNeedSP(needSP, continuationSP, color)

	def AppendSkillRequirement(self, skillIndex, skillLevel):

		skillMaxLevel = skill.GetSkillMaxLevel(skillIndex)

		if skillLevel >= skillMaxLevel:
			return

		isAppendHorizontalLine = False

		## Requirement
		if skill.IsSkillRequirement(skillIndex):

			if not isAppendHorizontalLine:
				isAppendHorizontalLine = True
				self.AppendHorizontalLine()

			requireSkillName, requireSkillLevel = skill.GetSkillRequirementData(skillIndex)

			color = self.CANNOT_LEVEL_UP_COLOR
			if skill.CheckRequirementSueccess(skillIndex):
				color = self.CAN_LEVEL_UP_COLOR
			self.AppendTextLine(localeinfo.TOOLTIP_REQUIREMENT_SKILL_LEVEL % (requireSkillName, requireSkillLevel), color)

		## Require Stat
		requireStatCount = skill.GetSkillRequireStatCount(skillIndex)
		if requireStatCount > 0:

			for i in xrange(requireStatCount):
				type, level = skill.GetSkillRequireStatData(skillIndex, i)
				if self.POINT_NAME_DICT.has_key(type):

					if not isAppendHorizontalLine:
						isAppendHorizontalLine = True
						self.AppendHorizontalLine()

					name = self.POINT_NAME_DICT[type]
					color = self.CANNOT_LEVEL_UP_COLOR
					if player.GetStatus(type) >= level:
						color = self.CAN_LEVEL_UP_COLOR
					self.AppendTextLine(localeinfo.TOOLTIP_REQUIREMENT_STAT_LEVEL % (name, level), color)

	def HasSkillLevelDescription(self, skillIndex, skillLevel):
		if skill.GetSkillAffectDescriptionCount(skillIndex) > 0:
			return True
		if skill.GetSkillCoolTime(skillIndex, skillLevel) > 0:
			return True
		if skill.GetSkillNeedSP(skillIndex, skillLevel) > 0:
			return True

		return False

	def AppendMasterAffectDescription(self, index, desc, color):
		self.AppendTextLine(desc, color)

	def AppendNextAffectDescription(self, index, desc):
		self.AppendTextLine(desc, self.DISABLE_COLOR)

	def AppendNeedHP(self, needSP, continuationSP, color):

		self.AppendTextLine(localeinfo.TOOLTIP_NEED_HP % (needSP), color)

		if continuationSP > 0:
			self.AppendTextLine(localeinfo.TOOLTIP_NEED_HP_PER_SEC % (continuationSP), color)

	def AppendNeedSP(self, needSP, continuationSP, color):

		if -1 == needSP:
			self.AppendTextLine(localeinfo.TOOLTIP_NEED_ALL_SP, color)

		else:
			self.AppendTextLine(localeinfo.TOOLTIP_NEED_SP % (needSP), color)

		if continuationSP > 0:
			self.AppendTextLine(localeinfo.TOOLTIP_NEED_SP_PER_SEC % (continuationSP), color)

	def AppendPartySkillData(self, skillGrade, skillLevel):
		# @fixme008 BEGIN
		def comma_fix(vl):
			return vl.replace("%,0f", "%.0f")
		# @fixme008 END

		if 1 == skillGrade:
			skillLevel += 19
		elif 2 == skillGrade:
			skillLevel += 29
		elif 3 == skillGrade:
			skillLevel =  40

		if skillLevel <= 0:
			return

		skillIndex = player.SKILL_INDEX_TONGSOL
		slotIndex = player.GetSkillSlotIndex(skillIndex)
		skillPower = player.GetSkillCurrentEfficientPercentage(slotIndex)
		if localeinfo.IsBRAZIL():
			k = skillPower
		else:
			k = player.GetSkillLevel(skillIndex) / 100.0
		self.AppendSpace(5)
		self.AutoAppendTextLine(localeinfo.TOOLTIP_PARTY_SKILL_LEVEL % skillLevel, self.NORMAL_COLOR)

		# @fixme008 BEGIN
		if skillLevel>=10:
			self.AutoAppendTextLine(comma_fix(localeinfo.PARTY_SKILL_ATTACKER) % chop( 10 + 60 * k ))

		if skillLevel>=20:
			self.AutoAppendTextLine(comma_fix(localeinfo.PARTY_SKILL_BERSERKER) 	% chop(1 + 5 * k))
			self.AutoAppendTextLine(comma_fix(localeinfo.PARTY_SKILL_TANKER) 	% chop(50 + 1450 * k))

		if skillLevel>=25:
			self.AutoAppendTextLine(comma_fix(localeinfo.PARTY_SKILL_BUFFER) % chop(5 + 45 * k ))

		if skillLevel>=35:
			self.AutoAppendTextLine(comma_fix(localeinfo.PARTY_SKILL_SKILL_MASTER) % chop(25 + 600 * k ))

		if skillLevel>=40:
			self.AutoAppendTextLine(comma_fix(localeinfo.PARTY_SKILL_DEFENDER) % chop( 5 + 30 * k ))
		# @fixme008 END

		self.AlignHorizonalCenter()

	def __AppendSummonDescription(self, skillLevel, color):
		if skillLevel > 1:
			self.AppendTextLine(localeinfo.SKILL_SUMMON_DESCRIPTION % (skillLevel * 10), color)
		elif 1 == skillLevel:
			self.AppendTextLine(localeinfo.SKILL_SUMMON_DESCRIPTION % (15), color)
		elif 0 == skillLevel:
			self.AppendTextLine(localeinfo.SKILL_SUMMON_DESCRIPTION % (10), color)

if __name__ == "__main__":
	import app
	import wndMgr
	import systemSetting
	import mousemodule
	import grp
	import ui

	#wndMgr.SetOutlineFlag(True)

	app.SetMouseHandler(mousemodule.mouseController)
	app.SetHairColorEnable(True)
	wndMgr.SetMouseHandler(mousemodule.mouseController)
	wndMgr.SetScreenSize(systemSetting.GetWidth(), systemSetting.GetHeight())
	app.Create("METIN2 CLOSED BETA", systemSetting.GetWidth(), systemSetting.GetHeight(), 1)
	mousemodule.mouseController.Create()

	toolTip = ItemToolTip()
	toolTip.ClearToolTip()
	#toolTip.AppendTextLine("Test")
	desc = "Item descriptions:|increase of width of display to 35 digits per row AND installation of function that the displayed words are not broken up in two parts, but instead if one word is too long to be displayed in this row, this word will start in the next row."
	summ = ""

	toolTip.AddItemData_Offline(10, desc, summ, 0, 0)
	toolTip.Show()

	app.Loop()

