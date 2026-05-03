import app
import ui
import ime
import uiscriptlocale
import localeinfo

if app.ENABLE_PLAYER_PIN_SYSTEM:
	import uitooltip

if app.ENABLE_BUY_STACK_FROM_SHOP:
	class MultipleBuysDialog(ui.ScriptWindow):
		def __init__(self):
			ui.ScriptWindow.__init__(self)
			self.Load()

		def __del__(self):
			self.ClearDictionary()
			self.board = None
			self.acceptButton = None
			self.cancelButton = None
			self.inputSlot = None
			self.inputValue = None
			self.cancelEvent = None
			self.priceMessagge = None
			self.price = 0
			self.slot = 0
			ui.ScriptWindow.__del__(self)

		def Load(self):
			self.price = 0
			self.slot = 0
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "uiscript/multiplebuysdialog.py")
			self.board = self.GetChild("Board")
			self.acceptButton = self.GetChild("AcceptButton")
			self.cancelButton = self.GetChild("CancelButton")
			self.inputSlot = self.GetChild("InputSlot")
			self.inputValue = self.GetChild("InputValue")
			self.inputValue.OnIMEUpdate = ui.__mem_func__(self.__OnValueUpdate)
			self.priceMessagge = self.GetChild("PriceMessagge")
			self.SetCancelEvent(self.Close)
			self.SetAcceptEvent(self.AcceptBuy)

		def Open(self):
			self.inputValue.SetNumberMode()
			self.inputValue.SetText("1")
			self.inputValue.SetFocus()
			self.priceMessagge.SetText(localeinfo.MULTIPLE_BUY_PRICE % str(self.price * int(self.inputValue.GetText())))
			self.SetCenterPosition()
			self.SetTop()
			self.Show()

		def Close(self):
			self.Hide()

		def __OnValueUpdate(self):
			ui.EditLine.OnIMEUpdate(self.inputValue)
			text = self.inputValue.GetText()
			if text == "":
				return
			elif int(text) <= 0:
				self.inputValue.SetText("1")
				text = "1"
			else:
				if int(text) > app.MULTIPLE_BUY_LIMIT:
					self.inputValue.SetText(str(app.MULTIPLE_BUY_LIMIT))
					text = str(app.MULTIPLE_BUY_LIMIT)
			
			self.priceMessagge.SetText(localeinfo.MULTIPLE_BUY_PRICE % str(self.price * int(text)))

		def AcceptBuy(self):
			text = self.inputValue.GetText()
			if text == "":
				return True
			
			count = int(text)
			if self.slot >= 0 and count > 0 and count <= app.MULTIPLE_BUY_LIMIT:
				import net
				net.SendShopBuyMultiplePacket(self.slot, count)
				self.Close()
			
			return True

		def SetAcceptEvent(self, event):
			self.acceptButton.SetEvent(event)
			self.inputValue.OnIMEReturn = event

		def SetCancelEvent(self, event):
			self.board.SetCloseEvent(event)
			self.cancelButton.SetEvent(event)
			self.inputValue.OnPressEscapeKey = event

		def GetText(self):
			return self.inputValue.GetText()

		def OnPressEscapeKey(self):
			self.Close()
			return True

class QuestionDialogItem(ui.ScriptWindow):

	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.__CreateDialog()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def __CreateDialog(self):
		pyScrLoader = ui.PythonScriptLoader()
		pyScrLoader.LoadScriptFile(self, "uiscript/questiondialogitem.py")

		self.board = self.GetChild("board")
		self.textLine = self.GetChild("message")
		self.acceptButton = self.GetChild("accept")
		self.destroyButton = self.GetChild("destroy")
		self.cancelButton = self.GetChild("cancel")

	def Open(self):
		self.SetCenterPosition()
		self.SetTop()
		self.Show()

	def Close(self):
		self.Hide()

	def SetWidth(self, width):
		height = self.GetHeight()
		self.SetSize(width, height)
		self.board.SetSize(width, height)
		self.SetCenterPosition()
		self.UpdateRect()

	def SAFE_SetAcceptEvent(self, event):
		self.acceptButton.SAFE_SetEvent(event)

	def SAFE_SetCancelEvent(self, event):
		self.cancelButton.SAFE_SetEvent(event)

	def SetAcceptEvent(self, event):
		self.acceptButton.SetEvent(event)

	def SetDestroyEvent(self, event):
		self.destroyButton.SetEvent(event)

	def SetCancelEvent(self, event):
		self.cancelButton.SetEvent(event)

	def SetText(self, text):
		self.textLine.SetText(text)

	def SetAcceptText(self, text):
		self.acceptButton.SetText(text)

	def SetCancelText(self, text):
		self.cancelButton.SetText(text)

	def OnPressEscapeKey(self):
		self.Close()
		return True

