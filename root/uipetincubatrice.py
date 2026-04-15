import os
import ui
import player
import mousemodule
import net
import app
import snd
import item
import chat
import grp
import uiscriptlocale
import constinfo
import ime
import wndMgr
import localeinfo

class PetSystemIncubator(ui.ScriptWindow):
	
	def __init__(self, new_pet):
		ui.ScriptWindow.__init__(self)
		self.__LoadWindow()
		self.LoadPetIncubatorImg(new_pet)

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def Show(self):
		ui.ScriptWindow.Show(self)

	def Close(self):
		self.Hide()
	
	def __LoadWindow(self):
		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "uiscript/pethatchingwindow.py")
		except:
			import exception
			exception.Abort("PetHatchingWindow.LoadWindow.LoadObject")
			
		try:
			self.board = self.GetChild("board")
			self.boardtitle = self.GetChild("PetHatching_TitleBar")
			
			self.petimg = self.GetChild("HatchingItemSlot")
			self.petname = self.GetChild("pet_name")
			self.HatchingButton = self.GetChild("HatchingButton")
			
			
			#Event
			self.boardtitle.SetCloseEvent(ui.__mem_func__(self.Close))
			self.HatchingButton.SetEvent(ui.__mem_func__(self.RequestHatching,))
			
			
		except:
			import exception
			exception.Abort("PetHatchingWindow.LoadWindow.BindObject")
			
	def LoadPetIncubatorImg(self, new_pet):
		petarryname = [localeinfo.PETINCUBATRICE_DIALOG4, localeinfo.PETINCUBATRICE_DIALOG5, localeinfo.PETINCUBATRICE_DIALOG6, localeinfo.PETINCUBATRICE_DIALOG7, localeinfo.PETINCUBATRICE_DIALOG8, localeinfo.PETINCUBATRICE_DIALOG9, localeinfo.PETINCUBATRICE_DIALOG10, localeinfo.PETINCUBATRICE_DIALOG11, localeinfo.PETINCUBATRICE_DIALOG12, localeinfo.PETINCUBATRICE_DIALOG13, localeinfo.PETINCUBATRICE_DIALOG14]
		petarryimg = [55701, 55702, 55703, 55704, 55705, 55706, 55707, 55708, 55709, 55710, 55711]
		chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.PETINCUBATRICE_DIALOG2 % (str(petarryname[int(new_pet)])))
		self.petimg.SetItemSlot(0,petarryimg[int(new_pet)], 0)
		self.petimg.SetAlwaysRenderCoverButton(0, True)
		#self.petimg.SetSlotBaseImage("icon/item/"+petarryimg[new_pet], 0, 0, 0, 0)
		
		
			
			
	def RequestHatching(self):
		if self.petname.GetText() == "" or len(self.petname.GetText()) < 4:
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.PETINCUBATRICE_DIALOG1)
			return
			
		if player.GetElk() < 100000:
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.PETINCUBATRICE_DIALOG2 % (str(localeinfo.NumberToMoneyString(100000))))
			return
			
		#chat.AppendChat(chat.CHAT_TYPE_INFO, "[Pet-Incubator]Invio pacchetto schiudi pet.")
		import chr
		chr.RequestPetName(self.petname.GetText())
		self.Close()


			


