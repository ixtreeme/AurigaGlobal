import ui
import uiscriptlocale
import net
import snd
import app
import mousemodule
import constinfo

class WebWindow(ui.ScriptWindow):
	def __init__(self, main = False):
		ui.ScriptWindow.__init__(self, "TOP_MOST")
		self.main = main
		self.oldPos = None
		self.IsShowWindowValue = False

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def LoadWindow(self):
		try:
			pyScrLoader = ui.PythonScriptLoader()
			if self.main:
				pyScrLoader.LoadScriptFile(self, "uiscript/webwindow_fullscreen.py")
			else:
				pyScrLoader.LoadScriptFile(self, "uiscript/webwindow.py")
		except:
			import exception
			exception.Abort("WebWindow.LoadDialog.LoadScript")

		try:
			GetObject=self.GetChild
			self.titleBar = GetObject("TitleBar")

		except:
			import exception
			exception.Abort("WebWindow.LoadDialog.BindObject")

		self.titleBar.SetCloseEvent(ui.__mem_func__(self.__OnCloseButtonClick))

	def Destroy(self):
		app.HideWebPage()
		self.ClearDictionary()
		self.titleBar = None

	def Open(self, url):
		self.Refresh()
		self.Show()
		self.SetCenterPosition()

		x, y = self.GetGlobalPosition()
		sx, sy = x + 10, y + 30
		ex, ey = sx + self.GetWidth() - 20, sy + self.GetHeight() - 40
		
		app.ShowWebPage(url, (sx, sy, ex, ey))
		self.IsShowWindowValue = True

	def Close(self):
		app.HideWebPage()
		self.Hide()
		self.IsShowWindowValue = False

	def Clear(self):
		self.Refresh()

	def Refresh(self):
		pass

	def __OnCloseButtonClick(self):
		self.Close()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def OnUpdate(self):
		newPos = self.GetGlobalPosition()
		if newPos == self.oldPos:
			return
		
		self.oldPos = newPos
		
		x, y = newPos
		sx, sy = x + 10, y + 30
		ex, ey = sx + self.GetWidth() - 20, sy + self.GetHeight() - 40
		app.MoveWebPage((sx, sy, ex, ey))

