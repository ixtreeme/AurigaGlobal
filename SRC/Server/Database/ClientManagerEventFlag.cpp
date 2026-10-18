// vim:ts=4 sw=4
#include "stdafx.h"
#include "ClientManager.h"
#include "Main.h"
#include "DBManager.h"
#include "QID.h"
#include <common/event_flag.h>
#include <Core/packet_string.h>

void CClientManager::LoadEventFlag()
{
	char szQuery[1024];
	snprintf(szQuery, sizeof(szQuery), "SELECT szName, lValue FROM quest%s WHERE dwPID = 0", GetTablePostfix());
	std::unique_ptr<SQLMsg> pmsg(CDBManager::instance().DirectQuery(szQuery));

	SQLResult* pRes = pmsg->Get();
	if (pRes->uiNumRows)
	{
		MYSQL_ROW row;
		while ((row = pRes->FetchRow()))
		{
			TPacketSetEventFlag p {};
			int64_t value = 0;
			if (!row[0] || !row[1] || !EventFlag::ParseValue(row[1], value) ||
				!EventFlag::BuildPacket(row[0], value, p)) {
				LOG_ERROR("Invalid event flag row");
				continue;
			}
			LOG_INFO("EventFlag Load {} {}", p.szFlagName, p.lValue);
			m_map_lEventFlag.insert(std::make_pair(std::string(p.szFlagName), p.lValue));
			ForwardPacket(HEADER_DG_SET_EVENT_FLAG, &p, sizeof(TPacketSetEventFlag));
		}
	}
}

void CClientManager::SetEventFlag(TPacketSetEventFlag* p)
{
	if (!p || !packet_string::IsTerminated(p->szFlagName) || !p->szFlagName[0])
		return;
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
				"REPLACE INTO quest%s (dwPID, szName, szState, lValue) VALUES(0, ?, '', ?)",
				GetTablePostfix());
		szQuery[1023] = '\0';

		CDBManager::instance().QueryPrepared(szQuery,
				{ SqlParam::Text(p->szFlagName), SqlParam::Int(p->lValue) });
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
void CClientManager::SetEventFlag(const char* flag, int64_t value)
{
	TPacketSetEventFlag p {};
	if (!flag || !EventFlag::BuildPacket(flag, value, p)) return;
	SetEventFlag(&p);
}
int64_t CClientManager::GetEventFlag(const char* flag)
{
	auto it = m_map_lEventFlag.find(flag);
	if (it != m_map_lEventFlag.end())
		return it->second;
	return 0;
}
#endif
