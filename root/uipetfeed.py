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

class PetFeedWindow(ui.ScriptWindow):

	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.arryfeed = [-1, -1, -1, -1, -1, -1, -1, -1, -1]
		self.__LoadWindow()
		

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def Show(self):
		ui.ScriptWindow.Show(self)
			
	def Close(self):
		for x in range(len(self.arryfeed)):
			self.arryfeed[x] = -1
			self.petslot.ClearSlot(x)
			self.petslot.RefreshSlot()
		self.Hide()
		constinfo.FEEDWIND = 0
	
	def __LoadWindow(self):
		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "uiscript/petfeedwindow.py")
		except:
			import exception
			exception.Abort("PetFeedWindow.LoadWindow.LoadObject")
			
		try:
			self.petfeed = self.GetChild("board")
			self.closebtn = self.GetChild("PetFeed_TitleBar")
			self.petslot = self.GetChild("FeedItemSlot")
			self.feedbtn = self.GetChild("FeedButton")
			
			##PetSlot
			
			self.petslot.SetSelectEmptySlotEvent(ui.__mem_func__(self.SelectEmptySlot))
			self.petslot.SetSelectItemSlotEvent(ui.__mem_func__(self.SelectItemSlot))
			self.petslot.SetUnselectItemSlotEvent(ui.__mem_func__(self.UseItemSlot))
			self.petslot.SetUseSlotEvent(ui.__mem_func__(self.UseItemSlot))
			
			##Event secondari
			
			self.feedbtn.SetEvent(ui.__mem_func__(self.SendPetItem))
			self.closebtn.SetCloseEvent(self.Close)
			
			
		except:
			import exception
			exception.Abort("PetFeedWindow.LoadWindow.BindObject")
	
	def SelectEmptySlot(self, selectedSlotPos):
		#chat.AppendChat(chat.CHAT_TYPE_INFO, "Empty"+str(selectedSlotPos))		

		if mousemodule.mouseController.isAttached():

			attachedSlotType = mousemodule.mouseController.GetAttachedType()
			attachedSlotPos = mousemodule.mouseController.GetAttachedSlotNumber()
			attachedItemCount = mousemodule.mouseController.GetAttachedItemCount()
			attachedItemIndex = mousemodule.mouseController.GetAttachedItemIndex()
			if attachedItemCount > 1:
				chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.PETFEED_DIALOG1)
				return
			if attachedItemCount == 1:
				attachedItemCount = 0
				
			if player.SLOT_TYPE_INVENTORY == attachedSlotType and not attachedSlotPos in self.arryfeed:
				itemCount = player.GetItemCount(attachedSlotPos)
				attachedCount = mousemodule.mouseController.GetAttachedItemCount()
				self.arryfeed[selectedSlotPos] = attachedSlotPos
				self.petslot.SetItemSlot(selectedSlotPos, attachedItemIndex, attachedItemCount)

			mousemodule.mouseController.DeattachObject()
	
	def SelectItemSlot(self, itemSlotIndex):
		chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.PETFEED_DIALOG2 % (str(itemSlotIndex)))
		self.arryfeed[itemSlotIndex] = -1
		self.petslot.ClearSlot(itemSlotIndex)
		self.petslot.RefreshSlot()
	
	def UseItemSlot(self, slotIndex):
		chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.PETFEED_DIALOG2 % (str(slotIndex)))
		
	def SendPetItem(self):
		for i in range(len(self.arryfeed)):
			if self.arryfeed[i] != -1:
				#chat.AppendChat(chat.CHAT_TYPE_INFO, "Oggetto inviato in pos"+str(i))
				net.SendChatPacket("/cubepetadd add %d %d" % (i, self.arryfeed[i]))
		net.SendChatPacket("/feedcubepet %d" % (constinfo.FEEDWIND))
		for x in range(len(self.arryfeed)):
			self.arryfeed[x] = -1
			self.petslot.ClearSlot(x)
			self.petslot.RefreshSlot()
		


