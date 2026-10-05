#include "stdafx.h"
#include <Core/Logging.hpp>
#include "constants.h"
#include "config.h"
#include "input.h"
#include "desc_client.h"
#include "desc_manager.h"
#include "protocol.h"
#include "locale_service.h"
#include "db.h"

#include "utils.h"

// #define ENABLE_ACCOUNT_W_SPECIALCHARS
bool FN_IS_VALID_LOGIN_STRING(const char *str)
{
	const char*	tmp;

	if (!str || !*str)
		return false;

	if (strlen(str) < 2)
		return false;

	for (tmp = str; *tmp; ++tmp)
	{
		if (isdigit(*tmp) || isalpha(*tmp))
			continue;

#ifdef ENABLE_ACCOUNT_W_SPECIALCHARS
		switch (*tmp)
		{
			case ' ':
			case '_':
			case '-':
			case '.':
			case '!':
			case '@':
			case '#':
			case '$':
			case '%':
			case '^':
			case '&':
			case '*':
			case '(':
			case ')':
				continue;
		}
#endif
		return false;
	}

	return true;
}

bool Login_IsInChannelService(const char* c_login)
{
	if (c_login[0] == '[')
		return true;
	return false;
}

CInputAuth::CInputAuth()
{
}

void CInputAuth::Login(LPDESC d, const char * c_pData)
{
	TPacketCGLogin3 * pinfo = (TPacketCGLogin3 *) c_pData;
	if (!g_bAuthServer)
	{
		LOG_ERROR("CInputAuth class is not for game server. IP {} might be a hacker.", inet_ntoa(d->GetAddr().sin_addr));
		d->DelayedDisconnect(5);
		return;
	}

	char login[LOGIN_MAX_LEN + 1];
	trim_and_lower(pinfo->login, login, sizeof(login));

	char passwd[PASSWD_MAX_LEN + 1];
	strlcpy(passwd, pinfo->passwd, sizeof(passwd));

	LOG_INFO("InputAuth::Login : {}({}) desc {}", login, strlen(login), static_cast<const void*>(get_pointer(d)));

	// check login string
	if (false == FN_IS_VALID_LOGIN_STRING(login))
	{
		LOG_INFO("InputAuth::Login : IS_NOT_VALID_LOGIN_STRING({}) desc {}", login, static_cast<const void*>(get_pointer(d)));
		LoginFailure(d, "NOID");
		return;
	}

	if (g_bNoMoreClient)
	{
		TPacketGCLoginFailure failurePacket;

		failurePacket.header = HEADER_GC_LOGIN_FAILURE;
		strlcpy(failurePacket.szStatus, "SHUTDOWN", sizeof(failurePacket.szStatus));

		d->Packet(&failurePacket, sizeof(failurePacket));
		return;
	}

	if (DESC_MANAGER::instance().FindByLoginName(login))
	{
		LoginFailure(d, "ALREADY");
		return;
	}

	uint32_t dwKey = DESC_MANAGER::instance().CreateLoginKey(d);
	LOG_INFO("InputAuth::Login: key ({}) login ({})", dwKey, login);

	TPacketCGLogin3 * p = M2_NEW TPacketCGLogin3;
	memcpy(p, pinfo, sizeof(TPacketCGLogin3));

	char szPasswd[PASSWD_MAX_LEN * 2 + 1];
	DBManager::instance().EscapeString(szPasswd, sizeof(szPasswd), passwd, strlen(passwd));

	char szLogin[LOGIN_MAX_LEN * 2 + 1];
	DBManager::instance().EscapeString(szLogin, sizeof(szLogin), login, strlen(login));

	// CHANNEL_SERVICE_LOGIN
	if (Login_IsInChannelService(szLogin))
	{
		LOG_INFO("ChannelServiceLogin [{}]", szLogin);

		DBManager::instance().ReturnQuery(QID_AUTH_LOGIN, dwKey, p,
				"SELECT '%s',password,social_id,id,status,availDt - NOW() > 0,"
				"UNIX_TIMESTAMP(silver_expire),"
				"UNIX_TIMESTAMP(gold_expire),"
				"UNIX_TIMESTAMP(safebox_expire),"
				"UNIX_TIMESTAMP(autoloot_expire),"
				"UNIX_TIMESTAMP(fish_mind_expire),"
				"UNIX_TIMESTAMP(marriage_fast_expire),"
				"UNIX_TIMESTAMP(money_drop_rate_expire),"
				"UNIX_TIMESTAMP(create_time)"
				" FROM account WHERE login='%s'",

				szPasswd, szLogin);
	}
	// END_OF_CHANNEL_SERVICE_LOGIN
	else
	{
		// @fixme138 1. PASSWORD('%s') -> %s 2. szPasswd wrapped inside mysql_hash_password(%s).c_str()
		DBManager::instance().ReturnQuery(QID_AUTH_LOGIN, dwKey, p,
			"SELECT PASSWORD ('%s'),password,social_id,id,"
#ifdef ENABLE_MULTI_LANGUAGE
			"language+0,"
#endif
			"status,availDt - NOW() > 0,"
			"UNIX_TIMESTAMP(silver_expire),"
			"UNIX_TIMESTAMP(gold_expire),"
			"UNIX_TIMESTAMP(safebox_expire),"
			"UNIX_TIMESTAMP(autoloot_expire),"
			"UNIX_TIMESTAMP(fish_mind_expire),"
			"UNIX_TIMESTAMP(marriage_fast_expire),"
			"UNIX_TIMESTAMP(money_drop_rate_expire),"
			"UNIX_TIMESTAMP(create_time)"
			" FROM account WHERE login='%s'",
			szPasswd,szLogin);
	}
}

