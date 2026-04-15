import app
import net
import chr
import player
import ui
import item
import skill
import uitooltip
import math
import wndMgr
import localeinfo

if app.ENABLE_VOTE4BUFF:
	voteBuffData = [0,0]

HEIGHT_WND = wndMgr.GetScreenHeight()

import uicommon
removeQuestionDialog = uicommon.QuestionDialog()
removeQuestionDialog.SetText("")
removeQuestionDialog.SetWidth(350)
removeQuestionDialog.Close()

def OnRemoveQuestionDialog(event, text):
	if removeQuestionDialog.IsShow():
		return
	
	removeQuestionDialog.SetText(text)
	removeQuestionDialog.SetAcceptEvent(lambda arg1 = True, arg2 = event: OnCloseRemoveQuestionDialog(arg1, arg2))
	removeQuestionDialog.SetCancelEvent(lambda arg1 = False, arg2 = event: OnCloseRemoveQuestionDialog(arg1, arg2))
	removeQuestionDialog.Open()

def OnCloseRemoveQuestionDialog(answer, event):
	if not removeQuestionDialog:
		return False
	
	removeQuestionDialog.Close()
	if not answer:
		return False
	
	net.SendChatPacket(event)
	return True

# WEDDING
class LovePointImage(ui.ExpandedImageBox):

	FILE_PATH = "d:/ymir work/ui/pattern/LovePoint/"
	FILE_DICT = {
		0 : FILE_PATH + "01.dds",
		1 : FILE_PATH + "02.dds",
		2 : FILE_PATH + "02.dds",
		3 : FILE_PATH + "03.dds",
		4 : FILE_PATH + "04.dds",
		5 : FILE_PATH + "05.dds",
	}

	def __init__(self):
		ui.ExpandedImageBox.__init__(self)

		self.loverName = ""
		self.lovePoint = 0

		self.toolTip = uitooltip.ToolTip(100)
		self.toolTip.HideToolTip()

	def __del__(self):
		ui.ExpandedImageBox.__del__(self)

	def SetLoverInfo(self, name, lovePoint):
		self.loverName = name
		self.lovePoint = lovePoint
		self.__Refresh()

	def OnUpdateLovePoint(self, lovePoint):
		self.lovePoint = lovePoint
		self.__Refresh()

	def __Refresh(self):
		self.lovePoint = max(0, self.lovePoint)
		self.lovePoint = min(100, self.lovePoint)

		if 0 == self.lovePoint:
			loveGrade = 0
		else:
			loveGrade = self.lovePoint / 25 + 1
		fileName = self.FILE_DICT.get(loveGrade, self.FILE_PATH+"00.dds")

		try:
			self.LoadImage(fileName)
		except:
			import dbg
			dbg.TraceError("LovePointImage.SetLoverInfo(lovePoint=%d) - LoadError %s" % (self.lovePoint, fileName))

		self.SetScale(0.7, 0.7)

		self.toolTip.ClearToolTip()
		self.toolTip.SetTitle(self.loverName)
		self.toolTip.AppendTextLine(localeinfo.AFF_LOVE_POINT % (self.lovePoint))
		self.toolTip.ResizeToolTip()

	def OnMouseOverIn(self):
		self.toolTip.ShowToolTip()

		if app.ENABLE_RENEWAL_AFFECT_SHOWER:
			self.SetScale(0.9, 0.9)

	def OnMouseOverOut(self):
		self.toolTip.HideToolTip()

		if app.ENABLE_RENEWAL_AFFECT_SHOWER:
			self.SetScale(0.7, 0.7)
# END_OF_WEDDING


class HorseImage(ui.ExpandedImageBox):

	FILE_PATH = "d:/ymir work/ui/pattern/HorseState/"

	FILE_DICT = {
		00 : FILE_PATH+"00.dds",
		01 : FILE_PATH+"00.dds",
		02 : FILE_PATH+"00.dds",
		03 : FILE_PATH+"00.dds",
		10 : FILE_PATH+"10.dds",
		11 : FILE_PATH+"11.dds",
		12 : FILE_PATH+"12.dds",
		13 : FILE_PATH+"13.dds",
		20 : FILE_PATH+"20.dds",
		21 : FILE_PATH+"21.dds",
		22 : FILE_PATH+"22.dds",
		23 : FILE_PATH+"23.dds",
		30 : FILE_PATH+"30.dds",
		31 : FILE_PATH+"31.dds",
		32 : FILE_PATH+"32.dds",
		33 : FILE_PATH+"33.dds",
	}

	def __init__(self):
		ui.ExpandedImageBox.__init__(self)

		#self.textLineList = []
		self.toolTip = uitooltip.ToolTip(100)
		self.toolTip.HideToolTip()

	def __GetHorseGrade(self, level):
		if 0 == level:
			return 0

		return (level-1)/10 + 1

	def SetState(self, level, health, battery):
		#self.textLineList=[]
		self.toolTip.ClearToolTip()

		if level>0:

			try:
				grade = self.__GetHorseGrade(level)
				self.__AppendText(localeinfo.LEVEL_LIST[grade])
			except IndexError:
				print "HorseImage.SetState(level=%d, health=%d, battery=%d) - Unknown Index" % (level, health, battery)
				return

			try:
				healthName=localeinfo.HEALTH_LIST[health]
				if len(healthName)>0:
					self.__AppendText(healthName)
			except IndexError:
				print "HorseImage.SetState(level=%d, health=%d, battery=%d) - Unknown Index" % (level, health, battery)
				return

			if health>0:
				if battery==0:
					self.__AppendText(localeinfo.NEEFD_REST)

			try:
				fileName=self.FILE_DICT[health*10+battery]
			except KeyError:
				print "HorseImage.SetState(level=%d, health=%d, battery=%d) - KeyError" % (level, health, battery)

			try:
				self.LoadImage(fileName)
			except:
				print "HorseImage.SetState(level=%d, health=%d, battery=%d) - LoadError %s" % (level, health, battery, fileName)

		self.SetScale(0.7, 0.7)

	def __AppendText(self, text):

		self.toolTip.AppendTextLine(text)
		self.toolTip.ResizeToolTip()

		#x=self.GetWidth()/2
		#textLine = ui.TextLine()
		#textLine.SetParent(self)
		#textLine.SetSize(0, 0)
		#textLine.SetOutline()
		#textLine.Hide()
		#textLine.SetPosition(x, 40+len(self.textLineList)*16)
		#textLine.SetText(text)
		#self.textLineList.append(textLine)

	def OnMouseOverIn(self):
		#for textLine in self.textLineList:
		#	textLine.Show()

		self.toolTip.ShowToolTip()

		if app.ENABLE_RENEWAL_AFFECT_SHOWER:
			self.SetScale(0.9, 0.9)

	def OnMouseOverOut(self):
		#for textLine in self.textLineList:
		#	textLine.Hide()

		self.toolTip.HideToolTip()

		if app.ENABLE_RENEWAL_AFFECT_SHOWER:
			self.SetScale(0.7, 0.7)


if not app.ENABLE_RENEWAL_AFFECT_SHOWER:
	# AUTO_POTION
	class AutoPotionImage(ui.ExpandedImageBox):

		FILE_PATH_HP = "d:/ymir work/ui/pattern/auto_hpgauge/"
		FILE_PATH_SP = "d:/ymir work/ui/pattern/auto_spgauge/"

		def __init__(self):
			ui.ExpandedImageBox.__init__(self)

			self.loverName = ""
			self.lovePoint = 0
			self.potionType = player.AUTO_POTION_TYPE_HP
			self.filePath = ""

			self.toolTip = uitooltip.ToolTip(100)
			self.toolTip.HideToolTip()

		def __del__(self):
			ui.ExpandedImageBox.__del__(self)

		def SetPotionType(self, type):
			self.potionType = type

			if player.AUTO_POTION_TYPE_HP == type:
				self.filePath = self.FILE_PATH_HP
			elif player.AUTO_POTION_TYPE_SP == type:
				self.filePath = self.FILE_PATH_SP


		def OnUpdateAutoPotionImage(self):
			self.__Refresh()

		def __Refresh(self):
			print "__Refresh"

			isActivated, currentAmount, totalAmount, slotIndex = player.GetAutoPotionInfo(self.potionType)

			amountPercent = (float(currentAmount) / totalAmount) * 100.0
			grade = math.ceil(amountPercent / 20)

			if 5.0 > amountPercent:
				grade = 0

			if 80.0 < amountPercent:
				grade = 4
				if 90.0 < amountPercent:
					grade = 5

			fmt = self.filePath + "%.2d.dds"
			fileName = fmt % grade

			print self.potionType, amountPercent, fileName

			try:
				self.LoadImage(fileName)
			except:
				import dbg
				dbg.TraceError("AutoPotionImage.__Refresh(potionType=%d) - LoadError %s" % (self.potionType, fileName))

			self.SetScale(0.7, 0.7)

			self.toolTip.ClearToolTip()

			if player.AUTO_POTION_TYPE_HP == type:
				self.toolTip.SetTitle(localeinfo.TOOLTIP_AUTO_POTION_HP)
			else:
				self.toolTip.SetTitle(localeinfo.TOOLTIP_AUTO_POTION_SP)

			self.toolTip.AppendTextLine(localeinfo.TOOLTIP_AUTO_POTION_REST	% (amountPercent))
			self.toolTip.ResizeToolTip()

		def OnMouseOverIn(self):
			self.toolTip.ShowToolTip()

		def OnMouseOverOut(self):
			self.toolTip.HideToolTip()
	# END_OF_AUTO_POTION

