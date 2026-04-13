// ===========================================================================
// [CLIENT] GuildMarkDownloader.cpp
// Guild Mark & Symbol Download - Client Side
// Connects to mark server, downloads mark blocks and guild symbols
// ===========================================================================
#include "StdAfx.h"
#include "GuildMarkDownloader.h"
#include "PythonCharacterManager.h"
#include "Packet.h"
#include "Test.h"

#include <cstdio>
#include <cassert>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Mark index entry as received from server
// Server sends uint16_t pairs via CopyMarkIdx
// ---------------------------------------------------------------------------
struct SMarkIndex
{
	uint16_t guild_id;
	uint16_t mark_id;
};

// ---------------------------------------------------------------------------
// [CLIENT] Construction / Destruction
// ---------------------------------------------------------------------------
CGuildMarkDownloader::CGuildMarkDownloader()
{
	SetRecvBufferSize(640 * 1024);
	SetSendBufferSize(1024);
	__Initialize();
}

CGuildMarkDownloader::~CGuildMarkDownloader()
{
	__OfflineState_Set();
}

// ---------------------------------------------------------------------------
// [CLIENT] Connect - start mark download session
// ---------------------------------------------------------------------------
bool CGuildMarkDownloader::Connect(const CNetworkAddress& c_rkNetAddr, uint32_t dwHandle, uint32_t dwRandomKey)
{
	__OfflineState_Set();

	m_dwHandle = dwHandle;
	m_dwRandomKey = dwRandomKey;
	m_dwTodo = TODO_RECV_MARK;

	return CNetworkStream::Connect(c_rkNetAddr);
}

// ---------------------------------------------------------------------------
// [CLIENT] ConnectToRecvSymbol - start symbol download session
// ---------------------------------------------------------------------------
bool CGuildMarkDownloader::ConnectToRecvSymbol(const CNetworkAddress& c_rkNetAddr, uint32_t dwHandle,
                                                uint32_t dwRandomKey,
                                                const std::vector<uint32_t>& c_rkVec_dwGuildID)
{
	__OfflineState_Set();

	m_dwHandle = dwHandle;
	m_dwRandomKey = dwRandomKey;
	m_dwTodo = TODO_RECV_SYMBOL;
	m_kVec_dwGuildID = c_rkVec_dwGuildID;

	return CNetworkStream::Connect(c_rkNetAddr);
}

// ---------------------------------------------------------------------------
// [CLIENT] Process / State machine
// ---------------------------------------------------------------------------
void CGuildMarkDownloader::Process()
{
	CNetworkStream::Process();

	if (!__StateProcess())
	{
		__OfflineState_Set();
		Disconnect();
	}
}

void CGuildMarkDownloader::OnConnectFailure()	{ __OfflineState_Set(); }
void CGuildMarkDownloader::OnConnectSuccess()	{ __LoginState_Set(); }
void CGuildMarkDownloader::OnRemoteDisconnect()	{ __OfflineState_Set(); }
void CGuildMarkDownloader::OnDisconnect()		{ __OfflineState_Set(); }

void CGuildMarkDownloader::__Initialize()
{
	m_eState = STATE_OFFLINE;
	m_pkMarkMgr = nullptr;
	m_currentRequestingImageIndex = 0;
	m_dwHandle = 0;
	m_dwRandomKey = 0;
	m_dwTodo = TODO_RECV_NONE;
	m_kVec_dwGuildID.clear();
}

bool CGuildMarkDownloader::__StateProcess()
{
	switch (m_eState)
	{
	case STATE_LOGIN:
		return __LoginState_Process();
	case STATE_COMPLETE:
		return false;
	default:
		return true;
	}
}

void CGuildMarkDownloader::__OfflineState_Set()
{
	__Initialize();
}

void CGuildMarkDownloader::__CompleteState_Set()
{
	m_eState = STATE_COMPLETE;
	CPythonCharacterManager::instance().RefreshAllGuildMark();
}

