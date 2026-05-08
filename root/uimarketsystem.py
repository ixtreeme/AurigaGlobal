import net
import player
import item
import snd
import shop
import wndMgr
import app
import chat
chr
import chrmgr

import ui
import uicommon
import uitooltip
import mousemodule
import constinfo
import playersettingmodule
import systemSetting
import marek38
import interfacemodule
import localeinfo

DarkmmoMarek38Npc = 0
DarkmmoMarek38Baslik = 0

PAZAR_IMAGE_DICT = {
	30000	: "d:/ymir work/ui/pattern/npcshop/30000.tga",
	30001	: "d:/ymir work/ui/pattern/npcshop/30001.tga",
	30002	: "d:/ymir work/ui/pattern/npcshop/30002.tga",
	30003	: "d:/ymir work/ui/pattern/npcshop/30003.tga",
	30004	: "d:/ymir work/ui/pattern/npcshop/30004.tga",
	30005	: "d:/ymir work/ui/pattern/npcshop/30005.tga",
	30006	: "d:/ymir work/ui/pattern/npcshop/30006.tga",
	30007	: "d:/ymir work/ui/pattern/npcshop/30007.tga",
}

class KasmirMarketiDialog(ui.ScriptWindow):
	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.npcNo = -1
		self.LoadWindow()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def LoadWindow(self):
		try:
			PythonScriptLoader = ui.PythonScriptLoader()
			PythonScriptLoader.LoadScriptFile(self, "uiscript/kasmirmarketidialog.py")
		except:
			import exception
			exception.Abort("KasmirMarketiDialog.LoadWindow.LoadObject")

		try:
			self.board = self.GetChild("board")
			self.npcTitle = self.GetChild("Npc_Title")
			self.devamButton = self.GetChild("DevamButton")
			self.cancelButton = self.GetChild("CancelButton")
			self.imageButton = self.GetChild("market_view")

			self.npcSec = {
				0 : self.GetChild("privateshop_0"),
				1 : self.GetChild("privateshop_1"),
				2 : self.GetChild("privateshop_2"),
				3 : self.GetChild("privateshop_3"),
				4 : self.GetChild("privateshop_4"),
				5 : self.GetChild("privateshop_5"),
				6 : self.GetChild("privateshop_6"),
				7 : self.GetChild("privateshop_7"),
			}
		except:
			import exception
			exception.Abort("KasmirMarketiDialog.LoadWindow.BindObject")

		self.devamButton.SetEvent(ui.__mem_func__(self.NextButton))
		self.cancelButton.SetEvent(ui.__mem_func__(self.KapatButton))

		self.board.SetCloseEvent(ui.__mem_func__(self.Close))
		self.npcTitle.SetMultiLine()
		self.npcTitle.SetLimitWidth(187)
		self.npcTitle.SetMax(250)

		for (tabKey, tabButton) in self.npcSec.items():
			tabButton.SetEvent(ui.__mem_func__(self.SelectNpc), tabKey)

	def Open(self):
		global DarkmmoMarek38Npc

		self.SetCenterPosition()
		self.SetTop()
		self.Show()

		self.npcNo = 0
		self.npcSec[0].Down()
		self.npcTitle.SetText(localeinfo.KASMIR_PAKET_STANDART)

		if DarkmmoMarek38Npc >= 0:
			DarkmmoMarek38Npc = 30000

		self.MarketResimleri(DarkmmoMarek38Npc)

	def Close(self):
		self.ClearDictionary()
		self.npcNo = None
		self.npcTitle = None
		self.devamButton = None
		self.cancelButton = None
		self.imageButton = None
		self.Hide()

	def SelectNpc(self, npcNo):
		global DarkmmoMarek38Npc

		if (self.npcNo == npcNo):
			return

		self.npcNo = npcNo
		for (tabKey, tabButton) in self.npcSec.items():
			if (tabKey != npcNo):
				tabButton.SetUp()
		self.npcSec[self.npcNo].Down()

		if self.npcNo == 0:
			self.npcTitle.SetText(localeinfo.KASMIR_MARKET_PRIVATESHOP_0)
		elif self.npcNo == 1:
			self.npcTitle.SetText(localeinfo.KASMIR_MARKET_PRIVATESHOP_1)
		elif self.npcNo == 2:
			self.npcTitle.SetText(localeinfo.KASMIR_MARKET_PRIVATESHOP_2)
		elif self.npcNo == 3:
			self.npcTitle.SetText(localeinfo.KASMIR_MARKET_PRIVATESHOP_3)
		elif self.npcNo == 4:
			self.npcTitle.SetText(localeinfo.KASMIR_MARKET_PRIVATESHOP_4)
		elif self.npcNo == 5:
			self.npcTitle.SetText(localeinfo.KASMIR_MARKET_PRIVATESHOP_5)
		elif self.npcNo == 6:
			self.npcTitle.SetText(localeinfo.KASMIR_MARKET_PRIVATESHOP_6)
		elif self.npcNo == 7:
			self.npcTitle.SetText(localeinfo.KASMIR_MARKET_PRIVATESHOP_7)

		DarkmmoMarek38Npc = self.npcNo + 30000

		self.MarketResimleri(DarkmmoMarek38Npc)

	def MarketResimleri(self, resim):
		race = resim
		self.img = ui.ImageBox()
		self.img.SetParent(self.imageButton)
		self.img.SetPosition(15, 45)
		self.img.LoadImage(PAZAR_IMAGE_DICT[race])
		self.img.Show()

	def NextButton(self):
		self.Close()
		MarketDialog = KasmirPaketiDialog()
		MarketDialog.Open()
		self.MarketDialog = MarketDialog

	def KapatButton(self):
		self.Close()

