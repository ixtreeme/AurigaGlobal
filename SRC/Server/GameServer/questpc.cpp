#include "stdafx.h"
#include <Core/Logging.hpp>
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "ecs/systems/PointSystem.hpp"
#include "config.h"
#include "constants.h"
#include "questmanager.h"
#include "packet.h"
#include "buffer_manager.h"
#include "char_interface.hpp"
#include "ecs/CharacterAccessors.hpp"
#include "desc_client.h"
#include "questevent.h"

namespace quest
{
	PC::PC() :
		m_bIsGivenReward(false),
		m_bShouldSendDone(false),
		m_dwID(0),
		m_RunningQuestState(nullptr),
		m_iSendToClient(0),
		m_bLoaded(false),
		m_iLastState(0),
		m_dwWaitConfirmFromPID(0),
		m_bConfirmWait(false)
	{
	}

	PC::~PC()
	{
		Destroy();
	}

	void PC::Destroy()
	{
		ClearTimer();
	}

	void PC::SetID(uint32_t dwID)
	{
		m_dwID = dwID;
		m_bShouldSendDone = false;
	}

	const string & PC::GetCurrentQuestName() const
	{
		return m_stCurQuest;
	}

	uint32_t	PC::GetCurrentQuestIndex() const
	{
		return CQuestManager::instance().GetQuestIndexByName(GetCurrentQuestName());
	}

	void PC::SetFlag(const string& name, int value, bool bSkipSave)
	{
		if ( test_server )
			LOG_TRACE("QUEST Setting flag {} {}", name.c_str(), value);
		else
			LOG_TRACE("QUEST Setting flag {} {}", name.c_str(), value);

		if (value == 0)
		{
			DeleteFlag(name);
			return;
		}

		if (const auto it = m_FlagMap.find(name); it == m_FlagMap.end())
			m_FlagMap.insert(make_pair(name, value));
		else if (it->second != value)
			it->second = value;
		else
			bSkipSave = true;

		if (!bSkipSave)
			SaveFlag(name, value);
	}

	bool PC::DeleteFlag(const string & name)
	{
		if (const auto it = m_FlagMap.find(name); it != m_FlagMap.end())
		{
			m_FlagMap.erase(it);
			SaveFlag(name, 0);
			return true;
		}

		return false;
	}

	int PC::GetFlag(const string & name)
	{
		if (const auto it = m_FlagMap.find(name); it != m_FlagMap.end())
		{
			LOG_TRACE("QUEST getting flag {} {}", name.c_str(), it->second);
			return it->second;
		}
		return 0;
	}

	void PC::SaveFlag(const string & name, int value)
	{
		if (const auto it = m_FlagSaveMap.find(name); it == m_FlagSaveMap.end())
			m_FlagSaveMap.insert(make_pair(name, value));
		else if (it->second != value)
			it->second = value;
	}

	// only from lua call
	void PC::SetCurrentQuestStateName(const string& state_name)
	{
		SetFlag(m_stCurQuest + ".__status", CQuestManager::Instance().GetQuestStateIndex(m_stCurQuest,state_name));
	}

	void PC::SetQuestState(const string& quest_name, const string& state_name)
	{
		SetQuestState(quest_name, CQuestManager::Instance().GetQuestStateIndex(quest_name, state_name));
	}

	void PC::SetQuestState(const string& quest_name, int new_state_index)
	{
		int iNowState = GetFlag(quest_name + ".__status");

		if (iNowState != new_state_index)
			AddQuestStateChange(quest_name, iNowState, new_state_index);
	}

	void PC::AddQuestStateChange(const string& quest_name, int prev_state, int next_state)
	{
		uint32_t dwQuestIndex = CQuestManager::instance().GetQuestIndexByName(quest_name);
		LOG_TRACE("QUEST reserve Quest State Change quest {}[{}] from {} to {}", quest_name.c_str(), dwQuestIndex, prev_state, next_state);
		m_QuestStateChange.emplace_back(dwQuestIndex, prev_state, next_state);
	}

