#include "StdAfx.h"
#include "PythonEventManager.h"
#include "PythonNetworkStream.h"
#include "PythonNonPlayer.h"

#include "AbstractApplication.h"

#include "../Game/ItemData.h"
#include "../Game/ItemManager.h"

#include "../Pack/EterPackManager.h"

#include "PythonMiniMap.h"
#include "PythonApplication.h"

const long c_lNormal_Waiting_Time =
#ifdef ENABLE_NEW_BUGFIXES
0
#else
3
#endif
;
constexpr int c_fLine_Temp = 16;

void ShowArgument(const script::TArgList& rArgumentList)
{
	for (auto& itor : rArgumentList)
	{
		const std::string& rName = itor.strName;

		Tracen(rName.c_str());
	}
}

const std::string& GetArgumentString(const char* c_szName, script::TArgList& rArgumentList)
{
	for (auto& itor : rArgumentList)
	{
		const std::string& rName = itor.strName;

		if (c_szName == rName)
			return itor.strValue;
	}

	static std::string strEmptyValue;
	return strEmptyValue;
}

const char* GetArgument(const char* c_szName, script::TArgList& rArgumentList)
{
	return GetArgumentString(c_szName, rArgumentList).c_str();
}

void GetCameraSettingFromArgList(script::TArgList& rArgList, IAbstractApplication::SCameraSetting* pCameraSetting)
{
	int ix = atoi(GetArgument("x", rArgList));
	int iy = atoi(GetArgument("y", rArgList));
	int iz = atoi(GetArgument("z", rArgList));
	int iUpDir = atoi(GetArgument("up", rArgList));
	int iViewDir = atoi(GetArgument("view", rArgList));
	int iCrossDir = atoi(GetArgument("cross", rArgList));
	int iDistance = atoi(GetArgument("distance", rArgList));
	int iRot = atoi(GetArgument("rot", rArgList));
	int iPitch = atoi(GetArgument("pitch", rArgList));

	ZeroMemory(pCameraSetting, sizeof(IAbstractApplication::SCameraSetting));
	pCameraSetting->v3CenterPosition.x = float(ix);
	pCameraSetting->v3CenterPosition.y = float(iy);
	pCameraSetting->v3CenterPosition.z = float(iz);
	pCameraSetting->kCmrPos.m_fUpDir = float(iUpDir);
	pCameraSetting->kCmrPos.m_fViewDir = float(iViewDir);
	pCameraSetting->kCmrPos.m_fCrossDir = float(iCrossDir);
	pCameraSetting->fZoom = float(iDistance);
	pCameraSetting->fRotation = float(iRot);
	pCameraSetting->fPitch = float(iPitch);
}

void CPythonEventManager::__InitEventSet(TEventSet& rEventSet)
{
	rEventSet.ix = 0;
	rEventSet.iy = 0;
	rEventSet.iWidth = 0;
	rEventSet.iyLocal = 0;

	rEventSet.isLock = false;
	rEventSet.lLastDelayTime = 0;
	rEventSet.iCurrentLetter = 0;
	rEventSet.CurrentColor = D3DXCOLOR(1, 1, 1, 1);
	rEventSet.strCurrentLine = "";

	rEventSet.pCurrentTextLine = nullptr;
	rEventSet.ScriptTextLineList.clear();

	rEventSet.isConfirmWait = FALSE;
	rEventSet.pConfirmTimeTextLine = nullptr;
	rEventSet.iConfirmEndTime = 0;

	rEventSet.DiffuseColor = D3DXCOLOR(1, 1, 1, 1);
	rEventSet.lWaitingTime = c_lNormal_Waiting_Time;
	rEventSet.iRestrictedCharacterCount = 30;

	rEventSet.iVisibleStartLine = 0;
	rEventSet.iVisibleLineCount = BOX_VISIBLE_LINE_COUNT;

	rEventSet.iAdjustLine = 0;

	rEventSet.isTextCenterMode = false;
	rEventSet.isWaitFlag = false;

	__InsertLine(rEventSet);
}

int CPythonEventManager::RegisterEventSet(const char* c_szFileName)
{
	CMappedFile File;
	const void* pMap;

	if (!CEterPackManager::Instance().Get(File, c_szFileName, &pMap))
		return -1;

	std::string strEventString;
	strEventString.resize(File.Size() + 1);

	File.Read(strEventString.data(), static_cast<int>(File.Size()));

	TEventSet* pEventSet = m_EventSetPool.Alloc();
	if (!pEventSet)
		return -1;

	if (!pEventSet->ScriptGroup.Create(strEventString))
	{
		__ClearEventSetp(pEventSet);
		return -1;
	}

	strncpy(pEventSet->szFileName, c_szFileName, 32);

	pEventSet->pCurrentTextLine = nullptr;
	pEventSet->poEventHandler = nullptr;

	__InitEventSet(*pEventSet);

	int iEmptySlotIndex = GetEmptyEventSetSlot();
	m_EventSetVector[iEmptySlotIndex] = pEventSet;
	return iEmptySlotIndex;
}

int CPythonEventManager::RegisterEventSetFromString(const std::string& strScript)
{
	TEventSet* pEventSet = m_EventSetPool.Alloc();
	if (!pEventSet)
		return -1;

	// SCRIPT_PARSING_FAILURE_CLEAR_BUG 스크립트 파싱 실패시 __ClearEventSetp 에서 에러 발생
	pEventSet->pCurrentTextLine = nullptr;
	// END_OF_SCRIPT_PARSING_FAILURE_CLEAR_BUG

	if (!pEventSet->ScriptGroup.Create(strScript))
	{
		__ClearEventSetp(pEventSet);
		return -1;
	}

	pEventSet->szFileName[0] = 0;
	pEventSet->poEventHandler = nullptr;
	__InitEventSet(*pEventSet);

	// NOTE : 만약 단순한 스크립트 이벤트 실행 커맨드라면 다시 만든다.
	script::TCmd ScriptCommand;
	int pEventPosition;
	int iEventType;
	if (pEventSet->ScriptGroup.ReadCmd(ScriptCommand))
	{
		if (GetScriptEventIndex(ScriptCommand.name.c_str(), &pEventPosition, &iEventType))
		{
			if (EVENT_TYPE_RUN_CINEMA == iEventType)
			{
				std::string strFileName = GetArgument("value", ScriptCommand.argList);

				__ClearEventSetp(pEventSet);

				return RegisterEventSet(strFileName.c_str());
			}
		}
	}

	int iEmptySlotIndex = GetEmptyEventSetSlot();
	m_EventSetVector[iEmptySlotIndex] = pEventSet;
	return iEmptySlotIndex;
}

