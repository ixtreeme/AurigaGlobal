#include "stdafx.h"
#include <Core/Logging.hpp>
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/MountSystem.hpp"
#include "../ecs/AIHelpers.hpp"
#include <sstream>
#include <vector>
#include <common/length.h>

#include "db.h"

#include "config.h"
#include "desc_client.h"
#include "desc_manager.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "../ecs/CharacterAccessors.hpp"
#include "../ecs/EntityFactory.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/systems/ItemSystem.hpp"
#include "../ecs/systems/SessionSystem.hpp"
#include "item.h"
#include "item_manager.h"
#include "p2p.h"
#include "log.h"
#include "login_data.h"
#include "locale_service.h"
#include "spam.h"
#include "shutdown_manager.h"
#ifdef ENABLE_HWID
#include "hwidmanager.h"
#include "Core/Logging.hpp"
#endif

DBManager::DBManager() : m_bIsConnect(false)
{
}

DBManager::~DBManager()
{
}

bool DBManager::Connect(const char * host, const int port, const char * user, const char * pwd, const char * db)
{
	if (m_sql.Setup(host, user, pwd, db, g_stLocale.c_str(), false, port))
		m_bIsConnect = true;

	if (!m_sql_direct.Setup(host, user, pwd, db, g_stLocale.c_str(), true, port))
		LOG_ERROR("cannot open direct sql connection to host {}", host);

	if (m_bIsConnect && true)
	{
		LoadDBString();
	}

	return m_bIsConnect;
}

void DBManager::Query(const char * c_pszFormat, ...)
{
	char szQuery[4096];
	va_list args;

	va_start(args, c_pszFormat);
	vsnprintf(szQuery, sizeof(szQuery), c_pszFormat, args);
	va_end(args);

	m_sql.AsyncQuery(szQuery);
}

SQLMsg * DBManager::DirectQuery(const char * c_pszFormat, ...)
{
	char szQuery[4096];
	va_list args;

	va_start(args, c_pszFormat);
	vsnprintf(szQuery, sizeof(szQuery), c_pszFormat, args);
	va_end(args);

#ifdef ENABLE_BUG_FIXES
	std::string sQuery(szQuery);
	return m_sql_direct.DirectQuery(sQuery.substr(0, sQuery.find_first_of(";") == std::string::npos ? sQuery.length(): sQuery.find_first_of(";")).c_str());
#else
	return m_sql_direct.DirectQuery(szQuery);
#endif
}

bool DBManager::IsConnected()
{
	return m_bIsConnect;
}

void DBManager::ReturnQuery(int iType, uint32_t dwIdent, void * pvData, const char * c_pszFormat, ...)
{
	// LOG_INFO("ReturnQuery {}", c_pszQuery);
	char szQuery[4096];
	va_list args;

	va_start(args, c_pszFormat);
	vsnprintf(szQuery, sizeof(szQuery), c_pszFormat, args);
	va_end(args);

	CReturnQueryInfo * p = M2_NEW CReturnQueryInfo;

	p->iQueryType = QUERY_TYPE_RETURN;
	p->iType = iType;
	p->dwIdent = dwIdent;
	p->pvData = pvData;

	m_sql.ReturnQuery(szQuery, p);
}

void DBManager::QueryPrepared(const char * c_pszSql, const SqlParams & params)
{
	m_sql.AsyncQueryPrepared(c_pszSql, params);
}

SQLMsg * DBManager::DirectQueryPrepared(const char * c_pszSql, const SqlParams & params)
{
	return m_sql_direct.DirectQueryPrepared(c_pszSql, params);
}

void DBManager::ReturnQueryPrepared(int iType, uint32_t dwIdent, void * pvData, const char * c_pszSql, const SqlParams & params)
{
	CReturnQueryInfo * p = M2_NEW CReturnQueryInfo;

	p->iQueryType = QUERY_TYPE_RETURN;
	p->iType = iType;
	p->dwIdent = dwIdent;
	p->pvData = pvData;

	m_sql.ReturnQueryPrepared(c_pszSql, params, p);
}

SQLMsg * DBManager::PopResult()
{
	SQLMsg * p;

	if (m_sql.PopResult(&p))
		return p;

	return nullptr;
}

void DBManager::Process()
{
	SQLMsg* pMsg = nullptr;

	while ((pMsg = PopResult()))
	{
		if(nullptr != pMsg->pvUserData )
		{
			switch( reinterpret_cast<CQueryInfo*>(pMsg->pvUserData)->iQueryType )
			{
				case QUERY_TYPE_RETURN:
					AnalyzeReturnQuery(pMsg);
					break;

				case QUERY_TYPE_FUNCTION:
					{
						CFuncQueryInfo* qi = reinterpret_cast<CFuncQueryInfo*>( pMsg->pvUserData );
						qi->f(pMsg);
						M2_DELETE(qi);
					}
					break;

				case QUERY_TYPE_AFTER_FUNCTION:
					{
						CFuncAfterQueryInfo* qi = reinterpret_cast<CFuncAfterQueryInfo*>( pMsg->pvUserData );
						qi->f();
						M2_DELETE(qi);
					}
					break;
			}
		}

		delete pMsg;
	}
}

