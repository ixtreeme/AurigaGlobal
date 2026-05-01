#include "stdafx.h"
#include <Core/Logging.hpp>
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "utils.h"
#include "config.h"
#include "questmanager.h"
#include "sectree_manager.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "ecs/CharacterAccessors.hpp"
#include "desc.h"
#include "packet.h"
#include "target.h"

/////////////////////////////////////////////////////////////////////
// Packet
/////////////////////////////////////////////////////////////////////
void SendTargetCreatePacket(LPDESC d, TargetInfo * info)
{
if (!info->bSendToClient)
return;

TPacketGCTargetCreate pck;

pck.bHeader = HEADER_GC_TARGET_CREATE;
pck.lID = info->iID;
pck.bType = info->iType;
pck.dwVID = info->iArg1;
strlcpy(pck.szName, info->szTargetDesc, sizeof(pck.szName));
d->Packet(&pck, sizeof(TPacketGCTargetCreate));
}

void SendTargetUpdatePacket(LPDESC d, int iID, int x, int y)
{
TPacketGCTargetUpdate pck;
pck.bHeader = HEADER_GC_TARGET_UPDATE;
pck.lID = iID;
pck.lX = x;
pck.lY = y;
d->Packet(&pck, sizeof(TPacketGCTargetUpdate));
LOG_TRACE("SendTargetUpdatePacket {} {}x{}", iID, x, y);
}

void SendTargetDeletePacket(LPDESC d, int iID)
{
TPacketGCTargetDelete pck;
pck.bHeader = HEADER_GC_TARGET_DELETE;
pck.lID = iID;
d->Packet(&pck, sizeof(TPacketGCTargetDelete));
}
/////////////////////////////////////////////////////////////////////
CTargetManager::CTargetManager() : m_iID(0)
{
}

CTargetManager::~CTargetManager()
{
}

EVENTFUNC(target_event)
{
	TargetInfo * info = dynamic_cast<TargetInfo *>( event->info );

	if ( info == nullptr)
	{
		LOG_ERROR("target_event> <Factor> Null pointer");
		return 0;
	}

// <Factor> Raplaced direct pointer reference with key searching.
	//LPCHARACTER pkChr = info->pkChr;
	LPCHARACTER pkChr = CHARACTER_MANAGER::instance().FindByPID(info->dwPID);
	if (pkChr == nullptr) {
		return 0; // <Factor> need to be confirmed
	}
	LPCHARACTER tch = nullptr;
	int x = 0, y = 0;
	int iDist = 5000;

	if (info->iMapIndex != ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(pkChr)))
		return MINMAX(passes_per_sec / 2, iDist / (1500 / passes_per_sec), passes_per_sec * 5);

	switch (info->iType)
	{
		case TARGET_TYPE_POS:
			x = info->iArg1;
			y = info->iArg2;
			iDist = DISTANCE_APPROX(ecs::PlayerRuntime::GetX(AIHelpers::EcsOf(pkChr)) - x, ecs::PlayerRuntime::GetY(AIHelpers::EcsOf(pkChr)) - y);
			break;

		case TARGET_TYPE_VID:
			{
				tch = CHARACTER_MANAGER::instance().Find(info->iArg1);

				if (tch && ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(tch)) == ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(pkChr)))
				{
					x = ecs::PlayerRuntime::GetX(AIHelpers::EcsOf(tch));
					y = ecs::PlayerRuntime::GetY(AIHelpers::EcsOf(tch));
					iDist = DISTANCE_APPROX(ecs::PlayerRuntime::GetX(AIHelpers::EcsOf(pkChr)) - x, ecs::PlayerRuntime::GetY(AIHelpers::EcsOf(pkChr)) - y);
				}
			}
			break;
	}

	bool bRet = true;

	if (iDist <= 500)
		bRet = quest::CQuestManager::instance().Target((ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(pkChr))), info->dwQuestIndex, info->szTargetName, "arrive");

	if (!tch && info->iType == TARGET_TYPE_VID)
	{
		quest::CQuestManager::instance().Target((ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(pkChr))), info->dwQuestIndex, info->szTargetName, "die");
		CTargetManager::instance().DeleteTarget((ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(pkChr))), info->dwQuestIndex, info->szTargetName);
	}

	if (event->is_force_to_end)
	{
		LOG_TRACE("target_event: event canceled");
		return 0;
	}

	if (x != info->iOldX || y != info->iOldY)
	{
		if (info->bSendToClient)
			SendTargetUpdatePacket(ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(pkChr)), info->iID, x, y);

		info->iOldX = x;
		info->iOldY = y;
	}

	if (!bRet)
		return passes_per_sec;
	else
		return MINMAX(passes_per_sec / 2, iDist / (1500 / passes_per_sec), passes_per_sec * 5);
}

