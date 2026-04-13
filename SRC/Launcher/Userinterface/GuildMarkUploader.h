// ===========================================================================
// [CLIENT] GuildMarkUploader.h
// Guild Mark & Symbol Upload - Client Side
// Connects to mark server, uploads mark image or guild symbol
// ===========================================================================
#pragma once

#include "../Render/NetStream.h"
#include "MarkImage.h"

#include <vector>
#include <cstdint>

class CGuildMarkUploader : public CNetworkStream, public CSingleton<CGuildMarkUploader>
{
public:
	enum EError
	{
		ERROR_NONE,
		ERROR_CONNECT,
		ERROR_LOAD,
		ERROR_WIDTH,
		ERROR_HEIGHT,
	};

	enum ESendType
	{
		SEND_TYPE_MARK,
		SEND_TYPE_SYMBOL,
	};

	CGuildMarkUploader();
	virtual ~CGuildMarkUploader();

	void Disconnect();
	bool Connect(const CNetworkAddress& c_rkNetAddr, uint32_t dwHandle, uint32_t dwRandomKey,
	             uint32_t dwGuildID, const char* c_szFileName, UINT* peError);
	bool ConnectToSendSymbol(const CNetworkAddress& c_rkNetAddr, uint32_t dwHandle, uint32_t dwRandomKey,
	                         uint32_t dwGuildID, const char* c_szFileName, UINT* peError);
	bool IsCompleteUploading();

	void Process();

private:
	enum EState
	{
		STATE_OFFLINE,
		STATE_LOGIN,
		STATE_COMPLETE,
	};

	// [CLIENT] Network callbacks
	void OnConnectFailure();
	void OnConnectSuccess();
	void OnRemoteDisconnect();
	void OnDisconnect();

	// [CLIENT] Image loading
	bool __LoadMark(const char* c_szFileName, UINT* peError);
	bool __LoadSymbol(const char* c_szFileName, UINT* peError);

	// [CLIENT] State machine
	void __Initialize();
	bool __StateProcess();

	void __OfflineState_Set();
	void __CompleteState_Set();

	void __LoginState_Set();
	bool __LoginState_Process();
	bool __LoginState_RecvPhase();
	bool __LoginState_RecvHandshake();
	bool __LoginState_RecvPing();
#ifdef _IMPROVED_PACKET_ENCRYPTION_
	bool __LoginState_RecvKeyAgreement();
	bool __LoginState_RecvKeyAgreementCompleted();
#endif

	bool __AnalyzePacket(UINT uHeader, UINT uPacketSize, bool (CGuildMarkUploader::*pfnDispatchPacket)());

	// [CLIENT] Send packets
	bool __SendMarkPacket();
	bool __SendSymbolPacket();

private:
	UINT		m_eState = STATE_OFFLINE;

	uint32_t	m_dwSendType = SEND_TYPE_MARK;
	uint32_t	m_dwHandle = 0;
	uint32_t	m_dwRandomKey = 0;
	uint32_t	m_dwGuildID = 0;

	SGuildMark	m_kMark{};

	// [CLIENT] Symbol buffer - RAII managed
	std::vector<uint8_t>	m_kSymbolBuf;
	uint32_t				m_dwSymbolCRC32 = 0;
};
