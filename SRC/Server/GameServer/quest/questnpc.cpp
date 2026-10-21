#include "stdafx.h"
#include <Core/Logging.hpp>
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/AIHelpers.hpp"
#include <fstream>
#include <sstream>
#include "questmanager.h"
#include "quest_runtime.h"
#include "profiler.h"
#include "config.h"
#include "char_interface.hpp"
#ifdef ENABLE_MULTI_LANGUAGE
#include "desc.h"
#include "Core/Logging.hpp"
#endif
// questpc.h: PC::typedef Quest
// questpc.h: PC::typedef map<unsigned long, QuestState> QuestInfo;
// typedef


namespace quest
{
	NPC::NPC()
	{
		m_vnum = 0;
	}

	NPC::~NPC()
	{
	}

	void NPC::Set(unsigned int vnum, const string & script_name)
	{
		m_vnum = vnum;
        for (auto& events : m_mapOwnQuest) events.clear();
        for (auto& events : m_mapOwnArgQuest) events.clear();

        auto& manager = CQuestManager::instance();
        // A numeric target may also have a human-readable alias in questnpc.json.
        const auto numeric = std::to_string(vnum);
        for (const auto& definition : manager.GetQuestSources()) {
            const auto questIndex = manager.GetQuestIndexByName(definition.name);
            for (const auto& handler : definition.handlers) {
                if (handler.target != script_name && !(vnum && handler.target == numeric)) continue;
                const auto event = manager.m_mapEventName.find(handler.event);
                if (event == manager.m_mapEventName.end()) continue; // validated at startup
                const int state = definition.states.at(handler.state);
                if (handler.argument.empty()) {
                    auto& code = m_mapOwnQuest[event->second][questIndex][state].m_code;
                    const std::string body = source::HandlerCode(handler);
                    code.insert(code.end(), body.begin(), body.end());
                } else {
                    AArgScript argument;
                    argument.arg = handler.argument;
                    argument.when_condition.assign(handler.condition.begin(), handler.condition.end());
                    argument.script.m_code.assign(handler.code.begin(), handler.code.end());
                    argument.script.m_code.push_back('\n');
                    argument.quest_index = questIndex;
                    argument.state_index = state;
                    m_mapOwnArgQuest[event->second][questIndex][state].push_back(std::move(argument));
                }
            }
        }
    }

	bool NPC::OnEnterState(PC& pc, uint32_t quest_index, int state)
	{
        auto& runtimeManager = CQuestManager::instance();
        RuntimeScope runtimeScope(runtimeManager, runtimeManager.CurrentRuntime(), true);
		return ExecuteEventScript(pc, QUEST_ENTER_STATE_EVENT, quest_index, state);
	}

	bool NPC::OnLeaveState(PC& pc, uint32_t quest_index, int state)
	{
        auto& runtimeManager = CQuestManager::instance();
        RuntimeScope runtimeScope(runtimeManager, runtimeManager.CurrentRuntime(), true);
		return ExecuteEventScript(pc, QUEST_LEAVE_STATE_EVENT, quest_index, state);
	}

	bool NPC::OnLetter(PC& pc, uint32_t quest_index, int state)
	{
        auto& runtimeManager = CQuestManager::instance();
        RuntimeScope runtimeScope(runtimeManager, runtimeManager.CurrentRuntime(), true);
		return ExecuteEventScript(pc, QUEST_LETTER_EVENT, quest_index, state);
	}