CLoginData * DBManager::GetLoginData(uint32_t dwKey)
{
	std::map<uint32_t, CLoginData *>::iterator it = m_map_pkLoginData.find(dwKey);

	if (it == m_map_pkLoginData.end())
		return nullptr;

	return it->second;
}

void DBManager::InsertLoginData(CLoginData * pkLD)
{
	m_map_pkLoginData.insert(std::make_pair(pkLD->GetKey(), pkLD));
}

void DBManager::DeleteLoginData(CLoginData * pkLD)
{
	std::map<uint32_t, CLoginData *>::iterator it = m_map_pkLoginData.find(pkLD->GetKey());

	if (it == m_map_pkLoginData.end())
		return;

	LOG_INFO("DeleteLoginData {} {}", pkLD->GetLogin(), static_cast<const void*>(pkLD));


	M2_DELETE(it->second);
	m_map_pkLoginData.erase(it);
}

void DBManager::AnalyzeReturnQuery(SQLMsg * pMsg)
{
	CReturnQueryInfo * qi = (CReturnQueryInfo *) pMsg->pvUserData;

	switch (qi->iType)
	{
		case QID_MOUNT_INVENTORY_LOAD:
		{
			auto* request = static_cast<MountInventoryLoadRequest*>(qi->pvData);
			const entt::entity ch = request ? request->character : entt::null;
			const entt::entity current = CHARACTER_MANAGER::instance().FindEntityByPID(qi->dwIdent);
			if (request && ch == current && ecs::IsCharacter(ch) &&
				ecs::PlayerRuntime::GetAccountID(ch) == request->accountId)
			{
				SQLResult* res = pMsg->Get();
				const auto columnCount = mysql_num_fields(res->pSQLResult);
				std::vector<TMountInventoryItemTable> items;
				items.reserve(res->uiNumRows);

				for (uint64_t i = 0; i < res->uiNumRows; ++i)
				{
					MYSQL_ROW row = res->FetchRow();
					if (!row)
						continue;

					TMountInventoryItemTable entry{};
					str_to_number(entry.id, row[0]);
					str_to_number(entry.slot, row[1]);
					str_to_number(entry.vnum, row[2]);
					str_to_number(entry.count, row[3]);
					str_to_number(entry.alSockets[0], row[4]);
					str_to_number(entry.alSockets[1], row[5]);
					str_to_number(entry.alSockets[2], row[6]);

					for (int j = 0; j < ITEM_ATTRIBUTE_MAX_NUM; ++j)
					{
						int typeIndex = 7 + j * 2;
						int valueIndex = typeIndex + 1;
						if (valueIndex >= static_cast<int>(columnCount))
							break;
						str_to_number(entry.aAttr[j].bType, row[typeIndex]);
						str_to_number(entry.aAttr[j].sValue, row[valueIndex]);
					}

					items.push_back(entry);
				}

				MountSystem::LoadMountInventory(ch, request->accountId,
					request->requestId, items);
			}
			if (request)
				M2_DELETE(request);
		}
		break;


		case QID_SAFEBOX_SIZE:
			{
				const entt::entity ch = CHARACTER_MANAGER::instance().FindEntityByPID(qi->dwIdent);

				if (ecs::IsCharacter(ch))
				{
					if (pMsg->Get()->uiNumRows > 0)
					{
						MYSQL_ROW row = pMsg->Get()->FetchRow();
						int	size = 0;
						str_to_number(size, row[0]);
						ecs::SessionSystem::SetSafeboxSize(ch, SAFEBOX_PAGE_SIZE * size);
					}
				}
			}
			break;

		case QID_DB_STRING:
			{
				m_map_dbstring.clear();
				m_vec_GreetMessage.clear();

				for (uint64_t i = 0; i < pMsg->Get()->uiNumRows; ++i)
				{
					MYSQL_ROW row = pMsg->Get()->FetchRow();
					//ch->SetSafeboxSize(SAFEBOX_PAGE_SIZE * atoi(row[0]));
					if (row[0] && row[1])
					{
						m_map_dbstring.insert(make_pair(std::string(row[0]), std::string(row[1])));
						LOG_INFO("DBSTR '{}' '{}'", row[0], row[1]);
					}
				}
				if (m_map_dbstring.find("GREET") != m_map_dbstring.end())
				{
					std::istringstream is(m_map_dbstring["GREET"]);
					while (!is.eof())
					{
						std::string str;
						getline(is, str);
						m_vec_GreetMessage.push_back(str);
					}
				}
			}
			break;

		case QID_LOTTO:
			{
				const entt::entity ch = CHARACTER_MANAGER::instance().FindEntityByPID(qi->dwIdent);
				uint32_t * pdw = (uint32_t *) qi->pvData;

				if (ecs::IsCharacter(ch))
				{
					if (pMsg->Get()->uiAffectedRows == 0 || pMsg->Get()->uiAffectedRows == (uint32_t)-1)
					{
						LOG_INFO("GIVE LOTTO FAIL TO pid {}", (ecs::PlayerRuntime::GetPlayerID(ch)));
					}
					else
					{
						const entt::entity pkItem = static_cast<int>(pdw[1]) > 0
							? ItemSystem::AutoGiveItemEcs(ch, pdw[0], pdw[1])
							: entt::null;

						if (ItemSystem::IsValidItem(pkItem))
						{
							LOG_INFO("GIVE LOTTO SUCCESS TO {} (pid {})", ecs::PlayerRuntime::GetName(ch).data(), qi->dwIdent);
							ItemSystem::SetItemSocket(pkItem, 0, pMsg->Get()->uiInsertID);
							ItemSystem::SetItemSocket(pkItem, 1, pdw[2]);
						}
						else
							LOG_INFO("GIVE LOTTO FAIL2 TO pid {}", (ecs::PlayerRuntime::GetPlayerID(ch)));
					}
				}

				M2_DELETE_ARRAY(pdw);
			}
			break;

		case QID_HIGHSCORE_REGISTER:
			{
				THighscoreRegisterQueryInfo * info = (THighscoreRegisterQueryInfo *) qi->pvData;
				bool bQuery = true;

				if (pMsg->Get()->uiNumRows)
				{
					MYSQL_ROW row = pMsg->Get()->FetchRow();

					if (row && row[0])
					{
						int iCur = 0;
						str_to_number(iCur, row[0]);

						if ((info->bOrder && iCur >= info->iValue) ||
								(!info->bOrder && iCur <= info->iValue))
							bQuery = false;
					}
				}

				if (bQuery)
				{
					char query[256];
					snprintf(query, sizeof(query), "REPLACE INTO highscore%s VALUES(?, ?, ?)", get_table_postfix());
					QueryPrepared(query, { SqlParam::Text(info->szBoard), SqlParam::UInt(info->dwPID), SqlParam::Int(info->iValue) });
				}

				M2_DELETE(info);
			}
			break;

		case QID_HIGHSCORE_SHOW:
			{
			}
			break;

			// BLOCK_CHAT
		case QID_BLOCK_CHAT_LIST:
			{
				const entt::entity ch = CHARACTER_MANAGER::instance().FindEntityByPID(qi->dwIdent);

				if (ch == entt::null)
					break;
				if (pMsg->Get()->uiNumRows)
				{
					MYSQL_ROW row;
					while ((row = pMsg->Get()->FetchRow()))
					{
						ecs::ChatSystem::Send(ch, CHAT_TYPE_INFO, "%s %s sec", row[0], row[1]);
					}
				}
#ifdef TEXTS_IMPROVEMENT
				else {
					ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 820, "");
				}
#endif
			}
			break;
			// END_OF_BLOCK_CHAT

					default:
			LOG_ERROR("FATAL ERROR!!! Unhandled return query id {}", qi->iType);
			break;
	}

	M2_DELETE(qi);
}

