#include "login_auth.h"

#include "login_config.h"
#include "login_db_link.h"
#include "login_limiter.h"
#include "login_session.h"

#include <Core/Logging.hpp>

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <random>

namespace
{
	// Premium slot indices (common/tables.h).
	enum
	{
		PREMIUM_EXP = 0,
		PREMIUM_ITEM,
		PREMIUM_SAFEBOX,
		PREMIUM_AUTOLOOT,
		PREMIUM_FISH_MIND,
		PREMIUM_MARRIAGE_FAST,
		PREMIUM_GOLD,
	};

	bool IsValidLoginString(const std::string& login)
	{
		if (login.size() < 2 || login.size() > LOGIN_MAX_LEN)
			return false;

		for (const unsigned char c : login)
			if (!std::isalnum(c))
				return false;

		return true;
	}
}

LoginAuth::LoginAuth() = default;

LoginAuth::~LoginAuth() = default;

bool LoginAuth::Init(const LoginConfig& config)
{
	m_blockLoginDate = config.blockLoginDate;

	if (!m_db.Connect(config))
		return false;

#ifdef ENABLE_HWID
	if (m_db.ConnectDirect(config))
		m_hwid.Initialize(m_db);
#endif

	return true;
}

void LoginAuth::Process()
{
	m_db.Process();
}

void LoginAuth::RequestLogin(LoginSession* session, const LoginCGLogin3* packet)
{
	std::string login(packet->login);

	for (char& c : login)
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

	if (!IsValidLoginString(login))
	{
		session->SendLoginFailure("NOID");
		return;
	}

	if (m_logins.contains(login) || m_pendingLogins.contains(login))
	{
		session->SendLoginFailure("ALREADY");
		return;
	}

	if (m_limiter && !m_limiter->AllowLoginAttempt(session->GetHost(), login))
	{
		LOG_INFO("LoginServer: login attempt for '{}' from {} blocked by the limiter", login, session->GetHost());
		session->SendLoginFailure("BLOCK");
		return;
	}

	const std::string passwd(packet->passwd);

	m_pendingLogins.insert(login);
	session->SetLogin(login);
	session->BeginDbQuery();

	LOG_INFO("LoginServer: login request for '{}'", login);

	const LoginCGLogin3 request = *packet;

	const std::string query =
		"SELECT PASSWORD(?), password, social_id, id,"
#ifdef ENABLE_MULTI_LANGUAGE
		" language+0,"
#endif
		" status, availDt - NOW() > 0,"
		" UNIX_TIMESTAMP(silver_expire),"
		" UNIX_TIMESTAMP(gold_expire),"
		" UNIX_TIMESTAMP(safebox_expire),"
		" UNIX_TIMESTAMP(autoloot_expire),"
		" UNIX_TIMESTAMP(fish_mind_expire),"
		" UNIX_TIMESTAMP(marriage_fast_expire),"
		" UNIX_TIMESTAMP(money_drop_rate_expire),"
		" UNIX_TIMESTAMP(create_time)"
		" FROM account WHERE login=?";

	m_db.Query(query,
		{ SqlParam::Text(passwd), SqlParam::Text(login) },
		[this, session, login, request](SQLMsg* msg) { OnLoginResult(session, login, msg, request); });
}

void LoginAuth::OnSessionClosed(LoginSession* session)
{
	if (session->GetLogin().empty())
		return;

	const auto it = m_logins.find(session->GetLogin());

	if (it != m_logins.end() && it->second == session)
		m_logins.erase(it);
}

void LoginAuth::OnLoginResult(LoginSession* session, const std::string& login, SQLMsg* msg, const LoginCGLogin3& request)
{
	session->EndDbQuery();
	m_pendingLogins.erase(login);

	if (session->IsClosed())
		return;

	SQLResult* result = msg ? msg->Get() : nullptr;

	if (!result || result->uiNumRows == 0)
	{
		session->SendLoginFailure("NOID");
		return;
	}

	MYSQL_ROW row = result->FetchRow();

	if (!row || !row[0] || !row[1] || !row[2] || !row[3])
	{
		session->SendLoginFailure("NOID");
		return;
	}

	int col = 0;
	const char* encPassword = row[col++];
	const char* storedPassword = row[col++];
	const char* socialId = row[col++];
	const char* accountIdText = row[col++];

	uint8_t language = 0;
#ifdef ENABLE_MULTI_LANGUAGE
	if (row[col])
		language = static_cast<uint8_t>(atoi(row[col]));
	++col;
#endif

	const char* status = row[col++];
	const char* notAvail = row[col++];

	int premiumTimes[PREMIUM_MAX_NUM] = {};

	for (int i = PREMIUM_EXP; i <= PREMIUM_GOLD; ++i)
	{
		if (row[col])
			premiumTimes[i] = atoi(row[col]);

		++col;
	}

	char createDate[16] = "00000000";

	if (row[col])
	{
		const time_t createTime = static_cast<time_t>(strtoll(row[col], nullptr, 10));
		const struct tm* createTm = localtime(&createTime);

		if (createTm)
			strftime(createDate, sizeof(createDate), "%Y%m%d", createTm);
	}

	if (strcmp(encPassword, storedPassword) != 0)
	{
		if (m_limiter)
			m_limiter->ReportLoginResult(session->GetHost(), login, false);

		session->SendLoginFailure("WRONGPWD");
		return;
	}

	if (notAvail && atoi(notAvail) != 0)
	{
		session->SendLoginFailure("NOTAVAIL");
		return;
	}

	if (strcmp(status, "OK") != 0)
	{
		session->SendLoginFailure(status);
		return;
	}

	if (!m_blockLoginDate.empty() && strncmp(createDate, m_blockLoginDate.c_str(), 8) >= 0)
	{
		session->SendLoginFailure("BLKLOGIN");
		return;
	}

	const uint32_t accountId = static_cast<uint32_t>(strtoul(accountIdText, nullptr, 10));

#ifdef ENABLE_HWID
	if (!m_hwid.CheckAndUpdate(session, login, request.hwid, accountId))
		return;
#endif

	const uint32_t loginKey = GenerateLoginKey();

	m_logins[login] = session;

	if (m_limiter)
		m_limiter->ReportLoginResult(session->GetHost(), login, true);

	LOG_INFO("LoginServer: login OK for '{}' (account {}, key {})", login, accountId, loginKey);

	session->SendAuthSuccess(loginKey);

	if (m_link)
	{
		LoginGDAuthLogin registration {};
		registration.dwID = accountId;
#ifdef ENABLE_HWID
		strlcpy(registration.hwid, request.hwid, sizeof(registration.hwid));
#endif
		registration.dwLoginKey = loginKey;
		strlcpy(registration.szLogin, login.c_str(), sizeof(registration.szLogin));
		strlcpy(registration.szSocialID, socialId, sizeof(registration.szSocialID));
		std::memcpy(registration.adwClientKey, request.adwClientKey, sizeof(registration.adwClientKey));
#ifdef ENABLE_MULTI_LANGUAGE
		registration.bLanguage = language;
#endif
		std::memcpy(registration.iPremiumTimes, premiumTimes, sizeof(registration.iPremiumTimes));

		m_link->SendAuthLogin(registration);
	}
}

uint32_t LoginAuth::GenerateLoginKey()
{
	static std::mt19937 rng(std::random_device {}());
	static std::uniform_int_distribution<uint32_t> dist(1, 0x7fffffff);

	return dist(rng);
}
