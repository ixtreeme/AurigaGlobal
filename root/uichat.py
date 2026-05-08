import app
if app.ENABLE_NEW_CHAT or app.LINK_IN_CHAT:
	import player
import dbg
import net
import ui
import grp
import chat
import wndMgr
import ime
import colorinfo
import constinfo
import systemSetting
import localeinfo
if app.ENABLE_NEW_CHAT:
	import os
	import uicommon, uiscriptlocale

ENABLE_CHAT_COMMAND = True
ENABLE_LAST_SENTENCE_STACK = True
ENABLE_INSULT_CHECK = True

if localeinfo.IsHONGKONG():
	ENABLE_LAST_SENTENCE_STACK = True

if localeinfo.IsEUROPE():
	ENABLE_CHAT_COMMAND = False

if localeinfo.IsCANADA():
	ENABLE_LAST_SENTENCE_STACK = False

chatInputSetList = []
def InsertChatInputSetWindow(wnd):
	global chatInputSetList
	chatInputSetList.append(wnd)
def RefreshChatMode():
	global chatInputSetList
	map(lambda wnd:wnd.OnRefreshChatMode(), chatInputSetList)
def DestroyChatInputSetWindow():
	global chatInputSetList
	chatInputSetList = []

## ChatModeButton
class ChatModeButton(ui.Window):

	OUTLINE_COLOR = grp.GenerateColor(1.0, 1.0, 1.0, 1.0)
	OVER_COLOR = grp.GenerateColor(1.0, 1.0, 1.0, 0.3)
	BUTTON_STATE_UP = 0
	BUTTON_STATE_OVER = 1
	BUTTON_STATE_DOWN = 2

	def __init__(self):
		ui.Window.__init__(self)
		self.state = None
		self.buttonText = None
		self.event = None
		self.SetWindowName("ChatModeButton")

		net.EnableChatInsultFilter(ENABLE_INSULT_CHECK)

	def __del__(self):
		ui.Window.__del__(self)

	def SAFE_SetEvent(self, event):
		self.event=ui.__mem_func__(event)

	def SetText(self, text):
		if None == self.buttonText:
			textLine = ui.TextLine()
			textLine.SetParent(self)
			textLine.SetWindowHorizontalAlignCenter()
			textLine.SetWindowVerticalAlignCenter()
			textLine.SetVerticalAlignCenter()
			textLine.SetHorizontalAlignCenter()
			textLine.SetPackedFontColor(self.OUTLINE_COLOR)
			textLine.Show()
			self.buttonText = textLine

		self.buttonText.SetText(text)

	def SetSize(self, width, height):
		self.width = width
		self.height = height
		ui.Window.SetSize(self, width, height)

	def OnMouseOverIn(self):
		self.state = self.BUTTON_STATE_OVER

	def OnMouseOverOut(self):
		self.state = self.BUTTON_STATE_UP

	def OnMouseLeftButtonDown(self):
		self.state = self.BUTTON_STATE_DOWN

	def OnMouseLeftButtonUp(self):
		self.state = self.BUTTON_STATE_UP
		if self.IsIn():
			self.state = self.BUTTON_STATE_OVER

		if None != self.event:
			self.event()

	def OnRender(self):

		(x, y) = self.GetGlobalPosition()

		grp.SetColor(self.OUTLINE_COLOR)
		grp.RenderRoundBox(x, y, self.width, self.height)

		if self.state >= self.BUTTON_STATE_OVER:
			grp.RenderRoundBox(x+1, y, self.width-2, self.height)
			grp.RenderRoundBox(x, y+1, self.width, self.height-2)

			if self.BUTTON_STATE_DOWN == self.state:
				grp.SetColor(self.OVER_COLOR)
				grp.RenderBar(x+1, y+1, self.width-2, self.height-2)

## ChatLine
class ChatLine(ui.EditLine):

	CHAT_MODE_NAME = {	chat.CHAT_TYPE_TALKING : localeinfo.CHAT_NORMAL,
						chat.CHAT_TYPE_PARTY : localeinfo.CHAT_PARTY,
						chat.CHAT_TYPE_GUILD : localeinfo.CHAT_GUILD,
						chat.CHAT_TYPE_SHOUT : localeinfo.CHAT_SHOUT, }

	def __init__(self):
		ui.EditLine.__init__(self)
		self.SetWindowName("Chat Line")
		self.lastShoutTime = 15
		self.eventEscape = lambda *arg: None
		self.eventReturn = lambda *arg: None
		self.eventTab = None
		self.chatMode = chat.CHAT_TYPE_TALKING
		self.bCodePage = True

		self.overTextLine = ui.TextLine()
		self.overTextLine.SetParent(self)
		self.overTextLine.SetPosition(-1, 0)
		self.overTextLine.SetFontColor(1.0, 1.0, 0.0)
		self.overTextLine.SetOutline()
		self.overTextLine.Hide()

		self.lastSentenceStack = []
		self.lastSentencePos = 0

	def SetChatMode(self, mode):
		self.chatMode = mode

	def GetChatMode(self):
		return self.chatMode

	def ChangeChatMode(self):
		if chat.CHAT_TYPE_TALKING == self.GetChatMode():
			self.SetChatMode(chat.CHAT_TYPE_PARTY)
			self.SetText("#")
			self.SetEndPosition()

		elif chat.CHAT_TYPE_PARTY == self.GetChatMode():
			self.SetChatMode(chat.CHAT_TYPE_GUILD)
			self.SetText("%")
			self.SetEndPosition()

		elif chat.CHAT_TYPE_GUILD == self.GetChatMode():
			self.SetChatMode(chat.CHAT_TYPE_SHOUT)
			self.SetText("!")
			self.SetEndPosition()

		elif chat.CHAT_TYPE_SHOUT == self.GetChatMode():
			self.SetChatMode(chat.CHAT_TYPE_TALKING)
			self.SetText("")

		self.__CheckChatMark()

	def GetCurrentChatModeName(self):
		try:
			return self.CHAT_MODE_NAME[self.chatMode]
		except:
			import exception
			exception.Abort("ChatLine.GetCurrentChatModeName")

	def SAFE_SetEscapeEvent(self, event):
		self.eventReturn = ui.__mem_func__(event)

	def SAFE_SetReturnEvent(self, event):
		self.eventEscape = ui.__mem_func__(event)

	def SAFE_SetTabEvent(self, event):
		self.eventTab = ui.__mem_func__(event)

	def SetTabEvent(self, event):
		self.eventTab = event

	def OpenChat(self):
		self.SetFocus()
		self.__ResetChat()

	def __ClearChat(self):
		self.SetText("")
		self.lastSentencePos = 0

	def __ResetChat(self):
		if chat.CHAT_TYPE_PARTY == self.GetChatMode():
			self.SetText("#")
			self.SetEndPosition()
		elif chat.CHAT_TYPE_GUILD == self.GetChatMode():
			self.SetText("%")
			self.SetEndPosition()
		elif chat.CHAT_TYPE_SHOUT == self.GetChatMode():
			self.SetText("!")
			self.SetEndPosition()
		else:
			self.__ClearChat()

		self.__CheckChatMark()

	def __SendChatPacket(self, text, type):
