#include "StdAfx.h"
#include <stdlib.h>
//#include <lzo-2.03/lzoLibLink.h>
#include "lzo.h"

#include "tea.h"
#include "debug.h"
#define ENABLE_LZ4_PACK_COMPRESSION
#ifdef ENABLE_LZ4_PACK_COMPRESSION
#include <lz4.h>
#endif

#define dbg_printf

static class LZOFreeMemoryMgr
{
public:
	enum
	{
		REUSING_CAPACITY = 64 * 1024,
	};

public:
	~LZOFreeMemoryMgr()
	{
		for (const auto& i : m_freeVector)
			delete []i;

		m_freeVector.clear();
	}

	uint8_t* Alloc(unsigned capacity)
	{
		assert(capacity > 0);
		if (capacity < REUSING_CAPACITY)
		{
			if (!m_freeVector.empty())
			{
				uint8_t* freeMem = m_freeVector.back();
				m_freeVector.pop_back();

				dbg_printf("lzo.reuse_alloc\t%p(%d) free\n", freeMem, capacity);
				return freeMem;
			}
			auto newMem = new uint8_t[REUSING_CAPACITY];
			dbg_printf("lzo.reuse_alloc\t%p(%d) real\n", newMem, capacity);
			return newMem;
		}
		auto newMem = new uint8_t[capacity];
		dbg_printf("lzo.real_alloc\t%p(%d)\n", newMem, capacity);
		return newMem;
	}

	void Free(uint8_t* ptr, unsigned capacity)
	{
		assert(ptr != NULL);
		assert(capacity > 0);
		if (capacity < REUSING_CAPACITY)
		{
			dbg_printf("lzo.reuse_free\t%p(%d)\n", ptr, capacity);
			m_freeVector.emplace_back(ptr);
			return;
		}

		dbg_printf("lzo.real_free\t%p(%d)\n", ptr, capacity);
		delete [] ptr;
	}

private:
	std::vector<uint8_t*> m_freeVector;
} gs_freeMemMgr;


DWORD CLZObject::ms_dwFourCC = MAKEFOURCC('M', 'C', 'O', 'Z');

CLZObject::CLZObject()
{
	Initialize();
}

void CLZObject::Initialize()
{
	m_bInBuffer = false;
	m_pbBuffer = nullptr;
	m_dwBufferSize = 0;

	m_pHeader = nullptr;
	m_pbIn = nullptr;
	m_bCompressed = false;
}

void CLZObject::Clear()
{
	if (m_pbBuffer && !m_bInBuffer)
		gs_freeMemMgr.Free(m_pbBuffer, m_dwBufferSize);

	if (m_dwBufferSize > 0)
	{
		dbg_printf("lzo.free %d\n", m_dwBufferSize);
	}

	Initialize();
}

CLZObject::~CLZObject()
{
	Clear();
}

DWORD CLZObject::GetSize() const
{
	assert(m_pHeader);

	if (m_bCompressed)
	{
		if (m_pHeader->dwEncryptSize)
			return sizeof(THeader) + sizeof(DWORD) + m_pHeader->dwEncryptSize;
		return sizeof(THeader) + sizeof(DWORD) + m_pHeader->dwCompressedSize;
	}
	return m_pHeader->dwRealSize;
}

void CLZObject::BeginCompress(const void* pvIn, const UINT uiInLen)
{
	m_pbIn = static_cast<const uint8_t*>(pvIn);
	m_dwBufferSize = sizeof(THeader) + sizeof(DWORD) + (uiInLen + uiInLen / 64 + 16 + 3) + 8;

	m_pbBuffer = gs_freeMemMgr.Alloc(m_dwBufferSize);
	memset(m_pbBuffer, 0, m_dwBufferSize);

	m_pHeader = (THeader*)m_pbBuffer;
	m_pHeader->dwFourCC = ms_dwFourCC;
	m_pHeader->dwEncryptSize = m_pHeader->dwCompressedSize = m_pHeader->dwRealSize = 0;
	m_pHeader->dwRealSize = uiInLen;
}