class PopupDialog(ui.ScriptWindow):

	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.__LoadDialog()
		self.acceptEvent = lambda *arg: None

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def __LoadDialog(self):
		try:
			PythonScriptLoader = ui.PythonScriptLoader()
			PythonScriptLoader.LoadScriptFile(self, "uiscript/popupdialog.py")

			self.board = self.GetChild("board")
			self.message = self.GetChild("message")
			self.accceptButton = self.GetChild("accept")
			self.accceptButton.SetEvent(ui.__mem_func__(self.Close))

		except:
			import exception
			exception.Abort("PopupDialog.LoadDialog.BindObject")

	def Open(self):
		self.SetCenterPosition()
		self.SetTop()
		self.Show()

	def Close(self):
		self.Hide()
		self.acceptEvent()

	def Destroy(self):
		self.Close()
		self.ClearDictionary()

	def SetWidth(self, width):
		height = self.GetHeight()
		self.SetSize(width, height)
		self.board.SetSize(width, height)
		self.SetCenterPosition()
		self.UpdateRect()

	def SetText(self, text):
		self.message.SetText(text)

	def SetAcceptEvent(self, event):
		self.acceptEvent = event

	def SetButtonName(self, name):
		self.accceptButton.SetText(name)

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def OnIMEReturn(self):
		self.Close()
		return True

class InputDialog(ui.ScriptWindow):

	def __init__(self):
		ui.ScriptWindow.__init__(self)

		self.__CreateDialog()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def __CreateDialog(self):

		pyScrLoader = ui.PythonScriptLoader()
		pyScrLoader.LoadScriptFile(self, "uiscript/inputdialog.py")

		getObject = self.GetChild
		self.board = getObject("Board")
		self.acceptButton = getObject("AcceptButton")
		self.cancelButton = getObject("CancelButton")
		self.inputSlot = getObject("InputSlot")
		self.inputValue = getObject("InputValue")

	def Open(self):
		self.inputValue.SetFocus()
		self.SetCenterPosition()
		self.SetTop()
		self.Show()

	def Close(self):
		self.ClearDictionary()
		self.board = None
		self.acceptButton = None
		self.cancelButton = None
		self.inputSlot = None
		self.inputValue = None
		self.Hide()

	def SetTitle(self, name):
		self.board.SetTitleName(name)

	def SetNumberMode(self):
		self.inputValue.SetNumberMode()

	def SetSecretMode(self):
		self.inputValue.SetSecret()

	def SetFocus(self):
		self.inputValue.SetFocus()

	def SetMaxLength(self, length):
		width = length * 6 + 10
		self.SetBoardWidth(max(width + 50, 160))
		self.SetSlotWidth(width)
		self.inputValue.SetMax(length)

	def SetSlotWidth(self, width):
		self.inputSlot.SetSize(width, self.inputSlot.GetHeight())
		self.inputValue.SetSize(width, self.inputValue.GetHeight())
		if self.IsRTL():
			self.inputValue.SetPosition(self.inputValue.GetWidth(), 0)

	def SetBoardWidth(self, width):
		self.SetSize(max(width + 50, 160), self.GetHeight())
		self.board.SetSize(max(width + 50, 160), self.GetHeight())
		if self.IsRTL():
			self.board.SetPosition(self.board.GetWidth(), 0)
		self.UpdateRect()

	def SetAcceptEvent(self, event):
		self.acceptButton.SetEvent(event)
		self.inputValue.OnIMEReturn = event

	def SetCancelEvent(self, event):
		self.board.SetCloseEvent(event)
		self.cancelButton.SetEvent(event)
		self.inputValue.OnPressEscapeKey = event

	def GetText(self):
		return self.inputValue.GetText()

	def EmptyText(self):
		self.inputValue.SetText("")

	def SetNewAcceptText(self, text):
		self.acceptButton.SetText(text)

	def SetNewCancelText(self, text):
		self.cancelButton.SetText(text)