if app.ENABLE_RENEWAL_AFFECT_SHOWER:
	# Priority buckets (fill these if you want specific affects pinned to first/last)
	PRIORITY_FIRST = set()
	PRIORITY_LAST = set()

	class AffectImage(ui.ExpandedImageBox):
		NORMAL_COLOR = 0xffC5C7C4
		BONUS_COLOR = 0xff95A693
		WHITE_COLOR = 0xffffffff

		def __init__(self):
			ui.ExpandedImageBox.__init__(self)
			self.toolTip = uitooltip.ToolTip()
			try:
				self.toolTip.HideToolTip()
			except:
				try:
					self.toolTip.Hide()
				except:
					pass

			self.isToolTipVisible = False
			self.currentToolTipText = None

			self.skillIndex = None
			self.priority = 0
			self.Canberemoved = False
			self.isSkillAffect = True
			self.description = None
			self.endTime = 0
			self.affect = None

		def SetCanberemoved(self, Canberemoved):
			self.Canberemoved = Canberemoved

		def SetPriority(self, priority):
			self.priority = priority

		def GetPriority(self):
			return self.priority

		def SetAffect(self, affect):
			self.affect = affect

		def GetAffect(self):
			return self.affect

		def __TipClear(self):
			if not self.toolTip:
				return
			try:
				self.toolTip.ClearToolTip()
			except:
				pass

		def __TipAppend(self, text):
			if not self.toolTip:
				return
			# Most clients support AppendTextLine(text, color) but keep it safe
			try:
				self.toolTip.AppendTextLine(text, self.WHITE_COLOR)
			except:
				try:
					self.toolTip.AppendTextLine(text)
				except:
					pass

		def __TipAlignResize(self):
			if not self.toolTip:
				return
			try:
				self.toolTip.AlignHorizonalCenter()
			except:
				pass
			try:
				self.toolTip.ResizeToolTip()
			except:
				pass

		def __TipShow(self):
			if not self.toolTip:
				return
			try:
				self.toolTip.ShowToolTip()
			except:
				try:
					self.toolTip.Show()
				except:
					pass

		def __TipHide(self):
			if not self.toolTip:
				return
			try:
				self.toolTip.HideToolTip()
			except:
				try:
					self.toolTip.Hide()
				except:
					pass

		def SetToolTipText(self, text, *args):
			# args ignored for compatibility (old signature had x,y)
			if hasattr(self, "currentToolTipText") and self.currentToolTipText == text:
				return

			self.__TipClear()
			self.__TipAppend(text)

			if self.Canberemoved:
				try:
					self.__TipAppend(localeinfo.AFFECTSHOWER_REMOVE_TEXT)
				except:
					pass

			self.__TipAlignResize()
			self.currentToolTipText = text

		def SetDescription(self, description):
			self.description = description
			self.UpdateDescription()

		def SetDuration(self, duration):
			self.endTime = 0
			if duration > 0:
				self.endTime = app.GetGlobalTimeStamp() + duration
				self.UpdateDescription()

		def UpdateAutoPotionDescription(self):
			if not self.toolTip:
				return

			if self.affect == chr.NEW_AFFECT_AUTO_HP_RECOVERY:
				potionType = player.AUTO_POTION_TYPE_HP
				potionName = localeinfo.TOOLTIP_AUTO_POTION_HP
			else:
				potionType = player.AUTO_POTION_TYPE_SP
				potionName = localeinfo.TOOLTIP_AUTO_POTION_SP

			isActivated, currentAmount, totalAmount, slotIndex = player.GetAutoPotionInfo(potionType)

			try:
				amountPercent = (float(currentAmount) / totalAmount) * 100.0
			except:
				amountPercent = 100.0

			self.__TipClear()
			try:
				self.toolTip.SetTitle(potionName)
			except:
				self.__TipAppend(potionName)
			try:
				self.toolTip.AppendTextLine(self.description % amountPercent)
			except:
				self.__TipAppend(self.description % amountPercent)

			self.__TipAlignResize()

		def FormatTime(self, time):
			if time < 0:
				return "0s"

			(d, remainder) = divmod(time, 86400)
			(h, remainder) = divmod(remainder, 3600)
			(m, s) = divmod(remainder, 60)

			time_parts = [
				"{}d".format(d) if d > 0 else "",
				"{}h".format(h) if h > 0 else "",
				"{}m".format(m) if m > 0 else "",
				"{}s".format(s) if s > 0 else "",
			]
			return " ".join(filter(None, time_parts))

		def UpdateDescription(self):
			if not self.toolTip or not self.description:
				return

			self.__TipClear()
			self.__TipAppend(self.description)

			if self.endTime > 0:
				remaining_time = self.endTime - app.GetGlobalTimeStamp()
				leftTime = self.FormatTime(remaining_time)
				timeText = "(%s : %s)" % (localeinfo.LEFT_TIME, leftTime)
				try:
					self.toolTip.AppendTextLine(timeText)
				except:
					self.__TipAppend(timeText)

			if self.Canberemoved:
				try:
					self.__TipAppend(localeinfo.AFFECTSHOWER_REMOVE_TEXT)
				except:
					pass

			self.__TipAlignResize()

		def SetSkillIndex(self, skillIndex):
			self.skillIndex = skillIndex

		def UpdateSkillDescription(self):
			if self.skillIndex is None:
				return

			slotIndex = player.GetSkillSlotIndex(self.skillIndex)
			skillCurrentPercentage = player.GetSkillCurrentEfficientPercentage(slotIndex)

			self.__TipClear()

			skillName = skill.GetSkillName(self.skillIndex)
			try:
				self.toolTip.SetTitle(skillName)
			except:
				self.__TipAppend(skillName)

			for i in xrange(skill.GetSkillAffectDescriptionCount(self.skillIndex)):
				description = skill.GetSkillAffectDescription(self.skillIndex, i, skillCurrentPercentage)
				try:
					self.toolTip.AppendTextLine(description)
				except:
					self.__TipAppend(description)

			if self.endTime > 0:
				remaining_time = self.endTime - app.GetGlobalTimeStamp()
				leftTime = self.FormatTime(remaining_time)
				timeText = "(%s : %s)" % (localeinfo.LEFT_TIME, leftTime)
				try:
					self.toolTip.AppendTextLine(timeText)
				except:
					self.__TipAppend(timeText)

			if self.Canberemoved:
				try:
					self.__TipAppend(localeinfo.AFFECTSHOWER_REMOVE_TEXT)
				except:
					pass

			self.__TipAlignResize()

		def SetSkillAffectFlag(self, flag):
			self.isSkillAffect = flag

		def IsSkillAffect(self):
			return self.isSkillAffect

		def OnMouseOverIn(self):
			self.SetScale(0.9, 0.9)
			self.__TipShow()
			self.isToolTipVisible = True

		def OnMouseOverOut(self):
			self.SetScale(0.7, 0.7)
			self.__TipHide()
			self.isToolTipVisible = False

