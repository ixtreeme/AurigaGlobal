#include "stdafx.h"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/SessionSystem.hpp"
#include "../ecs/AIHelpers.hpp"
#include <Core/Logging.hpp>
#include "config.h"
#include "utils.h"
#include "desc.h"
#include "desc_client.h"
#include "desc_manager.h"
#include "char_interface.hpp"
#include "../ecs/CharacterAccessors.hpp"
#include "protocol.h"
#include "packet.h"
#include "messenger_manager.h"
#include "sectree_manager.h"
#include "p2p.h"
#include "buffer_manager.h"
#include "guild.h"
#include "guild_manager.h"
#include "locale_service.h"
#include "log.h"
#include "shutdown_manager.h"
#include "../ecs/EventDispatcher.hpp"
#include "../ecs/events.hpp"
#include "Core/Logging.hpp"

extern int max_bytes_written;
extern int current_bytes_written;
extern int total_bytes_written;

DESC::DESC()
	: m_socket(auriga::net::Io())
{
	Initialize();
}

DESC::~DESC()
= default;

void DESC::Initialize()
{
	m_bDestroyed = false;

	m_pInputProcessor = nullptr;
	++m_generation;
	m_readPending = false;
	m_writePending = false;
	m_iPhase = PHASE_CLOSE;
	m_dwHandle = 0;

	m_wPort = 0;
	m_LastTryToConnectTime = 0;

	m_lpInputBuffer = nullptr;
	m_iMinInputBufferLen = 0;

	m_dwHandshake = 0;
	m_dwHandshakeSentTime = 0;
	m_iHandshakeRetry = 0;
	m_dwClientTime = 0;
	m_bHandshaking = false;

	m_lpBufferedOutputBuffer = nullptr;
	m_lpOutputBuffer = nullptr;

	m_pkPingEvent = nullptr;
	m_lpCharacter = entt::null;
	memset( &m_accountTable, 0, sizeof(m_accountTable) );

	memset( &m_SockAddr, 0, sizeof(m_SockAddr) );
	memset( &m_UDPSockAddr, 0, sizeof(m_UDPSockAddr) );

	m_pLogFile = nullptr;

#ifndef _IMPROVED_PACKET_ENCRYPTION_
	m_bEncrypted = false;
#endif

	m_wP2PPort = 0;
	m_bP2PChannel = 0;

	m_bAdminMode = false;
	m_bPong = true;
	m_bChannelStatusRequested = false;

	m_iCurrentSequence = 0;

	m_pkLoginKey = nullptr;
	m_dwLoginKey = 0;
#ifndef _IMPROVED_PACKET_ENCRYPTION_
	memset( m_adwDecryptionKey, 0, sizeof(m_adwDecryptionKey) );
	memset( m_adwEncryptionKey, 0, sizeof(m_adwEncryptionKey) );
#endif

	m_bCRCMagicCubeIdx = 0;
	m_dwProcCRC = 0;
	m_dwFileCRC = 0;
	m_bHackCRCQuery = 0;


	m_outtime = 0;
	m_playtime = 0;
	m_offtime = 0;

	m_pkDisconnectEvent = nullptr;
	m_seq_vector.clear();
}

void DESC::Destroy()
{
	if (m_bDestroyed) {
		return;
	}
	m_bDestroyed = true;

	if (m_pkLoginKey)
		m_pkLoginKey->Expire();

	if (GetAccountTable().id)
		DESC_MANAGER::instance().DisconnectAccount(GetAccountTable().login);

	if (m_pLogFile)
	{
		fclose(m_pLogFile);
		m_pLogFile = nullptr;
	}

	if (m_lpCharacter != entt::null)
	{
		ecs::SessionSystem::Disconnect(m_lpCharacter, "DESC::~DESC");
		m_lpCharacter = entt::null;
	}

	SAFE_BUFFER_DELETE(m_lpOutputBuffer);
	SAFE_BUFFER_DELETE(m_lpInputBuffer);

	event_cancel(&m_pkPingEvent);
	event_cancel(&m_pkDisconnectEvent);

	if (!g_bAuthServer)
	{
		if (m_accountTable.login[0] && m_accountTable.passwd[0])
		{
			TLogoutPacket pack;

			strlcpy(pack.login, m_accountTable.login, sizeof(pack.login));
			strlcpy(pack.passwd, m_accountTable.passwd, sizeof(pack.passwd));

			db_clientdesc->DBPacket(HEADER_GD_LOGOUT, m_dwHandle, &pack, sizeof(TLogoutPacket));
		}

		CShutdownManager::instance().RemoveDesc((LPDESC)this);
	}

	if (m_socket.is_open())
	{
		LOG_INFO("SYSTEM: closing socket. DESC #{}", GetSocket());
		Log("SYSTEM: closing socket. DESC #%d", static_cast<int>(GetSocket()));

#ifdef _IMPROVED_PACKET_ENCRYPTION_
		cipher_.CleanUp();
#endif

		boost::system::error_code ec;
		m_socket.close(ec);
	}

	m_seq_vector.clear();
}

