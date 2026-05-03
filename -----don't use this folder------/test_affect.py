import app
import chr
from interfacemodule import *

import wndMgr
import systemSetting
import mousemodule
import grp
import ui

app.SetMouseHandler(mousemodule.mouseController)
app.SetHairColorEnable(True)
wndMgr.SetMouseHandler(mousemodule.mouseController)
wndMgr.SetScreenSize(systemSetting.GetWidth(), systemSetting.GetHeight())
app.Create("METIN2", systemSetting.GetWidth(), systemSetting.GetHeight(), 1)
mousemodule.mouseController.Create()

import uiaffectshower

class TestGame(ui.Window):
	def __init__(self):
		ui.Window.__init__(self)

		self.affectShower = uiaffectshower.AffectShower()
		self.affectShower.BINARY_NEW_AddAffect(chr.NEW_AFFECT_EXP_BONUS_EURO_FREE, 0, 100, 1000)

		self.interface = Interface()
		self.interface.MakeInterface()
		self.interface.ShowDefaultWindows()


	def __del__(self):
		ui.Window.__del__(self)

	def OnKeyUp(self, key):
		print key
		return True

	def OnUpdate(self):
		app.UpdateGame()

	def OnRender(self):
		app.RenderGame()
		grp.PopState()
		grp.SetInterfaceRenderState()

game = TestGame()
game.SetSize(systemSetting.GetWidth(), systemSetting.GetHeight())
game.Show()

app.Loop()