	bool NPC::OnTarget(PC & pc, uint32_t dwQuestIndex, const char * c_pszTargetName, const char * c_pszVerb, bool & bRet)
	{
        auto& runtimeManager = CQuestManager::instance();
        RuntimeScope runtimeScope(runtimeManager, runtimeManager.CurrentRuntime(), true);
		LOG_INFO("OnTarget begin {} verb {} qi {}", c_pszTargetName, c_pszVerb, dwQuestIndex);

		bRet = false;

		PC::QuestInfoIterator itPCQuest = pc.quest_find(dwQuestIndex);

		if (itPCQuest == pc.quest_end())
		{
			LOG_INFO("no quest");
			return false;
		}

		int iState = itPCQuest->second.st;

		AArgQuestScriptType & r = m_mapOwnArgQuest[QUEST_TARGET_EVENT][dwQuestIndex];
		AArgQuestScriptType::iterator it = r.find(iState);

		if (it == r.end())
		{
			LOG_INFO("no target event, state {}", iState);
			return false;
		}

		vector<AArgScript>::iterator it_vec = it->second.begin();

		int iTargetLen = strlen(c_pszTargetName);

		while (it_vec != it->second.end())
		{
			AArgScript & argScript = *(it_vec++);
			const char * c_pszArg = argScript.arg.c_str();

			LOG_INFO("OnTarget compare {} {}", c_pszArg, argScript.arg.length());

			if (strncmp(c_pszArg, c_pszTargetName, iTargetLen))
				continue;

			const char * c_pszArgVerb = strchr(c_pszArg, '.');

			if (!c_pszArgVerb)
				continue;

			if (strcmp(++c_pszArgVerb, c_pszVerb))
				continue;

			if (argScript.when_condition.size() > 0)
				LOG_INFO("OnTarget when {} size {}", &argScript.when_condition[0], argScript.when_condition.size());

			if (argScript.when_condition.size() != 0 && !IsScriptTrue(&argScript.when_condition[0], argScript.when_condition.size()))
				continue;

			LOG_INFO("OnTarget execute qi {} st {} code {}", dwQuestIndex, iState, (const char *) argScript.script.GetCode());
			bRet = CQuestManager::ExecuteQuestScript(pc, dwQuestIndex, iState, argScript.script.GetCode(), argScript.script.GetSize());
			bRet = true;
			return true;
		}

		return false;
	}

	bool NPC::OnAttrIn(PC& pc)
	{
        auto& runtimeManager = CQuestManager::instance();
        RuntimeScope runtimeScope(runtimeManager, runtimeManager.CurrentRuntime(), true);
		return HandleEvent(pc, QUEST_ATTR_IN_EVENT);
	}

	bool NPC::OnAttrOut(PC& pc)
	{
        auto& runtimeManager = CQuestManager::instance();
        RuntimeScope runtimeScope(runtimeManager, runtimeManager.CurrentRuntime(), true);
		return HandleEvent(pc, QUEST_ATTR_OUT_EVENT);
	}

	bool NPC::OnTakeItem(PC& pc)
	{
        auto& runtimeManager = CQuestManager::instance();
        RuntimeScope runtimeScope(runtimeManager, runtimeManager.CurrentRuntime(), true);
		return HandleEvent(pc, QUEST_ITEM_TAKE_EVENT);
	}

	bool NPC::OnUseItem(PC& pc, bool bReceiveAll)
	{
        auto& runtimeManager = CQuestManager::instance();
        RuntimeScope runtimeScope(runtimeManager, runtimeManager.CurrentRuntime(), true);
		if (bReceiveAll)
			return HandleReceiveAllEvent(pc, QUEST_ITEM_USE_EVENT);
		else
			return HandleEvent(pc, QUEST_ITEM_USE_EVENT);
	}

	bool NPC::OnSIGUse(PC& pc, bool bReceiveAll)
	{
        auto& runtimeManager = CQuestManager::instance();
        RuntimeScope runtimeScope(runtimeManager, runtimeManager.CurrentRuntime(), true);
		if (bReceiveAll)
			return HandleReceiveAllEvent(pc, QUEST_SIG_USE_EVENT);
		else
			return HandleEvent(pc, QUEST_SIG_USE_EVENT);
	}

	bool NPC::OnClick(PC& pc)
	{
        auto& runtimeManager = CQuestManager::instance();
        RuntimeScope runtimeScope(runtimeManager, runtimeManager.CurrentRuntime(), true);
		return HandleEvent(pc, QUEST_CLICK_EVENT);
	}

	bool NPC::OnServerTimer(PC& pc)
	{
        auto& runtimeManager = CQuestManager::instance();
        RuntimeScope runtimeScope(runtimeManager, runtimeManager.CurrentRuntime(), true);
		return HandleReceiveAllEvent(pc, QUEST_SERVER_TIMER_EVENT);
	}

	bool NPC::OnTimer(PC& pc)
	{
        auto& runtimeManager = CQuestManager::instance();
        RuntimeScope runtimeScope(runtimeManager, runtimeManager.CurrentRuntime(), true);
		return HandleEvent(pc, QUEST_TIMER_EVENT);
	}