void CTargetManager::CreateTarget(uint32_t dwPID,
		uint32_t dwQuestIndex,
		const char * c_pszTargetName,
		int iType,
		int iArg1,
		int iArg2,
		int iMapIndex,
		const char * c_pszTargetDesc,
		int iSendFlag)
{
	LOG_TRACE("CreateTarget : target pid {} quest {} name {} arg {} {} {}", dwPID, dwQuestIndex, c_pszTargetName, iType, iArg1, iArg2);

	LPCHARACTER pkChr = CHARACTER_MANAGER::instance().FindByPID(dwPID);

	if (!pkChr)
	{
		LOG_ERROR("Cannot find character ptr by PID {}", dwPID);
		return;
	}

	if (ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(pkChr)) != iMapIndex)
		return;

	auto it = m_map_kListEvent.find(dwPID);

	if (it != m_map_kListEvent.end())
	{
		std::list<LPEVENT>::const_iterator it2 = it->second.begin();

		while (it2 != it->second.end())
		{
			LPEVENT pkEvent = *(it2++);
			TargetInfo* existInfo = dynamic_cast<TargetInfo*>(pkEvent->info);

			if (nullptr == existInfo)
			{
				LOG_ERROR("CreateTarget : event already exist, but have no info");
				return;
			}

			if (existInfo->dwQuestIndex == dwQuestIndex && !strcmp(existInfo->szTargetName, c_pszTargetName))
			{
				LOG_TRACE("CreateTarget : same target will be replaced");

				if (existInfo->bSendToClient)
					SendTargetDeletePacket(ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(pkChr)), existInfo->iID);

				if (c_pszTargetDesc)
				{
					strlcpy(existInfo->szTargetDesc, c_pszTargetDesc, sizeof(existInfo->szTargetDesc));
				}
				else
				{
					*existInfo->szTargetDesc = '\0';
				}

				existInfo->iID = ++m_iID;
				existInfo->iType = iType;
				existInfo->iArg1 = iArg1;
				existInfo->iArg2 = iArg2;
				existInfo->iOldX = 0;
				existInfo->iOldY = 0;
				existInfo->bSendToClient = iSendFlag ? true : false;

				SendTargetCreatePacket(ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(pkChr)), existInfo);
				return;
			}
		}
	}

	TargetInfo* newInfo = AllocEventInfo<TargetInfo>();

	if (c_pszTargetDesc)
	{
		strlcpy(newInfo->szTargetDesc, c_pszTargetDesc, sizeof(newInfo->szTargetDesc));
	}
	else
	{
		*newInfo->szTargetDesc = '\0';
	}

	newInfo->iID = ++m_iID;
	// <Factor> Removed pkChr
	//newInfo->pkChr = pkChr;
	newInfo->dwPID = dwPID;
	newInfo->dwQuestIndex = dwQuestIndex;
	strlcpy(newInfo->szTargetName, c_pszTargetName, sizeof(newInfo->szTargetName));
	newInfo->iType = iType;
	newInfo->iArg1 = iArg1;
	newInfo->iArg2 = iArg2;
	newInfo->iMapIndex = iMapIndex;
	newInfo->iOldX = 0;
	newInfo->iOldY = 0;
	newInfo->bSendToClient = iSendFlag ? true : false;

	LPEVENT event = event_create(target_event, newInfo, PASSES_PER_SEC(1));

	if (nullptr != event)
	{
		m_map_kListEvent[dwPID].push_back(event);

		SendTargetCreatePacket(ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(pkChr)), newInfo);
	}
}

