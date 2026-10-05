#include "stdafx.h"
#include <Core/Logging.hpp>
#include "config.h"
#include "utils.h"
#include "desc_client.h"
#include "desc_manager.h"
#include "char_interface.hpp"
#include "protocol.h"
#include "p2p.h"
#include "buffer_manager.h"
#include "guild_manager.h"
#include "db.h"

#include "party.h"
#include <common/CommonDefines.h>

LPCLIENT_DESC db_clientdesc = nullptr;
LPCLIENT_DESC g_pkAuthMasterDesc = nullptr;
LPCLIENT_DESC g_NetmarbleDBDesc = nullptr;


static const char* GetKnownClientDescName(LPCLIENT_DESC desc) {
	if (desc == db_clientdesc) {
		return "db_clientdesc";
	} else if (desc == g_pkAuthMasterDesc) {
		return "g_pkAuthMasterDesc";
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

	boost::system::error_code ec;
	m_socket.close(ec);
	m_socket = auriga::net::tcp::socket(auriga::net::Io());

	SetPhase(PHASE_CLIENT_CONNECTING);

	m_connectPending = true;
	++m_pendingAsyncOps;

	const uint64_t generation = m_generation;
	const std::string host = m_stHost;

	auto resolver = std::make_shared<auriga::net::tcp::resolver>(auriga::net::Io());

	resolver->async_resolve(host, std::to_string(m_wPort),
		[this, generation, resolver](const boost::system::error_code & resolve_ec, auriga::net::tcp::resolver::results_type results)
		{
			--m_pendingAsyncOps;

			if (generation != m_generation || m_bDestroyed || m_deletePending)
				return;

			if (resolve_ec)
			{
				m_connectPending = false;
				LOG_ERROR("SYSTEM: could not resolve {}:{} ({})", m_stHost.c_str(), m_wPort, resolve_ec.message().c_str());
				SetPhase(PHASE_CLIENT_CONNECTING);
				return;
			}

			++m_pendingAsyncOps;

			boost::asio::async_connect(m_socket, results,
				[this, generation, resolver](const boost::system::error_code & connect_ec, const auriga::net::tcp::endpoint &)
				{
					--m_pendingAsyncOps;

					if (generation != m_generation || m_bDestroyed || m_deletePending)
						return;

					m_connectPending = false;

					if (connect_ec)
					{
						boost::system::error_code ignored;
						m_socket.close(ignored);
						LOG_ERROR("SYSTEM: connection to {}:{} failed ({})", m_stHost.c_str(), m_wPort, connect_ec.message().c_str());
						SetPhase(PHASE_CLIENT_CONNECTING);
						return;
					}

					LOG_INFO("SYSTEM: connected to server ({}:{} ptr {})", m_stHost.c_str(), m_wPort, static_cast<const void*>(this));
					SetPhase(m_iPhaseWhenSucceed);
					StartRead();
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

				if (!g_bAuthServer)
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

				if (!g_bAuthServer)
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
				else
				{
					p.bAuthServer = true;
					buf.write(&p, sizeof(p));
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
	if (!m_socket.is_open()) {
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
	if (!m_socket.is_open()) {
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
	if (!g_bAuthServer) {
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

	InitializeBuffers();
}

void CLIENT_DESC::InitializeBuffers()
{
	m_lpOutputBuffer = buffer_new(1024 * 1024);
	m_lpInputBuffer = buffer_new(1024 * 1024);
	m_iMinInputBufferLen = 1024 * 1024;
}
