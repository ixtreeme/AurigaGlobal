#pragma once

#include <Core/Logging.hpp>
#include <fmt/format.h>

#include <string>
#include <unordered_map>
#include <utility>

#include <entt/entt.hpp>

#include "questnpc.h"

class ITEM;
class CHARACTER;
class CDungeon;

#ifdef ENABLE_NEWSTUFF
enum ETL { ETL_NIL, ETL_CFUN, ETL_LNUM, ETL_LSTR };

class lua_Any
{
public:
	// non-merged union data
	lua_CFunction cfVal;
	lua_Number lnVal;
	lua_String lsVal;
	// specified type
	ETL type;

	lua_Any() : cfVal(nullptr), lnVal(0), lsVal(nullptr), type(ETL_NIL)
	{
	}

	explicit lua_Any(const lua_CFunction a1) : cfVal(a1), lnVal(0), lsVal(nullptr), type(ETL_CFUN)
	{
	}

	explicit lua_Any(const lua_Number a1) : cfVal(nullptr), lnVal(a1), lsVal(nullptr), type(ETL_LNUM)
	{
	}

	explicit lua_Any(lua_String a1) : cfVal(nullptr), lnVal(0), lsVal(a1), type(ETL_LSTR)
	{
	}
};

typedef struct luaC_tab {
	const char* name;
	lua_Any val;
} _luaC_tab;
#endif

namespace quest
{
	bool IsScriptTrue(const char* code, int size);
	std::string ScriptToString(const std::string& str);

	class CQuestManager : public singleton<CQuestManager>
	{
	public:
		enum
		{
			QUEST_SKIN_NOWINDOW,
			QUEST_SKIN_NORMAL,
			//QUEST_SKIN_CINEMATIC,
			QUEST_SKIN_SCROLL = 4,
			QUEST_SKIN_CINEMATIC = 5,
			QUEST_SKIN_COUNT
		};

		typedef std::map<std::string, int>		TEventNameMap;
		typedef std::map<unsigned int, PC>	PCMap;

	public:
		CQuestManager();
		virtual ~CQuestManager();

		bool		Initialize();
		void		Destroy();

		bool		InitializeLua();
		lua_State* GetLuaState() { return L; }
		void		AddLuaFunctionTable(const char* c_pszName, luaL_reg* preg, bool bCheckIfExists = false) const;
		void		AddLuaFunctionSubTable(const char* c_pszName, const char* c_pszSubName, luaL_reg* preg) const;
#ifdef ENABLE_NEWSTUFF
		void		AppendLuaFunctionTable(const char* c_pszName, luaL_reg* preg, bool bForceCreation = false) const;
		void		AddLuaConstantGlobal(const char* c_pszName, lua_Number lNumber, bool bOverwrite = false) const;
		void		AddLuaConstantInTable(const char* c_pszName, const char* c_pszSubName, lua_Number lNumber, bool bForceCreation = false) const;
		void		AddLuaConstantInTable(const char* c_pszName, const char* c_pszSubName, const char* szString, bool bForceCreation = false) const;
		void		AddLuaConstantSubTable(const char* c_pszName, const char* c_pszSubName, luaC_tab* preg) const;
#endif

		TEventNameMap	m_mapEventName;

		QuestState		OpenState(const std::string& quest_name, int state_index) const;
		void		CloseState(QuestState& qs) const;
		bool		RunState(QuestState& qs);

		PC* GetPC(unsigned int pc);
		PC* GetPCForce(unsigned int pc);

		unsigned int	GetCurrentNPCRace();
		const std::string& GetCurrentQuestName();
		unsigned int	FindNPCIDByName(const std::string& name);

		//void		SetCurrentNPCCharacterPtr(LPCHARACTER ch) { m_pkCurrentNPC = ch; }
		LPCHARACTER		GetCurrentNPCCharacterPtr() const;

		void		SetCurrentEventIndex(int index) { m_iRunningEventIndex = index; }

