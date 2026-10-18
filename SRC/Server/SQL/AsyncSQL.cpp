#include <ctime>
#include <cstdlib>
#include <cstring>
#include <string>
#include <print>
#include <mysql/mysql.h>

#include "AsyncSQL.h"
#include "Core/Logging.hpp"

// TODO: Consider providing platform-independent mutex class.
#ifndef _WIN32
#define MUTEX_LOCK(mtx) pthread_mutex_lock(mtx)
#define MUTEX_UNLOCK(mtx) pthread_mutex_unlock(mtx)
#else
#define MUTEX_LOCK(mtx) ::EnterCriticalSection(mtx)
#define MUTEX_UNLOCK(mtx) ::LeaveCriticalSection(mtx)
#endif

MYSQL_ROW SQLResult::FetchRow()
{
	if (pSQLResult)
		return mysql_fetch_row(pSQLResult);

	if (preparedRowPos >= preparedRows.size())
		return nullptr;

	return preparedRows[preparedRowPos++].data();
}

namespace
{
	bool IsConnectionError(uint32_t uiSQLErrno)
	{
		switch (uiSQLErrno)
		{
			case CR_SOCKET_CREATE_ERROR:
			case CR_CONNECTION_ERROR:
			case CR_IPSOCK_ERROR:
			case CR_UNKNOWN_HOST:
			case CR_SERVER_GONE_ERROR:
			case CR_SERVER_LOST:
			case CR_CONN_HOST_ERROR:
			case ER_NOT_KEYFILE:
			case ER_CRASHED_ON_USAGE:
			case ER_CANT_OPEN_FILE:
			case ER_HOST_NOT_PRIVILEGED:
			case ER_HOST_IS_BLOCKED:
			case ER_PASSWORD_NOT_ALLOWED:
			case ER_PASSWORD_NO_MATCH:
			case ER_CANT_CREATE_THREAD:
			case ER_INVALID_USE_OF_NULL:
				return true;

			default:
				return false;
		}
	}

	// First read of a prepared result cell. A longer value is reported as
	// truncated with its full length and read again whole.
	constexpr size_t PREPARED_CELL_CAPACITY = 256;

	// Reads one column of the current row whole, after mysql_stmt_fetch
	// reported it longer than its bound buffer.
	bool FetchWholeColumn(MYSQL_STMT * stmt, unsigned int column, unsigned long length, std::string & out)
	{
		out.assign(length, '\0');

		MYSQL_BIND bind;
		memset(&bind, 0, sizeof(bind));

		unsigned long fetched = 0;
		my_bool isNull = 0;

		bind.buffer_type = MYSQL_TYPE_STRING;
		bind.buffer = out.data();
		bind.buffer_length = length;
		bind.length = &fetched;
		bind.is_null = &isNull;

		if (mysql_stmt_fetch_column(stmt, &bind, column, 0))
			return false;

		out.resize(fetched < length ? fetched : length);
		return true;
	}
}

CAsyncSQL::CAsyncSQL()
	: m_stHost(""), m_stUser(""), m_stPassword(""), m_stDB(""), m_stLocale(""),
	m_iMsgCount(0), m_iPort(0), m_bEnd(false),
#ifndef _WIN32
	m_hThread(0), 
#else
	m_hThread(INVALID_HANDLE_VALUE),
#endif
	m_mtxQuery(nullptr), m_mtxResult(nullptr),
	m_iQueryFinished(0), m_ulThreadID(0), m_iCopiedQuery(0)
{
	memset( &m_hDB, 0, sizeof(m_hDB) );
}

CAsyncSQL::~CAsyncSQL()
{
	Quit();
	Destroy();
}

void CAsyncSQL::Destroy()
{
	ClearPrepared();

	if (m_mysqlInitialized)
	{
		LOG_INFO("AsyncSQL: closing mysql connection.");
		mysql_close(&m_hDB);
		m_hDB.host = nullptr;
		m_mysqlInitialized = false;
	}
	for (auto* queue : { &m_queue_query, &m_queue_query_copy, &m_queue_result })
		while (!queue->empty()) { delete queue->front(); queue->pop(); }

	if (m_mtxQuery)
	{
#ifndef _WIN32
		pthread_mutex_destroy(m_mtxQuery);
#else
		::DeleteCriticalSection(m_mtxQuery);
#endif
		delete m_mtxQuery;
		m_mtxQuery = nullptr;
	}

	if (m_mtxResult)
	{
#ifndef _WIN32
		pthread_mutex_destroy(m_mtxResult);
#else
		::DeleteCriticalSection(m_mtxResult);
#endif
		delete m_mtxResult;
		m_mtxResult = nullptr;
	}
}