	bool NPC::OnKill(PC & pc)
	{
        auto& runtimeManager = CQuestManager::instance();
        RuntimeScope runtimeScope(runtimeManager, runtimeManager.CurrentRuntime(), true);
		//PROF_UNIT puOnKill("quest::NPC::OnKill");
		if (m_vnum)
		{
			//PROF_UNIT puOnKill1("onk1");
			return HandleEvent(pc, QUEST_KILL_EVENT);
		}
		else
		{
			//PROF_UNIT puOnKill2("onk2");
			return HandleReceiveAllEvent(pc, QUEST_KILL_EVENT);
		}
	}

#ifdef __DUNGEON_INFO_SYSTEM__
	bool NPC::OnQuestDamage(PC& pc)
	{
        auto& runtimeManager = CQuestManager::instance();
        RuntimeScope runtimeScope(runtimeManager, runtimeManager.CurrentRuntime(), true);
		if (m_vnum == 0)
			return HandleReceiveAllEvent(pc, QUEST_DAMAGE_EVENT);
		else
			return HandleEvent(pc, QUEST_DAMAGE_EVENT);
	}
#endif

	bool NPC::OnPartyKill(PC & pc)
	{
        auto& runtimeManager = CQuestManager::instance();
        RuntimeScope runtimeScope(runtimeManager, runtimeManager.CurrentRuntime(), true);
		if (m_vnum)
		{
			return HandleEvent(pc, QUEST_PARTY_KILL_EVENT);
		}
		else
		{
			return HandleReceiveAllEvent(pc, QUEST_PARTY_KILL_EVENT);
		}
	}

#ifdef ENABLE_QUEST_DIE_EVENT
	bool NPC::OnDie(PC& pc)
	{
        auto& runtimeManager = CQuestManager::instance();
        RuntimeScope runtimeScope(runtimeManager, runtimeManager.CurrentRuntime(), true);
		return HandleReceiveAllEvent(pc, QUEST_DIE_EVENT);
	}
#endif

	bool NPC::OnLevelUp(PC& pc)
	{
        auto& runtimeManager = CQuestManager::instance();
        RuntimeScope runtimeScope(runtimeManager, runtimeManager.CurrentRuntime(), true);
		return HandleReceiveAllEvent(pc, QUEST_LEVELUP_EVENT);
	}

	bool NPC::OnLogin(PC& pc, const char * c_pszQuestName)
	{
        auto& runtimeManager = CQuestManager::instance();
        RuntimeScope runtimeScope(runtimeManager, runtimeManager.CurrentRuntime(), true);
		/*
		   if (c_pszQuestName)
		   {
		   uint32_t dwQI = CQuestManager::instance().GetQuestIndexByName(c_pszQuestName);

		   if (dwQI)
		   {
		   std::string stQuestName(c_pszQuestName);

		   CQuestManager & q = CQuestManager::instance();

		   QuestMapType::iterator qmit = m_mapOwnQuest[QUEST_LOGIN_EVENT].begin();

		   while (qmit != m_mapOwnQuest[QUEST_LOGIN_EVENT].end())
		   {
		   if (qmit->first != dwQI)
		   {
		   ++qmit;
		   continue;
		   }

		   int iState = pc.GetFlag(stQuestName + "__status");

		   AQuestScriptType::iterator qsit;

		   if ((qsit = qmit->second.find(iState)) != qmit->second.end())
		   {
		   return q.ExecuteQuestScript(pc, stQuestName, iState, qsit->second.GetCode(), qsit->second.GetSize(), NULL, true);
		   }

		   ++qmit;
		   }

		   LOG_ERROR("Cannot find any code for {}", c_pszQuestName);
		   }
		   else
		   LOG_ERROR("Cannot find quest index by {}", c_pszQuestName);
		   }
		 */
		bool bRet = HandleReceiveAllNoWaitEvent(pc, QUEST_LOGIN_EVENT);
		HandleReceiveAllEvent(pc, QUEST_LETTER_EVENT);
		return bRet;
	}

	bool NPC::OnLogout(PC& pc)
	{
        auto& runtimeManager = CQuestManager::instance();
        RuntimeScope runtimeScope(runtimeManager, runtimeManager.CurrentRuntime(), true);
		return HandleReceiveAllEvent(pc, QUEST_LOGOUT_EVENT);
	}