		bool		UseItem(unsigned int pc, entt::entity item, bool bReceiveAll);
		bool		SIGUse(unsigned int pc, uint32_t sig_vnum, entt::entity item, bool bReceiveAll);
		bool		TakeItem(unsigned int pc, unsigned int npc, entt::entity item);
		LPITEM		GetCurrentItem();
		entt::entity	GetCurrentItemEntity();
		void		ClearCurrentItem();
		void		SetCurrentItem(entt::entity item);
		void		AddServerTimer(const std::string& name, uint32_t arg, LPEVENT event);
		void		ClearServerTimer(const std::string& name, uint32_t arg);
		void		ClearServerTimerNotCancel(const std::string& name, uint32_t arg);
		void		CancelServerTimers(uint32_t arg);

		void		SetServerTimerArg(uint32_t dwArg);
		uint32_t		GetServerTimerArg();

		// event over state and stae
		bool		ServerTimer(unsigned int npc, unsigned int arg);

		void		Login(unsigned int pc, const char* c_pszQuestName = nullptr);
		void		Logout(unsigned int pc);
		bool		Timer(unsigned int pc, unsigned int npc);
		bool		Click(unsigned int pc, LPCHARACTER pkNPC);
		void		Kill(unsigned int pc, unsigned int npc);
#ifdef ENABLE_QUEST_DIE_EVENT
		void		Die(unsigned int pc, unsigned int npc);
#endif
#if defined(__DUNGEON_INFO_SYSTEM__)
		void		QuestDamage(unsigned int pc, unsigned int npc);
#endif
		void		LevelUp(unsigned int pc);
		void		AttrIn(unsigned int pc, LPCHARACTER ch, int attr);
		void		AttrOut(unsigned int pc, LPCHARACTER ch, int attr);
		bool		Target(unsigned int pc, uint32_t dwQuestIndex, const char* c_pszTargetName, const char* c_pszVerb);
		bool		GiveItemToPC(unsigned int pc, LPCHARACTER pkChr);
		void		Unmount(unsigned int pc);

		void		QuestButton(unsigned int pc, unsigned int quest_index);
		void		QuestInfo(unsigned int pc, unsigned int quest_index);

		void		EnterState(uint32_t pc, uint32_t quest_index, int state);
		void		LeaveState(uint32_t pc, uint32_t quest_index, int state);

		void		Letter(uint32_t pc);
		void		Letter(uint32_t pc, uint32_t quest_index, int state);

		void		ItemInformer(unsigned int pc, unsigned int vnum);

		bool		CheckQuestLoaded(PC* pc) { return pc && pc->IsLoaded(); }

		// event occurs in one state
		void		Select(unsigned int pc, unsigned int selection);
		void		Resume(unsigned int pc);

		int		ReadQuestCategoryFile(uint16_t q_index);
		void		Input(unsigned int pc, const char* msg);
		void		Confirm(unsigned int pc, EQuestConfirmType confirm, unsigned int pc2 = 0);
		void		SelectItem(unsigned int pc, unsigned int selection);

		void		LogoutPC(entt::entity ch);
		void		DisconnectPC(entt::entity ch);

		QuestState* GetCurrentState() { return m_CurrentRunningState; }

		void 		LoadStartQuest(const std::string& quest_name, unsigned int idx);
		//bool		CanStartQuest(const string& quest_name, const PC& pc);
		bool		CanStartQuest(unsigned int quest_index, const PC& pc);
		bool		CanStartQuest(unsigned int quest_index);
		bool		CanEndQuestAtState(const std::string& quest_name, const string& state_name);

		LPCHARACTER		GetCurrentCharacterPtr() const { return m_pCurrentCharacter; }
		LPCHARACTER		GetCurrentPartyMember() const { return m_pCurrentPartyMember; }
		PC* GetCurrentPC() const { return m_pCurrentPC; }
		entt::entity	GetCurrentPCEntity() const;
		entt::entity	GetCurrentNPCEntity() const;
		entt::entity	GetPCEntity(lua_State* L);
		entt::entity	GetNPCEntity(lua_State* L);

		void		ClearScript();
		void		SendScript();
		void		AddScript(const std::string& str);

		void		BuildStateIndexToName(const char* questName) const;

		int			GetQuestStateIndex(const std::string& quest_name, const std::string& state_name);
		const char* GetQuestStateName(const std::string& quest_name, const int state_index);

		void		SetSkinStyle(int iStyle);

		void		SetNoSend() { m_bNoSend = true; }

		unsigned int	LoadTimerScript(const std::string& name);

		//unsigned int	RegisterQuestName(const string& name);

