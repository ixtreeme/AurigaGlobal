import wndMgr
import ui
import ime
import app
import localeinfo

class PickMoneyDialog(ui.ScriptWindow):
	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.division = 0
		self.unitValue = 1
		self.maxValue = 0
		self.eventAccept = 0
		self.allowStackPackageInput = False
		self.stackPackageCount = 0
		self.stackPackageSize = 0

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def LoadDialog(self):
		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "uiscript/pickmoneydialog.py")
		except:
			import exception
			exception.Abort("MoneyDialog.LoadDialog.LoadScript")
		
		try:
			self.board = self.GetChild("board")
			self.maxValueTextLine = self.GetChild("max_value")
			self.pickValueEditLine = self.GetChild("money_value")
			self.acceptButton = self.GetChild("accept_button")
			self.cancelButton = self.GetChild("cancel_button")
			self.divisionText = self.GetChild("divisionText")
			self.divisionBtn = self.GetChild("divisionBtn")
			self.packageCountText = self.GetChild("package_count_text")
			self.packageCountSlot = self.GetChild("package_count_slot")
			self.packageCountEditLine = self.GetChild("package_count_value")
			self.packageCountXText = self.GetChild("package_count_x")
			self.packageSizeText = self.GetChild("package_size_text")
			self.moneySlot = self.GetChild("money_slot")
		except:
			import exception
			exception.Abort("MoneyDialog.LoadDialog.BindObject")
		
		self.pickValueEditLine.SetReturnEvent(ui.__mem_func__(self.OnAccept))
		self.pickValueEditLine.SetEscapeEvent(ui.__mem_func__(self.Close))
		self.packageCountEditLine.SetReturnEvent(ui.__mem_func__(self.OnAccept))
		self.packageCountEditLine.SetEscapeEvent(ui.__mem_func__(self.Close))
		self.acceptButton.SetEvent(ui.__mem_func__(self.OnAccept))
		self.cancelButton.SetEvent(ui.__mem_func__(self.Close))
		self.board.SetCloseEvent(ui.__mem_func__(self.Close))
		self.divisionBtn.SetToggleDownEvent(lambda arg = 1: self.SetDivisionBtn(arg))
		self.divisionBtn.SetToggleUpEvent(lambda arg = 0: self.SetDivisionBtn(arg))

	def Destroy(self):
		self.ClearDictionary()
		self.eventAccept = 0
		self.maxValue = 0
		self.pickValueEditLine = 0
		self.acceptButton = 0
		self.cancelButton = 0
		self.packageCountEditLine = 0
		self.board = None

	def SetDivisionBtn(self, arg):
		self.division = arg

	def SetTitleName(self, text):
		self.board.SetTitleName(text)

	def SetAcceptEvent(self, event):
		self.eventAccept = event

	def SetMax(self, max):
		self.pickValueEditLine.SetMax(max)

	def Open(self, maxValue, unitValue=1, isDivion = False, isExchange = False, allowStackPackageInput = False):
		if localeinfo.IsYMIR() or localeinfo.IsCHEONMA() or localeinfo.IsHONGKONG():
			unitValue = ""
	
		if isExchange:
			self.SetSize(200, 90)
			if self.board:
				self.board.SetSize(200, 90)
			if self.acceptButton:
				self.acceptButton.SetPosition(19 + 15, 58)
			if self.cancelButton:
				self.cancelButton.SetPosition(90 + 15, 58)
	
			if self.moneySlot:
				self.moneySlot.SetPosition(65, 31)
	
			self.divisionText.Hide()
			self.divisionBtn.Hide()
			self.packageCountText.Hide()
			self.packageCountSlot.Hide()
			self.packageCountXText.Hide()
			self.packageSizeText.Hide()
	
		elif isDivion:
			y = 26
			self.SetSize(170, 90 + y)
			if self.board:
				self.board.SetSize(170, 90 + y)
			if self.acceptButton:
				self.acceptButton.SetPosition(19, 58 + y)
			if self.cancelButton:
				self.cancelButton.SetPosition(90, 58 + y)
	
			if self.moneySlot:
				self.moneySlot.SetPosition(50, 31)
	
			self.divisionText.Show()
			self.divisionBtn.Show()
			self.divisionBtn.SetUp()
			self.division = 0
			self.packageCountText.Hide()
			self.packageCountSlot.Hide()
			self.packageCountXText.Hide()
			self.packageSizeText.Hide()
	
		elif allowStackPackageInput:
			self.SetSize(230, 120)
			if self.board:
				self.board.SetSize(230, 120)
			if self.acceptButton:
				self.acceptButton.SetPosition(230/2 - 61 - 5, 92)
			if self.cancelButton:
				self.cancelButton.SetPosition(230/2 + 5, 92)
	
			if self.moneySlot:
				self.moneySlot.SetPosition(124, 50)
	
			self.divisionText.Hide()
			self.divisionBtn.Hide()
			self.packageCountText.Show()
			self.packageCountSlot.Show()
			self.packageCountXText.Show()
			self.packageSizeText.Show()
	
		else:
			self.SetSize(170, 90)
			if self.board:
				self.board.SetSize(170, 90)
			if self.acceptButton:
				self.acceptButton.SetPosition(19, 58)
			if self.cancelButton:
				self.cancelButton.SetPosition(90, 58)
	
			if self.moneySlot:
				self.moneySlot.SetPosition(50, 31)
	
			self.divisionText.Hide()
			self.divisionBtn.Hide()
			self.packageCountText.Hide()
			self.packageCountSlot.Hide()
			self.packageCountXText.Hide()
			self.packageSizeText.Hide()
	
		width = self.GetWidth()
		(mouseX, mouseY) = wndMgr.GetMousePosition()
		if mouseX + width/2 > wndMgr.GetScreenWidth():
			xPos = wndMgr.GetScreenWidth() - width
		elif mouseX - width/2 < 0:
			xPos = 0
		else:
			xPos = mouseX - width/2
	
		self.SetPosition(xPos, mouseY - self.GetHeight() - 20)
		if localeinfo.IsARABIC():
			self.maxValueTextLine.SetText("/" + str(maxValue))
		else:
			self.maxValueTextLine.SetText(" / " + str(maxValue))
	
		self.pickValueEditLine.SetNumberMode()
		self.packageCountEditLine.SetNumberMode()
	
		self.pickValueEditLine.SetText(str(unitValue))
		self.packageCountEditLine.SetText("1")
	
		if allowStackPackageInput:
			self.packageCountEditLine.SetFocus()
		else:
			self.pickValueEditLine.SetFocus()
	
		ime.SetCursorPosition(1)
	
		self.unitValue = unitValue
		self.maxValue = maxValue
		self.allowStackPackageInput = allowStackPackageInput
		self.stackPackageCount = 0
		self.stackPackageSize = 0
		self.Show()
		self.SetTop()
	def Close(self):
		self.pickValueEditLine.KillFocus()
		self.packageCountEditLine.KillFocus()
		self.Hide()

	def OnAccept(self):
		money = 0

		if self.allowStackPackageInput:
			packageCountText = self.packageCountEditLine.GetText()
			packageSizeText = self.pickValueEditLine.GetText()
			if packageCountText.isdigit() and packageSizeText.isdigit():
				packageCount = long(packageCountText)
				packageSize = long(packageSizeText)
				if packageCount > 0 and packageSize > 0:
					maxPackageCount = self.maxValue / packageSize
					if maxPackageCount > 0:
						packageCount = min(packageCount, maxPackageCount)
						self.stackPackageCount = packageCount
						self.stackPackageSize = packageSize
						money = packageSize
		else:
			text = self.pickValueEditLine.GetText()
			if len(text) > 0 and text.isdigit():
				money = long(text)
				money = min(money, self.maxValue)

		if money > 0:
			if self.eventAccept:
				self.eventAccept(money)
		
		self.Close()