	bool NPC::OnUnmount(PC& pc)
	{
        auto& runtimeManager = CQuestManager::instance();
        RuntimeScope runtimeScope(runtimeManager, runtimeManager.CurrentRuntime(), true);
		return HandleReceiveAllEvent(pc, QUEST_UNMOUNT_EVENT);
	}

	struct FuncMissHandleEvent
	{
		std::vector <uint32_t> vdwNewStartQuestIndices;
		int size;

		FuncMissHandleEvent() : vdwNewStartQuestIndices(0), size(0)
		{}

		bool Matched()
		{
			return vdwNewStartQuestIndices.size() != 0;
		}

		void operator()(PC::QuestInfoIterator& itPCQuest, NPC::QuestMapType::iterator& itQuestMap)
		{
			uint32_t dwQuestIndex = itQuestMap->first;

			if (NPC::HasStartState(itQuestMap->second) && CQuestManager::instance().CanStartQuest(dwQuestIndex))
			{
				size++;
				vdwNewStartQuestIndices.push_back(dwQuestIndex);
			}
		}
	};

	struct FuncMatchHandleEvent
	{
		bool bMatched;

		std::vector <uint32_t> vdwQuesIndices;
		std::vector <int> viPCStates;
		std::vector <const char*> vcodes;
		std::vector <int> vcode_sizes;
		int size;


		FuncMatchHandleEvent()
			: bMatched(false), vdwQuesIndices(0), viPCStates(0), vcodes(0), vcode_sizes(0), size(0)
		{}

		bool Matched()
		{
			return bMatched;
		}

		void operator()(PC::QuestInfoIterator& itPCQuest, NPC::QuestMapType::iterator& itQuestMap)
		{
			NPC::AQuestScriptType::iterator itQuestScript;

			int iState = itPCQuest->second.st;
			if ((itQuestScript = itQuestMap->second.find(iState)) != itQuestMap->second.end())
			{
				bMatched = true;
				size++;
				vdwQuesIndices.push_back(itQuestMap->first);
				viPCStates.push_back(iState);
				vcodes.push_back(itQuestScript->second.GetCode());
				vcode_sizes.push_back(itQuestScript->second.GetSize());
			}
		}
	};

	bool NPC::HandleEvent(PC& pc, int EventIndex)
	{
        auto& runtimeManager = CQuestManager::instance();
        RuntimeScope runtimeScope(runtimeManager, runtimeManager.CurrentRuntime(), true);
		if (EventIndex < 0 || EventIndex >= QUEST_EVENT_COUNT)
		{
			LOG_ERROR("QUEST invalid EventIndex : {}", EventIndex);
			return false;
		}

		if (pc.IsRunning())
		{
			if (test_server)
			{
				CQuestManager & mgr = CQuestManager::instance();

				LOG_ERROR("QUEST There's suspended quest state, can't run new quest state (quest: {} pc: {})", pc.GetCurrentQuestName().c_str(), ecs::PlayerRuntime::IsValid(mgr.GetCurrentCharacter()) ? ecs::PlayerRuntime::GetName(mgr.GetCurrentCharacter()).data() : "<none>");
			}

			return false;
		}

		FuncMissHandleEvent fMiss;
		FuncMatchHandleEvent fMatch;
		MatchingQuest(pc, m_mapOwnQuest[EventIndex], fMatch, fMiss);

		bool r = false;
		if (fMatch.Matched())
		{
			for (int i = 0; i < fMatch.size; i++)
			{
				if ( i != 0 ) {
					//PC * pPC = CQuestManager::instance().GetPC(pc.GetID());
				}

				CQuestManager::ExecuteQuestScript(pc, fMatch.vdwQuesIndices[i], fMatch.viPCStates[i],
					fMatch.vcodes[i], fMatch.vcode_sizes[i]);
			}
			r = true;
		}
		if (fMiss.Matched())
		{
			QuestMapType& rmapEventOwnQuest = m_mapOwnQuest[EventIndex];

			for (int i = 0; i < fMiss.size; i++)
			{
				AStateScriptType& script = rmapEventOwnQuest[fMiss.vdwNewStartQuestIndices[i]][0];
				CQuestManager::ExecuteQuestScript(pc, fMiss.vdwNewStartQuestIndices[i], 0, script.GetCode(), script.GetSize());
			}
			r = true;
		}
		else
		{
			return r;
		}
		return true;
	}