#ifndef _WIN32
void * AsyncSQLThread(void * arg)
#else
unsigned int __stdcall AsyncSQLThread(void* arg)
#endif
{
	CAsyncSQL * pSQL = ((CAsyncSQL *) arg);

	if (!pSQL->Connect())
		return NULL;

	pSQL->ChildLoop();
	return NULL;
}

bool CAsyncSQL::QueryLocaleSet()
{
	if (0 == m_stLocale.length())
	{
		LOG_ERROR("m_stLocale == 0");
		return true;
	}

	else if (m_stLocale == "ascii")
	{
		LOG_ERROR("m_stLocale == ascii");
		return true;
	}

	if (mysql_set_character_set(&m_hDB, m_stLocale.c_str()))
	{
		LOG_ERROR("cannot set locale {} by 'mysql_set_character_set', errno {} {}", m_stLocale.c_str(), mysql_errno(&m_hDB), mysql_error(&m_hDB));
		return false; 
	}

	LOG_INFO("\t--mysql_set_character_set({})", m_stLocale.c_str());

	// Statement handles were prepared under the previous charset.
	ClearPrepared();

	return true;
}

bool CAsyncSQL::Connect()
{
	SetConnectionState(ConnectionState::Connecting);
	if (nullptr == mysql_init(&m_hDB))
	{
		std::print(stderr, "mysql_init failed\n");
		SetConnectionState(ConnectionState::Failed);
		return false;
	}
	m_mysqlInitialized = true;
	if (m_timeoutSeconds)
	{
		mysql_options(&m_hDB, MYSQL_OPT_CONNECT_TIMEOUT, &m_timeoutSeconds);
		mysql_options(&m_hDB, MYSQL_OPT_READ_TIMEOUT, &m_timeoutSeconds);
		mysql_options(&m_hDB, MYSQL_OPT_WRITE_TIMEOUT, &m_timeoutSeconds);
	}

	//mysql_options(&m_hDB, MYSQL_SET_CHARSET_NAME, m_stLocale.c_str());

	if (!m_stLocale.empty())
	{
		if (mysql_options(&m_hDB, MYSQL_SET_CHARSET_NAME, m_stLocale.c_str()) != 0)
		{
			std::print(stderr, "mysql_option failed : MYSQL_SET_CHARSET_NAME %s ", mysql_error(&m_hDB));
		}
	}

	if (!mysql_real_connect(&m_hDB, m_stHost.c_str(), m_stUser.c_str(), m_stPassword.c_str(), m_stDB.c_str(), m_iPort, nullptr, CLIENT_MULTI_STATEMENTS))
	{
		std::print(stderr, "mysql_real_connect: %s\n", mysql_error(&m_hDB));
		SetConnectionState(ConnectionState::Failed);
		return false;
	}

	my_bool reconnect = true;

	if (0 != mysql_options(&m_hDB, MYSQL_OPT_RECONNECT, &reconnect))
		std::print(stderr, "mysql_option: %s\n", mysql_error(&m_hDB));

	std::print(stdout, "AsyncSQL: connected to %s (reconnect %s)\n", m_stHost, &reconnect);


	m_ulThreadID = mysql_thread_id(&m_hDB);
	SetConnectionState(ConnectionState::Ready);

	return true;
}

void CAsyncSQL::SetConnectionState(ConnectionState state)
{
	{
		std::lock_guard<std::mutex> lock(m_connectionMutex);
		m_connectionState = state;
	}
	m_connectionChanged.notify_all();
}

bool CAsyncSQL::WaitForConnection(std::chrono::milliseconds timeout)
{
	std::unique_lock<std::mutex> lock(m_connectionMutex);
	m_connectionChanged.wait_for(lock, timeout, [this] {
		return m_connectionState != ConnectionState::Connecting;
	});
	return m_connectionState == ConnectionState::Ready;
}

