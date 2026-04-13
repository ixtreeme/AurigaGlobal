// ===========================================================================
// [CLIENT] GuildMarkDownloader.h
// Guild Mark & Symbol Download - Client Side
// Connects to mark server, downloads mark blocks and guild symbols
// ===========================================================================
#pragma once

#include "../Render/NetStream.h"
#include "MarkManager.h"

#include <vector>
#include <cstdint>

class CGuildMarkDownloader : public CNetworkStream, public CSingleton<CGuildMarkDownloader>
{
public:
	CGuildMarkDownloader();
	virtual ~CGuildMarkDownloader();

	// [CLIENT] Connect to download marks
	bool Connect(const CNetworkAddress& c_rkNetAddr, uint32_t dwHandle, uint32_t dwRandomKey);

	// [CLIENT] Connect to download guild symbols
	bool ConnectToRecvSymbol(const CNetworkAddress& c_rkNetAddr, uint32_t dwHandle, uint32_t dwRandomKey,
	                         const std::vector<uint32_t>& c_rkVec_dwGuildID);

	void Process();

private:
	enum EState
	{
		STATE_OFFLINE,
		STATE_LOGIN,
		STATE_COMPLETE,
	};

	enum ETodo
	{
		TODO_RECV_NONE,
		TODO_RECV_MARK,
		TODO_RECV_SYMBOL,
	};

	// [CLIENT] Network callbacks
	void OnConnectFailure();
	void OnConnectSuccess();
	void OnRemoteDisconnect();
	void OnDisconnect();

	// [CLIENT] State machine
	void __Initialize();
	bool __StateProcess();

	UINT __GetPacketSize(UINT header);
	bool __DispatchPacket(UINT header);

	void __OfflineState_Set();
	void __CompleteState_Set();

	void __LoginState_Set();
	bool __LoginState_Process();

	// [CLIENT] Recv handlers
	bool __LoginState_RecvPhase();
	bool __LoginState_RecvHandshake();
	bool __LoginState_RecvPing();
	bool __LoginState_RecvMarkIndex();
	bool __LoginState_RecvMarkBlock();
	bool __LoginState_RecvSymbolData();
#ifdef _IMPROVED_PACKET_ENCRYPTION_
	bool __LoginState_RecvKeyAgreement();
	bool __LoginState_RecvKeyAgreementCompleted();
#endif

	// [CLIENT] Send requests
	bool __SendMarkIDXList();
	bool __SendMarkCRCList();
	bool __SendSymbolCRCList();

private:
	uint32_t	m_dwHandle = 0;
	uint32_t	m_dwRandomKey = 0;
	uint32_t	m_dwTodo = TODO_RECV_NONE;

	std::vector<uint32_t>	m_kVec_dwGuildID;

	UINT		m_eState = STATE_OFFLINE;
	uint8_t		m_currentRequestingImageIndex = 0;

	CGuildMarkManager*	m_pkMarkMgr = nullptr;
};
