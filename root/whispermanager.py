import ui
import app
import net
import chat
import playersettingmodule
import wndMgr
import uiguild
import uitooltip
import uicommon
import localeinfo

WHISPER_MAX_LEN_TEXT = 512

WHISPER_FLAG_MESSAGE = [
	"en", "ro", "it", "tr", "de", "pl", "pt", "es", "cz", "hu", "gl"
]

WHISPER_COLOR_TEXT = [
	0xFFffffff, 0xFFff796a, 0xFFb1ff80, 0xFF46deff
]

WHISPER_COLOR_MESSAGE = [
	0, 1, 2, 3
]

WHISPER_DESCRIPTION_LANGS = [

	localeinfo.WHISPERMANAGER_DIALOG1,
	localeinfo.WHISPERMANAGER_DIALOG2,
	localeinfo.WHISPERMANAGER_DIALOG3,
	localeinfo.WHISPERMANAGER_DIALOG4,
	localeinfo.WHISPERMANAGER_DIALOG5,
	localeinfo.WHISPERMANAGER_DIALOG6,
	localeinfo.WHISPERMANAGER_DIALOG7,
	localeinfo.WHISPERMANAGER_DIALOG8,
	localeinfo.WHISPERMANAGER_DIALOG9,
	localeinfo.WHISPERMANAGER_DIALOG10,
	localeinfo.WHISPERMANAGER_DIALOG11
]

class SelectKey(ui.ImageBox):
	def __init__(self, parent, x, y, event, filename = None):
		ui.ImageBox.__init__(self)
		self.SetParent(parent)
		self.SetPosition(x, y)
		
		listImage = {
			0 : "d:/ymir work/ui/whisper_color_text/white.tga",
			1 : "d:/ymir work/ui/whisper_color_text/red.tga",
			2 : "d:/ymir work/ui/whisper_color_text/green.tga",
			3 : "d:/ymir work/ui/whisper_color_text/blue.tga",
			4 : "d:/ymir work/ui/public/Parameter_Slot_01.sub"
		}

		if filename >= 0:
			filename = listImage[filename]
		else:
			filename = listImage[4]

		self.LoadImage(filename)
		self.mouse = uiguild.MouseReflector(self)
		self.mouse.SetSize(self.GetWidth(), self.GetHeight())

		image = ui.MakeImageBox(self, "d:/ymir work/ui/public/check_image.sub", 0, 0)
		image.AddFlag("not_pick")
		image.SetWindowHorizontalAlignCenter()
		image.SetWindowVerticalAlignCenter()
		image.Hide()
		self.Enable = True
		self.image = image
		self.event = event
		self.Show()
		self.mouse.UpdateRect()

	def __del__(self):
		ui.ImageBox.__del__(self)

	def SetCheck(self, flag):
		if flag:
			self.image.Show()
		else:
			self.image.Hide()

	def Disable(self):
		self.Enable = False

	def OnMouseOverIn(self):
		if not self.Enable:
			return
		self.mouse.Show()

	def OnMouseOverOut(self):
		if not self.Enable:
			return
		self.mouse.Hide()

	def OnMouseLeftButtonDown(self):
		if not self.Enable:
			return
		self.mouse.Down()

	def OnMouseLeftButtonUp(self):
		if not self.Enable:
			return
		self.mouse.Up()
		self.event()