EVENTFUNC(ping_event)
{
	DESC::desc_event_info* info = dynamic_cast<DESC::desc_event_info*>( event->info );

	if ( info == nullptr)
	{
		LOG_ERROR("ping_event> <Factor> Null pointer");
		return 0;
	}

	LPDESC desc = info->desc;

	if (desc->IsAdminMode())
	{
		g_dispatcher.trigger(ecs::EvDescPing { desc->GetHandle() });
		return (ping_event_second_cycle);
	}

	if (!desc->IsPong())
	{
		LOG_INFO("PING_EVENT: no pong {}", desc->GetHostName());

		desc->SetPhase(PHASE_CLOSE);
		g_dispatcher.trigger(ecs::EvDescPing { desc->GetHandle() });

		return (ping_event_second_cycle);
	}
	else
	{
		TPacketGCPing p;
		p.header = HEADER_GC_PING;
		desc->Packet(&p, sizeof(packet_ping));
		desc->SetPong(false);
	}


	desc->SendHandshake(get_dword_time(), 0);
	g_dispatcher.trigger(ecs::EvDescPing { desc->GetHandle() });

	return (ping_event_second_cycle);
}

bool DESC::IsPong()
{
	return m_bPong;
}

void DESC::SetPong(bool b)
{
	m_bPong = b;
}

bool DESC::Setup(auriga::net::tcp::socket&& sock, const sockaddr_in & c_rSockAddr, uint32_t _handle, uint32_t _handshake)
{
	m_socket	= std::move(sock);

	m_stHost		= inet_ntoa(c_rSockAddr.sin_addr);
	m_wPort			= c_rSockAddr.sin_port;
	m_dwHandle		= _handle;

	m_lpOutputBuffer = buffer_new(DEFAULT_PACKET_BUFFER_SIZE * 2);

	m_iMinInputBufferLen = MAX_INPUT_LEN >> 1;
	m_lpInputBuffer = buffer_new(MAX_INPUT_LEN);

	m_SockAddr = c_rSockAddr;

	// Ping Event
	desc_event_info* info = AllocEventInfo<desc_event_info>();

	info->desc = this;
	assert(m_pkPingEvent == NULL);

	m_pkPingEvent = event_create(ping_event, info, ping_event_second_cycle);

#ifndef _IMPROVED_PACKET_ENCRYPTION_
	memcpy(m_adwEncryptionKey, "1234abcd5678efgh", sizeof(uint32_t) * 4);
	memcpy(m_adwDecryptionKey, "1234abcd5678efgh", sizeof(uint32_t) * 4);
#endif // _IMPROVED_PACKET_ENCRYPTION_

	// Set Phase to handshake
	SetPhase(PHASE_HANDSHAKE);
	StartHandshake(_handshake);
	StartRead();

#if defined(__IMPROVED_HANDSHAKE_PROCESS__)
	// Set host handshake time
	DESC_MANAGER::instance().AcceptHandshake(m_stHost.c_str(),
		thecore_pulse() + PASSES_PER_SEC(INTRUSIVE_HANDSHAKE_PULSE));
#endif

	LOG_INFO("SYSTEM: new connection from [{}] fd: {} handshake {} output input_len {}, ptr {}", m_stHost.c_str(), GetSocket(), m_dwHandshake, buffer_size(m_lpInputBuffer), static_cast<const void*>(this));

	Log("SYSTEM: new connection from [%s] fd: %d handshake %u ptr %p", m_stHost.c_str(), static_cast<int>(GetSocket()), m_dwHandshake, this);
	return true;
}

