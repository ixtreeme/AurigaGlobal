#include "stdafx.h"
#include <Core/Logging.hpp>
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/AIHelpers.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include <fstream>
#include "constants.h"
#include "buffer_manager.h"
#include "packet.h"
#include "desc_client.h"
#include "desc_manager.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "questmanager.h"
#include "../ecs/components/quest_components.hpp"
#include "../ecs/EntityFactory.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/systems/ItemSystem.hpp"
#include "text_file_loader.h"
#include "item.h"
#include "item_manager.h"
#include "config.h"
#include "target.h"
#include "party.h"
#include "locale_service.h"
#include "dungeon.h"
#include "../ecs/VIDRegistry.hpp"


#ifdef __QUEST_RENEWAL__
#include <boost/tokenizer.hpp>
#endif

uint32_t g_GoldDropTimeLimitValue = 0;
#ifdef ENABLE_NEWSTUFF
uint32_t g_BoxUseTimeLimitValue = 0;
uint32_t g_BuySellTimeLimitValue = 0;
bool g_NoDropMetinStone = false;
bool g_NoMountAtGuildWar = false;
bool g_NoPotionsOnPVP = false;
#endif
extern bool DropEvent_CharStone_SetValue(const std::string& name, int value);
extern bool DropEvent_RefineBox_SetValue (const std::string& name, int value);

namespace {
std::vector<entt::entity> SnapshotQuestPlayers()
{
    std::vector<entt::entity> players;
    for (const auto* desc : DESC_MANAGER::instance().GetClientSet()) {
        if (!desc) continue;
        const auto e = desc->GetEntity();
        if (ecs::PlayerRuntime::IsPC(e) && ecs::PlayerRuntime::GetDesc(e) == desc)
            players.push_back(e);
    }
    return players;
}
}

namespace quest
{
	CQuestManager::CQuestManager()
		: m_dwServerTimerArg(0), m_iRunningEventIndex(0), L(nullptr), m_bNoSend (false),
		m_CurrentRunningState(nullptr), m_currentCharacter(entt::null), m_currentPartyMember(entt::null),
		m_pCurrentPC(nullptr),  m_iCurrentSkin(0), m_bError(false), m_pOtherPCBlockRootPC(nullptr)
	{
		// A character entity destroyed while its PC is the current one (or the
		// other-PC-block root) frees the PC through the component. Clear the
		// cached pointers before that happens; the connection is process-wide
		// and independent of a specific manager instance.
		static bool s_questPCStateHook = false;

		if (!s_questPCStateHook)
		{
			g_registry.on_destroy<ecs::QuestPCState>().connect<&CQuestManager::OnQuestPCStateDestroyed>();
			s_questPCStateHook = true;
		}
	}

	CQuestManager::~CQuestManager()
	{
		Destroy();
	}

	void CQuestManager::Destroy()
	{
		if (L)
		{
			lua_close(L);
			L = nullptr;
		}
	}

	bool CQuestManager::Initialize()
	{
		if (g_bAuthServer)
			return true;

		if (!InitializeLua())
			return false;

		m_mapEventName.insert(TEventNameMap::value_type("click", QUEST_CLICK_EVENT));
		m_mapEventName.insert(TEventNameMap::value_type("kill", QUEST_KILL_EVENT));
		m_mapEventName.insert(TEventNameMap::value_type("timer", QUEST_TIMER_EVENT));
		m_mapEventName.insert(TEventNameMap::value_type("levelup", QUEST_LEVELUP_EVENT));
		m_mapEventName.insert(TEventNameMap::value_type("login", QUEST_LOGIN_EVENT));
		m_mapEventName.insert(TEventNameMap::value_type("logout", QUEST_LOGOUT_EVENT));
		m_mapEventName.insert(TEventNameMap::value_type("button", QUEST_BUTTON_EVENT));
		m_mapEventName.insert(TEventNameMap::value_type("info", QUEST_INFO_EVENT));
		m_mapEventName.insert(TEventNameMap::value_type("chat", QUEST_CHAT_EVENT));
		m_mapEventName.insert(TEventNameMap::value_type("in", QUEST_ATTR_IN_EVENT));
		m_mapEventName.insert(TEventNameMap::value_type("out", QUEST_ATTR_OUT_EVENT));
		m_mapEventName.insert(TEventNameMap::value_type("use", QUEST_ITEM_USE_EVENT));
		m_mapEventName.insert(TEventNameMap::value_type("server_timer", QUEST_SERVER_TIMER_EVENT));
		m_mapEventName.insert(TEventNameMap::value_type("enter", QUEST_ENTER_STATE_EVENT));
		m_mapEventName.insert(TEventNameMap::value_type("leave", QUEST_LEAVE_STATE_EVENT));
		m_mapEventName.insert(TEventNameMap::value_type("letter", QUEST_LETTER_EVENT));
		m_mapEventName.insert(TEventNameMap::value_type("take", QUEST_ITEM_TAKE_EVENT));
		m_mapEventName.insert(TEventNameMap::value_type("target", QUEST_TARGET_EVENT));
		m_mapEventName.insert(TEventNameMap::value_type("party_kill", QUEST_PARTY_KILL_EVENT));
		m_mapEventName.insert(TEventNameMap::value_type("unmount", QUEST_UNMOUNT_EVENT));
		m_mapEventName.insert(TEventNameMap::value_type("sig_use", QUEST_SIG_USE_EVENT));
		m_mapEventName.insert(TEventNameMap::value_type("item_informer", QUEST_ITEM_INFORMER_EVENT));
#ifdef ENABLE_QUEST_DIE_EVENT
		m_mapEventName.insert(TEventNameMap::value_type("die", QUEST_DIE_EVENT));
#endif
#if defined(__DUNGEON_INFO_SYSTEM__)
		m_mapEventName.insert(TEventNameMap::value_type("damage", QUEST_DAMAGE_EVENT));
#endif

		m_bNoSend = false;

		m_iCurrentSkin = QUEST_SKIN_NORMAL;

		{
			ifstream inf((g_stQuestDir + "/questnpc.txt").c_str());
			int line = 0;

			if (!inf.is_open())
				LOG_ERROR("QUEST Cannot open 'questnpc.txt'");
			else
				LOG_TRACE("QUEST can open 'questnpc.txt' ({})", g_stQuestDir.c_str());

			while (1)
			{
				unsigned int vnum;

				inf >> vnum;

				line++;

				if (inf.fail())
					break;

				string s;
				getline(inf, s);
				unsigned int li = 0, ri = s.size()-1;
				while (li < s.size() && isspace(s[li])) li++;
				while (ri > 0 && isspace(s[ri])) ri--;

				if (ri < li)
				{
					LOG_ERROR("QUEST questnpc.txt:{}:npc name error", line);
					continue;
				}

				s = s.substr(li, ri-li+1);

				int	n = 0;
				str_to_number(n, s.c_str());
				if (n)
					continue;

				//cout << '-' << s << '-' << endl;
				if ( test_server )
					LOG_TRACE("QUEST reading script of {}({})", s.c_str(), vnum);
				m_mapNPC[vnum].Set(vnum, s);
				m_mapNPCNameID[s] = vnum;
			}

			// notarget quest
			m_mapNPC[0].Set(0, "notarget");
		}

		SetEventFlag("guild_withdraw_delay", 1);
		SetEventFlag("guild_disband_delay", 1);


#ifdef __QUEST_RENEWAL__
		ReadQuestCategoryToDict();
#endif


		return true;
	}

	unsigned int CQuestManager::FindNPCIDByName(const std::string& name)
	{
		const auto it = m_mapNPCNameID.find(name);
		return it != m_mapNPCNameID.end() ? it->second : 0;
	}