bool CAsyncSQL::Setup(const char * c_pszHost, const char * c_pszUser, const char * c_pszPassword, const char * c_pszDB, const char * c_pszLocale, bool bNoThread, int iPort)
{
	SetConnectionState(ConnectionState::Connecting);
	m_stHost = c_pszHost;
	m_stUser = c_pszUser;
	m_stPassword = c_pszPassword;
	m_stDB = c_pszDB;
	m_iPort = iPort;

	if (c_pszLocale)
	{
		m_stLocale = c_pszLocale;
		LOG_INFO("AsyncSQL: locale {}", m_stLocale.c_str());
	}

	if (!bNoThread)
	{
		/*
		if (!mysql_thread_safe())//
	    {
			LOG_ERROR("FATAL ERROR!! mysql client library was not compiled with thread safety");
			return false;
		}
		*/
#ifndef _WIN32
		m_mtxQuery = new pthread_mutex_t;
		m_mtxResult = new pthread_mutex_t;

		if (0 != pthread_mutex_init(m_mtxQuery, NULL))
		{
			LOG_ERROR("{}: {}", "pthread_mutex_init", strerror(errno));
			exit(0);
		}

		if (0 != pthread_mutex_init(m_mtxResult, NULL))
		{
			LOG_ERROR("{}: {}", "pthread_mutex_init", strerror(errno));
			exit(0);
		}

		if (pthread_create(&m_hThread, NULL, AsyncSQLThread, this) != 0)
		{
			m_hThread = 0;
			SetConnectionState(ConnectionState::Failed);
			return false;
		}
#else
		m_mtxQuery = new CRITICAL_SECTION;
		m_mtxResult = new CRITICAL_SECTION;

		::InitializeCriticalSection(m_mtxQuery);
		::InitializeCriticalSection(m_mtxResult);

		m_hThread = (HANDLE)::_beginthreadex(nullptr, 0, AsyncSQLThread, this, 0, nullptr);
		if (!m_hThread || m_hThread == INVALID_HANDLE_VALUE) {
			m_hThread = INVALID_HANDLE_VALUE;
			SetConnectionState(ConnectionState::Failed);
			LOG_ERROR("{}: {}", "CAsyncSQL::Setup", strerror(errno));
			return false;
		}
#endif

		return true;
	}
	else
		return Connect();
}

void CAsyncSQL::Quit()
{
	m_bEnd = true;
	m_sem.Release();

#ifndef _WIN32
	if (m_hThread)
	{
		pthread_join(m_hThread, NULL);
		m_hThread = NULL;
	}
#else
	if (m_hThread != INVALID_HANDLE_VALUE) {
		::WaitForSingleObject(m_hThread, INFINITE);
		::CloseHandle(m_hThread);
		m_hThread = INVALID_HANDLE_VALUE;
	}
#endif
}

SQLMsg * CAsyncSQL::DirectQuery(const char * c_pszQuery)
{
	SQLMsg * p = new SQLMsg;

	p->m_pkSQL = &m_hDB;
	p->iID = ++m_iMsgCount;
	p->stQuery = c_pszQuery;
	if (!EnsureConnectionReady())
	{
		p->uiSQLErrno = mysql_errno(&m_hDB);
		p->StorePrepared(0, 0);
		return p;
	}

	if (mysql_real_query(&m_hDB, p->stQuery.c_str(), p->stQuery.length()))
	{
		char buf[1024];

		snprintf(buf, sizeof(buf),
				"AsyncSQL::DirectQuery : mysql_query error: %s\nquery: %s",
				mysql_error(&m_hDB), p->stQuery.c_str());

		LOG_ERROR("{}", buf);
		p->uiSQLErrno = mysql_errno(&m_hDB);
	}

	p->Store();
	return p;
}

SQLMsg * CAsyncSQL::DirectQueryPrepared(const char * c_pszQuery, const SqlParams & params)
{
	SQLMsg * p = new SQLMsg;

	p->m_pkSQL = &m_hDB;
	p->iID = ++m_iMsgCount;
	p->stQuery = c_pszQuery;
	p->vecParams = params;
	p->bPrepared = true;

	if (!TryExecutePrepared(p))
	{
		// Keep Get() usable for the callers even on failure.
		p->StorePrepared(0, 0);
	}

	return p;
}

