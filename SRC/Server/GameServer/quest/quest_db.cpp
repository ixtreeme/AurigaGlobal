#include "stdafx.h"
#include <Core/Logging.hpp>
#include "input.h"
#include "../ecs/Registry.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "char.h"
#include "packet.h"
#include "protocol.h"
#include "utils.h"
#include "config.h"
#include "questmanager.h"
#include "../ecs/CharacterAccessors.hpp"
#include "char_manager.h"
#include "desc.h"
#include "event.h"


EVENTINFO(quest_login_event_info)
{
	uint32_t dwPID;

	quest_login_event_info()
	: dwPID( 0 )
	{
	}
};

EVENTFUNC(quest_login_event)
{
	quest_login_event_info* info = dynamic_cast<quest_login_event_info*>( event->info );

	if ( info == nullptr)
	{
		LOG_ERROR("quest_login_event> <Factor> Null pointer");
		return 0;
	}

	uint32_t dwPID = info->dwPID;

	const entt::entity ch = CHARACTER_MANAGER::instance().FindEntityByPID(dwPID);

	if (ch == entt::null)
		return 0;

	LPDESC d = ecs::PlayerRuntime::GetDesc(ch);

	if (!d)
		return 0;

	if (d->IsPhase(PHASE_HANDSHAKE) ||
		d->IsPhase(PHASE_LOGIN) ||
		d->IsPhase(PHASE_SELECT) ||
		d->IsPhase(PHASE_DEAD) ||
		d->IsPhase(PHASE_LOADING))
	{
		return PASSES_PER_SEC(1);
	}
	else if (d->IsPhase(PHASE_CLOSE))
	{
		return 0;
	}
	else if (d->IsPhase(PHASE_GAME))
	{
		LOG_INFO("QUEST_LOAD: Login pc {} by event", (ecs::PlayerRuntime::GetPlayerID(ch)));
		quest::CQuestManager::instance().Login((ecs::PlayerRuntime::GetPlayerID(ch)));
		return 0;
	}
	else
	{
		LOG_ERROR("input_db.cpp:quest_login_event INVALID PHASE pid {}", (ecs::PlayerRuntime::GetPlayerID(ch)));
		return 0;
	}
}

void CInputDB::QuestLoad(LPDESC d, const char * c_pData)
{
	if (nullptr == d)
		return;

	const entt::entity chEntity = d->GetEntity();
	if (!ecs::IsCharacter(chEntity))
		return;

	const uint32_t dwCount = decode_4bytes(c_pData);

	const TQuestTable* pQuestTable = reinterpret_cast<const TQuestTable*>(c_pData+4);

	if (nullptr != pQuestTable)
	{
		if (dwCount != 0)
		{
			if ((ecs::PlayerRuntime::GetPlayerID(chEntity)) != pQuestTable[0].dwPID)
			{
				LOG_ERROR("PID differs {} {}", (ecs::PlayerRuntime::GetPlayerID(chEntity)), pQuestTable[0].dwPID);
				return;
			}
		}

		LOG_INFO("QUEST_LOAD: count {}", dwCount);

		quest::PC * pkPC = quest::CQuestManager::instance().GetPCForce((ecs::PlayerRuntime::GetPlayerID(chEntity)));

		if (!pkPC)
		{
			LOG_ERROR("null quest::PC with id {}", pQuestTable[0].dwPID);
			return;
		}

		if (pkPC->IsLoaded())
			return;

		for (unsigned int i = 0; i < dwCount; ++i)
		{
			std::string st(pQuestTable[i].szName);

			st += ".";
			st += pQuestTable[i].szState;

			LOG_INFO("            {} {}", st.c_str(), pQuestTable[i].lValue);
#ifdef ENABLE_QUEST_SYSTEM_BUGFIXES
			int val = pQuestTable[i].lValue;
			bool skipSave = true;


				if (!strcmp(pQuestTable[i].szState, "__status"))
				 {
				const char* stateName = quest::CQuestManager::instance().GetQuestStateName(pQuestTable[i].szName, val);
				if (!stateName || !*stateName)
					 {
					const int startIdx = quest::CQuestManager::instance().GetQuestStateIndex(pQuestTable[i].szName, "start");
					LOG_ERROR("QUEST __status invalid: pid={} quest={} val={} -> start={}", +pQuestTable[i].dwPID, pQuestTable[i].szName, val, startIdx);
					val = startIdx ? startIdx : 0; // 0 -> DeleteFlag
					skipSave = false;
					}
				 }

				pkPC->SetFlag(st, val, skipSave);
#else
			pkPC->SetFlag(st, pQuestTable[i].lValue, false);
#endif
		}

		pkPC->SetLoaded();
		pkPC->Build();

		if (ecs::PlayerRuntime::GetDesc(chEntity)->IsPhase(PHASE_GAME))
		{
			LOG_INFO("QUEST_LOAD: Login pc {}", pQuestTable[0].dwPID);
			quest::CQuestManager::instance().Login(pQuestTable[0].dwPID);
		}
		else
		{
			quest_login_event_info* info = AllocEventInfo<quest_login_event_info>();
			info->dwPID = (ecs::PlayerRuntime::GetPlayerID(chEntity));

			event_create(quest_login_event, info, PASSES_PER_SEC(1));
		}
	}
}