void CPythonEventManager::ClearEventSeti(int iIndex)
{
	if (!CheckEventSetIndex(iIndex))
		return;

	__ClearEventSetp(m_EventSetVector[iIndex]);
	m_EventSetVector[iIndex] = nullptr;
}

void CPythonEventManager::__ClearEventSetp(TEventSet* pEventSet)
{
	if (!pEventSet)
		return;

	for (const auto& [ixLocal, iyLocal, pInstance] : pEventSet->ScriptTextLineList)
	{
		pInstance->Destroy();
		m_ScriptTextLinePool.Free(pInstance);
	}
	pEventSet->ScriptTextLineList.clear();

	if (pEventSet->pCurrentTextLine)
	{
		pEventSet->pCurrentTextLine->Destroy();
		m_ScriptTextLinePool.Free(pEventSet->pCurrentTextLine);
	}
	pEventSet->pCurrentTextLine = nullptr;
	pEventSet->strCurrentLine = "";
	pEventSet->iCurrentLetter = 0;

	m_EventSetPool.Free(pEventSet);
}

uint32_t CPythonEventManager::GetEmptyEventSetSlot()
{
	for (uint32_t i = 0; i < m_EventSetVector.size(); ++i)
	{
		if (nullptr == m_EventSetVector[i])
		{
			return i;
		}
	}

	m_EventSetVector.push_back(nullptr);
	return m_EventSetVector.size() - 1;
}

void CPythonEventManager::SetRestrictedCount(int iIndex, int iCount)
{
	if (!CheckEventSetIndex(iIndex))
		return;

	TEventSet* pEventSet = m_EventSetVector[iIndex];
	if (!pEventSet)
	{
		TraceError("CPythonEventManager::SetRestrictedCount m_EventSetVector[iIndex=%d]==NULL", iIndex);
		return;
	}

	pEventSet->iRestrictedCharacterCount = iCount;
}

void CPythonEventManager::SetEventHandler(int iIndex, PyObject* poEventHandler)
{
	if (!CheckEventSetIndex(iIndex))
		return;

	TEventSet* pEventSet = m_EventSetVector[iIndex];
	if (!pEventSet)
	{
		TraceError("CPythonEventManager::SetEventHandler m_EventSetVector[iIndex=%d]==NULL", iIndex);
		return;
	}

	pEventSet->poEventHandler = poEventHandler;
}

int CPythonEventManager::GetEventSetLocalYPosition(int iIndex)
{
	if (!CheckEventSetIndex(iIndex))
		return 0;

	TEventSet* pEventSet = m_EventSetVector[iIndex];
	if (!pEventSet)
		return 0;

	return pEventSet->iyLocal;
}

void CPythonEventManager::AddEventSetLocalYPosition(int iIndex, int iAddValue)
{
	if (!CheckEventSetIndex(iIndex))
		return;

	TEventSet* pEventSet = m_EventSetVector[iIndex];
	if (!pEventSet)
		return;

	pEventSet->iyLocal += iAddValue;
}

void CPythonEventManager::InsertText(int iIndex, const char* c_szText, int iX_pos)
{
	if (!CheckEventSetIndex(iIndex))
		return;

	TEventSet* pEventSet = m_EventSetVector[iIndex];
	if (!pEventSet)
		return;

	pEventSet->strCurrentLine = c_szText;
	if (pEventSet->pCurrentTextLine)
	{
		pEventSet->pCurrentTextLine->SetValue(c_szText);
		if (iX_pos == 0)
		{
			pEventSet->pCurrentTextLine->SetHorizonalAlign(CGraphicTextInstance::HORIZONTAL_ALIGN_CENTER);
		}
	}

	pEventSet->pCurrentTextLine->Update();
	__InsertLine(*pEventSet, FALSE, iX_pos);
}


void CPythonEventManager::UpdateEventSet(int iIndex, int ix, int iy)
{
	if (!CheckEventSetIndex(iIndex))
		return;

	TEventSet* pEventSet = m_EventSetVector[iIndex];
	if (!pEventSet)
		return;

	pEventSet->ix = ix;
	pEventSet->iy = iy;

	RefreshLinePosition(pEventSet);

	if (pEventSet->isConfirmWait)
	{
		int iLeftTime = max(0, pEventSet->iConfirmEndTime - timeGetTime() / 1000);
		pEventSet->pConfirmTimeTextLine->SetValue(_getf(m_strLeftTimeString.c_str(), iLeftTime));
	}

	if (pEventSet->isWaitFlag)
		return;

	///////////////////////////////////////////////////////////////////////////////////////////////

	// Process EventSet
	long lElapsedTime = CTimer::Instance().GetElapsedMilliecond();

	pEventSet->lLastDelayTime = max(0, pEventSet->lLastDelayTime - lElapsedTime);

	while (lElapsedTime > 0)
	{
		pEventSet->lLastDelayTime -= lElapsedTime;

		if (pEventSet->lLastDelayTime <= 0)
		{
			lElapsedTime = -pEventSet->lLastDelayTime;
			if (lElapsedTime <= 0)
				break;

			ProcessEventSet(pEventSet);

			if (pEventSet->lLastDelayTime < 0)
			{
				pEventSet->lLastDelayTime = 0;
				break;
			}
		}
		else
			break;
	}
}

void CPythonEventManager::SetEventSetWidth(int iIndex, int iWidth)
{
	if (!CheckEventSetIndex(iIndex))
		return;

	TEventSet* pEventSet = m_EventSetVector[iIndex];
	if (!pEventSet)
		return;

	pEventSet->iWidth = iWidth;
}

