#include "stdafx.h"
#include <Core/Logging.hpp>
#include <Core/peer_auth.h>
#include "config.h"
#include "utils.h"
#include "desc_client.h"
#include "desc_manager.h"
#include "char_interface.hpp"
#include "protocol.h"
#include "packet.h"
#include "p2p.h"
#include "buffer_manager.h"
#include "guild_manager.h"
#include "db.h"

#include "party.h"
#include <common/CommonDefines.h>

LPCLIENT_DESC db_clientdesc = nullptr;
LPCLIENT_DESC g_NetmarbleDBDesc = nullptr;


static const char* GetKnownClientDescName(LPCLIENT_DESC desc) {
	if (desc == db_clientdesc) {
		return "db_clientdesc";
	} else if (desc == g_NetmarbleDBDesc) {
		return "g_NetmarbleDBDesc";
	} 
	return "unknown";
}

CLIENT_DESC::CLIENT_DESC()
{
	m_iPhaseWhenSucceed = 0;
	m_bRetryWhenClosed = false;
	m_connectPending = false;
	m_bAwaitingAuth = false;
	m_bAuthResponseSent = false;
	m_LastTryToConnectTime = 0;
	m_tLastChannelStatusUpdateTime = 0;
}

CLIENT_DESC::~CLIENT_DESC()
{
}

void CLIENT_DESC::Destroy()
{
	P2P_MANAGER::instance().UnregisterConnector(this);

	if (this == db_clientdesc)
	{
		CPartyManager::instance().DeleteAllParty();
		CPartyManager::instance().DisablePCParty();
		CGuildManager::instance().StopAllGuildWar();
	}

	LOG_INFO("SYSTEM: closing client socket. DESC #{}", GetSocket());

	// Chain up to base class Destroy()
	DESC::Destroy();
}

void CLIENT_DESC::SetRetryWhenClosed(bool b)
{
	m_bRetryWhenClosed = b;
}

bool CLIENT_DESC::Connect(int iPhaseWhenSucceed)
{
	if (iPhaseWhenSucceed != 0)
		m_iPhaseWhenSucceed = iPhaseWhenSucceed;

	if (get_global_time() - m_LastTryToConnectTime < 3)
		return false;

	m_LastTryToConnectTime = get_global_time();

	if (IsConnected() || m_connectPending)
		return false;

	StartConnect();
	return true;
}

void CLIENT_DESC::StartConnect()
{
	LOG_INFO("SYSTEM: Trying to connect to {}:{}", m_stHost.c_str(), m_wPort);

	SetPhase(PHASE_CLIENT_CONNECTING);

	m_connectPending = true;

	// One token covers the whole resolve -> connect chain; the game
	// handoff consumes it exactly once on every path.
	const uint64_t generation = m_generation;
	const std::string host = m_stHost;
	const uint16_t port = m_wPort;

	m_pendingAsyncOps.fetch_add(1, std::memory_order_acq_rel);

	auriga::net::PostIo([this, generation, host, port]()
	{
		// Socket setup happens on the io thread: it may race with the
		// submission of the previous (aborted) session operations.
		boost::system::error_code ec;
		m_socket.close(ec);
		m_socket = auriga::net::tcp::socket(auriga::net::Io());

		auto resolver = std::make_shared<auriga::net::tcp::resolver>(auriga::net::Io());

		resolver->async_resolve(host, std::to_string(port),
			[this, generation, resolver](const boost::system::error_code & resolve_ec, auriga::net::tcp::resolver::results_type results)
			{
				if (resolve_ec)
				{
					QueueGameCompletion(generation,
						[this, resolve_ec]()
						{
							m_connectPending = false;
							LOG_ERROR("SYSTEM: could not resolve {}:{} ({})", m_stHost.c_str(), m_wPort, resolve_ec.message().c_str());
							SetPhase(PHASE_CLIENT_CONNECTING);
						});
					return;
				}

				boost::asio::async_connect(m_socket, results,
					[this, generation, resolver](const boost::system::error_code & connect_ec, const auriga::net::tcp::endpoint &)
					{
						// The native handle is read on the io thread and
						// cached for the game-thread logging/status paths.
						socket_t connectedHandle = static_cast<socket_t>(INVALID_SOCKET);

						if (!connect_ec)
							connectedHandle = static_cast<socket_t>(m_socket.native_handle());

						QueueGameCompletion(generation,
							[this, connect_ec, connectedHandle]()
							{
								m_connectPending = false;

								if (connect_ec)
								{
									CloseSocket();
									LOG_ERROR("SYSTEM: connection to {}:{} failed ({})", m_stHost.c_str(), m_wPort, connect_ec.message().c_str());
									SetPhase(PHASE_CLIENT_CONNECTING);
									return;
								}

								LOG_INFO("SYSTEM: connected to server ({}:{} ptr {})", m_stHost.c_str(), m_wPort, static_cast<const void*>(this));
								m_ioConnected = true;
								m_socketHandle = connectedHandle;
								StartRead();

								if (PeerAuth::HasKey())
								{
									// Wait for the server-side challenge before switching
									// to the operational phase.
									m_bAwaitingAuth = true;
									m_bAuthResponseSent = false;
									SetHeartbeat(0, 10);
								}
								else
								{
									SetPhase(m_iPhaseWhenSucceed);
									SetHeartbeat(30, 90);
								}
							});
					});
			});
	});
}

