import app
import dbg
import wndMgr
import systemSetting
import mousemodule
import networkmodule
import uicandidate
import constinfo
import musicinfo
import stringcommander
import localeinfo
import playersettingmodule

#bind_me(locals().values())

if constinfo.ENABLE_PASTE_FEATURE:
	from ui import EnablePaste as ui_EnablePaste
	ui_EnablePaste(True)

def RunApp():
	musicinfo.LoadLastPlayFieldMusic()

	app.SetHairColorEnable(constinfo.HAIR_COLOR_ENABLE)
	app.SetArmorSpecularEnable(constinfo.ARMOR_SPECULAR_ENABLE)
	app.SetWeaponSpecularEnable(constinfo.WEAPON_SPECULAR_ENABLE)

	app.SetMouseHandler(mousemodule.mouseController)
	wndMgr.SetMouseHandler(mousemodule.mouseController)
	wndMgr.SetScreenSize(systemSetting.GetWidth(), systemSetting.GetHeight())

	try:
		app.Create(localeinfo.APP_TITLE, systemSetting.GetWidth(), systemSetting.GetHeight(), 1)
	except RuntimeError, msg:
		msg = str(msg)
		if "CREATE_DEVICE" == msg:
			dbg.LogBox("Sorry, Your system does not support 3D graphics,\r\nplease check your hardware and system configeration\r\nthen try again.")
		else:
			dbg.LogBox("Metin2.%s" % msg)
		return
	
	app.SetCamera(1500.0, 30.0, 0.0, 180.0)
	playersettingmodule.LoadGameData("INIT")
	playersettingmodule.LoadGameData("SOUND")
	playersettingmodule.LoadGameData("EFFECT")
	playersettingmodule.LoadGameData("NPC")
	#Gets and sets the floating-point control word
	#app.SetControlFP()

	if not mousemodule.mouseController.Create():
		return

	mainStream = networkmodule.MainStream()
	mainStream.Create()

	#mainStream.SetLoadingPhase()
	#mainStream.SetLogoPhase()
	
	mainStream.SetLoginPhase()
	#mainStream.SetSelectCharacterPhase()
	#mainStream.SetCreateCharacterPhase()
	#mainStream.SetSelectEmpirePhase()
	#mainStream.SetGamePhase()
	
	app.Loop()
	mainStream.Destroy()

RunApp()