int DESC::ProcessInput()
{
	if (!m_lpInputBuffer)
	{
		LOG_ERROR("DESC::ProcessInput : nil input buffer");
		return -1;
	}

	if (!m_pInputProcessor)
		LOG_ERROR("no input processor");
#ifdef _IMPROVED_PACKET_ENCRYPTION_
	else
	{
		if (cipher_.activated()) {
			cipher_.Decrypt(const_cast<void*>(buffer_read_peek(m_lpInputBuffer)), (size_t)buffer_size(m_lpInputBuffer));
		}

		int iBytesProceed = 0;

		while (!m_pInputProcessor->Process(this, buffer_read_peek(m_lpInputBuffer), (int)buffer_size(m_lpInputBuffer), iBytesProceed))
		{
			buffer_read_proceed(m_lpInputBuffer, iBytesProceed);
			iBytesProceed = 0;
		}

		buffer_read_proceed(m_lpInputBuffer, iBytesProceed);
	}
#else
	else if (!m_bEncrypted)
	{
		int iBytesProceed = 0;

		while (!m_pInputProcessor->Process(this, buffer_read_peek(m_lpInputBuffer), buffer_size(m_lpInputBuffer), iBytesProceed))
		{
			buffer_read_proceed(m_lpInputBuffer, iBytesProceed);
			iBytesProceed = 0;
		}

		buffer_read_proceed(m_lpInputBuffer, iBytesProceed);
	}
	else
	{
		int iSizeBuffer = buffer_size(m_lpInputBuffer);

		if (iSizeBuffer & 7)
			iSizeBuffer -= iSizeBuffer & 7;

		if (iSizeBuffer > 0)
		{
			TEMP_BUFFER	tempbuf;
			LPBUFFER lpBufferDecrypt = tempbuf.getptr();
			buffer_adjust_size(lpBufferDecrypt, iSizeBuffer);

			int iSizeAfter = TEA_Decrypt((uint32_t *) buffer_write_peek(lpBufferDecrypt),
					(uint32_t *) buffer_read_peek(m_lpInputBuffer),
					GetDecryptionKey(),
					iSizeBuffer);

			buffer_write_proceed(lpBufferDecrypt, iSizeAfter);

			int iBytesProceed = 0;

			while (!m_pInputProcessor->Process(this, buffer_read_peek(lpBufferDecrypt), buffer_size(lpBufferDecrypt), iBytesProceed))
			{
				if (iBytesProceed > iSizeBuffer)
				{
					buffer_read_proceed(m_lpInputBuffer, iSizeBuffer);
					iSizeBuffer = 0;
					iBytesProceed = 0;
					break;
				}

				buffer_read_proceed(m_lpInputBuffer, iBytesProceed);
				iSizeBuffer -= iBytesProceed;

				buffer_read_proceed(lpBufferDecrypt, iBytesProceed);
				iBytesProceed = 0;
			}

			buffer_read_proceed(m_lpInputBuffer, iBytesProceed);
		}
	}
#endif // _IMPROVED_PACKET_ENCRYPTION_

	return 0;
}

void DESC::StartRead()
{
	if (m_readPending || m_bDestroyed || m_deletePending || !m_socket.is_open())
		return;

	m_readPending = true;
	++m_pendingAsyncOps;

	const uint64_t generation = m_generation;

	m_socket.async_read_some(
		boost::asio::buffer(m_readChunk),
		[this, generation](const boost::system::error_code & ec, std::size_t bytes)
		{
			--m_pendingAsyncOps;

			if (generation != m_generation || m_bDestroyed || m_deletePending)
				return;

			m_readPending = false;
			OnRead(ec, bytes);
		});
}

void DESC::OnRead(const boost::system::error_code & ec, std::size_t bytes)
{
	if (ec)
	{
		if (ec == boost::asio::error::operation_aborted)
			return;

		SetPhase(PHASE_CLOSE);
		return;
	}

	if (bytes == 0)
	{
		SetPhase(PHASE_CLOSE);
		return;
	}

	if (m_lpInputBuffer)
	{
		buffer_write(m_lpInputBuffer, m_readChunk.data(), static_cast<int>(bytes));
		ProcessInput();
	}

	StartRead();
}

void DESC::StartWrite()
{
	if (m_writePending || m_bDestroyed || m_deletePending || !m_socket.is_open())
		return;

	const int bytes = m_lpOutputBuffer ? static_cast<int>(buffer_size(m_lpOutputBuffer)) : 0;

	if (bytes <= 0)
		return;

	const char * readPoint = static_cast<const char *>(buffer_read_peek(m_lpOutputBuffer));
	m_writeChunk.assign(readPoint, readPoint + bytes);

	m_writePending = true;
	++m_pendingAsyncOps;

	const uint64_t generation = m_generation;

	boost::asio::async_write(
		m_socket,
		boost::asio::buffer(m_writeChunk),
		[this, generation](const boost::system::error_code & ec, std::size_t bytes_transferred)
		{
			--m_pendingAsyncOps;

			if (generation != m_generation || m_bDestroyed || m_deletePending)
				return;

			m_writePending = false;
			OnWrite(ec, bytes_transferred);
		});
}

