import ui
import app
import net
import chrmgr
import wndMgr

HIT = "d:/ymir work/ui/game/fishing/fishing_effect_hit.sub"
MISS = "d:/ymir work/ui/game/fishing/fishing_effect_miss.sub"

class FishingWindow(ui.ScriptWindow):
	DEBUG = False

	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.board = None
		self.guage = None
		self.goalText = None
		self.water = None
		self.fish = None
		self.need = 0
		self.lastUpdateTime = 0
		self.lastUpdateScale = 0
		self.limitTime = 0
		self.scallingProcess = 0.0
		self.recolored = 0
		self.isLoaded = 0
		self.closed = 0
		self.circle = None
		self.wasIn = False
		self.debugFishPos = None
		self.debugCirclePos = None
		self.debugMousePos = None
		self.effectImg = None
		self.effectProcess = 0
		self.effectWaterImg = None
		self.effectWaterProcess = 0
		self.effectX = 0
		self.effectY = 0
		self.success = False
		self.have = 0
		self.lastcatch = 0
		self.Load()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def Load(self):
		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "uiscript/fishinggamewindow.py")
		except:
			import exception
			exception.Abort("FishingWindow.Load.Object")
		
		try:
			self.board = self.GetChild("board")
			self.guage = self.GetChild("fishing_timer_gauge")
			self.goalText = self.GetChild("fishing_goal_count_text")
			self.water = self.GetChild("fishing_background_water")
			fish = ui.FishBox()
			fish.SetParent(self.water)
			fish.SetSize(30, 30)
			fish.SetPosition(0, 0)
			fish.Show()
			self.fish = fish
			self.circle = self.GetChild("fishing_goal_circle")
			self.debugFishPos = self.GetChild("debug_text_fish_pos")
			self.debugCirclePos = self.GetChild("debug_text_circle_pos")
			self.debugMousePos = self.GetChild("debug_text_mouse_pos")
			if not self.DEBUG:
				self.debugFishPos.Hide()
				self.debugCirclePos.Hide()
				self.debugMousePos.Hide()
			
			effect = ui.AniImageBox()
			effect.SetParent(self.board)
			effect.SetSize(30, 30)
			for i in xrange(4):
				effect.AppendImage("d:/ymir work/ui/game/fishing/wave/fishing_effect_wave_%d.sub" % (i + 1), 1.0, 1.0)
			(x, y) = self.fish.GetPosition()
			effect.SetPosition(0, 0)
			effect.Hide()
			self.effectWaterImg = effect
		except:
			import exception
			exception.Abort("FishingWindow.LoadObject.Bind")
		
		self.board.SetCloseEvent(ui.__mem_func__(self.Close))
		self.fish.SetEndMoveEvent(ui.__mem_func__(self.OnWaterEffect))
		self.fish.fishIcon.SetOnMouseLeftButtonUpEvent(ui.__mem_func__(self.OnClickFishArea))
		self.water.SetOnMouseLeftButtonUpEvent(ui.__mem_func__(self.OnClickWaterArea))
		self.isLoaded = 1

	def Destroy(self):
		self.ClearDictionary()
		self.board = None
		self.guage = None
		self.goalText = None
		self.water = None
		self.fish = None
		self.need = 0
		self.lastUpdateTime = 0
		self.lastUpdateScale = 0
		self.limitTime = 0
		self.scallingProcess = 0.0
		self.recolored = 0
		self.isLoaded = 0
		self.closed = 0
		self.circle = None
		self.wasIn = False
		self.debugFishPos = None
		self.debugCirclePos = None
		self.debugMousePos = None
		self.effectImg = None
		self.effectProcess = 0
		self.effectWaterImg = None
		self.effectWaterProcess = 0
		self.effectX = 0
		self.effectY = 0
		self.success = False
		self.have = 0
		self.lastcatch = 0

	def OnOpen(self, have, need):
		if self.isLoaded == 0:
			return
		
		if self.effectImg != None:
			self.effectImg.Hide()
			self.effectImg = None
			self.effectProcess = 0
		
		if self.effectWaterImg.IsShow():
			self.effectWaterImg.Hide()
		
		self.lastcatch = 0
		self.success = False
		self.have = 0
		self.need = need
		self.goalText.SetText("%d / %d" % (have, need))
		self.closed = 0
		self.SetCenterPosition()
		self.Show()
		self.lastUpdateTime = float(app.GetTime())
		self.lastUpdateScale = float(app.GetTime())
		self.scallingProcess = 1.0
		self.recolored = 0
		self.guage.SetImageDiffuseColor(1.0, 1.0, 1.0, 1.0)
		self.fish.MoveStart()

	def Close(self):
		if self.closed == 1:
			return
		
		if self.IsShow():
			net.StopFishingNew()
		
		self.closed = 1

	def OnClose(self):
		app.SetCursor(app.NORMAL)
		self.fish.MoveStop()
		self.Hide()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def OnUpdate(self):
		if self.IsShow():
			if self.effectImg != None:
				if self.effectProcess == 2 or self.effectProcess == 4 or self.effectProcess == 6:
					self.effectX -= 1
					self.effectY -= 1
					self.effectImg.SetPosition(self.effectX, self.effectY)
				self.effectProcess += 1
				if self.effectProcess >= 8:
					self.effectImg.Hide()
					self.effectImg = None
					self.effectProcess = 0
			
			if self.effectWaterImg.IsShow():
				self.effectWaterProcess += 1
				if self.effectWaterProcess > 10:
					self.effectWaterImg.Hide()
					self.effectWaterProcess = 0
			
			if self.water.IsIn() or self.fish.fishIcon.IsIn() or self.effectWaterImg.IsIn():
				app.SetCursor(app.FISH_CATCH)
			else:
				app.SetCursor(app.NORMAL)
			
			(xFish, yFish) = self.fish.GetPosition()
			if xFish >= 74 and xFish <= 175 and yFish >= 50 and yFish <= 141:
				if self.wasIn == False:
					self.circle.SetImageDiffuseColor(255.0 / 255.0, 173.0 / 255.0, 209.0 / 255.0, 0.8)
					self.wasIn = True
			else:
				if self.wasIn == True:
					self.circle.SetImageDiffuseColor(1.0, 1.0, 1.0, 1.0)
					self.wasIn = False
			
			if self.DEBUG:
				self.debugFishPos.SetText("Fish: %d, %d" % (xFish, yFish))
				(xMouse, yMouse) = self.GetMouseLocalPosition()
				self.debugMousePos.SetText("Mouse: %d, %d" % (xMouse, yMouse))
			
			d = float(app.GetTime()) - float(self.lastUpdateScale)
			if d >= 0.066:
				if self.scallingProcess >= 0.0:
					self.guage.SetPercentage(self.scallingProcess, 1.0)
				
				self.scallingProcess -= 0.005
				if self.scallingProcess <= 0.30 and self.recolored == 0:
					self.guage.SetImageDiffuseColor(1.0, 0.0, 0.0, 1.0)
					self.recolored = 1
				
				self.lastUpdateScale = float(app.GetTime())
			elif self.scallingProcess <= 0.30 and self.recolored == 0:
				self.guage.SetImageDiffuseColor(1.0, 0.0, 0.0, 1.0)
				self.recolored = 1

	def OnCatch(self, have):
		self.goalText.SetText("%d / %d" % (have, self.need))
		self.have = have

	def OnWaterEffect(self):
		if self.effectWaterImg != None:
			if self.effectWaterImg.IsShow():
				self.effectWaterImg.Hide()
			
			(x, y) = self.fish.GetPosition()
			self.effectWaterImg.SetPosition(x, y)
			self.effectWaterProcess = 0
			self.effectWaterImg.Show()

	def CreateEffect(self, e):
		(x, y) = self.GetMouseLocalPosition()
		x -= 5
		y -= 30
		
		if self.effectImg != None:
			self.effectImg.Hide()
			self.effectImg = None
			self.effectProcess = 0
		
		self.effectX = x
		self.effectY = y
		
		imgs = [HIT, MISS,]
		effect = ui.ImageBox()
		effect.SetParent(self.board)
		effect.LoadImage(imgs[e])
		effect.SetPosition(self.effectX, self.effectY)
		effect.Show()
		self.effectImg = effect

	def OnClickFishArea(self):
		if self.success == True:
			return
		
		if self.lastcatch > app.GetTime():
			return
		
		self.lastcatch = app.GetTime() + 1
		
		(xMouse, yMouse) = wndMgr.GetMousePosition()
		if self.wasIn == True:
			if self.have == self.need - 1:
				self.success = True
			
			self.CreateEffect(0)
			net.CatchFishingNew()
		else:
			self.CreateEffect(1)
			net.CatchFishingFailed()
		
		return True

	def OnClickWaterArea(self):
		if self.success == True:
			return
		
		(xMouse, yMouse) = wndMgr.GetMousePosition()
		self.CreateEffect(1)
		net.CatchFishingFailed()
		return True

	def OnCatchFailed(self):
		self.scallingProcess -= 0.060
		self.guage.SetPercentage(self.scallingProcess, 1.0)
		if self.scallingProcess <= 0.30 and self.recolored == 0:
			self.guage.SetImageDiffuseColor(1.0, 0.0, 0.0, 1.0)
			self.recolored = 1

