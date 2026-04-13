// ===========================================================================
// [CLIENT] GuildMarkUploader.cpp
// Guild Mark & Symbol Upload - Client Side
// Connects to mark server, uploads mark image or guild symbol
// ===========================================================================
#include "StdAfx.h"
#include "GuildMarkUploader.h"
#include "Packet.h"
#include "Test.h"

#include <cassert>
#include <cstdio>

// Maximum symbol file size (must match server's CGuildMarkManager::MAX_SYMBOL_SIZE)
static constexpr uint32_t MAX_SYMBOL_FILE_SIZE = 64 * 1024;

// Expected symbol image dimensions
static constexpr uint32_t SYMBOL_WIDTH = 64;
static constexpr uint32_t SYMBOL_HEIGHT = 128;


namespace
{
	bool LoadGuildMarkPixelsWithDirectX(const char* c_szFileName, Pixel* pOutPixels, UINT* peError)
	{
		LPDIRECT3DDEVICE9 pkDevice = CGraphicBase::GetD3DDevice();
		if (!pkDevice)
		{
			*peError = CGuildMarkUploader::ERROR_LOAD;
			return false;
		}

		D3DXIMAGE_INFO kImageInfo{};
		if (FAILED(D3DXGetImageInfoFromFileA(c_szFileName, &kImageInfo)))
		{
			*peError = CGuildMarkUploader::ERROR_LOAD;
			return false;
		}

		if (kImageInfo.Width != SGuildMark::WIDTH)
		{
			*peError = CGuildMarkUploader::ERROR_WIDTH;
			return false;
		}

		if (kImageInfo.Height != SGuildMark::HEIGHT)
		{
			*peError = CGuildMarkUploader::ERROR_HEIGHT;
			return false;
		}

		IDirect3DTexture9* pkTexture = nullptr;
		if (FAILED(D3DXCreateTextureFromFileExA(
			pkDevice,
			c_szFileName,
			kImageInfo.Width,
			kImageInfo.Height,
			1,
			0,
			D3DFMT_A8R8G8B8,
			D3DPOOL_SYSTEMMEM,
			D3DX_FILTER_NONE,
			D3DX_FILTER_NONE,
			0,
			&kImageInfo,
			nullptr,
			&pkTexture)))
		{
			*peError = CGuildMarkUploader::ERROR_LOAD;
			return false;
		}

		D3DLOCKED_RECT kLockedRect{};
		if (FAILED(pkTexture->LockRect(0, &kLockedRect, nullptr, D3DLOCK_READONLY)))
		{
			pkTexture->Release();
			*peError = CGuildMarkUploader::ERROR_LOAD;
			return false;
		}

		const uint32_t dwRowBytes = SGuildMark::WIDTH * sizeof(Pixel);
		uint8_t* pDst = reinterpret_cast<uint8_t*>(pOutPixels);
		const uint8_t* pSrc = reinterpret_cast<const uint8_t*>(kLockedRect.pBits);

		for (uint32_t y = 0; y < SGuildMark::HEIGHT; ++y)
		{
			memcpy(pDst + (y * dwRowBytes), pSrc + (y * kLockedRect.Pitch), dwRowBytes);
		}

		pkTexture->UnlockRect(0);
		pkTexture->Release();
		return true;
	}
}




// ---------------------------------------------------------------------------
// [CLIENT] Construction / Destruction
// ---------------------------------------------------------------------------
CGuildMarkUploader::CGuildMarkUploader()
{
	SetRecvBufferSize(1024);
	SetSendBufferSize(1024);
	__Initialize();
}

CGuildMarkUploader::~CGuildMarkUploader()
{
	__OfflineState_Set();
}

void CGuildMarkUploader::Disconnect()
{
	__OfflineState_Set();
}

bool CGuildMarkUploader::IsCompleteUploading()
{
	return STATE_OFFLINE == m_eState;
}