class wnd(ui.ScriptWindow):
	def __init__(self):
		ui.ScriptWindow.__init__(self)
		
		self.typeColor = 0
		self.typeLang = ""
		self.checkBoxTable = {}
		self.checkColorTable = {}
		
		self.Initialize()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def Initialize(self):
		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "uiscript/whispermanager.py")
		except:
			import exception
			exception.Abort("WhisperManager.Initialize.LoadObject")
		try:
			GetObject = self.GetChild

			self.board = GetObject("board")
			self.colorBoard = GetObject("ColorBoard")

			GetObject("accept_button").SetEvent(self.AskQuestion)
			GetObject("cancel_button").SetEvent(self.Close)
			GetObject("titlebar").SetCloseEvent(self.Close)
			GetObject("clear_button").SetEvent(self.Clear)
			
			self.textLength = GetObject("textLine1")
			self.textLang = GetObject("textLine2")

			self.textLine = GetObject("currentLine_Value")
			self.textLine.SetEscapeEvent(ui.__mem_func__(self.Close))
			self.textLine.SetFocus()
		except:
			import exception
			exception.Abort("WhisperManager.Initialize.BindObject")
		
		keyLang = 1
		for keyLang in xrange(len(WHISPER_FLAG_MESSAGE)):
			self.checkBoxTable.update({keyLang : [SelectKey(self.board, 10 + 55 * keyLang, 52, lambda arg = keyLang: self.SetLang(arg)), self.SetFlag(25 + 55 * keyLang, 35, self.MakeFlag(keyLang))]})
			
		for keyColor in xrange(len(WHISPER_COLOR_MESSAGE)):
			self.checkColorTable.update({keyColor : [SelectKey(self.colorBoard, 30, 10 + (keyColor * 30), lambda arg = keyColor: self.SetColor(arg), keyColor)]})

		self.SetCenterPosition()
		self.SetLang(10)
		self.SetColor(0)
		self.UpdateRect()

	def SetFlag(self, x, y, image):
		self.image = ui.ImageBox()
		self.image.SetParent(self)
		self.image.SetPosition(x, y)
		self.image.LoadImage(image)
		self.image.Show()
		return self.image	

	def MakeFlag(self, arg):
		return "d:/ymir work/ui/game/flag/%s.tga" % (WHISPER_FLAG_MESSAGE[arg])

	def SetColor(self, tokens):

		def Draw(it):
			self.typeColor = WHISPER_COLOR_MESSAGE[it]
			self.textLine.SetPackedFontColor(WHISPER_COLOR_TEXT[it])
			
		for it in xrange(len(WHISPER_COLOR_MESSAGE)):
			if tokens == it:
				Draw(it)
				self.checkColorTable[it][0].SetCheck(1)
			else:
				self.checkColorTable[it][0].SetCheck(0)
		
	def SetLang(self, tokens):
		for it in xrange(len(WHISPER_FLAG_MESSAGE)):
			if tokens == it:
				self.typeLang = WHISPER_FLAG_MESSAGE[it]
				self.checkBoxTable[it][0].SetCheck(1)
			else:
				self.checkBoxTable[it][0].SetCheck(0)

	def OnUpdate(self):
		(x, y) = wndMgr.GetMousePosition()

		def IsExistToolTip(key):
			return (self.checkBoxTable[key][0].IsIn())

		self.textLength.SetText(localeinfo.WHISPER_ADMIN_LEN_TEXT % (len(self.textLine.GetText()), WHISPER_MAX_LEN_TEXT))
		self.textLang.SetText(localeinfo.WHISPER_ADMIN_CUR_LANG % (self.typeLang))

		for key in xrange(len(WHISPER_FLAG_MESSAGE)):
			if IsExistToolTip(key):
				self.wndOpenToolTip = uitooltip.ToolTip()
				self.wndOpenToolTip.SetPosition(x + 25, y)
				self.wndOpenToolTip.AppendDescription(WHISPER_DESCRIPTION_LANGS[key], None, 0xffffa879)		

	def Destroy(self):
		self.ClearDictionary()
		self.typeColor = 0
		self.typeLang = ""
		self.checkBoxTable = {}
		self.checkColorTable = {}

	def Close(self):
		self.Hide()

	def Clear(self):
		self.textLine.SetText("")
		self.textLine.SetFocus()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def SendPacket(self):

		def GetText():
			return str(self.textLine.GetText()).replace(" ", "#")
			
		def GetLang():
			return self.typeLang
			
		def GetColor():
			return self.typeColor

		net.SendWhisperAdminPacket(GetText(), GetLang(), GetColor())
		self.Clear()
		
	def AnswerQuestion(self, answer):
		if not self.wndOpenQuestion:
			return

		self.wndOpenQuestion.Close()
		self.wndOpenQuestion = None
		
		if not answer:		
			return
			
		self.SendPacket()
		
	def AskQuestion(self):
		self.wndOpenQuestion = uicommon.QuestionDialog()
		self.wndOpenQuestion.SetText(localeinfo.WHISPER_ADMIN_QUESTION_SEND)
		self.wndOpenQuestion.SetWidth(300)
		self.wndOpenQuestion.SetAcceptEvent(lambda arg = True: self.AnswerQuestion(arg))
		self.wndOpenQuestion.SetCancelEvent(lambda arg = False: self.AnswerQuestion(arg))
		self.wndOpenQuestion.Open()