void CLIENT_DESC::Setup(const char * _host, uint16_t _port)
{
	// 1MB input/output buffer
	m_stHost = _host;
	m_wPort = _port;

	InitializeBuffers();
}

void CLIENT_DESC::SetPhase(int iPhase)
{
	switch (iPhase)
	{
		case PHASE_CLIENT_CONNECTING:
			LOG_INFO("PHASE_CLIENT_DESC::CONNECTING");
			m_pInputProcessor = nullptr;
			break;

		case PHASE_DBCLIENT:
			{
				LOG_INFO("PHASE_DBCLIENT");

				{
					static bool bSentBoot = false;

					if (!bSentBoot)
					{
						bSentBoot = true;
						TPacketGDBoot p;
						p.dwItemIDRange[0] = 0;
						p.dwItemIDRange[1] = 0;
						memcpy(p.szIP, g_szPublicIP, 16);
						DBPacket(HEADER_GD_BOOT, 0, &p, sizeof(p));
					}
				}

				TEMP_BUFFER buf;

				TPacketGDSetup p;

				memset(&p, 0, sizeof(p));
				strlcpy(p.szPublicIP, g_szPublicIP, sizeof(p.szPublicIP));

				{
					p.bChannel	= g_bChannel;
					p.wListenPort = mother_port;
					p.wP2PPort	= p2p_port;
					p.bAuthServer = false;
					map_allow_copy(p.alMaps, MAP_ALLOW_LIMIT);

					const DESC_MANAGER::DESC_SET & c_set = DESC_MANAGER::instance().GetClientSet();
					DESC_MANAGER::DESC_SET::const_iterator it;

					for (it = c_set.begin(); it != c_set.end(); ++it)
					{
						LPDESC d = *it;

						if (d->GetAccountTable().id != 0)
							++p.dwLoginCount;
					}

					buf.write(&p, sizeof(p));

					if (p.dwLoginCount)
					{
						TPacketLoginOnSetup pck;

						for (it = c_set.begin(); it != c_set.end(); ++it)
						{
							LPDESC d = *it;

							TAccountTable & r = d->GetAccountTable();

							if (r.id != 0)
							{
#ifdef ENABLE_HWID
								strlcpy(pck.hwid, r.hwid, sizeof(pck.hwid));
#endif
								pck.dwID = r.id;
								strlcpy(pck.szLogin, r.login, sizeof(pck.szLogin));
								strlcpy(pck.szSocialID, r.social_id, sizeof(pck.szSocialID));
								strlcpy(pck.szHost, d->GetHostName(), sizeof(pck.szHost));
								pck.dwLoginKey = d->GetLoginKey();
#ifdef ENABLE_MULTI_LANGUAGE
								pck.bLanguage = r.bLanguage;
#endif
#ifndef _IMPROVED_PACKET_ENCRYPTION_
								memcpy(pck.adwClientKey, d->GetDecryptionKey(), 16);
#endif

								buf.write(&pck, sizeof(TPacketLoginOnSetup));
							}
						}
					}

					LOG_INFO("DB_SETUP current user {} size {}", p.dwLoginCount, buf.size());

					CPartyManager::instance().EnablePCParty();
					//CPartyManager::instance().SendPartyToDB();
				}
				

				DBPacket(HEADER_GD_SETUP, 0, buf.read_peek(), buf.size());
				m_pInputProcessor = &m_inputDB;
			}
			break;

		case PHASE_P2P:
			LOG_INFO("PHASE_P2P");

			if (m_lpInputBuffer)
				buffer_reset(m_lpInputBuffer);

			if (m_lpOutputBuffer)
				buffer_reset(m_lpOutputBuffer);

			m_pInputProcessor = &m_inputP2P;

			// The early RegisterConnector Boot was dropped while the socket
			// was still connecting; send the initial peer state now.
			P2P_MANAGER::instance().OnConnectorReady(this);
			break;

		case PHASE_CLOSE:
			m_pInputProcessor = nullptr;
			break;

	}

	m_iPhase = iPhase;
}