void CTargetManager::DeleteTarget(uint32_t dwPID, uint32_t dwQuestIndex, const char * c_pszTargetName)
{
	auto it = m_map_kListEvent.find(dwPID);

	if (it == m_map_kListEvent.end())
		return;

	std::list<LPEVENT>::iterator it2 = it->second.begin();

	while (it2 != it->second.end())
	{
		LPEVENT pkEvent = *it2;
		TargetInfo * info = dynamic_cast<TargetInfo*>(pkEvent->info);

		if ( info == nullptr)
		{
			LOG_ERROR("CTargetManager::DeleteTarget> <Factor> Null pointer");
			++it2;
			continue;
		}

		if (dwQuestIndex == info->dwQuestIndex)
		{
			if (!c_pszTargetName || !strcmp(info->szTargetName, c_pszTargetName))
			{
				if (info->bSendToClient) {
					// <Factor> Removed pkChr
					// SendTargetDeletePacket for info->pkChr was replaced with PlayerRuntime::GetDesc.
					LPCHARACTER pkChr = CHARACTER_MANAGER::instance().FindByPID(info->dwPID);
					if (pkChr != nullptr) {
						SendTargetDeletePacket(ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(pkChr)), info->iID);
					}
				}

				event_cancel(&pkEvent);
				it2 = it->second.erase(it2);
				continue;
			}
		}

		it2++;
	}
}

LPEVENT CTargetManager::GetTargetEvent(uint32_t dwPID, uint32_t dwQuestIndex, const char * c_pszTargetName)
{
	auto it = m_map_kListEvent.find(dwPID);

	if (it == m_map_kListEvent.end())
		return nullptr;

	std::list<LPEVENT>::iterator it2 = it->second.begin();

	while (it2 != it->second.end())
	{
		LPEVENT pkEvent = *(it2++);
		TargetInfo * info = dynamic_cast<TargetInfo*>(pkEvent->info);

		if ( info == nullptr)
		{
			LOG_ERROR("CTargetManager::GetTargetEvent> <Factor> Null pointer");

			continue;
		}

		if (info->dwQuestIndex != dwQuestIndex)
			continue;

		if (strcmp(info->szTargetName, c_pszTargetName))
			continue;

		return pkEvent;
	}

	return nullptr;
}

TargetInfo * CTargetManager::GetTargetInfo(uint32_t dwPID, int iType, int iArg1)
{
	auto it = m_map_kListEvent.find(dwPID);

	if (it == m_map_kListEvent.end())
		return nullptr;

	std::list<LPEVENT>::iterator it2 = it->second.begin();

	while (it2 != it->second.end())
	{
		LPEVENT pkEvent = *(it2++);
		TargetInfo * info = dynamic_cast<TargetInfo*>(pkEvent->info);

		if ( info == nullptr)
		{
			LOG_ERROR("CTargetManager::GetTargetInfo> <Factor> Null pointer");

			continue;
		}

		if (!IS_SET(info->iType, iType))
			continue;

		if (info->iArg1 != iArg1)
			continue;

		return info;
	}

	return nullptr;
}

void CTargetManager::Logout(uint32_t dwPID)
{
	auto it = m_map_kListEvent.find(dwPID);

	if (it == m_map_kListEvent.end())
		return;

	std::list<LPEVENT>::iterator it2 = it->second.begin();

	while (it2 != it->second.end())
		event_cancel(&(*(it2++)));

	m_map_kListEvent.erase(it);
}


