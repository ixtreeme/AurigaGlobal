#pragma once

class CPythonMessenger : public CSingleton<CPythonMessenger>
{
	public:
		typedef std::set<std::string> TFriendNameMap;
		typedef std::map<std::string, uint8_t> TGuildMemberStateMap;

		enum EMessengerGroupIndex
		{
			MESSENGER_GRUOP_INDEX_FRIEND,
			MESSENGER_GRUOP_INDEX_GUILD,
#ifdef ENABLE_MESSENGER_TEAM
			MESSENGER_GROUP_INDEX_TEAM,
#endif
#ifdef ENABLE_MESSENGER_HELPER
			MESSENGER_GROUP_INDEX_HELPER,
#endif
		};

	public:
		CPythonMessenger();
		virtual ~CPythonMessenger();

		void Destroy();

		// Friend
		void RemoveFriend(const char * c_szKey);
		void OnFriendLogin(const char * c_szKey);
		void OnFriendLogout(const char * c_szKey);
		
		bool IsFriendByKey(const char * c_szKey);
		bool IsFriendByName(const char * c_szName);
#ifdef ENABLE_MESSENGER_TEAM
#ifdef ENABLE_MULTI_LANGUAGE
		void AppendLanguage(const char * name, const char * lang);
#endif
		void OnTeamLogin(const char * c_szKey);
		void OnTeamLogout(const char * c_szKey);
#endif
#ifdef ENABLE_MESSENGER_HELPER
		void OnHelperLogin(const char * c_szKey);
		void OnHelperLogout(const char * c_szKey);
#endif
		// Guild
		void AppendGuildMember(const char * c_szName);
		void RemoveGuildMember(const char * c_szName);
		void RemoveAllGuildMember();
		void LoginGuildMember(const char * c_szName);
		void LogoutGuildMember(const char * c_szName);
		void RefreshGuildMember();

		void SetMessengerHandler(PyObject* poHandler);

	protected:
		TFriendNameMap m_FriendNameMap;
		TGuildMemberStateMap m_GuildMemberStateMap;
#ifdef ENABLE_MESSENGER_TEAM
		TFriendNameMap m_TeamNameMap;
#endif
#ifdef ENABLE_MESSENGER_HELPER
		TFriendNameMap m_HelperNameMap;
#endif

	private:
		PyObject * m_poMessengerHandler;
};