else:

	
	
	class AffectImage(ui.ExpandedImageBox):
		def __init__(self):
			ui.ExpandedImageBox.__init__(self)
			self.toolTip = None
			self.text = None
			self.isSkillAffect = True
			self.description = None
			self.endTime = 0
			self.affect = None
			self.isClocked = True
			self.infinite = False
	
		def SetAffect(self, affect):
			self.affect = affect
	
		def GetAffect(self):
			return self.affect
	
		def SetToolTipText(self, text, x = 0, y = -19):
			if not self.toolTip:
				self.toolTip = uitooltip.ToolTip()
			
			self.text = text
	
		def SetDescription(self, description, infinite = False):
			self.infinite = infinite
			self.description = description
	
		def SetDuration(self, duration):
			self.endTime = 0
			if duration > 0:
				if app.ENABLE_VOTE_FOR_BONUS and self.affect == chr.AFFECT_VOTEFORBONUS:
					self.endTime = duration
				else:
					self.endTime = app.GetGlobalTimeStamp() + duration
	
		def UpdateAutoPotionDescription(self):
			if self.infinite:
				self.SetToolTipText(self.description)
			else:
				potionType = 0
				if self.affect == chr.NEW_AFFECT_AUTO_HP_RECOVERY:
					potionType = player.AUTO_POTION_TYPE_HP
				else:
					potionType = player.AUTO_POTION_TYPE_SP
	
				isActivated, currentAmount, totalAmount, slotIndex = player.GetAutoPotionInfo(potionType)
	
				#print "UpdateAutoPotionDescription ", isActivated, currentAmount, totalAmount, slotIndex
	
				amountPercent = 0.0
	
				try:
					amountPercent = (float(currentAmount) / totalAmount) * 100.0
				except:
					amountPercent = 100.0
	
				self.SetToolTipText(self.description % amountPercent, 0, 40)
	
		def SetClock(self, isClocked):
			self.isClocked = isClocked
	
		def UpdateDescription(self):
			if not self.isClocked and not (app.ENABLE_VOTE_FOR_BONUS and self.affect == chr.AFFECT_VOTEFORBONUS):
				self.__UpdateDescription2()
				return
	
			if not self.description and not (app.ENABLE_VOTE_FOR_BONUS and self.affect == chr.AFFECT_VOTEFORBONUS):
				return
	
			toolTip = self.description
			if not self.infinite:
				if self.endTime > 0:
					leftTime = localeinfo.SecondToDHM(self.endTime - app.GetGlobalTimeStamp())
					toolTip += " (%s : %s)" % (localeinfo.LEFT_TIME, leftTime)
			
			self.SetToolTipText(toolTip, 0, 40)
	
		#������������ ������ �������� ������ ����
		def __UpdateDescription2(self):
			if not self.description:
				return
			
			toolTip = self.description
			self.SetToolTipText(toolTip, 0, 40)
	
		def SetSkillAffectFlag(self, flag):
			self.isSkillAffect = flag
	
		def IsSkillAffect(self):
			return self.isSkillAffect
	
		def OnMouseOverIn(self):
			if app.ENABLE_VOTE4BUFF:
				if self.GetAffect() == chr.NEW_AFFECT_VOTE4BUFF:
					global voteBuffData
					interface = constInfo.GetInterfaceInstance()
					if interface:
						if interface.tooltipItem:
							interface.tooltipItem.ClearToolTip()
							interface.tooltipItem.AutoAppendNewTextLineResize(self.description)
							interface.tooltipItem.AppendSpace(5)
							interface.tooltipItem.AutoAppendNewTextLineResize(uiToolTip.ItemToolTip.POINT_DICT[voteBuffData[0]](voteBuffData[1]))
							interface.tooltipItem.AppendSpace(5)
							if self.endTime > 0:
								time = self.endTime - app.GetGlobalTimeStamp()
								day = int(int((time / 60) / 60) / 24)
								if day > 30:
									leftTime = uiScriptLocale.FOREVER
								else:
									leftTime = localeInfo.SecondToDHM(self.endTime - app.GetGlobalTimeStamp())
								interface.tooltipItem.AutoAppendNewTextLineResize(" (%s : %s)" % (localeInfo.LEFT_TIME, leftTime))
							interface.tooltipItem.ShowToolTip()
					return
	
			if self.toolTip:
				if app.ENABLE_VOTE4BUFF:
					if self.GetAffect() == chr.NEW_AFFECT_VOTE4BUFF:
						interface = constInfo.GetInterfaceInstance()
						if interface:
							if interface.tooltipItem:
								interface.tooltipItem.HideToolTip()
						return
	
				if not self.text:
					return
				
				self.toolTip.ClearToolTip()
				self.toolTip.AppendDescriptionEqual(self.text)
				self.toolTip.Show()
	
		def OnMouseOverOut(self):
			if self.toolTip:
				self.toolTip.Hide()
	
		def OnMouseLeftButtonUp(self):
			if not self.affect:
				return False
			
			if self.affect == chr.AFFECT_JEONGWI or\
				self.affect == chr.AFFECT_GEOMGYEONG or\
				self.affect == chr.AFFECT_CHEONGEUN or\
				self.affect == chr.AFFECT_GYEONGGONG or\
				self.affect == chr.AFFECT_GWIGEOM or\
				self.affect == chr.AFFECT_GONGPO or\
				self.affect == chr.AFFECT_JUMAGAP or\
				self.affect == chr.AFFECT_MUYEONG or\
				self.affect == chr.AFFECT_HEUKSIN or\
				self.affect == chr.AFFECT_HOSIN or\
				self.affect == chr.AFFECT_BOHO or\
				self.affect == chr.AFFECT_GICHEON or\
				self.affect == chr.AFFECT_KWAESOK or\
				self.affect == chr.AFFECT_JEUNGRYEOK:
				OnRemoveQuestionDialog("/remove_affect " + str(self.affect + 1), localeinfo.AFFECT_SKILL_QUESTION)
				return True
			
			if app.ENABLE_BLOCK_MULTIFARM:
				if self.affect == chr.AFFECT_DROP_UNBLOCK:
					OnRemoveQuestionDialog("/drop_block", localeinfo.AFFECT_DROP_QUESTION1)
					return True
				elif self.affect == chr.AFFECT_DROP_BLOCK:
					OnRemoveQuestionDialog("/drop_unblock", localeinfo.AFFECT_DROP_QUESTION2)
					return True
			
			return False
	
