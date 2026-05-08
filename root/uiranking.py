import net

import ui, uiscriptlocale
import grp

class RankingWindow(ui.ScriptWindow):
	MAX_CATEGORIES_BTN = 9
	MAX_CATEGORIES_TO_VIEW = 18
	TITLE_CATEGORIES_TO_VIEW = [uiscriptlocale.RANKING_CAT0, uiscriptlocale.RANKING_CAT1, uiscriptlocale.RANKING_CAT2, uiscriptlocale.RANKING_CAT3, uiscriptlocale.RANKING_CAT4, uiscriptlocale.RANKING_CAT5, uiscriptlocale.RANKING_CAT6, uiscriptlocale.RANKING_CAT7, uiscriptlocale.RANKING_CAT8, uiscriptlocale.RANKING_CAT9, uiscriptlocale.RANKING_CAT10, uiscriptlocale.RANKING_CAT11, uiscriptlocale.RANKING_CAT12, uiscriptlocale.RANKING_CAT13, uiscriptlocale.RANKING_CAT14, uiscriptlocale.RANKING_CAT15, uiscriptlocale.RANKING_CAT16, uiscriptlocale.RANKING_CAT17]
	IMAGES_CATEGORIES_TO_VIEW = [
									["d:/ymir work/ui/public/ranking/btn_cat0_normal.sub", "d:/ymir work/ui/public/ranking/btn_cat0_over.sub", "d:/ymir work/ui/public/ranking/btn_cat0_down.sub"],
									["d:/ymir work/ui/public/ranking/btn_cat1_normal.sub", "d:/ymir work/ui/public/ranking/btn_cat1_over.sub", "d:/ymir work/ui/public/ranking/btn_cat1_down.sub"],
									["d:/ymir work/ui/public/ranking/btn_cat2_normal.sub", "d:/ymir work/ui/public/ranking/btn_cat2_over.sub", "d:/ymir work/ui/public/ranking/btn_cat2_down.sub"],
									["d:/ymir work/ui/public/ranking/btn_cat3_normal.sub", "d:/ymir work/ui/public/ranking/btn_cat3_over.sub", "d:/ymir work/ui/public/ranking/btn_cat3_down.sub"],
									["d:/ymir work/ui/public/ranking/btn_cat4_normal.sub", "d:/ymir work/ui/public/ranking/btn_cat4_over.sub", "d:/ymir work/ui/public/ranking/btn_cat4_down.sub"],
									["d:/ymir work/ui/public/ranking/btn_cat5_normal.sub", "d:/ymir work/ui/public/ranking/btn_cat5_over.sub", "d:/ymir work/ui/public/ranking/btn_cat5_down.sub"],
									["d:/ymir work/ui/public/ranking/btn_cat6_normal.sub", "d:/ymir work/ui/public/ranking/btn_cat6_over.sub", "d:/ymir work/ui/public/ranking/btn_cat6_down.sub"],
									["d:/ymir work/ui/public/ranking/btn_cat7_normal.sub", "d:/ymir work/ui/public/ranking/btn_cat7_over.sub", "d:/ymir work/ui/public/ranking/btn_cat7_down.sub"],
									["d:/ymir work/ui/public/ranking/btn_cat8_normal.sub", "d:/ymir work/ui/public/ranking/btn_cat8_over.sub", "d:/ymir work/ui/public/ranking/btn_cat8_down.sub"],
									["d:/ymir work/ui/public/ranking/btn_cat9_normal.sub", "d:/ymir work/ui/public/ranking/btn_cat9_over.sub", "d:/ymir work/ui/public/ranking/btn_cat9_down.sub"],
									["d:/ymir work/ui/public/ranking/btn_cat10_normal.sub", "d:/ymir work/ui/public/ranking/btn_cat10_over.sub", "d:/ymir work/ui/public/ranking/btn_cat10_down.sub"],
									["d:/ymir work/ui/public/ranking/btn_cat11_normal.sub", "d:/ymir work/ui/public/ranking/btn_cat11_over.sub", "d:/ymir work/ui/public/ranking/btn_cat11_down.sub"],
									["d:/ymir work/ui/public/ranking/btn_cat12_normal.sub", "d:/ymir work/ui/public/ranking/btn_cat12_over.sub", "d:/ymir work/ui/public/ranking/btn_cat12_down.sub"],
									["d:/ymir work/ui/public/ranking/btn_cat13_normal.sub", "d:/ymir work/ui/public/ranking/btn_cat13_over.sub", "d:/ymir work/ui/public/ranking/btn_cat13_down.sub"],
									["d:/ymir work/ui/public/ranking/btn_cat14_normal.sub", "d:/ymir work/ui/public/ranking/btn_cat14_over.sub", "d:/ymir work/ui/public/ranking/btn_cat14_down.sub"],
									["d:/ymir work/ui/public/ranking/btn_cat15_normal.sub", "d:/ymir work/ui/public/ranking/btn_cat15_over.sub", "d:/ymir work/ui/public/ranking/btn_cat15_down.sub"],
									["d:/ymir work/ui/public/ranking/btn_cat16_normal.sub", "d:/ymir work/ui/public/ranking/btn_cat16_over.sub", "d:/ymir work/ui/public/ranking/btn_cat16_down.sub"],
									["d:/ymir work/ui/public/ranking/btn_cat17_normal.sub", "d:/ymir work/ui/public/ranking/btn_cat17_over.sub", "d:/ymir work/ui/public/ranking/btn_cat17_down.sub"]
	]

	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.titlebar = None
		self.scrollbar = None
		self.scrollbarCat = None
		self.catBtn = None
		self.rankLines = None
		self.rankData = None
		self.rankDataBackground = None
		self.rankInfo = None
		self.category = 0
		self.isLoaded = 0

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def Load(self):
		if self.isLoaded:
			return
		
		try:
			PythonScriptLoader = ui.PythonScriptLoader()
			PythonScriptLoader.LoadScriptFile(self, "uiscript/rankingboardwindow_sys.py")
		except:
			import exception
			exception.Abort("RankingWindow.LoadDialog.LoadObject")
		
		try:
			self.titlebar = self.GetChild("titlebar")
			self.scrollbar = self.GetChild("scrollbar")
			self.scrollbarCat = self.GetChild("scrollbarCat")
			self.catBtn = []
			for i in xrange(self.MAX_CATEGORIES_BTN):
				child = self.GetChild("btn_cat%d" % (i))
				self.catBtn.append(child)
			
			self.rankLines = []
			self.rankData = {
								"ID" : [],
								"NAME" : [],
								"LEVEL" : [],
			}
			
			self.rankDataBackground = {
								"ID" : [],
								"NAME" : [],
								"LEVEL" : [],
			}
			
			for i in xrange(14):
				lineStep = 24
				yPos = i * lineStep + 62
				fileName = "d:/ymir work/ui/public/ranking/rank_list.sub"
				if i == 13:
					yPos = (14) * lineStep + 50
					fileName = "d:/ymir work/ui/public/ranking/my_rank.sub"
				
				line = ui.MakeImageBox(self, fileName, 219, yPos)
				self.rankLines.append(line)
				
				rank = ui.Bar("TOP_MOST")
				rank.SetParent(self.rankLines[i])
				rank.SetPosition(12, 1)
				rank.SetSize(42, 17)
				rank.SetColor(grp.GenerateColor(0.0, 0.0, 0.0, 0.0))
				rank.Show()
				self.rankDataBackground["ID"].append(rank)
				
				name = ui.Bar("TOP_MOST")
				name.SetParent(self.rankLines[i])
				name.SetPosition(66, 1)
				name.SetSize(114, 17)
				name.SetColor(grp.GenerateColor(0.0, 0.0, 0.0, 0.0))
				name.Show()
				self.rankDataBackground["NAME"].append(name)
				
				level = ui.Bar("TOP_MOST")
				level.SetParent(self.rankLines[i])
				level.SetPosition(193, 1)
				level.SetSize(97, 17)
				level.SetColor(grp.GenerateColor(0.0, 0.0, 0.0, 0.0))
				level.Show()
				self.rankDataBackground["LEVEL"].append(level)
				
				ID = ui.MakeTextLine(self.rankDataBackground["ID"][i])
				NAME = ui.MakeTextLine(self.rankDataBackground["NAME"][i])
				LEVEL = ui.MakeTextLine(self.rankDataBackground["LEVEL"][i])
				
				self.rankData["ID"].append(ID)
				self.rankData["NAME"].append(NAME)
				self.rankData["LEVEL"].append(LEVEL)
		except:
			import exception
			exception.Abort("RankingWindow.LoadDialog.BindObject")
		
		self.titlebar.SetCloseEvent(ui.__mem_func__(self.Close))
		self.scrollbar.SetScrollEvent(ui.__mem_func__(self.OnScroll))
		self.scrollbarCat.SetScrollEvent(ui.__mem_func__(self.OnScrollCat))
		
		i = 0
		for item in self.catBtn:
			item.SetEvent(lambda arg = i: self.Choose(arg))
			i += 1

	def OnRunMouseWheel(self, nLen):
		cat = False
		for item in self.catBtn:
			if item.IsIn():
				cat = True
				break;
		
		subCat = False
		for item in self.rankLines:
			if item.IsIn():
				subCat = True
				break;
		
		if not subCat:
			for item in self.rankDataBackground["NAME"]:
				if item.IsIn():
					subCat = True
					break;
		
		if not subCat:
			for item in self.rankDataBackground["ID"]:
				if item.IsIn():
					subCat = True
					break;
		
		if not subCat:
			for item in self.rankDataBackground["LEVEL"]:
				if item.IsIn():
					subCat = True
					break;
		
		if nLen > 0:
			if subCat:
				self.scrollbar.OnUp()
				return True
			elif cat:
				self.scrollbarCat.OnUp()
				return True
		else:
			if subCat:
				self.scrollbar.OnDown()
				return True
			elif cat:
				self.scrollbarCat.OnDown()
				return True
		
		return False

	def ClearData(self):
		self.rankInfo = {
							"ID" : [],
							"NAME" : [],
							"EMPIRE" : [],
							"LEVEL" : [],
							"REAL_POSITION" : [],
		}
		
		for i in xrange(51):
			self.rankInfo["ID"].append("")
			self.rankInfo["NAME"].append("")
			self.rankInfo["EMPIRE"].append("")
			self.rankInfo["LEVEL"].append("")
			self.rankInfo["REAL_POSITION"].append("")

	def AddRank(self, line, name, points, level, realPosition):
		try:
			if realPosition != 0:
				self.rankInfo["ID"][50] = str(line)
				self.rankInfo["NAME"][50] = name
				self.rankInfo["EMPIRE"][50] = str(points)
				self.rankInfo["LEVEL"][50] = str(level)
				self.rankInfo["REAL_POSITION"][50] = str(realPosition)
			else:
				self.rankInfo["ID"][line] = str(line)
				self.rankInfo["NAME"][line] = name
				self.rankInfo["EMPIRE"][line] = str(points)
				self.rankInfo["LEVEL"][line] = str(level)
				self.rankInfo["REAL_POSITION"][line] = str(realPosition)
		except IndexError:
			return

	def RefreshList(self):
		i = 0
		c = 0
		for item in self.rankInfo["LEVEL"]:
			if self.rankInfo["LEVEL"][i] != "0" and i != 50:
				c += 1
			
			i += 1
		
		self.scrollbar.SetPos(0)
		if c < 14:
			self.scrollbar.SetMiddleBarSize(0.98)
		elif c == 14:
			size = float(13) / float(c)
			self.scrollbar.SetMiddleBarSize(size)
			self.scrollbar.Show()
		else:
			size = float(14) / float(c)
			self.scrollbar.SetMiddleBarSize(size)
			self.scrollbar.Show()

	def Destroy(self):
		self.Close()
		self.ClearDictionary()
		self.titlebar = None
		self.scrollbar = None
		self.scrollbarCat = None
		self.catBtn = None
		self.rankLines = None
		self.rankData = None
		self.rankDataBackground = None
		self.rankInfo = None
		self.category = 0
		self.isLoaded = 0

	def Open(self):
		if self.IsShow():
			self.Close()
			return
		
		if not self.isLoaded:
			self.Load()
		
		self.scrollbarCat.SetPos(0)
		size = float(9) / float(self.MAX_CATEGORIES_TO_VIEW)
		self.scrollbarCat.SetMiddleBarSize(size)
		
		self.Choose(0)
		self.OnScrollCat()
		self.SetCenterPosition()
		self.SetTop()
		self.Show()

	def Close(self):
		self.Hide()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def Choose(self, cat):
		for i in self.catBtn:
			i.SetUp()
		
		try:
			self.catBtn[cat].Down()
		except IndexError:
			return
		
		startIndex = int(float(self.MAX_CATEGORIES_TO_VIEW - 9) * self.scrollbarCat.GetPos())
		cat += startIndex
		self.category = cat
		net.SendChatPacket("/ranking_subcategory " + str(cat))

	def RefreshCatBtn(self):
		for i in self.catBtn:
			i.SetUp()
		
		startIndex = int(float(self.MAX_CATEGORIES_TO_VIEW - 9) * self.scrollbarCat.GetPos())
		if self.category-startIndex >= 0:
			try:
				self.catBtn[self.category-startIndex].Down()
			except IndexError:
				return

	def OnScrollCat(self):
		startIndex = int(float(self.MAX_CATEGORIES_TO_VIEW - 9) * self.scrollbarCat.GetPos())
		for i in xrange(self.MAX_CATEGORIES_BTN):
			idx = i + startIndex
			self.catBtn[i].SetText(self.TITLE_CATEGORIES_TO_VIEW[idx])
			self.catBtn[i].SetTextPosition(40, 35 / 2)
			self.catBtn[i].SetUpVisual(self.IMAGES_CATEGORIES_TO_VIEW[idx][0])
			self.catBtn[i].SetOverVisual(self.IMAGES_CATEGORIES_TO_VIEW[idx][1])
			self.catBtn[i].SetDownVisual(self.IMAGES_CATEGORIES_TO_VIEW[idx][2])
		
		self.RefreshCatBtn()

	def OnScroll(self):
		i = 0
		c = 0
		for item in self.rankInfo["LEVEL"]:
			if self.rankInfo["LEVEL"][i] != "0" and i != 50:
				c += 1
			
			i += 1
		
		startIndex = int(float(c - 13) * self.scrollbar.GetPos())
		j = 0
		m = False
		for i in xrange(startIndex, startIndex + 13):
			if i > 37 and j == 0:
				m = True
			
			if j == 12 and startIndex == 51:
				self.rankData["ID"][j].SetText("")
				self.rankData["NAME"][j].SetText("")
				self.rankData["LEVEL"][j].SetText("")
				break
			
			r = i
			if m:
				r = i - 13
			
			try:
				if str(int(self.rankInfo["ID"][r]) + 1) == "1":
					fileName = "d:/ymir work/ui/public/ranking/rank_list1.sub"
				elif str(int(self.rankInfo["ID"][r]) + 1) == "2":
					fileName = "d:/ymir work/ui/public/ranking/rank_list2.sub"
				elif str(int(self.rankInfo["ID"][r]) + 1) == "3":
					fileName = "d:/ymir work/ui/public/ranking/rank_list3.sub"
				else:
					fileName = "d:/ymir work/ui/public/ranking/rank_list.sub"
				
				self.rankLines[j].LoadImage(fileName)
				self.rankData["ID"][j].SetText(str(int(self.rankInfo["ID"][r]) + 1))
				self.rankData["NAME"][j].SetText(self.rankInfo["NAME"][r])
				self.rankData["LEVEL"][j].SetText(self.rankInfo["EMPIRE"][r])
				
				if self.rankInfo["LEVEL"][r] == "0":
					self.rankLines[j].Hide()
				else:
					self.rankLines[j].Show()
				
				j += 1
			except IndexError:
				pass
		
		try:
			if self.rankInfo["LEVEL"][50] == "0":
				self.rankLines[13].Hide()
			else:
				self.rankLines[13].Show()
			
			self.rankData["ID"][13].SetText(self.rankInfo["REAL_POSITION"][50])
			self.rankData["NAME"][13].SetText(self.rankInfo["NAME"][50])
			self.rankData["LEVEL"][13].SetText(self.rankInfo["EMPIRE"][50])
		except IndexError:
			pass
		
