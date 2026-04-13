#pragma once

#include <windows.h>
//#include "../../extern/include/lzo-2.03/lzo1x.h"
#include "Singleton.h"

class CLZObject
{
public:
#pragma pack(4)
	using THeader = struct SHeader
	{
		DWORD dwFourCC;
		DWORD dwEncryptSize;
		DWORD dwCompressedSize;
		DWORD dwRealSize;
	};
#pragma pack()

	CLZObject();
	~CLZObject();

	void Clear();

	void BeginCompress(const void* pvIn, UINT uiInLen);
	void BeginCompressInBuffer(const void* pvIn, UINT uiInLen, void* pvOut);
	bool Compress();
	bool BeginDecompress(const void* pvIn);
	bool Decompress(DWORD* pdwKey = nullptr);

	bool Encrypt(const DWORD* pdwKey) const;
	bool __Decrypt(const DWORD* key, uint8_t* data) const;

	[[nodiscard]] const THeader& GetHeader() const { return *m_pHeader; }
	[[nodiscard]] uint8_t* GetBuffer() const { return m_pbBuffer; }
	[[nodiscard]] DWORD GetSize() const;
	void AllocBuffer(DWORD dwSize);
	[[nodiscard]] DWORD GetBufferSize() const { return m_dwBufferSize; }

private:
	void Initialize();

	uint8_t* m_pbBuffer;
	DWORD m_dwBufferSize;

	THeader* m_pHeader;
	const uint8_t* m_pbIn;
	bool m_bCompressed;

	bool m_bInBuffer;

public:
	static DWORD ms_dwFourCC;
};

class CLZO : public CSingleton<CLZO>
{
public:
	CLZO();
	~CLZO() override;

	static bool CompressMemory(CLZObject& rObj, const void* pIn, UINT uiInLen);
	static bool CompressEncryptedMemory(CLZObject& rObj, const void* pIn, UINT uiInLen, DWORD* pdwKey);
	static bool Decompress(CLZObject& rObj, const uint8_t* pbBuf, DWORD* pdwKey = nullptr);
	[[nodiscard]] uint8_t* GetWorkMemory() const;

private:
	uint8_t* m_pWorkMem;
};
