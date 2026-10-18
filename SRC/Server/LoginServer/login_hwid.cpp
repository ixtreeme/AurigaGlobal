#include "login_hwid.h"

#include "login_db.h"
#include "login_session.h"

#include <Core/Logging.hpp>

#include <memory>

bool LoginHwid::Initialize(LoginDB& db)
{
	m_db = &db;

	std::unique_ptr<SQLMsg> msg(db.DirectQuery("SELECT hwid FROM account.blocked_hwids"));

	if (!msg || msg->uiSQLErrno || !msg->Get())
	{
		LOG_ERROR("LoginServer: cannot load the blocked HWID list");
		return false;
	}

	SQLResult* result = msg->Get();

	for (uint64_t i = 0; i < result->uiNumRows; ++i)
	{
		MYSQL_ROW row = result->FetchRow();

		if (row && row[0] && *row[0])
			m_blocked.insert(row[0]);
	}

	LOG_INFO("LoginServer: {} blocked HWID(s) loaded", m_blocked.size());
	return true;
}

bool LoginHwid::CheckAndUpdate(LoginSession* session, const std::string& login,
	const std::string& hwid, uint32_t accountId)
{
	if (hwid.empty())
	{
		session->SendLoginFailure("UPDATE");
		return false;
	}

	if (m_blocked.contains(hwid))
	{
		if (m_db)
		{
			m_db->Query("UPDATE account.account SET status='BAN2', hwid=? WHERE login=? AND STATUS='OK'",
				{ SqlParam::Text(hwid), SqlParam::Text(login) }, nullptr);
		}

		LOG_INFO("LoginServer: blocked HWID login attempt '{}'", login);
		session->SendLoginFailure("BAN2");
		return false;
	}

	if (m_db)
	{
		m_db->Query("UPDATE account.account SET hwid=? WHERE login=?",
			{ SqlParam::Text(hwid), SqlParam::Text(login) }, nullptr);

		m_db->Query("UPDATE player.player_index SET hwid=? WHERE id=?",
			{ SqlParam::Text(hwid), SqlParam::UInt(accountId) }, nullptr);
	}

	return true;
}