void CGuildMarkDownloader::__LoginState_Set()
{
	m_eState = STATE_LOGIN;
}

bool CGuildMarkDownloader::__LoginState_Process()
{
	uint8_t header;

	if (!Peek(sizeof(uint8_t), &header))
		return true;

	if (IsSecurityMode())
	{
		if (0 == header)
		{
			if (!Recv(sizeof(header), &header))
				return false;
			return true;
		}
	}

	UINT needPacketSize = __GetPacketSize(header);
	if (!needPacketSize)
		return false;

	if (!Peek(needPacketSize))
		return true;

	return __DispatchPacket(header);
}

// ---------------------------------------------------------------------------
// [CLIENT] Packet dispatch
// ---------------------------------------------------------------------------
UINT CGuildMarkDownloader::__GetPacketSize(UINT header)
{
	switch (header)
	{
	case HEADER_GC_PHASE:
		return sizeof(TPacketGCPhase);
	case HEADER_GC_HANDSHAKE:
		return sizeof(TPacketGCHandshake);
	case HEADER_GC_PING:
		return sizeof(TPacketGCPing);
	case HEADER_GC_MARK_IDXLIST:
		return sizeof(TPacketGCMarkIDXList);
	case HEADER_GC_MARK_BLOCK:
		return sizeof(TPacketGCMarkBlock);
	case HEADER_GC_GUILD_SYMBOL_DATA:
		return sizeof(TPacketGCGuildSymbolData);
	case HEADER_GC_MARK_DIFF_DATA:
		return sizeof(uint8_t);
#ifdef _IMPROVED_PACKET_ENCRYPTION_
	case HEADER_GC_KEY_AGREEMENT:
		return sizeof(TPacketKeyAgreement);
	case HEADER_GC_KEY_AGREEMENT_COMPLETED:
		return sizeof(TPacketKeyAgreementCompleted);
#endif
	}
	return 0;
}

bool CGuildMarkDownloader::__DispatchPacket(UINT header)
{
	switch (header)
	{
	case HEADER_GC_PHASE:
		return __LoginState_RecvPhase();
	case HEADER_GC_HANDSHAKE:
		return __LoginState_RecvHandshake();
	case HEADER_GC_PING:
		return __LoginState_RecvPing();
	case HEADER_GC_MARK_IDXLIST:
		return __LoginState_RecvMarkIndex();
	case HEADER_GC_MARK_BLOCK:
		return __LoginState_RecvMarkBlock();
	case HEADER_GC_GUILD_SYMBOL_DATA:
		return __LoginState_RecvSymbolData();
	case HEADER_GC_MARK_DIFF_DATA:
		return true;
#ifdef _IMPROVED_PACKET_ENCRYPTION_
	case HEADER_GC_KEY_AGREEMENT:
		return __LoginState_RecvKeyAgreement();
	case HEADER_GC_KEY_AGREEMENT_COMPLETED:
		return __LoginState_RecvKeyAgreementCompleted();
#endif
	}
	return false;
}

// ---------------------------------------------------------------------------
// [CLIENT] __LoginState_RecvHandshake - respond with mark login credentials
// ---------------------------------------------------------------------------
bool CGuildMarkDownloader::__LoginState_RecvHandshake()
{
	TPacketGCHandshake kPacketHandshake;
	if (!Recv(sizeof(kPacketHandshake), &kPacketHandshake))
		return false;

	TPacketCGMarkLogin kPacketMarkLogin;
	kPacketMarkLogin.header = HEADER_CG_MARK_LOGIN;
	kPacketMarkLogin.handle = m_dwHandle;
	kPacketMarkLogin.random_key = m_dwRandomKey;

	if (!Send(sizeof(kPacketMarkLogin), &kPacketMarkLogin))
		return false;

	return true;
}

