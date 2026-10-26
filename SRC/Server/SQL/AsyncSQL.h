#pragma once

#include <Core/stdafx.h>
#include <Core/log.h>

#include <string>
#include <queue>
#include <vector>
#include <map>
#include <deque>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <mysql/mysql.h>
#include <mysql/errmsg.h>
#include <mysql/mysqld_error.h>

#include "Semaphore.h"
#include "QueueDrain.h"

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

	// Prepared statements have no MYSQL_RES; their rows are materialized
	// as strings so FetchRow() behaves exactly like mysql_fetch_row().
	MYSQL_ROW FetchRow();
	// Byte lengths of the last fetched row, including embedded NUL bytes.
	unsigned long * FetchLengths();

	MYSQL_RES *	pSQLResult;
	uint64_t		uiNumRows;
	uint64_t		uiAffectedRows;
	uint64_t		uiInsertID;

	bool			bPrepared { false };
	std::deque<std::string>			preparedCells;
	std::vector<std::vector<char *>>	preparedRows;
	std::vector<std::vector<unsigned long>> preparedLengths;
	size_t			preparedRowPos { 0 };
	bool preparedRowValid { false };
} SQLResult;

#include "PreparedTransaction.h"

typedef struct _SQLMsg
{
	_SQLMsg() : m_pkSQL(nullptr), iID(0), uiResultPos(0), pvUserData(nullptr), bReturn(false), uiSQLErrno(0), bPrepared(false)
	{
	}

	~_SQLMsg()
	{
		queueDrain.Cancel();
		auto first = vec_pkResult.begin();
		auto past = vec_pkResult.end();

		while (first != past)
			delete *(first++);

		vec_pkResult.clear();
	}

	void Store()
	{
		int next = -1;
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
			if (!pRes->pSQLResult && mysql_field_count(m_pkSQL))
			{
				uiSQLErrno = mysql_errno(m_pkSQL);
				if (!uiSQLErrno) uiSQLErrno = CR_UNKNOWN_ERROR;
				return;
			}
			next = mysql_next_result(m_pkSQL);
		} while (next == 0);
		if (next > 0)
		{
			uiSQLErrno = mysql_errno(m_pkSQL);
			if (!uiSQLErrno) uiSQLErrno = CR_UNKNOWN_ERROR;
		}
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

		pRes->bPrepared = true;
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

	bool bTransaction { false };
	SqlTransaction transaction;
	SqlTransactionOutcome transactionOutcome { SqlTransactionOutcome::NotStarted };
	size_t transactionFailedStep { SqlTransactionNoStep };
	std::vector<size_t> transactionResultOffsets;
	uint64_t queueSequence = 0;
	bool queueUnconfirmed = false;
	SqlQueueDrain queueDrain;
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
		enum class ConnectionState { NotStarted, Connecting, Ready, Failed };
		ConnectionState GetConnectionState() const { return m_connectionState.load(); }
		bool WaitForConnection(std::chrono::milliseconds timeout);
		// Configure before Setup; zero preserves the connector default.
		void SetTimeouts(unsigned int seconds) { m_timeoutSeconds = seconds; }
		void SetRetryConnectionErrors(bool retry) { m_retryConnectionErrors = retry; }
		void SetFlushOnQuit(bool flush) { m_flushOnQuit = flush; }
		bool		QueryLocaleSet();

		void		AsyncQuery(const char * c_pszQuery);
		void		ReturnQuery(const char * c_pszQuery, void * pvUserData);
		SQLMsg *	DirectQuery(const char * c_pszQuery);

		// Prepared (parameterized) variants: the values in params are bound
		// through the binary protocol, no escaping/quoting is required.
		void		AsyncQueryPrepared(const char * c_pszQuery, const SqlParams & params);
		void		ReturnQueryPrepared(const char * c_pszQuery, const SqlParams & params, void * pvUserData);
		SQLMsg *	DirectQueryPrepared(const char * c_pszQuery, const SqlParams & params);

		// One owned job: no queue interleaving, reconnect or automatic replay inside it.
		// Direct variant requires Setup(..., bNoThread=true). Inspect transactionOutcome.
		SQLMsg * DirectTransactionPrepared(SqlTransaction transaction);
		void ReturnTransactionPrepared(SqlTransaction transaction, void * userData);
		// Enqueued after the source is stopped and its dirty caches are flushed.
		// Compare the completed position with the last VERIFIED baseline. A new
		// barrier does not erase old errors or ambiguous/retried write attempts.
		SqlQueueDrain DrainQueue();
		SqlQueuePosition QueueOrigin() const { return {m_queueLane, 0, 0, 0, 0}; }

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
		bool		EnsureConnectionReady();

		MYSQL_STMT *	GetOrPrepare(const std::string & sql, uint32_t & error);
		void		DropPrepared(const std::string & sql);
		void		ClearPrepared();

		// Executes one prepared message; on success stores the result set
		// (SELECT) or the affected rows / insert id (write) into p and
		// returns true.
		bool		TryExecutePrepared(SQLMsg * p);
		bool TryExecuteTransaction(SQLMsg * p);
		bool ExecuteQueued(SQLMsg *p);
		void RecordQueueFailure(SQLMsg *p);
		void CancelQueuedDrains();
		void InvalidateConnection();

		// Materializes a prepared result set into p as string cells.
		bool		StorePreparedResult(SQLMsg * p, MYSQL_STMT * stmt, MYSQL_RES * meta);

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

		std::atomic<bool> m_bEnd;
		std::atomic<ConnectionState> m_connectionState { ConnectionState::NotStarted };
		std::mutex m_connectionMutex;
		std::condition_variable m_connectionChanged;
		void SetConnectionState(ConnectionState state);
		unsigned int m_timeoutSeconds { 0 };
		bool m_retryConnectionErrors { true };
		bool m_flushOnQuit { true };
		bool m_mysqlInitialized { false };
		bool m_directConnection { false };
		bool m_inPreparedTransaction { false };
		unsigned long m_transactionThreadId { 0 };
		const uint64_t m_queueLane;
		uint64_t m_queueSequence = 0; // Assigned under the producer queue mutex.
		SqlQueuePosition m_queuePosition; // Worker only; published through drain state.

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