	void PC::SetQuest(const string& quest_name, QuestState& qs)
	{
		//LOG_INFO("PC SetQuest {}", quest_name.c_str());
		unsigned int qi = CQuestManager::instance().GetQuestIndexByName(quest_name);

		if (const auto it = m_QuestInfo.find(qi); it == m_QuestInfo.end())
			m_QuestInfo.insert(make_pair(qi, qs));
		else
			it->second = qs;

		m_stCurQuest = quest_name;
		m_RunningQuestState = &m_QuestInfo[qi];
		m_iSendToClient = 0;

		m_iLastState = qs.st;
		SetFlag(quest_name + ".__status", qs.st);

		//m_RunningQuestState->iIndex = GetCurrentQuestBeginFlag();
		m_RunningQuestState->iIndex = qi;
		m_bShouldSendDone = false;
		//if (GetCurrentQuestBeginFlag())
		//{
		//m_bSendToClient = true;
		//}
	}

	void PC::AddTimer(const string & name, LPEVENT pEvent)
	{
		RemoveTimer(name);
		m_TimerMap.insert(make_pair(name, pEvent));
		LOG_TRACE("QUEST add timer {} {}", static_cast<const void*>(get_pointer(pEvent)), m_TimerMap.size());
	}

	void PC::RemoveTimerNotCancel(const string & name)
	{
		if (const auto it = m_TimerMap.find(name); it != m_TimerMap.end())
		{
			LOG_TRACE("QUEST remove with no cancel {}", static_cast<const void*>(get_pointer(it->second)));
			m_TimerMap.erase(it);
		}

		LOG_TRACE("QUEST timer map size {} by RemoveTimerNotCancel", m_TimerMap.size());
	}

	void PC::RemoveTimer(const string & name)
	{
		const auto it = m_TimerMap.find(name);

		if (it != m_TimerMap.end())
		{
			LOG_TRACE("QUEST remove timer {}", static_cast<const void*>(get_pointer(it->second)));
			CancelTimerEvent(&it->second);
			m_TimerMap.erase(it);
		}

		LOG_TRACE("QUEST timer map size {} by RemoveTimer", m_TimerMap.size());
	}

	void PC::ClearTimer()
	{
		LOG_TRACE("QUEST clear timer {}", m_TimerMap.size());
		auto it = m_TimerMap.begin();

		while (it != m_TimerMap.end())
		{
			CancelTimerEvent(&it->second);
			++it;
		}

		m_TimerMap.clear();
	}

	void PC::SetCurrentQuestStartFlag()
	{
		if (!GetCurrentQuestBeginFlag())
		{
			SetCurrentQuestBeginFlag();
		}
	}

	void PC::SetCurrentQuestDoneFlag()
	{
		if (GetCurrentQuestBeginFlag())
		{
			ClearCurrentQuestBeginFlag();
		}
	}

