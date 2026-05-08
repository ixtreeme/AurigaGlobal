import app
import item
import nonplayer
import ui
import ingamewikiui
import wiki
import wndMgr
import grp


try:
	import dbg
	dbg.TraceError("[RZ] ingamewiki.py LOADED")
except:
	pass
class InGameWiki(ui.Window):
	def __init__(self):
		self.searchEdit = None
		self.chestLootPopup = None
		
		ui.Window.__init__(self)
		
		self.SetWindowName("InGameWiki")
		wiki.RegisterClass(self)
		
		self.objList = {}
		self.windowHistory = []
		self.currSelected = 0
		
		self.BuildUI()
		self.SetCenterPosition()
		self.Hide()
	
	def __del__(self):
		wiki.UnregisterClass()
		
		ui.Window.__del__(self)
	
	def Show(self):
		wndMgr.Show(self.hWnd)
		wiki.ShowModelViewManager(True)
	
	def Hide(self):
		wndMgr.Hide(self.hWnd)
		wiki.ShowModelViewManager(False)
		
		if self.searchEdit:
			self.searchEdit.KillFocus()
	
	def Close(self):
		self.Hide()
	
	def OnPressEscapeKey(self):
		self.Close()
		return True
	
	def BINARY_LoadInfo(self, objID, vnum):
		# Kesoi erkezo wiki válasz esetén elofordulhat, hogy a cél UI ablak már megszunt.
		# Ilyenkor a weakref proxy ReferenceError-t dobna -> ezt el kell nyelni es ki kell takaritani.
		try:
			obj = self.objList.get(objID)
			if obj:
				try:
					obj.NoticeMe()
					return
				except ReferenceError:
					# stale / dead proxy
					try:
						self.objList.pop(objID, None)
					except:
						pass
				except:
					pass

			# Fallback: ha a chest loot popup nyitva van és ehhez a vnumhoz tartozik, frissítsük azt is.
			try:
				if self.chestLootPopup and getattr(self.chestLootPopup, "vnum", 0) == vnum:
					self.chestLootPopup.NoticeMe()
			except ReferenceError:
				self.chestLootPopup = None
			except:
				pass

			# Takaritas: idonkent seprojuk ki a dead weakref-eket.
			try:
				for k, o in list(self.objList.items()):
					try:
						o.NoticeMe  # attribútum elérés is dobhat ReferenceError-t
					except ReferenceError:
						self.objList.pop(k, None)
			except:
				pass
		except:
			pass
	
	def BuildUI(self):
		ingamewikiui.InitMainWindow(self)
		ingamewikiui.BuildBaseMain(self)
	

	def OpenChestLootPopup(self, chestVnum):
		# Wiki foablak megnyitasa NELKUL csak a lada loot popup.
		try:
			vnum = int(chestVnum)
		except:
			vnum = 0
		if not vnum:
			return
	
		try:
			# *** JAVÍTÁS: Töröljük a régi popupot és mindig újat hozunk létre ***
			if self.chestLootPopup:
				try:
					self.chestLootPopup.Close()
					self.chestLootPopup = None
				except:
					pass
			
			# Új popup létrehozása
			self.chestLootPopup = ingamewikiui.WikiChestLootPopup(None)
			self.chestLootPopup.Open(vnum)
			self.chestLootPopup.SetTop()
		except:
			pass
	

	def OpenSpecialPage(self, oldWindow, vnum, isMonster = False):
		del self.windowHistory[self.currSelected + 1:]
		
		try:
			if oldWindow:
				del self.windowHistory[:]
				
				self.currSelected = 0
				self.windowHistory.append(oldWindow)
			
			if len(self.windowHistory) > 0:
				self.windowHistory[-1].Hide()
			
			newSpec = ingamewikiui.SpecialPageWindow(vnum, isMonster)
			newSpec.AddFlag("attach")
			newSpec.SetParent(self)
			# kozepre a mainBoard-on belul
			mx, my = ingamewikiui.mainBoardPos
			mw, mh = ingamewikiui.mainBoardSize
			sw, sh = newSpec.GetWidth(), newSpec.GetHeight()
			
			x = mx + int((mw - sw) / 2)
			y = my + int((mh - sh) / 2)
			
			newSpec.SetPosition(x, y)

			newSpec.Show()
			
			self.windowHistory.append(newSpec)
			self.currSelected = self.windowHistory.index(newSpec)
		except ReferenceError:
			pass
	
	def OnPressNameEscapeKey(self):
		if not self.searchEdit:
			return
		
		if not self.searchEdit.IsShowCursor() or self.searchEdit.GetText() == "":
			self.OnPressEscapeKey()
		else:
			self.searchEdit.SetText("")
			self.searchEditHint.SetText("")
	
	def Search_RefreshTextHint(self):
		EDIT_TEXT_BASE_COLOR = grp.GenerateColor(0.8549, 0.8549, 0.8549, 1.0)
		EDIT_TEXT_NOT_FOUND_COLOR = grp.GenerateColor(1.0, 0.2, 0.2, 1.0)
		
		self.searchEditHint.SetText("")
		self.searchEdit.SetPackedFontColor(EDIT_TEXT_BASE_COLOR)
		
		search_text = self.searchEdit.GetText()
		
		if len(search_text):
			(hintName, vnum) = item.GetItemDataByNamePart(search_text)
			if vnum == -1:
				(hintName, vnum) = nonplayer.GetMonsterDataByNamePart(search_text)
				if vnum == -1:
					self.searchEditHint.SetText("")
					self.searchEdit.SetPackedFontColor(EDIT_TEXT_NOT_FOUND_COLOR)
				else:
					self.searchEditHint.SetText(search_text + " " + hintName[len(search_text):])
			else:
				self.searchEditHint.SetText(search_text + " " + hintName[len(search_text):])
	
	def OnUpdate(self):
		(start, end) = self.searchEdit.GetRenderPos()
		self.searchEditHint.SetFixedRenderPos(start, end) if start else self.searchEditHint.SetFixedRenderPos(start, 17)
	
	def Search_CompleteTextSearch(self):
		if self.searchEditHint.GetText():
			oldText = self.searchEdit.GetText()
			self.searchEdit.SetText(oldText + self.searchEditHint.GetText()[len(oldText)+1:])
			self.searchEdit.SetEndPosition()
			self.Search_RefreshTextHint()
	
	def StartSearch(self):
		
		def check_exact_search(real_name, check_name):
			if not self.exactSearch.GetCheckStatus():
				return True
			
			if real_name.lower() != check_name.lower():
				return False
			
			return True
		
		search_text = self.searchEdit.GetText()
		if len(search_text):
			(search_name, search_vnum) = item.GetItemDataByNamePart(search_text)
			if search_vnum == -1 or not check_exact_search(search_name, search_text):
				(search_name, search_vnum) = nonplayer.GetMonsterDataByNamePart(search_text)
				if search_vnum == -1 or not check_exact_search(search_name, search_text):
					return
				
				self.CloseBaseWindows()
				self.OpenSpecialPage(None, search_vnum, True)
			else:
				self.CloseBaseWindows()
				
				item.SelectItem(search_vnum)
				
				# Mount itemeknél NE normalizáljunk refine vnumot, hanem az eredeti vnumot nyissuk.
				# Refine normalizálás csak WEAPON/ARMOR esetén legyen, különben sok tárgy (pl. mount pecsétek) elcsúszik.
				isMountItem = False
				isRefineItem = False
				try:
					itemType = item.GetItemType()
					subType = item.GetItemSubType()
					isRefineItem = itemType in (item.ITEM_TYPE_WEAPON, item.ITEM_TYPE_ARMOR)
					
					if itemType == item.ITEM_TYPE_COSTUME and subType in (2, 7):
						# mount costume (több rendszer)
						isMountItem = True
					
					# config listák alapján is mountnak tekintjük (nem minden mount item COSTUME típus)
					if not isMountItem and hasattr(ingamewikiconfig, "COSTUME_MOUNT_VNUMS") and search_vnum in ingamewikiconfig.COSTUME_MOUNT_VNUMS:
						isMountItem = True
					if not isMountItem and hasattr(ingamewikiconfig, "COSTUME_MOUNT_VNUMS_COSTUMES") and search_vnum in ingamewikiconfig.COSTUME_MOUNT_VNUMS_COSTUMES:
						isMountItem = True
				except:
					isMountItem = False
					isRefineItem = False
				
				if isMountItem:
					checkedVnum = search_vnum
				elif isRefineItem and wiki.CanIncrRefineLevel():
					startRefineVnum = wiki.GetWikiItemStartRefineVnum(search_vnum)
					checkedVnum = startRefineVnum if startRefineVnum != 0 else (int(search_vnum / 10) * 10)
				else:
					checkedVnum = search_vnum
				self.OpenSpecialPage(None, checkedVnum, False)
	
	def GoToLanding(self):
		self.CloseBaseWindows()
		self.categ.NotifyCategorySelect(None)
		self.customPageWindow.LoadFile("landingpage.txt")

	def CloseBaseWindows(self):
		self.mainWeaponWindow.Hide()
		self.mainChestWindow.Hide()
		self.mainBossWindow.Hide()
		self.customPageWindow.Hide()
		self.costumePageWindow.Hide()
		
		del self.windowHistory[:]
		self.currSelected = 0
