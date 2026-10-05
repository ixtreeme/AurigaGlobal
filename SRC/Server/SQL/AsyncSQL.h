#pragma once

#include <Core/stdafx.h>
#include <Core/log.h>

#include <string>
#include <queue>
#include <vector>
#include <map>
#include <mysql/mysql.h>
#include <mysql/errmsg.h>
#include <mysql/mysqld_error.h>

#include "Semaphore.h"

#define QUERY_MAX_LEN 8192

typedef struct _SQLResult
{
	_SQLResult()
	   	: pSQLResult(nullptr), uiNumRows(0), uiAffectedRows(0), uiInsertID(0)
	{
	}

	~_SQLResult()
	{
		if (pSQLResult)
		{
			mysql_free_result(pSQLResult);
			pSQLResult = nullptr;
		}
	}

	MYSQL_RES *	pSQLResult;
	uint64_t		uiNumRows;
	uint64_t		uiAffectedRows;
	uint64_t		uiInsertID;
} SQLResult;

// Out-of-band parameter for server-side prepared statements. The value is
// sent through the MySQL binary protocol, so no quoting/escaping is needed
// and the statement text never changes between executions (statement cache).
enum class SqlParamType : uint8_t
{
	Null,
	Signed,
	Unsigned,
	Real,
	Text,
	Blob,
};

struct SqlParam
{
	SqlParamType	type { SqlParamType::Null };
	int64_t		i64 { 0 };
	uint64_t	u64 { 0 };
	double		f64 { 0.0 };
	std::string	bytes;

	static SqlParam Null() { return SqlParam {}; }
	static SqlParam Int(int64_t v) { SqlParam p; p.type = SqlParamType::Signed; p.i64 = v; return p; }
	static SqlParam UInt(uint64_t v) { SqlParam p; p.type = SqlParamType::Unsigned; p.u64 = v; return p; }
	static SqlParam Real(double v) { SqlParam p; p.type = SqlParamType::Real; p.f64 = v; return p; }
	static SqlParam Text(std::string v) { SqlParam p; p.type = SqlParamType::Text; p.bytes = std::move(v); return p; }
	static SqlParam Text(const char * v) { return v ? Text(std::string(v)) : Null(); }
	static SqlParam Blob(std::string v) { SqlParam p; p.type = SqlParamType::Blob; p.bytes = std::move(v); return p; }
};

using SqlParams = std::vector<SqlParam>;

typedef struct _SQLMsg
{
	_SQLMsg() : m_pkSQL(nullptr), iID(0), uiResultPos(0), pvUserData(nullptr), bReturn(false), uiSQLErrno(0), bPrepared(false)
	{
	}

	~_SQLMsg()
	{
		auto first = vec_pkResult.begin();
		auto past = vec_pkResult.end();

		while (first != past)
			delete *(first++);

		vec_pkResult.clear();
	}

	void Store()
	{
		do
		{
			SQLResult * pRes = new SQLResult;

			pRes->pSQLResult = mysql_store_result(m_pkSQL);
			pRes->uiInsertID = mysql_insert_id(m_pkSQL);
			pRes->uiAffectedRows = mysql_affected_rows(m_pkSQL);

			if (pRes->pSQLResult)
			{
				pRes->uiNumRows = mysql_num_rows(pRes->pSQLResult);
			}
			else
			{
				pRes->uiNumRows = 0;
			}

			vec_pkResult.push_back(pRes);
		} while (!mysql_next_result(m_pkSQL));
	}

	SQLResult * Get()
	{
		if (uiResultPos >= vec_pkResult.size())
			return nullptr;

		return vec_pkResult[uiResultPos];
	}

	// Prepared statements (writes) have no text result set; the outcome is
	// the affected row / insert id pair.
	void StorePrepared(uint64_t affectedRows, uint64_t insertID)
	{
		SQLResult * pRes = new SQLResult;

		pRes->uiAffectedRows = affectedRows;
		pRes->uiInsertID = insertID;

		vec_pkResult.push_back(pRes);
	}

	MYSQL *			m_pkSQL;
	int				iID;
	std::string			stQuery;

	std::vector<SQLResult *>	vec_pkResult;
	uint32_t		uiResultPos;

	void *			pvUserData;
	bool			bReturn;

	uint32_t		uiSQLErrno;

	// When set, the message is executed as a server-side prepared
	// statement and vecParams is bound with mysql_stmt_bind_param.
	bool			bPrepared;
	SqlParams		vecParams;
} SQLMsg;