#		if text[0] == '/':
#			if ENABLE_CHAT_COMMAND or constinfo.CONSOLE_ENABLE:
#				pass
#			else:
#				return
		if net.IsChatInsultIn(text):
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.CHAT_INSULT_STRING)
		else:
			if app.LINK_IN_CHAT:
				link = constinfo.GetLink(text)
				if link != "":
					text = text.replace(link, "|cFF00C0FC|h|Hsysweb:" + link.replace("://", "XxX") + "|h" + link + "|h|r")
			
			net.SendChatPacket(text, type)

	def __SendPartyChatPacket(self, text):

		if 1 == len(text):
			self.RunCloseEvent()
			return

		self.__SendChatPacket(text[1:], chat.CHAT_TYPE_PARTY)
		self.__ResetChat()

	def __SendGuildChatPacket(self, text):

		if 1 == len(text):
			self.RunCloseEvent()
			return

		self.__SendChatPacket(text[1:], chat.CHAT_TYPE_GUILD)
		self.__ResetChat()

	def __SendShoutChatPacket(self, text):

		if 1 == len(text):
			self.RunCloseEvent()
			return

		if app.GetTime() < self.lastShoutTime + 15:
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.CHAT_SHOUT_LIMIT)
			self.__ResetChat()
			return

		self.__SendChatPacket(text[1:], chat.CHAT_TYPE_SHOUT)
		self.__ResetChat()

		self.lastShoutTime = app.GetTime()

	def __SendTalkingChatPacket(self, text):
		self.__SendChatPacket(text, chat.CHAT_TYPE_TALKING)
		self.__ResetChat()

	def OnIMETab(self):
		#if None != self.eventTab:
		#	self.eventTab()
		#return True
		return False

	def OnIMEUpdate(self):
		ui.EditLine.OnIMEUpdate(self)
		self.__CheckChatMark()

	def __CheckChatMark(self):

		self.overTextLine.Hide()

		text = self.GetText()
		if len(text) > 0:
			if '#' == text[0]:
				self.overTextLine.SetText("#")
				self.overTextLine.Show()
			elif '%' == text[0]:
				self.overTextLine.SetText("%")
				self.overTextLine.Show()
			elif '!' == text[0]:
				self.overTextLine.SetText("!")
				self.overTextLine.Show()

	def OnIMEKeyDown(self, key):
		# LAST_SENTENCE_STACK
		if app.VK_UP == key:
			self.__PrevLastSentenceStack()
			return True

		if app.VK_DOWN == key:
			self.__NextLastSentenceStack()
			return True
		# END_OF_LAST_SENTENCE_STACK

		ui.EditLine.OnIMEKeyDown(self, key)

	# LAST_SENTENCE_STACK
	def __PrevLastSentenceStack(self):
		global ENABLE_LAST_SENTENCE_STACK
		if not ENABLE_LAST_SENTENCE_STACK:
			return

		if self.lastSentenceStack and self.lastSentencePos < len(self.lastSentenceStack):
			self.lastSentencePos += 1
			lastSentence = self.lastSentenceStack[-self.lastSentencePos]
			self.SetText(lastSentence)
			self.SetEndPosition()

	def __NextLastSentenceStack(self):
		global ENABLE_LAST_SENTENCE_STACK
		if not ENABLE_LAST_SENTENCE_STACK:
			return

		if self.lastSentenceStack and self.lastSentencePos > 1:
			self.lastSentencePos -= 1
			lastSentence = self.lastSentenceStack[-self.lastSentencePos]
			self.SetText(lastSentence)
			self.SetEndPosition()

	def __PushLastSentenceStack(self, text):
		global ENABLE_LAST_SENTENCE_STACK
		if not ENABLE_LAST_SENTENCE_STACK:
			return

		if len(text) <= 0:
			return

		LAST_SENTENCE_STACK_SIZE = 32
		if len(self.lastSentenceStack) > LAST_SENTENCE_STACK_SIZE:
			self.lastSentenceStack.pop(0)

		self.lastSentenceStack.append(text)
	# END_OF_LAST_SENTENCE_STACK

	def OnIMEReturn(self):
		text = self.GetText()
		textLen=len(text)

		# LAST_SENTENCE_STACK
		self.__PushLastSentenceStack(text)
		# END_OF_LAST_SENTENCE_STACK

		textSpaceCount=text.count(' ')

		if (textLen > 0) and (textLen != textSpaceCount):
			if '#' == text[0]:
				self.__SendPartyChatPacket(text)
			elif '%' == text[0]:
				self.__SendGuildChatPacket(text)
			elif '!' == text[0]:
				self.__SendShoutChatPacket(text)
			else:
				self.__SendTalkingChatPacket(text)
		else:
			self.__ClearChat()
			self.eventReturn()

		return True

	def OnPressEscapeKey(self):
		self.__ClearChat()
		self.eventEscape()
		return True

	def RunCloseEvent(self):
		self.eventEscape()

	def BindInterface(self, interface):
		self.interface = interface

	def OnMouseLeftButtonDown(self):
		dbg.TraceError("def OnMouseLeftButtonDown(self): line 384 called")
		def _log(msg):
			try:
				import chat
				chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.OFFLINESHOP_CANT_SELECT_ITEM_DURING_BUILING)
				chat.AppendChat(chat.CHAT_TYPE_INFO, "[ShopLinkDBG] %s" % msg)
			except:
				pass
			try:
				dbg.TraceError("[ShopLinkDBG] %s" % msg)
			except:
				pass
	
		def _parse_num(s):
			s = (s or "").strip()
			if not s:
				return None
	
			# HEX csak akkor, ha 0x... vagy van benne A-F betû
			if s.startswith("0x") or any(c in "abcdefABCDEF" for c in s):
				try:
					return int(s.replace("0x", ""), 16)
				except:
					return None
	
			# kulonben DEC
			try:
				return int(s, 10)
			except:
				return None
	
		hyperlink = ui.GetHyperlink()
		dbg.TraceError("hyperlink=%r" % (hyperlink,))
	
		if not hyperlink:
			return False
	
		# ALT+click -> paste
		if app.IsPressed(app.DIK_LALT):
			try:
				link = chat.GetLinkFromHyperlink(hyperlink)
				ime.PasteString(link)
				dbg.TraceError("ALT paste ok: %r" % link)
			except Exception as e:
				dbg.TraceError("ALT paste EXC: %s" % e)
			return True
	
		# OFFLINESHOP shoplink (item:vnum:flag:socket0:socket1:socket2:transmute)
		if isinstance(hyperlink, str) and hyperlink.startswith("item:"):
			try:
				parts = hyperlink.split(":")
				dbg.TraceError("parts(%d)=%r" % (len(parts), parts))
	
				# socket0=owner_id, socket1=sentinel
				if len(parts) >= 6:
					owner_id = _parse_num(parts[3])   # socket0
					sentinel = _parse_num(parts[4])   # socket1
					dbg.TraceError("parsed owner_id=%r sentinel=%r" % (owner_id, sentinel))
	
					if sentinel == 0x0BADF00D and owner_id and owner_id > 0:
						dbg.TraceError("SENTINEL OK -> SendOpenShop(%d)" % owner_id)
						import offlineshop
						offlineshop.SendOpenShop(owner_id)
						return True
					else:
						dbg.TraceError("NOT shoplink (sentinel mismatch)")
			except Exception as e:
				dbg.TraceError("parse EXC: %s" % e)
	
		# ha nem shoplink, akkor normal tooltip
		dbg.TraceError("fallback tooltip")
		try:
			self.interface.MakeHyperlinkTooltip(hyperlink)
		except Exception as e:
			dbg.TraceError("tooltip EXC: %s" % e)
		return True

class ChatInputSet(ui.Window):

	CHAT_OUTLINE_COLOR = grp.GenerateColor(1.0, 1.0, 1.0, 1.0)

	def __init__(self):
		ui.Window.__init__(self)
		self.SetWindowName("ChatInputSet")

		InsertChatInputSetWindow(self)
		self.__Create()

	def __del__(self):
		ui.Window.__del__(self)

	def __Create(self):
		chatModeButton = ChatModeButton()
		chatModeButton.SetParent(self)
		chatModeButton.SetSize(40, 17)
		chatModeButton.SetText(localeinfo.CHAT_NORMAL)
		chatModeButton.SetPosition(7, 2)
		chatModeButton.SAFE_SetEvent(self.OnChangeChatMode)
		self.chatModeButton = chatModeButton

		chatLine = ChatLine()
		chatLine.SetParent(self)
		chatLine.SetMax(512)
		chatLine.SetUserMax(76)
		chatLine.SetText("")
		chatLine.SAFE_SetTabEvent(self.OnChangeChatMode)
		chatLine.x = 0
		chatLine.y = 0
		chatLine.width = 0
		chatLine.height = 0
		self.chatLine = chatLine

		btnSend = ui.Button()
		btnSend.SetParent(self)
		btnSend.SetUpVisual("d:/ymir work/ui/game/taskbar/Send_Chat_Button_01.sub")
		btnSend.SetOverVisual("d:/ymir work/ui/game/taskbar/Send_Chat_Button_02.sub")
		btnSend.SetDownVisual("d:/ymir work/ui/game/taskbar/Send_Chat_Button_03.sub")
		btnSend.SetToolTipText(localeinfo.CHAT_SEND_CHAT)
		btnSend.SAFE_SetEvent(self.chatLine.OnIMEReturn)
		self.btnSend = btnSend

	def Destroy(self):
		self.chatModeButton = None
		self.chatLine = None
		self.btnSend = None

	def Open(self):
		self.chatLine.Show()
		self.chatLine.SetPosition(57, 5)
		self.chatLine.SetFocus()
		self.chatLine.OpenChat()

		self.chatModeButton.SetPosition(7, 2)
		self.chatModeButton.Show()

		self.btnSend.Show()
		self.Show()

		self.RefreshPosition()
		return True

	def Close(self):
		self.chatLine.KillFocus()
		self.chatLine.Hide()
		self.chatModeButton.Hide()
		self.btnSend.Hide()
		self.Hide()
		return True

	def SetEscapeEvent(self, event):
		self.chatLine.SetEscapeEvent(event)

	def SetReturnEvent(self, event):
		self.chatLine.SetReturnEvent(event)

	def OnChangeChatMode(self):
		RefreshChatMode()

	def OnRefreshChatMode(self):
		self.chatLine.ChangeChatMode()
		self.chatModeButton.SetText(self.chatLine.GetCurrentChatModeName())

	def SetChatFocus(self):
		self.chatLine.SetFocus()

	def KillChatFocus(self):
		self.chatLine.KillFocus()

	def SetChatMax(self, max):
		self.chatLine.SetUserMax(max)

	def RefreshPosition(self):
		if localeinfo.IsARABIC():
			self.chatLine.SetSize(self.GetWidth() - 93, 18)
		else:
			self.chatLine.SetSize(self.GetWidth() - 93, 13)

		self.btnSend.SetPosition(self.GetWidth() - 25, 2)

		(self.chatLine.x, self.chatLine.y, self.chatLine.width, self.chatLine.height) = self.chatLine.GetRect()

	def BindInterface(self, interface):
		self.chatLine.BindInterface(interface)

	def OnRender(self):
		(x, y, width, height) = self.chatLine.GetRect()
		ui.RenderRoundBox(x-4, y-3, width+7, height+4, self.CHAT_OUTLINE_COLOR)