class InputDialogWithDescription(InputDialog):

	def __init__(self):
		ui.ScriptWindow.__init__(self)

		self.__CreateDialog()

	def __del__(self):
		InputDialog.__del__(self)

	def __CreateDialog(self):

		pyScrLoader = ui.PythonScriptLoader()
		if localeinfo.IsARABIC() :
			pyScrLoader.LoadScriptFile(self, uiscriptlocale.LOCALE_UISCRIPT_PATH + "inputdialogwithdescription.py")
		else:
			pyScrLoader.LoadScriptFile(self, "uiscript/inputdialogwithdescription.py")

		try:
			getObject = self.GetChild
			self.board = getObject("Board")
			self.acceptButton = getObject("AcceptButton")
			self.cancelButton = getObject("CancelButton")
			self.inputSlot = getObject("InputSlot")
			self.inputValue = getObject("InputValue")
			self.description = getObject("Description")

		except:
			import exception
			exception.Abort("InputDialogWithDescription.LoadBoardDialog.BindObject")

	def SetDescription(self, text):
		self.description.SetText(text)

class InputDialogWithDescription2(InputDialog):

	def __init__(self):
		ui.ScriptWindow.__init__(self)

		self.__CreateDialog()

	def __del__(self):
		InputDialog.__del__(self)

	def __CreateDialog(self):

		pyScrLoader = ui.PythonScriptLoader()
		pyScrLoader.LoadScriptFile(self, "uiscript/inputdialogwithdescription2.py")

		try:
			getObject = self.GetChild
			self.board = getObject("Board")
			self.acceptButton = getObject("AcceptButton")
			self.cancelButton = getObject("CancelButton")
			self.inputSlot = getObject("InputSlot")
			self.inputValue = getObject("InputValue")
			self.description1 = getObject("Description1")
			self.description2 = getObject("Description2")

		except:
			import exception
			exception.Abort("InputDialogWithDescription.LoadBoardDialog.BindObject")

	def SetDescription1(self, text):
		self.description1.SetText(text)

	def SetDescription2(self, text):
		self.description2.SetText(text)

class QuestionDialog(ui.ScriptWindow):

	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.__CreateDialog()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def __CreateDialog(self):
		pyScrLoader = ui.PythonScriptLoader()
		pyScrLoader.LoadScriptFile(self, "uiscript/questiondialog.py")

		self.board = self.GetChild("board")
		self.textLine = self.GetChild("message")
		self.acceptButton = self.GetChild("accept")
		self.cancelButton = self.GetChild("cancel")

	def Open(self):
		self.SetCenterPosition()
		self.SetTop()
		self.Show()

	def Close(self):
		self.Hide()

	def SetWidth(self, width):
		height = self.GetHeight()
		self.SetSize(width, height)
		self.board.SetSize(width, height)
		self.SetCenterPosition()
		self.UpdateRect()

	def SAFE_SetAcceptEvent(self, event):
		self.acceptButton.SAFE_SetEvent(event)

	def SAFE_SetCancelEvent(self, event):
		self.cancelButton.SAFE_SetEvent(event)

	def SetAcceptEvent(self, event):
		self.acceptButton.SetEvent(event)

	def SetCancelEvent(self, event):
		self.cancelButton.SetEvent(event)

	def SetText(self, text):
		self.textLine.SetText(text)

	def SetAcceptText(self, text):
		self.acceptButton.SetText(text)

	def SetCancelText(self, text):
		self.cancelButton.SetText(text)

	def OnPressEscapeKey(self):
		self.Close()
		return True