void CPythonEventManager::ProcessEventSet(TEventSet* pEventSet)
{
	if (pEventSet->isLock)
		return;

	script::TCmd ScriptCommand;

	IAbstractApplication& rApp = IAbstractApplication::GetSingleton();

	if (!pEventSet->ScriptGroup.GetCmd(ScriptCommand))
	{
		pEventSet->isLock = true;
		return;
	}

	int pEventPosition;
	int iEventType;
	if (!GetScriptEventIndex(ScriptCommand.name.c_str(), &pEventPosition, &iEventType))
		return;

	switch (iEventType)
	{
	case EVENT_TYPE_LETTER:
	{
		const std::string& c_rstValue = GetArgumentString("value", ScriptCommand.argList);
		pEventSet->strCurrentLine.append(c_rstValue);
		pEventSet->pCurrentTextLine->SetValueString(pEventSet->strCurrentLine);
		pEventSet->pCurrentTextLine->SetColor(pEventSet->CurrentColor.r, pEventSet->CurrentColor.g, pEventSet->CurrentColor.b);
		pEventSet->iCurrentLetter += c_rstValue.length();

		if (pEventSet->iCurrentLetter >= pEventSet->iRestrictedCharacterCount)
		{
			__InsertLine(*pEventSet);
		}

		pEventSet->lLastDelayTime = pEventSet->lWaitingTime;
		break;
	}

	case EVENT_TYPE_DELAY:
	{
		if (EVENT_POSITION_START == pEventPosition)
			pEventSet->lWaitingTime = atoi(GetArgument("value", ScriptCommand.argList));
		else
			pEventSet->lWaitingTime = c_lNormal_Waiting_Time;
		break;
	}

	case EVENT_TYPE_COLOR:
	{
		if (EVENT_POSITION_START == pEventPosition)
		{
			pEventSet->CurrentColor.r = std::stof(GetArgument("r", ScriptCommand.argList));
			pEventSet->CurrentColor.g = std::stof(GetArgument("g", ScriptCommand.argList));
			pEventSet->CurrentColor.b = std::stof(GetArgument("b", ScriptCommand.argList));
			pEventSet->CurrentColor.a = 1.0f;
		}
		else
		{
			pEventSet->CurrentColor.r = 1.0f;
			pEventSet->CurrentColor.g = 1.0f;
			pEventSet->CurrentColor.a = 1.0f;
			pEventSet->CurrentColor.b = 1.0f;
		}
		break;
	}

	case EVENT_TYPE_COLOR256:
	{
		if (EVENT_POSITION_START == pEventPosition)
		{
			pEventSet->CurrentColor.r = std::stof(GetArgument("r", ScriptCommand.argList)) / 255.0f;
			pEventSet->CurrentColor.g = std::stof(GetArgument("g", ScriptCommand.argList)) / 255.0f;
			pEventSet->CurrentColor.b = std::stof(GetArgument("b", ScriptCommand.argList)) / 255.0f;
			pEventSet->CurrentColor.a = 1.0f;
		}
		else
		{
			pEventSet->CurrentColor.r = 1.0f;
			pEventSet->CurrentColor.g = 1.0f;
			pEventSet->CurrentColor.a = 1.0f;
			pEventSet->CurrentColor.b = 1.0f;
		}
		break;
	}

	case EVENT_TYPE_ENTER:
	{
		__InsertLine(*pEventSet);
		break;
	}

	case EVENT_TYPE_WAIT:
	{
		//캐틱터 만들기 제국 설명등에서 설명 밑으로 쳐지는 버그 수정/EVENT_TYPE_WAIT 관련해서 테스트 필요.
		pEventSet->iyLocal = 0;
		pEventSet->isLock = true;
		break;
	}

	case EVENT_TYPE_NEXT:
	{
		MakeNextButton(pEventSet, BUTTON_TYPE_NEXT);
		pEventSet->iAdjustLine += 2;
		break;
	}

	case EVENT_TYPE_DONE:
	{
		MakeNextButton(pEventSet, BUTTON_TYPE_DONE);
		PyCallClassMemberFunc(pEventSet->poEventHandler, "DoneEvent", Py_BuildValue("()"));
		pEventSet->iAdjustLine += 2;
		break;
	}

	case EVENT_TYPE_CLEAR:
	{
		ClearLine(pEventSet);
		break;
	}

	case EVENT_TYPE_QUESTION:
	{
		MakeQuestion(pEventSet, ScriptCommand.argList);
		break;
	}

	case EVENT_TYPE_LEFT_IMAGE:
	{
		PyCallClassMemberFunc(pEventSet->poEventHandler, "OnLeftImage", Py_BuildValue("(s)", GetArgument("src", ScriptCommand.argList)));
		break;
	}

	case EVENT_TYPE_TOP_IMAGE:
	{
		PyCallClassMemberFunc(pEventSet->poEventHandler, "OnTopImage", Py_BuildValue("(s)", GetArgument("src", ScriptCommand.argList)));
		break;
	}

	case EVENT_TYPE_BACKGROUND_IMAGE:
	{
		PyCallClassMemberFunc(pEventSet->poEventHandler, "OnBackgroundImage", Py_BuildValue("(s)", GetArgument("src", ScriptCommand.argList)));
		break;
	}

	case EVENT_TYPE_IMAGE:
	{
		int x = atoi(GetArgument("x", ScriptCommand.argList));
		int y = atoi(GetArgument("y", ScriptCommand.argList));
		const char* src = GetArgument("src", ScriptCommand.argList);

		PyCallClassMemberFunc(pEventSet->poEventHandler, "OnImage", Py_BuildValue("(iis)", x, y, src));
		break;
	}

	case EVENT_TYPE_INSERT_IMAGE:
	{
		const std::string& imageFile = GetArgumentString("image_name", ScriptCommand.argList);
		const char* title = GetArgument("title", ScriptCommand.argList);
		const char* desc = GetArgument("desc", ScriptCommand.argList);
		int index = atoi(GetArgument("index", ScriptCommand.argList));
		int total = atoi(GetArgument("total", ScriptCommand.argList));

		if (imageFile.empty())
		{
			const char* imageType = GetArgument("image_type", ScriptCommand.argList);
			int iItemIndex = atoi(GetArgument("idx", ScriptCommand.argList));
			PyCallClassMemberFunc(pEventSet->poEventHandler, "OnInsertItemIcon", Py_BuildValue("(sissii)", imageType, iItemIndex, title, desc, index, total));
		}
		else
		{
			PyCallClassMemberFunc(pEventSet->poEventHandler, "OnInsertImage", Py_BuildValue("(ssssii)", imageFile.c_str(), title, title, desc, index, total));
		}
		pEventSet->iAdjustLine += 2;
		break;
	}

	case EVENT_TYPE_ADD_MAP_SIGNAL:
	{
		float x = std::stof(GetArgument("x", ScriptCommand.argList));
		float y = std::stof(GetArgument("y", ScriptCommand.argList));
		CPythonMiniMap::Instance().AddSignalPoint(x, y);
		CPythonMiniMap::Instance().OpenAtlasWindow();
		break;
	}

	case EVENT_TYPE_CLEAR_MAP_SIGNAL:
	{
		CPythonMiniMap::Instance().ClearAllSignalPoint();
		break;
	}

	// HIDE_QUEST_LETTER
	case EVENT_TYPE_QUEST_BUTTON_CLOSE:
	{
		PyCallClassMemberFunc(m_poInterface, "BINARY_ClearQuest",
			Py_BuildValue("(i)", atoi(GetArgument("idx", ScriptCommand.argList))));
		break;
	}
	// END_OF_HIDE_QUEST_LETTER

	case EVENT_TYPE_QUEST_BUTTON:
	{
		const std::string& c_rstType = GetArgumentString("icon_type", ScriptCommand.argList);
		const std::string& c_rstFile = GetArgumentString("icon_name", ScriptCommand.argList);

		int idx = atoi(GetArgument("idx", ScriptCommand.argList));
		const char* name = GetArgument("name", ScriptCommand.argList);

		// 퀘스트 UI 리뉴얼이 되면 해결 되므로 일단 용혼석만 땜빵 by chrislee
		if (!strcmp(name, "조각난 용혼석?....")) {
			PyCallClassMemberFunc(m_poInterface, "BINARY_RecvQuest", Py_BuildValue("(isss)", idx, name, "highlight", ""));
		}
		else {
			if (c_rstFile.empty())
			{
				PyCallClassMemberFunc(m_poInterface, "RecvQuest", Py_BuildValue("(is)", idx, name));
			}
			else
			{
				PyCallClassMemberFunc(m_poInterface, "BINARY_RecvQuest",
					Py_BuildValue("(isss)", idx, name, c_rstType.c_str(), c_rstFile.c_str()));
			}
		}
		break;
	}
	case EVENT_TYPE_SET_MESSAGE_POSITION:
	{
		break;
	}
	case EVENT_TYPE_ADJUST_MESSAGE_POSITION:
	{
		break;
	}
	case EVENT_TYPE_SET_CENTER_MAP_POSITION:
	{
		CPythonMiniMap::Instance().SetAtlasCenterPosition(atoi(GetArgument("x", ScriptCommand.argList)), atoi(GetArgument("y", ScriptCommand.argList)));
		break;
	}
	case EVENT_TYPE_SLEEP:
		pEventSet->lLastDelayTime = atoi(GetArgument("value", ScriptCommand.argList));
		break;
	case EVENT_TYPE_SET_CAMERA:
	{
		IAbstractApplication::SCameraSetting CameraSetting;
		GetCameraSettingFromArgList(ScriptCommand.argList, &CameraSetting);
		rApp.SetEventCamera(CameraSetting);
		break;
	}
	case EVENT_TYPE_BLEND_CAMERA:
	{
		IAbstractApplication::SCameraSetting CameraSetting;
		GetCameraSettingFromArgList(ScriptCommand.argList, &CameraSetting);

		float fBlendTime = atoi(GetArgument("blendtime", ScriptCommand.argList));

		rApp.BlendEventCamera(CameraSetting, fBlendTime);
		break;
	}
	case EVENT_TYPE_RESTORE_CAMERA:
	{
		rApp.SetDefaultCamera();
		break;
	}
	case EVENT_TYPE_FADE_OUT:
	{
		float fSpeed = std::stof(GetArgument("speed", ScriptCommand.argList));
		PyCallClassMemberFunc(pEventSet->poEventHandler, "FadeOut", Py_BuildValue("(f)", fSpeed));
		pEventSet->isWaitFlag = true;
		break;
	}
	case EVENT_TYPE_FADE_IN:
	{
		PyCallClassMemberFunc(pEventSet->poEventHandler, "FadeIn", Py_BuildValue("(f)", std::stof(GetArgument("speed", ScriptCommand.argList))));
		pEventSet->isWaitFlag = true;
		break;
	}
	case EVENT_TYPE_WHITE_OUT:
	{
		PyCallClassMemberFunc(pEventSet->poEventHandler, "WhiteOut", Py_BuildValue("(f)", std::stof(GetArgument("speed", ScriptCommand.argList))));
		pEventSet->isWaitFlag = true;
		break;
	}
	case EVENT_TYPE_WHITE_IN:
	{
		PyCallClassMemberFunc(pEventSet->poEventHandler, "WhiteIn", Py_BuildValue("(f)", std::stof(GetArgument("speed", ScriptCommand.argList))));
		pEventSet->isWaitFlag = true;
		break;
	}
	case EVENT_TYPE_CLEAR_TEXT:
	{
		ClearLine(pEventSet);
		break;
	}
	case EVENT_TYPE_TEXT_HORIZONTAL_ALIGN_CENTER:
	{
		pEventSet->isTextCenterMode = true;
		if (pEventSet->pCurrentTextLine)
			pEventSet->pCurrentTextLine->SetHorizonalAlign(CGraphicTextInstance::HORIZONTAL_ALIGN_CENTER);
		break;
	}
	case EVENT_TYPE_TITLE_IMAGE:
	{
		PyCallClassMemberFunc(pEventSet->poEventHandler, "OnTitleImage", Py_BuildValue("(s)", GetArgument("src", ScriptCommand.argList)));
		break;
	}
	case EVENT_TYPE_DUNGEON_RESULT:
	{
		int killstone_count = atoi(GetArgument("killstone_count", ScriptCommand.argList));
		int killmob_count = atoi(GetArgument("killmob_count", ScriptCommand.argList));
		int find_hidden = atoi(GetArgument("find_hidden", ScriptCommand.argList));
		int hidden_total = atoi(GetArgument("hidden_total", ScriptCommand.argList));
		int use_potion = atoi(GetArgument("use_potion", ScriptCommand.argList));
		int is_revived = atoi(GetArgument("is_revived", ScriptCommand.argList));
		int killallmob = atoi(GetArgument("killallmob", ScriptCommand.argList));
		int total_time = atoi(GetArgument("total_time", ScriptCommand.argList));
		int bonus_exp = atoi(GetArgument("bonus_exp", ScriptCommand.argList));

		PyCallClassMemberFunc(m_poInterface, "ShowDungeonResult",
			Py_BuildValue("(iiiiiiiii)",
				killstone_count,
				killmob_count,
				find_hidden,
				hidden_total,
				use_potion,
				is_revived,
				killallmob,
				total_time,
				bonus_exp));
		break;
	}
	case EVENT_TYPE_ITEM_NAME:
	{
		int iIndex = atoi(GetArgument("value", ScriptCommand.argList));
		CItemData* pItemData;
		if (CItemManager::Instance().GetItemDataPointer(iIndex, &pItemData))
		{
			pEventSet->strCurrentLine.append(pItemData->GetName());
			pEventSet->pCurrentTextLine->SetValue(pEventSet->strCurrentLine.c_str());
			pEventSet->pCurrentTextLine->SetColor(1.0f, 0.2f, 0.2f);
			pEventSet->iCurrentLetter += strlen(pItemData->GetName());
			if (pEventSet->iCurrentLetter >= pEventSet->iRestrictedCharacterCount)
				__InsertLine(*pEventSet);

			pEventSet->lLastDelayTime = pEventSet->lWaitingTime;
		}

		break;
	}
	case EVENT_TYPE_MONSTER_NAME:
	{
		int iIndex = atoi(GetArgument("value", ScriptCommand.argList));
		const char* c_szName;

		CPythonNonPlayer& rkNonPlayer = CPythonNonPlayer::Instance();
		if (rkNonPlayer.GetName(iIndex, &c_szName))
		{
			pEventSet->strCurrentLine.append(c_szName);
			pEventSet->pCurrentTextLine->SetValue(pEventSet->strCurrentLine.c_str());
			pEventSet->iCurrentLetter += strlen(c_szName);

			if (pEventSet->iCurrentLetter >= pEventSet->iRestrictedCharacterCount)
				__InsertLine(*pEventSet);

			pEventSet->lLastDelayTime = pEventSet->lWaitingTime;
		}

		break;
	}
	case EVENT_TYPE_WINDOW_SIZE:
	{
		int iWidth = atoi(GetArgument("width", ScriptCommand.argList));
		int iHeight = atoi(GetArgument("height", ScriptCommand.argList));
		PyCallClassMemberFunc(pEventSet->poEventHandler, "OnSize", Py_BuildValue("(ii)", iWidth, iHeight));
		break;
	}
	case EVENT_TYPE_INPUT:
	{
		__InsertLine(*pEventSet);
		PyCallClassMemberFunc(pEventSet->poEventHandler, "OnInput", Py_BuildValue("()"));
		break;
	}
	case EVENT_TYPE_CONFIRM_WAIT:
	{
		int iTimeOut = atoi(GetArgument("timeout", ScriptCommand.argList));
		pEventSet->isConfirmWait = TRUE;
		pEventSet->pConfirmTimeTextLine = pEventSet->pCurrentTextLine;
		pEventSet->iConfirmEndTime = timeGetTime() / 1000 + iTimeOut;
		__InsertLine(*pEventSet, TRUE);
		MakeNextButton(pEventSet, BUTTON_TYPE_CANCEL);
		break;
	}
	case EVENT_TYPE_END_CONFIRM_WAIT:
	{
		for (uint32_t i = 0; i < m_EventSetVector.size(); ++i)
		{
			if (nullptr == m_EventSetVector[i])
				continue;

			TEventSet* pSet = m_EventSetVector[i];
			if (!pSet->isConfirmWait)
				continue;

			pSet->isConfirmWait = FALSE;
			pSet->pConfirmTimeTextLine = nullptr;
			pSet->iConfirmEndTime = 0;

			PyCallClassMemberFunc(pSet->poEventHandler, "CloseSelf", Py_BuildValue("()"));
		}
		break;
	}
	case EVENT_TYPE_SELECT_ITEM:
	{
		PyCallClassMemberFunc(m_poInterface, "BINARY_OpenSelectItemWindow", Py_BuildValue("()"));
		break;
	}
	default: TraceError("valami rossz");
	}
}