void CAsyncSQL::AsyncQueryPrepared(const char * c_pszQuery, const SqlParams & params)
{
	SQLMsg * p = new SQLMsg;

	p->m_pkSQL = &m_hDB;
	p->iID = ++m_iMsgCount;
	p->stQuery = c_pszQuery;
	p->vecParams = params;
	p->bPrepared = true;

	PushQuery(p);
}

void CAsyncSQL::ReturnQueryPrepared(const char * c_pszQuery, const SqlParams & params, void * pvUserData)
{
	SQLMsg * p = new SQLMsg;

	p->m_pkSQL = &m_hDB;
	p->iID = ++m_iMsgCount;
	p->stQuery = c_pszQuery;
	p->vecParams = params;
	p->bPrepared = true;
	p->bReturn = true;
	p->pvUserData = pvUserData;

	PushQuery(p);
}

void CAsyncSQL::AsyncQuery(const char * c_pszQuery)
{
	SQLMsg * p = new SQLMsg;

	p->m_pkSQL = &m_hDB;
	p->iID = ++m_iMsgCount;
	p->stQuery = c_pszQuery;

	PushQuery(p);
}

void CAsyncSQL::ReturnQuery(const char * c_pszQuery, void * pvUserData)
{
	SQLMsg * p = new SQLMsg;

	p->m_pkSQL = &m_hDB;
	p->iID = ++m_iMsgCount;
	p->stQuery = c_pszQuery;
	p->bReturn = true;
	p->pvUserData = pvUserData;

	PushQuery(p);
}

bool CAsyncSQL::EnsureConnectionReady()
{
	if (m_ulThreadID != mysql_thread_id(&m_hDB))
	{
		LOG_INFO("MySQL connection was reconnected. querying locale set"); // @warme012
		if (!QueryLocaleSet())
			return false;
		m_ulThreadID = mysql_thread_id(&m_hDB);

		// Handles prepared on the previous connection are unusable.
		ClearPrepared();
	}
	return true;
}

void CAsyncSQL::ClearPrepared()
{
	for (auto & entry : m_map_prepared)
	{
		if (entry.second)
			mysql_stmt_close(entry.second);
	}

	m_map_prepared.clear();
}

void CAsyncSQL::DropPrepared(const std::string & sql)
{
	const auto it = m_map_prepared.find(sql);

	if (it == m_map_prepared.end())
		return;

	if (it->second)
		mysql_stmt_close(it->second);

	m_map_prepared.erase(it);
}

MYSQL_STMT * CAsyncSQL::GetOrPrepare(const std::string & sql)
{
	const auto it = m_map_prepared.find(sql);

	if (it != m_map_prepared.end())
		return it->second;

	MYSQL_STMT * stmt = mysql_stmt_init(&m_hDB);

	if (!stmt)
	{
		LOG_ERROR("AsyncSQL: mysql_stmt_init failed: {}", mysql_error(&m_hDB));
		return nullptr;
	}

	if (mysql_stmt_prepare(stmt, sql.c_str(), static_cast<unsigned long>(sql.length())))
	{
		LOG_ERROR("AsyncSQL: mysql_stmt_prepare failed: {} (query: {})", mysql_stmt_error(stmt), sql.c_str());
		mysql_stmt_close(stmt);
		return nullptr;
	}

	m_map_prepared.emplace(sql, stmt);
	return stmt;
}