	void CQuestManager::SelectItem(unsigned int pc, unsigned int selection)
	{
		PC* pPC = GetPC(pc);
		if (pPC && pPC->IsRunning() && pPC->GetRunningQuestState()->suspend_state == SUSPEND_STATE_SELECT_ITEM)
		{
			pPC->SetSendDoneFlag();
			pPC->GetRunningQuestState()->args=1;
			lua_pushnumber(pPC->GetRunningQuestState()->co,selection);

			if (!RunState(*pPC->GetRunningQuestState()))
			{
				CloseState(*pPC->GetRunningQuestState());
				pPC->EndRunning();
			}
		}
	}

	void CQuestManager::Confirm(unsigned int pc, EQuestConfirmType confirm, unsigned int pc2)
	{
		PC* pPC = GetPC(pc);

		if (!pPC->IsRunning())
		{
			LOG_ERROR("no quest running for pc, cannot process input : {}", pc);
			return;
		}

		if (pPC->GetRunningQuestState()->suspend_state != SUSPEND_STATE_CONFIRM)
		{
			LOG_ERROR("not wait for a confirm : {} {}", pc, pPC->GetRunningQuestState()->suspend_state);
			return;
		}

		if (pc2 && !pPC->IsConfirmWait(pc2))
		{
			LOG_ERROR("not wait for a confirm : {} {}", pc, pPC->GetRunningQuestState()->suspend_state);
			return;
		}

		pPC->ClearConfirmWait();

		pPC->SetSendDoneFlag();

		pPC->GetRunningQuestState()->args=1;
		lua_pushnumber(pPC->GetRunningQuestState()->co, confirm);

		AddScript("[END_CONFIRM_WAIT]");
		SetSkinStyle(QUEST_SKIN_NOWINDOW);
		SendScript();

		if (!RunState(*pPC->GetRunningQuestState()))
		{
			CloseState(*pPC->GetRunningQuestState());
			pPC->EndRunning();
		}

	}


#ifdef __QUEST_RENEWAL__
	int CQuestManager::GetQuestCategoryByQuestIndex(uint16_t q_index)
	{
		if (QuestCategoryIndexMap.contains(q_index))
			return QuestCategoryIndexMap[q_index];
		else
			return 0; /* DEFAULT_QUEST_CATEGORY */
	}

	void CQuestManager::ReadQuestCategoryToDict()
	{
		if (!QuestCategoryIndexMap.empty())
			QuestCategoryIndexMap.clear();

		ifstream inf((g_stQuestDir + "/questcategory.txt").c_str());

		if (!inf.is_open())
		{
			LOG_ERROR("QUEST Cannot open 'questcategory.txt'");
			return;
		}

		string lineFromFile;
		while (getline(inf, lineFromFile))
		{
			if (lineFromFile.empty())
				continue;

			boost::tokenizer token(lineFromFile, boost::escaped_list_separator('\\', '\t', '\"'));
			std::vector data(token.begin(), token.end());

			int category_num = atoi(data[0].c_str());
			string quest_name = data[1];

			unsigned int quest_index = CQuestManager::instance().GetQuestIndexByName(quest_name);

			if (test_server)
				LOG_TRACE("QUEST_CATEGORY_LINE: {} => {}, {}", lineFromFile.c_str(), data[0].c_str(), quest_name.c_str());

			if (quest_index != 0)
				QuestCategoryIndexMap[quest_index] = category_num;
			else
				LOG_ERROR("QUEST couldnt find QuestIndex for name Quest: {}({})", quest_name.c_str(), category_num);
		}
	}
#endif

	int CQuestManager::ReadQuestCategoryFile(uint16_t q_index)
	{

		ifstream inf((g_stQuestDir + "/questcategory.txt").c_str());
		int line = 0;
		int c_qi = 99;

		if (!inf.is_open())
			LOG_ERROR("QUEST Cannot open 'questcategory.txt'");
		else
			LOG_TRACE("QUEST can open 'questcategory.txt' ({})", g_stQuestDir.c_str());

		while (1)
		{
			string qn = CQuestManager::instance().GetQuestNameByIndex(q_index);

			unsigned int category_num;

			//enum
			//{
			//	MAIN_QUEST,		//0
			//	SUB_QUEST,		//1
			//	COLLECT_QUEST,	//2
			//	LEVELUP_QUEST,	//3
			//	SCROLL_QUEST,	//4
			//	SYSTEM_QUEST,	//5
			//};

			inf >> category_num;

			line++;

			if (inf.fail())
				break;

			string s;
			getline(inf, s);
			unsigned int li = 0, ri = s.size()-1;
			while (li < s.size() && isspace(s[li])) li++;
			while (ri > 0 && isspace(s[ri])) ri--;

			if (ri < li)
			{
				LOG_ERROR("QUEST questcategory.txt:{}:npc name error", line);
				continue;
			}

			s = s.substr(li, ri-li+1);

			int	n = 0;
			str_to_number(n, s.c_str());
			if (n)
				continue;

			//cout << '-' << s << '-' << endl;
			if ( test_server )
				LOG_TRACE("QUEST reading script of {}({})", s.c_str(), category_num);

			if (qn == s)
			{
				c_qi = category_num;
				break;
			}
		}

		// notarget quest
		//m_mapNPC[0].Set(0, "notarget");


		return c_qi;
	}

	void CQuestManager::Input(unsigned int pc, const char* msg)
	{
		PC* pPC = GetPC(pc);
		if (!pPC)
		{
			LOG_ERROR("no pc! : {}", pc);
			return;
		}

		if (!pPC->IsRunning())
		{
			LOG_ERROR("no quest running for pc, cannot process input : {}", pc);
			return;
		}

		if (pPC->GetRunningQuestState()->suspend_state != SUSPEND_STATE_INPUT)
		{
			LOG_ERROR("not wait for a input : {} {}", pc, pPC->GetRunningQuestState()->suspend_state);
			return;
		}

		pPC->SetSendDoneFlag();

		pPC->GetRunningQuestState()->args=1;
		lua_pushstring(pPC->GetRunningQuestState()->co,msg);

		if (!RunState(*pPC->GetRunningQuestState()))
		{
			CloseState(*pPC->GetRunningQuestState());
			pPC->EndRunning();
		}
	}

	void CQuestManager::Select(unsigned int pc, unsigned int selection)
	{
		PC* pPC;

		if ((pPC = GetPC(pc)) && pPC->IsRunning() && pPC->GetRunningQuestState()->suspend_state==SUSPEND_STATE_SELECT)
		{
			pPC->SetSendDoneFlag();

			if (!pPC->GetRunningQuestState()->chat_scripts.empty())
			{
				QuestState& old_qs = *pPC->GetRunningQuestState();
				CloseState(old_qs);

				if (selection >= pPC->GetRunningQuestState()->chat_scripts.size())
				{
					pPC->SetSendDoneFlag();
					GotoEndState(old_qs);
					pPC->EndRunning();
				}
				else
				{
					AArgScript* pas = pPC->GetRunningQuestState()->chat_scripts[selection];
					ExecuteQuestScript(*pPC, pas->quest_index, pas->state_index, pas->script.GetCode(), pas->script.GetSize());
				}
			}
			else
			{
				// on default
				pPC->GetRunningQuestState()->args=1;
				lua_pushnumber(pPC->GetRunningQuestState()->co,selection+1);

				if (!RunState(*pPC->GetRunningQuestState()))
				{
					CloseState(*pPC->GetRunningQuestState());
					pPC->EndRunning();
				}
			}
		}
		else
		{
			LOG_ERROR("wrong QUEST_SELECT request! : {}", pc);
		}
	}

	void CQuestManager::Resume(unsigned int pc)
	{
		PC * pPC;

		if ((pPC = GetPC(pc)) && pPC->IsRunning() && pPC->GetRunningQuestState()->suspend_state == SUSPEND_STATE_PAUSE)
		{
			pPC->SetSendDoneFlag();
			pPC->GetRunningQuestState()->args = 0;

			if (!RunState(*pPC->GetRunningQuestState()))
			{
				CloseState(*pPC->GetRunningQuestState());
				pPC->EndRunning();
			}
		}
			//LOG_ERROR("wrong QUEST_WAIT request! : %d", pc);
		//}
	}