if app.ENABLE_NEW_CHAT:
	class ItemOptionWindow(ui.ListBoxEx.Item):
		def __init__(self, name, idx, main):
			ui.ListBoxEx.Item.__init__(self)
			self.SetSize(89, 16)
			self.idx = idx
			self.main = main
			btn = ui.Button()
			btn.SetUpVisual("d:/ymir work/ui/chat/btn_empty_up.tga")
			btn.SetOverVisual("d:/ymir work/ui/chat/btn_empty_over.tga")
			btn.SetDownVisual("d:/ymir work/ui/chat/btn_empty_down.tga")
			btn.SetText(name)
			btn.SetParent(self)
			btn.SetPosition(0, 0)
			btn.Show()
			btn.SetEvent(self.Func)
			self.btn = btn

		def __del__(self):
			ui.ListBoxEx.Item.__del__(self)

		def SetSize(self, width, height):
			ui.ListBoxEx.Item.SetSize(self, width, height)

		def Func(self):
			if self.main:
				self.main.FuncChooseType(self.idx)

	class OptionWindow(ui.ScriptWindow):
		def __init__(self, parent):
			ui.ScriptWindow.__init__(self)
			self.parent = parent
			self.board = None
			self.chatting_type_slot = None
			self.chatting_type_listbox = None
			self.chatting_type_scrollbar = None
			self.cur_chatting_type_text = None
			self.myList = []
			self.origList = []
			self.optionListOff = []
			self.optionListOn = []
			self.selected_filter = -1
			self.save_button = None
			self.reset_button = None
			self.cancle_button = None
			self.tab_name_value = None
			self.tabname_accept_button = None
			self.question_dialog = None
			self.isLoaded = 0

		def __del__(self):
			ui.ScriptWindow.__del__(self)

		def LoadWindow(self):
			if self.isLoaded == 1:
				return
			
			try:
				pyScrLoader = ui.PythonScriptLoader()
				pyScrLoader.LoadScriptFile(self, "uiscript/chatsettingwindow.py")
				
				self.board = self.GetChild("board")
				self.chatting_type_select_button = self.GetChild("chatting_type_select_button")
				self.cur_chatting_type_text = self.GetChild("cur_chatting_type_text")
				self.save_button = self.GetChild("save_button")
				self.reset_button = self.GetChild("reset_button")
				self.cancle_button = self.GetChild("cancle_button")
				self.tab_name_value = self.GetChild("tab_name_value")
				self.tabname_accept_button = self.GetChild("tabname_accept_button")
				# disable gold and exp
				self.GetChild("chatting_setting_exp_bg").Hide()
				self.GetChild("chatting_setting_gold_bg").Hide()
				self.GetChild("chatting_setting_item_bg").SetPosition(18, 376- 60 - 48)
			except:
				import exception
				exception.Abort("OptionWindow.LoadWindow.LoadObject")
			
			try:
				self.optionListOff = []
				self.optionListOn = []
				posY = [
							81,
							81 + 18,
							81 + (18 * 2),
							81 + (18 * 3),
							81 + (18 * 4),
							81 + (18 * 5),
							219,
							268,
							268,
							#268 + 18,
							268 + (18 * 2),
				]
				c = self.parent.GetMaxMode()
				for i in xrange(c):
					img1 = ui.ExpandedImageBox("TOP_MOST")
					img1.SetParent(self.board)
					img1.LoadImage("d:/ymir work/ui/chat/chattingoption_check_box_off.sub")
					img1.SetPosition(145, posY[i])
					img1.SetOnMouseLeftButtonUpEvent(self.ClickCheck, i)
					img1.Hide()
					self.optionListOff.append(img1)
					
					img2 = ui.ExpandedImageBox("TOP_MOST")
					img2.SetParent(self.board)
					img2.LoadImage("d:/ymir work/ui/chat/chattingoption_check_box_on.sub")
					img2.SetPosition(145, posY[i])
					img2.SetOnMouseLeftButtonUpEvent(self.ClickUncheck, i)
					img2.Hide()
					self.optionListOn.append(img2)
				
				slot = ui.Bar3D()
				slot.SetParent(self.board)
				slot.SetSize(89, 80)
				slot.SetPosition(145, 64 + 18)
				slot.Hide()
				self.chatting_type_slot = slot
				scrollbar = ui.ScrollBar()
				scrollbar.SetParent(self.board)
				scrollbar.SetScrollBarSize(80)
				scrollbar.SetPosition(145 + 90, 64 + 18)
				scrollbar.Hide()
				self.chatting_type_scrollbar = scrollbar
				
				self.board.SetCloseEvent(ui.__mem_func__(self.Close))
				self.chatting_type_select_button.SetEvent(ui.__mem_func__(self.TypeFunc))
				self.save_button.SetEvent(ui.__mem_func__(self.TypeSave))
				self.reset_button.SetEvent(ui.__mem_func__(self.TypeReset))
				self.cancle_button.SetEvent(ui.__mem_func__(self.TypeCancel))
				self.tabname_accept_button.SetEvent(ui.__mem_func__(self.OnOpenQuestionDialog), 3)
				self.tab_name_value.SetEscapeEvent(ui.__mem_func__(self.Close))
			except:
				import exception
				exception.Abort("OptionWindow.LoadWindow.BindObject")
			
			self.isLoaded = 1
			self.Hide()

		def Destroy(self):
			self.ClearDictionary()
			self.board = None
			self.chatting_type_listbox = None
			self.chatting_type_slot = None
			self.chatting_type_scrollbar = None
			del self.myList
			self.myList = None
			del self.origList
			self.origList = None
			del self.optionListOff
			self.optionListOff = None
			del self.optionListOn
			self.optionListOn = None
			self.cur_chatting_type_text = None
			self.save_button = None
			self.reset_button = None
			self.cancle_button = None
			self.tab_name_value = None
			self.tabname_accept_button = None
			self.question_dialog = None

		def SelectFilter(self, idx):
			self.selected_filter = idx
			c = self.parent.GetMaxMode()
			for i in xrange(c):
				if len(self.myList[self.selected_filter]) > 0:
					if self.myList[self.selected_filter][i + 1] == "1":
						if i == 7 or i == 9:
							self.optionListOff[i].Hide()
							self.optionListOn[i].Hide()
						else:
							self.optionListOff[i].Hide()
							self.optionListOn[i].Show()
					else:
						if i == 7 or i == 9:
							self.optionListOff[i].Hide()
							self.optionListOn[i].Hide()
						else:
							self.optionListOff[i].Show()
							self.optionListOn[i].Hide()
			
			try:
				self.cur_chatting_type_text.SetText(self.myList[self.selected_filter][0])
			except:
				return

		def Open(self, my, idx):
			self.LoadWindow()
			if self.IsShow():
				self.Close()
			else:
				self.origList = []
				self.myList = []
				
				i = 0
				for item in my:
					self.origList.append(my[i][:])
					self.myList.append(my[i][:])
					i += 1
				
				self.chatting_type_slot.Hide()
				self.chatting_type_scrollbar.Hide()
				self.SelectFilter(idx)
				self.Show()
				self.SetTop()

		def Close(self):
			self.selected_filter = -1
			self.Hide()

		def OnPressEscapeKey(self):
			self.Close()
			return True

		def Refresh(self, my, idx):
			self.origList = []
			self.myList = []
			
			i = 0
			for item in my:
				self.origList.append(my[i][:])
				self.myList.append(my[i][:])
				i += 1
			
			self.chatting_type_slot.Hide()
			self.chatting_type_scrollbar.Hide()
			self.SelectFilter(idx)

		def TypeFunc(self):
			if self.chatting_type_listbox:
				del self.chatting_type_listbox
				self.chatting_type_listbox = None
			
			if self.chatting_type_slot.IsShow():
				self.chatting_type_slot.Hide()
				self.chatting_type_scrollbar.Hide()
			else:
				listBox = ui.ListBoxEx()
				listBox.SetParent(self.chatting_type_slot)
				listBox.SetPosition(0, 0)
				listBox.SetSize(89, 80)
				listBox.SetItemStep(16)
				listBox.SetViewItemCount(5)
				listBox.SetScrollBar(self.chatting_type_scrollbar)
				self.chatting_type_listbox = listBox
				
				c = 0
				j = 0
				for item in self.myList:
					if len(item) > 0:
						self.chatting_type_listbox.AppendItem(ItemOptionWindow(item[0], c, self))
						j += 1
					c += 1
				
				if j > 5:
					self.chatting_type_slot.SetSize(89, 80)
					self.chatting_type_listbox.SetSize(89, 80)
					size = float(5) / float(j)
					self.chatting_type_scrollbar.SetMiddleBarSize(size)
					self.chatting_type_scrollbar.Show()
				else:
					self.chatting_type_slot.SetSize(89, 16 * j)
					self.chatting_type_listbox.SetSize(89, 16 * j)
				
				self.chatting_type_listbox.Show()
				self.chatting_type_slot.Show()

		def FuncChooseType(self, idx):
			self.TypeFunc()
			if self.selected_filter == idx:
				return
			
			self.SelectFilter(idx)

		def ClickCheck(self, idx):
			if len(self.myList[self.selected_filter]) > 0:
				self.myList[self.selected_filter][idx + 1] = "1"
				self.SelectFilter(self.selected_filter)

		def ClickUncheck(self, idx):
			if len(self.myList[self.selected_filter]) > 0:
				self.myList[self.selected_filter][idx + 1] = "0"
				self.SelectFilter(self.selected_filter)

		def TypeSave(self):
			self.parent.SaveOption(self.myList)
			self.Close()

		def TypeReset(self):
			self.myList = []
			i = 0
			for item in self.origList:
				self.myList.append(self.origList[i][:])
				i += 1
			
			self.SelectFilter(self.selected_filter)

		def TypeCancel(self):
			i = 0
			c = self.parent.GetMaxMode()
			for i in xrange(c + 1):
				if len(self.myList[self.selected_filter]) > 0:
					if i == 0:
						txt = str(self.selected_filter)
						if txt == "0":
							self.myList[self.selected_filter][i] = uiscriptlocale.CHATTING_SETTING_DEFAULT_TITLE
						else:
							self.myList[self.selected_filter][i] = txt
					else:
						self.myList[self.selected_filter][i] = "1"
				
				i += 1
			
			self.origList = []
			i = 0
			for item in self.myList:
				self.origList.append(self.myList[i][:])
				i += 1
			
			self.parent.SaveOption(self.myList)
			self.SelectFilter(self.selected_filter)

		def ChangeName(self):
			self.OnCloseQuestionDialog()
			if self.selected_filter == 0:
				chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.CHATTING_SETTING_CHANGE_TITLE_NOT)
				return
			
			text = self.tab_name_value.GetText()
			if text.isspace() == True:  
				chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.CHATTING_SETTING_CHANGE_TITLE_NOT2)
				return
			
			if len(text) < 1:
				chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.CHATTING_SETTING_CHANGE_TITLE_NOT4)
				return
			
			if len(text) > 6:
				chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.CHATTING_SETTING_CHANGE_TITLE_NOT3)
				return
			
			if len(self.myList[self.selected_filter]) > 0:
				self.myList[self.selected_filter][0] = text
			
			self.parent.SaveOption(self.myList)
			if self.chatting_type_slot.IsShow():
				self.TypeFunc()
			
			try:
				self.cur_chatting_type_text.SetText(text)
				self.tab_name_value.SetText("")
			except:
				return

		def OnCloseQuestionDialog(self):
			if not self.question_dialog:
				return
			
			self.question_dialog.Close()
			self.question_dialog = None

		def OnOpenQuestionDialog(self, arg):
			if self.question_dialog:
				self.question_dialog.Close()
				self.question_dialog = None
			
			self.question_dialog = uicommon.QuestionDialog()
			self.question_dialog.SetText(localeinfo.CHATTING_SETTING_CHANGE_TITLE_NAME)
			if arg == 3:
				self.question_dialog.SetAcceptEvent(ui.__mem_func__(self.ChangeName))
			else:
				self.question_dialog.SetAcceptEvent(ui.__mem_func__(self.SetCancelEvent))
			self.question_dialog.SetCancelEvent(ui.__mem_func__(self.OnCloseQuestionDialog))
			self.question_dialog.Open()

