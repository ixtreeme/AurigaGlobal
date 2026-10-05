#include "stdafx.h"
#include <Core/Logging.hpp>
#include <Core/peer_auth.h>
#include "desc_p2p.h"
#include "protocol.h"
#include "packet.h"
#include "p2p.h"
#include <common/CommonDefines.h>

DESC_P2P::~DESC_P2P()
{
}

void DESC_P2P::Destroy()
{
	P2P_MANAGER::instance().UnregisterAcceptor(this);

	LOG_INFO("SYSTEM: closing p2p socket. DESC #{}", GetSocket());

	// Chain up to base class Destroy()
	DESC::Destroy();
}

#ifdef ENABLE_PORT_SECURITY
#include "config.h"
#endif
bool DESC_P2P::Setup(auriga::net::tcp::socket&& sock, const char * host, uint16_t wPort)
{
	AttachSocket(std::move(sock));
	m_stHost = host;
	m_wPort = wPort;

	CreateBuffers(1024 * 1024, 1024 * 1024);

	if (!m_lpInputBuffer || !m_lpOutputBuffer)
		return false;

	m_iMinInputBufferLen = 1024 * 1024;
#ifdef ENABLE_PORT_SECURITY
	if (strcmp(host, g_szPublicIP)) // refuse if remote host != public ip (only the same machine must be able to connect in here)
	{
		LOG_INFO("SYSTEM: new p2p connection from [{}] to [{}] fd: {} BLOCKED", host, g_szPublicIP, GetSocket());
		SetPhase(PHASE_CLOSE);
		return true;
	}
#endif
	SetPhase(PHASE_P2P);

	if (PeerAuth::HasKey())
	{
		m_bAwaitingAuth = true;
		SetHeartbeat(0, 10);
		SendAuthChallenge();
	}
	else
	{
		SetHeartbeat(30, 90);
	}

	StartRead();

	LOG_INFO("SYSTEM: new p2p connection from [{}] fd: {}", host, GetSocket());
	return true;
}

void DESC_P2P::OnInput()
{
	if (m_bAwaitingAuth)
	{
		HandleAuthInput();
		return;
	}

	DESC::OnInput();
}

void DESC_P2P::SendAuthChallenge()
{
	TPacketGGAuthChallenge challenge;
	challenge.bHeader = HEADER_GG_AUTH_CHALLENGE;

	PeerAuth::MakeNonce(challenge.nonce);
	memcpy(m_authNonce, challenge.nonce, sizeof(m_authNonce));

	m_authTimestamp = PeerAuth::NowSeconds();
	PeerAuth::EncodeTimestamp(m_authTimestamp, challenge.timestamp);
	PeerAuth::ComputeAuthMac(PeerAuth::ROLE_SERVER, challenge.nonce, m_authTimestamp, challenge.mac);

	Packet(&challenge, sizeof(challenge));
}

void DESC_P2P::HandleAuthInput()
{
	if (buffer_size(m_lpInputBuffer) < sizeof(TPacketGGAuth))
		return;

	TPacketGGAuth response;
	memcpy(&response, buffer_read_peek(m_lpInputBuffer), sizeof(response));
	buffer_read_proceed(m_lpInputBuffer, sizeof(response));

	if (response.bHeader != HEADER_GG_AUTH)
	{
		LOG_ERROR("DESC_P2P: expected auth response from {} ({})", m_stHost.c_str(), response.bHeader);
		SetPhase(PHASE_CLOSE);
		return;
	}

	const bool ok = PeerAuth::IsTimestampFresh(m_authTimestamp) &&
		PeerAuth::VerifyAuthMac(PeerAuth::ROLE_CLIENT, m_authNonce, m_authTimestamp, response.mac);

	// The challenge is single use.
	memset(m_authNonce, 0, sizeof(m_authNonce));
	m_authTimestamp = 0;

	TPacketGGAuthResult result;
	result.bHeader = HEADER_GG_AUTH_RESULT;
	result.bResult = ok ? 1 : 0;
	Packet(&result, sizeof(result));

	if (!ok)
	{
		LOG_ERROR("DESC_P2P: auth failed from {}", m_stHost.c_str());
		SetPhase(PHASE_CLOSE);
		return;
	}

	m_bAwaitingAuth = false;
	SetHeartbeat(30, 90);
	LOG_INFO("DESC_P2P: auth OK from {}", m_stHost.c_str());

	// The peer could not be announced before the handshake, because the
	// initial Boot would have raced with the auth response.
	P2P_MANAGER::instance().RegisterAcceptor(this);
}

void DESC_P2P::OnHeartbeatSend()
{
	if (m_bAwaitingAuth)
		return;

	TPacketGGPing p;
	p.bHeader = HEADER_GG_PING;
	Packet(&p, sizeof(p));
}

void DESC_P2P::SetPhase(int iPhase)
{
	static CInputP2P s_inputP2P;

	switch (iPhase)
	{
		case PHASE_P2P:
			LOG_INFO("PHASE_P2P");

			if (m_lpInputBuffer)
				buffer_reset(m_lpInputBuffer);

			if (m_lpOutputBuffer)
				buffer_reset(m_lpOutputBuffer);

			m_pInputProcessor = &s_inputP2P;
			break;

		case PHASE_CLOSE:
			m_pInputProcessor = nullptr;
			break;

		default:
			LOG_ERROR("DESC_P2P::SetPhase : Unknown phase");
			break;
	}

	m_iPhase = iPhase;
}