class QuestionDialog2(QuestionDialog):

	def __init__(self):
		QuestionDialog.__init__(self)
		self.__CreateDialog()

	def __del__(self):
		QuestionDialog.__del__(self)

	def __CreateDialog(self):
		pyScrLoader = ui.PythonScriptLoader()
		pyScrLoader.LoadScriptFile(self, "uiscript/questiondialog2.py")

		self.board = self.GetChild("board")
		self.textLine1 = self.GetChild("message1")
		self.textLine2 = self.GetChild("message2")
		self.acceptButton = self.GetChild("accept")
		self.cancelButton = self.GetChild("cancel")

	def SetText1(self, text):
		self.textLine1.SetText(text)

	def SetText2(self, text):
		self.textLine2.SetText(text)

class QuestionDialogWithTimeLimit(QuestionDialog2):
	def __init__(self):
		ui.ScriptWindow.__init__(self)

		self.__CreateDialog()
		self.endTime = 0
		self.timeoverMsg = None
		self.isCancelOnTimeover = False

	def __del__(self):
		QuestionDialog2.__del__(self)

	def __CreateDialog(self):
		pyScrLoader = ui.PythonScriptLoader()
		pyScrLoader.LoadScriptFile(self, "uiscript/questiondialog2.py")

		self.board = self.GetChild("board")
		self.textLine1 = self.GetChild("message1")
		self.textLine2 = self.GetChild("message2")
		self.acceptButton = self.GetChild("accept")
		self.cancelButton = self.GetChild("cancel")

	def Open(self, msg, timeout):
		self.SetCenterPosition()
		self.SetTop()
		self.Show()

		self.SetText1(msg)
		self.endTime = app.GetTime() + timeout

	def OnUpdate(self):
		leftTime = max(0, self.endTime - app.GetTime())
		self.SetText2(localeinfo.UI_LEFT_TIME % (leftTime))
		if leftTime<0.5:
			if self.timeoverMsg:
				chat.AppendChat(chat.CHAT_TYPE_INFO, self.timeoverMsg)
			if self.isCancelOnTimeover:
				self.cancelButton.CallEvent()

	def SetTimeOverMsg(self, msg):
		self.timeoverMsg = msg

	def SetCancelOnTimeOver(self):
		self.isCancelOnTimeover = True

class MoneyInputDialog(ui.ScriptWindow):

	def __init__(self):
		ui.ScriptWindow.__init__(self)

		self.moneyHeaderText = localeinfo.MONEY_INPUT_DIALOG_SELLPRICE
		self.__CreateDialog()
		self.SetMaxLength(13)
		#self.SetMaxLength(9)

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def __CreateDialog(self):

		pyScrLoader = ui.PythonScriptLoader()
		pyScrLoader.LoadScriptFile(self, "uiscript/moneyinputdialog.py")

		getObject = self.GetChild
		self.board = self.GetChild("board")
		self.acceptButton = getObject("AcceptButton")
		self.cancelButton = getObject("CancelButton")
		self.inputValue = getObject("InputValue")
		self.inputValue.SetNumberMode()
		self.inputValue.OnIMEUpdate = ui.__mem_func__(self.__OnValueUpdate)
		self.moneyText = getObject("MoneyValue")

		# Optional: market average price line (used by offline shop)
		try:
			self.marketAvgText = getObject("MarketAvgText")
			self.marketAvgText.Hide()
		except:
			self.marketAvgText = None

	def Open(self):
		self.inputValue.SetText("")
		self.inputValue.SetFocus()
		self.__OnValueUpdate()
		try:
			if self.marketAvgText:
				self.marketAvgText.SetText("")
				self.marketAvgText.Hide()
		except:
			pass
		self.SetCenterPosition()
		self.SetTop()
		self.Show()

	def Close(self):
		self.ClearDictionary()
		self.board = None
		self.acceptButton = None
		self.cancelButton = None
		self.inputValue = None
		self.Hide()

	def SetTitle(self, name):
		self.board.SetTitleName(name)

	def SetFocus(self):
		self.inputValue.SetFocus()

	def SetMaxLength(self, length):
		length = min(13, length)
		#length = min(9, length)
		self.inputValue.SetMax(length)

	def SetMoneyHeaderText(self, text):
		self.moneyHeaderText = text

	def SetMarketAverageText(self, text):
		"""Set/clear extra info line under the input (optional).
		Used by offline shop to display market average price.
		"""
		try:
			if not self.marketAvgText:
				return
			if text:
				self.marketAvgText.SetText(text)
				self.marketAvgText.Show()
			else:
				self.marketAvgText.SetText("")
				self.marketAvgText.Hide()
		except:
			pass

	def SetAcceptEvent(self, event):
		self.acceptButton.SetEvent(event)
		self.inputValue.OnIMEReturn = event

	def SetCancelEvent(self, event):
		self.board.SetCloseEvent(event)
		self.cancelButton.SetEvent(event)
		self.inputValue.OnPressEscapeKey = event

	def SetValue(self, value):
		value=chr(value)
		self.inputValue.SetText(value)
		self.__OnValueUpdate()
		ime.SetCursorPosition(int(len(value)))


	def GetText(self):
		return self.inputValue.GetText()

	def __OnValueUpdate(self):
		ui.EditLine.OnIMEUpdate(self.inputValue)

		text = self.inputValue.GetText()

		money = 0
		if text and text.isdigit():
			try:
				money = long(text)
				#money = int(text)
			except ValueError:
				money = 199999999

		self.moneyText.SetText(self.moneyHeaderText + localeinfo.NumberToMoneyString(money))