void CLIENT_DESC::DBPacketHeader(uint8_t bHeader, uint32_t dwHandle, uint32_t dwSize)
{
	buffer_write(m_lpOutputBuffer, encode_byte(bHeader), sizeof(uint8_t));
	buffer_write(m_lpOutputBuffer, encode_4bytes(dwHandle), sizeof(uint32_t));
	buffer_write(m_lpOutputBuffer, encode_4bytes(dwSize), sizeof(uint32_t));
}

void CLIENT_DESC::DBPacket(uint8_t bHeader, uint32_t dwHandle, const void * c_pvData, uint32_t dwSize)
{
	if (!IsConnected()) {
		LOG_INFO("CLIENT_DESC [{}] trying DBPacket() while not connected", GetKnownClientDescName(this));
		return;
	}
	LOG_TRACE("DB_PACKET: header {} handle {} size {} buffer_size {}", static_cast<int>(bHeader), dwHandle, dwSize, buffer_size(m_lpOutputBuffer));
	DBPacketHeader(bHeader, dwHandle, dwSize);

	if (c_pvData)
		buffer_write(m_lpOutputBuffer, c_pvData, dwSize);

	ProcessOutput();
}

void CLIENT_DESC::Packet(const void * c_pvData, int iSize)
{
	if (!IsConnected()) {
		LOG_INFO("CLIENT_DESC [{}] trying Packet() while not connected", GetKnownClientDescName(this));
		return;
	}
	buffer_write(m_lpOutputBuffer, c_pvData, iSize);
	ProcessOutput();
}

bool CLIENT_DESC::IsRetryWhenClosed()
{
	return (0 == thecore_is_shutdowned() && m_bRetryWhenClosed);
}

void CLIENT_DESC::Update(uint32_t t)
{
	{
		UpdateChannelStatus(t, false);
	}
}