class CAsyncSQL
{
	public:
		CAsyncSQL();
		virtual ~CAsyncSQL();

		void		Quit();

		bool		Setup(const char * c_pszHost, const char * c_pszUser, const char * c_pszPassword, const char * c_pszDB, const char * c_pszLocale, 
			bool bNoThread = false, int iPort = 0);

		bool		Connect();
		bool		QueryLocaleSet();

		void		AsyncQuery(const char * c_pszQuery);
		void		ReturnQuery(const char * c_pszQuery, void * pvUserData);
		SQLMsg *	DirectQuery(const char * c_pszQuery);

		// Prepared (parameterized) variants: the values in params are bound
		// through the binary protocol, no escaping/quoting is required.
		void		AsyncQueryPrepared(const char * c_pszQuery, const SqlParams & params);
		void		ReturnQueryPrepared(const char * c_pszQuery, const SqlParams & params, void * pvUserData);
		SQLMsg *	DirectQueryPrepared(const char * c_pszQuery, const SqlParams & params);

		uint32_t		CountQuery();
		uint32_t		CountResult();

		void		PushResult(SQLMsg * p);
		bool		PopResult(SQLMsg ** pp);

		void		ChildLoop();

		MYSQL *		GetSQLHandle();

		int			CountQueryFinished();
		void		ResetQueryFinished();

		uint32_t		EscapeString(char* dst, uint64_t dstSize, const char *src, uint32_t srcSize);

	protected:
		void		Destroy();

		// Refreshes the reconnect/locale state and drops the prepared
		// statement cache when the underlying connection changed.
		void		EnsureConnectionReady();

		MYSQL_STMT *	GetOrPrepare(const std::string & sql);
		void		DropPrepared(const std::string & sql);
		void		ClearPrepared();

		// Executes one prepared message; on success stores the affected
		// rows / insert id into p and returns true.
		bool		TryExecutePrepared(SQLMsg * p);

		void		PushQuery(SQLMsg * p);

		bool		PeekQuery(SQLMsg ** pp);
		bool		PopQuery(int iID);

		bool		PeekQueryFromCopyQueue(SQLMsg ** pp );
		int			CopyQuery();
		bool		PopQueryFromCopyQueue();

	public:
		int			GetCopiedQueryCount();
		void		ResetCopiedQueryCount();
		void		AddCopiedQueryCount( int iCopiedQuery );

		//private:
	protected:
		MYSQL m_hDB;

		std::string	m_stHost;
		std::string	m_stUser;
		std::string	m_stPassword;
		std::string	m_stDB;
		std::string	m_stLocale;

		int	m_iMsgCount;
		int m_iPort;

		std::queue<SQLMsg *> m_queue_query;
		std::queue<SQLMsg *> m_queue_query_copy;
		//std::map<int, SQLMsg *>	m_map_kSQLMsgUnfinished;

		std::queue<SQLMsg *> m_queue_result;

		// Prepared statement handles are cached per connection. The cache
		// is cleared whenever the connection was re-established or the
		// character set changed.
		std::map<std::string, MYSQL_STMT *> m_map_prepared;

		volatile bool m_bEnd;

#ifndef _WIN32
		pthread_t m_hThread;
		pthread_mutex_t	* m_mtxQuery;
		pthread_mutex_t	* m_mtxResult;
#else
		HANDLE m_hThread;
		CRITICAL_SECTION* m_mtxQuery;
		CRITICAL_SECTION* m_mtxResult;
#endif

		CSemaphore m_sem;

		int	m_iQueryFinished;

		uint32_t m_ulThreadID;
		int	m_iCopiedQuery;
};

class CAsyncSQL2 : public CAsyncSQL
{
	public:
		void SetLocale ( const std::string & stLocale );
};