// ---------------------------------------------------------------------------
// [CLIENT] __LoginState_RecvPing
// ---------------------------------------------------------------------------
bool CGuildMarkDownloader::__LoginState_RecvPing()
{
	TPacketGCPing kPacketPing;
	if (!Recv(sizeof(kPacketPing), &kPacketPing))
		return false;

	TPacketCGPong kPacketPong;
	kPacketPong.bHeader = HEADER_CG_PONG;

	if (!Send(sizeof(TPacketCGPong), &kPacketPong))
		return false;

	return true;  // FIX: always return a value
}

// ---------------------------------------------------------------------------
// [CLIENT] __LoginState_RecvPhase - after login phase, initiate the download
// ---------------------------------------------------------------------------
bool CGuildMarkDownloader::__LoginState_RecvPhase()
{
	TPacketGCPhase kPacketPhase;
	if (!Recv(sizeof(kPacketPhase), &kPacketPhase))
		return false;

	if (kPacketPhase.phase == PHASE_LOGIN)
	{
#ifndef _IMPROVED_PACKET_ENCRYPTION_
		const char* key = LocaleService_GetSecurityKey();
		SetSecurityMode(true, key);
#endif

		switch (m_dwTodo)
		{
		case TODO_RECV_NONE:
			assert(!"CGuildMarkDownloader::__LoginState_RecvPhase - Todo type is NONE");
			break;

		case TODO_RECV_MARK:
			if (!__SendMarkIDXList())
				return false;
			break;

		case TODO_RECV_SYMBOL:
			if (!__SendSymbolCRCList())
				return false;
			break;
		}
	}

	return true;
}

// ---------------------------------------------------------------------------
// [CLIENT] __SendMarkIDXList - request mark index from server
// ---------------------------------------------------------------------------
bool CGuildMarkDownloader::__SendMarkIDXList()
{
	TPacketCGMarkIDXList kPacket;
	kPacket.header = HEADER_CG_MARK_IDXLIST;

	if (!Send(sizeof(kPacket), &kPacket))
		return false;

	return true;
}

// ---------------------------------------------------------------------------
// [CLIENT] __LoginState_RecvMarkIndex - receive guild_id -> mark_id mapping
//
// CRITICAL: Server sends uint16_t pairs (2+2 bytes per entry) via CopyMarkIdx.
// We must read uint16_t values to match the wire format.
// ---------------------------------------------------------------------------
bool CGuildMarkDownloader::__LoginState_RecvMarkIndex()
{
	TPacketGCMarkIDXList kPacketMarkIndex;

	if (!Peek(sizeof(kPacketMarkIndex), &kPacketMarkIndex))
		return false;

	if (!Peek(kPacketMarkIndex.bufSize))
		return false;

	Recv(sizeof(kPacketMarkIndex));

	for (uint32_t i = 0; i < kPacketMarkIndex.count; ++i)
	{
		// FIX: Server sends uint16_t pairs, not uint32_t!
		uint16_t guildID = 0;
		uint16_t markID = 0;

		if (!Recv(sizeof(uint16_t), &guildID) ||
		    !Recv(sizeof(uint16_t), &markID))
		{
			TraceError("CGuildMarkDownloader::__LoginState_RecvMarkIndex: truncated at entry %u/%u",
			           i, kPacketMarkIndex.count);
			return false;
		}

		CGuildMarkManager::Instance().AddMarkIDByGuildID(
			static_cast<uint32_t>(guildID),
			static_cast<uint32_t>(markID));
	}

	CGuildMarkManager::Instance().LoadMarkImages();

	m_currentRequestingImageIndex = 0;
	__SendMarkCRCList();
	return true;
}

// ---------------------------------------------------------------------------
// [CLIENT] __SendMarkCRCList - send block CRC list for current image
// ---------------------------------------------------------------------------
bool CGuildMarkDownloader::__SendMarkCRCList()
{
	TPacketCGMarkCRCList kPacket;

	if (!CGuildMarkManager::Instance().GetBlockCRCList(m_currentRequestingImageIndex, kPacket.crclist))
	{
		__CompleteState_Set();
	}
	else
	{
		kPacket.header = HEADER_CG_MARK_CRCLIST;
		kPacket.imgIdx = m_currentRequestingImageIndex;
		++m_currentRequestingImageIndex;

		if (!Send(sizeof(kPacket), &kPacket))
			return false;
	}

	return true;
}

