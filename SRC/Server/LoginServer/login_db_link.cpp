#include <Core/stdafx.h>

#include "login_db_link.h"

#include <Core/Logging.hpp>

#include <boost/asio.hpp>

#include <cstring>
#include <memory>

namespace
{
	// GD/DG protocol headers (common/tables.h).
	constexpr uint8_t GD_AUTH_LOGIN = 100;
	constexpr uint8_t GD_PING = 120;
	constexpr uint8_t GD_AUTH = 122;
	constexpr uint8_t DG_PING = 213;
	constexpr uint8_t DG_AUTH_CHALLENGE = 215;
	constexpr uint8_t DG_AUTH_RESULT = 216;

	constexpr int INPUT_BUFFER_SIZE = 1024 * 1024;
	constexpr int RECONNECT_SECONDS = 3;
	constexpr int PING_INTERVAL_SECONDS = 30;
}

LoginDBLink::LoginDBLink() = default;

void LoginDBLink::Configure(const std::string& host, uint16_t port)
{
	m_host = host;
	m_port = port;

	LOG_INFO("LoginServer: database link target {}:{}", m_host, m_port);
}

void LoginDBLink::Tick()
{
	if (m_linked)
	{
		const uint64_t now = static_cast<uint64_t>(std::time(nullptr));

		if (m_lastPingTime == 0 || now - m_lastPingTime >= PING_INTERVAL_SECONDS)
		{
			m_lastPingTime = now;
			SendFrame(GD_PING, nullptr, 0);
		}

		return;
	}

	if (m_connectPending || m_host.empty() || m_port == 0)
		return;

	if (m_lastTryTime != 0 && static_cast<uint64_t>(std::time(nullptr)) - m_lastTryTime < RECONNECT_SECONDS)
		return;

	StartConnect();
}

void LoginDBLink::StartConnect()
{
	m_lastTryTime = static_cast<uint64_t>(std::time(nullptr));
	m_connectPending = true;

	ResetSession();

	const uint64_t generation = m_generation;
	const std::string host = m_host;
	const uint16_t port = m_port;

	m_pendingAsyncOps.fetch_add(1, std::memory_order_acq_rel);

	auriga::net::PostIo([this, generation, host, port]()
	{
		boost::system::error_code ec;
		m_socket.close(ec);
		m_socket = auriga::net::tcp::socket(auriga::net::Io());

		auto resolver = std::make_shared<auriga::net::tcp::resolver>(auriga::net::Io());

		resolver->async_resolve(host, std::to_string(port),
			[this, generation, resolver](const boost::system::error_code& resolveEc, auriga::net::tcp::resolver::results_type results)
			{
				if (resolveEc)
				{
					QueueGameCompletion(generation,
						[this, resolveEc]()
						{
							m_connectPending = false;
							LOG_ERROR("LoginServer: cannot resolve database link ({})", resolveEc.message().c_str());
						});
					return;
				}

				boost::asio::async_connect(m_socket, results,
					[this, generation, resolver](const boost::system::error_code& connectEc, const auriga::net::tcp::endpoint&)
					{
						QueueGameCompletion(generation,
							[this, connectEc]()
							{
								m_connectPending = false;

								if (connectEc)
								{
									LOG_ERROR("LoginServer: database link connect failed ({})", connectEc.message().c_str());
									return;
								}

								if (!m_lpInputBuffer)
									CreateBuffers(INPUT_BUFFER_SIZE, INPUT_BUFFER_SIZE);

								m_ioConnected = true;

								if (PeerAuth::HasKey())
									m_awaitingAuth = true;
								else
									m_linked = true;

								LOG_INFO("LoginServer: database link connected");
								StartRead();
							});
					});
			});
	});
}

void LoginDBLink::OnInput()
{
	ProcessPackets();
}

void LoginDBLink::OnPeerGone()
{
	m_linked = false;
	m_awaitingAuth = false;
	m_connectPending = false;
	DestroySession();
}