void CLZObject::BeginCompressInBuffer(const void* pvIn, const UINT uiInLen, void*)
{
	m_pbIn = static_cast<const uint8_t*>(pvIn);
	m_dwBufferSize = sizeof(THeader) + sizeof(DWORD) + (uiInLen + uiInLen / 64 + 16 + 3) + 8;

	m_pbBuffer = gs_freeMemMgr.Alloc(m_dwBufferSize);
	memset(m_pbBuffer, 0, m_dwBufferSize);

	m_pHeader = (THeader*)m_pbBuffer;
	m_pHeader->dwFourCC = ms_dwFourCC;
	m_pHeader->dwEncryptSize = m_pHeader->dwCompressedSize = m_pHeader->dwRealSize = 0;
	m_pHeader->dwRealSize = uiInLen;
	m_bInBuffer = true;
}

bool CLZObject::Compress()
{
	UINT iOutLen;

	uint8_t* pbBuffer = m_pbBuffer + sizeof(THeader);
	*(DWORD*)pbBuffer = ms_dwFourCC;
	pbBuffer += sizeof(DWORD);

#ifdef ENABLE_LZ4_PACK_COMPRESSION
	const int destBufferSize = LZ4_compressBound(m_pHeader->dwRealSize);

	iOutLen = LZ4_compress_default((const char*)m_pbIn, (char*)pbBuffer, m_pHeader->dwRealSize, destBufferSize);

	if (iOutLen < 1)
	{
		TraceError("LZ4: LZ4_compress_default failed");
		return false;
	}
#else
#if defined( LZO1X_999_MEM_COMPRESS )
	int r = lzo1x_999_compress((uint8_t*)m_pbIn, m_pHeader->dwRealSize, pbBuffer, (lzo_uint*)&iOutLen, CLZO::Instance().GetWorkMemory());
#else
	int r = lzo1x_1_compress((uint8_t*)m_pbIn, m_pHeader->dwRealSize, pbBuffer, (lzo_uint*)&iOutLen, CLZO::Instance().GetWorkMemory());
#endif

	if (LZO_E_OK != r)
	{
		TraceError("LZO: lzo1x_999_compress failed");
		return false;
	}
#endif

	m_pHeader->dwCompressedSize = iOutLen;
	m_bCompressed = true;
	return true;
}

bool CLZObject::BeginDecompress(const void* pvIn)
{
	const auto pHeader = (THeader*)pvIn;

	if (pHeader->dwFourCC != ms_dwFourCC)
	{
		TraceError("LZObject: not a valid data");
		return false;
	}

	m_pHeader = pHeader;
	m_pbIn = static_cast<const uint8_t*>(pvIn) + (sizeof(THeader) + sizeof(DWORD));

	m_dwBufferSize = pHeader->dwRealSize;
	m_pbBuffer = gs_freeMemMgr.Alloc(m_dwBufferSize);
	memset(m_pbBuffer, 0, pHeader->dwRealSize);
	return true;
}

class DecryptBuffer
{
public:
	enum
	{
		LOCAL_BUF_SIZE = 8 * 1024,
	};

public:
	explicit DecryptBuffer(unsigned size)
	{
		static unsigned count = 0;
		static unsigned sum = 0;
		static unsigned maxSize = 0;
		memset(&m_local_buf, 0, sizeof(m_local_buf));

		sum += size;
		count++;

		maxSize = max(size, maxSize);
		if (size >= LOCAL_BUF_SIZE)
		{
			m_buf = new char[size];
			dbg_printf("DecryptBuffer - AllocHeap %d max(%d) ave(%d)\n", size, maxSize / 1024, sum / count);
		}
		else
		{
			dbg_printf("DecryptBuffer - AllocStack %d max(%d) ave(%d)\n", size, maxSize / 1024, sum / count);
			m_buf = m_local_buf;
		}
	}

	~DecryptBuffer()
	{
		if (m_local_buf != m_buf)
		{
			dbg_printf("DecruptBuffer - FreeHeap\n");
			delete [] m_buf;
		}
		else
		{
			dbg_printf("DecruptBuffer - FreeStack\n");
		}
	}

	void* GetBufferPtr() const
	{
		return m_buf;
	}

private:
	char* m_buf;
	char m_local_buf[LOCAL_BUF_SIZE];
};