class AffectShower(ui.Window):

	MALL_DESC_IDX_START = 1000
	if app.ENABLE_RENEWAL_AFFECT_SHOWER:
		# indexes used for ordering (optional systems)
		WATER_DESC_IDX_START = 1200
		DEW_DESC_IDX_START = 1400
		DEW_RANGE_OFFSET = 200
		TOOLTIP_UPDATE_INTERVAL = 500
		MAX_ITEMS_PER_ROW = 10
	IMAGE_STEP = 25
	AFFECT_MAX_NUM = 32

	INFINITE_AFFECT_DURATION = 0x1FFFFFFF

	AFFECT_DATA_DICT =	{
			chr.AFFECT_POISON : (localeinfo.SKILL_TOXICDIE, "d:/ymir work/ui/skill/common/affect/poison.sub"),
			chr.AFFECT_SLOW : (localeinfo.SKILL_SLOW, "d:/ymir work/ui/skill/common/affect/slow.sub"),
			chr.AFFECT_STUN : (localeinfo.SKILL_STUN, "d:/ymir work/ui/skill/common/affect/stun.sub"),

			chr.AFFECT_ATT_SPEED_POTION : (localeinfo.SKILL_INC_ATKSPD, "d:/ymir work/ui/skill/common/affect/Increase_Attack_Speed.sub"),
			chr.AFFECT_MOV_SPEED_POTION : (localeinfo.SKILL_INC_MOVSPD, "d:/ymir work/ui/skill/common/affect/Increase_Move_Speed.sub"),
			chr.AFFECT_FISH_MIND : (localeinfo.SKILL_FISHMIND, "d:/ymir work/ui/skill/common/affect/fishmind.sub"),

			chr.AFFECT_JEONGWI : (localeinfo.SKILL_JEONGWI, "d:/ymir work/ui/skill/warrior/jeongwi_03.sub",),
			chr.AFFECT_GEOMGYEONG : (localeinfo.SKILL_GEOMGYEONG, "d:/ymir work/ui/skill/warrior/geomgyeong_03.sub",),
			chr.AFFECT_CHEONGEUN : (localeinfo.SKILL_CHEONGEUN, "d:/ymir work/ui/skill/warrior/cheongeun_03.sub",),
			chr.AFFECT_GYEONGGONG : (localeinfo.SKILL_GYEONGGONG, "d:/ymir work/ui/skill/assassin/gyeonggong_03.sub",),
			chr.AFFECT_EUNHYEONG : (localeinfo.SKILL_EUNHYEONG, "d:/ymir work/ui/skill/assassin/eunhyeong_03.sub",),
			chr.AFFECT_GWIGEOM : (localeinfo.SKILL_GWIGEOM, "d:/ymir work/ui/skill/sura/gwigeom_03.sub",),
			chr.AFFECT_GONGPO : (localeinfo.SKILL_GONGPO, "d:/ymir work/ui/skill/sura/gongpo_03.sub",),
			chr.AFFECT_JUMAGAP : (localeinfo.SKILL_JUMAGAP, "d:/ymir work/ui/skill/sura/jumagap_03.sub"),
			chr.AFFECT_HOSIN : (localeinfo.SKILL_HOSIN, "d:/ymir work/ui/skill/shaman/hosin_03.sub",),
			chr.AFFECT_BOHO : (localeinfo.SKILL_BOHO, "d:/ymir work/ui/skill/shaman/boho_03.sub",),
			chr.AFFECT_KWAESOK : (localeinfo.SKILL_KWAESOK, "d:/ymir work/ui/skill/shaman/kwaesok_03.sub",),
			chr.AFFECT_HEUKSIN : (localeinfo.SKILL_HEUKSIN, "d:/ymir work/ui/skill/sura/heuksin_03.sub",),
			chr.AFFECT_MUYEONG : (localeinfo.SKILL_MUYEONG, "d:/ymir work/ui/skill/sura/muyeong_03.sub",),
			chr.AFFECT_GICHEON : (localeinfo.SKILL_GICHEON, "d:/ymir work/ui/skill/shaman/gicheon_03.sub",),
			chr.AFFECT_JEUNGRYEOK : (localeinfo.SKILL_JEUNGRYEOK, "d:/ymir work/ui/skill/shaman/jeungryeok_03.sub",),
			chr.AFFECT_PABEOP : (localeinfo.SKILL_PABEOP, "d:/ymir work/ui/skill/sura/pabeop_03.sub",),
			chr.AFFECT_FALLEN_CHEONGEUN : (localeinfo.SKILL_CHEONGEUN, "d:/ymir work/ui/skill/warrior/cheongeun_03.sub",),
			28 : (localeinfo.SKILL_FIRE, "d:/ymir work/ui/skill/sura/hwayeom_03.sub",),
			chr.AFFECT_CHINA_FIREWORK : (localeinfo.SKILL_POWERFUL_STRIKE, "d:/ymir work/ui/skill/common/affect/powerfulstrike.sub",),

			#64 - END
			chr.NEW_AFFECT_EXP_BONUS : (localeinfo.TOOLTIP_MALL_EXPBONUS_STATIC, "d:/ymir work/ui/skill/common/affect/exp_bonus.sub",),

			chr.NEW_AFFECT_ITEM_BONUS : (localeinfo.TOOLTIP_MALL_ITEMBONUS_STATIC, "d:/ymir work/ui/skill/common/affect/item_bonus.sub",),
			chr.NEW_AFFECT_SAFEBOX : (localeinfo.TOOLTIP_MALL_SAFEBOX, "d:/ymir work/ui/skill/common/affect/safebox.sub",),
			chr.NEW_AFFECT_AUTOLOOT : (localeinfo.TOOLTIP_MALL_AUTOLOOT, "d:/ymir work/ui/skill/common/affect/autoloot.sub",),
			chr.NEW_AFFECT_FISH_MIND : (localeinfo.TOOLTIP_MALL_FISH_MIND, "d:/ymir work/ui/skill/common/affect/fishmind.sub",),
			chr.NEW_AFFECT_MARRIAGE_FAST : (localeinfo.TOOLTIP_MALL_MARRIAGE_FAST, "d:/ymir work/ui/skill/common/affect/marriage_fast.sub",),
			chr.NEW_AFFECT_GOLD_BONUS : (localeinfo.TOOLTIP_MALL_GOLDBONUS_STATIC, "d:/ymir work/ui/skill/common/affect/gold_bonus.sub",),

			chr.NEW_AFFECT_NO_DEATH_PENALTY : (localeinfo.TOOLTIP_APPLY_NO_DEATH_PENALTY, "d:/ymir work/ui/skill/common/affect/gold_premium.sub"),
			chr.NEW_AFFECT_SKILL_BOOK_BONUS : (localeinfo.TOOLTIP_APPLY_SKILL_BOOK_BONUS, "d:/ymir work/ui/skill/common/affect/gold_premium.sub"),
			chr.NEW_AFFECT_SKILL_BOOK_NO_DELAY : (localeinfo.TOOLTIP_APPLY_SKILL_BOOK_NO_DELAY, "d:/ymir work/ui/skill/common/affect/gold_premium.sub"),

			# �������� hp, sp
			chr.NEW_AFFECT_AUTO_HP_RECOVERY : (localeinfo.TOOLTIP_AUTO_POTION_REST, "d:/ymir work/ui/pattern/potions/72722.tga"),
			chr.NEW_AFFECT_AUTO_SP_RECOVERY : (localeinfo.TOOLTIP_AUTO_POTION_REST, "d:/ymir work/ui/pattern/potions/72727.tga"),

			MALL_DESC_IDX_START+player.POINT_MALL_ATTBONUS : (localeinfo.TOOLTIP_MALL_ATTBONUS_STATIC, "d:/ymir work/ui/skill/common/affect/att_bonus.sub",),
			MALL_DESC_IDX_START+player.POINT_MALL_DEFBONUS : (localeinfo.TOOLTIP_MALL_DEFBONUS_STATIC, "d:/ymir work/ui/skill/common/affect/def_bonus.sub",),
			MALL_DESC_IDX_START+player.POINT_MALL_EXPBONUS : (localeinfo.TOOLTIP_MALL_EXPBONUS, "d:/ymir work/ui/skill/common/affect/exp_bonus.sub",),
			MALL_DESC_IDX_START+player.POINT_MALL_ITEMBONUS : (localeinfo.TOOLTIP_MALL_ITEMBONUS, "d:/ymir work/ui/skill/common/affect/item_bonus.sub",),
			MALL_DESC_IDX_START+player.POINT_MALL_GOLDBONUS : (localeinfo.TOOLTIP_MALL_GOLDBONUS, "d:/ymir work/ui/skill/common/affect/gold_bonus.sub",),
			MALL_DESC_IDX_START+player.POINT_CRITICAL_PCT : (localeinfo.TOOLTIP_APPLY_CRITICAL_PCT,"d:/ymir work/ui/skill/common/affect/critical.sub"),
			MALL_DESC_IDX_START+player.POINT_PENETRATE_PCT : (localeinfo.TOOLTIP_APPLY_PENETRATE_PCT, "d:/ymir work/ui/skill/common/affect/gold_premium.sub"),
			MALL_DESC_IDX_START+player.POINT_MAX_HP_PCT : (localeinfo.TOOLTIP_MAX_HP_PCT, "d:/ymir work/ui/skill/common/affect/gold_premium.sub"),
			MALL_DESC_IDX_START+player.POINT_MAX_SP_PCT : (localeinfo.TOOLTIP_MAX_SP_PCT, "d:/ymir work/ui/skill/common/affect/gold_premium.sub"),

			MALL_DESC_IDX_START+player.POINT_PC_BANG_EXP_BONUS : (localeinfo.TOOLTIP_MALL_EXPBONUS_P_STATIC, "d:/ymir work/ui/skill/common/affect/EXP_Bonus_p_on.sub",),
			MALL_DESC_IDX_START+player.POINT_PC_BANG_DROP_BONUS: (localeinfo.TOOLTIP_MALL_ITEMBONUS_P_STATIC, "d:/ymir work/ui/skill/common/affect/Item_Bonus_p_on.sub",),

			MALL_DESC_IDX_START+player.ATT_SPEED : (localeinfo.TOOLTIP_ATT_SPEED, "d:/ymir work/ui/skill/common/affect/razor93.sub",),#green potion
			MALL_DESC_IDX_START+player.PVM : (localeinfo.TOOLTIP_APPLY_ATTBONUS_MEDI_PVM, "d:/ymir work/ui/skill/common/affect/razor93_pvm.sub",),#pvm



	}
	
	if app.__AUTO_QUQUE_ATTACK__:
		AFFECT_DATA_DICT[chr.AFFECT_AUTO_METIN_FARM] =  (localeinfo.NEW_AFFECT_AUTO_METIN_FARM, "icon/item/61400.tga")

	if app.ENABLE_VOTE4BUFF:
		AFFECT_DATA_DICT[chr.NEW_AFFECT_VOTE4BUFF] =  (localeinfo.NEW_AFFECT_VOTE4BUFF, "icon/affectshower/vote4buff.tga"),


	if app.NEW_PET_SYSTEM:
		AFFECT_PET_DATA_DICT = {
			5401 : (localeinfo.TOOLTIP_PETSKILL_1, "d:/ymir work/ui/skill/pet/jijoong.sub"),
			5402 : (localeinfo.TOOLTIP_PETSKILL_2, "d:/ymir work/ui/skill/pet/jijoong.sub"),
			5403 : (localeinfo.TOOLTIP_PETSKILL_3, "d:/ymir work/ui/skill/pet/jijoong.sub"),
			5404 : (localeinfo.TOOLTIP_PETSKILL_4, "d:/ymir work/ui/skill/pet/jijoong.sub"),
			5405 : (localeinfo.TOOLTIP_PETSKILL_5, "d:/ymir work/ui/skill/pet/banya.sub"),
			5406 : (localeinfo.TOOLTIP_PETSKILL_6, "d:/ymir work/ui/skill/pet/cheonryeong.sub"),
			5407 : (localeinfo.TOOLTIP_PETSKILL_7, "d:/ymir work/ui/skill/pet/choehoenbimu.sub"),
			5408 : (localeinfo.TOOLTIP_PETSKILL_8, "d:/ymir work/ui/skill/pet/stealmp.sub"),
			5409 : (localeinfo.TOOLTIP_PETSKILL_9, "d:/ymir work/ui/skill/pet/gold_drop.sub"),
			5410 : (localeinfo.TOOLTIP_PETSKILL_10, "d:/ymir work/ui/skill/pet/pacheon.sub"),
			5411 : (localeinfo.TOOLTIP_PETSKILL_11, "d:/ymir work/ui/skill/pet/block.sub"),
			5412 : (localeinfo.TOOLTIP_PETSKILL_12, "d:/ymir work/ui/skill/pet/reflect_melee.sub"),
		}
	
	if app.ENABLE_DRAGON_SOUL_SYSTEM:
		# ������ ��, �� ��.
		AFFECT_DATA_DICT[chr.NEW_AFFECT_DRAGON_SOUL_DECK1] = (localeinfo.TOOLTIP_DRAGON_SOUL_DECK1, "d:/ymir work/ui/dragonsoul/buff_ds_sky1.tga")
		AFFECT_DATA_DICT[chr.NEW_AFFECT_DRAGON_SOUL_DECK2] = (localeinfo.TOOLTIP_DRAGON_SOUL_DECK2, "d:/ymir work/ui/dragonsoul/buff_ds_land1.tga")
	

	if app.ENABLE_WOLFMAN_CHARACTER:
		AFFECT_DATA_DICT[chr.AFFECT_BLEEDING] = (localeinfo.SKILL_BLEEDING, "d:/ymir work/ui/skill/common/affect/poison.sub")
		AFFECT_DATA_DICT[chr.AFFECT_RED_POSSESSION] = (localeinfo.SKILL_GWIGEOM, "d:/ymir work/ui/skill/wolfman/red_possession_03.sub")
		AFFECT_DATA_DICT[chr.AFFECT_BLUE_POSSESSION] = (localeinfo.SKILL_CHEONGEUN, "d:/ymir work/ui/skill/wolfman/blue_possession_03.sub")

	if app.ENABLE_RENEWAL_AFFECT_SHOWER:
		AFFECT_DATA_DICT[chr.NEW_AFFECT_POLYMORPH] = (localeinfo.POLYMORPH_AFFECT_TOOLTIP, "icon/item/70104.tga")

	if app.ENABLE_BLOCK_MULTIFARM:
		AFFECT_DATA_DICT[chr.AFFECT_DROP_BLOCK] = (localeinfo.TOOLTIP_DROP_BLOCK, "d:/ymir work/ui/pattern/antifarm/no.dds")
		AFFECT_DATA_DICT[chr.AFFECT_DROP_UNBLOCK] = (localeinfo.TOOLTIP_DROP_UNBLOCK, "d:/ymir work/ui/pattern/antifarm/yes.dds")
	
	if app.ENABLE_RUNE_SYSTEM:
		AFFECT_DATA_DICT[chr.AFFECT_RUNE1] = (localeinfo.RUNE_ACTIVATED_1, "d:/ymir work/ui/pattern/rune/1.tga")
		AFFECT_DATA_DICT[chr.AFFECT_RUNE2] = (localeinfo.RUNE_ACTIVATED_2, "d:/ymir work/ui/pattern/rune/2.tga")
	
	if app.ENABLE_NEW_USE_POTION:
		AFFECT_DATA_DICT[chr.NEW_AFFECT_AUTO_HP_RECOVERY2] = (localeinfo.TOOLTIP_AUTO_POTION_REST, "d:/ymir work/ui/pattern/potions/72726.tga")
		AFFECT_DATA_DICT[chr.NEW_AFFECT_AUTO_SP_RECOVERY2] = (localeinfo.TOOLTIP_AUTO_POTION_REST, "d:/ymir work/ui/pattern/potions/72730.tga")
		AFFECT_DATA_DICT[chr.AFFECT_NEW_POTION1] = (localeinfo.AFFECT_NEW_POTION1, "d:/ymir work/ui/pattern/potions/88910.tga")
		AFFECT_DATA_DICT[chr.AFFECT_NEW_POTION2] = (localeinfo.AFFECT_NEW_POTION2, "d:/ymir work/ui/pattern/potions/88911.tga")
		AFFECT_DATA_DICT[chr.AFFECT_NEW_POTION3] = (localeinfo.AFFECT_NEW_POTION3, "d:/ymir work/ui/pattern/potions/88912.tga")
		AFFECT_DATA_DICT[chr.AFFECT_NEW_POTION4] = (localeinfo.AFFECT_NEW_POTION4, "d:/ymir work/ui/pattern/potions/88913.tga")
		AFFECT_DATA_DICT[chr.AFFECT_NEW_POTION5] = (localeinfo.AFFECT_NEW_POTION5, "d:/ymir work/ui/pattern/potions/88914.tga")
		AFFECT_DATA_DICT[chr.AFFECT_NEW_POTION6] = (localeinfo.AFFECT_NEW_POTION6, "d:/ymir work/ui/pattern/potions/88915.tga")
		AFFECT_DATA_DICT[chr.AFFECT_NEW_POTION7] = (localeinfo.AFFECT_NEW_POTION7, "d:/ymir work/ui/pattern/potions/88916.tga")
		AFFECT_DATA_DICT[chr.AFFECT_NEW_POTION8] = (localeinfo.AFFECT_NEW_POTION8, "d:/ymir work/ui/pattern/potions/88917.tga")
		AFFECT_DATA_DICT[chr.AFFECT_NEW_POTION9] = (localeinfo.AFFECT_NEW_POTION9, "d:/ymir work/ui/pattern/potions/88918.tga")
		AFFECT_DATA_DICT[chr.AFFECT_NEW_POTION10] = (localeinfo.AFFECT_NEW_POTION10, "d:/ymir work/ui/pattern/potions/88919.tga")
		AFFECT_DATA_DICT[chr.AFFECT_NEW_POTION11] = (localeinfo.AFFECT_NEW_POTION11, "d:/ymir work/ui/pattern/potions/88920.tga")
		AFFECT_DATA_DICT[chr.AFFECT_NEW_POTION12] = (localeinfo.AFFECT_NEW_POTION12, "d:/ymir work/ui/pattern/potions/88921.tga")
		AFFECT_DATA_DICT[chr.AFFECT_NEW_POTION13] = (localeinfo.AFFECT_NEW_POTION13, "d:/ymir work/ui/pattern/potions/88922.tga")
		AFFECT_DATA_DICT[chr.AFFECT_NEW_POTION14] = (localeinfo.AFFECT_NEW_POTION14, "d:/ymir work/ui/pattern/potions/88923.tga")
		AFFECT_DATA_DICT[chr.AFFECT_NEW_POTION15] = (localeinfo.AFFECT_NEW_POTION15, "d:/ymir work/ui/pattern/potions/88924.tga")
		AFFECT_DATA_DICT[chr.AFFECT_NEW_POTION16] = (localeinfo.AFFECT_NEW_POTION16, "d:/ymir work/ui/pattern/potions/88925.tga")
		AFFECT_DATA_DICT[chr.AFFECT_NEW_POTION17] = (localeinfo.AFFECT_NEW_POTION17, "d:/ymir work/ui/pattern/potions/88926.tga")
		AFFECT_DATA_DICT[chr.AFFECT_NEW_POTION18] = (localeinfo.AFFECT_NEW_POTION18, "d:/ymir work/ui/pattern/potions/88927.tga")
		AFFECT_DATA_DICT[chr.AFFECT_NEW_POTION19] = (localeinfo.AFFECT_NEW_POTION19, "d:/ymir work/ui/pattern/potions/88928.tga")
		AFFECT_DATA_DICT[chr.AFFECT_NEW_POTION20] = (localeinfo.AFFECT_NEW_POTION20, "d:/ymir work/ui/pattern/potions/88929.tga")
		AFFECT_DATA_DICT[chr.AFFECT_NEW_POTION21] = (localeinfo.AFFECT_NEW_POTION21, "d:/ymir work/ui/pattern/potions/72001.tga")
		AFFECT_DATA_DICT[chr.AFFECT_NEW_POTION22] = (localeinfo.AFFECT_NEW_POTION22, "d:/ymir work/ui/pattern/potions/71153.tga")
		AFFECT_DATA_DICT[chr.AFFECT_NEW_POTION23] = (localeinfo.AFFECT_NEW_POTION23, "d:/ymir work/ui/pattern/potions/72006.tga")
		AFFECT_DATA_DICT[chr.AFFECT_NEW_POTION24] = (localeinfo.AFFECT_NEW_POTION24, "d:/ymir work/ui/pattern/potions/71056.tga")
		AFFECT_DATA_DICT[chr.AFFECT_NEW_POTION25] = (localeinfo.AFFECT_NEW_POTION25, "d:/ymir work/ui/pattern/potions/71057.tga")
		AFFECT_DATA_DICT[chr.AFFECT_NEW_POTION26] = (localeinfo.AFFECT_NEW_POTION26, "d:/ymir work/ui/pattern/potions/71058.tga")
		AFFECT_DATA_DICT[chr.AFFECT_NEW_POTION27] = (localeinfo.AFFECT_NEW_POTION27, "d:/ymir work/ui/pattern/potions/71059.tga")
		AFFECT_DATA_DICT[chr.AFFECT_NEW_POTION28] = (localeinfo.AFFECT_NEW_POTION28, "d:/ymir work/ui/pattern/potions/71060.tga")
		AFFECT_DATA_DICT[chr.AFFECT_NEW_POTION29] = (localeinfo.AFFECT_NEW_POTION29, "d:/ymir work/ui/pattern/potions/71061.tga")
		AFFECT_DATA_DICT[chr.AFFECT_NEW_POTION30] = (localeinfo.AFFECT_NEW_POTION30, "d:/ymir work/ui/pattern/potions/72013.tga")
		AFFECT_DATA_DICT[chr.AFFECT_NEW_POTION31] = (localeinfo.AFFECT_NEW_POTION31, "d:/ymir work/ui/pattern/potions/72044.tga")
