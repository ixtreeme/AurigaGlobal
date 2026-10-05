#pragma once

#include <SQL/AsyncSQL.h>
#include "any_function.h"

enum
{
	QUERY_TYPE_RETURN = 1,
	QUERY_TYPE_FUNCTION = 2,
	QUERY_TYPE_AFTER_FUNCTION = 3,
};

enum
{
	QID_SAFEBOX_SIZE,
	QID_MOUNT_INVENTORY_LOAD,
	QID_DB_STRING,
	QID_AUTH_LOGIN,
	QID_LOTTO,
	QID_HIGHSCORE_REGISTER,
	QID_HIGHSCORE_SHOW,

	// BLOCK_CHAT
	QID_BLOCK_CHAT_LIST,
	// END_OF_BLOCK_CHAT

		// PROTECT_CHILD_FOR_NEWCIBN
	QID_PROTECT_CHILD,
	// END_PROTECT_CHILD_FOR_NEWCIBN
};

class CQueryInfo
{
	public:
		int	iQueryType;
};

class CReturnQueryInfo : public CQueryInfo
{
	public:
		int	iType;
		uint32_t	dwIdent;
		void			*	pvData;
};

class CFuncQueryInfo : public CQueryInfo
{
	public:
		any_function f;
};

class CFuncAfterQueryInfo : public CQueryInfo
{
	public:
		any_void_function f;
};

class CLoginData;


class DBManager : public singleton<DBManager>
{
	public:
		DBManager();
		virtual ~DBManager();

		bool			IsConnected();

		bool			Connect(const char * host, const int port, const char * user, const char * pwd, const char * db);
		void			Query(const char * c_pszFormat, ...);

		SQLMsg *		DirectQuery(const char * c_pszFormat, ...);
		void			ReturnQuery(int iType, uint32_t dwIdent, void* pvData, const char * c_pszFormat, ...);

		// Prepared (parameterized) variants for write statements. The SQL
		// text must use '?' placeholders; the values travel out-of-band
		// through the binary protocol, so no escaping/quoting is needed.
		void			QueryPrepared(const char * c_pszSql, const SqlParams & params);
		SQLMsg *		DirectQueryPrepared(const char * c_pszSql, const SqlParams & params);
		void			ReturnQueryPrepared(int iType, uint32_t dwIdent, void * pvData, const char * c_pszSql, const SqlParams & params);

		void			Process();
		void			AnalyzeReturnQuery(SQLMsg * pmsg);
		void			SendMoneyLog(uint8_t type, uint32_t vnum, int64_t gold);


		void			LoginPrepare(LPDESC d, uint32_t * pdwClientKey, int * paiPremiumTimes = nullptr);
		void			SendAuthLogin(LPDESC d);
		void			SendLoginPing(const char * c_pszLogin);

		void			InsertLoginData(CLoginData * pkLD);
		void			DeleteLoginData(CLoginData * pkLD);
		CLoginData *		GetLoginData(uint32_t dwKey);


		uint32_t			CountQuery()		{ return m_sql.CountQuery(); }
		uint32_t			CountQueryResult()	{ return m_sql.CountResult(); }
		void			ResetQueryResult()	{ m_sql.ResetQueryFinished(); }

		void			LoadDBString();
		const std::string &	GetDBString(const std::string& key);
		const std::vector<std::string> & GetGreetMessage();

		template<class Functor> void FuncQuery(Functor f, const char * c_pszFormat, ...);
		template<class Functor> void FuncAfterQuery(Functor f, const char * c_pszFormat, ...);

		template<class Functor> void FuncQueryPrepared(Functor f, const char * c_pszSql, const SqlParams & params);
		template<class Functor> void FuncAfterQueryPrepared(Functor f, const char * c_pszSql, const SqlParams & params);

		uint32_t EscapeString(char* dst, uint64_t dstSize, const char *src, uint32_t srcSize);

	private:
		SQLMsg *				PopResult();

		CAsyncSQL				m_sql;
		CAsyncSQL				m_sql_direct;
		bool					m_bIsConnect;

