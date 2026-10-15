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

bool LoginSession::Accept(auriga::net::tcp::socket&& sock)
{
	AttachSocket(std::move(sock));
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

	char* data = const_cast<char*>(static_cast<const char*>(buffer_read_peek(m_lpInputBuffer)));

	// Only what arrived since the last pass is still ciphertext. Bytes that
	// were already buffered when the cipher activated were sent before the
	// client could encrypt, so they count as plaintext too.
	if (m_cipher.activated() && total > m_plainInput)
		m_cipher.Decrypt(data + m_plainInput, total - m_plainInput);
	m_plainInput = total;

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

			case HEADER_CG_KEY_AGREEMENT:
				HandleKeyAgreement(data + offset, size);
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
	m_plainInput -= offset;
}

int LoginSession::PacketSize(const uint8_t* data, int) const
{
	switch (data[0])
	{
		case HEADER_CG_HANDSHAKE:
			return sizeof(LoginCGHandshake);

		case HEADER_CG_KEY_AGREEMENT:
			return sizeof(LoginKeyAgreement);

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
		SendKeyAgreement();
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

void LoginSession::HandleKeyAgreement(const char* data, int)
{
	if (m_phase != Phase::Handshake)
		return;

	const auto* p = reinterpret_cast<const LoginKeyAgreement*>(data);

	// The completion packet must leave in plaintext before activation.
	SendKeyAgreementCompleted();

	if (!m_cipher.IsKeyPrepared())
	{
		LOG_ERROR("LoginServer: cipher is not prepared");
		Close();
		return;
	}

	if (!m_cipher.Activate(false, p->wAgreedLength, p->data, p->wDataLength))
	{
		LOG_ERROR("LoginServer: key agreement failed (al={}, dl={})", p->wAgreedLength, p->wDataLength);
		Close();
		return;
	}

	m_phase = Phase::Auth;
	LOG_INFO("LoginServer: handshake complete, waiting for LOGIN3");
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

void LoginSession::SendKeyAgreement()
{
	LoginKeyAgreement packet {};
	size_t dataLength = LoginKeyAgreement::MAX_DATA_LEN;
	const size_t agreedLength = m_cipher.Prepare(packet.data, &dataLength);

	if (agreedLength == 0)
	{
		LOG_ERROR("LoginServer: key preparation failed");
		Close();
		return;
	}

	packet.bHeader = HEADER_GC_KEY_AGREEMENT;
	packet.wAgreedLength = static_cast<uint16_t>(agreedLength);
	packet.wDataLength = static_cast<uint16_t>(dataLength);

	if (WritePacket(&packet, sizeof(packet)))
		FlushOutput();
}

void LoginSession::SendKeyAgreementCompleted()
{
	LoginKeyAgreementCompleted packet {};
	packet.bHeader = HEADER_GC_KEY_AGREEMENT_COMPLETED;

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

	char* writePeek = static_cast<char*>(buffer_write_peek(m_lpOutputBuffer));
	buffer_write(m_lpOutputBuffer, data, size);

	if (m_cipher.activated())
		m_cipher.Encrypt(writePeek, size);

	return true;
}

void LoginSession::FlushOutput()
{
	StartWrite();
}
