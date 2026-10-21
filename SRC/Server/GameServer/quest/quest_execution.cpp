#include "stdafx.h"
#include "quest_runtime.h"

namespace quest {
	QuestState CQuestManager::OpenState(const string& quest_name, int state_index) const
	{
		QuestState qs;
		qs.runtime = m_currentRuntime;
		qs.args=0;
		qs.st = state_index;
		qs.co = lua_newthread(L);
		qs.ico = lua_ref(L, 1/*qs.co*/);
		return qs;
	}


	bool CQuestManager::RunState(QuestState & qs)
	{
        RuntimeScope runtime(*this, qs.runtime, true);
		ClearError();

		m_CurrentRunningState = &qs;

		int results = 0;
		LuaExecutionBudget budget(qs.co);
        int ret = lua_resume(qs.co, L, qs.args, &results);
		if (ret == LUA_OK) {
			GotoEndState(qs);
			return false;
		}
		if (ret == LUA_YIELD)
		{
			if (lua_gettop(qs.co) == 0)
			{
				// end of quest
				GotoEndState(qs);
				return false;
			}

			const char* action = lua_tostring(qs.co, 1);
			if (!action) { SetError(); GotoEndState(qs); return false; }
			if (!strcmp(action, "select"))
			{
				GotoSelectState(qs);
                lua_settop(qs.co, 0);
				return true;
			}

			if (!strcmp(lua_tostring(qs.co, 1), "wait"))
			{
				GotoPauseState(qs);
                lua_settop(qs.co, 0);
				return true;
			}

			if (!strcmp(lua_tostring(qs.co, 1), "input"))
			{
				GotoInputState(qs);
                lua_settop(qs.co, 0);
				return true;
			}

			if (!strcmp(lua_tostring(qs.co, 1), "confirm"))
			{
				GotoConfirmState(qs);
                lua_settop(qs.co, 0);
				return true;
			}

			if (!strcmp(lua_tostring(qs.co, 1), "select_item"))
			{
				GotoSelectItemState(qs);
                lua_settop(qs.co, 0);
				return true;
			}
		}
		else
		{
			sys_err("LUA_ERROR: {}", lua_tostring(qs.co, -1));
		}

		WriteRunningStateToSyserr();
		SetError();

		GotoEndState(qs);
		return false;
	}

	void CQuestManager::CloseState(QuestState& qs, bool aborting) const
	{
        RuntimeScope runtime(*const_cast<CQuestManager*>(this), qs.runtime, true);
        if (qs.runtime && qs.runtime->closed) { qs.co = nullptr; qs.runtime.reset(); qs.chat_scripts.clear(); return; }
		if (qs.co)
		{
			// Run Lua 5.5 to-be-closed variables before dropping the registry reference.
            {
                const bool staging = RuntimeIsStaging(qs.co);
                if (aborting) SetRuntimeStaging(qs.co, true);
                LuaExecutionBudget budget(qs.co);
                const int status = lua_closethread(qs.co, L);
                if (aborting) SetRuntimeStaging(qs.co, staging);
                if (status != LUA_OK) LOG_ERROR("QUEST coroutine close failed: {}", lua_tostring(qs.co, -1));
            }
			lua_unref(L, qs.ico);
			qs.co = nullptr;
		}
        qs.runtime.reset();
        qs.chat_scripts.clear();
	}
}
