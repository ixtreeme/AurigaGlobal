#include "stdafx.h"
#include <Core/Logging.hpp>
#include "desc_p2p.h"
#include "protocol.h"
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
	m_socket = std::move(sock);
	m_stHost = host;
	m_wPort = wPort;

	if (!(m_lpOutputBuffer = buffer_new(1024 * 1024)))
		return false;

	if (!(m_lpInputBuffer = buffer_new(1024 * 1024)))
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

	StartRead();

	LOG_INFO("SYSTEM: new p2p connection from [{}] fd: {}", host, GetSocket());
	return true;
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