void DESC::OnWrite(const boost::system::error_code & ec, std::size_t bytes)
{
	if (ec)
	{
		if (ec != boost::asio::error::operation_aborted)
			SetPhase(PHASE_CLOSE);
		return;
	}

	max_bytes_written = MAX(static_cast<int>(bytes), max_bytes_written);

	total_bytes_written += static_cast<int>(bytes);
	current_bytes_written += static_cast<int>(bytes);

	buffer_read_proceed(m_lpOutputBuffer, static_cast<int>(bytes));
	m_writeChunk.clear();

	if (buffer_size(m_lpOutputBuffer) > 0)
		StartWrite();
}

int DESC::ProcessOutput()
{
	StartWrite();
	return 0;
}

void DESC::BufferedPacket(const void * c_pvData, int iSize)
{
	if (m_iPhase == PHASE_CLOSE)
		return;

	if (!m_lpBufferedOutputBuffer)
		m_lpBufferedOutputBuffer = buffer_new(MAX(1024, iSize));

	buffer_write(m_lpBufferedOutputBuffer, c_pvData, iSize);
}

void DESC::Packet(const void * c_pvData, int iSize)
{
	assert(iSize > 0);

	if (m_iPhase == PHASE_CLOSE)
		return;

	if (!m_stRelayName.empty())
	{
		TPacketGGRelay p;

		p.bHeader = HEADER_GG_RELAY;
		strlcpy(p.szName, m_stRelayName.c_str(), sizeof(p.szName));
		p.lSize = iSize;

		if (!packet_encode(m_lpOutputBuffer, &p, sizeof(p)))
		{
			m_iPhase = PHASE_CLOSE;
			return;
		}

		m_stRelayName.clear();

		if (!packet_encode(m_lpOutputBuffer, c_pvData, iSize))
		{
			m_iPhase = PHASE_CLOSE;
			return;
		}
	}
	else
	{
		if (m_lpBufferedOutputBuffer)
		{
			buffer_write(m_lpBufferedOutputBuffer, c_pvData, iSize);

			c_pvData = buffer_read_peek(m_lpBufferedOutputBuffer);
			iSize = buffer_size(m_lpBufferedOutputBuffer);
		}

#ifdef _IMPROVED_PACKET_ENCRYPTION_
		void* buf = buffer_write_peek(m_lpOutputBuffer);


		if (packet_encode(m_lpOutputBuffer, c_pvData, iSize))
		{
			if (cipher_.activated()) {
				cipher_.Encrypt(buf, iSize);
			}
		}
		else
		{
			m_iPhase = PHASE_CLOSE;
		}
#else
		if (!m_bEncrypted)
		{
			if (!packet_encode(m_lpOutputBuffer, c_pvData, iSize))
			{
				m_iPhase = PHASE_CLOSE;
			}
		}
		else
		{
			if (buffer_has_space(m_lpOutputBuffer) < iSize + 8)
			{
#ifndef _IMPROVED_PACKET_ENCRYPTION_
				buffer_adjust_size(m_lpOutputBuffer, iSize);
				if (buffer_has_space(m_lpOutputBuffer) < iSize + 8) {
				LOG_ERROR("desc buffer mem_size overflow. memsize({}) write_pos({}) iSize({})", m_lpOutputBuffer->mem_size, m_lpOutputBuffer->write_point_pos, iSize);
				m_iPhase = PHASE_CLOSE;
				}
#else
				LOG_ERROR("desc buffer mem_size overflow. memsize({}) write_pos({}) iSize({})", m_lpOutputBuffer->mem_size, m_lpOutputBuffer->write_point_pos, iSize);

				m_iPhase = PHASE_CLOSE;
#endif
			}
			else
			{
				/* buffer_adjust_size(m_lpOutputBuffer, iSize + 8); */
				uint32_t * pdwWritePoint = (uint32_t *) buffer_write_peek(m_lpOutputBuffer);

				if (packet_encode(m_lpOutputBuffer, c_pvData, iSize))
				{
					int iSize2 = TEA_Encrypt(pdwWritePoint, pdwWritePoint, GetEncryptionKey(), iSize);

					if (iSize2 > iSize)
						buffer_write_proceed(m_lpOutputBuffer, iSize2 - iSize);
				}
			}
		}
#endif // _IMPROVED_PACKET_ENCRYPTION_

		SAFE_BUFFER_DELETE(m_lpBufferedOutputBuffer);
	}

	//LOG_INFO("{} bytes written (first byte {})", iSize, *(uint8_t *) c_pvData);
	if (m_iPhase != PHASE_CLOSE)
		StartWrite();
}