void CInputAuth::LoginOpenID(LPDESC d, const char * c_pData)
{
	//OpenID test code.
	TPacketCGLogin5 *tempInfo1 = (TPacketCGLogin5 *)c_pData;

	char* authKey = tempInfo1->authKey;
	char returnID[LOGIN_MAX_LEN + 1] = {0};

	int test_url_get_protocol = auth_OpenID(authKey, inet_ntoa(d->GetAddr().sin_addr), returnID);

	if (0!=test_url_get_protocol)
	{
		LoginFailure(d, "OpenID Fail");
		return;
	}

	TPacketCGLogin3 tempInfo2;
	strncpy(tempInfo2.login, returnID, LOGIN_MAX_LEN);
	strncpy(tempInfo2.passwd, "0000", PASSWD_MAX_LEN);
	for(int i=0;i<4;i++)
		tempInfo2.adwClientKey[i] = tempInfo1->adwClientKey[i];
	TPacketCGLogin3 * pinfo = &tempInfo2;

	if (!g_bAuthServer)
	{
		LOG_ERROR("CInputAuth class is not for game server. IP {} might be a hacker.", inet_ntoa(d->GetAddr().sin_addr));
		d->DelayedDisconnect(5);
		return;
	}

	char login[LOGIN_MAX_LEN + 1];
	trim_and_lower(pinfo->login, login, sizeof(login));

	char passwd[PASSWD_MAX_LEN + 1];
	strlcpy(passwd, pinfo->passwd, sizeof(passwd));

	LOG_INFO("InputAuth::Login : {}({}) desc {}", login, strlen(login), static_cast<const void*>(get_pointer(d)));

	// check login string
	if (false == FN_IS_VALID_LOGIN_STRING(login))
	{
		LOG_INFO("InputAuth::Login : IS_NOT_VALID_LOGIN_STRING({}) desc {}", login, static_cast<const void*>(get_pointer(d)));
		LoginFailure(d, "NOID");
		return;
	}

	if (g_bNoMoreClient)
	{
		TPacketGCLoginFailure failurePacket;

		failurePacket.header = HEADER_GC_LOGIN_FAILURE;
		strlcpy(failurePacket.szStatus, "SHUTDOWN", sizeof(failurePacket.szStatus));

		d->Packet(&failurePacket, sizeof(failurePacket));
		return;
	}

	if (DESC_MANAGER::instance().FindByLoginName(login))
	{
		LoginFailure(d, "ALREADY");
		return;
	}

	uint32_t dwKey = DESC_MANAGER::instance().CreateLoginKey(d);
	LOG_INFO("InputAuth::Login: key ({}) login ({})", dwKey, login);

	TPacketCGLogin3 * p = M2_NEW TPacketCGLogin3;
	memcpy(p, pinfo, sizeof(TPacketCGLogin3));

	char szPasswd[PASSWD_MAX_LEN * 2 + 1];
	DBManager::instance().EscapeString(szPasswd, sizeof(szPasswd), passwd, strlen(passwd));

	char szLogin[LOGIN_MAX_LEN * 2 + 1];
	DBManager::instance().EscapeString(szLogin, sizeof(szLogin), login, strlen(login));

	// CHANNEL_SERVICE_LOGIN
	if (Login_IsInChannelService(szLogin))
	{
		LOG_INFO("ChannelServiceLogin [{}]", szLogin);

		DBManager::instance().ReturnQuery(QID_AUTH_LOGIN, dwKey, p,
				"SELECT '%s',password,social_id,id,status,availDt - NOW() > 0,"
				"UNIX_TIMESTAMP(silver_expire),"
				"UNIX_TIMESTAMP(gold_expire),"
				"UNIX_TIMESTAMP(safebox_expire),"
				"UNIX_TIMESTAMP(autoloot_expire),"
				"UNIX_TIMESTAMP(fish_mind_expire),"
				"UNIX_TIMESTAMP(marriage_fast_expire),"
				"UNIX_TIMESTAMP(money_drop_rate_expire),"
				"UNIX_TIMESTAMP(create_time)"
				" FROM account WHERE login='%s'",

				szPasswd, szLogin);
	}
	// END_OF_CHANNEL_SERVICE_LOGIN
	else
	{
		// @fixme138 1. PASSWORD('%s') -> %s 2. szPasswd wrapped inside mysql_hash_password(%s).c_str()
		DBManager::instance().ReturnQuery(QID_AUTH_LOGIN, dwKey, p,
				"SELECT '%s',password,social_id,id,status,availDt - NOW() > 0,"
				"UNIX_TIMESTAMP(silver_expire),"
				"UNIX_TIMESTAMP(gold_expire),"
				"UNIX_TIMESTAMP(safebox_expire),"
				"UNIX_TIMESTAMP(autoloot_expire),"
				"UNIX_TIMESTAMP(fish_mind_expire),"
				"UNIX_TIMESTAMP(marriage_fast_expire),"
				"UNIX_TIMESTAMP(money_drop_rate_expire),"
				"UNIX_TIMESTAMP(create_time)"
				" FROM account WHERE login='%s'",
				/*mysql_hash_password(szPasswd).c_str(),*/ szPasswd,szLogin);
	}
}