bool CAsyncSQL::TryExecutePrepared(SQLMsg * p)
{
	if (!EnsureConnectionReady())
	{
		p->uiSQLErrno = mysql_errno(&m_hDB);
		return false;
	}

	MYSQL_STMT * stmt = GetOrPrepare(p->stQuery);

	if (!stmt)
	{
		p->uiSQLErrno = mysql_errno(&m_hDB);
		return false;
	}

	const size_t paramCount = static_cast<size_t>(mysql_stmt_param_count(stmt));

	if (paramCount != p->vecParams.size())
	{
		LOG_ERROR("AsyncSQL: prepared statement parameter mismatch (expected {}, got {}, query: {})",
			paramCount, p->vecParams.size(), p->stQuery.c_str());
		p->uiSQLErrno = 0;
		return false;
	}

	std::vector<MYSQL_BIND> binds(paramCount);
	std::vector<my_bool> isNull(paramCount, 0);
	std::vector<unsigned long> lengths(paramCount, 0);

	if (paramCount)
		memset(binds.data(), 0, sizeof(MYSQL_BIND) * paramCount);

	for (size_t i = 0; i < paramCount; ++i)
	{
		const SqlParam & param = p->vecParams[i];
		MYSQL_BIND & bind = binds[i];

		bind.is_null = &isNull[i];

		switch (param.type)
		{
			case SqlParamType::Null:
				bind.buffer_type = MYSQL_TYPE_NULL;
				isNull[i] = 1;
				break;

			case SqlParamType::Signed:
				bind.buffer_type = MYSQL_TYPE_LONGLONG;
				bind.buffer = const_cast<int64_t *>(&param.i64);
				bind.buffer_length = sizeof(param.i64);
				bind.is_unsigned = 0;
				break;

			case SqlParamType::Unsigned:
				bind.buffer_type = MYSQL_TYPE_LONGLONG;
				bind.buffer = const_cast<uint64_t *>(&param.u64);
				bind.buffer_length = sizeof(param.u64);
				bind.is_unsigned = 1;
				break;

			case SqlParamType::Real:
				bind.buffer_type = MYSQL_TYPE_DOUBLE;
				bind.buffer = const_cast<double *>(&param.f64);
				bind.buffer_length = sizeof(param.f64);
				break;

			case SqlParamType::Text:
			case SqlParamType::Blob:
				bind.buffer_type = (param.type == SqlParamType::Text) ? MYSQL_TYPE_STRING : MYSQL_TYPE_BLOB;
				bind.buffer = const_cast<char *>(param.bytes.data());
				bind.buffer_length = static_cast<unsigned long>(param.bytes.size());
				lengths[i] = static_cast<unsigned long>(param.bytes.size());
				bind.length = &lengths[i];
				break;
		}
	}

	if (paramCount && mysql_stmt_bind_param(stmt, binds.data()))
	{
		p->uiSQLErrno = mysql_stmt_errno(stmt);
		LOG_ERROR("AsyncSQL: mysql_stmt_bind_param failed: {} (query: {})", mysql_stmt_error(stmt), p->stQuery.c_str());
		return false;
	}

	if (mysql_stmt_execute(stmt))
	{
		p->uiSQLErrno = mysql_stmt_errno(stmt);
		LOG_ERROR("AsyncSQL: prepared statement failed: {} (query: {} errno: {})", mysql_stmt_error(stmt), p->stQuery.c_str(), p->uiSQLErrno);

		if (IsConnectionError(p->uiSQLErrno))
			DropPrepared(p->stQuery);

		return false;
	}

	MYSQL_RES * meta = mysql_stmt_result_metadata(stmt);

	if (meta)
	{
		const bool stored = StorePreparedResult(p, stmt, meta);
		mysql_free_result(meta);
		return stored;
	}

	p->StorePrepared(static_cast<uint64_t>(mysql_stmt_affected_rows(stmt)),
		static_cast<uint64_t>(mysql_stmt_insert_id(stmt)));
	p->uiSQLErrno = 0;
	return true;
}