void CPythonEventManager::RenderEventSet(int iIndex)
{
	if (!CheckEventSetIndex(iIndex))
		return;

	TEventSet* pEventSet = m_EventSetVector[iIndex];
	if (!pEventSet)
		return;

	int iCount = 0;

	for (TScriptTextLineList::iterator itor = pEventSet->ScriptTextLineList.begin(); itor != pEventSet->ScriptTextLineList.end(); ++itor, ++iCount)
	{
		if (iCount < pEventSet->iVisibleStartLine)
			continue;

		if (iCount >= pEventSet->iVisibleStartLine + pEventSet->iVisibleLineCount)
			continue;

		TTextLine& rkLine = *itor;
		CGraphicTextInstance* pInstance = rkLine.pInstance;

		pInstance->Update();
		pInstance->Render();
	}

	if (iCount >= pEventSet->iVisibleStartLine)
		if (iCount < pEventSet->iVisibleStartLine + pEventSet->iVisibleLineCount)
		{
			pEventSet->pCurrentTextLine->Update();
			pEventSet->pCurrentTextLine->Render();
		}
}

void CPythonEventManager::Skip(int iIndex)
{
	if (!CheckEventSetIndex(iIndex))
		return;

	TEventSet* pEventSet = m_EventSetVector[iIndex];

	if (true == pEventSet->isLock)
	{
		pEventSet->lLastDelayTime = 0;
		pEventSet->isLock = false;
	}
	else
	{
		pEventSet->lLastDelayTime = -1000;
	}
}