	void CQuestManager::EnterState(uint32_t pc, uint32_t quest_index, int state)
	{
		PC* pPC;
		if ((pPC = GetPC(pc)))
		{
			if (!CheckQuestLoaded(pPC))
				return;

			m_mapNPC[QUEST_NO_NPC].OnEnterState(*pPC, quest_index, state);
		}
		else
			LOG_ERROR("QUEST no such pc id : {}", pc);
	}

	void CQuestManager::LeaveState(uint32_t pc, uint32_t quest_index, int state)
	{
		PC* pPC;
		if ((pPC = GetPC(pc)))
		{
			if (!CheckQuestLoaded(pPC))
				return;

			m_mapNPC[QUEST_NO_NPC].OnLeaveState(*pPC, quest_index, state);
		}
		else
			LOG_ERROR("QUEST no such pc id : {}", pc);
	}

	void CQuestManager::Letter(uint32_t pc, uint32_t quest_index, int state)
	{
		PC* pPC;
		if ((pPC = GetPC(pc)))
		{
			if (!CheckQuestLoaded(pPC))
				return;

			m_mapNPC[QUEST_NO_NPC].OnLetter(*pPC, quest_index, state);
		}
		else
			LOG_ERROR("QUEST no such pc id : {}", pc);
	}

	void CQuestManager::LogoutPC(entt::entity ch)
	{
		PC * pPC = GetPC(ecs::PlayerRuntime::GetPlayerID(ch));

		if (pPC && pPC->IsRunning())
		{
			CloseState(*pPC->GetRunningQuestState());
			pPC->CancelRunning();
		}

		Logout(ecs::PlayerRuntime::GetPlayerID(ch));

		if (m_currentCharacter == ch)
		{
			m_currentCharacter = entt::null;
			m_pCurrentPC = nullptr;
		}
	}

	///////////////////////////////////////////////////////////////////////////////////////////
	//
	//
	///////////////////////////////////////////////////////////////////////////////////////////
	void CQuestManager::Login(unsigned int pc, const char * c_pszQuest)
	{
		PC * pPC;

		if ((pPC = GetPC(pc)))
		{
			if (!CheckQuestLoaded(pPC))
				return;

			m_mapNPC[QUEST_NO_NPC].OnLogin(*pPC, c_pszQuest);
		}
		else
		{
			LOG_ERROR("QUEST no such pc id : {}", pc);
		}
	}

	void CQuestManager::Logout(unsigned int pc)
	{
		PC * pPC;

		if ((pPC = GetPC(pc)))
		{
			if (!CheckQuestLoaded(pPC))
				return;

			m_mapNPC[QUEST_NO_NPC].OnLogout(*pPC);
		}
		else
			LOG_ERROR("QUEST no such pc id : {}", pc);
	}

#define ENABLE_PARTYKILL
	void CQuestManager::Kill(unsigned int pc, unsigned int npc)
	{
		//m_CurrentNPCRace = npc;
		PC * pPC;

		LOG_INFO("CQuestManager::Kill QUEST_KILL_EVENT (pc={}, npc={})", pc, npc);
		if ((pPC = GetPC(pc)))
		{
			const entt::entity ch = GetCurrentPCEntity();
			if (!CheckQuestLoaded(pPC))
				return;

			// kill call script
			if (npc >= MAIN_RACE_MAX_NUM) //@fixme109
				m_mapNPC[npc].OnKill(*pPC); //@warme004

			if (GetCurrentPCEntity() != ch || m_pCurrentPC != pPC) return;
			m_mapNPC[QUEST_NO_NPC].OnKill(*pPC);

#ifdef ENABLE_PARTYKILL
			// party_kill call script
			if (!ecs::PlayerRuntime::IsPC(ch)) return;
			const entt::entity pParty = ecs::SocialSystem::GetParty(ch);
			const entt::entity leaderEntity = pParty != entt::null ? PartySystem::GetLeader(pParty) : ch;

			if (ecs::PlayerRuntime::IsPC(leaderEntity))
			{
				m_currentPartyMember = ch;

                PC* leader = GetPC(ecs::PlayerRuntime::GetPlayerID(leaderEntity));
                if (!leader || !CheckQuestLoaded(leader)) return;
                if (npc >= MAIN_RACE_MAX_NUM) m_mapNPC[npc].OnPartyKill(*leader);
                if (GetCurrentPCEntity() != leaderEntity || m_pCurrentPC != leader) return;
                m_mapNPC[QUEST_NO_NPC].OnPartyKill(*leader);
                if (!ecs::PlayerRuntime::IsPC(ch)) return;
				pPC = GetPC(pc);
			}
#endif
		}
		else
			LOG_ERROR("QUEST: no such pc id : {}", pc);
	}

#ifdef ENABLE_QUEST_DIE_EVENT
	void CQuestManager::Die(unsigned int pc, unsigned int npc)
	{
		PC * pPC;

		LOG_INFO("CQuestManager::Kill QUEST_DIE_EVENT (pc={}, npc={})", pc, npc);

		if ((pPC = GetPC(pc)))
		{
			if (!CheckQuestLoaded(pPC))
				return;

			m_mapNPC[QUEST_NO_NPC].OnDie(*pPC);

		}
		else
			LOG_ERROR("QUEST: no such pc id : {}", pc);
	}
#endif

#if defined(__DUNGEON_INFO_SYSTEM__)
	void CQuestManager::QuestDamage(unsigned int pc, unsigned int npc)
	{
		PC * pPC;
		if ((pPC = GetPC(pc)))
		{
			if (!CheckQuestLoaded(pPC))
				return;

			m_mapNPC[npc].OnQuestDamage(*pPC);
			if (npc != QUEST_NO_NPC)
				m_mapNPC[QUEST_NO_NPC].OnQuestDamage(*pPC);
		}
	}
#endif

	bool CQuestManager::ServerTimer(unsigned int npc, unsigned int arg)
	{
		SetServerTimerArg(arg);
		LOG_INFO("XXX ServerTimer Call NPC {} vnum {} arg {}", static_cast<const void*>(GetPCForce(0)), npc, arg);
		m_currentPartyMember = entt::null;
		m_pCurrentPC = GetPCForce(0);
		m_currentCharacter = entt::null;
		return m_mapNPC[npc].OnServerTimer(*m_pCurrentPC);
	}

	bool CQuestManager::Timer(unsigned int pc, unsigned int npc)
	{
		PC* pPC;

		if ((pPC = GetPC(pc)))
		{
			if (!CheckQuestLoaded(pPC))
			{
				return false;
			}
			// call script
			return m_mapNPC[npc].OnTimer(*pPC);
		}
		else
		{
			//cout << "no such pc id : " << pc;
			LOG_ERROR("QUEST TIMER_EVENT no such pc id : {}", pc);
			return false;
		}
		//cerr << "QUEST TIMER" << endl;
	}

	void CQuestManager::LevelUp(unsigned int pc)
	{
		PC * pPC;

		if ((pPC = GetPC(pc)))
		{
			if (!CheckQuestLoaded(pPC))
				return;

			m_mapNPC[QUEST_NO_NPC].OnLevelUp(*pPC);
		}
		else
		{
			LOG_ERROR("QUEST LEVELUP_EVENT no such pc id : {}", pc);
		}
	}

	void CQuestManager::AttrIn(entt::entity pc, entt::entity ch, int attr)
	{
		if (!ecs::PlayerRuntime::IsPC(ch)) return;
		PC* pPC;
		if ((pPC = GetPC(pc)))
		{
			m_currentPartyMember = ch;
			if (!CheckQuestLoaded(pPC))
				return;

			// call script
			m_mapNPC[attr+QUEST_ATTR_NPC_START].OnAttrIn(*pPC);
		}
		else
		{
			//cout << "no such pc id : " << pc;
			LOG_ERROR("QUEST no such character entity : {}", entt::to_integral(pc));
		}
	}