// ---------------------------------------------------------------------------
// [CLIENT] __LoginState_RecvMarkBlock - receive compressed diff blocks
// ---------------------------------------------------------------------------
bool CGuildMarkDownloader::__LoginState_RecvMarkBlock()
{
	TPacketGCMarkBlock kPacket;

	if (!Peek(sizeof(kPacket), &kPacket))
		return false;

	if (!Peek(kPacket.bufSize))
		return false;

	Recv(sizeof(kPacket));

	uint8_t posBlock;
	uint32_t compSize;
	char compBuf[SGuildMarkBlock::MAX_COMP_SIZE];

	for (uint32_t i = 0; i < kPacket.count; ++i)
	{
		if (!Recv(sizeof(uint8_t), &posBlock) ||
		    !Recv(sizeof(uint32_t), &compSize))
		{
			TraceError("CGuildMarkDownloader::__LoginState_RecvMarkBlock: truncated block header at %u/%u",
			           i, kPacket.count);
			return false;
		}

		if (compSize > SGuildMarkBlock::MAX_COMP_SIZE)
		{
			TraceError("CGuildMarkDownloader::__LoginState_RecvMarkBlock: block %u corrupt (compSize=%u > max=%u)",
			           posBlock, compSize, SGuildMarkBlock::MAX_COMP_SIZE);
			// Try to skip the corrupt data and continue
			Recv(compSize);
			continue;
		}

		if (!Recv(compSize, compBuf))
		{
			TraceError("CGuildMarkDownloader::__LoginState_RecvMarkBlock: failed to recv block %u data", posBlock);
			return false;
		}

		CGuildMarkManager::Instance().SaveBlockFromCompressedData(
			kPacket.imgIdx, posBlock,
			reinterpret_cast<const uint8_t*>(compBuf), compSize);
	}

	// Save updated image to disk and reload texture
	if (kPacket.count > 0)
	{
		CGuildMarkManager::Instance().SaveMarkImage(kPacket.imgIdx);

		std::string imagePath;
		if (CGuildMarkManager::Instance().GetMarkImageFilename(kPacket.imgIdx, imagePath))
		{
			CResource* pResource = CResourceManager::Instance().GetResourcePointer(imagePath.c_str());
			if (pResource && pResource->IsType(CGraphicImage::Type()))
			{
				auto* pkGrpImg = static_cast<CGraphicImage*>(pResource);
				pkGrpImg->Reload();
			}
		}
	}

	// Continue with next image or complete
	if (m_currentRequestingImageIndex < CGuildMarkManager::Instance().GetMarkImageCount())
		__SendMarkCRCList();
	else
		__CompleteState_Set();

	return true;
}

// ---------------------------------------------------------------------------
// [CLIENT] __SendSymbolCRCList - send CRCs for all requested guild symbols
// ---------------------------------------------------------------------------
bool CGuildMarkDownloader::__SendSymbolCRCList()
{
	for (size_t i = 0; i < m_kVec_dwGuildID.size(); ++i)
	{
		const uint32_t guildID = m_kVec_dwGuildID[i];

		TPacketCGSymbolCRC kPacket;
		kPacket.header = HEADER_CG_GUILD_SYMBOL_CRC;
		kPacket.dwGuildID = guildID;

		std::string strFileName = GetGuildSymbolFileName(guildID);
		kPacket.dwCRC = GetFileCRC32(strFileName.c_str());
		kPacket.dwSize = GetFileSize(strFileName.c_str());

#ifdef _DEBUG
		printf("__SendSymbolCRCList: guild=%u crc=%u\n", guildID, kPacket.dwCRC);
#endif

		if (!Send(sizeof(kPacket), &kPacket))
			return false;
	}

	return true;
}