	struct FuncMissHandleReceiveAllEvent
	{
		bool bHandled;

		FuncMissHandleReceiveAllEvent()
		{
			bHandled = false;
		}

		void operator() (PC::QuestInfoIterator& itPCQuest, NPC::QuestMapType::iterator& itQuestMap)
		{
			uint32_t dwQuestIndex = itQuestMap->first;

			if (NPC::HasStartState(itQuestMap->second) && CQuestManager::instance().CanStartQuest(dwQuestIndex))
			{
				const NPC::AQuestScriptType & QuestScript = itQuestMap->second;
				auto it = QuestScript.find(QUEST_START_STATE_INDEX);

				if (it != QuestScript.end())
				{
					bHandled = true;
					CQuestManager::ExecuteQuestScript(
							*CQuestManager::instance().GetCurrentPC(),
							dwQuestIndex,
							QUEST_START_STATE_INDEX,
							it->second.GetCode(),
							it->second.GetSize());
				}
			}
		}
	};

	struct FuncMatchHandleReceiveAllEvent
	{
		bool bHandled;

		FuncMatchHandleReceiveAllEvent()
		{
			bHandled = false;
		}

		void operator() (PC::QuestInfoIterator& itPCQuest, NPC::QuestMapType::iterator& itQuestMap)
		{
			const NPC::AQuestScriptType& QuestScript = itQuestMap->second;
			int iPCState = itPCQuest->second.st;
			auto itQuestScript = QuestScript.find(iPCState);

			if (itQuestScript != QuestScript.end())
			{
				bHandled = true;

				CQuestManager::ExecuteQuestScript(
						*CQuestManager::instance().GetCurrentPC(),
						itQuestMap->first,
						iPCState,
						itQuestScript->second.GetCode(),
						itQuestScript->second.GetSize());
			}
		}
	};

	bool NPC::HandleReceiveAllEvent(PC& pc, int EventIndex)
	{
        auto& runtimeManager = CQuestManager::instance();
        RuntimeScope runtimeScope(runtimeManager, runtimeManager.CurrentRuntime(), true);
		if (EventIndex < 0 || EventIndex >= QUEST_EVENT_COUNT)
		{
			LOG_ERROR("QUEST invalid EventIndex : {}", EventIndex);
			return false;
		}

		if (pc.IsRunning())
		{
			if (test_server)
			{
				CQuestManager & mgr = CQuestManager::instance();

				LOG_ERROR("QUEST There's suspended quest state, can't run new quest state (quest: {} pc: {})", pc.GetCurrentQuestName().c_str(), ecs::PlayerRuntime::IsValid(mgr.GetCurrentCharacter()) ? ecs::PlayerRuntime::GetName(mgr.GetCurrentCharacter()).data() : "<none>");
			}

			return false;
		}

		FuncMissHandleReceiveAllEvent fMiss;
		FuncMatchHandleReceiveAllEvent fMatch;

		MatchingQuest(pc, m_mapOwnQuest[EventIndex], fMatch, fMiss);
		return fMiss.bHandled || fMatch.bHandled;
	}

	struct FuncDoNothing
	{
		void operator()(PC::QuestInfoIterator& itPCQuest, NPC::QuestMapType::iterator& itQuestMap)
		{
		}
	};

	struct FuncMissHandleReceiveAllNoWaitEvent
	{
		bool bHandled;

		FuncMissHandleReceiveAllNoWaitEvent()
		{
			bHandled = false;
		}


		void operator()(PC::QuestInfoIterator& itPCQuest, NPC::QuestMapType::iterator& itQuestMap)
		{
			uint32_t dwQuestIndex = itQuestMap->first;

			if (NPC::HasStartState(itQuestMap->second) && CQuestManager::instance().CanStartQuest(dwQuestIndex))
			{
				const NPC::AQuestScriptType& QuestScript = itQuestMap->second;
				auto it = QuestScript.find(QUEST_START_STATE_INDEX);
				if (it != QuestScript.end())
				{
					bHandled = true;
					PC* pPC = CQuestManager::instance().GetCurrentPC();
					if (CQuestManager::ExecuteQuestScript(
								*pPC,
								dwQuestIndex,
								QUEST_START_STATE_INDEX,
								it->second.GetCode(),
								it->second.GetSize()))
					{
						LOG_ERROR("QUEST NOT END RUNNING on Login/Logout - {}", CQuestManager::instance().GetQuestNameByIndex(itQuestMap->first).c_str());

						QuestState& rqs = *pPC->GetRunningQuestState();
						CQuestManager::instance().CloseState(rqs);
						pPC->EndRunning();
					}
				}
			}
		}
	};