class KasmirPaketiDialog(ui.ScriptWindow):
	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.baslik = -1
		self.LoadWindow()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def LoadWindow(self):
		try:
			PythonScriptLoader = ui.PythonScriptLoader()
			PythonScriptLoader.LoadScriptFile(self, "uiscript/kasmirpaketidialog.py")
		except:
			import exception
			exception.Abort("KasmirPaketiDialog.LoadWindow.LoadObject")

		try:
			self.board = self.GetChild("board")
			self.baslikTitle = self.GetChild("Baslik_Title")
			self.geriButton = self.GetChild("GeriButton")
			self.acceptButton = self.GetChild("AgreeButton")
			self.imageButton = self.GetChild("baslik_view")

			self.baslikSec = {
				0 : self.GetChild("Normal"),
				1 : self.GetChild("Fire"),
				2 : self.GetChild("Ice"),
				3 : self.GetChild("Paper"),
				4 : self.GetChild("Ribon"),
				5 : self.GetChild("Wing"),
			}
		except:
			import exception
			exception.Abort("KasmirPaketiDialog.LoadWindow.BindObject")

		self.board.SetCloseEvent(ui.__mem_func__(self.Close))
		self.baslikTitle.SetMultiLine()
		self.baslikTitle.SetLimitWidth(187)
		self.baslikTitle.SetMax(250)

		self.geriButton.SetEvent(ui.__mem_func__(self.BackButton))
		self.acceptButton.SetEvent(ui.__mem_func__(self.BittiButton))

		for (tabKey, tabButton) in self.baslikSec.items():
			tabButton.SetEvent(ui.__mem_func__(self.SelectBaslik), tabKey)

	def Open(self):
		global DarkmmoMarek38Baslik

		self.SetCenterPosition()
		self.SetTop()
		self.Show()

		self.baslik = 0
		self.baslikSec[0].Down()
		self.baslikTitle.SetText(localeinfo.KASMIR_PAKET_STANDART)

		if DarkmmoMarek38Baslik >= 0:
			DarkmmoMarek38Baslik = 1

		self.MarketResimleri(DarkmmoMarek38Npc)
		self.MarketBasliklari()

	def Close(self):
		self.ClearDictionary()
		self.baslik = None
		self.baslikTitle = None
		self.geriButton = None
		self.acceptButton = None
		self.imageButton = None
		self.Hide()

	def SelectBaslik(self, baslik):
		global DarkmmoMarek38Baslik

		if (self.baslik == baslik):
			return

		self.baslik = baslik
		for (tabKey, tabButton) in self.baslikSec.items():
			if (tabKey != baslik):
				tabButton.SetUp()
		self.baslikSec[self.baslik].Down()

		if self.baslik == 0:
			self.baslikTitle.SetText(localeinfo.KASMIR_PAKET_STANDART)
		elif self.baslik == 1:
			self.baslikTitle.SetText(localeinfo.KASMIR_PAKET_FIRE)
		elif self.baslik == 2:
			self.baslikTitle.SetText(localeinfo.KASMIR_PAKET_ICE)
		elif self.baslik == 3:
			self.baslikTitle.SetText(localeinfo.KASMIR_PAKET_PAPER)
		elif self.baslik == 4:
			self.baslikTitle.SetText(localeinfo.KASMIR_PAKET_RIBON)
		elif self.baslik == 5:
			self.baslikTitle.SetText(localeinfo.KASMIR_PAKET_WING)

		DarkmmoMarek38Baslik = self.baslik + 1
		self.MarketBasliklari()

	def MarketResimleri(self, resim):
		race = resim
		self.img = ui.ImageBox()
		self.img.SetParent(self.imageButton)
		self.img.SetPosition(15, 45)
		self.img.LoadImage(PAZAR_IMAGE_DICT[race])
		self.img.Show()

	def MarketBasliklari(self):
		if DarkmmoMarek38Baslik == 1:
			self.Darkmmo_marek38 = KasmirBaslikBoardNormal()
		elif DarkmmoMarek38Baslik == 2:
			self.Darkmmo_marek38 = KasmirBaslikBoardFire()
		elif DarkmmoMarek38Baslik == 3:
			self.Darkmmo_marek38 = KasmirBaslikBoardIce()
		elif DarkmmoMarek38Baslik == 4:
			self.Darkmmo_marek38 = KasmirBaslikBoardPaper()
		elif DarkmmoMarek38Baslik == 5:
			self.Darkmmo_marek38 = KasmirBaslikBoardRibon()
		elif DarkmmoMarek38Baslik == 6:
			self.Darkmmo_marek38 = KasmirBaslikBoardWing()
		else:
			self.Darkmmo_marek38 = KasmirBaslikBoardNormal()

		self.Darkmmo_marek38.SetParent(self.imageButton)
		self.Darkmmo_marek38.SetPosition(70, 30)
		self.Darkmmo_marek38.Open()
		self.Darkmmo_marek38.Show()

	def BackButton(self):
		self.Close()
		PaketDialog = KasmirMarketiDialog()
		PaketDialog.Open()
		self.PaketDialog = PaketDialog

	def BittiButton(self):
		global DarkmmoMarek38Npc
		global DarkmmoMarek38Baslik

		self.Close()

		if (DarkmmoMarek38Npc < 0 or DarkmmoMarek38Npc == 0):
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.UIMARKETSYS_DIALOG1 % (1))
			return True

		if (DarkmmoMarek38Baslik < 0 or DarkmmoMarek38Baslik == 0):
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.UIMARKETSYS_DIALOG1 % (2))
			return True

		marek38.VERI_PAKETI1=1

	def SetKasmirSystemNpc(self, npc):
		chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.UIMARKETSYS_DIALOG2 % (npc))

	def SetKasmirSystemBaslik(self, baslik):
		chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.UIMARKETSYS_DIALOG3 % (baslik))

