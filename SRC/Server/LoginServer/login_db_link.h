#pragma once

#include <Core/asio_engine.h>
#include <Core/peer_auth.h>
#include <Core/peer_session.h>
#include <Core/utils.h>

#include <common/CommonDefines.h>
#include <common/service.h>

#include "login_session.h"

#include <cstdint>
#include <string>

// Link to the Database server (the game's db_addr/db_port target).
//
// Framing matches the GD protocol: header(1) + handle(4) + size(4) + payload.
// When an INTERSERVER_KEY is configured the link performs the mutual
// challenge-response handshake before it is considered linked, exactly like
// the game server's db_clientdesc.
constexpr int SOCIAL_ID_MAX_LEN = 18;
constexpr int LANGUAGE_MAX_LEN = 2;
constexpr int PREMIUM_MAX_NUM = 9;

#pragma pack(1)

struct LoginDBHeader
{
	uint8_t		bHeader;
	uint32_t	dwHandle;
	uint32_t	dwSize;
};

struct LoginGDAuthLogin
{
	uint32_t	dwID;
#ifdef ENABLE_HWID
	char		hwid[HWID_LENGTH + 1];
#endif
	uint32_t	dwLoginKey;
	char		szLogin[LOGIN_MAX_LEN + 1];
	char		szSocialID[SOCIAL_ID_MAX_LEN + 1];
#ifdef ENABLE_MULTILANGUAGE_SYSTEM
	char		szLanguage[LANGUAGE_MAX_LEN + 1];
#endif
	uint32_t	adwClientKey[4];
#ifdef ENABLE_MULTI_LANGUAGE
	uint8_t		bLanguage;
#endif
	int			iPremiumTimes[PREMIUM_MAX_NUM];
};

#pragma pack()

class LoginDBLink : public PeerSession
{
public:
	LoginDBLink();
	~LoginDBLink() override = default;

	void Configure(const std::string& host, uint16_t port);

	// Server loop: retries the connection every few seconds.
	void Tick();

	bool IsLinked() const { return m_linked; }

	// Registers a completed login in the Database server.
	void SendAuthLogin(const LoginGDAuthLogin& data);

protected:
	void OnInput() override;
	void OnPeerGone() override;

private:
	void StartConnect();
	void ProcessPackets();
	void HandleAuthFrame(uint8_t header, const char* payload, uint32_t size);
	void SendFrame(uint8_t header, const void* payload, uint32_t size);

	std::string	m_host;
	uint16_t	m_port { 0 };
	uint64_t	m_lastTryTime { 0 };
	uint64_t	m_lastPingTime { 0 };
	bool		m_connectPending { false };
	bool		m_awaitingAuth { false };
	bool		m_linked { false };

	uint8_t		m_authNonce[PeerAuth::NONCE_SIZE] {};
	uint64_t	m_authTimestamp { 0 };
};