// ---------------------------------------------------------------------------
// [CLIENT] __LoadMark - load a 16x12 guild mark image via DevIL
// ---------------------------------------------------------------------------
bool CGuildMarkUploader::__LoadMark(const char* c_szFileName, UINT* peError)
{
	/*ILuint uImg;
	ilGenImages(1, &uImg);
	ilBindImage(uImg);
	ilEnable(IL_ORIGIN_SET);
	ilOriginFunc(IL_ORIGIN_UPPER_LEFT);

	if (!ilLoad(IL_TYPE_UNKNOWN, (const ILstring)c_szFileName))
	{
		*peError = ERROR_LOAD;
		ilDeleteImages(1, &uImg);
		return false;
	}

	if (ilGetInteger(IL_IMAGE_WIDTH) != SGuildMark::WIDTH)
	{
		*peError = ERROR_WIDTH;
		ilDeleteImages(1, &uImg);
		return false;
	}

	if (ilGetInteger(IL_IMAGE_HEIGHT) != SGuildMark::HEIGHT)
	{
		*peError = ERROR_HEIGHT;
		ilDeleteImages(1, &uImg);
		return false;
	}

	ilConvertImage(IL_BGRA, IL_BYTE);
	ilCopyPixels(0, 0, 0, SGuildMark::WIDTH, SGuildMark::HEIGHT, 1, IL_BGRA, IL_BYTE, m_kMark.m_apxBuf);

	ilDeleteImages(1, &uImg);
	return true;*/
	return LoadGuildMarkPixelsWithDirectX(c_szFileName, m_kMark.m_apxBuf, peError);
}

// ---------------------------------------------------------------------------
// [CLIENT] __LoadSymbol - validate and load a guild symbol file
//          Validates image dimensions via DevIL, then reads raw file bytes
//          NOTE: Does NOT call ilShutDown() - that would break all other
//          DevIL usage in the application
// ---------------------------------------------------------------------------
bool CGuildMarkUploader::__LoadSymbol(const char* c_szFileName, UINT* peError)
{
	// Step 1: Validate image dimensions via DevIL
	{
		D3DXIMAGE_INFO kImageInfo{};
		if (FAILED(D3DXGetImageInfoFromFileA(c_szFileName, &kImageInfo)))
		{
			*peError = ERROR_LOAD;
			//ilDeleteImages(1, &uImg);
			return false;
		}

		if (kImageInfo.Height != 64)
		{
			*peError = ERROR_WIDTH;
			//ilDeleteImages(1, &uImg);
			return false;
		}

		if (kImageInfo.Height != 128)
		{
			*peError = ERROR_HEIGHT;
			//ilDeleteImages(1, &uImg);
			return false;
		}

		//ilDeleteImages(1, &uImg);
		// NOTE: No ilShutDown() here - it would kill DevIL globally
	}

	// Step 2: Read raw file bytes into buffer
	FILE* file = fopen(c_szFileName, "rb");
	if (!file)
	{
		*peError = ERROR_LOAD;
		return false;
	}

	fseek(file, 0, SEEK_END);
	const long fileSize = ftell(file);
	fseek(file, 0, SEEK_SET);

	if (fileSize <= 0 || static_cast<uint32_t>(fileSize) > MAX_SYMBOL_FILE_SIZE)
	{
		TraceError("CGuildMarkUploader::__LoadSymbol: file size %ld out of range", fileSize);
		*peError = ERROR_LOAD;
		fclose(file);
		return false;
	}

	m_kSymbolBuf.resize(static_cast<size_t>(fileSize));

	if (fread(m_kSymbolBuf.data(), fileSize, 1, file) != 1)
	{
		TraceError("CGuildMarkUploader::__LoadSymbol: failed to read %s", c_szFileName);
		*peError = ERROR_LOAD;
		m_kSymbolBuf.clear();
		fclose(file);
		return false;
	}

	fclose(file);

	m_dwSymbolCRC32 = GetFileCRC32(c_szFileName);
	return true;
}