// ---------------------------------------------------------------------------
// [CLIENT] __LoginState_RecvSymbolData - receive full symbol data from server
// ---------------------------------------------------------------------------
bool CGuildMarkDownloader::__LoginState_RecvSymbolData()
{
	TPacketGCBlankDynamic packet;
	if (!Peek(sizeof(TPacketGCBlankDynamic), &packet))
		return true;

#ifdef _DEBUG
	printf("__LoginState_RecvSymbolData: bufSize=%d packetSize=%d\n", GetRecvBufferSize(), packet.size);
#endif

	if (packet.size > GetRecvBufferSize())
		return true;

	TPacketGCGuildSymbolData kPacketSymbolData;
	if (!Recv(sizeof(kPacketSymbolData), &kPacketSymbolData))
		return false;

	const uint32_t wDataSize = kPacketSymbolData.size - sizeof(kPacketSymbolData);
	const uint32_t dwGuildID = kPacketSymbolData.guild_id;

	// Validate data size
	if (wDataSize == 0 || wDataSize > 256 * 1024)
	{
		TraceError("CGuildMarkDownloader::__LoginState_RecvSymbolData: invalid data size %u for guild %u",
		           wDataSize, dwGuildID);
		// Skip the data
		Recv(wDataSize);
		return true;
	}

	// Use vector instead of raw new/delete
	std::vector<uint8_t> buf(wDataSize);

	if (!Recv(wDataSize, buf.data()))
		return false;

	MyCreateDirectory(g_strGuildSymbolPathName.c_str());

	std::string strFileName = GetGuildSymbolFileName(dwGuildID);

	FILE* fp = fopen(strFileName.c_str(), "wb");
	if (!fp)
	{
		TraceError("CGuildMarkDownloader::__LoginState_RecvSymbolData: cannot write %s", strFileName.c_str());
		return false;
	}

	fwrite(buf.data(), wDataSize, 1, fp);
	fclose(fp);

#ifdef _DEBUG
	printf("__LoginState_RecvSymbolData: file=%s size=%u guild=%u\n",
	       strFileName.c_str(), wDataSize, dwGuildID);
#endif

	return true;
}

// ---------------------------------------------------------------------------
// [CLIENT] Key agreement (encrypted connections)
// ---------------------------------------------------------------------------
#ifdef _IMPROVED_PACKET_ENCRYPTION_
bool CGuildMarkDownloader::__LoginState_RecvKeyAgreement()
{
	TPacketKeyAgreement packet;
	if (!Recv(sizeof(packet), &packet))
		return false;

	Tracenf("KEY_AGREEMENT RECV %u", packet.wDataLength);

	TPacketKeyAgreement packetToSend;
	size_t dataLength = TPacketKeyAgreement::MAX_DATA_LEN;
	size_t agreedLength = Prepare(packetToSend.data, &dataLength);

	if (agreedLength == 0)
	{
		Disconnect();
		return false;
	}

	assert(dataLength <= TPacketKeyAgreement::MAX_DATA_LEN);

	if (Activate(packet.wAgreedLength, packet.data, packet.wDataLength))
	{
		packetToSend.bHeader = HEADER_CG_KEY_AGREEMENT;
		packetToSend.wAgreedLength = static_cast<WORD>(agreedLength);
		packetToSend.wDataLength = static_cast<WORD>(dataLength);

		if (!Send(sizeof(packetToSend), &packetToSend))
		{
			Tracen("CGuildMarkDownloader::__LoginState_RecvKeyAgreement - Send error");
			return false;
		}

		Tracenf("KEY_AGREEMENT SEND %u", packetToSend.wDataLength);
	}
	else
	{
		Disconnect();
		return false;
	}

	return true;
}

bool CGuildMarkDownloader::__LoginState_RecvKeyAgreementCompleted()
{
	TPacketKeyAgreementCompleted packet;
	if (!Recv(sizeof(packet), &packet))
		return false;

	Tracenf("KEY_AGREEMENT_COMPLETED RECV");
	ActivateCipher();

	return true;
}
#endif // _IMPROVED_PACKET_ENCRYPTION_
