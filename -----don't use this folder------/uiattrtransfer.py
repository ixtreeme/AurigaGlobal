import app
import net
import player
import grp
import snd
import item
import chat
import mousemodule

import ui
import uitooltip
import uiscriptlocale
import uiinventory
import localeinfo

## If your system can transfer the bonuses from others costumes subtypes set ATTR_TRANSFER_LIMIT to 1 ##
ATTR_TRANSFER_LIMIT = 0
########################################################################################################

class TransferWindow(ui.ScriptWindow):
	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.isUsable = False
		self.isLoaded = 0
		self.xShopStart = 0
		self.yShopStart = 0
		self.def_mediumSlot = 999
		self.def_combSlot_1 = 999
		self.def_combSlot_2 = 999
		self.inventory = None

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def BindInventory(self, inventory):
		self.inventory = inventory

	def LoadWindow(self):
		if self.isLoaded == 1:
			return
		
		self.isLoaded = 1
		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "uiscript/itemcombinationwindow.py")
		except:
			import exception
			exception.Abort("AcceWindow.LoadDialog.LoadScript")
		
		try:
			GetObject = self.GetChild
			self.titleBar = GetObject("TitleBar")
			self.btnAccept = GetObject("AcceptButton")
			self.btnCancel = GetObject("CancelButton")
			self.mediumSlot = GetObject("MediumSlot")
			self.combSlot = GetObject("CombSlot")
		except:
			import exception
			exception.Abort("AcceWindow.LoadDialog.BindObject")
		
		self.titleBar.SetCloseEvent(ui.__mem_func__(self.__OnCloseButtonClick))
		self.btnCancel.SetEvent(ui.__mem_func__(self.__OnCloseButtonClick))
		self.btnAccept.SetEvent(ui.__mem_func__(self.__OnAcceptButtonClick))
		self.combSlot.SetSelectEmptySlotEvent(ui.__mem_func__(self.__OnSelectEmptySlot))
		self.combSlot.SetSelectItemSlotEvent(ui.__mem_func__(self.__OnSelectItemSlot))
		self.combSlot.SetOverInItemEvent(ui.__mem_func__(self.__OnOverInItem))
		self.combSlot.SetOverOutItemEvent(ui.__mem_func__(self.__OnOverOutItem))
		self.mediumSlot.SetSelectEmptySlotEvent(ui.__mem_func__(self.__OnSelectEmptySlot))
		self.mediumSlot.SetSelectItemSlotEvent(ui.__mem_func__(self.__OnSelectItemSlot))
		self.mediumSlot.SetOverInItemEvent(ui.__mem_func__(self.__OnOverInItem))
		self.mediumSlot.SetOverOutItemEvent(ui.__mem_func__(self.__OnOverOutItem))
		
		self.tooltipItem = None
		self.SLOT_SIZEX	= 32
		self.SLOT_SIZEY	= 32
		self.itemInfo = {}

	def IsOpened(self):
		if self.isLoaded == 1:
			return True
		
		return False

	def SetItemToolTip(self, itemTooltip):
		self.tooltipItem = itemTooltip

	def Destroy(self):
		self.ClearDictionary()
		self.titleBar = None
		self.btnAccept = None
		self.btnCancel = None
		self.combSlot = None
		self.mediumSlot = None
		self.tooltipItem = None
		self.def_mediumSlot = None
		self.def_combSlot_1 = None
		self.def_combSlot_2 = None
		self.inventory = None

	def Open(self):
		self.Refresh()
		self.Show()
		self.isUsable = True
		(self.xShopStart, self.yShopStart, z) = player.GetMainCharacterPosition()
		for i in xrange(4):
			self.combSlot.ClearSlot(i)
		
		self.mediumSlot.ClearSlot(0)
		self.def_mediumSlot = 999
		self.def_combSlot_1 = 999
		self.def_combSlot_2 = 999
		self.itemInfo = {}
		self.SetCenterPosition()

	def OnPressEscapeKey(self):
		self.__OnCloseButtonClick()
		return True

	def Close(self):
		if self.tooltipItem:
			self.tooltipItem.HideToolTip()
		
		self.Hide()
		self.Clear()

	def Clear(self):
		if self.inventory:
			self.inventory.ClearTransferSlot()
		
		for i in xrange(4):
			if i > 0:
				self.combSlot.ClearSlot(i)
				net.SendChatPacket("/attrtransfer del %d" % (i))
		
		self.def_combSlot_1 = 999
		self.def_combSlot_2 = 999
		
		self.mediumSlot.ClearSlot(0)
		self.def_mediumSlot = 999

	def Refresh(self):
		pass

	def Success(self):
		self.mediumSlot.ClearSlot(0)
		self.def_mediumSlot = 999
		self.Clear()

	def __OnCloseButtonClick(self):
		if self.isUsable:
			self.isUsable = False
			
			print "Bonus Transer closed!"
			net.SendChatPacket("/attrtransfer close")
		
		self.Close()

	def __OnAcceptButtonClick(self):
		print "Bonus Transer make!"
		net.SendChatPacket("/attrtransfer make")

	def __OnSelectEmptySlot(self, selectedSlotPos):
		isAttached = mousemodule.mouseController.isAttached()
		if isAttached:
			attachedSlotType = mousemodule.mouseController.GetAttachedType()
			attachedSlotPos = mousemodule.mouseController.GetAttachedSlotNumber()
			targetIndex = player.GetItemIndex(attachedSlotPos)
			if attachedSlotType != player.SLOT_TYPE_INVENTORY:
				return
			
			if selectedSlotPos == 0:
				item.SelectItem(targetIndex)
				if item.GetItemType() == item.ITEM_TYPE_TRANSFER_SCROLL:
					if self.def_mediumSlot != 999:
						self.mediumSlot.ClearSlot(0)
						self.def_mediumSlot = 999
					
					self.mediumSlot.SetItemSlot(0, player.GetItemIndex(attachedSlotPos), 1)
					net.SendChatPacket("/attrtransfer add %d %d" % (0, attachedSlotPos))
					self.def_mediumSlot = selectedSlotPos
					
					for slotPos, invenPos in self.itemInfo.items():
						if invenPos == attachedSlotPos:
							del self.itemInfo[slotPos]
					
					self.itemInfo[selectedSlotPos] = attachedSlotPos
					if self.inventory:
						self.inventory.TransferSlot(selectedSlotPos, attachedSlotPos)
				
				mousemodule.mouseController.DeattachObject()
			else:
				mousemodule.mouseController.DeattachObject()
				if self.def_mediumSlot == 999:
					chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.COMB_ALERT1)
					return
				
				if self.def_combSlot_2 == 999 and selectedSlotPos == 1:
					chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.COMB_ALERT2)
					return
				
				item.SelectItem(targetIndex)
				itemType = item.GetItemType()
				itemSubType = item.GetItemSubType()
				if itemType != item.ITEM_TYPE_COSTUME:
					chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.COMB_ALERT3)
					return
				
				if app.ENABLE_STOLE_REAL:
					if not itemSubType in (item.COSTUME_TYPE_BODY, item.COSTUME_TYPE_HAIR, item.COSTUME_TYPE_WEAPON, item.COSTUME_TYPE_STOLE):
						chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.COMB_ALERT4)
						return
				else:
					if not itemSubType in (item.COSTUME_TYPE_BODY, item.COSTUME_TYPE_HAIR, item.COSTUME_TYPE_WEAPON):
						chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.COMB_ALERT4)
						return
				
				if selectedSlotPos == 1:
					vnum = player.GetItemIndex(self.def_combSlot_2)
					item.SelectItem(vnum)
					Costume_1_SubType = item.GetItemSubType()
					myhair =	[73001,
								73002,
								73003,
								73004,
								73005,
								73006,
								73007,
								73008,
								73009,
								73010,
								73011,
								73012,
								75001,
								75002,
								75003,
								75004,
								75005,
								75006,
								75007,
								75008,
								75009,
								75010,
								75011,
								75012,
								75201,
								75202,
								75203,
								75204,
								75205,
								75206,
								75207,
								75208,
								75209,
								75210,
								75211,
								75212,
								73251,
								73252,
								73253,
								73254,
								73255,
								73256,
								73257,
								73258,
								73259,
								73260,
								73261,
								73262,
								73501,
								73502,
								73503,
								73504,
								73505,
								73506,
								73507,
								73508,
								73509,
								73510,
								73511,
								73512,
								75401,
								75402,
								75403,
								75404,
								75405,
								75406,
								75407,
								75408,
								75409,
								75410,
								75411,
								75412,
								73751,
								73752,
								73753,
								73754,
								73755,
								73756,
								73757,
								73758,
								73759,
								73760,
								73761,
								73762,
								75601,
								75602,
								75603,
								75604,
								75605,
								75606,
								75607,
								75608,
								75609,
								75610,
								75611,
								75612]
					
					if Costume_1_SubType != itemSubType or vnum in myhair:
						chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.COMB_ALERT6)
						return
				elif selectedSlotPos == 2:
					hasAttr = False
					for i in xrange(player.ATTRIBUTE_SLOT_NORM_NUM - 2):
						(t, v) = player.GetItemAttribute(attachedSlotPos, i)
						if t != 0:
							hasAttr = True
							break;
					
					if not hasAttr:
						chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.COMB_ALERT7)
						return
				
				self.combSlot.SetItemSlot(selectedSlotPos, player.GetItemIndex(attachedSlotPos), player.GetItemCount(attachedSlotPos))
				net.SendChatPacket("/attrtransfer add %d %d" % (selectedSlotPos, attachedSlotPos))
				if selectedSlotPos == 1:
					self.def_combSlot_1 = attachedSlotPos
				elif selectedSlotPos == 2:
					self.def_combSlot_2 = attachedSlotPos
				
				for slotPos, invenPos in self.itemInfo.items():
					if invenPos == attachedSlotPos:
						del self.itemInfo[slotPos]
				
				self.itemInfo[selectedSlotPos] = attachedSlotPos
				if self.inventory:
					self.inventory.TransferSlot(selectedSlotPos, attachedSlotPos)
				
				if selectedSlotPos == 1:
					self.combSlot.SetItemSlot(3, player.GetItemIndex(self.def_combSlot_1), 1)

	def __OnSelectItemSlot(self, selectedSlotPos):
		if selectedSlotPos == 3:
			return
		
		self.Clear()
		if selectedSlotPos == 0:
			if self.def_mediumSlot != 999:
				self.mediumSlot.ClearSlot(0)
				self.def_mediumSlot = 999

	def __OnOverInItem(self, slotIndex):
		if self.tooltipItem:
			if slotIndex == 3:
				self.tooltipItem.SetInventoryItemAttrTransfer(self.itemInfo[1], self.itemInfo[2])
			else:
				if self.itemInfo.has_key(slotIndex):
					self.tooltipItem.SetInventoryItem(self.itemInfo[slotIndex])

	def __OnOverOutItem(self):
		if self.tooltipItem:
			self.tooltipItem.HideToolTip()

	def OnUpdate(self):
		USE_SHOP_LIMIT_RANGE = 1000
		(x, y, z) = player.GetMainCharacterPosition()
		if abs(x - self.xShopStart) > USE_SHOP_LIMIT_RANGE or abs(y - self.yShopStart) > USE_SHOP_LIMIT_RANGE:
			self.__OnCloseButtonClick()
