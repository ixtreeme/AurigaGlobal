#ifndef __INC_LOG_MANAGER_H__
#define __INC_LOG_MANAGER_H__

#include <SQL/AsyncSQL.h>
#include <entt/entt.hpp>
#include "any_function.h"
#include "locale_service.h"

// #ifdef ENABLE_NEWSTUFF
enum log_level {LOG_LEVEL_NONE=0, LOG_LEVEL_MIN=1, LOG_LEVEL_MID=2, LOG_LEVEL_MAX=3};
// #endif

#ifdef ENABLE_NEWSTUFF
#define LOG_LEVEL_CHECK_N_RET(x) { if (g_iDbLogLevel < x) return; }
#else
#define LOG_LEVEL_CHECK_N_RET(x) { }
#endif

#ifdef ENABLE_NEWSTUFF
#define LOG_LEVEL_CHECK(x, fnc)	\
	{\
		if (g_iDbLogLevel >= (x))\
			fnc;\
	}
#else
#define LOG_LEVEL_CHECK(x, fnc)	{ fnc; }
#endif


class LogManager : public singleton<LogManager>
{
	public:
		LogManager();
		virtual ~LogManager();

		bool		IsConnected();

		bool		Connect(const char * host, const int port, const char * user, const char * pwd, const char * db);


		void		CharLog(uint32_t dwPID, uint32_t x, uint32_t y, uint32_t dw, const char * c_pszText, const char * c_pszHint, const char * c_pszIP);
		void		CharLog(entt::entity character, uint32_t dw, const char * c_pszText, const char * c_pszHint);

		void		LoginLog(bool isLogin, uint32_t dwAccountID, uint32_t dwPID, uint8_t bLevel, uint8_t bJob, uint32_t dwPlayTime);

		void		MoneyLog(uint8_t type, uint32_t vnum, int64_t gold);

		void		HackLog(const char * c_pszHackName, const char * c_pszLogin, const char * c_pszName, const char * c_pszIP);
		void		HackLog(const char * c_pszHackName, entt::entity ch);
		void		HackCRCLog(const char * c_pszHackName, const char * c_pszLogin, const char * c_pszName, const char * c_pszIP, uint32_t dwCRC);
#ifdef __ATTR_TRANSFER_SYSTEM__
#endif
		void		GMCommandLog(uint32_t dwPID, const char * szName, const char * szIP, uint8_t byChannel, const char * szCommand);
		void		SpeedHackLog(uint32_t pid, uint32_t x, uint32_t y, int hack_count);
		void		ChangeNameLog(uint32_t pid, const char * old_name, const char * new_name, const char * ip);
		void		ShoutLog(uint8_t bChannel, uint8_t bEmpire, const char * pszText);
		void		LevelLog(entt::entity pChar, unsigned int level, unsigned int playhour);
		void		BootLog(const char * c_pszHostName, uint8_t bChannel);
		void		QuestRewardLog(const char * c_pszQuestName, uint32_t dwPID, uint32_t dwLevel, int iValue1, int iValue2);
		void		DetailLoginLog(bool isLogin, entt::entity chEntity);
		void		DragonSlayLog(uint32_t dwGuildID, uint32_t dwDragonVnum, uint32_t dwStartTime, uint32_t dwEndTime);
		void		ChatLog(uint32_t where, uint32_t who_id, const char* who_name, uint32_t whom_id, const char* whom_name, const char* type, const char* msg, const char* ip);

#ifdef ENABLE_ACCE_SYSTEM
#endif
#ifdef ENABLE_ANTICHEAT
		void	CheatLog(int32_t pid, std::string hostname, std::string reason);
#endif

		size_t EscapeString(char* dst, size_t dstSize, const char *src, size_t srcSize);
	private:
		void		Query(const char * c_pszFormat, ...);

		CAsyncSQL	m_sql;
		bool		m_bIsConnect;
};

#endif