bool CAsyncSQL::StorePreparedResult(SQLMsg * p, MYSQL_STMT * stmt, MYSQL_RES * meta)
{
	if (mysql_stmt_store_result(stmt))
	{
		p->uiSQLErrno = mysql_stmt_errno(stmt);
		LOG_ERROR("AsyncSQL: mysql_stmt_store_result failed: {} (query: {})", mysql_stmt_error(stmt), p->stQuery.c_str());
		return false;
	}

	const size_t columnCount = mysql_num_fields(meta);

	std::vector<MYSQL_BIND> binds(columnCount);
	std::vector<my_bool> isNull(columnCount, 0);
	std::vector<unsigned long> lengths(columnCount, 0);
	std::vector<std::vector<char>> buffers(columnCount);

	if (columnCount)
		memset(binds.data(), 0, sizeof(MYSQL_BIND) * columnCount);

	// Fetch every column as string, matching the text protocol's
	// mysql_fetch_row() representation. MYSQL_FIELD::max_length is only
	// filled in when STMT_ATTR_UPDATE_MAX_LENGTH is set, so it cannot size
	// the buffers: they start at a fixed size and a longer value is read
	// again whole below.
	for (size_t i = 0; i < columnCount; ++i)
	{
		buffers[i].resize(PREPARED_CELL_CAPACITY);

		binds[i].buffer_type = MYSQL_TYPE_STRING;
		binds[i].buffer = buffers[i].data();
		binds[i].buffer_length = static_cast<unsigned long>(PREPARED_CELL_CAPACITY);
		binds[i].length = &lengths[i];
		binds[i].is_null = &isNull[i];
	}

	if (columnCount && mysql_stmt_bind_result(stmt, binds.data()))
	{
		p->uiSQLErrno = mysql_stmt_errno(stmt);
		LOG_ERROR("AsyncSQL: mysql_stmt_bind_result failed: {} (query: {})", mysql_stmt_error(stmt), p->stQuery.c_str());
		mysql_stmt_free_result(stmt);
		return false;
	}

	SQLResult * pRes = new SQLResult;
	pRes->bPrepared = true;

	int rc;

	while ((rc = mysql_stmt_fetch(stmt)) == 0 || rc == MYSQL_DATA_TRUNCATED)
	{
		std::vector<char *> row(columnCount, nullptr);

		for (size_t i = 0; i < columnCount; ++i)
		{
			if (isNull[i])
				continue;

			if (lengths[i] <= buffers[i].size())
				pRes->preparedCells.emplace_back(buffers[i].data(), lengths[i]);
			else if (!FetchWholeColumn(stmt, static_cast<unsigned int>(i), lengths[i], pRes->preparedCells.emplace_back()))
			{
				p->uiSQLErrno = mysql_stmt_errno(stmt);
				LOG_ERROR("AsyncSQL: mysql_stmt_fetch_column failed: {} (query: {})", mysql_stmt_error(stmt), p->stQuery.c_str());
				delete pRes;
				mysql_stmt_free_result(stmt);
				return false;
			}

			row[i] = pRes->preparedCells.back().data();
		}

		pRes->preparedRows.push_back(std::move(row));
	}

	if (rc != MYSQL_NO_DATA)
	{
		p->uiSQLErrno = mysql_stmt_errno(stmt);
		LOG_ERROR("AsyncSQL: mysql_stmt_fetch failed: {} (query: {})", mysql_stmt_error(stmt), p->stQuery.c_str());
		delete pRes;
		mysql_stmt_free_result(stmt);
		return false;
	}

	pRes->uiNumRows = pRes->preparedRows.size();

	p->vec_pkResult.push_back(pRes);
	p->uiSQLErrno = 0;

	mysql_stmt_free_result(stmt);

	return true;
}

void CAsyncSQL::PushResult(SQLMsg * p)
{
	MUTEX_LOCK(m_mtxResult);

	m_queue_result.push(p);

	MUTEX_UNLOCK(m_mtxResult);
}

bool CAsyncSQL::PopResult(SQLMsg ** pp)
{
	if (!m_mtxResult)
		return false;
	MUTEX_LOCK(m_mtxResult);

	if (m_queue_result.empty())
	{
		MUTEX_UNLOCK(m_mtxResult);
		return false;
	}

	*pp = m_queue_result.front();
	m_queue_result.pop();
	MUTEX_UNLOCK(m_mtxResult);
	return true;
}

void CAsyncSQL::PushQuery(SQLMsg * p)
{
	MUTEX_LOCK(m_mtxQuery);

	m_queue_query.push(p);
	//m_map_kSQLMsgUnfinished.insert(std::make_pair(p->iID, p));

	m_sem.Release();

	MUTEX_UNLOCK(m_mtxQuery);
}

bool CAsyncSQL::PeekQuery(SQLMsg ** pp)
{
	MUTEX_LOCK(m_mtxQuery);

	if (m_queue_query.empty())
	{
		MUTEX_UNLOCK(m_mtxQuery);
		return false;
	}

	*pp = m_queue_query.front();
	MUTEX_UNLOCK(m_mtxQuery);
	return true;
}