	void PC::SendQuestInfoPakcet()
	{
		assert(m_iSendToClient);
		assert(m_RunningQuestState);

		packet_quest_info qi;
		qi.header = HEADER_GC_QUEST_INFO;
		qi.size = sizeof(struct packet_quest_info);
		qi.index = m_RunningQuestState->iIndex;
		qi.flag = m_iSendToClient;
#ifdef __QUEST_RENEWAL__
		qi.c_index = CQuestManager::instance().GetQuestCategoryByQuestIndex(qi.index);
#endif

		TEMP_BUFFER buf;
		buf.write(&qi, sizeof(qi));

		if (m_iSendToClient & QUEST_SEND_ISBEGIN)
		{
			uint8_t temp = m_RunningQuestState->bStart ? 1 : 0;
			buf.write(&temp, 1);
			qi.size += 1;

			LOG_TRACE("QUEST BeginFlag {}", (int)temp);
		}

		if (m_iSendToClient & QUEST_SEND_TITLE)
		{
			m_RunningQuestState->_title.reserve(30 + 1);
			buf.write(m_RunningQuestState->_title.c_str(), 30 + 1);
			qi.size += 30 + 1;

			LOG_TRACE("QUEST Title {}", m_RunningQuestState->_title.c_str());
		}

		if (m_iSendToClient & QUEST_SEND_CLOCK_NAME)
		{
			m_RunningQuestState->_clock_name.reserve(16 + 1);
			buf.write(m_RunningQuestState->_clock_name.c_str(), 16 + 1);
			qi.size += 16 + 1;

			LOG_TRACE("QUEST Clock Name {}", m_RunningQuestState->_clock_name.c_str());
		}

		if (m_iSendToClient & QUEST_SEND_CLOCK_VALUE)
		{
			buf.write(&m_RunningQuestState->_clock_value, sizeof(int));
			qi.size += 4;

			LOG_TRACE("QUEST Clock Value {}", m_RunningQuestState->_clock_value);
		}

		if (m_iSendToClient & QUEST_SEND_COUNTER_NAME)
		{
			m_RunningQuestState->_counter_name.reserve(16 + 1);
			buf.write(m_RunningQuestState->_counter_name.c_str(), 16 + 1);
			qi.size += 16 + 1;

			LOG_TRACE("QUEST Counter Name {}", m_RunningQuestState->_counter_name.c_str());
		}

		if (m_iSendToClient & QUEST_SEND_COUNTER_VALUE)
		{
			buf.write(&m_RunningQuestState->_counter_value, sizeof(int));
			qi.size += 4;

			LOG_TRACE("QUEST Counter Value {}", m_RunningQuestState->_counter_value);
		}

		if (m_iSendToClient & QUEST_SEND_ICON_FILE)
		{
			m_RunningQuestState->_icon_file.reserve(24 + 1);
			buf.write(m_RunningQuestState->_icon_file.c_str(), 24 + 1);
			qi.size += 24 + 1;

			LOG_TRACE("QUEST Icon File {}", m_RunningQuestState->_icon_file.c_str());
		}

		ecs::PlayerRuntime::GetDesc(CQuestManager::instance().GetCurrentCharacter())->Packet(buf.read_peek(), buf.size());

		m_iSendToClient = 0;
//		if (m_iSendToClient & QUEST_SEND_TITLE) {
//			m_RunningQuestState->_title.reserve(30+1);
//			strlcpy(qi.szTitle, m_RunningQuestState->_title.c_str(), sizeof(qi.szTitle));
//		}
//		qi.szTitle[30] = '\0';
//
//		if (m_iSendToClient & QUEST_SEND_ISBEGIN) {
//			qi.isBegin = m_RunningQuestState->bStart?1:0;
//		}
//		else {
//			qi.isBegin = 0;
//		}
//
//		if (m_iSendToClient & QUEST_SEND_CLOCK_NAME) {
//			m_RunningQuestState->_clock_name.reserve(16+1);
//			strlcpy(qi.szClockName, m_RunningQuestState->_clock_name.c_str(), sizeof(qi.szClockName));
//		}
//		qi.szClockName[16] = '\0';
//
//		if (m_iSendToClient & QUEST_SEND_CLOCK_VALUE) {
//			qi.iClockValue = m_RunningQuestState->_clock_value;
//		}
//		else  {
//			qi.iClockValue = 0;
//		}
//
//		if (m_iSendToClient & QUEST_SEND_COUNTER_NAME) {
//			m_RunningQuestState->_counter_name.reserve(16+1);
//			strlcpy(qi.szCounterName, m_RunningQuestState->_counter_name.c_str(), sizeof(qi.szCounterName));
//		}
//		qi.szCounterName[16] = '\0';
//
//		if (m_iSendToClient & QUEST_SEND_COUNTER_VALUE) {
//			qi.iCounterValue = m_RunningQuestState->_counter_value;
//		}
//		else {
//			qi.iCounterValue = 0;
//		}
//
//		if (m_iSendToClient & QUEST_SEND_ICON_FILE) {
//			m_RunningQuestState->_icon_file.reserve(24+1);
//			strlcpy(qi.szIconFileName, m_RunningQuestState->_icon_file.c_str(), sizeof(qi.szIconFileName));
//		}
//		qi.szIconFileName[24] = '\0';
//
//		ecs::PlayerRuntime::GetDesc(CQuestManager::instance().GetCurrentCharacter())->Packet(&qi, sizeof(qi));
//		m_iSendToClient = 0;
	}