bool CLZObject::Decompress(DWORD* pdwKey)
{
	UINT uiSize;
#ifndef ENABLE_LZ4_PACK_COMPRESSION
	int r;
#endif

	if (m_pHeader->dwEncryptSize)
	{
		const DecryptBuffer buf(m_pHeader->dwEncryptSize);

		auto pbDecryptedBuffer = static_cast<uint8_t*>(buf.GetBufferPtr());

		__Decrypt(pdwKey, pbDecryptedBuffer);

		if (*(DWORD*)pbDecryptedBuffer != ms_dwFourCC)
		{
			TraceError("LZObject: key incorrect");
			return false;
		}

#ifdef ENABLE_LZ4_PACK_COMPRESSION
		uiSize = LZ4_decompress_safe((const char*)pbDecryptedBuffer + sizeof(DWORD), (char*)m_pbBuffer,
		                             m_pHeader->dwCompressedSize, m_pHeader->dwRealSize);
#else
		if (LZO_E_OK != (r = lzo1x_decompress(pbDecryptedBuffer + sizeof(DWORD), m_pHeader->dwCompressedSize, m_pbBuffer, (lzo_uint*)&uiSize, NULL)))
		{
			TraceError("LZObject: Decompress failed(decrypt) ret %d\n", r);
			return false;
		}
#endif
	}
	else
	{
		uiSize = m_pHeader->dwRealSize;

#ifdef ENABLE_LZ4_PACK_COMPRESSION
		uiSize = LZ4_decompress_safe((const char*)m_pbIn, (char*)m_pbBuffer, m_pHeader->dwCompressedSize,
		                             m_pHeader->dwRealSize);
#else
		if (LZO_E_OK != (r = lzo1x_decompress(m_pbIn, m_pHeader->dwCompressedSize, m_pbBuffer, (lzo_uint*)&uiSize, NULL)))
		{
			TraceError("LZObject: Decompress failed : ret %d, CompressedSize %d\n", r, m_pHeader->dwCompressedSize);
			return false;
		}
#endif
	}

	if (uiSize != m_pHeader->dwRealSize)
	{
		TraceError("LZObject: Size differs");
		return false;
	}

	return true;
}

bool CLZObject::Encrypt(const DWORD* pdwKey) const
{
	if (!m_bCompressed)
	{
		assert(!"not compressed yet");
		return false;
	}

	uint8_t* pbBuffer = m_pbBuffer + sizeof(THeader);
	m_pHeader->dwEncryptSize = tea_encrypt((DWORD*)pbBuffer, (const DWORD*)pbBuffer, pdwKey,
	                                       m_pHeader->dwCompressedSize + 19);
	return true;
}

bool CLZObject::__Decrypt(const DWORD* key, uint8_t* data) const
{
	assert(m_pbBuffer);

	tea_decrypt((DWORD*)data, (const DWORD*)(m_pbIn - sizeof(DWORD)), key, m_pHeader->dwEncryptSize);
	return true;
}

void CLZObject::AllocBuffer(const DWORD dwSrcSize)
{
	if (m_pbBuffer && !m_bInBuffer)
		gs_freeMemMgr.Free(m_pbBuffer, m_dwBufferSize);

	m_pbBuffer = gs_freeMemMgr.Alloc(dwSrcSize);
	m_dwBufferSize = dwSrcSize;
}

CLZO::CLZO() : m_pWorkMem(nullptr)
{
	/*if (lzo_init() != LZO_E_OK)
	{
		TraceError("LZO: cannot initialize");
		return;
	}*/

	m_pWorkMem = static_cast<uint8_t*>(malloc(LZ4_MEMORY_USAGE));

	if (nullptr == m_pWorkMem)
	{
		TraceError("LZO: cannot alloc memory");
		return;
	}
}

CLZO::~CLZO()
{
	if (m_pWorkMem)
	{
		free(m_pWorkMem);
		m_pWorkMem = nullptr;
	}
}

bool CLZO::CompressMemory(CLZObject& rObj, const void* pIn, const UINT uiInLen)
{
	rObj.BeginCompress(pIn, uiInLen);
	return rObj.Compress();
}

bool CLZO::CompressEncryptedMemory(CLZObject& rObj, const void* pIn, const UINT uiInLen, DWORD* pdwKey)
{
	rObj.BeginCompress(pIn, uiInLen);

	if (rObj.Compress())
	{
		if (rObj.Encrypt(pdwKey))
			return true;

		return false;
	}

	return false;
}

bool CLZO::Decompress(CLZObject& rObj, const uint8_t* pbBuf, DWORD* pdwKey)
{
	if (!rObj.BeginDecompress(pbBuf))
		return false;

	if (!rObj.Decompress(pdwKey))
		return false;

	return true;
}

uint8_t* CLZO::GetWorkMemory() const
{
	return m_pWorkMem;
}