bool CPythonEventManager::IsWait(int iIndex)
{
	if (!CheckEventSetIndex(iIndex))
		return false;

	TEventSet* pEventSet = m_EventSetVector[iIndex];
	if (!pEventSet)
	{
		return false;
	}

	return pEventSet->isLock;
}

void CPythonEventManager::EndEventProcess(int iIndex)
{
	if (!CheckEventSetIndex(iIndex))
		return;

	TEventSet* pEventSet = m_EventSetVector[iIndex];
	if (!pEventSet)
	{
		return;
	}

	pEventSet->isWaitFlag = false;
}

void CPythonEventManager::MakeNextButton(TEventSet* pEventSet, int iButtonType)
{
	__AddSpace(*pEventSet, c_fLine_Temp + 5);
	PyCallClassMemberFunc(pEventSet->poEventHandler, "MakeNextButton", Py_BuildValue("(i)", iButtonType));
}

void CPythonEventManager::MakeQuestion(TEventSet* pEventSet, script::TArgList& rArgumentList)
{
	if (rArgumentList.empty())
		return;

	PyCallClassMemberFunc(pEventSet->poEventHandler, "MakeQuestion", Py_BuildValue("(i)", rArgumentList.size()));
	pEventSet->nAnswer = rArgumentList.size();

	int iIndex = 0;
	for (script::TArgList::iterator itor = rArgumentList.begin(); itor != rArgumentList.end(); ++itor)
	{
		script::TArg& rArgument = *itor;
		PyCallClassMemberFunc(pEventSet->poEventHandler, "AppendQuestion", Py_BuildValue("(si)", rArgument.strValue.c_str(), iIndex));
		++iIndex;
	}
}


