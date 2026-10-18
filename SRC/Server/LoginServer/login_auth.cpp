#include "login_auth.h"

#include "login_config.h"
#include "login_db_link.h"
#include "login_limiter.h"
#include "login_session.h"

#include <Core/Logging.hpp>
#include <Core/Time.h>

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

	// The fixed-size strings of LOGIN3 are client bytes; only a terminated
	// one may be read as a C string.
	template <size_t N>
	bool Terminated(const char (&field)[N])
	{
		return std::memchr(field, '\0', N) != nullptr;
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
	if (!session || !packet || session->IsClosed())
		return;
	if (!session->CanStartLogin())
	{
		session->SendLoginFailure("ALREADY");
		return;
	}

	// A real client terminates every field; anything else is rejected before
	// login, password or hwid is read past its array.
	if (!Terminated(packet->login) || !Terminated(packet->passwd)
#ifdef ENABLE_HWID
		|| !Terminated(packet->hwid)
#endif
		)
	{
		session->SendLoginFailure("NOID");
		return;
	}

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
	session->BeginLogin(login);
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
	std::erase_if(m_logins, [session](const auto& entry) { return entry.second == session; });
	session->CompleteLogin(false);
}

void LoginAuth::FailLogin(LoginSession* session, const std::string& login, const char* status)
{
	m_pendingLogins.erase(login);
	session->EndDbQuery();
	session->CompleteLogin(false);
	if (!session->IsClosed() && status)
		session->SendLoginFailure(status);
}

void LoginAuth::OnRegistrationResult(LoginSession* session, const std::string& login, uint32_t loginKey, bool success)
{
	if (!success || session->IsClosed())
	{
		FailLogin(session, login, "SHUTDOWN");
		return;
	}

	m_pendingLogins.erase(login);
	session->EndDbQuery();
	session->CompleteLogin(true);
	m_logins[login] = session;
	if (m_limiter)
		m_limiter->ReportLoginResult(session->GetHost(), login, true);
	LOG_INFO("LoginServer: login registered for '{}'", login);
	session->SendAuthSuccess(loginKey);
}

void LoginAuth::OnLoginResult(LoginSession* session, const std::string& login, SQLMsg* msg, const LoginCGLogin3& request)
{
	if (session->IsClosed())
	{
		FailLogin(session, login, nullptr);
		return;
	}

	SQLResult* result = msg ? msg->Get() : nullptr;

	if (!result || result->uiNumRows == 0)
	{
		FailLogin(session, login, "NOID");
		return;
	}

	MYSQL_ROW row = result->FetchRow();

	if (!row || !row[0] || !row[1] || !row[2] || !row[3])
	{
		FailLogin(session, login, "NOID");
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

	int64_t premiumTimes[PREMIUM_MAX_NUM] = {};

	for (int i = PREMIUM_EXP; i <= PREMIUM_GOLD; ++i)
	{
		if (row[col])
			premiumTimes[i] = strtoll(row[col], nullptr, 10);

		++col;
	}

	char createDate[16] = "00000000";

	if (row[col])
	{
		const time_t createTime = static_cast<time_t>(strtoll(row[col], nullptr, 10));
		const std::tm createTm = auriga::time::LocalTime(createTime);

		strftime(createDate, sizeof(createDate), "%Y%m%d", &createTm);
	}

	if (strcmp(encPassword, storedPassword) != 0)
	{
		if (m_limiter)
			m_limiter->ReportLoginResult(session->GetHost(), login, false);

		FailLogin(session, login, "WRONGPWD");
		return;
	}

	if (notAvail && atoi(notAvail) != 0)
	{
		FailLogin(session, login, "NOTAVAIL");
		return;
	}

	if (!status || strcmp(status, "OK") != 0)
	{
		FailLogin(session, login, status ? status : "NOID");
		return;
	}

	if (!m_blockLoginDate.empty() && strncmp(createDate, m_blockLoginDate.c_str(), 8) >= 0)
	{
		FailLogin(session, login, "BLKLOGIN");
		return;
	}

	const uint32_t accountId = static_cast<uint32_t>(strtoul(accountIdText, nullptr, 10));

#ifdef ENABLE_HWID
	if (!m_hwid.CheckAndUpdate(session, login, request.hwid, accountId))
	{
		FailLogin(session, login, nullptr);
		return;
	}
#endif

	const uint32_t loginKey = GenerateLoginKey();

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

	if (!m_link || !m_link->SendAuthLogin(registration,
		[this, session, login, loginKey](bool success) { OnRegistrationResult(session, login, loginKey, success); }))
		FailLogin(session, login, "SHUTDOWN");
}

uint32_t LoginAuth::GenerateLoginKey()
{
	static std::mt19937 rng(std::random_device {}());
	static std::uniform_int_distribution<uint32_t> dist(1, 0x7fffffff);

	return dist(rng);
}