void DBManager::LoadDBString()
{
	ReturnQuery(QID_DB_STRING, 0, nullptr, "SELECT name, text FROM string%s", get_table_postfix());
}

const std::string& DBManager::GetDBString(const std::string& key)
{
	static std::string null_str = "";
	auto it = m_map_dbstring.find(key);
	if (it == m_map_dbstring.end())
		return null_str;
	return it->second;
}

const std::vector<std::string>& DBManager::GetGreetMessage()
{
	return m_vec_GreetMessage;
}

void DBManager::SendMoneyLog(uint8_t type, uint32_t vnum, int64_t gold)
{
	if (!gold)
		return;
	TPacketMoneyLog p;
	p.type = type;
	p.vnum = vnum;
	p.gold = gold;
	db_clientdesc->DBPacket(HEADER_GD_MONEY_LOG, 0, &p, sizeof(p));
}

uint32_t DBManager::EscapeString(char* dst, uint64_t dstSize, const char *src, uint32_t srcSize)
{
	return m_sql_direct.EscapeString(dst, dstSize, src, srcSize);
}

//
// Common SQL
//
AccountDB::AccountDB() :
	m_IsConnect(false)
{
}

bool AccountDB::IsConnected()
{
	return m_IsConnect;
}

bool AccountDB::Connect(const char * host, const int port, const char * user, const char * pwd, const char * db)
{
	m_IsConnect = m_sql_direct.Setup(host, user, pwd, db, "", true, port);

	if (false == m_IsConnect)
	{
		LOG_ERROR("cannot open direct sql connection to host: {} user: {} db: {}", host, user, db);
		return false;
	}

	return m_IsConnect;
}