void CPythonEventManager::SelectAnswer(int iIndex, int iAnswer)
{
	if (!CheckEventSetIndex(iIndex))
		return;

	//TEventSet * pEventSet = m_EventSetVector[iIndex];

	CPythonNetworkStream::Instance().SendScriptAnswerPacket(iAnswer);
}

void CPythonEventManager::SetVisibleStartLine(int iIndex, int iStartLine)
{
	if (!CheckEventSetIndex(iIndex))
		return;

	TEventSet* pEventSet = m_EventSetVector[iIndex];

	if (uint32_t(iStartLine) > pEventSet->ScriptTextLineList.size())
		return;

	pEventSet->iVisibleStartLine = iStartLine;
}

void CPythonEventManager::SetVisibleLineCount(int iIndex, int iLineCount)
{
	if (!CheckEventSetIndex(iIndex))
		return;

	TEventSet* pEventSet = m_EventSetVector[iIndex];

	pEventSet->iVisibleLineCount = iLineCount;
}

int CPythonEventManager::GetVisibleStartLine(int iIndex)
{
	if (!CheckEventSetIndex(iIndex))
		return 0;

	TEventSet* pEventSet = m_EventSetVector[iIndex];
	return pEventSet->iVisibleStartLine;
}

int CPythonEventManager::GetLineCount(int iIndex)
{
	if (!CheckEventSetIndex(iIndex))
		return 0;

	TEventSet* pEventSet = m_EventSetVector[iIndex];
	return pEventSet->ScriptTextLineList.size() + pEventSet->iAdjustLine;
}

void CPythonEventManager::ClearLine(TEventSet* pEventSet)
{
	if (!pEventSet)
		return;

	for (TScriptTextLineList::iterator itor = pEventSet->ScriptTextLineList.begin(); itor != pEventSet->ScriptTextLineList.end(); ++itor)
	{
		TTextLine& rkLine = *itor;
		CGraphicTextInstance* pInstance = rkLine.pInstance;
		pInstance->Destroy();
		pInstance->Update();
	}

	pEventSet->pCurrentTextLine->Destroy();
	pEventSet->pCurrentTextLine->Update();

	// clear
	pEventSet->pCurrentTextLine = nullptr;
	pEventSet->ScriptTextLineList.clear();

	__InsertLine(*pEventSet);
}

void CPythonEventManager::__InsertLine(TEventSet& rEventSet, bool isCenter, int iX_pos)
{
	if (rEventSet.pCurrentTextLine)
	{
		TTextLine kLine;
		if (CGraphicTextInstance::HORIZONTAL_ALIGN_CENTER == rEventSet.pCurrentTextLine->GetHorizontalAlign())
		{
			kLine.ixLocal = rEventSet.iWidth / 2;
			kLine.iyLocal = rEventSet.iyLocal;

		}
		else
		{
			int textWidth;
			int textHeight;
			rEventSet.pCurrentTextLine->GetTextSize(&textWidth, &textHeight);

			kLine.ixLocal = 0;
			if (iX_pos != 0)
			{
				kLine.ixLocal += (iX_pos - 20);
				kLine.ixLocal -= textWidth / 2;
			}


			kLine.iyLocal = rEventSet.iyLocal;
		}
		kLine.pInstance = rEventSet.pCurrentTextLine;
		rEventSet.ScriptTextLineList.push_back(kLine);
		__AddSpace(rEventSet, c_fLine_Temp);
	}

	// DEFAULT_FONT
	//CGraphicText * pText = (CGraphicText*)CResourceManager::Instance().GetTypeResourcePointer(g_strDefaultFontName.c_str());
	CGraphicText* pkDefaultFont = static_cast<CGraphicText*>(DefaultFont_GetResource());
	if (!pkDefaultFont)
	{
		TraceError("CPythonEventManager::InsertLine - CANNOT_FIND_DEFAULT_FONT");
		return;
	}

	rEventSet.pCurrentTextLine = m_ScriptTextLinePool.Alloc();
	if (!rEventSet.pCurrentTextLine)
	{
		TraceError("CPythonEventManager::InsertLine - OUT_OF_TEXT_LINE");
		return;
	}

	rEventSet.pCurrentTextLine->SetTextPointer(pkDefaultFont);
	// END_OF_DEFAULT_FONT

	rEventSet.pCurrentTextLine->SetColor(1.0f, 1.0f, 1.0f);
	rEventSet.pCurrentTextLine->SetValue("");

	if (rEventSet.isTextCenterMode || isCenter)
	{
		rEventSet.pCurrentTextLine->SetHorizonalAlign(CGraphicTextInstance::HORIZONTAL_ALIGN_CENTER);
		rEventSet.pCurrentTextLine->SetPosition(rEventSet.ix + rEventSet.iWidth / 2, rEventSet.iy + rEventSet.iyLocal);
	}
	else
	{
		rEventSet.pCurrentTextLine->SetHorizonalAlign(CGraphicTextInstance::HORIZONTAL_ALIGN_LEFT);
		rEventSet.pCurrentTextLine->SetPosition(rEventSet.ix, rEventSet.iy + rEventSet.iyLocal);
	}

	rEventSet.iCurrentLetter = 0;
	rEventSet.strCurrentLine = "";
}