	void PC::EndRunning()
	{
		// unlocked locked npc
		{
			LPCHARACTER npc = CQuestManager::instance().GetCurrentNPCCharacterPtr();
			const entt::entity ch = CQuestManager::instance().GetCurrentCharacter();

			if (npc && !(ecs::PlayerRuntime::IsPC(((npc) ? (npc)->GetEntityHandle() : entt::null))))
			{
				if ((ecs::PlayerRuntime::GetPlayerID(ch)) == npc->GetQuestNPCID())
				{
					npc->SetQuestNPCID(0);
					LOG_TRACE("QUEST NPC lock isn't unlocked : pid {}", (ecs::PlayerRuntime::GetPlayerID(ch)));
					CQuestManager::instance().WriteRunningStateToSyserr();
				}
			}
		}

		// commit data
		if (HasReward())
		{
			Save();

			LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();
			if (ch != nullptr) {
				Reward(ch);
				ch->Save();
			}
		}
		m_bIsGivenReward = false;

		if (m_iSendToClient)
		{
			LOG_TRACE("QUEST end running {}", m_iSendToClient);
			SendQuestInfoPakcet();
		}

		if (m_RunningQuestState == nullptr) {
			LOG_INFO("Entered PC::EndRunning() with invalid running quest state");
			return;
		}
		QuestState * pOldState = m_RunningQuestState;
		int iNowState = m_RunningQuestState->st;

		m_RunningQuestState = nullptr;

		if (m_iLastState != iNowState)
		{
			const entt::entity ch = CQuestManager::instance().GetCurrentCharacter();
			uint32_t dwQuestIndex = CQuestManager::instance().GetQuestIndexByName(m_stCurQuest);
			if (ecs::PlayerRuntime::IsValid(ch))
			{
				SetFlag(m_stCurQuest + ".__status", m_iLastState);
				CQuestManager::instance().LeaveState((ecs::PlayerRuntime::GetPlayerID(ch)), dwQuestIndex, m_iLastState);
				pOldState->st = iNowState;
				SetFlag(m_stCurQuest + ".__status", iNowState);
				CQuestManager::instance().EnterState((ecs::PlayerRuntime::GetPlayerID(ch)), dwQuestIndex, iNowState);
				if (GetFlag(m_stCurQuest + ".__status") == iNowState)
					CQuestManager::instance().Letter((ecs::PlayerRuntime::GetPlayerID(ch)), dwQuestIndex, iNowState);
			}
		}


		DoQuestStateChange();
	}

	void PC::DoQuestStateChange()
	{
		const entt::entity ch = CQuestManager::instance().GetCurrentCharacter();


		std::vector<TQuestStateChangeInfo> vecQuestStateChange;
		m_QuestStateChange.swap(vecQuestStateChange);

		for (const auto rInfo : vecQuestStateChange)
		{
			if (rInfo.quest_idx == 0)
				continue;

			uint32_t dwQuestIdx = rInfo.quest_idx;
			QuestInfoIterator it = quest_find(dwQuestIdx);
			const string stQuestName = CQuestManager::instance().GetQuestNameByIndex(dwQuestIdx);

			if (it == quest_end())
			{
				QuestState qs;
				qs.st = 0;

				m_QuestInfo.insert(make_pair(dwQuestIdx, qs));
				SetFlag(stQuestName + ".__status", 0);

				it = quest_find(dwQuestIdx);
			}

			LOG_TRACE("QUEST change reserved Quest State Change quest {} from {} to {} ({} {})", dwQuestIdx, rInfo.prev_state, rInfo.next_state, it->second.st, rInfo.prev_state);

			assert(it->second.st == rInfo.prev_state);

			CQuestManager::instance().LeaveState((ecs::PlayerRuntime::GetPlayerID(ch)), dwQuestIdx, rInfo.prev_state);
			it->second.st = rInfo.next_state;
			SetFlag(stQuestName + ".__status", rInfo.next_state);

			CQuestManager::instance().EnterState((ecs::PlayerRuntime::GetPlayerID(ch)), dwQuestIdx, rInfo.next_state);

			if (GetFlag(stQuestName + ".__status")==rInfo.next_state)
				CQuestManager::instance().Letter((ecs::PlayerRuntime::GetPlayerID(ch)), dwQuestIdx, rInfo.next_state);
		}
	}

	void PC::CancelRunning()
	{
		// cancel data
		m_RunningQuestState = nullptr;
		m_iSendToClient = 0;
		m_bShouldSendDone = false;
	}

	void PC::SetSendFlag(int idx)
	{
		m_iSendToClient |= idx;
	}

	void PC::ClearCurrentQuestBeginFlag()
	{
		//cerr << "iIndex " << m_RunningQuestState->iIndex << endl;
		SetSendFlag(QUEST_SEND_ISBEGIN);
		m_RunningQuestState->bStart = false;
		//SetFlag(m_stCurQuest+".__isbegin", 0);
	}

	void PC::SetCurrentQuestBeginFlag()
	{
		CQuestManager& q = CQuestManager::instance();
		int iQuestIndex = q.GetQuestIndexByName(m_stCurQuest);
		m_RunningQuestState->bStart = true;
		m_RunningQuestState->iIndex = iQuestIndex;

		SetSendFlag(QUEST_SEND_ISBEGIN);
		//SetFlag(m_stCurQuest+".__isbegin", iQuestIndex);
	}

