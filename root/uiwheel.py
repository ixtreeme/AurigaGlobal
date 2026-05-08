# -*- coding: utf-8 -*-
import ui, item, net, uitooltip, localeinfo

# NOTE:
# - This is a reworked "Wheel" UI into a 4x4 table (slot background per item).
# - Spin animation: yellow highlight runs across slots, final slot stays green.
# - Server packets kept the same:
#	 net.WheelPacket(1) close
#	 net.WheelPacket(2) request spin
#	 net.WheelPacket(3) spin finished (client ACK)
#
# Expected server behaviour:
# - client calls WheelPacket(2)
# - server responds by calling TurnWheel(ItemVnum, Count, SpinLoops, RandomIndex)
#   where RandomIndex is 0..15 (winning slot index)

try:
	import app
except:
	app = None


class WheelOfDestiny(ui.Window):
	SLOT_COUNT = 16
	GRID_COLS = 4
	GRID_ROWS = 4

	# highlight colors (AARRGGBB)
	COLOR_YELLOW = 0x66FFFF00
	COLOR_GREEN = 0x6600FF00

	def __init__(self):
		ui.Window.__init__(self)

		self.tooltipItem = uitooltip.ItemToolTip()
		self.tooltipItem.Hide()

		self.itemfortip = 0
		self.ItemIcon = None
		self.ItemCount = 0

		# animation state
		self._animActive = False
		self._currentIndex = -1
		self._targetIndex = 0
		self._stepsRemaining = 0
		self._nextStepTime = 0.0

		# slot ui
		self.SlotBg = None		  # last reward slot bg (right side)
		self.LastCountText = None

		self.Grid = None
		self.GridSlots = []		 # ExpandedImageBox backgrounds
		self.GridIcons = []		 # ExpandedImageBox icons (child of slot bg)
		self.GridCounts = []		# TextLine (child of icon)
		self.GridHL = []			# Bar highlight overlays (child of slot bg)
		self.GridVnum = [0] * self.SLOT_COUNT
		self.GridItemCount = [0] * self.SLOT_COUNT

		self.BuildWindow()
		self.Hide()
		self.Board.Hide()

	def __del__(self):
		ui.Window.__del__(self)

	def BuildWindow(self):
		self.Board = ui.ExpandedImageBox()
		self.Board.AddFlag("movable")
		self.Board.AddFlag("float")
		self.Board.LoadImage("schicksal/schicksal_board.tga")
		self.Board.SetCenterPosition()
		self.Board.Show()

		# Close
		self.CloseButton = ui.Button()
		self.CloseButton.SetParent(self.Board)
		self.CloseButton.SetEvent(ui.__mem_func__(self.ClosePacket))
		self.CloseButton.SetPosition(615, 10)
		self.CloseButton.SetUpVisual("d:/ymir work/ui/public/close_button_01.sub")
		self.CloseButton.SetOverVisual("d:/ymir work/ui/public/close_button_02.sub")
		self.CloseButton.SetDownVisual("d:/ymir work/ui/public/close_button_03.sub")
		self.CloseButton.Show()

		# RIGHT SIDE: last reward slot (kept)
		self.SlotBg = ui.ExpandedImageBox()
		self.SlotBg.SetParent(self.Board)
		self.SlotBg.SetPosition(553, 127)
		self.SlotBg.LoadImage("schicksal/slot.tga")
		self.SlotBg.AddFlag("not_pick")
		self.SlotBg.Show()

		# Last reward count (on the right slot)
		self.LastCountText = ui.TextLine()
		self.LastCountText.SetParent(self.SlotBg)
		self.LastCountText.SetPosition(48, 44)
		self.LastCountText.SetFontName("Arial:14")
		self.LastCountText.SetFontColor(1.0, 1.0, 0.5)
		try:
			self.LastCountText.SetOutline()
		except:
			pass
		self.LastCountText.Hide()

		# LEFT SIDE: grid panel (4x4)
		# We compute the wheel area size from the old wheel image, then center the grid into it.
		wheel_x, wheel_y = 15, 45
		wheel_w, wheel_h = 480, 480
		try:
			_tw = ui.ExpandedImageBox()
			_tw.LoadImage("schicksal/schicksal_rad.tga")
			wheel_w = _tw.GetWidth() or wheel_w
			wheel_h = _tw.GetHeight() or wheel_h
			_tw.Hide()
		except:
			pass

		# We load one slot.tga to get its size for spacing.
		tmp = ui.ExpandedImageBox()
		tmp.LoadImage("schicksal/slot.tga")
		slot_w = tmp.GetWidth()
		slot_h = tmp.GetHeight()
		tmp.Hide()

		# nice spacing (small gap so it looks like a table)
		gap = 50
		grid_w = self.GRID_COLS * slot_w + (self.GRID_COLS - 1) * gap
		grid_h = self.GRID_ROWS * slot_h + (self.GRID_ROWS - 1) * gap

		# keep sizes around for icon centering/scaling
		self._wheelX, self._wheelY = wheel_x, wheel_y
		self._wheelW, self._wheelH = wheel_w, wheel_h
		self._slotW, self._slotH = slot_w, slot_h
		self._gap = gap

		self.Grid = ui.Window()
		self.Grid.SetParent(self.Board)
		self.Grid.SetSize(grid_w, grid_h)

		grid_x = wheel_x + max(0, (wheel_w - grid_w) // 2 + 150)
		grid_y = wheel_y + max(0, (wheel_h - grid_h) // 2 + 150)
		self._gridX, self._gridY = grid_x, grid_y
		self._gridW, self._gridH = grid_w, grid_h
		self.Grid.SetPosition(grid_x, grid_y)
		self.Grid.Show()

		for i in range(self.SLOT_COUNT):
			col = i % self.GRID_COLS
			row = i // self.GRID_COLS

			bg = ui.ExpandedImageBox()
			bg.SetParent(self.Grid)
			bg.LoadImage("schicksal/slot.tga")
			bg.SetPosition(col * (slot_w + gap), row * (slot_h + gap))
			bg.Show()

			# highlight overlay (hidden by default)
			hl = ui.Bar()
			hl.SetParent(bg)
			hl.SetPosition(2, 2)
			hl.SetSize(slot_w - 4, slot_h - 4)
			hl.SetColor(0x00000000)
			hl.Hide()

			# icon placeholder (we'll scale/center on SetIcons)
			icon = ui.ExpandedImageBox()
			icon.SetParent(bg)
			# don't load image yet (SetIcons will load)
			icon.SetPosition(0, 0)
			icon.Show()

			# count text placeholder (parent to bg so it's always bottom-right of the slot)
			cnt = ui.TextLine()
			cnt.SetParent(bg)
			cnt.SetFontName("Arial:12")
			cnt.SetFontColor(1.0, 1.0, 0.5)
			try:
				cnt.SetOutline()
			except:
				pass
			cnt.Hide()

			self.GridSlots.append(bg)
			self.GridHL.append(hl)
			self.GridIcons.append(icon)
			self.GridCounts.append(cnt)

		# SPIN button (centered under the grid)
		self.Button = ui.Button()
		self.Button.SetParent(self.Board)
		self.Button.SetUpVisual("schicksal/schicksal_drehen_normal.tga")
		self.Button.SetOverVisual("schicksal/schicksal_drehen_over.tga")
		self.Button.SetDownVisual("schicksal/schicksal_drehen_down.tga")
		# centered under grid (try to use real button size)
		btn_w = 80
		btn_h = 80
		try:
			_tb = ui.ExpandedImageBox()
			_tb.LoadImage("schicksal/schicksal_drehen_normal.tga")
			btn_w = _tb.GetWidth() or btn_w
			btn_h = _tb.GetHeight() or btn_h
			_tb.Hide()
		except:
			pass

		# Put the button under the grid, but clamp inside the old wheel area so it never overlaps badly.
		btn_x = grid_x + (grid_w - btn_w) // 2 + 100
		btn_y = 570
		# try:
			# max_y = wheel_y + wheel_h - btn_h - 12
			# if btn_y > max_y:
				# btn_y = max_y
		# except:
			# pass
		self.Button.SetPosition(btn_x, btn_y)
		self.Button.SetEvent(lambda: net.WheelPacket(2))
		self.Button.Show()

		# Price / free spins texts (kept)
		self.Yang = ui.TextLine()
		self.Yang.SetParent(self.Board)
		self.Yang.SetPosition(57, 576)
		self.Yang.SetFontColor(0.5, 1.0, 0.5)
		self.Yang.SetFontName("Arial:16")
		self.Yang.Show()

		self.FreeSpin = ui.TextLine()
		self.FreeSpin.SetParent(self.Board)
		self.FreeSpin.SetPosition(450, 574)
		self.FreeSpin.SetFontColor(0.5, 1.0, 0.5)
		self.FreeSpin.SetFontName("Arial:20")
		self.FreeSpin.Show()

	# ---------------------------
	# Server -> client (start spin)
	# ---------------------------
	def TurnWheel(self, ItemId, Count, Spin, Random):
		# Random should be 0..15 (target index)
		try:
			target = int(Random) % self.SLOT_COUNT
		except:
			target = 0

		try:
			loops = int(Spin)
			if loops < 0:
				loops = 0
		except:
			loops = 0

		self.Button.Disable()

		self.Item = int(ItemId)
		self.ItemCount = int(Count)

		# total steps: full loops + stop on target
		# start from currentIndex+1, so add 1
		self._targetIndex = target
		self._stepsRemaining = loops * self.SLOT_COUNT + target + 1
		self._animActive = True

		# reset highlight to start fresh
		self.__ClearAllHighlights()
		self._currentIndex = -1

		# schedule first step now
		self._nextStepTime = self.__Now()

	def __Now(self):
		if app:
			try:
				return float(app.GetTime())
			except:
				pass
		# fallback: step every frame if no app
		return 0.0

	# ---------------------------
	# Slot content (server fills 16 items)
	# ---------------------------
	def SetIcons(self, vnum, count, number):
		try:
			idx = int(number)
		except:
			return
		if idx < 0 or idx >= self.SLOT_COUNT:
			return

		# Server usually starts a fresh list with number==0.
		# Clear visuals so we never keep stale icons/counts.
		if idx == 0:
			for i in range(self.SLOT_COUNT):
				try:
					self.GridIcons[i].Hide()
				except:
					pass
				try:
					self.GridCounts[i].Hide()
				except:
					pass
			# keep stored vnums/counts consistent
			self.GridVnum = [0] * self.SLOT_COUNT
			self.GridItemCount = [0] * self.SLOT_COUNT

		# store
		try:
			self.GridVnum[idx] = int(vnum)
		except:
			self.GridVnum[idx] = 0

		try:
			c = int(count)
		except:
			c = 0
		self.GridItemCount[idx] = c

		# load icon
		if self.GridVnum[idx]:
			item.SelectItem(self.GridVnum[idx])
			itemIcon = item.GetIconImageFileName()
		else:
			itemIcon = ""

		icon = self.GridIcons[idx]
		if itemIcon:
			# reset scaling (if client supports it)
			try:
				icon.SetScale(1.0, 1.0)
			except:
				pass
			icon.LoadImage(itemIcon)
		else:
			# hide if empty
			icon.Hide()
			self.GridCounts[idx].Hide()
			return

		# center (and scale-down if needed) inside slot bg
		bg = self.GridSlots[idx]
		try:
			iw = int(icon.GetWidth() or 0)
			ih = int(icon.GetHeight() or 0)
			bw = int(bg.GetWidth() or self._slotW or 0)
			bh = int(bg.GetHeight() or self._slotH or 0)

			inner_w = max(1, bw - 8)
			inner_h = max(1, bh - 8)

			scale = 1.0
			if iw > 0 and ih > 0:
				scale = min(float(inner_w) / float(iw), float(inner_h) / float(ih), 1.0)

			# If the client supports ExpandedImageBox scaling, enforce fit.
			try:
				if scale < 1.0:
					icon.SetScale(scale, scale)
			except:
				scale = 1.0

			draw_w = int(iw * scale)
			draw_h = int(ih * scale)
			icon.SetPosition((bw - draw_w) // 2, (bh - draw_h) // 2)
		except:
			icon.SetPosition(0, 0)
		icon.Show()

		# count label (if > 1) -> always bottom-right of the SLOT
		cnt = self.GridCounts[idx]
		if c > 1:
			cnt.SetText("x%d" % c)
			try:
				bw = int(bg.GetWidth() or self._slotW or 0)
				bh = int(bg.GetHeight() or self._slotH or 0)
				cnt.SetPosition(max(0, bw - 30), max(0, bh - 18))
			except:
				cnt.SetPosition(0, 0)
			cnt.Show()
		else:
			cnt.Hide()

	# ---------------------------
	# Last reward slot (right)
	# ---------------------------
	def __BuildLastItem(self, path):
		# remove old icon if present
		try:
			if self.ItemIcon:
				self.ItemIcon.Hide()
		except:
			pass

		self.ItemIcon = ui.ExpandedImageBox()
		self.ItemIcon.SetParent(self.SlotBg)
		self.ItemIcon.SetPosition(0, 0)
		self.ItemIcon.LoadImage(path)
		self.ItemIcon.Show()

		if self.ItemCount and int(self.ItemCount) > 1:
			self.LastCountText.SetText("x%d" % int(self.ItemCount))
			self.LastCountText.Show()
		else:
			self.LastCountText.Hide()

	# ---------------------------
	# Highlight helpers
	# ---------------------------
	def __ClearAllHighlights(self):
		for hl in self.GridHL:
			try:
				hl.Hide()
			except:
				pass

	def __SetHighlight(self, idx, color):
		if idx < 0 or idx >= self.SLOT_COUNT:
			return
		# hide others (only 1 active at a time)
		self.__ClearAllHighlights()
		hl = self.GridHL[idx]
		try:
			hl.SetColor(color)
			hl.Show()
		except:
			pass

	# ---------------------------
	# Update loop
	# ---------------------------
	def OnUpdate(self):
		# Tooltip on grid icons + last reward icon
		hovered = False
		try:
			if self.ItemIcon and self.ItemIcon.IsIn():
				self.tooltipItem.SetItemToolTip(self.itemfortip)
				hovered = True
		except:
			pass

		# grid tooltip
		for i in range(self.SLOT_COUNT):
			try:
				if self.GridIcons[i] and self.GridIcons[i].IsShow() and self.GridIcons[i].IsIn():
					v = self.GridVnum[i]
					if v:
						self.tooltipItem.SetItemToolTip(v)
						hovered = True
						break
			except:
				pass

		# nothing hovered
		if not hovered:
			self.tooltipItem.HideToolTip()

		# animation
		if not self._animActive:
			return

		now = self.__Now()
		if app and now < self._nextStepTime:
			return

		# next step
		self._currentIndex = (self._currentIndex + 1) % self.SLOT_COUNT
		self._stepsRemaining -= 1

		if self._stepsRemaining > 0:
			# moving highlight (yellow)
			self.__SetHighlight(self._currentIndex, self.COLOR_YELLOW)

			# slowdown curve
			# first fast, then slower in the last ~2 rounds
			remain = self._stepsRemaining
			delay = 0.04
			if remain <= (self.SLOT_COUNT * 2):
				# ease out
				# grow delay from ~0.05 to ~0.35
				t = float((self.SLOT_COUNT * 2) - remain) / float(self.SLOT_COUNT * 2)
				delay = 0.05 + (0.30 * (t * t))
			if app:
				self._nextStepTime = now + delay
			return

		# finished -> final highlight green
		self.__SetHighlight(self._currentIndex, self.COLOR_GREEN)

		# show last reward on the right
		try:
			item.SelectItem(self.Item)
			self.__BuildLastItem(item.GetIconImageFileName())
			self.itemfortip = self.Item
		except:
			pass

		# notify server & re-enable button
		try:
			net.WheelPacket(3)
		except:
			pass

		self._animActive = False
		self.Button.Enable()

	# ---------------------------
	# Close / open
	# ---------------------------
	def ClosePacket(self):
		# Pörgetés közben ne lehessen bezárni
		if getattr(self, "_animActive", False):
			try:
				import chat
				chat.AppendChat(chat.CHAT_TYPE_INFO, "Do not close now!!!")
			except:
				pass
			return
	
		# Ne zárjuk be lokálisan előbb!
		# Kérjük a szervert a CLOSE-ra, és majd a szerver küldi a BINARY_WHEEL_CLOSE-t,
		# amire ténylegesen bezárjuk az ablakot.
		try:
			net.WheelPacket(1)
		except:
			pass
	
	def OnPressEscapeKey(self):
		self.ClosePacket()
		return True
	

	def Close(self):
		if self.IsShow():
			# stop anim so it doesn't run in background
			self._animActive = False
			self.__ClearAllHighlights()

			self.Board.Hide()
			self.Hide()

	def Open(self, price, free):
		self.ItemIcon = None
		self.ItemCount = 0
		self.LastCountText.Hide()

		# reset highlights
		self._animActive = False
		self.__ClearAllHighlights()
		self._currentIndex = -1

		#self.FreeSpin.SetText(str(free))
		#self.Yang.SetText(localeinfo.NumberToMoneyString(price))

		self.Board.Show()
		self.Show()