void DESC::LargePacket(const void * c_pvData, int iSize)
{
	buffer_adjust_size(m_lpOutputBuffer, iSize);
	LOG_INFO("LargePacket Size {}", iSize);

	Packet(c_pvData, iSize);
}

void DESC::SetPhase(int _phase)
{
	m_iPhase = _phase;

	TPacketGCPhase pack;
	pack.header = HEADER_GC_PHASE;
	pack.phase = _phase;
	Packet(&pack, sizeof(TPacketGCPhase));

	switch (m_iPhase)
	{
		case PHASE_CLOSE:
			//MessengerManager::instance().Logout(GetAccountTable().login);
			m_pInputProcessor = &m_inputClose;
			break;

		case PHASE_HANDSHAKE:
			m_pInputProcessor = &m_inputHandshake;
			break;

		case PHASE_SELECT:
		case PHASE_LOGIN:
		case PHASE_LOADING:
#ifndef _IMPROVED_PACKET_ENCRYPTION_
			m_bEncrypted = true;
#endif
			m_pInputProcessor = &m_inputLogin;
			break;

		case PHASE_GAME:
		case PHASE_DEAD:
#ifndef _IMPROVED_PACKET_ENCRYPTION_
			m_bEncrypted = true;
#endif
			m_pInputProcessor = &m_inputMain;
			break;

		case PHASE_AUTH:
#ifndef _IMPROVED_PACKET_ENCRYPTION_
			m_bEncrypted = true;
#endif
			m_pInputProcessor = &m_inputAuth;
			LOG_INFO("AUTH_PHASE {}", static_cast<const void*>(this));
			break;
	}
}

void DESC::BindAccountTable(TAccountTable * pAccountTable)
{
	assert(pAccountTable != NULL);
	memcpy(&m_accountTable, pAccountTable, sizeof(TAccountTable));
	DESC_MANAGER::instance().ConnectAccount(m_accountTable.login, this);
}

void DESC::UDPGrant(const sockaddr_in & c_rSockAddr)
{
	m_UDPSockAddr = c_rSockAddr;

	TPacketGCBindUDP pack;

	pack.header	= HEADER_GC_BINDUDP;
	pack.addr	= m_UDPSockAddr.sin_addr.s_addr;
	pack.port	= m_UDPSockAddr.sin_port;

	Packet(&pack, sizeof(TPacketGCBindUDP));
}

void DESC::Log(const char * format, ...)
{
	if (!m_pLogFile)
		return;

	va_list args;

	time_t ct = get_global_time();
	tm tm = *localtime(&ct);

	fprintf(m_pLogFile,
			"%02d %02d %02d:%02d:%02d | ",
			tm.tm_mon + 1,
			tm.tm_mday,
			tm.tm_hour,
			tm.tm_min,
			tm.tm_sec);

	va_start(args, format);
	vfprintf(m_pLogFile, format, args);
	va_end(args);

	LOG_INFO("");

	fflush(m_pLogFile);
}

void DESC::StartHandshake(uint32_t _handshake)
{
	// Handshake
	m_dwHandshake = _handshake;

	SendHandshake(get_dword_time(), 0);

	m_iHandshakeRetry = 0;
}

void DESC::SendHandshake(uint32_t dwCurTime, int32_t lNewDelta)
{
	TPacketGCHandshake pack;

	pack.bHeader		= HEADER_GC_HANDSHAKE;
	pack.dwHandshake	= m_dwHandshake;
	pack.dwTime			= dwCurTime;
	pack.lDelta			= lNewDelta;

	Packet(&pack, sizeof(TPacketGCHandshake));

	m_dwHandshakeSentTime = dwCurTime;
	m_bHandshaking = true;
}