void CLIENT_DESC::UpdateChannelStatus(uint32_t t, bool fForce)
{
	enum {
		CHANNELSTATUS_UPDATE_PERIOD = 5*60*1000,
	};
	uint32_t tLCSUP = m_tLastChannelStatusUpdateTime+CHANNELSTATUS_UPDATE_PERIOD;
	if (fForce || tLCSUP < t) {
		int iTotal;
		int * paiEmpireUserCount;
		int iLocal;
		DESC_MANAGER::instance().GetUserCount(iTotal, &paiEmpireUserCount, iLocal);

		TChannelStatus channelStatus;
		channelStatus.nPort = mother_port;

		if (g_bNoMoreClient) channelStatus.bStatus = 0;
		else channelStatus.bStatus = iTotal > g_iFullUserCount ? 3 : iTotal > g_iBusyUserCount ? 2 : 1;

		DBPacket(HEADER_GD_UPDATE_CHANNELSTATUS, 0, &channelStatus, sizeof(channelStatus));
		m_tLastChannelStatusUpdateTime = t;
	}
}

void CLIENT_DESC::Reset()
{
	// Backup connection target info
	std::string host = m_stHost;
	uint16_t port = m_wPort;

	Destroy();
	Initialize();

	// Restore connection target info
	m_stHost = host;
	m_wPort = port;

	m_connectPending = false;
	m_bAwaitingAuth = false;
	m_bAuthResponseSent = false;

	InitializeBuffers();
}

void CLIENT_DESC::SendAuthResponse(const uint8_t nonce[16], uint64_t timestamp)
{
	uint8_t mac[PeerAuth::MAC_SIZE];
	PeerAuth::ComputeAuthMac(PeerAuth::ROLE_CLIENT, nonce, timestamp, mac);

	if (m_iPhaseWhenSucceed == PHASE_P2P)
	{
		TPacketGGAuth p;
		p.bHeader = HEADER_GG_AUTH;
		memcpy(p.mac, mac, sizeof(p.mac));
		Packet(&p, sizeof(p));
	}
	else
	{
		DBPacket(HEADER_GD_AUTH, 0, mac, sizeof(mac));
	}
}