	int PC::GetCurrentQuestBeginFlag()
	{
		return m_RunningQuestState?m_RunningQuestState->iIndex:0;
		//return GetFlag(m_stCurQuest+".__isbegin");
	}

	void PC::SetCurrentQuestTitle(const string& title)
	{
		SetSendFlag(QUEST_SEND_TITLE);
		m_RunningQuestState->_title = title;
	}

	void PC::SetQuestTitle(const string& quest, const string& title)
	{
		//SetSendFlag(QUEST_SEND_TITLE);
		QuestInfo::iterator it = m_QuestInfo.find(CQuestManager::instance().GetQuestIndexByName(quest));

		if (it != m_QuestInfo.end())
		{
			//(*it)->_title = title;
			QuestState* old = m_RunningQuestState;
			int old2 = m_iSendToClient;
			std::string oldquestname = m_stCurQuest;
			m_stCurQuest = quest;
			m_RunningQuestState = &it->second;
			m_iSendToClient = QUEST_SEND_TITLE;
			m_RunningQuestState->iIndex = GetCurrentQuestBeginFlag();

			SetCurrentQuestTitle(title);

			SendQuestInfoPakcet();

			m_stCurQuest = oldquestname;
			m_RunningQuestState = old;
			m_iSendToClient = old2;
		}
	}

	void PC::SetCurrentQuestClockName(const string& name)
	{
		SetSendFlag(QUEST_SEND_CLOCK_NAME);
		m_RunningQuestState->_clock_name = name;
	}

	void PC::SetCurrentQuestClockValue(int value)
	{
		SetSendFlag(QUEST_SEND_CLOCK_VALUE);
		m_RunningQuestState->_clock_value = value;
	}

	void PC::SetCurrentQuestCounterName(const string& name)
	{
		SetSendFlag(QUEST_SEND_COUNTER_NAME);
		m_RunningQuestState->_counter_name = name;
	}

	void PC::SetCurrentQuestCounterValue(int value)
	{
		SetSendFlag(QUEST_SEND_COUNTER_VALUE);
		m_RunningQuestState->_counter_value = value;
	}

	void PC::SetCurrentQuestIconFile(const string& icon_file)
	{
		SetSendFlag(QUEST_SEND_ICON_FILE);
		m_RunningQuestState->_icon_file = icon_file;
	}

	void PC::Save()
	{
		if (m_FlagSaveMap.empty())
			return;

		std::vector<TQuestTable> s_table;
		s_table.resize(m_FlagSaveMap.size());

		int i = 0;

		TFlagMap::iterator it = m_FlagSaveMap.begin();

		while (it != m_FlagSaveMap.end())
		{
			const std::string & stComp = it->first;
			int32_t lValue = it->second;

			++it;

			int iPos = stComp.find(".");

			if (iPos < 0)
			{
				LOG_ERROR("quest::PC::Save : cannot find . in FlagMap");
				continue;
			}

			string stName;
			stName.assign(stComp, 0, iPos);

			string stState;
			stState.assign(stComp, iPos + 1, stComp.length());

			if (stName.empty() || stState.empty())
			{
				LOG_ERROR("quest::PC::Save : invalid quest data: {}", stComp.c_str());
				continue;
			}

			LOG_TRACE("QUEST Save Flag {}, {} {} ({})", stName.c_str(), stState.c_str(), lValue, i);

			if (stName.length() >= QUEST_NAME_MAX_LEN)
			{
				LOG_ERROR("quest::PC::Save : quest name overflow");
				continue;
			}

			if (stState.length() >= QUEST_STATE_MAX_LEN)
			{
				LOG_ERROR("quest::PC::Save : quest state overflow");
				continue;
			}

			TQuestTable & r = s_table[i++];

			r.dwPID = m_dwID;
			strlcpy(r.szName, stName.c_str(), sizeof(r.szName));
			strlcpy(r.szState, stState.c_str(), sizeof(r.szState));
			r.lValue = lValue;
		}

		if (i > 0)
		{
			LOG_TRACE("QuestPC::Save {}", i);
			db_clientdesc->DBPacketHeader(HEADER_GD_QUEST_SAVE, 0, sizeof(TQuestTable) * i);
			db_clientdesc->Packet(&s_table[0], sizeof(TQuestTable) * i);
		}

		m_FlagSaveMap.clear();
	}

	bool PC::HasQuest(const string & quest_name)
	{
		unsigned int qi = CQuestManager::instance().GetQuestIndexByName(quest_name);
		return m_QuestInfo.contains(qi);
	}

