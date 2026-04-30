// vim:ts=4 sw=4
#include "stdafx.h"
#include "ClientManager.h"
#include "Main.h"
#include "Config.h"
#include "DBManager.h"
#include "QID.h"

void CClientManager::LoadEventFlag()
{
	char szQuery[1024];
	snprintf(szQuery, sizeof(szQuery), "SELECT szName, lValue FROM quest%s WHERE dwPID = 0", GetTablePostfix());
	std::unique_ptr<SQLMsg> pmsg(CDBManager::instance().DirectQuery(szQuery));

	SQLResult* pRes = pmsg->Get();
	if (pRes->uiNumRows)
	{
		MYSQL_ROW row;
		while ((row = mysql_fetch_row(pRes->pSQLResult)))
		{
			TPacketSetEventFlag p;
			strlcpy(p.szFlagName, row[0], sizeof(p.szFlagName));
			str_to_number(p.lValue, row[1]);
			LOG_INFO("EventFlag Load {} {}", p.szFlagName, p.lValue);
			m_map_lEventFlag.insert(std::make_pair(std::string(p.szFlagName), p.lValue));
			ForwardPacket(HEADER_DG_SET_EVENT_FLAG, &p, sizeof(TPacketSetEventFlag));
		}
	}
}

void CClientManager::SetEventFlag(TPacketSetEventFlag* p)
{
	ForwardPacket(HEADER_DG_SET_EVENT_FLAG, p, sizeof(TPacketSetEventFlag));

	bool bChanged = false;

	if (const auto it = m_map_lEventFlag.find(p->szFlagName); it == m_map_lEventFlag.end())
	{
		bChanged = true;
		m_map_lEventFlag.insert(std::make_pair(std::string(p->szFlagName), p->lValue));
	}
	else if (it->second != p->lValue)
	{
		bChanged = true;
		it->second = p->lValue;
	}

	if (bChanged)
	{
		char szQuery[1024];
		snprintf(szQuery, sizeof(szQuery),
				"REPLACE INTO quest%s (dwPID, szName, szState, lValue) VALUES(0, '%s', '', %d)",
				GetTablePostfix(), p->szFlagName, p->lValue);
		szQuery[1023] = '\0';

		//CDBManager::instance().ReturnQuery(szQuery, QID_QUEST_SAVE, 0, NULL);
		CDBManager::instance().AsyncQuery(szQuery);
		LOG_INFO("HEADER_GD_SET_EVENT_FLAG : Changed CClientmanager::SetEventFlag({} {}) ", p->szFlagName, p->lValue);
		return;
	}
	LOG_INFO("HEADER_GD_SET_EVENT_FLAG : No Changed CClientmanager::SetEventFlag({} {}) ", p->szFlagName, p->lValue);
}

void CClientManager::SendEventFlagsOnSetup(CPeer* peer)
{
	for (auto it = m_map_lEventFlag.begin(); it != m_map_lEventFlag.end(); ++it)
	{
		TPacketSetEventFlag p;
		strlcpy(p.szFlagName, it->first.c_str(), sizeof(p.szFlagName));
		p.lValue = it->second;
		peer->EncodeHeader(HEADER_DG_SET_EVENT_FLAG, 0, sizeof(TPacketSetEventFlag));
		peer->Encode(&p, sizeof(TPacketSetEventFlag));
	}
}

#ifdef ENABLE_ITEMSHOP
void CClientManager::SetEventFlag(const char* flag, int32_t value)
{
	TPacketSetEventFlag p;
	strlcpy(p.szFlagName, flag, sizeof(p.szFlagName));
	p.lValue = value;
	SetEventFlag(&p);
}
int CClientManager::GetEventFlag(const char* flag)
{
	auto it = m_map_lEventFlag.find(flag);
	if (it != m_map_lEventFlag.end())
		return it->second;
	return 0;
}
#endif