bool AccountDB::ConnectAsync(const char * host, const int port, const char * user, const char * pwd, const char * db, const char * locale)
{
	m_sql.Setup(host, user, pwd, db, locale, false, port);
	return true;
}

void AccountDB::SetLocale(const std::string & stLocale)
{
	m_sql_direct.SetLocale(stLocale);
	m_sql_direct.QueryLocaleSet();
}

SQLMsg* AccountDB::DirectQuery(const char * query)
{
	return m_sql_direct.DirectQuery(query);
}

void AccountDB::AsyncQuery(const char* query)
{
	m_sql.AsyncQuery(query);
}

SQLMsg* AccountDB::DirectQueryPrepared(const char * c_pszSql, const SqlParams & params)
{
	return m_sql_direct.DirectQueryPrepared(c_pszSql, params);
}

void AccountDB::AsyncQueryPrepared(const char * c_pszSql, const SqlParams & params)
{
	m_sql.AsyncQueryPrepared(c_pszSql, params);
}

void AccountDB::ReturnQueryPrepared(int iType, uint32_t dwIdent, void * pvData, const char * c_pszSql, const SqlParams & params)
{
	CReturnQueryInfo * p = M2_NEW CReturnQueryInfo;

	p->iQueryType = QUERY_TYPE_RETURN;
	p->iType = iType;
	p->dwIdent = dwIdent;
	p->pvData = pvData;

	m_sql.ReturnQueryPrepared(c_pszSql, params, p);
}

void AccountDB::ReturnQuery(int iType, uint32_t dwIdent, void * pvData, const char * c_pszFormat, ...)
{
	char szQuery[4096];
	va_list args;

	va_start(args, c_pszFormat);
	vsnprintf(szQuery, sizeof(szQuery), c_pszFormat, args);
	va_end(args);

	CReturnQueryInfo * p = M2_NEW CReturnQueryInfo;

	p->iQueryType = QUERY_TYPE_RETURN;
	p->iType = iType;
	p->dwIdent = dwIdent;
	p->pvData = pvData;

	m_sql.ReturnQuery(szQuery, p);
}

SQLMsg * AccountDB::PopResult()
{
	SQLMsg * p;

	if (m_sql.PopResult(&p))
		return p;

	return nullptr;
}

void AccountDB::Process()
{
	SQLMsg* pMsg = nullptr;

	while ((pMsg = PopResult()))
	{
		CQueryInfo* qi = (CQueryInfo *) pMsg->pvUserData;

		switch (qi->iQueryType)
		{
			case QUERY_TYPE_RETURN:
				AnalyzeReturnQuery(pMsg);
				break;
		}
	}

	delete pMsg;
}

extern unsigned int g_uiSpamReloadCycle;

enum EAccountQID
{
	QID_SPAM_DB,
};

static LPEVENT s_pkReloadSpamEvent = nullptr;

EVENTINFO(reload_spam_event_info)
{
	// used to send command
	uint32_t empty;
};

EVENTFUNC(reload_spam_event)
{
	AccountDB::instance().ReturnQuery(QID_SPAM_DB, 0, nullptr, "SELECT word, score FROM spam_db WHERE type='SPAM'");
	return PASSES_PER_SEC(g_uiSpamReloadCycle);
}

//#define ENABLE_SPAMDB_REFRESH
void LoadSpamDB()
{
	AccountDB::instance().ReturnQuery(QID_SPAM_DB, 0, nullptr, "SELECT word, score FROM spam_db WHERE type='SPAM'");
#ifdef ENABLE_SPAMDB_REFRESH
	if (NULL == s_pkReloadSpamEvent)
	{
		reload_spam_event_info* info = AllocEventInfo<reload_spam_event_info>();
		s_pkReloadSpamEvent = event_create(reload_spam_event, info, PASSES_PER_SEC(g_uiSpamReloadCycle));
	}
#endif
}

void CancelReloadSpamEvent() {
	s_pkReloadSpamEvent = nullptr;
}

void AccountDB::AnalyzeReturnQuery(SQLMsg * pMsg)
{
	CReturnQueryInfo * qi = (CReturnQueryInfo *) pMsg->pvUserData;

	switch (qi->iType)
	{
		case QID_SPAM_DB:
			{
				if (pMsg->Get()->uiNumRows > 0)
				{
					MYSQL_ROW row;

					SpamManager::instance().Clear();

					while ((row = pMsg->Get()->FetchRow()))
						SpamManager::instance().Insert(row[0], atoi(row[1]));
				}
			}
			break;
	}

	M2_DELETE(qi);
}