		std::map<std::string, std::string>	m_map_dbstring;
		std::vector<std::string>		m_vec_GreetMessage;
		std::map<uint32_t, CLoginData *>		m_map_pkLoginData;
};

template <class Functor> void DBManager::FuncQuery(Functor f, const char* c_pszFormat, ...)
{
	char szQuery[4096];
	va_list args;

	va_start(args, c_pszFormat);
	vsnprintf(szQuery, 4096, c_pszFormat, args);
	va_end(args);

	CFuncQueryInfo * p = M2_NEW CFuncQueryInfo;

	p->iQueryType = QUERY_TYPE_FUNCTION;
	p->f = f;

	m_sql.ReturnQuery(szQuery, p);
}

template <class Functor> void DBManager::FuncAfterQuery(Functor f, const char* c_pszFormat, ...)
{
	char szQuery[4096];
	va_list args;

	va_start(args, c_pszFormat);
	vsnprintf(szQuery, 4096, c_pszFormat, args);
	va_end(args);

	CFuncAfterQueryInfo * p = M2_NEW CFuncAfterQueryInfo;

	p->iQueryType = QUERY_TYPE_AFTER_FUNCTION;
	p->f = f;

	m_sql.ReturnQuery(szQuery, p);
}

template <class Functor> void DBManager::FuncQueryPrepared(Functor f, const char* c_pszSql, const SqlParams& params)
{
	CFuncQueryInfo * p = M2_NEW CFuncQueryInfo;

	p->iQueryType = QUERY_TYPE_FUNCTION;
	p->f = f;

	m_sql.ReturnQueryPrepared(c_pszSql, params, p);
}

template <class Functor> void DBManager::FuncAfterQueryPrepared(Functor f, const char* c_pszSql, const SqlParams& params)
{
	CFuncAfterQueryInfo * p = M2_NEW CFuncAfterQueryInfo;

	p->iQueryType = QUERY_TYPE_AFTER_FUNCTION;
	p->f = f;

	m_sql.ReturnQueryPrepared(c_pszSql, params, p);
}

/// Quoted and escaped SQL string literal built with the direct connection's
// charset. Use it for every user controlled value instead of interpolating
// the raw string:
//   snprintf(q, sizeof(q), "... WHERE name=%s", SqlString(name).c_str());
class SqlString
{
	public:
		explicit SqlString(const char * value);
		explicit SqlString(const std::string & value);
		const char * c_str() const { return m_value.c_str(); }

	private:
		std::string m_value;
};

///////////////////////////////////////////////////////////////
typedef struct SHighscoreRegisterQueryInfo
{
	char    szBoard[20+1];
	uint32_t   dwPID;
	int     iValue;
	bool    bOrder;
} THighscoreRegisterQueryInfo;



// ACCOUNT_DB
class AccountDB : public singleton<AccountDB>
{
	public:
		AccountDB();

		bool IsConnected();
		bool Connect(const char * host, const int port, const char * user, const char * pwd, const char * db);
		bool ConnectAsync(const char * host, const int port, const char * user, const char * pwd, const char * db, const char * locale);

		SQLMsg* DirectQuery(const char * query);
		void ReturnQuery(int iType, uint32_t dwIdent, void * pvData, const char * c_pszFormat, ...);
		void AsyncQuery(const char* query);

		SQLMsg* DirectQueryPrepared(const char * c_pszSql, const SqlParams & params);
		void ReturnQueryPrepared(int iType, uint32_t dwIdent, void * pvData, const char * c_pszSql, const SqlParams & params);
		void AsyncQueryPrepared(const char * c_pszSql, const SqlParams & params);

		void SetLocale(const std::string & stLocale);

		void Process();

	private:
		SQLMsg * PopResult();
		void AnalyzeReturnQuery(SQLMsg * pMsg);

		CAsyncSQL2	m_sql_direct;
		CAsyncSQL2	m_sql;
		bool		m_IsConnect;

};
//END_ACCOUNT_DB
