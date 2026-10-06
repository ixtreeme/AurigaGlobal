#pragma once

#include <Core/asio_engine.h>
#include <Core/peer_session.h>
#include <Core/utils.h>

#include <common/service.h>
#include <common/Headers/Header_CG.h>
#include <common/Headers/Header_GC.h>

#include "cipher.h"

#include <cstdint>
#include <string>

// Protocol sizes replicated from common/length.h so the login server stays
// free of the game header dependencies. Keep in sync with the client.
constexpr int LOGIN_MAX_LEN = 30;
constexpr int PASSWD_MAX_LEN = 16;
constexpr int ACCOUNT_STATUS_MAX_LEN = 8;
constexpr int CHARACTER_NAME_MAX_LEN = 24;
constexpr int HWID_LENGTH = 64;

// Login protocol packets. Only the handshake and account login subset is
// replicated here, with the exact client layout, so the login server does
// not pull the full game packet header set.
#pragma pack(1)

struct LoginCGHandshake
{
	uint8_t		bHeader;
	uint32_t	dwHandshake;
	uint32_t	dwTime;
	int32_t		lDelta;
};

struct LoginGCHandshake
{
	uint8_t		bHeader;
	uint32_t	dwHandshake;
	uint32_t	dwTime;
	int32_t		lDelta;
};

struct LoginCGLogin3
{
	uint8_t		header;
	char		login[LOGIN_MAX_LEN + 1];
	char		passwd[PASSWD_MAX_LEN + 1];
	uint32_t	adwClientKey[4];
#ifdef ENABLE_HWID
	char		hwid[HWID_LENGTH + 1];
#endif
};

struct LoginKeyAgreement
{
	static const int MAX_DATA_LEN = 256;

	uint8_t		bHeader;
	uint16_t	wAgreedLength;
	uint16_t	wDataLength;
	uint8_t		data[MAX_DATA_LEN];
};

struct LoginKeyAgreementCompleted
{
	uint8_t		bHeader;
	uint8_t		data[3];
};

struct LoginGCAuthSuccess
{
	uint8_t		bHeader;
	uint32_t	dwLoginKey;
	uint8_t		bResult;
};

struct LoginGCLoginFailure
{
	uint8_t		header;
	char		szStatus[ACCOUNT_STATUS_MAX_LEN + 1];
};

#pragma pack()

class LoginAuth;

// Client session for the login/auth role.
//
// Handshake phases (improved packet encryption):
//   server -> HANDSHAKE
//   client -> HANDSHAKE (echo + time delta)
//   server -> KEY_AGREEMENT
//   client -> KEY_AGREEMENT
//   server -> KEY_AGREEMENT_COMPLETED, cipher activates -> AUTH
//
// LOGIN3 handling (account lookup + login key reply) is the next step; it
// lands in HandleLogin().
class LoginSession : public PeerSession
{
public:
	LoginSession(uint32_t handshake, LoginAuth* auth);
	~LoginSession() override = default;

	bool Accept(auriga::net::tcp::socket&& sock);
	bool IsClosed() const { return m_closed; }

	const std::string& GetLogin() const { return m_login; }
	void SetLogin(const std::string& login) { m_login = login; }

	const std::string& GetHost() const { return m_host; }
	void SetHost(const std::string& host) { m_host = host; }

	// Account lookups in flight; the session cannot be reaped while a
	// result callback still references it.
	void BeginDbQuery() { ++m_pendingDbQueries; }
	void EndDbQuery() { --m_pendingDbQueries; }
	bool HasPendingDbWork() const { return m_pendingDbQueries > 0; }

	void SendAuthSuccess(uint32_t loginKey);
	void SendLoginFailure(const char* status);

protected:
	void OnInput() override;
	void OnPeerGone() override;
	void OnOutput() override {}

private:
	enum class Phase
	{
		Handshake,
		Auth,
		Closed,
	};

	void ProcessPackets();
	int  PacketSize(const uint8_t* data, int available) const;
	void HandleHandshake(const char* data, int length);
	void HandleKeyAgreement(const char* data, int length);
	void HandleLogin(const char* data, int length);

	void SendHandshakePacket(uint32_t dwTime, int32_t lDelta);
	void SendKeyAgreement();
	void SendKeyAgreementCompleted();

	bool WritePacket(const void* data, int size);
	void FlushOutput();

	void Close();

	Phase		m_phase { Phase::Handshake };
	bool		m_closed { false };

	uint32_t	m_handshake { 0 };
	uint32_t	m_handshakeSentTime { 0 };
	int		m_handshakeRetry { 0 };

	LoginAuth*	m_auth { nullptr };
	std::string	m_login;
	std::string	m_host;
	int		m_pendingDbQueries { 0 };

	Cipher		m_cipher;
};