void CLIENT_DESC::HandleAuthInput()
{
	if (m_iPhaseWhenSucceed == PHASE_P2P)
	{
		while (true)
		{
			const uint32_t available = buffer_size(m_lpInputBuffer);

			if (!m_bAuthResponseSent)
			{
				if (available < sizeof(TPacketGGAuthChallenge))
					return;

				TPacketGGAuthChallenge challenge;
				memcpy(&challenge, buffer_read_peek(m_lpInputBuffer), sizeof(challenge));
				buffer_read_proceed(m_lpInputBuffer, sizeof(challenge));

				if (challenge.bHeader != HEADER_GG_AUTH_CHALLENGE)
				{
					LOG_ERROR("CLIENT_DESC: unexpected packet during P2P auth from {} ({})", m_stHost.c_str(), challenge.bHeader);
					SetPhase(PHASE_CLOSE);
					return;
				}

				const uint64_t timestamp = PeerAuth::DecodeTimestamp(challenge.timestamp);

				if (!PeerAuth::IsTimestampFresh(timestamp) ||
					!PeerAuth::VerifyAuthMac(PeerAuth::ROLE_SERVER, challenge.nonce, timestamp, challenge.mac))
				{
					LOG_ERROR("CLIENT_DESC: P2P auth challenge rejected ({})", m_stHost.c_str());
					SetPhase(PHASE_CLOSE);
					return;
				}

				SendAuthResponse(challenge.nonce, timestamp);
				m_bAuthResponseSent = true;
				continue;
			}

			if (available < sizeof(TPacketGGAuthResult))
				return;

			TPacketGGAuthResult result;
			memcpy(&result, buffer_read_peek(m_lpInputBuffer), sizeof(result));
			buffer_read_proceed(m_lpInputBuffer, sizeof(result));

			if (result.bHeader != HEADER_GG_AUTH_RESULT)
			{
				LOG_ERROR("CLIENT_DESC: unexpected packet during P2P auth from {} ({})", m_stHost.c_str(), result.bHeader);
				SetPhase(PHASE_CLOSE);
				return;
			}

			m_bAwaitingAuth = false;

			if (result.bResult)
			{
				LOG_INFO("SYSTEM: P2P auth OK ({})", m_stHost.c_str());
				SetPhase(m_iPhaseWhenSucceed);
				SetHeartbeat(30, 90);
			}
			else
			{
				LOG_ERROR("SYSTEM: P2P auth failed ({})", m_stHost.c_str());
				SetPhase(PHASE_CLOSE);
			}

			return;
		}
	}

	// Database link: 9 byte framed challenge/result.
	while (true)
	{
		const uint32_t available = buffer_size(m_lpInputBuffer);

		if (available < 9)
			return;

		const uint8_t * headerData = static_cast<const uint8_t *>(buffer_read_peek(m_lpInputBuffer));
		uint8_t header;
		uint32_t size;
		memcpy(&header, headerData, sizeof(header));
		memcpy(&size, headerData + 5, sizeof(size));

		if (available < 9 + size)
			return;

		const uint8_t * payload = headerData + 9;

		if (!m_bAuthResponseSent)
		{
			if (header != HEADER_DG_AUTH_CHALLENGE ||
				size != static_cast<uint32_t>(PeerAuth::NONCE_SIZE + PeerAuth::TIMESTAMP_SIZE + PeerAuth::MAC_SIZE))
			{
				LOG_ERROR("CLIENT_DESC: unexpected packet during DB auth ({})", static_cast<int>(header));
				SetPhase(PHASE_CLOSE);
				return;
			}

			uint8_t nonce[PeerAuth::NONCE_SIZE];
			memcpy(nonce, payload, sizeof(nonce));
			const uint64_t timestamp = PeerAuth::DecodeTimestamp(payload + PeerAuth::NONCE_SIZE);
			const uint8_t * serverMac = payload + PeerAuth::NONCE_SIZE + PeerAuth::TIMESTAMP_SIZE;

			if (!PeerAuth::IsTimestampFresh(timestamp) ||
				!PeerAuth::VerifyAuthMac(PeerAuth::ROLE_SERVER, nonce, timestamp, serverMac))
			{
				LOG_ERROR("CLIENT_DESC: DB auth challenge rejected");
				SetPhase(PHASE_CLOSE);
				return;
			}

			buffer_read_proceed(m_lpInputBuffer, 9 + size);
			SendAuthResponse(nonce, timestamp);
			m_bAuthResponseSent = true;
			continue;
		}

		if (header != HEADER_DG_AUTH_RESULT || size < 1)
		{
			LOG_ERROR("CLIENT_DESC: unexpected packet during DB auth ({})", static_cast<int>(header));
			SetPhase(PHASE_CLOSE);
			return;
		}

		const uint8_t result = payload[0];
		buffer_read_proceed(m_lpInputBuffer, 9 + size);
		m_bAwaitingAuth = false;

		if (result)
		{
			LOG_INFO("SYSTEM: DB auth OK");
			SetPhase(m_iPhaseWhenSucceed);
			SetHeartbeat(30, 90);
		}
		else
		{
			LOG_ERROR("SYSTEM: DB auth failed");
			SetPhase(PHASE_CLOSE);
		}

		return;
	}
}

void CLIENT_DESC::OnInput()
{
	if (m_bAwaitingAuth)
	{
		HandleAuthInput();
		return;
	}

	DESC::OnInput();
}

void CLIENT_DESC::OnHeartbeatSend()
{
	if (m_bAwaitingAuth || !IsConnected())
		return;

	if (m_iPhaseWhenSucceed == PHASE_P2P && IsPhase(PHASE_P2P))
	{
		TPacketGGPing p;
		p.bHeader = HEADER_GG_PING;
		Packet(&p, sizeof(p));
	}
	else if (IsPhase(PHASE_DBCLIENT))
	{
		DBPacket(HEADER_GD_PING, 0, nullptr, 0);
	}
}

void CLIENT_DESC::InitializeBuffers()
{
	CreateBuffers(1024 * 1024, 1024 * 1024);
	m_iMinInputBufferLen = 1024 * 1024;
}