bool DESC::HandshakeProcess(uint32_t dwTime, int32_t lDelta, bool bInfiniteRetry)
{
	uint32_t dwCurTime = get_dword_time();

	if (lDelta < 0)
	{
		LOG_ERROR("Desc::HandshakeProcess : value error (lDelta {}, ip {})", lDelta, m_stHost.c_str());
		return false;
	}

	int bias = (int) (dwCurTime - (dwTime + lDelta));

	if (bias >= 0 && bias <= 50)
	{
		if (bInfiniteRetry)
		{
			uint8_t bHeader = HEADER_GC_TIME_SYNC;
			Packet(&bHeader, sizeof(uint8_t));
		}

		if (ecs::IsCharacter(GetEntity()))
			LOG_INFO("Handshake: client_time {} server_time {} name: {}", m_dwClientTime, dwCurTime, ecs::PlayerRuntime::GetName(GetEntity()).data());
		else
			LOG_INFO("Handshake: client_time {} server_time {}", m_dwClientTime, dwCurTime);

		m_dwClientTime = dwCurTime;
		m_bHandshaking = false;
		return true;
	}

	int32_t lNewDelta = (int32_t)(dwCurTime - dwTime) / 2;

	if (lNewDelta < 0)
	{
		LOG_INFO("Handshake: lower than zero {}", lNewDelta);
		lNewDelta = (dwCurTime - m_dwHandshakeSentTime) / 2;
	}

	LOG_INFO("Handshake: ServerTime {} dwTime {} lDelta {} SentTime {} lNewDelta {}", dwCurTime, dwTime, lDelta, m_dwHandshakeSentTime, lNewDelta);

	if (!bInfiniteRetry)
		if (++m_iHandshakeRetry > HANDSHAKE_RETRY_LIMIT) {
			LOG_ERROR("handshake retry limit reached! (limit {} character {})", HANDSHAKE_RETRY_LIMIT, ecs::IsCharacter(GetEntity()) ? ecs::PlayerRuntime::GetName(GetEntity()).data() : "!NO CHARACTER!");
			SetPhase(PHASE_CLOSE);
			return false;
		}

	SendHandshake(dwCurTime, lNewDelta);
	return false;
}

bool DESC::IsHandshaking()
{
	return m_bHandshaking;
}

uint32_t DESC::GetClientTime()
{
	return m_dwClientTime;
}

#ifdef _IMPROVED_PACKET_ENCRYPTION_
void DESC::SendKeyAgreement()
{
	TPacketKeyAgreement packet;

	size_t data_length = TPacketKeyAgreement::MAX_DATA_LEN;
	size_t agreed_length = cipher_.Prepare(packet.data, &data_length);
	if (agreed_length == 0) {
		// Initialization failure
		SetPhase(PHASE_CLOSE);
		return;
	}
	assert(data_length <= TPacketKeyAgreement::MAX_DATA_LEN);

	packet.bHeader = HEADER_GC_KEY_AGREEMENT;
	packet.wAgreedLength = (uint16_t)agreed_length;
	packet.wDataLength = (uint16_t)data_length;

	Packet(&packet, sizeof(packet));
}

void DESC::SendKeyAgreementCompleted()
{
	TPacketKeyAgreementCompleted packet;

	packet.bHeader = HEADER_GC_KEY_AGREEMENT_COMPLETED;

	Packet(&packet, sizeof(packet));
}

bool DESC::FinishHandshake(size_t agreed_length, const void* buffer, size_t length)
{
	return cipher_.Activate(false, agreed_length, buffer, length);
}

bool DESC::IsCipherPrepared()
{
	return cipher_.IsKeyPrepared();
}
#endif // #ifdef _IMPROVED_PACKET_ENCRYPTION_

void DESC::SetRelay(const char * c_pszName)
{
	m_stRelayName = c_pszName;
}

void DESC::BindCharacter(entt::entity character)
{
	m_lpCharacter = character;
}
void DESC::FlushOutput()
{
	if (!m_socket.is_open())
		return;

	if (buffer_size(m_lpOutputBuffer) <= 0)
		return;

	LOG_INFO("FLUSH START {}", buffer_size(m_lpOutputBuffer));

	StartWrite();

	const auto start = std::chrono::steady_clock::now();

	while (buffer_size(m_lpOutputBuffer) > 0 && !IsPhase(PHASE_CLOSE))
	{
		auriga::net::RunFor(std::chrono::milliseconds(10));

		const auto seconds_passed = std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::steady_clock::now() - start).count();

		if (seconds_passed > 10)
		{
			SetPhase(PHASE_CLOSE);
			break;
		}
	}

	if (buffer_size(m_lpOutputBuffer) == 0)
		LOG_INFO("FLUSH SUCCESS");
	else
		LOG_INFO("FLUSH FAIL");

	usleep(250000);
}

