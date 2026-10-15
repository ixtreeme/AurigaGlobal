#include <Core/stdafx.h>

#include "login_session.h"

#include "login_auth.h"

#include <Core/Logging.hpp>

#include <boost/asio.hpp>

namespace
{
	constexpr int HANDSHAKE_RETRY_LIMIT = 32;
	constexpr int MAX_INPUT_LEN = 300 * 1024;
	constexpr int OUTPUT_BUFFER_SIZE = 150 * 1024;
}

LoginSession::LoginSession(uint32_t handshake, LoginAuth* auth)
	: m_handshake(handshake), m_auth(auth)
{
}

bool LoginSession::Accept(auriga::net::tcp::socket&& sock, auriga::net::TlsServerContext& tls)
{
	// The handshake packet below waits until the TLS stream is up.
	AttachTlsSocket(std::move(sock), tls);
	CreateBuffers(MAX_INPUT_LEN, OUTPUT_BUFFER_SIZE);

	if (!m_lpInputBuffer || !m_lpOutputBuffer)
	{
		DestroySession();
		return false;
	}

	m_handshakeSentTime = get_dword_time();
	SendHandshakePacket(m_handshakeSentTime, 0);
	StartRead();
	return true;
}

void LoginSession::OnInput()
{
	ProcessPackets();
}

void LoginSession::OnPeerGone()
{
	Close();
}

void LoginSession::Close()
{
	if (m_closed)
		return;

	m_closed = true;
	m_phase = Phase::Closed;
	DestroySession();
}

void LoginSession::ProcessPackets()
{
	if (!m_lpInputBuffer)
		return;

	const int total = static_cast<int>(buffer_size(m_lpInputBuffer));

	if (total <= 0)
		return;

	const char* data = static_cast<const char*>(buffer_read_peek(m_lpInputBuffer));

	int offset = 0;

	while (offset < total)
	{
		const uint8_t header = static_cast<uint8_t>(data[offset]);
		const int size = PacketSize(reinterpret_cast<const uint8_t*>(data + offset), total - offset);

		if (size < 0)
		{
			LOG_ERROR("LoginServer: unhandled packet header 0x{:x}", static_cast<int>(header));
			Close();
			return;
		}

		if (size == 0 || offset + size > total)
			break;

		switch (header)
		{
			case HEADER_CG_HANDSHAKE:
				HandleHandshake(data + offset, size);
				break;

			case HEADER_CG_LOGIN3:
				HandleLogin(data + offset, size);
				break;

			default:
				break;
		}

		if (m_closed)
			return;

		offset += size;
	}

	buffer_read_proceed(m_lpInputBuffer, offset);
}

int LoginSession::PacketSize(const uint8_t* data, int) const
{
	switch (data[0])
	{
		case HEADER_CG_HANDSHAKE:
			return sizeof(LoginCGHandshake);

		case HEADER_CG_LOGIN3:
			return sizeof(LoginCGLogin3);

		default:
			return -1;
	}
}

void LoginSession::HandleHandshake(const char* data, int)
{
	if (m_phase != Phase::Handshake)
		return;

	const auto* p = reinterpret_cast<const LoginCGHandshake*>(data);

	if (p->dwHandshake != m_handshake)
	{
		LOG_ERROR("LoginServer: invalid handshake");
		Close();
		return;
	}

	const uint32_t now = get_dword_time();

	if (p->lDelta < 0)
	{
		LOG_ERROR("LoginServer: handshake delta error ({})", p->lDelta);
		Close();
		return;
	}

	const int bias = static_cast<int>(now - (p->dwTime + p->lDelta));

	if (bias >= 0 && bias <= 50)
	{
		// The stream is TLS: the completion tells the client to send LOGIN3.
		SendHandshakeCompleted();
		m_phase = Phase::Auth;
		LOG_INFO("LoginServer: handshake complete, waiting for LOGIN3");
		return;
	}

	int32_t newDelta = static_cast<int32_t>(now - p->dwTime) / 2;

	if (newDelta < 0)
		newDelta = static_cast<int32_t>(now - m_handshakeSentTime) / 2;

	if (++m_handshakeRetry > HANDSHAKE_RETRY_LIMIT)
	{
		LOG_ERROR("LoginServer: handshake retry limit reached");
		Close();
		return;
	}

	m_handshakeSentTime = now;
	SendHandshakePacket(now, newDelta);
}

void LoginSession::HandleLogin(const char* data, int)
{
	if (m_phase != Phase::Auth)
		return;

	if (!m_auth)
	{
		SendLoginFailure("SHUTDOWN");
		return;
	}

	m_auth->RequestLogin(this, reinterpret_cast<const LoginCGLogin3*>(data));
}

void LoginSession::SendAuthSuccess(uint32_t loginKey)
{
	LoginGCAuthSuccess packet {};
	packet.bHeader = HEADER_GC_AUTH_SUCCESS;
	packet.dwLoginKey = loginKey;
	packet.bResult = 1;

	if (WritePacket(&packet, sizeof(packet)))
		FlushOutput();
}

void LoginSession::SendLoginFailure(const char* status)
{
	LoginGCLoginFailure packet {};
	packet.header = HEADER_GC_LOGIN_FAILURE;
	strlcpy(packet.szStatus, status ? status : "", sizeof(packet.szStatus));

	LOG_INFO("LoginServer: login failure '{}'", packet.szStatus);

	if (WritePacket(&packet, sizeof(packet)))
		FlushOutput();
}

void LoginSession::SendHandshakePacket(uint32_t dwTime, int32_t lDelta)
{
	LoginGCHandshake packet {};
	packet.bHeader = HEADER_GC_HANDSHAKE;
	packet.dwHandshake = m_handshake;
	packet.dwTime = dwTime;
	packet.lDelta = lDelta;

	if (WritePacket(&packet, sizeof(packet)))
		FlushOutput();
}

void LoginSession::SendHandshakeCompleted()
{
	LoginHandshakeCompleted packet {};
	packet.bHeader = HEADER_GC_HANDSHAKE_COMPLETED;

	if (WritePacket(&packet, sizeof(packet)))
		FlushOutput();
}

bool LoginSession::WritePacket(const void* data, int size)
{
	if (!m_lpOutputBuffer || size <= 0)
		return false;

	if (buffer_has_space(m_lpOutputBuffer) < size)
	{
		LOG_ERROR("LoginServer: output buffer overflow ({} bytes)", size);
		return false;
	}

	buffer_write(m_lpOutputBuffer, data, size);
	return true;
}

void LoginSession::FlushOutput()
{
	StartWrite();
}
