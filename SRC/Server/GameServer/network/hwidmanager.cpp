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
		while ((row = mysql_fetch_row(msg->Get()->pSQLResult))) {
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
	std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("UPDATE account.account SET status='BAN2', hwid='%s' WHERE login='%s' and STATUS='OK'", hwid, login));
}

void CHwidManager::SendBlockHwid(const char * whoname, const char * targetname)
{
	THwidRequest pack;
	strlcpy(pack.whoname, whoname, sizeof(pack.whoname));
	strlcpy(pack.targetname, targetname, sizeof(pack.targetname));

	db_clientdesc->DBPacket(HEADER_GD_BLOCKHWID, 0, &pack, sizeof(pack));
}

void CHwidManager::RecvBlockHwid(const char * whoname, const char * targetname)
{
	if (!g_bAuthServer) {
		return;
	}

	char whoplayername[CHARACTER_NAME_MAX_LEN * 2 + 1];
	DBManager::instance().EscapeString(whoplayername, sizeof(whoplayername), whoname, strlen(whoname));

	char playername[CHARACTER_NAME_MAX_LEN * 2 + 1];
	DBManager::instance().EscapeString(playername, sizeof(playername), targetname, strlen(targetname));

	std::unique_ptr<SQLMsg> check(DBManager::instance().DirectQuery("SELECT mID FROM common.gmlist WHERE mName='%s' AND mAuthority='IMPLEMENTOR' LIMIT 1", playername));
	if (check->Get()->uiNumRows > 0) {
		LOG_INFO("HWID:: {} tried to block {} (IMPLEMENTOR).", whoplayername, playername);
		return;
	}

	std::unique_ptr<SQLMsg> msgid(DBManager::instance().DirectQuery("SELECT account_id FROM player.player WHERE name='%s' LIMIT 1", playername));
	if (msgid->Get()->uiNumRows > 0) {
		MYSQL_ROW rowid = mysql_fetch_row(msgid->Get()->pSQLResult);

		std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("SELECT hwid FROM player.player_index WHERE id=%d LIMIT 1", atoi(rowid[0])));
		if (msg->Get()->uiNumRows > 0) {
			MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult);

			char hwid[HWID_LENGTH];
			strlcpy(hwid, row[0], sizeof(hwid));

			if (strlen(hwid) == 0) {
				LOG_INFO("HWID:: Player: {} have invalid hwid, check them manually.", targetname);
				return;
			}

			if (IsBlocked(hwid)) {
				LOG_INFO("HWID:: {} is already blocked.", hwid);
				return;
			}

			m_blocked_hwid.push_back(hwid);

			std::string ids;

			std::unique_ptr<SQLMsg> getaccountsid(DBManager::instance().DirectQuery("SELECT id FROM player.player_index WHERE hwid='%s'", hwid));
			if (getaccountsid->Get()->uiNumRows > 0) {
				std::vector<int32_t> vec_ids;

				MYSQL_ROW row2;
				while ((row2 = mysql_fetch_row(getaccountsid->Get()->pSQLResult))) {
					int32_t id = atoi(row2[0]);
					if (std::find(vec_ids.begin(), vec_ids.end(), id) == vec_ids.end()) {
						vec_ids.push_back(id);
					}
				}

				for (auto id : vec_ids) {
					ids += std::to_string(id);
					ids += ",";
				}

				if (!ids.empty()) {
					ids.erase(ids.end()- 1);
					std::unique_ptr<SQLMsg> blockaccountsid(DBManager::instance().DirectQuery("UPDATE account.account SET status='BAN1' WHERE id IN (%s)", ids.c_str()));
				}
			}

			std::unique_ptr<SQLMsg> blockhwid(DBManager::instance().DirectQuery("INSERT INTO account.blocked_hwids SET hwid='%s', who='%s', accounts='%s'", hwid, whoplayername, ids.c_str()));
		}
	}
}

void CHwidManager::SendUnblockHwid(const char * whoname, const char * targetname)
{
	THwidRequest pack;
	strlcpy(pack.whoname, whoname, sizeof(pack.whoname));
	strlcpy(pack.targetname, targetname, sizeof(pack.targetname));

	db_clientdesc->DBPacket(HEADER_GD_UNBLOCKHWID, 0, &pack, sizeof(pack));
}

void CHwidManager::RecvUnblockHwid(const char * whoname, const char * targetname)
{
	if (!g_bAuthServer) {
		return;
	}

	char whoplayername[CHARACTER_NAME_MAX_LEN * 2 + 1];
	DBManager::instance().EscapeString(whoplayername, sizeof(whoplayername), whoname, strlen(whoname));

	char playername[CHARACTER_NAME_MAX_LEN * 2 + 1];
	DBManager::instance().EscapeString(playername, sizeof(playername), targetname, strlen(targetname));

	std::unique_ptr<SQLMsg> msgid(DBManager::instance().DirectQuery("SELECT account_id FROM player.player WHERE name='%s' LIMIT 1", playername));
	if (msgid->Get()->uiNumRows > 0) {
		MYSQL_ROW rowid = mysql_fetch_row(msgid->Get()->pSQLResult);

		std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("SELECT hwid FROM player.player_index WHERE id=%d LIMIT 1", atoi(rowid[0])));
		if (msg->Get()->uiNumRows > 0) {
			MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult);

			char hwid[HWID_LENGTH];
			strlcpy(hwid, row[0], sizeof(hwid));

			if (strlen(hwid) == 0) {
				LOG_INFO("HWID:: Player: {} have invalid hwid, check them manually.", targetname);
				return;
			}

			if (!IsBlocked(hwid)) {
				LOG_INFO("HWID:: {} is already unblocked.", hwid);
				return;
			}


			auto v = std::find(m_blocked_hwid.begin(), m_blocked_hwid.end(), hwid);
			if (v != m_blocked_hwid.end()) {
				m_blocked_hwid.erase(v);
			}

			std::unique_ptr<SQLMsg> getaccountsid(DBManager::instance().DirectQuery("SELECT accounts FROM account.blocked_hwids WHERE hwid='%s'", hwid));
			if (getaccountsid->Get()->uiNumRows > 0) {
				MYSQL_ROW row2 = mysql_fetch_row(getaccountsid->Get()->pSQLResult);

				char accountsid[255 + 1];
				strlcpy(accountsid, row2[0], sizeof(accountsid));

				std::unique_ptr<SQLMsg> unlockaccounts(DBManager::instance().DirectQuery("UPDATE account.account SET status='OK' WHERE id in (%s)", accountsid));
				std::unique_ptr<SQLMsg>(DBManager::instance().DirectQuery("DELETE FROM account.blocked_hwids WHERE hwid='%s'", hwid));
			}
		}
	}
}

