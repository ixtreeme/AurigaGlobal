import app
import net
import chr
import player
import ui
import wndMgr
import messenger
import guild
import nonplayer
import constinfo
import uitooltip
import item
import localeinfo

if app.ENABLE_VIEW_ELEMENT:
	ELEMENT_IMAGE_DIC = {1: "elect", 2: "fire", 3: "ice", 4: "wind", 5: "earth", 6 : "dark"}


if app.ENABLE_SEND_TARGET_INFO:
	def HAS_FLAG(value, flag):
		return (value & flag) == flag
class TargetBoard(ui.ThinBoard):
	if app.__AUTO_QUQUE_ATTACK__:
		def __AutoFarmInfo(self):
			if not self.autoFarmInfo:
				self.autoFarmInfo = self.AutoFarmInfoWindow()
			if self.autoFarmInfo.IsShow():
				self.autoFarmInfo.Close()
			else:
				self.autoFarmInfo.Open()
				(x, y) = self.autoFarmBtn.GetGlobalPosition()
				self.autoFarmInfo.SetPosition(x + 10, y + 30)
	
	if app.ENABLE_SEND_TARGET_INFO:
		class InfoBoard(ui.ThinBoard):
			class ItemListBoxItem(ui.ListBoxExNew.Item):
				def __init__(self, width):
					ui.ListBoxExNew.Item.__init__(self)

					image = ui.ExpandedImageBox()
					image.SetParent(self)
					image.Show()
					self.image = image

					nameLine = ui.TextLine()
					nameLine.SetParent(self)
					nameLine.SetPosition(32 + 5, 0)
					nameLine.Show()
					self.nameLine = nameLine

					self.SetSize(width, 32 + 5)

				def LoadImage(self, image, name = None):
					self.image.LoadImage(image)
					self.SetSize(self.GetWidth(), self.image.GetHeight() + 5 * (self.image.GetHeight() / 32))
					if name != None:
						self.SetText(name)

				def SetText(self, text):
					self.nameLine.SetText(text)

				def SetImageEventOverIn(self, func, arg):
					if self.image:
						self.image.SetStringEvent("MOUSE_OVER_IN", ui.__mem_func__(func), arg)

				def SetImageEventOverOut(self, func):
					if self.image:
						self.image.SetStringEvent("MOUSE_OVER_OUT", ui.__mem_func__(func))

				def RefreshHeight(self):
					ui.ListBoxExNew.Item.RefreshHeight(self)
					self.image.SetRenderingRect(0.0, 0.0 - float(self.removeTop) / float(self.GetHeight()), 0.0, 0.0 - float(self.removeBottom) / float(self.GetHeight()))
					self.image.SetPosition(0, - self.removeTop)

			MAX_ITEM_COUNT = 5

			EXP_BASE_LVDELTA = [
				1,  #  -15 0
				5,  #  -14 1
				10, #  -13 2
				20, #  -12 3
				30, #  -11 4
				50, #  -10 5
				70, #  -9  6
				80, #  -8  7
				85, #  -7  8
				90, #  -6  9
				92, #  -5  10
				94, #  -4  11
				96, #  -3  12
				98, #  -2  13
				100,	#  -1  14
				100,	#  0   15
				105,	#  1   16
				110,	#  2   17
				115,	#  3   18
				120,	#  4   19
				125,	#  5   20
				130,	#  6   21
				135,	#  7   22
				140,	#  8   23
				145,	#  9   24
				150,	#  10  25
				155,	#  11  26
				160,	#  12  27
				165,	#  13  28
				170,	#  14  29
				180,	#  15  30
			]

			RACE_FLAG_TO_NAME = {
				1 << 0  : localeinfo.TARGET_INFO_RACE_ANIMAL,
				1 << 1 	: localeinfo.TARGET_INFO_RACE_UNDEAD,
				1 << 2  : localeinfo.TARGET_INFO_RACE_DEVIL,
				1 << 3  : localeinfo.TARGET_INFO_RACE_HUMAN,
				1 << 4  : localeinfo.TARGET_INFO_RACE_ORC,
				1 << 5  : localeinfo.TARGET_INFO_RACE_MILGYO,
			}

			#SUB_RACE_FLAG_TO_NAME = {
			#	1 << 11 : localeinfo.TARGET_INFO_RACE_ELEC,
			#	1 << 12 : localeinfo.TARGET_INFO_RACE_FIRE,
			#	1 << 13 : localeinfo.TARGET_INFO_RACE_ICE,
			#	1 << 14 : localeinfo.TARGET_INFO_RACE_WIND,
			#	1 << 15 : localeinfo.TARGET_INFO_RACE_EARTH,
			#	1 << 16 : localeinfo.TARGET_INFO_RACE_DARK,
			#}

			if app.ENABLE_DS_RUNE:
				RACE_FLAG_TO_NAME[1 << 17] = localeinfo.TARGET_INFO_RACE_RUNE

			STONE_START_VNUM = 28030
			STONE_LAST_VNUM = 28042

			BOARD_WIDTH = 250

			def __init__(self):
				ui.ThinBoard.__init__(self)

				self.HideCorners(self.LT)
				self.HideCorners(self.RT)
				self.HideLine(self.T)

				self.race = 0
				self.hasItems = False

				self.itemTooltip = uitooltip.ItemToolTip()
				self.itemTooltip.HideToolTip()
				self.itemTooltip2 = uitooltip.ItemToolTip()
				self.itemTooltip2.HideToolTip()
				self.stoneImg = None
				self.stoneVnum = None
				self.stofaImg = None
				self.stofaVnum = None
				self.stofaStep = 0
				self.lastStofaVnum = 0
				self.nextStofaIconChange = 0
				self.saccaImg = None
				self.saccaVnum = None
				self.saccaStep = 0
				self.lastSaccaVnum = 0
				self.nextSaccaIconChange = 0
				self.sockImg = None
				self.sockVnum = None
				self.sockStep = 0
				self.lastSockVnum = 0
				self.nextSockIconChange = 0
				self.chestImg = None
				self.chestVnum = None
				self.chestStep = 0
				self.lastChestVnum = 0
				self.nextChestIconChange = 0
				self.chestBossImg = None
				self.chestBossVnum = None
				self.chestBossStep = 0
				self.lastChestBossVnum = 0
				self.nextChestBossIconChange = 0
				self.itemScrollBar = None
				self.lastStoneVnum = 0
				self.nextStoneIconChange = 0

				self.SetSize(self.BOARD_WIDTH + 50, 0)

			def __del__(self):
				ui.ThinBoard.__del__(self)

			def __UpdatePosition(self, targetBoard):
				self.SetPosition(targetBoard.GetLeft() + (targetBoard.GetWidth() - self.GetWidth()) / 2, targetBoard.GetBottom() - 17)

			def Open(self, targetBoard, race):
				if self.itemScrollBar != None:
					self.itemScrollBar.Destroy()
					self.itemScrollBar = None
				
				self.__LoadInformation(race)

				self.SetSize(self.BOARD_WIDTH, self.yPos + 10)
				self.__UpdatePosition(targetBoard)

				self.Show()

			def Refresh(self):
				self.__LoadInformation(self.race)
				self.SetSize(self.BOARD_WIDTH, self.yPos + 10)

			def Close(self):
				self.itemTooltip.HideToolTip()
				self.itemTooltip2.HideToolTip()
				self.Hide()

			def __LoadInformation(self, race):
				self.yPos = 7
				self.children = []
				self.race = race
				self.stoneImg = None
				self.stoneVnum = None
				self.nextStoneIconChange = 0
				self.stofaImg = None
				self.stofaVnum = None
				self.stofaStep = 0
				self.nextStofaIconChange = 0
				self.saccaImg = None
				self.saccaVnum = None
				self.saccaStep = 0
				self.nextSaccaIconChange = 0
				self.sockImg = None
				self.sockVnum = None
				self.sockStep = 0
				self.nextSockIconChange = 0
				self.chestImg = None
				self.chestVnum = None
				self.chestStep = 0
				self.lastChestVnum = 0
				self.nextChestIconChange = 0
				self.chestBossImg = None
				self.chestBossVnum = None
				self.chestBossStep = 0
				self.lastChestBossVnum = 0
				self.nextChestBossIconChange = 0
				self.__LoadInformation_Default(race)
				self.__LoadInformation_Race(race)
				self.__LoadInformation_Drops(race)

			def __LoadInformation_Default_GetHitRate(self, race):
				attacker_dx = nonplayer.GetMonsterDX(race)
				attacker_level = nonplayer.GetMonsterLevel(race)

				self_dx = player.GetStatus(player.DX)
				self_level = player.GetStatus(player.LEVEL)

				iARSrc = min(90, (attacker_dx * 4 + attacker_level * 2) / 6)
				iERSrc = min(90, (self_dx * 4 + self_level * 2) / 6)

				fAR = (float(iARSrc) + 210.0) / 300.0
				fER = (float(iERSrc) * 2 + 5) / (float(iERSrc) + 95) * 3.0 / 10.0

				return fAR - fER

			def __LoadInformation_Default(self, race):
				self.AppendSeperator()
				self.AppendTextLine(localeinfo.TARGET_INFO_MAX_HP % str(nonplayer.GetMonsterMaxHP(race)))

				# calc att damage
				monsterLevel = nonplayer.GetMonsterLevel(race)
				fHitRate = self.__LoadInformation_Default_GetHitRate(race)
				iDamMin, iDamMax = nonplayer.GetMonsterDamage(race)
				iDamMin = int((iDamMin + nonplayer.GetMonsterST(race)) * 2 * fHitRate) + monsterLevel * 2
				iDamMax = int((iDamMax + nonplayer.GetMonsterST(race)) * 2 * fHitRate) + monsterLevel * 2
				iDef = player.GetStatus(player.DEF_GRADE) * (100 + player.GetStatus(player.DEF_BONUS)) / 100
				fDamMulti = nonplayer.GetMonsterDamageMultiply(race)
				iDamMin = int(max(0, iDamMin - iDef) * fDamMulti)
				iDamMax = int(max(0, iDamMax - iDef) * fDamMulti)
				if iDamMin < 1:
					iDamMin = 1
				if iDamMax < 5:
					iDamMax = 5
				self.AppendTextLine(localeinfo.TARGET_INFO_DAMAGE % (str(iDamMin), str(iDamMax)))

				idx = min(len(self.EXP_BASE_LVDELTA) - 1, max(0, (monsterLevel + 15) - player.GetStatus(player.LEVEL)))
				iExp = nonplayer.GetMonsterExp(race) * self.EXP_BASE_LVDELTA[idx] / 100
				self.AppendTextLine(localeinfo.TARGET_INFO_EXP % str(iExp))

			def __LoadInformation_Race(self, race):
				dwRaceFlag = nonplayer.GetMonsterRaceFlag(race)
				self.AppendSeperator()

				mainrace = ""
				#subrace = ""
				for i in xrange(18):
					curFlag = 1 << i
					if HAS_FLAG(dwRaceFlag, curFlag):
						if self.RACE_FLAG_TO_NAME.has_key(curFlag):
							mainrace += self.RACE_FLAG_TO_NAME[curFlag] + ", "
						#elif self.SUB_RACE_FLAG_TO_NAME.has_key(curFlag):
							#subrace += self.SUB_RACE_FLAG_TO_NAME[curFlag] + ", "
				if nonplayer.IsMonsterStone(race):
					mainrace += localeinfo.TARGET_INFO_RACE_METIN + ", "
				if mainrace == "":
					mainrace = localeinfo.TARGET_INFO_NO_RACE
				else:
					mainrace = mainrace[:-2]
				# if subrace == "":
					# subrace = localeinfo.TARGET_INFO_NO_RACE
				# else:
					# subrace = subrace[:-2]

				self.AppendTextLine(localeinfo.TARGET_INFO_MAINRACE % mainrace)
				# self.AppendTextLine(localeinfo.TARGET_INFO_SUBRACE % subrace)

			def __LoadInformation_Drops(self, race):
				self.AppendSeperator()

				if race in constinfo.MONSTER_INFO_DATA:
					if len(constinfo.MONSTER_INFO_DATA[race]["items"]) == 0:
						self.AppendTextLine(localeinfo.TARGET_INFO_NO_ITEM_TEXT)
					else:
						itemListBox = ui.ListBoxExNew(32 + 5, self.MAX_ITEM_COUNT)
						itemListBox.SetSize(self.GetWidth() - 15 * 2 - ui.ScrollBar.SCROLLBAR_WIDTH, (32 + 5) * self.MAX_ITEM_COUNT)
						height = 0
						for curItem in constinfo.MONSTER_INFO_DATA[race]["items"]:
							if curItem.has_key("vnum_list"):
								height += self.AppendItem(itemListBox, curItem["vnum_list"], curItem["count"])
							else:
								height += self.AppendItem(itemListBox, curItem["vnum"], curItem["count"])
						if height < itemListBox.GetHeight():
							itemListBox.SetSize(itemListBox.GetWidth(), height)
						self.AppendWindow(itemListBox, 15)
						itemListBox.SetBasePos(0)

						if len(constinfo.MONSTER_INFO_DATA[race]["items"]) > itemListBox.GetViewItemCount():
							itemScrollBar = ui.ScrollBar()
							itemScrollBar.SetParent(self)
							itemScrollBar.SetPosition(itemListBox.GetRight(), itemListBox.GetTop())
							itemScrollBar.SetScrollBarSize(32 * self.MAX_ITEM_COUNT + 5 * (self.MAX_ITEM_COUNT - 1))
							itemScrollBar.SetMiddleBarSize(float(self.MAX_ITEM_COUNT) / float(height / (32 + 5)))
							itemScrollBar.Show()
							self.itemScrollBar = itemScrollBar
							itemListBox.SetScrollBar(itemScrollBar)
				else:
					self.AppendTextLine(localeinfo.TARGET_INFO_NO_ITEM_TEXT)

			def OnRunMouseWheel(self, nLen):
				if self.itemScrollBar != None:
					if nLen > 0:
						self.itemScrollBar.OnUp()
						return True
					else:
						self.itemScrollBar.OnDown()
						return True
					
					return False

			def AppendTextLine(self, text):
				textLine = ui.TextLine()
				textLine.SetParent(self)
				textLine.SetWindowHorizontalAlignCenter()
				textLine.SetHorizontalAlignCenter()
				textLine.SetText(text)
				textLine.SetPosition(0, self.yPos)
				textLine.Show()

				self.children.append(textLine)
				self.yPos += 17

			def AppendSeperator(self):
				img = ui.ImageBox()
				img.LoadImage("d:/ymir work/ui/seperator.tga")
				self.AppendWindow(img)
				img.SetPosition(img.GetLeft(), img.GetTop() - 15)
				self.yPos -= 15

			def AppendItem(self, listBox, vnums, count):
				if type(vnums) == int:
					vnum = vnums
				else:
					vnum = vnums[0]
				
				item.SelectItem(vnum)
				itemName = item.GetItemName()
				
				if self.stofaImg != None and (vnum == 80019 or vnum == 80022 or vnum == 80023 or vnum == 80024 or vnum == 80025 or vnum == 80026 or vnum == 80027):
					return 0
				
				if self.saccaImg != None and (vnum == 30094 or vnum == 30095 or vnum == 30096):
					return 0
				
				if self.sockImg != None and (vnum == 78201 or vnum == 78202 or vnum == 78203 or vnum == 78204):
					return 0
				
				if self.chestImg != None and (vnum == 39048 or vnum == 39050 or vnum == 39052 or vnum == 39054):
					return 0
				
				if self.chestBossImg != None and (vnum == 71203 or vnum == 71204 or vnum == 71205 or vnum == 71206):
					return 0
				
				if type(vnums) != int and len(vnums) > 1:
					vnums = sorted(vnums)
					realName = itemName[:itemName.find("+")]
					if item.GetItemType() == item.ITEM_TYPE_METIN:
						realName = localeinfo.TARGET_INFO_STONE_NAME
						itemName = realName + "+0 - +4"
					else:
						itemName = realName + "+" + str(vnums[0] % 10) + " - +" + str(vnums[len(vnums) - 1] % 10)
					vnum = vnums[len(vnums) - 1]

				myItem = self.ItemListBoxItem(listBox.GetWidth())
				myItem.LoadImage(item.GetIconImageFileName())
				if count <= 1:
					myItem.SetText(itemName)
				else:
					myItem.SetText("%dx %s" % (count, itemName))
				
				myItem.SAFE_SetOverInEvent(self.OnShowItemTooltip, vnum)
				myItem.SAFE_SetOverOutEvent(self.OnHideItemTooltip)
				myItem.SetImageEventOverIn(self.OnShowitemTooltip2, vnum)
				myItem.SetImageEventOverOut(self.OnHideItemTooltip2)
				listBox.AppendItem(myItem)

				if item.GetItemType() == item.ITEM_TYPE_METIN:
					self.stoneImg = myItem
					self.stoneVnum = vnums
					self.lastStoneVnum = self.STONE_LAST_VNUM + vnums[len(vnums) - 1] % 1000 / 100 * 100
				
				if vnum == 80019 or vnum == 80022 or vnum == 80023 or vnum == 80024 or vnum == 80025 or vnum == 80026 or vnum == 80027:
					self.stofaImg = myItem
					self.stofaVnum = vnum
					self.lastStofaVnum = 80019
					self.stofaStep = 0
				
				if vnum == 30094 or vnum == 30095 or vnum == 30096:
					self.saccaImg = myItem
					self.saccaVnum = vnum
					self.lastSaccaVnum = 30094
					self.saccaStep = 0
				
				if vnum == 78201 or vnum == 78202 or vnum == 78203 or vnum == 78204:
					self.sockImg = myItem
					self.sockVnum = vnum
					self.lastSockVnum = 78201
					self.sockStep = 0
				
				if vnum == 39048 or vnum == 39050 or vnum == 39052 or vnum == 39054:
					self.chestImg = myItem
					self.chestVnum = vnum
					self.lastChestVnum = 39048
					self.chestStep = 0
				
				if vnum == 71203 or vnum == 71204 or vnum == 71205 or vnum == 71206:
					self.chestBossImg = myItem
					self.chestBossVnum = vnum
					self.lastChestBossVnum = 71203
					self.chestBossStep = 0
				
				return myItem.GetHeight()

			def OnShowitemTooltip2(self, vnum):
				item.SelectItem(vnum)
				if item.GetItemType() == item.ITEM_TYPE_METIN:
					self.itemTooltip2.isStone = True
					self.itemTooltip2.isBook = False
					self.itemTooltip2.isBook2 = False
					self.itemTooltip2.isStofa = False
					self.itemTooltip2.isSacca = False
					self.itemTooltip2.isSock = False
					self.itemTooltip2.isChest = False
					self.itemTooltip2.isBossChest = False
					self.itemTooltip2.SetItemToolTip(self.lastStoneVnum)
				elif vnum == 80019 or vnum == 80022 or vnum == 80023 or vnum == 80024 or vnum == 80025 or vnum == 80026 or vnum == 80027:
					self.itemTooltip2.isStone = False
					self.itemTooltip2.isBook = False
					self.itemTooltip2.isBook2 = False
					self.itemTooltip2.isStofa = True
					self.itemTooltip2.isSacca = False
					self.itemTooltip2.isSock = False
					self.itemTooltip2.isChest = False
					self.itemTooltip2.isBossChest = False
					self.itemTooltip2.SetItemToolTip(self.lastStofaVnum)
				elif vnum == 30094 or vnum == 30095 or vnum == 30096:
					self.itemTooltip2.isStone = False
					self.itemTooltip2.isBook = False
					self.itemTooltip2.isBook2 = False
					self.itemTooltip2.isStofa = False
					self.itemTooltip2.isSacca = True
					self.itemTooltip2.isSock = False
					self.itemTooltip2.isChest = False
					self.itemTooltip2.isBossChest = False
					self.itemTooltip2.SetItemToolTip(self.lastSaccaVnum)
				elif vnum == 78201 or vnum == 78202 or vnum == 78203 or vnum == 78204:
					self.itemTooltip2.isStone = False
					self.itemTooltip2.isBook = False
					self.itemTooltip2.isBook2 = False
					self.itemTooltip2.isStofa = False
					self.itemTooltip2.isSacca = False
					self.itemTooltip2.isSock = True
					self.itemTooltip2.isChest = False
					self.itemTooltip2.isBossChest = False
					self.itemTooltip2.SetItemToolTip(self.lastSockVnum)
				elif vnum == 39048 or vnum == 39050 or vnum == 39052 or vnum == 39054:
					self.itemTooltip2.isStone = False
					self.itemTooltip2.isBook = False
					self.itemTooltip2.isBook2 = False
					self.itemTooltip2.isStofa = False
					self.itemTooltip2.isSacca = False
					self.itemTooltip2.isSock = False
					self.itemTooltip2.isChest = True
					self.itemTooltip2.isBossChest = False
					self.itemTooltip2.SetItemToolTip(self.lastChestVnum)
				elif vnum == 71203 or vnum == 71204 or vnum == 71205 or vnum == 71206:
					self.itemTooltip2.isStone = False
					self.itemTooltip2.isBook = False
					self.itemTooltip2.isBook2 = False
					self.itemTooltip2.isStofa = False
					self.itemTooltip2.isSacca = False
					self.itemTooltip2.isSock = False
					self.itemTooltip2.isChest = False
					self.itemTooltip2.isBossChest = True
					self.itemTooltip2.SetItemToolTip(self.lastChestBossVnum)
				else:
					self.itemTooltip2.isStone = False
					self.itemTooltip2.isBook = True
					self.itemTooltip2.isBook2 = True
					self.itemTooltip2.isStofa = False
					self.itemTooltip2.isSacca = False
					self.itemTooltip2.isSock = False
					self.itemTooltip2.isChest = False
					self.itemTooltip2.isBossChest = False
					self.itemTooltip2.SetItemToolTip(vnum)

			def OnHideItemTooltip2(self):
				self.itemTooltip2.HideToolTip()

			def OnShowItemTooltip(self, vnum):
				item.SelectItem(vnum)
				if item.GetItemType() == item.ITEM_TYPE_METIN:
					self.itemTooltip.isStone = True
					self.itemTooltip.isBook = False
					self.itemTooltip.isBook2 = False
					self.itemTooltip.isStofa = False
					self.itemTooltip.isSacca = False
					self.itemTooltip.isSock = False
					self.itemTooltip.isChest = False
					self.itemTooltip.isBossChest = False
					self.itemTooltip.SetItemToolTip(self.lastStoneVnum)
				elif vnum == 80019 or vnum == 80022 or vnum == 80023 or vnum == 80024 or vnum == 80025 or vnum == 80026 or vnum == 80027:
					self.itemTooltip.isStone = False
					self.itemTooltip.isBook = False
					self.itemTooltip.isBook2 = False
					self.itemTooltip.isStofa = True
					self.itemTooltip.isSacca = False
					self.itemTooltip.isSock = False
					self.itemTooltip.isChest = False
					self.itemTooltip.isBossChest = False
					self.itemTooltip.SetItemToolTip(self.lastStofaVnum)
				elif vnum == 30094 or vnum == 30095 or vnum == 30096:
					self.itemTooltip.isStone = False
					self.itemTooltip.isBook = False
					self.itemTooltip.isBook2 = False
					self.itemTooltip.isStofa = False
					self.itemTooltip.isSacca = True
					self.itemTooltip.isSock = False
					self.itemTooltip.isChest = False
					self.itemTooltip.isBossChest = False
					self.itemTooltip.SetItemToolTip(self.lastSaccaVnum)
				elif vnum == 78201 or vnum == 78202 or vnum == 78203 or vnum == 78204:
					self.itemTooltip.isStone = False
					self.itemTooltip.isBook = False
					self.itemTooltip.isBook2 = False
					self.itemTooltip.isStofa = False
					self.itemTooltip.isSacca = False
					self.itemTooltip.isSock = True
					self.itemTooltip.isChest = False
					self.itemTooltip.isBossChest = False
					self.itemTooltip.SetItemToolTip(self.lastSockVnum)
				elif vnum == 39048 or vnum == 39050 or vnum == 39052 or vnum == 39054:
					self.itemTooltip.isStone = False
					self.itemTooltip.isBook = False
					self.itemTooltip.isBook2 = False
					self.itemTooltip.isStofa = False
					self.itemTooltip.isSacca = False
					self.itemTooltip.isSock = False
					self.itemTooltip.isChest = True
					self.itemTooltip.isBossChest = False
					self.itemTooltip.SetItemToolTip(self.lastChestVnum)
				elif vnum == 71203 or vnum == 71204 or vnum == 71205 or vnum == 71206:
					self.itemTooltip.isStone = False
					self.itemTooltip.isBook = False
					self.itemTooltip.isBook2 = False
					self.itemTooltip.isStofa = False
					self.itemTooltip.isSacca = False
					self.itemTooltip.isSock = False
					self.itemTooltip.isChest = False
					self.itemTooltip.isBossChest = True
					self.itemTooltip.SetItemToolTip(self.lastChestBossVnum)
				else:
					self.itemTooltip.isStone = False
					self.itemTooltip.isBook = True
					self.itemTooltip.isBook2 = True
					self.itemTooltip.isStofa = False
					self.itemTooltip.isSacca = False
					self.itemTooltip.isSock = False
					self.itemTooltip.isChest = False
					self.itemTooltip.isBossChest = False
					self.itemTooltip.SetItemToolTip(vnum)

			def OnHideItemTooltip(self):
				self.itemTooltip.HideToolTip()

			def AppendWindow(self, wnd, x = 0, width = 0, height = 0):
				if width == 0:
					width = wnd.GetWidth()
				if height == 0:
					height = wnd.GetHeight()

				wnd.SetParent(self)
				if x == 0:
					wnd.SetPosition((self.GetWidth() - width) / 2, self.yPos)
				else:
					wnd.SetPosition(x, self.yPos)
				wnd.Show()

				self.children.append(wnd)
				self.yPos += height + 5

			def OnUpdate(self):
				if self.stoneImg != None and self.stoneVnum != None and app.GetTime() >= self.nextStoneIconChange:
					nextImg = self.lastStoneVnum + 1
					if nextImg % 100 > self.STONE_LAST_VNUM % 100:
						nextImg -= (self.STONE_LAST_VNUM - self.STONE_START_VNUM) + 1
					self.lastStoneVnum = nextImg
					self.nextStoneIconChange = app.GetTime() + 2

					item.SelectItem(nextImg)
					itemName = item.GetItemName()
					realName = itemName[:itemName.find("+")]
					realName = realName + "+0 - +4"
					self.stoneImg.LoadImage(item.GetIconImageFileName(), realName)

					if self.itemTooltip.IsShow() and self.itemTooltip.isStone:
						self.itemTooltip.SetItemToolTip(nextImg)
				
					if self.itemTooltip2.IsShow() and self.itemTooltip2.isStone:
						self.itemTooltip2.SetItemToolTip(nextImg)
				
				if self.stofaImg != None and self.stofaVnum != None and app.GetTime() >= self.nextStofaIconChange:
					myList = [
								80019,
								80022,
								80023,
								80024,
								80025,
								80026,
								80027,
					]
					
					if self.stofaStep >= 6:
						self.stofaStep = 0
					else:
						self.stofaStep += 1
					
					nextImg = myList[self.stofaStep]
					self.lastStofaVnum = nextImg
					self.nextStofaIconChange = app.GetTime() + 2.5
					
					item.SelectItem(nextImg)
					itemName = item.GetItemName()
					self.stofaImg.LoadImage(item.GetIconImageFileName(), itemName)
					if self.itemTooltip.IsShow() and self.itemTooltip.isStofa:
						self.itemTooltip.SetItemToolTip(nextImg)
					
					if self.itemTooltip2.IsShow() and self.itemTooltip2.isStofa:
						self.itemTooltip2.SetItemToolTip(nextImg)
					
				if self.saccaImg != None and self.saccaVnum != None and app.GetTime() >= self.nextSaccaIconChange:
					myList = [
								30094,
								30095,
								30096,
					]
					
					if self.saccaStep >= 2:
						self.saccaStep = 0
					else:
						self.saccaStep += 1
					
					nextImg = myList[self.saccaStep]
					self.lastSaccaVnum = nextImg
					self.nextSaccaIconChange = app.GetTime() + 3
					
					item.SelectItem(nextImg)
					itemName = item.GetItemName()
					self.saccaImg.LoadImage(item.GetIconImageFileName(), itemName)
					if self.itemTooltip.IsShow() and self.itemTooltip.isSacca:
						self.itemTooltip.SetItemToolTip(nextImg)
					
					if self.itemTooltip2.IsShow() and self.itemTooltip2.isSacca:
						self.itemTooltip2.SetItemToolTip(nextImg)
				
				if self.sockImg != None and self.sockVnum != None and app.GetTime() >= self.nextSockIconChange:
					myList = [
								78201,
								78202,
								78203,
								78204
					]
					
					if self.sockStep >= 3:
						self.sockStep = 0
					else:
						self.sockStep += 1
					
					nextImg = myList[self.sockStep]
					self.lastSockVnum = nextImg
					self.nextSockIconChange = app.GetTime() + 3
					
					item.SelectItem(nextImg)
					itemName = item.GetItemName()
					self.sockImg.LoadImage(item.GetIconImageFileName(), itemName)
					if self.itemTooltip.IsShow() and self.itemTooltip.isSock:
						self.itemTooltip.SetItemToolTip(nextImg)
					
					if self.itemTooltip2.IsShow() and self.itemTooltip2.isSock:
						self.itemTooltip2.SetItemToolTip(nextImg)
				
				if self.chestImg != None and self.chestVnum != None and app.GetTime() >= self.nextChestIconChange:
					myList = [
								39048,
								39050,
								39052,
								39054
					]
					
					if self.chestStep >= 3:
						self.chestStep = 0
					else:
						self.chestStep += 1
					
					nextImg = myList[self.chestStep]
					self.lastChestVnum = nextImg
					self.nextChestIconChange = app.GetTime() + 1.6
					
					item.SelectItem(nextImg)
					itemName = item.GetItemName()
					self.chestImg.LoadImage(item.GetIconImageFileName(), itemName)
					if self.itemTooltip.IsShow() and self.itemTooltip.isChest:
						self.itemTooltip.SetItemToolTip(nextImg)
					
					if self.itemTooltip2.IsShow() and self.itemTooltip2.isChest:
						self.itemTooltip2.SetItemToolTip(nextImg)
					
				if self.chestBossImg != None and self.chestBossVnum != None and app.GetTime() >= self.nextChestBossIconChange:
					myList = [
								71203,
								71204,
								71205,
								71206
					]
					
					if self.chestBossStep >= 3:
						self.chestBossStep = 0
					else:
						self.chestBossStep += 1
					
					nextImg = myList[self.chestBossStep]
					self.lastChestBossVnum = nextImg
					self.nextChestBossIconChange = app.GetTime() + 1.6
					
					item.SelectItem(nextImg)
					itemName = item.GetItemName()
					self.chestBossImg.LoadImage(item.GetIconImageFileName(), itemName)
					if self.itemTooltip.IsShow() and self.itemTooltip.isBossChest:
						self.itemTooltip.SetItemToolTip(nextImg)
						
						
	if app.__AUTO_QUQUE_ATTACK__:
		class AutoFarmInfoWindow(ui.ThinBoard):
			def __init__(self):
				ui.ThinBoard.__init__(self)
				self.__LoadWindow()

			def __LoadWindow(self):
				self.SetSize(356, 166)
				self.AddFlag("float")
				self.AddFlag("not_pick")

				title = ui.TextLine()
				title.SetParent(self)
				title.AddFlag("not_pick")
				title.SetHorizontalAlignCenter()
				title.SetPackedFontColor(0xFFFBC401)
				title.SetOutline()
				title.SetText("Target Metin Quque")
				title.SetPosition(self.GetWidth() / 2, 15)
				title.Show()
				self.title = title
				
				firstText = "Pressing |cFF74FF20Shift + Right Click|r on Metinstones your character will target#"\
				"a MetinStone and after you destroyed it, it will move to |cFF74FF20another one|r."

				firstDescription = ui.MultiTextLine()
				firstDescription.SetParent(self)
				firstDescription.AddFlag("not_pick")
				firstDescription.SetTextRange(13)
				firstDescription.SetTextType("horizontal#center")
				firstDescription.SetPosition(self.GetWidth() / 2, 37)
				firstDescription.SetOutline(1)
				firstDescription.SetText(firstText)
				firstDescription.Show()
				self.firstDescription = firstDescription

				secondText = "Max. 3 queued Metinstones (without Premium Metinfarm)#"\
				"Max. 5 queued Metinstones (with Premium Metinfarm)"

				secondDescription = ui.MultiTextLine()
				secondDescription.SetParent(self)
				secondDescription.AddFlag("not_pick")
				secondDescription.SetTextRange(13)
				secondDescription.SetTextType("horizontal#center")
				secondDescription.SetPosition(self.GetWidth() / 2, 73)
				secondDescription.SetOutline(1)
				secondDescription.SetPackedFontColor(0xFFFFD071)
				secondDescription.SetText(secondText)
				secondDescription.Show()
				self.secondDescription = secondDescription


				item.SelectItem(61400)
				itemIcon = ui.ImageBox()
				itemIcon.SetParent(self)
				itemIcon.AddFlag("not_pick")
				itemIcon.LoadImage(item.GetIconImageFileName())
				itemIcon.SetPosition((self.GetWidth() / 2) - 16, 105)
				itemIcon.Show()
				self.itemIcon = itemIcon

				itemName = ui.TextLine()
				itemName.SetParent(self)
				itemName.AddFlag("not_pick")
				itemName.SetHorizontalAlignCenter()
				itemName.SetPackedFontColor(0xFFFBC401)
				itemName.SetOutline()
				itemName.SetText("Premium Metinfarm")
				itemName.SetPosition(self.GetWidth() / 2, 105 + 32 + 5)
				itemName.Show()
				self.itemName = itemName

				self.SetCenterPosition()

			def Open(self):
				self.Show()
				self.SetTop()
			def Close(self):
				self.Hide()
			def OnPressEscapeKey(self):
				self.Close()
				return True

	BUTTON_NAME_LIST = (
		localeinfo.TARGET_BUTTON_WHISPER,
		localeinfo.TARGET_BUTTON_EXCHANGE,
		localeinfo.TARGET_BUTTON_FIGHT,
		localeinfo.TARGET_BUTTON_ACCEPT_FIGHT,
		localeinfo.TARGET_BUTTON_AVENGE,
		localeinfo.TARGET_BUTTON_FRIEND,
		localeinfo.TARGET_BUTTON_INVITE_PARTY,
		localeinfo.TARGET_BUTTON_LEAVE_PARTY,
		localeinfo.TARGET_BUTTON_EXCLUDE,
		localeinfo.TARGET_BUTTON_INVITE_GUILD,
		localeinfo.TARGET_BUTTON_DISMOUNT,
		localeinfo.TARGET_BUTTON_EXIT_OBSERVER,
		localeinfo.TARGET_BUTTON_VIEW_EQUIPMENT,
		localeinfo.TARGET_BUTTON_REQUEST_ENTER_PARTY,
		localeinfo.TARGET_BUTTON_BUILDING_DESTROY,
		localeinfo.TARGET_BUTTON_EMOTION_ALLOW,
		"VOTE_BLOCK_CHAT",
	)

	GRADE_NAME =	{
						nonplayer.PAWN : localeinfo.TARGET_LEVEL_PAWN,
						nonplayer.S_PAWN : localeinfo.TARGET_LEVEL_S_PAWN,
						nonplayer.KNIGHT : localeinfo.TARGET_LEVEL_KNIGHT,
						nonplayer.S_KNIGHT : localeinfo.TARGET_LEVEL_S_KNIGHT,
						nonplayer.BOSS : localeinfo.TARGET_LEVEL_BOSS,
						nonplayer.KING : localeinfo.TARGET_LEVEL_KING,
					}
	EXCHANGE_LIMIT_RANGE = 3000

	def __init__(self):
		ui.ThinBoard.__init__(self)

		name = ui.TextLine()
		name.SetParent(self)
		name.SetDefaultFontName()
		name.SetOutline()
		name.Show()
		
		if app.__AUTO_QUQUE_ATTACK__:
			autoFarmText = ui.TextLine()
			autoFarmText.SetParent(self)
			autoFarmText.SetOutline()
			autoFarmText.SetHorizontalAlignCenter()
			autoFarmText.SetText("|Eemoji/key_shift|e + |Eemoji/key_rclick|e Target Metin Quque")
			autoFarmText.Hide()
			self.autoFarmText = autoFarmText

			autoFarmBtn = ui.Button()
			autoFarmBtn.SetParent(self)
			autoFarmBtn.SetUpVisual("d:/ymir work/ui/pattern/q_mark_01.tga")
			autoFarmBtn.SetOverVisual("d:/ymir work/ui/pattern/q_mark_02.tga")
			autoFarmBtn.SetDownVisual("d:/ymir work/ui/pattern/q_mark_01.tga")
			autoFarmBtn.SetEvent(ui.__mem_func__(self.__AutoFarmInfo))
			autoFarmBtn.Hide()
			self.autoFarmBtn = autoFarmBtn

			self.autoFarmInfo = None

		hpGauge = ui.Gauge()
		hpGauge.SetParent(self)
		hpGauge.MakeGauge(130, "red")
		hpGauge.Hide()
		if app.ENABLE_VIEW_TARGET_DECIMAL_HP:
			hpDecimal = ui.TextLine()
			hpDecimal.SetParent(hpGauge)
			hpDecimal.SetDefaultFontName()
			hpDecimal.SetPosition(-100, 5)
			hpDecimal.SetOutline()
			hpDecimal.Hide()

		closeButton = ui.Button()
		closeButton.SetParent(self)
		closeButton.SetUpVisual("d:/ymir work/ui/public/close_button_01.sub")
		closeButton.SetOverVisual("d:/ymir work/ui/public/close_button_02.sub")
		closeButton.SetDownVisual("d:/ymir work/ui/public/close_button_03.sub")
		closeButton.SetPosition(30, 13)

		if localeinfo.IsARABIC():
			hpGauge.SetPosition(55, 17)
			hpGauge.SetWindowHorizontalAlignLeft()
			closeButton.SetWindowHorizontalAlignLeft()
		else:
			hpGauge.SetPosition(175, 17)
			hpGauge.SetWindowHorizontalAlignRight()
			closeButton.SetWindowHorizontalAlignRight()
		if app.ENABLE_SEND_TARGET_INFO:
			infoButton = ui.Button()
			infoButton.SetParent(self)
			infoButton.SetUpVisual("d:/ymir work/ui/pattern/q_mark_01.tga")
			infoButton.SetOverVisual("d:/ymir work/ui/pattern/q_mark_02.tga")
			infoButton.SetDownVisual("d:/ymir work/ui/pattern/q_mark_01.tga")
			infoButton.SetEvent(ui.__mem_func__(self.OnPressedInfoButton))
			infoButton.Hide()

			infoBoard = self.InfoBoard()
			infoBoard.Hide()
			infoButton.showWnd = infoBoard
		closeButton.SetEvent(ui.__mem_func__(self.OnPressedCloseButton))
		closeButton.Show()

		self.buttonDict = {}
		self.showingButtonList = []
		for buttonName in self.BUTTON_NAME_LIST:
			button = ui.Button()
			button.SetParent(self)

			if localeinfo.IsARABIC():
				button.SetUpVisual("d:/ymir work/ui/public/Small_Button_01.sub")
				button.SetOverVisual("d:/ymir work/ui/public/Small_Button_02.sub")
				button.SetDownVisual("d:/ymir work/ui/public/Small_Button_03.sub")
			else:
				button.SetUpVisual("d:/ymir work/ui/public/small_thin_button_01.sub")
				button.SetOverVisual("d:/ymir work/ui/public/small_thin_button_02.sub")
				button.SetDownVisual("d:/ymir work/ui/public/small_thin_button_03.sub")

			button.SetWindowHorizontalAlignCenter()
			button.SetText(buttonName)
			button.Hide()
			self.buttonDict[buttonName] = button
			self.showingButtonList.append(button)

		self.buttonDict[localeinfo.TARGET_BUTTON_WHISPER].SetEvent(ui.__mem_func__(self.OnWhisper))
		self.buttonDict[localeinfo.TARGET_BUTTON_EXCHANGE].SetEvent(ui.__mem_func__(self.OnExchange))
		self.buttonDict[localeinfo.TARGET_BUTTON_FIGHT].SetEvent(ui.__mem_func__(self.OnPVP))
		self.buttonDict[localeinfo.TARGET_BUTTON_ACCEPT_FIGHT].SetEvent(ui.__mem_func__(self.OnPVP))
		self.buttonDict[localeinfo.TARGET_BUTTON_AVENGE].SetEvent(ui.__mem_func__(self.OnPVP))
		self.buttonDict[localeinfo.TARGET_BUTTON_FRIEND].SetEvent(ui.__mem_func__(self.OnAppendToMessenger))
		self.buttonDict[localeinfo.TARGET_BUTTON_FRIEND].SetEvent(ui.__mem_func__(self.OnAppendToMessenger))
		self.buttonDict[localeinfo.TARGET_BUTTON_INVITE_PARTY].SetEvent(ui.__mem_func__(self.OnPartyInvite))
		self.buttonDict[localeinfo.TARGET_BUTTON_LEAVE_PARTY].SetEvent(ui.__mem_func__(self.OnPartyExit))
		self.buttonDict[localeinfo.TARGET_BUTTON_EXCLUDE].SetEvent(ui.__mem_func__(self.OnPartyRemove))

		self.buttonDict[localeinfo.TARGET_BUTTON_INVITE_GUILD].SAFE_SetEvent(self.__OnGuildAddMember)
		self.buttonDict[localeinfo.TARGET_BUTTON_DISMOUNT].SAFE_SetEvent(self.__OnDismount)
		self.buttonDict[localeinfo.TARGET_BUTTON_EXIT_OBSERVER].SAFE_SetEvent(self.__OnExitObserver)
		self.buttonDict[localeinfo.TARGET_BUTTON_VIEW_EQUIPMENT].SAFE_SetEvent(self.__OnViewEquipment)
		self.buttonDict[localeinfo.TARGET_BUTTON_REQUEST_ENTER_PARTY].SAFE_SetEvent(self.__OnRequestParty)
		self.buttonDict[localeinfo.TARGET_BUTTON_BUILDING_DESTROY].SAFE_SetEvent(self.__OnDestroyBuilding)
		self.buttonDict[localeinfo.TARGET_BUTTON_EMOTION_ALLOW].SAFE_SetEvent(self.__OnEmotionAllow)

		self.buttonDict["VOTE_BLOCK_CHAT"].SetEvent(ui.__mem_func__(self.__OnVoteBlockChat))

		self.name = name
		self.hpGauge = hpGauge
		if app.ENABLE_VIEW_TARGET_DECIMAL_HP:
			self.hpDecimal = hpDecimal
		if app.ENABLE_SEND_TARGET_INFO:
			self.infoButton = infoButton
		if app.ENABLE_SEND_TARGET_INFO:
			self.vnum = 0
		self.closeButton = closeButton
		self.nameString = 0
		self.nameLength = 0
		self.vid = 0
		self.eventWhisper = None
		self.isShowButton = False
		if app.ENABLE_VIEW_ELEMENT:
			self.elementImage = None

		self.__Initialize()
		self.ResetTargetBoard()

	def __del__(self):
		ui.ThinBoard.__del__(self)

		print "===================================================== DESTROYED TARGET BOARD"

	def __Initialize(self):
		self.nameString = ""
		self.nameLength = 0
		self.vid = 0
		if app.ENABLE_SEND_TARGET_INFO:
			self.vnum = 0
		self.isShowButton = False

	def Destroy(self):
		self.eventWhisper = None
		if app.ENABLE_SEND_TARGET_INFO:
			self.infoButton = None
		self.closeButton = None
		self.showingButtonList = None
		self.buttonDict = None
		self.name = None
		self.hpGauge = None
		if app.ENABLE_VIEW_TARGET_DECIMAL_HP:
			self.hpDecimal = None
		self.__Initialize()
		if app.ENABLE_VIEW_ELEMENT:
			self.elementImage = None

	if app.ENABLE_SEND_TARGET_INFO:
		def RefreshMonsterInfoBoard(self):
			if not self.infoButton.showWnd.IsShow():
				return

			self.infoButton.showWnd.Refresh()

		def OnPressedInfoButton(self):
			net.SendTargetInfoLoad(player.GetTargetVID())
			if self.infoButton.showWnd.IsShow():
				self.infoButton.showWnd.Close()
			elif self.vnum != 0:
				self.infoButton.showWnd.Open(self, self.vnum)
	def OnPressedCloseButton(self):
		player.ClearTarget()
		self.Close()

	def Close(self):
		self.__Initialize()
		if app.ENABLE_SEND_TARGET_INFO:
			self.infoButton.showWnd.Close()
		self.Hide()
		
		if app.__AUTO_QUQUE_ATTACK__:
			if self.autoFarmInfo:
				self.autoFarmInfo.Close()
				self.autoFarmInfo=None

	def Open(self, vid, name):
		if vid:
			if not constinfo.GET_VIEW_OTHER_EMPIRE_PLAYER_TARGET_BOARD():
				if not player.IsSameEmpire(vid):
					self.Hide()
					return

			if vid != self.GetTargetVID():
				self.ResetTargetBoard()
				self.SetTargetVID(vid)
				self.SetTargetName(name)

			if player.IsMainCharacterIndex(vid):
				self.__ShowMainCharacterMenu()
			elif chr.INSTANCE_TYPE_BUILDING == chr.GetInstanceType(self.vid):
				self.Hide()
			else:
				self.RefreshButton()
				self.Show()
		else:
			self.HideAllButton()
			self.__ShowButton(localeinfo.TARGET_BUTTON_WHISPER)
			self.__ShowButton("VOTE_BLOCK_CHAT")
			self.__ArrangeButtonPosition()
			self.SetTargetName(name)
			self.Show()

	def Refresh(self):
		if self.IsShow():
			if self.IsShowButton():
				self.RefreshButton()

	def RefreshByVID(self, vid):
		if vid == self.GetTargetVID():
			self.Refresh()

	def RefreshByName(self, name):
		if name == self.GetTargetName():
			self.Refresh()

	def __ShowMainCharacterMenu(self):
		canShow=0

		self.HideAllButton()

		if player.IsMountingHorse():
			self.__ShowButton(localeinfo.TARGET_BUTTON_DISMOUNT)
			canShow=1

		if player.IsObserverMode():
			self.__ShowButton(localeinfo.TARGET_BUTTON_EXIT_OBSERVER)
			canShow=1

		if canShow:
			self.__ArrangeButtonPosition()
			self.Show()
		else:
			self.Hide()

	def __ShowNameOnlyMenu(self):
		self.HideAllButton()

	def SetWhisperEvent(self, event):
		self.eventWhisper = event

	def UpdatePosition(self):
		self.SetPosition(wndMgr.GetScreenWidth()/2 - self.GetWidth()/2, 10)
		if self.elementImage:
			self.elementImage.SetPosition(self.GetWidth() - 21, -13)
			
		if app.__AUTO_QUQUE_ATTACK__:
			if chr.GetInstanceType(self.vid) == chr.INSTANCE_TYPE_STONE:
				self.SetSize(self.GetWidth(), self.GetHeight() + 20)

				self.autoFarmText.SetPosition(self.GetWidth() / 2, 30)
				self.autoFarmText.Show()

				self.autoFarmBtn.SetPosition((self.GetWidth() / 2) + (self.autoFarmText.GetTextSize()[1]/2) + 85, 30)
				self.autoFarmBtn.Show()
			else:
				self.autoFarmText.Hide()
				self.autoFarmBtn.Hide()
				if self.autoFarmInfo:
					self.autoFarmInfo.Close()

	def ResetTargetBoard(self):
		for btn in self.buttonDict.values():
			btn.Hide()
		
		self.__Initialize()
		
		self.name.SetPosition(0, 13)
		self.name.SetHorizontalAlignCenter()
		self.name.SetWindowHorizontalAlignCenter()
		self.hpGauge.Hide()
		if app.ENABLE_VIEW_ELEMENT and self.elementImage:
			self.elementImage.Hide()
			self.elementImage = None
		
		if app.ENABLE_SEND_TARGET_INFO:
			self.infoButton.Hide()
			self.infoButton.showWnd.Close()
		
		if app.ENABLE_VIEW_TARGET_DECIMAL_HP:
			self.hpDecimal.Hide()
		
		self.SetSizeNew(250, 65)

	def SetTargetVID(self, vid):
		self.vid = vid
		if app.ENABLE_SEND_TARGET_INFO:
			self.vnum = 0

	def SetEnemyVID(self, vid):
		self.SetTargetVID(vid)
		
		name = chr.GetNameByVID(vid)
		if app.ENABLE_SEND_TARGET_INFO:
			vnum = nonplayer.GetRaceNumByVID(vid)
		
		level = nonplayer.GetLevelByVID(vid)
		grade = nonplayer.GetGradeByVID(vid)

		nameFront = ""
		if -1 != level:
			nameFront += "Lv." + str(level) + " "
		if self.GRADE_NAME.has_key(grade):
			nameFront += "(" + self.GRADE_NAME[grade] + ") "

		self.SetTargetName(nameFront + name)
		if app.ENABLE_SEND_TARGET_INFO:
			(textWidth, textHeight) = self.name.GetTextSize()

			self.infoButton.SetPosition(textWidth + 25, 12)
			self.infoButton.SetWindowHorizontalAlignLeft()

			self.vnum = vnum
			if chr.GetInstanceType(self.vid) == chr.INSTANCE_TYPE_PLAYER:
				self.infoButton.Hide()
			else:
				self.infoButton.Show()

	def GetTargetVID(self):
		return self.vid

	def GetTargetName(self):
		return self.nameString

	def SetTargetName(self, name):
		self.nameString = name
		self.nameLength = len(name)
		self.name.SetText(name)


	if app.ENABLE_VIEW_TARGET_DECIMAL_HP:
		def SetHP(self, hpPercentage, iMinHP, iMaxHP):
			import chat
			if not self.hpGauge.IsShow():
				if app.ENABLE_VIEW_TARGET_PLAYER_HP:
					if self.showingButtonList:
						c = 0
						for i in self.showingButtonList:
							if i.IsShow():
								c += 1
						
						showingButtonCount = c
					else:
						showingButtonCount = 0
					
					if showingButtonCount > 0:
						if chr.GetInstanceType(self.vid) == chr.INSTANCE_TYPE_PLAYER:
							self.SetSizeNew(max(250, showingButtonCount * 75), 65)
						else:
							self.SetSizeNew(200 + 7*self.nameLength, 40)
					else:
						self.SetSizeNew(200 + 7*self.nameLength, 40)
				else:
					self.SetSizeNew(200 + 7*self.nameLength, self.GetHeight())
				
				if localeinfo.IsARABIC():
					self.name.SetPosition( self.GetWidth()-23, 13)
				else:
					self.name.SetPosition(23, 13)
				
				self.name.SetWindowHorizontalAlignLeft()
				self.name.SetHorizontalAlignLeft()
				self.hpGauge.Show()
				self.UpdatePosition()
			
			self.hpGauge.SetPercentage(hpPercentage, 100)
			if app.ENABLE_VIEW_TARGET_DECIMAL_HP:
				iMinHPText = '.'.join([i - 3 < 0 and str(iMinHP)[:i] or str(iMinHP)[i-3:i] for i in range(len(str(iMinHP)) % 3, len(str(iMinHP))+1, 3) if i])
				iMaxHPText = '.'.join([i - 3 < 0 and str(iMaxHP)[:i] or str(iMaxHP)[i-3:i] for i in range(len(str(iMaxHP)) % 3, len(str(iMaxHP))+1, 3) if i])
				self.hpDecimal.SetText(str(iMinHPText) + "/" + str(iMaxHPText))
				(textWidth, textHeight)=self.hpDecimal.GetTextSize()
				if localeinfo.IsARABIC():
					self.hpDecimal.SetPosition(120 / 2 + textWidth / 2, -13)
				else:
					self.hpDecimal.SetPosition(130 / 2 - textWidth / 2, -13)
				
				self.hpDecimal.Show()
	else:
		def SetHP(self, hpPercentage):
			if not self.hpGauge.IsShow():

				self.SetSizeNew(200 + 7*self.nameLength, self.GetHeight())

				if localeinfo.IsARABIC():
					self.name.SetPosition( self.GetWidth()-23, 13)
				else:
					self.name.SetPosition(23, 13)

				self.name.SetWindowHorizontalAlignLeft()
				self.name.SetHorizontalAlignLeft()
				self.hpGauge.Show()
				self.UpdatePosition()

			self.hpGauge.SetPercentage(hpPercentage, 100)

	def ShowDefaultButton(self):

		self.isShowButton = True
		self.showingButtonList.append(self.buttonDict[localeinfo.TARGET_BUTTON_WHISPER])
		self.showingButtonList.append(self.buttonDict[localeinfo.TARGET_BUTTON_EXCHANGE])
		self.showingButtonList.append(self.buttonDict[localeinfo.TARGET_BUTTON_FIGHT])
		self.showingButtonList.append(self.buttonDict[localeinfo.TARGET_BUTTON_EMOTION_ALLOW])
		for button in self.showingButtonList:
			button.Show()

	def HideAllButton(self):
		self.isShowButton = False
		for button in self.showingButtonList:
			button.Hide()
		self.showingButtonList = []

	def __ShowButton(self, name):

		if not self.buttonDict.has_key(name):
			return

		self.buttonDict[name].Show()
		self.showingButtonList.append(self.buttonDict[name])

	def __HideButton(self, name):

		if not self.buttonDict.has_key(name):
			return

		button = self.buttonDict[name]
		button.Hide()

		for btnInList in self.showingButtonList:
			if btnInList == button:
				self.showingButtonList.remove(button)
				break

	def OnWhisper(self):
		if None != self.eventWhisper:
			self.eventWhisper(self.nameString)

	def OnExchange(self):
		net.SendExchangeStartPacket(self.vid)

	def OnPVP(self):
		if app.ENABLE_PVP_ADVANCED:
			net.SendChatPacket("/pvp_advanced %d" % (self.vid))
		else:
			net.SendChatPacket("/pvp %d" % (self.vid))

	def OnAppendToMessenger(self):
		net.SendMessengerAddByVIDPacket(self.vid)

	def OnPartyInvite(self):
		net.SendPartyInvitePacket(self.vid)

	def OnPartyExit(self):
		net.SendPartyExitPacket()

	def OnPartyRemove(self):
		net.SendPartyRemovePacketVID(self.vid)

	def __OnGuildAddMember(self):
		net.SendGuildAddMemberPacket(self.vid)

	def __OnDismount(self):
		net.SendChatPacket("/unmount")

	def __OnExitObserver(self):
		net.SendChatPacket("/observer_exit")

	def __OnViewEquipment(self):
		net.SendChatPacket("/view_equip " + str(self.vid))

	def __OnRequestParty(self):
		net.SendChatPacket("/party_request " + str(self.vid))

	def __OnDestroyBuilding(self):
		net.SendChatPacket("/build d %d" % (self.vid))

	def __OnEmotionAllow(self):
		net.SendChatPacket("/emotion_allow %d" % (self.vid))

	def __OnVoteBlockChat(self):
		cmd = "/vote_block_chat %s" % (self.nameString)
		net.SendChatPacket(cmd)

	def OnPressEscapeKey(self):
		self.OnPressedCloseButton()
		return True

	def IsShowButton(self):
		return self.isShowButton

	def RefreshButton(self):

		self.HideAllButton()

		if chr.INSTANCE_TYPE_BUILDING == chr.GetInstanceType(self.vid):
			#self.__ShowButton(localeinfo.TARGET_BUTTON_BUILDING_DESTROY)
			#self.__ArrangeButtonPosition()
			return

		if player.IsPVPInstance(self.vid) or player.IsObserverMode():
			# PVP_INFO_SIZE_BUG_FIX
			self.SetSize(200 + 7*self.nameLength, 40)
			self.UpdatePosition()
			# END_OF_PVP_INFO_SIZE_BUG_FIX
			return

		self.ShowDefaultButton()

		if guild.MainPlayerHasAuthority(guild.AUTH_ADD_MEMBER):
			if not guild.IsMemberByName(self.nameString):
				if 0 == chr.GetGuildID(self.vid):
					self.__ShowButton(localeinfo.TARGET_BUTTON_INVITE_GUILD)

		if not messenger.IsFriendByName(self.nameString):
			self.__ShowButton(localeinfo.TARGET_BUTTON_FRIEND)

		if player.IsPartyMember(self.vid):

			self.__HideButton(localeinfo.TARGET_BUTTON_FIGHT)

			if player.IsPartyLeader(self.vid):
				self.__ShowButton(localeinfo.TARGET_BUTTON_LEAVE_PARTY)
			elif player.IsPartyLeader(player.GetMainCharacterIndex()):
				self.__ShowButton(localeinfo.TARGET_BUTTON_EXCLUDE)

		else:
			if player.IsPartyMember(player.GetMainCharacterIndex()):
				if player.IsPartyLeader(player.GetMainCharacterIndex()):
					self.__ShowButton(localeinfo.TARGET_BUTTON_INVITE_PARTY)
			else:
				if chr.IsPartyMember(self.vid):
					self.__ShowButton(localeinfo.TARGET_BUTTON_REQUEST_ENTER_PARTY)
				else:
					self.__ShowButton(localeinfo.TARGET_BUTTON_INVITE_PARTY)

			if player.IsRevengeInstance(self.vid):
				self.__HideButton(localeinfo.TARGET_BUTTON_FIGHT)
				self.__ShowButton(localeinfo.TARGET_BUTTON_AVENGE)
			elif player.IsChallengeInstance(self.vid):
				self.__HideButton(localeinfo.TARGET_BUTTON_FIGHT)
				self.__ShowButton(localeinfo.TARGET_BUTTON_ACCEPT_FIGHT)
			elif player.IsCantFightInstance(self.vid):
				self.__HideButton(localeinfo.TARGET_BUTTON_FIGHT)

			#if not player.IsSameEmpire(self.vid):
				#self.__HideButton(localeinfo.TARGET_BUTTON_INVITE_PARTY)
				#self.__HideButton(localeinfo.TARGET_BUTTON_FRIEND)
				#self.__HideButton(localeinfo.TARGET_BUTTON_FIGHT)


		distance = player.GetCharacterDistance(self.vid)
		if distance > self.EXCHANGE_LIMIT_RANGE:
			self.__HideButton(localeinfo.TARGET_BUTTON_EXCHANGE)
			self.__ArrangeButtonPosition()
		
		self.__ArrangeButtonPosition()

	def SetSizeNew(self, width, height):
		showingButtonCount = len(self.showingButtonList)
		nameLen = len(self.GetTargetName())
		if showingButtonCount == 1 and nameLen >= 8 and self.showingButtonList[0].GetText() == localeinfo.TARGET_BUTTON_DISMOUNT:
			width += nameLen * 2
		
		self.SetSize(width, height)

	def __ArrangeButtonPosition(self):
		showingButtonCount = len(self.showingButtonList)

		pos = -(showingButtonCount / 2) * 68
		if 0 == showingButtonCount % 2:
			pos += 34

		for button in self.showingButtonList:
			button.SetPosition(pos, 33)
			pos += 68
		
		if showingButtonCount == 1 and self.showingButtonList[0].GetText() == localeinfo.TARGET_BUTTON_DISMOUNT:
			showingButtonCount += 1
		
		if app.ENABLE_VIEW_TARGET_PLAYER_HP:
			width = max(250, showingButtonCount * 75)
			self.SetSizeNew(width, 65)
		else:
			self.SetSizeNew(max(150, showingButtonCount * 75), 65)
		
		self.UpdatePosition()

	def OnUpdate(self):
		if self.isShowButton:
			exchangeButton = self.buttonDict[localeinfo.TARGET_BUTTON_EXCHANGE]
			distance = player.GetCharacterDistance(self.vid)
			if distance < 0:
				return
			
			if exchangeButton.IsShow():
				if distance > self.EXCHANGE_LIMIT_RANGE:
					self.RefreshButton()
			else:
				if distance < self.EXCHANGE_LIMIT_RANGE:
					self.RefreshButton()

	if app.ENABLE_VIEW_ELEMENT:
		def SetElementImage(self, element):
			if element > 0 and element in ELEMENT_IMAGE_DIC.keys():
				self.elementImage = ui.ImageBox()
				self.elementImage.SetParent(self.name)
				self.elementImage.SetPosition(self.GetWidth() - 21, -13)
				self.elementImage.LoadImage("d:/ymir work/ui/game/12zi/element/%s.sub" % (ELEMENT_IMAGE_DIC[element]))
				self.elementImage.Show()
			elif element <= 0 and self.elementImage:
				self.elementImage.Hide()
				self.elementImage = None