bool CAsyncSQL::PopQuery(int iID)
{
	MUTEX_LOCK(m_mtxQuery);

	if (m_queue_query.empty())
	{
		MUTEX_UNLOCK(m_mtxQuery);
		return false;
	}

	m_queue_query.pop();
	//m_map_kSQLMsgUnfinished.erase(iID);

	MUTEX_UNLOCK(m_mtxQuery);
	return true;
}

bool CAsyncSQL::PeekQueryFromCopyQueue(SQLMsg ** pp)
{
	if (m_queue_query_copy.empty())
		return false;

	*pp = m_queue_query_copy.front();
	return true;
}

int CAsyncSQL::CopyQuery()
{
	MUTEX_LOCK(m_mtxQuery);

	if (m_queue_query.empty())
	{
		MUTEX_UNLOCK(m_mtxQuery);
		return -1;
	}

	while (!m_queue_query.empty())
	{
		SQLMsg * p = m_queue_query.front();
		m_queue_query_copy.push(p);
		m_queue_query.pop();
	}

	//m_map_kSQLMsgUnfinished.erase(iID);

	int count = static_cast<int>(m_queue_query_copy.size());	

	MUTEX_UNLOCK(m_mtxQuery);
	return count;
}

bool CAsyncSQL::PopQueryFromCopyQueue()
{
	if (m_queue_query_copy.empty())
	{
		return false;
	}

	m_queue_query_copy.pop();
	//m_map_kSQLMsgUnfinished.erase(iID);

	return true;
}
int		CAsyncSQL::GetCopiedQueryCount()
{
	return m_iCopiedQuery;
}
void 	CAsyncSQL::ResetCopiedQueryCount()
{
	m_iCopiedQuery = 0;
}

void	CAsyncSQL::AddCopiedQueryCount(int iCopiedQuery)
{
	m_iCopiedQuery += iCopiedQuery;
}



uint32_t CAsyncSQL::CountQuery()
{
	return m_queue_query.size();
}

uint32_t CAsyncSQL::CountResult()
{
	return m_queue_result.size();
}

void __timediff(struct timeval *a, struct timeval *b, struct timeval *rslt)
{
	if (a->tv_sec < b->tv_sec)
		rslt->tv_sec = rslt->tv_usec = 0;
	else if (a->tv_sec == b->tv_sec)
	{
		if (a->tv_usec < b->tv_usec)
			rslt->tv_sec = rslt->tv_usec = 0;
		else
		{
			rslt->tv_sec = 0;
			rslt->tv_usec = a->tv_usec - b->tv_usec;
		}
	}
	else
	{                      /* a->tv_sec > b->tv_sec */
		rslt->tv_sec = a->tv_sec - b->tv_sec;

		if (a->tv_usec < b->tv_usec)
		{
			rslt->tv_usec = a->tv_usec + 1000000 - b->tv_usec;
			rslt->tv_sec--;
		} else
			rslt->tv_usec = a->tv_usec - b->tv_usec;
	}
}

class cProfiler
{
public:
	cProfiler()
	{
		m_nInterval = 0;

		memset(&prev, 0, sizeof(prev));
		memset(&now, 0, sizeof(now));
		memset(&interval, 0, sizeof(interval));

		Start();
	}

	cProfiler(int nInterval = 100000)
	{
		m_nInterval = nInterval;

		memset(&prev, 0, sizeof(prev));
		memset(&now, 0, sizeof(now));
		memset(&interval, 0, sizeof(interval));

		Start();
	}

	void Start()
	{
		gettimeofday(&prev, (struct timezone*)0);
	}

	void Stop()
	{
		gettimeofday(&now, (struct timezone*)0);
		__timediff(&now, &prev, &interval);
	}

	bool IsOk()
	{
		if (interval.tv_sec > (m_nInterval / 1000000))
			return false;

		if (interval.tv_usec > m_nInterval)
			return false;

		return true;
	}

	struct timeval* GetResult() { return &interval; }
	int32_t GetResultSec() { return interval.tv_sec; }
	int32_t GetResultUSec() { return interval.tv_usec; }

private:
	int m_nInterval;
	struct timeval  prev;
	struct timeval  now;
	struct timeval	interval;
};