void LoginDBLink::ProcessPackets()
{
	if (!m_lpInputBuffer)
		return;

	const int total = static_cast<int>(buffer_size(m_lpInputBuffer));

	if (total < static_cast<int>(sizeof(LoginDBHeader)))
		return;

	const char* data = static_cast<const char*>(buffer_read_peek(m_lpInputBuffer));
	int offset = 0;

	while (offset + static_cast<int>(sizeof(LoginDBHeader)) <= total)
	{
		LoginDBHeader header {};
		std::memcpy(&header, data + offset, sizeof(header));

		if (header.dwSize > static_cast<uint32_t>(INPUT_BUFFER_SIZE))
		{
			LOG_ERROR("LoginServer: database link oversized frame ({})", header.dwSize);
			OnPeerGone();
			return;
		}

		if (offset + static_cast<int>(sizeof(LoginDBHeader)) + static_cast<int>(header.dwSize) > total)
			break;

		const char* payload = data + offset + sizeof(LoginDBHeader);

		if (m_awaitingAuth)
		{
			// A refused handshake drops the link, which releases the input
			// buffer `data` points into: nothing more may be read from it.
			if (!HandleAuthFrame(header.bHeader, payload, header.dwSize))
				return;
		}
		else if (header.bHeader == DG_PING)
			SendFrame(GD_PING, nullptr, 0);

		offset += static_cast<int>(sizeof(LoginDBHeader)) + static_cast<int>(header.dwSize);
	}

	buffer_read_proceed(m_lpInputBuffer, offset);
}

bool LoginDBLink::HandleAuthFrame(uint8_t header, const char* payload, uint32_t size)
{
	if (header == DG_AUTH_CHALLENGE)
	{
		if (size != static_cast<uint32_t>(PeerAuth::NONCE_SIZE + PeerAuth::TIMESTAMP_SIZE + PeerAuth::MAC_SIZE))
		{
			OnPeerGone();
			return false;
		}

		std::memcpy(m_authNonce, payload, PeerAuth::NONCE_SIZE);
		m_authTimestamp = PeerAuth::DecodeTimestamp(
			reinterpret_cast<const uint8_t*>(payload) + PeerAuth::NONCE_SIZE);

		const uint8_t* serverMac = reinterpret_cast<const uint8_t*>(payload)
			+ PeerAuth::NONCE_SIZE + PeerAuth::TIMESTAMP_SIZE;

		if (!PeerAuth::IsTimestampFresh(m_authTimestamp) ||
			!PeerAuth::VerifyAuthMac(PeerAuth::ROLE_SERVER, m_authNonce, m_authTimestamp, serverMac))
		{
			LOG_ERROR("LoginServer: database auth challenge rejected");
			OnPeerGone();
			return false;
		}

		uint8_t mac[PeerAuth::MAC_SIZE];
		PeerAuth::ComputeAuthMac(PeerAuth::ROLE_CLIENT, m_authNonce, m_authTimestamp, mac);
		SendFrame(GD_AUTH, mac, sizeof(mac));
		return true;
	}

	if (header == DG_AUTH_RESULT)
	{
		if (size < 1 || payload[0] == 0)
		{
			LOG_ERROR("LoginServer: database auth failed");
			OnPeerGone();
			return false;
		}

		m_awaitingAuth = false;
		m_linked = true;
		LOG_INFO("LoginServer: database link authenticated");
		return true;
	}

	return true;
}

void LoginDBLink::SendAuthLogin(const LoginGDAuthLogin& data)
{
	if (!m_linked)
	{
		LOG_ERROR("LoginServer: database link is down, auth login for '{}' not registered", data.szLogin);
		return;
	}

	SendFrame(GD_AUTH_LOGIN, &data, sizeof(data));
}

void LoginDBLink::SendFrame(uint8_t header, const void* payload, uint32_t size)
{
	if (!m_lpOutputBuffer)
		return;

	LoginDBHeader frame {};
	frame.bHeader = header;
	frame.dwHandle = 0;
	frame.dwSize = size;

	buffer_write(m_lpOutputBuffer, &frame, sizeof(frame));

	if (size && payload)
		buffer_write(m_lpOutputBuffer, payload, size);

	StartWrite();
}