		void		RegisterQuest(const std::string& name, unsigned int idx);
		unsigned int 	GetQuestIndexByName(const std::string& name);
		const std::string& GetQuestNameByIndex(unsigned int idx);

		void		RequestSetEventFlag(const std::string& name, int value);

		void		SetEventFlag(const std::string& name, int value);
		int			GetEventFlag(const std::string& name);
		void		BroadcastEventFlagOnLogin(entt::entity ch);

		void		SendEventFlagList(LPCHARACTER ch);

		void		Reload();

		//void		CreateAllButton(const std::string& quest_name, const std::string& button_name);
		void		SetError() { m_bError = true; }
		void		ClearError() { m_bError = false; }
		bool		IsError() { return m_bError; }
		void		WriteRunningStateToSyserr();
		void		QuestErrorImpl(const char* func, int line, const std::string& msg);

		template <typename... Args>
		void QuestErrorFmt(const char* func, int line, fmt::format_string<Args...> fmt, Args&&... args)
		{
			QuestErrorImpl(func, line, fmt::format(fmt, std::forward<Args>(args)...));
		}

		// Legacy printf-style bridge kept only until every questlua call site is migrated.
		void		QuestError(const char* func, int line, const char* fmt, ...);

		void		RegisterNPCVnum(uint32_t dwVnum);

	private:
		uint32_t			m_dwServerTimerArg;

		std::map<std::pair<std::string, uint32_t>, LPEVENT> m_mapServerTimer;

		int				m_iRunningEventIndex;

		std::map<std::string, int>		m_mapEventFlag;

		void			GotoSelectState(QuestState& qs);
		void			GotoPauseState(QuestState& qs);
		void			GotoEndState(QuestState& qs);
		void			GotoInputState(QuestState& qs);
		void			GotoConfirmState(QuestState& qs);
		void			GotoSelectItemState(QuestState& qs);

		lua_State* L;

		bool			m_bNoSend;

		std::set<unsigned int>			m_registeredNPCVnum;
		std::map<unsigned int, NPC>		m_mapNPC;
		std::map<std::string, unsigned int>	m_mapNPCNameID;
		std::map<std::string, unsigned int>	m_mapTimerID;

		QuestState* m_CurrentRunningState;

		PCMap			m_mapPC;

		LPCHARACTER		m_pCurrentCharacter;
		LPCHARACTER		m_pCurrentNPCCharacter;
		LPCHARACTER		m_pCurrentPartyMember;
		PC* m_pCurrentPC;

		std::string			m_strScript;
		int				m_iCurrentSkin;

		struct stringhash
		{
			size_t operator () (const std::string& str) const
			{
				const unsigned char* s = (const unsigned char*)str.c_str();
				const unsigned char* end = s + str.size();
				size_t h = 0;

				while (s < end)
				{
					h *= 16777619;
					h ^= (unsigned char)*(unsigned char*)(s++);
				}

				return h;

			}
		};

		typedef std::unordered_map<std::string, int, stringhash> THashMapQuestName;
		typedef std::unordered_map<unsigned int, std::vector<char> > THashMapQuestStartScript;

		THashMapQuestName			m_hmQuestName;
		THashMapQuestStartScript	m_hmQuestStartScript;
		std::map<unsigned int, std::string>	m_mapQuestNameByIndex;

		bool						m_bError;

	public:
		static bool ExecuteQuestScript(PC& pc, const std::string& quest_name, const int state, const char* code, const int code_size, std::vector<AArgScript*>* pChatScripts = nullptr, bool bUseCache = true);
		static bool ExecuteQuestScript(PC& pc, uint32_t quest_index, const int state, const char* code, const int code_size, std::vector<AArgScript*>* pChatScripts = nullptr, bool bUseCache = true);


		// begin_other_pc_blcok, end_other_pc_block
	public:
		void		BeginOtherPCBlock(uint32_t pid);
		void		EndOtherPCBlock();
		bool		IsInOtherPCBlock();
		PC* GetOtherPCBlockRootPC();
	private:
		PC* m_pOtherPCBlockRootPC;
		std::vector <uint32_t>	m_vecPCStack;
#ifdef __QUEST_RENEWAL__
	public:
		std::map<uint16_t, unsigned int> QuestCategoryIndexMap;
		void ReadQuestCategoryToDict();
		int GetQuestCategoryByQuestIndex(uint16_t q_index);
#endif
	};
};
