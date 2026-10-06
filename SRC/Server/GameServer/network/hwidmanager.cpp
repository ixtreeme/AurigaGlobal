#include "stdafx.h"
#include <Core/Logging.hpp>
#include <common/length.h>

#include "hwidmanager.h"
#include "config.h"
#include "db.h"
#include "packet.h"
#include "desc_client.h"

CHwidManager::CHwidManager()
{
	
}

CHwidManager::~CHwidManager()
{
	m_blocked_hwid.clear();
}

void CHwidManager::CleanBlocked()
{
	m_blocked_hwid.clear();
}

void CHwidManager::InitializeBlocked()
{
	CleanBlocked();

	std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("SELECT hwid FROM account.blocked_hwids"));
	if (msg->Get()->uiNumRows > 0) {
		MYSQL_ROW row;
		while ((row = msg->Get()->FetchRow())) {
			char hwid[HWID_LENGTH];
			strlcpy(hwid, row[0], sizeof(hwid));
			if (std::find(m_blocked_hwid.begin(), m_blocked_hwid.end(), hwid) == m_blocked_hwid.end()) {
				m_blocked_hwid.push_back(hwid);
			}
		}
	}
}

bool CHwidManager::IsBlocked(const char * hwid)
{
	return std::find(m_blocked_hwid.begin(), m_blocked_hwid.end(), hwid) != m_blocked_hwid.end();
}

void CHwidManager::AddHwidToAccount(const char * login, const char * hwid) {
	DBManager::instance().QueryPrepared(
		"UPDATE account.account SET status='BAN2', hwid=? WHERE login=? and STATUS='OK'",
		{ SqlParam::Text(hwid), SqlParam::Text(login) });
}

void CHwidManager::SendBlockHwid(const char * whoname, const char * targetname)
{
	THwidRequest pack;
	strlcpy(pack.whoname, whoname, sizeof(pack.whoname));
	strlcpy(pack.targetname, targetname, sizeof(pack.targetname));

	db_clientdesc->DBPacket(HEADER_GD_BLOCKHWID, 0, &pack, sizeof(pack));
}

void CHwidManager::SendUnblockHwid(const char * whoname, const char * targetname)
{
	THwidRequest pack;
	strlcpy(pack.whoname, whoname, sizeof(pack.whoname));
	strlcpy(pack.targetname, targetname, sizeof(pack.targetname));

	db_clientdesc->DBPacket(HEADER_GD_UNBLOCKHWID, 0, &pack, sizeof(pack));
}