EVENTFUNC(disconnect_event)
{
	DESC::desc_event_info* info = dynamic_cast<DESC::desc_event_info*>( event->info );

	if ( info == nullptr)
	{
		LOG_ERROR("disconnect_event> <Factor> Null pointer");
		return 0;
	}

	LPDESC d = info->desc;

	d->m_pkDisconnectEvent = nullptr;
	d->SetPhase(PHASE_CLOSE);
	g_dispatcher.trigger(ecs::EvDescDisconnect { d->GetHandle() });
	return 0;
}

bool DESC::DelayedDisconnect(int iSec)
{
	if (m_pkDisconnectEvent != nullptr) {
		return false;
	}

	desc_event_info* info = AllocEventInfo<desc_event_info>();
	info->desc = this;

	m_pkDisconnectEvent = event_create(disconnect_event, info, PASSES_PER_SEC(iSec));
	return true;
}

void DESC::DisconnectOfSameLogin()
{
	if (ecs::IsCharacter(GetEntity())) {
		if (m_pkDisconnectEvent)
			return;

#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(GetEntity(), CHAT_TYPE_INFO, 295, "");
#endif
		DelayedDisconnect(3);
	}
	else {
		SetPhase(PHASE_CLOSE);
	}
}

void DESC::SetAdminMode()
{
	m_bAdminMode = true;
}

bool DESC::IsAdminMode()
{
	return m_bAdminMode;
}

void DESC::SendLoginSuccessPacket()
{
	TAccountTable & rTable = GetAccountTable();

	TPacketGCLoginSuccess p;

	p.bHeader    = HEADER_GC_LOGIN_SUCCESS_NEWSLOT;

	p.handle     = GetHandle();
	p.random_key = DESC_MANAGER::instance().MakeRandomKey(GetHandle()); // FOR MARK
	memcpy(p.players, rTable.players, sizeof(rTable.players));

	for (int i = 0; i < PLAYER_PER_ACCOUNT; ++i)
	{
		CGuild* g = CGuildManager::instance().GetLinkedGuild(rTable.players[i].dwID);

		if (g)
		{
			p.guild_id[i] = g->GetID();
			strlcpy(p.guild_name[i], g->GetName(), sizeof(p.guild_name[i]));
		}
		else
		{
			p.guild_id[i] = 0;
			p.guild_name[i][0] = '\0';
		}
	}

	Packet(&p, sizeof(TPacketGCLoginSuccess));
}

//	TPacketGCStateCheck rp;
//
//	rp.header	= 1;
//	rp.key		= 0;
//	rp.index	= nIndex;
//
//	this->Packet(&rp, sizeof(rp));
//	// LOG_TRACE("STATE_CHECK PACKET PROCESSED.");
//}

void DESC::SetLoginKey(uint32_t dwKey)
{
	m_dwLoginKey = dwKey;
}

void DESC::SetLoginKey(CLoginKey * pkKey)
{
	m_pkLoginKey = pkKey;
	LOG_INFO("SetLoginKey {}", m_pkLoginKey->m_dwKey);
}

uint32_t DESC::GetLoginKey()
{
	if (m_pkLoginKey)
		return m_pkLoginKey->m_dwKey;

	return m_dwLoginKey;
}

const uint8_t* GetKey_20050304Myevan()
{
	static bool bGenerated = false;
	static uint32_t s_adwKey[1938];

	if (!bGenerated)
	{
		bGenerated = true;
		uint32_t seed = 1491971513;

		for (UINT i = 0; i < uint8_t(seed); ++i)
		{
			seed ^= 2148941891ul;
			seed += 3592385981ul;

			s_adwKey[i] = seed;
		}
	}

	return (const uint8_t*)s_adwKey;
}