	QuestState & PC::GetQuest(const string & quest_name)
	{
		unsigned int qi = CQuestManager::instance().GetQuestIndexByName(quest_name);
		return m_QuestInfo[qi];
	}

	void PC::GiveItem(const string& label, uint32_t dwVnum, int count)
	{
		LOG_TRACE("QUEST GiveItem {} {} {}", label.c_str(), dwVnum, count);
		if (!GetFlag(m_stCurQuest+"."+label))
		{
			m_vRewardData.emplace_back(RewardData::REWARD_TYPE_ITEM, dwVnum, count);
			//SetFlag(m_stCurQuest+"."+label,1);
		}
		else
			m_bIsGivenReward = true;
	}

	void PC::GiveExp(const string& label, uint32_t exp)
	{
		LOG_TRACE("QUEST GiveExp {} {}", label.c_str(), exp);

		if (!GetFlag(m_stCurQuest+"."+label))
		{
			m_vRewardData.emplace_back(RewardData::REWARD_TYPE_EXP, exp);
			//SetFlag(m_stCurQuest+"."+label,1);
		}
		else
			m_bIsGivenReward = true;
	}

	void PC::Reward(LPCHARACTER ch)
	{
		const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;
		if (m_bIsGivenReward)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(chEntity, CHAT_TYPE_INFO, 191, "");
#endif
			m_bIsGivenReward = false;
		}

		for (vector<RewardData>::iterator it = m_vRewardData.begin(); it != m_vRewardData.end(); ++it)
		{
			switch (it->type)
			{
				case RewardData::REWARD_TYPE_EXP:
					LOG_INFO("EXP cur {} add {} next {}", ch->GetExp(), it->value1, ch->GetNextExp());

					if (ch->GetExp() + it->value1 > ch->GetNextExp())
						ecs::PointSystem::Change(chEntity, POINT_EXP, ch->GetNextExp() - 1 - ch->GetExp());
					else
						ecs::PointSystem::Change(chEntity, POINT_EXP, it->value1);

					break;

				case RewardData::REWARD_TYPE_ITEM:
					ch->AutoGiveItem(it->value1, it->value2);
					break;

				case RewardData::REWARD_TYPE_NONE:
				default:
					LOG_ERROR("Invalid RewardData type");
					break;
			}
		}

		m_vRewardData.clear();
	}

	void PC::Build()
	{
		for (auto it = m_FlagMap.begin(); it != m_FlagMap.end(); ++it)
		{
			if (it->first.size()>9 && it->first.compare(it->first.size()-9,9, ".__status") == 0)
			{
				uint32_t dwQuestIndex = CQuestManager::instance().GetQuestIndexByName(it->first.substr(0, it->first.size()-9));
				int state = it->second;
				QuestState qs;
				qs.st = state;

				m_QuestInfo.insert(make_pair(dwQuestIndex, qs));
			}
		}
	}

	void PC::ClearQuest(const string& quest_name)
	{
		string quest_name_with_dot = quest_name + '.';
		for (auto it = m_FlagMap.begin(); it!= m_FlagMap.end();)
		{
			auto itNow = it++;
			if (itNow->second != 0 && itNow->first.starts_with(quest_name_with_dot))
			{
				//m_FlagMap.erase(itNow);
				SetFlag(itNow->first, 0);
			}
		}

		ClearTimer();

		quest::PC::QuestInfoIterator it = quest_begin();
		const unsigned int questindex = quest::CQuestManager::instance().GetQuestIndexByName(quest_name);

		while (it!= quest_end())
		{
			if (it->first == questindex)
			{
				it->second.st = 0;
				break;
			}

			++it;
		}
	}

	void PC::SendFlagList(LPCHARACTER ch)
	{
		for (auto it = m_FlagMap.begin(); it!= m_FlagMap.end(); ++it)
		{
			const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;
			if (it->first.size()>9 && it->first.compare(it->first.size()-9,9, ".__status") == 0)
			{
				const string quest_name = it->first.substr(0, it->first.size()-9);
				const char* state_name = CQuestManager::instance().GetQuestStateName(quest_name, it->second);
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(chEntity, CHAT_TYPE_INFO, 758, "%s#%s#%d", quest_name.c_str(), state_name, it->second);
#endif
			}
#ifdef TEXTS_IMPROVEMENT
			else {
				ecs::ChatSystem::SendNew(chEntity, CHAT_TYPE_INFO, 757, "%s#%d", it->first.c_str(), it->second);
			}
#endif
		}
	}
}



