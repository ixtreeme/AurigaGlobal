#pragma once

#include "Packet.h"

class CPythonGuild : public CSingleton<CPythonGuild>
{
	public:
		enum
		{
			GUILD_SKILL_MAX_NUM = 12,
#ifndef ADVANCED_GUILD_INFO
			ENEMY_GUILD_SLOT_MAX_COUNT = 6,
#else
			ENEMY_GUILD_SLOT_MAX_COUNT = 2,
#endif
		};

		typedef struct SGulidInfo
		{
			uint32_t dwGuildID;
			char szGuildName[GUILD_NAME_MAX_LEN+1];
			uint32_t dwMasterPID;
			uint32_t dwGuildLevel;
			uint32_t dwCurrentExperience;
			uint32_t dwCurrentMemberCount;
			uint32_t dwMaxMemberCount;
			uint32_t dwGuildMoney;
			bool bHasLand;

#ifdef ADVANCED_GUILD_INFO
			int trophies;
			int win;
			int loss;
			int draw;
#endif
		} TGuildInfo;

		typedef struct SGuildGradeData
		{
			SGuildGradeData() : byAuthorityFlag(0) {}
			SGuildGradeData(uint8_t byAuthorityFlag_, const char * c_szName_) : byAuthorityFlag(byAuthorityFlag_), strName(c_szName_) {}
			uint8_t byAuthorityFlag;
			std::string strName;
		} TGuildGradeData;
		typedef std::map<uint8_t, TGuildGradeData> TGradeDataMap;

		typedef struct SGuildMemberData
		{
			uint32_t dwPID;

			std::string strName;
			uint8_t byGrade;
			uint8_t byJob;
			uint8_t byLevel;
			uint8_t byGeneralFlag;
			uint32_t dwOffer;

			SGuildMemberData() : dwPID(0), byGrade(0), byJob(0), byLevel(0), byGeneralFlag(0), dwOffer(0) {}
		} TGuildMemberData;
		typedef std::vector<TGuildMemberData> TGuildMemberDataVector;

		typedef struct SGuildBoardCommentData
		{
			uint32_t dwCommentID;
			std::string strName;
			std::string strComment;

			SGuildBoardCommentData() : dwCommentID(0) {}
		} TGuildBoardCommentData;
		typedef std::vector<TGuildBoardCommentData> TGuildBoardCommentDataVector;

		typedef struct SGuildSkillData
		{
			uint8_t bySkillPoint;
			uint8_t bySkillLevel[GUILD_SKILL_MAX_NUM];
			WORD wGuildPoint;
			WORD wMaxGuildPoint;
		} TGuildSkillData;

		typedef std::map<uint32_t, std::string> TGuildNameMap;
#ifdef ENABLE_GUILD_RENEWAL_BY_RAZOR93
		typedef std::map<uint32_t, uint8_t> TGuildLevelMap;
#endif

	public:
		CPythonGuild();
		virtual ~CPythonGuild();

		void Destroy();

		void EnableGuild();
		void SetGuildMoney(uint32_t dwMoney);
#ifdef ADVANCED_GUILD_INFO
		void SetGuildTrophies(int iTrophies);
#endif
		void SetGuildEXP(uint8_t byLevel, uint32_t dwEXP);
		void SetGradeData(uint8_t byGradeNumber, TGuildGradeData rGuildGradeData);
		void SetGradeName(uint8_t byGradeNumber, const char * c_szName);
		void SetGradeAuthority(uint8_t byGradeNumber, uint8_t byAuthority);
		void ClearComment();
		void RegisterComment(uint32_t dwCommentID, const char * c_szName, const char * c_szComment);
		void RegisterMember(TGuildMemberData & rGuildMemberData);
		void ChangeGuildMemberGrade(uint32_t dwPID, uint8_t byGrade);
		void ChangeGuildMemberGeneralFlag(uint32_t dwPID, uint8_t byFlag);
		void RemoveMember(uint32_t dwPID);
		void RegisterGuildName(uint32_t dwID, const char * c_szName);
#ifdef ENABLE_GUILD_RENEWAL_BY_RAZOR93
		void RegisterGuildName(uint32_t dwID, const char * c_szName, uint8_t byLevel);
#endif

		bool IsMainPlayer(uint32_t dwPID);
		bool IsGuildEnable();
		TGuildInfo & GetGuildInfoRef();
		bool GetGradeDataPtr(uint32_t dwGradeNumber, TGuildGradeData ** ppData);
		const TGuildBoardCommentDataVector & GetGuildBoardCommentVector();
		uint32_t GetMemberCount();
		bool GetMemberDataPtr(uint32_t dwIndex, TGuildMemberData ** ppData);
		bool GetMemberDataPtrByPID(uint32_t dwPID, TGuildMemberData ** ppData);
		bool GetMemberDataPtrByName(const char * c_szName, TGuildMemberData ** ppData);
		uint32_t GetGuildMemberLevelSummary();
		uint32_t GetGuildMemberLevelAverage();
		uint32_t GetGuildExperienceSummary();
#ifdef ADVANCED_GUILD_INFO
		int GetGuildTrohpies();
		int GetGuildWin();
		int GetGuildLoss();
		int GetGuildDraw();
#endif
		TGuildSkillData & GetGuildSkillDataRef();
		bool GetGuildName(uint32_t dwID, std::string * pstrGuildName);
#ifdef ENABLE_GUILD_LV_SHOW_ABOVE_CHAR_RAZOR93
		int GetGuildLevel(uint32_t dwGuildID);//razor93
#endif

		uint32_t GetGuildID();
		bool HasGuildLand();

		void StartGuildWar(uint32_t dwEnemyGuildID);
		void EndGuildWar(uint32_t dwEnemyGuildID);
		uint32_t GetEnemyGuildID(uint32_t dwIndex);
		bool IsDoingGuildWar();

	protected:
		void __CalculateLevelAverage();
		void __SortMember();
		bool __IsGradeData(uint8_t byGradeNumber);

		void __Initialize();

	protected:
		TGuildInfo m_GuildInfo;
		TGradeDataMap m_GradeDataMap;
		TGuildMemberDataVector m_GuildMemberDataVector;
		TGuildBoardCommentDataVector m_GuildBoardCommentVector;
		TGuildSkillData m_GuildSkillData;
		TGuildNameMap m_GuildNameMap;
#ifdef ENABLE_GUILD_RENEWAL_BY_RAZOR93
		TGuildLevelMap m_GuildLevelMap;
#endif
		uint32_t m_adwEnemyGuildID[ENEMY_GUILD_SLOT_MAX_COUNT];

		uint32_t m_dwMemberLevelSummary;
		uint32_t m_dwMemberLevelAverage;
		uint32_t m_dwMemberExperienceSummary;

		bool m_bGuildEnable;
};