#ifndef _IMPROVED_PACKET_ENCRYPTION_
void DESC::SetSecurityKey(const uint32_t * c_pdwKey)
{
	const uint8_t * c_pszKey = (const uint8_t *) "JyTxtHljHJlVJHorRM301vf@4fvj10-v";

	c_pszKey = GetKey_20050304Myevan() + 37;

	memcpy(&m_adwDecryptionKey, c_pdwKey, 16);
	TEA_Encrypt(&m_adwEncryptionKey[0], &m_adwDecryptionKey[0], (const uint32_t *) c_pszKey, 16);

	LOG_INFO("SetSecurityKey decrypt {} {} {} {} encrypt {} {} {} {}", m_adwDecryptionKey[0], m_adwDecryptionKey[1], m_adwDecryptionKey[2], m_adwDecryptionKey[3], m_adwEncryptionKey[0], m_adwEncryptionKey[1], m_adwEncryptionKey[2], m_adwEncryptionKey[3]);
}
#endif // _IMPROVED_PACKET_ENCRYPTION_

void DESC::AssembleCRCMagicCube(uint8_t bProcPiece, uint8_t bFilePiece)
{
	static uint8_t abXORTable[32] =
	{
		102,  30, 0, 0, 0, 0, 0, 0,
		188,  44, 0, 0, 0, 0, 0, 0,
		39, 201, 0, 0, 0, 0, 0, 0,
		43,   5, 0, 0, 0, 0, 0, 0,
	};

	bProcPiece = (bProcPiece ^ abXORTable[m_bCRCMagicCubeIdx]);
	bFilePiece = (bFilePiece ^ abXORTable[m_bCRCMagicCubeIdx+1]);

	m_dwProcCRC |= bProcPiece << m_bCRCMagicCubeIdx;
	m_dwFileCRC |= bFilePiece << m_bCRCMagicCubeIdx;

	m_bCRCMagicCubeIdx += 8;

	if (!(m_bCRCMagicCubeIdx & 31))
	{
		m_dwProcCRC = 0;
		m_dwFileCRC = 0;
		m_bCRCMagicCubeIdx = 0;
	}
}

void DESC::push_seq(uint8_t hdr, uint8_t seq)
{
	if (m_seq_vector.size()>=20)
	{
		m_seq_vector.erase(m_seq_vector.begin());
	}

	seq_t info = {.hdr = hdr, .seq = seq };
	m_seq_vector.push_back(info);
}

uint8_t DESC::GetEmpire()
{
	return m_accountTable.bEmpire;
}

void DESC::ChatPacket(uint8_t type, const char * format, ...)
{
	char chatbuf[CHAT_MAX_LEN + 1];
	va_list args;

	va_start(args, format);
	int len = vsnprintf(chatbuf, sizeof(chatbuf), format, args);
	va_end(args);

	packet_chat pack_chat;

	pack_chat.header    = HEADER_GC_CHAT;
	pack_chat.size      = sizeof(packet_chat) + len;
	pack_chat.type      = type;
	pack_chat.id        = 0;
	pack_chat.bEmpire   = GetEmpire();

	TEMP_BUFFER buf;
	buf.write(&pack_chat, sizeof(packet_chat));
	buf.write(chatbuf, len);

	Packet(buf.read_peek(), buf.size());
}

#ifdef TEXTS_IMPROVEMENT
void DESC::ChatPacketNew(uint8_t type, uint32_t idx, const char * format, ...) {
	if (type != CHAT_TYPE_INFO &&
		type != CHAT_TYPE_NOTICE &&
		type != CHAT_TYPE_BIG_NOTICE
#ifdef ENABLE_DICE_SYSTEM
		 && type != CHAT_TYPE_DICE_INFO
#endif
#ifdef ENABLE_NEW_CHAT
		 && type != CHAT_TYPE_INFO_EXP
		 && type != CHAT_TYPE_INFO_ITEM
		 && type != CHAT_TYPE_INFO_VALUE
#endif
	)
		return;

	char chatbuf[256];
	va_list args;
	va_start(args, format);
	int len = vsnprintf(chatbuf, sizeof(chatbuf), format, args);
	va_end(args);

	// vsnprintf returns the length it would have written, which can exceed the
	// buffer. Writing p.size with that value makes the client reject the packet
	// ("cannot recv TPacketGCChatNew size").
	if (len < 0)
		len = 0;
	else if (len >= static_cast<int>(sizeof(chatbuf)))
		len = static_cast<int>(sizeof(chatbuf)) - 1;

	TPacketGCChatNew p;
	p.header = HEADER_GC_CHAT_NEW;
	p.type = type;
	p.idx = idx;
	p.size = sizeof(p) + len;

	TEMP_BUFFER buf;
	buf.write(&p, sizeof(p));
	if (len > 0) {
		buf.write(chatbuf, len);
	}
	Packet(buf.read_peek(), buf.size());
}
#endif