//OpenID
int CInputAuth::auth_OpenID(const char *authKey, const char *ipAddr, char *rID)
{
	//return value
	//0 : normal execution
	//1 : cannot connect to openid auth server
	//2 : socket_write failed
	//3 : openid auth server not reply
	//4 : Reply Error
	//5 : Incorrect auth key.

	extern char openid_host[256];
	extern char openid_uri[256];

	const int port = 80;

	// Dedicated io_context: this is a blocking helper called from the main
	// thread, it must not interleave with the shared server engine.
	boost::asio::io_context io;
	auriga::net::tcp::socket socket(io);
	boost::asio::steady_timer timer(io);
	boost::system::error_code ec;

	auto close_socket = [&socket]()
	{
		boost::system::error_code ignored;
		socket.close(ignored);
	};

	auriga::net::tcp::resolver resolver(io);
	auto results = resolver.resolve(openid_host, std::to_string(port), ec);

	if (ec)
	{
		LOG_ERROR("[auth_OpenID] : could not connect to OpenID server({}) ({})", openid_host, ec.message().c_str());
		return 1;
	}

	bool done = false;

	boost::asio::async_connect(socket, results,
		[&](const boost::system::error_code & e, const auriga::net::tcp::endpoint &)
		{
			if (!done)
			{
				ec = e;
				done = true;
			}
			timer.cancel();
		});

	timer.expires_after(std::chrono::seconds(3));
	timer.async_wait(
		[&](const boost::system::error_code & e)
		{
			if (!e)
			{
				close_socket();
				if (!done)
				{
					ec = boost::asio::error::timed_out;
					done = true;
				}
			}
		});

	io.run();

	if (!done || ec)
	{
		LOG_ERROR("[auth_OpenID] : could not connect to OpenID server({}) ({})", openid_host, ec.message().c_str());
		close_socket();
		return 1;
	}

	// send request
	char request[512];
	int request_len = snprintf(request, sizeof(request),
					"GET %s?auth_key=%s&ip=%s HTTP/1.1\r\n"
					"Host: %s\r\n"
					"Connection: Close\r\n\r\n",
					openid_uri, authKey, ipAddr, openid_host);

	done = false;
	ec.clear();

	boost::asio::async_write(socket, boost::asio::buffer(request, request_len),
		[&](const boost::system::error_code & e, std::size_t)
		{
			if (!done)
			{
				ec = e;
				done = true;
			}
			timer.cancel();
		});

	timer.expires_after(std::chrono::seconds(3));
	timer.async_wait(
		[&](const boost::system::error_code & e)
		{
			if (!e)
			{
				close_socket();
				if (!done)
				{
					ec = boost::asio::error::timed_out;
					done = true;
				}
			}
		});

	io.restart();
	io.run();

	if (!done || ec)
	{
		LOG_ERROR("[auth_OpenID] : could not send auth-request ({})", authKey);
		close_socket();
		return 2;
	}

    // read reply
    {
	char reply[1024] = {0};
	int len = -1;
	std::size_t read_len = 0;

	done = false;
	ec.clear();

	socket.async_read_some(boost::asio::buffer(reply, sizeof(reply) - 1),
		[&](const boost::system::error_code & e, std::size_t n)
		{
			if (!done)
			{
				ec = e;
				read_len = n;
				done = true;
			}
			timer.cancel();
		});

	timer.expires_after(std::chrono::seconds(3));
	timer.async_wait(
		[&](const boost::system::error_code & e)
		{
			if (!e)
			{
				close_socket();
				if (!done)
				{
					ec = boost::asio::error::timed_out;
					done = true;
				}
			}
		});

	io.restart();
	io.run();

	close_socket();

	if (done && !ec && read_len > 0)
		len = static_cast<int>(read_len);

	if (len <= 0)
	{
	    LOG_ERROR("[auth_OpenID] : could not recv auth-reply ({})", authKey);
	    return 3;
	}

	char buffer[1024];
	strcpy(buffer, reply);

	const char *delim = "\r\n";
	//char *last = 0;
	char *v = strtok(buffer, delim);
	char *result = nullptr;

	while (v)
	{
		result = v;
		v = strtok(nullptr, delim);
	}


	char *id = strtok(result, "%");
	char *success = strtok(nullptr, "%");

	if (!*id || !*success)
	{
	    LOG_ERROR("[auth_OpenID] : OpenID AuthServer Reply Error ({})", reply);
		return 4;
	}

	if (0 != strcmp("OK", success))
	{
		int returnNumber = 0;
		str_to_number(returnNumber, id);
		switch (returnNumber)
		{
		case 1:
			LOG_ERROR("[auth_OpenID] : AuthKey incorrect");
			break;
		case 2:
			LOG_ERROR("[auth_OpenID] : ip incorrect");
			break;
		case 3:
			LOG_ERROR("[auth_OpenID] : used AuthKey");
			break;
		case 4:
			LOG_ERROR("[auth_OpenID] : AuthKey not delivered");
			break;
		case 5:
			LOG_ERROR("[auth_OpenID] : ip not delivered");
			break;
		case 6:
			LOG_ERROR("[auth_OpenID] : AuthKey time over");
			break;
		default:
			break;
		}

		return 5;
	}

	strcpy(rID, id);

	return 0;
    }
}


int CInputAuth::Analyze(LPDESC d, uint8_t bHeader, const char * c_pData)
{

	if (!g_bAuthServer)
	{
		LOG_ERROR("CInputAuth class is not for game server. IP {} might be a hacker.", inet_ntoa(d->GetAddr().sin_addr));
		d->DelayedDisconnect(5);
		return 0;
	}

	int iExtraLen = 0;

	if (test_server)
		LOG_TRACE(" InputAuth Analyze Header[{}] ", static_cast<int>(bHeader));

	switch (bHeader)
	{
		case HEADER_CG_PONG:
			Pong(d);
			break;

		case HEADER_CG_LOGIN3:
			Login(d, c_pData);
			break;

		case HEADER_CG_LOGIN5_OPENID:
			if (openid_server)
				LoginOpenID(d, c_pData);
			else
				LOG_ERROR("HEADER_CG_LOGIN5_OPENID : wrong client access");
			break;
		case HEADER_CG_HANDSHAKE:
			break;

		default:
			LOG_ERROR("This phase does not handle this header {} (0x{:x})(phase: AUTH)", static_cast<int>(bHeader), static_cast<int>(bHeader));
			break;
	}

	return iExtraLen;
}
