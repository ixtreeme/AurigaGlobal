#blackdragonx61 (18.11.21)
import ui
import systemSetting

class PickUpFilterWindow(ui.ScriptWindow):

	def __init__(self) :
		ui.ScriptWindow.__init__(self)
		self.isLoaded = False
		self.FilterList = []
		self.FilterSizeList = []
		#self.modesinglebutton = None
		self.modeallbutton = None
		self.minrefinevalue = None
		self.maxrefinevalue = None
		self.minlevelvalue = None
		self.maxlevelvalue = None

	def __del__(self) :
		ui.ScriptWindow.__del__(self)

	def Destroy(self):
		self.FilterList = None
		self.FilterSizeList = None
		#self.modesinglebutton = None
		self.modeallbutton = None
		self.minrefinevalue = None
		self.maxrefinevalue = None
		self.minlevelvalue = None
		self.maxlevelvalue = None

	def __LoadWindow(self):
		if self.isLoaded:
			return

		self.isLoaded = True

		# script
		try:
			self.__LoadScript("uiscript/pickupfilterwindow.py")
		except:
			import exception
			exception.Abort("PickUpFilterWindow.__LoadWindow.__LoadScript")

		# object
		try:
			self.__BindObject()
		except:
			import exception
			exception.Abort("PickUpFilterWindow.__LoadWindow.__BindObject")

		# event
		try:
			self.__BindEvent()
		except:
			import exception
			exception.Abort("PickUpFilterWindow.__LoadWindow.__BindEvent")

	def __LoadScript(self, fileName):
		pyScrLoader = ui.PythonScriptLoader()
		pyScrLoader.LoadScriptFile(self, fileName)

	def __BindObject(self):
		for index in range(systemSetting.PICKUP_FILTER_COUNT):
			self.FilterList.append(self.GetChild("filter_{}".format(index)))

		for index in range(systemSetting.PICKUP_FILTER_SIZE_COUNT):
			self.FilterSizeList.append(self.GetChild("filter_size_{}".format(index)))

		#self.modesinglebutton = self.GetChild("mode_single")
		self.modeallbutton = self.GetChild("mode_all")

		self.minrefinevalue = self.GetChild("minrefinevalue")
		self.maxrefinevalue = self.GetChild("maxrefinevalue")

		self.minlevelvalue = self.GetChild("minlevelvalue")
		self.maxlevelvalue = self.GetChild("maxlevelvalue")

	def __BindEvent(self):
		self.GetChild("board").SetCloseEvent(ui.__mem_func__(self.Close))

		for (index, button) in enumerate(self.FilterList):
			button.SetToggleDownEvent(ui.__mem_func__(self.SetFilter), index)
			button.SetToggleUpEvent(ui.__mem_func__(self.SetFilter), index)

		for (index, button) in enumerate(self.FilterSizeList):
			button.SetToggleDownEvent(ui.__mem_func__(self.SetFilterSize), index)
			button.SetToggleUpEvent(ui.__mem_func__(self.SetFilterSize), index)

		# mode_all mindig aktív, és nem kapcsolható ki
		self.modeallbutton.SAFE_SetEvent(self.ForceModeAll)

		for slot in (self.minrefinevalue, self.maxrefinevalue, self.minlevelvalue, self.maxlevelvalue):
			slot.SetEscapeEvent(ui.__mem_func__(self.Close))
			slot.SetReturnEvent(ui.__mem_func__(self.SetSlotSettings))

	def ForceModeAll(self):
		# Mindig csak MODE_ALL legyen aktív
		systemSetting.SetPickUpFilterModeAll(True)

		# UI-ban is mindig benyomva, és próbáljuk letiltani a kattintást
		if self.modeallbutton:
			self.modeallbutton.Down()
			try:
				self.modeallbutton.Disable()
			except:
				pass

	def SetFilter(self, index):
		systemSetting.SetPickUpFilter(index)
		self.SetSlotSettings()
		self.RefreshSettings()

	def SetFilterSize(self, index):
		systemSetting.SetPickUpFilterSize(index)
		self.SetSlotSettings()
		self.RefreshSettings()

	def SetMode(self, all):
		# Ne lehessen kikapcsolni -> mindig TRUE
		self.ForceModeAll()
		self.SetSlotSettings()
		self.RefreshSettings()

	def SetSlotSettings(self):
		if len(self.minrefinevalue.GetText()) <= 0:
			self.minrefinevalue.SetText("0")
		if len(self.maxrefinevalue.GetText()) <= 0:
			self.maxrefinevalue.SetText("9")

		if len(self.minlevelvalue.GetText()) <= 0:
			self.minlevelvalue.SetText("0")
		if len(self.maxlevelvalue.GetText()) <= 0:
			self.maxlevelvalue.SetText("999")

		minrefine = int(self.minrefinevalue.GetText())
		maxrefine = int(self.maxrefinevalue.GetText())
		systemSetting.SetPickUpFilterRefine(minrefine, maxrefine)

		minlevel = int(self.minlevelvalue.GetText())
		maxlevel = int(self.maxlevelvalue.GetText())
		systemSetting.SetPickUpFilterLevel(minlevel, maxlevel)

	def RefreshSettings(self):
		# mode_all mindig aktív
		self.ForceModeAll()

		for (index, button) in enumerate(self.FilterList):
			if systemSetting.GetPickUpFilter(index):
				button.Down()
			else:
				button.SetUp()

		for (index, button) in enumerate(self.FilterSizeList):
			if systemSetting.GetPickUpFilterSize(index):
				button.Down()
			else:
				button.SetUp()

		(min_refine, max_refine) = systemSetting.GetPickUpFilterRefine()
		self.minrefinevalue.SetText(str(min_refine))
		self.maxrefinevalue.SetText(str(max_refine))

		(min_level, max_level) = systemSetting.GetPickUpFilterLevel()
		self.minlevelvalue.SetText(str(min_level))
		self.maxlevelvalue.SetText(str(min_level))
		self.maxlevelvalue.SetText(str(max_level))

	def Open(self):
		if self.IsShow():
			return

		self.__LoadWindow()

		self.ForceModeAll()
		self.RefreshSettings()
		self.SetCenterPosition()
		self.SetTop()
		self.Show()

	def Close(self):
		self.Hide()

	def OnPressEscapeKey(self):
		self.Close()
		return True