#	if app.AFFECT_EXP_ITEM_RAZOR93:
		AFFECT_DATA_DICT[chr.AFFECT_EXP_RAZOR93] = (localeinfo.AFFECT_NEW_POTION21, "d:/ymir work/ui/pattern/potions/72001.tga")
		AFFECT_DATA_DICT[chr.AFFECT_ITEM_RAZOR93] = (localeinfo.AFFECT_NEW_POTION23, "d:/ymir work/ui/pattern/potions/72006.tga")

	if app.ENABLE_VOTE_FOR_BONUS:
		AFFECT_DATA_DICT[chr.AFFECT_VOTEFORBONUS] = (localeinfo.TOOLTIP_AFFECT_VOTEFORBONUS, "d:/ymir work/ui/pattern/potions/votexbonus.tga")

	def __init__(self):
		ui.Window.__init__(self)

		self.serverPlayTime=0
		self.clientPlayTime=0

		self.lastUpdateTime=0
		self.affectImageDict={}
		self.horseImage=None
		self.lovePointImage=None
		if app.ENABLE_RENEWAL_AFFECT_SHOWER:
			self.autoPotionImageHP = None
			self.autoPotionImageSP = None
		else:
			self.autoPotionImageHP = AutoPotionImage()
			self.autoPotionImageSP = AutoPotionImage()
		self.SetPosition(10, 10)
		self.Show()

	def ClearAllAffects(self):
		self.horseImage=None
		self.lovePointImage=None
		self.affectImageDict={}
		self.__ArrangeImageList()

	def ClearAffects(self): ## ���� �������� ��������.
		self.living_affectImageDict={}
		for key, image in self.affectImageDict.items():
			if not image.IsSkillAffect():
				self.living_affectImageDict[key] = image
		self.affectImageDict = self.living_affectImageDict
		self.__ArrangeImageList()

	def CalculatePriority(self, affect):
		# PRIORITY_FIRST / PRIORITY_LAST can be configured in the renewal AffectImage block
		try:
			if affect in PRIORITY_FIRST:
				return 2  # high priority
		except:
			pass
		try:
			if affect in PRIORITY_LAST:
				return 5  # low priority
		except:
			pass

		# Optional ranges (Mall / water / dew systems)
		priority_ranges = []
		if hasattr(self, "WATER_DESC_IDX_START"):
			priority_ranges.append((self.MALL_DESC_IDX_START, self.WATER_DESC_IDX_START, 4))
			# priority_ranges.append((self.WATER_DESC_IDX_START, self.DEW_DESC_IDX_START, 6))
			# priority_ranges.append((self.DEW_DESC_IDX_START, self.DEW_DESC_IDX_START + self.DEW_RANGE_OFFSET, 4))

		for start, end, pr in priority_ranges:
			if start <= affect < end:
				return pr

		skillIndex = player.AffectIndexToSkillIndex(affect)
		if skillIndex != 0:
			return 1
		return 3

	def BINARY_NEW_AddAffect(self, type, pointIdx, value, duration):
		if app.ENABLE_RENEWAL_AFFECT_SHOWER:
			print "BINARY_NEW_AddAffect", type, pointIdx, value, duration

			affect = player.SkillIndexToAffectIndex(type)
			if affect == 0:
				affect = type
			original_affect = affect

			allowed_affects = [
				chr.NEW_AFFECT_POLYMORPH,
			]

			Skill_affects = [
				chr.AFFECT_JEONGWI,
				chr.AFFECT_GEOMGYEONG,
				chr.AFFECT_CHEONGEUN,
				chr.AFFECT_FALLEN_CHEONGEUN,
				chr.AFFECT_GYEONGGONG,
				chr.AFFECT_EUNHYEONG,
				chr.AFFECT_GWIGEOM,
				chr.AFFECT_GONGPO,
				chr.AFFECT_JUMAGAP,
				chr.AFFECT_HOSIN,
				chr.AFFECT_BOHO,
				chr.AFFECT_KWAESOK,
				chr.AFFECT_HEUKSIN,
				chr.AFFECT_MUYEONG,
				chr.AFFECT_GICHEON,
				chr.AFFECT_JEUNGRYEOK,
				chr.AFFECT_PABEOP,
			]

			if type < 500 and type not in allowed_affects and affect not in Skill_affects:
				return

			if type == chr.NEW_AFFECT_MALL:
				affect = self.MALL_DESC_IDX_START + pointIdx
			else:
				affect = original_affect

			if self.affectImageDict.get(affect):
				# Allow re-append only for skill affects (so descriptions can refresh)
				if player.AffectIndexToSkillIndex(affect) == 0:
					return

			# Data dict presence (pet + normal)
			if app.NEW_PET_SYSTEM:
				if not self.AFFECT_DATA_DICT.has_key(affect) and not self.AFFECT_PET_DATA_DICT.has_key(affect):
					return
			else:
				if not self.AFFECT_DATA_DICT.has_key(affect):
					return

			priority = self.CalculatePriority(affect)

			# Force duration 0 for these
			if affect in [
				chr.NEW_AFFECT_NO_DEATH_PENALTY,
				chr.NEW_AFFECT_SKILL_BOOK_BONUS,
				chr.NEW_AFFECT_AUTO_SP_RECOVERY,
				chr.NEW_AFFECT_AUTO_HP_RECOVERY,
				chr.NEW_AFFECT_SKILL_BOOK_NO_DELAY,
			]:
				duration = 0

			if app.NEW_PET_SYSTEM and self.AFFECT_PET_DATA_DICT.has_key(affect):
				affectData = self.AFFECT_PET_DATA_DICT[affect]
			else:
				affectData = self.AFFECT_DATA_DICT[affect]

			description = affectData[0]
			filename = affectData[1]

			if pointIdx == player.POINT_MALL_ITEMBONUS or pointIdx == player.POINT_MALL_GOLDBONUS:
				value = 1 + float(value) / 100.0

			# Resolve callable descriptions
			if affect != chr.NEW_AFFECT_AUTO_SP_RECOVERY and affect != chr.NEW_AFFECT_AUTO_HP_RECOVERY:
				if callable(description):
					description = description(float(value))

			try:
				print "Add affect %s" % affect
				image = AffectImage()
				image.SetParent(self)
				image.LoadImage(filename)
				image.SetDescription(description)
				image.SetDuration(duration)
				image.SetAffect(affect)

				# Skill affects -> removable by click
				if affect == player.SkillIndexToAffectIndex(type):
					skillIndex = player.AffectIndexToSkillIndex(player.SkillIndexToAffectIndex(type))
					image.SetSkillIndex(skillIndex)
					image.SetSkillAffectFlag(True)
					image.SetCanberemoved(True)
					image.UpdateSkillDescription()
					if hasattr(image, "SetEvent"):
						image.SetEvent(ui.__mem_func__(self.__QuestionRemoveAffect), "mouse_click", affect, description)
				else:
					image.SetSkillAffectFlag(False)
					EXP_BONUS_AFFECTS = set([chr.NEW_AFFECT_EXP_BONUS_EURO_FREE, chr.NEW_AFFECT_EXP_BONUS_EURO_FREE_UNDER_15])
					AUTO_POTION_AFFECTS = set([chr.NEW_AFFECT_AUTO_SP_RECOVERY, chr.NEW_AFFECT_AUTO_HP_RECOVERY])

					if affect in EXP_BONUS_AFFECTS or self.INFINITE_AFFECT_DURATION < duration:
						image.UpdateDescription()
					elif affect in AUTO_POTION_AFFECTS:
						image.UpdateAutoPotionDescription()
					elif affect == chr.AFFECT_GICHEON:
						image.SetSkillAffectFlag(False)
						image.SetDescription(localeinfo.SKILL_GICHEON % value)
						image.UpdateDescription()
					elif affect == chr.NEW_AFFECT_POLYMORPH:
						image.UpdateDescription()
						image.SetCanberemoved(True)
						if hasattr(image, "SetEvent"):
							image.SetEvent(ui.__mem_func__(self.__QuestionRemoveAffect), "mouse_click", affect, description)
					else:
						image.UpdateDescription()

				DRAGON_SOUL_AFFECTS = set([chr.NEW_AFFECT_DRAGON_SOUL_DECK1, chr.NEW_AFFECT_DRAGON_SOUL_DECK2])
				if affect in DRAGON_SOUL_AFFECTS:
					image.SetScale(1, 1)
				else:
					image.SetScale(0.7, 0.7)

				image.SetPriority(priority)

				image.Show()
				self.affectImageDict[affect] = image
				self.__ArrangeImageList()
			except Exception, e:
				print "except Aff affect ", e
				pass
			return

		# non-renewal (original behavior)

		print "BINARY_NEW_AddAffect", type, pointIdx, value, duration

		if type < 500:
			return

		if type == chr.NEW_AFFECT_MALL:
			affect = self.MALL_DESC_IDX_START + pointIdx
		else:
			affect = type

		if self.affectImageDict.has_key(affect):
			return

		if app.NEW_PET_SYSTEM:
			if not self.AFFECT_DATA_DICT.has_key(affect) and not self.AFFECT_PET_DATA_DICT.has_key(affect):
				return
		else:
			if not self.AFFECT_DATA_DICT.has_key(affect):
				return

		## ������ ����, ������ ������ Duration �� 0 ���� ��������.
		if affect == chr.NEW_AFFECT_NO_DEATH_PENALTY or\
		   affect == chr.NEW_AFFECT_SKILL_BOOK_BONUS or\
		   affect == chr.NEW_AFFECT_AUTO_SP_RECOVERY or\
		   affect == chr.NEW_AFFECT_AUTO_HP_RECOVERY or\
		   affect == chr.NEW_AFFECT_SKILL_BOOK_NO_DELAY:
			duration = 0

		if app.NEW_PET_SYSTEM:
			if affect >= 5400 and affect <= 5419:
				affectData = self.AFFECT_PET_DATA_DICT[affect]
				#chat.AppendChat(chat.CHAT_TYPE_INFO, "prova")
			else:
				affectData = self.AFFECT_DATA_DICT[affect]
		else:
			affectData = self.AFFECT_DATA_DICT[affect]

		description = affectData[0]
		filename = affectData[1]

		if pointIdx == player.POINT_MALL_ITEMBONUS or\
		   pointIdx == player.POINT_MALL_GOLDBONUS:
			value = 1 + float(value) / 100.0

		if app.NEW_PET_SYSTEM:
			if affect != chr.NEW_AFFECT_AUTO_SP_RECOVERY and affect != chr.NEW_AFFECT_AUTO_HP_RECOVERY and not self.AFFECT_PET_DATA_DICT.has_key(affect):
				if app.ENABLE_BLOCK_MULTIFARM and (affect == chr.AFFECT_DROP_BLOCK or affect == chr.AFFECT_DROP_UNBLOCK or affect == chr.AFFECT_EXP_RAZOR93 or affect == chr.AFFECT_ITEM_RAZOR93):
					duration = 0
				elif app.ENABLE_RUNE_SYSTEM and (affect == chr.AFFECT_RUNE1 or affect == chr.AFFECT_RUNE2):
					duration = 0
				elif app.ENABLE_NEW_USE_POTION and (affect >= chr.AFFECT_NEW_POTION1 and affect <= chr.AFFECT_NEW_POTION31):
					duration = 0
				elif app.ENABLE_VOTE_FOR_BONUS and affect == chr.AFFECT_VOTEFORBONUS:
					duration = duration
				else:
					if app.ENABLE_NEW_USE_POTION and (affect == chr.NEW_AFFECT_AUTO_HP_RECOVERY2 or affect == chr.NEW_AFFECT_AUTO_SP_RECOVERY2):
						duration = 0
					else:
						description = description(float(value))
		else:
			if affect != chr.NEW_AFFECT_AUTO_SP_RECOVERY and affect != chr.NEW_AFFECT_AUTO_HP_RECOVERY:
				if app.ENABLE_BLOCK_MULTIFARM and (affect == chr.AFFECT_DROP_BLOCK or affect == chr.AFFECT_DROP_UNBLOCK):
					duration = 0
				elif app.ENABLE_RUNE_SYSTEM and (affect == chr.AFFECT_RUNE1 or affect == chr.AFFECT_RUNE2):
					duration = 0
				elif app.ENABLE_NEW_USE_POTION and (affect >= chr.AFFECT_NEW_POTION1 or affect <= chr.AFFECT_NEW_POTION31):
					duration = 0
				else:
					if app.ENABLE_NEW_USE_POTION and (affect == chr.NEW_AFFECT_AUTO_HP_RECOVERY2 or affect == chr.NEW_AFFECT_AUTO_SP_RECOVERY2):
						duration = 0
						description = localeinfo.PERMANENT_EFFECT
					else:
						description = description(float(value))

		try:
			print "Add affect %s" % affect
			image = AffectImage()
			image.SetParent(self)
			image.LoadImage(filename)
			if app.ENABLE_NEW_USE_POTION and (affect == chr.NEW_AFFECT_AUTO_HP_RECOVERY2 or affect == chr.NEW_AFFECT_AUTO_SP_RECOVERY2):
				image.SetDescription(localeinfo.PERMANENT_EFFECT, True)
			else:
				image.SetDescription(description)

			image.SetAffect(affect)
			image.SetDuration(duration)
			if affect == chr.NEW_AFFECT_EXP_BONUS_EURO_FREE or affect == chr.NEW_AFFECT_EXP_BONUS_EURO_FREE_UNDER_15 or self.INFINITE_AFFECT_DURATION < duration:
				image.SetClock(False)
				image.UpdateDescription()
			elif affect == chr.NEW_AFFECT_AUTO_SP_RECOVERY or affect == chr.NEW_AFFECT_AUTO_HP_RECOVERY:
				image.UpdateAutoPotionDescription()
			else:
				image.UpdateDescription()

			if affect == chr.NEW_AFFECT_DRAGON_SOUL_DECK1 or affect == chr.NEW_AFFECT_DRAGON_SOUL_DECK2:
				image.SetScale(1, 1)
			else:
				image.SetScale(0.7, 0.7)

			image.SetSkillAffectFlag(False)
			image.Show()
			self.affectImageDict[affect] = image
			self.__ArrangeImageList()
		except Exception, e:
			print "except Aff affect ", e
			pass

	def BINARY_NEW_RemoveAffect(self, type, pointIdx):
		if type == chr.NEW_AFFECT_MALL:
			affect = self.MALL_DESC_IDX_START + pointIdx
		else:
			affect = type

		print "Remove Affect %s %s" % ( type , pointIdx )
		self.__RemoveAffect(affect)
		self.__ArrangeImageList()

	def SetAffect(self, affect):
		self.__AppendAffect(affect)
		self.__ArrangeImageList()

	def ResetAffect(self, affect):
		self.__RemoveAffect(affect)
		self.__ArrangeImageList()

	def SetLoverInfo(self, name, lovePoint):
		image = LovePointImage()
		image.SetParent(self)
		image.SetLoverInfo(name, lovePoint)
		self.lovePointImage = image
		self.__ArrangeImageList()

	def ShowLoverState(self):
		if self.lovePointImage:
			self.lovePointImage.Show()
			self.__ArrangeImageList()

	def HideLoverState(self):
		if self.lovePointImage:
			self.lovePointImage.Hide()
			self.__ArrangeImageList()

	def ClearLoverState(self):
		self.lovePointImage = None
		self.__ArrangeImageList()

	def OnUpdateLovePoint(self, lovePoint):
		if self.lovePointImage:
			self.lovePointImage.OnUpdateLovePoint(lovePoint)

	def SetHorseState(self, level, health, battery):
		if level==0:
			self.horseImage=None
			self.__ArrangeImageList()
		else:
			image = HorseImage()
			image.SetParent(self)
			image.SetState(level, health, battery)
			image.Show()

			self.horseImage=image
			self.__ArrangeImageList()

	def SetPlayTime(self, playTime):
		self.serverPlayTime = playTime
		self.clientPlayTime = app.GetTime()

	def __AppendAffect(self, affect):

		if self.affectImageDict.has_key(affect):
			return

		# ENABLE_VOTE4BUFF: voteBuffData is filled in BINARY_NEW_AddAffect (packet path)

		try:
			affectData = self.AFFECT_DATA_DICT[affect]
		except KeyError:
			return

		name = affectData[0]
		filename = affectData[1]

		skillIndex = player.AffectIndexToSkillIndex(affect)
		if 0 != skillIndex:
			name = skill.GetSkillName(skillIndex)

		image = AffectImage()
		image.SetParent(self)
		image.SetAffect(affect)
		image.SetSkillAffectFlag(True)

		if app.ENABLE_RENEWAL_AFFECT_SHOWER:
			priority = self.CalculatePriority(affect)
			image.SetPriority(priority)

		try:
			image.LoadImage(filename)
		except:
			pass

		if app.ENABLE_RENEWAL_AFFECT_SHOWER:
			if skillIndex != 0:
				image.SetCanberemoved(True)
				image.SetToolTipText(name)
				if hasattr(image, "SetEvent"):
					image.SetEvent(ui.__mem_func__(self.__QuestionRemoveAffect), "mouse_click", affect, name)
			else:
				image.SetToolTipText(name)
		else:
			image.SetToolTipText(name, 0, 40)
		image.SetScale(0.7, 0.7)
		image.Show()
		self.affectImageDict[affect] = image

	def __RemoveAffect(self, affect):
		"""
		if affect == chr.NEW_AFFECT_AUTO_SP_RECOVERY:
			self.autoPotionImageSP.Hide()

		if affect == chr.NEW_AFFECT_AUTO_HP_RECOVERY:
			self.autoPotionImageHP.Hide()
		"""

		if not self.affectImageDict.has_key(affect):
			print "__RemoveAffect %s ( No Affect )" % affect
			return

		print "__RemoveAffect %s ( Affect )" % affect
		del self.affectImageDict[affect]

		self.__ArrangeImageList()

	if app.ENABLE_RENEWAL_AFFECT_SHOWER:
		def __ArrangeImageList(self):
			ordered_elements = sorted(
				self.affectImageDict.values(),
				key=lambda image: image.GetPriority()
			)

			xPos, yPos = 0, 0
			xMax = 0
			countRow = 0

			if self.lovePointImage and self.lovePointImage.IsShow():
				self.lovePointImage.SetPosition(xPos, yPos)
				xPos += self.IMAGE_STEP
				countRow += 1

			if self.horseImage:
				self.horseImage.SetPosition(xPos, yPos)
				xPos += self.IMAGE_STEP
				countRow += 1

			for count, image in enumerate(ordered_elements):
				image.SetPosition(xPos, yPos)
				xPos += self.IMAGE_STEP
				countRow += 1

				if xMax < xPos:
					xMax = xPos

				if countRow == self.MAX_ITEMS_PER_ROW:
					xPos = 0
					yPos += self.IMAGE_STEP
					countRow = 0

			self.SetSize(xMax if xMax > 0 else self.IMAGE_STEP * self.MAX_ITEMS_PER_ROW, yPos + 26)
	else:
		def __ArrangeImageList(self):
			width = len(self.affectImageDict) * self.IMAGE_STEP
			if self.lovePointImage:
				width += self.IMAGE_STEP
			if self.horseImage:
				width += self.IMAGE_STEP

			self.SetSize(width, 26)

			xPos = 0

			if self.lovePointImage:
				if self.lovePointImage.IsShow():
					self.lovePointImage.SetPosition(xPos, 0)
					xPos += self.IMAGE_STEP

			if self.horseImage:
				self.horseImage.SetPosition(xPos, 0)
				xPos += self.IMAGE_STEP

			for image in self.affectImageDict.values():
				image.SetPosition(xPos, 0)
				xPos += self.IMAGE_STEP

	if app.ENABLE_RENEWAL_AFFECT_SHOWER:
		def OnUpdate(self):
			try:
				currentTime = app.GetGlobalTimeStamp()  # seconds
				if currentTime != getattr(self, "lastUpdateTime", 0):
					self.lastUpdateTime = currentTime
		
					for image in self.affectImageDict.values():
						if not getattr(image, "isToolTipVisible", False):
							continue
		
						if image.GetAffect() in (chr.NEW_AFFECT_AUTO_HP_RECOVERY, chr.NEW_AFFECT_AUTO_SP_RECOVERY):
							image.UpdateAutoPotionDescription()
						elif image.IsSkillAffect():
							image.UpdateSkillDescription()
						else:
							image.UpdateDescription()
			except Exception as e:
				print "AffectShower::OnUpdate error:", e


		def SyncAffectImages(self):
			for image in self.affectImageDict.values():
				try:
					image.isToolTipVisible = False
					if hasattr(image, 'toolTip') and image.toolTip:
						try:
							image.toolTip.HideToolTip()
						except:
							try:
								image.toolTip.Hide()
							except:
								pass
				except:
					pass

		def __QuestionRemoveAffect(self, arg, affect, name):
			self.RemoveQuestionDialog = uicommon.QuestionDialog()
			try:
				self.RemoveQuestionDialog.SetText(localeinfo.AFFECT_REMOVE_QUESTION % name)
			except:
				self.RemoveQuestionDialog.SetText(name)
			self.RemoveQuestionDialog.SetAcceptEvent(lambda arg=True: self.AnswerRemoveAffect(arg))
			self.RemoveQuestionDialog.SetCancelEvent(lambda arg=False: self.AnswerRemoveAffect(arg))
			self.RemoveQuestionDialog.affect = affect
			self.RemoveQuestionDialog.Open()

		def AnswerRemoveAffect(self, flag):
			if not hasattr(self, 'RemoveQuestionDialog') or not self.RemoveQuestionDialog:
				return
			if flag:
				# Fix: the server expects the affect type (1-based), not the skill index
				affect = self.RemoveQuestionDialog.affect
				# If we accidentally got a skillIndex, convert it
				try:
					if player.AffectIndexToSkillIndex(affect) == 0:
						conv = player.SkillIndexToAffectIndex(affect)
						if conv != 0:
							affect = conv
				except:
					pass
				net.SendChatPacket("/remove_affect %d" % (affect + 1))
			self.RemoveQuestionDialog.Close()
			self.RemoveQuestionDialog = None

		def Show(self):
			ui.Window.Show(self)
			self.SyncAffectImages()
	else:
		def OnUpdate(self):
			try:
				if app.GetGlobalTime() - self.lastUpdateTime > 500:
				#if 0 < app.GetGlobalTime():
					self.lastUpdateTime = app.GetGlobalTime()

					for image in self.affectImageDict.values():
						if image.GetAffect() == chr.NEW_AFFECT_AUTO_HP_RECOVERY or image.GetAffect() == chr.NEW_AFFECT_AUTO_SP_RECOVERY:
							image.UpdateAutoPotionDescription()
							continue

						if app.ENABLE_NEW_USE_POTION and (image.GetAffect() == chr.NEW_AFFECT_AUTO_HP_RECOVERY2 or image.GetAffect() == chr.NEW_AFFECT_AUTO_SP_RECOVERY2):
							image.UpdateAutoPotionDescription()
							continue

						if not image.IsSkillAffect():
							image.UpdateDescription()
			except Exception, e:
				print "AffectShower::OnUpdate error : ", e