#############################################################################################
#############################################################################################
#############################################################################################
class KasmirBaslikBoardNormal(ui.ThinBoard_Darkmmo_Normal):
	def __init__(self):
		ui.ThinBoard_Darkmmo_Normal.__init__(self, "UI_BOTTOM")
		self.__MakeTextLine()

	def __del__(self):
		ui.ThinBoard_Darkmmo_Normal.__del__(self)

	def __MakeTextLine(self):
		self.textLine = ui.TextLine()
		self.textLine.SetParent(self)
		self.textLine.SetWindowHorizontalAlignCenter()
		self.textLine.SetWindowVerticalAlignCenter()
		self.textLine.SetHorizontalAlignCenter()
		self.textLine.SetVerticalAlignCenter()
		self.textLine.Show()

	def Open(self):
		self.textLine.UpdateRect()
		self.SetSize(62, 20)
		self.Show() 

class KasmirBaslikBoardFire(ui.ThinBoard_Darkmmo_Fire):
	def __init__(self):
		ui.ThinBoard_Darkmmo_Fire.__init__(self, "UI_BOTTOM")
		self.__MakeTextLine()

	def __del__(self):
		ui.ThinBoard_Darkmmo_Fire.__del__(self)

	def __MakeTextLine(self):
		self.textLine = ui.TextLine()
		self.textLine.SetParent(self)
		self.textLine.SetWindowHorizontalAlignCenter()
		self.textLine.SetWindowVerticalAlignCenter()
		self.textLine.SetHorizontalAlignCenter()
		self.textLine.SetVerticalAlignCenter()
		self.textLine.Show()

	def Open(self):
		self.textLine.UpdateRect()
		self.SetSize(62, 20)
		self.Show() 