void CAsyncSQL::ChildLoop()
{
	cProfiler profiler(500000);

	while (!m_bEnd)
	{
		m_sem.Wait();

		int count = CopyQuery();

		if (count <= 0)
			continue;

		AddCopiedQueryCount(count);

		SQLMsg * p;

		while (count-- && (!m_bEnd || m_flushOnQuit))
		{
			profiler.Start();

			if (!PeekQueryFromCopyQueue(&p))
				continue;

			bool executed;

			if (p->bPrepared)
			{
				executed = TryExecutePrepared(p);
			}
			else
			{
				executed = (mysql_real_query(&m_hDB, p->stQuery.c_str(), p->stQuery.length()) == 0);

				if (!executed)
				{
					p->uiSQLErrno = mysql_errno(&m_hDB);

					LOG_ERROR("AsyncSQL: query failed: {} (query: {} errno: {})", mysql_error(&m_hDB), p->stQuery.c_str(), p->uiSQLErrno);
				}
			}

			if (!executed && m_retryConnectionErrors && IsConnectionError(p->uiSQLErrno))
			{
				m_sem.Release();
				LOG_ERROR("AsyncSQL: retrying");
				continue;
			}

			profiler.Stop();
			
			if (!profiler.IsOk())
				LOG_INFO("[QUERY : LONG INTERVAL(OverSec {}.{})] : {}", profiler.GetResultSec(), profiler.GetResultUSec(), p->stQuery.c_str());

			PopQueryFromCopyQueue();

			if (p->bReturn)
			{
				if (p->bPrepared)
				{
					if (!executed)
						p->StorePrepared(0, 0);
				}
				else
					p->Store();

				PushResult(p);
			}
			else
				delete p;

			++m_iQueryFinished;
		}
	}

	if (!m_flushOnQuit)
		return;
	SQLMsg * p;

	while (PeekQuery(&p))
	{
		bool executed;

		if (p->bPrepared)
		{
			executed = TryExecutePrepared(p);
		}
		else
		{
			executed = (mysql_real_query(&m_hDB, p->stQuery.c_str(), p->stQuery.length()) == 0);

			if (!executed)
			{
				p->uiSQLErrno = mysql_errno(&m_hDB);

				LOG_ERROR("AsyncSQL::ChildLoop : mysql_query error: {}:\nquery: {}", mysql_error(&m_hDB), p->stQuery.c_str());
			}
		}

		if (!executed && m_retryConnectionErrors && IsConnectionError(p->uiSQLErrno))
			continue;

		LOG_INFO("QUERY_FLUSH: {}", p->stQuery.c_str());

		PopQuery(p->iID);

		if (p->bReturn)
		{
			if (p->bPrepared)
			{
				if (!executed)
					p->StorePrepared(0, 0);
			}
			else
				p->Store();

			PushResult(p);
		}
		else
			delete p;

		++m_iQueryFinished;
	}
}

int CAsyncSQL::CountQueryFinished()
{
	return m_iQueryFinished;
}

void CAsyncSQL::ResetQueryFinished()
{
	m_iQueryFinished = 0;
}

MYSQL * CAsyncSQL::GetSQLHandle()
{
	return &m_hDB;
}

uint32_t CAsyncSQL::EscapeString(char* dst, uint64_t dstSize, const char *src, uint32_t srcSize)
{
	if (0 == srcSize)
	{
		memset(dst, 0, dstSize);
		return 0;
	}

	if (0 == dstSize)
		return 0;

	if (dstSize < srcSize * 2 + 1)
	{
		char tmp[256];
		uint32_t tmpLen = sizeof(tmp) > srcSize ? srcSize : sizeof(tmp);
		strlcpy(tmp, src, tmpLen);

		LOG_ERROR("FATAL ERROR!! not enough buffer size (dstSize {} srcSize {} src{}: {})", dstSize, srcSize, tmpLen != srcSize ? "(trimmed to 255 characters)" : "", tmp);

		dst[0] = '\0';
		return 0;
	}

	return mysql_real_escape_string(GetSQLHandle(), dst, src, srcSize);
}

void CAsyncSQL2::SetLocale(const std::string & stLocale)
{
	m_stLocale = stLocale;	
	QueryLocaleSet();
}

