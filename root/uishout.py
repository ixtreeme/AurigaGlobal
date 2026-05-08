import ui
import chat
import net
import constinfo

# NOTE:
# This file manages the Auto Shout UI. The actual sending usually happens elsewhere
# (using constinfo.auto_shout_text). To make links clickable for auto shout,
# we store the text in hyperlink format if it starts with http/https.

def _MakeSysWebLinkRedIfStartsWithHttp(text):
	"""If text starts with http:// or https://, wrap it as a sysweb hyperlink (red)."""
	try:
		if not isinstance(text, str):
			text = str(text)
	except:
		return text

	# Already contains a hyperlink code -> don't touch
	if "|H" in text:
		return text

	if text.startswith("https://") or text.startswith("http://"):
		# Take the first token as the link, keep the rest after a space
		sp = text.find(" ")
		if sp == -1:
			link = text
			rest = ""
		else:
			link = text[:sp]
			rest = text[sp:]  # includes the space

		hyper = "|cFFFF0000|h|Hsysweb:" + link.replace("://", "XxX") + "|h" + link + "|h|r"
		return hyper + rest

	return text


class ShoutManager(ui.ScriptWindow):

	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.board = None
		self.baslat = None
		self.durdur = None
		self.temizle = None
		self.bilgi = None
		self.yazi = None
		self.shopadvert = None

		self.LoadWindow()

	def __del__(self):
		try:
			self.Destroy()
		except:
			pass
		ui.ScriptWindow.__del__(self)

	def LoadWindow(self):
		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "uiscript/shout.py")
		except:
			import exception
			exception.Abort("ShoutManager.LoadWindow.LoadScript")

		try:
			GetObject = self.GetChild
			self.board = GetObject("board")
			self.baslat = GetObject("baslat")
			self.durdur = GetObject("durdur")
			self.temizle = GetObject("temizle")
			self.bilgi = GetObject("bilgi")
			self.yazi = GetObject("CommentValue")

			# Optional button in some uiscripts
			try:
				self.shopadvert = GetObject("shopadvert")
			except:
				self.shopadvert = None

			self.baslat.SetEvent(ui.__mem_func__(self.Baslat))
			self.durdur.SetEvent(ui.__mem_func__(self.Durdur))
			self.temizle.SetEvent(ui.__mem_func__(self.Temizle))

			if self.shopadvert:
				self.shopadvert.SetEvent(ui.__mem_func__(self.ShopAdvertise))

			self.board.SetCloseEvent(ui.__mem_func__(self.__OnCloseButtonClick))
		except:
			import exception
			exception.Abort("ShoutManager.LoadWindow.BindObject")

	def Destroy(self):
		self.ClearDictionary()
		self.board = None
		self.baslat = None
		self.durdur = None
		self.temizle = None
		self.bilgi = None
		self.yazi = None
		self.shopadvert = None

	def Open(self):
		if constinfo.auto_shout_status == 1:
			self.bilgi.SetText("Type the message you want to send. (Current Status: Active)")
		else:
			self.bilgi.SetText("Type the message you want to send. (Current Status: Inactive)")

		self.Show()
		self.SetCenterPosition()

	def OnUpdate(self):
		if not self.bilgi:
			return
		if constinfo.auto_shout_status == 1:
			self.bilgi.SetText("Type the message you want to send. (Current Status: Active)")
		else:
			self.bilgi.SetText("Type the message you want to send. (Current Status: Inactive)")

	def ShopAdvertise(self):
		try:
			self.yazi.SetText("/shoplink ")
			self.yazi.SetFocus()
		except:
			pass

	def Temizle(self):
		if self.yazi:
			self.yazi.SetText("")
		# also clear runtime values if you want
		# constinfo.auto_shout_text = ""

	def Baslat(self):
		# Save text for auto shout; convert starting http/https to clickable sysweb link (red)
		try:
			text = self.yazi.GetText()
		except:
			text = constinfo.auto_shout_text

		text = _MakeSysWebLinkRedIfStartsWithHttp(text)

		constinfo.auto_shout_text = text
		constinfo.auto_shout_status = 1

	def Durdur(self):
		constinfo.auto_shout_text = ""
		constinfo.auto_shout_status = 0

	def Close(self):
		self.Hide()

	def __OnCloseButtonClick(self):
		self.Hide()

	def OnPressEscapeKey(self):
		self.Close()
		return True