void CPythonEventManager::RefreshLinePosition(TEventSet* pEventSet)
{
	//int iCount = 0;
	for (TScriptTextLineList::iterator itor = pEventSet->ScriptTextLineList.begin(); itor != pEventSet->ScriptTextLineList.end(); ++itor)
	{
		TTextLine& rkLine = *itor;
		CGraphicTextInstance* pInstance = rkLine.pInstance;
		pInstance->SetPosition(pEventSet->ix + rkLine.ixLocal, pEventSet->iy + rkLine.iyLocal);
	}

	int ixTextPos;
	if (CGraphicTextInstance::HORIZONTAL_ALIGN_CENTER == pEventSet->pCurrentTextLine->GetHorizontalAlign())
	{
		ixTextPos = pEventSet->ix + pEventSet->iWidth / 2;
	}
	else
	{
		ixTextPos = pEventSet->ix;
	}
	pEventSet->pCurrentTextLine->SetPosition(ixTextPos, pEventSet->iy + pEventSet->iyLocal);
}

void CPythonEventManager::__AddSpace(TEventSet& rEventSet, int iSpace)
{
	rEventSet.iyLocal += iSpace;
}

bool CPythonEventManager::GetScriptEventIndex(const char* c_szName, int* pEventPosition, int* pEventType)
{
	const char* c_szEventName;

	if ('/' == c_szName[0])
	{
		*pEventPosition = EVENT_POSITION_END;
		c_szEventName = &c_szName[1];
	}
	else
	{
		*pEventPosition = EVENT_POSITION_START;
		c_szEventName = &c_szName[0];
	}

	std::map<std::string, int>::iterator it = EventTypeMap.find(c_szEventName);
	if (it == EventTypeMap.end())
	{
		Tracef(" !! PARSING ERROR - Strange Command : %s\n", c_szEventName);
		return false;
	}

	*pEventType = it->second;

	return true;
}

bool CPythonEventManager::CheckEventSetIndex(int iIndex)
{
	if (iIndex < 0)
		return false;

	if ((uint32_t)iIndex >= m_EventSetVector.size())
		return false;

	return true;
}

void CPythonEventManager::Destroy()
{
	m_EventSetVector.clear();
	m_EventSetPool.Clear();
	m_ScriptTextLinePool.Clear();
}

void CPythonEventManager::SetInterfaceWindow(PyObject* poInterface)
{
	m_poInterface = poInterface;
}

void CPythonEventManager::SetLeftTimeString(const char* c_szString)
{
	m_strLeftTimeString = c_szString;
}
#ifdef ENABLE_MAP_TELEPORTER
void CPythonEventManager::LoadMapConfig()
{
	m_vecMapConf.clear();

	const void* pData;
	CMappedFile File;

	if (!CEterPackManager::instance().GetFromPack(File, "data/maps/map_config.txt", &pData))
	{
		TraceError("CANNOT LOAD MAP CONFIG : cannot find packfile.");
		return;
	}


	CMemoryTextFileLoader textFileLoader;
	textFileLoader.Bind(File.Size(), pData);

	const int phase_search_map = 0;
	const int phase_read = 1;


	int iPhase = 0;

	CTokenVector token;
	TMapConfig map;

	for (uint32_t i = 0; i < textFileLoader.GetLineCount(); ++i)
	{
		if (iPhase == phase_search_map)
		{
			std::string line = textFileLoader.GetLineString(i);
			if (line.find('{') != std::string::npos)
			{
				iPhase++;
				map = {};
			}

			continue;
		}


		if (iPhase == phase_read)
		{
			std::string line = textFileLoader.GetLineString(i);
			if (line.find('}') != std::string::npos)
			{
				iPhase--;
				m_vecMapConf.push_back(map);
				continue;
			}
		}

		token.clear();
		textFileLoader.SplitLine(i, &token, "\t");

		if (token.size() != 0)
			if (token.back().back() == '\n')
				token.back().erase(token.back().length() - 1);

#ifdef _DEBUG
		std::string dbg = "MAPCONF: TOKEN [";
		for (auto& prt : token)
		{
			dbg += " - ";
			dbg += prt;
		}
		dbg += " ]";
#endif

		if (token.size() < 2)
			continue;

		if (token[0] == "price")
		{
			map.price = (uint32_t)std::stoul(token[1].c_str(), nullptr, 10);
			continue;
		}


		if (token[0] == "items")
		{
			for (auto i1 = 1; i1 < token.size(); i1++)
				map.items.emplace_back((uint32_t)std::stoul(token[i1].c_str(), nullptr, 10));
			continue;
		}


		if (token[0] == "buttonx")
		{
			map.lButtonX = (uint32_t)std::stoul(token[1].c_str(), nullptr, 10);
			continue;
		}

		if (token[0] == "buttony")
		{
			map.lButtonY = (uint32_t)std::stoul(token[1].c_str(), nullptr, 10);
			continue;
		}


		if (token[0] == "title")
		{
			strncpy(map.szTitle, token[1].c_str(), sizeof(map.szTitle));
			continue;
		}


		if (token[0] == "desc")
		{
			strncpy(map.szDesc, token[1].c_str(), sizeof(map.szDesc));
			continue;
		}

		if (token[0] == "level")
		{
			map.iLevel = (int)std::stoul(token[1].c_str(), nullptr, 10);
			continue;
		}

		if (token[0] == "levelMax")
		{
			map.iLevelMax = (int)std::stoul(token[1].c_str(), nullptr, 10);
			continue;
		}
	}
}
int CPythonEventManager::GetMapCount()
{
	return m_vecMapConf.size();
}
uint32_t CPythonEventManager::GetMapPrice(int index)
{
	if (index < m_vecMapConf.size() && index >= 0)
		return m_vecMapConf[index].price;

	return 0;
}
const char* CPythonEventManager::GetMapTitle(int index)
{
	if (index < m_vecMapConf.size() && index >= 0)
		return m_vecMapConf[index].szTitle;

	return nullptr;
}
const char* CPythonEventManager::GetMapDesc(int index)
{
	if (index < m_vecMapConf.size() && index >= 0)
		return m_vecMapConf[index].szDesc;
	return nullptr;
}