## ChatWindow
class ChatWindow(ui.Window):
	if app.ENABLE_NEW_CHAT:
		MAX_FILTERS = 10
		CHAT_WINDOW_WIDTH = 596
	else:
		CHAT_WINDOW_WIDTH = 600
	BOARD_START_COLOR = grp.GenerateColor(0.0, 0.0, 0.0, 0.0)
	BOARD_END_COLOR = grp.GenerateColor(0.0, 0.0, 0.0, 0.8)
	BOARD_MIDDLE_COLOR = grp.GenerateColor(0.0, 0.0, 0.0, 0.5)
	CHAT_OUTLINE_COLOR = grp.GenerateColor(1.0, 1.0, 1.0, 1.0)

	EDIT_LINE_HEIGHT = 25

	class ChatBackBoard(ui.Window):
		def __init__(self):
			ui.Window.__init__(self)
		def __del__(self):
			ui.Window.__del__(self)

	class ChatButton(ui.DragButton):

		def __init__(self):
			ui.DragButton.__init__(self)
			self.AddFlag("float")
			self.AddFlag("movable")
			self.AddFlag("restrict_x")
			self.topFlag = False
			self.SetWindowName("ChatWindow:ChatButton")


		def __del__(self):
			ui.DragButton.__del__(self)

		def SetOwner(self, owner):
			self.owner = owner

		def OnMouseOverIn(self):
			app.SetCursor(app.VSIZE)

		def OnMouseOverOut(self):
			app.SetCursor(app.NORMAL)

		def OnTop(self):
			if True == self.topFlag:
				return

			self.topFlag = True
			self.owner.SetTop()
			self.topFlag = False

	def __init__(self, interface):
		ui.Window.__init__(self)
		self.AddFlag("float")

		self.SetWindowName("ChatWindow")
		self.__RegisterChatColorDict()

		self.boardState = chat.BOARD_STATE_VIEW
		if app.ENABLE_NEW_CHAT:
			self.interface = interface
			self.CHAT_MODE_INDEX = [
									chat.CHAT_TYPE_TALKING,
									chat.CHAT_TYPE_PARTY,
									chat.CHAT_TYPE_GUILD,
									chat.CHAT_TYPE_SHOUT,
									chat.CHAT_TYPE_INFO,
									chat.CHAT_TYPE_NOTICE,
			]
			if app.ENABLE_DICE_SYSTEM:
				self.CHAT_MODE_INDEX.append(chat.CHAT_TYPE_DICE_INFO)
			
			self.CHAT_MODE_INDEX.append(chat.CHAT_TYPE_INFO_EXP)
			self.CHAT_MODE_INDEX.append(chat.CHAT_TYPE_INFO_ITEM)
			self.CHAT_MODE_INDEX.append(chat.CHAT_TYPE_INFO_VALUE)
			
			self.selected_filter = -1
			self.listChat = []
			self.wndOption = OptionWindow(self)
			for j in xrange(self.MAX_FILTERS):
				chatID = chat.CreateChatSet(j + chat.CHAT_NEW_WINDOW_START_IDX)
				self.listChat.append(chatID)
			#chat.SetBoardState(self.listChat[0], chat.BOARD_STATE_VIEW)
		else:
			self.m_chatID = chat.CreateChatSet(chat.CHAT_SET_CHAT_WINDOW)
			chat.SetBoardState(self.m_chatID, chat.BOARD_STATE_VIEW)

		self.xBar = 0
		self.yBar = 0
		self.widthBar = 0
		self.heightBar = 0
		self.curHeightBar = 0
		self.visibleLineCount = 0
		self.scrollBarPos = 1.0
		self.scrollLock = False

		chatInputSet = ChatInputSet()
		chatInputSet.SetParent(self)
		chatInputSet.SetEscapeEvent(ui.__mem_func__(self.CloseChat))
		chatInputSet.SetReturnEvent(ui.__mem_func__(self.CloseChat))
		chatInputSet.SetSize(550, 25)
		self.chatInputSet = chatInputSet

		btnSendWhisper = ui.Button()
		btnSendWhisper.SetParent(self)
		btnSendWhisper.SetUpVisual("d:/ymir work/ui/game/taskbar/Send_Whisper_Button_01.sub")
		btnSendWhisper.SetOverVisual("d:/ymir work/ui/game/taskbar/Send_Whisper_Button_02.sub")
		btnSendWhisper.SetDownVisual("d:/ymir work/ui/game/taskbar/Send_Whisper_Button_03.sub")
		btnSendWhisper.SetToolTipText(localeinfo.CHAT_SEND_MEMO)
		btnSendWhisper.Hide()
		self.btnSendWhisper = btnSendWhisper



		if app.ENABLE_MULTI_LANGUAGE:
			btnChatFilter = ui.Button()
			btnChatFilter.SetParent(self)
			btnChatFilter.SetUpVisual("d:/ymir work/ui/game/taskbar/ignore_button_01.sub")
			btnChatFilter.SetOverVisual("d:/ymir work/ui/game/taskbar/ignore_button_02.sub")
			btnChatFilter.SetDownVisual("d:/ymir work/ui/game/taskbar/ignore_button_03.sub")
			btnChatFilter.SetToolTipText("Filtri")
			btnChatFilter.SetEvent(ui.__mem_func__(self.ToggleChatFilter))
			btnChatFilter.Hide()
			self.btnChatFilter = btnChatFilter
			
			chatFilterBoard = ui.BorderA()
			chatFilterBoard.SetSize(80, 4 + (23 * app.LANGUAGE_MAX_NUM))
			chatFilterBoard.Hide()
			self.chatFilterBoard = chatFilterBoard
			
			self.languageButton = { }
			filterValues = systemSetting.GetChatFilterValue()

			for i in xrange(app.LANGUAGE_MAX_NUM):
				imgBack = ui.ExpandedImageBox("TOP_MOST")
				imgBack.SetParent(self.chatFilterBoard)
				imgBack.LoadImage("d:/ymir work/ui/public/Parameter_Slot_07.sub")
				imgBack.SetPosition(3, 4 + (23 * i))
				imgBack.SetEvent(ui.__mem_func__(self.ClickLanguageButton), "MOUSE_LEFT_BUTTON_UP", i)
				imgBack.SetTop()
				imgBack.Show()
				
				imgCheck = ui.ExpandedImageBox()
				imgCheck.SetParent(self.chatFilterBoard)
				imgCheck.AddFlag("not_pick")
				imgCheck.LoadImage("d:/ymir work/ui/public/check_image.sub")
				imgCheck.SetPosition(8, 4 + (23 * i))
				
				if constinfo.TRANSFORM_LANG(i + 1) in filterValues:
					imgCheck.Hide()
				else:
					imgCheck.Show()
				
				imgFlag = ui.ExpandedImageBox()
				imgFlag.SetParent(self.chatFilterBoard)
				imgFlag.AddFlag("not_pick")
				imgFlag.LoadImage("d:/ymir work/ui/flags/chat/flag_%s.tga" % constinfo.TRANSFORM_LANG(i + 1))
	
				imgFlag.SetScale(0.95, 0.95)
				imgFlag.SetPosition(45, 4 + (23 * i))
				imgFlag.Show()
				
				self.languageButton[i] = [ imgBack, imgCheck, imgFlag ]

		btnChatLog = ui.Button()
		btnChatLog.SetParent(self)
		btnChatLog.SetUpVisual("d:/ymir work/ui/game/taskbar/Open_Chat_Log_Button_01.sub")
		btnChatLog.SetOverVisual("d:/ymir work/ui/game/taskbar/Open_Chat_Log_Button_02.sub")
		btnChatLog.SetDownVisual("d:/ymir work/ui/game/taskbar/Open_Chat_Log_Button_03.sub")
		btnChatLog.SetToolTipText(localeinfo.CHAT_LOG)
		btnChatLog.Hide()
		self.btnChatLog = btnChatLog

		btnChatSizing = self.ChatButton()
		btnChatSizing.SetOwner(self)
		btnChatSizing.SetMoveEvent(ui.__mem_func__(self.Refresh))
		btnChatSizing.Hide()
		self.btnChatSizing = btnChatSizing
		if app.ENABLE_NEW_CHAT:
			imgChatBarLeft = ui.ImageBox()
			imgChatBarLeft.SetParent(self.btnChatSizing)
			imgChatBarLeft.AddFlag("not_pick")
			imgChatBarLeft.LoadImage("d:/ymir work/ui/chat/chat_linebar_left.tga")
			imgChatBarLeft.Show()
			self.imgChatBarLeft = imgChatBarLeft
			imgChatBarRight = ui.ImageBox()
			imgChatBarRight.SetParent(self.btnChatSizing)
			imgChatBarRight.AddFlag("not_pick")
			imgChatBarRight.LoadImage("d:/ymir work/ui/chat/chat_linebar_right.tga")
			imgChatBarRight.Show()
			self.imgChatBarRight = imgChatBarRight
			addtabBtn = ui.Button()
			addtabBtn.SetParent(self.imgChatBarRight)
			addtabBtn.SetUpVisual("d:/ymir work/ui/chat/btn_addtab01_default.tga")
			addtabBtn.SetDownVisual("d:/ymir work/ui/chat/btn_addtab01_down.tga")
			addtabBtn.SetOverVisual("d:/ymir work/ui/chat/btn_addtab01_over.tga")
			addtabBtn.SetPosition(8, 3)
			addtabBtn.SetToolTipText(localeinfo.CHATTING_SETTING_ADD)
			addtabBtn.SetEvent(self.AddTabOption)
			addtabBtn.Show()
			self.addtabBtn = addtabBtn
			optionBtn = ui.Button()
			optionBtn.SetParent(self.imgChatBarRight)
			optionBtn.SetUpVisual("d:/ymir work/ui/chat/btn_option01_default.tga")
			optionBtn.SetDownVisual("d:/ymir work/ui/chat/btn_option01_down.tga")
			optionBtn.SetOverVisual("d:/ymir work/ui/chat/btn_option01_over.tga")
			optionBtn.SetPosition(30, 3)
			optionBtn.SetToolTipText(localeinfo.CHATTING_SETTING_SETTING)
			optionBtn.SetEvent(self.OpenOption)
			optionBtn.Show()
			self.optionBtn = optionBtn
			
			self.tabLine = []
			self.tabBtn = []
			for i in xrange(self.MAX_FILTERS):
				tabLine = ui.ImageBox()
				tabLine.SetParent(self.btnChatSizing)
				tabLine.AddFlag("not_pick")
				tabLine.LoadImage("d:/ymir work/ui/chat/chatmenutab_line.tga")
				tabLine.SetPosition(1 + (i * 54), 0)
				tabLine.Hide()
				self.tabLine.append(tabLine)
				
				tabBtn = ui.RadioButton()
				tabBtn.SetParent(self.btnChatSizing)
				tabBtn.SetUpVisual("d:/ymir work/ui/chat/chatmenutab_default.tga")
				tabBtn.SetDownVisual("d:/ymir work/ui/chat/chatmenutab_down.tga")
				tabBtn.SetOverVisual("d:/ymir work/ui/chat/chatmenutab_drag.tga")
				tabBtn.SetPosition(3 + (i * 54), 0)
				tabBtn.Hide()
				tabBtn.SetEventNew(ui.__mem_func__(self.RemoveTabOption), "mouse_right_up", i)
				self.tabBtn.append(tabBtn)
		else:
			imgChatBarLeft = ui.ImageBox()
			imgChatBarLeft.SetParent(self.btnChatSizing)
			imgChatBarLeft.AddFlag("not_pick")
			imgChatBarLeft.LoadImage("d:/ymir work/ui/pattern/chat_bar_left.tga")
			imgChatBarLeft.Show()
			self.imgChatBarLeft = imgChatBarLeft
			imgChatBarRight = ui.ImageBox()
			imgChatBarRight.SetParent(self.btnChatSizing)
			imgChatBarRight.AddFlag("not_pick")
			imgChatBarRight.LoadImage("d:/ymir work/ui/pattern/chat_bar_right.tga")
			imgChatBarRight.Show()
			self.imgChatBarRight = imgChatBarRight
			imgChatBarMiddle = ui.ExpandedImageBox()
			imgChatBarMiddle.SetParent(self.btnChatSizing)
			imgChatBarMiddle.AddFlag("not_pick")
			imgChatBarMiddle.LoadImage("d:/ymir work/ui/pattern/chat_bar_middle.tga")
			imgChatBarMiddle.Show()
			self.imgChatBarMiddle = imgChatBarMiddle
		
		scrollBar = ui.ScrollBar()
		scrollBar.AddFlag("float")
		scrollBar.SetScrollEvent(ui.__mem_func__(self.OnScroll))
		self.scrollBar = scrollBar
		if app.ENABLE_NEW_CHAT:
			self.LoadOption()
		
		self.Refresh()
		self.chatInputSet.RefreshPosition() # RTL �� ������ ������ �������� ���� ������ ��������

	def __del__(self):
		ui.Window.__del__(self)

	def __RegisterChatColorDict(self):
		CHAT_COLOR_DICT = {
			chat.CHAT_TYPE_TALKING : colorinfo.CHAT_RGB_TALK,
			chat.CHAT_TYPE_INFO : colorinfo.CHAT_RGB_INFO,
			chat.CHAT_TYPE_NOTICE : colorinfo.CHAT_RGB_NOTICE,
			chat.CHAT_TYPE_PARTY : colorinfo.CHAT_RGB_PARTY,
			chat.CHAT_TYPE_GUILD : colorinfo.CHAT_RGB_GUILD,
			chat.CHAT_TYPE_COMMAND : colorinfo.CHAT_RGB_COMMAND,
			chat.CHAT_TYPE_SHOUT : colorinfo.CHAT_RGB_SHOUT,
			chat.CHAT_TYPE_WHISPER : colorinfo.CHAT_RGB_WHISPER,
		}
		if app.ENABLE_DICE_SYSTEM:
			CHAT_COLOR_DICT.update({chat.CHAT_TYPE_DICE_INFO : colorinfo.CHAT_RGB_DICE_INFO,})
		if app.ENABLE_NEW_CHAT:
			CHAT_COLOR_DICT.update({chat.CHAT_TYPE_INFO_EXP : colorinfo.CHAT_RGB_INFO_EXP,})
			CHAT_COLOR_DICT.update({chat.CHAT_TYPE_INFO_ITEM : colorinfo.CHAT_RGB_INFO_ITEM,})
			CHAT_COLOR_DICT.update({chat.CHAT_TYPE_INFO_VALUE : colorinfo.CHAT_RGB_INFO_VALUE,})
		
		for colorItem in CHAT_COLOR_DICT.items():
			type=colorItem[0]
			rgb=colorItem[1]
			chat.SetChatColor(type, rgb[0], rgb[1], rgb[2])

	def Destroy(self):
		self.chatInputSet.Destroy()
		self.chatInputSet = None
		
		self.btnSendWhisper = 0
		self.btnChatLog = 0
		self.btnChatSizing = 0
		if app.ENABLE_NEW_CHAT:
			if self.wndOption:
				self.wndOption.Destroy()
				self.wndOption = None
				del self.wndOption
			self.addtabBtn = None
			del self.addtabBtn
			self.optionBtn = None
			del self.optionBtn
			self.interface = None
			del self.interface
			self.tabLine = []
			self.tabBtn = []
			self.scrollBar = None
		
		if app.ENABLE_MULTI_LANGUAGE:
			self.btnChatFilter = 0
			self.chatFilterBoard = 0
			self.languageButton = { }

	def chatID(self):
		if app.ENABLE_NEW_CHAT:
			if self.selected_filter == -1:
				return self.listChat[0]
			else:
				return self.listChat[self.selected_filter]
		else:
			return self.m_chatID

	if app.ENABLE_NEW_CHAT:
		def SaveOption(self, my):
			self.optionDict = []
			i = 0
			for item in my:
				self.optionDict.append(my[i][:])
				i += 1
			
			fileName = "UserData/chatting/" + player.GetName()
			file = open(fileName, "w")
			for i in xrange(self.MAX_FILTERS):
				if (len(self.optionDict[i]) > 0):
					file.write(self.optionDict[i][0] + " " + self.optionDict[i][1] + " " + self.optionDict[i][2] + " " + self.optionDict[i][3] + " " + self.optionDict[i][4] + " " + self.optionDict[i][5] + " " + self.optionDict[i][6] + " " + self.optionDict[i][7] + " " + self.optionDict[i][8] + " " + self.optionDict[i][9] + " " + self.optionDict[i][10] + "\n")
				else:
					file.write("\n")
			file.close()
			
			if len(self.optionDict[self.selected_filter]) > 0:
				i = 0
				c = len(self.CHAT_MODE_INDEX)
				for i in xrange(c):
					if self.optionDict[self.selected_filter][i + 1] == "1":
						chat.EnableChatMode(self.chatID(), self.CHAT_MODE_INDEX[i])
					else:
						chat.DisableChatMode(self.chatID(), self.CHAT_MODE_INDEX[i])
			
			for i in xrange(self.MAX_FILTERS):
				if (len(self.optionDict[i]) > 0):
					for item in self.tabBtn:
						if i == item.GetIndex():
							item.SetText(self.optionDict[i][0])
							break
			
			self.Refresh()

		def GetMaxMode(self):
			return len(self.CHAT_MODE_INDEX)

		def OpenOption(self):
			if not self.wndOption:
				return
			
			self.wndOption.Open(self.optionDict, self.selected_filter)

		def RemoveTabOption(self, arg1, arg2):
			if arg2 == 0:
				return
			
			idx = self.tabBtn[arg2].GetIndex()
			
			fileName = "UserData/chatting/" + player.GetName()
			file = open(fileName, "w")
			for i in xrange(self.MAX_FILTERS):
				if (len(self.optionDict[i]) > 0):
					if i == idx: 
						file.write("\n")
					else:
						file.write(self.optionDict[i][0] + " " + self.optionDict[i][1] + " " + self.optionDict[i][2] + " " + self.optionDict[i][3] + " " + self.optionDict[i][4] + " " + self.optionDict[i][5] + " " + self.optionDict[i][6] + " " + self.optionDict[i][7] + " " + self.optionDict[i][8] + " " + self.optionDict[i][9] + " " + self.optionDict[i][10] + " " + str(self.selected_filter) + "\n")
				else:
					file.write("\n")
			file.close()
			
			if self.selected_filter == idx:
				self.selected_filter = 0
			self.RefreshOption(False, True)

		def AddTabOption(self):
			for i in xrange(self.MAX_FILTERS):
				if (len(self.optionDict[i]) < 1):
					self.optionDict[i].append(str(i))
					break
				
				if i == int(self.MAX_FILTERS - 1):
					chat.AppendChat(chat.CHAT_TYPE_INFO, localeinfo.CHATTING_SETTING_ADD_MAX)
					return
			
			fileName = "UserData/chatting/" + player.GetName()
			file = open(fileName, "w")
			for i in xrange(self.MAX_FILTERS):
				if (len(self.optionDict[i]) > 0):
					try:
						file.write(self.optionDict[i][0] + " " + self.optionDict[i][1] + " " + self.optionDict[i][2] + " " + self.optionDict[i][3] + " " + self.optionDict[i][4] + " " + self.optionDict[i][5] + " " + self.optionDict[i][6] + " " + self.optionDict[i][7] + " " + self.optionDict[i][8] + " " + self.optionDict[i][9] + " " + self.optionDict[i][10] + " " + str(self.selected_filter) + "\n")
					except:
						file.write(self.optionDict[i][0] + " 1 1 1 1 1 1 1 0 1 1 " + str(self.selected_filter) + "\n")
				else:
					file.write("\n")
			file.close()
			self.RefreshOption(False, True)

		def RefreshButton(self, arg, go = False, state = chat.BOARD_STATE_EDIT):
			if self.selected_filter == arg and go == False:
				return
			
			self.selected_filter = arg
			
			for i in xrange(self.MAX_FILTERS):
				if self.tabBtn[i].IsShow() and self.tabBtn[i].GetIndex() == self.selected_filter:
					self.tabLine[i].Hide()
					self.tabBtn[i].Down()
				else:
					self.tabLine[i].Show()
					self.tabBtn[i].SetUp()
			
			if (len(self.optionDict[self.selected_filter]) > 0):
				i = 0
				c = len(self.CHAT_MODE_INDEX)
				for i in xrange(c):
					if self.optionDict[self.selected_filter][i + 1] == "1":
						chat.EnableChatMode(self.chatID(), self.CHAT_MODE_INDEX[i])
					else:
						chat.DisableChatMode(self.chatID(), self.CHAT_MODE_INDEX[i])
			
			fileName = "UserData/chatting/" + player.GetName()
			file = open(fileName, "w")
			for i in xrange(self.MAX_FILTERS):
				if (len(self.optionDict[i]) > 0):
					try:
						file.write(self.optionDict[i][0] + " " + self.optionDict[i][1] + " " + self.optionDict[i][2] + " " + self.optionDict[i][3] + " " + self.optionDict[i][4] + " " + self.optionDict[i][5] + " " + self.optionDict[i][6] + " " + self.optionDict[i][7] + " " + self.optionDict[i][8] + " " + self.optionDict[i][9] + " " + self.optionDict[i][10] + " " + str(self.selected_filter) + "\n")
					except:
						file.write(self.optionDict[i][0] + " 1 1 1 1 1 1 1 0 1 1 " + str(self.selected_filter) + "\n")
				else:
					file.write("\n")
			file.close()
			
			chat.SetBoardState(self.chatID(), state)
			self.boardState = state
			self.Refresh()

		def LoadOption(self):
			fileName = "UserData/chatting/" + player.GetName()
			try:
				file = open(fileName)
				lines = file.readlines()
				if len(lines) > 0:
					try:
						myList = lines[0].split()
						c = len(self.CHAT_MODE_INDEX)
						for j in xrange(c):
							if myList[j + 1] == "1":
								chat.EnableChatMode(self.chatID(), self.CHAT_MODE_INDEX[j])
							else:
								chat.DisableChatMode(self.chatID(), self.CHAT_MODE_INDEX[j])
						
						self.selected_filter = int(myList[c + 1])
						file.close()
						self.RefreshOption(False, True, True)
					except:
						file.close()
						return
			except:
				return

		def RefreshOption(self, retry = False, update = False, first = False):
			fileName = "UserData/chatting/" + player.GetName()
			try:
				file = open(fileName)
				lines = file.readlines()
				if len(lines) > 0:
					self.optionDict = []
					for i in xrange(self.MAX_FILTERS):
						try:
							myList = lines[i].split()
							self.optionDict.append(myList)
						except:
							myList = []
							self.optionDict.append(myList)
				file.close()
				
				j = 0
				for item in self.tabLine:
					self.tabBtn[j].Hide()
					item.Show()
					j += 1
				
				for i in xrange(self.MAX_FILTERS):
					if (len(self.optionDict[i]) > 0):
						for item in self.tabBtn:
							if not item.IsShow():
								item.SetIndex(i)
								item.SAFE_SetEvent(self.RefreshButton, item.GetIndex())
								item.SetTextAdjust(self.optionDict[i][0], 2)
								if i == 0:
									item.SetToolTipText(localeinfo.CHATTING_SETTING_LEFT2)
								else:
									item.SetToolTipText(localeinfo.CHATTING_SETTING_LEFT1)
								item.Show()
								break
				
				if self.selected_filter == -1:
					if first:
						self.RefreshButton(0, state = chat.BOARD_STATE_VIEW)
					else:
						self.RefreshButton(0)
				else:
					if first:
						self.RefreshButton(self.selected_filter, True, state = chat.BOARD_STATE_VIEW)
					else:
						self.RefreshButton(self.selected_filter, True)
				
				if update == True:
					if self.wndOption.IsShow():
						self.wndOption.Refresh(self.optionDict, self.selected_filter)
			except:
				if retry == True:
					return
				
				if not os.path.exists("UserData"):
					os.makedirs("UserData")
				
				if not os.path.exists(os.getcwd() + os.sep + "UserData\chatting"):
					os.mkdir(os.getcwd() + os.sep + "UserData\chatting")
				
				file = open(fileName, "w")
				file.write(uiscriptlocale.CHATTING_SETTING_DEFAULT_TITLE + " 1 1 1 1 1 1 1 0 1 1 -1\n")
				for i in xrange(int(self.MAX_FILTERS - 1)):
					file.write("\n")
				file.close()
				self.RefreshOption(True)

	################
	## Open & Close
	def OpenChat(self):
		self.SetSize(self.CHAT_WINDOW_WIDTH, 25)
		if app.ENABLE_NEW_CHAT:
			if self.interface:
				if self.interface.ChatLogStat():
					self.interface.ToggleChatLogWindow()
			
			self.RefreshOption()
		
		chat.SetBoardState(self.chatID(), chat.BOARD_STATE_EDIT)
		self.boardState = chat.BOARD_STATE_EDIT

		(x, y, width, height) = self.GetRect()
		(btnX, btnY) = self.btnChatSizing.GetGlobalPosition()

		if localeinfo.IsARABIC():
			chat.SetPosition(self.chatID(), x + width - 10, y)
		else:
			chat.SetPosition(self.chatID(), x + 10, y)

		chat.SetHeight(self.chatID(), y - btnY - self.EDIT_LINE_HEIGHT + 100)

		if self.IsShow():
			self.btnChatSizing.Show()
	
		self.Refresh()

		self.btnSendWhisper.SetPosition(self.GetWidth() - 50, 2)
		self.btnSendWhisper.Show()
		
		if app.ENABLE_MULTI_LANGUAGE:
			self.chatFilterBoard.SetPosition(x + width, y - self.chatFilterBoard.GetHeight())
			self.btnChatFilter.SetPosition(self.GetWidth() - 75, 2)
			self.btnChatFilter.Show()

		self.btnChatLog.SetPosition(self.GetWidth() - 25, 2)
		self.btnChatLog.Show()

		self.chatInputSet.Open()
		self.chatInputSet.SetTop()
		self.SetTop()
		
	if app.ENABLE_MULTI_LANGUAGE:
		def GetFilterText(self):
			sFilter = ""
			for key, value in self.languageButton.iteritems():
				if not value[1].IsShow():
					langName = constinfo.TRANSFORM_LANG(key + 1)
					if langName != "None":
						sFilter = sFilter + langName + "|"
				
			return sFilter
			
		def ClickLanguageButton(self, event, lang):
			if self.languageButton[lang][1]:
				if self.languageButton[lang][1].IsShow():
					self.languageButton[lang][1].Hide()
				else:
					self.languageButton[lang][1].Show()
					
			systemSetting.SetChatFilterValue(self.GetFilterText())
			
		def ToggleChatFilter(self):
			if self.chatFilterBoard:
				if self.chatFilterBoard.IsShow():
					self.chatFilterBoard.Hide()
				else:
					self.chatFilterBoard.Show()
					self.chatFilterBoard.SetTop()

	def CloseChat(self):
		chat.SetBoardState(self.chatID(), chat.BOARD_STATE_VIEW)
		self.boardState = chat.BOARD_STATE_VIEW

		(x, y, width, height) = self.GetRect()

		if localeinfo.IsARABIC():
			chat.SetPosition(self.chatID(), x + width - 10, y + self.EDIT_LINE_HEIGHT)
		else:
			chat.SetPosition(self.chatID(), x + 10, y + self.EDIT_LINE_HEIGHT)

		self.SetSize(self.CHAT_WINDOW_WIDTH, 0)

		self.chatInputSet.Close()
		self.btnSendWhisper.Hide()
		self.btnChatLog.Hide()
		self.btnChatSizing.Hide()

		if app.ENABLE_MULTI_LANGUAGE:
			self.btnChatFilter.Hide()
			self.chatFilterBoard.Hide()

		self.Refresh()

	def SetSendWhisperEvent(self, event):
		self.btnSendWhisper.SetEvent(event)

	def SetOpenChatLogEvent(self, event):
		self.btnChatLog.SetEvent(event)

	def IsEditMode(self):
		if chat.BOARD_STATE_EDIT == self.boardState:
			return True

		return False

	def __RefreshSizingBar(self):
		(x, y, width, height) = self.GetRect()
		gxChat, gyChat = self.btnChatSizing.GetGlobalPosition()
		self.btnChatSizing.SetPosition(x, gyChat)
		if app.ENABLE_NEW_CHAT:
			self.btnChatSizing.SetSize(width, 26)
			self.imgChatBarLeft.SetPosition(-1, 17)
			self.imgChatBarRight.SetPosition(width - 55, 0)
		else:
			self.btnChatSizing.SetSize(width, 22)
			self.imgChatBarLeft.SetPosition(0, 0)
			self.imgChatBarRight.SetPosition(width - 64, 0)
			self.imgChatBarMiddle.SetPosition(64, 0)
			self.imgChatBarMiddle.SetRenderingRect(0.0, 0.0, float(width - 128) / 64.0 - 1.0, 0.0)

	def SetPosition(self, x, y):
		ui.Window.SetPosition(self, x, y)
		self.__RefreshSizingBar()

	def SetSize(self, width, height):
		ui.Window.SetSize(self, width, height)
		self.__RefreshSizingBar()

	def SetHeight(self, height):
		gxChat, gyChat = self.btnChatSizing.GetGlobalPosition()
		self.btnChatSizing.SetPosition(gxChat, wndMgr.GetScreenHeight() - height)

	###########
	## Refresh
	def Refresh(self):
		if self.boardState == chat.BOARD_STATE_EDIT:
			self.RefreshBoardEditState()
		elif self.boardState == chat.BOARD_STATE_VIEW:
			self.RefreshBoardViewState()

	def RefreshBoardEditState(self):

		(x, y, width, height) = self.GetRect()
		(btnX, btnY) = self.btnChatSizing.GetGlobalPosition()

		self.xBar = x
		self.yBar = btnY
		self.widthBar = width
		self.heightBar = y - btnY + self.EDIT_LINE_HEIGHT
		self.curHeightBar = self.heightBar

		if localeinfo.IsARABIC():
			chat.SetPosition(self.chatID(), x + width - 10, y)
		else:
			chat.SetPosition(self.chatID(), x + 10, y)

		chat.SetHeight(self.chatID(), y - btnY - self.EDIT_LINE_HEIGHT)
		chat.ArrangeShowingChat(self.chatID())

		if btnY > y:
			self.btnChatSizing.SetPosition(btnX, y)
			self.heightBar = self.EDIT_LINE_HEIGHT

		if app.ENABLE_MULTI_LANGUAGE:
			if btnY > y - self.chatFilterBoard.GetHeight() and self.chatFilterBoard.IsShow():
				self.chatFilterBoard.Hide()

	def RefreshBoardViewState(self):
		if not self.btnChatSizing:
			return
		
		(x, y, width, height) = self.GetRect()
		(btnX, btnY) = self.btnChatSizing.GetGlobalPosition()
		
		textAreaHeight = self.visibleLineCount * chat.GetLineStep(self.chatID())

		if localeinfo.IsARABIC():
			chat.SetPosition(self.chatID(), x + width - 10, y + self.EDIT_LINE_HEIGHT)
		else:
			chat.SetPosition(self.chatID(), x + 10, y + self.EDIT_LINE_HEIGHT)

		chat.SetHeight(self.chatID(), y - btnY - self.EDIT_LINE_HEIGHT + 100)

		if self.boardState == chat.BOARD_STATE_EDIT:
			textAreaHeight += 45
		elif self.visibleLineCount != 0:
			textAreaHeight += 10 + 10

		self.xBar = x
		self.yBar = y + self.EDIT_LINE_HEIGHT - textAreaHeight
		self.widthBar = width
		self.heightBar = textAreaHeight

		self.scrollBar.Hide()

	##########
	## Render
	def OnUpdate(self):
		if app.ENABLE_NEW_CHAT:
			if self.interface:
				if self.interface.ChatLogStat():
					return
		
		if self.boardState == chat.BOARD_STATE_EDIT:
			chat.Update(self.chatID())
		elif self.boardState == chat.BOARD_STATE_VIEW:
			if systemSetting.IsViewChat():
				chat.Update(self.chatID())

	def OnRender(self):
		if app.ENABLE_NEW_CHAT:
			if self.interface:
				if self.interface.ChatLogStat():
					return
		
		if chat.GetVisibleLineCount(self.chatID()) != self.visibleLineCount:
			self.visibleLineCount = chat.GetVisibleLineCount(self.chatID())
			self.Refresh()

		if self.curHeightBar != self.heightBar:
			self.curHeightBar += (self.heightBar - self.curHeightBar) / 10

		if self.boardState == chat.BOARD_STATE_EDIT:
			grp.SetColor(self.BOARD_MIDDLE_COLOR)
			if app.ENABLE_NEW_CHAT:
				grp.RenderBar(self.xBar, self.yBar + (self.heightBar - self.curHeightBar) + 10 + 10, self.widthBar, self.curHeightBar)
			else:
				grp.RenderBar(self.xBar, self.yBar + (self.heightBar - self.curHeightBar) + 10, self.widthBar, self.curHeightBar)
			chat.Render(self.chatID())
		elif self.boardState == chat.BOARD_STATE_VIEW:
			if systemSetting.IsViewChat():
				grp.RenderGradationBar(self.xBar, self.yBar + (self.heightBar - self.curHeightBar), self.widthBar, self.curHeightBar, self.BOARD_START_COLOR, self.BOARD_END_COLOR)
				chat.Render(self.chatID())

	##########
	## Event
	def OnTop(self):
		self.btnChatSizing.SetTop()
		self.scrollBar.SetTop()

	def OnScroll(self):
		if not self.scrollLock:
			self.scrollBarPos = self.scrollBar.GetPos()

		lineCount = chat.GetLineCount(self.chatID())
		visibleLineCount = chat.GetVisibleLineCount(self.chatID())
		endLine = visibleLineCount + int(float(lineCount - visibleLineCount) * self.scrollBarPos)

		chat.SetEndPos(self.chatID(), self.scrollBarPos)

	def OnChangeChatMode(self):
		self.chatInputSet.OnChangeChatMode()

	def SetChatFocus(self):
		self.chatInputSet.SetChatFocus()