	void CQuestManager::AttrOut(entt::entity pc, entt::entity ch, int attr)
	{
		if (!ecs::PlayerRuntime::IsPC(ch)) return;
		PC* pPC;
		if ((pPC = GetPC(pc)))
		{
			//m_pCurrentCharacter = ch;
			m_currentPartyMember = ch;
			if (!CheckQuestLoaded(pPC))
				return;

			// call script
			m_mapNPC[attr+QUEST_ATTR_NPC_START].OnAttrOut(*pPC);
		}
		else
		{
			//cout << "no such pc id : " << pc;
			LOG_ERROR("QUEST no such character entity : {}", entt::to_integral(pc));
		}
	}

	bool CQuestManager::Target(unsigned int pc, uint32_t dwQuestIndex, const char * c_pszTargetName, const char * c_pszVerb)
	{
		PC * pPC;

		if ((pPC = GetPC(pc)))
		{
			if (!CheckQuestLoaded(pPC))
				return false;

			bool bRet;
			return m_mapNPC[QUEST_NO_NPC].OnTarget(*pPC, dwQuestIndex, c_pszTargetName, c_pszVerb, bRet);
		}

		return false;
	}

	void CQuestManager::QuestInfo(unsigned int pc, unsigned int quest_index)
	{
		PC* pPC;

		if ((pPC = GetPC(pc)))
		{
			// call script
			if (!CheckQuestLoaded(pPC))
			{
#ifdef TEXTS_IMPROVEMENT
				const entt::entity ch = GetCurrentPCEntity();
				if (ch != entt::null) {
					ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 510, "");
				}
#endif
				return;
			}

			m_mapNPC[QUEST_NO_NPC].OnInfo(*pPC, quest_index);
		}
		else
		{
			//cout << "no such pc id : " << pc;
			LOG_ERROR("QUEST INFO_EVENT no such pc id : {}", pc);
		}
	}

	void CQuestManager::QuestButton(unsigned int pc, unsigned int quest_index)
	{
		PC* pPC;
		if ((pPC = GetPC(pc)))
		{
			// call script
			if (!CheckQuestLoaded(pPC))
			{
#ifdef TEXTS_IMPROVEMENT
				const entt::entity ch = GetCurrentPCEntity();
				if (ch != entt::null) {
					ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 510, "");
				}
#endif
				return;
			}
			m_mapNPC[QUEST_NO_NPC].OnButton(*pPC, quest_index);
		}
		else
		{
			//cout << "no such pc id : " << pc;
			LOG_ERROR("QUEST CLICK_EVENT no such pc id : {}", pc);
		}
	}

	bool CQuestManager::TakeItem(unsigned int pc, unsigned int npc, entt::entity item)
	{
		//m_CurrentNPCRace = npc;
		PC* pPC;

		if ((pPC = GetPC(pc)))
		{
			if (!CheckQuestLoaded(pPC))
			{
#ifdef TEXTS_IMPROVEMENT
				const entt::entity ch = GetCurrentPCEntity();
				if (ch != entt::null) {
					ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 510, "");
				}
#endif
				return false;
			}
			// call script
			SetCurrentItem(item);
			return m_mapNPC[npc].OnTakeItem(*pPC);
		}
		else
		{
			//cout << "no such pc id : " << pc;
			LOG_ERROR("QUEST USE_ITEM_EVENT no such pc id : {}", pc);
			return false;
		}
	}

	bool CQuestManager::UseItem(unsigned int pc, entt::entity item, bool bReceiveAll)
	{
		if (test_server)
			LOG_TRACE("questmanager::UseItem Start : itemVnum : {} PC : {}", ItemSystem::GetItemOriginalVnum(item), pc);
		PC* pPC;
		if ((pPC = GetPC(pc)))
		{
			if (!CheckQuestLoaded(pPC))
			{
#ifdef TEXTS_IMPROVEMENT
				const entt::entity ch = GetCurrentPCEntity();
				if (ch != entt::null) {
					ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 510, "");
				}
#endif
				return false;
			}
			// call script
			SetCurrentItem(item);

			return m_mapNPC[ItemSystem::GetItemVnum(item)].OnUseItem(*pPC, bReceiveAll);
		}
		else
		{
			//cout << "no such pc id : " << pc;
			LOG_ERROR("QUEST USE_ITEM_EVENT no such pc id : {}", pc);
			return false;
		}
	}

	bool CQuestManager::SIGUse(unsigned int pc, uint32_t sig_vnum, entt::entity item, bool bReceiveAll)
	{
		if (test_server)
			LOG_TRACE("questmanager::SIGUse Start : itemVnum : {} PC : {}", ItemSystem::GetItemOriginalVnum(item), pc);
		PC* pPC;
		if ((pPC = GetPC(pc)))
		{
			if (!CheckQuestLoaded(pPC))
			{
#ifdef TEXTS_IMPROVEMENT
				const entt::entity ch = GetCurrentPCEntity();
				if (ch != entt::null) {
					ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 510, "");
				}
#endif
				return false;
			}
			// call script
			SetCurrentItem(item);

			return m_mapNPC[sig_vnum].OnSIGUse(*pPC, bReceiveAll);
		}
		else
		{
			//cout << "no such pc id : " << pc;
			LOG_ERROR("QUEST USE_ITEM_EVENT no such pc id : {}", pc);
			return false;
		}
	}

	bool CQuestManager::GiveItemToPC(unsigned int pc, entt::entity pkChr)
	{
		if (!ecs::PlayerRuntime::IsPC(pkChr))
			return false;

		PC * pPC = GetPC(pc);

		if (pPC)
		{
			if (!CheckQuestLoaded(pPC))
				return false;

			TargetInfo * pInfo = CTargetManager::instance().GetTargetInfo(pc, TARGET_TYPE_VID, ecs::PlayerRuntime::GetPacketVID(pkChr));

			if (pInfo)
			{
				bool bRet;

				if (m_mapNPC[QUEST_NO_NPC].OnTarget(*pPC, pInfo->dwQuestIndex, pInfo->szTargetName, "click", bRet))
					return true;
			}
		}

		return false;
	}

	bool CQuestManager::Click(entt::entity pc, entt::entity chrTarget)
	{
		if (!ecs::PlayerRuntime::IsValid(chrTarget)) return false;
		PC * pPC = GetPC(pc);
		const entt::entity causer = GetCurrentPCEntity();
        const uint32_t playerID = ecs::PlayerRuntime::GetPlayerID(pc);

		if (pPC)
		{
			if (!CheckQuestLoaded(pPC))
			{
#ifdef TEXTS_IMPROVEMENT
				const entt::entity ch = GetCurrentPCEntity();
				if (ch != entt::null) {
					ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 510, "");
				}
#endif
				return false;
			}

			TargetInfo * pInfo = CTargetManager::instance().GetTargetInfo(playerID, TARGET_TYPE_VID, ecs::PlayerRuntime::GetPacketVID(chrTarget));
			if (test_server)
			{
				LOG_INFO("CQuestManager::Click(pid={}, npc_name={}) - target_info({:x})", playerID, ecs::PlayerRuntime::GetName(chrTarget).data(), reinterpret_cast<uintptr_t>(pInfo));
			}

			if (pInfo)
			{
				bool bRet;
				if (m_mapNPC[QUEST_NO_NPC].OnTarget(*pPC, pInfo->dwQuestIndex, pInfo->szTargetName, "click", bRet))
					return bRet;
			}

			if (GetCurrentPCEntity() != causer || !ecs::PlayerRuntime::IsValid(chrTarget) || m_pCurrentPC != pPC) return false;

			uint32_t dwCurrentNPCRace = ecs::PlayerRuntime::GetRaceNum(chrTarget);

			if (ecs::PlayerRuntime::IsNPC(chrTarget))
			{
				const auto it = m_mapNPC.find(dwCurrentNPCRace);

				if (it == m_mapNPC.end())
				{
					LOG_INFO("CQuestManager::Click(pid={}, target_npc_name={}) - NOT EXIST NPC RACE VNUM[{}]", playerID, ecs::PlayerRuntime::GetName(chrTarget).data(), dwCurrentNPCRace); // @warme012
					return false;
				}

				// call script
				if (it->second.HasChat())
				{
					// if have chat, give chat
					if (test_server)
						LOG_INFO("CQuestManager::Click->OnChat");

					if (!it->second.OnChat(*pPC))
					{
						if (test_server)
							LOG_INFO("CQuestManager::Click->OnChat Failed");

						if (GetCurrentPCEntity() != causer || !ecs::PlayerRuntime::IsValid(chrTarget) || m_pCurrentPC != pPC) return false;
						return it->second.OnClick(*pPC);
					}

					return true;
				}
				else
				{
					// else click
					return it->second.OnClick(*pPC);
				}
			}
			return false;
		}
		else
		{
			//cout << "no such pc id : " << pc;
			LOG_ERROR("QUEST CLICK_EVENT no such pc id : {}", playerID);
			return false;
		}
		//cerr << "QUEST CLICk" << endl;
	}

	void CQuestManager::Unmount(unsigned int pc)
	{
		PC * pPC;

		if ((pPC = GetPC(pc)))
		{
			if (!CheckQuestLoaded(pPC))
				return;

			m_mapNPC[QUEST_NO_NPC].OnUnmount(*pPC);
		}
		else
			LOG_ERROR("QUEST no such pc id : {}", pc);
	}
	void CQuestManager::ItemInformer(unsigned int pc,unsigned int vnum)
	{
		PC* pPC;
		pPC = GetPC(pc);

		if (!pPC) {
			return;
		}

		m_mapNPC[QUEST_NO_NPC].OnItemInformer(*pPC,vnum);
	}
	///////////////////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////////////////////
	void CQuestManager::LoadStartQuest(const std::string& quest_name, unsigned int idx)
	{
		for (auto it = g_setQuestObjectDir.begin(); it != g_setQuestObjectDir.end(); ++it)
		{
			const std::string& stQuestObjectDir = *it;
			string full_name = stQuestObjectDir + "/begin_condition/" + quest_name;
			ifstream inf(full_name.c_str());

			if (inf.is_open())
			{
				LOG_TRACE("QUEST loading begin condition for {}", quest_name.c_str());

				istreambuf_iterator<char> ib(inf), ie;
				copy(ib, ie, back_inserter(m_hmQuestStartScript[idx]));
			}
		}
	}

	bool CQuestManager::CanStartQuest(unsigned int quest_index, const PC& pc)
	{
		return CanStartQuest(quest_index);
	}

	bool CQuestManager::CanStartQuest(unsigned int quest_index)
	{
		if (THashMapQuestStartScript::iterator it; (it = m_hmQuestStartScript.find(quest_index)) == m_hmQuestStartScript.end())
			return true;
		else
		{
			int x = lua_gettop(L);
			lua_dobuffer(L, it->second.data(), it->second.size(), "StartScript");
			int bStart = lua_toboolean(L, -1);
			lua_settop(L, x);
			return bStart != 0;
		}
	}

	bool CQuestManager::CanEndQuestAtState(const std::string& quest_name, const std::string& state_name)
	{
		return false;
	}

	void CQuestManager::DisconnectPC(entt::entity ch)
	{
        if (!ecs::PlayerRuntime::IsPC(ch)) return;

        const auto* state = g_registry.try_get<ecs::QuestPCState>(ch);
        if (!state || !state->pc) return;

        quest::PC* pPC = state->pc.get();
        if (m_pCurrentPC == pPC) {
            m_pCurrentPC = nullptr;
            m_currentCharacter = entt::null;
        }
        if (m_currentPartyMember == ch) m_currentPartyMember = entt::null;
        if (m_pOtherPCBlockRootPC == pPC) m_pOtherPCBlockRootPC = nullptr;

        // Dropping the component destroys the PC and cancels its timers.
        g_registry.remove<ecs::QuestPCState>(ch);
	}

	void CQuestManager::OnQuestPCStateDestroyed(entt::registry& registry, entt::entity entity)
	{
		CQuestManager* manager = CQuestManager::instance_ptr();

		if (!manager)
			return;

		const auto* state = registry.try_get<ecs::QuestPCState>(entity);
		quest::PC* pPC = state ? state->pc.get() : nullptr;

		if (!pPC)
			return;

		if (manager->m_pCurrentPC == pPC)
		{
			manager->m_pCurrentPC = nullptr;
			manager->m_currentCharacter = entt::null;
		}

		if (manager->m_pOtherPCBlockRootPC == pPC)
			manager->m_pOtherPCBlockRootPC = nullptr;

		if (manager->m_currentPartyMember == entity)
			manager->m_currentPartyMember = entt::null;
	}

	PC* CQuestManager::GetPCForEntity(entt::entity character, uint32_t pid)
	{
		if (character == entt::null || !g_registry.valid(character))
			return nullptr;

		auto& state = g_registry.get_or_emplace<ecs::QuestPCState>(character);

		if (!state.pc)
		{
			state.pc = std::make_unique<PC>();
			state.pc->SetID(pid);
		}

		return state.pc.get();
	}

	PC * CQuestManager::GetPCForce(unsigned int pc)
	{
		// The server timer runs without a character, so it keeps a synthetic
		// PC that is not tied to any entity. Its lifetime is process-wide.
		if (pc == 0)
		{
			static PC s_serverPC;
			return &s_serverPC;
		}

		const entt::entity character = CHARACTER_MANAGER::instance().FindEntityByPID(pc);

		if (character == entt::null || !ecs::PlayerRuntime::IsPC(character))
			return nullptr;

		return GetPCForEntity(character, pc);
	}


    PC* CQuestManager::GetPC(unsigned int pc)
    {
        return GetPC(CHARACTER_MANAGER::instance().FindEntityByPID(pc));
    }

    PC* CQuestManager::GetPC(entt::entity character)
    {
        if (!ecs::PlayerRuntime::IsPC(character)) {
            m_currentCharacter = entt::null;
            m_pCurrentPC = nullptr;
            return nullptr;
        }
        m_pCurrentPC = GetPCForEntity(character, ecs::PlayerRuntime::GetPlayerID(character));
        m_currentCharacter = character;
        return m_pCurrentPC;
    }

	entt::entity CQuestManager::GetCurrentPCEntity() const
	{
		return ecs::PlayerRuntime::IsPC(m_currentCharacter) ? m_currentCharacter : entt::null;
	}

	entt::entity CQuestManager::GetCurrentPartyMemberEntity() const
	{
		return GetCurrentPCEntity() != entt::null && ecs::PlayerRuntime::IsPC(m_currentPartyMember)
            ? m_currentPartyMember : entt::null;
	}

	entt::entity CQuestManager::GetCurrentNPCEntity() const
	{
		return ecs::PlayerRuntime::GetQuestNPC(GetCurrentPCEntity());
	}

	entt::entity CQuestManager::GetPCEntity(lua_State* L)
	{
		(void)L;
		return GetCurrentPCEntity();
	}

	entt::entity CQuestManager::GetNPCEntity(lua_State* L)
	{
		(void)L;
		return GetCurrentNPCEntity();
	}

	void CQuestManager::ClearScript()
	{
		m_strScript.clear();
		m_iCurrentSkin = QUEST_SKIN_NORMAL;
	}

	void CQuestManager::AddScript(const std::string& str)
	{
		m_strScript+=str;
	}

	void CQuestManager::SendScript()
	{
		if (m_bNoSend)
		{
			m_bNoSend = false;
			ClearScript();
			return;
		}

		if (m_strScript=="[DONE]" || m_strScript == "[NEXT]")
		{
			if (m_pCurrentPC && !m_pCurrentPC->GetAndResetDoneFlag() && m_strScript=="[DONE]" && m_iCurrentSkin == QUEST_SKIN_NORMAL && !IsError())
			{
				ClearScript();
				return;
			}
			m_iCurrentSkin = QUEST_SKIN_NOWINDOW;
		}

		// Legacy debug: Send Quest Script to current character name.
		//send -_-!
		struct ::packet_script packet_script;

		packet_script.header = HEADER_GC_SCRIPT;
		packet_script.skin = m_iCurrentSkin;
		packet_script.src_size = m_strScript.size();
		packet_script.size = packet_script.src_size + sizeof(struct packet_script);
		TEMP_BUFFER buf;
		buf.write(&packet_script, sizeof(struct packet_script));
		buf.write(&m_strScript[0], m_strScript.size());

		const entt::entity ch = GetCurrentPCEntity();
		LPDESC desc = ecs::PlayerRuntime::GetDesc(ch);

		if (ch == entt::null || !desc)
		{
			ClearScript();
			return;
		}

		desc->Packet(buf.read_peek(), buf.size());

		if (test_server)
			LOG_INFO("m_strScript {} size {}", m_strScript.c_str(), buf.size());

		ClearScript();
	}

	const char* CQuestManager::GetQuestStateName(const std::string& quest_name, const int state_index)
	{
		int x = lua_gettop(L);
		lua_getglobal(L, quest_name.c_str());
		if (lua_isnil(L,-1))
		{
			LOG_ERROR("QUEST wrong quest state file {}.{}", quest_name.c_str(), state_index);
			lua_settop(L,x);
			return "";
		}
		lua_pushnumber(L, state_index);
		lua_gettable(L, -2);

		const char* str = lua_tostring(L, -1);
		lua_settop(L, x);
#ifdef ENABLE_QUEST_SYSTEM_BUGFIXES

		return str ? str : "";
#else


		return str;
#endif
	}

	int CQuestManager::GetQuestStateIndex(const std::string& quest_name, const std::string& state_name)
	{
		int x = lua_gettop(L);
		lua_getglobal(L, quest_name.c_str());
		if (lua_isnil(L,-1))
		{
			LOG_ERROR("QUEST wrong quest state file {}.{}", quest_name.c_str(), state_name.c_str());
			lua_settop(L,x);
			return 0;
		}
		lua_pushstring(L, state_name.c_str());
		lua_gettable(L, -2);

		int v = (int)rint(lua_tonumber(L,-1));
		lua_settop(L, x);
		if ( test_server )
			LOG_INFO("[QUESTMANAGER] GetQuestStateIndex x({}) v({}) {} {}", v, x, quest_name.c_str(), state_name.c_str());
		return v;
	}

	void CQuestManager::SetSkinStyle(int iStyle)
	{
		if (iStyle<0 || iStyle >= QUEST_SKIN_COUNT)
		{
			m_iCurrentSkin = QUEST_SKIN_NORMAL;
		}
		else
			m_iCurrentSkin = iStyle;
	}

	unsigned int CQuestManager::LoadTimerScript(const std::string& name)
	{
		std::map<std::string, unsigned int>::iterator it;
		if ((it = m_mapTimerID.find(name)) != m_mapTimerID.end())
		{
			return it->second;
		}
		else
		{
			unsigned int new_id = UINT_MAX - m_mapTimerID.size();

			m_mapNPC[new_id].Set(new_id, name);
			m_mapTimerID.insert(std::make_pair(name, new_id));

			return new_id;
		}
	}

	unsigned int CQuestManager::GetCurrentNPCRace()
	{
		return ecs::PlayerRuntime::GetRaceNum(GetCurrentNPCEntity());
	}

	entt::entity CQuestManager::GetCurrentItemEntity()
	{
		return ecs::PlayerRuntime::GetQuestItem(GetCurrentCharacter());
	}

	void CQuestManager::ClearCurrentItem()
	{
		ecs::PlayerRuntime::SetQuestItem(GetCurrentCharacter(), entt::null);
	}

	void CQuestManager::SetCurrentItem(entt::entity item)
	{
		ecs::PlayerRuntime::SetQuestItem(GetCurrentCharacter(), item);
	}

	const std::string & CQuestManager::GetCurrentQuestName()
	{
		return GetCurrentPC()->GetCurrentQuestName();
	}

	void CQuestManager::RegisterQuest(const std::string & stQuestName, unsigned int idx)
	{
		assert(idx > 0);



		if (auto it = m_hmQuestName.find(stQuestName); it != m_hmQuestName.end())
			return;

		m_hmQuestName.insert(std::make_pair(stQuestName, idx));
		LoadStartQuest(stQuestName, idx);
		m_mapQuestNameByIndex.insert(std::make_pair(idx, stQuestName));

		LOG_TRACE("QUEST: Register {} {}", idx, stQuestName.c_str());
	}

	unsigned int CQuestManager::GetQuestIndexByName(const std::string& name)
	{
		THashMapQuestName::iterator it = m_hmQuestName.find(name);

		if (it == m_hmQuestName.end())
			return 0; // RESERVED

		return it->second;
	}

	const std::string & CQuestManager::GetQuestNameByIndex(unsigned int idx)
	{
		auto it = m_mapQuestNameByIndex.find(idx);
		if ( it == m_mapQuestNameByIndex.end())
		{
			LOG_ERROR("cannot find quest name by index {}", idx);
			assert(!"cannot find quest name by index");

			static std::string st = "";
			return st;
		}

		return it->second;
	}

	void CQuestManager::SendEventFlagList(entt::entity chEntity)
	{
		if (!ecs::PlayerRuntime::IsValid(chEntity))
			return;

		const auto flags = m_mapEventFlag;
		for (auto it = flags.begin(); it != flags.end(); ++it)
		{
			if (!ecs::PlayerRuntime::IsValid(chEntity))
				return;
			const std::string& flagname = it->first;
			int value = it->second;
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(chEntity, CHAT_TYPE_INFO, 757, "%s#%d", flagname.c_str(), value);
#endif
		}
	}

	void CQuestManager::RequestSetEventFlag(const std::string& name, int value)
	{
		TPacketSetEventFlag p;
		strlcpy(p.szFlagName, name.c_str(), sizeof(p.szFlagName));
		p.lValue = value;
		db_clientdesc->DBPacket(HEADER_GD_SET_EVENT_FLAG, 0, &p, sizeof(TPacketSetEventFlag));
	}

	void CQuestManager::SetEventFlag(const std::string& name, int value)
	{
		static const char*	DROPEVENT_CHARTONE_NAME		= "drop_char_stone";
		static const int	DROPEVENT_CHARTONE_NAME_LEN = strlen(DROPEVENT_CHARTONE_NAME);

		int prev_value = m_mapEventFlag[name];

		LOG_TRACE("QUEST eventflag {} {} prev_value {}", name.c_str(), value, m_mapEventFlag[name]);
		m_mapEventFlag[name] = value;

		if (name == "mob_item")
		{
			CHARACTER_MANAGER::instance().SetMobItemRate(value);
		}
		else if (name == "mob_dam")
		{
			CHARACTER_MANAGER::instance().SetMobDamageRate(value);
		}
		else if (name == "mob_gold")
		{
			CHARACTER_MANAGER::instance().SetMobGoldAmountRate(value);
		}
		else if (name == "mob_gold_pct")
		{
			CHARACTER_MANAGER::instance().SetMobGoldDropRate(value);
		}
		else if (name == "user_dam")
		{
			CHARACTER_MANAGER::instance().SetUserDamageRate(value);
		}
		else if (name == "user_dam_buyer")
		{
			CHARACTER_MANAGER::instance().SetUserDamageRatePremium(value);
		}
		else if (name == "mob_exp")
		{
			CHARACTER_MANAGER::instance().SetMobExpRate(value);
		}
		else if (name == "mob_item_buyer")
		{
			CHARACTER_MANAGER::instance().SetMobItemRatePremium(value);
		}
		else if (name == "mob_exp_buyer")
		{
			CHARACTER_MANAGER::instance().SetMobExpRatePremium(value);
		}
		else if (name == "mob_gold_buyer")
		{
			CHARACTER_MANAGER::instance().SetMobGoldAmountRatePremium(value);
		}
		else if (name == "mob_gold_pct_buyer")
		{
			CHARACTER_MANAGER::instance().SetMobGoldDropRatePremium(value);
		}
		else if (name == "crcdisconnect")
		{
			DESC_MANAGER::instance().SetDisconnectInvalidCRCMode(value != 0);
		}
		else if (name == "newyear_boom")
		{
			for (const entt::entity ch : SnapshotQuestPlayers())
			{

				if (!ecs::PlayerRuntime::IsPC(ch))
					continue;

				ecs::ChatSystem::Send(ch, CHAT_TYPE_COMMAND, "newyear_boom %d", value);
			}
		}
		else if ( name == "eclipse" )
		{
			std::string mode("");

			if ( value == 1 )
			{
				mode = "dark";
			}
			else
			{
				mode = "light";
			}

			for (const entt::entity ch : SnapshotQuestPlayers())
			{
				if (!ecs::PlayerRuntime::IsPC(ch))
					continue;

				ecs::ChatSystem::Send(ch, CHAT_TYPE_COMMAND, "DayMode %s", mode.c_str());
			}
		}
		else if (name == "day")
		{
			for (const entt::entity ch : SnapshotQuestPlayers())
			{
				if (!ecs::PlayerRuntime::IsPC(ch))
					continue;
				if (value)
				{
					ecs::ChatSystem::Send(ch, CHAT_TYPE_COMMAND, "DayMode dark");
				}
				else
				{
					ecs::ChatSystem::Send(ch, CHAT_TYPE_COMMAND, "DayMode light");
				}
			}
		}
		else if (name == "pre_event_hc")
		{
			const uint32_t EventNPC = 20090;

			struct SEventNPCPosition
			{
				int32_t lMapIndex;
				int32_t x;
				int32_t y;
			} positions[] = {
				{ 3, 588, 617 },
				{ 23, 397, 250 },
				{ 43, 567, 426 },
				{ 0, 0, 0 },
			};

			if (value && !prev_value)
			{
				SEventNPCPosition* pPosition = positions;

				while (pPosition->lMapIndex)
				{
					if (map_allow_find(pPosition->lMapIndex))
					{
						PIXEL_POSITION pos;

						if (!SECTREE_MANAGER::instance().GetMapBasePositionByMapIndex(pPosition->lMapIndex, pos))
						{
							LOG_ERROR("cannot get map base position {}", pPosition->lMapIndex);
							++pPosition;
							continue;
						}

						CHARACTER_MANAGER::instance().SpawnMobEntity(EventNPC, pPosition->lMapIndex, pos.x+pPosition->x*100, pos.y+pPosition->y*100, 0, false, -1);
					}
					pPosition++;
				}
			}
			else if (!value && prev_value)
			{
				std::vector<entt::entity> i;

				if (CHARACTER_MANAGER::instance().GetCharactersByRaceNum(EventNPC, i))
				{
					for (const entt::entity chEntity : i)
					{
						switch (ecs::PlayerRuntime::GetMapIndex(chEntity))
						{
							case 3:
							case 23:
							case 43:
								M2_DESTROY_CHARACTER(chEntity);
								break;
						}
					}
				}
			}
		}
		else if (name.compare(0, DROPEVENT_CHARTONE_NAME_LEN, DROPEVENT_CHARTONE_NAME)== 0)
		{
			DropEvent_CharStone_SetValue(name, value);
		}
		else if (name.compare(0, strlen("refine_box"), "refine_box")== 0)
		{
			DropEvent_RefineBox_SetValue(name, value);
		}
		else if (name == "gold_drop_limit_time")
		{
			g_GoldDropTimeLimitValue = value * 1000;
		}
#ifdef ENABLE_NEWSTUFF
		else if (name == "box_use_limit_time")
		{
			g_BoxUseTimeLimitValue = value * 1000;
		}
		else if (name == "buysell_limit_time")
		{
			g_BuySellTimeLimitValue = value * 1000;
		}
		else if (name == "no_drop_metin_stone")
		{
			g_NoDropMetinStone = !!value;
		}
		else if (name == "no_mount_at_guild_war")
		{
			g_NoMountAtGuildWar = !!value;
		}
		else if (name == "no_potions_on_pvp")
		{
			g_NoPotionsOnPVP = !!value;
		}
#endif
	}

	int	CQuestManager::GetEventFlag(const std::string& name)
	{
		std::map<std::string,int>::iterator it = m_mapEventFlag.find(name);

		if (it == m_mapEventFlag.end())
			return 0;

		return it->second;
	}

	void CQuestManager::BroadcastEventFlagOnLogin(entt::entity ch)
	{
		int iEventFlagValue;

		if ((iEventFlagValue = quest::CQuestManager::instance().GetEventFlag("xmas_snow")))
		{
			ecs::ChatSystem::Send(ch, CHAT_TYPE_COMMAND, "xmas_snow %d", iEventFlagValue);
		}

		if ((iEventFlagValue = quest::CQuestManager::instance().GetEventFlag("xmas_boom")))
		{
			ecs::ChatSystem::Send(ch, CHAT_TYPE_COMMAND, "xmas_boom %d", iEventFlagValue);
		}

		if ((iEventFlagValue = quest::CQuestManager::instance().GetEventFlag("xmas_tree")))
		{
			ecs::ChatSystem::Send(ch, CHAT_TYPE_COMMAND, "xmas_tree %d", iEventFlagValue);
		}

		if ((iEventFlagValue = quest::CQuestManager::instance().GetEventFlag("day")))
		{
			ecs::ChatSystem::Send(ch, CHAT_TYPE_COMMAND, "DayMode dark");
		}

		if ((iEventFlagValue = quest::CQuestManager::instance().GetEventFlag("newyear_boom")))
		{
			ecs::ChatSystem::Send(ch, CHAT_TYPE_COMMAND, "newyear_boom %d", iEventFlagValue);
		}

		if ( (iEventFlagValue = quest::CQuestManager::instance().GetEventFlag("eclipse")) )
		{
			std::string mode;

			if ( iEventFlagValue == 1 ) mode = "dark";
			else mode = "light";

			ecs::ChatSystem::Send(ch, CHAT_TYPE_COMMAND, "DayMode %s", mode.c_str());
		}
	}

	void CQuestManager::Reload()
	{
		lua_close(L);
		m_mapNPC.clear();
		m_mapNPCNameID.clear();
		m_hmQuestName.clear();
		m_mapTimerID.clear();
		m_hmQuestStartScript.clear();
		m_mapEventName.clear();
		L = nullptr;
		Initialize();

		for (auto it = m_registeredNPCVnum.begin(); it != m_registeredNPCVnum.end(); ++it)
		{
			char buf[256];
			uint32_t dwVnum = *it;
			snprintf(buf, sizeof(buf), "%u", dwVnum);
			m_mapNPC[dwVnum].Set(dwVnum, buf);
		}
	}

	bool CQuestManager::ExecuteQuestScript(PC& pc, uint32_t quest_index, const int state, const char* code, const int code_size, std::vector<AArgScript*>* pChatScripts, bool bUseCache)
	{
		return ExecuteQuestScript(pc, CQuestManager::instance().GetQuestNameByIndex(quest_index), state, code, code_size, pChatScripts, bUseCache);
	}

	bool CQuestManager::ExecuteQuestScript(PC& pc, const std::string& quest_name, const int state, const char* code, const int code_size, std::vector<AArgScript*>* pChatScripts, bool bUseCache)
	{
		QuestState qs = CQuestManager::instance().OpenState(quest_name, state);
		if (pChatScripts)
			qs.chat_scripts.swap(*pChatScripts);

		if (bUseCache)
		{
			lua_getglobal(qs.co, "__codecache");
			// stack : __codecache
			lua_pushnumber(qs.co, (long)code);
			// stack : __codecache (codeptr)
			lua_rawget(qs.co, -2);
			// stack : __codecache (compiled-code)
			if (lua_isnil(qs.co, -1))
			{
				// cache miss

				// load code to lua,
				// save it to cache
				// and only function remain in stack
				lua_pop(qs.co, 1);
				// stack : __codecache
				luaL_loadbuffer(qs.co, code, code_size, quest_name.c_str());
				// stack : __codecache (compiled-code)
				lua_pushnumber(qs.co, (long)code);
				// stack : __codecache (compiled-code) (codeptr)
				lua_pushvalue(qs.co, -2);
				// stack : __codecache (compiled-code) (codeptr) (compiled_code)
				lua_rawset(qs.co, -4);
				// stack : __codecache (compiled-code)
				lua_remove(qs.co, -2);
				// stack : (compiled-code)
			}
			else
			{
				// cache hit
				lua_remove(qs.co, -2);
				// stack : (compiled-code)
			}
		}
		else
			luaL_loadbuffer(qs.co, code, code_size, quest_name.c_str());

		pc.SetQuest(quest_name, qs);

		QuestState& rqs = *pc.GetRunningQuestState();
		if (!CQuestManager::instance().RunState(rqs))
		{
			CQuestManager::instance().CloseState(rqs);
			pc.EndRunning();
			return false;
		}
		return true;
	}

	void CQuestManager::RegisterNPCVnum(uint32_t dwVnum)
	{
		if (m_registeredNPCVnum.find(dwVnum) != m_registeredNPCVnum.end())
			return;

		m_registeredNPCVnum.insert(dwVnum);

		char buf[256];
		DIR* dir;

		for (auto it = g_setQuestObjectDir.begin(); it != g_setQuestObjectDir.end(); ++it)
		{
			const std::string& stQuestObjectDir = *it;
			snprintf(buf, sizeof(buf), "%s/%u", stQuestObjectDir.c_str(), dwVnum);
			LOG_INFO("{}", buf);

			if ((dir = opendir(buf)))
			{
				closedir(dir);
				snprintf(buf, sizeof(buf), "%u", dwVnum);
				LOG_INFO("{}", buf);

				m_mapNPC[dwVnum].Set(dwVnum, buf);
			}
		}
	}

	void CQuestManager::WriteRunningStateToSyserr()
	{
		const char * state_name = GetQuestStateName(GetCurrentQuestName(), GetCurrentState()->st);

		string event_index_name = "";
		for (auto it = m_mapEventName.begin(); it != m_mapEventName.end(); ++it)
		{
			if (it->second == m_iRunningEventIndex)
			{
				event_index_name = it->first;
				break;
			}
		}

		LOG_ERROR("LUA_ERROR: quest {}.{} {}", GetCurrentQuestName().c_str(), state_name, event_index_name.c_str());
		if (GetCurrentPCEntity() != entt::null && test_server)
			ecs::ChatSystem::Send(GetCurrentCharacter(), CHAT_TYPE_PARTY, "LUA_ERROR: quest %s.%s %s", GetCurrentQuestName().c_str(), state_name, event_index_name.c_str() );
	}

	void CQuestManager::QuestErrorImpl(const char* func, int line, const std::string& msg)
	{
		LOG_ERROR("[QUEST {}:{}] {}", func ? func : "", line, msg);
		if (test_server)
		{
			const entt::entity ch = GetCurrentCharacter();

			if (ecs::PlayerRuntime::IsValid(ch))
			{
				ecs::ChatSystem::Send(ch, CHAT_TYPE_PARTY, "error occurred on [%s:%d]", func,line);
				ecs::ChatSystem::Send(ch, CHAT_TYPE_PARTY, "%s", msg.c_str());
			}
		}
	}

	void CQuestManager::QuestError(const char* func, int line, const char* fmt, ...)
	{
		char szMsg[4096];
		va_list args;

		va_start(args, fmt);
		vsnprintf(szMsg, sizeof(szMsg), fmt, args);
		va_end(args);

		QuestErrorImpl(func, line, szMsg);
	}

	void CQuestManager::AddServerTimer(const std::string& name, uint32_t arg, LPEVENT event)
	{
		LOG_INFO("XXX AddServerTimer {} {} {}", name.c_str(), arg, static_cast<const void*>(get_pointer(event)));
		if (m_mapServerTimer.contains(std::make_pair(name, arg)))
		{
			LOG_ERROR("already registered server timer name:{} arg:{}", name.c_str(), arg);
			return;
		}
		m_mapServerTimer.insert(std::make_pair(make_pair(name, arg), event));
	}

	void CQuestManager::ClearServerTimerNotCancel(const std::string& name, uint32_t arg)
	{
		m_mapServerTimer.erase(std::make_pair(name, arg));
	}

	//	itertype(m_mapServerTimer) it = m_mapServerTimer.find(std::make_pair(name, arg));
	//		m_mapServerTimer.erase(it);

	void CQuestManager::ClearServerTimer(const std::string& name, uint32_t arg)
	{
		auto it = m_mapServerTimer.find(std::make_pair(name, arg));
		if (it != m_mapServerTimer.end())
		{
			auto event = it->second;
			event_cancel(&event);
			m_mapServerTimer.erase(it);
		}
	}

	void CQuestManager::CancelServerTimers(uint32_t arg)
	{
		//		++it;
		//		it = m_mapServerTimer.erase(it);
		for (auto it = m_mapServerTimer.begin(); it != m_mapServerTimer.end();) {
			if (it->first.second == arg) {
				LPEVENT event = it->second;
				event_cancel(&event);
				m_mapServerTimer.erase(it++);
			}
			else {
				++it;
			}
		}
	}

	//	itertype(m_mapServerTimer) it = m_mapServerTimer.begin();
	//			m_mapServerTimer.erase(it++);
	//			++it;

	void CQuestManager::SetServerTimerArg(uint32_t dwArg)
	{
		m_dwServerTimerArg = dwArg;
	}

	uint32_t CQuestManager::GetServerTimerArg()
	{
		return m_dwServerTimerArg;
	}

	void CQuestManager::BeginOtherPCBlock(uint32_t pid)
	{
		const entt::entity ch = GetCurrentCharacter();
		if (!ecs::PlayerRuntime::IsValid(ch))
		{
			LOG_ERROR("NULL?");
			return;
		}
		/*
		# 1. current pid = pid0 <- It will be m_pOtherPCBlockRootPC.
		begin_other_pc_block(pid1)
			# 2. current pid = pid1
			begin_other_pc_block(pid2)
				# 3. current_pid = pid2
			end_other_pc_block()
		end_other_pc_block()
		*/
		// when begin_other_pc_block(pid1)
		if (m_vecPCStack.empty())
		{
			m_pOtherPCBlockRootPC = GetCurrentPC();
		}
		m_vecPCStack.push_back(ch);
		GetPC(pid);
	}

	void CQuestManager::EndOtherPCBlock()
	{
		if (m_vecPCStack.size() == 0)
		{
			LOG_ERROR("end_other_pc_block called with an empty character stack");
			return;
		}

        const entt::entity previous = m_vecPCStack.back();
        m_vecPCStack.pop_back();
        if (ecs::PlayerRuntime::IsPC(previous)) {
            const auto pid = ecs::PlayerRuntime::GetPlayerID(previous);
            if (CHARACTER_MANAGER::instance().FindEntityByPID(pid) == previous) GetPC(pid);
            else { m_currentCharacter = entt::null; m_pCurrentPC = nullptr; }
        } else {
            m_currentCharacter = entt::null;
            m_pCurrentPC = nullptr;
        }

		if (m_vecPCStack.empty())
		{
			m_pOtherPCBlockRootPC = nullptr;
		}
	}

	bool CQuestManager::IsInOtherPCBlock()
	{
		return !m_vecPCStack.empty();
	}

	PC*	CQuestManager::GetOtherPCBlockRootPC()
	{
		return m_pOtherPCBlockRootPC;
	}
}