void CPythonEventManager::GetMapItems(int index, MAPITEMS** ppData)
{
	*ppData = nullptr;

	if (index < m_vecMapConf.size() && index >= 0)
		*ppData = &m_vecMapConf[index].items;
}


void CPythonEventManager::GetMapButtonPosition(int index, int* x, int* y)
{
	*x = 0;
	*y = 0;

	if (index < m_vecMapConf.size() && index >= 0)
	{
		*x = m_vecMapConf[index].lButtonX;
		*y = m_vecMapConf[index].lButtonY;
	}
}

int CPythonEventManager::GetMapLevel(int index)
{
	if (index < m_vecMapConf.size() && index >= 0)
		return m_vecMapConf[index].iLevel;
	return 0;
}

int CPythonEventManager::GetMapLevelMax(int index)
{
	if (index < m_vecMapConf.size() && index >= 0)
		return m_vecMapConf[index].iLevelMax;
	return 0;
}
#endif

void CPythonEventManager::SetFontColor(int iIndex, float fRed, float fGreen, float fBlue)
{
	if (!CheckEventSetIndex(iIndex))
		return;

	TEventSet* pEventSet = m_EventSetVector[iIndex];
	if (!pEventSet)
		return;

	if (pEventSet->pCurrentTextLine)
	{
		pEventSet->CurrentColor.r = fRed;
		pEventSet->CurrentColor.g = fGreen;
		pEventSet->CurrentColor.b = fBlue;
		pEventSet->pCurrentTextLine->SetColor(pEventSet->CurrentColor.r, pEventSet->CurrentColor.g, pEventSet->CurrentColor.b);
	}
}

CPythonEventManager::CPythonEventManager()
	: m_isQuestConfirmWait(0), m_poInterface(nullptr), m_strLeftTimeString("")
{
	EventTypeMap["LETTER"] = EVENT_TYPE_LETTER;
	EventTypeMap["COLOR"] = EVENT_TYPE_COLOR;
	EventTypeMap["DELAY"] = EVENT_TYPE_DELAY;
	EventTypeMap["ENTER"] = EVENT_TYPE_ENTER;
	EventTypeMap["WAIT"] = EVENT_TYPE_WAIT;
	EventTypeMap["CLEAR"] = EVENT_TYPE_CLEAR;
	EventTypeMap["QUESTION"] = EVENT_TYPE_QUESTION;
	EventTypeMap["NEXT"] = EVENT_TYPE_NEXT;
	EventTypeMap["DONE"] = EVENT_TYPE_DONE;

	EventTypeMap["LEFTIMAGE"] = EVENT_TYPE_LEFT_IMAGE;
	EventTypeMap["TOPIMAGE"] = EVENT_TYPE_TOP_IMAGE;
	EventTypeMap["BGIMAGE"] = EVENT_TYPE_BACKGROUND_IMAGE;
	EventTypeMap["IMAGE"] = EVENT_TYPE_IMAGE;

	EventTypeMap["ADDMAPSIGNAL"] = EVENT_TYPE_ADD_MAP_SIGNAL;
	EventTypeMap["CLEARMAPSIGNAL"] = EVENT_TYPE_CLEAR_MAP_SIGNAL;

	EventTypeMap["SETMSGPOS"] = EVENT_TYPE_SET_MESSAGE_POSITION;
	EventTypeMap["ADJMSGPOS"] = EVENT_TYPE_ADJUST_MESSAGE_POSITION;
	EventTypeMap["SETCMAPPOS"] = EVENT_TYPE_SET_CENTER_MAP_POSITION;

	EventTypeMap["QUESTBUTTON"] = EVENT_TYPE_QUEST_BUTTON;

	// HIDE_QUEST_LETTER
	EventTypeMap["QUESTBUTTON_CLOSE"] = EVENT_TYPE_QUEST_BUTTON_CLOSE;
	// END_OF_HIDE_QUEST_LETTER

	EventTypeMap["SLEEP"] = EVENT_TYPE_SLEEP;
	EventTypeMap["SET_CAMERA"] = EVENT_TYPE_SET_CAMERA;
	EventTypeMap["BLEND_CAMERA"] = EVENT_TYPE_BLEND_CAMERA;
	EventTypeMap["RESTORE_CAMERA"] = EVENT_TYPE_RESTORE_CAMERA;
	EventTypeMap["FADE_OUT"] = EVENT_TYPE_FADE_OUT;
	EventTypeMap["FADE_IN"] = EVENT_TYPE_FADE_IN;
	EventTypeMap["WHITE_OUT"] = EVENT_TYPE_WHITE_OUT;
	EventTypeMap["WHITE_IN"] = EVENT_TYPE_WHITE_IN;
	EventTypeMap["CLEAR_TEXT"] = EVENT_TYPE_CLEAR_TEXT;
	EventTypeMap["TEXT_HORIZONTAL_ALIGN_CENTER"] = EVENT_TYPE_TEXT_HORIZONTAL_ALIGN_CENTER;
	EventTypeMap["TITLE_IMAGE"] = EVENT_TYPE_TITLE_IMAGE;

	EventTypeMap["RUN_CINEMA"] = EVENT_TYPE_RUN_CINEMA;
	EventTypeMap["DUNGEON_RESULT"] = EVENT_TYPE_DUNGEON_RESULT;

	EventTypeMap["ITEM"] = EVENT_TYPE_ITEM_NAME;
	EventTypeMap["MOB"] = EVENT_TYPE_MONSTER_NAME;

	EventTypeMap["COLOR256"] = EVENT_TYPE_COLOR256;
	EventTypeMap["WINDOW_SIZE"] = EVENT_TYPE_WINDOW_SIZE;

	EventTypeMap["INPUT"] = EVENT_TYPE_INPUT;
	EventTypeMap["CONFIRM_WAIT"] = EVENT_TYPE_CONFIRM_WAIT;
	EventTypeMap["END_CONFIRM_WAIT"] = EVENT_TYPE_END_CONFIRM_WAIT;

	EventTypeMap["INSERT_IMAGE"] = EVENT_TYPE_INSERT_IMAGE;

	EventTypeMap["SELECT_ITEM"] = EVENT_TYPE_SELECT_ITEM;
#ifdef ENABLE_MAP_TELEPORTER
	LoadMapConfig();
#endif
}

CPythonEventManager::~CPythonEventManager() = default;