	struct FuncMatchHandleReceiveAllNoWaitEvent
	{
		bool bHandled;

		FuncMatchHandleReceiveAllNoWaitEvent()
		{
			bHandled = false;
		}

		void operator()(PC::QuestInfoIterator & itPCQuest, NPC::QuestMapType::iterator & itQuestMap)
		{
			const NPC::AQuestScriptType & QuestScript = itQuestMap->second;
			int iPCState = itPCQuest->second.st;
			auto itQuestScript = QuestScript.find(iPCState);

			if (itQuestScript != QuestScript.end())
			{
				PC * pPC = CQuestManager::instance().GetCurrentPC();

				if (CQuestManager::ExecuteQuestScript(
							*pPC,
							itQuestMap->first,
							iPCState,
							itQuestScript->second.GetCode(),
							itQuestScript->second.GetSize()))
				{
					LOG_ERROR("QUEST NOT END RUNNING on Login/Logout - {}", CQuestManager::instance().GetQuestNameByIndex(itQuestMap->first).c_str());

					QuestState& rqs = *pPC->GetRunningQuestState();
					CQuestManager::instance().CloseState(rqs);
					pPC->EndRunning();
				}
				bHandled = true;
			}
		}
	};

	bool NPC::HandleReceiveAllNoWaitEvent(PC& pc, int EventIndex)
	{
        auto& runtimeManager = CQuestManager::instance();
        RuntimeScope runtimeScope(runtimeManager, runtimeManager.CurrentRuntime(), true);
		//cerr << EventIndex << endl;
		if (EventIndex<0 || EventIndex>=QUEST_EVENT_COUNT)
		{
			LOG_ERROR("QUEST invalid EventIndex : {}", EventIndex);
			return false;
		}

		/*
		if (pc.IsRunning())
		{
			if (test_server)
			{
				CQuestManager & mgr = CQuestManager::instance();

				LOG_ERROR("QUEST There\'s suspended quest state, can\'t run new quest state (quest: {} pc: {})",
						pc.GetCurrentQuestName().c_str(),
						ecs::PlayerRuntime::IsValid(mgr.GetCurrentCharacter()) ? ecs::PlayerRuntime::GetName(mgr.GetCurrentCharacter()).data() : "<none>");
			}

			return false;
		}
		*/

		//FuncDoNothing fMiss;
		FuncMissHandleReceiveAllNoWaitEvent fMiss;
		FuncMatchHandleReceiveAllNoWaitEvent fMatch;

		QuestMapType& rmapEventOwnQuest = m_mapOwnQuest[EventIndex];
		MatchingQuest(pc, rmapEventOwnQuest, fMatch, fMiss);

		return fMatch.bHandled || fMiss.bHandled;
	}

	bool NPC::OnInfo(PC & pc, unsigned int quest_index)
	{
        auto& runtimeManager = CQuestManager::instance();
        RuntimeScope runtimeScope(runtimeManager, runtimeManager.CurrentRuntime(), true);
		const int EventIndex = QUEST_INFO_EVENT;

		if (pc.IsRunning())
		{
			if (test_server)
			{
				CQuestManager & mgr = CQuestManager::instance();

				LOG_ERROR("3. QUEST There's suspended quest state, can't run new quest state (quest: {} pc: {})", pc.GetCurrentQuestName().c_str(), ecs::PlayerRuntime::IsValid(mgr.GetCurrentCharacter()) ? ecs::PlayerRuntime::GetName(mgr.GetCurrentCharacter()).data() : "<none>");
			}

			return false;
		}

		PC::QuestInfoIterator itPCQuest = pc.quest_find(quest_index);

		if (pc.quest_end() == itPCQuest)
		{
			LOG_ERROR("QUEST no quest by (quest {})", quest_index);
			return false;
		}

		QuestMapType & rmapEventOwnQuest = m_mapOwnQuest[EventIndex];
		QuestMapType::iterator itQuestMap = rmapEventOwnQuest.find(quest_index);

		const char * questName = CQuestManager::instance().GetQuestNameByIndex(quest_index).c_str();

		if (itQuestMap == rmapEventOwnQuest.end())
		{
			LOG_ERROR("QUEST no info event (quest {})", questName);
			return false;
		}

		AQuestScriptType::iterator itQuestScript = itQuestMap->second.find(itPCQuest->second.st);

		if (itQuestScript == itQuestMap->second.end())
		{
			LOG_ERROR("QUEST no info script by state {} (quest {})", itPCQuest->second.st, questName);
			return false;
		}

		CQuestManager::ExecuteQuestScript(pc, quest_index, itPCQuest->second.st, itQuestScript->second.GetCode(), itQuestScript->second.GetSize());
		return true;
	}