## ChatLogWindow
class ChatLogWindow(ui.Window):

	BLOCK_WIDTH = 32
	CHAT_MODE_NAME = [ localeinfo.CHAT_NORMAL, localeinfo.CHAT_PARTY, localeinfo.CHAT_GUILD, localeinfo.CHAT_SHOUT, localeinfo.CHAT_INFORMATION, localeinfo.CHAT_NOTICE, ]
	CHAT_MODE_INDEX = [ chat.CHAT_TYPE_TALKING,
						chat.CHAT_TYPE_PARTY,
						chat.CHAT_TYPE_GUILD,
						chat.CHAT_TYPE_SHOUT,
						chat.CHAT_TYPE_INFO,
						chat.CHAT_TYPE_NOTICE, ]

	if app.ENABLE_DICE_SYSTEM:
		CHAT_MODE_NAME.append(localeinfo.CHAT_DICE_INFO)
		CHAT_MODE_INDEX.append(chat.CHAT_TYPE_DICE_INFO)
	if app.ENABLE_NEW_CHAT:
		CHAT_MODE_NAME.append(localeinfo.CHAT_TYPE_INFO_EXP)
		CHAT_MODE_NAME.append(localeinfo.CHAT_TYPE_INFO_ITEM)
		CHAT_MODE_NAME.append(localeinfo.CHAT_TYPE_INFO_VALUE)
		CHAT_MODE_INDEX.append(chat.CHAT_TYPE_INFO_EXP)
		CHAT_MODE_INDEX.append(chat.CHAT_TYPE_INFO_ITEM)
		CHAT_MODE_INDEX.append(chat.CHAT_TYPE_INFO_VALUE)
		CHAT_LOG_WINDOW_MINIMUM_WIDTH = 546
	else:
		CHAT_LOG_WINDOW_MINIMUM_WIDTH = 450
	CHAT_LOG_WINDOW_MINIMUM_HEIGHT = 120

	class ResizeButton(ui.DragButton):

		def __init__(self):
			ui.DragButton.__init__(self)

		def __del__(self):
			ui.DragButton.__del__(self)

		def OnMouseOverIn(self):
			app.SetCursor(app.HVSIZE)

		def OnMouseOverOut(self):
			app.SetCursor(app.NORMAL)

	def __init__(self, interface):

		self.allChatMode = True
		self.chatInputSet = None
		if app.ENABLE_NEW_CHAT:
			self.interface = interface
		ui.Window.__init__(self)
		self.AddFlag("float")
		self.AddFlag("movable")
		self.SetWindowName("ChatLogWindow")
		self.__CreateChatInputSet()
		self.__CreateWindow()
		self.__CreateButton()
		self.__CreateScrollBar()

		self.chatID = chat.CreateChatSet(chat.CHAT_SET_LOG_WINDOW)
		chat.SetBoardState(self.chatID, chat.BOARD_STATE_LOG)
		for i in self.CHAT_MODE_INDEX:
			chat.EnableChatMode(self.chatID, i)

		self.SetPosition(20, 20)
		self.SetSize(self.CHAT_LOG_WINDOW_MINIMUM_WIDTH, self.CHAT_LOG_WINDOW_MINIMUM_HEIGHT)
		self.btnSizing.SetPosition(self.CHAT_LOG_WINDOW_MINIMUM_WIDTH-self.btnSizing.GetWidth(), self.CHAT_LOG_WINDOW_MINIMUM_HEIGHT-self.btnSizing.GetHeight()+2)

		self.OnResize()

	def __CreateChatInputSet(self):
		chatInputSet = ChatInputSet()
		chatInputSet.SetParent(self)
		chatInputSet.SetEscapeEvent(ui.__mem_func__(self.Close))
		chatInputSet.SetWindowVerticalAlignBottom()
		chatInputSet.Open()
		chatInputSet.BindInterface(self.interface)
		self.chatInputSet = chatInputSet

	def __CreateWindow(self):
		imgLeft = ui.ImageBox()
		imgLeft.AddFlag("not_pick")
		imgLeft.SetParent(self)

		imgCenter = ui.ExpandedImageBox()
		imgCenter.AddFlag("not_pick")
		imgCenter.SetParent(self)

		imgRight = ui.ImageBox()
		imgRight.AddFlag("not_pick")
		imgRight.SetParent(self)

		if localeinfo.IsARABIC():
			imgLeft.LoadImage("locale/ae/ui/pattern/titlebar_left.tga")
			imgCenter.LoadImage("locale/ae/ui/pattern/titlebar_center.tga")
			imgRight.LoadImage("locale/ae/ui/pattern/titlebar_right.tga")
		else:
			imgLeft.LoadImage("d:/ymir work/ui/pattern/chatlogwindow_titlebar_left.tga")
			imgCenter.LoadImage("d:/ymir work/ui/pattern/chatlogwindow_titlebar_middle.tga")
			imgRight.LoadImage("d:/ymir work/ui/pattern/chatlogwindow_titlebar_right.tga")

		imgLeft.Show()
		imgCenter.Show()
		imgRight.Show()

		btnClose = ui.Button()
		btnClose.SetParent(self)
		btnClose.SetUpVisual("d:/ymir work/ui/public/close_button_01.sub")
		btnClose.SetOverVisual("d:/ymir work/ui/public/close_button_02.sub")
		btnClose.SetDownVisual("d:/ymir work/ui/public/close_button_03.sub")
		btnClose.SetToolTipText(localeinfo.UI_CLOSE, 0, -23)
		btnClose.SetEvent(ui.__mem_func__(self.Close))
		btnClose.Show()

		btnSizing = self.ResizeButton()
		btnSizing.SetParent(self)
		btnSizing.SetMoveEvent(ui.__mem_func__(self.OnResize))
		btnSizing.SetSize(16, 16)
		btnSizing.Show()

		titleName = ui.TextLine()
		titleName.SetParent(self)

		if localeinfo.IsARABIC():
			titleName.SetPosition(self.GetWidth()-20, 6)
		else:
			titleName.SetPosition(20, 6)

		titleName.SetText(localeinfo.CHAT_LOG_TITLE)
		titleName.Show()

		self.imgLeft = imgLeft
		self.imgCenter = imgCenter
		self.imgRight = imgRight
		self.btnClose = btnClose
		self.btnSizing = btnSizing
		self.titleName = titleName

	def __CreateButton(self):

		if localeinfo.IsARABIC():
			bx = 20
		else:
			bx = 13

		btnAll = ui.RadioButton()
		btnAll.SetParent(self)
		btnAll.SetPosition(bx, 24)
		btnAll.SetUpVisual("d:/ymir work/ui/public/xsmall_button_01.sub")
		btnAll.SetOverVisual("d:/ymir work/ui/public/xsmall_button_02.sub")
		btnAll.SetDownVisual("d:/ymir work/ui/public/xsmall_button_03.sub")
		btnAll.SetText(localeinfo.CHAT_ALL)
		btnAll.SetEvent(ui.__mem_func__(self.ToggleAllChatMode))
		btnAll.Down()
		btnAll.Show()
		self.btnAll = btnAll

		x = bx + 48
		i = 0
		self.modeButtonList = []
		for name in self.CHAT_MODE_NAME:
			btn = ui.ToggleButton()
			btn.SetParent(self)
			btn.SetPosition(x, 24)
			btn.SetUpVisual("d:/ymir work/ui/public/xsmall_button_01.sub")
			btn.SetOverVisual("d:/ymir work/ui/public/xsmall_button_02.sub")
			btn.SetDownVisual("d:/ymir work/ui/public/xsmall_button_03.sub")
			btn.SetText(name)
			btn.Show()

			mode = self.CHAT_MODE_INDEX[i]
			btn.SetToggleUpEvent(lambda arg=mode: self.ToggleChatMode(arg))
			btn.SetToggleDownEvent(lambda arg=mode: self.ToggleChatMode(arg))
			self.modeButtonList.append(btn)

			x += 48
			i += 1

	def __CreateScrollBar(self):
		scrollBar = ui.SmallThinScrollBar()
		scrollBar.SetParent(self)
		scrollBar.Show()
		scrollBar.SetScrollEvent(ui.__mem_func__(self.OnScroll))
		self.scrollBar = scrollBar
		self.scrollBarPos = 1.0

	def __del__(self):
		ui.Window.__del__(self)

	def Destroy(self):
		self.imgLeft = None
		self.imgCenter = None
		self.imgRight = None
		self.btnClose = None
		self.btnSizing = None
		self.modeButtonList = []
		self.scrollBar = None
		self.chatInputSet = None

	def ToggleAllChatMode(self):
		if self.allChatMode:
			return

		self.allChatMode = True

		for i in self.CHAT_MODE_INDEX:
			chat.EnableChatMode(self.chatID, i)
		for btn in self.modeButtonList:
			btn.SetUp()

	def ToggleChatMode(self, mode):
		if self.allChatMode:
			self.allChatMode = False
			for i in self.CHAT_MODE_INDEX:
				chat.DisableChatMode(self.chatID, i)
			chat.EnableChatMode(self.chatID, mode)
			self.btnAll.SetUp()

		else:
			chat.ToggleChatMode(self.chatID, mode)

	def SetSize(self, width, height):
		self.imgCenter.SetRenderingRect(0.0, 0.0, float((width - self.BLOCK_WIDTH*2) - self.BLOCK_WIDTH) / self.BLOCK_WIDTH, 0.0)
		self.imgCenter.SetPosition(self.BLOCK_WIDTH, 0)
		self.imgRight.SetPosition(width - self.BLOCK_WIDTH, 0)

		if localeinfo.IsARABIC():
			self.titleName.SetPosition(self.GetWidth()-20, 3)
			self.btnClose.SetPosition(3, 3)
			self.scrollBar.SetPosition(1, 45)
		else:
			self.btnClose.SetPosition(width - self.btnClose.GetWidth() - 5, 5)
			self.scrollBar.SetPosition(width - 15, 45)

		self.scrollBar.SetScrollBarSize(height - 45 - 12)
		self.scrollBar.SetPos(self.scrollBarPos)
		ui.Window.SetSize(self, width, height)

	def Open(self):
		if app.ENABLE_NEW_CHAT:
			if self.interface:
				if self.interface.ChatChatStat():
					self.interface.ToggleChat()
		
		self.OnResize()
		self.chatInputSet.SetChatFocus()
		self.Show()

	def Close(self):
		if self.chatInputSet:
			self.chatInputSet.KillChatFocus()
		self.Hide()

	def OnResize(self):
		x, y = self.btnSizing.GetLocalPosition()
		width = self.btnSizing.GetWidth()
		height = self.btnSizing.GetHeight()

		if x < self.CHAT_LOG_WINDOW_MINIMUM_WIDTH - width:
			self.btnSizing.SetPosition(self.CHAT_LOG_WINDOW_MINIMUM_WIDTH - width, y)
			return
		if y < self.CHAT_LOG_WINDOW_MINIMUM_HEIGHT - height:
			self.btnSizing.SetPosition(x, self.CHAT_LOG_WINDOW_MINIMUM_HEIGHT - height)
			return

		self.scrollBar.LockScroll()
		self.SetSize(x + width, y + height)
		self.scrollBar.UnlockScroll()

		if localeinfo.IsARABIC():
			self.chatInputSet.SetPosition(20, 25)
		else:
			self.chatInputSet.SetPosition(0, 25)

		self.chatInputSet.SetSize(self.GetWidth() - 20, 20)
		self.chatInputSet.RefreshPosition()
		self.chatInputSet.SetChatMax(self.GetWidth() / 8)

	def OnScroll(self):
		self.scrollBarPos = self.scrollBar.GetPos()

		lineCount = chat.GetLineCount(self.chatID)
		visibleLineCount = chat.GetVisibleLineCount(self.chatID)
		endLine = visibleLineCount + int(float(lineCount - visibleLineCount) * self.scrollBarPos)

		chat.SetEndPos(self.chatID, self.scrollBarPos)

	def OnRender(self):
		(x, y, width, height) = self.GetRect()

		if localeinfo.IsARABIC():
			grp.SetColor(0x77000000)
			grp.RenderBar(x+2, y+45, 13, height-45)

			grp.SetColor(0x77000000)
			grp.RenderBar(x, y, width, height)
			grp.SetColor(0x77000000)
			grp.RenderBox(x, y, width-2, height)
			grp.SetColor(0x77000000)
			grp.RenderBox(x+1, y+1, width-2, height)

			grp.SetColor(0xff989898)
			grp.RenderLine(x+width-13, y+height-1, 11, -11)
			grp.RenderLine(x+width-9, y+height-1, 7, -7)
			grp.RenderLine(x+width-5, y+height-1, 3, -3)
		else:
			grp.SetColor(0x77000000)
			grp.RenderBar(x+width-15, y+45, 13, height-45)

			grp.SetColor(0x77000000)
			grp.RenderBar(x, y, width, height)
			grp.SetColor(0x77000000)
			grp.RenderBox(x, y, width-2, height)
			grp.SetColor(0x77000000)
			grp.RenderBox(x+1, y+1, width-2, height)

			grp.SetColor(0xff989898)
			grp.RenderLine(x+width-13, y+height-1, 11, -11)
			grp.RenderLine(x+width-9, y+height-1, 7, -7)
			grp.RenderLine(x+width-5, y+height-1, 3, -3)

		#####

		chat.ArrangeShowingChat(self.chatID)

		if localeinfo.IsARABIC():
			chat.SetPosition(self.chatID, x + width - 10, y + height - 25)
		else:
			chat.SetPosition(self.chatID, x + 10, y + height - 25)

		chat.SetHeight(self.chatID, height - 45 - 25)
		chat.Update(self.chatID)
		chat.Render(self.chatID)

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def OnMouseLeftButtonDown(self):
		dbg.TraceError("def OnMouseLeftButtonDown(self): line 2083 called")
		def _log(msg):
			try:
				import chat
				chat.AppendChat(chat.CHAT_TYPE_INFO, "[ShopLinkDBG] %s" % msg)
			except:
				pass
			try:
				
				TraceError("[ShopLinkDBG] %s" % msg)
			except:
				pass
	
		def _parse_num(s):
			s = (s or "").strip()
			if not s:
				return None
	
			# HEX csak akkor, ha 0x... vagy van benne A-F betû
			if s.startswith("0x") or any(c in "abcdefABCDEF" for c in s):
				try:
					return int(s.replace("0x", ""), 16)
				except:
					return None
	
			# kulonben DEC
			try:
				return int(s, 10)
			except:
				return None
	
		hyperlink = ui.GetHyperlink()
		dbg.TraceError("hyperlink=%r" % (hyperlink,))
	
		if not hyperlink:
			return False
	
		# ALT+click -> paste
		if app.IsPressed(app.DIK_LALT):
			try:
				link = chat.GetLinkFromHyperlink(hyperlink)
				ime.PasteString(link)
				dbg.TraceError("ALT paste ok: %r" % link)
			except Exception as e:
				dbg.TraceError("ALT paste EXC: %s" % e)
			return True
	
		# OFFLINESHOP shoplink (item:vnum:flag:socket0:socket1:socket2:transmute)
		if isinstance(hyperlink, str) and hyperlink.startswith("item:"):
			try:
				parts = hyperlink.split(":")
				dbg.TraceError("parts(%d)=%r" % (len(parts), parts))
	
				# socket0=owner_id, socket1=sentinel
				if len(parts) >= 6:
					owner_id = _parse_num(parts[3])   # socket0
					sentinel = _parse_num(parts[4])   # socket1
					dbg.TraceError("parsed owner_id=%r sentinel=%r" % (owner_id, sentinel))
	
					if sentinel == 0x0BADF00D and owner_id and owner_id > 0:
						dbg.TraceError("SENTINEL OK -> SendOpenShop(%d)" % owner_id)
						import offlineshop
						offlineshop.SendOpenShop(owner_id)
						return True
					else:
						dbg.TraceError("NOT shoplink (sentinel mismatch)")
			except Exception as e:
				dbg.TraceError("parse EXC: %s" % e)
	
		# ha nem shoplink, akkor normal tooltip
		dbg.TraceError("fallback tooltip")
		try:
			self.interface.MakeHyperlinkTooltip(hyperlink)
		except Exception as e:
			dbg.TraceError("tooltip EXC: %s" % e)
		return True