class KasmirBaslikBoardIce(ui.ThinBoard_Darkmmo_Ice):
	def __init__(self):
		ui.ThinBoard_Darkmmo_Ice.__init__(self, "UI_BOTTOM")
		self.__MakeTextLine()

	def __del__(self):
		ui.ThinBoard_Darkmmo_Ice.__del__(self)

	def __MakeTextLine(self):
		self.textLine = ui.TextLine()
		self.textLine.SetParent(self)
		self.textLine.SetWindowHorizontalAlignCenter()
		self.textLine.SetWindowVerticalAlignCenter()
		self.textLine.SetHorizontalAlignCenter()
		self.textLine.SetVerticalAlignCenter()
		self.textLine.Show()

	def Open(self):
		self.textLine.UpdateRect()
		self.SetSize(62, 20)
		self.Show() 

class KasmirBaslikBoardPaper(ui.ThinBoard_Darkmmo_Paper):
	def __init__(self):
		ui.ThinBoard_Darkmmo_Paper.__init__(self, "UI_BOTTOM")
		self.__MakeTextLine()

	def __del__(self):
		ui.ThinBoard_Darkmmo_Paper.__del__(self)

	def __MakeTextLine(self):
		self.textLine = ui.TextLine()
		self.textLine.SetParent(self)
		self.textLine.SetWindowHorizontalAlignCenter()
		self.textLine.SetWindowVerticalAlignCenter()
		self.textLine.SetHorizontalAlignCenter()
		self.textLine.SetVerticalAlignCenter()
		self.textLine.Show()

	def Open(self):
		self.textLine.UpdateRect()
		self.SetSize(62, 20)
		self.Show() 

class KasmirBaslikBoardRibon(ui.ThinBoard_Darkmmo_Ribon):
	def __init__(self):
		ui.ThinBoard_Darkmmo_Ribon.__init__(self, "UI_BOTTOM")
		self.__MakeTextLine()

	def __del__(self):
		ui.ThinBoard_Darkmmo_Ribon.__del__(self)

	def __MakeTextLine(self):
		self.textLine = ui.TextLine()
		self.textLine.SetParent(self)
		self.textLine.SetWindowHorizontalAlignCenter()
		self.textLine.SetWindowVerticalAlignCenter()
		self.textLine.SetHorizontalAlignCenter()
		self.textLine.SetVerticalAlignCenter()
		self.textLine.Show()

	def Open(self):
		self.textLine.UpdateRect()
		self.SetSize(62, 20)
		self.Show() 

class KasmirBaslikBoardWing(ui.ThinBoard_Darkmmo_Wing):
	def __init__(self):
		ui.ThinBoard_Darkmmo_Wing.__init__(self, "UI_BOTTOM")
		self.__MakeTextLine()

	def __del__(self):
		ui.ThinBoard_Darkmmo_Wing.__del__(self)

	def __MakeTextLine(self):
		self.textLine = ui.TextLine()
		self.textLine.SetParent(self)
		self.textLine.SetWindowHorizontalAlignCenter()
		self.textLine.SetWindowVerticalAlignCenter()
		self.textLine.SetHorizontalAlignCenter()
		self.textLine.SetVerticalAlignCenter()
		self.textLine.Show()

	def Open(self):
		self.textLine.UpdateRect()
		self.SetSize(62, 20)
		self.Show() 
#############################################################################################
#############################################################################################
#############################################################################################

class InputDialogKasmir(ui.ScriptWindow):
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

	def GetNpcKasmir(self):
		return DarkmmoMarek38Npc

	def GetBaslikKasmir(self):
		return DarkmmoMarek38Baslik