	bool NPC::OnButton(PC & pc, unsigned int quest_index)
	{
        auto& runtimeManager = CQuestManager::instance();
        RuntimeScope runtimeScope(runtimeManager, runtimeManager.CurrentRuntime(), true);
		const int EventIndex = QUEST_BUTTON_EVENT;

		if (pc.IsRunning())
		{
			if (test_server) {
				CQuestManager & mgr = CQuestManager::instance();
				LOG_ERROR("QUEST There's suspended quest state, can't run new quest state (quest: {} pc: {})", pc.GetCurrentQuestName().c_str(), ecs::PlayerRuntime::IsValid(mgr.GetCurrentCharacter()) ? ecs::PlayerRuntime::GetName(mgr.GetCurrentCharacter()).data() : "<none>");
			}

			return false;
		}

		PC::QuestInfoIterator itPCQuest = pc.quest_find(quest_index);

		QuestMapType & rmapEventOwnQuest = m_mapOwnQuest[EventIndex];
		QuestMapType::iterator itQuestMap = rmapEventOwnQuest.find(quest_index);

		if (itQuestMap == rmapEventOwnQuest.end())
			return false;

		int iState = 0;

		if (itPCQuest != pc.quest_end())
		{
			iState = itPCQuest->second.st;
		}
		else
		{
			if (CQuestManager::instance().CanStartQuest(itQuestMap->first, pc) && HasStartState(itQuestMap->second))
				iState = 0;
			else
				return false;
		}

		AQuestScriptType::iterator itQuestScript=itQuestMap->second.find(iState);

		if (itQuestScript==itQuestMap->second.end())
			return false;

		CQuestManager::ExecuteQuestScript(pc, quest_index, iState, itQuestScript->second.GetCode(), itQuestScript->second.GetSize());
		return true;
	}

	struct FuncMissChatEvent
	{
		FuncMissChatEvent(vector<AArgScript*>& rAvailScript)
			: rAvailScript(rAvailScript)
			{}

		void operator()(PC::QuestInfoIterator& itPCQuest, NPC::ArgQuestMapType::iterator& itQuestMap)
		{
			if (CQuestManager::instance().CanStartQuest(itQuestMap->first) && NPC::HasStartState(itQuestMap->second))
			{
				size_t i;
				for (i = 0; i < itQuestMap->second[QUEST_START_STATE_INDEX].size(); ++i)
				{
					if (itQuestMap->second[QUEST_START_STATE_INDEX][i].when_condition.size() == 0 ||
							IsScriptTrue(&itQuestMap->second[QUEST_START_STATE_INDEX][i].when_condition[0], itQuestMap->second[QUEST_START_STATE_INDEX][i].when_condition.size()))
						rAvailScript.push_back(&itQuestMap->second[QUEST_START_STATE_INDEX][i]);
				}
			}
		}

		vector<AArgScript*>& rAvailScript;
	};

	struct FuncMatchChatEvent
	{
		FuncMatchChatEvent(vector<AArgScript*>& rAvailScript)
			: rAvailScript(rAvailScript)
			{}

		void operator()(PC::QuestInfoIterator& itPCQuest, NPC::ArgQuestMapType::iterator& itQuestMap)
		{
			int iState = itPCQuest->second.st;
			map<int,vector<AArgScript> >::iterator itQuestScript = itQuestMap->second.find(iState);
			if (itQuestScript != itQuestMap->second.end())
			{
				size_t i;
				for (i = 0; i < itQuestMap->second[iState].size(); i++)
				{
					if (itQuestMap->second[iState][i].when_condition.empty() ||
							IsScriptTrue(&itQuestMap->second[iState][i].when_condition[0], itQuestMap->second[iState][i].when_condition.size()))
						rAvailScript.push_back(&itQuestMap->second[iState][i]);
				}
			}
		}