if app.ENABLE_PLAYER_PIN_SYSTEM:
	class InputPinCodeDialog(ui.ScriptWindow):
		PIN_CODE_TOOLTIP = [
			localeinfo.PIN_CODE_TOOLTIP_LINE1,
			localeinfo.PIN_CODE_TOOLTIP_LINE2,
			localeinfo.PIN_CODE_TOOLTIP_LINE3,
			localeinfo.PIN_CODE_TOOLTIP_LINE4,
			localeinfo.PIN_CODE_TOOLTIP_LINE5
		]

		def __init__(self):
			ui.ScriptWindow.__init__(self)
			self.__CreateDialog()

		def __del__(self):
			ui.ScriptWindow.__del__(self)

		def __CreateDialog(self):
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "uiscript/inputpincodedialog.py")

			getObject = self.GetChild
			self.board = getObject("Board")
			self.pinCodeSlot = getObject("PinCodeSlot")
			self.pinCodeValue = getObject("PinCodeValue")
			self.acceptButton = getObject("AcceptButton")
			self.cancelButton = getObject("CancelButton")
			self.toolTipButton = getObject("ToolTipButton")
			self.toolTip = self.CreateToolTip(localeinfo.INPUT_PIN_CODE_DIALOG_TITLE, self.PIN_CODE_TOOLTIP)
			self.toolTip.SetTop()
			self.toolTipButton.SetToolTipWindow(self.toolTip)

		def CreateToolTip(self, title, descList):
			toolTip = uitooltip.ToolTip()
			toolTip.SetTitle(title)
			toolTip.AppendSpace(7)

			for desc in descList:
				toolTip.AutoAppendTextLine(desc)

			toolTip.AlignHorizonalCenter()
			toolTip.SetTop()
			return toolTip

		def Open(self):
			self.pinCodeValue.SetFocus()
			self.SetCenterPosition()
			self.SetTop()
			self.Show()

		def Close(self):
			self.ClearDictionary()
			self.board = None
			self.toolTipButton = None
			self.toolTip = None
			self.acceptButton = None
			self.cancelButton = None
			self.pinCodeSlot = None
			self.pinCodeValue = None
			self.Hide()

		def SetTitle(self, name):
			self.board.SetTitleName(name)

		def SetNumberMode(self):
			self.pinCodeValue.SetNumberMode()

		def SetUseCodePage(self, bUse = True):
			self.pinCodeValue.SetUseCodePage(bUse)

		def SetSecretMode(self):
			self.pinCodeValue.SetSecret()

		def SetFocus(self):
			self.pinCodeValue.SetFocus()

		def SetAcceptEvent(self, event):
			self.acceptButton.SetEvent(event)
			self.pinCodeValue.OnIMEReturn = event

		def SetCancelEvent(self, event):
			self.board.SetCloseEvent(event)
			self.cancelButton.SetEvent(event)
			self.pinCodeValue.OnPressEscapeKey = event

		def GetText(self):
			return self.pinCodeValue.GetText()

	class InputNewPinCodeDialog(ui.ScriptWindow):
		def __init__(self):
			ui.ScriptWindow.__init__(self)
			self.__CreateDialog()

		def __del__(self):
			ui.ScriptWindow.__del__(self)

		def __CreateDialog(self):
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "uiscript/inputnewpincodedialog.py")

			getObject = self.GetChild
			self.board = getObject("Board")
			self.acceptButton = getObject("AcceptButton")
			self.cancelButton = getObject("CancelButton")
			self.pinCodeSlot = getObject("PinCodeSlot")
			self.pinCodeSlotCheck = getObject("PinCodeSlotCheck")
			self.pinCodeValue = getObject("PinCodeValue")
			self.pinCodeValueCheck = getObject("PinCodeValueCheck")

			self.pinCodeValue.SetTabEvent(lambda arg = 1: self.OnNextFocus(arg))
			self.pinCodeValueCheck.SetTabEvent(lambda arg = 2: self.OnNextFocus(arg))

		def OnNextFocus(self, arg):
			if 1 == arg:
				self.pinCodeValue.KillFocus()
				self.pinCodeValueCheck.SetFocus()
			elif 2 == arg:
				self.pinCodeValueCheck.KillFocus()
				self.pinCodeValue.SetFocus()

		def Open(self):
			self.pinCodeValue.SetFocus()
			self.SetCenterPosition()
			self.SetTop()
			self.Show()

		def Destroy(self):
			self.Close()

		def Close(self):
			self.ClearDictionary()
			self.board = None
			self.acceptButton = None
			self.cancelButton = None
			self.pinCodeSlot = None
			self.pinCodeSlotCheck = None
			self.pinCodeValue = None
			self.pinCodeValueCheck = None
			self.Hide()

		def SetTitle(self, name):
			self.board.SetTitleName(name)

		def SetNumberMode(self):
			self.pinCodeValue.SetNumberMode()
			self.pinCodeValueCheck.SetNumberMode()

		def SetUseCodePage(self, bUse = True):
			self.pinCodeValue.SetUseCodePage(bUse)
			self.pinCodeValueCheck.SetUseCodePage(bUse)

		def SetSecretMode(self):
			self.pinCodeValue.SetSecret()
			self.pinCodeValueCheck.SetSecret()

		def SetFocus(self):
			self.pinCodeValue.SetFocus()

		def SetAcceptEvent(self, event):
			self.acceptButton.SetEvent(event)
			self.pinCodeValueCheck.OnIMEReturn = event

		def SetCancelEvent(self, event):
			self.board.SetCloseEvent(event)
			self.cancelButton.SetEvent(event)
			self.pinCodeValue.OnPressEscapeKey = event
			self.pinCodeValueCheck.OnPressEscapeKey = event

		def GetText(self):
			return self.pinCodeValue.GetText()

		def GetTextCheck(self):
			return self.pinCodeValueCheck.GetText()