// ---------------------------------------------------------------------------
// [CLIENT] Connect - initiate mark upload
// ---------------------------------------------------------------------------
bool CGuildMarkUploader::Connect(const CNetworkAddress& c_rkNetAddr, uint32_t dwHandle,
                                  uint32_t dwRandomKey, uint32_t dwGuildID,
                                  const char* c_szFileName, UINT* peError)
{
	__OfflineState_Set();
	SetRecvBufferSize(1024);
	SetSendBufferSize(1024);

	if (!CNetworkStream::Connect(c_rkNetAddr))
	{
		*peError = ERROR_CONNECT;
		return false;
	}

	m_dwSendType = SEND_TYPE_MARK;
	m_dwHandle = dwHandle;
	m_dwRandomKey = dwRandomKey;
	m_dwGuildID = dwGuildID;

	if (!__LoadMark(c_szFileName, peError))
		return false;

	return true;
}

// ---------------------------------------------------------------------------
// [CLIENT] ConnectToSendSymbol - initiate symbol upload
// ---------------------------------------------------------------------------
bool CGuildMarkUploader::ConnectToSendSymbol(const CNetworkAddress& c_rkNetAddr, uint32_t dwHandle,
                                              uint32_t dwRandomKey, uint32_t dwGuildID,
                                              const char* c_szFileName, UINT* peError)
{
	__OfflineState_Set();
	SetRecvBufferSize(1024);
	SetSendBufferSize(64 * 1024);

	if (!CNetworkStream::Connect(c_rkNetAddr))
	{
		*peError = ERROR_CONNECT;
		return false;
	}

	m_dwSendType = SEND_TYPE_SYMBOL;
	m_dwHandle = dwHandle;
	m_dwRandomKey = dwRandomKey;
	m_dwGuildID = dwGuildID;

	if (!__LoadSymbol(c_szFileName, peError))
		return false;

	return true;
}

// ---------------------------------------------------------------------------
// [CLIENT] Process / State machine
// ---------------------------------------------------------------------------
void CGuildMarkUploader::Process()
{
	CNetworkStream::Process();

	if (!__StateProcess())
	{
		__OfflineState_Set();
		Disconnect();
	}
}

void CGuildMarkUploader::OnConnectFailure()	{ __OfflineState_Set(); }
void CGuildMarkUploader::OnConnectSuccess()	{ __LoginState_Set(); }
void CGuildMarkUploader::OnRemoteDisconnect()	{ __OfflineState_Set(); }
void CGuildMarkUploader::OnDisconnect()		{ __OfflineState_Set(); }

void CGuildMarkUploader::__Initialize()
{
	m_eState = STATE_OFFLINE;
	m_dwGuildID = 0;
	m_dwHandle = 0;
	m_dwRandomKey = 0;
	m_dwSendType = SEND_TYPE_MARK;
	m_kSymbolBuf.clear();
	m_dwSymbolCRC32 = 0;
}

bool CGuildMarkUploader::__StateProcess()
{
	switch (m_eState)
	{
	case STATE_LOGIN:
		return __LoginState_Process();
	default:
		return true;
	}
}

void CGuildMarkUploader::__OfflineState_Set()
{
	__Initialize();
}

void CGuildMarkUploader::__CompleteState_Set()
{
	m_eState = STATE_COMPLETE;
	__OfflineState_Set();
}

void CGuildMarkUploader::__LoginState_Set()
{
	m_eState = STATE_LOGIN;
}

bool CGuildMarkUploader::__LoginState_Process()
{
	if (!__AnalyzePacket(HEADER_GC_PHASE, sizeof(TPacketGCPhase), &CGuildMarkUploader::__LoginState_RecvPhase))
		return false;

	if (!__AnalyzePacket(HEADER_GC_HANDSHAKE, sizeof(TPacketGCHandshake), &CGuildMarkUploader::__LoginState_RecvHandshake))
		return false;

	if (!__AnalyzePacket(HEADER_GC_PING, sizeof(TPacketGCPing), &CGuildMarkUploader::__LoginState_RecvPing))
		return false;

#ifdef _IMPROVED_PACKET_ENCRYPTION_
	if (!__AnalyzePacket(HEADER_GC_KEY_AGREEMENT, sizeof(TPacketKeyAgreement), &CGuildMarkUploader::__LoginState_RecvKeyAgreement))
		return false;

	if (!__AnalyzePacket(HEADER_GC_KEY_AGREEMENT_COMPLETED, sizeof(TPacketKeyAgreementCompleted), &CGuildMarkUploader::__LoginState_RecvKeyAgreementCompleted))
		return false;
#endif

	return true;
}