		vector<AArgScript*>& rAvailScript;
	};

	bool NPC::OnChat(PC& pc)
	{
        auto& runtimeManager = CQuestManager::instance();
        RuntimeScope runtimeScope(runtimeManager, runtimeManager.CurrentRuntime(), true);
		if (pc.IsRunning())
		{
			if (test_server)
			{
				CQuestManager & mgr = CQuestManager::instance();

				LOG_ERROR("5. QUEST There's suspended quest state, can't run new quest state (quest: {} pc: {})", pc.GetCurrentQuestName().c_str(), ecs::PlayerRuntime::IsValid(mgr.GetCurrentCharacter()) ? ecs::PlayerRuntime::GetName(mgr.GetCurrentCharacter()).data() : "<none>");
			}

			return false;
		}

		const int EventIndex = QUEST_CHAT_EVENT;
		vector<AArgScript*> AvailScript;

		FuncMatchChatEvent fMatch(AvailScript);
		FuncMissChatEvent fMiss(AvailScript);
		MatchingQuest(pc, m_mapOwnArgQuest[EventIndex], fMatch, fMiss);


		if (AvailScript.empty())
			return false;

		{

			ostringstream os;
			os << "select(";
			os << '"' << ScriptToString(AvailScript[0]->arg.c_str()) << '"';
			for (size_t i = 1; i < AvailScript.size(); i++)
			{
				os << ",\"" << ScriptToString(AvailScript[i]->arg.c_str()) << '"';
			}
#ifdef ENABLE_MULTI_LANGUAGE
			{
				uint8_t bLang = 0;
				const entt::entity ch = CQuestManager::instance().GetCurrentCharacter();
				if (ecs::PlayerRuntime::IsValid(ch)) {
					LPDESC d = ecs::PlayerRuntime::GetDesc(ch);
					if (d) {
						bLang = d->GetLanguage();
					}
				}

				os << ", '"<< m_closeText[bLang] <<"'";
			}
#else
			os << ", '"<<" Close" <<"'";
#endif
			os << ")";

			CQuestManager::ExecuteQuestScript(pc, "QUEST_CHAT_TEMP_QUEST", 0, os.str().c_str(), os.str().size(), &AvailScript, false);
		}

		return true;
	}

	bool NPC::HasChat()
	{
		return !m_mapOwnArgQuest[QUEST_CHAT_EVENT].empty();
	}

	bool NPC::ExecuteEventScript(PC& pc, int EventIndex, uint32_t dwQuestIndex, int iState)
	{
        auto& runtimeManager = CQuestManager::instance();
        RuntimeScope runtimeScope(runtimeManager, runtimeManager.CurrentRuntime(), true);
		QuestMapType& rQuest = m_mapOwnQuest[EventIndex];

		auto itQuest = rQuest.find(dwQuestIndex);
		if (itQuest == rQuest.end())
		{
			LOG_INFO("ExecuteEventScript ei {} qi {} is {} - NO QUEST", EventIndex, dwQuestIndex, iState);
			return false;
		}

		AQuestScriptType& rScript = itQuest->second;
		auto itState = rScript.find(iState);
		if (itState == rScript.end())
		{
			LOG_INFO("ExecuteEventScript ei {} qi {} is {} - NO STATE", EventIndex, dwQuestIndex, iState);
			return false;
		}

		LOG_INFO("ExecuteEventScript ei {} qi {} is {}", EventIndex, dwQuestIndex, iState);
		CQuestManager::instance().SetCurrentEventIndex(EventIndex);
		return CQuestManager::ExecuteQuestScript(pc, dwQuestIndex, iState, itState->second.GetCode(), itState->second.GetSize());
	}

	bool NPC::OnItemInformer(PC& pc, unsigned int vnum)
	{
        auto& runtimeManager = CQuestManager::instance();
        RuntimeScope runtimeScope(runtimeManager, runtimeManager.CurrentRuntime(), true);
		return HandleEvent(pc, QUEST_ITEM_INFORMER_EVENT);
	}
}