if app.ENABLE_PVP_ADVANCED:
	class Duel_InputMoneyBet(InputDialog):
		def __init__(self):
			ui.ScriptWindow.__init__(self)
			self.LoadDialog()

		def __del__(self):
			InputDialog.__del__(self)

		def LoadDialog(self):
			pyScrLoader = ui.PythonScriptLoader()
			if localeinfo.IsARABIC() :
				pyScrLoader.LoadScriptFile(self, uiscriptlocale.LOCALE_UISCRIPT_PATH + "inputdialogwithdescription.py")
			else:
				pyScrLoader.LoadScriptFile(self, "uiscript/inputdialogwithdescription.py")
			try:
				getObject = self.GetChild
				self.board = getObject("Board")
				self.acceptButton = getObject("AcceptButton")
				self.cancelButton = getObject("CancelButton")
				self.inputSlot = getObject("InputSlot")
				self.inputValue = getObject("InputValue")
				self.description = getObject("Description")
			except:
				import exception
				exception.Abort("Duel_InputMoneyBet.LoadDialog.BindObject")

		def SetText(self, text):
			self.inputValue.SetText(str(text))

		def SetDescription(self, text):
			self.description.SetText(text)

		def SetAcceptText(self, text):
			self.acceptButton.SetText(text)

		def SetCancelText(self, text):
			self.cancelButton.SetText(text)
