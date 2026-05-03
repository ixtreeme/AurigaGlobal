import app
import player
import ui
import item
import uitooltip
import wndMgr
import localeinfo

class CharacterDetailsUI(ui.ScriptWindow):
	def __init__(self, parent):
		self.uiCharacterStatus = parent
		ui.ScriptWindow.__init__(self)
		self.toolTip = uitooltip.ToolTip()
		self.__LoadScript()

	def __del__(self):
		self.uiCharacterStatus = None
		self.toolTip = None
		ui.ScriptWindow.__del__(self)

	def __LoadScript(self):
		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "uiscript/characterdetailswindow.py")
		except:
			import exception
			exception.Abort("CharacterDetailsUI.__LoadScript")

		# CharacterWindow.py width offset
		self.Width = 253 - 3

		self.board = self.GetChild("MainBoard")
		self.GetChild("TitleBar").CloseButtonHide()

		self.ScrollBar = self.GetChild("ScrollBar")
		self.ScrollBar.SetScrollEvent(ui.__mem_func__(self.OnScroll))

		# UI rows: left=PVM, right=PVP
		self.UI_MAX_ROWS = 11
		self.UI_MAX_COUNT = self.UI_MAX_ROWS * 2

		self.labelList = []
		self.labelValueList = []
		self.labelTextList = []

		for i in xrange(self.UI_MAX_COUNT):
			self.labelList.append(self.GetChild("label%s" % i))
			self.labelValueList.append(self.GetChild("labelvalue%s" % i))
			self.labelTextList.append(self.GetChild("labelname%s" % i))

		self.__Initialize()

	def __Initialize(self):
		# ---------------- PVM (LEFT) ----------------
		# (text, point_id)  -> point_id used in player.GetStatus(point_id)
		self.PvmList = [
			# PVM extra / farm
			(localeinfo.DETAILS_14, 122),      # Átlagos kár
			(localeinfo.DETAILS_79, 162),      # Általános sebzés [PVM]


			(localeinfo.NEW_DETAILS_6, 159),   # Metinek elleni erõ (PVM)
			(localeinfo.NEW_DETAILS_7, 160),   # Bossok elleni erõ (PVM)
			(localeinfo.DETAILS_5, 53),        # Szörnyek elleni erõ
			(localeinfo.DETAILS_71, 93),       # Testi/mágikus támadás

			(localeinfo.NEW_DETAILS_9, 170),   # Kritikus csapás esélye (PVM)
			(localeinfo.DETAILS_20, 40),       # Krit. találat esélye
			(localeinfo.DETAILS_21, 41),       # Átható találat

			(localeinfo.DETAILS_53, 38),       # Ájulási esély
			(localeinfo.DETAILS_54, 39),       # Lassítás esélye
			(localeinfo.DETAILS_55, 37),       # Mérgezés esélye

			(localeinfo.DETAILS_68, 116),      # EXP
			(localeinfo.NEW_DETAILS_2, 83),    # EXP bónusz esély


			(localeinfo.DETAILS_69, 84),       # Yang drop
			(localeinfo.DETAILS_70, 85),       # Item drop

			(localeinfo.DETAILS_65, 79),       # Esély a testi támadás visszaverésére
			(localeinfo.DETAILS_63, 67),       # Esély a testi támadás kivédésére

			# Védekezés / alap harc
			(localeinfo.NEW_DETAILS_8, 161),   # Szörny ellenállás


			# Regeneráció / elszívás
			(localeinfo.DETAILS_61, 32),       # HP Regenerácó
			(localeinfo.DETAILS_62, 33),       # MP Regenerácó
			(localeinfo.DETAILS_59, 63),       # Sebzés levonva a HP-ból
			(localeinfo.DETAILS_60, 64),       # Sebzés levonva a MP-ból
			(localeinfo.DETAILS_66, 87),       # Esély a HP visszatöltésére
			(localeinfo.DETAILS_67, 82),       # Esély a MP visszatöltésére

			# PVM mob típus bónuszok
			(localeinfo.DETAILS_7, 44),        # Állatok elleni erõ
			(localeinfo.DETAILS_3, 45),        # Orkok elleni erõ
			(localeinfo.DETAILS_4, 47),        # Élõholtak elleni erõ
			(localeinfo.DETAILS_8, 46),        # Ezot. elleni erõ
			(localeinfo.DETAILS_9, 48),        # Ördögök elleni erõ

			# Elementális
			(localeinfo.DETAILS_30, 146),      # Villám ereje
			(localeinfo.DETAILS_33, 50),       # Tûz ereje
			(localeinfo.DETAILS_31, 51),       # Jég ereje
			(localeinfo.DETAILS_34, 147),      # Szél ereje
			(localeinfo.DETAILS_35, 148),      # Föld ereje
			(localeinfo.DETAILS_32, 149),      # Sötétség ereje

			(localeinfo.DETAILS_24, 76),       # Villám ellenállás
			(localeinfo.DETAILS_27, 75),       # Tûz ellenállás
			(localeinfo.DETAILS_25, 133),      # Jég ellenállás
			(localeinfo.DETAILS_28, 78),       # Szél ellenállás
			(localeinfo.DETAILS_29, 134),      # Föld ellenállás
			(localeinfo.DETAILS_26, 135),      # Sötétség ellenállás
		]

		# ---------------- PVP (RIGHT) ----------------
		# (text, point_id)
		self.PvpList = [
			# Sebzés / támadás
			(localeinfo.DETAILS_16, 121),      # Készség kár
			(localeinfo.DETAILS_71, 93),       # Testi/mágikus támadás

			(localeinfo.DETAILS_1, 43),        # Félemberek elleni erõ
			(localeinfo.DETAILS_36, 54),       # Harcosok elleni erõ
			(localeinfo.DETAILS_37, 55),       # Nindzsák elleni erõ
			(localeinfo.DETAILS_38, 56),       # Surák elleni erõ
			(localeinfo.DETAILS_39, 57),       # Sámánok elleni erõ


			(localeinfo.DETAILS_53, 38),       # Ájulási esély
			(localeinfo.DETAILS_54, 39),       # Lassítás esélye
			(localeinfo.DETAILS_55, 37),       # Mérgezés esélye

			# Védekezés (PVP)
			(localeinfo.NEW_DETAILS_1, 156),   # Félemberek elleni védelem
			(localeinfo.DETAILS_41, 59),       # Harcos védelem
			(localeinfo.DETAILS_42, 60),       # Nindzsa védelem
			(localeinfo.DETAILS_43, 61),       # Sura védelem
			(localeinfo.DETAILS_44, 62),       # Sámán védelem

			# Fegyver védelem
			(localeinfo.DETAILS_46, 69),       # Kard védelem
			(localeinfo.DETAILS_47, 70),       # Kétkezes védelem
			(localeinfo.DETAILS_48, 71),       # Tõr védelem
			(localeinfo.DETAILS_50, 72),       # Harang védelem
			(localeinfo.DETAILS_51, 73),       # Legyezõ védelem
			(localeinfo.DETAILS_52, 74),       # Nyíl védelem

			# Fegyver ellenállás
			(localeinfo.NEW_DETAILS_10, 150),  # Kard ellenállás
			(localeinfo.NEW_DETAILS_11, 151),  # Kétkezes ellenállás
			(localeinfo.NEW_DETAILS_12, 152),  # Tõr elleni ellenállás
			(localeinfo.NEW_DETAILS_13, 153),  # Nyíl ellenállás
			(localeinfo.NEW_DETAILS_14, 154),  # Legyezõ elleni ellenállás
			(localeinfo.NEW_DETAILS_15, 155),  # Harang elleni ellenállás

			(localeinfo.DETAILS_76, 77),       # Mágia ellenállás
			(localeinfo.DETAILS_72, 98),       # Varázsvédelem

			(localeinfo.DETAILS_22, 136),      # Krit. találat ell.
			(localeinfo.DETAILS_23, 137),      # Átható találat ell.
			(localeinfo.DETAILS_15, 124),      # Átlagos kár ell.
			(localeinfo.DETAILS_17, 123),      # Készség kár ellenállás

			(localeinfo.DETAILS_56, 81),       # Méreg ellenállás
			(localeinfo.DETAILS_64, 68),       # Esély a nyilak elkerülésére
			(localeinfo.DETAILS_65, 79),       # Esély a testi támadás visszaverésére
			(localeinfo.DETAILS_63, 67),       # Esély a testi támadás kivédésére


			# Regeneráció / elszívás
			(localeinfo.DETAILS_61, 32),       # HP Regenerácó
			(localeinfo.DETAILS_62, 33),       # MP Regenerácó
			(localeinfo.DETAILS_59, 63),       # Sebzés levonva a HP-ból
			(localeinfo.DETAILS_60, 64),       # Sebzés levonva a MP-ból
			(localeinfo.DETAILS_66, 87),       # Esély a HP visszatöltésére
			(localeinfo.DETAILS_67, 82),       # Esély a MP visszatöltésére
		]
	
	

		max_len = len(self.PvmList)
		if len(self.PvpList) > max_len:
			max_len = len(self.PvpList)

		self.Diff = max_len - self.UI_MAX_ROWS
		if self.Diff <= 0:
			self.Diff = 0
			self.ScrollBar.Hide()
		else:
			self.ScrollBar.Show()
			self.ScrollBar.SetScrollStep(1.0 / float(self.Diff))

		self.ScollPos = 0
		self.RefreshLabel()

	def Show(self):
		ui.ScriptWindow.Show(self)

	def Close(self):
		self.Hide()

	def AdjustPosition(self, x, y):
		self.SetPosition(x + self.Width, y)

	def OnRunMouseWheel(self, nLen):
		if self.board and self.ScrollBar:
			isIn = False

			if self.board.IsIn():
				isIn = True

			if self.ScrollBar.IsIn():
				isIn = True

			if not isIn:
				for w in self.labelList:
					if w.IsIn():
						isIn = True
						break

				if not isIn:
					for w in self.labelTextList:
						if w.IsIn():
							isIn = True
							break

			if isIn:
				if nLen > 0:
					self.ScrollBar.OnUp2()
					return True
				else:
					self.ScrollBar.OnDown2()
					return True

		return False

	def OnScroll(self):
		self.RefreshLabel()

	def RefreshLabel(self):
		if self.Diff > 0:
			self.ScollPos = int(self.ScrollBar.GetPos() * self.Diff)
		else:
			self.ScollPos = 0

		for row in xrange(self.UI_MAX_ROWS):
			left_idx = row
			right_idx = row + self.UI_MAX_ROWS

			pvm_i = row + self.ScollPos
			if pvm_i < len(self.PvmList):
				(text, point) = self.PvmList[pvm_i]
				self.labelTextList[left_idx].Show()
				self.labelList[left_idx].Show()
				self.labelTextList[left_idx].SetText(text)
				self.labelValueList[left_idx].SetText(str(player.GetStatus(point)))
			else:
				self.labelTextList[left_idx].Hide()
				self.labelList[left_idx].Hide()

			pvp_i = row + self.ScollPos
			if pvp_i < len(self.PvpList):
				(text, point) = self.PvpList[pvp_i]
				self.labelTextList[right_idx].Show()
				self.labelList[right_idx].Show()
				self.labelTextList[right_idx].SetText(text)
				self.labelValueList[right_idx].SetText(str(player.GetStatus(point)))
			else:
				self.labelTextList[right_idx].Hide()
				self.labelList[right_idx].Hide()

	def OnMoveWindow(self, x, y):
		if self.uiCharacterStatus:
			self.uiCharacterStatus.AdjustPosition(x, y, self.Width)
