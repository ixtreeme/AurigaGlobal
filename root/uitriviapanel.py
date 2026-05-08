# -*- coding: utf-8 -*-

import ui
import dbg
import pyapi
import os
import app
import net
import player
import chat

# Fallback: if ThinBoardCircle does not exist, use ThinBoard or Bar
ThinBoardLike = getattr(ui, "ThinBoardCircle", None) or getattr(ui, "ThinBoard", ui.Bar)

class TriviaPanelWindow(ui.BoardWithTitleBar):
	def __init__(self):
		ui.BoardWithTitleBar.__init__(self)
		self.textShow1 = None
		self.textShow2 = None
		self.textShow3 = None
		self.textShow4 = None
		self.ThinBoardList = None
		self.ThinBoardList2 = None
		self.ThinBoardList3 = None
		self.ThinBoardList4 = None
		self.statusInput = None
		self.statusInput2 = None
		self.statusInput3 = None
		self.statusInput4 = None
		self.btn1 = None

	def __del__(self):
		ui.BoardWithTitleBar.__del__(self)

	def BuildWindow(self):
		self.SetSize(244, 230)
		self.Show()
		self.AddFlag("float")
		self.AddFlag("movable")
		self.SetTitleName("GM: Trivia Event Panel")
		self.SetCenterPosition()
		self.LoadWindow()

	def LoadWindow(self):
		# Question label
		self.textShow1 = ui.TextLine()
		self.textShow1.SetParent(self)
		self.textShow1.SetPosition(122, 33)
		self.textShow1.SetHorizontalAlignCenter()
		self.textShow1.SetDefaultFontName()
		self.textShow1.SetText("Question (type the question here)")
		self.textShow1.Show()

		# Question input
		self.ThinBoardList = ThinBoardLike()
		self.ThinBoardList.SetParent(self)
		self.ThinBoardList.SetSize(200, 25)
		self.ThinBoardList.SetPosition(22, 50)
		self.ThinBoardList.Show()

		self.statusInput = ui.EditLine()
		self.statusInput.SetParent(self.ThinBoardList)
		self.statusInput.SetSize(350, 18)
		self.statusInput.SetPosition(10, self.ThinBoardList.GetHeight() / 2 - 5)
		self.statusInput.SetMax(64)
		self.statusInput.SetText("")
		self.statusInput.Show()

		# Answer label
		self.textShow2 = ui.TextLine()
		self.textShow2.SetParent(self)
		self.textShow2.SetPosition(122, 83)
		self.textShow2.SetHorizontalAlignCenter()
		self.textShow2.SetDefaultFontName()
		self.textShow2.SetText("Answer (type the answer here)")
		self.textShow2.Show()

		# Answer input
		self.ThinBoardList2 = ThinBoardLike()
		self.ThinBoardList2.SetParent(self)
		self.ThinBoardList2.SetSize(200, 25)
		self.ThinBoardList2.SetPosition(22, 100)
		self.ThinBoardList2.Show()

		self.statusInput2 = ui.EditLine()
		self.statusInput2.SetParent(self.ThinBoardList2)
		self.statusInput2.SetSize(350, 18)
		self.statusInput2.SetPosition(10, self.ThinBoardList2.GetHeight() / 2 - 5)
		self.statusInput2.SetMax(64)
		self.statusInput2.SetText("")
		self.statusInput2.Show()

		# Item vnum label + input
		self.ThinBoardList3 = ThinBoardLike()
		self.ThinBoardList3.SetParent(self)
		self.ThinBoardList3.SetSize(80, 25)
		self.ThinBoardList3.SetPosition(22, 150)
		self.ThinBoardList3.Show()

		self.textShow3 = ui.TextLine()
		self.textShow3.SetParent(self.ThinBoardList3)
		self.textShow3.SetPosition(40, -17)
		self.textShow3.SetHorizontalAlignCenter()
		self.textShow3.SetDefaultFontName()
		self.textShow3.SetText("Item vnum")
		self.textShow3.Show()

		self.statusInput3 = ui.EditLine()
		self.statusInput3.SetParent(self.ThinBoardList3)
		self.statusInput3.SetSize(350, 18)
		self.statusInput3.SetPosition(10, self.ThinBoardList3.GetHeight() / 2 - 5)
		self.statusInput3.SetMax(16)
		self.statusInput3.SetText("")
		self.statusInput3.Show()

		# Item count label + input
		self.ThinBoardList4 = ThinBoardLike()
		self.ThinBoardList4.SetParent(self)
		self.ThinBoardList4.SetSize(80, 25)
		self.ThinBoardList4.SetPosition(142, 150)
		self.ThinBoardList4.Show()

		self.textShow4 = ui.TextLine()
		self.textShow4.SetParent(self.ThinBoardList4)
		self.textShow4.SetPosition(30, -17)
		self.textShow4.SetDefaultFontName()
		self.textShow4.SetText("Count")
		self.textShow4.Show()

		self.statusInput4 = ui.EditLine()
		self.statusInput4.SetParent(self.ThinBoardList4)
		self.statusInput4.SetSize(350, 18)
		self.statusInput4.SetPosition(10, self.ThinBoardList4.GetHeight() / 2 - 5)
		self.statusInput4.SetMax(16)
		self.statusInput4.SetText("")
		self.statusInput4.Show()

		# Start button
		self.btn1 = ui.Button()
		self.btn1.SetParent(self)
		self.btn1.SetPosition(32, 188)
		self.btn1.SetUpVisual("d:/ymir work/ui/public/xlarge_button_01.sub")
		self.btn1.SetOverVisual("d:/ymir work/ui/public/xlarge_button_02.sub")
		self.btn1.SetDownVisual("d:/ymir work/ui/public/xlarge_button_03.sub")
		self.btn1.SetText("Start Trivia Event")
		self.btn1.SetEvent(self.TriviaEvent)
		self.btn1.Show()

	def TriviaEvent(self):
		if len(self.statusInput.GetText()) == 0:
			chat.AppendChat(chat.CHAT_TYPE_INFO, "You must enter a question first.")
			return

		if len(self.statusInput2.GetText()) == 0:
			chat.AppendChat(chat.CHAT_TYPE_INFO, "You must enter an answer first.")
			return

		if len(self.statusInput3.GetText()) == 0 or len(self.statusInput4.GetText()) == 0:
			chat.AppendChat(chat.CHAT_TYPE_INFO, "You must enter an item vnum and its quantity.")
			return

		cmd = '/trivia atheroOsugePuternik "{}" "{}" {} {}'.format(
			self.statusInput.GetText(),
			self.statusInput2.GetText(),
			self.statusInput3.GetText(),
			self.statusInput4.GetText()
		)
		net.SendChatPacket(cmd)

		self.statusInput.SetText("")
		self.statusInput2.SetText("")
		self.statusInput3.SetText("")
		self.statusInput4.SetText("")
		self.Close()

	def OpenWindow(self):
		if self.IsShow():
			self.Close()
		else:
			self.BuildWindow()

	def Close(self):
		self.Hide()