// ---------------------------------------------------------------------------
// [CLIENT] __SendMarkPacket - send 16x12 mark image to server
// ---------------------------------------------------------------------------
bool CGuildMarkUploader::__SendMarkPacket()
{
	TPacketCGMarkUpload kPacket;
	kPacket.header = HEADER_CG_MARK_UPLOAD;
	kPacket.gid = m_dwGuildID;

	static_assert(sizeof(kPacket.image) == sizeof(m_kMark.m_apxBuf),
		"Mark packet image size must match SGuildMark buffer size");

	memcpy(kPacket.image, m_kMark.m_apxBuf, sizeof(kPacket.image));

	if (!Send(sizeof(kPacket), &kPacket))
		return false;

	return true;
}

// ---------------------------------------------------------------------------
// [CLIENT] __SendSymbolPacket - send guild symbol file data to server
// ---------------------------------------------------------------------------
bool CGuildMarkUploader::__SendSymbolPacket()
{
	if (m_kSymbolBuf.empty())
		return false;

	TPacketCGSymbolUpload kPacket;
	kPacket.header = HEADER_CG_GUILD_SYMBOL_UPLOAD;
	kPacket.handle = m_dwGuildID;
	kPacket.size = static_cast<uint16_t>(sizeof(TPacketCGSymbolUpload) + m_kSymbolBuf.size());

	if (!Send(sizeof(kPacket), &kPacket))
		return false;

	if (!Send(m_kSymbolBuf.size(), m_kSymbolBuf.data()))
		return false;

#ifdef _DEBUG
	printf("__SendSymbolPacket: guild=%u packetSize=%u bufSize=%zu crc=%u\n",
	       m_dwGuildID, kPacket.size, m_kSymbolBuf.size(), m_dwSymbolCRC32);
#endif

	CNetworkStream::__SendInternalBuffer();
	__CompleteState_Set();

	return true;
}

// ---------------------------------------------------------------------------
// [CLIENT] Recv handlers
// ---------------------------------------------------------------------------
bool CGuildMarkUploader::__LoginState_RecvPhase()
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

		if (m_dwSendType == SEND_TYPE_MARK)
		{
			if (!__SendMarkPacket())
				return false;
		}
		else if (m_dwSendType == SEND_TYPE_SYMBOL)
		{
			if (!__SendSymbolPacket())
				return false;
		}
	}

	return true;
}

bool CGuildMarkUploader::__LoginState_RecvHandshake()
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

bool CGuildMarkUploader::__LoginState_RecvPing()
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

#ifdef _IMPROVED_PACKET_ENCRYPTION_
bool CGuildMarkUploader::__LoginState_RecvKeyAgreement()
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
			Tracen("CGuildMarkUploader::__LoginState_RecvKeyAgreement - Send error");
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

bool CGuildMarkUploader::__LoginState_RecvKeyAgreementCompleted()
{
	TPacketKeyAgreementCompleted packet;
	if (!Recv(sizeof(packet), &packet))
		return false;

	Tracenf("KEY_AGREEMENT_COMPLETED RECV");
	ActivateCipher();

	return true;
}
#endif // _IMPROVED_PACKET_ENCRYPTION_

bool CGuildMarkUploader::__AnalyzePacket(UINT uHeader, UINT uPacketSize,
                                          bool (CGuildMarkUploader::*pfnDispatchPacket)())
{
	uint8_t bHeader;
	if (!Peek(sizeof(bHeader), &bHeader))
		return true;

	if (bHeader != uHeader)
		return true;

	if (!Peek(uPacketSize))
		return true;

	return (this->*pfnDispatchPacket)();
}
