// vim:ts=8 sw=4
#ifndef __INC_METIN2_DB_DBMANAGER_H__
#define __INC_METIN2_DB_DBMANAGER_H__

#include <mysql/mysql.h>

#include <SQL/AsyncSQL.h>

class CQueryInfo
{
    public:
	int	iType;
	uint32_t	dwIdent;
	void *	pvData;
};

enum eSQL_SLOT
{
	SQL_PLAYER,
	SQL_ACCOUNT,
	SQL_COMMON,
	//SQL_ITEMSHOP,
	SQL_MAX_NUM,
};

class CDBManager : public singleton<CDBManager>
{
    protected:
	void			Initialize();
	void			Destroy();

    public:
	CDBManager();
	virtual ~CDBManager();

	void			Clear();
	void			Quit();

	int			Connect(int iSlot, const char * host, int port, const char* dbname, const char* user, const char* pass);

	void			ReturnQuery(const char * c_pszQuery, int iType, uint32_t dwIdent, void * pvData, int iSlot = SQL_PLAYER);
	void			AsyncQuery(const char * c_pszQuery, int iSlot = SQL_PLAYER);
	SQLMsg *		DirectQuery(const char * c_pszQuery, int iSlot = SQL_PLAYER);

	// Prepared (parameterized) variants for write statements. The SQL text
	// must use '?' placeholders; the values travel out-of-band through the
	// binary protocol, so no escaping/quoting is needed.
	void			QueryPrepared(const char * c_pszSql, const SqlParams & params, int iSlot = SQL_PLAYER);
	void			ReturnQueryPrepared(const char * c_pszSql, const SqlParams & params, int iType, uint32_t dwIdent, void * pvData, int iSlot = SQL_PLAYER);
	SQLMsg *		DirectQueryPrepared(const char * c_pszSql, const SqlParams & params, int iSlot = SQL_PLAYER);
	// Owns the complete commit boundary; never separate BEGIN/COMMIT queue jobs.
	SQLMsg *		DirectTransactionPrepared(SqlTransaction transaction, int iSlot = SQL_PLAYER);
	//SQLMsg *		DirectQuery_ItemShop(const char * c_pszQuery, int iSlot = SQL_ITEMSHOP);

	SQLMsg *		PopResult();
	SQLMsg * 		PopResult(eSQL_SLOT slot );

	unsigned long		EscapeString(void * to, const void * from, unsigned long length, int iSlot = SQL_PLAYER);

	uint32_t			CountReturnQuery(int i) { return m_mainSQL[i] ? m_mainSQL[i]->CountQuery() : 0; }
	uint32_t			CountReturnResult(int i) { return m_mainSQL[i] ? m_mainSQL[i]->CountResult() : 0; }
	uint32_t			CountReturnQueryFinished(int i) { return m_mainSQL[i] ? m_mainSQL[i]->CountQueryFinished() : 0; }
	uint32_t			CountReturnCopiedQuery(int i) { return m_mainSQL[i] ? m_mainSQL[i]->GetCopiedQueryCount() : 0; }

	uint32_t			CountAsyncQuery(int i) { return m_asyncSQL[i] ? m_asyncSQL[i]->CountQuery() : 0; }
	uint32_t			CountAsyncResult(int i) { return m_asyncSQL[i] ? m_asyncSQL[i]->CountResult() : 0; }
	uint32_t			CountAsyncQueryFinished(int i) { return m_asyncSQL[i] ? m_asyncSQL[i]->CountQueryFinished() : 0; }
	uint32_t			CountAsyncCopiedQuery(int i) { return m_asyncSQL[i] ? m_asyncSQL[i]->GetCopiedQueryCount() : 0; }

	void			ResetCounter()
	{
	    for (int i = 0; i < SQL_MAX_NUM; ++i)
		{
			if (m_mainSQL[i])
			{
				m_mainSQL[i]->ResetQueryFinished();
				m_mainSQL[i]->ResetCopiedQueryCount();
			}

			if (m_asyncSQL[i])
			{
				m_asyncSQL[i]->ResetQueryFinished();
				m_asyncSQL[i]->ResetCopiedQueryCount();
			}
		}
	}

    private:
	CAsyncSQL2 *		m_mainSQL[SQL_MAX_NUM];
	CAsyncSQL2 *	 	m_directSQL[SQL_MAX_NUM];
	CAsyncSQL2 *		m_asyncSQL[SQL_MAX_NUM];

	int			m_quit;		// looping flag

	//CHARSET
	public:
	void SetLocale(const char * szLocale );
	void QueryLocaleSet();
	private:

	//END_CHARSET
};

#endif
